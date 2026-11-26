# Result negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/result_case.cmake
#
# Every input is a mutated COPY of contracts/examples/result-pass.json (with copies of
# contracts/examples/manifest.json, registry/disputes.json, and contracts/evidence.json)
# under BIN/nc/result_<case>; the committed files are never modified and no bad example is
# committed (D-04). Each negative variant asserts a non-zero exit AND the validator's
# stable reason code through cmake/ExpectFail.cmake; a stale mutation fails with
# `mutate: no-match`. A variant changes one field; co-reported findings are accepted
# because the REGEX asserts that the targeted code line is present.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "result-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z0-9_]+$")
  message(FATAL_ERROR "result-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/result_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(CHECK "${SRC}/cmake/CheckResult.cmake")
file(COPY_FILE "${SRC}/contracts/examples/result-pass.json" "${dir}/result.json")
file(COPY_FILE "${SRC}/contracts/examples/manifest.json" "${dir}/manifest.json")
file(COPY_FILE "${SRC}/registry/disputes.json" "${dir}/disputes.json")
file(COPY_FILE "${SRC}/contracts/evidence.json" "${dir}/evidence.json")
set(REGISTRY_COPY "${dir}/disputes.json")
set(VOCAB_COPY "${dir}/evidence.json")

# result_copy(<out-var> <variant>): a fresh sandbox copy of the example result.
function(result_copy out variant)
  set(path "${dir}/result_${variant}.json")
  file(COPY_FILE "${dir}/result.json" "${path}")
  # Older contract cases intentionally exercise the pre-Phase-10 result shape.
  gbl_json_mutate_remove("${path}" phase10)
  gbl_json_mutate_remove("${path}" raw_video)
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

function(result_copy_phase10 out variant)
  set(path "${dir}/result_${variant}.json")
  file(COPY_FILE "${dir}/result.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

function(result_copy_current_producer_phase10 out variant)
  set(path "${dir}/result_${variant}.json")
  file(COPY_FILE "${dir}/result_actual_producer.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# manifest_copy(<out-var> <variant>): a fresh sandbox copy of the example manifest.
function(manifest_copy out variant)
  set(path "${dir}/manifest_${variant}.json")
  file(COPY_FILE "${dir}/manifest.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# result_command(<out-var> <result> <manifest>): the validator command line.
function(result_command out result manifest)
  set(command
    "${CMAKE_COMMAND}" "-DRESULT=${result}" "-DMANIFEST=${manifest}"
    "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}")
  if(GBL_RESULT_REQUIRE_AUDIO)
    list(APPEND command -DGBL_RESULT_REQUIRE_AUDIO=ON)
  endif()
  list(APPEND command -P "${CHECK}")
  set(${out} "${command}" PARENT_SCOPE)
endfunction()

function(result_add_audio path)
  gbl_json_mutate_set("${path}" [=[{
    "model":"gamebloke-audio-model-4",
    "profile":{"id":"agb","version":16},
    "policy_classification":"project-selected",
    "hardware_qualification":"UNKNOWN",
    "policy":{
      "tap":"pre-pwm",
      "tap_policy":"fixed-reset-anchored-grid",
      "encoding":"signed-s16le",
      "channel_layout":"stereo-interleaved-left-first",
      "sample_rate_hz":32768,
      "sample_cycles":"512",
      "interval_policy":"start-exclusive-end-inclusive",
      "implemented_sources":["psg-channel-1","psg-channel-2","psg-channel-3","psg-channel-4","direct-sound-fifo-a-signed-latch","direct-sound-fifo-b-signed-latch"],
      "unsupported":["pwm-carrier","bias-quantization","analog-output","host-resampling","host-playback"],
      "evidence_limits":["software-model-output-only","no-physical-hardware-qualification","no-broad-game-compatibility-claim","sha256-proves-byte-identity-not-semantic-correctness-or-rights"]
    },
    "requested":true,
    "requested_interval":{"start_cycle":"0","end_cycle":"1"},
    "actual_interval":{"start_cycle":"0","end_cycle":"1"},
    "cadence":{"sample_cycles":"512","format":"s16le-stereo"},
    "capture_state":"complete",
    "interval_complete":true,
    "frame_count":0,
    "byte_count":0,
    "artifact":{"state":"emitted","byte_count":0,"sha256":"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","error":null}
  }]=] audio)
endfunction()

# expect_code(<code> <result> [<manifest>]): the validator fails with <code>.
function(expect_code code result)
  set(manifest "${dir}/manifest.json")
  if(NOT "${ARGV2}" STREQUAL "")
    set(manifest "${ARGV2}")
  endif()
  result_command(cmd "${result}" "${manifest}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "result: ${code}")
endfunction()

# expect_detail(<code> <detail-regex> <result> [<manifest>]): fails with <code> and that
# finding detail (findings are printed unwrapped). A bare code can be emitted by another
# check, so a control that targets one trigger names its detail.
function(expect_detail code detail result)
  set(manifest "${dir}/manifest.json")
  if(NOT "${ARGV3}" STREQUAL "")
    set(manifest "${ARGV3}")
  endif()
  result_command(cmd "${result}" "${manifest}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "result: ${code}: ${detail}")
endfunction()

# expect_ok(<status> <result> [<manifest>]): the validator accepts with that status.
function(expect_ok status result)
  set(manifest "${dir}/manifest.json")
  if(NOT "${ARGV2}" STREQUAL "")
    set(manifest "${ARGV2}")
  endif()
  result_command(cmd "${result}" "${manifest}")
  gbl_expect_pass(COMMAND ${cmd} REGEX "result: ok \\(${status}\\)")
endfunction()

function(phase10_report_failure detail)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "1..1")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo
    "not ok 1 - Phase 10 result validator rejects malformed PPU and video claims")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "# ${detail}")
  message(FATAL_ERROR "result-case: phase10: ${detail}")
endfunction()

function(phase10_report_success)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "1..1")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo
    "ok 1 - Phase 10 result validator rejects malformed PPU and video claims")
endfunction()

function(phase10_expect_ok result_path)
  set(expected_status "PASS")
  if(NOT "${ARGV1}" STREQUAL "")
    set(expected_status "${ARGV1}")
  endif()
  result_command(cmd "${result_path}" "${dir}/manifest.json")
  execute_process(COMMAND ${cmd} RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE error)
  set(combined "${output}${error}")
  if(NOT "${result}" STREQUAL "0" OR
      NOT combined MATCHES "result: ok \\(${expected_status}\\)")
    phase10_report_failure("expected valid ${expected_status} Phase 10 result; exit=${result}; output=${combined}")
  endif()
endfunction()

function(phase10_expect_code code result_path)
  result_command(cmd "${result_path}" "${dir}/manifest.json")
  execute_process(COMMAND ${cmd} RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE error)
  set(combined "${output}${error}")
  string(FIND "${combined}" "result: ${code}" code_offset)
  if("${result}" STREQUAL "0" OR code_offset EQUAL -1)
    phase10_report_failure("expected ${code}; exit=${result}; output=${combined}")
  endif()
endfunction()

function(result_add_phase10_valid_video result)
  # One complete visible epoch ends at line 160: 160 * 1232 cycles.
  gbl_json_mutate_set("${result}" "\"197120\"" time cycles)
  gbl_json_mutate_set("${result}" "\"197120\"" time budget_cycles)
  gbl_json_mutate_set("${result}" "\"197120\"" time bus_cycles)
  gbl_json_mutate_set("${result}" "\"197120\"" phase9 machine_cycles)
  gbl_json_mutate_set("${result}" "160" phase10 raster line)
  gbl_json_mutate_set("${result}" "\"0\"" phase10 raster line_cycle)
  gbl_json_mutate_set("${result}" "\"197120\"" phase10 raster machine_cycle)
  gbl_json_mutate_set("${result}" "\"valid\"" phase10 frame state)
  gbl_json_mutate_set("${result}" "\"none\"" phase10 frame reason)
  gbl_json_mutate_set("${result}" "true" phase10 frame supported_controls)
  gbl_json_mutate_set("${result}" "\"0\"" phase10 frame epoch)
  gbl_json_mutate_set("${result}" "\"0\"" phase10 frame sample_cycle)
  gbl_json_mutate_set("${result}" "\"197120\"" phase10 frame completion_cycle)
  gbl_json_mutate_set("${result}" "true" phase10 controls supported)
  gbl_json_mutate_set("${result}" "true" raw_video requested)
  gbl_json_mutate_set("${result}" "\"emitted\"" raw_video status)
  gbl_json_mutate_set("${result}" "true" raw_video emitted)
  gbl_json_mutate_set("${result}" "76800" raw_video byte_length)
  gbl_json_mutate_set("${result}" "\"abababababababababababababababababababababababababababababababab\"" raw_video sha256)
endfunction()

function(result_add_phase10_output_failure result)
  gbl_json_mutate_set("${result}" "\"HARNESS_FAIL\"" status)
  gbl_json_mutate_set("${result}" "\"video-output-failed\"" reason)
  gbl_json_mutate_set("${result}" "true" raw_video requested)
  gbl_json_mutate_set("${result}" "\"failed\"" raw_video status)
  gbl_json_mutate_set("${result}" "\"video-frame-unavailable\"" raw_video failure)
endfunction()

function(result_add_phase10_hblank_dependency result)
  gbl_json_mutate_set("${result}" "true" phase10 hblank_trigger_dependency observed)
  gbl_json_mutate_set("${result}" "\"disputed\"" phase10 hblank_trigger_dependency classification)
  gbl_json_mutate_set("${result}" "\"disp-hblank-flag-edge\"" phase10 hblank_trigger_dependency dispute_id)
  gbl_json_mutate_set("${result}" "[\"disp-hblank-flag-edge\"]" disputes)
  gbl_json_mutate_set("${result}" "[\"disputed\"]" labels)
endfunction()

function(result_add_phase10_capabilities result)
  file(READ "${SRC}/profiles/agb.json" profile_doc)
  string(JSON profile_version GET "${profile_doc}" profile_version)
  string(JSON display_capabilities GET "${profile_doc}"
    ppu_policy display_capabilities)
  string(JSON video_memory_access GET "${profile_doc}"
    ppu_policy video_memory_access)
  gbl_json_mutate_set("${result}" "\"gamebloke-ppu-observation-2\""
    phase10 model)
  gbl_json_mutate_set("${result}" "${profile_version}"
    phase10 profile_version)
  gbl_json_mutate_set("${result}" "${display_capabilities}"
    phase10 display_capabilities)
  gbl_json_mutate_set("${result}" "${video_memory_access}"
    phase10 video_memory_access)
  gbl_json_mutate_set("${result}" "\"bgr555-le\"" raw_video format)
endfunction()

function(result_copy_production_phase10_observation result)
  set(control "${BIN}/gamebloke-run-control")
  set(rom "${BIN}/generated/fixtures/owned-arm-pass.gba")
  if(NOT EXISTS "${control}" OR NOT EXISTS "${rom}")
    phase10_report_failure("production observation prerequisites are missing")
  endif()
  execute_process(COMMAND "${control}" --rom "${rom}"
      --fixture synthetic-control-pass --cycles 1000
    RESULT_VARIABLE producer_result OUTPUT_VARIABLE producer_json
    ERROR_VARIABLE producer_error)
  if(NOT "${producer_result}" STREQUAL "0" OR
      NOT "${producer_error}" STREQUAL "")
    phase10_report_failure(
      "production capture failed; exit=${producer_result}; stderr=${producer_error}")
  endif()
  string(JSON producer_model GET "${producer_json}" phase10 model)
  if(NOT producer_model STREQUAL "gamebloke-ppu-observation-2")
    phase10_report_failure("production capture did not emit observation model 2")
  endif()

  # The runner's synthetic fixture identity is not in the public result manifest.
  # Keep the manifest-owned contract identity from result-pass.json and copy the
  # complete machine observation group from the real serialized producer output.
  file(READ "${result}" result_doc)
  foreach(field IN ITEMS semantic time phase8 phase9 phase10 raw_video)
    string(JSON value GET "${producer_json}" ${field})
    string(JSON result_doc ERROR_VARIABLE set_error SET
      "${result_doc}" ${field} "${value}")
    if(NOT "${set_error}" STREQUAL "NOTFOUND" AND
        NOT "${set_error}" STREQUAL "")
      phase10_report_failure("could not copy production field ${field}")
    endif()
  endforeach()
  file(WRITE "${result}" "${result_doc}\n")
endfunction()

function(result_add_time_extension result)
  gbl_json_mutate_remove("${result}" phase8)
  gbl_json_mutate_remove("${result}" phase9)
  gbl_json_mutate_remove("${result}" time wait_cycles)
  foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
      dma_destination_cycles dma_completed_units dma_pause_cycles)
    gbl_json_mutate_remove("${result}" time ${field})
  endforeach()
  foreach(field IN ITEMS model qualification bus_cycles internal_cycles)
    gbl_json_mutate_remove("${result}" time ${field})
  endforeach()
  gbl_json_mutate_set("${result}" "{}" timer0)
  gbl_json_mutate_set("${result}" "\"gamebloke-timer-model-1\"" time model)
  gbl_json_mutate_set("${result}" "\"project-model-only\"" time qualification)
  gbl_json_mutate_set("${result}" "\"10\"" time bus_cycles)
  gbl_json_mutate_set("${result}" "\"14\"" time internal_cycles)
  gbl_json_mutate_set("${result}" "true" timer0 if_request)
endfunction()

function(result_add_phase8_extension result)
  result_add_time_extension("${result}")
  gbl_json_mutate_set("${result}" "\"24\"" time bus_cycles)
  gbl_json_mutate_set("${result}" "\"0\"" time internal_cycles)
  gbl_json_mutate_set("${result}"
    "{\"model\":\"gamebloke-irq-input-model-1\",\"qualification\":\"project-model-only\",\"observed\":true,\"irq\":{\"if\":8,\"ie\":8,\"ime\":1,\"sample\":{\"cycle\":\"4\",\"if\":0,\"ie\":8,\"ime\":1,\"eligible_sources\":0,\"eligible\":false,\"cpsr_i_masked\":false,\"accepted\":false,\"entered\":false}},\"last_exception\":\"none\",\"power\":{\"state\":\"running\",\"cause\":\"wake_event\",\"wake_source_mask\":8,\"wake_cycle\":\"5\"},\"keypad\":{\"keyinput\":1022,\"keycnt\":0,\"condition\":false,\"irq_condition\":false,\"applied_events\":1,\"pending_events\":0},\"wait_cycles\":\"4\"}"
    phase8)
  gbl_json_mutate_set("${result}" "\"0\"" phase8 wait_cycles)
endfunction()

function(result_add_phase9_extension result)
  result_add_phase8_extension("${result}")
  gbl_json_mutate_set("${result}" "\"0\"" time wait_cycles)
  gbl_json_mutate_set("${result}" "\"0\"" time dma_startup_cycles)
  gbl_json_mutate_set("${result}" "\"0\"" time dma_source_cycles)
  gbl_json_mutate_set("${result}" "\"0\"" time dma_destination_cycles)
  gbl_json_mutate_set("${result}" "\"0\"" time dma_completed_units)
  gbl_json_mutate_set("${result}" "\"0\"" time dma_pause_cycles)
  gbl_json_mutate_set("${result}"
    "{\"model\":\"gamebloke-dma-observation-1\",\"observed\":true,\"waitcnt\":0,\"bus_owner\":\"cpu\",\"cpu_paused\":false,\"machine_cycles\":\"24\",\"channels\":[{\"channel\":0,\"source_address\":\"0x00000000\",\"destination_address\":\"0x00000000\",\"current_source\":\"0x00000000\",\"current_destination\":\"0x00000000\",\"read_latch\":\"0x00000000\",\"count\":0,\"control\":0,\"remaining\":0,\"completed_units\":\"0\",\"pending\":false,\"armed\":false,\"active\":false,\"startup_complete\":false},{\"channel\":1,\"source_address\":\"0x00000000\",\"destination_address\":\"0x00000000\",\"current_source\":\"0x00000000\",\"current_destination\":\"0x00000000\",\"read_latch\":\"0x00000000\",\"count\":0,\"control\":0,\"remaining\":0,\"completed_units\":\"0\",\"pending\":false,\"armed\":false,\"active\":false,\"startup_complete\":false},{\"channel\":2,\"source_address\":\"0x00000000\",\"destination_address\":\"0x00000000\",\"current_source\":\"0x00000000\",\"current_destination\":\"0x00000000\",\"read_latch\":\"0x00000000\",\"count\":0,\"control\":0,\"remaining\":0,\"completed_units\":\"0\",\"pending\":false,\"armed\":false,\"active\":false,\"startup_complete\":false},{\"channel\":3,\"source_address\":\"0x00000000\",\"destination_address\":\"0x00000000\",\"current_source\":\"0x00000000\",\"current_destination\":\"0x00000000\",\"read_latch\":\"0x00000000\",\"count\":0,\"control\":0,\"remaining\":0,\"completed_units\":\"0\",\"pending\":false,\"armed\":false,\"active\":false,\"startup_complete\":false}],\"timing\":{\"dma_startup_cycles\":\"0\",\"dma_source_cycles\":\"0\",\"dma_destination_cycles\":\"0\",\"dma_completed_units\":\"0\",\"dma_pause_cycles\":\"0\"},\"waitcnt_policy\":{\"manual_waits\":\"nintendo-agb-programming-manual-v1.1-sections-3.3-3.3.2\",\"other_region_costs\":\"project-selected\",\"128k_boundary_source\":\"gbatek-3.06-community-documentation\",\"hardware_qualification\":\"UNKNOWN\"},\"startup_policy\":{\"cycles_per_request\":\"2\",\"provenance\":\"project-selected\",\"manual_activation_delay\":\"described; conversion is selected\",\"hardware_qualification\":\"UNKNOWN\"},\"latch_policy\":{\"policy\":\"per-channel-retained-latch\",\"provenance\":\"project-selected\",\"fallback_observed\":false,\"classification\":null,\"dispute_id\":null,\"hardware_qualification\":\"UNKNOWN\"},\"prefetch\":\"unmodeled\"}"
    phase9)
endfunction()

function(result_add_phase9_dma_transfer result)
  result_add_phase9_extension("${result}")
  gbl_json_mutate_set("${result}" "\"32\"" time cycles)
  gbl_json_mutate_set("${result}" "\"32\"" time budget_cycles)
  gbl_json_mutate_set("${result}" "\"2\"" time dma_startup_cycles)
  gbl_json_mutate_set("${result}" "\"5\"" time dma_source_cycles)
  gbl_json_mutate_set("${result}" "\"1\"" time dma_destination_cycles)
  gbl_json_mutate_set("${result}" "\"1\"" time dma_completed_units)
  gbl_json_mutate_set("${result}" "\"8\"" time dma_pause_cycles)
  gbl_json_mutate_set("${result}" "\"32\"" phase9 machine_cycles)
  foreach(pair IN ITEMS
      "dma_startup_cycles;2" "dma_source_cycles;5" "dma_destination_cycles;1"
      "dma_completed_units;1" "dma_pause_cycles;8")
    list(GET pair 0 field)
    list(GET pair 1 value)
    gbl_json_mutate_set("${result}" "\"${value}\"" phase9 timing "${field}")
  endforeach()
  gbl_json_mutate_set("${result}"
    "{\"channel\":3,\"source_address\":\"0x08000200\",\"destination_address\":\"0x03000020\",\"current_source\":\"0x08000202\",\"current_destination\":\"0x03000022\",\"read_latch\":\"0xBEEFBEEF\",\"count\":1,\"control\":0,\"remaining\":0,\"completed_units\":\"1\",\"pending\":false,\"armed\":false,\"active\":false,\"startup_complete\":false}"
    phase9 channels 3)
endfunction()

function(result_add_phase9_latch_observation result)
  result_add_phase9_dma_transfer("${result}")
  gbl_json_mutate_set("${result}" "true" phase9 latch_policy fallback_observed)
  gbl_json_mutate_set("${result}" "\"disputed\"" phase9 latch_policy classification)
  gbl_json_mutate_set("${result}" "\"disp-dma-latch\"" phase9 latch_policy dispute_id)
  gbl_json_mutate_set("${result}" "[\"disp-dma-latch\"]" disputes)
  gbl_json_mutate_set("${result}" "[\"disputed\"]" labels)
  gbl_json_mutate_set("${result}"
    "{\"mask\":\"8\",\"count\":\"4\",\"capacity\":\"4\",\"complete\":true,\"emitted\":true}"
    trace)
endfunction()

function(result_phase8_set_not_run result)
  gbl_json_mutate_set("${result}" "false" phase8 observed)
  foreach(path IN ITEMS irq/if irq/ie irq/ime irq/sample last_exception
      power/state power/cause power/wake_source_mask power/wake_cycle
      keypad/keyinput keypad/keycnt keypad/condition keypad/irq_condition
      keypad/applied_events keypad/pending_events wait_cycles)
    string(REPLACE "/" ";" keys "${path}")
    file(READ "${result}" phase8_json)
    string(JSON phase8_field_type ERROR_VARIABLE phase8_field_error TYPE
      "${phase8_json}" phase8 ${keys})
    if(NOT "${phase8_field_error}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "result-case: not-run phase8 field missing: ${path}")
    endif()
    if(NOT phase8_field_type STREQUAL "NULL")
      gbl_json_mutate_set("${result}" "null" phase8 ${keys})
    endif()
  endforeach()
endfunction()

function(result_phase9_set_not_run result)
  gbl_json_mutate_set("${result}" "false" phase9 observed)
  gbl_json_mutate_set("${result}" "null" phase9 latch_policy fallback_observed)
  foreach(path IN ITEMS waitcnt bus_owner cpu_paused machine_cycles channels)
    string(REPLACE "/" ";" keys "${path}")
    gbl_json_mutate_set("${result}" "null" phase9 ${keys})
  endforeach()
endfunction()

# hex(<out-var> <fragment> <times>): a synthesized hex string.
function(hex out fragment times)
  string(REPEAT "${fragment}" ${times} value)
  set(${out} "${value}" PARENT_SCOPE)
endfunction()

if(CASE STREQUAL "instruction_reasons")
  # Descriptive GUEST_STOP tokens were already permitted by result/1. No
  # vocabulary/schema expansion is needed for the append-only CPU reasons.
  foreach(reason IN ITEMS unpredictable-instruction later-architecture-instruction)
    result_copy(r ${reason})
    gbl_json_mutate_set("${r}" "\"GUEST_STOP\"" status)
    gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
    gbl_json_mutate_set("${r}" "\"${reason}\"" reason)
    gbl_json_mutate_set("${r}" "\"${reason}\"" termination)
    expect_ok(GUEST_STOP "${r}")
  endforeach()

elseif(CASE STREQUAL "pass_without_run")
  # The fixture did not run, but the digest and everything else say PASS.
  result_copy(r no_run)
  gbl_json_mutate_set("${r}" "false" fixture ran)
  expect_code(pass-without-run "${r}")

elseif(CASE STREQUAL "all_states")
  # Every one of the nine states has a minimal accepted document, so none is unreachable.
  # The names and rules are written out here on purpose: this is the independent oracle
  # for the data the validator reads.
  foreach(state IN ITEMS PASS GUEST_FAIL GUEST_STOP HARNESS_FAIL TIMEOUT CANCELLED
      MISSING SKIPPED REFUSED)
    result_copy(r state_${state})
    if(NOT state STREQUAL "PASS")
      gbl_json_mutate_set("${r}" "\"${state}\"" status)
    endif()
    if(state STREQUAL "GUEST_FAIL")
      gbl_json_mutate_set("${r}" "\"fail\"" semantic state)
      gbl_json_mutate_set("${r}" "\"2\"" semantic case_id)
    elseif(state STREQUAL "GUEST_STOP")
      gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
      gbl_json_mutate_set("${r}" "\"3\"" semantic case_id)
      gbl_json_mutate_set("${r}" "\"no-bios-swi\"" reason)
      gbl_json_mutate_set("${r}" "\"no-bios-swi\"" termination)
    elseif(state STREQUAL "HARNESS_FAIL")
      gbl_json_mutate_set("${r}" "\"trace-output-full\"" reason)
      gbl_json_mutate_set("${r}" "\"trace-output-full\"" termination)
      gbl_json_mutate_set("${r}" "{\"complete\": false}" trace)
    elseif(state STREQUAL "TIMEOUT")
      gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
      gbl_json_mutate_set("${r}" "\"3\"" semantic case_id)
      gbl_json_mutate_set("${r}" "\"cycle-budget\"" reason)
      gbl_json_mutate_set("${r}" "\"timeout\"" termination)
      gbl_json_mutate_set("${r}" "\"23\"" time cycles)
      gbl_json_mutate_set("${r}" "\"23\"" time bus_cycles)
      gbl_json_mutate_set("${r}" "\"23\"" phase9 machine_cycles)
    elseif(state STREQUAL "CANCELLED")
      gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
      gbl_json_mutate_set("${r}" "\"3\"" semantic case_id)
      gbl_json_mutate_set("${r}" "\"cancelled\"" reason)
      gbl_json_mutate_set("${r}" "\"cancelled\"" termination)
      gbl_json_mutate_set("${r}" "\"1\"" time cycles)
      gbl_json_mutate_set("${r}" "\"1\"" time bus_cycles)
      gbl_json_mutate_set("${r}" "\"1\"" phase9 machine_cycles)
    endif()
    if(state STREQUAL "MISSING" OR state STREQUAL "SKIPPED" OR state STREQUAL "REFUSED")
      gbl_json_mutate_set("${r}" "false" fixture ran)
      gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
      gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
      gbl_json_mutate_set("${r}" "\"absent\"" semantic state)
      gbl_json_mutate_set("${r}" "null" semantic case_id)
      gbl_json_mutate_set("${r}" "\"0\"" time cycles)
      gbl_json_mutate_set("${r}" "\"0\"" time instructions)
      gbl_json_mutate_set("${r}" "\"0\"" time budget_cycles)
      gbl_json_mutate_set("${r}" "\"0\"" time bus_cycles)
      gbl_json_mutate_set("${r}" "null" timer0 if_request)
      result_phase8_set_not_run("${r}")
      result_phase9_set_not_run("${r}")
      if(state STREQUAL "MISSING")
        gbl_json_mutate_set("${r}" "\"rom-file-not-found\"" reason)
        gbl_json_mutate_set("${r}" "\"not-started\"" termination)
      elseif(state STREQUAL "SKIPPED")
        gbl_json_mutate_set("${r}" "\"required-bios-unavailable\"" reason)
        gbl_json_mutate_set("${r}" "\"skipped-before-run\"" termination)
      else()
        gbl_json_mutate_set("${r}" "\"declared-extent\"" reason)
        gbl_json_mutate_set("${r}" "\"refused-before-run\"" termination)
      endif()
    endif()
    if(state STREQUAL "MISSING" OR state STREQUAL "SKIPPED")
      gbl_json_mutate_set("${r}" "\"the fixture was not built in this run\"" reason)
    endif()
    if(state STREQUAL "REFUSED")
      gbl_json_mutate_set("${r}"
        "{\"code\": \"declared-extent\", \"postcondition\": \"no state was changed\"}" refusal)
    endif()
    expect_ok("${state}" "${r}")
  endforeach()

elseif(CASE STREQUAL "semantic_time")
  # The exact budget boundary is inclusive; the checked-in example sits at that edge.
  result_copy(r exact_boundary)
  expect_ok(PASS "${r}")
  result_copy(r under_budget)
  gbl_json_mutate_set("${r}" "\"25\"" time budget_cycles)
  expect_ok(PASS "${r}")
  result_copy(r max_uint64_boundary)
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" phase9 machine_cycles)
  expect_ok(PASS "${r}")

  # Historical result/1 remains valid with no extension. Present extensions are
  # all-or-nothing and carry independently typed model, component, and IF fields.
  result_copy(r model_extension_valid)
  result_add_time_extension("${r}")
  expect_ok(PASS "${r}")
  result_copy(r model_extension_missing_model)
  result_add_time_extension("${r}")
  gbl_json_mutate_remove("${r}" time model)
  expect_detail(missing-field "time.model" "${r}")
  result_copy(r model_extension_missing_request)
  result_add_time_extension("${r}")
  gbl_json_mutate_remove("${r}" timer0 if_request)
  expect_detail(missing-field "timer0.if_request" "${r}")

  result_copy(r model_extension_max_uint64)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "\"0\"" time internal_cycles)
  expect_ok(PASS "${r}")
  result_copy(r model_extension_bus_overflow)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"18446744073709551616\"" time bus_cycles)
  expect_detail(time-overflow "time.bus_cycles" "${r}")
  result_copy(r model_extension_sum_overflow)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "\"1\"" time internal_cycles)
  expect_detail(time-sum-overflow "time.bus_cycles \\+ time.internal_cycles" "${r}")
  result_copy(r model_extension_mismatched_total)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"11\"" time bus_cycles)
  expect_detail(bad-time-components "time.cycles does not equal" "${r}")
  result_copy(r model_extension_numeric_bus)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "10" time bus_cycles)
  expect_detail(bad-time "time.bus_cycles must be a decimal string" "${r}")
  result_copy(r model_extension_internal_overflow)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"18446744073709551616\"" time internal_cycles)
  expect_detail(time-overflow "time.internal_cycles" "${r}")
  result_copy(r wrong_time_model)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"hardware-gba-v1\"" time model)
  expect_detail(bad-time-model "time.model" "${r}")
  result_copy(r inflated_time_qualification)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"hardware-qualified\"" time qualification)
  expect_detail(bad-time-qualification "time.qualification" "${r}")
  result_copy(r wrong_if_request_type)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "0" timer0 if_request)
  expect_detail(bad-timer0-request "timer0.if_request must be boolean or null" "${r}")

  result_copy(r zero_budget_request_null)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"TIMEOUT\"" status)
  gbl_json_mutate_set("${r}" "\"cycle-budget\"" reason)
  gbl_json_mutate_set("${r}" "\"timeout\"" termination)
  gbl_json_mutate_set("${r}" "\"absent\"" semantic state)
  gbl_json_mutate_set("${r}" "\"0\"" semantic case_id)
  gbl_json_mutate_set("${r}" "\"0\"" time cycles)
  gbl_json_mutate_set("${r}" "\"0\"" time instructions)
  gbl_json_mutate_set("${r}" "\"0\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"0\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "\"0\"" time internal_cycles)
  gbl_json_mutate_set("${r}" "null" timer0 if_request)
  expect_ok(TIMEOUT "${r}")

  result_copy(r real_timeout_request_clear)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"TIMEOUT\"" status)
  gbl_json_mutate_set("${r}" "\"cycle-budget\"" reason)
  gbl_json_mutate_set("${r}" "\"timeout\"" termination)
  gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
  gbl_json_mutate_set("${r}" "\"3\"" semantic case_id)
  gbl_json_mutate_set("${r}" "false" timer0 if_request)
  expect_ok(TIMEOUT "${r}")

  result_copy(r not_run_request_boolean)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"CANCELLED\"" status)
  gbl_json_mutate_set("${r}" "false" fixture ran)
  gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
  gbl_json_mutate_set("${r}" "\"not-started\"" termination)
  gbl_json_mutate_set("${r}" "\"absent\"" semantic state)
  gbl_json_mutate_set("${r}" "null" semantic case_id)
  gbl_json_mutate_set("${r}" "\"0\"" time cycles)
  gbl_json_mutate_set("${r}" "\"0\"" time instructions)
  gbl_json_mutate_set("${r}" "\"0\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"0\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "\"0\"" time internal_cycles)
  expect_detail(bad-timer0-request "not-run results require null" "${r}")

  result_copy(r request_does_not_complete_semantics)
  result_add_time_extension("${r}")
  gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
  expect_detail(status-semantic-mismatch "PASS requires semantic.state pass" "${r}")

  result_copy(r pending_pass)
  gbl_json_mutate_set("${r}" "\"pending\"" semantic state)
  expect_detail(status-semantic-mismatch "PASS requires semantic.state pass" "${r}")
  result_copy(r bad_case_id)
  gbl_json_mutate_set("${r}" "1" semantic case_id)
  expect_detail(bad-semantic "semantic.case_id must be a string or null" "${r}")
  result_copy(r noncanonical_case_id)
  gbl_json_mutate_set("${r}" "\"01\"" semantic case_id)
  expect_detail(bad-semantic "semantic.case_id" "${r}")
  result_copy(r case_id_overflow)
  gbl_json_mutate_set("${r}" "\"4294967296\"" semantic case_id)
  expect_detail(bad-semantic "semantic.case_id exceeds uint32" "${r}")
  result_copy(r absent_case_id)
  gbl_json_mutate_set("${r}" "\"0\"" semantic case_id)
  expect_detail(bad-semantic "completed mailbox needs a nonzero case_id" "${r}")

  result_copy(r numeric_cycles)
  gbl_json_mutate_set("${r}" "24" time cycles)
  expect_detail(bad-time "time.cycles must be a decimal string" "${r}")
  result_copy(r noncanonical_cycles)
  gbl_json_mutate_set("${r}" "\"01\"" time cycles)
  expect_detail(bad-time "time.cycles" "${r}")
  result_copy(r overflow_cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551616\"" time cycles)
  expect_detail(time-overflow "time.cycles" "${r}")
  result_copy(r over_budget)
  gbl_json_mutate_set("${r}" "\"25\"" time cycles)
  expect_detail(cycles-exceed-budget "time.cycles exceeds time.budget_cycles" "${r}")

  result_copy(r bad_stop_termination)
  gbl_json_mutate_set("${r}" "\"GUEST_STOP\"" status)
  gbl_json_mutate_set("${r}" "\"no-bios-swi\"" reason)
  gbl_json_mutate_set("${r}" "\"timeout\"" termination)
  expect_detail(bad-termination "no-BIOS GUEST_STOP termination must match reason" "${r}")
  result_copy(r missing_stop_reason)
  gbl_json_mutate_set("${r}" "\"GUEST_STOP\"" status)
  gbl_json_mutate_set("${r}" "\"no-bios-swi\"" termination)
  gbl_json_mutate_set("${r}" "\"no-bios-swi\"" reason)
  gbl_json_mutate_remove("${r}" reason)
  expect_detail(bad-termination "no-BIOS GUEST_STOP termination must match reason" "${r}")
  result_copy(r incomplete_pass)
  gbl_json_mutate_set("${r}" "{\"complete\": false}" trace)
  expect_detail(incomplete-trace "PASS cannot have an incomplete trace" "${r}")
  result_copy(r complete_trace_full)
  gbl_json_mutate_set("${r}" "\"HARNESS_FAIL\"" status)
  gbl_json_mutate_set("${r}" "\"trace-output-full\"" reason)
  gbl_json_mutate_set("${r}" "\"trace-output-full\"" termination)
  gbl_json_mutate_set("${r}" "{\"complete\": true}" trace)
  expect_detail(incomplete-trace "trace-output-full HARNESS_FAIL requires trace.complete false" "${r}")
  result_copy(r bad_trace_type)
  gbl_json_mutate_set("${r}" "false" trace)
  expect_detail(bad-trace "trace must be an object or null" "${r}")
  result_copy(r bad_termination_type)
  gbl_json_mutate_set("${r}" "[]" termination)
  expect_detail(bad-termination "termination must be a string" "${r}")
  result_copy(r trace_full_wrong_status)
  gbl_json_mutate_set("${r}" "\"GUEST_STOP\"" status)
  gbl_json_mutate_set("${r}" "\"other-stop\"" reason)
  gbl_json_mutate_set("${r}" "\"trace-output-full\"" termination)
  gbl_json_mutate_set("${r}" "{\"complete\": false}" trace)
  expect_detail(bad-termination
    "trace-output-full requires HARNESS_FAIL with matching reason" "${r}")
  result_copy(r incomplete_non_harness_trace)
  gbl_json_mutate_set("${r}" "\"GUEST_STOP\"" status)
  gbl_json_mutate_set("${r}" "\"no-bios-swi\"" reason)
  gbl_json_mutate_set("${r}" "\"no-bios-swi\"" termination)
  gbl_json_mutate_set("${r}" "{\"complete\": false}" trace)
  expect_detail(incomplete-trace
    "incomplete trace requires trace-output-full HARNESS_FAIL" "${r}")

elseif(CASE STREQUAL "phase9")
  # Historical result/1 remains valid when the whole optional DMA extension is absent.
  result_copy(r historical_without_phase9)
  gbl_json_mutate_remove("${r}" phase9)
  foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
      dma_destination_cycles dma_completed_units dma_pause_cycles)
    gbl_json_mutate_remove("${r}" time ${field})
  endforeach()
  expect_ok(PASS "${r}")

  result_copy(r current_phase9)
  result_add_phase9_extension("${r}")
  expect_ok(PASS "${r}")

  result_copy(r current_phase9_dma_transfer)
  result_add_phase9_dma_transfer("${r}")
  expect_ok(PASS "${r}")

  result_copy(r current_phase9_reactivated_channel)
  result_add_phase9_dma_transfer("${r}")
  # The final channel snapshot describes its latest activation, while the
  # run-level total includes both activations of the same channel.
  gbl_json_mutate_set("${r}" "\"2\"" time dma_completed_units)
  gbl_json_mutate_set("${r}" "\"2\"" phase9 timing dma_completed_units)
  expect_ok(PASS "${r}")

  result_copy(r current_phase9_pending_dma_startup)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "32768" phase9 channels 3 control)
  gbl_json_mutate_set("${r}" "1" phase9 channels 3 remaining)
  gbl_json_mutate_set("${r}" "true" phase9 channels 3 pending)
  expect_ok(PASS "${r}")

  result_copy(r current_phase9_with_disputed_latch)
  result_add_phase9_latch_observation("${r}")
  expect_ok(PASS "${r}")

elseif(CASE STREQUAL "phase9_negative")
  result_copy(r phase9_missing_model)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_remove("${r}" phase9 model)
  expect_detail(missing-field "phase9.model" "${r}")

  result_copy(r phase9_wrong_waitcnt_type)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "\"0\"" phase9 waitcnt)
  expect_detail(bad-phase9-type "phase9.waitcnt" "${r}")

  result_copy(r phase9_time_overflow)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "\"18446744073709551616\""
    time dma_startup_cycles)
  expect_detail(time-overflow "time.dma_startup_cycles" "${r}")

  result_copy(r phase9_total_overflow)
  result_add_phase9_dma_transfer("${r}")
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "\"1\"" time internal_cycles)
  expect_detail(time-sum-overflow "CPU, HALT, and DMA timing components" "${r}")

  result_copy(r phase9_pause_exceeds_elapsed)
  result_add_phase9_dma_transfer("${r}")
  gbl_json_mutate_set("${r}" "\"33\"" time dma_pause_cycles)
  gbl_json_mutate_set("${r}" "\"33\"" phase9 timing dma_pause_cycles)
  expect_detail(bad-phase9-timing "time.dma_pause_cycles exceeds time.cycles" "${r}")

  result_copy(r phase9_pause_component_mismatch)
  result_add_phase9_dma_transfer("${r}")
  gbl_json_mutate_set("${r}" "\"7\"" time dma_pause_cycles)
  gbl_json_mutate_set("${r}" "\"7\"" phase9 timing dma_pause_cycles)
  expect_detail(bad-phase9-timing
    "time.dma_pause_cycles differs from DMA timing components" "${r}")

  result_copy(r phase9_wrong_total)
  result_add_phase9_dma_transfer("${r}")
  gbl_json_mutate_set("${r}" "\"31\"" time cycles)
  gbl_json_mutate_set("${r}" "\"31\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"31\"" phase9 machine_cycles)
  expect_detail(bad-time-components "time.cycles does not equal" "${r}")

  result_copy(r phase9_unknown_owner)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "\"host\"" phase9 bus_owner)
  expect_detail(bad-phase9-owner "phase9.bus_owner" "${r}")

  result_copy(r phase9_owner_pause_mismatch)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "\"dma\"" phase9 bus_owner)
  expect_detail(bad-phase9-owner "phase9.cpu_paused must match the bus owner" "${r}")

  result_copy(r phase9_invalid_channel)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "4" phase9 channels 3 channel)
  expect_detail(bad-phase9-channel "phase9.channels" "${r}")

  result_copy(r phase9_inconsistent_progress)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "true" phase9 channels 3 active)
  expect_detail(bad-phase9-progress "phase9.channels" "${r}")

  result_copy(r phase9_completed_unit_total_mismatch)
  result_add_phase9_dma_transfer("${r}")
  gbl_json_mutate_set("${r}" "\"2\"" time dma_completed_units)
  gbl_json_mutate_set("${r}" "\"3\"" phase9 timing dma_completed_units)
  expect_detail(bad-phase9-timing
    "phase9.timing.dma_completed_units differs from time.dma_completed_units" "${r}")

  result_copy(r phase9_final_channel_total_exceeds_run)
  result_add_phase9_dma_transfer("${r}")
  gbl_json_mutate_set("${r}" "\"0\"" time dma_completed_units)
  gbl_json_mutate_set("${r}" "\"0\"" phase9 timing dma_completed_units)
  expect_detail(bad-phase9-progress
    "final channel activation completions exceed time.dma_completed_units" "${r}")

  result_copy(r phase9_inconsistent_component)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "\"6\"" phase9 timing dma_source_cycles)
  expect_detail(bad-phase9-timing "phase9.timing.dma_source_cycles" "${r}")

  result_copy(r phase9_wait_component_mismatch)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "\"1\"" time wait_cycles)
  expect_detail(bad-phase9-timing "time.wait_cycles differs from phase8.wait_cycles" "${r}")

  result_copy(r phase9_missing_latch_link)
  result_add_phase9_latch_observation("${r}")
  gbl_json_mutate_set("${r}" "[]" disputes)
  gbl_json_mutate_set("${r}" "[]" labels)
  expect_detail(undisclosed-dispute "disp-dma-latch" "${r}")

  result_copy(r phase9_settled_latch)
  result_add_phase9_latch_observation("${r}")
  gbl_json_mutate_set("${r}" "\"settled\"" phase9 latch_policy classification)
  gbl_json_mutate_set("${r}" "\"QUALIFIED\"" phase9 latch_policy hardware_qualification)
  expect_detail(bad-phase9-latch "latch fallback must remain disputed and UNKNOWN" "${r}")

  result_copy(r phase9_modeled_prefetch)
  result_add_phase9_extension("${r}")
  gbl_json_mutate_set("${r}" "\"modeled\"" phase9 prefetch)
  expect_detail(bad-phase9-prefetch "phase9.prefetch" "${r}")

  result_copy(r phase9_partial_without_object)
  gbl_json_mutate_remove("${r}" phase9)
  foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
      dma_destination_cycles dma_completed_units dma_pause_cycles)
    gbl_json_mutate_remove("${r}" time ${field})
  endforeach()
  gbl_json_mutate_set("${r}" "\"0\"" time dma_startup_cycles)
  expect_detail(partial-phase9-extension "time DMA fields require phase9" "${r}")

elseif(CASE STREQUAL "audio_policy")
  # A historical result/1 receipt without audio remains valid by default.
  expect_ok(PASS "${dir}/result.json")
  set(GBL_RESULT_REQUIRE_AUDIO ON)
  expect_code(bad-audio "${dir}/result.json")

  result_copy(r current_audio_policy)
  result_add_audio("${r}")
  expect_ok(PASS "${r}")

  result_copy(r incomplete_audio_capture)
  result_add_audio("${r}")
  gbl_json_mutate_set("${r}" "\"30\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"25\"" audio requested_interval end_cycle)
  gbl_json_mutate_set("${r}" "\"24\"" audio actual_interval end_cycle)
  gbl_json_mutate_set("${r}" "\"incomplete\"" audio capture_state)
  gbl_json_mutate_set("${r}" "false" audio interval_complete)
  gbl_json_mutate_set("${r}" "\"not-written\"" audio artifact state)
  gbl_json_mutate_set("${r}" "null" audio artifact byte_count)
  gbl_json_mutate_set("${r}" "null" audio artifact sha256)
  expect_ok(PASS "${r}")

  result_copy(r failed_audio_setup)
  result_add_audio("${r}")
  gbl_json_mutate_set("${r}" "\"30\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"25\"" audio requested_interval end_cycle)
  gbl_json_mutate_set("${r}" "null" audio actual_interval)
  gbl_json_mutate_set("${r}" "\"failed\"" audio capture_state)
  gbl_json_mutate_set("${r}" "null" audio interval_complete)
  gbl_json_mutate_set("${r}" "null" audio frame_count)
  gbl_json_mutate_set("${r}" "null" audio byte_count)
  gbl_json_mutate_set("${r}" "\"not-written\"" audio artifact state)
  gbl_json_mutate_set("${r}" "null" audio artifact byte_count)
  gbl_json_mutate_set("${r}" "null" audio artifact sha256)
  expect_ok(PASS "${r}")

  result_copy(r wrong_audio_profile)
  result_add_audio("${r}")
  gbl_json_mutate_set("${r}" "15" audio profile version)
  expect_code(bad-audio "${r}")

  result_copy(r qualified_audio_claim)
  result_add_audio("${r}")
  gbl_json_mutate_set("${r}" "\"QUALIFIED\"" audio hardware_qualification)
  expect_code(bad-audio "${r}")

  result_copy(r audio_interval_over_budget)
  result_add_audio("${r}")
  gbl_json_mutate_set("${r}" "\"25\"" audio requested_interval end_cycle)
  expect_code(bad-audio "${r}")

  result_copy(r audio_frame_count_mismatch)
  result_add_audio("${r}")
  gbl_json_mutate_set("${r}" "1" audio frame_count)
  expect_code(bad-audio "${r}")

  result_copy(r audio_bad_digest)
  result_add_audio("${r}")
  gbl_json_mutate_set("${r}" "\"not-a-sha256\"" audio artifact sha256)
  expect_code(bad-audio "${r}")

  result_copy(r audio_wrong_artifact_state)
  result_add_audio("${r}")
  gbl_json_mutate_set("${r}" "\"not-written\"" audio artifact state)
  expect_code(bad-audio "${r}")

  result_copy(r audio_wrong_empty_digest)
  result_add_audio("${r}")
  gbl_json_mutate_set("${r}" "\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\"" audio artifact sha256)
  expect_detail(bad-audio "empty artifact" "${r}")

elseif(CASE STREQUAL "phase10")
  phase10_expect_ok("${dir}/result.json")

  result_copy(r historical_without_phase10)
  phase10_expect_ok("${r}")

  result_copy_phase10(r historical_model1_with_complete_capabilities)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"gamebloke-ppu-observation-1\""
    phase10 model)
  phase10_expect_ok("${r}")

  result_copy_phase10(r current_profile_model2)
  result_add_phase10_capabilities("${r}")
  phase10_expect_ok("${r}")

  result_copy_phase10(r current_production_observation)
  result_copy_production_phase10_observation("${r}")
  phase10_expect_ok("${r}")

  result_copy_phase10(r partial_without_raw_video)
  gbl_json_mutate_remove("${r}" raw_video)
  phase10_expect_code(partial-phase10-extension "${r}")

  result_copy_phase10(r partial_without_ppu)
  gbl_json_mutate_remove("${r}" phase10)
  phase10_expect_code(partial-phase10-extension "${r}")

  result_copy_phase10(r complete_video_output)
  result_add_phase10_capabilities("${r}")
  result_add_phase10_valid_video("${r}")
  phase10_expect_ok("${r}")

  result_copy_phase10(r classified_output_failure)
  result_add_phase10_capabilities("${r}")
  result_add_phase10_output_failure("${r}")
  phase10_expect_ok("${r}" HARNESS_FAIL)

  result_copy_phase10(r disputed_hblank_dependency)
  result_add_phase10_capabilities("${r}")
  result_add_phase10_hblank_dependency("${r}")
  phase10_expect_ok("${r}")

  phase10_report_success()

elseif(CASE STREQUAL "phase10_negative")
  file(COPY_FILE "${dir}/result.json" "${dir}/result_actual_producer.json")
  result_copy_production_phase10_observation(
    "${dir}/result_actual_producer.json")
  phase10_expect_ok("${dir}/result_actual_producer.json")

  result_copy_phase10(r missing_current_display_capabilities)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_remove("${r}" phase10 display_capabilities)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r missing_current_capability_member)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_remove("${r}" phase10 display_capabilities modes 0 implementation_status)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_current_producer_phase10(r missing_current_composition_features)
  gbl_json_mutate_remove("${r}" phase10 display_capabilities composition_features)
  phase10_expect_code(bad-phase10-capability "${r}")

  foreach(feature IN ITEMS rectangular_windows object_windows alpha_blending
      brightness bg_mosaic obj_mosaic forced_blank)
    result_copy_current_producer_phase10(r false_current_feature_${feature})
    gbl_json_mutate_set("${r}" "false"
      phase10 display_capabilities composition_features ${feature})
    phase10_expect_code(bad-phase10-capability "${r}")
  endforeach()

  result_copy_current_producer_phase10(r wrong_current_feature_classification)
  gbl_json_mutate_set("${r}" "\"hardware-qualified\""
    phase10 display_capabilities composition_features classification)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_current_producer_phase10(r overstated_current_feature_qualification)
  gbl_json_mutate_set("${r}" "\"QUALIFIED\""
    phase10 display_capabilities composition_features hardware_qualification)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r missing_current_profile_version)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_remove("${r}" phase10 profile_version)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_profile_version_type)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"10\"" phase10 profile_version)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r stale_current_profile_version)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "12" phase10 profile_version)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_observation_model_type)
  gbl_json_mutate_set("${r}" "2" phase10 model)
  phase10_expect_code(bad-phase10-type "${r}")

  result_copy_phase10(r wrong_current_capability_object_type)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "[]" phase10 video_memory_access)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_mode_id)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "2" phase10 display_capabilities modes 3 mode)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_source)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"affine\""
    phase10 display_capabilities modes 3 backgrounds 0 source)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_text_source)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"affine\""
    phase10 display_capabilities modes 0 backgrounds 0 source)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_mode1_affine_source)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"text\""
    phase10 display_capabilities modes 1 backgrounds 2 source)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_mode2_affine_source)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"text\""
    phase10 display_capabilities modes 2 backgrounds 1 source)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_dimension)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "241"
    phase10 display_capabilities modes 3 backgrounds 0 dimensions 0 width)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_mode4_source)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"direct-color-bitmap\""
    phase10 display_capabilities modes 4 backgrounds 0 source)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_mode5_source)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"indexed-bitmap\""
    phase10 display_capabilities modes 5 backgrounds 0 source)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_mode4_dimension)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "239"
    phase10 display_capabilities modes 4 backgrounds 0 dimensions 0 width)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_mode5_dimension)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "159"
    phase10 display_capabilities modes 5 backgrounds 0 dimensions 0 width)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_text_dimension)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "255"
    phase10 display_capabilities modes 1 backgrounds 0 dimensions 0 width)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_mode1_affine_dimension)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "255"
    phase10 display_capabilities modes 1 backgrounds 2 dimensions 0 width)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_mode2_affine_dimension)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "255"
    phase10 display_capabilities modes 2 backgrounds 1 dimensions 0 width)
  phase10_expect_code(bad-phase10-capability "${r}")

  foreach(mode IN ITEMS 0 1 2 3 4 5)
    result_copy_phase10(r stale_supported_mode_${mode}_status)
    result_add_phase10_capabilities("${r}")
    gbl_json_mutate_set("${r}" "\"not-implemented\""
      phase10 display_capabilities modes ${mode} implementation_status)
    phase10_expect_code(bad-phase10-capability "${r}")
  endforeach()

  result_copy_current_producer_phase10(r complete_current_mode_overclaim)
  gbl_json_mutate_set("${r}" "\"complete\""
    phase10 display_capabilities modes 3 implementation_status)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_implementation_status)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"not-implemented\""
    phase10 display_capabilities modes 3 implementation_status)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_raw_format)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"RGB555\""
    phase10 display_capabilities raw_output format)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r wrong_current_artifact_format)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"RGB555\"" raw_video format)
  phase10_expect_code(bad-phase10-video-output "${r}")

  result_copy_phase10(r wrong_current_artifact_format_type)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "555" raw_video format)
  phase10_expect_code(bad-phase10-video-output "${r}")

  result_copy_phase10(r wrong_current_raw_byte_length)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "76799"
    phase10 display_capabilities raw_output byte_length)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r escalated_current_qualification)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"QUALIFIED\""
    phase10 video_memory_access hardware_qualification)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r added_current_ppu_cycles)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "1"
    phase10 video_memory_access ppu_extra_cycles)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r current_model_partial_capabilities)
  gbl_json_mutate_set("${r}" "\"gamebloke-ppu-observation-2\""
    phase10 model)
  gbl_json_mutate_set("${r}" "14" phase10 profile_version)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_current_producer_phase10(r stale_current_producer_profile_14)
  gbl_json_mutate_set("${r}" "14" phase10 profile_version)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r historical_model1_partial_capabilities)
  gbl_json_mutate_set("${r}" "10" phase10 profile_version)
  phase10_expect_code(bad-phase10-capability "${r}")

  result_copy_phase10(r unknown_observation_model)
  gbl_json_mutate_set("${r}" "\"gamebloke-ppu-observation-99\""
    phase10 model)
  phase10_expect_code(bad-phase10-type "${r}")

  result_copy_phase10(r wrong_width)
  gbl_json_mutate_set("${r}" "239" phase10 frame width)
  phase10_expect_code(bad-phase10-frame "${r}")
  phase10_expect_ok("${dir}/result.json")

  result_copy_phase10(r wrong_observed_type)
  gbl_json_mutate_set("${r}" "\"true\"" phase10 observed)
  phase10_expect_code(bad-phase10-type "${r}")
  phase10_expect_ok("${dir}/result.json")

  result_copy_phase10(r wrong_stride)
  gbl_json_mutate_set("${r}" "481" phase10 frame stride_bytes)
  phase10_expect_code(bad-phase10-frame "${r}")
  phase10_expect_ok("${dir}/result.json")

  result_copy_phase10(r wrong_byte_count)
  gbl_json_mutate_set("${r}" "76799" phase10 frame byte_length)
  phase10_expect_code(bad-phase10-frame "${r}")
  phase10_expect_ok("${dir}/result.json")

  result_copy_phase10(r raster_time_mismatch)
  gbl_json_mutate_set("${r}" "\"23\"" phase10 raster machine_cycle)
  phase10_expect_code(bad-phase10-machine-time "${r}")
  phase10_expect_ok("${dir}/result.json")

  result_copy_phase10(r valid_frame_missing_completion)
  result_add_phase10_capabilities("${r}")
  result_add_phase10_valid_video("${r}")
  gbl_json_mutate_set("${r}" "null" phase10 frame completion_cycle)
  phase10_expect_code(bad-phase10-frame-validity "${r}")
  result_copy_phase10(r valid_frame_clean_twin)
  result_add_phase10_capabilities("${r}")
  result_add_phase10_valid_video("${r}")
  phase10_expect_ok("${r}")

  result_copy_phase10(r emitted_from_invalid_frame)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "true" raw_video requested)
  gbl_json_mutate_set("${r}" "\"emitted\"" raw_video status)
  gbl_json_mutate_set("${r}" "true" raw_video emitted)
  gbl_json_mutate_set("${r}" "76800" raw_video byte_length)
  gbl_json_mutate_set("${r}" "\"abababababababababababababababababababababababababababababababab\"" raw_video sha256)
  phase10_expect_code(bad-phase10-video-output "${r}")
  phase10_expect_ok("${dir}/result.json")

  result_copy_phase10(r malformed_video_digest)
  result_add_phase10_capabilities("${r}")
  result_add_phase10_valid_video("${r}")
  gbl_json_mutate_set("${r}" "\"ABAB\"" raw_video sha256)
  phase10_expect_code(bad-phase10-video-digest "${r}")
  result_copy_phase10(r valid_digest_clean_twin)
  result_add_phase10_capabilities("${r}")
  result_add_phase10_valid_video("${r}")
  phase10_expect_ok("${r}")

  result_copy_phase10(r wrong_video_length)
  result_add_phase10_capabilities("${r}")
  result_add_phase10_valid_video("${r}")
  gbl_json_mutate_set("${r}" "76799" raw_video byte_length)
  phase10_expect_code(bad-phase10-video-output "${r}")
  result_copy_phase10(r valid_length_clean_twin)
  result_add_phase10_capabilities("${r}")
  result_add_phase10_valid_video("${r}")
  phase10_expect_ok("${r}")

  result_copy_phase10(r false_hardware_qualification)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"QUALIFIED\"" phase10 timing hardware_qualification)
  phase10_expect_code(bad-phase10-policy "${r}")
  phase10_expect_ok("${dir}/result.json")

  result_copy_phase10(r wrong_hblank_offset)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "\"1008\"" phase10 timing hblank_offset_cycles)
  phase10_expect_code(bad-phase10-policy "${r}")
  phase10_expect_ok("${dir}/result.json")

  result_copy_phase10(r undisclosed_hblank_dependency)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "true" phase10 hblank_trigger_dependency observed)
  gbl_json_mutate_set("${r}" "\"disputed\"" phase10 hblank_trigger_dependency classification)
  gbl_json_mutate_set("${r}" "\"disp-hblank-flag-edge\"" phase10 hblank_trigger_dependency dispute_id)
  phase10_expect_code(bad-phase10-dependency "${r}")
  phase10_expect_ok("${dir}/result.json")

  result_copy_phase10(r unobserved_with_raster)
  result_add_phase10_capabilities("${r}")
  gbl_json_mutate_set("${r}" "false" phase10 observed)
  phase10_expect_code(bad-phase10-not-observed "${r}")
  phase10_expect_ok("${dir}/result.json")

  phase10_report_success()

elseif(CASE STREQUAL "phase_positive")
  result_copy(r legacy_without_phase8)
  foreach(field IN ITEMS model qualification bus_cycles internal_cycles wait_cycles
      dma_startup_cycles dma_source_cycles dma_destination_cycles
      dma_completed_units dma_pause_cycles)
    gbl_json_mutate_remove("${r}" time ${field})
  endforeach()
  gbl_json_mutate_remove("${r}" timer0)
  gbl_json_mutate_remove("${r}" phase8)
  gbl_json_mutate_remove("${r}" phase9)
  expect_ok(PASS "${r}")

  result_copy(r complete_phase8_tuple)
  result_add_phase8_extension("${r}")
  expect_ok(PASS "${r}")

  result_copy(r phase8_wait_cycles_in_total)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "\"20\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "\"4\"" phase8 wait_cycles)
  expect_ok(PASS "${r}")

  hex(h64 "a" 64)
  result_copy(r phase8_with_script_identity)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "\"script\"" claim input_timeline kind)
  gbl_json_mutate_set("${r}" "\"${h64}\"" claim input_timeline sha256)
  expect_ok(PASS "${r}")

  result_copy(r phase8_zero_budget_nulls)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "\"TIMEOUT\"" status)
  gbl_json_mutate_set("${r}" "\"cycle-budget\"" reason)
  gbl_json_mutate_set("${r}" "\"timeout\"" termination)
  gbl_json_mutate_set("${r}" "\"absent\"" semantic state)
  gbl_json_mutate_set("${r}" "\"0\"" semantic case_id)
  gbl_json_mutate_set("${r}" "\"0\"" time cycles)
  gbl_json_mutate_set("${r}" "\"0\"" time instructions)
  gbl_json_mutate_set("${r}" "\"0\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"0\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "null" timer0 if_request)
  result_phase8_set_not_run("${r}")
  expect_ok(TIMEOUT "${r}")

  result_copy(r phase8_uint64_max_boundary)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551614\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "\"1\"" phase8 wait_cycles)
  expect_ok(PASS "${r}")

elseif(CASE STREQUAL "phase_negative")
  result_copy(r missing_phase8_field)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_remove("${r}" phase8 irq ime)
  expect_detail(missing-field "phase8.irq.ime" "${r}")

  result_copy(r phase8_wrong_type)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "0" phase8 keypad condition)
  expect_detail(bad-phase8-type "phase8.keypad.condition" "${r}")

  result_copy(r entered_without_acceptance)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "true" phase8 irq sample entered)
  expect_detail(bad-phase8-entry "entered requires accepted IRQ exception" "${r}")

  result_copy(r accepted_while_sample_masked)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "8" phase8 irq sample if)
  gbl_json_mutate_set("${r}" "8" phase8 irq sample eligible_sources)
  gbl_json_mutate_set("${r}" "true" phase8 irq sample eligible)
  gbl_json_mutate_set("${r}" "true" phase8 irq sample cpsr_i_masked)
  gbl_json_mutate_set("${r}" "true" phase8 irq sample accepted)
  expect_detail(bad-phase8-eligibility "accepted conflicts with sampled" "${r}")

  result_copy(r wake_without_source)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "null" phase8 power wake_source_mask)
  expect_detail(bad-phase8-wake "wake cause requires source and cycle" "${r}")

  result_copy(r not_run_observations)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "false" phase8 observed)
  expect_detail(bad-phase8-not-run "unobserved phase8 requires null" "${r}")

  hex(h10 "a" 10)
  result_copy(r phase8_bad_script_digest)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "\"script\"" claim input_timeline kind)
  gbl_json_mutate_set("${r}" "\"${h10}\"" claim input_timeline sha256)
  expect_detail(bad-input-timeline "claim.input_timeline.sha256" "${r}")

  result_copy(r lost_wait_cycles)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "\"20\"" time bus_cycles)
  gbl_json_mutate_set("${r}" "\"4\"" phase8 wait_cycles)
  expect_ok(PASS "${r}")
  gbl_json_mutate_set("${r}" "\"0\"" phase8 wait_cycles)
  expect_detail(bad-time-components
    "time.cycles does not equal time.bus_cycles \\+ time.internal_cycles \\+ phase8.wait_cycles" "${r}")

  result_copy(r phase8_wait_overflow)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "\"18446744073709551616\"" phase8 wait_cycles)
  expect_detail(time-overflow "phase8.wait_cycles" "${r}")

  result_copy(r phase8_sum_overflow)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time budget_cycles)
  gbl_json_mutate_set("${r}" "\"18446744073709551615\"" time bus_cycles)
  expect_ok(PASS "${r}")
  gbl_json_mutate_set("${r}" "\"1\"" phase8 wait_cycles)
  expect_detail(time-sum-overflow
    "time.bus_cycles \\+ time.internal_cycles \\+ phase8.wait_cycles exceeds UINT64_MAX" "${r}")

  result_copy(r inflated_phase8_qualification)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "\"hardware-qualified\"" phase8 qualification)
  expect_detail(bad-phase8-qualification "phase8.qualification" "${r}")

  result_copy(r handler_success_claim)
  result_add_phase8_extension("${r}")
  gbl_json_mutate_set("${r}" "8" phase8 irq sample if)
  gbl_json_mutate_set("${r}" "8" phase8 irq sample eligible_sources)
  gbl_json_mutate_set("${r}" "true" phase8 irq sample eligible)
  gbl_json_mutate_set("${r}" "true" phase8 irq sample accepted)
  gbl_json_mutate_set("${r}" "true" phase8 irq sample entered)
  gbl_json_mutate_set("${r}" "\"irq\"" phase8 last_exception)
  expect_detail(bad-phase8-handler-claim
    "entered IRQ must remain GUEST_STOP with no-bios-irq" "${r}")

elseif(CASE STREQUAL "usage")
  # No RESULT, and no MANIFEST.
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DMANIFEST=${dir}/manifest.json"
      "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "result: usage")
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DRESULT=${dir}/result.json"
      "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "result: usage")

elseif(CASE STREQUAL "bad_schema")
  result_copy(r schema)
  gbl_json_mutate_set("${r}" "\"gamebloke.result/2\"" schema)
  expect_code(bad-schema "${r}")

elseif(CASE STREQUAL "missing_field")
  # Every required top-level key, fixture.id, fixture.rom_sha256, and each of the six claim
  # fields removed; fixture.ran as a string instead of a boolean.
  foreach(path IN ITEMS
      schema fixture status claim disputes labels limits semantic time termination trace
      fixture/id fixture/rom_sha256 semantic/state semantic/case_id time/cycles
      time/instructions time/budget_cycles
      claim/machine claim/boot claim/bios_provider claim/cart claim/input_timeline
      claim/evidence)
    string(REPLACE "/" ";" keys "${path}")
    string(REPLACE "/" "_" tag "${path}")
    result_copy(r rm_${tag})
    gbl_json_mutate_remove("${r}" ${keys})
    expect_code(missing-field "${r}")
  endforeach()
  result_copy(r ran_string)
  gbl_json_mutate_set("${r}" "\"true\"" fixture ran)
  expect_code(missing-field "${r}")

elseif(CASE STREQUAL "unknown_status")
  set(i 0)
  foreach(v IN ITEMS "pass" "PASSED" "OK" "DISPUTED")
    result_copy(r status${i})
    gbl_json_mutate_set("${r}" "\"${v}\"" status)
    expect_code(unknown-status "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "pass_without_digest")
  # PASS with a null digest, and with a 63-hex digest (the claim cart changed to match, so
  # only the digest is wrong).
  hex(h63 "a" 63)
  result_copy(r null_digest)
  gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
  expect_code(pass-without-digest "${r}")
  result_copy(r short_digest)
  gbl_json_mutate_set("${r}" "\"${h63}\"" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "\"${h63}\"" claim cart rom_sha256)
  expect_code(pass-without-digest "${r}")

elseif(CASE STREQUAL "ran_state_mismatch")
  # A state whose row says the fixture did not run, claimed with ran true; a state whose
  # row says it ran, claimed with ran false.
  result_copy(r skipped_ran)
  gbl_json_mutate_set("${r}" "\"SKIPPED\"" status)
  gbl_json_mutate_set("${r}" "\"not built in this run\"" reason)
  expect_code(ran-state-mismatch "${r}")
  result_copy(r fail_not_ran)
  gbl_json_mutate_set("${r}" "\"GUEST_FAIL\"" status)
  gbl_json_mutate_set("${r}" "false" fixture ran)
  expect_code(ran-state-mismatch "${r}")

elseif(CASE STREQUAL "missing_digest")
  # States that ran must carry a digest.
  foreach(state IN ITEMS GUEST_FAIL GUEST_STOP TIMEOUT)
    result_copy(r digest_${state})
    gbl_json_mutate_set("${r}" "\"${state}\"" status)
    gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
    gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
    expect_detail(missing-digest "${state} needs fixture.rom_sha256" "${r}")
  endforeach()
  # A state whose digest is optional still rejects a malformed digest string (10 hex).
  result_copy(r malformed)
  gbl_json_mutate_set("${r}" "\"HARNESS_FAIL\"" status)
  gbl_json_mutate_set("${r}" "\"0123456789\"" fixture rom_sha256)
  expect_detail(missing-digest "fixture.rom_sha256 is malformed" "${r}")

elseif(CASE STREQUAL "missing_reason")
  # SKIPPED with no reason; MISSING with an empty reason.
  result_copy(r skipped)
  gbl_json_mutate_set("${r}" "\"SKIPPED\"" status)
  gbl_json_mutate_set("${r}" "false" fixture ran)
  gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
  expect_code(missing-reason "${r}")
  result_copy(r missing)
  gbl_json_mutate_set("${r}" "\"MISSING\"" status)
  gbl_json_mutate_set("${r}" "false" fixture ran)
  gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
  gbl_json_mutate_set("${r}" "\"\"" reason)
  expect_code(missing-reason "${r}")

elseif(CASE STREQUAL "missing_refusal")
  # REFUSED with no refusal object, with no code, and with no postcondition.
  set(i 0)
  foreach(v IN ITEMS "" "{\"postcondition\": \"no state was changed\"}"
      "{\"code\": \"declared-extent\"}")
    result_copy(r refused${i})
    gbl_json_mutate_set("${r}" "\"REFUSED\"" status)
    gbl_json_mutate_set("${r}" "false" fixture ran)
    gbl_json_mutate_set("${r}" "null" fixture rom_sha256)
    gbl_json_mutate_set("${r}" "null" claim cart rom_sha256)
    if(NOT "${v}" STREQUAL "")
      gbl_json_mutate_set("${r}" "${v}" refusal)
    endif()
    expect_code(missing-refusal "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_machine")
  set(i 0)
  foreach(v IN ITEMS "agb-micro" "AGB")
    result_copy(r machine${i})
    gbl_json_mutate_set("${r}" "\"${v}\"" claim machine)
    expect_code(bad-machine "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_boot")
  result_copy(r boot)
  gbl_json_mutate_set("${r}" "\"bios\"" claim boot)
  expect_code(bad-boot "${r}")

elseif(CASE STREQUAL "bad_bios_provider")
  # The retired milestone label, an empty hle list, a branch-named replacement, a short
  # user-official digest, and a trailing space.
  set(i 0)
  foreach(v IN ITEMS "M1" "hle:" "replacement:x@main" "user-official@abc" "none ")
    result_copy(r bios${i})
    gbl_json_mutate_set("${r}" "\"${v}\"" claim bios_provider)
    expect_code(bad-bios-provider "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_cart")
  result_copy(r kind)
  gbl_json_mutate_set("${r}" "\"flash\"" claim cart kind)
  expect_code(bad-cart "${r}")
  result_copy(r digest)
  gbl_json_mutate_set("${r}" "\"xyz\"" claim cart rom_sha256)
  expect_code(bad-cart "${r}")

elseif(CASE STREQUAL "bad_input_timeline")
  hex(h10 "a" 10)
  result_copy(r kind)
  gbl_json_mutate_set("${r}" "\"movie\"" claim input_timeline kind)
  expect_code(bad-input-timeline "${r}")
  result_copy(r script_no_digest)
  gbl_json_mutate_set("${r}" "\"script\"" claim input_timeline kind)
  expect_code(bad-input-timeline "${r}")
  result_copy(r script_short_digest)
  gbl_json_mutate_set("${r}" "\"script\"" claim input_timeline kind)
  gbl_json_mutate_set("${r}" "\"${h10}\"" claim input_timeline sha256)
  expect_code(bad-input-timeline "${r}")

elseif(CASE STREQUAL "bad_evidence")
  # Empty list; an item without a class; a class outside the vocabulary; a class that
  # differs from the manifest row (the example row is suite-self-checking).
  result_copy(r empty)
  gbl_json_mutate_set("${r}" "[]" claim evidence)
  expect_code(bad-evidence "${r}")
  result_copy(r no_class)
  gbl_json_mutate_remove("${r}" claim evidence 0 provenance_class)
  expect_code(bad-evidence "${r}")
  result_copy(r foreign_class)
  gbl_json_mutate_set("${r}" "\"emulator-agreement\"" claim evidence 0 provenance_class)
  expect_code(bad-evidence "${r}")
  result_copy(r row_class)
  gbl_json_mutate_set("${r}" "\"project-authored\"" claim evidence 0 provenance_class)
  expect_code(bad-evidence "${r}")
  # An item that does not name its fixture (WR-08).
  result_copy(r no_fixture)
  gbl_json_mutate_remove("${r}" claim evidence 0 fixture)
  expect_detail(bad-evidence "claim.evidence\\[0\\]: fixture" "${r}")

elseif(CASE STREQUAL "claim_mismatch")
  # The claim names a different ROM than the fixture block.
  hex(other "cd" 32)
  result_copy(r digest)
  gbl_json_mutate_set("${r}" "\"${other}\"" claim cart rom_sha256)
  expect_detail(claim-mismatch "claim.cart.rom_sha256 differs" "${r}")

  # The evidence list names a fixture, but not the one this result is for. A second
  # manifest row exists so the item is a known fixture (no unknown-fixture finding).
  manifest_copy(m two_rows)
  file(READ "${m}" mdoc)
  string(JSON row GET "${mdoc}" fixtures 0)
  gbl_json_mutate_set("${m}" "${row}" fixtures 1)
  gbl_json_mutate_set("${m}" "\"other-row\"" fixtures 1 id)
  result_copy(r evidence_other)
  gbl_json_mutate_set("${r}" "\"other-row\"" claim evidence 0 fixture)
  expect_detail(claim-mismatch "no evidence item names example-row" "${r}" "${m}")

elseif(CASE STREQUAL "manifest_digest")
  # WR-04: the result digest is tied to the manifest row's pinned digest. The row becomes
  # a url row; its sha256 differs from the result's 0123... digest (negative), then
  # equals it (the same document is accepted, so the tie is the only difference).
  hex(row_other "ab" 32)
  manifest_copy(m differs)
  gbl_json_mutate_set("${m}"
    "{\"kind\": \"url\", \"url\": \"https://example.invalid/a.dat\", \"sha256\": \"${row_other}\"}"
    fixtures 0 source)
  expect_detail(claim-mismatch
    "fixture.rom_sha256 differs from the manifest digest for example-row" "${dir}/result.json" "${m}")
  file(READ "${dir}/result.json" rdoc)
  string(JSON same GET "${rdoc}" fixture rom_sha256)
  manifest_copy(m same)
  gbl_json_mutate_set("${m}"
    "{\"kind\": \"url\", \"url\": \"https://example.invalid/a.dat\", \"sha256\": \"${same}\"}"
    fixtures 0 source)
  expect_ok(PASS "${dir}/result.json" "${m}")
  # An in-tree row ties the same way.
  manifest_copy(m tree)
  gbl_json_mutate_set("${m}"
    "{\"kind\": \"in-tree\", \"path\": \"tests/fixtures/x.dat\", \"sha256\": \"${row_other}\"}"
    fixtures 0 source)
  expect_detail(claim-mismatch "fixture.rom_sha256 differs from the manifest digest"
    "${dir}/result.json" "${m}")

elseif(CASE STREQUAL "artifact_digest")
  # The git source commit and toolchain stay provenance fields; the optional
  # artifact SHA is the single runtime ROM identity projected into result/1.
  hex(artifact_digest "ab" 32)
  hex(changed_digest "cd" 32)
  manifest_copy(m artifact)
  string(JSON artifact SET "{}" path "\"diagnostics/original.gba\"")
  string(JSON artifact SET "${artifact}" sha256 "\"${artifact_digest}\"")
  gbl_json_mutate_set("${m}" "${artifact}" fixtures 0 build artifact)

  result_copy(r artifact_match)
  gbl_json_mutate_set("${r}" "\"${artifact_digest}\"" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "\"${artifact_digest}\"" claim cart rom_sha256)
  expect_ok(PASS "${r}" "${m}")

  result_copy(r stale_artifact_result)
  gbl_json_mutate_set("${r}" "\"${changed_digest}\"" fixture rom_sha256)
  gbl_json_mutate_set("${r}" "\"${changed_digest}\"" claim cart rom_sha256)
  expect_detail(claim-mismatch
    "fixture.rom_sha256 differs from the manifest digest for example-row"
    "${r}" "${m}")

  # Legacy git metadata still has no output digest tie in result/1.
  result_copy(r legacy_git)
  expect_ok(PASS "${r}")

elseif(CASE STREQUAL "empty_limits")
  set(i 0)
  foreach(v IN ITEMS "[]" "[\"\"]" "\"not a list\"")
    result_copy(r limits${i})
    gbl_json_mutate_set("${r}" "${v}" limits)
    expect_code(empty-limits "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bios_provider_forms")
  # All five R1 forms are accepted; the example already holds the first.
  hex(h40 "ab" 20)
  hex(h64 "cd" 32)
  result_copy(r none)
  expect_ok(PASS "${r}")
  set(i 0)
  foreach(v IN ITEMS "project-stub" "hle:CpuSet,Div" "replacement:open-bios@${h40}"
      "user-official@${h64}")
    result_copy(r bios${i})
    gbl_json_mutate_set("${r}" "\"${v}\"" claim bios_provider)
    expect_ok(PASS "${r}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "unknown_fixture")
  # The result names a fixture the manifest does not hold; so does an evidence item.
  result_copy(r id)
  gbl_json_mutate_set("${r}" "\"nope-row\"" fixture id)
  expect_code(unknown-fixture "${r}")
  result_copy(r evidence)
  gbl_json_mutate_set("${r}" "\"nope-row\"" claim evidence 0 fixture)
  expect_code(unknown-fixture "${r}")

elseif(CASE STREQUAL "unknown_dispute")
  result_copy(r nope)
  gbl_json_mutate_set("${r}" "[\"disp-nope\"]" disputes)
  gbl_json_mutate_set("${r}" "[\"disputed\"]" labels)
  expect_code(unknown-dispute "${r}")

elseif(CASE STREQUAL "undisclosed_dispute")
  # The manifest row points at a registry entry (class disputed) and the evidence item
  # carries that class, but the result lists no dispute and no label; then it lists the
  # dispute and still omits the label.
  manifest_copy(m disputed)
  gbl_json_mutate_set("${m}" "\"disputed\"" fixtures 0 expected provenance_class)
  gbl_json_mutate_set("${m}" "\"disp-mgba-suite-27\"" fixtures 0 expected dispute)
  result_copy(r silent)
  gbl_json_mutate_set("${r}" "\"disputed\"" claim evidence 0 provenance_class)
  expect_code(undisclosed-dispute "${r}" "${m}")
  result_copy(r no_label)
  gbl_json_mutate_set("${r}" "\"disputed\"" claim evidence 0 provenance_class)
  gbl_json_mutate_set("${r}" "[\"disp-mgba-suite-27\"]" disputes)
  expect_code(undisclosed-dispute "${r}" "${m}")
  # A listed dispute with no label is undisclosed even when the row is not disputed.
  result_copy(r listed_only)
  gbl_json_mutate_set("${r}" "[\"disp-dma-latch\"]" disputes)
  expect_code(undisclosed-dispute "${r}")

elseif(CASE STREQUAL "label_without_dispute")
  result_copy(r label)
  gbl_json_mutate_set("${r}" "[\"disputed\"]" labels)
  expect_code(label-without-dispute "${r}")

elseif(CASE STREQUAL "bad_label")
  result_copy(r verified)
  gbl_json_mutate_set("${r}" "[\"verified\"]" labels)
  expect_code(bad-label "${r}")
  result_copy(r duplicate)
  gbl_json_mutate_set("${r}" "[\"disp-dma-latch\"]" disputes)
  gbl_json_mutate_set("${r}" "[\"disputed\", \"disputed\"]" labels)
  expect_code(bad-label "${r}")

elseif(CASE STREQUAL "disputed")
  # The honest form: the row is disputed, the evidence says so, and the result lists the
  # registry id and carries the label (ROADMAP SC5).
  manifest_copy(m disputed)
  gbl_json_mutate_set("${m}" "\"disputed\"" fixtures 0 expected provenance_class)
  gbl_json_mutate_set("${m}" "\"disp-mgba-suite-27\"" fixtures 0 expected dispute)
  result_copy(r disputed)
  gbl_json_mutate_set("${r}" "\"disputed\"" claim evidence 0 provenance_class)
  gbl_json_mutate_set("${r}" "[\"disp-mgba-suite-27\"]" disputes)
  gbl_json_mutate_set("${r}" "[\"disputed\"]" labels)
  expect_ok(PASS "${r}" "${m}")

else()
  message(FATAL_ERROR "result-case: unknown case: ${CASE}")
endif()
