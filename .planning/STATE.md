---
gsd_state_version: "1.0"
milestone: v1.0
milestone_name: Evidence Spine
current_phase: 4
current_phase_name: CPU Execution Core and Headless Runner
status: executing
stopped_at: Phase 4 plans checked, ready to execute
last_updated: "2026-11-11T04:53:28.299Z"
last_activity: 2026-11-10
last_activity_desc: Phase 4 execution started
state_head: 360ce44ecd4165ab963eac0e96b7b3f0bed2c522
progress:
  total_phases: 6
  completed_phases: 3
  total_plans: 38
  completed_plans: 28
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-10)

**Core value:** A fresh Gamebloke instance, given a named GBA profile, content, and timed logical input, produces deterministic, observable, evidence-backed machine behavior, and every result says exactly what it proves and what it does not.
**Current milestone:** v1.0 Evidence Spine (a GSD milestone label, not a software version)
**Current focus:** Phase 4 — CPU Execution Core and Headless Runner

## Current Position

Phase: 4 (CPU Execution Core and Headless Runner) — EXECUTING
Plan: 4 of 13
Status: Ready to execute
Last activity: 2026-11-10 — Phase 4 execution started

Progress: [█████░░░░░] 50%

## Performance Metrics

**Velocity:**
- Total plans completed: 25
- Average duration: -
- Total execution time: 0.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 11 | - | - |
| 2 | 7 | - | - |
| 3 | 7 | - | - |

**Recent Trend:** Not enough data to calculate yet.

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
| Phase 01 P11 | not measured | 3 tasks | 3 files |
| Phase 02 P01 | 45 min | 3 tasks | 9 files |
| Phase 02 P02 | 25 min | 2 tasks | 6 files |
| Phase 02 P03 | 25 min | 3 tasks | 8 files |
| Phase 02 P04 | 35 min | 3 tasks | 9 files |
| Phase 02 P05 | 7 min | 3 tasks | 8 files |
| Phase 02 P06 | 20 min | 3 tasks | 6 files |
| Phase 02 P07 | 25 min | 3 tasks | 5 files |
| Phase 03 P01 | 26 min | 3 tasks | 10 files |
| Phase 03 P02 | 21 min | 2 tasks | 7 files |
| Phase 03 P03 | 20 min | 3 tasks | 10 files |
| Phase 03 P04 | 38 min | 3 tasks | 8 files |
| Phase 03 P05 | 23min | 2 tasks | 10 files |
| Phase 03 P07 | 44 min | 3 tasks | 17 files |
| Phase 04 P01 | not_measured | 1 tasks | 5 files |
| Phase 04 P02 | not_measured | 2 tasks | 9 files |
| Phase 04 P03 | not_measured | 3 tasks | 8 files |

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
- [Phase 01]: 01-11: owner replied `pending` for the real logo digest (EVID-04 real-logo item is owner-pending, human-needed; logo.sha256 unchanged); close-out reached main through one PR (#3) squash-merged through the ruleset; EVID-04 left Pending in REQUIREMENTS.md until the verifier decides
- [Phase 01]: Owner authorized (2026-11-10): one PR for the Phase 1 review fixes, and a standing one-PR-per-phase policy for Phases 2-6: push the phase branch, open one PR, squash-merge through the ruleset once CI required is green. Stop and report on red CI; never bypass the ruleset or change repo settings; any other publication needs a fresh answer. — Ruleset 26908822 only accepts changes through PRs; phase work cannot reach main otherwise.
- [Phase 02]: result_labels omitted from contracts/evidence.json: the only label, disputed, is a rule (plan 06 hard-codes it) not a vocabulary entry
- [Phase 02]: Strict JSON loader uses a structural depth walk for trailing content, not a bracket-class regex; findings never echo file content
- [Phase 02]: 02-02: Errata dispute entry enumerates twelve topics from the primary mGBA forum post (RESEARCH merged two rumble lines into eleven); issue and errata hardware claims are labeled SRC with UNK noted, never REF
- [Phase 02]: Profile timing includes dots_per_line (308, REF) so all five timing invariants are checkable; hblank_flag_edge_cycle must carry a dispute id (dispute-unknown otherwise)
- [Phase 02]: Manifest field names fixed for gamebloke.manifest/1 (source, license, build, bios_requirement, expected, permitted_use, optional fetch.pin_dest); renaming needs a /2 bump
- [Phase 02]: pins: bad-url reuses the manifest URL rule (https only, no whitespace, quote, or backslash); a row with a forbidden character reports bad-row plus its field codes
- [Phase 02]: fetch-pin.sh runs ${CURL:-curl} (the program its presence check tests); fetch controls assert non-zero exit plus reason text, never an exact status
- [Phase 02]: fixture-dir gate counts git ls-files output lines (no -z: a CMake variable cannot hold NUL); prints only the count
- [Phase 02]: Result validator hard-codes only the disputed label (I2); ROADMAP SC5 delivered at validator level in Phase 2, producer half moves to Phase 4 when the runner emits results
- [Phase 02]: 02-07: pins.txt is linted by manifest.lint (MODE=all); no separate pins gate exists
- [Phase 02]: 02-07: floor (3.25.3) and Linux container (aarch64) runs pass 252/250/2; live CI and RESEARCH A1 stay pending with the orchestrator
- [Phase 03]: Phase 03-01: caller-owned deterministic state; raw access uses little-endian EWRAM, one project-policy cycle, checked atomic time, and host-managed same-instance synchronization.
- [Phase 03]: Keep schema gamebloke.profile/1 and bump the additive policy-bearing profile to profile_version 2.
- [Phase 03]: Record one-cycle bus costs and zero initialization as IMPL project policy, not measured hardware behavior.
- [Phase 03]: Keep absent-BIOS, unsupported-I/O, and ROM-tail current-latch reads explicitly unverified; WAITCNT, prefetch, BIOS protection, and active I/O devices remain unmodeled.
- [Phase 03]: ROM load size refusals are ordered: zero, below 0xC0, above 32 MiB, then odd; valid-size pointer and allocator checks follow.
- [Phase 03]: The host allocator owns copied ROM storage; successful replacement commits and cold-resets before releasing the previous allocation.
- [Phase 03]: Refusal evidence compares canonical machine fields and arrays, ROM contents, and caller bytes; the digest is secondary evidence.
- [Phase 03]: 03-04: ROM-tail reads use the current-latch project policy; shorter-cartridge hardware behavior remains explicitly unverified.
- [Phase 03]: 03-04: Model only DISPCNT mode bits 0-2; mode 6-7 video-byte threshold behavior is project policy, not hardware evidence.
- [Phase 03]: 03-04: Cite GBATEK as community reference evidence and keep BUS-02/BUS-03 pending until integrated phase evidence and final verification support completion.
- [Phase 03]: Header findings are advisory for every size-valid direct-start image and commit only with a successful copied-ROM load.
- [Phase 03]: Canonical logo stays UNCHECKED/OWNER_PENDING; synthetic comparisons prove mechanics only.
- [Phase 03]: Compute the header checksum by subtracting bytes 0xA0-0xBC and 0x19 from zero modulo 256 before comparing byte 0xBD.
- [Phase 03]: Canonical profile document synchronization supports plain Markdown plus fenced/indented examples and HTML comments; reject unsupported raw HTML blocks. Normalize once before any metadata, heading, section, or table discovery, and use that same visible content everywhere; require exactly one visible standalone version line.
- [Phase 03]: For document-parser fixes, add controls for hidden headings/tables, adjacent contradictory metadata, and markup opened before and closed after the synchronized section. After a broad phase review, target follow-up review at changed files and the confirmed root-cause family; do not repeat unchanged core review without a new concern.
- [Phase 03]: Profile document controls exercise distinct mutations. The final `profile.nc.bus_policy_doc` test took 3.15 s in CI and 2.88 s in hygiene, below each lane's five slowest tests; no duplicate validator setup was supported by the measurements. CI (17:03:57–17:06:02) and hygiene (17:06:11–17:07:25) did not overlap. The broader runtime increase from earlier 49–97 second CI samples is unexplained by retained logs; no optimization was justified.

- [Phase 03]: Verifier MVP-format repair: restated the existing approved technical goal as an As-a/I-want-to/so-that story; retained the original technical boundary and every success criterion. This is agent-authored formatting, not new owner scope or risk acceptance; no replanning or gate bypass.

- [Phase 03]: Plan 07 wires the bounded digest comparison through the real copied-ROM loader and validates the fixed build authority. Fresh current-inventory sanitizer evidence and clean independent code/security reviews close the implementation gaps. Production authority remains owner-pending; synthetic matches do not complete LOAD-02.

- [Phase 03]: Initial hosted stop: draft PR #6 failed GNU compilation on an implicit unsigned-to-byte conversion in the test-state helper. No fix or retry preceded owner authorization. The subsequent authorized correction and local retest are recorded in 03-CI-HANDOFF.md; use the PR live checks for the corrected hosted head.

- [Phase 03]: Owner authorized the explicit test-helper byte conversion and bounded local/hosted verification on existing PR #6. Preserve all warning flags. Renew the standing stop if the new hosted run is red; logo authority/manual acceptance and Phase 4 hold are unchanged.

- [Phase 03]: Owner supplied two local files and explicitly authorized ignored copies plus agent-run logo hashing/verification. Both windows agree; profile version 3 activates the configured digest. Current CI, fresh sanitizers and instrumented local production-header witnesses pass. Refreshing phase acceptance; no private bytes are published and no hardware/game-compatibility or rights claim follows.

- [Phase 03]: Activation verification confirms every technical must-have. Only two judgment-tier claim reviews remain; no further owner data/helper action is needed. The owner explicitly delegated local file handling, which supersedes the earlier owner-must-run procedure for those inputs. Preserve that authority across runtimes instead of asking the owner to repeat commands the agent can perform.

- [Phase 03]: Published activation head passes hosted Linux GCC, macOS AppleClang, Clang sanitizers, hygiene and CI required on PR #6. Measured longest lane was macOS: about 94 seconds job-log span and 81.42 seconds tests, with one configure/build sequence. No low-risk duplicate-build remedy was established. A metadata-triggered old-head run was superseded/cancelled; its fail-closed aggregator does not represent a failure of the tested activation head. Retained results: build/phase03-logo-activation/hosted-result.json. Final local acceptance bookkeeping is held for the next phase-boundary commit to avoid triggering an unchanged full CI run.

- [Phase 03]: The owner explicitly accepted both documented phase claim limits. UAT and canonical verification pass; all seven implementation plans are complete. Existing PR #6 is the publication path; push final acceptance bookkeeping once and wait for green required checks before squash merge. No further acceptance prompt is needed for these same limits. The phase contains no frontend; visual review is not applicable. Graduation scan has only two completed prior phases, below its threshold.

- [Phase 03]: PR #6 squash-merged through the ruleset; main required CI passes. Final-head and main timing/inventory snapshots are retained in build/phase03-logo-activation/merge-main-result.json. Private inputs and original user edits were preserved. A stash backup of only the generated state cache remains local after a clean restore; no reset or branch deletion was used.

- [Phase 04]: Required ARM7TDMI DDI0029G manual and GBATEK 3.06 CPU source reading are complete; independent CPU/GBA/runner notes and the synthesized context distinguish source observations from project policy. The GBA note was recovered complete after interrupting prolonged finalization, without repeating source reading. Routine reversible choices are recorded as agent decisions under standing autonomy, not fabricated owner votes. The original Phase 4 goal is retained verbatim as its technical boundary alongside the MVP user-story format.

- [Phase 04]: Keep one canonical `04-RESEARCH.md`; supporting lenses use `*-NOTES.md`. The generic research-file resolver selected a partial ARM lens when several files ended in RESEARCH.md, creating a misleading drift warning and hiding the synthesized validation section. Supporting-note renaming and explicit context reconciliation fix the consumer input without repeating hardware research.

- [Phase 04]: Plans 01–02 are committed. Configured production/synthetic and scratch builds share the CPU source; an empty loaded-ROM context now refuses before effects. Canonical CI and focused configured AppleClang ASan/UBSan CPU checks pass. The default sanitizer preset names Linux clang-21; local macOS uses the documented fresh /usr/bin/clang override. CPU/runner requirements remain partial; continue with Plan 03. Local sanitizer log: build/phase04-tracer/plan02-asan-macos.log.

### Pending Todos

None yet.

### Blockers/Concerns

- G1 (license) is open. The public repo has no LICENSE, so default copyright applies. This does not block v1.0. Do not add a LICENSE without the owner.
- Phase 5: the xPack toolchain identity (ARMv4T, identical bytes on macOS and Linux) is [UNK]. The spike gates any committed ROM digest; the fallback is Linux digest ownership.
- Phase 1: runner image labels are verified live: `ubuntu-26.04` and `macos-26` (PR B runs 42737527955, 42737836505).
- Phase 1 follow-up: the owner-authorized digest is activated on main through merged PR #6; main CI passes. No further owner file/helper task remains.
- Phase 1 follow-up: the real-logo rule is active on main. Private inputs remain ignored and absent from public CI; its owner-local optional check remains visibly skipped there.
- Next milestone (v2.0) needs G4 (stub authoring) approval before stub work begins.

- Phase 3 is complete and merged through PR #6, with required checks green on its final head and on main. No further owner input is needed for that accepted scope.


## Deferred Items

- Before milestone close, reconcile Phase 3 Nyquist document status/taxonomy with
  its actual passing behavior evidence. The artifact remains draft; phase
  completion emits this nonblocking metadata warning. Do not fabricate probe
  results or turn it into a missing production-code claim. The summary path
  warning for the portable `~/.codex` runtime reference is a prose-path scanner
  limitation, not a missing shipped artifact.


## Session Continuity

Last session: 2026-11-10T23:59:45.464Z
Stopped at: Phase 4 plans checked, ready to execute
Resume file: None
