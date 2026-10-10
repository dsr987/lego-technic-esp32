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
print("Version found: " + version)

if not readme_path.is_file():
raise SystemExit("ERROR: README.md not found")

readme = readme_path.read_text(encoding="utf-8")

version_pattern = re.compile(
r"(^Текущая версия прошивки:\s*)**.*?**",
re.MULTILINE,
)

readme, version_count = version_pattern.subn(
lambda m: m.group(1) + "" + version + "",
readme,
count=1,
)

if version_count != 1:
raise SystemExit("ERROR: Firmware version line not found in README.md")

demo_url = (
"https://dsr987.github.io/lego-technic-esp32/index.html?v="
+ quote(version, safe=".-_")
)

url_pattern = re.compile(
r"https://dsr987.github.io/lego-technic-esp32/index.html(?:?v=[^)\s]*)?"
)

readme, url_count = url_pattern.subn(lambda m: demo_url, readme)

if url_count == 0:
raise SystemExit("ERROR: Demo URL not found in README.md")

readme_path.write_text(readme, encoding="utf-8", newline="\n")

print("README version updated: " + version)
print("Demo URL updated: " + demo_url)