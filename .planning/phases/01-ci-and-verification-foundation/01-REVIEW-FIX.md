---
phase: 01-ci-and-verification-foundation
fixed_at: 2026-11-09T00:00:00Z
review_path: .planning/phases/01-ci-and-verification-foundation/01-REVIEW.md
iteration: 1
findings_in_scope: 9
fixed: 9
skipped: 0
open: 0
failed: 0
status: all_fixed
---

# Phase 1: Code Review Fix Report

**Fixed at:** 2026-11-09
**Source review:** `.planning/phases/01-ci-and-verification-foundation/01-REVIEW.md`
**Iteration:** 1
**Scope:** Critical and Warning (CR-01, WR-01 to WR-08). Info findings (IN-01 to IN-06) were not touched.

**Summary:**
- Findings in scope: 9
- Fixed: 9
- Skipped: 0
- Failed: 0
- Open: 0

## Per-finding results

| Finding | Status | Commit | Control(s) added | Notes |
| ------- | ------ | ------ | ---------------- | ----- |
| CR-01 | fixed (requires human verification) | `0e80632` | `hygiene.signal`, `signature.signal` (signal probe, TERM and HUP, with an unsignalled baseline); `traps.static` gate (`scripts/check-traps.sh`); `traps.nc.exit_idiom`, `traps.nc.no_handler`, `traps.nc.no_files`, `traps.pos.clean`; `traps.nc.resumes` (pre-fix idiom put back, probe must fail) | All four scripts now trap `EXIT` for cleanup and give `HUP`/`INT`/`TERM` their own `exit 129/130/143` handlers. A deterministic signal test is feasible for TERM and HUP only: a background job of a non-interactive shell starts with SIGINT ignored, so INT cannot be trapped there. The INT handler (and `signature-digest.sh` and `apply-repo-settings.sh`, which the probe does not run) is covered by the static gate, which also fails if the `trap ... EXIT HUP INT TERM` idiom comes back. |
| WR-01 | fixed | `327314e` (shared with WR-02, WR-03) | `ci.yml.nc.extra_file`, `ci.yml.nc.extra_file_pins`, `ci.yml.nc.no_ci_yml`, `ci.yml.pos.all_files`, `ci.workflow.coverage` | `ci.actions.pinned` and `ci.yml.policy` now run with no file argument, so every `.github/workflows/*.yml` and `*.yaml` is checked. General rules apply to every file. The required-check rules (`required-check`, `needs-missing`, heavy-job preset-only) apply to `ci.yml`, and `missing-ci-yml` fires if it is absent. Findings now end with the file name in brackets. `docs/verify.md` states what runs. `ci.yml.nc.extra_file_pins` guards the pre-existing no-argument mode of `check-action-pins.sh` and passes against the old script; the wiring defect itself is guarded by `ci.workflow.coverage`. |
| WR-02 | fixed (requires human verification) | `327314e` | `ci.yml.nc.job_permissions`, `ci.yml.nc.job_permissions_inline` | Job-level `permissions` is parsed. Anything other than `contents: read` and `pull-requests: read` (or `{}`) reports `workflow: job-permissions: <job>`. The real `ci.yml` keeps passing (its `changes` and `title` jobs use exactly those two). |
| WR-03 | fixed (requires human verification) | `327314e` | `ci.yml.nc.run_by_path`, `ci.yml.nc.run_script` (the existing `non_preset_run` control still applies) | Denylist replaced by an allowlist for the heavy jobs (`build`, `asan`, `hygiene`): only `cmake --workflow --preset <name>` and the `dpkg -s ... apt-get install` guard line are accepted. Any other run line, including a tool called by path, a script, or an appended command, is `non-preset-run`. Consequence for later edits: a new run line in a heavy job needs a matching allowlist change in `scripts/check-workflow.sh`. |
| WR-04 | fixed | `d2dcd56` | `core.nc.quoted`, `core.nc.continuation`, `core.nc.comment`, `core.nc.computed`, `core.nc.empty`, `core.nc.missing`, `core.pos.in_tree` | Quoted includes must resolve under a scanned directory; continuations are joined; one-line block comments are dropped; a non-literal include operand is refused; an absent or empty directory fails with `core: no-files`. Scanned extensions widened to c h inc def cc cpp cxx hpp hh S s. Not done: the review's defense-in-depth suggestions (`-ffreestanding` include restriction, `nm -u` on the archive) and trigraph/digraph spellings of `#include`. |
| WR-05 | fixed (requires human verification) | `26ffacc` | rename-padding case and 8 further table rows in `ci.classify.cases`; marker-contract check against `ci.yml` | The workflow now emits `F <path>` per API entry plus `R <old path>` per rename; the classifier counts `F` lines against `changed_files` and tests every `F` and `R` path for `.planning/`. An unmarked line counts as non-planning, so a stale writer fails closed. The new `gh api --jq` expression was checked against sample JSON with local `jq`, but not against a live GitHub API call; the first pull request run is the real proof. |
| WR-06 | fixed | `02f6b53` | `inventory.nc.label_substring` | `check-junit.sh` anchors the exclude to the whole label (bare `post` and `^post$` behave the same); `GblTests.cmake` and the harness cases pass `^post$`, the text the ctest presets filter on. |
| WR-07 | fixed | `81dd417` | `hygiene.nc.credential_upper` (four shapes, four findings, no value echoed); upper-case short value added to the clean sandbox | The assignment grep is `-i` and the key word may carry a name suffix, so the four shapes the review lists are found. The self-match property holds (`hygiene.tree` stays clean). |
| WR-08 | fixed | `7d9374b` | `nc.meta.pass_requires_exit_status` | `runner.version` and `runner.help` run through `ExpectFail.cmake` in a new `EXPECT=pass` script mode that requires exit 0 and the regex. Verified by a real mutation: a runner returning 7 after `--version` fails `runner.version`. |

Also committed: `05cc8af` (docs only) records the `traps` family, `core: no-files`, and the exit-status rule in `docs/verify.md`.

## Deviation: shared commit for WR-01, WR-02, WR-03

The three findings edit the same awk parser in `scripts/check-workflow.sh` and the same registration block, so they landed as one commit (`327314e`) instead of three. Everything else is one commit per finding.

## Verification

Every gate and control below ran in the main checkout on macOS (AppleClang, Ninja, CMake 4.x) unless stated. The Linux runs used the digest-pinned `ubuntu:26.04` container through `scripts/linux-lane.sh` (native architecture).

| Check | Result |
| ----- | ------ |
| `cmake --workflow --preset ci` | exit 0, `inventory: expected 120, ran 118, skipped 2 (allowed 2)` (was 93/91/2) |
| `cmake --workflow --preset hygiene` | exit 0, `inventory: expected 50, ran 48, skipped 2 (allowed 2)` (was 41/39/2) |
| `sh scripts/linux-lane.sh ci gcc` | exit 0, `inventory: expected 120, ran 118, skipped 2 (allowed 2)` |
| `sh scripts/linux-lane.sh asan` | exit 0, `inventory: expected 16, ran 16, skipped 0 (allowed 0)` (the lane gained the seven new core tests) |
| `sh scripts/hygiene.sh --tree` | `hygiene: ok (97 files)` |
| `sh scripts/signature-scan.sh --tree` | `signature: ok (97 files)` |

Mutation checks (scratch copies or temporary edits, reverted, nothing committed):

- CR-01: the pre-fix trap block in `hygiene.sh`, `signature-scan.sh` and `apply-repo-settings.sh` fails `hygiene.signal`, `signature.signal` (`signal-probe: resumed`), `traps.static` and `traps.nc.resumes` (its anchor is the fixed block, so it reports `mutate: no-match` once the block is gone).
- WR-01/02/03: the original `check-workflow.sh` fails `job_permissions`, `job_permissions_inline`, `run_by_path`, `run_script`, `extra_file`, `no_ci_yml` and the all-files positive; re-adding a file argument to the registration fails `ci.workflow.coverage`.
- WR-04: disabling the quoted check, the continuation join, the comment strip, or the no-files check each fails its own control (and the in-tree positive when it exercises the join or comment path).
- WR-05: counting `R` lines as entries fails three table rows and the rename-padding reason check.
- WR-06: reverting to the unanchored match fails `inventory.nc.label_substring`.
- WR-07: dropping `-i`, or dropping the name suffix, fails `hygiene.nc.credential_upper`.
- WR-08: a runner returning 7 after `--version` fails `runner.version`.

## Not covered

- IN-01 to IN-06 were out of scope and are unchanged.
- The WR-05 workflow change and the WR-01 file-name suffix on findings have not run on GitHub; the first pull request run of `ci.yml` is their live proof.
- Publication (push, pull request) was not done, by instruction.

---

_Fixed: 2026-11-09_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
