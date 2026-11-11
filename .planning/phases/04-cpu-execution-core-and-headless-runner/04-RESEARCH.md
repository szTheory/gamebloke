# Phase 04 Research Synthesis

## Recommendation and scope

Use the canonical C17 machine and bus, adding explicit CPU state and one
interpreter path. Build a thin executable path through CPU execution, mailbox
inspection and a real runner result before expanding mode/exception/trace and
relocation evidence. CPU-03/04/05 and PROF-03 are required here; full CPU-01/02
instruction coverage and the cross-platform diagnostic toolchain remain Phase 5.

The discussion context records routine reversible engineering choices under the
owner's autonomous policy, not new hardware facts or rights grants. The original
roadmap success criteria are unchanged. The goal was restated as a user story
for MVP format while retaining the original technical boundary verbatim.

## Completed source work

- `04-ARM-NOTES.md`: independent research confirms full reading of official
  ARM DDI0029G, Issue G, ARM7TDMI Rev 3, all 282 pages including appendices/index.
  It supplies architecture/version, bank/exception and defined-C arithmetic
  references. The additional architecture/C references were consulted for
  specific clauses, not claimed fully read.
- `04-GBA-NOTES.md`: full GBATEK 3.06 ARM CPU Reference block read, with exact
  source identity/coverage and later-architecture exclusions. Its GBA notes are
  community-source observations, not Nintendo-issued or measured hardware proof.
- `04-RUNNER-NOTES.md`: independent comparison of manifest-backed results,
  split harness composition and raw debug modes against existing source contracts.

Full copyrighted source caches remain ignored in `build/phase04-research/`.
The GBA researcher completed the written note before the parent interrupted a
prolonged finalization; the parent reconciled and read that complete artifact.
No reading claim is inferred from a truncated tool excerpt or a missing artifact.

## Decisions that follow from the evidence

The exact policy and integration decisions are in `04-CONTEXT.md`. In particular:

1. Separate execution address, operand-visible PC and pipeline state. Derive
   exception return addresses with explicit current/next-address definitions,
   checked against TRM tables for ARM and Thumb. Do not reuse a generic PC offset.
2. Preserve independent banked registers and SPSRs; User/System share registers
   and lack SPSRs. Test actual transitions and restoration, not just enum values.
3. Direct start is deterministic project policy, not a synthesized BIOS snapshot:
   ROM entry, ARM/System mode, masked IRQ/FIQ, zero register banks, empty pipeline,
   zero machine time/latch. Update the profile's versioned concrete policy and
   narrative together. Guest/owned crt0 code initializes any required stack.
4. Complete exception bookkeeping then stop before absent-handler fetch under
   BIOS `none`. Keep named causes separate from unsupported valid instructions.
5. Use defined unsigned helpers, independently chosen arithmetic vectors, real
   pipeline/bus observations and the sanitizer lane. Existing scratch mutation
   compilation must include every new core dependency, not stale source lists.
6. A bounded instruction/refill preflight should enforce a hard caller cycle cap
   and trace capacity without requiring mid-instruction preemption or silently
   exceeding the cap. The plan must state and test actual cost/capacity accounting;
   this is an implementation obligation, not an assumption that it already works.
7. A side-effect-free peek observes mailbox state without guest bus effects.
   Enumerate new CPU state in whole-state/identity tests so omitted fields cannot
   disappear behind an otherwise repeatable digest.
8. Keep one result producer and the closed nine-state contract. Prefer validated,
   compiled immutable fixture metadata rather than a second full runtime JSON
   validator or a CMake/source dependency after installation. Match the loaded
   byte digest to that metadata before evidence PASS. Bare input is diagnostic
   only. Carry every fixture dispute and explicit limit to emitted JSON.

The compiled-index approach is a parent synthesis of the runner alternatives:
it retains manifest authority while avoiding duplicated parser/validator lifetime
cost. A different bounded, dependency-free design is allowed only if it preserves
the same claim binding and relocated-runtime guarantees. Do not invent a new
result status or accept caller-asserted qualification to avoid implementing this.

## Validation Architecture

Use the existing CTest/CMake inventories and tiny C assertion harness. Every plan
leads with a real tracer through its changed production surface, then expands
distinct failure cases. Prefer fast assertions in one compiled test executable
over repeated project builds for individual CPU vectors. Add a small set of
semantic mutation controls for distinct recurring failure classes, with clean
companions and current compiler/sanitizer flags.

| Requirement | Behavioral witness | Meaningful negative evidence |
| --- | --- | --- |
| CPU-03 | ARM/Thumb PC reads, sequential/refill/branch-state transitions and real bus/latch observations | Corrupt visible PC/refill or bypass the observed bus path and require the witness to fail |
| CPU-04 | Every register bank and exception entry from relevant states, exact saved CPSR/LR/vector/masks | Flatten a bank or use the wrong LR/SPSR rule and require distinct failures |
| CPU-05 | Shift/carry/RRX/add/sub overflow boundary vectors and clean sanitizer execution | Existing sanitizer meta-controls plus a helper semantic fault that its vectors catch |
| PROF-03 | No-BIOS IRQ/SWI/undefined/coprocessor named stops after entry, before vector fetch | Permit handler fetch/continuation or erase the cause and require failure |
| RUN-01 | Actual producer emits bounded, contract-valid JSON with actual cycles, semantic result and limits | Unregistered/mismatched content cannot PASS; cap overflow, missing and refused input are explicit |
| RUN-02 | Valid/missing/partial mailbox observations and unchanged full state, latch, trace and clock after peek | Mutate state/time or accept zero IWRAM as a verdict and require failure |
| RUN-03 | Category-mask records, exact order and explicit capacity stop without dropped records | One-record-short or overwritten/suppressed record faults must be observable |
| RUN-04 | Install to relative prefix, move it, run from unrelated cwd and compare output | Remove a runtime artifact or use an obsolete source/prefix path and require the relocation check to detect it |

Also verify actual producer dispute labels/ids through the Phase 2 result checker;
transactional reset/load/refusal includes CPU fields; every configured core and
scratch control uses one canonical source set; independent instances do not share
mutable CPU state. Test command inventory/post checks must expose permitted skips.

Task verification is targeted. A full canonical CI run occurs at meaningful plan
wave/integration boundaries; fresh complete sanitizers cover the final source and
inventory. Review measured CI critical paths before adding work. Current hosted
Phase 3 main tests were about 70 seconds Linux, 73 seconds macOS, 45 seconds ASan
and 29 seconds hygiene; these are CI observations, not emulator throughput or a
guarantee for Phase 4. Keep the existing three-minute job limits and remove
evidence-supported duplicate setup/build work before considering any limit change.

## Scope and failure traps

- A title screen, branch, correct PC read, hash or diagnostic result is not broad
  GBA compatibility, BIOS execution, timing accuracy or a redistribution right.
- A missing tool, test, fixture, trace record or contract field is not success.
- No-BIOS policy must not silently execute zero/open-bus vector contents.
- Conditional/operand cases unsupported by the chosen ISA slice must be explicitly
  stopped and listed; do not label a partial handler as full ARM/Thumb coverage.
- No private/commercial ROM CPU runs, new BIOS provider, external API integration,
  dependency, license or release is needed. Use owned generated inputs only.
- Exact mailbox fields, internal structs, trace representation, opcode slice and
  helper/module layout remain planner/executor choices subject to acceptance tests.

## Assumption-delta disposition

The advisory detector matched “another directory” in the relocation criterion.
Decision: `no-change`. The primary identity remains the tested executable and
its fixture/content identity; an installation prefix is a replaceable location,
not a second machine or evidence authority. Keep all runtime assets relative or
explicitly provided. Lift this disposition into the relevant install plan; it
requires no new owner product decision.

## Reconciled planning inputs

This synthesis is the single canonical `*-RESEARCH.md`; independent source notes
use `*-NOTES.md` so workflow discovery cannot accidentally choose one partial
lens instead of the complete synthesis. Reconciled against the current context:
cycle/refill and trace preflight preserve hard bounds; every new CPU field joins
canonical refusal/reset comparisons; compiled fixture metadata binds loaded-byte
digests and disputes without runtime CMake; profile policy changes are versioned
and explicitly distinguished from hardware/BIOS observations. The validation
strategy below follows those current choices, rather than only the older ARM lens.
