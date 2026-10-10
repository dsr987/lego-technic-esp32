#!/usr/bin/env python3

import re
from pathlib import Path
from urllib.parse import quote

changelog_path = Path("CHANGELOG.md")
readme_path = Path("README.md")

if not changelog_path.is_file():
raise SystemExit("ERROR: CHANGELOG.md not found")

changelog = changelog_path.read_text(encoding="utf-8")

match = re.search(
r"^##\s+(.+?)\s+[—-]\s+\d{4}-\d{2}-\d{2}\s*$",
changelog,
re.MULTILINE,
)

if not match:
raise SystemExit("ERROR: Version heading not found in CHANGELOG.md")

version = match.group(1).strip()
print(f"Version found: {version}")

if not readme_path.is_file():
raise SystemExit("ERROR: README.md not found")

readme = readme_path.read_text(encoding="utf-8")

readme, version_count = re.subn(
r"(^Текущая версия прошивки:\s*)**.*?**",
lambda m: m.group(1) + "" + version + "",
readme,
count=1,
flags=re.MULTILINE,
)

if version_count != 1:
raise SystemExit("ERROR: Firmware version line not found in README.md")

safe_version = quote(version, safe=".-_")
demo_url = (
"https://dsr987.github.io/lego-technic-esp32/"
"index.html?v=" + safe_version
)

readme, url_count = re.subn(
r"https://dsr987.github.io/lego-technic-esp32/"
r"index.html(?:?[^\s)"']*)?",
lambda m: demo_url,
readme,
)

if url_count == 0:
raise SystemExit("ERROR: Demo URL not found in README.md")

readme_path.write_text(readme, encoding="utf-8", newline="\n")

print(f"README version updated: {version}")
print(f"Demo URL updated: {demo_url}")