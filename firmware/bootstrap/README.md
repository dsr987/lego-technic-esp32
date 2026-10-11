# LEGO Technic ESP32 — initial USB flash / OTA bootstrap

This folder contains the first-start firmware for a classic ESP32 DevKit / ESP32-WROOM-32 board. It creates the same access point as the main project and exposes ElegantOTA, so after the first USB flash the main firmware can be uploaded through a browser.

## Files

- `LEGO_Technic_ESP32_Bootstrap/LEGO_Technic_ESP32_Bootstrap.ino` — bootstrap source sketch.
- `flash_bootstrap.bat` — Windows script that installs/configures Arduino CLI, installs the required board core/libraries, enables ElegantOTA async mode, compiles the sketch, and uploads it over USB.
- `install_arduino_cli.ps1` — downloads the official Windows x64 Arduino CLI if it is not installed.
- `patch_elegantota.ps1` — enables async mode in the installed ElegantOTA library.
- `prebuilt/esp32_ota_bootstrap.bin` — produced by the repository CI after a successful bootstrap build. This is an **application-only** binary, not a full blank-device flash image.

## First-time USB flash

1. Download or clone the repository, keeping the folder structure intact.
2. On Windows, connect the ESP32 DevKit to USB. Close Arduino IDE Serial Monitor and any other app that might hold the serial port.
3. Run `flash_bootstrap.bat` from this directory. It may need an internet connection to download Arduino CLI, the ESP32 Core, and libraries, but it does not require manually searching for or installing ElegantOTA.
4. Select the serial port shown for the ESP32, for example `COM3`.
5. If upload fails with a connection timeout, retry and put the board into download mode during connection (on many ESP32 DevKit boards, hold BOOT while upload starts, then release it when writing begins).
6. Wait for `SUCCESS`.

## Upload the main project through OTA

1. Connect a phone or computer to Wi-Fi `LegoTechnic`.
2. Wi-Fi password: `12345678`.
3. Open `http://192.168.4.1/update`.
4. OTA login: `admin`; OTA password: `admin`.
5. Upload the main project's compiled application file `build_firmware_for_esp32.bin` downloaded from a successful GitHub Actions firmware artifact.
6. Wait for upload/reboot, reconnect to the access point if needed, then open the main GUI at `http://192.168.4.1/`.

The bootstrap and main firmware are compiled for `esp32:esp32:esp32` using ESP32 Arduino Core `2.0.9` and the default partition scheme, so the OTA slots are expected to remain compatible. Do not change the target board or partition scheme on only one side without testing OTA compatibility.

## Important notes

- `prebuilt/esp32_ota_bootstrap.bin` is only the app image. Do **not** write it alone at flash offset `0x0` on a blank ESP32. The BAT script compiles and uploads the complete sketch using Arduino CLI, which chooses the correct flash offsets and required images.
- This process requires an internet connection for the first setup unless the Arduino CLI, core, and libraries are already cached on that computer.
- The AP and OTA credentials intentionally match the current main firmware. They are default project credentials and are not suitable for an untrusted environment. Change them in both the bootstrap sketch and main firmware before distributing a public hardware build more broadly.
- This script is for initial USB provisioning. Routine firmware updates should use ElegantOTA, after the main firmware is running.
