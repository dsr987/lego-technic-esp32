# Полный снимок исходного кода LEGO Technic ESP32

> **Снимок:** прошивка 0.14.6  
> **Ветка-источник:** `main`  
> **Коммит-источник:** `08c408b759fe3e32fde08f5017b8928ac6f0d816`  
> **Назначение:** единый документ для чтения и реверс-инжиниринга.  
> **Целостность:** исходники ниже воспроизведены целиком из файлов репозитория без объединения логики, сокращений или исправлений. Заголовки сохраняют исходные пути.  

## Состав и границы снимка

В документ включены все исходные файлы проекта из `lego_technic_esp32/` с расширениями `.ino`, `.cpp` и `.h`. Это единый текстовый снимок, а не новый монолитный исходник для компиляции: порядок файлов и границы модулей сохранены для навигации. Внешние библиотеки Arduino/ESP32 в репозиторий не входят и здесь не дублируются. Файлы с бинарными/закодированными графическими ресурсами не являются исходным кодом модулей и не включены.


---

## `lego_technic_esp32/lego_technic_esp32.ino`

```cpp
/*
 * LEGO Technic ESP32
 * Main Arduino sketch: filename and containing directory must match.
 * Shared module implementations remain in their dedicated .cpp/.h files.
 * Версия разработки: 0.14.6
 *
 * Текущий выпуск: калибровка измерения напряжения батареи и управление PF-сервоприводом.
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
 * - Adafruit Motor Shield V2 Library 1.1.4
 *
 * История предыдущего выпуска:
 * 0.9.0 — OLED Update 2.0 ([PR #10](https://github.com/dsr987/lego-technic-esp32/pull/10))
 * - Добавлены растровые пиктограммы режимов и заставка проекта на OLED.
 * - Добавлена короткая неблокирующая анимация запуска «Initialising…».
 * - Рабочая панель OLED показывает режим, процент заряда и напряжение батареи, количество подключённых клиентов Wi-Fi и предупреждение о низком заряде.
 * - Если инициализация OLED не удалась, прошивка продолжает запуск без попыток выводить данные на недоступный дисплей.
 *
 * CHANGELOG_VERSION: 0.14.6
 * CHANGELOG_START
 * - PF Servo заранее снижает скорость при подходе к крайнему левому положению, чтобы уменьшить удар о механический ограничитель. ([PR #154](https://github.com/dsr987/lego-technic-esp32/pull/154))
 * CHANGELOG_END
 *
 * тестовый запуск сборки
 */

#include <Arduino.h>
#include <esp_system.h>
#include <stdarg.h>
#include <stdio.h>
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
#include "serial_log.h"

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
volatile bool oledDisplayEnabled = true; // Всегда включён после запуска; состояние не сохраняется.

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
unsigned long lastWebStatusMillis = 0;

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
uint8_t motorModeC = 2;
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
int pfServoSpeedPercent = 50;

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
  PositionalServoStatus positionalStatus;
  positionalServoGetStatus(positionalStatus);

  const int trimOffsetUs = steerCenterUs - 1500;
  const int steerTrimDegrees = trimOffsetUs >= 0
      ? (trimOffsetUs * 180 + 500) / 1000
      : (trimOffsetUs * 180 - 500) / 1000;

  snprintf(
    buf,
    n,
    "{\"st\":1,\"v\":%.2f,\"p\":%d,\"r\":%d,"
    "\"m\":%d,\"c\":%d,\"sta\":%d,"
    "\"up\":%lu,\"heap\":%lu,\"minh\":%lu,\"lb\":%d,\"oled\":%d,"
    "\"ra\":%d,\"rb\":%d,\"rc\":%d,\"rd\":%d,\"re\":%d,\"rf\":%d,"
    "\"lf\":%d,\"lr\":%d,\"es\":%d,"
    "\"cfgA\":%d,\"cfgB\":%d,\"cfgC\":%d,\"cfgD\":%d,\"cfgE\":%d,\"cfgF\":%d,"
    "\"tr\":%d,\"md\":%d,"
    "\"pfstate\":%d,\"pfmv\":%d,\"pftarget\":%d,"
    "\"pfhas\":%d,\"pfvalid\":%d,\"pfdone\":%d,\"pfs\":%d,\"pfzone\":%d}",
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
    oledDisplayEnabled ? 1 : 0,
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
    2,
    (int)motorModeD,
    (int)motorModeE,
    (int)motorModeF,
    steerTrimDegrees,
    steerMaxAngleDeg,
    (int)positionalStatus.state,
    positionalStatus.feedbackMv,
    (int)positionalStatus.target,
    positionalStatus.targetValid ? 1 : 0,
    positionalStatus.feedbackValid ? 1 : 0,
    positionalStatus.targetReached ? 1 : 0,
    positionalServoGetSpeedPercent(),
    (int)positionalStatus.feedbackZone
  );
}

// ---------- Диагностический журнал для Web GUI ----------
namespace {
constexpr uint8_t SERIAL_LOG_CAPACITY = 64;
constexpr size_t SERIAL_LOG_LINE_LENGTH = 128;
struct SerialLogEntry { uint32_t id; char text[SERIAL_LOG_LINE_LENGTH]; };
SerialLogEntry serialLogEntries[SERIAL_LOG_CAPACITY];
uint8_t serialLogWriteIndex = 0, serialLogCount = 0;
uint32_t serialLogNextId = 1;
portMUX_TYPE serialLogMux = portMUX_INITIALIZER_UNLOCKED;
void appendSerialLog(const char *message) {
  if (!message) return;
  portENTER_CRITICAL(&serialLogMux);
  SerialLogEntry &entry = serialLogEntries[serialLogWriteIndex];
  entry.id = serialLogNextId++;
  snprintf(entry.text, sizeof(entry.text), "%s", message);
  serialLogWriteIndex = static_cast<uint8_t>((serialLogWriteIndex + 1) % SERIAL_LOG_CAPACITY);
  if (serialLogCount < SERIAL_LOG_CAPACITY) ++serialLogCount;
  portEXIT_CRITICAL(&serialLogMux);
}
void appendJsonString(String &json, const char *value) {
  json += '"';
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) {
    switch (*p) {
      case '"': json += "\\\""; break;
      case '\\': json += "\\\\"; break;
      case '\n': json += "\\n"; break;
      case '\r': json += "\\r"; break;
      case '\t': json += "\\t"; break;
      default: if (*p >= 0x20) json += static_cast<char>(*p); break;
    }
  }
  json += '"';
}
}
void serialLogBegin(unsigned long baudRate) { Serial.begin(baudRate); }
void serialLogPrintln(const char *message) {
  Serial.println(message ? message : "");
  appendSerialLog(message ? message : "");
}
void serialLogPrintf(const char *format, ...) {
  if (!format) return;
  char line[SERIAL_LOG_LINE_LENGTH];
  va_list args; va_start(args, format); vsnprintf(line, sizeof(line), format, args); va_end(args);
  Serial.print(line);
  appendSerialLog(line);
}
String serialLogReadJson(uint32_t afterId) {
  uint8_t count;
  uint32_t latestId;
  portENTER_CRITICAL(&serialLogMux);
  count = serialLogCount;
  latestId = serialLogNextId - 1;
  portEXIT_CRITICAL(&serialLogMux);

  String json;
  json.reserve(256 + static_cast<size_t>(count) * 220);
  json = "{\"latest\":";
  json += String(latestId);
  json += ",\"lines\":[";

  // Копируем по одной записи: большой снимок кольцевого буфера не занимает стек
  // задачи AsyncWebServer. Если запись успела перезаписаться во время чтения,
  // пропускаем её по проверке id.
  const uint32_t firstId = latestId >= count ? latestId - count + 1 : 1;
  bool first = true;
  for (uint32_t id = firstId; id <= latestId; ++id) {
    if (id <= afterId) continue;

    SerialLogEntry entry;
    const uint8_t index = static_cast<uint8_t>((id - 1) % SERIAL_LOG_CAPACITY);
    portENTER_CRITICAL(&serialLogMux);
    entry = serialLogEntries[index];
    portEXIT_CRITICAL(&serialLogMux);
    if (entry.id != id) continue;

    if (!first) json += ',';
    first = false;
    json += "{\"id\":";
    json += String(entry.id);
    json += ",\"text\":";
    appendJsonString(json, entry.text);
    json += '}';
  }

  json += "]}";
  return json;
}

// ---------- Инициализация ----------

static const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN: return "UNKNOWN";
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "OTHER";
  }
}

void setup() {
  // Временная диагностика FeatherWing доступна в Serial Monitor на 115200 baud.
  serialLogBegin(115200);
  delay(200);
  serialLogPrintln("");
  const esp_reset_reason_t resetReason = esp_reset_reason();
  serialLogPrintf("[FW] reset_reason=%d (%s)\n",
      static_cast<int>(resetReason), resetReasonName(resetReason));
  serialLogPrintln("[FW] LEGO Technic ESP32 boot; FeatherWing diagnostics enabled");

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
  // Частота 1600 Гц применяется только к отдельному TB6612FNG.
  ledcSetup(LEDC_CH_C, TB6612FNG_PWM_FREQUENCY_HZ, 8);
  ledcAttachPin(TB_PWMA, LEDC_CH_C);
  ledcSetup(LEDC_CH_D, TB6612FNG_PWM_FREQUENCY_HZ, 8);
  ledcAttachPin(TB_PWMB, LEDC_CH_D);

  // 4. Инициализация рулевого сервопривода.
  steerServo.setPeriodHertz(50);
  // Keep the full 500..2500 us command range for +/-90-degree center trim and steering.
  steerServo.attach(SERVO_PIN, 500, 2500);

  // 5. Запуск общей I2C-шины для OLED и PCA9685 FeatherWing.
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setTimeOut(50);

  // Инициализируем PCA9685 до OLED, чтобы создать общую блокировку Wire.
  // Отсутствующий FeatherWing не должен останавливать запуск Wi-Fi/OLED.
  const bool featherWingReady = i2cMotorBackendBegin(Wire);
  serialLogPrintf("[FW][PCA9685] setup result: %s\n", featherWingReady ? "READY" : "NOT READY");

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
  positionalServoSetSpeedPercent(pfServoSpeedPercent);

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

  // Диагностический буфер доступен через Wi-Fi, без USB-UART.
  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *req) {
    uint32_t after = 0;
    if (req->hasParam("after")) after = static_cast<uint32_t>(req->getParam("after")->value().toInt());
    req->send(200, "application/json; charset=utf-8", serialLogReadJson(after));
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
  stopAll("startup");

  // Первичное обновление данных мониторинга.
  updateStatus();

  // Запускаем отсчёт тайм-аутов после завершения инициализации.
  lastCmdMillis = millis();
  lastDisplayStatusMillis = millis();
  startDisplayStartupAnimation();
}

// ---------- Основной цикл ----------

void loop() {

  // I²C-команда включения OLED обрабатывается только из loop(), не из AsyncTCP.
  processDisplayControl();

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
      stopAll("watchdog timeout");
    }
  }

  // Анимация запуска обновляется без блокировки WebSocket, OTA и watchdog.
  const bool startupAnimating = updateDisplayStartup();

  // Телеметрия веб-интерфейса обновляется независимо от ADC и цикла OLED.
  // positionalServoGetStatus() читает только последнее измерение, не запускает ADC.
  if ((unsigned long)(millis() - lastWebStatusMillis) >= 100) {
    lastWebStatusMillis = millis();
    if (ws.count() > 0) {
      char buf[384];
      buildStatus(buf, sizeof(buf));
      ws.textAll(buf);
    }
  }

  // Периодическое обновление мониторинга и OLED.
  if (millis() - lastDisplayStatusMillis > 1000) {
    lastDisplayStatusMillis = millis();

    updateStatus();
    if (!startupAnimating) {
      updateDisplay();
    }

  }

  // Обслуживание OTA и очистка отключившихся клиентов.
  ElegantOTA.loop();
  ws.cleanupClients();
}
```

---

## `lego_technic_esp32/hardware_config.h`

```h
#pragma once

// ---------- TB6612FNG — моторы C и D, Hardware 2.0 ----------
#define TB_STBY   25

#define TB_AIN1   18
#define TB_AIN2   19
#define TB_PWMA   23

#define TB_BIN1   17
#define TB_BIN2   16
#define TB_PWMB   4

// ---------- Сервоприводы и аналоговые входы ----------
#define SERVO_PIN           27
#define BATT_PIN            34
#define SERVO_FEEDBACK_PIN  35

// Делитель батареи для текущей сборки: Rверх=100 кОм, Rниз=2×20 кОм последовательно (40 кОм).
// Исторический номинал Rниз=39 кОм заменён доступной парой 20 кОм; см. BATT_DIVIDER_FACTOR.
// Внешняя подтяжка обратной связи позиционного сервопривода: GPIO35 -> GND, 10 кОм.

// ---------- I2C: OLED + PCA9685 FeatherWing ----------
#define I2C_SDA   21
#define I2C_SCL   22

// Частота PWM отдельного TB6612FNG (каналы C/D).
constexpr uint16_t TB6612FNG_PWM_FREQUENCY_HZ = 1600;

// ---------- Каналы LEDC для TB6612FNG C/D ----------
#define LEDC_CH_C 6
#define LEDC_CH_D 7

```

---

## `lego_technic_esp32/battery_monitor.cpp`

```cpp

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "motor_control.h"
#include "battery_monitor.h"

// GPIO34 — вход измерения напряжения батареи (определён в hardware_config.h).
#include "hardware_config.h"

// Коэффициент делителя напряжения: отношение V_ADC / V_battery.
// Для Rверх=100 кОм и Rниз=2×20 кОм (40 кОм): 40 / (100 + 40) = 0.285714...
// Это физический коэффициент схемы, а не полевая калибровочная поправка.
#define BATT_DIVIDER_FACTOR 0.2857

// >>> КАЛИБРОВКА НАПРЯЖЕНИЯ БАТАРЕИ <<<
// Это единственный коэффициент, который нужно менять после сравнения с мультиметром.
// 1.0733 = полевая поправка по измерениям: 7.50 В на экране / 8.05 В мультиметром.
// Расчёт: 8.05 / 7.50 = 1.073333...; используется округление до 4 знаков.
// Для расчёта используй tools/calibrate_battery.py и текущие показания GUI/OLED.
// Не изменяй BATT_DIVIDER_FACTOR вместо этой поправки, если делитель физически не менялся.
static const float BATT_CALIBRATION_FACTOR = 1.0733f;

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

// Состояние приложения объявлено в lego_technic_esp32.ino
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
  return (vAdc / BATT_DIVIDER_FACTOR) * BATT_CALIBRATION_FACTOR;
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

```

---

## `lego_technic_esp32/battery_monitor.h`

```h

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

```

---

## `lego_technic_esp32/display_assets.h`

```h
#pragma once

// OLED Update 2.0 1-bit graphics for Adafruit_GFX::drawBitmap().
// Bitmaps are MSB-first and stored in flash memory via PROGMEM.
#include <Arduino.h>
#include <pgmspace.h>

constexpr uint8_t OLED_BOOT_WIDTH = 76;
constexpr uint8_t OLED_BOOT_HEIGHT = 60;
constexpr uint8_t OLED_ICON_TEST_WIDTH = 41;
constexpr uint8_t OLED_ICON_TEST_HEIGHT = 41;
constexpr uint8_t OLED_ICON_CAR_WIDTH = 41;
constexpr uint8_t OLED_ICON_CAR_HEIGHT = 20;
constexpr uint8_t OLED_ICON_TANK_WIDTH = 41;
constexpr uint8_t OLED_ICON_TANK_HEIGHT = 28;
constexpr uint8_t OLED_ICON_LOW_BATTERY_WIDTH = 41;
constexpr uint8_t OLED_ICON_LOW_BATTERY_HEIGHT = 22;

static const uint8_t OLED_BOOT_LOGO[] PROGMEM = {
  0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x1F, 0xC0, 0x00, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00, 0x3F, 0xE0, 0x00, 0x00,
  0x00, 0x03, 0x83, 0xFF, 0xFF, 0xFF, 0xF0, 0x70, 0x00, 0x00, 0x00, 0x06, 0x31, 0xFF, 0xFF, 0xFF, 0xE1, 0x18, 0x00, 0x00,
  0x00, 0x0C, 0xFC, 0xFF, 0xFF, 0xFF, 0xCF, 0xCC, 0x00, 0x00, 0x00, 0x0D, 0xFE, 0x40, 0x00, 0x00, 0x9F, 0xEC, 0x00, 0x00,
  0x00, 0x09, 0xFE, 0x40, 0x00, 0x00, 0x9F, 0xE4, 0x00, 0x00, 0x00, 0x09, 0xFE, 0x40, 0x00, 0x00, 0x9F, 0xE4, 0x00, 0x00,
  0x00, 0x09, 0xFE, 0x40, 0x00, 0x00, 0x9F, 0xE4, 0x00, 0x00, 0x00, 0x09, 0xFE, 0x62, 0x4C, 0xD8, 0x9F, 0xE4, 0x00, 0x00,
  0x00, 0x1C, 0xFC, 0xC2, 0x4C, 0xD8, 0xCF, 0xCC, 0x00, 0x00, 0x00, 0x0E, 0x79, 0x82, 0x4C, 0xD8, 0x67, 0x9C, 0x00, 0x00,
  0x00, 0x07, 0x03, 0x82, 0x4C, 0xD8, 0x30, 0x3C, 0x00, 0x00, 0x00, 0x07, 0xFE, 0x1F, 0xFF, 0xFF, 0x1F, 0xFC, 0x00, 0x00,
  0x00, 0x07, 0xFC, 0x3F, 0xFF, 0xFF, 0x8F, 0xFC, 0x00, 0x00, 0x00, 0x07, 0x00, 0x60, 0x00, 0x01, 0xC0, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x60, 0x00, 0x00, 0xC0, 0x3C, 0x00, 0x00, 0x00, 0x07, 0x00, 0x60, 0x00, 0x00, 0xC0, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x0F, 0xE0, 0x00, 0x00, 0xFC, 0x3C, 0x00, 0x00, 0x00, 0x07, 0x0F, 0xE0, 0x00, 0x00, 0xFC, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x60, 0x7F, 0xC0, 0xC0, 0x3C, 0x00, 0x00, 0x00, 0x07, 0x00, 0x61, 0xFF, 0xE0, 0xC0, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x00, 0xE3, 0xE1, 0xF8, 0xC0, 0x3C, 0x00, 0x00, 0x00, 0x07, 0x0F, 0xE7, 0x80, 0x78, 0xFC, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x07, 0xE6, 0x3F, 0x98, 0xFC, 0x3C, 0x00, 0x00, 0x00, 0x07, 0x00, 0x60, 0xFF, 0xC0, 0xC0, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x61, 0xC0, 0xE0, 0xC0, 0x3C, 0x00, 0x00, 0x00, 0x07, 0x0F, 0xE0, 0xBF, 0x00, 0xFC, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x0F, 0xE0, 0x73, 0x80, 0xFC, 0x3C, 0x00, 0x00, 0x00, 0x07, 0x00, 0x60, 0x20, 0x00, 0xC0, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x60, 0x0E, 0x00, 0xC0, 0x3C, 0x00, 0x00, 0x00, 0x07, 0x00, 0xE0, 0x1E, 0x00, 0xC0, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x0F, 0xE0, 0x0E, 0x00, 0xFC, 0x3C, 0x00, 0x00, 0x00, 0x07, 0x07, 0xE0, 0x00, 0x00, 0xFC, 0x3C, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x60, 0x00, 0x00, 0xC0, 0x3C, 0x00, 0x00, 0x00, 0x07, 0xF8, 0x60, 0x00, 0x00, 0xC7, 0xFC, 0x00, 0x00,
  0x00, 0x07, 0xFE, 0x60, 0x00, 0x01, 0xDF, 0xFC, 0x00, 0x00, 0x00, 0x07, 0x87, 0x7F, 0xFF, 0xFF, 0xB8, 0x7C, 0x00, 0x00,
  0x00, 0x0E, 0x01, 0xBF, 0xFF, 0xFF, 0x60, 0x1C, 0x00, 0x00, 0x00, 0x0C, 0xFC, 0xC2, 0x4C, 0xD8, 0xCF, 0x8C, 0x00, 0x00,
  0x00, 0x0D, 0xFE, 0xC2, 0x4C, 0xD8, 0xDF, 0xCC, 0x00, 0x00, 0x00, 0x09, 0xFE, 0x42, 0x4C, 0xD8, 0x9F, 0xE4, 0x00, 0x00,
  0x00, 0x09, 0xFE, 0x42, 0x44, 0x80, 0x9F, 0xE4, 0x00, 0x00, 0x00, 0x09, 0xFE, 0x40, 0x00, 0x00, 0x9F, 0xE4, 0x00, 0x00,
  0x00, 0x09, 0xFE, 0x60, 0x00, 0x00, 0x9F, 0xE4, 0x00, 0x00, 0x00, 0x0C, 0xFC, 0xC0, 0x00, 0x00, 0xCF, 0xCC, 0x00, 0x00,
  0x00, 0x06, 0x79, 0xC0, 0x00, 0x00, 0xE7, 0x98, 0x00, 0x00, 0x00, 0x03, 0x03, 0xFF, 0xFF, 0xFF, 0xF0, 0x30, 0x00, 0x00,
  0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x1F, 0xC0, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x8F, 0x77, 0x03, 0xBB, 0xA5, 0x4D, 0xC0, 0xEE, 0xF7, 0x70, 0x8E, 0x44, 0x81, 0x32, 0x3D, 0xC9, 0x00, 0xCC, 0xB3, 0x30,
  0x8C, 0x55, 0x81, 0x22, 0x35, 0x49, 0x00, 0x82, 0xE3, 0x40, 0xEF, 0x77, 0x01, 0x3B, 0xA5, 0x4C, 0xC0, 0xEE, 0x8E, 0xF0
};

static const uint8_t OLED_ICON_TEST[] PROGMEM = {
  0x00, 0x00, 0x00, 0x1F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x1F, 0xC0, 0x00, 0xF8, 0x00, 0x00, 0x7F, 0x80, 0x00, 0xF8, 0x00,
  0x00, 0x7F, 0x80, 0x00, 0xFE, 0x00, 0x01, 0xFE, 0x00, 0x00, 0xFE, 0x00, 0x01, 0xFE, 0x03, 0x80, 0xFF, 0x80, 0x03, 0xF8,
  0x03, 0x80, 0x3F, 0x80, 0x03, 0xF8, 0x07, 0x80, 0x3F, 0x80, 0x03, 0xF8, 0x07, 0x80, 0x0F, 0xE0, 0x03, 0xF8, 0x1F, 0x80,
  0x0F, 0xE0, 0x03, 0xFE, 0x1F, 0x80, 0x00, 0xF8, 0x03, 0xFF, 0xFF, 0x80, 0x00, 0xF8, 0x03, 0xFF, 0xFF, 0x80, 0x00, 0x3C,
  0x03, 0xFF, 0xFE, 0x00, 0x00, 0x1C, 0x0F, 0xFF, 0xFE, 0x00, 0x00, 0x0F, 0x0F, 0xFF, 0xF8, 0x00, 0x00, 0x03, 0x3F, 0xFF,
  0xE0, 0x00, 0x00, 0x02, 0x3F, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0, 0x00, 0x00,
  0x00, 0x03, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x03, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x3F,
  0xFC, 0x78, 0x00, 0x00, 0x00, 0x3F, 0xFC, 0x78, 0x00, 0x00, 0x00, 0xFF, 0xF0, 0xFE, 0x00, 0x00, 0x00, 0xFF, 0xF0, 0xFE,
  0x00, 0x00, 0x03, 0xFF, 0xC3, 0xFF, 0x80, 0x00, 0x03, 0xFF, 0xC3, 0xFF, 0x80, 0x00, 0x0F, 0xFF, 0x00, 0xFF, 0xE0, 0x00,
  0x0F, 0xFF, 0x00, 0xFF, 0xE0, 0x00, 0x3F, 0xFE, 0x00, 0x7F, 0xF8, 0x00, 0x3F, 0xFC, 0x00, 0x7F, 0xF8, 0x00, 0xF1, 0xF8,
  0x00, 0x1F, 0xFC, 0x00, 0xF1, 0xE0, 0x00, 0x07, 0xFF, 0x00, 0xF1, 0xE0, 0x00, 0x07, 0xFF, 0x00, 0xFF, 0xE0, 0x00, 0x01,
  0xFF, 0x00, 0xFF, 0xE0, 0x00, 0x01, 0xFF, 0x00, 0x3F, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x3E, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x3E, 0x00
};

static const uint8_t OLED_ICON_CAR[] PROGMEM = {
  0x00, 0x00, 0x1F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x70, 0x38, 0x0F, 0x80, 0x00, 0x00,
  0xE0, 0x38, 0x1F, 0x80, 0x00, 0x01, 0x80, 0x38, 0x1F, 0x00, 0x00, 0x03, 0x00, 0x7F, 0xC4, 0x00, 0x07, 0xFE, 0x01, 0xFF,
  0xF8, 0x00, 0x1F, 0xFF, 0xFF, 0xF8, 0x3C, 0x00, 0x1F, 0xFF, 0xFF, 0xF0, 0x1C, 0x00, 0x78, 0x0F, 0xFF, 0xE0, 0x1F, 0x00,
  0xE3, 0xC7, 0xFF, 0x87, 0xC7, 0x00, 0xEF, 0xF1, 0xFF, 0x0F, 0xE3, 0x00, 0xCC, 0x31, 0xFF, 0x38, 0x73, 0x00, 0x18, 0x98,
  0x00, 0x31, 0x30, 0x00, 0x1B, 0xD9, 0xFF, 0x37, 0xB0, 0x00, 0x1B, 0xD9, 0xFF, 0x37, 0xB0, 0x00, 0x18, 0x98, 0x00, 0x31,
  0x30, 0x00, 0x0C, 0x30, 0x00, 0x08, 0x60, 0x00, 0x03, 0xE0, 0x00, 0x07, 0xC0, 0x00, 0x03, 0xE0, 0x00, 0x07, 0xC0, 0x00
};

static const uint8_t OLED_ICON_TANK[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xE7, 0x80, 0x00, 0x00,
  0x00, 0x3F, 0xE3, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xE3, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFB, 0x00, 0x00, 0x00,
  0x00, 0xC2, 0x1B, 0x00, 0x00, 0x00, 0x00, 0xC2, 0x1B, 0x00, 0x00, 0x00, 0x00, 0xC2, 0x1B, 0xFF, 0xF0, 0x00,
  0x00, 0xC2, 0x1F, 0xFF, 0xFC, 0x00, 0x00, 0xC2, 0x1F, 0xFF, 0xFC, 0x00, 0x00, 0xC2, 0x1F, 0xFB, 0x6C, 0x00,
  0x00, 0xFD, 0xFF, 0xFB, 0x6C, 0x00, 0x00, 0xFD, 0xFF, 0xFB, 0x6C, 0x00, 0x01, 0xFF, 0xFF, 0xFB, 0x6F, 0x00,
  0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xFE, 0x00,
  0x1F, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x73, 0xC7, 0x8F, 0x1E, 0x3F, 0x80,
  0x72, 0x44, 0x89, 0x12, 0x27, 0x80, 0x72, 0x44, 0x89, 0x12, 0x27, 0x80, 0x73, 0xC7, 0x8F, 0x1E, 0x3F, 0x80,
  0x1F, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x06, 0x66, 0x66, 0x66, 0x66, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t OLED_ICON_LOW_BATTERY[] PROGMEM = {
  0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x8F,
  0x3F, 0xFF, 0xFC, 0x00, 0x3F, 0xE3, 0x3F, 0xFF, 0xFC, 0x00, 0xE0, 0x03, 0xC0, 0x00, 0x0E, 0x00, 0xE0, 0x01, 0xE0, 0x00,
  0x06, 0x00, 0xEF, 0x01, 0xF0, 0x00, 0x07, 0x80, 0xEF, 0x00, 0x70, 0x00, 0x07, 0x80, 0xEF, 0x00, 0x3C, 0x00, 0x07, 0x80,
  0xEF, 0x00, 0x1C, 0x00, 0x07, 0x80, 0xEF, 0x00, 0x0E, 0x00, 0x07, 0x80, 0xCF, 0x00, 0x0F, 0x80, 0x07, 0x80, 0xCF, 0x00,
  0x03, 0x80, 0x07, 0x80, 0xCF, 0x00, 0x01, 0xC0, 0x07, 0x80, 0xCF, 0x00, 0x00, 0xF0, 0x07, 0x80, 0xC0, 0x00, 0x00, 0x70,
  0x06, 0x00, 0xE0, 0x00, 0x00, 0x38, 0x0E, 0x00, 0x3F, 0xFF, 0xFF, 0x9E, 0xFC, 0x00, 0x3F, 0xFF, 0xFF, 0x8E, 0x7C, 0x00,
  0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00
};


```

---

## `lego_technic_esp32/display_control.cpp`

```cpp
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESPAsyncWebServer.h>
#include <pgmspace.h>
#include <stdio.h>

#include "motor_control.h"
#include "battery_monitor.h"
#include "display_control.h"
#include "display_assets.h"
#include "i2c_motor_backend.h"

// Дисплей, WebSocket и признак успешной инициализации объявлены в lego_technic_esp32.ino.
extern Adafruit_SSD1306 display;
extern AsyncWebSocket ws;
extern bool oledAvailable;
extern volatile bool oledDisplayEnabled;

// Режим работы объявлен в lego_technic_esp32.ino.
extern Mode currentMode;

// Состояние загрузочной анимации.
static bool startupAnimationActive = false;
static unsigned long startupAnimationStartedAt = 0;
static int lastStartupDotCount = 0;
static bool displaySnapshotValid = false;
static int lastDisplayedBatteryPercent = -1;
static int lastDisplayedVoltageTenths = -1;
static bool lastDisplayedLowBattery = false;
static portMUX_TYPE displayPowerMux = portMUX_INITIALIZER_UNLOCKED;
static volatile int8_t requestedDisplayPower = -1;

// OLED никогда не ждёт освобождения общей I²C: если шилд использует шину
// или ждёт её, кадр остаётся в локальном буфере и будет отправлен позже.
static bool flushDisplay() {
  if (!oledDisplayEnabled || !i2cMotorBackendTryLockSharedI2c()) return false;
  display.display();
  i2cMotorBackendUnlockSharedI2c();
  return true;
}

bool setOledDisplayEnabled(bool enabled) {
  oledDisplayEnabled = enabled;
  if (!oledAvailable) return true;

  // WebSocket callback only queues the request. All OLED I²C and buffer
  // operations are performed later from loop(), never from AsyncTCP context.
  portENTER_CRITICAL(&displayPowerMux);
  requestedDisplayPower = enabled ? 1 : 0;
  portEXIT_CRITICAL(&displayPowerMux);
  return true;
}

void processDisplayControl() {
  if (!oledAvailable) return;

  portENTER_CRITICAL(&displayPowerMux);
  const int8_t initialRequest = requestedDisplayPower;
  portEXIT_CRITICAL(&displayPowerMux);
  if (initialRequest < 0 || !i2cMotorBackendTryLockSharedI2c()) return;

  portENTER_CRITICAL(&displayPowerMux);
  const int8_t request = requestedDisplayPower;
  requestedDisplayPower = -1;
  portEXIT_CRITICAL(&displayPowerMux);

  if (request >= 0) {
    display.ssd1306_command(request == 1 ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
    if (request == 1) display.display();
    displaySnapshotValid = false;
  }
  i2cMotorBackendUnlockSharedI2c();
}

// ---------- Компактный пиксельный шрифт 4x5 ----------
// Нужен только для строки клиентов: 5 пикселей на символ позволяют
// сохранить подпись и Wi-Fi-значок в узкой правой колонке.
static const uint8_t compactFont[][5] PROGMEM = {
  {0b0110, 0b1000, 0b1000, 0b1000, 0b0110}, // c
  {0b1000, 0b1000, 0b1000, 0b1000, 0b1110}, // l
  {0b0100, 0b0000, 0b0100, 0b0100, 0b0100}, // i
  {0b0110, 0b1001, 0b1110, 0b1000, 0b0111}, // e
  {0b1100, 0b1010, 0b1010, 0b1010, 0b1010}, // n
  {0b1110, 0b0100, 0b0100, 0b0100, 0b0010}, // t
  {0b0111, 0b1000, 0b0110, 0b0001, 0b1110}, // s
  {0b0000, 0b0100, 0b0000, 0b0100, 0b0000}, // :
  {0b0000, 0b0000, 0b0000, 0b0000, 0b0000}, // space
  {0b0110, 0b1001, 0b1001, 0b1001, 0b0110}, // 0
  {0b0100, 0b1100, 0b0100, 0b0100, 0b1110}, // 1
  {0b1110, 0b0001, 0b0110, 0b1000, 0b1111}, // 2
  {0b1110, 0b0001, 0b0110, 0b0001, 0b1110}, // 3
  {0b1010, 0b1010, 0b1111, 0b0010, 0b0010}, // 4
  {0b1111, 0b1000, 0b1110, 0b0001, 0b1110}, // 5
  {0b0110, 0b1000, 0b1110, 0b1001, 0b0110}, // 6
  {0b1111, 0b0001, 0b0010, 0b0100, 0b0100}, // 7
  {0b0110, 0b1001, 0b0110, 0b1001, 0b0110}, // 8
  {0b0110, 0b1001, 0b0111, 0b0001, 0b0110}  // 9
};

static int compactGlyphIndex(char c) {
  switch (c) {
    case 'c': return 0;
    case 'l': return 1;
    case 'i': return 2;
    case 'e': return 3;
    case 'n': return 4;
    case 't': return 5;
    case 's': return 6;
    case ':': return 7;
    case ' ': return 8;
    case '0': return 9;
    case '1': return 10;
    case '2': return 11;
    case '3': return 12;
    case '4': return 13;
    case '5': return 14;
    case '6': return 15;
    case '7': return 16;
    case '8': return 17;
    case '9': return 18;
    default: return 8;
  }
}

static void drawCompactText(int x, int y, const char *text) {
  while (*text) {
    const int glyphIndex = compactGlyphIndex(*text++);
    for (int row = 0; row < 5; row++) {
      const uint8_t bits = pgm_read_byte(&compactFont[glyphIndex][row]);
      for (int col = 0; col < 4; col++) {
        if (bits & (1U << (3 - col))) {
          display.drawPixel(x + col, y + row, SSD1306_WHITE);
        }
      }
    }
    x += 5;
  }
}

// ---------- Маленькие статусные пиктограммы ----------

static void drawBatteryIndicator(int x, int y, int percent) {
  percent = constrain(percent, 0, 100);

  // Корпус и контакт батареи.
  display.drawRect(x, y, 22, 13, SSD1306_WHITE);
  display.fillRect(x + 22, y + 4, 3, 5, SSD1306_WHITE);

  // Заполнение занимает до 16 пикселей внутренней ширины.
  const int fillWidth = (16 * percent + 50) / 100;
  if (fillWidth > 0) {
    display.fillRect(x + 3, y + 3, fillWidth, 7, SSD1306_WHITE);
  }
}

static void drawLightningIcon(int x, int y) {
  display.fillTriangle(x + 6, y, x + 1, y + 7, x + 6, y + 7, SSD1306_WHITE);
  display.fillTriangle(x + 5, y + 5, x + 2, y + 13, x + 10, y + 5, SSD1306_WHITE);
}

static void drawWifiIcon(int x, int y) {
  // Геометрическая пиктограмма Wi-Fi, рисуется прямо в буфер OLED.
  display.drawLine(x, y + 2, x + 2, y, SSD1306_WHITE);
  display.drawLine(x + 2, y, x + 5, y, SSD1306_WHITE);
  display.drawLine(x + 5, y, x + 7, y + 2, SSD1306_WHITE);

  display.drawLine(x + 2, y + 4, x + 3, y + 3, SSD1306_WHITE);
  display.drawLine(x + 3, y + 3, x + 4, y + 3, SSD1306_WHITE);
  display.drawLine(x + 4, y + 3, x + 5, y + 3, SSD1306_WHITE);
  display.drawLine(x + 5, y + 3, x + 6, y + 4, SSD1306_WHITE);

  display.drawPixel(x + 4, y + 6, SSD1306_WHITE);
  display.drawPixel(x + 4, y + 7, SSD1306_WHITE);
}

// ---------- Заставка и анимация запуска ----------

void showBootLogo() {
  if (!oledAvailable) return;

  displaySnapshotValid = false;
  display.clearDisplay();
  display.drawBitmap(
    (128 - OLED_BOOT_WIDTH) / 2,
    (64 - OLED_BOOT_HEIGHT) / 2,
    OLED_BOOT_LOGO,
    OLED_BOOT_WIDTH,
    OLED_BOOT_HEIGHT,
    SSD1306_WHITE
  );
  flushDisplay();
}

void showInitialisingScreen() {
  if (!oledAvailable) return;

  displaySnapshotValid = false;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setCursor(19, 28);
  display.print("Initialising");
  flushDisplay();
}

void startDisplayStartupAnimation() {
  if (!oledAvailable) return;

  startupAnimationStartedAt = millis();
  lastStartupDotCount = 0;
  displaySnapshotValid = false;
  startupAnimationActive = true;
}

bool updateDisplayStartup() {
  if (!oledAvailable || !startupAnimationActive) return false;

  const unsigned long elapsed = millis() - startupAnimationStartedAt;
  if (elapsed >= 1000) {
    startupAnimationActive = false;
    return false;
  }

  // Одна, две, три точки, затем цикл повторяется.
  const int dotCount = static_cast<int>((elapsed / 250) % 3) + 1;
  if (dotCount != lastStartupDotCount) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextWrap(false);
    display.setCursor(19, 28);
    display.print("Initialising");
    for (int i = 0; i < dotCount; i++) {
      display.print('.');
    }
    if (flushDisplay()) {
      lastStartupDotCount = dotCount;
    }
  }

  return true;
}

// ---------- Иконка текущего режима ----------

static void drawModeIcon() {
  const uint8_t *icon = OLED_ICON_TEST;
  uint8_t width = OLED_ICON_TEST_WIDTH;
  uint8_t height = OLED_ICON_TEST_HEIGHT;

  if (currentMode == MODE_TANK) {
    icon = OLED_ICON_TANK;
    width = OLED_ICON_TANK_WIDTH;
    height = OLED_ICON_TANK_HEIGHT;
  } else if (currentMode == MODE_CAR) {
    icon = OLED_ICON_CAR;
    width = OLED_ICON_CAR_WIDTH;
    height = OLED_ICON_CAR_HEIGHT;
  }

  // Левая половина экрана отдана активному режиму.
  display.drawBitmap(
    (64 - width) / 2,
    (64 - height) / 2,
    icon,
    width,
    height,
    SSD1306_WHITE
  );
}

// ---------- Экран критически низкого заряда ----------

static void drawLowBatteryScreen() {
  display.drawBitmap(
    (128 - OLED_ICON_LOW_BATTERY_WIDTH) / 2,
    4,
    OLED_ICON_LOW_BATTERY,
    OLED_ICON_LOW_BATTERY_WIDTH,
    OLED_ICON_LOW_BATTERY_HEIGHT,
    SSD1306_WHITE
  );

  display.setTextSize(1);
  display.setCursor((128 - 12 * 6) / 2, 38);
  display.print("low battery,");
  display.setCursor((128 - 13 * 6) / 2, 49);
  display.print("please charge");
}

// ---------- Рабочая панель OLED Update 2.0 ----------

void updateDisplay() {
  if (!oledAvailable || !oledDisplayEnabled) return;

  // OLED показывает напряжение с точностью до 0,1 В. Передавать кадр
  // повторно нет смысла, пока не изменился заряд, отображаемое напряжение
  // или состояние критического предупреждения.
  const int voltageTenths = static_cast<int>(battV * 10.0f + 0.5f);
  const bool lowBattery = lowBattCount >= 3;
  if (displaySnapshotValid &&
      lastDisplayedBatteryPercent == battPct &&
      lastDisplayedVoltageTenths == voltageTenths &&
      lastDisplayedLowBattery == lowBattery) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  // При подтверждённом низком заряде заменяем панель состояния предупреждением.
  if (lowBattery) {
    drawLowBatteryScreen();
    if (flushDisplay()) {
      lastDisplayedBatteryPercent = battPct;
      lastDisplayedVoltageTenths = voltageTenths;
      lastDisplayedLowBattery = lowBattery;
      displaySnapshotValid = true;
    }
    return;
  }

  drawModeIcon();

  // Вертикально отделяем иконку режима от телеметрии.
  display.drawLine(64, 2, 64, 61, SSD1306_WHITE);

  // Верхняя зона: графический уровень заряда и проценты.
  drawBatteryIndicator(68, 8, battPct);
  display.setTextSize(1);
  display.setCursor(96, 10);
  display.printf("%d%%", constrain(battPct, 0, 100));

  display.drawLine(66, 31, 126, 31, SSD1306_WHITE);

  // Средняя зона: текущее напряжение.
  drawLightningIcon(69, 35);
  display.setCursor(82, 38);
  display.printf("%.1fV", battV);

  display.drawLine(66, 48, 126, 48, SSD1306_WHITE);

  // Нижняя зона: Wi-Fi-пиктограмма и число WebSocket-клиентов.
  drawWifiIcon(67, 52);
  char clientText[16];
  snprintf(clientText, sizeof(clientText), "clients: %u", static_cast<unsigned int>(ws.count()));
  drawCompactText(78, 53, clientText);

  if (flushDisplay()) {
    lastDisplayedBatteryPercent = battPct;
    lastDisplayedVoltageTenths = voltageTenths;
    lastDisplayedLowBattery = lowBattery;
    displaySnapshotValid = true;
  }
}

```

---

## `lego_technic_esp32/display_control.h`

```h
#pragma once

// Начальная заставка OLED.
void showBootLogo();

// Текст на время настройки Wi-Fi и остальных компонентов.
void showInitialisingScreen();

// Запустить и обновлять короткую анимацию Initialising...
void startDisplayStartupAnimation();
bool updateDisplayStartup();

// Временно включить или выключить OLED без сохранения состояния в NVS.
// Команда из WebSocket только ставится в очередь; I²C выполняется из loop().
bool setOledDisplayEnabled(bool enabled);
void processDisplayControl();

// Обновление рабочего экрана OLED.
void updateDisplay();

```

---

## `lego_technic_esp32/i2c_motor_backend.cpp`

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "i2c_motor_backend.h"
#include "serial_log.h"

namespace {

constexpr uint8_t I2C_MOTOR_WING_ADDRESS = 0x60;
constexpr uint16_t MOTOR_PWM_FREQUENCY_HZ = 1600;
constexpr uint8_t MOTOR_COUNT = 4;
constexpr unsigned long MOTOR_COMMAND_REFRESH_MS = 500;

// Официальная библиотека Adafruit управляет PCA9685 и TB6612FNG целиком.
// Порядок объектов соответствует логическим каналам A/B/E/F и физическим M1–M4.
Adafruit_MotorShield motorShield(I2C_MOTOR_WING_ADDRESS);
Adafruit_DCMotor *motors[MOTOR_COUNT] = {nullptr, nullptr, nullptr, nullptr};

int8_t lastDirection[MOTOR_COUNT] = {0, 0, 0, 0};
int16_t lastReportedSpeed[MOTOR_COUNT] = {127, 127, 127, 127};
unsigned long lastMotorWriteMs[MOTOR_COUNT] = {0, 0, 0, 0};
bool backendReady = false;
bool rejectedCommandLogged = false;
SemaphoreHandle_t backendMutex = nullptr;

// Счётчик ожидающих команд защищён отдельной короткой critical section.
// Пока моторная команда ждёт I²C, OLED не начинает новую передачу.
portMUX_TYPE motorPriorityMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t waitingMotorCommands = 0;

void registerMotorWaiter() {
  portENTER_CRITICAL(&motorPriorityMux);
  ++waitingMotorCommands;
  portEXIT_CRITICAL(&motorPriorityMux);
}

void unregisterMotorWaiter() {
  portENTER_CRITICAL(&motorPriorityMux);
  if (waitingMotorCommands > 0) --waitingMotorCommands;
  portEXIT_CRITICAL(&motorPriorityMux);
}

bool hasWaitingMotorCommands() {
  portENTER_CRITICAL(&motorPriorityMux);
  const bool waiting = waitingMotorCommands > 0;
  portEXIT_CRITICAL(&motorPriorityMux);
  return waiting;
}

const char *motorName(uint8_t index) {
  static const char *NAMES[MOTOR_COUNT] = {"A/M1", "B/M2", "E/M3", "F/M4"};
  return index < MOTOR_COUNT ? NAMES[index] : "UNKNOWN";
}

class BackendLock {
 public:
  explicit BackendLock(
      TickType_t waitTicks = pdMS_TO_TICKS(250),
      bool motorPriority = false
  ) : acquired_(false) {
    if (motorPriority) registerMotorWaiter();
    acquired_ = backendMutex != nullptr &&
        xSemaphoreTake(backendMutex, waitTicks) == pdTRUE;
    if (motorPriority) unregisterMotorWaiter();
  }

  ~BackendLock() {
    if (acquired_) {
      xSemaphoreGive(backendMutex);
    }
  }

  bool acquired() const {
    return acquired_;
  }

 private:
  bool acquired_;
};

bool initializeMotorShield(TwoWire &wire) {
  serialLogPrintf(
      "[FW][MOTOR SHIELD] Initializing Adafruit Motor Shield V2 at 0x%02X, PWM=%u Hz\n",
      static_cast<unsigned>(I2C_MOTOR_WING_ADDRESS),
      static_cast<unsigned>(MOTOR_PWM_FREQUENCY_HZ));

  if (!motorShield.begin(MOTOR_PWM_FREQUENCY_HZ, &wire)) {
    serialLogPrintln("[FW][MOTOR SHIELD][ERROR] Adafruit library initialization failed");
    return false;
  }

  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    motors[i] = motorShield.getMotor(static_cast<uint8_t>(i + 1));
    if (motors[i] == nullptr) {
      serialLogPrintf("[FW][MOTOR SHIELD][ERROR] Failed to get motor object M%u\n",
                      static_cast<unsigned>(i + 1));
      return false;
    }

    motors[i]->run(RELEASE);
    lastDirection[i] = 0;
    lastReportedSpeed[i] = 127;
    lastMotorWriteMs[i] = 0;
  }

  serialLogPrintln("[FW][MOTOR SHIELD][OK] Adafruit backend READY; A/B/E/F mapped to M1/M2/M3/M4");
  return true;
}

} // namespace

bool i2cMotorBackendBegin(TwoWire &wire) {
  // Инициализация выполняется до запуска WebSocket-управления.
  serialLogPrintln("[FW][MOTOR SHIELD] Starting Adafruit backend initialization");
  if (backendMutex == nullptr) {
    backendMutex = xSemaphoreCreateMutex();
  }
  if (backendMutex == nullptr) {
    serialLogPrintln("[FW][MOTOR SHIELD][ERROR] Failed to create I2C mutex");
    backendReady = false;
    return false;
  }

  BackendLock lock(pdMS_TO_TICKS(1000));
  if (!lock.acquired()) {
    serialLogPrintln("[FW][MOTOR SHIELD][ERROR] Failed to acquire I2C mutex during initialization");
    backendReady = false;
    return false;
  }

  backendReady = false;
  rejectedCommandLogged = false;
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    motors[i] = nullptr;
    lastDirection[i] = 0;
    lastReportedSpeed[i] = 127;
  }

  if (!initializeMotorShield(wire)) {
    backendReady = false;
    serialLogPrintln("[FW][MOTOR SHIELD][ERROR] Backend NOT READY; motor commands will be rejected until reboot");
    return false;
  }

  backendReady = true;
  return true;
}

bool i2cMotorBackendIsReady() {
  BackendLock lock;
  return lock.acquired() && backendReady;
}

bool i2cMotorBackendLockSharedI2c(uint32_t timeoutMs) {
  return backendMutex != nullptr &&
         xSemaphoreTake(backendMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

bool i2cMotorBackendTryLockSharedI2c() {
  if (backendMutex == nullptr || hasWaitingMotorCommands()) return false;
  if (xSemaphoreTake(backendMutex, 0) != pdTRUE) return false;

  // Recheck after taking the mutex to close the race with a newly queued motor command.
  if (hasWaitingMotorCommands()) {
    xSemaphoreGive(backendMutex);
    return false;
  }
  return true;
}

void i2cMotorBackendUnlockSharedI2c() {
  if (backendMutex != nullptr) {
    xSemaphoreGive(backendMutex);
  }
}

bool i2cMotorBackendSetMotor(I2CMotorChannel channel, int speedPercent) {
  // Motor operations have priority over OLED traffic on the shared Wire bus.
  BackendLock lock(pdMS_TO_TICKS(250), true);
  if (!lock.acquired()) {
    return false;
  }

  const uint8_t index = static_cast<uint8_t>(channel);
  if (!backendReady || index >= MOTOR_COUNT || motors[index] == nullptr) {
    if (!rejectedCommandLogged) {
      serialLogPrintf("[FW][MOTOR SHIELD][WARN] Motor command rejected: channel=%u backendReady=%s\n",
                      static_cast<unsigned>(index), backendReady ? "true" : "false");
      rejectedCommandLogged = true;
    }
    return false;
  }

  speedPercent = constrain(speedPercent, -100, 100);
  const unsigned long now = millis();
  Adafruit_DCMotor *motor = motors[index];

  // Heartbeat repeats channel values every 150 ms. Do not rewrite identical PWM
  // and direction on every heartbeat; reassert unchanged outputs every 500 ms
  // so a transient I²C write failure can recover without continuous bus traffic.
  if (speedPercent == lastReportedSpeed[index] &&
      (unsigned long)(now - lastMotorWriteMs[index]) < MOTOR_COMMAND_REFRESH_MS) {
    return true;
  }

  if (speedPercent == 0) {
    motor->run(RELEASE);
    lastDirection[index] = 0;
    lastMotorWriteMs[index] = now;
    if (lastReportedSpeed[index] != 0) {
      serialLogPrintf("[FW][MOTOR SHIELD][CMD] %s speed=0%% (release)\n", motorName(index));
    }
    lastReportedSpeed[index] = 0;
    return true;
  }

  const int8_t direction = speedPercent > 0 ? 1 : -1;
  if (lastDirection[index] != 0 && lastDirection[index] != direction) {
    motor->run(RELEASE);
  }

  const int magnitude = speedPercent > 0 ? speedPercent : -speedPercent;
  const uint8_t pwm = static_cast<uint8_t>((magnitude * 255 + 50) / 100);
  motor->setSpeed(pwm);
  motor->run(direction > 0 ? FORWARD : BACKWARD);

  lastDirection[index] = direction;
  lastMotorWriteMs[index] = now;
  if (lastReportedSpeed[index] != speedPercent) {
    serialLogPrintf("[FW][MOTOR SHIELD][CMD] %s speed=%d%% direction=%s pwm=%u\n",
                    motorName(index), speedPercent, direction > 0 ? "forward" : "reverse",
                    static_cast<unsigned>(pwm));
    lastReportedSpeed[index] = speedPercent;
  }
  return true;
}

void i2cMotorBackendStopAll() {
  BackendLock lock(pdMS_TO_TICKS(250), true);
  if (!lock.acquired() || !backendReady) {
    return;
  }

  for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
    if (motors[i] == nullptr) {
      backendReady = false;
      serialLogPrintf("[FW][MOTOR SHIELD][ERROR] Motor object M%u is unavailable during STOP ALL\n",
                      static_cast<unsigned>(i + 1));
      return;
    }

    motors[i]->run(RELEASE);
    lastDirection[i] = 0;
    lastReportedSpeed[i] = 0;
    lastMotorWriteMs[i] = millis();
  }
  serialLogPrintln("[FW][MOTOR SHIELD] STOP ALL completed");
}

```

---

## `lego_technic_esp32/i2c_motor_backend.h`

```h
#pragma once

#include <Arduino.h>
#include <Wire.h>

// Логические каналы FeatherWing не зависят от физических выходов M1–M4.
// Hardware pre2.0 использует FeatherWing для A, B, E и F.
enum class I2CMotorChannel : uint8_t {
  A = 0, // M1
  B = 1, // M2
  E = 2, // M3
  F = 3  // M4
};

// Вызывать после Wire.begin(...). Функция использует существующую I2C-шину
// и не запускает вторую шину самостоятельно.
bool i2cMotorBackendBegin(TwoWire &wire = Wire);

// Это состояние инициализации ПО, а не гарантия постоянной доступности I2C.
bool i2cMotorBackendIsReady();

// Общая блокировка Wire для дисплея и моторного backend.
// Обычный lock нужен только для инициализации; OLED в рабочем цикле использует try-lock,
// чтобы сразу уступить шину ожидающей команде моторного шилда.
bool i2cMotorBackendLockSharedI2c(uint32_t timeoutMs = 250);
bool i2cMotorBackendTryLockSharedI2c();
void i2cMotorBackendUnlockSharedI2c();

// Задаёт подписанную скорость в диапазоне -100..100.
// Возвращает false, если backend не инициализирован или канал некорректен.
// Arduino-библиотека не отдаёт статус каждой I2C-записи, поэтому true не
// является подтверждением, что последняя транзакция была физически доставлена.
bool i2cMotorBackendSetMotor(I2CMotorChannel channel, int speedPercent);

// Переводит четыре канала FeatherWing (A, B, E, F) в RELEASE и скорость 0.
void i2cMotorBackendStopAll();

```

---

## `lego_technic_esp32/motor_control.cpp`

```cpp

#include <Arduino.h>

#include "hardware_config.h"
#include "motor_control.h"
#include "steering_control.h"
#include "positional_servo.h"
#include "i2c_motor_backend.h"
#include "serial_log.h"

// Состояние объявлено и определяется в lego_technic_esp32.ino
extern uint8_t motorModeC;
extern Mode currentMode;

extern int motorAVal;
extern int motorBVal;
extern int motorCVal;
extern int motorDVal;
extern int motorEVal;
extern int motorFVal;
extern int servoVal;

extern bool reverseA;
extern bool reverseB;
extern bool reverseC;
extern bool reverseD;
extern bool reverseE;
extern bool reverseF;
extern bool engineSimOn;


// ---------- Базовое управление мостом TB6612FNG ----------

static void setDCBridge(int in1, int in2, int pwmChannel, int val) {
  val = constrain(val, -100, 100);
  // ESP32 LEDC can behave unexpectedly at the exact 100% duty endpoint (255 at 8-bit resolution).
  // Cap PWM at 254: this is effectively full power but avoids the timer overflow edge case.
  int duty = map(abs(val), 0, 100, 0, 254);

  if (val > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (val < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }

  ledcWrite(pwmChannel, duty);
}


// ---------- Мотор A ----------

void applyMotorA(int val) {
  motorAVal = constrain(val, -100, 100);

  i2cMotorBackendSetMotor(
    I2CMotorChannel::A,
    reverseA ? -motorAVal : motorAVal
  );

  if (engineSimOn && currentMode == MODE_CAR) {
    int aux = (int)(
      33.0 + 67.0 * abs(motorAVal) / 100.0 + 0.5
    );

    int actualB = reverseB ? -aux : aux;
    const int previousB = motorBVal;
    motorBVal = actualB;
    if (previousB != actualB) {
      serialLogPrintf("[ENG][OUTPUT] throttleA=%d motorB=%d reverseB=%d\n",
          motorAVal, actualB, reverseB ? 1 : 0);
    }

    i2cMotorBackendSetMotor(I2CMotorChannel::B, actualB);
  }
}


// ---------- Мотор B ----------

void applyMotorB(int val) {
  const int previousB = motorBVal;
  motorBVal = constrain(val, -100, 100);
  if (!engineSimOn && previousB != motorBVal) {
    serialLogPrintf("[MOTOR B][MANUAL] command=%d reverse=%d\n",
        motorBVal, reverseB ? 1 : 0);
  }

  i2cMotorBackendSetMotor(
    I2CMotorChannel::B,
    reverseB ? -motorBVal : motorBVal
  );
}


// ---------- Мотор C ----------

void applyMotorC(int val) {
  motorCVal = constrain(val, -100, 100);

  setDCBridge(
    TB_AIN1,
    TB_AIN2,
    LEDC_CH_C,
    (motorModeC == 2) ? motorCVal : (reverseC ? -motorCVal : motorCVal)
  );
}


// ---------- Мотор D ----------

void applyMotorD(int val) {
  motorDVal = constrain(val, -100, 100);

  setDCBridge(
    TB_BIN1,
    TB_BIN2,
    LEDC_CH_D,
    reverseD ? -motorDVal : motorDVal
  );
}


// ---------- Моторы E/F на FeatherWing ----------

void applyMotorE(int val) {
  motorEVal = constrain(val, -100, 100);
  i2cMotorBackendSetMotor(
    I2CMotorChannel::E,
    reverseE ? -motorEVal : motorEVal
  );
}

void applyMotorF(int val) {
  motorFVal = constrain(val, -100, 100);
  i2cMotorBackendSetMotor(
    I2CMotorChannel::F,
    reverseF ? -motorFVal : motorFVal
  );
}


// ---------- Общее включение драйверов ----------

void updateDriverStandby() {
  // STBY относится только к прямому TB6612 каналов C/D.
  // FeatherWing A/B/E/F управляется независимо через PCA9685.
  const bool directDriverActive = motorCVal != 0 || motorDVal != 0;
  digitalWrite(TB_STBY, directDriverActive ? HIGH : LOW);
}


// ---------- Аварийная остановка ----------

void stopAll(const char *reason) {
  serialLogPrintf("[FW][STOP] reason=%s\n", reason ? reason : "unspecified");
  // Отменяем автоматическое позиционирование C перед общей остановкой.
  positionalServoCancel();

  // Отключаем имитацию двигателя.
  engineSimOn = false;

  motorAVal = 0;
  motorBVal = 0;
  motorCVal = 0;
  motorDVal = 0;
  motorEVal = 0;
  motorFVal = 0;

  // Останавливаем FeatherWing-каналы A/B/E/F и прямые C/D.
  i2cMotorBackendStopAll();
  setDCBridge(TB_AIN1, TB_AIN2, LEDC_CH_C, 0);
  setDCBridge(TB_BIN1, TB_BIN2, LEDC_CH_D, 0);

  applySteer(0);
  updateDriverStandby();
}

```

---

## `lego_technic_esp32/motor_control.h`

```h

#pragma once

// Режимы работы модели
enum Mode {
  MODE_TANK = 0,
  MODE_CAR = 1
};

// Управление отдельными каналами моторов
void applyMotorA(int val);
void applyMotorB(int val);
void applyMotorC(int val);
void applyMotorD(int val);
void applyMotorE(int val);
void applyMotorF(int val);

// Общее управление драйверами
void updateDriverStandby();
void stopAll(const char *reason);

```

---

## `lego_technic_esp32/positional_servo.cpp`

```cpp
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "hardware_config.h"
#include "motor_control.h"
#include "positional_servo.h"
#include "serial_log.h"

// Значения напряжения получены измерением существующего механизма.
// Это стартовая калибровка; перед эксплуатацией требуется сверка по ADC ESP32.
static constexpr int FEEDBACK_LEFT_MV   = 1635;
static constexpr int FEEDBACK_CENTER_MV = 816;
static constexpr int FEEDBACK_RIGHT_MV  = 545;

static constexpr int FEEDBACK_TOLERANCE_MV = 45;
static constexpr int FEEDBACK_VALID_MIN_MV = 250;
static constexpr int FEEDBACK_VALID_MAX_MV = 2200;
static constexpr int FEEDBACK_ZONE_MIN_MV = 500;
static constexpr int FEEDBACK_ZONE_MAX_MV = 1850;
static constexpr int FEEDBACK_ZONE_HYSTERESIS_MV = 25;
static constexpr int FEEDBACK_ZONE_BOUNDARIES_MV[] = {635, 780, 990, 1375};

static constexpr int POSITION_MOTOR_SPEED = 70; // 100% пользовательской настройки
// Левый ход по журналам проходит последние ~500 мВ слишком быстро для
// штатного окна 150 мВ. Используем две ступени замедления только влево.
static constexpr int LEFT_APPROACH_WINDOW_MV = 600;
static constexpr int LEFT_APPROACH_SPEED_PERCENT = 75;
static constexpr int FINAL_APPROACH_WINDOW_MV = 150;
static constexpr int FINAL_APPROACH_SPEED_PERCENT = 50;
static constexpr unsigned long FEEDBACK_SAMPLE_INTERVAL_MS = 10;
static constexpr int POSITION_MOTOR_RAMP_STEP = 5;
static constexpr unsigned long POSITION_MOTOR_RAMP_INTERVAL_MS = 20;
static constexpr unsigned long UPDATE_INTERVAL_MS = 10;
static constexpr unsigned long POSITION_TIMEOUT_MS = 3000;
static constexpr unsigned long STALL_TIMEOUT_MS = 1200;
static constexpr unsigned long DIRECTION_DEADTIME_MS = 30;
static constexpr int MIN_PROGRESS_MV = 30;

static PositionalServoState state = PositionalServoState::Idle;
static bool targetValid = false;
static int targetVoltageMv = FEEDBACK_CENTER_MV;
static int movementDirection = 0;
static int filteredVoltageMv = 0;
static int rawFeedbackMv = 0;
static unsigned long lastDiagnosticLogMs = 0;
static int progressReferenceMv = 0;
static unsigned long lastUpdateMs = 0;
static unsigned long movementStartMs = 0;
static unsigned long lastProgressMs = 0;
static unsigned long directionChangeStartMs = 0;
static unsigned long lastMotorRampMs = 0;
static int appliedMotorSpeed = 0;
static uint8_t approachPhase = 0; // 0 — рабочая, 1 — раннее замедление, 2 — финальное
static bool filterReady = false;
static uint8_t invalidReadCount = 0;
static int feedbackZone = (int)PositionalServoFeedbackZone::Unknown;
static unsigned long lastFeedbackSampleMs = 0;
static int speedPercent = 50;

// WebSocket callbacks run in the AsyncTCP task while positionalServoUpdate()
// runs in loop(). A mutex protects controller state and all C-channel writes.
static SemaphoreHandle_t controllerMutex = nullptr;
static portMUX_TYPE cancelRequestMux = portMUX_INITIALIZER_UNLOCKED;
static bool cancelRequested = false;

// Never let a WebSocket callback or the main loop wait forever for controller state.
// WebSocket calls wait at most 10 ms; the control loop uses a zero-wait try-lock.
static bool lockController(uint32_t timeoutMs = 10) {
  return controllerMutex != nullptr &&
      xSemaphoreTake(controllerMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

static void unlockController() {
  if (controllerMutex != nullptr) {
    xSemaphoreGive(controllerMutex);
  }
}

static void requestControllerCancel() {
  portENTER_CRITICAL(&cancelRequestMux);
  cancelRequested = true;
  portEXIT_CRITICAL(&cancelRequestMux);
}

static bool consumeControllerCancel() {
  portENTER_CRITICAL(&cancelRequestMux);
  const bool requested = cancelRequested;
  cancelRequested = false;
  portEXIT_CRITICAL(&cancelRequestMux);
  return requested;
}

static void clearControllerCancel() {
  portENTER_CRITICAL(&cancelRequestMux);
  cancelRequested = false;
  portEXIT_CRITICAL(&cancelRequestMux);
}

static void updateFeedbackZone(int voltageMv) {
  if (voltageMv < FEEDBACK_ZONE_MIN_MV || voltageMv > FEEDBACK_ZONE_MAX_MV) {
    feedbackZone = (int)PositionalServoFeedbackZone::Unknown;
    return;
  }

  int candidateZone = 0;
  while (candidateZone < 4 &&
         voltageMv >= FEEDBACK_ZONE_BOUNDARIES_MV[candidateZone]) {
    ++candidateZone;
  }

  if (feedbackZone < 0 || feedbackZone > 4 ||
      feedbackZone == (int)PositionalServoFeedbackZone::Unknown) {
    feedbackZone = candidateZone;
    return;
  }

  while (feedbackZone < candidateZone && feedbackZone < 4 &&
         voltageMv >= FEEDBACK_ZONE_BOUNDARIES_MV[feedbackZone] +
                          FEEDBACK_ZONE_HYSTERESIS_MV) {
    ++feedbackZone;
  }
  while (feedbackZone > candidateZone && feedbackZone > 0 &&
         voltageMv < FEEDBACK_ZONE_BOUNDARIES_MV[feedbackZone - 1] -
                         FEEDBACK_ZONE_HYSTERESIS_MV) {
    --feedbackZone;
  }
}

static int readFeedbackMillivolts() {
  const int rawMv = (int)analogReadMilliVolts(SERVO_FEEDBACK_PIN);
  rawFeedbackMv = rawMv;
  if (!filterReady) {
    filteredVoltageMv = rawMv;
    filterReady = true;
  } else {
    // Небольшой IIR-фильтр подавляет шум ADC, не добавляя заметной задержки.
    filteredVoltageMv = (filteredVoltageMv * 3 + rawMv) / 4;
  }
  updateFeedbackZone(filteredVoltageMv);
  return filteredVoltageMv;
}

static bool feedbackIsValid(int voltageMv) {
  // Обрыв при внешней подтяжке вниз даёт уровень около 0 В;
  // короткое замыкание на питание даёт уровень выше рабочего диапазона.
  return voltageMv >= FEEDBACK_VALID_MIN_MV &&
         voltageMv <= FEEDBACK_VALID_MAX_MV;
}

static void stopPositionMotor() {
  const bool wasMoving = state == PositionalServoState::Moving ||
      state == PositionalServoState::WaitingForDirectionChange;
  if (wasMoving) {
    serialLogPrintf("[PF][STOP] target=%dmV feedback=%dmV raw=%dmV elapsed=%lums\n",
        targetVoltageMv, filteredVoltageMv, rawFeedbackMv,
        (unsigned long)(millis() - movementStartMs));
  }
  applyMotorC(0);
  updateDriverStandby();
  movementDirection = 0;
  appliedMotorSpeed = 0;
  state = PositionalServoState::Idle;
}

static void setPositionFault(const char *reason) {
  serialLogPrintf("[PF][FAULT] reason=%s state=%d feedback=%dmV raw=%dmV target=%dmV elapsed=%lums noProgress=%lums pwm=%d\n",
      reason ? reason : "unknown", (int)state, filteredVoltageMv, rawFeedbackMv,
      targetVoltageMv, (unsigned long)(millis() - movementStartMs),
      (unsigned long)(millis() - lastProgressMs), appliedMotorSpeed);
  applyMotorC(0);
  updateDriverStandby();
  movementDirection = 0;
  appliedMotorSpeed = 0;
  state = PositionalServoState::Fault;
}

static int targetVoltage(PositionalServoTarget target) {
  switch (target) {
    case PositionalServoTarget::Left:  return FEEDBACK_LEFT_MV;
    case PositionalServoTarget::Right: return FEEDBACK_RIGHT_MV;
    case PositionalServoTarget::Center:
    default:                           return FEEDBACK_CENTER_MV;
  }
}

static void startPositionMotor(unsigned long now) {
  progressReferenceMv = filteredVoltageMv;
  movementStartMs = now;
  lastProgressMs = now;
  lastUpdateMs = now;
  state = PositionalServoState::Moving;
  appliedMotorSpeed = 0;
  approachPhase = 0;
  lastMotorRampMs = now;

  // Запускаем привод с нулевой мощностью; дальнейший набор скорости выполняется в loop().
  applyMotorC(0);
  updateDriverStandby();
}

void positionalServoBegin() {
  if (controllerMutex == nullptr) {
    controllerMutex = xSemaphoreCreateMutex();
  }
  if (controllerMutex == nullptr) {
    state = PositionalServoState::Fault;
    return;
  }

  if (!lockController(100)) {
    state = PositionalServoState::Fault;
    serialLogPrintln("[PF][FAULT] reason=controller mutex unavailable during initialization");
    return;
  }
  pinMode(SERVO_FEEDBACK_PIN, INPUT);
  analogSetPinAttenuation(SERVO_FEEDBACK_PIN, ADC_11db);

  filterReady = false;
  invalidReadCount = 0;
  lastFeedbackSampleMs = 0;
  speedPercent = 50;
  state = PositionalServoState::Idle;
  movementDirection = 0;
  targetValid = false;

  // Первичное заполнение фильтра до приёма команд из WebSocket.
  for (uint8_t i = 0; i < 8; ++i) {
    readFeedbackMillivolts();
    delay(2);
  }
  unlockController();
}

bool positionalServoRequest(PositionalServoTarget target) {
  if (!lockController()) {
    serialLogPrintln("[PF][WARN] request rejected: controller busy");
    return false;
  }
  clearControllerCancel();

  const int currentMv = filteredVoltageMv;
  if (!feedbackIsValid(currentMv)) {
    setPositionFault("invalid feedback before request");
    unlockController();
    return false;
  }

  targetVoltageMv = targetVoltage(target);
  targetValid = true;
  serialLogPrintf("[PF][REQUEST] target=%d targetMv=%d feedback=%dmV raw=%dmV speed=%d%% state=%d\n",
      (int)target, targetVoltageMv, filteredVoltageMv, rawFeedbackMv,
      speedPercent, (int)state);
  invalidReadCount = 0;

  const int difference = targetVoltageMv - currentMv;
  if (abs(difference) <= FEEDBACK_TOLERANCE_MV) {
    stopPositionMotor();
    unlockController();
    return true;
  }

  const int requestedDirection = difference > 0 ? 1 : -1;
  const unsigned long now = millis();

  // Не меняем полярность H-моста на ходу. При реверсе сначала снимаем PWM
  // и отключаем TB6612 через STBY, затем выдерживаем паузу перед новым ходом.
  if ((state == PositionalServoState::Moving ||
       state == PositionalServoState::WaitingForDirectionChange) &&
      requestedDirection != movementDirection) {
    applyMotorC(0);
    updateDriverStandby();
    movementDirection = requestedDirection;
    directionChangeStartMs = now;
    state = PositionalServoState::WaitingForDirectionChange;
    unlockController();
    return true;
  }

  movementDirection = requestedDirection;
  if (state == PositionalServoState::Moving) {
    // Новая цель в том же направлении: продолжаем движение без остановки.
    unlockController();
    return true;
  }

  if (state == PositionalServoState::WaitingForDirectionChange) {
    // Цель изменилась, но направление осталось прежним: сохраняем deadtime.
    unlockController();
    return true;
  }

  startPositionMotor(now);
  lastDiagnosticLogMs = now;
  serialLogPrintf("[PF][START] direction=%s feedback=%dmV raw=%dmV target=%dmV\n",
      movementDirection > 0 ? "LEFT" : "RIGHT", filteredVoltageMv,
      rawFeedbackMv, targetVoltageMv);
  unlockController();
  return true;
}

void positionalServoUpdate() {
  if (!lockController(0)) return;

  if (consumeControllerCancel()) {
    stopPositionMotor();
    movementDirection = 0;
    appliedMotorSpeed = 0;
    state = PositionalServoState::Idle;
    unlockController();
    return;
  }

  const unsigned long now = millis();
  if ((unsigned long)(now - lastFeedbackSampleMs) >= FEEDBACK_SAMPLE_INTERVAL_MS) {
    lastFeedbackSampleMs = now;
    readFeedbackMillivolts();
  }

  if (state != PositionalServoState::Moving &&
      state != PositionalServoState::WaitingForDirectionChange) {
    unlockController();
    return;
  }
  if (state == PositionalServoState::Moving &&
      (unsigned long)(now - lastDiagnosticLogMs) >= 100) {
    lastDiagnosticLogMs = now;
    serialLogPrintf("[PF][MOVE] direction=%s feedback=%dmV raw=%dmV target=%dmV pwm=%d remaining=%dmV elapsed=%lums noProgress=%lums heap=%lu\n",
        movementDirection > 0 ? "LEFT" : "RIGHT", filteredVoltageMv,
        rawFeedbackMv, targetVoltageMv, appliedMotorSpeed,
        abs(targetVoltageMv - filteredVoltageMv),
        (unsigned long)(now - movementStartMs),
        (unsigned long)(now - lastProgressMs), (unsigned long)ESP.getFreeHeap());
  }

  if (state == PositionalServoState::WaitingForDirectionChange) {
    if ((unsigned long)(now - directionChangeStartMs) < DIRECTION_DEADTIME_MS) {
      unlockController();
      return;
    }

    const int currentMv = filteredVoltageMv;
    if (!feedbackIsValid(currentMv)) {
      if (++invalidReadCount >= 3) setPositionFault("invalid feedback while reversing");
      unlockController();
      return;
    }
    invalidReadCount = 0;

    const bool targetReached = movementDirection > 0
        ? currentMv >= targetVoltageMv - FEEDBACK_TOLERANCE_MV
        : currentMv <= targetVoltageMv + FEEDBACK_TOLERANCE_MV;
    if (targetReached) {
      stopPositionMotor();
    } else {
      startPositionMotor(now);
    }
    unlockController();
    return;
  }

  if ((unsigned long)(now - lastUpdateMs) < UPDATE_INTERVAL_MS) {
    unlockController();
    return;
  }
  lastUpdateMs = now;

  // Плавно увеличиваем PWM от 0 до рабочего значения за ~280 мс.
  if ((unsigned long)(now - lastMotorRampMs) >= POSITION_MOTOR_RAMP_INTERVAL_MS) {
    lastMotorRampMs = now;
    const int configuredSpeed = max(1, (POSITION_MOTOR_SPEED * speedPercent + 50) / 100);
    const int remainingMv = abs(targetVoltageMv - filteredVoltageMv);

    const bool movingToLeft = movementDirection > 0 &&
        targetVoltageMv == FEEDBACK_LEFT_MV;
    uint8_t requestedPhase = 0;
    if (movingToLeft && remainingMv <= FINAL_APPROACH_WINDOW_MV) {
      requestedPhase = 2;
    } else if (movingToLeft && remainingMv <= LEFT_APPROACH_WINDOW_MV) {
      requestedPhase = 1;
    } else if (remainingMv <= FINAL_APPROACH_WINDOW_MV) {
      requestedPhase = 1;
    }

    // Фазу замедления фиксируем до завершения текущего движения: шум ADC
    // не должен случайно вернуть привод на полную скорость.
    if (requestedPhase > approachPhase) {
      approachPhase = requestedPhase;
      const int phasePercent = movingToLeft && approachPhase == 1
          ? LEFT_APPROACH_SPEED_PERCENT
          : FINAL_APPROACH_SPEED_PERCENT;
      const int phaseSpeed = max(1, (configuredSpeed * phasePercent + 50) / 100);
      const int phaseTargetSpeed = movementDirection > 0 ? -phaseSpeed : phaseSpeed;
      serialLogPrintf("[PF][DECEL] direction=%s phase=%u remaining=%dmV pwm=%d targetPwm=%d\n",
          movementDirection > 0 ? "LEFT" : "RIGHT",
          (unsigned int)approachPhase, remainingMv, appliedMotorSpeed, phaseTargetSpeed);
    }

    const int phasePercent = approachPhase == 0 ? 100
        : (movingToLeft && approachPhase == 1
            ? LEFT_APPROACH_SPEED_PERCENT : FINAL_APPROACH_SPEED_PERCENT);
    const int phaseSpeed = max(1, (configuredSpeed * phasePercent + 50) / 100);
    const int targetSpeed = movementDirection > 0 ? -phaseSpeed : phaseSpeed;

    if (abs(targetSpeed) < abs(appliedMotorSpeed)) {
      // Замедление применяем сразу, а не растягиваем снижение PWM на 80 мс.
      appliedMotorSpeed = targetSpeed;
    } else if (appliedMotorSpeed < targetSpeed) {
      appliedMotorSpeed = min(appliedMotorSpeed + POSITION_MOTOR_RAMP_STEP, targetSpeed);
    } else if (appliedMotorSpeed > targetSpeed) {
      appliedMotorSpeed = max(appliedMotorSpeed - POSITION_MOTOR_RAMP_STEP, targetSpeed);
    }
    applyMotorC(appliedMotorSpeed);
    updateDriverStandby();
  }

  const int currentMv = filteredVoltageMv;
  if (!feedbackIsValid(currentMv)) {
    if (++invalidReadCount >= 3) setPositionFault("invalid feedback while moving");
    unlockController();
    return;
  }
  invalidReadCount = 0;

  // Проверяем достижение цели именно по направлению движения.
  // Промежуточный контакт не является целью и не может остановить привод.
  const bool targetReached = movementDirection > 0
      ? currentMv >= targetVoltageMv - FEEDBACK_TOLERANCE_MV
      : currentMv <= targetVoltageMv + FEEDBACK_TOLERANCE_MV;

  if (targetReached) {
    stopPositionMotor();
    unlockController();
    return;
  }

  if (abs(currentMv - progressReferenceMv) >= MIN_PROGRESS_MV) {
    progressReferenceMv = currentMv;
    lastProgressMs = now;
  }

  if ((unsigned long)(now - movementStartMs) >= POSITION_TIMEOUT_MS ||
      (unsigned long)(now - lastProgressMs) >= STALL_TIMEOUT_MS) {
    setPositionFault((unsigned long)(now - movementStartMs) >= POSITION_TIMEOUT_MS
        ? "movement timeout" : "no progress timeout");
  }

  unlockController();
}

void positionalServoCancel() {
  if (controllerMutex == nullptr) {
    state = PositionalServoState::Idle;
    return;
  }
  if (!lockController()) {
    requestControllerCancel();
    return;
  }
  clearControllerCancel();
  if (state == PositionalServoState::Moving ||
      state == PositionalServoState::WaitingForDirectionChange) {
    serialLogPrintf("[PF][CANCEL] feedback=%dmV raw=%dmV target=%dmV elapsed=%lums\n",
        filteredVoltageMv, rawFeedbackMv, targetVoltageMv,
        (unsigned long)(millis() - movementStartMs));
    applyMotorC(0);
    updateDriverStandby();
  }
  movementDirection = 0;
  state = PositionalServoState::Idle;
  unlockController();
}

bool positionalServoIsMoving() {
  if (!lockController(1)) return false;
  const bool moving = state == PositionalServoState::Moving ||
      state == PositionalServoState::WaitingForDirectionChange;
  unlockController();
  return moving;
}

void positionalServoSetSpeedPercent(int percent) {
  if (!lockController()) return;
  speedPercent = constrain(percent, 10, 100);

  // При уменьшении ограничения немедленно снижаем уже применённую скорость.
  // Это не позволяет старому PWM временно превышать новое значение настройки.
  const int maxSpeed = max(1, (POSITION_MOTOR_SPEED * speedPercent + 50) / 100);
  if (state == PositionalServoState::Moving && abs(appliedMotorSpeed) > maxSpeed) {
    appliedMotorSpeed = movementDirection > 0 ? -maxSpeed : maxSpeed;
    applyMotorC(appliedMotorSpeed);
    updateDriverStandby();
  }
  unlockController();
}

int positionalServoGetSpeedPercent() {
  if (!lockController(1)) return 50;
  const int value = speedPercent;
  unlockController();
  return value;
}

void positionalServoGetStatus(PositionalServoStatus &status) {
  status.state = PositionalServoState::Fault;
  status.target = PositionalServoTarget::Center;
  status.feedbackMv = 0;
  status.targetMv = FEEDBACK_CENTER_MV;
  status.targetValid = false;
  status.feedbackValid = false;
  status.targetReached = false;
  status.feedbackZone = PositionalServoFeedbackZone::Unknown;

  if (!lockController(1)) return;

  const int currentMv = filteredVoltageMv;
  status.state = state;
  status.target = targetValid
      ? (targetVoltageMv == FEEDBACK_LEFT_MV ? PositionalServoTarget::Left
        : targetVoltageMv == FEEDBACK_RIGHT_MV ? PositionalServoTarget::Right
        : PositionalServoTarget::Center)
      : PositionalServoTarget::Center;
  status.feedbackMv = currentMv;
  status.feedbackZone = (PositionalServoFeedbackZone)feedbackZone;
  status.targetMv = targetVoltageMv;
  status.targetValid = targetValid;
  status.feedbackValid = feedbackIsValid(currentMv);
  status.targetReached = state == PositionalServoState::Idle &&
      targetValid && status.feedbackValid &&
      abs(currentMv - targetVoltageMv) <= FEEDBACK_TOLERANCE_MV;
  unlockController();
}

```

---

## `lego_technic_esp32/positional_servo.h`

```h
#pragma once

#include <Arduino.h>

// Автоматическое позиционирование мотора C по аналоговой обратной связи.
enum class PositionalServoTarget : uint8_t {
  Left,
  Center,
  Right
};

enum class PositionalServoState : uint8_t {
  Idle,
  Moving,
  WaitingForDirectionChange,
  Fault
};

enum class PositionalServoFeedbackZone : uint8_t {
  Right = 0,
  RightToCenter = 1,
  Center = 2,
  CenterToLeft = 3,
  Left = 4,
  Unknown = 5
};

struct PositionalServoStatus {
  PositionalServoState state;
  PositionalServoTarget target;
  int feedbackMv;
  int targetMv;
  bool targetValid;
  bool feedbackValid;
  bool targetReached;
  PositionalServoFeedbackZone feedbackZone;
};

void positionalServoBegin();
bool positionalServoRequest(PositionalServoTarget target);
void positionalServoUpdate();
void positionalServoCancel();
bool positionalServoIsMoving();
void positionalServoSetSpeedPercent(int percent);
int positionalServoGetSpeedPercent();
void positionalServoGetStatus(PositionalServoStatus &status);

```

---

## `lego_technic_esp32/preferences_manager.cpp`

```cpp

#include <Arduino.h>
#include <Preferences.h>

#include "preferences_manager.h"

// Объект и состояние определяются в lego_technic_esp32.ino
extern Preferences prefs;

extern bool reverseA;
extern bool reverseB;
extern bool reverseC;
extern bool reverseD;
extern bool reverseE;
extern bool reverseF;

extern uint8_t motorModeA;
extern uint8_t motorModeB;
extern uint8_t motorModeC;
extern uint8_t motorModeD;
extern uint8_t motorModeE;
extern uint8_t motorModeF;

extern bool ledFrontOn;
extern bool ledRearOn;

extern int steerCenterUs;
extern int steerMaxAngleDeg;
extern int pfServoSpeedPercent;


// ---------- Загрузка настроек ----------

void loadPreferences() {
  reverseA = prefs.getUChar("revA", 0) != 0;
  reverseB = prefs.getUChar("revB", 0) != 0;
  reverseC = prefs.getUChar("revC", 0) != 0;
  reverseD = prefs.getUChar("revD", 0) != 0;
  reverseE = prefs.getUChar("revE", 0) != 0;
  reverseF = prefs.getUChar("revF", 0) != 0;

  motorModeA = prefs.getUChar("modeA", 0);
  if (motorModeA > 1) motorModeA = 0;
  motorModeB = prefs.getUChar("modeB", 0);
  if (motorModeB > 1) motorModeB = 0;
  // Канал C жёстко закреплён за позиционным PF-сервоприводом.
  // Старое значение modeC в NVS намеренно игнорируется.
  motorModeC = 2;
  motorModeD = prefs.getUChar("modeD", 0);
  if (motorModeD > 1) motorModeD = 0;
  motorModeE = prefs.getUChar("modeE", 0);
  if (motorModeE > 1) motorModeE = 0;
  motorModeF = prefs.getUChar("modeF", 0);
  if (motorModeF > 1) motorModeF = 0;

  ledFrontOn = prefs.getUChar("ledF", 0) != 0;
  ledRearOn  = prefs.getUChar("ledR", 0) != 0;

  // Center calibration is limited to +/-90 degrees around the default 1500 us.
  // Keep persisted values inside the full 500..2500 us command range.
  steerCenterUs = constrain(prefs.getInt("steer_c", 1500), 1000, 2000);
  steerMaxAngleDeg = constrain(prefs.getInt("steer_a", 45), 45, 90);
  pfServoSpeedPercent = constrain((int)prefs.getUChar("pfspd", 50), 10, 100);
}


// ---------- Сохранение калибровки руля ----------

void saveSteeringCenter(int value) {
  prefs.putInt("steer_c", value);
}

void saveSteeringMaxAngle(int value) {
  prefs.putInt("steer_a", value);
}

void savePfServoSpeedPercent(int value) {
  prefs.putUChar("pfspd", (uint8_t)constrain(value, 10, 100));
}


// ---------- Сохранение реверса моторов ----------

void saveReverseA(bool value) {
  prefs.putUChar("revA", value ? 1 : 0);
}

void saveReverseB(bool value) {
  prefs.putUChar("revB", value ? 1 : 0);
}

void saveReverseC(bool value) {
  prefs.putUChar("revC", value ? 1 : 0);
}

void saveReverseD(bool value) {
  prefs.putUChar("revD", value ? 1 : 0);
}

void saveReverseE(bool value) {
  prefs.putUChar("revE", value ? 1 : 0);
}

void saveReverseF(bool value) {
  prefs.putUChar("revF", value ? 1 : 0);
}


// ---------- Сохранение света ----------

void saveLedFront(bool value) {
  prefs.putUChar("ledF", value ? 1 : 0);
}

void saveLedRear(bool value) {
  prefs.putUChar("ledR", value ? 1 : 0);
}


// ---------- Сохранение режимов управления моторами ----------

void saveMotorModeA(uint8_t value) { prefs.putUChar("modeA", value); }
void saveMotorModeB(uint8_t value) { prefs.putUChar("modeB", value); }
void saveMotorModeD(uint8_t value) { prefs.putUChar("modeD", value); }
void saveMotorModeE(uint8_t value) { prefs.putUChar("modeE", value); }
void saveMotorModeF(uint8_t value) { prefs.putUChar("modeF", value); }

```

---

## `lego_technic_esp32/preferences_manager.h`

```h

#pragma once

#include <Arduino.h>

// Загрузка сохранённых настроек
void loadPreferences();

// Режимы управления моторами: 0 — слайдер, 1 — кнопки, 2 — позиционный (только C).
void saveMotorModeA(uint8_t value);
void saveMotorModeB(uint8_t value);
void saveMotorModeD(uint8_t value);
void saveMotorModeE(uint8_t value);
void saveMotorModeF(uint8_t value);

// Настройки рулевого управления
void saveSteeringCenter(int value);
void saveSteeringMaxAngle(int value);
void savePfServoSpeedPercent(int value);

// Реверс моторов
void saveReverseA(bool value);
void saveReverseB(bool value);
void saveReverseC(bool value);
void saveReverseD(bool value);
void saveReverseE(bool value);
void saveReverseF(bool value);

// Передний и задний свет
void saveLedFront(bool value);
void saveLedRear(bool value);

```
