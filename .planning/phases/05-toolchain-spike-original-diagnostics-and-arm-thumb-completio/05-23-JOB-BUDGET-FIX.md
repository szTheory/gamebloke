# Collector isolation review fix

Independent review WR-01 found the local collector used `any` across three
synthetic input tags and did not require a marked disposable source/output tree.
The final CMake validator already refused mixed tagged evidence, so the gap was
an early consistency defect, not an observed live-proof bypass.

The parent reproduced the old collector accepting one tagged input plus two
untagged inputs under explicit synthetic mode. Its output remained explicitly
synthetic. The corrected collector requires boolean tags on every input in
synthetic mode, marked source and output scopes under a disposable build tree,
and all input/log paths contained in that scope. Live mode refuses true tags
and marked synthetic source/output contexts. These checks occur before the
full source preflight or large output/log parsing. No budget, producer byte,
receipt identity or qualification rule changed.

The actual mixed-tag reproduction now refuses before collection. The full
source/collector controls passed again in8.76seconds wall/7.78CPU, including
all seven partial-tag combinations, three wrong tag types, unmarked source/
output and outside-log refusals. Marked clean and real consumer-descendant
controls still pass as synthetic only; every previous per-job budget, source,
status and log-binding refusal remains. Evidence is retained under ignored
`build/phase05-job-budgets/review-fix/`; the old accepted reproduction is labelled
synthetic and is not supplied as hosted proof. Final independent re-review is
pending. The approved hosted attempt remains unused.

## Marked-ancestor closure and strict policy consistency

Re-review identified the broader reserved-context defect: a marked parent was
not checked for every live source/evidence path. Both Python collection and the
CMake live final gate now refuse markers across source, output and every input
ancestor. Both resolved and original path contexts are checked where relevant;
CMake argument indexing preserves a path containing a semicolon. Explicit
synthetic mode remains isolated and all-tagged; no universal authentication of
arbitrarily forged unmarked documents is claimed.

Current source/collector controls passed in9.33seconds, including CMake source,
output and each of four evidence-ancestor refusals, Python source/output and all
seven input/log ancestors, a semicolon path, clean unmarked-context refusal of
incomplete data, marked synthetic positives and real consumer-descendant
acceptance. Evidence is`review-fix/current-context-controls.*`. The initial new
negative fixture used the frozen older validator file; its byte-identity guard
now copies/checks the actual current consumer files before execution. That setup
failure is retained, not presented as a product RED. A separate initial test
expectation was corrected to the real missing-head refusal for empty documents.

The parent also reproduced a concrete policy-reader disagreement: CMake's direct
JSON reader accepted a comment that Python rejected. The shared budget loader
now uses the existing strict JSON loader; comment, trailing-comma and duplicate
policy keys are refused by cheap controls. No producer bytes/modes, job budgets
or r4 receipts changed. Final focused re-review remains the next boundary.

## Original-path alias regression

Focused re-review correctly found that canonicalizing SRC before the live guard
lost its original marked ancestor. The guard now receives original SRC/BIN as
well as canonical source/output paths. Current controls pass CMake and Python
source/output/input symlinks from a marked scope to unmarked targets, alongside
all ancestor, type, budget and source-binding cases. The full focused source/
collector check passed in10.70seconds wall/8.84CPU; evidence is ignored
`review-fix/alias-context-controls.*`. This was a narrow path-context correction,
not a producer change or a new hosted attempt. Final re-review is pending.

Final independent re-review is clean: both findings are resolved with their
history preserved. The collector and CMake paths check original and canonical
source/output contexts and all evidence ancestors. Current alias controls pass.
No hosted attempt has been used under the targeted budget renewal.
