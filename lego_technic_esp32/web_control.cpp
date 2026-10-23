#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "motor_control.h"
#include "web_control.h"
#include "steering_control.h"
#include "preferences_manager.h"

// ---------- Состояние, объявленное в lego_technic_esp32.ino ----------

extern Mode currentMode;
extern volatile unsigned long lastCmdMillis;
extern volatile unsigned long lastWsActivityMillis;
extern volatile unsigned long lastHeartbeatMillis;
extern volatile unsigned long heartbeatCount;
extern Preferences prefs;

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
extern void buildStatus(char *buf, size_t n);

// ---------- Обработка входящих команд WebSocket ----------

void handleWsMessage(AsyncWebSocketClient *client, uint8_t *data, size_t len) {
  StaticJsonDocument<256> doc;

  if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
    return;
  }

  // Любое корректно разобранное сообщение подтверждает активность WebSocket.
  // Отдельно учитываем heartbeat и команды управления, чтобы отличать потерю связи
  // от отсутствия движения ползунков и видеть возраст каждого типа активности.
  const unsigned long messageMillis = millis();
  lastWsActivityMillis = messageMillis;

  // Reset persistent ESP32 settings and restore safe defaults.
  if (doc.containsKey("reset")) {
    if (doc.size() != 1 || !doc["reset"].is<int>() || doc["reset"].as<int>() != 1) {
      return;
    }

    prefs.clear();
    loadPreferences(); // Defaults: center 1500 us, max angle 45 deg, reversals/lights off.
    engineSimOn = false;
    stopAll();
    applyLeds();
    lastCmdMillis = millis();

    if (client) client->text("{\"reset\":1}");
    return;
  }

  // Heartbeat подтверждает, что вкладка управления ещё активна.
  // Он намеренно НЕ повторяет значения слайдеров: после срабатывания
  // CMD_TIMEOUT_MS остановленные моторы не должны запуститься повторно
  // только потому, что браузер восстановил соединение или таймеры.
  if (doc.containsKey("hb")) {
    if (doc.size() != 1 || !doc["hb"].is<int>() || doc["hb"].as<int>() != 1) {
      return;
    }

    lastHeartbeatMillis = messageMillis;
    heartbeatCount++;
    return;
  }

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

  // Центр рулевого сервопривода
  if (doc.containsKey("trim")) {
    if (!doc["trim"].is<int>()) return;

    lastCmdMillis = millis();
    // Keep the calibrated center within the range that allows symmetric +/-135 degrees.
    // trim is expressed in degrees relative to neutral (0 degrees).
    const int trimDegrees = constrain(doc["trim"].as<int>(), -45, 45);
    const int trimOffsetUs = (trimDegrees * 1000 + (trimDegrees >= 0 ? 90 : -90)) / 180;
    steerCenterUs = 1500 + trimOffsetUs;

    saveSteeringCenter(steerCenterUs);
    applySteer(servoVal);
    return;
  }

  // Максимальный угол рулевого управления
  if (doc.containsKey("maxdeg")) {
    if (!doc["maxdeg"].is<int>()) return;

    lastCmdMillis = millis();
    steerMaxAngleDeg = constrain(doc["maxdeg"].as<int>(), 5, 135);

    saveSteeringMaxAngle(steerMaxAngleDeg);
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
    saveReverseA(reverseA);
  }

  if (doc.containsKey("revB")) {
    if (!doc["revB"].is<int>()) return;

    reverseB = doc["revB"].as<int>() == 1;
    saveReverseB(reverseB);
  }

  if (doc.containsKey("revC")) {
    if (!doc["revC"].is<int>()) return;

    reverseC = doc["revC"].as<int>() == 1;
    saveReverseC(reverseC);
  }

  if (doc.containsKey("revD")) {
    if (!doc["revD"].is<int>()) return;

    reverseD = doc["revD"].as<int>() == 1;
    saveReverseD(reverseD);
  }

  // Передний и задний свет. Физическая реализация applyLeds() пока пустая.
  if (doc.containsKey("ledF")) {
    if (!doc["ledF"].is<int>()) return;

    ledFrontOn = doc["ledF"].as<int>() == 1;
    saveLedFront(ledFrontOn);
    applyLeds();
  }

  if (doc.containsKey("ledR")) {
    if (!doc["ledR"].is<int>()) return;

    ledRearOn = doc["ledR"].as<int>() == 1;
    saveLedRear(ledRearOn);
    applyLeds();
  }

  // Имитация ДВС разрешена только в режиме классического автомобиля.
  // При выключении B обязательно получает нулевую команду.
  if (doc.containsKey("eng")) {
    if (!doc["eng"].is<int>()) return;

    bool requestedEngineSim = doc["eng"].as<int>() == 1;
    engineSimOn = requestedEngineSim && currentMode == MODE_CAR;

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
    char buf[384];
    buildStatus(buf, sizeof(buf));
    client->text(buf);
  } else if (type == WS_EVT_DATA) {
    handleWsMessage(client, data, len);
  }
}
