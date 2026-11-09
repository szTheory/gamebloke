# Emulator engineering DNA for Gamebloke

Prepared 2026-11-09 as self-contained input to the Gamebloke GSD kickoff.
This combines transferable lessons from the owner's emulator projects,
read-only inspection of the current LibFuturo contract, and public GBA
hardware/test references. The GBA notes are a dated research map, not an
authoritative complete specification or accepted implementation design.

## Evidence labels and limits

Keep owner direction, primary-source fact, community reference, code
observation, historical report, measured project result, inference, proposal,
and unknown distinct. A hardware manual or test document may still be
incomplete, misinterpreted, or version-specific. An emulator's source and
passing tests show what that implementation does; they do not alone prove
physical hardware behavior. A deterministic hash proves repeatability, not
accuracy. Keep cited fixture provenance and rights attached to any results.

The GBA references below include a mirror of Nintendo's AGB Programming Manual
and community-maintained technical documents. Preserve the document/version,
verify disputed or timing-sensitive details from current source material, and
seek a physical or genuinely hardware-derived oracle only when it changes a
material claim. This setup did not run software, tests, ROMs, a build, hardware,
or CI.

## Transferable emulator lessons

### Core/API boundaries

- Keep deterministic machine behavior behind explicit instance, content,
  profile, input, time/advance, output, lifecycle, and error boundaries.
  Keep host clocks, files, physical controls, UI, device I/O, and durable save
  writes outside the machine.
- A context-shaped API does not prove instance independence, thread safety, or
  reentrancy. Inspect mutable globals, reset behavior, callbacks, ownership,
  teardown, and concurrency before promising them.
- Specify failed load and other refused-operation effects, including whether
  the active session, machine state, and caller buffers remain unchanged.
  Version API/behavior, state format, content identity, media format, and
  profile separately only when a real consumer requires those identities.
- Build a headless runner and a clean installed consumer early enough to expose
  package, relocation, headers, runtime loading, and host-boundary faults.
  Source unit tests cannot prove those separate surfaces.

### Deterministic evidence

- Use fixed logical input with explicit sample times, fresh isolated machine
  state, and repeatable guest-time bounds. Record exact source/profile/content
  identity and build.
- Observe machine time/progress, guest-semantic conditions, registers or
  selected device effects, raw video, raw audio, and host presentation
  separately.
- Pair hashes with meaningful assertions such as a known guest progress state,
  documented register/memory effects, or an independent expected output.
  Emulator-to-emulator agreement is useful for finding disagreement, but two
  cores can share the same error.
- Make the required test inventory exact. Missing binaries, skipped tests,
  stale build output, harness faults, guest failures, timeouts, and cancelled
  runs must stay distinguishable. Include a negative control that should fail
  for an intentional reason.
- Keep long fuzzing, broad game matrices, physical tests, and noisy performance
  distributions outside the fast gate until their distinct recurring value and
  cost are clear.

### Dependencies, performance, CI, and release

- Prefer small, flat dependency trees and a documented reason for each
  dependency. Copying a small helper can be cheaper if provenance and
  maintenance ownership are clear; avoid gratuitous wrappers.
- Establish correctness and a named workload before optimizing. Keep emulator
  cycles/second, frame latency, audio underruns, memory, and power distinct
  from CI queue/setup/build/test duration.
- Use one documented local command that matches the fast required PR signal.
  Add CI only when it catches a distinct recurring failure at useful
  runtime/maintenance cost. After every CI run, inspect the measured critical
  path for duplicate setup/builds, needless matrix cells, and other safe
  improvements without weakening the evidence.
- Measure before adding caches, concurrency, or wider matrices. A cache must
  have tested invalidation and failure behavior; a broad matrix must catch a
  distinct failure class.
- At milestone close, reconcile main and pull requests, rerun the exact
  required checks for the final source/package, update the rolling roadmap and
  lessons, and target a release from the exact tested artifact when its
  contents, rights, readiness, and authority are clear.

## GBA hardware map for GSD

These are useful starting facts; verify details against the linked documents
before turning them into requirements or code assertions.

| Area | Starting evidence | GSD question / likely trap |
|---|---|---|
| CPU | AGB uses an ARM7TDMI at about 16.78 MHz with ARM and Thumb execution. The GBA also has a separate 8-bit CPU for backward-compatible GB/CGB mode. | What CPU revision/behavior and observable precision form the first profile? Do not silently treat the GB CPU/mode as part of GBA support. |
| Bus/memory | The machine has internal and external work RAM, VRAM, palette RAM, OAM, a memory-mapped device range, Game Pak regions, and a BIOS region. Game Pak wait states and instruction prefetch make access timing stateful. | Define guest-time units, sequential/nonsequential access, alignment, prefetch, bus ownership, open-bus behavior, and BIOS access rules. An instruction counter or fixed cycles per frame may miss interactions. |
| Timing/events | Visible raster is 240x160 within 308 dots per line and 228 lines per frame in the AGB manual. HBlank/VBlank, VCOUNT, timers, IRQs, DMA, sound FIFO requests, and bus stalls interact. | Verify exact edges and arbitration. A nominal frame total is not a substitute for event/timing evidence. An event-driven or cycle-aware design is a recommendation to evaluate, not a selected scheduler. |
| Video | There are six display modes: three tile-oriented modes and three bitmap modes. Background/object affine transforms, windows, priorities, blending, mosaic, DMA, and line/object limits matter. | Decide which raw output is observable and when registers/memory writes take effect. A once-per-frame renderer can miss raster effects and restricted VRAM/OAM/palette access. Preserve raw machine output separately from host conversion/scaling. |
| Audio | Four Game Boy-compatible PSG channels combine with two Direct Sound FIFO channels. Timers and DMA feed Direct Sound; SOUNDBIAS/PWM behavior affects output. | Separate audio register/events, exact sample production, raw PCM, resampling, and host playback. A non-silent or matching host-output hash is weak evidence without semantic assertions. |
| Input | The physical console has a compact digital keypad vocabulary (A/B, L/R, Start/Select, and four directions). | Name the input sample point and guest-time relationship. Keep logical replay input separate from host remapping; test opposing/no/wrong input. |
| Cartridge/save | Cartridges can add ROM banking/configuration, SRAM/FRAM, serial EEPROM, flash commands/banking, RTC/GPIO, or sensors. Backup type is not a single generic flat byte array and may not be unambiguously declared by the ROM header. | Model content and backup device explicitly. Test detection/configuration, address/protocol boundaries, save/import/export, and rejected writes. Do not guess a save type from a filename/title without evidence. |
| Optional devices | RTC, solar sensor, tilt/gyro, rumble, e-Reader, link cable, and Wireless Adapter add device or protocol state. | State which device profile is included or deferred. A link-port mode is not automatically generic cable multiplayer or Wireless Adapter emulation. |
| BIOS/compatibility | GBA boot and BIOS SWIs can affect behavior; high-level replacement is not identical to Nintendo BIOS execution. GB/CGB backward compatibility uses another CPU/machine mode. | Keep BIOS mode and compatibility claims explicit. Evaluate legal user-supplied or independently authored paths and their testability; do not bundle a BIOS dump. |

The AGB manual reports 16 KiB BIOS ROM, 256 KiB external WRAM, 32 KiB internal WRAM, 96 KiB VRAM, 1 KiB OAM, and 1 KiB palette RAM. These values help bound interfaces and tests; do not use a factual summary as a substitute for exact access rules.

### Accuracy, profile, and architecture pitfalls

- CPU instruction semantics, pipeline-visible state, memory bus cycles, DMA, timers, IRQs, PPU access restrictions/events, and audio FIFO timing are coupled. A CPU that passes opcode tests may still run games incorrectly because its system timing is too coarse.
- A final framebuffer can hide incorrect raster timing, mid-frame effects, or access restrictions. Keep timing-sensitive test output and raw capture separate from UI presentation.
- Save-state or snapshot correctness is machine-wide: test save/restore at boundary and in-flight operations, then compare exact continuation. Do not assume another core's format is portable or compatible.
- GBA ROM headers do not by themselves fully specify cartridge backup hardware. Some software markers and databases help, but can be incomplete or wrong. Support evidence should identify the exact ROM/configuration/device expectation.
- Content readers process untrusted bytes. Test truncated headers, overflow-sized lengths, malformed offsets, unusual but valid sizes, unsupported backup types, and state-preservation on refusal.
- HLE BIOS is a viable option to study, not an equivalent BIOS implementation. Record which BIOS-dependent features and games the chosen mode can support.
- Keep base GBA, GB/CGB compatibility, handheld form-factor behavior, cartridges, and accessories as separate profile/roadmap questions rather than one unbounded promise.

### Test and fixture strategy

Candidate public suites to review:

- [mGBA Game Boy Advance Test Suite](https://github.com/mgba-emu/suite) — repository identifies an MIT license.
- [jsmolka GBA tests](https://github.com/jsmolka/gba-tests) — repository identifies an MIT license.
- [NanoBoyAdvance hardware tests](https://github.com/nba-emu/hw-test) — repository identifies BSD-3-Clause terms.
- [GBATEK](https://rust-console.github.io/gbatek-gbaonly/) and [Tonc GBA programming reference](https://gbadev.net/tonc/) — detailed community hardware/programming references, not official Nintendo specifications.

Review the exact test cases, build system, result capture, expected outputs, fixture terms,
and BIOS requirements before admitting any suite to local or public CI. A suite
may be visually interactive or require a particular boot path. A project/repo
license does not automatically clear every embedded ROM, image, font, or sample
for every redistribution use. Keep fixture identity and permission as separate
facts.

Useful automation sequence to evaluate:

1. Source-built, rights-cleared test ROMs with machine-readable status output,
   fixture manifest, and deliberate positive and negative controls.
2. Component checks for CPU arithmetic/pipeline, memory map, bus/wait states,
   interrupt/timer edges, DMA sequencing, PPU modes/access rules, audio
   registers/FIFOs, controller polling, cartridge protocols, and state behavior.
3. Deterministic headless test runs from fresh instances with explicit timed
   input. Capture guest progress and time separately from raw video and audio.
4. Install/relocate a small external consumer of the real public artifact and
   exercise load, run, input, output, save-media, state, refusal, and teardown
   semantics that are claimed.
5. Add hardware-derived vectors/captures only with exact device/setup and
   evidence provenance. Keep physical hardware checks limited to claims that
   cannot be answered by trustworthy automation.

Use test suites and game runs to find specific failures, not as one aggregate
percentage that hides coverage or setup gaps. A matching result from mGBA,
NanoBoyAdvance, or another emulator is a differential signal—not independent
hardware truth. Clearly label any test expectation copied from an emulator.

## Current portfolio and LibFuturo context

GabbaBoy is a mature GB/CGB core and not a GBA implementation. Playstead's
current GB/GBC/GBA route uses managed mGBA as an external process adapter.
This is a current product observation, not an instruction that Gamebloke must
integrate or replace it. Identify the concrete user, owner-core, accuracy, or
developer outcome Gamebloke will deliver beside that route.

At the time of this bootstrap review, LibFuturo is an experimental synchronous
C API with frame-oriented work, logical-button input, bounded execution,
caller-owned software video/audio buffers, and profile-defined media/state
operations. Its real profiles did not include GBA. It is a candidate, not a
stable ABI commitment. A GBA adapter proof must test:

- whether the selected GBA profile can be driven with the API's advance and
  input-sampling boundaries without hiding timing that the product promises;
- how BIOS and cartridge content/configuration are named and admitted;
- how raw GBA video/palette representation, variable startup/frame details,
  audio sample cadence, and exact work limits fit the profile;
- what save-media identity and extent are exposed for the selected backup
  devices, plus the interaction with snapshots;
- whether the installed consumer works after relocation and is tested through
  public headers and binaries.

Host controller discovery, file I/O, A/V presentation, and durable save writes
belong to the host. A successful compile does not qualify an adapter, a browser
runtime, a physical controller, or hardware accuracy. Recheck the current
LibFuturo contract before any milestone decision; its files and profiles can
change independently.

## Project-by-project learning without copying stops

The following lessons were distilled from read-only review of recent emulator
projects. They are engineering context only; their hardware, APIs, releases,
test results, and unresolved gates do not transfer to Gamebloke.

- **GabbaBoy:** exact profiles and user-input-driven guest progress are more
  useful than broad compatibility labels. Keep guest semantics separate from
  image/audio digests and include negative controls. Its GB/CGB hardware and
  active GBA-independent release gates remain project-specific.
- **Nesturbator:** the installed and relocatable consumer exposes package and
  host faults that source tests miss. Its current NES-to-LibFuturo provider gate
  and any one-off diagnostic authority do not transfer.
- **Glueyneo:** bind consumer evidence to the intended saved core/build identity;
  a missing or stale binary invalidates the claim. Its Neo Geo and Playstead
  stops are project-specific.
- **Muhtari:** validate raw video/audio formats, buffer bounds, input and refusal
  postconditions at the public consumer boundary. Atari profile values do not
  transfer.
- **Cogmaster:** separate frame-aligned input, semantic guest progress, image
  output, and audio output in a headless runner. Its Sega hardware behavior and
  sound cadence do not transfer.
- **Superfriendo and DuoSpool:** defer shared API adoption until a real consumer
  proves the contract fit. Keep profile, media, and host ownership explicit;
  neither SNES nor PC Engine hardware is a GBA source.
- **Lynxplosion and iamverysmartvision:** be explicit about boot/profile choices,
  controller or accessory behavior, fixture rights, and diagnostic-result
  capture. Their current open hardware/firmware decisions are not GBA evidence.
- **Schwaystation:** a planned architecture or test card is not implementation
  or game-compatibility evidence. Preserve the boundary between a working
  harness and a working emulator.

When later milestones revisit these projects, inspect only the current evidence
that can change the live decision. Never copy another project's active stop,
CI cap, permission, test fixture, API, or readiness status into Gamebloke.

## Decision-specific GSD fan-out and milestone loop

For each milestone discussion:

1. Read Gamebloke's current PROJECT, REQUIREMENTS, ROADMAP, STATE, decisions,
   lessons, tests, current source, CI, and outstanding release/verification
   evidence.
2. Select independent research questions from the real decision. Use the
   relevant CPU/bus, PPU/video, audio, cartridge/peripheral, product, API/host,
   QA/security, performance, dependency, and DevOps lenses; avoid making
   irrelevant roles repeat generic arguments.
3. Read current primary/authoritative sources, inspect current sibling or
   LibFuturo evidence only when it can change the choice, compare alternatives
   and failure modes, and do one adversarial pass.
4. Recommend one path with evidence, tradeoffs, risk and an automated evidence
   plan. Stop when further research no longer changes it.
5. Record accepted owner choices separately from reversible defaults and
   unresolved material decisions. Update the rolling roadmap so the next
   milestone is ready for a bounded discussion and autonomous execution.
6. At close, verify the exact final source and package, keep main green, triage
   PRs, write lessons and release the exact tested artifact when ready.

## Sources checked 2026-11-09

- Nintendo, [Game Boy Advance technical data](https://www.nintendo.com/en-gb/Support/Legacy-system/Technical-data-619479.html) — official summary dimensions, CPU class, display, color and controls.
- Nintendo, [AGB Programming Manual v1.1](https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) — detailed CPU, memory, I/O, graphics, audio, and timing reference. This is a mirror of a Nintendo document; verify important claims against exact sections and other evidence.
- Arm, [ARM7TDMI Technical Reference Manual](https://documentation-service.arm.com/static/5e8e1323fd977155116a3129) — CPU execution model and bus behavior.
- GBATEK, [GBA-only hardware reference](https://rust-console.github.io/gbatek-gbaonly/) — community technical reference for LCD timing, cartridge protocols, GPIO, and related details; not Nintendo-issued.
- [Tonc GBA programming reference](https://gbadev.net/tonc/) and [gbadoc](https://gbadev.net/gbadoc/) — community implementation-oriented documentation, not independent hardware qualification.
- [mGBA test suite](https://github.com/mgba-emu/suite), [jsmolka tests](https://github.com/jsmolka/gba-tests), and [NanoBoyAdvance hardware tests](https://github.com/nba-emu/hw-test) — test-discovery leads; inspect exact case results and terms before use.
- [mGBA](https://mgba.io/) — active multi-platform GBA emulator and useful differential/debugging implementation, not a standalone accuracy oracle.
- Nintendo, [hardware/software lifetime sales](https://www.nintendo.co.jp/ir/en/finance/hard_soft/index.html?country=66) — historical product reach, not current use or evidence of unmet demand.

No binaries, copyrighted content, firmware, or private saves were copied into this pack.
