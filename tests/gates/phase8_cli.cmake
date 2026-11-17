cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase8-cli: usage: -D${required}= is required")
  endif()
endforeach()
get_filename_component(BIN "${BIN}" ABSOLUTE)
set(runner "${BIN}/gamebloke-run")
set(control "${BIN}/gamebloke-run-control")
set(fixture "${BIN}/generated/fixtures/owned-arm-pass.gba")
set(scratch "${BIN}/phase8-cli")
foreach(path IN ITEMS "${runner}" "${control}" "${fixture}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "phase8-cli: required-input-missing")
  endif()
endforeach()
file(MAKE_DIRECTORY "${scratch}")

function(expect_field json expected)
  string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "phase8-cli: expected ${ARGN}=${expected}; got ${actual}; ${json_error}")
  endif()
endfunction()

function(run_valid output_variable name)
  execute_process(
    COMMAND "${control}" --rom "${fixture}" --fixture synthetic-control-pass
      --cycles 1000 ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "0" OR NOT "${error}" STREQUAL "")
    message(FATAL_ERROR
      "phase8-cli: ${name} expected exit 0; exit=${result}; stderr=${error}")
  endif()
  expect_field("${output}" PASS status)
  expect_field("${output}" pass semantic state)
  expect_field("${output}" 36865 semantic case_id)
  set(${output_variable} "${output}" PARENT_SCOPE)
endfunction()

function(expect_usage_refusal name expected_reason script_path)
  set(trace_path "${scratch}/${name}.trace.jsonl")
  file(REMOVE "${trace_path}")
  execute_process(
    COMMAND "${control}" --rom "${fixture}"
      --fixture synthetic-control-pass --cycles 1000
      --trace-mask 1 --trace-capacity 32 --trace-output "${trace_path}"
      --input-script "${script_path}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  string(FIND "${error}" "gamebloke-run: ${expected_reason}" reason_offset)
  string(FIND "${error}" "${script_path}" path_offset)
  if(NOT "${result}" STREQUAL "2" OR NOT "${output}" STREQUAL "" OR
      reason_offset EQUAL -1 OR NOT path_offset EQUAL -1 OR
      EXISTS "${trace_path}")
    message(FATAL_ERROR
      "phase8-cli: ${name} expected usage refusal ${expected_reason} without guest/output effects; exit=${result}; stdout=${output}; stderr=${error}")
  endif()
endfunction()

execute_process(
  COMMAND "${runner}" --help
  RESULT_VARIABLE help_result
  OUTPUT_VARIABLE help_output
  ERROR_VARIABLE help_error)
if(NOT "${help_result}" STREQUAL "0" OR
    NOT "${help_output}" MATCHES "--input-script PATH" OR
    NOT "${help_error}" STREQUAL "")
  message(FATAL_ERROR "phase8-cli: --help does not document --input-script")
endif()

set(absent_output "")
execute_process(
  COMMAND "${control}" --rom "${fixture}"
    --fixture synthetic-control-pass --cycles 1000
  RESULT_VARIABLE absent_result
  OUTPUT_VARIABLE absent_output
  ERROR_VARIABLE absent_error)
if(NOT "${absent_result}" STREQUAL "0" OR
    NOT "${absent_error}" STREQUAL "")
  message(FATAL_ERROR "phase8-cli: absent-script run failed")
endif()
expect_field("${absent_output}" none claim input_timeline kind)
expect_field("${absent_output}" 1023 phase8 keypad keyinput)

set(empty_script "${scratch}/empty.txt")
file(WRITE "${empty_script}" "")
run_valid(empty_json empty-script --input-script "${empty_script}")
expect_field("${empty_json}" script claim input_timeline kind)
string(JSON empty_digest GET "${empty_json}" claim input_timeline sha256)
string(SHA256 expected_empty_digest "gamebloke-input-timeline-1")
if(NOT "${empty_digest}" STREQUAL "${expected_empty_digest}")
  message(FATAL_ERROR "phase8-cli: empty script digest is not canonical")
endif()

set(valid_script "${scratch}/cycle-zero.txt")
file(WRITE "${valid_script}" "0 1\n")
run_valid(valid_json valid-script --input-script "${valid_script}")
run_valid(replay_json valid-script-replay --input-script "${valid_script}")
expect_field("${valid_json}" script claim input_timeline kind)
expect_field("${valid_json}" 1022 phase8 keypad keyinput)
expect_field("${replay_json}" 1022 phase8 keypad keyinput)
string(JSON valid_digest GET "${valid_json}" claim input_timeline sha256)
string(JSON replay_digest GET "${replay_json}" claim input_timeline sha256)
string(LENGTH "${valid_digest}" valid_digest_length)
if(NOT "${valid_digest}" STREQUAL "${replay_digest}" OR
    NOT valid_digest_length EQUAL 64 OR
    NOT "${valid_digest}" MATCHES "^[0-9a-f]+$")
  message(FATAL_ERROR "phase8-cli: same script digest is not canonical/repeatable")
endif()

set(crlf_script "${scratch}/crlf.txt")
file(WRITE "${crlf_script}" "0 1\r\n")
run_valid(crlf_json crlf-script --input-script "${crlf_script}")
expect_field("${crlf_json}" 1022 phase8 keypad keyinput)
set(equal_cycle_script "${scratch}/equal-cycle.txt")
file(WRITE "${equal_cycle_script}" "0 1\n0 0\n")
run_valid(equal_json equal-cycle-script --input-script "${equal_cycle_script}")
expect_field("${equal_json}" 1023 phase8 keypad keyinput)

set(max_cycle_script "${scratch}/max-cycle.txt")
file(WRITE "${max_cycle_script}" "18446744073709551615 1023\n")
run_valid(max_cycle_json max-cycle --input-script "${max_cycle_script}")
expect_field("${max_cycle_json}" script claim input_timeline kind)

set(max_count_script "${scratch}/max-count.txt")
file(WRITE "${max_count_script}" "")
foreach(index RANGE 1 256)
  file(APPEND "${max_count_script}" "0 1\n")
endforeach()
run_valid(max_count_json max-count --input-script "${max_count_script}")
expect_field("${max_count_json}" 256 phase8 keypad applied_events)

set(missing_script "${scratch}/missing.txt")
execute_process(
  COMMAND "${control}" --rom "${fixture}"
    --fixture synthetic-control-pass --cycles 1000
    --input-script "${missing_script}"
  RESULT_VARIABLE missing_result
  OUTPUT_VARIABLE missing_output
  ERROR_VARIABLE missing_error)
if(NOT "${missing_result}" STREQUAL "1" OR
    NOT "${missing_error}" STREQUAL "")
  message(FATAL_ERROR "phase8-cli: missing-script I/O result was not serialized")
endif()
expect_field("${missing_output}" MISSING status)
expect_field("${missing_output}" input-script-file-not-found reason)
expect_field("${missing_output}" OFF fixture ran)
expect_field("${missing_output}" none claim input_timeline kind)

file(MAKE_DIRECTORY "${scratch}/read-error")
execute_process(
  COMMAND "${control}" --rom "${fixture}"
    --fixture synthetic-control-pass --cycles 1000
    --input-script "${scratch}/read-error"
  RESULT_VARIABLE read_result
  OUTPUT_VARIABLE read_output
  ERROR_VARIABLE read_error)
if(NOT "${read_result}" STREQUAL "1" OR
    NOT "${read_error}" STREQUAL "")
  message(FATAL_ERROR "phase8-cli: script read-error result was not serialized")
endif()
expect_field("${read_output}" HARNESS_FAIL status)
expect_field("${read_output}" input-script-read-failed reason)
expect_field("${read_output}" OFF fixture ran)

set(blank_script "${scratch}/blank.txt")
file(WRITE "${blank_script}" " \t\r\n")
expect_usage_refusal(blank-only input-script-blank-line "${blank_script}")
set(extra_blank_script "${scratch}/extra-blank.txt")
file(WRITE "${extra_blank_script}" "0 1\n\n")
expect_usage_refusal(extra-blank-line input-script-blank-line
  "${extra_blank_script}")

set(trailing_script "${scratch}/trailing.txt")
file(WRITE "${trailing_script}" "0 1 extra\n")
expect_usage_refusal(trailing-token input-script-invalid-grammar
  "${trailing_script}")
set(decreasing_script "${scratch}/decreasing.txt")
file(WRITE "${decreasing_script}" "1 1\n0 2\n")
expect_usage_refusal(decreasing-cycle input-script-cycle-decreasing
  "${decreasing_script}")
set(overflow_script "${scratch}/overflow.txt")
file(WRITE "${overflow_script}" "18446744073709551616 1\n")
expect_usage_refusal(uint64-overflow input-script-cycle-overflow
  "${overflow_script}")
set(mask_script "${scratch}/mask.txt")
file(WRITE "${mask_script}" "0 1024\n")
expect_usage_refusal(key-mask input-script-key-mask-out-of-range
  "${mask_script}")
set(long_script "${scratch}/long.txt")
string(REPEAT "0" 26 long_line)
file(WRITE "${long_script}" "${long_line}\n")
expect_usage_refusal(overlong-line input-script-line-too-long "${long_script}")
set(count_script "${scratch}/count.txt")
file(WRITE "${count_script}" "")
foreach(index RANGE 1 257)
  file(APPEND "${count_script}" "0 0\n")
endforeach()
expect_usage_refusal(excessive-records input-script-too-many-records
  "${count_script}")

set(nul_script "${scratch}/nul.txt")
execute_process(
  COMMAND sh -c "printf '0 1\\000' > \"$1\"" _ "${nul_script}"
  RESULT_VARIABLE nul_write_result
  ERROR_VARIABLE nul_write_error)
if(NOT "${nul_write_result}" STREQUAL "0" OR
    NOT "${nul_write_error}" STREQUAL "")
  message(FATAL_ERROR "phase8-cli: could not create embedded-NUL control")
endif()
expect_usage_refusal(embedded-nul input-script-nul-byte "${nul_script}")

set(boundary_script "${scratch}/at-cap.txt")
string(REPEAT "0 0\n" 4096 boundary_contents)
file(WRITE "${boundary_script}" "${boundary_contents}")
expect_usage_refusal(at-cap-count input-script-too-many-records
  "${boundary_script}")

set(over_script "${scratch}/over-cap.txt")
string(REPEAT "x" 16385 over_contents)
file(WRITE "${over_script}" "${over_contents}")
expect_usage_refusal(over-cap-invalid-prefix input-script-too-large
  "${over_script}")
set(preflight_trace "${scratch}/preflight-order.trace.jsonl")
file(REMOVE "${preflight_trace}")
execute_process(
  COMMAND "${control}" --rom "${scratch}/missing-rom.gba"
    --fixture synthetic-control-pass --cycles 1000
    --trace-mask 1 --trace-capacity 32 --trace-output "${preflight_trace}"
    --input-script "${over_script}"
  RESULT_VARIABLE preflight_result
  OUTPUT_VARIABLE preflight_output
  ERROR_VARIABLE preflight_error)
string(FIND "${preflight_error}" "gamebloke-run: input-script-too-large"
  preflight_reason_offset)
if(NOT "${preflight_result}" STREQUAL "2" OR
    NOT "${preflight_output}" STREQUAL "" OR
    preflight_reason_offset EQUAL -1 OR EXISTS "${preflight_trace}")
  message(FATAL_ERROR
    "phase8-cli: script validation did not precede ROM/output handling")
endif()
set(large_script "${scratch}/large.txt")
string(REPEAT "!" 262144 large_contents)
file(WRITE "${large_script}" "${large_contents}")
expect_usage_refusal(large-over-cap-invalid-prefix input-script-too-large
  "${large_script}")

message("phase8-cli: explicit scripts, canonical identity, parser bounds, and refusal controls passed")
