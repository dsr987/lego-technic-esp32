# Add the OTA bootstrap to LEGO Technic ESP32

This bundle adds an initial USB provisioning sketch and a Windows BAT helper. The initial firmware creates the same `LegoTechnic` access point and starts ElegantOTA at `/update`; once that is flashed, the main project firmware can be uploaded from the browser.

## Files in this bundle

- `.github/workflows/ci-build-deploy.yml` — full updated main workflow. It builds the bootstrap source when changed, publishes a downloadable bootstrap artifact, and updates `firmware/bootstrap/prebuilt/esp32_ota_bootstrap.bin` in the branch after a successful build. It preserves the main firmware build and GitHub Pages deployment.
- `firmware/bootstrap/LEGO_Technic_ESP32_Bootstrap/LEGO_Technic_ESP32_Bootstrap.ino` — starter firmware source.
- `firmware/bootstrap/flash_bootstrap.bat` — Windows first-time USB flashing script.
- `firmware/bootstrap/install_arduino_cli.ps1` — installs the official Arduino CLI if it is not found on PATH.
- `firmware/bootstrap/patch_elegantota.ps1` — enables ElegantOTA async mode for ESPAsyncWebServer.
- `firmware/bootstrap/README.md` — instructions for initial flashing and switching to the main firmware through OTA.

## How to install into the repository

1. Open the repository branch `feature/four-dc-motors`.
2. Replace `.github/workflows/ci-build-deploy.yml` with the version from this bundle.
3. Upload the complete `firmware/bootstrap/` folder from this bundle to the same path in the repository, keeping folder names unchanged.
4. Commit the workflow and bootstrap files together. Because the workflow path filter includes `firmware/bootstrap/**`, the push should trigger a build.
5. Open Actions → `Build, Extract and Deploy`. Wait for the step `Compile OTA bootstrap firmware` and the artifact `esp32-ota-bootstrap-feature-four-dc-motors-<run>`.
6. After a successful bootstrap build, CI commits the application-only binary to `firmware/bootstrap/prebuilt/esp32_ota_bootstrap.bin`. If the commit does not appear, inspect the job log at `Commit prebuilt OTA bootstrap binary`.
7. Download the repository or copy `firmware/bootstrap/` to a Windows PC, run `flash_bootstrap.bat`, select the ESP32 COM port, and wait for success.
8. Connect to `LegoTechnic` (password `12345678`), open `http://192.168.4.1/update`, sign in with `admin` / `admin`, and upload the main project's application `.bin` from a successful firmware artifact.

## Important distinction

`esp32_ota_bootstrap.bin` is the compiled application image, not a full flash image for a blank device. The BAT script performs a normal Arduino CLI compile-and-upload over USB, which ensures the correct bootloader and partition details are used for initial provisioning. The GitHub Actions bootstrap artifact includes the full generated build-output folder for diagnostics/reference.

The workflow requires `contents: write` so it can commit the generated prebuilt binary to the current branch. Keep the existing `Project Auditor` workflow unchanged.
