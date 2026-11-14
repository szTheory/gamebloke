# Phase 5 process correction after the owner's progress check

The original autonomous task continues. The owner asked for a sanity check after a long session. Phase 5 contains 25 plans spanning full ARM/Thumb completion and original diagnostic evidence; completion of an implementation plan does not mean phase acceptance.

The existing `05-VALIDATION.md` Sampling Rate already calls for focused behavior and negative/clean controls after production changes, with canonical CI at integration boundaries and final source. Repeating full native/compiler/sanitizer inventories around every small plan was additional orchestration overhead. Full suites have generally taken minutes; serial source research, verbose test authoring, review and artifact cycles account for substantial additional wall time. Do not attribute all elapsed time to CI.

Apply this cadence now:

- For each change, run its planned focused commands, meaningful negative/clean controls, relevant CPU regressions and targeted native sanitizers. Compile the first committed GREEN with strict GNU warnings early; use the existing pinned Linux toolchain, without running the entire inventory merely to check compilation.
- Review coherent instruction families before broad integration reruns. Preserve close scrutiny of banking/status, dynamic PC refill and whole-unit atomicity. Group simpler Thumb reviews by family where their dependencies permit it.
- Run full canonical/compiler/sanitizer inventories after review fixes at real integration boundaries: ARM completion (Plan 13), Thumb completion, explicit diagnostic/hosted-witness gates, and final source. Preserve all required phase gates, inventory audits, negative controls, allowed-skip policy and fixed hosted limits.
- Keep required summary metadata and self-checks, but make prose concise and link exact receipts. Avoid copying the same test history into several artifacts or creating extra status-only commits.
- Use compact table-driven test inputs and shared arrange/run/cleanup plumbing. Keep expected results independent of production helpers. Store fixtures start with contrasting sentinels and literal expected addresses; generated CPU state must be consumed by a next-call test at important boundaries.
- Preserve honest history: an already-implemented behavior has a GREEN baseline plus isolated fault controls, never an artificial production regression to manufacture RED.

Current Plan 13 implementation remains active. Root owns family review and the following full ARM integration checks. Routine publication remains batched under the existing one-PR-per-phase policy. Model mappings, project scope, rights restrictions, human acceptance gates and stop-on-new-hosted-red policy are unchanged.

## First bounded stronger-executor observation

Owner authorized Plan14 alone at configured gpt-6.1-sol/high. Dispatch to terminal implementation/verification handoff took about16min; review and one adjacent evidence-label correction added about14min. No concrete Thumb defect was found in that review. Different plan scope, starting code and corrected verification cadence prevent a model-only speedup claim; billing usage was not available. Routing restored to exact pretrial dirty bytes afterward, default executor Luna/xhigh. Keep implementation, review/rework and integration wait measured separately. Source audit before claiming hardware behavior remains essential: this batch corrected its own Thumb multiply timing oracle and exposed an older overconfident ARM alias label.

## Reduce context and fixture mistakes without new infrastructure

Use a fresh executor for the next bounded instruction batch, with durable plans/notes/receipts as its context. The reused executor had accumulated several families and briefly confused a later RED commit with its starting HEAD; the saved dispatch record and graph resolved that without a ref change. Treat saved epochs/HEAD as authoritative. This is a context-management change, not a proven speedup.

Known fixture traps: seed architectural r13 through the correct bank slot, not r14; observe stored data through memory because write trace values are bus return values; account for already queued instructions when asserting next fetch. Keep sorted selector inventories synchronized before classifying RED, and keep test setup failures distinct from intended behavioral failures. Root can apply a trivial proven four-line review cleanup inline with focused checks rather than creating another implementation handoff.

## Boundary integration evidence and reporting hygiene

Plan18's complete source integration took18m30s across four sequential lanes, including setup/builds. Native canonical/ASan CTest185.16/124.26s; local pinnedGNU/ClangASan241.98/467.32s. A preceding native run267.80s failed only reporting hygiene; these unequal/noisy observations are not a throughput or model benchmark. Preserve guest throughput separately from compiler/test/orchestration time.

Keep committed evidence paths relative. An absolute checkout home path in a fixer report passed diff-whitespace checks but failed canonical hygiene; targeted hygiene and a complete inventory rerun established the correction. Do the applicable hygiene check before committing newly generated reports, especially reports produced by a reused agent.

Before Plan24 publication, inspect measured mutation-compile costs and the fixed180s hosted budget. Repeated compilation of unchanged source within clean/mutant pairs is a concrete candidate to measure, extending the existing guarded six-CPU-case optimization only if paired receipts demonstrate savings with all failure classes preserved. Header-affecting/unknown cases must retain safe full rebuilds. Do not infer hosted timing from these local runs or add caches/concurrency/dependencies/timeouts by default.

## Original diagnostic matrices: validate meaning before scaling

Plan19's independent original assembly exposed a real signed-long-multiply decoder bug that native opcode fixtures shared. Check instruction encodings against GNU assembly and primary instruction forms; independent expected arithmetic alone is insufficient when both producer and test misidentify an opcode.

Before scaling a new matrix, run a real clean/broken tracer, then representative addressing/flags/PC/stop cases with full traces. A mailbox pass missed one case falling through into embedded address data. Source completeness and assembler syntax do not establish semantic coverage: review caught wrong PC operand placement, writeback enabled in no-writeback cases, invalid Mode3 force-user inventory, and duplicated boundary vectors. Corrected source contracts, actual emitted opcode checks and targeted mutation controls now protect those gaps. Apply this scrutiny while authoring Thumb, rather than after its entire matrix is built.

Group demonstrated producer fixes before rebuilding at one frozen source revision. Preserve completed OS work across transport loss, record unknown exit codes honestly, and rebuild only when authority inputs actually change. A validated internal row-location hint removed repeated JSON searches while retaining literal case-ID and parent-authority checks; isolated lookup timing is not a full-build or emulator benchmark.

The corrected ARM builder/raw gate took48.05/25.53s locally; expanded ledger controls took108.62s. Inspect this distinct recurring control cost before Plan24's fixed180s hosted path. Preserve every regression class while reducing repeated parsing/validation where measurements justify it. Do not assume the old hosted green covers these changes. The original formal TDD reporter gap remains visible in WINDOWS; unsupported raw CMake output cannot be presented as recognized RED evidence.

## Thumb source review and timing attribution

Plan20's genuine narrow CTest/JUnit RED reports passed the installed classifier after an initial working-directory setup failure was explicitly rejected. Use the real supported reporter plus semantic inspection; parsing a report alone cannot establish an intended failing behavior.

Derive form-defining opcode bits independently of a case's supplied full opcode. Coordinated source/metadata mutations exposed missing high-register selector and PUSH/POP LR/PC constraints. Carry tests must contrast incoming C with the independently required outgoing bit, and count-zero preservation must use both C inputs. New immediate/register shift and ROR0/33 vectors detect isolated carry-only faults; native tests alone do not establish original guest coverage.

Challenge candidate findings against primary instruction fields and the real assembler. A proposed residual BX issue was rejected: bits5–3 select the source register, and GNU confirms0x4718 is legal BX r3. Tightening those variable bits would have restricted a valid instruction form.

Current source produces118 Thumb semantic pairs/eight stops, with65 raw controls. Full ledger controls retain187 refusals. Initial Plan20 ledger wall time was220.44s; a later run recorded2833.27s and a selected build919.70s, without established attribution. Do not treat unexplained wall intervals as CPU work, hosted timing or model speed. During required CI-budget work, measure attributable command CPU time and elapsed time separately, inspect repeated parsing/validation and targeted processes, and preserve every failure class. No whole-machine restart or additional cache/concurrency/dependency is justified by a timeout alone.

## Admission scale and control cost

Measure the full authority readers and malformed-input controls at the proposed
fixture count before scaling admission. A representative guest pair established
runtime correctness but did not expose the722-row manifest's repeated strict
parsing cost. Reuse the canonical parsed authority across result checks and
index projection; do not weaken validation or change ROM producer inputs merely
to avoid consumer overhead.

Inspect duplicate command execution before adding workers. The failure-reason
and no-echo assertions formerly launched the identical loader twice. Pairing
those assertions on one execution removed duplicate work, with controls for
leaks, unexpected success, wrong failure and exact single execution. The
remaining independent trailing variants then justified a bounded three-worker
pool: all63 variants remain visible and mandatory. A serial matched measurement
fell64.54→22.01 seconds, with CPU62.61→50.68 seconds. Full canonical CI passed
in166.72 seconds; hosted timing remains a separate later gate. See
[the measured report](05-21-ADMISSION-PERFORMANCE.md).

Check resolved containment before copying, writing or hashing fixture paths.
Checking the generated index afterward cannot undo an earlier write through an
escaping symlink. Controls must preserve an outside synthetic sentinel and
include a valid contained companion. Force file-mode comparison per Git command
when executable mode is part of source authority; repository configuration must
not silently weaken that check.

## Verify review premises against actual storage

Plan22 review initially treated the machine RAM regions as allocated pointers.
They are inline arrays, so structure assignment already copies those bytes. A
small probe against the real canonical comparison helper changed each RAM
region independently and confirmed both equality and digest detect memory-only
changes. The reviewer withdrew the warning; no snapshot redesign was needed.
Check actual field definitions and reproduce the proposed failure before adding
an abstraction or extra allocations in response to a review claim. Keep false
findings distinct from defects that were fixed.

## New hosted runtime stop after complete local proof

The Plan23 source freeze passed ten local lanes and complete literal ROM parity,
but fresh hosted native builds and ARM groups hit the180-second cap. Historical
runner overhead plus local timing did not predict current hosted costs closely
enough. Preserve the resulting stop, measure actual current hosted setup/build/
test costs, and target proven duplicate verification work at final fixture scale.
Local Linux ARM64 timing and Darwin portability lanes cannot qualify hosted
Ubuntu-x64 budgets. Do not turn a timeout into another unchanged retry or larger
cap. See [the concrete stop and recovery proposal](05-23-HOSTED-STOP.md).

## Measure shared validation primitives before repeating proof matrices

The first consumer repair removed real duplicate authority loads but left
substantial unfinished hosted work. Count all remaining tests and front-stage
costs before spending the renewed attempt; a timeout duration is not a completed
baseline. Pause further expensive lanes when early evidence still lacks margin.
A subsequent isolated probe found the strict JSON census reparsing full arrays
per child: the same15886member count used11.60seconds userCPU versus0.10 after
counting CMake's serialized parsed tree. Independent structural counts and
adversarial duplicate controls guard the equivalence. Producer-input changes
still require a new freeze and fresh proof; no historical receipt is relabelled.
