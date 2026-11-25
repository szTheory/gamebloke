# Requirements: Gamebloke

**Defined:** 2026-11-24  
**Milestone:** v6.0 Public API Host Session Readiness  
**Core Value:** A fresh Gamebloke instance, given a named GBA profile, content,
and timed logical input, produces deterministic, observable,
evidence-backed machine behavior, and every result says exactly what it proves
and what it does not.

## v1 Requirements

Requirements committed for this milestone. Each maps to exactly one roadmap
phase.

### Archived Evidence

- [x] **EVID-03**: A developer can locate and preflight the archived Phase 21
  witness after milestone archival; its runner, checker, manifest, notice, and
  README references resolve to the archived records, and a missing reference
  fails clearly before any guest content is loaded. The local Phase 21 fixture
  is not loaded again.

### Public Host Session

- [x] **CONS-02**: A C17 consumer can use only the installed public v1 API and
  static package to create a fresh supported machine, load a rights-cleared
  fixture, schedule logical input, perform repeated bounded advances, and
  destroy the machine cleanly.
- [x] **CONS-03**: A consumer can report guest semantic completion, guest
  cycles, raw video, and raw audio independently; literal output expectations
  and semantic assertions establish correctness, while digests only supplement
  replay identity.
- [x] **API-06**: A consumer can observe and retry a refused bounded operation
  without partial machine-state or caller-output publication, reaching the
  same declared guest result from fresh state.

### Fixture Evidence

- [x] **FIX-01**: Every fixture used for the host-session proof identifies its
  source, revision, build recipe, digest, license and notices, BIOS
  requirement, expected behavior, and permitted use; the Phase 21 local-only
  diagnostic is not reused.

### Playstead Host Fit

- [x] **INT-02**: A host-fit report maps the labeled Playstead source snapshot
  to Gamebloke v1 lifecycle, timed input, cycle budget, raw video/audio,
  teardown, and save capabilities, and lists unsupported behavior and
  unknowns without claiming Playstead integration, hardware accuracy, or a
  compatibility advantage.

## Future Requirements

Acknowledged, but not committed to v6.0:

- **INT-03**: An owner-approved Playstead route can select Gamebloke alongside
  the existing mGBA route on a clean owner-selected source baseline.
- **CART-01**: A named consumer can select one explicit cartridge save device
  and preserve its state through a host-owned persistence adapter.
- **COMP-01**: A named compatibility claim can be evaluated against an exact
  profile, rights-cleared fixture, and independent oracle.
- Package publication, software release, and physical hardware qualification
  remain separately gated owner decisions.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Reuse of the Phase 21 diagnostic in a new phase or Playstead evaluation | Its notice permits local Phase 21 use only; redistribution rights are unknown. |
| Changes to the Playstead repository or its active mGBA route | The sibling checkout is heavily modified and diverged; v6.0 keeps it read-only and does not imply route-change authority. |
| New cartridge save device or persistent file writes in the core | No v6.0 consumer scenario selects a device; persistence stays in host adapters. |
| BIOS support, broad retail compatibility, or hardware accuracy claims | The existing `agb` direct-start/no-BIOS, ROM-only profile and current evidence do not establish these claims. |
| Public package or software release | Licensing, fixture rights, package readiness, qualification claims, and publication authority remain separate gates. |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| EVID-03 | Phase 22 | Complete |
| CONS-02 | Phase 23 | Complete |
| CONS-03 | Phase 23 | Complete |
| API-06 | Phase 23 | Complete |
| FIX-01 | Phase 23 | Complete |
| INT-02 | Phase 23 | Complete |

**Coverage:**
- v1 requirements: 6 total
- Mapped to phases: 6
- Unmapped: 0

---
*Requirements defined: 2026-11-24*  
*Last updated: 2026-11-24 after v6.0 roadmap mapping*
