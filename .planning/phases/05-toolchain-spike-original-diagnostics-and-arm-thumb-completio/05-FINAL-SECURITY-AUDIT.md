---
phase: 05-toolchain-spike-original-diagnostics-and-arm-thumb-completio
status: SECURED
scope: local implementation and recorded 13-file Plan25 candidate over 8e21ef6
declared_threats: 51
unresolved_declared_mitigations: 0
asvs_level: 1
blocking_threshold: high
final_hosted_qualification: pending
owner_phase_acceptance: pending
---

# Phase5 final local security audit

The independent gsd-security-auditor found no absent declared mitigation or
unresolved security blocker. This verifies implemented controls and local
evidence; it does not complete Phase5 or qualify the final candidate on hosted
CI. The auditor returned its structured verdict without writing files under its
single-writer contract; the parent persisted that result here.

## Evidence inspected

E1 — Toolchain: scripts/fetch-toolchain.sh:71 validates exact release, host, URL,
archive size and SHA before extraction. Lines212–308 restrict members/links;
342–366 stage/promote installation. Passing toolchain.offline evidence covers
redirect, size, digest, containment and previous-install controls.

E2 — Generated authority: cmake/BuildArmDiagnostics.cmake:213,
cmake/CheckDiagnosticAuthority.cmake:3 and
cmake/GenerateFixtureIndex.cmake:323 bind source bytes/modes/history, built
artifacts and compiled index. Actual artifact.admission.controls passes changed
source/output/result bindings, symlink escapes and stale-index cleanup refusals.
Object/final-ELF attributes, executable disassembly and exact-name scans remain
separate checks.

E3 — CPU boundaries: src/cpu.c:3135 checks descriptor counts, costs, counter
headroom and spans. The complete trace preflight at3492 precedes execution at3749.
tests/cpu_test.c:13351 seeds non-default state and caller guards, then checks
exact-fit/short-capacity postconditions. Current sanitizer evidence includes
boundary selectors and semantic mutants for late refusal, maximum trace, effect
ordering, loaded PC, flags and separate BL halves.

E4 — Receipts: tests/gates/diagnostic_receipt.cmake:51 validates source, manifest,
runner/native executables, result/ROM bytes, semantic IDs and complete sets;
255–299 requires admitted execution and rechecks identities. Actual current
diagnostics evidence contains every722 admitted output receipt and dependency/
receipt corruption controls.

E5 — CI/evidence: cmake/CiPreflight.cmake:115,
scripts/collect-hosted-proof.py:90 and
tests/gates/final_diagnostic_proof.cmake:695 enforce admitted presets, producer
authority, exact job membership/budgets, source/run/log/output/API bindings and
synthetic isolation. Concrete negatives cover stale source, incomplete sets,
wrong hashes, red/missing jobs, overruns, mixed tags and external aliases.
Final current hosted execution remains pending, distinct from checker controls.

E6 — Installed/refusal: tests/gates/runner_relocation.cmake:234 compares moved
installed/build executable identities and exact positive/broken/stop JSON and
trace. runner/run.c:408 uses compiled authority and refuses mismatched actual
input before execution. Independent fresh read-only invocations observed PASS,
GUEST_FAIL, GUEST_STOP/no-bios-swi and wrong-image
REFUSED/fixture-digest-mismatch with ran=false, zero progress and
guest-not-executed.

E7 — Rights/claims: diagnostics/armv4t/NOTICE:5 preserves fixture-specific public
CI permission and forbids generated ROM redistribution. Workflow uploads remain
failure JUnit only. Final/probe records preserve qualification and judgment
limits rather than treating them as passes.

## Declared threat mapping

All50 numbered dispositions are mitigate, with implemented controls found.
There are no accepted-risk or transfer entries. The shared conditional
package-manager threat is inactive.

| Threat | Severity | Local mitigation evidence |
| --- | --- | --- |
| T-05-01 |high |E1 exact host pins, pre-extraction digest, member/link validation and promotion |
| T-05-02 |medium |E1 redirect URL excluded from receipts/logs, redirect/token negatives |
| T-05-03 |high |E2 BuildArmDiagnostics:433, diagnostic_linkmap:100, attributes/disassembly/scans |
| T-05-04 |medium |E2 complete proof and reason-specific architecture/runtime/map/signature refusals |
| T-05-05 |high |E5 admitted presets, verified tools, dependencies/failure handling |
| T-05-06 |medium |E5 source/run/job/output and exact approved180/300second budgets |
| T-05-07 |high |E2 artifact path/type/digest checks, immutable generated index |
| T-05-08 |medium |E2 artifact roundtrip/legacy controls separate source/tool/output |
| T-05-09 |high |E2 tracked inputs, forced mode comparison, ancestry and byte hashes |
| T-05-10 |medium |E4/E7 semantic pairs and narrow NOTICE, expectations do not grant rights |
| T-05-11 |high |E3 cpu.c:1725 SWP shared preflight before ordered effects |
| T-05-12 |medium |E3 bounded slots/work/trace, swap/boundary selectors |
| T-05-13 |high |E3 cpu.c:1400 PSR/bank effects captured before commit |
| T-05-14 |medium |E3 bounded PSR unit and counter/capacity checks |
| T-05-15 |high |E3 cpu.c:1088 ARM ALU descriptor/common preflight |
| T-05-16 |medium |E3 bounded ALU/trace, shift/arithmetic controls |
| T-05-17 |high |E3 cpu.c:1577 multiply captured operands/checked costs |
| T-05-18 |medium |E3 bounded multiply timing, signed/unsigned/flag-mutant controls |
| T-05-19 |high |E3 cpu.c:1785 transfer/refill preflight |
| T-05-20 |medium |E3 transfer/loaded-PC capacity/refusal controls |
| T-05-21 |high |E3 cpu.c:1909 subword descriptor/checked transforms |
| T-05-22 |medium |E3 subword/alignment selectors/bounded slots |
| T-05-23 |high |E3 cpu.c:2716 block descriptor/full effect and cost preflight |
| T-05-24 |medium |E3 maximum LDM/STM exact/short capacity and overflow controls |
| T-05-25 |high |E3 captured block banks/SPSR/result-driven refill tests |
| T-05-26 |medium |E3 bounded block edges/unchanged refusal state |
| T-05-27 |high |E3 cpu.c:2052 Thumb ALU/high-register descriptors |
| T-05-28 |medium |E3 Thumb ALU/high selectors/common bounds |
| T-05-29 |high |E3 cpu.c:984 Thumb transfer preparation before effects |
| T-05-30 |medium |E3 Thumb transfer/SP selectors/common preflight |
| T-05-31 |high |E3 cpu.c:2169 Thumb multiple/stack captured unit |
| T-05-32 |medium |E3 maximum stack/multiple/short-boundary controls |
| T-05-33 |high |E3 cpu.c:2306 independently decoded Thumb control/BL units |
| T-05-34 |medium |E3 separate BL retirement/budget/trace, no fused loop |
| T-05-35 |high |cpu.c:2991/3650 exact cause classification/actual stop controls |
| T-05-36 |medium |Profile source/version/scope drift controls and E7 limits |
| T-05-37 |high |cases-arm.S:11/CheckArmCoverage:348 actual fixed-ID assertion companions/scans |
| T-05-38 |medium |CheckArmCoverage:254 ID uniqueness/bodies/omissions/stops, E4 results |
| T-05-39 |high |cases-thumb.S:8 coverage format controls/positive-broken receipts |
| T-05-40 |medium |E3/E4 separate BL/native API receipts retain boundary distinctions |
| T-05-41 |high |E2 complete722-output authority, source modes/history/loaded bytes |
| T-05-42 |medium |E2/E5 independent source/output/index negatives and host/cmp distinctions |
| T-05-43 |high |core_mutation:517 exact mutants need named semantic failures/clean companions |
| T-05-44 |medium |E3 complete state/buffer checks, actual late-refusal/trace-loss mutants |
| T-05-45 |high |E4 complete admitted set, required absence/all-skip/stage escapes fail |
| T-05-46 |medium |diagnostic_dependency:69/221 absence/unusable/build distinction, BIOS/mailbox refusals |
| T-05-47 |high |E5 admitted enforcement/proof escape negatives/complete routing |
| T-05-48 |medium |E5 stale/wrong/missing identities/jobs refused, final hosted candidate still pending |
| T-05-49 |high |E6 compiled authority/actual-byte refusal/moved install/missing executable |
| T-05-50 |medium |E7 local evidence separated from pending rights/probes/hardware/owner judgments |
| T-05-SC |high |Conditional inactive, no npm/pip/cargo installation or new library path found |

## Recent boundaries and fresh checks

scripts/linux-lane.sh:51 clones history and overlays Git-selected changes,
including working deletions/modes. Ignored private/build content is excluded;
only the selected snapshot is mounted read-only with4CPU/8GiB. EXIT cleanup and
separate terminating signal handlers are present. The retained disposable
sentinel test covers exclusions, modes, deletion, untracked files and history;
its original mode failure and passing correction remain available.

diagnostic_setup.cmake:93 captures the actual final runner hash after admitted
execution/rebuild. The existing checker still refuses a stale runner; its
explicit new negative is diagnostic_receipt_controls.cmake:69.

The auditor independently rechecked all13 recorded file byte/mode identities,
four saved JUnit inventories,15 successful terminal receipts and fresh full
preflight. Its initial direct-control assertion expected HARNESS_FAIL; the
documented actual refusal is REFUSED. The corrected audit assertion passed
without implementation/evidence change. No source or shared file was written
by that auditor. No unregistered threat flags were found in existing summary
sections; Plan24/25 completion summaries remain intentionally absent.

## Pending gates

Exact-candidate hosted execution/new authority, ancestry-preserving merge choice
and owner guide/bespoke judgments remain pending. Hardware, BIOS, games,
controllers, release, project-wide licensing, unclassified probes, descriptor-less
prohibitions and historical process deviations remain unqualified/unresolved.
The later parent correction of stale profile evidence wording is a separate
bounded document/consistency change, not new hardware or hosted qualification.
