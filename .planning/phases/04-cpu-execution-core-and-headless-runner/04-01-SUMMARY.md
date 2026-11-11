---
phase: 04-cpu-execution-core-and-headless-runner
plan: "01"
subsystem: cpu
tags: [c17, arm7tdmi, bus, bounded-run]
requires:
  - phase: 03-bus-api-memory-map-and-rom-loading
    provides: [checked bus reads, machine lifecycle, copied ROM loading]
provides:
  - Deterministic direct-start ARM CPU state with explicit banks and pipeline.
  - Bounded bus-backed ARM MOV-register and B execution path.
  - CPU lifecycle, budget, equality, and digest witnesses.
affects: [phase-04-cpu-execution-core-and-headless-runner, phase-05-diagnostics]
actuals:
  tokens: 10116
  tasks: 1
  commits: 2
commits: 2
plan_head_before: 58e61a89d00b82763db1ab3faa0322869e79cd8d
plan_head_after: d6b5b4b01fae70388415e7a3bfbbca00cffeac7a
tech-stack:
  added: []
  patterns:
    - One decoded unit drives both bus/cycle preflight and execution.
    - Every fetch uses the canonical bus as the cycle and open-bus authority.
key-files:
  created: [src/cpu.c, tests/cpu_test.c]
  modified:
    - include/gamebloke/gamebloke.h
    - src/machine.c
    - tests/state_check.h
key-decisions:
  - "Direct start uses ARM/System, CPSR 0xDF, PC 0x08000000, zero banks, and an empty pipeline as project policy."
  - "A refill costs two bus reads, sequential MOV costs one fetch, and taken B refills with two reads; these are project cycle costs, not hardware timing claims."
requirements-completed: []
requirements-partial: [CPU-03, RUN-01]
coverage:
  - id: D1
    description: Direct-start state and a real bus-backed ARM MOV/B pipeline path.
    verification:
      - kind: unit
        ref: tests/cpu_test.c (tracer and lifecycle selectors)
        status: pass
    human_judgment: false
  - id: D2
    description: Whole-unit cycle bounds, overflow preflight, and unchanged state on refusal.
    verification:
      - kind: unit
        ref: tests/cpu_test.c (budget selector)
        status: pass
    human_judgment: false
duration: not_measured
completed: 2026-11-10
status: complete
---

# Phase 04 Plan 01: First Bus-Backed Bounded ARM Execution Summary

**Fresh machines now start in the declared direct-start state and execute a bounded ARM MOV/B slice through real bus fetches.**

## Accomplishments

- Added per-instance ARM register banks, SPSRs, two explicit pipeline slots, stop state, and the bounded `gbl_cpu_run` contract.
- Implemented shared decode/preflight/execute for empty-queue refill, unshifted register MOV, and B, with PC reads at current address +8.
- Extended lifecycle equality and fixed-order digest coverage for every CPU field; tests cover reset, ROM replacement/refusal, exact and one-short bounds, and counter/address overflow.

## Task Commits

- RED: `1b8b4a1` — test(04-01): add failing direct-start CPU assertion
- GREEN: `d6b5b4b` — feat(04-01): implement bounded bus-backed ARM tracer

## Verification

The RED run compiled and executed the TAP target. It failed on the intended missing behavior (CPSR and execution address were zero); `gsd-tools check tdd-red-evidence` returned `RED_EVIDENCE_OK`.

The focused C17 build passed all 18 assertions across `tracer`, `lifecycle`, and `budget`. The same selectors passed with the project warning set and `-Werror`, and under ASan/UBSan using `-fsanitize=address,undefined -fno-sanitize-recover=all -g -fno-omit-frame-pointer`. Measured compile-and-run times were 0.93 s focused, 0.75 s with warnings, and 1.38 s sanitized.

```sh
cmake -E make_directory build/phase04-tracer
cc -std=c17 -DGBL_VERSION_STRING='"0.0.0"' -Iinclude src/version.c src/machine.c src/bus.c src/loader.c src/cpu.c tests/cpu_test.c -o build/phase04-tracer/cpu_test
build/phase04-tracer/cpu_test tracer
build/phase04-tracer/cpu_test lifecycle
build/phase04-tracer/cpu_test budget
```

## Scope and Requirement Status

This plan implements the first ARM tracer only. Other instructions stop explicitly; Thumb execution, the JSON CLI result, full Phase 4 ISA and runner integration remain for later plans. CPU-03 and RUN-01 are therefore partially addressed, not complete. The synthetic in-memory program used ordinary loader allocation; no BIOS or private ROM was executed.

## Deviations and Issues

None from the plan. The strict warning build first identified an unused test helper; it was removed and the existing ownership helper was exercised, after which the warning build passed.

No TODO or placeholder stubs were found in the changed files. Unsupported instructions stop explicitly as the planned slice boundary. No new security trust boundary was introduced.

## Next Step

Plan 04-02 can register the CPU source and selectors in the configured build/test lanes. Phase state, roadmap, and requirement bookkeeping remain with the parent executor.

## Performance

- **Duration:** Not measured; the executor start timestamp was not captured.
- **Completed:** 2026-11-10 (America/New_York)
- **Tasks:** 1
- **Files modified:** 5

---
*Phase: 04-cpu-execution-core-and-headless-runner*
*Plan: 01*

## Self-Check: PASSED

All five planned source/test files and this summary exist, and both RED/GREEN commits are ancestors of the current branch head.
