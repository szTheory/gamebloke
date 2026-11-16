# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived with open evidence gaps on 2026-11-15. Phases 1–5 are complete; Phase 6 remains incomplete.
- 🚧 **v2.0 Time, Interrupts, DMA, First Pixels** — Phases 7–10; roadmap ready for Phase 7 discussion and planning.
- 📋 **v3.0 PPU Completion** — Planned.
- 📋 **v4.0 Audio** — Planned.
- 📋 **v5.0 Cartridge and Saves** — Planned.
- 📋 **v6.0 API, Consumer, Packaging** — Planned.

GSD milestone numbers and Gamebloke software versions are separate. Milestone progress does not imply a software release, hardware qualification, or broad compatibility.

## v2.0 — Time, Interrupts, DMA, First Pixels

## Overview

Milestone v2.0 makes machine time and events observable through one canonical C17 core, then uses that foundation for interrupts, HALT, logical input, DMA, and raw mode-3 pixels. The first guest proof is a small original BIOS-free program that polls Timer 0 overflow. Its semantic outcome, elapsed cycles, and device interrupt request state remain separate observations.

The existing one-cycle cost for every successful bus access is a placeholder. Phase 7 must expose bus and internal-cycle accounting and label its evidence honestly. It may prove semantic scheduler/timer behavior under a named project model; exact hardware-timing claims remain blocked until the necessary costs and edges are sourced and modeled. Phase 9 completes the scoped WAITCNT and bus-arbitration work. All device phases reuse the same time authority, with cheap event-boundary and test-selection controls preceding longer suites.

The current scope contains 13 requirements. BIOS-stub authoring (`BIOS-01`), SRAM (`CART-01`), and the mGBA deep differential (`EVID-07`) remain deferred in [REQUIREMENTS.md](REQUIREMENTS.md).

## Phases

- [ ] **Phase 7: Time Authority and Timer 0 Polling Proof** - One machine-time path, explicit cycle-accounting scope, four timers, and a BIOS-free polling guest with cheap boundary controls.
- [ ] **Phase 8: IRQ, HALT, and Timestamped Input** - Device requests, CPU interrupt acceptance, HALT wake, and logical keypad events remain separately observable.
- [ ] **Phase 9: Bus Timing, DMA, and Arbitration** - WAITCNT and declared prefetch scope feed timed DMA arbitration while the CPU pauses and devices continue.
- [ ] **Phase 10: PPU Timing and First Pixels** - PPU status/trigger boundaries and raw mode-3 output support the original polling Playable 0 scenario.

## Phase Details

### Phase 7: Time Authority and Timer 0 Polling Proof

**Goal**: A core developer can run an original BIOS-free Timer 0 polling guest from fresh state and inspect a deterministic overflow outcome, its elapsed time, and its declared timing limits through one machine-owned event path.
**Depends on**: Existing v1.0 core and runner; the two archived Phase 6 evidence gaps remain open and do not require another Phase 6 execution.
**Requirements**: TIME-01, TIME-03, TIME-05, BUS-06, VERIF-08
**Success Criteria** (what must be TRUE):
  1. Identical fresh-state runs dispatch plain-data events in `(cycle, priority, sequence)` order. Focused cases prove before/at/after and equal-cycle behavior, reset and stale-event handling, and a clock-overflow refusal with documented state postconditions.
  2. A developer can inspect the CPU's bus-access and internal-cycle contributions and the active profile's cost table by region, width, and sequential state. Source-backed, project-selected, and unmodeled costs are explicit; uniform one-cycle placeholders cannot be presented as hardware timing.
  3. All four timers expose reload, prescaler, enable/start, overflow, and cascade behavior. Counter reads agree with scheduled state at defined boundaries, and timer overflow request state can be observed without causing CPU exception entry.
  4. The original Timer 0 polling guest reports its semantic overflow result separately from machine cycles and interrupt request state. A deliberately faulty guest/device control produces a named failure; any result using unqualified cycle costs is visibly labeled project-model evidence.
  5. The canonical local verification path runs the short event-boundary cases before longer device/guest suites, proves its focused selection includes the intended cases, and rejects missing/empty selection through a meaningful negative control.
**Plans**:
**Wave 1**
- [x] 07-01-PLAN.md — Scheduler and Timer 0 guest tracer; exact fast selection

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 07-02-PLAN.md — Four timer boundaries and atomic refusal

**Wave 3** *(blocked on Wave 2 completion)*
- [ ] 07-03-PLAN.md — CPU bus/internal cycle attribution

**Wave 4** *(blocked on Wave 3 completion)*
- [ ] 07-04-PLAN.md — Timing profile provenance and validation

**Wave 5** *(blocked on Wave 4 completion)*
- [ ] 07-05-PLAN.md — Runner observations and result contract

**Wave 6** *(blocked on Wave 5 completion)*
- [ ] 07-06-PLAN.md — Original generated guests and determinism

**Wave 7** *(blocked on Wave 6 completion)*
- [ ] 07-07-PLAN.md — Cheap-before-long verification and phase-end judgments
**Research flag**: Phase-specific research is complete for model planning. The plan defines project policies for timer start, live reload, and same-cycle ordering; exact GBA hardware edges remain UNKNOWN/unqualified. WAITCNT and prefetch qualification remain Phase 9.
**Owner gates**: None for this BIOS-free slice. No stub code, external fixture fetch, hosted run, or publication is needed to establish its local evidence.

### Phase 8: IRQ, HALT, and Timestamped Input

**Goal**: A developer can distinguish a device interrupt request, CPU eligibility and acceptance, HALT wake, and timestamped logical input while all enabled devices share the same guest clock.
**Depends on**: Phase 7
**Requirements**: TIME-02, TIME-04, INPUT-01
**Success Criteria** (what must be TRUE):
  1. Tests can observe IF requests independently from IE/IME and CPSR masking, then observe CPU sampling at the defined instruction boundary. Eligible requests follow the CPU exception path or produce a classified no-handler `GUEST_STOP` under the selected BIOS profile; a timer/keypad event never invokes CPU entry directly.
  2. HALT wakes at the declared event boundary independently of IRQ acceptance. Timers and other enabled devices continue while the CPU waits; STOP and deadlock have distinct `GUEST_STOP` causes.
  3. The same timestamped logical keypad timeline produces the same active-low key state, KEYCNT condition results, and request transitions, including event-boundary cases. No host device discovery or host clock participates in the machine result.
  4. Guest progress, elapsed machine time, device request state, CPU acceptance/entry, and wake cause remain separately inspectable, so a wake or request alone cannot be reported as handler execution.
**Plans**: TBD
**Research flag**: Resolve ARM7TDMI exception-entry timing from the exact-core reference; qualify GBA interrupt and HALT conditions and keypad edges separately. Upstream test expectations are differential evidence unless independently qualified.
**Owner gates**: G4 still blocks BIOS-stub authoring. This phase uses the selected no-BIOS classified-stop policy and does not introduce `BIOS-01` or an IRQ-driven playable variant.

### Phase 9: Bus Timing, DMA, and Arbitration

**Goal**: A developer can observe WAITCNT-aware bus costs and deterministic DMA transfers that arbitrate for the bus while preserving machine events and explicit timing evidence limits.
**Depends on**: Phase 8
**Requirements**: BUS-05, DMA-01
**Success Criteria** (what must be TRUE):
  1. WAITCNT accesses follow the documented region/width and sequential/non-sequential tables, including 128 KiB boundary cases. The active profile explicitly identifies Game Pak prefetch as modeled with evidence or unmodeled, and BIOS read protection follows the selected BIOS policy.
  2. All four DMA channels produce the expected transfers under channel priority, start-delay, count/address-mask, and unit-timing cases. Results expose the bus owner and CPU pause rather than completing transfers through untimed callbacks.
  3. Scheduled timers and other enabled events continue during DMA ownership, including events at transfer boundaries; the CPU resumes through the canonical execution path after ownership returns.
  4. Per-channel latch cases state their expected-value provenance and retain the dispute-registry link for unresolved behavior. Deliberately wrong transfer/timing controls fail for named semantic reasons; repeatable hashes alone do not establish accuracy.
**Plans**: TBD
**Research flag**: WAITCNT, sequential transitions, prefetch scope, DMA startup/arbitration timing, and the per-channel latch dispute require primary-source and bounded differential research. PPU-triggered DMA is integrated in Phase 10.
**Owner gates**: SRAM `CART-01` and the mGBA deep lane `EVID-07` remain deferred. G8 and per-fixture rights/provenance review must be resolved before that lane is promoted or fetched.

### Phase 10: PPU Timing and First Pixels

**Goal**: A developer can inspect deterministic PPU boundaries and raw mode-3 BGR555 output from an original BIOS-free polling scenario, and the owner can judge Playable 0 using that scenario.
**Depends on**: Phase 9
**Requirements**: PPU-01, PPU-02, PLAY-01
**Success Criteria** (what must be TRUE):
  1. VCOUNT, DISPSTAT, and VBlank/HBlank/VCOUNT requests change at explicit line/dot event boundaries. Before/at/after checks retain the HBlank edge's source and dispute labels without claiming hardware qualification.
  2. PPU interrupt requests use the existing request path, and PPU DMA triggers use the existing arbiter. Their observable ordering agrees with the declared same-cycle policy while the CPU and devices share one time authority.
  3. Mode-3 output contains raw BGR555 pixels derived from canonical memory and the documented latch-point policy. Semantic line/pixel checks detect a deliberately wrong color/address/latch result, and repeated fresh runs produce identical frame bytes.
  4. An original in-tree BIOS-free polling scenario responds to a deterministic logical input timeline and produces observable guest progress, machine time, and raw video as separate outputs, without audio or save requirements.
  5. The owner records the G6 playability judgment for Playable 0. Until that judgment is recorded, automated guest/pixel evidence can pass while subjective playable acceptance remains visibly pending.
**Plans**: TBD
**Research flag**: PPU edge and latch timing, register/memory write boundaries, PPU/DMA interactions, and expected-pixel provenance need phase-specific research. A frame hash establishes repeatability unless an independent oracle backs it.
**Owner gates**: G6 is the owner's subjective acceptance. G4 continues to gate the deferred IRQ-driven/stub variant. Physical display/controller qualification and publication remain separate owner gates.

## Progress

**Execution order:** 7 → 8 → 9 → 10. Phase 7 is first in this four-phase milestone.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 7. Time Authority and Timer 0 Polling Proof | 2/7 | In Progress | - |
| 8. IRQ, HALT, and Timestamped Input | 0/TBD | Not started | - |
| 9. Bus Timing, DMA, and Arbitration | 0/TBD | Not started | - |
| 10. PPU Timing and First Pixels | 0/TBD | Not started | - |

**Coverage:** 13 of 13 current v2.0 requirements map to exactly one phase; no orphans or duplicates. Deferred requirements and v1.0 evidence carryovers are excluded from this total.

## Owner Gates and Evidence Limits

| Gate | Owner | Consequence and next action |
|------|-------|-----------------------------|
| G4 BIOS-stub authoring | Owner | `BIOS-01` and the IRQ-driven playable variant remain deferred. Obtain approval before any stub code is authored. |
| G6 Playable 0 judgment | Owner | Review the concrete original polling scenario at Phase 10 acceptance; automated evidence does not supply subjective acceptance. |
| G8 devkitPro terms / mGBA deep lane | Owner | `EVID-07` remains deferred. Resolve terms and per-fixture rights/provenance before promotion, fetch, or execution. |
| External publication / physical qualification | Owner | Neither follows from this roadmap or local automated evidence; preserve the existing stops and authority limits. |

## Evidence Carryovers from v1.0

The v1.0 milestone audit satisfied 37 of 39 requirements; Phase 6 remains incomplete. These are open evidence gaps, outside the current v2.0 requirement map:

- `DIAG-04`: ARM and Thumb stopped before source-bound result evaluation. Memory R12=0 was recorded; BIOS was skipped for its BIOS requirement. Local Linux ARM64 success does not close the missing ARM/Thumb observations.
- `DET-02`: Hosted Linux x64 GCC/Clang evidence has not run. Local Linux ARM64 results do not satisfy the hosted requirement.

See [the archived roadmap](milestones/v1.0-ROADMAP.md), [requirements](milestones/v1.0-REQUIREMENTS.md), [milestone audit](milestones/v1.0-MILESTONE-AUDIT.md), and [Phase 6 verification](milestones/v1.0-phases/06-determinism-gates-and-third-party-cpu-fixtures/06-VERIFICATION.md). No hosted rerun or publication is authorized by carrying these gaps forward.

## Later Milestones

- **v3.0 PPU Completion:** display modes 0–5, OBJ, windows, blending, mosaic, and video-memory contention and latch timing.
- **v4.0 Audio:** GBA PSG and Direct Sound FIFO, with a canonical raw model stream.
- **v5.0 Cartridge and Saves:** declared SRAM, Flash, EEPROM, FRAM, and save-media operations; promote `CART-01` when required by the selected outcome.
- **v6.0 API, Consumer, Packaging:** stable public API, relocatable package, installed consumers, and optional user-supplied official BIOS mode.

Deferred `BIOS-01` and `EVID-07` require explicit promotion after their gates are resolved; no later milestone placement is implied here.

---
*Roadmap updated: 2026-11-15 for milestone v2.0 Time, Interrupts, DMA, First Pixels*
