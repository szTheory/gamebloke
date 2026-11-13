---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "17"
subsystem: cpu
tags: [armv4t, thumb, bl, irq]
requires:
  - phase: 05-16
    provides: Canonical bounded CPU units and Thumb stack/multiple transfers
provides:
  - Independently retired Thumb BL prefix and suffix using architectural banked LR
  - Real API IRQ boundary evidence and complete Thumb branch/SWI control tests
affects: [cpu, thumb-integration, isa-classification]
actuals:
  tokens: 8519
  tasks: 2
  commits: 3
plan_head_before: f5915a5122f350d4a3f3f785a5b9d21c9f1e1786
plan_head_after: 9c0cb78c3523d60f3902a7e14d5370eab832315a
tech-stack:
  added: []
  patterns: [Independent BL halves through existing decoded effects and fetch slots]
key-files:
  created: []
  modified: [src/cpu.c, tests/cpu_test.c, tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt]
key-decisions:
  - Architectural LR is the only state connecting BL halves; no fused instruction or pending-BL state.
  - Isolated-half probes and modulo arithmetic at unpredictable address boundaries demonstrate deterministic project policy, not a hardware oracle.
  - Existing branch and SWI behavior receives GREEN baseline and isolated fault controls without manufacturing a production regression.
requirements-completed: []
coverage:
  - id: D1
    description: BL halves independently retire, fetch, trace and stop at budget boundaries.
    verification:
      - kind: unit
        ref: build/ci/cpu_test thumb_bl
        status: pass
      - kind: unit
        ref: AppleClang ASan/UBSan cpu.thumb_bl
        status: pass
    human_judgment: false
  - id: D2
    description: Real unmasked/masked IRQ between BL halves and Thumb branch/SWI control boundaries.
    verification:
      - kind: unit
        ref: build/ci/cpu_test thumb_control
        status: pass
      - kind: unit
        ref: AppleClang ASan/UBSan cpu.thumb_control
        status: pass
    human_judgment: false
duration: 14m05s
completed: 2026-11-13
status: complete
---

# Phase 05 Plan 17: Independent Thumb BL and Control Boundaries Summary

**Thumb BL halves now retire independently through the canonical CPU unit, with banked LR and a real API IRQ boundary between halves.**

## Results

- Prefix sign extension and sequential PC/LR effects, suffix target/link and Thumb return bit, independent traces/retirement, actual target execution on the next run, and exact/one-short resource boundaries pass.
- Explicitly unmasked privileged System-mode IRQ entry after the prefix saves Thumb CPSR/SPSR and the IRQ link while preserving other banks and System LR. A following run stops before BIOS vector fetch. The masked companion preserves the entire machine and caller trace.
- Control tests cover all fourteen condition predicates over every NZCV combination and four signed offsets, unconditional branch endpoints, every SWI imm8 and reserved condition14 encoding, static B/BX/BL target guards, and atomic resource refusals.
- No second interpreter, hidden BL state, speculative instruction execution, active device, BIOS handler or new dependency was introduced. CPU requirements remain phase-wide pending; shared planning state belongs to the parent.

## Verification and Receipts

All paths below are repository-relative ignored local receipts under `build/phase05-plan17/`.

| Check | Result | Receipt |
| --- | --- | --- |
| Task 1 RED: `build/ci/cpu_test thumb_bl` | Intended missing BL decode assertions fail; zero-budget control passes. `tdd-red-evidence` returns `RED_EVIDENCE_OK`. | `task1-red.tap`, `task1-red.json` |
| Both planned configure/build/selector commands | Pass; BL 23 assertions, control 50 assertions. | `task1-final-*.log`, `task1-final.tap`, `task2-*.log`, `task2-baseline.tap` |
| Committed Task 1 tracer rerun | Pass before Task 2 expansion. | `tracer-configure.log`, `tracer-build.log`, `tracer.tap` |
| Seven disposable semantic faults and clean companions | All named faults detected; both clean selectors pass. | `controls.py`, `controls.log`, individual fault TAP files |
| Final focused native regressions | All 12 selected CPU groups pass. | `final-regressions.log`, `final-bl.tap`, `final-control.tap` |
| `/usr/bin/clang` AppleClang ASan/UBSan | All 8 selected CPU groups pass. | `final-asan-configure.log`, `final-asan-build.log`, `final-asan-tests.log` |
| Pinned Ubuntu 26.04/aarch64 GCC 15.2 compile-only | Pass at first committed GREEN and final task commit; compiles CPU and tests and links `cpu_test`. | `early-gnu.log` at `ceb5daf`, `final-gnu.log` at `9c0cb78` |
| Diff hygiene | `git diff --check` passes. No introduced stubs or additional security surface found. | Current committed diff |

Faults remove BL prefix sign extension, the suffix return bit or actual LR dependence, invert IRQ masking or condition evaluation, restrict SWI to immediate zero, and remove unconditional branch sign extension. They compile against the real test object and canonical core archive from disposable source copies; no production source is modified to create RED.

Task 2 was already GREEN: its control flow and exception API existed. The new assertions passed before any additional production change, then isolated faults demonstrated detection. No artificial RED implementation commit was created.

## Task Commits

1. `d399fa0` — Task 1 RED tests and synchronized selectors/inventories.
2. `ceb5daf` — Task 1 GREEN independent BL decode and corrected existing decode expectation.
3. `9c0cb78` — Task 2 control/IRQ tests and shared resource-boundary plumbing.

The measured three-commit range precedes this metadata commit. Token actuals use chars/4 of the realized owned implementation/test diff, rounded up. Duration starts at authoritative dispatch `2026-11-13T00:27:50.597114Z` and includes setup through final GNU completion at `00:41:55Z`.

## Deviations and Source Limits

- **[Rule 1 — contradictory old control]** The old decode test called `0xF000` valid but unsupported. It now asserts successful prefix retirement alongside the unchanged real ARM UND witness. No reserved, unpredictable, extension or absent-coprocessor encoding replaces it as a supposed legal unsupported witness. Final public classification/profile/result coherence remains Plan 18.
- **[Rule 2 — verification registration]** Registered both new selectors in CTest and sorted CI/ASan inventories, as necessary to exercise the plan through configured targets.
- Initial paired-stream setup cleared only validity flags, then retained the sequential-fetch marker. The CPU correctly rejected that inconsistent empty pipeline. Tests now clear both full slots and the marker. These setup failures were retained in `task1-diagnose.*` and earlier GREEN receipts; they are not additional RED evidence or a production defect.

[ARM DDI0100I A7.1.17](https://documentation-service.arm.com/static/5f8dacc8f86e16515cdb865a) supplies the BL high/low offset and link rules. Its pairing and memory-boundary restrictions classify isolated halves and overflowing targets as potentially unpredictable. Tests of these cases establish Gamebloke's explicit independent-half/modulo policy only. Existing checked static refill refusals remain intact. Timing and request classes remain the abstract uniform-bus model, without hardware timing qualification.

## Remaining Work

No unresolved Plan 17 defect or required skipped check. Parent owns independent review, shared STATE/ROADMAP/requirements bookkeeping, Plan 18 classification/profile synchronization, and full Thumb integration afterward. Full inventories and hosted CI were intentionally not started here under the accepted focused verification cadence. Existing hosted-red stop and all rights/publication gates remain in force. No user setup is required.

## Self-Check: PASSED

All five modified implementation/test files, both selectors' receipts, RED evidence, controls, native/sanitizer logs and both GNU logs exist. All three task commits are ancestors of final task HEAD, and the persisted ledger measures three commits. Owner-managed dirty files were neither staged nor changed by this plan.

## Root reviewed closeout

[Independent standard-depth review](05-17-REVIEW.md) is clean, including the BL/IRQ boundary and project-policy labels. Root reran BL/control/decode selectors successfully. Current schema/UI gates do not block; codebase drift is explicitly skipped without STRUCTURE.md. Plan18 owns additive stop reasons and coherent profile5 before full Thumb/compiler/sanitizer inventory integration. No newer hosted evidence is claimed. Root saved dispatch-to-terminal observation950s (includes metadata handoff after the summary's14m05s verification interval), receipt `build/phase05-plan17/dispatch.json`.
