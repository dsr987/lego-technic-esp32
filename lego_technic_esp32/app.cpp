/*
 * LEGO Technic ESP32
 * Версия разработки: 0.5.29 GUI Update 3.0
 *
 * Обновление веб-интерфейса GUI Update 3.0.
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
 * CHANGELOG_VERSION: 0.5.29
 * CHANGELOG_START
 * - Add a separate battery-voltage calibration factor and a tested helper for deriving it from multimeter readings. ([PR #11](https://github.com/dsr987/lego-technic-esp32/pull/11))
 * CHANGELOG_END
 *
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

#define SERVO_PIN 27
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// ---------- Режим и состояние системы ----------

Mode currentMode = MODE_TANK;

// Объект постоянных настроек
Preferences prefs;

// OLED-дисплей
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool oledAvailable = false;
bool motorBoardReady = false;

// Рулевой сервопривод
Servo steerServo;

// Веб-сервер и WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ---------- Тайм-аут управления ----------

// Если команды управления не поступают в течение
// указанного времени, моторы должны быть остановлены.
unsigned long lastCmdMillis = 0;
const unsigned long CMD_TIMEOUT_MS = 500; // safety: стоп, если нет команд
unsigned long lastDisplayStatusMillis = 0;

// Диагностика защитного тайм-аута хранится только в RAM и сбрасывается при перезапуске.
unsigned long safetyTimeoutCount = 0;
unsigned long lastSafetyTimeoutMillis = 0;
bool safetyTimeoutLatched = false;

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

// Статус GUI: батарея, Wi-Fi, режим, количество WS-клиентов, конфигурация
// и read-only диагностика ресурсов/активности ESP32.
void buildStatus(char *buf, size_t n) {
  snprintf(
    buf,
    n,
    "{\"st\":1,\"v\":%.2f,\"p\":%d,\"r\":%d,"
    "\"m\":%d,\"c\":%d,\"sta\":%d,"
    "\"up\":%lu,\"heap\":%lu,\"minh\":%lu,\"age\":%lu,\"lb\":%d,"
    "\"fsc\":%lu,\"fsa\":%lu,"
    "\"ra\":%d,\"rb\":%d,\"rc\":%d,\"rd\":%d,"
    "\"lf\":%d,\"lr\":%d,\"es\":%d,"
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
    (unsigned long)(millis() - lastCmdMillis),
    lowBattCount,
    (unsigned long)safetyTimeoutCount,
    (unsigned long)(safetyTimeoutCount > 0 ? (millis() - lastSafetyTimeoutMillis) / 1000UL : 0),
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

  // Моторы управляются через PCA9685 на общей I2C-шине.\n\n  // 4. Инициализация рулевого сервопривода.
  steerServo.setPeriodHertz(50);
  steerServo.attach(SERVO_PIN, 1000, 2000);

  // 5. Инициализация OLED-дисплея.
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  oledAvailable = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  motorBoardReady = i2cMotorBackendBegin(Wire);

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
  ElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);

  // 10. Приводим моторы и руль в начальное состояние.
  stopAll();

  // Первичное обновление данных мониторинга.
  updateStatus();

  // Запускаем отсчёт тайм-аутов после завершения инициализации.
  lastCmdMillis = millis();
  lastDisplayStatusMillis = millis();
  startDisplayStartupAnimation();
}

// Записывает одно срабатывание защиты на текущем цикле потери команд.
void recordSafetyTimeout() {
  if (!safetyTimeoutLatched) {
    safetyTimeoutCount++;
    lastSafetyTimeoutMillis = millis();
    safetyTimeoutLatched = true;
  }
}

// ---------- Основной цикл ----------

void loop() {

  // Аварийная остановка при отсутствии команд управления.
  if (millis() - lastCmdMillis > CMD_TIMEOUT_MS) {
    if (motorAVal != 0 ||
        motorBVal != 0 ||
        motorCVal != 0 ||
        motorDVal != 0 ||
        servoVal != 0 ||
        engineSimOn) {
      recordSafetyTimeout();
      stopAll();
    }
  } else {
    // Новая валидная активность снова разрешает учитывать следующий timeout.
    safetyTimeoutLatched = false;
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
      char buf[256];
      buildStatus(buf, sizeof(buf));
      ws.textAll(buf);
    }
  }

  // Обслуживание OTA и очистка отключившихся клиентов.
  ElegantOTA.loop();
  ws.cleanupClients();
}