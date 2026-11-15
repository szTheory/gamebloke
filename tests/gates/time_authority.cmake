# Compare the configured candidate with a disposable clean compilation of the
# same CPU observation helper. All generated inputs and outputs stay in BIN.
cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "time-authority: missing required argument ${required}")
  endif()
endforeach()
if(NOT CASE STREQUAL "clean")
  message(FATAL_ERROR "time-authority: unsupported case '${CASE}'")
endif()

function(gbl__cache_value key output)
  file(STRINGS "${BIN}/CMakeCache.txt" matching_lines
    REGEX "^${key}:[^=]*=")
  list(LENGTH matching_lines line_count)
  if(NOT line_count EQUAL 1)
    message(FATAL_ERROR "time-authority: cache value '${key}' is missing or ambiguous")
  endif()
  list(GET matching_lines 0 matching_line)
  string(REGEX REPLACE "^[^=]*=" "" value "${matching_line}")
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

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(root "${BIN}/nc/time_authority/${CASE}-${run_id}")
set(input_root "${root}/inputs")
set(object_root "${root}/objects")
set(common_object_root "${object_root}/common")
file(MAKE_DIRECTORY "${input_root}/src" "${input_root}/runner"
  "${input_root}/tests" "${input_root}/include/gamebloke"
  "${input_root}/generated/control" "${object_root}"
  "${common_object_root}" "${root}/test-work")

foreach(source IN ITEMS version.c machine.c bus.c loader.c inspect.c cpu.c
    logo_sha256.h cpu_helpers.h)
  file(COPY_FILE "${SRC}/src/${source}" "${input_root}/src/${source}")
endforeach()
foreach(source IN ITEMS run.c run.h result.c sha256.c sha256.h)
  file(COPY_FILE "${SRC}/runner/${source}" "${input_root}/runner/${source}")
endforeach()
file(COPY_FILE "${SRC}/include/gamebloke/gamebloke.h"
  "${input_root}/include/gamebloke/gamebloke.h")
file(COPY_FILE "${SRC}/tests/determinism_test.c"
  "${input_root}/tests/determinism_test.c")
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

gbl__compile_source("${input_root}/src/cpu.c" "${object_root}/cpu-clean.o"
  clean_cpu_object)
set(clean_executable "${root}/determinism_test-clean")
execute_process(
  COMMAND "${compiler}" ${base_flags} ${config_flags} ${common_objects}
    "${clean_cpu_object}" -o "${clean_executable}" ${link_flags}
    ${link_config_flags}
  RESULT_VARIABLE link_result OUTPUT_VARIABLE link_out ERROR_VARIABLE link_err)
if(NOT "${link_result}" STREQUAL "0")
  message(FATAL_ERROR
    "time-authority: clean link failed; not semantic evidence\n${link_out}${link_err}")
endif()

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
message(STATUS
  "PASS bus.time_authority.clean: candidate and fresh clean compilation match; ${clean_line}")
