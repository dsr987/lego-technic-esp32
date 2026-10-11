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
  echo [ERROR] Cannot find bootstrap sketch:
  echo         "%SKETCH_DIR%\%SKETCH_NAME%.ino"
  goto :fail
)

if not exist "%DATA_DIR%" mkdir "%DATA_DIR%"

where arduino-cli.exe >nul 2>&1
if not errorlevel 1 (
  set "ARDUINO_CLI=arduino-cli.exe"
) else (
  if not exist "%PORTABLE_CLI%" (
    echo Arduino CLI not found. Downloading the official Windows x64 build...
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
echo [1/5] Refreshing Arduino package index...
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" core update-index
if errorlevel 1 goto :fail

echo.
echo [2/5] Installing ESP32 Arduino Core 2.0.9...
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" core install esp32:esp32@2.0.9
if errorlevel 1 goto :fail

echo.
echo [3/5] Installing libraries used by the project...
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" lib install "ESPAsyncWebServer@3.1.0"
if errorlevel 1 goto :fail
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" lib install "AsyncTCP@1.1.4"
if errorlevel 1 goto :fail
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" lib install "ElegantOTA"
if errorlevel 1 goto :fail

echo.
echo [4/5] Enabling ElegantOTA async mode...
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%patch_elegantota.ps1" -Sketchbook "%DATA_DIR%\sketchbook"
if errorlevel 1 goto :fail

echo.
echo Available serial ports:
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" board list
if errorlevel 1 goto :fail

echo.
set "ESP32_PORT="
set /p "ESP32_PORT=Enter the ESP32 serial port (for example COM3): "
if not defined ESP32_PORT (
  echo [ERROR] No serial port was entered.
  goto :fail
)

echo.
echo [5/5] Compiling and flashing the OTA bootstrap to %ESP32_PORT%...
echo Keep the ESP32 connected by USB. If upload fails, put the board into download mode and retry.
"%ARDUINO_CLI%" --config-file "%CONFIG_FILE%" compile --upload --fqbn esp32:esp32:esp32 --port "%ESP32_PORT%" "%SKETCH_DIR%"
if errorlevel 1 goto :fail

echo.
echo ============================================================
echo SUCCESS: OTA bootstrap flashed successfully.
echo 1. Connect your phone/computer to Wi-Fi: LegoTechnic
echo 2. Wi-Fi password: 12345678
echo 3. Open: http://192.168.4.1/update
echo 4. OTA login: admin    OTA password: admin
echo 5. Upload the main firmware application .bin file.
echo ============================================================
echo.
pause
exit /b 0

:fail
echo.
echo [ERROR] Bootstrap setup or USB flashing failed.
echo Check the message above, the COM port, and the USB-UART driver.
echo If the port is busy, close Arduino IDE Serial Monitor and other serial tools.
echo.
pause
exit /b 1
