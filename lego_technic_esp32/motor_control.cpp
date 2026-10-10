
#include <Arduino.h>

#include "hardware_config.h"
#include "motor_control.h"

// Состояние объявлено и определяется в app.cpp
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

// Реализация сервопривода пока остаётся в app.cpp
extern void applySteer(int val);


// ---------- Базовое управление мостом TB6612FNG ----------

static void setDCBridge(int in1, int in2, int pwmChannel, int val) {
  val = constrain(val, -100, 100);
  int duty = map(abs(val), 0, 100, 0, 255);

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

  setDCBridge(
    TB_AIN1,
    TB_AIN2,
    LEDC_CH_A,
    reverseA ? -motorAVal : motorAVal
  );

  if (engineSimOn && currentMode == MODE_CAR) {
    int aux = (int)(
      33.0 + 67.0 * abs(motorAVal) / 100.0 + 0.5
    );

    int actualB = reverseB ? -aux : aux;
    motorBVal = actualB;

    setDCBridge(
      TB_BIN1,
      TB_BIN2,
      LEDC_CH_B,
      actualB
    );
  }
}


// ---------- Мотор B ----------

void applyMotorB(int val) {
  motorBVal = constrain(val, -100, 100);

  setDCBridge(
    TB_BIN1,
    TB_BIN2,
    LEDC_CH_B,
    reverseB ? -motorBVal : motorBVal
  );
}


// ---------- Мотор C ----------

void applyMotorC(int val) {
  motorCVal = constrain(val, -100, 100);

  setDCBridge(
    TB2_AIN1,
    TB2_AIN2,
    LEDC_CH_C,
    reverseC ? -motorCVal : motorCVal
  );
}


// ---------- Мотор D ----------

void applyMotorD(int val) {
  motorDVal = constrain(val, -100, 100);

  setDCBridge(
    TB2_BIN1,
    TB2_BIN2,
    LEDC_CH_D,
    reverseD ? -motorDVal : motorDVal
  );
}


// ---------- Общее включение драйверов ----------

void updateDriverStandby() {
  bool active =
    motorAVal != 0 ||
    motorBVal != 0 ||
    motorCVal != 0 ||
    motorDVal != 0 ||
    servoVal != 0 ||
    engineSimOn;

  // Оба TB6612FNG используют общий STBY.
  digitalWrite(TB_STBY, active ? HIGH : LOW);
}


// ---------- Аварийная остановка ----------

void stopAll() {
  // Отключаем имитацию двигателя.
  engineSimOn = false;

  motorAVal = 0;
  motorBVal = 0;
  motorCVal = 0;
  motorDVal = 0;

  setDCBridge(TB_AIN1, TB_AIN2, LEDC_CH_A, 0);
  setDCBridge(TB_BIN1, TB_BIN2, LEDC_CH_B, 0);
  setDCBridge(TB2_AIN1, TB2_AIN2, LEDC_CH_C, 0);
  setDCBridge(TB2_BIN1, TB2_BIN2, LEDC_CH_D, 0);

  applySteer(0);
  updateDriverStandby();
}
