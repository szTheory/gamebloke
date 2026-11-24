# Roadmap: Gamebloke

## Milestones

- ✅ **v1.0 Evidence Spine** — Archived 2026-11-15 with Phase 6 incomplete and `DIAG-04` / `DET-02` carried forward. See [archive](milestones/v1.0-ROADMAP.md) and [audit](milestones/v1.0-MILESTONE-AUDIT.md).
- ✅ **v2.0 Time, Interrupts, DMA, First Pixels** — Completed locally 2026-11-19; scoped Playable 0 demo accepted. No software release or hardware qualification claimed. See [archive](milestones/v2.0-ROADMAP.md).
- ✅ **v3.0 PPU Modes and Composition** — Completed locally 2026-11-21; deterministic modes 0–5 and composition model verified. No software release or hardware qualification claimed. See [archive](milestones/v3.0-ROADMAP.md) and [audit](milestones/v3.0-MILESTONE-AUDIT.md).
- ✅ **v4.0 Audio** — Completed locally 2026-11-23; 13/13 requirements, four phases, and five integration flows verified. Audit verdict: documented tech debt only; no software release or hardware qualification claimed. See [archive](milestones/v4.0-ROADMAP.md) and [audit](milestones/v4.0-MILESTONE-AUDIT.md).
- ✅ **v5.0 Deterministic Core API and Installed Consumer Proof** — Completed locally 2026-11-24; all 9 requirements satisfied and both phases verified. Audit retains one non-blocking relocated-consumer evidence limit. No software release or hardware qualification claimed. See [archive](milestones/v5.0-ROADMAP.md) and [audit](milestones/v5.0-MILESTONE-AUDIT.md).
- 🚧 **v6.0 Public API Host Session Readiness** — Phases 22–23.

## v6.0 — Public API Host Session Readiness

Restore the operational references for the archived installed-consumer proof,
then demonstrate a sustained, deterministic session for the `agb`
direct-start/no-BIOS, ROM-only profile through the installed public C API.
Use that evidence to assess Playstead's host needs from a labeled, read-only
source snapshot. See the [accepted scope](research/V6.0-MILESTONE-SCOPE.md).

## Phases

- [ ] **Phase 22: Archived Installed-Consumer Evidence** - Restore and check the archived proof's operational references.
- [ ] **Phase 23: Public API Host Session and Playstead Fit** - Demonstrate a bounded installed-consumer session and assess Playstead's host needs.

<details>
<summary>✅ v5.0 Deterministic Core API and Installed Consumer Proof — COMPLETED LOCALLY 2026-11-24</summary>

- [x] Phase 20: Bounded Core API (1/1 plan) — completed 2026-11-23
- [x] Phase 21: Installed Consumer Proof and LibFuturo Fit (2/2 plans) — completed 2026-11-24

</details>

<details>
<summary>✅ v4.0 Audio phases — completed 2026-11-23</summary>

- [x] Phase 16: Audio Registers and Capture Contract — 3/3 plans
- [x] Phase 17: PSG Channels and Sequencer — 4/4 plans
- [x] Phase 18: Direct Sound and FIFO DMA — 4/4 plans
- [x] Phase 19: Stereo Mixing and Runner Evidence — 6/6 plans

</details>

<details>
<summary>✅ v3.0 PPU phases — completed 2026-11-21</summary>

- [x] Phase 11: Sampled Rows and Transactional Visibility — 8/8 plans
- [x] Phase 12: Background Modes 0–5 — 8/8 plans
- [x] Phase 13: Objects and Layer Priorities — 8/8 plans
- [x] Phase 14: Windows and Display Effects — 11/11 plans
- [x] Phase 15: Production Access and Runner Evidence — 5/5 plans

</details>

## Phase Details

### Phase 22: Archived Installed-Consumer Evidence

**Goal**: Developers can locate the archived Phase 21 witness and preflight its
local proof entry point after milestone archival.

**Depends on**: Phase 21 (archived)

**Requirements**: EVID-03

**Success Criteria** (what must be TRUE):

1. A developer starting at the installed-consumer README can locate the
   archived manifest and notice. The proof runner and checker resolve those
   same records with the active Phase 21 directory absent.
2. An automated missing-reference control names the unavailable record and
   exits nonzero before any fixture generation or guest load. The positive
   preflight checks the restored entry point without loading the Phase 21
   fixture.

**Plans**: TBD

Phase 22 verifies the archived references and retains Phase 21's original run
as the installed-consumer witness. The archived notice still limits fixture
use to local Phase 21 verification.

### Phase 23: Public API Host Session and Playstead Fit

**Goal**: Integrators can reproduce a deterministic host session through the
installed public C API and use measured results to judge fit for Playstead.

**Depends on**: Phase 22

**Requirements**: CONS-02, CONS-03, API-06, FIX-01, INT-02

**Success Criteria** (what must be TRUE):

1. The new project-authored fixture is distinct from the Phase 21 diagnostic.
   Its record identifies the source, revision, build recipe, digest, license,
   notices, BIOS requirements, expected guest behavior, and permission for
   each environment where it runs.
2. A C17 consumer built outside the source tree with only the relocated
   static package and public v1 headers starts a fresh supported machine,
   loads the new fixture, schedules logical input at named guest cycles across
   multiple advances, stays within each caller-specified cycle budget, and
   destroys the machine cleanly.
3. Fresh-state replay independently reports guest semantic completion, guest
   cycles, timed logical input, raw BGR555LE video, and raw pre-PWM S16LE audio.
   Guest assertions and literal media checks establish the expected result,
   and negative controls show changed input or media is detected. Digests
   supplement replay identity. The report labels host presentation as outside
   the session evidence.
4. A deliberately refused bounded operation returns its documented status
   with caller output sentinels intact. Public observations and the retry
   against a fresh-state reference show the refusal published no partial
   machine changes and the corrected request reaches the same declared guest
   result.
5. A source-cited Playstead host-fit report records the inspected revision and
   working-tree state. It maps lifecycle, timed input, cycle bounds, raw
   video/audio, teardown, and saves to the measured Gamebloke v1 boundary, with
   unsupported behavior and unknowns explicit. The assessment uses read-only
   source observations and leaves the existing managed mGBA route in place.

**Plans**: TBD

Run cheap reference and fixture checks before the focused consumer proof and
the required canonical CI workflow. Automate objective verification. A
fixture-use decision is `blocking-human` only if the required use lacks
documented authority and no authorized fixture can supply the evidence.
Playstead modifications or route replacement, save-device implementation,
BIOS support, broad compatibility claims, physical hardware qualification,
package publication, and software release require separate scope.

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 22. Archived Installed-Consumer Evidence | 0/TBD | Not started | - |
| 23. Public API Host Session and Playstead Fit | 0/TBD | Not started | - |

## Rolling Horizon

This horizon was refreshed after v5.0 closed locally on 2026-11-24 and after a
read-only pass on the next product boundary. The owner selected **v6.0 Public
API Host Session Readiness** for the current GSD milestone.

### Near term

- **v6.0 Public API Host Session Readiness.** First restore the
  archived Phase 21 proof's operational references. Then use a fresh,
  rights-cleared project diagnostic with a public-only relocated consumer to
  prove repeated bounded session advances, timed input, semantic guest result,
  literal raw video/audio output, refusal handling, and teardown. Record a
  bounded Playstead host-fit assessment from source observations without
  changing that application. This addresses the gap between v5.0's finite
  diagnostic consumer and a sustained host session while keeping the evidence
  inside Gamebloke.
- Keep mGBA as Playstead's default. Promote an opt-in app route only after an
  owner-selected clean Playstead source snapshot, specific fixture permission,
  and an independent guest oracle are available. Defer if v6.0 shows that
  saves, BIOS behavior, unsupported timing, or host-owned work outside this
  repository are required first.

### Mid term

- **Opt-in Playstead route:** after the v6.0 host session and fit evidence,
  integrate only against a clean owner-selected Playstead baseline and a
  separately authorized fixture. Preserve mGBA by default unless the owner
  approves a route decision.
- **Consumer-driven cartridge saves:** add one explicitly identified device
  only when a named scenario requires save continuity and its read/write
  behavior can be checked end to end. Keep persistence in the host adapter;
  never infer the device from a title or filename. Defer broad SRAM/Flash/EEPROM
  coverage until distinct consumers require it.
- **Targeted portability and compatibility evidence:** promote when a specific
  profile and observable claim are selected, fixtures have adequate rights and
  provenance, and the run environment is available. Keep `DIAG-04`, `DET-02`,
  model timing disputes, hosted execution, and physical qualification as
  separate evidence gates.

### Long term

- Package or release the exact verified artifact only after licensing, fixture
  rights, package readiness, evidence limits, and publication authority are
  resolved. Hardware qualification remains a separate owner-controlled gate.

### Decision notes

- **Repository evidence:** v5.0 satisfied all nine requirements and verified
  the relocated installed consumer; its audit lists only non-blocking consumer
  lifecycle evidence breadth ([v5.0 audit](milestones/v5.0-MILESTONE-AUDIT.md)).
- **Evidence maintenance observation:** the Phase 21 manifest, notice,
  checker, shell runner, and README still reference `.planning/phases/...`
  after the artifacts moved to `.planning/milestones/v5.0-phases/...`. A
  read-only path check confirmed the old notice path is absent and the archived
  notice exists; the focused diagnostic was not rerun.
- **Owner direction:** Gamebloke should add value beside Playstead's managed
  mGBA route; route replacement is not assumed ([PROJECT.md](PROJECT.md)).
- **Fresh local observation:** Playstead's support matrix still identifies
  managed mGBA 0.10.5 as its GBA runtime. The sibling checkout is heavily
  modified and diverged from its remote, so it is not a safe clean baseline for
  integration changes; any evaluation should keep that repository read-only
  until a clean, current source snapshot is selected.
- **Rights boundary:** the Phase 21 diagnostic is local-only for Phase 21 and
  is not authorized for a new Playstead evaluation. v6.0 needs a separately
  permitted fixture; a digest alone does not grant reuse.
- **Inference:** v5.0 makes public embedding plausible, but its finite consumer
  does not prove continuous frame/audio host handling, Playstead fit, or that
  Gamebloke improves compatibility or save reliability.
- **Unknown:** current Playstead source/build behavior and any user-visible
  advantage from an opt-in Gamebloke route require a clean source snapshot and
  further evidence before app integration scope is fixed.

GSD milestone numbers are planning labels, not Gamebloke software versions.
