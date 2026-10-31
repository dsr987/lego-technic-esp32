
#pragma once

#include <Arduino.h>

// Загрузка сохранённых настроек
void loadPreferences();

// Режимы управления моторами: 0 — слайдер, 1 — кнопки, 2 — позиционный (только C).
void saveMotorModeA(uint8_t value);
void saveMotorModeB(uint8_t value);
void saveMotorModeD(uint8_t value);
void saveMotorModeE(uint8_t value);
void saveMotorModeF(uint8_t value);

// Настройки рулевого управления
void saveSteeringCenter(int value);
void saveSteeringMaxAngle(int value);
void savePfServoSpeedPercent(int value);

// Реверс моторов
void saveReverseA(bool value);
void saveReverseB(bool value);
void saveReverseC(bool value);
void saveReverseD(bool value);
void saveReverseE(bool value);
void saveReverseF(bool value);

// Передний и задний свет
void saveLedFront(bool value);
void saveLedRear(bool value);
