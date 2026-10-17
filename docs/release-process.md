# Release version automation

The release metadata process uses pull request labels and one changelog summary as its inputs. The canonical source for the firmware version and current release notes remains lego_technic_esp32/app.cpp. The existing build workflow regenerates CHANGELOG.md, README.md, and index.html, then builds the firmware and deploys GitHub Pages.

## Required PR label

Every pull request targeting main should have exactly one of these labels. After the existing Build, Extract and Deploy workflow completes successfully for a main push, release automation asks GitHub which merged PR is associated with that exact commit. This handles the first PR that installs the workflow and avoids the privileged pull_request_target event. The first installation PR creates missing labels. It does not change the firmware version. For this one-time PR, use the line Release type: release:none in its body. Until the first merged PR is recorded in state.json, the validator retains this explicit fallback; afterwards, a real release label is required.

| Label | Result while the project is in beta |
| --- | --- |
| release:patch | Increment patch: 0.5.27 to 0.5.28 |
| release:minor | Increment minor and reset patch: 0.5.27 to 0.6.0 |
| release:stable | Promote 0.x.y to 1.0.0 when beta is explicitly over |
| release:pre-release | Start the next 0.x minor pre-release or continue the active pre-release without changing its numeric version |
| release:major | After 1.0.0, increment the major version and reset minor and patch |
| release:none | No firmware-version bump for documentation or automation-only changes |

Do not use release:major to leave beta. Use release:stable for the first stable release. The validation workflow checks that exactly one recognized release label is present once setup is complete. The temporary body fallback is disabled after a post-installation PR has been processed and the state is committed.

## Fixed-version pre-release

The first PR labelled `release:pre-release` advances the current `0.x.y` version to the next minor version (for example, `0.9.1` to `0.10.0`) and activates the pre-release state in `.github/release-automation/state.json`. Later firmware PRs with the same label keep that numeric version and append their changelog bullets to the current release block. While the state is active, `release:patch`, `release:minor`, and `release:major` are rejected; use `release:none` for documentation-only changes.

Pre-release builds are compiled and retained as GitHub Actions artifacts. They are not published as GitHub Releases and must not replace the latest stable download. The changelog generator refreshes the newest section for a repeated version so the full pre-release history stays visible.

When ready for the first stable release, a PR labelled `release:stable` promotes the firmware to `1.0.0`, carries accumulated pre-release notes into the stable changelog, and clears the pre-release state. Stable publication then returns to the normal release workflow.

## Changelog summary

For every release label, including release:none, add one heading named Changelog (level 2) to the PR description with at least one concise bullet. This is the required PR summary; do not manually copy it to CHANGELOG.md, README.md, index.html, or app.cpp. For release:none, the summary is kept in the PR record and does not produce a firmware changelog/version update.

Example:

    ## Changelog
    - Restore the three battery-warning levels in the web interface.
    - Add a safe offline preview mode for checking warning thresholds.

## What happens after merge

1. After the successful Build, Extract and Deploy run for a main push, the release workflow asks GitHub for pull requests associated with that run's exact commit. If no merged PR targeting main is associated with it, release processing is skipped. The workflow checks out trusted main and never checks out a PR head branch.
2. It ensures the release labels exist, validates the version transition, updates or appends the current CHANGELOG block in app.cpp, updates the version header, and records processed PR numbers in .github/release-automation/state.json so retries cannot bump the version twice.
3. By running only after the existing CI/deploy run completes, the versioning commit cannot race that run's generated-file commit. The CI path filters include tools and workflow files so automation-only PRs also enter this sequence.
4. For a firmware change, the workflow explicitly dispatches ci-build-deploy.yml. This is necessary because pushes made with GITHUB_TOKEN do not automatically start ordinary push workflows. The existing workflow generates CHANGELOG.md, synchronizes the GUI version in index.html, updates README.md, compiles firmware, and deploys the site. During pre-release mode, the resulting binary stays in the Actions artifact; the release publisher checks the tracked state and skips creating a GitHub Release.

The version is advanced only after merge, so parallel feature branches do not independently hard-code competing versions. Physical hardware validation remains separate from automated compilation and tests.
