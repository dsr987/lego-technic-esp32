

#include <Arduino.h>
#include <ESP32Servo.h>

#include "steering_control.h"

// Объект и параметры объявлены в lego_technic_esp32.ino
extern Servo steerServo;
extern int servoVal;
extern int steerCenterUs;
extern int steerMaxAngleDeg;

// Преобразование градусов в микросекунды
// Keep the existing 1000 us / 180-degree calibration scale.
// A 135-degree command therefore spans 750 us from the calibrated center.
static const float US_PER_DEGREE = 1000.0 / 180.0;


// ---------- Управление рулевым сервоприводом ----------

static int targetPulseUs = 1500;
static int currentPulseUs = 1500;
static bool pulseInitialized = false;
static unsigned long lastPulseUpdateMs = 0;

void applySteer(int val) {
  servoVal = constrain(val, -100, 100);
  targetPulseUs = steerCenterUs +
      (int)((servoVal / 100.0) * steerMaxAngleDeg * US_PER_DEGREE);
  targetPulseUs = constrain(targetPulseUs, 500, 2500);

  if (!pulseInitialized) {
    currentPulseUs = steerCenterUs;
    pulseInitialized = true;
    steerServo.writeMicroseconds(currentPulseUs);
  }
}

// Вызывать из loop(): ограничиваем шаг без delay() и блокировки WebSocket.
void updateSteering() {
  if (!pulseInitialized) {
    currentPulseUs = steerCenterUs;
    targetPulseUs = steerCenterUs;
    pulseInitialized = true;
    steerServo.writeMicroseconds(currentPulseUs);
    lastPulseUpdateMs = millis();
    return;
  }

  const unsigned long now = millis();
  if (now - lastPulseUpdateMs < 20) return;
  lastPulseUpdateMs = now;

  const int maxStepUs = 20;
  if (currentPulseUs < targetPulseUs) {
    currentPulseUs = min(currentPulseUs + maxStepUs, targetPulseUs);
  } else if (currentPulseUs > targetPulseUs) {
    currentPulseUs = max(currentPulseUs - maxStepUs, targetPulseUs);
  }
  steerServo.writeMicroseconds(currentPulseUs);
}
 