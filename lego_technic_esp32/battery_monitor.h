
#pragma once

// Показатели батареи и Wi-Fi, используемые другими модулями
extern float battV;
extern int battPct;
extern int wifiRssi;

// Счётчик подтверждений низкого заряда для OLED
extern int lowBattCount;

// Мониторинг батареи
float readBatteryVoltage();
int batteryPercent(float voltage);
int getBestRssi();
void updateStatus();
