---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-13T09:12:01Z
depth: standard
files_reviewed: 3
files_reviewed_list:
  - cmake/CheckArmCoverage.cmake
  - diagnostics/armv4t/cases.json
  - tests/gates/coverage_case.cmake
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05: Code Review Report

**Reviewed:** 2026-11-13T09:12:01Z
**Depth:** standard
**Files Reviewed:** 3
**Status:** clean

## Summary

Re-reviewed the three Plan 20 correction paths against the original findings and the phase's encoding and arithmetic contracts. The format 5 and 14 crosswalk now derives required selector/list bits from each named form and checks that masks cover them. The shift cases and checker pin the incoming carry, operands, results, NZCV fields, probe count, and actual opcode. No active findings remain in this scoped re-review.

## Narrative Findings (AI reviewer)

No active findings.

## Resolved Findings (prior review)

### CR-01: Coverage crosswalk omitted format-defining operand bits — RESOLVED

**Original severity:** BLOCKER
**Original file:** `cmake/CheckArmCoverage.cmake:622-626`

The original checker did not enforce format 5 H1/H2 selectors or format 14 LR/PC selection. The revised crosswalk derives selector values from each named form and requires the corresponding bits in the supplied mask. It also fixes the concrete empty and maximum list boundaries and the format 15 base-in-list witnesses. The controls coordinate instruction-source and supplied-opcode mutations, then verify rejection; weak-mask mutations are rejected as well ([CheckArmCoverage.cmake](cmake/CheckArmCoverage.cmake:622), [coverage_case.cmake](tests/gates/coverage_case.cmake:333)).

The unrestricted BX form intentionally leaves its Rm selector variable. Its positive `0x4700` control remains accepted, and GNU 15.2.1 decodes `0x4718` as `bx r3`; this is a valid register selection, not an unchecked reserved field.

### WR-01: Special LSR and ASR carry cases could pass with stale carry — RESOLVED

**Original severity:** WARNING
**Original file:** `diagnostics/armv4t/cases.json:9879-9903`

Immediate LSR/ASR #32 now check both incoming C values against the carry derived from operand bit 31. Register-count-zero cases add contrasting incoming carry and sign-bit vectors while retaining the original count boundaries. The checker binds each source sequence to literal result/NZCV/opcode vectors and exact probe count ([CheckArmCoverage.cmake](cmake/CheckArmCoverage.cmake:780), [cases.json](diagnostics/armv4t/cases.json:9876)). The gate includes clean-first mutations for both carry seeds, instruction replacement, mask weakening, encoding changes, and probe omission ([coverage_case.cmake](tests/gates/coverage_case.cmake:422)).

### WR-02: Register ROR omitted zero and non-multiple over-32 boundaries — RESOLVED

**Original severity:** WARNING
**Original file:** `diagnostics/armv4t/cases.json:10062-10090`

Stable ID `1800200037` adds ROR count-zero vectors for both carry inputs and count-33 vectors whose bit-zero/result/carry outcomes differ. Its four source probes, full opcode values, results, and NZCV expectations are checked independently; the existing count-32/64 case remains. The gate rejects changes to count 33, operand bit zero, result, flags, opcode, or the whole case ([CheckArmCoverage.cmake](cmake/CheckArmCoverage.cmake:820), [cases.json](diagnostics/armv4t/cases.json:16329), [coverage_case.cmake](tests/gates/coverage_case.cmake:445)).

The correction report records the focused clean/broken guest runs and isolated carry-fault controls. This re-review did not repeat the full matrix or ledger gates.

---

_Reviewed: 2026-11-13T09:12:01Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: standard_
