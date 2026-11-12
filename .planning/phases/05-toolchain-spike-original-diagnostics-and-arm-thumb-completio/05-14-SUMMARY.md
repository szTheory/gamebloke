---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "14"
subsystem: cpu
tags: [arm7tdmi, armv4t, thumb, arithmetic, banked-registers]
requires:
  - phase: 05-13
    provides: bounded canonical CPU units and ARM integration baseline
provides:
  - Thumb formats 1–4 arithmetic, shifts and all sixteen ALU operations
  - Thumb high-register ADD/CMP/MOV and PC/BX operand coverage
  - Independent semantic, bank, next-call and atomic-capacity tests
affects: [05-15, 05-17, 05-18, cpu-diagnostics]
actuals:
  tokens: 9148
  tasks: 2
  commits: 4
  plan_head_before: 946e4ec8561d832cf1bcd865d775efd03bdbb43e
  plan_head_after: 35c42856ddf37ba3e16c5879c5ad483ca6716bfd
tech-stack:
  added: []
  patterns: [captured Thumb operands in canonical bounded units, literal result and flag vectors]
key-files:
  created: []
  modified: [src/cpu.c, tests/cpu_test.c, tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt]
key-decisions:
  - "Thumb MUL termination uses original Rd; identical Rd/Rm is unpredictable before ARMv6 (DDI0100I A7.1.45). Signed termination and incoming C preservation retain project policy."
  - "Reject low/low high-register ADD/CMP/MOV and CMP with PC as first operand under ARMv4T; ADD/MOV PC retain T while only BX interworks."
requirements-completed: []
coverage:
  - id: D1
    description: "Legal Thumb arithmetic/shift/ALU results, flags and internal work."
    verification:
      - { kind: unit, ref: "build/ci/cpu_test thumb_alu", status: pass }
      - { kind: unit, ref: "native AppleClang ASan/UBSan cpu.thumb_alu", status: pass }
    human_judgment: false
  - id: D2
    description: "High-register bank visibility, PC refills, BX, ARMv4 restrictions and atomic bounds."
    verification:
      - { kind: unit, ref: "build/ci/cpu_test thumb_high", status: pass }
      - { kind: unit, ref: "native AppleClang ASan/UBSan cpu.thumb_high", status: pass }
    human_judgment: false
duration: "15min00s from parent dispatch through final verification; summary commit excluded"
started: "2026-11-12T22:12:57.548777Z"
verified: "2026-11-12T22:27:58Z"
completed: 2026-11-12
status: complete
---

# Phase 05 Plan 14: Thumb arithmetic and high registers Summary

**Thumb formats 1–5 now execute their legal ARMv4T arithmetic/register forms through the existing decode, preflight and bus-owned execution path.**

## Accomplishments

- Added immediate/register shifts, register/immediate3 add/sub and all sixteen ALU operations. Defined flags and preservation masks are tested with literal independent expectations; register counts use their low eight bits.
- Added high ADD/CMP and MOV-to-PC while preserving BX guards. Register matrices cover every legal non-PC destination/source combination in all seven modes, with contrasting visible/invisible banks; all PC destination sources and all BX source registers consume actual refilled instructions on the next call.
- Added 0/31/32/33/255/256 register counts, carry/overflow sentinels consumed by conditional branches, all multiply termination lengths, alias restrictions, and exact/one-short cycle, trace, sequence, machine-counter and retirement headroom. Refusals compare canonical machine state, ownership bindings and caller-buffer canaries.

## Task Commits

| Task | RED | GREEN |
| --- | --- | --- |
| Formats 1–4 | `1419821` | `a0c4174` |
| High registers/PC and source audit | `f5228a3` | `35c4285` |

No refactor commit was needed. `actuals.tokens` is ceil(realized task diff characters / 4): 36,590 / 4. The commit count is measured from the persisted per-plan Git ledger through the final task commit; the separate summary commit is outside that interval.

## Verification

Receipts are ignored local files under `build/phase05-plan14/`; no generated content was admitted as a fixture.

- Both planned configure/build commands ran: `cmake --preset ci && cmake --build --preset ci`, followed by the owning selector. Final committed selectors pass: **36 thumb_alu assertions and 129 thumb_high assertions** (`committed-alu.tap`, `committed-high.tap`).
- Native focused regressions: `ctest --test-dir build/ci -R '^cpu\.(thumb|thumb_alu|thumb_high|arithmetic|arm_alu|arm_shifts_conditions|multiply_signed_timing|arm_pc_return|arm_block_banks|arm_block_edges|loaded_pc|decode|alignment|modes|trace_capacity|slots_bounds)$' --output-on-failure` — **16 selectors pass**, 0.31 seconds (`regressions.log`).
- Native AppleClang `/usr/bin/clang`, Debug, `-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`; configure/build only `cpu_test` under `build/phase05-plan14/asan`, then the same selector regex — **16 selectors pass**, 5.74 seconds (`asan-tests.log`, `asan/junit.xml`).
- Early GNU compile-only: `sh build/phase05-plan13/root-gnu-build-only.sh > build/phase05-plan14/early-gnu.log 2>&1` — **passes** at first GREEN `a0c4174`, pinned Ubuntu 26.04/aarch64 GCC 15.2, tracked-source clone, `cpu_test` target only. Final-source compile-only at `35c4285` also **passes** with the same helper (`final-gnu.log`).
- `python3 build/phase05-plan14/controls.py` — isolated source copies link against the configured test/core objects; clean companions pass both selectors. Eight mutants produce assertion failures: immediate MOV clears C/V, ADC ignores C, shifts reduce count modulo 32, FIQ reads User bank, register shifts omit internal work, high PC writes interwork, MUL times the wrong operand, and MUL imports ARMv6 aliases (`controls.log` and individual TAP reports).
- `git diff --check` passes. No hosted run, full inventory, private ROM, BIOS, fixture admission or publication was performed. Broad integration remains at the established Thumb completion boundary.

## TDD Gate Compliance

Both RED records returned `RED_EVIDENCE_OK` before implementation: `task1-red.json` identifies the executed LSL-zero assertion (27 new cases fail; four existing immediate cases pass); `task2-red.json` identifies missing high ADD (28 fail, 68 already-green assertions). Existing behavior was left intact and receives isolated fault controls.

The later multiply source audit corrected an initially wrong timing oracle. `mul-audit-red.json` and its unchanged TAP report show the corrected original-Rd timing assertion failing against the first implementation, with classifier approval before correction. Final task tests include the correction. The post-commit task-1 tracer reran successfully before task-2 expansion. Global `workflow.tdd_mode` is false; the task-level TDD sequence was followed.

## Decisions and sources

[ARM ARM DDI0100I](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a) A7.1.6/.23/.44 supplies high-register restrictions; A7.1.45 specifies original-Rd multiply termination and pre-v6 identical-operand unpredictability. [ARM7TDMI TRM DDI0029G](https://documentation-service.arm.com/static/5e8e1323fd977155116a3129) §6.5/.6 supplies core-local work. Signed short-multiply termination and unpredictable C preservation remain explicit project policies; no hardware timing qualification follows.

## Deviations from Plan

1. **Rule 3 — Selector registration.** Added both selectors to `tests/core.cmake` and both sorted inventories so normal and sanitizer configurations discover them. Included in the respective RED commits.
2. **Rule 1 — Thumb MUL operand audit.** The first GREEN used encoded source-register timing. Source audit found original destination timing and the pre-v6 alias restriction; corrected code and independent vectors in `35c4285`, verified with dedicated wrong-operand/alias mutants. History was preserved.

During test authoring, corrected a branch trace expectation to charge each refill slot separately and selected the genuinely overflowing static BX target. Isolated mutation setup initially rejected ambiguous match strings; narrowed them before any mutation run. These were harness corrections, not production regressions or hidden passes.

## Issues and handoff

No unresolved issue or stub prevents this plan's goal. The pre-existing ARM multiply alias policy is unchanged and was flagged to the parent for family review; this summary makes no claim about its qualification. CPU-01/CPU-02 remain phase-wide pending, so `requirements-completed` stays empty. Public profile/reason coherence belongs to Plan 18 and the valid unsupported Thumb BL-prefix control remains until Plan 17.

Parent owns independent family review, shared STATE/ROADMAP/requirements bookkeeping, trial measurement/routing restoration and the phase boundary. Those files and the existing dirty owner files were left untouched. No user setup is needed; the parent acts next.

## Self-Check: PASSED

Required summary and all five modified task files exist. All four task commits are ancestors of recorded `plan_head_after`; task paths are clean after their commits. Final committed selectors, focused native/sanitized regressions, isolated controls and early/final GNU compilation passed. No tracked files were deleted. Existing owner changes remain outside these commits.
