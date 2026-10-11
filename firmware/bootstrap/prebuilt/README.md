The CI workflow writes `esp32_ota_bootstrap.bin` here after a successful bootstrap compile and commits it to the current branch.

The generated `.bin` is an application image for OTA upload to an already provisioned ESP32; it is not a complete flash image for a blank device. Use `../flash_bootstrap.bat` for first-time USB flashing.
