#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "motor_control.h"
#include "web_control.h"
#include "steering_control.h"
#include "preferences_manager.h"
#include "positional_servo.h"
#include "display_control.h"

// ---------- Состояние, объявленное в lego_technic_esp32.ino ----------

extern Mode currentMode;
extern volatile unsigned long lastCmdMillis;
extern Preferences prefs;

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

extern uint8_t motorModeA;
extern uint8_t motorModeB;
extern uint8_t motorModeC;
extern uint8_t motorModeD;
extern uint8_t motorModeE;
extern uint8_t motorModeF;

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

  const unsigned long messageMillis = millis();

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

  // Управление OLED действует только до перезагрузки и не записывается в NVS.
  if (doc.containsKey("oled")) {
    if (doc.size() != 1 || !doc["oled"].is<int>()) return;
    const int requestedState = doc["oled"].as<int>();
    if (requestedState != 0 && requestedState != 1) return;
    setOledDisplayEnabled(requestedState == 1);
    return;
  }

  // Смена конфигурации не отправляет команду мотору и допускается только на остановленном канале.
  if (doc.containsKey("cfgA") || doc.containsKey("cfgB") ||
      doc.containsKey("cfgD") ||
      doc.containsKey("cfgE") || doc.containsKey("cfgF")) {
    if (doc.size() != 1) return;
    if (doc.containsKey("cfgA")) {
      if (!doc["cfgA"].is<int>() || doc["cfgA"].as<int>() < 0 || doc["cfgA"].as<int>() > 1 || motorAVal != 0) return;
      motorModeA = (uint8_t)doc["cfgA"].as<int>(); saveMotorModeA(motorModeA);
    } else if (doc.containsKey("cfgB")) {
      if (!doc["cfgB"].is<int>() || doc["cfgB"].as<int>() < 0 || doc["cfgB"].as<int>() > 1 || motorBVal != 0) return;
      motorModeB = (uint8_t)doc["cfgB"].as<int>(); saveMotorModeB(motorModeB);
    } else if (doc.containsKey("cfgD")) {
      if (!doc["cfgD"].is<int>() || doc["cfgD"].as<int>() < 0 || doc["cfgD"].as<int>() > 1 || motorDVal != 0) return;
      motorModeD = (uint8_t)doc["cfgD"].as<int>(); saveMotorModeD(motorModeD);
    } else if (doc.containsKey("cfgE")) {
      if (!doc["cfgE"].is<int>() || doc["cfgE"].as<int>() < 0 || doc["cfgE"].as<int>() > 1 || motorEVal != 0) return;
      motorModeE = (uint8_t)doc["cfgE"].as<int>(); saveMotorModeE(motorModeE);
    } else {
      if (!doc["cfgF"].is<int>() || doc["cfgF"].as<int>() < 0 || doc["cfgF"].as<int>() > 1 || motorFVal != 0) return;
      motorModeF = (uint8_t)doc["cfgF"].as<int>(); saveMotorModeF(motorModeF);
    }
    return;
  }

  // Команда автоматического позиционирования мотора C. Одно нажатие задаёт цель,
  // движение продолжается автономно до достижения цели или срабатывания защиты.
  if (doc.containsKey("posC")) {
    if (doc.size() != 1 || !doc["posC"].is<const char*>() || motorModeC != 2) return;
    const char* target = doc["posC"].as<const char*>();
    PositionalServoTarget positionTarget;
    if (strcmp(target, "left") == 0) {
      positionTarget = PositionalServoTarget::Left;
    } else if (strcmp(target, "center") == 0) {
      positionTarget = PositionalServoTarget::Center;
    } else if (strcmp(target, "right") == 0) {
      positionTarget = PositionalServoTarget::Right;
    } else {
      return;
    }
    lastCmdMillis = messageMillis;
    positionalServoRequest(positionTarget);
    return;
  }

  // Heartbeat одновременно передаёт актуальные значения всех каналов.
  // Поэтому команды управления продолжают обновляться, даже если пользователь
  // держит слайдер неподвижно и браузер не генерирует новые input-события.
  if (doc.containsKey("hb")) {
    if (!doc["hb"].is<int>() ||
        doc["hb"].as<int>() != 1 ||
        doc.size() != 8 ||
        !doc["A"].is<int>() ||
        !doc["B"].is<int>() ||
        !doc["C"].is<int>() ||
        !doc["D"].is<int>() ||
        !doc["E"].is<int>() ||
        !doc["F"].is<int>() ||
        !doc["S"].is<int>()) {
      return;
    }

    const int a = doc["A"].as<int>();
    const int b = doc["B"].as<int>();
    const int c = doc["C"].as<int>();
    const int d = doc["D"].as<int>();
    const int e = doc["E"].as<int>();
    const int f = doc["F"].as<int>();
    const int steer = doc["S"].as<int>();
    if (a < -100 || a > 100 || b < -100 || b > 100 ||
        c < -100 || c > 100 || d < -100 || d > 100 ||
        e < -100 || e > 100 || f < -100 || f > 100 ||
        steer < -100 || steer > 100) {
      return;
    }

    applyMotorA(a);
    if (!engineSimOn) applyMotorB(b);
    // В позиционном режиме канал C принадлежит автономному регулятору.
    if (motorModeC != 2) applyMotorC(c);
    applyMotorD(d);
    applyMotorE(e);
    applyMotorF(f);
    applySteer(steer);
    updateDriverStandby();
    lastCmdMillis = messageMillis;
    return;
  }

  // Переключение режима
  if (doc.containsKey("mode")) {
    if (!doc["mode"].is<int>()) return;

    int requestedMode = doc["mode"].as<int>();

    if (requestedMode < MODE_TANK || requestedMode > MODE_CAR) {
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
    if (strcmp(ch, "C") == 0 && motorModeC == 2) return;

    if (strcmp(ch, "A") != 0 &&
        strcmp(ch, "B") != 0 &&
        strcmp(ch, "C") != 0 &&
        strcmp(ch, "D") != 0 &&
        strcmp(ch, "E") != 0 &&
        strcmp(ch, "F") != 0 &&
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

  if (doc.containsKey("revE")) {
    if (!doc["revE"].is<int>()) return;

    reverseE = doc["revE"].as<int>() == 1;
    saveReverseE(reverseE);
  }

  if (doc.containsKey("revF")) {
    if (!doc["revF"].is<int>()) return;

    reverseF = doc["revF"].as<int>() == 1;
    saveReverseF(reverseF);
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
    } else if (strcmp(ch, "E") == 0) {
      applyMotorE(val);
    } else if (strcmp(ch, "F") == 0) {
      applyMotorF(val);
    } else if (strcmp(ch, "S") == 0) {
      applySteer(val);
    }
  } else if (doc.containsKey("revA") ||
             doc.containsKey("revB") ||
             doc.containsKey("revC") ||
             doc.containsKey("revD") ||
             doc.containsKey("revE") ||
             doc.containsKey("revF") ||
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
