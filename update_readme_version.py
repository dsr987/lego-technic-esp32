#!/usr/bin/env python3
"""
Автоматически обновляет версию прошивки в README.md
на основе версии из app.cpp
"""

import re
from pathlib import Path

def get_version_from_cpp(cpp_file="lego_technic_esp32/app.cpp"):
    """Извлекает версию из комментария в app.cpp"""
    path = Path(cpp_file)
    if not path.exists():
        print(f"❌ Файл {cpp_file} не найден")
        return None
    
    content = path.read_text(encoding='utf-8')
    
    # Ищем строку вида: // ESP32 Lego Technic motorization — Версия: 0.2.34
    match = re.search(r'Версия:\s*([\d.]+(?:\s+\w+)?)', content)
    
    if match:
        version = match.group(1).strip()
        print(f"✅ Найдена версия в app.cpp: {version}")
        return version
    else:
        print("❌ Версия не найдена в app.cpp")
        return None

def update_readme(readme_file="README.md", new_version=None):
    """Обновляет версию в README.md"""
    if not new_version:
        print("❌ Не передана версия для обновления")
        return False
    
    path = Path(readme_file)
    if not path.exists():
        print(f"❌ Файл {readme_file} не найден")
        return False
    
    content = path.read_text(encoding='utf-8')
    
    # Ищем и заменяем версию в строке "Текущая версия прошивки: X.Y.Z"
    old_pattern = r'(Текущая версия прошивки:\s*)([\d.]+\s*(?:beta|alpha|rc)?\s*)'
    new_text = r'\1' + new_version
    
    updated_content = re.sub(old_pattern, new_text, content, flags=re.IGNORECASE)
    
    if updated_content == content:
        print("⚠️  Версия в README не изменилась или не найдена")
        return False
    
    path.write_text(updated_content, encoding='utf-8')
    print(f"✅ README.md обновлен: версия {new_version}")
    return True

if __name__ == "__main__":
    version = get_version_from_cpp()
    if version:
        update_readme(new_version=version)