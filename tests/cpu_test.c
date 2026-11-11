#include "gamebloke/gamebloke.h"

#include "../src/cpu_helpers.h"
#include "state_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_IMAGE_SIZE 0xC0u
#define TEST_MOV_R0_PC UINT32_C(0xE1A0000F)
#define TEST_BRANCH_TO_08000010 UINT32_C(0xEA000001)
#define TEST_MOV_R1_R0 UINT32_C(0xE1A01000)
#define TEST_MOV_R2_R0 UINT32_C(0xE1A02000)
#define TEST_CPSR_N UINT32_C(0x80000000)
#define TEST_CPSR_Z UINT32_C(0x40000000)
#define TEST_CPSR_C UINT32_C(0x20000000)
#define TEST_CPSR_V UINT32_C(0x10000000)
#define TEST_CPSR_MODE_MASK UINT32_C(0x0000001F)
#define TEST_CPSR_USER UINT32_C(0x00000010)
#define TEST_CPSR_FIQ UINT32_C(0x00000011)
#define TEST_CPSR_IRQ UINT32_C(0x00000012)
#define TEST_CPSR_SVC UINT32_C(0x00000013)
#define TEST_CPSR_ABT UINT32_C(0x00000017)
#define TEST_CPSR_UND UINT32_C(0x0000001B)
#define TEST_CPSR_SYSTEM UINT32_C(0x0000001F)

static unsigned int test_number;
static unsigned int test_failures;

typedef struct cpu_test_allocator_state {
    bool fail_next_allocation;
} cpu_test_allocator_state;

static void test_case(bool passed, const char *description)
{
    test_number++;
    printf("%s %u - %s\n", passed ? "ok" : "not ok", test_number,
           description);
    if (!passed) {
        test_failures++;
    }
}

static void *cpu_test_allocate(void *context, size_t size)
{
    cpu_test_allocator_state *state = context;

    if (state != NULL && state->fail_next_allocation) {
        state->fail_next_allocation = false;
        return NULL;
    }
    return malloc(size);
}

static void cpu_test_release(void *context, void *pointer, size_t size)
{
    (void)context;
    (void)size;
    free(pointer);
}

static void write_word(uint8_t *bytes, size_t offset, uint32_t word)
{
    bytes[offset] = (uint8_t)(word & UINT32_C(0xFF));
    bytes[offset + 1u] = (uint8_t)((word >> 8u) & UINT32_C(0xFF));
    bytes[offset + 2u] = (uint8_t)((word >> 16u) & UINT32_C(0xFF));
    bytes[offset + 3u] = (uint8_t)((word >> 24u) & UINT32_C(0xFF));
}

static void write_halfword(uint8_t *bytes, size_t offset, uint16_t halfword)
{
    bytes[offset] = (uint8_t)(halfword & UINT16_C(0x00FF));
    bytes[offset + 1u] = (uint8_t)(halfword >> 8u);
}

static void make_empty_image(uint8_t image[TEST_IMAGE_SIZE])
{
    memset(image, 0, TEST_IMAGE_SIZE);
}

static void make_program(uint8_t image[TEST_IMAGE_SIZE])
{
    size_t index;

    for (index = 0u; index < TEST_IMAGE_SIZE; ++index) {
        image[index] = 0u;
    }
    write_word(image, 0x00u, TEST_MOV_R0_PC);
    write_word(image, 0x04u, TEST_BRANCH_TO_08000010);
    write_word(image, 0x08u, TEST_MOV_R1_R0);
    write_word(image, 0x0Cu, TEST_MOV_R1_R0);
    write_word(image, 0x10u, TEST_MOV_R2_R0);
}

static bool machine_initialize(gbl_machine *machine,
                               cpu_test_allocator_state *allocator_state)
{
    const gbl_allocator allocator = {
        allocator_state, cpu_test_allocate, cpu_test_release
    };

    return gbl_machine_init(machine, &allocator) == GBL_BUS_STATUS_OK;
}

static bool machine_load_with_allocator(
    gbl_machine *machine, const uint8_t image[TEST_IMAGE_SIZE],
    cpu_test_allocator_state *allocator_state)
{
    gbl_load_result load_result;

    if (!machine_initialize(machine, allocator_state)) {
        return false;
    }
    load_result = gbl_load_rom(machine, image, TEST_IMAGE_SIZE);
    if (load_result.status != GBL_LOAD_STATUS_OK ||
        load_result.reason != GBL_LOAD_REASON_NONE) {
        gbl_machine_dispose(machine);
        return false;
    }
    return true;
}

static bool machine_load(gbl_machine *machine,
                         const uint8_t image[TEST_IMAGE_SIZE])
{
    return machine_load_with_allocator(machine, image, NULL);
}

static void seed_nondefault_cpu_state(gbl_cpu_state *cpu)
{
    size_t index;

    for (index = 0u; index < 8u; ++index) {
        cpu->r0_7[index] = UINT32_C(0x11000000) + (uint32_t)index;
    }
    for (index = 0u; index < 7u; ++index) {
        cpu->usr_r8_14[index] = UINT32_C(0x22000000) + (uint32_t)index;
        cpu->fiq_r8_14[index] = UINT32_C(0x33000000) + (uint32_t)index;
    }
    for (index = 0u; index < 2u; ++index) {
        cpu->irq_r13_14[index] = UINT32_C(0x44000000) + (uint32_t)index;
        cpu->svc_r13_14[index] = UINT32_C(0x55000000) + (uint32_t)index;
        cpu->abt_r13_14[index] = UINT32_C(0x66000000) + (uint32_t)index;
        cpu->und_r13_14[index] = UINT32_C(0x77000000) + (uint32_t)index;
    }
    for (index = 0u; index < 5u; ++index) {
        cpu->spsr[index] = UINT32_C(0x88000000) + (uint32_t)index;
    }
    cpu->cpsr = UINT32_C(0xA0000013);
    cpu->execution_address = UINT32_C(0x08000040);
    cpu->pipeline[0] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000040), UINT32_C(0xE1A00000), 4u, true
    };
    cpu->pipeline[1] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000044), UINT32_C(0xE1A01000), 4u, true
    };
    cpu->next_fetch_sequential = true;
    cpu->retired_instructions = UINT64_C(0x123456789);
    cpu->stopped = true;
    cpu->stop_reason = GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
    cpu->last_exception = (gbl_cpu_last_exception){
        GBL_EXCEPTION_FIQ,
        UINT32_C(0x08000050),
        UINT32_C(0x08000054),
        UINT32_C(0x08000050),
        false
    };
}

static bool run_units(gbl_machine *machine, uint64_t cycles, uint64_t units,
                      gbl_run_result *result)
{
    const gbl_run_options options = { cycles, units };

    return gbl_cpu_run(machine, &options, result) == GBL_RUN_STATUS_OK;
}

static bool run_arm_instruction(gbl_machine *machine, uint32_t opcode,
                                uint32_t cpsr, uint64_t cycle_budget,
                                gbl_run_result *result)
{
    const uint32_t address = UINT32_C(0x08000000);

    machine->cpu.execution_address = address;
    machine->cpu.cpsr = (cpsr & ~UINT32_C(0x1F)) | TEST_CPSR_SYSTEM;
    machine->cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        address, opcode, 4u, true
    };
    machine->cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        address + UINT32_C(4), UINT32_C(0xE1A00000), 4u, true
    };
    machine->cpu.next_fetch_sequential = true;
    machine->cpu.stopped = false;
    machine->cpu.stop_reason = GBL_CPU_STOP_NONE;
    return run_units(machine, cycle_budget, 1u, result);
}

static bool run_arm_instruction_in_mode(gbl_machine *machine,
                                        uint32_t opcode, uint32_t cpsr,
                                        gbl_run_result *result)
{
    const uint32_t address = UINT32_C(0x08000000);

    machine->cpu.execution_address = address;
    machine->cpu.cpsr = cpsr;
    machine->cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        address, opcode, 4u, true
    };
    machine->cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        address + UINT32_C(4), UINT32_C(0xE1A00000), 4u, true
    };
    machine->cpu.next_fetch_sequential = true;
    machine->cpu.stopped = false;
    machine->cpu.stop_reason = GBL_CPU_STOP_NONE;
    return run_units(machine, UINT64_C(1), 1u, result);
}

static bool run_single_instruction(gbl_machine *machine, uint32_t opcode,
                                   uint32_t cpsr, gbl_run_result *result)
{
    const uint64_t cycle_budget =
        (opcode & UINT32_C(0x10)) != 0u ? UINT64_C(2) : UINT64_C(1);

    return run_arm_instruction(machine, opcode, cpsr, cycle_budget, result);
}

static bool run_thumb_instruction(gbl_machine *machine, uint16_t opcode,
                                  uint32_t cpsr, uint64_t cycle_budget,
                                  gbl_run_result *result)
{
    const uint32_t address = UINT32_C(0x08000000);

    machine->cpu.execution_address = address;
    machine->cpu.cpsr = (cpsr & ~UINT32_C(0x1F)) | TEST_CPSR_SYSTEM |
                        UINT32_C(0x00000020);
    machine->cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        address, (uint32_t)opcode, 2u, true
    };
    machine->cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        address + UINT32_C(2), UINT32_C(0x00004600), 2u, true
    };
    machine->cpu.next_fetch_sequential = true;
    machine->cpu.stopped = false;
    machine->cpu.stop_reason = GBL_CPU_STOP_NONE;
    return run_units(machine, cycle_budget, 1u, result);
}

static void test_lifecycle(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    uint8_t image_before[TEST_IMAGE_SIZE];
    uint8_t replacement[TEST_IMAGE_SIZE];
    uint8_t replacement_before[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    cpu_test_allocator_state allocator_state = { false };
    bool loaded;

    make_program(image);
    memcpy(image_before, image, sizeof(image_before));
    make_program(replacement);
    memcpy(replacement_before, replacement, sizeof(replacement_before));
    loaded = machine_load_with_allocator(&machine, image, &allocator_state);
    test_case(loaded && machine.initialized &&
                  gbl_test_cpu_is_direct_start(&machine.cpu),
              "load initializes the complete direct-start CPU state");
    if (!loaded) {
        test_case(false, "reset clears CPU banks and restores the direct-start policy");
        test_case(false, "successful replacement load cold-resets pipeline and counters");
        test_case(false, "canonical equality and digest cover every CPU field");
        test_case(false, "invalid run arguments preserve machine and output storage");
        test_case(false, "failed allocation preserves CPU state and caller bytes");
        test_case(false, "refused ROM load preserves every persistent CPU field");
        test_case(false, "run without a loaded ROM is refused without effects");
        test_case(false, "CPU state remains isolated between machine instances");
        return;
    }

    seed_nondefault_cpu_state(&machine.cpu);
    machine.master_cycles = 33u;
    gbl_machine_reset(&machine);
    test_case(gbl_test_cpu_is_direct_start(&machine.cpu) &&
                  machine.master_cycles == 0u,
              "reset clears CPU banks and restores the direct-start policy");

    seed_nondefault_cpu_state(&machine.cpu);
    machine.master_cycles = UINT64_C(0x123456789);
    (void)gbl_load_rom(&machine, replacement, sizeof(replacement));
    test_case(gbl_test_cpu_is_direct_start(&machine.cpu) &&
                  machine.master_cycles == 0u,
              "successful replacement load cold-resets pipeline and counters");

    {
        gbl_machine changed = machine;
        const uint64_t base_digest = gbl_test_canonical_digest(&machine);
        bool all_fields_detected = true;

#define CHECK_CPU_FIELD(change)                                             \
        do {                                                                \
            changed = machine;                                              \
            change;                                                         \
            if (gbl_test_canonical_equal(&machine, &changed) ||             \
                gbl_test_canonical_digest(&changed) == base_digest) {       \
                all_fields_detected = false;                                \
            }                                                               \
        } while (0)

        CHECK_CPU_FIELD(changed.cpu.r0_7[7] = 1u);
        CHECK_CPU_FIELD(changed.cpu.usr_r8_14[6] = 1u);
        CHECK_CPU_FIELD(changed.cpu.fiq_r8_14[6] = 1u);
        CHECK_CPU_FIELD(changed.cpu.irq_r13_14[1] = 1u);
        CHECK_CPU_FIELD(changed.cpu.svc_r13_14[1] = 1u);
        CHECK_CPU_FIELD(changed.cpu.abt_r13_14[1] = 1u);
        CHECK_CPU_FIELD(changed.cpu.und_r13_14[1] = 1u);
        CHECK_CPU_FIELD(changed.cpu.spsr[4] = 1u);
        CHECK_CPU_FIELD(changed.cpu.cpsr ^= 1u);
        CHECK_CPU_FIELD(changed.cpu.execution_address ^= 1u);
        CHECK_CPU_FIELD(changed.cpu.pipeline[0].address = 1u);
        CHECK_CPU_FIELD(changed.cpu.pipeline[0].opcode = 1u);
        CHECK_CPU_FIELD(changed.cpu.pipeline[0].width = 1u);
        CHECK_CPU_FIELD(changed.cpu.pipeline[0].valid = true);
        CHECK_CPU_FIELD(changed.cpu.pipeline[1].address = 1u);
        CHECK_CPU_FIELD(changed.cpu.pipeline[1].opcode = 1u);
        CHECK_CPU_FIELD(changed.cpu.pipeline[1].width = 1u);
        CHECK_CPU_FIELD(changed.cpu.pipeline[1].valid = true);
        CHECK_CPU_FIELD(changed.cpu.next_fetch_sequential = true);
        CHECK_CPU_FIELD(changed.cpu.retired_instructions = 1u);
        CHECK_CPU_FIELD(changed.cpu.stopped = true);
        CHECK_CPU_FIELD(changed.cpu.stop_reason =
                            GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION);
        CHECK_CPU_FIELD(changed.cpu.last_exception.kind =
                            GBL_EXCEPTION_UNDEFINED);
        CHECK_CPU_FIELD(changed.cpu.last_exception.current_address = 1u);
        CHECK_CPU_FIELD(changed.cpu.last_exception.next_address = 1u);
        CHECK_CPU_FIELD(changed.cpu.last_exception.fault_address = 1u);
        CHECK_CPU_FIELD(changed.cpu.last_exception.thumb_state = true);
#undef CHECK_CPU_FIELD

        int alternate_allocator_identity = 1;
        changed = machine;
        changed.allocator.context = &alternate_allocator_identity;
        test_case(all_fields_detected &&
                      gbl_test_canonical_equal(&machine, &changed) &&
                      !gbl_test_same_instance_identity_equal(&machine,
                                                             &changed),
                  "canonical equality and digest cover every CPU field while allocator identity stays separate");
    }

    seed_nondefault_cpu_state(&machine.cpu);
    machine.master_cycles = UINT64_C(0x123456789);
    {
        const gbl_machine before = machine;
        const gbl_run_result output_before = {
            GBL_RUN_PROGRESS, GBL_RUN_REASON_UNIT_LIMIT, 77u, 88u
        };
        const gbl_run_options options = { 1u, 1u };
        gbl_run_result output = output_before;
        const bool null_options = gbl_cpu_run(&machine, NULL, &output) ==
                                  GBL_RUN_STATUS_INVALID_ARGUMENT;
        const bool null_machine = gbl_cpu_run(NULL, &options, &output) ==
                                  GBL_RUN_STATUS_INVALID_ARGUMENT;
        const bool null_result = gbl_cpu_run(&machine, &options, NULL) ==
                                 GBL_RUN_STATUS_INVALID_ARGUMENT;

        test_case(null_options && null_machine && null_result &&
                      gbl_test_canonical_equal(&before, &machine) &&
                      gbl_test_same_instance_identity_equal(&before, &machine) &&
                      output.termination == output_before.termination &&
                      output.reason == output_before.reason &&
                      output.cycles == output_before.cycles &&
                      output.instructions == output_before.instructions,
                  "invalid run arguments preserve machine and output storage");
    }

    {
        const gbl_machine before = machine;
        const uint64_t digest_before = gbl_test_canonical_digest(&machine);
        gbl_load_result refused;

        allocator_state.fail_next_allocation = true;
        refused = gbl_load_rom(&machine, replacement, sizeof(replacement));
        test_case(refused.status == GBL_LOAD_STATUS_RESOURCE_FAILURE &&
                      refused.reason == GBL_LOAD_REASON_ALLOCATION &&
                      gbl_test_canonical_equal(&before, &machine) &&
                      gbl_test_same_instance_identity_equal(&before, &machine) &&
                      digest_before == gbl_test_canonical_digest(&machine) &&
                      gbl_test_bytes_equal(replacement, replacement_before,
                                           sizeof(replacement)),
                  "failed allocation preserves CPU state and caller bytes");
    }

    {
        const gbl_machine before = machine;
        const uint64_t digest_before = gbl_test_canonical_digest(&machine);
        const gbl_load_result refused = gbl_load_rom(&machine, image, 0u);

        test_case(refused.status == GBL_LOAD_STATUS_REFUSED &&
                      refused.reason == GBL_LOAD_REASON_ROM_ZERO &&
                      gbl_test_canonical_equal(&before, &machine) &&
                      gbl_test_same_instance_identity_equal(&before, &machine) &&
                      digest_before == gbl_test_canonical_digest(&machine) &&
                      gbl_test_bytes_equal(image, image_before,
                                           sizeof(image)),
                  "refused ROM load preserves every persistent CPU field");
    }

    {
        gbl_machine no_rom = { 0 };
        cpu_test_allocator_state no_rom_allocator_state = { false };
        const gbl_run_options options = { 2u, 0u };
        const gbl_run_result output_before = {
            GBL_RUN_PROGRESS, GBL_RUN_REASON_UNIT_LIMIT, 77u, 88u
        };
        const bool initialized = machine_initialize(&no_rom,
                                                    &no_rom_allocator_state);

        if (initialized) {
            const gbl_machine before = no_rom;
            gbl_run_result output = output_before;
            const gbl_run_status status =
                gbl_cpu_run(&no_rom, &options, &output);

            test_case(status == GBL_RUN_STATUS_INVALID_ARGUMENT &&
                          gbl_test_canonical_equal(&before, &no_rom) &&
                          gbl_test_same_instance_identity_equal(&before,
                                                               &no_rom) &&
                          output.termination == output_before.termination &&
                          output.reason == output_before.reason &&
                          output.cycles == output_before.cycles &&
                          output.instructions == output_before.instructions,
                      "run without a loaded ROM is refused without effects");
            gbl_machine_dispose(&no_rom);
        } else {
            test_case(false,
                      "run without a loaded ROM is refused without effects");
        }
    }

    {
        uint8_t second_image[TEST_IMAGE_SIZE];
        gbl_machine first = { 0 };
        gbl_machine second = { 0 };
        gbl_run_result first_result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool first_loaded;
        bool second_loaded;
        bool first_ran;

        make_program(second_image);
        first_loaded = machine_load(&first, image);
        second_loaded = machine_load(&second, second_image);
        first_ran = first_loaded &&
                    run_units(&first, 3u, 2u, &first_result);
        test_case(first_loaded && second_loaded && first_ran &&
                      first_result.termination == GBL_RUN_PROGRESS &&
                      first_result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      first_result.cycles == 3u &&
                      first.cpu.r0_7[0] == UINT32_C(0x08000008) &&
                      first.master_cycles == 3u &&
                      gbl_test_cpu_is_direct_start(&second.cpu) &&
                      second.master_cycles == 0u,
                  "CPU state remains isolated between machine instances");
        gbl_machine_dispose(&first);
        gbl_machine_dispose(&second);
    }
    gbl_machine_dispose(&machine);
}

static void test_tracer(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran;

    make_program(image);
    loaded = machine_load(&machine, image);
    if (!loaded) {
        test_case(false, "empty refill fetches two instructions through bus");
        test_case(false, "MOV reads architectural PC and updates latch");
        test_case(false, "B refills the target and discards the stale slot");
        test_case(false, "target marker executes after branch refill");
        return;
    }

    ran = run_units(&machine, 2u, 1u, &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                  result.cycles == 2u && result.instructions == 0u &&
                  machine.master_cycles == 2u &&
                  machine.cpu.pipeline[0].address == UINT32_C(0x08000000) &&
                  machine.cpu.pipeline[0].opcode == TEST_MOV_R0_PC &&
                  machine.cpu.pipeline[1].address == UINT32_C(0x08000004) &&
                  machine.cpu.pipeline[1].opcode ==
                      TEST_BRANCH_TO_08000010 &&
                  machine.open_bus == TEST_BRANCH_TO_08000010 &&
                  machine.cpu.next_fetch_sequential,
              "empty refill fetches two words, costs two cycles and latches bus data");

    ran = run_units(&machine, 1u, 1u, &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  result.cycles == 1u && result.instructions == 1u &&
                  machine.cpu.r0_7[0] == UINT32_C(0x08000008) &&
                  machine.cpu.execution_address == UINT32_C(0x08000004) &&
                  machine.open_bus == TEST_MOV_R1_R0,
              "MOV r0,pc observes address plus eight through the sequential bus fetch");

    ran = run_units(&machine, 2u, 1u, &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  result.cycles == 2u && result.instructions == 1u &&
                  machine.cpu.execution_address == UINT32_C(0x08000010) &&
                  machine.cpu.pipeline[0].address == UINT32_C(0x08000010) &&
                  machine.cpu.pipeline[0].opcode == TEST_MOV_R2_R0 &&
                  machine.cpu.r0_7[1] == 0u,
              "B refills its target and discards the stale sequential slot");

    ran = run_units(&machine, 1u, 1u, &result);
    test_case(ran && result.instructions == 1u &&
                  machine.cpu.r0_7[1] == 0u &&
                  machine.cpu.r0_7[2] == UINT32_C(0x08000008),
              "target marker executes while the stale marker remains untouched");
    gbl_machine_dispose(&machine);
}

static void test_exceptions(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result;
    bool loaded;
    bool exact;

    make_empty_image(image);
    write_word(image, 0x00u, UINT32_C(0xEF000000)); /* SWI #0 */
    write_word(image, 0x04u, UINT32_C(0xE3A0002A));
    write_word(image, 0x08u, UINT32_C(0xE3A00063)); /* absent BIOS vector */
    loaded = machine_load(&machine, image);
    test_case(loaded, "SWI test image loads through the canonical ROM bus");
    if (!loaded) {
        return;
    }

    {
        gbl_machine short_machine = { 0 };
        gbl_run_result short_result;
        const bool short_loaded = machine_load(&short_machine, image);
        if (short_loaded) {
            short_machine.cpu.cpsr = TEST_CPSR_SYSTEM;
        }
        const bool short_run = short_loaded &&
            run_units(&short_machine, UINT64_C(2), 0u, &short_result);

        test_case(short_run &&
                      short_result.termination == GBL_RUN_TIMEOUT &&
                      short_result.reason == GBL_RUN_REASON_CYCLE_BUDGET &&
                      short_result.cycles == UINT64_C(2) &&
                      short_machine.master_cycles == UINT64_C(2) &&
                      short_machine.cpu.cpsr == TEST_CPSR_SYSTEM &&
                      short_machine.cpu.svc_r13_14[1] == 0u &&
                      short_machine.cpu.spsr[2] == 0u &&
                      !short_machine.cpu.stopped &&
                      short_machine.cpu.last_exception.kind ==
                          GBL_EXCEPTION_NONE,
                  "one-short cycle budget leaves the SWI entry uncommitted");
        gbl_machine_dispose(&short_machine);
    }

    machine.cpu.cpsr = UINT32_C(0xA000001F); /* N/C, System, F clear */
    exact = run_units(&machine, UINT64_C(3), 0u, &result);
    test_case(exact && result.termination == GBL_RUN_STOPPED &&
                  result.reason == GBL_RUN_REASON_GUEST_STOP &&
                  result.cycles == UINT64_C(3) &&
                  result.instructions == UINT64_C(1) &&
                  machine.cpu.stop_reason == GBL_CPU_STOP_SWI_NO_BIOS,
              "ARM SWI consumes its internal entry cycle and stops as guest");
    test_case(machine.cpu.cpsr == UINT32_C(0xA0000093) &&
                  machine.cpu.svc_r13_14[1] == UINT32_C(0x08000004) &&
                  machine.cpu.spsr[2] == UINT32_C(0xA000001F),
              "ARM SWI saves old CPSR and current-plus-four in the SVC bank");
    test_case(machine.cpu.execution_address == UINT32_C(0x00000008) &&
                  !machine.cpu.pipeline[0].valid &&
                  !machine.cpu.pipeline[1].valid &&
                  machine.open_bus == UINT32_C(0xE3A0002A) &&
                  machine.cpu.last_exception.kind == GBL_EXCEPTION_SWI &&
                  machine.cpu.last_exception.current_address ==
                      UINT32_C(0x08000000) &&
                  machine.cpu.last_exception.next_address ==
                      UINT32_C(0x08000004) &&
                  machine.cpu.last_exception.fault_address ==
                      UINT32_C(0x08000000) &&
                  !machine.cpu.last_exception.thumb_state,
              "ARM SWI selects vector PC without fetching absent handler bytes");
    gbl_machine_dispose(&machine);
}

static void test_masked_requests(void)
{
    gbl_machine machine = { 0 };
    const gbl_cpu_exception_context context = {
        UINT32_C(0x08000000), UINT32_C(0x08000004),
        UINT32_C(0x08000000), false
    };
    gbl_cpu_status status;

    if (!machine_initialize(&machine, NULL)) {
        test_case(false, "masked IRQ request preserves state and time");
        test_case(false, "masked FIQ request preserves state and time");
        return;
    }
    machine.master_cycles = UINT64_C(17);
    machine.open_bus = UINT32_C(0xA5A55A5A);
    machine.cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000000), UINT32_C(0xE1A00000), 4u, true
    };
    machine.cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000004), UINT32_C(0xE1A01000), 4u, true
    };
    machine.cpu.next_fetch_sequential = true;
    {
        const gbl_machine before = machine;

        status = gbl_cpu_enter_exception(&machine, GBL_EXCEPTION_IRQ,
                                         &context);
        test_case(status == GBL_CPU_STATUS_MASKED &&
                      gbl_test_canonical_equal(&before, &machine),
                  "masked IRQ request preserves state and time");
        machine = before;
        status = gbl_cpu_enter_exception(&machine, GBL_EXCEPTION_FIQ,
                                         &context);
        test_case(status == GBL_CPU_STATUS_MASKED &&
                      gbl_test_canonical_equal(&before, &machine),
                  "masked FIQ request preserves state and time");
    }
    gbl_machine_dispose(&machine);
}

static void test_modes(void)
{
    typedef struct mode_case {
        uint32_t mode;
        uint32_t expected_r8;
        uint32_t expected_r13;
        uint32_t expected_r14;
        uint32_t expected_spsr;
        gbl_cpu_status spsr_status;
    } mode_case;
    static const mode_case cases[] = {
        { TEST_CPSR_USER, 0x11000000u, 0x11000005u, 0x11000006u,
          0xA8000000u, GBL_CPU_STATUS_SPSR_ABSENT },
        { TEST_CPSR_FIQ, 0x22000000u, 0x22000005u, 0x22000006u,
          0xA8000000u, GBL_CPU_STATUS_OK },
        { TEST_CPSR_IRQ, 0x11000000u, 0x33000000u, 0x33000001u,
          0xA8000001u, GBL_CPU_STATUS_OK },
        { TEST_CPSR_SVC, 0x11000000u, 0x44000000u, 0x44000001u,
          0xA8000002u, GBL_CPU_STATUS_OK },
        { TEST_CPSR_ABT, 0x11000000u, 0x55000000u, 0x55000001u,
          0xA8000003u, GBL_CPU_STATUS_OK },
        { TEST_CPSR_UND, 0x11000000u, 0x66000000u, 0x66000001u,
          0xA8000004u, GBL_CPU_STATUS_OK },
        { TEST_CPSR_SYSTEM, 0x11000000u, 0x11000005u, 0x11000006u,
          0xA8000000u, GBL_CPU_STATUS_SPSR_ABSENT }
    };
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result;
    bool loaded;
    bool registers_match = true;
    bool spsr_mapping_match = true;
    size_t index;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    for (index = 0u; index < 7u; ++index) {
        machine.cpu.usr_r8_14[index] = UINT32_C(0x11000000) +
                                       (uint32_t)index;
        machine.cpu.fiq_r8_14[index] = UINT32_C(0x22000000) +
                                       (uint32_t)index;
    }
    for (index = 0u; index < 2u; ++index) {
        machine.cpu.irq_r13_14[index] = UINT32_C(0x33000000) +
                                       (uint32_t)index;
        machine.cpu.svc_r13_14[index] = UINT32_C(0x44000000) +
                                       (uint32_t)index;
        machine.cpu.abt_r13_14[index] = UINT32_C(0x55000000) +
                                       (uint32_t)index;
        machine.cpu.und_r13_14[index] = UINT32_C(0x66000000) +
                                       (uint32_t)index;
    }
    for (index = 0u; index < 5u; ++index) {
        machine.cpu.spsr[index] = UINT32_C(0xA8000000) +
                                 (uint32_t)index;
    }
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint32_t spsr = UINT32_C(0xDEADBEEF);
        const bool mode_set =
            gbl_cpu_set_cpsr(&machine, cases[index].mode) ==
            GBL_CPU_STATUS_OK;
        const bool read_r8 = loaded && mode_set &&
            run_arm_instruction_in_mode(&machine, UINT32_C(0xE1A00008),
                                        cases[index].mode, &result) &&
            machine.cpu.r0_7[0] == cases[index].expected_r8;
        const bool read_r13 = loaded && mode_set &&
            run_arm_instruction_in_mode(&machine, UINT32_C(0xE1A0000D),
                                        cases[index].mode, &result) &&
            machine.cpu.r0_7[0] == cases[index].expected_r13;
        const bool read_r14 = loaded && mode_set &&
            run_arm_instruction_in_mode(&machine, UINT32_C(0xE1A0000E),
                                        cases[index].mode, &result) &&
            machine.cpu.r0_7[0] == cases[index].expected_r14;
        const gbl_cpu_status spsr_status =
            gbl_cpu_get_spsr(&machine, &spsr);

        registers_match = registers_match && read_r8 && read_r13 && read_r14;
        spsr_mapping_match = spsr_mapping_match &&
            spsr_status == cases[index].spsr_status &&
            (spsr_status == GBL_CPU_STATUS_OK
                 ? spsr == cases[index].expected_spsr
                 : spsr == UINT32_C(0xDEADBEEF));
    }
    test_case(registers_match,
              "all seven modes select shared and private physical registers");
    test_case(spsr_mapping_match,
              "exception SPSRs stay private while User and System lack one");

    {
        const gbl_machine before = machine;
        const gbl_cpu_status invalid = gbl_cpu_set_cpsr(
            &machine, UINT32_C(0x00000016));
        const gbl_cpu_status null_machine = gbl_cpu_set_cpsr(
            NULL, TEST_CPSR_SYSTEM);

        test_case(invalid == GBL_CPU_STATUS_INVALID_MODE &&
                      null_machine == GBL_CPU_STATUS_INVALID_ARGUMENT &&
                      gbl_test_canonical_equal(&before, &machine),
                  "invalid CPSR modes and null transitions preserve state");
    }

    {
        const gbl_cpu_status user_spsr = gbl_cpu_set_cpsr(
            &machine, TEST_CPSR_USER);
        uint32_t output = UINT32_C(0x12345678);
        const gbl_cpu_status absent = gbl_cpu_get_spsr(&machine, &output);

        test_case(user_spsr == GBL_CPU_STATUS_OK &&
                      absent == GBL_CPU_STATUS_SPSR_ABSENT &&
                      output == UINT32_C(0x12345678),
                  "User SPSR access is absent and leaves the output untouched");
    }

    {
        const gbl_cpu_status enter_thumb = gbl_cpu_set_cpsr(
            &machine, TEST_CPSR_SYSTEM | UINT32_C(0x00000020));
        const bool thumb_refill = enter_thumb == GBL_CPU_STATUS_OK &&
            !machine.cpu.pipeline[0].valid && !machine.cpu.pipeline[1].valid &&
            !machine.cpu.next_fetch_sequential;
        const gbl_cpu_status return_arm = gbl_cpu_set_cpsr(
            &machine, TEST_CPSR_SYSTEM);

        test_case(thumb_refill && return_arm == GBL_CPU_STATUS_OK &&
                      !machine.cpu.pipeline[0].valid &&
                      !machine.cpu.pipeline[1].valid &&
                      !machine.cpu.next_fetch_sequential,
                  "validated ARM/Thumb CPSR changes invalidate stale pipeline slots");
    }

    gbl_machine_dispose(&machine);
}

static void test_exception_matrix(void)
{
    typedef struct exception_case {
        gbl_exception_kind kind;
        gbl_cpu_exception_context context;
        uint32_t old_cpsr;
        uint32_t expected_cpsr;
        uint32_t expected_mode;
        uint32_t expected_vector;
        uint32_t expected_lr;
        size_t expected_spsr_index;
        uint32_t expected_spsr;
        gbl_cpu_stop_reason expected_stop;
    } exception_case;
    static const exception_case cases[] = {
        { GBL_EXCEPTION_UNDEFINED,
          { 0x08000100u, 0x08000104u, 0x08000200u, false },
          0xA000005Fu, 0xA00000DBu, TEST_CPSR_UND, 4u, 0x08000104u, 4u,
          0xA000005Fu, GBL_CPU_STOP_UNDEFINED_NO_BIOS },
        { GBL_EXCEPTION_UNDEFINED,
          { 0x08000102u, 0x08000104u, 0x08000202u, true },
          0xA000007Fu, 0xA00000DBu, TEST_CPSR_UND, 4u, 0x08000104u, 4u,
          0xA000007Fu, GBL_CPU_STOP_UNDEFINED_NO_BIOS },
        { GBL_EXCEPTION_RESET,
          { 0x08000100u, 0x08000104u, 0x08000200u, false },
          0xA000005Fu, 0xA00000D3u, TEST_CPSR_SVC, 0u, 0u, 2u, 0u,
          GBL_CPU_STOP_RESET_NO_BIOS },
        { GBL_EXCEPTION_SWI,
          { 0x08000102u, 0x08000104u, 0x08000202u, true },
          0xA000007Fu, 0xA00000D3u, TEST_CPSR_SVC, 8u, 0x08000104u, 2u,
          0xA000007Fu, GBL_CPU_STOP_SWI_NO_BIOS },
        { GBL_EXCEPTION_PREFETCH_ABORT,
          { 0x08000100u, 0x08000104u, 0x08000200u, false },
          0xA000005Fu, 0xA00000D7u, TEST_CPSR_ABT, 12u, 0x08000204u, 3u,
          0xA000005Fu, GBL_CPU_STOP_PREFETCH_ABORT_NO_BIOS },
        { GBL_EXCEPTION_PREFETCH_ABORT,
          { 0x08000102u, 0x08000104u, 0x08000202u, true },
          0xA000007Fu, 0xA00000D7u, TEST_CPSR_ABT, 12u, 0x08000206u, 3u,
          0xA000007Fu, GBL_CPU_STOP_PREFETCH_ABORT_NO_BIOS },
        { GBL_EXCEPTION_DATA_ABORT,
          { 0x08000100u, 0x08000104u, 0x08000200u, false },
          0xA000005Fu, 0xA00000D7u, TEST_CPSR_ABT, 16u, 0x08000208u, 3u,
          0xA000005Fu, GBL_CPU_STOP_DATA_ABORT_NO_BIOS },
        { GBL_EXCEPTION_DATA_ABORT,
          { 0x08000102u, 0x08000104u, 0x08000202u, true },
          0xA000007Fu, 0xA00000D7u, TEST_CPSR_ABT, 16u, 0x0800020Au, 3u,
          0xA000007Fu, GBL_CPU_STOP_DATA_ABORT_NO_BIOS },
        { GBL_EXCEPTION_IRQ,
          { 0x08000100u, 0x08000104u, 0x08000200u, false },
          0xA000005Fu, 0xA00000D2u, TEST_CPSR_IRQ, 24u, 0x08000108u, 1u,
          0xA000005Fu, GBL_CPU_STOP_IRQ_NO_BIOS },
        { GBL_EXCEPTION_IRQ,
          { 0x08000102u, 0x08000104u, 0x08000202u, true },
          0xA000007Fu, 0xA00000D2u, TEST_CPSR_IRQ, 24u, 0x08000108u, 1u,
          0xA000007Fu, GBL_CPU_STOP_IRQ_NO_BIOS },
        { GBL_EXCEPTION_FIQ,
          { 0x08000100u, 0x08000104u, 0x08000200u, false },
          0xA000001Fu, 0xA00000D1u, TEST_CPSR_FIQ, 28u, 0x08000108u, 0u,
          0xA000001Fu, GBL_CPU_STOP_FIQ_NO_BIOS },
        { GBL_EXCEPTION_FIQ,
          { 0x08000102u, 0x08000104u, 0x08000202u, true },
          0xA000003Fu, 0xA00000D1u, TEST_CPSR_FIQ, 28u, 0x08000108u, 0u,
          0xA000003Fu, GBL_CPU_STOP_FIQ_NO_BIOS },
        { GBL_EXCEPTION_COPROCESSOR,
          { 0x08000100u, 0x08000104u, 0x08000200u, false },
          0xA000005Fu, 0xA00000DBu, TEST_CPSR_UND, 4u, 0x08000104u, 4u,
          0xA000005Fu, GBL_CPU_STOP_COPROCESSOR_NO_BIOS }
    };
    bool every_entry_correct = true;
    size_t case_index;

    for (case_index = 0u;
         case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
        gbl_machine machine = { 0 };
        const exception_case *item = &cases[case_index];
        bool entry_correct = machine_initialize(&machine, NULL);

        if (entry_correct) {
            machine.cpu.cpsr = item->old_cpsr;
            machine.master_cycles = UINT64_C(17);
            machine.open_bus = UINT32_C(0xA5A55A5A);
            machine.cpu.retired_instructions = UINT64_C(23);
            machine.cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
                item->context.current_address,
                UINT32_C(0xE1A00000),
                item->context.thumb_state ? 2u : 4u,
                true
            };
            machine.cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
                item->context.next_address,
                UINT32_C(0xE1A01000),
                item->context.thumb_state ? 2u : 4u,
                true
            };
            machine.cpu.next_fetch_sequential = true;
            machine.cpu.und_r13_14[1] = UINT32_C(0xA1A1A1A1);
            machine.cpu.svc_r13_14[1] = UINT32_C(0xB2B2B2B2);
            machine.cpu.abt_r13_14[1] = UINT32_C(0xC3C3C3C3);
            machine.cpu.irq_r13_14[1] = UINT32_C(0xD4D4D4D4);
            machine.cpu.fiq_r8_14[6] = UINT32_C(0xE5E5E5E5);
            entry_correct =
                gbl_cpu_enter_exception(&machine, item->kind,
                                        &item->context) ==
                    GBL_CPU_STATUS_OK &&
                machine.cpu.cpsr == item->expected_cpsr &&
                machine.cpu.execution_address == item->expected_vector &&
                machine.cpu.stop_reason == item->expected_stop &&
                machine.cpu.stopped &&
                machine.cpu.retired_instructions == UINT64_C(23) &&
                machine.master_cycles == UINT64_C(18) &&
                machine.open_bus == UINT32_C(0xA5A55A5A) &&
                !machine.cpu.pipeline[0].valid &&
                !machine.cpu.pipeline[1].valid &&
                !machine.cpu.next_fetch_sequential &&
                machine.cpu.spsr[item->expected_spsr_index] ==
                    item->expected_spsr &&
                machine.cpu.last_exception.kind == item->kind &&
                machine.cpu.last_exception.current_address ==
                    item->context.current_address &&
                machine.cpu.last_exception.next_address ==
                    item->context.next_address &&
                machine.cpu.last_exception.fault_address ==
                    item->context.fault_address &&
                machine.cpu.last_exception.thumb_state ==
                    item->context.thumb_state;
            switch (item->expected_mode) {
            case TEST_CPSR_FIQ:
                entry_correct = entry_correct &&
                    machine.cpu.fiq_r8_14[6] == item->expected_lr;
                break;
            case TEST_CPSR_IRQ:
                entry_correct = entry_correct &&
                    machine.cpu.irq_r13_14[1] == item->expected_lr;
                break;
            case TEST_CPSR_SVC:
                entry_correct = entry_correct &&
                    machine.cpu.svc_r13_14[1] == item->expected_lr;
                break;
            case TEST_CPSR_ABT:
                entry_correct = entry_correct &&
                    machine.cpu.abt_r13_14[1] == item->expected_lr;
                break;
            case TEST_CPSR_UND:
                entry_correct = entry_correct &&
                    machine.cpu.und_r13_14[1] == item->expected_lr;
                break;
            default:
                entry_correct = false;
                break;
            }
        }
        every_entry_correct = every_entry_correct && entry_correct;
        gbl_machine_dispose(&machine);
    }
    test_case(every_entry_correct,
              "ARM and Thumb exceptions use exact address bases, masks, SPSRs, banks and vectors");
}

static void test_exception_refusals(void)
{
    gbl_machine machine = { 0 };
    const gbl_cpu_exception_context valid = {
        UINT32_C(0x08000000), UINT32_C(0x08000004),
        UINT32_C(0x08000000), false
    };
    gbl_cpu_exception_context invalid = valid;
    bool initialized;

    initialized = machine_initialize(&machine, NULL);
    if (!initialized) {
        test_case(false, "invalid exception contexts preserve state");
        test_case(false, "invalid kinds, nulls and cycle overflow refuse cleanly");
        return;
    }
    machine.cpu.cpsr = TEST_CPSR_SYSTEM;
    machine.master_cycles = UINT64_C(19);
    machine.open_bus = UINT32_C(0xA5A55A5A);
    machine.cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        valid.current_address, UINT32_C(0xE1A00000), 4u, true
    };
    machine.cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        valid.next_address, UINT32_C(0xE1A01000), 4u, true
    };
    machine.cpu.next_fetch_sequential = true;
    {
        const gbl_machine before = machine;
        gbl_cpu_status status;

        invalid.current_address += 2u;
        status = gbl_cpu_enter_exception(&machine, GBL_EXCEPTION_SWI,
                                         &invalid);
        test_case(status == GBL_CPU_STATUS_INVALID_CONTEXT &&
                      gbl_test_canonical_equal(&before, &machine),
                  "invalid exception contexts preserve state and time");

        status = gbl_cpu_enter_exception(
            &machine, (gbl_exception_kind)99, &valid);
        const bool invalid_kind = status ==
            GBL_CPU_STATUS_INVALID_EXCEPTION &&
            gbl_test_canonical_equal(&before, &machine);
        status = gbl_cpu_enter_exception(&machine, GBL_EXCEPTION_SWI, NULL);
        const bool null_context = status == GBL_CPU_STATUS_INVALID_ARGUMENT &&
            gbl_test_canonical_equal(&before, &machine);
        test_case(invalid_kind && null_context,
                  "invalid exception kinds and null contexts refuse without effects");
    }
    machine.master_cycles = UINT64_MAX;
    {
        const gbl_machine before_overflow = machine;
        const gbl_cpu_status status = gbl_cpu_enter_exception(
            &machine, GBL_EXCEPTION_SWI, &valid);

        test_case(status == GBL_CPU_STATUS_COUNTER_OVERFLOW &&
                      gbl_test_canonical_equal(&before_overflow, &machine),
                  "exception cycle overflow is refused before CPU mutation");
    }
    gbl_machine_dispose(&machine);
}

static void test_exception_restore_and_overwrite(void)
{
    gbl_machine restored = { 0 };
    gbl_machine overwritten = { 0 };
    const gbl_cpu_exception_context first = {
        UINT32_C(0x08000100), UINT32_C(0x08000104),
        UINT32_C(0x08000100), false
    };
    const gbl_cpu_exception_context second = {
        UINT32_C(0x00000008), UINT32_C(0x0000000C),
        UINT32_C(0x00000008), false
    };
    const uint32_t original_cpsr = UINT32_C(0xA000001F);
    uint32_t saved_cpsr = 0u;
    bool restore_ok;
    bool overwrite_ok;

    restore_ok = machine_initialize(&restored, NULL);
    if (restore_ok) {
        restored.cpu.cpsr = original_cpsr;
        restore_ok =
            gbl_cpu_enter_exception(&restored, GBL_EXCEPTION_SWI, &first) ==
                GBL_CPU_STATUS_OK &&
            gbl_cpu_get_spsr(&restored, &saved_cpsr) == GBL_CPU_STATUS_OK &&
            saved_cpsr == original_cpsr &&
            gbl_cpu_set_cpsr(&restored, saved_cpsr) == GBL_CPU_STATUS_OK &&
            (restored.cpu.cpsr & TEST_CPSR_MODE_MASK) == TEST_CPSR_SYSTEM &&
            (restored.cpu.cpsr & UINT32_C(0x00000020)) == 0u &&
            !restored.cpu.pipeline[0].valid &&
            !restored.cpu.pipeline[1].valid;
    }
    test_case(restore_ok,
              "saved CPSR can be restored through the validated mode transition");

    overwrite_ok = machine_initialize(&overwritten, NULL);
    if (overwrite_ok) {
        overwritten.cpu.cpsr = original_cpsr;
        overwrite_ok =
            gbl_cpu_enter_exception(&overwritten, GBL_EXCEPTION_SWI, &first) ==
                GBL_CPU_STATUS_OK &&
            overwritten.cpu.spsr[2] == original_cpsr &&
            overwritten.cpu.svc_r13_14[1] == UINT32_C(0x08000104) &&
            gbl_cpu_enter_exception(&overwritten, GBL_EXCEPTION_SWI,
                                    &second) == GBL_CPU_STATUS_OK &&
            overwritten.cpu.spsr[2] == UINT32_C(0xA0000093) &&
            overwritten.cpu.svc_r13_14[1] == UINT32_C(0x0000000C) &&
            overwritten.cpu.last_exception.current_address == 8u &&
            overwritten.master_cycles == UINT64_C(2);
    }
    test_case(overwrite_ok,
              "a repeated same-mode exception overwrites its own SPSR and LR bank");
    gbl_machine_dispose(&restored);
    gbl_machine_dispose(&overwritten);
}

static bool run_guest_exception_case(bool thumb, uint32_t opcode,
                                     gbl_exception_kind kind,
                                     gbl_cpu_stop_reason stop_reason,
                                     uint32_t vector, uint32_t link_register)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result;
    const uint32_t marker = UINT32_C(0xE3A0005A);
    bool loaded;
    bool ran;

    make_empty_image(image);
    if (thumb) {
        write_halfword(image, 0u, (uint16_t)opcode);
        write_halfword(image, 2u, UINT16_C(0x46C0));
    } else {
        write_word(image, 0u, opcode);
        write_word(image, 4u, marker);
    }
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.cpsr = TEST_CPSR_SYSTEM |
            (thumb ? UINT32_C(0x00000020) : 0u);
    }
    /* Leave room for a broken stop latch to fetch from the absent vector. */
    ran = loaded && run_units(&machine, UINT64_C(5), 0u, &result);
    if (!ran) {
        gbl_machine_dispose(&machine);
        return false;
    }
    {
        const uint32_t expected_current = UINT32_C(0x08000000);
        const uint32_t expected_next = expected_current +
            (thumb ? UINT32_C(2) : UINT32_C(4));
        const bool observed = result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_GUEST_STOP &&
            result.cycles == UINT64_C(3) &&
            result.instructions == UINT64_C(1) &&
            machine.master_cycles == UINT64_C(3) &&
            machine.cpu.execution_address == vector &&
            machine.cpu.stop_reason == stop_reason &&
            machine.cpu.last_exception.kind == kind &&
            machine.cpu.last_exception.current_address == expected_current &&
            machine.cpu.last_exception.next_address == expected_next &&
            machine.cpu.last_exception.fault_address == expected_current &&
            machine.cpu.last_exception.thumb_state == thumb &&
            !machine.cpu.pipeline[0].valid &&
            !machine.cpu.pipeline[1].valid &&
            machine.open_bus ==
                (thumb ? UINT32_C(0x46C046C0) : marker);
        const uint32_t banked_lr =
            kind == GBL_EXCEPTION_SWI ? machine.cpu.svc_r13_14[1] :
            kind == GBL_EXCEPTION_COPROCESSOR ||
                    kind == GBL_EXCEPTION_UNDEFINED
                ? machine.cpu.und_r13_14[1]
                : 0u;
        const bool link_matches =
            (kind == GBL_EXCEPTION_SWI ||
             kind == GBL_EXCEPTION_COPROCESSOR ||
             kind == GBL_EXCEPTION_UNDEFINED) &&
            banked_lr == link_register;

        gbl_machine_dispose(&machine);
        return observed && link_matches;
    }
}

static void test_no_bios(void)
{
    const bool swi_and_undefined =
        run_guest_exception_case(false, UINT32_C(0xEF000042),
                                 GBL_EXCEPTION_SWI,
                                 GBL_CPU_STOP_SWI_NO_BIOS, 8u,
                                 UINT32_C(0x08000004)) &&
        run_guest_exception_case(true, UINT32_C(0x0000DF42),
                                 GBL_EXCEPTION_SWI,
                                 GBL_CPU_STOP_SWI_NO_BIOS, 8u,
                                 UINT32_C(0x08000002)) &&
        run_guest_exception_case(false, UINT32_C(0xF1A00000),
                                 GBL_EXCEPTION_UNDEFINED,
                                 GBL_CPU_STOP_UNDEFINED_NO_BIOS, 4u,
                                 UINT32_C(0x08000004)) &&
        run_guest_exception_case(true, UINT32_C(0x0000DE00),
                                 GBL_EXCEPTION_UNDEFINED,
                                 GBL_CPU_STOP_UNDEFINED_NO_BIOS, 4u,
                                 UINT32_C(0x08000002));
    const bool coprocessor_groups =
        run_guest_exception_case(false, UINT32_C(0xEC000000),
                                 GBL_EXCEPTION_COPROCESSOR,
                                 GBL_CPU_STOP_COPROCESSOR_NO_BIOS, 4u,
                                 UINT32_C(0x08000004)) &&
        run_guest_exception_case(false, UINT32_C(0xEE000000),
                                 GBL_EXCEPTION_COPROCESSOR,
                                 GBL_CPU_STOP_COPROCESSOR_NO_BIOS, 4u,
                                 UINT32_C(0x08000004)) &&
        run_guest_exception_case(false, UINT32_C(0xEE000010),
                                 GBL_EXCEPTION_COPROCESSOR,
                                 GBL_CPU_STOP_COPROCESSOR_NO_BIOS, 4u,
                                 UINT32_C(0x08000004));

    test_case(swi_and_undefined,
              "ARM and Thumb SWI/undefined guests stop with named BIOS-none causes");
    test_case(coprocessor_groups,
              "coprocessor load/store/data/register groups enter UND then stop");
}

static void test_budget(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;

    make_program(image);
    loaded = machine_load(&machine, image);
    if (!loaded) {
        test_case(false, "one-cycle refill refusal leaves the initial state untouched");
        test_case(false, "zero cycle budget performs no work");
        test_case(false, "one-short MOV boundary leaves state untouched");
        test_case(false, "one-short branch refill leaves state untouched");
        test_case(false, "exact-fit branch commits once");
        test_case(false, "clock overflow is rejected before a refill");
        test_case(false, "retired-instruction overflow is rejected before MOV");
        test_case(false, "fetch extent overflow is refused without bus effects");
        test_case(false, "clock at UINT64_MAX refuses the next fetch atomically");
        return;
    }

    {
        const gbl_machine before = machine;
        const uint64_t digest = gbl_test_canonical_digest(&machine);
        const bool ran = run_units(&machine, 1u, 0u, &result);

        test_case(ran && result.termination == GBL_RUN_TIMEOUT &&
                      result.reason == GBL_RUN_REASON_CYCLE_BUDGET &&
                      result.cycles == 0u && result.instructions == 0u &&
                      gbl_test_canonical_equal(&before, &machine) &&
                      digest == gbl_test_canonical_digest(&machine),
                  "one-cycle refill refusal leaves the initial state untouched");
    }

    {
        const gbl_machine before = machine;
        const bool ran = run_units(&machine, 0u, 0u, &result);

        test_case(ran && result.termination == GBL_RUN_TIMEOUT &&
                      result.reason == GBL_RUN_REASON_CYCLE_BUDGET &&
                      result.cycles == 0u && result.instructions == 0u &&
                      gbl_test_canonical_equal(&before, &machine),
                  "zero cycle budget performs no work");
    }

    (void)run_units(&machine, 2u, 1u, &result);
    {
        const gbl_machine before = machine;
        const bool ran = run_units(&machine, 0u, 1u, &result);

        test_case(ran && result.termination == GBL_RUN_TIMEOUT &&
                      result.reason == GBL_RUN_REASON_CYCLE_BUDGET &&
                      result.cycles == 0u && result.instructions == 0u &&
                      gbl_test_canonical_equal(&before, &machine),
                  "one-short MOV boundary leaves state untouched");
    }

    (void)run_units(&machine, 1u, 1u, &result);
    {
        const gbl_machine before = machine;
        const bool ran = run_units(&machine, 1u, 1u, &result);

        test_case(ran && result.termination == GBL_RUN_TIMEOUT &&
                      result.reason == GBL_RUN_REASON_CYCLE_BUDGET &&
                      result.cycles == 0u && result.instructions == 0u &&
                      gbl_test_canonical_equal(&before, &machine),
                  "one-short branch refill leaves state untouched");
    }
    {
        const bool ran = run_units(&machine, 2u, 1u, &result);

        test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                      result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      result.cycles == 2u && result.instructions == 1u &&
                      machine.cpu.retired_instructions == 2u &&
                      machine.cpu.execution_address == UINT32_C(0x08000010),
                  "exact-fit branch commits once with its two-cycle refill");
    }
    gbl_machine_dispose(&machine);

    {
        uint8_t overflow_image[TEST_IMAGE_SIZE];
        gbl_machine overflow_machine = { 0 };
        bool overflow_loaded;

        make_program(overflow_image);
        overflow_loaded = machine_load(&overflow_machine, overflow_image);
        if (overflow_loaded) {
            overflow_machine.master_cycles = UINT64_MAX - UINT64_C(1);
            {
                const gbl_machine overflow_before = overflow_machine;
                const bool ran =
                    run_units(&overflow_machine, 2u, 0u, &result);

                test_case(ran && result.termination == GBL_RUN_STOPPED &&
                              result.reason ==
                                  GBL_RUN_REASON_COUNTER_OVERFLOW &&
                              result.cycles == 0u &&
                              gbl_test_canonical_equal(&overflow_before,
                                                       &overflow_machine),
                          "clock overflow is rejected before a refill");
            }
            overflow_machine.master_cycles = UINT64_MAX - UINT64_C(2);
            {
                const bool exact_ran =
                    run_units(&overflow_machine, 2u, 1u, &result);
                const gbl_machine at_limit = overflow_machine;
                const bool refused_at_limit =
                    run_units(&overflow_machine, 1u, 1u, &result);

                test_case(exact_ran &&
                              at_limit.master_cycles == UINT64_MAX &&
                              result.termination == GBL_RUN_STOPPED &&
                              result.reason ==
                                  GBL_RUN_REASON_COUNTER_OVERFLOW &&
                              result.cycles == 0u && refused_at_limit &&
                              gbl_test_canonical_equal(&at_limit,
                                                       &overflow_machine),
                          "clock at UINT64_MAX refuses the next fetch atomically");
            }
            overflow_machine.cpu.pipeline[0].valid = true;
            overflow_machine.cpu.pipeline[0].address =
                UINT32_C(0x08000000);
            overflow_machine.cpu.pipeline[0].opcode = TEST_MOV_R0_PC;
            overflow_machine.cpu.pipeline[0].width = 4u;
            overflow_machine.cpu.pipeline[1].valid = true;
            overflow_machine.cpu.pipeline[1].address =
                UINT32_C(0x08000004);
            overflow_machine.cpu.pipeline[1].opcode = TEST_BRANCH_TO_08000010;
            overflow_machine.cpu.pipeline[1].width = 4u;
            overflow_machine.cpu.next_fetch_sequential = true;
            overflow_machine.cpu.retired_instructions = UINT64_MAX;
            {
                const gbl_machine retired_before = overflow_machine;
                const bool ran =
                    run_units(&overflow_machine, 1u, 0u, &result);

                test_case(ran && result.termination == GBL_RUN_STOPPED &&
                              result.reason ==
                                  GBL_RUN_REASON_COUNTER_OVERFLOW &&
                              result.instructions == 0u &&
                              gbl_test_canonical_equal(&retired_before,
                                                       &overflow_machine),
                          "retired-instruction overflow is rejected before MOV");
            }
            overflow_machine.cpu = (gbl_cpu_state){ 0 };
            overflow_machine.cpu.cpsr = UINT32_C(0x000000DF);
            overflow_machine.cpu.execution_address = UINT32_MAX - 3u;
            overflow_machine.master_cycles = 0u;
            {
                const gbl_machine extent_before = overflow_machine;
                const bool ran = run_units(&overflow_machine, 2u, 0u, &result);

                test_case(ran && result.termination == GBL_RUN_STOPPED &&
                              result.reason ==
                                  GBL_RUN_REASON_INVALID_CPU_STATE &&
                              result.cycles == 0u &&
                              gbl_test_canonical_equal(&extent_before,
                                                       &overflow_machine),
                          "fetch extent overflow is refused without bus effects");
            }
            gbl_machine_dispose(&overflow_machine);
        } else {
            test_case(false, "clock overflow is rejected before a refill");
            test_case(false, "retired-instruction overflow is rejected before MOV");
            test_case(false, "fetch extent overflow is refused without bus effects");
        }
    }
}

static void test_arithmetic(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran;

    make_program(image);
    loaded = machine_load(&machine, image);

    {
        static const uint8_t amounts[] = { 0u, 1u, 31u, 32u, 33u, 255u };
        static const uint32_t expected_values[] = {
            UINT32_C(0x80000001), UINT32_C(2), UINT32_C(0x80000000),
            0u, 0u, 0u
        };
        static const bool expected_carries[] = {
            true, true, false, true, false, false
        };
        bool vectors_match = true;
        size_t index;

        for (index = 0u; index < sizeof(amounts) / sizeof(amounts[0]);
             ++index) {
            const gbl_shift_result shifted = gbl_shift(
                GBL_SHIFT_LSL, UINT32_C(0x80000001), amounts[index], true,
                true);

            if (shifted.value != expected_values[index] ||
                shifted.carry != expected_carries[index]) {
                vectors_match = false;
            }
        }
        test_case(loaded && vectors_match,
                  "shift helper defines counts zero, 1, 31, 32, 33 and 255");
    }

    {
        const gbl_shift_result rrx = gbl_shift(
            GBL_SHIFT_ROR, UINT32_C(3), 0u, true, false);
        const gbl_shift_result lsr = gbl_shift(
            GBL_SHIFT_LSR, UINT32_C(0x80000001), 0u, false, false);
        const gbl_shift_result asr = gbl_shift(
            GBL_SHIFT_ASR, UINT32_C(0x80000001), 0u, false, false);

        test_case(loaded && rrx.value == UINT32_C(0x80000001) && rrx.carry &&
                      lsr.value == 0u && lsr.carry &&
                      asr.value == UINT32_MAX && asr.carry,
                  "shift helper distinguishes RRX, LSR zero and ASR zero");
    }

    {
        const gbl_arithmetic_result addition = gbl_add_flags(
            UINT32_C(0x7FFFFFFF), UINT32_C(1));
        const gbl_arithmetic_result subtraction = gbl_sub_flags(
            UINT32_C(0x80000000), UINT32_C(1));

        test_case(loaded && addition.value == UINT32_C(0x80000000) &&
                      !addition.carry && addition.overflow &&
                      subtraction.value == UINT32_C(0x7FFFFFFF) &&
                      subtraction.carry && subtraction.overflow,
                  "add/sub helpers return wrapped value, carry and overflow");
    }

    machine.cpu.r0_7[0] = UINT32_C(0x7FFFFFFF);
    machine.cpu.r0_7[1] = UINT32_C(1);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE0902001),
                                            TEST_CPSR_V, &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  result.instructions == 1u &&
                  machine.cpu.r0_7[2] == UINT32_C(0x80000000) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_V |
                                       TEST_CPSR_C | TEST_CPSR_Z)) ==
                      (TEST_CPSR_N | TEST_CPSR_V),
              "ADDS wraps at signed maximum and sets N/V independently");

    machine.cpu.r0_7[0] = UINT32_C(0x80000000);
    machine.cpu.r0_7[1] = UINT32_C(1);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE0503001),
                                            0u, &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  result.instructions == 1u &&
                  machine.cpu.r0_7[3] == UINT32_C(0x7FFFFFFF) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_V |
                                       TEST_CPSR_C | TEST_CPSR_Z)) ==
                      (TEST_CPSR_V | TEST_CPSR_C),
              "SUBS wraps at signed minimum and sets carry/no-borrow and V");

    machine.cpu.r0_7[0] = UINT32_C(0x80000001);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02080),
                                            TEST_CPSR_V, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(2) &&
                  (machine.cpu.cpsr & (TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_C | TEST_CPSR_V),
              "MOVS LSL immediate one returns shifted bit as carry");

    machine.cpu.r0_7[0] = UINT32_C(0x80000001);
    machine.cpu.r0_7[1] = UINT32_C(0x00000100);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02110),
                                            TEST_CPSR_C, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0x80000001) &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "register shift uses low eight count bits and preserves carry at zero");

    machine.cpu.r0_7[0] = UINT32_C(0x80000001);
    machine.cpu.r0_7[1] = UINT32_C(31);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02110),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0x80000000) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_C)) ==
                      TEST_CPSR_N,
              "register LSL count 31 returns value and carry");

    machine.cpu.r0_7[0] = UINT32_C(0x80000001);
    machine.cpu.r0_7[1] = UINT32_C(32);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02110),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[2] == 0u &&
                  result.cycles == 2u &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "register LSL count 32 costs an internal cycle and uses bit zero");

    machine.cpu.r0_7[0] = UINT32_C(0x80000001);
    machine.cpu.r0_7[1] = UINT32_C(33);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02110),
                                            TEST_CPSR_C, &result);
    test_case(ran && machine.cpu.r0_7[2] == 0u &&
                  (machine.cpu.cpsr & TEST_CPSR_C) == 0u,
              "register LSL count 33 produces zero and clear carry");

    machine.cpu.r0_7[0] = UINT32_C(0x80000001);
    machine.cpu.r0_7[1] = UINT32_C(255);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02110),
                                            TEST_CPSR_C, &result);
    test_case(ran && machine.cpu.r0_7[2] == 0u &&
                  (machine.cpu.cpsr & TEST_CPSR_C) == 0u,
              "register LSL count 255 is not reduced modulo word width");

    machine.cpu.r0_7[0] = UINT32_C(0x80000001);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B03020),
                                            TEST_CPSR_V, &result);
    test_case(ran && machine.cpu.r0_7[3] == 0u &&
                  (machine.cpu.cpsr & (TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_C | TEST_CPSR_V),
              "encoded LSR immediate zero means shift by 32");

    machine.cpu.r0_7[0] = UINT32_C(0x80000001);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B03040),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[3] == UINT32_MAX &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "encoded ASR immediate zero sign-fills by 32");

    machine.cpu.r0_7[0] = UINT32_C(3);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B04060),
                                            TEST_CPSR_C, &result);
    test_case(ran && machine.cpu.r0_7[4] == UINT32_C(0x80000001) &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "ROR immediate zero performs RRX using old carry");

    machine.cpu.r0_7[0] = UINT32_C(3);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B04060),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[4] == UINT32_C(1) &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "RRX with clear old carry inserts zero and returns bit zero");

    machine.cpu.r0_7[0] = UINT32_C(0x12345678);
    machine.cpu.r0_7[4] = UINT32_C(0xA5A5A5A5);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0x11A04000),
                                            TEST_CPSR_Z, &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  result.instructions == 1u &&
                  machine.cpu.r0_7[4] == UINT32_C(0xA5A5A5A5) &&
                  machine.cpu.execution_address == UINT32_C(0x08000004),
              "failed ARM condition retires without data effects");

    machine.cpu.r0_7[0] = 0u;
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE3A00102),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[0] == UINT32_C(0x80000000),
              "MOV immediate rotates the eight-bit literal by an even amount");

    machine.cpu.r0_7[2] = UINT32_C(0x80000000);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE2921102),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[1] == 0u &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V),
              "ADDS immediate uses rotated operand and independent carry/overflow");

    machine.cpu.r0_7[0] = UINT32_C(0xF0F0);
    machine.cpu.r0_7[1] = UINT32_C(0x0FF0);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE0102001),
                                            TEST_CPSR_C | TEST_CPSR_V, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0xF0) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_C | TEST_CPSR_V),
              "ANDS updates N/Z/C while preserving V");

    machine.cpu.r0_7[0] = UINT32_C(0xF0);
    machine.cpu.r0_7[1] = UINT32_C(0x0F);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1803001),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[3] == UINT32_C(0xFF),
              "ORR combines two register operands");

    machine.cpu.r0_7[0] = UINT32_C(1);
    machine.cpu.r0_7[1] = UINT32_C(1);
    machine.cpu.r0_7[2] = UINT32_C(0xA5A5A5A5);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1500001),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0xA5A5A5A5) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_Z | TEST_CPSR_C),
              "CMP sets flags without writing its encoded destination");

    {
        static const bool expected[] = {
            false, true, true, false, false, true, false, true,
            true, false, true, false, true, false, true
        };
        bool predicates_match = true;
        uint8_t condition;

        for (condition = 0u; condition < (uint8_t)15u; ++condition) {
            const uint32_t opcode = ((uint32_t)condition << 28u) |
                                    UINT32_C(0x01A04000);

            machine.cpu.r0_7[0] = UINT32_C(0x12345678);
            machine.cpu.r0_7[4] = UINT32_C(0xA5A5A5A5);
            ran = loaded && run_single_instruction(
                &machine, opcode, TEST_CPSR_C, &result);
            if (!ran || result.instructions != 1u ||
                machine.cpu.r0_7[4] !=
                    (expected[condition] ? UINT32_C(0x12345678)
                                         : UINT32_C(0xA5A5A5A5))) {
                predicates_match = false;
            }
        }
        test_case(loaded && predicates_match,
                  "all ARM condition predicates select the expected effects");
    }

    machine.cpu.r0_7[0] = UINT32_C(0x80000001);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02000),
                                            TEST_CPSR_C, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0x80000001) &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "encoded LSL immediate zero leaves value and carry unchanged");

    machine.cpu.r0_7[0] = UINT32_C(0x80000003);
    machine.cpu.r0_7[1] = UINT32_C(1);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02130),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0x40000001) &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "register LSR shifts the selected operand and carry");

    machine.cpu.r0_7[0] = UINT32_C(0x80000003);
    machine.cpu.r0_7[1] = UINT32_C(1);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02150),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0xC0000001) &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "register ASR sign-extends with an explicit unsigned mask");

    machine.cpu.r0_7[0] = UINT32_C(0x80000003);
    machine.cpu.r0_7[1] = UINT32_C(1);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02170),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0xC0000001) &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "register ROR rotates by the low eight-bit count");

    machine.cpu.r0_7[0] = UINT32_C(0x12345678);
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02460),
                                            0u, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0x78123456) &&
                  (machine.cpu.cpsr & TEST_CPSR_C) == 0u,
              "nonzero immediate ROR rotates and reports result bit 31");

    {
        const gbl_shift_result lsr32 = gbl_shift(
            GBL_SHIFT_LSR, UINT32_C(0x80000001), 32u, false, true);
        const gbl_shift_result asr33 = gbl_shift(
            GBL_SHIFT_ASR, UINT32_C(0x80000001), 33u, false, true);
        const gbl_shift_result ror33 = gbl_shift(
            GBL_SHIFT_ROR, UINT32_C(0x80000003), 33u, false, true);

        test_case(lsr32.value == 0u && lsr32.carry &&
                      asr33.value == UINT32_MAX && asr33.carry &&
                      ror33.value == UINT32_C(0xC0000001) && ror33.carry,
                  "helper defines large LSR/ASR/ROR counts without host UB");
    }

    gbl_machine_dispose(&machine);
}

static void test_thumb(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran;

    make_empty_image(image);
    write_halfword(image, 0x10u, UINT16_C(0x2342));
    write_halfword(image, 0x12u, UINT16_C(0x2301));
    write_word(image, 0x20u, UINT32_C(0xE1A00000));
    loaded = machine_load(&machine, image);

    machine.cpu.r0_7[0] = UINT32_C(0x08000011);
    machine.cpu.usr_r8_14[6] = UINT32_C(0xA5A5A5A5);
    {
        const uint64_t cycles_before = machine.master_cycles;
        const bool one_short = loaded && run_arm_instruction(
            &machine, UINT32_C(0xE12FFF10), 0u, 1u, &result) &&
            result.termination == GBL_RUN_TIMEOUT &&
            result.reason == GBL_RUN_REASON_CYCLE_BUDGET &&
            result.cycles == 0u && machine.master_cycles == cycles_before &&
            machine.cpu.execution_address == UINT32_C(0x08000000) &&
            (machine.cpu.cpsr & UINT32_C(0x20)) == 0u &&
            machine.cpu.usr_r8_14[6] == UINT32_C(0xA5A5A5A5);
    ran = loaded && run_arm_instruction(&machine, UINT32_C(0xE12FFF10), 0u,
                                        2u, &result);
    test_case(one_short && ran && result.cycles == 2u &&
                  result.instructions == 1u &&
                  machine.cpu.execution_address == UINT32_C(0x08000010) &&
                  (machine.cpu.cpsr & UINT32_C(0x20)) != 0u &&
                  machine.cpu.pipeline[0].width == 2u &&
                  machine.cpu.pipeline[0].opcode == UINT32_C(0x2342) &&
                  machine.cpu.pipeline[1].opcode == UINT32_C(0x2301),
              "ARM BX preflights its budget then enters Thumb through bus refill");
    }

    ran = loaded && run_units(&machine, 1u, 1u, &result);
    test_case(ran && result.cycles == 1u && result.instructions == 1u &&
                  machine.cpu.r0_7[3] == UINT32_C(0x42) &&
                  machine.cpu.execution_address == UINT32_C(0x08000012),
              "Thumb instruction fetched by BX executes through the same core");

    machine.cpu.r0_7[0] = 0u;
    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x207F),
                                          TEST_CPSR_C | TEST_CPSR_V, 1u,
                                          &result);
    test_case(ran && machine.cpu.r0_7[0] == UINT32_C(0x7F) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_C | TEST_CPSR_V),
              "Thumb MOV immediate updates N/Z and preserves C/V");

    machine.cpu.r0_7[0] = UINT32_C(0x7FFFFFFF);
    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x3001), 0u,
                                          1u, &result);
    test_case(ran && machine.cpu.r0_7[0] == UINT32_C(0x80000000) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_N | TEST_CPSR_V),
              "Thumb ADD immediate wraps at signed maximum and sets overflow");

    machine.cpu.r0_7[0] = UINT32_C(0x80000000);
    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x3801), 0u,
                                          1u, &result);
    test_case(ran && machine.cpu.r0_7[0] == UINT32_C(0x7FFFFFFF) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_C | TEST_CPSR_V),
              "Thumb SUB immediate wraps with no-borrow carry and overflow");

    machine.cpu.r0_7[0] = UINT32_C(1);
    machine.cpu.r0_7[1] = UINT32_C(0xA5A5A5A5);
    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x2801), 0u,
                                          1u, &result);
    test_case(ran && machine.cpu.r0_7[1] == UINT32_C(0xA5A5A5A5) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_Z | TEST_CPSR_C),
              "Thumb CMP immediate updates flags without changing registers");

    machine.cpu.r0_7[0] = UINT32_C(0x12345678);
    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x4678), 0u,
                                          1u, &result);
    machine.cpu.r0_7[0] = UINT32_C(0x87654321);
    ran = ran && run_thumb_instruction(&machine, UINT16_C(0x4680), 0u,
                                       1u, &result);
    test_case(ran && machine.cpu.r0_7[0] == UINT32_C(0x87654321) &&
                  machine.cpu.usr_r8_14[0] == UINT32_C(0x87654321),
              "Thumb high MOV reads PC plus four and writes high registers");

    machine.cpu.r0_7[4] = UINT32_C(0x08000020);
    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x4720), 0u,
                                          2u, &result);
    test_case(ran && result.cycles == 2u &&
                  machine.cpu.execution_address == UINT32_C(0x08000020) &&
                  (machine.cpu.cpsr & UINT32_C(0x20)) == 0u &&
                  machine.cpu.pipeline[0].width == 4u &&
                  machine.cpu.pipeline[0].opcode == UINT32_C(0xE1A00000),
              "Thumb BX returns to ARM and refills aligned word instructions");

    gbl_machine_dispose(&machine);
}

static void test_decode(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran;

    make_empty_image(image);
    write_word(image, 0x10u, UINT32_C(0xE1A0200E));
    write_halfword(image, 0x06u, UINT16_C(0x2105));
    write_halfword(image, 0x08u, UINT16_C(0x2001));
    loaded = machine_load(&machine, image);

    ran = loaded && run_arm_instruction(&machine, UINT32_C(0xEB000002), 0u,
                                        2u, &result);
    ran = ran && result.instructions == 1u && result.cycles == 2u &&
          machine.cpu.execution_address == UINT32_C(0x08000010) &&
          machine.cpu.usr_r8_14[6] == UINT32_C(0x08000004) &&
          machine.cpu.pipeline[0].opcode == UINT32_C(0xE1A0200E);
    ran = ran && run_units(&machine, 1u, 1u, &result) &&
          machine.cpu.r0_7[2] == UINT32_C(0x08000004);
    test_case(loaded && ran,
              "ARM BL links the next instruction and refills without stale slots");

    ran = loaded && run_arm_instruction(&machine, UINT32_C(0x1A000001),
                                        TEST_CPSR_Z, 1u, &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  result.cycles == 1u && machine.cpu.execution_address ==
                      UINT32_C(0x08000004) &&
                  machine.cpu.pipeline[0].address == UINT32_C(0x08000004),
              "condition-failed ARM branch uses sequential fetch only");

    {
        const uint64_t cycles_before = machine.master_cycles;
        const bool one_short = loaded && run_thumb_instruction(
            &machine, UINT16_C(0xE002), 0u, 1u, &result) &&
            result.termination == GBL_RUN_TIMEOUT &&
            result.reason == GBL_RUN_REASON_CYCLE_BUDGET &&
            result.cycles == 0u && machine.master_cycles == cycles_before &&
            machine.cpu.execution_address == UINT32_C(0x08000000);
        ran = loaded && run_thumb_instruction(&machine, UINT16_C(0xE002), 0u,
                                              2u, &result);
        test_case(one_short && ran && result.cycles == 2u &&
                  machine.cpu.execution_address == UINT32_C(0x08000008) &&
                  machine.cpu.pipeline[0].opcode == UINT32_C(0x2001) &&
                  machine.cpu.pipeline[0].width == 2u,
                  "Thumb branch preflights budget and discards stale slots");
    }

    {
        const bool taken = loaded && run_thumb_instruction(
            &machine, UINT16_C(0xD001), TEST_CPSR_Z, 2u, &result) &&
            result.cycles == 2u &&
            machine.cpu.execution_address == UINT32_C(0x08000006) &&
            machine.cpu.pipeline[0].opcode == UINT32_C(0x2105);
        const bool failed = loaded && run_thumb_instruction(
            &machine, UINT16_C(0xD001), 0u, 1u, &result) &&
            result.cycles == 1u &&
            machine.cpu.execution_address == UINT32_C(0x08000002);

        test_case(taken && failed,
                  "Thumb conditional branch selects target or sequential slot");
    }

    {
        const bool multiply = loaded && run_arm_instruction(
            &machine, UINT32_C(0xE0000090), 0u, 1u, &result) &&
            result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
            machine.cpu.stop_reason == GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
        const bool later_condition = loaded && run_arm_instruction(
            &machine, UINT32_C(0xF1A00000), 0u, 1u, &result) &&
            result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_GUEST_STOP &&
            machine.cpu.stop_reason == GBL_CPU_STOP_UNDEFINED_NO_BIOS &&
            machine.cpu.spsr[4] == TEST_CPSR_SYSTEM &&
            machine.cpu.execution_address == UINT32_C(4);

        test_case(multiply && later_condition,
                  "valid unsupported and reserved ARM words stop distinctly");
    }

    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0xDE00), 0u,
                                          1u, &result);
    test_case(ran && result.termination == GBL_RUN_STOPPED &&
                  result.reason == GBL_RUN_REASON_GUEST_STOP &&
                  machine.cpu.stop_reason == GBL_CPU_STOP_UNDEFINED_NO_BIOS &&
                  machine.cpu.spsr[4] ==
                      (TEST_CPSR_SYSTEM | UINT32_C(0x20)) &&
                  machine.cpu.execution_address == UINT32_C(4),
              "reserved Thumb conditional encoding is undefined, not a no-op");

    {
        const uint64_t cycles_before = machine.master_cycles;
        bool invalid_arm_alignment;
        bool invalid_refill_extent;

        machine.cpu.r0_7[0] = UINT32_C(0x08000002);
        invalid_arm_alignment = loaded && run_thumb_instruction(
            &machine, UINT16_C(0x4700), 0u, 2u, &result) &&
            result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
            result.cycles == 0u && machine.master_cycles == cycles_before;
        machine.cpu.r0_7[0] = UINT32_C(0xFFFFFFFC);
        invalid_refill_extent = loaded && run_arm_instruction(
            &machine, UINT32_C(0xE12FFF10), 0u, 2u, &result) &&
            result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
            result.cycles == 0u && machine.master_cycles == cycles_before;

        test_case(invalid_arm_alignment && invalid_refill_extent,
                  "BX rejects ARM misalignment and refill extent before effects");
    }

    gbl_machine_dispose(&machine);
}

static void test_alignment(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran;

    make_empty_image(image);
    write_word(image, 0x0Cu, UINT32_C(0x76543210));
    write_word(image, 0x08u, UINT32_C(0xDEADBEEF));
    loaded = machine_load(&machine, image);
    machine.iwram[0x10u] = UINT8_C(0x11);
    machine.iwram[0x11u] = UINT8_C(0x22);
    machine.iwram[0x12u] = UINT8_C(0x33);
    machine.iwram[0x13u] = UINT8_C(0x44);
    machine.cpu.r0_7[0] = UINT32_C(0x03000011);
    ran = loaded && run_arm_instruction(&machine, UINT32_C(0xE5902000), 0u,
                                        2u, &result);
    test_case(ran && result.cycles == 2u &&
                  machine.cpu.r0_7[2] == UINT32_C(0x11443322),
              "unaligned ARM word LDR rotates aligned bus data by the address lane");

    machine.cpu.r0_7[0] = UINT32_C(0x03000011);
    machine.cpu.r0_7[1] = UINT32_C(0xAABBCCDD);
    ran = loaded && run_arm_instruction(&machine, UINT32_C(0xE5801000), 0u,
                                        2u, &result);
    test_case(ran && result.cycles == 2u &&
                  machine.iwram[0x10u] == UINT8_C(0xDD) &&
                  machine.iwram[0x11u] == UINT8_C(0xCC) &&
                  machine.iwram[0x12u] == UINT8_C(0xBB) &&
                  machine.iwram[0x13u] == UINT8_C(0xAA),
              "unaligned ARM word STR aligns its data bus request down");

    machine.cpu.r0_7[0] = UINT32_C(0x03000020);
    machine.cpu.r0_7[1] = UINT32_C(0xA5);
    machine.iwram[0x20u] = UINT8_C(0x11);
    machine.iwram[0x21u] = UINT8_C(0x22);
    machine.iwram[0x22u] = UINT8_C(0x33);
    machine.iwram[0x23u] = UINT8_C(0x44);
    ran = loaded && run_arm_instruction(&machine, UINT32_C(0xE5C01003), 0u,
                                        2u, &result);
    test_case(ran && machine.iwram[0x20u] == UINT8_C(0x11) &&
                  machine.iwram[0x21u] == UINT8_C(0x22) &&
                  machine.iwram[0x22u] == UINT8_C(0x33) &&
                  machine.iwram[0x23u] == UINT8_C(0xA5),
              "ARM STRB writes exactly the selected byte through the bus");

    machine.cpu.r0_7[0] = UINT32_C(0x03000020);
    machine.iwram[0x23u] = UINT8_C(0xAB);
    ran = loaded && run_arm_instruction(&machine, UINT32_C(0xE5D02003), 0u,
                                        2u, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0xAB),
              "ARM LDRB zero-extends the requested bus byte");

    ran = loaded && run_arm_instruction(&machine, UINT32_C(0xE59F2004), 0u,
                                        2u, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0x76543210),
              "ARM LDR literal reads PC plus eight through the canonical bus");

    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x4801), 0u,
                                          2u, &result);
    test_case(ran && result.cycles == 2u &&
                  machine.cpu.r0_7[0] == UINT32_C(0xDEADBEEF),
              "Thumb PC-relative LDR aligns PC plus four before its offset");

    machine.iwram[0x30u] = UINT8_C(0x12);
    machine.iwram[0x31u] = UINT8_C(0x34);
    machine.iwram[0x32u] = UINT8_C(0x56);
    machine.iwram[0x33u] = UINT8_C(0x78);
    machine.cpu.r0_7[1] = UINT32_C(0x03000030);
    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x6808), 0u,
                                          2u, &result);
    machine.cpu.r0_7[1] = UINT32_C(0x03000030);
    machine.cpu.r0_7[2] = UINT32_C(0xA5);
    ran = ran && run_thumb_instruction(&machine, UINT16_C(0x704A), 0u,
                                       2u, &result);
    machine.cpu.r0_7[0] = UINT32_C(0xAABBCCDD);
    machine.cpu.r0_7[1] = UINT32_C(0x03000040);
    ran = ran && run_thumb_instruction(&machine, UINT16_C(0x6048), 0u,
                                       2u, &result);
    machine.iwram[0x50u] = UINT8_C(0x11);
    machine.iwram[0x51u] = UINT8_C(0xAB);
    machine.cpu.r0_7[1] = UINT32_C(0x03000050);
    ran = ran && run_thumb_instruction(&machine, UINT16_C(0x784A), 0u,
                                       2u, &result);
    test_case(ran && machine.cpu.r0_7[0] == UINT32_C(0xAABBCCDD) &&
                  machine.cpu.r0_7[2] == UINT32_C(0xAB) &&
                  machine.iwram[0x31u] == UINT8_C(0xA5) &&
                  machine.iwram[0x44u] == UINT8_C(0xDD) &&
                  machine.iwram[0x45u] == UINT8_C(0xCC) &&
                  machine.iwram[0x46u] == UINT8_C(0xBB) &&
                  machine.iwram[0x47u] == UINT8_C(0xAA),
              "Thumb immediate word/byte loads and stores use low registers");

    machine.iwram[0x10u] = UINT8_C(0x11);
    machine.iwram[0x11u] = UINT8_C(0x22);
    machine.iwram[0x12u] = UINT8_C(0x33);
    machine.iwram[0x13u] = UINT8_C(0x44);
    machine.cpu.r0_7[0] = UINT32_C(0x03000011);
    machine.cpu.execution_address = UINT32_C(0x08000000);
    machine.cpu.cpsr = TEST_CPSR_SYSTEM;
    machine.cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000000), UINT32_C(0xE5902000), 4u, true
    };
    machine.cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000004), UINT32_C(0xE1A00000), 4u, true
    };
    machine.cpu.next_fetch_sequential = true;
    machine.cpu.stopped = false;
    machine.cpu.stop_reason = GBL_CPU_STOP_NONE;
    {
        const gbl_machine before = machine;
        const gbl_run_options short_options = { 1u, 1u };
        const gbl_run_options exact_options = { 2u, 1u };
        gbl_run_result short_result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        gbl_run_result exact_result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        const bool one_short = gbl_cpu_run(&machine, &short_options,
                                           &short_result) ==
                                   GBL_RUN_STATUS_OK &&
                               short_result.termination == GBL_RUN_TIMEOUT &&
                               short_result.reason ==
                                   GBL_RUN_REASON_CYCLE_BUDGET &&
                               short_result.cycles == 0u &&
                               gbl_test_canonical_equal(&before, &machine);
        const bool exact = gbl_cpu_run(&machine, &exact_options,
                                       &exact_result) == GBL_RUN_STATUS_OK &&
                           exact_result.termination == GBL_RUN_PROGRESS &&
                           exact_result.cycles == 2u &&
                           machine.cpu.r0_7[2] == UINT32_C(0x11443322);

        test_case(one_short && exact,
                  "transfer preflight refuses one-short and commits exact cost");
    }

    gbl_machine_dispose(&machine);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        return 2;
    }
    printf("TAP version 13\n");
    if (strcmp(argv[1], "lifecycle") == 0) {
        printf("1..9\n");
        test_lifecycle();
    } else if (strcmp(argv[1], "tracer") == 0) {
        printf("1..4\n");
        test_tracer();
    } else if (strcmp(argv[1], "exceptions") == 0) {
        printf("1..13\n");
        test_exceptions();
        test_masked_requests();
        test_exception_matrix();
        test_exception_refusals();
        test_exception_restore_and_overwrite();
    } else if (strcmp(argv[1], "modes") == 0) {
        printf("1..5\n");
        test_modes();
    } else if (strcmp(argv[1], "no_bios") == 0) {
        printf("1..2\n");
        test_no_bios();
    } else if (strcmp(argv[1], "budget") == 0) {
        printf("1..9\n");
        test_budget();
    } else if (strcmp(argv[1], "arithmetic") == 0) {
        printf("1..28\n");
        test_arithmetic();
    } else if (strcmp(argv[1], "thumb") == 0) {
        printf("1..8\n");
        test_thumb();
    } else if (strcmp(argv[1], "decode") == 0) {
        printf("1..7\n");
        test_decode();
    } else if (strcmp(argv[1], "alignment") == 0) {
        printf("1..8\n");
        test_alignment();
    } else {
        return 2;
    }
    return test_failures == 0u ? 0 : 1;
}
