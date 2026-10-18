#!/usr/bin/env python3
"""Run the complete LEGO Technic ESP32 regression suite manually.

This script is intentionally not called by GitHub Actions. Run it from the
repository root before preparing an official firmware release:

    python tools/run_regression_checks.py

It requires a clean Git working tree, Python 3, Node.js, Git, and arduino-cli.
The script installs the project's pinned ESP32 core and libraries before compiling.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENERATED_FILES = (
    "lego_technic_esp32/web_page.cpp",
    "index.html",
    "CHANGELOG.md",
    "README.md",
)
ESP32_INDEX = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
LIBRARIES = (
    "ESPAsyncWebServer@3.1.0",
    "AsyncTCP@1.1.4",
    "ArduinoJson",
    "Adafruit SSD1306",
    "Adafruit GFX Library",
    "Adafruit BusIO",
    "ESP32Servo",
    "ElegantOTA@3.1.0",
)


def run(*args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    print("\n[RUN] " + " ".join(args), flush=True)
    return subprocess.run(args, cwd=ROOT, text=True, check=check)


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"Required command not found in PATH: {name}")


def require_clean_tree() -> None:
    result = run("git", "status", "--porcelain", check=True)
    if result.stdout.strip():
        raise RuntimeError(
            "The working tree must be clean before regression checks. "
            "Commit or stash local changes first; the generated-file check temporarily "
            "runs generators and restores their original tracked files."
        )


def check_generated_files() -> None:
    paths = [ROOT / item for item in GENERATED_FILES]
    snapshots = {path: path.read_bytes() for path in paths}
    mismatch = False
    try:
        run(sys.executable, "tools/embed_web_gui_assets.py")
        run(sys.executable, "tools/update_gui_version.py")

        web_page = (ROOT / "lego_technic_esp32/web_page.cpp").read_text(encoding="utf-8")
        app_cpp = (ROOT / "lego_technic_esp32/app.cpp").read_text(encoding="utf-8")
        if "PAGE_HTML" in web_page:
            html_source = "lego_technic_esp32/web_page.cpp"
        elif "PAGE_HTML" in app_cpp:
            html_source = "lego_technic_esp32/app.cpp"
        else:
            raise RuntimeError("PAGE_HTML was not found in web_page.cpp or app.cpp")

        run(sys.executable, "tools/extract_html.py", html_source, "index.html")
        run(
            sys.executable,
            "tools/extract_changelog.py",
            "lego_technic_esp32/app.cpp",
            "CHANGELOG.md",
        )
        run(sys.executable, "tools/update_readme_version.py")

        result = run("git", "diff", "--exit-code", "--", *GENERATED_FILES, check=False)
        mismatch = result.returncode != 0
        if mismatch:
            raise RuntimeError(
                "Generated files are out of sync with their sources. The diff above shows "
                "the required updates; original tracked files have been restored."
            )
        print("[PASS] Generated GUI, changelog and README are current.")
    finally:
        for path, content in snapshots.items():
            path.write_bytes(content)


def install_and_compile_firmware() -> None:
    require_tool("arduino-cli")
    run("arduino-cli", "config", "init", "--overwrite")
    run("arduino-cli", "config", "add", "board_manager.additional_urls", ESP32_INDEX)
    run("arduino-cli", "core", "update-index")
    run("arduino-cli", "core", "install", "esp32:esp32@2.0.9")
    run("arduino-cli", "lib", "install", *LIBRARIES)
    run(
        "arduino-cli",
        "compile",
        "--fqbn",
        "esp32:esp32:esp32",
        "--warnings",
        "default",
        "--build-property",
        "compiler.cpp.extra_flags=-DELEGANTOTA_USE_ASYNC_WEBSERVER=1",
        "--output-dir",
        str(ROOT / ".regression-build"),
        "lego_technic_esp32",
    )


def main() -> int:
    try:
        require_tool("git")
        require_tool("node")
        require_clean_tree()

        print("=" * 72)
        print("LEGO TECHNIC ESP32 — MANUAL REGRESSION SUITE")
        print("=" * 72)
        print("These checks are intentionally not run by GitHub Actions.")

        python_files = (
            "tools/embed_web_gui_assets.py",
            "tools/update_gui_version.py",
            "tools/extract_html.py",
            "tools/extract_changelog.py",
            "tools/update_readme_version.py",
            "tools/test_extract_changelog.py",
            "tools/release_automation.py",
            "tools/test_release_automation.py",
            "tools/project_auditor.py",
            "tools/calibrate_battery.py",
            "tools/test_calibrate_battery.py",
        )
        run(sys.executable, "-m", "py_compile", *python_files)
        run(sys.executable, "tools/test_extract_changelog.py")
        run(sys.executable, "tools/test_release_automation.py")
        run("node", "tools/test_battery_warnings.js")
        run(sys.executable, "tools/project_auditor.py")
        run(sys.executable, "tools/test_calibrate_battery.py")

        if shutil.which("ruby"):
            run(
                "ruby",
                "-e",
                'require "yaml"; ARGV.each { |path| YAML.parse_file(path); puts "PASS: YAML syntax #{path}" }',
                ".github/workflows/ci-build-deploy.yml",
                ".github/workflows/pull-request-validation.yml",
                ".github/workflows/release-versioning.yml",
                ".github/workflows/release-metadata-validation.yml",
                ".github/workflows/publish-release.yml",
            )
        else:
            print("[WARN] Ruby not found; workflow YAML syntax check was skipped.")

        check_generated_files()
        install_and_compile_firmware()
        print("\n" + "=" * 72)
        print("ALL MANUAL REGRESSION CHECKS PASSED")
        print("This does not replace physical hardware testing.")
        print("=" * 72)
        return 0
    except (RuntimeError, subprocess.CalledProcessError, OSError) as exc:
        print(f"\n[FAIL] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
