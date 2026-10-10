
#pragma once

#include <Preferences.h>

// Загрузка сохранённых настроек
void loadPreferences();

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
