---
gsd_state_version: "1.0"
milestone: v1.0
milestone_name: Evidence Spine
current_phase: 01
current_phase_name: CI and Verification Foundation
status: executing
stopped_at: Completed 01-10-PLAN.md
last_updated: "2026-11-10T01:56:07.875Z"
last_activity: 2026-11-09
last_activity_desc: Phase 01 execution started
state_head: 56a89f9d4b4097f406b60567f3d426452f3a01c2
progress:
  total_phases: 6
  completed_phases: 0
  total_plans: 11
  completed_plans: 10
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-09)

**Core value:** A fresh Gamebloke instance, given a named GBA profile, content, and timed logical input, produces deterministic, observable, evidence-backed machine behavior, and every result says exactly what it proves and what it does not.
**Current milestone:** v1.0 Evidence Spine (a GSD milestone label, not a software version)
**Current focus:** Phase 01 — CI and Verification Foundation

## Current Position

Phase: 01 (CI and Verification Foundation) — EXECUTING
Plan: 11 of 11
Status: Ready to execute
Last activity: 2026-11-09 — Phase 01 execution started

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 0
- Average duration: -
- Total execution time: 0.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**
- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 3 min | 2 tasks | 10 files |
| Phase 01 P02 | 5 min | 2 tasks | 10 files |
| Phase 01 P03 | 4 min | 2 tasks | 8 files |
| Phase 01 P04 | 5 min | 2 tasks | 11 files |
| Phase 01 P05 | 20 min | 2 tasks | 7 files |
| Phase 01 P06 | 10 min | 3 tasks | 10 files |
| Phase 01 P07 | 4 min | 2 tasks | 6 files |
| Phase 01 P08 | not measured | 2 tasks | 6 files |
| Phase 01 P09 | 10 min | 2 tasks | 14 files |
| Phase 01 P10 | not measured | 3 tasks | 4 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: Milestone v1.0 is spine first (G10 default). SUMMARY phases 1-3 are split into six phases. CI lands first; diagnostics come before determinism gates.
- [Roadmap]: Fixture policy runs on reversible defaults: pinned SHA-256 fetch, no third-party binaries in tree, no logo bytes in owned fixtures (G2, G3).
- [Roadmap]: The VERIF-06 sanitizer lanes land in Phase 1, so the CPU work (CPU-05) is UBSan-gated from its first commit.
- [Phase 01]: ci-post test preset uses output.verbosity verbose so the inventory summary reaches green logs (B1)
- [Phase 01]: Inventory compare is whole-line (name, labels, skip-ok); lines byte-sorted; reason codes inventory: unsorted|configure-mismatch
- [Phase 01]: gbl_expect_pass takes optional REGEX and wrappers print gate output on success so permitted-skip reasons reach ctest -V logs
- [Phase 01]: Negative controls match reason-code prefixes via cmake/ExpectFail.cmake (non-zero exit AND regex; exact EXIT only for documented codes like runner 2); wrapper has a meta-control
- [Phase 01]: hygiene.sh scans its own source with no exemption; regex sets are written so none matches its own line
- [Phase 01]: Hygiene findings print path:line only and file lists are NUL-delimited (git ls-files -z, find -print0, xargs -0); allowances apply to the whole grep -o match
- [Phase 01]: Logo-window rule hashes every regular file of at least 160 bytes (stricter than D-07's every binary file) so a no-NUL crafted file cannot evade it — T-01-12; cost is one dd per file
- [Phase 01]: Signature allowlist exempts only rom-extension and binary-not-allowlisted; logo-window and bios-digest are never exempted — T-01-09; proved by allowlisted stand-in controls
- [Phase 01]: Logo digest row stays owner-pending until plan 11; signature.logo.real_digest and signature.owner_local report SKIPPED with reasons — The real digest is not published in a primary source (A1); the owner supplies it with scripts/signature-digest.sh
- [Phase 01]: asan preset carries -fno-sanitize-recover=all; controls assert non-zero exit plus sanitizer text; Pitfall 2 observed (control fails with 'gate PASSED but must fail' without the flag)
- [Phase 01]: scripts/linux-lane.sh pins ubuntu:26.04 by index digest and installs libc6-dev explicitly (gcc without recommends cannot link); Linux proof ran on aarch64, x86_64 proof is plan 09 CI
- [Phase 01]: 01-06: classifier strips leading zeros with POSIX shell (no 10# bashism, dash-safe); over-nine-digit counts treated as over cap
- [Phase 01]: 01-06: CI required aggregator treats unset HEAVY as not false (fail closed); heavy jobs skipped with HEAVY true or empty fail
- [Phase 01]: 01-06: asan job installs clang-21 and libclang-rt-21-dev when absent; ubuntu-26.04 label is not in actionlint's built-in list (plan 07 needs a label config if it uses actionlint)
- [Phase 01]: Workflow policy gate also forbids direct ctest/cpack/ninja/make in the build, asan and hygiene jobs, not only non-workflow cmake — A heavy lane must reach its checks only through the workflow preset; stricter than the plan text and the real workflow is clean
- [Phase 01]: check-workflow.sh permissions and required-check rules have no committed mutation control (plan fixes ten cases and 81/41 totals); exercised by scratch mutations only — Adding controls would change inventory totals later plans may rely on; recorded as a Known Gap for the owner or checker
- [Phase 01]: 01-08: main ruleset limited to pull_request and required_status_checks (CI required, 15368), bypass_actors empty; apply script writes only that ruleset and refuses until a CI required run succeeded on main
- [Phase 01]: 01-08: --check compares the live ruleset projected onto the committed file's keys (server defaults are not drift); other repo settings stay advisory
- [Phase 01]: Every cmake -P entry script pins cmake_minimum_required(VERSION 3.25) so the gates run on the 3.25 floor (CMP0057 IN_LIST)
- [Phase 01]: All six ci.yml job timeouts are 3 minutes: slowest measured job was 36 s, so the plan floor of 3 applies (runs 42737527955#1,#2)
- [Phase 01]: PR B (#1) squash-merged as d1b5932; main push run 42737946832 reports CI required success; local main reset to origin/main with backup/phase-01-pre-squash
- [Phase 01]: 01-10: the main ruleset requires `CI required` with no bypass, so GSD now uses per-phase branches (`git.branching_strategy: phase`, template `gsd/phase-{phase}-{slug}` unchanged) and every change reaches main through a PR
- [Phase 01]: 01-10: PR A (.planning-only) ran heavy=false with CI required success; ruleset 26908822 applied last and PR A merged through it; owner chose close-out-pr (one additional close-out PR, exercised by plan 11)

### Pending Todos

None yet.

### Blockers/Concerns

- G1 (license) is open. The public repo has no LICENSE, so default copyright applies. This does not block v1.0. Do not add a LICENSE without the owner.
- Phase 5: the xPack toolchain identity (ARMv4T, identical bytes on macOS and Linux) is [UNK]. The spike gates any committed ROM digest; the fallback is Linux digest ownership.
- Phase 1: runner image labels are verified live: `ubuntu-26.04` and `macos-26` (PR B runs 42737527955, 42737836505).
- Phase 1: the real-logo digest (EVID-04) is `owner-pending (human-needed)`. The owner replied `pending` at plan 11; next action: owner runs `scripts/signature-digest.sh logo <file>` on a file they hold and commits the digest through a PR.
- Phase 1: real-logo rule inactive on origin/main until a digest lands (tests/signatures/logo.sha256 row is owner-pending; synthesized controls still guard the logo-window path)
- Next milestone (v2.0) needs G4 (stub authoring) approval before stub work begins.

## Deferred Items

Items acknowledged and deferred at milestone close, most recent first:

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| *(none)* | | | | |

## Session Continuity

Last session: 2026-11-10T01:56:07.851Z
Stopped at: Completed 01-10-PLAN.md
Resume file: None
