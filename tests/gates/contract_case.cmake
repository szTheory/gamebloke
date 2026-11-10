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

# expect_code(<code> <file> [<detail-regex>]): the real validator on <file> fails with
# <code>, and with the specific finding detail when one is given. findings are printed
# with message(), which is not wrapped, so the detail is matched as printed.
function(expect_code code file)
  set(detail "")
  if(NOT "${ARGV2}" STREQUAL "")
    set(detail ": ${ARGV2}")
  endif()
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DVOCAB=${file}" -P "${CHECK}"
    REGEX "contract: ${code}${detail}")
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

elseif(CASE STREQUAL "bad_schema")
  vocab_copy(f schema)
  gbl_json_mutate_set("${f}" "\"gamebloke.evidence/2\"" schema)
  expect_code(bad-schema "${f}")

elseif(CASE STREQUAL "missing_field")
  vocab_copy(f states)
  gbl_json_mutate_remove("${f}" states)
  expect_code(missing-field "${f}")
  vocab_copy(f claim_string)
  gbl_json_mutate_set("${f}" "\"not-an-array\"" claim_fields)
  expect_code(missing-field "${f}")

elseif(CASE STREQUAL "label_set")
  # Index 5 is one past the end of the five-member array: SET appends.
  vocab_copy(f extra)
  gbl_json_mutate_set("${f}" "\"calibrated:x\"" source_labels 5)
  expect_code(label-set "${f}")
  vocab_copy(f short)
  gbl_json_mutate_remove("${f}" source_labels 4)
  expect_code(label-set "${f}")

elseif(CASE STREQUAL "class_set")
  vocab_copy(f extra)
  gbl_json_mutate_set("${f}" "\"emulator-agreement\"" provenance_classes 5)
  expect_code(class-set "${f}")

elseif(CASE STREQUAL "claim_set")
  # Index 2 is bios_provider; index 6 is one past the end.
  vocab_copy(f short)
  gbl_json_mutate_remove("${f}" claim_fields 2)
  expect_code(claim-set "${f}")
  vocab_copy(f extra)
  gbl_json_mutate_set("${f}" "\"rom\"" claim_fields 6)
  expect_code(claim-set "${f}")

elseif(CASE STREQUAL "bad_state_rule")
  # states[0] is PASS, [6] MISSING, [7] SKIPPED.
  vocab_copy(f pass_ran)
  gbl_json_mutate_set("${f}" "\"either\"" states 0 ran)
  expect_code(bad-state-rule "${f}")
  vocab_copy(f skipped_reason)
  gbl_json_mutate_set("${f}" "\"optional\"" states 7 reason)
  expect_code(bad-state-rule "${f}")
  vocab_copy(f digest_value)
  gbl_json_mutate_set("${f}" "\"maybe\"" states 0 digest)
  expect_code(bad-state-rule "${f}")
  vocab_copy(f refusal_key)
  gbl_json_mutate_set("${f}" "\"required\"" states 6 refusal)
  expect_code(bad-state-rule "${f}")

elseif(CASE STREQUAL "weakened_state_rule")
  # Each variant weakens ONE locked rule to a value that is individually valid (so the
  # generic allowed-value check stays silent) and asserts the pinned-rule finding text.
  # states[0] PASS, [6] MISSING, [7] SKIPPED, [8] REFUSED.
  vocab_copy(f pass_digest)
  gbl_json_mutate_set("${f}" "\"optional\"" states 0 digest)
  expect_code(bad-state-rule "${f}" "PASS: digest must be required")
  vocab_copy(f pass_ran)
  gbl_json_mutate_set("${f}" "\"false\"" states 0 ran)
  expect_code(bad-state-rule "${f}" "PASS: ran must be true")
  vocab_copy(f missing_ran)
  gbl_json_mutate_set("${f}" "\"true\"" states 6 ran)
  expect_code(bad-state-rule "${f}" "MISSING: ran must be false")
  vocab_copy(f missing_reason)
  gbl_json_mutate_set("${f}" "\"optional\"" states 6 reason)
  expect_code(bad-state-rule "${f}" "MISSING: reason must be required")
  vocab_copy(f skipped_ran)
  gbl_json_mutate_set("${f}" "\"either\"" states 7 ran)
  expect_code(bad-state-rule "${f}" "SKIPPED: ran must be false")
  vocab_copy(f skipped_reason)
  gbl_json_mutate_set("${f}" "\"optional\"" states 7 reason)
  expect_code(bad-state-rule "${f}" "SKIPPED: reason must be required")
  vocab_copy(f refused_ran)
  gbl_json_mutate_set("${f}" "\"either\"" states 8 ran)
  expect_code(bad-state-rule "${f}" "REFUSED: ran must be false")
  vocab_copy(f refused_refusal_removed)
  gbl_json_mutate_remove("${f}" states 8 refusal)
  expect_code(bad-state-rule "${f}" "REFUSED: refusal must be required")

elseif(CASE STREQUAL "semicolon_item")
  # One JSON string that holds the whole pinned set joined by `;` would split into five
  # list elements and fake it; every vocabulary array and the state names reject it.
  vocab_copy(f labels)
  gbl_json_mutate_set("${f}" "[\"REF;SRC;IMPL;INF;UNK\"]" source_labels)
  expect_code(missing-field "${f}" "source_labels\\[0\\]: contains a semicolon")
  vocab_copy(f classes)
  gbl_json_mutate_set("${f}"
    "[\"project-authored;hardware-observed;reference-emulator;suite-self-checking;disputed\"]"
    provenance_classes)
  expect_code(missing-field "${f}" "provenance_classes\\[0\\]: contains a semicolon")
  vocab_copy(f claims)
  gbl_json_mutate_set("${f}"
    "[\"machine;boot;bios_provider;cart;input_timeline;evidence\"]" claim_fields)
  expect_code(missing-field "${f}" "claim_fields\\[0\\]: contains a semicolon")
  vocab_copy(f non_string)
  gbl_json_mutate_set("${f}" "7" source_labels 0)
  expect_code(missing-field "${f}" "source_labels\\[0\\]")
  vocab_copy(f state_name)
  gbl_json_mutate_set("${f}" "\"PASS;X\"" states 0 name)
  expect_code(bad-state-rule "${f}" "states\\[0\\]: name")

elseif(CASE STREQUAL "usage")
  gbl_expect_fail(COMMAND "${CMAKE_COMMAND}" -P "${CHECK}" REGEX "contract: usage")

else()
  message(FATAL_ERROR "contract-case: unknown case: ${CASE}")
endif()
