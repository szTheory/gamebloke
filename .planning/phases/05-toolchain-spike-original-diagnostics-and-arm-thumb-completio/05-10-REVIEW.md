---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
reviewed: 2026-11-12T17:16:23Z
depth: deep
files_reviewed: 5
files_reviewed_list:
  - src/cpu.c
  - tests/cpu_test.c
  - tests/core.cmake
  - tests/inventory/ci.txt
  - tests/inventory/asan.txt
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Phase 05 Plan 10: Code Review Report

**Reviewed:** 2026-11-12T17:16:23Z
**Depth:** deep
**Files Reviewed:** 5
**Status:** clean

## Summary

Re-reviewed the plan 10 source scope and the fixes through `99d036a`. All three findings from the earlier plan-scoped review are resolved, and this pass found no new issues. The two store operand guards now refuse the ARM UNPREDICTABLE cases before effects; ARM7TDMI DDI0029G still does not specify a core-specific result for those aliases. The LDR-PC wrap exception accepts only the exact ARM `0xFFFFFFFC`/`0` pipeline pair and stops before boundary instruction effects. This is a plan-scoped review, not a Phase 05 completion claim.

No tests were run during this re-review. The targeted pre-fix negative-control logs were inspected and fail on the assertions for each finding. The fix report records passing native CI and AppleClang ASan/UBSan runs; pinned Linux runs remain root-owned and are not represented here.

## Narrative Findings (AI reviewer)

No current findings. The three earlier findings are retained below as resolved history.

## Resolved Findings

### CR-01: **BLOCKER** — Store writeback alias is refused

**Status:** RESOLVED
**Files:** `src/cpu.c:1596-1608`; `tests/cpu_test.c:2658-2688`

The guard now rejects `writeback && Rd == Rn` for loads and stores before planning effects. The former positive `STR r0, [r0], #4` oracle was replaced by legal STR/STRB `Rd == Rn` cases without writeback; tests add atomic refusals for pre-indexed and post-indexed STR and STRB aliases. The refusal helper checks canonical state, machine identity, trace metadata, and trace bytes.

ARM DDI0100I §§A4.1.99–A4.1.100 classify these store aliases as UNPREDICTABLE. ARM7TDMI DDI0029G §§6.7–6.8 describe generic transfer timing but do not state a core-specific alias result; no hardware behavior is inferred. The pre-fix `cr01-red.log` control fails exactly the four new alias-refusal assertions.

### CR-02: **BLOCKER** — STRB with PC source is refused

**Status:** RESOLVED
**Files:** `src/cpu.c:1605-1608`; `tests/cpu_test.c:2672-2698`

Byte transfers with `Rd == R15` now take the unpredictable refusal path for both directions. The atomic refusal tests cover both LDRB-to-PC and STRB-from-PC; the legal word STR-PC test remains. ARM DDI0100I §A4.1.100 marks STRB with `Rd == R15` UNPREDICTABLE, and DDI0029G §6.8 provides no special STRB-PC result. The pre-fix `cr02-red.log` control fails on the STRB-PC refusal assertion.

### WR-01: **WARNING** — LDR-PC refill wrap now has an explicit next-run boundary result

**Status:** RESOLVED
**Files:** `src/cpu.c:651-681`, `src/cpu.c:1923-1932`; `tests/cpu_test.c:3002-3078`

Pipeline readiness accepts the modulo pair only when ARM state is active and the addresses are exactly `0xFFFFFFFC` and `0`; normal validity checks still require both slots, correct widths/alignment, execution-address agreement, and sequential state. Decode recognizes that same pair and returns the existing unpredictable/unsupported stop before the instruction can create cycle, trace, bus, register, or retirement effects. Other overflowing address pairs remain rejected, and the static B/BX scheduling guards are unchanged.

The continuation test checks the `0xFFFFFFFC`/`0` refill, then requires the next run to stop with zero cycles and instructions. It compares full canonical state and instance identity (allowing only the documented stop fields to change) and verifies the prior caller trace metadata and bytes remain unchanged. ARM DDI0100I §A2.7.1 classifies sequential execution across this boundary as UNPREDICTABLE; this is the emulator's explicit stop policy, not a hardware-accuracy claim. The pre-fix `wr01-red.log` control fails on the boundary assertion.

---

_Reviewed: 2026-11-12T17:16:23Z_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
