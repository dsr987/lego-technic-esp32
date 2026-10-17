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
    "i2c_motor_backend",
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



def check_tank_visual_regressions() -> None:
    """Protect the reviewed tank GUI from arrow, tint, layering and sync regressions."""
    html = read_text(REPO_ROOT / "index.html")
    page = read_text(SOURCE_DIR / "web_page.cpp")
    if html is None:
        report("FAIL", "Cannot read index.html for tank visual regression checks")
        return
    if page is None:
        report("FAIL", "Cannot read web_page.cpp for tank visual regression checks")
        return

    embedded_start = page.find("<!DOCTYPE html>")
    embedded_end = page.find(')HTML";', embedded_start)
    if embedded_start < 0 or embedded_end < 0:
        report("FAIL", "Cannot locate embedded PAGE_HTML boundaries in web_page.cpp")
    else:
        embedded_html = page[embedded_start:embedded_end]
        if embedded_html == html.lstrip():
            report("PASS", "index.html exactly matches embedded PAGE_HTML")
        else:
            report("FAIL", "index.html differs from embedded PAGE_HTML")

    arrow_pattern = re.compile(
        r'track-direction-markers|id=["\']t-(?:left|right)-(?:f|r)["\']'
    )
    if not arrow_pattern.search(html):
        report("PASS", "Tank direction arrows remain removed")
    else:
        report("FAIL", "Tank direction-arrow markup has returned")

    required_tank_tokens = (
        'id="t-left-glow"',
        'id="t-right-glow"',
        'id="tank-left-tint-filter"',
        'id="tank-right-tint-filter"',
        'id="tank-left-tint-flood"',
        'id="tank-right-tint-flood"',
        'overflow:visible}',
    )
    for token in required_tank_tokens:
        if token in html:
            report("PASS", f"Tank visual layer present: {token}")
        else:
            report("FAIL", f"Tank visual layer missing: {token}")

    paint_match = re.search(
        r"function\s+paintTank\s*\(\s*L\s*,\s*R\s*\)\s*\{(.*?)"
        r"\n\s*\}\s*\n\s*function\s+paintCar\s*\(",
        html,
        re.DOTALL,
    )
    if not paint_match:
        report("FAIL", "Cannot locate paintTank() for visual direction checks")
        return

    paint = paint_match.group(1)
    direction_patterns = (
        (r"leftForward\s*=\s*L\s*>\s*8", "left forward threshold"),
        (r"leftReverse\s*=\s*L\s*<\s*-8", "left reverse threshold"),
        (r"rightForward\s*=\s*R\s*>\s*8", "right forward threshold"),
        (r"rightReverse\s*=\s*R\s*<\s*-8", "right reverse threshold"),
        (
            r"leftFlood\.setAttribute\(\s*['\"]flood-color['\"]\s*,\s*leftReverse\s*\?\s*red\s*:\s*green\s*\)",
            "left track uses green forward and red reverse",
        ),
        (
            r"rightFlood\.setAttribute\(\s*['\"]flood-color['\"]\s*,\s*rightReverse\s*\?\s*red\s*:\s*green\s*\)",
            "right track uses green forward and red reverse",
        ),
        (
            r"leftGlow\.setAttribute\(\s*['\"]opacity['\"]\s*,\s*\(leftForward\s*\|\|\s*leftReverse\)\s*\?\s*['\"]0\.58['\"]\s*:\s*['\"]0['\"]\s*\)",
            "left track tint is translucent and off at rest",
        ),
        (
            r"rightGlow\.setAttribute\(\s*['\"]opacity['\"]\s*,\s*\(rightForward\s*\|\|\s*rightReverse\)\s*\?\s*['\"]0\.58['\"]\s*:\s*['\"]0['\"]\s*\)",
            "right track tint is translucent and off at rest",
        ),
    )
    for pattern, description in direction_patterns:
        if re.search(pattern, paint):
            report("PASS", description)
        else:
            report("FAIL", f"Tank lighting regression: {description}")

    if "drop-shadow" not in paint and "style.filter" not in paint:
        report("PASS", "Tank lighting uses translucent fills instead of contour-only shadows")
    else:
        report("FAIL", "Tank lighting reverted to contour-only shadow effects")

    left_track = html.find('<g id="t-left-group">')
    right_track = html.find('<g id="t-right-group">')
    chassis = html.find('<image id="crawler-body"')
    if 0 <= left_track < right_track < chassis:
        report("PASS", "Both tank tracks are rendered beneath the crawler body")
    else:
        report("FAIL", "Tank layer order must be left track, right track, then crawler body")



def check_turn_signal_regressions() -> None:
    """Keep front/rear turn signals attached to the four original light positions."""
    html = read_text(REPO_ROOT / "index.html")
    if html is None:
        report("FAIL", "Cannot read index.html for turn-signal regression checks")
        return

    original_lamps = (
        (
            r'<ellipse\s+id="hl-fl"\s+class="vehicle-light"\s+cx="71"\s+cy="57"\s+rx="15"\s+ry="11"\s+fill="url\(#front-headlight-gradient\)"',
            "front-left headlight glow matches the car artwork",
        ),
        (
            r'<ellipse\s+id="hl-fr"\s+class="vehicle-light"\s+cx="188"\s+cy="57"\s+rx="15"\s+ry="11"\s+fill="url\(#front-headlight-gradient\)"',
            "front-right headlight glow matches the car artwork",
        ),
        (
            r'<ellipse\s+id="hl-rl"\s+class="vehicle-light"\s+cx="41"\s+cy="386"\s+rx="17"\s+ry="11"\s+fill="url\(#rear-headlight-gradient\)"',
            "rear-left light glow matches the car artwork",
        ),
        (
            r'<ellipse\s+id="hl-rr"\s+class="vehicle-light"\s+cx="211"\s+cy="386"\s+rx="17"\s+ry="11"\s+fill="url\(#rear-headlight-gradient\)"',
            "rear-right light glow matches the car artwork",
        ),
    )
    for pattern, description in original_lamps:
        if re.search(pattern, html):
            report("PASS", description)
        else:
            report("FAIL", f"Turn-signal regression: {description}")

    if not re.search(r'id="turn-signal-(?:left|right)"', html):
        report("PASS", "Turn signals reuse original headlight SVG elements")
    else:
        report("FAIL", "Extra turn-signal overlays may misalign with the original lights")

    required_calls = (
        (r"paintSignalLamp\(\$\(['\"]hl-fl['\"]\),\s*turningLeft", "left front indicator follows left turn"),
        (r"paintSignalLamp\(\$\(['\"]hl-rl['\"]\),\s*turningLeft", "left rear indicator follows left turn"),
        (r"paintSignalLamp\(\$\(['\"]hl-fr['\"]\),\s*turningRight", "right front indicator follows right turn"),
        (r"paintSignalLamp\(\$\(['\"]hl-rr['\"]\),\s*turningRight", "right rear indicator follows right turn"),
    )
    for pattern, description in required_calls:
        if re.search(pattern, html):
            report("PASS", description)
        else:
            report("FAIL", f"Turn-signal regression: {description}")

    if re.search(r"@keyframes\s+turnSignalBlink\s*\{0%,44%\s*\{\s*opacity:1\s*\}\s*45%,100%\s*\{\s*opacity:0\s*\}\}", html):
        report("PASS", "Turn-signal blink timing remains unchanged")
    else:
        report("FAIL", "Turn-signal blink animation or timing has changed")

    if re.search(r"function\s+paintLights\s*\(\s*\).*?paintTurnSignals\(\)\s*;", html, re.DOTALL):
        report("PASS", "Light-button updates preserve turn-signal state")
    else:
        report("FAIL", "paintLights() must reapply turn-signal state after headlight updates")



def check_accent_theme_regressions() -> None:
    """Ensure accent themes only personalize GUI colors and survive reloads."""
    html = read_text(REPO_ROOT / "index.html")
    if html is None:
        report("FAIL", "Cannot read index.html for accent-theme checks")
        return

    select_match = re.search(
        r'<select\s+id="accent-select">(.*?)</select>',
        html,
        re.DOTALL,
    )
    if not select_match:
        report("FAIL", "Settings must contain the accent-select control")
        return

    options = re.findall(r'<option\s+value="([^"]+)"', select_match.group(1))
    expected_options = ("classic-blue", "racing-red", "neon-green")
    if tuple(options) == expected_options:
        report("PASS", "Accent selector contains Classic Blue, Racing Red and Neon Green")
    else:
        report("FAIL", "Accent selector options are missing or in an unexpected order")

    theme_css = (
        (r'\[data-accent="racing-red"\]\s*\{[^}]*--blue:#f43f5e;--blue-dk:#e11d48;--accent-rgb:244,63,94', "Racing Red dark palette"),
        (r'\[data-theme="light"\]\[data-accent="racing-red"\]\s*\{[^}]*--blue:#e11d48;--blue-dk:#be123c;--accent-rgb:225,29,72', "Racing Red light palette"),
        (r'\[data-accent="neon-green"\]\s*\{[^}]*--blue:#22c55e;--blue-dk:#16a34a;--accent-rgb:34,197,94', "Neon Green dark palette"),
        (r'\[data-theme="light"\]\[data-accent="neon-green"\]\s*\{[^}]*--blue:#16a34a;--blue-dk:#15803d;--accent-rgb:22,163,74', "Neon Green light palette"),
    )
    for pattern, description in theme_css:
        if re.search(pattern, html):
            report("PASS", description)
        else:
            report("FAIL", f"Missing accent color rule: {description}")

    accent_function = re.search(
        r"function\s+setAccent\s*\(\s*value\s*\)\s*\{(.*?)\n\s*\}",
        html,
        re.DOTALL,
    )
    if accent_function:
        body = accent_function.group(1)
        checks = (
            "classic-blue" in body and "racing-red" in body and "neon-green" in body,
            re.search(r"setAttribute\(\s*['\"]data-accent['\"]", body) is not None,
            re.search(r"localStorage\.setItem\(\s*['\"]lcc_accent['\"]", body) is not None,
        )
        if all(checks):
            report("PASS", "Accent selection is validated, applied and saved to localStorage")
        else:
            report("FAIL", "setAccent() must validate, apply and save the selected preset")
    else:
        report("FAIL", "setAccent() is missing")

    if (
        re.search(r"setAccent\(localStorage\.getItem\(\s*['\"]lcc_accent['\"]\s*\)", html)
        and re.search(r"\$\(['\"]accent-select['\"]\)\.onchange\s*=\s*function", html)
    ):
        report("PASS", "Saved accent preset is restored and the settings control responds to changes")
    else:
        report("FAIL", "Accent preset must load on startup and respond to selector changes")


    if "rgba(var(--accent-rgb)," in html and "rgba(59,130,246," not in html:
        report("PASS", "Accent-dependent interface glows use the selected palette")
    else:
        report("FAIL", "Accent-dependent interface glows must use --accent-rgb")

    if (
        "localStorage.setItem('lcc_th',t)" in html
        and "id=\"turn-signal-gradient\"" in html
        and "id=\"wheel-tint-flood\"" in html
    ):
        report("PASS", "Accent presets leave light/dark mode, wheel tint and turn-signal colors intact")
    else:
        report("FAIL", "Accent presets must not replace existing theme mode or vehicle signal colors")



def check_diagnostics_regressions() -> None:
    """Validate read-only diagnostic telemetry and its GUI without control commands."""
    app = read_text(SOURCE_DIR / "app.cpp")
    control = read_text(SOURCE_DIR / "web_control.cpp")
    html = read_text(REPO_ROOT / "index.html")
    if app is None or control is None or html is None:
        report("FAIL", "Cannot read app.cpp, web_control.cpp or index.html for diagnostics checks")
        return

    status_fields = ("sta", "up", "heap", "minh", "age", "lb", "fsc", "fsa")
    status_function = re.search(
        r"void\s+buildStatus\s*\(\s*char\s*\*\s*buf\s*,\s*size_t\s+n\s*\)\s*\{(.*?)"
        r"\n\s*\}\s*\n",
        app,
        re.DOTALL,
    )
    if status_function:
        body = status_function.group(1)
        required_sources = (
            ("sta", "WiFi.softAPgetStationNum()"),
            ("up", "millis() / 1000UL"),
            ("heap", "ESP.getFreeHeap()"),
            ("minh", "ESP.getMinFreeHeap()"),
            ("age", "millis() - lastCmdMillis"),
            ("lb", "lowBattCount"),
            ("fsc", "safetyTimeoutCount"),
            ("fsa", "lastSafetyTimeoutMillis"),
        )
        for field, source in required_sources:
            if re.search(rf'\\"{field}\\"\s*:', body) and source in body:
                report("PASS", f"Diagnostic field {field} uses {source}")
            else:
                report("FAIL", f"Diagnostic status field {field} missing or has an unexpected source")

        if "char buf[256];" in app and "char buf[256];" in control:
            report("PASS", "Status buffers fit the extended diagnostic JSON")
        else:
            report("FAIL", "Extended diagnostic JSON requires 256-byte status buffers")
    else:
        report("FAIL", "Could not locate buildStatus() for diagnostic telemetry checks")

    timeout_loop = re.search(
        r"void\s+loop\s*\(\s*\)\s*\{(.*?)//\s*Обслуживание OTA",
        app,
        re.DOTALL,
    )
    timeout_body = timeout_loop.group(1) if timeout_loop else ""
    counter_function_start = app.find("void recordSafetyTimeout()")
    counter_function_end = app.find("// ---------- Основной цикл ----------", counter_function_start)
    counter_body = (
        app[counter_function_start:counter_function_end]
        if counter_function_start >= 0 and counter_function_end > counter_function_start
        else ""
    )
    timeout_diagnostics_ok = (
        "unsigned long safetyTimeoutCount = 0;" in app
        and "unsigned long lastSafetyTimeoutMillis = 0;" in app
        and "bool safetyTimeoutLatched = false;" in app
        and "recordSafetyTimeout();" in timeout_body
        and re.search(
            r"if\s*\(\s*!safetyTimeoutLatched\s*\)\s*\{.*?"
            r"safetyTimeoutCount\+\+;.*?lastSafetyTimeoutMillis\s*=\s*millis\(\);.*?"
            r"safetyTimeoutLatched\s*=\s*true;",
            counter_body,
            re.DOTALL,
        )
        and "safetyTimeoutLatched = false;" in timeout_body
        and "stopAll();" in timeout_body
    )
    if timeout_diagnostics_ok:
        report("PASS", "Safety timeout activations are counted once without changing the stop action")
    else:
        report("FAIL", "Safety timeout diagnostics must count the protected stop once and retain stopAll()")

    diag_heading = html.find("<h3>Диагностика системы</h3>")
    buttons_heading = html.find("<h3>Кнопки</h3>")
    diag_block_start = html.rfind('<div class="block">', 0, diag_heading)
    buttons_block_start = html.rfind('<div class="block">', 0, buttons_heading)
    if (
        diag_heading >= 0
        and buttons_heading > diag_heading
        and diag_block_start >= 0
        and buttons_block_start > diag_block_start
        and len(re.findall(r'<div class="block">', html[diag_block_start:buttons_block_start])) == 1
        and html[diag_block_start:buttons_block_start].rstrip().endswith("</div>")
    ):
        report("PASS", "Diagnostics block is last in settings, immediately before the Buttons guide")
    else:
        report("FAIL", "Diagnostics must be placed at the end of settings immediately before the Buttons guide")

    diag_ids = (
        "diag-link", "diag-age", "diag-wifi", "diag-clients", "diag-rssi",
        "diag-battery", "diag-battery-state", "diag-uptime", "diag-heap",
        "diag-minheap", "diag-message-age", "diag-failsafe-count",
        "diag-failsafe-last",
    )
    for element_id in diag_ids:
        if re.search(rf"""id=["']{re.escape(element_id)}["']""", html):
            report("PASS", f"Diagnostics display contains {element_id}")
        else:
            report("FAIL", f"Diagnostics display is missing {element_id}")

    update_match = re.search(
        r"function\s+updateDiagnostics\s*\(\s*d\s*\)\s*\{(.*?)"
        r"\n\s*\}\s*\n\s*function\s+renderDiagnosticFreshness",
        html,
        re.DOTALL,
    )
    if update_match:
        body = update_match.group(1)
        if all(token in body for token in ("d.sta", "d.c", "d.r", "d.v", "d.p", "d.lb", "d.up", "d.heap", "d.minh", "d.age", "d.fsc", "d.fsa")):
            report("PASS", "GUI renders all transmitted read-only diagnostic values")
        else:
            report("FAIL", "GUI parser is missing one or more diagnostic telemetry fields")
        if not re.search(r"\btxWS\s*\(", body):
            report("PASS", "Diagnostic rendering does not send control commands")
        else:
            report("FAIL", "Diagnostic rendering must not send WebSocket control commands")
    else:
        report("FAIL", "updateDiagnostics() is missing")

    if (
        "updateDiagnostics(d);" in html
        and "setInterval(renderDiagnosticFreshness, 1000)" in html
        and "now - lastTelemetryAt > 3000" in html
    ):
        report("PASS", "Diagnostic connection freshness updates and detects stale telemetry")
    else:
        report("FAIL", "Diagnostic screen must update freshness and flag stale telemetry")
    event_log_checks = (
        ('id="diag-log"', "event log container"),
        ('id="diag-log-clear"', "clear-log button"),
        ("if(diagnosticEvents.length > 20) diagnosticEvents.length = 20", "bounded 20-event history"),
        ("Соединение с ESP32 потеряно", "disconnect event"),
        ("Телеметрия не обновляется более 3 секунд", "stale telemetry event"),
        ("Получение телеметрии восстановлено", "telemetry recovery event"),
        ("Состояние батареи: ", "battery state change event"),
        ("Сработала защита по тайм-ауту на ESP32", "firmware safety-timeout event"),
        ("event.code", "WebSocket close code"),
        ("event.wasClean", "WebSocket clean-close flag"),
        ("event.reason", "WebSocket close reason"),
        ("message.textContent = event.message", "safe text rendering"),
    )
    for token, description in event_log_checks:
        if token in html:
            report("PASS", f"Diagnostic event log includes {description}")
        else:
            report("FAIL", f"Diagnostic event log is missing {description}")




def check_battery_warning_regressions() -> None:
    web_page = read_text(SOURCE_DIR / "web_page.cpp")
    if web_page is None:
        report("FAIL", "Cannot read web_page.cpp for battery warning checks")
        return

    required_tokens = (
        'id="battery-warning-10"',
        'id="battery-warning-5"',
        'id="battery-warning-1"',
        "function nextBatteryWarningLevel(",
        "function shouldShowBatteryWarning(",
        "updateBatteryWarnings(d.v, d.p);",
        "if(batteryWarningLevel === 3 && previousLevel !== 3)",
        "zeroAll();",
        "dismissBatteryWarning(1)",
        "dismissBatteryWarning(2)",
        "BATTERY_WARNING_STATE_START",
        "BATTERY_WARNING_STATE_END",
        'id="battery-warning-test-controls"',
        'data-battery-test-percent="10"',
        'data-battery-test-percent="5"',
        'data-battery-test-percent="1"',
        "function setupBatteryWarningPreview()",
        "batteryWarningTestMode = new URLSearchParams(window.location.search).get('batteryTest') === '1'",
        "if(batteryWarningTestMode){",
        "setupBatteryWarningPreview();",

    )
    missing = [token for token in required_tokens if token not in web_page]
    if missing:
        report("FAIL", "Battery warning regression check missing: " + ", ".join(missing))
    else:
        report("PASS", "GUI retains 10%, 5%, and 1% battery warnings and critical-stop behavior")

    if 'overlay.setAttribute(\'aria-hidden\', visible ? \'false\' : \'true\')' in web_page:
        report("PASS", "Battery warning overlay updates accessibility visibility state")
    else:
        report("FAIL", "Battery warning overlay must update its accessibility visibility state")



def check_battery_calibration() -> None:
    """Keep the physical divider ratio distinct from the field-calibration multiplier."""
    monitor = read_text(SOURCE_DIR / "battery_monitor.cpp")
    calculator = read_text(SCRIPT_DIR / "calibrate_battery.py")
    if monitor is None:
        report("FAIL", "Cannot read battery_monitor.cpp for calibration checks")
        return
    if calculator is None:
        report("FAIL", "Missing tools/calibrate_battery.py calibration calculator")
        return

    checks = (
        ("BATT_DIVIDER_FACTOR 0.2680", "physical battery divider factor is explicit"),
        ("BATT_CALIBRATION_FACTOR = 1.0000f", "separate default calibration multiplier"),
        ("(vAdc / BATT_DIVIDER_FACTOR) * BATT_CALIBRATION_FACTOR", "calibration multiplier is applied to measured voltage"),
        ("tools/calibrate_battery.py", "source comment points to the calibration calculator"),
    )
    for token, description in checks:
        if token in monitor:
            report("PASS", description)
        else:
            report("FAIL", f"Battery calibration is missing: {description}")

    calculator_checks = (
        ("def calculate_scale_factor(", "calculator derives a scale correction"),
        ("def calculate_updated_factor(", "calculator supports an existing calibration value"),
        ('"--pair"', "calculator accepts measurement pairs"),
    )
    for token, description in calculator_checks:
        if token in calculator:
            report("PASS", description)
        else:
            report("FAIL", f"Battery calibration script is missing: {description}")



def check_i2c_motor_backend_integration() -> None:
    """Validate the PCA9685 motor backend is integrated without legacy GPIO drive code."""
    header = read_text(SOURCE_DIR / "i2c_motor_backend.h")
    backend = read_text(SOURCE_DIR / "i2c_motor_backend.cpp")
    motor = read_text(SOURCE_DIR / "motor_control.cpp")
    app = read_text(SOURCE_DIR / "app.cpp")
    hardware = read_text(SOURCE_DIR / "hardware_config.h")
    display = read_text(SOURCE_DIR / "display_control.cpp")

    if any(item is None for item in (header, backend, motor, app, hardware, display)):
        report("FAIL", "Cannot read all files required for I2C motor backend integration checks")
        return

    required_header = (
        "enum class I2CMotorChannel",
        "i2cMotorBackendBegin",
        "i2cMotorBackendIsReady",
        "i2cMotorBackendSetMotor",
        "i2cMotorBackendStopAll",
    )
    missing = [token for token in required_header if token not in header]
    if missing:
        report("FAIL", "I2C backend public API missing: " + ", ".join(missing))
    else:
        report("PASS", "I2C backend exposes initialization, readiness, per-motor and stop-all APIs")

    required_backend = (
        "#include <Adafruit_MotorShield.h>",
        "I2C_MOTOR_WING_ADDRESS = 0x60",
        "MOTOR_PWM_FREQUENCY_HZ = 1600",
        "shield.begin(MOTOR_PWM_FREQUENCY_HZ, &wire)",
        "shield.getMotor(static_cast<uint8_t>(i + 1))",
        "FORWARD",
        "BACKWARD",
        "RELEASE",
        "constrain(speedPercent, -100, 100)",
        "setSpeed(0)",
        "xSemaphoreCreateMutex()",
        "xSemaphoreTake(backendMutex, waitTicks)",
        "xSemaphoreGive(backendMutex)",
        "BackendLock lock",
    )
    missing = [token for token in required_backend if token not in backend]
    if missing:
        report("FAIL", "I2C backend implementation missing expected behavior: " + ", ".join(missing))
    else:
        report("PASS", "Backend initializes PCA9685, maps M1-M4 and applies signed speed/release behavior")

    channel_calls = {
        "A": r"i2cMotorBackendSetMotor\(\s*I2CMotorChannel::A",
        "B": r"i2cMotorBackendSetMotor\(\s*I2CMotorChannel::B",
        "C": r"i2cMotorBackendSetMotor\(\s*I2CMotorChannel::C",
        "D": r"i2cMotorBackendSetMotor\(\s*I2CMotorChannel::D",
    }
    for channel, pattern in channel_calls.items():
        if re.search(pattern, motor):
            report("PASS", f"Logical motor {channel} routes through the I2C backend")
        else:
            report("FAIL", f"Logical motor {channel} is not routed through the I2C backend")

    if "setDCBridge(" not in motor and "ledcWrite(" not in motor and "digitalWrite(" not in motor:
        report("PASS", "Active motor control no longer drives the legacy TB6612 GPIO/LEDC pins")
    else:
        report("FAIL", "Legacy GPIO/LEDC motor control remains in motor_control.cpp")

    if "i2cMotorBackendStopAll();" in motor and "i2cMotorBackendSetMotor" in motor:
        report("PASS", "stopAll() and per-motor commands use the I2C backend")
    else:
        report("FAIL", "Motor stop/command paths are not fully connected to the I2C backend")

    init_pattern = (
        r"Wire\.begin\(I2C_SDA_PIN, I2C_SCL_PIN\);.*?"
        r"motorBoardReady\s*=\s*i2cMotorBackendBegin\(Wire\);"
    )
    if re.search(init_pattern, app, re.DOTALL):
        report("PASS", "Motor board initializes on the existing Wire bus after Wire.begin()")
    else:
        report("FAIL", "Motor board must initialize on the existing Wire bus after Wire.begin()")

    if (
        "pinMode(TB_" not in app
        and "ledcSetup(LEDC_CH_" not in app
        and "ledcAttachPin(TB_" not in app
        and "TB_STBY" not in hardware
        and "TB_AIN1" not in hardware
    ):
        report("PASS", "Obsolete direct motor GPIO configuration is removed from this branch")
    else:
        report("FAIL", "Obsolete direct motor GPIO setup or pin map remains active")

    if "if (!motorBoardReady)" in display and 'display.print("I2C ERR")' in display:
        report("PASS", "OLED displays a warning when the PCA9685 motor board is not detected")
    else:
        report("FAIL", "OLED must report an unavailable I2C motor board")


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
        check_battery_calibration()
        print("\n--- Application checks ---")
        check_app_entry_points()
        check_motor_safety_anchor()
        print("\n--- I2C motor backend integration checks ---")
        check_i2c_motor_backend_integration()
        print("\n--- Control-safety regression checks ---")
        check_gui_heartbeat_safety()
        check_gui_engine_guards()
        check_firmware_heartbeat_and_watchdog()
        print("\n--- GUI visual regression checks ---")
        check_tank_visual_regressions()
        print("\n--- Turn-signal regression checks ---")
        check_turn_signal_regressions()
        print("\n--- Accent-theme regression checks ---")
        check_accent_theme_regressions()
        print("\n--- Read-only diagnostics regression checks ---")
        check_diagnostics_regressions()
        check_battery_warning_regressions()

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
