---
phase: 01-ci-and-verification-foundation
plan: 03
subsystem: infra
tags: [hygiene, posix-sh, cmake, ctest, negative-controls, pre-commit, secrets, privacy]

requires:
  - phase: 01-ci-and-verification-foundation
    provides: "gbl_add_test, inventory compare, gbl_expect_fail / gbl_expect_pass, check-junit.sh (plans 01 and 02)"
provides:
  - "scripts/hygiene.sh --tree | --staged | --dir <path>: refuses credential shapes, non-noreply emails, absolute home paths, and (staged) a non-noreply commit identity; findings print path:line only"
  - "16 hygiene.* tests with fragment-built planted controls and positive companions, registered in the ci lane"
  - "Light `hygiene` lane (configure/build/test/test-post presets and workflow) that compiles nothing and runs only repository gates"
  - "Opt-in .githooks/pre-commit (mode 100755) running the staged scan, with one control per hook failure reason"
affects: [01-04, 01-05, 01-06, 01-07, 01-08, 01-09, 01-10, 01-11]

actuals:
  tokens: 14000
  tasks: 2
  commits: 2
plan_head_before: 6a88ee31e8c717e6d193da93b2b0020a5adeed81
plan_head_after: 6dde63f87e148f33824bfe4f8af988bac71c86f1

tech-stack:
  added: []
  patterns:
    - "Hygiene findings print `hygiene: <code>: <path>:<line>`; the matched text is reduced to a line number inside the pipeline and never printed"
    - "Allowances (runner home, noreply and example addresses) apply to the whole grep -o match, so one allowed address cannot hide a disallowed one on the same line"
    - "Planted offenders are assembled from fragments and string(RANDOM) at test time under build/ci/nc/<case>; the git history holds no realistic literal"
    - "Staged controls run in a real sandbox git repository with GIT_* state scrubbed through cmake -E env --unset"
    - "A control that plants several shapes also counts the findings (expect_findings), so each shape is proven detected and not just the first"

key-files:
  created:
    - scripts/hygiene.sh
    - tests/gates.cmake
    - tests/gates/hygiene_case.cmake
    - tests/inventory/hygiene.txt
    - .githooks/pre-commit
  modified:
    - tests/inventory/ci.txt
    - CMakeLists.txt
    - CMakePresets.json

key-decisions:
  - "hygiene.sh scans its own source with no exemption; the regexes use escaped dots and bracket classes so none matches its own line, and a CTest-run --tree covers it"
  - "File lists for all three modes are NUL-delimited temp files (git ls-files -z, git diff -z, find -print0) fed to xargs -0, so newline and leading-dash names are scanned and a file count is available"
  - "The hook check matches only non-comment lines for the --staged call, so a comment mentioning it cannot satisfy hygiene.hook"
  - "The stop-policy and no-commit-on-main checks from the sibling are not ported; the hook is not enabled in this repository"

patterns-established:
  - "Case drivers take -DCASE/-DSRC/-DBIN, rebuild BIN/nc/<case>, and call the shared control wrapper; the hook case owns no sandbox so parent controls can invoke it on planted copies"

requirements-completed: [VERIF-07, VERIF-02]

duration: 4 min
completed: 2026-11-10
status: complete
---

# Phase 1 Plan 03: Repository Hygiene Gate Summary

**`scripts/hygiene.sh` now fails the verify command on credential shapes, non-noreply addresses, and absolute home paths (never echoing the match), proven by 16 hygiene tests with fragment-built planted controls; a compile-free `hygiene` workflow (`inventory: expected 16, ran 16`) is ready for the always-on CI job, and an opt-in staged hook is available (`inventory: expected 39, ran 39` in the ci lane).**

## Performance

- **Duration:** 4 min
- **Started:** 2026-11-10T00:52Z
- **Completed:** 2026-11-10T00:56Z (before the SUMMARY and state commits)
- **Tasks:** 2
- **Files:** 5 created, 3 modified

## Accomplishments

- `scripts/hygiene.sh --tree` passes the real tree including all of `.planning/` (`hygiene: ok (54 files)`); an untracked file whose name contains a newline and a planted home path is reported with exit 1.
- `--dir <path>` is the git-free mode the controls use; an unknown flag exits 2 with `hygiene: usage`; `--dir scripts` exits 0 so the scanner does not match itself.
- `--staged` reads staged blobs (a planted path is still found after the working copy is cleaned) and fails with `hygiene: identity: author` or `committer` for an address outside `@users.noreply.github.com` / `noreply@github.com`, never printing the address.
- Controls: `hygiene.nc.{credential,private_key,email,home_path,home_path_encoded,newline_name,staged_home_path,staged_identity}` plus `hygiene.pos.{clean,staged_clean}`; the credential control plants four shapes (access-key id, GitHub token, Slack token, key assignment) and requires four findings; the home-path control plants macOS, Linux and Windows forms and requires three.
- `hygiene.hook` plus `hygiene.nc.hook_{missing,not_executable,no_staged}` keep the hook present, executable, and calling `--staged`, each control on a planted copy.
- `cmake --workflow --preset hygiene` builds no C target (`ninja: no work to do.`), and `ctest --test-dir build/hygiene -N` lists 17 tests (16 hygiene plus `inventory.junit`), none `core.` or `runner.`.

## Task Commits

1. **Task 1: Verify command refuses credentials, non-noreply emails, and home paths, each proven by a planted control** - `579b5fc` (feat)
2. **Task 2: Light hygiene lane, staged mode with identity check, opt-in hook** - `6dde63f` (feat)

**Plan metadata:** committed separately with this SUMMARY.

## Verification Evidence

- Plan verification, from clean `build/ci` and `build/hygiene`: `sh scripts/hygiene.sh --tree` exits 0; `cmake --workflow --preset hygiene` exits 0 with `inventory: expected 16, ran 16, skipped 0 (allowed 0)`; `cmake --workflow --preset ci` exits 0 with `inventory: expected 39, ran 39, skipped 0 (allowed 0)`; `ctest -N` on the ci lane reports 40 tests. PASS.
- Task 1 automated verify: all of its commands exit 0 and `-N -R '^hygiene\.'` reports `Total Tests: 9` at that commit. PASS.
- Task 1 acceptance: `--bogus` exits 2 with `hygiene: usage`; `--dir scripts` exits 0; `git grep` for the GitHub-token and access-key shapes prints nothing; `-V` on `hygiene.nc.credential` shows four `hygiene: credential:` findings and no token body. PASS.
- Task 2 acceptance: `ctest --test-dir build/hygiene -N` lists 17 tests with no `core.`/`runner.`; `git ls-files -s .githooks/pre-commit` shows `100755`; `git config --get core.hooksPath` prints nothing; `hygiene.nc.staged_identity -V` shows `hygiene: identity: author`; `hygiene.nc.hook_ -V` shows `expect-fail: ok:` three times with `hygiene: hook-missing`, `hygiene: hook-not-executable`, `hygiene: hook-no-staged`. PASS.
- RED before GREEN: with the controls and inventory registered and no script, all 9 tests failed; they passed once `scripts/hygiene.sh` existed.
- Mutations (each reverted afterwards, the script restored byte-identical):
  - Letting any `/Users/` or `/home/` path count as allowed fails `hygiene.nc.home_path` and `hygiene.nc.newline_name`.
  - Disabling the key-assignment pattern fails `hygiene.nc.credential`.
  - Removing the identity finding fails `hygiene.nc.staged_identity`.
  - Reading the working file instead of the staged blob fails `hygiene.nc.staged_home_path`.
  - Removing the hook's execute bit fails `hygiene.hook`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] Multi-shape controls count their findings**
- **Found during:** Task 1
- **Issue:** The plan has one `credential` file with four shapes and one `home_path` input, but `gbl_expect_fail` accepts a single regex, so a scanner that caught only the first shape would still pass the control.
- **Fix:** `expect_findings` in `tests/gates/hygiene_case.cmake` re-runs the scan and requires one finding per planted line (4 credential shapes, 3 home-path forms: macOS, Linux, Windows).
- **Files modified:** tests/gates/hygiene_case.cmake
- **Commit:** 579b5fc

**2. [Rule 2 - Missing critical functionality] Staged blob and identity-environment hardening**
- **Found during:** Task 2
- **Issue:** A staged control that cleans the working copy is the only proof that `--staged` reads the index, and a sandbox repository inherits `GIT_*` variables when run inside a hook or a CI job, which could supply a different identity or redirect the repository.
- **Fix:** `staged_home_path` stages the planted path and then overwrites the working file with clean text; every staged case runs under `cmake -E env --unset=GIT_DIR ...` (work tree, index, author and committer variables).
- **Files modified:** tests/gates/hygiene_case.cmake
- **Commit:** 6dde63f

### Process notes (no code deviation)

**3. File lists use `find -print0` rather than `find -exec sh -c ... {} +`.** All three modes share one NUL-delimited list file and `xargs -0`, which keeps the file count available for `hygiene: ok (<N> files)` and keeps one worker for every mode. Behavior on newline and leading-dash names is the same and is covered by `hygiene.nc.newline_name`.

**4. TDD by registered controls.** Both tasks are `tdd="true"`; the controls are the tests and ship in the same task commit as the code. A real RED state was observed (9 of 9 failing without the script) and the mutations above show each control fails when its detection is removed. No separate RED commit, matching plans 01 and 02.

**5. CMakeLists.txt needed no change in Task 2.** The lane include and the existing core/runner lane guard from Tasks 1 and plan 01 already covered the hygiene lane; the build step reports no work.

**6. Commit guard on `main`.** `main` is flagged protected by the guard; the orchestrator pinned this plan to local `main` (`branching_strategy: none`), so commits were made there as instructed. Nothing was pushed, the hook was not activated, and no config changed.

**Total deviations:** 2 auto-fixed (both Rule 2), 4 process notes. **Impact:** none on scope; both fixes make controls stricter.

## Auth Gates

None.

## Known Stubs

None.

## Threat Flags

None. The plan's threat register is covered: T-01-04 (`hygiene.tree` in the ci and hygiene lanes, path:line output only), T-01-05 (no realistic literal committed, confirmed by `git grep`), T-01-06 (NUL-safe iteration, `--` before paths, `hygiene.nc.newline_name`), T-01-07 accepted (hook is a convenience, `hygiene.hook` and its three controls keep it wired). No registry packages were installed.

## Issues Encountered

- Finding paths in `--dir` mode include the sandbox directory and appear in the test log under the build tree; nothing from those logs is tracked.
- A newline in a file name prints across two output lines; the reason-code prefix is still on the first.

## Next Phase Readiness

Ready for 01-04. The `hygiene` workflow preset and `tests/inventory/hygiene.txt` exist for the always-on CI job (`cmake --workflow --preset hygiene`). `tests/gates.cmake` is the registration point the signature gate (plan 05 or later) extends, and `tests/inventory/asan.txt` still does not exist. Enabling the hook stays the owner's choice (`git config core.hooksPath .githooks`).

## Self-Check: PASSED

- All files under key-files exist on disk (`scripts/hygiene.sh`, `tests/gates.cmake`, `tests/gates/hygiene_case.cmake`, `tests/inventory/hygiene.txt`, `.githooks/pre-commit`, and the three modified files).
- Commits `579b5fc` and `6dde63f` are ancestors of HEAD; `git rev-list --count` from the recorded base measures 2 commits.
