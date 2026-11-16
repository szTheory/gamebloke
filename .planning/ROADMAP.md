# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Milestone work archived with open evidence gaps. Phases 1–5 are complete; Phase 6 remains incomplete. This GSD milestone label is not a software release.
- 📋 **v2.0 Time, Interrupts, DMA, First Pixels** — Planned; phase numbering continues at 7.
- 📋 **v3.0 PPU Completion** — Planned.
- 📋 **v4.0 Audio** — Planned.
- 📋 **v5.0 Cartridge and Saves** — Planned.
- 📋 **v6.0 API, Consumer, Packaging** — Planned.

## v1.0 Evidence Spine — Archived Record

The milestone established the evidence spine: a canonical C17/CMake verification path, fixture and result contracts, a timing-bearing bus and safe loader, a bus-backed ARM7TDMI runner, original diagnostics, and determinism gates.

**Audit:** 37 of 39 requirements are satisfied. Phase 6 is incomplete. The audit carries these evidence gaps forward:

- `DIAG-04`: ARM and Thumb diagnostics stopped before source-bound result evaluation. Memory recorded R12=0; BIOS was skipped for its BIOS requirement. Local Linux ARM64 runs passed, but that does not close the ARM/Thumb results.
- `DET-02`: Hosted Linux x64 GCC and Clang evidence has not run. No hosted CI result is claimed.

See [the archived roadmap](milestones/v1.0-ROADMAP.md), [requirements](milestones/v1.0-REQUIREMENTS.md), [milestone audit](milestones/v1.0-MILESTONE-AUDIT.md), and [Phase 6 verification](milestones/v1.0-phases/06-determinism-gates-and-third-party-cpu-fixtures/06-VERIFICATION.md).

## v2.0 Time, Interrupts, DMA, First Pixels — Planned

This milestone starts at Phase 7 and targets one reliable master-clock event path, then IRQs, timers, HALT and timestamped input, bus wait states, SRAM, DMA, PPU timing, and a mode-3 renderer. Its visible goal is Playable 0. Begin with the scheduler and a small Timer 0 polling guest. Challenge the existing one-cycle-per-successful-access accounting before building timer claims on it. Keep device IRQ requests distinct from ARM7TDMI exception entry. Put cheap event-boundary and test-selection checks before longer suites.

BIOS-stub authoring remains gated by G4 owner approval before implementation begins. The mGBA suite deep lane remains gated by the devkitPro terms review (G8), and Playable 0 acceptance remains the owner's judgment (G6). Research refresh and phase scope will be set during v2.0 planning.

## Later Milestones

- **v3.0 PPU Completion:** display modes 0–5, OBJ, windows, blending, mosaic, and video-memory contention and latch timing.
- **v4.0 Audio:** GBA PSG and Direct Sound FIFO, with a canonical raw model stream.
- **v5.0 Cartridge and Saves:** declared Flash, EEPROM, FRAM, and save-media operations.
- **v6.0 API, Consumer, Packaging:** stable public API, relocatable package, installed consumers, and optional user-supplied official BIOS mode.

GSD milestone numbers and Gamebloke software versions are separate. No release, hardware qualification, or broad compatibility claim is implied by milestone archival.
