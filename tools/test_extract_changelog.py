#!/usr/bin/env python3
"""Host-only tests for generated CHANGELOG updates."""

import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from datetime import date

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_changelog as changelog  # noqa: E402


class UpdateChangelogTests(unittest.TestCase):
    def write_and_update(self, initial, entries):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "CHANGELOG.md"
            path.write_text(initial, encoding="utf-8")
            changelog.update_changelog(str(path), "0.9.1", entries)
            return path.read_text(encoding="utf-8")

    def test_appends_new_entries_to_existing_version_and_preserves_old_ones(self):
        initial = """# История версий

## 0.9.1 — 2026-10-16
### Изменено
- Existing entry.
- Shared entry.

## 0.9.0 — 2026-10-15
### Изменено
- Older version entry.
"""
        result = self.write_and_update(
            initial,
            "- Shared entry.\n- New entry.",
        )
        self.assertIn("- Existing entry.", result)
        self.assertIn("- Shared entry.", result)
        self.assertIn("- New entry.", result)
        self.assertIn("- Older version entry.", result)
        self.assertEqual(result.count("## 0.9.1"), 1)
        self.assertEqual(result.index("- Shared entry."), result.index("- New entry.") - len("- Shared entry.\n"))

    def test_existing_version_without_changed_subsection_gets_one(self):
        initial = """# История версий

## 0.9.1 — 2026-10-16
### Исправлено
- Keep this section.

## 0.9.0 — 2026-10-15
- Older version entry.
"""
        result = self.write_and_update(initial, "- New entry.")
        self.assertIn("### Изменено\n- New entry.", result)
        self.assertIn("### Исправлено\n- Keep this section.", result)
        self.assertIn("- Older version entry.", result)

    def test_update_is_idempotent_and_does_not_duplicate_entries(self):
        initial = """# История версий

## 0.9.1 — 2026-10-16
### Изменено
- Existing entry.
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "CHANGELOG.md"
            path.write_text(initial, encoding="utf-8")
            changelog.update_changelog(str(path), "0.9.1", "- Existing entry.\n- New entry.")
            first = path.read_text(encoding="utf-8")
            changelog.update_changelog(str(path), "0.9.1", "- Existing entry.\n- New entry.")
            second = path.read_text(encoding="utf-8")
        self.assertEqual(first, second)
        self.assertEqual(second.count("- New entry."), 1)
        self.assertIn("## 0.9.1 — 2026-10-16", second)

    def test_new_version_is_inserted_at_top_and_existing_history_is_preserved(self):
        initial = "# История версий\n\n## 0.9.0 — 2026-10-15\n- Older entry.\n"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "CHANGELOG.md"
            path.write_text(initial, encoding="utf-8")
            changelog.update_changelog(str(path), "0.9.2", "- Fresh entry.")
            result = path.read_text(encoding="utf-8")
        self.assertTrue(result.startswith("# История версий\n\n## 0.9.2 — "))
        self.assertIn("- Older entry.", result)
        self.assertIn(date.today().isoformat(), result)


if __name__ == "__main__":
    unittest.main(verbosity=2)
