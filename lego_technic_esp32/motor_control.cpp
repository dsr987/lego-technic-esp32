
#include <Arduino.h>

#include "motor_control.h"
#include "i2c_motor_backend.h"
#include "steering_control.h"

// Состояние объявлено и определяется в app.cpp
extern Mode currentMode;

extern int motorAVal;
extern int motorBVal;
extern int motorCVal;
extern int motorDVal;

extern bool reverseA;
extern bool reverseB;
extern bool reverseC;
extern bool reverseD;
extern bool engineSimOn;


// ---------- Мотор A ----------

void applyMotorA(int val) {
  motorAVal = constrain(val, -100, 100);

  i2cMotorBackendSetMotor(
    I2CMotorChannel::A,
    reverseA ? -motorAVal : motorAVal
  );

  if (engineSimOn && currentMode == MODE_CAR) {
    int aux = (int)(
      33.0 + 67.0 * abs(motorAVal) / 100.0 + 0.5
    );

    int actualB = reverseB ? -aux : aux;
    motorBVal = actualB;

    i2cMotorBackendSetMotor(I2CMotorChannel::B, actualB);
  }
}


// ---------- Мотор B ----------

void applyMotorB(int val) {
  motorBVal = constrain(val, -100, 100);

  i2cMotorBackendSetMotor(
    I2CMotorChannel::B,
    reverseB ? -motorBVal : motorBVal
  );
}


// ---------- Мотор C ----------

void applyMotorC(int val) {
  motorCVal = constrain(val, -100, 100);

  i2cMotorBackendSetMotor(
    I2CMotorChannel::C,
    reverseC ? -motorCVal : motorCVal
  );
}


// ---------- Мотор D ----------

void applyMotorD(int val) {
  motorDVal = constrain(val, -100, 100);

  i2cMotorBackendSetMotor(
    I2CMotorChannel::D,
    reverseD ? -motorDVal : motorDVal
  );
}


// ---------- Общее включение драйверов ----------

void updateDriverStandby() {
  // На PCA9685 Motor Wing нет отдельной линии STBY от ESP32.
  // Каждый канал останавливается через RELEASE, а stopAll() отправляет
  // команды всем четырём каналам.
}


// ---------- Аварийная остановка ----------

void stopAll() {
  // Отключаем имитацию двигателя и сбрасываем логические команды.
  engineSimOn = false;

  motorAVal = 0;
  motorBVal = 0;
  motorCVal = 0;
  motorDVal = 0;

  i2cMotorBackendStopAll();
  applySteer(0);
  updateDriverStandby();
}
