
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "motor_control.h"
#include "battery_monitor.h"

// GPIO34 — вход измерения напряжения батареи
#define BATT_PIN 34

// Коэффициент делителя напряжения
#define BATT_DIVIDER_FACTOR 0.2680

// Пороговые значения и параметры фильтрации
static const float LOW_BATTERY_THRESHOLD_V = 6.0;
static const float BATTERY_DISCONNECTED_V = 0.5;
static const unsigned long REST_SETTLE_MS = 400;

// Показатели, доступные другим модулям
float battV = 0.0;
int battPct = 0;
int wifiRssi = 0;
int lowBattCount = 0;

// Состояние мониторинга
static bool wasAtRest = true;
static unsigned long restStartMillis = 0;
static bool firstStatusRun = true;

// Состояние приложения объявлено в app.cpp
extern Mode currentMode;

extern int motorAVal;
extern int motorBVal;
extern int motorCVal;
extern int motorDVal;
extern int servoVal;

extern bool engineSimOn;


// ---------- Измерение напряжения ----------

float readBatteryVoltage() {
  long sum = 0;

  for (int i = 0; i < 8; i++) {
    sum += analogRead(BATT_PIN);
  }

  float vAdc = (sum / 8.0) / 4095.0 * 3.3;
  return vAdc / BATT_DIVIDER_FACTOR;
}


// ---------- Расчёт уровня заряда ----------

int batteryPercent(float v) {
  static const float PV[] = {6.0, 6.8, 7.0, 7.4, 7.8, 8.2};
  static const float PP[] = {0, 10, 25, 50, 75, 100};

  if (v <= PV[0]) return 0;
  if (v >= PV[5]) return 100;

  for (int i = 0; i < 5; i++) {
    if (v < PV[i + 1]) {
      return (int)(
        PP[i] +
        (PP[i + 1] - PP[i]) *
        (v - PV[i]) /
        (PV[i + 1] - PV[i]) + 0.5
      );
    }
  }

  return 100;
}


// ---------- Уровень Wi-Fi-сигнала ----------

int getBestRssi() {
  wifi_sta_list_t list;

  if (esp_wifi_ap_get_sta_list(&list) != ESP_OK || list.num == 0) {
    return 0;
  }

  int best = -127;

  for (int i = 0; i < list.num; i++) {
    if (list.sta[i].rssi > best) {
      best = list.sta[i].rssi;
    }
  }

  return best;
}


// ---------- Обновление показателей ----------

void updateStatus() {
  battV = readBatteryVoltage();

  bool atRest = (engineSimOn && currentMode == MODE_CAR)
    ? (
        motorAVal == 0 &&
        motorCVal == 0 &&
        motorDVal == 0 &&
        servoVal == 0
      )
    : (
        motorAVal == 0 &&
        motorBVal == 0 &&
        motorCVal == 0 &&
        motorDVal == 0 &&
        servoVal == 0
      );

  if (atRest) {
    if (!wasAtRest) {
      restStartMillis = millis();
      wasAtRest = true;
    }

    if (firstStatusRun ||
        millis() - restStartMillis > REST_SETTLE_MS) {
      battPct = batteryPercent(battV);
      firstStatusRun = false;
    }
  } else {
    wasAtRest = false;
  }

  wifiRssi = getBestRssi();

  bool below = (
    battV > BATTERY_DISCONNECTED_V &&
    battV < LOW_BATTERY_THRESHOLD_V
  );

  if (below) {
    if (lowBattCount < 3) {
      lowBattCount++;
    }
  } else if (
    battV > LOW_BATTERY_THRESHOLD_V + 0.3 ||
    battV <= BATTERY_DISCONNECTED_V
  ) {
    lowBattCount = 0;
  }
}
