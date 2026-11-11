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
  foreach(source IN ITEMS version.c machine.c bus.c loader.c cpu.c)
    file(COPY_FILE "${SRC}/src/${source}" "${sandbox}/src/${source}")
  endforeach()
  file(COPY_FILE "${SRC}/src/logo_sha256.h" "${sandbox}/src/logo_sha256.h")
  file(COPY_FILE "${SRC}/src/cpu_helpers.h" "${sandbox}/src/cpu_helpers.h")
  file(COPY_FILE "${SRC}/include/gamebloke/gamebloke.h"
    "${sandbox}/include/gamebloke/gamebloke.h")
  file(COPY_FILE "${SRC}/tests/bus_test.c" "${sandbox}/tests/bus_test.c")
  file(COPY_FILE "${SRC}/tests/check.h" "${sandbox}/tests/check.h")
  file(COPY_FILE "${SRC}/tests/state_check.h"
    "${sandbox}/tests/state_check.h")
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
      "${sandbox}/src/cpu.c"
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

function(gbl__mutate_loader loader_source mutation)
  if(mutation STREQUAL "allocate_before_validate")
    gbl_text_mutate("${loader_source}" "    if (length == 0u) {" [=[    if (length == 0u) {
        void *probe = machine->allocator.allocate(
            machine->allocator.context, length);
        if (probe != NULL) {
            machine->allocator.release(machine->allocator.context, probe, length);
        }]=])
  elseif(mutation STREQUAL "refusal_state")
    gbl_text_mutate("${loader_source}" "    if (length < 0xC0u) {" [=[    if (length < 0xC0u) {
        machine->open_bus ^= UINT32_C(1);]=])
  elseif(mutation STREQUAL "refusal_rom_identity")
    gbl_text_mutate("${loader_source}" "    if (length < 0xC0u) {" [=[    if (length < 0xC0u) {
        if (machine->rom != NULL) {
            machine->rom = (uint8_t *)(uintptr_t)image;
        }]=])
  elseif(mutation STREQUAL "refusal_allocator_identity")
    gbl_text_mutate("${loader_source}" "        result.reason = GBL_LOAD_REASON_ROM_SHORT;" [=[        result.reason = GBL_LOAD_REASON_ROM_SHORT;
        machine->allocator.allocate = NULL;]=])
  elseif(mutation STREQUAL "allocation_state")
    gbl_text_mutate("${loader_source}" "    if (candidate == NULL) {" [=[    if (candidate == NULL) {
        machine->open_bus ^= UINT32_C(1);]=])
  elseif(mutation STREQUAL "caller_bytes")
    gbl_text_mutate("${loader_source}" "    if (length < 0xC0u) {" [=[    if (length < 0xC0u) {
        *(uint8_t *)(uintptr_t)image ^= UINT8_C(1);]=])
  elseif(mutation STREQUAL "logo_claim")
    gbl_text_mutate("${loader_source}"
      "    diagnostics.canonical_logo = GBL_HEADER_LOGO_UNCHECKED;"
      "    diagnostics.canonical_logo = GBL_HEADER_LOGO_CHECKED_MATCH;")
  elseif(mutation STREQUAL "logo_digest_bypass")
    gbl_text_mutate("${loader_source}"
      [=[    if (GBL_LOGO_EXPECTED_SHA256[0] != '\0') {]=]
      [=[    if (candidate == NULL) {]=])
  elseif(mutation STREQUAL "logo_digest_forced_match")
    gbl_text_mutate("${loader_source}"
      "                : GBL_HEADER_LOGO_CHECKED_MISMATCH;"
      "                : GBL_HEADER_LOGO_CHECKED_MATCH;")
  else()
    message(FATAL_ERROR "mutation: unknown loader case '${mutation}'")
  endif()
endfunction()

function(gbl__copy_loader_inputs sandbox)
  file(MAKE_DIRECTORY "${sandbox}/src" "${sandbox}/include/gamebloke"
    "${sandbox}/tests")
  foreach(source IN ITEMS version.c machine.c bus.c loader.c cpu.c)
    file(COPY_FILE "${SRC}/src/${source}" "${sandbox}/src/${source}")
  endforeach()
  file(COPY_FILE "${SRC}/src/logo_sha256.h" "${sandbox}/src/logo_sha256.h")
  file(COPY_FILE "${SRC}/src/cpu_helpers.h" "${sandbox}/src/cpu_helpers.h")
  file(COPY_FILE "${SRC}/include/gamebloke/gamebloke.h"
    "${sandbox}/include/gamebloke/gamebloke.h")
  file(COPY_FILE "${SRC}/tests/loader_test.c" "${sandbox}/tests/loader_test.c")
  file(COPY_FILE "${SRC}/tests/check.h" "${sandbox}/tests/check.h")
  file(COPY_FILE "${SRC}/tests/state_check.h"
    "${sandbox}/tests/state_check.h")
endfunction()

function(gbl__mutate_cpu cpu_source mutation)
  if(mutation STREQUAL "pc_refill")
    gbl_text_mutate("${cpu_source}" "unit->branch_target = target;"
      "unit->branch_target = target + UINT32_C(2);")
  elseif(mutation STREQUAL "shift_carry")
    gbl_text_mutate("${cpu_source}"
      "result.value = ((uint32_t)carry_in << 31u) | (value >> 1u);"
      "result.value = ((uint32_t)carry_in << 31u) | (value >> 2u);")
  elseif(mutation STREQUAL "bank_alias")
    gbl_text_mutate("${cpu_source}" "if (mode == GBL_CPU_MODE_FIQ) {"
      "if (mode == UINT32_C(0x10)) {")
  elseif(mutation STREQUAL "exception_lr")
    # The first next+4 rule is IRQ; leave FIQ's independent rule intact.
    gbl_text_mutate("${cpu_source}"
      "context->next_address, UINT32_C(4),"
      "context->next_address, UINT32_C(8),")
  elseif(mutation STREQUAL "vector_fetch")
    gbl_text_mutate("${cpu_source}" "cpu->stopped = true;"
      "cpu->stopped = false;")
    gbl_text_mutate("${cpu_source}"
      [=[            run_result.reason = GBL_RUN_REASON_GUEST_STOP;
            break;]=]
      [=[            run_result.reason = GBL_RUN_REASON_GUEST_STOP;
            continue;]=])
  else()
    message(FATAL_ERROR "mutation: unknown CPU case '${mutation}'")
  endif()
endfunction()

function(gbl__copy_cpu_inputs sandbox)
  file(MAKE_DIRECTORY "${sandbox}/src" "${sandbox}/include/gamebloke"
    "${sandbox}/tests")
  foreach(source IN ITEMS version.c machine.c bus.c loader.c cpu.c)
    file(COPY_FILE "${SRC}/src/${source}" "${sandbox}/src/${source}")
  endforeach()
  file(COPY_FILE "${SRC}/src/logo_sha256.h" "${sandbox}/src/logo_sha256.h")
  file(COPY_FILE "${SRC}/src/cpu_helpers.h" "${sandbox}/src/cpu_helpers.h")
  file(COPY_FILE "${SRC}/include/gamebloke/gamebloke.h"
    "${sandbox}/include/gamebloke/gamebloke.h")
  foreach(test_header IN ITEMS check.h state_check.h)
    file(COPY_FILE "${SRC}/tests/${test_header}"
      "${sandbox}/tests/${test_header}")
  endforeach()
  file(COPY_FILE "${SRC}/tests/cpu_test.c" "${sandbox}/tests/cpu_test.c")
endfunction()

function(gbl__compile_cpu sandbox mutation output_executable)
  gbl__copy_cpu_inputs("${sandbox}")
  if(NOT mutation STREQUAL "")
    if(mutation STREQUAL "shift_carry")
      gbl__mutate_cpu("${sandbox}/src/cpu_helpers.h" "${mutation}")
    else()
      gbl__mutate_cpu("${sandbox}/src/cpu.c" "${mutation}")
    endif()
  endif()

  separate_arguments(base_flags NATIVE_COMMAND "${C_FLAGS}")
  separate_arguments(config_flags NATIVE_COMMAND "${C_FLAGS_CONFIG}")
  separate_arguments(link_flags NATIVE_COMMAND "${LINKER_FLAGS}")
  separate_arguments(link_config_flags NATIVE_COMMAND "${LINKER_FLAGS_CONFIG}")
  set(warning_flags
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
    -Wmissing-prototypes -Wundef -Wcast-qual -Werror)
  set(core_flags -fno-stack-protector -U_FORTIFY_SOURCE)
  set(executable "${sandbox}/cpu_test")
  execute_process(
    COMMAND "${COMPILER}" -std=c17 ${base_flags} ${config_flags}
      ${warning_flags} ${core_flags} "-DGBL_VERSION_STRING=\"${VERSION}\""
      "-I${sandbox}/include" "-I${sandbox}/tests"
      "${sandbox}/src/version.c" "${sandbox}/src/machine.c"
      "${sandbox}/src/bus.c" "${sandbox}/src/loader.c"
      "${sandbox}/src/cpu.c" "${sandbox}/tests/cpu_test.c"
      -o "${executable}" ${link_flags} ${link_config_flags}
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out
    ERROR_VARIABLE compile_err)
  if(NOT "${compile_result}" STREQUAL "0")
    message(FATAL_ERROR
      "mutation: compile failed (${compile_result}); not semantic evidence\n${compile_out}${compile_err}")
  endif()
  set(${output_executable} "${executable}" PARENT_SCOPE)
endfunction()

function(gbl__run_cpu_pair mutation label selector)
  string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef run_id)
  set(root "${BIN}/nc/core_mutation/${CASE}-${label}-${run_id}")
  gbl__compile_cpu("${root}/mutant" "${mutation}" mutant_executable)
  if(mutation STREQUAL "pc_refill")
    set(failure_regex "not ok 1 - ARM BL links the next instruction and refills without stale slots")
  elseif(mutation STREQUAL "shift_carry")
    set(failure_regex "not ok 2 - shift helper distinguishes RRX, LSR zero and ASR zero")
  elseif(mutation STREQUAL "bank_alias")
    set(failure_regex "not ok 1 - all seven modes select shared and private physical registers")
  elseif(mutation STREQUAL "exception_lr")
    set(failure_regex "not ok 8 - ARM and Thumb exceptions use exact address bases, masks, SPSRs, banks and vectors")
  elseif(mutation STREQUAL "vector_fetch")
    set(failure_regex "not ok 1 - ARM and Thumb SWI/undefined guests stop with named BIOS-none causes")
  else()
    message(FATAL_ERROR "mutation: no semantic failure assertion for '${mutation}'")
  endif()
  gbl_expect_fail(COMMAND "${mutant_executable}" "${selector}"
    REGEX "${failure_regex}")

  gbl__compile_cpu("${root}/clean" "" clean_executable)
  if(selector STREQUAL "arithmetic")
    set(clean_regex "ok 28 - helper defines large LSR/ASR/ROR counts without host UB")
  elseif(selector STREQUAL "modes")
    set(clean_regex "ok 1 - all seven modes select shared and private physical registers")
  elseif(selector STREQUAL "exceptions")
    set(clean_regex "ok 8 - ARM and Thumb exceptions use exact address bases, masks, SPSRs, banks and vectors")
  elseif(selector STREQUAL "no_bios")
    set(clean_regex "ok 1 - ARM and Thumb SWI/undefined guests stop with named BIOS-none causes")
  else()
    set(clean_regex "ok 1 - ARM BL links the next instruction and refills without stale slots")
  endif()
  gbl_expect_pass(COMMAND "${clean_executable}" "${selector}"
    REGEX "${clean_regex}")
endfunction()

function(gbl__run_cpu_clean)
  string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef run_id)
  set(root "${BIN}/nc/core_mutation/${CASE}-positive-${run_id}")
  gbl__compile_cpu("${root}/clean" "" clean_executable)
  foreach(selector IN ITEMS arithmetic thumb decode alignment modes exceptions no_bios)
    gbl_expect_pass(COMMAND "${clean_executable}" "${selector}")
  endforeach()
endfunction()

function(gbl__compile_loader sandbox mutation output_executable expectation_mode)
  gbl__copy_loader_inputs("${sandbox}")
  if(NOT mutation STREQUAL "")
    gbl__mutate_loader("${sandbox}/src/loader.c" "${mutation}")
  endif()

  separate_arguments(base_flags NATIVE_COMMAND "${C_FLAGS}")
  separate_arguments(config_flags NATIVE_COMMAND "${C_FLAGS_CONFIG}")
  separate_arguments(link_flags NATIVE_COMMAND "${LINKER_FLAGS}")
  separate_arguments(link_config_flags NATIVE_COMMAND "${LINKER_FLAGS_CONFIG}")
  set(warning_flags
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
    -Wmissing-prototypes -Wundef -Wcast-qual -Werror)
  set(core_flags -fno-stack-protector -U_FORTIFY_SOURCE)
  set(logo_flags "")
  if(expectation_mode STREQUAL "synthetic")
    set(logo_flags
      "-DGBL_LOGO_EXPECTED_SHA256=\"8ec8d30af0acb1e6ff15d8ea332a0e6b60ab60ab41cef968e07c7706b349d9d1\""
      -DGBL_TEST_LOGO_EXPECTATION_MODE=1)
  endif()
  set(executable "${sandbox}/loader_test")
  execute_process(
    COMMAND "${COMPILER}" -std=c17 ${base_flags} ${config_flags}
      ${warning_flags} ${core_flags} "-DGBL_VERSION_STRING=\"${VERSION}\""
      ${logo_flags}
      "-I${sandbox}/include" "-I${sandbox}/tests"
      "${sandbox}/src/version.c" "${sandbox}/src/machine.c"
      "${sandbox}/src/bus.c" "${sandbox}/src/loader.c"
      "${sandbox}/src/cpu.c"
      "${sandbox}/tests/loader_test.c" -o "${executable}"
      ${link_flags} ${link_config_flags}
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out
    ERROR_VARIABLE compile_err)
  if(NOT "${compile_result}" STREQUAL "0")
    message(FATAL_ERROR
      "mutation: compile failed (${compile_result}); not semantic evidence\n${compile_out}${compile_err}")
  endif()
  set(${output_executable} "${executable}" PARENT_SCOPE)
endfunction()

function(gbl__run_loader_pair mutation label)
  string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef run_id)
  set(root "${BIN}/nc/core_mutation/${CASE}-${label}-${run_id}")
  set(expectation_mode "")
  if(mutation MATCHES "^logo_digest_(bypass|forced_match)$")
    set(expectation_mode synthetic)
  endif()
  gbl__compile_loader("${root}/mutant" "${mutation}" mutant_executable
    "${expectation_mode}")
  set(failure_regex "check failed")
  if(mutation MATCHES "^refusal_(rom|allocator)_identity$")
    set(failure_regex "check failed: gbl_test_same_instance_identity_equal")
  elseif(mutation STREQUAL "logo_digest_bypass")
    set(failure_regex
      "check failed: diagnostics.canonical_logo == GBL_HEADER_LOGO_CHECKED_MATCH")
  elseif(mutation STREQUAL "logo_digest_forced_match")
    set(failure_regex
      "check failed: diagnostics.canonical_logo == GBL_HEADER_LOGO_CHECKED_MISMATCH")
  endif()
  gbl_expect_fail(COMMAND "${mutant_executable}" "${SELECTOR}"
    REGEX "${failure_regex}")
  gbl__compile_loader("${root}/clean" "" clean_executable
    "${expectation_mode}")
  gbl_expect_pass(COMMAND "${clean_executable}" "${SELECTOR}"
    REGEX "loader: ${SELECTOR} ok")
endfunction()

function(gbl__run_loader_clean)
  string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef run_id)
  set(root "${BIN}/nc/core_mutation/${CASE}-positive-${run_id}")
  gbl__compile_loader("${root}/clean" "" clean_executable "")
  gbl_expect_pass(COMMAND "${clean_executable}" lengths
    REGEX "loader: lengths ok")
  gbl_expect_pass(COMMAND "${clean_executable}" diagnostics
    REGEX "loader: diagnostics ok")
endfunction()

if(CASE MATCHES "^cpu_")
  if(CASE STREQUAL "cpu_clean")
    gbl__run_cpu_clean()
  else()
    string(REPLACE "cpu_" "" cpu_mutation "${CASE}")
    gbl__run_cpu_pair("${cpu_mutation}" "${cpu_mutation}" "${SELECTOR}")
  endif()
elseif(CASE MATCHES "^loader_")
  if(CASE STREQUAL "loader_clean")
    gbl__run_loader_clean()
  else()
    string(REPLACE "loader_" "" loader_mutation "${CASE}")
    gbl__run_loader_pair("${loader_mutation}" "${loader_mutation}")
  endif()
elseif(CASE STREQUAL "clean")
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
