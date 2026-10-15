#!/usr/bin/env python3
"""Host-only unit tests for release versioning and PR changelog parsing."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import release_automation as release  # noqa: E402


class ReleaseTypeTests(unittest.TestCase):
    def test_requires_exactly_one_release_label(self):
        for labels in ("", "bug", "release:patch,release:minor"):
            with self.subTest(labels=labels), self.assertRaises(release.ReleaseMetadataError):
                release.parse_release_type(labels)

    def test_rejects_unknown_release_label(self):
        with self.assertRaisesRegex(release.ReleaseMetadataError, "Unknown release label"):
            release.parse_release_type("release:turbo")

    def test_bootstrap_body_fallback_works_only_when_explicitly_enabled(self):
        body = "Some setup context\n\nRelease type: release:none\n"
        self.assertEqual(release.parse_release_type("", body, allow_bootstrap_fallback=True), "none")
        with self.assertRaises(release.ReleaseMetadataError):
            release.parse_release_type("", body, allow_bootstrap_fallback=False)

    def test_label_is_authoritative_over_bootstrap_body_fallback(self):
        body = "Release type: release:none"
        self.assertEqual(release.parse_release_type("release:minor", body, allow_bootstrap_fallback=True), "minor")

    def test_supported_labels(self):
        cases = {
            "release:patch": "patch",
            "release:minor": "minor",
            "release:major": "major",
            "release:stable": "stable",
            "release:none": "none",
        }
        for label, expected in cases.items():
            with self.subTest(label=label):
                self.assertEqual(release.parse_release_type(label), expected)


class VersionBumpTests(unittest.TestCase):
    def test_beta_patch(self):
        self.assertEqual(release.format_version(release.bump_version((0, 5, 27), "patch")), "0.5.28")

    def test_beta_minor_resets_patch(self):
        self.assertEqual(release.format_version(release.bump_version((0, 5, 27), "minor")), "0.6.0")

    def test_stable_release_promotes_beta_to_one(self):
        self.assertEqual(release.format_version(release.bump_version((0, 6, 8), "stable")), "1.0.0")

    def test_major_requires_stable_project(self):
        with self.assertRaisesRegex(release.ReleaseMetadataError, "still in 0.x beta"):
            release.bump_version((0, 6, 0), "major")

    def test_stable_label_is_only_for_zero_major(self):
        with self.assertRaisesRegex(release.ReleaseMetadataError, "only for promoting"):
            release.bump_version((1, 2, 3), "stable")

    def test_stable_major_bump(self):
        self.assertEqual(release.format_version(release.bump_version((1, 2, 3), "major")), "2.0.0")

    def test_release_none_does_not_bump(self):
        self.assertIsNone(release.bump_version((0, 5, 27), "none"))


class ChangelogParsingTests(unittest.TestCase):
    def test_extracts_only_changelog_section_bullets(self):
        body = """Intro text

## Changelog
- Restore battery warnings at 10%, 5%, and 1%.
- Add isolated offline preview mode.

## Tests
- Not part of the release notes.
"""
        self.assertEqual(
            release.extract_changelog_bullets(body),
            ["Restore battery warnings at 10%, 5%, and 1%.", "Add isolated offline preview mode."],
        )

    def test_accepts_case_insensitive_heading_and_numbered_list(self):
        self.assertEqual(release.extract_changelog_bullets("## CHANGELOG\n1. Fixed an issue.\n"), ["Fixed an issue."])

    def test_missing_or_empty_section_is_invalid(self):
        self.assertEqual(release.extract_changelog_bullets("No changelog section."), [])
        with self.assertRaises(release.ReleaseMetadataError):
            release.validate_summary([])

    def test_bullet_limit(self):
        with self.assertRaisesRegex(release.ReleaseMetadataError, "400 characters"):
            release.validate_summary(["x" * 401])


class AppUpdateTests(unittest.TestCase):
    SAMPLE_APP = """/*
 * Версия разработки: 0.5.27 GUI Update 3.0
 * CHANGELOG_VERSION: 0.5.27
 * CHANGELOG_START
 * - Old release entry.
 * CHANGELOG_END
 */
"""

    def test_updates_both_versions_and_replaces_current_changelog(self):
        result = release.update_app_text(
            self.SAMPLE_APP,
            "0.6.0",
            ["Restore battery warnings.", "Add safe preview mode."],
            42,
        )
        self.assertIn("Версия разработки: 0.6.0 GUI Update 3.0", result)
        self.assertIn("CHANGELOG_VERSION: 0.6.0", result)
        self.assertIn("- Restore battery warnings. ([PR #42](https://github.com/dsr987/lego-technic-esp32/pull/42))", result)
        self.assertIn("- Add safe preview mode.", result)
        self.assertNotIn("Old release entry", result)

    def test_cpp_comment_terminator_is_sanitized(self):
        result = release.update_app_text(self.SAMPLE_APP, "0.5.28", ["Handles */ safely"], 7)
        self.assertIn("Handles * / safely", result)
        self.assertNotIn("Handles */ safely", result)

    def test_requires_changelog_markers_and_version_header(self):
        with self.assertRaises(release.ReleaseMetadataError):
            release.update_app_text("CHANGELOG_VERSION: 0.5.27", "0.5.28", ["Note"], 7)


if __name__ == "__main__":
    unittest.main(verbosity=2)
