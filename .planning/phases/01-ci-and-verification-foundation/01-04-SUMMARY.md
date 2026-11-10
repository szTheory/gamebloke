---
phase: 01-ci-and-verification-foundation
plan: 04
subsystem: infra
tags: [signature-scan, rights, posix-sh, cmake, ctest, negative-controls, sha256, digest-only]

requires:
  - phase: 01-ci-and-verification-foundation
    provides: "gbl_add_test with SKIPPABLE, inventory compare and junit check, gbl_expect_fail / gbl_expect_pass, tests/gates.cmake registration point, hygiene lane (plans 01 to 03)"
provides:
  - "scripts/signature-scan.sh --tree | --dir | --extra-digests | --allowlist | --print-config: refuses a logo-window digest match, a BIOS-digest match, ROM/save file names, and unlisted binaries, from committed SHA-256 digests only"
  - "tests/signatures/{logo,bios}.sha256 and allowlist.txt: digest-only lists; the logo row is the visible owner-pending marker"
  - "cmake/CheckSignatureDigests.cmake: wellformed (six reasons), real-logo, and owner-local modes"
  - "scripts/signature-digest.sh: owner helper that prints only a digest of a logo window or a BIOS-sized file"
  - "23 signature.* tests in the ci and hygiene lanes: 15 controls, 4 positives, 4 gates, 2 of them visible SKIPPED states with reasons"
affects: [01-05, 01-06, 01-07, 01-08, 01-09, 01-10, 01-11]

actuals:
  tokens: 9500
  tasks: 2
  commits: 2
plan_head_before: 0dd420f6f864733ec79ad084a1ceeeca4b42aa43
plan_head_after: 718d58a22b66b000abcbfa4feed951f9e2810775

tech-stack:
  added: []
  patterns:
    - "Window bytes 4..159 come from `dd bs=4 skip=1 count=39` and are hashed with `cmake -E sha256sum` in batches; never file(READ HEX) with string(SHA256)"
    - "Hashing CMake is the CMAKE environment variable (tests pass ${CMAKE_COMMAND}), so the 3.25 floor run uses the same binary"
    - "Scan internals live in scripts/lib/signature-common.sh and run inside `xargs -0 sh -c '. lib; fn \"$@\"'` workers, so names with newlines or leading dashes are scanned"
    - "Findings print `signature: <code>: <path>`; only paths and digests are ever printed"
    - "Well-formedness controls mutate a sandbox COPY of the scripts and lists with anchored edits that fail `mutate: no-match` when stale; the unmutated copy has its own positive"
    - "Logo list rewrites in controls rebuild the header from comment lines and write the row in the case, so they stay valid after plan 11 replaces the owner-pending row"

key-files:
  created:
    - scripts/signature-scan.sh
    - scripts/signature-digest.sh
    - scripts/lib/signature-common.sh
    - tests/signatures/logo.sha256
    - tests/signatures/bios.sha256
    - tests/signatures/allowlist.txt
    - cmake/CheckSignatureDigests.cmake
    - tests/gates/signature_case.cmake
  modified:
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt

key-decisions:
  - "The logo-window rule applies to every scanned regular file of at least 160 bytes, which is stricter than D-07's wording (every binary file): a crafted file with no NUL bytes would otherwise evade it, and the cost is one dd per file"
  - "The allowlist exempts only the rom-extension and binary-not-allowlisted rules; the logo and BIOS controls allowlist their planted file and must still fail"
  - "An absent digest or allowlist file fails the scan closed (`signature: missing-list`), never reads as empty"
  - "Until plan 11 supplies the real logo digest the row is `owner-pending`: signature.logo.real_digest and signature.owner_local report SKIPPED with a reason, never PASS"

patterns-established:
  - "Case drivers that need a source layout copy scripts/ and tests/signatures/ into BIN/nc/<case>/src and run the real checker against it"

requirements-completed: [EVID-04, VERIF-02, VERIF-03]

duration: 5 min
completed: 2026-11-10
status: complete
---

# Phase 1 Plan 04: Rights Signature Scan Summary

**`scripts/signature-scan.sh` fails the verify command on a listed logo-window or BIOS digest, a ROM/save file name, or an unlisted binary, using committed digests only, with 19 synthesized-stand-in controls and a well-formedness check; the real logo digest is visibly owner-pending (`inventory: expected 62, ran 60, skipped 2 (allowed 2)` in the ci lane).**

## Performance

- **Duration:** 5 min
- **Started:** 2026-11-10T00:59Z
- **Completed:** 2026-11-10T01:05Z (before the SUMMARY and state commits)
- **Tasks:** 2
- **Files:** 8 created, 3 modified

## Accomplishments

- `sh scripts/signature-scan.sh --tree` exits 0 on the real tree (`signature: ok (61 files)` at Task 1) and `signature.tree` takes about 0.9 s of wall time in ctest, well under the 10 s expectation.
- The logo rule hashes bytes 4..159 of every regular file of at least 160 bytes in one batched `cmake -E sha256sum`; the BIOS rule hashes every file of exactly 16384 bytes. The logo control also checks that the owner helper's digest equals an independent one-byte-at-a-time `dd bs=1 skip=4 count=156` digest, so a shifted or truncated window cannot pass.
- Rules 3 and 4 (extension, binary) honour the allowlist; rules 1 and 2 never do. Controls `signature.nc.logo` and `signature.nc.bios` allowlist their planted file and still fail with `signature: logo-window` / `signature: bios-digest`. A zero-byte file counts as text.
- `signature.digests.wellformed` passes on the committed lists and has six controls, one per failure reason (`missing-list`, `bad-bios-row`, `no-gba-bios`, `logo-row-count`, `bad-logo-row`, `wrong-default-path`) on a mutated sandbox copy, plus `signature.pos.digests_copy` proving the unmutated copy passes. The six `expect-fail: ok: /signature-digests: ` lines were counted in the log.
- `signature.nc.owner_file_mismatch` and `signature.pos.owner_file_match` prove the owner-local check fails on a stand-in digest that is not the planted file's, and passes when the row equals the helper's digest.
- The two visible skips appear in the post-step listing: `SKIPPED signature.logo.real_digest: ... | logo digest owner-pending (tests/signatures/logo.sha256; owner action in plan 11)` and `SKIPPED signature.owner_local: ... | GBL_OWNER_LOGO_FILE not set`.
- Stand-in logo and BIOS blobs are synthesized from `/dev/urandom` and `/dev/zero` under `build/ci/nc/` and registered only through `--extra-digests`. No real logo, BIOS, or ROM byte was written, downloaded, copied, hashed, or printed, and no owner-supplied file was opened. `git status --porcelain` is clean after the runs.

## Task Commits

1. **Task 1: Verify command scans the tree for logo, BIOS, ROM/save, and unlisted binaries against committed digests only** - `73aaf0d` (feat)
2. **Task 2: Synthesized stand-in controls for every rule and digest-list reason, plus the owner digest helper** - `718d58a` (feat)

**Plan metadata:** committed separately with this SUMMARY.

## Verification Evidence

- Plan verification from clean `build/ci` and `build/hygiene`: `sh scripts/signature-scan.sh --tree` exits 0; `cmake --workflow --preset ci` exits 0 with `inventory: expected 62, ran 60, skipped 2 (allowed 2)` and both SKIPPED lines; `cmake --workflow --preset hygiene` exits 0 with `inventory: expected 39, ran 37, skipped 2 (allowed 2)`. PASS.
- Task 1 automated verify and acceptance: all commands exit 0; `--print-config` prints three paths ending in the three list names; `grep -c '^owner-pending '` prints 1; `grep -c 'file(READ'` is 0 for both files; `grep -c '^GBL_LOGO_LIST_DEFAULT='` prints 1; the line directly above the `ds-gba-mode-bios` row contains `SINGLE-SOURCE` and `LOW CONFIDENCE`. PASS.
- Task 2 acceptance: `-R '^signature\.(nc|pos)\.'` runs 19 tests, all pass; `signature-digest.sh` with no arguments exits 2 with `signature-digest: usage`; 6 well-formedness `expect-fail: ok` lines; `signature-digests: owner-file-mismatch` and `owner-file-match` both in the output; `signature: logo-window:` appears and no 312-hex-character line does; no new tracked-tree file outside the committed set. PASS.
- RED before GREEN: with the four Task 1 tests registered and no scripts, all 4 failed; they passed once the scan, lists, and checker existed.
- Mutations (each reverted afterwards, the files restored byte-identical, `git diff` empty):
  - Letting the allowlist skip the logo window fails `signature.nc.logo`.
  - Letting the allowlist skip the BIOS hash fails `signature.nc.bios`.
  - Dropping the zero-byte-file guard fails `signature.pos.binary_allowlisted`.
  - Shortening the window to 38 blocks passes the scan controls (the scan and helper share the function) but fails `signature.nc.logo` through its independent window digest.
- Manual probe: an untracked `*.rom` file in the working tree made `--tree` exit 1 with `signature: rom-extension`, then was removed.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] Independent window check in the logo control**
- **Found during:** Task 2 (mutation test)
- **Issue:** The scan and the owner helper share `gbl_window_file`, so a wrong window offset or length is self-consistent and would pass every control, while the committed digest the owner supplies later depends on the window being exactly bytes 4..159.
- **Fix:** `signature.nc.logo` also hashes `dd bs=1 skip=4 count=156` of the planted file and requires it to equal the helper's digest, and asserts the scan output holds no 312-hex-character dump.
- **Files modified:** tests/gates/signature_case.cmake
- **Commit:** 718d58a

**2. [Rule 2 - Missing critical functionality] Fail-closed list handling and stricter finding detail**
- **Found during:** Task 1
- **Issue:** The plan lists `bad-digest-row` and `usage` as scan reason codes but does not say what an absent list or allowlist does; reading it as empty would remove protection silently.
- **Fix:** an absent logo list, BIOS list, allowlist, or extra-digests file prints `signature: missing-list: <path>` and exits 1. The logo and BIOS controls match the planted file name (`standin\.dat`), not just the reason code.
- **Files modified:** scripts/signature-scan.sh, tests/gates/signature_case.cmake
- **Commit:** 73aaf0d, 718d58a

### Process notes (no code deviation)

**3. D-07 wording.** CONTEXT.md D-07 says the logo window is hashed for "every binary file"; this plan and the code hash it for every scanned regular file of at least 160 bytes, which is stricter, so a crafted file without NUL bytes cannot evade the rule (T-01-12).

**4. Ignored files are not scanned.** `--tree` lists tracked plus untracked-not-ignored files, as the plan says, so a file ignored by `.gitignore` (for example `*.sav`, `*.gba`) is not scanned. Those cannot be committed without `git add -f`; the scan covers what could enter the repository.

**5. TDD by registered controls.** Both tasks are `tdd="true"`; the tests ship in the same task commit as the code, as in plans 01 to 03. A real RED state was observed for Task 1 (4 of 4 failing) and the mutations above show each control fails when its detection is removed. Task 2's 19 controls passed on first run because the scan they exercise was complete, and the mutations are the evidence they can fail.

**6. Commit guard on `main`.** `main` is flagged protected by the guard; the orchestrator pinned plans 01-01 to 01-08 to local `main` (`branching_strategy: none`), so commits were made there as instructed. Nothing was pushed and no config changed.

**Total deviations:** 2 auto-fixed (both Rule 2), 4 process notes. **Impact:** none on scope; both fixes make controls stricter.

## Auth Gates

None.

## Known Stubs

None. The `owner-pending` logo row is a deliberate, visible placeholder (not a stub in code): plan 11 replaces it with the owner-supplied digest, and until then `signature.logo.real_digest` reports SKIPPED.

## Threat Flags

None. T-01-08 (`signature.tree` in the ci and hygiene lanes with all four rules and controls), T-01-09 (allowlist never exempts digest rules, proved by the logo and BIOS controls), T-01-10 (six well-formedness controls, positive copy, owner-pending shown as SKIPPED, owner-local mismatch and match), T-01-11 (paths and digests only, no window dump), and T-01-12 (window hashed for every file of at least 160 bytes) are mitigated. No registry packages were installed.

## Issues Encountered

- `bad-digest-row` findings print the digest list's path, which in a control is under the build tree; nothing from those logs is tracked.
- The DS-family BIOS row (`ds-gba-mode-bios`) is single-source and low confidence (A2); the row says so, and the extension and binary rules remain the backstop.

## Next Phase Readiness

Ready for 01-05. Outstanding owner action: plan 11 must replace the `owner-pending nintendo-logo-window` row in `tests/signatures/logo.sha256` with the line printed by `scripts/signature-digest.sh logo <file>` run on a file the owner already holds, outside the repository, or record it as a human-needed gap. Until then `signature.logo.real_digest` stays SKIPPED, and `GBL_OWNER_LOGO_FILE` enables the owner-local cross-check once a real row exists.

## Self-Check: PASSED

- All files under key-files exist on disk (the eight created files and the three modified files).
- Commits `73aaf0d` and `718d58a` are ancestors of HEAD; `git rev-list --count` from the recorded base measures 2 commits.
