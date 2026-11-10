# Manifest negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/manifest_case.cmake
#
# Every input is a mutated COPY of contracts/examples/manifest.json (with copies of
# tests/fixtures/manifest.json, pins.txt, registry/disputes.json, and
# contracts/evidence.json) under BIN/nc/manifest_<case>; the committed files are never
# modified and no bad example is committed (D-04). Each negative variant asserts a
# non-zero exit AND the linter's stable reason code through cmake/ExpectFail.cmake; a
# stale mutation fails with `mutate: no-match`. A variant changes one field;
# co-reported findings are accepted because the REGEX asserts that the targeted code
# line is present.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "manifest-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "manifest-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/manifest_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(CHECK "${SRC}/cmake/CheckManifest.cmake")
file(COPY_FILE "${SRC}/contracts/examples/manifest.json" "${dir}/manifest.json")
file(COPY_FILE "${SRC}/tests/fixtures/manifest.json" "${dir}/empty.json")
file(COPY_FILE "${SRC}/pins.txt" "${dir}/pins.txt")
file(COPY_FILE "${SRC}/registry/disputes.json" "${dir}/disputes.json")
file(COPY_FILE "${SRC}/contracts/evidence.json" "${dir}/evidence.json")
set(VOCAB_COPY "${dir}/evidence.json")
set(REGISTRY_COPY "${dir}/disputes.json")
set(PINS_COPY "${dir}/pins.txt")

# The example row's commit, used to build variants.
set(COMMIT "0123456789abcdef0123456789abcdef01234567")

# manifest_copy(<out-var> <variant>): a fresh sandbox copy of the example manifest.
function(manifest_copy out variant)
  set(path "${dir}/manifest_${variant}.json")
  file(COPY_FILE "${dir}/manifest.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# manifest_command(<out-var> <mode> <manifest>): the linter command line.
function(manifest_command out mode manifest)
  set(${out}
    "${CMAKE_COMMAND}" "-DMODE=${mode}" "-DMANIFEST=${manifest}" "-DPINS=${PINS_COPY}"
    "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    PARENT_SCOPE)
endfunction()

# expect_code(<code> <manifest>): the linter (MODE=manifest) fails with <code>.
function(expect_code code manifest)
  manifest_command(cmd manifest "${manifest}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "manifest: ${code}")
endfunction()

if(CASE STREQUAL "complete_row")
  manifest_copy(m complete)
  manifest_command(cmd manifest "${m}")
  gbl_expect_pass(COMMAND ${cmd} REGEX "manifest: ok \\(1 rows\\)")

elseif(CASE STREQUAL "unpinned_revision")
  # A branch, HEAD, a short name, a 7-hex short SHA, uppercase hex, and a row with only
  # a tag are all outside a 40-lowercase-hex commit.
  string(TOUPPER "${COMMIT}" upper)
  set(i 0)
  foreach(v IN ITEMS "main" "HEAD" "master" "0123abc" "${upper}")
    manifest_copy(m commit${i})
    gbl_json_mutate_set("${m}" "\"${v}\"" fixtures 0 source commit)
    expect_code(unpinned-revision "${m}")
    math(EXPR i "${i}+1")
  endforeach()
  manifest_copy(m tag_only)
  gbl_json_mutate_remove("${m}" fixtures 0 source commit)
  expect_code(unpinned-revision "${m}")

else()
  message(FATAL_ERROR "manifest-case: unknown case: ${CASE}")
endif()
