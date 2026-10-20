# Physical light control — GPIO5/GPIO15 implementation

## Scope

This branch implements WebSocket-controlled front/rear lights using GPIO5 and GPIO15 while leaving motor control, steering, OLED, battery monitoring and the GUI design unchanged.

- Front light: GPIO5 (LED_FRONT_PIN).
- Rear light: GPIO15 (LED_REAR_PIN).
- The current implementation assumes an active-low, high-impedance logic input to an external LED driver. Verify this polarity and electrical interface against the actual lighting circuit before connecting it.
- ESP32 light states are volatile. The controller no longer reads or writes NVS keys ledF and ledR; both lights start logically OFF after reset.
- GPIOs are not configured or written during setup(). The first GPIO write/configuration occurs only after the WebSocket receives an ON command for that light. An OFF command before the first ON leaves the pin untouched.
- Once a pin has been activated, an OFF command drives the configured inactive level.

## Critical hardware caveat

GPIO5 and GPIO15 are strapping pins on the classic ESP32. Deferring pinMode() until a WebSocket ON command does not remove their hardware strap behavior during reset and boot. The attached circuit must not force an incompatible level at reset. Check the actual ESP32 module, board schematic, driver input impedance and pull resistors. If cold boot, reset, firmware upload or OTA recovery becomes unreliable, do not merge this branch; use the I²C-expander option instead.

The LED load must not be powered directly from an ESP32 GPIO. Use suitable resistors for low-current indicator LEDs or a transistor/MOSFET driver for the actual light load, with a common ground where required.

## Firmware behavior

1. WebSocket commands ledF: 1 / ledR: 1 lazily configure only the requested GPIO as OUTPUT and then assert its active level.
2. WebSocket commands ledF: 0 / ledR: 0 drive the inactive level only if that GPIO was initialized earlier.
3. The two channels are independent.
4. No motor-control, servo, OLED or battery-monitor paths are intentionally changed.
5. LED_ACTIVE_LOW in hardware_config.h defines the assumed output polarity; change it only after verifying the real driver circuit.

## Required verification

### Automated and manual checks

- The current `ci-build-deploy.yml` workflow handles the configured automated build/check events; do not use the removed `pull-request-validation.yml` workflow.
- Keep `python3 tools/run_regression_checks.py` as the single additional manual software regression check before release.
- Review the diff for unrelated changes.

### Bench / hardware tests before any merge

1. Cold boot repeatedly with the LED driver physically connected and disconnected where safe.
2. Verify reset, serial/programming recovery and OTA update behavior.
3. Verify both lights remain OFF after boot even if they were ON before power was removed.
4. Verify front ON/OFF, rear ON/OFF, and both ON/OFF independently.
5. Check for unintended flashes during boot and reset.
6. Drive all four motors, move the steering servo, refresh OLED telemetry and use the WebSocket GUI while toggling the lights.
7. Watch for ESP32 resets, Wi-Fi/WebSocket disconnects, motor glitches or other changes in behavior.

## Merge decision

Do not merge this branch until the bench tests pass on the actual hardware. Any boot, programming, OTA or runtime issue involving GPIO5/GPIO15 is a stop condition. In that case, preserve the working branch and implement the lights through an I²C GPIO expander when that hardware is available.
