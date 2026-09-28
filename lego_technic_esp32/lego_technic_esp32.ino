// ESP32 Lego Technic motorization — Версия: 0.0.6
// Библиотеки: ESPAsyncWebServer 3.1.0 (форк lacamera из Library Manager), AsyncTCP 1.1.4, ArduinoJson,
// Adafruit_SSD1306, Adafruit_GFX, Adafruit_BusIO, ESP32Servo, ElegantOTA (ayushsharma82)
// ESP32 core: 2.0.9 — зафиксирован сознательно (конфликт ledc API и веб-сервера на core 3.x).
// Сборка/заливка: arduino-cli + .bat-скрипт (компилирует под esp32:esp32@2.0.9, находит порт, шьёт).
// ElegantOTA: в файле <библиотеки>/ElegantOTA/src/ElegantOTA.h
// "#define ELEGANTOTA_USE_ASYNC_WEBSERVER 0" -> "...1" (правится один раз, вручную).

/*
 * ПОЛНАЯ СХЕМА ПОДКЛЮЧЕНИЯ (включая то, что не идёт на GPIO ESP32)
 *
 * Питание (силовые, толстые провода):
 *   2x18650 (+/-) -> HX-2S-JH20 BMS (P+/P-) -> выключатель на корпусе -> общая шина (+, -)
 *   Шина (+) -> Mini360 IN+ ; Шина (-) -> Mini360 IN- ; Mini360 OUT (выставить 5В) -> ESP32 5V/VIN
 *   Шина (+) -> TB6612 VM ; Шина (-) -> TB6612 GND (напрямую, минуя Mini360 — до 8.4В в норме для TB6612)
 *   TB6612 VCC (логика, 3.3-5В) -> ESP32 3V3 или 5V (см. даташит модуля TB6612 — обычно VCC=5V ок)
 *   Общий GND: батарея/BMS/выключатель/Mini360/TB6612/ESP32/OLED/серва — единая точка (важно для стабильной работы АЦП)
 *
 * Моторы (силовые, толстые провода, 2-pin разъёмы):
 *   TB6612 AO1/AO2 -> Lego мотор A (канал A)
 *   TB6612 BO1/BO2 -> Lego мотор B (канал B)
 *
 * Серва (сигнальный, тонкий провод, 3 жилы на 4-pin разъёме):
 *   Geekservo VCC -> 5В (с выхода Mini360) ; GND -> общий GND ; сигнал -> SERVO_PIN (см. ниже)
 *
 * OLED (сигнальный, 4-pin разъём):
 *   VCC -> 3.3В (ESP32 3V3) ; GND -> общий GND ; SDA -> OLED_SDA ; SCL -> OLED_SCL
 *
 * Делитель напряжения батареи (для BATT_PIN):
 *   Шина (+) -> R1(100к) -> точка замера -> R2(39к) -> GND ; точка замера -> BATT_PIN
 *
 * GPIO ESP32 (сигнальные, тонкие провода) — см. комментарии на каждой строке ниже.
 */
// Что исправлено и улучшено в 0.0.6:
// - OLED: предупреждение "LOW BATTERY" на весь экран при напряжении ниже 6.0В (2S, 3.0В/банка —
//   безопасный минимум для 18650). Если напряжение <0.5В — считаем, что батарея физически не
//   подключена (стенд на USB) и предупреждение не показываем, это не разряд, а отладка.
// - Предупреждение с антидребезгом: включается после 3 замеров подряд (~3 с) ниже порога и
//   гаснет только выше 6.3В — просадка под нагрузкой мотора не даёт ложных срабатываний.
// - OLED: справа появилась иконка текущего режима (танк/машинка), рисуется примитивами
//   Adafruit_GFX, без внешних битмапов.
// - Дубль блока со схемой подключения убран, в списке библиотек указаны реальные версии.
// (альфа-тест по 0.0.5 подтвердил: калибровка батареи точная, серво без дребезга после переноса
//  LEDC-каналов на 4/5, кнопки калибровки руля в GUI работают без нареканий)

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <ElegantOTA.h>

// ---------- Пины ----------
#define TB_STBY   4   // TB6612 STBY — разрешение работы драйвера (HIGH = включён, LOW = выключен/спит)
#define TB_AIN1   16  // TB6612 AIN1 — направление канала A (мотор 1: левый в танке / привод в классике)
#define TB_AIN2   17  // TB6612 AIN2 — направление канала A, второй вход (см. AIN1)
#define TB_PWMA   18  // TB6612 PWMA — скорость канала A (ШИМ, 0-255)
#define TB_BIN1   19  // TB6612 BIN1 — направление канала B (мотор 2: правый в танке / доп.мотор-лебёдка в классике)
#define TB_BIN2   21  // TB6612 BIN2 — направление канала B, второй вход (см. BIN1)
#define TB_PWMB   22  // TB6612 PWMB — скорость канала B (ШИМ, 0-255)
#define LEDC_CH_A 4   // номер LEDC-канала (не GPIO!) для ledcSetup/ledcAttachPin — привязан к TB_PWMA
#define LEDC_CH_B 5   // номер LEDC-канала (не GPIO!) для ledcSetup/ledcAttachPin — привязан к TB_PWMB
// Каналы 4/5, а не 0/1: библиотека ESP32Servo сама занимает LEDC-канал под капотом
// и по умолчанию ищет свободный начиная с 0 — если бы наши моторы сидели на 0/1,
// возможен конфликт (одна и та же аппаратная линия ШИМ пишется и мотором, и сервой) —
// вероятная причина дребезга сервы при газовании мотора.
#define SERVO_PIN 27  // Geekservo 360° сигнальный провод — руль в режиме "классика" (не используется в танке)
#define OLED_SDA  25  // OLED SSD1306 — линия данных I2C (программный I2C, не дефолтные 21/22 — те заняты TB6612)
#define OLED_SCL  26  // OLED SSD1306 — линия тактирования I2C
#define BATT_PIN  34  // Вход АЦП со среднего вывода делителя напряжения батареи (R1=100к сверху, R2=39к к GND).
                      // Вход-only пин (ADC1_CH6), выходом быть не может — здесь это не важно.

// Делитель батареи: R1(верх, к батарее)=100k, R2(низ, к GND)=39k
// factor = R2/(R1+R2) = 39/139
// Делитель R1=100k/R2=39k номинально даёт 39/139=0.2806, но реальные резисторы
// с разбросом ±5%. Откалибровано по факту: код показывал 7.67В, мультиметр — 8.03В.
// Если после замены резисторов/батареи снова разъедется — пересчитать так же:
// новый = старый * (показание_кода / показание_мультиметра)
#define BATT_DIVIDER_FACTOR 0.2680

// ---------- Режимы ----------
enum Mode { MODE_TANK = 0, MODE_CAR = 1 };
Mode currentMode = MODE_TANK;

Preferences prefs;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Servo steerServo;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

unsigned long lastCmdMillis = 0;
const unsigned long CMD_TIMEOUT_MS = 500; // safety: стоп, если нет команд

int motorAVal = 0; // -100..100, текущее значение (для отображения/отладки)
int motorBVal = 0;
int servoVal  = 0;

// ---------- Калибровка руля ----------
// us = центр + (val/100) * maxAngleDeg * (1000/180)
// 1000/180 — стандартная шкала хобби-серв: 1000мкс на 180°.
const float US_PER_DEGREE = 1000.0 / 180.0;
int steerCenterUs   = 1500; // по умолчанию — геометрический центр (1000-2000мкс)
int steerMaxAngleDeg = 45;  // по умолчанию — умеренный, безопасный для линковки угол

void loadSteerCal() {
  steerCenterUs = prefs.getInt("steer_c", 1500);
  steerMaxAngleDeg = prefs.getInt("steer_a", 45);
}
void applySteer(int val) {
  servoVal = val;
  int us = steerCenterUs + (int)((val / 100.0) * steerMaxAngleDeg * US_PER_DEGREE);
  us = constrain(us, 500, 2500); // абсолютный предохранитель — не даём улететь за физически безопасный диапазон
  steerServo.writeMicroseconds(us);
}

// ---------- Мотор ----------
// pwmChannel — номер LEDC-канала (LEDC_CH_A / LEDC_CH_B), НЕ номер GPIO.
void setMotor(int in1, int in2, int pwmChannel, int val) {
  val = constrain(val, -100, 100);
  int duty = map(abs(val), 0, 100, 0, 255);
  if (val > 0) { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); }
  else if (val < 0) { digitalWrite(in1, LOW); digitalWrite(in2, HIGH); }
  else { digitalWrite(in1, LOW); digitalWrite(in2, LOW); }
  ledcWrite(pwmChannel, duty);
}

void stopAll() {
  setMotor(TB_AIN1, TB_AIN2, LEDC_CH_A, 0);
  setMotor(TB_BIN1, TB_BIN2, LEDC_CH_B, 0);
  steerServo.writeMicroseconds(steerCenterUs); // стоп/центр (с учётом калибровки)
  motorAVal = motorBVal = servoVal = 0;
}

// ---------- Батарея ----------
float readBatteryVoltage() {
  // усреднение по 8 выборкам — ADC ESP32 шумит
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(BATT_PIN);
  float raw = sum / 8.0;
  float vAdc = raw / 4095.0 * 3.3;
  return vAdc / BATT_DIVIDER_FACTOR;
  // ВНИМАНИЕ: ADC ESP32 нелинеен у краёв диапазона. После сборки
  // сверить реальное напряжение батареи мультиметром и при
  // необходимости скорректировать BATT_DIVIDER_FACTOR.
}

int batteryPercent(float v) {
  if (v >= 8.2) return 100;
  if (v >= 7.8) return 75;
  if (v >= 7.4) return 50;
  if (v >= 7.0) return 25;
  if (v >= 6.8) return 10;
  return 0;
}

// ---------- OLED ----------
// Пороги предупреждения о разряде: 18650 безопасный минимум ~3.0В/банка, 2 банки последовательно = 6.0В.
// Ниже 0.5В — батарея физически не подключена (стенд на USB), это не разряд, предупреждение не нужно.
#define LOW_BATTERY_THRESHOLD_V 6.0
#define BATTERY_DISCONNECTED_V  0.5

void drawModeIcon() {
  int x0 = 96, y0 = 4;
  if (currentMode == MODE_TANK) {
    display.fillRect(x0 + 4, y0 + 10, 20, 12, SSD1306_WHITE);   // корпус
    display.fillRect(x0 + 10, y0 + 3, 8, 8, SSD1306_WHITE);     // башня
    display.drawLine(x0 + 18, y0 + 6, x0 + 27, y0 + 6, SSD1306_WHITE); // ствол
    display.fillRect(x0 + 1, y0 + 22, 26, 4, SSD1306_WHITE);    // гусеницы
  } else {
    display.fillRoundRect(x0 + 2, y0 + 8, 24, 10, 3, SSD1306_WHITE);  // кузов
    display.fillRoundRect(x0 + 8, y0 + 3, 12, 7, 2, SSD1306_WHITE);   // крыша/кабина
    display.fillCircle(x0 + 7, y0 + 20, 3, SSD1306_WHITE);            // колесо
    display.fillCircle(x0 + 21, y0 + 20, 3, SSD1306_WHITE);           // колесо
  }
}

void updateDisplay(int clients) {
  float v = readBatteryVoltage();
  int pct = batteryPercent(v);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Антидребезг: вызывается раз в секунду, тревога — после 3 замеров подряд ниже порога,
  // сброс — только выше порога +0.3В (или когда батарея отключена).
  static int lowBattCount = 0;
  bool below = (v > BATTERY_DISCONNECTED_V && v < LOW_BATTERY_THRESHOLD_V);
  if (below) { if (lowBattCount < 3) lowBattCount++; }
  else if (v > LOW_BATTERY_THRESHOLD_V + 0.3 || v <= BATTERY_DISCONNECTED_V) lowBattCount = 0;

  // Предупреждение о разряде — перекрывает всё остальное на экране
  if (lowBattCount >= 3) {
    display.setTextSize(3);
    display.setCursor(37, 4);
    display.print("LOW");
    display.setTextSize(2);
    display.setCursor(22, 32);
    display.print("BATTERY");
    display.setTextSize(1);
    display.setCursor(46, 54);
    display.printf("%.2fV\n", v);
    display.display();
    return;
  }

  // Крупно — то, что нужно видеть одним взглядом, не всматриваясь
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(currentMode == MODE_TANK ? "TANK" : "CAR");
  display.setCursor(0, 20);
  display.printf("%d%%\n", pct);

  // Мелко — техническая информация для отладки
  display.setTextSize(1);
  display.setCursor(0, 44);
  display.printf("%.2fV\n", v);
  display.setCursor(0, 54);
  display.printf("Clients: %d\n", clients);

  drawModeIcon();

  display.display();
}

// ---------- Режим (сохранение) ----------
void loadMode() {
  prefs.begin("cfg", false);
  currentMode = (Mode)prefs.getUChar("mode", MODE_TANK);
}
void saveMode(Mode m) {
  currentMode = m;
  prefs.putUChar("mode", (uint8_t)m);
}

// ---------- WebSocket ----------
void handleWsMessage(uint8_t *data, size_t len) {
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, data, len) != DeserializationError::Ok) return;

  lastCmdMillis = millis();

  if (doc.containsKey("mode")) {
    saveMode((Mode)doc["mode"].as<int>());
    stopAll();
    return;
  }

  // Калибровка руля: {"trim": -60..60} — смещение центра в мкс от 1500 (абсолютное значение, не дельта)
  if (doc.containsKey("trim")) {
    steerCenterUs = 1500 + constrain(doc["trim"].as<int>(), -400, 400);
    prefs.putInt("steer_c", steerCenterUs);
    applySteer(servoVal); // сразу применить с новым центром, не дожидаясь следующего движения слайдера
    return;
  }
  // {"maxdeg": 5..90} — половина полного диапазона поворота
  if (doc.containsKey("maxdeg")) {
    steerMaxAngleDeg = constrain(doc["maxdeg"].as<int>(), 5, 90);
    prefs.putInt("steer_a", steerMaxAngleDeg);
    applySteer(servoVal);
    return;
  }

  const char* ch = doc["ch"] | "";
  int val = doc["val"] | 0;

  if (currentMode == MODE_TANK) {
    if (strcmp(ch, "A") == 0) { motorAVal = val; setMotor(TB_AIN1, TB_AIN2, LEDC_CH_A, val); }
    else if (strcmp(ch, "B") == 0) { motorBVal = val; setMotor(TB_BIN1, TB_BIN2, LEDC_CH_B, val); }
  } else { // MODE_CAR
    if (strcmp(ch, "A") == 0) { motorAVal = val; setMotor(TB_AIN1, TB_AIN2, LEDC_CH_A, val); }      // drive
    else if (strcmp(ch, "B") == 0) { motorBVal = val; setMotor(TB_BIN1, TB_BIN2, LEDC_CH_B, val); } // accessory
    else if (strcmp(ch, "S") == 0) applySteer(val);                                                 // steer
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) handleWsMessage(data, len);
}

// ---------- HTML страница ----------
const char PAGE_HTML[] = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
<title>Lego Control Center</title>
<style>
:root{
  --bg:#0d0f12; --panel:#141820; --panel2:#1b2029; --border:#2a3140;
  --blue:#3b82f6; --blue-dk:#2563eb; --emerald:#10b981; --rose:#f43f5e; --cyan:#22d3ee;
  --text:#e2e8f0; --muted:#94a3b8;
}
*{box-sizing:border-box}
html,body{height:100%;margin:0;overflow:hidden;overscroll-behavior:none;touch-action:none;
  background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
  user-select:none;-webkit-user-select:none}
body{display:flex;flex-direction:column;align-items:center;justify-content:space-between;
  padding:env(safe-area-inset-top,10px) 10px env(safe-area-inset-bottom,10px)}
.wrap{width:100%;max-width:900px}

/* ---- шапка ---- */
.header{display:flex;justify-content:space-between;align-items:center;gap:8px;
  background:rgba(20,24,32,.85);border:1px solid var(--border);padding:8px 12px;border-radius:16px;box-shadow:0 4px 20px rgba(0,0,0,.3)}
.brand{display:flex;align-items:center;gap:10px}
.brand-icon{width:32px;height:32px;border-radius:10px;background:rgba(59,130,246,.15);
  border:1px solid rgba(59,130,246,.4);display:flex;align-items:center;justify-content:center;font-size:16px}
.brand-title{font-size:13px;font-weight:700;line-height:1}
.brand-sub{display:flex;align-items:center;gap:6px;font-size:10px;color:var(--muted);margin-top:3px}
.dot{width:6px;height:6px;border-radius:50%;background:var(--emerald);animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.mode-switch{display:flex;gap:4px;background:rgba(2,6,15,.6);border:1px solid var(--border);border-radius:12px;padding:4px}
.mode-btn{border:none;background:transparent;color:var(--muted);font-size:12px;font-weight:600;
  padding:7px 12px;border-radius:9px;cursor:pointer}
.mode-btn.active{background:var(--blue-dk);color:#fff;box-shadow:0 0 14px rgba(59,130,246,.5)}
.badge{font-size:10px;font-weight:700;padding:4px 8px;border-radius:8px;
  background:rgba(16,185,129,.1);color:var(--emerald);border:1px solid rgba(16,185,129,.25)}
.icon-btn{width:32px;height:32px;border-radius:10px;background:var(--panel2);border:1px solid var(--border);
  color:var(--text);display:flex;align-items:center;justify-content:center;cursor:pointer;font-size:14px}

/* ---- панели ---- */
.panel{background:rgba(20,24,32,.6);border:1px solid var(--border);border-radius:22px;
  padding:14px;box-shadow:0 20px 40px rgba(0,0,0,.35)}
.row{display:flex;align-items:center;justify-content:space-between;gap:10px}
.col{display:flex;flex-direction:column;align-items:center;gap:4px}
.lbl{font-size:10px;font-weight:600;letter-spacing:.05em;text-transform:uppercase;color:var(--muted)}
.val{font-size:12px;font-weight:700;font-family:monospace;color:var(--blue)}
.val.g{color:var(--emerald)} .val.c{color:var(--cyan)}

.slider-v-container{height:150px;width:44px;display:flex;align-items:center;justify-content:center;
  background:rgba(2,6,15,.7);border-radius:16px;border:1px solid var(--border);box-shadow:inset 0 2px 8px rgba(0,0,0,.4)}
input[type=range]{-webkit-appearance:none;background:transparent;touch-action:pan-y}
input[type=range]::-webkit-slider-runnable-track{width:100%;height:12px;cursor:pointer;background:#1b2029;border-radius:8px;border:1px solid var(--border)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;height:34px;width:24px;border-radius:8px;
  background:var(--blue);cursor:pointer;margin-top:-12px;border:2px solid #fff;box-shadow:0 0 12px rgba(59,130,246,.8)}
.slider-v{width:150px !important;height:44px !important;transform:rotate(-90deg);transform-origin:center}

.stop-btn{margin-top:8px;padding:7px 16px;background:rgba(244,63,94,.15);border:1px solid rgba(244,63,94,.4);
  color:var(--rose);font-weight:700;font-size:12px;border-radius:12px;cursor:pointer}

.side{width:170px;display:flex;flex-direction:column;gap:10px}
.field{background:rgba(2,6,15,.55);padding:10px;border-radius:16px;border:1px solid var(--border)}
.field .row{margin-bottom:4px}
.field label{font-size:11px;font-weight:600;color:#cbd5e1}
.field input[type=range]{width:100%}

.hidden{display:none !important}

.gear{transform-box:fill-box;transform-origin:center}
.wheel{transform-box:fill-box;transform-origin:center;transition:transform .08s ease-out}

.footer{width:100%;background:rgba(20,24,32,.9);border:1px solid var(--border);border-radius:14px;
  padding:6px 12px;display:flex;justify-content:space-between;align-items:center;margin-top:8px}
.footer span:first-child{font-size:10px;color:var(--muted)}
.footer code{font-size:11px;color:var(--cyan);background:#020617;padding:2px 8px;border-radius:6px;border:1px solid var(--border)}

.overlay{display:none;position:fixed;inset:0;z-index:50;background:rgba(2,6,15,.96);
  flex-direction:column;align-items:center;justify-content:center;padding:24px;text-align:center}
@media (orientation:portrait){.overlay{display:flex}}
.overlay-icon{width:70px;height:70px;margin-bottom:20px;border-radius:20px;background:rgba(59,130,246,.15);
  border:1px solid rgba(59,130,246,.4);display:flex;align-items:center;justify-content:center;font-size:34px}
.overlay h2{font-size:18px;margin:0 0 8px}
.overlay p{font-size:13px;color:var(--muted);max-width:280px;margin:0 0 24px;line-height:1.5}
.fs-btn{padding:13px 22px;border-radius:16px;background:var(--blue-dk);color:#fff;border:none;
  font-weight:700;font-size:13px;cursor:pointer;box-shadow:0 0 20px rgba(59,130,246,.4)}
</style>
</head>
<body>

<div class="overlay" id="rotate-overlay">
  <div class="overlay-icon">&#128241;</div>
  <h2>Поверните устройство</h2>
  <p>Для управления моделью переведите телефон в горизонтальное положение или включите полноэкранный режим.</p>
  <button class="fs-btn" onclick="toggleFullscreen()">&#9974; Включить полный экран</button>
</div>

<div class="wrap">
  <div class="header">
    <div class="brand">
      <div class="brand-icon">&#9881;</div>
      <div>
        <div class="brand-title">Lego Control Center</div>
        <div class="brand-sub"><span class="dot"></span><span id="ip-address">192.168.4.1</span></div>
      </div>
    </div>
    <div class="mode-switch">
      <button id="btn-tank" class="mode-btn active" onclick="switchMode(0)">&#128737; Танковый</button>
      <button id="btn-classic" class="mode-btn" onclick="switchMode(1)">&#128663; Классический</button>
    </div>
    <div class="row" style="gap:6px">
      <button class="icon-btn" onclick="toggleFullscreen()" title="На весь экран">&#9974;</button>
      <span class="badge">ESP32</span>
    </div>
  </div>

  <main id="tank-panel" class="panel" style="margin-top:10px">
    <div class="row">
      <div class="col">
        <span class="lbl">Левая
