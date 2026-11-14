---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-13T05:54:40Z
depth: standard
files_reviewed: 5
files_reviewed_list:
  - cmake/BuildArmDiagnostics.cmake
  - cmake/CheckArmCoverage.cmake
  - diagnostics/armv4t/cases.json
  - tests/gates/coverage_case.cmake
  - tests/gates/diagnostics_case.cmake
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-13T05:54:40Z
**Depth:** standard
**Files Reviewed:** 5 corrected production paths
**Status:** clean

## Summary

Scoped re-review of the five correction paths confirms all four original findings are resolved, with no new correctness defect found in the source contracts, assembler checks, ledger controls, or raw observation gate.

The corrected ledger contains 236 semantic clean/broken pairs and four stop witnesses, for 476 ARM outputs. Historical Plan 19 receipts remain 244 pairs and 492 outputs; those are retained as history, not current evidence.

| Measure | Historical Plan 19 | Corrected source |
|---|---:|---:|
| Ledger rows | 357 | 349 |
| Inventory targets | 334 | 326 |
| Pending rows | 356 | 348 |
| Semantic pairs | 244 | 236 |
| Stop witnesses | 4 | 4 |
| Output images | 492 | 476 |

Existing frozen receipts show the corrected builder produced all 476 outputs, the raw gate observed the 236 clean and 236 broken results plus four stops, and the ledger gate rejected 35 source mutations. Raw results remain unregistered and fixture identity remains null; fixture admission has not occurred.

## Resolved Findings (historical)

### WR-01: PC+12 case read PC from the wrong operand — RESOLVED

**Original issue:** Case `507654410` placed PC in Rn and used an immediate shift, so it tested the +8 read rather than the required register-controlled Operand2 +12 read.

**Disposition:** The program now executes opcode `0xE082031F`, `ADD r0,r2,pc,LSL r3`, with Rn=r2 holding 3, Rs=r3 holding 1, and Rm=PC. For instruction address A, the expected result is `2*(A+12)+3`; the A+8 path differs by `0xFFFFFFF8`. The checker binds the source sequence and arithmetic, the builder checks the actual GNU disassembly and opcode, and three mutation controls reject the former Rn-PC form, immediate-shift encoding, and +8 oracle.

**Evidence:** `diagnostics/armv4t/cases.json:3350`; `cmake/CheckArmCoverage.cmake:357-363`; `cmake/BuildArmDiagnostics.cmake:931-938`; `tests/gates/coverage_case.cmake:216-222`; `build/phase05-plan19/review-fix/tracer-opcodes.json`; `build/phase05-plan19/review-fix/final-ledger-controls.log:2-4`.

### WR-02: No-writeback block cases changed the base — RESOLVED

**Original issue:** Eight `normal-no-writeback` rows used `!`, asserted base changes, and recorded `writeback: true`, duplicating writeback cases.

**Disposition:** The eight programs now snapshot r4, execute their IA/IB/DA/DB LDM or STM without `!`, and assert the base remains equal to its snapshot. All metadata is `writeback: false`. The checker requires this source relationship; the builder independently checks each real assembled opcode has W=0, the correct addressing mode and L bit, Rn=r4, and register list `{r0,r1}`. Eight per-row `!` mutations, plus base-oracle and metadata mutations, are refused.

**Evidence:** `diagnostics/armv4t/cases.json:8050, 8216, 8382, 8548, 8714, 8837, 8960, 9083`; `cmake/CheckArmCoverage.cmake:364-376`; `cmake/BuildArmDiagnostics.cmake:939-961`; `tests/gates/coverage_case.cmake:224-236`; `build/phase05-plan19/review-fix/tracer-opcodes.json`; `build/phase05-plan19/review-fix/final-ledger-controls.log:5-14`.

### WR-03: Halfword force-user inventory required an unpredictable form — RESOLVED

**Original issue:** The Mode 3 halfword/signed inventory required post-index force-user forms, even though ARMv4T Mode 3 requires P=0,W=0 for post-index and defines P=0,W=1 as unpredictable. The old rows duplicated ordinary post-index instructions.

**Disposition:** `post-index-force-user` remains only in the Mode 2 word/byte inventory. Mode 3 now expects pre-no-writeback, pre-writeback, and post-index forms. The eight invalid rows were removed without reassigning their IDs. The checker rejects each of the eight former family/form labels when reintroduced, and no production case still claims a halfword force-user form.

**Evidence:** `cmake/CheckArmCoverage.cmake:96-110, 378-380`; `tests/gates/coverage_case.cmake:238-248`; `build/phase05-plan19/review-fix/current-counts.json`; `build/phase05-plan19/review-fix/final-ledger-controls.log:15-22`.

### WR-04: Shifter boundary rows omitted their named vectors — RESOLVED

**Original issue:** Two shifter boundary rows duplicated ordinary LSL #0 and LSR #1 tests with only operand `0x80000001` and incoming C=1.

**Disposition:** Cases `293321832` and `292551648` now each execute operands 0, 1, the sign bit, and all ones with incoming C=0 and C=1. They test immediate LSL #0 and register ROR with count zero, snapshot CPSR before checking flags, and independently derive N, Z, C, and V for each vector. The checker requires each setup/instruction/result/flag sequence; the builder checks eight real operation opcodes in each image. Fourteen clean-first mutations cover every vector per row and the C setup, result, and flags. Broken companions stop at their first failed assertion; the evidence does not claim they reached every later broken assertion.

**Evidence:** `diagnostics/armv4t/cases.json:4464, 4504`; `cmake/CheckArmCoverage.cmake:381-409`; `cmake/BuildArmDiagnostics.cmake:962-975`; `tests/gates/coverage_case.cmake:251-265`; `build/phase05-plan19/review-fix/final-ledger-controls.log:23-36`.

## Separate review limits and outstanding gates

The correction changes no CPU implementation, so this scoped re-review did not rerun native CPU tests. The prior human review flags for the decoder and signed long multiply remain open; this AI review does not satisfy either flag.

The correction report records the earlier classifier result as `INVALID_RED` with unsupported report format. This re-review does not claim supported formal TDD compliance or close the original process deviation.

No fixture admission, cross-host comparison, hardware qualification, upload, or release is claimed. Plan 21 retains cross-host comparison and admission ownership.

---

_Reviewed: 2026-11-13T05:54:40Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
