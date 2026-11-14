---
phase: 05
plan: "19"
type: review-correction
status: verified-awaiting-independent-review
producer_commit: 4c7449e87b483cf4c26cda22c83d951a3cdeaa11
base_commit: a1f2eb2
findings_addressed: [WR-01, WR-02, WR-03, WR-04]
requirements_completed: []
---

# Plan 19 ARM diagnostic coverage corrections

All four reviewed source defects have corrections and executable regression controls. The frozen producer builds 236 original ARM semantic pairs and four stop witnesses; all 476 raw executions pass the independent observation gate. Independent parent review remains next. This report supplements the original SUMMARY and REVIEW without rewriting their historical counts or receipts.

## Corrections and architectural basis

| Finding | Correction and independent expectation | Verification |
| --- | --- | --- |
| WR-01 | Stable case `507654410` now executes explicit opcode `0xE082031F`: `ADD r0,r2,pc,LSL r3`, with Rn=r2=3, Rm=R15 and Rs=r3=1. Operand2 therefore reads A+12; result is `2*(A+12)+3`. ADR independently recovers instruction address A, then ordinary arithmetic forms that expectation before subtraction. Correct difference is zero; an A+8 interpretation gives `0xFFFFFFF8`, the deliberately broken expectation. | Checker requires the contiguous operand/setup/arithmetic/assertion sequence. Real GNU disassembly must contain the exact opcode and operand form. Mutations restore the old Rn-PC immediate form, change shift encoding, and change oracle +12 to +8; each is refused. |
| WR-02 | All eight LDM/STM IA/IB/DA/DB `normal-no-writeback` programs omit `!`, retain their transfer-value assertions, snapshot r4 into r6 and compare the final r4 with r6. All eight metadata fields are false. | Each real assembled opcode is checked for AL/class/P/U/L/Rn/register-list fields with W=0 and S=0. Source controls independently reject `!` on each row, a changed base comparison and true writeback metadata. |
| WR-03 | Mode 2 word/byte inventory retains force-user T forms. Mode 3 halfword/signed inventory has only pre-no-writeback, pre-writeback and post-index. Eight invalid duplicate rows and their CPU-5 companion references are removed. No remaining ID is reassigned and no new case is invented. | Each of the eight illegal family/form combinations is separately reintroduced with an ordinary post-index program and refused. The existing CPU unpredictable-instruction handling is untouched. |
| WR-04 | Cases `293321832` and `292551648` now each execute zero, one, sign-bit and all-ones operands with incoming C=0 and C=1. The first uses immediate LSL #0; the second uses register ROR with r2=0. Each of the eight operations per program snapshots CPSR before comparing result and NZCV; N=operand bit31, Z=(operand==0), C=incoming and V=0. Each result's broken expectation flips bit0; each broken flag expectation flips C. Flags are reseeded before every operation. | Checker binds each executed input/setup/instruction/result/flag sequence. Builder checks eight actual matching operation opcodes per image (`E1B00001` or `E1B00271`, GNU alias `rors`). Controls replace each named vector, erase the C=0 setup, reverse a result assertion and reverse a flag assertion for each program. |

No second assembler was introduced. The checker intentionally protects these narrow reviewed source contracts; the selected GNU assembler and objdump remain the encoding authority. The real production runner observes the assertions. Broken companions stop at their first failed assertion; a broken run does not claim to have reached every later broken assertion.

Primary local references used: `build/phase04-research/arm/ARM-DDI0100I.txt`, A5.1 shifter rules, A5.2.1 pp. A5-18–19 (Mode 2 P/W and T), A5.3.1 pp. A5-33–34 (Mode 3 P=0 requires W=0), and `build/phase04-research/arm/ARM7TDMI-DDI0029G.txt` §6.5/Table 6-4 pp. 6-7–8 (register-controlled data operation sequencing). The accepted instruction-specific PC+12 rule and boundary vectors remain recorded in `05-CPU-NOTES.md`, CPU-3 and the full-coverage crosswalk. General PC+8 is not substituted for the operand-specific rule.

Removed IDs: `348959853`, `386420797`, `395853884`, `423103475`, `427234685`, `434308478`, `492687423`, `523744941`. Exact old rows remain in ignored receipt `build/phase05-plan19/review-fix/removed-mode3-rows.json` and in Git history. Existing source audit documents, licenses, notices and original Plan 19 receipts remain historical and unchanged.

## Current inventory

| Observation | Historical Plan 19 | Corrected current producer |
| --- | ---: | ---: |
| Ledger rows | 357 | 349 |
| ARM/Thumb inventory targets | 334 | 326 |
| Pending rows | 356 | 348 |
| Source-verified smoke rows | 1 | 1 |
| Original ARM semantic pairs | 244 | 236 |
| Separate named stop witnesses | 4 | 4 |
| ARM API-only rows | 19 | 19 |
| ARM matrix output images | 492 | 476 |

Checker semantic totals derive from its architecture-defined inventory, excluding the named NV stop. Builder and consumer totals derive from the validated current producers instead of repeating historical counts. Existing stop/API counts and boundaries remain constrained. `build/phase05-plan19/review-fix/current-counts.json` records the source revision, IDs and current counts.

## Evidence and exact commands

All receipt paths below are repository-relative, ignored local evidence under `build/phase05-plan19/review-fix/` unless otherwise stated.

1. Before source corrections, the new checker executed against the old ledger and refused all four semantic mismatch classes. Original output: `old-source-red.log`. Exact command: `cmake -DMODE=draft -DMATRIX=diagnostics/armv4t/cases.json -P cmake/CheckArmCoverage.cmake`. Corrected source GREEN is in `source-green.log`. The old full ledger is preserved in `old-cases.json`.
2. `python3 build/phase05-plan19/review-fix/tracers.py` assembled and linked the eleven changed programs using the actual pinned selected GNU tools, repaired their headers and ran both companions with the production runner. `tracers.log` and `tracers/summary.json` report 22 real executions, zero failures; individual results/traces and disassembly are under `tracers/`. These deliberately labeled debug tracers reuse existing startup objects and are not final producer receipts. `tracer-opcodes.json` independently records the PC and eight block opcodes for both variants.
3. `cmake -E time cmake -DSRC=. -DOUT=build/phase05-matrix/arm -DTOOLCHAIN_ROOT=build/toolchains -DMODE=all -DISA=arm -P cmake/BuildArmDiagnostics.cmake` returned exit 0 at frozen source `4c7449e87b483cf4c26cda22c83d951a3cdeaa11`: 236 pairs, four stops, 476 outputs; 48.0467 seconds. `final-matrix-build.log` and `build/phase05-matrix/arm/proof/build-receipt.json` retain exact source/input/tool identity, v4T attributes, original-only objects/maps and signature checks. This is the sole full matrix rebuild for this correction batch.
4. `cmake -E time cmake -DCASE=arm-pre-admission -DSRC=. -DBIN=build/ci -DARTIFACT_ROOT=build/phase05-matrix/arm -P tests/gates/diagnostics_case.cmake` returned exit 0; 25.5305 seconds. `final-raw-gate.log`, plus individual JSON, traces, canonical checker refusals, controls and summary under `build/ci/diagnostics-results/arm-pre-admission/`, establish 236 clean semantic results, 236 deliberately failing results with exact IDs and four named pending stops. All remain raw GUEST_STOP with null fixture identity. The canonical checker must still report its exact two unregistered-admission findings; 25 independent clean-first observation controls, stop contexts and loaded-PC trace checks remain intact.
5. `cmake -E time cmake -DCASE=ledger-controls -DSRC=. -DBIN=build/phase05-coverage -P tests/gates/coverage_case.cmake` returned exit 0; 108.62 seconds. `final-ledger-controls.log` records all pre-existing controls plus 35 new semantic mutation controls: three PC, ten block source/metadata/base, eight Mode 3 and fourteen boundary vector/carry/result/flag controls. Every mutation begins from the successful clean corrected ledger. Validated index hints, wrong-ID/range/malformed/external-hint refusals, source and policy companions, pending/final/admission behavior remain checked. The longer control path reflects distinct added regressions; this is not an emulator throughput benchmark.
6. `git diff --check`, `sh scripts/hygiene.sh --tree` and staged hygiene passed for the producer commit. `final-hygiene-tree.log` records final tree hygiene. No tracked deletion was committed. Production edits are limited to the five explicitly owned diagnostic/checker/builder/gate files.

Before the aggregate output and raw summary were regenerated, the prior receipt and summary were copied to `prior-matrix-receipt.json` and `prior-raw-summary.txt`. Earlier matrix/failure/recovery outputs in `build/phase05-plan19/` were not rewritten; original 244-pair reports remain history, not current evidence.

## Formal TDD reporting limit

The correction checks produced actual old-source failures before the source fixes, but their CMake console output is not a supported format-based TDD report. An honest replay against retained `old-cases.json` is recorded in `red-evidence.json`; the actual classifier output in `red-classifier.json` is `INVALID_RED`, `invalid_record`, `Unsupported report format`. It reports no recognized tests. This report does not claim `RED_EVIDENCE_OK`, a supported test-report RED commit, or retroactive closure of the original Plan 19 TDD process deviation. The successful real assembler/runtime gates and clean-first regression controls establish corrected behavior; they do not replace the formal reporting contract. Shared process-deviation bookkeeping remains parent-owned.

## Limits and handoff

No CPU/native-test files, admission manifest, fixture digest, NOTICE/license, dependency, private content, shared planning state, routing, WINDOWS, ROADMAP or requirements were changed. Existing runners already include the earlier core correction; CPU tests were not rerun because this batch changes no CPU code. No new core defect was observed. No new stub or security boundary was introduced.

This is local original-source diagnostic evidence only: no cross-host byte comparison, hardware timing, BIOS/game compatibility, physical qualification, fixture admission, upload, hosted CI run, publication, release, Plan 20 or Phase 6 claim follows. Plan 21 retains cross-host comparison/admission ownership. The parent acts next: independently review the five-file correction and this evidence, then perform shared close-out. No owner action is needed for this local handoff.

## Self-check

Producer commit exists and is an ancestor of HEAD; all five production paths and this report exist. Final matrix receipt source matches the frozen producer, current counts satisfy `2*236+4=476`, no remaining ID changed, and all three final aggregate commands returned exit 0. Shared owner/root dirty and untracked paths remain intact. Formal TDD reporting remains explicitly limited as above.
