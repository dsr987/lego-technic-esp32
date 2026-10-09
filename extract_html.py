#!/usr/bin/env python3
"""
Извлекает HTML-страницу из app.cpp (константа PAGE_HTML)
и сохраняет её как отдельный файл index.html.
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

    # Ищем начало блока HTML
    start_match = re.search(r'R"HTML\(', content)
    
    if not start_match:
        print("❌ Не найдено начало HTML блока (R\"HTML(\")")
        sys.exit(1)

    # Ищем конец блока - ищем первое вхождение )HTML" после начала
    html_start = start_match.end()
    remaining_content = content[html_start:]
    
    # Ищем )HTML" с возможными символами после (точка с запятой, пробелы)
    end_match = re.search(r'\)HTML"[;]?\s*\n', remaining_content)
    
    if not end_match:
        # Пробуем найти просто )HTML"
        end_match = re.search(r'\)HTML"', remaining_content)
    
    if not end_match:
        print("❌ Не найден конец HTML блока")
        print("💡 Проверьте, что в app.cpp блок заканчивается строкой: )HTML\";")
        print("💡 Или пришлите последние 5 строк HTML блока из app.cpp")
        sys.exit(1)

    # Извлекаем HTML
    html = remaining_content[:end_match.start()]

    Path(output_file).write_text(html, encoding='utf-8')
    print(f"✅ HTML успешно извлечён: {output_file}")
    print(f"   Размер: {len(html)} байт, строк: {html.count(chr(10)) + 1}")

if __name__ == "__main__":
    in_file = sys.argv[1] if len(sys.argv) > 1 else "lego_technic_esp32/app.cpp"
    out_file = sys.argv[2] if len(sys.argv) > 2 else "index.html"
    extract_html(in_file, out_file)