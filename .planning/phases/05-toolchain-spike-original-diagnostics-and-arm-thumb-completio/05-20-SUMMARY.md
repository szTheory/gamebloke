---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
plan: "20"
subsystem: diagnostics
tags: [armv4t, thumb, original-source, mailbox, trace, coverage]
requires:
  - phase: 05-19
    provides: Reviewed ARM producer, pinned GNU toolchain and raw observation infrastructure
provides:
  - 117 original Thumb semantic pairs and eight separate source-built stop witnesses
  - Actual separate BL retirement, refill and intermediate/final LR/PC observations
  - Strict official-format crosswalk with opcode, omission and boundary controls
affects: [05-21, 05-22, 05-23, 05-24, 05-25]
actuals:
  tokens: 105345
  tasks: 2
  commits: 7
plan_head_before: 6d5850f13db46f51d635d0bec0fab6c743a35f4d
plan_head_after: 5406f70bb649262478cd38a924fedbdf3a53c66d
tech-stack:
  added: []
  patterns: [Literal stable IDs, independently checked labeled opcodes, guest data-read state captures, separate semantic and stop evidence]
key-files:
  created: [diagnostics/armv4t/cases-thumb.S]
  modified: [diagnostics/armv4t/assertions.inc, diagnostics/armv4t/cases.json, diagnostics/armv4t/inputs.txt, cmake/BuildArmDiagnostics.cmake, cmake/CheckArmCoverage.cmake, tests/gates/diagnostics_case.cmake, tests/gates/coverage_case.cmake]
key-decisions:
  - Preserve existing IDs while correcting the official nineteen-format crosswalk and adding missing legal forms.
  - Preserve raw GUEST_STOP with null fixture identity; semantic pass/fail never supplies admission authority.
  - BL captures use guest stores followed by actual data reads; write trace return values are not stored-data evidence.
requirements-completed: [DIAG-01, DIAG-03, CPU-02]
coverage:
  - id: D1
    description: Original Thumb programs and shared ARM/Thumb assertions cover all nineteen official formats and named operation/list boundaries.
    verification:
      - kind: integration
        ref: build/phase05-plan20/frozen-matrix-build.log
        status: pass
      - kind: integration
        ref: build/phase05-plan20/final-acceptance.log
        status: pass
    human_judgment: false
  - id: D2
    description: ISA-bounded producer and exact input list preserve source/tool/ELF/map/signature authority and normal lookup fallback.
    verification:
      - kind: integration
        ref: build/phase05-plan20/frozen-matrix-build-receipt.json
        status: pass
      - kind: integration
        ref: build/phase05-plan20/fallback-build-receipt.json
        status: pass
      - kind: integration
        ref: build/phase05-plan20/stale-source-refusal.json
        status: pass
    human_judgment: false
  - id: D3
    description: Literal IDs, operation encoding crosswalk, pending boundaries and omission/mismatch controls reject silent coverage loss.
    verification:
      - kind: integration
        ref: build/phase05-plan20/final-ledger-controls.log
        status: pass
    human_judgment: false
  - id: D4
    description: Raw consumer asserts every pair/stop and separate BL halves; actual CPU API groups cover unmasked IRQ entry without handler execution.
    verification:
      - kind: integration
        ref: build/phase05-plan20/final-acceptance-receipt.json
        status: pass
      - kind: integration
        ref: build/ci/diagnostics-results/thumb-pre-admission/summary.txt
        status: pass
    human_judgment: false
duration: 35m00s
completed: 2026-11-13
status: complete
---

# Phase 05 Plan 20: Original Thumb Diagnostic Matrix Summary

**117 original Thumb pass/fail pairs and eight honest stop witnesses execute through the production runner, with separately observed BL halves and independently checked opcode forms.**

## Results

The frozen producer is `5406f70bb649262478cd38a924fedbdf3a53c66d`. Its 242 outputs yield 117 semantic passes, 117 deliberately broken failures with equal stable case IDs, and eight pending stops. Every result remains raw GUEST_STOP with null fixture ID and independently checked actual ROM/claim digests. The canonical result checker must refuse each raw document with exactly its two existing admission findings; no fixture identity or evidence claim is fabricated.

The official DDI0029G Figure 1-6 crosswalk is now explicit: signed byte/halfword register loads are format8, immediate byte transfers format9, halfword immediate transfers format10, SWI format17, unconditional branch format18, and both BL halves format19. ASR, signed positive/negative loads, high-register operand combinations, PC sources/destinations, mixed ARM/Thumb BX, carry inputs, shift-count boundaries, odd transfers, stack/list edges and maximum legal lists have concrete original programs. Existing IDs remain fixed; 36 additions use literal nonzero IDs and explicit order. Every labeled tested instruction has real GNU opcode-field validation, independently checked against its format/operation.

The current ledger has 385 rows, 362 inventory targets, 384 pending rows and the one unchanged source-verified smoke pair. ARM retains 236 semantic producers, four stops and 19 API rows. Thumb has 117 semantic producers and eight separate stops: empty PUSH, POP, STMIA and LDMIA, STMIA base-middle/base-last project refusals, reserved condition E, and SWI imm8. Pending admission is intentional and does not mean source execution was skipped.

All three BL rows expose actual prefix/suffix instruction addresses, distinct retirement counts/cycles, prefix sequential fetch and suffix nonsequential/sequential refill. Guest stores followed by actual LDR data observations establish prefix LR=P+4, captured PC=P+8, final LR=(S+2)|1 and captured target PC=T+6. At the frozen source, one clean receipt records P=0x0800011A, S=0x0800014C, retirements9/32 and data reads 0x0800011E, 0x08000122, 0x0800014F, 0x08000156. Broken companions fail the first prefix assertion; their suffix/final captures correctly remain null. Six machine-readable BL receipts and complete traces live beside the raw results.

CPU-7 names actual `cpu_test thumb_bl` and `cpu_test thumb_control` evidence, including `test_thumb_bl_irq_boundary`, explicit CPSR.I clearing before API injection, saved Thumb state and stopping without a handler. It remains non-CLI API evidence. Thumb SWI executes actual opcode0xDFA7 and stops before vector fetch; this does not claim BIOS service or handler continuation.

## Commands and receipts

All evidence paths are repository-relative ignored local artifacts. No private ROM or BIOS was accessed.

| Command | Observed outcome |
| --- | --- |
| `cmake -DSRC=. -DOUT=build/phase05-matrix/thumb -DTOOLCHAIN_ROOT=build/toolchains -DMODE=all -DISA=thumb -P cmake/BuildArmDiagnostics.cmake` | Exit0; 117 pairs/eight stops/242 images, 31.13s. `build/phase05-plan20/frozen-matrix-build.log` and `frozen-matrix-build-receipt.json`; full per-image authority under `build/phase05-matrix/thumb/proof/` and `cases/`. |
| `cmake -DCASE=thumb-pre-admission -DSRC=. -DBIN=build/ci -DARTIFACT_ROOT=build/phase05-matrix/thumb -P tests/gates/diagnostics_case.cmake && build/ci/cpu_test thumb_bl && build/ci/cpu_test thumb_control` | Every command exit0; combined21.57s. `build/phase05-plan20/final-acceptance.log` and `final-acceptance-receipt.json`. All242 raw observations plus65 clean-first controls pass. |
| `cmake -DCASE=ledger-controls -DSRC=. -DBIN=build/phase05-plan20/final-ledger -P tests/gates/coverage_case.cmake` | Exit0; 220.44s. `build/phase05-plan20/final-ledger-controls.log` and matching receipt. Preserves35 prior ARM source controls, adds24 Thumb controls, and retains23 other refusal observations plus hint/output-root checks. |
| `cmake -DSRC=. -DOUT=build/phase05-spike/plan20-fallback -DTOOLCHAIN_ROOT=build/toolchains -DMODE=case -DISA=thumb -DCASE_ID=631794229 -P cmake/BuildArmDiagnostics.cmake` | Exit0;4.31s, no index hint. Literal `cmp` of both standalone companions against the matrix outputs succeeds on this host. `build/phase05-plan20/fallback-build.log` and matching receipt. This is not cross-host comparison. |
| Same raw gate with a deliberately stale generated receipt source revision | Expected nonzero/stale producer-input refusal before execution. Original receipt restored exactly. `build/phase05-plan20/stale-source-refusal.json` and `.log`. |
| `sh scripts/hygiene.sh --tree`, `sh scripts/signature-scan.sh --tree`, staged hygiene, `git diff --check` | Pass. Final tree logs are `build/phase05-plan20/final-hygiene.log` and `final-signature.log`. Generated exact-name ROM scans are separately enforced by the builder. |

The65 runtime controls comprise13 common identity/status/semantic/time/trace controls,16 named-stop context controls and36 BL controls. Each BL mutation requires its concrete expected refusal, including separate retirements, addresses/refill slots and all four register-data captures. The24 new ledger controls reject omitted legal forms, missing source/probes/labels, wrong or weakened opcode declarations, incorrect format, missing named vectors/carry setup, exclusion of the broken flag bit from its mask, missing BL actual reads, generic IRQ links and reordered stable IDs.

The full ledger check is a concrete Plan24 cost candidate:220.44s locally while other focused checks also ran. This is not hosted timing or emulator throughput. No hosted job ran and no180s hosted limit, dependency, cache or concurrency setting changed. Broad CI/compiler/sanitizer inventories remain the later integration plans' work; this source-only plan changes no CPU/runner C code and reused the existing production runner.

## Commits and source sequencing

| Commit | Scope |
| --- | --- |
| `bc2b424` | Task1 executable producer RED contract |
| `34c8507` | Selected Thumb producer, original wrapper/shared assertion path and input binding |
| `c5bbb43` | Actual Thumb1 low-register assertion assembler correction |
| `a080547` | Task1 expanded matrix and official format crosswalk |
| `71a5804` | Task2 executable raw-boundary RED contract |
| `c8ab454` | Independent operation fields and BL actual-data captures |
| `5406f70` | Completed consumer/controls, maximum-list cases and explicit API-only producer refusal |

First actual clean/broken tracer evidence follows `c5bbb43`. The first aggregate at `a080547` built113 pairs/eight stops/234 images; intermediate source `c8ab454` verified the same count and initial raw gate. These receipts are historical after the four maximum-list additions. The final242-image build and acceptance chain identify `5406f70`. Original Plan19 historical and corrected ARM receipts were not rewritten or rebuilt; changed shared producer inputs intentionally make earlier ARM receipts stale for future admission.

## TDD Gate Compliance

Both tasks have genuine narrow old-behavior RED before their implementations, real CTest JUnit reports, fresh report timestamps, saved semantic assessments and installed classifier verdict `RED_EVIDENCE_OK`. Exact artifacts are `build/phase05-plan20/tdd-task1/red-evidence.json`, `red.xml`, `red-classifier.json` and the corresponding `tdd-task2/` files. Task1 failed because the stable Thumb case lacked runnable source; Task2 failed because the consumer did not support the built Thumb matrix. Test commits precede their feature commits in ancestry.

The first task1 wrapper attempt had an incorrect working directory. Its report parsed, but semantic inspection rejected it as setup failure; `invalid-setup-evidence.json` and `invalid-setup-console.log` preserve that rejection. The corrected rerun supplied valid RED before producer implementation. Task1's actual CTest GREEN is `green-v2.xml`; the frozen builder/acceptance receipts establish the expanded behavior. Task2's exact underlying consumer command supplies current GREEN in the final acceptance receipt. No synthetic JUnit cases or execution counts were authored.

The strict builder requires source inputs already exist in a commit, so provisional producer commits necessarily precede their pinned build receipts. The first provisional source then revealed a real assembler error, corrected in `c5bbb43`; neither provisional commit alone is acceptance evidence. This sequencing is recorded explicitly. The original Plan19 formal reporter gap remains open and is not repaired retroactively here.

## Deviations from Plan

- **Rule2 — incomplete draft inventory:** Primary local DDI0029G/DDI0100I references establish the corrected nineteen-format mapping and missing legal ASR/signed/high/PC/list forms. Existing IDs remain; independent opcode/source/runtime controls protect the additions.
- **Rule2 — shared assertion/control support:** Parent explicitly extended ownership to `diagnostics/armv4t/assertions.inc`. ARM behavior is preserved; Thumb uses register-preserving assertions with reachable failure branches. Four focused real ARM clean/broken writer runs and Thumb tracers pass. The narrowly necessary `coverage_case.cmake` extension retains all ARM controls and replaces its obsolete assumption of an unimplemented Thumb row with an explicit API-only/no-CLI producer refusal.
- **Rule1 — source authoring corrections:** Actual GNU/traced runs exposed low-register MOV syntax, a stray pending Thumb-function annotation affecting a PC literal, alignment padding modifying captured flags, and the preserved checked-BX alignment requirement. Original source was corrected; expectations were not calibrated to the core. DDI0100I-defined LDM base-in-list and CMP PC-source forms use explicit Thumb encodings where GNU unified syntax rejects the otherwise defined form. Complete disassembly and negative/clean receipts remain under `build/phase05-plan20/representative-v1/`, `debug-tracers/`, timestamped debug directories and tracer logs. Existing trace paths refuse overwrite; debug reruns now use distinct roots and the initial trace-write refusals remain recorded.

No CPU/native-test change, new library, rights decision, authentication gate, fixture admission, binary upload, remote execution, publication or Phase6 work occurred. No implementation stub or skipped planned verification remains.

## Handoff and metrics

Parent owns independent review, shared STATE/ROADMAP/requirements/config/routing/WINDOWS updates and phase acceptance. The requirement IDs above associate the completed local deliverables with this plan; shared requirement status was not changed. Cross-host literal byte comparison and digest admission remain Plan21. Human decoder/multiply review gates and the standing BIOS/device/timing/qualification limits remain. No human review, hardware accuracy, broad game compatibility, release or cross-host equality is claimed.

Actuals measure421,379 characters of the realized committed implementation diff divided by4, rounded up to105,345; these are not model/billing tokens. Seven commits are measured from the persisted authoritative dispatch base through the recorded task head. Eight implementation paths changed. Duration uses saved dispatch `2026-11-13T05:59:34.396785Z` through acceptance/self-check preparation `2026-11-13T06:34:34.805861Z`; `build/phase05-plan20/final-metrics.json` preserves epochs and method. Owner dirty/untracked files remain intact. The parent acts next for independent review and shared close-out; no owner action is needed for this local plan handoff.

## Self-Check: PASSED

All eight implementation paths and this summary exist; all seven measured task commits are ancestors of HEAD. Independent self-check re-read all242 ROMs, source-program/ROM/receipt/result digests and case IDs, complete traces and retirement totals, six BL receipts and all planned command receipts. Coverage metadata classifies all four deliverables with passing automated evidence and no errors. `build/phase05-plan20/self-check.json` records the checks. No task-owned code change remains uncommitted; shared owner files are preserved.
