#!/usr/bin/env python3
"""Проверки структуры проекта и защиты от регрессий LEGO Technic ESP32."""

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
    "lego_technic_esp32.ino",
    "hardware_config.h",
    *(f"{name}.h" for name in MODULES),
    *(f"{name}.cpp" for name in MODULES),
)

PASS_COUNT = 0
FAIL_COUNT = 0
WARN_COUNT = 0


def report(status: str, message: str) -> None:
    global PASS_COUNT, FAIL_COUNT, WARN_COUNT
    display_status = {"PASS": "УСПЕХ", "FAIL": "ОШИБКА", "WARN": "ВНИМАНИЕ"}.get(status, status)
    print(f"[{display_status}] {message}")
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
        report("FAIL", f"Каталог исходников не найден: {SOURCE_DIR.relative_to(REPO_ROOT)}")
        return False

    report("PASS", "Каталог исходников существует")
    for relative_name in REQUIRED_FILES:
        if (SOURCE_DIR / relative_name).is_file():
            report("PASS", f"Обязательный файл: {relative_name}")
        else:
            report("FAIL", f"Не найден обязательный файл: {relative_name}")
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
            report("FAIL", f"Не удалось прочитать: {source_file.name}")
            continue

        for include_name in include_pattern.findall(content):
            include_name = include_name.replace("\\", "/")
            if resolve_local_include(source_file, include_name):
                report("PASS", f'Файл {source_file.name} подключает "{include_name}"')
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
            report("FAIL", f"Не удалось прочитать {implementation.name}")
            continue

        included_headers = re.findall(
            r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', content, re.MULTILINE
        )
        if any(Path(name).name == header.name for name in included_headers):
            report("PASS", f"{implementation.name} содержит подключение {header.name}")
        else:
            report("FAIL", f"{implementation.name} не подключает собственный заголовок {header.name}")


def check_app_entry_points() -> None:
    app_content = read_text(SOURCE_DIR / "lego_technic_esp32.ino")
    if app_content is None:
        report("FAIL", "Не удалось прочитать lego_technic_esp32.ino")
        return

    for function_name in ("setup", "loop"):
        if re.search(rf"\bvoid\s+{function_name}\s*\(", app_content):
            report("PASS", f"lego_technic_esp32.ino содержит {function_name}()")
        else:
            report("FAIL", f"lego_technic_esp32.ino не содержит {function_name}()")


def check_motor_safety_anchor() -> None:
    motor_content = read_text(SOURCE_DIR / "motor_control.cpp")
    if motor_content is None:
        report("FAIL", "Не удалось прочитать motor_control.cpp")
        return

    if re.search(r"\bvoid\s+stopAll\s*\(", motor_content):
        report("PASS", "motor_control.cpp содержит stopAll()")
    else:
        report("FAIL", "motor_control.cpp не содержит void stopAll()")


def check_gui_heartbeat_safety() -> None:
    """Heartbeat должен повторять актуальные значения управления без дополнительных reconnect-защит."""
    page = read_text(SOURCE_DIR / "web_page.cpp")
    if page is None:
        report("FAIL", "Не удалось прочитать web_page.cpp for heartbeat regression checks")
        return

    expected_packet = "ws.send(JSON.stringify({hb:1, A:lastSent.A, B:lastSent.B, C:lastSent.C, D:lastSent.D, S:lastSent.S}));"
    if expected_packet in page and "}, 150);" in page:
        report("PASS", "Heartbeat каждые 150 мс передаёт актуальные значения A/B/C/D/S")
    else:
        report("FAIL", "Heartbeat должен каждые 150 мс передавать актуальные значения всех каналов")

    open_start = page.find("ws.onopen = function(){")
    close_start = page.find("ws.onclose = function(){", open_start)
    onopen = page[open_start:close_start] if open_start >= 0 and close_start > open_start else ""
    if onopen and "zeroAll()" not in onopen and "txWS({eng:0})" not in onopen:
        report("PASS", "При переподключении нет дополнительного сброса команд поверх watchdog/heartbeat")
    else:
        report("FAIL", "В обработчике переподключения остался дополнительный сброс команд")

def check_gui_engine_guards() -> None:
    page = read_text(SOURCE_DIR / "web_page.cpp")
    if page is None:
        report("FAIL", "Не удалось прочитать web_page.cpp for engine-simulation checks")
        return

    # The engine simulation only makes sense in classic-car mode with at least two motors.
    button_guard = re.search(
        r"\$\s*\(\s*['\"]eng-btn['\"]\s*\)\s*\.\s*onclick\s*=\s*function\s*\(\s*\)\s*\{\s*"
        r"if\s*\(\s*motorCount\s*<\s*2\s*\|\|\s*mode\s*!==\s*1\s*\)\s*return\s*;",
        page,
    )
    if button_guard:
        report("PASS", "Кнопка двигателя учитывает число моторов и режим классического автомобиля")
    else:
        report("FAIL", "Кнопка двигателя должна блокировать motorCount < 2 или mode !== 1")

    mode_guard = re.search(
        r"if\s*\(\s*m\s*!==\s*1\s*&&\s*eng\s*\)\s*\{"
        r"(?:(?!\n\s*\}).)*?txWS\s*\(\s*\{\s*eng\s*:\s*0\s*\}\s*\)\s*;",
        page,
        re.DOTALL,
    )
    if mode_guard:
        report("PASS", "При выходе из режима классического автомобиля имитация двигателя отключается")
    else:
        report("FAIL", "switchMode() должна отправлять eng=0 при выходе из режима классического автомобиля")

    low_motor_guard = re.search(
        r"if\s*\(\s*motorCount\s*<\s*2\s*\)\s*\{"
        r"(?:(?!\n\s*\}).)*?eng\s*=\s*false\s*;"
        r"(?:(?!\n\s*\}).)*?txWS\s*\(\s*\{\s*eng\s*:\s*0\s*\}\s*\)\s*;"
        r"(?:(?!\n\s*\}).)*?txWS\s*\(\s*\{\s*ch\s*:\s*['\"]B['\"]\s*,\s*val\s*:\s*0\s*\}\s*\)\s*;",
        page,
        re.DOTALL,
    )
    if low_motor_guard:
        report("PASS", "В конфигурации с одним мотором имитация двигателя отключена, канал B обнулён")
    else:
        report("FAIL", "При motorCount < 2 имитация двигателя должна отключаться, а мотор B обнуляться")


def check_firmware_heartbeat_and_watchdog() -> None:
    control = read_text(SOURCE_DIR / "web_control.cpp")
    app = read_text(SOURCE_DIR / "lego_technic_esp32.ino")
    if control is None or app is None:
        report("FAIL", "Не удалось прочитать исходники watchdog/heartbeat")
        return

    heartbeat_start = control.find('if (doc.containsKey("hb"))')
    heartbeat_end = control.find("// Переключение режима", heartbeat_start)
    hb = control[heartbeat_start:heartbeat_end] if heartbeat_start >= 0 and heartbeat_end > heartbeat_start else ""
    checks = (
        'doc.size() != 6' in hb,
        'doc["hb"].is<int>()' in hb,
        'doc["A"].is<int>()' in hb and 'doc["B"].is<int>()' in hb,
        'doc["C"].is<int>()' in hb and 'doc["D"].is<int>()' in hb and 'doc["S"].is<int>()' in hb,
        "applyMotorA(a)" in hb and "applyMotorB(b)" in hb and "applyMotorC(c)" in hb and "applyMotorD(d)" in hb,
        "applySteer(steer)" in hb,
        "lastCmdMillis = messageMillis" in hb,
        "updateDriverStandby()" in hb,
    )
    if all(checks):
        report("PASS", "Heartbeat валидирует и применяет актуальные команды всех каналов")
    else:
        report("FAIL", "Heartbeat должен проверять A/B/C/D/S, применять команды и обновлять watchdog")

    timeout_ok = "const unsigned long CMD_TIMEOUT_MS = 500;" in app
    watchdog_ok = "watchdogNow - lastCommandAt" in app and "millis() - lastCmdMillis" in app and "stopAll();" in app
    diagnostics_removed = all(token not in app for token in ("lastWsActivityMillis", "lastHeartbeatMillis", "heartbeatCount", "safetyTimeoutCount"))
    if timeout_ok and watchdog_ok and diagnostics_removed:
        report("PASS", "Оставлен один watchdog с тайм-аутом 500 мс без дополнительных защитных счётчиков")
    else:
        report("FAIL", "Watchdog должен останавливать выходы через 500 мс и не содержать лишних защитных счётчиков")




def check_tank_visual_regressions() -> None:
    """Защищает проверенный танковый интерфейс от регрессий стрелок, оттенков, слоёв и синхронизации."""
    html = read_text(REPO_ROOT / "index.html")
    page = read_text(SOURCE_DIR / "web_page.cpp")
    if html is None:
        report("FAIL", "Не удалось прочитать index.html for tank visual regression checks")
        return
    if page is None:
        report("FAIL", "Не удалось прочитать web_page.cpp for tank visual regression checks")
        return

    embedded_start = page.find("<!DOCTYPE html>")
    embedded_end = page.find(')HTML";', embedded_start)
    if embedded_start < 0 or embedded_end < 0:
        report("FAIL", "Не удалось найти границы встроенного PAGE_HTML в web_page.cpp")
    else:
        embedded_html = page[embedded_start:embedded_end]
        if embedded_html == html.lstrip():
            report("PASS", "index.html полностью совпадает со встроенным PAGE_HTML")
        else:
            report("FAIL", "index.html отличается от встроенного PAGE_HTML")

    arrow_pattern = re.compile(
        r'track-direction-markers|id=["\']t-(?:left|right)-(?:f|r)["\']'
    )
    if not arrow_pattern.search(html):
        report("PASS", "Стрелки направления движения по-прежнему удалены")
    else:
        report("FAIL", "В разметку вернулись стрелки направления движения")

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
            report("PASS", f"Слой танкового интерфейса присутствует: {token}")
        else:
            report("FAIL", f"Отсутствует слой танкового интерфейса: {token}")

    paint_match = re.search(
        r"function\s+paintTank\s*\(\s*L\s*,\s*R\s*\)\s*\{(.*?)"
        r"\n\s*\}\s*\n\s*function\s+paintCar\s*\(",
        html,
        re.DOTALL,
    )
    if not paint_match:
        report("FAIL", "Не удалось найти paintTank() для проверки направлений движения")
        return

    paint = paint_match.group(1)
    direction_patterns = (
        (r"leftForward\s*=\s*L\s*>\s*8", "порог движения левой гусеницы вперёд"),
        (r"leftReverse\s*=\s*L\s*<\s*-8", "порог движения левой гусеницы назад"),
        (r"rightForward\s*=\s*R\s*>\s*8", "порог движения правой гусеницы вперёд"),
        (r"rightReverse\s*=\s*R\s*<\s*-8", "порог движения правой гусеницы назад"),
        (
            r"leftFlood\.setAttribute\(\s*['\"]flood-color['\"]\s*,\s*leftReverse\s*\?\s*red\s*:\s*green\s*\)",
            "левая гусеница зелёная при движении вперёд и красная при движении назад",
        ),
        (
            r"rightFlood\.setAttribute\(\s*['\"]flood-color['\"]\s*,\s*rightReverse\s*\?\s*red\s*:\s*green\s*\)",
            "правая гусеница зелёная при движении вперёд и красная при движении назад",
        ),
        (
            r"leftGlow\.setAttribute\(\s*['\"]opacity['\"]\s*,\s*\(leftForward\s*\|\|\s*leftReverse\)\s*\?\s*['\"]0\.58['\"]\s*:\s*['\"]0['\"]\s*\)",
            "подсветка левой гусеницы полупрозрачная и выключена в покое",
        ),
        (
            r"rightGlow\.setAttribute\(\s*['\"]opacity['\"]\s*,\s*\(rightForward\s*\|\|\s*rightReverse\)\s*\?\s*['\"]0\.58['\"]\s*:\s*['\"]0['\"]\s*\)",
            "подсветка правой гусеницы полупрозрачная и выключена в покое",
        ),
    )
    for pattern, description in direction_patterns:
        if re.search(pattern, paint):
            report("PASS", description)
        else:
            report("FAIL", f"Регрессия освещения танка: {description}")

    if "drop-shadow" not in paint and "style.filter" not in paint:
        report("PASS", "В освещении танка используются полупрозрачные заливки вместо одних контурных теней")
    else:
        report("FAIL", "Освещение танка снова использует только контурные тени")

    left_track = html.find('<g id="t-left-group">')
    right_track = html.find('<g id="t-right-group">')
    chassis = html.find('<image id="crawler-body"')
    if 0 <= left_track < right_track < chassis:
        report("PASS", "Обе гусеницы отрисовываются под корпусом краулера")
    else:
        report("FAIL", "Порядок слоёв танка должен быть таким: левая гусеница, правая гусеница, корпус краулера")



def check_turn_signal_regressions() -> None:
    """Сохраняет передние и задние поворотники на четырёх исходных позициях фонарей."""
    html = read_text(REPO_ROOT / "index.html")
    if html is None:
        report("FAIL", "Не удалось прочитать index.html for turn-signal regression checks")
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
            report("FAIL", f"Регрессия поворотников: {description}")

    if not re.search(r'id="turn-signal-(?:left|right)"', html):
        report("PASS", "Поворотники используют исходные SVG-элементы фар")
    else:
        report("FAIL", "Дополнительные слои поворотников могут сместиться относительно исходных фонарей")

    required_calls = (
        (r"paintSignalLamp\(\$\(['\"]hl-fl['\"]\),\s*turningLeft", "передний левый указатель следует левому повороту"),
        (r"paintSignalLamp\(\$\(['\"]hl-rl['\"]\),\s*turningLeft", "задний левый указатель следует левому повороту"),
        (r"paintSignalLamp\(\$\(['\"]hl-fr['\"]\),\s*turningRight", "передний правый указатель следует правому повороту"),
        (r"paintSignalLamp\(\$\(['\"]hl-rr['\"]\),\s*turningRight", "задний правый указатель следует правому повороту"),
    )
    for pattern, description in required_calls:
        if re.search(pattern, html):
            report("PASS", description)
        else:
            report("FAIL", f"Регрессия поворотников: {description}")

    if re.search(r"@keyframes\s+turnSignalBlink\s*\{0%,44%\s*\{\s*opacity:1\s*\}\s*45%,100%\s*\{\s*opacity:0\s*\}\}", html):
        report("PASS", "Период мигания поворотников не изменился")
    else:
        report("FAIL", "Изменились анимация или период мигания поворотников")

    if re.search(r"function\s+paintLights\s*\(\s*\).*?paintTurnSignals\(\)\s*;", html, re.DOTALL):
        report("PASS", "Обновление кнопок света сохраняет состояние поворотников")
    else:
        report("FAIL", "paintLights() должна повторно применять состояние поворотников после обновления фар")



def check_accent_theme_regressions() -> None:
    """Проверяет, что акцентные темы меняют только цвета интерфейса и сохраняются после перезагрузки."""
    html = read_text(REPO_ROOT / "index.html")
    if html is None:
        report("FAIL", "Не удалось прочитать index.html for accent-theme checks")
        return

    select_match = re.search(
        r'<select\s+id="accent-select">(.*?)</select>',
        html,
        re.DOTALL,
    )
    if not select_match:
        report("FAIL", "В настройках должен быть элемент выбора акцентного цвета")
        return

    options = re.findall(r'<option\s+value="([^"]+)"', select_match.group(1))
    expected_options = ("classic-blue", "racing-red", "neon-green")
    if tuple(options) == expected_options:
        report("PASS", "Список акцентных цветов содержит Classic Blue, Racing Red и Neon Green")
    else:
        report("FAIL", "Пункты выбора акцентного цвета отсутствуют или идут в неожиданном порядке")

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
            report("FAIL", f"Отсутствует правило акцентного цвета: {description}")

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
            report("PASS", "Выбор акцента проверяется, применяется и сохраняется в localStorage")
        else:
            report("FAIL", "setAccent() должна проверять, применять и сохранять выбранный пресет")
    else:
        report("FAIL", "Функция setAccent() отсутствует")

    if (
        re.search(r"setAccent\(localStorage\.getItem\(\s*['\"]lcc_accent['\"]\s*\)", html)
        and re.search(r"\$\(['\"]accent-select['\"]\)\.onchange\s*=\s*function", html)
    ):
        report("PASS", "Сохранённый акцентный пресет восстанавливается, элемент настроек реагирует на изменения")
    else:
        report("FAIL", "Акцентный пресет должен загружаться при запуске и реагировать на выбор")


    if "rgba(var(--accent-rgb)," in html and "rgba(59,130,246," not in html:
        report("PASS", "Свечение интерфейса использует выбранную палитру")
    else:
        report("FAIL", "Свечение интерфейса должно использовать --accent-rgb")

    if (
        "localStorage.setItem('lcc_th',t)" in html
        and "id=\"turn-signal-gradient\"" in html
        and "id=\"wheel-tint-flood\"" in html
    ):
        report("PASS", "Акцентные пресеты не меняют светлую/тёмную тему, оттенок колёс и цвета поворотников")
    else:
        report("FAIL", "Акцентные пресеты не должны менять режим темы или цвета сигналов модели")



def check_diagnostics_regressions() -> None:
    """Проверяет только базовую read-only телеметрию интерфейса."""
    app = read_text(SOURCE_DIR / "lego_technic_esp32.ino")
    html = read_text(REPO_ROOT / "index.html")
    if app is None or html is None:
        report("FAIL", "Не удалось прочитать исходники диагностики")
        return

    required_fields = ("WiFi.softAPgetStationNum()", "millis() / 1000UL", "ESP.getFreeHeap()", "ESP.getMinFreeHeap()", "lowBattCount", "reverseA", "reverseB", "reverseC", "reverseD", "engineSimOn", "steerTrimDegrees", "steerMaxAngleDeg")
    if "void buildStatus(" in app and all(field in app for field in required_fields):
        report("PASS", "Статус содержит только базовые показатели системы и настройки")
    else:
        report("FAIL", "В JSON статуса отсутствуют необходимые базовые поля")

    removed_fields = ("lastWsActivityMillis", "lastHeartbeatMillis", "heartbeatCount", "safetyTimeoutCount", "fscmd", "fsh", "fsa2")
    if all(field not in app for field in removed_fields):
        report("PASS", "Дополнительные счётчики и возраста защитного тайм-аута удалены")
    else:
        report("FAIL", "В прошивке остались дополнительные поля диагностики защитного тайм-аута")

    removed_gui = ("diag-message-age", "diag-failsafe-count", "diag-failsafe-last", "возраст heartbeat ", "heartbeat принято:")
    if all(field not in html for field in removed_gui):
        report("PASS", "Из веб-интерфейса удалены подробности ложных срабатываний watchdog")
    else:
        report("FAIL", "В веб-интерфейсе остались подробности защитной диагностики")

    if "updateDiagnostics(d);" in html and "txWS(" not in html[html.find("function updateDiagnostics"):html.find("function renderDiagnosticFreshness")]:
        report("PASS", "Отрисовка базовой диагностики не отправляет команды управления")
    else:
        report("FAIL", "Диагностика должна оставаться read-only")


def check_battery_warning_regressions() -> None:
    web_page = read_text(SOURCE_DIR / "web_page.cpp")
    if web_page is None:
        report("FAIL", "Не удалось прочитать web_page.cpp for battery warning checks")
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
        report("FAIL", "Отсутствует регрессионная проверка предупреждений батареи: " + ", ".join(missing))
    else:
        report("PASS", "Интерфейс сохраняет предупреждения батареи на 10%, 5% и 1%, а также критическую остановку")

    if 'overlay.setAttribute(\'aria-hidden\', visible ? \'false\' : \'true\')' in web_page:
        report("PASS", "Слой предупреждения батареи обновляет состояние доступности для вспомогательных технологий")
    else:
        report("FAIL", "Слой предупреждения батареи должен обновлять состояние видимости для вспомогательных технологий")



def check_battery_calibration() -> None:
    """Keep the physical divider ratio distinct from the field-calibration multiplier."""
    monitor = read_text(SOURCE_DIR / "battery_monitor.cpp")
    calculator = read_text(SCRIPT_DIR / "calibrate_battery.py")
    if monitor is None:
        report("FAIL", "Не удалось прочитать battery_monitor.cpp for calibration checks")
        return
    if calculator is None:
        report("FAIL", "Не найден калькулятор калибровки tools/calibrate_battery.py")
        return

    checks = (
        ("BATT_DIVIDER_FACTOR 0.2857", "physical battery divider factor is explicit"),
        ("BATT_CALIBRATION_FACTOR = 1.0000f", "separate default calibration multiplier"),
        ("(vAdc / BATT_DIVIDER_FACTOR) * BATT_CALIBRATION_FACTOR", "calibration multiplier is applied to measured voltage"),
        ("tools/calibrate_battery.py", "source comment points to the calibration calculator"),
    )
    for token, description in checks:
        if token in monitor:
            report("PASS", description)
        else:
            report("FAIL", f"Отсутствует калибровка батареи: {description}")

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


def main() -> int:
    print("=" * 64)
    print("LEGO TECHNIC ESP32 — АУДИТ ПРОЕКТА")
    print("=" * 64)
    print(f"Репозиторий: {REPO_ROOT}")
    print(f"Исходники:  {SOURCE_DIR}")
    print()

    source_exists = check_required_files()
    if source_exists:
        print("\n--- Проверка локальных подключений ---")
        check_local_includes()
        print("\n--- Проверка модулей ---")
        check_module_pairs()
        check_battery_calibration()
        print("\n--- Проверка приложения ---")
        check_app_entry_points()
        check_motor_safety_anchor()
        print("\n--- Регрессионные проверки безопасности управления ---")
        check_gui_heartbeat_safety()
        check_gui_engine_guards()
        check_firmware_heartbeat_and_watchdog()
        print("\n--- Проверка визуальных регрессий интерфейса ---")
        check_tank_visual_regressions()
        print("\n--- Проверка регрессий поворотников ---")
        check_turn_signal_regressions()
        print("\n--- Проверка регрессий акцентных тем ---")
        check_accent_theme_regressions()
        print("\n--- Проверка диагностики только для чтения ---")
        check_diagnostics_regressions()
        check_battery_warning_regressions()

    print()
    print("=" * 64)
    print("ИТОГИ АУДИТА")
    print("=" * 64)
    print(f"УСПЕШНО: {PASS_COUNT}")
    print(f"ОШИБОК: {FAIL_COUNT}")
    print(f"ПРЕДУПРЕЖДЕНИЙ: {WARN_COUNT}")

    if FAIL_COUNT:
        print("\nАудит завершился с ошибками. Исправь указанные нарушения структуры или безопасности.")
        return 1

    print("\nАудит пройден.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
