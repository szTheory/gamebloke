cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase10-cli: usage: -D${required}= is required")
  endif()
endforeach()
if(NOT CASE STREQUAL "verify" AND NOT CASE STREQUAL "control")
  message(FATAL_ERROR "phase10-cli: CASE must be verify or control")
endif()
get_filename_component(BIN "${BIN}" ABSOLUTE)
set(runner "${BIN}/gamebloke-run")
set(control "${BIN}/gamebloke-run-control")
set(fixture "${BIN}/generated/fixtures/owned-arm-pass.gba")
set(scratch "${BIN}/phase10-cli")
foreach(path IN ITEMS "${runner}" "${control}" "${fixture}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "phase10-cli: required-input-missing:${path}")
  endif()
endforeach()
file(MAKE_DIRECTORY "${scratch}")

function(phase10_fail detail)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "1..1")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo
    "not ok 1 - bounded raw-video CLI preserves result and input evidence")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "# ${detail}")
  message(FATAL_ERROR "phase10-cli: ${detail}")
endfunction()

function(expect_usage name expected_reason)
  execute_process(
    COMMAND "${control}" --rom "${fixture}" --fixture synthetic-control-pass
      --cycles 1000 ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "2" OR NOT "${output}" STREQUAL "" OR
      NOT "${error}" MATCHES "usage: gamebloke-run")
    phase10_fail("${name}: expected side-effect-free usage refusal; exit=${result}; stdout=${output}; stderr=${error}")
  endif()
  if(NOT "${expected_reason}" STREQUAL "" AND
      NOT "${error}" MATCHES "gamebloke-run: ${expected_reason}")
    phase10_fail("${name}: missing named refusal ${expected_reason}; stderr=${error}")
  endif()
endfunction()

execute_process(COMMAND "${runner}" --help
  RESULT_VARIABLE help_result OUTPUT_VARIABLE help_output ERROR_VARIABLE help_error)
if(NOT "${help_result}" STREQUAL "0" OR
    NOT "${help_output}" MATCHES "--video-output PATH" OR
    NOT "${help_error}" STREQUAL "")
  phase10_fail("--help does not document --video-output PATH")
endif()

expect_usage(missing-path "" --video-output)
expect_usage(empty-path "" --video-output "")
expect_usage(duplicate-path "" --video-output "${scratch}/one.raw"
  --video-output "${scratch}/two.raw")
expect_usage(cycle-frame-conflict "" --video-output "${scratch}/conflict.raw"
  --frames 1)

# Existing input and output names, including normalized aliases, are refused before
# the guest or any other writer can run. The final writer still uses exclusive create.
set(fixture_alias "${BIN}/generated/fixtures/./owned-arm-pass.gba")
expect_usage(rom-alias video-output-path-conflict
  --video-output "${fixture_alias}")

set(input_script "${scratch}/input.txt")
file(WRITE "${input_script}" "0 1\n")
file(SHA256 "${input_script}" input_before)
expect_usage(input-alias video-output-path-conflict
  --input-script "${input_script}"
  --video-output "${scratch}/./input.txt")
file(SHA256 "${input_script}" input_after)
if(NOT "${input_before}" STREQUAL "${input_after}")
  phase10_fail("input alias changed the logical-input script")
endif()

set(trace_path "${scratch}/trace.jsonl")
file(REMOVE "${trace_path}")
expect_usage(trace-alias video-output-path-conflict
  --trace-mask 1 --trace-capacity 32 --trace-output "${trace_path}"
  --video-output "${scratch}/./trace.jsonl")
if(EXISTS "${trace_path}")
  phase10_fail("trace alias created a trace before refusing the output conflict")
endif()
expect_usage(result-alias video-output-path-conflict --video-output /dev/stdout)

set(unrequested_path "${scratch}/unrequested.raw")
file(REMOVE "${unrequested_path}")
execute_process(
  COMMAND "${control}" --rom "${fixture}" --fixture synthetic-control-pass
    --frames 1
  RESULT_VARIABLE absent_result OUTPUT_VARIABLE absent_json ERROR_VARIABLE absent_error)
if(NOT "${absent_result}" STREQUAL "0" OR NOT "${absent_error}" STREQUAL "")
  phase10_fail("no-output-request run failed; exit=${absent_result}; stderr=${absent_error}")
endif()
string(JSON absent_requested GET "${absent_json}" raw_video requested)
string(JSON absent_status GET "${absent_json}" raw_video status)
string(JSON absent_emitted GET "${absent_json}" raw_video emitted)
string(JSON absent_digest_type TYPE "${absent_json}" raw_video sha256)
if(absent_requested OR NOT absent_status STREQUAL "not-requested" OR
    absent_emitted OR NOT absent_digest_type STREQUAL "NULL" OR
    EXISTS "${unrequested_path}")
  phase10_fail("no-output-request result claimed a raw artifact")
endif()

set(output_path "${scratch}/incomplete.raw")
file(REMOVE "${output_path}")
execute_process(
  COMMAND "${control}" --rom "${fixture}" --fixture synthetic-control-pass
    --cycles 1000 --input-script "${input_script}" --video-output "${output_path}"
  RESULT_VARIABLE requested_result OUTPUT_VARIABLE requested_json ERROR_VARIABLE requested_error)
if(NOT "${requested_result}" STREQUAL "1" OR
    NOT "${requested_error}" STREQUAL "" OR EXISTS "${output_path}")
  phase10_fail("unsupported synthetic frame was not refused cleanly; exit=${requested_result}; stderr=${requested_error}")
endif()
string(JSON requested_status GET "${requested_json}" status)
string(JSON requested_reason GET "${requested_json}" reason)
string(JSON requested_flag GET "${requested_json}" raw_video requested)
string(JSON video_status GET "${requested_json}" raw_video status)
string(JSON video_emitted GET "${requested_json}" raw_video emitted)
string(JSON video_failure GET "${requested_json}" raw_video failure)
string(JSON video_length_type TYPE "${requested_json}" raw_video byte_length)
string(JSON video_digest_type TYPE "${requested_json}" raw_video sha256)
string(JSON input_kind GET "${requested_json}" claim input_timeline kind)
string(JSON input_applied GET "${requested_json}" phase8 keypad applied_events)
string(JSON budget GET "${requested_json}" time budget_cycles)
if(NOT requested_status STREQUAL "HARNESS_FAIL" OR
    NOT requested_reason STREQUAL "video-output-failed" OR
    NOT requested_flag OR NOT video_status STREQUAL "failed" OR
    video_emitted OR NOT video_failure STREQUAL "video-frame-unavailable" OR
    NOT video_length_type STREQUAL "NULL" OR
    NOT video_digest_type STREQUAL "NULL" OR
    NOT input_kind STREQUAL "script" OR NOT input_applied EQUAL 1 OR
    NOT budget STREQUAL "1000")
  phase10_fail("CLI result did not preserve input, cycle budget, and classified video refusal")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "1..1")
execute_process(COMMAND "${CMAKE_COMMAND}" -E echo
  "ok 1 - bounded raw-video CLI preserves result and input evidence")
