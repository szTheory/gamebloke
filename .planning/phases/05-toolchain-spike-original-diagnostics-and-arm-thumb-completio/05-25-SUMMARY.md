---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "25"
subsystem: testing
tags: [installed-consumer, sanitizer, coverage, source-snapshot, evidence]
requires:
  - phase: "05-24"
    provides: Current admitted required hosted evidence
provides:
  - Installed production semantic positive/broken/stop JSON and trace verification
  - Recurring offline source-snapshot and required installed-consumer guards
  - Independent final code/security/goal/coverage evidence with retained unknowns
affects: [phase-acceptance, future-device-work]
tech-stack:
  added: []
  patterns: [current post-build runner binding, explicit claim boundaries]
key-files:
  created: [tests/gates/linux_snapshot.cmake, .planning/phases/05-toolchain-spike-original-diagnostics-and-arm-thumb-completio/05-FINAL-CHECKS.md]
  modified: [tests/gates/runner_relocation.cmake, tests/gates/diagnostic_setup.cmake, scripts/linux-lane.sh, tests/diagnostics.cmake, docs/evidence-contract.md, docs/verify.md]
key-decisions:
  - "Installed evidence must use actual admitted semantics outside the source/build tree."
  - "Keep private exclusions, history, modes and cleanup under recurring synthetic helper controls."
  - "Owner guide clarity and historical unknowns remain human boundaries, not fabricated test passes."
requirements-completed: [DIAG-01, DIAG-02, DIAG-03, VERIF-04, CPU-01, CPU-02]
duration: "multi-resume; complete duration not reconstructed"
completed: 2026-11-14
status: complete
coverage:
  - id: D1
    description: Moved installed production runner retains admitted positive/broken/stop identity and exact JSON/trace
    verification:
      - kind: integration
        ref: "tests/gates/runner_relocation.cmake; required runner.relocation_admitted; 05-FINAL-CHECKS.md"
        status: pass
    human_judgment: false
  - id: D2
    description: Production CPU and exact ledger have native/compiler/sanitizer and complete original diagnostic evidence
    requirement: CPU-01
    verification:
      - kind: integration
        ref: "05-LOCAL-GOAL-REVIEW.md; 05-FINAL-CHECKS.md; 05-HOSTED-CHECKS.md"
        status: pass
    human_judgment: false
  - id: D3
    description: Owner finds the evidence guide clear and understands remaining bespoke judgments
    verification: []
    human_judgment: true
    rationale: "Guide clarity and retained unknowns require owner judgment at the existing phase boundary; automation cannot supply it."
---

# Plan25: Installed consumer and final evidence audit

All implementation and automated integration/audit work has terminal evidence.
The existing owner guide/bespoke-judgment checkpoint remains pending; this
execution summary does not accept that checkpoint or complete Phase5.

## Implementation and actual evidence

Primary source6211cb5 retains the offline source-snapshot regression in CI and
hygiene and makes installed admitted relocation required in ARM/all diagnostics.
The installed executable is moved and invoked from an unrelated directory with
explicit owned input. Actual admitted positive, same-ID broken and BIOS-none SWI
stop witnesses match exact JSON/trace and executable/input identity; missing
executable and clean companions remain required. Runtime source/manifest/index
overrides are absent. Thumb has no invented ARM installed test.

The helper test uses a synthetic two-commit repository and exercises private/
ignored exclusions, history, modes, deletions, spaced names, signals and cleanup.
Meaningful mutants must refuse. It does not access real private ROM/BIOS bytes.
The actual runner digest is captured after its admitted rebuild; the concrete
sanitizer stale-identity failure and corrected retests are preserved.

The original final checks include fresh full AppleClang ASan/UBSan, native Linux
GNU/Clang, all-row coverage and complete admitted outputs. Later consumer repair
has current focused sanitizer/refusal evidence, canonical CI and required
794/794zero-skip diagnostics. The final policy-only change has bounded exact
budget/collector controls and actual current hosted execution. No old full
matrix is relabeled as rerun after a test-only change. See05-FINAL-CHECKS.md,
05-25-COVERAGE-FIX.md and05-FINAL-CONSUMER-REPAIR.md for exact scoped receipts.

Independent code/security audits have no open implementation finding. Goal and
Nyquist reviews resolved the stale profile wording, recurring helper and
required installed-coverage findings. CPU-1..9, all19Thumb forms and all386ledger
rows retain actual semantic/stop/API evidence and explicit project policies.
The collector drivers are manual parser controls, not recurring CI coverage or
actual hosted qualification. Current actual hosted proof is separately verified
for35c7513 in05-HOSTED-CHECKS.md.

## Owner judgment and retained limits

Coverage D3 is deliberately human-only and pending. Three unclassified Phase5
probes, three descriptor-less prohibitions, eleven historical Phase4 rows and
Plan19's historical test-first record gap remain unknown. New tests do not
reconstruct chronology, invent probe results or waive rights. Formal Nyquist
compliance remains unasserted. Shared requirement statuses remain Pending until
the phase boundary despite the technical mapping in this summary.

No BIOS execution, physical hardware/controller accuracy, broad commercial-game
compatibility, display/audio/device behavior, stable package release, new project
license or Phase6 work is qualified. The one-time merge-commit recommendation
preserves producer ancestry; the owner has not approved that squash exception.

## Self-check

Reviewed source and saved evidence bindings, actual required inventories,
unchanged producer85c/input95, all722 output pins and preserved R4 artifacts are
checked. The final records descendant still needs its own hosted result. Owner
action follows technical publication; no redundant acknowledgment is needed
for the currently approved records work.
