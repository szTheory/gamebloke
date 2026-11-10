# Vocabulary-contract negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/contract_case.cmake
#
# Every input is a mutated COPY of contracts/evidence.json under
# BIN/nc/contract_<case>; the committed file is never modified and no bad
# example is committed (D-04). `copy` proves the unmutated copy is accepted, so
# every negative case fails only for its planted reason. Each negative variant
# asserts a non-zero exit AND the validator's stable reason code through
# cmake/ExpectFail.cmake; a stale mutation fails with `mutate: no-match`.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "contract-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "contract-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/contract_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(CHECK "${SRC}/cmake/CheckContract.cmake")

# vocab_copy(<out-var> <variant>): a fresh sandbox copy of the real vocabulary.
function(vocab_copy out variant)
  set(path "${dir}/evidence_${variant}.json")
  file(COPY_FILE "${SRC}/contracts/evidence.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# expect_code(<code> <file>): the real validator on <file> fails with <code>.
function(expect_code code file)
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DVOCAB=${file}" -P "${CHECK}"
    REGEX "contract: ${code}")
endfunction()

if(CASE STREQUAL "copy")
  vocab_copy(f copy)
  gbl_expect_pass(
    COMMAND "${CMAKE_COMMAND}" "-DVOCAB=${f}" -P "${CHECK}"
    REGEX "contract: ok \\(9 states")

elseif(CASE STREQUAL "state_set")
  # A missing name, a tenth name, and a case variant all break the pinned set.
  vocab_copy(f missing)
  gbl_text_mutate("${f}"
    "    {\"name\": \"GUEST_STOP\", \"ran\": \"true\", \"digest\": \"required\", \"reason\": \"optional\"},\n" "")
  expect_code(state-set "${f}")

  vocab_copy(f tenth)
  gbl_text_mutate("${f}" "\"states\": [" "\"states\": [{\"name\": \"DONE\", \"ran\": \"true\", \"digest\": \"required\", \"reason\": \"optional\"},")
  expect_code(state-set "${f}")

  vocab_copy(f case)
  gbl_text_mutate("${f}" "{\"name\": \"PASS\"" "{\"name\": \"pass\"")
  expect_code(state-set "${f}")

else()
  message(FATAL_ERROR "contract-case: unknown case: ${CASE}")
endif()
