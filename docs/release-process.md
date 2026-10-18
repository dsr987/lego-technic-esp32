# Release version automation

The release metadata process uses merged pull request labels and a changelog summary as inputs when a change is delivered through a PR. The canonical source for the firmware version and current release notes remains `lego_technic_esp32/app.cpp`. The build workflow regenerates derived files such as `CHANGELOG.md`, `README.md`, and `index.html`, then builds firmware and deploys GitHub Pages according to its current path filters. Treat this document as a description of the post-merge flow, not a list of mandatory pre-merge checks.

## Required PR label

Every pull request targeting main should have exactly one of these labels. After the existing Build, Extract and Deploy workflow completes successfully for a main push, release automation asks GitHub which merged PR is associated with that exact commit. This handles the first PR that installs the workflow and avoids the privileged pull_request_target event. The first installation PR creates missing labels. It does not change the firmware version. For this one-time PR, use the line Release type: release:none in its body. Until the first merged PR is recorded in state.json, the validator retains this explicit fallback; afterwards, a real release label is required.

| Label | Result while the project is in beta |
| --- | --- |
| release:patch | Increment patch: 0.9.1 to 0.9.2 (example only) |
| release:minor | Increment minor and reset patch: 0.9.1 to 0.10.0 (example only) |
| release:stable | Promote 0.x.y to 1.0.0 when beta is explicitly over |
| release:major | After 1.0.0, increment the major version and reset minor and patch |
| release:none | No firmware-version bump for documentation or automation-only changes |

Do not use release:major to leave beta. Use release:stable for the first stable release. The post-merge workflow resolves the merged PR and uses its release label and changelog summary. The current `main` does not contain a separate `release-metadata-validation.yml` or `pull-request-validation.yml`; do not treat this document as proof of a pre-merge required check. Confirm the actual workflow and repository settings before relying on a gate.

## Changelog summary

For every release label, including release:none, add one heading named Changelog (level 2) to the PR description with at least one concise bullet. This is the required PR summary; do not manually copy it to CHANGELOG.md, README.md, index.html, or app.cpp. For release:none, the summary is kept in the PR record and does not produce a firmware changelog/version update.

Example:

    ## Changelog
    - Restore the three battery-warning levels in the web interface.
    - Add a safe offline preview mode for checking warning thresholds.

## What happens after merge

1. After the successful Build, Extract and Deploy run for a main push, the release workflow asks GitHub for pull requests associated with that run's exact commit. If no merged PR targeting main is associated with it, release processing is skipped. The workflow checks out trusted main and never checks out a PR head branch.
2. It ensures the release labels exist, calculates the next version, replaces the current CHANGELOG block in app.cpp, updates the version header, and records processed PR numbers in .github/release-automation/state.json so retries cannot bump the version twice.
3. The workflow coordinates with the completed build and regenerates derived files when the version metadata changes. Check `.github/workflows/release-versioning.yml` for the exact current sequence and conditions.
4. A new build may be explicitly dispatched by the automation after a version bump because pushes made with `GITHUB_TOKEN` do not automatically start ordinary push workflows. The build workflow regenerates derived files, compiles firmware when needed, and deploys the site according to its path filters.

The version is advanced only after merge, so parallel feature branches do not independently hard-code competing versions. Physical hardware validation remains separate from automated compilation and tests.
