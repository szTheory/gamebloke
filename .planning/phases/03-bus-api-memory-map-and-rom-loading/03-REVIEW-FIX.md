---
phase: 03-bus-api-memory-map-and-rom-loading
status: all_fixed
mode: recovery-follow-up
---

# Phase 03 review fixes

The full core review and earlier parser follow-ups are retained in the archived
review/fix reports. The daemon restart left the last two findings unapplied;
this follow-up completes them without repeating unchanged core work.

## Recovered fixes

- Unmodeled IDs are checked as exact occurrences and total row counts, not a
  deduplicated set. Duplicate insertion and duplicate replacement are rejected.
- Both table collectors require the documented outer pipes and a blank, real
  ATX heading, or end-of-document boundary. Left-only, right-only, absent outer
  pipes, and `#text` continuations are rejected rather than omitted.
- The canonical profile guide states the supported table format.

## Evidence

The two new control groups failed before the fixes with `gate PASSED but must
fail`. After the fixes, all 29 profile checks passed. Canonical CI passed with
292 expected, 290 run, and the two permitted owner-logo skips; hygiene passed
with 200 expected, 198 run, and the same skips. Inventory post-checks passed.
All prior CI, hygiene, and sanitizer inventory entries remain present.

The independent recovery delta review is clean in `03-REVIEW.md`. The last C
source/test changes previously passed ASan/UBSan (38/38); this recovery changes
only CMake profile checking, its controls, and narrative. No sanitizer rerun is
claimed for the CMake-only recovery.

## Preserved limits

LOAD-02 real-logo comparison remains partial/owner-pending. No real logo or
BIOS data was acquired. Project timing, fill, latch, and ROM-tail choices remain
project policy rather than hardware qualification. AGENTS/config user edits
remain outside the commits for this fix.
