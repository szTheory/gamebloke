---
status: testing
phase: 03-bus-api-memory-map-and-rom-loading
source: [03-VERIFICATION.md]
started: 2026-11-11T00:18:48Z
updated: 2026-11-11T00:18:48Z
---

## Current Test

number: 1
name: Approved production logo digest
expected: |
  The owner supplies the approved logo-window digest using a file already held,
  through the established reviewed workflow. A rebuilt production core reports
  checked match or advisory mismatch against that authority. No raw bytes are
  requested or acquired. This verifies a configured comparison, not rights or
  broad hardware compatibility.
awaiting: owner-held authority data; preserve the existing pending decision

## Tests

### 1. Approved production logo digest
expected: The owner runs `sh scripts/signature-digest.sh logo <file>` with the file outside the repository, and supplies the printed row through the existing Phase 3 PR. Apply the approved profile-version and narrative policy before claiming activation, then rebuild and verify the production diagnostic. Synthetic match/mismatch witnesses already pass but cannot complete this item.
result: [pending]

### 2. Review project-policy descriptions
expected: The timing, initialization, latch and ROM-tail descriptions in `docs/profile-agb.md` and `docs/bus-api.md` remain labelled project policy or unverified behavior, without hardware-accuracy or broad-compatibility claims. Human judgment is required by the verification workflow; automated review is recorded separately.
result: [pending]

### 3. Review synthetic-evidence limits
expected: `docs/verify.md` describes synthetic logo tests as comparison mechanics without presenting them as genuine Nintendo identity, canonical acceptance or redistribution permission.
result: [pending]

### 4. Before any ROM-tail hardware-accuracy claim
expected: Obtain suitable hardware evidence before changing the current unverified label. This is a future claim prerequisite, not an additional requirement to accept the current software policy; preserve the existing label meanwhile.
result: [pending]

## Summary

total: 4
passed: 0
issues: 0
pending: 4
skipped: 0
blocked: 0

## Gaps

No open implementation gaps are reported. The production authority and human
judgment items above remain unresolved. Do not advance Phase 3 or start Phase 4.
Resume with `$gsd-verify-work 03` after the owner supplies the authority and review.
