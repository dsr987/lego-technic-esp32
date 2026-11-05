#pragma once

// Начальная заставка OLED.
void showBootLogo();

// Текст на время настройки Wi-Fi и остальных компонентов.
void showInitialisingScreen();

// Запустить и обновлять короткую анимацию Initialising...
void startDisplayStartupAnimation();
bool updateDisplayStartup();

// Временно включить или выключить OLED без сохранения состояния в NVS.
// Команда из WebSocket только ставится в очередь; I²C выполняется из loop().
bool setOledDisplayEnabled(bool enabled);
void processDisplayControl();

// Обновление рабочего экрана OLED.
void updateDisplay();
