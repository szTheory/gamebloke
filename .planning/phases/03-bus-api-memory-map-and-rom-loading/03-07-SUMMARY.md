---
phase: "03-bus-api-memory-map-and-rom-loading"
plan: "07"
subsystem: "bus-and-rom-loader"
tags: [loader, logo-digest, C17, CMake, sanitizer, verification]
requires: ["03-06"]
provides:
  - "Fixed 156-byte SHA-256 logo-window diagnostic on the copied ROM candidate"
  - "Strict configure-time digest authority parsing and malformed-metadata controls"
  - "Synthetic canonical-source loader witnesses, semantic mutants, and fresh lane evidence"
affects: ["loader API diagnostics", "CI/ASan/hygiene inventories", "verification documentation"]
tech-stack:
  added: []
  patterns:
    - "Private fixed-window SHA-256 helper authored from NIST FIPS 180-4"
    - "Shared production and synthetic core targets differing only by private expected digest"
    - "Strict configure parser with a single stable refusal reason"
key-files:
  created:
    - "src/logo_sha256.h"
    - "cmake/ReadLogoDigest.cmake"
    - "tests/gates/logo_digest_case.cmake"
  modified:
    - "CMakeLists.txt"
    - "include/gamebloke/gamebloke.h"
    - "src/loader.c"
    - "tests/loader_test.c"
    - "tests/core.cmake"
    - "tests/gates/core_mutation.cmake"
    - "tests/gates.cmake"
    - "tests/inventory/ci.txt"
    - "tests/inventory/asan.txt"
    - "tests/inventory/hygiene.txt"
    - "docs/verify.md"
    - ".planning/phases/03-bus-api-memory-map-and-rom-loading/03-07-PLAN.md"
    - ".planning/phases/03-bus-api-memory-map-and-rom-loading/03-EVIDENCE.md"
    - ".planning/phases/03-bus-api-memory-map-and-rom-loading/03-VALIDATION.md"
decisions:
  - "Production reads only the existing trusted digest row; owner-pending continues to report UNCHECKED/OWNER_PENDING."
  - "Synthetic match and mismatch use the canonical loader sources and prove mechanics only, not Nintendo identity or rights."
  - "No library, dependency, owner file, ROM, BIOS bytes, or canonical digest was introduced."
metrics:
  duration: "44 min"
  completed_date: "2026-11-10"
  tasks: 3
  files: 17
  commits: 5
  plan_head_before: "980b4ef5504120f1aa2de2631b06ad2a74be04ad"
  plan_head_after: "eabbd3393401129d5ecea5a6bd0f20de17c40595"
status: complete
actuals:
  tokens: 14523
  tasks: 3
  commits: 5
---

# Phase 03 Plan 07: Integrated Logo-Window Digest Diagnostics

The canonical copied-ROM loader now reports synthetic logo-window digest matches and mismatches as advisory diagnostics, with production still pending the owner-approved digest.

## What Changed

- Added an original private SHA-256 helper for exactly 156 bytes, backed by independent zero-byte and pattern vectors. The loader checks candidate bytes at offsets `[0x04, 0xA0)` after the validated copy and before commit; a mismatch does not refuse loading.
- Factored the canonical core source list and target options into shared configuration for production and a synthetic test core. Production consumes only the existing authority row; the synthetic target uses owned build-tree bytes and a separate expected digest.
- Added strict configure-time metadata parsing. Malformed, duplicate, or ambiguous rows fail with `logo-digest: invalid-authority` without echoing metadata.
- Added integrated match/mismatch and exact-window assertions, parser cases, digest-bypass and forced-match mutants, clean companions, and preserved ROM/allocator identity controls. Registered the parser case in hygiene only; CI and ASan register it through the shared core test file.
- Updated verification guidance and current execution evidence. The Plan 06 inventory counts remain clearly marked historical.

## Verification

- Task 1 tracer and focused loader/integration/core-purity checks passed; the TDD red witness failed on the missing production call before implementation and passed after integration.
- Focused loader and authority-parser checks passed. Both digest semantic mutants and both ownership identity mutants failed for their intended assertions, while clean companions passed.
- `cmake --workflow --preset ci`: 296 expected, 294 ran, 2 allowed owner skips; inventory post passed.
- `cmake --workflow --preset hygiene`: 201 expected, 199 ran, 2 allowed owner skips; inventory post passed.
- Fresh `/usr/bin/clang` ASan/UBSan lane: 42/42 passed, zero skips; post inventory passed 42/42. Both ownership identity controls and sanitizer negative controls ran.
- Final tree hygiene and signature scans each passed over 195 files; `git diff --check` passed.

Full output, current inventories, compiler configuration, JUnit files, and the initial hygiene failure records are linked from [03-EVIDENCE.md](03-EVIDENCE.md). The initial CI/hygiene runs found two absolute GSD context paths in the plan; commit `bb31d49` made those references portable, and final reruns passed with `pipefail` enabled.

## Deviations from Plan

**1. [Rule 3 - Blocking hygiene issue] Made generated GSD references portable.**
- **Found during:** Task 3
- **Issue:** The tree hygiene gate rejected two absolute home-directory references in the plan's execution context.
- **Fix:** Changed only those references to the established `~/.codex` form in a separate documentation commit, then reran CI and hygiene successfully.
- **Files modified:** `.planning/phases/03-bus-api-memory-map-and-rom-loading/03-07-PLAN.md`
- **Commit:** `bb31d49`

## Process Note

Keep generated planning references portable and stage/scan new plans before committing them; the initial lane logs retain evidence of this hygiene failure and its fix.

## Remaining Acceptance Gate

**LOAD-02 remains PARTIAL/Pending.** The production expected digest stays empty while the owner row is pending, and canonical loaded diagnostics remain `UNCHECKED/OWNER_PENDING`. Synthetic success proves the integrated comparison mechanics only. The owner is next to supply the approved digest through the existing reviewed workflow. Phase acceptance and Phase 4 remain unstarted.

## Self-Check: PASSED

- Summary, created helper/parser/driver, evidence, and validation files exist.
- All five plan commits are reachable from the current phase branch.
- The created and modified plan files contain no unfinished stub or TODO markers.
