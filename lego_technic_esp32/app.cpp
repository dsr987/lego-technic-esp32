// ESP32 Lego Technic motorization — Версия: 0.3.2
// Библиотеки: ESPAsyncWebServer, AsyncTCP, ArduinoJson, Adafruit_SSD1306, Adafruit_GFX, ESP32Servo, ElegantOTA
// ESP32 core: 2.0.9 (совместимость с LEDC и AsyncWebServer)
// CHANGELOG 0.3.3:
// - проведен полный рефакторинг кода, основные блоки вынесены во внешние файлы и изалекаются через функции:
//--#include "hardware_config.h"
//--#include "motor_control.h"
//--#include "web_page.h"
//--#include "web_control.h"
//--#include "battery_monitor.h"
//--#include "display_control.h"
//--#include "steering_control.h"
//--#include "preferences_manager.h"

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
#include "battery_monitor.h"
#include "display_control.h"
#include "steering_control.h"
#include "preferences_manager.h"

// Периферия
#define SERVO_PIN      27
#define OLED_SDA       25
#define OLED_SCL       26
#define LED_FRONT_PIN  32
#define LED_REAR_PIN   33

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
int steerCenterUs    = 1500;
int steerMaxAngleDeg = 45;

void applyLeds() {
  // Свет временно отключён: GPIO32 и GPIO33 используются
  // для управления вторым TB6612FNG.
}

// ----------- Мониторинг ---------

void buildStatus(char *buf, size_t n) {
  snprintf(buf, n, "{\"st\":1,\"v\":%.2f,\"p\":%d,\"r\":%d,\"m\":%d,\"c\":%d,\"ra\":%d,\"rb\":%d,\"rc\":%d,\"rd\":%d,\"lf\":%d,\"lr\":%d,\"es\":%d,\"tr\":%d,\"md\":%d}",
           battV, battPct, wifiRssi, (int)currentMode, (int)ws.count(),
           reverseA ? 1 : 0, reverseB ? 1 : 0, reverseC ? 1 : 0, reverseD ? 1 : 0,
           ledFrontOn ? 1 : 0, ledRearOn ? 1 : 0, engineSimOn ? 1 : 0,
           steerCenterUs - 1500, steerMaxAngleDeg);
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
