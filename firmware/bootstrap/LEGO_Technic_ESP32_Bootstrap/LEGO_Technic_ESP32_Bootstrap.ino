/*
 * LEGO Technic ESP32 — OTA Bootstrap
 *
 * Purpose:
 *   First USB flash for a new ESP32 DevKit/WROOM board.
 *   After this firmware starts, the normal project firmware can be uploaded
 *   from a browser through ElegantOTA.
 *
 * Keep the AP, OTA route, OTA credentials, ESP32 core and partition scheme
 * compatible with the main firmware in lego_technic_esp32/app.cpp.
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

namespace {

constexpr char AP_SSID[] = "LegoTechnic";
constexpr char AP_PASSWORD[] = "12345678";
constexpr char OTA_USERNAME[] = "admin";
constexpr char OTA_PASSWORD[] = "admin";

AsyncWebServer server(80);

const char BOOTSTRAP_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LEGO Technic ESP32 — OTA Setup</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    body { margin: 0; min-height: 100vh; display: grid; place-items: center;
           background: #101318; color: #f3f4f6; }
    main { width: min(92vw, 520px); box-sizing: border-box; padding: 28px;
           border: 1px solid #343a46; border-radius: 20px; background: #191e26; }
    h1 { margin-top: 0; font-size: 1.5rem; }
    p { line-height: 1.55; color: #c4cad4; }
    code { color: #fff; }
    a.button { display: block; margin-top: 24px; padding: 14px 18px;
                border-radius: 12px; text-align: center; text-decoration: none;
                background: #2563eb; color: white; font-weight: 700; }
    .note { margin-top: 20px; font-size: .9rem; }
  </style>
</head>
<body>
  <main>
    <h1>LEGO Technic ESP32</h1>
    <p>Стартовая прошивка установлена. Теперь можно загрузить основную прошивку проекта через ElegantOTA.</p>
    <p>Адрес контроллера: <code>192.168.4.1</code></p>
    <a class="button" href="/update">Открыть обновление прошивки</a>
    <p class="note">После успешного обновления контроллер перезапустится и запустит основную веб-панель.</p>
  </main>
</body>
</html>
)HTML";

} // namespace

void setup() {
  WiFi.mode(WIFI_AP);

  // Use the same access point settings as the main project firmware.
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    delay(500);
    ESP.restart();
    return;
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html; charset=utf-8", BOOTSTRAP_PAGE);
  });

  server.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"ok\":true,\"ota\":\"/update\"}");
  });

  // The installed ElegantOTA library must have async mode enabled.
  ElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);
  server.begin();
}

void loop() {
  ElegantOTA.loop();
}
