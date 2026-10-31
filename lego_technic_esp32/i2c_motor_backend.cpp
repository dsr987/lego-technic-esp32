#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "i2c_motor_backend.h"
#include "serial_log.h"

namespace {

constexpr uint8_t I2C_MOTOR_WING_ADDRESS = 0x60;
constexpr uint16_t MOTOR_PWM_FREQUENCY_HZ = 1600;
constexpr uint8_t MOTOR_COUNT = 4;

constexpr uint8_t PCA9685_MODE1 = 0x00;
constexpr uint8_t PCA9685_PRESCALE = 0xFE;
constexpr uint8_t PCA9685_LED0_ON_L = 0x06;

struct MotorPins {
  uint8_t pwm;
  uint8_t in1;
  uint8_t in2;
};

// Раскладка выходов повторяет Adafruit Motor Shield V2.
// Логические A/B/E/F соответствуют физическим выходам M1/M2/M3/M4.
constexpr MotorPins MOTOR_PINS[MOTOR_COUNT] = {
  {8, 10, 9},   // A -> M1
  {13, 11, 12}, // B -> M2
  {2, 4, 3},    // E -> M3
  {7, 5, 6}     // F -> M4
};

TwoWire *motorWire = &Wire;
int8_t lastDirection[MOTOR_COUNT] = {0, 0, 0, 0};
int16_t lastReportedSpeed[MOTOR_COUNT] = {127, 127, 127, 127};
bool backendReady = false;
bool rejectedCommandLogged = false;
SemaphoreHandle_t backendMutex = nullptr;

const char *motorName(uint8_t index) {
  static const char *NAMES[MOTOR_COUNT] = {"A/M1", "B/M2", "E/M3", "F/M4"};
  return index < MOTOR_COUNT ? NAMES[index] : "UNKNOWN";
}

class BackendLock {
 public:
  explicit BackendLock(TickType_t waitTicks = pdMS_TO_TICKS(250))
      : acquired_(backendMutex != nullptr &&
                  xSemaphoreTake(backendMutex, waitTicks) == pdTRUE) {}

  ~BackendLock() {
    if (acquired_) {
      xSemaphoreGive(backendMutex);
    }
  }

  bool acquired() const {
    return acquired_;
  }

 private:
  bool acquired_;
};

// Любая ошибка транзакции защёлкивает отказ: обычные команды и повторные
// записи не выполняются до перезапуска устройства. Это не аппаратный стоп.
bool writeRegister(uint8_t reg, uint8_t value) {
  if (motorWire == nullptr) {
    serialLogPrintln("[FW][PCA9685][ERROR] Wire pointer is null during register write");
    backendReady = false;
    return false;
  }

  motorWire->beginTransmission(I2C_MOTOR_WING_ADDRESS);
  const uint8_t regWritten = motorWire->write(reg);
  const uint8_t valueWritten = motorWire->write(value);
  const uint8_t status = motorWire->endTransmission();

  if (regWritten != 1 || valueWritten != 1 || status != 0) {
    serialLogPrintf("[FW][PCA9685][ERROR] I2C write reg=0x%02X value=0x%02X regBytes=%u valueBytes=%u endStatus=%u\n",
                  reg, value, static_cast<unsigned>(regWritten), static_cast<unsigned>(valueWritten), static_cast<unsigned>(status));
    backendReady = false;
    return false;
  }

  return true;
}

bool readRegister(uint8_t reg, uint8_t &value) {
  if (motorWire == nullptr) {
    serialLogPrintln("[FW][PCA9685][ERROR] Wire pointer is null during register read");
    backendReady = false;
    return false;
  }

  motorWire->beginTransmission(I2C_MOTOR_WING_ADDRESS);
  const uint8_t regWritten = motorWire->write(reg);
  const uint8_t txStatus = motorWire->endTransmission(false);
  if (regWritten != 1 || txStatus != 0) {
    serialLogPrintf("[FW][PCA9685][ERROR] I2C register select reg=0x%02X bytes=%u endStatus=%u\n",
                  reg, static_cast<unsigned>(regWritten), static_cast<unsigned>(txStatus));
    backendReady = false;
    return false;
  }

  const uint8_t received = motorWire->requestFrom(I2C_MOTOR_WING_ADDRESS, static_cast<uint8_t>(1));
  if (received != 1 || motorWire->available() < 1) {
    serialLogPrintf("[FW][PCA9685][ERROR] I2C read reg=0x%02X received=%u available=%d\n",
                  reg, static_cast<unsigned>(received), motorWire->available());
    backendReady = false;
    return false;
  }

  value = static_cast<uint8_t>(motorWire->read());
  return true;
}

// PCA9685 PWM-регистр: ON=0, OFF=value. Значение 4096 означает полный ON.
bool writePwm(uint8_t channel, uint16_t on, uint16_t off) {
  if (channel > 15 || motorWire == nullptr) {
    backendReady = false;
    return false;
  }

  const uint8_t buffer[5] = {
    static_cast<uint8_t>(PCA9685_LED0_ON_L + 4 * channel),
    static_cast<uint8_t>(on & 0xFF),
    static_cast<uint8_t>((on >> 8) & 0x1F),
    static_cast<uint8_t>(off & 0xFF),
    static_cast<uint8_t>((off >> 8) & 0x1F)
  };

  motorWire->beginTransmission(I2C_MOTOR_WING_ADDRESS);
  const size_t written = motorWire->write(buffer, sizeof(buffer));
  const uint8_t status = motorWire->endTransmission();

  if (written != sizeof(buffer) || status != 0) {
    serialLogPrintf("[FW][PCA9685][ERROR] PWM write channel=%u bytes=%u expected=%u endStatus=%u\n",
                  static_cast<unsigned>(channel), static_cast<unsigned>(written), static_cast<unsigned>(sizeof(buffer)), static_cast<unsigned>(status));
    backendReady = false;
    return false;
  }

  return true;
}

bool setPin(uint8_t pin, bool high) {
  return high ? writePwm(pin, 4096, 0) : writePwm(pin, 0, 0);
}

bool setMotorSpeed(uint8_t index, uint8_t speed) {
  return writePwm(MOTOR_PINS[index].pwm, 0, static_cast<uint16_t>(speed) * 16U);
}

bool releaseMotor(uint8_t index) {
  return setPin(MOTOR_PINS[index].in1, false) &&
         setPin(MOTOR_PINS[index].in2, false);
}

bool setMotorDirection(uint8_t index, int8_t direction) {
  const MotorPins &pins = MOTOR_PINS[index];

  // Порядок повторяет Adafruit_DCMotor::run(): сначала неактивный вход,
  // затем активный. При реверсе вызывающий код предварительно делает RELEASE.
  if (direction > 0) {
    return setPin(pins.in2, false) && setPin(pins.in1, true);
  }
  return setPin(pins.in1, false) && setPin(pins.in2, true);
}

bool initializePca9685() {
  serialLogPrintf("[FW][PCA9685] Initializing at I2C address 0x%02X, target PWM=%u Hz\n",
                static_cast<unsigned>(I2C_MOTOR_WING_ADDRESS), static_cast<unsigned>(MOTOR_PWM_FREQUENCY_HZ));
  uint8_t oldMode = 0;
  if (!readRegister(PCA9685_MODE1, oldMode)) {
    serialLogPrintln("[FW][PCA9685][ERROR] Failed to read MODE1; initialization aborted");
    return false;
  }
  serialLogPrintf("[FW][PCA9685] MODE1 before init: 0x%02X\n", static_cast<unsigned>(oldMode));

  const uint8_t sleepMode = static_cast<uint8_t>((oldMode & 0x7F) | 0x10);
  const float adjustedFrequency = MOTOR_PWM_FREQUENCY_HZ * 0.9f;
  const float prescaleValue =
      25000000.0f / 4096.0f / adjustedFrequency - 1.0f;
  const uint8_t prescale = static_cast<uint8_t>(prescaleValue + 0.5f);
  serialLogPrintf("[FW][PCA9685] Calculated PRESCALE: %u\n", static_cast<unsigned>(prescale));

  if (!writeRegister(PCA9685_MODE1, sleepMode) ||
      !writeRegister(PCA9685_PRESCALE, prescale) ||
      !writeRegister(PCA9685_MODE1, oldMode)) {
    return false;
  }

  delay(5);

  if (!writeRegister(PCA9685_MODE1, static_cast<uint8_t>(oldMode | 0xA1))) {
    return false;
  }

  // Как и Adafruit_MotorShield::begin(), выключаем все 16 выходов.
  for (uint8_t channel = 0; channel < 16; ++channel) {
    if (!writePwm(channel, 0, 0)) {
      return false;
    }
  }

  serialLogPrintln("[FW][PCA9685] Initialization complete; all 16 PWM channels cleared");
  return true;
}

} // namespace

bool i2cMotorBackendBegin(TwoWire &wire) {
  // Инициализация выполняется до запуска WebSocket-управления.
  serialLogPrintln("[FW][PCA9685] Starting FeatherWing backend initialization");
  if (backendMutex == nullptr) {
    backendMutex = xSemaphoreCreateMutex();
  }
  if (backendMutex == nullptr) {
    serialLogPrintln("[FW][PCA9685][ERROR] Failed to create I2C mutex");
    backendReady = false;
    return false;
  }

  BackendLock lock(pdMS_TO_TICKS(1000));
  if (!lock.acquired()) {
    serialLogPrintln("[FW][PCA9685][ERROR] Failed to acquire I2C mutex during initialization");
    backendReady = false;
    return false;
  }

  backendReady = false;
  rejectedCommandLogged = false;
  motorWire = &wire;
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    lastDirection[i] = 0;
    lastReportedSpeed[i] = 127;
  }

  if (!initializePca9685()) {
    backendReady = false;
    serialLogPrintln("[FW][PCA9685][ERROR] Backend NOT READY; motor commands will be rejected until reboot");
    return false;
  }

  backendReady = true;
  serialLogPrintln("[FW][PCA9685][OK] Backend READY; A/B/E/F mapped to M1/M2/M3/M4");
  return true;
}

bool i2cMotorBackendIsReady() {
  BackendLock lock;
  return lock.acquired() && backendReady;
}

bool i2cMotorBackendLockSharedI2c(uint32_t timeoutMs) {
  return backendMutex != nullptr &&
         xSemaphoreTake(backendMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void i2cMotorBackendUnlockSharedI2c() {
  if (backendMutex != nullptr) {
    xSemaphoreGive(backendMutex);
  }
}

bool i2cMotorBackendSetMotor(I2CMotorChannel channel, int speedPercent) {
  BackendLock lock;
  if (!lock.acquired()) {
    return false;
  }

  const uint8_t index = static_cast<uint8_t>(channel);
  if (!backendReady || index >= MOTOR_COUNT) {
    if (!rejectedCommandLogged) {
      serialLogPrintf("[FW][PCA9685][WARN] Motor command rejected: channel=%u backendReady=%s\n",
                    static_cast<unsigned>(index), backendReady ? "true" : "false");
      rejectedCommandLogged = true;
    }
    return false;
  }

  speedPercent = constrain(speedPercent, -100, 100);

  if (speedPercent == 0) {
    if (!setMotorSpeed(index, 0) || !releaseMotor(index)) {
      return false;
    }
    lastDirection[index] = 0;
    if (lastReportedSpeed[index] != 0) {
      serialLogPrintf("[FW][PCA9685][CMD] %s speed=0%% (release)\n", motorName(index));
      lastReportedSpeed[index] = 0;
    }
    return true;
  }

  const int8_t direction = speedPercent > 0 ? 1 : -1;

  if (lastDirection[index] != 0 && lastDirection[index] != direction) {
    if (!setMotorSpeed(index, 0) || !releaseMotor(index)) {
      return false;
    }
  }

  if (!setMotorDirection(index, direction)) {
    return false;
  }

  const int magnitude = speedPercent > 0 ? speedPercent : -speedPercent;
  const uint8_t pwm = static_cast<uint8_t>((magnitude * 255 + 50) / 100);
  if (!setMotorSpeed(index, pwm)) {
    return false;
  }

  lastDirection[index] = direction;
  if (lastReportedSpeed[index] != speedPercent) {
    serialLogPrintf("[FW][PCA9685][CMD] %s speed=%d%% direction=%s pwm=%u\n",
                  motorName(index), speedPercent, direction > 0 ? "forward" : "reverse", static_cast<unsigned>(pwm));
    lastReportedSpeed[index] = speedPercent;
  }
  return true;
}

void i2cMotorBackendStopAll() {
  BackendLock lock;
  if (!lock.acquired() || !backendReady) {
    return;
  }

  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    if (!setMotorSpeed(i, 0) || !releaseMotor(i)) {
      // Ошибка уже защёлкнула backendReady=false. Не отправляем новые
      // команды по потенциально неисправной шине.
      return;
    }
    lastDirection[i] = 0;
    lastReportedSpeed[i] = 0;
  }
  serialLogPrintln("[FW][PCA9685] STOP ALL completed");
}
