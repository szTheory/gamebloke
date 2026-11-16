# Requirements: Gamebloke

**Defined:** 2026-11-15
**Core Value:** A fresh Gamebloke instance, given a named GBA profile, content, and timed logical input, produces deterministic, observable, evidence-backed machine behavior. Every result says exactly what it proves and what it does not.

**Current milestone:** v2.0 Time, Interrupts, DMA, First Pixels. This is a GSD milestone label, not a software version.
**Research:** `.planning/research/SUMMARY.md` (medium overall confidence; phase-level source work remains required).

## Milestone v2.0 Requirements

### Time and timers

- [ ] **TIME-01**: A machine instance advances guest time through one deterministic scheduler. Events are plain data ordered by `(cycle, priority, sequence)`, and tests prove equal-cycle ordering, before/at/after boundaries, reset behavior, stale-event handling, and overflow refusal.
- [ ] **TIME-03**: The four timers expose documented reload, prescaler, enable/start delay, overflow, and cascade behavior. Counter reads agree with scheduled event state at defined boundaries; timer overflow requests are observable independently from CPU exception entry.
- [ ] **TIME-05**: An original BIOS-free Timer 0 polling guest reports its semantic overflow result separately from elapsed machine cycles and interrupt request state. Its result labels timing as model-only unless the required bus and internal-cycle costs are sourced and modeled.

### Bus and cycle accounting

- [ ] **BUS-05**: WAITCNT behavior uses documented wait tables and sequential/non-sequential rules, including the 128 KiB boundary. Game Pak prefetch is either modeled with explicit evidence or listed as unmodeled in the active profile; BIOS read protection follows the selected BIOS policy.
- [ ] **BUS-06**: CPU elapsed time is derived from explicit bus-access and internal-cycle components. The profile identifies source-backed, project-selected, and unmodeled costs by access region, width, and sequential state. A one-cycle placeholder cannot be presented as hardware timing.

### Interrupts, HALT, and input

- [ ] **TIME-02**: Device requests set IF independently of IE/IME and the CPU mask. The CPU samples eligible requests at a defined instruction boundary and either enters a modeled ARM7TDMI IRQ exception state under the selected BIOS profile or reports a classified `GUEST_STOP` when no handler is available; tests distinguish request, eligibility, acceptance, and entry.
- [ ] **TIME-04**: HALT wake is a machine state transition at a defined event boundary, independent of whether the CPU accepts an IRQ. Timers and other enabled devices continue to advance; STOP and deadlock produce distinct `GUEST_STOP` causes.
- [ ] **INPUT-01**: Timestamped logical keypad events update active-low key state at explicit machine cycles. KEYCNT condition evaluation and interrupt requests are observable, deterministic, and separate from host controller discovery.

### DMA

- [ ] **DMA-01**: Four DMA channels use the shared machine clock and bus arbitration. Tests cover priority, start delay, CPU pause, transfer unit timing, count/address masking, and per-channel latch behavior; disputed behavior remains linked to the dispute registry until evidence resolves it.

### PPU and first pixels

- [ ] **PPU-01**: PPU line/dot events update VCOUNT and DISPSTAT and request VBlank, HBlank, and VCOUNT interrupts or DMA triggers at explicit boundaries. The HBlank edge remains labeled with its evidence and dispute status; tests assert the chosen project behavior without overstating hardware qualification.
- [ ] **PPU-02**: A deterministic mode-3 renderer produces raw BGR555 pixels from the canonical machine state. Register and memory effects use an explicit latch-point policy, and tests check semantic line/pixel behavior as well as repeatable frame bytes.
- [ ] **PLAY-01**: An original in-tree BIOS-free polling scenario exercises mode-3 output as Playable 0, with guest progress, machine time, and raw video observed separately. Playability is accepted only by the owner's judgment (G6). An IRQ-driven variant is deferred until its BIOS requirements are authorized.

### Verification

- [ ] **VERIF-08**: Cheap event-boundary checks run before longer device/guest suites. A focused test-selection check proves that the intended cases are selected and that a missing or empty selection fails visibly; a negative control proves the gate rejects the fault.

## Future Requirements

These do not block v2.0 and require separate milestone promotion or owner-gate resolution.

- **BIOS-01**: Project-authored `project-stub` BIOS image with vector table, IRQ entry, and a documented SWI subset. G4 owner approval is required before any stub code is authored.
- **CART-01**: Declared 32 KiB SRAM backup device. Defer until a milestone outcome requires persistent cartridge state.
- **EVID-07**: mGBA-suite deep differential with an SRAM-log adapter. Resolve G8 devkitPro terms and per-fixture rights/provenance before fetching or running this lane.
- **PPU-03/04, AUD-01/02/03, CART-02/03/04/05, API-01/02, PKG-01, BIOS-02**: Retain the later capability goals recorded in the archived requirements and rolling roadmap.

## v1.0 Evidence Carryovers

These are open v1.0 gaps, not v2.0 requirements or passes:

- **DIAG-04**: Local ARM and Thumb diagnostics stopped before source-bound result evaluation. Memory R12=0 was recorded and BIOS was skipped for its BIOS requirement.
- **DET-02**: Hosted Linux x64 GCC/Clang evidence has not run. Local Linux ARM64 results do not satisfy this requirement.

No hosted rerun or publication is authorized by carrying these items forward.

## Out of Scope

| Feature | Reason |
|---------|--------|
| BIOS-stub implementation before G4 approval | Requires explicit owner approval and clean-room posture. |
| Commercial ROMs, Nintendo BIOS bytes, private saves, or fixtures without redistribution rights | Rights and security boundary; do not commit or use in public CI. |
| mGBA deep differential before G8 and fixture review | devkitPro terms, fixture rights, and output provenance remain unresolved. |
| Full PPU modes, OBJ, windows, blending, mosaic, audio, and specialty cartridge devices | Separate later milestones; keep the first visible slice focused on mode 3. |
| Hardware qualification or broad game compatibility claims | Require independent or physical evidence beyond deterministic tests or emulator agreement. |

## Traceability

Every current v2.0 requirement maps to exactly one phase. Status changes only when phase evidence satisfies the requirement.

| Requirement | Phase | Status |
|-------------|-------|--------|
| TIME-01 | Phase 7 | Pending |
| TIME-02 | Phase 8 | Pending |
| TIME-03 | Phase 7 | Pending |
| TIME-04 | Phase 8 | Pending |
| TIME-05 | Phase 7 | Pending |
| BUS-05 | Phase 9 | Pending |
| BUS-06 | Phase 7 | Pending |
| INPUT-01 | Phase 8 | Pending |
| DMA-01 | Phase 9 | Pending |
| PPU-01 | Phase 10 | Pending |
| PPU-02 | Phase 10 | Pending |
| PLAY-01 | Phase 10 | Pending |
| VERIF-08 | Phase 7 | Pending |

**Coverage:** 13 current requirements; 13 mapped to exactly one phase; 0 unmapped; 0 duplicate phase assignments. Deferred requirements and v1.0 evidence carryovers are excluded.

---
*Requirements defined: 2026-11-15*
*Last updated: 2026-11-15 after v2.0 roadmap mapping*
