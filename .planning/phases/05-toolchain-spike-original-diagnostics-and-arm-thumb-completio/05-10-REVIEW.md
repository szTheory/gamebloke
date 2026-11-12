---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-12T16:53:46Z
depth: deep
files_reviewed: 5
files_reviewed_list:
  - src/cpu.c
  - tests/cpu_test.c
  - tests/core.cmake
  - tests/inventory/ci.txt
  - tests/inventory/asan.txt
findings:
  critical: 2
  warning: 1
  info: 0
  total: 3
  necessity: 0
status: issues_found
---

# Phase 05 Plan 10: Code Review Report

**Reviewed:** 2026-11-12T16:53:46Z  
**Depth:** deep  
**Files Reviewed:** 5  
**Status:** issues_found

## Summary

Reviewed the plan 10 changes from `32bdd5b` through `dbeff1b`. Traced immediate and scaled-register transfers, captured operands and writeback, bus data alignment, loaded-PC result-to-refill resolution, whole-unit preflight, and test registration. Checked architectural restrictions against the cached ARM ARM DDI0100I and the loaded-PC path against the existing pipeline consumer. Found two unpredictable store forms that execute effects, including one positively asserted by a test, plus a pipeline-consistency defect at the documented modulo refill boundary. Tests were not run. This plan-scoped review does not establish full Phase 05 completion.

## Critical Issues

### CR-01: **BLOCKER** — Store writeback alias is executed as a valid transfer

**File:** `src/cpu.c:1591-1595`; `tests/cpu_test.c:2553-2575`
**Severity:** BLOCKER
**Issue:** The guard at line 1591 applies only to loads. The test encodes `STR r0, [r0], #4` and expects the old base value to be stored before writeback. ARM DDI0100I §A4.1.99 (pp. A4-193–194) says writeback with `Rd == Rn` is UNPREDICTABLE for STR; §A4.1.100 (pp. A4-195–196) says the same for STRB. I checked the exact-core ARM7TDMI TRM DDI0029G §§6.7–6.8 (pp. 6-12–6-14): it describes load/store cycles and simultaneous store/writeback timing, but does not specify the result of this same-register alias. Its glossary defines UNPREDICTABLE as a result that cannot be relied upon. So the positive test is only a chosen emulator behavior, not evidence of an ARM7TDMI-specific result. The decoder performs the bus write and base update instead of refusing the architectural UNPREDICTABLE form under this project's refusal policy.
**Fix:** Apply the `writeback && data_register == base_register` refusal to stores as well as loads, before adding any bus slot or effect. Replace the positive alias test with an atomic refusal assertion that checks machine state and trace-buffer preservation.

### CR-02: **BLOCKER** — STRB with PC source is not refused

**File:** `src/cpu.c:1591-1595`, `src/cpu.c:1624-1628`; `tests/cpu_test.c:2642-2668`
**Severity:** BLOCKER
**Issue:** The guard rejects only `load && byte && data_register == 15`. For `STRB` with `Rd == R15`, decode continues, reads PC using the store-source context, and writes its low byte. ARM DDI0100I §A4.1.100 (p. A4-195) marks R15 as the STRB source UNPREDICTABLE. ARM7TDMI DDI0029G §6.8 (p. 6-14) gives generic store-cycle data flow but no STRB-PC-specific result that would justify selecting one behavior. The existing negative test covers LDRB only, so it misses this path.
**Fix:** Refuse byte transfers with `data_register == 15` regardless of load/store direction before planning effects. Add an atomic STRB-to-PC refusal case to the transfer guard tests.

## Warnings

### WR-01: **WARNING** — Wrapped LDR-PC refill creates a pipeline pair the next decode rejects

**File:** `src/cpu.c:604-620`, `src/cpu.c:651-669`, `src/cpu.c:1909-1911`; `tests/cpu_test.c:2970-3002`
**Severity:** WARNING
**Issue:** The new policy creates an LDR-PC refill pair with first address `0xFFFFFFFC` and second address `0`, and the test checks that state. On the next `gbl_cpu_run`, `gbl_cpu_pipeline_is_ready` calls checked `gbl_cpu_address_add(0xFFFFFFFC, 4, ...)` and rejects the pair; `gbl_cpu_decode` then returns false before it can apply any explicit boundary policy. This is a pipeline pair the implementation itself produced but its next-run consumer labels invalid. The wrap test stops after the load and misses the inconsistency. ARM DDI0100I §A2.7.1 (pp. A2-30–31) says sequential execution across this boundary is UNPREDICTABLE; that does not require the emulator to execute through it, but the current generic invalid-state failure is not the documented loaded-PC wrap policy.
**Fix:** Make the consumer handle the modulo-adjacent pair consistently with the chosen loaded-PC policy, then stop or continue at the architectural unpredictable boundary through an explicit result. Add a second `gbl_cpu_run` check after the `0xFFFFFFFC` target that asserts the selected boundary outcome rather than `invalid CPU state`; retain static B/BX guards.

---

_Reviewed: 2026-11-12T16:53:46Z_  
_Reviewer: the agent (gsd-code-reviewer)_  
_Depth: deep_
