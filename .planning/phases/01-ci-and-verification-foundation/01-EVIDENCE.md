# Phase 1 Evidence Record

Evidence label for every entry below: **observed locally** unless a later section says otherwise. Dates are UTC.
No absolute local paths, scratch locations, or non-noreply email addresses appear in this file.

## CMake 3.25 floor

- Date: 2026-11-10. Plan 01-09 Task 1 (VERIF-01, orchestrator resolution 4).
- Toolchain: `cmake version 3.25.3` from the official Kitware release tarball `cmake-3.25.3-macos-universal.tar.gz`
  (https://github.com/Kitware/CMake/releases/download/v3.25.3/). No PyPI cmake package was used.
- Tarball SHA-256: `771548ed2abae17f3fd28dcfa572ba3fe9f970652a72c36c2e1aafdee93a234e`, verified with `shasum -a 256`
  against its line in Kitware's `cmake-3.25.3-SHA-256.txt` from the same release URL (lines identical).
  Extracted into a fresh temporary directory outside the repository; used locally only, never in CI.
- Command: `<cmake 3.25.3> --workflow --preset ci` from the repository root after `rm -rf build/ci`, with the `CMAKE`
  environment variable set to the same binary for the scan scripts.
- First run (before the fix): exit 8, 11 of 93 tests failed. Cause: script-mode (`-P`) scripts start with every
  policy OLD, so `IN_LIST` was a plain argument under CMP0057 OLD on 3.25 and `if(NOT x IN_LIST y)` was rejected. The
  presets file itself was accepted. Fixed in commit `9dfeda8` (each `-P` entry script now calls
  `cmake_minimum_required(VERSION 3.25)`); the default cmake (4.4.3) was unaffected.
- Final run on the fixed source: exit 0, `100% tests passed, 0 tests failed out of 93`, post step 1 of 1 passed, and the
  summary line:
  `inventory: expected 93, ran 91, skipped 2 (allowed 2)`
- output.verbosity accepted on CMake 3.25.3 (summary line present in the floor log)
- Other tool versions printed: CMake 4.4.3 (default on the host), Apple clang 21.0.0 (clang-2100.1.1.101) on arm64
  macOS; in the Linux container (below) CMake 4.2.3, GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1), Ubuntu clang 21.1.8.
  `clang-21` itself is not on the macOS host; the asan lane runs only in the Linux container and in CI.
- Same default-cmake runs on the fixed source: `cmake --workflow --preset ci` exit 0 (93 expected, 91 ran, 2 skipped);
  `cmake --workflow --preset hygiene` exit 0 (`inventory: expected 41, ran 39, skipped 2 (allowed 2)`).

## Linux pre-checks

- Date: 2026-11-10, run on the committed source at `9dfeda8` immediately before the first push (Pitfall 11).
- Docker daemon architecture (`docker info --format '{{.Architecture}}'`): `aarch64`; Docker server 29.5.2.
- Container architecture printed by `scripts/linux-lane.sh` (`uname -m`): `aarch64` (native, not emulated; the x86_64
  proof is the CI runner).
- Image: `ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c` (the multi-architecture
  index digest pinned in `scripts/linux-lane.sh`).
- `sh scripts/linux-lane.sh ci gcc`: exit 0. CMake 4.2.3, GCC 15.2.0. `100% tests passed, 0 tests failed out of 93`.
  Final summary line: `inventory: expected 93, ran 91, skipped 2 (allowed 2)`
- `sh scripts/linux-lane.sh asan` (clang-21): exit 0. Ubuntu clang 21.1.8. `100% tests passed, 0 tests failed out of 9`.
  Final summary line: `inventory: expected 9, ran 9, skipped 0 (allowed 0)`

## PR B (source)

Observed via `gh pr view` and `gh run view` (GitHub API), 2026-11-10 UTC; run data copied from plan 01-09's SUMMARY.

- PR: https://github.com/szTheory/gamebloke/pull/1, title `ci: add verification foundation and CI required check`,
  state `MERGED` at 2026-11-10T01:44:44Z (squash, no admin bypass).
- Merge commit on main: `d1b59327fc348138ff725781b8c29f04c5f6f350` (tree identical to branch tip `6cdff80`).
- Branch commits: `9dfeda8` (floor policy fix), `2df5446` (guide and evidence), `6cdff80` (measured timeouts).

| Run ID | Event | Head | Conclusion |
|--------|-------|------|------------|
| 42737527955 attempt 1 | pull_request | `2df5446` | success |
| 42737527955 attempt 2 (rerun) | pull_request | `2df5446` | success |
| 42737836505 | pull_request | `6cdff80` (merged tree) | success |

Per-job conclusions (every PR run above): changes `success`, hygiene `success`, title `success`,
build (ubuntu-26.04, gcc, GNU) `success`, build (macos-26, cc, AppleClang) `success`, asan `success`,
CI required `success`. A source change ran every lane.

Per-job durations in seconds (`startedAt` to `completedAt`):

| Job | Run 1 | Run 2 (rerun) | Run 42737836505 | Main push |
|-----|-------|---------------|-----------------|-----------|
| changes | 5 | 4 | 5 | 4 |
| hygiene | 8 | 8 | 9 | 9 |
| title | 4 | 6 | 4 | skipped |
| build ubuntu-26.04 GNU | 20 | 12 | 10 | 14 |
| build macos-26 AppleClang | 36 | 27 | 19 | 28 |
| asan | 7 | 10 | 9 | 10 |
| CI required | 6 | 4 | 6 | 5 |

Derived timeouts (D-15): slowest per job from runs 42737527955 attempts 1 and 2 was changes 5 s, hygiene 8 s, title 6 s,
build 36 s, asan 10 s, CI required 6 s. ceil(2 x slowest) is at most 2 minutes, so the plan's floor of 3 applies and
`timeout-minutes: 3` is set on all six jobs. The samples are small (four runs for macOS); re-measure when the build grows.

## main push run

Observed via `gh run view`.

- Run 42737946832, event `push`, head `d1b5932` on main: conclusion `success`.
- Jobs: changes, hygiene, build (ubuntu-26.04, gcc, GNU), build (macos-26, cc, AppleClang), asan, CI required all
  `success`; title `skipped` by design on push.
- The `CI required` check run on main is `success` (integration 15368), which satisfies the apply script's guard.

## PR A (.planning-only)

Observed via `gh pr view` and `gh run view`, 2026-11-10 UTC.

- PR: https://github.com/szTheory/gamebloke/pull/2, title `docs(01): record CI routing evidence and switch to phase branches`.
  Every changed path is under `.planning/` (7 files). Opened before the ruleset existed; merged after it, through it.
- Run 42738583708 (pull_request, head `6a166ea`): conclusion `success`. The `changes` job printed `heavy=false`
  (`CHANGED_FILES: 7`), and the `CI required` job printed `ci-required: ok (5 jobs, heavy=false, event=pull_request)`.

| Job | Conclusion |
|-----|------------|
| changes | success |
| hygiene | success |
| title | success |
| build (ubuntu-26.04 and macos-26 matrix) | skipped |
| asan | skipped |
| CI required | success |

  The two build legs are one matrix job in the run's job list, reported `skipped` as a whole.
- Merge: `gh pr merge 2 --squash --delete-branch`, no admin flag, no re-run needed; merged 2026-11-10T01:51:21Z as
  `599559c5cf48d575795dec679b37974ee77acce2` on main. The merge was accepted through the active ruleset; the
  command's local fast-forward of the checked-out branch then failed because local main still carried the plan 09
  commit under a different hash (a local-only step; handled by the tree-equality check and reset below).
- Main push run 42738703843 (head `599559c`): conclusion `success`. The push event runs every lane: hygiene, changes,
  asan, build ubuntu-26.04 GNU, build macos-26 AppleClang and CI required are `success` (title `skipped` by design).
  Durations (s): hygiene 9, changes 4, asan 11, build ubuntu 23, build macos 20, CI required 6.
  The `CI required` check run on main is `success`.
- Local follow-up: `git diff --quiet origin/main 6a166ea` (the unmerged PR commit) reported equal trees; local main
  was reset to origin/main; the phase close-out branch `gsd/phase-01-ci-and-verification-foundation` was created
  from origin/main and is not pushed.

## Ruleset

Observed via `scripts/apply-repo-settings.sh`, `gh api`, 2026-11-10 UTC.

- Order (D-13, orchestrator resolution 2): applied only after main already had a successful `CI required` check run
  (push run 42737946832), and after PR A's `.planning/`-only run was green.
- `sh scripts/apply-repo-settings.sh --apply`: `repo-settings: created ruleset main`, then `repo-settings: ruleset ok (id 26908822)`.
  The live API accepted `.github/rulesets/main.json` unchanged (no `allowed_merge_methods` fix was needed).
- Ruleset ID: 26908822, enforcement `active`, target branch `~DEFAULT_BRANCH`.
- `sh scripts/apply-repo-settings.sh --check`: exit 0. Output, verbatim:

```
repo-settings: ruleset ok (id 26908822)
repo-settings: advisory: squash_merge_commit_title=COMMIT_OR_PR_TITLE (not applied: outside the owner's ruleset-only authorization)
repo-settings: advisory: actions sha_pinning_required=false (not applied: outside the owner's ruleset-only authorization)
repo-settings: advisory: secret_scanning=enabled (not applied: outside the owner's ruleset-only authorization)
repo-settings: advisory: secret_scanning_push_protection=enabled (not applied: outside the owner's ruleset-only authorization)
repo-settings: advisory: rules not requested: deletion, non_fast_forward, required_linear_history, merge-method restriction (not applied: outside the owner's ruleset-only authorization)
```

- `gh api repos/szTheory/gamebloke/rules/branches/main` lists `pull_request` and `required_status_checks` (context
  `CI required`), both from ruleset 26908822. `gh api repos/szTheory/gamebloke/rulesets/26908822 --jq '.bypass_actors | length'`
  prints `0`.
- Unchanged settings after the apply: `squash_merge_commit_title` is `COMMIT_OR_PR_TITLE`, Actions
  `sha_pinning_required` is `false`. Nothing other than the ruleset was written.
- Open item: real-logo rule inactive on origin/main until a digest lands (tests/signatures/logo.sha256 row is owner-pending; synthesized controls still guard the logo-window path)

## Real logo digest

- Status: `owner-pending (human-needed)`. Recorded 2026-11-10 UTC by plan 01-11 Task 2 from the owner's Task 1 reply, `pending`.
- Owner: the repository owner. No agent opened, copied, or hashed any owner-held file; no digest crossed.
- Next action: the owner runs `sh scripts/signature-digest.sh logo <file>` on a GBA-header-bearing file they already hold,
  outside the repository, and commits the printed `<64 hex> nintendo-logo-window` line through a PR, replacing the single
  `owner-pending nintendo-logo-window` row in `tests/signatures/logo.sha256`. A matching hash proves identity, not rights.
- Current visible state: `tests/signatures/logo.sha256` is unchanged (the `owner-pending` row). `signature.logo.real_digest`
  reports SKIPPED with its reason (never PASS), `signature.owner_local` reports SKIPPED, and the inventory summary shows
  `skipped 2 (allowed 2)`. CI stays green. EVID-04's real-logo item is a human-needed gap; the synthesized logo-window
  controls still guard the rule path.

## Close-out publication

- Date: 2026-11-10 UTC. Plan 01-10 Task 3 (checkpoint:decision, `gate="blocking-human"`, no auto-select).
- Owner's answer, copied from the decision line: `close-out-pr` (authorize ONE additional close-out PR under the same
  rules: `CI required` must pass and the PR is squash-merged through the ruleset, no admin bypass, no direct push to main).
- The authorization is exercised by plan 01-11 Task 3, not by plan 01-10. Plan 01-10 opened no third PR and pushed nothing;
  the close-out branch `gsd/phase-01-ci-and-verification-foundation` is still local and unpushed.
- Plan 01-11 (2026-11-10 UTC): the owner replied `pending` to the digest request, so the status line is unchanged:
  real-logo rule inactive on origin/main until a digest lands
- Plan 01-11 Task 3 exercises the authorization: the close-out branch is pushed and merged through one PR (squash,
  through the ruleset, no admin bypass). The PR number and run IDs are in 01-11-SUMMARY.md and the plan 01-11 report.
