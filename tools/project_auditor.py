
#!/usr/bin/env python3
"""Simple structural auditor for the LEGO Technic ESP32 project."""

from pathlib import Path
import re
import sys


# ------------------------------------------------------------
# Project layout
# ------------------------------------------------------------

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


# ------------------------------------------------------------
# Output
# ------------------------------------------------------------

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


# ------------------------------------------------------------
# File and include checks
# ------------------------------------------------------------

def check_required_files() -> bool:
    if not SOURCE_DIR.is_dir():
        report(
            "FAIL",
            f"Source directory not found: {SOURCE_DIR.relative_to(REPO_ROOT)}",
        )
        return False

    report("PASS", "Source directory exists")

    for relative_name in REQUIRED_FILES:
        path = SOURCE_DIR / relative_name

        if path.is_file():
            report("PASS", f"Required file: {relative_name}")
        else:
            report("FAIL", f"Missing required file: {relative_name}")

    return True


def resolve_local_include(source_file: Path, include_name: str) -> bool:
    """Find a quoted include in common project locations."""
    candidates = (
        source_file.parent / include_name,
        SOURCE_DIR / include_name,
        SOURCE_DIR / "include" / include_name,
        REPO_ROOT / include_name,
        REPO_ROOT / "include" / include_name,
    )

    return any(candidate.is_file() for candidate in candidates)


def check_local_includes() -> None:
    include_pattern = re.compile(
        r'^\s*#\s*include\s*"([^"]+)"',
        re.MULTILINE,
    )

    source_files = list(SOURCE_DIR.rglob("*.h"))
    source_files += list(SOURCE_DIR.rglob("*.cpp"))

    for source_file in sorted(set(source_files)):
        content = read_text(source_file)

        if content is None:
            report("FAIL", f"Cannot read: {source_file.name}")
            continue

        for include_name in include_pattern.findall(content):
            # Ignore includes that are not local project files.
            if "\\" in include_name:
                include_name = include_name.replace("\\", "/")

            if resolve_local_include(source_file, include_name):
                report(
                    "PASS",
                    f'Include "{include_name}" in {source_file.name}',
                )
            else:
                report(
                    "FAIL",
                    f'Missing local include "{include_name}" '
                    f'in {source_file.relative_to(REPO_ROOT)}',
                )


# ------------------------------------------------------------
# Module checks
# ------------------------------------------------------------

def check_module_pairs() -> None:
    for module in MODULES:
        header = SOURCE_DIR / f"{module}.h"
        implementation = SOURCE_DIR / f"{module}.cpp"

        if not header.is_file() or not implementation.is_file():
            # Missing files are already reported by check_required_files().
            continue

        content = read_text(implementation)

        if content is None:
            report("FAIL", f"Cannot read {implementation.name}")
            continue

        included_headers = re.findall(
            r'^\s*#\s*include\s*[<"]([^>"]+)[>"]',
            content,
            re.MULTILINE,
        )

        if any(Path(name).name == header.name for name in included_headers):
            report("PASS", f"{implementation.name} includes {header.name}")
        else:
            report(
                "FAIL",
                f"{implementation.name} does not include its own "
                f"header {header.name}",
            )


# ------------------------------------------------------------
# Application entry points
# ------------------------------------------------------------

def check_app_entry_points() -> None:
    app_file = SOURCE_DIR / "app.cpp"

    if not app_file.is_file():
        return

    content = read_text(app_file)

    if content is None:
        report("FAIL", "Cannot read app.cpp")
        return

    # These checks deliberately verify only that the functions exist.
    # Compilation remains responsible for validating their implementation.
    for function_name in ("setup", "loop"):
        pattern = rf"\bvoid\s+{function_name}\s*\("
        if re.search(pattern, content):
            report("PASS", f"app.cpp contains {function_name}()")
        else:
            report("FAIL", f"app.cpp is missing {function_name}()")


def check_motor_safety_anchor() -> None:
    """Check that the central stop function has not disappeared."""
    motor_file = SOURCE_DIR / "motor_control.cpp"

    if not motor_file.is_file():
        return

    content = read_text(motor_file)

    if content is None:
        report("FAIL", "Cannot read motor_control.cpp")
        return

    # Accept common return types and optional whitespace.
    pattern = r"\bvoid\s+stopAll\s*\("
    if re.search(pattern, content):
        report("PASS", "motor_control.cpp contains stopAll()")
    else:
        report(
            "FAIL",
            "motor_control.cpp is missing void stopAll()",
        )


# ------------------------------------------------------------
# Main
# ------------------------------------------------------------

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

    print()
    print("=" * 64)
    print("AUDIT SUMMARY")
    print("=" * 64)
    print(f"PASS: {PASS_COUNT}")
    print(f"FAIL: {FAIL_COUNT}")
    print(f"WARN: {WARN_COUNT}")

    if FAIL_COUNT:
        print("\nAudit failed. Fix the reported structural errors.")
        return 1

    print("\nAudit passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
