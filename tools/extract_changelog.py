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
        r"^##\s+" + re.escape(version) + r"\s*[—-].*$",
        re.MULTILINE,
    )
    version_match = version_pattern.search(content)
    all_sections = list(re.finditer(r"^##\s+.+$", content, re.MULTILINE))

    if version_match:
        # Repeated pre-release builds reuse one numeric version. Refresh only the
        # newest section; a stale build must not rewrite an older historical entry.
        if not all_sections or all_sections[0].start() != version_match.start():
            print(f"OK: Version {version} already exists in history; leaving older entry unchanged.")
            return

        heading_end = content.find("\n", version_match.start())
        if heading_end == -1:
            heading_end = len(content)
        heading_line = content[version_match.start():heading_end]
        next_heading = next(
            (section.start() for section in all_sections if section.start() > version_match.start()),
            len(content),
        )
        section = (
            f"{heading_line}\n"
            f"### Изменено\n"
            f"{entries}\n\n"
        )
        if content[version_match.start():next_heading] == section:
            print(f"OK: Version {version} changelog is already current.")
            return

        content = content[:version_match.start()] + section + content[next_heading:].lstrip("\n")
        path.write_text(content, encoding="utf-8", newline="\n")
        print(f"OK: Updated CHANGELOG version {version}")
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