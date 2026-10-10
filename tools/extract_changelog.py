
#!/usr/bin/env python3
"""Extract the latest CHANGELOG block from app.cpp."""

import re
import sys
from datetime import date
from pathlib import Path


CHANGELOG_PATTERN = re.compile(
    r"^[ \t]*(?://|/\*|\*)[ \t]*CHANGELOG[ \t]+"
    r"([\d.]+(?:[ \t]+\w+)?)[ \t]*:[ \t]*$",
    re.MULTILINE,
)

VERSION_PATTERN = re.compile(
    r"^##\s+"
    r"([\d.]+(?:\s+\w+)?)\s*[—-]",
    re.MULTILINE,
)


def extract_changelog_from_cpp(cpp_file):
    """Read the first CHANGELOG block from app.cpp."""
    path = Path(cpp_file)

    if not path.is_file():
        raise SystemExit(f"ERROR: File not found: {path}")

    content = path.read_text(encoding="utf-8")
    match = CHANGELOG_PATTERN.search(content)

    if not match:
        raise SystemExit(
            f"ERROR: CHANGELOG marker not found in {path}"
        )

    version = match.group(1).strip()
    lines = []
    started = False

    for raw_line in content[match.end():].splitlines():
        line = raw_line.strip()

        if line.startswith("*/"):
            break

        if line.startswith("*"):
            line = line[1:].strip()
        elif line.startswith("//"):
            line = line[2:].strip()
        else:
            break

        if not line:
            if started:
                break
            continue

        if line.startswith("- "):
            lines.append(line)
            started = True
        elif started:
            break

    if not lines:
        raise SystemExit(
            f"ERROR: No changelog entries found for version {version}"
        )

    return version, "\n".join(lines)


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


if __name__ == "__main__":
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
