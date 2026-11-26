cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "timer0-guest: missing required argument ${required}")
  endif()
endforeach()
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)

set(control "${BIN}/gamebloke-run-control")
set(production "${BIN}/gamebloke-run")
set(fixtures "${BIN}/generated/fixtures")
set(control_manifest "${BIN}/generated/control/manifest.json")
set(control_index "${BIN}/generated/control/fixture_index.h")
set(production_index "${BIN}/generated/production/fixture_index.h")
set(scratch "${BIN}/nc/timer0_guest")
set(timer0_pass "${fixtures}/owned-timer0-pass.gba")
set(timer0_disabled "${fixtures}/owned-timer0-disabled.gba")
file(MAKE_DIRECTORY "${scratch}")

foreach(path IN ITEMS "${control}" "${production}" "${timer0_pass}"
    "${timer0_disabled}" "${control_manifest}" "${control_index}"
    "${production_index}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "timer0-guest: required generated input is missing")
  endif()
endforeach()

function(expect_json_type json expected)
  string(JSON actual ERROR_VARIABLE json_error TYPE "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "timer0-guest: expected JSON type ${ARGN}=${expected}; got ${actual}; ${json_error}")
  endif()
endfunction()

function(expect_json_field json expected)
  string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "timer0-guest: expected ${ARGN}=${expected}; got ${actual}; ${json_error}")
  endif()
endfunction()

function(expect_json_boolean json expected)
  string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR
      "timer0-guest: could not read boolean ${ARGN}; ${json_error}")
  endif()
  if("${expected}" STREQUAL "true")
    if(NOT actual)
      message(FATAL_ERROR "timer0-guest: expected ${ARGN}=true; got false")
    endif()
  elseif("${expected}" STREQUAL "false")
    if(actual)
      message(FATAL_ERROR "timer0-guest: expected ${ARGN}=false; got true")
    endif()
  else()
    message(FATAL_ERROR "timer0-guest: invalid boolean expectation")
  endif()
endfunction()

function(run_json output_variable name cli expected_exit expected_status
    expected_reason)
  execute_process(
    COMMAND "${cli}" ${ARGN}
    RESULT_VARIABLE run_result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT "${run_result}" STREQUAL "${expected_exit}")
    message(FATAL_ERROR
      "timer0-guest: ${name} exit ${run_result}, expected ${expected_exit}; stderr=${error}")
  endif()
  string(JSON actual_status ERROR_VARIABLE status_error GET "${output}" status)
  if(NOT "${status_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual_status}" STREQUAL "${expected_status}")
    message(FATAL_ERROR
      "timer0-guest: ${name} status ${actual_status}, expected ${expected_status}; ${status_error}; stdout=${output}")
  endif()
  string(JSON actual_reason ERROR_VARIABLE reason_error GET "${output}" reason)
  if(NOT "${reason_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual_reason}" STREQUAL "${expected_reason}")
    message(FATAL_ERROR
      "timer0-guest: ${name} reason ${actual_reason}, expected ${expected_reason}; ${reason_error}")
  endif()
  string(REGEX MATCH "^[{][^\n]*[}]\n$" one_json_line "${output}")
  if(NOT "${one_json_line}" STREQUAL "${output}")
    message(FATAL_ERROR "timer0-guest: ${name} did not write exactly one JSON line")
  endif()
  string(FIND "${output}" "${SRC}" source_path_offset)
  string(FIND "${output}" "${BIN}" build_path_offset)
  if(NOT source_path_offset EQUAL -1 OR NOT build_path_offset EQUAL -1)
    message(FATAL_ERROR "timer0-guest: ${name} exposed a host path")
  endif()
  set(${output_variable} "${output}" PARENT_SCOPE)
endfunction()

function(expect_time_extension json)
  expect_json_type("${json}" OBJECT time)
  expect_json_field("${json}" gamebloke-timer-model-1 time model)
  expect_json_field("${json}" project-model-only time qualification)
  foreach(field IN ITEMS cycles instructions budget_cycles bus_cycles internal_cycles)
    expect_json_type("${json}" STRING time ${field})
  endforeach()
  expect_json_type("${json}" BOOLEAN timer0 if_request)
endfunction()

function(find_control_row output_variable manifest id)
  string(JSON row_count LENGTH "${manifest}" fixtures)
  set(found_index -1)
  if(row_count GREATER 0)
    math(EXPR row_last "${row_count}-1")
    foreach(index RANGE 0 ${row_last})
      string(JSON candidate GET "${manifest}" fixtures ${index} id)
      if("${candidate}" STREQUAL "${id}")
        if(NOT found_index EQUAL -1)
          message(FATAL_ERROR "timer0-guest: duplicate control manifest row ${id}")
        endif()
        set(found_index "${index}")
      endif()
    endforeach()
  endif()
  if(found_index EQUAL -1)
    message(FATAL_ERROR "timer0-guest: control manifest row missing: ${id}")
  endif()
  set(${output_variable} "${found_index}" PARENT_SCOPE)
endfunction()

function(expect_control_row manifest id image relative_path generator_sha)
  find_control_row(row_index "${manifest}" "${id}")
  file(SHA256 "${image}" image_sha)
  string(JSON row_source_kind GET "${manifest}" fixtures ${row_index} source kind)
  string(JSON row_source_path GET "${manifest}" fixtures ${row_index} source path)
  string(JSON row_source_sha GET "${manifest}" fixtures ${row_index} source sha256)
  if(NOT row_source_kind STREQUAL "in-tree" OR
      NOT row_source_path STREQUAL "${relative_path}" OR
      NOT row_source_sha STREQUAL "${image_sha}")
    message(FATAL_ERROR
      "timer0-guest: source identity not bound to generated bytes for ${id}")
  endif()

  string(JSON row_bios GET "${manifest}" fixtures ${row_index} bios_requirement)
  string(JSON row_provenance GET "${manifest}" fixtures ${row_index} expected provenance_class)
  string(JSON row_dispute_type ERROR_VARIABLE dispute_error TYPE
    "${manifest}" fixtures ${row_index} expected dispute)
  if(NOT row_bios STREQUAL "none" OR
      NOT row_provenance STREQUAL "project-authored" OR
      NOT dispute_error STREQUAL "NOTFOUND" OR
      NOT row_dispute_type STREQUAL "NULL")
    message(FATAL_ERROR "timer0-guest: wrong provenance or BIOS metadata for ${id}")
  endif()

  string(JSON row_license GET "${manifest}" fixtures ${row_index} license spdx)
  string(JSON row_redistributable GET "${manifest}" fixtures ${row_index} license redistributable)
  string(JSON notice_count LENGTH "${manifest}" fixtures ${row_index} license notices)
  if(NOT row_license STREQUAL "LicenseRef-Gamebloke-Synthetic-Control" OR
      NOT row_redistributable STREQUAL "unknown" OR
      NOT notice_count EQUAL 1)
    message(FATAL_ERROR "timer0-guest: license scope changed for ${id}")
  endif()
  string(JSON row_notice GET "${manifest}" fixtures ${row_index} license notices 0)
  if(NOT row_notice STREQUAL "tests/fixtures/timer0-NOTICE.md")
    message(FATAL_ERROR "timer0-guest: notice binding is missing for ${id}")
  endif()
  string(JSON permitted_count LENGTH "${manifest}" fixtures ${row_index} permitted_use)
  if(NOT permitted_count EQUAL 1)
    message(FATAL_ERROR "timer0-guest: permitted-use scope changed for ${id}")
  endif()
  string(JSON permitted_use GET "${manifest}" fixtures ${row_index} permitted_use 0)
  if(NOT permitted_use STREQUAL "local-only")
    message(FATAL_ERROR "timer0-guest: non-local use was granted for ${id}")
  endif()

  string(JSON recipe GET "${manifest}" fixtures ${row_index} build recipe)
  string(JSON toolchain GET "${manifest}" fixtures ${row_index} build toolchain identity)
  if(NOT recipe MATCHES "tests/fixtures/instruction_image[.]c SHA-256=${generator_sha}" OR
      NOT toolchain MATCHES "^CMake-configured .+ C17 generator$")
    message(FATAL_ERROR "timer0-guest: generator/compiler provenance missing for ${id}")
  endif()
endfunction()

function(validate_result name json)
  set(result_file "${scratch}/${name}.json")
  file(WRITE "${result_file}" "${json}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DRESULT=${result_file}"
      "-DMANIFEST=${control_manifest}"
      "-DREGISTRY=${SRC}/registry/disputes.json"
      "-DVOCAB=${SRC}/contracts/evidence.json"
      -P "${SRC}/cmake/CheckResult.cmake"
    RESULT_VARIABLE check_result OUTPUT_VARIABLE check_out ERROR_VARIABLE check_err)
  string(FIND "${check_out}${check_err}" "result: ok (" accepted_offset)
  if(NOT "${check_result}" STREQUAL "0" OR accepted_offset EQUAL -1)
    message(FATAL_ERROR
      "timer0-guest: CheckResult rejected ${name}; ${check_out}${check_err}")
  endif()
endfunction()

function(run_fixture_index root output_result output_stdout output_stderr pair_mode)
  set(command_arguments
    "-DGBL_SOURCE_DIR=${SRC}"
    "-DGBL_OUTPUT_ROOT=${root}"
    "-DGBL_ARTIFACT_ROOT=${BIN}"
    "-DGBL_CONTROL_IMAGE=${fixtures}/owned-arm-pass.gba"
    "-DGBL_CONTROL_FAIL_IMAGE=${fixtures}/owned-arm-fail.gba"
    "-DGBL_CONTROL_SKIP_IMAGE=${fixtures}/owned-arm-pass.gba"
    "-DGBL_CONTROL_ZERO_CASE_IMAGE=${fixtures}/owned-arm-pass-zero-case.gba"
    "-DGBL_CONTROL_REGISTER_ARM_ZERO_IMAGE=${fixtures}/owned-register-arm-zero.gba"
    "-DGBL_CONTROL_REGISTER_ARM_FAIL_IMAGE=${fixtures}/owned-register-arm-fail.gba"
    "-DGBL_CONTROL_REGISTER_ARM_STOP_IMAGE=${fixtures}/owned-register-arm-stop.gba"
    "-DGBL_CONTROL_REGISTER_THUMB_ZERO_IMAGE=${fixtures}/owned-register-thumb-zero.gba"
    "-DGBL_CONTROL_REGISTER_THUMB_FAIL_IMAGE=${fixtures}/owned-register-thumb-fail.gba"
    "-DGBL_CONTROL_MANIFEST_OUTPUT=${root}/generated/control/manifest.json"
    "-DGBL_MANIFEST=${SRC}/tests/fixtures/manifest.json"
    "-DGBL_REGISTRY=${SRC}/registry/disputes.json"
    "-DGBL_VOCAB=${SRC}/contracts/evidence.json"
    "-DGBL_OUT_PRODUCTION=${root}/generated/production/fixture_index.h"
    "-DGBL_OUT_CONTROL=${root}/generated/control/fixture_index.h")
  if(pair_mode STREQUAL "pass-only")
    list(APPEND command_arguments
      "-DGBL_CONTROL_TIMER0_IMAGE=${timer0_pass}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" ${command_arguments}
      -P "${SRC}/cmake/GenerateFixtureIndex.cmake"
    RESULT_VARIABLE run_result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  set(${output_result} "${run_result}" PARENT_SCOPE)
  set(${output_stdout} "${output}" PARENT_SCOPE)
  set(${output_stderr} "${error}" PARENT_SCOPE)
endfunction()

function(classify_timer0_timeout json output_reason)
  string(JSON status GET "${json}" status)
  string(JSON result_reason GET "${json}" reason)
  string(JSON termination GET "${json}" termination)
  string(JSON semantic GET "${json}" semantic state)
  string(JSON case_id GET "${json}" semantic case_id)
  string(JSON cycles GET "${json}" time cycles)
  string(JSON budget GET "${json}" time budget_cycles)
  string(JSON request GET "${json}" timer0 if_request)
  set(classification "not-timer0-overflow-timeout")
  if(status STREQUAL "TIMEOUT" AND result_reason STREQUAL "cycle-budget" AND
      termination STREQUAL "timeout" AND semantic STREQUAL "absent" AND
      case_id STREQUAL "0" AND request STREQUAL "OFF" AND
      budget STREQUAL "10000" AND cycles GREATER 0 AND
      cycles LESS_EQUAL budget)
    set(classification "timer0-overflow-timeout")
  endif()
  set(${output_reason} "${classification}" PARENT_SCOPE)
endfunction()

file(SIZE "${timer0_pass}" pass_size)
file(SIZE "${timer0_disabled}" disabled_size)
if(NOT pass_size EQUAL 576 OR NOT disabled_size EQUAL pass_size)
  message(FATAL_ERROR "timer0-guest: generated image size is not 0x240 bytes")
endif()
file(READ "${timer0_pass}" pass_hex HEX)
file(READ "${timer0_disabled}" disabled_hex HEX)
string(TOLOWER "${pass_hex}" pass_hex)
string(TOLOWER "${disabled_hex}" disabled_hex)
string(SUBSTRING "${pass_hex}" 8 376 zero_header)
if(NOT zero_header MATCHES "^0+$")
  message(FATAL_ERROR "timer0-guest: generated image header/logo bytes are not zero")
endif()
math(EXPR timer_control_hex_offset "0x106 * 2")
math(EXPR timer_control_suffix_offset "${timer_control_hex_offset} + 2")
string(SUBSTRING "${pass_hex}" 0 ${timer_control_hex_offset} pass_prefix)
string(SUBSTRING "${pass_hex}" ${timer_control_suffix_offset} -1 pass_suffix)
string(SUBSTRING "${pass_hex}" ${timer_control_hex_offset} 2 pass_enable_byte)
string(SUBSTRING "${disabled_hex}" ${timer_control_hex_offset} 2 disabled_enable_byte)
if(NOT pass_enable_byte STREQUAL "c0" OR
    NOT disabled_enable_byte STREQUAL "40" OR
    NOT "${pass_prefix}40${pass_suffix}" STREQUAL "${disabled_hex}")
  message(FATAL_ERROR
    "timer0-guest: disabled fixture differs from clean image outside Timer 0 enable")
endif()

file(SHA256 "${SRC}/tests/fixtures/instruction_image.c" generator_source_sha)
file(READ "${control_manifest}" control_manifest_text)
expect_control_row("${control_manifest_text}"
  synthetic-control-timer0-pass "${timer0_pass}"
  generated/fixtures/owned-timer0-pass.gba "${generator_source_sha}")
expect_control_row("${control_manifest_text}"
  synthetic-control-timer0-disabled "${timer0_disabled}"
  generated/fixtures/owned-timer0-disabled.gba "${generator_source_sha}")

file(READ "${control_index}" control_index_text)
file(READ "${production_index}" production_index_text)
foreach(id IN ITEMS synthetic-control-timer0-pass synthetic-control-timer0-disabled)
  if(NOT "${control_index_text}" MATCHES "${id}" OR
      "${production_index_text}" MATCHES "${id}")
    message(FATAL_ERROR "timer0-guest: Timer 0 authority leaked or is missing: ${id}")
  endif()
endforeach()

# Callers that omit both optional Timer 0 controls preserve the prior control
# set. Supplying only one image is rejected before any index is written.
run_fixture_index("${scratch}/legacy-index" legacy_result legacy_out legacy_err none)
if(NOT "${legacy_result}" STREQUAL "0")
  message(FATAL_ERROR "timer0-guest: legacy optional-input call failed: ${legacy_out}${legacy_err}")
endif()
file(READ "${scratch}/legacy-index/generated/control/manifest.json" legacy_manifest)
string(JSON legacy_count LENGTH "${legacy_manifest}" fixtures)
string(JSON current_count LENGTH "${control_manifest_text}" fixtures)
set(legacy_ids "")
set(current_without_timer0_ids "")
if(legacy_count GREATER 0)
  math(EXPR legacy_last "${legacy_count}-1")
  foreach(index RANGE 0 ${legacy_last})
    string(JSON id GET "${legacy_manifest}" fixtures ${index} id)
    list(APPEND legacy_ids "${id}")
  endforeach()
endif()
if(current_count GREATER 0)
  math(EXPR current_last "${current_count}-1")
  foreach(index RANGE 0 ${current_last})
    string(JSON id GET "${control_manifest_text}" fixtures ${index} id)
    if(NOT id STREQUAL "synthetic-control-timer0-pass" AND
        NOT id STREQUAL "synthetic-control-timer0-disabled")
      list(APPEND current_without_timer0_ids "${id}")
    endif()
  endforeach()
endif()
if(NOT "${legacy_ids}" STREQUAL "${current_without_timer0_ids}")
  message(FATAL_ERROR "timer0-guest: omitted optional inputs changed prior control rows")
endif()
run_fixture_index("${scratch}/one-sided-index" one_sided_result
  one_sided_out one_sided_err pass-only)
if("${one_sided_result}" STREQUAL "0" OR
    NOT "${one_sided_err}" MATCHES "fixture-index: timer0-image-pair-required")
  message(FATAL_ERROR
    "timer0-guest: one-sided Timer 0 control input was not rejected visibly")
endif()

set(trace_one "${scratch}/control-pass-one.trace.jsonl")
set(trace_two "${scratch}/control-pass-two.trace.jsonl")
set(raw_trace "${scratch}/production-pass.trace.jsonl")
file(REMOVE "${trace_one}" "${trace_two}" "${raw_trace}")
run_json(control_pass_one control-pass-one "${control}" 0 PASS guest-mailbox-pass
  --rom "${timer0_pass}" --fixture synthetic-control-timer0-pass --cycles 10000
  --trace-mask 7 --trace-capacity 65536 --trace-output "${trace_one}")
run_json(control_pass_two control-pass-two "${control}" 0 PASS guest-mailbox-pass
  --rom "${timer0_pass}" --fixture synthetic-control-timer0-pass --cycles 10000
  --trace-mask 7 --trace-capacity 65536 --trace-output "${trace_two}")

foreach(name IN ITEMS control_pass_one control_pass_two)
  set(json "${${name}}")
  expect_json_field("${json}" synthetic-control-timer0-pass fixture id)
  expect_json_boolean("${json}" true fixture ran)
  expect_json_field("${json}" pass semantic state)
  expect_json_field("${json}" 7001 semantic case_id)
  expect_json_boolean("${json}" true timer0 if_request)
  expect_time_extension("${json}")
  expect_json_field("${json}" mailbox-complete termination)
  string(JSON cycles GET "${json}" time cycles)
  string(JSON instructions GET "${json}" time instructions)
  if(cycles LESS 1 OR cycles GREATER 10000 OR instructions LESS 1)
    message(FATAL_ERROR "timer0-guest: clean guest exceeded its model-cycle bound")
  endif()
  validate_result("${name}" "${json}")
endforeach()
if(NOT "${control_pass_one}" STREQUAL "${control_pass_two}")
  message(FATAL_ERROR "timer0-guest: fresh control processes reported different results")
endif()
foreach(trace IN ITEMS "${trace_one}" "${trace_two}")
  if(NOT EXISTS "${trace}" OR IS_DIRECTORY "${trace}")
    message(FATAL_ERROR "timer0-guest: clean guest trace is missing")
  endif()
endforeach()
file(READ "${trace_one}" trace_one_text)
file(READ "${trace_two}" trace_two_text)
if("${trace_one_text}" STREQUAL "" OR
    NOT "${trace_one_text}" STREQUAL "${trace_two_text}")
  message(FATAL_ERROR "timer0-guest: fresh control traces differ")
endif()
foreach(json IN ITEMS "${control_pass_one}" "${control_pass_two}")
  expect_json_boolean("${json}" true trace emitted)
  expect_json_boolean("${json}" true trace complete)
endforeach()

run_json(control_disabled control-disabled "${control}" 1 TIMEOUT cycle-budget
  --rom "${timer0_disabled}"
  --fixture synthetic-control-timer0-disabled --cycles 10000)
expect_json_field("${control_disabled}" synthetic-control-timer0-disabled fixture id)
expect_json_boolean("${control_disabled}" true fixture ran)
expect_json_field("${control_disabled}" absent semantic state)
expect_json_field("${control_disabled}" 0 semantic case_id)
expect_json_boolean("${control_disabled}" false timer0 if_request)
expect_time_extension("${control_disabled}")
expect_json_field("${control_disabled}" timeout termination)
validate_result("control-disabled" "${control_disabled}")
classify_timer0_timeout("${control_disabled}" disabled_classification)
if(NOT disabled_classification STREQUAL "timer0-overflow-timeout")
  message(FATAL_ERROR
    "timer0-guest: disabled guest did not produce timer0-overflow-timeout")
endif()
classify_timer0_timeout("${control_pass_one}" clean_classification)
if(clean_classification STREQUAL "timer0-overflow-timeout")
  message(FATAL_ERROR
    "timer0-guest: clean document was accepted by timer0-overflow-timeout fault checker")
endif()
message(STATUS
  "EXPECTED timer0-overflow-timeout: disabled timer reached the bounded guest stop without marker or IF")
message(STATUS
  "PASS timer0-guest: clean document rejected by the timer0-overflow-timeout fault checker")

run_json(production_pass production-pass "${production}" 1 GUEST_STOP unregistered-content
  --rom "${timer0_pass}" --cycles 10000 --trace-mask 7
  --trace-capacity 65536 --trace-output "${raw_trace}")
expect_json_type("${production_pass}" NULL fixture id)
expect_json_boolean("${production_pass}" true fixture ran)
expect_json_field("${production_pass}" pass semantic state)
expect_json_field("${production_pass}" 7001 semantic case_id)
expect_json_boolean("${production_pass}" true timer0 if_request)
expect_time_extension("${production_pass}")
expect_json_field("${production_pass}" unregistered-content termination)
string(JSON production_evidence_count LENGTH "${production_pass}" claim evidence)
if(NOT production_evidence_count EQUAL 0)
  message(FATAL_ERROR "timer0-guest: raw production observation gained fixture admission")
endif()
string(JSON production_cycles GET "${production_pass}" time cycles)
if(production_cycles LESS 1 OR production_cycles GREATER 10000)
  message(FATAL_ERROR "timer0-guest: production raw guest exceeded its model-cycle bound")
endif()
if(NOT EXISTS "${raw_trace}" OR IS_DIRECTORY "${raw_trace}")
  message(FATAL_ERROR "timer0-guest: production raw guest trace is missing")
endif()
file(READ "${raw_trace}" raw_trace_text)
if(NOT "${raw_trace_text}" STREQUAL "${trace_one_text}")
  message(FATAL_ERROR
    "timer0-guest: production raw observation changed guest trace bytes")
endif()

message(STATUS
  "PASS timer0-guest: generated BIOS-free Timer 0 guest, exact digest authority, raw production classification, semantic/IF/model observations, and fresh trace equality")
