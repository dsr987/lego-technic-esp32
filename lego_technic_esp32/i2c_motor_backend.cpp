#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "i2c_motor_backend.h"
#include "serial_log.h"

namespace {

constexpr uint8_t I2C_MOTOR_WING_ADDRESS = 0x60;
constexpr uint16_t MOTOR_PWM_FREQUENCY_HZ = 1600;
constexpr uint8_t MOTOR_COUNT = 4;

// Официальная библиотека Adafruit управляет PCA9685 и TB6612FNG целиком.
// Порядок объектов соответствует логическим каналам A/B/E/F и физическим M1–M4.
Adafruit_MotorShield motorShield(I2C_MOTOR_WING_ADDRESS);
Adafruit_DCMotor *motors[MOTOR_COUNT] = {nullptr, nullptr, nullptr, nullptr};

int8_t lastDirection[MOTOR_COUNT] = {0, 0, 0, 0};
int16_t lastReportedSpeed[MOTOR_COUNT] = {127, 127, 127, 127};
bool backendReady = false;
bool rejectedCommandLogged = false;
SemaphoreHandle_t backendMutex = nullptr;
TwoWire *backendWire = nullptr;

// Назначения выходов PCA9685 по таблице Adafruit Motor Shield V2.
constexpr uint8_t MOTOR_PWM_PINS[MOTOR_COUNT] = {8, 13, 2, 7};
constexpr uint8_t MOTOR_IN1_PINS[MOTOR_COUNT] = {10, 11, 4, 5};
constexpr uint8_t MOTOR_IN2_PINS[MOTOR_COUNT] = {9, 12, 3, 6};
constexpr uint8_t REG_PCA9685_MODE1 = 0x00;
constexpr uint8_t PCA9685_MODE2 = 0x01;
constexpr uint8_t PCA9685_LED0_ON_L = 0x06;
constexpr uint8_t REG_PCA9685_PRESCALE = 0xFE;

const char *motorName(uint8_t index) {
  static const char *NAMES[MOTOR_COUNT] = {"A/M1", "B/M2", "E/M3", "F/M4"};
  return index < MOTOR_COUNT ? NAMES[index] : "UNKNOWN";
}

bool readPca9685Register(uint8_t reg, uint8_t &value) {
  if (backendWire == nullptr) {
    return false;
  }

  backendWire->beginTransmission(I2C_MOTOR_WING_ADDRESS);
  backendWire->write(reg);
  const uint8_t txStatus = backendWire->endTransmission(false);
  if (txStatus != 0) {
    serialLogPrintf("[FW][PCA9685][READ ERROR] reg=0x%02X I2C tx=%u\n",
                    static_cast<unsigned>(reg),
                    static_cast<unsigned>(txStatus));
    return false;
  }

  const uint8_t received = static_cast<uint8_t>(
      backendWire->requestFrom(I2C_MOTOR_WING_ADDRESS, static_cast<uint8_t>(1)));
  if (received != 1 || backendWire->available() < 1) {
    serialLogPrintf("[FW][PCA9685][READ ERROR] reg=0x%02X received=%u\n",
                    static_cast<unsigned>(reg),
                    static_cast<unsigned>(received));
    return false;
  }

  value = static_cast<uint8_t>(backendWire->read());
  return true;
}

void logPca9685Channel(uint8_t motorIndex, const char *signal, uint8_t channel) {
  const uint8_t firstRegister = static_cast<uint8_t>(PCA9685_LED0_ON_L + channel * 4);
  uint8_t values[4] = {0, 0, 0, 0};
  for (uint8_t i = 0; i < 4; ++i) {
    if (!readPca9685Register(static_cast<uint8_t>(firstRegister + i), values[i])) {
      serialLogPrintf("[FW][PCA9685][READBACK] %s %s CH%u unavailable\n",
                      motorName(motorIndex), signal,
                      static_cast<unsigned>(channel));
      return;
    }
  }

  const uint16_t on = static_cast<uint16_t>(
      (static_cast<uint16_t>(values[1] & 0x0F) << 8) | values[0]);
  const uint16_t off = static_cast<uint16_t>(
      (static_cast<uint16_t>(values[3] & 0x0F) << 8) | values[2]);
  const bool fullOn = (values[1] & 0x10) != 0;
  const bool fullOff = (values[3] & 0x10) != 0;

  serialLogPrintf(
      "[FW][PCA9685][READBACK] %s %s CH%u ON=%03X OFF=%03X FULL_ON=%u FULL_OFF=%u\n",
      motorName(motorIndex), signal, static_cast<unsigned>(channel),
      static_cast<unsigned>(on), static_cast<unsigned>(off),
      fullOn ? 1U : 0U, fullOff ? 1U : 0U);
}

void logPca9685MotorState(uint8_t motorIndex) {
  uint8_t mode1 = 0;
  uint8_t mode2 = 0;
  uint8_t prescale = 0;
  const bool mode1Ok = readPca9685Register(REG_PCA9685_MODE1, mode1);
  const bool mode2Ok = readPca9685Register(PCA9685_MODE2, mode2);
  const bool prescaleOk = readPca9685Register(REG_PCA9685_PRESCALE, prescale);

  serialLogPrintf(
      "[FW][PCA9685][READBACK] %s MODE1=%s0x%02X MODE2=%s0x%02X PRESCALE=%s0x%02X\n",
      motorName(motorIndex), mode1Ok ? "" : "ERR/",
      static_cast<unsigned>(mode1), mode2Ok ? "" : "ERR/",
      static_cast<unsigned>(mode2), prescaleOk ? "" : "ERR/",
      static_cast<unsigned>(prescale));

  logPca9685Channel(motorIndex, "PWM", MOTOR_PWM_PINS[motorIndex]);
  logPca9685Channel(motorIndex, "IN1", MOTOR_IN1_PINS[motorIndex]);
  logPca9685Channel(motorIndex, "IN2", MOTOR_IN2_PINS[motorIndex]);
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

bool initializeMotorShield(TwoWire &wire) {
  serialLogPrintf(
      "[FW][MOTOR SHIELD] Initializing Adafruit Motor Shield V2 at 0x%02X, PWM=%u Hz\n",
      static_cast<unsigned>(I2C_MOTOR_WING_ADDRESS),
      static_cast<unsigned>(MOTOR_PWM_FREQUENCY_HZ));

  if (!motorShield.begin(MOTOR_PWM_FREQUENCY_HZ, &wire)) {
    serialLogPrintln("[FW][MOTOR SHIELD][ERROR] Adafruit library initialization failed");
    return false;
  }

  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    motors[i] = motorShield.getMotor(static_cast<uint8_t>(i + 1));
    if (motors[i] == nullptr) {
      serialLogPrintf("[FW][MOTOR SHIELD][ERROR] Failed to get motor object M%u\n",
                      static_cast<unsigned>(i + 1));
      return false;
    }

    motors[i]->run(RELEASE);
    lastDirection[i] = 0;
    lastReportedSpeed[i] = 127;
  }

  serialLogPrintln("[FW][MOTOR SHIELD][OK] Adafruit backend READY; A/B/E/F mapped to M1/M2/M3/M4");
  return true;
}

} // namespace

bool i2cMotorBackendBegin(TwoWire &wire) {
  // Инициализация выполняется до запуска WebSocket-управления.
  serialLogPrintln("[FW][MOTOR SHIELD] Starting Adafruit backend initialization");
  if (backendMutex == nullptr) {
    backendMutex = xSemaphoreCreateMutex();
  }
  if (backendMutex == nullptr) {
    serialLogPrintln("[FW][MOTOR SHIELD][ERROR] Failed to create I2C mutex");
    backendReady = false;
    return false;
  }

  BackendLock lock(pdMS_TO_TICKS(1000));
  if (!lock.acquired()) {
    serialLogPrintln("[FW][MOTOR SHIELD][ERROR] Failed to acquire I2C mutex during initialization");
    backendReady = false;
    return false;
  }

  backendReady = false;
  rejectedCommandLogged = false;
  backendWire = &wire;
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    motors[i] = nullptr;
    lastDirection[i] = 0;
    lastReportedSpeed[i] = 127;
  }

  if (!initializeMotorShield(wire)) {
    backendReady = false;
    serialLogPrintln("[FW][MOTOR SHIELD][ERROR] Backend NOT READY; motor commands will be rejected until reboot");
    return false;
  }

  backendReady = true;
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
  if (!backendReady || index >= MOTOR_COUNT || motors[index] == nullptr) {
    if (!rejectedCommandLogged) {
      serialLogPrintf("[FW][MOTOR SHIELD][WARN] Motor command rejected: channel=%u backendReady=%s\n",
                      static_cast<unsigned>(index), backendReady ? "true" : "false");
      rejectedCommandLogged = true;
    }
    return false;
  }

  speedPercent = constrain(speedPercent, -100, 100);
  Adafruit_DCMotor *motor = motors[index];

  if (speedPercent == 0) {
    motor->run(RELEASE);
    lastDirection[index] = 0;
    if (lastReportedSpeed[index] != 0) {
      serialLogPrintf("[FW][MOTOR SHIELD][CMD] %s speed=0%% (release)\n", motorName(index));
      lastReportedSpeed[index] = 0;
      logPca9685MotorState(index);
    }
    return true;
  }

  const int8_t direction = speedPercent > 0 ? 1 : -1;
  if (lastDirection[index] != 0 && lastDirection[index] != direction) {
    motor->run(RELEASE);
  }

  const int magnitude = speedPercent > 0 ? speedPercent : -speedPercent;
  const uint8_t pwm = static_cast<uint8_t>((magnitude * 255 + 50) / 100);
  motor->setSpeed(pwm);
  motor->run(direction > 0 ? FORWARD : BACKWARD);

  lastDirection[index] = direction;
  if (lastReportedSpeed[index] != speedPercent) {
    serialLogPrintf("[FW][MOTOR SHIELD][CMD] %s speed=%d%% direction=%s pwm=%u\n",
                    motorName(index), speedPercent, direction > 0 ? "forward" : "reverse",
                    static_cast<unsigned>(pwm));
    lastReportedSpeed[index] = speedPercent;
    // Adafruit_MotorShield ignores I2C write status. Read the PCA9685 registers
    // back so the log distinguishes a successful function call from delivered writes.
    logPca9685MotorState(index);
  }
  return true;
}

void i2cMotorBackendStopAll() {
  BackendLock lock;
  if (!lock.acquired() || !backendReady) {
    return;
  }

  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    if (motors[i] == nullptr) {
      backendReady = false;
      serialLogPrintf("[FW][MOTOR SHIELD][ERROR] Motor object M%u is unavailable during STOP ALL\n",
                      static_cast<unsigned>(i + 1));
      return;
    }

    motors[i]->run(RELEASE);
    lastDirection[i] = 0;
    lastReportedSpeed[i] = 0;
  }
  serialLogPrintln("[FW][MOTOR SHIELD] STOP ALL completed");
}
