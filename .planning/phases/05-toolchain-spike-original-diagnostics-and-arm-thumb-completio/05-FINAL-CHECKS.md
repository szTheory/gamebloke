# Phase 5 final local checks and claim guide

This is a local candidate record, not Plan24/25 completion or phase acceptance.
The expanded current candidate and recurring-coverage correction are recorded in
[05-25-COVERAGE-FIX.md](05-25-COVERAGE-FIX.md), with a separate snapshot under
`build/phase05-plan25/coverage-fix/`. The initial13-file record and timing table
below retain their original evidence; they are not relabeled as the expanded
candidate's test results. Parent's current three profile files are included in
the new binding and current canonical31profile tests passed.
The parent owns independent code/security/goal/Nyquist audits, the final exact-head
hosted gate, and the owner boundary. No hosted attempt, push or merge occurred
in this work. The prior renewed hosted attempt was consumed successfully; it
does not authorize another attempt.

## Identity and observations

The actual checkout is `8e21ef63017a4b4a98d7a585cf7bddc2a9628fdd` plus the saved
local consumer candidate. Runtime receipts record that actual HEAD, never a
future commit. Candidate paths, hashes/modes and commands are retained under
`build/phase05-plan25/`. Parent must bind the eventual reviewed commit separately.

The producer remains `85c361edfd6b56640e68e64c5e32527f85408836`, with input digest
`95d1b9c7f8c580c37a0051efb54424d40d7c50a57d2774c33512001f31331d43`.
The manifest digest remains
`643f18a6ed5649b0797a75a105a64765eee4936bee985251326224ca4fdf11cb`.
Its 722 output pins, rights and expectations are unchanged. Exact producer
bytes/modes/ancestry and every retained R4 terminal artifact digest pass the
local audit. The complete prior local producer proof is preserved, without
rerunning or relabeling unchanged cross-host evidence.

The current mandatory admitted diagnostic workflow passed with no skips and
strict inventory reconciliation. Its actual results are 355 PASS, 355 deliberate
GUEST_FAIL and 12 named guest stops, for 367 guest IDs. Nineteen native API rows
retain their own actual assertion evidence and no invented ROM/CLI PASS.
`build/phase05-plan25/identity-coverage-audit.json` crosschecks all 386 ledger
rows against these actual results or API receipts, their fresh execution token,
tested consumer, producer, hashes and bounded progress.

The installed production check now tests admitted `arm-343721884`, its genuine
broken companion, and `arm-stop-1765902634`. Only `bin/gamebloke-run` is installed,
then moved and launched from unrelated cwd using explicitly copied owned input.
Plain JSON, requested-trace JSON and trace bytes match the build executable
exactly. SHA identity, semantic state, exact IDs, named SWI stop and guest progress
are checked separately. Runtime index/manifest/source overrides are unset;
neither executable receives a source, CMake or manifest argument. The raw owned
image check and missing-executable negative with clean companion remain active.

## Commands and environment

All commands run from the checkout root in background CLI sessions. The local
host is macOS ARM64, CMake4.4.3, with `/usr/bin/clang` (AppleClang21.0.0.21000101) explicitly selected for the fresh
AppleClang sanitizer lane. Linux uses native ARM64 and the unchanged
`ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c`
image, GNU for native and clang-21 for sanitizers, with four CPUs/eight GiB.
Local Linux ARM64 is not the distinct hosted Linux x64 witness.

| Command/check | Actual local status |
| --- | --- |
| `cmake -DREQUIRE_AUTHORITY=ON -P cmake/CiPreflight.cmake` | Passed exact budgets, producer inputs, modes and ancestry before long work. |
| `sh scripts/fetch-toolchain.sh --check --root build/toolchains` | Existing Darwin ARM64 pinned install passed; no reinstall/download. |
| `cmake --workflow --preset ci` | Final current-source run after setup/helper corrections passed136.64s; strict1107expected/381ran/726explicit optional skips. Earlier trap-only correction run139.58s is separate. |
| `cmake --workflow --preset diagnostics` | Final current-source pass82.91s,793main checks/zero skips/strict post; earlier92.10s pass remains separately logged. |
| `cmake -DMODE=final -DMATRIX=diagnostics/armv4t/cases.json -P cmake/CheckArmCoverage.cmake` | Passed11.78s; unchanged producer/checker. |
| `cmake -DSRC=. -DBIN=build/diagnostics -DMODE=admitted -P tests/gates/runner_relocation.cmake` | Final post-identity-fix pass4.05s, exact positive/broken/stop JSON/trace; earlier3.59s pass separate. |
| Fresh AppleClang sanitizer configure/build/test/post with in-root `junit.xml` | New `build/phase05-plan25/asan-macos-final`: configure2.53s/build7.58s/main191.66s/post0.17s;849main checks,0skips, strict post passed. Initial stale-runner failure and94.51s narrow/4.52s receipt-control fixes remain separate. |
| `sh scripts/linux-lane.sh ci gcc` | Passed194.65s total; setup23s/workflow169s. GNU15.2.0/CMake4.2.3/native ARM64; strict1107expected/381ran/726explicit optional skips. |
| `sh scripts/linux-lane.sh asan clang` | Passed150.85s total; setup31s/workflow117s. Clang21.1.8/CMake4.2.3/native ARM64; strict849expected/124ran/725explicit optional guest skips. |
| `cmake -DCASE=ledger-controls -DSRC=. -DBIN=build/phase05-plan25/coverage -P tests/gates/coverage_case.cmake` | Full actual clean/negative controls passed; approximately528s observed, no precise attached timer. |
| `cmake -DCASE=all -DSRC=. -DBIN=build/phase05-plan25/dependency -DRUNNER_BIN=build/diagnostics -P tests/gates/diagnostic_dependency.cmake` | Passed1.07s with actual BIOS/dependency/refusal companions. |
| Current original-object-only map/architecture/sections/disassembly check | Passed all722maps in2.58s; no rebuild or old-proof relabeling. |
| Generated exact-name signature scans | Passed2smoke/476ARM/244Thumb files in0.14/6.58/3.19s; exact-name allowlists exempt only extension/binary rules, never logo/BIOS checks. |
| Final source/path audit | Passed all386rows/722actual output paths and hashes/19APIreceipts, producer modes/ancestry and every retained R4 terminal artifact digest. |
| `cmake --workflow --preset hygiene` | Passed114.94s; strict207expected/206ran/1explicit owner-local skip. Source and signature gates passed. |

Each raw command vector and timing is retained separately. Setup, configure,
build, test, post and whole-command wall time are not emulator throughput.
Runner child CPU seconds cover local child processes; Docker daemon/container
CPU consumption is not measured by that host counter. Four CPUs are an allocation
ceiling, not a claim that four CPUs were busy throughout.

Fresh macOS configure/build/main/post are separately timed2.53/7.58/191.66/0.17s.
For final canonical diagnostics, CTest main/post are80.31/0.12s of82.91s total.
Linux GNU main/post are148.92/0.26s; its4.2s configure is printed by CMake.
GNU build was not separately clocked: the remaining workflow interval includes
build and command overhead and is not reported as a precise build time.
Linux Clang main/post are97.35/0.14s; observed workflow-output boundaries put
configure/build at4.84/15.05s including buffering/transport overhead. Outer helper
wall also includes source export and container startup/teardown. These local
measurements neither renew nor predict a hosted job's exact180/300s budget.

The bottleneck review preserved all10 unchanged R4 lanes and avoided another
producer-parity matrix. The broad528s ledger corruption sweep remains a one-off
planned audit rather than a new recurring CI gate. Fresh container setup costs
are explicit23/31s; the existing disposable per-lane helper uses separate fresh
GNU and Clang environments. No cache, concurrency, package dependency or timeout
was added to hide this cost. The real sanitizer receipt-order defect was diagnosed
before its one fresh full retest, rather than retrying unchanged failures.
The final hygiene critical path is measured too: admitted hosted-receipt refusal
controls44.43s, source hygiene10.59s, signatures7.79s and manifest5.88s are the
largest individual checks. They cover distinct existing boundaries; no cache,
concurrency or weaker evidence was introduced merely to shorten the log.

Guest assembly recipes retain `-mcpu=arm7tdmi`, explicit case/ISA/broken
selection, the owned linker script, `objcopy -O binary` and owned header repair.
The unchanged manifest recipes and producer input digest bind exact flags and
sources; current per-image maps/attributes/disassembly independently reject
guest runtime/CRT/library additions. Host sanitizer flags remain
`-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`
with matching sanitizer link flags. This does not turn host C instrumentation
into guest timing or hardware evidence.

## Architecture and boundary crosswalk

The exact ledger stores all instruction/operand rows, independent derivations,
stable IDs and fixture/API links. The final coverage gate checks legal-form
completeness; the execution audit checks actual observations. They are separate
evidence. Defined exact-core semantics, secondary GBA reference observations,
and chosen deterministic unpredictable policies retain their source labels.

| Area | Evidence and boundary |
| --- | --- |
| CPU-1 | Version/classification and base-in-list policies; guest semantics/stops plus native boundary rows. Condition 1111 is unpredictable. |
| CPU-2 | Full ALU/shifter carry/count/flag rows and native vectors; no blanket preservation oracle. |
| CPU-3 | Instruction-specific PC operands/visible PC/refills and actual read observations. |
| CPU-4 | ARM/Thumb block/stack ordering, empty/base-in-list policies, user banks and PC/SPSR returns. |
| CPU-5 | Subword/sign/alignment/rotation rows and native bus-backed checks. |
| CPU-6 | MUL/MLA/UMULL/UMLAL/SMULL/SMLAL with defined N/Z and separately labeled C/CV policy. |
| CPU-7 | Actual separate BL half fetch/retirement and intermediate LR/PC; API IRQ injection after unmasking, without handler/controller. |
| CPU-8 | PSR masks/privilege/banks/SPSR/returns and invalid-state refusals. |
| CPU-9 | ARM SWI 24-bit and Thumb 8-bit comment fields; distinct undefined/coprocessor/no-BIOS causes. |

All 19 Thumb formats map to the actual ledger families:

| Formats | Families |
| --- | --- |
| 1, 2, 3, 4 | Immediate shifts; add/subtract; immediate operations; register ALU. |
| 5, 6 | High-register operations/BX; PC-relative load. |
| 7, 8 | Register-offset transfers; signed/halfword register-offset transfers. |
| 9, 10 | Immediate byte/word transfers; immediate halfword transfers. |
| 11, 12, 13 | SP-relative transfers; PC/SP address formation; SP adjustment. |
| 14, 15 | PUSH/POP; multiple transfers. |
| 16, 17, 18, 19 | Conditional branches; SWI; unconditional branch; separately retired BL halves. |

Whole-unit refusal evidence assumes the current uniform, non-side-effecting
checked bus contract. Future effectful MMIO, arbitration, variable waits or DMA
need the documented prepare/commit or explicitly scoped partial-abort contract.
The native maximum-unit tests do not authorize silently extending that guarantee.

## Every planning source row retains an evidence destination

This reconciles the rows in `05-SOURCE-AUDIT.md`; its COVERED labels remain
planning coverage, not implementation pass or independent sign-off.

| Source rows | Actual evidence destination / outstanding limit |
| --- | --- |
| GOAL story, SC1 | Preserved R4 literal parity/architecture proofs; prior hosted source proof; final exact-head hosted gate pending. |
| GOAL SC2–4; REQ DIAG-01/02/03, VERIF-04 | Original NOTICE/inputs/recipes, immutable manifest/index, actual admitted JSON, failure/absence/required guards and final scans. |
| GOAL SC5; REQ CPU-01/02 | Exact coverage gate, all-row execution crosswalk, CPU-1..9 and all19 formats above; native semantic mutants and refusal tests. |
| RESEARCH architecture1, architecture4, architecture5 | Source/tool/artifact separation, retained tool/output proof and actual production/result/API receipts. |
| RESEARCH architecture2, architecture3, dont-hand-roll | Canonical CPU decoded-unit implementation/native tests, complete ledger and unchanged dependency tree. |
| RESEARCH architecture6, adversarial, validation | Archive/map/signature/identity/refusal controls, current compiler/inventory/installed-consumer lanes; final independent and hosted gates pending. |
| CONTEXT D-01–04 | Existing three host pins, bounded fetch/extract/notices, original smoke/attributes/map/signature and distinct literal versus hosted hash proof. |
| CONTEXT D-05 | Measured setup/build/test/post/critical path and stopped hosted ledgers. Later D-23 explicitly amends only two job caps to300s; other nine stay180s. |
| CONTEXT D-06–12 | CPU/ledger/profile5 scope and policies, no second interpreter, same preflight plan, banks/refills/time bounds and cause classification. Parent independent source review remains pending. |
| CONTEXT D-13–18 | Owned startup/mailbox/assertions; separate artifact/source/tool identity; source-only producer before admission; exact real pair/stop/API crosswalk and map/signature proof. |
| CONTEXT D-19–22 | Visible optional/BIOS absence, mandatory setup/receipt controls, current inventories, fixture-only rights, profile5 limits and retained unresolved probes. |

There is no UI, ORM/database, AI framework or external service integration to
qualify. The developer-tool download and existing CPU API are accurately scoped
in `COVERAGE.md`; they are not evidence for an invented service integration.

## Rights, judgment and remaining gates

Original source NOTICE authorizes necessary public-CI compilation/execution of
these diagnostics only. It grants no project-wide license, third-party permission
or ROM binary distribution. No new LICENSE, dependency, ROM upload or fixture pin
was introduced. Git path inventory and synthetic ignored sentinels establish the
selected exclusion behavior; private owner ROM/BIOS paths were not accessed.

The three unclassified Phase5 probes and three descriptor-less prohibitions stay
unresolved. Historical eleven Phase4 rows and Plan19's historical TDD deviation
remain unchanged. The owner reviews guide clarity and bespoke judgments at the
explicit phase boundary; no test result or authored acceptance criterion is a
waiver. BIOS execution, physical controller/hardware accuracy, game compatibility,
device/host presentation, release and Phase6 remain unqualified/out of scope.

After parent audits/fixes and final candidate binding, parent can prepare the
concrete new hosted-authority decision. No authority is inferred here. At the
later merge boundary, the recommended one-time merge commit preserves producer
ancestry; the standing squash instruction alone cannot authorize that exception.
Owner decision and main proof remain pending. No current user action is needed
for the remaining authorized local work.

## Local candidate handoff

All planned local implementation/integration, compiler, inventory, negative,
coverage, map/signature and hygiene checks now have actual terminal evidence.
`build/phase05-plan25/candidate-hashes.json` records all13owned files, modes,
actual checked-out HEAD and successful command receipts; candidate.patch keeps
the tracked consumer diff. No Plan25 SUMMARY or shared state/requirement update
was written. Final independent audits, exact committed consumer binding, hosted
authority/checks and owner judgment remain pending with their named owners above.
The subsequent source-coverage audit requested a recurring helper regression and
mandatory ARM/all installed relocation. Both now have retained controls and
passing current checks, including a new Linux GNU run and latest-log manual
collector/legacy controls. The full evidence and expanded hash/patch location
are in05-25-COVERAGE-FIX.md. The earlier independent review reports are unchanged;
parent owns their rereview against this expanded candidate.
