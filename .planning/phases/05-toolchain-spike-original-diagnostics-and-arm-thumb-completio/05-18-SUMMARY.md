---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "18"
subsystem: cpu
tags: [armv4t, classification, profile, runner]
requires:
  - phase: 05-17
    provides: Complete canonical ARM/Thumb family implementations and independent BL halves
provides:
  - Append-only unpredictable and later-architecture stop reasons through the real runner writer
  - Exact-core decode audit and coherent profile5 policy, narrative and controls
affects: [cpu, profile, diagnostic-integration]
actuals:
  tokens: 21359
  tasks: 2
  commits: 5
plan_head_before: 94e80eff754f9f33e932faa9013d7609ea1484e4
plan_head_after: e20ee01936aa7d2bb61b94f9da1c014015a8f67a
tech-stack:
  added: []
  patterns: [Append-only public classification, exact policy tables and clean-first controls]
key-files:
  created: []
  modified: [include/gamebloke/gamebloke.h, src/cpu.c, runner/run.c, runner/result.c, tests/cpu_test.c, tests/runner_test.c, profiles/agb.json, docs/profile-agb.md, cmake/CheckProfile.cmake, tests/gates/profile_case.cmake, tests/gates/result_case.cmake, docs/evidence-contract.md, tests/core.cmake, tests/gates.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt]
key-decisions:
  - Preserve all old enum values and result/1 vocabulary; existing descriptive reasons already suffice.
  - Preserve tested SWP/SWPB base/source/destination alias snapshots as explicit project policy, alongside the short-MUL alias and other named executing exceptions.
  - Recognized BLX-immediate reporting precedes generic NV as project reporting policy; no later instruction executes.
requirements-completed: []
coverage:
  - id: D1
    description: Exact-core exceptional/refusal causes reach CPU state, shared runner classification and actual serialized claim/time.
    verification:
      - kind: unit
        ref: build/ci/cpu_test isa_classification
        status: pass
      - kind: integration
        ref: build/ci/runner_test and runner.result_contract
        status: pass
    human_judgment: false
  - id: D2
    description: Profile5 concrete fields, scope, source labels and stop reasons remain synchronized and reject planted drift.
    verification:
      - kind: integration
        ref: profile.nc.cpu_policy_value and profile.nc.cpu_policy_doc
        status: pass
    human_judgment: false
duration: 22m47s
completed: 2026-11-13
status: complete
---

# Phase 05 Plan 18: Exact-Core Classification and Profile 5 Summary

**The CPU distinguishes unpredictable operands, recognized later encodings, UND and absent coprocessors; profile5 describes the implemented families and their explicit project policies.**

## Results and commits

| Task | Commits | Result |
| --- | --- | --- |
| 1 classification | `16c65d5` RED, `408b584` GREEN, `e227599` audit refinement | New run reasons10/11 and CPU stops12/13 append without renumbering. Classification, bank/SPSR/LR, no-effect refusal, complete opcode trace and actual runner serialization witnesses pass. |
| 2 profile5 | `bcfa1c6` RED, `e20ee01` GREEN | 26 CPU policy fields, 20 direct-start rows, 13 supported/6 excluded scope rows and six stop-reason rows are pinned and synchronized. |

No fabricated legal unsupported witness was introduced. Existing unsupported ABI values and static target refusals remain. SWI preserves ARM imm24 and all Thumb imm8 values in full instruction/exception trace context. SWI/UND/absent-coprocessor witnesses enter the proper exception bank and stop before fetching vector bytes; none claims handler continuation or PASS. The seven-case API-to-classifier/writer matrix is lower-boundary evidence, separate from fixture admission and the real CLI result-contract gate.

## Source audit

Read local primary-source extracts `build/phase04-research/arm/ARM7TDMI-DDI0029G.txt` §1.4/Tables1-2..1-7 and CPU timing sections, and `ARM-DDI0100I.txt` A3.16/A4–A7 operand/version rules. GBATEK3.06 supplies selected GBA reference policies, not hardware measurements. The exact committed source/ledger hashes and crosswalk are in [durable decode-source audit](05-18-DECODE-AUDIT.json).

| Decoder region | Source/form audit and existing native seams |
| --- | --- |
| ARM conditions/DP/Operand2 | All16 operations, rotated immediates and immediate/register shifts; `arm_alu`, `arm_shifts_conditions`, `arm_pc_return`. NV and malformed allocated PSR/BX/DP patterns now have honest causes. |
| ARM multiply/status | Six multiplies/S variants, operand restrictions, PSR masks/privilege/SPSR; `multiply_signed_timing`, `multiply_unsigned`, `psr`. |
| ARM control/PC | B/BL/BX and architectural PC contexts/returns; `decode`, `arm_pc_return`, `loaded_pc`. Existing static target limits retained. |
| ARM transfers | Mode2 P/U/B/W/L, register shifts/T forms, Mode3 signed/subword forms; `arm_transfer`, `arm_subword`, `arm_subword_alignment`, `loaded_pc`. |
| ARM blocks/swap | IA/IB/DA/DB, S/user-bank/return/list edges and ordered SWP/B; `arm_block*`, `slots`, `slots_bounds`. Malformed SWP allocated bits stop as unpredictable. |
| Thumb formats1–5 | Immediate/register ALU, shifts and legal high-register operations; `thumb_alu`, `thumb_high`. |
| Thumb formats6–12 | PC-relative, register-offset, signed, immediate-offset, halfword, SP-relative transfers and PC/SP address formation; `thumb_transfer`, `thumb_sp`. |
| Thumb formats13–19 | SP adjustment, PUSH/POP, STMIA/LDMIA, conditional B, SWI, unconditional B and BL; `thumb_sp`, `thumb_stack`, `thumb_multiple`, `thumb_bl`, `thumb_control`. |
| Exceptions/extensions/holes | `isa_classification`, `no_bios`: allocated later BLX/BXJ/BKPT/CLZ/DSP/UMAAL/doubleword/MCRR/MRRC/recognized REV patterns, reserved UND, malformed allocated encodings and absent coprocessor are distinguished. |

The crosswalk accounts for all357 existing ledger rows across35 family groups. It makes **zero executed-original-case claims**: the356 pending rows, frozen producer inputs, toolchain pins and two admitted smoke ROM rows are unchanged. No legal missing family was found by this source audit; full original diagnostic execution/admission remains later work.

## Verification receipts

All paths below are ignored local receipts under `build/phase05-plan18/`; no private inputs were accessed.

- Task1 behavioral RED: `task1-red.tap` and `task1-red-evidence.json`, classifier `RED_EVIDENCE_OK`; intended NV/public-cause failure, clean exception companions passed. Task2 version4→5 assertion: `task2-red.xml`, `task2-red-evidence.json`, `task2-red-verdict.json`, also `RED_EVIDENCE_OK`. A preliminary unchanged-value mutation was invalid setup, not counted as RED.
- `cmake --preset ci && cmake --build --preset ci`, `build/ci/cpu_test isa_classification`, `build/ci/runner_test`: final39 CPU assertions and24 runner assertions pass (`final-classification.tap`, `final-runner.tap`). Postcommit task1 tracer also passed before expansion.
- Existing `CheckResult.cmake` accepts the new descriptive GUEST_STOP tokens without schema/vocabulary edits: `cmake -DCASE=instruction_reasons -DSRC=. -DBIN=build/ci -P tests/gates/result_case.cmake` (`result-existing-contract.log`).
- Both planned profile controls and direct validator pass: `task2-values.log`, `task2-docs.log`, `task2-profile.log`. Controls cover missing/type/value/count/version/scope/source inflation, every new field's document drift, reason drift and false BIOS/DMA claims, each after a clean companion.
- Focused native CPU/runner/profile/result regressions:20 passed at task1,77 at initial profile integration, final79 passed (`task1-final-native.log`, `task2-related-native.log`, `final-native.log`). Final run took79.76s. This is a focused selection, not the full canonical inventory.
- AppleClang ASan/UBSan: task1 eight and final eight selected CPU/runner checks pass (`task1-asan.log`, `final-asan.log`; final21.12s). Initial extra runner-contract attempt visibly refused a missing unbuilt production executable; building its required target resolved setup before the final successful run.
- Early GNU15.2 Linux arm64 compile-only at `408b584`: `sh build/phase05-plan13/root-gnu-build-only.sh` (`task1-gnu-build-only.log`), CPU target only. Final pinned GNU compile-only at `e20ee01`: `sh build/phase05-plan18/final-gnu-build-only.sh` (`final-gnu-build-only.log`), CPU tests, runner tests and both runner targets. The ignored copy only expands the original target list. Both exit0; no GNU test execution is claimed.
- `git diff --check`, sorted edited inventories and unchanged diagnostics/manifest/toolchain inputs pass. No tracked file deletions or new unfinished implementation stubs were introduced.

## Decisions, deviations and remaining work

- Necessary ancillary synchronization: runner result limits, evidence guide, selector/result registration and sorted inventories. Result/1 vocabulary/schema stayed unchanged. Shared STATE/ROADMAP/requirements/config/routing bookkeeping remains parent-owned and untouched.
- [Rule3 — blocking verification] The existing cancellation test could SIGINT a cold child before handler installation. Preserve SIG_IGN across exec, then resend within the unchanged watchdog; successful cancellation still requires the actual CLI handler and serialized CANCELLED result. No production handler or timeout limit changed.
- Source audit exposed SWP/SWPB Rn=Rd/Rn=Rm as ARM ARM-unpredictable. Parent directed retaining the already-tested snapshots and labeling them project policy. Profile5 also explicitly names short MUL alias, selected ARM list rules and isolated/modulo BL behavior as exceptions to the general named-stop policy. Thumb same-register MUL, empty lists and STM base-middle/last still stop before effects.
- Source-local costs/flags, loaded-PC mask/wrap, static refusal boundaries, one-cycle bus costs, unqualified prefetch/waits/latches and store-trace bus-result-zero remain explicit. No BIOS/device/hardware/game/release claim follows.
- Native full inventories, post-review compiler/sanitizer integration, original diagnostic implementation/admission and phase-wide requirement completion belong to the parent/later plans. No hosted run, publication, Phase19 execution, new dependency or rights expansion occurred.

Actuals are85,436 realized implementation-diff characters/4 =21,359 tokens and five measured commits before this metadata commit, from the persisted dispatch-authoritative base. Timing is dispatch00:49:29Z to final-source receipts01:12:16Z, excluding subsequent summary/review.

## Self-Check: PASSED

All16 changed implementation paths and the summary exist; all five listed commits are ancestors of HEAD. Final source receipts identify `e20ee01`, source/ledger audit hashes match disk, no task changes remain uncommitted, and parent-owned dirty files were preserved.

## Root reviewed implementation closeout

Independent review found ARM condition classification occurring before annulment, plus incorrect Thumb format grouping. Both are fixed and re-reviewed clean at `a30f05e`; [review](05-18-REVIEW.md), [fix evidence](05-18-REVIEW-FIX.md). New regression cases fail on the old source and pass with the shared annul guard; passing-condition stop companions remain intact. Focused native11 and AppleClangASan11, profile validator and clean-first controls, and pinnedGNU CPU/runner compilation pass. Full current-source integration remains pending.

The logic-fix human-review flag remains for the existing phase acceptance/UAT; AI re-review does not satisfy it. Source/ledger crosswalk is retained compactly in05-18-DECODE-AUDIT.json, joining exact original case forms from the unchanged hash-pinned ledger; no original case-execution claim is added. Root restored exact pre-extension dirty owner config after the two approved stronger CPU batches; default executor again resolvesLuna/xhigh, both runtime maps preserved.

## Full current-source Thumb/API/profile integration

All four lanes ran sequentially at frozen HEAD `e34d94485d8f8ffc35b57f0bfacc8ed1be469d08`; wrapper verifies unchanged HEAD after every lane.

| Lane | Inventory and audit | Main CTest seconds |
| --- | --- | --- |
| Native canonical CI | 354 ran, one permitted owner-local signature skip; post audit passes | 185.16 |
| Native AppleClang ASan/UBSan | 97 ran, zero skips; post audit passes | 124.26 |
| Pinned Ubuntu26.04/aarch64 GCC15.2 | 354 ran, one permitted owner-local signature skip; post audit passes | 241.98 |
| Pinned Ubuntu26.04/aarch64 Clang21 ASan/UBSan | 97 ran, zero skips; post audit passes | 467.32 |

Receipts: `build/phase05-plan18/integration-v2/integration.log`, `integration-head.txt` and per-lane logs; wall02:01:13Z–02:19:43Z includes setup/builds. The initial native run caught an absolute home path in the new review-fix report; corrected to repository-relative prose, targeted hygiene passed, then reran the complete inventory. Original failed receipts remain under `build/phase05-plan18/`. No hosted run or new hosted stop occurred.

Current schema/UI gates do not block; codebase drift is explicitly skipped without STRUCTURE.md. Source implementation and profile integration are complete locally. Original guest diagnostic source/execution/admission remains Plan19 onward. Human review of the decoder logic fix stays an explicit phase acceptance item. No phase-wide CPU or diagnostic requirement is marked complete here.
