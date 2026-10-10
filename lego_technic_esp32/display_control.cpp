
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESPAsyncWebServer.h>

#include "motor_control.h"
#include "battery_monitor.h"
#include "display_control.h"

// Дисплей и WebSocket объявлены в app.cpp
extern Adafruit_SSD1306 display;
extern AsyncWebSocket ws;

// Режим работы объявлен в app.cpp
extern Mode currentMode;


// ---------- Иконка режима ----------

static void drawModeIcon() {
  int x = 92;
  int y = 42;

  display.drawRect(x, y, 24, 16, SSD1306_WHITE);

  if (currentMode == MODE_TANK) {
    display.fillRect(
      x + 6, y + 2, 12, 8, SSD1306_WHITE
    );
    display.drawLine(
      x + 12, y + 6, x + 28, y + 6, SSD1306_WHITE
    );
  } else if (currentMode == MODE_CAR) {
    display.fillCircle(
      x + 6, y + 18, 3, SSD1306_WHITE
    );
    display.fillCircle(
      x + 18, y + 18, 3, SSD1306_WHITE
    );
    display.fillRect(
      x + 2, y + 4, 20, 10, SSD1306_WHITE
    );
  } else {
    // MODE_TEST
    display.drawLine(
      x + 12, y + 2, x + 12, y + 14, SSD1306_WHITE
    );
    display.drawLine(
      x + 6, y + 4, x + 18, y + 4, SSD1306_WHITE
    );
    display.drawLine(
      x + 6, y + 8, x + 18, y + 8, SSD1306_WHITE
    );
  }
}


// ---------- Обновление OLED ----------

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Предупреждение о низком заряде
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

  // Название текущего режима
  display.setTextSize(2);
  display.setCursor(0, 0);

  if (currentMode == MODE_TANK) {
    display.print("TANK");
  } else if (currentMode == MODE_CAR) {
    display.print("CAR");
  } else {
    display.print("TEST");
  }

  // Уровень заряда
  display.setCursor(0, 20);
  display.printf("%d%%\n", battPct);

  // Напряжение батареи и количество клиентов
  display.setTextSize(1);

  display.setCursor(0, 44);
  display.printf("%.2fV\n", battV);

  display.setCursor(0, 54);
  display.printf("Clients: %d\n", ws.count());

  drawModeIcon();
  display.display();
}
