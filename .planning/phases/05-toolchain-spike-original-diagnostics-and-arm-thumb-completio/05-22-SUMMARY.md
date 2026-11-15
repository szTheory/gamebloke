---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "22"
subsystem: testing
tags: [armv4t, cpu, mutation, atomicity, cmake, sanitizer]
requires:
  - phase: "05-21"
    provides: Reviewed immutable production diagnostic admission and existing native CPU selectors
provides:
  - Eight actual disposable CPU fault controls with named independent semantic failures and clean companions
  - Complete clean copied-core coverage of all 40 native selectors
  - 63 maximum-unit capacity, overflow and wrapped-address witnesses with complete refusal postconditions
affects: [05-23, 05-24, 05-25, cpu-verification, bus-contract]
tech-stack:
  added: []
  patterns: [guarded same-build object reuse, field-based trace equality, independent maximum-unit boundary matrix]
key-files:
  created:
    - .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-22-SUMMARY.md
  modified:
    - tests/gates/core_mutation.cmake
    - tests/cpu_test.c
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
    - tests/state_check.h
    - .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-CPU-NOTES.md
key-decisions:
  - "Retain the correct production implementation; establish semantic RED with actual isolated core mutants rather than regressing production."
  - "Reuse only explicitly reviewed CPU-source-only objects under existing compiler, flag, source, header and object identity guards; unknown and header mutations retain full compilation."
  - "Whole-unit atomicity depends on the current uniform bus; side-effecting or faulting devices and variable waits require a preparation/transaction contract before expansion."
requirements-completed: [CPU-01, CPU-02, DIAG-03]
actuals:
  tokens: 11022
  tasks: 2
  commits: 2
commits: 2
plan_head_before: f1fb184d8aedddd0c85d52c3ca85d992b7d95f10
plan_head_after: 2cfaba5b97bba9a8e022ab71d010d12c4c21f2aa
duration: "23m40s measured execution through Task2 commit, including review handoff"
completed: 2026-11-13
status: complete
coverage:
  - id: D1
    description: Actual CPU faults fail named independent semantic assertions while clean copied sources exercise every native selector
    requirement: DIAG-03
    verification:
      - kind: integration
        ref: "ctest cpu.nc.block_effect_order/loaded_pc/maximum_trace/multiply_flags/bl_halves/late_refusal and cpu.pos.clean; build/phase05-plan22/integration.log"
        status: pass
    human_judgment: false
  - id: D2
    description: Maximum ARM and Thumb units complete at exact headroom and refuse atomically when capacity or counters are one short
    requirement: CPU-01
    verification:
      - kind: unit
        ref: "cpu_test unit_boundaries, arm_block_bounds and slots_bounds; build/phase05-plan22/integration.log"
        status: pass
      - kind: unit
        ref: "ASan/UBSan focused controls and all native selectors; build/phase05-plan22/asan-current.log"
        status: pass
    human_judgment: false
---

# Phase 05 Plan 22: CPU Mutation and Maximum-unit Refusal Summary

**Actual CPU mutants fail independent semantic assertions, and maximum ARM/Thumb units preserve complete state on capacity refusal while exact-fit work completes.**

## Accomplishments

The new controls change actual copied production CPU source for old-bank effect
capture, result-driven PC refill, a lost maximum trace record, multiply N flags,
long accumulation carry, Thumb BL intermediate LR, a write before refusal, and
a malformed second write after the first effect. Each must compile and fail its
specific semantic assertion; unmatched anchors and compilation failures remain
setup failures. Same-source/options clean companions pass. Existing guarded
prepared objects cover unchanged translation units, including `inspect.c` for
CPU controls; only reviewed CPU-source-only mutations gain reuse eligibility.
The clean companion derives and runs all 40 selectors from the native registry,
and the native runner now checks its declared assertion count.

The shared matrix covers sixteen-register LDM/STM with PC, full FIQ-bank LDM
with SPSR return, maximum User-bank LDM, SWP, maximum Thumb PUSH/POP, wrapped
STM data addresses and a loaded-PC refill crossing the address boundary. Each
has exact-fit, one-short cycle/trace/sequence, master-clock overflow, retirement
overflow and simultaneous exact-counter cases. Refusals compare canonical
fields and all inline memory bytes, independently saved ROM bytes, ownership
identities, allocator state, options, caller image/result guards and every
trace record. Digests supplement equality. Trace comparisons use public fields
instead of padding bytes. Exact completion checks independent register/store
values, writeback/SP, ordered bus addresses/classes/times and dynamic refill.

## Verification and Performance

- Task1 configure/build and the planned focused mutation/clean CTest expression
  passed; final focused tracer time was 5.53 seconds.
- Task2's planned `cmake --build --preset ci`, `build/ci/cpu_test unit_boundaries`,
  `build/ci/cpu_test arm_block_bounds` and `build/ci/cpu_test slots_bounds` passed.
  Final focused native mutation/boundary coverage passed in 10.50 seconds.
- Native AppleClang ASan/UBSan focused controls and all-selector clean companion
  passed in 24.00 seconds. The final pre-review matrix/late-control/clean rerun
  passed in 11.45 seconds. The later duplicate typedef removal changed no
  executable behavior and was covered by the final canonical workflow.
- GNU 13.5 strict C17 compilation of all six canonical CPU core sources and the
  real test passed, followed by the matrix/block/slot selectors. This used an
  existing Linux amd64 compiler image under emulation, isolated to 4 CPUs/8 GB;
  it is local compiler evidence, not a new hosted qualification.
- After clean independent review, the single full `cmake --workflow --preset ci`
  passed in **152.383 seconds**, including the JUnit inventory check: 361 tests
  ran and one existing optional owner-local check was visibly skipped. No
  required Plan22 check was skipped. Local elapsed time is not hosted proof.
- The final workflow's new mutation controls plus complete clean companion
  consumed 6.701 seconds of sequential test time; their slowest test was
  1.698 seconds. Existing JSON, runner-contract and metadata gates dominate
  the measured path. No additional cache, dependency or concurrency change was
  warranted, and hosted limits remain unchanged.
- Fresh hygiene and `git diff --check` passed before the task commit.

Receipts are under `build/phase05-plan22/`: `task1-final-ctest.log`,
`current-focused.log`, their JUnit files, `task2-current.tap`, `task2-block.tap`,
`task2-slots.tap`, `asan-focused.log`, `asan-current.log`, `gnu-current.log`,
`integration.log`, `integration-receipt.json` and `critical-path.json`.
Each of the eight named mutant TAP runs has a `*-red.json` record and
`*-verdict.json` supported-classifier receipt reporting `RED_EVIDENCE_OK`, with
the actual target assertion and semantic assessment preserved.

`actuals.json` records the persisted ledger, exact task-file digests and 44,087
realized diff characters across the seven task files: ceiling(chars/4) is
11,022 estimated tokens. This excludes summary metadata and harness tokens.
The measured interval starts at 20:09:19 UTC and ends at the Task2 commit around
20:32:59 UTC; it includes the review handoff and excludes initial context reads.

## Task Commits

1. `9cefd37` — `test(05-22): detect representative CPU faults in isolated core copies`
2. `2cfaba5` — `test(05-22): prove maximum-unit refusal and checked-request boundaries`

The measured commit ledger ends at the second task commit; summary metadata is
committed separately. Parent owns shared state, roadmap, requirements and the
review report commit.

## Review and Issues Encountered

[Independent review](05-22-REVIEW.md) is clean with zero active findings. Its
original shallow-snapshot concern was factually incorrect and withdrawn:
`gbl_machine` embeds all five RAM regions as inline arrays, so struct assignment
copies their bytes. The actual C17 `snapshot-probe/probe.c` and `result.log`
under the receipt directory mutate each region independently; the saved byte
remains unchanged and canonical equality/digest detect each memory-only change.
Restoration passes. This was not a production defect or a snapshot redesign.
The reviewer also identified a duplicate consecutive typedef, which the parent
removed before final integration.

One early post-effect mutant used the wrong loop variable and failed compilation.
That setup failure remains in `task2-ctest.log`; correcting the disposable mutant
produced the named semantic failure and a passing clean companion. It never
counted as detection. A misplaced generated JUnit file was moved under the
ignored build directory; no generated untracked file remains from this work.

## TDD Gate Compliance

Both tasks are tagged TDD; configured project TDD mode was false. Existing
production behavior was correct and the new boundary matrix initially passed.
No production regression was introduced to manufacture RED. The actual isolated
mutants provide complete supported TAP RED receipts and clean companions; both
task commits contain tests/controls rather than a new production behavior.
No invented failing-production or RED-to-GREEN commit history is claimed. The
earlier Plan19 formal reporting deviation remains unchanged in its own record.

## Deviations from Plan

No product or architectural deviation. Local setup corrections and the duplicate
typedef cleanup are described above. No production CPU fix, producer-input
change, fixture re-admission, dependency install or hosted publication occurred.

## Scope and Remaining Work

No required Plan22 work remains. There are no new stubs, skipped required tests,
unrun required checks or additional security surfaces. The optional owner-local
content skip is inherited and was not exercised or changed.

Whole-unit atomicity depends on the current fixed-cost bus. The bus shares the
machine master clock; there is no separate persistent bus cycle counter.
Side-effecting/faulting MMIO, variable waits, arbitration or concurrent devices
require the future preparation/transaction contract documented in
[CPU notes](05-CPU-NOTES.md) before extending this guarantee. Hardware accuracy,
BIOS execution, broad compatibility and release qualification are not established.
Frozen producer inputs and admitted ROM authority remain unchanged. No Plan23
or Phase6 work was started. Parent performs the remaining shared bookkeeping;
no owner action is needed.

## Self-Check: PASSED

All seven task files and this summary exist. Both task commits are ancestors of
HEAD. The full canonical workflow receipt records exit 0, inventory validation
passed, and fresh hygiene/diff checks passed. Production core, diagnostic
producer inputs and hosted workflow files are unchanged by this plan.
