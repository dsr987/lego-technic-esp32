#!/usr/bin/env python3
"""
Извлекает HTML-страницу из app.cpp (константа PAGE_HTML)
и сохраняет её как отдельный файл index.html.
Запускается автоматически через GitHub Actions.
"""

import sys
import re
from pathlib import Path

def extract_html(input_file, output_file):
    path = Path(input_file)
    if not path.exists():
        print(f"❌ Файл не найден: {input_file}")
        sys.exit(1)

    content = path.read_text(encoding='utf-8')

    # Ищем блок между R"HTML( и )HTML";
    pattern = re.compile(r'R"HTML\((.*?)\)HTML";', re.DOTALL)
    match = pattern.search(content)

    if not match:
        print("❌ Не найден блок HTML между R\"HTML( и )HTML\";")
        sys.exit(1)

    html = match.group(1)
    Path(output_file).write_text(html, encoding='utf-8')
    print(f"✅ HTML извлечён: {output_file}")
    print(f"   Размер: {len(html)} байт, строк: {html.count(chr(10)) + 1}")

if __name__ == "__main__":
    in_file = sys.argv[1] if len(sys.argv) > 1 else "lego_technic_esp32/app.cpp"
    out_file = sys.argv[2] if len(sys.argv) > 2 else "index.html"
    extract_html(in_file, out_file)