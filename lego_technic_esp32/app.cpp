// ESP32 Lego Technic motorization — Версия: 0.3.2
// Библиотеки: ESPAsyncWebServer, AsyncTCP, ArduinoJson, Adafruit_SSD1306, Adafruit_GFX, ESP32Servo, ElegantOTA
// ESP32 core: 2.0.9 (совместимость с LEDC и AsyncWebServer)
// CHANGELOG 0.3.3:
// - проведен полный рефакторинг кода, основные блоки вынесены во внешние файлы и изалекаются через функции:
//--#include "hardware_config.h"
//--#include "motor_control.h"
//--#include "web_page.h"
//--#include "web_control.h"
 

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <ElegantOTA.h>
#include "hardware_config.h"
#include "motor_control.h"
#include "web_page.h"
#include "web_control.h"

// Периферия
#define SERVO_PIN      27
#define OLED_SDA       25
#define OLED_SCL       26
#define BATT_PIN       34
#define LED_FRONT_PIN  32
#define LED_REAR_PIN   33
#define BATT_DIVIDER_FACTOR 0.2680

// ---------- Переменные Режимов и Состояния ----------
Mode currentMode = MODE_TANK;

Preferences prefs;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Servo steerServo;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long lastCmdMillis = 0;
const unsigned long CMD_TIMEOUT_MS = 500;

int motorAVal = 0;
int motorBVal = 0;
int motorCVal = 0;
int motorDVal = 0;
int servoVal  = 0;

bool reverseA = false;
bool reverseB = false;
bool reverseC = false;
bool reverseD = false;

bool ledFrontOn = false;
bool ledRearOn  = false;
bool engineSimOn = false;

// Калибровка сервопривода
const float US_PER_DEGREE = 1000.0 / 180.0;
int steerCenterUs    = 1500;
int steerMaxAngleDeg = 45;

void loadPreferences() {
  reverseA = prefs.getUChar("revA", 0) != 0;
  reverseB = prefs.getUChar("revB", 0) != 0;
  reverseC = prefs.getUChar("revC", 0) != 0;
  reverseD = prefs.getUChar("revD", 0) != 0;
  ledFrontOn = prefs.getUChar("ledF", 0) != 0;
  ledRearOn  = prefs.getUChar("ledR", 0) != 0;
  steerCenterUs = prefs.getInt("steer_c", 1500);
  steerMaxAngleDeg = prefs.getInt("steer_a", 45);
}

void applyLeds() {
  // Свет временно отключён: GPIO32 и GPIO33 используются
  // для управления вторым TB6612FNG.
}

void applySteer(int val) {
  servoVal = val;
  int us = steerCenterUs + (int)((val / 100.0) * steerMaxAngleDeg * US_PER_DEGREE);
  us = constrain(us, 500, 2500);
  steerServo.writeMicroseconds(us);
}

// ---------- Батарея и Мониторинг ----------
float battV = 0.0;
int   battPct = 0;
int   wifiRssi = 0;

#define LOW_BATTERY_THRESHOLD_V 6.0
#define BATTERY_DISCONNECTED_V  0.5
#define REST_SETTLE_MS 400

bool wasAtRest = true;
unsigned long restStartMillis = 0;
bool firstStatusRun = true;
int lowBattCount = 0;

float readBatteryVoltage() {
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(BATT_PIN);
  float vAdc = (sum / 8.0) / 4095.0 * 3.3;
  return vAdc / BATT_DIVIDER_FACTOR;
}

int batteryPercent(float v) {
  static const float PV[] = {6.0, 6.8, 7.0, 7.4, 7.8, 8.2};
  static const float PP[] = {0,   10,  25,  50,  75,  100};
  if (v <= PV[0]) return 0;
  if (v >= PV[5]) return 100;
  for (int i = 0; i < 5; i++) {
    if (v < PV[i + 1]) return (int)(PP[i] + (PP[i + 1] - PP[i]) * (v - PV[i]) / (PV[i + 1] - PV[i]) + 0.5);
  }
  return 100;
}

int getBestRssi() {
  wifi_sta_list_t list;
  if (esp_wifi_ap_get_sta_list(&list) != ESP_OK || list.num == 0) return 0;
  int best = -127;
  for (int i = 0; i < list.num; i++) {
    if (list.sta[i].rssi > best) best = list.sta[i].rssi;
  }
  return best;
}

void updateStatus() {
  battV = readBatteryVoltage();

  bool atRest = (engineSimOn && currentMode == MODE_CAR)
                  ? (motorAVal == 0 && motorCVal == 0 && motorDVal == 0 && servoVal == 0)
                  : (motorAVal == 0 && motorBVal == 0 && motorCVal == 0 && motorDVal == 0 && servoVal == 0);

  if (atRest) {
    if (!wasAtRest) { restStartMillis = millis(); wasAtRest = true; }
    if (firstStatusRun || millis() - restStartMillis > REST_SETTLE_MS) {
      battPct = batteryPercent(battV);
      firstStatusRun = false;
    }
  } else {
    wasAtRest = false;
  }

  wifiRssi = getBestRssi();

  bool below = (battV > BATTERY_DISCONNECTED_V && battV < LOW_BATTERY_THRESHOLD_V);
  if (below) { if (lowBattCount < 3) lowBattCount++; }
  else if (battV > LOW_BATTERY_THRESHOLD_V + 0.3 || battV <= BATTERY_DISCONNECTED_V) lowBattCount = 0;
}

void buildStatus(char *buf, size_t n) {
  snprintf(buf, n, "{\"st\":1,\"v\":%.2f,\"p\":%d,\"r\":%d,\"m\":%d,\"c\":%d,\"ra\":%d,\"rb\":%d,\"rc\":%d,\"rd\":%d,\"lf\":%d,\"lr\":%d,\"es\":%d,\"tr\":%d,\"md\":%d}",
           battV, battPct, wifiRssi, (int)currentMode, (int)ws.count(),
           reverseA ? 1 : 0, reverseB ? 1 : 0, reverseC ? 1 : 0, reverseD ? 1 : 0,
           ledFrontOn ? 1 : 0, ledRearOn ? 1 : 0, engineSimOn ? 1 : 0,
           steerCenterUs - 1500, steerMaxAngleDeg);
}

// ---------- Display ----------
void drawModeIcon() {
  int x = 92, y = 42;
  display.drawRect(x, y, 24, 16, SSD1306_WHITE); // Base

  if (currentMode == MODE_TANK) {
    display.fillRect(x + 6, y + 2, 12, 8, SSD1306_WHITE); // Turret
    display.drawLine(x + 12, y + 6, x + 28, y + 6, SSD1306_WHITE); // Barrel
  } else if (currentMode == MODE_CAR) {
    display.fillCircle(x + 6, y + 18, 3, SSD1306_WHITE); // Wheel L
    display.fillCircle(x + 18, y + 18, 3, SSD1306_WHITE); // Wheel R
    display.fillRect(x + 2, y + 4, 20, 10, SSD1306_WHITE); // Body
  } else { // MODE_TEST
    display.drawLine(x + 12, y + 2, x + 12, y + 14, SSD1306_WHITE); // Handle
    display.drawLine(x + 6, y + 4, x + 18, y + 4, SSD1306_WHITE); // Head top
    display.drawLine(x + 6, y + 8, x + 18, y + 8, SSD1306_WHITE); // Head bottom
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Предупреждение о разряде
  if (lowBattCount >= 3) {
    display.setTextSize(2);
    display.setCursor(12, 10);
    display.print("LOW");
    display.setCursor(12, 30);
    display.print("BATTERY");
    display.setTextSize(1);
    display.setCursor(30, 54);
    display.printf("%.2fV", battV);
    display.display();
    return;
  }

  display.setTextSize(2);
  display.setCursor(0, 0);
  if (currentMode == MODE_TANK) display.print("TANK");
  else if (currentMode == MODE_CAR) display.print("CAR");
  else display.print("TEST");

  display.setCursor(0, 20);
  display.printf("%d%%\n", battPct);

  display.setTextSize(1);
  display.setCursor(0, 44);
  display.printf("%.2fV\n", battV);
  display.setCursor(0, 54);
  display.printf("Clients: %d\n", ws.count());

  drawModeIcon();
  display.display();
}

// ---------- Setup / Loop ----------

void setup() {
  // Аппаратная Serial-отладка отключена.
  // Диагностика остаётся через Wi-Fi.

  // Сначала удерживаем оба TB6612FNG в режиме ожидания.
  pinMode(TB_STBY, OUTPUT);
  digitalWrite(TB_STBY, LOW);

  // Направление каналов A/B/C/D
  pinMode(TB_AIN1, OUTPUT);
  pinMode(TB_AIN2, OUTPUT);
  pinMode(TB_BIN1, OUTPUT);
  pinMode(TB_BIN2, OUTPUT);

  pinMode(TB2_AIN1, OUTPUT);
  pinMode(TB2_AIN2, OUTPUT);
  pinMode(TB2_BIN1, OUTPUT);
  pinMode(TB2_BIN2, OUTPUT);

  // Начальное безопасное состояние входов направления
  digitalWrite(TB_AIN1, LOW);
  digitalWrite(TB_AIN2, LOW);
  digitalWrite(TB_BIN1, LOW);
  digitalWrite(TB_BIN2, LOW);
  digitalWrite(TB2_AIN1, LOW);
  digitalWrite(TB2_AIN2, LOW);
  digitalWrite(TB2_BIN1, LOW);
  digitalWrite(TB2_BIN2, LOW);

  // Четыре канала PWM, 5 кГц, 8 бит
  ledcSetup(LEDC_CH_A, 5000, 8);
  ledcAttachPin(TB_PWMA, LEDC_CH_A);

  ledcSetup(LEDC_CH_B, 5000, 8);
  ledcAttachPin(TB_PWMB, LEDC_CH_B);

  ledcSetup(LEDC_CH_C, 5000, 8);
  ledcAttachPin(TB2_PWMA, LEDC_CH_C);

  ledcSetup(LEDC_CH_D, 5000, 8);
  ledcAttachPin(TB2_PWMB, LEDC_CH_D);

  steerServo.setPeriodHertz(50);
  steerServo.attach(SERVO_PIN, 1000, 2000);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // Статус инициализации на OLED
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.print("INIT...");
  display.display();
  delay(1000); // Задержка перед активацией драйверов

  prefs.begin("cfg", false);
  loadPreferences();
  applyLeds();

  WiFi.softAP("LegoTechnic", "12345678");

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", PAGE_HTML);
  });

  // Эндпоинт для калибровки
  server.on("/calib", HTTP_GET, [](AsyncWebServerRequest *req) {
    String json = "{\"trim\":" + String(steerCenterUs - 1500) +
                  ",\"maxdeg\":" + String(steerMaxAngleDeg) +
                  ",\"revA\":" + String(reverseA ? 1 : 0) +
                  ",\"revB\":" + String(reverseB ? 1 : 0) +
                  ",\"revC\":" + String(reverseC ? 1 : 0) +
                  ",\"ledF\":" + String(ledFrontOn ? 1 : 0) +
                  ",\"ledR\":" + String(ledRearOn ? 1 : 0) + "}";
    req->send(200, "application/json", json);
  });

  ElegantOTA.begin(&server, "admin", "admin");
  server.begin();

  stopAll();
  updateStatus();
  lastCmdMillis = millis();
}

void loop() {
  static unsigned long lastStatus = 0;

  if (millis() - lastCmdMillis > CMD_TIMEOUT_MS) {
  if (motorAVal != 0 ||
      motorBVal != 0 ||
      motorCVal != 0 ||
      motorDVal != 0 ||
      servoVal != 0 ||
      engineSimOn) {
    stopAll();
  }
  }

  if (millis() - lastStatus > 1000) {
    lastStatus = millis();
    updateStatus();
    updateDisplay();
    if (ws.count() > 0) {
      char buf[200];
      buildStatus(buf, sizeof(buf));
      ws.textAll(buf);
    }
  }

  ElegantOTA.loop();
  ws.cleanupClients();
}
