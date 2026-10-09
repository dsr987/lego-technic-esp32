#!/usr/bin/env python3
"""
Извлекает CHANGELOG из комментариев в app.cpp
и добавляет новую секцию в CHANGELOG.md.

Формат комментариев в app.cpp:
// CHANGELOG 0.2.33:
// - Описание изменения 1
// - Описание изменения 2

Скрипт ищет блок, начинающийся с "// CHANGELOG X.Y.Z:" 
и заканчивающийся следующей пустой строкой или другим комментарием.
"""

import sys
import re
from pathlib import Path
from datetime import datetime

def extract_changelog_from_cpp(cpp_file):
    """Извлекает changelog из комментариев в app.cpp"""
    path = Path(cpp_file)
    if not path.exists():
        print(f"❌ Файл не найден: {cpp_file}")
        return None, None

    content = path.read_text(encoding='utf-8')

    # Ищем блок: // CHANGELOG X.Y.Z:
    # followed by lines starting with // -
    pattern = re.compile(
        r'//\s*CHANGELOG\s+([\d.]+(?:\s+\w+)?)\s*:\s*\n'  # Заголовок: CHANGELOG 0.2.33:
        r'((?://.*\n)+)',  # Тело: строки, начинающиеся с //
        re.MULTILINE
    )
    
    match = pattern.search(content)
    if not match:
        print("❌ Не найден блок CHANGELOG в app.cpp")
        return None, None

    version = match.group(1).strip()
    changelog_lines = match.group(2).strip()
    
    # Убираем префикс "// " из каждой строки
    lines = []
    for line in changelog_lines.split('\n'):
        line = line.strip()
        if line.startswith('//'):
            line = line[2:].strip()
            if line.startswith('- '):
                lines.append(line)
    
    if not lines:
        print("❌ Блок CHANGELOG пустой")
        return None, None

    return version, '\n'.join(lines)

def update_changelog_md(md_file, version, changelog_text):
    """Добавляет новую секцию в CHANGELOG.md"""
    path = Path(md_file)
    
    if not path.exists():
        print(f"⚠️  Файл {md_file} не найден, создаём новый")
        content = "# История версий\n\n"
    else:
        content = path.read_text(encoding='utf-8')
    
    # Проверяем, есть ли уже эта версия
    if f"## {version}" in content:
        print(f"⚠️  Версия {version} уже есть в CHANGELOG.md, пропускаем")
        return False
    
    # Формируем новую секцию
    today = datetime.now().strftime("%Y-%m-%d")
    new_section = f"""
## {version} — {today}
### Изменения
{changelog_text}

"""
    
    # Вставляем после заголовка "# История версий"
    # Ищем первую секцию ## и вставляем перед ней
    first_section = re.search(r'\n## ', content)
    if first_section:
        insert_pos = first_section.start()
        content = content[:insert_pos] + new_section + content[insert_pos:]
    else:
        # Если нет секций, добавляем в конец
        content += new_section
    
    path.write_text(content, encoding='utf-8')
    print(f"✅ CHANGELOG.md обновлён: добавлена версия {version}")
    return True

if __name__ == "__main__":
    cpp_file = sys.argv[1] if len(sys.argv) > 1 else "lego_technic_esp32/app.cpp"
    md_file = sys.argv[2] if len(sys.argv) > 2 else "CHANGELOG.md"
    
    version, changelog = extract_changelog_from_cpp(cpp_file)
    
    if version and changelog:
        print(f"📝 Найдена версия: {version}")
        print(f"📝 Изменения:\n{changelog}\n")
        update_changelog_md(md_file, version, changelog)
    else:
        print("⚠️  Блок CHANGELOG не найден или пустой")