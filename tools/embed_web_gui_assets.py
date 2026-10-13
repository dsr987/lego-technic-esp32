#!/usr/bin/env python3
"""Embed optimized WebP assets into the self-contained Web GUI SVG."""

import base64
import binascii
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAGE = ROOT / "lego_technic_esp32" / "web_page.cpp"
ASSETS = {
    "car-body": ROOT / "lego_technic_esp32" / "assets" / "classic_chassis.webp.b64",
    "wheel-art": ROOT / "lego_technic_esp32" / "assets" / "classic_wheel.webp.b64",
}


def read_webp(path: Path) -> str:
    payload = "".join(path.read_text(encoding="ascii").split())
    try:
        decoded = base64.b64decode(payload, validate=True)
    except (ValueError, binascii.Error) as exc:
        raise SystemExit(f"ERROR: invalid base64 asset: {path.relative_to(ROOT)}: {exc}") from exc

    if len(decoded) < 12 or decoded[:4] != b"RIFF" or decoded[8:12] != b"WEBP":
        raise SystemExit(f"ERROR: asset is not a WebP image: {path.relative_to(ROOT)}")

    declared_size = int.from_bytes(decoded[4:8], "little") + 8
    if declared_size != len(decoded):
        raise SystemExit(
            f"ERROR: WebP size mismatch in {path.relative_to(ROOT)}: "
            f"header={declared_size}, actual={len(decoded)}"
        )

    return payload


def main() -> int:
    original = PAGE.read_text(encoding="utf-8")
    content = original

    for element_id, path in ASSETS.items():
        payload = read_webp(path)
        pattern = re.compile(
            rf'(<image id="{re.escape(element_id)}" href="data:image/webp;base64,)[^"]*(")'
        )
        content, count = pattern.subn(rf"\g<1>{payload}\g<2>", content)
        if count != 1:
            raise SystemExit(
                f"ERROR: expected exactly one embedded image for {element_id}, found {count}"
            )

    if content != original:
        PAGE.write_text(content, encoding="utf-8", newline="\n")
        print("OK: embedded optimized chassis and wheel WebP assets")
    else:
        print("OK: Web GUI image assets are already up to date")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
