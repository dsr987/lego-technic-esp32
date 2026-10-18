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
    version_match = version_pattern.search(content)
    entry_lines = list(dict.fromkeys(
        line.strip() for line in entries.splitlines() if line.strip().startswith("- ")
    ))
    entry_text = "\n".join(entry_lines)

    if version_match:
        section_start = version_match.start()
        next_heading = re.search(
            r"^##\s+",
            content[version_match.end():],
            re.MULTILINE,
        )
        section_end = (
            version_match.end() + next_heading.start()
            if next_heading
            else len(content)
        )
        section_text = content[section_start:section_end]
        existing_entries = {
            line.strip()
            for line in section_text.splitlines()
            if line.strip().startswith("- ")
        }
        existing_pr_refs = set(re.findall(r"\[PR #([0-9]+)\]", section_text))
        missing_entries = []
        for line in entry_lines:
            if line in existing_entries:
                continue
            incoming_pr_refs = set(re.findall(r"\[PR #([0-9]+)\]", line))
            # CHANGELOG.md may contain expanded notes for a PR while app.cpp
            # contains only that PR's compact summary. Do not append a second,
            # less detailed summary when that PR is already represented.
            if incoming_pr_refs and incoming_pr_refs.issubset(existing_pr_refs):
                continue
            missing_entries.append(line)

        if not missing_entries:
            print(f"OK: Version {version} already contains all changelog entries; no changes.")
            return

        section_heading = re.search(r"^##\s+[^\n]+", section_text, re.MULTILINE)
        if section_heading is None:
            raise SystemExit(f"ERROR: Could not locate heading for version {version}")

        changed_heading = re.search(
            r"^###\s+Изменено\s*$",
            section_text[section_heading.end():],
            re.MULTILINE,
        )
        if changed_heading:
            subsection_start = section_heading.end() + changed_heading.end()
            next_subsection = re.search(
                r"^###\s+",
                section_text[subsection_start:],
                re.MULTILINE,
            )
            subsection_end = (
                subsection_start + next_subsection.start()
                if next_subsection
                else len(section_text)
            )
            existing_block = section_text[subsection_start:subsection_end].rstrip()
            suffix = section_text[subsection_end:].lstrip("\n")
            updated_block = (
                existing_block
                + ("\n" if existing_block else "")
                + "\n".join(missing_entries)
                + "\n\n"
            )
            section_text = (
                section_text[:subsection_start]
                + updated_block
                + suffix
            )
        else:
            insert_at = section_heading.end()
            added_section = (
                "\n### Изменено\n"
                + "\n".join(missing_entries)
                + "\n\n"
            )
            section_text = (
                section_text[:insert_at]
                + added_section
                + section_text[insert_at:].lstrip("\n")
            )

        content = content[:section_start] + section_text + content[section_end:]
        path.write_text(content, encoding="utf-8", newline="\n")
        print(f"OK: Updated CHANGELOG version {version} with {len(missing_entries)} new changelog entries.")
        return

    section = (
        f"## {version} — {date.today().isoformat()}\n"
        f"### Изменено\n"
        f"{entry_text}\n\n"
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