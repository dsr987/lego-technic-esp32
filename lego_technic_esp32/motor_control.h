
#pragma once

// Режимы работы модели
enum Mode {
  MODE_TANK = 0,
  MODE_CAR = 1,
  MODE_TEST = 2
};

// Управление отдельными каналами моторов
void applyMotorA(int val);
void applyMotorB(int val);
void applyMotorC(int val);
void applyMotorD(int val);

// Общее управление драйверами
void updateDriverStandby();
void stopAll();
