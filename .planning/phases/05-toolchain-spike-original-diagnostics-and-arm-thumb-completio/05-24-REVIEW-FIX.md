# Plan24 synthetic alias review fix

The independent review reproduced synthetic collection through an original
source symlink alias outside its marked disposable build scope. Its target was
inside that scope and all documents remained synthetic; no live qualification
bypass was established. The original report remains in05-24-LOCAL-REVIEW.md.

The parent added a concrete regression to the existing admitted control driver.
Before the fix it failed because external-alias-source_root was accepted; the
unaltered failure remains inbuild/phase05-plan24/review-alias-red.log.
The collector now checks every original normalized absolute path and resolved
target for source, output, three API documents and four logs against the same
marked scope. This check runs before opening API documents; document tags are
checked after parsing. The live marked-ancestor refusals remain in force.
Both marker and tagged-document requirements remain unchanged.

All nine external alias controls and existing admitted collector/CMake clean
and refusal controls passed in42.37seconds, using the actual full admitted
workflow log. Evidence:review-alias-green-2.log. The first post-fix harness run
stopped because live marked-context refusal now precedes document-tag refusal;
the expected reason was updated without weakening the refusal. Its log remains
review-alias-green.log. The legacy producer-only source/collector/isolation
controls also passed inreview-legacy-green.log. No full diagnostic matrix was
repeated: emitter, runner, producer and admitted results did not change.

The workflow comments now describe admitted execution and full-history build,
ASan and diagnostic lanes. Workflow policy, cheap source/mode/ancestry preflight,
Python parsing and whitespace checks pass. Exact current fix-file hashes are
recorded inbuild/phase05-plan24/review-fix-file-hashes.json. Independent re-review
is pending; no commit, hosted attempt or plan completion follows from this fix.
