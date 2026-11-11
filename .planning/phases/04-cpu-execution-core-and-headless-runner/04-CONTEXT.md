# Phase 04: CPU Execution Core and Headless Runner - Context

**Gathered:** 2026-11-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Deliver CPU-03/04/05, PROF-03 and RUN-01..04: explicit ARM7TDMI pipeline and
register/mode/exception state, defined unsigned arithmetic helpers, a bounded
execution path through the canonical bus, and a relocated installed CLI that
emits one evidence-bearing JSON result. The mailbox is observed through a
state/time-neutral peek; selected trace categories use bounded storage and
explicit output-full termination. Manifest disputes flow to the real producer.

Full ARM/Thumb instruction coverage and original toolchain-built diagnostics
remain Phase 5. Use in-memory instruction words and project-authored generated
test images in this phase. The private inputs supplied for logo-header checks
are not CPU execution fixtures or compatibility oracles.
</domain>

<decisions>
## Implementation Decisions

The owner authorized autonomous work and routine reversible engineering choices.
The choices below are agent-authored implementations of the existing roadmap and
accepted profile/evidence policies, not new claims of hardware accuracy or owner
approval of individual numeric hardware assumptions. No unresolved product,
rights, cost or irreversible choice was identified by the discussion research.
Do not manufacture another approval request for these routine slices.

### Direct-start CPU policy
- Keep `agb`, `direct-start`, BIOS provider `none`, ROM-only cartridge scope.
- Start the execution address at `0x08000000`, ARM state, System mode, condition
  flags clear and IRQ/FIQ masked (`CPSR=0x000000DF`). These deterministic defaults
  are project policy, not a measured BIOS handoff. Zero general/banked registers,
  SP/LR and valid SPSR storage; User/System have no architectural SPSR.
- Keep BIOS stack conventions out of the initializer. Owned diagnostics establish
  any stack they require. Start the pipeline empty, fill through real bus fetches,
  charge all work to the existing clock, and distinguish execution address from
  operand-visible PC. Keep startup/prefetch details explicit and tested.
- Retain Phase 3 memory/latch policies and hardware-unknown labels. CPU fetches
  use the same bus; do not claim full region/state-specific hardware open bus or
  Game Pak prefetch. Bump the profile version and synchronize its concrete fields,
  narrative, validator and controls for claim-affecting CPU policy additions.

### Interpreter and exception boundary
- One interpreter path, explicit pending instruction state, ARM/Thumb PC reads
  at +8/+4, centralized banking and exception transitions, no decode cache or
  fused instruction path. Define the precise Phase 4 opcode slice in the plans;
  include enough real instructions to exercise modes, branches/state switches,
  mailbox stores and runner termination. Full coverage remains Phase 5.
- Preserve every ARM7TDMI mode and exception entry. Specify LR source addresses
  as current/next instruction addresses, not an ambiguous shared `PC` offset;
  cross-check each ARM/Thumb case against the cited TRM tables.
- For BIOS-none exceptions, complete architected banking/SPSR/LR/mask/state/vector
  bookkeeping, then return a named guest stop before fetching a handler from
  absent BIOS. This stop edge is project policy. Unsupported valid instructions
  are distinguished from architecturally undefined encodings; no silent success.
- Use explicit unsigned shift/carry and add/sub overflow helpers. Guard zero,
  32 and larger shift cases before C shifts; test independently chosen vectors
  and use current sanitizer controls. No source of machine time outside the bus.

### Bounded runs, inspection and trace
- Cycle bounds are guest master-cycle bounds; frame bounds convert using the
  profile frame-cycle constant with checked arithmetic. Keep host time separate.
- Prefer whole-instruction/initial-refill preflight so work never silently exceeds
  the requested cap and no mid-instruction preemption is required. If the next
  bounded unit cannot fit, stop without that unit's effects and report TIMEOUT
  with actual progress/cycles. Plans must prove the preflight cost/trace contract
  for the selected instruction slice; do not implement a second semantic engine
  for prediction or weaken the bound without surfacing the concrete consequence.
- Peek is a bounded observation, not a guest bus transaction: no time, latch,
  trace, CPU or canonical state changes. Compare the whole enumerated state,
  including all new CPU fields, rather than padding or just a digest.
- Trace category mask selects instruction, bus-access and exception records.
  Preflight required capacity, preserve records, and stop explicitly on output
  full; never overwrite or silently drop events. Keep the nine result states:
  use HARNESS_FAIL plus `trace-output-full` reason/termination for this condition,
  with semantic result reported separately. Incomplete required trace cannot PASS.

### Runner result and installed-consumer contract
- Preserve `gamebloke.result/1` and the existing claim tuple/status vocabulary.
  Derive PASS only from a registered fixture's validated metadata, loaded-byte
  digest and complete versioned mailbox verdict, not a bare path or arbitrary
  zero-filled IWRAM. Keep status, semantic result, machine cycles and limits distinct.
- Prefer immutable metadata generated from the canonical validated manifest at
  build time and compiled into the runner. A fixture selector and actual ROM hash
  bind the run to its row, including dispute ids and provenance. This avoids
  duplicating the full CMake JSON validator in a new runtime parser and keeps the
  installed executable independent of source paths and CMake at runtime.
  An alternative equally bounded manifest-backed design is at the planner's
  discretion if it preserves this authority and evidence without new dependencies.
- Carry fixture disputes into emitted ids and the `disputed` label. Feed actual
  producer documents to the Phase 2 validator, including negative controls.
- Define a magic/version/state-bearing IWRAM mailbox; zero memory is not a
  verdict. Check complete guest completion before coincident budget exhaustion;
  harness/output failures remain distinct and cannot be hidden by guest PASS.
- Bound file reads, hash exactly the loaded bytes, escape JSON strings, exclude
  machine-local paths, and keep malformed/missing/refused/cancelled/timeout/guest
  stop outcomes distinct. A raw unregistered ROM must not produce evidence PASS.
- Install to relative destinations; relocate the prefix and compare installed
  output to build-tree output. Use explicit input paths with no source-tree or
  temporary-test-environment dependency.

### Agent's discretion
Exact structs/private modules, mailbox layout, trace record format and bounded
capacity accounting, CLI spelling/exit-code mapping within the existing result
contract, fixture-index generation, hash helper reuse, and plan decomposition
are routine reversible choices. Maintain a flat C17/CMake/POSIX tree. Add no new
runtime dependency without concrete value; no license, BIOS stub/HLE, physical
qualification, private-ROM execution or release is authorized by this phase.
</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `gbl_machine` owns per-instance memory, master time, latch, ROM and allocator
  state; lifecycle and transactional ROM replacement/refusal are already tested.
- Bus read/write/internal calls are the single checked time owner and distinguish
  declared map outcomes. CPU-level alignment and fetch behavior were left here.
- `tests/state_check.h`, bus/loader tests and the mutation driver provide canonical
  field/array/identity controls. Extend them for every new CPU field and source.
- Strict CMake contract/profile/manifest/result validators, dispute registry,
  test inventories, semantic negative controls and current sanitizer lanes exist.
- The private SHA-256 logo helper is fixed-window only. A whole-ROM hash needs a
  correct bounded general span/host helper and independent vectors; do not pretend
  the current fixed 156-byte function hashes arbitrary input.

### Established Patterns
Caller-owned deterministic state; host-supplied allocation; no core host includes;
no global mutable machine state; explicit refusal postconditions; synthetic test
inputs; one canonical CI workflow; warnings as errors. Both configured cores share
one source list. Scratch mutation compilation currently lists core sources and
must be updated coherently when CPU files/reset dependencies are added.

### Integration Points
`include/gamebloke/gamebloke.h`, `src/{machine,bus,loader}.c`, `runner/main.c`,
`CMakeLists.txt`, `tests/core.cmake`, `tests/gates/core_mutation.cmake`, sorted
lane inventories, profile/contract files and developer docs. Runner currently
implements only help/version; no CPU, peek, trace or install route exists.
</code_context>

<specifics>
## Specific Ideas

- Read `04-ARM-NOTES.md`, `04-GBA-NOTES.md`, and `04-RUNNER-NOTES.md`.
  The official DDI0029G Issue G manual was read in full, all 282 pages. The complete
  GBATEK 3.06 ARM CPU Reference section was read; later ARM-version material is
  explicitly excluded. Full copyrighted source caches remain ignored in build/.
- ARM documentation is the architectural authority; GBATEK is a community source,
  not Nintendo documentation or physical hardware qualification. Keep observed
  source facts, proposed project policy and measured test results distinct.
- Validate PC/refill, all banks/exception entries, shift/carry/overflow, state-neutral
  peek, bounded trace and budget refusal, dispute emission and relocated output.
  Every important gate has a meaningful negative control; missing/skipped checks
  stay visible. Avoid new build/setup duplication and preserve current CI limits.
- Existing owner policy authorizes one Phase 4 PR and squash merge through the
  ruleset once required checks pass. Stop/report on red hosted CI; never bypass.
  Automated apps/tests stay background; no focus-taking UI is needed for this phase.
</specifics>

<deferred>
## Deferred Ideas

Full ARM/Thumb diagnostic coverage and toolchain reproducibility: Phase 5.
Determinism/optimization/compiler/interleave equivalence gates: Phase 6.
Scheduler, active IRQ controller, timers, DMA, BIOS stub/HLE and pixels: later
milestone with its original owner gates. Physical post-BIOS/hardware open-bus
qualification, public embedding ABI, LibFuturo qualification and releases remain
separate. No source reading, fixture license or digest grants redistribution rights.
</deferred>
