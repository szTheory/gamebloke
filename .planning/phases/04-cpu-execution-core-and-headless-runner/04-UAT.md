---
status: testing
phase: 04-cpu-execution-core-and-headless-runner
source: [04-01-SUMMARY.md, 04-02-SUMMARY.md, 04-03-SUMMARY.md, 04-04-SUMMARY.md, 04-05-SUMMARY.md, 04-06-SUMMARY.md, 04-07-SUMMARY.md, 04-08-SUMMARY.md, 04-09-SUMMARY.md, 04-10-SUMMARY.md, 04-11-SUMMARY.md, 04-12-SUMMARY.md, 04-13-SUMMARY.md]
started: 2026-11-11T13:04:15.531833Z
updated: 2026-11-11T13:04:15.531833Z
---

## Current Test

number: 28
name: Evidence guide clarity and rights/claim boundaries
expected: |
  The guide makes clear that the bounded runner and synthetic controls establish implemented software behavior. They do not grant fixture rights or establish hardware accuracy, BIOS boot, or broad game compatibility. All eleven probe rows remain unresolved.
awaiting: user response

## Tests

### 1. Direct-start state and a real bus-backed ARM MOV/B pipeline path.
expected: Direct-start state and a real bus-backed ARM MOV/B pipeline path.
result: pass
source: automated
coverage_ref: 04-01-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 2. Whole-unit cycle bounds, overflow preflight, and unchanged state on refusal.
expected: Whole-unit cycle bounds, overflow preflight, and unchanged state on refusal.
result: pass
source: automated
coverage_ref: 04-01-SUMMARY.md#D2
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 3. Both configured cores and clean bus/loader scratch controls compile the canonical CPU source.
expected: Both configured cores and clean bus/loader scratch controls compile the canonical CPU source.
result: pass
source: automated
coverage_ref: 04-02-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 4. CPU reset, ROM replacement/refusal, no-ROM handling, canonical fields and hard cycle caps have observable witnesses.
expected: CPU reset, ROM replacement/refusal, no-ROM handling, canonical fields and hard cycle caps have observable witnesses.
result: pass
source: automated
coverage_ref: 04-02-SUMMARY.md#D2
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 5. ARM arithmetic and defined shift behavior, including flag and boundary cases.
expected: ARM arithmetic and defined shift behavior, including flag and boundary cases.
result: pass
source: automated
coverage_ref: 04-03-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 6. The supported Thumb, branch/BX, and word/byte transfer slice uses bus-backed fetch/refill and data requests.
expected: The supported Thumb, branch/BX, and word/byte transfer slice uses bus-backed fetch/refill and data requests.
result: pass
source: automated
coverage_ref: 04-03-SUMMARY.md#D2
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 7. CPU semantic selectors and their clean/mutated controls are registered in CI and ASan lanes.
expected: CPU semantic selectors and their clean/mutated controls are registered in CI and ASan lanes.
result: pass
source: automated
coverage_ref: 04-03-SUMMARY.md#D3
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 8. All processor modes select the expected shared/private banks; User/System SPSR access is explicitly absent.
expected: All processor modes select the expected shared/private banks; User/System SPSR access is explicitly absent.
result: pass
source: automated
coverage_ref: 04-04-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 9. Exception entry saves CPSR and explicit address context, selects the expected LR/vector/masks, and stops before BIOS-less handler fetch.
expected: Exception entry saves CPSR and explicit address context, selects the expected LR/vector/masks, and stops before BIOS-less handler fetch.
result: pass
source: automated
coverage_ref: 04-04-SUMMARY.md#D2
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 10. Mode, exception, and no-BIOS witnesses and their mutation controls run in CI and ASan lanes.
expected: Mode, exception, and no-BIOS witnesses and their mutation controls run in CI and ASan lanes.
result: pass
source: automated
coverage_ref: 04-04-SUMMARY.md#D3
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 11. The v4 profile and visible narrative agree on CPU direct-start defaults, sourced ARM facts, no-BIOS stop edge, and the bounded instruction slice.
expected: The v4 profile and visible narrative agree on CPU direct-start defaults, sourced ARM facts, no-BIOS stop edge, and the bounded instruction slice.
result: pass
source: automated
coverage_ref: 04-05-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 12. CPU policy negative controls run in the sorted CI and hygiene inventories.
expected: CPU policy negative controls run in the sorted CI and hygiene inventories.
result: pass
source: automated
coverage_ref: 04-05-SUMMARY.md#D2
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 13. A real guest store sequence produces only a complete versioned mailbox verdict, with state-neutral IWRAM observation and explicit refusal behavior.
expected: A real guest store sequence produces only a complete versioned mailbox verdict, with state-neutral IWRAM observation and explicit refusal behavior.
result: pass
source: automated
coverage_ref: 04-06-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 14. Configured and scratch builds include the same inspection source; a time-mutating peek is caught by the semantic neutrality assertion.
expected: Configured and scratch builds include the same inspection source; a time-mutating peek is caught by the semantic neutrality assertion.
result: pass
source: automated
coverage_ref: 04-06-SUMMARY.md#D2
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 15. A host-side SHA-256 utility hashes exactly the caller-provided span and preserves output on invalid span refusals; runner binding remains unimplemented in this slice.
expected: A host-side SHA-256 utility hashes exactly the caller-provided span and preserves output on invalid span refusals; runner binding remains unimplemented in this slice.
result: pass
source: automated
coverage_ref: 04-08-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 16. Fatal-validated canonical metadata and separate synthetic control metadata compile into immutable indexes.
expected: Fatal-validated canonical metadata and separate synthetic control metadata compile into immutable indexes.
result: pass
source: automated
coverage_ref: 04-09-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 17. Generated instruction images exercise mailbox, timeout, exception, Thumb, and BX paths; stale image digests are rejected.
expected: Generated instruction images exercise mailbox, timeout, exception, Thumb, and BX paths; stale image digests are rejected.
result: pass
source: automated
coverage_ref: 04-09-SUMMARY.md#D2
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 18. SHA-256 vectors and metadata identity controls are registered in CI and ASan lanes.
expected: SHA-256 vectors and metadata identity controls are registered in CI and ASan lanes.
result: pass
source: automated
coverage_ref: 04-09-SUMMARY.md#D3
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 19. Bounded CLI outcomes preserve status, semantic result, machine time, claim, and limits.
expected: Bounded CLI outcomes preserve status, semantic result, machine time, claim, and limits.
result: pass
source: automated
coverage_ref: 04-11-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 20. Registered mailbox outcomes and unregistered raw results keep their distinct classifications.
expected: Registered mailbox outcomes and unregistered raw results keep their distinct classifications.
result: pass
source: automated
coverage_ref: 04-11-SUMMARY.md#D2
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 21. Trace capacity, output-full, deterministic JSONL, and write failure behavior are bounded and explicit.
expected: Trace capacity, output-full, deterministic JSONL, and write failure behavior are bounded and explicit.
result: pass
source: automated
coverage_ref: 04-11-SUMMARY.md#D3
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 22. Every CPU exception stop maps to GUEST_STOP with its stable production JSON reason.
expected: Every CPU exception stop maps to GUEST_STOP with its stable production JSON reason.
result: pass
source: automated
coverage_ref: 04-11-SUMMARY.md#D4
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 23. Result validation binds status to mailbox semantics, exact machine-time bounds, termination, and trace completeness.
expected: Result validation binds status to mailbox semantics, exact machine-time bounds, termination, and trace completeness.
result: pass
source: automated
coverage_ref: 04-12-SUMMARY.md#D1
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 24. Actual producer output validates through the same checker and preserves registered dispute disclosure.
expected: Actual producer output validates through the same checker and preserves registered dispute disclosure.
result: pass
source: automated
coverage_ref: 04-12-SUMMARY.md#D2
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 25. Bounded trace execution and explicit capacity exhaustion
expected: Bounded trace execution and explicit capacity exhaustion
result: pass
source: automated
coverage_ref: 04-07-SUMMARY.md; cpu.trace, cpu.trace_capacity, runner.result_contract
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 26. Fresh host CLI, bounded input identity and separated JSON outcomes
expected: Fresh host CLI, bounded input identity and separated JSON outcomes
result: pass
source: automated
coverage_ref: 04-10-SUMMARY.md; runner.unit, runner.result_contract
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 27. Installed production CLI after prefix relocation
expected: Installed production CLI after prefix relocation
result: pass
source: automated
coverage_ref: 04-13-SUMMARY.md; runner.relocation and relocation_control
reported: Measured agent verification under existing authorization; not an owner-reported test. See final checks and independent verification.

### 28. Evidence guide clarity and rights/claim boundaries
expected: Evidence guide clarity and rights/claim boundaries
result: pending
source: human
coverage_ref: 04-12-SUMMARY.md#D3; 04-VERIFICATION.md
rationale: Plan 04-12 D3 reserves the subjective clarity judgment for a human. This does not ask for a rights waiver, hardware claim or probe-status resolution.

## Summary

total: 28
passed: 27
issues: 0
pending: 1
skipped: 0
blocked: 0

## Gaps

No current implementation/documentation gap. One subjective clarity judgment remains pending. Legacy summaries were reconciled against their measured tests and the independent verification, not automatically passed from missing structured coverage. No owner answer is inferred.
