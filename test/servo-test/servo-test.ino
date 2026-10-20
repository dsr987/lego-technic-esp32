/*
 * LEGO Technic ESP32 — Servo range tester
 *
 * Standalone diagnostic sketch. Does not use or modify the main firmware.
 * Board: classic ESP32 Dev Module / ESP32-WROOM-32
 * Servo signal: GPIO27 (same signal pin as the main firmware).
 *
 * SAFETY:
 * - Power the servo from a suitable external supply, not the ESP32 3V3 pin.
 * - Connect servo GND and ESP32 GND together.
 * - Start with the mechanical linkage disconnected.
 * - Initial pulse range is deliberately conservative: 1000..2000 us.
 * - A requested "360°" is only a command mapping, NOT proof the servo can
 *   physically rotate 360°. Stop if it buzzes, strains, heats, or hits a stop.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESP32Servo.h>

static constexpr uint8_t SERVO_PIN = 27;
static constexpr char AP_SSID[] = "Servo-Test";
static constexpr char AP_PASSWORD[] = "12345678";
static constexpr uint16_t DEFAULT_MIN_US = 1000;
static constexpr uint16_t DEFAULT_MAX_US = 2000;
static constexpr uint16_t HARD_MIN_US = 500;
static constexpr uint16_t HARD_MAX_US = 2500;

WebServer server(80);
Servo servo;

int commandDegrees = 90;
uint16_t minPulseUs = DEFAULT_MIN_US;
uint16_t maxPulseUs = DEFAULT_MAX_US;

int pulseForDegrees(int degrees) {
  degrees = constrain(degrees, 0, 360);
  return map(degrees, 0, 360, minPulseUs, maxPulseUs);
}

void moveServo(int degrees) {
  commandDegrees = constrain(degrees, 0, 360);
  servo.writeMicroseconds(pulseForDegrees(commandDegrees));
}

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Servo Test</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#111827;color:#f3f4f6}
body{max-width:620px;margin:0 auto;padding:20px}
h1{font-size:1.6rem;margin-bottom:6px}.muted{color:#9ca3af;font-size:.92rem}
.panel{background:#1f2937;border:1px solid #374151;border-radius:14px;padding:16px;margin:14px 0}
.value{font-size:2.5rem;font-weight:750;text-align:center;margin:8px}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
button{background:#374151;color:#fff;border:1px solid #4b5563;border-radius:10px;padding:12px 4px;font-size:1rem}
button:active{background:#2563eb}.wide{width:100%;margin-top:8px}
input{width:100%;accent-color:#60a5fa}label{display:block;margin:12px 0 4px}
.warning{color:#fbbf24}.small{font-size:.85rem}
</style></head><body>
<h1>Servo range tester</h1>
<div class="muted">ESP32 GPIO27 · локальная Wi-Fi-точка доступа</div>
<div class="panel"><div class="muted">Командное положение</div>
<div class="value"><span id="deg">90</span>°</div>
<div class="muted" style="text-align:center">Импульс: <b id="pulse">1250</b> µs</div>
<label for="angle">Угол команды: <span id="rangeVal">90°</span></label>
<input id="angle" type="range" min="0" max="360" step="1" value="90">
<div class="grid" style="margin-top:12px">
<button onclick="move(-5)">−5°</button><button onclick="move(5)">+5°</button>
<button onclick="move(-1)">−1°</button><button onclick="move(1)">+1°</button>
</div></div>
<div class="panel"><b>Пресеты</b><div class="grid" style="margin-top:12px">
<button onclick="setAngle(0)">0°</button><button onclick="setAngle(45)">45°</button>
<button onclick="setAngle(90)">90°</button><button onclick="setAngle(135)">135°</button>
<button onclick="setAngle(180)">180°</button><button onclick="setAngle(270)">270°</button>
<button onclick="setAngle(360)">360°</button><button onclick="setAngle(90)">Центр 90°</button>
</div></div>
<div class="panel"><b>Диапазон импульсов</b>
<p class="muted">Начните с 1000–2000 µs. Меняйте только небольшими шагами, если серва не упирается в механические ограничители.</p>
<label>Минимум: <span id="minVal">1000</span> µs</label>
<input id="minPulse" type="range" min="500" max="1400" step="10" value="1000">
<label>Максимум: <span id="maxVal">2000</span> µs</label>
<input id="maxPulse" type="range" min="1600" max="2500" step="10" value="2000">
<button class="wide" onclick="applyRange()">Применить диапазон импульсов</button>
</div>
<div class="panel"><b>Восстановление основной прошивки</b><p class="muted">После эксперимента можно вернуть основную прошивку через OTA, не подключая USB.</p><form method="POST" action="/update" enctype="multipart/form-data"><input type="file" name="firmware" accept=".bin" required><button class="wide" type="submit">Загрузить BIN и перезапустить</button></form></div><p class="warning small">Важно: надпись 360° задаёт командный диапазон, а не измеренный физический ход. Не удерживайте серву в упоре.</p>
<script>
const $=id=>document.getElementById(id);
let angle=90,minPulse=1000,maxPulse=2000;
function pulse(){return Math.round(minPulse+(maxPulse-minPulse)*angle/360)}
function render(){ $('deg').textContent=angle;$('rangeVal').textContent=angle+'°';$('angle').value=angle;$('pulse').textContent=pulse(); }
async function send(path){try{const r=await fetch(path);if(!r.ok)throw Error(r.status);const d=await r.json();angle=d.deg;minPulse=d.min;maxPulse=d.max;$('minPulse').value=minPulse;$('maxPulse').value=maxPulse;$('minVal').textContent=minPulse;$('maxVal').textContent=maxPulse;render();}catch(e){console.error(e)}}
function setAngle(v){angle=Math.max(0,Math.min(360,v));render();send('/move?deg='+angle)}
function move(delta){setAngle(angle+delta)}
function applyRange(){const lo=Number($('minPulse').value),hi=Number($('maxPulse').value);if(hi<=lo+100){alert('Разница между импульсами должна быть больше 100 µs');return}send('/range?min='+lo+'&max='+hi)}
$('angle').addEventListener('input',e=>{angle=Number(e.target.value);render()});
$('angle').addEventListener('change',()=>send('/move?deg='+angle));
$('minPulse').addEventListener('input',e=>$('minVal').textContent=e.target.value);
$('maxPulse').addEventListener('input',e=>$('maxVal').textContent=e.target.value);
send('/status');
</script></body></html>
)HTML";

void sendStatus() {
  String json = "{\"deg\":" + String(commandDegrees) +
                ",\"pulse\":" + String(pulseForDegrees(commandDegrees)) +
                ",\"min\":" + String(minPulseUs) +
                ",\"max\":" + String(maxPulseUs) + "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, minPulseUs, maxPulseUs);
  moveServo(commandDegrees);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  server.on("/status", HTTP_GET, sendStatus);
  server.on("/move", HTTP_GET, []() {
    if (!server.hasArg("deg")) {
      server.send(400, "application/json", "{\"error\":\"missing deg\"}");
      return;
    }
    moveServo(server.arg("deg").toInt());
    sendStatus();
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(Update.hasError() ? 500 : 200, "text/plain", Update.hasError() ? "OTA update failed" : "OTA update complete; rebooting");
    delay(500);
    if (!Update.hasError()) ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("OTA upload start: %s\\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) Serial.printf("OTA upload complete: %u bytes\\n", upload.totalSize);
      else Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      Update.abort();
    }
  });
  server.on("/range", HTTP_GET, []() {
    if (!server.hasArg("min") || !server.hasArg("max")) {
      server.send(400, "application/json", "{\"error\":\"missing pulse range\"}");
      return;
    }
    const int lo = constrain(server.arg("min").toInt(), HARD_MIN_US, 1400);
    const int hi = constrain(server.arg("max").toInt(), 1600, HARD_MAX_US);
    if (hi <= lo + 100) {
      server.send(400, "application/json", "{\"error\":\"invalid range\"}");
      return;
    }
    minPulseUs = lo;
    maxPulseUs = hi;
    servo.attach(SERVO_PIN, minPulseUs, maxPulseUs);
    moveServo(commandDegrees);
    sendStatus();
  });
  server.begin();

  Serial.println("Servo tester ready.");
  Serial.printf("Wi-Fi: %s / %s\n", AP_SSID, AP_PASSWORD);
  Serial.println("Open http://192.168.4.1/");
  Serial.printf("Servo signal GPIO%u, initial command %d degrees (%d us).\n",
                SERVO_PIN, commandDegrees, pulseForDegrees(commandDegrees));
}

void loop() {
  server.handleClient();
}
