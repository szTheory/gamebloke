# Phase 5 local Nyquist readiness review

**Initial audit scope:** Read-only coverage review of the local candidate at
checkout `8e21ef6`, not formal phase verification or completion. At that time,
the candidate had a 13-file record plus three later profile changes. No tests,
source, shared validation map, state, or summary were changed or rerun for that
audit. The independent follow-up below reviews the expanded candidate and also
does not rerun tests.

## Independent follow-up rereview — coverage-fix candidate

This update supersedes the initial status below for the Linux snapshot helper,
the required installed-consumer check, and local candidate binding. I reviewed
the tracked test sources and registrations, recorded CTest/JUnit results, and
recomputed every entry in
`build/phase05-plan25/coverage-fix/candidate-hashes.json`. I did not rerun tests.

| Earlier finding | Current status | Evidence and boundary |
| --- | --- | --- |
| Linux snapshot helper had no durable behavioral test (**BLOCKING**) | **RESOLVED for local recurring coverage.** The tracked `tests/gates/linux_snapshot.cmake` invokes the actual helper against an offline Docker witness and a two-commit synthetic Git repository. It asserts ancestor bytes/chain and commit count; ignored private/build sentinels are excluded; edits, deletions, a space-containing untracked file, executable mode with `core.filemode=false`, and an owned symlink are handled; exact read-only mount, 4 CPU/8 GiB bounds, image/compiler/workflow arguments, signal exits, and cleanup are checked. Five meaningful helper mutations are expected to fail. | Registered as `linux.snapshot` in `tests/gates.cmake`, `tests/inventory/ci.txt`, and `tests/inventory/hygiene.txt`. The current CI and hygiene JUnit records show it ran and passed (2.61s and 2.43s); the recorded fresh Linux run passed it in 0.44s. The history mutant has an actual failing assertion receipt, and the privacy, mode, signal and cleanup mutant logs show their intended reasons for rejection. This establishes recurring synthetic safety coverage; no real private content or Docker/network is used by the focused test. |
| Installed admitted relocation was skippable and absent from required diagnostics (**WARNING**) | **RESOLVED in required ARM/all diagnostics.** `tests/diagnostics.cmake` registers the admitted relocation gate against the actual `diagnostic_execution` fixture without `SKIPPABLE`; Thumb is unchanged. | The recorded all/ARM CTest registration JSON has no skip property; literal diagnostics inventories omit `skip-ok`; the diagnostics JUnit has 794/794 run, zero skips, with `runner.relocation_admitted` passing. The focused ARM evidence also passes with real setup. General CI still has its explicitly optional skip when the toolchain is absent; that is not counted as required admitted coverage. |
| Candidate hash record predates the three profile paths | **RESOLVED for this uncommitted local candidate.** The expanded record binds 25 path hashes and modes, including `profiles/agb.json`, `docs/profile-agb.md`, and `cmake/CheckProfile.cmake`, plus current evidence receipts. | I recomputed all 25 file hashes/modes with no mismatch and verified the recorded JUnit hashes. The current CI JUnit contains all 31 listed profile cases with no missing, failed, or skipped case. The record remains bound to checkout `8e21ef6` plus the uncommitted candidate; a later committed exact-head binding is still pending. |
| Plan24 collector/legacy controls are manual rather than recurring CI coverage | **Still a documented local-only limitation, not a newly observed failure.** | The expanded record retains their latest passing local results but correctly does not claim recurring CI or hosted qualification for those controls. |

**Current local coverage conclusion:** The two targeted recurring-coverage findings
are closed by this candidate, and its expanded local binding is internally
consistent. This rereview does not clear the pending committed consumer identity,
new hosted authority, owner judgments, unresolved probes/prohibitions, or the
formal `05-VALIDATION.md` status. Formal Nyquist compliance and Phase 5
completion remain unasserted.

## Coverage grid

| Plans / work | Observable behavior and actual evidence | Assessment |
| --- | --- | --- |
| 05-01–05-05: pinned toolchain, original diagnostic builds, outputs and runner/result contract | The final diagnostics workflow builds and runs the admitted ARM/Thumb sources. The immutable producer and its 722 output pins remain unchanged; current local checks retain map, architecture, signature, source identity and result controls. | Covered locally. Historical hosted producer proof is preserved; it is not current-consumer hosted proof. |
| 05-06–05-20: ARM/Thumb execution and policy cases | Current admitted run records 355 `PASS`, 355 deliberate `GUEST_FAIL`, 12 named `GUEST_STOP`, and 19 API receipts. The coverage audit crosschecks all 386 ledger rows, CPU-1..9 and all 19 Thumb formats. | Covered locally with semantic outcomes and separately labeled policies/API boundaries. No hardware accuracy claim follows. |
| 05-21–05-22: exact identities, semantic mutants and refusal boundaries | Current receipts bind the actual consumer and producer identities. Native mutation/refusal, unit-boundary, separate BL retirement and actual-result checks run in the required diagnostics lane. | Covered locally; no producer requalification was needed because its inputs are unchanged. |
| 05-23: lane inventory, skip rules and complete execution set | `diagnostics.receipt.controls` exercises zero/missing/wrong IDs, incomplete mailbox, missing receipts, incomplete set, all-skipped refusal and exact stop reason. `diagnostics.dependency.controls` checks optional absence versus required/unusable tools, real BIOS `ran=false`, and producer result refusals. The current diagnostics JUnit has 794 tests, zero failures and zero skips; `diagnostics.execution_set`, `diagnostics.final.outputs`, and the controls are present and passed. | Covered and recurring in the required diagnostics lane. General CI’s 726 specific optional tool skips are reconciled separately and are not counted as required guest passes. |
| 05-24: admitted policy/history, hosted evidence and collectors | The diagnostics preset asserts admitted stage; workflow and CI-needs controls are registered. The actual final receipt guard and CMake emitter run in the diagnostics lane. Full versus shallow history controls passed separately. `tests/ci/admitted_hosted_cases.py` has actual-log parsing plus synthetic Python/CMake positive, refusal, identity, status, progress, API-set, alias and producer-only controls; the latest recorded admitted-driver run passed in 41.03s. The current legacy producer-only source/collector/isolation driver passed in 7.52s. | Behavior is observed. The collector/legacy driver is tracked but local-only and not in CTest inventories, so its mutations are not an automatic recurring CI gate. Preserve this as a manual local check when collector/parser code changes; do not report its build-directory output as CI coverage. |
| 05-25: installed consumer — initial audit finding, superseded above | `runner_relocation.cmake` checks moved-prefix execution from unrelated cwd, built/installed executable identity, actual semantic positive and broken pair, no-BIOS SWI stop, exact JSON/trace and missing-executable refusal. The initial audit correctly found the check was only optional in ordinary CI. The follow-up confirms a required no-skip registration in ARM/all diagnostics with the real setup fixture. | **Resolved in the required diagnostics lane.** Current diagnostics JUnit reports the admitted check run and passed; ordinary CI still explicitly skips when the optional toolchain is absent. |
| 05-25 local Linux helper change — initial audit finding, superseded above | Initial review found only an ignored one-commit helper test. The follow-up confirms this was corrected: `tests/gates/linux_snapshot.cmake` is tracked and registered, uses two real commits, exercises the actual helper, includes signal/cleanup paths and five mutation controls, and passes in current CI, hygiene, and fresh Linux receipts. | **Resolved for local recurring coverage.** The original finding and evidence are retained in the follow-up table above; they describe the earlier candidate only. |

## Initial findings and next actions (preserved from the first audit)

The statuses in this historical section are as of the initial audit and are
superseded by the independent follow-up above where noted.

### BLOCKING — retain the Linux snapshot safety regression as a recurring test

Evidence: [scripts/linux-lane.sh](../../../scripts/linux-lane.sh:51)
performs clone/patch/untracked export and sets cleanup/signal traps. There is no
matching tracked test under `tests/`; the focused evidence is in
`build/phase05-plan25/test_linux_snapshot.py` and its disposable logs. A later
change could reintroduce a checkout-wide mount, copy ignored owner content, drop
a working mode/deletion, or lose history while CI stays green.

Recommended fix: retain an offline synthetic-repository control in the tracked
test suite, invoked by a useful canonical lane. Use at least two source commits
and assert the full ancestor chain, not only `--is-shallow-repository=false`;
retain checks for ignored sentinels, mode changes with `core.filemode=false`,
deletions, unignored new files, mount flags/resource bounds, and nonzero signal
exit with cleanup. Use synthetic content only. Then run this focused control and
the bounded current local checks affected by the wiring before the host request.

### Initial WARNING (now resolved in required diagnostics) — admitted installed relocation was optional

At the initial audit, the direct installed-consumer command and local ASan lane
passed, but the test was `SKIPPABLE` and the required diagnostics inventory had
no relocation entry. The follow-up above records that the candidate now adds
the required ARM/all diagnostics registration without a skip property and that
the current diagnostics run passed it. The ordinary CI tool-absent skip remains
an explicit optional skip, not required admitted coverage.

### WARNING — collector controls are local-only

The Plan24 Python/CMake hosted collector controls are real and passed against the
actual admitted log; they are not an ignored test file. They are also not
registered in a CTest inventory, and the legacy-positive evidence lives in
`build/phase05-plan24/`. The current collector and hosted validator inputs were
not changed by the later Plan25/profile edits, so this is not an observed
behavior failure or a reason to repeat the full matrix. Keep the documented
local-only command as an explicit check when those parser/emitter/legacy paths
change; do not count it as recurring CI coverage.

### Initial evidence-binding finding (resolved locally; commit binding remains pending)

At the initial audit, `build/phase05-plan25/candidate-hashes.json` bound only the
13-file Plan25 candidate and predated the three profile paths. The follow-up
above verifies the expanded 25-file record, including those paths, against
current bytes/modes and JUnit hashes. This closes the local candidate-binding
gap. A committed exact-head binding is still required before hosted authority;
the profile wording change did not alter machine behavior or producer inputs.

## Pending boundaries (not local test failures)

- The final exact-head hosted check remains pending new owner authority and a
  reviewed committed consumer identity. Historical hosted source proof remains
  tied to consumer `56c`; it must not be presented as current-head proof.
- Owner review of evidence-guide clarity and bespoke judgments remains pending.
- The three unclassified Phase 5 probes, three descriptor-less prohibitions,
  eleven historical Phase 4 rows, and Plan19 TDD-history deviation remain
  unresolved. They are not converted into test passes or waivers here.
- `05-VALIDATION.md` remains `draft`, `nyquist_compliant: false`,
  `wave_0_complete: false`. This audit does not make it formally compliant or
  complete Phase 5.

**Initial overall (first audit; superseded by the follow-up):** The
CPU/diagnostic result boundary and Plan23 refusal/skip guards had current
behavioral evidence. At that point the Linux snapshot helper lacked a durable
behavioral test, installed relocation was not mandatory in diagnostics, and
the candidate hash record was stale. The follow-up closes those local coverage
findings. No long reproduction or whole-matrix rerun was needed for this audit.
