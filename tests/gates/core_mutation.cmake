# Compile the production bus and its real semantic test from a disposable
# source copy, then prove one exact source mutation fails and a clean companion
# passes. All generated source and binaries stay beneath the selected build dir.
cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE SELECTOR COMPILER COMPILER_ID VERSION)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "mutation: missing required argument ${required}")
  endif()
endforeach()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

if(NOT COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
  message(FATAL_ERROR "mutation: unsupported compiler id '${COMPILER_ID}'")
endif()

function(gbl__mutate_bus bus_source mutation)
  if(mutation STREQUAL "ewram_mirror")
    gbl_text_mutate("${bus_source}" "#define GBL_EWRAM_MASK 0x3FFFFu"
      "#define GBL_EWRAM_MASK 0x3FFFDu")
  elseif(mutation STREQUAL "iwram_mirror")
    gbl_text_mutate("${bus_source}" "#define GBL_IWRAM_MASK 0x7FFFu"
      "#define GBL_IWRAM_MASK 0x7FFDu")
  elseif(mutation STREQUAL "vram_mirror")
    gbl_text_mutate("${bus_source}" "#define GBL_VRAM_MASK 0x1FFFFu"
      "#define GBL_VRAM_MASK 0x1FFFDu")
  elseif(mutation STREQUAL "video_bg_obj")
    gbl_text_mutate("${bus_source}" "if (index < obj_base) {"
      "if (index >= obj_base) {")
  elseif(mutation STREQUAL "video_oam_byte")
    set(find [=[        if (request_width == 1u) {
            return GBL_BUS_OUTCOME_RAM;
        }
]=])
    set(replace [=[        if (request_width == 1u) {
            memory_byte = gbl_bus_memory_byte(machine, region, address);
            *memory_byte = value;
            return GBL_BUS_OUTCOME_RAM;
        }
]=])
    gbl_text_mutate("${bus_source}" "${find}" "${replace}")
  elseif(mutation STREQUAL "clock_overflow")
    gbl_text_mutate("${bus_source}"
      "if (UINT64_C(1) > UINT64_MAX - machine->master_cycles) {"
      "if (machine->master_cycles < UINT64_MAX && UINT64_C(1) > UINT64_MAX - machine->master_cycles) {")
  else()
    message(FATAL_ERROR "mutation: unknown case '${mutation}'")
  endif()
endfunction()

function(gbl__copy_inputs sandbox)
  file(MAKE_DIRECTORY "${sandbox}/src" "${sandbox}/include/gamebloke"
    "${sandbox}/tests")
  foreach(source IN ITEMS version.c machine.c bus.c loader.c)
    file(COPY_FILE "${SRC}/src/${source}" "${sandbox}/src/${source}")
  endforeach()
  file(COPY_FILE "${SRC}/include/gamebloke/gamebloke.h"
    "${sandbox}/include/gamebloke/gamebloke.h")
  file(COPY_FILE "${SRC}/tests/bus_test.c" "${sandbox}/tests/bus_test.c")
  file(COPY_FILE "${SRC}/tests/check.h" "${sandbox}/tests/check.h")
endfunction()

function(gbl__compile_bus sandbox mutation output_executable)
  gbl__copy_inputs("${sandbox}")
  if(NOT mutation STREQUAL "")
    gbl__mutate_bus("${sandbox}/src/bus.c" "${mutation}")
  endif()

  separate_arguments(base_flags NATIVE_COMMAND "${C_FLAGS}")
  separate_arguments(config_flags NATIVE_COMMAND "${C_FLAGS_CONFIG}")
  separate_arguments(link_flags NATIVE_COMMAND "${LINKER_FLAGS}")
  separate_arguments(link_config_flags NATIVE_COMMAND "${LINKER_FLAGS_CONFIG}")
  set(warning_flags
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
    -Wmissing-prototypes -Wundef -Wcast-qual -Werror)
  set(core_flags -fno-stack-protector -U_FORTIFY_SOURCE)
  set(executable "${sandbox}/bus_test")
  execute_process(
    COMMAND "${COMPILER}" -std=c17 ${base_flags} ${config_flags}
      ${warning_flags} ${core_flags} "-DGBL_VERSION_STRING=\"${VERSION}\""
      "-I${sandbox}/include" "-I${sandbox}/tests"
      "${sandbox}/src/version.c" "${sandbox}/src/machine.c"
      "${sandbox}/src/bus.c" "${sandbox}/src/loader.c"
      "${sandbox}/tests/bus_test.c" -o "${executable}"
      ${link_flags} ${link_config_flags}
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out
    ERROR_VARIABLE compile_err)
  if(NOT "${compile_result}" STREQUAL "0")
    message(FATAL_ERROR
      "mutation: compile failed (${compile_result}); not semantic evidence\n${compile_out}${compile_err}")
  endif()
  set(${output_executable} "${executable}" PARENT_SCOPE)
endfunction()

function(gbl__run_pair mutation label)
  string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef run_id)
  set(root "${BIN}/nc/core_mutation/${CASE}-${label}-${run_id}")
  gbl__compile_bus("${root}/mutant" "${mutation}" mutant_executable)
  if(NOT mutation STREQUAL "")
    gbl_expect_fail(COMMAND "${mutant_executable}" "${SELECTOR}"
      REGEX "check failed")
  endif()
  gbl__compile_bus("${root}/clean" "" clean_executable)
  gbl_expect_pass(COMMAND "${clean_executable}" "${SELECTOR}"
    REGEX "bus: ${SELECTOR} ok")
endfunction()

if(CASE STREQUAL "clean")
  string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef run_id)
  set(root "${BIN}/nc/core_mutation/${CASE}-positive-${run_id}")
  gbl__compile_bus("${root}/clean" "" clean_executable)
  gbl_expect_pass(COMMAND "${clean_executable}" "${SELECTOR}"
    REGEX "bus: ${SELECTOR} ok")
elseif(CASE STREQUAL "video_bytes")
  gbl__run_pair("video_bg_obj" "bg_obj")
  # This independent variant proves the OAM byte-write assertion is meaningful.
  gbl__run_pair("video_oam_byte" "oam")
else()
  gbl__run_pair("${CASE}" "${CASE}")
endif()
