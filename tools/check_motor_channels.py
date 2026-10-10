
from pathlib import Path
import sys

source = Path("lego_technic_esp32/app.cpp")

if not source.exists():
    print(f"ERROR: source file not found: {source}")
    sys.exit(1)

code = source.read_text(encoding="utf-8")

checks = {
    "Motor A handler": "void applyMotorA(int val)",
    "Motor B handler": "void applyMotorB(int val)",
    "Motor C handler": "void applyMotorC(int val)",
    "Motor D handler": "void applyMotorD(int val)",
    "D command routing": 'strcmp(ch, "D")',
    "D PWM channel": "LEDC_CH_D",
    "D reverse preference": 'prefs.getUChar("revD", 0)',
    "D UI slider": 'id="slider-test-D"',
    "D heartbeat": "D:0",
    "Stop all four motors": "motorDVal = 0;",
    "WebSocket disconnect handler": "ws.onclose",
    "Disconnect resets commands": "zeroAll();",
    "Mode switch disables engine simulation": "if(m!==1)",
    "Mode command stops motors": "stopAll();",

}

failed = []

for name, marker in checks.items():
    passed = marker in code
    print(f"[{'PASS' if passed else 'FAIL'}] {name}")
    if not passed:
        failed.append(name)

if failed:
    print(f"\n{len(failed)} check(s) failed.")
    sys.exit(1)

print("\nAll basic source checks passed.")
