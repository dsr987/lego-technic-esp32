
from pathlib import Path
import sys

SOURCE_FILES = [
    Path("lego_technic_esp32/app.cpp"),
    Path("lego_technic_esp32/motor_control.cpp"),
    Path("lego_technic_esp32/hardware_config.h"),
]


def extract_block(code, marker):
    """Extract a brace-delimited block while ignoring strings and comments."""
    start = code.find(marker)
    if start < 0:
        return None

    opening = code.find("{", start + len(marker))
    if opening < 0:
        return None

    depth = 0
    state = "normal"
    quote = ""
    i = opening

    while i < len(code):
        ch = code[i]
        nxt = code[i + 1] if i + 1 < len(code) else ""

        if state == "line_comment":
            if ch == "\n":
                state = "normal"

        elif state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "normal"
                i += 1

        elif state == "string":
            if ch == "\\":
                i += 1
            elif ch == quote:
                state = "normal"

        else:
            if ch == "/" and nxt == "/":
                state = "line_comment"
                i += 1
            elif ch == "/" and nxt == "*":
                state = "block_comment"
                i += 1
            elif ch in ("'", '"', "`"):
                state = "string"
                quote = ch
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return code[opening:i + 1]

        i += 1

    return None


def main():
        missing = [path for path in SOURCE_FILES if not path.is_file()]

    if missing:
        for path in missing:
            print(f"ERROR: source file not found: {path}")
        return 1

    code = "\n".join(
        path.read_text(encoding="utf-8")
        for path in SOURCE_FILES
    )

    stop_all = extract_block(code, "void stopAll()")
    handler = extract_block(code, "void handleWsMessage(")
    switch_mode = extract_block(code, "function switchMode(m)")
    zero_all = extract_block(code, "function zeroAll()")
    disconnect = extract_block(code, "ws.onclose = function()")
    status = extract_block(code, "void buildStatus(")

    checks = {
        "Motor A handler": "void applyMotorA(int val)" in code,
        "Motor B handler": "void applyMotorB(int val)" in code,
        "Motor C handler": "void applyMotorC(int val)" in code,
        "Motor D handler": "void applyMotorD(int val)" in code,
        "D command routing": handler is not None
            and 'strcmp(ch, "D")' in handler,
        "D PWM channel": "LEDC_CH_D" in code,
        "D reverse preference": 'prefs.getUChar("revD", 0)' in code,
        "D UI slider": 'id="slider-test-D"' in code,
        "D heartbeat": "D:0" in code,

        "stopAll block found": stop_all is not None,
        "Stop motor A inside stopAll": stop_all is not None
            and "setDCBridge(TB_AIN1, TB_AIN2, LEDC_CH_A, 0);" in stop_all,
        "Stop motor B inside stopAll": stop_all is not None
            and "setDCBridge(TB_BIN1, TB_BIN2, LEDC_CH_B, 0);" in stop_all,
        "Stop motor C inside stopAll": stop_all is not None
            and "setDCBridge(TB2_AIN1, TB2_AIN2, LEDC_CH_C, 0);" in stop_all,
        "Stop motor D inside stopAll": stop_all is not None
            and "setDCBridge(TB2_BIN1, TB2_BIN2, LEDC_CH_D, 0);" in stop_all,
        "Engine simulation disabled in stopAll": stop_all is not None
            and "engineSimOn = false;" in stop_all,

        "WebSocket disconnect handler": disconnect is not None,
        "Disconnect resets commands": disconnect is not None
            and "zeroAll();" in disconnect,
        "Disconnect reconnects": disconnect is not None
            and "setTimeout(initWS,1000);" in disconnect,

        "Mode switch block found": switch_mode is not None,
        "Mode switch disables engine simulation": switch_mode is not None
            and "if(m!==1)" in switch_mode,
        "Mode switch sends mode": switch_mode is not None
            and "txWS({mode: m});" in switch_mode,
        "Mode switch resets commands": switch_mode is not None
            and "zeroAll();" in switch_mode,

        "zeroAll block found": zero_all is not None,
        "zeroAll resets A": zero_all is not None
            and "txWS({ch:'A', val:0})" in zero_all,
        "zeroAll resets B": zero_all is not None
            and "txWS({ch:'B', val:0})" in zero_all,
        "zeroAll resets C": zero_all is not None
            and "txWS({ch:'C', val:0})" in zero_all,
        "zeroAll resets D": zero_all is not None
            and "txWS({ch:'D', val:0})" in zero_all,
        "zeroAll resets steering": zero_all is not None
            and "txWS({ch:'S', val:0})" in zero_all,

        "Mode command calls stopAll": handler is not None
            and 'doc.containsKey("mode")' in handler
            and "stopAll();" in handler,
        "Status JSON block found": status is not None,
        "Status JSON rd/lf separator": status is not None
            and r'\"rd\":%d,\"lf\":%d' in status,
    }

    failed = []

    for name, passed in checks.items():
        print(f"[{'PASS' if passed else 'FAIL'}] {name}")
        if not passed:
            failed.append(name)

    if failed:
        print(f"\n{len(failed)} check(s) failed:")
        for name in failed:
            print(f" - {name}")
        return 1

    print(f"\nAll {len(checks)} source checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
