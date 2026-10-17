#!/usr/bin/env python3
"""Synchronize the web GUI header version with CHANGELOG_VERSION in app.cpp."""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
APP_CPP = ROOT / "lego_technic_esp32" / "app.cpp"
WEB_PAGE_CPP = ROOT / "lego_technic_esp32" / "web_page.cpp"
INDEX_HTML = ROOT / "index.html"


def replace_brand_version(text: str, version: str) -> tuple[str, int]:
    """Keep the beta suffix for 0.x firmware and remove it from stable 1.x+."""
    major = int(version.split(".", 1)[0])
    suffix = " · beta" if major == 0 else ""
    pattern = re.compile(
        r'(<div class="brand-ver">)\d+\.\d+\.\d+(\s*·\s*beta)?(</div>)'
    )
    return pattern.subn(
        lambda match: f"{match.group(1)}{version}{suffix}{match.group(3)}",
        text,
    )


def main() -> int:
    app = APP_CPP.read_text(encoding="utf-8-sig")
    version_match = re.search(
        r"^\s*\*?\s*CHANGELOG_VERSION:\s*(\d+\.\d+\.\d+)\s*$",
        app,
        re.MULTILINE,
    )
    if not version_match:
        print(f"ERROR: CHANGELOG_VERSION not found in {APP_CPP}", file=sys.stderr)
        return 1

    version = version_match.group(1)

    updated = 0
    for path in (WEB_PAGE_CPP, INDEX_HTML):
        if not path.exists():
            # index.html may be generated later in the workflow, so only the
            # embedded source is required to exist at this point.
            if path == INDEX_HTML:
                continue
            print(f"ERROR: Required GUI source not found: {path}", file=sys.stderr)
            return 1

        text = path.read_text(encoding="utf-8-sig")
        text, count = replace_brand_version(text, version)
        if count != 1:
            print(
                f"ERROR: Expected exactly one GUI version label in {path}, found {count}",
                file=sys.stderr,
            )
            return 1

        path.write_text(text, encoding="utf-8")
        print(f"Updated GUI version to {version}: {path.relative_to(ROOT)}")
        updated += count

    if updated == 0:
        print("ERROR: No GUI version labels were updated", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
