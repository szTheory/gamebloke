# Mutation controls for the workflow gates (D-03, D-10, D-11).
#   cmake -DCASE=<name> -DSRC=<source root> -DBIN=<build root> -P tests/ci/workflow_cases.cmake
#
# Every case reads the REAL .github/workflows/ci.yml, applies one
# first-occurrence replacement, writes the result to <BIN>/nc/<case>/ci.yml,
# and runs the case's gate on that copy through gbl_expect_fail with the
# reason it must give. No fixture workflow is committed, so a control cannot
# drift from the workflow it guards. An anchor that no longer occurs in
# ci.yml stops the case with `mutate: no-match`, so a stale control cannot
# silently test an unmutated file.
#
# Cases:
#   pins:     pins_unpinned pins_no_version_comment      (scripts/check-action-pins.sh)
#   workflow: clean (must pass) and the policy mutations (scripts/check-workflow.sh)
#
# Reason codes this file prints:
#   mutate: no-match
#   workflow-cases: ...
if(NOT DEFINED CASE OR NOT DEFINED SRC OR NOT DEFINED BIN)
  message(FATAL_ERROR "workflow-cases: -DCASE=, -DSRC= and -DBIN= are required")
endif()
include("${SRC}/cmake/ExpectFail.cmake")

set(workflow "${SRC}/.github/workflows/ci.yml")
if(NOT EXISTS "${workflow}")
  message(FATAL_ERROR "workflow-cases: missing ${workflow}")
endif()
file(READ "${workflow}" text)

# First-occurrence replacement. Sets <out> in the caller's scope.
function(gbl__mutate out text anchor replacement)
  string(FIND "${text}" "${anchor}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "mutate: no-match: anchor not found in ci.yml: ${anchor}")
  endif()
  string(LENGTH "${anchor}" alen)
  string(SUBSTRING "${text}" 0 ${pos} head)
  math(EXPR tail_start "${pos} + ${alen}")
  string(SUBSTRING "${text}" ${tail_start} -1 tail)
  set(${out} "${head}${replacement}${tail}" PARENT_SCOPE)
endfunction()

# Per case: gate (pins|workflow), anchor, repl, regex. The clean case has no anchor.
set(gate "workflow")
set(anchor "")
set(repl "")
set(regex "")

if(CASE STREQUAL "pins_unpinned")
  set(gate "pins")
  set(anchor "@3d3c42e5aac5ba805825da76410c181273ba90b1")
  set(repl "@v7")
  set(regex "pins: unpinned")
elseif(CASE STREQUAL "pins_no_version_comment")
  set(gate "pins")
  set(anchor " # v7.0.1")
  set(repl "")
  set(regex "pins: no-version-comment")
else()
  message(FATAL_ERROR "workflow-cases: unknown CASE '${CASE}'")
endif()

set(dir "${BIN}/nc/${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(copy "${dir}/ci.yml")

if(CASE STREQUAL "clean")
  file(WRITE "${copy}" "${text}")
else()
  gbl__mutate(mutated "${text}" "${anchor}" "${repl}")
  if("${mutated}" STREQUAL "${text}")
    message(FATAL_ERROR "mutate: no-match: replacement left ci.yml unchanged (${CASE})")
  endif()
  file(WRITE "${copy}" "${mutated}")
endif()

if(gate STREQUAL "pins")
  set(gate_cmd sh "${SRC}/scripts/check-action-pins.sh" "${copy}")
else()
  set(gate_cmd sh "${SRC}/scripts/check-workflow.sh" "${copy}")
endif()

if(CASE STREQUAL "clean")
  gbl_expect_pass(COMMAND ${gate_cmd} REGEX "workflow: ok \\(" WORKING_DIRECTORY "${SRC}")
else()
  gbl_expect_fail(COMMAND ${gate_cmd} REGEX "${regex}" WORKING_DIRECTORY "${SRC}")
endif()
