# Plan23 parsed JSON census repair

The bounded performance repair remains active and its one fresh hosted attempt
is unused. This reviewed change replaces repeated recursive parsing with an
exact census of the same CMake-parsed tree. It changes a producer input, so the
prior41931a2 partial proof is preserved historically and a new freeze plus
complete fresh proof is required before any readmission.

## Concrete change and invariant

The strict loader already parses the full document and rejects comments,
trailing commas and trailing content. Its duplicate-key check compares raw
member tokens with the retained parsed-member count. The old recursive census
reparsed a complete parent array for each child, including722 fixture rows.

The new private census obtains CMake's serialized parsed object/array, strips
strings with the exact existing string-literal expression, then counts retained
member tokens and structural opening braces/brackets. Every parsed object
member appears once in serialization, including nested arrays. Duplicate or
decoded-alias keys still collapse in the parsed tree and therefore still cause
the existing raw/parsed count mismatch. Overwritten subtrees cannot compensate
for the dropped duplicate member. Scalar roots still produce three zero counts.
No input parser, reason code, raw strictness guard, no-echo behavior, permitted
leniency, inventory, job, dependency, concurrency or timeout policy was changed.

## Actual measurements and controls

On the exact same canonical manifest, old and new counts are15886members,
5055objects and1445arrays. The isolated original census used11.6008seconds user
CPU and34.04wall; replacement0.0998CPU and0.52wall. Shared-machine contention
means this is an attributable census measurement, not an end-to-end speedup or
hosted runtime guarantee. The comparison is a performance baseline, not a
manufactured failing behavior.

An independent Python parsed-tree walk supplied exact counts for all nine actual
checked documents, including the large diagnostic ledger. All counts matched.
Registered strict-loader tests passed, including all original trailing/no-echo
controls and added decoded Unicode aliases, quote aliases inside arrays and an
overwritten nested subtree. Positive punctuation, semicolon and Unicode-key
strings remain accepted; the positive fixture asserts20members/9objects/3arrays.
The runner consumer subset and every manifest/result control passed. No check
was skipped in these focused suites. The first consumer selector selected three
tests rather than the intended broader prefix set; the separately corrected
prefix command ran all55manifest/result cases. No unexecuted case is claimed.

Evidence is retained under ignored
`build/phase05-plan23-runtime-repair/json-census/`: exact original function,
before/after CPU and wall timings, nine-document independent expectations,
12-test JSON JUnit, three-test consumer JUnit and55-test manifest/result JUnit.
The earlier41931a2 partial CI/ARM/Thumb evidence remains under
`build/phase05-plan23-proof-r3/` and is not relabelled.

## Independent review and remaining work

[Independent review](05-23-CENSUS-REVIEW.md) found no active defects. The CMake
3.25.3 tagged implementation supports root GET; this is source-level floor
compatibility evidence, not direct execution of a local3.25 binary. Current
native executable tests used4.4.3; fresh Linux evidence must record its version.

Parent owns a new immutable freeze, complete Darwin/native Linux proof and
literal comparisons, affected canonical/sanitizer/hygiene paths, the readiness
decision and at most one fresh hosted attempt. Any new hosted red stops work
again. No hosted result, fixture readmission, Plan23/Phase5 completion, hardware
qualification or Phase6 is claimed. No user input is needed for these gates.
