# Case driver for the CI routing scripts.
#   cmake -DCASE=needs|compiler_id_guard -DSRC=<source root> [-DBIN=<build root>] -P tests/ci/ci_cases.cmake
#
# needs: drives cmake/CiRequired.cmake over a case table. Each element is
#   <pass|fail> <HEAVY or -> <EVENT> <compact NEEDS json> [<reason regex>]
# `-` stands for an empty HEAVY. The JSON has no spaces so each element splits
# on spaces. A `fail` case must exit non-zero, print `ci-required:`, and, when a
# reason is given, print it too (`&` separates several required fragments; `\ `
# is a space inside one).
#
# compiler_id_guard: configuring with GBL_EXPECT_C_ID=NoSuchCompiler must fail
# with `gamebloke: compiler-id-mismatch`, so the identity guard cannot rot into
# a no-op. Needs -DBIN=<build root>; works in a sandbox under it.
#
# Reason codes this file prints:
#   needs-case: FAIL <n>: <line>
#   needs-cases: <n> ok
# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED CASE OR NOT DEFINED SRC)
  message(FATAL_ERROR "ci-cases: -DCASE= and -DSRC= are required")
endif()

function(check_diagnostic_groups text)
  include("${SRC}/cmake/CiPreflight.cmake")
  string(REGEX MATCHALL "- runner: [^\n]+\n            host: [^\n]+\n            group: [^\n]+\n            cc: [^\n]+\n            id: [^\n]+" rows "${text}")
  list(SORT rows)
  set(expected "")
  foreach(group IN ITEMS arm thumb)
    list(APPEND expected
      "- runner: ubuntu-26.04\n            host: linux-x64\n            group: ${group}\n            cc: gcc\n            id: GNU"
      "- runner: macos-26\n            host: darwin-arm64\n            group: ${group}\n            cc: cc\n            id: AppleClang")
  endforeach()
  list(SORT expected)
  if(NOT rows STREQUAL expected)
    message(FATAL_ERROR "ci-routing: diagnostic-group-matrix-mismatch")
  endif()
  foreach(required IN ITEMS
      [=[name: diagnostics (${{ matrix.host }}, ${{ matrix.group }})]=]
      "if: matrix.group == 'arm'\n        run: cmake --workflow --preset diagnostics-arm"
      "if: matrix.group == 'thumb'\n        run: cmake --workflow --preset diagnostics-thumb")
    string(FIND "${text}" "${required}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "ci-routing: diagnostic-group-command-mismatch")
    endif()
  endforeach()
endfunction()

if(CASE STREQUAL "diagnostic-route-control")
  file(READ "${WORKFLOW}" text)
  check_diagnostic_groups("${text}")
  return()
endif()

function(check_diagnostics_route source_root)
  include("${source_root}/cmake/CiPreflight.cmake")
  gbl_ci_workflow("${source_root}" "${source_root}/.github/workflows/ci.yml")
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}"
    "-DBIN=${source_root}/build/phase05-spike/ci-preflight-controls"
    -P "${source_root}/tests/ci/preflight_cases.cmake"
    RESULT_VARIABLE preflight_rc OUTPUT_VARIABLE preflight_out ERROR_VARIABLE preflight_err)
  if(NOT preflight_rc STREQUAL "0")
    message(FATAL_ERROR "ci-routing: budget-preflight-controls-failed:${preflight_out}${preflight_err}")
  endif()
  set(workflow_file "${source_root}/.github/workflows/ci.yml")
  if(NOT EXISTS "${workflow_file}")
    message(FATAL_ERROR "ci-routing: ci-workflow-missing")
  endif()
  file(READ "${workflow_file}" workflow_text)
  string(FIND "${workflow_text}" "\n  diagnostics:\n" diagnostics_start)
  if(diagnostics_start EQUAL -1)
    message(FATAL_ERROR "ci-routing: required-diagnostics-job-missing")
  endif()
  set(required_needs "needs: [changes, hygiene, title, build, asan, diagnostics]")
  string(FIND "${workflow_text}" "${required_needs}" needs_position)
  if(needs_position EQUAL -1)
    message(FATAL_ERROR "ci-routing: diagnostics-job-not-in-required-needs")
  endif()

  string(FIND "${workflow_text}" "\n  ci-required:\n" required_start)
  if(required_start EQUAL -1 OR required_start LESS diagnostics_start)
    message(FATAL_ERROR "ci-routing: required-check-job-missing-or-misordered")
  endif()
  math(EXPR diagnostics_length "${required_start} - ${diagnostics_start}")
  string(SUBSTRING "${workflow_text}" ${diagnostics_start}
    ${diagnostics_length} diagnostics_block)
  check_diagnostic_groups("${diagnostics_block}")
  foreach(expected IN ITEMS
      "runner: ubuntu-26.04" "host: linux-x64"
      "runner: macos-26" "host: darwin-arm64"
      [=[timeout-minutes: ${{ matrix.minutes }}]=] "fetch-depth: 0"
      "persist-credentials: false"
      [=[ref: ${{ github.event.pull_request.head.sha || github.sha }}]=]
      "sh scripts/fetch-toolchain.sh"
      "cmake --workflow --preset diagnostics-arm"
      "cmake --workflow --preset diagnostics-thumb")
    string(FIND "${diagnostics_block}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "ci-routing: diagnostics-job-missing-${expected}")
    endif()
  endforeach()
  if(diagnostics_block MATCHES "actions/upload-artifact@")
    message(FATAL_ERROR "ci-routing: diagnostic-artifact-upload-forbidden")
  endif()

  # The static checker must catch a diagnostic job that is removed from the
  # required needs list, even though no job result then reaches CiRequired.cmake.
  set(needs_control_root "${source_root}/build/phase05-spike/ci-needs-control")
  file(MAKE_DIRECTORY "${needs_control_root}")
  string(REPLACE "${required_needs}"
    "needs: [changes, hygiene, title, build, asan]" missing_diagnostic_need
    "${workflow_text}")
  if(missing_diagnostic_need STREQUAL workflow_text)
    message(FATAL_ERROR "ci-routing: needs-negative-control-no-match")
  endif()
  set(control_workflow "${needs_control_root}/ci.yml")
  foreach(group IN ITEMS wrong thumb)
    string(REPLACE "group: arm" "group: ${group}" mutation "${diagnostics_block}")
    file(WRITE "${control_workflow}" "${mutation}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -DCASE=diagnostic-route-control
      "-DSRC=${source_root}" "-DWORKFLOW=${control_workflow}" -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(rc STREQUAL "0" OR NOT "${output}${error}" MATCHES "diagnostic-group-matrix-mismatch")
      message(FATAL_ERROR "ci-routing: diagnostic-group-control-missed")
    endif()
  endforeach()
  file(WRITE "${control_workflow}" "${missing_diagnostic_need}")
  execute_process(
    COMMAND sh "${source_root}/scripts/check-workflow.sh" "${control_workflow}"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE control_rc OUTPUT_VARIABLE control_out
    ERROR_VARIABLE control_error)
  if("${control_rc}" STREQUAL "0" OR
     NOT "${control_out}${control_error}" MATCHES
       "workflow: needs-missing: diagnostics")
    message(FATAL_ERROR
      "ci-routing: missing-diagnostic-need-control-missed: ${control_error}${control_out}")
  endif()
  foreach(mutation IN ITEMS history upload)
    if(mutation STREQUAL "history")
      string(REPLACE "fetch-depth: 0" "fetch-depth: 1" changed "${workflow_text}")
      set(reason "workflow: full-history-required")
    else()
      string(REPLACE "path: build/ci/junit.xml" "path: build/**/*.gba" changed "${workflow_text}")
      set(reason "workflow: junit-only-upload")
    endif()
    file(WRITE "${control_workflow}" "${changed}")
    execute_process(COMMAND sh "${source_root}/scripts/check-workflow.sh" "${control_workflow}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(rc STREQUAL "0" OR NOT "${output}${error}" MATCHES "${reason}")
      message(FATAL_ERROR "ci-routing: missing-${mutation}-refusal:${output}${error}")
    endif()
  endforeach()
endfunction()

if(CASE STREQUAL "needs")
  set(cases
    # every job succeeded
    [=[pass true pull_request {"changes":{"result":"success","outputs":{"heavy":"true"}},"hygiene":{"result":"success","outputs":{}},"title":{"result":"success","outputs":{}},"build":{"result":"success","outputs":{}},"asan":{"result":"success","outputs":{}},"diagnostics":{"result":"success","outputs":{}}}]=]
    # planning-only: heavy lanes skipped on purpose
    [=[pass false pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"skipped"},"asan":{"result":"skipped"},"diagnostics":{"result":"skipped"}}]=]
    # a heavy job skipped while heavy=true must fail
    [=[fail true pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"skipped"},"asan":{"result":"success"}} not\ allowed:\ build=skipped]=]
    # diagnostics may skip only when the complete file listing makes the change planning-only
    [=[fail true pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"success"},"asan":{"result":"success"},"diagnostics":{"result":"skipped"}} diagnostics=skipped]=]
    # a failed or cancelled diagnostics matrix result is never allowed
    [=[fail true pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"success"},"asan":{"result":"success"},"diagnostics":{"result":"failure"}} diagnostics=failure]=]
    # a missing result from the required diagnostics job fails closed
    [=[fail true pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"success"},"asan":{"result":"success"},"diagnostics":{"outputs":{}}} diagnostics=missing]=]
    # changes failed: HEAVY is empty, so skipped heavy jobs fail too
    [=[fail - pull_request {"changes":{"result":"failure"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"skipped"},"asan":{"result":"skipped"}} changes=failure&build=skipped]=]
    # hygiene is always required
    [=[fail false pull_request {"changes":{"result":"success"},"hygiene":{"result":"skipped"},"title":{"result":"success"},"build":{"result":"skipped"},"asan":{"result":"skipped"}} hygiene=skipped]=]
    # title skipped is fine off pull requests, never on them
    [=[pass true push {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"skipped"},"build":{"result":"success"},"asan":{"result":"success"}}]=]
    [=[fail true pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"skipped"},"build":{"result":"success"},"asan":{"result":"success"}} title=skipped]=]
    [=[fail false pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"skipped"},"build":{"result":"skipped"},"asan":{"result":"skipped"}} title=skipped]=]
    # a failed or cancelled lane fails the check (matrix-style build result included)
    [=[fail true pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"failure"},"asan":{"result":"success"}} build=failure]=]
    [=[fail true pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"success"},"asan":{"result":"cancelled"}} asan=cancelled]=]
    # changes itself skipped or failed is never allowed
    [=[fail false pull_request {"changes":{"result":"skipped"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"skipped"},"asan":{"result":"skipped"}} changes=skipped]=]
    # heavy unset with every heavy job skipped fails closed
    [=[fail - push {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"skipped"},"build":{"result":"skipped"},"asan":{"result":"skipped"}} build=skipped]=]
    # an empty needs object
    [=[fail true pull_request {} needs\ is\ empty]=]
    # a job without a result member is not a success
    [=[fail true pull_request {"build":{"outputs":{}}} build=missing]=]
  )
  set(n 0)
  set(failures "")
  foreach(line IN LISTS cases)
    math(EXPR n "${n} + 1")
    if(NOT line MATCHES "^(pass|fail) ([^ ]+) ([^ ]+) ([^ ]+)( (.*))?$")
      message(FATAL_ERROR "needs-case: malformed case ${n}: ${line}")
    endif()
    set(want "${CMAKE_MATCH_1}")
    set(heavy "${CMAKE_MATCH_2}")
    set(event "${CMAKE_MATCH_3}")
    set(json "${CMAKE_MATCH_4}")
    set(reason "${CMAKE_MATCH_6}")
    if(heavy STREQUAL "-")
      set(heavy "")
    endif()
    string(REPLACE "\\ " " " reason "${reason}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env "NEEDS=${json}" "HEAVY=${heavy}" "EVENT=${event}"
        "${CMAKE_COMMAND}" -P "${SRC}/cmake/CiRequired.cmake"
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
    string(REGEX REPLACE "\n" " " flat "${out}")
    if(want STREQUAL "pass")
      if(NOT rc EQUAL 0 OR NOT flat MATCHES "ci-required: ok")
        list(APPEND failures "needs-case: FAIL ${n}: wanted pass, got exit ${rc}: ${line}\n${out}")
      endif()
    else()
      if(rc EQUAL 0)
        list(APPEND failures "needs-case: FAIL ${n}: wanted fail, got pass: ${line}")
      elseif(NOT flat MATCHES "ci-required:")
        list(APPEND failures "needs-case: FAIL ${n}: failed without a ci-required reason: ${line}\n${out}")
      else()
        string(REPLACE "&" ";" fragments "${reason}")
        foreach(fragment IN LISTS fragments)
          if(NOT flat MATCHES "${fragment}")
            list(APPEND failures "needs-case: FAIL ${n}: reason /${fragment}/ missing: ${line}\n${out}")
          endif()
        endforeach()
      endif()
    endif()
  endforeach()
  if(failures)
    foreach(f IN LISTS failures)
      message("${f}")
    endforeach()
    message(FATAL_ERROR "needs-case: FAIL (see above)")
  endif()
  check_diagnostics_route("${SRC}")
  message("needs-cases: ${n} ok")
elseif(CASE STREQUAL "compiler_id_guard")
  if(NOT DEFINED BIN)
    message(FATAL_ERROR "ci-cases: -DBIN= is required for compiler_id_guard")
  endif()
  include("${SRC}/cmake/ExpectFail.cmake")
  set(sandbox "${BIN}/nc/compiler_id_guard")
  file(REMOVE_RECURSE "${sandbox}")
  file(MAKE_DIRECTORY "${sandbox}")
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -E env GBL_EXPECT_C_ID=NoSuchCompiler
      "${CMAKE_COMMAND}" -S "${SRC}" -B "${sandbox}" -G Ninja -DGBL_LANE=hygiene
    REGEX "gamebloke: compiler-id-mismatch")
else()
  message(FATAL_ERROR "ci-cases: unknown CASE '${CASE}'")
endif()
