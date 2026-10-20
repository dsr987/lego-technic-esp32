#pragma once

#include <Arduino.h>
#include <Wire.h>

// Логические каналы не зависят от физических номеров выходов платы.
enum class I2CMotorChannel : uint8_t {
  A = 0, // M1
  B = 1, // M2
  C = 2, // M3
  D = 3  // M4
};

// Вызывать после Wire.begin(...). Функция использует существующую I2C-шину
// и не запускает вторую шину самостоятельно.
bool i2cMotorBackendBegin(TwoWire &wire = Wire);

// Это состояние инициализации ПО, а не гарантия постоянной доступности I2C.
bool i2cMotorBackendIsReady();

// Общая блокировка Wire для дисплея и моторного backend.
// Вызывать вокруг операций, которые физически обращаются к I²C.
bool i2cMotorBackendLockSharedI2c(uint32_t timeoutMs = 250);
void i2cMotorBackendUnlockSharedI2c();

// Задаёт подписанную скорость в диапазоне -100..100.
// Возвращает false, если backend не инициализирован или канал некорректен.
// Arduino-библиотека не отдаёт статус каждой I2C-записи, поэтому true не
// является подтверждением, что последняя транзакция была физически доставлена.
bool i2cMotorBackendSetMotor(I2CMotorChannel channel, int speedPercent);

// Переводит все четыре канала в RELEASE и устанавливает скорость в ноль.
void i2cMotorBackendStopAll();
