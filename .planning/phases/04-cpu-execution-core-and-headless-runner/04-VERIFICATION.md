---
phase: 04-cpu-execution-core-and-headless-runner
verified: 2026-11-11T12:53:27Z
status: gaps_found
score: 40/41 must-haves verified
covered_files:
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-01-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-01-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-02-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-02-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-03-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-03-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-04-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-04-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-05-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-05-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-06-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-06-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-07-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-07-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-08-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-08-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-09-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-09-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-10-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-10-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-11-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-11-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-12-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-12-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-13-PLAN.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-13-SUMMARY.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-FINAL-CHECKS.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-REVIEW-DISPOSITION.md
  - .planning/phases/04-cpu-execution-core-and-headless-runner/04-REVIEW.md
  - CMakeLists.txt
  - build/phase04-research/edge-coverage.json
  - cmake/CheckProfile.cmake
  - cmake/CheckResult.cmake
  - cmake/GenerateFixtureIndex.cmake
  - contracts/evidence.json
  - contracts/examples/result-pass.json
  - docs/evidence-contract.md
  - docs/profile-agb.md
  - docs/verify.md
  - include/gamebloke/gamebloke.h
  - profiles/agb.json
  - registry/disputes.json
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
  - tests/fixtures/manifest.json
  - tests/gates.cmake
  - tests/gates/core_mutation.cmake
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
covered_digest: "v4:sha256:1d72386f5b8ce1f6f838d7bcc786b5a391e459ac66dc840ede278958d2ee5f42"
behavior_unverified: 0
overrides_applied: 0
gaps:
  - truth: "The evidence guide states the exact unresolved status of every probe-taxonomy row."
    status: failed
    reason: "docs/evidence-contract.md describes five category-labeled rows as explicit, but build/phase04-research/edge-coverage.json records all 11 rows as unresolved (five category-labeled and six unclassified). This makes the documented status ambiguous."
    artifacts:
      - path: "docs/evidence-contract.md"
        issue: "Lines 239-242 say five rows are classified as explicit and only six are unresolved."
      - path: "build/phase04-research/edge-coverage.json"
        issue: "All 11 rows have status unresolved; coverage.resolved is 0."
    missing:
      - "State that all 11 rows remain unresolved: five have category labels (three CPU-04 and two RUN-01), and six are unclassified."
---

# Phase 04: CPU Execution Core and Headless Runner Verification Report

**Phase Goal:** As a core developer, I want to execute ARM7TDMI code from a fresh bus-backed instance and obtain bounded headless JSON results with separate status, semantic result, machine time, claim and limits, so that I can inspect deterministic CPU behavior and judge exactly what each diagnostic result establishes.

**Verified:** 2026-11-11T12:53:27Z
**Status:** gaps_found
**Re-verification:** No; initial verification

## User Flow Coverage

The roadmap goal is a valid MVP user story. The GSD user-story validator returned valid=true.

| Step | Expected | Evidence | Status |
| --- | --- | --- | --- |
| Start from a fresh machine and execute bus-backed ARM/Thumb instructions. | CPU state, fetches and machine time come from the canonical machine and bus. | src/machine.c, src/cpu.c; cpu.tracer, cpu.arithmetic and lifecycle witnesses. | VERIFIED |
| Bound a headless run by cycles or frames. | Run stops at its checked budget without overshoot. | runner/main.c, runner/run.c; runner boundary tests and CI inventory. | VERIFIED |
| Inspect one machine-readable result. | Status, mailbox semantics, time, claim tuple and limits remain separate fields. | runner/result.c, cmake/CheckResult.cmake; actual producer-to-checker gate. | VERIFIED |
| Decide exactly what unresolved diagnostic policy records establish. | Documentation agrees with the taxonomy status and does not imply an unresolved row is resolved. | docs/evidence-contract.md conflicts with build/phase04-research/edge-coverage.json for five category-labeled rows. | FAILED |

## Goal Achievement

The five roadmap success criteria and 35 plan-level must-have truths were checked against implementation, tests and wiring. The single additional truth derived from the phase outcome—accurate disclosure of unresolved probe status—fails because the evidence guide and taxonomy data disagree. Score: 40/41.

### Observable Truths

| # | Truths checked | Status | Evidence |
| --- | --- | --- | --- |
| R1 | ARM/Thumb direct-start executes from memory; PC reads are +8/+4; the open-bus latch is explicit; there is no decode cache/fusion; guarded shifts cover ≥32, ROR #0/RRX and overflow behavior. | VERIFIED | src/cpu.c, src/cpu_helpers.h, tests/cpu_test.c; arithmetic/tracer tests and fresh UBSan lane. |
| R2 | Modes and register banks are distinct as specified; exception entry saves CPSR and return context; BIOS-none exceptions stop before vector fetch with classified reasons. | VERIFIED | src/cpu.c, tests/cpu_test.c, tests/runner_test.c; mode/exception/no-BIOS tests and shared classifier/writer tests. External IRQ/reset/abort mappings use the CPU API seam, as documented. |
| R3 | A fresh bounded runner emits one JSON result with distinct status, semantic result, machine time, claim and limits; mailbox completion is neutral and has precedence at the budget boundary. | VERIFIED | runner/run.c, runner/result.c, src/inspect.c, cmake/CheckResult.cmake; runner producer/result-contract tests. |
| R4 | Selected instruction, bus and exception traces are bounded and ordered; capacity exhaustion is explicit and preserves previous records without yielding PASS. | VERIFIED | src/cpu.c, tests/cpu_test.c, runner/run.c; cpu.tracer and runner result-contract controls. |
| R5 | The installed CLI works after moving its prefix and matches build-tree JSON and trace output from an unrelated working directory. | VERIFIED | tests/gates/runner_relocation.cmake; runner.relocation passed and final inventory passed. |
| P01.a–c | Fresh load/reset state; ARM MOV/B bus fetches, PC and latch; cycle-cap atomicity. | VERIFIED | src/machine.c, src/cpu.c, tests/cpu_test.c; bus-backed CPU tracer and exact/one-short cycle witnesses. |
| P02.a–c | Configured/scratch builds share CPU sources; persistent fields join lifecycle/equality/digest/refusal checks; bounded ARM path remains registered. | VERIFIED | CMakeLists.txt, tests/gates/core_mutation.cmake, tests/cpu_test.c; canonical build and lifecycle/mutation controls. |
| P03.a–c | Shift/add/sub edge results and flags; branch/BX refill through bus; supported instructions execute while unsupported and undefined cases differ. | VERIFIED | src/cpu_helpers.h, src/cpu.c, tests/cpu_test.c; cpu.arithmetic and decode/alignment coverage. |
| P04.a–c | Bank isolation; absent User/System SPSR and refusal atomicity; saved exception context and stop before BIOS vector fetch. | VERIFIED | src/cpu.c, include/gamebloke/gamebloke.h, tests/cpu_test.c; cpu.modes and cpu.no_bios. |
| P05.a–b | Profile and narrative agree on direct-start/no-BIOS policy and enumerate implemented/excluded CPU scope without broad accuracy claims. | VERIFIED | profiles/agb.json, docs/profile-agb.md, cmake/CheckProfile.cmake; profile lint and negative controls in final CI evidence. |
| P06.a–c | Peek is bounded and neutral; partial/invalid mailbox values are not verdicts; stable failing case id and exact-boundary completion work. | VERIFIED | src/inspect.c, include/gamebloke/gamebloke.h, runner tests; mailbox and runner semantic witnesses. |
| P07.a–c | Category mask, guest-time ordering and capacity preflight; no partial instruction/refill effects or lost prior records; trace exhaustion stays non-PASS. | VERIFIED | src/cpu.c, tests/cpu_test.c, runner/run.c; cpu.tracer and result-contract tests. |
| P08.a–b | SHA-256 covers exactly loaded bytes and independent empty/padding/multiblock vectors; invalid spans refuse without changing output. | VERIFIED | runner/sha256.c, tests/hash_test.c; runner.hash in the fresh canonical CI evidence. |
| P09.a–b | Only validated manifest rows become production metadata; synthetic controls use the canonical runner/core with only index input changed and claim no rights. | VERIFIED | cmake/GenerateFixtureIndex.cmake, tests/fixtures/instruction_image.c, tests/gates/runner_metadata.cmake; production manifest stays empty and metadata gate passed. |
| P10.a–b | Each invocation is fresh and bounded, emits one separated JSON result; registered exact digest plus completed mailbox is required for evidence PASS, while raw input stays diagnostic. | VERIFIED | runner/main.c, runner/run.c, runner/result.c; producer tests, raw timeout/malformed outcomes and checker. |
| P11.a–c | Cycle/frame conversion is checked; trace-full/cancel/missing/refusal/timeout/guest-stop remain distinct and paths are excluded; producer tests exercise bounded outcomes. | VERIFIED | runner/run.c, tests/runner_test.c, tests/gates/runner_case.cmake; full CI and result-contract tests. |
| P12.a–c | Actual registered producer JSON passes the Phase 2 checker; semantic/time/dispute/trace mutations fail with named reasons; raw diagnostics cannot validate as evidence PASS. | VERIFIED | cmake/CheckResult.cmake, tests/gates/runner_case.cmake, contracts/examples/result-pass.json; runner.result_contract passed. |
| P13.a–c | Relocated production CLI preserves output and identity; no new fixture authority appears; final workflows/inventories cover current sources and controls. | VERIFIED | CMakeLists.txt, tests/gates/runner_relocation.cmake, docs/verify.md; relocation, inventory and final lane evidence. |
| G1 | Evidence documentation reports the current state of the 11 taxonomy rows without implying any is resolved. | FAILED | docs/evidence-contract.md:239-242 describes five as explicit; build/phase04-research/edge-coverage.json:4-110 marks all 11 unresolved and resolved=0. See structured gap above. |

## Required Artifacts

All 36 artifact entries declared across plans 04-01 through 04-13 passed the artifact verifier. Direct source review confirmed substantive implementations and wiring; the rows below group each plan’s declared artifacts.

| Plan | Declared artifacts | Status |
| --- | --- | --- |
| 04-01 | src/cpu.c; tests/cpu_test.c; include/gamebloke/gamebloke.h | VERIFIED |
| 04-02 | CMakeLists.txt; tests/gates/core_mutation.cmake; tests/cpu_test.c | VERIFIED |
| 04-03 | src/cpu_helpers.h; tests/cpu_test.c | VERIFIED |
| 04-04 | src/cpu.c; include/gamebloke/gamebloke.h; tests/cpu_test.c | VERIFIED |
| 04-05 | profiles/agb.json; docs/profile-agb.md; cmake/CheckProfile.cmake | VERIFIED |
| 04-06 | src/inspect.c; include/gamebloke/gamebloke.h | VERIFIED |
| 04-07 | include/gamebloke/gamebloke.h; src/cpu.c; tests/cpu_test.c | VERIFIED |
| 04-08 | runner/sha256.c; tests/hash_test.c | VERIFIED |
| 04-09 | tests/fixtures/instruction_image.c; cmake/GenerateFixtureIndex.cmake; tests/gates/runner_metadata.cmake | VERIFIED |
| 04-10 | runner/run.h and runner/run.c; runner/result.c; runner/main.c | VERIFIED |
| 04-11 | tests/runner_test.c; tests/gates/runner_case.cmake; tests/core.cmake | VERIFIED |
| 04-12 | cmake/CheckResult.cmake; tests/gates/runner_case.cmake; contracts/examples/result-pass.json | VERIFIED |
| 04-13 | CMakeLists.txt; tests/gates/runner_relocation.cmake; docs/verify.md | VERIFIED |

## Key Link Verification

The GSD link query was run for all 13 plans, but each declared from field is a descriptive component label rather than a relative file path, so the query returned “Source file not found (from: must be a relative file path)” for all 33 links. This is a plan-field shape limitation, not evidence of broken wiring. I traced the actual paths manually:

| Plan | Link traced | Evidence | Status |
| --- | --- | --- | --- |
| 04-01 | Machine reset → CPU direct-start state; CPU execution → bus read/write/internal cycle API. | src/machine.c, src/cpu.c, tests/cpu_test.c | WIRED |
| 04-02 | Shared source list → configured/scratch cores; persistent CPU fields → lifecycle and refusal checks. | CMakeLists.txt, tests/gates/core_mutation.cmake, tests/cpu_test.c | WIRED |
| 04-03 | Decoder → guarded arithmetic helpers; branch/BX → refill and shared cycle/trace scheduling. | src/cpu.c, src/cpu_helpers.h, tests/cpu_test.c | WIRED |
| 04-04 | SWI/undefined/coprocessor and explicit exception requests → centralized entry → stop reason. | src/cpu.c, runner/run.c, tests/cpu_test.c, tests/runner_test.c | WIRED |
| 04-05 | CPU policy ↔ profile fields ↔ documented rows and negative controls. | profiles/agb.json, docs/profile-agb.md, cmake/CheckProfile.cmake | WIRED |
| 04-06 | Runner observation → mailbox reader → bounded neutral peek; refusal → unchanged output/state. | runner/run.c, src/inspect.c, tests/runner_test.c | WIRED |
| 04-07 | Decoded unit → trace capacity preflight; actual bus results/exception snapshots → selected trace records. | src/cpu.c, tests/cpu_test.c | WIRED |
| 04-08 | Exact input span → SHA-256; refusal leaves output untouched. | runner/run.c, runner/sha256.c, tests/hash_test.c | WIRED |
| 04-09 | Manifest/registry/vocabulary validation → generated index; production/control targets share runner/core sources. | CMakeLists.txt, cmake/GenerateFixtureIndex.cmake, tests/gates/runner_metadata.cmake | WIRED |
| 04-10 | CLI options → bounded input/runner → CPU/mailbox → one result writer; metadata and exact digest gate PASS. | runner/main.c, runner/run.c, runner/result.c | WIRED |
| 04-11 | Remaining checked budget → canonical CPU units; trace-full/cancel/results → distinct status and JSON. | runner/run.c, tests/runner_test.c, tests/gates/runner_case.cmake | WIRED |
| 04-12 | Actual control-runner stdout → result checker; exact decimal and dispute fields → validation rules. | tests/gates/runner_case.cmake, cmake/CheckResult.cmake, tests/gates/result_case.cmake | WIRED |
| 04-13 | Install → moved prefix/unrelated cwd → production runner; workflow and inventories include current tests. | CMakeLists.txt, tests/gates/runner_relocation.cmake, docs/verify.md | WIRED |

## Data-Flow Trace

| Value | Source and path | Status |
| --- | --- | --- |
| PC, registers, CPSR, pipeline and machine cycles | Fresh machine initialization → canonical CPU decoder → bus accesses and preflighted cycle counters. | FLOWING |
| Trace records | Selected category mask → CPU instruction/bus/exception events → bounded caller buffer → optional JSONL writer. | FLOWING |
| Semantic result and case id | IWRAM bytes → side-effect-free peek → mailbox validation → runner JSON. | FLOWING |
| ROM digest and fixture authority | Bounded file bytes → exact-span SHA-256 → generated manifest index comparison → claim/evidence fields. | FLOWING |
| Status, time, limits and claim | Runner state/result model → one JSON writer → checker cross-validates status, semantic state, bound and manifest. | FLOWING |

The synthetic control index is intentionally used only for producer/checker controls. Production metadata comes from the canonical empty manifest, so synthetic rows do not create fixture approvals or rights.

## Behavioral Spot-Checks

| Behavior | Command/evidence | Result | Status |
| --- | --- | --- | --- |
| CPU arithmetic, modes, no-BIOS stops and trace behavior; result contract; install relocation. | ctest --test-dir build/ci -R '^(cpu\\.tracer|cpu\\.arithmetic|cpu\\.modes|cpu\\.no_bios|runner\\.result_contract|runner\\.relocation)$' --no-tests=error --output-on-failure | 6/6 passed in 1.05 seconds. | PASS |
| Current native canonical workflow. | Recorded in 04-FINAL-CHECKS.md; cmake --workflow --preset ci | Exit 0; inventory 329 expected, 328 passed and one allowed owner-local skip. | PASS |
| Fresh native ASan/UBSan workflow. | Recorded in 04-FINAL-CHECKS.md | 72/72 passed; zero skips. | PASS |
| Pinned Ubuntu GNU 15.2.0 canonical workflow. | Recorded in 04-FINAL-CHECKS.md | Exit 0; 329 expected, 328 passed and one allowed owner-local skip. | PASS |
| Hygiene and signature gates. | Recorded in 04-FINAL-CHECKS.md | Exit 0; inventory passed with the allowed owner-local skip. | PASS |

The recorded complete workflows are fresh evidence; they were not rerun for this verification. No private ROM or BIOS was executed.

## Probe Execution

| Probe | Command | Result | Status |
| --- | --- | --- | --- |
| Phase-declared/conventional probe scripts | Searched plan/summary declarations and scripts/*/tests/probe-*.sh | No probe scripts or declared script paths found; nothing to execute. | N/A |

The separate edge-coverage record contains 11 unresolved entries: five category-labeled (CPU-04 adjacency, empty, ordering; RUN-01 boundary and precision) and six unclassified (CPU-03, CPU-05, PROF-03, RUN-02, RUN-03, RUN-04). Concrete tests establish implementation behavior; they do not resolve the taxonomy metadata. The two descriptor-less standing restrictions also remain visible: rights-bearing/private inputs must not be repurposed or treated as rights-granted by synthetic metadata, and evidence must not inflate hardware, BIOS or compatibility claims.

## Requirements Coverage

| Requirement | Source plans | Status | Evidence |
| --- | --- | --- | --- |
| CPU-03 | 04-01, 04-02, 04-03, 04-05, 04-13 | SATISFIED | Explicit pipeline/PC/latch path, bus-backed execution, no cache/fusion; CPU tests and UBSan lane. |
| CPU-04 | 04-04, 04-05, 04-13 | SATISFIED | Mode/bank/SPSR and exception-return matrix; cpu.modes and cpu.no_bios. |
| CPU-05 | 04-03, 04-05, 04-13 | SATISFIED | Guarded shift and unsigned arithmetic helpers; arithmetic test and fresh UBSan lane. |
| PROF-03 | 04-04, 04-05, 04-11, 04-12, 04-13 | SATISFIED | No-BIOS classified stops. Reset/IRQ/FIQ/prefetch-abort/data-abort use the CPU API into the same production classifier/writer; guest-triggerable SWI/undefined/coprocessor paths are exercised through the runner. |
| RUN-01 | 04-01, 04-02, 04-03, 04-05 through 04-13 | SATISFIED | Fresh bounded run; one JSON result; separate time, semantic, claim and limits; raw diagnostic cannot become evidence PASS. |
| RUN-02 | 04-06, 04-07, 04-09 through 04-13 | SATISFIED | Neutral IWRAM peek/mailbox reader feeds runner semantic result. |
| RUN-03 | 04-07, 04-10 through 04-13 | SATISFIED | Category-masked bounded trace; capacity exhaustion is explicit and cannot pass. Successful write records reflect the bus API result value of zero, not the attempted payload; this documented limit does not claim a physical write latch. |
| RUN-04 | 04-13 | SATISFIED | Moved-prefix installed CLI emits the same JSON and trace from an unrelated cwd. |

No Phase 04 requirement is orphaned. The requirements ledger remains Pending until the phase workflow records its verification result; this report does not edit it.

## Anti-Patterns and Test Quality

| File/scope | Finding | Severity | Impact |
| --- | --- | --- | --- |
| Scoped source, docs and tests | No unreferenced TBD/FIXME/XXX markers, empty implementation stubs, disabled target tests or test-only skip paths found. | None | No blocker. |
| Profile, bus API and evidence docs | Placeholder wording refers to explicit project timing/latch policies and unsupported device behavior, not empty CPU/runner implementations. | Info | These limits are documented and tested as current policy, not represented as hardware truth. |
| Phase review disposition | Four recorded review findings are marked fixed; zero remain open. | None | Review disposition aligns with present code and final-check evidence. |

The named CTest cases are present in the lane inventories. The checked controls exercise negative outcomes as well as clean companions. The final source/inventory/hygiene evidence is in 04-FINAL-CHECKS.md.

## Human Verification Required

Plan 04-12 decision D3 remains pending. After the exact taxonomy wording correction and narrow re-verification, a human should judge whether docs/evidence-contract.md explains the real CLI, mailbox, metadata authority and rights/claim boundaries clearly to a core developer. The required outcome is a clear explanation of what synthetic validation does and does not establish; this is a clarity review, not a waiver of rights restrictions or a resolution of the taxonomy rows. No human acceptance is inferred here.

Other implementation limits remain explicit: this phase does not deliver the full ARM/Thumb diagnostic scope (Phase 5), deeper determinism work (Phase 6), BIOS execution/HLE, hardware qualification, broad game compatibility or real fixture approvals. Five exception mappings are API/classifier/writer tested but not triggered by guest instructions or CLI because no interrupt/device source is modeled. Trace write records expose the bus API's result value, currently zero, rather than the attempted write payload.

## Gaps Summary

The executable CPU and runner requirements are implemented and supported by direct tests and fresh lane evidence. One documentation truth fails: the guide calls five category-labeled probe rows explicit even though the edge-coverage record leaves every row unresolved. Correct the sentence to say all 11 rows remain unresolved, with five category-labeled and six unclassified, then rerun the narrow documentation/policy review and refresh this report fingerprint. The pending D3 human clarity judgment remains separate and must not be auto-approved.

No plan-declared shell probe was missing. No requirement gap was found in the CPU or runner behavior. Broader ISA coverage and deeper determinism remain explicitly assigned to later phases.

## Decision Coverage

The GSD decision-coverage query found no trackable decision records for this phase. No decision was inferred or fabricated.

---

_Verified: 2026-11-11T12:53:27Z_
_Verifier: gsd-verifier_
