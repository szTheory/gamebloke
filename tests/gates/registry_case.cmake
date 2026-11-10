# Dispute-registry negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/registry_case.cmake
#
# Every input is a mutated COPY of registry/disputes.json (with a copy of
# contracts/evidence.json as the vocabulary) under BIN/nc/registry_<case>; the
# committed files are never modified and no bad example is committed (D-04).
# `copy` proves the unmutated copy is accepted, so every negative case fails
# only for its planted reason. Each negative variant asserts a non-zero exit
# AND the validator's stable reason code through cmake/ExpectFail.cmake; a stale
# mutation fails with `mutate: no-match`.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "registry-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "registry-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/registry_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(CHECK "${SRC}/cmake/CheckRegistry.cmake")
file(COPY_FILE "${SRC}/contracts/evidence.json" "${dir}/evidence.json")
set(VOCAB_COPY "${dir}/evidence.json")

# registry_copy(<out-var> <variant>): a fresh sandbox copy of the real registry.
function(registry_copy out variant)
  set(path "${dir}/disputes_${variant}.json")
  file(COPY_FILE "${SRC}/registry/disputes.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# expect_code(<code> <file>): the real validator on <file> fails with <code>.
function(expect_code code file)
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DREGISTRY=${file}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "registry: ${code}")
endfunction()

if(CASE STREQUAL "copy")
  registry_copy(f copy)
  gbl_expect_pass(
    COMMAND "${CMAKE_COMMAND}" "-DREGISTRY=${f}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "registry: ok \\(6 entries\\)")

elseif(CASE STREQUAL "seed_missing")
  # Delete each seed in turn from a fresh copy; each deletion names its own id.
  file(READ "${SRC}/registry/disputes.json" real)
  string(JSON n LENGTH "${real}" entries)
  if(NOT n EQUAL 6)
    message(FATAL_ERROR "registry-case: expected 6 seed entries, found ${n}")
  endif()
  foreach(i RANGE 5)
    string(JSON seed GET "${real}" entries ${i} id)
    registry_copy(f seed${i})
    gbl_json_mutate_remove("${f}" entries ${i})
    expect_code("seed-missing: ${seed}" "${f}")
  endforeach()

elseif(CASE STREQUAL "bad_schema")
  registry_copy(f schema)
  gbl_json_mutate_set("${f}" "\"gamebloke.disputes/0\"" schema)
  expect_code(bad-schema "${f}")

elseif(CASE STREQUAL "missing_field")
  # Entry 0 is the first seed. Absent, null, and empty summary; empty affects.
  registry_copy(f summary_removed)
  gbl_json_mutate_remove("${f}" entries 0 summary)
  expect_code(missing-field "${f}")
  registry_copy(f summary_null)
  gbl_json_mutate_set("${f}" "null" entries 0 summary)
  expect_code(missing-field "${f}")
  registry_copy(f summary_empty)
  gbl_json_mutate_set("${f}" "\"\"" entries 0 summary)
  expect_code(missing-field "${f}")
  registry_copy(f affects_empty)
  gbl_json_mutate_set("${f}" "[]" entries 0 affects)
  expect_code(missing-field "${f}")

elseif(CASE STREQUAL "duplicate_id")
  # Entry 1 takes entry 0's id. The overwritten seed also reports seed-missing;
  # that co-reported line is accepted, the REGEX asserts duplicate-id is present.
  registry_copy(f dup)
  gbl_json_mutate_set("${f}" "\"disp-dma-latch\"" entries 1 id)
  expect_code(duplicate-id "${f}")

elseif(CASE STREQUAL "bad_id")
  registry_copy(f upper)
  gbl_json_mutate_set("${f}" "\"DMA-latch\"" entries 0 id)
  expect_code(bad-id "${f}")
  registry_copy(f underscore)
  gbl_json_mutate_set("${f}" "\"disp_x\"" entries 0 id)
  expect_code(bad-id "${f}")

elseif(CASE STREQUAL "bad_status")
  registry_copy(f resolved)
  gbl_json_mutate_set("${f}" "\"resolved\"" entries 0 status)
  expect_code(bad-status "${f}")

elseif(CASE STREQUAL "bad_source_label")
  # A reserved later-phase label and a lower-case variant are both outside the
  # five-label vocabulary.
  registry_copy(f calibrated)
  gbl_json_mutate_set("${f}" "\"calibrated:x\"" entries 0 sources 0 label)
  expect_code(bad-source-label "${f}")
  registry_copy(f lower)
  gbl_json_mutate_set("${f}" "\"ref\"" entries 0 sources 2 label)
  expect_code(bad-source-label "${f}")

elseif(CASE STREQUAL "no_sources")
  registry_copy(f empty)
  gbl_json_mutate_set("${f}" "[]" entries 0 sources)
  expect_code(no-sources "${f}")
  registry_copy(f removed)
  gbl_json_mutate_remove("${f}" entries 0 sources)
  expect_code(no-sources "${f}")

elseif(CASE STREQUAL "usage")
  gbl_expect_fail(COMMAND "${CMAKE_COMMAND}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "registry: usage")

elseif(CASE STREQUAL "extra_entry")
  # A well-formed seventh entry is accepted: the check is not a frozen-file
  # comparison. SET at index 6 appends to the six-member array.
  registry_copy(f extra)
  gbl_json_mutate_set("${f}"
    "{\"id\": \"disp-example-extra\", \"summary\": \"A synthesized entry used only by this control.\", \"sources\": [{\"label\": \"UNK\", \"ref\": \"none\", \"note\": \"Synthesized at test time.\"}], \"status\": \"open\", \"affects\": [\"example\"]}"
    entries 6)
  gbl_expect_pass(
    COMMAND "${CMAKE_COMMAND}" "-DREGISTRY=${f}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "registry: ok \\(7 entries\\)")

else()
  message(FATAL_ERROR "registry-case: unknown case: ${CASE}")
endif()
