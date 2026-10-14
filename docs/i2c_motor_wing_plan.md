# I2C Motor FeatherWing clone — migration plan

## Scope and branch boundary

This branch prepares a controlled migration from the four existing GPIO/LEDC motor outputs to a compact PCA9685 + 2× TB6612FNG motor board compatible with the Adafruit DC Motor + Stepper FeatherWing design.

- Branch: `feature/i2c-motor-wing`
- Base: `feature/web-gui-layered-car`
- This branch is intentionally independent of `feature/physical-light-control`. Do not merge the experimental GPIO5/GPIO15 light implementation into this branch automatically.
- First commit(s) should be documentation and architecture only. Do not modify the working motor implementation until the hardware interface and staged tests are agreed.
- No pull request or merge is created by this plan.

## Expected hardware

The purchased board is reported to have a PCA9685 PWM controller and two TB6612FNG dual H-bridge drivers. It is described as four bidirectional DC motor outputs (M1–M4), nominal motor supply 4.5–13.5 V, 1.2 A per bridge with 3 A brief peak. Treat vendor current claims as provisional until the exact clone and cooling are verified.

The official Adafruit design uses a 16-channel PCA9685 and four H-bridges. One addressable board defaults to I²C address `0x60`; solder jumpers A0–A4 select address offsets. The Adafruit documentation describes four DC motors or two stepper motors and notes that the logic side requires 3.3 V and GND. The clone must be checked against the actual PCB markings and trace routing; do not assume every clone is electrically identical.

References:
- Adafruit FeatherWing pinouts: https://learn.adafruit.com/adafruit-stepper-dc-motor-featherwing/pinouts
- Adafruit Arduino library guide: https://learn.adafruit.com/adafruit-stepper-dc-motor-featherwing/library-reference
- Adafruit Motor Shield V2 overview/specification: https://learn.adafruit.com/adafruit-motor-shield-v2-for-arduino

## Known PCA9685 channel map

The reported map matches the Adafruit/CircuitPython arrangement of a third TB6612 PWM input held HIGH while the two direction inputs are PWM-driven. The Adafruit Arduino Motor Shield V2 library uses the same three PCA9685 outputs per motor but abstracts them differently: it varies the dedicated PWM input for speed and drives IN1/IN2 as direction/state outputs.

| Logical motor | Board output | TB6612 PWM channel | IN1 channel | IN2 channel |
|---|---|---:|---:|---:|
| A | M1 | 8 | 10 | 9 |
| B | M2 | 13 | 11 | 12 |
| C | M3 | 2 | 4 | 3 |
| D | M4 | 7 | 5 | 6 |

Do not mix the two drive strategies in one implementation. Preferred first implementation: use the Arduino Adafruit Motor Shield V2 library API (motor speed plus direction/release) if the clone proves compatible. In that scheme the dedicated PWM input receives speed PWM and IN1/IN2 select direction/brake/release according to the library. Confirm the actual clone's mapping before relying on the library. A small direct-PCA9685 driver is a fallback only if the library is incompatible or obscures a necessary behavior.

The table above describes the reported clone's port/channel assignments. Verify against the physical board and official library source before the first motor is powered.

## Existing firmware interface to preserve

The existing higher-level motor API in `motor_control.h` is:
- `applyMotorA(int val)`
- `applyMotorB(int val)`
- `applyMotorC(int val)`
- `applyMotorD(int val)`
- `updateDriverStandby()`
- `stopAll()`

Keep these function names and their `-100..100` command semantics where practical. The WebSocket protocol, GUI, logical channel names, configured channel reversals, steering/servo, OLED, battery monitoring, diagnostic telemetry, and timeout handling should not need UI/protocol changes.

The current engine-simulation feature calculates motor B from motor A while in classic-car mode. It must continue to work with the new motor backend without applying the reverse setting twice or accidentally reusing a stale B command.

## Target architecture

1. Keep the current public `motor_control.h` API as the boundary between application logic and motor hardware.
2. Move PCA9685/driver-specific operations into a small implementation layer (or replace the internals of `motor_control.cpp` while keeping its interface stable). Avoid spreading raw PCA9685 channel numbers throughout `app.cpp`, WebSocket code, and GUI code.
3. Create one I²C bus on the existing ESP32 pins: SDA GPIO25 and SCL GPIO26. OLED is already on this bus, with reported address `0x3C`; the motor board's default `0x60` is distinct. Initialize `Wire` only once and pass the same `TwoWire` instance to the motor-board library.
4. Power the motor board's logic pins from the documented 3.3 V logic supply and connect common GND. Feed motor voltage to the separate motor power terminals within the actual board and motor ratings. Do not assume the motor terminal's positive supply also powers the ESP32 safely.
5. Initialize the motor controller after starting the I²C bus. If initialization fails, keep logical motor commands from activating outputs and report the failure through the existing diagnostics/logging path if feasible. Test behavior when the I²C device is absent.
6. At startup, explicitly command all four motors to zero/released state before the web control becomes active. Do not rely solely on power-up defaults.
7. Map signed input `-100..100` to motor speed `0..255`, with direction determined by sign and the existing per-motor reverse preference. A zero command must result in a defined stopped/released state.
8. Remove direct motor GPIO `pinMode`, `digitalWrite`, `ledcSetup`, `ledcAttachPin` and `ledcWrite` setup/control for the old two-TB6612 wiring only after the new backend is implemented and compile checks pass.
9. Retain the existing command watchdog and `stopAll()` path. Also account for the fault case below; successful normal-operation tests alone are not enough.

## I²C and motor-safety limitation to resolve

The current direct-driver design has an ESP32-controlled shared TB6612 STBY line. On a PCA9685-only interface, the last programmed PWM and direction outputs can remain present if the I²C bus or firmware stops responding. Therefore, the existing 500 ms command timeout does **not by itself guarantee a motor stop if I²C writes no longer reach the board**.

Before considering this migration complete, investigate how the actual clone exposes TB6612 STBY / output-enable or whether an independent hardware motor-power cutoff can be used. If no independent cutoff exists, document this limitation and restrict hardware tests to a setup where motor power can be disconnected immediately. Do not claim bus-loss fail-safe behavior until tested and supported by the hardware.

The motor driver's nominal current figure is not enough to approve compatibility. Verify each motor's stall/start current, shared supply capability, wiring, cooling, and board revision before prolonged or four-motor operation.

## Small-step implementation sequence

### Step 0 — branch and documentation
- Create this isolated branch from the current GUI branch.
- Record the board channel map, I²C address, API boundary, and known risks.
- Do not change motor code yet.

### Step 1 — compile-ready driver scaffold
- Add a small board-specific motor backend module and the library dependency in the appropriate build/CI install list.
- Keep the old implementation available until the new code compiles and its call contract is reviewed.
- Add checks for the channel map, unique initialization, default address, and safe initial motor values.
- Compile firmware and run the project auditor. This is a software-only gate, not hardware validation.

### Step 2 — controlled backend switch
- Switch the bodies of `applyMotorA-D`, `stopAll`, and driver readiness handling to the new backend.
- Preserve reverse settings and engine-simulation behavior.
- Keep changes out of WebSocket parsing and the GUI.
- Compile, review the diff, and run static checks before any bench connection.

### Step 3 — bench tests, one motor at a time
1. With motor supply disconnected, boot ESP32 and OLED; verify the PCA9685 is detected on `0x60`.
2. Verify an absent board does not hang boot or activate motor commands.
3. Connect one motor only. Start with a low command, verify zero, forward, reverse, and timeout stop.
4. Repeat separately for M2, M3, and M4. Confirm logical A/B/C/D match the physical ports.
5. Run the existing reversal controls and classic-car engine simulation.
6. Test all four motors together only after individual tests pass; monitor driver temperature and supply droop.
7. Test loss of WebSocket and loss of I²C separately. Verify the actual outcome instead of assuming both are protected identically.
8. Re-test steering, OLED refresh, battery measurement, diagnostics, Wi-Fi, OTA, and WebSocket reconnection.

### Step 4 — merge readiness
- Record the exact tested board revision, I²C address, supply voltage, motor configuration, and test result.
- Require ESP32 CI build and project auditor to pass.
- Require bench tests and a reviewed I²C fault-response plan.
- Only then consider a PR. No merge is implied by this plan.

## Non-goals for the first migration

- Do not move lights to remaining PCA9685 channels yet. The clone may not expose spare channels conveniently, and motor control must be validated first.
- Do not migrate steering servo output to PCA9685.
- Do not add MCP23017 or other extra I²C peripherals as part of the first motor-board migration.
- Do not change Web GUI layout, WebSocket command names, battery calibration, OLED behavior, or version/changelog metadata just to scaffold the branch.
