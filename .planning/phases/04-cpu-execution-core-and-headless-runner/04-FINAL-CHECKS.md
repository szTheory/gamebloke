---
phase: "04"
status: technical_checks_passed
checked: "2026-11-11"
---

# Phase 04 final technical checks

After the review fixes and TAP-plan correction, the source is unchanged through these checks. This records local technical evidence; it does not approve the phase or imply hosted CI success.

| Check | Actual result | Retained local evidence |
| --- | --- | --- |
| Canonical AppleClang regression command `cmake --workflow --preset ci` | Exit 0; 329 expected, 328 ran, one allowed owner-local skip; post inventory passed. CTest 77.04 seconds. | `build/phase04-review/final-ci.log`, `build/ci/junit.xml` |
| Fresh AppleClang ASan/UBSan configure, build, asan and asan-post | Exit 0; 72 expected and ran, zero skipped; post inventory passed. CTest 64.06 seconds. | `build/phase04-review/final-asan.log`, `build/asan/junit.xml` |
| Existing pinned Ubuntu 26.04 aarch64 GNU lane | Exit 0 with GCC 15.2.0 and CMake 4.2.3; 329 expected, 328 ran, one allowed owner-local skip; post inventory passed. CTest 77.20 seconds. | `build/phase04-review/final-gnu-ci.log` |
| Final source tree hygiene and rights signatures | Both exit 0, 258 files inspected. Ignored private inputs remain outside the public tree. | Direct script output; `hygiene: ok`, `signature: ok` |
| Focused post-review regression | Mailbox, alignment, decode, real producer/result contract and metadata all passed after TAP correction. | `04-REVIEW-FIX.md`, standard re-review and targeted security recheck |

The GNU lane reused the existing local runner and pinned image, with the already documented disposable `make` package correction so nested configure controls run. It did not alter repository toolchain files or host packages. It is native aarch64 evidence; the hosted x86_64 lane remains a separate check.

## Prior-phase regression gate

The current canonical CI command includes the existing Phase 1 verification controls, Phase 2 profile/manifest/result gates, and Phase 3 bus/loader controls. All ran through their current inventory. The fresh prior-verification file list is retained in `build/phase04-review/regression-files` and the workflow handoff directory. The generic workflow sniff has no CMake arm and would fall back to `true`; the actual canonical CMake command above provides meaningful regression evidence instead. No no-op is reported as a test.

## Review and claim boundaries

Independent standard code re-review is clean after three correctness fixes and one documentation fix. All four finding dispositions are fixed. Targeted independent security recheck closed all authored threats without risk acceptance. The validation map remains behaviorally covered, with the new zero-ID producer and CPU regressions recorded.

The production fixture manifest/index remain empty. Synthetic rows and generated images are local validation controls and grant no redistribution rights. No private ROM was executed; the separately authorized owner-local check inspected only the bounded header window. The public owner-local skip is expected and does not create a new owner-data gate.

The subjective guide-clarity judgment, eleven unresolved probe/policy rows, two descriptor-less standing restrictions, project timing and current-latch assumptions, API-only reachability of five external exception mappings, and write-trace result-zero limitation remain explicit. No hardware qualification, broad compatibility, BIOS boot, license grant, release, or owner acceptance is claimed.

## Critical path

Measured native regression CTest times are about 77 seconds and fresh sanitizer CTest about 64 seconds. GNU and AppleClang provide distinct compiler evidence; sanitizer controls exercise instrumented paths. No cache, extra dependency, new CI job, concurrency or timeout increase was added. Hosted job timing still needs inspection after publication.
