---
phase: 05
plan: "20"
status: corrected-awaiting-independent-review
source_commit: b1ce937f322865e3fbfbcd8873283f485a4a2644
dispatch_base: 241ef019f73c5c574c1ed59bb269bf094f12ce84
findings_addressed: [CR-01, WR-01, WR-02]
files_changed: [cmake/CheckArmCoverage.cmake, diagnostics/armv4t/cases.json, tests/gates/coverage_case.cmake]
receipt_root: build/phase05-plan20/review-fix
---

# Plan 20 review corrections

The coverage checker now binds the named Thumb operand/list forms to architecture-derived encoding bits. Immediate and register shift witnesses distinguish carry computation from preservation, and a new fixed-ID ROR case covers zero and non-multiple counts above 32. The frozen source produces 118 semantic pairs and eight stop witnesses; every production observation and all required local gates passed. Independent re-review and shared close-out remain with the parent.

## Corrections and independent expectations

| Finding | Correction | Evidence |
| --- | --- | --- |
| CR-01 | Format5 ADD/CMP/MOV derive H1/H2 from high/high, low/high and high/low forms. PC-source/destination forms also require the actual R15 selector. High-only BX requires its selector, while the ordinary BX form accepts either legal Rm selector. Format14 derives LR/PC bit8 from ordinary included/excluded forms and both instructions in the PUSH/POP companion. Empty and maximum lists retain their concrete list boundary, including named format15 stop and maximum-list witnesses. Masks must contain the required bits; supplied opcode values cannot define the expected form. | `old-behavior.json`, `old-coordinated-1800200004.json`, `old-coordinated-785410767.json` and their logs under the receipt root record old-checker false acceptance. The full ledger records 70 coordinated source/opcode and weakened-mask refusals plus a positive ordinary low-register BX control. |
| WR-01 | Existing IDs742650290 and1800200023 now execute C-in0 with bit31 set before their retained C-in1 immediate LSR/ASR32 vectors. IDs1800200027/28 retain all original counts and add register-count0 with C-in0/sign-bit1 and C-in1/sign-bit0. Both incoming carry values are therefore checked independently of operand bit31. Every flag mutation flips asserted bit29. | Ten real GNU clean/broken runs cover these four cases plus the new ROR case. Actual labeled opcode fields and observations are in `tracer-observations.json`. Four isolated C-only fault controls show that stale or sign-derived carry fails the new guest assertions. |
| WR-02 | New literal ID1800200037 covers ROR count0 with both carry inputs, then count33 with contrasting operand bit0/C-in. Count33 of0x80000001 with C-in0 yields0xC0000000/C1; count33 of0x80000000 with C-in1 yields0x40000000/C0. Existing ID736225106 and its count32/64 vectors remain unchanged. | Literal source/result/NZCV/opcode tuples are independently enforced. The full ledger includes 35 new shift/ROR source, vector, mask, encoding and probe-omission refusals. Two additional isolated C-only faults establish count0 preservation and count33 carry sensitivity. |

The independent source is DDI0100I Thumb ASR(1)/(2), A7-15/17; LSR(1)/(2), A7-68/70; ROR, A7-92, and DDI0029G Figure1-6 for form encoding. Local primary text remains at `build/phase04-research/arm/ARM-DDI0100I.txt` and `ARM7TDMI-DDI0029G.txt`. The expected arithmetic and flags were derived from those operation rules, never calibrated against core output. N/Z follow the result, C follows the specified bit or old C, and V remains the explicitly seeded zero.

All previous Thumb IDs and ordering remain, followed by the new literal ID. Existing ARM and API rows compare identically with the dispatch base. The wrapper, shared ARM/Thumb assertion writer, CPU, native tests and diagnostic raw consumer were not changed. The checker changes remain specific to existing crosswalk boundaries and named witnesses.

## Actual verification

All paths below are repository-relative ignored artifacts beneath `build/phase05-plan20/review-fix/` unless a full path is given. Each command receipt contains the actual argument vector, exit code, UTC timestamps and elapsed wall time; corresponding logs are retained.

| Command | Outcome and receipt |
| --- | --- |
| Five `cmake -DSRC=. -DOUT=build/phase05-spike/plan20-review-fix-<id> -DTOOLCHAIN_ROOT=build/toolchains -DMODE=case -DISA=thumb -DCASE_ID=<id> -P cmake/BuildArmDiagnostics.cmake` commands, followed by clean and broken `build/ci/gamebloke-run --rom ... --cycles 1000 --trace-mask 7 --trace-capacity 4096 --trace-output ...` runs | All five builds and ten real observations passed. Successful build receipts use `tracer-build-v2-742650290.json` and `tracer-build-v3-<id>.json` for the other four. `tracers-v3.json`, `tracer-observations.json` and `tracers-v3/` retain actual encodings/results/full traces. |
| `python3 build/phase05-plan20/review-fix/carry-faults.py` | Exit0. Six isolated test-only ROMs preserve the real tested operation/result and inject only a C-bit fault before capture. The original new guest assertions fail with their correct IDs. Clean production companions passed first. `carry-faults.json`, `carry-fault-observations.json` and `carry-faults/` retain commands, original tested opcodes, source, disassembly, ROMs and traces. No production C was altered. |
| `cmake -DCASE=ledger-controls -DSRC=. -DBIN=build/phase05-plan20/review-fix/full-ledger -P tests/gates/coverage_case.cmake` | Exit0. `ledger-controls.json` and `.log`; 187 concrete refusal observations retain the prior35 ARM,24 Thumb and23 other classes and add105 controls. Positive draft, ordinary low-BX, smoke-admission and output/hint boundaries also passed. No existing control was removed. |
| `cmake -DSRC=. -DOUT=build/phase05-matrix/thumb -DTOOLCHAIN_ROOT=build/toolchains -DMODE=all -DISA=thumb -P cmake/BuildArmDiagnostics.cmake` | Exit0,24.16s observed wall time. Exactly one full frozen Thumb rebuild:118 semantic pairs/eight stops/244 images. `frozen-matrix-build.json`, `.log` and `frozen-matrix-build-receipt.json`. Per-image ELF/map/program/source/tool/signature receipts remain in `build/phase05-matrix/thumb/`. |
| `cmake --build build/ci --target gamebloke-run cpu_test` | Exit0,1.25s. `native-build.json` and `.log`. |
| `cmake -DCASE=thumb-pre-admission -DSRC=. -DBIN=build/ci -DARTIFACT_ROOT=build/phase05-matrix/thumb -P tests/gates/diagnostics_case.cmake`, then `build/ci/cpu_test thumb_bl`, then `build/ci/cpu_test thumb_control` | Every command exit0. `pre-admission.json`, `cpu-thumb-bl.json`, `cpu-thumb-control.json` and matching logs. Raw gate16.04s; complete build/acceptance chain43.19s. `final-chain.json` records the combined sequence. |
| Independent receipt/byte/trace audit | Exit0. `self-check-final.json`, `.log`, `self-check.json` and executable `self-check.py`. Re-read all244 ROM/claim digests, case IDs, exact generated programs and their receipt hashes, pinned source revisions, ELF/maps and complete traces. Confirmed118 passes,118 deliberate failures, eight pending stops and65 runtime controls. |

All244 raw documents retain GUEST_STOP and null fixture ID. The canonical result checker refuses every document with exactly its two established admission findings. The unchanged65 runtime controls include all eight stop contexts and separate BL prefix/suffix retirement, refill, intermediate/final LR/PC and actual data-read captures; all six BL observation receipts remain in `raw-results/`. CPU API tests preserve unmasked IRQ entry between BL halves without claiming handler execution.

The current ledger has386 rows,363 inventory targets and385 pending rows, with the unchanged single source-verified smoke pair. ARM retains236 semantic producers, four stops and19 API rows. Earlier Plan20 reports of117 pairs/242 images are historical and were not rewritten. Before canonical output replacement, the previous matrix and raw results were copied to `historical-before-fix/build/phase05-matrix/thumb/` and `historical-before-fix/build/ci/diagnostics-results/thumb-pre-admission/` beneath the receipt root.

## Sequencing, limits and handoff

`b1ce937` is the provisional source commit required by the strict builder; the actual GNU and production-runner acceptance evidence follows it. All producer changes were grouped before the single full frozen matrix rebuild. The first scratch build used a disallowed output location and was refused before a build; the next successful LSR clean run exposed a local script's integer-versus-string case-ID assertion. Both setup failures remain in `tracers.json`/`tracers-v2.json` and logs. The corrected script reused the successful pinned build and saved fresh traces separately. An unsupported staged signature-scan invocation stopped the initial commit command; its supported tree scan and staged hygiene then passed before the source commit.

Old-checker false acceptance and the missing pre-fix carry/ROR source witnesses were recorded before implementation in `old-behavior.json`. These are regression reproduction/source-audit evidence, **not formal TDD/JUnit RED claims**. No synthetic JUnit report or classifier result was authored. The original Plan19 formal reporter gap remains open.

The ledger command recorded2833.27s elapsed wall time and the selected tracer sequence930.63s, including one selected build recording919.70s while the other four recorded roughly3–4s. These are observed wall intervals without established attribution, not emulator throughput, hosted timing or a model comparison. The previously identified Plan24 critical-path issue remains visible; no check class, fixed180s hosted limit, cache, concurrency or dependency setting changed.

No hosted run occurred; the older hosted green does not cover this source. ARM was not rebuilt wholesale: its older receipts intentionally remain stale after shared-input changes. Cross-host literal comparison and fixture admission remain Plan21. No private content, dependency/license/rights change, binary upload, remote work, publication, hardware/BIOS/game qualification or Phase6 work occurred. No new network/auth/file trust boundary was introduced; the changes strengthen existing source-to-coverage checks.

The parent owns independent re-review, the original SUMMARY/REVIEW, shared STATE/ROADMAP/requirements/WINDOWS/routing and subsequent phase flow. No owner action is needed for this completed local correction batch.

## Self-check: passed

All three corrected implementation paths exist and are committed at the frozen source. The report preserves historical evidence and source limits. Fresh diff checks, tree hygiene, signature scanning and staged hygiene accompany the final report commit; only the assigned paths are staged. Owner and parent dirty/untracked files remain intact.
