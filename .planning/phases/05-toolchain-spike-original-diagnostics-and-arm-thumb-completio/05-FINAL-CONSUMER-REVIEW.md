# Independent final consumer review

**Scope:** The nine-source-file consumer repair at `HEAD 6211cb5` plus the current uncommitted candidate. Source hashes match `build/phase05-final-consumer-repair/candidate-hashes.json`.

**Result:** No open findings. The warning from the first pass is closed by the focused guard correction and its graph mutation controls. The current smoke target wiring is correct in the generated Ninja graph. Hosted timing and qualification limits remain unchanged.

## Findings

### WARNING (CLOSED) — Smoke prerequisite guard can accept a full-corpus dependency

**Status:** Closed in the follow-up change to `tests/toolchain.cmake`.

**Original location:** `tests/toolchain.cmake:379-384` in the first reviewed candidate. The corrected graph checks are now at `tests/toolchain.cmake:344-503`.

**Issue:** `check_smoke_dependency` accepts the prerequisite when `ninja -t commands gbl_diagnostic_smoke` contains `-DMODE=smoke` and `admission-smoke`, and lacks the literal strings `all-admitted` and `phase05-matrix`. Ninja command output lists the command that invokes `diagnostics_case.cmake`; the script's downstream preparation is opaque to that text scan. In the current generated build, `ninja -t commands gbl_diagnostics` includes the full preparation command `-DCASE=prepare-admitted-indexes -DGROUP=all`, without either forbidden string. Combining that corpus-target command with the valid smoke command satisfies all three current string predicates. Adding `gbl_diagnostics` as a dependency of `gbl_diagnostic_smoke` could therefore pass this guard while pulling full admitted preparation into the supposed minimal prerequisite. This leaves the exact regression the guard is meant to prevent undetected and can restore the hosted job's long pre-check work.

**Read-only reproduction:** Queried the current generated targets with `ninja -t commands`. The smoke target contains both expected substrings; the full `gbl_diagnostics` target contains `-DCASE=prepare-admitted-indexes -DGROUP=all` but no `all-admitted` or `phase05-matrix`. A read-only predicate check over both command sets returned `has_smoke_mode=true`, `has_admission_smoke=true`, `has_forbidden_all_admitted_or_matrix=false`, and `corpus_target_contains_preparation=true`.

**Requested fix at first review:** Inspect the actual Ninja dependency graph for `gbl_diagnostic_smoke` and require its dependencies to resolve only to the smoke receipt producer and its source inputs. Add negative graph controls for a corpus target and prepared-all output without building the corpus.

**Closure rationale:** The guard now checks the direct input sets for the smoke target, its phony helper node, and the receipt-producing command against the exact expected nodes and `inputs.txt` leaves (`tests/toolchain.cmake:344-412`). It separately parses `ninja -t commands` and compares the complete command vector, working directory, CMake executable, source/output/tool roots, producer commit, smoke mode, and script (`tests/toolchain.cmake:413-439`). This rejects extra opaque commands as well as hidden target dependencies. The control graph mutates the target with `gbl_diagnostics` and `prepared-all.stamp`, appends an extra command, removes a pinned source edge, and verifies specific refusal reasons; clean and restored graphs must pass (`tests/toolchain.cmake:442-503`).

**Re-review evidence:** All nine current source hashes match `candidate-hashes.json`; the only source hash changed since the first pass is `tests/toolchain.cmake`. A read-only invocation of `cmake -DCASE=smoke-graph` against the cold source and binary paths containing spaces exited 0, independently validating the generated receipt/source graph and parsed producer argv. `guard-review-cold-junit.xml` and `guard-review-warm-junit.xml` show the recurring test passing with clean, corpus-target, prepared-output, extra-command, missing-source, and restored controls. The recorded actual corpus-target build mutation fails with `smoke-graph-inputs-mismatch`, its restored build passes, the legacy-path and missing-registration negatives fail for their expected reasons, and the Thumb companion exits successfully without ARM smoke registration. `guard-review-controls.json` records the spaced paths and corpus absence. The original red-evidence receipt remains as history for the first guard's false acceptance.

No overconstraint was found in the focused pass: the exact current command and graph passed on the spaced cold context, the warm registered test passed, and the Thumb path correctly bypasses the ARM graph check. The controls query copied Ninja files only; they did not execute corpus build commands.

## Review limits

The final Linux ARM cold run precedes the last target-ordering correction; the recorded final-source cold/warm graph controls and current required diagnostics workflow cover that correction separately. Local Linux ARM64 remains above the 180-second hosted job limit and does not establish hosted x64 timing or qualification. This follow-up was limited to read-only graph inspection and the owned review report; no heavy suite or hosted action was run, and no source or shared state was modified.
