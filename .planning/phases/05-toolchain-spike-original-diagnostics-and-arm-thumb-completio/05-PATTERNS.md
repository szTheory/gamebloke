# Phase 5: Toolchain Spike, Original Diagnostics, and ARM/Thumb Completion - Pattern Map

**Mapped:** 2026-11-11
**Files analyzed:** 38 existing integration points and proposed files
**Analogs found:** 32 / 38 (six roles have no close source analog)

## File Classification

The phase docs do not fix new source filenames. Proposed paths keep source inputs reviewable and outputs under the ignored build tree. “Proposed” means planning suggestion, not a current file.

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| src/cpu.c | service/core interpreter | transform | src/cpu.c | exact seam; expand existing unit |
| tests/cpu_test.c | test | transform | tests/cpu_test.c | exact |
| tests/state_check.h | utility/test helper | transform | tests/state_check.h | exact |
| tests/gates/core_mutation.cmake | test/control | transform | tests/gates/core_mutation.cmake | exact |
| toolchain/arm-none-eabi.lock (proposed) | config | file-I/O/batch | pins.txt, cmake/CheckManifest.cmake | role-match; separate from fixture pins |
| scripts/fetch-toolchain.sh (proposed) | utility | file-I/O | scripts/fetch-pin.sh | role-match for verified staging; no extractor analog |
| tests/gates/toolchain_case.cmake (proposed) | test/control | batch | tests/gates/fetch_case.cmake | role-match |
| diagnostics/armv4t/startup.S (proposed) | test fixture | transform | none | no analog |
| diagnostics/armv4t/cases-arm.S (proposed) | test fixture | transform | none | no analog |
| diagnostics/armv4t/cases-thumb.S (proposed) | test fixture | transform | none | no analog |
| diagnostics/armv4t/linker.ld (proposed) | config | batch | none | no analog |
| cmake/FixGbaHeader.cmake (proposed) | utility | transform/file-I/O | none | no header-fixer analog |
| cmake/BuildArmDiagnostics.cmake (proposed) | config/build script | batch | CMakeLists.txt | role-match |
| CMakeLists.txt | config/build graph | batch | CMakeLists.txt | exact |
| tests/fixtures/manifest.json | config/fixture authority | transform | contracts/examples/manifest.json | role-match |
| cmake/CheckManifest.cmake | validator/utility | transform | cmake/CheckManifest.cmake | exact |
| cmake/GenerateFixtureIndex.cmake | producer/utility | transform | cmake/GenerateFixtureIndex.cmake | exact |
| cmake/CheckResult.cmake | validator/utility | transform | cmake/CheckResult.cmake | exact |
| runner/run.c | controller/producer | request-response | runner/run.c | exact; immutable index and pre-execution digest gate |
| tests/gates/manifest_case.cmake | test/control | transform | tests/gates/manifest_case.cmake | exact |
| tests/gates/result_case.cmake | test/control | transform | tests/gates/result_case.cmake | exact |
| tests/gates/runner_metadata.cmake | test/integration | request-response | tests/gates/runner_metadata.cmake | exact |
| tests/gates/diagnostics_case.cmake (proposed) | test/integration | request-response | tests/gates/runner_case.cmake | role-match |
| tests/gates/diagnostic_linkmap.cmake (proposed) | test/control | batch | none | no map-audit analog |
| scripts/signature-scan.sh | utility/gate | file-I/O/batch | scripts/signature-scan.sh | exact |
| tests/gates/signature_case.cmake | test/control | file-I/O | tests/gates/signature_case.cmake | exact |
| tests/gates.cmake | config/test registration | batch | tests/gates.cmake | exact |
| tests/core.cmake | config/test registration | batch | tests/core.cmake | exact |
| cmake/GblTests.cmake | test registration/helper | request-response | cmake/GblTests.cmake | exact |
| tests/inventory/ci.txt | config/test inventory | batch | tests/inventory/ci.txt | exact |
| tests/inventory/asan.txt | config/test inventory | batch | tests/inventory/asan.txt | role-match; keep safe if diagnostics do not run in ASan |
| .github/workflows/ci.yml | config/workflow | batch | .github/workflows/ci.yml | exact |
| cmake/CiRequired.cmake (conditional) | config/gate | request-response | cmake/CiRequired.cmake | role-match if a separate required toolchain job is chosen |
| profiles/agb.json | config/profile | transform | profiles/agb.json | exact |
| docs/profile-agb.md | documentation | transform | docs/profile-agb.md | exact |
| cmake/CheckProfile.cmake | validator | transform | cmake/CheckProfile.cmake | exact |
| tests/gates/profile_case.cmake | test/control | transform | tests/gates/profile_case.cmake | exact |
| tests/gates/runner_relocation.cmake | test/integration | file-I/O/request-response | tests/gates/runner_relocation.cmake | exact regression boundary |

Every existing analog named here was confirmed by git ls-files; none is an ignored build/runtime mirror.

## Pattern Assignments

### CPU unit expansion: src/cpu.c, tests/cpu_test.c, tests/state_check.h, tests/gates/core_mutation.cmake

**Analog:** The current src/cpu.c unit is the right implementation seam, and existing CPU tests exercise its bounded preflight contract. Extend that one ephemeral unit and commit path; do not create another interpreter or effectful dry-run.

Current unit shape and hard limits (src/cpu.c, lines 22–65 and 1173–1226):

~~~c
#define GBL_CPU_TRACE_UNIT_RECORD_MAX 3u

/* One ephemeral decode is used both to preflight this unit and to execute it. */
typedef struct gbl_cpu_unit {
    gbl_cpu_unit_kind kind;
    uint32_t instruction_address;
    uint32_t opcode;
    uint32_t branch_target;
    /* ... */
    uint8_t fetch_count;
    uint8_t fetch_width;
    uint8_t internal_cycles;
    /* ... */
    uint32_t fetch_address[2];
    gbl_bus_access_class fetch_class[2];
    uint64_t cycle_cost;
} gbl_cpu_unit;
~~~

~~~c
const uint64_t expected_cost = (uint64_t)unit->fetch_count +
                               (uint64_t)unit->internal_cycles +
                               (has_data_access ? UINT64_C(1) : UINT64_C(0));

if (unit->cycle_cost > remaining_budget ||
    unit->cycle_cost > UINT64_MAX - machine->master_cycles ||
    unit->cycle_cost != expected_cost || unit->fetch_count > 2u ||
    /* ...validate exception, fetch, and data spans... */) {
    /* Refuse before execution. */
}
~~~

The run loop decodes once, validates cycle and trace headroom, then performs bus work and commits register effects (src/cpu.c, lines 1501–1528 and 1598–1734). Preserve that order. New ordered slots need declared capacity for full instruction work, checked spans/alignment, counter headroom and trace sequence capacity. For memory-loaded PC targets, keep refill slots symbolic until prior actual read results exist; resolve after the read under the already-preflighted current one-cycle bus cost. Future MMIO faults or variable waits require a new bus preparation/transaction contract before expanding this guarantee.

Existing test helpers compare canonical fields and contents rather than padding or allocator identity (tests/state_check.h, lines 131–175), then digest canonical bytes in fixed order (lines 255–293). Reuse these for state and caller-buffer refusal postconditions. tests/cpu_test.c, lines 1207–1217, is the compact one-short example: snapshot state/digest, run with too little budget, then assert timeout, zero work, equality and unchanged digest. Add one-short/exact-fit and boundary cases for maximum transaction/trace work and dynamic refill with non-default machine state.

For CPU mutations, tests/gates/core_mutation.cmake, lines 173–214 and 300–327, copies production source into an isolated build sandbox, changes one exact source expression, requires a named semantic assertion to fail, then compiles a clean companion. Extend for representative new semantics such as multi-transfer/writeback or dynamic PC resolution; do not count only changed guest test data as a core fault control.

### Toolchain lock/fetch: toolchain/arm-none-eabi.lock, scripts/fetch-toolchain.sh, tests/gates/toolchain_case.cmake

**Analog:** scripts/fetch-pin.sh, lines 159–188, stages a download inside its destination root, installs cleanup traps, applies bounded curl behavior, checks response/nonempty content, hashes before promotion, and only then places it. tests/gates/fetch_case.cmake, lines 286–294, fakes a redirecting curl with no network and checks a reason-coded failure plus no placement.

Keep pins.txt and scripts/fetch-pin.sh unchanged: they intentionally refuse redirects and cap files at 268435456 bytes. Use a separate toolchain lock/fetch/extract path for large pinned archives. Validate exact lock rows and approved redirect shape; never print signed URLs; check size and SHA before extraction; reject unsafe archive paths/types/links; extract into a fresh temporary tree; clean on failure; promote only after validation while preserving a prior install.

~~~sh
status=$("$curl_prog" -q --fail --silent --show-error --globoff \
  --proto "$proto" \
  --connect-timeout 20 --max-time 600 --max-filesize 268435456 \
  --write-out '%{http_code}' \
  --output "$tmp/payload" --url "$url") || die download-failed "$dest"
[ -s "$tmp/payload" ] || die empty-response "$dest"
got=$("$cmake_prog" -E sha256sum "$tmp/payload") || die digest-tool-failed "$dest"
got=${got%% *}
[ "$got" = "$want" ] || die digest-mismatch "$dest"
~~~

This is the current fetcher’s shape only; the dedicated large-tool path must retain its narrower tool-specific policy and must not inherit the generic 256 MiB cap.

No tracked safe archive-extraction implementation exists. Keep the helper small and use current shell/CMake/OS primitives. Offline controls should cover wrong redirect host/scheme/count, status/size/hash, unsafe member, corruption, failed-extractor non-invocation, no leftovers/out-of-root writes and prior-install preservation. Inspect actual notices after verifying the real pin.

### Original diagnostic/build pipeline: diagnostics/armv4t/*, cmake/BuildArmDiagnostics.cmake, cmake/FixGbaHeader.cmake, CMakeLists.txt

**Analog:** CMakeLists.txt, lines 102–159, declares generated binary outputs, makes the generator a target dependency, and feeds outputs into validated immutable indexes. Build ARM and Thumb sources from explicit project-owned assembly/startup/linker/header inputs under the selected build root with the same explicit dependency style.

~~~cmake
add_custom_command(
  OUTPUT ${GBL_OWNED_FIXTURE_FILES}
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${GBL_FIXTURE_DIR}"
  COMMAND "$<TARGET_FILE:gbl_instruction_image>" "${GBL_FIXTURE_DIR}"
  DEPENDS gbl_instruction_image
  COMMENT "Generating original owned instruction images under the build tree"
  VERBATIM)
add_custom_target(gbl_owned_instruction_images
  DEPENDS ${GBL_OWNED_FIXTURE_FILES})
~~~

The tracked tree has no ARM startup, linker script, header fixer or link-map audit. Treat proposed assembly inputs as original source. Set stack and mailbox explicitly, keep startup/mailbox instructions within the implemented subset, and write completion last. Use direct assembler/linker/objcopy plus project header fixer. Record per-object/final-ELF attributes, executable-section disassembly, exact map inputs, no-runtime proof, signature scan, tool identity and timing. A CPU flag, empty unresolved-symbol output or map filename alone is not proof. The map gate should allow only named project objects and reject runtime/archive members.

The gating build must compare the same final ROM bytes from macOS arm64 and hosted Linux x64 before any fixture ROM digest is admitted. Record the separate Linux arm64 witness independently. If macOS/Linux x64 bytes differ, retain and report the mismatch and use the documented Linux-owned digest fallback with honest host identities; never silently compare against a platform-specific digest.

Use one build-root-relative output path per preset (for example, generated/diagnostics/armv4t/), never a source-tree ROM or shared mutable binary directory. Generated ROMs remain ignored by .gitignore (/build*/ and *.gba, lines 1–11). Do not add broad build-tree or extension exceptions to source-control scans.

### Manifest/index/result identity: tests/fixtures/manifest.json, cmake/CheckManifest.cmake, cmake/GenerateFixtureIndex.cmake, cmake/CheckResult.cmake, runner/run.c

**Analog:** The manifest validator distinguishes source.kind=git with source commit from URL/in-tree source SHA (cmake/CheckManifest.cmake, lines 216–260); build toolchain identity/hash is independently validated at lines 304–327. GenerateFixtureIndex.cmake, lines 168–191, checks in-tree source bytes; lines 216–240 emit the resolved digest into immutable runner metadata.

~~~cmake
if("${source_kind}" STREQUAL "in-tree")
  gbl_json_string(relative_path "${production_doc}" fixtures ${index}
    source path)
  gbl_json_string(expected_digest "${production_doc}" fixtures ${index}
    source sha256)
  set(source_path "${GBL_SOURCE_DIR}/${relative_path}")
  if(NOT EXISTS "${source_path}" OR IS_DIRECTORY "${source_path}")
    gbl_finding(fixture-index in-tree-file-missing "${row_id}")
  else()
    file(SHA256 "${source_path}" actual_digest)
    if(NOT "${expected_digest}" STREQUAL "${actual_digest}")
      gbl_finding(fixture-index digest-mismatch "${row_id}")
    endif()
  endif()
endif()
~~~

Do not overload a git source commit or assembly digest as the ROM digest. Add an optional manifest/1 build-artifact declaration (build-root-relative path and SHA-256) only if needed to carry generated output identity separately from source revision. Preserve legacy rows and gamebloke.result/1; project artifact digest into existing immutable rom_sha256 metadata. Update CheckManifest, index generation and result digest binding coherently, retaining legacy controls. Configure/index generation must remain valid when optional local tools or outputs are absent; actual execution must still compare loaded bytes to indexed digest before guest execution.

runner/run.c, lines 423–460, refuses missing/mismatched registered-fixture digests before execution and produces SKIPPED/ran=false when indexed BIOS need is unmet. Keep that boundary. CheckResult.cmake, lines 358–366, binds reported digest to a row-pinned digest when available. Reuse the result identity path rather than adding a result/1 field. Add stale-source and stale-output controls independently; failed generation must not leave stale indexes. Source-only changes precede output-digest admission so the source commit does not contain its own pin.

tests/gates/manifest_case.cmake and tests/gates/result_case.cmake provide validation style: mutate sandbox copies, assert nonzero status plus stable finding code/detail, and retain clean positive cases. Keep the empty production manifest distinct from synthetic controls; those rows are not production authority or rights permission.

### Diagnostic result gate, exact output scan, inventory and CI

**Production CLI analog:** tests/gates/runner_case.cmake, lines 73–111, runs a real CLI through execute_process, checks exit/status/reason and one-line JSON, then reads semantic fields. It asserts an expected case ID explicitly at lines 180–209 because checker acceptance alone does not establish which case ran. For Phase 5, use gamebloke-run --fixture <id> for positive and broken guest assertions, then pass actual result to CheckResult.cmake. Assert exact status, mailbox state, case ID and fixture digest separately. Broken variants must execute the same instruction/assertion path and change the expected value; they must not write FAIL directly. Keep source/core mutations distinct from guest-negative plumbing.

**Production/control parity analog:** CMakeLists.txt, lines 89–100 and 161–183, shares runner/core sources and options while varying only private immutable index directories. tests/gates/runner_metadata.cmake, lines 69–98, inspects target wiring and refuses drift; lines 140–167 control stale image identity. Preserve source/options parity. Run diagnostics against the production index, not gamebloke-run-control or its synthetic manifest. tests/gates/runner_relocation.cmake, lines 29–108 and 223–252, moves the installed runner, clears index-selection environment variables, compares built/installed identity and behavior, and has a missing-executable control; preserve this regression.

**Rights scan analog:** scripts/signature-scan.sh, lines 139–158, scans a directory independent of Git, while scripts/lib/signature-common.sh, lines 57–80, evaluates logo/BIOS digests even for allowlisted files and limits allowlists to binary/extension findings. tests/gates/signature_case.cmake, lines 91–110 and 205–242, keeps control files outside the scanned tree and proves allowlisting does not hide logo/BIOS matches. Scan the exact generated ROM directory with a temporary allowlist of exact expected names. Keep logo and BIOS checks active; do not add broad *.gba or build-root exceptions.

**Skip/inventory analog:** cmake/GblTests.cmake, lines 13–54, marks only declared SKIPPABLE tests with the explicit SKIPPED: regular expression and records skip-ok in inventory. cmake/InventoryCompare.cmake, lines 21–58, requires a sorted exact match between registered tests and inventory. Local missing-toolchain tests can use this visible skip pattern with one stable reason. Present-but-broken setup/build/guest/checker paths must fail. Required diagnostic CI must separately fail setup if pinned tools are missing, prove guest tests actually ran, and prevent all-skipped from passing. Keep unmet BIOS as producer SKIPPED/ran=false, distinct from missing developer tools. Add tests to ci.txt and asan.txt only for lanes that build/use those assets.

CMakeLists.txt, lines 34–39, documents cmake --workflow --preset ci as the canonical verify command; CMakePresets.json, lines 47–63, makes inventory/no-tests behavior explicit. .github/workflows/ci.yml, lines 92–120, shows current build job and failure-only JUnit upload. Keep each hosted job capped at 180 seconds, do not upload ROMs, and choose placement after measuring fresh download/extract/build time. A separate toolchain job is justified only by its distinct recurring failure class; if selected, include it in workflow needs and required-check predicate.

### Profile completion: profiles/agb.json, docs/profile-agb.md, cmake/CheckProfile.cmake, tests/gates/profile_case.cmake

**Analog:** The profile is version 4 (profiles/agb.json, line 3; docs/profile-agb.md, line 18). CheckProfile.cmake, lines 381–405, checks supported/excluded operand rows in order; lines 1232–1291 compare visible documentation version and policy tables to JSON. tests/gates/profile_case.cmake, lines 30–105, copies committed files to a sandbox, proves clean copies pass, and requires exact validator findings for mutations.

For the expanded CPU claim, update profile version 4 to 5 and synchronize supported/excluded scope, timing/policy narrative, validators and positive/negative controls together. Keep machine/BIOS/claim limits honest; diagnostics do not claim hardware, BIOS, game compatibility or release qualification.

## Shared Patterns

### Whole-unit preflight and refusal
**Apply to:** CPU decode/execution and host tests.
**Sources:** src/cpu.c:35–65,1173–1226,1476–1528; tests/state_check.h:131–175,255–293; tests/cpu_test.c:1207–1217.

Plan from maximum declared work before effects. Preserve canonical state, counters, trace contents and caller output on refusal. Test non-default initial state, exact-fit success and one-short refusal; digest complements field equality.

### Verify-before-use identity
**Apply to:** tool downloads and source-built fixtures.
**Sources:** scripts/fetch-pin.sh:159–188; cmake/GenerateFixtureIndex.cmake:168–191; runner/run.c:423–460.

Keep archive hash, source revision and emitted ROM digest separate. Verify tools before extraction and fixture bytes before guest execution. Do not make output availability a configure-time requirement for unrelated lanes.

### Visible skips and mandatory CI
**Apply to:** optional local toolchain use and required diagnostic CI.
**Sources:** cmake/GblTests.cmake:43–54; cmake/InventoryCompare.cmake:21–58; .github/workflows/ci.yml:92–120.

A skip carries a specific reason and appears in inventory/JUnit. Required CI independently requires tool presence and actual guest execution. Build/execution failures are failures, not dependency skips.

### Rights controls
**Apply to:** every generated diagnostic output.
**Sources:** scripts/signature-scan.sh:139–158; scripts/lib/signature-common.sh:57–80; tests/gates/signature_case.cmake:205–242.

Exact-name allowlists may exempt expected ROM-extension/binary findings only. Logo-window and known-BIOS checks remain mandatory even for allowed generated ROMs. Keep ROM bytes and outputs out of source control and artifact uploads.

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| diagnostics/armv4t/startup.S, cases-arm.S, cases-thumb.S, linker.ld | guest test/config | transform/batch | No tracked ARM source, startup, linker script or source-owned GBA diagnostic exists. These must be original project inputs. |
| cmake/FixGbaHeader.cmake | utility | transform | No tracked header fixer exists; keep it small and test deterministic header fields. |
| tests/gates/diagnostic_linkmap.cmake | test/control | batch | No tracked link-map audit exists; add exact project-object checks and a planted runtime/archive negative control. |
| scripts/fetch-toolchain.sh extraction stage | utility | file-I/O | fetch-pin.sh covers bounded download/hash/promotion, not redirected archive validation or safe extraction. |

## Metadata

**Analog search scope:** src/, runner/, cmake/, scripts/, tests/, profiles/, docs/, .github/workflows/ and phase research.
**Tracked-source gate:** every existing code analog named above is in git ls-files.
**Current integration seam:** the production runner reads its immutable generated index; canonical CMake workflow is cmake --workflow --preset ci; toolchain work must fit the existing 180-second job cap based on measurement.
**Pattern extraction date:** 2026-11-11
