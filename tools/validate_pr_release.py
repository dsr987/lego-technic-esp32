#!/usr/bin/env python3
"""Validate release labels and user-facing changelog text on a pull request."""
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "lego_technic_esp32" / "app.cpp"
VALID = {"release:patch", "release:minor", "release:stable", "release:none"}


def error(message: str) -> None:
    print(f"::error::{message}")
    raise SystemExit(1)


def main() -> None:
    try:
        labels = json.loads(os.environ.get("PR_LABELS_JSON", "[]"))
    except json.JSONDecodeError:
        error("Could not parse PR labels")
    selected = set(labels) & VALID
    if len(selected) != 1:
        error("Add exactly one release label: release:patch, release:minor, release:stable, or release:none.")

    release = next(iter(selected))
    body = os.environ.get("PR_BODY") or ""
    match = re.search(r"(?ims)^##\s+Changelog\s*\n(.*?)(?=^##\s+|\Z)", body)
    if release != "release:none":
        if not match:
            error("Add a '## Changelog' section to the PR description.")
        bullets = [
            re.sub(r"^\s*(?:[-*+]\s+|\d+[.)]\s+)", "", line).strip()
            for line in match.group(1).splitlines()
        ]
        bullets = [line for line in bullets if line and not line.startswith("<!--")]
        if not bullets:
            error("The '## Changelog' section needs at least one bullet.")
    version_match = re.search(r"(?m)^\s*\* CHANGELOG_VERSION: (\d+)\.(\d+)\.(\d+)\s*$", APP.read_text(encoding="utf-8"))
    if not version_match:
        error("Could not read CHANGELOG_VERSION from app.cpp")
    major, minor, patch = map(int, version_match.groups())
    current = f"{major}.{minor}.{patch}"
    if release == "release:patch":
        proposed = f"{major}.{minor}.{patch + 1}"
    elif release == "release:minor":
        proposed = f"{major}.{minor + 1}.0" if major == 0 else f"{major}.{minor + 1}.0"
    elif release == "release:stable":
        if major != 0:
            error("release:stable is only valid when promoting a 0.x.y version to 1.0.0.")
        proposed = "1.0.0"
    else:
        proposed = current + " (unchanged)"
    summary = f"### Release metadata\n\n- Label: `{release}`\n- Current version on base: `{current}`\n- Proposed version: `{proposed}`\n"
    print(summary)
    with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as stream:
        stream.write(summary)


if __name__ == "__main__":
    main()
