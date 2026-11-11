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
