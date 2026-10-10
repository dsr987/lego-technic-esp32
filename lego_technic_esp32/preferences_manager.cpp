
#include <Arduino.h>
#include <Preferences.h>

#include "preferences_manager.h"

// Объект и состояние определяются в app.cpp
extern Preferences prefs;

extern bool reverseA;
extern bool reverseB;
extern bool reverseC;
extern bool reverseD;

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

  ledFrontOn = prefs.getUChar("ledF", 0) != 0;
  ledRearOn  = prefs.getUChar("ledR", 0) != 0;

  steerCenterUs = prefs.getInt("steer_c", 1500);
  steerMaxAngleDeg = prefs.getInt("steer_a", 45);
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


// ---------- Сохранение света ----------

void saveLedFront(bool value) {
  prefs.putUChar("ledF", value ? 1 : 0);
}

void saveLedRear(bool value) {
  prefs.putUChar("ledR", value ? 1 : 0);
}
