# Plan23 skip-classification review fix

The new negative control reproduced the warning: CTest returned success with
three skips even though two CMake commands failed. Both failures included the
same text as the genuine optional-absence marker.

`cmake/DiagnosticSkip.cmake` now restricts the diagnostic tests to a whole-output,
reason-specific absence line. A CMake fatal error or forwarded tool output no
longer matches it. Required diagnostic commands disable skip matching entirely.
Existing literal native inventories describe the optional baseline; required
mode remains stricter through its commands, receipt guard and CTest properties.
No extra process, dependency, concurrency or producer input was introduced.

Actual child CTest/JUnit controls now observe two failures and one genuine skip
in optional mode, and three failures with zero skips in required mode. The
inventory post gate rejects both failed suites despite their baseline `skip-ok`
tags. The outer dependency control passes only when those failures stay visible.
The genuine-absence-only suite still passes its normal inventory post check.

After the fix, all744 diagnostic tests in the native optional subset passed
their expected outcomes in7.30 seconds:724 guest/setup/set entries visibly
skipped, API boundaries and dependency controls ran, and none failed. Actual
required configuration also succeeded with the pinned tools; CTest discovery
confirmed skip matching disabled for all724 applicable tests. Exact receipts
are under `build/phase05-plan23-skip-review/`, including `red.log`, `green.log`,
`optional.xml`, the child marker projects, and configuration logs.

The root `ci` and `asan` presets now explicitly select the same unqualified
proof stage already exercised by the candidate. This makes the source-proof
publication independent of an old local CMake cache. A plain canonical `ci`
configure confirmed that stage. Plan24 must explicitly set all required native
and diagnostic presets back to admitted after fresh proof/readmission, including
overriding old proof caches, and provide enough Git history for source ancestry.

This fix and staging follow-through remain uncommitted for independent review.
The three changed producer inputs, candidate digest, historical canonical pins
and pending fresh local/hosted proof sequence remain unchanged. No new hosted
attempt or qualification is claimed.
