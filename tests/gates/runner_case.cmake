cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN VALIDATE_PRODUCER)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "runner-case: usage: -D${required}= is required")
  endif()
endforeach()
if(NOT VALIDATE_PRODUCER)
  message(FATAL_ERROR "runner-case: VALIDATE_PRODUCER must be enabled")
endif()
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)

set(control "${BIN}/gamebloke-run-control")
set(production "${BIN}/gamebloke-run")
set(unit_test "${BIN}/runner_test")
set(fixtures "${BIN}/generated/fixtures")
set(scratch "${BIN}/runner-cases")
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

function(expect_json_type json expected)
  string(JSON actual ERROR_VARIABLE json_error TYPE "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "runner-case: expected JSON type ${ARGN}=${expected}; got ${actual}; ${json_error}")
  endif()
endfunction()

if(DEFINED CASE AND CASE STREQUAL "audio_cli")
  set(audio_rom "${BIN}/generated/diagnostics/armv4t/arm/roms/arm-268640271.gba")
  if(NOT EXISTS "${production}" OR NOT EXISTS "${audio_rom}" OR
      NOT EXISTS "${unit_test}")
    message(FATAL_ERROR "runner-case: audio_cli production inputs are missing")
  endif()
  set(audio_scratch "${scratch}/audio-cli")
  file(MAKE_DIRECTORY "${audio_scratch}")
  set(audio_output "${audio_scratch}/empty.raw")
  file(REMOVE "${audio_output}")

  execute_process(
    COMMAND "${production}" --rom "${audio_rom}" --fixture arm-268640271
      --cycles 1000 --audio-output "${audio_output}" --audio-cycles 1
    RESULT_VARIABLE audio_rc OUTPUT_VARIABLE audio_json ERROR_VARIABLE audio_error)
  if(NOT "${audio_rc}" STREQUAL "0")
    message(FATAL_ERROR
      "runner-case: production audio CLI failed (${audio_rc}): ${audio_error}; ${audio_json}")
  endif()
  expect_json_field("${audio_json}" PASS status)
  expect_json_field("${audio_json}" mailbox-complete termination)
  expect_json_field("${audio_json}" gamebloke-audio-model-4 audio model)
  expect_json_field("${audio_json}" agb audio profile id)
  expect_json_field("${audio_json}" 16 audio profile version)
  expect_json_field("${audio_json}" project-selected audio policy_classification)
  expect_json_field("${audio_json}" UNKNOWN audio hardware_qualification)
  expect_json_field("${audio_json}" pre-pwm audio policy tap)
  expect_json_field("${audio_json}" fixed-reset-anchored-grid audio policy tap_policy)
  expect_json_field("${audio_json}" signed-s16le audio policy encoding)
  expect_json_field("${audio_json}" 32768 audio policy sample_rate_hz)
  expect_json_field("${audio_json}" 512 audio policy sample_cycles)
  expect_json_boolean("${audio_json}" true audio requested)
  expect_json_field("${audio_json}" 1 audio requested_interval end_cycle)
  expect_json_field("${audio_json}" 1 audio actual_interval end_cycle)
  expect_json_field("${audio_json}" complete audio capture_state)
  expect_json_boolean("${audio_json}" true audio interval_complete)
  expect_json_field("${audio_json}" 0 audio frame_count)
  expect_json_field("${audio_json}" 0 audio byte_count)
  expect_json_field("${audio_json}" emitted audio artifact state)
  expect_json_field("${audio_json}" e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 audio artifact sha256)
  file(SIZE "${audio_output}" audio_size)
  file(SHA256 "${audio_output}" audio_digest)
  if(NOT audio_size EQUAL 0 OR
      NOT audio_digest STREQUAL "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")
    message(FATAL_ERROR "runner-case: exact empty audio file evidence differs")
  endif()
  string(JSON source_count LENGTH "${audio_json}" audio policy implemented_sources)
  if(NOT source_count EQUAL 6)
    message(FATAL_ERROR "runner-case: audio implementation source inventory is incomplete")
  endif()

  function(expect_audio_usage name)
    execute_process(COMMAND "${production}" ${ARGN}
      RESULT_VARIABLE usage_rc OUTPUT_VARIABLE usage_stdout ERROR_VARIABLE usage_stderr)
    if(NOT "${usage_rc}" STREQUAL "2" OR NOT "${usage_stdout}" STREQUAL "" OR
        NOT "${usage_stderr}" MATCHES "^usage: gamebloke-run")
      message(FATAL_ERROR
        "runner-case: audio ${name} was not refused before execution (${usage_rc}): ${usage_stdout}${usage_stderr}")
    endif()
  endfunction()

  expect_audio_usage(duplicate-output --rom "${audio_rom}" --cycles 1000
    --audio-output "${audio_scratch}/a.raw" --audio-output "${audio_scratch}/b.raw")
  expect_audio_usage(empty-output --rom "${audio_rom}" --cycles 1000 --audio-output "")
  expect_audio_usage(cycles-require-output --rom "${audio_rom}" --cycles 1000
    --audio-cycles 1)
  expect_audio_usage(empty-cycles --rom "${audio_rom}" --cycles 1000
    --audio-output "${audio_scratch}/empty-cycles.raw" --audio-cycles "")
  expect_audio_usage(duplicate-cycles --rom "${audio_rom}" --cycles 1000
    --audio-output "${audio_scratch}/dup.raw" --audio-cycles 1 --audio-cycles 2)
  expect_audio_usage(overflow-cycles --rom "${audio_rom}" --cycles 1000
    --audio-output "${audio_scratch}/overflow.raw" --audio-cycles 18446744073709551616)
  expect_audio_usage(signed-cycles --rom "${audio_rom}" --cycles 1000
    --audio-output "${audio_scratch}/signed.raw" --audio-cycles +1)
  expect_audio_usage(negative-cycles --rom "${audio_rom}" --cycles 1000
    --audio-output "${audio_scratch}/negative.raw" --audio-cycles -1)
  expect_audio_usage(malformed-cycles --rom "${audio_rom}" --cycles 1000
    --audio-output "${audio_scratch}/malformed.raw" --audio-cycles 1x)
  expect_audio_usage(zero-cycles --rom "${audio_rom}" --cycles 1000
    --audio-output "${audio_scratch}/zero.raw" --audio-cycles 0)
  expect_audio_usage(oversized-cycles --rom "${audio_rom}" --cycles 20000000
    --audio-output "${audio_scratch}/large.raw" --audio-cycles 16777217)
  expect_audio_usage(outside-run-budget --rom "${audio_rom}" --cycles 10
    --audio-output "${audio_scratch}/outside.raw" --audio-cycles 11)
  expect_audio_usage(default-outside-bound --rom "${audio_rom}" --cycles 16777217
    --audio-output "${audio_scratch}/default-large.raw")
  expect_audio_usage(duplicate-rom-alias --rom "${audio_rom}" --cycles 1000
    --audio-output "${audio_rom}" --audio-cycles 1)
  expect_audio_usage(stdout-alias --rom "${audio_rom}" --cycles 1000
    --audio-output /dev/stdout --audio-cycles 1)

  set(audio_script "${audio_scratch}/input.script")
  file(WRITE "${audio_script}" "not-loaded-before-alias-check")
  expect_audio_usage(input-alias --rom "${audio_rom}" --cycles 1000
    --input-script "${audio_script}" --audio-output "${audio_script}" --audio-cycles 1)
  expect_audio_usage(trace-alias --rom "${audio_rom}" --cycles 1000
    --trace-output "${audio_scratch}/trace.jsonl"
    --audio-output "${audio_scratch}/trace.jsonl" --audio-cycles 1)
  expect_audio_usage(video-alias --rom "${audio_rom}" --cycles 1000
    --video-output "${audio_scratch}/video.raw"
    --audio-output "${audio_scratch}/video.raw" --audio-cycles 1)

  set(audio_hardlink "${audio_scratch}/rom-hardlink.gba")
  set(audio_symlink "${audio_scratch}/rom-symlink.gba")
  file(REMOVE "${audio_hardlink}" "${audio_symlink}")
  file(CREATE_LINK "${audio_rom}" "${audio_hardlink}" RESULT hardlink_result)
  file(CREATE_LINK "${audio_rom}" "${audio_symlink}" SYMBOLIC RESULT symlink_result)
  if(hardlink_result STREQUAL "0")
    expect_audio_usage(hardlink-alias --rom "${audio_rom}" --cycles 1000
      --audio-output "${audio_hardlink}" --audio-cycles 1)
  endif()
  if(symlink_result STREQUAL "0")
    expect_audio_usage(symlink-alias --rom "${audio_rom}" --cycles 1000
      --audio-output "${audio_symlink}" --audio-cycles 1)
  endif()

  set(missing_script "${audio_scratch}/missing.script")
  file(REMOVE "${missing_script}")
  execute_process(
    COMMAND "${production}" --rom "${audio_rom}" --cycles 1000
      --input-script "${missing_script}" --audio-output "${audio_scratch}/missing-script.raw"
      --audio-cycles 1
    RESULT_VARIABLE missing_rc OUTPUT_VARIABLE missing_json ERROR_VARIABLE missing_error)
  if(NOT "${missing_rc}" STREQUAL "1")
    message(FATAL_ERROR "runner-case: missing script did not return its structured failure")
  endif()
  expect_json_field("${missing_json}" MISSING status)
  expect_json_boolean("${missing_json}" true audio requested)
  expect_json_field("${missing_json}" unavailable audio capture_state)
  expect_json_field("${missing_json}" not-written audio artifact state)
  expect_json_field("${missing_json}" 1 audio requested_interval end_cycle)

  message(STATUS "runner-case: production audio CLI controls passed")
  return()
endif()
foreach(path IN ITEMS "${control}" "${production}" "${unit_test}"
    "${fixtures}/owned-arm-pass.gba" "${fixtures}/owned-arm-fail.gba"
    "${fixtures}/owned-arm-pass-zero-case.gba"
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

function(expect_active_result_extension json)
  expect_json_type("${json}" OBJECT timer0)
  expect_json_type("${json}" STRING time model)
  expect_json_field("${json}" gamebloke-timer-model-1 time model)
  expect_json_type("${json}" STRING time qualification)
  expect_json_field("${json}" project-model-only time qualification)
  foreach(field IN ITEMS cycles instructions budget_cycles bus_cycles internal_cycles
      wait_cycles
      dma_startup_cycles dma_source_cycles dma_destination_cycles
      dma_completed_units dma_pause_cycles)
    expect_json_type("${json}" STRING time ${field})
  endforeach()
  expect_json_type("${json}" OBJECT phase9)
  expect_json_field("${json}" gamebloke-dma-observation-1 phase9 model)
  expect_json_type("${json}" BOOLEAN phase9 observed)
  expect_json_type("${json}" OBJECT phase9 timing)
  string(JSON cycles GET "${json}" time cycles)
  foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
      dma_destination_cycles dma_completed_units dma_pause_cycles)
    expect_json_type("${json}" STRING phase9 timing ${field})
    string(JSON time_component GET "${json}" time ${field})
    string(JSON phase9_component GET "${json}" phase9 timing ${field})
    if(NOT "${time_component}" STREQUAL "${phase9_component}")
      message(FATAL_ERROR "runner-case: phase9 timing differs from time.${field}")
    endif()
  endforeach()
  string(JSON phase9_observed GET "${json}" phase9 observed)
  if(phase9_observed)
    expect_json_type("${json}" NUMBER phase9 waitcnt)
    expect_json_type("${json}" STRING phase9 bus_owner)
    expect_json_type("${json}" BOOLEAN phase9 cpu_paused)
    expect_json_type("${json}" STRING phase9 machine_cycles)
    expect_json_type("${json}" ARRAY phase9 channels)
    string(JSON channel_count LENGTH "${json}" phase9 channels)
    if(NOT channel_count EQUAL 4)
      message(FATAL_ERROR "runner-case: phase9 must expose all four DMA channels")
    endif()
    string(JSON phase9_machine_cycles GET "${json}" phase9 machine_cycles)
    if(NOT "${phase9_machine_cycles}" STREQUAL "${cycles}")
      message(FATAL_ERROR
        "runner-case: phase9 machine time ${phase9_machine_cycles} differs from total cycles ${cycles}")
    endif()
  else()
    foreach(path IN ITEMS waitcnt bus_owner cpu_paused machine_cycles channels)
      string(REPLACE "/" ";" keys "${path}")
      expect_json_type("${json}" NULL phase9 ${keys})
    endforeach()
  endif()
  expect_json_field("${json}" nintendo-agb-programming-manual-v1.1-sections-3.3-3.3.2
    phase9 waitcnt_policy manual_waits)
  expect_json_field("${json}" project-selected phase9 waitcnt_policy other_region_costs)
  expect_json_field("${json}" gbatek-3.06-community-documentation
    phase9 waitcnt_policy 128k_boundary_source)
  expect_json_field("${json}" UNKNOWN phase9 waitcnt_policy hardware_qualification)
  expect_json_field("${json}" 2 phase9 startup_policy cycles_per_request)
  expect_json_field("${json}" project-selected phase9 startup_policy provenance)
  expect_json_field("${json}" "described; conversion is selected"
    phase9 startup_policy manual_activation_delay)
  expect_json_field("${json}" UNKNOWN phase9 startup_policy hardware_qualification)
  expect_json_field("${json}" per-channel-retained-latch phase9 latch_policy policy)
  expect_json_field("${json}" project-selected phase9 latch_policy provenance)
  expect_json_field("${json}" UNKNOWN phase9 latch_policy hardware_qualification)
  expect_json_field("${json}" unmodeled phase9 prefetch)
  string(JSON fallback_type TYPE "${json}" phase9 latch_policy fallback_observed)
  if(NOT fallback_type STREQUAL "BOOLEAN" AND NOT fallback_type STREQUAL "NULL")
    message(FATAL_ERROR "runner-case: latch fallback observation must be boolean or null")
  endif()
  if(fallback_type STREQUAL "BOOLEAN")
    string(JSON fallback_observed GET "${json}" phase9 latch_policy fallback_observed)
    if(fallback_observed)
      expect_json_field("${json}" disputed phase9 latch_policy classification)
      expect_json_field("${json}" disp-dma-latch phase9 latch_policy dispute_id)
    else()
      expect_json_type("${json}" NULL phase9 latch_policy classification)
      expect_json_type("${json}" NULL phase9 latch_policy dispute_id)
    endif()
  else()
    expect_json_type("${json}" NULL phase9 latch_policy classification)
    expect_json_type("${json}" NULL phase9 latch_policy dispute_id)
  endif()
  string(JSON ran GET "${json}" fixture ran)
  string(JSON cycles GET "${json}" time cycles)
  string(JSON instructions GET "${json}" time instructions)
  string(JSON request_type ERROR_VARIABLE request_error TYPE
    "${json}" timer0 if_request)
  if(NOT "${request_error}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR
      "runner-case: active producer omitted timer0.if_request: ${request_error}")
  endif()
  if(NOT ran OR (cycles STREQUAL "0" AND instructions STREQUAL "0"))
    if(NOT request_type STREQUAL "NULL")
      message(FATAL_ERROR
        "runner-case: not-run result requires timer0.if_request null; got ${request_type}")
    endif()
  elseif(NOT request_type STREQUAL "BOOLEAN")
    message(FATAL_ERROR
      "runner-case: executed result requires boolean timer0.if_request; got ${request_type}")
  endif()

  expect_json_type("${json}" OBJECT phase10)
  expect_json_field("${json}" gamebloke-ppu-observation-2 phase10 model)
  expect_json_type("${json}" BOOLEAN phase10 observed)
  expect_json_type("${json}" OBJECT phase10 timing)
  expect_json_field("${json}" gamebloke-ppu-model-1 phase10 timing model)
  expect_json_field("${json}" 1006 phase10 timing hblank_offset_cycles)
  expect_json_field("${json}" project-selected phase10 timing policy)
  expect_json_field("${json}" REF phase10 timing evidence_class)
  expect_json_field("${json}" disputed phase10 timing classification)
  expect_json_field("${json}" disp-hblank-flag-edge phase10 timing dispute_id)
  expect_json_field("${json}" UNKNOWN phase10 timing hardware_qualification)
  string(JSON ppu_observed GET "${json}" phase10 observed)
  if(ppu_observed)
    expect_json_type("${json}" OBJECT phase10 raster)
    expect_json_type("${json}" OBJECT phase10 frame)
    expect_json_type("${json}" OBJECT phase10 controls)
    expect_json_type("${json}" OBJECT phase10 hblank_trigger_dependency)
    foreach(field IN ITEMS line line_cycle machine_cycle)
      if(field STREQUAL "line")
        expect_json_type("${json}" NUMBER phase10 raster ${field})
      else()
        expect_json_type("${json}" STRING phase10 raster ${field})
      endif()
    endforeach()
    string(JSON frame_epoch_type TYPE "${json}" phase10 raster frame_epoch)
    if(NOT frame_epoch_type STREQUAL "STRING" AND
        NOT frame_epoch_type STREQUAL "NULL")
      message(FATAL_ERROR
        "runner-case: Phase 10 raster frame epoch must be string or null")
    endif()
    foreach(pair IN ITEMS "width;240" "height;160" "stride_bytes;480"
        "byte_length;76800")
      list(GET pair 0 field)
      list(GET pair 1 value)
      expect_json_field("${json}" "${value}" phase10 frame ${field})
    endforeach()
    expect_json_field("${json}" bgr555-le phase10 frame format)
    expect_json_field("${json}" canonical-vram phase10 frame source)
    expect_json_field("${json}" row-start phase10 frame latch_policy)
    expect_json_type("${json}" BOOLEAN phase10 frame supported_controls)
    expect_json_type("${json}" BOOLEAN phase10 controls supported)
    expect_json_field("${json}" UNKNOWN
      phase10 hblank_trigger_dependency hardware_qualification)
    string(JSON ppu_cycle GET "${json}" phase10 raster machine_cycle)
    string(JSON total_cycles GET "${json}" time cycles)
    if(NOT "${ppu_cycle}" STREQUAL "${total_cycles}")
      message(FATAL_ERROR
        "runner-case: Phase 10 machine cycle differs from total machine time")
    endif()
  else()
    foreach(path IN ITEMS raster frame controls hblank_trigger_dependency)
      expect_json_type("${json}" NULL phase10 ${path})
    endforeach()
  endif()
  expect_json_type("${json}" OBJECT raw_video)
  expect_json_boolean("${json}" false raw_video requested)
  expect_json_field("${json}" not-requested raw_video status)
  expect_json_boolean("${json}" false raw_video emitted)
  expect_json_type("${json}" NULL raw_video byte_length)
  expect_json_type("${json}" NULL raw_video sha256)
  expect_json_type("${json}" NULL raw_video failure)
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
  expect_active_result_extension("${output}")
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

function(result_request_json_string out value)
  string(REPLACE "\\" "\\\\" encoded "${value}")
  string(REPLACE "\"" "\\\"" encoded "${encoded}")
  set(hex "0123456789abcdef")
  foreach(code RANGE 1 31)
    string(ASCII ${code} character)
    math(EXPR high "${code}/16")
    math(EXPR low "${code}%16")
    string(SUBSTRING "${hex}" ${high} 1 high)
    string(SUBSTRING "${hex}" ${low} 1 low)
    string(REPLACE "${character}" "\\u00${high}${low}" encoded "${encoded}")
  endforeach()
  set(${out} "\"${encoded}\"" PARENT_SCOPE)
endfunction()

function(check_result_json name document manifest expected)
  set(path "${scratch}/${name}.json")
  if(ARGC GREATER 4)
    set(path "${ARGV4}")
  endif()
  file(WRITE "${path}" "${document}\n")
  get_property(requests GLOBAL PROPERTY GBL_RUNNER_RESULT_REQUESTS)
  if(NOT requests)
    set(requests "[]")
  endif()
  string(JSON count LENGTH "${requests}")
  set(row "{}")
  foreach(key IN ITEMS name path manifest expected)
    result_request_json_string(encoded "${${key}}")
    string(JSON row SET "${row}" "${key}" "${encoded}")
  endforeach()
  string(JSON requests SET "${requests}" ${count} "${row}")
  set_property(GLOBAL PROPERTY GBL_RUNNER_RESULT_REQUESTS "${requests}")
endfunction()

function(check_result_report name report expected)
  string(REGEX MATCH "runner-result-batch: end:${name}:([0-9]+)" ending "${report}")
  if(NOT ending)
    message(FATAL_ERROR "runner-case: missing result check: ${name}")
  endif()
  set(findings "${CMAKE_MATCH_1}")
  if(expected STREQUAL "ok")
    if(NOT findings EQUAL 0)
      message(FATAL_ERROR
        "runner-case: ${name} should validate; output=${report}")
    endif()
  elseif(findings EQUAL 0 OR
      NOT "${report}" MATCHES "result: ${expected}(:|[ \n])")
    message(FATAL_ERROR
      "runner-case: ${name} should fail with ${expected}; output=${report}")
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
set(zero_case_rom "${fixtures}/owned-arm-pass-zero-case.gba")
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

# Exercise the actual request/validator path with JSON-special filename bytes.
# This is a local synthetic control, never a new fixture authority.
set(special_root "${scratch}/quote-\"-tab-\t")
file(MAKE_DIRECTORY "${special_root}")
file(COPY_FILE "${BIN}/generated/control/manifest.json" "${special_root}/manifest.json")
check_result_json("json-special-path" "${pass_json}"
  "${special_root}/manifest.json" ok "${special_root}/result.json")

# This generated control has a valid indexed identity and a completed PASS state,
# but case ID zero is malformed. It must run through the guest path without ever
# becoming producer PASS; a digest mismatch would leave fixture.ran false.
run_json(zero_case_json registered-zero-case-pass "${control}" 1 TIMEOUT
  cycle-budget --rom "${zero_case_rom}"
  --fixture synthetic-control-pass-zero-case --cycles 1000)
expect_json_boolean("${zero_case_json}" true fixture ran)
expect_json_field("${zero_case_json}" synthetic-control-pass-zero-case fixture id)
expect_json_field("${zero_case_json}" malformed semantic state)
expect_json_field("${zero_case_json}" 0 semantic case_id)

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

string(JSON missing_dispute_id SET "${pass_json}" disputes "[]")
expect_result_rejection("dispute-id-removed" "${missing_dispute_id}"
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
  run_json(no_bios_json "no-bios-${exception}" "${production}" 1
    GUEST_STOP "${exception_reason}" --rom "${fixtures}/owned-arm-${exception}.gba"
    --cycles 1000)
  expect_json_field("${no_bios_json}" "${exception_reason}" termination)
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

# The include API reports findings; the separate result.nc tests retain the
# script-mode nonzero-exit contract. One child checks all actual producer
# documents against each strictly validated, immutable authority just once.
get_property(requests GLOBAL PROPERTY GBL_RUNNER_RESULT_REQUESTS)
set(request_file "${scratch}/result-requests.json")
file(WRITE "${request_file}" "${requests}\n")
set(batch "${SRC}/tests/gates/runner_result_batch.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DSRC=${SRC}"
  "-DREQUESTS=${request_file}" -P "${batch}"
  RESULT_VARIABLE batch_rc OUTPUT_VARIABLE batch_output ERROR_VARIABLE batch_error)
if(NOT batch_rc STREQUAL "0")
  message(FATAL_ERROR "runner-case: result batch failed: ${batch_output}${batch_error}")
endif()
string(JSON request_count LENGTH "${requests}")
math(EXPR request_last "${request_count}-1")
foreach(index RANGE 0 ${request_last})
  string(JSON name GET "${requests}" ${index} name)
  string(JSON expected GET "${requests}" ${index} expected)
  set(begin "runner-result-batch: begin:${name}\n")
  string(FIND "${batch_error}" "${begin}" start)
  string(FIND "${batch_error}" "runner-result-batch: end:${name}:" end)
  if(start LESS 0 OR end LESS start)
    message(FATAL_ERROR "runner-case: missing result report: ${name}")
  endif()
  string(LENGTH "${batch_error}" report_length)
  math(EXPR remaining "${report_length}-${end}")
  string(SUBSTRING "${batch_error}" ${end} ${remaining} tail)
  string(FIND "${tail}" "\n" end_length)
  math(EXPR length "${end}-${start}+${end_length}")
  string(SUBSTRING "${batch_error}" ${start} ${length} report)
  check_result_report("${name}" "${report}" "${expected}")
endforeach()
foreach(control IN ITEMS changed replaced-by-symlink)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DSRC=${SRC}"
    "-DCONTROL=${control}" "-DCONTROL_ROOT=${scratch}/authority-${control}"
    -P "${batch}" RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(rc STREQUAL "0" OR NOT "${output}${error}" MATCHES "runner-result-batch: authority-changed")
    message(FATAL_ERROR "runner-case: immutable authority control failed: ${control}:${output}${error}")
  endif()
endforeach()
message("runner-case: actual producer contract, CLI status, bounds, trace and refusal matrix passed")
