
#!/usr/bin/env python3
"""Обновляет версию прошивки и ссылку на демо в README.md."""

import re
import sys
from pathlib import Path
from urllib.parse import quote

ROOT_DIR = Path(__file__).resolve().parent.parent
CHANGELOG_PATH = ROOT_DIR / "CHANGELOG.md"
README_PATH = ROOT_DIR / "README.md"

DEMO_URL = "https://dsr987.github.io/lego-technic-esp32/index.html"


def get_version() -> str:
    """Извлекает актуальную версию из первого заголовка версии в CHANGELOG.md."""
    if not CHANGELOG_PATH.exists():
        raise FileNotFoundError(f"Не найден файл: {CHANGELOG_PATH}")

    changelog = CHANGELOG_PATH.read_text(encoding="utf-8-sig")

    for line in changelog.splitlines():
        match = re.match(r"^##\s+(.+?)\s*$", line)

        if not match:
            continue

        heading = match.group(1).strip()

        # Пропускаем общий заголовок истории версий.
        if heading.lower().startswith(("история версий", "changelog")):
            continue

        # Убираем дату, если заголовок имеет формат:
        # ## 0.3.3 — 2026-10-10
        heading = re.sub(
            r"\s+[—–-]\s+\d{4}-\d{2}-\d{2}\s*$",
            "",
            heading,
        ).strip()

        if heading:
            return heading

    raise ValueError("Не удалось найти версию в заголовках CHANGELOG.md")


def update_readme(version: str) -> None:
    """Обновляет строку версии и параметр v в URL демо."""
    if not README_PATH.exists():
        raise FileNotFoundError(f"Не найден файл: {README_PATH}")

    readme = README_PATH.read_text(encoding="utf-8-sig")

    # Обновляем строку версии, независимо от того,
    # выделена жирным вся строка или только номер версии.
    version_pattern = re.compile(
        r"^\s*\**\s*Текущая версия прошивки\s*:\s*"
        r".*?\s*\**\s*$",
        re.MULTILINE | re.IGNORECASE,
    )

    readme, version_count = version_pattern.subn(
        f"**Текущая версия прошивки: {version}**",
        readme,
        count=1,
    )

    if version_count == 0:
        raise ValueError(
            "ERROR: Firmware version line not found in README.md"
        )

    # Обновляем версию в ссылке на демо, сохраняя остальную ссылку.
    encoded_version = quote(version, safe="")
    demo_pattern = re.compile(
        r"(" + re.escape(DEMO_URL) + r"\?v=)[^)\s\"']+"
    )

    readme, demo_count = demo_pattern.subn(
        lambda match: match.group(1) + encoded_version,
        readme,
    )

    if demo_count == 0:
        raise ValueError(
            "ERROR: Demo URL with ?v= parameter not found in README.md"
        )

    README_PATH.write_text(readme, encoding="utf-8")

    print(f"README.md updated successfully.")
    print(f"Firmware version: {version}")
    print(f"Version lines updated: {version_count}")
    print(f"Demo URLs updated: {demo_count}")


def main() -> int:
    try:
        version = get_version()
        print(f"Version found: {version}")
        update_readme(version)
        return 0
    except (OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
