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
