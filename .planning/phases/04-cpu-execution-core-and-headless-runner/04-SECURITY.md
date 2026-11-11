---
phase: "04"
slug: "cpu-execution-core-and-headless-runner"
status: draft
threats_open: 2
threats_closed: 19
threats_total: 21
asvs_level: 1
block_on: high
created: "2026-11-11"
---

# Phase 04 — Security

Independent gsd-security-auditor verdict: OPEN_THREATS. Two blocking authored mitigations share one mailbox-validation root cause. No risk was accepted; phase advancement remains blocked until corrected and rechecked.

## Trust Boundaries

| Boundary | Description | Data Crossing |
|---|---|---|
| Guest/caller → machine | Checked canonical CPU, bus and neutral inspection | Instructions, spans, state |
| Immutable metadata → producer | Separate canonical and synthetic authority; exact loaded-byte identity | Fixture provenance, digest, disputes |
| Core → host observations | Bounded results/traces with explicit failure precedence | Mailbox, time, JSON/JSONL |
| Build → installed product | Production-only moved-prefix execution | Binary and original control inputs |

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation evidence | Status |
|---|---|---|---|---|---|---|
| T-04-01 | Tampering | bounded CPU unit | high | mitigate | src/cpu.c:1173,1498; tests/cpu_test.c:1230,1274 — complete-unit budget/counter preflight and exact/one-short/overflow witnesses. | closed |
| T-04-SC | Tampering | package installation | high | mitigate | All thirteen conditional instances: phase diff adds original C17/CMake code and no new dependency/package-manager install. CMakeLists.txt:13,163 installs only the project product, not an external package. | closed |
| T-04-02 | Information disclosure | canonical CPU lifecycle | medium | mitigate | src/machine.c:10; tests/state_check.h:47; tests/cpu_test.c:400 — complete clearing, field-wise state and refusal/ownership witnesses. | closed |
| T-04-03 | Tampering | instruction arithmetic/decode | high | mitigate | src/cpu_helpers.h:34,110,122; src/cpu.c:632,695; tests/gates/core_mutation.cmake:307 — guarded unsigned arithmetic and semantic RRX mutant. | closed |
| T-04-04 | Denial of service | branch and transfer budget | high | mitigate | src/cpu.c:548,1498; tests/cpu_test.c:1252 — checked transfer/refill spans and atomic unit bounds. | closed |
| T-04-05 | Elevation of privilege | bank/mode selection | high | mitigate | src/cpu.c:102,439,474; tests/cpu_test.c:757; tests/gates/core_mutation.cmake:309 — bank selection and User/System SPSR absence. | closed |
| T-04-06 | Tampering | exception snapshot and no-BIOS stop | high | mitigate | src/cpu.c:159,333,353; tests/cpu_test.c:918; tests/gates/core_mutation.cmake:311 — explicit exception bases, saved CPSR and no handler fetch. | closed |
| T-04-07 | Repudiation | profile/claim policy | medium | mitigate | cmake/CheckProfile.cmake:324,1356,1373; tests/gates/profile_case.cmake:107,164 — versioned policy and rendered-document controls. | closed |
| T-04-08 | Information disclosure | peek span/destination | high | mitigate | src/inspect.c:3,106,119,127; tests/cpu_test.c:2206 — checked peek extents/aliases and unchanged state/output witnesses. | closed |
| T-04-09 | Spoofing | mailbox verdict | high | mitigate | OPEN: src/inspect.c:173 rejects zero case ID only for FAIL; PASS/0 reaches GBL_MAILBOX_PASS at :176, contrary to docs/evidence-contract.md:104 and cmake/CheckResult.cmake:241. tests/cpu_test.c:2291 lacks PASS/0. | open |
| T-04-10 | Tampering | bounded trace completeness | high | mitigate | src/cpu.c:1332,1354,1393; tests/cpu_test.c:2379,2413 — full record capacity/sequence preflight, preserved prior records and exact order. | closed |
| T-04-11 | Denial of service | trace spans and masks | high | mitigate | src/cpu.c:1282,1173; tests/gates/core_mutation.cmake:319 — trace spans/masks/aliases and bounded execution. | closed |
| T-04-12 | Spoofing | fixture content authority | high | mitigate | cmake/GenerateFixtureIndex.cmake:109; runner/run.c:387,440,548; tests/gates/runner_metadata.cmake:128,156 — fatal authority validation and actual-byte digest binding. | closed |
| T-04-13 | Tampering | synthetic producer wiring | high | mitigate | CMakeLists.txt:91,159,165; tests/gates/runner_metadata.cmake:32,69,79 — shared canonical source/options and separate index authority. | closed |
| T-04-15 | Spoofing | producer PASS/status | high | mitigate | OPEN: runner/run.c:252 and :299 accept the parser terminal PASS/0. A matching authoritative row could produce evidence PASS; an empty present production index does not implement the declared complete-mailbox prerequisite. | open |
| T-04-16 | Information disclosure | JSON and trace output | high | mitigate | runner/result.c:34,93; runner/run.c:322; tests/runner_test.c:835; tests/gates/runner_case.cmake:104,344 — escaping/path exclusion and trace write/close failure precedence. | closed |
| T-04-14 | Denial of service | CLI file/option bounds | high | mitigate | runner/main.c:98,199; runner/run.c:143,204,242; tests/gates/runner_case.cmake:294,367; tests/runner_test.c:703 — checked bounds, sentinel reads and cleanup. | closed |
| T-04-17 | Repudiation | actual result evidence | high | mitigate | tests/gates/runner_case.cmake:74,125,178,231,302,309 — real stdout, same validator, semantic controls and changed-byte refusal. | closed |
| T-04-18 | Tampering | semantic/time/dispute fields | high | mitigate | cmake/CheckResult.cmake:122,215,266,427,468,678; tests/gates/runner_case.cmake:206-229,335 — exact numeric, semantic, trace and dispute consistency. | closed |
| T-04-19 | Spoofing | installed executable identity | high | mitigate | tests/gates/runner_relocation.cmake:29,38,55,127,195,240 — moved prefix, executable/input/result/trace identity and missing-executable control. | closed |
| T-04-20 | Tampering | final source and verification coverage | high | mitigate | CMakeLists.txt:66; tests/core.cmake:97; tests/gates/core_mutation.cmake:246,293; final CI/ASan/hygiene JUnit inventories — canonical compile/control wiring, no compile-failure semantic substitute. | closed |

The shared supply-chain identifier represents thirteen authored conditional rows; all instances are preserved in the discovery register. Blocking counts include only open threats at or above high. No below-threshold open threats were found.

## Remediation Required

Reject zero case IDs for both completed terminal states in the canonical parser. Add a real mailbox assertion and producer regression that reaches this boundary with valid identity; a mismatched digest refusing before execution is not sufficient. Then rerun focused CPU/runner/result checks and targeted security verification. The downstream result validator already rejecting PASS/0 does not repair the producer.

## Accepted Risks Log

No accepted risks. No transfer or owner waiver was inferred.

## Scope and Limits

This verifies the authored register, not arbitrary native-host safety or hardware qualification. Preserve unresolved probe/policy flags, the pending human guide review, five exception sources available only through the lower API, and the separate write-trace zero-result/payload review concern. No private ROM/BIOS content was accessed or executed.

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|---|---|---|---|---|
| 2026-11-11 | 21 | 19 | 2 | gsd-security-auditor; parent recorded returned evidence |

## Sign-Off

- [x] All threats have authored mitigate dispositions.
- [x] No accepted risks were fabricated.
- [ ] Blocking mitigations verified closed.
- [ ] status: verified.

**Approval:** pending technical remediation.
