#pragma once

// Начальная заставка OLED.
void showBootLogo();

// Текст на время настройки Wi-Fi и остальных компонентов.
void showInitialisingScreen();

// Запустить и обновлять короткую анимацию Initialising...
void startDisplayStartupAnimation();
bool updateDisplayStartup();

// Временно включить или выключить OLED без сохранения состояния в NVS.
bool setOledDisplayEnabled(bool enabled);

// Обновление рабочего экрана OLED.
void updateDisplay();
