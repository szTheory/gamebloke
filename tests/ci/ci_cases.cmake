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

if(CASE STREQUAL "needs")
  set(cases
    # every job succeeded
    [=[pass true pull_request {"changes":{"result":"success","outputs":{"heavy":"true"}},"hygiene":{"result":"success","outputs":{}},"title":{"result":"success","outputs":{}},"build":{"result":"success","outputs":{}},"asan":{"result":"success","outputs":{}}}]=]
    # planning-only: heavy lanes skipped on purpose
    [=[pass false pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"skipped"},"asan":{"result":"skipped"}}]=]
    # a heavy job skipped while heavy=true must fail
    [=[fail true pull_request {"changes":{"result":"success"},"hygiene":{"result":"success"},"title":{"result":"success"},"build":{"result":"skipped"},"asan":{"result":"success"}} not\ allowed:\ build=skipped]=]
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
