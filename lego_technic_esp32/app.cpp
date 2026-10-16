/*
 * LEGO Technic ESP32
 * Версия разработки: 0.9.1
 *
 * Текущий выпуск: калибровка измерения напряжения батареи.
 * В проекте также доступны обновлённый Web GUI и OLED Update 2.0.
 *
 * Совместимость:
 * - ESP32 Arduino Core 2.0.9
 * - ESPAsyncWebServer
 * - AsyncTCP
 * - Adafruit_SSD1306
 * - Adafruit_GFX
 * - ESP32Servo
 * - Preferences
 * - ElegantOTA
 *
 * История предыдущего выпуска:
 * 0.9.0 — OLED Update 2.0 ([PR #10](https://github.com/dsr987/lego-technic-esp32/pull/10))
 * - Добавлены растровые пиктограммы режимов и заставка проекта на OLED.
 * - Добавлена короткая неблокирующая анимация запуска «Initialising…».
 * - Рабочая панель OLED показывает режим, процент заряда и напряжение батареи, количество подключённых клиентов Wi-Fi и предупреждение о низком заряде.
 * - Если инициализация OLED не удалась, прошивка продолжает запуск без попыток выводить данные на недоступный дисплей.
 *
 * CHANGELOG_VERSION: 0.9.1
 * CHANGELOG_START
 * - Добавлен отдельный коэффициент калибровки напряжения батареи и скрипт расчёта поправки по показаниям мультиметра. ([PR #11](https://github.com/dsr987/lego-technic-esp32/pull/11))
 * CHANGELOG_END
 *
 */

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <ElegantOTA.h>

// ---------- Модули проекта ----------

#include "hardware_config.h"
#include "motor_control.h"
#include "web_page.h"
#include "web_control.h"
#include "battery_monitor.h"
#include "display_control.h"
#include "steering_control.h"
#include "preferences_manager.h"
