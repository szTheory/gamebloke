# Plan24 local execution candidate

Status: local implementation and checks passed; parent review/commits and the
final admitted hosted gate remain pending. This file does not complete Plan24,
advance state, mark requirements or authorize publication.

## Identity and ownership

Actual checked-out consumer is `aabb1e247f7b44f202a41f7267730aa51d81eaf4` plus
the consumer-only candidate diff. Per-file SHA256s and the reviewed diff are
saved in `build/phase05-plan24/`; they identify uncommitted files separately from
the checked-out HEAD. Actual runtime receipts bind that HEAD and a fresh token;
the parent must bind/retest the eventual committed consumer appropriately.

Immutable producer85c/input95, the canonical manifest digest
`643f18a6ed5649b0797a75a105a64765eee4936bee985251326224ca4fdf11cb`, all722 output
pins/rights/expectations and producer bytes/modes/ancestry are unchanged.
No producer input in `diagnostics/armv4t/inputs.txt` was modified. Preserved r4
proof artifacts and historical ledgers were not rewritten. The fetcher checked
the existing pinned install; no tool download or installation occurred.

Parent owns commits, publication, shared planning/config/state and producer
admission. This executor made no staging, commit, push, CI-trigger, settings,
merge, release, LICENSE or private-content operation. Parent dirty AGENTS,
config/state/cache and handoff files were preserved. No Plan25 source was added.

## Changed files

| Files | Purpose |
| --- | --- |
| `CMakePresets.json`, `tests/toolchain.cmake` | Explicit admitted canonical presets and matching contract |
| `cmake/CiPreflight.cmake`, `tests/ci/admission_cases.cmake`, `tests/ci/preflight_cases.cmake` | Cheap admitted/history/override consistency and concrete refusal controls |
| `.github/workflows/ci.yml`, `scripts/check-workflow.sh`, `tests/ci/ci_cases.cmake` | Full native/ASan history; exact placement, needs and JUnit-only routing controls |
| `tests/ci/history_cases.cmake` | Actual independent full/depth-one clone configuration controls |
| `tests/gates/diagnostic_dependency.cmake`, `tests/gates/diagnostic_hosted_log.cmake` | Post-guard actual admitted hosted text witnesses |
| `scripts/collect-hosted-proof.py`, `tests/gates/final_diagnostic_proof.cmake`, `tests/gates/admitted_hosted_proof.cmake` | Default admitted hosted collection and independent semantic/native validation; explicit preserved producer path |
| `tests/ci/admitted_hosted_cases.py`, `tests/ci/job_budget_cases.py` | Actual local-log parsing, isolated admitted negatives and legacy compatibility |
| `scripts/linux-lane.sh` | Required ordinary make and established four-CPU/eight-GiB bound |
| `docs/verify.md`, `05-HOSTED-CHECKS.md`, this report | Commands, evidence boundaries, measurements and remaining gate |

Existing inventory names and the admitted final-output fixture/dependency
registration were reused. No inventory or producer registration churn was
needed. No new library, runtime Python dependency, job or cache was added.

## Actual controls and measured checks

All app/build/test work ran as background CLI operations without desktop
activation. Heavy work was serial; small policy/read-only checks ran separately.
Times below are wall times, not emulator throughput or fresh-host predictions.

| Actual command/check | Observed result |
| --- | --- |
| `cmake -DREQUIRE_AUTHORITY=ON -P cmake/CiPreflight.cmake` | Exact policy/input/mode/ancestry passed; final0.06s |
| `sh scripts/fetch-toolchain.sh --check --host darwin-arm64 --root build/toolchains` | Existing pinned install passed; no install |
| New `tests/ci/admission_cases.cmake` | Clean plus five proof overrides, five missing-stage fields and duplicate-stage JSON refusal passed |
| `cmake -DCASE=needs -DSRC=. -DBIN=build/phase05-plan24/ci -P tests/ci/ci_cases.cmake` | Exact policy/routing and17 result predicates passed; final0.86s |
| `tests/ci/history_cases.cmake` in fresh contained roots | Real full-history ci/ASan/all/ARM/Thumb configured; real shallow admitted ci/ASan/diagnostics refused before compiler discovery; explicit optional local ci proof configured; final16.62s |
| Initial canonical admitted `cmake --workflow --preset diagnostics` |793 passed,0failed/0skipped, strict post;83.10s |
| Final canonical admitted `cmake --workflow --preset diagnostics` after emitter integration |793 passed,0failed/0skipped, strict793/793 post;84.92s; configure2.22/build0.19/test82.37/post0.13 |
| Explicit `mandatory-final` before/after emitter | Actual exact receipt guard passed;4.05s guard-only,5.39s with text witnesses |
| Current result/identity audit and actual final workflow-log parse |722 actual guests:355PASS/355GUEST_FAIL/12stops,367IDs;19 exact native API rows; source, consumer, fresh token, hashes and progress checked |
| `cmake --preset ci`, then `cmake --build --preset ci` |2.45s/7.64s; cached proof reset to admitted; existing deliberately absent optional tool root retained |
| Focused native `ctest --test-dir build/ci -R ... --output-junit phase24-native-controls.xml` |64 selected policy/metadata/relocation/API/dependency checks passed;12.15s; setup/execution-set have two explicit allowed absent-tool skips;19 API checks executed |
| `cmake --preset asan -B build/phase05-plan24/asan -DCMAKE_C_COMPILER=/usr/bin/clang -DGBL_TOOLCHAIN_ROOT=build/phase05-plan23-absent`, then build |2.48s/7.70s; actual macOS compiler, admitted metadata, no tools/outputs required |
| Focused sanitized CPU/metadata/relocation/dependency CTest |5 passed;14.73s; no sanitizer findings |
| `cmake -DCASE=all -DSRC=. -DBIN=build/phase05-plan24/dependency -DRUNNER_BIN=build/diagnostics -P tests/gates/diagnostic_dependency.cmake` | Genuine optional absence, mandatory absence, wrong/unusable tool, BIOS refusal and mailbox controls passed |
| `python3 tests/ci/admitted_hosted_cases.py --local-log build/phase05-plan24/admitted-log.log --local-proof build/phase05-plan23-proof-r4/preflight/local-proof.json --bin build/phase05-plan24/admitted-controls-4` | Actual local log checked; isolated collector/CMake clean+negative controls passed39.62s; synthetic only |
| `python3 tests/ci/job_budget_cases.py --source-root . --bin build/phase05-plan24/legacy-controls-1 --local-proof build/phase05-plan23-proof-r4/preflight/local-proof.json` | Legacy producer/source/collector/isolation controls passed8.30s with explicit producer-only stage |
| Old real run supplied to `final-hosted-proof` | Refused `live-run-not-current-green`; never relabeled as current |
| Producer-only synthetic receipt supplied with no HOSTED_STAGE | Refused `host-source-identity-mismatch`; default final path requires admission |
| Linux helper with stub Docker, plus `sh -n` | Exact make/package,4CPU/8GiB/native-architecture/canonical-command wiring passed; no Docker/container work launched |
| Workflow/action pin policy, Python AST parsing and whitespace checks | Passed; same seven definitions/eleven names/caps/actions/needs/permissions |
| Repository hygiene and rights signature tree scans | Passed; signature scan12.52s; ignored ROM/tool/runtime outputs stayed excluded |

Raw command vectors, console reports, JUnit documents, setup/build/test/post
boundaries and identity audits remain under `build/phase05-plan24/`. The native
and ASan regexes are preserved in their timing JSON. Warm existing output reuse
is explicit; initial smoke or prior producer-proof timings do not stand for a
fresh final admitted hosted pipeline.

Within the final main-test step, actual setup/native/guest execution took26.05s,
the exact execution-set check3.18s, mandatory admitted guard plus log4.99s,
coverage14.10s, pipeline refusals2.85s and receipt corruption controls4.35s.
Those setup/guest operations are not a pure guest-throughput benchmark.

## Implementation findings and test history

The newly authored admission control actually reproduced the required-preset
proof escape before its implementation: the preflight accepted a proof-valued
ci preset. The supported unittest assertion and unmodified report were persisted
as `red.json`/`red.txt`; the installed SDK returned `RED_EVIDENCE_OK`. The clean
companion passed. After implementation that same control passed. Parent retains
all commit responsibility; no historical RED commits are invented.

Changing the final required lane to admitted exposed an evidence consumer gap:
the hosted collector recognized only pre-admission source-proof log markers.
The fix emits a separate admitted schema after the existing exact guard and
checks it in both collector and CMake. It preserves the old producer proof and
explicit producer-only format. New actual local logs provide the positive
witness; marked synthetic controls cover proof-stage escape, wrong producer,
consumer/run, missing guest, wrong status/progress, native evidence/token/set,
tool pin and live synthetic refusal. No synthetic host fields qualify hardware
or a live job.

Three initial new-driver failures were harness issues: the expected proof-stage
reason, a control-output filename colliding with its input run document, and a
validation scratch root outside the disposable source's build directory. Their
unmodified logs remain as admitted-hosted-controls attempts1–3. Corrected
controls passed on attempt4. No producer, emulator result or authority was
changed to satisfy them. No further fix loop was run.

The final source/evidence pipeline has no intentionally unwired stubs, skipped
required diagnostic tests or new external endpoints. The final hosted
verification itself remains unrun because its one-attempt authority is
exhausted; parent owns that remaining gate and any shared defect bookkeeping.

## Remaining gate and next action

See [the hosted candidate](05-HOSTED-CHECKS.md) for the concrete authority and
headroom boundary. Parent independently reviews/commits this candidate and
schedules remaining authorized local Phase5 validation. No user action is
needed for that local work. A new exact-head hosted attempt needs new owner
authority; no attempt was made or inferred here. Plan24-SUMMARY, shared state,
requirement completion and Plan25 source were deliberately left to their owners.

## Local self-check: passed

All20 owned candidate files exist and are recorded with content hashes and modes
in the ignored snapshot. Actual source authority, current result inventories and
the final workflow's parsed log agree. Preserved r4 artifact digests still match.
No generated bytecode or task-created untracked runtime output remains outside
the ignored build tree. Plan24-SUMMARY is absent as required by the hosted gate.
