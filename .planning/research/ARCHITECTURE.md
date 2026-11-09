# Architecture Research

**Domain:** Deterministic, observable, accuracy-focused Game Boy Advance emulator core with a headless runner and thin host adapters
**Researched:** 2026-11-09
**Confidence:** MEDIUM overall. HIGH on frame/line geometry and on what the test ROMs measure (read from source this session). MEDIUM on the timing-model recommendation (reasoned from two emulators' source plus test sources, not from a Gamebloke prototype). LOW on a few edge-cycle alignments, listed under Unknowns.

## Evidence labels used in this file

| Tag | Meaning |
|-----|---------|
| `[REF]` | Fact stated by a hardware reference document (GBATEK mirror, NBA hw-docs). Both are community-maintained, not Nintendo-issued. The Nintendo AGB manual and ARM7TDMI TRM were not re-read this session; no claim below rests on them alone. |
| `[IMPL]` | Observation of what a specific emulator's current source does. Shows that implementation's behavior, not hardware truth. |
| `[TEST]` | Observation of what a test ROM's source asserts or how it reports. The expected values are hardware-derived by the test authors; provenance not independently verified. |
| `[INF]` | My inference or recommendation. Labeled so it is not mistaken for a source fact. |
| `[UNK]` | Not resolved by this research. |
| `[OWNER]` | Already decided in PROJECT.md / AGENTS.md. |

## Stale or imprecise bootstrap claims (checked 2026-11-09)

| Bootstrap claim | Result |
|-----------------|--------|
| 308 dots x 228 lines, visible 240x160 | CONFIRMED `[REF]` GBATEK "LCD Dimensions and Timings": 240 visible + 68 HBlank = 308 dots; 160 + 68 = 228 lines. |
| 4 cycles per dot | CONFIRMED `[REF]` GBATEK: "The drawing time for each dot is 4 CPU cycles." So 1232 cycles/line, 280,896 cycles/frame. `[IMPL]` mGBA `video.h` constants `VIDEO_HORIZONTAL_LENGTH 1232`, `VIDEO_TOTAL_LENGTH 280896`; NBA PPU chain 1007 + 225 = 1232. |
| Clock "about 16.78 MHz" | CONFIRMED and sharpened `[REF]`: 16 x 1024 x 1024 = 16,777,216 Hz. |
| Frame rate | GBATEK prints "ca. 59.737 Hz". Exact value from the cycle counts is 16,777,216 / 280,896 = 262144/4389 = 59.72750... Hz `[INF]` (arithmetic checked). Use the rational, not 59.737. |
| HBlank starts at 960 cycles | IMPRECISE. `[REF]` GBATEK: drawing is 960 cycles but "the H-Blank flag is '0' for a total of 1006 cycles". `[IMPL]` mGBA schedules HBlank at 1008 (`VIDEO_HDRAW_LENGTH`) with a 224 tail; NBA schedules at 1007 with a 225 tail. Three sources, three numbers; see Unknown U1. Never hard-code 960 as the flag edge. |
| NanoBoyAdvance / hw-test on GitHub | STALE. `[REF]` Both GitHub READMEs say development moved to Codeberg and the GitHub repos are archived read-only. Pin Codeberg commits. hw-test is BSD-3-Clause; NBA itself is GPL-3.0-or-later (reading it for behavior is fine; copying code into an MIT/other-licensed core is not decided and is an owner gate). |
| mGBA as test reference | `[REF]` mgba.io: latest release is 0.10.5 (2025-03-08), "last release before 0.11". Pin a commit, not `master`. |
| "A once-per-frame renderer can miss raster effects" | CONFIRMED and strengthened: hw-test's `ppu/ram-access-timing` sweeps every one of the 1232 cycles of a line (see Decision 2). |
| mGBA suite is a score | `[IMPL]` `passes`/`totalResults` count individual comparisons (up to 20 per test), not tests. A "score" mixes coverage. Report per-case, not a percentage. |

## Recommendations at a glance

| # | Decision | Recommendation |
|---|----------|----------------|
| 1 | Timing model | Per-access cycle-stamped bus plus a deterministic event scheduler, one `u64` master clock in 16,777,216 Hz cycles. Devices are evaluated lazily where pure (timers, DISPSTAT/VCOUNT) and scheduled where they have side effects. Events set state; a bus arbiter consumes it at defined slots. Not instruction-stepped, not per-cycle lockstep. |
| 2 | PPU | Scanline pixel renderer driven by scheduler events and register-latch points, plus a separate cycle-granular PPU timing and memory-contention model. Defer per-dot mid-scanline pixel fetch. Raw output is 240x160 BGR555 little-endian 16-bit words, no host conversion. |
| 3 | Audio | Core emits a raw fixed-cadence stereo stream at 262,144 Hz (one hold-sample every 64 cycles, exactly 4389 per frame) carrying the post-clip 10-bit mix. SOUNDBIAS rate and resolution are observed as trace events. No resampler, filter, or gain in the core. |
| 4 | API | Opaque instance, no globals, no callbacks. Primitive advance is a bounded cycle budget with stop conditions (`frame_ready`, budget, output full). Frame advance is a helper. Timestamped input events. Caller-owned A/V/trace buffers. Core copies ROM and BIOS on load. Backup device is declared by the caller, never inferred. |
| 5 | LibFuturo | Frame-level fit is workable. Concrete gaps: no 16-bit video format, single content span for two content kinds (BIOS + ROM), no sub-frame input, fixed audio rate field. Record, do not adopt. |
| 6 | Observability | Caller-owned fixed-record trace buffer with a category mask. Runner result schema keeps semantic status, machine time, video digest, audio digest separate. Result channels: original diagnostics use a fixed memory result block read by a side-effect-free peek; third-party suites use per-fixture adapters (mGBA debug port, SRAM log, screen). |
| 7 | Build order | CPU + flat bus + runner first; then scheduler/IRQ/timers + BIOS stub; then bus timing and prefetch; then DMA; then PPU timing, PPU pixels, PPU contention; then audio; then backup devices; then API hardening, state, installed consumer. Details below. |

## Standard Architecture

### System Overview

```
 HOST SIDE (owns files, clocks, devices, UI, durable writes, resampling, color conversion)
┌──────────────────────────────────────────────────────────────────────────────┐
│  headless runner        host adapters (small)       LibFuturo adapter (later) │
│  manifest + result      window / audio / keymap     maps frame advance        │
│  schema, digests                                                              │
└───────────────┬───────────────────────┬──────────────────────┬───────────────┘
                │  bytes in: ROM, BIOS, saves, state           │
                │  events in: timestamped keypad changes       │
                │  budgets in: cycles + stop mask              │
                ▼                                              ▲
┌──────────────────────────────────────────────────────────────┴───────────────┐
│  PUBLIC C-SHAPED API (opaque instance, status codes, caller-owned buffers)    │
│  create/destroy, load/unload, reset, queue_input, run, copy_frame, copy_audio,│
│  media size/copy/import, state size/save/load, peek, probe_*                  │
└───────────────┬──────────────────────────────────────────────────────────────┘
                │ no callbacks upward; outputs are copied into caller buffers
┌───────────────▼──────────────────────────────────────────────────────────────┐
│  CANONICAL MACHINE CORE (no host deps, no wall clock, no files, no globals)   │
│                                                                              │
│   ┌────────┐  access(addr,size,kind)   ┌──────────────────────────┐           │
│   │  CPU   │ ────────────────────────► │ BUS + ARBITER            │           │
│   │ARM/Thumb│ ◄─── data, cost ──────── │ region tables, WAITCNT,  │           │
│   └───┬────┘                           │ prefetch, open bus, DMA  │           │
│       │ advance(cycles)                │ ownership                │           │
│       ▼                                └──┬──────┬──────┬─────┬───┘           │
│   ┌───────────────┐   events (data)       │      │      │     │               │
│   │ SCHEDULER     │ ─────────────┐   ┌────▼──┐ ┌─▼────┐ │  ┌──▼───────────┐   │
│   │ u64 cycles    │              ├──►│ MEM   │ │ IO   │ │  │ CART         │   │
│   │ (time,prio,seq)│             │   │ewram, │ │ regs │ │  │ ROM view,    │   │
│   └──┬─────┬──────┘              │   │iwram, │ └─┬────┘ │  │ backup devs  │   │
│      │     │                     │   │vram,  │   │      │  └──────────────┘   │
│  ┌───▼─┐ ┌─▼───┐ ┌─────┐ ┌─────┐  │   │pram,  │ ┌─▼───┐ ┌▼─────┐              │
│  │ PPU │ │TIMER│ │ DMA │ │ APU │  │   │oam,   │ │ IRQ │ │KEYPAD│              │
│  │time+│ │x4   │ │x4   │ │PSG+ │  │   │bios   │ │ctrl │ │      │              │
│  │pix  │ │     │ │     │ │FIFO │  │   └───────┘ └─────┘ └──────┘              │
│  └─────┘ └─────┘ └─────┘ └─────┘  │                                           │
│                                   ▼                                           │
│   TRACE (record ring, caller-owned)     STATE (serialize/validate)            │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Component Boundaries

| Component | Responsibility | Communicates With | Must NOT |
|-----------|----------------|-------------------|----------|
| Scheduler | Single `u64` master cycle clock; ordered event queue keyed (timestamp, priority, sequence id); events are plain data (class enum + payload), serializable | Everything that schedules; CPU/bus call `advance(n)` | Hold closures or function pointers in queued state (breaks snapshots); read wall clock |
| CPU (ARM7TDMI) | ARM and Thumb decode/execute, pipeline-visible state (PC+8/+4, refill), exception entry, mode banking | Bus (every fetch/load/store), IRQ controller (sampled at instruction boundary), scheduler (via bus) | Know wait states or devices; count cycles itself |
| Bus + arbiter | Address decode, access classification (N/S/I/C), per-region width and wait-state tables, WAITCNT, Game Pak prefetch unit, open-bus and BIOS-read protection, DMA bus ownership, charge cost and call `scheduler.advance` | CPU, DMA, mem, IO, cart, PPU contention query | Run device logic inline; recurse into itself from event handlers |
| Mem | Backing arrays and mirroring for BIOS, EWRAM, IWRAM, PRAM, VRAM, OAM; deterministic power-on fill | Bus, PPU (reads), state | Own timing |
| IO | Register file dispatch, read masks, write side effects routed to devices | Bus, all devices | Hide side effects (every register write goes through a named handler) |
| IRQ controller | IE/IF/IME, line from sources, latency modeling, HALT wake | Devices (set IF), CPU (asserts line), scheduler | Execute handlers |
| Timers x4 | Prescaler, reload, cascade, overflow; lazily computed counter on read | IO, scheduler (overflow events), APU (FIFO pops), IRQ | Tick every cycle |
| DMA x4 | Channel state, latches, transfer units, start conditions (immediate, VBlank, HBlank, special/FIFO/video capture) | Bus arbiter (as bus master), PPU and APU (request signals), IRQ | Be entered from inside an event callback; it runs only at arbiter slots |
| PPU | (a) timing: line/dot events, VCOUNT, DISPSTAT flags, IRQ and DMA requests; (b) contention schedule for VRAM/PRAM/OAM; (c) pixel renderer producing raw BGR555 scanlines; (d) register latch points | Scheduler, IO, Mem, IRQ, DMA, bus (contention query) | Convert color, scale, or know the host |
| APU | PSG 1-4, Direct Sound A/B FIFOs, timer-driven pops, FIFO refill requests to DMA, mixer with SOUNDBIAS clip, raw sample emission | Timers, DMA, IO, scheduler | Resample, filter, apply gain/pan, or touch a device |
| Keypad | KEYINPUT level, KEYCNT interrupt condition, applies queued timestamped events at exact cycles | API input queue, IRQ, scheduler | Read physical devices |
| Cart | ROM image (copied, length-validated), header facts, caller-declared backup device (SRAM/FRAM, Flash 64K/128K, EEPROM 512B/8KiB), durable media array plus protocol state machine, generation counter | Bus, API (media copy/import), state | Infer device from title or filename; write files |
| BIOS | Region content and mode (see Decision 4 and slice S2): user-supplied 16 KiB image, or project-authored stub image | Bus, CPU | Ship Nintendo bytes |
| Trace | Category-masked fixed-size records into caller-owned storage | Instrumented sites | Call back into the host; allocate; format strings |
| State | Versioned serialization of the whole machine including scheduler queue, prefetch, DMA latches, open-bus latches | All components | Be added last (see Pattern 4) |
| Public API | The only exported surface; maps errors, enforces postconditions | Host side | Expose internal pointers |
| Headless runner | Loads manifest entry, drives the API, applies input script, captures raw A/V, writes result schema | Public API only (no core internals) | Share code paths the core does not expose publicly |

### Recommended Project Structure

Language and build system are decided elsewhere (STACK research). The layout below is language-neutral. `[INF]`

```
core/                  # canonical machine; zero host dependencies
  sched/ cpu/ bus/ mem/ io/ irq/ timer/ dma/ ppu/ apu/ keypad/ cart/ bios/ trace/ state/
api/                   # public headers + glue; the only thing consumers link against
runner/                # headless runner, result writer, digest helpers
adapters/              # small host adapters (window/audio/keymap), later LibFuturo adapter
fixtures/              # manifests + project-authored sources only; third-party binaries stay out until rights review
tools/                 # manifest validator, digest tool, negative-control drivers
tests/                 # component, integration, headless guest, installed consumer, negative controls
```

Rationale: `core/` cannot include anything from `runner/` or `adapters/` (enforce with a build-time dependency check); the runner uses `api/` exactly as an external consumer would, which is what makes the later installed-consumer check meaningful.

## Decision 1: Timing model

### Options

| Option | Description | Evidence it can reach | Cost |
|--------|-------------|----------------------|------|
| A. Instruction-stepped, per-access cycle sum | CPU executes one instruction, sums access costs, then a scheduler/event check runs at the instruction boundary. Device registers evaluated lazily from the current cycle. This is mGBA's shape. `[IMPL]` `ARMRun`: `while (cpu->cycles >= cpu->nextEvent) processEvents` before and after each step; events never fire mid-instruction. mGBA timers are lazy (`GBATimerUpdateRegister` derives the counter from `mTimingCurrentTime`). | Instruction cycle counts, timer-measured loops, DISPSTAT read edges (if flags are derived from the cycle at the access). Misses effects that need an event to fire between two accesses of one instruction (DMA stealing the bus, PPU contention that depends on the exact access cycle, mid-LDM IRQ line changes). mGBA compensates with `cyclesLate` arguments and hand-tuned constants (e.g. `GBARaiseIRQ(..., cyclesLate - 6)` which the source itself marks as unexplained). `[IMPL]` | Lowest. Fastest to first ROM running. Hard to retrofit exactness. |
| B. Per-access cycle-stamped bus + event scheduler | Every bus access charges its cost and calls `scheduler.advance(cost)`; due events fire in timestamp order inside the instruction at their exact timestamps. DMA is a bus master serviced at arbiter slots. Prefetch is a state machine advanced by the same clock. This is NBA's shape. `[IMPL]` NBA `Bus::Step(cycles)` always calls `scheduler.AddCycles(cycles)` and advances the prefetch countdown; `Scheduler::Step` pops every event with timestamp <= target and sets `timestamp_now` to each event's timestamp while its callback runs; binary min-heap keyed `(timestamp << 2) | priority`, capacity 64. | Everything A reaches, plus exact event-vs-access ordering. Matches how both hardware test suites measure (see table below). | Moderate. One extra indirection per access. Requires event handlers that never re-enter the bus. |
| C. Per-cycle lockstep of all components | Every component ticks every cycle. | Theoretical maximum. | Highest cost, slowest, large surface for bugs; no evidence in this research that any passing result needs it. NBA, the most accurate of the two sources read, is not this: `Bus::Idle()` approximates CPU/DMA parallelism with `parallel_internal_cpu_cycle_limit` and its own comment calls it an approximation. `[IMPL]` |

### Recommendation: Option B

`[INF]` Choose B. Reasons grounded in what the tests do:

- `[TEST]` hw-test `ppu/status-irq-dma` loops `cycle = 0..1231` and for each value runs an H-blank IRQ handler that delays `cycle` cycles then reads DISPSTAT, VCOUNT, or IF. It records the cycle at which each flag changes (expected values 144, 1151, 145, 1153, 146, 147 relative to its harness). Passing needs flag edges and IRQ delivery at the correct single cycle relative to CPU execution.
- `[TEST]` hw-test `ppu/ram-access-timing` repeats the same sweep for PRAM, VRAM, and OAM accesses and records extra stall cycles per cycle position, across sprite and background configurations (it lists cases such as "3x 4bpp RSD 16px (OAM)", "Locked H-blank (VRAM)").
- `[TEST]` `irq/irq-delay` expects exactly 92 / 112 / 120 timer ticks for an IRQ taken while executing from IWRAM / EWRAM / ROM. `dma/start-delay` expects 20 for an immediate DMA; `dma/force-nseq-access` expects 88; `dma/burst-into-tears` expects 41; `haltcnt` expects 12 and 4155; `timer/tick-before-reload` asserts a timer can tick in the cycle before its reload value is applied. All are measured with timer 0 at prescaler F/1, so timers must be exact to one cycle.
- `[TEST]` mGBA suite `timing.c` has 132 entries (131 measured plus calibration) covering `nop`, `ldr`/`str`/`ldm`/`stm` against stack, ROM, and OAM-to-ROM overflow, `mul`-family with 10 operand variants, branches, loops, BIOS calls, and 16 DMA cases, each over ROM variants labeled with P/N/S flags and WAITCNT settings (naming inferred from labels; the excerpt shows no WAITCNT writes). It needs per-access N/S classification, prefetch on and off, and wait-state tables. BIOS-call entries (Division, Sqrt, ArcTan, CpuSet) can only be matched with real BIOS code executing `[INF]`.

Why not A as the end state: A can pass many of the above (lazy flag/timer derivation covers reads), but the cases that depend on event placement between accesses (DMA start and hold relative to multi-cycle instructions; prefetch stop penalty; contention depending on access cycle) are exactly where mGBA needs hand-tuned fudge terms. Retrofitting B onto an A-shaped CPU means rewriting every memory access in the CPU, which is the largest module.

Why not C: no test source read requires it, and it is the most expensive path. Revisit only if a specific hw-test case cannot be matched under B (record as a research trigger).

Cheap path: slice 1 may implement B's API with trivial costs (every access 1 cycle) so the interface exists from day one and tests pin it. `[INF]`

### Guest-time unit

`[INF]` Unit: one master clock cycle = 1/16,777,216 s, `u64`. No half-cycle unit is needed (GabbaBoy uses half-dots to model GB double speed; the GBA has a single speed and both NBA and mGBA schedule in whole cycles). `[IMPL]`

Derived constants `[REF]` + arithmetic `[INF]`:

| Quantity | Value |
|----------|-------|
| Cycles per dot | 4 |
| Cycles per line | 1232 (960 visible + 272 HBlank nominal) |
| Cycles per frame | 280,896 = 2^6 x 3 x 7 x 11 x 19 |
| Frame rate | 262144/4389 Hz (exactly) |
| Audio period at SOUNDBIAS 0..3 | 512 / 256 / 128 / 64 cycles (32,768 / 65,536 / 131,072 / 262,144 Hz) `[REF]` |
| Audio samples per frame at 262,144 Hz | 4389 (integer) |
| Audio samples per frame at 32,768 Hz | 548.625 = 4389/8 (non-integer; repeats every 8 frames) |

### How the subsystems interact (single model)

| Concern | Model under B | Evidence label |
|---------|---------------|----------------|
| CPU access cost | Bus returns cost per access from tables indexed by region, width, N/S. ROM 16-bit bus: 32-bit access is two 16-bit fragments, the second always sequential. Cost = 1 + waitstates. Default ROM 5/5/8 for 8/16/32-bit; EWRAM 3/3/6; IWRAM/IO/BIOS 1; PRAM/VRAM 16-bit with +1 on 32-bit. | `[REF]` GBATEK memory table and WAITCNT text |
| Forced non-sequential | First access in each 128 KiB ROM block is non-sequential (e.g. `LDMIA [801fff8h]` is N at 8020000h). | `[REF]` GBATEK; `[TEST]` hw-test `bus/128kb-boundary` |
| Prefetch buffer | Stateful unit advanced by the master clock while the CPU is not using ROM: separate ARM (4 x 32-bit) and Thumb (8 x 16-bit) capacity in NBA; data accesses stop it; hit costs 1 cycle; stopping can add a 1-cycle penalty. WAITCNT bit 14 enables. Must be in the snapshot. | `[IMPL]` NBA `Bus::Prefetch/StopPrefetch/Step`. GBATEK mirror used here has only a stub section. `[UNK]` U2 |
| DMA bus ownership | CPU is paused while DMA runs; lower-priority channels pause for higher. Cost for n units: 2N + 2(n-1)S + xI, with 2I internal (4I if both ends in Game Pak). Sound and blanking DMA can be paused and CPU resumes. Per-channel 32-bit latch exists (16-bit DMAs duplicate the halfword; sources below 0x02000000 do not update the latch). | `[REF]` GBATEK ("2N+2(n-1)S+xI"; GBATEK itself marks parts of DMA lockup behavior "???"); `[TEST]` `dma/latch` |
| DMA start | Arbiter services requests at the next access boundary; start delay is observable (hw-test expects 20 timer ticks for immediate DMA reading a timer). | `[TEST]` |
| Timers | Counter computed lazily from clock and start time; overflow events scheduled absolute; reload copied on overflow or 0->1 start; register changes take one cycle to apply; cascade handled at overflow event. | `[REF]` NBA hw-docs ("Changes ... take one cycle to apply"); `[IMPL]` mGBA lazy update and absolute scheduling; `[TEST]` `tick-before-reload` |
| IRQ latency | Sources set IF at event time; CPU samples the line at instruction boundaries with a modeled fixed delay; HALT wake lands on an exact cycle. Absolute latency constants come from hw-test, not from prose. | `[TEST]` irq-delay 92/112/120; `[UNK]` U1 for alignment |
| HBlank/VBlank/VCOUNT | PPU events at scheduler timestamps. VBlank flag set on lines 160..226 (not 227); HBlank flag toggles on all 228 lines but no HBlank IRQ/DMA inside VBlank (`[REF]` GBATEK: "no H-Blank interrupts are generated within V-Blank period"); HBlank DMA fires on visible lines; video capture DMA lines 2..161 (`[IMPL]` both mGBA and NBA). | `[REF]` + `[IMPL]` |
| Audio FIFO DMA | Timer 0/1 overflow pops a FIFO sample; when FIFO falls to 16 bytes the APU raises a DMA request on DMA1/DMA2; transfer is 4 x 32-bit units, destination fixed. A higher-priority DMA can hold it off. | `[REF]` GBATEK "Sound DMA (FIFO Timing Mode)" |

### Rule that keeps B simple

`[INF]` Scheduler events never perform bus accesses and never call into the CPU. They only change device state, raise IRQ/DMA request lines, or schedule more events. Only the bus arbiter, between accesses and during internal cycles, starts a DMA unit or lets the CPU take an IRQ. This prevents recursion (event -> DMA -> bus -> advance -> event) and keeps the queue snapshot-safe.

### What bounds an `advance` call

`[INF]` Stop checks happen after each CPU instruction and each DMA unit. While the CPU is halted with nothing else runnable, time jumps to min(next event, budget end), so a halted machine lands exactly on the budget. A budget can overshoot only by one instruction or DMA unit; the profile must declare and test that maximum (measure; do not guess). GabbaBoy's "preflight the whole instruction cost" is not available here because cost depends on prefetch and DMA state, so Gamebloke reports `consumed_cycles` exactly and permits bounded overshoot. `[INF]`

### Boot phase is an observable choice

`[IMPL]` mGBA with no full BIOS starts VCOUNT at 0x7E with a 120-cycle offset to the first HBlank event; with a full BIOS it starts at VCOUNT 0 and offset 1008. So "machine time zero" and the initial raster phase depend on BIOS mode. The profile must state them; they are part of the claim. `[INF]`

## Decision 2: PPU

### Options

| Option | What it can reproduce | What it cannot |
|--------|----------------------|----------------|
| Frame renderer | Static scenes | Raster effects; unacceptable for this project |
| Scanline renderer at one event per line | Per-scanline register changes (HBlank DMA scroll/palette tricks), windows, most games. `[IMPL]` mGBA calls `drawScanline` at the start of HBlank for each visible line and `finishFrame` at VBlank start. NBA composes BG/window/merge at the start of each visible line (`BeginHDrawVDraw`) and runs sprite drawing as a separate 1232-cycle loop. | Mid-scanline register effects; VRAM/OAM/PRAM access contention (CPU stalls) |
| Per-dot / per-fetch renderer | Mid-scanline effects, exact contention | Cost and bug surface; no test in this research requires pixel-accurate mid-line output |

### Recommendation

`[INF]` Split the PPU into two cooperating parts behind one component boundary:

1. **Timing and contention model (cycle granular, required early).** The hardware's memory-access pattern is regular and documented `[REF]` NBA hw-docs: backgrounds have a wait period (usually 31 cycles, `31 - 4*(HOFS mod 8)` with sub-tile scroll) then fetch roughly one pixel per background per 4 cycles, "cycle #1005 appears to be the last cycle that may fetch data"; sprite rendering starts at cycle 40 of the previous line and runs until HBlank of the previous line when DISPCNT bit 5 = 1, or cycle 40 of the current line when 0; "Both stages only access OAM/VRAM every two cycles"; compositing begins after a 46-cycle wait, one output pixel per 4 cycles, and the doc's own figures for the last pixel disagree (1006 vs 46 + 4x240 = 1008). The contention model answers one question for the bus: "is VRAM/PRAM/OAM being read by the PPU at cycle X of this line, so the CPU access gains a cycle?" `[REF]` GBATEK: "Plus 1 cycle if GBA accesses video memory at the same time." This is a lookup derived from DISPCNT, enabled layers and sprite state, not a pixel pipeline. Verified by `ppu/ram-access-timing` `[TEST]`.
2. **Pixel renderer (scanline, bit-exact for what the profile claims).** Runs from a scheduler event and consumes an explicit snapshot of "latched" registers. Latch points are first-class data (which register is latched at which cycle of which line) so they can be tuned against `ppu/dispcnt-latch`, `bgx`, `bgpd`, `sprite-hmosaic` `[TEST]` without changing interfaces. If a later profile needs mid-line pixel effects, only this renderer is replaced; bus, scheduler, and API do not change.

### Mid-scanline effects and access restrictions

`[REF]` GBATEK: VRAM, OAM, PRAM are accessible during HBlank/VBlank (or forced blank); OAM in HBlank only if DISPCNT "H-Blank Interval Free" is set; the CPU can access them anytime but gains one wait cycle when the display controller is reading. `[TEST]` `ppu/vram-mirror`: VRAM accesses above 0x06010000 (text modes) or 0x06014000 (bitmap modes) go through the OBJ engine, so in bitmap modes 0x06018000-0x0601BFFF writes are discarded; the README notes it passes on GBA/GBA SP but not on DS Lite or 3DS GBA mode. That is a mirror-and-routing rule the memory component needs, not a timing rule.

`[INF]` What to claim: per-scanline latching of the registers hw-test covers and the contention model are in scope for the first PPU profile; true mid-scanline pixel-position register writes are out of scope until a named fixture requires them.

### Raw output format

| Option | Verdict |
|--------|---------|
| Native 15-bit color as 16-bit little-endian words, 240x160, stride caller-chosen, bit 15 = 0, bit layout identical to palette RAM (R in bits 0-4, G 5-9, B 10-14) | Recommended. Lossless relative to what the compositor produces; zero conversion; digests are exact. GabbaBoy's `gbb_copy_frame_rgb555` precedent: "no colour correction", explicit little-endian byte pairs for digests. `[IMPL]` (GabbaBoy header) |
| RGB24/RGBA from the core | Reject. Hides the 5-bit values, forces a color-response policy into the core, and is a host concern. |
| Indexed output | Not applicable: modes 3 and 5 are direct color and blending creates new colors. |

`[INF]` Frame completion: the frame is copyable when the PPU reaches VCOUNT = 160 (VBlank start); expose `frame_generation` and the completion cycle stamp. Forced-blank output (white) and the undocumented green-swap register (`ppu/greenswap` `[TEST]`) are in the raw format, not the host's.

## Decision 3: Audio

### What the hardware does `[REF]` GBATEK SOUNDBIAS / "PWM"

- PSG channels 1-4 are generated internally at 262.144 kHz; Direct Sound is driven by timer overflows; "the final sound output is resampled to a rate of 32.768kHz, at 9bit depth" by default.
- SOUNDBIAS bits 14-15 select 9-bit/32.768 kHz (default), 8-bit/65.536 kHz, 7-bit/131.072 kHz, 6-bit/262.144 kHz. Bits 1-9 are the bias (default 0x100).
- Each FIFO can span the full +/-0x200 range; each PSG one quarter; sum plus bias is clipped to 0..0x3FF. The 1-bit PWM stage runs at 16.78 MHz: at the default rate each sample is 512 output bits (N low, 512-N high). `[UNK]` U4: how the hardware reduces 262.144 kHz PSG to the 32.768 kHz output (decimate, average, hold) is not specified by the reference text read here.

### Options

| Option | Pros | Cons |
|--------|------|------|
| Variable-cadence stream at the SOUNDBIAS period with rate tags | Closest to the hardware output stage | Cadence changes mid-run; per-frame sample counts are non-integer at the default rate (548.625); every consumer must handle rate segments |
| Fixed 262,144 Hz stream: one hold-sample per 64 cycles | Constant cadence; exactly 4389 stereo samples per frame, no remainder; every SOUNDBIAS period (512/256/128/64) is a multiple of 64 cycles so a lower-rate output is represented losslessly by holding; the rate changes become trace events | 8x more samples than needed at the default rate (about 17.5 KiB per frame as s16 stereo), trivial CPU cost; phase of a mid-period rate change is an assumption (U4) |
| Fixed 32,768 Hz stream | Smallest | Cannot carry higher SOUNDBIAS rates without a second format; 548.625/frame needs an accumulator (LibFuturo's Muhtari profile already does this with 800/801 via an 88/13,125 accumulator `[IMPL]` libfuturo docs) |
| Host-rate (48 kHz) output with an in-core resampler | Easy playback | Violates the portability constraint and destroys raw-output evidence. mGBA does resampling in-core (bundled blip-buf, LGPL per LibFuturo research); do not copy that boundary. |

### Recommendation

`[INF]` Fixed 262,144 Hz hold stream, stereo, each sample the post-clip 10-bit unsigned value per channel (0..0x3FF) before PWM quantization, stored as 16-bit unsigned or a documented signed mapping. The mixer recomputes only when an input changes (PSG duty edge, FIFO pop, register write), so cost is per change, not per sample. Sample k represents machine cycles [64k, 64k+64). Audio samples are produced from `advance` into the caller's buffer; capacity is checked before an operation can add samples (stop with `OUTPUT_FULL` before mutating, GabbaBoy precedent `[IMPL]`). Everything past this stream (low-pass, resample to 48 kHz, gain, headphone stereo) is host or adapter work. The audio component also never talks to a device.

Verification: an original diagnostic that writes known FIFO words and timer reload values must yield an exact, independently computable sample vector (analogous to LibFuturo's Muhtari PCM arithmetic vector). `[INF]`

## Decision 4: Core/host boundary and public API

### Principles

- `[OWNER]` Canonical core has no host files, wall clock, UI, A/V devices, or save-file policy.
- `[INF]` No global mutable state. Immutable tables are fine; every wait-state table, event queue, and latch is per instance. A context-shaped API does not prove independence; add a test that runs two instances interleaved and compares to two isolated runs. (EMULATOR-DNA lesson.)
- `[INF]` No callbacks from the core during `run`. Output goes to caller buffers. This removes reentrancy and ABI hazards and matches LibFuturo's "no public host reentry during advancement" constraint.
- `[INF]` Allocation only at create and load; `run`, `reset`, `peek`, `copy_*` do not allocate (GabbaBoy precedent). Optional allocator injection at create (LibFuturo `lf_create_options` precedent) to keep adapters trivial.

### API sketch (names are placeholders; shape follows GabbaBoy and LibFuturo conventions)

```c
typedef struct gbl_instance gbl_instance;

/* Profile is explicit: model + BIOS mode + feature set. No default-by-title. */
gbl_status gbl_create(const gbl_profile_id *profile, const gbl_create_options *opt, gbl_instance **out);
gbl_status gbl_destroy(gbl_instance **owned);

/* Content admission: bytes are COPIED on success; caller may free immediately.
   A refused load leaves the active session (or the unloaded state) unchanged. */
gbl_status gbl_probe_cartridge(const gbl_profile_id *p, const uint8_t *rom, size_t n, gbl_cart_info *out); /* no instance; hints only */
gbl_status gbl_load_bios(gbl_instance *i, const uint8_t *bytes, size_t n);          /* n must equal 16384; reports digest, never "approves" */
gbl_status gbl_load_cartridge(gbl_instance *i, const gbl_cart_request *req);        /* req: rom span, declared backup device id, size */
gbl_status gbl_unload_cartridge(gbl_instance *i);
gbl_status gbl_reset(gbl_instance *i, gbl_reset_kind kind);                         /* documented time + queue + RAM effects; retains ROM, BIOS, backup media */

/* Input: timestamped logical key changes, absolute machine cycles, nondecreasing,
   not earlier than now. All-or-nothing admission. */
gbl_status gbl_queue_input(gbl_instance *i, const gbl_input_event *ev, size_t n);
gbl_status gbl_get_clock(const gbl_instance *i, gbl_clock *out);                    /* now_cycles, last_queued_cycles, frame_generation */

/* Advance: budget in master cycles + stop mask. Optional caller-owned outputs. */
gbl_status gbl_run(gbl_instance *i, const gbl_run_request *req, gbl_run_result *out);
gbl_status gbl_copy_frame(const gbl_instance *i, uint16_t *px, size_t cap_bytes, size_t stride_bytes, gbl_frame_info *out);
/* audio samples are written by gbl_run into req->audio (capacity in sample frames) */

/* Persistent media (durable array only; device protocol state is not part of it) */
gbl_status gbl_media_info(const gbl_instance *i, gbl_media_info *out);              /* kind, exact extent, identity string, generation */
gbl_status gbl_copy_media(const gbl_instance *i, uint8_t *dst, size_t cap);
gbl_status gbl_import_media(gbl_instance *i, const uint8_t *src, size_t n);         /* exact extent; allowed after load/reset before first run */

/* State: opaque, versioned, identity-bound; validated fully before any mutation. */
gbl_status gbl_state_size(const gbl_instance *i, size_t *out);
gbl_status gbl_save_state(const gbl_instance *i, uint8_t *dst, size_t cap);
gbl_status gbl_load_state(gbl_instance *i, const uint8_t *src, size_t n);

/* Side-effect-free debug read (no open-bus latch update, no cycle cost). */
gbl_status gbl_peek(const gbl_instance *i, uint32_t addr, uint8_t *dst, size_t n);
```

```c
typedef struct {
  uint64_t budget_cycles;          /* run until now >= start + budget, bounded overshoot */
  uint32_t stop_mask;              /* FRAME_READY | BUDGET | OUTPUT_FULL always implied */
  gbl_audio_frame *audio;  size_t audio_capacity_frames;   /* may be NULL with 0 */
  gbl_trace_record *trace; size_t trace_capacity;          /* may be NULL with 0 */
  uint32_t trace_categories;
} gbl_run_request;

typedef struct {
  uint64_t consumed_cycles;        /* exact */
  uint64_t now_cycles;             /* absolute machine time after the call */
  gbl_stop_reason reason;          /* BUDGET, FRAME_READY, OUTPUT_FULL, NO_PROGRESS, GUEST_STOP (see below) */
  uint64_t frame_generation;       /* increments at VBlank start; copy with gbl_copy_frame */
  size_t audio_frames_written, trace_written;
} gbl_run_result;
```

### Answers to the specific questions

| Question | Recommendation |
|----------|----------------|
| Instance lifetime | Host owns the handle; create/destroy; one thread at a time per instance; separate instances share nothing. `[INF]` |
| BIOS and cartridge load/unload | BIOS is optional input, loaded before power-on/reset takes effect, copied, length must be exactly 16,384; the core reports its SHA-256 so the host/manifest can bind identity (a matching hash proves identity, not rights). Cartridge: ROM copied (up to 32 MiB window `[REF]` GBATEK memory map), header facts validated for truncation and size, backup device passed by the caller. Load while running is a refusal or a documented replace; either way a refused load leaves the active session unchanged (LibFuturo LC-LOAD-01 precedent). |
| Backup device | Never inferred from filename or title `[OWNER]`. `probe` may report hints from the ID strings Nintendo's tools embed (`EEPROM_Vnnn`, `SRAM_Vnnn`, `FLASH_Vnnn`, `FLASH512_Vnnn`, `FLASH1M_Vnnn` `[REF]` GBATEK) but they are advisory, and EEPROM size is ambiguous between 512 B and 8 KiB so the caller must declare it. |
| Bounded advancement unit | Cycles, with a frame stop condition. `[INF]` Cycles are a superset: a frame helper is "run until `FRAME_READY`", and sub-frame input fixtures need cycles. Frames-only would force sub-frame fixtures to be impossible and would hide timing the product claims. |
| Input sampling | Timestamped events applied by the scheduler at the exact cycle. Convenience: an event at `now` equals "sampled at advance start" (what LibFuturo offers). KEYINPUT is level state read whenever the guest polls; KEYCNT can raise an IRQ; the sample point is therefore the event timestamp, stated in the result. `[REF]` GBATEK suggests reading KEYINPUT once per frame, which is a software convention, not hardware. |
| A/V buffers | Caller-owned. Audio goes into the `run` request buffer; video is copied after `FRAME_READY` via `copy_frame` with generation and completion stamp. A run that spans several frames still yields only the latest frame; capture every frame by stopping at `FRAME_READY`. Checked arithmetic and aliasing rules before any write. |
| Reset | `reset(kind)`: power-on (deterministic RAM fill policy, post-boot or BIOS-entry state per profile, machine time 0, input queue cleared) vs soft reset if a profile needs it. Retains ROM, BIOS, and backup media. Documents each effect. |
| Errors and refusal postconditions | Every operation lists status, instance state, and caller-buffer postconditions. Preflight rejections (bad pointer, overflow, capacity) do not advance time or write buffers. `run` may return partial progress only with an explicit reason and exact `consumed_cycles`. Distinct statuses: invalid argument, unsupported profile/device, ROM truncated/too large/size mismatch, BIOS wrong length, input queue full or invalid event, buffer too small, state identity mismatch / corrupt / unsupported version, out of memory. |
| Save media | `media_info` + copy + import over the durable array only; command state machines (Flash/EEPROM protocol) reset to idle on import. Generation increments when bytes change (GabbaBoy: once per changed byte; LibFuturo: guest generation token, not a host durability receipt). The host owns files and acknowledgments. Import allowed after load/reset and before the first run, matching LibFuturo's rule. `[INF]` |
| Save states | Header: magic, version, profile id, core build id, ROM SHA-256 and extent, BIOS mode and digest, backup device, payload length, CRC. Excludes ROM/BIOS bytes. Includes scheduler queue (as data), prefetch, DMA latches, open-bus latches, input queue, audio mixer state. Decode into a candidate, validate completely, then swap (LibFuturo LC-SNAPSHOT-04: rejected restore preserves the machine). A restore of in-flight DMA/timer/FIFO state is a mandatory test. Estimated size is a few hundred KiB (VRAM 96 KiB + EWRAM 256 KiB + IWRAM 32 KiB + PRAM 1 KiB + OAM 1 KiB per the AGB manual figures in the bootstrap, plus devices). `[INF]` |
| GUEST_STOP | The GBA has no architectural halt-and-catch-fire. Use it only for explicitly unsupported behavior the core refuses to guess (e.g., an unmodeled peripheral), so unsupported never becomes silent wrong output. `[INF]` |

### BIOS and boot policy (architecture constraint)

`[INF]` IRQ delivery goes through the BIOS: the CPU takes the IRQ at vector 0x18 in BIOS space and the BIOS stub pushes registers and jumps through the pointer at 0x03FFFFFC (mirror of 0x03007FFC); GBATEK prints this stub's disassembly `[REF]`. SWI also vectors into BIOS. So the BIOS region cannot be left as "nothing" even for the first IRQ test. Three paths, all behind the BIOS component:

| Mode | What it is | Can claim |
|------|-----------|-----------|
| User-supplied image | 16 KiB bytes loaded by the caller; read-protection when PC is outside BIOS (reads return the last BIOS fetch) needs a latch | Real BIOS timing for IRQ/SWI; the only mode that can pass BIOS-math and BIOS-call timing cases (inference from `timing.c` and `bios-math.c`) |
| Project-authored stub image | Original ARM code assembled to the BIOS region: vector table, IRQ entry stub behavior, SWI dispatch implemented as guest code where practical | Deterministic, rights-clean, runs through the normal CPU/bus so IRQ and SWI timing is guest-visible; not equivalent to Nintendo's BIOS and must not be described as such |
| Pure HLE interception | Host-side SWI functions bypass guest code | Fast; not cycle-accurate; label separately |

Recommendation: stub image as the default no-Nintendo-bytes path plus user-supplied image as an explicit second profile. Write the stub from behavior (registers, vectors, documented stub semantics), not by transcribing Nintendo's code; the rights review of reproducing the GBATEK listing is an owner/legal gate. Per-fixture results must report `skipped: requires user-supplied BIOS` distinctly from `failed`. `[INF]`

## Decision 5: LibFuturo fit (observations only, not adoption)

Source: `~/projects/libfuturo/include/libfuturo/libfuturo.h`, `docs/contract.md`, `docs/muhtari-profile.md`, `.planning/PROJECT.md`, `.planning/PRODUCT-ROADMAP.md`, read 2026-11-09. The contract is experimental and changing (PROJECT.md records v1.9 browser-host work in progress); recheck before any decision. `[REF]`/`[IMPL]` (read from files).

| Topic | LibFuturo today | GBA situation | Fit |
|-------|-----------------|---------------|-----|
| Advance unit | `lf_run_request.work_units` in the profile's unit; `LF_TIME_FRAME` = one completed profile frame-advance interval; first interval may be shorter. | Frame = 280,896 cycles; exact cadence 262144/4389 Hz fits `frame_rate_numerator/denominator`. A frame boundary can be defined at VBlank start. | Fits for frame-level use. No sub-frame advance. |
| Finer clock | `cumulative_core_ticks` + `core_tick_time_unit`; enum has `LF_TIME_NES_HALF_MASTER_TICK` as precedent; Muhtari reports no ticks. | Master cycles at 16,777,216 Hz are natural core ticks. | Needs a new `lf_time_unit` value (e.g., GBA master cycle). Small additive change. |
| Startup interval | Muhtari declares a partial first interval (8,190 ticks, 54 samples). | Boot phase depends on BIOS mode (mGBA no-BIOS starts at VCOUNT 0x7E +120). | Fits: the profile can declare the startup rule. |
| Work bounds / overshoot | `max_work_units`, `atomic_quantum_units`, `max_overshoot_units`, `actual_units`. | Frame-granular stop means 0 overshoot in frames; cycle overshoot is internal. | Fits. |
| Input | `lf_input.buttons` 32-bit mask; `LF_INPUT_AT_ADVANCE_START` is the only sampling mode. | 10 keys fit bits 0-9 in KEYINPUT order (A, B, Select, Start, Right, Left, Up, Down, R, L `[REF]`). Games may poll mid-frame; KEYCNT can raise IRQs. | Fits at frame boundaries. Mismatch for any claim needing timed events inside a frame. |
| Video format | `LF_VIDEO_GRAY8`, `RGB24`, `INDEX8`; `video_bytes_per_pixel` exists in the profile. | Raw 240x160 BGR555, 2 bytes per pixel, constant size. | Mismatch: needs a new enum value (e.g., 16-bit 15-bit-color word) and stride/alignment rules for 16-bit pixels. INDEX8 precedent shows raw, no conversion is accepted. |
| Audio | `LF_AUDIO_S16_NATIVE`, interleaved, rate as numerator/denominator, `max_audio_samples_per_call`, per-call counts may vary (Muhtari 800/801). | Fixed 262,144 Hz stereo -> 4389 per frame (integer). A 32,768 Hz stream would be 4389/8 per frame (548 or 549, 8-frame period). | Fits only if the core exposes one fixed rate; SOUNDBIAS-variable cadence would not fit the profile's single rate field. Whether LibFuturo hosts accept a 262,144 Hz stream is `[UNK]`; the macOS host's resampling behavior was not read. |
| Content admission | `lf_load_request` has one `content_data` span and a `configuration_id` string. Nesturbator and Muhtari profiles are "no BIOS". | Two content kinds: ROM (up to 32 MiB) and optional 16 KiB BIOS. BIOS is rights-bearing and user-supplied. | Mismatch for BIOS mode. A no-BIOS/stub-BIOS profile fits one span. A user-BIOS profile needs a second admission slot or a distinct call. |
| Configuration identity | `configuration_id` NULL = default; `effective_configuration_id` returned. | Backup device, BIOS mode, profile feature set are all configuration facts. | Fits: encode as named configurations (e.g., stub BIOS + SRAM 32 KiB). Many combinations, so keep the set small and explicit. |
| Save media | One `lf_copy_media` span, `identity` string, `media_extent_bytes` 0 = content-dependent, `guest_generation`; import after load and before first positive advance. | SRAM 32 KiB, Flash 64/128 KiB, EEPROM 512 B / 8 KiB. Device not in the header; EEPROM size ambiguous. | Fits if the device is declared by configuration. RTC (deferred) would add a second block. |
| State | Opaque snapshot, exact-build identity, qualified per-profile exceptions, rejected restore preserves machine. | Machine state is a few hundred KiB; must include in-flight scheduler events. | Fits mechanically; qualification is per profile and per build, so a GBA profile starts with no snapshot claim. |
| Reset | No `lf_reset`; reload or recreate. | GBA soft reset semantics and power cycle. | Minor mismatch; recreate/reload is acceptable for a first profile. |
| Trace / observability | Mandatory tracing excluded from the hot path; none in the header. | Gamebloke's evidence depends on trace, peek, and result blocks. | Not a mismatch: observability lives on Gamebloke's native API and runner; the adapter exposes none. LibFuturo-driven runs would prove less. |
| Allocator | `lf_create_options` allocate/deallocate callbacks. | Core allocates at create/load only. | Fits; allocator injection should be optional in the native API. |
| Concurrency | Host serializes; no concurrent use. | Same. | Fits. |

## Decision 6: Observability

### Trace and event hooks

`[INF]` A caller-owned array of fixed-size records, enabled by a category mask in each `run` request. No callbacks, no strings, no allocation in the core. If the buffer cannot hold a whole operation's records, stop with `OUTPUT_FULL` before mutating state so records are never silently dropped (GabbaBoy `gbb_run_ex` precedent `[IMPL]`).

| Category | Record content (all carry machine cycle) |
|----------|------------------------------------------|
| INSTR | pc, opcode, ARM/Thumb flag, cpsr summary, r0-r15 snapshot optional |
| BUS | address, size, kind (N/S/I/C), region, cost charged, prefetch hit/stop, DMA vs CPU master |
| IRQ | source set/cleared, IF/IE/IME effective, taken, halt entered/woken |
| DMA | channel start/unit/pause/resume/end, latch value, trigger source |
| TIMER | start/stop, overflow, cascade, reload |
| PPU | line start, HBlank, VBlank, VCOUNT match, flag edges, contention stall applied, latch events |
| APU | FIFO pop/refill request/underrun, SOUNDBIAS change, PSG trigger |
| CART | backup command, write committed to durable array, generation change |
| BIOS | SWI number, vector entry |

The instruction-level categories dominate cost; keep each disabled category to a mask test at the instrumented site. Measure overhead later against a named workload (AGENTS.md: correctness first).

### Headless runner result schema

`[INF]` One JSON object per run. Fields are separate so one cannot masquerade as another.

```
schema_version
fixture:   { id, manifest_digest, source_rev, rom_sha256, rom_size, license_ref }   // from the fixture manifest
machine:   { profile_id, core_build_id, bios_mode, bios_sha256|null, backup_device, config_id }
input:     { script_digest, event_count }
status:    one of PASS | GUEST_FAIL | HARNESS_FAIL | TIMEOUT | CANCELLED | MISSING | SKIPPED | REFUSED
           (+ reason string and, for SKIPPED, the unmet requirement e.g. "needs user-supplied BIOS")
semantic:  { channel, verdict, passed, total, detail }      // what the guest itself reported
time:      { cycles_total, frames_completed, stop_reason, budget_cycles }
video:     { format: "bgr555-le", width: 240, height: 160, frames_digested, digest_algo, digest(s) }
audio:     { format, rate_hz: 262144, channels: 2, frames_total, digest_algo, digest }
trace:     { enabled_categories, digest|null }
limits:    [ "what this result does not prove" ]
```

`TIMEOUT` means the machine-time budget ran out before the semantic channel reported; `HARNESS_FAIL` means the runner or fixture setup failed (I/O, allocation, refused load); `GUEST_FAIL` means the guest reported failure; `MISSING` and `SKIPPED` are explicit rows, never absent (AGENTS.md). Each gate needs a negative control that must produce a non-PASS status for a known reason.

### How fixtures report results (read from source this session)

| Fixture | Channel | Headless handling |
|---------|---------|-------------------|
| Original diagnostics (project-authored) | `[INF]` A fixed memory result block (magic, status, check count, first failing id) in IWRAM/EWRAM, read through `gbl_peek` at stop; no hardware-visible debug device | Cleanest: no register that real hardware lacks |
| mGBA suite | `[TEST]` `main.c`: `debugprintf("BEGIN: %s")` and `debugprintf("END: %i/%i")` through `mgba_printf`, active only if `mgba_open()` succeeded; per-line pass/fail drawn on screen; a `savprintf` log is appended to SRAM (to 0x8000 bytes) and mirrored to `mgba_printf`. Debug registers per community crates: enable 0x04FFF780 (write 0xC0DE, read back 0x1DEA), string buffer 0x04FFF600 (256 bytes), flags/send 0x04FFF700 (flag 0x0100 sends). `[IMPL]` mGBA `io.c` confirms `gba->debug = value == 0xC0DE`, read returns 0x1DEA when enabled. | Either implement the mGBA debug port as an optional, profile-flagged non-hardware device (a guest can detect it, so default off for hardware-fidelity runs), or read the SRAM log via `copy_media` with the fixture declared as SRAM 32 KiB. Counts are comparisons, not tests. |
| NBA hw-test | `[TEST]` `lib/source/test.c`: `printf` to the libgba console; `PASS n` / `FAIL n expected: ...`, then "congratulations!" or `pass: / total:`. Expected values are timer tick counts. No memory-mapped result. | Needs a per-fixture adapter: read the text from VRAM tile map (inference: libgba console draws into the BG map) or compare the raw video digest to a recorded expectation with a semantic note. The ROMs and `expected.png` files are committed in the repo; rights and provenance of built binaries and images are `[UNK]` (needs per-fixture review). |
| jsmolka gba-tests | `[REF]` README: all-pass message or the number of the first failed test on screen, needs BG mode 4; license MIT. Register-based reporting not confirmed. | Screen-based adapter (raw frame vs recorded semantic expectation); inspect source before relying on any register convention. |

## Data Flow

```
Host bytes (ROM, BIOS, saves, state) ──► api: validate lengths/extents ──► core copy ──► load OK or refusal
Host events (key changes @ cycle) ─────► input queue ──► keypad applies at exact cycle ──► IF/KEYINPUT
Host run(budget, stop mask) ───────────► loop:
    CPU step ─► bus.access(addr,size,kind) ─► cost ─► scheduler.advance(cost)
        │                                              │
        │                                              └─► due events fire in (time,prio,seq) order:
        │                                                    PPU line/flag, timer overflow, FIFO pop,
        │                                                    sample boundary, IRQ line, DMA request
        └─ at arbiter slots: DMA unit runs (CPU paused) | IRQ taken at instruction boundary
    stop check after each instruction / DMA unit ─► reason, consumed_cycles
Core outputs (copied to caller buffers) ◄── frame (BGR555) | audio stream (262,144 Hz) | trace | media | state | run result
Runner ─► digests (video, audio separately) + semantic channel + machine time ─► result JSON
```

Direction is strictly host to core for inputs and core to caller buffers for outputs; no upward calls. The scheduler is the only holder of time.

## Architectural Patterns

### Pattern 1: Access-granular clock

**What:** Every bus access returns its cost and advances the scheduler before the next access begins. **When:** always, from slice 1 (costs may be placeholders). **Trade-off:** small per-access overhead; buys exact event ordering. `[INF]` informed by NBA `Bus::Step`.

### Pattern 2: Lazy pure state, scheduled side effects

**What:** Derive pure observable values (timer counter, VCOUNT, DISPSTAT flags) from the clock at read time; schedule an event only when something must happen (IRQ, DMA request, FIFO pop, overflow). **Trade-off:** requires discipline about which reads are pure. `[IMPL]` mGBA timers; `[INF]`.

### Pattern 3: Events set state, arbiter consumes it

See "Rule that keeps B simple" above. `[INF]`

### Pattern 4: State-ready from the first slice

Every piece of timing state (prefetch, DMA latch, open-bus latch, queue contents, pending IRQ lines) is plain data in the instance. Adding snapshots later is then serialization, not redesign. Events as closures or pointers are forbidden (NBA's queue stores event class + uid precisely for this `[IMPL]`).

### Pattern 5: Profile-declared precision

The profile names what is modeled (prefetch yes/no, contention yes/no, boot phase, BIOS mode, devices). A result's `limits` field lists what is not. A passing test then supports exactly one named claim. `[OWNER]`

## Anti-Patterns

| Anti-pattern | Why wrong | Instead |
|--------------|-----------|---------|
| Frame-stepped core (run 280,896 cycles, then do device work) | Hides raster, IRQ, and DMA ordering; fails the hw-test sweeps | Cycle budget primitive; frame is a stop condition |
| Fixed cycles per instruction table | Misses N/S, wait states, prefetch, DMA | Per-access costs from region tables |
| Resampler or filter in the core | Destroys raw evidence; license and portability cost | Raw 262,144 Hz stream; host resamples |
| Color conversion in the core | Hides 5-bit values; policy belongs to the host | Raw BGR555 words |
| Inferring save type from filename, title, or ID string | Violates the project rule; EEPROM size is ambiguous | Caller-declared device; probe is a hint |
| One aggregate pass percentage | mGBA counts comparisons; hides which area fails | Per-case results with status, plus coverage list |
| Treating an emulator's passing result as hardware truth | Two cores can share an error (EMULATOR-DNA) | Label expectations as test-authored or emulator-derived |
| HLE BIOS silently as the default and called equivalent | Not the same; BIOS-timing and math cases need real BIOS | Explicit BIOS mode in the profile and in every result |
| Callbacks from the core into the host | Reentrancy, ABI, ordering hazards | Caller-owned buffers |
| Global tables mutable per run | Breaks instance independence and determinism | Per-instance tables; test two interleaved instances |
| Event handlers that perform bus accesses | Recursion and snapshot complexity | Arbiter slots |
| Uninitialized RAM left to the host allocator | Nondeterministic runs | Documented deterministic fill; note it is a software policy, not a hardware power-on claim (GabbaBoy phrasing `[IMPL]`) |

## Performance and Scale Considerations

Correctness before speed (`[OWNER]`); these are shape notes, not targets.

| Concern | Real-time play | Headless batch (CI fixtures) | Many instances |
|---------|----------------|------------------------------|----------------|
| Work rate | 16,777,216 cycles per emulated second; B's per-access scheduler call is the hot path | Run at maximum speed; budgets in cycles keep it bounded | Instances share nothing, so parallel runs are safe by construction |
| Idle skipping | HALT fast-forward to next event | Same; lands exactly on budget | Same |
| Memory | ROM up to 32 MiB copy + about 0.4 MiB machine state per instance | Same | Memory dominated by the ROM copy; consider documenting a read-only shared ROM view later |
| Audio | 4389 hold-samples per frame, computed on mixer change | Same | Same |
| First bottleneck | CPU interpreter dispatch + bus access path | Fixture count and build/setup, not emulation | Memory bandwidth |

First bottleneck to measure later: access-granular scheduler overhead. Do not optimize before a named workload and a correctness baseline exist.

## Suggested Build Order (vertical slices)

Each slice ends in something independently verifiable. Dependencies are listed explicitly.

| Slice | Contents | Depends on | Independent verification | Notes |
|-------|----------|-----------|--------------------------|-------|
| S0 | Fixture manifest schema, result schema, canonical verify command with a negative control, rights process for first fixtures | none | Manifest validator rejects a bad entry; a deliberately failing fixture yields non-PASS | Runs alongside S1 |
| S1 | CPU (ARM + Thumb) + flat bus with the access-granular API (placeholder costs) + EWRAM/IWRAM/ROM + headless runner + `peek` + result block | S0 | Project-authored ARM/Thumb diagnostics reporting through the memory result block; jsmolka arm/thumb after per-fixture rights review `[REF]` MIT; deterministic machine-time digest repeatable | First API-shaped surface exists internally. CPU quirks (unaligned LDR rotate, LDM/STM writeback edge cases, PC-relative reads, mode banking) live here. |
| S2 | Scheduler + IRQ controller + timers + HALTCNT + keypad + BIOS stub image (vectors, IRQ entry, SWI skeleton) + input queue | S1 | Original diagnostics: timer overflow timing in cycles, IRQ entry/exit, HALT wake; hw-test `timer/*` (start-stop, reload, tick-before-reload) `[TEST]` | BIOS policy must land here because IRQ entry goes through BIOS. All later timing tests measure with timers, so this precedes S3. |
| S3 | Bus timing: WAITCNT, N/S rules, 128 KiB boundary, EWRAM/PRAM/VRAM widths, prefetch unit, open bus, BIOS read protection; add SRAM (needed by the mGBA suite log) | S2 | mGBA suite Memory, I/O read, Timing (non-BIOS entries); hw-test `bus/128kb-boundary`, `irq/irq-delay`, `haltcnt` `[TEST]` | Needs a result adapter for third-party suites (S3 includes the mGBA debug-port or SRAM-log reader). |
| S4 | DMA x4: bus ownership, start delay, latches, units, immediate first; blanking/special start deferred to S5a/S6 | S3 | hw-test `dma/start-delay`, `latch`, `force-nseq-access`, `burst-into-tears`; mGBA suite DMA `[TEST]` | Do not let DMA events call the bus (Pattern 3). |
| S5a | PPU timing only: line/dot events, VCOUNT, DISPSTAT, VBlank/HBlank/VCOUNT IRQs, HBlank/VBlank/video-capture DMA triggers, no pixels | S4 | hw-test `ppu/status-irq-dma` `[TEST]`; original raster-sync diagnostic | Resolves U1 empirically against the harness. |
| S5b | PPU pixels: modes 0-5, OBJ (regular + affine), windows, blending, mosaic, raw BGR555 frame, forced blank | S5a | Original scene diagnostics with independently computed expected frames; raw video digest plus semantic assertions; hw-test `ppu/*` expected images after provenance review | Keep the renderer behind the latch-point abstraction. |
| S5c | PPU contention schedule, latch timing (DISPCNT, BG2X/BG3X, PD), VRAM mirror rules | S5b | hw-test `ram-access-timing`, `dispcnt-latch`, `bgx`, `bgpd`, `vram-mirror`, `sprite-hmosaic`, `greenswap` `[TEST]` | Highest research risk; see flags. |
| S6 | Audio: PSG 1-4, FIFO A/B, timer pops, DMA special requests, mixer/SOUNDBIAS clip, 262,144 Hz raw stream | S4, S5a (FIFO DMA uses special timing) | Original diagnostic with exact sample vector; semantic FIFO refill assertions; separate audio digest | No resampling anywhere. |
| S7 | Backup devices: Flash (64/128 KiB, command and bank protocol), EEPROM (512 B / 8 KiB, DMA3 bit protocol), FRAM; media copy/import; generation | S4 | jsmolka `save`, original save diagnostics, export/import round trip; truncated/oversized import refusals | EEPROM needs DMA, so it follows S4. SRAM already exists from S3. |
| S8 | API hardening, state save/load (including in-flight DMA/timer/FIFO), fuzzing of loaders, installed relocatable consumer, optional LibFuturo adapter proof | S1-S7 as needed | Installed consumer exercises load, run, input, output, media, state, refusal, teardown; two-instance independence test | Public exposure is gated by the API definition being complete. |
| S9 (optional, parallel after S3) | User-supplied BIOS mode | S3 | mGBA BIOS math and BIOS-call timing entries; results marked SKIPPED when no BIOS is supplied | Needs a user-provided image; never in CI by default. |

Why this order: every timing test measures with timers, so timers and the scheduler come before bus timing; DMA and prefetch both depend on the bus arbiter; PPU timing before pixels because blanking DMA and the hw-test sweeps need only timing, and many games' sync loops need only VCOUNT; audio needs DMA special timing and timers; save-device protocols need DMA for EEPROM. State is designed in from S1 but exercised in S8.

## Unknowns and Research Flags for Phase Planning

| ID | Unknown | Why it matters | How to resolve |
|----|---------|----------------|----------------|
| U1 | Absolute alignment of HBlank/VBlank/VCOUNT flag edges and IRQ delivery relative to CPU-visible time. GBATEK says the HBlank flag is 0 for 1006 cycles; mGBA schedules at 1008; NBA at 1007; NBA's own hw-docs disagree with themselves (1006 vs 1008). | Sets the PPU event table constants | Treat the hw-test `ppu/status-irq-dma` sweep as the oracle (it reports relative cycles); calibrate constants to match it; record the harness-relative nature. Needs phase research at S5a. |
| U2 | Full Game Pak prefetch behavior across WAITCNT settings, including interactions with DMA and the "stop penalty" | Timing and mGBA suite P/N/S cases | NBA `bus/timing.cc` as implementation reference; hw-test and mGBA suite cases as oracles. Phase research at S3. |
| U3 | CPU/DMA parallel internal cycles, DMA start/hold arbitration with multi-cycle instructions | DMA tests; NBA itself approximates it | Phase research at S4; use hw-test DMA tests; accept a documented approximation if necessary and say so. |
| U4 | How hardware reduces PSG (262.144 kHz) to the SOUNDBIAS output rate; phase of a mid-period rate change | Whether the fixed 262,144 Hz hold stream is faithful at the default rate | No test in this research targets it. Mark the stream as "model" not "hardware-derived"; research before claiming audio accuracy. |
| U5 | Which mGBA suite and hw-test cases mGBA and NBA themselves pass or fail | Calibrating expectations; avoiding chasing test-specific errors | NBA README says "most" of the mGBA suite; no per-case table was found (`docs/ACCURACY.md` not located). Run both suites against Gamebloke and keep per-case results. |
| U6 | Rights of built test binaries and images in hw-test (committed `.gba`, `expected.png`, libgba/devkitARM runtime and font) and of the mGBA suite's `gfx` assets | Fixture admission | Per-fixture rights review (owner gate). Prefer building from source with a manifest recipe. |
| U7 | Boot phase after real BIOS vs post-boot state; duration of real BIOS boot | Machine-time zero, profile claim | Decide in the profile; measure with user-supplied BIOS in S9. |
| U8 | LibFuturo: audio rate acceptance at 262,144 Hz in hosts; second content slot for BIOS | Adapter feasibility | Only when the adapter proof is scheduled; read the current contract then. |
| U9 | Whether hw-test console text can be read reliably from VRAM for a headless adapter | Result channel for hw-test | Prototype in S3 when the first hw-test ROM runs. |
| U10 | Language-level API/ABI details | Out of this document's scope | STACK research. |

Recommended research flags: S3 (prefetch), S4 (DMA arbitration), S5a/S5c (flag alignment and contention) need dedicated phase research; S1, S2, S7, S8 follow established patterns and are unlikely to need more than targeted lookups.

## Sources

Read this session (2026-11-09):

- GBATEK, GBA-only mirror, https://rust-console.github.io/gbatek-gbaonly/ (LCD dimensions, memory access table, VRAM/OAM/PRAM access note, WAITCNT text, DMA transfer rate and sound FIFO DMA, SOUNDBIAS and PWM, KEYINPUT, backup ID strings, BIOS IRQ stub). Community reference; note the printed 59.737 Hz is inexact.
- NanoBoyAdvance hardware docs, https://nba-emu.github.io/hw-docs/print.html (PPU fetch patterns, timer timing notes). Community reference.
- NanoBoyAdvance source (GitHub mirror, archived; project now on Codeberg): `src/nba/include/nba/scheduler.hh`, `src/nba/src/bus/timing.cc`, `src/nba/src/hw/ppu/ppu.cc`; README at https://github.com/nba-emu/NanoBoyAdvance (GPL-3.0-or-later; claims cycle accuracy for CPU, DMA, timers, PPU, prefetch; passes AGS aging cartridge tests and "most" of the mGBA suite).
- NanoBoyAdvance hw-test (Codeberg), https://codeberg.org/nba-emu/hw-test, files `lib/source/test.c`, `irq/irq-delay`, `dma/*`, `timer/*`, `haltcnt`, `bus/128kb-boundary`, `ppu/status-irq-dma`, `ppu/ram-access-timing`, `ppu/*/README.md` (BSD-3-Clause per the GitHub mirror).
- mGBA suite, https://github.com/mgba-emu/suite: `src/main.c`, `src/timing.c`, `src/dma.c`, file listing (14 registered suites); MIT.
- mGBA source: `src/arm/arm.c`, `src/gba/timer.c`, `src/gba/video.c`, `src/gba/io.c`, `include/mgba/internal/gba/video.h`, `include/mgba/internal/gba/memory.h` (https://github.com/mgba-emu/mgba). mGBA 0.10.5 is the current release per https://mgba.io/.
- Community mGBA debug-register documentation via third-party crates (agb, gba on docs.rs); mGBA `io.c` confirms the 0xC0DE / 0x1DEA handshake. No official mGBA doc page was found for these addresses.
- jsmolka gba-tests README, https://github.com/jsmolka/gba-tests (MIT).
- LibFuturo: `~/projects/libfuturo/include/libfuturo/libfuturo.h`, `docs/contract.md`, `docs/muhtari-profile.md`, `.planning/PROJECT.md`, `.planning/PRODUCT-ROADMAP.md` (read-only).
- GabbaBoy: `~/projects/gabbaboy/include/gabbaboy/gabbaboy.h`, `docs/host-contract.json`, `docs/diagnostic-evidence.md` (read-only; lessons only, GB hardware does not transfer).
- Gamebloke: `.planning/PROJECT.md`, `AGENTS.md`, `.planning/bootstrap/EMULATOR-DNA.md`.

Not read: Nintendo AGB Programming Manual, ARM7TDMI TRM (cited in bootstrap), Tonc/gbadoc, NBA `docs/ACCURACY.md` (not found).

---
*Architecture research for: deterministic accuracy-focused GBA emulator core*
*Researched: 2026-11-09*
