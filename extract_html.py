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

    # Ищем начало блока HTML (гибкий поиск)
    # Варианты: R"HTML(", R"html(", R"(...)", const char PAGE_HTML[] = R"HTML(
    start_patterns = [
        r'R"HTML\(',
        r'R"html\(',
        r'R"\(',
        r'PAGE_HTML\[\]\s*(?:PROGMEM\s*)?=\s*R"HTML\(',
    ]
    
    start_match = None
    for pattern in start_patterns:
        start_match = re.search(pattern, content)
        if start_match:
            break
    
    if not start_match:
        print("❌ Не найдено начало HTML блока (R\"HTML(\" или аналог)")
        print(" Проверьте, что в app.cpp есть строка вида: const char PAGE_HTML[] PROGMEM = R\"HTML(")
        sys.exit(1)

    # Ищем конец блока
    end_patterns = [
        r'\)HTML";',
        r'\)html";',
        r'\)";',
    ]
    
    end_match = None
    for pattern in end_patterns:
        end_match = re.search(pattern, content[start_match.end():])
        if end_match:
            break
    
    if not end_match:
        print("❌ Не найден конец HTML блока")
        sys.exit(1)

    # Извлекаем HTML между началом и концом
    html_start = start_match.end()
    html_end = html_start + end_match.start()
    html = content[html_start:html_end]

    Path(output_file).write_text(html, encoding='utf-8')
    print(f"✅ HTML успешно извлечён: {output_file}")
    print(f"   Размер: {len(html)} байт, строк: {html.count(chr(10)) + 1}")

if __name__ == "__main__":
    in_file = sys.argv[1] if len(sys.argv) > 1 else "lego_technic_esp32/app.cpp"
    out_file = sys.argv[2] if len(sys.argv) > 2 else "index.html"
    extract_html(in_file, out_file)