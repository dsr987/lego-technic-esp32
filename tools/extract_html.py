
#!/usr/bin/env python3
"""Извлекает PAGE_HTML из файла C++ в index.html."""

import re
import sys
from pathlib import Path


def extract_html(input_file: str, output_file: str) -> None:
    source_path = Path(input_file)
    output_path = Path(output_file)

    if not source_path.is_file():
        raise SystemExit(f"ОШИБКА: исходный файл не найден: {source_path}")

    content = source_path.read_text(encoding="utf-8")

    # Locate the PAGE_HTML declaration and its raw C++ string.
    declaration = re.search(
        r"\bPAGE_HTML\b[^=]*=\s*R\"HTML\(",
        content,
    )

    if declaration is None:
        raise SystemExit(
            f"ОШИБКА: не найдена строка PAGE_HTML в {source_path}"
        )

    html_start = declaration.end()
    html_end = content.find(")HTML\"", html_start)

    if html_end == -1:
        raise SystemExit(
            f"ERROR: Closing )HTML\" delimiter not found in {source_path}"
        )

    html = content[html_start:html_end]

    if not html.strip():
        raise SystemExit("ОШИБКА: извлечённый HTML пуст")

    if not re.search(r"<!DOCTYPE\s+html|<html\b", html, re.IGNORECASE):
        raise SystemExit("ОШИБКА: извлечённое содержимое не похоже на HTML")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(html, encoding="utf-8", newline="\n")

    print(f"ГОТОВО: HTML извлечён из {source_path}")
    print(f"Результат: {output_path}")
    print(f"Символов: {len(html)}")
    print(f"Строк: {html.count(chr(10)) + 1}")


if __name__ == "__main__":
    source = (
        sys.argv[1]
        if len(sys.argv) > 1
        else "lego_technic_esp32/web_page.cpp"
    )
    destination = sys.argv[2] if len(sys.argv) > 2 else "index.html"

    extract_html(source, destination)
