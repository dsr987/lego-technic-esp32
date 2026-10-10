
from pathlib import Path
import re
import sys


# ============================================================
# LEGO Technic ESP32 — Project Auditor
#
# Версия: 1.0
# Назначение:
#   Проверка структуры проекта и согласованности исходников.
#
# Скрипт ничего не исправляет автоматически.
# FAIL означает, что проверку необходимо разобрать.
# WARN означает потенциальную проблему, требующую внимания.
# ============================================================

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "lego_technic_esp32"
TOOLS = ROOT / "tools"

REQUIRED_FILES = [
    "app.cpp",
    "hardware_config.h",
    "motor_control.h",
    "motor_control.cpp",
    "web_page.h",
    "web_page.cpp",
    "web_control.h",
    "web_control.cpp",
    "battery_monitor.h",
    "battery_monitor.cpp",
    "display_control.h",
    "display_control.cpp",
    "steering_control.h",
    "steering_control.cpp",
    "preferences_manager.h",
    "preferences_manager.cpp",
]

MODULE_PAIRS = [
    ("motor_control.h", "motor_control.cpp"),
    ("web_page.h", "web_page.cpp"),
    ("web_control.h", "web_control.cpp"),
    ("battery_monitor.h", "battery_monitor.cpp"),
    ("display_control.h", "display_control.cpp"),
    ("steering_control.h", "steering_control.cpp"),
    ("preferences_manager.h", "preferences_manager.cpp"),
]

results = []
warnings = []


def report(name, passed, details=""):
    status = "PASS" if passed else "FAIL"
    results.append((status, name, details))
    suffix = f" — {details}" if details else ""
    print(f"[{status}] {name}{suffix}")


def warn(name, details):
    warnings.append((name, details))
    print(f"[WARN] {name} — {details}")


def read_source(relative_path):
    path = PROJECT / relative_path

    if not path.is_file():
        return None

    try:
        return path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeError) as exc:
        report(
            f"Read {relative_path}",
            False,
            str(exc),
        )
        return None


def extract_block(code, marker):
    """Find a brace-delimited block while ignoring comments and strings."""
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


def check_required_files():
    print("\n=== 1. Project structure ===")

    missing = []

    for relative_path in REQUIRED_FILES:
        exists = (PROJECT / relative_path).is_file()

        report(
            f"File exists: {relative_path}",
            exists,
        )

        if not exists:
            missing.append(relative_path)

    return not missing


def check_module_pairs():
    print("\n=== 2. Header/source module pairs ===")

    ok = True

    for header, source in MODULE_PAIRS:
        header_path = PROJECT / header
        source_path = PROJECT / source

        pair_exists = header_path.is_file() and source_path.is_file()

        report(
            f"Module pair: {header} + {source}",
            pair_exists,
        )

        if not pair_exists:
            ok = False
            continue

        source_code = read_source(source)

        if source_code is None:
            ok = False
            continue

        includes_header = (
            f'#include "{header}"' in source_code
        )

        report(
            f"{source} includes {header}",
            includes_header,
        )

        if not includes_header:
            ok = False

    return ok


def check_local_includes(sources):
    print("\n=== 3. Local includes ===")

    ok = True
    include_pattern = re.compile(
        r'^\s*#\s*include\s*"([^"]+)"',
        re.MULTILINE,
    )

    for relative_path, code in sources.items():
        if code is None:
            continue

        parent = (PROJECT / relative_path).parent

        for include_name in include_pattern.findall(code):
            candidates = [
                parent / include_name,
                PROJECT / include_name,
            ]

            if not any(candidate.is_file() for candidate in candidates):
                report(
                    f"Include in {relative_path}: {include_name}",
                    False,
                    "Local header not found",
                )
                ok = False

    if ok:
        report(
            "Local include references",
            True,
            "All quoted local headers were found",
        )

    return ok


def check_module_wiring(sources):
    print("\n=== 4. Main module wiring ===")

    app = sources.get("app.cpp") or ""

    expected_headers = [
        "hardware_config.h",
        "motor_control.h",
        "web_page.h",
        "web_control.h",
        "battery_monitor.h",
        "display_control.h",
        "steering_control.h",
        "preferences_manager.h",
    ]

    ok = True

    for header in expected_headers:
        found = f'#include "{header}"' in app

        report(
            f"app.cpp includes {header}",
            found,
        )

        if not found:
            ok = False

    return ok


def check_motor_control(sources):
    print("\n=== 5. Motor control ===")

    all_code = "\n".join(
        code for code in sources.values() if code is not None
    )

    web = sources.get("web_control.cpp") or ""
    motors = sources.get("motor_control.cpp") or ""
    config = sources.get("hardware_config.h") or ""
    page = sources.get("web_page.cpp") or ""

    ok = True

    required_functions = [
        "applyMotorA",
        "applyMotorB",
        "applyMotorC",
        "applyMotorD",
        "stopAll",
    ]

    for function in required_functions:
        found = re.search(
            rf"\b{re.escape(function)}\s*\(",
            all_code,
        ) is not None

        report(f"Function exists: {function}", found)

        if not found:
            ok = False

    handler = extract_block(web, "void handleWsMessage(")

    report(
        "WebSocket message handler",
        handler is not None,
    )

    if handler is None:
        ok = False
        handler = ""

    for channel in ("A", "B", "C", "D"):
        found = f'"{channel}"' in handler

        report(
            f"WebSocket channel {channel}",
            found,
        )

        if not found:
            ok = False

    pwm_d = "LEDC_CH_D" in config and "LEDC_CH_D" in motors

    report("Motor D PWM configuration", pwm_d)

    if not pwm_d:
        ok = False

    reverse_d = 'prefs.getUChar("revD"' in (
        sources.get("preferences_manager.cpp") or ""
    )

    report("Motor D reverse setting is loaded", reverse_d)

    if not reverse_d:
        ok = False

    ui_d = 'id="slider-test-D"' in page

    report("Motor D UI slider", ui_d)

    if not ui_d:
        ok = False

    heartbeat_d = "D:0" in page

    report("Motor D heartbeat/zero command", heartbeat_d)

    if not heartbeat_d:
        ok = False

    stop_all = extract_block(motors, "void stopAll()")

    report("stopAll implementation", stop_all is not None)

    if stop_all is None:
        ok = False
    else:
        for channel in ("A", "B", "C", "D"):
            found = re.search(
                rf"applyMotor{channel}\s*\(\s*0\s*\)",
                stop_all,
            ) is not None

            # Some implementations stop the bridges directly
            # instead of calling applyMotorX(0).
            if not found:
                found = re.search(
                    rf"LEDC_CH_{channel}\s*,\s*0",
                    stop_all,
                ) is not None

            report(
                f"stopAll stops motor {channel}",
                found,
            )

            if not found:
                ok = False

        engine_off = "engineSimOn = false" in stop_all

        report(
            "stopAll disables engine simulation",
            engine_off,
        )

        if not engine_off:
            ok = False

    return ok


def check_websocket_safety(sources):
    print("\n=== 6. WebSocket and failsafe ===")

    app = sources.get("app.cpp") or ""
    web = sources.get("web_control.cpp") or ""
    page = sources.get("web_page.cpp") or ""

    ok = True

    disconnect = extract_block(page, "ws.onclose = function()")

    report(
        "WebSocket disconnect handler",
        disconnect is not None,
    )

    if disconnect is None:
        ok = False
    else:
        resets = "zeroAll();" in disconnect
        reconnects = "setTimeout(initWS,1000);" in disconnect

        report("Disconnect resets browser commands", resets)
        report("WebSocket reconnect is scheduled", reconnects)

        if not resets or not reconnects:
            ok = False

    timeout_configured = (
        "CMD_TIMEOUT_MS" in app
        and "lastCmdMillis" in app
        and "stopAll();" in app
    )

    report(
        "Firmware command timeout/failsafe markers",
        timeout_configured,
    )

    if not timeout_configured:
        ok = False

    mode_handler = extract_block(web, "void handleWsMessage(")

    mode_ok = (
        mode_handler is not None
        and 'doc.containsKey("mode")' in mode_handler
        and "stopAll();" in mode_handler
    )

    report("Mode change stops existing commands", mode_ok)

    if not mode_ok:
        ok = False

    return ok


def check_status_json(sources):
    print("\n=== 7. Status JSON ===")

    app = sources.get("app.cpp") or ""
    status = extract_block(app, "void buildStatus(")

    if status is None:
        report("buildStatus implementation", False)
        return False

    report("buildStatus implementation", True)

    # Do not require one exact C++ string literal. Adjacent string
    # literals and formatting changes should not cause false failures.
    required_fields = [
        '"st"',
        '"v"',
        '"p"',
        '"r"',
        '"m"',
        '"c"',
        '"ra"',
        '"rb"',
        '"rc"',
        '"rd"',
        '"lf"',
        '"lr"',
        '"es"',
        '"tr"',
        '"md"',
    ]

    ok = True

    for field in required_fields:
        found = field in status

        report(
            f"Status JSON field {field}",
            found,
        )

        if not found:
            ok = False

    # Check ordering rather than requiring an exact separator.
    rd_pos = status.find(r'\"rd\"')
    lf_pos = status.find(r'\"lf\"')

    separator_ok = (
        rd_pos >= 0
        and lf_pos > rd_pos
        and "," in status[rd_pos:lf_pos]
    )

    report(
        "Status JSON rd/lf order and separator",
        separator_ok,
    )

    if not separator_ok:
        ok = False

    return ok


def check_preferences(sources):
    print("\n=== 8. Preferences ===")

    manager = sources.get("preferences_manager.cpp") or ""
    web = sources.get("web_control.cpp") or ""
    app = sources.get("app.cpp") or ""

    ok = True

    keys = {
        "revA": "UChar",
        "revB": "UChar",
        "revC": "UChar",
        "revD": "UChar",
        "ledF": "UChar",
        "ledR": "UChar",
        "steer_c": "Int",
        "steer_a": "Int",
    }

    for key, value_type in keys.items():
        getter = f'get{value_type}("{key}"'
        setter = f'put{value_type}("{key}"'

        loaded = getter in manager
        saved = setter in manager or setter in web

        report(f"Preference loaded: {key}", loaded)
        report(f"Preference saved: {key}", saved)

        if not loaded or not saved:
            ok = False

    begin_pos = app.find("prefs.begin(")
    load_pos = app.find("loadPreferences()")

    order_ok = (
        begin_pos >= 0
        and load_pos > begin_pos
    )

    report(
        "Preferences opened before loading",
        order_ok,
    )

    if not order_ok:
        ok = False

    return ok


def check_gpio_and_pwm(sources):
    print("\n=== 9. GPIO and PWM configuration ===")

    config = sources.get("hardware_config.h") or ""
    app = sources.get("app.cpp") or ""
    motors = sources.get("motor_control.cpp") or ""

    ok = True

    required_defines = [
        "TB_STBY",
        "TB_AIN1",
        "TB_AIN2",
        "TB_PWMA",
        "TB_BIN1",
        "TB_BIN2",
        "TB_PWMB",
        "TB2_AIN1",
        "TB2_AIN2",
        "TB2_PWMA",
        "TB2_BIN1",
        "TB2_BIN2",
        "TB2_PWMB",
        "LEDC_CH_A",
        "LEDC_CH_B",
        "LEDC_CH_C",
        "LEDC_CH_D",
    ]

    for name in required_defines:
        found = re.search(
            rf"^\s*#\s*define\s+{re.escape(name)}\s+\d+\b",
            config,
            re.MULTILINE,
        ) is not None

        report(f"Hardware define: {name}", found)

        if not found:
            ok = False

    pin_defines = dict(
        re.findall(
            r"^\s*#\s*define\s+((?:TB2?|TB)_?(?:STBY|AIN1|AIN2|BIN1|BIN2|PWMA|PWMB))\s+(\d+)\b",
            config,
            re.MULTILINE,
        )
    )

    # Explicit pin table to check physical GPIO collisions.
    pin_names = [
        "TB_STBY",
        "TB_AIN1", "TB_AIN2", "TB_PWMA",
        "TB_BIN1", "TB_BIN2", "TB_PWMB",
        "TB2_AIN1", "TB2_AIN2", "TB2_PWMA",
        "TB2_BIN1", "TB2_BIN2", "TB2_PWMB",
    ]

    values = {}

    for name in pin_names:
        match = re.search(
            rf"^\s*#\s*define\s+{re.escape(name)}\s+(\d+)\b",
            config,
            re.MULTILINE,
        )

        if match:
            values[name] = int(match.group(1))

    duplicates = {}

    for name, value in values.items():
        duplicates.setdefault(value, []).append(name)

    collisions = {
        pin: names
        for pin, names in duplicates.items()
        if len(names) > 1
    }

    report(
        "No duplicate GPIO assignments in motor configuration",
        not collisions,
        str(collisions) if collisions else "",
    )

    if collisions:
        ok = False

    # GPIO32/33 are intentionally assigned to TB6612FNG #2.
    for name, expected in (("TB2_BIN1", 32), ("TB2_BIN2", 33)):
        actual = values.get(name)

        found = actual == expected

        report(
            f"{name} uses GPIO{expected}",
            found,
        )

        if not found:
            ok = False

    led_noop = re.search(
        r"void\s+applyLeds\s*\(\s*\)\s*\{\s*(?:/\*.*?\*/\s*)?(?://[^\n]*\s*)*\}",
        app,
        re.DOTALL,
    ) is not None

    if not led_noop:
        warn(
            "applyLeds implementation",
            "Could not identify the empty placeholder automatically; inspect app.cpp manually.",
        )

    pwm_channels = {}

    for name in ("LEDC_CH_A", "LEDC_CH_B", "LEDC_CH_C", "LEDC_CH_D"):
        match = re.search(
            rf"^\s*#\s*define\s+{name}\s+(\d+)\b",
            config,
            re.MULTILINE,
        )

        if match:
            pwm_channels[name] = int(match.group(1))

    duplicate_pwm = len(set(pwm_channels.values())) != len(pwm_channels)

    report(
        "PWM channels are unique",
        not duplicate_pwm,
        str(pwm_channels),
    )

    if duplicate_pwm:
        ok = False

    return ok


def check_legacy_workflow_reference():
    print("\n=== 10. GitHub Actions workflow ===")

    workflows_dir = ROOT / ".github" / "workflows"

    if not workflows_dir.is_dir():
        warn(
            "Workflow directory",
            ".github/workflows was not found; check the repository layout.",
        )
        return True

    workflow_files = list(workflows_dir.glob("*.yml"))
    workflow_files += list(workflows_dir.glob("*.yaml"))

    if not workflow_files:
        warn(
            "Workflow files",
            "No .yml or .yaml files found in .github/workflows.",
        )
        return True

    old_reference = "check_motor_channels.py"
    new_reference = "project_auditor.py"
    found_new = False
    found_old = False

    for path in workflow_files:
        try:
            content = path.read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as exc:
            warn(f"Read workflow {path.name}", str(exc))
            continue

        if new_reference in content:
            found_new = True

        if old_reference in content:
            found_old = True
            warn(
                f"Old script reference in {path.name}",
                "Update the command to tools/project_auditor.py after adding the new file.",
            )

    report(
        "Workflow references project_auditor.py",
        found_new,
        "At least one workflow invokes the new auditor" if found_new else "",
    )

    if found_old:
        warn(
            "Old auditor reference",
            "Remove the old invocation once the new workflow command is in place.",
        )

    return True


def main():
    print("=" * 64)
    print("LEGO TECHNIC ESP32 — PROJECT AUDITOR")
    print(f"Repository: {ROOT}")
    print(f"Project:    {PROJECT}")
    print("=" * 64)

    structure_ok = check_required_files()

    # Continue checking whatever files are available, so one missing
    # file does not hide all other useful diagnostics.
    sources = {
        relative_path: read_source(relative_path)
        for relative_path in REQUIRED_FILES
    }

    check_module_pairs()
    check_local_includes(sources)
    check_module_wiring(sources)
    check_motor_control(sources)
    check_websocket_safety(sources)
    check_status_json(sources)
    check_preferences(sources)
    check_gpio_and_pwm(sources)
    check_legacy_workflow_reference()

    failed = [item for item in results if item[0] == "FAIL"]

    print("\n" + "=" * 64)
    print("AUDIT SUMMARY")
    print("=" * 64)
    print(f"PASS: {sum(1 for item in results if item[0] == 'PASS')}")
    print(f"FAIL: {len(failed)}")
    print(f"WARN: {len(warnings)}")

    if failed:
        print("\nFailed checks:")

        for _, name, details in failed:
            suffix = f" — {details}" if details else ""
            print(f" - {name}{suffix}")

        print(
            "\nAudit failed. Review each FAIL before changing source code."
        )
        return 1

    if not structure_ok:
        print("\nOne or more required source files are missing.")
        return 1

    print(
        "\nAll mandatory source checks passed. "
        "This does not replace compilation or hardware testing."
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
