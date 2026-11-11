---
phase: 04
review: 04-REVIEW.md
titles: json
findings:
  - id: CR-01
    severity: critical
    disposition: fixed
    title: "A completed PASS mailbox can use case ID zero"
  - id: CR-02
    severity: critical
    disposition: fixed
    title: "Thumb immediate word loads do not rotate unaligned data"
  - id: CR-03
    severity: critical
    disposition: fixed
    title: "Conditional ARM SWI and coprocessor instructions ignore their condition"
  - id: WR-01
    severity: warning
    disposition: fixed
    title: "Verification guide omits the Phase 4 validator reason codes"
open: 0
total: 4
recorded: 2026-11-11T12:32:20.488Z
---

# Phase 04: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| CR-01 | critical | fixed | 04-REVIEW-FIX.md (not in the current review) |
| CR-02 | critical | fixed | 04-REVIEW-FIX.md (not in the current review) |
| CR-03 | critical | fixed | 04-REVIEW-FIX.md (not in the current review) |
| WR-01 | warning | fixed | 04-REVIEW-FIX.md (not in the current review) |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
