#include <Arduino.h>
#include <Adafruit_MotorShield.h>
#include <Wire.h>

#include "i2c_motor_backend.h"

namespace {

constexpr uint8_t I2C_MOTOR_WING_ADDRESS = 0x60;
constexpr uint16_t MOTOR_PWM_FREQUENCY_HZ = 1600;
constexpr uint8_t MOTOR_COUNT = 4;

Adafruit_MotorShield shield(I2C_MOTOR_WING_ADDRESS);
Adafruit_DCMotor *motors[MOTOR_COUNT] = {nullptr, nullptr, nullptr, nullptr};
int8_t lastDirection[MOTOR_COUNT] = {0, 0, 0, 0};
bool backendReady = false;

} // namespace

bool i2cMotorBackendBegin(TwoWire &wire) {
  backendReady = false;

  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    motors[i] = nullptr;
    lastDirection[i] = 0;
  }

  // Adafruit_MotorShield::begin() checks for the PCA9685 and clears all
  // PCA9685 outputs on success. The caller owns Wire.begin() and its pins.
  if (!shield.begin(MOTOR_PWM_FREQUENCY_HZ, &wire)) {
    return false;
  }

  // Library ports 1..4 are M1..M4. Do not pass zero-based indexes directly
  // to getMotor(): the Adafruit API is one-based.
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    motors[i] = shield.getMotor(static_cast<uint8_t>(i + 1));
    if (motors[i] == nullptr) {
      return false;
    }

    motors[i]->setSpeed(0);
    motors[i]->run(RELEASE);
  }

  backendReady = true;
  return true;
}

bool i2cMotorBackendIsReady() {
  return backendReady;
}

bool i2cMotorBackendSetMotor(I2CMotorChannel channel, int speedPercent) {
  const uint8_t index = static_cast<uint8_t>(channel);
  if (!backendReady || index >= MOTOR_COUNT || motors[index] == nullptr) {
    return false;
  }

  speedPercent = constrain(speedPercent, -100, 100);
  Adafruit_DCMotor *motor = motors[index];

  if (speedPercent == 0) {
    motor->setSpeed(0);
    motor->run(RELEASE);
    lastDirection[index] = 0;
    return true;
  }

  const int8_t direction = speedPercent > 0 ? 1 : -1;

  // Remove drive before changing direction so the two direction inputs are
  // not switched while the prior non-zero PWM command is still requested.
  if (lastDirection[index] != 0 && lastDirection[index] != direction) {
    motor->setSpeed(0);
    motor->run(RELEASE);
  }

  motor->run(direction > 0 ? FORWARD : BACKWARD);

  const int magnitude = speedPercent > 0 ? speedPercent : -speedPercent;
  const uint8_t pwm = static_cast<uint8_t>((magnitude * 255 + 50) / 100);
  motor->setSpeed(pwm);
  lastDirection[index] = direction;

  return true;
}

void i2cMotorBackendStopAll() {
  if (!backendReady) {
    return;
  }

  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    if (motors[i] != nullptr) {
      motors[i]->setSpeed(0);
      motors[i]->run(RELEASE);
    }
    lastDirection[i] = 0;
  }
}
