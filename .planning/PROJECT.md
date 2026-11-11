# Gamebloke

## What This Is

Gamebloke is a new, owner-controlled Game Boy Advance emulator. It aims to be
high-quality, accurate, deterministic, and observable. Its canonical machine
core is portable and has no host dependencies. A deterministic headless runner
and small host adapters sit around it. It is for the owner's emulator portfolio
and for developers and integrators who need a GBA machine they can test,
trace, and embed. Every compatibility claim names an exact machine profile,
BIOS mode, cartridge and save device, and the evidence behind it.

## Core Value

A fresh Gamebloke instance, given a named GBA profile, content, and timed
logical input, produces deterministic, observable, evidence-backed machine
behavior. That behavior covers guest progress, machine time, CPU/device
effects, raw video, and raw audio, all observed separately, and every result
says exactly what it proves and what it does not.

## Requirements

### Validated

- ✓ One canonical local verification command (`cmake --workflow --preset ci`), identical to the required `CI required` check, with a negative control for every gate — Phase 1 (verified 2026-11-10: 120 tests ran/2 visible skips locally and on Linux GCC + macOS arm64; ASan/UBSan lane; hygiene and rights-signature scans; ruleset requires the check). The manifest-lint clause joins in Phase 2.

- ✓ Caller-owned deterministic machine state, timing-bearing checked bus, declared GBA map/mirror/width policies, and transactional copied-ROM loading with bounded advisory diagnostics — Phase 3. Timing/fill/latch policies are not hardware qualification.
- ✓ Owner-authorized logo-window authority and normal/sanitized production-header comparison witnesses — Phase 3. Only the digest is published; no provenance, redistribution or game-compatibility claim follows.

- ✓ Bus-backed bounded ARM/Thumb subset, explicit pipeline/modes/exceptions, neutral mailbox inspection, category traces, exact machine-time JSON results and installed CLI relocation — Phase 4. Full ISA diagnostics remain Phase 5; timing/latch policies are unverified and trace writes currently retain the bus result zero.
- ✓ Profile version 4 names the direct-start/no-BIOS CPU policy and implemented/excluded operand scope; actual producer results preserve dispute disclosure — Phase 4.

### Active

These are hypotheses until shipped and verified. Formal REQ-IDs live in
REQUIREMENTS.md.

- [ ] A named, explicit first GBA machine profile (model, BIOS mode, cartridge/backup scope) with support claims tied to evidence
- [ ] A portable canonical machine core that owns deterministic CPU/device state and has no host files, wall clock, UI, A/V devices, or save-file policy
- [ ] ARM7TDMI ARM/Thumb execution plus the memory bus, wait states, and prefetch, at a timing precision defined by the selected profile
- [ ] Interrupts, timers, DMA, and video/audio event scheduling in guest-time units
- [ ] PPU output in raw machine form (all six display modes as the profile requires), kept separate from host conversion and scaling
- [ ] Audio (PSG + Direct Sound FIFO) produced as raw machine samples, kept separate from host resampling and playback
- [ ] Logical keypad input sampled at explicit machine-time boundaries
- [ ] Cartridge ROM validation (sizes, header, truncation) plus explicitly modeled backup devices. Save type is never inferred from a filename or title.
- [ ] A documented BIOS/boot policy that works without a bundled Nintendo BIOS dump
- [ ] A deterministic headless runner with machine-readable semantic results and raw A/V capture. Missing, skipped, harness-failed, timed-out, cancelled, and guest-failed results stay distinct.
- [ ] A fixture manifest for every test ROM (source, revision, build recipe, digest, license, permitted use, BIOS mode, expected result)
- [ ] An original or rights-reviewed diagnostic plus a legally cleared playable homebrew scenario, visible early
- [ ] A defined public API boundary (lifecycle, load/unload, input timing, bounded advancement, buffer ownership, reset, errors, refusal postconditions) before it is exposed
- [ ] An installed, relocatable external consumer once packaging exists
- [ ] Named benchmark workloads measured only after a correctness baseline exists

### Out of Scope

These are deferred, not abandoned. Each records what evidence would promote it.

- Bundling Nintendo BIOS bytes, commercial ROMs, private saves, or unlicensed fixtures. Rights boundary; never in source control or public CI.
- GB/CGB backward-compatibility mode. It is a distinct machine/CPU mode, and GabbaBoy owns the separate GB/CGB core. Promote only with an explicit owner decision and a profile.
- Link cable multiplayer, Wireless Adapter, e-Reader, and other accessories. Deferred profiles. Promote when a concrete user outcome and test evidence exist.
- Specialty cartridge devices (RTC, solar, tilt/gyro, rumble). Deferred until a cartridge profile requires them and fixtures exist.
- Universal "all retail games" compatibility claims. Compatibility is stated per profile, per evidence.
- Physical hardware qualification and physical controller qualification. Human gate; needs owner hardware and authority.
- Public binary releases and announcements. Repo visibility is settled (public), but publishing release artifacts stays an owner gate.
- Automatic replacement of Playstead's managed mGBA route. Not assumed; any Playstead integration needs its own decision.
- Choosing the project license. Owner gate (see Key Decisions).

## Context

- **Portfolio:** The owner keeps several emulator projects, all C/CMake with MIT licenses where present (GabbaBoy GB/CGB, Nesturbator, Cogmaster, Muhtari, Glueyneo, Superfriendo, DuoSpool, Lynxplosion, iamverysmartvision, Schwaystation). These are engineering lessons only. Their hardware, APIs, gates, and stops do not transfer.
- **Playstead:** It currently uses a managed mGBA process adapter for GB/GBC/GBA (pinned macOS 26/arm64). Its persistent-save qualification is GB/GBC-focused, and its GBA regression is limited to restore/Continue-menu. Gamebloke must find its own value beside this route.
- **LibFuturo** (`~/projects/libfuturo`): an experimental synchronous C API (frame-oriented advance, logical buttons, bounded work, caller-owned software A/V buffers, profile-defined media/state). It has no GBA profile. It is a candidate integration, not an accepted dependency.
- **Historical reach:** Nintendo reports 81.51M GBA units and 377.42M software units. This is distribution history, not current demand.
- **Hardware references:** AGB Programming Manual (mirror), ARM7TDMI TRM, GBATEK, Tonc/gbadoc. Candidate test suites: mGBA suite, jsmolka gba-tests, NanoBoyAdvance hw-test. Each needs a per-fixture rights/BIOS/output review.
- **Bootstrap pack:** `.planning/bootstrap/` (dated 2026-11-09) is research input, not specification.
- **Repository state at init:** No source code. Public GitHub repo `szTheory/gamebloke` was created on 2026-11-09 at the owner's direction so CI can run early.

## Constraints

- **Rights:** No Nintendo BIOS, commercial ROMs, private saves, or unlicensed fixtures in source control or public CI. A matching hash proves identity, not redistribution rights.
- **Portability:** The canonical core is independent of host files, wall-clock scheduling, device discovery, UI, A/V devices, and durable save policy.
- **Determinism:** Fresh isolated state plus explicit timed input must produce repeatable results. Hashes alone prove repeatability, not accuracy.
- **Dependencies:** Keep the tree small and flat. Each dependency needs a documented reason (lifetime, platform, license, supply chain).
- **Security:** ROMs, BIOS, saves, and metadata are untrusted input. Validate lengths and extents before allocation or mutation. Refused operations have documented postconditions.
- **Evidence:** Owner decisions, source facts, observations, measurements, inferences, recommendations, and unknowns are labeled distinctly.
- **Human gates:** Publication, copyrighted content, physical qualification, licensing, and subjective product review stay explicit owner gates.
- **Performance:** Establish correctness and named workloads first. Never trade correct output for speed.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Project name "Gamebloke" (owner) | Owner reviewed the old Apple IIe name collision and accepted reuse. Not trademark clearance. | ✓ Settled |
| Build a new GBA emulator (owner) | Broad console coverage with an owner-controlled core | ✓ Settled |
| Autonomous OpenGSD with a rolling roadmap (owner) | Routine reversible work proceeds; material choices are surfaced | ✓ Settled |
| Gamebloke does not assume it replaces Playstead's mGBA route (owner) | Value beside mGBA must be identified and measured | ✓ Settled |
| Differentiator beside managed mGBA | Research → requirements | — Pending (research) |
| First machine profile (model/region/BIOS mode) | Research → requirements | — Pending (research) |
| BIOS and boot policy | Research → requirements | — Pending (research) |
| CPU/system timing architecture | Research → phase discussion | — Pending (research) |
| Cartridge format and backup-media scope | Research → requirements | — Pending (research) |
| First diagnostic and playable fixture | Requires a rights review | — Pending (research + owner rights gate) |
| Implementation language, build, API/ABI | Research → requirements | — Pending (research) |
| Host platform and installed consumer | Research → roadmap | — Pending (research) |
| LibFuturo fit | Prove with a real core/consumer before adoption | — Pending (later phase) |
| Public GitHub repo `szTheory/gamebloke` (owner, 2026-11-09) | Owner wants CI running early. No PII or credentials committed; noreply git identity. | ✓ Settled |
| Required check `CI required` on `ubuntu-26.04` + `macos-26` (arm64), actions SHA-pinned, `main` protected by a PR-only ruleset with no bypass actors (owner-authorized, 2026-11-10) | One command, same verdict locally and in CI; nothing reaches main except through a green PR | ✓ Good (Phase 1) |
| Standing PR policy for Phases 2-6: one PR per phase, squash-merged through the ruleset when CI required is green; stop on red; no bypass or settings changes (owner, 2026-11-10) | Ruleset only accepts PRs; keeps CI proof per phase | ✓ Exercised through Phase 3; Phase 4 final-head publication in progress |
| Owner-authorized logo authority (2026-11-10) | Owner supplied two existing local files and delegated ignored copies, hashing and checks; only the shared digest is published | Verified, owner-accepted and activated on main through PR #6. No rights/provenance inference. |
| Canonical CPU/runner evidence boundary (Phase 4) | One bus-backed interpreter, whole-unit budget/trace preflight, distinct status/semantic/time/claim/limits, build-time fixture authority | Verified in local/hosted checks and independent reviews; full ISA remains Phase 5 |
| Evidence guide clarity (owner, 2026-11-11) | Owner explicitly accepted the explanation after software-evidence and rights/qualification limits were stated | ✓ Accepted; no rights grant or hardware/game claim |
| Project license | Owner authority. The repo is public with no LICENSE file yet, so default copyright applies until the owner chooses one. | — Pending (owner gate) |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check: still the right priority?
3. Audit Out of Scope: reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-11-11 after Phase 4 acceptance*
