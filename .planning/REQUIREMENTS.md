# Requirements: Gamebloke

**Defined:** 2026-11-09
**Core Value:** A fresh Gamebloke instance, given a named GBA profile, content, and timed logical input, produces deterministic, observable, evidence-backed machine behavior. That behavior covers guest progress, machine time, CPU/device effects, raw video, and raw audio, all observed separately, and every result says exactly what it proves and what it does not.

**Current milestone:** v1.0 Evidence Spine. This is a GSD milestone number, not a software version: GSD milestones and software versions are numbered separately (AGENTS.md).
**Source:** `.planning/research/SUMMARY.md` (reconciliations R1–R13, owner gates G1–G13). Evidence labels follow SUMMARY.md.

## v1 Requirements (Milestone v1.0: Evidence Spine)

These are the decisions that are expensive to reverse: timing API, pipeline state, determinism, and provenance. They are built and verified before any visible output. None of them needs an owner rights decision beyond the reversible fixture defaults (pinned fetch; no third-party binaries in the tree).

### Profile and claims

- [ ] **PROF-01**: Developer can read a versioned `agb` profile document. It records machine `agb`, boot `direct-start`, BIOS provider, cartridge (ROM-only for v1.0), timing constants with their source labels, and an explicit unmodeled list. The list includes SP/Micro machine-visible differences, prefetch (until modeled), and the HBlank edge (U1).
- [ ] **PROF-02**: Every runner result carries a claim tuple `{machine, boot, bios_provider, cart, input_timeline, evidence}`. BIOS provider uses only the R1 values (`none`, `project-stub`, `hle:<list>`, `replacement:<name>@<commit>`, `user-official@<digest>`).
- [ ] **PROF-03**: Under BIOS provider `none`, a guest that takes an IRQ or executes SWI, an undefined instruction, or a coprocessor opcode ends in a classified status (GUEST_STOP), never silent continuation.

### Fixtures and evidence

- [ ] **EVID-01**: Developer can declare each fixture in a manifest row with: source URL, pinned revision, license and notices, build recipe, toolchain identity/digest, BIOS requirement, expected-value provenance class, and permitted use.
- [ ] **EVID-02**: A manifest linter rejects rows with missing fields, unpinned (branch) revisions, or an unknown provenance class.
- [x] **EVID-03**: Third-party fixtures are fetched by SHA-256 from `pins.txt`. A digest mismatch fails loudly. No third-party binary is committed.
- [ ] **EVID-04**: A signature scan in the verify command fails if a committed file contains Nintendo logo bytes, a known BIOS image, or a ROM/save file outside the allowlist.
- [ ] **EVID-05**: The runner reports results using a distinct taxonomy: PASS, GUEST_FAIL, GUEST_STOP, HARNESS_FAIL, TIMEOUT, CANCELLED, MISSING, SKIPPED, REFUSED. A fixture that did not run is never PASS.
- [ ] **EVID-06**: A dispute registry records known reference disagreements, seeded with the DMA-latch dispute, the GBATEK errata, the HBlank edge sources, and mGBA-suite issues #22/#24/#27. A fixture whose expected value is disputed is labeled as such in its results.

### Verification and CI

- [ ] **VERIF-01**: Developer can run one canonical command (`cmake --workflow --preset ci`). It builds, tests, lints the manifest, scans signatures, and checks the test inventory locally, and it is identical to the required CI check.
- [x] **VERIF-02**: Every gate has at least one negative control that must fail for a stated reason. The verify command fails if a negative control passes.
- [x] **VERIF-03**: A test-inventory check (CTest + junit) fails if any expected test is missing or silently skipped. Skipped tests are reported with a reason.
- [ ] **VERIF-04**: A missing toolchain or BIOS reports SKIPPED with a reason, never a silent fallback or PASS.
- [x] **VERIF-05**: GitHub Actions runs a single required `CI required` check on PRs and `main`, on Linux (GCC) and macOS arm64, with SHA-pinned actions. `.planning/`-only changes skip heavy lanes.
- [x] **VERIF-06**: CI runs ASan + UBSan builds of the core and runner.
- [x] **VERIF-07**: A repository hygiene check fails on credentials, non-noreply email addresses, or absolute home-directory paths in tracked files.

### CPU

- [ ] **CPU-01**: The core executes the ARM7TDMI ARM (ARMv4T) instruction set, including barrel-shifter carry, LDM/STM edge cases (empty list, base in list, user-bank transfer), misaligned load rotation, multiply flags, and PSR transfers. Each case is pinned by a diagnostic.
- [ ] **CPU-02**: The core executes the Thumb instruction set, including the BL prefix/suffix pair as two separate instructions.
- [ ] **CPU-03**: The core models the two-stage-visible pipeline (PC reads +8/+4) and the open-bus latch as explicit state, with no decode cache or fused-instruction shortcuts.
- [ ] **CPU-04**: The core switches modes and banks registers correctly for all ARM7TDMI modes. Exception entry saves the correct return address and SPSR.
- [ ] **CPU-05**: CPU arithmetic is free of C undefined behavior (shift ≥32, `ROR #0`/RRX, signed overflow), enforced by shift helpers and the UBSan lane.

### Bus and memory

- [ ] **BUS-01**: Every CPU memory access goes through a bus API that returns its cycle cost and its sequential/non-sequential class, and advances one `u64` master clock in 16,777,216 Hz cycles. Placeholder costs are allowed, but the API is final.
- [ ] **BUS-02**: The bus decodes the full GBA memory map (BIOS, EWRAM, IWRAM, I/O, PRAM, VRAM, OAM, Game Pak ROM) with documented mirrors and width rules, and unmapped reads return the open-bus value.
- [ ] **BUS-03**: Power-on memory and register contents are filled deterministically according to a documented, profile-recorded policy.
- [ ] **BUS-04**: A trace-hash gate fails if any handler advances machine time without going through the bus API.

### Content loading

- [ ] **LOAD-01**: The core copies a ROM image on load and validates its length (zero, below header size, above 32 MiB, odd length are distinct refusals) before allocation or state mutation.
- [ ] **LOAD-02**: Header findings (logo, checksum, game code) are reported as loader diagnostics, not refusals, under `direct-start`.
- [ ] **LOAD-03**: A refused load leaves the instance state and caller buffers byte-identical to before the call, verified by test.

### Runner and observability

- [ ] **RUN-01**: Developer can run a ROM headless from a fresh instance with a cycle or frame bound, and get one JSON result. Status, semantic result, machine time (cycles), claim tuple, and a `limits` field naming what the result does not prove are kept as separate fields.
- [ ] **RUN-02**: The runner reads a fixture's semantic result from an IWRAM mailbox through a side-effect-free `peek` that does not advance time or change state.
- [ ] **RUN-03**: The runner can emit a category-masked guest-time trace (instruction, bus access, exception) into a bounded buffer. It stops with an explicit output-full status instead of dropping records.
- [ ] **RUN-04**: The installed runner CLI works after relocation (`cmake --install` to one prefix, moved to another, then run).

### Diagnostics and fixtures

- [ ] **DIAG-01**: Original ARM and Thumb diagnostics build from source in CI with a pinned `arm-none-eabi` toolchain, a project crt0, linker script, and header fixer. The output has no logo bytes and no linked runtime library.
- [ ] **DIAG-02**: A toolchain spike confirms ARMv4T output (`Tag_CPU_arch: v4T`) and identical ROM bytes on macOS arm64 and Linux before any ROM digest is committed. If it fails, digest ownership moves to Linux.
- [ ] **DIAG-03**: Each original diagnostic reports a machine-readable verdict and failing case ID through the mailbox, and a deliberately broken variant serves as its negative control.
- [ ] **DIAG-04**: jsmolka gba-tests `arm`, `thumb`, and `memory` run from a pinned fetch. Their result channel (r12 at the idle loop) is verified by reading `m_test_eval` at the pinned revision before results count as PASS (R6). `bios.gba` reports SKIPPED.

### Determinism

- [ ] **DET-01**: The same profile, ROM, and input produce byte-identical results and trace hashes across repeated runs, both in-process and fresh-process.
- [ ] **DET-02**: Builds at -O0 and -O2, and from two compilers (GCC and Clang), produce identical trace hashes for every owned diagnostic.
- [ ] **DET-03**: Two instances run interleaved in one process produce the same results as each run alone (no global mutable state).

## v2 Requirements (later rolling milestones)

These are tracked but not in the current roadmap. Each later milestone is promoted with `/gsd-new-milestone`, which refreshes the research and the owner gates first. Phase numbers continue.

### Milestone v2.0: Time, Interrupts, DMA, First Pixels

- **TIME-01**: Master-clock scheduler with plain-data events ordered by (time, priority, sequence)
- **TIME-02**: IRQ controller, IME/IE/IF, sampled at instruction boundaries; latency pinned by hw-test `irq/irq-delay` (differential)
- **TIME-03**: Four timers with lazy counter reads, scheduled overflow, cascade, and start delay
- **TIME-04**: HALT wake at the exact edge; STOP and deadlock reported as GUEST_STOP
- **INPUT-01**: Keypad as timestamped logical input events applied at explicit cycles; KEYCNT IRQ
- **BIOS-01**: `project-stub` BIOS image (vector table, IRQ entry at 0x18, documented SWI subset), authored from documented behavior (owner gate G4)
- **BUS-05**: WAITCNT wait table, N/S rules, 128 KiB boundary, Game Pak prefetch (or declared unmodeled), BIOS read protection
- **CART-01**: 32 KiB SRAM backup device with a declared type
- **DMA-01**: Four DMA channels with priority, CPU pause, start delay, unit timing, count/address masking; per-channel latch under the dispute registry
- **PPU-01**: Line/dot timing, VCOUNT, DISPSTAT, VBlank/HBlank/VCOUNT IRQs and DMA triggers (U1 calibration against hw-test)
- **PPU-02**: Mode-3 per-line renderer producing raw BGR555 frames behind a latch-point table
- **PLAY-01**: Playable 0, an original in-tree mode-3 game (polling, no BIOS/audio/save), plus an IRQ-driven variant using the stub; the owner judges playability (G6)
- **EVID-07**: mGBA suite (memory, io-read, non-BIOS timing) as a deep-lane differential with an SRAM-log adapter, reported per comparison

### Milestone v3.0: PPU Completion

- **PPU-03**: Modes 0–5, regular and affine OBJ with a per-line cycle budget, windows, blending, mosaic, forced blank, green-swap
- **PPU-04**: VRAM/PRAM/OAM contention schedule and register latch table (DISPCNT, BG2X/BG3X, BGPD); VRAM mirror rules; project raster fixtures

### Milestone v4.0: Audio

- **AUD-01**: PSG channels 1–4 using the GBA register map (not GB APU reuse)
- **AUD-02**: Direct Sound FIFO A/B with timer pops and FIFO DMA; SOUNDBIAS; mixer and clip
- **AUD-03**: Canonical raw 262,144 Hz u16 model stream (4,389 samples/frame), labeled as a model; exact-sample diagnostic; separate audio digest
- **PLAY-02**: Playable 1, interrupt-driven through the stub, with sound and SRAM (may land in v5.0)

### Milestone v5.0: Cartridge and Saves

- **CART-02**: Flash 64K/128K command state machine with declared chip IDs and bank switching
- **CART-03**: EEPROM 512 B / 8 KiB via DMA3 with declared width (no size inference)
- **CART-04**: FRAM as labeled SRAM-class; media copy/import with generation counters; advisory-only ID-string scanner
- **CART-05**: ROM length sweeps and malformed-content refusals with postcondition tests

### Milestone v6.0: API, Consumer, Packaging

- **API-01**: Public C API: opaque instance, bounded advance by cycle budget with stop reasons, timestamped input, caller-owned buffers, media import/export, `peek`, allocator injection; lifecycle and refusal document first
- **API-02**: Symbol scan for writable file-scope data; loader fuzzing on a schedule
- **PKG-01**: Relocatable CMake package and installed C/C++ consumers; Linux x86_64 byte-identical cross-check; be32 hash-equality lane
- **BIOS-02** (optional): User-supplied official BIOS mode, local only, digest recorded, SKIPPED in public CI (G7)

### Later (v1.x and beyond)

- **STATE-01**: Snapshot/restore format with an in-flight round-trip entry gate (R9)
- **LF-01**: LibFuturo adapter proof after the real core and consumer exist (16-bit video, 262,144 Hz rate, second content slot for BIOS: U8)
- **BIOS-03**: Cult-of-GBA replacement fixture when a named fixture needs SWIs beyond the stub (G5)
- **ACC-01**: RTC/GPIO cartridge device (first accessory candidate), then rumble, solar, and tilt
- **DBG-01**: GDB stub; user save states; rewind
- **PERF-01**: Named benchmark workloads with correct output held constant

## Out of Scope

| Feature | Reason |
|---------|--------|
| Bundled Nintendo BIOS, commercial ROMs, private saves, unlicensed fixtures | Rights boundary (AGENTS.md); never in source control or public CI |
| Title or game-code keyed save-type database | Save type is always declared; the ID-string scan is advisory only (R1, CART research) |
| Single compatibility percentage or "passes AGS" headline | Misrepresents evidence; results are per case with provenance |
| HLE presented as the real BIOS | HLE is only a labeled `hle:<list>` overlay |
| GB/CGB backward-compatibility mode | Separate machine; GabbaBoy owns GB/CGB; needs an explicit owner decision |
| Link cable, Wireless Adapter, e-Reader | Deferred profiles; promote with a concrete user outcome and fixtures |
| `agb-micro` profile | Machine-visible differences need owner hardware (G12) |
| JIT/dynarec or decode caches before correctness | Removes observable pipeline state (CP-2); caches only behind a trace-equivalence gate |
| Embedded Lua, FFmpeg/GIF capture, wall-clock RTC in the core | Host concerns or nondeterministic; violate the core/host split |
| Copying code from GPL (NanoBoyAdvance) or MPL (mGBA) sources | License not chosen (G1); read for behavior only |
| Replacing Playstead's mGBA route | Not assumed; any integration is a separate decision (G13) |

## Traceability

Every v1 requirement maps to exactly one phase of milestone v1.0 (see `.planning/ROADMAP.md`).

| Requirement | Phase | Status |
|-------------|-------|--------|
| PROF-01 | Phase 2 | Pending |
| PROF-02 | Phase 2 | Pending |
| PROF-03 | Phase 4 | Pending |
| EVID-01 | Phase 2 | Pending |
| EVID-02 | Phase 2 | Pending |
| EVID-03 | Phase 2 | Complete |
| EVID-04 | Phase 1 | Pending (partial: real-logo digest is an owner gate; BIOS, extension, and binary rules live) |
| EVID-05 | Phase 2 | Pending |
| EVID-06 | Phase 2 | Pending |
| VERIF-01 | Phase 1 (manifest-lint clause: Phase 2) | Pending (partial: canonical command and CI parity done; manifest lint joins in Phase 2) |
| VERIF-02 | Phase 1 | Complete |
| VERIF-03 | Phase 1 | Complete |
| VERIF-04 | Phase 5 | Pending |
| VERIF-05 | Phase 1 | Complete |
| VERIF-06 | Phase 1 | Complete |
| VERIF-07 | Phase 1 | Complete |
| CPU-01 | Phase 5 | Pending |
| CPU-02 | Phase 5 | Pending |
| CPU-03 | Phase 4 | Pending |
| CPU-04 | Phase 4 | Pending |
| CPU-05 | Phase 4 | Pending |
| BUS-01 | Phase 3 | Pending |
| BUS-02 | Phase 3 | Pending |
| BUS-03 | Phase 3 | Pending |
| BUS-04 | Phase 6 | Pending |
| LOAD-01 | Phase 3 | Pending |
| LOAD-02 | Phase 3 | Pending |
| LOAD-03 | Phase 3 | Pending |
| RUN-01 | Phase 4 | Pending |
| RUN-02 | Phase 4 | Pending |
| RUN-03 | Phase 4 | Pending |
| RUN-04 | Phase 4 | Pending |
| DIAG-01 | Phase 5 | Pending |
| DIAG-02 | Phase 5 | Pending |
| DIAG-03 | Phase 5 | Pending |
| DIAG-04 | Phase 6 | Pending |
| DET-01 | Phase 6 | Pending |
| DET-02 | Phase 6 | Pending |
| DET-03 | Phase 6 | Pending |

**Coverage:**
- v1 requirements: 39 total
- Mapped to phases: 39
- Unmapped: 0
- Per phase: Phase 1: 7, Phase 2: 7, Phase 3: 6, Phase 4: 8, Phase 5: 6, Phase 6: 5

---
*Requirements defined: 2026-11-09*
*Last updated: 2026-11-09 after roadmap creation (traceability filled)*
