# Pitfalls Research

**Domain:** Deterministic, accuracy-focused Game Boy Advance emulator (greenfield; portable core + headless runner + thin hosts)
**Researched:** 2026-11-09
**Confidence:** MEDIUM-HIGH for the adversarial claim check, fixture/test-suite findings and process pitfalls (primary sources and repo contents read this session). MEDIUM for the per-subsystem edge-case catalog (several items are background knowledge that must be proven by a test before they become requirements; each is tagged).

## How to read this file

Evidence tags (every non-obvious claim carries one):

| Tag | Meaning |
|-----|---------|
| `[PRIMARY]` | Official vendor document read this session (Arm TRM cover/revision, Nintendo pages). |
| `[GBATEK]` | GBATEK 3.06 (problemkaputt.de, downloaded and grepped) or its markdown forks. Community reference by the no$gba author, not Nintendo-issued. |
| `[TEST-SRC]` | Source or file tree of a public test suite, read this session (mGBA suite, jsmolka gba-tests, NanoBoyAdvance hw-test). |
| `[EMU-SRC]` | Emulator source read this session (mGBA 0.10.5 tag). Shows what that emulator does, not what hardware does. |
| `[ISSUE]` | Issue tracker or forum thread read this session. |
| `[ARITH]` | Computed in this session. |
| `[KNOWLEDGE]` | Background knowledge not re-verified this session. A hypothesis: write a test or find a source before turning it into a requirement. |
| `[INFERENCE]` | Recommendation or reasoning by the researcher. |

Phase tags map to the roadmap vocabulary requested: **CPU**, **bus+timing**, **IRQ-timer-DMA**, **PPU**, **audio**, **cartridge**, **API+consumer**, **verification-infra**.

## Headline findings (read these first)

1. **No public GBA test suite is a hardware oracle.** The mGBA suite has open issues saying expected values are wrong or fail on real hardware (#22, #24, #27), jsmolka's own `unsafe/` README says two GBATEK statements pass on mGBA but fail on hardware, and mGBA 0.10.5 and NanoBoyAdvance's hw-test disagree on how many DMA latches exist. Every expected value needs a provenance class in the fixture manifest. (CP-5)
2. **GBATEK is necessary but not sufficient.** Its CPU chapter is shared across ARMv4/v5/v6 (ARM7TDMI is ARMv4T), it has known errata, internal contradictions about DMA and Game Pak access, a wrong frame rate (59.737 Hz printed, 59.7275 Hz actual), and it does not mention the DMA latch at all. The seed's GBA-only link is a fork of an older version (2.9b vs 3.06). (Claim table)
3. **Every ROM that boots on a real BIOS carries Nintendo's 156-byte logo.** jsmolka's MIT test ROM source embeds it in `lib/header.asm`. A repo licence does not clear that content; the project's own diagnostic must decide this on purpose. (CP-6)
4. **The popular suites report results in ways a headless runner cannot consume as-is.** jsmolka and mGBA draw to the screen; mGBA also uses an mGBA-specific debug log interface; NBA hw-test prints text through a devkitPro console. "Machine-readable semantic results" has to be built by Gamebloke, per suite or per own fixture. (CP-5, EV-2)
5. **The recommended first slice is too wide for one milestone.** It chains five independent critical paths and two owner gates. A spine-first shape is safer. (First-milestone critique)

---

## Critical Pitfalls

### CP-1: Timing model too coarse, or chosen late

**What goes wrong:** The core counts one cycle (or a fixed cost) per instruction, or runs "N cycles per scanline", and bolts waitstates on later. Everything downstream (prefetch, DMA stalls, timer reads, IRQ latency, PPU sampling points, audio FIFO cadence) then has to be retrofitted into code that has no per-bus-access cost.
**Why it happens:** Instruction interpreters are easiest to write against an instruction counter. Opcode test ROMs pass without precise timing, which hides the debt.
**Consequences:** Rewrite of the CPU/bus boundary. Games that poll timers or rely on HBlank DMA timing misbehave while every opcode test passes. `[INFERENCE]` supported by `[GBATEK]`: access cost is region-, width- and sequence-dependent (EWRAM 3/3/6 cycles for 8/16/32-bit, Game Pak ROM 5/5/8 at default waits, N vs S, a 32-bit ROM access is two 16-bit accesses, +1 cycle when the PPU is using video memory).
**How to avoid:** Decide in the first CPU phase that every bus access returns a cycle cost and sequential/non-sequential state, and that the CPU, DMA and prefetch all advance one master clock (16,777,216 Hz, `[GBATEK]`). Choose scheduler granularity (event-driven scheduler on the master clock, not a per-cycle loop everywhere) and record in the profile what is *not* modeled (for example VRAM contention). A first version may set waitstates to a documented constant table, but the interface must exist.
**Warning signs:** `cycles += 1` in the instruction handlers; frame length expressed as an instruction count; no function that answers "how many cycles did this access cost and was it sequential".
**Automated check:** A table-driven unit test of access cost per region/width/sequence derived from the GBATEK table; a trace-hash gate that fails if any instruction handler advances the clock without going through the bus API (grep/lint in the canonical verify command).
**Phase:** CPU and bus+timing (decision), IRQ-timer-DMA (consumer of the clock).

### CP-2: Pipeline and prefetch state treated as an optimization detail

**What goes wrong:** Decoded-instruction caches, fused Thumb BL, "fast ROM" paths and later JIT remove state the guest can observe: the 2-stage fetch/decode distance (stores to the next instruction's address are not seen), open-bus values derived from the last prefetched opcode, and Game Pak prefetch buffer contents.
**Why it happens:** Performance instincts. ARM7TDMI looks simple.
**Consequences:** Self-modifying code and code run from VRAM/IWRAM misbehaves; open-bus reads return wrong values; ROM timing wrong; the structure cannot be recovered without rewriting the fetch path. `[TEST-SRC]` jsmolka's `nes/nes.asm` copies code into VRAM and tests the "two stage pipeline"; issue #8 added a pipeline test for bad data-processing opcodes. `[GBATEK]` open bus returns "the recently pre-fetched opcode" and the Thumb case depends on memory region and alignment (IWRAM has stateful OldLO/OldHI behavior).
**How to avoid:** Keep an explicit fetch pipeline model (two prefetched opcodes plus the open-bus latch) in the interpreter from day one. A decode cache is allowed only if invalidation is proven against the pipeline tests. Model the Game Pak prefetch buffer (up to eight 16-bit values per `[GBATEK]`) as explicit state, or declare it unmodeled in the profile and have it fail visibly, not silently.
**Warning signs:** `fetch()` that reads memory at PC directly; a Thumb BL implemented as one 32-bit step; PC arithmetic done at decode time.
**Automated check:** jsmolka `nes.gba` (pipeline) plus own self-modifying-code fixtures (write to PC+4 and PC+8 and observe which executes); an assertion that two fetch implementations (cached vs plain) produce identical traces on all fixtures.
**Phase:** CPU, bus+timing.

### CP-3: BIOS policy decided by accident

**What goes wrong:** The team ships HLE SWIs and a hand-made "post-BIOS" register state, then discovers that behavior depends on BIOS-resident state: the last-fetched-BIOS-opcode read value, IWRAM/stack defaults, IRQ trampoline, IntrWait flags, and values left in I/O registers.
**Why it happens:** HLE looks equivalent for the SWIs games call. The observable BIOS-read protection and post-boot state are easy to forget.
**Consequences:** A whole fixture class (`bios.gba`-style tests) cannot pass without real BIOS bytes that cannot be in public CI; HLE passes some games and mysteriously fails others; uninitialized-RAM reads change behavior between runs.
**Evidence:** `[GBATEK]` BIOS reads outside BIOS return the most recent successfully fetched BIOS opcode (value at 00DCh+8 after startup/SoftReset, 0134h+8 during IRQ, 013Ch+8 after IRQ, 0188h+8 after SWI). `[TEST-SRC]` jsmolka `bios/bios.asm` asserts the opcode words 0xE129F000 (startup) and 0xE3A02004 (after SWI), so a no-BIOS core cannot pass it. `[TEST-SRC]` Cult-of-GBA replacement BIOS (MIT, last push 2025-04-06) states "high accuracy is a non-goal" and lists unimplemented sound functions and BIOS-read handling in its TODO. NanoBoyAdvance's README calls an unofficial BIOS "less accurate".
**How to avoid:** The profile names the **BIOS mode** as a first-class value: `none/direct-boot`, `hle`, `user-supplied-official`, `replacement-bios-<name>@<rev>`. Each mode has a written post-boot state table (registers, SPs per mode, IWRAM clearing, I/O defaults, WAITCNT, DISPCNT, SOUNDBIAS) and an explicit "last BIOS fetch" state machine. Fixtures declare `requires_bios`. No claim applies across modes. Reproducing even four Nintendo BIOS opcode words as constants is a small rights question: flag it as an owner decision.
**Warning signs:** "boot" code that sets PC=0x08000000 and nothing else; the same pass/fail table used for HLE and real BIOS runs.
**Automated check:** Run every fixture in each supported BIOS mode and diff against `expected_by_mode` in the manifest; a gate that fails if a fixture marked `requires_bios` is reported as `pass` in a mode that cannot satisfy it (it must be `skipped-bios-unavailable`).
**Phase:** CPU (boot state), bus+timing (BIOS read protection), verification-infra (manifest field).

### CP-4: Renderer that samples registers once per frame or per line without a latch model

**What goes wrong:** The PPU renders a full frame (or a line) from the register state at one instant. Games use HBlank DMA and mid-line writes (gradients, per-line scroll, window shapes, per-line BG2X/Y affine tricks, DISPCNT toggles), and the hardware latches different registers at different points.
**Evidence:** `[TEST-SRC]` NBA hw-test has dedicated ROMs for when BG2X/BG3X are latched (`ppu/bgx`), when BG2PD/BG3PD are latched (`ppu/bgpd`), per-scanline DISPCNT enable/forced-blank effects (`ppu/dispcnt-latch`), DISPSTAT/VCOUNT/IRQ/DMA edge timing (`ppu/status-irq-dma`) and horizontal sprite mosaic (`ppu/sprite-hmosaic`). `[GBATEK]` the internal reference-point registers are loaded at VBlank and also reloaded immediately when software writes the reference point outside VBlank, then incremented per line. `[TEST-SRC]` the expected output of those ROMs is a committed image whose capture method is not stated.
**How to avoid:** Design the PPU as a per-scanline renderer with an explicit table: *register -> latch point (cycle within the 1232-cycle line, or VBlank)*. Register writes carry a timestamp from the scheduler. Start with documented approximations and mark each unmodeled latch as such in the profile. Do not fix latch points from GBATEK prose alone; cite a hardware-derived test.
**Warning signs:** Renderer called once at VBlank; no timestamp on I/O writes; HBlank DMA to PPU registers "works" because writes apply instantly.
**Automated check:** Own raster-effect ROM (HBlank DMA into BG2X, a mid-line DISPCNT write) whose expected output is stated in the manifest with its provenance; hw-test PPU ROMs run as differential signal only (see CP-5).
**Phase:** PPU, with IRQ-timer-DMA for HBlank DMA timing.

### CP-5: Trusting emulator agreement or test-suite expected values as hardware truth

**What goes wrong:** "mGBA and NBA both pass" is recorded as accuracy. Test ROMs are assumed to encode hardware behavior.
**Evidence:**
- `[ISSUE]` mGBA suite #27 (open, 2026-08-15): "wrong misc hblank expected values, Layer Toggle 2 Actual is wrong on hardware, Window offscreen reset is broken on Actual and Expected". #24 (open): "most of the hblank timing tests fail on hardware (GBP with dev cart)". #22 (open): Memory tests "1514/1552" on a GBA SP with a flash cart. Closed #21/#19/#23 show expected values corrected to match hardware after the fact.
- `[TEST-SRC]` jsmolka `unsafe/README.md`: the GBATEK SRAM-mirror statement "passes on No$GBA and mGBA but not on real hardware"; unused-ROM-read statement "passes on mGBA but not on No$GBA or real hardware" (the author attributes some failures to the flash card).
- `[EMU-SRC]` + `[TEST-SRC]` mGBA 0.10.5 `src/gba/dma.c` keeps one `memory->dmaTransferRegister` shared by all channels, while NBA hw-test `dma/latch` asserts "each DMA channel has a separate latch" and that a 16-bit DMA duplicates the value into both halves. Two reference emulators disagree on a basic structure. The hw-test comment does not state its hardware provenance.
- `[ISSUE]` mGBA forum thread (secondary report): at least one jsmolka flash test failed on real hardware; classic ARMWrestler LDM-writeback tests reportedly fail on hardware. Treat as LOW confidence leads.
**How to avoid:** Add `expected_provenance` to every manifest entry with a closed enum: `hardware-capture(device, cart type, date)`, `vendor-doc`, `community-doc(GBATEK section)`, `test-author-asserted`, `emulator-derived(name, version)`, `self-derived`. Rule: only the first two may support a "hardware-accurate" claim. Keep a **dispute registry** (BUS-5 DMA latch is the first entry) listing behaviors where references disagree, with the profile's chosen behavior and the evidence needed to change it. Hardware measurements, when the owner provides them, record cart type because flash carts change ROM timing and mirroring.
**Warning signs:** README says "passes mGBA suite"; expected files copied from another emulator's output; no field recording where an expected value came from.
**Automated check:** Manifest linter fails if `expected_provenance` is missing, or if a result is labeled `hardware-accurate` while provenance is not in the first two classes.
**Phase:** verification-infra (manifest), then every subsystem phase.

### CP-6: Fixture rights traps (repo licence is not fixture clearance)

**What goes wrong:** A suite's MIT/BSD licence is treated as clearance for every byte of its binaries.
**Evidence:**
- `[TEST-SRC]` jsmolka `lib/header.asm` embeds the 156-byte Nintendo logo (starts `24 FF AE 51 69 9A ...`). The repo licence is MIT, copyright Julian Smolka; that does not cover Nintendo's logo. Every ROM intended to pass a real BIOS header check needs these bytes (devkitPro's `gbafix -p`, used by the mGBA suite Makefile, inserts them).
- `[TEST-SRC]` the mGBA suite repo LICENSE is MIT (2015 Jeffrey Pfau) but `include/mgba.h` carries a BSD 2-clause header (2016). Mixed per-file licences inside one repo.
- `[TEST-SRC]` the mGBA suite builds with devkitARM `gba_rules`, `libgba` and `gbafix`; NBA hw-test Makefiles link `-lmm -lgba` (Maxmod and libgba) and commit prebuilt `.gba` files. Those binaries embed third-party runtime code with its own licences. jsmolka commits prebuilt `.gba` files assembled with FASMARM.
- `[TEST-SRC]` NBA hw-test GitHub repo is archived (2026-06-21 "Close GitHub repository"); the project continues at `codeberg.org/nba-emu/hw-test` (BSD-3-Clause LICENSE present there, `© 2021 fleroviux`).
- The AGB Programming Manual link in the bootstrap is a third-party mirror of a Nintendo document that returned HTTP 403 to automated access. Rights status is unknown. `[INFERENCE]`
**How to avoid:** For each third-party fixture record: source URL(s) and pinned commit, who authored each included file, toolchain and linked libraries, digest of the exact binary, notices, and `redistributable: yes/no/unknown`. Default third-party fixtures to **fetched at a pinned revision and run locally, not vendored**, until the owner clears them. For Gamebloke's own diagnostics: build from original assembly with a documented toolchain, no linked runtime libs, and treat the logo area as a named owner decision (zeroed and flagged `boots-on-real-bios: no` unless the owner clears it). Never paste GBATEK text or Nintendo manual text into the repo; cite section names.
**Warning signs:** `third_party/` directory with `.gba` files and one LICENSE at the top; fixtures with no author-per-file record.
**Automated check:** A fixture-manifest check that fails on any `.gba`/`.bin` in the tree without a manifest entry; a scan for the logo byte sequence in committed files that fails unless the manifest entry has `contains_nintendo_logo: owner-cleared`.
**Phase:** verification-infra. Owner gate for the logo and any third-party vendoring.

### CP-7: Non-determinism leaks (UB, uninitialized memory, floats, threads, wall clock)

**What goes wrong:** Identical inputs give different traces across runs, compilers, optimization levels or hosts.
**Why it happens (GBA/ARM-specific causes first):**
- ARM shifts by 32 or more, `ASR #32`, `ROR #0` as RRX, and signed overflow in ADC/SBC flag computation map to **undefined behavior in C** if written naively (shift count >= width). This is the most common source of compiler-dependent CPU results. `[KNOWLEDGE]`
- Power-on RAM contents are hardware-random; games sometimes read uninitialized IWRAM/EWRAM. A core that mallocs without zeroing produces different guest behavior each run. `[KNOWLEDGE]`
- Host floating point in resampling or LCD colour correction inside the core; PPU or audio on a separate thread; `rand()`, `time()`, pointer-ordered containers; padding bytes in hashed state structs.
- RTC cartridges read the host clock (device deferred, but the boundary must be designed now).
**How to avoid:** Integer-only core. Profile defines power-on memory fill (for example zero or a documented seeded pattern). All shifts go through helpers that handle counts >= 32 explicitly. No threads inside the core. Time sources are injected values, never read. State hashing serializes fields explicitly (no raw struct hashing).
**Warning signs:** Hash differs between `-O0` and `-O2`, or between gcc and clang; flaky fixture that "passes on rerun".
**Automated check:** (a) build with `-fsanitize=undefined,address` in the canonical verify command; (b) the same fixtures at `-O0` and `-O2` and two compilers must produce byte-identical trace hashes; (c) run each fixture twice in one process and once in a fresh process with ASLR randomized; (d) a negative control: inject a deliberate uninitialized read and confirm the gate fails.
**Phase:** CPU (UB), verification-infra (gates), API+consumer (time injection).

### CP-8: Global state, callbacks and a public API drawn before the boundary is defined

**What goes wrong:** Static tables with mutable state, singleton "the GBA", or callback-driven bus access (core calls a host function for each read) make multiple instances, reentrancy, and refusal postconditions impossible, and the first consumer bakes the accident into an ABI.
**Evidence:** `[INFERENCE]` supported by the project's own constraint (instance independence not provable from a context-shaped API) and `[LibFuturo contract]` which defines refusal and ownership semantics per profile.
**How to avoid:** One heap-allocated machine struct, no mutable file-scope state (immutable decode tables are fine), explicit allocator or fixed-size blocks, no host callbacks on the hot path. Write the lifecycle/refusal-postcondition document before the first exported symbol.
**Warning signs:** `static` in function bodies that mutate; tests that must run in a fixed order; failed load leaving a half-initialized machine.
**Automated check:** Two-instance interleave test (step A,B,A,B and compare each against a solo run); a symbol scan (`nm`) failing on writable file-scope data not on an allow-list; refused-load test asserting machine state and caller buffers are byte-identical before and after.
**Phase:** API+consumer (boundary), verification-infra (checks); the checks should exist from the first CPU phase.

### CP-9: Save-state and save-file format lock-in

**What goes wrong:** A state or `.sav` layout is exposed early, then the core's internal timing state changes and every stored state or save becomes unreadable or silently wrong.
**GBA-specific risks:** State must include the pipeline/open-bus latches, per-channel DMA latch and in-flight DMA progress, prefetch buffer, timer phase, scheduler event queue, PPU line position, FIFO contents, and backup-device command state (flash command sequence, EEPROM bitstream position). `[INFERENCE]` Nearly every GBA save is battery-backed media; restoring an older machine state can overwrite newer save data. `[LibFuturo coordination doc, 2026-11-05]` the current snapshot profile refuses battery-backed content for exactly this reason.
**How to avoid:** Do not promise a state format in early milestones. Keep machine state in one relocatable struct so serialization stays possible. Specify save-media export/import (device-sized raw bytes, per-device) separately from machine snapshots. When states arrive: versioned header binding profile, BIOS mode, content digest, behavior revision; mid-DMA and mid-EEPROM-transfer round-trip tests; refusal leaves the receiver unchanged.
**Warning signs:** `fwrite(machine, sizeof machine)`; a state loader that trusts embedded lengths.
**Automated check:** Run N cycles, snapshot at a boundary and in-flight, restore into a fresh instance, run M more, compare against an uninterrupted run (trace hash equal).
**Phase:** API+consumer; cartridge for save-media format.

### CP-10: Overpromising compatibility; aggregate scores as claims

**What goes wrong:** "Supports GBA" or "passes 97% of the mGBA suite" appears in docs.
**Evidence:** `[TEST-SRC]` mGBA suite UI prints `passes/total` per suite (`sprintf("%4u/%-4u")`); `[ISSUE]` #22 quotes "1514/1552". One root cause can fail hundreds of generated sub-cases, so a percentage does not describe coverage. `[TEST-SRC]` suites change under you: mGBA suite commits in June-July 2026 fixed flaky DMA0 wrap-around tests and re-baselined a test "with modern gcc values". `[GBATEK]`/hw-test show the hardware itself varies by model.
**How to avoid:** A claim ladder in docs: *named profile -> named fixture ID -> expected provenance -> result*. No percentages. Denominators are pinned suite revision plus build digest. Group failures by root cause.
**Automated check:** A docs linter that fails on `%` next to "pass"/"compat" in README/release notes, and a report generator that emits per-test-ID rows only.
**Phase:** verification-infra.

### CP-11: First milestone overscoped

See "First-milestone critique" below. Short form: the bootstrap's first slice is five parallel critical paths and two owner gates in one milestone.
**Phase:** roadmap.

---

## Subsystem Edge-Case Catalog

Tables: *Pitfall | Warning signs | Prevention | Automated check | Phase | Evidence*. Items tagged `[KNOWLEDGE]` need a test or source before they become requirements.

### CPU (ARM7TDMI, ARMv4T)

| ID | Pitfall | Warning signs | Prevention | Automated check | Phase | Evidence |
|----|---------|---------------|------------|-----------------|-------|----------|
| CPU-1 | **ARMv5/ARM9 behavior copied into an ARMv4T core.** GBATEK's CPU chapter covers v4/v5/v6 together: LDR PC sets T only on v5; LDM with PC leaves T unchanged on v4; STM base-in-list stores differ v4 vs v5; MUL carry is "destroyed" on v4 and unchanged on v5. | Code copied from a DS/ARM9 emulator; BX-style interworking on `LDR PC`. | Per-instruction semantic notes tagged ARMv4T only. Never cite a GBATEK CPU line without checking the version tag. | jsmolka `arm.gba`/`thumb.gba` (`flags`, `block_transfer`, `multiply`, `psr_transfer` sources exist). | CPU | `[GBATEK]` (mgba-emu gbatek `gba.md` LDM/STM and MUL notes), `[TEST-SRC]` |
| CPU-2 | **Barrel shifter and carry.** Shift by immediate 0 vs 32 per type (LSR #0 means 32, ASR #0 means 32, ROR #0 means RRX), register shifts use the low 8 bits, counts >= 32, ROR by multiples of 32 (result unchanged, carry = bit 31), carry-out updates C only for logical ops with S, register-specified shift costs an extra internal cycle and PC reads as +12. Naive C is UB. | `x << n` with unchecked `n`; carry tests pass only at `-O0`. | Single shifter function returning `{value, carry}` with explicit branches for 0, 1-31, 32, >32. | mGBA suite `shifter` and `carry` suites, jsmolka `arm/shifts`, own exhaustive table test over (type, amount 0..40, value patterns, carry-in) against a second independent reference implementation. | CPU | `[KNOWLEDGE]`, `[TEST-SRC]` (suite files `shifter.c`, `carry.c`) |
| CPU-3 | **Pipeline-visible PC.** ARM R15 = instruction+8, Thumb +4; `STR PC` stores +12 in ARM state; register-shifted operand reads PC+12; Thumb PC-relative loads use word-aligned PC; branch flushes refill the 2-stage pipe. | PC computed only at decode; `STR PC` tests absent. | Keep R15 as "fetch address" and derive the visible value at read. | jsmolka `arm`/`thumb` pipeline-related tests, issue #8 test. | CPU | `[KNOWLEDGE]`, `[TEST-SRC]` |
| CPU-4 | **LDM/STM edge cases.** Empty register list (transfers R15 and adjusts base by +/-0x40, ARMv4), base register in list (STM stores old base if first in list else new; LDM base value wins, no writeback on v4), S-bit user-bank transfers, LDM with PC and S copies SPSR to CPSR, transfers always process the list in increasing address order even for decrementing modes (matters for I/O). Thumb LDMIA/STMIA share the empty-list rule. | Only the common PUSH/POP paths tested. | Implement from the v4 rules in `[GBATEK]`; keep one block-transfer routine for ARM and Thumb. | jsmolka `arm/block_transfer`, issues #2 and #12 (empty Rlist tests added because emulators got them wrong). | CPU | `[GBATEK]`, `[ISSUE]` |
| CPU-5 | **Misaligned and sub-word access.** Misaligned `LDR` returns the aligned word rotated; misaligned `LDRH` rotates by 8; misaligned `LDRSH` behaves as `LDRSB` (sign-extended byte); stores force alignment; byte/halfword reads of 32-bit-only regions extract from a rotated word. | Rotation only applied for `LDR`. | One load path that returns the raw aligned word + alignment and lets each instruction post-process. | mGBA suite `memory`, jsmolka `memory.gba` and `thumb/memory`. | CPU, bus+timing | `[GBATEK]` (rotate statement), `[KNOWLEDGE]` (LDRSH case) |
| CPU-6 | **MUL/MLA/long multiply cycles and flags.** Internal cycles depend on the multiplier operand's leading bits (early termination, 1-4), +1 for accumulate, long forms add; signed and unsigned long forms terminate differently; C flag value after MUL on ARMv4 is architecturally "destroyed" (hardware value must come from a test). | Fixed 2-cycle multiply. | Table-driven cycle function; do not guess the C value, take it from a hardware-derived test. | mGBA suite `multiply-long`, `timing`; jsmolka `arm/multiply`. | CPU | `[KNOWLEDGE]`, `[GBATEK]` |
| CPU-7 | **Thumb BL is two instructions.** First half sets LR, second half branches; each is fetched/timed separately and an IRQ can land between them. Fused BL changes LR in the intermediate state, cycle cost and ROM prefetch behavior. Thumb `POP {PC}` does not interwork on v4T. | One 32-bit "BL" step. | Implement as two Thumb instructions. | jsmolka `thumb/branches`; an own fixture taking an IRQ between BL halves. | CPU | `[KNOWLEDGE]` |
| CPU-8 | **CPSR/SPSR/mode handling.** Banked R13/R14 per mode and R8-R12 for FIQ, user-mode MSR can change flags only, SPSR read in User/System is unpredictable, T bit changes only via BX (and exception return), IRQ entry saves CPSR to SPSR_irq, sets I, forces ARM state, LR_irq = next instruction + 4. FIQ is not wired to a GBA source but banking must still be right for MSR. | Mode switch tests only via exceptions. | Banked-register table driven by mode; unit tests for MSR field masks. | jsmolka `arm/psr_transfer`, `flags`. | CPU | `[KNOWLEDGE]` |
| CPU-9 | **SWI/undefined/BKPT and coprocessor opcodes.** SWI immediate is bits 23-16 in ARM but the low byte in Thumb; undefined instructions and coprocessor opcodes take the undefined vector; HLE SWIs have no cycle cost unless charged; ARM7TDMI has no BKPT. | One SWI number decode for both states; HLE SWIs advancing 0 cycles. | HLE SWI handlers return a documented cycle count or explicitly charge "unmodeled". | mGBA suite `bios-math` (needs a BIOS mode), own SWI fixtures. | CPU | `[KNOWLEDGE]` |

### Bus and timing

| ID | Pitfall | Warning signs | Prevention | Automated check | Phase | Evidence |
|----|---------|---------------|------------|-----------------|-------|----------|
| BUS-1 | **WAITCNT configuration.** First access (N) 4/3/2/8 cycles, second (S) differs per wait-state region (WS0 2/1, WS1 4/1, WS2 8/1), SRAM waits, a 32-bit Game Pak access is two 16-bit accesses (the second always sequential), forced non-sequential at each 128 KiB boundary, cartridge starts with 4,2 waits and prefetch off at reset. | One S/N pair for all ROM; no 128 KiB boundary. | Wait-state table built from WAITCNT writes; region decode picks WS0/1/2. | mGBA suite `timing`, NBA `bus/128kb-boundary`. | bus+timing | `[GBATEK]`, `[TEST-SRC]` |
| BUS-2 | **Game Pak prefetch.** Eight-halfword buffer; only for opcode fetches from ROM; fills during internal cycles and during non-ROM data accesses; "prefetch disable bug" turns 1S into 1N for opcodes with internal cycles; branches flush; interplay with DMA and ROM data reads is the hard part. | "Prefetch enabled = all ROM fetches 1 cycle". | Model the buffer as state with its own clock. If deferred, the profile says `prefetch: unmodeled` and the manifest carries `requires_prefetch` for affected fixtures. | mGBA suite `timing` (LDM across ROM boundary was added after bugs, issue #13), own prefetch fixtures from the NBA tests once reviewed. | bus+timing | `[GBATEK]`, `[ISSUE]` |
| BUS-3 | **Open bus (CPU).** Reads from unused regions return the recently prefetched opcode: ARM `[$+8]`; Thumb results depend on region and alignment (two 16-bit fragments, special OldLO/OldHI behavior in IWRAM). GBATEK warns results may vary by model and conditions. | Unused reads return 0. | Implement the documented rule table with a profile flag; keep the last-prefetch latch. | mGBA suite `memory` out-of-bounds cases (note issue #22: they fail on hardware with a flash cart, so expected values need provenance). | bus+timing | `[GBATEK]`, `[ISSUE]` |
| BUS-4 | **BIOS read protection.** BIOS is readable only while PC is inside BIOS; otherwise reads return the last fetched BIOS opcode (value depends on whether boot, IRQ or SWI ran last). | HLE returns 0 or real bytes unconditionally. | See CP-3. | jsmolka `bios.gba` (real BIOS only); own state-machine unit test. | bus+timing | `[GBATEK]`, `[TEST-SRC]` |
| BUS-5 | **DMA open-bus latch.** DMA reads from unreadable addresses return a latch, not the CPU prefetch. NBA hw-test claims: separate latch per channel, 16-bit DMA duplicates the value into both halves, sources below 0x02000000 do not update the latch, unreadable I/O reads fall back to the prefetched opcode. mGBA 0.10.5 uses a single shared register. GBATEK does not mention a DMA latch (grep of the 3.06 text). | A single `uint32 dma_latch` or none. | Put it in the dispute registry (CP-5); implement per-channel as the profile default only after a hardware-derived source is identified, else flag `dma-latch: per-channel (hw-test claim, unverified)`. | NBA `dma/latch` as differential signal; own ROM reproducing it with provenance field. | bus+timing, IRQ-timer-DMA | `[EMU-SRC]`, `[TEST-SRC]`, `[GBATEK]` |
| BUS-6 | **Memory mirrors and widths.** EWRAM 256K and IWRAM 32K repeat through their 16 MB windows; palette 1K; VRAM repeats in 128K steps (64K+32K+32K); BIOS, ROM and I/O are not mirrored except the register at 0x04000800 (every 64K, AGB/AGS only; locks up GBA Micro at the fastest EWRAM wait; unsupported in DS GBA mode); 8-bit writes to BG/palette VRAM duplicate the byte, to OAM and OBJ VRAM are ignored; SRAM 16/32-bit access replicates the byte. | One flat 16 MB array per region without write-width rules. | Region descriptors with mirror mask, width rules and write semantics. | jsmolka `memory/mirrors`, `video_strb`; mGBA suite `memory`. | bus+timing | `[GBATEK]` |
| BUS-7 | **Post-BIOS state and uninitialized RAM.** Direct boot needs register, stack-pointer, IWRAM and I/O defaults, or games read garbage. See CP-3, CP-7. | Boot only sets PC. | Written post-boot state table per BIOS mode. | Own fixture that reads documented default registers and a canary region. | CPU, bus+timing | `[GBATEK]` (default stack areas), `[KNOWLEDGE]` |
| BUS-8 | **HALT and STOP.** HALTCNT write pauses the CPU until (IE and IF) is nonzero (timers, DMA, PPU continue); STOP pauses most hardware including video and sound and needs a wake source (keypad IRQ, Game Pak IRQ). POSTFLG interacts with reset. A deterministic runner can deadlock on STOP with no input. | Halt implemented as "skip to next frame". | Wake conditions evaluated at exact edges; runner reports `guest-stopped` as its own result, not `timeout`. | NBA `haltcnt`; own STOP-with-no-wake fixture expecting `guest-stopped`. | IRQ-timer-DMA, verification-infra | `[GBATEK]`, `[TEST-SRC]` |

### IRQ, timers, DMA

| ID | Pitfall | Warning signs | Prevention | Automated check | Phase | Evidence |
|----|---------|---------------|------------|-----------------|-------|----------|
| IRQ-1 | **IRQ delay and race with IE/IF/IME writes.** The CPU takes an IRQ after a short, specific latency from the request; clearing IME or IE races with a pending request (GBATEK quotes Nintendo's caution and marks it with "?"). IF is write-1-to-clear. | IRQ taken at instruction boundary with 0 delay; handlers not tested when IE is cleared in the same window. | Sample the IRQ line at the documented point; test the race cases. | NBA `irq/irq-delay` and archived `cancel-irq-ie/if/ime`; mGBA suite `timer-irq`. | IRQ-timer-DMA | `[GBATEK]`, `[TEST-SRC]` |
| IRQ-2 | **Timer start delay, reload and cascade.** Writing the reload value does not change the live counter; the reload is copied on overflow or on the 0-to-1 start transition (a simultaneous 32-bit write uses the new reload); count-up (cascade) increments when the lower timer overflows and is not valid for timer 0; prescalers 1/64/256/1024; starting a timer has a latency; reads during run must return the exact counter. Lazy "compute on read" timers often mis-order cascades and FIFO sampling. | Timers advance per frame or per scanline; cascade tested only at one prescaler. | Timer state expressed in master-clock events; exact overflow timestamps. | NBA `timer/reload`, `timer/start-stop`; mGBA suite `timers`, `timer-irq`. | IRQ-timer-DMA | `[GBATEK]`, `[TEST-SRC]` |
| IRQ-3 | **DMA start, priority and stall behavior.** The CPU is paused while a DMA runs; priority DMA0 > 1 > 2 > 3 and a lower channel is paused while a higher one runs; immediate DMAs start after a short delay (GBATEK: wait 2 cycles before touching DMA registers); transfers are 1N + (n-1)S per side with 2I internal (4I if both ends are in Game Pak); count 0 means 0x4000 (DMA0-2) or 0x10000 (DMA3); addresses are masked to 27 bits (DMA0 source, all destinations except DMA3) or 28 bits; HBlank/VBlank/special start; sound FIFO mode ignores count and width and moves 4 words. | Immediate DMA runs instantly and atomically; DMA does not stall the CPU. | DMA as a bus master in the scheduler; per-unit arbitration; per-channel state. | mGBA suite `dma` (large generated matrix), NBA `dma/start-delay`, `dma/force-nseq-access`, `dma/burst-into-tears`. | IRQ-timer-DMA | `[GBATEK]`, `[TEST-SRC]` |
| IRQ-4 | **GBATEK DMA statements that contradict.** The memory-map and "Game Pak DMA" sections say only DMA3 may access Game Pak ROM, yet DMA1/DMA2 source registers are described as "any memory" and some lines carry "?!" markers. The mGBA forum errata list says DMA3 transfers from SRAM to ROM can hard-lock the GBA. | A rule coded from one GBATEK paragraph. | Decide by test and record in the dispute registry. | NBA/mGBA DMA tests as differential signal; own fixtures per channel/region. | IRQ-timer-DMA | `[GBATEK]`, `[ISSUE]` |
| IRQ-5 | **EEPROM DMA coupling.** EEPROM access works only via DMA3 with specific lengths; ordinary LDRH/STRH do not work; higher-priority DMAs must be disabled. | EEPROM implemented as memory-mapped bytes. | See CART-2. | See CART-2. | IRQ-timer-DMA, cartridge | `[GBATEK]` |

### PPU

| ID | Pitfall | Warning signs | Prevention | Automated check | Phase | Evidence |
|----|---------|---------------|------------|-----------------|-------|----------|
| PPU-1 | **Frame and line timing constants.** 240 visible dots, 68 HBlank, 4 cycles per dot = 1232 cycles per line, 228 lines = 280,896 cycles per frame; HBlank IRQ/DMA are not generated during VBlank lines; the exact cycle where the HBlank flag changes within a line must come from a test, not prose. Frame rate is 16,777,216/280,896 = **262144/4389 Hz = 59.7275 Hz**, not 60 and not GBATEK's printed 59.737. | Hardcoded 60 fps; 59.737 constant. | Constants derived from cycle counts. | Unit test asserting the arithmetic and a trace test of DISPSTAT/VCOUNT edges against NBA `ppu/status-irq-dma` and mGBA `video` (`hblankBit`). | PPU, bus+timing | `[GBATEK]`, `[ARITH]` |
| PPU-2 | **Affine reference point latching.** Internal reference registers load at VBlank, reload immediately when written outside VBlank, then step by PB/PD per line; mosaic and DMA-to-register effects complicate this; registers are 28-bit signed. | Per-frame copy only. | Internal reference registers with timestamped writes. | NBA `ppu/bgx`, `ppu/bgpd`; mGBA suite `video`. | PPU | `[GBATEK]`, `[TEST-SRC]` |
| PPU-3 | **OBJ cycle budget.** Per-line OBJ rendering budget is 1210 cycles (954 if "H-Blank Interval Free" is set); normal OBJ costs 1 cycle/pixel, affine OBJ 10 + 2/pixel; offscreen higher-priority OBJs still consume budget; whether vertically-offscreen OBJs consume budget is an open question in GBATEK itself. OAM changes become visible with a delay (mGBA suite has an "OAM Update Delay" video test, issue #8). | All 128 OBJs always drawn. | Budget accounting in the line renderer, with the open GBATEK question listed as a dispute. | mGBA suite `video`; own overload ROM. | PPU | `[GBATEK]`, `[ISSUE]` |
| PPU-4 | **Mosaic.** BG and OBJ have separate sizes; vertical mosaic counters reset per frame; interactions with affine BGs and with horizontal sprite mosaic have documented hardware tests. | Mosaic as a post-process on the finished line. | Per-layer mosaic latch states. | NBA `ppu/sprite-hmosaic`. | PPU | `[TEST-SRC]` |
| PPU-5 | **Windows.** WIN0/WIN1 priority, coordinate wrapping when X2 < X1, OBJ window, per-window effect enable. The mGBA forum errata says GBATEK does not describe wrapping properly; mGBA suite issue #27 reports an incorrect "Window offscreen reset" expectation. | Window rectangle as min/max. | Implement wrap rules from hardware tests only. | mGBA suite `video`; own window fixtures with provenance field. | PPU | `[ISSUE]` |
| PPU-6 | **Blending edge cases.** Only first-target over second-target pixels blend; coefficients saturate; semi-transparent OBJ overrides the BLDCNT brighten/darken mode (affects Golden Sun fights per the errata list); effects can be disabled by window; BLDALPHA is readable but BLDY is not. | Blend applied to any pixel pair. | Layer-pair state machine; document 5-bit truncation vs rounding via test. | mGBA suite `video`; own blend matrix. | PPU | `[ISSUE]`, `[KNOWLEDGE]` |
| PPU-7 | **Bitmap modes and OBJ VRAM boundary.** Modes 3-5 reduce OBJ tiles to 512-1023 (OBJ VRAM starts at 0x06014000); GBATEK describes a 128K VRAM repeat, but NBA hw-test `vram-mirror` reports that in bitmap modes accesses to 0x06018000-0x0601BFFF do not behave as a mirror (writes discarded, reads "may always return 0?") and that this passes on GBA and GBA SP but not on DS Lite/3DS GBA mode. Invalid tile numbers in text modes behave differently between GBA/SP and DS. | Mirror implemented as `addr & 0x1FFFF` with no mode dependence. | Mode-dependent VRAM access function. | NBA `ppu/vram-mirror`, jsmolka `memory/video_strb`; keep it in the dispute registry (hardware vs GBATEK). | PPU, bus+timing | `[GBATEK]`, `[TEST-SRC]` |
| PPU-8 | **Video-memory access contention.** CPU access to VRAM/palette/OAM costs +1 cycle when the PPU uses the memory at the same time (GBATEK footnote); OAM is accessible in HBlank only with "H-Blank Interval Free"; forced blank allows fast access. | No contention penalty. | Mark `vram-contention: unmodeled` in the profile until a test drives it. | NBA `ppu/ram-access-timing`. | PPU, bus+timing | `[GBATEK]`, `[TEST-SRC]` |
| PPU-9 | **Raw output vs host conversion.** The core's raw video is 15-bit BGR555 per pixel plus forced-blank behavior; LCD colour correction/gamma and scaling belong to the host. SP front-light vs backlight and Micro screens are host presentation, not machine behavior. | Core emits RGB888 with a gamma curve. | Define raw video as BGR555 + frame metadata. | Round-trip test of raw format. | PPU, API+consumer | `[GBATEK]`, `[INFERENCE]` |

### Audio

| ID | Pitfall | Warning signs | Prevention | Automated check | Phase | Evidence |
|----|---------|---------------|------------|-----------------|-------|----------|
| AUD-1 | **Direct Sound FIFO model.** 32-byte (8 x 32-bit) FIFO per channel; a timer overflow pops one byte; at 16 bytes remaining the channel requests 4 words by DMA1/DMA2 (special mode, destination fixed); reset-FIFO bits; each channel selects timer 0 or 1. GBATEK does not specify underrun (empty on overflow) or overrun behavior; a higher-priority DMA can starve the request. | Ring buffer with no request threshold; underrun produces silence by assumption. | Implement the pop/request rule exactly; make underrun/overrun an explicit profile choice backed by a test. | Own FIFO fixtures: underrun, overrun, reset, DMA starvation. | audio, IRQ-timer-DMA | `[GBATEK]` |
| AUD-2 | **Timer-to-FIFO coupling.** Sample cadence is the exact timer overflow period in master-clock cycles, including reload writes during playback; lazy timer evaluation breaks this. | Audio generated per frame from a nominal sample rate. | Timer overflow events directly drive FIFO pops. | Cycle-exact cadence test for several reload values. | audio, IRQ-timer-DMA | `[GBATEK]` |
| AUD-3 | **SOUNDBIAS resolution and rate.** Bits 14-15 select 9/8/7/6 bit at 32.768/65.536/131.072/262.144 kHz; default bias 0x200; FIFO channels span the full +/-0x200 range, each PSG channel a quarter; the mix plus bias clips to 0..0x3FF. At the default rate a frame is 280,896/512 = **548.625 samples**, i.e. 4389 samples per 8 frames, so frame-sized audio buffers are not integer-sized. The rate can change at runtime. | Fixed 44.1 kHz output inside the core; per-frame buffer of constant size. | Core outputs samples tagged with the SOUNDBIAS-selected sample clock; resampling is a host function. | Unit test of mixing/clipping arithmetic; cadence test that 8 frames produce 4389 samples at default bias. | audio | `[GBATEK]`, `[ARITH]` |
| AUD-4 | **PSG differs from Game Boy/CGB.** GBA PSG registers live at 0x04000060-0xAE, wave RAM is two 32-digit banks with bank/dimension select and a 75% volume bit, there is no Vin, SOUNDCNT_X bit 7 clearing resets PSG registers, PSG is mixed through SOUNDCNT_H volume (25/50/100%) beside the FIFOs. Reusing GabbaBoy's APU without these deltas is a hardware-transfer trap. | APU lifted from the GB core; wave RAM single bank. | Treat GB APU code as a reference for the frame-sequencer idea only; implement GBA register/mixer behavior from GBATEK "Comparison of CGB and GBA Sound". The mGBA forum calls "The Audio Advance" the best audio reference (secondary report). | PSG register-reset and wave-bank fixtures. | audio | `[GBATEK]`, `[ISSUE]` |
| AUD-5 | **Host audio callback drives the emulation loop.** Couples the core to wall clock and creates nondeterminism and underruns. | Core step size chosen by the audio device buffer. | Core advances by guest-time units; host buffers and resamples. | Headless run produces identical raw audio regardless of host buffer size. | API+consumer | `[INFERENCE]` |

### Cartridge and backup media

| ID | Pitfall | Warning signs | Prevention | Automated check | Phase | Evidence |
|----|---------|---------------|------------|-----------------|-------|----------|
| CART-1 | **Save-type misdetection.** There is no backup-type field in the header; Nintendo's tools embed ID strings (`EEPROM_Vnnn`, `SRAM_Vnnn`, `FLASH_Vnnn`, `FLASH512_Vnnn`, `FLASH1M_Vnnn`); the strings are word-aligned library text, can appear in unrelated data, can be absent in homebrew/hacks, and `EEPROM_V` does not give the size; no$gba defaults 128K flash to 64K without `FLASH1M_V`. The project forbids filename/title inference. | A game database keyed by title; silent default to SRAM. | Save type and size are an explicit input to the profile/manifest. A tag scan may only produce an advisory suggestion the runner reports; "unspecified" is a refusal, not a default. | Matrix ROMs: each tag, conflicting tags, no tag, tag inside data. Assert reported advisory and the refusal path. | cartridge | `[GBATEK]`, `[TEST-SRC]` (mGBA suite embeds `SRAM_V123`) |
| CART-2 | **EEPROM size autodetection via DMA length.** GBATEK: "no autodetection mechanism... a hardcoded bus width must be used". Emulators such as mGBA infer the size at run time from the DMA transfer length (`GBASavedataWriteEEPROM`, `_ensureEeprom` in 0.10.5): a set-address-for-read is 2+6+1 = 9 bits (512 B) or 2+14+1 = 17 bits (8 KB), writes 73 or 81 bits, and the data read is 68 bits either way. Pitfalls: guessing on the first read, losing a size decision across restore, import of a save file whose size disagrees, wrong address window (EEPROM at 0x0DFFFF00-0x0DFFFFFF when ROM > 16 MB, anywhere in 0x0D000000-0x0DFFFFFF otherwise), expecting CPU halfword access to work. | Size picked on first access; EEPROM mapped as bytes. | Make size a manifest input; treat inference as advisory and mark "size undetermined" until a set-address transfer arrives; document refusal when a transfer contradicts the declared size. | Synthetic ROM issuing 9-bit and 17-bit address transfers; negative control with wrong size. | cartridge, IRQ-timer-DMA | `[GBATEK]`, `[EMU-SRC]` |
| CART-3 | **Flash chip IDs, commands, banking.** ID reads via AA/55/90 then 0xF0; device and manufacturer bytes at 0xE000001/0xE000000 (SST D4BF, Macronix 1CC2, Panasonic 1B32, Atmel 3D1F, Sanyo 1362, Macronix 128K 09C2 per GBATEK); 4 KB sector erase 0x30 except Atmel (128-byte erase-write sector); chip erase 0x10; erased bytes read 0xFF; writes need prior erase; bank select command 0xB0 for 128K; Macronix force-end 0xF0; games poll for completion and use ID to pick a driver. Picking the wrong ID silently makes the driver issue the wrong command set. | A flat 64K/128K RAM with a fixed ID. | Flash as a command state machine selected by the profile's chip ID; persist as a device-sized image. | jsmolka `save/flash*`, `save/flash128`, `save/flash64`, mGBA suite memory/save paths; own fixtures per chip family. Note: a jsmolka issue (#14) reports timing sensitivity on flash carts. | cartridge | `[GBATEK]`, `[TEST-SRC]`, `[ISSUE]` |
| CART-4 | **ROM size, mirroring and reads past the end.** The header has no size field; ROM length is the file length (up to 32 MiB); WS0/WS1/WS2 windows alias the same ROM; reads past the end are cartridge-dependent (GBATEK: with no cartridge the area is filled by "Address/2 AND FFFFh"; jsmolka's `unsafe` test says this passes on mGBA, fails on no$gba and hardware); non-power-of-two and odd-length files exist. | Reads past the end return 0 or crash. | Profile chooses and labels the behavior; length rules in the loader (zero length, below header size, above 32 MiB, odd length) produce distinct refusal codes. | Fuzz/length-sweep over every boundary; unused-ROM-read fixture marked `provenance: disputed`. | cartridge | `[GBATEK]`, `[TEST-SRC]` |
| CART-5 | **GPIO window lives inside ROM address space.** RTC/solar/rumble/gyro registers sit in the ROM chip near 0x080000C4. jsmolka's header branches to 0x080000C0, so test ROM code can occupy bytes in that range; if GPIO is always mapped, ordinary ROM reads are corrupted. | GPIO mapped unconditionally. | Map GPIO only when the profile enables a device and the guest enables read access. Devices are deferred; keep the window rule documented. | jsmolka ROMs must still run with GPIO disabled. | cartridge | `[GBATEK]`, `[TEST-SRC]`, `[KNOWLEDGE]` (port addresses) |
| CART-6 | **Malformed or truncated ROM/save/BIOS as untrusted input.** Sizes from file length are trusted for allocation; header logo/checksum/fixed-byte validation either rejects homebrew (jsmolka's header leaves the checksum zero until patched) or is skipped silently; save import with wrong length. | `malloc(file_size)` before limits; header check hard-fails. | Validate length and extents before allocation or mutation; header validation is a profile policy (warn vs refuse) with stable status codes; refusal leaves the machine untouched. | Truncate every fixture at every header/size boundary; oversize and odd-size cases; assert before/after state equality. | cartridge, API+consumer | `[INFERENCE]` |
| CART-7 | **Flash-cart contaminated hardware results.** Hardware runs on flash carts (EZ-Flash Omega, "dev cart") can differ from retail carts in ROM timing, mirroring and SRAM/flash behavior. Both mGBA suite #22 and jsmolka #14 report results on such carts. | A hardware capture with no cart recorded. | Manifest hardware-capture entries record cart type and console model. | Manifest linter (CP-5). | verification-infra | `[ISSUE]` |
| CART-8 | **SRAM mirroring and size.** GBATEK states the 64K SRAM area mirrors across E000000h-FFFFFFFh and 32K chips repeat twice; jsmolka's `unsafe` test says this fails on real hardware (with a flash-card caveat); mGBA suite #17 notes SRAM mirror tests wrote to OAM, corrupting later runs. SRAM and FRAM both behave as 8-bit RAM. | Mirror implemented from the GBATEK sentence. | Dispute-registry entry; default to the smallest claimed behavior, flagged. | `unsafe/` ROM as a labeled disputed fixture. | cartridge | `[GBATEK]`, `[TEST-SRC]`, `[ISSUE]` |

### Testing and evidence

| ID | Pitfall | Warning signs | Prevention | Automated check | Phase | Evidence |
|----|---------|---------------|------------|-----------------|-------|----------|
| EV-1 | **Visual-only results.** jsmolka shows the number of the first failed test on screen in mode 4 (README); the mGBA suite renders a text UI and logs through the `mgba.h` interface, which is an mGBA-specific channel, not hardware; NBA hw-test prints PASS/FAIL text via a devkitPro console and compares against committed `expected.png`/`.jpg` images of unstated capture method. | A runner that screenshots and eyeballs. | Per-suite result adapters. jsmolka sources set register r12 to the test number before branching to a shared `eval` routine, so r12 at the terminal loop is a candidate semantic channel (verify by reading `eval`; `[INFERENCE]`). For own fixtures, write results to a documented IWRAM mailbox that the host reads; no emulator-only bus device. | Adapter unit tests; a fixture whose adapter finds no result must report `harness-failed`, never `pass`. | verification-infra | `[TEST-SRC]` |
| EV-2 | **BIOS-dependent fixtures in public CI.** jsmolka `bios.gba` needs real BIOS-fetched opcodes; mGBA `bios-math` needs BIOS routines; NBA requires a user-supplied BIOS in its own README. | CI that "passes" because the test was skipped or auto-detected. | `requires_bios` in the manifest; status `skipped-bios-unavailable` is distinct and counted; public CI runs only BIOS-free fixtures. | Gate: required BIOS-free fixture skipped -> fail; BIOS-required fixture reported pass without a BIOS -> fail. | verification-infra | `[TEST-SRC]` |
| EV-3 | **Expected values depend on the toolchain.** mGBA suite commit "Update hblankBit test with modern gcc values" shows expectations tied to compiler output; rebuilding is not the same fixture. | Building third-party suites in CI from source. | Manifest stores the prebuilt binary digest plus build recipe and toolchain version; never regenerate expected values from a rebuilt binary without review. | Digest check at load. | verification-infra | `[TEST-SRC]` |
| EV-4 | **Hash-only "accuracy".** A frame or audio hash proves repeatability. Re-baselining golden hashes from your own output is `self-derived`. | `update-golden` scripts with no review step. | Pair every hash with a semantic assertion (guest progress, register/memory state) or an independent expected image. | Gate rejecting a golden-hash change without a manifest provenance change. | verification-infra | `[INFERENCE]` (matches AGENTS.md) |
| EV-5 | **"GBA" is not one machine.** Hardware differences are machine-visible, not just host-visible: the 0x04000800 EWRAM wait register is unsupported on DS GBA mode and locks up a GBA Micro at the fastest setting; the Micro lacks the cartridge switch and cannot detect 8-bit carts; invalid-tile rendering differs on DS; NBA reports `vram-mirror` fails on DS Lite/3DS in GBA mode while passing on GBA/SP. | Profile says "GBA" only. | Profile names the model (AGB, AGS, Micro, DS-in-GBA-mode) and which differences are modeled. | Manifest requires `model`; fixtures list models they were validated on. | verification-infra | `[GBATEK]`, `[TEST-SRC]` |
| EV-6 | **Suite and link drift.** NBA hw-test moved to Codeberg (GitHub archived), mGBA suite changed in 2026, GBATEK exists in several versions (2.9b fork vs 3.06). | Floating URLs, unpinned revisions. | Pin by commit and digest; record all known URLs; record GBATEK version per citation. | Manifest linter. | verification-infra | `[TEST-SRC]`, `[GBATEK]` |

### Project and process

| ID | Pitfall | Warning signs | Prevention | Automated check | Phase | Evidence |
|----|---------|---------------|------------|-----------------|-------|----------|
| PROC-1 | **Premature JIT/dynarec.** ARM7TDMI is a 16.78 MHz in-order core; a correct interpreter is already far faster than real time on a modern host, while a JIT adds nondeterminism risk and breaks the stale-pipeline semantics of CP-2. | Performance work before a named workload and correctness baseline. | Interpreter first; optimization only after a correctness baseline and a named workload, outputs held constant (also AGENTS.md rule). | Benchmark gate that compares trace hash before and after any optimization. | CPU, verification-infra | `[INFERENCE]` |
| PROC-2 | **Core coupled to host frontends.** SDL/windowing calls, `printf` tracing, file access, wall-clock pacing or RTC time reads inside the core; link-cable sockets. | Core links a host library. | Core target has no host includes; host adapters are separate targets; time and I/O are injected. | CI link check (core library must link with no external symbols beyond libc subset); ban-list grep. | API+consumer | `[INFERENCE]` |
| PROC-3 | **LibFuturo fit assumed.** LibFuturo currently has synthetic, NES and Atari 7800 profiles (no GBA) and a frame-oriented advance. GBA specifics to prove: exact cadence 262144/4389 fps, 548.625 audio samples per frame at default bias, runtime sample-rate changes, BGR555 video, BIOS-mode plus cartridge identity, three save-device families, and the snapshot profile's refusal of battery-backed content. | Gamebloke bending its core to a frame-only input model. | Build the real core and consumer first; prove a concrete mismatch before proposing a contract change (AGENTS.md). | Adapter test only after the core API is stable. | API+consumer | `[LibFuturo README/contract]`, `[ARITH]` |
| PROC-4 | **Cross-project transfer.** GabbaBoy's APU and timing assumptions, Playstead's mGBA route assumptions, or another emulator's stops are copied as hardware facts. | "Same as GabbaBoy" in a design note. | DNA rule: lessons transfer, hardware and gates do not. | Review checklist item. | all | `[DNA]` |
| PROC-5 | **Toolchain for own fixtures drags in licences.** devkitARM crt0/libgba/libtonc/Maxmod bring their own licences into binaries. | Fixtures built with `-lgba`. | Freestanding assembly plus own linker script; header generator without logo; record the toolchain in the manifest. | Manifest linter lists linked libraries; fails on non-empty list without clearance. | verification-infra | `[TEST-SRC]` (hw-test links `-lmm -lgba`) |

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Instruction-counted time | Fast first CPU | Rewrite of CPU/bus/DMA boundary (CP-1) | Never |
| Fused Thumb BL / decode cache without pipeline proof | Speed | Wrong SMC, IRQ-between-BL, open bus (CP-2) | Only after pipeline tests pass and cache-off equivalence is gated |
| Once-per-frame PPU | Quick pictures | Rewrite for HBlank effects (CP-4) | Only as a throwaway debugging viewer, never as the machine output |
| Flat 64K flash/EEPROM array | Saves "work" | Wrong driver detection, lost saves (CART-2/3) | Never for a support claim; fine for a labeled stub |
| Zero-fill power-on RAM undocumented | Deterministic | Hides games that read RAM | Acceptable if the profile documents it |
| Copying third-party test binaries into the repo | Convenience | Rights exposure (CP-6) | Only after manifest + owner clearance |
| Accepting emulator output as expected values | Quick coverage | Shared errors (CP-5) | Only labeled `emulator-derived`, never as proof |
| Skipping unmodeled features silently | Fewer failures | Hidden gaps | Never; mark `unmodeled` in the profile |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| LibFuturo | Forcing frame-only input and fixed-size audio onto GBA | Prove the specific mismatch with a real core; use 262144/4389 fps and tagged sample clock (PROC-3) |
| Test suites | Taking their pass counter as the result | Per-suite result adapters and per-test IDs (EV-1, CP-10) |
| Host audio | Letting the device callback pace the core | Guest-time advance; host resamples (AUD-5) |
| Save files | Assuming other emulators' `.sav` layouts and sizes match | Define Gamebloke's device-sized raw format; test imports as untrusted (CART-6); EEPROM byte/bit order across tools is not hardware-defined `[KNOWLEDGE]` |
| BIOS | Bundling or auto-downloading dumps | User-supplied or independently authored paths with explicit BIOS mode (CP-3) |
| Playstead/mGBA | Assuming replacement | Out of scope per PROJECT.md; record differentiator evidence instead |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Per-cycle stepping of every component | 16.8 M loop iterations/s of host work with branchy checks | Event scheduler with next-event time; step only to the next event `[INFERENCE]` | Matters when adding PPU/audio per-dot work, not before |
| Checking IRQ/DMA/timers after every instruction | Slow headless runs | `next_event` comparison on the master clock | Visible once timers and DMA are exact |
| Per-access function-pointer calls through a host callback | Slow and breaks determinism | Region-decoded inline access | Visible in ROM-heavy workloads |
| Optimizing before a named workload exists | Churn; regressions in accuracy | AGENTS.md: correctness baseline first, hold outputs constant | From the first optimization attempt |
| Wide CI matrices | Long wait, little signal | Add only for a distinct recurring failure class | From the first CI setup |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Allocating from file length or header-adjacent fields | Memory exhaustion/overflow (High) | Validate length and extents before allocation (CART-6) |
| Guest-controlled indices into VRAM/OAM/tile maps (invalid tile numbers, affine coordinates, mosaic sizes) | Host OOB read/write (High) | Mask/clamp in the access function; sanitizer builds and fuzzing |
| Trusting EEPROM bitstream length or flash command sequences | State corruption; OOB writes (Medium) | State machines with bounded counters and refusal on unexpected length |
| DMA count/address arithmetic (0x10000 counts, 27/28-bit masks, decrement past 0) | Integer overflow (Medium) | Use 32-bit math with explicit masking; unit tests at the limits |
| Loading a state or save without a digest and length check | Corruption/arbitrary state (Medium) | Versioned header, bounded lengths, refusal postconditions (CP-9) |
| Treating BIOS file as trusted | Wrong-mode execution or crashes (Low-Medium) | Length check (16 KiB), digest recorded, mode from profile |
| Embedding Nintendo-owned bytes (logo, BIOS words) unintentionally | Rights exposure (High) | CP-6 scans and owner gate |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Silent save-type default | Lost progress | Report advisory detection and require explicit configuration (CART-1) |
| Missing BIOS prompt with no mode explanation | Confusing failures | Name the BIOS mode and what it can and cannot run (CP-3) |
| Ambiguous exit status in the runner | CI can't tell harness failures from guest failures | Distinct codes/statuses: missing, skipped, harness-failed, timed-out, cancelled, guest-failed, guest-stopped |
| "Compatible" labels | Overpromise | Claim ladder (CP-10) |

## "Looks Done But Isn't" Checklist

- [ ] **CPU passes jsmolka arm/thumb:** often missing timing (cycle costs, prefetch) -> verify an independent timing test and that the profile lists prefetch as modeled or unmodeled.
- [ ] **Title screen boots:** often missing correct save-device behavior -> verify save/reload on each backup device with explicit configuration.
- [ ] **Audio "plays":** often missing exact FIFO/timer cadence and SOUNDBIAS handling -> verify the 4389-samples-per-8-frames cadence and clipping math.
- [ ] **Frame looks right:** often missing mid-frame latch behavior -> verify an HBlank DMA to BG2X/DISPCNT ROM.
- [ ] **Headless runner exits 0:** often means "no test ran" -> verify the negative control and that skipped/missing are visible.
- [ ] **Installed consumer works:** often only works from the source tree -> verify after relocating the install prefix and running from a clean directory.
- [ ] **Fixture "MIT-licensed":** often missing per-file/embedded-asset review -> verify logo scan and linked-library list.
- [ ] **Two instances supported:** often untested -> verify the interleave test.
- [ ] **State restore works:** often only at frame boundaries -> verify mid-DMA and mid-EEPROM-transfer snapshots.
- [ ] **"Deterministic":** often only on one compiler/opt level -> verify gcc/clang and -O0/-O2 trace equality and sanitizer builds.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Coarse timing (CP-1) | HIGH | Introduce the bus-access cost API, rewrite handlers; re-baseline fixtures with provenance |
| Fused/cached pipeline (CP-2) | MEDIUM-HIGH | Reinstate the fetch pipeline; keep cache behind an equivalence gate |
| Wrong BIOS mode assumptions (CP-3) | MEDIUM | Add BIOS-mode field and post-boot tables; reclassify fixtures |
| Per-frame PPU (CP-4) | HIGH | Move to per-line renderer with timestamped writes |
| Emulator-derived expectations (CP-5) | MEDIUM | Relabel provenance; remove accuracy claims; seek hardware-derived sources |
| Rights problem found late (CP-6) | MEDIUM-HIGH | Remove binaries, rewrite history if public (owner gate), re-author fixtures |
| UB-driven nondeterminism (CP-7) | MEDIUM | Sanitizer pass, replace shift/arith helpers, bisect by trace diff |
| Global state (CP-8) | HIGH | Introduce machine struct, remove statics; API break |
| State/save lock-in (CP-9) | HIGH | Versioned migration or explicit break with documentation |
| Overpromised compatibility (CP-10) | LOW-MEDIUM | Retract claims; adopt the claim ladder |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| CP-1 timing model | CPU, bus+timing | Access-cost table tests; lint that handlers use the bus API |
| CP-2 pipeline/prefetch | CPU, bus+timing | Pipeline/SMC fixtures; cached vs plain trace equality |
| CP-3 BIOS policy | CPU, bus+timing, verification-infra | Per-mode fixture matrix; BIOS-skip gate |
| CP-4 PPU latching | PPU (+ IRQ-timer-DMA) | Raster-effect fixtures with provenance |
| CP-5 oracle trust | verification-infra | Manifest linter on `expected_provenance`; dispute registry |
| CP-6 fixture rights | verification-infra (owner gate) | Logo scan, manifest completeness check |
| CP-7 non-determinism | CPU, verification-infra | Sanitizers, -O0/-O2, two-compiler trace equality |
| CP-8 globals/API | API+consumer (checks from CPU phase on) | Interleave test, symbol scan, refusal postcondition tests |
| CP-9 state/save lock-in | API+consumer, cartridge | Boundary and in-flight snapshot round trips |
| CP-10 overpromise | verification-infra | Docs linter, per-test-ID reports |
| CP-11 overscoped M1 | roadmap | Milestone exit criteria review |
| CPU-1..9 | CPU | jsmolka arm/thumb, mGBA shifter/carry/multiply-long, own exhaustive tables |
| BUS-1..8 | bus+timing | mGBA timing/memory suites, NBA bus/haltcnt, own tables |
| IRQ-1..5 | IRQ-timer-DMA | NBA irq/timer/dma ROMs, mGBA timers/timer-irq/dma |
| PPU-1..9 | PPU | NBA ppu ROMs, mGBA video, own raster fixtures |
| AUD-1..5 | audio | Cadence and mixing unit tests, FIFO fixtures |
| CART-1..8 | cartridge | Save-device matrix, length sweep, flash/EEPROM state-machine tests |
| EV-1..6 | verification-infra | Adapter tests, skip gates, digest checks |
| PROC-1..5 | CPU, API+consumer, verification-infra | Benchmark gate, link check, review checklist |

## Phase-Specific Warnings and Research Flags

| Phase Topic | Likely Pitfall | Mitigation | Needs deeper phase research? |
|-------------|----------------|------------|------------------------------|
| CPU | UB in shifts; ARMv5 contamination; pipeline | CP-7, CPU-1..3 | Standard patterns; TRM cycle tables should be read in full during planning |
| bus+timing | Prefetch/DMA interplay; open-bus rules; BIOS-read state | CP-2, BUS-2..5 | **Yes** (hardware-derived sources are thin; GBATEK silent on DMA latch) |
| IRQ-timer-DMA | IRQ latency exactness; DMA arbitration; start delays | IRQ-1..3 | **Yes** (NBA hw-test provenance unverified) |
| PPU | Latch points; OBJ budget; window/blend errata | CP-4, PPU-2..7 | **Yes** for latch points; modes 0-5 basic rendering is standard |
| audio | FIFO under/overrun; sample clock exposure | AUD-1..3 | Moderate (GBATEK silent on underrun) |
| cartridge | Save-type policy; EEPROM sizing; flash chip selection | CART-1..4 | Moderate; chip ID table is documented |
| API+consumer | Globals; refusal postconditions; LibFuturo fit | CP-8, CP-9, PROC-2..3 | Standard patterns; LibFuturo fit needs a real core |
| verification-infra | Provenance, rights, adapters, determinism gates | CP-5..7, EV-1..6 | Standard; owner gate on rights |

---

## Adversarial Claim Check

Status values: **confirmed**, **confirmed with caveat**, **stale**, **wrong**, **unverifiable**. Sources were fetched 2026-11-09.

| # | Claim (document) | Status | Evidence and source | Impact |
|---|------------------|--------|---------------------|--------|
| 1 | AGB CPU is ARM7TDMI at about 16.78 MHz with ARM and Thumb (DNA) | confirmed | GBATEK 3.06 "GBA Technical Data": ARM7TDMI 16.78 MHz; system clock 16*1024*1024 Hz. Arm TRM link resolves to "ARM7TDMI (Rev 3) TRM", ARM DDI 0029G, April 2001 `[PRIMARY]`. Nintendo's UK page says only "32-bit CPU with integrated memory" (no part name or clock). | Cite GBATEK/TRM, not Nintendo's page, for the CPU claim |
| 2 | GBA has a separate 8-bit CPU for GB/CGB mode (DNA, SEED) | confirmed with caveat | GBATEK: "CGB Mode Z80/8080-style 8bit CPU, 4.2MHz or 8.4MHz". Nintendo UK page: "Compatible with virtually all Game Boy, Game Boy Pocket and Game Boy Color games". GBATEK: GBA Micro lacks the cartridge switch and cannot detect 8-bit carts. | Compatibility mode is model-dependent; out of scope stays correct |
| 3 | 240x160 within 308 dots/line and 228 lines/frame "in the AGB manual" (DNA) | confirmed with caveat (numbers); attribution unverifiable | GBATEK: 240+68 = 308 dots, 1232 cycles/line, 228 lines, 280,896 cycles/frame. The AGB manual mirror returned HTTP 403, so the attribution is unchecked. GBATEK prints "ca. 59.737 Hz" and "13.620 kHz"; arithmetic gives 59.7275 Hz (= 262144/4389) and 13.618 kHz `[ARITH]`. Same typo in the 2.9b fork. | Derive constants from cycle counts; GBATEK has a numeric typo |
| 4 | Six display modes: three tile, three bitmap (DNA) | confirmed | GBATEK DISPCNT "BG Mode 0-5"; modes 3-5 bitmap; frame select for 4 and 5; Mode 5 is 160x128 | none |
| 5 | Four PSG channels plus two Direct Sound FIFOs; timers and DMA feed FIFOs; SOUNDBIAS/PWM affects output (DNA) | confirmed | GBATEK Sound chapters; SOUNDBIAS 9/8/7/6-bit resolution at 32.768-262.144 kHz | Sample rate is guest-variable (AUD-3) |
| 6 | Memory sizes: 16 KiB BIOS, 256 KiB EWRAM, 32 KiB IWRAM, 96 KiB VRAM, 1 KiB OAM, 1 KiB palette "per AGB manual" (DNA) | confirmed (values); attribution unverifiable | GBATEK Technical Data and Memory Map agree. AGB manual mirror 403. | none |
| 7 | Game Pak wait states and prefetch make access timing stateful (DNA) | confirmed, incomplete | GBATEK WAITCNT, prefetch buffer (8 halfwords), forced non-sequential at each 128K ROM block, "prefetch disable bug" | The DNA table understates this; see BUS-1/2 |
| 8 | Nintendo reports 81.51 M GBA hardware and 377.42 M software units (SEED, PROJECT) | confirmed with caveat | Nintendo IR table, as of 2026-09-30: 81.51 and 377.42, "worldwide consolidated sales in units life-to-date"; the page does not say whether the row includes SP/Micro | Distribution history only, as stated |
| 9 | mGBA suite repository identifies an MIT license (DNA) | confirmed with caveat | `LICENSE` MIT (c) 2015 Jeffrey Pfau. `include/mgba.h` has a BSD 2-clause header. No README in repo. Build needs devkitARM `gba_rules`, libgba, `gbafix`; no prebuilt ROM in the tree. Last push 2026-07-09. | Per-file licence review needed (CP-6) |
| 10 | jsmolka GBA tests identify an MIT license (DNA) | confirmed with caveat | `LICENSE` MIT (c) 2019 Julian Smolka. `lib/header.asm` embeds the 156-byte Nintendo logo. Prebuilt `.gba` files committed. `unsafe/README.md` documents tests that fail on hardware. Last push 2025-04-19. | Logo and unsafe tests need explicit handling |
| 11 | NanoBoyAdvance hardware tests are BSD-3-Clause (DNA) | confirmed, link stale | `LICENSE` BSD 3-Clause (c) 2021 fleroviux on both hosts. GitHub repo is archived (2026-06-21 "Close GitHub repository"); development moved to codeberg.org/nba-emu/hw-test. Makefiles link `-lmm -lgba`; prebuilt `.gba` committed. | Update the link; third-party code in binaries |
| 12 | GBATEK GBA-only reference at rust-console.github.io (DNA, PROJECT) | stale (version) | It is a Markdown fork "extracted from no$gba version 2.9b"; the current GBATEK is 3.06 (problemkaputt.de, mirrored by mgba-emu.github.io/gbatek). Sampled GBA topics (prefetch, unpredictable things, reference-point caution, flash tables) are textually equivalent; the 2.9b fork has fewer Sanyo/Macronix mentions. Not exhaustively diffed. | Cite GBATEK version per claim; use 3.06 |
| 13 | GBATEK is a community reference, not Nintendo-issued (DNA) | confirmed, plus errata | mGBA forum master post (2015, edited 2017) calls it "the de facto standard... It has errata and fails to cover certain issues"; lists errata (sprite semi-transparency vs brighten/darken, WIN wrapping, DMA3 SRAM-to-ROM lockup, JOY register direction, PWM rumble). 3.06 text itself has "?" markers and contradictory DMA/Game Pak statements. | Do not code from GBATEK prose alone |
| 14 | Tonc and gbadoc are community implementation-oriented references (DNA) | confirmed (links live); content not audited | Both URLs returned 200 | none |
| 15 | Nintendo GBA technical data page provides official dimensions, CPU class, display, colour and controls (DNA) | confirmed with caveat | Page is the UK "Legacy system" page: 240x160, 32,768 colours, 2xAA, 82x144.5x24.5 mm; CPU only "32-bit CPU with integrated memory"; controls not seen in extraction | Minor |
| 16 | AGB Programming Manual v1.1 mirror (DNA) | unverifiable | manuallib mirror returned HTTP 403; third-party mirror of a Nintendo document; rights unknown | Do not vendor; cite sections only if accessed legitimately |
| 17 | mGBA is an active multi-platform emulator and useful differential/debug implementation, not an oracle (DNA) | confirmed with caveat | Default branch pushed 2026-11-06; latest release 0.10.5 on 2025-03-09 (about 20 months old); licence MPL-2.0 (differs from the MIT suite). Not an oracle: see CP-5 | MPL-2.0 file-level copyleft if code is ever copied |
| 18 | Emulator agreement is not hardware truth (DNA) | confirmed | CP-5 evidence (mGBA suite #22/#24/#27, jsmolka `unsafe`, DMA latch disagreement) | Strengthens the rule |
| 19 | Header does not fully specify backup hardware (DNA) | confirmed | GBATEK: "Nintendo didn't include a backup-type entry in the ROM header"; detection by library ID strings; no$gba defaults 128K flash to 64K | CART-1 |
| 20 | EEPROM size can be inferred from DMA length (task premise, not in DNA) | confirmed as emulator technique, wrong as hardware fact | GBATEK: "There seems to be no autodection mechanism... hardcoded bus width"; mGBA 0.10.5 infers at runtime from transfer length | CART-2 |
| 21 | HLE BIOS is viable to study but not equivalent to the real BIOS (DNA) | confirmed | Cult-of-GBA BIOS: "high accuracy is a non-goal", incomplete sound SWIs and BIOS-read handling; NBA README calls an unofficial BIOS "less accurate"; jsmolka `bios.gba` requires real-BIOS opcode words | CP-3 |
| 22 | A once-per-frame renderer can miss raster effects (DNA) | confirmed | NBA hw-test ROMs for BG2X/Y, BG2PD, DISPCNT latch per scanline, status IRQ/DMA; GBATEK reference-point caution | CP-4 |
| 23 | LibFuturo is an experimental synchronous C API with no GBA profile (DNA) | confirmed | LibFuturo README lists `synthetic-v1`, Nesturbator NES and Muhtari 7800 profiles; coordination doc (2026-11-05) says the snapshot profile refuses battery-backed content | GBA specifics (cadence, samples/frame) are `[ARITH]` |
| 24 | Optional cartridge devices: RTC, solar, tilt/gyro, rumble (SEED) | confirmed | GBATEK GPIO, RTC, solar, tilt, gyro, rumble sections (Boktai RTC+solar; WarioWare Twisted rumble+gyro) | Deferral stays valid |
| 25 | SP and Micro differences are mostly host form-factor/presentation (SEED asks the question) | partly wrong | Some differences are machine-visible: 0x04000800 EWRAM wait behavior (Micro locks up at fastest), Micro lacks the cartridge switch for GB carts, DS GBA mode lacks the register; hardware tests differ on DS Lite/3DS | EV-5: profile must name the model |
| 26 | Playstead uses managed mGBA pinned to macOS 26/arm64 (PROJECT) | unverifiable | Local planning claim, not checked here | none |
| 27 | Keypad is a compact digital vocabulary (A/B, L/R, Start/Select, four directions) (DNA) | confirmed | GBATEK "4 Direction Keys, 6 Buttons" | none |
| 28 | Candidate suites need per-fixture review of build, expected outputs, BIOS (DNA) | confirmed, and understated | Findings in CP-5, CP-6, EV-1..3: visual-only results, mGBA-specific log channel, BIOS-dependent ROMs, toolchain-dependent expectations, third-party code in binaries | The review is a real work package, not a formality |

**Net result of the adversarial pass:** no claim in the bootstrap is flatly wrong on hardware facts. The material problems are (a) stale or archived source links (GBATEK 2.9b fork, GitHub hw-test), (b) under-stated fixture risk (logo bytes, mixed licences, linked libraries, tool-dependent expectations, mGBA-specific logging), (c) the seed's question about SP/Micro being host-only, which the sources answer as "not entirely", and (d) a timing constant typo in GBATEK that the bootstrap would inherit if copied.

---

## First-milestone critique

**Bootstrap direction under review:** one early vertical path combining a named profile, a source-built original/rights-reviewed diagnostic, deterministic headless execution, machine-readable results, raw video and audio capture, an independent installed consumer, and a visible legally cleared playable homebrew scenario.

**Verdict: too much for a first milestone.** `[INFERENCE]` supported by the findings above.

1. **It chains five independent critical paths.** (a) A working ARM7TDMI plus bus; (b) a PPU good enough for raw video; (c) timers + DMA + FIFO + PSG for raw audio; (d) a fixture-building toolchain with manifest and rights review; (e) a public API and package boundary. Each is a milestone-sized uncertainty in a codebase that does not exist yet.
2. **Two items are owner-gated.** Fixture rights (including the logo question, CP-6) and a "legally cleared playable homebrew" cannot complete on engineering effort alone. A milestone whose exit depends on both can stall without any technical failure.
3. **The diagnostic can be circular.** A cycle-aware diagnostic authored by the emulator's author encodes the author's understanding; its expected values are `self-derived`. It proves self-consistency and exercises the harness, not accuracy. External suites carry expected values of mixed provenance (CP-5) and are visual-only or emulator-logged (EV-1). Neither provides an independent oracle by itself.
4. **"Raw audio capture" in milestone one has nothing to capture.** Meaningful audio needs timers, DMA, FIFO and PSG with exact cadence (AUD-1..3). Stubbed audio would produce a capture format that gets redefined.
5. **"Installed consumer" implies an API boundary before the machine exists.** CP-8 says to write lifecycle and refusal semantics before exposing symbols, which is hard to do before the CPU/bus design is settled. A relocatable installed *runner* is cheap and still exposes packaging and path faults.
6. **A playable game needs most of the machine** (modes 0/1/2 or 3/4, OBJ, IRQs, timers, DMA, input, usually sound and a save device). Calling it early guarantees either overreach or a misleading toy.

### Recommended shape (spine first, visible output second)

| Milestone | Scope | Exit evidence | Why this order |
|-----------|-------|---------------|----------------|
| **M1: Evidence spine** | Profile doc (model, BIOS mode `direct-boot`, cartridge ROM-only, unmodeled list); ARM+Thumb interpreter on a master-clock bus API with per-access cost (CP-1/2 decisions made, values may start as tables); region decode with mirrors and width rules; deterministic headless runner with the full result taxonomy (missing, skipped, harness-failed, timed-out, cancelled, guest-failed, guest-stopped); fixture manifest with provenance, rights and BIOS fields and its linter; own tiny assembly fixtures (no logo, no linked libs) reporting through an IWRAM mailbox; third-party CPU suites (jsmolka arm/thumb/memory) run locally from pinned revisions after the logo/licence review; sanitizer and two-compiler/-O0/-O2 trace gates; negative controls; installed-runner relocation smoke test (CLI, JSON output, no C API yet); two-instance interleave test. Optional: a labeled VRAM memory snapshot (not a video claim). | Canonical verify command green; every required fixture present; skipped BIOS fixtures visible; manifest linter and negative controls demonstrably fail when broken | Settles the architecture decisions that are expensive to reverse (timing API, pipeline, determinism, provenance) and needs no owner rights decision beyond fixture vendoring policy |
| **M2: Time, interrupts, DMA, video timing** | Scheduler, IRQ, timers (cascade), HALT/STOP, DMA arbitration, DISPSTAT/VCOUNT edges; first per-line renderer for modes 3/4/0 with raw BGR555 capture; keypad input at explicit boundaries; own raster-effect fixture; first visible output | Own fixtures plus NBA/mGBA timing suites as differential signals with recorded provenance; first original homebrew diagnostic that draws and responds to input | Gives the owner something visible without pretending audio or full PPU |
| **M3: PPU completion** | Modes 0-5, OBJ with budget, affine, windows, blending, mosaic | Rendering fixtures with provenance; dispute registry entries | Largest rendering surface after timing is trustworthy |
| **M4: Audio** | FIFO, timers coupling, PSG, SOUNDBIAS, tagged raw sample stream | Cadence tests; capture format frozen only now | Needs M2 timers/DMA |
| **M5: Cartridge/save** | Save devices as explicit inputs; EEPROM/flash state machines; loader refusals; save-media export/import | Device matrix; length sweep; refusal postconditions | Parallelizable with M3/M4 |
| **M6: API + consumer** | Lifecycle/refusal doc, public C API, installed C consumer, LibFuturo adapter evaluation, state format decision | Installed relocated consumer; interleave and state round-trip tests | Boundary drawn after the machine exists |

**Alternative considered (thin vertical first):** Mode 3 plus KEYINPUT plus VBlank, with a tiny original homebrew visible in M1. It maximizes early visibility but forces the timing API decision (CP-1) and the BIOS/post-boot decision (CP-3) while the CPU is also new, and it cannot honestly claim raw audio. Choose it only if the owner needs a visible demo for Playstead within weeks; otherwise M2 delivers visibility with less rework risk.

**What to keep from the bootstrap:** the named profile, the fixture manifest, the distinct result states, negative controls, the "installed and relocated" idea (as a runner first), original-fixtures-first, and the rule that a visible playable scenario must not wait forever (it appears at the end of M2 as an original diagnostic, and a cleared homebrew game when the owner gate resolves).

**What would change this recommendation:** an owner decision that the first outcome is Playstead integration (favours the thin-vertical option and an earlier C API); evidence that a hardware-derived oracle (owner hardware captures) is available early (favours writing M1 fixtures against it); or a discovered need for BIOS-dependent behavior in the first game targeted (moves CP-3 earlier).

---

## Sources

- `[GBATEK]` GBATEK 3.06, https://problemkaputt.de/gbatek.htm (downloaded and searched); markdown forks https://mgba-emu.github.io/gbatek/ (3.06) and https://rust-console.github.io/gbatek-gbaonly/ (2.9b). Sections used: Technical Data, Memory Map, LCD Dimensions and Timings, OBJ cycle limits, Sound, Timers, DMA, System Control (WAITCNT, prefetch, HALTCNT), Cartridges (backup IDs, SRAM, EEPROM, Flash, GPIO), Unpredictable Things, ARM Memory Block Data Transfer.
- `[PRIMARY]` Arm, ARM7TDMI (Rev 3) Technical Reference Manual, ARM DDI 0029G (cover and release table read), https://documentation-service.arm.com/static/5e8e1323fd977155116a3129
- `[PRIMARY]` Nintendo UK GBA technical data, https://www.nintendo.com/en-gb/Support/Legacy-system/Technical-data-619479.html
- `[PRIMARY]` Nintendo IR hardware/software lifetime sales (as of 2026-09-30), https://www.nintendo.co.jp/ir/en/finance/hard_soft/index.html
- `[TEST-SRC]` mGBA suite, https://github.com/mgba-emu/suite (tree, `src/main.c`, Makefile, LICENSE, issues #8, #13, #17, #19-#27); jsmolka gba-tests, https://github.com/jsmolka/gba-tests (tree, README, `unsafe/README.md`, `lib/header.asm`, `bios/bios.asm`, `nes/nes.asm`, issues #2, #8, #12, #14); NanoBoyAdvance hw-test, https://codeberg.org/nba-emu/hw-test and archived https://github.com/nba-emu/hw-test (tree, `lib/source/test.c`, `dma/latch`, `ppu/*` READMEs, Makefile `LIBS`).
- `[EMU-SRC]` mGBA 0.10.5 `src/gba/dma.c`, `src/gba/savedata.c`, https://github.com/mgba-emu/mgba/tree/0.10.5 (licence MPL-2.0 per repository metadata).
- `[EMU-SRC]` NanoBoyAdvance README (accuracy claims, GPL-3.0, BIOS requirement), https://github.com/nba-emu/NanoBoyAdvance
- `[TEST-SRC]` Cult-of-GBA replacement BIOS, https://github.com/Cult-of-GBA/BIOS (MIT, README accuracy statement)
- `[ISSUE]` mGBA forum, "Game Boy Advance Documentation" master post, https://forums.mgba.io/showthread.php?tid=343 (GBATEK errata list)
- LibFuturo README, `docs/contract.md`, and `docs/coordination/machine-state-next-milestone-context-2026-11-05.md` under `~/projects/libfuturo` (local, read-only).
- Not reachable: AGB Programming Manual mirror (HTTP 403); Emulation General Wiki GBA tests page (HTTP 403).

---
*Pitfalls research for: deterministic GBA emulator (Gamebloke)*
*Researched: 2026-11-09*
