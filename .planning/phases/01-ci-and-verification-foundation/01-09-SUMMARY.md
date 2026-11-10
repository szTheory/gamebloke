---
phase: 01-ci-and-verification-foundation
plan: 09
subsystem: infra
tags: [github-actions, cmake-3.25-floor, squash-merge, measured-timeouts, linux-lane, evidence]

requires:
  - phase: 01-ci-and-verification-foundation
    provides: "ci.yml with the CI required aggregator (plans 01-06); ci lane at 93 expected, hygiene at 41; scripts/linux-lane.sh and the asan lane (plan 05); ruleset file and guarded apply script, not applied (plan 08)"
provides:
  - "01-EVIDENCE.md with the CMake 3.25.3 floor and Linux pre-check sections filled; later sections marked pending for plans 10 and 11"
  - "docs/verify.md: 112-line developer guide to the verify command, lanes, gates, reason codes, adding tests, signatures, hook, CI shape, slow lanes and the manifest.lint extension point"
  - "PR B (#1) squash-merged to main; ci.yml and the whole phase source are live; main's push run reports CI required success"
  - "Measured job timeouts in ci.yml (3 minutes each, every job commented with its run IDs)"
  - "Script-mode (-P) entry scripts pin the 3.25 policy set, so the gate scripts run on the 3.25 floor"
affects: [01-10, 01-11]

actuals:
  tokens: 4781
  tasks: 2
  commits: 1
plan_head_before: 8ed7f50fdeafcd6019dfd1d8e861ff82ca50952b
plan_head_after: d1b59327fc348138ff725781b8c29f04c5f6f350

tech-stack:
  added: []
  patterns:
    - "Every cmake -P entry script starts with cmake_minimum_required(VERSION 3.25) so script mode does not run with all policies OLD (CMP0057 IN_LIST) on the floor"
    - "Squash merge reuses the branch tree: tree equality (git diff --quiet origin/main <branch>) gates the local reset, and a backup ref keeps the pre-squash commits"

key-files:
  created:
    - docs/verify.md
    - .planning/phases/01-ci-and-verification-foundation/01-EVIDENCE.md
  modified:
    - .github/workflows/ci.yml
    - cmake/CheckCoreIncludes.cmake
    - cmake/CheckSignatureDigests.cmake
    - cmake/ExpectFail.cmake
    - cmake/InventoryCompare.cmake
    - cmake/RulesetCheck.cmake
    - tests/harness_case.cmake
    - tests/gates/hygiene_case.cmake
    - tests/gates/signature_case.cmake
    - tests/ci/ci_cases.cmake
    - tests/ci/ruleset_cases.cmake
    - tests/ci/workflow_cases.cmake

key-decisions:
  - "All six job timeouts are 3 minutes. The slowest measured job was 36 s (macOS build), so ceil(2 x slowest) is 2 minutes and the plan's floor of 3 applies to every job"
  - "The 3.25 floor is held by pinning policies in each script-mode entry script, not by raising the floor or special-casing IN_LIST"
  - "Linux pre-check runs were taken on commit 9dfeda8 (final build source); the later commits only add docs, evidence and ci.yml timeout values"

patterns-established:
  - "Floor evidence records the first failing run and its cause next to the passing run, so the floor result is not a bare pass"

requirements-completed: [VERIF-01, VERIF-05]

coverage:
  - id: D1
    description: "The ci workflow preset passes on CMake 3.25.3 (Kitware tarball, SHA-256 verified) with the inventory summary line in the log, confirming the verbose post-step output on the floor"
    requirement: "VERIF-01"
    verification:
      - kind: other
        ref: "<cmake 3.25.3> --workflow --preset ci: exit 0, 100% tests passed out of 93, inventory: expected 93, ran 91, skipped 2 (allowed 2); recorded in 01-EVIDENCE.md"
        status: pass
    human_judgment: false
  - id: D2
    description: "The ci gcc and asan lanes pass in the digest-pinned ubuntu:26.04 container on the native aarch64 architecture before the first push"
    requirement: "VERIF-01"
    verification:
      - kind: other
        ref: "sh scripts/linux-lane.sh ci gcc (93 expected, 91 ran, 2 skipped); sh scripts/linux-lane.sh asan (9 expected, 9 ran, 0 skipped)"
        status: pass
    human_judgment: false
  - id: D3
    description: "PR B's run shows changes, hygiene, title, both build legs, asan and CI required all success, so a source change runs every lane"
    requirement: "VERIF-05"
    verification:
      - kind: other
        ref: "gh run view 42737527955 (attempts 1 and 2) and 42737836505: all seven jobs success"
        status: pass
    human_judgment: false
  - id: D4
    description: "PR B is squash-merged without bypass and the main push run reports CI required success"
    requirement: "VERIF-05"
    verification:
      - kind: other
        ref: "gh run view 42737946832 (push, main d1b5932): conclusion success; check run CI required success from integration 15368"
        status: pass
    human_judgment: false
  - id: D5
    description: "docs/verify.md lets a developer learn the verify system, slow lanes and extension points from one page"
    requirement: "VERIF-01"
    verification:
      - kind: other
        ref: "grep checks for the canonical command, Slow-lane checks and manifest.lint; 112 lines"
        status: pass
    human_judgment: true
    rationale: "Whether the guide reads well and is complete enough for a new developer is a judgment no test asserts"

duration: 10 min
completed: 2026-11-10
status: complete
---

# Phase 1 Plan 09: First Live CI Run, Measured Timeouts, and Squash Merge Summary

**The CMake 3.25.3 floor and both Linux lanes were proven locally (after a policy fix that the floor run exposed), PR B (#1) ran every lane green on GitHub, took measured 3-minute timeouts, and was squash-merged; main's push run reports `CI required` success.**

## Performance

- **Duration:** 10 min (2026-11-10T01:36:22Z to 01:46:22Z)
- **Tasks:** 2
- **Files:** 2 created, 12 modified (diff from the plan base: 14 files, 229 insertions, 6 deletions)
- **Verification surface after merge:** ci lane 93 expected / 91 ran / 2 skipped; hygiene 41 / 39 / 2; asan 9 / 9 / 0

## Accomplishments

- **CMake 3.25 floor (VERIF-01):** downloaded `cmake-3.25.3-macos-universal.tar.gz` from the Kitware release, verified SHA-256 `771548ed2abae17f3fd28dcfa572ba3fe9f970652a72c36c2e1aafdee93a234e` against Kitware's published list, extracted outside the repository, and ran `--workflow --preset ci`. Final run: exit 0, 93 of 93 passed, `inventory: expected 93, ran 91, skipped 2 (allowed 2)` in the log, so `output.verbosity` is accepted and effective on 3.25.3 (B1). No PyPI cmake anywhere.
- **Floor finding and fix:** the first floor run exited 8 with 11 of 93 failing. Script-mode `-P` scripts start with every policy OLD, so `if(NOT x IN_LIST y)` was rejected (CMP0057) on 3.25; the presets file was accepted. Every `-P` entry script now calls `cmake_minimum_required(VERSION 3.25)`. Default cmake 4.4.3 still passes (93/91/2 ci, 41/39/2 hygiene).
- **Linux pre-checks (Pitfall 11):** Docker was available (daemon aarch64, native, not emulated). `sh scripts/linux-lane.sh ci gcc` exit 0 (CMake 4.2.3, GCC 15.2.0, 93/91/2); `sh scripts/linux-lane.sh asan` exit 0 (clang 21.1.8, 9/9/0). Image `ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c`.
- **Developer guide:** `docs/verify.md` (112 lines) covers the canonical command and its four steps, the three lanes, why `ctest` alone is not the verify command, adding tests and inventory lines, negative controls, reason-code families, the signature lists, allowlist scope and owner-pending logo row, the opt-in hook, CI shape, a `Slow-lane checks` section, and the `manifest.lint` extension point.
- **Publication (D-12):** phase branch pushed, PR B opened, run found within about 45 seconds (no bootstrap problem), all seven jobs green, a rerun green, measured timeouts committed and green, squash-merged with no admin bypass, main push run green, local main reset to origin/main after a tree-equality check and a backup ref.

## PR B and run record (copied into 01-EVIDENCE.md by plan 10)

- **PR B:** https://github.com/szTheory/gamebloke/pull/1, title `ci: add verification foundation and CI required check`, state `MERGED` at 2026-11-10T01:44:44Z.
- **Merge commit on main:** `d1b59327fc348138ff725781b8c29f04c5f6f350` (squash; tree identical to branch tip `6cdff80`).
- **Branch commits:** `9dfeda8` (floor fix), `2df5446` (guide and evidence), `6cdff80` (measured timeouts). `9dfeda8` and `2df5446` are kept by local ref `backup/phase-01-pre-squash`; all three remain reachable from the PR ref until GitHub expires it.

| Run ID | Event | Head | Conclusion |
|--------|-------|------|------------|
| 42737527955 attempt 1 | pull_request | `2df5446` | success |
| 42737527955 attempt 2 (rerun) | pull_request | `2df5446` | success |
| 42737836505 | pull_request | `6cdff80` (merged tree) | success |
| 42737946832 | push (main) | `d1b5932` | success |

All seven jobs (changes, hygiene, title, build ubuntu-26.04 GNU, build macos-26 AppleClang, asan, CI required) were `success` in every PR run. On the push run `title` was `skipped` by design and the other six were `success`; the `CI required` check run is `success` from integration 15368.

**Per-job durations (seconds, `startedAt` to `completedAt`):**

| Job | Run 1 | Run 2 (rerun) | Run 42737836505 | Main push |
|-----|-------|---------------|-----------------|-----------|
| changes | 5 | 4 | 5 | 4 |
| hygiene | 8 | 8 | 9 | 9 |
| title | 4 | 6 | 4 | skipped |
| build ubuntu-26.04 GNU | 20 | 12 | 10 | 14 |
| build macos-26 AppleClang | 36 | 27 | 19 | 28 |
| asan | 7 | 10 | 9 | 10 |
| CI required | 6 | 4 | 6 | 5 |

**Timeouts (D-15):** the two samples were run 1 and its rerun (42737527955 attempts 1 and 2). Slowest per job: changes 5 s, hygiene 8 s, title 6 s, build 36 s, asan 10 s, CI required 6 s. ceil(2 x slowest) is at most 2 minutes, so the floor of 3 applies and `timeout-minutes: 3` is set on all six jobs, each commented `# 2 x slowest of runs 42737527955#1,42737527955#2 (<s>s)`. `grep -c '# provisional'` on origin/main prints 0 and `grep -c '2 x slowest of runs'` prints 6. Later runs (19 to 28 s for the macOS build) stayed far inside 3 minutes.

**Critical path (AGENTS.md):** a PR run takes about 52 s of wall clock: `changes` (5 s), then the macOS build (the longest job), then `CI required`. No duplicated builds or setup were found. The ubuntu GNU build and asan finish before macOS. The `ci` lane repeats the hygiene tests the always-on `hygiene` job also runs (8 to 9 s); left as is until measured cost justifies a change.

**Asan and Linux logs (confirmed although Docker was available):** the asan job log contains `shift exponent 32 is too large for 32-bit type`, `AddressSanitizer: heap-buffer-overflow`, and `inventory: expected 9, ran 9, skipped 0 (allowed 0)`; the ubuntu GNU leg and the macOS leg each print `inventory: expected 93, ran 91, skipped 2 (allowed 2)`.

## Task Commits

1. **Task 1: floor and Linux pre-checks, developer guide** - `9dfeda8` (fix, floor policy pin) and `2df5446` (docs, guide and evidence)
2. **Task 2: publication** - `6cdff80` (ci, measured timeouts) on the PR branch, squash-merged as `d1b5932` on main

## Verification Evidence

- Task 1 automated command: hygiene `--tree` ok, signature scan ok, evidence greps (floor section, 93/91/2 line, verbosity line), `cmake --workflow --preset ci` and `manifest.lint` greps all pass; `wc -l docs/verify.md` is 112; `git grep "pip install"` over `.github scripts cmake CMakePresets.json` prints nothing.
- Task 2 automated command on the reset main (HEAD equals origin/main): latest main push run `success`, `CI required` check run `success`, `git diff --quiet origin/main HEAD`, local `cmake --workflow --preset ci` exit 0 with `inventory: expected 93, ran 91, skipped 2 (allowed 2)`. Printed `VERIFY-OK`.
- Acceptance: PR B `state MERGED`; `backup/phase-01-pre-squash` exists; `git ls-remote --heads origin` shows only `main`; the final PR run lists both build legs and asan as `success`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Script-mode gate scripts failed on the CMake 3.25 floor**
- **Found during:** Task 1 (floor run)
- **Issue:** Script mode (`cmake -P`) starts with every policy OLD. `IN_LIST` is then a plain argument (CMP0057), so the core-include gate, the inventory comparison, the ruleset validator and the ruleset case driver failed with "Unknown arguments specified": 11 of 93 tests failed, and the workflow stopped before the post step. The presets file itself was accepted. This is not the I-1 stop condition (that is a rejected presets file, or a missing summary line with exit 0).
- **Fix:** `cmake_minimum_required(VERSION 3.25)` at the top of each `-P` entry script, with a comment. For `InventoryCompare.cmake`, which is also `include()`d at configure time, the pin precedes the function definition because a function keeps the policies in force where it is defined.
- **Files modified:** cmake/CheckCoreIncludes.cmake, cmake/CheckSignatureDigests.cmake, cmake/ExpectFail.cmake, cmake/InventoryCompare.cmake, cmake/RulesetCheck.cmake, tests/harness_case.cmake, tests/gates/hygiene_case.cmake, tests/gates/signature_case.cmake, tests/ci/ci_cases.cmake, tests/ci/ruleset_cases.cmake, tests/ci/workflow_cases.cmake (outside the plan's `files_modified`; needed to complete the floor requirement)
- **Verification:** floor run exit 0 with 93/91/2; default cmake ci 93/91/2 and hygiene 41/39/2; Linux gcc and asan lanes and the three GitHub runs all green on the fixed source
- **Commit:** `9dfeda8`

**Total deviations:** 1 auto-fixed (Rule 1). **Impact:** none on scope or test counts; it removes a floor-only failure that the floor check exists to catch.

### Plan-detail choices (not defects)

- The first floor run is recorded in 01-EVIDENCE.md alongside the passing run rather than only the final pass.
- Linux pre-checks ran on `9dfeda8`; the later commits add docs, evidence and `timeout-minutes` values only, and the PR's own runs cover the final tree.
- `commits:` in the frontmatter is the measured count from the recorded base to main after the squash (1). The plan's own work was 3 branch commits plus the squash; see Task Commits.

## Known Gaps

- The `macos-26` annotation GitHub printed ("jobs targeting macOS arm64 runners may experience longer queue times") affects queue time, which `timeout-minutes` does not count; no action. If macOS runtime ever approaches 3 minutes the timeout should be re-measured.
- The 3-minute timeouts rest on small samples (four runs for macOS). Re-measure when the build grows.

## Auth Gates

None. `gh auth status` was valid with admin on the repository.

## Known Stubs

None. `01-EVIDENCE.md` sections after `## Linux pre-checks` say `Pending: recorded by plan 09 Task 2 / plan 10 / plan 11`; that is the plan's design (plan 10 copies the run IDs above into `## PR B (source)` and `## main push run`).

## Threat Flags

None beyond the plan's register. T-01-24: hygiene `--tree` ran before the push and the CI hygiene job passed; T-01-25: Kitware tarball verified by SHA-256, extracted outside the repository, used once, never in CI; T-01-26: merged only after all jobs were green, no admin bypass; T-01-27: tree equality and backup ref before the reset; T-01-SC-09: no package install. No ruleset, repository setting, release or tag was touched.

## Next Phase Readiness

Plan 01-10 can apply the ruleset: a `CI required` run has succeeded on main (run 42737946832), so `sh scripts/apply-repo-settings.sh --apply` will pass its own guard. Local main equals origin/main plus this plan's SUMMARY and STATE commit (planning-only, unpushed), which plan 10 carries to main through PR A. Record PR B's number, run IDs, durations and merge SHA from this file into 01-EVIDENCE.md.

## Self-Check: PASSED

Created files exist (`docs/verify.md`, `01-EVIDENCE.md`); the modified `ci.yml` and `cmake/InventoryCompare.cmake` exist; merge commit `d1b5932` is an ancestor of main; `9dfeda8`, `2df5446` and `6cdff80` are intentionally not ancestors of main after the squash (the first two sit under `backup/phase-01-pre-squash`, all three under `refs/pull/1/head`); plan verification re-run green after the reset (93/91/2).
