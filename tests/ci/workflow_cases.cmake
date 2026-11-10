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
elseif(CASE STREQUAL "clean")
  set(gate "workflow")
elseif(CASE STREQUAL "moving_label")
  # The moving runner label is built from two fragments so this file never
  # carries the string the policy gate looks for.
  set(moving_a "ubuntu-")
  set(moving_b "latest")
  set(anchor "ubuntu-26.04")
  set(repl "${moving_a}${moving_b}")
  set(regex "workflow: moving-label")
elseif(CASE STREQUAL "path_filter")
  set(anchor "    branches: [main]")
  set(repl "${anchor}\n    paths: [\"src/**\"]")
  set(regex "workflow: path-filter")
elseif(CASE STREQUAL "pr_target")
  set(anchor "  pull_request:")
  set(repl "  pull_request_target:")
  set(regex "workflow: pr-target")
elseif(CASE STREQUAL "needs_missing")
  set(anchor "needs: [changes, hygiene, title, build, asan]")
  set(repl "needs: [changes, hygiene, title, build]")
  set(regex "workflow: needs-missing: asan")
elseif(CASE STREQUAL "expression_in_run")
  set(anchor "run: cmake --workflow --preset hygiene")
  set(repl [=[run: cmake --workflow --preset hygiene && echo "${{ github.head_ref }}"]=])
  set(regex "workflow: expression-in-run")
elseif(CASE STREQUAL "non_preset_run")
  set(anchor "cmake --workflow --preset ci")
  set(repl "cmake --preset ci && cmake --build --preset ci")
  set(regex "workflow: non-preset-run: build")
elseif(CASE STREQUAL "unknown_preset")
  set(anchor "--preset asan")
  set(repl "--preset nosuchlane")
  set(regex "workflow: unknown-preset: nosuchlane")
elseif(CASE STREQUAL "no_timeout")
  # Renaming the key removes the setting from the first job without depending
  # on the text after it.
  set(anchor "timeout-minutes:")
  set(repl "removed-timeout:")
  set(regex "workflow: no-timeout: changes")
elseif(CASE STREQUAL "persist_credentials")
  set(anchor "persist-credentials: false")
  set(repl "persist-credentials: true")
  set(regex "workflow: persist-credentials")
elseif(CASE STREQUAL "continue_on_error")
  set(anchor [=[    runs-on: ${{ matrix.runner }}]=])
  set(repl "${anchor}\n    continue-on-error: true")
  set(regex "workflow: continue-on-error")
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
