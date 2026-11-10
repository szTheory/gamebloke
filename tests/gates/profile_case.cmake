# Profile negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/profile_case.cmake
#
# Every input is a mutated COPY of profiles/agb.json or docs/profile-agb.md (with
# copies of registry/disputes.json and contracts/evidence.json) under
# BIN/nc/profile_<case>; the committed files are never modified and no bad example
# is committed (D-04). `copy` proves the unmutated copies are accepted, so every
# negative case fails only for its planted reason. Each negative variant asserts a
# non-zero exit AND the validator's stable reason code through
# cmake/ExpectFail.cmake; a stale mutation fails with `mutate: no-match`. A variant
# changes one field; co-reported findings are accepted because the REGEX asserts
# that the targeted code line is present.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "profile-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "profile-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/profile_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(CHECK "${SRC}/cmake/CheckProfile.cmake")
file(COPY_FILE "${SRC}/contracts/evidence.json" "${dir}/evidence.json")
file(COPY_FILE "${SRC}/registry/disputes.json" "${dir}/disputes.json")
set(VOCAB_COPY "${dir}/evidence.json")
set(REGISTRY_COPY "${dir}/disputes.json")

# profile_copy(<out-var> <variant>): a fresh sandbox copy of the real profile.
function(profile_copy out variant)
  set(path "${dir}/profile_${variant}.json")
  file(COPY_FILE "${SRC}/profiles/agb.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# doc_copy(<out-var> <variant>): a fresh sandbox copy of the real document.
function(doc_copy out variant)
  set(path "${dir}/doc_${variant}.md")
  file(COPY_FILE "${SRC}/docs/profile-agb.md" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# check_command(<out-var> <profile> <doc>): the validator command line.
function(check_command out profile doc)
  set(${out}
    "${CMAKE_COMMAND}" "-DPROFILE=${profile}" "-DDOC=${doc}"
    "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    PARENT_SCOPE)
endfunction()

# expect_code(<code> <profile> [<doc>]): the validator fails with <code>. The
# document defaults to an unmodified copy.
function(expect_code code profile)
  if(ARGC GREATER 2)
    set(doc "${ARGV2}")
  else()
    doc_copy(doc unchanged_${code})
  endif()
  check_command(cmd "${profile}" "${doc}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "profile: ${code}")
endfunction()

if(CASE STREQUAL "copy")
  profile_copy(p copy)
  doc_copy(d copy)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v1, 11 timing constants, 9 unmodeled\\)")

elseif(CASE STREQUAL "unmodeled_missing")
  # Remove each of the nine required entries in turn from a fresh copy; each
  # removal names its own id.
  file(READ "${SRC}/profiles/agb.json" real)
  string(JSON n LENGTH "${real}" unmodeled)
  if(NOT n EQUAL 9)
    message(FATAL_ERROR "profile-case: expected 9 unmodeled entries, found ${n}")
  endif()
  math(EXPR last "${n}-1")
  foreach(i RANGE ${last})
    string(JSON id GET "${real}" unmodeled ${i} id)
    profile_copy(p missing${i})
    gbl_json_mutate_remove("${p}" unmodeled ${i})
    expect_code("unmodeled-missing: ${id}" "${p}")
  endforeach()

else()
  message(FATAL_ERROR "profile-case: unknown case: ${CASE}")
endif()
