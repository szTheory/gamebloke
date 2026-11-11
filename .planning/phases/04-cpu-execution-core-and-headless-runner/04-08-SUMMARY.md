---
phase: 04-cpu-execution-core-and-headless-runner
plan: "08"
subsystem: runner
tags: [C17, SHA-256, CAVP, ASan, UBSan]

# Dependency graph
requires:
  - phase: 04-cpu-execution-core-and-headless-runner
    provides: "The plan 04-07 bounded CPU/trace foundation; this slice adds an independent host-side span digest utility."
provides:
  - "Original dependency-free SHA-256 over an exact caller-owned byte span with lowercase hex output."
  - "Independent RFC/CAVP vectors, synthetic whole-span and tail-mutation witnesses, and refusal postconditions."
affects: [headless runner, loaded-byte identity, 04-09 runner integration]

# Actuals (#2632): chars/4 over the realized implementation/test diff.
actuals:
  tokens: 4414.25
  tasks: 1
  commits: 3
commits: 3
plan_head_before: e3f12f3831290a66ee0212131a671e4fea515d7e
plan_head_after: 9e65eab5e845d591355c9d0a8f555fcf96c587e

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Hashing accepts an explicit caller-owned span; the host helper has no file, clock, machine-state, allocation, or shared mutable-state dependency."
    - "Standard-vector provenance and synthetic oracle results are recorded separately from cryptographic validation or content-rights claims."

key-files:
  created:
    - runner/sha256.c
    - runner/sha256.h
    - tests/hash_test.c
    - .planning/phases/04-cpu-execution-core-and-headless-runner/04-08-SUMMARY.md
  modified: []

key-decisions:
  - "Keep the arbitrary-span SHA-256 helper separate from the existing fixed 156-byte logo-window helper."
  - "Reject null/nonempty input, bit-length overflow, null output, and input/output overlap before writing any output byte."
  - "Do not register CMake targets in this slice; plan 04-09 owns build and runner integration."

requirements-completed: [] # The utility is proven here; runner binding remains for plan 04-09.

coverage:
  - id: D1
    description: "A host-side SHA-256 utility hashes exactly the caller-provided span and preserves output on invalid span refusals; runner binding remains unimplemented in this slice."
    requirement: RUN-01
    verification:
      - kind: unit
        ref: "tests/hash_test.c#14 TAP cases; focused strict-warning and ASan/UBSan invocations"
        status: pass
    human_judgment: false

metrics:
  duration: "about 24 minutes; exact plan start time was not captured"
  completed: 2026-11-11
status: complete
---

# Phase 4 Plan 8: Bounded SHA-256 Span Summary

**A dependency-free C17 helper now hashes arbitrary caller-owned spans with independent SHA-256 vectors and explicit no-write refusal behavior.**

## Performance

- **Duration:** About 24 minutes; the exact plan start timestamp was not captured.
- **Started:** Not captured.
- **Completed:** 2026-11-11T07:48:59Z.
- **Tasks:** 1/1.
- **Implementation/test files:** 3 created.
- **Measured focused command wall time:** The final post-commit tracer run completed in about 0.16 seconds. The strict-warning and ASan/UBSan runs after the full-buffer assertion change completed in about 0.20 and 0.75 seconds while the three builds ran concurrently.

## Accomplishments

- Added `gbl_sha256_span`, an original SHA-256 implementation following NIST FIPS 180-4 Sections 5.1.1, 5.2, and 6.2. It consumes exactly the supplied span, handles padding with one fixed 64-byte block, and allocates no memory.
- Defined lowercase 64-character hexadecimal output plus a terminator. Empty input accepts a null data pointer. Invalid nonempty null input, unrepresentable bit length, null output, and input/output overlap return false before output writes.
- Added 14 TAP cases for empty input, `abc`, NIST CAVP 55/56/63/64-byte vectors, a 65-byte independent oracle, a NIST 163-byte multi-block vector, a synthetic 192-byte image span and its last-byte mutation, and invalid argument postconditions.
- Documented provenance in source: FIPS 180-4 supplies the algorithm; RFC 6234 supplies the `abc` example; NIST CAVP `SHA256ShortMsg.rsp` and `SHA256LongMsg.rsp` supply standard response vectors. NIST describes these vectors as informal checks, not validation. The 65-byte range and 192-byte generated spans were independently cross-checked with Python `hashlib` and OpenSSL.

## TDD Gate Compliance

- **RED:** Compiled the test against a temporary RED-only API stub using strict warnings. TAP reported the target empty-span digest assertion as `not ok`; the stub returned 64 zeroes instead of the NIST digest. All 14 tests executed, with the four invalid-input controls passing. `node /Users/jon/.codex/gsd-core/bin/gsd-tools.cjs check tdd-red-evidence build/phase04-tracer/hash-red-evidence.json --raw` returned `RED_EVIDENCE_OK` (`target_test_failed`). The stub lives outside the repository and is not part of the implementation.
- **GREEN:** The production implementation passed all 14 cases. A first GREEN run exposed a transcription error in the 163-byte NIST message; that fixture was corrected to match the source response file, then the full set passed. Review also tightened invalid-span assertions to compare the entire 65-byte output buffer; the normal, strict-warning, and sanitizer runs passed after that test change.
- **REFACTOR:** No separate refactor commit was needed; the production code remains a small isolated module.

## Verification

- `cmake -E make_directory build/phase04-tracer && cc -std=c17 -Irunner -Itests runner/sha256.c tests/hash_test.c -o build/phase04-tracer/hash_test && build/phase04-tracer/hash_test` — passed all 14 TAP cases. The final run after the test-hardening commit passed for the tracer feedback gate.
- `cc -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -Irunner -Itests runner/sha256.c tests/hash_test.c -o build/phase04-tracer/hash_test_strict && build/phase04-tracer/hash_test_strict` — passed all 14 cases with warnings as errors.
- `/usr/bin/clang -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -Irunner -Itests runner/sha256.c tests/hash_test.c -o build/phase04-tracer/hash_test_sanitize && build/phase04-tracer/hash_test_sanitize` — passed all 14 cases under AddressSanitizer and UndefinedBehaviorSanitizer.
- `git diff --check` — passed. Stub/placeholder scan of all three implementation/test files found no tracked stub markers.
- Python `hashlib` and OpenSSL agreed on the independent 65-byte, 192-byte, and 192-byte final-byte-mutation digests pinned in `tests/hash_test.c`.

The complete CMake/CI workflow was not run: these files are intentionally unregistered in this slice, and plan 04-09 owns their build and runner integration. No full CPU rebuild, private-ROM read, or guest execution was needed for this host-only helper.

## Task Commits

1. **Task 1 RED:** `be2790d` — independent span vectors and refusal assertions.
2. **Task 1 GREEN:** `b33114c` — bounded SHA-256 implementation and corrected 163-byte vector.
3. **Task 1 refusal postcondition:** `9e65eab` — compare the full output buffer after null-span and bit-length refusals.

The summary metadata commit is separate from the three measured task commits.

## Supported Scope and Limits

The helper hashes the bytes passed by its caller. This plan does not open files, bind the helper to a loaded ROM, add CMake registration, or change machine state. The implementation is not FIPS validated. A matching digest proves byte identity only; it grants no permission to execute, commit, upload, or redistribute content.

## Decisions Made

- Preserve separate ownership between the existing fixed-window logo digest and the new general-purpose byte-span helper.
- Treat malformed spans and aliases as refusals with a stable false return and unchanged output buffer.
- Keep the runner integration point for plan 04-09, where the helper can be connected to the actual loaded bytes.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected a malformed NIST long-message test vector**
- **Found during:** Task 1 GREEN verification.
- **Issue:** A missing hexadecimal nibble made the transcribed message 162 bytes instead of the official 163-byte NIST vector.
- **Fix:** Corrected the final nibble against `SHA256LongMsg.rsp`.
- **Files modified:** `tests/hash_test.c`.
- **Verification:** The corrected input decodes to 163 bytes and matches the NIST digest; all 14 cases pass in normal, strict-warning, and sanitizer builds.
- **Committed in:** `b33114c`.

**Total deviations:** 1 auto-fixed (Rule 1 - Bug).
**Impact on plan:** The correction restored the planned independent multi-block witness; no scope or API change was needed.

## Issues Encountered

The first implementation run correctly exposed the vector transcription error before the GREEN commit. The source response file was checked directly, the fixture was corrected, and the focused suite then passed.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Plan 04-09 can register the new helper and bind its digest to the actual loaded byte span. `RUN-01` remains partial until that runner path is integrated and verified. This plan makes no full-runner, hardware, compatibility, or rights qualification claim.

## Self-Check: PASSED

- `runner/sha256.c`, `runner/sha256.h`, `tests/hash_test.c`, and this summary exist.
- Task commits `be2790d`, `b33114c`, and `9e65eab` are ancestors of `HEAD`.
- The persisted plan ledger measures 3 task commits; all planned behavior and focused verification passed.
