---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "01"
subsystem: toolchain
tags: [arm-none-eabi, xPack, archive-validation, CMake]

# Dependency graph
requires:
  - phase: 02
    provides: fixture provenance and validation gates used by the project
  - phase: 04
    provides: canonical runner and CPU verification path retained by CI
provides:
  - Exact xPack GNU Arm Embedded 15.2.1-1.1 asset pins for Darwin arm64 and Linux x64/arm64
  - Bounded HTTPS installer with archive validation, private staging, and atomic promotion
  - Offline refusal matrix covering download, archive, and filesystem postconditions
affects: [phase-05-diagnostic-builds, toolchain-consumers]

# Actuals measured over the realized implementation diff (37,500 chars / 4).
actuals:
  tokens: 9375
  tasks: 2
  commits: 5
commits: 5
plan_head_before: 26081bfae176e16f07be57223782426c358e591a
plan_head_after: d417197cd0fa33e92a9659096c557026ba809064

# Tech tracking
tech-stack:
  added: [xPack GNU Arm Embedded GCC 15.2.1-1.1, curl, tar]
  patterns: [host-bound lock rows, bounded single-redirect download, safe archive extraction, atomic promotion]

key-files:
  created: [toolchain/arm-none-eabi.lock, scripts/fetch-toolchain.sh, tests/gates/toolchain_case.cmake]
  modified: []

key-decisions:
  - "Pin the exact published archive size and SHA-256 independently for Darwin arm64, Linux x64, and Linux arm64."
  - "Permit one HTTPS redirect only to release-assets.githubusercontent.com and never write signed redirect URLs to output or receipts."
  - "Reject a lock row whose archive URL does not match the selected host, before invoking curl."

patterns-established:
  - "Check archive size and digest before extraction; validate every archive member and link before atomic install promotion."
  - "Use fake curl and tar executables to verify refusal reasons and filesystem postconditions without network access."

requirements-completed: []

# Coverage metadata
coverage:
  - id: D1
    description: "Install the exact native xPack toolchain and emit a local identity and notice receipt."
    verification:
      - kind: integration
        ref: "cmake -DCASE=installed -DSRC=. -DBIN=build/phase05-toolchain -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/toolchain_case.cmake"
        status: pass
      - kind: unit
        ref: "cmake -DCASE=lock -DSRC=. -P tests/gates/toolchain_case.cmake"
        status: pass
    human_judgment: false
  - id: D2
    description: "Refuse unsafe host, redirect, payload, archive, and filesystem cases while retaining a clean valid redirect path."
    verification:
      - kind: integration
        ref: "cmake -DCASE=offline-all -DSRC=. -DBIN=build/phase05-toolchain -P tests/gates/toolchain_case.cmake"
        status: pass
    human_judgment: false

# Metrics
duration: 50min
completed: 2026-11-11
status: complete
---

# Phase 05 Plan 01: Verified Toolchain Installer Summary

**Pinned xPack GNU Arm Embedded 15.2.1-1.1 to exact host assets and installed it through bounded download, validated extraction, and atomic promotion.**

## Performance

- **Duration:** 50 minutes
- **Started:** 2026-11-11T16:13:37Z
- **Completed:** 2026-11-11T17:03:51Z
- **Tasks:** 2
- **Files modified:** 3 implementation and test files

## Accomplishments

- Added the exact release, URL, byte count, SHA-256, redirect host, and archive root for Darwin arm64, Linux x64, and Linux arm64.
- Added a host-aware installer that bounds the HTTPS transfer, validates the redirect and archive contents, records notice inventory, and promotes only a verified install.
- Added a network-free 21-case refusal and success matrix with checks for extractor invocation, prior-install preservation, and out-of-root writes.
- Installed and executed the native Darwin arm64 assembler and readelf from the pinned artifact.

The actual local receipt records xPack release `15.2.1-1.1`, archive size `293820200` bytes, SHA-256 `574082d35e49a2bcbdc355836b2a3ae5e5bb3b9456c9f5e37177db2ab4aad870`, download time 33 seconds, extraction time 4 seconds, and 53 notice/license paths. GCC reports `arm-none-eabi-gcc (xPack GNU Arm Embedded GCC arm64) 15.2.1 20251203`. The receipt is under ignored `build/toolchains/`; it contains no signed redirect URL and no toolchain binary is tracked.

## Task Commits

1. **Task 1: Install one exact pinned host tool through verified atomic promotion** — `be515bb` (RED test), `674000c` (implementation).
2. **Task 2: Refuse every unsafe tool request and archive with a clean companion** — `18e2150` (RED test), `7117e45` (offline matrix), `d417197` (host-to-asset validation fix).

The plan commit ledger measured five task commits from `26081bfae176e16f07be57223782426c358e591a` through `d417197cd0fa33e92a9659096c557026ba809064`.

## TDD Gate Compliance

- Task 1 RED evidence: `build/phase05-toolchain-red.json`; `tdd-red-evidence` returned `RED_EVIDENCE_OK` for the missing exact lock. The later lock and installed selectors passed.
- Task 2 RED evidence: `build/phase05-toolchain-lock-mismatch-red.json`; `tdd-red-evidence` returned `RED_EVIDENCE_OK` because a Darwin row naming the Linux archive was accepted before the fix. The corrected row now fails with `lock-url` before the injected curl runs.
- Task 2 GREEN: the complete `offline-all` selector passed all 21 refusal and clean-success cases.

## Verification

- `sh -n scripts/fetch-toolchain.sh` — passed.
- `cmake -DCASE=lock -DSRC=. -P tests/gates/toolchain_case.cmake` — passed.
- `cmake -DCASE=installed -DSRC=. -DBIN=build/phase05-toolchain -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/toolchain_case.cmake` — passed; both installed binutils tools executed and the receipt matched the actual archive.
- `cmake -DCASE=wrong-host-row -DSRC=. -DBIN=build/phase05-toolchain -P tests/gates/toolchain_case.cmake` — passed; a cross-host URL is rejected before curl.
- `cmake -DCASE=offline-all -DSRC=. -DBIN=build/phase05-toolchain -P tests/gates/toolchain_case.cmake` — passed all 21 cases, including a valid allowed redirect and safe internal links.
- `cmake --workflow --preset ci` — configure and build passed; all 329 tests passed, then `inventory.junit` passed. The existing optional `signature.owner_local` test was reported skipped because `GBL_OWNER_LOGO_FILE` was unset; the inventory allowed exactly this skip. This was not a hosted CI run.
- `git diff --check` — passed.

## Files Created

- `toolchain/arm-none-eabi.lock` — exact host-specific archive identities.
- `scripts/fetch-toolchain.sh` — lock validation, bounded download, safe extraction, notice receipt, and atomic installation.
- `tests/gates/toolchain_case.cmake` — installed, lock, cross-host mismatch, and offline refusal selectors.

## Decisions Made

- Keep toolchain pins separate from generic fixture pins and leave `scripts/fetch-pin.sh` unchanged.
- Require an exact host-to-asset URL mapping in addition to the locked size and digest.
- Store all downloaded and installed artifacts only under ignored build output.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Reject a cross-host archive URL before download**
- **Found during:** Task 2, when the first RED test showed a Darwin lock row could name and install the Linux asset.
- **Issue:** Host selection alone did not guarantee that the locked URL pointed to that host's exact published archive.
- **Fix:** Bound each host row to its expected official release asset URL and refuse mismatches before invoking curl.
- **Files modified:** `scripts/fetch-toolchain.sh`, `tests/gates/toolchain_case.cmake`.
- **Verification:** The negative control now receives `lock-url` before curl; `offline-all` and the installed case pass.
- **Committed in:** `d417197`.

**Total deviations:** 1 auto-fixed correctness issue. All other implementation and test work followed the plan.

## Issues Encountered

- The first native installation attempt refused before extraction because an initial symlink-target normalization expression was malformed. It created no promoted install. The expression was corrected, and the subsequent real download was verified and installed.
- A CMake regular-expression length assertion did not behave as intended for the digest check; the test now checks exact length and hexadecimal characters separately.

## Known Stubs

None in the three plan artifacts.

## Next Phase Readiness

The pinned Darwin toolchain is installed locally, and the installer has offline refusal coverage. This plan does not establish Linux runtime identity, ARMv4T diagnostic output, byte-identical diagnostic ROMs, or diagnostic CI integration. DIAG-01 and DIAG-02 therefore remain pending; later Phase 5 plans own those checks. No ROM or BIOS data was read, and no toolchain binary was committed or uploaded.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-11*

## Self-Check: PASSED

- Summary file exists at the canonical plan path.
- All five task commits are ancestors of the plan head recorded above.
- The installed toolchain receipt and executable outputs remain ignored build artifacts.
