# Plan23 review candidate

Status: implementation candidate for parent review, not plan completion. Root HEAD
remains `426af7e`; no root task commit, source freeze, pin change, publication,
shared-state update or Plan24 switch occurred. Owner changes and unrelated notes
remain untouched. No owner action is needed; the parent reviews and acts next.

## Changes to review

- `cmake/CheckArmCoverage.cmake` and `diagnostics/armv4t/cases.json` distinguish
  actual semantic positive/broken pairs, one exact named stop witness, and
  API-only native selectors/assertions. All 386 IDs and their order, every guest
  program, instruction expectation and byte-affecting flags remain unchanged.
  The ledger's verified state describes its observed boundary; it does not
  replace fresh producer proof or admission.
- Non-inventory row 1833963930 now truthfully describes static BX refusal versus
  undefined-instruction classification. Its former valid-but-unimplemented
  wording is not claimed verified. Existing `decode` assertions establish the
  actual boundary; no unimplemented legal opcode or guest fixture was invented.
- `cmake/BuildArmDiagnostics.cmake` changes only two state guards so reviewed
  semantic/stop producers remain selectable after their ledger state becomes
  source-verified/verified. CPU/API-only rows still cannot produce a guest ROM.
- `cmake/GenerateFixtureIndex.cmake` adds explicit `GBL_PROOF_METADATA=ON`.
  Its historical production projection has private immutable qualification `0`;
  the separate artificial control index retains qualification `1`. The shared
  lookup in `runner/run.c` refuses qualification `0` as existing `fixture-unknown`
  before guest execution, including matching historical bytes. Default/admitted
  generation retains strict real ancestry, producer bytes/modes and containment.
  No public ABI, profile or reason vocabulary changes.
- `tests/diagnostics.cmake` registers every selected real output and native
  boundary at top-directory scope. One actual grouped setup produces fresh
  runner JSON/TAP; individual tests and the mandatory exact-set guard bind
  configure token, source input identity, source pin, runner, result and loaded
  bytes, exact nonzero case ID, state and named stop. API rows bind source-derived
  assertion-row hashes and actual complete native TAP. Production execution
  continues using the canonical parsed authority/result checker.
- Proof observations now use verified copies under each build's own
  `generated/diagnostics/proof` root. Existing producer roots remain separately
  identified; case receipts do not pretend that copied or historical data ran.
- `tests/gates/diagnostic_admission.cmake` supports the existing all/ARM/Thumb
  grouping and source preparation ahead of index generation. The admitted
  CMake target prepares its complete selected group, replacing smoke-only wiring.
- Literal inventories were generated from actual registration and then checked
  by the existing configure/JUnit gates. `diagnostics-arm.txt` is necessary
  because ARM and all now have different real case sets. Four hosted jobs and
  existing presets/grouping remain intact. Native ci/asan inventories stay
  literal when `GBL_REQUIRE_DIAGNOSTICS` is enabled; commands enforce required
  execution and never emit optional-absence skips in that mode.
- Dependency, pipeline and receipt controls cover genuine optional absence,
  required/wrong/unusable tools, actual compile/link/header/map/scan failures,
  guest/checker errors, stale/missing receipts/output, exact IDs/stops, required
  all-skipped/proof-mode refusal and actual declared-BIOS `ran=false`.

New helpers are `cmake/DiagnosticMatrix.cmake`, `tests/diagnostics.cmake`,
`tests/gates/diagnostic_{setup,receipt,receipt_controls,dependency,pipeline}.cmake`
and `tests/inventory/diagnostics-arm.txt`. Related existing consumer/control
changes are in `CMakeLists.txt`, `cmake/GblTests.cmake`,
`tests/gates/{coverage_case,diagnostics_case,diagnostic_admission,runner_metadata}.cmake`,
the other four lane inventories, and `docs/verify.md`.

## Authority and required next sequence

The canonical manifest still has 722 historical output pins for source
`c3af6debd2f162c8eb91dc96906cf6f6a5ed5d16`, whose producer digest is
`d9c844e91e92f36f8d07ca2cda14841954115674d39b7fa8efce8596bc7c0341`.
Those rows and prior hosted run 43478622357 remain historical evidence.

Only three listed producer inputs changed: the builder's two guards, coverage
checker and ledger. Current candidate producer digest is
`255c3be8ffeb947cd0c2162ef8ff7b29da020892cdc4c2e3a124a51912358734`.
The ignored disposable clone at `build/phase05-plan23-candidate` uses explicitly
nonfinal local test authority `273222ba9042be5214cb8e87bee03755094c57f2`.
Later consumer-only synchronization left that authority and all producer input
bytes unchanged. It is not the parent's final source freeze or hosted proof.

Parent next: independently review/fix this candidate, freeze one reviewed
source-only revision, obtain fresh complete local literal parity and all four
real hosted group witnesses, then re-admit exact output digests/source pins.
Unchanged ROM bytes do not waive this sequence. Any new hosted red retains the
standing stop; no retry, cap increase or publication was attempted here.

Before publishing the source-proof head, native ci/asan jobs need an explicit
unqualified proof-metadata configuration while their old pins are historical.
Fresh admitted defaults intentionally refuse these changed producer inputs.
After readmission, Plan24 must restore required admitted execution and arrange
sufficient real checkout history to prove the pinned source ancestor in every
admitted metadata lane. Verify that history locally before publication; do not
weaken ancestry or discover the missing-history dependency via a hosted red.

## Verification evidence

All receipts below remain ignored under `build/`.

- Registration RED used actual CTest discovery, not a fabricated count: the
  reviewed ARM case was absent from the original seven registrations. The real
  Node TAP report and supported classifier receipt returned `RED_EVIDENCE_OK`.
  Current registration passes the same assertion. Files:
  `phase05-plan23-registration{,-current}.json`, `*-registration-red.json`,
  `*-registration-verdict.json`, `*-registration-green.tap`.
- Proof-isolation RED actually returned PASS for matching historical source-built
  bytes. Its supported TAP classifier returned `RED_EVIDENCE_OK`; the fixed
  runner returns REFUSED/fixture-unknown, ran=false and zero progress. Saved in
  `phase05-plan23-proof-{red,verdict}.json`, `*-proof-final.tap` and the real
  producer JSON. An earlier missing historical path was setup failure, not RED.
- The original admitted-pair baseline passed in 24.92 seconds. The nonfinal source
  tracer built and observed the real pair in 13.51 seconds, and each positive/broken
  receipt check took 0.01 seconds. Raw observations were explicitly refused admission
  by canonical CheckResult; no raw PASS was relabelled as production PASS.
  Logs: `phase05-plan23-{pair,tracer,receipt-positive,receipt-broken}.log`.
- Candidate ARM source build produced 478 outputs/241 guest IDs in 32.28 seconds.
  Its first complete 549-test lane and post inventory passed in 83.72 seconds. The
  stronger 550-test run, adding per-build copies and coordinated receipt/native
  integrity controls, also passed and reconciled JUnit, in 191.26 seconds locally.
  Logs: `phase05-plan23-arm-{build,tests}.log`,
  `phase05-plan23-arm-current-{tests,post}.log`. These local timings are not
  hosted-budget qualification.
- Candidate Thumb source/host build produced 244 outputs/126 IDs in 40.34 seconds;
  its complete 292-test lane and post inventory passed in 84.01 seconds, with no
  required skip. Logs: `phase05-plan23-thumb-{build,tests,post}.log`.
  ARM+Thumb cover the actual 722 outputs; this is local source/observation
  evidence, not complete cross-host proof or new admission.
- Native canonical workflow passed in explicitly configured proof metadata mode
  with an absent optional tool root: 1106 tests, no failures, 725 expected skips
  (724 diagnostic entries plus the inherited owner-local check), and exact
  JUnit reconciliation. Main test time 177.61 seconds. The setup commands were
  `cmake --preset ci -DGBL_DIAGNOSTIC_STAGE=proof
  -DGBL_TOOLCHAIN_ROOT=build/phase05-plan23-absent`, then
  `cmake --workflow --preset ci`. Log: `phase05-plan23-ci.log`.
- The final quiet optional diagnostic subset passed all 744 registered tests
  in 6.71 seconds, including every specific guest skip, the API-only assertions
  and dependency controls. This exercises the optimized absence path with no
  competing Gamebloke probe. Log/JUnit: `phase05-plan23-optional-current.log`,
  `build/ci/phase05-plan23-optional-current-junit.xml`.
- Sanitized native runner contract, relocation/negative relocation and dependency
  controls passed. Metadata's first focused run lacked the hash-test executable;
  building that prerequisite and rerunning metadata passed in 27.19 seconds. There was
  no sanitizer finding. The moved installed proof runner is byte-identical to
  its built executable and refuses matching historical bytes at zero progress.
  Logs: `phase05-plan23-asan-tests.log`, `*-asan-metadata-retest.log`,
  `phase05-plan23-installed-proof.json`. This used native AppleClang sanitizer
  flags, not an ARM-toolchain install in the asan metadata lane.
- Required native configuration succeeds with the real pinned tool and the same
  literal inventory. A missing required install fails configure specifically
  as `required-diagnostics-toolchain-invalid`/`install-missing`, without SKIP.
  Logs: `phase05-plan23-required-config-{positive,negative}.log`.
- Default generator rejects current stale producer inputs; its log is
  `phase05-plan23-default-stale.log`. Source audit confirms 386 stable IDs/order,
  unchanged guest programs and unchanged canonical pins in
  `phase05-plan23-source-audit.json`.
- Final focused current controls passed: actual instrumented clean source builds
  plus compile/link/header/map/scan refusals 2.16 seconds; coordinated receipt/source/ID/
  mailbox/stop/native/skip/stage controls 3.05 seconds; final coverage clean plus negative
  boundaries 21.83 seconds. Logs: `phase05-plan23-{pipeline,receipts,coverage}-final.log`.
  Dependency controls passed twice after making their generated setup idempotent,
  and both PASS/GUEST_FAIL false-completion variants are checked. Logs:
  `phase05-plan23-dependency-{final,repeat}.log`.

## Measured duplicate work and remaining limits

No new cache, dependency or concurrency was added. Canonical global authority
parsing remains grouped, with actual per-case evidence. The pipeline controls
reuse only the real aggregate builder's current source-identity parent proof;
they never manufacture that receipt or retain their temporary smoke child as
qualified production output.

The final coverage control already runs the full clean final checker before its
negative input. Its separate standalone clean registration repeated 8–16 seconds of
measured work, so it was folded into one inventoried final gate. Optional absence
retains every named current-token skip while avoiding repeated guest source/
binary hashes when no guest ran; all actual execution retains those checks.
Local timings varied substantially even for unchanged native/checker work; an
attributable process check found no leftover Gamebloke build/test process.
Hosted limits remain 180 seconds and need fresh actual evidence after review/freeze.

The canonical all-group workflow and positive required admitted execution of the
new producer authority remain final integration work after the source-proof/
readmission gate. They are not claimed green from split raw observations.
No SUMMARY.md, shared-state completion, new qualification or release is claimed.
There are no implementation stubs or new public network/auth/schema surfaces.

## Parent review follow-up

Independent review reported one diagnostic skip-classification warning. The
parent reproduced it and added a whole-output absence marker, disabled skip
matching in required mode, and actual failing-marker/JUnit controls. The full
optional subset passes with only its intended skips; required discovery has no
diagnostic skip matching. Native `ci`/`asan` presets now explicitly select the
unqualified proof stage for the upcoming source-only publication, rather than
depending on cached configuration. These consumer-only changes need narrow
re-review; see [the fix evidence](05-23-REVIEW-FIX.md). All producer inputs and
the pending final source-freeze/proof/readmission sequence remain unchanged.
