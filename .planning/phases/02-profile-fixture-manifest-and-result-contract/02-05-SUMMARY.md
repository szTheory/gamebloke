---
phase: 02-profile-fixture-manifest-and-result-contract
plan: 05
subsystem: testing
tags: [posix-sh, curl, sha256, pinned-fetch, verify-before-place, negative-controls, gitignore, linux-lane]

requires:
  - phase: 02-profile-fixture-manifest-and-result-contract
    provides: pins.txt format and linter, mutation helpers (gbl_text_mutate), ExpectFail wrappers (02-01, 02-04); Phase 1 trap idiom enforced by scripts/check-traps.sh
provides:
  - scripts/fetch-pin.sh, a POSIX pinned fetch that places a file only after its SHA-256 matches the pins.txt row
  - cmake/CheckFixtureDir.cmake, the gate that .fixtures/ is ignored and nothing under it is tracked, registered as fetch.untracked in the canonical command
  - tests/gates/fetch_case.cmake driver with 19 registered tests (file:// only, no network)
  - /.fixtures/ in .gitignore and curl in the scripts/linux-lane.sh apt line (R-5)
affects: [02-07, phase-05-fixture-rows, phase-06-fixture-rows]

requirements-completed: [EVID-03]

actuals:
  tokens: 13300
  tasks: 3
  commits: 3
plan_head_before: 719236c6ed45a5a9369085f95ab570ffcf37bbd2
plan_head_after: bd8e6180193a0def642625fb3eebe4299ad9221a

tech-stack:
  added: []
  patterns:
    - "Verify-before-place: temp directory inside the destination root, hash, then mv; EXIT removes the temp directory and HUP/INT/TERM each exit non-zero (Phase 1 idiom)"
    - "Row text is only a quoted argument or a case pattern, never evaluated; curl is limited to the row's single scheme for the request and for redirects"
    - "Sandbox-per-variant controls: new_sandbox() resets pins and root, each variant mutates the sandbox pins with gbl_text_mutate and asserts the reason text plus nothing placed"

key-files:
  created:
    - scripts/fetch-pin.sh
    - cmake/CheckFixtureDir.cmake
    - tests/gates/fetch_case.cmake
  modified:
    - .gitignore
    - scripts/linux-lane.sh
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt

key-decisions:
  - "The curl program is ${CURL:-curl}, the same variable the presence check tests, so the no_curl control names an absent program without touching PATH."
  - "Controls assert a non-zero exit plus reason text and never an exact exit status (W4); the script still exits 2 for usage, missing-pins, and no-curl as its header documents."
  - "The fixture-dir gate counts ls-files output lines instead of using -z (CMake variables cannot hold NUL separators); it prints only the count, never a path."
  - "not_git sets GIT_CEILING_DIRECTORIES to the parent of the sandbox directory; the control was shown to fail (gate PASSED) when the ceiling is removed."

patterns-established:
  - "Every fetch control runs `cmake -E env GBL_FETCH_ALLOW_FILE=1 CMAKE=<cmake> sh scripts/fetch-pin.sh --pins <sandbox> --root <sandbox> <dest>` against a synthesized payload; no https row exists in any test sandbox"

duration: 7min
completed: 2026-11-10
status: complete

coverage:
  - id: D1
    description: "fetch-pin.sh downloads a pinned file to a temporary directory in the root, hashes it with cmake -E sha256sum, and places it only on a digest match; a mismatch exits non-zero with `fetch-pin: digest-mismatch`, places nothing, and leaves no temporary directory"
    requirement: "EVID-03"
    verification:
      - kind: unit
        ref: "ctest -R '^fetch\\.(pos\\.fetch_ok|nc\\.digest_mismatch)$' (ci, hygiene, and the Linux container)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Every other fetch-pin reason code (usage, missing-pins, no-row, bad-row, bad-digest, bad-url, bad-dest, no-curl, download-failed, symlink, dest-is-dir) has a control that fails for exactly its reason with nothing placed; re-fetching the same pin is idempotent"
    requirement: "EVID-03"
    verification:
      - kind: unit
        ref: "ctest -R '^fetch\\.(nc\\.|pos\\.idempotent)' (14 fetch tests after Task 2)"
        status: pass
    human_judgment: false
  - id: D3
    description: ".fixtures/ is ignored and untracked, enforced by the fetch.untracked gate in the canonical command with controls for not-ignored, tracked, git-failed, and usage"
    requirement: "EVID-03"
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci (inventory: expected 225, ran 223, skipped 2 (allowed 2)); fetch.untracked Passed"
        status: pass
    human_judgment: false
  - id: D4
    description: "curl is installed in the Linux-lane container so the file:// controls run there"
    requirement: "EVID-03"
    verification:
      - kind: integration
        ref: "sh scripts/linux-lane.sh ci gcc (aarch64 container, gcc): 225 tests, fetch.* all Passed, exit 0"
        status: pass
    human_judgment: false
  - id: D5
    description: "curl is present on the GitHub ubuntu-26.04 runner (RESEARCH A1)"
    requirement: "EVID-03"
    verification: []
    human_judgment: true
    rationale: "Not checkable locally: the runner image list for 26.04 was never read. Pending; owner is the orchestrator, checked on the phase PR's first CI run (I4)."
---

# Phase 2 Plan 05: Pinned fetch Summary

**A POSIX `scripts/fetch-pin.sh` that downloads by pins.txt row into a temporary directory inside `.fixtures/`, hashes with `cmake -E sha256sum`, and moves the file into place only when the SHA-256 matches, with a control for every reason code through synthesized `file://` fixtures, `.fixtures/` ignored, and a gate proving nothing under it is tracked.**

## Performance

- **Duration:** about 7 min
- **Completed:** 2026-11-10
- **Tasks:** 3 (one tracer, two TDD-style control tasks)
- **Files:** 8 (3 created, 5 edited)

## Accomplishments

- `scripts/fetch-pin.sh [--pins <file>] [--root <dir>] <dest>` follows the RESEARCH prototype: allow-listed `<dest>` (charset, no empty, `.`, or `..` part, no leading slash), strict three-field rows with duplicate detection, 64 lowercase hex digest, https only (`file://` only when `GBL_FETCH_ALLOW_FILE` is exactly 1), `${CURL:-curl}` with `--fail --silent --show-error --globoff --proto/--proto-redir =<scheme> --max-redirs 5 --connect-timeout 20 --max-time 600 --max-filesize 268435456 --url`, `pwd -P` containment of the destination parent, refusal of a symlink or directory at the destination, then `mv -f` and `fetch-pin: ok: <dest>`. `set -eu`, `set -f`, `eval`, `--insecure`, and a combined EXIT/signal trap all stay out (grep counts 0); the four-line trap idiom is present once. shellcheck `-s sh` is clean and `sh scripts/check-traps.sh` prints `traps: ok (17 files)`.
- `.gitignore` gains a commented `/.fixtures/`; `git check-ignore -q .fixtures/probe.dat` exits 0; `git ls-files .fixtures` printed nothing; no `.fixtures` directory exists at the repository root after the tests.
- `cmake/CheckFixtureDir.cmake` reports `fixture-dir: usage|git-failed|not-ignored|tracked` and `fixture-dir: ok`; `cmake -DROOT=. -P cmake/CheckFixtureDir.cmake` prints `fixture-dir: ok`. It runs as `fetch.untracked` inside `cmake --workflow --preset ci`.
- `scripts/linux-lane.sh` installs `curl`. `dash -n` and shellcheck pass on it, and `sh scripts/linux-lane.sh ci gcc` ran the full ci workflow in the digest-pinned ubuntu:26.04 container (aarch64, gcc) with all fetch tests passing and exit 0. That is a native-arch container proof, not the x86_64 runner.

## Tests added (19)

fetch.nc.{bad_dest, bad_digest, bad_row, bad_url, dest_is_dir, digest_mismatch, download_failed, missing_pins, no_curl, no_row, not_git, not_ignored, symlink, tracked, untracked_usage, usage}; fetch.pos.{fetch_ok, idempotent}; fetch.untracked.

## Final lane counts

- ci: `inventory: expected 225, ran 223, skipped 2 (allowed 2)`
- hygiene: `inventory: expected 155, ran 153, skipped 2 (allowed 2)`
- asan: unchanged (`tests/inventory/asan.txt` untouched)
- `grep -c '^fetch\.' tests/inventory/hygiene.txt` is 19; `ctest --test-dir build/hygiene -N -R '^fetch\.'` reports `Total Tests: 19`.
- Per-task `ctest -N` totals matched the plan: ci 209 after Task 1 and 221 after Task 2 (the final ci total is 226 tests, which is `expected 225` plus the one test the inventory does not count, as in 02-04). The hygiene fetch filter reported 2, 14, then 19.
- `json-real: 7 documents` is unchanged; `sh scripts/hygiene.sh --tree` printed `hygiene: ok (136 files)` and `signature-scan.sh --tree` printed `signature: ok (136 files)` before this SUMMARY was added.

## Control proofs (scratch copies, not committed)

- Tracer: dropping the digest comparison from a scratch copy of the script made `fetch.nc.digest_mismatch` fail with `expect-fail: gate PASSED but must fail (negative control broken)`; replacing the EXIT cleanup with a no-op made it fail with `temporary directory left behind` and made `fetch.pos.fetch_ok` fail the same way.
- Task 2: for 17 scratch weakenings (one or two per reason code: the component rule and charset in `check_dest`, the 64-length and hex checks, the field-count and duplicate checks, the `file://` allow variable, the unsafe-character case, the scheme default, both symlink checks, the directory check, the curl presence check, the download failure, the no-row and missing-pins checks, and the usage check) the matching control failed every time. No weakening left a control passing.
- Task 3: removing the tracked-count check, the not-ignored branch, or the usage check from a scratch `CheckFixtureDir.cmake` failed `fetch.nc.tracked`, `fetch.nc.not_ignored`, and `fetch.nc.untracked_usage`; removing the ceiling from the `not_git` control made it fail (the build tree sits inside the enclosing repository, so git finds it); a scratch clone with the `/.fixtures/` line removed printed `fixture-dir: not-ignored`; a scratch clone with a force-added `.fixtures/` file printed `fixture-dir: tracked: 1 paths`.
- Log acceptance: `fetch.nc.bad_url` carries four `expect-fail: ok: /fetch-pin: bad-url/` lines (http, ftp, `file://` without `GBL_FETCH_ALLOW_FILE`, and the double-quote URL); `fetch.nc.bad_dest` carries four `bad-dest` lines. `grep -E -c 'EXIT[[:space:]]+[0-9]' tests/gates/fetch_case.cmake` prints 0.
- Dash observation (Pitfall 13): `dash scripts/fetch-pin.sh` with the driver's own arguments against the driver's sandboxes printed `fetch-pin: ok: example/payload.bin` and exit 0 for the fetch_ok pin, and `fetch-pin: digest-mismatch: example/payload.bin: pinned <digest>, got <digest>` with exit 1 and an empty root for the digest_mismatch pin. `dash scripts/fetch-pin.sh` with no arguments printed `fetch-pin: usage: ...` and exited 2. `sh` (bash 3.2 in POSIX mode) gave the same results.
- Floor spot check: all 18 distinct `fetch_case.cmake` cases (every control and both positives, including the sandbox-git ones) and the real-repository `CheckFixtureDir.cmake` run pass under the Kitware 3.25.3 binary (digest verified earlier). Plan 07 owns the full floor workflow run.

## Task Commits

1. **Task 1: Tracer: fetch-pin.sh places a verified file:// fixture and a mismatch places nothing** - `0069f38` (feat)
2. **Task 2: Fetch controls for every remaining reason code and an idempotent re-fetch positive** - `4318cf5` (test)
3. **Task 3: The untracked-fixtures gate with its controls, then the full workflows** - `bd8e618` (feat)

**Plan metadata:** committed with this SUMMARY (docs).

## Decisions Made

See key-decisions above. Tracer gate: the Task 1 verify command (2 fetch tests, ci 209, check-traps green, `/.fixtures/` ignored, curl in the apt line) was green before any expansion task started.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocker] `git ls-files -z` replaced by a line count in CheckFixtureDir.cmake**
- **Found during:** Task 3
- **Issue:** the plan calls for `git ls-files -z`, but a CMake variable cannot hold the NUL separators, so the output could not be counted reliably.
- **Fix:** run `git -c core.quotepath=off ls-files -- .fixtures` and count output lines; the gate prints only `tracked: <n> paths`, never a name, as the plan requires. A forced-add control proves it.
- **Files modified:** cmake/CheckFixtureDir.cmake
- **Commit:** bd8e618

**2. [Rule 2 - Missing critical] Unreadable final line and unusable parent handled in fetch-pin.sh**
- **Found during:** Task 1
- **Issue:** the plan's row loop would drop a last pins row that has no trailing newline, and an unusable destination parent (for example a dangling symlink) would abort under `set -e` with no reason code.
- **Fix:** the read loop also accepts a final unterminated line, and a failing `mkdir -p` of the parent reports `fetch-pin: bad-dest: parent not creatable: <dest>`. No new reason code was introduced.
- **Files modified:** scripts/fetch-pin.sh
- **Commit:** 0069f38

**Total deviations:** 2 auto-fixed (1 blocker, 1 missing critical). **Impact:** none on planned behavior or counts. Extra control variants (three usage shapes inside `fetch.nc.usage`, two shapes each inside `bad_row`, `bad_digest`, `symlink`, four inside `bad_dest` and `bad_url`) sit inside the stated reason codes.

## Issues Encountered

None. A first scratch run of the `not_git` revert used a ceiling equal to the sandbox directory and an out-of-repository build directory, which could not distinguish the control; it was redone with the build directory inside the repository and the ceiling removed, which did.

## Known Stubs

None. `pins.txt` still has no rows by design (Phases 5 and 6 add the first fixtures); no third-party bytes were fetched this phase.

## Threat Flags

None. No new network endpoint, auth path, or trust-boundary access beyond the plan's threat model. T-02-17 (traversal) is covered by the bad_dest, symlink, and dest_is_dir controls; T-02-18 (option injection) by the four-shape bad_url control and `--url`; T-02-19 (tampered download) by digest_mismatch with nothing placed; T-02-20 (scheme downgrade) by the single-scheme `--proto` and the `file://` refusal control; T-02-21 by the temp directory inside the root, the trap idiom, and check-traps; T-02-22 by the curl limits; T-02-23 by `/.fixtures/` plus fetch.untracked and its controls; T-02-24 because every control uses `file://` under the build directory; T-02-SC-05 because curl is installed from the Ubuntu archive inside the pinned image.

## Pending items

- **RESEARCH A1 is pending.** Whether `curl` is present on the GitHub ubuntu-26.04 runner image is unverified; the 26.04 image list was never read. Owner: the orchestrator, checked on the phase PR's first CI run (I4). The script exits 2 with `fetch-pin: no-curl` if it is absent, so a gap fails loudly rather than skipping.
- The Linux-container proof above is aarch64; the x86_64 Linux proof is the ubuntu-26.04 CI runner.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for 02-06 (result validator). Plan 07 adds the contract guide and verify-guide reason codes for `fetch-pin` and `fixture-dir`, the CMake 3.25.3 floor workflow run, and the Linux lane evidence.

## Self-Check: PASSED

scripts/fetch-pin.sh, cmake/CheckFixtureDir.cmake, and tests/gates/fetch_case.cmake exist; commits 0069f38, 4318cf5, and bd8e618 are ancestors of HEAD; `git rev-list --count` from the recorded base measured 3; ci 225/223/2 and hygiene 155/153/2 and both tree scans passed; acceptance criteria for all three tasks re-ran green.
