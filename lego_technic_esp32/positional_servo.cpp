#include <Arduino.h>

#include "hardware_config.h"
#include "motor_control.h"
#include "positional_servo.h"

// Значения напряжения получены измерением существующего механизма.
// Это стартовая калибровка; перед эксплуатацией требуется сверка по ADC ESP32.
static constexpr int FEEDBACK_LEFT_MV   = 1635;
static constexpr int FEEDBACK_CENTER_MV = 816;
static constexpr int FEEDBACK_RIGHT_MV  = 545;

static constexpr int FEEDBACK_TOLERANCE_MV = 45;
static constexpr int FEEDBACK_VALID_MIN_MV = 250;
static constexpr int FEEDBACK_VALID_MAX_MV = 2200;

static constexpr int POSITION_MOTOR_SPEED = 70;
static constexpr unsigned long UPDATE_INTERVAL_MS = 10;
static constexpr unsigned long POSITION_TIMEOUT_MS = 3000;
static constexpr unsigned long STALL_TIMEOUT_MS = 1200;
static constexpr int MIN_PROGRESS_MV = 30;

enum class PositionState : uint8_t {
  Idle,
  Moving,
  Fault
};

static PositionState state = PositionState::Idle;
static int targetVoltageMv = FEEDBACK_CENTER_MV;
static int movementDirection = 0;
static int filteredVoltageMv = 0;
static int progressReferenceMv = 0;
static unsigned long lastUpdateMs = 0;
static unsigned long movementStartMs = 0;
static unsigned long lastProgressMs = 0;
static bool filterReady = false;
static uint8_t invalidReadCount = 0;

static int readFeedbackMillivolts() {
  const int rawMv = (int)analogReadMilliVolts(SERVO_FEEDBACK_PIN);
  if (!filterReady) {
    filteredVoltageMv = rawMv;
    filterReady = true;
  } else {
    // Небольшой IIR-фильтр подавляет шум ADC, не добавляя заметной задержки.
    filteredVoltageMv = (filteredVoltageMv * 3 + rawMv) / 4;
  }
  return filteredVoltageMv;
}

static bool feedbackIsValid(int voltageMv) {
  // Обрыв при внешней подтяжке вниз даёт уровень около 0 В;
  // короткое замыкание на питание даёт уровень выше рабочего диапазона.
  return voltageMv >= FEEDBACK_VALID_MIN_MV &&
         voltageMv <= FEEDBACK_VALID_MAX_MV;
}

static void stopPositionMotor() {
  applyMotorC(0);
  updateDriverStandby();
  movementDirection = 0;
  state = PositionState::Idle;
}

static void setPositionFault() {
  applyMotorC(0);
  updateDriverStandby();
  movementDirection = 0;
  state = PositionState::Fault;
}

static int targetVoltage(PositionalServoTarget target) {
  switch (target) {
    case PositionalServoTarget::Left:  return FEEDBACK_LEFT_MV;
    case PositionalServoTarget::Right: return FEEDBACK_RIGHT_MV;
    case PositionalServoTarget::Center:
    default:                           return FEEDBACK_CENTER_MV;
  }
}

void positionalServoBegin() {
  pinMode(SERVO_FEEDBACK_PIN, INPUT);
  analogSetPinAttenuation(SERVO_FEEDBACK_PIN, ADC_11db);

  filterReady = false;
  invalidReadCount = 0;
  state = PositionState::Idle;
  movementDirection = 0;

  // Первичное заполнение фильтра до приёма команд из WebSocket.
  for (uint8_t i = 0; i < 8; ++i) {
    readFeedbackMillivolts();
    delay(2);
  }
}

bool positionalServoRequest(PositionalServoTarget target) {
  const int currentMv = readFeedbackMillivolts();
  if (!feedbackIsValid(currentMv)) {
    setPositionFault();
    return false;
  }

  targetVoltageMv = targetVoltage(target);
  invalidReadCount = 0;

  const int difference = targetVoltageMv - currentMv;
  if (abs(difference) <= FEEDBACK_TOLERANCE_MV) {
    stopPositionMotor();
    return true;
  }

  // Положительное направление механизма повышает напряжение обратной связи.
  // applyMotorC() инвертирует знак в режиме C=2, сохраняя прежнюю ориентацию UI.
  movementDirection = difference > 0 ? 1 : -1;
  progressReferenceMv = currentMv;
  movementStartMs = millis();
  lastProgressMs = movementStartMs;
  lastUpdateMs = 0;
  state = PositionState::Moving;

  applyMotorC(movementDirection > 0
      ? -POSITION_MOTOR_SPEED
      : POSITION_MOTOR_SPEED);
  updateDriverStandby();
  return true;
}

void positionalServoUpdate() {
  if (state != PositionState::Moving) return;

  const unsigned long now = millis();
  if ((unsigned long)(now - lastUpdateMs) < UPDATE_INTERVAL_MS) return;
  lastUpdateMs = now;

  const int currentMv = readFeedbackMillivolts();
  if (!feedbackIsValid(currentMv)) {
    if (++invalidReadCount >= 3) setPositionFault();
    return;
  }
  invalidReadCount = 0;

  // Проверяем достижение цели именно по направлению движения.
  // Промежуточный контакт не является целью и не может остановить привод.
  const bool targetReached = movementDirection > 0
      ? currentMv >= targetVoltageMv - FEEDBACK_TOLERANCE_MV
      : currentMv <= targetVoltageMv + FEEDBACK_TOLERANCE_MV;

  if (targetReached) {
    stopPositionMotor();
    return;
  }

  if (abs(currentMv - progressReferenceMv) >= MIN_PROGRESS_MV) {
    progressReferenceMv = currentMv;
    lastProgressMs = now;
  }

  if ((unsigned long)(now - movementStartMs) >= POSITION_TIMEOUT_MS ||
      (unsigned long)(now - lastProgressMs) >= STALL_TIMEOUT_MS) {
    setPositionFault();
  }
}

void positionalServoCancel() {
  if (state == PositionState::Moving) {
    applyMotorC(0);
    updateDriverStandby();
  }
  movementDirection = 0;
  state = PositionState::Idle;
}

bool positionalServoIsMoving() {
  return state == PositionState::Moving;
}
