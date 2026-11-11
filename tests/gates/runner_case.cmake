cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "runner-case: usage: -D${required}= is required")
  endif()
endforeach()
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)

set(control "${BIN}/gamebloke-run-control")
set(production "${BIN}/gamebloke-run")
set(unit_test "${BIN}/runner_test")
set(fixtures "${BIN}/generated/fixtures")
set(scratch "${BIN}/runner-cases")
foreach(path IN ITEMS "${control}" "${production}" "${unit_test}"
    "${fixtures}/owned-arm-pass.gba" "${fixtures}/owned-arm-fail.gba"
    "${fixtures}/owned-arm-partial.gba" "${fixtures}/owned-arm-pass-mutated.gba"
    "${fixtures}/owned-arm-loop.gba" "${fixtures}/owned-arm-swi.gba"
    "${fixtures}/owned-arm-undefined.gba"
    "${fixtures}/owned-arm-coprocessor.gba")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "runner-case: required generated input is missing")
  endif()
endforeach()

file(MAKE_DIRECTORY "${scratch}")
execute_process(
  COMMAND "${unit_test}" prepare "${scratch}/refusals"
  RESULT_VARIABLE prepare_result
  OUTPUT_VARIABLE prepare_stdout
  ERROR_VARIABLE prepare_stderr)
if(NOT "${prepare_result}" STREQUAL "0" OR
    NOT EXISTS "${scratch}/refusals/empty.gba" OR
    NOT EXISTS "${scratch}/refusals/short.gba" OR
    NOT EXISTS "${scratch}/refusals/odd.gba" OR
    NOT EXISTS "${scratch}/refusals/oversized.gba")
  message(FATAL_ERROR "runner-case: refusal-input-generation-failed")
endif()

function(expect_json_field json expected)
  string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "runner-case: expected ${ARGN}=${expected}; got ${actual}; ${json_error}")
  endif()
endfunction()

function(expect_json_boolean json expected)
  string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR
      "runner-case: could not read boolean ${ARGN}; ${json_error}")
  endif()
  if("${expected}" STREQUAL "true")
    if(NOT actual)
      message(FATAL_ERROR "runner-case: expected ${ARGN}=true; got false")
    endif()
  elseif("${expected}" STREQUAL "false")
    if(actual)
      message(FATAL_ERROR "runner-case: expected ${ARGN}=false; got true")
    endif()
  else()
    message(FATAL_ERROR "runner-case: invalid boolean expectation")
  endif()
endfunction()

function(run_json output_variable name cli expected_exit expected_status
    expected_reason)
  execute_process(
    COMMAND "${cli}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "${expected_exit}")
    message(FATAL_ERROR
      "runner-case: ${name} exit ${result}, expected ${expected_exit}; stderr=${error}")
  endif()
  string(JSON actual_status ERROR_VARIABLE json_error GET "${output}" status)
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual_status}" STREQUAL "${expected_status}")
    message(FATAL_ERROR
      "runner-case: ${name} status ${actual_status}, expected ${expected_status}; ${json_error}; stdout=${output}")
  endif()
  string(JSON actual_reason ERROR_VARIABLE reason_error GET "${output}" reason)
  if("${expected_reason}" STREQUAL "READ_ERROR")
    if(NOT "${actual_reason}" MATCHES "^rom-(read|open)-failed$")
      message(FATAL_ERROR
        "runner-case: ${name} reason ${actual_reason}, expected a bounded read/open error")
    endif()
  elseif(NOT "${reason_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual_reason}" STREQUAL "${expected_reason}")
    message(FATAL_ERROR
      "runner-case: ${name} reason ${actual_reason}, expected ${expected_reason}; ${reason_error}")
  endif()
  string(REGEX MATCH "^[{][^\n]*[}]\n$" one_json_line "${output}")
  if(NOT "${one_json_line}" STREQUAL "${output}")
    message(FATAL_ERROR "runner-case: ${name} did not write exactly one JSON line")
  endif()
  string(FIND "${output}" "${SRC}" source_path_offset)
  string(FIND "${output}" "${BIN}" build_path_offset)
  if(NOT source_path_offset EQUAL -1 OR NOT build_path_offset EQUAL -1)
    message(FATAL_ERROR "runner-case: ${name} exposed a host path")
  endif()
  set(${output_variable} "${output}" PARENT_SCOPE)
endfunction()

function(expect_usage name cli)
  execute_process(
    COMMAND "${cli}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "2" OR NOT "${output}" STREQUAL "" OR
      NOT "${error}" MATCHES "^usage: gamebloke-run")
    message(FATAL_ERROR
      "runner-case: ${name} was not rejected as a usage error; exit=${result}; stdout=${output}; stderr=${error}")
  endif()
endfunction()

function(check_result_json name document manifest expected)
  set(path "${scratch}/${name}.json")
  file(WRITE "${path}" "${document}\n")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DRESULT=${path}" "-DMANIFEST=${manifest}"
      "-DREGISTRY=${SRC}/registry/disputes.json"
      "-DVOCAB=${SRC}/contracts/evidence.json"
      -P "${SRC}/cmake/CheckResult.cmake"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  set(report "${output}\n${error}")
  if(expected STREQUAL "ok")
    if(NOT "${result}" STREQUAL "0" OR
        NOT "${report}" MATCHES "result: ok ")
      message(FATAL_ERROR
        "runner-case: ${name} should validate; exit=${result}; output=${report}")
    endif()
  elseif("${result}" STREQUAL "0" OR
      NOT "${report}" MATCHES "result: ${expected}(:|[ \n])")
    message(FATAL_ERROR
      "runner-case: ${name} should fail with ${expected}; exit=${result}; output=${report}")
  endif()
endfunction()

function(expect_result_rejection name document reason)
  check_result_json("producer-${name}" "${document}"
    "${BIN}/generated/control/manifest.json" "${reason}")
endfunction()

function(expect_result_acceptance name document)
  check_result_json("producer-${name}" "${document}"
    "${BIN}/generated/control/manifest.json" "ok")
endfunction()

# Unknown test selectors must fail closed; no accidental pass-through path.
execute_process(
  COMMAND "${unit_test}" unknown-selector
  RESULT_VARIABLE selector_result
  OUTPUT_VARIABLE selector_stdout
  ERROR_VARIABLE selector_stderr)
if(NOT "${selector_result}" STREQUAL "2" OR
    NOT "${selector_stdout}" STREQUAL "" OR
    NOT "${selector_stderr}" MATCHES "^usage: runner_test")
  message(FATAL_ERROR "runner-case: runner_test accepted an unknown selector")
endif()

set(pass_rom "${fixtures}/owned-arm-pass.gba")
set(fail_rom "${fixtures}/owned-arm-fail.gba")
set(partial_rom "${fixtures}/owned-arm-partial.gba")
set(mutated_rom "${fixtures}/owned-arm-pass-mutated.gba")
set(refusal_dir "${scratch}/refusals")

run_json(pass_json registered-pass "${control}" 0 PASS guest-mailbox-pass
  --rom "${pass_rom}" --fixture synthetic-control-pass --cycles 1000)
expect_json_field("${pass_json}" pass semantic state)
expect_json_field("${pass_json}" 36865 semantic case_id)
expect_json_field("${pass_json}" 1000 time budget_cycles)
string(JSON pass_cycles GET "${pass_json}" time cycles)
if(pass_cycles GREATER 1000)
  message(FATAL_ERROR "runner-case: PASS exceeded its cycle budget")
endif()
string(JSON pass_dispute_count LENGTH "${pass_json}" disputes)
string(JSON pass_dispute GET "${pass_json}" disputes 0)
string(JSON pass_label_count LENGTH "${pass_json}" labels)
string(JSON pass_label GET "${pass_json}" labels 0)
if(NOT pass_dispute_count EQUAL 1 OR
    NOT pass_dispute STREQUAL "disp-gbatek-errata" OR
    NOT pass_label_count EQUAL 1 OR NOT pass_label STREQUAL "disputed")
  message(FATAL_ERROR
    "runner-case: actual disputed producer output lost its exact dispute disclosure")
endif()
expect_result_acceptance("registered-disputed-pass" "${pass_json}")

# The same actual bytes are intentionally absent from the production manifest. The
# generated row is only an artificial local validator input, not a fixture or rights grant.
check_result_json("synthetic-row-rejected-by-production-authority" "${pass_json}"
  "${SRC}/tests/fixtures/manifest.json" "unknown-fixture")

# These copies come from actual CLI output, not a test-authored result document. Every
# mutation must be rejected for the contract defect it introduces.
string(JSON pending_pass SET "${pass_json}" semantic state "\"pending\"")
expect_result_rejection("pass-with-pending-mailbox" "${pending_pass}"
  "status-semantic-mismatch")
string(JSON no_semantic REMOVE "${pass_json}" semantic)
expect_result_rejection("missing-semantic" "${no_semantic}" "missing-field")
string(JSON over_budget SET "${pass_json}" time cycles "\"1001\"")
expect_result_rejection("cycles-over-budget" "${over_budget}"
  "cycles-exceed-budget")
string(JSON noncanonical_cycles SET "${pass_json}" time cycles "\"01\"")
expect_result_rejection("noncanonical-cycle-string" "${noncanonical_cycles}"
  "bad-time")
string(JSON overflow_cycles SET "${pass_json}" time cycles
  "\"18446744073709551616\"")
expect_result_rejection("uint64-cycle-overflow" "${overflow_cycles}"
  "time-overflow")
string(JSON missing_cycles REMOVE "${pass_json}" time cycles)
expect_result_rejection("missing-cycle-count" "${missing_cycles}" "missing-field")

string(JSON missing_dispute REMOVE "${pass_json}" disputes)
expect_result_rejection("dispute-id-removed" "${missing_dispute}"
  "undisclosed-dispute")
string(JSON wrong_dispute_label SET "${pass_json}" labels "[]")
expect_result_rejection("disputed-label-removed" "${wrong_dispute_label}"
  "undisclosed-dispute")

run_json(fail_json registered-failure "${control}" 1 GUEST_FAIL
  guest-mailbox-fail --rom "${fail_rom}" --fixture synthetic-control-fail
  --cycles 1000)
expect_json_field("${fail_json}" fail semantic state)
expect_json_field("${fail_json}" 36866 semantic case_id)
string(JSON wrong_failure_case_id SET "${fail_json}" semantic case_id "36866")
expect_result_rejection("failure-case-id-number" "${wrong_failure_case_id}"
  "bad-semantic")
expect_result_acceptance("registered-disputed-failure" "${fail_json}")

run_json(skip_json bios-required "${control}" 1 SKIPPED
  required-bios-unavailable --rom "${pass_rom}"
  --fixture synthetic-control-bios-required --cycles 1000)
expect_json_boolean("${skip_json}" false fixture ran)
expect_json_field("${skip_json}" 0 time cycles)

run_json(raw_json unregistered-raw-pass "${production}" 1 GUEST_STOP
  unregistered-content --rom "${pass_rom}" --cycles 1000)
expect_json_field("${raw_json}" pass semantic state)
expect_json_field("${raw_json}" 36865 semantic case_id)

run_json(raw_fail_json unregistered-raw-failure "${production}" 1 GUEST_STOP
  unregistered-content --rom "${fail_rom}" --cycles 1000)
expect_json_field("${raw_fail_json}" fail semantic state)
expect_json_field("${raw_fail_json}" 36866 semantic case_id)
string(JSON raw_claimed_pass SET "${raw_json}" status "\"PASS\"")
expect_result_rejection("diagnostic-cannot-become-pass" "${raw_claimed_pass}"
  "bad-evidence")
check_result_json("raw-diagnostic-rejected" "${raw_json}"
  "${SRC}/tests/fixtures/manifest.json" "bad-evidence")

run_json(zero_json zero-cycle-budget "${control}" 1 TIMEOUT cycle-budget
  --rom "${pass_rom}" --fixture synthetic-control-pass --cycles 0)
expect_json_field("${zero_json}" 0 time cycles)
expect_json_field("${zero_json}" 0 time instructions)
expect_json_field("${zero_json}" 0 time budget_cycles)
expect_json_field("${zero_json}" absent semantic state)

run_json(frame_zero_json zero-frame-budget "${control}" 1 TIMEOUT cycle-budget
  --rom "${pass_rom}" --fixture synthetic-control-pass --frames 0)
expect_json_field("${frame_zero_json}" 0 time budget_cycles)
run_json(frame_one_json one-frame-budget "${control}" 0 PASS guest-mailbox-pass
  --rom "${pass_rom}" --fixture synthetic-control-pass --frames 1)
expect_json_field("${frame_one_json}" 280896 time budget_cycles)

set(exact_cycles "${pass_cycles}")
run_json(exact_json exact-cycle-budget "${control}" 0 PASS guest-mailbox-pass
  --rom "${pass_rom}" --fixture synthetic-control-pass --cycles "${exact_cycles}")
expect_json_field("${exact_json}" "${exact_cycles}" time cycles)
math(EXPR short_cycles "${exact_cycles} - 1")
run_json(short_json one-short-cycle-budget "${control}" 1 TIMEOUT cycle-budget
  --rom "${pass_rom}" --fixture synthetic-control-pass --cycles "${short_cycles}")
string(JSON short_actual_cycles GET "${short_json}" time cycles)
if(short_actual_cycles GREATER short_cycles)
  message(FATAL_ERROR "runner-case: one-short budget overshot its cycle cap")
endif()

run_json(missing_json missing-rom "${production}" 1 MISSING rom-file-not-found
  --rom "${scratch}/missing.gba" --cycles 0)
expect_json_boolean("${missing_json}" false fixture ran)
run_json(read_error_json directory-read-error "${production}" 1 HARNESS_FAIL
  READ_ERROR --rom "${scratch}" --cycles 0)

run_json(empty_json empty-rom "${production}" 1 REFUSED rom-empty
  --rom "${refusal_dir}/empty.gba" --cycles 0)
run_json(short_json short-rom "${production}" 1 REFUSED rom-too-short
  --rom "${refusal_dir}/short.gba" --cycles 0)
run_json(odd_json odd-rom "${production}" 1 REFUSED rom-odd-length
  --rom "${refusal_dir}/odd.gba" --cycles 0)
run_json(oversized_json overlarge-rom "${production}" 1 REFUSED rom-too-large
  --rom "${refusal_dir}/oversized.gba" --cycles 0)
run_json(digest_json digest-mismatch "${control}" 1 REFUSED
  fixture-digest-mismatch --rom "${mutated_rom}"
  --fixture synthetic-control-pass --cycles 1000)
expect_json_boolean("${digest_json}" false fixture ran)
run_json(unknown_fixture_json unknown-fixture "${control}" 1 REFUSED
  fixture-unknown --rom "${pass_rom}" --fixture no-such-control --cycles 0)

run_json(partial_json partial-mailbox "${production}" 1 TIMEOUT cycle-budget
  --rom "${partial_rom}" --cycles 1000)
expect_json_field("${partial_json}" pending semantic state)
expect_json_field("${partial_json}" 36867 semantic case_id)

foreach(exception IN ITEMS swi undefined coprocessor)
  if(exception STREQUAL "swi")
    set(exception_reason "no-bios-swi")
  elseif(exception STREQUAL "undefined")
    set(exception_reason "no-bios-undefined")
  else()
    set(exception_reason "no-bios-coprocessor")
  endif()
  run_json("${exception}_json" "no-bios-${exception}" "${production}" 1
    GUEST_STOP "${exception_reason}" --rom "${fixtures}/owned-arm-${exception}.gba"
    --cycles 1000)
endforeach()

run_json(trace_full_json trace-output-full "${control}" 1 HARNESS_FAIL
  trace-output-full --rom "${pass_rom}" --fixture synthetic-control-pass
  --cycles 1000 --trace-mask 1 --trace-capacity 1)
string(JSON trace_complete GET "${trace_full_json}" trace complete)
expect_json_boolean("${trace_full_json}" false trace complete)
expect_json_boolean("${trace_full_json}" false trace emitted)
expect_result_acceptance("trace-output-full-harness-failure" "${trace_full_json}")
string(JSON trace_full_as_pass SET "${trace_full_json}" status "\"PASS\"")
expect_result_rejection("trace-full-cannot-pass" "${trace_full_as_pass}"
  "incomplete-trace")

set(trace_one "${scratch}/trace-one.jsonl")
set(trace_two "${scratch}/trace-two.jsonl")
set(trace_existing "${scratch}/trace-existing.jsonl")
file(REMOVE "${trace_one}" "${trace_two}")
file(WRITE "${trace_existing}" "existing")
run_json(trace_write_fail_json trace-write-failure "${control}" 1
  HARNESS_FAIL trace-write-failed --rom "${pass_rom}"
  --fixture synthetic-control-pass --cycles 1000 --trace-mask 1
  --trace-capacity 65536 --trace-output "${trace_existing}")
expect_json_field("${trace_write_fail_json}" pass semantic state)
expect_json_field("${trace_write_fail_json}" 36865 semantic case_id)

run_json(trace_one_json trace-output-one "${control}" 0 PASS
  guest-mailbox-pass --rom "${pass_rom}" --fixture synthetic-control-pass
  --cycles 1000 --trace-mask 1 --trace-capacity 65536 --trace-output "${trace_one}")
run_json(trace_two_json trace-output-two "${control}" 0 PASS
  guest-mailbox-pass --rom "${pass_rom}" --fixture synthetic-control-pass
  --cycles 1000 --trace-mask 1 --trace-capacity 65536 --trace-output "${trace_two}")
expect_json_boolean("${trace_one_json}" true trace emitted)
expect_json_boolean("${trace_one_json}" true trace complete)
file(READ "${trace_one}" trace_one_text)
file(READ "${trace_two}" trace_two_text)
if("${trace_one_text}" STREQUAL "" OR
    NOT "${trace_one_text}" STREQUAL "${trace_two_text}" OR
    NOT "${trace_one_text}" MATCHES "^\{.*\}\n")
  message(FATAL_ERROR "runner-case: selected trace JSONL is empty or nondeterministic")
endif()

expect_usage(uint64-overflow "${control}" --rom "${pass_rom}" --cycles
  18446744073709551616)
expect_usage(signed-cycle-bound "${control}" --rom "${pass_rom}" --cycles +1)
expect_usage(negative-cycle-bound "${control}" --rom "${pass_rom}" --cycles -1)
expect_usage(fractional-cycle-bound "${control}" --rom "${pass_rom}" --cycles 1.5)
expect_usage(trailing-cycle-bound "${control}" --rom "${pass_rom}" --cycles 1x)
expect_usage(empty-cycle-bound "${control}" --rom "${pass_rom}" --cycles "")
expect_usage(duplicate-cycle-bound "${control}" --rom "${pass_rom}"
  --cycles 1 --cycles 2)
expect_usage(mutually-exclusive-bounds "${control}" --rom "${pass_rom}"
  --cycles 1 --frames 1)
expect_usage(duplicate-rom-option "${control}" --rom "${pass_rom}" --rom
  "${pass_rom}" --cycles 1)
expect_usage(frames-product-overflow "${control}" --rom "${pass_rom}"
  --frames 65671081374280)
expect_usage(trace-capacity-overflow "${control}" --rom "${pass_rom}"
  --cycles 1 --trace-capacity 65537)

message("runner-case: actual CLI status, bounds, trace and refusal matrix passed")
