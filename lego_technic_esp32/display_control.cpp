#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESPAsyncWebServer.h>
#include <pgmspace.h>
#include <stdio.h>

#include "motor_control.h"
#include "battery_monitor.h"
#include "display_control.h"
#include "display_assets.h"

// Дисплей, WebSocket и признак успешной инициализации объявлены в app.cpp.
extern Adafruit_SSD1306 display;
extern AsyncWebSocket ws;
extern bool oledAvailable;
extern bool motorBoardReady;

// Режим работы объявлен в app.cpp.
extern Mode currentMode;

// Состояние загрузочной анимации.
static bool startupAnimationActive = false;
static unsigned long startupAnimationStartedAt = 0;
static int lastStartupDotCount = 0;

// ---------- Компактный пиксельный шрифт 4x5 ----------
// Нужен только для строки клиентов: 5 пикселей на символ позволяют
// сохранить подпись и Wi-Fi-значок в узкой правой колонке.
static const uint8_t compactFont[][5] PROGMEM = {
  {0b0110, 0b1000, 0b1000, 0b1000, 0b0110}, // c
  {0b1000, 0b1000, 0b1000, 0b1000, 0b1110}, // l
  {0b0100, 0b0000, 0b0100, 0b0100, 0b0100}, // i
  {0b0110, 0b1001, 0b1110, 0b1000, 0b0111}, // e
  {0b1100, 0b1010, 0b1010, 0b1010, 0b1010}, // n
  {0b1110, 0b0100, 0b0100, 0b0100, 0b0010}, // t
  {0b0111, 0b1000, 0b0110, 0b0001, 0b1110}, // s
  {0b0000, 0b0100, 0b0000, 0b0100, 0b0000}, // :
  {0b0000, 0b0000, 0b0000, 0b0000, 0b0000}, // space
  {0b0110, 0b1001, 0b1001, 0b1001, 0b0110}, // 0
  {0b0100, 0b1100, 0b0100, 0b0100, 0b1110}, // 1
  {0b1110, 0b0001, 0b0110, 0b1000, 0b1111}, // 2
  {0b1110, 0b0001, 0b0110, 0b0001, 0b1110}, // 3
  {0b1010, 0b1010, 0b1111, 0b0010, 0b0010}, // 4
  {0b1111, 0b1000, 0b1110, 0b0001, 0b1110}, // 5
  {0b0110, 0b1000, 0b1110, 0b1001, 0b0110}, // 6
  {0b1111, 0b0001, 0b0010, 0b0100, 0b0100}, // 7
  {0b0110, 0b1001, 0b0110, 0b1001, 0b0110}, // 8
  {0b0110, 0b1001, 0b0111, 0b0001, 0b0110}  // 9
};

static int compactGlyphIndex(char c) {
  switch (c) {
    case 'c': return 0;
    case 'l': return 1;
    case 'i': return 2;
    case 'e': return 3;
    case 'n': return 4;
    case 't': return 5;
    case 's': return 6;
    case ':': return 7;
    case ' ': return 8;
    case '0': return 9;
    case '1': return 10;
    case '2': return 11;
    case '3': return 12;
    case '4': return 13;
    case '5': return 14;
    case '6': return 15;
    case '7': return 16;
    case '8': return 17;
    case '9': return 18;
    default: return 8;
  }
}

static void drawCompactText(int x, int y, const char *text) {
  while (*text) {
    const int glyphIndex = compactGlyphIndex(*text++);
    for (int row = 0; row < 5; row++) {
      const uint8_t bits = pgm_read_byte(&compactFont[glyphIndex][row]);
      for (int col = 0; col < 4; col++) {
        if (bits & (1U << (3 - col))) {
          display.drawPixel(x + col, y + row, SSD1306_WHITE);
        }
      }
    }
    x += 5;
  }
}

// ---------- Маленькие статусные пиктограммы ----------

static void drawBatteryIndicator(int x, int y, int percent) {
  percent = constrain(percent, 0, 100);

  // Корпус и контакт батареи.
  display.drawRect(x, y, 22, 13, SSD1306_WHITE);
  display.fillRect(x + 22, y + 4, 3, 5, SSD1306_WHITE);

  // Заполнение занимает до 16 пикселей внутренней ширины.
  const int fillWidth = (16 * percent + 50) / 100;
  if (fillWidth > 0) {
    display.fillRect(x + 3, y + 3, fillWidth, 7, SSD1306_WHITE);
  }
}

static void drawLightningIcon(int x, int y) {
  display.fillTriangle(x + 6, y, x + 1, y + 7, x + 6, y + 7, SSD1306_WHITE);
  display.fillTriangle(x + 5, y + 5, x + 2, y + 13, x + 10, y + 5, SSD1306_WHITE);
}

static void drawWifiIcon(int x, int y) {
  // Геометрическая пиктограмма Wi-Fi, рисуется прямо в буфер OLED.
  display.drawLine(x, y + 2, x + 2, y, SSD1306_WHITE);
  display.drawLine(x + 2, y, x + 5, y, SSD1306_WHITE);
  display.drawLine(x + 5, y, x + 7, y + 2, SSD1306_WHITE);

  display.drawLine(x + 2, y + 4, x + 3, y + 3, SSD1306_WHITE);
  display.drawLine(x + 3, y + 3, x + 4, y + 3, SSD1306_WHITE);
  display.drawLine(x + 4, y + 3, x + 5, y + 3, SSD1306_WHITE);
  display.drawLine(x + 5, y + 3, x + 6, y + 4, SSD1306_WHITE);

  display.drawPixel(x + 4, y + 6, SSD1306_WHITE);
  display.drawPixel(x + 4, y + 7, SSD1306_WHITE);
}

// ---------- Заставка и анимация запуска ----------

void showBootLogo() {
  if (!oledAvailable) return;

  display.clearDisplay();
  display.drawBitmap(
    (128 - OLED_BOOT_WIDTH) / 2,
    (64 - OLED_BOOT_HEIGHT) / 2,
    OLED_BOOT_LOGO,
    OLED_BOOT_WIDTH,
    OLED_BOOT_HEIGHT,
    SSD1306_WHITE
  );
  display.display();
}

void showInitialisingScreen() {
  if (!oledAvailable) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setCursor(19, 28);
  display.print("Initialising");
  display.display();
}

void startDisplayStartupAnimation() {
  if (!oledAvailable) return;

  startupAnimationStartedAt = millis();
  lastStartupDotCount = 0;
  startupAnimationActive = true;
}

bool updateDisplayStartup() {
  if (!oledAvailable || !startupAnimationActive) return false;

  const unsigned long elapsed = millis() - startupAnimationStartedAt;
  if (elapsed >= 1000) {
    startupAnimationActive = false;
    return false;
  }

  // Одна, две, три точки, затем цикл повторяется.
  const int dotCount = static_cast<int>((elapsed / 250) % 3) + 1;
  if (dotCount != lastStartupDotCount) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextWrap(false);
    display.setCursor(19, 28);
    display.print("Initialising");
    for (int i = 0; i < dotCount; i++) {
      display.print('.');
    }
    display.display();
    lastStartupDotCount = dotCount;
  }

  return true;
}

// ---------- Иконка текущего режима ----------

static void drawModeIcon() {
  const uint8_t *icon = OLED_ICON_TEST;
  uint8_t width = OLED_ICON_TEST_WIDTH;
  uint8_t height = OLED_ICON_TEST_HEIGHT;

  if (currentMode == MODE_TANK) {
    icon = OLED_ICON_TANK;
    width = OLED_ICON_TANK_WIDTH;
    height = OLED_ICON_TANK_HEIGHT;
  } else if (currentMode == MODE_CAR) {
    icon = OLED_ICON_CAR;
    width = OLED_ICON_CAR_WIDTH;
    height = OLED_ICON_CAR_HEIGHT;
  }

  // Левая половина экрана отдана активному режиму.
  display.drawBitmap(
    (64 - width) / 2,
    (64 - height) / 2,
    icon,
    width,
    height,
    SSD1306_WHITE
  );
}

// ---------- Экран критически низкого заряда ----------

static void drawLowBatteryScreen() {
  display.drawBitmap(
    (128 - OLED_ICON_LOW_BATTERY_WIDTH) / 2,
    4,
    OLED_ICON_LOW_BATTERY,
    OLED_ICON_LOW_BATTERY_WIDTH,
    OLED_ICON_LOW_BATTERY_HEIGHT,
    SSD1306_WHITE
  );

  display.setTextSize(1);
  display.setCursor((128 - 12 * 6) / 2, 38);
  display.print("low battery,");
  display.setCursor((128 - 13 * 6) / 2, 49);
  display.print("please charge");
}

// ---------- Рабочая панель OLED Update 2.0 ----------

void updateDisplay() {
  if (!oledAvailable) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  // При подтверждённом низком заряде заменяем панель состояния предупреждением.
  if (lowBattCount >= 3) {
    drawLowBatteryScreen();
    display.display();
    return;
  }

  // Приоритет остаётся у предупреждения низкого заряда.
  if (!motorBoardReady) {
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print("MOTOR");
    display.setCursor(0, 32);
    display.print("I2C ERR");
    display.display();
    return;
  }

  drawModeIcon();

  // Вертикально отделяем иконку режима от телеметрии.
  display.drawLine(64, 2, 64, 61, SSD1306_WHITE);

  // Верхняя зона: графический уровень заряда и проценты.
  drawBatteryIndicator(68, 8, battPct);
  display.setTextSize(1);
  display.setCursor(96, 10);
  display.printf("%d%%", constrain(battPct, 0, 100));

  display.drawLine(66, 31, 126, 31, SSD1306_WHITE);

  // Средняя зона: текущее напряжение.
  drawLightningIcon(69, 35);
  display.setCursor(82, 38);
  display.printf("%.1fV", battV);

  display.drawLine(66, 48, 126, 48, SSD1306_WHITE);

  // Нижняя зона: Wi-Fi-пиктограмма и число WebSocket-клиентов.
  drawWifiIcon(67, 52);
  char clientText[16];
  snprintf(clientText, sizeof(clientText), "clients: %u", static_cast<unsigned int>(ws.count()));
  drawCompactText(78, 53, clientText);

  display.display();
}
