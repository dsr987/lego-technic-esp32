# Release version automation

The release metadata process uses pull request labels and one changelog summary as its inputs. The canonical source for the firmware version and current release notes remains lego_technic_esp32/app.cpp. The existing build workflow regenerates CHANGELOG.md, README.md, and index.html, then builds the firmware and deploys GitHub Pages.

## Required PR label

Every pull request targeting main should have exactly one of these labels. After the existing Build, Extract and Deploy workflow completes successfully for a main push, release automation asks GitHub which merged PR is associated with that exact commit. This handles the first PR that installs the workflow and avoids the privileged pull_request_target event. The first installation PR creates missing labels. It does not change the firmware version. For this one-time PR, use the line Release type: release:none in its body. Until the first merged PR is recorded in state.json, the validator retains this explicit fallback; afterwards, a real release label is required.

| Label | Result while the project is in beta |
| --- | --- |
| release:patch | Increment patch: 0.5.27 to 0.5.28 |
| release:minor | Increment minor and reset patch: 0.5.27 to 0.6.0 |
| release:stable | Promote 0.x.y to 1.0.0 when beta is explicitly over |
| release:pre-release | Start the next 0.x.0 pre-release version once, then keep that version fixed while accumulating changelog entries |
| release:major | After 1.0.0, increment the major version and reset minor and patch |
| release:none | No firmware-version bump for documentation or automation-only changes |

Do not use release:major to leave beta. Use release:stable for the first stable release. Use release:pre-release to start the planned validation cycle: the first such PR increments the minor version and resets patch to zero, later pre-release PRs keep the same version, and their changelog bullets accumulate until release:stable promotes the combined changes to 1.0.0. While the cycle is active, release:patch/minor/major are rejected for firmware changes. Pre-release builds remain Actions artifacts and are not published as GitHub Releases. The validation workflow checks that exactly one recognized release label is present once setup is complete. The temporary body fallback is disabled after a post-installation PR has been processed and the state is committed.

## Changelog summary

For every release label, including release:none, add one heading named Changelog (level 2) to the PR description with at least one concise bullet. This is the required PR summary; do not manually copy it to CHANGELOG.md, README.md, index.html, or app.cpp. For release:none, the summary is kept in the PR record and does not produce a firmware changelog/version update.

Example:

    ## Changelog
    - Restore the three battery-warning levels in the web interface.
    - Add a safe offline preview mode for checking warning thresholds.

## What happens after merge

1. After the successful Build, Extract and Deploy run for a main push, the release workflow asks GitHub for pull requests associated with that run's exact commit. If no merged PR targeting main is associated with it, release processing is skipped. The workflow checks out trusted main and never checks out a PR head branch.
2. It ensures the release labels exist, calculates the next version, updates or accumulates the current CHANGELOG block in app.cpp, updates the version header, and records processed PR numbers plus active pre-release state in .github/release-automation/state.json so retries cannot bump the version twice.
3. By running only after the existing CI/deploy run completes, the versioning commit cannot race that run's generated-file commit. The CI path filters include tools and workflow files so automation-only PRs also enter this sequence.
4. For an actual version bump, the workflow explicitly dispatches ci-build-deploy.yml. This is necessary because pushes made with GITHUB_TOKEN do not automatically start ordinary push workflows. The existing workflow generates CHANGELOG.md, synchronizes the GUI version in index.html (including removing the beta badge after stable promotion), updates README.md, compiles firmware, and deploys the site. The release publisher checks the committed pre-release state and skips GitHub Release publication while the pre-release cycle is active.

The version is advanced only after merge, so parallel feature branches do not independently hard-code competing versions. Physical hardware validation remains separate from automated compilation and tests.
