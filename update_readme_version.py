#!/usr/bin/env python3
"""
Автоматически обновляет версию прошивки и ссылку на демо GUI в README.md
"""

import re
from pathlib import Path

def get_version_from_cpp(cpp_file="lego_technic_esp32/app.cpp"):
    path = Path(cpp_file)
    if not path.exists():
        print(f"❌ Файл {cpp_file} не найден")
        return None
    
    content = path.read_text(encoding='utf-8')
    match = re.search(r'Версия:\s*([\d.]+(?:\s+\w+)?)', content)
    
    if match:
        version = match.group(1).strip()
        print(f"✅ Найдена версия в app.cpp: {version}")
        return version
    else:
        print("❌ Версия не найдена в app.cpp")
        return None

def update_readme(readme_file="README.md", new_version=None):
    if not new_version:
        print("❌ Не передана версия")
        return False
    
    path = Path(readme_file)
    if not path.exists():
        print(f"❌ Файл {readme_file} не найден")
        return False
    
    content = path.read_text(encoding='utf-8')
    
    # 1. Обновляем текст версии
    def replace_version_text(match):
        prefix = match.group(1)
        return prefix + new_version
    
    pattern_version = r'(Текущая версия прошивки:\s*)([\d.]+\s*(?:beta|alpha|rc)?\s*)'
    content = re.sub(pattern_version, replace_version_text, content, flags=re.IGNORECASE)
    
    # 2. Обновляем ссылку на GitHub Pages (добавляем ?v=VERSION для обхода кэша)
    pages_url = f"https://dsr987.github.io/lego-technic-esp32/index.html?v={new_version}"
    
    # Заменяем любую старую ссылку на index.html
    old_links = [
        r'https://raw\.githack\.com/dsr987/lego-technic-esp32/main/index\.html(?:\?v=[\d.]+)?',
        r'https://dsr987\.github\.io/lego-technic-esp32/index\.html(?:\?v=[\d.]+)?',
    ]
    for old_link_pattern in old_links:
        content = re.sub(old_link_pattern, pages_url, content)
    
    path.write_text(content, encoding='utf-8')
    print(f"✅ README.md обновлен: версия {new_version}, ссылка на GUI обновлена")
    return True

if __name__ == "__main__":
    version = get_version_from_cpp()
    if version:
        update_readme(new_version=version)