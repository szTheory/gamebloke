---
phase: "04"
slug: "cpu-execution-core-and-headless-runner"
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-10"
---

# Phase 04 — Validation Strategy

The task map below records the completed behavioral evidence for all 25 Phase 04 tasks. This audit cross-references the live tests and registrations, checks the current JUnit reports, and replays the focused CPU/runner witnesses listed below. Formal phase acceptance, security/code review, and the human judgment items remain with the parent workflow; this validation status does not claim those reviews are complete.

## Audit result

All eight requirement IDs map to observable behavior in registered tests. No automated behavioral gap was found, so this audit adds no test file. All 25 task rows are marked covered and link to the owning summary's verification record.

The audit replay ran the CPU unit selectors, all eight CPU semantic mutation controls and their clean companion, the runner host suite, actual producer/result validation, and both relocation checks: 27/27 passed in 18.53 seconds. The latest persisted Phase 13 JUnit reports show CI 329 cases with zero failures and one allowed `signature.owner_local` skip, ASan 72 cases with zero failures or skips, and hygiene 204 cases with zero failures and one allowed `signature.owner_local` skip. Plan 13 records the corresponding full workflow and post-inventory checks.

The empty production fixture manifest remains authoritative. The generated instruction images and synthetic metadata are local validator inputs, not rights grants. API-level exception tests cover all eight exception classifications; guest/CLI execution currently reaches SWI, undefined, and coprocessor stops only. The five externally triggered exception kinds remain API-only. The write trace currently reports the bus result value `0`, not the attempted store payload; this remains documented for independent code review.

## Test infrastructure and cadence

Existing CTest/C17 assertion harness, CMake 3.25 floor, POSIX controls, sorted inventories and canonical source lists are retained. No package/runtime dependency or new testing framework is required.

- Task verification uses the exact commands below from the repository root, in the background.
- Targeted CTest commands use --no-tests=error; direct selectors must fail for unknown/missing cases.
- Do not rebuild the project for each opcode/vector. Compile one assertion executable, run its cases; small semantic scratch variants retain configured warnings and sanitizer flags.
- Canonical final command: `cmake --workflow --preset ci`.
- Fresh local sanitizer command: `cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan && ctest --preset asan && ctest --preset asan-post`. Linux hosted CI keeps its canonical asan workflow.
- Keep current three-minute hosted job limits. Record actual final critical-path durations before changing setup/build work. Source-list extensions may increase existing scratch compile cost: if measurements threaten the bound, prefer linking unchanged canonical objects plus the changed mutant unit with semantic wiring controls; no speculative cache/concurrency/dependency/timeout addition.
- Existing allowed skips remain visible through inventory post checks. New CPU/runner/producer/relocation checks are required, not permitted silent skips. Do not invent future inventory totals or runtime measurements.
- Audit replay command (no rebuild): `ctest --test-dir build/ci -R '^(cpu\.(tracer|lifecycle|budget|arithmetic|thumb|decode|alignment|modes|exceptions|no_bios|peek|mailbox|trace_capacity|trace|nc\.(pc_refill|shift_carry|bank_alias|exception_lr|vector_fetch|peek_effect|trace_loss|budget_overshoot)|pos\.clean)|runner\.(unit|result_contract|relocation|relocation_control))$' --no-tests=error --output-on-failure` — 27/27 passed in 18.53 seconds.

## Per-task plan/wave verification map

Each wave is serial because public header, CPU implementation, CMake and inventories overlap. The leading task in every plan is a real production tracer. No checkpoint is required by these reversible local changes.

| Task | Plan/wave | Requirements | Threat refs | Behavioral witness and test artifact creation dependency | Command | Audited evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 04-01-01 | 01 / 1 | CPU-03, RUN-01 | T-04-01 | The real ARM path and exact/one-short refill and MOV/B bounds pass with explicit bank/pipeline storage and no second interpreter. Creation dependency: Existing Phase3 bus/loader; this task creates the CPU interface and native cpu_test tracer. | V-04-01-01 | [COVERED — 04-01-SUMMARY verification](04-01-SUMMARY.md#verification) |
| 04-02-01 | 02 / 2 | CPU-03, RUN-01 | T-04-02 | Both cores compile canonical sources and the existing clean mutation controls execute them. Creation dependency: Plan04-01 complete; this task creates/extends tests/core.cmake, tests/gates/core_mutation.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt. | V-04-02-01 | [COVERED — 04-02-SUMMARY verification](04-02-SUMMARY.md#verification) |
| 04-02-02 | 02 / 2 | CPU-03, RUN-01 | T-04-02 | Every persistent CPU field participates in lifecycle, canonical comparison and non-default refusal evidence. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/cpu_test.c, tests/state_check.h. | V-04-02-02 | [COVERED — 04-02-SUMMARY verification](04-02-SUMMARY.md#verification) |
| 04-03-01 | 03 / 3 | CPU-03, CPU-05, RUN-01 | T-04-03, T-04-04 | Real shifted arithmetic uses defined helpers and the exact supported operand slice is enforced. Creation dependency: Plan04-02 complete; this task creates/extends tests/cpu_test.c. | V-04-03-01 | [COVERED — 04-03-SUMMARY verification](04-03-SUMMARY.md#verification) |
| 04-03-02 | 03 / 3 | CPU-03, CPU-05, RUN-01 | T-04-03, T-04-04 | Owned ARM-to-Thumb-to-ARM and IWRAM transfer paths work with precise supported/excluded operand classification. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/cpu_test.c. | V-04-03-02 | [COVERED — 04-03-SUMMARY verification](04-03-SUMMARY.md#verification) |
| 04-03-03 | 03 / 3 | CPU-03, CPU-05, RUN-01 | T-04-03, T-04-04 | The registered CPU gates catch concrete pipeline and arithmetic faults without repeated builds per vector. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/gates/core_mutation.cmake, tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt, tests/cpu_test.c. | V-04-03-03 | [COVERED — 04-03-SUMMARY verification](04-03-SUMMARY.md#verification) |
| 04-04-01 | 04 / 4 | CPU-04, PROF-03, CPU-03 | T-04-05, T-04-06 | A real guest SWI reaches a precise architected entry snapshot and explicit BIOS-none stop. Creation dependency: Plan04-03 complete; this task creates/extends tests/cpu_test.c, tests/state_check.h. | V-04-04-01 | [COVERED — 04-04-SUMMARY verification](04-04-SUMMARY.md#verification) |
| 04-04-02 | 04 / 4 | CPU-04, PROF-03, CPU-03 | T-04-05, T-04-06 | All modes and exception entries have exact state evidence and named BIOS-none stops; no active device controller is implied. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/cpu_test.c, tests/state_check.h. | V-04-04-02 | [COVERED — 04-04-SUMMARY verification](04-04-SUMMARY.md#verification) |
| 04-04-03 | 04 / 4 | CPU-04, PROF-03, CPU-03 | T-04-05, T-04-06 | Independent negative controls establish the mode and no-BIOS gates have teeth. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/gates/core_mutation.cmake, tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt. | V-04-04-03 | [COVERED — 04-04-SUMMARY verification](04-04-SUMMARY.md#verification) |
| 04-05-01 | 05 / 5 | CPU-03, CPU-04, CPU-05, PROF-03 | T-04-07 | Version4 profile, CPU implementation and visible narrative agree, with source and project-policy claims kept distinct. Creation dependency: Plan04-04 complete; this task creates/extends cmake/CheckProfile.cmake, tests/gates/profile_case.cmake. | V-04-05-01 | [COVERED — 04-05-SUMMARY verification](04-05-SUMMARY.md#verification) |
| 04-05-02 | 05 / 5 | CPU-03, CPU-04, CPU-05, PROF-03 | T-04-07 | The canonical profile gate checks CPU defaults, supported scope and narrative in both gate lanes. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/gates.cmake, tests/inventory/ci.txt, tests/inventory/hygiene.txt. | V-04-05-02 | [COVERED — 04-05-SUMMARY verification](04-05-SUMMARY.md#verification) |
| 04-06-01 | 06 / 6 | RUN-02, RUN-01 | T-04-08, T-04-09 | A real guest-written mailbox is observed through a bounded, state-neutral path. Creation dependency: Plan04-05 complete; this task creates/extends tests/cpu_test.c. | V-04-06-01 | [COVERED — 04-06-SUMMARY verification](04-06-SUMMARY.md#verification) |
| 04-06-02 | 06 / 6 | RUN-02, RUN-01 | T-04-08, T-04-09 | Both configured cores and scratch controls exercise the same neutral peek/mailbox implementation. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/core.cmake, tests/gates/core_mutation.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt. | V-04-06-02 | [COVERED — 04-06-SUMMARY verification](04-06-SUMMARY.md#verification) |
| 04-07-01 | 07 / 7 | RUN-03, RUN-01, RUN-02 | T-04-10, T-04-11 | The actual guest path emits bounded selected records with atomic capacity refusal. Creation dependency: Plan04-06 complete; this task creates/extends tests/cpu_test.c, tests/state_check.h. | V-04-07-01 | [COVERED — 04-07-SUMMARY verification](04-07-SUMMARY.md#verification) |
| 04-07-02 | 07 / 7 | RUN-03, RUN-01, RUN-02 | T-04-10, T-04-11 | Trace completeness, order, no-effect refusal and hard-cycle gates have meaningful negative evidence. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/cpu_test.c, tests/gates/core_mutation.cmake, tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt. | V-04-07-02 | [COVERED — 04-07-SUMMARY verification](04-07-SUMMARY.md#verification) |
| 04-08-01 | 08 / 8 | RUN-01 | T-04-12 | A bounded arbitrary-span helper has independent vectors and is ready to bind actual loaded bytes. Creation dependency: Plan04-07 complete; this task creates/extends tests/hash_test.c. | V-04-08-01 | [COVERED — 04-08-SUMMARY verification](04-08-SUMMARY.md#verification) |
| 04-09-01 | 09 / 9 | RUN-01, RUN-02 | T-04-13 | Canonical validated metadata and explicitly synthetic control metadata are generated separately from one authority path without a new rights grant. Creation dependency: Plan04-08 complete; this task creates/extends tests/fixtures/instruction_image.c, tests/gates/runner_metadata.cmake. | V-04-09-01 | [COVERED — 04-09-SUMMARY verification](04-09-SUMMARY.md#verification) |
| 04-09-02 | 09 / 9 | RUN-01, RUN-02 | T-04-13 | Hash/index gates run in both compiled lanes with meaningful clean/negative controls. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt, tests/gates/runner_metadata.cmake, tests/hash_test.c. | V-04-09-02 | [COVERED — 04-09-SUMMARY verification](04-09-SUMMARY.md#verification) |
| 04-10-01 | 10 / 10 | RUN-01, RUN-02 | T-04-15, T-04-16 | One actual canonical CLI path produces metadata-bound JSON and raw diagnostic execution stays non-PASS. Creation dependency: Plan04-09 complete; this task creates/extends runner/main.c. | V-04-10-01 | [COVERED — 04-10-SUMMARY verification](04-10-SUMMARY.md#verification) |
| 04-11-01 | 11 / 11 | RUN-01, RUN-02, RUN-03, PROF-03 | T-04-14 | Actual CLI outcomes and error precedence are deterministic, bounded, escaped and explicit. Creation dependency: Plan04-10 complete; this task creates/extends tests/runner_test.c, tests/gates/runner_case.cmake. | V-04-11-01 | [COVERED — 04-11-SUMMARY verification](04-11-SUMMARY.md#verification) |
| 04-11-02 | 11 / 11 | RUN-01, RUN-02, RUN-03, PROF-03 | T-04-14 | Canonical ci/asan lanes run the real producer and its bounded failure cases. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt. | V-04-11-02 | [COVERED — 04-11-SUMMARY verification](04-11-SUMMARY.md#verification) |
| 04-12-01 | 12 / 12 | RUN-01, RUN-02, RUN-03, PROF-03 | T-04-17, T-04-18 | The existing result checker accepts genuine matching producer documents and rejects meaningful actual-output corruptions. Creation dependency: Plan04-11 complete; this task creates/extends cmake/CheckResult.cmake, tests/gates/runner_case.cmake, tests/gates/result_case.cmake. | V-04-12-01 | [COVERED — 04-12-SUMMARY verification](04-12-SUMMARY.md#verification) |
| 04-12-02 | 12 / 12 | RUN-01, RUN-02, RUN-03, PROF-03 | T-04-17, T-04-18 | Producer/validator and legacy contract controls coexist without a detached evidence path. The evidence guide documents the real CLI, mailbox completion, exact time fields, synthetic authority and supported/unsupported scope. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/core.cmake, tests/gates.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt. | V-04-12-02 | [COVERED — 04-12-SUMMARY verification](04-12-SUMMARY.md#verification) |
| 04-13-01 | 13 / 13 | RUN-04, RUN-01, RUN-02, RUN-03, CPU-03, CPU-04, CPU-05, PROF-03 | T-04-19, T-04-20 | The installed production executable genuinely runs from a moved prefix and unrelated cwd with identical actual observations. Creation dependency: Plan04-12 complete; this task creates/extends tests/gates/runner_relocation.cmake, tests/core.cmake, tests/inventory/ci.txt, tests/inventory/asan.txt. | V-04-13-01 | [COVERED — 04-13-SUMMARY verification](04-13-SUMMARY.md#verification) |
| 04-13-02 | 13 / 13 | RUN-04, RUN-01, RUN-02, RUN-03, CPU-03, CPU-04, CPU-05, PROF-03 | T-04-19, T-04-20 | Final source/inventory/sanitizer evidence is current and measured, with honest remaining claim/probe limits and no publication by this task. Creation dependency: Earlier task(s) in this plan create production/test interfaces; this task creates/extends tests/core.cmake, tests/gates/core_mutation.cmake. | V-04-13-02 | [COVERED — 04-13-SUMMARY verification](04-13-SUMMARY.md#verification) |

## Requirement-to-test cross-reference

| Requirement | Required behavior | Behavioral witnesses and negative evidence | Audit status |
| --- | --- | --- | --- |
| CPU-03 | ARM/Thumb pipeline visibility, PC reads, bus-backed fetches, and explicit open-bus latch behavior. | `cpu.tracer` asserts ARM PC+8, refill order, branch slot invalidation, and latch update; `cpu.thumb`/`cpu.decode` cover Thumb PC+4 and state changes; `cpu.nc.pc_refill` must fail its named semantic assertion; `cpu.pos.clean` is the companion. | COVERED |
| CPU-04 | Correct seven-mode bank selection, SPSR presence, exception snapshot, LR bases, and named stops before vector fetch. | `cpu.modes`, `cpu.exceptions`, `cpu.no_bios`; bank-alias, LR, and vector-fetch mutants. Reset/abort/IRQ/FIQ entry is exercised through the actual CPU exception API. | COVERED; five external exception sources remain API-only. |
| CPU-05 | Defined shifts/arithmetic at edge counts and clean sanitizer execution. | `cpu.arithmetic` executes independent helper vectors and real decoded arithmetic; `cpu.nc.shift_carry` checks the RRX mutation; full ASan/UBSan inventory includes `sanitizer.ubsan.shift32`. | COVERED |
| PROF-03 | BIOS-none exceptions stop with a stable reason and cannot fetch an absent handler. | `cpu.no_bios` executes guest SWI/undefined/coprocessor cases; `runner.unit` sends all eight exception kinds through the production stop classifier and JSON writer. | COVERED; IRQ, reset, abort, and FIQ are API-classified, not guest-triggered. |
| RUN-01 | Fresh bounded execution emits one result with status, semantic outcome, exact machine time, claim, and limits kept distinct. | `cpu.lifecycle`/`cpu.budget`; `runner.tracer`, `runner.unit`, `runner.result_contract`, `runner.hash`, and `runner.metadata`; actual producer PASS/FAIL/raw/zero/exact/one-short cases and result mutations. | COVERED |
| RUN-02 | Mailbox reads use a state-neutral IWRAM peek and accept only complete protocol records. | `cpu.peek`, `cpu.mailbox`, `cpu.nc.peek_effect`; full canonical state and caller-buffer checks; actual producer output flows through `CheckResult.cmake`. | COVERED |
| RUN-03 | Category-masked guest-time tracing is bounded, ordered, and explicit on capacity or output failure. | `cpu.trace`, `cpu.trace_capacity`, `cpu.nc.trace_loss`, `cpu.nc.budget_overshoot`, `runner.unit`, and `runner.result_contract`; exact/one-short capacity, trace-write failure and incomplete-trace precedence are exercised. | COVERED; write records report current bus result `0`, not attempted store payload. |
| RUN-04 | Installed production runner survives install-prefix move and unrelated working directory. | `runner.relocation` compares the production executable, JSON and JSONL trace; `runner.relocation_control` removes the executable and requires the named `missing-executable` failure. | COVERED |

## Manual-only and unresolved review items

- Plan 04-12's D3 human judgment remains pending: a human must judge whether `docs/evidence-contract.md` explains the evidence and rights boundaries clearly. Automated documentation checks do not approve that judgment.
- The six spec-less unclassified rows and the five category-labeled-but-unresolved probe rows remain unchanged in the authoritative report. Their behavioral tests establish concrete software witnesses, not probe taxonomy resolutions.
- The two descriptor-less standing-policy flags remain open: no reuse of private or rights-bearing test inputs or synthetic rights grants; no inflated hardware, BIOS, or compatibility claims. No synthetic acceptance or owner decision was added.
- This audit does not make the plan's API-only exception sources guest/CLI reachable and does not decide the documented trace-write payload limitation. Those scope and review boundaries stay explicit.

## Exact commands and observable failure directions

Each command is inherited from proven CMake/CTest patterns or names a target/script created by the ordered task. Commands introducing source files require those files to exist first; no nonexistent test is represented as execution evidence.

### V-04-01-01

```sh
cmake -E make_directory build/phase04-tracer && cc -std=c17 -DGBL_VERSION_STRING='"0.0.0"' -Iinclude src/version.c src/machine.c src/bus.c src/loader.c src/cpu.c tests/cpu_test.c -o build/phase04-tracer/cpu_test && build/phase04-tracer/cpu_test tracer
```

Fails when: A stale branch slot executes, visible PC differs from current+8, a fetch bypasses the latch, or a one-short unit changes any canonical field.

### V-04-02-01

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(core\.version|cpu\.tracer|bus\.pos\.clean|loader\.pos\.clean)$' --no-tests=error --output-on-failure
```

Fails when: Either configured core or scratch control fails to link the CPU, inventories disagree, or cpu.tracer is absent.

### V-04-02-02

```sh
cmake --build --preset ci && ctest --test-dir build/ci -R '^(cpu\.(lifecycle|budget)|machine\.state|loader\.(transaction|diagnostics))$' --no-tests=error --output-on-failure
```

Fails when: Any new field survives reset incorrectly, disappears from equality/digest, leaks between instances, or changes on a refused load/run unit.

### V-04-03-01

```sh
cmake --build --preset ci && build/ci/cpu_test arithmetic
```

Fails when: Any boundary helper or real decoded instruction has wrong carry/overflow, causes sanitizer UB, or applies effects for a failed condition.

### V-04-03-02

```sh
cmake --build --preset ci && build/ci/cpu_test thumb && build/ci/cpu_test decode && build/ci/cpu_test alignment
```

Fails when: State switch keeps stale pipeline words, Thumb PC is wrong, transfer bypasses the bus or an excluded case silently executes.

### V-04-03-03

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^cpu\.(arithmetic|thumb|decode|alignment|nc\.(pc_refill|shift_carry)|pos\.clean)$' --no-tests=error --output-on-failure
```

Fails when: A planted CPU semantic fault passes, a clean companion fails, or a new source/header is absent from a configured/scratch core.

### V-04-04-01

```sh
cmake --build --preset ci && build/ci/cpu_test exceptions
```

Fails when: SWI saves a postentry CPSR, wrong LR/bank/vector, fetches handler bytes or exceeds its one-short cap.

### V-04-04-02

```sh
cmake --build --preset ci && build/ci/cpu_test modes && build/ci/cpu_test exceptions && build/ci/cpu_test no_bios
```

Fails when: Adjacent banks alias, User/System exposes SPSR, a snapshot happens after mutation, any LR uses the wrong defined address, or stopped execution fetches a vector.

### V-04-04-03

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^cpu\.(modes|exceptions|no_bios|nc\.(bank_alias|exception_lr|vector_fetch))$' --no-tests=error --output-on-failure
```

Fails when: Any deliberate bank/LR/vector fault passes or any named required witness is missing.

### V-04-05-01

```sh
cmake -DCASE=cpu_policy_value -DSRC=. -DBIN=build/ci -P tests/gates/profile_case.cmake && cmake -DCASE=cpu_policy_doc -DSRC=. -DBIN=build/ci -P tests/gates/profile_case.cmake && cmake -DPROFILE=profiles/agb.json -DDOC=docs/profile-agb.md -DREGISTRY=registry/disputes.json -DVOCAB=contracts/evidence.json -P cmake/CheckProfile.cmake
```

Fails when: A concrete CPU default/support row contradicts implementation or a mutated/hidden table is accepted.

### V-04-05-02

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^profile\.' --no-tests=error --output-on-failure
```

Fails when: Any old/new profile control disappears, accepts its deliberate corruption or reports a stale no-match mutation.

### V-04-06-01

```sh
cc -std=c17 -DGBL_VERSION_STRING='"0.0.0"' -Iinclude src/version.c src/machine.c src/bus.c src/loader.c src/cpu.c src/inspect.c tests/cpu_test.c -o build/phase04-tracer/cpu_test && build/phase04-tracer/cpu_test peek
```

Fails when: Peek changes any canonical field, time/latch/trace or caller bytes on refusal, or incomplete/zero mailbox becomes PASS.

### V-04-06-02

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^cpu\.(peek|mailbox|nc\.peek_effect)$' --no-tests=error --output-on-failure
```

Fails when: Invalid spans mutate buffer/state, a partial mailbox becomes terminal, or planted peek effects evade the gate.

### V-04-07-01

```sh
cmake --build --preset ci && build/ci/cpu_test trace
```

Fails when: A disabled category appears, event values/time differ from real bus results, or one-short capacity alters state/records.

### V-04-07-02

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^cpu\.(trace|trace_capacity|nc\.(trace_loss|budget_overshoot))$' --no-tests=error --output-on-failure
```

Fails when: A lost/overwritten record or cap overshoot passes, or masks/capacity affect guest state without an explicit refusal.

### V-04-08-01

```sh
cmake -E make_directory build/phase04-tracer && cc -std=c17 -Irunner -Itests runner/sha256.c tests/hash_test.c -o build/phase04-tracer/hash_test && build/phase04-tracer/hash_test
```

Fails when: Any independent digest differs, boundary padding reads beyond span, refusal changes output, or host SHA arithmetic triggers UB.

### V-04-09-01

```sh
cmake --preset ci && cmake --build --preset ci && cmake -DSRC=. -DBIN=build/ci -P tests/gates/runner_metadata.cmake
```

Fails when: Invalid manifest produces an index, empty canonical index grants a fixture, a changed image matches stale metadata, or control/production sources/options diverge.

### V-04-09-02

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^runner\.(hash|metadata)$' --no-tests=error --output-on-failure
```

Fails when: A generated-authority mismatch, invalid metadata or deliberately wrong independent hash expectation is accepted.

### V-04-10-01

```sh
cmake --preset ci && cmake --build --preset ci && build/ci/gamebloke-run-control --rom build/ci/generated/fixtures/owned-arm-pass.gba --fixture synthetic-control-pass --cycles 1000
```

Fails when: The producer does not execute real guest stores, emits more than one JSON document, omits real cycles/digest/semantic fields, or raw production input yields PASS.

### V-04-11-01

```sh
cmake --build --preset ci && build/ci/runner_test && cmake -DSRC=. -DBIN=build/ci -P tests/gates/runner_case.cmake
```

Fails when: Any bounded read/conversion overshoots, result conflates termination with semantic outcome, cancellation escapes watchdog, or incomplete trace/zero mailbox yields PASS.

### V-04-11-02

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^runner\.(tracer|unit|cases|help|version|usage_error|no_args)$' --no-tests=error --output-on-failure
```

Fails when: A required producer case is missing or detached, or legacy help/usage regresses.

### V-04-12-01

```sh
cmake --build --preset ci && cmake -DSRC=. -DBIN=build/ci -DVALIDATE_PRODUCER=ON -P tests/gates/runner_case.cmake
```

Fails when: A synthetic canned producer is accepted despite changed guest code, missing dispute survives validation, or a malformed/overshooting semantic/time result validates.

### V-04-12-02

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^(runner\.result_contract|result\.)' --no-tests=error --output-on-failure
```

Fails when: A result gate disappears, fails only from stale/no-match input, or actual producer validation is not registered. The evidence guide contradicting actual CLI flags, mailbox order, result fields or authority is an acceptance failure.

### V-04-13-01

```sh
cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^runner\.relocation(_control)?$' --no-tests=error --output-on-failure
```

Fails when: Moved CLI needs source/CMake/old prefix, differs in JSON/trace/output status, invents PASS for raw data, or missing-executable control passes.

### V-04-13-02

```sh
cmake --workflow --preset ci && cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan && ctest --preset asan && ctest --preset asan-post && git diff --check
```

Fails when: Any required lane/control/inventory fails, instrumentation omits final sources, core includes host APIs, or execution evidence is missing/skipped without an allowed explicit reason.

## Wave-0 / missing-test scaffold responsibilities

The infrastructure already exists. There is no detached test-only foundation phase: every tracer writes the failing behavioral test and real production path together.

| Missing artifact | Created by | First execution |
| --- | --- | --- |
| tests/cpu_test.c; CPU structs/interpreter | 04-01-01 | direct native compile and tracer selector |
| cpu_test CMake target/initial selectors and source-list parity | 04-02-01 | targeted CTest |
| arithmetic/Thumb/decode/alignment selectors | 04-03-01/02; registration and controls04-03-03 | direct selectors then CTest |
| mode/exception/no-BIOS selectors and controls | 04-04-01/02; registration04-04-03 | direct selectors then CTest |
| CPU profile validator/control cases | 04-05-01; registration04-05-02 | direct script then CTest |
| inspect.c and peek/mailbox selectors | 04-06-01; source/registration/control04-06-02 | direct compile then CTest |
| selected traces/capacity selectors and controls | 04-07-01/02 | direct selector then CTest |
| sha256.c/h and tests/hash_test.c | 04-08-01; CMake target/registration04-09-02 | direct compile then CTest |
| instruction-image generator, immutable index and metadata script | 04-09-01; registration04-09-02 | script then CTest |
| actual production/control CLI sources and generated-index linking | 04-10-01 | direct actual CLI run |
| runner_test, failure-injection host seams and runner_case script | 04-11-01; registration/inventories04-11-02 | direct executable/script then CTest |
| actual-producer result checker witnesses | 04-12-01; registration04-12-02 | actual output through Phase2 checker |
| moved-prefix installed production runner check | 04-13-01 | install/move/unrelated-cwd CTest |

## Important controls and provenance

- PC/refill, shifter carry, bank isolation, exception LR and absent-vector-fetch faults compile against the canonical core and must fail semantic assertions with clean companions.
- Peek effects, trace record loss/overwrite and one-unit budget bypass faults must be caught by full state/record evidence. Compilation failure is never semantic evidence.
- Whole-span SHA uses independently chosen expected vectors, not the private fixed156 helper as an arbitrary-ROM oracle.
- Production metadata derives only from the validated canonical manifest, which may remain empty. Synthetic metadata/index inputs are hermetic contract controls and grant no rights. Production/control runner targets have identical sources/options/core; only immutable index input differs.
- Producer controls mutate real guest code and actual stdout, proving the result path is not a detached canned JSON generator. Expected dispute ids/label come from the generated synthetic row, and actual emitted output reaches CheckResult.cmake.
- Installed relocation compares the real production executable and actual raw diagnostic cycles/semantics/JSON/trace; unregistered bytes cannot PASS. A missing-executable negative control must fail for its named reason.
- No Nintendo bytes, private ROM/BIOS CPU runs, external assemblers, BIOS HLE/stub, package installs, stable embedding ABI, project-license decision or release enters this phase.

## Probe accounting and remaining judgment flags

The authoritative `build/phase04-research/edge-coverage.json` contains 11 rows and reports `resolved: 0`, `unresolved: 11`, and `unclassified: 6`. Five rows have category labels (CPU-04 adjacency, empty, ordering; RUN-01 boundary, precision), while six remain unclassified (CPU-03, CPU-05, PROF-03, RUN-02, RUN-03, RUN-04). Plans and tests specify concrete behavioral witnesses for the classified categories, but this audit does not rewrite the probe output or convert those witnesses into taxonomy resolutions. Preserve the exact 11 rows and all six unclassified flags.

The in-prompt prohibition precision pass retained two descriptor-less standing-policy flags: no private/rights-bearing fixture repurposing or fake rights grants (plan01), and no inflated hardware/BIOS/compatibility claims (plan10). They remain unresolved/flagged-unverified for honest judgment disposition; no human acceptance or wired check was fabricated. No-silent-drop equality:2=2+0. Ordinary correctness and canon memory/security issues stay in behavioral tests and the security review.

## Revision 2 context-budget disposition

The checker warning boundary includes ten files. The three flagged plans now split at their existing task boundaries into thirteen plans with the same25 tasks and exact verification commands. Unique edit sets range from three to nine files. Original01 becomes01/02; original07 becomes08/09; original08 becomes10/11. Other plans shift in order, as recorded by the complete mapping in13-PLAN.

The first ARM native tracer executes in01. Canonical configured cores and scratch compilation are wired in02 before arithmetic/Thumb expansion in03. Native hash vectors execute in08 before metadata/target registration in09. The real CLI smoke executes in10 before runner_test and failure-case target creation/registration in11. Single-task plans01,08 and10 are intentional coherent tracers; there is no new phase boundary, behavior reduction, rights grant or dependency.

## Validation sign-off after execution

- [x] All 25 task commands and their observable failure directions have evidence in the owning summaries; the focused audit replay passed.
- [x] Required test artifacts and CTest registrations exist, and the current CTest inventory exposes required tests rather than hiding them as skips.
- [x] Plan 13 audited canonical core, runner, and scratch source wiring, plus warning and sanitizer instrumentation.
- [x] CPU semantic mutants and clean companions passed as controls; the focused replay reran all eight CPU mutants.
- [x] Plan 13 CI, fresh ASan/UBSan, post-inventory, and hygiene evidence reconciles; only the documented owner-local checks were skipped.
- [x] Actual producer JSON, dispute disclosure, exact caps, and moved-prefix output are covered by the contract and relocation tests.
- [x] Final critical-path measurements and setup/build observations are recorded in `04-13-SUMMARY.md`.
- [x] All 11 probe rows, six unclassified flags, and two descriptor-less policy flags remain visible without fabricated resolution.
- [x] This independent Nyquist coverage audit is recorded; security/code review and formal phase acceptance remain parent-owned and are not claimed complete.

The Phase 04 automated validation map is complete. Human judgment on evidence-guide clarity and rights boundaries remains pending, as do the parent-owned review and phase-acceptance steps. This is not a phase-completion or release claim.

## Validation Audit 2026-11-11

| Metric | Count |
|---|---|
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |
| Task map entries audited | 25 |
