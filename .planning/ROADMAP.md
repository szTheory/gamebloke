# Roadmap: Gamebloke

## Milestones

- 🚧 **v1.0 Evidence Spine** - Phases 1-6 (in progress)
- 📋 **v2.0 Time, Interrupts, DMA, First Pixels** - planned (phase numbers continue from 7)
- 📋 **v3.0 PPU Completion** - planned
- 📋 **v4.0 Audio** - planned
- 📋 **v5.0 Cartridge and Saves** - planned
- 📋 **v6.0 API, Consumer, Packaging** - planned

**About milestone labels:** "v1.0", "v2.0", and so on are GSD milestone labels, not software
versions. GSD milestone numbers and Gamebloke software version numbers are separate (AGENTS.md).
Finishing milestone v1.0 does not mean a Gamebloke 1.0 release, and no release is claimed
without owner gate G11.

## Overview

Milestone v1.0 builds the evidence spine: the parts that are expensive to change later. These
are the timing-bearing bus API, explicit CPU pipeline state, determinism, and fixture
provenance. CI and the canonical verify command come first, so every later phase lands behind
a required check with negative controls. Next comes the vocabulary that results and fixtures
use: the `agb` profile, the claim tuple, the manifest, and the result taxonomy. Then the core
is built from the bus outward. The bus API and loader come first, then the CPU execution core
and headless runner, then full ARM/Thumb coverage driven by original diagnostics built with a
proven-reproducible toolchain. Determinism gates and the first third-party CPU fixtures close
the milestone. Milestone v1.0 has no visible output. Its visible artifact is the diagnostic
result JSON. The first pixels and Playable 0 arrive in milestone v2.0.

Source: `.planning/research/SUMMARY.md` (provisional phases 1-3, split here into six phases with
cleaner verifiable boundaries). Dependency edges kept: the verify command and the manifest/result
contract come first; the bus API comes before CPU completion; diagnostics come before
determinism gates.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: CI and Verification Foundation** - One canonical verify command, the required `CI required` check, sanitizer lanes, test inventory, hygiene and signature scans, each with negative controls (completed 2026-11-09)
- [ ] **Phase 2: Profile, Fixture Manifest, and Result Contract** - Versioned `agb` profile, claim tuple, manifest and linter, pinned fetch, result taxonomy, and dispute registry
- [ ] **Phase 3: Bus API, Memory Map, and ROM Loading** - Final cost-returning bus API on a u64 master clock, full memory map, deterministic power-on state, and a loader that refuses bad input safely
- [ ] **Phase 4: CPU Execution Core and Headless Runner** - Explicit-pipeline ARM7TDMI with modes and exceptions, and a runner that emits one bounded JSON result with mailbox, peek, and trace
- [ ] **Phase 5: Toolchain Spike, Original Diagnostics, and ARM/Thumb Completion** - Reproducible pinned toolchain, project-authored diagnostics with negative variants, and full ARMv4T ARM and Thumb coverage
- [ ] **Phase 6: Determinism Gates and Third-Party CPU Fixtures** - Repeat, optimization-level, two-compiler, and interleave equality, the bus-only time gate, and jsmolka CPU suites through a verified result channel

## Phase Details

### Phase 1: CI and Verification Foundation
**Goal**: From the first code commit, one command gives the same verdict locally and as the required GitHub check. It builds a skeleton core and runner, proves every expected test ran, runs sanitizers, and guards the public repository against rights and hygiene leaks. Every gate has a negative control.
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: VERIF-01, VERIF-02, VERIF-03, VERIF-05, VERIF-06, VERIF-07, EVID-04
**Success Criteria** (what must be TRUE):
  1. `cmake --workflow --preset ci` passes locally on macOS arm64 against a skeleton C17 core library and runner (CMake 3.25 floor, preset schema 6, Ninja). The GitHub Actions check named `CI required` runs that same workflow preset on pull requests and on `main`, on Linux (GCC) and macOS arm64, with every action pinned by commit SHA.
  2. A pull request that changes only `.planning/` skips the heavy lanes and still reports `CI required` as passed. A pull request that changes source runs every lane.
  3. ASan and UBSan builds of the core and runner run in CI. A deliberate undefined-behavior negative control, such as a shift by 32, is caught by the UBSan lane.
  4. The CTest plus junit test-inventory check fails when an expected test is removed or silently skipped, and it lists every skipped test with its reason.
  5. The hygiene check fails on planted credentials, non-noreply email addresses, and absolute home-directory paths. The signature scan fails on planted Nintendo-logo, known-BIOS, and out-of-allowlist ROM/save signatures. These negative controls are synthesized at test time, and no real logo or BIOS bytes are committed. If any negative control passes, the verify command itself fails.
**Plans**: 11/11 plans complete (waves 1-11, serial chain: shared edits to CMakeLists.txt, CMakePresets.json, .github/workflows/ci.yml, tests/ci_gates.cmake, and the per-lane inventories, then publication order)

Plans:
**Wave 1**
- [x] 01-01-PLAN.md — Tracer: skeleton core/runner, `ci` workflow preset with verbose post step, configure-time inventory compare, junit post step

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 01-02-PLAN.md — Gate controls: full junit inventory contract, ExpectFail wrapper and meta-control, registration-refusal controls, test-time junit shapes, core/runner contract tests

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 01-03-PLAN.md — Hygiene gate (credentials, non-noreply email, home paths) with planted controls, `hygiene` lane, opt-in staged hook with its controls

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 01-04-PLAN.md — Signature scan (logo window, BIOS digest, ROM/save extension, unlisted binary) with synthesized controls, digest-list well-formedness and owner-local mismatch controls; owner-pending logo row reported as SKIPPED

**Wave 5** *(blocked on Wave 4 completion)*
- [x] 01-05-PLAN.md — `asan` lane (Clang ASan+UBSan, shift-by-32 and heap-overflow controls) and local Linux runner (Docker optional)

**Wave 6** *(blocked on Wave 5 completion)*
- [x] 01-06-PLAN.md — CI routing: classifier, `CI required` aggregator, ci.yml on ubuntu-26.04/macos-26 with title job and compiler-id guard

**Wave 7** *(blocked on Wave 6 completion)*
- [x] 01-07-PLAN.md — Action-pin and workflow policy gates, each rule with a mutation control on a copy of the real ci.yml

**Wave 8** *(blocked on Wave 7 completion)*
- [x] 01-08-PLAN.md — Ruleset file and guarded, idempotent apply/check script (drift check ignores server-filled defaults) with offline controls for every GitHub-facing branch

**Wave 9** *(blocked on Wave 8 completion)*
- [x] 01-09-PLAN.md — CMake 3.25 floor and Linux pre-check evidence, developer guide, PR B (source) published, timed, and merged

**Wave 10** *(blocked on Wave 9 completion)*
- [x] 01-10-PLAN.md — PR A (.planning-only) proves skip routing; ruleset applied last; GSD switched to phase branches; owner decides close-out publication

**Wave 11** *(blocked on Wave 10 completion)*
- [x] 01-11-PLAN.md — Owner supplies the real logo digest (or it stays visibly owner-pending); close-out published or kept local per the owner's decision
**Research flag**: Standard patterns (sibling-proven in cogmaster, nesturbator, gabbaboy). Check the GitHub runner image names (`ubuntu-*`, `macos-*`) against GitHub's current list, because the research did not verify them [UNK].
**Owner gates**: None blocking. G1 (license) is surfaced but not blocking: the repo is public with no LICENSE file, so default copyright applies until the owner chooses. Do not add a LICENSE file without the owner. G11 is untouched because CI publishes no release artifacts.

### Phase 2: Profile, Fixture Manifest, and Result Contract
**Goal**: Results and fixtures have a defined, machine-checked vocabulary: a versioned `agb` profile, a claim tuple on every result, manifest rows with provenance, a SHA-256 pinned fetch, a nine-state result taxonomy, and a dispute registry. Nothing can claim more than its evidence supports.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: PROF-01, PROF-02, EVID-01, EVID-02, EVID-03, EVID-05, EVID-06
**Success Criteria** (what must be TRUE):
  1. A versioned `agb` profile document records machine `agb`, boot `direct-start`, BIOS provider, ROM-only cartridge, timing constants each with a source label, and an unmodeled list naming SP/Micro machine-visible differences, prefetch, and the HBlank edge (U1). A schema check run by the verify command rejects a profile that is missing any of these.
  2. The manifest linter accepts a complete row. It rejects, each with a stated reason, a row with a missing field, a row pinned to a branch instead of a commit, and a row with an unknown provenance class. The linter is registered in the canonical `cmake --workflow --preset ci` command at the `manifest.lint` extension point left by Phase 1, which completes the "lints the manifest" clause of VERIF-01.
  3. Fetching a pinned third-party fixture by SHA-256 from `pins.txt` puts it in an untracked location. A digest-mismatch negative control exits non-zero with a loud error, and `git ls-files` lists no third-party binary.
  4. The result schema defines PASS, GUEST_FAIL, GUEST_STOP, HARNESS_FAIL, TIMEOUT, CANCELLED, MISSING, SKIPPED, and REFUSED, plus the claim tuple `{machine, boot, bios_provider, cart, input_timeline, evidence}`. Its validator rejects a BIOS provider outside the R1 values and rejects a PASS for a fixture that did not run.
  5. The dispute registry holds the seeded entries (DMA latch, GBATEK errata, HBlank edge sources, mGBA-suite #22/#24/#27). A fixture whose expected value points at a registry entry produces a result labeled disputed.
**Plans**: 1/7 plans executed (waves 1-7, serial chain: shared edits to tests/gates.cmake and the ci and hygiene inventories; final counts ci 252 expected, hygiene 182, asan 16 unchanged)

Plans:
**Wave 1**
- [x] 02-01-PLAN.md — Tracer: strict JSON loader, `contracts/evidence.json` vocabulary (nine states as data, labels, classes, claim fields), contract validator in the ci command, json and contract controls

**Wave 2** *(blocked on Wave 1 completion)*
- [ ] 02-02-PLAN.md — Dispute registry seeded with six verified entries and its validator, with a control per reason code

**Wave 3** *(blocked on Wave 2 completion)*
- [ ] 02-03-PLAN.md — Versioned `agb` profile, `docs/profile-agb.md` with doc-sync check, R1 BIOS-provider grammar, profile validator and controls

**Wave 4** *(blocked on Wave 3 completion)*
- [ ] 02-04-PLAN.md — Fixture manifest (zero rows) and example row, pins.txt lint with a required header line, `manifest.lint` registered in the canonical command, manifest and pins controls

**Wave 5** *(blocked on Wave 4 completion)*
- [ ] 02-05-PLAN.md — `scripts/fetch-pin.sh` verify-before-place with `file://` controls, `.fixtures/` ignored and untracked gate, curl in the Linux lane

**Wave 6** *(blocked on Wave 5 completion)*
- [ ] 02-06-PLAN.md — Result validator: claim tuple, state rules, R1 provider, dispute disclosure, with controls and all-states positive

**Wave 7** *(blocked on Wave 6 completion)*
- [ ] 02-07-PLAN.md — Contract guide and verify-guide reason codes, CMake 3.25.3 floor run, Linux lane, timings evidence, validation sign-off
**Research flag**: Standard patterns (manifest, linter, and pinned fetch are sibling-proven). Settle SP and AGS model naming in the profile document using the R8 position.
**Owner gates**: None blocking. Reversible defaults apply: G3 (pinned fetch, no third-party binaries in tree) and G2 (no logo bytes in owned fixtures). G9 (differentiator wording) uses the working wording and does not block engineering.

### Phase 3: Bus API, Memory Map, and ROM Loading
**Goal**: The canonical core has its final timing-bearing bus API, the full GBA memory map, deterministic power-on state, and a ROM loader that validates untrusted input before it allocates memory or mutates state.
**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: BUS-01, BUS-02, BUS-03, LOAD-01, LOAD-02, LOAD-03
**Success Criteria** (what must be TRUE):
  1. Bus tests show that every access returns a cycle cost and a sequential/non-sequential class, and that it advances one `u64` master clock counted in 16,777,216 Hz cycles. The profile labels any placeholder costs as placeholders.
  2. Decode tests cover BIOS, EWRAM, IWRAM, I/O, PRAM, VRAM, OAM, and Game Pak ROM with their documented mirrors and 8/16/32-bit width rules, each case citing its GBATEK 3.06 section. Unmapped reads return the current open-bus latch value.
  3. Two fresh instances have byte-identical memory and register contents at power-on, and those contents match the fill policy recorded in the profile.
  4. Zero-length, below-header-size, above-32-MiB, and odd-length images each give a distinct REFUSED reason. A full-state digest and a caller-buffer comparison prove that a refused load leaves both byte-identical. After a successful load, changing the caller's buffer does not change the instance.
  5. Under `direct-start`, a ROM with a missing or mismatched logo, a bad header checksum, or an unusual game code still loads, and each finding is reported as a loader diagnostic, not a refusal.
**Plans**: TBD
**Research flag**: Light research. Read the GBATEK 3.06 memory-map, mirror, width, and open-bus sections. Power-on RAM contents are [UNK], so the fill policy is a documented project choice, not a hardware claim. Cite GBATEK 3.06, not the 2.9b-derived fork.
**Owner gates**: None blocking. G1 reuse rule: do not copy code from GPL (NanoBoyAdvance) or MPL (mGBA) sources; read them for behavior only.

### Phase 4: CPU Execution Core and Headless Runner
**Goal**: A fresh instance executes ARM7TDMI code only through the bus, with explicit pipeline, open-bus, mode, and exception state. The headless runner turns each bounded run into one JSON result that keeps status, semantic result, machine time, claim, and limits separate.
**Mode:** mvp
**Depends on**: Phase 3
**Requirements**: CPU-03, CPU-04, CPU-05, PROF-03, RUN-01, RUN-02, RUN-03, RUN-04
**Success Criteria** (what must be TRUE):
  1. CPU tests use instruction words built in memory by test code, with no committed ROM files. They show PC reads +8 in ARM state and +4 in Thumb state, the open-bus latch updated as explicit state, and no decode cache or fused-instruction path. Shift and overflow helper tests cover shifts of 32 or more, `ROR #0`/RRX, and signed overflow, and the UBSan lane stays clean.
  2. Mode and exception tests show the correct register bank, return address, and SPSR for every ARM7TDMI mode and exception entry. Under BIOS provider `none`, an IRQ entry, SWI, undefined instruction, or coprocessor opcode each ends the run as GUEST_STOP with the cause named, never as silent continuation.
  3. Running the runner on a ROM image with a cycle or frame bound produces one JSON document with separate `status`, semantic result, machine time in cycles, claim tuple, and `limits` fields. The semantic result is read from the IWRAM mailbox by `peek`, and a test shows that `peek` leaves the state digest and the master clock unchanged.
  4. A trace run with a category mask writes only the selected categories (instruction, bus access, exception) into a bounded buffer. With an undersized buffer, the run ends with an explicit output-full status and drops no records silently.
  5. The runner CLI is installed with `cmake --install` to one prefix, then moved to another directory. From there it runs and produces a JSON result identical to the one from the build tree.
**Plans**: TBD
**Research flag**: Needs research. Read the ARM7TDMI TRM (ARM DDI 0029G) and the GBATEK 3.06 CPU chapters in full, with ARMv4T version tags, before planning. Start the CP-3 post-boot state table for `direct-start` with BIOS provider `none`.
**Owner gates**: None blocking. G1 reuse rule as in Phase 3.

### Phase 5: Toolchain Spike, Original Diagnostics, and ARM/Thumb Completion
**Goal**: Project-authored ARM and Thumb diagnostics are built from source with a pinned toolchain whose output is proven to be ARMv4T and reproducible. The diagnostics drive the CPU to full ARMv4T ARM and Thumb coverage, and every quirk is pinned by a case ID with a deliberately broken variant as its negative control.
**Mode:** mvp
**Depends on**: Phase 4 (the runner and mailbox read diagnostic verdicts) and Phase 2 (manifest rows for owned fixtures). The toolchain-spike plan does not depend on the core and may start first within the phase.
**Requirements**: DIAG-01, DIAG-02, DIAG-03, VERIF-04, CPU-01, CPU-02
**Success Criteria** (what must be TRUE):
  1. Before any ROM digest is committed, the toolchain spike records `Tag_CPU_arch: v4T` from `readelf -A` and byte-identical diagnostic ROMs built on macOS arm64 and on Linux. If the bytes differ, digest ownership moves to Linux and the decision is logged.
  2. CI builds the original ARM and Thumb diagnostics from source with the pinned xPack `arm-none-eabi` toolchain, the project crt0, linker script, and header fixer. Each has a manifest row. The signature scan and a link-map check confirm no logo bytes and no linked runtime library.
  3. When the toolchain is absent, or when a fixture's declared BIOS requirement cannot be met, the affected tests report SKIPPED with a reason in the inventory. They are never PASS and never fall back silently.
  4. Each diagnostic run through the runner yields PASS from its mailbox verdict. Its deliberately broken variant yields GUEST_FAIL and names the failing case ID.
  5. Diagnostic case IDs cover barrel-shifter carry, LDM/STM with an empty list, base in list, and user-bank transfer, misaligned-load rotation, multiply flags, PSR transfers, and the Thumb BL prefix and suffix executed as two separate instructions. All of them pass.
**Plans**: TBD
**Research flag**: Needs research plus a gating spike. The xPack toolchain identity (ARMv4T output, libgcc division resolution, identical bytes across hosts) is [UNK]. The fallback is Ubuntu apt binutils for CI, with digest ownership on Linux. The CPU quirk list (PITFALLS CPU-1 to CPU-9) needs a test pin for each case.
**Owner gates**: None blocking. G2 default (no logo bytes in owned fixtures) is enforced by the scan; it is reversible. The license of the owned diagnostics follows G1, which is surfaced but not blocking. Do not use devkitARM, libgba, or gbafix for owned fixtures.

### Phase 6: Determinism Gates and Third-Party CPU Fixtures
**Goal**: The evidence spine is closed. Repeatability is proven across runs, processes, optimization levels, compilers, and interleaved instances. Machine time cannot advance outside the bus API. jsmolka's CPU suites count as evidence only through a result channel verified against their source.
**Mode:** mvp
**Depends on**: Phase 5
**Requirements**: DET-01, DET-02, DET-03, BUS-04, DIAG-04
**Success Criteria** (what must be TRUE):
  1. Repeating a run with the same profile, ROM, and input, both in-process and in a fresh process, produces byte-identical JSON results and trace hashes.
  2. For every owned diagnostic, -O0 and -O2 builds and GCC and Clang builds produce identical trace hashes. Two instances run interleaved in one process each produce the same results as when run alone.
  3. The trace-hash gate fails, for a stated reason, on a negative-control handler that advances machine time without going through the bus API.
  4. jsmolka gba-tests `arm`, `thumb`, and `memory` run from a pinned fetch. They count as PASS only after the r12-at-idle-loop channel has been checked against `m_test_eval` at the pinned revision, and the manifest row records that check. A failing run ends as GUEST_FAIL or GUEST_STOP, never PASS. `bios.gba` reports SKIPPED with a BIOS-requirement reason.
  5. At milestone exit, `cmake --workflow --preset ci` and `CI required` are green, every negative control fails for its stated reason, and the inventory shows the skipped BIOS fixtures with their reasons.
**Plans**: TBD
**Research flag**: Standard patterns for sanitizers, two-compiler trace equality, and interleave tests. One small targeted source read: jsmolka `m_test_eval` and the idle loop at the pinned commit (R6).
**Owner gates**: None blocking. G3 default (pinned fetch; jsmolka is never committed) and G2 (jsmolka's `lib/header.asm` embeds the logo, so fetched ROMs stay untracked and outside the scanned tree) are reversible defaults. G7 is untouched: no official BIOS is used, and `bios.gba` is SKIPPED.

## Planned Milestones (rolling)

These milestones are planned, not scheduled. Each is promoted with `/gsd-new-milestone`, which
refreshes the research and the owner gates first, then adds that milestone's phase sections.
Phase numbers continue from 7. This roadmap intentionally has no phase sections for them.

- 📋 **v2.0 Time, Interrupts, DMA, First Pixels**: The first visible output. A master-clock scheduler, IRQs, timers, HALT, keypad, the `project-stub` BIOS, bus wait states and prefetch, SRAM, DMA, PPU timing, and a mode-3 renderer, ending with **Playable 0**.
  - Requirements: TIME-01, TIME-02, TIME-03, TIME-04, INPUT-01, BIOS-01, BUS-05, CART-01, DMA-01, PPU-01, PPU-02, PLAY-01, EVID-07
  - Owner gates: G4 (stub authoring approval, before stub work begins), G6 (playable asset authorship; the owner judges playability), G8 (devkitPro terms for the mGBA-suite devkitARM deep lane)
  - Research: HIGH for bus/prefetch (U2), DMA (U3, latch dispute), and PPU timing (U1). Moderate for the stub SWI subset and post-boot state (U7)
- 📋 **v3.0 PPU Completion**: Modes 0-5, OBJ, windows, blending, mosaic, then VRAM/PRAM/OAM contention and register latch timing.
  - Requirements: PPU-03, PPU-04
  - Owner gates: none blocking by default. G12 for any hardware-accuracy claim
  - Research: HIGHEST project risk (latch points, hw-test `expected.png` provenance)
- 📋 **v4.0 Audio**: PSG channels 1-4 on the GBA register map, Direct Sound FIFO A/B with FIFO DMA, SOUNDBIAS, and a canonical raw 262,144 Hz u16 model stream with its own digest. **Playable 1** (interrupt-driven through the stub, with sound and SRAM) lands here or in v5.0.
  - Requirements: AUD-01, AUD-02, AUD-03, PLAY-02
  - Owner gates: G6 (Playable 1 acceptance)
  - Research: MEDIUM-HIGH (U4 PSG output reduction, FIFO underrun and overrun)
- 📋 **v5.0 Cartridge and Saves**: Declared Flash, EEPROM, and FRAM devices, media copy and import with generation counters, an advisory-only ID scanner, and ROM length and malformed-content refusals. Can run alongside v3.0 and v4.0 once v2.0 DMA lands.
  - Requirements: CART-02, CART-03, CART-04, CART-05 (and PLAY-02, if it slips from v4.0)
  - Owner gates: none blocking by default. G12 for hardware flash claims
  - Research: MEDIUM (EEPROM width and DMA3 length protocol)
- 📋 **v6.0 API, Consumer, Packaging**: A public C API with its lifecycle and refusal document written first, a symbol scan and loader fuzzing, a relocatable CMake package with installed C/C++ consumers, cross-platform hash checks, and an optional user-supplied official BIOS mode.
  - Requirements: API-01, API-02, PKG-01, BIOS-02 (optional)
  - Owner gates: G1 (license, before packaging), G11 (publication of any release), G7 (BIOS-02 private use)
  - Research: standard patterns (sibling-proven)

Later (v1.x and beyond, not scheduled): STATE-01, LF-01, BIOS-03 (G5), ACC-01, DBG-01, PERF-01.

**Where the playables land:**
- Milestone v1.0: none. The visible artifact is the diagnostic result JSON.
- **Playable 0**: end of milestone v2.0. Original, in-tree, mode-3, polling only, with no BIOS, audio, or save, plus an IRQ-driven variant using the stub. Accepted only by the owner's playability judgment (G6).
- **Playable 1**: milestone v4.0 or v5.0. Interrupt-driven through the stub, with sound and SRAM (G6).

## Owner Gates

From `.planning/research/SUMMARY.md`. Milestone v1.0 needs no blocking owner gate. Its fixture
policy runs on reversible defaults: pinned fetch, no third-party binaries in the tree, and no logo
bytes in owned fixtures.

| ID | Gate | Status | Default in use | Blocks (earliest milestone) | Next action |
|----|------|--------|----------------|-----------------------------|-------------|
| G1 | Project license | Open, surfaced, not blocking v1.0 | None chosen; default copyright applies to the public repo. MIT recommended (MEDIUM). No GPL/MPL code copying | v6.0 packaging; any external contributor or reuse | Owner decides, before v6.0 packaging at the latest |
| G2 | Nintendo logo bytes in committed or published fixtures | Default applied (reversible) | No logo bytes in owned fixtures; signature scan enforces it | Any logo-bearing committed fixture or "boots on a real BIOS" claim (earliest v6.0 BIOS-02) | Decide before any logo-bearing fixture is considered |
| G3 | Third-party fixture vendoring | Default applied (reversible) | Pinned SHA-256 fetch; no third-party binaries in tree | Committing any jsmolka, mGBA suite, hw-test, or SingleStepTests file | Decide before any third-party file is committed |
| G4 | BIOS stub authoring approval (clean-room posture) | Open | Approve recommended | v2.0 (BIOS-01, before stub work begins) | Owner approval at v2.0 kickoff |
| G5 | Replacement BIOS use (emibios, Cult-of-GBA) | Open | emibios pinned differential only; Cult-of-GBA as a v1.x fixture. Only the owner calls a replacement "clean" | Later (BIOS-03); any replacement-BIOS fixture or claim | Decide before the first replacement fixture |
| G6 | Playable asset authorship and policy | Open | Original, in-tree, from-scratch assets; owner judges playability | v2.0 (PLAY-01 acceptance); v4.0/v5.0 (PLAY-02) | Confirm authorship approach before the Playable 0 phase |
| G7 | Proprietary and private oracles (AGS, owner BIOS dump, SingleStepTests) | Open | Never in repo, CI, artifacts, or screenshots | v6.0 (BIOS-02); any AGS claim; SingleStepTests deep lane; U7 verification | Owner decision before any is used |
| G8 | devkitPro redistribution terms; r67 vs r68 image identity | Open | Local use or pull by digest only; no mirror or cache | v2.0 (EVID-07 mGBA-suite devkitARM deep lane) | Resolve before any image cache or mirror |
| G9 | Differentiator wording | Working wording stands | R4 wording, contract-first | Public positioning only; not engineering | Owner confirms when convenient |
| G10 | First outcome direction (Playstead integration or a quick visible demo) | Default applied (reversible) | Spine first (this roadmap) | Milestone shape | Owner may revisit at any milestone boundary |
| G11 | Publication (release artifacts, announcements) | Open | No release claimed | Any release (earliest v6.0 close) | Decide at each milestone close |
| G12 | Physical hardware (captures, AGB vs Micro, subjective playability, controller) | Open (human gate) | No hardware claims | Any hardware-accuracy claim; `agb-micro` | When a claim needs it |
| G13 | Playstead integration | Not assumed | No integration work | Any integration work (none planned) | Separate owner decision |

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. CI and Verification Foundation | 11/11 | Complete    | 2026-11-09 |
| 2. Profile, Fixture Manifest, and Result Contract | 1/7 | In Progress | - |
| 3. Bus API, Memory Map, and ROM Loading | 0/TBD | Not started | - |
| 4. CPU Execution Core and Headless Runner | 0/TBD | Not started | - |
| 5. Toolchain Spike, Original Diagnostics, and ARM/Thumb Completion | 0/TBD | Not started | - |
| 6. Determinism Gates and Third-Party CPU Fixtures | 0/TBD | Not started | - |

---
*Roadmap created: 2026-11-09 for milestone v1.0 Evidence Spine (a GSD milestone label, not a software version)*
