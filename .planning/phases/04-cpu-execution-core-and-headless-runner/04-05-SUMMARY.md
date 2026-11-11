---
phase: 04-cpu-execution-core-and-headless-runner
plan: "05"
subsystem: cpu/testing
tags: [C17, ARM7TDMI, profile-policy, CMake, mutation-controls]

# Dependency graph
requires:
  - phase: 04-cpu-execution-core-and-headless-runner
    provides: Bus-backed direct-start CPU, register banks, exceptions, and bounded interpreter from plans 04-01 through 04-04.
provides:
  - Version 4 AGB CPU policy with separate project defaults and sourced ARM PC/bank facts.
  - Strict profile/value/narrative validation and semantic negative controls in CI and hygiene lanes.
affects: [Phase 4 headless runner, later profile consumers, Phase 5 ISA expansion]

# Actuals (#2632), chars/4 over this plan's implementation and ledger diff.
actuals:
  tokens: 8282
  tasks: 2
  commits: 2
commits: 2
plan_head_before: cbd4d61ac0c79de918537ab2518a633b9733c3d8
plan_head_after: dc1c5a03012a7d168178833405925d2a7fd33ca8

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Profile mutations derive their expected version from the current canonical JSON profile.
    - Profile policy and rendered Markdown tables are checked as semantic values, including hidden-content normalization.

key-files:
  created:
    - .planning/phases/04-cpu-execution-core-and-headless-runner/04-05-SUMMARY.md
  modified:
    - profiles/agb.json
    - docs/profile-agb.md
    - cmake/CheckProfile.cmake
    - tests/gates/profile_case.cmake
    - tests/gates.cmake
    - tests/inventory/ci.txt
    - tests/inventory/hygiene.txt
    - .planning/WINDOWS.md

key-decisions:
  - "Keep gamebloke.profile/1 and bump the additive policy-bearing profile to version 4."
  - "Label deterministic CPU startup and cycle choices as IMPL; label ARM PC offsets and register-bank facts as REF."
  - "Keep absent-BIOS exception stops before vector fetch and preserve the unqualified BIOS open-bus and Game Pak prefetch limits."
  - "Preserve the existing owner-authorized logo digest and make no new content-rights claim."

patterns-established:
  - "Profile controls must start with a clean copied profile/document and prove each deliberate value or narrative mutation fails for its named reason."
  - "Current profile version mutations use the source profile's version instead of stale literals."

requirements-completed: [] # Parent owns cross-plan requirement roll-up; this plan is partial evidence only.
coverage:
  - id: D1
    description: "The v4 profile and visible narrative agree on CPU direct-start defaults, sourced ARM facts, no-BIOS stop edge, and the bounded instruction slice."
    verification:
      - kind: integration
        ref: "cmake -DCASE=cpu_policy_value and cpu_policy_doc profile controls; CheckProfile.cmake"
        status: pass
    human_judgment: false
  - id: D2
    description: "CPU policy negative controls run in the sorted CI and hygiene inventories."
    verification:
      - kind: integration
        ref: "cmake --workflow --preset ci; cmake --workflow --preset hygiene"
        status: pass
    human_judgment: false

duration: not_measured
completed: 2026-11-11
status: complete
---

# Phase 4 Plan 5: Direct-start CPU Profile Policy Summary

**AGB profile v4 now records the deterministic CPU start state and bounded ARM/Thumb scope, while keeping architecture facts and project timing policy visibly separate.**

## Performance

- **Duration:** Not measured; the executor start timestamp was not captured.
- **Started:** Not recorded.
- **Completed:** 2026-11-11.
- **Tasks:** 2/2.
- **Plan files modified:** 7 declared implementation/test files plus the broken-windows ledger entry.
- **Canonical CI:** 314 tests inventoried; 313 passed and the permitted `signature.owner_local` test was skipped because `GBL_OWNER_LOGO_FILE` was unset. Test time was 73.26 seconds; inventory post-check passed.
- **Hygiene:** 203 tests inventoried; 202 passed and the same permitted owner-local test was skipped. Test time was 36.11 seconds; inventory post-check passed.
- **Critical path:** CI configure took about 0.1 seconds and the build had no work. Tests dominated. The additional profile tests did not require another setup or build.

## Accomplishments

- Kept schema `gamebloke.profile/1`, bumped `profile_version` from 3 to 4, and added `cpu_policy` for entry address `0x08000000`, CPSR `0x000000DF`, zero-filled banks/SPSRs, absent User/System SPSR, empty pipeline, and single-interpreter execution.
- Marked startup, cycle budgeting, exception-entry cost, and the BIOS-none stop edge as Gamebloke project policy. Marked ARM/Thumb PC offsets and register banking as sourced ARM7TDMI architecture facts. No BIOS stack convention was added.
- Documented the implemented ARM and Thumb data-processing, branch, and transfer subset plus excluded PC operands, writeback/post-index, halfword/signed, block, multiply, swap, PSR, coprocessor-execution, Thumb BL-pair, other unimplemented, and later-architecture cases.
- Refined the BIOS-protection limit: CPU fetches use the real bus, while region/state-specific open-bus behavior and Game Pak prefetch remain unqualified. Retained the owner-authorized logo digest and existing rights/compatibility caveats.
- Extended strict profile validation and copied-input controls for missing policy, wrong entry/CPSR mode/flags/PC, fake User/System SPSR, wrong BIOS-none stop, inflated evidence source, omitted/excess instruction rows, and stale, duplicated, or hidden narrative rows. Version controls now follow the current profile version.
- Registered `profile.nc.cpu_policy_value` and `profile.nc.cpu_policy_doc` in both CI and hygiene inventories.

## Verification

- `cmake -DCASE=cpu_policy_value -DSRC=. -DBIN=build/ci -P tests/gates/profile_case.cmake && cmake -DCASE=cpu_policy_doc -DSRC=. -DBIN=build/ci -P tests/gates/profile_case.cmake && cmake -DPROFILE=profiles/agb.json -DDOC=docs/profile-agb.md -DREGISTRY=registry/disputes.json -DVOCAB=contracts/evidence.json -P cmake/CheckProfile.cmake` — passed before and after the Task 1 commit. Each corrupted value/narrative was rejected with its expected reason; clean copies passed.
- `cmake --preset ci && cmake --build --preset ci && ctest --test-dir build/ci -R '^profile\.' --no-tests=error --output-on-failure` — 31/31 profile tests passed in 9.74 seconds.
- `cmake --workflow --preset ci` — 314/314 inventory entries accounted for; 313 passed and one allowed owner-local check skipped. Full test time was 73.26 seconds and `inventory.junit` passed.
- `cmake --workflow --preset hygiene` — 203/203 inventory entries accounted for; 202 passed and one allowed owner-local check skipped. Full test time was 36.11 seconds and `inventory.junit` passed.
- `git diff --check` — passed before both task commits. No task commit deleted tracked files.
- The unchanged CPU implementation was exercised by the full CI workflow's 16 CPU-labeled tests; no CPU source changes were needed.

The owner-local logo check remains an allowed skip because `GBL_OWNER_LOGO_FILE` was unset. It is recorded in `.planning/WINDOWS.md`; this plan did not read or execute any ROM or BIOS content.

## Task Commits

1. **Task 1: Validate a readable profile that matches running CPU policy** — `a8b5400` (`feat(04-05): validate direct-start CPU profile policy`).
2. **Task 2: Keep CPU policy controls in every profile gate lane** — `dc1c5a0` (`test(04-05): register CPU policy profile controls`).

## Files Created/Modified

- `profiles/agb.json` — additive v4 CPU defaults, architecture source labels, and explicit opcode/operand arrays.
- `docs/profile-agb.md` — synchronized CPU policy/support tables and hardware-limit narrative.
- `cmake/CheckProfile.cmake` — strict CPU value/source validation and visible-table synchronization.
- `tests/gates/profile_case.cmake` — semantic profile and Markdown mutations, clean positives, and version-relative controls.
- `tests/gates.cmake`, `tests/inventory/ci.txt`, and `tests/inventory/hygiene.txt` — both new controls registered in sorted lanes.
- `.planning/WINDOWS.md` — recorded the pre-existing current-latch placeholder and the permitted owner-local skipped test.

## Decisions Made

The JSON profile remains authoritative. It states the exact initialized values and implemented instruction slice, while the narrative makes clear that the start state and cycle values are deterministic project choices rather than a measured BIOS handoff or hardware timing model. The ARM reference label is limited to the PC and bank facts. CPU fetches remain bus-backed; no region-specific BIOS open-bus or Game Pak prefetch qualification is claimed.

## Deviations from Plan

None. The initial validator draft exposed a table-key mismatch and a CMake list delimiter issue while running the planned controls; both were corrected within the planned files before commit. No source behavior or task scope changed.

## Known Stubs

| File | Existing limitation |
| --- | --- |
| `profiles/agb.json:129`; `docs/profile-agb.md:124` | Current-latch reads for absent BIOS, unsupported I/O, and ROM tails remain an explicitly unverified project placeholder. No hardware read behavior is claimed. |

## Outstanding Planning Flags

- `PROF-03` remains an unclassified spec-less probe assumption, as recorded by the preceding plan.
- The standing prohibitions on repurposing private/rights-bearing inputs or inferring rights, and on inflating hardware/BIOS/compatibility claims, remain flagged-unverified. This plan does not resolve those flags.
- No physical hardware qualification, broad compatibility claim, BIOS behavior claim, redistribution right, or release qualification was established.

## Next Phase Readiness

Later Phase 4 plans can consume the versioned CPU startup and instruction-scope policy. The full phase and requirements remain under the parent workflow's roll-up.

---
*Plan: 04-05*
*Completed: 2026-11-11*

## Self-Check: PASSED

- The summary and all eight modified plan/ledger files exist.
- Both task commits (`a8b5400`, `dc1c5a0`) are ancestors of the plan head recorded above.
- The focused value/document controls, full CI workflow, full hygiene workflow, and both post-run inventory checks passed as documented.
