---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "11"
subsystem: cpu
tags: [armv4t, ldrh, strh, ldrsb, ldrsh, alignment, preflight, ctest, asan]

requires:
  - phase: 05-10
    provides: Canonical CPU-unit dispatch, data-transfer slots, and verified native CI path
provides:
  - ARMv4T halfword and signed-byte transfers through the canonical bounded slot path
  - GBA-reference odd-address policies checked against actual bus results
  - The arm_subword_alignment selector in CI and sanitizer inventories
affects:
  - Later Phase 05 CPU completion plans
  - Phase 06 cross-compiler and CPU fixture verification

actuals:
  tokens: 13517.75
  tasks: 2
  commits: 3
commits: 3
plan_head_before: 8f861f79d961c0416d48204d4982cbb1aca68de2
plan_head_after: 69f309546c4a61fa429500edbe587532096799d5

tech-stack:
  added: []
  patterns:
    - Subword CPU slots consume actual bus results before applying alignment and sign transforms
key-files:
  created: []
  modified:
    - src/cpu.c
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
key-decisions:
  - "Mode 3 legal forms use the shared CPU-unit slot path with captured operands and preflighted effects."
  - "GBA odd LDRH rotates the aligned-down halfword by eight; odd LDRSH reads and signs the addressed byte; odd STRH aligns down, following GBATEK 3.06 as a reference policy."
  - "The alignment behavior is reference-backed emulator policy; no physical hardware measurement or qualification is claimed."
patterns-established:
  - "Compute read postprocessing from the actual bus result and keep its bus width/address visible in trace records."
requirements-completed: []

coverage:
  - id: D1
    description: "Legal ARMv4T LDRH, STRH, LDRSB, and LDRSH address forms execute through the canonical unit path with documented restrictions."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test arm_subword (46 assertions)"
        status: pass
      - kind: unit
        ref: "cmake --workflow --preset ci"
        status: pass
    human_judgment: false
  - id: D2
    description: "Odd subword behavior uses GBATEK reference policy and actual bus results, with atomic preflight checks."
    verification:
      - kind: unit
        ref: "build/ci/cpu_test arm_subword_alignment (37 assertions)"
        status: pass
      - kind: unit
        ref: "AppleClang ASan/UBSan: ctest --preset asan"
        status: pass
    human_judgment: false

duration: 32min
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 11: ARM Halfword and Signed Transfers Summary

**ARMv4T halfword and signed transfers use canonical CPU slots, with GBA-specific odd-address policies tested against actual bus results.**

## Performance

- **Duration:** 32 minutes from the first task commit.
- **Started:** 2026-11-12T12:36:31-05:00
- **Completed:** 2026-11-12T13:08:35-05:00
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments

- Added LDRH, STRH, LDRSB, and LDRSH decoding for legal immediate split-nibble and register-offset forms, including pre/post indexing, U, and valid writeback. Operands are captured before effects; the bounded shared slots reserve bus, trace, cycle, and counter costs before execution.
- Preserved ARMv4T restrictions for signed stores, later-version LDRD/STRD encodings, PC operands, base/data aliases, and register-offset hazards. Refusals remain atomic.
- Applied the GBATEK 3.06 GBA reference policy: odd LDRH rotates the aligned-down halfword by eight, odd LDRSH reads a signed byte at the original address, and odd STRH aligns down. Tests derive read expectations from the actual bus result and cover RAM, ROM tail, absent BIOS, unimplemented I/O, and unmapped outcomes. This is not a hardware measurement.
- Added `arm_subword_alignment` to the CTest selector and both required test inventories.

## Task Commits

Task 1: Execute legal ARMv4T halfword and signed transfers:

1. `5c6aea9` — failing ARM subword transfer tests.
2. `4a16143` — canonical ARM subword transfer implementation.

Task 2: Exercise odd and misaligned subword behavior:

1. `af438af` — bus-result, alignment, wrap, and preflight assertions plus selector inventories.

The persisted plan ledger measures three task commits from `8f861f7` through `af438af`; the plan metadata commit is recorded separately after the planning-state update.

## Files Modified

- `src/cpu.c` — Mode 3 decode, legality checks, old-operand snapshots, read transforms, aligned-down stores, and bounded bus slots.
- `tests/cpu_test.c` — 46 legal-form/refusal assertions and 37 alignment, bus-result, wrap, trace, and boundary assertions.
- `tests/core.cmake` — registered `arm_subword_alignment`.
- `tests/inventory/ci.txt` and `tests/inventory/asan.txt` — included the selector in both required inventories.

## Verification Evidence

- Task 1 RED: `build/phase05-plan11/task1-red-evidence.json` was classified `RED_EVIDENCE_OK`. The named LDRH assertion failed on the unsupported Mode 3 behavior; 29 of 46 assertions failed and the 17 refusal assertions passed. After implementation, the required tracer rerun passed 46/46.
- Task 2 baseline: `arm_subword_alignment` passed 37/37 before its test commit and again after it. Since Task 1 already implemented the odd-address policies, this was an expected green baseline.
- Disposable mutation controls: a halfword-rotation mutation failed assertion 24, a sign-extension mutation failed assertion 20, and a store-alignment mutation failed assertion 26. Each clean companion passed. The controls used isolated copies under `build/phase05-plan11`; no mutation was applied to tracked production files.
- Native CI: `cmake --workflow --preset ci` passed all 341 expected entries; 340 ran and passed, with the single allowed `signature.owner_local` skip because `GBL_OWNER_LOGO_FILE` was unset. The post-inventory audit confirmed the count.
- AppleClang ASan/UBSan: configured and built with `/usr/bin/clang`; `ctest --preset asan` passed all 84 entries with no skips. `ctest --preset asan-post` confirmed 84 expected, 84 run, and zero skipped.
- No hosted run was started for this local plan. Linux pinned GCC/Clang verification and independent review remain with the root executor.

## Decisions Made

- Keep legal and reserved Mode 3 encodings distinct: ARMv4T halfword/signed transfers execute; signed stores and later-version doubleword encodings stop as unsupported.
- Use actual bus-result values and outcomes for GBA-specific postprocessing, including non-RAM regions. No extra preflight read, second interpreter, or rollback path was introduced.
- Track CPU-01 as phase-wide and leave it pending; this plan alone does not establish full CPU completion or hardware accuracy.

## TDD Gate Compliance

- Task 1 followed RED then GREEN: the 46-assertion semantic RED was recorded and classified before the feature commit; the post-commit tracer passed.
- Task 2 could not produce an honest pre-implementation RED because Task 1 had already committed the correct alignment behavior. Following the parent executor instruction, the implementation was not regressed. The test-only commit was supported by three isolated mutation controls that each failed on its intended assertion while its clean companion passed. This is a documented TDD sequencing deviation, not a claimed pre-implementation RED.

## Deviations from Plan

**1. Task 2 baseline was already green**

- **Found during:** Task 2 baseline verification.
- **Issue:** The plan requested a failing test before implementing odd-address behavior, but that behavior had already been implemented as part of Task 1's correct transfer path.
- **Fix:** Kept production semantics intact, added the alignment-specific selector, and verified its assertions against three isolated incorrect CPU copies.
- **Files modified:** `tests/cpu_test.c`, `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt`.
- **Verification:** Baseline and clean companions passed 37/37; each targeted mutation failed on its corresponding rotation, sign, or aligned-store assertion.
- **Committed in:** `af438af`.

**2. Corrected test-oracle expectations during Task 1**

- The task's test oracle initially expected three cycles for a non-PC transfer and reversed the little-endian STRH bytes. The canonical two-cycle result and low-byte-first store (`D4 C3`) were verified, corrected in the tests, and the final 46-assertion tracer passed.

**Total deviations:** 2; both retained the requested semantics and improved evidence quality. No scope expansion.

## Issues Encountered

- The first disposable rotation mutant used `value = value`, which Clang rejected under `-Wself-assign -Werror` before semantic execution. Only the ignored mutation copy was changed to a non-self-assignment mutant; the corrected control then failed on the intended rotation assertion. No tracked source change resulted.
- No implementation, native CI, or sanitizer failures remain.

## User Setup Required

None.

## Next Phase Readiness

Plan 11 is complete locally. Root owns independent review and pinned Linux verification. CPU-01 and DIAG remain phase-wide pending requirements; no hosted coverage, private ROM/BIOS access, physical hardware evidence, or broad compatibility claim is added here. Plan 12 has not started.

## Self-Check: PASSED

- Summary file exists; all three measured task commits are ancestors of the recorded plan head.
- Task commit count and plan head were measured from `.git/gsd-plan-head-before-05-11`.
- `.planning/state.json` still matches its pre-handler snapshot byte for byte.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*


## Independent review corrections and final Linux verification

Review identified a U-polarity cross-product coverage gap. Root's first pinned GCC build additionally failed on implicit narrowing after integer promotion in the transform-count sum; no Linux tests or sequential Clang lane ran on that initial attempt. The original failure is retained in `build/phase05-plan11/linux-gcc.log`. These were local findings under existing fix authority, with no hosted run.

Two finding-scoped commits close the issues: `4c3ed1b` uses unsigned arithmetic for the bounded 0–4 transform count while retaining all validation, and `69f3095` covers both U values independently for every immediate/register offset, pre-index, and post-index form across all four operations. The subword selector now has 70 assertions. An isolated wrong-U variant fails the newly covered subtract case while its add companion and the clean selector pass. `05-11-REVIEW.md` is clean after independent re-review; `05-11-REVIEW-FIX.md` preserves evidence. The three original task commits/ledger remain their original scope; the two review commits are additional fixes.

Final source `69f309546c4a61fa429500edbe587532096799d5` passed focused normal and AppleClang ASan selectors (70 subword and 37 alignment assertions), then root's sequential full pinned Ubuntu 26.04 native arm64 GCC and Clang 21 sanitizer lanes. GCC ran 340 tests with the single existing allowed owner-local skip and passing post-inventory; Clang ASan/UBSan/LeakSanitizer ran 84/84 with zero skips and passing post-inventory. Final logs/receipts are `build/phase05-plan11/review-fix/final-linux-*`. Earlier full native results above remain evidence for their recorded pre-review source, with final native focused and Linux full checks establishing the corrections.

Wave hooks found no schema drift or UI change needing a contract; codebase drift has the existing advisory no-STRUCTURE.md skip. Routine plan publication remains batched until explicit hosted-witness or final phase gates; the latest published green `a16dad4` / run43249533205 is separate from this local source. No required gate is waived. CPU/DIAG remain phase-wide pending; Plan 12 is next.

Evidence lesson: when a planned behavior is already implemented, record the baseline green and prove new assertions with isolated fault controls. Preserve the real implementation/history and document the sequencing deviation; never manufacture RED by regressing correct production code.
