@echo off
setlocal EnableExtensions
chcp 65001 >nul

set "SCRIPT_DIR=%~dp0"
set "SKETCH_DIR=%SCRIPT_DIR%LEGO_Technic_ESP32_Bootstrap"
set "SKETCH_NAME=LEGO_Technic_ESP32_Bootstrap"
set "DATA_DIR=%LOCALAPPDATA%\LegoTechnicESP32"
set "CONFIG_FILE=%DATA_DIR%\arduino-cli.yaml"
set "PORTABLE_CLI=%DATA_DIR%\arduino-cli.exe"

if not exist "%SKETCH_DIR%\%SKETCH_NAME%.ino" (
  echo [ОШИБКА] Не найден загрузочный скетч:
  echo         "%SKETCH_DIR%\%SKETCH_NAME%.ino"
  goto :fail
)

if not exist "%DATA_DIR%" mkdir "%DATA_DIR%"

where arduino-cli.exe >nul 2>&1
if not errorlevel 1 (
  set "ARDUINO_CLI=arduino-cli.exe"
) else (
  if not exist "%PORTABLE_CLI%" (
    echo Arduino CLI не найден. Загружается официальная версия для Windows x64...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%install_arduino_cli.ps1"
    if errorlevel 1 goto :fail
  )
  set "ARDUINO_CLI=%PORTABLE_CLI%"
)

if not exist "%CONFIG_FILE%" (
  "%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" config init --overwrite
  if errorlevel 1 goto :fail
)

"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" config set directories.data "%DATA_DIR%\data"
if errorlevel 1 goto :fail
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" config set directories.downloads "%DATA_DIR%\downloads"
if errorlevel 1 goto :fail
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" config set sketchbook.path "%DATA_DIR%\sketchbook"
if errorlevel 1 goto :fail

"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" config get board_manager.additional_urls 2>nul | findstr /I /C:"https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json" >nul
if errorlevel 1 (
  "%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" config add board_manager.additional_urls "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
  if errorlevel 1 goto :fail
)

echo.
echo [1/5] Обновление индекса пакетов Arduino...
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" core update-index
if errorlevel 1 goto :fail

echo.
echo [2/5] Установка ESP32 Arduino Core 2.0.9...
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" core install esp32:esp32@2.0.9
if errorlevel 1 goto :fail

echo.
echo [3/5] Установка библиотек проекта...
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" lib install "ESPAsyncWebServer@3.1.0"
if errorlevel 1 goto :fail
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" lib install "AsyncTCP@1.1.4"
if errorlevel 1 goto :fail
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" lib install "ElegantOTA"
if errorlevel 1 goto :fail

echo.
echo [4/5] Включение асинхронного режима ElegantOTA...
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%patch_elegantota.ps1" -Sketchbook "%DATA_DIR%\sketchbook"
if errorlevel 1 goto :fail

echo.
echo Доступные последовательные порты:
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" board list
if errorlevel 1 goto :fail

echo.
set "ESP32_PORT="
set /p "ESP32_PORT=Введи последовательный порт ESP32 (например, COM3): "
if not defined ESP32_PORT (
  echo [ОШИБКА] Последовательный порт не указан.
  goto :fail
)

echo.
echo [5/5] Компиляция и прошивка загрузчика OTA в порт %ESP32_PORT%...
echo Не отключай ESP32 от USB. Если загрузка не удалась, переведи плату в режим загрузки и повтори попытку.
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" compile --upload --fqbn esp32:esp32:esp32 --port "%ESP32_PORT%" "%SKETCH_DIR%"
if errorlevel 1 goto :fail

echo.
echo ============================================================
echo УСПЕХ: загрузчик OTA успешно прошит.
echo 1. Подключи телефон или компьютер к сети Wi-Fi: LegoTechnic
echo 2. Пароль Wi-Fi: 12345678
echo 3. Открой: http://192.168.4.1/update
echo 4. Логин OTA: admin    Пароль OTA: admin
echo 5. Загрузи основной бинарный файл прошивки .bin.
echo ============================================================
echo.
pause
exit /b 0

:fail
echo.
echo [ОШИБКА] Настройка загрузчика или прошивка через USB не удалась.
echo Проверь сообщение выше, COM-порт и драйвер USB-UART.
echo Если порт занят, закрой Serial Monitor в Arduino IDE и другие программы для работы с последовательным портом.
echo.
pause
exit /b 1
