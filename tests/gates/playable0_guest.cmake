cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "playable0-guest: missing required argument ${required}")
  endif()
endforeach()
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)

if(NOT CASE MATCHES "^(admission|verify|control)$")
  message(FATAL_ERROR "playable0-guest: unknown case ${CASE}")
endif()

set(generator "${BIN}/gbl_instruction_image")
if(DEFINED GENERATOR AND NOT "${GENERATOR}" STREQUAL "")
  set(generator "${GENERATOR}")
endif()

if(CASE STREQUAL "verify" OR CASE STREQUAL "control")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${SRC}" "-DBIN=${SRC}/build/ci"
      -DCASE=verify -P "${SRC}/tests/gates/phase10_policy.cmake"
    RESULT_VARIABLE preflight_result
    OUTPUT_VARIABLE preflight_output
    ERROR_VARIABLE preflight_error
    TIMEOUT 30)
  if(NOT "${preflight_result}" STREQUAL "0")
    message(FATAL_ERROR
      "playable0-guest: phase10 preflight failed (${preflight_result}); ${preflight_error}${preflight_output}")
  endif()
endif()

if(CASE STREQUAL "admission")
set(output_dir "${BIN}/playable0-admission")
if(NOT EXISTS "${generator}" OR IS_DIRECTORY "${generator}")
  message(FATAL_ERROR "playable0-guest: local instruction generator is missing")
endif()
file(MAKE_DIRECTORY "${output_dir}")
execute_process(
  COMMAND "${generator}" "${output_dir}" --include-local-playable0
  RESULT_VARIABLE generate_result
  OUTPUT_VARIABLE generate_output
  ERROR_VARIABLE generate_error)
if(NOT "${generate_result}" STREQUAL "0")
  message(FATAL_ERROR
    "playable0-guest: explicit local guest generation failed (${generate_result}); ${generate_error}${generate_output}")
endif()
foreach(name IN ITEMS owned-playable0.gba owned-playable0-input-disabled.gba
    owned-playable0-wrong-color.gba owned-playable0-wrong-address.gba)
  set(guest "${output_dir}/${name}")
  if(NOT EXISTS "${guest}" OR IS_DIRECTORY "${guest}")
    message(FATAL_ERROR "playable0-guest: generated local variant is missing: ${name}")
  endif()
  file(SIZE "${guest}" guest_size)
  if(NOT guest_size EQUAL 2048)
    message(FATAL_ERROR
      "playable0-guest: generated local variant ${name} has unexpected size ${guest_size}")
  endif()
endforeach()

# An ordinary generator invocation must not emit local-only guest variants.
set(default_dir "${BIN}/playable0-default-generator")
file(MAKE_DIRECTORY "${default_dir}")
execute_process(
  COMMAND "${generator}" "${default_dir}"
  RESULT_VARIABLE default_result
  OUTPUT_VARIABLE default_output
  ERROR_VARIABLE default_error)
if(NOT "${default_result}" STREQUAL "0")
  message(FATAL_ERROR
    "playable0-guest: default generator failed (${default_result}); ${default_error}${default_output}")
endif()
foreach(name IN ITEMS owned-playable0.gba owned-playable0-input-disabled.gba
    owned-playable0-wrong-color.gba owned-playable0-wrong-address.gba)
  if(EXISTS "${default_dir}/${name}")
    message(FATAL_ERROR
      "playable0-guest: default generator leaked local-only variant ${name}")
  endif()
endforeach()

# The lane refuses CI before any build artifact can be generated.
set(ci_refusal_dir "${BIN}/playable0-ci-refusal")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env CI=true "${CMAKE_COMMAND}"
    -S "${SRC}" -B "${ci_refusal_dir}" -DGBL_LANE=local-playable0
  RESULT_VARIABLE ci_result
  OUTPUT_VARIABLE ci_output
  ERROR_VARIABLE ci_error)
if("${ci_result}" STREQUAL "0" OR
    NOT "${ci_output}${ci_error}" MATCHES "local-playable0 lane is prohibited when CI=true")
  message(FATAL_ERROR
    "playable0-guest: CI guard did not refuse configure; exit=${ci_result}; ${ci_error}${ci_output}")
endif()
if(EXISTS "${ci_refusal_dir}/generated/fixtures/owned-playable0.gba")
  message(FATAL_ERROR "playable0-guest: CI refusal left a local guest artifact")
endif()
message(STATUS "playable0-guest: original local guest generation admitted")
return()
endif()

set(output_dir "${BIN}/playable0")
set(runner "${BIN}/gamebloke-run")
set(guest "${BIN}/generated/fixtures/owned-playable0.gba")
set(input_script "${output_dir}/neutral-right-hold-release.txt")
set(video_path "${output_dir}/neutral.raw")
file(MAKE_DIRECTORY "${output_dir}")
foreach(path IN ITEMS "${runner}" "${guest}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "playable0-guest: local run input is missing: ${path}")
  endif()
endforeach()

file(WRITE "${input_script}"
  "0 0\n1450000 16\n1730000 17\n2010000 1\n2290000 0\n")
execute_process(
  COMMAND "${runner}" --rom "${guest}" --fixture owned-playable0
    --cycles 2808960 --input-script "${input_script}"
    --video-output "${video_path}"
  RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_output
  ERROR_VARIABLE run_error
  TIMEOUT 30)
if(NOT "${run_result}" STREQUAL "0" AND
    NOT "${run_result}" STREQUAL "1")
  message(FATAL_ERROR
    "playable0-guest: runner did not complete a bounded result (${run_result}); ${run_error}${run_output}")
endif()
string(REGEX MATCH "^[{][^\n]*[}]\n$" one_json_line "${run_output}")
if(NOT "${one_json_line}" STREQUAL "${run_output}")
  message(FATAL_ERROR "playable0-guest: runner did not write one JSON result line")
endif()
string(JSON actual_status ERROR_VARIABLE status_error GET "${run_output}" status)
string(JSON actual_reason ERROR_VARIABLE reason_error GET "${run_output}" reason)
if(NOT "${status_error}" STREQUAL "NOTFOUND" OR
    NOT "${reason_error}" STREQUAL "NOTFOUND")
  message(FATAL_ERROR
    "playable0-guest: runner result JSON is malformed; ${status_error}${reason_error}")
endif()
if(NOT "${actual_status}" STREQUAL "PASS" OR
    NOT "${run_result}" STREQUAL "0")
  message(FATAL_ERROR
    "playable0-guest: expected registered local status PASS; got ${actual_status}/${actual_reason} (exit ${run_result})")
endif()
string(JSON semantic_state GET "${run_output}" semantic state)
string(JSON semantic_case GET "${run_output}" semantic case_id)
string(JSON applied_events GET "${run_output}" phase8 keypad applied_events)
string(JSON pending_events GET "${run_output}" phase8 keypad pending_events)
string(JSON final_keyinput GET "${run_output}" phase8 keypad keyinput)
string(JSON cycle_count GET "${run_output}" time cycles)
string(JSON frame_state GET "${run_output}" phase10 frame state)
string(JSON frame_length GET "${run_output}" phase10 frame byte_length)
string(JSON video_emitted GET "${run_output}" raw_video emitted)
string(JSON reported_video_sha GET "${run_output}" raw_video sha256)
if(NOT semantic_state STREQUAL "pass" OR
    NOT semantic_case STREQUAL "2884108288" OR
    NOT applied_events STREQUAL "5" OR NOT pending_events STREQUAL "0" OR
    NOT final_keyinput STREQUAL "1023" OR
    NOT cycle_count MATCHES "^[1-9][0-9]*$" OR
    cycle_count GREATER 2808960 OR NOT frame_state STREQUAL "valid" OR
    NOT frame_length STREQUAL "76800" OR NOT video_emitted OR
    NOT EXISTS "${video_path}")
  message(FATAL_ERROR
    "playable0-guest: semantic/frame expectation failed (semantic=${semantic_state}/${semantic_case}; input=${applied_events}/${pending_events}/${final_keyinput}; cycles=${cycle_count}; frame=${frame_state}/${frame_length}; emitted=${video_emitted})")
endif()
file(SIZE "${video_path}" actual_video_length)
file(SHA256 "${video_path}" actual_video_sha)
if(NOT actual_video_length EQUAL 76800 OR
    NOT actual_video_sha STREQUAL reported_video_sha)
  message(FATAL_ERROR
    "playable0-guest: raw output metadata mismatch (${actual_video_length}/${actual_video_sha}/${reported_video_sha})")
endif()
message(STATUS
  "playable0-guest: registered PASS; case=${semantic_case}; events=${applied_events}; cycles=${cycle_count}; raw_bytes=${actual_video_length}; sha256=${actual_video_sha}")
