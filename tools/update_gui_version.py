#!/usr/bin/env python3
"""Синхронизирует версию в заголовке веб-интерфейса с CHANGELOG_VERSION в lego_technic_esp32.ino."""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAIN_SKETCH = ROOT / "lego_technic_esp32" / "lego_technic_esp32.ino"
WEB_PAGE_CPP = ROOT / "lego_technic_esp32" / "web_page.cpp"
INDEX_HTML = ROOT / "index.html"


def main() -> int:
    app = MAIN_SKETCH.read_text(encoding="utf-8-sig")
    version_match = re.search(
        r"^\s*\*?\s*CHANGELOG_VERSION:\s*(\d+\.\d+\.\d+)\s*$",
        app,
        re.MULTILINE,
    )
    if not version_match:
        print(f"ОШИБКА: CHANGELOG_VERSION не найден в {MAIN_SKETCH}", file=sys.stderr)
        return 1

    version = version_match.group(1)
    pattern = re.compile(r'(<div class="brand-ver">)\d+\.\d+\.\d+(\s*·\s*beta</div>)')

    updated = 0
    for path in (WEB_PAGE_CPP, INDEX_HTML):
        if not path.exists():
            # index.html may be generated later in the workflow, so only the
            # embedded source is required to exist at this point.
            if path == INDEX_HTML:
                continue
            print(f"ОШИБКА: не найден обязательный исходник интерфейса: {path}", file=sys.stderr)
            return 1

        text = path.read_text(encoding="utf-8-sig")
        text, count = pattern.subn(rf"\g<1>{version}\g<2>", text)
        if count != 1:
            print(
                f"ОШИБКА: ожидалась ровно одна метка версии интерфейса в {path}, найдено: {count}",
                file=sys.stderr,
            )
            return 1

        path.write_text(text, encoding="utf-8")
        print(f"Версия интерфейса обновлена до {version}: {path.relative_to(ROOT)}")
        updated += count

    if updated == 0:
        print("ОШИБКА: ни одна метка версии интерфейса не обновлена", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
