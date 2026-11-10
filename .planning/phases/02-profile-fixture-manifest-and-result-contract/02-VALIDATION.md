---
phase: "02"
slug: profile-fixture-manifest-and-result-contract
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-10"
---

# Phase 02 — Validation Strategy

> Per-phase validation contract. The full requirement-to-test map is in
> 02-RESEARCH.md `## Validation Architecture`; this file is the execution contract.

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest via `gbl_add_test` plus the Phase 1 sorted-inventory and junit checks |
| **Config file** | `CMakeLists.txt`, `CMakePresets.json`, `tests/gates.cmake`, `tests/inventory/{ci,hygiene}.txt` |
| **Quick run command** | `cmake -S . -B build/hygiene -G Ninja -DGBL_LANE=hygiene && ctest --test-dir build/hygiene -LE post -R '^(json\|contract\|registry\|profile\|manifest\|pins\|fetch\|result)\.' -j4` |
| **Full suite command** | `cmake --workflow --preset ci` and `cmake --workflow --preset hygiene` |
| **Floor run** | Kitware 3.25.3 `cmake --workflow --preset ci` once at the phase end (per `docs/verify.md` slow-lane checks) |

## Sampling Rate

- **After every task commit:** the quick command for the family touched, plus `sh scripts/hygiene.sh --tree`
- **After every plan wave:** `cmake --workflow --preset hygiene` and `cmake --workflow --preset ci`
- **Before `/gsd-verify-work`:** `ci` green locally and on the 3.25.3 floor, `sh scripts/check-traps.sh`, and the phase PR's `CI required` green
- **Max feedback latency:** 90 seconds

## Per-Task Verification Map

| Requirement | Behavior | Test Type | Automated Command | File Exists | Status |
|-------------|----------|-----------|-------------------|-------------|--------|
| PROF-01 | Real profile complete; incomplete profile rejected; doc and JSON unmodeled lists in sync | gate + control | `ctest -R '^profile\.'` | ❌ W0 | ⬜ pending |
| PROF-02 | Claim tuple present; BIOS provider outside R1 rejected | control | `ctest -R '^result\.nc\.'` | ❌ W0 | ⬜ pending |
| EVID-01 | Complete manifest row accepted; all eight fields required | positive + control | `ctest -R '^manifest\.'` | ❌ W0 | ⬜ pending |
| EVID-02 | Missing field, branch pin, unknown provenance class each rejected with a reason; linter runs in the canonical command | control + gate | `ctest -R '^manifest\.'`; `cmake --workflow --preset ci` | ❌ W0 | ⬜ pending |
| EVID-03 | Pinned fetch verifies before placing; mismatch exits non-zero loudly with nothing placed; `.fixtures/` ignored; no third-party binary tracked | positive + control + gate | `ctest -R '^(fetch\|pins)\.'` | ❌ W0 | ⬜ pending |
| EVID-05 | Nine states as data; unknown state and PASS-without-run rejected | control | `ctest -R '^(contract\|result)\.nc\.'` | ❌ W0 | ⬜ pending |
| EVID-06 | Registry holds six seeds; a result pointing at an entry must carry `disputed` | gate + control | `ctest -R '^registry\.'`; `ctest -R '^result\.nc\.(undisclosed_dispute\|unknown_dispute)'` | ❌ W0 | ⬜ pending |
| VERIF-01 (manifest clause) | `manifest.lint` runs inside `cmake --workflow --preset ci` | gate | `cmake --workflow --preset ci` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

## Wave 0 Requirements

- [ ] `cmake/GblJson.cmake` (strict loader), `cmake/Check{Profile,Registry,Manifest,Result}.cmake`
- [ ] `contracts/evidence.json`, `profiles/agb.json`, `registry/disputes.json`, `tests/fixtures/manifest.json`, `pins.txt`
- [ ] `docs/profile-agb.md`, `docs/evidence-contract.md`; update `docs/verify.md`
- [ ] `scripts/fetch-pin.sh`; `.gitignore` gains `.fixtures/`; `scripts/linux-lane.sh` gains `curl`
- [ ] `tests/gates/{json,contract,registry,profile,manifest,pins,fetch,result}_case.cmake` plus a shared mutation helper
- [ ] Registrations in `tests/gates.cmake` and matching sorted lines in `tests/inventory/{ci,hygiene}.txt` (not `asan.txt`)

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Live CI of the phase PR | VERIF-01 | Needs a real GitHub run | `gh run view <id>` for the phase PR: all lanes green, `CI required` success; record in the SUMMARY |
| CMake 3.25.3 floor run | VERIF-01 | CI runners ship 4.x | Run the ci workflow once with the Kitware 3.25.3 tarball (SHA-256 verified) |

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 90s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
