#!/usr/bin/env python3
"""Extract a marked CHANGELOG block from app.cpp."""

import re
import sys
from datetime import date
from pathlib import Path

VERSION_MARKER = "CHANGELOG_VERSION:"
START_MARKER = "CHANGELOG_START"
END_MARKER = "CHANGELOG_END"

VERSION_PATTERN = re.compile(
r"^##\s+([\d.]+(?:\s+\w+)?)\s*[—-]",
re.MULTILINE,
)

def normalize_comment_line(raw_line):
"""Remove C/C++ comment prefixes from one line."""
line = raw_line.strip()

if line.startswith("/*"):
    line = line[2:].strip()
elif line.startswith("*/"):
    line = line[2:].strip()

if line.startswith("*"):
    line = line[1:].strip()
elif line.startswith("//"):
    line = line[2:].strip()

return line

def extract_changelog_from_cpp(cpp_file):
"""Extract entries between CHANGELOG_START and CHANGELOG_END."""
path = Path(cpp_file)

if not path.is_file():
    raise SystemExit(f"ERROR: File not found: {path}")

content = path.read_text(encoding="utf-8")
version = None
started = False
ended = False
entries = []

for raw_line in content.splitlines():
    line = normalize_comment_line(raw_line)

    if line.startswith(VERSION_MARKER):
        version = line[len(VERSION_MARKER):].strip()
        continue

    if line == START_MARKER:
        if started:
            raise SystemExit("ERROR: Duplicate CHANGELOG_START marker")
        started = True
        continue

    if line == END_MARKER:
        if not started:
            raise SystemExit("ERROR: CHANGELOG_END before CHANGELOG_START")
        ended = True
        break

    if started:
        if not line:
            continue
        if line.startswith("- "):
            entries.append(line)
        else:
            raise SystemExit(
                f"ERROR: Unexpected line inside CHANGELOG block: {line}"
            )

if not version:
    raise SystemExit(
        f"ERROR: {VERSION_MARKER} marker not found in {path}"
    )

if not started:
    raise SystemExit(
        f"ERROR: {START_MARKER} marker not found in {path}"
    )

if not ended:
    raise SystemExit(
        f"ERROR: {END_MARKER} marker not found in {path}"
    )

if not entries:
    raise SystemExit(
        f"ERROR: No changelog entries found for version {version}"
    )

return version, "\n".join(entries)

def update_changelog_md(md_file, version, changelog_text):
"""Insert a new version after the main heading, preserving history."""
path = Path(md_file)

if path.exists():
    content = path.read_text(encoding="utf-8")
else:
    content = "# История версий\n\n"

existing_versions = {
    match.group(1).strip()
    for match in VERSION_PATTERN.finditer(content)
}

if version in existing_versions:
    print(f"OK: Version {version} already exists; no changes.")
    return False

new_section = (
    f"## {version} — {date.today().isoformat()}\n"
    f"### Изменено\n"
    f"{changelog_text}\n\n"
)

heading = re.search(r"^# .+$", content, re.MULTILINE)

if heading:
    insert_pos = heading.end()
    content = (
        content[:insert_pos]
        + "\n\n"
        + new_section
        + content[insert_pos:].lstrip("\n")
    )
else:
    content = "# История версий\n\n" + new_section + content

path.write_text(content, encoding="utf-8", newline="\n")
print(f"OK: Added CHANGELOG version {version}")
return True

if name == "main":
cpp_file = (
sys.argv[1]
if len(sys.argv) > 1
else "lego_technic_esp32/app.cpp"
)
md_file = (
sys.argv[2]
if len(sys.argv) > 2
else "CHANGELOG.md"
)

version, changelog = extract_changelog_from_cpp(cpp_file)

print(f"Version: {version}")
print(changelog)

update_changelog_md(md_file, version, changelog)