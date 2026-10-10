/*
 * LEGO Technic ESP32
 * Версия разработки: 0.3.3 Refactoring
 * Ветка: feature/four-dc-motors
 *
 * ВАЖНО:
 * Это подготовительная версия, не готовый релиз.
 * Выпуск 0.3.3 планируется после проверки на реальном оборудовании.
 *
 * Совместимость:
 * - ESP32 Arduino Core 2.0.9
 * - ESPAsyncWebServer
 * - AsyncTCP
 * - Adafruit_SSD1306
 * - Adafruit_GFX
 * - ESP32Servo
 * - Preferences
 * - ElegantOTA
 *
 * CHANGELOG_VERSION: 0.3.3 Refactoring
 * CHANGELOG_START
 * - Код разделён на отдельные модули .h и .cpp.
 * - Управление четырьмя DC-моторами вынесено в motor_control.
 * - HTML-интерфейс вынесен в web_page.
 * - WebSocket-команды вынесены в web_control.
 * - Мониторинг батареи и Wi-Fi вынесен в battery_monitor.
 * - OLED-дисплей вынесен в display_control.
 * - Управление сервоприводом вынесено в steering_control.
 * - Загрузка и сохранение настроек вынесены в preferences_manager.
 * - Конфигурация выводов и PWM-каналов вынесена в hardware_config.h.
 * - GPIO32 и GPIO33 используются вторым TB6612FNG для мотора D.
 * - Функция applyLeds() сохранена, но пока ничего не делает.
 * - Эндпоинт /calib временно отключён: текущий интерфейс его не использует.
 * CHANGELOG_END
 *
 * Перед выпуском:
 * - Проверить работу четырёх моторов и сервопривода на модели.
 * - Проверить остановку моторов при потере WebSocket-соединения.
 * - Проверить восстановление управления после переподключения.
 * - Проверить измерение батареи и OLED.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <ElegantOTA.h>

// ---------- Модули проекта ----------

#include "hardware_config.h"
#include "motor_control.h"
#include "web_page.h"
#include "web_control.h"
#include "battery_monitor.h"
#include "display_control.h"
#include "steering_control.h"
#include "preferences_manager.h"

// ---------- Периферия ----------

#define SERVO_PIN 27
#define OLED_SDA  25
#define OLED_SCL  26

// ---------- Режим и состояние системы ----------

Mode currentMode = MODE_TANK;

// Объект постоянных настроек
Preferences prefs;

// OLED-дисплей
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Рулевой сервопривод
Servo steerServo;

// Веб-сервер и WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------- Тайм-аут управления ----------

// Если команды управления не поступают в течение
// указанного времени, моторы должны быть остановлены.
unsigned long lastCmdMillis = 0;
const unsigned long CMD_TIMEOUT_MS = 500;

// ---------- Текущие значения каналов ----------

int motorAVal = 0;
int motorBVal = 0;
int motorCVal = 0;
int motorDVal = 0;
int servoVal  = 0;

// ---------- Настройки реверса ----------

bool reverseA = false;
bool reverseB = false;
bool reverseC = false;
bool reverseD = false;

// ---------- Состояние света ----------

// Состояния сохранены для совместимости с интерфейсом.
// GPIO для фар пока не назначены: GPIO32 и GPIO33
// используются вторым TB6612FNG.
bool ledFrontOn = false;
bool ledRearOn  = false;

// Режим имитации двигателя
bool engineSimOn = false;

// ---------- Калибровка рулевого сервопривода ----------

int steerCenterUs    = 1500;
int steerMaxAngleDeg = 45;

// ---------- Свет ----------

void applyLeds() {
  /*
   * Временно пустая функция.
   *
   * GPIO32 и GPIO33 используются вторым TB6612FNG:
   * GPIO32 — TB2_BIN1
   * GPIO33 — TB2_BIN2
   *
   * После выбора отдельных GPIO для фар здесь
   * будет реализовано управление светом.
   */
}

// ---------- Формирование статуса для веб-интерфейса ----------

void buildStatus(char *buf, size_t n) {
  snprintf(
    buf,
    n,
    "{\"st\":1,\"v\":%.2f,\"p\":%d,\"r\":%d,"
    "\"m\":%d,\"c\":%d,"
    "\"ra\":%d,\"rb\":%d,\"rc\":%d,\"rd\":%d,"
    "\"lf\":%d,\"lr\":%d,\"es\":%d,"
    "\"tr\":%d,\"md\":%d}",
    battV,
    battPct,
    wifiRssi,
    (int)currentMode,
    (int)ws.count(),
    reverseA ? 1 : 0,
    reverseB ? 1 : 0,
    reverseC ? 1 : 0,
    reverseD ? 1 : 0,
    ledFrontOn ? 1 : 0,
    ledRearOn ? 1 : 0,
    engineSimOn ? 1 : 0,
    steerCenterUs - 1500,
    steerMaxAngleDeg
  );
}

// ---------- Инициализация ----------

void setup() {
  // Аппаратная Serial-отладка не используется.
  // Диагностика доступна через Wi-Fi и веб-интерфейс.

  // 1. Безопасное состояние драйверов.
  // Оба TB6612FNG выключены до завершения настройки.
  pinMode(TB_STBY, OUTPUT);
  digitalWrite(TB_STBY, LOW);

  // 2. Настройка входов направления четырёх моторов.
  pinMode(TB_AIN1, OUTPUT);
  pinMode(TB_AIN2, OUTPUT);

  pinMode(TB_BIN1, OUTPUT);
  pinMode(TB_BIN2, OUTPUT);

  pinMode(TB2_AIN1, OUTPUT);
  pinMode(TB2_AIN2, OUTPUT);

  pinMode(TB2_BIN1, OUTPUT);
  pinMode(TB2_BIN2, OUTPUT);

  // Устанавливаем безопасное начальное состояние входов.
  digitalWrite(TB_AIN1, LOW);
  digitalWrite(TB_AIN2, LOW);

  digitalWrite(TB_BIN1, LOW);
  digitalWrite(TB_BIN2, LOW);

  digitalWrite(TB2_AIN1, LOW);
  digitalWrite(TB2_AIN2, LOW);

  digitalWrite(TB2_BIN1, LOW);
  digitalWrite(TB2_BIN2, LOW);

  // 3. Настройка четырёх PWM-каналов.
  // Частота 5 кГц, разрешение 8 бит.
  // LEDC-каналы и GPIO определены в hardware_config.h.

  ledcSetup(LEDC_CH_A, 5000, 8);
  ledcAttachPin(TB_PWMA, LEDC_CH_A);

  ledcSetup(LEDC_CH_B, 5000, 8);
  ledcAttachPin(TB_PWMB, LEDC_CH_B);

  ledcSetup(LEDC_CH_C, 5000, 8);
  ledcAttachPin(TB2_PWMA, LEDC_CH_C);

  ledcSetup(LEDC_CH_D, 5000, 8);
  ledcAttachPin(TB2_PWMB, LEDC_CH_D);

  // 4. Инициализация рулевого сервопривода.
  steerServo.setPeriodHertz(50);
  steerServo.attach(SERVO_PIN, 1000, 2000);

  // 5. Инициализация OLED-дисплея.
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 20);
  display.print("INIT...");
  display.display();

  // Короткая пауза для отображения сообщения запуска.
  // Драйверы всё это время остаются выключенными.
  delay(1000);

  // 6. Открытие хранилища и загрузка настроек.
  // prefs.begin() обязательно выполняется до loadPreferences().
  prefs.begin("cfg", false);
  loadPreferences();

  // Свет пока не управляется физически, функция оставлена
  // для совместимости с веб-командами.
  applyLeds();

  // 7. Запуск точки доступа Wi-Fi.
  WiFi.softAP("LegoTechnic", "12345678");

  // 8. Настройка WebSocket и HTTP-сервера.
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Главная страница веб-интерфейса.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", PAGE_HTML);
  });

  /*
   * ВРЕМЕННО ОТКЛЮЧЁННЫЙ ЭНДПОИНТ /calib
   *
   * В текущем web_page.cpp обращений к /calib нет.
   * Настройки руля и реверса обрабатываются через WebSocket.
   *
   * Если в будущем появится клиент, использующий GET /calib,
   * этот блок можно раскомментировать. Перед включением
   * проверьте соответствие формата JSON клиенту.
   *
   * server.on("/calib", HTTP_GET, [](AsyncWebServerRequest *req) {
   *   String json =
   *       "{\"trim\":" + String(steerCenterUs - 1500) +
   *       ",\"maxdeg\":" + String(steerMaxAngleDeg) +
   *       ",\"revA\":" + String(reverseA ? 1 : 0) +
   *       ",\"revB\":" + String(reverseB ? 1 : 0) +
   *       ",\"revC\":" + String(reverseC ? 1 : 0) +
   *       ",\"revD\":" + String(reverseD ? 1 : 0) +
   *       ",\"ledF\":" + String(ledFrontOn ? 1 : 0) +
   *       ",\"ledR\":" + String(ledRearOn ? 1 : 0) + "}";
   *
   *   req->send(200, "application/json", json);
   * });
   */

  // 9. Запуск OTA-обновления и HTTP-сервера.
  ElegantOTA.begin(&server, "admin", "admin");
  server.begin();

  // 10. Приводим моторы и руль в начальное состояние.
  stopAll();

  // Первичное обновление данных мониторинга.
  updateStatus();

  // Запускаем отсчёт тайм-аута управления.
  lastCmdMillis = millis();
}

// ---------- Основной цикл ----------

void loop() {
  static unsigned long lastStatus = 0;

  // Аварийная остановка при отсутствии команд управления.
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

  // Периодическое обновление мониторинга и OLED.
  if (millis() - lastStatus > 1000) {
    lastStatus = millis();

    updateStatus();
    updateDisplay();

    // Отправляем актуальный статус подключённым клиентам.
    if (ws.count() > 0) {
      char buf[200];
      buildStatus(buf, sizeof(buf));
      ws.textAll(buf);
    }
  }

  // Обслуживание OTA и очистка отключившихся клиентов.
  ElegantOTA.loop();
  ws.cleanupClients();
}
