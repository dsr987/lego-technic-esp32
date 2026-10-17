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
import extract_changelog  # noqa: E402
import update_gui_version  # noqa: E402


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
                "PR_BODY": "Release type: release:none\n\n## Changelog\n- Initialize release metadata tracking.",
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
        self.assertIn("gh workflow run ci-build-deploy.yml --ref main --field build_bootstrap=false", workflow)
        self.assertIn('ensure_label "release:pre-release"', workflow)
        self.assertIn("gh pr create", workflow)
        self.assertIn("gh pr merge", workflow)
        self.assertIn("secrets.RELEASE_AUTOMATION_TOKEN", workflow)
        self.assertIn("event pull_request", workflow)
        self.assertIn("--limit 20 --json databaseId,status,conclusion", workflow)
        self.assertIn('[0].databaseId // empty', workflow)
        self.assertIn('select(.status == "completed" and .conclusion == "success")', workflow)
        self.assertIn("x-access-token:%s", workflow)
        self.assertIn("git config --local --unset-all http.https://github.com/.extraheader", workflow)
        self.assertNotIn("gh workflow run pull-request-validation.yml", workflow)
        self.assertNotIn("gh workflow run release-metadata-validation.yml", workflow)
        self.assertNotIn("git push origin HEAD:main", workflow)

        metadata_validation = (
            Path(__file__).resolve().parents[1]
            / ".github" / "workflows" / "release-metadata-validation.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("workflow_dispatch:", metadata_validation)
        self.assertIn("PR_NUMBER_INPUT", metadata_validation)
        self.assertIn("PR_HEAD_SHA_EVENT", metadata_validation)
        self.assertIn('gh api "repos/${GITHUB_REPOSITORY}/pulls/${pr_number}"', metadata_validation)
        self.assertIn("current PR metadata rather than trusting the event snapshot", metadata_validation)
        self.assertNotIn("cancel-in-progress: true", metadata_validation)

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
        self.assertIn("https://github.com/dsr987/lego-technic-esp32/releases/latest/download/lego_technic_esp32.ino.bin", build_workflow)
        self.assertNotIn("https://dsr987.github.io/lego-technic-esp32/downloads/build_firmware_for_esp32.bin", build_workflow)

        publish_workflow = (
            Path(__file__).resolve().parents[1]
            / ".github" / "workflows" / "publish-release.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("pre_release_active", publish_workflow)
        self.assertIn("steps.release_mode.outputs.publish == 'true'", publish_workflow)
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
                "body": "Release type: release:none\n\n## Changelog\n- Initialize release metadata tracking.",
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

    def test_validate_pr_requires_changelog_for_release_none(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            app_path = root / "app.cpp"
            state_path = root / "state.json"
            app_path.write_text(AppUpdateTests.SAMPLE_APP, encoding="utf-8")
            state_path.write_text(
                json.dumps({
                    "schema_version": 1,
                    "labels_initialized": True,
                    "processed_prs": [],
                    "release_prs": [],
                }),
                encoding="utf-8",
            )
            env = {
                "PR_LABELS": "release:none",
                "PR_BODY": "No changelog section.",
                "GITHUB_STEP_SUMMARY": "",
            }
            with patch.object(release, "APP_CPP", app_path):
                with patch.object(release, "STATE_PATH", state_path):
                    with patch.dict(os.environ, env, clear=False):
                        result = release.validate_pr()
            self.assertEqual(result, 1)

    def test_apply_release_rejects_release_none_without_changelog(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            app_path = root / "app.cpp"
            state_path = root / "state.json"
            output_path = root / "github-output"
            app_path.write_text(AppUpdateTests.SAMPLE_APP, encoding="utf-8")
            state_path.write_text(
                json.dumps({
                    "schema_version": 1,
                    "labels_initialized": False,
                    "processed_prs": [],
                    "release_prs": [],
                }),
                encoding="utf-8",
            )
            before = app_path.read_text(encoding="utf-8")
            env = {
                "PR_LABELS": "release:none",
                "PR_BODY": "Release type: release:none",
                "PR_NUMBER": "29",
                "GITHUB_OUTPUT": str(output_path),
                "GITHUB_STEP_SUMMARY": "",
            }
            with patch.object(release, "APP_CPP", app_path):
                with patch.object(release, "STATE_PATH", state_path):
                    with patch.dict(os.environ, env, clear=False):
                        result = release.apply_release()
            self.assertEqual(result, 1)
            state = json.loads(state_path.read_text(encoding="utf-8"))
            self.assertFalse(state["labels_initialized"])
            self.assertEqual(state["processed_prs"], [])
            self.assertEqual(app_path.read_text(encoding="utf-8"), before)

    def test_prepare_push_rejects_release_none_without_changelog(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            event_path = root / "associated-prs.json"
            state_path = root / "state.json"
            output_path = root / "github-output"
            event_path.write_text(json.dumps([{
                "number": 29,
                "merged_at": "2026-10-17T10:00:00Z",
                "base": {"ref": "main"},
                "head": {"ref": "fix/example"},
                "labels": [{"name": "release:none"}],
                "body": "No changelog section.",
            }]), encoding="utf-8")
            state_path.write_text(
                json.dumps({
                    "schema_version": 1,
                    "labels_initialized": True,
                    "processed_prs": [],
                    "release_prs": [],
                }),
                encoding="utf-8",
            )
            env = {
                "GITHUB_ENV": str(root / "github-env"),
                "GITHUB_OUTPUT": str(output_path),
                "GITHUB_STEP_SUMMARY": "",
            }
            with patch.object(release, "STATE_PATH", state_path):
                with patch.dict(os.environ, env, clear=False):
                    result = release.prepare_push(str(event_path))
            self.assertEqual(result, 1)
            self.assertFalse(output_path.exists())

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

    def test_prepare_push_skips_automation_metadata_pr(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            event_path = root / "associated-prs.json"
            output_path = root / "github-output"
            event_path.write_text(json.dumps([{
                "number": 21,
                "merged_at": "2026-10-17T05:12:00Z",
                "base": {"ref": "main"},
                "head": {"ref": "automation/release-metadata-pr-20"},
                "labels": [{"name": "release:none"}],
                "body": "Release type: release:none",
            }]), encoding="utf-8")
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
            "release:pre-release": "pre-release",
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

    def test_pre_release_starts_next_minor_then_keeps_same_version(self):
        inactive = {"pre_release_active": False, "pre_release_version": None}
        next_version, append_existing, active = release.resolve_release_transition(
            (0, 9, 1), "pre-release", inactive
        )
        self.assertEqual(release.format_version(next_version), "0.10.0")
        self.assertFalse(append_existing)
        self.assertTrue(active)

        active_state = {"pre_release_active": True, "pre_release_version": "0.10.0"}
        same_version, append_existing, active = release.resolve_release_transition(
            (0, 10, 0), "pre-release", active_state
        )
        self.assertEqual(release.format_version(same_version), "0.10.0")
        self.assertTrue(append_existing)
        self.assertTrue(active)

    def test_stable_promotion_ends_pre_release_and_keeps_accumulated_changelog(self):
        active_state = {"pre_release_active": True, "pre_release_version": "0.10.0"}
        next_version, append_existing, active = release.resolve_release_transition(
            (0, 10, 0), "stable", active_state
        )
        self.assertEqual(release.format_version(next_version), "1.0.0")
        self.assertTrue(append_existing)
        self.assertFalse(active)

    def test_active_pre_release_rejects_regular_version_bumps(self):
        active_state = {"pre_release_active": True, "pre_release_version": "0.10.0"}
        with self.assertRaisesRegex(release.ReleaseMetadataError, "A pre-release is active"):
            release.resolve_release_transition((0, 10, 0), "patch", active_state)

    def test_pre_release_cannot_start_after_stable_major(self):
        with self.assertRaisesRegex(release.ReleaseMetadataError, "only available before"):
            release.resolve_release_transition(
                (1, 0, 0), "pre-release",
                {"pre_release_active": False, "pre_release_version": None},
            )


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

    def test_appends_entries_during_pre_release_without_dropping_previous_notes(self):
        first = release.update_app_text(
            self.SAMPLE_APP, "0.6.0", ["First pre-release fix."], 100
        )
        second = release.update_app_text(
            first, "0.6.0", ["Second pre-release fix."], 101, append_existing=True
        )
        self.assertIn("CHANGELOG_VERSION: 0.6.0", second)
        self.assertIn("First pre-release fix.", second)
        self.assertIn("Second pre-release fix.", second)
        self.assertEqual(len(release.extract_app_changelog_bullets(second)), 3)

    def test_requires_changelog_markers_and_version_header(self):
        with self.assertRaises(release.ReleaseMetadataError):
            release.update_app_text("CHANGELOG_VERSION: 0.5.27", "0.5.28", ["Note"], 7)


class PreReleaseLifecycleTests(unittest.TestCase):
    def test_start_continue_and_promote_pre_release(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            app_path = root / "app.cpp"
            state_path = root / "state.json"
            output_path = root / "github-output"
            app_path.write_text(AppUpdateTests.SAMPLE_APP, encoding="utf-8")
            state_path.write_text(
                json.dumps({
                    "schema_version": 1,
                    "labels_initialized": True,
                    "processed_prs": [],
                    "release_prs": [],
                }),
                encoding="utf-8",
            )

            changes = [
                (100, "release:pre-release", "First pre-release fix.", "0.6.0", True),
                (101, "release:pre-release", "Second pre-release fix.", "0.6.0", True),
                (102, "release:stable", "Publish stable firmware.", "1.0.0", False),
            ]
            with patch.object(release, "APP_CPP", app_path), patch.object(release, "STATE_PATH", state_path):
                for number, label, bullet, expected_version, expected_active in changes:
                    env = {
                        "PR_LABELS": label,
                        "PR_BODY": f"Release type: {label}\\n\\n## Changelog\\n- {bullet}",
                        "PR_NUMBER": str(number),
                        "GITHUB_OUTPUT": str(output_path),
                        "GITHUB_STEP_SUMMARY": "",
                    }
                    with patch.dict(os.environ, env, clear=False):
                        self.assertEqual(release.apply_release(), 0)

                    app_text = app_path.read_text(encoding="utf-8")
                    self.assertIn(f"CHANGELOG_VERSION: {expected_version}", app_text)
                    state = json.loads(state_path.read_text(encoding="utf-8"))
                    self.assertEqual(state["pre_release_active"], expected_active)
                    self.assertEqual(state["pre_release_version"], expected_version if expected_active else None)

            final_app = app_path.read_text(encoding="utf-8")
            for entry in ("First pre-release fix.", "Second pre-release fix.", "Publish stable firmware."):
                self.assertIn(entry, final_app)
            final_state = json.loads(state_path.read_text(encoding="utf-8"))
            self.assertEqual(final_state["processed_prs"], [100, 101, 102])
            self.assertEqual(final_state["release_prs"], [100, 101, 102])
            self.assertEqual(final_state["last_version"], "1.0.0")


class GeneratedFileVersionTests(unittest.TestCase):
    def test_changelog_refreshes_existing_latest_pre_release_section(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "CHANGELOG.md"
            extract_changelog.update_changelog(path, "0.10.0", "- First fix.")
            before = path.read_text(encoding="utf-8")
            extract_changelog.update_changelog(path, "0.10.0", "- First fix.\\n- Second fix.")
            after = path.read_text(encoding="utf-8")
            self.assertEqual(after.count("## 0.10.0"), 1)
            self.assertIn("First fix.", after)
            self.assertIn("Second fix.", after)
            self.assertNotEqual(after, before)

    def test_stale_build_does_not_rewrite_older_changelog_entry(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "CHANGELOG.md"
            path.write_text(
                "# История версий\\n\\n## 0.10.0 — 2026-10-17\\n### Изменено\\n- Current.\\n\\n"
                "## 0.9.1 — 2026-10-16\\n### Изменено\\n- Older.\\n\\n",
                encoding="utf-8",
            )
            extract_changelog.update_changelog(path, "0.9.1", "- Stale output.")
            content = path.read_text(encoding="utf-8")
            self.assertIn("- Older.", content)
            self.assertNotIn("- Stale output.", content)

    def test_gui_version_suffix_tracks_beta_or_stable_major(self):
        initial = '<div class="brand-ver">0.9.1 · beta</div>'
        pre_release, pre_count = update_gui_version.replace_brand_version(initial, "0.10.0")
        self.assertEqual(pre_count, 1)
        self.assertEqual(pre_release, '<div class="brand-ver">0.10.0 · beta</div>')
        stable, stable_count = update_gui_version.replace_brand_version(pre_release, "1.0.0")
        self.assertEqual(stable_count, 1)
        self.assertEqual(stable, '<div class="brand-ver">1.0.0</div>')


if __name__ == "__main__":
    unittest.main(verbosity=2)
