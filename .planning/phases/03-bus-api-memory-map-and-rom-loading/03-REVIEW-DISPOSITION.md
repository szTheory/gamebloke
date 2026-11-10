---
phase: 03
review: 03-REVIEW.md
titles: json
findings:
  - id: WR-01
    severity: warning
    disposition: open
    title: "Profile policy rows are validated outside their sections"
  - id: WR-02
    severity: warning
    disposition: open
    title: "Refusal snapshots omit allocator and ROM identity"
open: 2
total: 2
recorded: "2026-11-10T19:36:31Z"
---

# Phase 03: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-01 | warning | open | `cmake/CheckProfile.cmake:431-442` |
| WR-02 | warning | open | `tests/state_check.h:34-60`; `tests/loader_test.c:212-225`; `tests/bus_test.c:225-263` |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.

