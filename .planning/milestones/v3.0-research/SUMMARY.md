# Project Research Summary

**Project:** Gamebloke
**Domain:** Deterministic, accuracy-focused Game Boy Advance emulator: portable C core, headless runner, thin host adapters
**Researched:** 2026-11-09
**Synthesized from:** `.planning/research/STACK.md`, `FEATURES.md`, `ARCHITECTURE.md`, `PITFALLS.md` (all dated 2026-11-09), checked against `.planning/PROJECT.md` and `AGENTS.md`
**Confidence:** MEDIUM overall. HIGH for language, build, fixture-rights facts, and frame arithmetic. MEDIUM for the timing and PPU model. LOW for edge-cycle alignment, prefetch detail, and PSG output reduction.

---

## Evidence labels

The four research files used different tag sets. Every non-obvious claim below carries one of these labels.

| Label | Meaning | Mapped from the research files |
|-------|---------|--------------------------------|
| [OWNER] | Owner decision, recorded in PROJECT.md or AGENTS.md | ARCHITECTURE `[OWNER]` |
| [SRC] | Primary-source fact read directly on 2026-11-09: vendor document, upstream repo file, license file, package index, API | STACK `[PF]`; FEATURES `[FACT]` where primary; PITFALLS `[PRIMARY]`; ARCHITECTURE claim-check primary rows |
| [REF] | Community hardware reference (GBATEK, Tonc, NBA hw-docs). Makes a hardware claim. Not Nintendo-issued and not verified against hardware | FEATURES and ARCHITECTURE `[REF]`; PITFALLS `[GBATEK]` |
| [IMPL] | What a specific emulator, test suite, sibling repo, or local probe does. True of that artifact, not of hardware | STACK `[OBS]`; ARCHITECTURE `[IMPL]` `[TEST]`; PITFALLS `[EMU-SRC]` `[TEST-SRC]` |
| [INF] | Inference, or arithmetic checked in research. May be wrong | `[INF]`, `[ARITH]`, `[INFERENCE]` |
| [REC] | Recommendation made by this synthesis | New in this file |
| [UNK] | Not determined. Do not build on it without a spike | `[UNK]`, and `[KNOWLEDGE]` (untested background) |

Issue-tracker and forum reports are [SRC] only for the fact that the report exists. Any hardware claim inside a report is [UNK] until someone reproduces it.

---

## Executive Summary

Gamebloke answers a narrower question than a player-facing emulator does: given a named profile, a ROM, and timed logical input, what does a fresh instance do, and what exactly does the evidence prove? Accuracy in this domain comes from three things: an exact per-access timing model (ARM7TDMI, wait states, Game Pak prefetch), an interrupt path that runs through BIOS code, and a PPU that latches registers at specific points inside each scanline. NanoBoyAdvance, SkyEmu, and mGBA already cover much of that ground [SRC]. None of them publishes a determinism contract or separates results into machine-readable evidence classes [SRC]. mGBA 0.10.5, the version Playstead pins, has no non-interactive scripting [IMPL]. That gap, plus the contract, is Gamebloke's place. Raw accuracy is not the differentiator.

The recommended build: C17 with CMake (3.25 floor) and Ninja, following the owner's sibling-repo conventions [IMPL] [REC]. A canonical core with no host dependencies runs on a master-clock bus. Every access returns a cost and advances one `u64` scheduler counted in 16,777,216 Hz cycles. The first profile is `agb`, booted direct-start with a project-authored BIOS stub, and no Nintendo bytes ship [REC]. Fixtures are manifest rows that record provenance, license, and expected-value class. The first milestone is an evidence spine (CPU, bus API, runner, manifest, determinism gates), not a wide vertical slice. The first visible output is an original, in-tree, polling-only mode-3 game at the end of milestone 2 [REC].

The main risks are: (1) choosing timing or pipeline granularity late, which forces a CPU rewrite; (2) letting BIOS policy drift by accident, since the IRQ vector lives in BIOS space; (3) treating test-suite expected values or emulator agreement as hardware truth, when no public suite is a hardware oracle; (4) rights exposure from Nintendo logo bytes and third-party binaries; and (5) an overscoped first milestone. Several decisions need the owner: the project license (none chosen yet), approval of the BIOS stub approach, the logo and vendoring policy, playable-asset authorship, and any use of proprietary or privately held oracles. The full gate list is in the Owner gates section.

---

## Key Findings

### Recommended stack (STACK.md)

C17 (`CMAKE_C_STANDARD 17`, extensions off) for the core, runner, and public API, matching gabbaboy, nesturbator, cogmaster, and libfuturo [IMPL]. C over C++ (no gain, more ABI surface) and over Rust (memory safety is real, but almost none of the portfolio harness transfers to Rust). Confidence is HIGH against C++ and MEDIUM against Rust. Revisit only if fuzzing finds memory-safety defects in the loader or state codec after validators exist [REC].

Build: CMake with a 3.25 floor and preset schema 6 (3.25 is the first release with workflow presets; schema 11 would force CMake 4.3 on consumers) [SRC]. Ninja. One canonical command, `cmake --workflow --preset ci`, shared by local runs and the required CI check [IMPL] [REC]. No test framework: CTest plus a small check header and a junit inventory check that proves every expected test ran [IMPL]. In-tree SHA-256. No Python build dependency.

Owned ROM fixtures: assembly first, using the pinned xPack `arm-none-eabi` 15.2.1-1.1 release (SHA-256 pinned per host) [SRC], with a project crt0, a linker script, and a roughly 50-line header fixer [REC]. Do not use devkitARM, libgba, or maxmod for owned fixtures (their licenses travel into every ROM), gbafix (GPL), or `-ffast-math` builds (codegen drifts with compiler version) [SRC] [INF]. Unverified: that xPack emits ARMv4T code and identical bytes on macOS arm64 and Linux [UNK], so a spike must confirm it before any digest is committed.

Third-party fixtures, pinned by commit SHA and never by branch [REC]: jsmolka gba-tests first (MIT; `arm`, `thumb`, `memory`, `ppu`, `save`) [SRC]. mGBA suite and NBA hw-test in a devkitARM Docker deep lane pinned by image digest [SRC]. SingleStepTests as a small deep-lane subset, labeled differential [SRC]. Reject: the ARMwrestler fork (no license file), AGS Aging (proprietary Nintendo), Butano Fighter (NC-SA music), Google-Dino-GBA (Google art), Celeste GBA (no license), and gbatris (separate music copyright) [SRC].

CI shape: one required check named `CI required`; PR lanes for hygiene, title, build and CTest (GCC on ubuntu, arm64 macOS as the owner's host), ASan and UBSan (clang-21, Linux), and owned-fixture digest and judge checks. Scheduled lanes for nofp, be32 hash-equality, wasm32, libFuturo fuzzing, and devkitARM third-party suites [IMPL] [REC]. The runner image names are unverified against GitHub's current list [UNK].

### Expected features (FEATURES.md)

**Table stakes (v1, machine level):** ARM7TDMI ARM and Thumb; memory map with wait states, prefetch, and open bus; interrupts, four timers, and four DMA channels with arbitration; PPU with six display modes, sprites, affine transforms, windows, blending, and mosaic; four PSG channels plus two Direct Sound FIFOs with SOUNDBIAS; keypad as timed logical input; SRAM, Flash, and EEPROM with a declared device; BIOS handling per R1 below. Player features (game window, audio playback, save states, rewind, fast-forward, shaders, cheats) are host or later work, not core v1 [REC].

**Differentiators:** a deterministic fresh-instance runner with timed input; semantic results with distinct failure classes and a "proves / does not prove" field; a guest-time event trace; an evidence-labelled claim tuple per profile; an explicit cartridge and backup declaration with advisory-only detection; strict API postconditions [INF] [REC].

**First profile:** `agb`, boot mode `direct-start`, BIOS provider as its own field in every claim. The SP is `agb` plus a host presentation preset, claimed only with evidence. The Micro is deferred (`agb-micro` needs owner hardware) [REC]. Claim tuple proposed by FEATURES: `{machine, boot, bios_provider, cart{rom_size, backup_device}, input_timeline, evidence}` [REC].

**Cartridge v1:** ROM up to 32 MiB with header diagnostics; SRAM first; EEPROM 512 B and 8 KiB and Flash 64K and 128K complete v1; FRAM treated as labeled SRAM-class; Atmel 128-byte-sector flash, multicart, and multiboot deferred. No game-code or title save database, ever. Save type is declared, never inferred [OWNER] [REC].

**Deferred, each with promotion evidence:** RTC and GPIO (first accessory candidate), solar, tilt and gyro, rumble, link cable and SIO, Wireless Adapter, e-Reader, GB/CGB mode (owner decision already), HLE BIOS, GDB stub, user save states, rewind, Lua (never) [REC].

**Anti-features:** bundled BIOS, commercial ROMs, or private saves; a title-keyed save database; a single compatibility percentage or "passes AGS" headline; HLE presented as real BIOS; embedded Lua; FFmpeg or GIF capture in core; wall-clock RTC; "passes because the runner did not run it" [REC].

**Competitors (verified 2026-11-09):** mGBA 0.10.5 (latest tagged release) has no headless runner. Unreleased 0.11.0 on master adds a headless frontend with `--script`, an `-S` SWI exit, an `-R` register exit code, and startup scripts [SRC]. NanoBoyAdvance is cycle-accurate for most components and has no determinism guarantee [SRC]. SkyEmu has an HTTP control server with `/step`, `/input`, and `/screen`, and it disclaims determinism [SRC]. No surveyed emulator publishes a determinism contract [SRC].

### Architecture approach (ARCHITECTURE.md)

- **Core/host split.** The core has no files, wall clock, UI, A/V devices, or save-file policy [OWNER]. The headless runner and host adapters use only the public API, which makes the later installed-consumer check meaningful. A build-time dependency check should enforce the direction [REC].
- **Timing.** Every bus access returns its cost (region, width, sequential or non-sequential) and advances one `u64` scheduler in master cycles (16,777,216 Hz). Scheduler events are plain data. They never perform bus accesses and never call the CPU. DMA and IRQ acceptance happen only at arbiter slots. This is "Option B" in ARCHITECTURE, and it matches the access-granular design in PITFALLS CP-1 [REC] [INF].
- **Lazy versus scheduled state.** Pure observable values (timer counter, VCOUNT, DISPSTAT flags) are derived at read time. Events are scheduled only for side effects (IRQ, DMA request, FIFO pop, overflow) [IMPL] [REC].
- **PPU split.** A cycle-granular timing and contention model comes early. A per-line pixel renderer follows, driven by an explicit latch-point table. Raw output is 240x160 little-endian 16-bit BGR555 words with no host conversion [REC].
- **Audio.** A raw fixed-cadence stream, see R3 [REC].
- **API.** Opaque instance; no globals; no callbacks during `run`; caller-owned buffers; bounded advance by cycle budget with explicit stop reasons; timestamped input events; the caller declares the backup device; side-effect-free `peek` [REC].
- **Observability.** A category-masked fixed-record trace. The runner emits one JSON result whose fields stay separate: status, semantic channel, machine time, video digest, audio digest, trace, and a `limits` array naming what the result does not prove [REC].
- **Components:** scheduler; ARM7TDMI CPU; bus and arbiter; memory and I/O; IRQ controller; timers x4; DMA x4; PPU; APU; keypad; cartridge and backup devices; BIOS image; trace; state; public API; headless runner.

### Critical pitfalls (PITFALLS.md)

1. **Timing granularity chosen late (CP-1).** Instruction-counted time forces a rewrite of the CPU/bus boundary. Every access needs a cost and sequential/non-sequential state from milestone 1. Prevention: a per-access cost API, with costs allowed to be placeholders at first.
2. **Pipeline and prefetch treated as optimization (CP-2).** Decode caches, fused Thumb BL, and fast-ROM paths remove observable state (two-stage pipeline, open-bus latch, prefetch buffer). Prevention: explicit pipeline and open-bus model from day one. Caches only behind a trace-equivalence gate.
3. **BIOS policy by accident (CP-3).** The IRQ vector (0x18) and SWI entry live in BIOS space [REF]. Prevention: BIOS provider is a first-class claim field. Fixtures declare `requires_bios`. Each mode has its own post-boot state table.
4. **Oracle trust (CP-5).** No public suite is a hardware oracle. The mGBA suite has open issues stating expected values are wrong or fail on hardware (#22, #24, #27) [SRC]. jsmolka's `unsafe/` README says some GBATEK statements pass on mGBA but fail on hardware [SRC]. mGBA 0.10.5 and NBA hw-test disagree on whether DMA latches are shared or per-channel [IMPL]. Prevention: every expected value carries a provenance class. Only hardware capture or vendor documents can support a "hardware-accurate" claim.
5. **Rights traps (CP-6).** The 156-byte Nintendo logo is embedded in jsmolka's `lib/header.asm` [SRC], and the mGBA suite's build inserts it via `gbafix -p` [SRC]. The mGBA suite mixes MIT and BSD-2 headers and has font bitmaps with no stated provenance [SRC]. Prebuilt devkitARM binaries embed libgba [SRC]. Prevention: pinned fetch by default, and a signature scan in the verify command.
6. **Non-determinism (CP-7).** Shifts by 32 or more, `ROR #0` as RRX, and signed-overflow carry logic are undefined behavior in naive C [INF, a C-language rule; the hardware result for each case needs a test]. Power-on RAM contents are not defined by the research, so reads of uninitialized RAM may vary [UNK]. Prevention: integer-only core, shift helpers, a documented deterministic fill, sanitizer builds, and two-compiler and -O0/-O2 trace equality.
7. **Overscope and lock-in (CP-11, CP-9, CART-1/2).** The bootstrap's first slice chains five independent critical paths and two owner gates [INF]. Save-type misdetection and EEPROM size inference are common failures [REF] [IMPL]. Prevention: spine first; declared devices; no state-format promise until the layout is stable.

---

## Cross-file reconciliations

Each item states the positions, the resolution, and why. Items the files already agree on are listed at the end.

### R1. BIOS default and boot path [REC]

- **Positions.** STACK proposes "M0 = direct boot plus Gamebloke-owned HLE SWIs", with Cult-of-GBA built from source as an early test BIOS and emibios as a differential lead. FEATURES proposes direct-start with option A (no BIOS) first, option B (a project-authored minimal stub: IRQ trampoline plus SWI subset) second, Cult-of-GBA at v1.x, HLE deferred, and a user-supplied official BIOS as optional. ARCHITECTURE proposes the stub as the default no-Nintendo-bytes path, a user image as a second profile, and pure HLE as a separately labeled mode. PITFALLS requires BIOS mode to be a first-class profile field with per-mode post-boot tables.
- **Deciding fact.** Interrupt delivery goes through BIOS code, because the IRQ vector sits at 0x00000018 in BIOS ROM and SWI also enters through BIOS [REF]. HLE SWIs alone therefore cannot cover interrupts. Something in the BIOS region must take the IRQ, or the host must intercept IRQ entry, and that intercept is not guest-visible.
- **Resolution.** Two orthogonal axes. Boot path is `direct-start` for every first claim. The BIOS provider is recorded in every claim tuple, with these values:
- `none`: polling-only diagnostics. A guest that takes an IRQ or executes an SWI with no provider is classified (GUEST_STOP or refusal), never silently continued. Example: jsmolka's failure path calls SWI 6 (Div) [SRC], so under `none` a failing run must end in a classified status.
- `project-stub`: Gamebloke-authored guest code in the BIOS region. It holds the vector table, the IRQ entry at 0x18, and an SWI dispatch with a documented subset. It is the default for any IRQ- or SWI-using fixture. It is written from documented behavior, not transcribed from Nintendo's listing (owner gate G4).
- `hle:<swi-list>`: host-side SWI overlay. Allowed only per named SWI, declared in the claim tuple and in results, never the default.
- `replacement:<name>@<commit>`: Cult-of-GBA at a pinned commit, a v1.x fixture (see R7).
- `user-official@<digest>`: optional, local only, never in public CI. Public CI reports `SKIPPED`.
- **Why.** Stub-as-guest-code keeps IRQ and SWI cost visible through the bus, which the milestone 2 timing work needs. It is rights-clean by authorship. HLE stays available for speed later, under labels [REF] [INF]. STACK's M0 is right about "no Nintendo bytes" but should read "stub, with labeled HLE overlays", not "HLE SWIs".
- **Also.** Under direct-start, the boot-time header check is skipped, so header findings are loader diagnostics [REF] [REC]. Refusal on header mismatch applies only under full-boot with a real provider.
- **Naming.** Retire STACK's "M0, M1, M1b, M2" labels for BIOS modes. They collide with milestone numbers.

### R2. Milestone shape and where the playable lands [REC]

- **Positions.** PITFALLS recommends a narrow milestone 1 "evidence spine" (CPU, bus with per-access timing API, runner, manifest, determinism gates), then milestones for timing and IRQ, PPU, audio, cartridge, and API. ARCHITECTURE gives slices S0 to S9. The seed suggested a wide first vertical slice with a visible playable early. STACK recommends an owner-authored original game because no third-party title is fully cleared [SRC].
- **Resolution.** One first-milestone scope: PITFALLS milestone 1, which equals ARCHITECTURE S0 plus S1 plus the runner and gates. After it, a rolling sequence of milestones 2 to 6, mapped from S2 to S9 (see Implications). The playable lands in three steps:
- Milestone 1: none. The milestone's visible artifact is the diagnostic result block and JSON.
- End of milestone 2 (phase 7): **playable 0**, an original, in-tree mode-3 game using polling VCOUNT and KEYINPUT, with no BIOS, no audio, and no save. An interrupt-driven variant uses the stub.
- End of milestone 4 or 5: **playable 1**, interrupt-driven through the stub, with sound and SRAM.
- gba-pong (AnthonyCalandra, MIT) is conditional third-party corroboration only, after its build and attribution check. It links libgba (LGPL with attribution) and calls SWI 0xC CpuFastSet [SRC] [REC].
- **Why.** The milestone 1 items are the decisions that are expensive to reverse: timing API, pipeline state, determinism, and provenance. They need no owner rights decision beyond the fixture policy. A wide first slice chains five critical paths and two gated items, which would stall without any technical failure [INF]. Playable 0 gives the owner something visible at milestone 2 without pretending to have audio or a full PPU.
- **Owner alternative.** ARCHITECTURE's "thin vertical" option (mode 3, KEYINPUT, VBlank, and a tiny homebrew in milestone 1) is valid only if the owner needs a visible demo within weeks, or if the first outcome is Playstead integration (gate G10). Either changes the milestone shape. The default is spine first.

### R3. Audio cadence [REC]

- **Positions.** ARCHITECTURE recommends a fixed 262,144 Hz hold stream: one sample per 64 cycles, exactly 4,389 stereo samples per frame. PITFALLS AUD-3 notes that at the default SOUNDBIAS (32,768 Hz) a frame is 548.625 samples, so frame-sized buffers are not integer. STACK recommends s16le stereo "at the machine sample rate", which depends on SOUNDBIAS and is marked [UNK].
- **Arithmetic.** 280,896 cycles per frame / 64 = 4,389 [INF]. 280,896 / 512 = 548.625 [INF]. SOUNDBIAS periods are 512, 256, 128, and 64 cycles, for 32.768, 65.536, 131.072, and 262.144 kHz [REF].
- **Resolution.** The core emits the fixed 262,144 Hz stream as canonical raw audio. Each SOUNDBIAS output rate is a declared hold of that stream, which is lossless because every period is a multiple of 64 cycles. SOUNDBIAS changes are trace events. Each sample is the post-clip 10-bit unsigned mix per channel, stored as u16 little-endian with values 0 to 0x3FF. STACK's "s16le" wording is dropped: any signed mapping is a host conversion. The whole stream is labeled a **model**, not hardware-derived. How the hardware reduces the 262 kHz PSG output to the output rate is unknown (U4) [UNK].
- **Why.** An integer per-frame count, no accumulator in the core, and no rate segments for consumers. Cost: about 8 times more samples than needed at the default rate, or about 17.6 kB per frame as 16-bit stereo (4,389 x 2 x 2 bytes) [INF]. ARCHITECTURE's "about 17.5 KiB" figure should read about 17.6 kB (about 17.1 KiB). LibFuturo's single rate field and `LF_AUDIO_S16_NATIVE` fit only if hosts accept 262,144 Hz, which is unverified [UNK].

### R4. Differentiator wording [REC] [OWNER]

- **Positions.** FEATURES recommends "deterministic, evidence-labelled machine verification" and gives a draft statement: a fresh instance, a named profile, and timed logical input yield a machine-readable verdict, a guest-time trace, and raw A/V, each separate, with a statement of what each proves. Caveat: mGBA 0.10.5 has no headless runner, but unreleased 0.11.0 on master does (see Key Findings). The 0.10.5 release date appears as 2025-03-08 in FEATURES and ARCHITECTURE and 2025-03-09 in PITFALLS. Check the release record before quoting it.
- **Resolution.** Keep the wording as a working claim. The owner confirms it, and confirmation is not blocking (G9). Lead with the contract (determinism, result taxonomy, proves and does-not-prove, guest-time trace), not with "headless". Stop listing "headless runner" as a differentiator once mGBA 0.11 is tagged. Re-check mGBA's tagged releases, not its master branch, at each roadmap review. The Playstead blocker is a 0.10.5 fact [IMPL, from the sibling planning record]; do not extend it to mGBA master.
- **Why.** No surveyed competitor states a determinism guarantee [SRC]. The result taxonomy is the part that survives upstream catching up.

### R5. Language, build, and license [REC] [OWNER]

- **Positions.** STACK recommends C17, CMake 3.25 with preset schema 6, Ninja, and MIT, and lists repository visibility as an owner gate. PROJECT.md records that visibility is settled: the repository is public as of 2026-11-09. ARCHITECTURE and PITFALLS are language-neutral.
- **Resolution.** Adopt STACK's C17, CMake 3.25, and Ninja. Remove repository visibility from the gate list. License remains an open owner gate, with MIT recommended at MEDIUM confidence. Until the owner chooses, the public repository carries no license grant and default copyright applies. Surface this now rather than at packaging time [OWNER] [SRC].
- **Reuse rule.** Until the license is chosen, do not copy code from GPL (NBA master) or MPL (mGBA) references. Read them for behavior only. NBA's `phoenix` branch is BSD-2-Clause and in development; treat it the same way [SRC] [REC]. LibFuturo is Apache-2.0, not MIT, so do not assume its header license for an adapter [SRC].

### R6. jsmolka's result channel (r12) [REC]

- **Positions.** STACK states that jsmolka passes when the idle loop has r12 equal to 0, and that failure leaves the first failing test number in r12 [SRC], citing `m_test_eval`. ARCHITECTURE says register reporting is not confirmed. PITFALLS calls r12 a candidate that must be verified by reading `eval` [INF].
- **Resolution.** Treat r12 as a candidate channel, not a result, until phase 2 reads `m_test_eval` and the idle loop at the pinned revision. Until then, screen output is only a non-PASS signal [REC].
- **Why.** The three files disagree on whether this was observed, and the cost of being wrong is a false PASS or a misattributed failure.

### R7. Cult-of-GBA and emibios timing [REC]

- **Positions.** STACK places Cult-of-GBA in milestone 1 as a test-only replacement BIOS built from source with GNU `as`. FEATURES places it at v1.x, after the stub, once a named fixture needs SWIs beyond the stub.
- **Resolution.** Use FEATURES' timing. The project-authored stub covers IRQ dispatch and the early SWI subset. Cult-of-GBA becomes a pinned replacement fixture only when a named fixture needs more SWIs. Facts: MIT, (c) 2020 to 2021 DenSinH and fleroviux, last commit 2025-04-06, "high accuracy is a non-goal", several sound SWIs unimplemented, boot-screen glyph data with no stated provenance [SRC].
- **emibios.** It stays a pinned, never-committed differential lead (STACK's M1b). It is LGPL-3.0-or-later for the BIOS, with MIT tests, and version 0.2.1 was released 2026-10-26 [SRC]. Using it requires an owner decision (G5). Only the owner may call any replacement "clean" or "cleared" [OWNER].

### R8. SP and Micro models [REC]

- **Positions.** FEATURES treats the SP as the same machine core as the AGB, and the Micro as a profile difference only for GB/CGB mode [REF]. The ARCHITECTURE claim check (row 25) calls the "host-only" framing partly wrong: the 0x04000800 EWRAM-wait register is machine-visible and locks up the Micro at the fastest setting, and the Micro lacks the cartridge switch [REF] [SRC].
- **Resolution.** The v1 profile is `agb` only. The SP is `agb` plus a host preset, claimed only with evidence. GBATEK's "100% compatible" statement is a community [REF] claim. Micro-visible differences go in the profile's unmodeled list. `agb-micro` waits for owner hardware (G12) [REC].
- **Why.** The bootstrap assumption that SP and Micro differ only on the host is wrong for at least two machine-visible facts. Listing them as unmodeled keeps claims honest without creating a second profile now.

### R9. State and snapshot timing [REC]

- **Positions.** FEATURES places internal snapshot and restore at v1.x, after the layout stabilizes, with user save states and rewind deferred. ARCHITECTURE's S8 includes state save and load, including in-flight DMA, timer, and FIFO state, inside API hardening. PITFALLS CP-9 says not to promise a state format early.
- **Resolution.** The design constraint applies from milestone 1: all timing state is plain data in the instance, queued events carry no closures or function pointers, and power-on fill is deterministic. No state format and no round-trip promise until the layout is stable after milestone 6. Ship it as v1.x, as FEATURES says. The entry gate is the in-flight round trip (mid-DMA, mid-EEPROM transfer, FIFO contents) [REC].
- **Why.** Snapshot is not needed for the core value, which is fresh-instance determinism [OWNER]. Format lock-in is the larger risk (CP-9, CART-2).

### R10. Fixture vendoring default [REC] [OWNER]

- **Positions.** STACK allows pinned fetch or a committed copy, decided per fixture. PITFALLS defaults third-party fixtures to pinned fetch and local run, not vendored, until the owner clears them.
- **Resolution.** Use PITFALLS' default. Fetch by SHA-256 through `fetch-pin.sh`. Keep no third-party binaries in the tree. A committed fixture needs a manifest row, its notices, and owner clearance (G3). Owned fixtures are built from source, carry no logo bytes, and link no runtime libraries [REC].
- **Why.** A repository license does not clear embedded assets: logos, fonts, music, and prebuilt runtime code [SRC]. A pinned fetch used only as runtime input does not redistribute the file. That is an inference, and the rights question stays with the owner [INF].

### R11. PPU sequencing [REC]

- **Positions.** PITFALLS puts a first per-line renderer and visible output in milestone 2. ARCHITECTURE puts PPU timing (S5a) before PPU pixels (S5b), after DMA.
- **Resolution.** Both. Milestone 2 ends with a mode-3 pixel path behind the latch-point abstraction, after the S5a timing work. Full pixel work (modes 0 to 5, OBJ, windows, blending) is milestone 3. Contention and latch tuning (S5c) follow in milestone 3 [REC].
- **Why.** Timing must come first for the hw-test sweeps, and visible output must not wait for the full PPU.

### R12. mGBA suite lane placement [REC]

- **Positions.** STACK puts the mGBA suite in the deep lane only: built in a devkitARM Docker image pinned by digest, never committed, driven by timed A presses, and read through its SRAM log. ARCHITECTURE uses it from S3 onward. PITFALLS EV-3 warns that its expected values depend on GCC version.
- **Resolution.** The mGBA suite is a deep-lane differential fixture from phase 5 (milestone 2) onward. It needs IRQ and SWI 5 (VBlankIntrWait) from the stub, and an SRAM result reader from phase 5. Pin the suite SHA, the image digest, and the recorded package versions. Report per comparison, never as a percentage. Its `passes/totalResults` counts comparisons, up to 20 per test [IMPL]. It is not in the PR gate [REC].

### R13. Naming collisions [REC]

- STACK's BIOS labels (M0, M1, M1b, M2) collide with milestone numbering in PITFALLS. Retire them. Use the BIOS provider values in R1 everywhere.

### Where the files already agree (no reconciliation needed)

- **Timing:** per-access cost plus an event scheduler, in whole master cycles. Not per-cycle lockstep, and not instruction-counted (ARCHITECTURE Option B, PITFALLS CP-1, FEATURES scheduler).
- **Save type:** always declared by the caller. Never inferred from title or filename. The ID-string scan is advisory only (all four files).
- **HLE:** never presented as the real BIOS (FEATURES, ARCHITECTURE, PITFALLS).
- **Core shape:** integer-only; no host callbacks; caller-owned buffers; no globals (all four).
- **Output:** raw BGR555 video and raw audio, separate from each other; no resampling, color conversion, or scaling in the core (ARCHITECTURE, STACK, PITFALLS).
- **Frame constants:** 308 dots per line (240 visible plus 68 HBlank), 4 cycles per dot, 1,232 cycles per line, 228 lines, and 280,896 cycles per frame. Frame rate is 262144/4389 Hz, which is 59.7275... Hz [REF] [INF]. GBATEK's printed 59.737 Hz and 13.620 kHz are inexact; the second works out to about 13.618 kHz [INF].
- **HBlank flag edge is unresolved (U1).** GBATEK says the flag is clear for 1,006 cycles; NBA schedules at 1,007; mGBA at 1,008 [REF] [IMPL]. Never hard-code 960 as the flag edge. Calibrate against the hw-test sweep (phase 7).
- **Result taxonomy and fixture rules** (all four files).
- **NBA license wording.** GPL-3.0 on Codeberg master; ARCHITECTURE says "or later" (not confirmed by the file read elsewhere). Treat NBA as GPL-3.0 for planning and read it for behavior only [SRC].

---

## Stale or wrong bootstrap claims (consolidated)

The bootstrap pack is dated input, not specification. Each claim below was checked on 2026-11-09 against the sources the research files cite. Re-verify before any of them becomes a requirement.

| # | Bootstrap claim | Status | Correct statement (label) |
|---|-----------------|--------|---------------------------|
| 1 | NBA hw-test linked on GitHub, BSD-3 | Stale link | GitHub repo archived 2026-06-21. Development is at `codeberg.org/nba-emu/hw-test`. Pin HEAD `fbc99140e06f` (2026-08-10). BSD-3-Clause, (c) 2021 fleroviux [SRC] |
| 2 | NanoBoyAdvance linked on GitHub | Stale | Archived. Codeberg master is GPL-3.0. `phoenix` is BSD-2-Clause (relicense commit 2026-07-26, in development) [SRC] |
| 3 | GBATEK "GBA-only" fork as the reference | Stale version | The rust-console fork is derived from no$gba 2.9b. Cite GBATEK 3.06 (problemkaputt.de or the mgba-emu markdown mirror) per claim. ARCHITECTURE's citations point at the 2.9b fork and must be re-pointed [SRC] [REF] |
| 4 | Frame rate 59.737 Hz | Inexact | 262144/4389 = 59.7275... Hz. Use the rational [INF] |
| 5 | HBlank at 960 cycles | Imprecise | Visible drawing is 960 cycles. Flag-clear edge is 1,006 (GBATEK), 1,007 (NBA), 1,008 (mGBA). Unresolved (U1) [REF] [IMPL] [UNK] |
| 6 | Normmatt "open-source BIOS" | Wrong | The repo is a disassembly that needs a Nintendo `baserom.bin` to build, and it has no license file. Do not use it. Community claims that other projects embed it are unreconciled [SRC] [UNK] |
| 7 | Nintendo logo in fixtures | Rights issue | jsmolka's `lib/header.asm` embeds the 156-byte logo. The mGBA-suite build runs `gbafix -p`. devkitPro's crt0 zero-fills it. Owned fixtures omit it, and a scan enforces that [SRC] |
| 8 | "mGBA suite and jsmolka are MIT" | Incomplete | Repo LICENSEs are MIT. mGBA: `mgba.h` has a BSD-2 header, font bitmaps lack provenance, and the build needs devkitARM, libgba, and gbafix. jsmolka: prebuilt FASMARM ROMs embed the logo, `bios/bios.gba` needs a Nintendo BIOS, and `unsafe/` results are disputed on hardware [SRC] |
| 9 | "mGBA 0.10.5 is current" | Partly stale | 0.10.5 is the latest tagged release (2025-03-08 or 03-09; sources differ). Master has unreleased 0.11 with a headless frontend and scripting [SRC] |
| 10 | "SP and Micro are host-only" | Partly wrong | 0x04000800 behavior and the Micro's missing cartridge switch are machine-visible [REF] [SRC] |
| 11 | "EEPROM size is inferred from DMA length" | Wrong as hardware fact | That is an emulator technique (mGBA, at runtime). GBATEK says no autodetection exists [REF] [IMPL] |
| 12 | mGBA suite "score" | Misleading | `passes/totalResults` counts comparisons, up to 20 per test. Report per case [IMPL] |
| 13 | AGB ARM7TDMI at about 16.78 MHz | Confirmed | GBATEK and the ARM7TDMI TRM cover agree. Nintendo's UK page says only "32-bit CPU" [SRC] [REF] |
| 14 | AGB Programming Manual mirror | Unverifiable | Mirror returned HTTP 403. It is a third-party copy of a Nintendo document. Do not vendor it. Cite sections only if accessed legitimately [SRC] |
| 15 | Nintendo sales 81.51M hardware, 377.42M software | Confirmed | As of 2026-09-30, life-to-date. This is distribution history, not current demand [SRC] |
| 16 | "Siblings are MIT where present" (PROJECT.md) | Incomplete | LibFuturo is Apache-2.0 [SRC] |
| 17 | Repository visibility open (STACK gate list) | Stale | Public since 2026-11-09 [OWNER] |
| 18 | "Endrift's timing tests" as a separate suite | No separate repo found | The timing and DMA suites are inside mgba-emu/suite [INF] |
| 19 | Playstead pinned to mGBA 0.10.5 on macOS 26 arm64 | Not re-checked | Sibling planning record only [IMPL] |
| 20 | Devkit r67 and r68 identity | Unknown | Docker image `devkitpro/devkitarm:20260610` was pushed two hours before the r68 buildscripts commit. Whether it holds r67.1 or r68 is unknown. Record `dkp-pacman -Q` in each manifest [SRC] [UNK] |

---

## Implications for Roadmap

### Milestone sequence

Milestone numbers are GSD milestones, not software versions. Phase numbers are provisional. The roadmapper may merge adjacent phases, but it must keep the dependency edges listed below. Each phase names its research flag.

**Milestone 1: Evidence spine (first milestone; no visible output)**

- **Phase 1: Profile, manifest, and result contract (ARCHITECTURE S0).** Delivers the `agb` profile document (direct-start, BIOS `none`, ROM-only cartridge, unmodeled list). Delivers the fixture manifest schema and linter: source URL, pinned revision, license and notices, build recipe, toolchain digest, BIOS mode, expected-value provenance class, permitted use. Delivers the result taxonomy as data: PASS, GUEST_FAIL, GUEST_STOP, HARNESS_FAIL, TIMEOUT, CANCELLED, MISSING, SKIPPED, REFUSED. Delivers the ROM, BIOS, and logo signature scan, the `pins.txt` SHA-256 fetch, and the canonical verify command with a negative control. A missing toolchain must report SKIPPED loudly, never fall back silently [STACK, PITFALLS] [REC]. Addresses: manifest and claim tuple; distinct result states; negative controls. Avoids: CP-5, CP-6, CP-10, EV-2, CP-11. Research flag: **standard patterns**, sibling-proven in cogmaster and nesturbator.

- **Phase 2: CPU, bus API, runner (ARCHITECTURE S1).** Delivers an ARM7TDMI ARM and Thumb interpreter with an explicit two-stage pipeline and an open-bus latch. Delivers a bus API that returns cost and N/S class per access (placeholder costs allowed), so every access advances the scheduler. Delivers region decode with mirrors and width rules, deterministic power-on fill, and explicit classification of SWI, undefined, and coprocessor opcodes. Delivers an IWRAM mailbox result block, read by side-effect-free `peek`. Delivers original ARM and Thumb diagnostics built with GNU `as`, a project linker script, and a header fixer, with no logo bytes and no linked runtime. Delivers the xPack toolchain spike (ARMv4T output, division resolved from libgcc, identical bytes on macOS and Linux). Delivers a CLI runner that emits JSON, and a relocation smoke test for the installed CLI. Addresses: the ARM7TDMI core; the deterministic headless runner; the original diagnostic. Avoids: CP-1, CP-2, CP-7, CP-8, CPU-1 to CPU-9, BUS-6, and the start of the CP-3 post-boot state table. Research flag: **read the ARM7TDMI TRM and GBATEK 3.06 CPU chapters in full**, with ARMv4T version tags. Otherwise standard.

- **Phase 3: Determinism gates and first third-party CPU fixtures.** Delivers jsmolka `arm`, `thumb`, and `memory`, from pinned fetch, after gates G2 and G3. Delivers ASan and UBSan builds, -O0 against -O2 trace equality, two-compiler trace equality, fresh-process and in-process repeat runs, and a two-instance interleave test. Delivers refused-load postconditions (state and buffers byte-identical). Delivers a trace-hash gate that fails if a handler advances time without the bus API, and a test-inventory check via CTest and junit. Exit criteria: the verify command is green, every negative control fails for a known reason, and skipped BIOS fixtures are visible. Addresses: the determinism and evidence constraints. Avoids: CP-7, CP-8, CP-5 (provenance enforced), EV-4. Research flag: **standard patterns**.

**Milestone 2: Time, interrupts, DMA, first pixels (first visible output)**

- **Phase 4: Scheduler, IRQ, timers, HALT, keypad, BIOS stub (ARCHITECTURE S2).** Delivers a `u64` master-clock scheduler with plain-data events ordered by (time, priority, sequence). Delivers an IRQ controller sampled at instruction boundaries; four timers with lazy reads and scheduled overflow; HALTCNT wake at the exact edge; keypad input applied at timestamped cycles. Delivers the `project-stub` BIOS image: vector table, IRQ entry at 0x18, and an SWI dispatch with a documented subset. Delivers the `none` classification for IRQ or SWI without a provider. Gate: owner approval of stub authoring (G4) comes first. Addresses: interrupts, timers, keypad, BIOS handling. Avoids: CP-3, IRQ-1, IRQ-2, BUS-8 (HALT or STOP deadlock, reported as GUEST_STOP), PROC-1 (no JIT). Research flag: **moderate**. The SWI subset and interrupt latency need research. Use hw-test `timer/*` and `irq/irq-delay` as oracles, with provenance recorded.

- **Phase 5: Bus timing, prefetch, SRAM, mGBA suite (differential).** Delivers a WAITCNT-driven wait table, N/S classification, forced non-sequential access at each 128 KiB boundary, and Game Pak prefetch as state (or declared unmodeled per fixture). Delivers CPU open bus, a BIOS read-protection latch, and a 32 KiB SRAM device with a declared type, which is also the result channel. Delivers the mGBA suite memory, io-read, and non-BIOS timing entries as a deep-lane differential, with an SRAM-log result adapter (R12). Adds hw-test `bus/128kb-boundary` and `haltcnt` as differential checks. Addresses: wait states, prefetch, open bus. Avoids: CP-2, BUS-1 to BUS-4, BUS-6, CART-1, CART-8. Research flag: **HIGH**. U2 (prefetch across WAITCNT settings), BIOS read protection, and GBATEK contradictions. Also spike hw-test console-text reading (U9) and the mGBA suite headless harness.

- **Phase 6: DMA as bus master (ARCHITECTURE S4).** Delivers four channels with priority, CPU pause, start delay, and unit timing (2N + 2(n-1)S + xI for Game Pak). Delivers per-channel latches under the dispute registry (PITFALLS BUS-5). Delivers count and address masks tested at their limits (count 0 means 0x4000 or 0x10000; addresses masked to 27 or 28 bits). Immediate DMA comes first. HBlank, VBlank, and special start follow in phases 7 and 10. DMA events never call the bus. Addresses: DMA, arbitration. Avoids: IRQ-3, IRQ-4, BUS-5, and DMA integer overflow (security). Research flag: **HIGH**. U3 (arbitration with multi-cycle instructions), the per-channel latch dispute, and the DMA3 SRAM-to-ROM hazard reported in the mGBA forum errata (secondary report [ISSUE]).

- **Phase 7: PPU timing and first visible output, playable 0 (ARCHITECTURE S5a plus the mode-3 slice of S5b).** Delivers line and dot events, VCOUNT, DISPSTAT flags, VBlank, HBlank, and VCOUNT IRQs, and HBlank, VBlank, and video-capture DMA triggers. Delivers a mode-3 per-line renderer producing raw BGR555 frames, behind the latch-point table. Delivers keypad input at explicit boundaries. Delivers playable 0: an original mode-3 game using polling VCOUNT and KEYINPUT, with no BIOS, audio, or save, plus an IRQ-driven variant using the stub. Records a raw digest and semantic assertions for each. Gate: owner judges playability (G6, human gate). Addresses: visual output and the first playable. Avoids: PPU-1, CP-4 (latch table exists), CP-10. Research flag: **HIGH** for timing alignment (U1). Calibrate against hw-test `ppu/status-irq-dma`, record the result as harness-relative, and put the edges in the profile.

**Milestone 3: PPU completion**

- **Phase 8: PPU pixels complete (ARCHITECTURE S5b).** Delivers modes 0 to 5; OBJ regular and affine with a per-line budget (1,210 cycles, or 954 with H-blank interval free); windows; blending; mosaic; forced blank; green-swap. Output stays raw BGR555. Addresses: display modes, sprites, windows, blending, mosaic. Avoids: PPU-3 to PPU-7. Research flag: **MEDIUM**. Window wrap rules come from tests only (PPU-5). The OBJ budget's treatment of vertically offscreen sprites is an open question in GBATEK. Blending errata (PPU-6) need the tests.

- **Phase 9: PPU contention and latch timing (ARCHITECTURE S5c).** Delivers the CPU VRAM, PRAM, and OAM contention schedule (+1 cycle when the PPU is reading). Delivers the latch table for DISPCNT, BG2X and BG3X, and BG PD registers. Delivers mode-dependent VRAM mirror rules. Adds the project's own raster fixtures (HBlank DMA into BG2X, mid-line DISPCNT write) with stated provenance. Adds hw-test `ram-access-timing`, `dispcnt-latch`, `bgx`, `bgpd`, `vram-mirror`, `sprite-hmosaic` as differential checks with provenance. Research flag: **HIGHEST** risk in the project. GBATEK does not define the latch points, and the hw-test expected images have unstated provenance [REF] [UNK].

**Milestone 4: Audio**

- **Phase 10: Audio (ARCHITECTURE S6).** Delivers PSG channels 1 to 4 using the GBA register map and wave banks, not the GB APU. Delivers FIFO A and B with timer pops, FIFO DMA special requests (via phase 6), SOUNDBIAS, the mixer and clip, and the fixed 262,144 Hz u16 model stream (R3). Delivers an exact-sample-vector diagnostic and a separate audio digest. Playable 1 can land here or in milestone 5. Addresses: PSG, Direct Sound, SOUNDBIAS. Avoids: AUD-1 to AUD-5 (especially AUD-4, reusing GB APU code). Research flag: **MEDIUM-HIGH**. U4 (output reduction), FIFO underrun and overrun behavior (GBATEK is silent), and GB-versus-GBA PSG differences.

**Milestone 5: Cartridge and saves (can run in parallel with milestones 3 and 4 once phase 6 lands)**

- **Phase 11: Cartridge and backup devices (ARCHITECTURE S7).** Delivers header diagnostics and ROM length rules with distinct refusals (zero, below header, above 32 MiB, odd length). The ROM ceiling is 32 MiB, less 256 bytes when EEPROM sits at the top of the window [REF]. Delivers a Flash 64K and 128K command state machine with a declared chip ID per size (Panasonic 1B32 and Sanyo 1362, as mGBA uses) and bank switching. Delivers EEPROM 512 B and 8 KiB via DMA3 with declared width. Delivers FRAM as labeled SRAM-class. Delivers media copy and import, generation counters, an ID-string advisory scanner that reports only, and length sweeps with refusal checks. Addresses: declared saves, export and import, refusals. Avoids: CART-1 to CART-7, CART-2 (no size inference), CP-6 (untrusted input). Research flag: **MEDIUM**. EEPROM width and DMA3 length protocol. jsmolka issue #14 reports timing sensitivity on flash carts [ISSUE].

**Milestone 6: API, consumer, packaging**

- **Phase 12: Public API boundary (ARCHITECTURE S8a).** Write the lifecycle and refusal document before the first export. Delivers an opaque instance; bounded advance by cycles with stop reasons; timestamped input; caller-owned buffers; media copy and import; state-ready data layout with no format promise; `peek`; optional allocator injection. Adds a symbol scan for writable file-scope data, the interleave test at scale, and fuzzing of loaders (the libFuturo lane runs on schedule). Avoids: CP-8, CP-9 (no format), PROC-2. Research flag: **standard patterns**.

- **Phase 13: Installed relocatable C consumer and cross-platform determinism (ARCHITECTURE S8b).** Delivers a CMake package config (relocatable, with a prefix derived from the file's location), C and C++ consumers, a Linux x86_64 byte-identical cross-check on schedule first and then in the PR gate once the core has substance, and the be32 hash-equality lane on schedule. Research flag: **standard patterns**, sibling-proven.

- **Phase 14 (optional, parallel-eligible after phase 5): user-supplied official BIOS mode (ARCHITECTURE S9).** Local only. Records the digest. Public CI reports SKIPPED. Enables jsmolka `bios/` and the mGBA BIOS-math entries. Gate G7 (private use). Research flag: low.

- **Phase 15 (deferred to v1.x or later; not required for the core value):** snapshot and restore format, with the in-flight round-trip entry gate (R9); LibFuturo adapter proof (needs a real core and consumer first, plus 16-bit video, a 262,144 Hz rate accepted by hosts, and a second content slot for BIOS; U8); Cult-of-GBA replacement fixture (R7), when a named fixture needs it; GDB stub; user save states; rewind. Research flag: **MEDIUM** for snapshot; the LibFuturo adapter needs a fresh read of its contract at that time.

### Where the playable lands

- None in milestone 1.
- Playable 0 at the end of milestone 2 (phase 7): original, in-tree, mode-3, polling, no BIOS, no audio, no save. Owner judges playability (G6).
- Playable 1 at the end of milestone 4 or 5: interrupt-driven through the stub, with sound and SRAM.
- gba-pong only as conditional third-party corroboration, after its build and attribution check [SRC].

### Phase ordering rationale

- **Timing API before scheduler before bus timing before DMA before PPU timing before pixels before contention.** Each later phase measures with the timers and bus costs that the earlier one defines. Every timing test measures against timers, so timers and the scheduler come before bus timing [REC].
- **The stub (phase 4) before the mGBA suite, any IRQ-using fixture, and playable 1.** The mGBA suite calls VBlankIntrWait and irqInit, so the stub must provide SWI 5 and IRQ dispatch first [SRC].
- **DMA (phase 6) before FIFO DMA (phase 10) and EEPROM (phase 11).** Both depend on DMA timing and bus ownership [REC].
- **PPU timing before pixels.** Blanking DMA and the hw-test sweeps need only timing. Many games' sync loops need only VCOUNT [IMPL] [REC].
- **Milestone 1 contains the decisions that are expensive to reverse** (timing API, pipeline state, determinism, provenance). It needs no owner rights decision beyond the fixture policy [REC].

### Research flags

**Needs deeper research before planning (run the research step first):**

- **Phase 9, PPU latch and contention (highest risk).** Latch points per register; contention cycle positions; hw-test provenance of `expected.png` [UNK].
- **Phase 5, bus and prefetch.** Prefetch state across WAITCNT settings, the stop penalty, BIOS read protection, and the open-bus latch (U2) [UNK].
- **Phase 6, DMA.** Arbitration with multi-cycle instructions (U3), the per-channel latch dispute (BUS-5), DMA3 SRAM-to-ROM behavior (IRQ-4), and start-delay constants [UNK].
- **Phase 7, PPU timing.** HBlank flag alignment (U1). Calibrate against hw-test `ppu/status-irq-dma` [UNK].
- **Phase 10, audio.** U4 (PSG reduction to the output rate), FIFO underrun and overrun, and GBA-versus-GB PSG register deltas [UNK].
- **Phase 4, stub.** The SWI subset and the post-boot state table S (U7). Requires owner approval first (G4).
- **Phase 2, CPU.** Full read of the TRM and GBATEK 3.06 CPU chapters with version tags. The CPU quirk list (CPU-1 to CPU-9) needs test pins.
- **Phase 11, cartridge.** EEPROM width and DMA3 length protocol; Flash behavior on hardware.

**Spikes (not research, but gating):**

- xPack toolchain identity: ARMv4T, libgcc division, identical bytes on macOS and Linux. Exit criterion of phase 2. If it fails, switch to Ubuntu apt binutils for CI and move digest ownership to Linux [SRC] [UNK].
- hw-test console text reading from VRAM (U9). Phase 5.
- mGBA suite headless harness (SRAM log, A-press timing, which suites are meaningful under the stub). Phase 5.

**Standard patterns (skip the research step):** phase 1 (manifest, linter, verify command); phase 3 (sanitizers, two-compiler trace equality, two-instance interleave); phase 12 (C API lifecycle and refusal patterns); phase 13 (CMake package, installed consumer). These are sibling-proven in cogmaster, nesturbator, and gabbaboy [IMPL].

**Partly standard:** phase 8. Modes 0 to 5 basic rendering is standard. Window wrap, OBJ budget, and blending edge rules are not. Research those edge rules only.

---

## Owner gates

Gates are the items the owner must decide or perform. Each has a recommended default and names what it blocks. Nothing else in this summary is a gate.

| ID | Gate | Recommended default | Blocks | Next action |
|----|------|---------------------|--------|-------------|
| G1 | Project license | MIT (MEDIUM confidence) [REC] | Any external reuse or packaging. Code reading from GPL or MPL references beyond behavior. Until chosen, default copyright applies [OWNER] | Surface now. Decide before phase 13 packaging and before any external contributor |
| G2 | Nintendo logo bytes in any committed or published fixture | No. Owned fixtures omit the logo; the scan enforces it | Any fixture that must boot a logo-checking BIOS (emibios header build; hardware). Any "boots on a real BIOS" claim | Decide before any fixture with logo bytes is considered |
| G3 | Third-party fixture vendoring | Pinned fetch; no third-party binaries in tree | Committing jsmolka, mGBA suite, hw-test, or SingleStepTests subsets | Decide before any third-party file is committed |
| G4 | BIOS stub authoring approval, clean-room posture (author from documented behavior, not transcription of Nintendo listings) | Approve | Phase 4 | Owner approval before phase 4 begins |
| G5 | Replacement BIOS use (emibios, Cult-of-GBA) | Pinned differential only for emibios; Cult-of-GBA as a v1.x fixture. Owner alone decides any "clean" or "cleared" claim | Any replacement-BIOS fixture or claim | Decide before the first replacement fixture |
| G6 | Playable asset authorship and policy | Original, in-tree, all assets authored from scratch, license per G1. Third-party title only after per-asset review. Playability is a human judgment | Playable 0 acceptance (phase 7) and playable 1 | Confirm authorship approach before phase 7 |
| G7 | Proprietary and private oracles: AGS Aging cartridge, owner's official BIOS dump, SingleStepTests (NBA-derived data; the legal question is unresolved [UNK]) | Owner decides whether private use is allowed. Never in repo, CI, artifacts, or screenshots. SingleStepTests in the deep lane only, labeled differential | Any AGS-based claim; SingleStepTests; post-boot state checks (U7); phase 14 | Owner decision before any of these is used |
| G8 | devkitPro redistribution terms (unresolved; the site returned HTTP 403), and the r67 versus r68 image question | Local use or pulls by digest only. Do not mirror or cache | The devkitARM deep lane in CI; any CI artifact caching the image | Resolve before any cache or mirror |
| G9 | Differentiator wording | Working wording (R4) stands | Public positioning only. Not blocking engineering | Owner confirms when convenient |
| G10 | First outcome direction: Playstead integration, or a visible demo within weeks? | No. Spine first | Milestone shape (thin-vertical alternative in R2) | Decide before the roadmap is locked |
| G11 | Publication: release artifacts, announcements | Owner gate. No release is claimed | Milestone 6 release | Decide at milestone close |
| G12 | Physical hardware: captures with recorded cart type; AGB versus Micro claims; subjective playability; physical controller | Human gate | Any hardware-accuracy claim; `agb-micro` | When a claim needs it |
| G13 | Playstead integration | Not assumed | Any integration work | Separate decision |

---

## Confidence Assessment

| Area | Confidence | Basis |
|------|------------|-------|
| Stack | MEDIUM-HIGH | Sibling conventions, upstream licenses, and fixture digests were read directly [IMPL] [SRC]. Unverified: xPack output identity across hosts (spike). Legal conclusions are inference and owner gates |
| Features | MEDIUM-HIGH | Hardware facts from GBATEK, a community reference [REF]. Differentiator ranking is inference (MEDIUM). SP equivalence is per GBATEK only |
| Architecture | MEDIUM | Frame geometry and test-measured quantities are HIGH. The timing model is reasoned from two emulators' source and test sources, not prototyped. U1, U2, and U4 are LOW |
| Pitfalls | MEDIUM-HIGH for the adversarial check and fixture findings; MEDIUM for the subsystem catalog | Many catalog rows are untested background [UNK] until a test pins them |
| Overall | MEDIUM | Strong on process, language, and rights. Weakest on the timing constants and PPU latch points that accuracy claims depend on |

---

## Gaps to Address

Each gap names how to handle it during planning or execution.

- **U1, HBlank flag alignment.** Sources disagree (1,006, 1,007, 1,008). Calibrate against hw-test `ppu/status-irq-dma` in phase 7. Keep the result harness-relative and record it in the profile.
- **U2, prefetch across WAITCNT.** Research in phase 5. Until then, set `prefetch: unmodeled` in the profile and tag affected fixtures `requires_prefetch`.
- **U3, DMA arbitration with multi-cycle instructions.** Research in phase 6. A documented approximation is acceptable if stated as one.
- **U4, PSG output reduction.** Audio claims stay "model" until researched in phase 10. No test in this research targets it.
- **U7, post-boot state S.** Define it from documented behavior in phase 4. Verify against a privately held official BIOS only if G7 allows.
- **DMA latch dispute** (per-channel versus shared; GBATEK silent). Record in the dispute registry. Choose a profile default only with a hardware-derived source.
- **r12 result channel** for jsmolka. Verify in phase 2 (R6).
- **hw-test `expected.png` provenance and console text reading (U9).** Prototype in phase 5. Treat `expected.png` as unknown provenance until the author states otherwise.
- **xPack toolchain identity.** Spike at phase 2. Fallback is Ubuntu apt binutils, with digest ownership moved to Linux.
- **mGBA suite headless harness.** Phase 5.
- **LibFuturo fit.** Gaps: no 16-bit video format; no 262,144 Hz rate confirmed for hosts; no second content slot for BIOS (U8). Only at the phase 15 adapter proof, after the real core and consumer exist. Recheck the contract then.
- **devkitPro terms and r67 versus r68.** Record `dkp-pacman -Q` in each manifest. Gate G8.
- **gba-pong buildability.** Unknown. Conditional only; not planned.
- **mGBA 0.11 release timing.** Re-check at each roadmap review. The differentiator wording depends on it (R4).
- **Replacement-BIOS accuracy deltas** (Cult versus emibios versus Nintendo). Matter only once a claim depends on them.
- **CI runner image names** (`ubuntu-26.04`, `macos-15`). Not verified against GitHub's current image list.
- **SP and AGS model naming.** Settle in the profile document (phase 1), using the R8 position.
- **Dispute registry for reference disagreements.** Seed it with the DMA latch, the GBATEK errata (mGBA forum list: sprite semi-transparency versus brighten/darken, WIN wrapping, DMA3 SRAM-to-ROM lockup, JOY direction, PWM rumble [ISSUE]), SRAM mirror behavior (CART-8), and the flash-card hardware results (CART-7).

---

## Sources

Sources are grouped by evidence label. The four research files give full detail and per-claim URLs.

**[SRC] Primary sources read directly, 2026-11-09:**
- mGBA: suite repository (LICENSE, Makefile, `docker-build.sh`, `src/main.c`, `src/mgba.c`, `include/mgba.h`, commit log, issues #8, #13, #17, #19 to #27); mGBA 0.10.5 and master source (`src/gba/dma.c`, `savedata.c`, `overrides.c`, `gba.c`, `bios.c`, `hle-bios.*`, `io.c`, `video.h`, `headless-main.c`); CHANGES on master; releases and downloads at mgba.io.
- jsmolka gba-tests (LICENSE, README, `lib/header.asm`, `lib/macros.inc`, `bios/bios.asm`, `unsafe/README.md`, `nes/nes.asm`, issues #2, #8, #12, #14).
- NanoBoyAdvance hw-test on Codeberg (`codeberg.org/nba-emu/hw-test`, HEAD `fbc99140e06f`), and the archived GitHub notice. NanoBoyAdvance master LICENSE and source (`scheduler.hh`, `bus/timing.cc`, `ppu.cc`) on Codeberg.
- SingleStepTests/arm7tdmi README.
- Cult-of-GBA/BIOS (LICENSE, README, `make.sh`, `entrypoint.s`).
- coolbho3k/emibios (README, LICENSE, `build.zig.zon`, release assets).
- Normmatt/gba_bios README.
- devkitPro repositories (libgba, devkitarm-crtls, buildscripts, docker, gba-tools) and the Docker Hub API for `devkitpro/devkitarm`.
- xPack arm-none-eabi GCC releases; Ubuntu package index for `gcc-arm-none-eabi` per release.
- ARM7TDMI (Rev 3) TRM, ARM DDI 0029G (cover and release table).
- Nintendo UK GBA technical data page; Nintendo IR lifetime sales, as of 2026-09-30.
- CMake cmake-presets(7) schema table.
- SkyEmu README and HTTP control server document; ares v144 release notes; BizHawk.
- Test ROM candidates: AnthonyCalandra/gba-pong; fritzem/Google-Dino-GBA; jtsiomb/gbatris; GValiente/butano; pinobatch/240p-test-mini; DenSinH/FuzzARM; destoer/armwrestler-gba-fixed; ladystarbreeze/GBA-Test-Collection; felixjones/gba-toolchain; rust-console/gba; agbrs/agb; velipso/gvasm; Kingcom/armips. Used for license and asset review only. No ROM or BIOS binary was downloaded.
- gbadev-org Homebrew Hub catalogue (251 `game.json` files read; no ROMs).

**[REF] Community hardware references (not Nintendo-issued; not verified against hardware):**
- GBATEK 3.06 (problemkaputt.de; mgba-emu markdown mirror). Errata noted by mGBA forum thread tid=343 [ISSUE].
- GBATEK 2.9b-derived fork (rust-console `gbatek-gbaonly`): cited by ARCHITECTURE; to be re-pointed to 3.06 (see stale claim 3).
- NanoBoyAdvance hardware docs (`nba-emu.github.io/hw-docs`).
- Tonc and gbadoc (links resolve; content not audited).
- Community mGBA debug-register references in the agb and gba crate docs; mGBA `io.c` confirms the 0xC0DE and 0x1DEA handshake [IMPL].

**[IMPL] Sibling repos and local observations:**
- Read-only: `~/projects/gabbaboy`, `~/projects/nesturbator`, `~/projects/cogmaster`, `~/projects/libfuturo` (CMakeLists, presets, workflows, `pins.txt`, `fetch-pin.sh`, `ASSET_POLICY.md`, `PROVENANCE.md`, `tests/check.h`, `runner/png.c`, `libfuturo.h`, `docs/contract.md`, `docs/muhtari-profile.md`).
- Playstead planning records (`~/projects/playstead/.planning/`), read only. Source of the 0.10.5 blocker claim, which is [IMPL] for Playstead's pinned version.
- Local probes: Homebrew `arm-none-eabi-gcc` 16.2.0 multilib and `readelf -A`; `clang 21 --target=armv4t-none-eabi` compile; `llvm-nm` division symbol. True of that machine only.

**Not reached (unknown or unverified):**
- AGB Programming Manual mirror (HTTP 403). Emulation General Wiki GBA tests page (HTTP 403). gametechwiki (Cloudflare challenge). devkitpro.org (HTTP 403). NanoBoyAdvance `docs/ACCURACY.md` (not located).

---
*Research completed: 2026-11-09*
*Ready for roadmap: yes, with the owner gates above visible in the roadmap*