#pragma once

// Начальная заставка OLED.
void showBootLogo();

// Текст на время настройки Wi-Fi и остальных компонентов.
void showInitialisingScreen();

// Запустить и обновлять короткую анимацию Initialising...
void startDisplayStartupAnimation();
bool updateDisplayStartup();

// Обновление рабочего экрана OLED.
void updateDisplay();
