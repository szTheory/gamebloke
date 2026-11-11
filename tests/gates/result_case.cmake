# Result negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/result_case.cmake
#
# Every input is a mutated COPY of contracts/examples/result-pass.json (with copies of
# contracts/examples/manifest.json, registry/disputes.json, and contracts/evidence.json)
# under BIN/nc/result_<case>; the committed files are never modified and no bad example is
# committed (D-04). Each negative variant asserts a non-zero exit AND the validator's
# stable reason code through cmake/ExpectFail.cmake; a stale mutation fails with
# `mutate: no-match`. A variant changes one field; co-reported findings are accepted
# because the REGEX asserts that the targeted code line is present.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "result-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "result-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/result_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(CHECK "${SRC}/cmake/CheckResult.cmake")
file(COPY_FILE "${SRC}/contracts/examples/result-pass.json" "${dir}/result.json")
file(COPY_FILE "${SRC}/contracts/examples/manifest.json" "${dir}/manifest.json")
file(COPY_FILE "${SRC}/registry/disputes.json" "${dir}/disputes.json")
file(COPY_FILE "${SRC}/contracts/evidence.json" "${dir}/evidence.json")
set(REGISTRY_COPY "${dir}/disputes.json")
set(VOCAB_COPY "${dir}/evidence.json")

# result_copy(<out-var> <variant>): a fresh sandbox copy of the example result.
function(result_copy out variant)
  set(path "${dir}/result_${variant}.json")
  file(COPY_FILE "${dir}/result.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# manifest_copy(<out-var> <variant>): a fresh sandbox copy of the example manifest.
function(manifest_copy out variant)
  set(path "${dir}/manifest_${variant}.json")
  file(COPY_FILE "${dir}/manifest.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# result_command(<out-var> <result> <manifest>): the validator command line.
function(result_command out result manifest)
  set(${out}
    "${CMAKE_COMMAND}" "-DRESULT=${result}" "-DMANIFEST=${manifest}"
    "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    PARENT_SCOPE)
endfunction()

# expect_code(<code> <result> [<manifest>]): the validator fails with <code>.
function(expect_code code result)
  set(manifest "${dir}/manifest.json")
  if(NOT "${ARGV2}" STREQUAL "")
    set(manifest "${ARGV2}")
  endif()
  result_command(cmd "${result}" "${manifest}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "result: ${code}")
endfunction()

# expect_detail(<code> <detail-regex> <result> [<manifest>]): fails with <code> and that
# finding detail (findings are printed unwrapped). A bare code can be emitted by another
# check, so a control that targets one trigger names its detail.
function(expect_detail code detail result)
  set(manifest "${dir}/manifest.json")
  if(NOT "${ARGV3}" STREQUAL "")
    set(manifest "${ARGV3}")
  endif()
  result_command(cmd "${result}" "${manifest}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "result: ${code}: ${detail}")
endfunction()

# expect_ok(<status> <result> [<manifest>]): the validator accepts with that status.
function(expect_ok status result)
  set(manifest "${dir}/manifest.json")
  if(NOT "${ARGV2}" STREQUAL "")
    set(manifest "${ARGV2}")
  endif()
  result_command(cmd "${result}" "${manifest}")
  gbl_expect_pass(COMMAND ${cmd} REGEX "result: ok \\(${status}\\)")
endfunction()

# hex(<out-var> <fragment> <times>): a synthesized hex string.
function(hex out fragment times)
  string(REPEAT "${fragment}" ${times} value)
  set(${out} "${value}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "pass_without_run")
  # The fixture did not run, but the digest and everything else say PASS.
  result_copy(r no_run)
  gbl_json_mutate_set("${r}" "false" fixture ran)
  expect_code(pass-without-run "${r}")

elseif(CASE STREQUAL "all_states")
  # Every one of the nine states has a minimal accepted document, so none is unreachable.
  # The names and rules are written out here on purpose: this is the independent oracle
  # for the data the validator reads.
  foreach(state IN ITEMS PASS GUEST_FAIL GUEST_STOP HARNESS_FAIL TIMEOUT CANCELLED
      MISSING SKIPPED REFUSED)
    result_copy(r state_${state})
    if(NOT state STREQUAL "PASS")
      gbl_json_mutate_set("${r}" "\"${state}\"" status)
    endif()
    if(state STREQUAL "GUEST_FAIL")
      gbl_json_mutate_set("${r}" "\"fail\"" semantic state)
      gbl_json_mutate_set("${r}" "\"2\"" semantic case_id)
    elseif(state STREQUAL "GUEST_STOP")
      gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
      gbl_json_mutate_set("${r}" "\"3\"" semantic case_id)
      gbl_json_mutate_set("${r}" "\"no-bios-swi\"" reason)
      gbl_json_mutate_set("${r}" "\"no-bios-swi\"" termination)
    elseif(state STREQUAL "HARNESS_FAIL")
      gbl_json_mutate_set("${r}" "\"trace-output-full\"" reason)
      gbl_json_mutate_set("${r}" "\"trace-output-full\"" termination)
      gbl_json_mutate_set("${r}" "{\"complete\": false}" trace)
    elseif(state STREQUAL "TIMEOUT")
      gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
      gbl_json_mutate_set("${r}" "\"3\"" semantic case_id)
      gbl_json_mutate_set("${r}" "\"cycle-budget\"" reason)
      gbl_json_mutate_set("${r}" "\"timeout\"" termination)
      gbl_json_mutate_set("${r}" "\"23\"" time cycles)
    elseif(state STREQUAL "CANCELLED")
      gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
      gbl_json_mutate_set("${r}" "\"3\"" semantic case_id)
      gbl_json_mutate_set("${r}" "\"cancelled\"" reason)
      gbl_json_mutate_set("${r}" "\"cancelled\"" termination)
      gbl_json_mutate_set("${r}" "\"1\"" time cycles)
    endif()
    if(state STREQUAL "MISSING" OR state STREQUAL "SKIPPED" OR state STREQUAL "REFUSED")
      gbl_json_mutate_set("${r}" "false" fixture ran)
      gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
      gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
      gbl_json_mutate_set("${r}" "\"absent\"" semantic state)
      gbl_json_mutate_set("${r}" "null" semantic case_id)
      gbl_json_mutate_set("${r}" "\"0\"" time cycles)
      gbl_json_mutate_set("${r}" "\"0\"" time instructions)
      gbl_json_mutate_set("${r}" "\"0\"" time budget_cycles)
      if(state STREQUAL "MISSING")
        gbl_json_mutate_set("${r}" "\"rom-file-not-found\"" reason)
        gbl_json_mutate_set("${r}" "\"not-started\"" termination)
      elseif(state STREQUAL "SKIPPED")
        gbl_json_mutate_set("${r}" "\"required-bios-unavailable\"" reason)
        gbl_json_mutate_set("${r}" "\"skipped-before-run\"" termination)
      else()
        gbl_json_mutate_set("${r}" "\"declared-extent\"" reason)
        gbl_json_mutate_set("${r}" "\"refused-before-run\"" termination)
      endif()
    endif()
    if(state STREQUAL "MISSING" OR state STREQUAL "SKIPPED")
      gbl_json_mutate_set("${r}" "\"the fixture was not built in this run\"" reason)
    endif()
    if(state STREQUAL "REFUSED")
      gbl_json_mutate_set("${r}"
        "{\"code\": \"declared-extent\", \"postcondition\": \"no state was changed\"}" refusal)
    endif()
    expect_ok("${state}" "${r}")
  endforeach()

elseif(CASE STREQUAL "semantic_time")
  # The exact budget boundary is inclusive; the checked-in example sits at that edge.
  result_copy(r exact_boundary)
  expect_ok(PASS "${r}")
  result_copy(r under_budget)
  gbl_json_mutate_set("${r}" "\"25\"" time budget_cycles)
  expect_ok(PASS "${r}")

  result_copy(r pending_pass)
  gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
  expect_detail(status-semantic-mismatch "PASS requires semantic.state pass" "${r}")
  result_copy(r bad_case_id)
  gbl_json_mutate_set("${r}" "1" semantic case_id)
  expect_detail(bad-semantic "semantic.case_id must be a string or null" "${r}")
  result_copy(r absent_case_id)
  gbl_json_mutate_set("${r}" "\"0\"" semantic case_id)
  expect_detail(bad-semantic "completed mailbox needs a nonzero case_id" "${r}")

  result_copy(r numeric_cycles)
  gbl_json_mutate_set("${r}" "24" time cycles)
  expect_detail(bad-time "time.cycles must be a decimal string" "${r}")
  result_copy(r noncanonical_cycles)
  gbl_json_mutate_set("${r}" "\"01\"" time cycles)
  expect_detail(bad-time "time.cycles" "${r}")
  result_copy(r overflow_cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551616\"" time cycles)
  expect_detail(time-overflow "time.cycles" "${r}")
  result_copy(r over_budget)
  gbl_json_mutate_set("${r}" "\"25\"" time cycles)
  expect_detail(cycles-exceed-budget "time.cycles exceeds time.budget_cycles" "${r}")

  result_copy(r bad_stop_termination)
  gbl_json_mutate_set("${r}" "\"GUEST_STOP\"" status)
  gbl_json_mutate_set("${r}" "\"no-bios-swi\"" reason)
  gbl_json_mutate_set("${r}" "\"timeout\"" termination)
  expect_detail(bad-termination "no-BIOS GUEST_STOP termination must match reason" "${r}")
  result_copy(r missing_stop_reason)
  gbl_json_mutate_set("${r}" "\"GUEST_STOP\"" status)
  gbl_json_mutate_set("${r}" "\"no-bios-swi\"" termination)
  gbl_json_mutate_set("${r}" "\"no-bios-swi\"" reason)
  gbl_json_mutate_remove("${r}" reason)
  expect_detail(bad-termination "no-BIOS GUEST_STOP termination must match reason" "${r}")
  result_copy(r incomplete_pass)
  gbl_json_mutate_set("${r}" "{\"complete\": false}" trace)
  expect_detail(incomplete-trace "PASS cannot have an incomplete trace" "${r}")
  result_copy(r complete_trace_full)
  gbl_json_mutate_set("${r}" "\"HARNESS_FAIL\"" status)
  gbl_json_mutate_set("${r}" "\"trace-output-full\"" reason)
  gbl_json_mutate_set("${r}" "\"trace-output-full\"" termination)
  gbl_json_mutate_set("${r}" "{\"complete\": true}" trace)
  expect_detail(incomplete-trace "trace-output-full HARNESS_FAIL requires trace.complete false" "${r}")

elseif(CASE STREQUAL "usage")
  # No RESULT, and no MANIFEST.
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DMANIFEST=${dir}/manifest.json"
      "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "result: usage")
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DRESULT=${dir}/result.json"
      "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "result: usage")

elseif(CASE STREQUAL "bad_schema")
  result_copy(r schema)
  gbl_json_mutate_set("${r}" "\"gamebloke.result/2\"" schema)
  expect_code(bad-schema "${r}")

elseif(CASE STREQUAL "missing_field")
  # Every required top-level key, fixture.id, fixture.rom_sha256, and each of the six claim
  # fields removed; fixture.ran as a string instead of a boolean.
  foreach(path IN ITEMS
      schema fixture status claim disputes labels limits semantic time termination trace
      fixture/id fixture/rom_sha256 semantic/state semantic/case_id time/cycles
      time/instructions time/budget_cycles
      claim/machine claim/boot claim/bios_provider claim/cart claim/input_timeline
      claim/evidence)
    string(REPLACE "/" ";" keys "${path}")
    string(REPLACE "/" "_" tag "${path}")
    result_copy(r rm_${tag})
    gbl_json_mutate_remove("${r}" ${keys})
    expect_code(missing-field "${r}")
  endforeach()
  result_copy(r ran_string)
  gbl_json_mutate_set("${r}" "\"true\"" fixture ran)
  expect_code(missing-field "${r}")

elseif(CASE STREQUAL "unknown_status")
  set(i 0)
  foreach(v IN ITEMS "pass" "PASSED" "OK" "DISPUTED")
    result_copy(r status${i})
    gbl_json_mutate_set("${r}" "\"${v}\"" status)
    expect_code(unknown-status "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "pass_without_digest")
  # PASS with a null digest, and with a 63-hex digest (the claim cart changed to match, so
  # only the digest is wrong).
  hex(h63 "a" 63)
  result_copy(r null_digest)
  gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
  expect_code(pass-without-digest "${r}")
  result_copy(r short_digest)
  gbl_json_mutate_set("${r}" "\"${h63}\"" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "\"${h63}\"" claim cart rom_sha256)
  expect_code(pass-without-digest "${r}")

elseif(CASE STREQUAL "ran_state_mismatch")
  # A state whose row says the fixture did not run, claimed with ran true; a state whose
  # row says it ran, claimed with ran false.
  result_copy(r skipped_ran)
  gbl_json_mutate_set("${r}" "\"SKIPPED\"" status)
  gbl_json_mutate_set("${r}" "\"not built in this run\"" reason)
  expect_code(ran-state-mismatch "${r}")
  result_copy(r fail_not_ran)
  gbl_json_mutate_set("${r}" "\"GUEST_FAIL\"" status)
  gbl_json_mutate_set("${r}" "false" fixture ran)
  expect_code(ran-state-mismatch "${r}")

elseif(CASE STREQUAL "missing_digest")
  # States that ran must carry a digest.
  foreach(state IN ITEMS GUEST_FAIL GUEST_STOP TIMEOUT)
    result_copy(r digest_${state})
    gbl_json_mutate_set("${r}" "\"${state}\"" status)
    gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
    gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
    expect_detail(missing-digest "${state} needs fixture.rom_sha256" "${r}")
  endforeach()
  # A state whose digest is optional still rejects a malformed digest string (10 hex).
  result_copy(r malformed)
  gbl_json_mutate_set("${r}" "\"HARNESS_FAIL\"" status)
  gbl_json_mutate_set("${r}" "\"0123456789\"" fixture rom_sha256)
  expect_detail(missing-digest "fixture.rom_sha256 is malformed" "${r}")

elseif(CASE STREQUAL "missing_reason")
  # SKIPPED with no reason; MISSING with an empty reason.
  result_copy(r skipped)
  gbl_json_mutate_set("${r}" "\"SKIPPED\"" status)
  gbl_json_mutate_set("${r}" "false" fixture ran)
  gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
  expect_code(missing-reason "${r}")
  result_copy(r missing)
  gbl_json_mutate_set("${r}" "\"MISSING\"" status)
  gbl_json_mutate_set("${r}" "false" fixture ran)
  gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
  gbl_json_mutate_set("${r}" "\"\"" reason)
  expect_code(missing-reason "${r}")

elseif(CASE STREQUAL "missing_refusal")
  # REFUSED with no refusal object, with no code, and with no postcondition.
  set(i 0)
  foreach(v IN ITEMS "" "{\"postcondition\": \"no state was changed\"}"
      "{\"code\": \"declared-extent\"}")
    result_copy(r refused${i})
    gbl_json_mutate_set("${r}" "\"REFUSED\"" status)
    gbl_json_mutate_set("${r}" "false" fixture ran)
    gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
    gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
    if(NOT "${v}" STREQUAL "")
      gbl_json_mutate_set("${r}" "${v}" refusal)
    endif()
    expect_code(missing-refusal "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_machine")
  set(i 0)
  foreach(v IN ITEMS "agb-micro" "AGB")
    result_copy(r machine${i})
    gbl_json_mutate_set("${r}" "\"${v}\"" claim machine)
    expect_code(bad-machine "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_boot")
  result_copy(r boot)
  gbl_json_mutate_set("${r}" "\"bios\"" claim boot)
  expect_code(bad-boot "${r}")

elseif(CASE STREQUAL "bad_bios_provider")
  # The retired milestone label, an empty hle list, a branch-named replacement, a short
  # user-official digest, and a trailing space.
  set(i 0)
  foreach(v IN ITEMS "M1" "hle:" "replacement:x@main" "user-official@abc" "none ")
    result_copy(r bios${i})
    gbl_json_mutate_set("${r}" "\"${v}\"" claim bios_provider)
    expect_code(bad-bios-provider "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_cart")
  result_copy(r kind)
  gbl_json_mutate_set("${r}" "\"flash\"" claim cart kind)
  expect_code(bad-cart "${r}")
  result_copy(r digest)
  gbl_json_mutate_set("${r}" "\"xyz\"" claim cart rom_sha256)
  expect_code(bad-cart "${r}")

elseif(CASE STREQUAL "bad_input_timeline")
  hex(h10 "a" 10)
  result_copy(r kind)
  gbl_json_mutate_set("${r}" "\"movie\"" claim input_timeline kind)
  expect_code(bad-input-timeline "${r}")
  result_copy(r script_no_digest)
  gbl_json_mutate_set("${r}" "\"script\"" claim input_timeline kind)
  expect_code(bad-input-timeline "${r}")
  result_copy(r script_short_digest)
  gbl_json_mutate_set("${r}" "\"script\"" claim input_timeline kind)
  gbl_json_mutate_set("${r}" "\"${h10}\"" claim input_timeline sha256)
  expect_code(bad-input-timeline "${r}")

elseif(CASE STREQUAL "bad_evidence")
  # Empty list; an item without a class; a class outside the vocabulary; a class that
  # differs from the manifest row (the example row is suite-self-checking).
  result_copy(r empty)
  gbl_json_mutate_set("${r}" "[]" claim evidence)
  expect_code(bad-evidence "${r}")
  result_copy(r no_class)
  gbl_json_mutate_remove("${r}" claim evidence 0 provenance_class)
  expect_code(bad-evidence "${r}")
  result_copy(r foreign_class)
  gbl_json_mutate_set("${r}" "\"emulator-agreement\"" claim evidence 0 provenance_class)
  expect_code(bad-evidence "${r}")
  result_copy(r row_class)
  gbl_json_mutate_set("${r}" "\"project-authored\"" claim evidence 0 provenance_class)
  expect_code(bad-evidence "${r}")
  # An item that does not name its fixture (WR-08).
  result_copy(r no_fixture)
  gbl_json_mutate_remove("${r}" claim evidence 0 fixture)
  expect_detail(bad-evidence "claim.evidence\\[0\\]: fixture" "${r}")

elseif(CASE STREQUAL "claim_mismatch")
  # The claim names a different ROM than the fixture block.
  hex(other "cd" 32)
  result_copy(r digest)
  gbl_json_mutate_set("${r}" "\"${other}\"" claim cart rom_sha256)
  expect_detail(claim-mismatch "claim.cart.rom_sha256 differs" "${r}")

  # The evidence list names a fixture, but not the one this result is for. A second
  # manifest row exists so the item is a known fixture (no unknown-fixture finding).
  manifest_copy(m two_rows)
  file(READ "${m}" mdoc)
  string(JSON row GET "${mdoc}" fixtures 0)
  gbl_json_mutate_set("${m}" "${row}" fixtures 1)
  gbl_json_mutate_set("${m}" "\"other-row\"" fixtures 1 id)
  result_copy(r evidence_other)
  gbl_json_mutate_set("${r}" "\"other-row\"" claim evidence 0 fixture)
  expect_detail(claim-mismatch "no evidence item names example-row" "${r}" "${m}")

elseif(CASE STREQUAL "manifest_digest")
  # WR-04: the result digest is tied to the manifest row's pinned digest. The row becomes
  # a url row; its sha256 differs from the result's 0123... digest (negative), then
  # equals it (the same document is accepted, so the tie is the only difference).
  hex(row_other "ab" 32)
  manifest_copy(m differs)
  gbl_json_mutate_set("${m}"
    "{\"kind\": \"url\", \"url\": \"https://example.invalid/a.dat\", \"sha256\": \"${row_other}\"}"
    fixtures 0 source)
  expect_detail(claim-mismatch
    "fixture.rom_sha256 differs from the manifest digest for example-row" "${dir}/result.json" "${m}")
  file(READ "${dir}/result.json" rdoc)
  string(JSON same GET "${rdoc}" fixture rom_sha256)
  manifest_copy(m same)
  gbl_json_mutate_set("${m}"
    "{\"kind\": \"url\", \"url\": \"https://example.invalid/a.dat\", \"sha256\": \"${same}\"}"
    fixtures 0 source)
  expect_ok(PASS "${dir}/result.json" "${m}")
  # An in-tree row ties the same way.
  manifest_copy(m tree)
  gbl_json_mutate_set("${m}"
    "{\"kind\": \"in-tree\", \"path\": \"tests/fixtures/x.dat\", \"sha256\": \"${row_other}\"}"
    fixtures 0 source)
  expect_detail(claim-mismatch "fixture.rom_sha256 differs from the manifest digest"
    "${dir}/result.json" "${m}")

elseif(CASE STREQUAL "empty_limits")
  set(i 0)
  foreach(v IN ITEMS "[]" "[\"\"]" "\"not a list\"")
    result_copy(r limits${i})
    gbl_json_mutate_set("${r}" "${v}" limits)
    expect_code(empty-limits "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bios_provider_forms")
  # All five R1 forms are accepted; the example already holds the first.
  hex(h40 "ab" 20)
  hex(h64 "cd" 32)
  result_copy(r none)
  expect_ok(PASS "${r}")
  set(i 0)
  foreach(v IN ITEMS "project-stub" "hle:CpuSet,Div" "replacement:open-bios@${h40}"
      "user-official@${h64}")
    result_copy(r bios${i})
    gbl_json_mutate_set("${r}" "\"${v}\"" claim bios_provider)
    expect_ok(PASS "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "unknown_fixture")
  # The result names a fixture the manifest does not hold; so does an evidence item.
  result_copy(r id)
  gbl_json_mutate_set("${r}" "\"nope-row\"" fixture id)
  expect_code(unknown-fixture "${r}")
  result_copy(r evidence)
  gbl_json_mutate_set("${r}" "\"nope-row\"" claim evidence 0 fixture)
  expect_code(unknown-fixture "${r}")

elseif(CASE STREQUAL "unknown_dispute")
  result_copy(r nope)
  gbl_json_mutate_set("${r}" "[\"disp-nope\"]" disputes)
  gbl_json_mutate_set("${r}" "[\"disputed\"]" labels)
  expect_code(unknown-dispute "${r}")

elseif(CASE STREQUAL "undisclosed_dispute")
  # The manifest row points at a registry entry (class disputed) and the evidence item
  # carries that class, but the result lists no dispute and no label; then it lists the
  # dispute and still omits the label.
  manifest_copy(m disputed)
  gbl_json_mutate_set("${m}" "\"disputed\"" fixtures 0 expected provenance_class)
  gbl_json_mutate_set("${m}" "\"disp-mgba-suite-27\"" fixtures 0 expected dispute)
  result_copy(r silent)
  gbl_json_mutate_set("${r}" "\"disputed\"" claim evidence 0 provenance_class)
  expect_code(undisclosed-dispute "${r}" "${m}")
  result_copy(r no_label)
  gbl_json_mutate_set("${r}" "\"disputed\"" claim evidence 0 provenance_class)
  gbl_json_mutate_set("${r}" "[\"disp-mgba-suite-27\"]" disputes)
  expect_code(undisclosed-dispute "${r}" "${m}")
  # A listed dispute with no label is undisclosed even when the row is not disputed.
  result_copy(r listed_only)
  gbl_json_mutate_set("${r}" "[\"disp-dma-latch\"]" disputes)
  expect_code(undisclosed-dispute "${r}")

elseif(CASE STREQUAL "label_without_dispute")
  result_copy(r label)
  gbl_json_mutate_set("${r}" "[\"disputed\"]" labels)
  expect_code(label-without-dispute "${r}")

elseif(CASE STREQUAL "bad_label")
  result_copy(r verified)
  gbl_json_mutate_set("${r}" "[\"verified\"]" labels)
  expect_code(bad-label "${r}")
  result_copy(r duplicate)
  gbl_json_mutate_set("${r}" "[\"disp-dma-latch\"]" disputes)
  gbl_json_mutate_set("${r}" "[\"disputed\", \"disputed\"]" labels)
  expect_code(bad-label "${r}")

elseif(CASE STREQUAL "disputed")
  # The honest form: the row is disputed, the evidence says so, and the result lists the
  # registry id and carries the label (ROADMAP SC5).
  manifest_copy(m disputed)
  gbl_json_mutate_set("${m}" "\"disputed\"" fixtures 0 expected provenance_class)
  gbl_json_mutate_set("${m}" "\"disp-mgba-suite-27\"" fixtures 0 expected dispute)
  result_copy(r disputed)
  gbl_json_mutate_set("${r}" "\"disputed\"" claim evidence 0 provenance_class)
  gbl_json_mutate_set("${r}" "[\"disp-mgba-suite-27\"]" disputes)
  gbl_json_mutate_set("${r}" "[\"disputed\"]" labels)
  expect_ok(PASS "${r}" "${m}")

else()
  message(FATAL_ERROR "result-case: unknown case: ${CASE}")
endif()
