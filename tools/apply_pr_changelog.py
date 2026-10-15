#!/usr/bin/env python3
"""Apply a merged PR's release metadata to app.cpp changelog.

This script runs only in the trusted post-merge workflow on main.
The event data is passed as JSON in PR_EVENT_PATH.
"""
from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "lego_technic_esp32" / "app.cpp"
VERSION_RE = re.compile(r"(?m)^\s*\* CHANGELOG_VERSION: (\d+)\.(\d+)\.(\d+)\s*$")
START = " * CHANGELOG_START"
END = " * CHANGELOG_END"


def fail(message: str) -> "None":
    print(f"::error::{message}")
    raise SystemExit(1)


def main() -> None:
    event_path = os.environ.get("PR_EVENT_PATH")
    if not event_path:
        fail("PR_EVENT_PATH is required")
    try:
        event = json.loads(Path(event_path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"Cannot read pull request event: {exc}")

    pr = event.get("pull_request", {})
    number = pr.get("number")
    if not isinstance(number, int) or number < 1:
        fail("Missing valid pull request number")

    labels = {
        label.get("name", "")
        for label in pr.get("labels", [])
        if isinstance(label, dict)
    }
    release_labels = labels & {
        "release:patch", "release:minor", "release:stable", "release:none"
    }
    if len(release_labels) != 1:
        fail(f"Expected exactly one release label, found: {sorted(release_labels)}")
    release_type = next(iter(release_labels))

    body = pr.get("body") or ""
    match = re.search(
        r"(?ims)^##\s+Changelog\s*\n(.*?)(?=^##\s+|\Z)", body
    )
    if release_type != "release:none" and not match:
        fail("Merged PR must contain a '## Changelog' section")
    notes = []
    if match:
        for line in match.group(1).splitlines():
            item = re.sub(r"^\s*(?:[-*+]\s+|\d+[.)]\s+)", "", line).strip()
            if item and not item.startswith("<!--"):
                notes.append(item)
    if release_type != "release:none" and not notes:
        fail("'## Changelog' section must contain at least one bullet")

    text = APP.read_text(encoding="utf-8")
    version_match = VERSION_RE.search(text)
    if not version_match:
        fail("CHANGELOG_VERSION not found in app.cpp")
    major, minor, patch = map(int, version_match.groups())
    current = f"{major}.{minor}.{patch}"

    # Idempotency: do not apply the same PR twice.
    marker = f"PR #{number}:"
    if marker in text:
        print(f"PR #{number} is already recorded; no changes made.")
        return

    if release_type == "release:patch":
        if major == 0:
            patch += 1
        else:
            patch += 1
    elif release_type == "release:minor":
        if major == 0:
            minor += 1
            patch = 0
        else:
            minor += 1
            patch = 0
    elif release_type == "release:stable":
        if major != 0:
            fail("release:stable is only valid while promoting a 0.x.y version")
        major, minor, patch = 1, 0, 0
    elif release_type == "release:none":
        print(f"PR #{number} is marked release:none; no version or changelog update.")
        return

    new_version = f"{major}.{minor}.{patch}"
    if new_version != current:
        text, count = VERSION_RE.subn(
            f" * CHANGELOG_VERSION: {new_version}", text, count=1
        )
        if count != 1:
            fail("Could not update CHANGELOG_VERSION")
        header_re = re.compile(r"(?m)^(\s*\* Версия разработки: )\d+\.\d+\.\d+")
        text, count = header_re.subn(rf"\g<1>{new_version}", text, count=1)
        if count != 1:
            fail("Could not update development version header")

    # PR text is untrusted input; keep it safe inside the C-style comment.
    def safe_comment(value: str) -> str:
        value = value.replace("*/", "* /")
        return re.sub(r"[\\x00-\\x08\\x0b\\x0c\\x0e-\\x1f]", "", value).strip()

    title = safe_comment(str(pr.get("title", "Merged pull request")).replace("\\n", " "))
    safe_notes = [safe_comment(note) for note in notes]
    safe_notes = [note for note in safe_notes if note]
    changelog = f" * {marker} {title}"
    if safe_notes:
        changelog += "\\n" + "\\n".join(f" *   - {note}" for note in safe_notes)
    start_idx = text.find(START)
    end_idx = text.find(END)
    if start_idx < 0 or end_idx < 0 or end_idx < start_idx:
        fail("CHANGELOG_START/CHANGELOG_END markers not found")
    insertion = text.find("\n", start_idx) + 1
    text = text[:insertion] + changelog + "\n" + text[insertion:]
    APP.write_text(text, encoding="utf-8")
    print(f"Recorded PR #{number}: {current} -> {new_version}")


if __name__ == "__main__":
    main()
