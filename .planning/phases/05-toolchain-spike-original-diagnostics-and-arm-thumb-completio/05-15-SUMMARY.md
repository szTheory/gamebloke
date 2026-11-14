---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "15"
subsystem: cpu
tags: [arm7tdmi, armv4t, thumb, bus, alignment]
requires:
  - phase: 05-14
    provides: Thumb ALU/high-register baseline and canonical bounded CPU units
provides:
  - Thumb formats 6–13 register, immediate, halfword, SP-relative, address and stack-adjust execution
  - Focused transfer/SP selectors with independent address, result, bank and preflight checks
affects: [05-16, 05-17, 05-18, cpu-diagnostics]
actuals:
  tokens: 11997
  tasks: 2
  commits: 5
  plan_head_before: 957aa02c6c972662213a4397da44cbb7786c8bae
  plan_head_after: d8097bffc9aa9192dc9ba822d7f10f2e89781cf2
tech-stack:
  added: []
  patterns: [canonical bounded transfer slots, captured banked operands, independently derived instruction vectors]
key-files:
  created: []
  modified: [src/cpu.c, tests/cpu_test.c, tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt]
key-decisions:
  - "Formats 6–13 share the existing bounded transaction/result path; data values and time remain bus-owned."
  - "Odd halfword behavior follows the selected GBA reference policy in GBATEK 3.06; this is not a hardware measurement."
requirements-completed: []
coverage:
  - id: D1
    description: "Thumb PC literal, register-offset and immediate word/byte/halfword transfers use independent addresses and actual bus results."
    verification:
      - { kind: unit, ref: "build/ci/cpu_test thumb_transfer (31 assertions)", status: pass }
      - { kind: unit, ref: "native AppleClang ASan/UBSan cpu.thumb_transfer", status: pass }
    human_judgment: false
  - id: D2
    description: "Thumb halfword-immediate, SP-relative, PC/SP address-generation and SP-adjust forms preserve alignment, banks, flags and refusal postconditions."
    verification:
      - { kind: unit, ref: "build/ci/cpu_test thumb_sp (36 assertions)", status: pass }
      - { kind: unit, ref: "native AppleClang ASan/UBSan cpu.thumb_sp", status: pass }
      - { kind: integration, ref: "focused CPU CTest regressions (11 selectors)", status: pass }
    human_judgment: false
duration: "16min18s from first RED receipt through final verification; summary work excluded"
started: "2026-11-12T22:58:13Z"
verified: "2026-11-12T23:14:31Z"
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 15: Thumb transfers and stack-address forms Summary

**Thumb formats 6–13 now use the canonical bounded CPU path for transfers, address generation and stack adjustment.**

## Accomplishments

- Added Thumb register-offset signed/unsigned byte and halfword loads, word/halfword/byte stores, and immediate word/byte/halfword transfers. Actual bus values feed result transforms, including word rotation, odd LDRH rotation, and the selected odd LDRSH-as-signed-byte policy.
- Added SP-relative word access, PC/SP-relative address generation, and modulo-32 ADD/SUB SP. Register banks and store sources are captured before effects; flags remain unchanged where the instruction requires it.
- Kept access and sequential-fetch slots in the shared bounded unit. Exact-fit succeeds; one-short cycle and trace limits preserve canonical state, machine identity, and caller trace bytes.
- Registered `thumb_transfer` and `thumb_sp` in the focused CMake selector and both CPU inventories.

## Task Commits

| Task | RED | Additional fix | GREEN |
| --- | --- | --- | --- |
| Formats 6–9 | `32bf7a5` | — | `c3d4dd4` |
| Formats 10–13 | `2246e5b` | `d4e8e5e` | `d8097bf` |

The additional Task 2 commit corrects the test fixture’s bank-array index for r13. Plan task commits measured from `957aa02c6c972662213a4397da44cbb7786c8bae` through `d8097bffc9aa9192dc9ba822d7f10f2e89781cf2`: **5**. The summary commit is outside that interval.

## Verification

Receipts are ignored local files under `build/phase05-plan15/`.

- Task 1 RED was machine-validated as `RED_EVIDENCE_OK` for the executed odd-address LDRSH assertion. The committed post-commit tracer passed **31/31** `thumb_transfer` assertions before Task 2 expansion.
- Task 2 RED was machine-validated as `RED_EVIDENCE_OK`: all **36** `thumb_sp` assertions initially reached valid opcodes that stopped as unsupported; the named A801 modulo-wrap case reported reason 3 and left Rd unchanged. No setup or build failure was used as RED evidence.
- After correcting the SP fixture bank index, `cmake --preset ci && cmake --build --preset ci && build/ci/cpu_test thumb_sp` passed **36/36**. The `thumb_transfer` regression passed **31/31**.
- Native focused regressions: `ctest --test-dir build/ci --output-on-failure -R '^cpu\\.(thumb|thumb_alu|thumb_high|thumb_sp|thumb_transfer|arm_pc_return|loaded_pc|arm_transfer|arm_subword|arm_subword_alignment|alignment)$'` — **11 selectors pass** (`cpu-regressions.log`).
- AppleClang 21.0.1 ASan/UBSan configured with `/usr/bin/clang`, built only `cpu_test`, and ran the same focused 11-selector regression set — **11 pass, 0 failures/errors**. JUnit receipt: `build/phase05-plan15/asan/build/phase05-plan15/asan/junit-targeted.xml`.
- Pinned GNU build-only check after the first GREEN passed at `c3d4dd4`; the final-source helper `sh build/phase05-plan13/root-gnu-build-only.sh` passed at exact HEAD `d8097bf`, using a tracked-source clone, aarch64 Ubuntu 26.04, GCC 15.2, and only the `cpu_test` build target (`final-gnu.log`).
- `git diff --check` passed. Full compiler/sanitizer inventories and hosted CI remain at the root-owned Thumb integration boundary; this plan does not claim those gates.

## Decisions and sources

[ARM7TDMI DDI0029G Table 1-7](https://documentation-service.arm.com/static/5e8e1323fd977155116a3129) and [ARM ARM DDI0100I A6.5](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a) define the Thumb transfer/address instruction forms and scaled offsets. Odd GBA halfword handling follows the reference policy in [GBATEK 3.06, ARM CPU memory alignments](https://mgba-emu.github.io/gbatek/#arm-cpu-memory-alignments). These references inform the emulator’s selected policy; no physical hardware behavior was measured.

## Deviations from Plan

### Auto-fixed issues

**1. Rule 1 — Corrected SP test bank selection**
- **Found during:** Task 2 GREEN verification.
- **Issue:** The test fixture stored the System/User r13 value in `usr_r8_14[6]`, which is r14; r13 is `usr_r8_14[5]`. This made otherwise implemented SP cases read the wrong register.
- **Fix:** Updated SP-relative, address-generation, stack-adjust and preflight fixtures to seed and check r13’s actual bank slot. No production behavior was changed for the fixture error.
- **Files modified:** `tests/cpu_test.c`.
- **Verification:** `thumb_sp` passed 36/36, then all 11 native and ASan/UBSan focused regressions passed.
- **Committed in:** `d4e8e5e`.

**2. Rule 3 — Added selector and inventory registration**
- **Found during:** Task 1 and Task 2 test authoring.
- **Issue:** New selectors need explicit CMake, CI and sanitizer inventory entries to be discoverable by focused and full lanes.
- **Fix:** Registered `thumb_transfer` and `thumb_sp` in those three inventories.
- **Files modified:** `tests/core.cmake`, `tests/inventory/ci.txt`, `tests/inventory/asan.txt`.
- **Verification:** Both selectors ran directly and appeared in the focused native and sanitizer CTest lists.
- **Committed in:** `32bf7a5`, `2246e5b`.

**Total deviations:** 2 auto-fixed issues. Both were required for correct test setup and discoverability; scope and production semantics stayed within Plan15.

## Issues Encountered

The first Task 2 GREEN attempt passed halfword and PC-based cases but exposed the test fixture’s r14/r13 indexing error in the SP cases. The fixture was fixed in its own commit, and the focused suite then passed. No implementation blocker remains.

## User Setup Required

None.

## Next Phase Readiness

Plan15’s two tasks are complete. Root owns the independent CPU-family review, shared STATE/ROADMAP/requirements bookkeeping, and the final Thumb integration compiler/sanitizer/inventory gates. CPU-02 remains phase-wide pending; no full ARM qualification, hosted CI or public/private ROM work is claimed here.

## Self-Check: PASSED

The summary file exists, all five task commits are ancestors of recorded `plan_head_after`, and the five owned implementation/test paths match the measured diff. Task selectors, focused native/ASan regressions, early/final GNU compile-only checks and the classifier-validated RED records have receipts under `build/phase05-plan15/`. No tracked files were deleted. Owner-dirty shared planning files and root handoffs remain unstaged.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-12*

## Root reviewed closeout

[Standard-depth independent review](05-15-REVIEW.md) is clean across all five changed implementation/test-registration files. Root reran both focused selectors successfully after review. Schema/UI gates do not block; codebase drift remains explicitly skipped without STRUCTURE.md. Full inventories remain at the Thumb completion boundary; last published CI still covers earlier source only. Plan16 is next.
