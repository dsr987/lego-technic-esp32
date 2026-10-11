

#include <Arduino.h>
#include <ESP32Servo.h>

#include "steering_control.h"

// Объект и параметры объявлены в app.cpp
extern Servo steerServo;
extern int servoVal;
extern int steerCenterUs;
extern int steerMaxAngleDeg;

// Преобразование градусов в микросекунды
static const float US_PER_DEGREE = 1000.0 / 180.0;


// ---------- Управление рулевым сервоприводом ----------

void applySteer(int val) {
  servoVal = val;

  int us = steerCenterUs +
           (int)((val / 100.0) *
                 steerMaxAngleDeg *
                 US_PER_DEGREE);

  us = constrain(us, 500, 2500);

  steerServo.writeMicroseconds(us);
}
 