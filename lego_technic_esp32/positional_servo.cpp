#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "hardware_config.h"
#include "motor_control.h"
#include "positional_servo.h"
#include "serial_log.h"

// Значения напряжения получены измерением существующего механизма.
// Это стартовая калибровка; перед эксплуатацией требуется сверка по ADC ESP32.
static constexpr int FEEDBACK_LEFT_MV   = 1635;
static constexpr int FEEDBACK_CENTER_MV = 816;
static constexpr int FEEDBACK_RIGHT_MV  = 545;

static constexpr int FEEDBACK_TOLERANCE_MV = 45;
static constexpr int FEEDBACK_VALID_MIN_MV = 250;
static constexpr int FEEDBACK_VALID_MAX_MV = 2200;
static constexpr int FEEDBACK_ZONE_MIN_MV = 500;
static constexpr int FEEDBACK_ZONE_MAX_MV = 1850;
static constexpr int FEEDBACK_ZONE_HYSTERESIS_MV = 25;
static constexpr int FEEDBACK_ZONE_BOUNDARIES_MV[] = {635, 780, 990, 1375};

static constexpr int POSITION_MOTOR_SPEED = 70; // 100% пользовательской настройки
// Левый ход по журналам проходит последние ~500 мВ слишком быстро для
// штатного окна 150 мВ. Используем две ступени замедления только влево.
static constexpr int LEFT_APPROACH_WINDOW_MV = 600;
static constexpr int LEFT_APPROACH_SPEED_PERCENT = 75;
static constexpr int FINAL_APPROACH_WINDOW_MV = 150;
static constexpr int FINAL_APPROACH_SPEED_PERCENT = 50;
static constexpr unsigned long FEEDBACK_SAMPLE_INTERVAL_MS = 10;
static constexpr int POSITION_MOTOR_RAMP_STEP = 5;
static constexpr unsigned long POSITION_MOTOR_RAMP_INTERVAL_MS = 20;
static constexpr unsigned long UPDATE_INTERVAL_MS = 10;
static constexpr unsigned long POSITION_TIMEOUT_MS = 3000;
static constexpr unsigned long STALL_TIMEOUT_MS = 1200;
static constexpr unsigned long DIRECTION_DEADTIME_MS = 30;
static constexpr int MIN_PROGRESS_MV = 30;

static PositionalServoState state = PositionalServoState::Idle;
static bool targetValid = false;
static int targetVoltageMv = FEEDBACK_CENTER_MV;
static int movementDirection = 0;
static int filteredVoltageMv = 0;
static int rawFeedbackMv = 0;
static unsigned long lastDiagnosticLogMs = 0;
static int progressReferenceMv = 0;
static unsigned long lastUpdateMs = 0;
static unsigned long movementStartMs = 0;
static unsigned long lastProgressMs = 0;
static unsigned long directionChangeStartMs = 0;
static unsigned long lastMotorRampMs = 0;
static int appliedMotorSpeed = 0;
static uint8_t approachPhase = 0; // 0 — рабочая, 1 — раннее замедление, 2 — финальное
static bool filterReady = false;
static uint8_t invalidReadCount = 0;
static int feedbackZone = (int)PositionalServoFeedbackZone::Unknown;
static unsigned long lastFeedbackSampleMs = 0;
static int speedPercent = 50;

// WebSocket callbacks run in the AsyncTCP task while positionalServoUpdate()
// runs in loop(). A mutex protects controller state and all C-channel writes.
static SemaphoreHandle_t controllerMutex = nullptr;
static portMUX_TYPE cancelRequestMux = portMUX_INITIALIZER_UNLOCKED;
static bool cancelRequested = false;

// Never let a WebSocket callback or the main loop wait forever for controller state.
// WebSocket calls wait at most 10 ms; the control loop uses a zero-wait try-lock.
static bool lockController(uint32_t timeoutMs = 10) {
  return controllerMutex != nullptr &&
      xSemaphoreTake(controllerMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

static void unlockController() {
  if (controllerMutex != nullptr) {
    xSemaphoreGive(controllerMutex);
  }
}

static void requestControllerCancel() {
  portENTER_CRITICAL(&cancelRequestMux);
  cancelRequested = true;
  portEXIT_CRITICAL(&cancelRequestMux);
}

static bool consumeControllerCancel() {
  portENTER_CRITICAL(&cancelRequestMux);
  const bool requested = cancelRequested;
  cancelRequested = false;
  portEXIT_CRITICAL(&cancelRequestMux);
  return requested;
}

static void clearControllerCancel() {
  portENTER_CRITICAL(&cancelRequestMux);
  cancelRequested = false;
  portEXIT_CRITICAL(&cancelRequestMux);
}

static void updateFeedbackZone(int voltageMv) {
  if (voltageMv < FEEDBACK_ZONE_MIN_MV || voltageMv > FEEDBACK_ZONE_MAX_MV) {
    feedbackZone = (int)PositionalServoFeedbackZone::Unknown;
    return;
  }

  int candidateZone = 0;
  while (candidateZone < 4 &&
         voltageMv >= FEEDBACK_ZONE_BOUNDARIES_MV[candidateZone]) {
    ++candidateZone;
  }

  if (feedbackZone < 0 || feedbackZone > 4 ||
      feedbackZone == (int)PositionalServoFeedbackZone::Unknown) {
    feedbackZone = candidateZone;
    return;
  }

  while (feedbackZone < candidateZone && feedbackZone < 4 &&
         voltageMv >= FEEDBACK_ZONE_BOUNDARIES_MV[feedbackZone] +
                          FEEDBACK_ZONE_HYSTERESIS_MV) {
    ++feedbackZone;
  }
  while (feedbackZone > candidateZone && feedbackZone > 0 &&
         voltageMv < FEEDBACK_ZONE_BOUNDARIES_MV[feedbackZone - 1] -
                         FEEDBACK_ZONE_HYSTERESIS_MV) {
    --feedbackZone;
  }
}

static int readFeedbackMillivolts() {
  const int rawMv = (int)analogReadMilliVolts(SERVO_FEEDBACK_PIN);
  rawFeedbackMv = rawMv;
  if (!filterReady) {
    filteredVoltageMv = rawMv;
    filterReady = true;
  } else {
    // Небольшой IIR-фильтр подавляет шум ADC, не добавляя заметной задержки.
    filteredVoltageMv = (filteredVoltageMv * 3 + rawMv) / 4;
  }
  updateFeedbackZone(filteredVoltageMv);
  return filteredVoltageMv;
}

static bool feedbackIsValid(int voltageMv) {
  // Обрыв при внешней подтяжке вниз даёт уровень около 0 В;
  // короткое замыкание на питание даёт уровень выше рабочего диапазона.
  return voltageMv >= FEEDBACK_VALID_MIN_MV &&
         voltageMv <= FEEDBACK_VALID_MAX_MV;
}

static void stopPositionMotor() {
  const bool wasMoving = state == PositionalServoState::Moving ||
      state == PositionalServoState::WaitingForDirectionChange;
  if (wasMoving) {
    serialLogPrintf("[PF][STOP] target=%dmV feedback=%dmV raw=%dmV elapsed=%lums\n",
        targetVoltageMv, filteredVoltageMv, rawFeedbackMv,
        (unsigned long)(millis() - movementStartMs));
  }
  applyMotorC(0);
  updateDriverStandby();
  movementDirection = 0;
  appliedMotorSpeed = 0;
  state = PositionalServoState::Idle;
}

static void setPositionFault(const char *reason) {
  serialLogPrintf("[PF][FAULT] reason=%s state=%d feedback=%dmV raw=%dmV target=%dmV elapsed=%lums noProgress=%lums pwm=%d\n",
      reason ? reason : "unknown", (int)state, filteredVoltageMv, rawFeedbackMv,
      targetVoltageMv, (unsigned long)(millis() - movementStartMs),
      (unsigned long)(millis() - lastProgressMs), appliedMotorSpeed);
  applyMotorC(0);
  updateDriverStandby();
  movementDirection = 0;
  appliedMotorSpeed = 0;
  state = PositionalServoState::Fault;
}

static int targetVoltage(PositionalServoTarget target) {
  switch (target) {
    case PositionalServoTarget::Left:  return FEEDBACK_LEFT_MV;
    case PositionalServoTarget::Right: return FEEDBACK_RIGHT_MV;
    case PositionalServoTarget::Center:
    default:                           return FEEDBACK_CENTER_MV;
  }
}

static void startPositionMotor(unsigned long now) {
  progressReferenceMv = filteredVoltageMv;
  movementStartMs = now;
  lastProgressMs = now;
  lastUpdateMs = now;
  state = PositionalServoState::Moving;
  appliedMotorSpeed = 0;
  approachPhase = 0;
  lastMotorRampMs = now;

  // Запускаем привод с нулевой мощностью; дальнейший набор скорости выполняется в loop().
  applyMotorC(0);
  updateDriverStandby();
}

void positionalServoBegin() {
  if (controllerMutex == nullptr) {
    controllerMutex = xSemaphoreCreateMutex();
  }
  if (controllerMutex == nullptr) {
    state = PositionalServoState::Fault;
    return;
  }

  if (!lockController(100)) {
    state = PositionalServoState::Fault;
    serialLogPrintln("[PF][FAULT] reason=controller mutex unavailable during initialization");
    return;
  }
  pinMode(SERVO_FEEDBACK_PIN, INPUT);
  analogSetPinAttenuation(SERVO_FEEDBACK_PIN, ADC_11db);

  filterReady = false;
  invalidReadCount = 0;
  lastFeedbackSampleMs = 0;
  speedPercent = 50;
  state = PositionalServoState::Idle;
  movementDirection = 0;
  targetValid = false;

  // Первичное заполнение фильтра до приёма команд из WebSocket.
  for (uint8_t i = 0; i < 8; ++i) {
    readFeedbackMillivolts();
    delay(2);
  }
  unlockController();
}

bool positionalServoRequest(PositionalServoTarget target) {
  if (!lockController()) {
    serialLogPrintln("[PF][WARN] request rejected: controller busy");
    return false;
  }
  clearControllerCancel();

  const int currentMv = filteredVoltageMv;
  if (!feedbackIsValid(currentMv)) {
    setPositionFault("invalid feedback before request");
    unlockController();
    return false;
  }

  targetVoltageMv = targetVoltage(target);
  targetValid = true;
  serialLogPrintf("[PF][REQUEST] target=%d targetMv=%d feedback=%dmV raw=%dmV speed=%d%% state=%d\n",
      (int)target, targetVoltageMv, filteredVoltageMv, rawFeedbackMv,
      speedPercent, (int)state);
  invalidReadCount = 0;

  const int difference = targetVoltageMv - currentMv;
  if (abs(difference) <= FEEDBACK_TOLERANCE_MV) {
    stopPositionMotor();
    unlockController();
    return true;
  }

  const int requestedDirection = difference > 0 ? 1 : -1;
  const unsigned long now = millis();

  // Не меняем полярность H-моста на ходу. При реверсе сначала снимаем PWM
  // и отключаем TB6612 через STBY, затем выдерживаем паузу перед новым ходом.
  if ((state == PositionalServoState::Moving ||
       state == PositionalServoState::WaitingForDirectionChange) &&
      requestedDirection != movementDirection) {
    applyMotorC(0);
    updateDriverStandby();
    movementDirection = requestedDirection;
    directionChangeStartMs = now;
    state = PositionalServoState::WaitingForDirectionChange;
    unlockController();
    return true;
  }

  movementDirection = requestedDirection;
  if (state == PositionalServoState::Moving) {
    // Новая цель в том же направлении: продолжаем движение без остановки.
    unlockController();
    return true;
  }

  if (state == PositionalServoState::WaitingForDirectionChange) {
    // Цель изменилась, но направление осталось прежним: сохраняем deadtime.
    unlockController();
    return true;
  }

  startPositionMotor(now);
  lastDiagnosticLogMs = now;
  serialLogPrintf("[PF][START] direction=%s feedback=%dmV raw=%dmV target=%dmV\n",
      movementDirection > 0 ? "LEFT" : "RIGHT", filteredVoltageMv,
      rawFeedbackMv, targetVoltageMv);
  unlockController();
  return true;
}

void positionalServoUpdate() {
  if (!lockController(0)) return;

  if (consumeControllerCancel()) {
    stopPositionMotor();
    movementDirection = 0;
    appliedMotorSpeed = 0;
    state = PositionalServoState::Idle;
    unlockController();
    return;
  }

  const unsigned long now = millis();
  if ((unsigned long)(now - lastFeedbackSampleMs) >= FEEDBACK_SAMPLE_INTERVAL_MS) {
    lastFeedbackSampleMs = now;
    readFeedbackMillivolts();
  }

  if (state != PositionalServoState::Moving &&
      state != PositionalServoState::WaitingForDirectionChange) {
    unlockController();
    return;
  }
  if (state == PositionalServoState::Moving &&
      (unsigned long)(now - lastDiagnosticLogMs) >= 100) {
    lastDiagnosticLogMs = now;
    serialLogPrintf("[PF][MOVE] direction=%s feedback=%dmV raw=%dmV target=%dmV pwm=%d remaining=%dmV elapsed=%lums noProgress=%lums heap=%lu\n",
        movementDirection > 0 ? "LEFT" : "RIGHT", filteredVoltageMv,
        rawFeedbackMv, targetVoltageMv, appliedMotorSpeed,
        abs(targetVoltageMv - filteredVoltageMv),
        (unsigned long)(now - movementStartMs),
        (unsigned long)(now - lastProgressMs), (unsigned long)ESP.getFreeHeap());
  }

  if (state == PositionalServoState::WaitingForDirectionChange) {
    if ((unsigned long)(now - directionChangeStartMs) < DIRECTION_DEADTIME_MS) {
      unlockController();
      return;
    }

    const int currentMv = filteredVoltageMv;
    if (!feedbackIsValid(currentMv)) {
      if (++invalidReadCount >= 3) setPositionFault("invalid feedback while reversing");
      unlockController();
      return;
    }
    invalidReadCount = 0;

    const bool targetReached = movementDirection > 0
        ? currentMv >= targetVoltageMv - FEEDBACK_TOLERANCE_MV
        : currentMv <= targetVoltageMv + FEEDBACK_TOLERANCE_MV;
    if (targetReached) {
      stopPositionMotor();
    } else {
      startPositionMotor(now);
    }
    unlockController();
    return;
  }

  if ((unsigned long)(now - lastUpdateMs) < UPDATE_INTERVAL_MS) {
    unlockController();
    return;
  }
  lastUpdateMs = now;

  // Плавно увеличиваем PWM от 0 до рабочего значения за ~280 мс.
  if ((unsigned long)(now - lastMotorRampMs) >= POSITION_MOTOR_RAMP_INTERVAL_MS) {
    lastMotorRampMs = now;
    const int configuredSpeed = max(1, (POSITION_MOTOR_SPEED * speedPercent + 50) / 100);
    const int remainingMv = abs(targetVoltageMv - filteredVoltageMv);

    uint8_t requestedPhase = 0;
    if (movementDirection > 0) {
      if (remainingMv <= FINAL_APPROACH_WINDOW_MV) {
        requestedPhase = 2;
      } else if (remainingMv <= LEFT_APPROACH_WINDOW_MV) {
        requestedPhase = 1;
      }
    } else if (remainingMv <= FINAL_APPROACH_WINDOW_MV) {
      requestedPhase = 1;
    }

    // Фазу замедления фиксируем до завершения текущего движения: шум ADC
    // не должен случайно вернуть привод на полную скорость.
    if (requestedPhase > approachPhase) {
      approachPhase = requestedPhase;
      const int phasePercent = movementDirection > 0 && approachPhase == 1
          ? LEFT_APPROACH_SPEED_PERCENT
          : FINAL_APPROACH_SPEED_PERCENT;
      const int phaseSpeed = max(1, (configuredSpeed * phasePercent + 50) / 100);
      const int phaseTargetSpeed = movementDirection > 0 ? -phaseSpeed : phaseSpeed;
      serialLogPrintf("[PF][DECEL] direction=%s phase=%u remaining=%dmV pwm=%d targetPwm=%d\\n",
          movementDirection > 0 ? "LEFT" : "RIGHT",
          (unsigned int)approachPhase, remainingMv, appliedMotorSpeed, phaseTargetSpeed);
    }

    const int phasePercent = approachPhase == 0 ? 100
        : (movementDirection > 0 && approachPhase == 1
            ? LEFT_APPROACH_SPEED_PERCENT : FINAL_APPROACH_SPEED_PERCENT);
    const int phaseSpeed = max(1, (configuredSpeed * phasePercent + 50) / 100);
    const int targetSpeed = movementDirection > 0 ? -phaseSpeed : phaseSpeed;

    if (abs(targetSpeed) < abs(appliedMotorSpeed)) {
      // Замедление применяем сразу, а не растягиваем снижение PWM на 80 мс.
      appliedMotorSpeed = targetSpeed;
    } else if (appliedMotorSpeed < targetSpeed) {
      appliedMotorSpeed = min(appliedMotorSpeed + POSITION_MOTOR_RAMP_STEP, targetSpeed);
    } else if (appliedMotorSpeed > targetSpeed) {
      appliedMotorSpeed = max(appliedMotorSpeed - POSITION_MOTOR_RAMP_STEP, targetSpeed);
    }
    applyMotorC(appliedMotorSpeed);
    updateDriverStandby();
  }

  const int currentMv = filteredVoltageMv;
  if (!feedbackIsValid(currentMv)) {
    if (++invalidReadCount >= 3) setPositionFault("invalid feedback while moving");
    unlockController();
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
    unlockController();
    return;
  }

  if (abs(currentMv - progressReferenceMv) >= MIN_PROGRESS_MV) {
    progressReferenceMv = currentMv;
    lastProgressMs = now;
  }

  if ((unsigned long)(now - movementStartMs) >= POSITION_TIMEOUT_MS ||
      (unsigned long)(now - lastProgressMs) >= STALL_TIMEOUT_MS) {
    setPositionFault((unsigned long)(now - movementStartMs) >= POSITION_TIMEOUT_MS
        ? "movement timeout" : "no progress timeout");
  }

  unlockController();
}

void positionalServoCancel() {
  if (controllerMutex == nullptr) {
    state = PositionalServoState::Idle;
    return;
  }
  if (!lockController()) {
    requestControllerCancel();
    return;
  }
  clearControllerCancel();
  if (state == PositionalServoState::Moving ||
      state == PositionalServoState::WaitingForDirectionChange) {
    serialLogPrintf("[PF][CANCEL] feedback=%dmV raw=%dmV target=%dmV elapsed=%lums\n",
        filteredVoltageMv, rawFeedbackMv, targetVoltageMv,
        (unsigned long)(millis() - movementStartMs));
    applyMotorC(0);
    updateDriverStandby();
  }
  movementDirection = 0;
  state = PositionalServoState::Idle;
  unlockController();
}

bool positionalServoIsMoving() {
  if (!lockController(1)) return false;
  const bool moving = state == PositionalServoState::Moving ||
      state == PositionalServoState::WaitingForDirectionChange;
  unlockController();
  return moving;
}

void positionalServoSetSpeedPercent(int percent) {
  if (!lockController()) return;
  speedPercent = constrain(percent, 10, 100);

  // При уменьшении ограничения немедленно снижаем уже применённую скорость.
  // Это не позволяет старому PWM временно превышать новое значение настройки.
  const int maxSpeed = max(1, (POSITION_MOTOR_SPEED * speedPercent + 50) / 100);
  if (state == PositionalServoState::Moving && abs(appliedMotorSpeed) > maxSpeed) {
    appliedMotorSpeed = movementDirection > 0 ? -maxSpeed : maxSpeed;
    applyMotorC(appliedMotorSpeed);
    updateDriverStandby();
  }
  unlockController();
}

int positionalServoGetSpeedPercent() {
  if (!lockController(1)) return 50;
  const int value = speedPercent;
  unlockController();
  return value;
}

void positionalServoGetStatus(PositionalServoStatus &status) {
  status.state = PositionalServoState::Fault;
  status.target = PositionalServoTarget::Center;
  status.feedbackMv = 0;
  status.targetMv = FEEDBACK_CENTER_MV;
  status.targetValid = false;
  status.feedbackValid = false;
  status.targetReached = false;
  status.feedbackZone = PositionalServoFeedbackZone::Unknown;

  if (!lockController(1)) return;

  const int currentMv = filteredVoltageMv;
  status.state = state;
  status.target = targetValid
      ? (targetVoltageMv == FEEDBACK_LEFT_MV ? PositionalServoTarget::Left
        : targetVoltageMv == FEEDBACK_RIGHT_MV ? PositionalServoTarget::Right
        : PositionalServoTarget::Center)
      : PositionalServoTarget::Center;
  status.feedbackMv = currentMv;
  status.feedbackZone = (PositionalServoFeedbackZone)feedbackZone;
  status.targetMv = targetVoltageMv;
  status.targetValid = targetValid;
  status.feedbackValid = feedbackIsValid(currentMv);
  status.targetReached = state == PositionalServoState::Idle &&
      targetValid && status.feedbackValid &&
      abs(currentMv - targetVoltageMv) <= FEEDBACK_TOLERANCE_MV;
  unlockController();
}
