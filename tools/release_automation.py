#!/usr/bin/env python3
"""Validate pull-request release metadata and apply it to the canonical app.cpp changelog."""

from __future__ import annotations

import json
import os
import re
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
APP_CPP = ROOT / "lego_technic_esp32" / "app.cpp"
STATE_PATH = ROOT / ".github" / "release-automation" / "state.json"
REPOSITORY_URL = "https://github.com/dsr987/lego-technic-esp32"

RELEASE_TYPES = {
    "release:patch": "patch",
    "release:minor": "minor",
    "release:major": "major",
    "release:stable": "stable",
    "release:none": "none",
}
VERSION_PATTERN = re.compile(r"^\s*\*\s*CHANGELOG_VERSION:\s*(\d+\.\d+\.\d+)\s*$", re.MULTILINE)
CHANGELOG_BLOCK_PATTERN = re.compile(
    r"(?ms)^(\s*\*\s*CHANGELOG_START\s*\n).*?^(\s*\*\s*CHANGELOG_END\s*)$"
)


class ReleaseMetadataError(ValueError):
    """Raised when release metadata is incomplete or inconsistent."""


def parse_release_type(raw_labels: str, body: str = "", allow_bootstrap_fallback: bool = False) -> str:
    labels = [label.strip() for label in raw_labels.split(",") if label.strip()]
    release_labels = [label for label in labels if label.startswith("release:")]
    unknown = [label for label in release_labels if label not in RELEASE_TYPES]
    if unknown:
        raise ReleaseMetadataError(
            "Unknown release label(s): " + ", ".join(unknown) + ". "
            "Use one of: " + ", ".join(RELEASE_TYPES)
        )
    if len(release_labels) == 1:
        return RELEASE_TYPES[release_labels[0]]
    if len(release_labels) > 1:
        raise ReleaseMetadataError(
            "A PR must have exactly one release label: " + ", ".join(RELEASE_TYPES)
        )

    # One-time bootstrap only: there are no release labels in the repository
    # until the first successful run provisions them. Once initialized, labels
    # are mandatory and the body fallback is no longer accepted.
    if allow_bootstrap_fallback:
        matches = re.findall(r"(?im)^Release type:[ \t]*(release:[a-z]+)[ \t]*$", body)
        if len(matches) == 1 and matches[0] in RELEASE_TYPES:
            return RELEASE_TYPES[matches[0]]
    raise ReleaseMetadataError(
        "A PR must have exactly one release label: " + ", ".join(RELEASE_TYPES)
    )


def current_version(app_text: str) -> tuple[int, int, int]:
    matches = VERSION_PATTERN.findall(app_text)
    if len(matches) != 1:
        raise ReleaseMetadataError(
            f"Expected exactly one CHANGELOG_VERSION marker, found {len(matches)}"
        )
    parts = tuple(int(part) for part in matches[0].split("."))
    if len(parts) != 3:
        raise ReleaseMetadataError("CHANGELOG_VERSION must use MAJOR.MINOR.PATCH")
    return parts  # type: ignore[return-value]


def format_version(version: tuple[int, int, int]) -> str:
    return ".".join(str(part) for part in version)


def bump_version(version: tuple[int, int, int], release_type: str) -> tuple[int, int, int] | None:
    major, minor, patch = version
    if release_type == "none":
        return None
    if release_type == "patch":
        return major, minor, patch + 1
    if release_type == "minor":
        return major, minor + 1, 0
    if release_type == "stable":
        if major != 0:
            raise ReleaseMetadataError(
                "release:stable is only for promoting a 0.x.y beta to 1.0.0"
            )
        return 1, 0, 0
    if release_type == "major":
        if major == 0:
            raise ReleaseMetadataError(
                "The project is still in 0.x beta. Use release:stable for the first stable release."
            )
        return major + 1, 0, 0
    raise ReleaseMetadataError(f"Unsupported release type: {release_type}")


def extract_changelog_bullets(body: str) -> list[str]:
    match = re.search(
        r"(?ims)^##[ \t]+changelog[ \t]*\r?\n(.*?)(?=^##[ \t]+|\Z)",
        body,
    )
    if not match:
        return []

    bullets: list[str] = []
    for raw_line in match.group(1).splitlines():
        line = raw_line.strip()
        if not line:
            continue
        bullet = re.match(r"^(?:[-*+]\s+|\d+[.)]\s+)(.+)$", line)
        if not bullet:
            continue
        text = bullet.group(1).strip()
        text = re.sub(r"\s+", " ", text)
        if text:
            bullets.append(text)
    return bullets


def validate_summary(bullets: list[str]) -> None:
    if not bullets:
        raise ReleaseMetadataError(
            "Add a non-empty '## Changelog' section with at least one bullet. "
            "Use 'release:none' for PRs that should not create a firmware release."
        )
    for bullet in bullets:
        if len(bullet) > 400:
            raise ReleaseMetadataError("Each changelog bullet must be 400 characters or fewer")


def append_step_summary(lines: list[str]) -> None:
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not summary_path:
        return
    with open(summary_path, "a", encoding="utf-8") as summary:
        summary.write("\n".join(lines) + "\n")


def write_step_output(**values: str) -> None:
    output_path = os.environ.get("GITHUB_OUTPUT")
    if not output_path:
        return
    with open(output_path, "a", encoding="utf-8") as output:
        for key, value in values.items():
            output.write(f"{key}={value}\n")


def load_state() -> dict:
    if not STATE_PATH.exists():
        return {"schema_version": 1, "labels_initialized": False, "processed_prs": [], "release_prs": []}
    try:
        state = json.loads(STATE_PATH.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as error:
        raise ReleaseMetadataError(f"Cannot read release automation state: {error}") from error
    if state.get("schema_version") != 1 or not isinstance(state.get("processed_prs"), list):
        raise ReleaseMetadataError("Release automation state has an unsupported format")
    state.setdefault("labels_initialized", False)
    state.setdefault("release_prs", [])
    if not isinstance(state["release_prs"], list):
        raise ReleaseMetadataError("Release automation state has an unsupported release_prs field")
    return state


def validate_pr() -> int:
    try:
        state = load_state()
        release_type = parse_release_type(
            os.environ.get("PR_LABELS", ""),
            os.environ.get("PR_BODY", ""),
            allow_bootstrap_fallback=not state.get("labels_initialized", False),
        )
        app_text = APP_CPP.read_text(encoding="utf-8")
        version = current_version(app_text)
        current = format_version(version)

        bullets = extract_changelog_bullets(os.environ.get("PR_BODY", ""))
        validate_summary(bullets)

        if release_type == "none":
            message = "Release type is none. This PR will not bump firmware version or changelog."
            print(f"OK: {message}")
            append_step_summary(
                [
                    "## Release metadata",
                    "",
                    f"- Current version: **{current}**",
                    "- Changelog summary:",
                    *[f"  - {bullet}" for bullet in bullets],
                    f"- {message}",
                ]
            )
            return 0

        next_version = format_version(bump_version(version, release_type))  # type: ignore[arg-type]
        print(f"OK: release:{release_type}; proposed version {current} -> {next_version}")
        for bullet in bullets:
            print(f"- {bullet}")
        append_step_summary(
            [
                "## Release metadata validation",
                "",
                f"- Release type: **{release_type}**",
                f"- Current version: **{current}**",
                f"- Proposed version: **{next_version}**",
                "- Changelog summary:",
                *[f"  - {bullet}" for bullet in bullets],
            ]
        )
        return 0
    except (OSError, ReleaseMetadataError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        append_step_summary(["## Release metadata validation", "", f"**Failed:** {error}"])
        return 1


def write_state(state: dict) -> None:
    STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
    STATE_PATH.write_text(json.dumps(state, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def update_app_text(
    app_text: str,
    new_version: str,
    bullets: list[str],
    pr_number: int,
) -> str:
    app_text, version_count = re.subn(
        r"(^\s*\*\s*CHANGELOG_VERSION:\s*)\d+\.\d+\.\d+(\s*$)",
        rf"\g<1>{new_version}\g<2>",
        app_text,
        count=1,
        flags=re.MULTILINE,
    )
    if version_count != 1:
        raise ReleaseMetadataError(f"Expected one version marker to update, found {version_count}")

    app_text, header_count = re.subn(
        r"(^\s*\*\s*Версия разработки:\s*)\d+\.\d+\.\d+",
        rf"\g<1>{new_version}",
        app_text,
        count=1,
        flags=re.MULTILINE,
    )
    if header_count != 1:
        raise ReleaseMetadataError(f"Expected one development-version header to update, found {header_count}")

    changelog_match = CHANGELOG_BLOCK_PATTERN.search(app_text)
    if not changelog_match:
        raise ReleaseMetadataError("Could not find the CHANGELOG_START/CHANGELOG_END block")

    safe_bullets = []
    for bullet in bullets:
        safe = re.sub(r"\s+", " ", bullet).strip().replace("*/", "* /")
        safe = safe.replace("\r", " ").replace("\n", " ")
        safe_bullets.append(safe)
    pr_link = f"([PR #{pr_number}]({REPOSITORY_URL}/pull/{pr_number}))"
    safe_bullets[0] = f"{safe_bullets[0]} {pr_link}"

    changelog_lines = "".join(f" * - {bullet}\n" for bullet in safe_bullets)
    replacement = changelog_match.group(1) + changelog_lines + changelog_match.group(2)
    return app_text[: changelog_match.start()] + replacement + app_text[changelog_match.end() :]


def write_env_value(name: str, value: str) -> None:
    """Write a value to GITHUB_ENV without interpreting multiline PR text."""
    env_path = os.environ.get("GITHUB_ENV")
    if not env_path:
        raise ReleaseMetadataError("GITHUB_ENV is required for push-event metadata")
    delimiter = f"EOF_{uuid.uuid4().hex}"
    with open(env_path, "a", encoding="utf-8") as env:
        env.write(f"{name}<<{delimiter}\n{value}\n{delimiter}\n")


def prepare_push(event_path: str) -> int:
    """Resolve the merged PR associated with a main push and export its metadata."""
    try:
        try:
            pulls = json.loads(Path(event_path).read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise ReleaseMetadataError(f"Cannot read associated pull requests: {error}") from error
        if not isinstance(pulls, list):
            raise ReleaseMetadataError("GitHub's associated pull-request response must be a list")

        candidates = [
            pr for pr in pulls
            if isinstance(pr, dict)
            and pr.get("number")
            and pr.get("merged_at")
            and (pr.get("base") or {}).get("ref") == "main"
            # The bot's metadata PR only updates idempotency state and must not
            # recursively generate another metadata PR after it is merged.
            and not str(((pr.get("head") or {}).get("ref") or "")).startswith(
                "automation/release-metadata-pr-"
            )
        ]
        if not candidates:
            print("No merged PR for main is associated with this push; release processing skipped.")
            write_step_output(process="false")
            return 0
        if len(candidates) != 1:
            raise ReleaseMetadataError(
                f"Expected one merged PR associated with commit, found {len(candidates)}; refusing to guess."
            )

        pr = candidates[0]
        number = pr.get("number")
        if not isinstance(number, int) or number < 1:
            raise ReleaseMetadataError("Associated pull request has no valid number")
        labels = ",".join(
            str(label.get("name", ""))
            for label in pr.get("labels", [])
            if isinstance(label, dict) and label.get("name")
        )
        body = str(pr.get("body") or "")

        state = load_state()
        processed = {int(value) for value in state["processed_prs"]}
        if number not in processed:
            release_labels = [label.strip() for label in labels.split(",") if label.strip().startswith("release:")]
            if not release_labels:
                print(
                    f"PR #{number} has no release label; skipping version metadata automation. "
                    "Use an explicit release:* label when a version or changelog update is intended."
                )
                write_step_output(process="false")
                return 0
            release_type = parse_release_type(
                labels,
                body,
                allow_bootstrap_fallback=not state.get("labels_initialized", False),
            )
            validate_summary(extract_changelog_bullets(body))
        else:
            release_type = "already-processed"

        write_env_value("PR_NUMBER", str(number))
        write_env_value("PR_LABELS", labels)
        write_env_value("PR_BODY", body)
        write_step_output(process="true", release_type=release_type)
        print(f"Resolved merged PR #{number} for commit metadata processing.")
        return 0
    except (OSError, ReleaseMetadataError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        append_step_summary(["## Release metadata preparation", "", f"**Failed:** {error}"])
        return 1


def apply_release() -> int:
    try:
        state = load_state()
        raw_pr_number = os.environ.get("PR_NUMBER", "")
        if not raw_pr_number.isdigit() or int(raw_pr_number) < 1:
            raise ReleaseMetadataError("PR_NUMBER must be a positive integer")
        pr_number = int(raw_pr_number)

        processed = {int(value) for value in state["processed_prs"]}
        release_processed = {int(value) for value in state.get("release_prs", [])}
        if pr_number in processed:
            if pr_number in release_processed:
                print(f"Release PR #{pr_number} was already processed; allow build/deploy retry.")
                write_step_output(
                    release="true",
                    changed="false",
                    version=str(state.get("last_version", "")),
                )
            else:
                print(f"Non-release PR #{pr_number} was already processed; no version bump needed.")
                write_step_output(release="false", changed="false", version="")
            return 0

        body = os.environ.get("PR_BODY", "")
        release_type = parse_release_type(
            os.environ.get("PR_LABELS", ""),
            body,
            allow_bootstrap_fallback=not state.get("labels_initialized", False),
        )

        bullets = extract_changelog_bullets(body)
        validate_summary(bullets)

        # Mark label enforcement as initialized only when a PR is successfully processed.
        state["labels_initialized"] = True

        if release_type == "none":
            state["processed_prs"] = sorted(processed | {pr_number})
            state["release_prs"] = sorted(release_processed)
            state["last_processed_pr"] = pr_number
            state["last_release_type"] = "none"
            write_state(state)
            print("Release type is none; no firmware-version or changelog update is needed.")
            write_step_output(release="false", changed="true", version="")
            return 0

        app_text = APP_CPP.read_text(encoding="utf-8")
        old_version_tuple = current_version(app_text)
        new_version_tuple = bump_version(old_version_tuple, release_type)
        if new_version_tuple is None:
            raise ReleaseMetadataError(f"Unsupported release type: {release_type}")
        old_version = format_version(old_version_tuple)
        new_version = format_version(new_version_tuple)

        new_app_text = update_app_text(app_text, new_version, bullets, pr_number)
        APP_CPP.write_text(new_app_text, encoding="utf-8", newline="\n")

        state["processed_prs"] = sorted(processed | {pr_number})
        state["release_prs"] = sorted(release_processed | {pr_number})
        state["last_processed_pr"] = pr_number
        state["last_release_type"] = release_type
        state["last_version"] = new_version
        write_state(state)

        print(f"Updated app.cpp: {old_version} -> {new_version} from PR #{pr_number}.")
        for bullet in bullets:
            print(f"- {bullet}")
        append_step_summary(
            [
                "## Firmware release metadata updated",
                "",
                f"- Source PR: [#{pr_number}]({REPOSITORY_URL}/pull/{pr_number})",
                f"- Version: **{old_version}** -> **{new_version}**",
                f"- Release type: **{release_type}**",
            ]
        )
        write_step_output(release="true", changed="true", version=new_version)
        return 0
    except (OSError, ReleaseMetadataError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        append_step_summary(["## Firmware release metadata", "", f"**Failed:** {error}"])
        return 1


def main() -> int:
    if len(sys.argv) not in {2, 3}:
        print("Usage: release_automation.py {validate-pr|apply|prepare-push [associated-prs.json]}", file=sys.stderr)
        return 2
    if sys.argv[1] == "validate-pr" and len(sys.argv) == 2:
        return validate_pr()
    if sys.argv[1] == "apply" and len(sys.argv) == 2:
        return apply_release()
    if sys.argv[1] == "prepare-push" and len(sys.argv) == 3:
        return prepare_push(sys.argv[2])
    print("Usage: release_automation.py {validate-pr|apply|prepare-push [associated-prs.json]}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
