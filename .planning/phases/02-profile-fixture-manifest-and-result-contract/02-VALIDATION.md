---
phase: "02"
slug: profile-fixture-manifest-and-result-contract
status: validated
nyquist_compliant: true
wave_0_complete: true
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
| **Quick run command** | `cmake -S . -B build/hygiene -G Ninja -DGBL_LANE=hygiene && ctest --test-dir build/hygiene -LE post -R '^(json|contract|registry|profile|manifest|pins|fetch|result)\.' --no-tests=error -j4` |
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
| PROF-01 | Real profile complete; incomplete profile rejected; doc and JSON unmodeled lists in sync | gate + control | `ctest -R '^profile\.'` | ✅ | ✅ green |
| PROF-02 | Claim tuple present; BIOS provider outside R1 rejected | control | `ctest -R '^result\.nc\.'` | ✅ | ✅ green |
| EVID-01 | Complete manifest row accepted; all eight fields required | positive + control | `ctest -R '^manifest\.'` | ✅ | ✅ green |
| EVID-02 | Missing field, branch pin, unknown provenance class each rejected with a reason; linter runs in the canonical command | control + gate | `ctest -R '^manifest\.'`; `cmake --workflow --preset ci` | ✅ | ✅ green |
| EVID-03 | Pinned fetch verifies before placing; mismatch exits non-zero loudly with nothing placed; `.fixtures/` ignored; no third-party binary tracked | positive + control + gate | `ctest -R '^fetch\.'`; `ctest -R '^pins\.'` | ✅ | ✅ green |
| EVID-05 | Nine states as data; unknown state and PASS-without-run rejected | control | `ctest -R '^contract\.nc\.'`; `ctest -R '^result\.nc\.'` | ✅ | ✅ green |
| EVID-06 | Registry holds six seeds; a result pointing at an entry must carry `disputed` | gate + control | `ctest -R '^registry\.'`; `ctest -R '^result\.nc\.undisclosed_dispute'`; `ctest -R '^result\.nc\.unknown_dispute'` | ✅ | ✅ green |
| VERIF-01 (manifest clause) | `manifest.lint` runs inside `cmake --workflow --preset ci` | gate | `cmake --workflow --preset ci` | ✅ | ✅ green |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky. Green rows were proven by the passing test families named in the command column in `cmake --workflow --preset ci` (252 expected, 250 ran, 2 skipped) and `--preset hygiene` (182, 180, 2) on 2026-11-10; the nyquist flag covers only these automated rows.*

## Wave 0 Requirements

- [x] `cmake/GblJson.cmake` (strict loader), `cmake/Check{Profile,Registry,Manifest,Result}.cmake`
- [x] `contracts/evidence.json`, `profiles/agb.json`, `registry/disputes.json`, `tests/fixtures/manifest.json`, `pins.txt`
- [x] `docs/profile-agb.md`, `docs/evidence-contract.md`; update `docs/verify.md`
- [x] `scripts/fetch-pin.sh`; `.gitignore` gains `.fixtures/`; `scripts/linux-lane.sh` gains `curl`
- [x] `tests/gates/{json,contract,registry,profile,manifest,pins,fetch,result}_case.cmake` plus a shared mutation helper
- [x] Registrations in `tests/gates.cmake` and matching sorted lines in `tests/inventory/{ci,hygiene}.txt` (not `asan.txt`)

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions | Status |
|----------|-------------|------------|-------------------|--------|
| Live CI of the phase PR | VERIF-01 | Needs a real GitHub run | `gh run view <id>` for the phase PR: all lanes green, `CI required` success; record in the SUMMARY | pending (owner: orchestrator, at publication) |
| RESEARCH A1: curl present on the GitHub `ubuntu-26.04` runner | EVID-03 | Only a hosted run can show the runner image | Read the first hosted run of the phase PR: the `fetch.*` controls must pass without an extra install step | pending (owner: orchestrator, at publication) |
| CMake 3.25.3 floor run | VERIF-01 | CI runners ship 4.x | Run the ci workflow once with the Kitware 3.25.3 tarball (SHA-256 verified) | done 2026-11-10: exit 0, `inventory: expected 252, ran 250, skipped 2 (allowed 2)`; see 02-EVIDENCE.md |
| Linux container lane | VERIF-01 | Needs Docker | `sh scripts/linux-lane.sh ci gcc` | done 2026-11-10 (aarch64, native): exit 0, same summary line; see 02-EVIDENCE.md. The x86_64 proof is the hosted CI run above |

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 90s (ci workflow 34 s, hygiene workflow 25 s locally)
- [x] `nyquist_compliant: true` set in frontmatter (automated rows only; live CI and A1 remain manual and pending above)

**Approval:** validated for the automated rows on 2026-11-10; live CI of the phase PR and RESEARCH A1 pending with the orchestrator
