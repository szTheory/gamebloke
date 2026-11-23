cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "audio-guest: missing required argument ${required}")
  endif()
endforeach()
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)

set(runner "${BIN}/gamebloke-run")
set(audio_image "${BIN}/generated/fixtures/owned-audio-mix.gba")
set(scratch "${BIN}/nc/audio_guest")
set(expected_pcm_hex "db660000db66007fdb66007fdb660080")
file(MAKE_DIRECTORY "${scratch}")

foreach(path IN ITEMS "${runner}" "${audio_image}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "audio-guest: required local input is missing")
  endif()
endforeach()
if(NOT "${audio_image}" STREQUAL
    "${BIN}/generated/fixtures/owned-audio-mix.gba")
  message(FATAL_ERROR "audio-guest: generated image escaped its build path")
endif()

function(expect_json_field json expected)
  string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "audio-guest: expected ${ARGN}=${expected}; got ${actual}; ${json_error}")
  endif()
endfunction()

function(expect_json_type json expected)
  string(JSON actual ERROR_VARIABLE json_error TYPE "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "audio-guest: expected ${ARGN} type ${expected}; got ${actual}; ${json_error}")
  endif()
endfunction()

function(expect_json_boolean json expected)
  string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR
      "audio-guest: could not read boolean ${ARGN}; ${json_error}")
  endif()
  if("${expected}" STREQUAL "true" AND NOT actual)
    message(FATAL_ERROR "audio-guest: expected ${ARGN}=true")
  elseif("${expected}" STREQUAL "false" AND actual)
    message(FATAL_ERROR "audio-guest: expected ${ARGN}=false")
  endif()
endfunction()

function(read_raw_run output_variable name audio_path trace_path)
  file(REMOVE "${audio_path}" "${trace_path}")
  execute_process(
    COMMAND "${runner}" --rom "${audio_image}" --cycles 20000
      --audio-output "${audio_path}" --audio-cycles 2048
      --trace-mask 15 --trace-capacity 65536 --trace-output "${trace_path}"
    RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error TIMEOUT 30)
  if(NOT "${run_result}" STREQUAL "1")
    message(FATAL_ERROR
      "audio-guest: ${name} raw runner exit ${run_result}, expected unregistered exit 1; ${run_error}${run_output}")
  endif()
  string(REGEX MATCH "^[{][^\n]*[}]\n$" one_json_line "${run_output}")
  if(NOT "${one_json_line}" STREQUAL "${run_output}")
    message(FATAL_ERROR "audio-guest: ${name} did not emit one JSON line")
  endif()
  expect_json_field("${run_output}" GUEST_STOP status)
  expect_json_field("${run_output}" unregistered-content reason)
  expect_json_type("${run_output}" NULL fixture id)
  expect_json_type("${run_output}" BOOLEAN fixture ran)
  expect_json_boolean("${run_output}" true fixture ran)
  expect_json_field("${run_output}" pass semantic state)
  expect_json_field("${run_output}" 19001 semantic case_id)
  expect_json_field("${run_output}" unregistered-content termination)
  string(JSON claim_count LENGTH "${run_output}" claim evidence)
  if(NOT claim_count EQUAL 0)
    message(FATAL_ERROR "audio-guest: raw unregistered observation gained evidence claims")
  endif()

  expect_json_field("${run_output}" gamebloke-audio-model-4 audio model)
  expect_json_boolean("${run_output}" true audio requested)
  expect_json_field("${run_output}" complete audio capture_state)
  expect_json_boolean("${run_output}" true audio interval_complete)
  expect_json_field("${run_output}" 4 audio frame_count)
  expect_json_field("${run_output}" 16 audio byte_count)
  expect_json_field("${run_output}" emitted audio artifact state)
  expect_json_field("${run_output}" 0 audio actual_interval start_cycle)
  expect_json_field("${run_output}" 2048 audio actual_interval end_cycle)

  if(NOT EXISTS "${audio_path}" OR IS_DIRECTORY "${audio_path}" OR
      NOT EXISTS "${trace_path}" OR IS_DIRECTORY "${trace_path}")
    message(FATAL_ERROR "audio-guest: ${name} raw audio or trace output is missing")
  endif()
  file(SIZE "${audio_path}" actual_pcm_size)
  file(READ "${audio_path}" actual_pcm_hex HEX)
  file(SHA256 "${audio_path}" actual_pcm_sha)
  expect_json_field("${run_output}" "${actual_pcm_sha}" audio artifact sha256)
  if(NOT actual_pcm_size EQUAL 16 OR
      NOT "${actual_pcm_hex}" STREQUAL "${expected_pcm_hex}")
    message(FATAL_ERROR
      "audio-guest: ${name} PCM mismatch: bytes=${actual_pcm_size}, hex=${actual_pcm_hex}")
  endif()

  string(JSON cycles GET "${run_output}" time cycles)
  string(JSON dma_completed GET "${run_output}" time dma_completed_units)
  string(JSON channel_dma_completed GET "${run_output}" phase9 channels 1 completed_units)
  if(NOT cycles MATCHES "^[1-9][0-9]*$" OR cycles GREATER 20000 OR
      NOT dma_completed MATCHES "^[1-9][0-9]*$" OR
      NOT channel_dma_completed MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
      "audio-guest: ${name} lacks bounded guest progress or FIFO DMA accounting")
  endif()

  file(STRINGS "${trace_path}" trace_lines ENCODING UTF-8)
  set(refill_found FALSE)
  foreach(line IN LISTS trace_lines)
    string(JSON category ERROR_VARIABLE category_error GET "${line}" category)
    if("${category_error}" STREQUAL "NOTFOUND" AND category EQUAL 8)
      string(JSON channel GET "${line}" dma channel)
      string(JSON dma_status GET "${line}" dma status)
      string(JSON source_address GET "${line}" dma source_address)
      string(JSON destination_address GET "${line}" dma destination_address)
      string(JSON width GET "${line}" dma width)
      string(JSON guest_cycle GET "${line}" guest_cycle)
      if(channel EQUAL 1 AND dma_status STREQUAL "ok" AND
          source_address STREQUAL "0x08000300" AND
          destination_address STREQUAL "0x040000A0" AND width EQUAL 4 AND
          guest_cycle LESS 2048)
        set(refill_found TRUE)
      endif()
    endif()
  endforeach()
  if(NOT refill_found)
    message(FATAL_ERROR
      "audio-guest: ${name} trace has no DMA1 FIFO-A transfer from the original payload inside the capture interval")
  endif()
  set(${output_variable} "${run_output}" PARENT_SCOPE)
endfunction()

# Local-only fixtures must be refused before CMake can create build targets or
# generated bytes in a CI environment.
set(ci_refusal_dir "${scratch}/ci-refusal")
file(REMOVE_RECURSE "${ci_refusal_dir}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env CI=true "${CMAKE_COMMAND}"
    -S "${SRC}" -B "${ci_refusal_dir}" -DGBL_LANE=local-audio
  RESULT_VARIABLE ci_result OUTPUT_VARIABLE ci_output ERROR_VARIABLE ci_error
  TIMEOUT 30)
if("${ci_result}" STREQUAL "0" OR
    NOT "${ci_output}${ci_error}" MATCHES
      "local-audio lane is prohibited when CI=true")
  message(FATAL_ERROR
    "audio-guest: CI guard did not refuse local-audio; exit=${ci_result}; ${ci_error}${ci_output}")
endif()
if(EXISTS "${ci_refusal_dir}/generated/fixtures/owned-audio-mix.gba")
  message(FATAL_ERROR "audio-guest: CI refusal left a local audio image")
endif()

set(audio_one "${scratch}/run-one.raw")
set(trace_one "${scratch}/run-one.trace.jsonl")
set(audio_two "${scratch}/run-two.raw")
set(trace_two "${scratch}/run-two.trace.jsonl")
read_raw_run(run_one "first" "${audio_one}" "${trace_one}")
read_raw_run(run_two "repeat" "${audio_two}" "${trace_two}")
if(NOT "${run_one}" STREQUAL "${run_two}")
  message(FATAL_ERROR "audio-guest: fresh production runner result JSON differs")
endif()
file(READ "${trace_one}" trace_one_text)
file(READ "${trace_two}" trace_two_text)
file(READ "${audio_one}" audio_one_hex HEX)
file(READ "${audio_two}" audio_two_hex HEX)
file(SHA256 "${audio_one}" audio_one_sha)
file(SHA256 "${audio_two}" audio_two_sha)
if(NOT "${trace_one_text}" STREQUAL "${trace_two_text}" OR
    NOT "${audio_one_hex}" STREQUAL "${audio_two_hex}" OR
    NOT "${audio_one_sha}" STREQUAL "${audio_two_sha}")
  message(FATAL_ERROR
    "audio-guest: fresh trace, PCM or artifact identity differs")
endif()

function(pcm_expectation_matches expected actual output_variable)
  if("${expected}" STREQUAL "${actual}")
    set(${output_variable} TRUE PARENT_SCOPE)
  else()
    set(${output_variable} FALSE PARENT_SCOPE)
  endif()
endfunction()
pcm_expectation_matches("${expected_pcm_hex}" "${audio_one_hex}"
  correct_pcm_match)
pcm_expectation_matches("db660000db66007fdb66007fdb66007f" "${audio_one_hex}"
  altered_pcm_match)
if(NOT correct_pcm_match OR altered_pcm_match)
  message(FATAL_ERROR "audio-guest: literal PCM negative control did not reject drift")
endif()

# The same original generator can make a build-only altered mailbox marker for
# a negative control. It is excluded from every manifest and lane inventory.
set(negative_marker_image
  "${BIN}/generated/fixtures/owned-audio-mix-invalid-marker.gba")
execute_process(
  COMMAND "${BIN}/gbl_audio_guest_image" "${BIN}/generated/fixtures"
    --negative-completion-marker
  RESULT_VARIABLE negative_generate_result
  OUTPUT_VARIABLE negative_generate_output ERROR_VARIABLE negative_generate_error
  TIMEOUT 30)
if(NOT "${negative_generate_result}" STREQUAL "0" OR
    NOT EXISTS "${negative_marker_image}" OR IS_DIRECTORY "${negative_marker_image}")
  message(FATAL_ERROR
    "audio-guest: could not create bounded mailbox negative control; ${negative_generate_error}${negative_generate_output}")
endif()
file(SHA256 "${negative_marker_image}" negative_marker_sha)
file(SHA256 "${audio_image}" expected_marker_sha)
if(negative_marker_sha STREQUAL expected_marker_sha)
  message(FATAL_ERROR "audio-guest: negative marker image did not differ")
endif()
execute_process(
  COMMAND "${runner}" --rom "${negative_marker_image}" --cycles 8000
  RESULT_VARIABLE negative_marker_result
  OUTPUT_VARIABLE negative_marker_output ERROR_VARIABLE negative_marker_error
  TIMEOUT 30)
if(NOT "${negative_marker_result}" STREQUAL "1")
  message(FATAL_ERROR
    "audio-guest: wrong completion marker runner exit ${negative_marker_result}; ${negative_marker_error}${negative_marker_output}")
endif()
expect_json_field("${negative_marker_output}" TIMEOUT status)
expect_json_field("${negative_marker_output}" cycle-budget reason)
expect_json_field("${negative_marker_output}" pending semantic state)
expect_json_field("${negative_marker_output}" 19001 semantic case_id)
message(STATUS
  "audio-guest: wrong completion marker remained pending; altered literal PCM expectation rejected")
message(STATUS
  "audio-guest: raw BIOS-free guest reached case 19001; 4-frame literal PCM and actual FIFO-A DMA refill passed twice; local-only CI guard refused selection")
