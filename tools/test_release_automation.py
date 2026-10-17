#!/usr/bin/env python3
"""Host-only unit tests for release versioning and PR changelog parsing."""

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import release_automation as release  # noqa: E402


class FirstRunBootstrapTests(unittest.TestCase):
    def test_release_none_first_run_initializes_state_without_bumping_version(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            app_path = root / "app.cpp"
            state_path = root / "state.json"
            output_path = root / "github-output"
            app_path.write_text(AppUpdateTests.SAMPLE_APP, encoding="utf-8")
            state_path.write_text(
                json.dumps({"schema_version": 1, "labels_initialized": False, "processed_prs": []}),
                encoding="utf-8",
            )
            before = app_path.read_text(encoding="utf-8")
            env = {
                "PR_LABELS": "",
                "PR_BODY": "Release type: release:none",
                "PR_NUMBER": "8",
                "GITHUB_OUTPUT": str(output_path),
                "GITHUB_STEP_SUMMARY": "",
            }
            with patch.object(release, "APP_CPP", app_path), \
                 patch.object(release, "STATE_PATH", state_path), \
                 patch.dict(os.environ, env, clear=False):
                result = release.apply_release()

            self.assertEqual(result, 0)
            updated_state = json.loads(state_path.read_text(encoding="utf-8"))
            self.assertTrue(updated_state["labels_initialized"])
            self.assertEqual(updated_state["processed_prs"], [8])
            self.assertEqual(app_path.read_text(encoding="utf-8"), before)
            self.assertIn("release=false", output_path.read_text(encoding="utf-8"))

            # A retry after the state commit must not require the one-time body fallback.
            with patch.object(release, "APP_CPP", app_path), \
                 patch.object(release, "STATE_PATH", state_path), \
                 patch.dict(os.environ, env, clear=False):
                retry_result = release.apply_release()
            self.assertEqual(retry_result, 0)
            self.assertEqual(app_path.read_text(encoding="utf-8"), before)

    def test_release_workflow_processes_main_push_without_pull_request_target(self):
        workflow = (
            Path(__file__).resolve().parents[1]
            / ".github" / "workflows" / "release-versioning.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("workflow_run:", workflow)
        self.assertIn("Build, Extract and Deploy", workflow)
        self.assertIn("PUSH_SHA", workflow)
        self.assertIn("prepare-push", workflow)
        self.assertNotIn("pull_request_target:", workflow)
        self.assertIn("group: release-versioning-main", workflow)
        self.assertIn("gh workflow run ci-build-deploy.yml --ref main", workflow)
        self.assertIn("steps.update.outputs.release == 'true'", workflow)
        self.assertIn("steps.update.outputs.changed == 'true'", workflow)
        self.assertIn("github.event.workflow_run.event == 'push'", workflow)
        self.assertIn("workflow_dispatch", workflow)
        self.assertIn("gh pr create", workflow)
        self.assertIn("gh pr merge", workflow)
        self.assertIn("secrets.RELEASE_AUTOMATION_TOKEN", workflow)
        self.assertIn("event pull_request", workflow)
        self.assertIn("x-access-token:%s", workflow)
        self.assertNotIn("gh workflow run pull-request-validation.yml", workflow)
        self.assertNotIn("gh workflow run release-metadata-validation.yml", workflow)
        self.assertNotIn("git push origin HEAD:main", workflow)

        metadata_validation = (
            Path(__file__).resolve().parents[1]
            / ".github" / "workflows" / "release-metadata-validation.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("workflow_dispatch:", metadata_validation)
        self.assertIn("PR_NUMBER_INPUT", metadata_validation)
        self.assertIn("pr_head_sha", metadata_validation)

        pr_validation = (
            Path(__file__).resolve().parents[1]
            / ".github" / "workflows" / "pull-request-validation.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("Verify generated GUI and release files are current", pr_validation)
        self.assertIn("Compile OTA bootstrap and verify committed prebuilt image", pr_validation)

        publish_workflow = (
            Path(__file__).resolve().parents[1]
            / ".github" / "workflows" / "publish-release.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("github.event.workflow_run.event == 'workflow_dispatch'", publish_workflow)
        self.assertIn("github.event.workflow_run.actor.login == 'github-actions[bot]'", publish_workflow)

        build_workflow = (
            Path(__file__).resolve().parents[1]
            / ".github" / "workflows" / "ci-build-deploy.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("'tools/**'", build_workflow)
        self.assertIn("'.github/workflows/**'", build_workflow)
        self.assertIn("build_bootstrap:", build_workflow)
        self.assertIn("this workflow must not push directly to protected main", build_workflow)

    def test_prepare_push_exports_metadata_for_initial_install_pr(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            event_path = root / "associated-prs.json"
            state_path = root / "state.json"
            env_path = root / "github-env"
            output_path = root / "github-output"
            state_path.write_text(
                json.dumps({"schema_version": 1, "labels_initialized": False, "processed_prs": []}),
                encoding="utf-8",
            )
            event_path.write_text(json.dumps([{
                "number": 8,
                "merged_at": "2026-10-15T10:00:00Z",
                "base": {"ref": "main"},
                "labels": [],
                "body": "Release type: release:none",
            }]), encoding="utf-8")
            env = {
                "GITHUB_ENV": str(env_path),
                "GITHUB_OUTPUT": str(output_path),
                "GITHUB_STEP_SUMMARY": "",
            }
            with patch.object(release, "STATE_PATH", state_path), patch.dict(os.environ, env, clear=False):
                result = release.prepare_push(str(event_path))
            self.assertEqual(result, 0)
            exported = env_path.read_text(encoding="utf-8")
            self.assertIn("PR_NUMBER<<", exported)
            self.assertIn("PR_NUMBER", exported)
            self.assertIn("PR_BODY", exported)
            self.assertIn("Release type: release:none", exported)
            self.assertIn("process=true", output_path.read_text(encoding="utf-8"))

    def test_prepare_push_skips_main_push_without_merged_pr(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            event_path = root / "associated-prs.json"
            output_path = root / "github-output"
            event_path.write_text("[]", encoding="utf-8")
            with patch.dict(os.environ, {"GITHUB_OUTPUT": str(output_path), "GITHUB_STEP_SUMMARY": ""}, clear=False):
                result = release.prepare_push(str(event_path))
            self.assertEqual(result, 0)
            self.assertIn("process=false", output_path.read_text(encoding="utf-8"))


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
