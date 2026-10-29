
#include <Arduino.h>

#include "hardware_config.h"
#include "motor_control.h"
#include "steering_control.h"
#include "i2c_motor_backend.h"

// Состояние объявлено и определяется в lego_technic_esp32.ino
extern uint8_t motorModeC;
extern Mode currentMode;

extern int motorAVal;
extern int motorBVal;
extern int motorCVal;
extern int motorDVal;
extern int servoVal;

extern bool reverseA;
extern bool reverseB;
extern bool reverseC;
extern bool reverseD;
extern bool engineSimOn;


// ---------- Базовое управление мостом TB6612FNG ----------

static void setDCBridge(int in1, int in2, int pwmChannel, int val) {
  val = constrain(val, -100, 100);
  // ESP32 LEDC can behave unexpectedly at the exact 100% duty endpoint (255 at 8-bit resolution).
  // Cap PWM at 254: this is effectively full power but avoids the timer overflow edge case.
  int duty = map(abs(val), 0, 100, 0, 254);

  if (val > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (val < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }

  ledcWrite(pwmChannel, duty);
}


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

  setDCBridge(
    TB_AIN1,
    TB_AIN2,
    LEDC_CH_C,
    (motorModeC == 2 || reverseC) ? -motorCVal : motorCVal
  );
}


// ---------- Мотор D ----------

void applyMotorD(int val) {
  motorDVal = constrain(val, -100, 100);

  setDCBridge(
    TB_BIN1,
    TB_BIN2,
    LEDC_CH_D,
    reverseD ? -motorDVal : motorDVal
  );
}


// ---------- Общее включение драйверов ----------

void updateDriverStandby() {
  // STBY относится только к прямому TB6612 каналов C/D.
  // FeatherWing A/B/E/F управляется независимо через PCA9685.
  const bool directDriverActive = motorCVal != 0 || motorDVal != 0;
  digitalWrite(TB_STBY, directDriverActive ? HIGH : LOW);
}


// ---------- Аварийная остановка ----------

void stopAll() {
  // Отключаем имитацию двигателя.
  engineSimOn = false;

  motorAVal = 0;
  motorBVal = 0;
  motorCVal = 0;
  motorDVal = 0;

  // Останавливаем FeatherWing-каналы A/B/E/F и прямые C/D.
  i2cMotorBackendStopAll();
  setDCBridge(TB_AIN1, TB_AIN2, LEDC_CH_C, 0);
  setDCBridge(TB_BIN1, TB_BIN2, LEDC_CH_D, 0);

  applySteer(0);
  updateDriverStandby();
}
