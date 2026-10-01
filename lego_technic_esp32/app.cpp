// ESP32 Lego Technic motorization — Версия: 0.0.9 "LED Update"
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
 *   Общий GND: батарея/BMS/выключатель/Mini360/TB6612/ESP32/OLED/серва/LED — единая точка (важно для стабильной работы АЦП)
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
 * LED (3-pin разъём: левый/GND/правый):
 *   GPIO32 -> резистор ~150Ω -> анод переднего (зелёная иконка) светодиода -> катод -> GND
 *   GPIO33 -> резистор ~150Ω -> анод заднего (красная иконка) светодиода -> катод -> GND
 *   Резистор ставится в разрыв сигнального провода, не на GND. Подобрать номинал под конкретный
 *   светодиод (см. комментарий в шапке версии 0.0.9) — ориентир ~8-10мА, не выше 15-20мА на пин.
 *
 * Делитель напряжения батареи (для BATT_PIN):
 *   Шина (+) -> R1(100к) -> точка замера -> R2(39к) -> GND ; точка замера -> BATT_PIN
 *
 * GPIO ESP32 (сигнальные, тонкие провода) — см. комментарии на каждой строке ниже.
 */
// Что добавлено в 0.0.9 "LED Update":
// - Управление светом: GPIO32 (передние, зелёная иконка) и GPIO33 (задние, красная иконка).
//   Простое вкл/выкл через кнопки 1 и 2 в GUI (были заглушками), состояние сохраняется в NVS,
//   одинаково в обоих режимах (танк/классика) — привязано к физическому выходу, не к режиму,
//   по той же логике, что и реверс моторов. Кнопки 3/4 остаются заглушками без функций.
// - Иконки лампочек нарисованы прямо на кнопках 1/2: тусклые в выключенном состоянии,
//   яркие с подсветкой во включённом.
// (0.0.8 Cleanup: убран мёртвый CSS; 0.0.7c ещё не протестирован на реальном железе —
//  плата вышла из строя при сборке, новая прошивка проверялась только компиляцией)

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
#define LED_FRONT_PIN 32 // Передние LED (зелёная иконка, кнопка 1) — через резистор на GND
#define LED_REAR_PIN  33 // Задние LED (красная иконка, кнопка 2) — через резистор на GND

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

int motorAVal = 0; // -100..100, как пришло со слайдера (реверс сюда не подмешивается)
int motorBVal = 0;
int servoVal  = 0;

// ---------- Реверс моторов ----------
// Привязан к физическому каналу (A/B), не к режиму: один и тот же мотор в танке и в классике —
// это один и тот же канал, реверс должен быть одинаковым в обоих режимах.
bool reverseA = false;
bool reverseB = false;

void loadMotorRev() {
  reverseA = prefs.getUChar("revA", 0) != 0;
  reverseB = prefs.getUChar("revB", 0) != 0;
}

// ---------- LED (фары) ----------
// Как и реверс — привязаны к физическому выходу, не к режиму.
bool ledFrontOn = false;
bool ledRearOn  = false;

void loadLeds() {
  ledFrontOn = prefs.getUChar("ledF", 0) != 0;
  ledRearOn  = prefs.getUChar("ledR", 0) != 0;
}
void applyLeds() {
  digitalWrite(LED_FRONT_PIN, ledFrontOn ? HIGH : LOW);
  digitalWrite(LED_REAR_PIN, ledRearOn ? HIGH : LOW);
}

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

void applyMotorA(int val) {
  motorAVal = val; // сохраняем "как есть", реверс — только на выходе в железо
  setMotor(TB_AIN1, TB_AIN2, LEDC_CH_A, reverseA ? -val : val);
}
void applyMotorB(int val) {
  motorBVal = val;
  setMotor(TB_BIN1, TB_BIN2, LEDC_CH_B, reverseB ? -val : val);
}

void stopAll() {
  applyMotorA(0);
  applyMotorB(0);
  steerServo.writeMicroseconds(steerCenterUs); // стоп/центр (с учётом калибровки)
  motorAVal = motorBVal = servoVal = 0;
}

// ---------- Батарея и статус ----------
// Пороги предупреждения о разряде: 18650 безопасный минимум ~3.0В/банка, 2 банки последовательно = 6.0В.
// Ниже 0.5В — батарея физически не подключена (стенд на USB), это не разряд, предупреждение не нужно.
#define LOW_BATTERY_THRESHOLD_V 6.0
#define BATTERY_DISCONNECTED_V  0.5
#define REST_SETTLE_MS 400 // сколько мс все каналы должны быть на нуле, прежде чем обновлять заряд (%)

float battV = 0.0;    // напряжение батареи, В — живой замер каждую секунду, всегда актуален
int   battPct = 0;    // заряд, % — обновляется ТОЛЬКО в покое (см. updateStatus)
int   wifiRssi = 0;   // RSSI лучшего клиента точки доступа, dBm; 0 = клиентов нет
int   lowBattCount = 0; // сколько замеров подряд ниже порога (антидребезг предупреждения, по живому В)

bool wasAtRest = true;
unsigned long restStartMillis = 0;
bool firstStatusRun = true;

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

// Плавный процент заряда: линейная интерполяция между точками кривой 2S.
// 6.0В = 0%, ~6.08В = 1%, 6.8В = 10%, 7.0В = 25%, 7.4В = 50%, 7.8В = 75%, 8.2В+ = 100%.
int batteryPercent(float v) {
  static const float PV[] = {6.0, 6.8, 7.0, 7.4, 7.8, 8.2};
  static const float PP[] = {0,   10,  25,  50,  75,  100};
  if (v <= PV[0]) return 0;
  if (v >= PV[5]) return 100;
  for (int i = 0; i < 5; i++) {
    if (v < PV[i + 1]) {
      return (int)(PP[i] + (PP[i + 1] - PP[i]) * (v - PV[i]) / (PV[i + 1] - PV[i]) + 0.5);
    }
  }
  return 100;
}

// RSSI клиента, как его видит сама плата (режим точки доступа). Если клиентов несколько — лучший.
int getBestRssi() {
  wifi_sta_list_t list;
  if (esp_wifi_ap_get_sta_list(&list) != ESP_OK || list.num == 0) return 0;
  int best = -127;
  for (int i = 0; i < list.num; i++) {
    if (list.sta[i].rssi > best) best = list.sta[i].rssi;
  }
  return best;
}

// Раз в секунду: напряжение — всегда, заряд(%) — только в покое, RSSI и антидребезг LOW BATTERY.
void updateStatus() {
  battV = readBatteryVoltage(); // честный вольтметр, живой всегда

  bool atRest = (motorAVal == 0 && motorBVal == 0 && servoVal == 0);
  if (atRest) {
    if (!wasAtRest) { restStartMillis = millis(); wasAtRest = true; }
    if (firstStatusRun || millis() - restStartMillis > REST_SETTLE_MS) {
      battPct = batteryPercent(battV); // заряд обновляем только "отстоявшись" без нагрузки
      firstStatusRun = false;
    }
  } else {
    wasAtRest = false; // под нагрузкой — просто не трогаем battPct, держим последнее спокойное значение
  }

  wifiRssi = getBestRssi();

  // Антидребезг предупреждения — по живому напряжению (это безопасность, должна реагировать быстро,
  // даже под нагрузкой): тревога — после 3 замеров подряд ниже порога, сброс — выше порога +0.3В.
  bool below = (battV > BATTERY_DISCONNECTED_V && battV < LOW_BATTERY_THRESHOLD_V);
  if (below) { if (lowBattCount < 3) lowBattCount++; }
  else if (battV > LOW_BATTERY_THRESHOLD_V + 0.3 || battV <= BATTERY_DISCONNECTED_V) lowBattCount = 0;
}

// Статус для веб-страницы: v — вольты (живой), p — % заряда (только в покое), r — RSSI,
// m — режим, c — WS-клиенты, ra/rb — реверс каналов A/B, lf/lr — состояние LED
void buildStatus(char *buf, size_t n) {
  snprintf(buf, n, "{\"st\":1,\"v\":%.2f,\"p\":%d,\"r\":%d,\"m\":%d,\"c\":%d,\"ra\":%d,\"rb\":%d,\"lf\":%d,\"lr\":%d}",
           battV, battPct, wifiRssi, (int)currentMode, (int)ws.count(),
           reverseA ? 1 : 0, reverseB ? 1 : 0, ledFrontOn ? 1 : 0, ledRearOn ? 1 : 0);
}

// ---------- OLED ----------
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
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

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
    display.printf("%.2fV\n", battV);
    display.display();
    return;
  }

  // Крупно — то, что нужно видеть одним взглядом, не всматриваясь
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(currentMode == MODE_TANK ? "TANK" : "CAR");
  display.setCursor(0, 20);
  display.printf("%d%%\n", battPct);

  // Мелко — техническая информация для отладки
  display.setTextSize(1);
  display.setCursor(0, 44);
  display.printf("%.2fV\n", battV);
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

  // Реверс мотора: {"rev":"A"} или {"rev":"B"} — переключатель, применяется сразу к текущему val
  if (doc.containsKey("rev")) {
    const char* rch = doc["rev"] | "";
    if (strcmp(rch, "A") == 0) {
      reverseA = !reverseA;
      prefs.putUChar("revA", reverseA ? 1 : 0);
      applyMotorA(motorAVal);
    } else if (strcmp(rch, "B") == 0) {
      reverseB = !reverseB;
      prefs.putUChar("revB", reverseB ? 1 : 0);
      applyMotorB(motorBVal);
    }
    return;
  }

  // Свет: {"led":"F"} или {"led":"R"} — переключатель
  if (doc.containsKey("led")) {
    const char* lch = doc["led"] | "";
    if (strcmp(lch, "F") == 0) {
      ledFrontOn = !ledFrontOn;
      prefs.putUChar("ledF", ledFrontOn ? 1 : 0);
    } else if (strcmp(lch, "R") == 0) {
      ledRearOn = !ledRearOn;
      prefs.putUChar("ledR", ledRearOn ? 1 : 0);
    }
    applyLeds();
    return;
  }

  const char* ch = doc["ch"] | "";
  int val = doc["val"] | 0;

  if (currentMode == MODE_TANK) {
    if (strcmp(ch, "A") == 0) applyMotorA(val);
    else if (strcmp(ch, "B") == 0) applyMotorB(val);
  } else { // MODE_CAR
    if (strcmp(ch, "A") == 0) applyMotorA(val);       // drive
    else if (strcmp(ch, "B") == 0) applyMotorB(val);  // accessory
    else if (strcmp(ch, "S") == 0) applySteer(val);   // steer
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // сразу отдаём статус новому клиенту, не дожидаясь секундного таймера
    char buf[144];
    buildStatus(buf, sizeof(buf));
    client->text(buf);
  } else if (type == WS_EVT_DATA) {
    handleWsMessage(data, len);
  }
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
  --blue:#3b82f6; --blue-dk:#2563eb; --emerald:#10b981; --rose:#f43f5e; --amber:#f59e0b; --cyan:#22d3ee;
  --text:#e2e8f0; --muted:#94a3b8;
  --sh:200px;  /* длина вертикальных ползунков — подгоняется скриптом под высоту экрана */
  --T:68px;    /* толщина ползунков газа и руля */
}
*{box-sizing:border-box}
html,body{height:100%;margin:0;overflow:hidden;overscroll-behavior:none;touch-action:none;
  background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
  user-select:none;-webkit-user-select:none}
body{padding:max(env(safe-area-inset-top,0px),6px) 10px max(env(safe-area-inset-bottom,0px),6px);
  display:flex;justify-content:center}
.wrap{width:100%;max-width:1000px;height:100%;display:flex;flex-direction:column;gap:8px}

/* ---- шапка ---- */
.header{flex:none;display:flex;justify-content:space-between;align-items:center;gap:8px;
  background:rgba(20,24,32,.85);border:1px solid var(--border);padding:8px 12px;border-radius:16px;box-shadow:0 4px 20px rgba(0,0,0,.3)}
.brand{display:flex;align-items:center;gap:10px}
.brand-icon{width:32px;height:32px;border-radius:10px;background:rgba(59,130,246,.15);
  border:1px solid rgba(59,130,246,.4);display:flex;align-items:center;justify-content:center;font-size:16px}
.brand-title{font-size:13px;font-weight:700;line-height:1}
.brand-sub{display:flex;align-items:center;gap:6px;font-size:10px;color:var(--muted);margin-top:3px}
.brand-ver{font-size:9px;color:var(--muted);opacity:.7;margin-top:1px}
.dot{width:6px;height:6px;border-radius:50%;background:var(--emerald);animation:pulse 1.5s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.mode-switch{display:flex;gap:4px;background:rgba(2,6,15,.6);border:1px solid var(--border);border-radius:12px;padding:4px}
.mode-btn{border:none;background:transparent;color:var(--muted);font-size:12px;font-weight:600;
  padding:7px 12px;border-radius:9px;cursor:pointer}
.mode-btn.active{background:var(--blue-dk);color:#fff;box-shadow:0 0 14px rgba(59,130,246,.5)}
.badge{font-size:10px;font-weight:700;padding:4px 8px;border-radius:8px;white-space:nowrap;
  background:rgba(16,185,129,.1);color:var(--emerald);border:1px solid rgba(16,185,129,.25)}
.badge.bad{background:rgba(244,63,94,.12);color:var(--rose);border-color:rgba(244,63,94,.3)}
.icon-btn{width:32px;height:32px;border-radius:10px;background:var(--panel2);border:1px solid var(--border);
  color:var(--text);display:flex;align-items:center;justify-content:center;cursor:pointer;font-size:14px}

/* статус в шапке: Wi-Fi и батарея */
.hstats{display:flex;align-items:center;gap:12px;transition:opacity .2s}
.hstats.stale{opacity:.35}
.stat{display:flex;align-items:center;gap:6px;font-size:11px;font-weight:600;white-space:nowrap}
.bars{display:flex;align-items:flex-end;gap:2px;height:16px}
.bars i{display:block;width:4px;border-radius:1px;background:#2a3140}
.bars i:nth-child(1){height:5px}.bars i:nth-child(2){height:9px}
.bars i:nth-child(3){height:13px}.bars i:nth-child(4){height:16px}
.bars.w1 i.on{background:var(--rose)}
.bars.w2 i.on{background:var(--amber)}
.bars.w3 i.on,.bars.w4 i.on{background:var(--emerald)}
.bat{position:relative;width:26px;height:13px;border:2px solid var(--muted);border-radius:3px;padding:1px}
.bat::after{content:"";position:absolute;right:-5px;top:2px;width:3px;height:5px;background:var(--muted);border-radius:0 2px 2px 0}
.bat-fill{height:100%;width:0;background:var(--emerald);border-radius:1px}
.bat.warn .bat-fill{background:var(--amber)}
.bat.crit .bat-fill{background:var(--rose)}

/* баннер о заряде */
.banner{flex:none;display:flex;align-items:center;justify-content:space-between;gap:10px;
  padding:7px 12px;border-radius:12px;font-size:12px;font-weight:600}
.banner.lvl1{background:rgba(245,158,11,.14);border:1px solid rgba(245,158,11,.5);color:#fbbf24}
.banner.lvl2{background:rgba(244,63,94,.16);border:1px solid rgba(244,63,94,.55);color:#fb7185}
.banner-x{font-size:14px;opacity:.8;cursor:pointer}

/* ---- панели ---- */
.panel{flex:1;min-height:0;display:flex;align-items:center;justify-content:center;
  background:rgba(20,24,32,.6);border:1px solid var(--border);border-radius:22px;
  padding:14px;box-shadow:0 20px 40px rgba(0,0,0,.35);overflow:hidden}
.cluster{display:flex;align-items:center;justify-content:center;gap:clamp(24px,7vw,70px)}
.row{display:flex;align-items:center;justify-content:space-between;gap:10px}
.col{display:flex;flex-direction:column;align-items:center;gap:4px}
.lbl{font-size:10px;font-weight:600;letter-spacing:.05em;text-transform:uppercase;color:var(--muted);white-space:nowrap}
.val{font-size:12px;font-weight:700;font-family:monospace;color:var(--blue)}
.val.g{color:var(--emerald)} .val.c{color:var(--cyan)}

/* ползунки */
input[type=range]{-webkit-appearance:none;appearance:none;background:transparent;touch-action:pan-y;margin:0}
input[type=range]::-webkit-slider-runnable-track{height:22px;cursor:pointer;background:#1b2029;border-radius:12px;border:1px solid var(--border)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:32px;height:60px;margin-top:-19px;
  border-radius:12px;background:var(--blue);cursor:pointer;border:2px solid #fff;box-shadow:0 0 14px rgba(59,130,246,.8)}
input.slim::-webkit-slider-runnable-track{height:14px}
input.slim::-webkit-slider-thumb{width:22px;height:34px;margin-top:-11px;border-radius:9px}

.slider-v-container{height:var(--sh);width:var(--T);display:flex;align-items:center;justify-content:center;
  background:rgba(2,6,15,.7);border-radius:20px;border:1px solid var(--border);box-shadow:inset 0 2px 8px rgba(0,0,0,.4)}
.slider-v{flex:none;width:var(--sh) !important;height:var(--T) !important;transform:rotate(-90deg);transform-origin:center}
.slider-h-container{width:100%;height:var(--T);display:flex;align-items:center;padding:0 8px;
  background:rgba(2,6,15,.7);border-radius:20px;border:1px solid var(--border);box-shadow:inset 0 2px 8px rgba(0,0,0,.4)}
.slider-h-container input{width:100%}
.slim-wrap{height:36px;display:flex;align-items:center}
.slim-wrap input{width:100%}

/* иконки и кнопки */
.tank-svg{height:calc(var(--sh) - 24px);width:auto;aspect-ratio:240/280}
.car-svg{height:calc(var(--sh) - 8px);width:auto;aspect-ratio:240/200}
.btn-row{display:flex;gap:8px;margin-top:8px}
.aux-btn{width:46px;height:36px;border-radius:12px;background:var(--panel2);border:1px solid var(--border);
  color:var(--text);font-weight:700;font-size:14px;cursor:pointer;display:flex;align-items:center;justify-content:center}
.aux-btn:active{background:var(--blue-dk)}

/* кнопки света — иконка лампочки, цвет фиксирован (перед/зад), яркость/подсветка зависит от состояния */
.led-btn svg{color:#3a4252;transition:color .15s,filter .15s}
.led-btn[data-ch="F"].active svg{color:#10b981;filter:drop-shadow(0 0 5px #10b981)}
.led-btn[data-ch="R"].active svg{color:#f43f5e;filter:drop-shadow(0 0 5px #f43f5e)}

/* кнопка реверса — привязана к каналу (data-ch), одинаковое состояние во всех режимах */
.rev-btn{margin-top:6px;padding:5px 10px;border-radius:10px;background:var(--panel2);border:1px solid var(--border);
  color:var(--muted);font-size:10px;font-weight:700;letter-spacing:.03em;cursor:pointer;white-space:nowrap}
.rev-btn.active{background:rgba(59,130,246,.18);border-color:var(--blue);color:var(--blue)}

/* правая колонка классики */
.side{width:300px;align-self:stretch;display:flex;flex-direction:column;justify-content:space-between;gap:8px}
.field{background:rgba(2,6,15,.55);padding:8px 10px;border-radius:16px;border:1px solid var(--border)}
.field label{font-size:11px;font-weight:600;color:#cbd5e1}
.field .row{margin-bottom:4px}
.cal{display:flex;align-items:center;gap:6px;margin-top:6px}
.mini-btn{width:30px;height:30px;border-radius:9px;background:var(--panel2);border:1px solid var(--border);
  color:var(--text);font-size:16px;cursor:pointer;padding:0}
.num{width:56px;height:30px;background:#0d0f12;color:#e2e8f0;border:1px solid var(--border);border-radius:8px;padding:4px}

.hidden{display:none !important}
.wheel{transform-box:fill-box;transform-origin:center;transition:transform .08s ease-out}

/* иконка доп. мотора: пульсирующая молния */
@keyframes boltPulse{0%,100%{opacity:.55}50%{opacity:1}}
.bolt-glow{filter:drop-shadow(0 0 5px #22d3ee)}
.bolt-pulse{animation:boltPulse 1s ease-in-out infinite}

.footer{flex:none;background:rgba(20,24,32,.9);border:1px solid var(--border);border-radius:14px;
  padding:6px 12px;display:flex;justify-content:space-between;align-items:center}
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
        <div class="brand-ver">(alpha 0.0.9)</div>
      </div>
    </div>
    <div class="mode-switch">
      <button id="btn-tank" class="mode-btn active" onclick="switchMode(0)">&#128737; Танковый</button>
      <button id="btn-classic" class="mode-btn" onclick="switchMode(1)">&#128663; Классический</button>
    </div>
    <div class="row" style="gap:10px">
      <div class="hstats stale" id="hstats">
        <div class="stat" title="Уровень Wi-Fi">
          <div class="bars w0" id="wifi-bars"><i></i><i></i><i></i><i></i></div>
          <span id="wifi-txt">--</span>
        </div>
        <div class="stat" title="Аккумулятор">
          <div class="bat" id="bat-ic"><div class="bat-fill" id="bat-fill"></div></div>
          <span id="batt-txt">--</span>
        </div>
      </div>
      <button class="icon-btn" onclick="toggleFullscreen()" title="На весь экран">&#9974;</button>
      <span class="badge bad" id="link-badge">Нет связи</span>
    </div>
  </div>

  <div id="batt-banner" class="banner hidden" onclick="dismissBanner()">
    <span id="banner-text"></span>
    <span class="banner-x" id="banner-x">&#10005;</span>
  </div>

  <main id="tank-panel" class="panel">
    <div class="cluster">
      <div class="col">
        <span class="lbl">Левая гусеница</span>
        <span class="val" id="val-left">0%</span>
        <div class="slider-v-container">
          <input type="range" id="slider-left" class="slider-v" min="-100" max="100" value="0"
                 oninput="updateTank('A',this.value)"
                 onmouseup="resetSlider('slider-left','A',updateTank)" ontouchend="resetSlider('slider-left','A',updateTank)">
        </div>
        <button class="rev-btn" data-ch="A" onclick="toggleRev('A')">&#8644; РЕВЕРС</button>
      </div>

      <div class="col">
        <svg class="tank-svg" viewBox="0 0 240 280">
          <defs>
            <pattern id="tread" width="62" height="16" patternUnits="userSpaceOnUse">
              <rect width="62" height="16" fill="#090d16"/>
              <rect y="12" width="62" height="4" fill="#1b2029"/>
            </pattern>
          </defs>
          <rect x="64" y="30" width="112" height="220" rx="20" fill="#1b2029" stroke="#2a3140" stroke-width="3"/>
          <rect x="80" y="52" width="80" height="176" rx="12" fill="#0d0f12" stroke="#1b2029" stroke-width="2"/>
          <rect x="112" y="24" width="16" height="96" rx="5" fill="#2a3140" stroke="#3b82f6" stroke-width="1.5" opacity=".7"/>
          <circle cx="120" cy="150" r="34" fill="#1b2029" stroke="#3b82f6" stroke-width="2.5" opacity=".9"/>
          <g>
            <rect id="t-left-body" x="6" y="14" width="62" height="252" rx="14" fill="url(#tread)" stroke="#2a3140" stroke-width="3"/>
            <polygon id="t-left-f" points="37,50 18,84 56,84" fill="#2a3140"/>
            <polygon id="t-left-r" points="37,230 18,196 56,196" fill="#2a3140"/>
          </g>
          <g>
            <rect id="t-right-body" x="172" y="14" width="62" height="252" rx="14" fill="url(#tread)" stroke="#2a3140" stroke-width="3"/>
            <polygon id="t-right-f" points="203,50 184,84 222,84" fill="#2a3140"/>
            <polygon id="t-right-r" points="203,230 184,196 222,196" fill="#2a3140"/>
          </g>
        </svg>
        <div class="btn-row">
          <button class="aux-btn led-btn" data-ch="F" onclick="toggleLed('F')" title="Передние фары">
            <svg viewBox="0 0 24 24" width="18" height="18"><circle cx="12" cy="10" r="7" fill="currentColor"/><rect x="9" y="17" width="6" height="4" rx="1" fill="currentColor"/></svg>
          </button>
          <button class="aux-btn led-btn" data-ch="R" onclick="toggleLed('R')" title="Задние фонари">
            <svg viewBox="0 0 24 24" width="18" height="18"><circle cx="12" cy="10" r="7" fill="currentColor"/><rect x="9" y="17" width="6" height="4" rx="1" fill="currentColor"/></svg>
          </button>
          <button class="aux-btn" onclick="auxBtn(3)">3</button>
          <button class="aux-btn" onclick="auxBtn(4)">4</button>
        </div>
      </div>

      <div class="col">
        <span class="lbl">Правая гусеница</span>
        <span class="val" id="val-right">0%</span>
        <div class="slider-v-container">
          <input type="range" id="slider-right" class="slider-v" min="-100" max="100" value="0"
                 oninput="updateTank('B',this.value)"
                 onmouseup="resetSlider('slider-right','B',updateTank)" ontouchend="resetSlider('slider-right','B',updateTank)">
        </div>
        <button class="rev-btn" data-ch="B" onclick="toggleRev('B')">&#8644; РЕВЕРС</button>
      </div>
    </div>
  </main>

  <main id="classic-panel" class="panel hidden">
    <div class="cluster">
      <div class="col">
        <span class="lbl">Газ (A)</span>
        <span class="val g" id="val-drive">0%</span>
        <div class="slider-v-container">
          <input type="range" id="slider-drive" class="slider-v" min="-100" max="100" value="0"
                 oninput="updateDrive('A',this.value)"
                 onmouseup="resetSlider('slider-drive','A',updateDrive)" ontouchend="resetSlider('slider-drive','A',updateDrive)">
        </div>
        <button class="rev-btn" data-ch="A" onclick="toggleRev('A')">&#8644; РЕВЕРС</button>
      </div>

      <div class="col">
        <svg class="car-svg" viewBox="0 0 240 200">
          <rect x="70" y="20" width="100" height="160" rx="18" fill="#1b2029" stroke="#2a3140" stroke-width="3"/>
          <polygon id="c-fwd" points="120,38 105,55 135,55" fill="#2a3140"/>
          <polygon id="c-rev" points="120,162 105,145 135,145" fill="#2a3140"/>

          <!-- Иконка доп. мотора: стрелка влево — корпус с молнией — стрелка вправо. Стрелки не крутятся,
               просто загораются в сторону текущего направления вращения. -->
          <g transform="translate(120,100)">
            <polygon id="aux-arrow-l" points="-44,0 -30,-9 -30,9" fill="#2a3140"/>
            <rect x="-16" y="-14" width="32" height="28" rx="8" fill="#1b2029" stroke="#2a3140" stroke-width="2"/>
            <polygon id="aux-bolt" points="1,-10 -6,2 0,2 -3,10 7,-3 1,-3" fill="#2a3140"/>
            <polygon id="aux-arrow-r" points="44,0 30,-9 30,9" fill="#2a3140"/>
          </g>

          <g id="c-fl" class="wheel" transform="translate(36,45)"><rect x="-11" y="-20" width="22" height="40" rx="6" fill="#0d0f12" stroke="#3b82f6" stroke-width="2"/></g>
          <g id="c-fr" class="wheel" transform="translate(204,45)"><rect x="-11" y="-20" width="22" height="40" rx="6" fill="#0d0f12" stroke="#3b82f6" stroke-width="2"/></g>
          <g><rect x="25" y="135" width="22" height="45" rx="6" fill="#0d0f12" stroke="#2a3140" stroke-width="2"/></g>
          <g><rect x="193" y="135" width="22" height="45" rx="6" fill="#0d0f12" stroke="#2a3140" stroke-width="2"/></g>
        </svg>
        <div class="btn-row">
          <button class="aux-btn led-btn" data-ch="F" onclick="toggleLed('F')" title="Передние фары">
            <svg viewBox="0 0 24 24" width="18" height="18"><circle cx="12" cy="10" r="7" fill="currentColor"/><rect x="9" y="17" width="6" height="4" rx="1" fill="currentColor"/></svg>
          </button>
          <button class="aux-btn led-btn" data-ch="R" onclick="toggleLed('R')" title="Задние фонари">
            <svg viewBox="0 0 24 24" width="18" height="18"><circle cx="12" cy="10" r="7" fill="currentColor"/><rect x="9" y="17" width="6" height="4" rx="1" fill="currentColor"/></svg>
          </button>
          <button class="aux-btn" onclick="auxBtn(3)">3</button>
          <button class="aux-btn" onclick="auxBtn(4)">4</button>
        </div>
      </div>

      <div class="side">
        <div class="field">
          <div class="row"><label>Доп. мотор (B)</label><span class="val c" id="val-aux">0%</span></div>
          <div class="slim-wrap">
            <input type="range" class="slim" id="slider-aux" min="-100" max="100" value="0"
                   oninput="updateAux('B',this.value)"
                   onmouseup="resetSlider('slider-aux','B',updateAux)" ontouchend="resetSlider('slider-aux','B',updateAux)">
          </div>
          <div class="cal">
            <span class="lbl">Центр</span>
            <button class="mini-btn" onclick="adjTrim(-10)">&minus;</button>
            <span class="val" id="val-trim">0</span>
            <button class="mini-btn" onclick="adjTrim(10)">+</button>
            <span class="lbl" style="margin-left:6px">Макс.&#176;</span>
            <input type="number" class="num" id="maxdeg-input" min="5" max="90" value="45" onchange="setMaxDeg(this.value)">
          </div>
          <div class="cal">
            <span class="lbl">Реверс мотора B</span>
            <button class="rev-btn" data-ch="B" onclick="toggleRev('B')">&#8644;</button>
          </div>
        </div>
        <div>
          <div class="row"><label class="lbl">Руль (Servo)</label><span class="val" id="val-steer">0%</span></div>
          <div class="slider-h-container">
            <input type="range" id="slider-steer" min="-100" max="100" value="0"
                   oninput="updateSteer('S',this.value)"
                   onmouseup="resetSlider('slider-steer','S',updateSteer)" ontouchend="resetSlider('slider-steer','S',updateSteer)">
          </div>
        </div>
      </div>
    </div>
  </main>

  <div class="footer">
    <span>Монитор порта (TX):</span>
    <code id="telemetry">{"ch":"A","val":0}</code>
  </div>
</div>

<script>
document.addEventListener('touchmove', function(e){ if (e.target.tagName !== 'INPUT') e.preventDefault(); }, {passive:false});

async function toggleFullscreen(){
  try{
    if(!document.fullscreenElement){
      await document.documentElement.requestFullscreen();
      if(screen.orientation && screen.orientation.lock) await screen.orientation.lock('landscape').catch(()=>{});
    } else { document.exitFullscreen(); }
  }catch(e){}
}

// ---------- подгонка длины ползунков под высоту экрана ----------
function fitSliders(){
  var p = document.querySelector('.panel:not(.hidden)');
  if (!p) return;
  var h = Math.max(110, Math.min(240, p.clientHeight - 96));
  document.documentElement.style.setProperty('--sh', h + 'px');
}
window.addEventListener('resize', fitSliders);
window.addEventListener('load', fitSliders);

// ---------- WebSocket с автопереподключением ----------
var ws = null;
var state = {A:0, B:0, S:0}; // последние отправленные значения по каждому каналу
var curMode = 0, modeHoldUntil = 0;

function wsOpen(){ return ws && ws.readyState === WebSocket.OPEN; }

function connectWS(){
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen = function(){ setLink(true); stopAll(); loadCalib(); };
  ws.onclose = function(){ setLink(false); setTimeout(connectWS, 1000); };
  ws.onmessage = onStatus;
}

function setLink(ok){
  var b = document.getElementById('link-badge');
  b.textContent = ok ? 'ESP32' : 'Нет связи';
  b.classList.toggle('bad', !ok);
  document.getElementById('hstats').classList.toggle('stale', !ok);
  if (!ok) updateWifi(0);
}

function send(ch, val){
  state[ch] = parseInt(val);
  var data = {ch:ch, val:state[ch]};
  document.getElementById('telemetry').textContent = JSON.stringify(data);
  if (wsOpen()) ws.send(JSON.stringify(data));
}

// Пока слайдер держат на месте, браузер не шлёт новых oninput-событий,
// а серверный сторож (500мс без пакетов) глушит мотор/серву, приняв тишину за обрыв связи.
// Поэтому раз в 150мс подтверждаем текущие значения, даже если они не менялись.
setInterval(function(){
  if (!wsOpen()) return;
  for (var ch in state) ws.send(JSON.stringify({ch:ch, val:state[ch]}));
}, 150);

// ---------- статус от платы: Wi-Fi, батарея, режим, реверс, свет ----------
function onStatus(ev){
  var d;
  try { d = JSON.parse(ev.data); } catch(e){ return; }
  if (!d.st) return;
  updateWifi(d.r);
  updateBattery(d.v, d.p);
  if (typeof d.m === 'number' && d.m !== curMode && Date.now() > modeHoldUntil) applyMode(d.m);
  if (typeof d.ra === 'number') { revState.A = !!d.ra; updateRevButtons(); }
  if (typeof d.rb === 'number') { revState.B = !!d.rb; updateRevButtons(); }
  if (typeof d.lf === 'number') { ledState.F = !!d.lf; updateLedButtons(); }
  if (typeof d.lr === 'number') { ledState.R = !!d.lr; updateLedButtons(); }
}

function updateWifi(r){
  var n = 0, txt = '--';
  if (r < 0){
    n = r > -55 ? 4 : (r > -65 ? 3 : (r > -75 ? 2 : 1));
    txt = r + ' dBm';
  }
  var bars = document.getElementById('wifi-bars');
  bars.className = 'bars w' + n;
  for (var i = 0; i < 4; i++) bars.children[i].classList.toggle('on', i < n);
  document.getElementById('wifi-txt').textContent = txt;
}

var battLevel = 0;      // 0 — норма, 1 — низкий (<=10%), 2 — разряжен (<=1%)
var dismissedLvl = 0;   // уровень, для которого баннер закрыт вручную
var bannerShown = false;

function updateBattery(v, p){
  var fill = document.getElementById('bat-fill');
  var txt = document.getElementById('batt-txt');
  var ic = document.getElementById('bat-ic');
  if (v < 0.5){   // батарея не подключена (питание по USB)
    fill.style.width = '0%';
    txt.textContent = 'USB';
    ic.className = 'bat';
    battLevel = 0;
    showBanner(p);
    return;
  }
  txt.textContent = p + '% \u00b7 ' + v.toFixed(1) + 'V';
  fill.style.width = p + '%';
  ic.className = 'bat' + (p <= 10 ? ' crit' : (p <= 30 ? ' warn' : ''));

  // уровень баннера с гистерезисом, чтобы не мигал на границе
  var lvl = battLevel;
  if (p <= 1) lvl = 2;
  else if (p <= 10 && lvl < 1) lvl = 1;
  else if (lvl === 2 && p >= 4) lvl = 1;
  if (lvl >= 1 && p >= 13) lvl = 0;
  battLevel = lvl;
  showBanner(p);
}

function showBanner(p){
  if (battLevel === 0) dismissedLvl = 0;
  var show = (battLevel === 2) || (battLevel === 1 && dismissedLvl !== 1);
  var el = document.getElementById('batt-banner');
  if (show){
    el.className = 'banner lvl' + battLevel;
    document.getElementById('banner-text').textContent = (battLevel === 2)
      ? 'Аккумулятор почти разряжен (' + p + '%). Остановите модель и зарядите батарею!'
      : 'Низкий заряд аккумулятора (' + p + '%). Скоро потребуется зарядка.';
    document.getElementById('banner-x').style.display = (battLevel === 1) ? '' : 'none';
  } else {
    el.className = 'banner hidden';
  }
  if (show !== bannerShown){ bannerShown = show; fitSliders(); }
}

function dismissBanner(){
  if (battLevel === 1){ dismissedLvl = 1; showBanner(0); }
}

// ---------- режимы ----------
// applyMode — только интерфейс; switchMode — команда плате + интерфейс
function applyMode(m){
  curMode = m;
  document.getElementById('tank-panel').classList.toggle('hidden', m !== 0);
  document.getElementById('classic-panel').classList.toggle('hidden', m !== 1);
  document.getElementById('btn-tank').classList.toggle('active', m === 0);
  document.getElementById('btn-classic').classList.toggle('active', m === 1);
  fitSliders();
}

function switchMode(m){
  modeHoldUntil = Date.now() + 1500; // не даём устаревшему статусу переключить экран обратно
  if (wsOpen()) ws.send(JSON.stringify({mode:m}));
  applyMode(m);
  stopAll();
}

// ---------- управление ----------
function updateTank(ch, val){
  send(ch, val);
  var n = parseInt(val), s = (ch === 'A') ? 'left' : 'right';
  document.getElementById('val-' + s).textContent = val + '%';
  document.getElementById('t-' + s + '-f').setAttribute('fill', n > 0 ? '#3b82f6' : '#2a3140');
  document.getElementById('t-' + s + '-r').setAttribute('fill', n < 0 ? '#3b82f6' : '#2a3140');
  document.getElementById('t-' + s + '-body').setAttribute('stroke', n !== 0 ? '#3b82f6' : '#2a3140');
}

function updateDrive(ch, val){
  send(ch, val);
  document.getElementById('val-drive').textContent = val + '%';
  var n = parseInt(val);
  document.getElementById('c-fwd').setAttribute('fill', n > 0 ? '#10b981' : '#2a3140');
  document.getElementById('c-rev').setAttribute('fill', n < 0 ? '#10b981' : '#2a3140');
}

function updateSteer(ch, val){
  send(ch, val);
  document.getElementById('val-steer').textContent = val + '%';
  var deg = val * 0.35;
  document.getElementById('c-fl').style.transform = 'translate(36px,45px) rotate(' + deg + 'deg)';
  document.getElementById('c-fr').style.transform = 'translate(204px,45px) rotate(' + deg + 'deg)';
}

// ---------- доп. мотор: молния + статичные стрелки направления ----------
function updateAux(ch, val){
  send(ch, val);
  document.getElementById('val-aux').textContent = val + '%';
  var n = parseInt(val);
  var active = n !== 0;
  var bolt = document.getElementById('aux-bolt');
  bolt.classList.toggle('bolt-glow', active);
  bolt.classList.toggle('bolt-pulse', active);
  document.getElementById('aux-arrow-l').setAttribute('fill', n < 0 ? '#22d3ee' : '#2a3140');
  document.getElementById('aux-arrow-r').setAttribute('fill', n > 0 ? '#22d3ee' : '#2a3140');
}

// Кнопки 3/4 пока без функций — свободные заглушки под будущий функционал.
function auxBtn(n){ }

// ---------- свет (привязан к физическому выходу, не к режиму) ----------
var ledState = {F:false, R:false};

function toggleLed(ch){
  ledState[ch] = !ledState[ch]; // применится по факту сервером, но обновим сразу для отклика
  updateLedButtons();
  if (wsOpen()) ws.send(JSON.stringify({led: ch}));
}
function updateLedButtons(){
  document.querySelectorAll('.led-btn[data-ch="F"]').forEach(function(b){ b.classList.toggle('active', ledState.F); });
  document.querySelectorAll('.led-btn[data-ch="R"]').forEach(function(b){ b.classList.toggle('active', ledState.R); });
}

// ---------- реверс моторов (привязан к каналу, не к режиму) ----------
var revState = {A:false, B:false};

function toggleRev(ch){
  revState[ch] = !revState[ch]; // применится по факту сервером, но обновим сразу для отклика
  updateRevButtons();
  if (wsOpen()) ws.send(JSON.stringify({rev: ch}));
}
function updateRevButtons(){
  document.querySelectorAll('.rev-btn[data-ch="A"]').forEach(function(b){ b.classList.toggle('active', revState.A); });
  document.querySelectorAll('.rev-btn[data-ch="B"]').forEach(function(b){ b.classList.toggle('active', revState.B); });
}

// ---------- калибровка руля ----------
var trimValue = 0; // мкс, абсолютное смещение от 1500 — плата хранит то же самое значение

function adjTrim(delta){
  trimValue = Math.max(-400, Math.min(400, trimValue + delta));
  document.getElementById('val-trim').textContent = trimValue;
  if (wsOpen()) ws.send(JSON.stringify({trim: trimValue}));
}

function setMaxDeg(v){
  var deg = Math.max(5, Math.min(90, parseInt(v) || 45));
  document.getElementById('maxdeg-input').value = deg;
  if (wsOpen()) ws.send(JSON.stringify({maxdeg: deg}));
}

// Подтягиваем сохранённую на плате калибровку (руль + реверс + свет)
function loadCalib(){
  fetch('/calib').then(function(r){ return r.json(); }).then(function(c){
    trimValue = c.trim;
    document.getElementById('val-trim').textContent = trimValue;
    document.getElementById('maxdeg-input').value = c.maxdeg;
    revState.A = !!c.revA;
    revState.B = !!c.revB;
    updateRevButtons();
    ledState.F = !!c.ledF;
    ledState.R = !!c.ledR;
    updateLedButtons();
  }).catch(function(){});
}

function resetSlider(id, ch, fn){ var s = document.getElementById(id); s.value = 0; fn(ch, 0); }

function stopAll(){
  resetSlider('slider-left','A',updateTank);
  resetSlider('slider-right','B',updateTank);
  resetSlider('slider-drive','A',updateDrive);
  resetSlider('slider-steer','S',updateSteer);
  resetSlider('slider-aux','B',updateAux);
}

fitSliders();
connectWS();
</script>
</body>
</html>
)HTML";

void setup() {
  Serial.begin(115200);

  pinMode(TB_STBY, OUTPUT); digitalWrite(TB_STBY, HIGH); // включить драйвер
  pinMode(TB_AIN1, OUTPUT); pinMode(TB_AIN2, OUTPUT);
  pinMode(TB_BIN1, OUTPUT); pinMode(TB_BIN2, OUTPUT);
  ledcSetup(LEDC_CH_A, 5000, 8);      // канал, частота 5кГц, разрешение 8 бит (0-255)
  ledcAttachPin(TB_PWMA, LEDC_CH_A);  // привязка канала к физическому GPIO
  ledcSetup(LEDC_CH_B, 5000, 8);
  ledcAttachPin(TB_PWMB, LEDC_CH_B);

  pinMode(LED_FRONT_PIN, OUTPUT);
  pinMode(LED_REAR_PIN, OUTPUT);

  steerServo.setPeriodHertz(50);
  steerServo.attach(SERVO_PIN, 1000, 2000);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  loadMode();
  loadSteerCal();
  loadMotorRev();
  loadLeds();
  applyLeds();

  WiFi.softAP("LegoTechnic", "12345678"); // TODO: сменить пароль

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/html", PAGE_HTML);
  });
  server.on("/calib", HTTP_GET, [](AsyncWebServerRequest *req) {
    String json = "{\"trim\":" + String(steerCenterUs - 1500) +
                  ",\"maxdeg\":" + String(steerMaxAngleDeg) +
                  ",\"revA\":" + String(reverseA ? 1 : 0) +
                  ",\"revB\":" + String(reverseB ? 1 : 0) +
                  ",\"ledF\":" + String(ledFrontOn ? 1 : 0) +
                  ",\"ledR\":" + String(ledRearOn ? 1 : 0) + "}";
    req->send(200, "application/json", json);
  });

  // Веб-обновление прошивки: http://192.168.4.1/update, залить .bin — без USB и без Arduino IDE.
  // admin/admin — ок для отладки в поле (за WPA2-паролем самой точки доступа), но перед тем как
  // показывать модель кому-то ещё — сменить на что-то менее очевидное.
  ElegantOTA.begin(&server, "admin", "admin");
  server.begin();

  stopAll();
  updateStatus(); // первичные значения для первого подключившегося клиента
  lastCmdMillis = millis();
}

void loop() {
  static unsigned long lastStatus = 0;

  // Safety: нет команд > CMD_TIMEOUT_MS — стоп
  if (millis() - lastCmdMillis > CMD_TIMEOUT_MS) stopAll();

  if (millis() - lastStatus > 1000) {
    lastStatus = millis();
    updateStatus();
    updateDisplay(ws.count());
    if (ws.count() > 0) {
      char buf[144];
      buildStatus(buf, sizeof(buf));
      ws.textAll(buf);
    }
  }

  ElegantOTA.loop();
  ws.cleanupClients();
}