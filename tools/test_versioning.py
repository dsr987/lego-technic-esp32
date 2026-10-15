#!/usr/bin/env python3
"""Regression tests for PR release metadata automation; no network or ESP32 required."""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
APPLY = REPO / "tools" / "apply_pr_changelog.py"
VALIDATE = REPO / "tools" / "validate_pr_release.py"

APP_FIXTURE = """/*
 * LEGO Technic ESP32
 * Версия разработки: 0.5.27 GUI Update 3.0
 *
 * CHANGELOG_VERSION: 0.5.27
 * CHANGELOG_START
 * - Existing entry.
 * CHANGELOG_END
 */
"""


class VersioningTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "tools").mkdir()
        (self.root / "lego_technic_esp32").mkdir()
        shutil.copy(APPLY, self.root / "tools" / APPLY.name)
        shutil.copy(VALIDATE, self.root / "tools" / VALIDATE.name)
        (self.root / "lego_technic_esp32" / "app.cpp").write_text(APP_FIXTURE, encoding="utf-8")
        self.event_path = self.root / "event.json"
        self.summary_path = self.root / "summary.md"

    def tearDown(self):
        self.temp.cleanup()

    def run_apply(self, label, number=42, notes=None):
        body = "## Changelog\n" + "".join(f"- {note}\n" for note in (notes or ["Restored battery warnings."]))
        event = {"pull_request": {"number": number, "title": "Test change", "body": body,
                 "labels": [{"name": label}]}}
        self.event_path.write_text(json.dumps(event), encoding="utf-8")
        env = dict(os.environ, PR_EVENT_PATH=str(self.event_path))
        return subprocess.run([sys.executable, str(self.root / "tools" / APPLY.name)],
                              cwd=self.root, env=env, text=True, capture_output=True)

    def test_patch_bump_and_changelog(self):
        result = self.run_apply("release:patch")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        app = (self.root / "lego_technic_esp32" / "app.cpp").read_text(encoding="utf-8")
        self.assertIn("CHANGELOG_VERSION: 0.5.28", app)
        self.assertIn("Версия разработки: 0.5.28", app)
        self.assertIn("PR #42:", app)
        self.assertIn("Restored battery warnings.", app)

    def test_minor_bump_resets_patch(self):
        result = self.run_apply("release:minor")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        app = (self.root / "lego_technic_esp32" / "app.cpp").read_text(encoding="utf-8")
        self.assertIn("CHANGELOG_VERSION: 0.6.0", app)

    def test_stable_promotion(self):
        result = self.run_apply("release:stable")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        app = (self.root / "lego_technic_esp32" / "app.cpp").read_text(encoding="utf-8")
        self.assertIn("CHANGELOG_VERSION: 1.0.0", app)

    def test_release_none_does_not_change_file(self):
        before = (self.root / "lego_technic_esp32" / "app.cpp").read_text(encoding="utf-8")
        result = self.run_apply("release:none")
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        after = (self.root / "lego_technic_esp32" / "app.cpp").read_text(encoding="utf-8")
        self.assertEqual(before, after)

    def test_idempotency(self):
        self.assertEqual(self.run_apply("release:patch").returncode, 0)
        after_first = (self.root / "lego_technic_esp32" / "app.cpp").read_text(encoding="utf-8")
        self.assertEqual(self.run_apply("release:patch").returncode, 0)
        after_second = (self.root / "lego_technic_esp32" / "app.cpp").read_text(encoding="utf-8")
        self.assertEqual(after_first, after_second)

    def test_validation_requires_changelog_and_label(self):
        env = dict(os.environ, PR_LABELS_JSON='["release:patch"]', PR_BODY="No changelog section",
                   GITHUB_STEP_SUMMARY=str(self.summary_path))
        result = subprocess.run([sys.executable, str(self.root / "tools" / VALIDATE.name)],
                                cwd=self.root, env=env, text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Changelog", result.stdout)

    def test_validation_outputs_proposed_version(self):
        env = dict(os.environ, PR_LABELS_JSON='["release:minor"]',
                   PR_BODY="## Changelog\n- Add a feature.\n",
                   GITHUB_STEP_SUMMARY=str(self.summary_path))
        result = subprocess.run([sys.executable, str(self.root / "tools" / VALIDATE.name)],
                                cwd=self.root, env=env, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertIn("0.6.0", self.summary_path.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
