
#pragma once

// Загрузка сохранённых настроек
void loadPreferences();

// Режимы управления моторами: 0 — слайдер, 1 — кнопки, 2 — позиционный (только C).
void saveMotorModeA(uint8_t value);
void saveMotorModeB(uint8_t value);
void saveMotorModeC(uint8_t value);
void saveMotorModeD(uint8_t value);

// Настройки рулевого управления
void saveSteeringCenter(int value);
void saveSteeringMaxAngle(int value);

// Реверс моторов
void saveReverseA(bool value);
void saveReverseB(bool value);
void saveReverseC(bool value);
void saveReverseD(bool value);

// Передний и задний свет
void saveLedFront(bool value);
void saveLedRear(bool value);
