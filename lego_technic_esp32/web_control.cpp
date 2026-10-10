#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "motor_control.h"
#include "web_control.h"

// ---------- Состояние, объявленное в app.cpp ----------

extern Mode currentMode;
extern Preferences prefs;

extern unsigned long lastCmdMillis;

extern int motorAVal;
extern int motorBVal;
extern int motorCVal;
extern int motorDVal;
extern int servoVal;

extern bool reverseA;
extern bool reverseB;
extern bool reverseC;
extern bool reverseD;

extern bool ledFrontOn;
extern bool ledRearOn;
extern bool engineSimOn;

extern int steerCenterUs;
extern int steerMaxAngleDeg;

// ---------- Функции из других модулей ----------

extern void applyLeds();
extern void applySteer(int val);

extern void buildStatus(char *buf, size_t n);

// ---------- Обработка входящих команд WebSocket ----------

void handleWsMessage(uint8_t *data, size_t len) {
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, data, len) != DeserializationError::Ok) return;

  // Переключение режима
  if (doc.containsKey("mode")) {
    if (!doc["mode"].is<int>()) return;

    int requestedMode = doc["mode"].as<int>();
    if (requestedMode < MODE_TANK || requestedMode > MODE_TEST) {
      return;
    }

    lastCmdMillis = millis();
    currentMode = static_cast<Mode>(requestedMode);
    engineSimOn = false;
    stopAll();
    return;
  }

  // Центр и калибровка рулевого сервопривода
  if (doc.containsKey("trim")) {
    if (!doc["trim"].is<int>()) return;

    lastCmdMillis = millis();
    steerCenterUs = 1500 + constrain(doc["trim"].as<int>(), -400, 400);
    prefs.putInt("steer_c", steerCenterUs);
    applySteer(servoVal);
    return;
  }

  if (doc.containsKey("maxdeg")) {
    if (!doc["maxdeg"].is<int>()) return;

    lastCmdMillis = millis();
    steerMaxAngleDeg = constrain(doc["maxdeg"].as<int>(), 5, 90);
    prefs.putInt("steer_a", steerMaxAngleDeg);
    applySteer(servoVal);
    return;
  }

  // Проверка команды управления каналом
  bool hasChannel = doc.containsKey("ch");
  const char* ch = "";
  int val = 0;

  if (hasChannel) {
    if (!doc["ch"].is<const char*>() || !doc["val"].is<int>()) {
      return;
    }

    ch = doc["ch"].as<const char*>();
    val = doc["val"].as<int>();

    if (val < -100 || val > 100) return;

    if (strcmp(ch, "A") != 0 &&
        strcmp(ch, "B") != 0 &&
        strcmp(ch, "C") != 0 &&
        strcmp(ch, "D") != 0 &&
        strcmp(ch, "S") != 0) {
      return;
    }
  }

  // Настройки реверса моторов
  if (doc.containsKey("revA")) {
    if (!doc["revA"].is<int>()) return;
    reverseA = doc["revA"].as<int>() == 1;
    prefs.putUChar("revA", reverseA ? 1 : 0);
  }

  if (doc.containsKey("revB")) {
    if (!doc["revB"].is<int>()) return;
    reverseB = doc["revB"].as<int>() == 1;
    prefs.putUChar("revB", reverseB ? 1 : 0);
  }

  if (doc.containsKey("revC")) {
    if (!doc["revC"].is<int>()) return;
    reverseC = doc["revC"].as<int>() == 1;
    prefs.putUChar("revC", reverseC ? 1 : 0);
  }

  if (doc.containsKey("revD")) {
    if (!doc["revD"].is<int>()) return;
    reverseD = doc["revD"].as<int>() == 1;
    prefs.putUChar("revD", reverseD ? 1 : 0);
  }

  // Передний и задний свет
  if (doc.containsKey("ledF")) {
    if (!doc["ledF"].is<int>()) return;
    ledFrontOn = doc["ledF"].as<int>() == 1;
    prefs.putUChar("ledF", ledFrontOn ? 1 : 0);
    applyLeds();
  }

  if (doc.containsKey("ledR")) {
    if (!doc["ledR"].is<int>()) return;
    ledRearOn = doc["ledR"].as<int>() == 1;
    prefs.putUChar("ledR", ledRearOn ? 1 : 0);
    applyLeds();
  }

  // Симуляция двигателя
  if (doc.containsKey("eng")) {
    if (!doc["eng"].is<int>()) return;
    engineSimOn = doc["eng"].as<int>() == 1;

    if (!engineSimOn) {
      motorBVal = 0;
      applyMotorB(0);
    } else {
      applyMotorA(motorAVal);
    }
  }

  // Применяем команду управления только после проверки
  if (hasChannel) {
    lastCmdMillis = millis();

    if (strcmp(ch, "A") == 0) {
      applyMotorA(val);
    } else if (strcmp(ch, "B") == 0 && !engineSimOn) {
      applyMotorB(val);
    } else if (strcmp(ch, "C") == 0) {
      applyMotorC(val);
    } else if (strcmp(ch, "D") == 0) {
      applyMotorD(val);
    } else if (strcmp(ch, "S") == 0) {
      applySteer(val);
    }
  } else if (doc.containsKey("revA") ||
             doc.containsKey("revB") ||
             doc.containsKey("revC") ||
             doc.containsKey("revD") ||
             doc.containsKey("ledF") ||
             doc.containsKey("ledR") ||
             doc.containsKey("eng")) {
    lastCmdMillis = millis();
  }

  updateDriverStandby();
}

// ---------- События WebSocket ----------

void onWsEvent(
  AsyncWebSocket *server,
  AsyncWebSocketClient *client,
  AwsEventType type,
  void *arg,
  uint8_t *data,
  size_t len
) {
  if (type == WS_EVT_CONNECT) {
    char buf[200];
    buildStatus(buf, sizeof(buf));
    client->text(buf);
  } else if (type == WS_EVT_DATA) {
    handleWsMessage(data, len);
  }
}