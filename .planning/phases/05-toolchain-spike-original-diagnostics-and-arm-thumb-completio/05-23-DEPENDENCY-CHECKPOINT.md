# Plan23 internal source-authority checkpoint

Plan23 is incomplete. Consumer registration and dependency controls need a reviewed
source-proof staging adjustment before the final coverage gate can pass honestly.
The parent owns that technical review and any hosted publication. No owner
acknowledgment is requested. No producer input, manifest pin, shared planning state
or hosted workflow was changed during this investigation.

## Observed dependency

- `diagnostics/armv4t/cases.json` has stage `draft`: 354 semantic rows,
  12 guest-stop rows, 19 CPU/API-only rows and one registered smoke-pair row.
  Coverage states are 385 pending and one source-verified.
- `cmake/CheckArmCoverage.cmake` final mode first requires stage `final`, then
  requires every row to be verified. Its common verified-state branch requires
  two distinct fixture names and a same-case failure ID. Its final manifest
  branch requires both fixture names to exist. Those contracts incorrectly
  require ROM pairs for single named stops and CPU/API-only boundaries.
- `diagnostics/armv4t/inputs.txt` explicitly includes both the ledger and checker.
  Even witness-only changes require a fresh complete proof and readmission.
- `cmake/GenerateFixtureIndex.cmake` always checks canonical diagnostic source
  authority before projecting metadata. Changing either producer input while
  retaining the current canonical pins therefore also blocks metadata-only
  ci/asan builds. Production freshness must retain that fail-closed behavior.

Current immutable source authority is
`c3af6debd2f162c8eb91dc96906cf6f6a5ed5d16`, producer-input digest
`d9c844e91e92f36f8d07ca2cda14841954115674d39b7fa8efce8596bc7c0341`.
The existing 722 admitted output rows and their prior proof cannot silently be
relabelled as proof of a changed producer revision.

## Recommended technical sequence

1. Review boundary-specific final coverage contracts: semantic rows require a
   real positive/broken pair and exact nonzero shared ID; named stops require
   their one real admitted witness and exact stop reason; CPU/API-only rows
   require an explicit mapping to actual registered native selectors and
   evidence, without fabricated guest fixtures or CLI PASS claims. Retain the
   existing draft producer path and architecture/source validation.
2. Implement the checker/ledger update with meaningful broken/clean controls.
   A verified ledger entry describes its actual evidence boundary; it does not
   substitute for fresh output proof or admission. Commit all changed producer
   inputs as a new source-only authority before producing proof.
3. If configuring a native runner during pre-admission needs canonical metadata,
   use an explicit, narrowly scoped pre-admission metadata projection of the
   historical immutable manifest authority. Default and admitted generation,
   execution, output validation and source freshness must continue refusing stale
   producer inputs. Do not globally disable freshness or turn historical rows
   into newly qualified fixtures. Controls must prove the exception cannot
   satisfy required admitted execution.
4. Build the entire unchanged-or-changed output set anew under that source-only
   authority; obtain fresh complete local literal byte comparisons and the four
   real hosted group witnesses within the existing 180-second limits. Any hosted
   red preserves the standing stop. Parent handles publication and witnesses.
5. Re-admit every exact output digest with the new source authority, then enable
   final consumer coverage, actual execution receipt sets and full registration.
   Preserve the explicit proof stage through Plan23; Plan24 owns the required
   admitted-stage switch. Compare bytes honestly even if they remain identical.

An alternative is to retain the frozen draft producer and implement a separate
consumer coverage report tied to actual admitted receipts. That avoids a fresh
producer proof but does not satisfy the planned canonical final-ledger gate;
it needs an explicit plan adjustment. The boundary-specific checker and fresh
proof route above is recommended.

## Measured consumer tracer before bulk expansion

- `cmake --preset diagnostics` passed in 0.18 seconds. Actual
  `ctest --test-dir build/diagnostics --show-only=json-v1` took 0.03 seconds and
  contains seven existing tests, with no per-case registration.
- A real Node TAP assertion against that CTest output fails specifically because
  reviewed case `arm-343721884` has no registered execution witness. The supported
  GSD classifier returned `RED_EVIDENCE_OK`; this is a registration assertion,
  not a setup failure or a fabricated guest result.
- `cmake -DCASE=admitted-pair -DSRC=. -DBIN=build/ci
  -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/diagnostics_case.cmake` passed
  in 24.92 seconds with two actual production results: one PASS and one
  GUEST_FAIL, both checked by the canonical result contract and exact
  semantic/identity/progress assertions. This is local admitted execution,
  not new cross-host proof.

The existing admitted helper already parses global authorities once and invokes
the canonical `gbl_result_check` for each result. It also regenerates the index
and builds the runner once per invocation. Repeating this full helper 722 times
would duplicate that measured cost. Register an execution setup for the full
reviewed group, retain its real per-output JSON, then give every inventoried
case its own receipt/identity check and a mandatory exact-set guard. Measure the
small grouped tracer before expansion; do not reparse the complete manifest
once per case, weaken canonical checks, or accept stale setup receipts.

Receipts remain ignored under `build/`: `phase05-plan23-configure.log`,
`phase05-plan23-registration.json`, `phase05-plan23-registration-time.txt`,
`phase05-plan23-registration.tap`, `phase05-plan23-registration-red.json`,
`phase05-plan23-registration-verdict.json`, and `phase05-plan23-pair.log`.
The diagnostic preset was configured and the existing ci admitted-pair helper
regenerated build-only metadata; no full canonical workflow was claimed.

## Handoff

Parent reviews the staging choice and owns the next action. Plan23 has zero
completed tasks and no task commit. No production or final coverage gate is
reported green. Preserve the source authority until that review resolves the
dependency; no user action is needed.

## Parent technical resolution after restart

The repository, existing receipts and local tools survived the restart. A fresh
native `cpu_test unit_boundaries` run passed all63 assertions. No interrupted
Gamebloke process needed cleanup. The parent accepts the recommended
boundary-specific checker and fresh source-proof/readmission route as routine
implementation within the authorized Phase5 scope; this is not a new owner
decision or a waiver of evidence requirements.

- Semantic rows retain actual positive/broken fixture pairs. Stops bind their
  one actual witness and reason. API-only rows bind actual registered native
  selectors/assertions and retain their lower boundary. No fabricated pair or
  guest success is permitted. A verified coverage row describes its existing
  observed evidence; fresh proof of the revised producer remains a separate gate.
- Any explicit pre-admission metadata projection must be unqualified at runtime.
  Prefer a private immutable index marker consumed by the shared production
  lookup path and an existing refusal reason, preserving identical runner
  source/options between production and controls. Historical metadata projection
  must not enable even a hash-matching registered fixture to execute or satisfy
  mandatory admitted evidence. Default/admitted generation retains freshness.
- Implement and review the producer/checker/ledger and consumer staging changes
  together before freezing a new source-only authority. Keep original source
  programs, IDs and ARMv4T audits unchanged unless a concrete defect requires it.
  Measure grouped execution/receipt registration before expanding it.
- Parent owns independent review, the new frozen authority, complete local
  parity, real hosted witnesses and subsequent exact output re-admission. No
  new pins, hosted attempt or final admitted claim precedes those gates. The
  standing stop on any new hosted red remains in force.

The executor proceeds to a review-ready source/staging candidate and returns an
internal checkpoint before freezing/publishing proof. No owner action is needed.
