# Admission control runtime correction

The expanded production manifest made strict-loader regression controls the
largest measured local CI cost. The first full admission workflow passed in
253.63 seconds; its trailing-content test took100.98 seconds. Those observations
remain historical and are not hosted failures.

The consumer-only correction retains every real document and all seven tails.
Failure reason and no-content-echo assertions now inspect one actual child
execution instead of running the same command twice. Only the independent,
read-only trailing-content variants use a nested CTest pool, bounded to three
workers. Other project tests remain serial. The nested run must succeed, execute
the exact expected count and skip nothing. No producer input, fixture pin,
workflow job, dependency, sanitizer flag or180-second hosted limit changed.

## Measured evidence

Measurements ran serially without competing project probes. Wall/user/system
times are seconds and describe verification cost, not emulator throughput.

| Command scope | Result | Wall / user / system |
| --- | --- | --- |
| Original `json_case.cmake`, `CASE=trailing_content`, saved before editing | Pass |64.54 /62.61 /1.14 |
| Candidate `CASE=no_echo_guard` | Leak, successful-child and wrong-reason meta-controls detected; clean paired assertion executes once |0.06 /0.03 /0.02 |
| Candidate `CASE=trailing_content` | All63 actual variants pass; none skipped; three workers |22.01 /50.68 /1.27 |
| `cmake --workflow --preset ci` | Full checks and post inventory pass |166.72 /150.25 /26.04 |

Exact local logs and timing records are under
`build/phase05-admission-performance/`: `baseline-trailing.*`,
`candidate-no_echo_guard.*`, `candidate-trailing_content.*`, and
`candidate-ci.*`. The nested JUnit and per-variant output are under each run's
`nc/json_trailing_content/variants/` directory. The canonical inventory remains
355 expected,354 executed and one explicitly permitted owner-local skip. All
admitted fixtures themselves ran in the separate complete admission proof.

The initial copied-script baseline used a relative source argument and refused
the real-document inventory before loading it. It was a setup error, corrected
with an absolute source root before the recorded measurement; it is not RED
evidence or a performance sample. The existing baseline's100.98-second test and
the fresh64.54-second sample differ, so the isolated before/after measurements
above are the useful comparison. Full253.63→166.72 workflow observations include
host variation and are not attributed wholly to this change.

The new paired assertion has controls for leaked content, an unexpected zero
exit, a wrong failure reason and exact single execution. Every original
trailing-content base/tail still invokes the real unchanged strict loader.
A fresh Git comparison with forced file-mode checking confirms every producer
input remains identical to the verified source-only authority. Its existing
local literal byte comparison and hosted hash witness remain distinct evidence.

## Remaining work

Independent review of these changes and the admission containment fixes is clean;
see [the review](05-21-ADMISSION-REVIEW.md). The local result is not a new hosted
timing claim. Cold compiler,
sanitizer and required hosted checks remain owned by the later Phase5 integration
plans before publication/acceptance. No hosted attempt was made here.
