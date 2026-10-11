#!/usr/bin/env python3
"""Structural and regression-safety checks for LEGO Technic ESP32."""

from pathlib import Path
import re
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
SOURCE_DIR = REPO_ROOT / "lego_technic_esp32"

MODULES = (
    "motor_control",
    "web_page",
    "web_control",
    "battery_monitor",
    "display_control",
    "steering_control",
    "preferences_manager",
)

REQUIRED_FILES = (
    "app.cpp",
    "hardware_config.h",
    *(f"{name}.h" for name in MODULES),
    *(f"{name}.cpp" for name in MODULES),
)

PASS_COUNT = 0
FAIL_COUNT = 0
WARN_COUNT = 0


def report(status: str, message: str) -> None:
    global PASS_COUNT, FAIL_COUNT, WARN_COUNT
    print(f"[{status}] {message}")
    if status == "PASS":
        PASS_COUNT += 1
    elif status == "FAIL":
        FAIL_COUNT += 1
    elif status == "WARN":
        WARN_COUNT += 1


def read_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError):
        return None


def check_required_files() -> bool:
    if not SOURCE_DIR.is_dir():
        report("FAIL", f"Source directory not found: {SOURCE_DIR.relative_to(REPO_ROOT)}")
        return False

    report("PASS", "Source directory exists")
    for relative_name in REQUIRED_FILES:
        if (SOURCE_DIR / relative_name).is_file():
            report("PASS", f"Required file: {relative_name}")
        else:
            report("FAIL", f"Missing required file: {relative_name}")
    return True


def resolve_local_include(source_file: Path, include_name: str) -> bool:
    candidates = (
        source_file.parent / include_name,
        SOURCE_DIR / include_name,
        SOURCE_DIR / "include" / include_name,
        REPO_ROOT / include_name,
        REPO_ROOT / "include" / include_name,
    )
    return any(candidate.is_file() for candidate in candidates)


def check_local_includes() -> None:
    include_pattern = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
    source_files = list(SOURCE_DIR.rglob("*.h")) + list(SOURCE_DIR.rglob("*.cpp"))

    for source_file in sorted(set(source_files)):
        content = read_text(source_file)
        if content is None:
            report("FAIL", f"Cannot read: {source_file.name}")
            continue

        for include_name in include_pattern.findall(content):
            include_name = include_name.replace("\\", "/")
            if resolve_local_include(source_file, include_name):
                report("PASS", f'Include "{include_name}" in {source_file.name}')
            else:
                report(
                    "FAIL",
                    f'Missing local include "{include_name}" in {source_file.relative_to(REPO_ROOT)}',
                )


def check_module_pairs() -> None:
    for module in MODULES:
        header = SOURCE_DIR / f"{module}.h"
        implementation = SOURCE_DIR / f"{module}.cpp"
        if not header.is_file() or not implementation.is_file():
            continue

        content = read_text(implementation)
        if content is None:
            report("FAIL", f"Cannot read {implementation.name}")
            continue

        included_headers = re.findall(
            r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', content, re.MULTILINE
        )
        if any(Path(name).name == header.name for name in included_headers):
            report("PASS", f"{implementation.name} includes {header.name}")
        else:
            report("FAIL", f"{implementation.name} does not include its own header {header.name}")


def check_app_entry_points() -> None:
    app_content = read_text(SOURCE_DIR / "app.cpp")
    if app_content is None:
        report("FAIL", "Cannot read app.cpp")
        return

    for function_name in ("setup", "loop"):
        if re.search(rf"\bvoid\s+{function_name}\s*\(", app_content):
            report("PASS", f"app.cpp contains {function_name}()")
        else:
            report("FAIL", f"app.cpp is missing {function_name}()")


def check_motor_safety_anchor() -> None:
    motor_content = read_text(SOURCE_DIR / "motor_control.cpp")
    if motor_content is None:
        report("FAIL", "Cannot read motor_control.cpp")
        return

    if re.search(r"\bvoid\s+stopAll\s*\(", motor_content):
        report("PASS", "motor_control.cpp contains stopAll()")
    else:
        report("FAIL", "motor_control.cpp is missing void stopAll()")


def check_gui_heartbeat_safety() -> None:
    """Heartbeat must keep the watchdog alive without replaying old slider values."""
    page = read_text(SOURCE_DIR / "web_page.cpp")
    if page is None:
        report("FAIL", "Cannot read web_page.cpp for heartbeat regression checks")
        return

    interval_pattern = re.compile(
        r"setInterval\s*\(\s*function\s*\(\s*\)\s*\{(.*?)\}\s*,\s*150\s*\)\s*;",
        re.DOTALL,
    )
    intervals = interval_pattern.findall(page)
    safe_intervals = [
        block for block in intervals
        if re.search(r"ws\s*\.\s*send\s*\(\s*JSON\s*\.\s*stringify\s*\(\s*\{\s*hb\s*:\s*1\s*\}\s*\)\s*\)", block)
    ]

    if not safe_intervals:
        report("FAIL", "GUI heartbeat must send only {hb:1} every 150 ms")
    elif any(re.search(r"lastSent|txWS\s*\(\s*\{\s*ch\s*:", block) for block in safe_intervals):
        report("FAIL", "GUI heartbeat appears to replay previous motor/servo values")
    else:
        report("PASS", "GUI heartbeat sends a watchdog-only {hb:1} message")

    # A freshly opened/reconnected socket must not restore the former joystick state.
    onopen = re.search(r"ws\s*\.\s*onopen\s*=\s*function\s*\(\s*\)\s*\{(.*?)\n\s*\};\s*\n\s*ws\s*\.\s*onclose", page, re.DOTALL)
    if onopen and all(token in onopen.group(1) for token in ("eng = false", "zeroAll()", "txWS({eng:0})")):
        report("PASS", "WebSocket reconnect clears old controls and engine simulation")
    else:
        report("FAIL", "WebSocket onopen must clear controls and send eng=0")


def check_gui_engine_guards() -> None:
    page = read_text(SOURCE_DIR / "web_page.cpp")
    if page is None:
        report("FAIL", "Cannot read web_page.cpp for engine-simulation checks")
        return

    # The engine simulation only makes sense in classic-car mode with at least two motors.
    button_guard = re.search(
        r"\$\s*\(\s*['\"]eng-btn['\"]\s*\)\s*\.\s*onclick\s*=\s*function\s*\(\s*\)\s*\{\s*"
        r"if\s*\(\s*motorCount\s*<\s*2\s*\|\|\s*mode\s*!==\s*1\s*\)\s*return\s*;",
        page,
    )
    if button_guard:
        report("PASS", "Engine button is guarded by motor count and classic-car mode")
    else:
        report("FAIL", "Engine button must reject motorCount < 2 or mode !== 1")

    mode_guard = re.search(
        r"if\s*\(\s*m\s*!==\s*1\s*&&\s*eng\s*\)\s*\{"
        r"(?:(?!\n\s*\}).)*?txWS\s*\(\s*\{\s*eng\s*:\s*0\s*\}\s*\)\s*;",
        page,
        re.DOTALL,
    )
    if mode_guard:
        report("PASS", "Leaving classic-car mode disables engine simulation")
    else:
        report("FAIL", "switchMode() must send eng=0 when leaving classic-car mode")

    low_motor_guard = re.search(
        r"if\s*\(\s*motorCount\s*<\s*2\s*\)\s*\{"
        r"(?:(?!\n\s*\}).)*?eng\s*=\s*false\s*;"
        r"(?:(?!\n\s*\}).)*?txWS\s*\(\s*\{\s*eng\s*:\s*0\s*\}\s*\)\s*;"
        r"(?:(?!\n\s*\}).)*?txWS\s*\(\s*\{\s*ch\s*:\s*['\"]B['\"]\s*,\s*val\s*:\s*0\s*\}\s*\)\s*;",
        page,
        re.DOTALL,
    )
    if low_motor_guard:
        report("PASS", "Single-motor configuration disables engine simulation and zeros B")
    else:
        report("FAIL", "motorCount < 2 must disable engine simulation and zero motor B")


def check_firmware_heartbeat_and_watchdog() -> None:
    control = read_text(SOURCE_DIR / "web_control.cpp")
    app = read_text(SOURCE_DIR / "app.cpp")
    if control is None:
        report("FAIL", "Cannot read web_control.cpp for heartbeat regression checks")
        return
    if app is None:
        report("FAIL", "Cannot read app.cpp for watchdog regression checks")
        return

    heartbeat_branch = re.search(
        r"if\s*\(\s*doc\.containsKey\(\s*['\"]hb['\"]\s*\)\s*\)\s*\{(.*?)\n\s*\}\s*\n\s*//\s*Переключение режима",
        control,
        re.DOTALL,
    )
    if heartbeat_branch:
        hb = heartbeat_branch.group(1)
        checks = (
            re.search(r"doc\.size\(\)\s*!=\s*1", hb),
            re.search(r"doc\s*\[\s*['\"]hb['\"]\s*\]\s*\.\s*is<int>\(\)", hb),
            re.search(r"doc\s*\[\s*['\"]hb['\"]\s*\]\s*\.\s*as<int>\(\)\s*!=\s*1", hb),
            re.search(r"lastCmdMillis\s*=\s*millis\(\)", hb),
            re.search(r"\breturn\s*;", hb),
        )
        touches_outputs = re.search(r"\bapplyMotor[A-D]?\s*\(|\bapplySteer\s*\(|\bstopAll\s*\(", hb)
        if all(checks) and not touches_outputs:
            report("PASS", "Firmware accepts only a standalone {hb:1} watchdog message")
        else:
            report("FAIL", "Firmware heartbeat must validate {hb:1}, refresh timeout and return without driving outputs")
    else:
        report("FAIL", "web_control.cpp is missing the isolated heartbeat handler")

    timeout_ok = re.search(r"const\s+unsigned\s+long\s+CMD_TIMEOUT_MS\s*=\s*500\s*;", app)
    watchdog_ok = re.search(
        r"if\s*\(\s*millis\(\)\s*-\s*lastCmdMillis\s*>\s*CMD_TIMEOUT_MS\s*\)\s*\{"
        r"(?:(?!\n\s*\}).)*?stopAll\s*\(\s*\)\s*;",
        app,
        re.DOTALL,
    )
    if timeout_ok and watchdog_ok:
        report("PASS", "Firmware retains the 500 ms command-timeout motor stop")
    else:
        report("FAIL", "Firmware must stop all outputs when the 500 ms command timeout expires")


def main() -> int:
    print("=" * 64)
    print("LEGO TECHNIC ESP32 — PROJECT AUDITOR")
    print("=" * 64)
    print(f"Repository: {REPO_ROOT}")
    print(f"Source:     {SOURCE_DIR}")
    print()

    source_exists = check_required_files()
    if source_exists:
        print("\n--- Local include checks ---")
        check_local_includes()
        print("\n--- Module checks ---")
        check_module_pairs()
        print("\n--- Application checks ---")
        check_app_entry_points()
        check_motor_safety_anchor()
        print("\n--- Control-safety regression checks ---")
        check_gui_heartbeat_safety()
        check_gui_engine_guards()
        check_firmware_heartbeat_and_watchdog()

    print()
    print("=" * 64)
    print("AUDIT SUMMARY")
    print("=" * 64)
    print(f"PASS: {PASS_COUNT}")
    print(f"FAIL: {FAIL_COUNT}")
    print(f"WARN: {WARN_COUNT}")

    if FAIL_COUNT:
        print("\nAudit failed. Fix the reported structural or safety-regression errors.")
        return 1

    print("\nAudit passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
