---
phase: 04-cpu-execution-core-and-headless-runner
reviewed: 2026-11-11T12:07:40Z
depth: standard
files_reviewed: 36
files_reviewed_list:
  - CMakeLists.txt
  - cmake/CheckProfile.cmake
  - cmake/CheckResult.cmake
  - cmake/GenerateFixtureIndex.cmake
  - contracts/examples/result-pass.json
  - docs/evidence-contract.md
  - docs/profile-agb.md
  - docs/verify.md
  - include/gamebloke/gamebloke.h
  - profiles/agb.json
  - runner/main.c
  - runner/result.c
  - runner/run.c
  - runner/run.h
  - runner/sha256.c
  - runner/sha256.h
  - src/cpu.c
  - src/cpu_helpers.h
  - src/inspect.c
  - src/machine.c
  - tests/core.cmake
  - tests/cpu_test.c
  - tests/fixtures/instruction_image.c
  - tests/gates.cmake
  - tests/gates/core_mutation.cmake
  - tests/gates/profile_case.cmake
  - tests/gates/result_case.cmake
  - tests/gates/runner_case.cmake
  - tests/gates/runner_metadata.cmake
  - tests/gates/runner_relocation.cmake
  - tests/hash_test.c
  - tests/inventory/asan.txt
  - tests/inventory/ci.txt
  - tests/inventory/hygiene.txt
  - tests/runner_test.c
  - tests/state_check.h
findings:
  critical: 3
  warning: 1
  info: 0
  total: 4
  necessity: 0
status: issues_found
---

# Phase 04: Code Review Report

**Reviewed:** 2026-11-11T12:07:40Z  
**Depth:** standard  
**Files Reviewed:** 36  
**Status:** issues_found

## Summary

The CPU and headless-runner implementation has three correctness defects that can produce a false guest PASS, misread unaligned Thumb word loads, or enter an exception for a condition-failed ARM instruction. The verification guide also omits the Phase 4 profile and result reason codes. No necessity findings were substantiated. No tests were run.

## Narrative Findings (AI reviewer)

## Critical Issues

### CR-01: A completed PASS mailbox can use case ID zero

**Severity:** BLOCKER  
**File:** src/inspect.c:173-180  
**Issue:** The completed-mailbox check rejects case ID zero only for FAIL. A completed PASS with case ID zero is therefore returned as GBL_MAILBOX_PASS. The runner treats that semantic state as a PASS for registered metadata (runner/run.c:299-307), even though the result contract requires every completed PASS or FAIL to have a nonzero case ID (docs/evidence-contract.md:101-104; cmake/CheckResult.cmake:241-243). The JSON validator catches this inconsistency downstream, but the producer itself must not classify the mailbox as PASS. The currently empty production fixture index reduces when this path can be reached; it does not make the mailbox parser or producer invariant correct.
**Fix:** Reject zero IDs for either completed state, then add a producer regression that observes a completed PASS with ID zero and proves it cannot return GBL_RUNNER_PASS. Keep the result-validator negative control as a separate consumer check.

```c
if (case_id == 0u) {
    return (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
}
```

### CR-02: Thumb immediate word loads do not rotate unaligned data

**Severity:** BLOCKER  
**File:** src/cpu.c:1031-1033  
**Issue:** The shared transfer helper aligns word bus reads down and applies the address-lane rotation only when rotate_word_load is true (src/cpu.c:568-571, 1627-1634). ARM LDR enables that behavior, but Thumb immediate transfers pass false for every load. For an unaligned base, a Thumb word LDR therefore returns the aligned word unchanged instead of rotating it by (address & 3) * 8 as required by the documented ARM7/GBA word-load behavior (04-GBA-NOTES.md:26). The supported-profile entry includes immediate word LDR without excluding unaligned bases (docs/profile-agb.md:189; profiles/agb.json:117).
**Fix:** Enable rotation for Thumb word loads only; byte loads and stores keep their current behavior. For example, pass load && !byte as the final argument to gbl_cpu_prepare_data_transfer. Add lane 1, 2, and 3 cases for immediate word loads, including the base-plus-offset address.

### CR-03: Conditional ARM SWI and coprocessor instructions ignore their condition

**Severity:** BLOCKER  
**File:** src/cpu.c:1086-1093  
**Issue:** ARM SWI and coprocessor encodings schedule exceptions before the decoder checks the instruction condition. A conditional SWI or coprocessor instruction whose condition fails still enters the exception path and stops the guest. The neighboring BX and branch paths correctly test the condition, and ARM SWI syntax and semantics include the condition field.
**Fix:** Check gbl_cpu_condition_passed for these ARM instructions before scheduling an exception. When the condition fails, schedule the normal sequential fetch and do not change exception state. Add a failing-condition SWI case and a failing-condition coprocessor case that assert sequential progress and no exception entry.

## Warnings

### WR-01: Verification guide omits the Phase 4 validator reason codes

**Severity:** WARNING  
**File:** docs/verify.md:66-70, 266-271  
**Issue:** The guide says the profile validator checks the Phase 3 policy fields and lists only the Phase 3 bus/init/read reason codes. The validator now also reports cpu-policy-missing, cpu-policy-source, cpu-policy-value, and cpu-policy-doc (cmake/CheckProfile.cmake:50-53). The result-validator catalog also omits the Phase 4 codes bad-semantic, status-semantic-mismatch, bad-time, time-overflow, cycles-exceed-budget, bad-termination, bad-trace, and incomplete-trace, which are implemented in cmake/CheckResult.cmake:66-74. This leaves the documented failure-code reference incomplete for current profile and runner checks.
**Fix:** Extend the profile policy-code list with the four cpu-policy codes and add the eight Phase 4 result codes to the result list.

---

_Reviewed: 2026-11-11T12:07:40Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: standard_

