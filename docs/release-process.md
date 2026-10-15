# Release version automation

The release metadata process uses pull request labels and one changelog summary as its inputs. The canonical source for the firmware version and current release notes remains lego_technic_esp32/app.cpp. The existing build workflow regenerates CHANGELOG.md, README.md, and index.html, then builds the firmware and deploys GitHub Pages.

## Required PR label

Every pull request targeting main should have exactly one of these labels. The PR that first installs the automation cannot run its own pull_request_target close workflow because that workflow was not present in main when the close event was emitted. A push-triggered bootstrap job therefore creates the labels when release-versioning.yml first lands on main. That bootstrap job does not change the firmware version. For this one-time installation PR, use the line Release type: release:none in the PR body. Until the first later merged PR records labels_initialized=true in state.json, the validator retains this explicit fallback; afterwards, a real release label is required.

| Label | Result while the project is in beta |
| --- | --- |
| release:patch | Increment patch: 0.5.27 to 0.5.28 |
| release:minor | Increment minor and reset patch: 0.5.27 to 0.6.0 |
| release:stable | Promote 0.x.y to 1.0.0 when beta is explicitly over |
| release:major | After 1.0.0, increment the major version and reset minor and patch |
| release:none | No firmware-version bump for documentation or automation-only changes |

Do not use release:major to leave beta. Use release:stable for the first stable release. The validation workflow checks that exactly one recognized release label is present once setup is complete. The temporary body fallback is disabled after a post-installation PR has been processed and the state is committed.

## Changelog summary

For any release label other than release:none, add one heading named Changelog (level 2) to the PR description with one or more concise bullets. This is the only user-written release summary; do not manually copy it to CHANGELOG.md, README.md, index.html, or app.cpp.

Example:

    ## Changelog
    - Restore the three battery-warning levels in the web interface.
    - Add a safe offline preview mode for checking warning thresholds.

## What happens after merge

1. On the initial merge that adds release-versioning.yml, the push-triggered bootstrap job creates any missing labels. It deliberately does not attempt a version bump for that infrastructure-only merge. For subsequent merged PRs, the trusted pull_request_target workflow ensures the labels exist, then reads the merged PR's label and changelog section. It checks out main, never the untrusted PR head.
2. It calculates the next version, replaces the current CHANGELOG block in app.cpp, updates the version header, and records the PR number in .github/release-automation/state.json so retries cannot apply the same release twice.
3. The workflow explicitly dispatches ci-build-deploy.yml. This is necessary because pushes made with GITHUB_TOKEN do not automatically start ordinary push workflows.
4. The existing workflow generates CHANGELOG.md, synchronizes the GUI version in index.html, updates README.md, compiles firmware as needed, and deploys the site.

The version is advanced only after merge, so parallel feature branches do not independently hard-code competing versions. Physical hardware validation remains separate from automated compilation and tests.
