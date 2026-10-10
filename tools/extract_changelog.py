#!/usr/bin/env python3
"""Extract a marked CHANGELOG block from app.cpp."""

import re
import sys
from datetime import date
from pathlib import Path


def clean_line(raw):
    line = raw.strip()

    if line.startswith("/*"):
        line = line[2:].strip()

    if line.startswith("*/"):
        line = line[2:].strip()

    if line.startswith("*"):
        line = line[1:].strip()
    elif line.startswith("//"):
        line = line[2:].strip()

    return line


def extract_changelog(cpp_file):
    path = Path(cpp_file)

    if not path.is_file():
        raise SystemExit(f"ERROR: File not found: {path}")

    version = None
    started = False
    ended = False
    entries = []

    for raw in path.read_text(encoding="utf-8").splitlines():
        line = clean_line(raw)

        if line.startswith("CHANGELOG_VERSION:"):
            version = line.split(":", 1)[1].strip()
            continue

        if line == "CHANGELOG_START":
            started = True
            continue

        if line == "CHANGELOG_END":
            ended = True
            break

        if started and line.startswith("- "):
            entries.append(line)

    if not version:
        raise SystemExit("ERROR: CHANGELOG_VERSION marker not found")

    if not started:
        raise SystemExit("ERROR: CHANGELOG_START marker not found")

    if not ended:
        raise SystemExit("ERROR: CHANGELOG_END marker not found")

    if not entries:
        raise SystemExit(f"ERROR: No entries found for version {version}")

    return version, "\n".join(entries)


def update_changelog(md_file, version, entries):
    path = Path(md_file)
    content = (
        path.read_text(encoding="utf-8")
        if path.exists()
        else "# История версий\n\n"
    )

    version_pattern = re.compile(
        r"^##\s+" + re.escape(version) + r"\s*[—-]",
        re.MULTILINE,
    )

    if version_pattern.search(content):
        print(f"OK: Version {version} already exists; no changes.")
        return

    section = (
        f"## {version} — {date.today().isoformat()}\n"
        f"### Изменено\n"
        f"{entries}\n\n"
    )

    heading = re.search(r"^# .+$", content, re.MULTILINE)

    if heading:
        position = heading.end()
        content = (
            content[:position]
            + "\n\n"
            + section
            + content[position:].lstrip("\n")
        )
    else:
        content = "# История версий\n\n" + section + content

    path.write_text(content, encoding="utf-8", newline="\n")
    print(f"OK: Added CHANGELOG version {version}")


if __name__ == "__main__":
    cpp_file = (
        sys.argv[1]
        if len(sys.argv) > 1
        else "lego_technic_esp32/app.cpp"
    )
    md_file = sys.argv[2] if len(sys.argv) > 2 else "CHANGELOG.md"

    version, entries = extract_changelog(cpp_file)
    print(f"Version: {version}")
    print(entries)
    update_changelog(md_file, version, entries)