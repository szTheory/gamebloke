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
include("${SRC}/tests/gates/mutation_objects.cmake")

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
    set(changed_source src/bus.c)
  else()
    set(changed_source "")
  endif()
  gbl__reuse_mutation_objects("${sandbox}" bus "${changed_source}" executable reused)
  if(reused)
    set(${output_executable} "${executable}" PARENT_SCOPE)
    return()
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
  elseif(mutation STREQUAL "trace_loss")
    gbl_text_mutate("${cpu_source}"
      [=[    if (record_count == 0u) {
        return;
    }
    for (index = 0u; index < record_count; ++index) {]=]
      [=[    if (record_count == 0u) {
        return;
    }
    record_count--;
    for (index = 0u; index < record_count; ++index) {]=])
  elseif(mutation STREQUAL "budget_overshoot")
    gbl_text_mutate("${cpu_source}"
      [=[        if (run_options.unit_limit != 0u &&
            completed_units >= run_options.unit_limit) {]=]
      [=[        if (completed_units == UINT64_MAX) {]=])
  elseif(mutation STREQUAL "block_effect_order")
    # Resolve destinations using the restored bank instead of the captured old
    # bank: a real exception-return ordering error, not an altered expectation.
    gbl_text_mutate("${cpu_source}" "effect->bank_cpsr, value);"
      "unit->restore_cpsr_from_spsr ? unit->status_effects[0].value : effect->bank_cpsr, value);")
  elseif(mutation STREQUAL "loaded_pc")
    gbl_text_mutate("${cpu_source}"
      "(unit->results[unit->fetch_result_index] &"
      "((unit->results[unit->fetch_result_index] ^ UINT32_C(4)) &")
  elseif(mutation STREQUAL "maximum_trace")
    gbl_text_mutate("${cpu_source}"
      "    for (index = 0u; index < record_count; ++index) {"
      "    if (record_count == 19u) { record_count--; }\n    for (index = 0u; index < record_count; ++index) {")
  elseif(mutation STREQUAL "multiply_flags")
    gbl_text_mutate("${cpu_source}" "flags = gbl_cpu_nz_flags(short_result);"
      "flags = gbl_cpu_nz_flags(short_result) ^ GBL_CPU_CPSR_N;")
  elseif(mutation STREQUAL "multiply_accumulate")
    gbl_text_mutate("${cpu_source}" "result = accumulator + product;"
      "result = accumulator ^ product;")
  elseif(mutation STREQUAL "bl_halves")
    gbl_text_mutate("${cpu_source}" "offset << 12u" "offset << 11u")
  elseif(mutation STREQUAL "late_refusal")
    gbl_text_mutate("${cpu_source}" [=[                               &preflight_reason)) {
            run_result.termination =]=] [=[                               &preflight_reason)) {
            if (unit.data_slot_count > 0u &&
                unit.data_slots[0].operation == GBL_CPU_DATA_WRITE) {
                const gbl_cpu_data_slot *first = &unit.data_slots[0];
                (void)gbl_bus_write(machine, first->address, first->width,
                                    first->access_class, first->write_value);
            }
            run_result.termination =]=])
  elseif(mutation STREQUAL "post_effect_request")
    gbl_text_mutate("${cpu_source}" [=[                        machine, slot->address, slot->width,
                        slot->access_class, slot->write_value);]=]
      [=[                        machine, slot->address, data_index == 1u ? 3u : slot->width,
                        slot->access_class, slot->write_value);]=])
  else()
    message(FATAL_ERROR "mutation: unknown CPU case '${mutation}'")
  endif()
endfunction()

function(gbl__mutate_peek inspect_source public_header mutation)
  if(NOT mutation STREQUAL "peek_effect")
    message(FATAL_ERROR "mutation: unknown peek case '${mutation}'")
  endif()

  file(READ "${inspect_source}" inspect_text)
  string(REPLACE "gbl_machine_peek(const gbl_machine *machine,"
    "gbl_machine_peek(gbl_machine *machine," inspect_text "${inspect_text}")
  string(REPLACE "gbl_mailbox_read(const gbl_machine *machine)"
    "gbl_mailbox_read(gbl_machine *machine)" inspect_text "${inspect_text}")
  string(REPLACE "    gbl_inspect_copy_bytes(out, &machine->iwram[offset], length);" [=[    if (length == 1u && address == UINT32_C(0x03000000)) {
        machine->master_cycles += UINT64_C(1);
    }
    gbl_inspect_copy_bytes(out, &machine->iwram[offset], length);]=] inspect_text
    "${inspect_text}")
  file(WRITE "${inspect_source}" "${inspect_text}")

  file(READ "${public_header}" header_text)
  string(REPLACE "gbl_machine_peek(const gbl_machine *machine,"
    "gbl_machine_peek(gbl_machine *machine," header_text "${header_text}")
  string(REPLACE "gbl_mailbox_read(const gbl_machine *machine)"
    "gbl_mailbox_read(gbl_machine *machine)" header_text "${header_text}")
  file(WRITE "${public_header}" "${header_text}")
endfunction()

function(gbl__copy_cpu_inputs sandbox)
  file(MAKE_DIRECTORY "${sandbox}/src" "${sandbox}/include/gamebloke"
    "${sandbox}/tests")
  foreach(source IN ITEMS version.c machine.c bus.c loader.c cpu.c inspect.c)
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

function(gbl__cpu_compile_flags base_output config_output warning_output
    core_output link_output link_config_output)
  separate_arguments(base_flags NATIVE_COMMAND "${C_FLAGS}")
  separate_arguments(config_flags NATIVE_COMMAND "${C_FLAGS_CONFIG}")
  separate_arguments(link_flags NATIVE_COMMAND "${LINKER_FLAGS}")
  separate_arguments(link_config_flags NATIVE_COMMAND "${LINKER_FLAGS_CONFIG}")
  set(warning_flags
    -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
    -Wmissing-prototypes -Wundef -Wcast-qual -Werror)
  set(core_flags -fno-stack-protector -U_FORTIFY_SOURCE)
  set(${base_output} "${base_flags}" PARENT_SCOPE)
  set(${config_output} "${config_flags}" PARENT_SCOPE)
  set(${warning_output} "${warning_flags}" PARENT_SCOPE)
  set(${core_output} "${core_flags}" PARENT_SCOPE)
  set(${link_output} "${link_flags}" PARENT_SCOPE)
  set(${link_config_output} "${link_config_flags}" PARENT_SCOPE)
endfunction()

function(gbl__compile_cpu sandbox mutation output_executable)
  gbl__copy_cpu_inputs("${sandbox}")
  if(NOT mutation STREQUAL "")
    if(mutation STREQUAL "shift_carry")
      gbl__mutate_cpu("${sandbox}/src/cpu_helpers.h" "${mutation}")
    elseif(mutation STREQUAL "peek_effect")
      gbl__mutate_peek("${sandbox}/src/inspect.c"
        "${sandbox}/include/gamebloke/gamebloke.h" "${mutation}")
    else()
      gbl__mutate_cpu("${sandbox}/src/cpu.c" "${mutation}")
    endif()
  endif()

  if(mutation STREQUAL "" OR mutation IN_LIST GBL_CPU_SOURCE_ONLY_MUTATIONS)
    set(changed_source "")
    if(NOT mutation STREQUAL "")
      set(changed_source src/cpu.c)
    endif()
    gbl__reuse_mutation_objects("${sandbox}" cpu "${changed_source}" executable reused)
    if(reused)
      set(${output_executable} "${executable}" PARENT_SCOPE)
      return()
    endif()
  endif()

  gbl__cpu_compile_flags(base_flags config_flags warning_flags core_flags
    link_flags link_config_flags)
  set(executable "${sandbox}/cpu_test")
  execute_process(
    COMMAND "${COMPILER}" -std=c17 ${base_flags} ${config_flags}
      ${warning_flags} ${core_flags} "-DGBL_VERSION_STRING=\"${VERSION}\""
      "-I${sandbox}/include" "-I${sandbox}/tests"
      "${sandbox}/src/version.c" "${sandbox}/src/machine.c"
      "${sandbox}/src/bus.c" "${sandbox}/src/loader.c"
      "${sandbox}/src/cpu.c" "${sandbox}/src/inspect.c"
      "${sandbox}/tests/cpu_test.c"
      -o "${executable}" ${link_flags} ${link_config_flags}
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out
    ERROR_VARIABLE compile_err)
  if(NOT "${compile_result}" STREQUAL "0")
    message(FATAL_ERROR
      "mutation: compile failed (${compile_result}); not semantic evidence\n${compile_out}${compile_err}")
  endif()
  set(${output_executable} "${executable}" PARENT_SCOPE)
endfunction()

# Only these mutations edit src/cpu.c and leave every other copied source and
# header byte-identical. New and header-affecting mutations use the full-build
# path unless they are deliberately added here after dependency review.
set(GBL_CPU_SOURCE_ONLY_MUTATIONS
  pc_refill bank_alias exception_lr vector_fetch trace_loss budget_overshoot
  block_effect_order loaded_pc maximum_trace multiply_flags multiply_accumulate
  bl_halves late_refusal post_effect_request)

function(gbl__assert_cpu_reuse_inputs mutant_sandbox clean_sandbox mutation)
  if("${mutant_sandbox}" STREQUAL "${clean_sandbox}")
    message(FATAL_ERROR "mutation: reuse-control: mutant and clean copies are not isolated")
  endif()

  file(GLOB_RECURSE mutant_inputs RELATIVE "${mutant_sandbox}"
    "${mutant_sandbox}/*")
  file(GLOB_RECURSE clean_inputs RELATIVE "${clean_sandbox}"
    "${clean_sandbox}/*")
  list(SORT mutant_inputs)
  list(SORT clean_inputs)
  if(NOT "${mutant_inputs}" STREQUAL "${clean_inputs}")
    message(FATAL_ERROR
      "mutation: reuse-control: copied input sets differ for '${mutation}'")
  endif()
  list(FIND mutant_inputs "src/cpu.c" cpu_source_index)
  if(cpu_source_index EQUAL -1)
    message(FATAL_ERROR "mutation: reuse-control: src/cpu.c is not a copied input")
  endif()

  foreach(input IN LISTS mutant_inputs)
    if(NOT input STREQUAL "src/cpu.c")
      file(SHA256 "${mutant_sandbox}/${input}" mutant_hash)
      file(SHA256 "${clean_sandbox}/${input}" clean_hash)
      if(NOT "${mutant_hash}" STREQUAL "${clean_hash}")
        message(FATAL_ERROR
          "mutation: reuse-control: shared input changed for '${mutation}': ${input}")
      endif()
    endif()
  endforeach()

  file(SHA256 "${mutant_sandbox}/src/cpu.c" mutant_cpu_hash)
  file(SHA256 "${clean_sandbox}/src/cpu.c" clean_cpu_hash)
  if("${mutant_cpu_hash}" STREQUAL "${clean_cpu_hash}")
    message(FATAL_ERROR
      "mutation: reuse-control: '${mutation}' did not change its isolated CPU source")
  endif()

  list(LENGTH mutant_inputs input_count)
  math(EXPR shared_input_count "${input_count} - 1")
  message(STATUS
    "mutation: reuse-control passed for '${mutation}': ${shared_input_count} shared inputs match; src/cpu.c differs")
endfunction()

function(gbl__compile_cpu_object sandbox source object_dir object_name output_object)
  gbl__cpu_compile_flags(base_flags config_flags warning_flags core_flags
    link_flags link_config_flags)
  file(MAKE_DIRECTORY "${object_dir}")
  set(object "${object_dir}/${object_name}.o")
  execute_process(
    COMMAND "${COMPILER}" -std=c17 ${base_flags} ${config_flags}
      ${warning_flags} ${core_flags} "-DGBL_VERSION_STRING=\"${VERSION}\""
      "-I${sandbox}/include" "-I${sandbox}/tests"
      -c "${sandbox}/${source}" -o "${object}"
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out
    ERROR_VARIABLE compile_err)
  if(NOT "${compile_result}" STREQUAL "0")
    message(FATAL_ERROR
      "mutation: compile failed for ${source} (${compile_result}); not semantic evidence\n${compile_out}${compile_err}")
  endif()
  set(${output_object} "${object}" PARENT_SCOPE)
endfunction()

function(gbl__link_cpu sandbox object_dir variant objects output_executable)
  gbl__cpu_compile_flags(base_flags config_flags warning_flags core_flags
    link_flags link_config_flags)
  file(MAKE_DIRECTORY "${object_dir}")
  set(executable "${object_dir}/${variant}-cpu_test")
  execute_process(
    COMMAND "${COMPILER}" -std=c17 ${base_flags} ${config_flags}
      ${warning_flags} ${core_flags} "-DGBL_VERSION_STRING=\"${VERSION}\""
      "-I${sandbox}/include" "-I${sandbox}/tests"
      ${objects} -o "${executable}" ${link_flags} ${link_config_flags}
    RESULT_VARIABLE link_result OUTPUT_VARIABLE link_out
    ERROR_VARIABLE link_err)
  if(NOT "${link_result}" STREQUAL "0")
    message(FATAL_ERROR
      "mutation: link failed for ${variant} (${link_result}); not semantic evidence\n${link_out}${link_err}")
  endif()
  set(${output_executable} "${executable}" PARENT_SCOPE)
endfunction()

function(gbl__compile_cpu_reused_pair root mutation output_mutant
    output_clean)
  set(mutant_sandbox "${root}/mutant")
  set(clean_sandbox "${root}/clean")
  gbl__copy_cpu_inputs("${mutant_sandbox}")
  gbl__copy_cpu_inputs("${clean_sandbox}")
  gbl__mutate_cpu("${mutant_sandbox}/src/cpu.c" "${mutation}")
  gbl__assert_cpu_reuse_inputs("${mutant_sandbox}" "${clean_sandbox}"
    "${mutation}")

  # Compile unchanged translation units once for this pair. The identity guard
  # above covers every copied input except cpu.c, including all headers and the
  # real cpu_test.c. The two CPU objects and links remain independent.
  set(shared_objects "")
  foreach(source IN ITEMS src/version.c src/machine.c src/bus.c src/loader.c)
    get_filename_component(object_name "${source}" NAME_WE)
    gbl__compile_cpu_object("${clean_sandbox}" "${source}"
      "${root}/objects/shared" "${object_name}" object)
    list(APPEND shared_objects "${object}")
  endforeach()
  gbl__compile_cpu_object("${clean_sandbox}" src/inspect.c
    "${root}/objects/shared" inspect inspect_object)
  gbl__compile_cpu_object("${clean_sandbox}" tests/cpu_test.c
    "${root}/objects/shared" cpu_test cpu_test_object)

  gbl__compile_cpu_object("${mutant_sandbox}" src/cpu.c
    "${root}/objects/mutant" cpu_mutant mutant_cpu_object)
  gbl__compile_cpu_object("${clean_sandbox}" src/cpu.c
    "${root}/objects/clean" cpu_clean clean_cpu_object)
  if("${mutant_cpu_object}" STREQUAL "${clean_cpu_object}")
    message(FATAL_ERROR
      "mutation: reuse-control: mutant and clean CPU objects are not isolated for '${mutation}'")
  endif()
  file(SHA256 "${mutant_cpu_object}" mutant_object_hash)
  file(SHA256 "${clean_cpu_object}" clean_object_hash)
  if("${mutant_object_hash}" STREQUAL "${clean_object_hash}")
    message(FATAL_ERROR
      "mutation: reuse-control: mutant and clean CPU objects are identical for '${mutation}'")
  endif()

  # Preserve the original source/object order at link time.
  set(mutant_objects "${shared_objects};${mutant_cpu_object};${inspect_object};${cpu_test_object}")
  set(clean_objects "${shared_objects};${clean_cpu_object};${inspect_object};${cpu_test_object}")
  gbl__link_cpu("${mutant_sandbox}" "${root}/mutant" mutant
    "${mutant_objects}" mutant_executable)
  gbl__link_cpu("${clean_sandbox}" "${root}/clean" clean
    "${clean_objects}" clean_executable)
  if("${mutant_executable}" STREQUAL "${clean_executable}")
    message(FATAL_ERROR
      "mutation: reuse-control: mutant and clean executables are not isolated")
  endif()
  message(STATUS
    "mutation: reuse-control passed for '${mutation}': six common objects shared; CPU objects and executables are separate")

  set(${output_mutant} "${mutant_executable}" PARENT_SCOPE)
  set(${output_clean} "${clean_executable}" PARENT_SCOPE)
endfunction()

function(gbl__run_cpu_pair mutation label selector)
  string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef run_id)
  set(root "${BIN}/nc/core_mutation/${CASE}-${label}-${run_id}")
  if(mutation IN_LIST GBL_CPU_SOURCE_ONLY_MUTATIONS AND NOT DEFINED PREPARED_ROOT)
    gbl__compile_cpu_reused_pair("${root}" "${mutation}" mutant_executable
      clean_executable)
  else()
    if(NOT mutation IN_LIST GBL_CPU_SOURCE_ONLY_MUTATIONS)
      message(STATUS
        "mutation: full-rebuild path retained for '${mutation}' (headers or other shared inputs may differ)")
    endif()
    gbl__compile_cpu("${root}/mutant" "${mutation}" mutant_executable)
    gbl__compile_cpu("${root}/clean" "" clean_executable)
  endif()
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
  elseif(mutation STREQUAL "peek_effect")
    set(failure_regex "not ok 3 - peek and mailbox reads leave full state and caller guards unchanged")
  elseif(mutation STREQUAL "trace_loss")
    set(failure_regex "not ok 2 - exact trace capacity records instruction, data and refill order")
  elseif(mutation STREQUAL "budget_overshoot")
    set(failure_regex "not ok 29 - unit limit stops after one instruction despite spare cycle budget")
  elseif(mutation STREQUAL "block_effect_order")
    set(failure_regex "not ok 5 - LDM\\^ PC returns from FIQ using captured banked destinations and Thumb SPSR")
  elseif(mutation STREQUAL "loaded_pc")
    set(failure_regex "not ok 1 - LDR PC refills from the actual BIOS-absent word result")
  elseif(mutation STREQUAL "maximum_trace")
    set(failure_regex "not ok 1 - sixteen-register LDM uses the final bus result for PC and fits 19 trace records")
  elseif(mutation STREQUAL "multiply_flags")
    set(failure_regex "not ok 1 - MULS high-bit product sets N and preserves chosen C/V")
  elseif(mutation STREQUAL "multiply_accumulate")
    set(failure_regex "not ok 6 - UMLALS carries low accumulator overflow into the high word")
  elseif(mutation STREQUAL "bl_halves")
    set(failure_regex "not ok 2 - Thumb BL isolated prefix literal vector 1 uses banked LR and exact fetches")
  elseif(mutation STREQUAL "late_refusal")
    set(failure_regex "not ok 9 - maximum STM16-PC boundary 1 preserves whole-unit effects and caller guards")
  elseif(mutation STREQUAL "post_effect_request")
    set(failure_regex "not ok 8 - maximum STM16-PC boundary 0 preserves whole-unit effects and caller guards")
  else()
    message(FATAL_ERROR "mutation: no semantic failure assertion for '${mutation}'")
  endif()
  gbl_expect_fail(COMMAND "${mutant_executable}" "${selector}"
    REGEX "${failure_regex}")

  if(selector STREQUAL "arithmetic")
    set(clean_regex "ok 28 - helper defines large LSR/ASR/ROR counts without host UB")
  elseif(selector STREQUAL "modes")
    set(clean_regex "ok 1 - all seven modes select shared and private physical registers")
  elseif(selector STREQUAL "exceptions")
    set(clean_regex "ok 8 - ARM and Thumb exceptions use exact address bases, masks, SPSRs, banks and vectors")
  elseif(selector STREQUAL "no_bios")
    set(clean_regex "ok 1 - ARM and Thumb SWI/undefined guests stop with named BIOS-none causes")
  elseif(selector STREQUAL "peek")
    set(clean_regex "ok 3 - peek and mailbox reads leave full state and caller guards unchanged")
  elseif(selector STREQUAL "mailbox")
    set(clean_regex "ok 4 - invalid state and failing verdict without a case ID are malformed")
  elseif(selector STREQUAL "trace" OR selector STREQUAL "trace_capacity")
    set(clean_regex "ok 2 - exact trace capacity records instruction, data and refill order")
  elseif(mutation MATCHES "^(block_effect_order|loaded_pc|maximum_trace|multiply_flags|multiply_accumulate|bl_halves|late_refusal|post_effect_request)$")
    string(REPLACE "not ok " "ok " clean_regex "${failure_regex}")
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
  # The native registry is the selector authority. Derive the complete list so
  # future groups cannot be silently omitted from this clean copied core.
  file(STRINGS "${SRC}/tests/cpu_test.c" groups REGEX "CPU_TEST_GROUP\\(\"")
  set(selectors "")
  foreach(group IN LISTS groups)
    string(REGEX REPLACE ".*CPU_TEST_GROUP\\(\"([^\"]+)\".*" "\\1" selector "${group}")
    list(APPEND selectors "${selector}")
  endforeach()
  list(LENGTH selectors selector_count)
  if(selector_count LESS 39)
    message(FATAL_ERROR "mutation: incomplete native selector registry")
  endif()
  foreach(selector IN LISTS selectors)
    gbl_expect_pass(COMMAND "${clean_executable}" "${selector}")
  endforeach()
  message(STATUS "mutation: clean copied core exercised all ${selector_count} native selectors: ${selectors}")
  if(DEFINED PREPARED_ROOT)
    gbl__mutation_reuse_controls()
  endif()
endfunction()

function(gbl__compile_loader sandbox mutation output_executable expectation_mode)
  gbl__copy_loader_inputs("${sandbox}")
  if(NOT mutation STREQUAL "")
    gbl__mutate_loader("${sandbox}/src/loader.c" "${mutation}")
    set(changed_source src/loader.c)
  else()
    set(changed_source "")
  endif()
  if(NOT expectation_mode STREQUAL "synthetic")
    gbl__reuse_mutation_objects("${sandbox}" loader "${changed_source}" executable reused)
    if(reused)
      set(${output_executable} "${executable}" PARENT_SCOPE)
      return()
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

if(CASE STREQUAL "prepare")
  gbl__prepare_mutation_objects()
elseif(CASE STREQUAL "reuse-control")
  set(sandbox "${BIN}/nc/mutation-reuse-${CONTROL}")
  file(REMOVE_RECURSE "${sandbox}")
  gbl__copy_cpu_inputs("${sandbox}")
  if(CONTROL STREQUAL "flags")
    string(APPEND C_FLAGS " -DGBL_REUSE_MISMATCH")
  elseif(CONTROL STREQUAL "source")
    file(APPEND "${sandbox}/src/cpu.c" "\n/* isolated identity control */\n")
  elseif(CONTROL STREQUAL "header")
    file(APPEND "${sandbox}/src/cpu_helpers.h" "\n/* isolated identity control */\n")
  else()
    message(FATAL_ERROR "mutation: unknown-reuse-control")
  endif()
  gbl__reuse_mutation_objects("${sandbox}" cpu "" unused reused)
  message(FATAL_ERROR "mutation: changed-reuse-input-accepted")
elseif(CASE MATCHES "^cpu_")
  if(CASE STREQUAL "cpu_clean")
    gbl__run_cpu_clean()
  else()
    string(REPLACE "cpu_" "" cpu_mutation "${CASE}")
    gbl__run_cpu_pair("${cpu_mutation}" "${cpu_mutation}" "${SELECTOR}")
    if(cpu_mutation STREQUAL "multiply_flags")
      gbl__run_cpu_pair(multiply_accumulate multiply_accumulate "${SELECTOR}")
    elseif(cpu_mutation STREQUAL "late_refusal")
      gbl__run_cpu_pair(post_effect_request post_effect_request "${SELECTOR}")
    endif()
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
