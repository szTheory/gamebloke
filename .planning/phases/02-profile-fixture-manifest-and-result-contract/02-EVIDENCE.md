# Phase 2 Evidence Record

Evidence label for every entry below: **observed locally** unless a later section says otherwise. Dates are UTC.
No absolute local paths, scratch locations, or non-noreply email addresses appear in this file.

## CMake 3.25 floor

- Date: 2026-11-10. Plan 02-07 Task 2 (VERIF-01 floor parity for the Phase 2 validators; RESEARCH Pitfall 5).
- Toolchain: `cmake version 3.25.3` from the official Kitware release tarball `cmake-3.25.3-macos-universal.tar.gz`
  (https://github.com/Kitware/CMake/releases/download/v3.25.3/). No PyPI cmake package was used.
- Tarball SHA-256: `771548ed2abae17f3fd28dcfa572ba3fe9f970652a72c36c2e1aafdee93a234e`, re-verified with `shasum -a 256`
  against its line in Kitware's `cmake-3.25.3-SHA-256.txt` from the same release (lines identical), the same digest
  Phase 1 recorded in 01-EVIDENCE.md. The tarball was freshly extracted into a temporary directory outside the repository
  for this run; used locally only, never in CI.
- Command: `<cmake 3.25.3> --workflow --preset ci` from the repository root after `rm -rf build/ci`, with the binary's
  directory first on `PATH` and the `CMAKE` environment variable set to the same binary for the scan scripts.
  The configure cache of the run records CMake 3.25.3 (`CMAKE_CACHE_MAJOR_VERSION` 3, `MINOR` 25, `PATCH` 3).
- Result: exit 0, `100% tests passed, 0 tests failed out of 252`, post step 1 of 1 passed, wall-clock 31 s, and the
  summary line:
  `inventory: expected 252, ran 250, skipped 2 (allowed 2)`
- No source fix was needed: the Phase 1 `cmake_minimum_required(VERSION 3.25)` pin in each `-P` script held, and no
  newer `string(JSON)` subcommand is used (the validators use only GET, TYPE, MEMBER, LENGTH, REMOVE, SET, EQUAL).
- Confirmed passing in the floor log (B1 and I1 re-proven on 3.25.3):
  - `json.nc.trailing_content` Passed (the structural trailing-content walk rejects text after the root object)
  - `json.pos.real_docs` Passed (the strict loader accepts every real authority document)
  - `json.nc.no_echo_guard` Passed (the no-echo helper detects a leaked marker)
  - `json.pos.strict_clean` Passed (the parse-success test, `ERROR_VARIABLE` empty or `NOTFOUND`, holds on 3.25.3)

## Linux container

- Date: 2026-11-10.
- Docker daemon architecture (`docker info --format '{{.Architecture}}'`): `aarch64`; Docker server 29.5.2.
- Container architecture printed by `scripts/linux-lane.sh`: `aarch64` (native, not emulated; the x86_64 proof is the CI
  runner).
- Image: the `ubuntu:26.04` multi-architecture index digest pinned in `scripts/linux-lane.sh`.
- `sh scripts/linux-lane.sh ci gcc`: exit 0, about 54 s wall-clock. Versions printed: CMake 4.2.3, GCC 15.2.0
  (Ubuntu 15.2.0-16ubuntu1). `100% tests passed, 0 tests failed out of 252` and the summary line:
  `inventory: expected 252, ran 250, skipped 2 (allowed 2)`
- curl is installed by the lane's apt line, and the Phase 2 fetch controls (`fetch.*`, which use `file://` under
  `GBL_FETCH_ALLOW_FILE=1`) ran and passed in the container.
- The asan lane was not re-run in this plan: no source, test, or inventory under it changed (`tests/inventory/asan.txt`
  is untouched, 16 expected, 16 ran, 0 skipped).

## Timings

I5 CI time budget. Default CMake 4.4.3, Apple clang 21.0.0 on arm64 macOS, fresh build directories (`rm -rf build/ci
build/hygiene`), parallel ctest as set in the presets:

| Workflow | Wall-clock | Ctest test time | Summary line | vs 3-minute job timeout |
|----------|------------|-----------------|--------------|-------------------------|
| `cmake --workflow --preset ci` | 34 s | 33.7 s | `inventory: expected 252, ran 250, skipped 2 (allowed 2)` | 19 percent |
| `cmake --workflow --preset hygiene` | 25 s | 24.4 s | `inventory: expected 182, ran 180, skipped 2 (allowed 2)` | 14 percent |
| `<cmake 3.25.3> --workflow --preset ci` (floor) | 31 s | not recorded | same as the ci row | 17 percent |
| `sh scripts/linux-lane.sh ci gcc` (container, aarch64) | 54 s | 24.1 s | same as the ci row | 30 percent |

Five slowest Phase 2 tests (ci lane, seconds): `manifest.nc.missing_field` 1.78, `json.nc.trailing_content` 1.04,
`result.nc.missing_field` 0.70, `profile.nc.missing_field` 0.49, `profile.nc.unmodeled_missing` 0.44. The slowest tests
overall are Phase 1 tests: `hygiene.tree` 3.4, `signature.signal` 2.3, `traps.nc.resumes` 2.3, `hygiene.signal` 2.2.

No workflow is above the 90-second flag, and no test is anywhere near it. The reduction step if a hosted runner needs it
(fewer real-document variants in `json.nc.trailing_content`, or a higher ctest parallel level in the preset) is therefore
not applied. Escalation: the orchestrator may raise `timeout-minutes` in `.github/workflows/ci.yml` in the phase PR if
the measured CI time on the hosted runners requires it; this plan did not edit `ci.yml`. The Phase 1 CI measurements
(macOS build the slowest job at 36 s) predate the Phase 2 validators; the hosted runs of the phase PR are the real
measurement.

## Pending at publication

Both items are owned by the orchestrator and neither is claimed here.

- Live CI run of the phase PR: all lanes green and `CI required` success on the hosted runners. Next action: the
  orchestrator publishes the phase PR and records the run in the phase verification.
- RESEARCH A1: that the GitHub `ubuntu-26.04` runner image already has `curl`. The fetch controls need it. The Linux
  container proof above installs curl itself, so it does not answer A1. Next action: the orchestrator reads the first
  hosted run of the phase PR; if curl is absent the fix is one `apt-get install curl` line in the workflow (which this
  plan may not edit).
