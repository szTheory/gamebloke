# Phase 3 hosted CI stop

Draft PR: https://github.com/szTheory/gamebloke/pull/6

Run: https://github.com/szTheory/gamebloke/actions/runs/42930902436

The owner-authorized red-CI stop is active. No source fix, retry, new push,
ruleset bypass, merge or Phase 4 start was performed after observing the failure.
This handoff is local bookkeeping and is not pushed, because a push or PR-body
edit would trigger another run. Main remains unchanged.

## Observed failure

The Linux GCC 15.2.0 lane configured successfully and built both core libraries,
then failed compiling the bus and loader test executables. At
`tests/state_check.h:136`, the shared canonical-state digest helper passes
`machine->rom == NULL ? 0u : 1u` to a `uint8_t` parameter. GCC reports:

```text
error: conversion from 'unsigned int' to 'uint8_t' may change value [-Werror=conversion]
```

The conditional result is bounded to zero or one. Recommended remedy: explicitly
convert that result to `uint8_t`, retaining all warning/error flags. This is a
recommendation, not an applied or verified fix. The GCC lane never reached its
tests, so no GNU test-pass or full hosted-green claim is supported.

## Other observations at the stop

- Hosted hygiene succeeded: 199 checks ran with two documented owner-data skips,
  and its inventory reconciliation passed.
- Hosted Linux Clang ASan/UBSan succeeded: all 42 current checks ran, zero skipped,
  and inventory reconciliation passed.
- Change classification and title checks succeeded.
- The macOS build remained queued at the last observation. The aggregate required
  check was not complete. No further waiting, retry or cancellation was initiated.
- Hygiene test time was 37.87 seconds; sanitizer test time was 36.47 seconds.
  GCC failed before tests and macOS had not run, so the complete critical path
  cannot be measured yet. Logs show one configure/build per lane; no duplicate
  setup/build remedy, cache, concurrency or timeout change is justified.

## Next actor and scope

The owner decides whether to authorize the narrow test-helper portability fix
and bounded local/hosted verification on this same PR. Before another hosted run,
check existing-run status and validate the exact GCC warning configuration; do
not weaken flags or blindly rerun the unchanged head. Refresh source fingerprints
after an authorized source change.

Separately, the approved production logo digest and manual claim review remain
owner-pending in `03-UAT.md`. A green CI run cannot resolve them or authorize
Phase 4. The existing `pending` decision is preserved without asking the owner
to reaffirm it.

## Lesson

AppleClang and sanitizer success did not establish warning-clean GNU compilation
for the shared test helper. Use the existing GCC lane or a matching local GNU
check for concrete portability changes before publication; keep compiler
identity and warning flags in the evidence. No additional CI job is needed for
this already-covered failure class.

## Authorized bounded continuation

The owner replied `yes` to the proposed explicit conversion and bounded local/
hosted retest on the existing PR. This authorizes resolving this diagnosed
compiler failure; it does not resolve the logo authority or manual review,
authorize merging while acceptance is pending, or start Phase 4. The standing
stop applies again if the new hosted run is red. Historical observations above
are retained as observations at the original stop.

The test helper now explicitly converts the bounded zero-or-one expression to
`uint8_t`. Warning flags and production code are unchanged. The bounded local verification below has completed. The next hosted run must
verify the pushed head; the earlier hosted failure is not erased.

## Bounded local retest evidence

- Linux aarch64 container, the existing pinned Ubuntu 26.04 image, GCC
  15.2.0 and CMake 4.2.3: canonical CI workflow passed, 294 checks ran, two
  documented owner-data skips, and inventory reconciliation passed. The exact
  warning policy still includes `-Wconversion` and warnings as errors.
- The first container attempt compiled the fixed code successfully but failed
  the nested digest-parser configure. A disposable copy of the local runner
  installed `make` alongside its original tool packages; the focused parser
  fixture then passed, followed by the full canonical workflow. Repository
  runner/package files were not changed. Preserve the original failure log;
  consider the local runner tool-package omission in future maintenance.
- Fresh installed AppleClang 21.0.0.21000101 ASan/UBSan configuration, build,
  all 42 current checks and inventory reconciliation passed, zero skipped.
  The initial plain preset attempt stopped at configure because `clang-21` is
  absent locally; the successful run explicitly selected the installed compiler.
- Retained local logs: `build/phase03-gcc-retest/linux-ci.log` (initial container
  failure), `linux-ci-with-make.log` (successful bounded retest),
  `local-asan.log` (initial unavailable-compiler configure), and
  `local-asan-installed-clang.log` (successful fresh sanitizer run), plus the
  retained sanitizer JUnit and compiler cache in the same directory.
- Inline review: the conditional can produce only zero or one, both exactly
  representable by `uint8_t`. The explicit conversion preserves digest input
  values and does not weaken state comparisons or touch production code.
  No broader code/security re-audit or new test was warranted for this cast.

Use the live checks on PR #6 for the hosted verdict of the corrected head.
Stop again on any red hosted result; keep the PR draft and preserve the manual
acceptance gate even if every automated lane becomes green.
