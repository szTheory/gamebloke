# Phase 5: Toolchain Spike, Original Diagnostics, and ARM/Thumb Completion - Context

**Gathered:** 2026-11-11
**Status:** Ready for planning

<domain>
## Phase Boundary

Project-authored ARM and Thumb diagnostics build from source with pinned tools, show ARMv4T output and measured cross-host byte identity before any ROM digest is committed, and drive the canonical CPU to the complete exact ARM7TDMI ARMv4T ARM/Thumb instruction and operand/addressing scope. Real production-runner mailbox verdicts identify each named case and its deliberately broken variant. Preserve the six roadmap requirements and all five criteria; no BIOS/device, hardware-accuracy, game-compatibility or release expansion.
</domain>

<decisions>
## Implementation Decisions

The owner requested autonomous work from Phase 3, accepted Phase 4's evidence guide, and authorized the standing phase-PR policy. The reversible choices below are agent decisions under that authority, informed by three independent research lenses. They are not new owner votes, measured toolchain results, licensing decisions, or physical qualification.

### Pinned toolchain and gating spike
- Pin xPack binary release 15.2.1-1.1 and the exact official macOS arm64, Linux x64 and Linux arm64 assets, sizes and SHA-256 values recorded in 05-TOOLCHAIN-NOTES.md. Use a dedicated developer-tool lock and bounded fetch/extract path. Preserve generic fixture pins/fetcher's existing no-redirect and size contracts.
- Permit only the specifically reviewed HTTPS release redirect shape/host, exact expected byte count and hash; do not log signed URLs. Verify before extraction, validate archive paths/types/links, stage in a fresh temporary tree, preserve prior installs on refusal, and promote atomically. Inspect actual packaged component notices after verification. Tools remain ignored local/build content; no package-manager or Python runtime dependency is introduced.
- The first executable tracer is original minimal ARM/Thumb source plus owned startup/link/header handling. Capture input-object and final-ELF readelf architecture attributes, executable-section disassembly, exact link input map, no-runtime check, generated-image signature scan and fresh fetch/extract/build timing. A flag or empty unresolved-symbol list alone is not the proof.
- Prove macOS arm64 and Linux output bytes equal before committing any diagnostic ROM digest. Local native Linux arm64 is a separately labeled Linux witness; hosted Linux x64 remains distinct evidence. Record exact host identities. If required cross-host bytes differ, retain the mismatch and use the roadmap's explicit Linux digest-ownership fallback, with honest per-host execution identity; never silently pass macOS bytes against another digest.
- The spike must precede digest admission and must measure actual tool time. Archive contents/notice inventory, output attributes, parity and runtime remain unknown until that work. No arbitrary hosted retries: standing stop-on-red applies. Keep every hosted job at 180 seconds. Choose placement from measurements; do not append a large install to near-limit jobs or add speculative cache/concurrency/dependencies. Any separate toolchain job must cover the distinct recurring build/reproducibility failure class. Do not upload ROM artifacts merely to share work between lanes.

### One complete interpreter and bounded units
- Complete all exact ARM7TDMI ARMv4T families/forms: all ARM ALU/shifter/conditions/PC uses, control flow, six multiply forms, PSR, single byte/word and halfword/signed transfers with legal addressing modes, SWP/SWPB, all block/user-bank/exception-return cases; all 19 Thumb formats and their operation/operand variants, including independently retired BL halves. Keep a machine-readable family/form/case crosswalk; the named quirks are a minimum, not the full scope.
- Use exact-core DDI0029G, ARM ARM DDI0100I version rules, and GBA-specific GBATEK 3.06 cross-checks. Include UMULL/UMLAL/SMULL/SMLAL on this exact core. Correct the historical CPU-9 SWI note: ARM comment field is 24 bits, Thumb 8 bits. ARMv4 condition 1111 is unpredictable, not automatically UND.
- Expand one ephemeral decoded unit into bounded ordered transaction/result/register-effect slots. Preflight and execution consume the same decode. Snapshot original operands/banks/status before effects; never add a second interpreter or effectful preflight reads. SWP has two ordered data slots; block transfers may have sixteen plus refill slots. Derive full trace/sequence/cycle bounds from the maximum declared work rather than retaining the old three-record staging cap.
- Memory-loaded PC targets use symbolic refill slots resolved from prior actual read results. Under the current uniform one-cycle bus, preflight exact slot counts and modeled internal work before effects. Validate all concrete spans, alignment classes and counter headroom. Capture exception-return SPSR/bank destinations before loads; restore status and resolve state/alignment in explicit order. Prove no checked-request failure after effects under the present bus contract; future effectful/faulting MMIO or address-dependent waits need a new bus transaction/preparation contract or explicit partial-abort semantics before device scope grows.
- Preserve current explicit timing limitations and bus-only time authority. New core-local internal work (register shifts/multiply) must have a small source-referenced, decoded cost table and be charged through bus-owned advancement; it does not establish machine wait-state/prefetch or physical cycle accuracy. Existing timing behavior should change only where the concrete Phase 5 policy/table requires it and tests/documentation explain it.
- Architecturally unpredictable outcomes get explicit deterministic project treatment, never a fabricated silicon oracle: preserve incoming C for short multiply and C/V for long multiply while asserting defined N/Z and short-multiply V preservation; invalid PSR modes/absent SPSR and other unsupported unpredictable operand combinations stop with a distinct documented classification. New memory-loaded-PC alignment/refill edge policies clear required alignment bits and use 32-bit wrapped refill-slot arithmetic where completion requires it; label these unpredictable-boundary policies. Preserve existing checked B/BX refusal contracts unless a documented, tested in-scope correction requires change; do not silently weaken all address guards.
- Keep valid unsupported, UND, unpredictable, later extension and absent-coprocessor cause distinctions honest. Final legal in-scope family/forms must have no unimplemented holes. Coprocessor absence still enters architectural UND bookkeeping and the named BIOS-none stop; no coprocessor or BIOS execution. Any necessary additive reason/enum update must reach classifier, validator, profile, docs and controls together.

### Original diagnostics and output identity
- Write original assembly, crt0, linker script and header fixer; do not copy SDK startup, font, logo, runtime or private content. Establish explicit stack and mailbox storage from the current direct-start state, write completion last, and use stable nonzero uint32 case IDs with documented independent expectations.
- For generated source-owned fixtures, prefer a narrowly optional build-artifact declaration (path and SHA-256) in manifest/1, separate from source revision identity. Preserve legacy rows and result/1. A git source pin continues to identify reviewed source; the artifact pin identifies actual loaded ROM bytes. Do not overload an assembly source digest as the ROM digest or rely on a lane-specific missing in-tree file. Validate the new shape and all digest bindings, with real stale-source/output controls.
- Source-only commit(s) precede digest-bearing admission commits so a fixture cannot self-reference its own containing commit. The recipe identifies every original input and tool pin; if a shared input changes, rebuild and re-admit affected outputs. Index generation must work from validated committed authority even when optional local tools/outputs are absent; actual execution still hashes the bytes before guest execution. The new output path is an explicit build-root-relative artifact, not a runtime manifest override or global shared mutable binary directory.
- Each positive and broken variant runs the same real instruction/assertion path through production gamebloke-run --fixture, then the canonical result checker. Assert exact PASS/GUEST_FAIL, semantic state, failing case ID and identity separately; checker acceptance alone does not verify expected case ID. Negative variants change the assertion's expected operand/result, never directly fabricate FAIL or bypass the instruction being tested. Add core semantic mutations where they establish detection of the implementation fault, distinct from guest-variant plumbing.
- The two normal ARM/Thumb semantic diagnostics require actual production PASS and assertion-broken GUEST_FAIL. Separate SWI/UND/absent-coprocessor stop witnesses execute the exceptional operation and assert architected state/named GUEST_STOP, because BIOS-none cannot continue to a handler. Their source-built images retain normal parity/admission checks. API IRQ-between-BL remains labeled lower-boundary evidence, never a CLI PASS claim.
- CPU-1 through CPU-9 map to case IDs: version boundaries, shifter/carry, instruction-specific PC reads, block transfers, subword/alignment, multiply flags, two Thumb BL instructions, PSR/banking/returns, and exception decode. BL interruption uses the real CPU exception API with IRQ explicitly unmasked first; do not invent a live IRQ controller or execute a nonexistent handler.
- Generated ROMs stay ignored. Scan their exact output directory with an exact-name temporary binary/extension allowlist; logo/BIOS digest checks remain mandatory. Link-map inputs must be original project objects only, supported by symbol/disassembly checks and meaningful negative controls. No broadly exempted build tree or ROM source-control allowlist.

### CI, rights and acceptance
- Missing optional local toolchain produces a specific inventoried CTest SKIPPED reason; unmet declared BIOS produces actual result SKIPPED/ran=false. Missing/mismatched tools in the required diagnostic CI path fail setup, rather than allowing all diagnostics to skip. Build/execution errors are failures, not dependency skips. No silent compiler or BIOS fallback.
- Decide CI/ASan build participation from measured pipeline cost while preserving same canonical production runner/index authority. Both configured lanes must configure safely with the real manifest; no absent generated artifact can make an unrelated lane misleadingly fail or pass. Keep local canonical command aligned with required CI and meaningful inventory/negative controls.
- Current task authority covers owner-controlled original diagnostics and their necessary public-CI execution. Record only that fixture-scoped permission and original authorship, with explicit notices and permitted-use limits. Do not label MIT, add a project LICENSE, infer third-party rights, grant general redistribution, upload ROM artifacts, or repurpose Phase 4 synthetic rows. If implementation needs rights beyond this scope, stop for that concrete choice.
- Bump profile version 4 to 5 for the actual expanded CPU claim, synchronizing JSON policy fields, narrative, validators and controls. Keep other schema/status vocabularies unless a necessary additive field/reason demands a coherent update. Retain timing/latch unknowns, historical eleven unresolved Phase 4 probe rows, and no hardware/BIOS/game/release qualification. Phase 6 still owns determinism and third-party CPU suites.

### The agent's discretion
Exact source layout, case-ID allocation, implementation/plan count, helper/slot structures, stable reason names, small additive manifest artifact field shape and portable build-root path rules. Apply all concrete fields/tests/docs through completion; routine slices do not need owner acknowledgments. Do not reduce full ISA scope or turn measured spike failures into hidden passes.
</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- src/cpu.c owns canonical decode/preflight/run, bank/exception transitions and tracing; src/cpu_helpers.h owns unsigned shifts/arithmetic.
- Existing machine/bus/loader/inspection APIs and full-state test equality/digest/refusal helpers; production/control source-list parity and mutation controls.
- GenerateFixtureIndex.cmake, manifest/profile/result validators, real runner identity/JSON pipeline, sorted CTest/JUnit inventories, signature scanner and installed production relocation tests.
### Established Patterns
- C17, CMake 3.25/Ninja, POSIX sh/CMake scripts; strict warnings, no new host/runtime framework; reason-matched negative controls, measured background checks.
- One PR per phase, squash through green required CI and verify main; stop on red hosted CI before fixes/retries. Preserve user AGENTS/config/state-cache edits.
### Integration Points
- CPU slot plan and whole-unit commit; CMake generated diagnostics before runnable targets; optional artifact SHA to production index/result binding; explicit dependency skips and mandatory CI guard; profile 5 support/policy tables.
</code_context>

<specifics>
## Specific Ideas

Use the three supporting NOTES files as source-grounded inputs and canonical 05-RESEARCH.md as synthesis. They distinguish source observations, engineering recommendations and unmeasured spike claims. ARM/Thumb diagnostics are the first real source-owned production fixture admissions, rather than upgraded synthetic contract rows.
</specifics>

<deferred>
## Deferred Ideas

Phase 6 owns repeat/optimization/compiler/interleave equality and pinned third-party suites. BIOS/HLE, active IRQ/timers/DMA, device scheduling, Game Pak timing/prefetch, graphics/audio, physical qualification, broad compatibility, stable package/consumer API, project licensing and releases remain their existing later scopes/gates.
</deferred>
