/*
 * LEGO Technic ESP32
 * Версия разработки: 0.5.27 GUI Update 3.0
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
 * CHANGELOG_VERSION: 0.5.27
 * CHANGELOG_START
 * - PWM мотора D перенесён на GPIO2; обновлены распиновка и техническая схема.
 * - Настройки Wi-Fi (имя сети и пароль) и учётные данные ElegantOTA вынесены в отдельный блок в начале app.cpp с поясняющими комментариями.
 * - Настроена единая логика включения асинхронного режима ElegantOTA при компиляции основной и стартовой прошивок OTA Bootstrap.
 * - Исправлена сборка OTA Bootstrap: добавлен необходимый параметр компиляции для работы ElegantOTA с AsyncWebServer.
 * - Уточнена конфигурация GitHub Actions для сборки основной прошивки и OTA Bootstrap.
 * - Удалены отдельные workflows Project Auditor и Check motor channels; статический аудит проекта сохранён в Pull Request Validation.
 * - Обновлена схема автоматизации CI/CD в соответствии с текущей структурой GitHub Actions.
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
const unsigned long CMD_TIMEOUT_MS = 500; // safety: стоп, если нет команд

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

// Логическое состояние света хранится только в RAM.
// После сброса обе фары по умолчанию выключены.
bool ledFrontOn = false;
bool ledRearOn  = false;

// Режим имитации двигателя
bool engineSimOn = false;

// ---------- Калибровка рулевого сервопривода ----------

int steerCenterUs    = 1500;
int steerMaxAngleDeg = 45;

// ---------- Свет ----------
// GPIO5 и GPIO15 не настраиваются при загрузке: это strapping-пины.
// Выводы впервые становятся OUTPUT только после команды включения по WebSocket.
namespace {
bool ledFrontOutputInitialized = false;
bool ledRearOutputInitialized = false;

void applyLedOutput(uint8_t pin, bool requestedOn, bool &outputInitialized) {
  if (!requestedOn) {
    if (outputInitialized) {
#if LED_ACTIVE_LOW
      digitalWrite(pin, HIGH);
#else
      digitalWrite(pin, LOW);
#endif
    }
    return;
  }

  if (!outputInitialized) {
    // Сначала заранее выставляем неактивный уровень, чтобы не создать импульс
    // при переключении GPIO в OUTPUT. Код выполняется только по команде ON.
#if LED_ACTIVE_LOW
    digitalWrite(pin, HIGH);
#else
    digitalWrite(pin, LOW);
#endif
    pinMode(pin, OUTPUT);
    outputInitialized = true;
  }

#if LED_ACTIVE_LOW
  digitalWrite(pin, LOW);
#else
  digitalWrite(pin, HIGH);
#endif
}
} // namespace

void applyLeds() {
  applyLedOutput(LED_FRONT_PIN, ledFrontOn, ledFrontOutputInitialized);
  applyLedOutput(LED_REAR_PIN, ledRearOn, ledRearOutputInitialized);
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

  // GPIO фар намеренно не настраиваем при загрузке.
  // Они будут настроены только после WebSocket-команды включения света.

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

  // Запускаем отсчёт тайм-аута управления.
  lastCmdMillis = millis();
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
  static unsigned long lastStatus = 0;

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

  // Периодическое обновление мониторинга и OLED.
  if (millis() - lastStatus > 1000) {
    lastStatus = millis();

    updateStatus();
    updateDisplay();

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