# Remove redundant work after a certain JSON refusal

The reviewed `d128c40` preflight passed all five Darwin lanes functionally and
its722 fresh Darwin ROMs literally matched the preserved c3 baseline. Its clean
shallow native CI took181.34 seconds before hosted setup, so no hosted attempt
was made. Linux proof had not started. Exact historical evidence remains in
`build/phase05-plan23-proof/INTERNAL-CHECKPOINT.md`; it is not complete cross-host
proof and will not be relabelled under a new authority.

The measured trailing-content control took27.68 seconds. The strict loader had
already recorded a structural refusal but still recursively counted the full
parsed object members. Each of the seven tails repeated that unnecessary work
over the expanded manifest. The narrow change skips the census only when the
input is already guaranteed to hit the existing fatal refusal. Every otherwise
acceptable object still takes the exact original census and duplicate-key
check. No formerly rejected input can become accepted through this condition.

The new instrumented control first demonstrated the old redundant call. Its
clean input proves that the census probe is active; its trailing input must
produce the existing trailing-content refusal without entering that census.
Duplicate-only inputs still fail through the real duplicate-key check. All63
real trailing variants and their no-echo assertions remain mandatory.

Current focused results after the code change:

| Check | Result | Wall seconds |
| --- | --- | --- |
| Trailing variants and census controls | All pass |3.88 |
| Duplicate-key controls | Pass |0.03 |
| Tricky clean object | Pass |0.01 |
| Real committed documents | Pass |7.34 |
| Complete registered JSON control subset |12 pass, no skips |10.73 |

Exact logs/timings/JUnit are under `build/phase05-plan23-json-refusal/`. These
isolated measurements are not a promised full-workflow or hosted speedup;
current clean-checkout timings must be measured again.

`cmake/GblJson.cmake` is a producer input. Independent review and a new source-only
freeze are therefore required, followed by fresh complete local/hosted output
proof and exact readmission. Original guest programs, IDs, tool pins and flags
are unchanged. Historical canonical pins and all prior receipts remain intact.
No timeout, job count, dependency, concurrency or rights change occurred.
