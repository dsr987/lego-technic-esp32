# Physical light control — implementation plan

## Goal

Implement physical front and rear light switching while keeping the existing WebSocket commands `ledF` / `ledR` and NVS keys `ledF` / `ledR` compatible. Avoid changing motor control, steering, OLED, and battery-monitor logic.

## Current software state

- `web_control.cpp` already accepts `ledF` and `ledR`, updates `ledFrontOn` / `ledRearOn`, persists the settings, and calls `applyLeds()`.
- `preferences_manager.cpp` already saves and loads those two logical states.
- `app.cpp` currently has an empty `applyLeds()`.
- GPIO assignment is centralized in `hardware_config.h`, apart from servo and OLED definitions currently in `app.cpp`.
- No physical LED GPIOs have been assigned in this planning stage.

## GPIO inventory from the current feature branch

| GPIO | Current use / constraint |
| ---: | --- |
| 2 | PWM for motor D |
| 4 | Shared TB6612FNG STBY |
| 13, 14, 23 | Motor C control |
| 16, 17, 18 | Motor A control |
| 19, 21, 22 | Motor B control |
| 25, 26 | OLED I²C SDA/SCL |
| 27 | Steering servo |
| 32, 33 | Motor D direction |
| 34 | Battery voltage ADC input; input-only |
| 35, 36, 39 | Input-only on the ESP32-WROOM-32 family |
| 6–11 | Reserved for module flash; don't use |

That leaves GPIO 0, 1, 3, 5, 12 and 15 as nominally unassigned output-capable pins, but all have meaningful boot/serial caveats:
- GPIO0 controls boot/download mode: avoid.
- GPIO12 controls the flash-supply strapping option on classic ESP32: strongly avoid.
- GPIO1/GPIO3 are UART0 TX/RX used by serial upload and recovery: avoid unless the serial recovery path is intentionally sacrificed and electrically verified.
- GPIO5 and GPIO15 are strapping pins sampled during reset. They are the remaining direct-GPIO candidates, but external circuitry must not force their levels during reset. GPIO5 has a default pull-up and participates in SDIO timing strapping; GPIO15 has a default pull-up and affects boot-log output.

## Decision needed before assigning pins

### Option A — use GPIO5 and GPIO15 for two active-low, high-impedance driver inputs
Pros: no additional digital module, no rewiring of current motor/OLED/servo pins.
Cons: both pins are strapping pins. The actual LED-driver schematic, input/pull resistor network, and behavior during cold boot must be checked before approving this option. Do not connect LED loads directly to ESP32 GPIOs.

### Option B — add an I²C GPIO expander
Pros: avoids using boot-strapping pins and preserves current motor and peripheral assignments; OLED already uses an I²C bus on GPIO25/26.
Cons: requires another hardware module, confirms its address does not conflict with other I²C devices, additional wiring, driver transistors/MOSFETs, and code. Output state during expander reset must be designed to leave lights off.

### Option C — reassign existing peripheral pins
Possible only with explicit hardware rewiring and an updated pinout. This is not the default choice because existing pins are heavily allocated, and strapping/UART pins should not be consumed casually.

## Electrical requirements

- Do not power high-current LEDs/strips directly from an ESP32 GPIO.
- Identify LED supply voltage and maximum current first.
- Use an appropriate series resistor for low-current LEDs or a transistor/MOSFET driver for larger loads.
- Ensure common ground if the LED supply and ESP32 supply are separate.
- Design hardware defaults so front and rear lights remain OFF while the ESP32 resets or boots.
- Confirm whether the light module is active-high or active-low before coding `applyLeds()`.

## Planned implementation sequence

1. Confirm board/module variant and the front/rear LED supply, current, and driver circuit.
2. Select pins/interface and write the final pin definitions in `hardware_config.h`.
3. Initialize both outputs to the OFF state early in `setup()`, without disturbing motor-driver safe initialization.
4. Implement `applyLeds()` using the existing persisted logical states.
5. Verify WebSocket and saved settings still operate without changing their keys or protocol.
6. Extend `tools/project_auditor.py` to check pin definitions, safe initialization and `applyLeds()` implementation.
7. Run unit/static checks and ESP32 build in CI; then test both lights, reboot behavior, OTA, and all motor channels on hardware.

No GPIO selection or code change to the light outputs is made until Option A vs Option B is chosen and the physical driver/load details are known.
