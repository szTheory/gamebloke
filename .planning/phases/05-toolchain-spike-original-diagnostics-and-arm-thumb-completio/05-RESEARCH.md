# Phase 5 Research: Toolchain, Original Diagnostics, Full ARMv4T

**Researched:** 2026-11-11
**Domain:** ARM7TDMI completion and source-owned diagnostic evidence
**Confidence:** High for current source seams and exact-core instruction inventory; medium for community GBA quirks; unmeasured for tool output/parity/timing and physical behavior.

## Summary

Implement the complete exact ARM7TDMI ARMv4T scope through the current interpreter, driven by original assembly diagnostics and real production-runner results. First prove the pinned cross-toolchain with a minimal end-to-end image before admitting any ROM digest. Grow the same decoded-unit plan so block transfers, SWP, PC loads and exception returns preserve the existing bounded-run guarantee. Keep source revision, tool archive and generated ROM identities distinct.

Three independent supporting lenses are authoritative detail: [toolchain](05-TOOLCHAIN-NOTES.md), [CPU](05-CPU-NOTES.md), [diagnostics/admission](05-DIAGNOSTIC-NOTES.md). They inspected official primary sources and current repository code. No archive was downloaded/executed and no Phase 5 build/test ran during research. Their recommendations have been resolved in 05-CONTEXT.md; unresolved experimental facts are owned by the first spike, not assumed answers.

## Standard Stack

Retain C17/CMake 3.25/Ninja, POSIX sh and CMake validation/test scripts, current check harness, canonical interpreter/bus/runner and strict warnings. Use xPack arm-none-eabi binary release 15.2.1-1.1, with exact official per-host SHA-256/size pins from TOOLCHAIN-NOTES. Use assembler, direct linker with owned scripts/objects, objcopy/readelf/nm/objdump; no SDK runtime, libgba, devkitARM, gbafix, npm/xpm, Python runtime or new emulator dependency.

The official assets are roughly 294–307 MB, above the fixture fetcher's 256 MiB cap, and use a reviewed GitHub release redirect. A dedicated tool lock/fetch path preserves generic fixture pin/redirect rules. Check exact bytes before safe extraction and binary execution. Actual license/member inventory remains a spike deliverable; packaging MIT is not every component's license and does not license fixture output.

## Architecture Patterns

### 1. Gating source-to-result tracer

The first production slice is a minimal original ARM/Thumb image plus verified tools, owned crt0/link/header fixer, attributes/disassembly/map checks and generated signature scan. Record exact ARMv4T tags and byte comparison on macOS arm64 and Linux before a ROM digest enters the manifest. A local Linux arm64 container is a separately labeled witness; final hosted Linux x64 output must also have its actual evidence. Record an honest Linux-owned fallback if parity fails, without claiming missing equality. No public binary release/artifact upload or project-license change is needed.

### 2. One decoded plan with symbolic dependencies

Current src/cpu.c caps its unit at two fetches, one data operation and three staged trace records. Refactor that ephemeral plan to bounded ordered fetch/data/result/register/status effects, still decoded once. Block transfers need up to sixteen data transactions; SWP needs read then write. A memory-loaded PC makes refill addresses result-dependent. Reserve exact work from slot counts and the present uniform bus cost, then resolve symbolic target slots from actual earlier read results during execution. Never perform an effectful data read in CPU preflight or emulate the instruction twice.

Preflight every concrete span, data/fetch count, clock/retirement/trace/sequence headroom and policy class before effects. Model-dependent pure target alignment and explicitly labeled unpredictable-boundary handling must make the dynamic refill slots non-refusing under the current bus contract. Current BIOS-absent/unmapped/I/O reads are outcomes, not post-validation bus errors. Future side-effecting/faulting MMIO or variable wait costs require a new preparation/transaction or partial-abort contract before they are modeled.

Snapshot store operands, base values and bank/status views. LDM^ PC captures the old SPSR and target banks, loads non-PC registers to their intended bank, restores CPSR, then resolves refill in the restored state. Preserve current checked static B/BX refusals unless a documented correction is required. For newly supported dynamic loaded-PC unpredictable alignment/top-refill cases, the context chooses deterministic masking/wrapped slot completion, explicitly without silicon claims. Effective data-address arithmetic is modulo 2^32; sequential instruction overflow itself is unpredictable, not an ARM promise to wrap.

All cycles remain bus-owned. A source-referenced table may charge new core-local shift/multiply work; machine wait states, prefetch and physical cycle qualification remain unmodeled. Keep the existing trace-write bus-result-zero limitation explicit unless a separately justified correction is made.

### 3. Full family/form and case ledger

Use the complete inventory in CPU-NOTES: sixteen ARM ALU opcodes and all operand forms; PC/control/conditions; all six multiply forms; full MRS/MSR; all legal single, halfword/signed and SWP addressing/operand forms; IA/IB/DA/DB block transfer, empty/base-in-list/user-bank/returns; all nineteen Thumb formats and their operation/register/address variants. Keep legal supported, true UND, unpredictable, later extension and absent-coprocessor classes separate. Final completion has no silently excluded legal in-scope family or operand form.

Correct bootstrap assumptions: exact ARM7TDMI supports long multiply despite a generic withdrawn ARMv4xM note; ARM SWI comment is 24 bits while Thumb is 8; ARMv4 condition 1111 is unpredictable. Short multiply C and long C/V have no architectural fixed value before v5. Context chooses deterministic preservation, labeled project policy, with N/Z and defined preservation bits tested independently.

Case matrix entries name nonzero numeric ID, ISA/family/form, setup, observed result, independently derived expectation/primary reference, positive/broken variants and exact expected mailbox ID. Named CPU-1 through CPU-9 are minimum cases, not the whole ISA. Thumb BL prefix/suffix retire independently; a separate real CPU API witness unmasking IRQ first tests entry between them without claiming live device IRQs or BIOS service.

### 4. Source revision versus output bytes

Current git manifest rows pin source but yield no expected ROM digest; in-tree rows hash a repository-relative file before index generation. Do not bind source-assembly hash as ROM identity or use a global lane-specific ignored output path to obscure authority. Add an optional, validated build-artifact path/SHA declaration to manifest/1 for source-built original fixtures, preserving legacy row behavior and result/1. Emit the committed artifact pin in the production index and result checker; loaded bytes must match before execution. Missing optional tools/output must not prevent unrelated lane configuration or silently turn a digest-less row into PASS.

Use source-only commits before admission commits, identifying every tracked assembly/startup/link/header input and actual tool archive pin. Avoid a row pinning the commit that contains itself. Rebuild/re-admit all affected outputs when a shared source changes. Actual generated output checks remain mandatory before required CI admits a result; metadata is not proof that the build ran.

### 5. Guest outcome, harness and skip boundaries

Use the production runner and canonical result checker. Check exact status, mailbox state, nonzero case ID and byte identity in the test gate, because checker acceptance does not assert the diagnostic's expected failing ID. A deliberately broken variant changes the tested assertion's expectation and reaches a real GUEST_FAIL; it cannot just emit a fabricated FAIL mailbox. Core semantic mutation controls provide separate detection of an actual implementation bug.

No local tool yields an explicit inventoried dependency SKIPPED; unusable tool/build/guest/checker is failure. Required diagnostic CI requires pinned tool presence and actual guest execution, so all diagnostics cannot skip green. Unmet declared BIOS yields real runner SKIPPED/ran=false, with no fallback. These source-owned images use BIOS none. Host sanitizers establish host safety, not guest correctness.

### 6. CI and rights mechanics

Generated ignored ROMs are invisible to --tree. Scan the exact output directory with an exact-name temporary binary/extension allowlist; known logo and BIOS comparisons are never exempt. Validate map inputs as project objects only; unresolved-symbol scan alone cannot show no statically linked runtime. Never upload ROMs merely to share build work between jobs.

Start from measured toolchain smoke-build times. Main Phase 4 jobs completed in macOS 140s, Linux129s, ASan119s, hygiene48s, aggregate5s; fixed limit180s. The earlier final PR jobs were macOS145s, Linux98s, ASan124s. Keep setup/compile/test distinct from emulator throughput. Choose mandatory diagnostic job placement from evidence; a focused new toolchain/guest job is justified only by its distinct recurring failure class and avoiding duplicate large installs. Keep canonical command/inventory aligned, do not add caches/concurrency/dependencies or change limits by default.

Fixtures are original owner-controlled source authored for this authorized work. Record narrow fixture-specific public-CI use and notices; do not infer a broad license or redistribution grant, copy MIT from another project, create LICENSE, or admit synthetic Phase 4 rows as real fixtures. No private ROM, BIOS or third-party suite is used. Exact content outside this original/necessary-CI scope remains an owner gate.

## Don't Hand-Roll

- Do not add a second ISA interpreter/predictor, guest simulator for tests, runtime JSON/manifest parser, SDK/startup/runtime, archive library dependency, package manager or compiler fallback. Reuse canonical decoded execution, CMake validators, original tiny assembler sources and installed standard archive/tool primitives with explicit controls.
- Do not build another hash implementation or weaken generic fixture fetch limits for a large developer tool.
- Do not re-implement expected results by calling the helper under test; use explicit independent vectors/source derivations.

## Common Pitfalls and Adversarial Checks

- Pinning ARMv4T only via flags, accepting a malformed/missing tag, or ignoring executable opcodes: inspect input objects and final ELF plus disassembly, and use actual guest cases.
- Pinning digest before parity, accepting different source/tool/ROM identities, stale generated outputs/indexes after failure, or comparing synthetic producer output: phase ordering, separate identities, real source+output mutations and registered production runner controls.
- Permitting all skips in CI, treating every tool error as missing, running old cached image after build failure: explicit missing-dependency control versus mandatory present/ran guard and stale-output cleanup.
- Archive redirects logging signed tokens, broad host/cap expansion, hash after extraction, escaping member/link/special node, partial install overwriting previous tool: bounded network-free failure controls and real exact-pin smoke installation.
- Dynamic PC target or bank writeback causing failure after prior bus effects; block maximum-work unit exceeding cap/trace/sequence/counter: exact/one-short tests with non-default whole state and buffers; maximum register lists; semantic mutants with clean companions.
- Empty/base-in-list/user-bank/mode return rules borrowed from ARMv5; Thumb BL fusion; guessed multiply C; ARM SWI byte truncation: version-tagged CPU-1–9 expectations and full family/form ledger.
- Main/source tree scan passing while ignored ROM leaks logo or linked startup/runtime: exact generated-directory scan and map gate with planted controls.

## Validation Architecture

Retain CTest/native assertion executables and sorted per-lane inventory. The existing canonical commands are cmake --workflow --preset ci and fresh local AppleClang sanitizer configure/build/asan/asan-post with the existing explicit compiler override. Tests run in background. No new runtime test framework is needed.

Validation layers:
1. Tool lock/fetch/extract controls run without network: wrong host/scheme/redirect count/status/size/hash/member/path/link/type/corrupt archive, prior-install preservation and cleanup. A failed hash must never invoke extractor.
2. First real spike: exact installed archive/version/components, readelf object+ELF tags, disassembly, explicit map input audit, output signature scan, exact host ROM byte comparison and timed fresh install/build. No ROM digest admission before these facts are captured; fallback explicit.
3. Each CPU tracer has real original guest result plus native state/trace/refusal tests; bounds/overflow/sequence/maximum-list controls run with non-default state. Cover every family/form and CPU-1–9 with independent semantic expectations and selected actual source mutants.
4. Every admitted positive/broken image executes through production immutable metadata and result checker, exact case ID and identity; stale source/output/digest/index, incomplete mailbox, missing tool and unmet BIOS controls.
5. Final canonical local CI, host sanitizer/GNU parity appropriate to changed source, generated-image scan/map checks, current inventory and installed-runner regression. Inspect measured required CI and main timing before completion; stop on red hosted CI.

Plan verification commands must refer to artifacts created by ordered tasks, never assumed future files. Planner should write task-specific commands once exact target names exist; do not fabricate current passing counts. First spike may require a deliberately staged draft of the phase's one PR for hosted x64 evidence, but no final fixture digest or completed qualification is inferred until evidence satisfies the gate.

## Open Experiments and Claim Limits

Unmeasured: actual archive contents/notice/member inventory, unpacked footprint, output architecture attributes, macOS/Linux parity, fresh install/build timing, and full CPU implementation. The existing three-minute limit may constrain placement; failed hosted work renews the stop, not permission for automatic retries. Physical hardware accuracy, broad game compatibility, BIOS handling and releases remain unclaimed. Historical Phase 4 probe statuses stay unresolved and Phase 3 Nyquist metadata debt remains for milestone close.

## Sources

See all exact primary links and local file/line witnesses in the three NOTES files. Key external sources: ARM7TDMI TRM DDI0029G; ARM ARM DDI0100I; Arm ABI addenda32/AAELF32; GNU readelf/linker manuals; official xPack release API and installation documentation. GBATEK3.06 is a community GBA-specific reference, not a hardware measurement. Current-source observations and unmeasured proposed experiments remain distinct.
