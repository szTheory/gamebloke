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

Pending: recorded by plan 09 Task 2 / plan 10 / plan 11.

## Ruleset

Pending: recorded by plan 09 Task 2 / plan 10 / plan 11.

## Real logo digest

Pending: recorded by plan 09 Task 2 / plan 10 / plan 11.

## Close-out publication

Pending: recorded by plan 09 Task 2 / plan 10 / plan 11.
