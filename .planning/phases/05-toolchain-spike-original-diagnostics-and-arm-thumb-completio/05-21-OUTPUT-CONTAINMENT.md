# Fixture-index output containment

Re-review confirmed the first three admission fixes and the consumer runtime
correction, then reproduced an adjacent generator-output escape. Checking ROM
paths did not protect generated headers or the generated control manifest.

The generator now requires an explicit absolute `GBL_OUTPUT_ROOT`, independently
supplied by every direct caller. It checks all three output paths before any
mutation, removes only safely contained stale headers, and uses the shared
containment helper for directory creation and every write. An escaping target
is left untouched. Missing ROM artifact roots remain supported by metadata-only
lanes; their generated-output boundary is still mandatory.

The actual new control first failed with
`output-symlink-was-not-refused:production:parent` while the old generator
returned success. After the fix, nine actual symlink controls pass: each of the
production header, control header and control manifest is tested through a
parent link, a final-file link and a dangling final link. Outside synthetic
sentinels retain their bytes, dangling targets remain absent, and a clean
contained-output companion generates successfully. These controls run inside
the existing compatibility selector and registered hygiene admission control.
One initial GREEN attempt exposed an assertion reason-spelling mismatch; that
test setup was corrected before the passing run, without changing the refusal.

## Current verification

All checks ran serially in the background after the last implementation change.
Times are wall/user/system seconds and do not measure emulator throughput.

| Check | Actual result | Seconds |
| --- | --- | --- |
| `artifact_case.cmake CASE=compatibility-and-controls` | All output, source, identity, legacy and path controls pass |2.65 wall |
| `cmake --workflow --preset ci` | Canonical checks and post inventory pass |158.21 /153.66 /18.30 |
| `diagnostics_case.cmake CASE=all-admitted` |722 production results:355 positives,355 deliberate failures,12 exact named stops |28.15 /25.75 /1.91 |
| `artifact_case.cmake CASE=admission-controls` | Source/output/pin/index refusals and clean production companions pass |11.00 /10.39 /0.36 |
| `git diff --check` | Pass |Not benchmarked |

Exact local receipts are under `build/phase05-admission-performance/`:
`output-red.log`, `output-green.log`, and
`output-current-{ci,all,controls}.{log,json}`. All722 results still use the
immutable source-only authority and the same actual output pins. Producer
inputs remain unchanged. The shorter later times are observations, not a claim
that the small output-guard change caused those savings.

Independent closure of the new output finding is next. Final cold compiler,
sanitizer and hosted checks remain in the later Phase5 plans. No hosted run,
ROM upload, rights change, external personal-file access or Phase6 work occurred.
