
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
  motorModeC = prefs.getUChar("modeC", 0);
  if (motorModeC > 2) motorModeC = 0;
  motorModeD = prefs.getUChar("modeD", 0);
  if (motorModeD > 1) motorModeD = 0;
  motorModeE = prefs.getUChar("modeE", 0);
  if (motorModeE > 1) motorModeE = 0;
  motorModeF = prefs.getUChar("modeF", 0);
  if (motorModeF > 1) motorModeF = 0;

  ledFrontOn = prefs.getUChar("ledF", 0) != 0;
  ledRearOn  = prefs.getUChar("ledR", 0) != 0;

  // Clamp persisted calibration to the supported symmetric steering range.
  steerCenterUs = constrain(prefs.getInt("steer_c", 1500), 1250, 1750);
  steerMaxAngleDeg = constrain(prefs.getInt("steer_a", 45), 5, 135);
}


// ---------- Сохранение калибровки руля ----------

void saveSteeringCenter(int value) {
  prefs.putInt("steer_c", value);
}

void saveSteeringMaxAngle(int value) {
  prefs.putInt("steer_a", value);
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
void saveMotorModeC(uint8_t value) { prefs.putUChar("modeC", value); }
void saveMotorModeD(uint8_t value) { prefs.putUChar("modeD", value); }
void saveMotorModeE(uint8_t value) { prefs.putUChar("modeE", value); }
void saveMotorModeF(uint8_t value) { prefs.putUChar("modeF", value); }
