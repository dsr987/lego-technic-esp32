
#include <Arduino.h>
#include <ESP32Servo.h>

#include "steering_control.h"

// Объекты и состояние определяются в app.cpp
extern Servo steerServo;
extern int servoVal;
extern int steerCenterUs;
extern int steerMaxAngleDeg;