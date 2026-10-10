#!/usr/bin/env python3
"""Update the firmware version and demo URL in README.md."""

import re
import sys
from pathlib import Path
from urllib.parse import quote

CHANGELOG_FILE = Path("CHANGELOG.md")
README_FILE = Path("README.md")
PAGES_BASE_URL = (
"https://dsr987.github.io/lego-technic-esp32/index.html"
)

def get_version_from_changelog(changelog_file=CHANGELOG_FILE):
"""Read the latest version from the first CHANGELOG heading."""
path = Path(changelog_file)

if not path.is_file():
    raise SystemExit(f"ERROR: File not found: {path}")

content = path.read_text(encoding="utf-8")

match = re.search(
    r"^##\s+(.+?)\s+[—-]\s+\d{4}-\d{2}-\d{2}\s*$",
    content,
    re.MULTILINE,
)

if not match:
    raise SystemExit(
        f"ERROR: Latest version heading not found in {path}"
    )

version = match.group(1).strip()
print(f"Version found: {version}")
return version

def update_readme(new_version, readme_file=README_FILE):
"""Update the version label and cache-busting demo URL."""
path = Path(readme_file)

if not path.is_file():
    raise SystemExit(f"ERROR: File not found: {path}")

content = path.read_text(encoding="utf-8")

version_pattern = re.compile(
    r"(^Текущая версия прошивки:\s*)\*\*.*?\*\*",
    re.MULTILINE,
)

content, version_count = version_pattern.subn(
    lambda match: f"{match.group(1)}**{new_version}**",
    content,
    count=1,
)

if version_count != 1:
    raise SystemExit(
        "ERROR: Firmware version line not found in README.md"
    )

safe_version = quote(new_version, safe=".-_")
pages_url = f"{PAGES_BASE_URL}?v={safe_version}"

url_pattern = re.compile(
    r"https://dsr987\.github\.io/lego-technic-esp32/"
    r"index\.html(?:\?[^\s)\"']*)?"
)

content, url_count = url_pattern.subn(pages_url, content)

if url_count == 0:
    raise SystemExit("ERROR: Demo URL not found in README.md")

path.write_text(content, encoding="utf-8", newline="\n")
print(f"README version updated: {new_version}")
print(f"Demo URL updated: {pages_url}")

if name == "main":
version = get_version_from_changelog()
update_readme(version)