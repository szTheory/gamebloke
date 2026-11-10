# Ruleset validator controls and apply-script controls (D-03, D-13, W4).
#   cmake -DCASE=<name> -DSRC=<source root> -DBIN=<build root> -P tests/ci/ruleset_cases.cmake
#
# Every case builds its JSON at test time with string(JSON) from the committed
# .github/rulesets/main.json, under <BIN>/nc/<case> (rebuilt each run). Nothing
# under tests/ is a canned ruleset or a canned GitHub response.
#
# Cases:
#   valid      the committed ruleset passes the validator
#   bypass     a bypass actor is added           -> ruleset: bypass-actors
#   extra_rule a deletion rule is added          -> ruleset: unauthorized-rule: deletion
#   context    the required context is renamed   -> ruleset: required-check
#
# Reason codes this file prints:
#   ruleset-case: ...
if(NOT DEFINED CASE OR NOT DEFINED SRC OR NOT DEFINED BIN)
  message(FATAL_ERROR "ruleset-case: -DCASE=, -DSRC= and -DBIN= are required")
endif()
include("${SRC}/cmake/ExpectFail.cmake")

set(committed "${SRC}/.github/rulesets/main.json")
if(NOT EXISTS "${committed}")
  message(FATAL_ERROR "ruleset-case: missing ${committed}")
endif()
file(READ "${committed}" ruleset)

set(dir "${BIN}/nc/${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")

# Validator command for a ruleset file.
function(rs_validate_cmd out file)
  set(${out} "${CMAKE_COMMAND}" -DMODE=validate "-DFILE=${file}"
    -P "${SRC}/cmake/RulesetCheck.cmake" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "valid")
  rs_validate_cmd(cmd "${committed}")
  gbl_expect_pass(COMMAND ${cmd} REGEX "ruleset: ok")
elseif(CASE STREQUAL "bypass" OR CASE STREQUAL "extra_rule" OR CASE STREQUAL "context")
  if(CASE STREQUAL "bypass")
    string(JSON mutated SET "${ruleset}" bypass_actors 0
      "{\"actor_id\": 5, \"actor_type\": \"RepositoryRole\", \"bypass_mode\": \"always\"}")
    set(regex "ruleset: bypass-actors")
  elseif(CASE STREQUAL "extra_rule")
    string(JSON mutated SET "${ruleset}" rules 2 "{\"type\": \"deletion\"}")
    set(regex "ruleset: unauthorized-rule: deletion")
  else()
    string(JSON mutated SET "${ruleset}" rules 1 parameters required_status_checks 0
      context "\"CI optional\"")
    set(regex "ruleset: required-check")
  endif()
  if("${mutated}" STREQUAL "${ruleset}")
    message(FATAL_ERROR "ruleset-case: mutation left the ruleset unchanged (${CASE})")
  endif()
  file(WRITE "${dir}/main.json" "${mutated}")
  rs_validate_cmd(cmd "${dir}/main.json")
  gbl_expect_fail(COMMAND ${cmd} REGEX "${regex}")
else()
  message(FATAL_ERROR "ruleset-case: unknown CASE '${CASE}'")
endif()
