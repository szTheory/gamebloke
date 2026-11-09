---
phase: "01"
slug: ci-and-verification-foundation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: "2026-11-09"
---

# Phase 01 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest (CMake 3.25 floor) + in-tree `tests/check.h`; junit via `ctest --output-junit` |
| **Config file** | `CMakePresets.json` (schema 6) + `tests/inventory/<lane>.txt` — none exist; Wave 0 creates them |
| **Quick run command** | `ctest --test-dir build/ci -R '^(hygiene\|signature)\.' --output-on-failure` |
| **Full suite command** | `cmake --workflow --preset ci` (and `cmake --workflow --preset asan` on Linux) |
| **Estimated runtime** | ~60 seconds locally |

---

## Sampling Rate

- **After every task commit:** the quick command for the touched area plus `sh scripts/hygiene.sh --tree`
- **After every plan wave:** `cmake --workflow --preset ci`
- **Before `/gsd-verify-work`:** `ci` and `asan` workflows green (asan on Linux), both real PRs green, `CI required` live evidence recorded
- **Max feedback latency:** 90 seconds

---

## Per-Task Verification Map

| Requirement | Behavior | Test Type | Automated Command | File Exists | Status |
|-------------|----------|-----------|-------------------|-------------|--------|
| VERIF-01 | One workflow preset builds, tests, scans, inventories; CI calls exactly it | workflow + static | `cmake --workflow --preset ci`; test `ci.yml.preset` | ❌ W0 | ⬜ pending |
| VERIF-02 | Each gate has a failing control; wrapper fails when a gate passes | meta | `nc.meta.passing_gate_fails_wrapper`, `nc.<gate>.*` | ❌ W0 | ⬜ pending |
| VERIF-03 | Removed/skipped/extra/duplicate/no-reason tests fail the inventory; skips listed | unit | `inventory.junit`, `inventory.nc.*` | ❌ W0 | ⬜ pending |
| VERIF-05 | `CI required` aggregator, classifier, title check, SHA pins | unit | `ci.required.cases`, `ci.classify.cases`, `ci.title.cases`, `ci.actions.pinned` | ❌ W0 | ⬜ pending |
| VERIF-06 | ASan/UBSan lane; shift-by-32 and heap-overflow controls caught | integration | `cmake --workflow --preset asan` (Linux) | ❌ W0 | ⬜ pending |
| VERIF-07 | Credentials, non-noreply email, home path fail; clean tree passes | unit + tree | `hygiene.tree`, `hygiene.nc.*` | ❌ W0 | ⬜ pending |
| EVID-04 | Logo window, BIOS size+digest, ROM extension, unlisted binary fail; allowlisted binary passes | unit + tree | `signature.tree`, `signature.nc.*`, `signature.digests.wellformed` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `CMakeLists.txt`, `CMakePresets.json`, `cmake/{GblTests,InventoryCompare,ExpectFail,CiRequired}.cmake`
- [ ] `scripts/{hygiene,signature-scan,check-junit,classify-changes,check-title,check-action-pins,apply-repo-settings}.sh`
- [ ] `tests/{check.h,version_test.c,ub_control.c,asan_control.c}`, `tests/inventory/*.txt`, `tests/signatures/*`
- [ ] `.github/workflows/ci.yml`, `.github/rulesets/main.json`, `.githooks/pre-commit`
- [ ] Framework install: none (CMake, Ninja, clang present)

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Live PR routing (heavy lanes run / skip) | VERIF-05 | Needs real GitHub runs | `gh run view <id>` for the source PR (all lanes) and the `.planning/`-only PR (heavy skipped, `CI required` success); record run IDs in `.planning/` |
| CMake 3.25 floor | VERIF-01 | CI runners ship 4.4.x | Run the `ci` workflow preset once with a 3.25.x CMake; record as evidence |
| Real Nintendo-logo digest provenance | EVID-04 | Owner must supply a digest from a file they hold; no bytes handled in repo | Run `scripts/signature-digest.sh logo <file>`; commit only the digest |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 90s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
