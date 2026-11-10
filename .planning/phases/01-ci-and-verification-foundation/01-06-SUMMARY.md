---
phase: 01-ci-and-verification-foundation
plan: 06
subsystem: infra
tags: [github-actions, ci, aggregator, classifier, conventional-commits, cmake, posix-sh, negative-controls]

requires:
  - phase: 01-ci-and-verification-foundation
    provides: "workflow presets ci, hygiene, asan; gbl_add_test and inventory compare; gbl_expect_fail wrapper; tests/gates.cmake; asan lane and local Linux runner (plans 01 to 05)"
provides:
  - "scripts/classify-changes.sh <count>: heavy=false only for a complete, .planning/-only listing; every other input prints heavy=true; exit 2 on a bad count"
  - "cmake/CiRequired.cmake: aggregator predicate over NEEDS/HEAVY/EVENT; a heavy job skipped while HEAVY is not exactly false fails"
  - "scripts/check-title.sh <title>: Conventional Commits PR title gate with reason codes empty, too-long, not-conventional, usage"
  - ".github/workflows/ci.yml: changes, hygiene, title, build matrix (ubuntu-26.04 GCC, macos-26 AppleClang), asan (clang-21), and the single `CI required` check; SHA-pinned actions"
  - "GBL_EXPECT_C_ID configure guard (`gamebloke: compiler-id-mismatch`)"
  - "tests/ci_gates.cmake: lane-guarded CI gate registrations; ci.classify.cases, ci.required.cases, ci.title.cases, build.nc.compiler_id_guard"
affects: [01-07, 01-08, 01-09, 01-10, 01-11]

actuals:
  tokens: 6500
  tasks: 3
  commits: 3
plan_head_before: 7df9f59dad0a8aa08d23cef11b18f4c6312964df
plan_head_after: 3db0fa7d476a49efe6061e64064d7a3fb2a4e478

tech-stack:
  added: []
  patterns:
    - "Routing logic lives in unit-tested scripts (case tables), not inline workflow shell; the workflow only wires env to them"
    - "PR metadata reaches shell only through env: or a gh call; no expression inside a run block"
    - "ci_gates.cmake is included for ci and hygiene; every registration sits inside if(GBL_LANE STREQUAL \"ci\") unless marked hygiene-safe and listed in hygiene.txt"
    - "Case-table fail rows carry optional `&`-separated reason fragments so a control cannot pass for the wrong reason (CMake JSON member order is not input order)"

key-files:
  created:
    - .github/workflows/ci.yml
    - cmake/CiRequired.cmake
    - scripts/classify-changes.sh
    - scripts/check-title.sh
    - tests/ci_gates.cmake
    - tests/ci/classify_cases.sh
    - tests/ci/ci_cases.cmake
    - tests/ci/title_cases.sh
  modified:
    - CMakeLists.txt
    - tests/inventory/ci.txt

key-decisions:
  - "Leading zeros are stripped from the changed-file count with a POSIX loop instead of `10#`, which dash rejects; a count over nine digits is treated as over the cap"
  - "A title with a second line is refused as not-conventional, because grep matches per line and would otherwise judge only the first line"
  - "The asan setup step checks and installs clang-21 together with libclang-rt-21-dev (01-05 finding), not libclang-rt-21-dev alone"
  - "Build matrix written as block mappings, one key per line, so plan 07's awk readers stay simple"

patterns-established:
  - "Pitfall 6 defense: the aggregator treats unset HEAVY as not false, so a failed `changes` job cannot let skipped heavy jobs pass"
  - "Compiler identity is asserted at configure time from CI env, so a runner-image change cannot silently swap the compiler"

requirements-completed: [VERIF-05, VERIF-01, VERIF-02]

duration: 10 min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 06: CI Routing, Aggregator, and Workflow Summary

**A single `CI required` check now sits over a classifier, an aggregator, a live-title gate, and three preset-only lanes (Linux GCC, macOS arm64, Linux clang-21 sanitizers); the routing predicates are case-table tested, and the workflow is SHA-pinned with no expression inside any run block.**

## Performance

- **Duration:** about 10 min
- **Tasks:** 3
- **Files:** 8 created, 2 modified

## Accomplishments

- **Classifier (D-11):** prints `heavy=false` only with at least one path, at least as many listed names as changed files (renames add a line), every name under `.planning/?*`, no `(listing failed)`, and a count of at most 3000. The 14-case table covers both prefix lookalikes (`.planningx/`, `.planning-evil/`), the short listing, the rename pairs, the 3001 cap, and usage. The short-listing case is also checked for its own reason text.
- **Aggregator (D-11, Pitfall 6):** 14-case table including heavy-skipped-while-heavy (fails with `not allowed: build=skipped`), empty HEAVY after a failed `changes`, hygiene never skippable, title skippable only off pull requests, cancelled and failed lanes, an empty `{}`, and a job without a `result` member.
- **Workflow:** triggers `pull_request` (opened, synchronize, reopened, edited) and `push` to main with no path filter; `CI required` runs `if: always()` and needs changes, hygiene, title, build, asan. `build` and `asan` only call `cmake --workflow --preset ci|asan`; `hygiene` only `cmake --workflow --preset hygiene`. Six checkouts, six `persist-credentials: false`. Only `changes` and `title` add `pull-requests: read`. Concurrency cancels superseded pull_request runs but never push runs. The title job reads the live title with `gh pr view` into a variable and passes it as one quoted argument.
- **Compiler guard:** `GBL_EXPECT_C_ID=NoSuchCompiler` fails configure with `gamebloke: compiler-id-mismatch: expected NoSuchCompiler got <id>`; `build.nc.compiler_id_guard` keeps it live.
- **Title gate (D-14):** 19-case table, including exactly 100 vs 101 characters, empty, no-argument and two-argument usage, and a shell-injection check that title text is never executed.

## Task Commits

1. **Task 1: classifier routes .planning/-only changes and fails closed otherwise** - `929c561` (feat)
2. **Task 2: CI required aggregator refuses wrongly skipped or failed lanes** - `98baf55` (feat)
3. **Task 3: CI workflow with presets, live title check, and compiler guard** - `3db0fa7` (feat)

## Verification Evidence

- `cmake --workflow --preset ci` on macOS arm64 (AppleClang): exit 0, `inventory: expected 66, ran 64, skipped 2 (allowed 2)`.
- `cmake --workflow --preset hygiene`: exit 0, `inventory: expected 39, ran 37, skipped 2 (allowed 2)` (unchanged, no ci-only test leaked into the hygiene lane).
- Linux proof (aarch64 container, ubuntu:26.04, `sh scripts/linux-lane.sh ci gcc`): `100% tests passed, 0 tests failed out of 66` and `inventory: expected 66, ran 64, skipped 2 (allowed 2)`; asan lane `inventory: expected 9, ran 9, skipped 0 (allowed 0)`. The new routing tests, JSON parsing in the aggregator, and the guard all pass under Ubuntu's CMake 4.2.3 and dash.
- Mutation checks (scratch copies, not committed): replacing the classifier prefix test with a loose `.planning*` made four classifier cases fail; making the aggregator ignore HEAVY made three aggregator cases fail; raising the title length limit to 1000 made the 101-character case fail.
- The case runners also pass under `dash`, the `sh` of the ubuntu runner.
- Action SHAs re-verified read-only with `gh api`: `actions/checkout` tag v7.0.1 is `3d3c42e5aac5ba805825da76410c181273ba90b1`; `actions/upload-artifact` tag v7.0.2 is `cf430e030ddbb5b0abf93d22962f4752f3646cd9`.
- Acceptance criteria re-run: one `name: CI required`; runs-on values only `ubuntu-26.04`, `macos-26`, or `${{ matrix.runner }}`; 6 `persist-credentials: false` for 6 `actions/checkout@`; checkout precedes the classify step in `changes`; `needs:` of `ci-required` lists all five; zero `${{` inside run blocks; no `-latest`, `pull_request_target`, `continue-on-error`, or `secrets`; `check-title.sh 'Add stuff'` exits 1 with `title: not-conventional`; the guard fails configure as required.
- Nothing was pushed and no workflow was triggered; the first live run is plan 09.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Non-POSIX `10#` arithmetic in the classifier**
- **Found during:** Task 1, first draft run under `dash`
- **Issue:** `$((10#$count))` is a bashism; dash (the ubuntu runner's `sh`) rejects it, so the classifier would have errored on Linux.
- **Fix:** strip leading zeros with a POSIX loop; a count over nine digits is treated as over the cap.
- **Files modified:** scripts/classify-changes.sh
- **Verification:** case runner passes under both `sh` (macOS) and `dash`; leading-zero count case added.
- **Commit:** 929c561 (fixed before first commit)

**2. [Rule 2 - Missing critical] Multi-line title refused**
- **Found during:** Task 3 design of check-title.sh
- **Issue:** `grep -Eq` matches per line, so a title whose second line is arbitrary but whose first line is conventional would pass.
- **Fix:** a title containing a newline is rejected as `title: not-conventional`.
- **Files modified:** scripts/check-title.sh
- **Commit:** 3db0fa7

**3. [Rule 3 - Blocking] asan setup step installs clang-21 as well**
- **Found during:** Task 3, reading 01-05-SUMMARY.md
- **Issue:** the plan's setup line checked only `libclang-rt-21-dev`, but the asan preset hardcodes the `clang-21` binary; 01-05 recorded both package names as required if absent.
- **Fix:** the check and the install cover `clang-21` and `libclang-rt-21-dev`.
- **Files modified:** .github/workflows/ci.yml
- **Commit:** 3db0fa7

### Process notes (no code deviation)

**4. Direct commits on `main`:** committed locally on `main` as the orchestrator directed (`git.branching_strategy: none`); nothing was pushed.

**5. actionlint label list:** `actionlint` (local) reports the only finding that `ubuntu-26.04` is not in its built-in runner-label list. The label is the orchestrator-mandated one (resolution 3) and a moving `-latest` label is prohibited, so it stays. Plan 09's first live run is the proof the label resolves; if a policy gate in plan 07 reuses actionlint it will need an `actionlint.yaml` label entry.

**Total deviations:** 3 auto-fixed (1 Rule 1, 1 Rule 2, 1 Rule 3), 2 process notes. **Impact:** none on the planned deliverables.

## Auth Gates

None.

## Known Stubs

None. `timeout-minutes` values carry `# provisional` by plan; plan 09 replaces them with measured values (D-15).

## Threat Flags

None beyond the plan's threat model. T-01-15 (script injection): PR number, changed-file count, and title arrive only through `env:` or `gh`; zero `${{` in any run block. T-01-16: every `uses:` is a 40-hex SHA with a version comment. T-01-17/18: covered by `ci.required.cases` and `ci.classify.cases`. T-01-19: top-level `contents: read`, `pull-requests: read` only on `changes` and `title`, `persist-credentials: false` on all six checkouts. T-01-23: title read into a variable, passed quoted, matched with `grep -E`; the injection check in `ci.title.cases` confirms it is never executed.

## Issues Encountered

None.

## Next Phase Readiness

Ready for 01-07: the workflow file has a regular shape (block mappings, one key per line, pin lines `uses: <action>@<40-hex> # v<version>`) for the awk-based pin and policy gates. Those gates should also assert that every job id appears in `ci-required`'s `needs`, that no `${{` appears in a run block, and (if actionlint is used) add `ubuntu-26.04` to its label config.

## Self-Check: PASSED

- Created files exist: .github/workflows/ci.yml, cmake/CiRequired.cmake, scripts/classify-changes.sh, scripts/check-title.sh, tests/ci_gates.cmake, tests/ci/classify_cases.sh, tests/ci/ci_cases.cmake, tests/ci/title_cases.sh.
- Commits `929c561`, `98baf55`, and `3db0fa7` are ancestors of HEAD; `git rev-list --count` from the recorded base measures 3.
