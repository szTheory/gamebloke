---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "19"
subsystem: diagnostics
tags: [armv4t, original-source, mailbox, trace, coverage]
requires:
  - phase: 05-18
    provides: Reviewed ARM/Thumb implementation, profile5 and complete local integration
provides:
  - 244 original ARM semantic pairs and four separately classified stop witnesses
  - Source-pinned matrix production and independent raw observation controls
affects: [05-20, 05-21, 05-22, 05-23, 05-24, 05-25]
actuals:
  tokens: 116494
  tasks: 2
  commits: 14
plan_head_before: 2ad62474991899021d89fbb497a78cb738ec703b
plan_head_after: 1116fff5f3d4bb77e127ab18e82be3fc24097206
tech-stack:
  added: []
  patterns: [Stable literal case IDs, separate semantic and stop evidence, validated row-location hints]
key-files:
  created: [diagnostics/armv4t/cases-arm.S, diagnostics/armv4t/assertions.inc]
  modified: [diagnostics/armv4t/cases.json, diagnostics/armv4t/inputs.txt, cmake/BuildArmDiagnostics.cmake, cmake/CheckArmCoverage.cmake, tests/gates/diagnostics_case.cmake, tests/gates/coverage_case.cmake, src/cpu.c, tests/cpu_test.c]
key-decisions:
  - Raw semantic observations remain unregistered GUEST_STOP; no fixture identity, evidence claim or digest admission is invented.
  - Guest stops and CPU API evidence remain separate from semantic pairs; BIOS-none never claims handler continuation.
  - An internal row-index hint must match the stable literal CASE_ID and retain source/toolchain authority checks.
requirements-completed: []
requirements-contributed: [DIAG-01, DIAG-03, CPU-01]
coverage:
  - id: D1
    description: Original ARM matrix builds every selected semantic companion and stop image with exact output receipts.
    verification:
      - kind: integration
        ref: build/phase05-plan19/final-matrix-build-v2.log
        status: pass
    human_judgment: false
  - id: D2
    description: Every raw pair and named stop has independent identity, mailbox, time and trace observations with clean-first controls.
    verification:
      - kind: integration
        ref: build/phase05-plan19/final-raw-gate-v2.log
        status: pass
      - kind: integration
        ref: build/phase05-plan19/final-ledger-controls-v2.log
        status: pass
    human_judgment: false
duration: 160m21s
completed: 2026-11-13
status: complete
---

# Phase 05 Plan 19: Original ARM Diagnostic Matrix Summary

**244 original ARM pass/fail pairs and four honest stop witnesses execute through the production runner, with pinned build outputs and independent trace controls.**

## Results

The final matrix is pinned to producer source `6eca7e5`. All492 real raw executions report GUEST_STOP with null fixture ID and matching actual ROM/claim digests:244 semantic pass,244 semantic fail with the same exact case ID, and four pending witnesses for NV, SWI imm24, reserved UND and absent coprocessor. SWI/UND/coprocessor traces identify the actual opcode, exception kind and instruction address, ending before vector fetch. NV proves the fetched refused encoding without inventing an exception. Internal exception banks/status remain explicitly lower-boundary CPU API evidence.

The gate checks fresh progress/time, complete ordered traces, exact odd LDR/LDM-PC refill and target execution, and25 clean-first negative controls. The canonical result checker independently refuses every raw document with exactly its expected null-fixture/empty-evidence admission findings. This is neither manifest-backed PASS nor fixture admission.

The357-row ledger retains334 inventory targets,356 pending admission rows, one previously source-verified smoke pair,244 semantic ARM producers, four stop producers and19 ARM API-only rows. CPU-1..6/8/9 and policy companions remain bound to relevant source cases; Thumb execution/admission remains subsequent work. No requirement is marked phase-complete here.

## Verification and receipts

All paths are repository-relative, ignored local receipts; no private content was accessed.

| Command | Final result |
| --- | --- |
| `cmake -DSRC=. -DOUT=build/phase05-matrix/arm -DTOOLCHAIN_ROOT=build/toolchains -DMODE=all -DISA=arm -P cmake/BuildArmDiagnostics.cmake` | Exit0;492 exact outputs,244 pairs/four stops; per-case identity, v4T attributes, original-object maps and exact-name signature scan.65.11s under `cmake -E time`; `build/phase05-plan19/final-matrix-build-v2.log`. |
| `cmake -DCASE=arm-pre-admission -DSRC=. -DBIN=build/ci -DARTIFACT_ROOT=build/phase05-matrix/arm -P tests/gates/diagnostics_case.cmake` | Exit0; all492 executions and25 controls;28.61s. `build/phase05-plan19/final-raw-gate-v2.log`; actual JSON, checker refusals, traces, controls and summary under `build/ci/diagnostics-results/arm-pre-admission/`. |
| `cmake -DCASE=ledger-controls -DSRC=. -DBIN=build/phase05-coverage -P tests/gates/coverage_case.cmake` | Exit0; strict draft/final/admission, omitted source/policy/flags/edge cases, wrong companions, pending refusal and four invalid-hint controls;63.16s. `build/phase05-plan19/final-ledger-controls-v2.log`. |
| Standalone `MODE=case CASE_ID=525943575` under `build/phase05-spike/plan19-final-case` | Exit0; normal ID-lookup fallback remains usable. Literal `cmp` proves both companions equal the matrix outputs on this same host;3.84s. `build/phase05-plan19/standalone-fallback.log`. This is not cross-host evidence. |

Canonical runners were rebuilt against the corrected core (`recovered-runner-build.log`). The separately owned [CPU correction report](05-19-CPU-MULTIPLY-FIX.md) records genuine old-predicate RED plus focused native/AppleClang ASan/UBSan GREEN. Whitespace and final tree/staged hygiene pass; no tracked deletion or unfinished implementation stub was introduced. No new full inventory or hosted run is claimed.

## Commits

Complete measured range from original dispatch base `2ad6247` through task head `1116fff`:

| Commit | Work |
| --- | --- |
| `1738a78` | Initial matrix-output RED test commit |
| `210d5c5` | Selected ARM semantic builder and original assertion source |
| `7dcbaf0` | Full original ARM source ledger |
| `fb74c25` | Complete ARM matrix builder |
| `2482324` | Pending stop witnesses and ledger boundaries |
| `2140faf` | Refuse cases without actual producers |
| `ad58984` | Bind coverage authorities to producer inputs |
| `8fa5ecf` | Reuse verified parent input/tool authority for children |
| `6a9d869` | Pin concrete policy source edges |
| `53785c3` | Accept masked-only assertion producers |
| `1f83bcc` | Parent-assigned signed long-multiply core/native-fixture correction |
| `a8071f4` | Repair source setups and validate row-location hints |
| `6eca7e5` | Branch around LDM-PC embedded data |
| `1116fff` | Complete raw observation and trace gate |

## Deviations and recovery

- **Rule1, source setup/oracle fixes:** Real execution exposed System-mode SPSR/S-bit accesses, nonzero reserved PSR fields, assembler `#0` shift aliases, an LDR-PC pointer error, and a banked-destination observation error. Cases now establish valid privileged state and restore the caller's System stack. Reserved x/s writes preserve zero; LSR/ASR `#32` forces architectural shift_imm=0. GBATEK3.06 independently specifies odd LDRH as a32-bit ROR8: `ROR32(0x3344,8)=0x44000033`, rather than a16-bit byte swap. These fixes passed64 isolated real-assembler companion runs before producer commit.
- **Rule1, core bug:** SMULL/SMLAL original signed arithmetic exposed inverted bit22 interpretation. The parent assigned only CPU/native-test files to a separate fixer; no expected result was calibrated to the emulator. Both signed and unsigned high-bit contrasts now pass.
- **Rule1, trace-discovered source bug:** A whole-matrix traced sweep found exactly one remaining failure: clean LDM-PC executed its embedded target word after assertions. The missing branch was fixed, both companions checked, then the producer matrix rebuilt. Failing trace/build receipts remain in `build/phase05-plan19/trace-sweep/`, `ldm-data-fallthrough-refusal.log` and `ldm-fallthrough-build-receipt.json`.
- **Rule1, gate:** Replace unsupported CMake `{40}` regex repetition with explicit length/hex checks. Preserve the canonical checker's intentional raw-admission refusal instead of changing its authority contract.
- **Bounded builder efficiency:** A measured late-row lookup took0.755s versus0.014s for direct extraction plus literal-ID validation. The parent-authorized internal hint retains all identity checks and standalone fallback; invalid hints refuse before output mutation. These are isolated lookup measurements, not emulator throughput or a controlled full-build comparison.

The daemon restart lost the original build's transport exit code, not its OS work or commits. Its completed492-output receipt was independently checked and preserved as `build/phase05-plan19/preserved-53785-build-receipts.json`; `daemon-recovery.json` records the unknown transport. Subsequent rebuilds were required by demonstrated producer source corrections. Both fresh aggregate commands returned exit0; the first took76.45s and the final65.11s. Original failures and recovery records were retained.

## TDD Gate Compliance

The original RED commit exists, but its preserved `baseline-builder-red.log` is empty and no original `tdd-red-evidence`/`RED_EVIDENCE_OK` artifact was found. Task2 has no separate RED commit in the original range. Formal original RED compliance cannot be reconstructed from commit names. Actual source failures, subsequent GREEN runs and clean-first controls are preserved; they do not retroactively supply the missing historical gate record. Parent review owns this recorded process deviation and shared defect bookkeeping.

## Limits and handoff

No new digest admission, manifest row, license/dependency, ROM upload, private ROM/BIOS use, hosted run, publication, release or Phase6 work occurred. Two existing smoke rows and provenance remain unchanged. Current unpredictable-policy, placeholder timing/latch and BIOS/device limits remain. Cross-host literal byte comparison and final admission belong to Plan21; later integration and independent review remain parent-owned. The earlier decoder logic-review item and new multiply correction's human-review item remain for existing phase acceptance, with no human-review claim here.

Shared STATE/ROADMAP/requirements/config/routing and owner dirty files were preserved. The parent performs shared bookkeeping, independent review and routing restoration at the phase boundary; this executor did not begin Plan20.

Actuals use465,973 realized committed diff characters/4, rounded up to116,494 tokens, and14 commits measured from the persisted original plan ledger. Duration spans original dispatch `2026-11-13T02:27:56Z` through final task head receipts `05:08:17Z`, including daemon/recovery elapsed time; it is not active-model runtime or a cost benchmark. Metrics are in `build/phase05-plan19/final-metrics.json`.

## Self-Check: PASSED

All ten committed implementation paths and this summary exist; all14 commits in the measured range are ancestors of HEAD. Final aggregate/raw/ledger receipts identify the recorded producer source and counts, both planned commands returned exit0, and same-host standalone/matrix companions compare equal. `build/phase05-plan19/self-check.json` records the checks. Task source changes are committed; parent-owned dirty/untracked files remain intact. Final tree and staged hygiene pass.
