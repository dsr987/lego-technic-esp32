#!/usr/bin/env python3
"""Extract a marked CHANGELOG block from app.cpp."""

import json
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


def update_changelog(md_file, version, entries, refresh_existing=False):
    path = Path(md_file)
    content = (
        path.read_text(encoding="utf-8")
        if path.exists()
        else "# История версий\n\n"
    )

    section = (
        f"## {version} — {date.today().isoformat()}\n"
        f"### Изменено\n"
        f"{entries}\n\n"
    )

    version_section = re.compile(
        r"(?ms)^##\s+" + re.escape(version) + r"\s*[—-].*?(?=^##\s+|\Z)"
    )
    match = version_section.search(content)
    if match and not refresh_existing:
        print(f"OK: Version {version} already exists; no changes.")
        return
    if match:
        existing_section = match.group(0)
        change_heading = re.search(r"(?m)^### Изменено\\s*$", existing_section)
        new_entries = [line for line in entries.splitlines() if line.strip()]

        if change_heading:
            body_start = change_heading.end()
            next_subsection = re.search(r"(?m)^###\\s+", existing_section[body_start:])
            body_end = (
                body_start + next_subsection.start()
                if next_subsection
                else len(existing_section)
            )
            existing_body = existing_section[body_start:body_end]
            existing_lines = existing_body.splitlines()
            existing_entry_lines = {line.strip() for line in existing_lines if line.strip().startswith("- ")}
            missing_entries = [line for line in new_entries if line.strip() not in existing_entry_lines]
            if missing_entries:
                updated_body = existing_body.rstrip() + "\\n" + "\\n".join(missing_entries) + "\\n"
                existing_section = (
                    existing_section[:body_start]
                    + updated_body
                    + existing_section[body_end:]
                )
        else:
            heading_end = existing_section.find("\\n") + 1
            inserted = "### Изменено\\n" + "\\n".join(new_entries) + "\\n\\n"
            existing_section = (
                existing_section[:heading_end]
                + "\\n"
                + inserted
                + existing_section[heading_end:]
            )

        content = content[:match.start()] + existing_section + content[match.end():]
        print(f"OK: Updated CHANGELOG version {version}")
    else:
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
            content = "# История версий\n\n" + section
        print(f"OK: Added CHANGELOG version {version}")

    path.write_text(content, encoding="utf-8", newline="\n")


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
    state_path = Path(__file__).resolve().parent.parent / ".github" / "release-automation" / "state.json"
    refresh_existing = False
    if state_path.is_file():
        try:
            state = json.loads(state_path.read_text(encoding="utf-8"))
            refresh_existing = (
                state.get("pre_release_active") is True
                and state.get("pre_release_version") == version
            )
        except (OSError, json.JSONDecodeError) as error:
            raise SystemExit(f"ERROR: Cannot read pre-release state: {error}") from error

    update_changelog(md_file, version, entries, refresh_existing=refresh_existing)