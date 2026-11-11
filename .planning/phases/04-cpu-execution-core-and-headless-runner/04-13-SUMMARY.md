---
phase: 04-cpu-execution-core-and-headless-runner
plan: "13"
subsystem: runner
tags: [CMake, C17, CLI, relocation, JSON, CPU, CI, ASAN, UBSAN]

# Dependency graph
requires:
  - phase: 04-12
    provides: Actual producer/result contract, semantic/time validation, and dispute disclosure
provides:
  - Relocated installed production runner check with exact result and trace comparisons
  - Final source, inventory, sanitizer, and evidence-claim audit for Phase 4
affects: [runner, evidence-contract, verification, CPU, CI, ASAN]

# Measured from the persisted plan ledger; excludes this metadata commit.
actuals:
  tokens: 4713
  tasks: 2
  commits: 3
plan_head_before: 42d4a19f7c5d49db0e80d01f1a50b46932004ee6
plan_head_after: 575a419998145b2b1bc3e0df25322e9bffb15893

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Install only the production CLI at a relative bin destination and run it from a moved prefix.
    - Compare executable and input digests plus exact bounded JSON and trace bytes from an unrelated working directory.
    - Keep canonical empty production metadata separate from generated synthetic control metadata.
key-files:
  created:
    - .planning/phases/04-cpu-execution-core-and-headless-runner/04-13-SUMMARY.md
  modified:
    - CMakeLists.txt
    - tests/gates/runner_relocation.cmake
    - tests/core.cmake
    - tests/inventory/ci.txt
    - tests/inventory/asan.txt
    - docs/verify.md
    - docs/evidence-contract.md
key-decisions:
  - A moved prefix changes installation location, not executable identity or fixture authority; no assumption-delta decision changed.
  - Keep production fixture authority bound to the empty canonical manifest; generated synthetic control rows grant no rights and are not installed.
  - Preserve the current trace-write result value of zero as an explicit implementation limitation for independent review; do not infer the attempted payload or hardware latch value.
metrics:
  duration: "29m 35s at final verification capture"
  completed: 2026-11-11
status: complete
requirements-completed: [] # Parent/auditor owns the phase-level roll-up.
requirements-addressed: [RUN-04, RUN-01, RUN-02, RUN-03, CPU-03, CPU-04, CPU-05, PROF-03]
---

# Phase 4 Plan 13: Relocated Installed Runner and Final Evidence Summary

**The installed production runner now survives prefix relocation with byte-identical bounded output, while the final source, inventory, sanitizer, and claim-boundary evidence is documented.**

## Accomplishments

- Installed only `gamebloke-run` at relative `bin/`. The relocation gate installs it to one prefix, moves that prefix, and invokes the real build-tree and installed production executables from an unrelated working directory using the same generated owned image and bounded cycle budget.
- Compared executable SHA-256 and loaded-image digest, exact raw JSON, trace-enabled JSON, and JSONL trace contents. The test checks semantic mailbox state, case id, guest progress, machine cycles, budget, trace completeness, and absence of source/build/prefix paths in emitted output. It also requires the install to contain only `bin/gamebloke-run`.
- Kept the real relocation input unregistered in the canonical empty production manifest. Its expected outcome is `GUEST_STOP` / `unregistered-content` with its actual semantic and time observations, not a synthetic or rights-backed `PASS`. The control removes the installed executable, requires `runner-relocation: missing-executable`, and then confirms the clean installation still passes.
- Added exact current runner selectors and phase-end commands to `docs/verify.md`. Updated `docs/evidence-contract.md` with the CPU slice, profile and BIOS limits, exception reachability, trace-write-value limitation, unresolved probe rows, and rights/claim review flags.
- Audited source and build wiring. `tests/gates/core_mutation.cmake` needed no changes: the existing scratch copy/compile lists already include the CPU and inspection units where required, preserve clean companions, and compile the mutated unit with the current instrumentation and warning flags.

## Task Commits

1. **Task 1 RED:** `3a9a9ff` — `test(04-13): add failing relocated production runner gate`
2. **Task 1 GREEN:** `9aff12f` — `feat(04-13): install and verify the relocated production runner`
3. **Task 2:** `575a419` — `fix(04-13): enforce install contents and document final evidence`

The plan ledger measured three task commits from `42d4a19f7c5d49db0e80d01f1a50b46932004ee6` through `575a419998145b2b1bc3e0df25322e9bffb15893`. The summary metadata commit is separate.

## TDD Gate Compliance

- **RED:** Commit `3a9a9ff` added the relocation test and CI/ASAN registrations before the install rule. The focused test failed for `runner-relocation: missing-executable` because the installed executable did not exist. JUnit at `build/ci/plan13-red.xml` and evidence record `build/ci/plan13-red-evidence.json` were accepted by `gsd_run check tdd-red-evidence` as `RED_EVIDENCE_OK target_test_failed`; setup and compilation succeeded.
- **GREEN:** Commit `9aff12f` added the single relative install rule and wired the test to actual production output. The tracer feedback rerun passed both relocation selectors. Test expectations were corrected to the producer's actual `unregistered-content` termination and CMake's JSON boolean spelling `ON`; no production behavior was changed to satisfy a test.
- **Task 2 verification:** After adding install-tree contents and trace-path assertions, the focused clean/control pair passed 2/2 in 0.51 seconds.

## Verification

- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^runner\.relocation(_control)?$' --no-tests=error --output-on-failure` — both relocated production behavior and the missing-executable control passed.
- `cmake --workflow --preset ci` — 329 inventory entries expected; 328 passed and the one allowed `signature.owner_local` check skipped because `GBL_OWNER_LOGO_FILE` was unset. `ci-post` passed. CTest took 83.00 seconds; configure took 0.1 seconds and the incremental build had no work.
- `cmake --fresh --preset asan -DCMAKE_C_COMPILER=/usr/bin/clang && cmake --build --preset asan && ctest --preset asan && ctest --preset asan-post` — fresh AppleClang 21 ASan/UBSan build completed in 50 build steps; all 72 tests passed and the post inventory reported 72 expected, zero skipped. CTest took 64.80 seconds; fresh configure took 0.3 seconds. The intentional heap-overflow control matched the expected ASan report; the host emitted external-symbolizer startup warnings during that control.
- `cmake --workflow --preset hygiene` — 204 inventory entries expected; 203 passed and the same one allowed owner-local signature check skipped. `hygiene-post` passed. The final full tree run took 37.08 seconds; configure took 0.0 seconds and the incremental build had no work. After the final documentation metric edit, `hygiene.tree` and `signature.tree` passed 2/2 in 7.61 seconds.
- `git diff --check` — passed.

These CI, sanitizer, and hygiene durations measure build and test work, not emulator throughput. The local 83-second CI test critical path remains below the existing 180-second hosted job cap; no timeout, cache, dependency, or concurrency change was made. The hosted Linux sanitizer lane remains configured for its existing `clang-21` environment.

## Relocation Observation

The generated owned image ran through the production index from both executables. Both produced byte-identical JSON and trace output from the unrelated directory. The raw result was `GUEST_STOP` with reason and termination `unregistered-content`, `semantic.state=pass`, `semantic.case_id=36865`, `time.cycles=26`, `time.instructions=12`, `time.budget_cycles=1000`, and a complete 38-record trace. This observes the guest mailbox and deterministic machine time under the current project profile. Because the production manifest is empty, it does not validate as a registered fixture result and does not establish fixture permission or hardware behavior.

## Source and Build Wiring Audit

| Surface | Audited wiring and disposition |
| --- | --- |
| Canonical core | `GBL_CORE_SOURCES` is `src/version.c`, `src/machine.c`, `src/bus.c`, `src/loader.c`, `src/cpu.c`, and `src/inspect.c`. Both `gamebloke_core` and `gamebloke_core_logo_synthetic` compile that list with the same public include, C version, warning link, and core options; only the private logo expected-digest definition differs. Private headers are `src/logo_sha256.h` and `src/cpu_helpers.h`; the public contract is `include/gamebloke/gamebloke.h`. |
| Runner pair | Production and control targets both use `runner/main.c`, `runner/run.c`, `runner/result.c`, and `runner/sha256.c`, link the same canonical core, and retain the same target options. Their generated fixture-index include directory is the intended difference and is checked by `runner.metadata`. Only the production executable is installed. |
| Scratch mutation builds | Bus and loader copies compile the relevant `version`, `machine`, `bus`, `loader`, and `cpu` units with their helper/public headers and clean semantic companions. CPU mutations copy and compile `version`, `machine`, `bus`, `loader`, `cpu`, and `inspect`, with `logo_sha256.h`, `cpu_helpers.h`, the public header, state/test helpers, and `cpu_test.c`; the `peek_effect` mutation changes and compiles `inspect.c`. Their compile flags include the configured compiler flags, warnings-as-errors, core hardening policy, and the lane's sanitizer instrumentation. The independent in-tree no-host-include gate remains active. |
| Lane registration | The 11 actual runner selectors are `runner.hash`, `runner.version`, `runner.tracer`, `runner.unit`, `runner.result_contract`, `runner.relocation`, `runner.relocation_control`, `runner.help`, `runner.metadata`, `runner.usage_error`, and `runner.no_args`. The two new relocation selectors are registered in CI and ASAN inventories in sorted order. Final inventories were 329 CI, 72 ASAN, and 204 hygiene checks. |
| Profile and authority | `profiles/agb.json` and `docs/profile-agb.md` are at profile version 4: `agb`, `direct-start`, BIOS provider `none`, ROM-only cartridge, partial ARM7TDMI execution slice, and project placeholder timing. Production metadata is generated from the canonical empty manifest; generated control metadata/images remain build-only artificial validation inputs with no rights grant. |

## Phase Artifact and Interface Inventory

The phase produced and audited these contracts across all 25 plan tasks:

| Interface | Preserved fields, symbols, or artifacts |
| --- | --- |
| Machine/CPU state | `gbl_machine.cpu`; `r0_7[8]`, `usr_r8_14[7]`, `fiq_r8_14[7]`, `irq_r13_14[2]`, `svc_r13_14[2]`, `abt_r13_14[2]`, `und_r13_14[2]`, `spsr[5]`, `cpsr`, `execution_address`, both pipeline entries' `address/opcode/width/valid`, `next_fetch_sequential`, `retired_instructions`, `stopped`, `stop_reason`, and `last_exception.kind/current_address/next_address/fault_address`. Lifecycle reset/dispose/refusal, field-wise equality, and fixed-order digest checks remain part of the core contract. |
| Bounded CPU operation | `gbl_cpu_run`, `gbl_run_options.cycle_budget/unit_limit/category_mask/trace`, and `gbl_run_result.termination/reason/cycles/instructions`; one canonical bus-backed interpreter with preflighted units. Terminations are `GBL_RUN_PROGRESS`, `GBL_RUN_BUDGET`, `GBL_RUN_GUEST_STOP`, `GBL_RUN_TRACE_OUTPUT_FULL`, `GBL_RUN_INVALID_ARGUMENT`, and `GBL_RUN_BUS_ERROR`. |
| Arithmetic and CPU policy | `gbl_shift`, `gbl_add_flags`, and `gbl_sub_flags`; version 4 profile fields cover source/citation, entry/CPSR/register and SPSR fill, User/System SPSR, pipeline, ARM/Thumb PC offsets, timing, exception entry, BIOS-none stop, execution, supported, and excluded forms. Supported ARM data processing is MOV/ADD/SUB/CMP/AND/ORR with rotated-immediate or register operands and immediate/register-specified LSL/LSR/ASR/ROR, S flag, and destinations r0-r14; B/BL/BX; and pre-indexed no-writeback byte/word LDR/STR with immediate 12-bit U plus/minus offsets, r0-r14 base/data, and PC base only for positive LDR literal. Supported Thumb forms are immediate8 MOV/ADD/SUB/CMP; high-register MOV to r0-r14/BX; conditional branches except SWI/reserved conditions and unconditional B; PC-relative word LDR at aligned current+4; and low-register immediate word/byte LDR/STR. Excluded forms include ARM register-specified shifts involving r15, r15 destinations outside branch/BX, STR PC source and PC reads; post-index/writeback, halfword/signed, block, multiply, swap, full PSR, and coprocessor execution; Thumb data-processing r15 writes outside BX/branch, BL pair, other unlisted transfers; later extensions and all other valid unimplemented forms. The authoritative table is linked from `docs/evidence-contract.md`. |
| Modes and exception API | `gbl_cpu_set_cpsr`, `gbl_cpu_get_spsr`, `gbl_cpu_enter_exception`; reset/undefined/SWI/prefetch-abort/data-abort/IRQ/FIQ/coprocessor kinds and all named no-BIOS stop mappings. CPU API, shared classifier, and JSON tests cover all eight; the current guest/CLI directly triggers only SWI, undefined, and coprocessor. The remaining five are not described as CLI reachable. |
| Neutral inspection and mailbox | `gbl_machine_peek`, `gbl_peek_status`, `gbl_mailbox_read`; mailbox at `0x03007FC0`, magic `0x47424C4B`, version 1, terminal state/case id and completion marker `0x434F4D50`. Peek observes IWRAM without bus timing and tests preserve the full-state digest and master clock. PASS/FAIL requires the full mailbox marker protocol. |
| Trace | `GBL_TRACE_INSTRUCTION/BUS/EXCEPTION`, category mask, bounded buffer, and record fields `category/guest_cycle/instruction_address/opcode/address/value/width/access_class/bus_outcome/exception_kind`; the 65,536-record ceiling, exact mask/order, unit preflight, output-full status, no silent drop, and JSONL output remain covered. A successful CPU store currently emits `value: 0` from the bus result; the record does not carry the attempted payload. |
| Hash and metadata | `gbl_sha256_span(data,length,out_hex65)`; immutable metadata fields `id/rom_sha256/provenance_class/dispute_ids/dispute_count/bios_requirement/permitted_use/mailbox_convention`; production canonical index and separate synthetic control index. Generated instruction images are original build-tree controls. |
| CLI | `--rom PATH`, exactly one of `--cycles DECIMAL` or `--frames DECIMAL`, optional `--fixture ID`, `--trace-mask INTEGER`, `--trace-capacity INTEGER`, and `--trace-output PATH`, plus `--help` and `--version`. Frame conversion remains 280896 cycles and trace capacity is capped at 65,536. No runtime manifest/index/source/CMake override was added. |
| `gamebloke.result/1` JSON | Top-level `schema`, `fixture.id/ran/rom_sha256`, `status`, `reason`, `refusal`, `claim.machine/boot/bios_provider/cart`, `input_timeline`, `evidence`, `disputes`, `labels`, `limits`, `semantic.state/case_id`, `time.cycles/instructions/budget_cycles`, `termination`, `expected_sha256`, and `trace`. Claim includes cart kind/digest and evidence fixture/provenance class. Time values are canonical decimal strings; requested trace carries `mask/capacity/count/complete`. Semantic output remains separate from status, machine time, claim, and limits; exact fixture and loaded-image digest roles remain distinct. |
| Build/runtime products and gates | `GBL_CORE_SOURCES`, `GBL_RUNNER_SOURCES`, `cpu_test`, `hash_test`, `runner_test`, original instruction-image generator, production/control runners and generated indexes/images. Only relative `bin/gamebloke-run` is installed. Existing CPU, profile, loader, result, metadata, producer, inventory, relocation, and negative-control registrations remain in their declared lanes. |

The phase's new tracked source artifacts were `src/cpu.c`, `src/cpu_helpers.h`,
`src/inspect.c`, `runner/sha256.c`, `runner/sha256.h`, `runner/run.c`, `runner/run.h`,
`runner/result.c`, `tests/cpu_test.c`, `tests/hash_test.c`, `tests/runner_test.c`,
`tests/fixtures/instruction_image.c`, `cmake/GenerateFixtureIndex.cmake`, and the
`tests/gates/runner_metadata.cmake`, `tests/gates/runner_case.cmake`, and
`tests/gates/runner_relocation.cmake` controls. Runtime/generated products include
`generated/production/fixture_index.h`, `generated/control/fixture_index.h`, the
control `manifest.json`, and owned instruction images under `generated/fixtures/`, all
under the build directory. These source files and generated interfaces are retained in
the plans and prior summaries; Plan 13 added no parallel CPU or metadata implementation.

## Requirement and Success-Criteria Audit

Requirements were addressed by the listed source plans; the phase-level checkbox roll-up remains parent-owned and `requirements-completed` stays empty here.

| Requirement | Evidence plan coverage |
| --- | --- |
| CPU-03 | Plans 01–05 and 13: bus-backed execution, pipeline/PC observations, lifecycle, and final source/sanitizer audit. |
| CPU-04 | Plans 04–05 and 13: banked modes, SPSRs, exception entry, profile policy, and final mapping audit. |
| CPU-05 | Plans 03, 05, and 13: defined ARM arithmetic/shifts, profile boundary, UBSan and source audit. |
| PROF-03 | Plans 04–05 and 10–13: no-BIOS behavior, versioned profile, real producer output, exception reason mapping, and final claim limits. |
| RUN-01 | Plans 01–03 and 06–13: cycle bounds, exact time representation, result statuses, semantic/time fields, hash identity, metadata and final relocation. |
| RUN-02 | Plans 06, 08–13: neutral peek/mailbox, content digest and authority, actual producer/checker behavior and final source audit. |
| RUN-03 | Plans 07 and 10–13: selected bounded trace, capacity/output failure, result contract, and final trace/claim documentation. |
| RUN-04 | Plan 13: actual production installation, prefix move, unrelated cwd, exact JSON/trace comparison, and missing-executable control. |

| Phase success criterion | Evidence/disposition |
| --- | --- |
| 1. In-memory CPU words, ARM/Thumb PC reads, explicit latch, no decode cache/fusion, arithmetic edge cases, clean UBSan. | Covered by CPU unit and mutation controls, including arithmetic, pipeline, latch, and sanitizer paths; all 72 ASAN inventory tests passed in this run. |
| 2. Correct banks/LR/SPSR and named BIOS-none exception stops. | Bank and exception API tests plus production classifier/JSON tests cover all eight exception mappings. Guest/CLI triggers remain limited to SWI, undefined, and coprocessor; reset, IRQ, FIQ, prefetch-abort, and data-abort stay API-only. Parent phase review retains that reachability distinction for the criterion's final disposition. |
| 3. Bounded JSON separates status, mailbox semantic result, machine time, claim, and limits; neutral peek; exact dispute producer/validator. | Covered by actual producer/checker integration, mailbox state/digest/time tests, and dispute preservation controls. Registered synthetic PASS/FAIL are only control inputs; the production manifest remains empty. |
| 4. Selected bounded traces and explicit output-full behavior. | Covered by trace mask/order/capacity and producer tests plus passing trace-loss/output-full negative controls. Write records currently contain the bus result value zero, not the attempted CPU store payload; see the documented review limitation. |
| 5. Installed CLI survives `cmake --install`, prefix move, and unrelated cwd with identical JSON. | Covered by the real production relocation test. Install contents are exactly the CLI executable; executable/image digests and JSON/trace bytes match, and the missing-executable control fails for its named reason. |

The plan task crosswalk accounts for all 25 task IDs:

| Plan | Tasks and coverage |
| --- | --- |
| 04-01 | `04-01-01` execute one ARM path through a bounded machine. |
| 04-02 | `04-02-01` integrate the same CPU in configured and scratch builds; `04-02-02` test reset, refusal, and hard caps. |
| 04-03 | `04-03-01` ARM arithmetic/flags; `04-03-02` Thumb/BX/transfers/alignment; `04-03-03` arithmetic and pipeline controls. |
| 04-04 | `04-04-01` real SWI bank state; `04-04-02` modes, exceptions, LR bases; `04-04-03` bank/LR/fetch regressions. |
| 04-05 | `04-05-01` profile matches running CPU policy; `04-05-02` profile controls in every applicable lane. |
| 04-06 | `04-06-01` neutral mailbox inspection after a guest store; `04-06-02` peek refusals and mailbox cases. |
| 04-07 | `04-07-01` selected bounded guest trace; `04-07-02` record loss/order/capacity controls. |
| 04-08 | `04-08-01` whole-image SHA-256 with independent vectors. |
| 04-09 | `04-09-01` immutable metadata and owned control images; `04-09-02` hash/metadata gates without duplicate builds. |
| 04-10 | `04-10-01` generated image through production CLI and JSON writer. |
| 04-11 | `04-11-01` bounded refusals/cancellation/trace/status precedence; `04-11-02` actual runner test lane registrations. |
| 04-12 | `04-12-01` actual semantic/time/dispute producer output; `04-12-02` controls in canonical lanes. |
| 04-13 | `04-13-01` moved-prefix production CLI; `04-13-02` final source/inventory/sanitizer audit. |

The counts by plan are 1, 2, 3, 3, 2, 2, 2, 1, 2, 1, 2, 2, and 2, totaling 25.

## Probe and Assumption Audit

The plan's source audit keeps all 11 spec-less probe rows visible: five classified as explicit and six still unresolved. The five classified rows are CPU-04 bank adjacency, absent User/System SPSR, exception-entry ordering, plus RUN-01 budget boundaries and uint64 precision. The following six rows remain flagged for phase verification even though concrete tests exist: CPU-03, CPU-05, PROF-03, RUN-02, RUN-03, and RUN-04. No row was dropped or promoted through implementation alone.

Two descriptor-less project-policy flags also remain unresolved: private/rights-bearing inputs must not be repurposed and synthetic metadata grants no rights; evidence must not inflate hardware, BIOS, or compatibility claims. No synthetic checker descriptor or human pass was fabricated. The primary identity assumption delta remains **no change**: moving the install prefix changes location, while executable bytes and the loaded-image digest remain the identity evidence. The phase's human judgment about explanation clarity and rights boundaries is not auto-approved; the parent will present it during independent phase review.

## Known Stubs and Review Limitations

| File | Line | Status |
| --- | ---: | --- |
| `docs/evidence-contract.md` | 209 | The inherited placeholder bus-cost and current-latch limitations remain documented and are already open in `.planning/WINDOWS.md` entries 1 and 2; no duplicate ledger entry was added. |
| `docs/evidence-contract.md` | 229 | Successful CPU write trace records currently use `gbl_bus_write`'s zero result value and do not preserve the attempted payload. This is documented for independent contract review; the parent owns any cross-phase ledger decision. |

There were no unrun planned verification commands. The only lane skips were the already-allowed `signature.owner_local` checks because `GBL_OWNER_LOGO_FILE` was unset; ASAN had no skips. No private ROM or BIOS was read or executed, and no fixture, BIOS, or other binary content was committed.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical assertion] Enforced the install-only-CLI and output path-leak boundaries**
- **Found during:** Task 2 audit.
- **Issue:** The relocation gate detected a missing executable and compared actual output, but did not prove that the prefix contained no extra installed files or scan trace bytes for source/build/prefix paths.
- **Fix:** Asserted the moved prefix contains exactly `bin/gamebloke-run` and extended the path-leak scan to the trace-enabled JSON and JSONL contents. Removed a duplicate trace-JSON comparison already guaranteed by the byte-identical JSON comparison.
- **Files modified:** `tests/gates/runner_relocation.cmake`.
- **Verification:** Both relocation selectors passed 2/2; full CI and the docs-sensitive hygiene/signature checks passed.
- **Commit:** `575a419`.

**Total deviations:** 1 auto-fixed. **Impact:** Strengthened the declared install/runtime isolation contract without adding an install asset, dependency, CLI interface, or product claim.

## Threat Surface Scan

The plan adds no new network endpoint, authorization path, runtime index/manifest access, schema, or public core ABI. The installed CLI accepts only the existing explicit input/output paths and compiled production metadata. The control executable and generated synthetic metadata/images remain uninstalled and confined to the build tree.

## Next Phase Readiness

Plan 13's implementation and local evidence are complete. Phase 5 remains responsible for the broader ISA/toolchain diagnostic scope; Phase 6 remains responsible for deeper determinism work. Independent parent review owns the phase-level requirement and success-criterion roll-up, rights/clarity judgment, trace-write-value disposition, and any phase boundary decision. This plan makes no release, hardware-accuracy, BIOS-backed boot, broad compatibility, or rights qualification claim.

## Self-Check: PASSED

- The summary exists at the required phase path.
- All three task commits are ancestors of the branch HEAD.
- The persisted plan ledger base, measured task commit count (`3`), and recorded task HEAD agree.
- `git diff --check` passed; `gsd_run check evaluation-scope --plan 04-13 --commits-only --raw` resolved all three plan commits with no missing or outside-union files.
