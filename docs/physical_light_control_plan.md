# Physical light control — prototype implementation

## Scope

This branch adds WebSocket-controlled front/rear light states while aiming to leave motor control, steering, OLED, battery monitoring and the GUI design unchanged.

The current GPIO-based implementation is a software prototype, not a finalized hardware design:

- `LED_FRONT_PIN` and `LED_REAR_PIN` in `hardware_config.h` are temporary placeholders (currently GPIO5 and GPIO15).
- The final pins and electrical interface have not been selected.
- Possible implementations include direct ESP32 control where electrically appropriate, transistor/MOSFET switching, or PCA9685 outputs combined with suitable driver stages.
- `LED_ACTIVE_LOW` is also a temporary assumption and must match the eventual driver circuit.
- ESP32 light states are volatile. The controller no longer reads or writes NVS keys `ledF` and `ledR`; both lights start logically OFF after reset.

Do not treat the current pin numbers, polarity or output circuit as a hardware recommendation. Update the implementation after the lighting architecture is chosen.

## Current firmware behavior

1. WebSocket commands `ledF: 1` / `ledR: 1` lazily configure the corresponding placeholder GPIO as OUTPUT and assert its configured active level.
2. WebSocket commands `ledF: 0` / `ledR: 0` drive the inactive level only if that GPIO was initialized earlier.
3. The two channels are independent.
4. No motor-control, servo, OLED or battery-monitor paths are intentionally changed.
5. Pin selection, polarity and driver-stage behavior remain provisional until the physical design is decided.

## Required verification

### Automated and manual checks

- The current `ci-build-deploy.yml` workflow handles the configured automated build/check events; do not use the removed `pull-request-validation.yml` workflow.
- Keep `python3 tools/run_regression_checks.py` as the single additional manual software regression check before release.
- Review the diff for unrelated changes.

### Hardware tests after the lighting architecture is selected

1. Confirm the final GPIO/expander assignments against the actual schematic.
2. Verify voltage, current, polarity, and driver requirements for the selected light modules.
3. Verify front and rear ON/OFF independently and together.
4. Check for unintended flashes during startup, reset and firmware update.
5. Drive all four motors, move the steering servo, refresh OLED telemetry and use the WebSocket GUI while toggling the lights.
6. Watch for resets, Wi-Fi/WebSocket disconnects, motor glitches or other changes in behavior.

## Merge decision

The current implementation is a provisional software prototype. Before treating it as hardware-ready, choose the lighting architecture and update the GPIO assignments, polarity, driver interface and relevant documentation, then run software regression and bench tests on the actual hardware.
