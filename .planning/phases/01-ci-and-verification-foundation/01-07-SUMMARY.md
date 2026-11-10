---
phase: 01-ci-and-verification-foundation
plan: 07
subsystem: infra
tags: [github-actions, ci, action-pinning, workflow-policy, posix-sh, awk, cmake, negative-controls]

requires:
  - phase: 01-ci-and-verification-foundation
    provides: ".github/workflows/ci.yml (six jobs, SHA-pinned actions, CI required aggregator); tests/ci_gates.cmake lane-guard rule; gbl_expect_fail/gbl_expect_pass; gbl_add_test and inventory compare (plans 01 to 06)"
provides:
  - "scripts/check-action-pins.sh [workflow...]: every non-local uses: line must be owner/repo[/path]@<40 lowercase hex> with a # v<digit> comment; reasons pins: unpinned, pins: no-version-comment, pins: ok (<n> uses)"
  - "scripts/check-workflow.sh [file]: static policy gate over ci.yml with twelve reason codes (moving-label, path-filter, pr-target, needs-missing, required-check, expression-in-run, non-preset-run, unknown-preset, no-timeout, permissions, persist-credentials, continue-on-error) plus no-jobs; preset names come from cmake --workflow --list-presets"
  - "tests/ci/workflow_cases.cmake: case driver that mutates a copy of the real ci.yml (first occurrence, mutate: no-match when the anchor is gone) and runs the case's gate through gbl_expect_fail or gbl_expect_pass"
  - "ci.actions.pinned and ci.yml.policy as hygiene-safe gates (ci and hygiene lanes); ci.actions.nc.{unpinned,no_version_comment}, ci.yml.pos.copy and ten ci.yml.nc.* controls in the ci lane only"
affects: [01-08, 01-09, 01-10, 01-11]

actuals:
  tokens: 5600
  tasks: 2
  commits: 2
plan_head_before: 2b17c06936eb62538e8a5517d72dea4974a18743
plan_head_after: 8cee9d6782e064a833484c7788c561db2bf07f5f

tech-stack:
  added: []
  patterns:
    - "Workflow gates are checked-in text scanners (POSIX sh plus awk) with a reason code per rule; nothing is installed"
    - "Mutation controls rewrite a copy of the real workflow at test time instead of committing fixture workflows, and a stale anchor fails loudly with mutate: no-match"
    - "Hygiene-safe gates (no build tree, cheap) sit outside the ci guard and in both inventories; every control stays inside the ci guard"

key-files:
  created:
    - scripts/check-action-pins.sh
    - scripts/check-workflow.sh
    - tests/ci/workflow_cases.cmake
  modified:
    - tests/ci_gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt

key-decisions:
  - "Block scalars are tracked so run: | content is judged as content: a ${{ inside a run block counts even in a shell comment (the runner still expands it), but a run block never counts as workflow structure"
  - "The preset-only rule also covers ctest, cpack, ninja and make (not only cmake) in the build, asan and hygiene jobs, so a heavy lane cannot bypass the workflow preset with a direct tool call; the asan apt-get step is untouched because it invokes none of them"
  - "A workflow with no recognisable jobs fails closed (workflow: no-jobs) instead of passing vacuously"
  - "The no_timeout control renames the first timeout-minutes key instead of deleting its line, so it does not depend on the comment text that plan 09 may change when timeouts stop being provisional"

patterns-established:
  - "Case drivers for gates over a real file: read, mutate first occurrence, assert changed, write under BIN/nc/<case>, run through the expect wrapper"
  - "Reason-code prefixes are stable (workflow: / pins: / mutate:); any detail after the prefix is a job id, preset name, or line number"

requirements-completed: [VERIF-05, VERIF-02, VERIF-01]

coverage:
  - id: D1
    description: "Every uses: line in the workflow is pinned to a 40-hex SHA with a version comment, enforced by a gate in the ci and hygiene lanes"
    requirement: "VERIF-05"
    verification:
      - kind: unit
        ref: "ctest -R ^ci\\.actions\\. (ci.actions.pinned, ci.actions.nc.unpinned, ci.actions.nc.no_version_comment)"
        status: pass
    human_judgment: false
  - id: D2
    description: "ci.yml policy gate (labels, triggers, CI required needs, no expressions in run, preset-only heavy lanes, timeouts, credentials, permissions, continue-on-error) with a mutation control per case"
    requirement: "VERIF-01"
    verification:
      - kind: unit
        ref: "ctest -R ^ci\\.yml\\. (ci.yml.policy, ci.yml.pos.copy, ten ci.yml.nc.*)"
        status: pass
    human_judgment: false

duration: 4 min
completed: 2026-11-09
status: complete
---

# Phase 1 Plan 07: Action Pin and Workflow Policy Gates Summary

**Two checked-in text gates now keep `.github/workflows/ci.yml` honest: every action is pinned to a 40-hex SHA with a version comment, and a twelve-rule policy gate guards labels, triggers, the `CI required` needs list, run-block expressions, preset-only heavy lanes, timeouts, credentials and permissions; each case has a control that mutates a copy of the real workflow.**

## Performance

- **Duration:** about 4 min (measured wall clock for the session)
- **Tasks:** 2
- **Files:** 3 created, 3 modified

## Accomplishments

- **Pin gate (D-10):** `scripts/check-action-pins.sh` reads each `uses:` line (list-item or mapping form, quoted or not), skips `./` references, and requires a 40-character lowercase-hex ref and a `# v<digit>` comment. The real workflow reports `pins: ok (7 uses)`. It runs on `.github/workflows/ci.yml` as `ci.actions.pinned` in both the `ci` and `hygiene` lanes.
- **Policy gate (D-01, D-10, D-11, D-16):** `scripts/check-workflow.sh` reports every finding, then exits 1; the real workflow reports `workflow: ok (6 jobs)`. Heavy-lane commands are compared with the presets that `cmake --workflow --list-presets` returns (ci, hygiene, asan), so a lane that names a preset that does not exist is caught.
- **Controls without fixtures:** `tests/ci/workflow_cases.cmake` mutates the real file. Each control printed exactly its expected reason (checked from the `-V` output): `moving-label: 32`, `path-filter: 11`, `pr-target: 7`, `needs-missing: asan`, `expression-in-run: 68`, `non-preset-run: build`, `unknown-preset: nosuchlane`, `no-timeout: changes`, `persist-credentials: 40`, `continue-on-error: 104`, and the pin controls `pins: unpinned` and `pins: no-version-comment`. A case whose anchor is missing stops with `mutate: no-match` (confirmed on a scratch source tree whose workflow lacks `pull_request:`). No fixture workflow is committed.
- **Lane membership:** `ci.actions.pinned` and `ci.yml.policy` are the only registrations outside the ci guard, and the only additions to `tests/inventory/hygiene.txt`. The positive copy and all twelve controls are inside the guard.

## Task Commits

1. **Task 1: action pin gate with mutation controls on the real workflow** - `7c9b27d` (feat)
2. **Task 2: static workflow policy gate with ten mutation controls** - `8cee9d6` (feat)

## Verification Evidence

- `cmake --workflow --preset ci`: exit 0, `inventory: expected 81, ran 79, skipped 2 (allowed 2)`.
- `cmake --workflow --preset hygiene`: exit 0, `inventory: expected 41, ran 39, skipped 2 (allowed 2)`.
- `ctest --test-dir build/ci -R '^ci\.yml\.'`: 12 tests, 100% passed; `grep -c 'expect-fail: ok: /workflow: '` over the control output prints 10. `ctest -R '^ci\.actions\.'` runs 3 tests, all passed, with `expect-fail: ok:` for both `pins: unpinned` and `pins: no-version-comment`.
- `ctest --test-dir build/hygiene -N` lists `ci.actions.pinned` and `ci.yml.policy` and no `ci.actions.nc.`, `ci.yml.nc.` or `ci.yml.pos.` test. `git ls-files tests/fixtures/workflows` prints nothing.
- Linux proof (aarch64 container, ubuntu:26.04, `sh scripts/linux-lane.sh ci gcc`): `100% tests passed, 0 tests failed out of 81`, `inventory: expected 81, ran 79, skipped 2 (allowed 2)`. Both gates also run under `dash`.
- Scratch mutations of a copy (not committed) confirmed the two rules that have no committed control: top-level `contents: write` and an extra `id-token: write` both give `workflow: permissions`; `if: always()` changed to `if: success()` and a renamed `CI required` job both give `workflow: required-check`; a matrix runner ending in the moving label gives `workflow: moving-label`.
- Nothing was pushed and no workflow ran; the first live run is plan 09.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Pin gate read no input**
- **Found during:** Task 1, first run of check-action-pins.sh
- **Issue:** the awk program was not given the file operand, so it read standard input and reported `pins: ok (0 uses)` for every file, including a copy with unpinned refs.
- **Fix:** pass `"$f"` to awk. Re-ran the real file (7 uses), an unpinned copy (six findings), and the no-argument default.
- **Files modified:** scripts/check-action-pins.sh
- **Commit:** 7c9b27d (fixed before the first commit)

### Plan-detail choices (not defects)

- **`no_timeout` control renames the key instead of deleting the line.** The plan says to remove the first `timeout-minutes` line. A rename has the same effect for the gate and does not depend on the `# provisional` comment text.
- **Task 1 and Task 2 are single `feat` commits, not RED then GREEN.** Both tasks carry `tdd="true"` but the plan is `type: execute`, the tests and the script are one deliverable, and a committed failing test would turn the `ci` lane red on main. No separate RED commit was made (the controls cannot pass until the gates exist), and the earlier plans in this phase used the same one-commit-per-task shape.
- **Preset-only rule widened** to ctest, cpack, ninja and make (see key-decisions); stricter than the plan text, and the real workflow is clean.

**Total deviations:** 1 auto-fixed (Rule 1), 3 plan-detail choices. **Impact:** none on scope or test counts (81/79/2 and 41/39/2 as the plan requires).

## Known Gaps

- Two rules in `scripts/check-workflow.sh`, `permissions` and `required-check`, have no committed mutation control. The plan fixes the control set at ten policy cases and the inventory totals at 81 and 41, and its case list does not include them, although one must-have says every rule has a control. They were exercised by scratch mutations (above) but nothing in the repository keeps them live. Adding two `ci.yml.nc.*` cases would change the totals that plans 08 to 11 may rely on, so it is left as a decision for the owner or the plan 08/09 checker. Proposed cases: `permissions` (first `contents: read` to `contents: write`) and `required_check` (`if: always()` to `if: success()`).
- `workflow: ...: <line>` codes carry a line number, and the checkout/needs/timeout rules assume the two-space block-style YAML that ci.yml uses; an unrecognisable shape fails closed with `workflow: no-jobs` rather than passing.

## Auth Gates

None.

## Known Stubs

None.

## Threat Flags

None. No new network endpoint, auth path, file access pattern or schema; the gates only read workflow text, and T-01-20 (workflow edits that weaken gates or unpin actions) is mitigated by the pin and policy gates in the `ci` and `hygiene` lanes. No package was installed (T-01-SC-07).

## Next Phase Readiness

Plan 01-08 (ruleset file and its apply/check script) can proceed. The workflow job ids, the `CI required` name, and the 81/79/2 and 41/39/2 inventory totals are the new baseline for it.

## Self-Check: PASSED

Created files and both task commits (7c9b27d, 8cee9d6) verified present; plan verification re-run green (81/79/2 ci, 41/39/2 hygiene).
