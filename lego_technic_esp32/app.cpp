// ESP32 Lego Technic motorization — Версия: 0.3.2
// Библиотеки: ESPAsyncWebServer, AsyncTCP, ArduinoJson, Adafruit_SSD1306, Adafruit_GFX, ESP32Servo, ElegantOTA
// ESP32 core: 2.0.9 (совместимость с LEDC и AsyncWebServer)
// CHANGELOG 0.3.2:
// - Переписан GUI heartbeat: единый объект lastSent{A,B,C,S}, обновляется внутри txWS(),
//   раз в 150мс переотправляются все 4 канала без привязки к режиму. В 0.3.1 heartbeat
//   слал только канал A (дважды), причём в Классике/Тесте второй раз брал значение
//   из скрытого танкового ползунка (всегда 0) — газ в Классике дёргался каждые ~150мс.
//   Канал B/C/S heartbeat вообще не видел — Cruise Control и Тестовый режим по-прежнему
//   обнулялись watchdog'ом через 500мс. Сейчас это исправлено единообразно для всех режимов.
// (0.3.1: исправлена полярность DRV8825 (раздельный DRV_EN, инверсия в updateDriverStandby),
//  скорость мотора C — через ledcChangeFrequency вместо duty, LOW BATTERY на OLED,
//  иконки режимов (танк/машина/ключ), /calib эндпоинт + trim/maxdeg в статусе,
//  убран сброс фар при смене режима в zeroAll())

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

// ---------- Пины Драйверов и Периферии ----------
#define TB_STBY   4   // TB6612 STBY (HIGH = Включен)
#define DRV_EN    13  // DRV8825 nENABLE/nSLEEP (LOW = Включен). ВАЖНО: физически отделён от GPIO 4!

// TB6612FNG — Моторы A и B
#define TB_AIN1   16
#define TB_AIN2   17
#define TB_PWMA   18
#define TB_BIN1   19
#define TB_BIN2   21
#define TB_PWMB   22

// DRV8825 — Мотор C
#define DRV_DIR   23
#define DRV_STEP  5

// LEDC Каналы (ШИМ)
#define LEDC_CH_A 4
#define LEDC_CH_B 5
#define LEDC_CH_C 6

// Прочая периферия
#define SERVO_PIN   27
#define OLED_SDA    25
#define OLED_SCL    26
#define BATT_PIN    34
#define LED_FRONT_PIN 32
#define LED_REAR_PIN  33

#define BATT_DIVIDER_FACTOR 0.2680

// ---------- Переменные Режимов и Состояния ----------
enum Mode { MODE_TANK = 0, MODE_CAR = 1, MODE_TEST = 2 };
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
int servoVal  = 0;

bool reverseA = false;
bool reverseB = false;
bool reverseC = false;

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
  ledFrontOn = prefs.getUChar("ledF", 0) != 0;
  ledRearOn  = prefs.getUChar("ledR", 0) != 0;
  steerCenterUs = prefs.getInt("steer_c", 1500);
  steerMaxAngleDeg = prefs.getInt("steer_a", 45);
}

void applyLeds() {
  digitalWrite(LED_FRONT_PIN, ledFrontOn ? HIGH : LOW);
  digitalWrite(LED_REAR_PIN, ledRearOn ? HIGH : LOW);
}

void applySteer(int val) {
  servoVal = val;
  int us = steerCenterUs + (int)((val / 100.0) * steerMaxAngleDeg * US_PER_DEGREE);
  us = constrain(us, 500, 2500);
  steerServo.writeMicroseconds(us);
}

// ---------- Управление Моторами ----------
void setDCBridge(int in1, int in2, int pwmChannel, int val) {
  val = constrain(val, -100, 100);
  int duty = map(abs(val), 0, 100, 0, 255);
  if (val > 0) { digitalWrite(in1, HIGH); digitalWrite(in2, LOW); }
  else if (val < 0) { digitalWrite(in1, LOW); digitalWrite(in2, HIGH); }
  else { digitalWrite(in1, LOW); digitalWrite(in2, LOW); }
  ledcWrite(pwmChannel, duty);
}

void setDRV8825Motor(int dirPin, int pwmChannel, int val) {
  val = constrain(val, -100, 100);
  digitalWrite(dirPin, val >= 0 ? HIGH : LOW);

  if (val == 0) {
    ledcWrite(pwmChannel, 0);
  } else {
    // Для шагового драйвера важна частота фронтов, а не скважность
    int freq = map(abs(val), 1, 100, 100, 5000); // От 100 Гц до 5000 Гц
    ledcChangeFrequency(pwmChannel, freq, 8);
    ledcWrite(pwmChannel, 128); // 50% duty cycle для чётких фронтов
  }
}

void applyMotorA(int val) {
  motorAVal = val;
  setDCBridge(TB_AIN1, TB_AIN2, LEDC_CH_A, reverseA ? -val : val);

  if (engineSimOn && currentMode == MODE_CAR) {
    int aux = (int)(33.0 + 67.0 * abs(motorAVal) / 100.0 + 0.5);
    int actualB = reverseB ? -aux : aux;
    motorBVal = actualB;
    setDCBridge(TB_BIN1, TB_BIN2, LEDC_CH_B, actualB);
  }
}

void applyMotorB(int val) {
  motorBVal = val;
  setDCBridge(TB_BIN1, TB_BIN2, LEDC_CH_B, reverseB ? -val : val);
}

void applyMotorC(int val) {
  motorCVal = val;
  setDRV8825Motor(DRV_DIR, LEDC_CH_C, reverseC ? -val : val);
}

void updateDriverStandby() {
  bool active = (motorAVal != 0) || (motorBVal != 0) || (motorCVal != 0) || (servoVal != 0) || engineSimOn;
  // TB6612 включается HIGH, DRV8825 включается LOW
  digitalWrite(TB_STBY, active ? HIGH : LOW);
  digitalWrite(DRV_EN, active ? LOW : HIGH);
}

void stopAll() {
  motorAVal = 0;
  motorCVal = 0;
  applyMotorA(0);
  applyMotorC(0);
  applySteer(0);

  if (!engineSimOn) {
    motorBVal = 0;
    applyMotorB(0);
  }
  updateDriverStandby();
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
                  ? (motorAVal == 0 && motorCVal == 0 && servoVal == 0)
                  : (motorAVal == 0 && motorBVal == 0 && motorCVal == 0 && servoVal == 0);

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
  snprintf(buf, n, "{\"st\":1,\"v\":%.2f,\"p\":%d,\"r\":%d,\"m\":%d,\"c\":%d,\"ra\":%d,\"rb\":%d,\"rc\":%d,\"lf\":%d,\"lr\":%d,\"es\":%d,\"tr\":%d,\"md\":%d}",
           battV, battPct, wifiRssi, (int)currentMode, (int)ws.count(),
           reverseA ? 1 : 0, reverseB ? 1 : 0, reverseC ? 1 : 0,
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

// ---------- WebSocket ----------
void handleWsMessage(uint8_t *data, size_t len) {
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, data, len) != DeserializationError::Ok) return;

  lastCmdMillis = millis();

  if (doc.containsKey("mode")) {
    currentMode = (Mode)doc["mode"].as<int>();
    engineSimOn = false;
    stopAll();
    return;
  }

  if (doc.containsKey("trim")) {
    steerCenterUs = 1500 + constrain(doc["trim"].as<int>(), -400, 400);
    prefs.putInt("steer_c", steerCenterUs);
    applySteer(servoVal);
    return;
  }

  if (doc.containsKey("maxdeg")) {
    steerMaxAngleDeg = constrain(doc["maxdeg"].as<int>(), 5, 90);
    prefs.putInt("steer_a", steerMaxAngleDeg);
    applySteer(servoVal);
    return;
  }

  if (doc.containsKey("revA")) { reverseA = doc["revA"].as<int>() == 1; prefs.putUChar("revA", reverseA ? 1 : 0); }
  if (doc.containsKey("revB")) { reverseB = doc["revB"].as<int>() == 1; prefs.putUChar("revB", reverseB ? 1 : 0); }
  if (doc.containsKey("revC")) { reverseC = doc["revC"].as<int>() == 1; prefs.putUChar("revC", reverseC ? 1 : 0); }

  if (doc.containsKey("ledF")) { ledFrontOn = doc["ledF"].as<int>() == 1; prefs.putUChar("ledF", ledFrontOn ? 1 : 0); applyLeds(); }
  if (doc.containsKey("ledR")) { ledRearOn = doc["ledR"].as<int>() == 1; prefs.putUChar("ledR", ledRearOn ? 1 : 0); applyLeds(); }

  if (doc.containsKey("eng")) {
    engineSimOn = doc["eng"].as<int>() == 1;
    if (!engineSimOn) {
      motorBVal = 0;
      applyMotorB(0);
    } else {
      applyMotorA(motorAVal);
    }
  }

  const char* ch = doc["ch"] | "";
  int val = doc["val"] | 0;

  if (strcmp(ch, "A") == 0) applyMotorA(val);
  else if (strcmp(ch, "B") == 0 && !engineSimOn) applyMotorB(val);
  else if (strcmp(ch, "C") == 0) applyMotorC(val);
  else if (strcmp(ch, "S") == 0) applySteer(val);

  updateDriverStandby();
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    char buf[200];
    buildStatus(buf, sizeof(buf));
    client->text(buf);
  } else if (type == WS_EVT_DATA) {
    handleWsMessage(data, len);
  }
}

// ---------- HTML GUI v0.3.2 ----------
const char PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
<title>Lego Control Center</title>
<style>
:root{
  --bg:#0d0f12; --panel:#141820; --panel2:#1b2029; --border:#2a3140;
  --blue:#3b82f6; --blue-dk:#2563eb; --emerald:#10b981; --rose:#f43f5e; --amber:#f59e0b; --cyan:#22d3ee;
  --text:#e2e8f0; --muted:#94a3b8;
  --track:#1b2029; --svg-fill:#1b2029; --svg-stroke:#2a3140; --svg-deep:#0d0f12;
  --sh:180px; --T:64px;
}
[data-theme="light"]{
  --bg:#e8eef5; --panel:#ffffff; --panel2:#eef2f7; --border:#c5d0de;
  --blue:#2563eb; --blue-dk:#1d4ed8; --emerald:#059669; --rose:#e11d48; --amber:#d97706; --cyan:#0891b2;
  --text:#0f172a; --muted:#64748b;
  --track:#dbe3ee; --svg-fill:#dbe3ee; --svg-stroke:#94a3b8; --svg-deep:#cbd5e1;
}
*{box-sizing:border-box}
html,body{height:100%;margin:0;overflow:hidden;overscroll-behavior:none;touch-action:none;
  background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
  user-select:none;-webkit-user-select:none}
body{padding:max(env(safe-area-inset-top,0px),6px) 10px max(env(safe-area-inset-bottom,0px),6px);
  display:flex;justify-content:center}
.wrap{width:100%;max-width:1000px;height:100%;display:flex;flex-direction:column;gap:6px}

.header{flex:none;display:flex;justify-content:space-between;align-items:center;gap:8px;
  background:var(--panel);border:1px solid var(--border);padding:6px 10px;border-radius:14px}
.brand{display:flex;align-items:center;gap:8px;min-width:0}
.brand-icon{width:28px;height:28px;border-radius:8px;background:rgba(59,130,246,.15);
  border:1px solid rgba(59,130,246,.4);display:flex;align-items:center;justify-content:center;color:var(--blue);flex-shrink:0}
.brand-title{font-size:12px;font-weight:700;line-height:1}
.brand-sub{display:flex;align-items:center;gap:5px;font-size:9px;color:var(--muted);margin-top:2px}
.brand-ver{font-size:9px;color:var(--muted);opacity:.7;margin-top:1px}
.dot{width:6px;height:6px;border-radius:50%;background:var(--emerald);animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.mode-switch{display:flex;gap:3px;background:var(--panel2);border:1px solid var(--border);border-radius:11px;padding:3px;flex-shrink:0}
.mode-btn{border:none;background:transparent;color:var(--muted);font-size:11px;font-weight:600;
  padding:6px 10px;border-radius:8px;cursor:pointer;white-space:nowrap}
.mode-btn.active{background:var(--blue-dk);color:#fff;box-shadow:0 0 12px rgba(59,130,246,.45)}
.badge{font-size:9px;font-weight:700;padding:3px 7px;border-radius:7px;white-space:nowrap;
  background:rgba(16,185,129,.12);color:var(--emerald);border:1px solid rgba(16,185,129,.3)}
.icon-btn{width:30px;height:30px;border-radius:9px;background:var(--panel2);border:1px solid var(--border);
  color:var(--text);display:flex;align-items:center;justify-content:center;cursor:pointer;font-size:13px;flex-shrink:0}
.hstats{display:flex;align-items:center;gap:10px}
.stat{display:flex;align-items:center;gap:5px;font-size:10px;font-weight:600;white-space:nowrap}
.bars{display:flex;align-items:flex-end;gap:2px;height:14px}
.bars i{display:block;width:3px;border-radius:1px;background:var(--border)}
.bars i:nth-child(1){height:4px}.bars i:nth-child(2){height:7px}
.bars i:nth-child(3){height:11px}.bars i:nth-child(4){height:14px}
.bars.w1 i.on{background:var(--rose)}.bars.w2 i.on{background:var(--amber)}
.bars.w3 i.on,.bars.w4 i.on{background:var(--emerald)}
.bat{position:relative;width:24px;height:12px;border:2px solid var(--muted);border-radius:3px;padding:1px}
.bat::after{content:"";position:absolute;right:-4px;top:2px;width:2px;height:5px;background:var(--muted);border-radius:0 1px 1px 0}
.bat-fill{height:100%;width:70%;background:var(--emerald);border-radius:1px}
.bat.warn .bat-fill{background:var(--amber)}.bat.crit .bat-fill{background:var(--rose)}

.panel{flex:1;min-height:0;display:flex;align-items:center;justify-content:center;
  background:var(--panel);border:1px solid var(--border);border-radius:18px;padding:10px;overflow:hidden}
.cluster{display:flex;align-items:center;justify-content:center;gap:clamp(14px,4vw,40px);height:100%;width:100%}
.col{display:flex;flex-direction:column;align-items:center;gap:3px;height:100%;justify-content:center}
.lbl{font-size:9px;font-weight:600;letter-spacing:.04em;text-transform:uppercase;color:var(--muted);white-space:nowrap}
.val{font-size:12px;font-weight:700;font-family:ui-monospace,monospace;color:var(--blue)}
.val.g{color:var(--emerald)}.val.c{color:var(--cyan)}.val.a{color:var(--amber)}

input[type=range]{-webkit-appearance:none;appearance:none;background:transparent;touch-action:pan-y;margin:0}
input[type=range]::-webkit-slider-runnable-track{
  height:22px;cursor:pointer;background:#1b2029;border-radius:12px;border:1px solid var(--border)}
[data-theme="light"] input[type=range]::-webkit-slider-runnable-track{background:#dbe3ee}
input[type=range]::-webkit-slider-thumb{
  -webkit-appearance:none;width:32px;height:60px;margin-top:-19px;
  border-radius:12px;background:var(--blue);cursor:pointer;border:2px solid #fff;
  box-shadow:0 0 14px rgba(59,130,246,.8)}
input[type=range]:disabled::-webkit-slider-thumb{background:#475569;box-shadow:none;border-color:#64748b}
input[type=range]:disabled::-webkit-slider-runnable-track{opacity:.4}
input.slim::-webkit-slider-runnable-track{height:14px}
input.slim::-webkit-slider-thumb{width:22px;height:40px;margin-top:-13px;border-radius:10px}

.slider-v-container{height:var(--sh);width:var(--T);display:flex;align-items:center;justify-content:center;
  background:rgba(2,6,15,.7);border-radius:20px;border:1px solid var(--border);box-shadow:inset 0 2px 8px rgba(0,0,0,.4)}
[data-theme="light"] .slider-v-container{background:#e2e8f0;box-shadow:inset 0 2px 6px rgba(0,0,0,.08)}
.slider-v{flex:none;width:var(--sh)!important;height:var(--T)!important;transform:rotate(-90deg);transform-origin:center}
.slider-h-container{width:100%;height:var(--T);display:flex;align-items:center;padding:0 8px;
  background:rgba(2,6,15,.7);border-radius:20px;border:1px solid var(--border);box-shadow:inset 0 2px 8px rgba(0,0,0,.4)}
[data-theme="light"] .slider-h-container{background:#e2e8f0;box-shadow:inset 0 2px 6px rgba(0,0,0,.08)}
.slider-h-container input{width:100%}

.slider-v-container input.slider-v::-webkit-slider-thumb{
  width:36px;height:90px;margin-top:-34px;border-radius:14px}
.slider-v-container .slider-v{height:110px!important}

.slim-container { position: relative; min-height: 52px; margin-top: 4px; }
.slim-wrap {
  height: 52px; display: flex; align-items: center; padding: 0 8px;
  background: rgba(2,6,15,.7); border-radius: 12px; border: 1px solid var(--border);
  box-shadow: inset 0 2px 8px rgba(0,0,0,.4);
}
[data-theme="light"] .slim-wrap { background:#e2e8f0; box-shadow: inset 0 2px 6px rgba(0,0,0,.08); }
.slim-wrap input { width: 100%; }
.slim-container > .slim-wrap {
  position: absolute; top: 0; left: 0; right: 0; height: 52px;
  opacity: 1; transform: scale(1);
  transition: opacity 0.35s ease, transform 0.35s ease; pointer-events: auto;
}
.slim-container > .slim-wrap.hidden { opacity: 0; transform: scale(0.95); pointer-events: none; }
.eng-label {
  position: absolute; top: 0; left: 0; right: 0; height: 52px;
  display: flex; align-items: center; justify-content: center;
  font-size: 13px; font-weight: 800; letter-spacing: .04em; text-transform: uppercase;
  color: var(--amber); text-align: center;
  background: rgba(245,158,11,.08); border-radius: 12px; border: 1px solid rgba(245,158,11,.35);
  opacity: 0; transform: scale(0.95);
  transition: opacity 0.35s ease, transform 0.35s ease; pointer-events: none;
}
.eng-label.visible { opacity: 1; transform: scale(1); pointer-events: auto; }

.tank-svg{height:calc(var(--sh) - 8px);width:auto;aspect-ratio:240/280;max-height:100%}
.car-svg{height:calc(var(--sh) - 4px);width:auto;aspect-ratio:240/200;max-height:100%}
.car-wrap{position:relative;display:flex;flex-direction:column;align-items:center}
.btn-row{display:flex;gap:8px;margin-top:6px;flex-shrink:0}

#tank-panel .cluster{justify-content:space-evenly;width:100%;padding:0 8px}
#tank-panel .slider-v-container{width:168px}
#classic-panel .cluster{display:grid;grid-template-columns:1.15fr .85fr 1.15fr;align-items:center;width:100%}
#classic-panel .col:first-child{justify-self:center}
#classic-panel .slider-v-container{width:160px}
#classic-panel .side{width:min(340px,36vw)}

#test-panel .cluster{display:flex;align-items:center;justify-content:space-around;width:100%;height:100%;gap:clamp(16px,4vw,40px)}
#test-panel .motors-group{display:flex;align-items:center;justify-content:center;gap:clamp(24px,5vw,48px)}
#test-panel .servo-group{display:flex;flex-direction:column;align-items:center;justify-content:center;width:min(380px,40vw);gap:16px}

.side-stack { position: relative; flex: 1 1 auto; min-height: 0; width: 100%; overflow: hidden; }
.dash{
  position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; gap: 10px;
  opacity: 1; transform: translateY(0);
  transition: transform 0.42s cubic-bezier(0.22, 0.9, 0.3, 1), opacity 0.35s ease; pointer-events: auto;
}
.dash.hide{ opacity: 0; transform: translateY(-110%); pointer-events: none; }
.side-top{
  position: absolute; inset: 0; display: flex; flex-direction: column; gap: 6px; justify-content: flex-start;
  opacity: 0; transform: translateY(110%);
  transition: transform 0.42s cubic-bezier(0.22, 0.9, 0.3, 1), opacity 0.35s ease; pointer-events: none;
}
.side-top.open{ opacity: 1; transform: translateY(0); pointer-events: auto; }
.side-top .field{flex:1;display:flex;flex-direction:column;justify-content:center}

.gauge{display:flex;flex-direction:column;align-items:center;width:46%}
.gauge svg{width:100%;height:auto;max-height:118px}
.dead-btn{opacity:.38;pointer-events:none;font-weight:800;font-size:14px;color:var(--muted)}

.aux-btn{
  width:44px;height:44px;border-radius:14px;
  background:rgba(20,24,32,.85);border:1.5px solid #2a3140;
  color:#64748b;cursor:pointer;
  display:flex;align-items:center;justify-content:center;
  transition:border-color .28s ease, box-shadow .28s ease, color .28s ease, background .28s ease, transform .18s ease;
  font-size:20px;line-height:1;padding:0;
}
.aux-btn:active{transform:scale(.92)}
[data-theme="light"] .aux-btn{background:#eef2f7;border-color:#c5d0de;color:#94a3b8}
.aux-btn .ico{font-size:20px;line-height:1;filter:grayscale(.3) opacity(.75)}
.aux-btn.active .ico{filter:none;opacity:1}

.led-btn[data-ch="F"].active{ border-color:#eab308;color:#eab308; box-shadow:0 0 14px rgba(234,179,8,.5), inset 0 0 8px rgba(234,179,8,.1); }
.led-btn[data-ch="R"].active{ border-color:#f43f5e;color:#f43f5e; box-shadow:0 0 14px rgba(244,63,94,.5), inset 0 0 8px rgba(244,63,94,.1); }
.reset-btn:active{ border-color:var(--rose);color:var(--rose); box-shadow:0 0 14px rgba(244,63,94,.5); }
.eng-btn.active{ border-color:#f59e0b;color:#f59e0b; box-shadow:0 0 12px rgba(245,158,11,.45), inset 0 0 8px rgba(245,158,11,.08); }
.aux-mode-btn.active{ border-color:#22d3ee;color:#22d3ee; box-shadow:0 0 12px rgba(34,211,238,.4), inset 0 0 8px rgba(34,211,238,.08); }

.rev-btn{margin-top:4px;padding:4px 8px;border-radius:8px;background:var(--panel2);border:1px solid var(--border);
  color:var(--muted);font-size:9px;font-weight:700;cursor:pointer;white-space:nowrap}
.rev-btn.active{background:rgba(59,130,246,.15);border-color:var(--blue);color:var(--blue)}

.hold-btn{padding:4px 8px;border-radius:8px;background:var(--panel2);border:1px solid var(--border);
  color:var(--muted);font-size:9px;font-weight:700;cursor:pointer;white-space:nowrap}
.hold-btn.active{background:rgba(16,185,129,.15);border-color:var(--emerald);color:var(--emerald)}

.side{ width:min(260px,28vw);align-self:stretch; display:flex;flex-direction:column;gap:6px; min-height:0; }
.side-bottom{flex:0 0 auto;margin-top:auto}
.field{background:var(--panel2);padding:8px;border-radius:12px;border:1px solid var(--border)}
.field label{font-size:11px;font-weight:600;color:var(--text)}
.field .row{display:flex;align-items:center;justify-content:space-between;margin-bottom:4px}

.mini-btn{width:28px;height:28px;border-radius:8px;background:var(--panel2);border:1px solid var(--border);
  color:var(--text);font-size:14px;cursor:pointer;padding:0}
.sheet .mini-btn{width:52px;height:48px;font-size:22px;border-radius:12px}
.sheet .rev-btn, .sheet .hold-btn{min-width:64px;min-height:44px;font-size:14px;padding:8px 12px;border-radius:12px}
.sheet .num{width:72px;height:44px;font-size:16px}
.num{width:48px;height:28px;background:var(--panel);color:var(--text);border:1px solid var(--border);border-radius:7px;padding:2px;text-align:center;font-weight:700;font-size:12px}

.sheet select {
  width: 100%; padding: 8px; background: var(--panel); color: var(--text);
  border: 1px solid var(--border); border-radius: 8px; font-size: 13px; font-weight: 600; outline: none;
}

.hidden{display:none!important}
.wheel{transform-box:fill-box;transform-origin:center;transition:transform .08s ease-out}

#t-left-body, #t-right-body, #t-left-f, #t-left-r, #t-right-f, #t-right-r, #w-rl, #w-rr, #c-fwd, #c-rev { transition: stroke 0.36s ease, fill 0.36s ease, filter 0.36s ease; }
#t-left-body.active-light, #t-right-body.active-light, #t-left-f.active-light, #t-left-r.active-light, #t-right-f.active-light, #t-right-r.active-light, #w-rl.active-light, #w-rr.active-light, #c-fwd.active-light, #c-rev.active-light { transition: stroke 0.18s ease, fill 0.18s ease, filter 0.18s ease; }

#car-body { transition: stroke 1.00s cubic-bezier(.22,.9,.3,1), stroke-width 1.00s cubic-bezier(.22,.9,.3,1), filter 1.00s cubic-bezier(.22,.9,.3,1); }
#car-body.eng-glow { transition: stroke 0.50s cubic-bezier(.22,.9,.3,1), stroke-width 0.50s cubic-bezier(.22,.9,.3,1), filter 0.50s cubic-bezier(.22,.9,.3,1); }

.sparks { pointer-events:none; opacity: 0; transition: opacity 1.00s ease; }
.sparks.on { opacity: 1; transition: opacity 0.50s ease; }

#motors-icon { transition: opacity 0.40s ease; }
#motor-b-group { transition: opacity 0.40s ease, transform 0.40s cubic-bezier(0.22, 0.9, 0.3, 1); }
#motor-c-group { transition: opacity 0.40s ease, transform 0.40s cubic-bezier(0.22, 0.9, 0.3, 1); }
#motor-c-group.centered { transform: translateY(-28px); }

@keyframes boltPulse{0%,100%{opacity:.55}50%{opacity:1}}
.bolt-glow{filter:drop-shadow(0 0 5px #22d3ee)}
.bolt-pulse{animation:boltPulse 1s ease-in-out infinite}
@keyframes sparkFly{0%{transform:translate(0,0) scale(1);opacity:1}100%{transform:translate(var(--sx),var(--sy)) scale(.2);opacity:0}}
.spark{position:absolute;width:3px;height:3px;border-radius:50%;background:var(--amber);
  box-shadow:0 0 6px 2px rgba(245,158,11,.8);animation:sparkFly .9s ease-out infinite}

.footer{flex:none;background:var(--panel);border:1px solid var(--border);border-radius:12px;
  padding:5px 10px;display:flex;justify-content:space-between;align-items:center}
.footer span:first-child{font-size:9px;color:var(--muted)}
.footer code{font-size:10px;color:var(--cyan);background:var(--panel2);padding:2px 7px;border-radius:5px;border:1px solid var(--border)}

.overlay{display:none;position:fixed;inset:0;z-index:50;background:var(--bg);
  flex-direction:column;align-items:center;justify-content:center;padding:24px;text-align:center}
@media (orientation:portrait){.overlay{display:flex}}
.overlay-icon{width:64px;height:64px;margin-bottom:16px;border-radius:16px;background:rgba(59,130,246,.15);
  border:1px solid rgba(59,130,246,.4);display:flex;align-items:center;justify-content:center;font-size:28px}
.overlay h2{font-size:16px;margin:0 0 6px}
.overlay p{font-size:12px;color:var(--muted);max-width:260px;margin:0 0 18px;line-height:1.45}
.fs-btn{padding:12px 20px;border-radius:14px;background:var(--blue-dk);color:#fff;border:none;
  font-weight:700;font-size:13px;cursor:pointer;box-shadow:0 0 16px rgba(59,130,246,.4)}

.sbg{position:fixed;inset:0;z-index:40;background:rgba(0,0,0,.45); opacity:0;pointer-events:none;transition:opacity .28s ease}
.sbg.open{opacity:1;pointer-events:auto}
.sheet{position:fixed;top:0;right:0;bottom:0;z-index:41;width:min(300px,85vw);
  background:var(--panel);border-left:1px solid var(--border); transform:translateX(105%);
  transition:transform .34s cubic-bezier(.22,.9,.3,1); display:flex;flex-direction:column;overflow:hidden;touch-action:pan-y;
  will-change:transform;box-shadow:-12px 0 32px rgba(0,0,0,.35)}
.sheet.open{transform:translateX(0)}
.sheet-h{flex:0 0 auto;display:flex;align-items:center;justify-content:space-between;padding:10px 12px;border-bottom:1px solid var(--border)}
.sheet-b{flex:1 1 auto;min-height:0;overflow-y:auto;-webkit-overflow-scrolling:touch;padding:10px 12px 24px;
  display:flex;flex-direction:column;gap:8px;touch-action:pan-y}
.block{background:var(--panel2);border:1px solid var(--border);border-radius:10px;padding:10px;flex-shrink:0}
.block h3{font-size:10px;text-transform:uppercase;letter-spacing:.05em;color:var(--muted);margin:0 0 8px}
.line{display:flex;align-items:center;gap:6px;margin-bottom:6px;font-size:11px;font-weight:600;color:var(--muted)}
.line:last-child{margin-bottom:0}
.grow{flex:1}
.svg-body{fill:var(--svg-fill);stroke:var(--svg-stroke)}
.svg-deep{fill:var(--svg-deep);stroke:var(--svg-stroke)}
</style>
</head>
<body>

<div class="overlay" id="rotate-overlay">
  <div class="overlay-icon">📱</div>
  <h2>Поверните устройство</h2>
  <p>Для управления переведите телефон в горизонтальное положение или включите полный экран.</p>
  <button class="fs-btn" onclick="toggleFullscreen()">⛶ Включить полный экран</button>
</div>

<div class="wrap">
  <div class="header">
    <div class="brand">
      <div class="brand-icon">
        <svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor">
          <rect x="2" y="5" width="20" height="15" rx="3" fill="none" stroke="currentColor" stroke-width="2"/>
          <circle cx="7" cy="9" r="2"/><circle cx="17" cy="9" r="2"/><circle cx="7" cy="16" r="2"/><circle cx="17" cy="16" r="2"/>
        </svg>
      </div>
      <div>
        <div class="brand-title">Lego Control Center</div>
        <div class="brand-sub"><span class="dot"></span><span id="ip-addr">192.168.4.1</span></div>
        <div class="brand-ver">0.3.2</div>
      </div>
    </div>
    <div class="mode-switch">
      <button id="btn-tank" class="mode-btn active" type="button">🛡 Танковый</button>
      <button id="btn-classic" class="mode-btn" type="button">🚗 Классический</button>
      <button id="btn-test" class="mode-btn" type="button">🔧 Тестовый</button>
    </div>
    <div style="display:flex;align-items:center;gap:8px;flex-shrink:0">
      <div class="hstats">
        <div class="stat">
          <div class="bars w0" id="wifi-bars"><i></i><i></i><i></i><i></i></div>
          <span id="wifi-txt">--</span>
        </div>
        <div class="stat">
          <div class="bat" id="bat-ic"><div class="bat-fill" id="bat-fill" style="width:0%"></div></div>
          <span id="batt-txt">--% · --V</span>
        </div>
      </div>
      <button class="icon-btn" type="button" onclick="toggleFullscreen()" title="Полный экран">⛶</button>
      <button class="icon-btn" type="button" id="themeBtn" title="Тема">🌙</button>
      <button class="icon-btn" type="button" id="setBtn" title="Настройки">⚙</button>
      <span class="badge" id="ws-status">ПОДКЛЮЧЕНИЕ</span>
    </div>
  </div>

  <main id="tank-panel" class="panel">
    <div class="cluster">
      <div class="col">
        <span class="lbl">Левая гусеница</span>
        <span class="val" id="val-left">0%</span>
        <div class="slider-v-container"><input type="range" id="slider-left" class="slider-v" min="-100" max="100" value="0"></div>
      </div>
      <div class="col">
        <svg class="tank-svg" viewBox="0 0 240 280">
          <rect x="64" y="30" width="112" height="220" rx="20" class="svg-body" stroke-width="3"/>
          <rect x="80" y="52" width="80" height="176" rx="12" class="svg-deep" stroke-width="2"/>
          <rect x="112" y="24" width="16" height="96" rx="5" fill="var(--svg-stroke)" stroke="var(--blue)" stroke-width="1.5" opacity=".7"/>
          <circle cx="120" cy="150" r="34" class="svg-body" stroke="var(--blue)" stroke-width="2.5" opacity=".9"/>
          <g id="t-left-group">
            <rect id="t-left-body" x="6" y="14" width="62" height="252" rx="14" class="svg-deep" stroke="var(--svg-stroke)" stroke-width="3"/>
            <path d="M 6 32 H 68 M 6 52 H 68 M 6 72 H 68 M 6 92 H 68 M 6 112 H 68 M 6 132 H 68 M 6 152 H 68 M 6 172 H 68 M 6 192 H 68 M 6 212 H 68 M 6 232 H 68 M 6 248 H 68" stroke="var(--svg-stroke)" stroke-width="2" stroke-linecap="round"/>
            <polygon id="t-left-f" points="37,42 18,72 56,72" fill="var(--svg-stroke)"/>
            <polygon id="t-left-r" points="37,238 18,208 56,208" fill="var(--svg-stroke)"/>
          </g>
          <g id="t-right-group">
            <rect id="t-right-body" x="172" y="14" width="62" height="252" rx="14" class="svg-deep" stroke="var(--svg-stroke)" stroke-width="3"/>
            <path d="M 172 32 H 234 M 172 52 H 234 M 172 72 H 234 M 172 92 H 234 M 172 112 H 234 M 172 132 H 234 M 172 152 H 234 M 172 172 H 234 M 172 192 H 234 M 172 212 H 234 M 172 232 H 234 M 172 248 H 234" stroke="var(--svg-stroke)" stroke-width="2" stroke-linecap="round"/>
            <polygon id="t-right-f" points="203,42 184,72 222,72" fill="var(--svg-stroke)"/>
            <polygon id="t-right-r" points="203,238 184,208 222,208" fill="var(--svg-stroke)"/>
          </g>
        </svg>
        <div class="btn-row">
          <button class="aux-btn dead-btn" type="button" disabled>1</button>
          <button class="aux-btn dead-btn" type="button" disabled>2</button>
          <button class="aux-btn dead-btn" type="button" disabled>3</button>
          <button class="aux-btn dead-btn" type="button" disabled>4</button>
        </div>
      </div>
      <div class="col">
        <span class="lbl">Правая гусеница</span>
        <span class="val" id="val-right">0%</span>
        <div class="slider-v-container"><input type="range" id="slider-right" class="slider-v" min="-100" max="100" value="0"></div>
      </div>
    </div>
  </main>

  <main id="classic-panel" class="panel hidden">
    <div class="cluster">
      <div class="col">
        <span class="lbl">Газ (A)</span>
        <span class="val g" id="val-drive">0%</span>
        <div class="slider-v-container"><input type="range" id="slider-drive" class="slider-v" min="-100" max="100" value="0"></div>
      </div>
      <div class="col">
        <div class="car-wrap">
          <div class="sparks" id="sparks" style="position:absolute;inset:0">
            <i class="spark" style="left:30%;top:40%;--sx:-18px;--sy:-22px;animation-delay:0s"></i>
            <i class="spark" style="left:70%;top:35%;--sx:16px;--sy:-20px;animation-delay:.15s"></i>
            <i class="spark" style="left:45%;top:55%;--sx:-12px;--sy:18px;animation-delay:.3s"></i>
            <i class="spark" style="left:60%;top:50%;--sx:14px;--sy:16px;animation-delay:.45s"></i>
            <i class="spark" style="left:35%;top:28%;--sx:-8px;--sy:-24px;animation-delay:.6s"></i>
            <i class="spark" style="left:55%;top:70%;--sx:10px;--sy:20px;animation-delay:.75s"></i>
          </div>
          <svg class="car-svg" viewBox="0 0 240 200">
            <rect id="car-body" x="70" y="20" width="100" height="160" rx="18" class="svg-body" stroke-width="3"/>
            <polygon id="c-fwd" points="120,38 105,55 135,55" fill="var(--svg-stroke)"/>
            <polygon id="c-rev" points="120,162 105,145 135,145" fill="var(--svg-stroke)"/>
            <g id="motors-icon" opacity="0">
              <g id="motor-b-group" opacity="1">
                <polygon id="b-al" points="76,72 90,63 90,81" fill="var(--svg-stroke)"/>
                <rect x="104" y="58" width="32" height="28" rx="7" class="svg-deep" stroke-width="2"/>
                <polygon id="b-bolt" points="121,62 114,74 120,74 117,82 127,69 121,69" fill="var(--svg-stroke)"/>
                <polygon id="b-ar" points="164,72 150,63 150,81" fill="var(--svg-stroke)"/>
              </g>
              <g id="motor-c-group">
                <polygon id="c-al" points="76,128 90,119 90,137" fill="var(--svg-stroke)"/>
                <rect x="104" y="114" width="32" height="28" rx="7" class="svg-deep" stroke-width="2"/>
                <polygon id="c-bolt" points="121,118 114,130 120,130 117,138 127,125 121,125" fill="var(--svg-stroke)"/>
                <polygon id="c-ar" points="164,128 150,119 150,137" fill="var(--svg-stroke)"/>
              </g>
            </g>
            <circle id="hl-fl" cx="88" cy="32" r="5" fill="#3a4252"/>
            <circle id="hl-fr" cx="152" cy="32" r="5" fill="#3a4252"/>
            <circle id="hl-rl" cx="88" cy="168" r="5" fill="#3a4252"/>
            <circle id="hl-rr" cx="152" cy="168" r="5" fill="#3a4252"/>
            <g id="c-fl" class="wheel" transform="translate(36,45)"><rect id="w-fl" x="-11" y="-20" width="22" height="40" rx="6" class="svg-deep" stroke="var(--blue)" stroke-width="2"/></g>
            <g id="c-fr" class="wheel" transform="translate(204,45)"><rect id="w-fr" x="-11" y="-20" width="22" height="40" rx="6" class="svg-deep" stroke="var(--blue)" stroke-width="2"/></g>
            <g id="c-rl" transform="translate(36,155)"><rect id="w-rl" x="-11" y="-22" width="22" height="45" rx="6" class="svg-deep" stroke="var(--svg-stroke)" stroke-width="2"/></g>
            <g id="c-rr" transform="translate(204,155)"><rect id="w-rr" x="-11" y="-22" width="22" height="45" rx="6" class="svg-deep" stroke="var(--svg-stroke)" stroke-width="2"/></g>
          </svg>
        </div>
        <div class="btn-row">
          <button class="aux-btn led-btn" data-ch="F" type="button" title="Передние фары"><span class="ico">💡</span></button>
          <button class="aux-btn led-btn" data-ch="R" type="button" title="Задние фонари"><span class="ico">🛑</span></button>
          <button class="aux-btn eng-btn" id="eng-btn" type="button" title="Имитация ДВС → мотор B"><span class="ico">⚙️</span></button>
          <button class="aux-btn aux-mode-btn" id="aux-mode-btn" type="button" title="Доп. моторы B/C"><span class="ico">🎛️</span></button>
        </div>
      </div>
      <div class="side">
        <div class="side-stack">
          <div class="dash" id="dash">
            <div class="gauge">
              <svg viewBox="0 0 120 100">
                <path d="M18 78 A42 42 0 0 1 102 78" fill="none" stroke="var(--border)" stroke-width="8" stroke-linecap="round"/>
                <path id="rpm-arc" d="M18 78 A42 42 0 0 1 102 78" fill="none" stroke="#f59e0b" stroke-width="8" stroke-linecap="round" stroke-dasharray="0 200"/>
                <g id="rpm-ticks"></g>
                <line id="rpm-needle" x1="60" y1="78" x2="60" y2="40" stroke="var(--text)" stroke-width="2.5" stroke-linecap="round"/>
                <circle cx="60" cy="78" r="4" fill="#f59e0b"/>
                <text x="60" y="94" text-anchor="middle" fill="var(--muted)" font-size="9" font-weight="700">×1000</text>
              </svg>
            </div>
            <div class="gauge">
              <svg viewBox="0 0 120 100">
                <path d="M18 78 A42 42 0 0 1 102 78" fill="none" stroke="var(--border)" stroke-width="8" stroke-linecap="round"/>
                <path id="spd-arc" d="M18 78 A42 42 0 0 1 102 78" fill="none" stroke="#22d3ee" stroke-width="8" stroke-linecap="round" stroke-dasharray="0 200"/>
                <line id="spd-needle" x1="60" y1="78" x2="60" y2="40" stroke="var(--text)" stroke-width="2.5" stroke-linecap="round"/>
                <circle cx="60" cy="78" r="4" fill="#22d3ee"/>
                <text x="60" y="94" text-anchor="middle" fill="var(--muted)" font-size="9" font-weight="700">м/с</text>
              </svg>
            </div>
          </div>
          <div class="side-top" id="aux-panel">
            <div class="field" id="field-B">
              <div class="row"><label>Мотор B</label><span class="val c" id="val-aux">0%</span></div>
              <div class="slim-container">
                <div class="slim-wrap" id="wrap-aux">
                  <input type="range" class="slim" id="slider-aux" min="-100" max="100" value="0" style="width:100%">
                </div>
                <div class="eng-label" id="eng-label">Имитация ДВС</div>
              </div>
            </div>
            <div class="field" id="field-C">
              <div class="row"><label>Мотор C</label><span class="val a" id="val-C">0%</span></div>
              <div class="slim-wrap">
                <input type="range" class="slim" id="slider-C" min="-100" max="100" value="0" style="width:100%">
              </div>
            </div>
          </div>
        </div>
        <div class="side-bottom">
          <div class="row" style="display:flex;justify-content:space-between;margin-bottom:4px">
            <span class="lbl">Руль (Servo)</span><span class="val" id="val-steer">0%</span>
          </div>
          <div class="slider-h-container">
            <input type="range" id="slider-steer" min="-100" max="100" value="0">
          </div>
        </div>
      </div>
    </div>
  </main>

  <main id="test-panel" class="panel hidden">
    <div class="cluster">
      <div class="motors-group">
        <div class="col">
          <span class="lbl">Мотор A</span>
          <span class="val g" id="val-test-A">0%</span>
          <div class="slider-v-container"><input type="range" id="slider-test-A" class="slider-v" min="-100" max="100" value="0"></div>
        </div>
        <div class="col">
          <span class="lbl">Мотор B</span>
          <span class="val c" id="val-test-B">0%</span>
          <div class="slider-v-container"><input type="range" id="slider-test-B" class="slider-v" min="-100" max="100" value="0"></div>
        </div>
        <div class="col">
          <span class="lbl">Мотор C</span>
          <span class="val a" id="val-test-C">0%</span>
          <div class="slider-v-container"><input type="range" id="slider-test-C" class="slider-v" min="-100" max="100" value="0"></div>
        </div>
      </div>
      <div class="servo-group">
        <div class="btn-row" style="margin-top:0; justify-content:center; gap:12px">
          <button class="aux-btn led-btn" data-ch="F" type="button" title="Передние фары"><span class="ico">💡</span></button>
          <button class="aux-btn led-btn" data-ch="R" type="button" title="Задние фонари"><span class="ico">🛑</span></button>
          <button class="aux-btn reset-btn" id="test-reset-btn" type="button" title="Сбросить всё в 0"><span class="ico">🔄</span></button>
        </div>
        <div style="width:100%">
          <div class="row" style="display:flex;justify-content:space-between;margin-bottom:4px">
            <span class="lbl">Серво (Servo)</span><span class="val" id="val-test-S">0%</span>
          </div>
          <div class="slider-h-container">
            <input type="range" id="slider-test-S" min="-100" max="100" value="0">
          </div>
        </div>
      </div>
    </div>
  </main>

  <div class="footer">
    <span>Монитор порта (TX):</span>
    <code id="telemetry">{"ch":"—","val":0}</code>
  </div>
</div>

<div class="sbg" id="sbg"></div>
<aside class="sheet" id="sheet">
  <div class="sheet-h">
    <b style="font-size:13px">Настройки</b>
    <button class="icon-btn" type="button" id="sheetX">✕</button>
  </div>
  <div class="sheet-b" id="sheetBody">
    <div class="block">
      <h3>Виброотклик (Haptic)</h3>
      <div class="line" style="flex-direction:column; align-items:stretch; gap:8px;">
        <select id="haptic-select" onchange="setHapticMode(this.value)">
          <option value="all">Вся вибрация (кнопки + джойстики)</option>
          <option value="buttons">Только кнопки</option>
          <option value="none">Выключена</option>
        </select>
      </div>
    </div>
    <div class="block">
      <h3>Удержание (Cruise Control)</h3>
      <div class="line"><span>Мотор A</span><span class="grow"></span><button class="hold-btn" data-ch="A" type="button">ВЫКЛ</button></div>
      <div class="line"><span>Мотор B</span><span class="grow"></span><button class="hold-btn" data-ch="B" type="button">ВЫКЛ</button></div>
      <div class="line"><span>Мотор C</span><span class="grow"></span><button class="hold-btn" data-ch="C" type="button">ВЫКЛ</button></div>
    </div>
    <div class="block">
      <h3>Калибровка руля</h3>
      <div class="line">
        <span>Центр</span><span class="grow"></span>
        <button class="mini-btn" type="button" id="trimM">−</button>
        <span class="val" id="val-trim">0</span>
        <button class="mini-btn" type="button" id="trimP">+</button>
      </div>
      <div class="line">
        <span>Макс.°</span><span class="grow"></span>
        <input class="num" id="maxdeg-input" type="number" min="5" max="90" value="45">
      </div>
    </div>
    <div class="block">
      <h3>Реверс моторов</h3>
      <div class="line"><span>Канал A</span><span class="grow"></span><button class="rev-btn" data-ch="A" type="button">⇄</button></div>
      <div class="line"><span>Канал B</span><span class="grow"></span><button class="rev-btn" data-ch="B" type="button">⇄</button></div>
      <div class="line"><span>Канал C</span><span class="grow"></span><button class="rev-btn" data-ch="C" type="button">⇄</button></div>
    </div>
    <div class="block">
      <h3>Кнопки</h3>
      <div style="font-size:13px;color:var(--text);line-height:1.85">
        <div>💡 <b style="color:#eab308">Свет</b></div>
        <div>🛑 <b style="color:#f43f5e">Стоп</b></div>
        <div>⚙️ <b style="color:var(--amber)">Имитация ДВС</b></div>
        <div>🎛️ <b style="color:var(--cyan)">Доп. моторы</b></div>
      </div>
    </div>
  </div>
</aside>

<script>
(function(){
  function $(id){ return document.getElementById(id); }
  var ws = null;

  // --- Единый источник правды для heartbeat: последние отправленные значения A/B/C/S ---
  var lastSent = {A:0, B:0, C:0, S:0};

  function txWS(obj){
    $('telemetry').textContent = JSON.stringify(obj);
    if (obj.ch && lastSent.hasOwnProperty(obj.ch)) lastSent[obj.ch] = obj.val;
    if(ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
  }

  var mode=0, rev={A:0,B:0,C:0}, led={F:0,R:0};
  var eng=false, auxMode=false, trim=0;
  var drive=0, auxB=0, auxC=0, steer=0;
  
  var holdConfig = { A: false, B: false, C: false };
  var holdActive = { A: false, B: false, C: false };
  var holdTimer = { A: null, B: null, C: null };
  var holdStartVal = { A: 0, B: 0, C: 0 };

  var hapticMode = localStorage.getItem('lcc_haptic') || 'all';
  if($('haptic-select')) $('haptic-select').value = hapticMode;

  function vibrate(pattern) {
    if (hapticMode === 'none') return;
    if (navigator.vibrate) navigator.vibrate(pattern);
  }
  function vibrateClick() { vibrate(15); }
  function vibrateEdge() { vibrate(30); }
  function vibrateCruise() { vibrate([40, 30, 40]); }

  function setHapticMode(val) {
    hapticMode = val;
    localStorage.setItem('lcc_haptic', val);
    vibrateClick();
  }

  function initWS(){
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onopen = function(){ $('ws-status').textContent='ESP32'; $('ws-status').style.color='var(--emerald)'; };
    ws.onclose = function(){ 
      $('ws-status').textContent='НЕТ СВЯЗИ'; 
      $('ws-status').style.color='var(--rose)'; 
      updateWifi(0);
      setTimeout(initWS,1000); 
    };
    ws.onmessage = function(e){
      try{
        var d = JSON.parse(e.data);
        if(d.st){
          $('batt-txt').textContent = d.p + '% · ' + d.v.toFixed(1) + 'V';
          $('bat-fill').style.width = d.p + '%';
          updateWifi(d.r);
          
          if(typeof d.ra !== 'undefined') { rev.A = !!d.ra; updateRevUI('A'); }
          if(typeof d.rb !== 'undefined') { rev.B = !!d.rb; updateRevUI('B'); }
          if(typeof d.rc !== 'undefined') { rev.C = !!d.rc; updateRevUI('C'); }
          if(typeof d.lf !== 'undefined') { led.F = !!d.lf; updateLedUI('F'); }
          if(typeof d.lr !== 'undefined') { led.R = !!d.lr; updateLedUI('R'); }
          if(typeof d.es !== 'undefined') { eng = !!d.es; updateEngUI(); applyEngGate(); }
          if(typeof d.tr !== 'undefined') { trim = d.tr; $('val-trim').textContent = trim; }
          if(typeof d.md !== 'undefined') { $('maxdeg-input').value = d.md; }
        }
      }catch(err){}
    };
  }

  // Heartbeat: раз в 150мс переотправляем ВСЕ 4 канала из lastSent, без привязки к режиму.
  // Прошивка сама решит, что из этого актуально для текущего режима — лишний пакет по
  // неиспользуемому каналу безвреден. Это чинит Cruise Control и Тестовый режим разом.
  setInterval(function(){
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    ['A','B','C','S'].forEach(function(ch){
      ws.send(JSON.stringify({ch:ch, val:lastSent[ch]}));
    });
  }, 150);

  function updateWifi(r){
    var n = 0, txt = '--';
    if (r < 0){
      n = r > -55 ? 4 : (r > -65 ? 3 : (r > -75 ? 2 : 1));
      txt = r + ' dBm';
    }
    var bars = document.getElementById('wifi-bars');
    if(bars){
      bars.className = 'bars w' + n;
      for (var i = 0; i < 4; i++) bars.children[i].classList.toggle('on', i < n);
    }
    var wtxt = document.getElementById('wifi-txt');
    if(wtxt) wtxt.textContent = txt;
  }

  function updateRevUI(ch){
    document.querySelectorAll('.rev-btn[data-ch="'+ch+'"]').forEach(function(b){
      b.classList.toggle('active', rev[ch]);
    });
  }

  function updateLedUI(ch){
    document.querySelectorAll('.led-btn[data-ch="'+ch+'"]').forEach(function(b){
      b.classList.toggle('active', led[ch]);
    });
    paintLights();
  }

  var th=localStorage.getItem('lcc_th')||'dark';
  function setTh(t){
    th=t; document.documentElement.setAttribute('data-theme', t==='light'?'light':'dark');
    $('themeBtn').textContent=t==='light'?'☀️':'🌙';
    localStorage.setItem('lcc_th',t);
  }
  setTh(th);
  $('themeBtn').onclick=function(){ setTh(th==='dark'?'light':'dark'); };

  window.toggleFullscreen=async function(){
    try{
      if(!document.fullscreenElement){
        await document.documentElement.requestFullscreen();
        if(screen.orientation&&screen.orientation.lock) await screen.orientation.lock('landscape').catch(function(){});
      } else document.exitFullscreen();
    }catch(e){}
  };

  function fitSliders(){
    var p=document.querySelector('.panel:not(.hidden)');
    if(!p) return;
    document.documentElement.style.setProperty('--sh', Math.max(100, Math.min(220, p.clientHeight-88))+'px');
  }
  window.addEventListener('resize', fitSliders);
  window.addEventListener('load', fitSliders);

  function switchMode(m){
    vibrateClick();
    mode=m;
    $('btn-tank').classList.toggle('active', m===0);
    $('btn-classic').classList.toggle('active', m===1);
    $('btn-test').classList.toggle('active', m===2);
    
    $('tank-panel').classList.toggle('hidden', m!==0);
    $('classic-panel').classList.toggle('hidden', m!==1);
    $('test-panel').classList.toggle('hidden', m!==2);

    if(m===0){ eng=false; updateEngUI(); applyEngGate(); }
    txWS({mode: m});
    zeroAll(); setTimeout(fitSliders,30);
  }
  $('btn-tank').onclick=function(){ switchMode(0); };
  $('btn-classic').onclick=function(){ switchMode(1); };
  $('btn-test').onclick=function(){ switchMode(2); };

  function zeroAll(){
    drive=auxB=auxC=steer=0;
    ['A','B','C'].forEach(function(ch){ clearHoldState(ch); });
    ['slider-left','slider-right','slider-drive','slider-aux','slider-C','slider-steer',
     'slider-test-A','slider-test-B','slider-test-C','slider-test-S'].forEach(function(id){ var e=$(id); if(e) e.value=0; });
    ['val-left','val-right','val-drive','val-aux','val-C','val-steer',
     'val-test-A','val-test-B','val-test-C','val-test-S'].forEach(function(id){ var e=$(id); if(e) e.textContent='0%'; });

    // Фары намеренно не трогаем — привязаны к физическому выходу, не к режиму.
    txWS({ch:'A', val:0}); txWS({ch:'B', val:0}); txWS({ch:'C', val:0}); txWS({ch:'S', val:0});
    paintTank(0,0); paintCar();
  }

  function toggleLightState(el, active){
    if(!el) return;
    el.classList.toggle('active-light', active);
  }

  function paintTank(L,R){
    var green='#10b981', red='#f43f5e', off='var(--svg-stroke)';
    var lf=$('t-left-f'),lr=$('t-left-r'),rf=$('t-right-f'),rr=$('t-right-r');
    if(lf){ 
      toggleLightState(lf, L>8);
      toggleLightState(lr, L<-8);
      lf.setAttribute('fill', L>8 ? green : off); 
      lr.setAttribute('fill', L<-8 ? red : off); 
    }
    if(rf){ 
      toggleLightState(rf, R>8);
      toggleLightState(rr, R<-8);
      rf.setAttribute('fill', R>8 ? green : off); 
      rr.setAttribute('fill', R<-8 ? red : off); 
    }
    var lb=$('t-left-body'),rb=$('t-right-body');
    if(lb){
      toggleLightState(lb, Math.abs(L)>8);
      lb.setAttribute('stroke', L>8 ? green : (L<-8 ? red : 'var(--svg-stroke)'));
      lb.style.filter = L>8 ? 'drop-shadow(0 0 6px #10b981)' : (L<-8 ? 'drop-shadow(0 0 6px #f43f5e)' : '');
    }
    if(rb){
      toggleLightState(rb, Math.abs(R)>8);
      rb.setAttribute('stroke', R>8 ? green : (R<-8 ? red : 'var(--svg-stroke)'));
      rb.style.filter = R>8 ? 'drop-shadow(0 0 6px #10b981)' : (R<-8 ? 'drop-shadow(0 0 6px #f43f5e)' : '');
    }
  }

  function paintCar(){
    var A=drive, S=steer, B=auxB, C=auxC;
    var fwd=$('c-fwd'), revp=$('c-rev');
    if(fwd){
      toggleLightState(fwd, A>8);
      fwd.setAttribute('fill', A>8?'var(--blue)':'var(--svg-stroke)');
    }
    if(revp){
      toggleLightState(revp, A<-8);
      revp.setAttribute('fill', A<-8?'var(--blue)':'var(--svg-stroke)');
    }

    var body=$('car-body');
    if(body){
      body.classList.toggle('eng-glow', eng);
      if(eng){
        body.setAttribute('stroke', '#f59e0b');
        body.setAttribute('stroke-width', '4');
        body.style.filter = 'drop-shadow(0 0 8px rgba(245,158,11,.7))';
      } else {
        body.setAttribute('stroke', 'var(--svg-stroke)');
        body.setAttribute('stroke-width', '3');
        body.style.filter = '';
      }
    }

    var ang = S * 0.28;
    var fl=$('c-fl'), fr=$('c-fr');
    if(fl) fl.setAttribute('transform', 'translate(36,45) rotate('+ang+')');
    if(fr) fr.setAttribute('transform', 'translate(204,45) rotate('+ang+')');
    var wfl=$('w-fl'), wfr=$('w-fr');
    var steerOn = Math.abs(S)>5;
    if(wfl && wfr){
      wfl.setAttribute('stroke', steerOn ? '#3b82f6' : 'var(--svg-stroke)');
      wfl.style.filter = steerOn ? 'drop-shadow(0 0 6px #3b82f6)' : '';
      wfr.setAttribute('stroke', steerOn ? '#3b82f6' : 'var(--svg-stroke)');
      wfr.style.filter = steerOn ? 'drop-shadow(0 0 6px #3b82f6)' : '';
    }

    var wrl=$('w-rl'), wrr=$('w-rr');
    if(wrl && wrr){
      toggleLightState(wrl, Math.abs(A)>8);
      toggleLightState(wrr, Math.abs(A)>8);
      var col = 'var(--svg-stroke)', filt = '';
      if(A>8){ col='#10b981'; filt='drop-shadow(0 0 6px #10b981)'; }
      else if(A<-8){ col='#f43f5e'; filt='drop-shadow(0 0 6px #f43f5e)'; }
      wrl.setAttribute('stroke', col); wrl.style.filter=filt;
      wrr.setAttribute('stroke', col); wrr.style.filter=filt;
    }

    var icon=$('motors-icon');
    if(icon) icon.setAttribute('opacity', auxMode ? '1' : '0');
    if(auxMode){
      var bGroup = document.getElementById('motor-b-group');
      var cGroup = document.getElementById('motor-c-group');
      
      if(bGroup) bGroup.setAttribute('opacity', eng ? '0' : '1');
      if(cGroup) cGroup.classList.toggle('centered', eng);

      if(!eng) setMotorViz('b', B);
      setMotorViz('c', C);
    }
  }

  function setMotorViz(prefix, val){
    var al=$(prefix+'-al'), ar=$(prefix+'-ar'), bolt=$(prefix+'-bolt');
    if(!al) return;
    al.setAttribute('fill', val<-8?'#22d3ee':'var(--svg-stroke)');
    ar.setAttribute('fill', val>8?'#22d3ee':'var(--svg-stroke)');
    if(bolt){
      bolt.setAttribute('fill', Math.abs(val)>8?'#22d3ee':'var(--svg-stroke)');
      bolt.setAttribute('class', Math.abs(val)>8?'bolt-glow bolt-pulse':'');
    }
  }

  function paintLights(){
    var yOn='#eab308', rOn='#f43f5e', off='#3a4252';
    var fl=$('hl-fl'),fr=$('hl-fr'),rl=$('hl-rl'),rr=$('hl-rr');
    if(fl){
      fl.setAttribute('fill', led.F?yOn:off); fr.setAttribute('fill', led.F?yOn:off);
      fl.style.filter = fr.style.filter = led.F?'drop-shadow(0 0 5px #eab308)':'';
    }
    if(rl){
      rl.setAttribute('fill', led.R?rOn:off); rr.setAttribute('fill', led.R?rOn:off);
      rl.style.filter = rr.style.filter = led.R?'drop-shadow(0 0 5px #f43f5e)':'';
    }
  }

  function engineBFromDrive(){ return Math.round(33+67*Math.abs(drive)/100); }

  function updateEngUI(){
    var b=$('eng-btn'); if(b) b.classList.toggle('active', eng);
    var s=$('sparks'); if(s) s.classList.toggle('on', eng);
    paintCar();
  }
  
  function applyEngGate(){
    var slB=$('slider-aux'), wrap=$('wrap-aux'), lab=$('eng-label');
    if(!slB || !wrap || !lab) return;
    
    if(eng){
      wrap.classList.add('hidden');
      lab.classList.add('visible');
      auxB=engineBFromDrive();
      slB.value=auxB; $('val-aux').textContent=auxB+'%';
    } else {
      wrap.classList.remove('hidden');
      lab.classList.remove('visible');
      slB.disabled=false;
      auxB=0; slB.value=0; $('val-aux').textContent='0%';
    }
    paintCar();
  }
  
  function updateAuxModeUI(){
    var b=$('aux-mode-btn'); if(b) b.classList.toggle('active', auxMode);
    var pan=$('aux-panel');
    if(!pan) return;
    var dash=$('dash');
    if(auxMode){
      if(dash) dash.classList.add('hide');
      pan.classList.add('open');
    } else {
      if(dash) dash.classList.remove('hide');
      pan.classList.remove('open');
      if(!eng){ auxB=0; var slB=$('slider-aux'); if(slB) slB.value=0; $('val-aux').textContent='0%'; txWS({ch:'B',val:0}); }
      auxC=0; var slC=$('slider-C'); if(slC) slC.value=0; $('val-C').textContent='0%'; txWS({ch:'C',val:0});
    }
    paintCar();
  }

  $('eng-btn').onclick=function(){ vibrateClick(); eng=!eng; updateEngUI(); applyEngGate(); txWS({eng: eng?1:0}); };
  $('aux-mode-btn').onclick=function(){ vibrateClick(); auxMode=!auxMode; updateAuxModeUI(); applyEngGate(); };

  var springs = {};
  function springTo(el, target, onFrame){
    if(!el) return;
    if(springs[el.id]) cancelAnimationFrame(springs[el.id]);
    var from = +el.value;
    var t0 = performance.now();
    var dur = 220;
    function step(now){
      var k = Math.min(1, (now-t0)/dur);
      var e = 1 - Math.pow(1-k, 3);
      var v = Math.round(from + (target-from)*e);
      el.value = v;
      onFrame(v, k>=1);
      if(k<1) springs[el.id] = requestAnimationFrame(step);
      else delete springs[el.id];
    }
    springs[el.id] = requestAnimationFrame(step);
  }
  function cancelSpring(el){
    if(el && springs[el.id]){ cancelAnimationFrame(springs[el.id]); delete springs[el.id]; }
  }

  function clearHoldState(ch){
    if(holdTimer[ch]){ clearTimeout(holdTimer[ch]); holdTimer[ch] = null; }
    holdActive[ch] = false;
  }

  function handleInputHold(ch, val){
    if(!holdConfig[ch]) return;
    if(holdActive[ch]){
      holdActive[ch] = false;
    }
    if(Math.abs(val) > 50){
      if(!holdTimer[ch] || Math.abs(val - holdStartVal[ch]) > 5){
        if(holdTimer[ch]) clearTimeout(holdTimer[ch]);
        holdStartVal[ch] = val;
        holdTimer[ch] = setTimeout(function(){
          holdActive[ch] = true;
          holdTimer[ch] = null;
        }, 1500);
      }
    } else {
      if(holdTimer[ch]){ clearTimeout(holdTimer[ch]); holdTimer[ch] = null; }
    }
  }

  function bindSpring(el, ch, onFrame){
    function start(){ 
      cancelSpring(el); 
      if(ch) clearHoldState(ch);
    }
    el.addEventListener('mousedown', start);
    el.addEventListener('touchstart', start, {passive:true});
    function release(){
      if(el.disabled || mode===2) return;
      var target = (ch && holdActive[ch]) ? +el.value : 0;
      springTo(el, target, onFrame);
    }
    el.addEventListener('mouseup', release);
    el.addEventListener('touchend', release);
    el.addEventListener('touchcancel', release);
  }

  $('slider-left').oninput=function(){
    cancelSpring(this);
    var v=+this.value; 
    handleInputHold('A', v);
    $('val-left').textContent=v+'%'; txWS({ch:'A',val:v}); 
    paintTank(v,+$('slider-right').value);
    if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-left'), 'A', function(v){
    $('val-left').textContent=v+'%'; txWS({ch:'A',val:v}); 
    paintTank(v,+$('slider-right').value);
  });

  $('slider-right').oninput=function(){
    cancelSpring(this);
    var v=+this.value; 
    if(mode===0) handleInputHold('B', v);
    $('val-right').textContent=v+'%'; txWS({ch:'B',val:v}); 
    paintTank(+$('slider-left').value,v);
    if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-right'), 'B', function(v){
    $('val-right').textContent=v+'%'; txWS({ch:'B',val:v}); 
    paintTank(+$('slider-left').value,v);
  });

  $('slider-drive').oninput=function(){
    cancelSpring(this);
    drive=+this.value; 
    handleInputHold('A', drive);
    $('val-drive').textContent=drive+'%'; txWS({ch:'A',val:drive});
    if(eng){ auxB=engineBFromDrive(); $('slider-aux').value=auxB; $('val-aux').textContent=auxB+'%'; }
    paintCar();
    if ((drive === 100 || drive === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-drive'), 'A', function(v){
    drive=v; $('val-drive').textContent=v+'%'; txWS({ch:'A',val:v});
    if(eng){ auxB=engineBFromDrive(); $('slider-aux').value=auxB; $('val-aux').textContent=auxB+'%'; }
    paintCar();
  });

  $('slider-aux').oninput=function(){
    if(eng) return;
    cancelSpring(this);
    auxB=+this.value; 
    handleInputHold('B', auxB);
    $('val-aux').textContent=auxB+'%'; txWS({ch:'B',val:auxB}); 
    paintCar();
    if ((auxB === 100 || auxB === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-aux'), 'B', function(v){
    if(eng) return;
    auxB=v; $('val-aux').textContent=v+'%'; txWS({ch:'B',val:v}); 
    paintCar();
  });

  $('slider-C').oninput=function(){
    cancelSpring(this);
    auxC=+this.value; 
    handleInputHold('C', auxC);
    $('val-C').textContent=auxC+'%'; txWS({ch:'C',val:auxC}); 
    paintCar();
    if ((auxC === 100 || auxC === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-C'), 'C', function(v){
    auxC=v; $('val-C').textContent=v+'%'; txWS({ch:'C',val:v}); 
    paintCar();
  });

  $('slider-steer').oninput=function(){
    cancelSpring(this);
    steer=+this.value; $('val-steer').textContent=steer+'%'; txWS({ch:'S',val:steer}); paintCar();
    if ((steer === 100 || steer === -100) && hapticMode === 'all') vibrateEdge();
  };
  bindSpring($('slider-steer'), null, function(v){
    steer=v; $('val-steer').textContent=v+'%'; txWS({ch:'S',val:v}); paintCar();
  });

  $('slider-test-A').oninput=function(){ var v=+this.value; $('val-test-A').textContent=v+'%'; txWS({ch:'A',val:v}); if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge(); };
  $('slider-test-B').oninput=function(){ var v=+this.value; $('val-test-B').textContent=v+'%'; txWS({ch:'B',val:v}); if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge(); };
  $('slider-test-C').oninput=function(){ var v=+this.value; $('val-test-C').textContent=v+'%'; txWS({ch:'C',val:v}); if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge(); };
  $('slider-test-S').oninput=function(){ var v=+this.value; $('val-test-S').textContent=v+'%'; txWS({ch:'S',val:v}); if ((v === 100 || v === -100) && hapticMode === 'all') vibrateEdge(); };
  
  $('test-reset-btn').onclick=function(){ vibrateClick(); zeroAll(); };

  document.querySelectorAll('.rev-btn').forEach(function(b){
    b.addEventListener('click', function(){
      vibrateClick();
      var ch=b.getAttribute('data-ch'); if(!ch) return;
      rev[ch]=!rev[ch];
      updateRevUI(ch);
      var obj = {}; obj['rev' + ch] = rev[ch] ? 1 : 0;
      txWS(obj);
    });
  });

  document.querySelectorAll('.hold-btn').forEach(function(b){
    b.addEventListener('click', function(){
      vibrateClick();
      var ch=b.getAttribute('data-ch'); if(!ch) return;
      holdConfig[ch] = !holdConfig[ch];
      clearHoldState(ch);
      b.classList.toggle('active', holdConfig[ch]);
      b.textContent = holdConfig[ch] ? 'ВКЛ' : 'ВЫКЛ';
      if (holdConfig[ch]) vibrateCruise();
    });
  });

  document.querySelectorAll('.led-btn').forEach(function(b){
    b.addEventListener('click', function(){
      vibrateClick();
      var c=b.getAttribute('data-ch');
      led[c]=!led[c];
      updateLedUI(c);
      var obj = {}; obj['led' + c] = led[c] ? 1 : 0;
      txWS(obj);
    });
  });

  function openS(v){ 
    vibrateClick();
    $('sheet').classList.toggle('open',v); 
    $('sbg').classList.toggle('open',v); 
  }
  $('setBtn').onclick=function(){ openS(true); };
  $('sheetX').onclick=function(){ openS(false); };
  $('sbg').onclick=function(){ openS(false); };
  $('sheetBody').addEventListener('touchmove', function(e){ e.stopPropagation(); }, {passive:true});

  $('trimM').onclick=function(){ vibrateClick(); trim=Math.max(-400,trim-50); $('val-trim').textContent=trim; txWS({trim:trim}); };
  $('trimP').onclick=function(){ vibrateClick(); trim=Math.min(400,trim+50); $('val-trim').textContent=trim; txWS({trim:trim}); };
  $('maxdeg-input').onchange=function(){ vibrateClick(); txWS({maxdeg: +this.value||45}); };

  var speedMs = 0, rpmShown = 0, moveDir = 0;
  function setNeedle(id, t){
    var el=$(id); if(!el) return;
    var ang = -90 + Math.max(0, Math.min(1, t))*180;
    el.setAttribute('transform', 'rotate('+ang+' 60 78)');
  }
  function setArc(id, t){
    var el=$(id); if(!el) return;
    var len = 132;
    var v = Math.max(0, Math.min(1, t))*len;
    el.setAttribute('stroke-dasharray', v+' '+(200-v));
  }
  setInterval(function(){
    var dir = drive>6 ? 1 : (drive<-6 ? -1 : 0);
    var braking = (dir !== 0 && moveDir !== 0 && dir !== moveDir) || (dir === 0 && (speedMs>0.15 || rpmShown>40));
    if(braking){
      speedMs += (0 - speedMs) * 0.22;
      rpmShown += (0 - rpmShown) * 0.22;
      if(speedMs < 0.2 && rpmShown < 30){ speedMs = 0; rpmShown = 0; moveDir = dir; }
    } else {
      if(dir) moveDir = dir;
      var targetRpm = eng ? (980 + (6000-980)*(Math.abs(drive)/100)) : 0;
      rpmShown += (targetRpm - rpmShown) * 0.045;
      var targetSpd = (Math.abs(drive)/100)*10;
      speedMs += (targetSpd - speedMs) * 0.04;
      if(!eng && rpmShown < 8) rpmShown = 0;
      if(targetSpd===0 && speedMs<0.05) speedMs = 0;
    }
    setNeedle('rpm-needle', rpmShown/7000);
    setArc('rpm-arc', rpmShown/7000);
    setNeedle('spd-needle', speedMs/10);
    setArc('spd-arc', speedMs/10);
  }, 50);

  document.addEventListener('touchmove', function(e){
    if(e.target.closest && e.target.closest('.sheet-b')) return;
    if(e.target.tagName!=='INPUT') e.preventDefault();
  }, {passive:false});

  initWS();
  updateEngUI(); applyEngGate(); paintLights();
  switchMode(0); fitSliders();
})();
</script>
</body>
</html>
)HTML";

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);

  pinMode(TB_STBY, OUTPUT); digitalWrite(TB_STBY, LOW);
  pinMode(DRV_EN, OUTPUT); digitalWrite(DRV_EN, HIGH); // DRV8825 активен при LOW

  pinMode(TB_AIN1, OUTPUT); pinMode(TB_AIN2, OUTPUT);
  pinMode(TB_BIN1, OUTPUT); pinMode(TB_BIN2, OUTPUT);
  pinMode(DRV_DIR, OUTPUT);

  ledcSetup(LEDC_CH_A, 5000, 8); ledcAttachPin(TB_PWMA, LEDC_CH_A);
  ledcSetup(LEDC_CH_B, 5000, 8); ledcAttachPin(TB_PWMB, LEDC_CH_B);
  ledcSetup(LEDC_CH_C, 5000, 8); ledcAttachPin(DRV_STEP, LEDC_CH_C);

  pinMode(LED_FRONT_PIN, OUTPUT);
  pinMode(LED_REAR_PIN, OUTPUT);

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
    stopAll();
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
