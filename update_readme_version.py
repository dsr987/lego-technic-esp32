#!/usr/bin/env python3
"""
Автоматически обновляет версию прошивки и ссылку на демо GUI в README.md
Версию берёт из первой записи в CHANGELOG.md
"""

import re
from pathlib import Path

def get_version_from_changelog(changelog_file="CHANGELOG.md"):
    """Извлекает версию из первой записи CHANGELOG.md"""
    path = Path(changelog_file)
    if not path.exists():
        print(f"❌ Файл {changelog_file} не найден")
        return None
    
    content = path.read_text(encoding='utf-8')
    
    # Ищем первую запись вида: ## 0.2.35 — 2026-10-09
    # или ## 0.2.35 beta — 2026-10-09
    match = re.search(r'^##\s+([\d.]+(?:\s+\w+)?)\s*[—-]', content, re.MULTILINE)
    
    if match:
        version = match.group(1).strip()
        print(f"✅ Найдена версия в CHANGELOG.md: {version}")
        return version
    else:
        print("❌ Версия не найдена в CHANGELOG.md")
        return None

def update_readme(readme_file="README.md", new_version=None):
    if not new_version:
        print("❌ Не передана версия")
        return False
    
    path = Path(readme_file)
    if not path.exists():
        print(f" Файл {readme_file} не найден")
        return False
    
    content = path.read_text(encoding='utf-8')
    
    # 1. Обновляем текст версии (ищем "**X.Y.Z beta**" или "**X.Y.Z**")
    def replace_version_text(match):
        # Сохраняем форматирование (жирный текст) и суффикс если есть
        prefix = "**"
        suffix = " beta**" if "beta" in match.group(0).lower() else "**"
        return f"{prefix}{new_version}{suffix}"
    
    # Ищем: "**0.2.33 beta**" или "**0.2.33**"
    pattern_version = r'\*\*[\d.]+(?:\s+\w+)?\*\*'
    content = re.sub(pattern_version, replace_version_text, content, count=1)
    
    # 2. Обновляем ссылку на GitHub Pages (добавляем ?v=VERSION для обхода кэша)
    pages_url = f"https://dsr987.github.io/lego-technic-esp32/index.html?v={new_version}"
    
    # Заменяем любую старую ссылку на index.html с параметром ?v=
    old_link_pattern = r'https://dsr987\.github\.io/lego-technic-esp32/index\.html\?v=[\d.]+'
    content = re.sub(old_link_pattern, pages_url, content)
    
    path.write_text(content, encoding='utf-8')
    print(f"✅ README.md обновлен: версия {new_version}, ссылка на GUI обновлена")
    return True

if __name__ == "__main__":
    version = get_version_from_changelog()
    if version:
        update_readme(new_version=version)