/*
 * LEGO Technic ESP32
 * Main Arduino sketch: filename and containing directory must match.
 * Shared module implementations remain in their dedicated .cpp/.h files.
 * Версия разработки: 0.11.0
 *
 * Текущий выпуск: калибровка измерения напряжения батареи.
 * В проекте также доступны обновлённый Web GUI и OLED Update 2.0.
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
 * История предыдущего выпуска:
 * 0.9.0 — OLED Update 2.0 ([PR #10](https://github.com/dsr987/lego-technic-esp32/pull/10))
 * - Добавлены растровые пиктограммы режимов и заставка проекта на OLED.
 * - Добавлена короткая неблокирующая анимация запуска «Initialising…».
 * - Рабочая панель OLED показывает режим, процент заряда и напряжение батареи, количество подключённых клиентов Wi-Fi и предупреждение о низком заряде.
 * - Если инициализация OLED не удалась, прошивка продолжает запуск без попыток выводить данные на недоступный дисплей.
 *
 * CHANGELOG_VERSION: 0.11.0
 * CHANGELOG_START
 * - Добавлено автоматическое позиционирование мотора C по аналоговой обратной связи с тремя фиксированными положениями. ([PR #104](https://github.com/dsr987/lego-technic-esp32/pull/104))
 * - Добавлена безопасная пауза перед реверсом при смене цели во время движения.
 * CHANGELOG_END
 *
 * тестовый запуск сборки
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
#include "positional_servo.h"
#include "preferences_manager.h"
#include "i2c_motor_backend.h"

// ============================================================
// НАСТРОЙКИ ДОСТУПА — МЕНЯТЬ ЗДЕСЬ
// ============================================================

// Имя и пароль Wi-Fi-точки доступа ESP32.
// Пароль Wi-Fi должен содержать не менее 8 символов.
constexpr char AP_SSID[]     = "LegoTechnic";
constexpr char AP_PASSWORD[] = "12345678";

// Логин и пароль для входа в ElegantOTA (/update).
constexpr char OTA_USERNAME[] = "admin";
constexpr char OTA_PASSWORD[] = "admin";

// ВАЖНО:
// Такие же значения необходимо указать в файле
// firmware/bootstrap/LEGO_Technic_ESP32_Bootstrap/
// LEGO_Technic_ESP32_Bootstrap.ino
// ============================================================

// ---------- Периферия ----------

// SERVO_PIN, I2C_SDA и I2C_SCL определены в hardware_config.h.
#define OLED_SDA  I2C_SDA
#define OLED_SCL  I2C_SCL

// ---------- Режим и состояние системы ----------

Mode currentMode = MODE_TANK;

// Объект постоянных настроек
Preferences prefs;

// OLED-дисплей
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledAvailable = false;

// Рулевой сервопривод
Servo steerServo;

// Веб-сервер и WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------- Тайм-аут управления ----------

// Если команды управления не поступают в течение
// указанного времени, моторы должны быть остановлены.
// Эти метки записываются асинхронным обработчиком WebSocket и читаются из loop().
volatile unsigned long lastCmdMillis = 0; // Последняя команда управления или пакет heartbeat с актуальными значениями
const unsigned long CMD_TIMEOUT_MS = 500;
unsigned long lastDisplayStatusMillis = 0;

// ---------- Текущие значения каналов ----------

int motorAVal = 0;
int motorBVal = 0;
int motorCVal = 0;
int motorDVal = 0;
int motorEVal = 0;
int motorFVal = 0;
int servoVal  = 0;

// ---------- Настройки реверса ----------

bool reverseA = false;
bool reverseB = false;
bool reverseC = false;
bool reverseD = false;
bool reverseE = false;
bool reverseF = false;

// Режимы управления каналами: 0 — слайдер, 1 — кнопки, 2 — позиционный (только C).
uint8_t motorModeA = 0;
uint8_t motorModeB = 0;
uint8_t motorModeC = 0;
uint8_t motorModeD = 0;
uint8_t motorModeE = 0;
uint8_t motorModeF = 0;

// ---------- Состояние света ----------

// Состояния сохранены для совместимости с интерфейсом.
// GPIO для фар пока не назначены; GPIO32 и GPIO33 свободны в Hardware pre2.0.
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
   * GPIO32 и GPIO33 пока не назначены.
   *
   * После выбора отдельных GPIO для фар здесь
   * будет реализовано управление светом.
   */
}

// ---------- Формирование статуса для веб-интерфейса ----------

// Статус GUI: батарея, Wi-Fi, режим, количество клиентов, конфигурация и ресурсы ESP32.
void buildStatus(char *buf, size_t n) {
  const int trimOffsetUs = steerCenterUs - 1500;
  const int steerTrimDegrees = trimOffsetUs >= 0
      ? (trimOffsetUs * 180 + 500) / 1000
      : (trimOffsetUs * 180 - 500) / 1000;

  snprintf(
    buf,
    n,
    "{\"st\":1,\"v\":%.2f,\"p\":%d,\"r\":%d,"
    "\"m\":%d,\"c\":%d,\"sta\":%d,"
    "\"up\":%lu,\"heap\":%lu,\"minh\":%lu,\"lb\":%d,"
    "\"ra\":%d,\"rb\":%d,\"rc\":%d,\"rd\":%d,\"re\":%d,\"rf\":%d,"
    "\"lf\":%d,\"lr\":%d,\"es\":%d,"
    "\"cfgA\":%d,\"cfgB\":%d,\"cfgC\":%d,\"cfgD\":%d,\"cfgE\":%d,\"cfgF\":%d,"
    "\"tr\":%d,\"md\":%d}",
    battV,
    battPct,
    wifiRssi,
    (int)currentMode,
    (int)ws.count(),
    (int)WiFi.softAPgetStationNum(),
    (unsigned long)(millis() / 1000UL),
    (unsigned long)ESP.getFreeHeap(),
    (unsigned long)ESP.getMinFreeHeap(),
    lowBattCount,
    reverseA ? 1 : 0,
    reverseB ? 1 : 0,
    reverseC ? 1 : 0,
    reverseD ? 1 : 0,
    reverseE ? 1 : 0,
    reverseF ? 1 : 0,
    ledFrontOn ? 1 : 0,
    ledRearOn ? 1 : 0,
    engineSimOn ? 1 : 0,
    (int)motorModeA,
    (int)motorModeB,
    (int)motorModeC,
    (int)motorModeD,
    (int)motorModeE,
    (int)motorModeF,
    steerTrimDegrees,
    steerMaxAngleDeg
  );
}

// ---------- Инициализация ----------

void setup() {
  // Аппаратная Serial-отладка не используется.
  // Диагностика доступна через Wi-Fi и веб-интерфейс.

  // 1. Безопасное состояние единственного прямого TB6612FNG (каналы C/D).
  pinMode(TB_STBY, OUTPUT);
  digitalWrite(TB_STBY, LOW);

  pinMode(TB_AIN1, OUTPUT);
  pinMode(TB_AIN2, OUTPUT);
  pinMode(TB_BIN1, OUTPUT);
  pinMode(TB_BIN2, OUTPUT);

  digitalWrite(TB_AIN1, LOW);
  digitalWrite(TB_AIN2, LOW);
  digitalWrite(TB_BIN1, LOW);
  digitalWrite(TB_BIN2, LOW);

  // Каналы C/D используют прямой PWM ESP32. A/B/E/F идут через PCA9685.
  ledcSetup(LEDC_CH_C, 5000, 8);
  ledcAttachPin(TB_PWMA, LEDC_CH_C);
  ledcSetup(LEDC_CH_D, 5000, 8);
  ledcAttachPin(TB_PWMB, LEDC_CH_D);

  // 4. Инициализация рулевого сервопривода.
  steerServo.setPeriodHertz(50);
  // Positional servo: allow the full calibrated pulse range for up to 270-degree steering.
  steerServo.attach(SERVO_PIN, 500, 2500);

  // 5. Запуск общей I2C-шины для OLED и PCA9685 FeatherWing.
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setTimeOut(50);

  // Инициализируем PCA9685 до OLED, чтобы создать общую блокировку Wire.
  // Отсутствующий FeatherWing не должен останавливать запуск Wi-Fi/OLED.
  const bool featherWingReady = i2cMotorBackendBegin(Wire);
  (void)featherWingReady;

  const bool displayLockAcquired = i2cMotorBackendLockSharedI2c(1000);
  oledAvailable = displayLockAcquired && display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (displayLockAcquired) {
    i2cMotorBackendUnlockSharedI2c();
  }

  if (oledAvailable) {
    // Фирменная заставка появляется только после успешного определения OLED.
    showBootLogo();
    delay(1000);

    // Оставляем экран запуска на время инициализации периферии и сети.
    showInitialisingScreen();
  }

  // 6. Открытие хранилища и загрузка настроек.
  // prefs.begin() обязательно выполняется до loadPreferences().
  prefs.begin("cfg", false);
  loadPreferences();

  // Аналоговая обратная связь позиционного мотора C (GPIO35).
  positionalServoBegin();

  // Свет пока не управляется физически, функция оставлена
  // для совместимости с веб-командами.
  applyLeds();

  // 7. Запуск точки доступа Wi-Fi.
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  // 8. Настройка WebSocket и HTTP-сервера.
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Главная страница веб-интерфейса.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", PAGE_HTML);
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

  // 9. Настройка OTA и явный запуск HTTP-сервера.
  // ElegantOTA регистрирует OTA-маршруты, но не заменяет server.begin().
  ElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);
  server.begin();

  // 10. Приводим моторы и руль в начальное состояние.
  stopAll();

  // Первичное обновление данных мониторинга.
  updateStatus();

  // Запускаем отсчёт тайм-аутов после завершения инициализации.
  lastCmdMillis = millis();
  lastDisplayStatusMillis = millis();
  startDisplayStartupAnimation();
}

// ---------- Основной цикл ----------

void loop() {

  // Автопозиционирование C выполняется независимо от частоты WebSocket heartbeat.
  positionalServoUpdate();

  // Единственная защита управления: остановить выходы, если пакет с командами
  // и heartbeat не поступал более 500 мс. Повторная проверка исключает гонку
  // с асинхронным WebSocket-обработчиком, который мог обновить метку между чтениями.
  const unsigned long watchdogNow = millis();
  const unsigned long lastCommandAt = lastCmdMillis;
  if ((unsigned long)(watchdogNow - lastCommandAt) > CMD_TIMEOUT_MS) {
    const bool outputsActive = motorAVal != 0 ||
        motorBVal != 0 ||
        (motorCVal != 0 && !positionalServoIsMoving()) ||
        motorDVal != 0 ||
        motorEVal != 0 ||
        motorFVal != 0 ||
        servoVal != 0 ||
        engineSimOn;
    if (outputsActive &&
        (unsigned long)(millis() - lastCmdMillis) > CMD_TIMEOUT_MS) {
      stopAll();
    }
  }

  // Анимация запуска обновляется без блокировки WebSocket, OTA и watchdog.
  const bool startupAnimating = updateDisplayStartup();

  // Периодическое обновление мониторинга и OLED.
  if (millis() - lastDisplayStatusMillis > 1000) {
    lastDisplayStatusMillis = millis();

    updateStatus();
    if (!startupAnimating) {
      updateDisplay();
    }

    // Отправляем актуальный статус подключённым клиентам.
    if (ws.count() > 0) {
      char buf[384];
      buildStatus(buf, sizeof(buf));
      ws.textAll(buf);
    }
  }

  // Обслуживание OTA и очистка отключившихся клиентов.
  ElegantOTA.loop();
  ws.cleanupClients();
}