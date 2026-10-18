#!/usr/bin/env python3
"""Host-only tests for generated CHANGELOG updates."""

import sys
import tempfile
import unittest
from pathlib import Path
from datetime import date

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_changelog as changelog  # noqa: E402


class UpdateChangelogTests(unittest.TestCase):
    def write_and_update(self, initial, entries, version="0.9.1"):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "CHANGELOG.md"
            path.write_text(initial, encoding="utf-8")
            changelog.update_changelog(str(path), version, entries)
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

    def test_compact_summary_is_not_duplicated_when_pr_is_already_detailed(self):
        initial = """# История версий

## 0.9.1 — 2026-10-16
### Изменено
- Added detailed battery calibration instructions. ([PR #11](https://github.com/dsr987/lego-technic-esp32/pull/11))
- Added automated tests and safe calibration guidance. ([PR #11](https://github.com/dsr987/lego-technic-esp32/pull/11))
"""
        result = self.write_and_update(
            initial,
            "- Added battery calibration and a calculation helper. ([PR #11](https://github.com/dsr987/lego-technic-esp32/pull/11))",
        )
        self.assertNotIn("- Added battery calibration and a calculation helper.", result)
        self.assertIn("- Added detailed battery calibration instructions.", result)

    def test_new_pr_entry_is_appended_even_when_version_already_exists(self):
        initial = """# История версий

## 0.9.1 — 2026-10-16
### Изменено
- Existing entry. ([PR #11](https://github.com/dsr987/lego-technic-esp32/pull/11))
"""
        result = self.write_and_update(
            initial,
            "- New entry. ([PR #12](https://github.com/dsr987/lego-technic-esp32/pull/12))",
        )
        self.assertIn("- Existing entry.", result)
        self.assertIn("- New entry.", result)

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
