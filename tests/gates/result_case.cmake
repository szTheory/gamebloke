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
    if(state STREQUAL "MISSING" OR state STREQUAL "SKIPPED" OR state STREQUAL "REFUSED")
      gbl_json_mutate_set("${r}" "false" fixture ran)
      gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
      gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
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

else()
  message(FATAL_ERROR "result-case: unknown case: ${CASE}")
endif()
