---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "02"
subsystem: toolchain
tags: [armv4t, thumb1, xpack, cmake, diagnostics, parity]

requires:
  - phase: 05-01
    provides: Pinned xPack host archives, verified installer, install receipts, and notice inventory.
provides:
  - Original ARM/Thumb smoke and assertion-broken images with complete mailbox observations.
  - Strict object/map/architecture/signature controls and local macOS/Linux arm64 byte-parity evidence.
affects: [05-03, 05-21, diagnostics, toolchain]

actuals:
  tokens: 16727
  tasks: 2
  commits: 6
plan_head_before: 36261d93edea8a250bff8a3024bf5079344f52b4
plan_head_after: c6ce7680ef0e9affc5582c061388b724e728f978

tech-stack:
  added: []
  patterns:
    - Direct ARM7TDMI assembler/linker flow with project-owned startup, linker script, and bounded header fixer.
    - Receipt-backed exact local host output-set comparison before any digest admission.

key-files:
  created:
    - diagnostics/armv4t/startup.S
    - diagnostics/armv4t/smoke.S
    - diagnostics/armv4t/linker.ld
    - cmake/FixGbaHeader.cmake
    - cmake/BuildArmDiagnostics.cmake
    - tests/gates/diagnostic_linkmap.cmake
    - tests/gates/toolchain_spike.cmake
    - .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-TOOLCHAIN-SPIKE.md
  modified: []

key-decisions:
  - "Keep the production runner's GUEST_STOP/unregistered-content exit 1 as the raw result, and assert mailbox state/case identity separately."
  - "Limit the parity result to local macOS arm64 and native Linux arm64; hosted Linux x64 remains a separate witness, and no ROM digest is admitted here."

patterns-established:
  - "Publish terminal mailbox completion last so a partial guest run cannot appear complete."
  - "Audit every object and final ELF, every executable section, the exact link-map input set, and generated-directory signatures."

requirements-completed: []
coverage:
  - id: D1
    description: Original mixed ARM/Thumb positive and deliberately broken smoke images reach the production runner mailbox with stable case identity.
    verification:
      - kind: integration
        ref: "ctest --test-dir build/phase05-spike/task1-green --output-on-failure"
        status: pass
      - kind: other
        ref: "build/phase05-spike/native/proof/{smoke,smoke-broken}/elf-sections.txt and executable.disasm.txt"
        status: pass
    human_judgment: false
  - id: D2
    description: Strict map, v4T, executable-section, signature, receipt, and exact local arm64 parity gates.
    verification:
      - kind: integration
        ref: "cmake -DCASE=local-proof -DSRC=. -DBIN=build/phase05-spike -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/toolchain_spike.cmake"
        status: pass
      - kind: other
        ref: "cmake -DCASE=offline-controls -DSRC=. -DBIN=build/phase05-spike -P tests/gates/toolchain_spike.cmake"
        status: pass
      - kind: other
        ref: "cmake -DCASE=proof-only -DSRC=. -DBIN=build/phase05-spike -P tests/gates/toolchain_spike.cmake"
        status: pass
    human_judgment: false

duration: 68min
completed: 2026-11-11
status: complete
---

# Phase 05 Plan 02: Original ARM/Thumb Toolchain Spike Summary

**Project-owned ARM/Thumb smoke images now build with v4T evidence and byte-identical local macOS arm64/Linux arm64 outputs, with the real runner's mailbox result checked independently from its unregistered-content stop.**

## Performance

- **Duration:** 68 minutes, measured from the Plan 02 start marker in `STATE.md` through the completed evidence report.
- **Started:** 2026-11-11T17:07:39Z
- **Completed:** 2026-11-11T18:15:27Z
- **Tasks:** 2
- **Files modified:** 8 declared plan artifacts

## Accomplishments

- Built original positive and deliberately broken ARM/Thumb images with direct `as`, `ld`, and `objcopy`; the header/logo window stays zeroed and the stack is separate from the mailbox.
- Verified exact v4T, ARM, and Thumb-1 attributes on each object and final ELF; audited all executable sections, project-only map inputs, no unresolved symbols, output names, and signature scans.
- Compared the complete macOS arm64 and native Linux arm64 output sets with literal `cmp`; both image pairs returned 0. The Linux xPack install and fresh/cached timing evidence are recorded in [05-TOOLCHAIN-SPIKE.md](./05-TOOLCHAIN-SPIKE.md).

## Task Commits

1. **Task 1: Build and observe one original mixed ARM/Thumb smoke image** — `a482f19` (RED), `785a27e` (GREEN), `ecab34c` (evidence correction).
2. **Task 2: Gate link/architecture/signature proof and compare exact local host bytes** — `f148918` (RED), `b3cf9a0` (GREEN), `c6ce768` (measured evidence report).

The plan commit ledger records six task/evidence commits from `36261d9` through `c6ce768`. The summary and canonical planning bookkeeping are committed separately after self-check.

## Files Created/Modified

- `diagnostics/armv4t/startup.S` — Sets the stack and publishes the completed five-word mailbox last.
- `diagnostics/armv4t/smoke.S` — Runs fixed ARM and Thumb arithmetic checks with positive and broken expectations.
- `diagnostics/armv4t/linker.ld` — Places the entry/header at ROM start and retains the project-owned executable sections.
- `cmake/FixGbaHeader.cmake` — Validates size and zero logo window, writes the checksum through a staged output, and checks postconditions.
- `cmake/BuildArmDiagnostics.cmake` — Builds both variants, verifies tool receipts/attributes/maps/disassembly, and writes ignored receipts.
- `tests/gates/diagnostic_linkmap.cmake` — Requires exact object inputs, v4T attributes, and complete executable-section evidence.
- `tests/gates/toolchain_spike.cmake` — Runs synthetic controls, local build/parity proof, read-only proof validation, and fail-closed hosted proof validation.
- `05-TOOLCHAIN-SPIKE.md` — Records actual host/archive/timing/parity observations and remaining limits.

## Decisions Made

- The unregistered positive and broken images both make the runner return `GUEST_STOP` / `unregistered-content` with exit 1. The acceptance check separately requires mailbox `pass` or `fail` with case `20481`; no production runner behavior was changed.
- Local byte identity is established only for macOS arm64 and native Linux arm64. Hosted Linux x64 was not run in this plan, and no generated ROM digest was added to a manifest or pin.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Disassembled the executable GBA header section as well as `.text`**

- **Found during:** Task 2 map and section-gate implementation.
- **Issue:** Task 1 captured `.text` disassembly but the linked `.gba_header` section is also marked executable and contains `_start`'s entry branch.
- **Fix:** Captured `readelf -SW`, required the complete executable section set, and disassembled `.gba_header` plus `.text`.
- **Files modified:** `cmake/BuildArmDiagnostics.cmake`
- **Verification:** Fresh two-image build and both real-runner mailbox CTests passed; the saved disassembly shows both executable sections.
- **Committed in:** `ecab34c`.

**2. [Evidence adjustment] Checked the documented raw runner result separately from mailbox semantics**

- **Found during:** Task 1 verification.
- **Issue:** The plan's command list treats any nonzero shell exit as failure, but the planned unregistered image intentionally exits 1 with `GUEST_STOP` / `unregistered-content` after exposing its mailbox.
- **Fix:** The focused CTest assertions require that exact raw status/reason/exit and separately require semantic mailbox state and case ID for both positive and broken images.
- **Files modified:** None; verification followed the task's stated behavior and owner instruction.
- **Verification:** Both positive and broken runner cases passed with exact documented raw status and distinct mailbox semantics.

**Total deviations:** 2 (one correctness fix, one verification interpretation adjustment).
**Impact on plan:** Required evidence is complete. Hosted Linux x64 and digest admission remain outside this plan's result.

## TDD Gate Compliance

- **Task 1 RED:** `arm-thumb-smoke-mailbox` ran the production executable before the completion marker existed and failed on the intended pending mailbox; the recorded JUnit was classified `RED_EVIDENCE_OK`.
- **Task 1 GREEN:** Both positive and broken variants then passed semantic checks for `pass` and `fail`, case `20481`, while preserving raw `GUEST_STOP` / exit 1.
- **Task 2 RED:** The offline control target failed specifically because the partial link-map gate accepted synthetic `Tag_CPU_arch: v5TE`; `gsd_run check tdd-red-evidence` returned `RED_EVIDENCE_OK`.
- **Task 2 GREEN:** Offline map/architecture/section/signature controls, local-proof, proof-only, and the CTest harness passed after implementation.

## Issues Encountered

- The first Task 2 RED classifier attempt used a narrative string instead of the unchanged JUnit XML in its `output` field and pointed at a relative report path. The evidence record was corrected to include the real JUnit report; the classifier then accepted the intended failing target. No implementation change was authorized by the invalid record.
- Receipt serialization initially truncated a quoted Docker `RepoDigests` array. Image ID/platform and repository digest were split into separate read-only inspections; the cached Linux witness and proof-only recheck passed with a complete receipt.

## User Setup Required

None. No external service, private ROM/BIOS, or owner-provided setup was used.

## Next Phase Readiness

- The original local ARM/Thumb tracer and macOS/Linux arm64 parity evidence are ready for the next Phase 05 plan.
- Do not mark `DIAG-01` or `DIAG-02` fully complete from this plan alone. Hosted Linux x64 evidence is still pending, and no ROM digest has been admitted.

## Self-Check: PASSED

All eight declared artifacts exist, and each of the six plan commits is an ancestor of the final task-artifact head recorded above.

---
*Phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio*
*Completed: 2026-11-11*
