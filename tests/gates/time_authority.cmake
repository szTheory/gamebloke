# Compare the configured candidate with a disposable clean compilation of the
# same CPU observation helper. All generated inputs and outputs stay in BIN.
cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "time-authority: missing required argument ${required}")
  endif()
endforeach()
if(NOT CASE MATCHES "^(clean|control)$")
  message(FATAL_ERROR "time-authority: unsupported case '${CASE}'")
endif()

function(gbl__cache_value key output)
  file(STRINGS "${BIN}/CMakeCache.txt" matching_lines
    REGEX "^${key}:")
  list(LENGTH matching_lines line_count)
  if(NOT line_count EQUAL 1)
    message(FATAL_ERROR "time-authority: cache value '${key}' is missing or ambiguous")
  endif()
  list(GET matching_lines 0 matching_line)
  string(FIND "${matching_line}" "=" delimiter_position)
  if(delimiter_position EQUAL -1)
    message(FATAL_ERROR "time-authority: cache value '${key}' has no delimiter")
  endif()
  math(EXPR value_position "${delimiter_position} + 1")
  string(SUBSTRING "${matching_line}" ${value_position} -1 value)
  set(${output} "${value}" PARENT_SCOPE)
endfunction()

function(gbl__compile_source source object output)
  execute_process(
    COMMAND "${compiler}" -std=c17 ${base_flags} ${config_flags}
      ${warning_flags} ${core_flags} ${definitions} ${include_flags}
      -c "${source}" -o "${object}"
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out
    ERROR_VARIABLE compile_err)
  if(NOT "${compile_result}" STREQUAL "0")
    message(FATAL_ERROR
      "time-authority: compile failed for ${source}; not semantic evidence\n${compile_out}${compile_err}")
  endif()
  set(${output} "${object}" PARENT_SCOPE)
endfunction()

function(gbl__link_helper cpu_object executable)
  execute_process(
    COMMAND "${compiler}" ${base_flags} ${config_flags} ${common_objects}
      "${cpu_object}" -o "${executable}" ${link_flags}
      ${link_config_flags}
    RESULT_VARIABLE link_result OUTPUT_VARIABLE link_out ERROR_VARIABLE link_err)
  if(NOT "${link_result}" STREQUAL "0")
    message(FATAL_ERROR
      "time-authority: link failed for ${executable}; not semantic evidence\n${link_out}${link_err}")
  endif()
endfunction()

function(gbl__mutate_cpu cpu_source opcode)
  set(anchor [=[        gbl_cpu_trace_commit(run_options.trace_buffer, trace_records,
                             trace_record_count);]=])
  file(READ "${cpu_source}" cpu_text)
  string(FIND "${cpu_text}" "${anchor}" anchor_position)
  if(anchor_position EQUAL -1)
    message(FATAL_ERROR
      "time-authority: verified non-bus CPU handler anchor is missing")
  endif()
  string(SUBSTRING "${cpu_text}" ${anchor_position} -1 after_anchor)
  string(LENGTH "${anchor}" anchor_length)
  string(SUBSTRING "${after_anchor}" ${anchor_length} -1 after_first_anchor)
  string(FIND "${after_first_anchor}" "${anchor}" duplicate_position)
  if(NOT duplicate_position EQUAL -1)
    message(FATAL_ERROR
      "time-authority: non-bus CPU handler anchor is ambiguous")
  endif()

  set(increment
    "\n        /* disposable time-authority mutation */\n        if (unit.kind == GBL_CPU_UNIT_DATA_PROCESSING &&\n            unit.opcode == UINT32_C(${opcode})) {\n            machine->master_cycles += UINT64_C(1);\n        }")
  string(REPLACE "${anchor}" "${anchor}${increment}" mutated_text
    "${cpu_text}")
  string(REGEX MATCHALL "disposable time-authority mutation"
    increment_sites "${mutated_text}")
  list(LENGTH increment_sites increment_count)
  if(NOT increment_count EQUAL 1)
    message(FATAL_ERROR
      "time-authority: expected exactly one disposable time increment, found ${increment_count}")
  endif()
  file(WRITE "${cpu_source}" "${mutated_text}")
endfunction()

function(gbl__require_hash_mismatch clean_path mutant_path label trace_expectation)
  file(SHA256 "${clean_path}" clean_hash)
  file(SHA256 "${mutant_path}" mutant_hash)
  if("${clean_hash}" STREQUAL "${mutant_hash}")
    message(FATAL_ERROR
      "time-authority: successfully executed ${label} mutant was accepted")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${clean_path}" "${mutant_path}" RESULT_VARIABLE exact_compare)
  if(NOT "${exact_compare}" STREQUAL "1")
    message(FATAL_ERROR
      "time-authority: ${label} observation comparison failed generically (${exact_compare})")
  endif()
  file(READ "${clean_path}" clean_bytes)
  file(READ "${mutant_path}" mutant_bytes)
  string(FIND "${clean_bytes}" "\n" clean_header_end)
  string(FIND "${mutant_bytes}" "\n" mutant_header_end)
  if(clean_header_end EQUAL -1 OR mutant_header_end EQUAL -1)
    message(FATAL_ERROR
      "time-authority: ${label} canonical result header is missing")
  endif()
  math(EXPR clean_trace_start "${clean_header_end} + 1")
  math(EXPR mutant_trace_start "${mutant_header_end} + 1")
  string(SUBSTRING "${clean_bytes}" ${clean_trace_start} -1 clean_trace)
  string(SUBSTRING "${mutant_bytes}" ${mutant_trace_start} -1 mutant_trace)
  if(trace_expectation STREQUAL "same" AND
     NOT "${clean_trace}" STREQUAL "${mutant_trace}")
    message(FATAL_ERROR
      "time-authority: final-only mutant changed trace records; final-cycle binding was not isolated")
  endif()
  if(trace_expectation STREQUAL "different" AND
     "${clean_trace}" STREQUAL "${mutant_trace}")
    message(FATAL_ERROR
      "time-authority: executed non-bus mutation did not alter trace timestamps")
  endif()
  message(STATUS
    "EXPECTED time-authority-trace-hash-mismatch: ${label} mutant hash ${mutant_hash} differs from clean ${clean_hash}")
endfunction()

function(gbl__require_timer_state_mismatch clean_path mutant_path)
  file(READ "${clean_path}" clean_bytes)
  file(READ "${mutant_path}" mutant_bytes)
  string(FIND "${clean_bytes}" "\n" clean_header_end)
  string(FIND "${mutant_bytes}" "\n" mutant_header_end)
  if(clean_header_end EQUAL -1 OR mutant_header_end EQUAL -1)
    message(FATAL_ERROR "time-authority: Timer 0 observation header is missing")
  endif()
  string(SUBSTRING "${clean_bytes}" 0 ${clean_header_end} clean_header)
  string(SUBSTRING "${mutant_bytes}" 0 ${mutant_header_end} mutant_header)
  math(EXPR clean_trace_start "${clean_header_end} + 1")
  math(EXPR mutant_trace_start "${mutant_header_end} + 1")
  string(SUBSTRING "${clean_bytes}" ${clean_trace_start} -1 clean_trace)
  string(SUBSTRING "${mutant_bytes}" ${mutant_trace_start} -1 mutant_trace)
  if(NOT "${clean_trace}" STREQUAL "${mutant_trace}")
    message(FATAL_ERROR
      "time-authority: timer-state control changed ordinary trace bytes")
  endif()
  foreach(field IN ITEMS model qualification semantic_state case_id cycles
      instructions master_cycles interrupt_flags if_request timer0_generation
      timer0_enabled timer0_irq_enabled event_count event0_live
      event0_generation time_state_digest trace_count)
    string(JSON clean_${field} ERROR_VARIABLE clean_${field}_error
      GET "${clean_header}" ${field})
    string(JSON mutant_${field} ERROR_VARIABLE mutant_${field}_error
      GET "${mutant_header}" ${field})
    if(NOT "${clean_${field}_error}" STREQUAL "NOTFOUND" OR
        NOT "${mutant_${field}_error}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR
        "time-authority: timer-state observation field ${field} is missing")
    endif()
  endforeach()
  if(NOT clean_model STREQUAL "gamebloke-timer-model-1" OR
      NOT clean_qualification STREQUAL "project-model-only" OR
      NOT clean_semantic_state STREQUAL "pass" OR
      NOT clean_case_id STREQUAL "7001" OR
      NOT clean_if_request OR NOT clean_timer0_enabled OR
      NOT clean_timer0_irq_enabled OR NOT clean_event0_live OR
      clean_event_count LESS 1 OR
      NOT clean_timer0_generation STREQUAL clean_event0_generation)
    message(FATAL_ERROR
      "time-authority: clean Timer 0 observation lacks live model/IF state")
  endif()
  if(NOT mutant_model STREQUAL clean_model OR
      NOT mutant_qualification STREQUAL clean_qualification OR
      NOT mutant_semantic_state STREQUAL clean_semantic_state OR
      NOT mutant_case_id STREQUAL clean_case_id OR
      NOT mutant_cycles STREQUAL clean_cycles OR
      NOT mutant_instructions STREQUAL clean_instructions OR
      NOT mutant_master_cycles STREQUAL clean_master_cycles OR
      mutant_if_request OR NOT mutant_timer0_enabled OR
      NOT mutant_timer0_irq_enabled OR NOT mutant_event0_live OR
      mutant_timer0_generation STREQUAL clean_timer0_generation OR
      NOT mutant_event0_generation STREQUAL clean_event0_generation OR
      mutant_time_state_digest STREQUAL clean_time_state_digest OR
      NOT mutant_trace_count STREQUAL clean_trace_count)
    message(FATAL_ERROR
      "time-authority: timer-state mutant did not isolate live generation/IF changes")
  endif()
  message(STATUS
    "EXPECTED time-authority-timer-state-mismatch: live timer generation and IF request changed while trace bytes stayed identical")
endfunction()

function(gbl__run_observation executable mode output_path expected_status)
  execute_process(
    COMMAND "${executable}" time-authority "${mode}" "${output_path}"
    RESULT_VARIABLE run_result OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err)
  if(NOT "${run_result}" STREQUAL "0")
    message(FATAL_ERROR
      "time-authority: guest observation failed (${run_result}); not mutation evidence\n${run_out}${run_err}")
  endif()
  if(NOT run_out MATCHES "PASS determinism[.]time_authority: mode=${mode} termination=${expected_status}")
    message(FATAL_ERROR
      "time-authority: observation lacks its expected guest status or complete result line\n${run_out}${run_err}")
  endif()
  if(NOT EXISTS "${output_path}" OR IS_DIRECTORY "${output_path}")
    message(FATAL_ERROR "time-authority: observation output is missing")
  endif()
  file(SIZE "${output_path}" output_size)
  if(output_size LESS 128)
    message(FATAL_ERROR "time-authority: observation output is empty or incomplete")
  endif()
  set(GBL_LAST_RUN_OUTPUT "${run_out}" PARENT_SCOPE)
endfunction()

function(gbl__verify_timer_observation candidate_executable clean_executable
    output_root)
  set(candidate_output "${output_root}/timer-candidate.jsonl")
  set(clean_output "${output_root}/timer-clean.jsonl")
  gbl__run_observation("${candidate_executable}" timer
    "${candidate_output}" mailbox-complete)
  set(candidate_line "${GBL_LAST_RUN_OUTPUT}")
  gbl__run_observation("${clean_executable}" timer "${clean_output}"
    mailbox-complete)
  set(clean_line "${GBL_LAST_RUN_OUTPUT}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${candidate_output}" "${clean_output}"
    RESULT_VARIABLE timer_compare)
  if(NOT "${timer_compare}" STREQUAL "0")
    message(FATAL_ERROR
      "time-authority: fresh Timer 0 candidate and clean observations differ\n${candidate_line}${clean_line}")
  endif()
  set(GBL_TIMER_CLEAN_OUTPUT "${clean_output}" PARENT_SCOPE)
  message(STATUS
    "PASS time-authority: clean Timer 0 guest binds live timer/event/IF state to the canonical observation")
endfunction()

gbl__cache_value(CMAKE_C_COMPILER compiler)
gbl__cache_value(CMAKE_BUILD_TYPE build_type)
gbl__cache_value(CMAKE_C_FLAGS c_flags)
gbl__cache_value(CMAKE_EXE_LINKER_FLAGS linker_flags)
gbl__cache_value(CMAKE_PROJECT_VERSION project_version)
string(TOUPPER "${build_type}" build_type_upper)
if(build_type_upper STREQUAL "")
  set(build_type_upper NONE)
endif()
gbl__cache_value(CMAKE_C_FLAGS_${build_type_upper} c_flags_config)
gbl__cache_value(CMAKE_EXE_LINKER_FLAGS_${build_type_upper} linker_flags_config)

file(GLOB compiler_metadata "${BIN}/CMakeFiles/*/CMakeCCompiler.cmake")
list(LENGTH compiler_metadata compiler_metadata_count)
if(NOT compiler_metadata_count EQUAL 1)
  message(FATAL_ERROR "time-authority: configured compiler metadata is missing or ambiguous")
endif()
list(GET compiler_metadata 0 compiler_metadata_path)
file(STRINGS "${compiler_metadata_path}" compiler_id_lines
  REGEX "^set[(]CMAKE_C_COMPILER_ID ")
if(NOT compiler_id_lines MATCHES "^set[(]CMAKE_C_COMPILER_ID \"(GNU|Clang|AppleClang)\"[)]$")
  message(FATAL_ERROR "time-authority: unsupported configured compiler identity")
endif()

separate_arguments(base_flags NATIVE_COMMAND "${c_flags}")
separate_arguments(config_flags NATIVE_COMMAND "${c_flags_config}")
separate_arguments(link_flags NATIVE_COMMAND "${linker_flags}")
separate_arguments(link_config_flags NATIVE_COMMAND "${linker_flags_config}")
set(warning_flags
  -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
  -Wmissing-prototypes -Wundef -Wcast-qual -Werror)
set(core_flags -fno-stack-protector -U_FORTIFY_SOURCE)
message(STATUS
  "time-authority: compiler=${compiler}; compile-flags=${base_flags} ${config_flags} ${warning_flags} ${core_flags}; link-flags=${link_flags} ${link_config_flags}")

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(root "${BIN}/nc/time_authority/${CASE}-${run_id}")
set(input_root "${root}/inputs")
set(object_root "${root}/objects")
set(common_object_root "${object_root}/common")
file(MAKE_DIRECTORY "${input_root}/src" "${input_root}/runner"
  "${input_root}/tests" "${input_root}/include/gamebloke"
  "${input_root}/generated/control" "${object_root}"
  "${common_object_root}" "${root}/test-work")

foreach(source IN ITEMS version.c machine.c audio.c bus.c loader.c inspect.c
    cpu.c logo_sha256.h cpu_helpers.h ppu_internal.h audio_internal.h)
  file(COPY_FILE "${SRC}/src/${source}" "${input_root}/src/${source}")
endforeach()
foreach(source IN ITEMS run.c run.h result.c sha256.c sha256.h)
  file(COPY_FILE "${SRC}/runner/${source}" "${input_root}/runner/${source}")
endforeach()
file(COPY_FILE "${SRC}/include/gamebloke/gamebloke.h"
  "${input_root}/include/gamebloke/gamebloke.h")
file(COPY_FILE "${SRC}/tests/determinism_test.c"
  "${input_root}/tests/determinism_test.c")
file(COPY_FILE "${SRC}/tests/state_check.h"
  "${input_root}/tests/state_check.h")
file(COPY_FILE "${BIN}/generated/control/fixture_index.h"
  "${input_root}/generated/control/fixture_index.h")

set(definitions
  "-DGBL_VERSION_STRING=\"${project_version}\""
  "-DGBL_DETERMINISM_FIXTURE_DIR=\"${BIN}/generated/fixtures\""
  "-DGBL_DETERMINISM_SCRATCH_DIR=\"${root}/test-work\"")
set(include_flags
  "-I${input_root}/include" "-I${input_root}/runner"
  "-I${input_root}/tests" "-I${input_root}/generated/control")

set(common_sources
  "${input_root}/src/version.c"
  "${input_root}/src/machine.c"
  "${input_root}/src/audio.c"
  "${input_root}/src/bus.c"
  "${input_root}/src/loader.c"
  "${input_root}/src/inspect.c"
  "${input_root}/runner/run.c"
  "${input_root}/runner/result.c"
  "${input_root}/runner/sha256.c"
  "${input_root}/tests/determinism_test.c")
set(common_objects)
foreach(source IN LISTS common_sources)
  get_filename_component(source_name "${source}" NAME)
  string(REPLACE ".c" ".o" object_name "${source_name}")
  gbl__compile_source("${source}" "${common_object_root}/${object_name}"
    object_path)
  list(APPEND common_objects "${object_path}")
endforeach()

file(SHA256 "${SRC}/src/cpu.c" source_cpu_hash_before)
gbl__compile_source("${input_root}/src/cpu.c" "${object_root}/cpu-clean.o"
  clean_cpu_object)
set(clean_executable "${root}/determinism_test-clean")
gbl__link_helper("${clean_cpu_object}" "${clean_executable}")

if(DEFINED CANDIDATE AND NOT "${CANDIDATE}" STREQUAL "")
  set(candidate "${CANDIDATE}")
else()
  set(candidate "${BIN}/determinism_test")
endif()
if(NOT EXISTS "${candidate}" OR IS_DIRECTORY "${candidate}")
  message(FATAL_ERROR "time-authority: clean candidate binary is missing: ${candidate}")
endif()

set(candidate_output "${root}/candidate-full.jsonl")
set(clean_output "${root}/clean-full.jsonl")
gbl__run_observation("${candidate}" full "${candidate_output}" 2)
set(candidate_line "${GBL_LAST_RUN_OUTPUT}")
gbl__run_observation("${clean_executable}" full "${clean_output}" 2)
set(clean_line "${GBL_LAST_RUN_OUTPUT}")
file(SHA256 "${candidate_output}" candidate_hash)
file(SHA256 "${clean_output}" clean_hash)
if(NOT "${candidate_hash}" STREQUAL "${clean_hash}")
  message(FATAL_ERROR
    "time-authority: clean candidate and disposable clean baseline differ\n${candidate_line}${clean_line}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
  "${candidate_output}" "${clean_output}"
  RESULT_VARIABLE exact_compare)
if(NOT "${exact_compare}" STREQUAL "0")
  message(FATAL_ERROR "time-authority: clean observations are not byte-identical")
endif()
gbl__verify_timer_observation("${candidate}" "${clean_executable}" "${root}")
if(CASE STREQUAL "clean")
  message(STATUS
    "PASS bus.time_authority.clean: candidate and fresh clean compilation match; ${clean_line}")
  return()
endif()

set(clean_final_output "${root}/clean-final.jsonl")
gbl__run_observation("${clean_executable}" final "${clean_final_output}" 0)

foreach(label IN ITEMS midpath final)
  set(mutant_root "${root}/mutants/${label}")
  file(MAKE_DIRECTORY "${mutant_root}/src")
  set(mutant_cpu_source "${mutant_root}/src/cpu.c")
  file(COPY_FILE "${input_root}/src/cpu.c" "${mutant_cpu_source}")
  file(COPY_FILE "${input_root}/src/cpu_helpers.h"
    "${mutant_root}/src/cpu_helpers.h")
  if(label STREQUAL "midpath")
    set(target_opcode "0xE3A00403")
    set(observation_mode full)
    set(expected_status 2)
    set(trace_expectation different)
  else()
    set(target_opcode "0xE1A04002")
    set(observation_mode final)
    set(expected_status 0)
    set(trace_expectation same)
  endif()
  gbl__mutate_cpu("${mutant_cpu_source}" "${target_opcode}")
  file(SHA256 "${input_root}/src/cpu.c" clean_cpu_hash)
  file(SHA256 "${mutant_cpu_source}" mutant_cpu_hash)
  if("${clean_cpu_hash}" STREQUAL "${mutant_cpu_hash}")
    message(FATAL_ERROR "time-authority: disposable CPU mutation changed no bytes")
  endif()
  set(mutant_cpu_object "${object_root}/cpu-${label}.o")
  gbl__compile_source("${mutant_cpu_source}" "${mutant_cpu_object}"
    mutant_cpu_object)
  set(mutant_executable "${mutant_root}/determinism_test-${label}")
  gbl__link_helper("${mutant_cpu_object}" "${mutant_executable}")
  set(mutant_output "${root}/${label}.jsonl")
  if(observation_mode STREQUAL "full")
    set(clean_reference "${clean_output}")
  else()
    set(clean_reference "${clean_final_output}")
  endif()
  gbl__run_observation("${mutant_executable}" "${observation_mode}"
    "${mutant_output}" "${expected_status}")
  gbl__require_hash_mismatch("${clean_reference}" "${mutant_output}"
    "${label}" "${trace_expectation}")
endforeach()

set(timer_mutant_output "${root}/timer-mutant.jsonl")
gbl__run_observation("${clean_executable}" timer-mutant
  "${timer_mutant_output}" mailbox-complete)
set(timer_mutant_line "${GBL_LAST_RUN_OUTPUT}")
if(NOT timer_mutant_line MATCHES
    "EXPECTED time-authority-timer-state-mismatch")
  message(FATAL_ERROR
    "time-authority: timer-state mutation did not emit its named mismatch\n${timer_mutant_line}")
endif()
gbl__require_timer_state_mismatch("${root}/timer-clean.jsonl"
  "${timer_mutant_output}")

file(SHA256 "${SRC}/src/cpu.c" source_cpu_hash_after)
if(NOT "${source_cpu_hash_before}" STREQUAL "${source_cpu_hash_after}")
  message(FATAL_ERROR
    "time-authority: production src/cpu.c changed during disposable mutation")
endif()
message(STATUS
  "PASS bus.time_authority.control: clean companions pass; midpath/final clock mutants and live timer/event/IF state mutation are rejected with named evidence")
