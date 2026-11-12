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
#define TEST_SWP_ADDRESS UINT32_C(0x03000041)
#define TEST_SWP_BASE_ADDRESS UINT32_C(0x03000040)
#define TEST_MAILBOX_OFFSET 0x7FC0u
#define TEST_MAILBOX_PASS_CASE UINT32_C(0x0000002A)
#define TEST_MAILBOX_FAIL_CASE UINT32_C(0x0000BEEF)

static unsigned int test_number;
static unsigned int test_failures;

typedef struct cpu_test_allocator_state {
    bool fail_next_allocation;
    size_t successful_allocation_count;
    size_t live_allocation_count;
    size_t live_bytes;
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
    void *allocation;

    if (state != NULL && state->fail_next_allocation) {
        state->fail_next_allocation = false;
        return NULL;
    }
    allocation = malloc(size);
    if (state != NULL && allocation != NULL) {
        state->successful_allocation_count++;
        state->live_allocation_count++;
        state->live_bytes += size;
    }
    return allocation;
}

static void cpu_test_release(void *context, void *pointer, size_t size)
{
    cpu_test_allocator_state *state = context;

    if (state != NULL && pointer != NULL) {
        state->live_allocation_count--;
        state->live_bytes -= size;
    }
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
    const gbl_run_options options = {
        .cycle_budget = cycles,
        .unit_limit = units,
        .category_mask = 0u,
        .trace_buffer = NULL
    };

    return gbl_cpu_run(machine, &options, result) == GBL_RUN_STATUS_OK;
}

static bool run_traced_units(gbl_machine *machine, uint64_t cycles,
                             uint64_t units, uint32_t category_mask,
                             gbl_trace_buffer *trace,
                             gbl_run_result *result)
{
    const gbl_run_options options = {
        .cycle_budget = cycles,
        .unit_limit = units,
        .category_mask = category_mask,
        .trace_buffer = trace
    };

    return gbl_cpu_run(machine, &options, result) == GBL_RUN_STATUS_OK;
}

static void prepare_arm_instruction(gbl_machine *machine, uint32_t opcode,
                                    uint32_t cpsr)
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
}

static bool run_arm_instruction(gbl_machine *machine, uint32_t opcode,
                                uint32_t cpsr, uint64_t cycle_budget,
                                gbl_run_result *result)
{
    prepare_arm_instruction(machine, opcode, cpsr);
    return run_units(machine, cycle_budget, 1u, result);
}

static bool run_arm_instruction_with_trace(
    gbl_machine *machine, uint32_t opcode, uint32_t cpsr,
    uint64_t cycle_budget, gbl_trace_buffer *trace, gbl_run_result *result)
{
    const gbl_run_options options = {
        .cycle_budget = cycle_budget,
        .unit_limit = 1u,
        .category_mask = GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS,
        .trace_buffer = trace
    };

    prepare_arm_instruction(machine, opcode, cpsr);
    return gbl_cpu_run(machine, &options, result) == GBL_RUN_STATUS_OK;
}

static uint32_t encode_arm_transfer(bool register_offset, bool preindexed,
                                    bool add_offset, bool byte,
                                    bool writeback, bool load,
                                    uint8_t base_register,
                                    uint8_t data_register, uint16_t offset)
{
    return UINT32_C(0xE4000000) |
           (register_offset ? UINT32_C(1) << 25u : 0u) |
           (preindexed ? UINT32_C(1) << 24u : 0u) |
           (add_offset ? UINT32_C(1) << 23u : 0u) |
           (byte ? UINT32_C(1) << 22u : 0u) |
           (writeback ? UINT32_C(1) << 21u : 0u) |
           (load ? UINT32_C(1) << 20u : 0u) |
           ((uint32_t)base_register << 16u) |
           ((uint32_t)data_register << 12u) | (uint32_t)offset;
}

static uint32_t encode_arm_subword(bool register_offset, bool preindexed,
                                   bool add_offset, bool writeback,
                                   bool load, bool sign, bool halfword,
                                   uint8_t base_register,
                                   uint8_t data_register, uint16_t offset)
{
    uint32_t opcode = UINT32_C(0xE0000090) |
        (preindexed ? UINT32_C(1) << 24u : 0u) |
        (add_offset ? UINT32_C(1) << 23u : 0u) |
        (!register_offset ? UINT32_C(1) << 22u : 0u) |
        (writeback ? UINT32_C(1) << 21u : 0u) |
        (load ? UINT32_C(1) << 20u : 0u) |
        ((uint32_t)base_register << 16u) |
        ((uint32_t)data_register << 12u) |
        (sign ? UINT32_C(1) << 6u : 0u) |
        (halfword ? UINT32_C(1) << 5u : 0u);

    if (register_offset) {
        opcode |= (uint32_t)(offset & UINT16_C(0x000F));
    } else {
        opcode |= ((uint32_t)(offset & UINT16_C(0x00F0)) << 4u) |
                  (uint32_t)(offset & UINT16_C(0x000F));
    }
    return opcode;
}

static uint32_t read_test_word(const uint8_t *bytes, size_t offset)
{
    return (uint32_t)bytes[offset] |
           ((uint32_t)bytes[offset + 1u] << 8u) |
           ((uint32_t)bytes[offset + 2u] << 16u) |
           ((uint32_t)bytes[offset + 3u] << 24u);
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
    cpu_test_allocator_state allocator_state = { 0 };
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
        const gbl_run_options options = {
            .cycle_budget = 1u,
            .unit_limit = 1u,
            .category_mask = 0u,
            .trace_buffer = NULL
        };
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
        cpu_test_allocator_state no_rom_allocator_state = { 0 };
        const gbl_run_options options = {
            .cycle_budget = 2u,
            .unit_limit = 0u,
            .category_mask = 0u,
            .trace_buffer = NULL
        };
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
        run_guest_exception_case(false, UINT32_C(0xE7F000F0),
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

static void test_arm_adc_carry_in(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran = false;

    make_program(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = 0u;
        ran = run_arm_instruction(&machine, UINT32_C(0xE2B00001),
                                  TEST_CPSR_C | TEST_CPSR_V, 2u, &result);
    }
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                  result.instructions == 1u &&
                  machine.cpu.r0_7[0] == UINT32_C(2) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) == 0u,
              "ADC incorporates CPSR carry-in in the actual register and NZCV result");
    gbl_machine_dispose(&machine);
}

typedef struct arm_alu_test_vector {
    const char *description;
    uint32_t opcode;
    uint32_t left;
    uint32_t right;
    uint32_t initial_flags;
    uint32_t expected_result;
    uint32_t expected_flags;
    uint32_t flag_mask;
    bool writes_r2;
} arm_alu_test_vector;

static void test_arm_alu_vectors(void)
{
    static const uint32_t nzcv = TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C |
                                 TEST_CPSR_V;
    static const uint32_t nzc = TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C;
    static const arm_alu_test_vector vectors[] = {
        { "ANDS result and logical flag mask", UINT32_C(0xE0112003),
          UINT32_C(0xF0F00FF0), UINT32_C(0x0FF0F00F), nzcv,
          UINT32_C(0x00F00000), TEST_CPSR_C, nzc, true },
        { "EORS result and logical flag mask", UINT32_C(0xE0312003),
          UINT32_C(0xF0F00FF0), UINT32_C(0x0FF0F00F), nzcv,
          UINT32_C(0xFF00FFFF), TEST_CPSR_N | TEST_CPSR_C, nzc, true },
        { "SUBS result, no-borrow carry, and overflow", UINT32_C(0xE0512003),
          UINT32_C(0x80000000), UINT32_C(1), nzcv,
          UINT32_C(0x7FFFFFFF), TEST_CPSR_C | TEST_CPSR_V, nzcv, true },
        { "RSBS operand order, borrow, and overflow", UINT32_C(0xE0712003),
          UINT32_C(0x80000000), UINT32_C(0), nzcv,
          UINT32_C(0x80000000), TEST_CPSR_N | TEST_CPSR_V, nzcv, true },
        { "ADDS result and signed overflow", UINT32_C(0xE0912003),
          UINT32_C(0x7FFFFFFF), UINT32_C(1), nzcv,
          UINT32_C(0x80000000), TEST_CPSR_N | TEST_CPSR_V, nzcv, true },
        { "ADCS includes carry-in", UINT32_C(0xE0B12003),
          UINT32_C(0x7FFFFFFF), UINT32_C(0), nzcv,
          UINT32_C(0x80000000), TEST_CPSR_N | TEST_CPSR_V, nzcv, true },
        { "SBCS subtracts borrow when carry is clear", UINT32_C(0xE0D12003),
          UINT32_C(0x80000000), UINT32_C(0x7FFFFFFF),
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_V, UINT32_C(0),
          TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V, nzcv, true },
        { "RSCS reverses operands and includes borrow", UINT32_C(0xE0F12003),
          UINT32_C(0x80000000), UINT32_C(0x7FFFFFFF),
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_V, UINT32_C(0xFFFFFFFE),
          TEST_CPSR_N | TEST_CPSR_V, nzcv, true },
        { "TST updates flags without a destination", UINT32_C(0xE1110003),
          UINT32_C(0x80000000), UINT32_C(0x80000000), nzcv,
          UINT32_C(0x80000000), TEST_CPSR_N | TEST_CPSR_C, nzc, false },
        { "TEQ updates flags without a destination", UINT32_C(0xE1310003),
          UINT32_C(0x80000000), UINT32_C(0x80000000), nzcv,
          UINT32_C(0), TEST_CPSR_Z | TEST_CPSR_C, nzc, false },
        { "CMP updates arithmetic flags without a destination",
          UINT32_C(0xE1510003), UINT32_C(0x80000000),
          UINT32_C(0x80000000), nzcv, UINT32_C(0),
          TEST_CPSR_Z | TEST_CPSR_C, nzcv, false },
        { "CMN updates arithmetic flags without a destination",
          UINT32_C(0xE1710003), UINT32_C(0x7FFFFFFF), UINT32_C(1), nzcv,
          UINT32_C(0x80000000), TEST_CPSR_N | TEST_CPSR_V, nzcv, false },
        { "ORRS result and logical flag mask", UINT32_C(0xE1912003),
          UINT32_C(0x0000F0F0), UINT32_C(0xF000000F), nzcv,
          UINT32_C(0xF000F0FF), TEST_CPSR_N | TEST_CPSR_C, nzc, true },
        { "MOVS result and logical flag mask", UINT32_C(0xE1B02003),
          UINT32_C(0), UINT32_C(0), nzcv, UINT32_C(0),
          TEST_CPSR_Z | TEST_CPSR_C, nzc, true },
        { "BICS result and logical flag mask", UINT32_C(0xE1D12003),
          UINT32_C(0xFFFF0000), UINT32_C(0xFF00FF00), nzcv,
          UINT32_C(0x00FF0000), TEST_CPSR_C, nzc, true },
        { "MVNS result and logical flag mask", UINT32_C(0xE1F02003),
          UINT32_C(0), UINT32_MAX, nzcv, UINT32_C(0),
          TEST_CPSR_Z | TEST_CPSR_C, nzc, true }
    };
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;
    size_t index;

    make_program(image);
    loaded = machine_load(&machine, image);
    for (index = 0u; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        const arm_alu_test_vector *vector = &vectors[index];
        const uint32_t sentinel = UINT32_C(0x13579BDF);
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool ran = false;
        bool registers_match;
        bool flags_match;

        machine.cpu.r0_7[0] = sentinel;
        machine.cpu.r0_7[1] = vector->left;
        machine.cpu.r0_7[2] = sentinel;
        machine.cpu.r0_7[3] = vector->right;
        if (loaded) {
            ran = run_arm_instruction(&machine, vector->opcode,
                                      vector->initial_flags, 1u, &result);
        }
        registers_match = machine.cpu.r0_7[0] == sentinel &&
                          machine.cpu.r0_7[1] == vector->left &&
                          machine.cpu.r0_7[3] == vector->right &&
                          (vector->writes_r2
                               ? machine.cpu.r0_7[2] ==
                                     vector->expected_result
                               : machine.cpu.r0_7[2] == sentinel);
        flags_match =
            (machine.cpu.cpsr & vector->flag_mask) ==
                vector->expected_flags &&
            (machine.cpu.cpsr & nzcv & ~vector->flag_mask) ==
                (vector->initial_flags & nzcv & ~vector->flag_mask);
        test_case(loaded && ran &&
                      result.termination == GBL_RUN_PROGRESS &&
                      result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      result.instructions == 1u && result.cycles == 1u &&
                      registers_match && flags_match,
                  vector->description);
    }
    gbl_machine_dispose(&machine);
}

static void test_arm_alu_without_s_preserves_flags(void)
{
    const uint32_t initial_flags = TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C |
                                   TEST_CPSR_V;
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran = false;

    make_program(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[1] = UINT32_C(1);
        machine.cpu.r0_7[2] = UINT32_C(0xA5A5A5A5);
        machine.cpu.r0_7[3] = UINT32_C(2);
        ran = run_arm_instruction(&machine, UINT32_C(0xE0812003),
                                  initial_flags, 1u, &result);
    }
    test_case(loaded && ran && result.instructions == 1u &&
                  machine.cpu.r0_7[2] == UINT32_C(3) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) ==
                      initial_flags,
              "ADD without S preserves all four status flags");
    gbl_machine_dispose(&machine);
}

typedef struct arm_multiply_unsigned_test_vector {
    const char *description;
    uint32_t opcode;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t initial_flags;
    uint32_t expected_r2;
    uint32_t expected_r4;
    uint32_t expected_flags;
} arm_multiply_unsigned_test_vector;

static void test_multiply_unsigned(void)
{
    static const uint32_t nzcv = TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C |
                                 TEST_CPSR_V;
    static const arm_multiply_unsigned_test_vector vectors[] = {
        { "MULS high-bit product sets N and preserves chosen C/V",
          UINT32_C(0xE0120391), UINT32_C(0x80000000), UINT32_C(0xA5A5A5A5),
          UINT32_C(1), 0u, TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          UINT32_C(0x80000000), 0u, TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V },
        { "MUL without S preserves NZCV on the all-ones low product",
          UINT32_C(0xE0020391), UINT32_MAX, 0u, UINT32_MAX, 0u, nzcv,
          UINT32_C(1), 0u, nzcv },
        { "MLAS reads the original aliased accumulator before its destination",
          UINT32_C(0xE0322391), UINT32_C(2), UINT32_MAX, UINT32_C(3), 0u,
          TEST_CPSR_N | TEST_CPSR_V, UINT32_C(5), 0u, TEST_CPSR_V },
        { "UMULLS returns both words of the all-ones unsigned product",
          UINT32_C(0xE0D42391), UINT32_MAX, UINT32_C(0xA5A5A5A5),
          UINT32_MAX, UINT32_C(0x5A5A5A5A), TEST_CPSR_Z | TEST_CPSR_C |
              TEST_CPSR_V,
          UINT32_C(1), UINT32_C(0xFFFFFFFE),
          TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V },
        { "UMLALS carries low accumulator overflow into the high word",
          UINT32_C(0xE0F42391), UINT32_C(1), UINT32_MAX, UINT32_C(1),
          UINT32_C(7), nzcv, 0u, UINT32_C(8), TEST_CPSR_C | TEST_CPSR_V },
        { "UMULLS zero product sets Z and preserves chosen C/V",
          UINT32_C(0xE0D42391), 0u, UINT32_C(0xA5A5A5A5), UINT32_MAX,
          UINT32_C(0x5A5A5A5A), TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          0u, 0u, TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V }
    };
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;
    size_t index;

    make_program(image);
    loaded = machine_load(&machine, image);
    for (index = 0u; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        const arm_multiply_unsigned_test_vector *vector = &vectors[index];
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool ran = false;

        machine.cpu.r0_7[1] = vector->r1;
        machine.cpu.r0_7[2] = vector->r2;
        machine.cpu.r0_7[3] = vector->r3;
        machine.cpu.r0_7[4] = vector->r4;
        if (loaded) {
            ran = run_arm_instruction(&machine, vector->opcode,
                                      vector->initial_flags, 16u, &result);
        }
        test_case(loaded && ran &&
                      result.termination == GBL_RUN_PROGRESS &&
                      result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      result.instructions == 1u && result.cycles >= 1u &&
                      machine.cpu.r0_7[1] == vector->r1 &&
                      machine.cpu.r0_7[3] == vector->r3 &&
                      machine.cpu.r0_7[2] == vector->expected_r2 &&
                      machine.cpu.r0_7[4] == vector->expected_r4 &&
                      (machine.cpu.cpsr & nzcv) == vector->expected_flags,
                  vector->description);
    }
    gbl_machine_dispose(&machine);
}

static void test_arm_register_shifted_pc_operand_uses_plus_twelve(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran = false;

    make_program(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[1] = 0u;
        machine.cpu.r0_7[2] = UINT32_C(0xA5A5A5A5);
        ran = run_arm_instruction(&machine, UINT32_C(0xE1B0211F),
                                  TEST_CPSR_C | TEST_CPSR_V, 2u, &result);
    }
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                  result.instructions == 1u && result.cycles == 2u &&
                  machine.cpu.r0_7[2] == UINT32_C(0x0800000C) &&
                  (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                       TEST_CPSR_C | TEST_CPSR_V)) ==
                      (TEST_CPSR_C | TEST_CPSR_V),
              "register-shifted Operand2 reads PC as instruction address plus twelve");
    gbl_machine_dispose(&machine);
}

typedef struct arm_operand2_test_vector {
    const char *description;
    uint32_t opcode;
    uint32_t input_value;
    uint32_t register_count;
    uint32_t initial_flags;
    uint32_t expected_value;
    uint32_t expected_nzc;
    uint64_t expected_cycles;
} arm_operand2_test_vector;

static void test_arm_operand2_shift_vectors(void)
{
    static const uint32_t cv = TEST_CPSR_C | TEST_CPSR_V;
    static const uint32_t nzc = TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C;
    static const arm_operand2_test_vector vectors[] = {
        { "rotated immediate zero preserves carry", UINT32_C(0xE3B02000),
          0u, 0u, cv, 0u, TEST_CPSR_Z | TEST_CPSR_C, 1u },
        { "rotated immediate two derives clear carry", UINT32_C(0xE3B02180),
          0u, 0u, cv, UINT32_C(0x20), 0u, 1u },
        { "rotated immediate eight derives set carry", UINT32_C(0xE3B02480),
          0u, 0u, cv, UINT32_C(0x80000000), TEST_CPSR_N | TEST_CPSR_C, 1u },
        { "immediate LSL zero preserves carry", UINT32_C(0xE1B02000),
          UINT32_C(0x80000001), 0u, cv, UINT32_C(0x80000001),
          TEST_CPSR_N | TEST_CPSR_C, 1u },
        { "immediate LSL one returns shifted bit as carry",
          UINT32_C(0xE1B02080), UINT32_C(0x80000001), 0u, cv,
          UINT32_C(2), TEST_CPSR_C, 1u },
        { "immediate LSR zero means thirty-two", UINT32_C(0xE1B02020),
          UINT32_C(0x80000001), 0u, cv, 0u, TEST_CPSR_Z | TEST_CPSR_C, 1u },
        { "immediate LSR one shifts and returns bit zero",
          UINT32_C(0xE1B020A0), UINT32_C(0x80000001), 0u, cv,
          UINT32_C(0x40000000), TEST_CPSR_C, 1u },
        { "immediate ASR zero sign-fills by thirty-two",
          UINT32_C(0xE1B02040), UINT32_C(0x80000001), 0u, cv,
          UINT32_MAX, TEST_CPSR_N | TEST_CPSR_C, 1u },
        { "immediate ASR one sign-fills and returns bit zero",
          UINT32_C(0xE1B020C0), UINT32_C(0x80000001), 0u, cv,
          UINT32_C(0xC0000000), TEST_CPSR_N | TEST_CPSR_C, 1u },
        { "immediate ROR zero performs RRX with old carry",
          UINT32_C(0xE1B02060), UINT32_C(3), 0u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 1u },
        { "immediate ROR one rotates the register operand",
          UINT32_C(0xE1B020E0), UINT32_C(3), 0u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 1u },
        { "register LSL count zero preserves value and carry",
          UINT32_C(0xE1B02110), UINT32_C(0x80000001), 0u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register LSL count one shifts and returns bit thirty-one",
          UINT32_C(0xE1B02110), UINT32_C(0x80000001), 1u, cv,
          UINT32_C(2), TEST_CPSR_C, 2u },
        { "register LSL count thirty-one returns bit one as carry",
          UINT32_C(0xE1B02110), UINT32_C(0x80000001), 31u, cv,
          UINT32_C(0x80000000), TEST_CPSR_N, 2u },
        { "register LSL count thirty-two returns bit zero as carry",
          UINT32_C(0xE1B02110), UINT32_C(0x80000001), 32u, cv,
          0u, TEST_CPSR_Z | TEST_CPSR_C, 2u },
        { "register LSL count thirty-three clears carry",
          UINT32_C(0xE1B02110), UINT32_C(0x80000001), 33u, cv,
          0u, TEST_CPSR_Z, 2u },
        { "register LSL count 255 clears carry",
          UINT32_C(0xE1B02110), UINT32_C(0x80000001), 255u, cv,
          0u, TEST_CPSR_Z, 2u },
        { "register LSL count 256 uses low eight zero bits",
          UINT32_C(0xE1B02110), UINT32_C(0x80000001), 256u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register LSR count zero preserves value and carry",
          UINT32_C(0xE1B02130), UINT32_C(0x80000001), 0u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register LSR count one shifts and returns bit zero",
          UINT32_C(0xE1B02130), UINT32_C(0x80000001), 1u, cv,
          UINT32_C(0x40000000), TEST_CPSR_C, 2u },
        { "register LSR count thirty-one returns bit thirty",
          UINT32_C(0xE1B02130), UINT32_C(0x80000001), 31u, cv,
          UINT32_C(1), 0u, 2u },
        { "register LSR count thirty-two returns bit thirty-one",
          UINT32_C(0xE1B02130), UINT32_C(0x80000001), 32u, cv,
          0u, TEST_CPSR_Z | TEST_CPSR_C, 2u },
        { "register LSR count thirty-three clears carry",
          UINT32_C(0xE1B02130), UINT32_C(0x80000001), 33u, cv,
          0u, TEST_CPSR_Z, 2u },
        { "register LSR count 255 clears carry",
          UINT32_C(0xE1B02130), UINT32_C(0x80000001), 255u, cv,
          0u, TEST_CPSR_Z, 2u },
        { "register LSR count 256 uses low eight zero bits",
          UINT32_C(0xE1B02130), UINT32_C(0x80000001), 256u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ASR count zero preserves value and carry",
          UINT32_C(0xE1B02150), UINT32_C(0x80000001), 0u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ASR count one sign-fills and returns bit zero",
          UINT32_C(0xE1B02150), UINT32_C(0x80000001), 1u, cv,
          UINT32_C(0xC0000000), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ASR count thirty-one sign-fills",
          UINT32_C(0xE1B02150), UINT32_C(0x80000001), 31u, cv,
          UINT32_MAX, TEST_CPSR_N, 2u },
        { "register ASR count thirty-two sign-fills by word width",
          UINT32_C(0xE1B02150), UINT32_C(0x80000001), 32u, cv,
          UINT32_MAX, TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ASR count thirty-three sign-fills",
          UINT32_C(0xE1B02150), UINT32_C(0x80000001), 33u, cv,
          UINT32_MAX, TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ASR count 255 sign-fills",
          UINT32_C(0xE1B02150), UINT32_C(0x80000001), 255u, cv,
          UINT32_MAX, TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ASR count 256 uses low eight zero bits",
          UINT32_C(0xE1B02150), UINT32_C(0x80000001), 256u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ROR count zero preserves value and carry",
          UINT32_C(0xE1B02170), UINT32_C(0x80000001), 0u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ROR count one rotates right",
          UINT32_C(0xE1B02170), UINT32_C(0x80000001), 1u, cv,
          UINT32_C(0xC0000000), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ROR count thirty-one rotates left one",
          UINT32_C(0xE1B02170), UINT32_C(0x80000001), 31u, cv,
          UINT32_C(3), 0u, 2u },
        { "register ROR count thirty-two preserves value and derives carry",
          UINT32_C(0xE1B02170), UINT32_C(0x80000001), 32u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ROR count thirty-three repeats count one",
          UINT32_C(0xE1B02170), UINT32_C(0x80000001), 33u, cv,
          UINT32_C(0xC0000000), TEST_CPSR_N | TEST_CPSR_C, 2u },
        { "register ROR count 255 repeats count thirty-one",
          UINT32_C(0xE1B02170), UINT32_C(0x80000001), 255u, cv,
          UINT32_C(3), 0u, 2u },
        { "register ROR count 256 uses low eight zero bits",
          UINT32_C(0xE1B02170), UINT32_C(0x80000001), 256u, cv,
          UINT32_C(0x80000001), TEST_CPSR_N | TEST_CPSR_C, 2u }
    };
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;
    size_t index;

    make_program(image);
    loaded = machine_load(&machine, image);
    for (index = 0u; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        const arm_operand2_test_vector *vector = &vectors[index];
        const uint32_t sentinel = UINT32_C(0xA5A5A5A5);
        const uint64_t cycles_before = machine.master_cycles;
        const uint64_t retired_before = machine.cpu.retired_instructions;
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool ran = false;

        machine.cpu.r0_7[0] = vector->input_value;
        machine.cpu.r0_7[1] = vector->register_count;
        machine.cpu.r0_7[2] = sentinel;
        if (loaded) {
            ran = run_arm_instruction(&machine, vector->opcode,
                                      vector->initial_flags,
                                      vector->expected_cycles, &result);
        }
        test_case(loaded && ran &&
                      result.termination == GBL_RUN_PROGRESS &&
                      result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      result.instructions == 1u &&
                      result.cycles == vector->expected_cycles &&
                      machine.master_cycles ==
                          cycles_before + vector->expected_cycles &&
                      machine.cpu.retired_instructions == retired_before + 1u &&
                      machine.cpu.r0_7[0] == vector->input_value &&
                      machine.cpu.r0_7[1] == vector->register_count &&
                      machine.cpu.r0_7[2] == vector->expected_value &&
                      (machine.cpu.cpsr & nzc) == vector->expected_nzc &&
                      (machine.cpu.cpsr & TEST_CPSR_V) ==
                          (vector->initial_flags & TEST_CPSR_V),
                  vector->description);
    }
    gbl_machine_dispose(&machine);
}

typedef struct arm_condition_test_vector {
    uint8_t condition;
    uint32_t passing_flags;
    uint32_t failing_flags;
    const char *passing_description;
    const char *failing_description;
} arm_condition_test_vector;

static void arm_condition_case(gbl_machine *machine, bool loaded,
                               uint8_t condition, uint32_t flags,
                               bool should_execute,
                               const char *description)
{
    const uint32_t address = UINT32_C(0x08000000);
    const uint32_t source = UINT32_C(0x2468ACE0);
    const uint32_t sentinel = UINT32_C(0xA55AA55A);
    const uint64_t cycles_before = machine->master_cycles;
    const uint64_t retired_before = machine->cpu.retired_instructions;
    const uint32_t opcode = ((uint32_t)condition << 28u) |
                            UINT32_C(0x01A02001);
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool ran = false;

    machine->cpu.r0_7[0] = UINT32_C(0x11223344);
    machine->cpu.r0_7[1] = source;
    machine->cpu.r0_7[2] = sentinel;
    if (loaded) {
        ran = run_arm_instruction(machine, opcode, flags, 1u, &result);
    }
    test_case(loaded && ran &&
                  result.termination == GBL_RUN_PROGRESS &&
                  result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                  result.instructions == 1u && result.cycles == 1u &&
                  machine->master_cycles == cycles_before + 1u &&
                  machine->cpu.retired_instructions == retired_before + 1u &&
                  machine->cpu.execution_address == address + UINT32_C(4) &&
                  machine->cpu.pipeline[0].address == address + UINT32_C(4) &&
                  machine->cpu.pipeline[1].address == address + UINT32_C(8) &&
                  machine->cpu.r0_7[0] == UINT32_C(0x11223344) &&
                  machine->cpu.r0_7[1] == source &&
                  machine->cpu.r0_7[2] ==
                      (should_execute ? source : sentinel) &&
                  (machine->cpu.cpsr &
                   (TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V)) ==
                      flags,
              description);
}

static void test_arm_condition_predicates(void)
{
    static const arm_condition_test_vector vectors[] = {
        { 0u, TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C,
          TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          "EQ executes when Z is set", "EQ retires without effects when Z is clear" },
        { 1u, TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C,
          "NE executes when Z is clear", "NE retires without effects when Z is set" },
        { 2u, TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_V,
          "CS executes when C is set", "CS retires without effects when C is clear" },
        { 3u, TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_V,
          TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          "CC executes when C is clear", "CC retires without effects when C is set" },
        { 4u, TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          "MI executes when N is set", "MI retires without effects when N is clear" },
        { 5u, TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          "PL executes when N is clear", "PL retires without effects when N is set" },
        { 6u, TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C,
          "VS executes when V is set", "VS retires without effects when V is clear" },
        { 7u, TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C,
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_V,
          "VC executes when V is clear", "VC retires without effects when V is set" },
        { 8u, TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          "HI executes when C is set and Z is clear",
          "HI retires without effects when Z is set" },
        { 9u, TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          "LS executes when C is clear",
          "LS retires without effects when C is set and Z is clear" },
        { 10u, TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C,
          "GE executes when N equals V", "GE retires without effects when N differs from V" },
        { 11u, TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C,
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          "LT executes when N differs from V", "LT retires without effects when N equals V" },
        { 12u, TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          "GT executes when Z is clear and N equals V",
          "GT retires without effects when Z is set" },
        { 13u, TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          "LE executes when Z is set", "LE retires without effects when Z is clear and N equals V" }
    };
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;
    size_t index;

    make_program(image);
    loaded = machine_load(&machine, image);
    for (index = 0u; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        arm_condition_case(&machine, loaded, vectors[index].condition,
                           vectors[index].passing_flags, true,
                           vectors[index].passing_description);
        arm_condition_case(&machine, loaded, vectors[index].condition,
                           vectors[index].failing_flags, false,
                           vectors[index].failing_description);
    }
    gbl_machine_dispose(&machine);
}

static void test_arm_al_ignores_nzcv(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;

    make_program(image);
    loaded = machine_load(&machine, image);
    arm_condition_case(&machine, loaded, 14u,
                       TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
                       true, "AL executes when all NZCV flags are set");
    arm_condition_case(&machine, loaded, 14u, 0u, true,
                       "AL executes when all NZCV flags are clear");
    gbl_machine_dispose(&machine);
}

static void test_arm_condition_1111_is_unpredictable(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_machine before;
    gbl_machine expected;
    gbl_trace_record records[2];
    gbl_trace_record records_before[2];
    gbl_trace_buffer trace;
    gbl_trace_buffer trace_before;
    gbl_run_options options;
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    gbl_run_status status = GBL_RUN_STATUS_INVALID_ARGUMENT;

    make_program(image);
    loaded = machine_load(&machine, image);
    memset(records, 0, sizeof(records));
    records[0].guest_cycle = UINT64_C(17);
    records[0].address = UINT32_C(0x03000040);
    records[1].guest_cycle = UINT64_C(19);
    records[1].address = UINT32_C(0x03000044);
    memcpy(records_before, records, sizeof(records));
    trace = (gbl_trace_buffer){ records, 2u, 1u, UINT64_C(23) };
    trace_before = trace;

    if (loaded) {
        machine.cpu.execution_address = UINT32_C(0x08000000);
        machine.cpu.cpsr = TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C |
                           TEST_CPSR_V | TEST_CPSR_SYSTEM;
        machine.cpu.r0_7[1] = UINT32_C(0x7FFFFFFF);
        machine.cpu.r0_7[2] = UINT32_C(0xA5A5A5A5);
        machine.cpu.r0_7[3] = UINT32_C(1);
        machine.cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
            UINT32_C(0x08000000), UINT32_C(0xF0912003), 4u, true
        };
        machine.cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
            UINT32_C(0x08000004), UINT32_C(0xE1A00000), 4u, true
        };
        machine.cpu.next_fetch_sequential = true;
        machine.cpu.stopped = false;
        machine.cpu.stop_reason = GBL_CPU_STOP_NONE;
        before = machine;
        expected = before;
        expected.cpu.stopped = true;
        expected.cpu.stop_reason = GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
        options = (gbl_run_options){
            .cycle_budget = 1u,
            .unit_limit = 1u,
            .category_mask = GBL_TRACE_CATEGORY_MASK,
            .trace_buffer = &trace
        };
        status = gbl_cpu_run(&machine, &options, &result);
    } else {
        before = machine;
        expected = machine;
    }
    test_case(loaded && status == GBL_RUN_STATUS_OK &&
                  result.termination == GBL_RUN_STOPPED &&
                  result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
                  result.instructions == 0u && result.cycles == 0u &&
                  gbl_test_canonical_equal(&machine, &expected) &&
                  gbl_test_same_instance_identity_equal(&before, &machine) &&
                  trace.records == trace_before.records &&
                  trace.capacity == trace_before.capacity &&
                  trace.count == trace_before.count &&
                  trace.next_sequence == trace_before.next_sequence &&
                  gbl_test_bytes_equal((const uint8_t *)records,
                                       (const uint8_t *)records_before,
                                       sizeof(records)),
              "ARM condition 1111 is refused as unpredictable before effects");
    gbl_machine_dispose(&machine);
}

static void test_arm_register_shift_cycle_preflight(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_machine before;
    gbl_trace_record records[3];
    gbl_trace_record records_before[3];
    gbl_trace_buffer trace;
    gbl_trace_buffer trace_before;
    gbl_run_options options;
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    gbl_run_status status = GBL_RUN_STATUS_INVALID_ARGUMENT;

    make_program(image);
    loaded = machine_load(&machine, image);
    memset(records, 0, sizeof(records));
    records[0].guest_cycle = UINT64_C(31);
    records[0].address = UINT32_C(0x03000050);
    memcpy(records_before, records, sizeof(records));
    trace = (gbl_trace_buffer){ records, 3u, 1u, UINT64_C(41) };
    trace_before = trace;
    if (loaded) {
        machine.cpu.execution_address = UINT32_C(0x08000000);
        machine.cpu.cpsr = TEST_CPSR_C | TEST_CPSR_V | TEST_CPSR_SYSTEM;
        machine.cpu.r0_7[0] = UINT32_C(0x80000001);
        machine.cpu.r0_7[1] = UINT32_C(1);
        machine.cpu.r0_7[2] = UINT32_C(0xA5A5A5A5);
        machine.cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
            UINT32_C(0x08000000), UINT32_C(0xE1B02110), 4u, true
        };
        machine.cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
            UINT32_C(0x08000004), UINT32_C(0xE1A00000), 4u, true
        };
        machine.cpu.next_fetch_sequential = true;
        machine.cpu.stopped = false;
        machine.cpu.stop_reason = GBL_CPU_STOP_NONE;
        before = machine;
        options = (gbl_run_options){
            .cycle_budget = 1u,
            .unit_limit = 1u,
            .category_mask = GBL_TRACE_CATEGORY_MASK,
            .trace_buffer = &trace
        };
        status = gbl_cpu_run(&machine, &options, &result);
    } else {
        before = machine;
    }
    test_case(loaded && status == GBL_RUN_STATUS_OK &&
                  result.termination == GBL_RUN_TIMEOUT &&
                  result.reason == GBL_RUN_REASON_CYCLE_BUDGET &&
                  result.instructions == 0u && result.cycles == 0u &&
                  gbl_test_canonical_equal(&before, &machine) &&
                  gbl_test_same_instance_identity_equal(&before, &machine) &&
                  trace.records == trace_before.records &&
                  trace.capacity == trace_before.capacity &&
                  trace.count == trace_before.count &&
                  trace.next_sequence == trace_before.next_sequence &&
                  gbl_test_bytes_equal((const uint8_t *)records,
                                       (const uint8_t *)records_before,
                                       sizeof(records)),
              "one-short register-shift budget refuses the whole unit and trace");

    if (loaded) {
        const uint64_t cycles_before = machine.master_cycles;
        const uint64_t retired_before = machine.cpu.retired_instructions;

        options.cycle_budget = 2u;
        result = (gbl_run_result){
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        status = gbl_cpu_run(&machine, &options, &result);
        test_case(status == GBL_RUN_STATUS_OK &&
                      result.termination == GBL_RUN_PROGRESS &&
                      result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      result.instructions == 1u && result.cycles == 2u &&
                      machine.master_cycles == cycles_before + 2u &&
                      machine.cpu.retired_instructions == retired_before + 1u &&
                      machine.cpu.r0_7[2] == UINT32_C(2) &&
                      (machine.cpu.cpsr & (TEST_CPSR_N | TEST_CPSR_Z |
                                           TEST_CPSR_C | TEST_CPSR_V)) ==
                          (TEST_CPSR_C | TEST_CPSR_V) &&
                      trace.count == 3u &&
                      trace.next_sequence == trace_before.next_sequence + 2u,
                  "exact register-shift budget charges one internal bus cycle");
    } else {
        test_case(false,
                  "exact register-shift budget charges one internal bus cycle");
    }
    gbl_machine_dispose(&machine);
}

static bool run_arm_program_unit(gbl_machine *machine, uint64_t cycle_budget,
                                 gbl_run_result *result)
{
    return run_units(machine, cycle_budget, 1u, result);
}

static void test_arm_transfer_preindexed_writeback(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran = false;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = UINT32_C(0x03000040);
        machine.cpu.r0_7[1] = UINT32_C(0xA1B2C3D4);
        ran = run_arm_instruction(&machine, UINT32_C(0xE5A01004),
                                  TEST_CPSR_SYSTEM, UINT64_C(8), &result);
    }

    test_case(loaded && ran &&
                  machine.iwram[0x44u] == UINT8_C(0xD4) &&
                  machine.iwram[0x45u] == UINT8_C(0xC3) &&
                  machine.iwram[0x46u] == UINT8_C(0xB2) &&
                  machine.iwram[0x47u] == UINT8_C(0xA1) &&
                  machine.cpu.r0_7[0] == UINT32_C(0x03000044),
              "ARM STR pre-indexed writeback stores at adjusted address and updates base");
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
}

typedef struct arm_subword_address_case {
    const char *name;
    bool register_offset;
    bool preindexed;
    bool add_offset;
    bool writeback;
    uint16_t offset;
} arm_subword_address_case;

static const arm_subword_address_case arm_subword_address_cases[] = {
    { "immediate offset", false, true, true, false, UINT16_C(0xA4) },
    { "immediate pre-index", false, true, false, true, UINT16_C(0xA4) },
    { "immediate post-index", false, false, true, false, UINT16_C(0xA4) },
    { "register offset", true, true, true, false, UINT16_C(0x24) },
    { "register pre-index", true, true, false, true, UINT16_C(0x24) },
    { "register post-index", true, false, false, false, UINT16_C(0x24) }
};

static bool arm_subword_form_case(uint8_t transfer_kind,
                                  const arm_subword_address_case *address_case)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_trace_record records[4] = { 0 };
    gbl_trace_buffer trace = { records, 4u, 0u, UINT64_C(40) };
    gbl_run_result result = { 0 };
    const uint32_t initial_base = UINT32_C(0x03000400);
    const uint32_t offset = address_case->offset;
    const uint32_t updated_base = address_case->add_offset
        ? initial_base + offset
        : initial_base - offset;
    const uint32_t access_address = address_case->preindexed
        ? updated_base
        : initial_base;
    const bool writeback = !address_case->preindexed ||
                           address_case->writeback;
    const uint32_t expected_base = writeback ? updated_base : initial_base;
    const bool load = transfer_kind != 1u;
    const bool sign = transfer_kind >= 2u;
    const bool halfword = transfer_kind != 2u;
    const uint8_t width = halfword ? 2u : 1u;
    const uint32_t raw_value = transfer_kind == 0u
        ? UINT32_C(0x00001234)
        : transfer_kind == 2u ? UINT32_C(0x00000080)
        : transfer_kind == 3u ? UINT32_C(0x00008001)
                              : 0u;
    const uint32_t expected_result = transfer_kind == 0u
        ? UINT32_C(0x00001234)
        : transfer_kind == 2u ? UINT32_C(0xFFFFFF80)
        : transfer_kind == 3u ? UINT32_C(0xFFFF8001)
                              : 0u;
    const uint32_t source_value = UINT32_C(0xA1B2C3D4);
    uint32_t opcode;
    size_t index;
    bool saw_data = false;
    bool observed;

    make_empty_image(image);
    if (transfer_kind == 1u) {
        memset(image, 0, sizeof(image));
    }
    if (!machine_load(&machine, image)) {
        return false;
    }
    machine.cpu.r0_7[0] = initial_base;
    machine.cpu.r0_7[1] = offset;
    machine.cpu.r0_7[2] = source_value;
    if (transfer_kind == 0u) {
        machine.iwram[access_address - UINT32_C(0x03000000)] = UINT8_C(0x34);
        machine.iwram[access_address - UINT32_C(0x03000000) + 1u] =
            UINT8_C(0x12);
    } else if (transfer_kind == 1u) {
        machine.iwram[access_address - UINT32_C(0x03000000)] = UINT8_C(0xEE);
        machine.iwram[access_address - UINT32_C(0x03000000) + 1u] =
            UINT8_C(0xEE);
    } else if (transfer_kind == 2u) {
        machine.iwram[access_address - UINT32_C(0x03000000)] = UINT8_C(0x80);
    } else {
        machine.iwram[access_address - UINT32_C(0x03000000)] = UINT8_C(0x01);
        machine.iwram[access_address - UINT32_C(0x03000000) + 1u] =
            UINT8_C(0x80);
    }
    opcode = encode_arm_subword(
        address_case->register_offset, address_case->preindexed,
        address_case->add_offset, address_case->writeback, load, sign,
        halfword, 0u, 2u, address_case->register_offset
                             ? UINT16_C(1)
                             : address_case->offset);
    observed = run_arm_instruction_with_trace(
        &machine, opcode, TEST_CPSR_SYSTEM, UINT64_C(3), &trace, &result) &&
        result.termination == GBL_RUN_PROGRESS &&
        result.cycles == UINT64_C(3) &&
        result.instructions == UINT64_C(1) && machine.master_cycles == 3u &&
        machine.cpu.r0_7[0] == expected_base &&
        (load ? machine.cpu.r0_7[2] == expected_result
              : machine.iwram[access_address - UINT32_C(0x03000000)] ==
                    UINT8_C(0xC3) &&
                    machine.iwram[access_address - UINT32_C(0x03000000) + 1u]
                        == UINT8_C(0xD4));
    for (index = 0u; index < trace.count; ++index) {
        if (records[index].category == GBL_TRACE_BUS &&
            records[index].address == access_address &&
            records[index].width == width &&
            (!load || records[index].value == raw_value)) {
            saw_data = true;
        }
    }
    observed = observed && saw_data && trace.count == 4u &&
               trace.next_sequence == UINT64_C(44);
    gbl_machine_dispose(&machine);
    return observed;
}

static bool arm_subword_refusal_is_atomic(uint32_t opcode)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_machine before = { 0 };
    gbl_machine expected = { 0 };
    gbl_trace_record records[4] = { 0 };
    gbl_trace_record records_before[4];
    gbl_trace_buffer trace = { records, 4u, 1u, UINT64_C(71) };
    const gbl_run_options options = {
        .cycle_budget = UINT64_C(8),
        .unit_limit = 1u,
        .category_mask = GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS,
        .trace_buffer = &trace
    };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran = false;
    bool atomic = false;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    records[0] = (gbl_trace_record){
        .sequence = UINT64_C(70),
        .guest_cycle = UINT64_C(19),
        .category = GBL_TRACE_BUS,
        .instruction_address = UINT32_C(0x08000080),
        .opcode = UINT32_C(0xE1A00000),
        .address = UINT32_C(0x03000180),
        .value = UINT32_C(0xAABBCCDD),
        .width = 4u,
        .access_class = GBL_BUS_CLASS_SEQUENTIAL,
        .bus_outcome = GBL_BUS_OUTCOME_RAM
    };
    if (loaded) {
        gbl_machine_reset(&machine);
        machine.cpu.r0_7[0] = UINT32_C(0x03000100);
        machine.cpu.r0_7[1] = UINT32_C(4);
        machine.cpu.r0_7[2] = UINT32_C(0xA1B2C3D4);
        write_word(machine.iwram, 0x100u, UINT32_C(0x11223344));
        prepare_arm_instruction(&machine, opcode, TEST_CPSR_SYSTEM);
        before = machine;
        expected = before;
        memcpy(records_before, records, sizeof(records_before));
        ran = gbl_cpu_run(&machine, &options, &result) ==
              GBL_RUN_STATUS_OK;
        expected.cpu.stopped = true;
        expected.cpu.stop_reason = GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
        atomic = ran && result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
            gbl_test_canonical_equal(&expected, &machine) &&
            gbl_test_same_instance_identity_equal(&before, &machine) &&
            trace.count == 1u && trace.next_sequence == UINT64_C(71) &&
            memcmp(records, records_before, sizeof(records)) == 0;
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
    return loaded && atomic;
}

static void test_arm_subword_address_matrix(void)
{
    static const char *const transfer_names[] = {
        "LDRH", "STRH", "LDRSB", "LDRSH"
    };
    uint8_t transfer_kind;
    size_t address_index;

    for (transfer_kind = 0u; transfer_kind < 4u; ++transfer_kind) {
        for (address_index = 0u;
             address_index < sizeof(arm_subword_address_cases) /
                                 sizeof(arm_subword_address_cases[0]);
             ++address_index) {
            char description[128];
            const bool passed = arm_subword_form_case(
                transfer_kind, &arm_subword_address_cases[address_index]);

            (void)snprintf(
                description, sizeof(description),
                "ARM %s executes %s with correct data, address and writeback",
                transfer_names[transfer_kind],
                arm_subword_address_cases[address_index].name);
            test_case(passed, description);
        }
    }
}

static void test_arm_subword_pc_and_alias_rules(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran;

    make_empty_image(image);
    write_halfword(image, 0x08u, UINT16_C(0x3456));
    image[0x18u] = UINT8_C(0x80);
    loaded = machine_load(&machine, image);
    ran = loaded && run_arm_instruction(
        &machine,
        encode_arm_subword(false, true, true, false, true, false, true,
                           15u, 1u, 0u),
        TEST_CPSR_SYSTEM, UINT64_C(3), &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  machine.cpu.r0_7[1] == UINT32_C(0x00003456),
              "ARM LDRH permits PC-relative immediate offset using PC+8");
    if (loaded) {
        gbl_machine_dispose(&machine);
    }

    machine = (gbl_machine){ 0 };
    loaded = machine_load(&machine, image);
    machine.cpu.r0_7[2] = UINT32_C(0x10);
    ran = loaded && run_arm_instruction(
        &machine,
        encode_arm_subword(true, true, true, false, true, true, false,
                           15u, 1u, 2u),
        TEST_CPSR_SYSTEM, UINT64_C(3), &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  machine.cpu.r0_7[1] == UINT32_C(0xFFFFFF80),
              "ARM LDRSB permits PC-relative register offset using PC+8");
    if (loaded) {
        gbl_machine_dispose(&machine);
    }

    machine = (gbl_machine){ 0 };
    make_empty_image(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = UINT32_C(0x03000100);
        machine.iwram[0x100u] = UINT8_C(0x34);
        machine.iwram[0x101u] = UINT8_C(0x12);
        ran = run_arm_instruction(
            &machine,
            encode_arm_subword(false, true, true, false, true, false, true,
                               0u, 0u, 0u),
            TEST_CPSR_SYSTEM, UINT64_C(3), &result);
    } else {
        ran = false;
    }
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  machine.cpu.r0_7[0] == UINT32_C(0x00001234),
              "ARM LDRH permits Rd=Rn when writeback is absent");
    if (loaded) {
        gbl_machine_dispose(&machine);
    }

    machine = (gbl_machine){ 0 };
    make_empty_image(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.open_bus = UINT32_C(0x00000080);
        machine.cpu.r0_7[0] = UINT32_C(0x00000040);
        ran = run_arm_instruction(
            &machine,
            encode_arm_subword(true, true, true, false, true, true, false,
                               0u, 0u, 0u),
            TEST_CPSR_SYSTEM, UINT64_C(3), &result);
    } else {
        ran = false;
    }
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  machine.cpu.r0_7[0] == UINT32_C(0xFFFFFF80),
              "ARM LDRSB permits Rn=Rm when register offset has no writeback");
    if (loaded) {
        gbl_machine_dispose(&machine);
    }

    machine = (gbl_machine){ 0 };
    make_empty_image(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = UINT32_C(0x03000100);
        ran = run_arm_instruction(
            &machine,
            encode_arm_subword(false, true, true, false, false, false, true,
                               0u, 0u, 0u),
            TEST_CPSR_SYSTEM, UINT64_C(3), &result);
    } else {
        ran = false;
    }
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  machine.iwram[0x100u] == UINT8_C(0x00) &&
                  machine.iwram[0x101u] == UINT8_C(0x01) &&
                  machine.cpu.r0_7[0] == UINT32_C(0x03000100),
              "ARM STRH permits Rd=Rn when writeback is absent");
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
}

static void test_arm_subword_architectural_refusals(void)
{
    static const struct {
        uint32_t opcode;
        const char *description;
    } cases[] = {
        { 0u, "ARM LDRH refuses PC destination before any effect" },
        { 0u, "ARM LDRSB refuses PC destination before any effect" },
        { 0u, "ARM LDRSH refuses PC destination before any effect" },
        { 0u, "ARM STRH refuses PC source before any effect" },
        { 0u, "ARM LDRH refuses writeback when Rd equals Rn" },
        { 0u, "ARM LDRSB refuses writeback when Rd equals Rn" },
        { 0u, "ARM LDRSH refuses writeback when Rd equals Rn" },
        { 0u, "ARM STRH refuses writeback when Rd equals Rn" },
        { 0u, "ARM Mode 3 register offset refuses Rm=PC" },
        { 0u, "ARM Mode 3 pre-index refuses PC base with writeback" },
        { 0u, "ARM Mode 3 post-index refuses PC base" },
        { 0u, "ARM Mode 3 refuses P=0 with W=1" },
        { 0u, "ARMv4T Mode 3 pre-index writeback refuses Rn=Rm" },
        { 0u, "ARMv4T Mode 3 post-index refuses Rn=Rm" },
        { 0u, "ARM Mode 3 register offset refuses nonzero SBZ field" },
        { 0u, "ARMv4T does not execute later-version LDRD encoding" },
        { 0u, "ARMv4T does not execute later-version STRD encoding" }
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint32_t opcode = 0u;

        switch (index) {
        case 0u:
            opcode = encode_arm_subword(false, true, true, false, true,
                                         false, true, 0u, 15u, 0u);
            break;
        case 1u:
            opcode = encode_arm_subword(false, true, true, false, true,
                                         true, false, 0u, 15u, 0u);
            break;
        case 2u:
            opcode = encode_arm_subword(false, true, true, false, true,
                                         true, true, 0u, 15u, 0u);
            break;
        case 3u:
            opcode = encode_arm_subword(false, true, true, false, false,
                                         false, true, 0u, 15u, 0u);
            break;
        case 4u:
            opcode = encode_arm_subword(false, true, true, true, true,
                                         false, true, 0u, 0u, 4u);
            break;
        case 5u:
            opcode = encode_arm_subword(false, true, true, true, true,
                                         true, false, 0u, 0u, 4u);
            break;
        case 6u:
            opcode = encode_arm_subword(false, true, true, true, true,
                                         true, true, 0u, 0u, 4u);
            break;
        case 7u:
            opcode = encode_arm_subword(false, true, true, true, false,
                                         false, true, 0u, 0u, 4u);
            break;
        case 8u:
            opcode = encode_arm_subword(true, true, true, false, true,
                                         false, true, 0u, 2u, 15u);
            break;
        case 9u:
            opcode = encode_arm_subword(false, true, true, true, true,
                                         false, true, 15u, 2u, 0u);
            break;
        case 10u:
            opcode = encode_arm_subword(true, false, true, false, true,
                                         false, true, 15u, 2u, 1u);
            break;
        case 11u:
            opcode = encode_arm_subword(false, false, true, true, true,
                                         false, true, 0u, 2u, 4u);
            break;
        case 12u:
            opcode = encode_arm_subword(true, true, true, true, true,
                                         false, true, 0u, 2u, 0u);
            break;
        case 13u:
            opcode = encode_arm_subword(true, false, true, false, true,
                                         false, true, 0u, 2u, 0u);
            break;
        case 14u:
            opcode = encode_arm_subword(true, true, true, false, true,
                                         false, true, 0u, 2u, 4u) |
                     (UINT32_C(1) << 8u);
            break;
        case 15u:
            opcode = encode_arm_subword(false, true, true, false, false,
                                         true, false, 0u, 2u, 4u);
            break;
        default:
            opcode = encode_arm_subword(false, true, true, false, false,
                                         true, true, 0u, 2u, 4u);
            break;
        }
        test_case(arm_subword_refusal_is_atomic(opcode),
                  cases[index].description);
    }
}

static void test_arm_transfer_immediate_matrix(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;
    uint8_t preindexed;
    uint8_t add_offset;
    uint8_t byte;
    uint8_t writeback;
    uint8_t load;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    for (preindexed = 0u; preindexed < 2u; ++preindexed) {
        for (add_offset = 0u; add_offset < 2u; ++add_offset) {
            for (byte = 0u; byte < 2u; ++byte) {
                for (writeback = 0u; writeback < 2u; ++writeback) {
                    for (load = 0u; load < 2u; ++load) {
                        const uint32_t initial_base = UINT32_C(0x03000100);
                        const uint32_t offset = UINT32_C(4);
                        const uint32_t adjusted = add_offset != 0u
                            ? initial_base + offset
                            : initial_base - offset;
                        const uint32_t address = preindexed != 0u
                            ? adjusted
                            : initial_base;
                        const uint32_t request_address = byte != 0u
                            ? address
                            : address & ~UINT32_C(3);
                        const size_t data_offset =
                            (size_t)(request_address - UINT32_C(0x03000000));
                        const bool update_base = preindexed == 0u ||
                                                 writeback != 0u;
                        const uint32_t expected_base = update_base
                            ? adjusted
                            : initial_base;
                        const uint32_t opcode = encode_arm_transfer(
                            false, preindexed != 0u, add_offset != 0u,
                            byte != 0u, writeback != 0u, load != 0u, 0u, 2u,
                            (uint16_t)offset);
                        gbl_run_result result = { 0 };
                        bool ran = false;
                        bool observed;
                        char description[160];

                        if (loaded) {
                            gbl_machine_reset(&machine);
                            memset(machine.iwram + 0x100u, 0xA7, 0x20u);
                            machine.cpu.r0_7[0] = initial_base;
                            machine.cpu.r0_7[1] = UINT32_C(0x76543210);
                            machine.cpu.r0_7[2] = UINT32_C(0xA1B2C3D4);
                            if (load != 0u) {
                                if (byte != 0u) {
                                    machine.iwram[data_offset] = UINT8_C(0x5A);
                                } else {
                                    write_word(machine.iwram, data_offset,
                                               UINT32_C(0x11223344));
                                }
                            }
                            ran = run_arm_instruction(
                                &machine, opcode, TEST_CPSR_SYSTEM,
                                UINT64_C(8), &result);
                        }
                        observed = loaded && ran &&
                            result.termination == GBL_RUN_PROGRESS &&
                            result.instructions == 1u &&
                            machine.cpu.r0_7[0] == expected_base;
                        if (load != 0u) {
                            observed = observed &&
                                machine.cpu.r0_7[2] ==
                                    (byte != 0u ? UINT32_C(0x5A)
                                                : UINT32_C(0x11223344));
                        } else if (byte != 0u) {
                            observed = observed &&
                                machine.iwram[data_offset] == UINT8_C(0xD4);
                        } else {
                            observed = observed &&
                                read_test_word(machine.iwram, data_offset) ==
                                    UINT32_C(0xA1B2C3D4);
                        }
                        (void)snprintf(
                            description, sizeof(description),
                            "ARM %s%s immediate P%u U%u W%u executes with %s",
                            load != 0u ? "LDR" : "STR",
                            byte != 0u ? "B" : "", preindexed,
                            add_offset, writeback,
                            !preindexed && writeback ? "force-user post-index"
                                                     : "architectural writeback");
                        test_case(observed, description);
                    }
                }
            }
        }
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
}

static void test_arm_transfer_scaled_register_matrix(void)
{
    static const struct {
        const char *name;
        uint8_t shift;
        uint8_t amount;
        uint32_t source;
        uint32_t offset;
        bool carry;
    } vectors[] = {
        { "LSL #0", 0u, 0u, UINT32_C(3), UINT32_C(3), false },
        { "LSL #31", 0u, 31u, UINT32_C(1), UINT32_C(0x80000000), false },
        { "LSR #1", 1u, 1u, UINT32_C(0x60000000), UINT32_C(0x30000000), false },
        { "LSR #32", 1u, 0u, UINT32_MAX, 0u, false },
        { "ASR #1", 2u, 1u, UINT32_C(0xFFFFFFFE), UINT32_MAX, false },
        { "ASR #32", 2u, 0u, UINT32_C(0x80000000), UINT32_MAX, false },
        { "ROR #1", 3u, 1u, UINT32_C(0x80000001), UINT32_C(0xC0000000), false },
        { "ROR #31", 3u, 31u, UINT32_C(0x80000000), UINT32_C(1), false },
        { "RRX", 3u, 0u, UINT32_C(2), UINT32_C(0x80000001), true }
    };
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;
    size_t vector_index;
    uint8_t preindexed;
    uint8_t add_offset;
    uint8_t byte;
    uint8_t writeback;
    uint8_t load;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    for (vector_index = 0u;
         vector_index < sizeof(vectors) / sizeof(vectors[0]);
         ++vector_index) {
        const uint32_t target = UINT32_C(0x03001000);
        const uint16_t shift_spec =
            (uint16_t)(((uint32_t)vectors[vector_index].amount << 7u) |
                       ((uint32_t)vectors[vector_index].shift << 5u) |
                       UINT32_C(1));

        for (preindexed = 0u; preindexed < 2u; ++preindexed) {
            for (add_offset = 0u; add_offset < 2u; ++add_offset) {
                for (byte = 0u; byte < 2u; ++byte) {
                    for (writeback = 0u; writeback < 2u; ++writeback) {
                        for (load = 0u; load < 2u; ++load) {
                            const uint32_t initial_base =
                                preindexed != 0u
                                    ? (add_offset != 0u
                                           ? target - vectors[vector_index].offset
                                           : target + vectors[vector_index].offset)
                                    : target;
                            const uint32_t adjusted = add_offset != 0u
                                ? initial_base + vectors[vector_index].offset
                                : initial_base - vectors[vector_index].offset;
                            const uint32_t address = preindexed != 0u
                                ? target
                                : initial_base;
                            const size_t data_offset =
                                (size_t)(address - UINT32_C(0x03000000));
                            const bool update_base = preindexed == 0u ||
                                                     writeback != 0u;
                            const uint32_t expected_base = update_base
                                ? adjusted
                                : initial_base;
                            const uint32_t opcode = encode_arm_transfer(
                                true, preindexed != 0u, add_offset != 0u,
                                byte != 0u, writeback != 0u, load != 0u,
                                0u, 2u, shift_spec);
                            const uint32_t cpsr = TEST_CPSR_SYSTEM |
                                (vectors[vector_index].carry
                                     ? TEST_CPSR_C
                                     : 0u);
                            gbl_run_result result = { 0 };
                            bool ran = false;
                            bool observed;
                            char description[176];

                            if (loaded) {
                                gbl_machine_reset(&machine);
                                memset(machine.iwram + 0x1000u, 0xA7, 0x20u);
                                machine.cpu.r0_7[0] = initial_base;
                                machine.cpu.r0_7[1] =
                                    vectors[vector_index].source;
                                machine.cpu.r0_7[2] = UINT32_C(0xA1B2C3D4);
                                if (load != 0u) {
                                    if (byte != 0u) {
                                        machine.iwram[data_offset] =
                                            UINT8_C(0x5A);
                                    } else {
                                        write_word(machine.iwram, data_offset,
                                                   UINT32_C(0x11223344));
                                    }
                                }
                                ran = run_arm_instruction(
                                    &machine, opcode, cpsr, UINT64_C(8),
                                    &result);
                            }
                            observed = loaded && ran &&
                                result.termination == GBL_RUN_PROGRESS &&
                                result.instructions == 1u &&
                                machine.cpu.r0_7[0] == expected_base;
                            if (load != 0u) {
                                observed = observed &&
                                    machine.cpu.r0_7[2] ==
                                        (byte != 0u ? UINT32_C(0x5A)
                                                    : UINT32_C(0x11223344));
                            } else if (byte != 0u) {
                                observed = observed &&
                                    machine.iwram[data_offset] == UINT8_C(0xD4);
                            } else {
                                observed = observed &&
                                    read_test_word(machine.iwram, data_offset) ==
                                        UINT32_C(0xA1B2C3D4);
                            }
                            (void)snprintf(
                                description, sizeof(description),
                                "ARM %s%s scaled-register %s P%u U%u W%u",
                                load != 0u ? "LDR" : "STR",
                                byte != 0u ? "B" : "",
                                vectors[vector_index].name, preindexed,
                                add_offset, writeback);
                            test_case(observed, description);
                        }
                    }
                }
            }
        }
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
}

static void test_arm_transfer_word_and_byte_lanes(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;
    uint8_t lane;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    for (lane = 0u; lane < 4u; ++lane) {
        uint8_t operation;

        for (operation = 0u; operation < 4u; ++operation) {
            const bool byte = (operation & 1u) != 0u;
            const bool load = operation < 2u;
            const uint32_t address = UINT32_C(0x03000200) + lane;
            const size_t aligned_offset = 0x200u;
            const uint32_t raw_word = UINT32_C(0x11223344);
            const uint32_t rotated_word = lane == 0u
                ? raw_word
                : (raw_word >> ((uint32_t)lane * 8u)) |
                      (raw_word << (32u - (uint32_t)lane * 8u));
            const uint32_t opcode = encode_arm_transfer(
                false, true, true, byte, false, load, 0u, 2u, 0u);
            gbl_run_result result = { 0 };
            bool ran = false;
            bool observed;
            char description[144];

            if (loaded) {
                gbl_machine_reset(&machine);
                write_word(machine.iwram, aligned_offset, raw_word);
                machine.cpu.r0_7[0] = address;
                machine.cpu.r0_7[2] = UINT32_C(0xA1B2C3D4);
                ran = run_arm_instruction(&machine, opcode, TEST_CPSR_SYSTEM,
                                          UINT64_C(8), &result);
            }
            observed = loaded && ran &&
                result.termination == GBL_RUN_PROGRESS &&
                result.instructions == 1u;
            if (load) {
                observed = observed &&
                    machine.cpu.r0_7[2] ==
                        (byte ? (uint32_t)machine.iwram[aligned_offset + lane]
                               : rotated_word);
            } else if (byte) {
                observed = observed &&
                    machine.iwram[aligned_offset + lane] == UINT8_C(0xD4) &&
                    (lane == 0u ||
                     machine.iwram[aligned_offset + lane - 1u] ==
                         (uint8_t)(raw_word >> (((uint32_t)lane - 1u) * 8u))) &&
                    (lane == 3u ||
                     machine.iwram[aligned_offset + lane + 1u] ==
                         (uint8_t)(raw_word >> (((uint32_t)lane + 1u) * 8u)));
            } else {
                observed = observed &&
                    read_test_word(machine.iwram, aligned_offset) ==
                        UINT32_C(0xA1B2C3D4);
            }
            (void)snprintf(description, sizeof(description),
                           "ARM %s%s transfer handles effective-address lane %u",
                           load ? "LDR" : "STR", byte ? "B" : "", lane);
            test_case(observed, description);
        }
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
}

static void test_arm_transfer_pc_contexts(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran;

    make_empty_image(image);
    image[7] = UINT8_C(0xA7);
    write_word(image, 0x0Cu, UINT32_C(0x55667788));
    write_word(image, 0x48u, UINT32_C(0xCAFEBABE));
    loaded = machine_load(&machine, image);

    ran = loaded && run_arm_instruction(
        &machine, UINT32_C(0xE59F2040), TEST_CPSR_SYSTEM, UINT64_C(8),
        &result);
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  machine.cpu.r0_7[2] == UINT32_C(0xCAFEBABE),
              "ARM word transfer uses PC+8 as the immediate base register");

    ran = loaded && run_arm_instruction(
        &machine, UINT32_C(0xE55F2001), TEST_CPSR_SYSTEM, UINT64_C(8),
        &result);
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  machine.cpu.r0_7[2] == UINT32_C(0xA7),
              "ARM byte transfer uses PC+8 with a subtracting immediate offset");

    machine.cpu.r0_7[1] = UINT32_C(4);
    ran = loaded && run_arm_instruction(
        &machine,
        encode_arm_transfer(true, true, true, false, false, true, 15u, 2u,
                            1u),
        TEST_CPSR_SYSTEM, UINT64_C(8), &result);
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  machine.cpu.r0_7[2] == UINT32_C(0x55667788),
              "ARM register-offset transfer permits PC base and reads its exact PC context");

    machine.cpu.r0_7[0] = UINT32_C(0x03000240);
    ran = loaded && run_arm_instruction(
        &machine, UINT32_C(0xE580F004), TEST_CPSR_SYSTEM, UINT64_C(8),
        &result);
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  read_test_word(machine.iwram, 0x244u) ==
                      UINT32_C(0x0800000C),
              "ARM STR PC stores the instruction-address-plus-twelve value");
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
}

static void test_arm_transfer_store_source_snapshot(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran = false;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = UINT32_C(0x03000300);
        ran = run_arm_instruction(
            &machine,
            encode_arm_transfer(false, true, true, false, false, false,
                                0u, 0u, 0u),
            TEST_CPSR_SYSTEM, UINT64_C(8), &result);
    }
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  read_test_word(machine.iwram, 0x300u) ==
                      UINT32_C(0x03000300) &&
                  machine.cpu.r0_7[0] == UINT32_C(0x03000300),
              "ARM STR permits Rd=Rn alias without base writeback");
    if (loaded) {
        machine.cpu.r0_7[0] = UINT32_C(0x03000305);
        ran = run_arm_instruction(
            &machine,
            encode_arm_transfer(false, true, true, true, false, false,
                                0u, 0u, 0u),
            TEST_CPSR_SYSTEM, UINT64_C(8), &result);
    }
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  machine.iwram[0x305u] == UINT8_C(0x05) &&
                  machine.cpu.r0_7[0] == UINT32_C(0x03000305),
              "ARM STRB permits Rd=Rn alias without base writeback");
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
}

static bool arm_transfer_unpredictable_is_atomic(uint32_t opcode)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_machine before = { 0 };
    gbl_machine expected = { 0 };
    gbl_trace_record records[4] = { 0 };
    gbl_trace_record records_before[4];
    gbl_trace_buffer trace = { records, 4u, 1u, UINT64_C(71) };
    const gbl_run_options options = {
        .cycle_budget = UINT64_C(8),
        .unit_limit = 1u,
        .category_mask = GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS,
        .trace_buffer = &trace
    };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran = false;
    bool atomic = false;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    records[0] = (gbl_trace_record){
        .sequence = UINT64_C(70),
        .guest_cycle = UINT64_C(19),
        .category = GBL_TRACE_BUS,
        .instruction_address = UINT32_C(0x08000080),
        .opcode = UINT32_C(0xE1A00000),
        .address = UINT32_C(0x03000180),
        .value = UINT32_C(0xAABBCCDD),
        .width = 4u,
        .access_class = GBL_BUS_CLASS_SEQUENTIAL,
        .bus_outcome = GBL_BUS_OUTCOME_RAM
    };
    if (loaded) {
        gbl_machine_reset(&machine);
        machine.cpu.r0_7[0] = UINT32_C(0x03000100);
        machine.cpu.r0_7[1] = UINT32_C(4);
        machine.cpu.r0_7[2] = UINT32_C(0xA1B2C3D4);
        write_word(machine.iwram, 0x100u, UINT32_C(0x11223344));
        prepare_arm_instruction(&machine, opcode, TEST_CPSR_SYSTEM);
        before = machine;
        expected = before;
        memcpy(records_before, records, sizeof(records_before));
        ran = gbl_cpu_run(&machine, &options, &result) ==
              GBL_RUN_STATUS_OK;
        expected.cpu.stopped = true;
        expected.cpu.stop_reason = GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
        atomic = ran && result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
            gbl_test_canonical_equal(&expected, &machine) &&
            gbl_test_same_instance_identity_equal(&before, &machine) &&
            trace.count == 1u && trace.next_sequence == UINT64_C(71) &&
            memcmp(records, records_before, sizeof(records)) == 0;
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
    return loaded && atomic;
}

static void test_arm_transfer_unpredictable_refusals(void)
{
    const uint32_t register_pc = encode_arm_transfer(
        true, true, true, false, false, true, 0u, 2u, UINT16_C(15));
    const uint32_t preindexed_word_store_alias = encode_arm_transfer(
        false, true, true, false, true, false, 0u, 0u, 4u);
    const uint32_t postindexed_word_store_alias = encode_arm_transfer(
        false, false, true, false, false, false, 0u, 0u, 4u);
    const uint32_t preindexed_byte_store_alias = encode_arm_transfer(
        false, true, true, true, true, false, 0u, 0u, 4u);
    const uint32_t postindexed_byte_store_alias = encode_arm_transfer(
        false, false, true, true, false, false, 0u, 0u, 4u);
    const uint32_t preindexed_alias = encode_arm_transfer(
        true, true, true, false, true, true, 0u, 2u, 0u);
    const uint32_t postindexed_alias = encode_arm_transfer(
        true, false, true, false, false, false, 0u, 2u, 0u);
    const uint32_t pc_writeback = encode_arm_transfer(
        false, true, true, false, true, false, 15u, 2u, 4u);
    const uint32_t byte_pc_destination = encode_arm_transfer(
        false, true, true, true, false, true, 0u, 15u, 0u);
    const uint32_t byte_store_pc_source = encode_arm_transfer(
        false, true, true, true, false, false, 0u, 15u, 0u);
    const uint32_t load_base_alias = encode_arm_transfer(
        false, true, true, false, true, true, 0u, 0u, 4u);

    test_case(arm_transfer_unpredictable_is_atomic(register_pc),
              "ARM single transfer refuses R15 as scaled-register offset atomically");
    test_case(arm_transfer_unpredictable_is_atomic(preindexed_word_store_alias),
              "ARM STR pre-indexed writeback refuses Rd=Rn atomically");
    test_case(arm_transfer_unpredictable_is_atomic(postindexed_word_store_alias),
              "ARM STR post-indexed writeback refuses Rd=Rn atomically");
    test_case(arm_transfer_unpredictable_is_atomic(preindexed_byte_store_alias),
              "ARM STRB pre-indexed writeback refuses Rd=Rn atomically");
    test_case(arm_transfer_unpredictable_is_atomic(postindexed_byte_store_alias),
              "ARM STRB post-indexed writeback refuses Rd=Rn atomically");
    test_case(arm_transfer_unpredictable_is_atomic(preindexed_alias),
              "ARMv4T pre-indexed writeback refuses Rn=Rm atomically");
    test_case(arm_transfer_unpredictable_is_atomic(postindexed_alias),
              "ARMv4T post-indexed register transfer refuses Rn=Rm atomically");
    test_case(arm_transfer_unpredictable_is_atomic(pc_writeback),
              "ARM single transfer refuses PC base writeback atomically");
    test_case(arm_transfer_unpredictable_is_atomic(byte_pc_destination),
              "ARM LDRB to PC refuses architecturally unpredictable destination atomically");
    test_case(arm_transfer_unpredictable_is_atomic(byte_store_pc_source),
              "ARM STRB from PC refuses architecturally unpredictable source atomically");
    test_case(arm_transfer_unpredictable_is_atomic(load_base_alias),
              "ARMv4T LDR refuses writeback when Rd equals Rn atomically");
}

static bool arm_transfer_preflight_boundary(uint64_t cycle_budget,
                                            size_t trace_capacity,
                                            gbl_run_reason expected_reason)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_machine before = { 0 };
    gbl_trace_record records[4] = { 0 };
    gbl_trace_record records_before[4];
    gbl_trace_buffer trace = { records, trace_capacity, 1u, UINT64_C(90) };
    const uint32_t opcode = UINT32_C(0xE5A01004);
    const gbl_run_options options = {
        .cycle_budget = cycle_budget,
        .unit_limit = 1u,
        .category_mask = GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS,
        .trace_buffer = &trace
    };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran = false;
    bool matched = false;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    records[0] = (gbl_trace_record){
        .sequence = UINT64_C(89),
        .guest_cycle = UINT64_C(13),
        .category = GBL_TRACE_INSTRUCTION,
        .instruction_address = UINT32_C(0x08000080),
        .opcode = UINT32_C(0xE1A00000)
    };
    if (loaded) {
        gbl_machine_reset(&machine);
        machine.cpu.r0_7[0] = UINT32_C(0x03000100);
        machine.cpu.r0_7[1] = UINT32_C(0xA1B2C3D4);
        write_word(machine.iwram, 0x104u, 0u);
        prepare_arm_instruction(&machine, opcode, TEST_CPSR_SYSTEM);
        before = machine;
        memcpy(records_before, records, sizeof(records_before));
        ran = gbl_cpu_run(&machine, &options, &result) ==
              GBL_RUN_STATUS_OK;
        if (expected_reason == GBL_RUN_REASON_CYCLE_BUDGET ||
            expected_reason == GBL_RUN_REASON_TRACE_OUTPUT_FULL) {
            matched = ran && result.reason == expected_reason &&
                result.termination ==
                    (expected_reason == GBL_RUN_REASON_CYCLE_BUDGET
                         ? GBL_RUN_TIMEOUT
                         : GBL_RUN_TRACE_OUTPUT_FULL) &&
                gbl_test_canonical_equal(&before, &machine) &&
                gbl_test_same_instance_identity_equal(&before, &machine) &&
                trace.count == 1u && trace.next_sequence == UINT64_C(90) &&
                memcmp(records, records_before, sizeof(records)) == 0;
        } else {
            matched = ran && result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                result.termination == GBL_RUN_PROGRESS &&
                result.instructions == 1u && trace.count == 4u &&
                trace.next_sequence == UINT64_C(93) &&
                machine.cpu.r0_7[0] == UINT32_C(0x03000104) &&
                read_test_word(machine.iwram, 0x104u) ==
                    UINT32_C(0xA1B2C3D4);
        }
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
    return loaded && matched;
}

static void test_arm_transfer_preflight_boundaries(void)
{
    test_case(arm_transfer_preflight_boundary(
                  UINT64_C(1), 4u, GBL_RUN_REASON_CYCLE_BUDGET),
              "ARM transfer one-short cycle budget preserves full state and trace buffer");
    test_case(arm_transfer_preflight_boundary(
                  UINT64_C(8), 3u, GBL_RUN_REASON_TRACE_OUTPUT_FULL),
              "ARM transfer one-short trace capacity preserves full state and trace buffer");
    test_case(arm_transfer_preflight_boundary(
                  UINT64_C(2), 4u, GBL_RUN_REASON_UNIT_LIMIT),
              "ARM transfer exact cycle and trace reservations commit the whole unit");
}

static bool arm_transfer_observes_bus_outcome(uint32_t address,
                                             gbl_bus_outcome expected_outcome)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_trace_record records[4] = { 0 };
    gbl_trace_buffer trace = { records, 4u, 0u, UINT64_C(0) };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran = false;
    size_t index;
    size_t matching_reads = 0u;
    bool matched = false;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = address;
        machine.cpu.r0_7[2] = UINT32_C(0xDEADBEEF);
        machine.open_bus = UINT32_C(0xA1B2C3D4);
        ran = run_arm_instruction_with_trace(
            &machine, UINT32_C(0xE5D02000), TEST_CPSR_SYSTEM,
            UINT64_C(8), &trace, &result);
        for (index = 0u; index < trace.count; ++index) {
            const gbl_trace_record *record = &records[index];

            if (record->category == GBL_TRACE_BUS &&
                record->address == address) {
                matching_reads++;
                matched = record->bus_outcome == expected_outcome &&
                          record->value == machine.cpu.r0_7[2];
            }
        }
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
    return loaded && ran && result.termination == GBL_RUN_PROGRESS &&
           matching_reads == 1u && matched;
}

static void test_arm_transfer_unmodeled_bus_results(void)
{
    test_case(arm_transfer_observes_bus_outcome(
                  UINT32_C(0x00000000), GBL_BUS_OUTCOME_BIOS_ABSENT),
              "ARM byte load consumes the actual BIOS-absent bus result once");
    test_case(arm_transfer_observes_bus_outcome(
                  UINT32_C(0x04000004), GBL_BUS_OUTCOME_IO_UNIMPLEMENTED),
              "ARM byte load consumes the actual unimplemented-I/O bus result once");
    test_case(arm_transfer_observes_bus_outcome(
                  UINT32_C(0xFFFFFFFF), GBL_BUS_OUTCOME_UNMAPPED),
              "ARM byte load consumes the actual unmapped maximum-address result once");
}

static bool arm_transfer_increment_wraps_to_bios(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_trace_record records[4] = { 0 };
    gbl_trace_buffer trace = { records, 4u, 0u, UINT64_C(0) };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran = false;
    size_t index;
    size_t matching_reads = 0u;
    bool observed_read = false;
    bool wrapped = false;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = UINT32_MAX;
        ran = run_arm_instruction_with_trace(
            &machine, UINT32_C(0xE5F02001), TEST_CPSR_SYSTEM,
            UINT64_C(8), &trace, &result);
        for (index = 0u; index < trace.count; ++index) {
            const gbl_trace_record *record = &records[index];

            if (record->category == GBL_TRACE_BUS && record->address == 0u) {
                matching_reads++;
                observed_read = record->bus_outcome ==
                                    GBL_BUS_OUTCOME_BIOS_ABSENT &&
                                record->value == machine.cpu.r0_7[2];
            }
        }
        wrapped = result.termination == GBL_RUN_PROGRESS &&
                  machine.cpu.r0_7[0] == 0u && matching_reads == 1u &&
                  observed_read;
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
    return loaded && ran && wrapped;
}

static void test_arm_transfer_modulo_address_wrap(void)
{
    test_case(arm_transfer_increment_wraps_to_bios(),
              "ARM single-transfer pre-index address and writeback wrap modulo 2^32");
}

static bool arm_loaded_pc_ignores_target_lane(uint8_t lane)
{
    const uint32_t target = UINT32_C(0x08000040);
    const uint32_t data_address = UINT32_C(0x03000100);
    const uint32_t raw_word = target | (uint32_t)lane;
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_trace_record records[4] = { 0 };
    gbl_trace_buffer trace = { records, 4u, 0u, UINT64_C(7) };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran = false;
    bool matched;

    make_empty_image(image);
    write_word(image, 0x40u, UINT32_C(0xE1A0A00A));
    write_word(image, 0x44u, UINT32_C(0xE1A0B00B));
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = data_address;
        write_word(machine.iwram, 0x100u, raw_word);
        ran = run_arm_instruction_with_trace(
            &machine, UINT32_C(0xE590F000), TEST_CPSR_SYSTEM,
            UINT64_C(3), &trace, &result);
    }
    matched = loaded && ran &&
              result.termination == GBL_RUN_PROGRESS &&
              result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
              result.instructions == 1u && result.cycles == UINT64_C(3) &&
              trace.count == 4u &&
              trace.next_sequence == UINT64_C(11) &&
              records[1].address == (data_address & ~UINT32_C(3)) &&
              records[1].value == raw_word &&
              records[1].bus_outcome == GBL_BUS_OUTCOME_RAM &&
              records[2].address == target &&
              records[2].value == UINT32_C(0xE1A0A00A) &&
              records[2].access_class == GBL_BUS_CLASS_NONSEQUENTIAL &&
              records[3].address == target + UINT32_C(4) &&
              records[3].value == UINT32_C(0xE1A0B00B) &&
              records[3].access_class == GBL_BUS_CLASS_SEQUENTIAL &&
              machine.cpu.execution_address == target &&
              machine.cpu.cpsr == TEST_CPSR_SYSTEM &&
              machine.cpu.pipeline[0].address == target &&
              machine.cpu.pipeline[0].opcode == UINT32_C(0xE1A0A00A) &&
              machine.cpu.pipeline[0].width == 4u &&
              machine.cpu.pipeline[1].address == target + UINT32_C(4) &&
              machine.cpu.pipeline[1].opcode == UINT32_C(0xE1A0B00B) &&
              machine.cpu.pipeline[1].width == 4u;
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
    return matched;
}

static bool arm_loaded_pc_uses_actual_bus_result(
    uint32_t address, gbl_bus_outcome expected_outcome)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_trace_record records[4] = { 0 };
    gbl_trace_buffer trace = { records, 4u, 0u, UINT64_C(0) };
    gbl_run_result result = { 0 };
    uint32_t loaded_value = 0u;
    uint32_t target = 0u;
    bool loaded;
    bool ran = false;
    bool found_data = false;
    bool matched;
    size_t index;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = address;
        machine.open_bus = UINT32_C(0xA1B2C3D4);
        ran = run_arm_instruction_with_trace(
            &machine, UINT32_C(0xE590F000), TEST_CPSR_SYSTEM,
            UINT64_C(3), &trace, &result);
        for (index = 0u; index < trace.count; ++index) {
            if (records[index].category == GBL_TRACE_BUS &&
                records[index].address == (address & ~UINT32_C(3))) {
                loaded_value = records[index].value;
                target = loaded_value & ~UINT32_C(3);
                found_data = records[index].bus_outcome == expected_outcome;
                break;
            }
        }
    }
    matched = loaded && ran && found_data &&
              result.termination == GBL_RUN_PROGRESS &&
              result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
              result.instructions == 1u && result.cycles == UINT64_C(3) &&
              trace.count == 4u && records[2].address == target &&
              records[3].address == target + UINT32_C(4) &&
              machine.cpu.execution_address == target &&
              machine.cpu.cpsr == TEST_CPSR_SYSTEM &&
              machine.cpu.pipeline[0].address == target &&
              machine.cpu.pipeline[1].address == target + UINT32_C(4);
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
    return matched;
}

static void test_loaded_pc_actual_bus_outcomes(void)
{
    test_case(arm_loaded_pc_uses_actual_bus_result(
                  UINT32_C(0x00000000), GBL_BUS_OUTCOME_BIOS_ABSENT),
              "LDR PC refills from the actual BIOS-absent word result");
    test_case(arm_loaded_pc_uses_actual_bus_result(
                  UINT32_C(0x04000004), GBL_BUS_OUTCOME_IO_UNIMPLEMENTED),
              "LDR PC refills from the actual unimplemented-I/O word result");
    test_case(arm_loaded_pc_uses_actual_bus_result(
                  UINT32_MAX - UINT32_C(3), GBL_BUS_OUTCOME_UNMAPPED),
              "LDR PC refills from the actual unmapped maximum word-address result");
}

static bool arm_loaded_pc_wraps_second_refill(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_machine before_boundary = { 0 };
    gbl_machine expected_boundary = { 0 };
    gbl_trace_record records[4] = { 0 };
    gbl_trace_record records_before_boundary[4];
    gbl_trace_buffer trace = { records, 4u, 0u, UINT64_C(0) };
    gbl_trace_buffer trace_before_boundary;
    const gbl_run_options boundary_options = {
        .cycle_budget = UINT64_C(1),
        .unit_limit = 1u,
        .category_mask = 0u,
        .trace_buffer = NULL
    };
    gbl_run_result result = { 0 };
    gbl_run_result boundary_result = { 0 };
    bool loaded;
    bool ran = false;
    bool first_refill_matched;
    bool boundary_matched = false;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[0] = UINT32_C(0x03000100);
        write_word(machine.iwram, 0x100u, UINT32_C(0xFFFFFFFE));
        ran = run_arm_instruction_with_trace(
            &machine, UINT32_C(0xE590F000), TEST_CPSR_SYSTEM,
            UINT64_C(3), &trace, &result);
    }
    first_refill_matched = loaded && ran &&
                           result.termination == GBL_RUN_PROGRESS &&
                           result.instructions == 1u &&
                           result.cycles == UINT64_C(3) &&
                           trace.count == 4u &&
                           records[1].value == UINT32_C(0xFFFFFFFE) &&
                           records[2].address == UINT32_C(0xFFFFFFFC) &&
                           records[2].bus_outcome == GBL_BUS_OUTCOME_UNMAPPED &&
                           records[3].address == 0u &&
                           records[3].bus_outcome ==
                               GBL_BUS_OUTCOME_BIOS_ABSENT &&
                           machine.cpu.execution_address ==
                               UINT32_C(0xFFFFFFFC) &&
                           machine.cpu.pipeline[0].address ==
                               UINT32_C(0xFFFFFFFC) &&
                           machine.cpu.pipeline[1].address == 0u &&
                           machine.cpu.pipeline[0].width == 4u &&
                           machine.cpu.pipeline[1].width == 4u;
    if (first_refill_matched) {
        gbl_run_status boundary_status;

        before_boundary = machine;
        expected_boundary = before_boundary;
        expected_boundary.cpu.stopped = true;
        expected_boundary.cpu.stop_reason =
            GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
        trace_before_boundary = trace;
        memcpy(records_before_boundary, records, sizeof(records));
        boundary_status = gbl_cpu_run(&machine, &boundary_options,
                                      &boundary_result);
        boundary_matched =
            boundary_status == GBL_RUN_STATUS_OK &&
            boundary_result.termination == GBL_RUN_STOPPED &&
            boundary_result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
            boundary_result.cycles == 0u && boundary_result.instructions == 0u &&
            gbl_test_canonical_equal(&expected_boundary, &machine) &&
            gbl_test_same_instance_identity_equal(&before_boundary, &machine) &&
            trace.count == trace_before_boundary.count &&
            trace.next_sequence == trace_before_boundary.next_sequence &&
            memcmp(records, records_before_boundary, sizeof(records)) == 0;
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
    return first_refill_matched && boundary_matched;
}

static bool arm_loaded_pc_preflight_boundary(
    uint64_t cycle_budget, size_t trace_capacity, size_t trace_count,
    uint64_t next_sequence, uint64_t master_cycles,
    uint64_t retired_instructions, gbl_run_reason expected_reason)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_machine before;
    gbl_trace_record records[4];
    gbl_trace_record records_before[4];
    gbl_trace_buffer trace;
    const gbl_trace_buffer trace_before = {
        records, trace_capacity, trace_count, next_sequence
    };
    gbl_run_result result = { 0 };
    bool loaded;
    bool ran = false;
    bool matched;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    memset(records, 0xA5, sizeof(records));
    memcpy(records_before, records, sizeof(records_before));
    trace = trace_before;
    if (loaded) {
        machine.master_cycles = master_cycles;
        machine.cpu.retired_instructions = retired_instructions;
        machine.cpu.r0_7[0] = UINT32_C(0x03000100);
        write_word(machine.iwram, 0x100u, UINT32_C(0x08000040));
        prepare_arm_instruction(&machine, UINT32_C(0xE590F000),
                                TEST_CPSR_SYSTEM);
        before = machine;
        ran = run_traced_units(
            &machine, cycle_budget, 1u,
            GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS, &trace, &result);
        if (expected_reason == GBL_RUN_REASON_CYCLE_BUDGET ||
            expected_reason == GBL_RUN_REASON_TRACE_OUTPUT_FULL ||
            expected_reason == GBL_RUN_REASON_COUNTER_OVERFLOW) {
            matched = ran && result.reason == expected_reason &&
                      result.cycles == 0u && result.instructions == 0u &&
                      gbl_test_canonical_equal(&before, &machine) &&
                      gbl_test_same_instance_identity_equal(&before,
                                                            &machine) &&
                      trace.records == trace_before.records &&
                      trace.count == trace_before.count &&
                      trace.capacity == trace_before.capacity &&
                      trace.next_sequence == trace_before.next_sequence &&
                      memcmp(records, records_before,
                             sizeof(records_before)) == 0;
        } else {
            matched = ran && result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      result.instructions == 1u &&
                      result.cycles == UINT64_C(3) && trace.count == 4u &&
                      trace.next_sequence == next_sequence + UINT64_C(4) &&
                      records[0].sequence == next_sequence &&
                      records[3].sequence == next_sequence + UINT64_C(3) &&
                      machine.master_cycles == master_cycles + UINT64_C(3) &&
                      machine.cpu.retired_instructions ==
                          retired_instructions + UINT64_C(1) &&
                      machine.cpu.execution_address == UINT32_C(0x08000040) &&
                      machine.cpu.pipeline[1].address == UINT32_C(0x08000044);
        }
    } else {
        matched = false;
    }
    if (loaded) {
        gbl_machine_dispose(&machine);
    }
    return matched;
}

static void test_loaded_pc_target_and_preflight(void)
{
    uint8_t lane;

    for (lane = 0u; lane < 4u; ++lane) {
        test_case(arm_loaded_pc_ignores_target_lane(lane),
                  "ARMv4 LDR PC ignores loaded target lane bits and keeps ARM refill state");
    }
    test_case(arm_transfer_unpredictable_is_atomic(encode_arm_transfer(
                  false, true, true, false, false, true, 0u, 15u, 1u)),
              "ARM LDR PC refuses an unaligned source address atomically");
    test_case(arm_transfer_unpredictable_is_atomic(encode_arm_transfer(
                  false, false, true, false, true, true, 0u, 15u, 4u)),
              "ARMv4T LDRT to PC is architecturally unpredictable and atomic");
    test_case(arm_loaded_pc_wraps_second_refill(),
              "LDR PC wraps its refill then stops at the ARM sequential-wrap boundary");
    test_case(arm_loaded_pc_preflight_boundary(
                  UINT64_C(2), 4u, 0u, UINT64_C(0), UINT64_C(0),
                  UINT64_C(0),
                  GBL_RUN_REASON_CYCLE_BUDGET),
              "LDR PC one-short cycle budget preserves canonical state and caller trace");
    test_case(arm_loaded_pc_preflight_boundary(
                  UINT64_C(3), 4u, 1u, UINT64_C(90), UINT64_C(0),
                  UINT64_C(0),
                  GBL_RUN_REASON_TRACE_OUTPUT_FULL),
              "LDR PC one-short trace capacity preserves canonical state and caller trace");
    test_case(arm_loaded_pc_preflight_boundary(
                  UINT64_C(3), 4u, 0u, UINT64_MAX - UINT64_C(3),
                  UINT64_C(0), UINT64_C(0),
                  GBL_RUN_REASON_TRACE_OUTPUT_FULL),
              "LDR PC trace sequence overflow is refused before the data read");
    test_case(arm_loaded_pc_preflight_boundary(
                  UINT64_C(3), 4u, 0u, UINT64_MAX - UINT64_C(4),
                  UINT64_C(0), UINT64_C(0), GBL_RUN_REASON_NONE),
              "LDR PC exact cycle and trace reservations commit the complete refill");
    test_case(arm_loaded_pc_preflight_boundary(
                  UINT64_C(3), 4u, 0u, UINT64_C(0),
                  UINT64_MAX - UINT64_C(2), UINT64_C(0),
                  GBL_RUN_REASON_COUNTER_OVERFLOW),
              "LDR PC master-cycle overflow is refused before the data read");
    test_case(arm_loaded_pc_preflight_boundary(
                  UINT64_C(3), 4u, 0u, UINT64_C(0), UINT64_C(0), UINT64_MAX,
                  GBL_RUN_REASON_COUNTER_OVERFLOW),
              "LDR PC retired-instruction overflow is refused before the data read");
    test_case(arm_loaded_pc_preflight_boundary(
                  UINT64_C(3), 4u, 0u, UINT64_C(0),
                  UINT64_MAX - UINT64_C(3), UINT64_MAX - UINT64_C(1),
                  GBL_RUN_REASON_NONE),
              "LDR PC exact machine-counter, retired-count, cycle, and trace limits commit");
}

static void test_loaded_pc(void)
{
    test_loaded_pc_actual_bus_outcomes();
    test_loaded_pc_target_and_preflight();
}

static void test_arm_pc_operand_contexts(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran;

    make_empty_image(image);
    write_word(image, 0x00u, UINT32_C(0xE1A0000F)); /* MOV R0, PC */
    loaded = machine_load(&machine, image);
    ran = loaded && run_arm_program_unit(&machine, 2u, &result) &&
          run_arm_program_unit(&machine, 1u, &result) &&
          result.instructions == 1u && result.cycles == 1u &&
          machine.cpu.r0_7[0] == UINT32_C(0x08000008) &&
          machine.cpu.execution_address == UINT32_C(0x08000004);
    test_case(ran,
              "normal ARM data-processing PC operand reads current plus eight");
    gbl_machine_dispose(&machine);

    make_empty_image(image);
    write_word(image, 0x00u, UINT32_C(0xE1A0F000)); /* MOV PC, R0 */
    write_word(image, 0x40u, UINT32_C(0xE1A0A00A));
    write_word(image, 0x44u, UINT32_C(0xE1A0B00B));
    loaded = machine_load(&machine, image);
    machine.cpu.r0_7[0] = UINT32_C(0x08000040);
    ran = loaded && run_arm_program_unit(&machine, 2u, &result) &&
          run_arm_program_unit(&machine, 2u, &result) &&
          result.termination == GBL_RUN_PROGRESS &&
          result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
          result.instructions == 1u && result.cycles == 2u &&
          machine.cpu.cpsr == UINT32_C(0x000000DF) &&
          machine.cpu.execution_address == UINT32_C(0x08000040) &&
          machine.cpu.pipeline[0].width == 4u &&
          machine.cpu.pipeline[0].opcode == UINT32_C(0xE1A0A00A) &&
          machine.cpu.pipeline[1].opcode == UINT32_C(0xE1A0B00B);
    test_case(ran,
              "ARM data-processing PC destination branches without interworking");
    gbl_machine_dispose(&machine);

    make_empty_image(image);
    write_word(image, 0x00u, UINT32_C(0xE580F000)); /* STR PC, [R0] */
    loaded = machine_load(&machine, image);
    machine.cpu.r0_7[0] = UINT32_C(0x02000000);
    ran = loaded && run_arm_program_unit(&machine, 2u, &result) &&
          run_arm_program_unit(&machine, 2u, &result) &&
          result.instructions == 1u && result.cycles == 2u &&
          machine.ewram[0] == UINT8_C(0x0C) &&
          machine.ewram[1] == UINT8_C(0x00) &&
          machine.ewram[2] == UINT8_C(0x00) &&
          machine.ewram[3] == UINT8_C(0x08);
    test_case(ran, "ARM STR PC source uses current plus twelve");
    gbl_machine_dispose(&machine);
}

static void test_thumb_pc_operand_contexts(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool normal_pc;
    bool aligned_address;

    make_empty_image(image);
    loaded = machine_load(&machine, image);
    normal_pc = loaded && run_thumb_instruction(
        &machine, UINT16_C(0x4678), 0u, 1u, &result) &&
        result.instructions == 1u && result.cycles == 1u &&
        machine.cpu.r0_7[0] == UINT32_C(0x08000004);
    test_case(normal_pc, "Thumb high-register PC operand reads current plus four");
    gbl_machine_dispose(&machine);

    make_empty_image(image);
    write_halfword(image, 0x02u, UINT16_C(0x4800)); /* LDR R0, [PC, #0] */
    write_word(image, 0x04u, UINT32_C(0x12345678));
    loaded = machine_load(&machine, image);
    machine.cpu.cpsr = TEST_CPSR_SYSTEM | UINT32_C(0x00000020);
    machine.cpu.execution_address = UINT32_C(0x08000002);
    machine.cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000002), UINT32_C(0x00004800), 2u, true
    };
    machine.cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000004), UINT32_C(0x00005678), 2u, true
    };
    machine.cpu.next_fetch_sequential = true;
    aligned_address = loaded && run_arm_program_unit(&machine, 2u, &result) &&
        result.instructions == 1u && result.cycles == 2u &&
        machine.cpu.r0_7[0] == UINT32_C(0x12345678) &&
        machine.cpu.execution_address == UINT32_C(0x08000004);
    test_case(aligned_address,
              "Thumb literal address uses aligned current plus four");
    gbl_machine_dispose(&machine);
}

static void test_pc_return_condition_and_refusal(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_machine before;
    gbl_machine expected;
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool condition_failed;
    bool absent_spsr;

    make_empty_image(image);
    write_word(image, 0x00u, UINT32_C(0x01B0F00E)); /* MOVEQS PC, LR */
    loaded = machine_load(&machine, image);
    machine.cpu.cpsr = TEST_CPSR_SVC | UINT32_C(0x000000C0);
    machine.cpu.spsr[2] = TEST_CPSR_USER | TEST_CPSR_N | TEST_CPSR_C;
    machine.cpu.svc_r13_14[1] = UINT32_C(0x08000040);
    condition_failed = loaded && run_arm_program_unit(&machine, 2u, &result) &&
        result.cycles == 2u &&
        run_arm_program_unit(&machine, 1u, &result) &&
        result.termination == GBL_RUN_PROGRESS &&
        result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
        result.instructions == 1u && result.cycles == 1u &&
        machine.cpu.cpsr == (TEST_CPSR_SVC | UINT32_C(0x000000C0)) &&
        machine.cpu.execution_address == UINT32_C(0x08000004) &&
        machine.cpu.pipeline[0].address == UINT32_C(0x08000004) &&
        machine.cpu.spsr[2] == (TEST_CPSR_USER | TEST_CPSR_N | TEST_CPSR_C) &&
        machine.cpu.svc_r13_14[1] == UINT32_C(0x08000040);
    test_case(condition_failed,
              "condition-failed S+PC retires sequentially without status restore");
    gbl_machine_dispose(&machine);

    make_empty_image(image);
    write_word(image, 0x00u, UINT32_C(0xE1B0F00E)); /* MOVS PC, LR */
    loaded = machine_load(&machine, image);
    machine.cpu.r0_7[0] = UINT32_C(0xA5A5A5A5);
    machine.cpu.usr_r8_14[6] = UINT32_C(0x08000040);
    absent_spsr = loaded && run_arm_program_unit(&machine, 2u, &result) &&
        result.cycles == 2u;
    before = machine;
    expected = machine;
    expected.cpu.stopped = true;
    expected.cpu.stop_reason = GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
    absent_spsr = absent_spsr && run_arm_program_unit(&machine, 2u, &result) &&
        result.termination == GBL_RUN_STOPPED &&
        result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
        result.instructions == 0u && result.cycles == 0u &&
        machine.master_cycles == before.master_cycles &&
        gbl_test_canonical_equal(&expected, &machine) &&
        gbl_test_same_instance_identity_equal(&before, &machine);
    test_case(absent_spsr,
              "S+PC without an active SPSR refuses before machine effects");
    gbl_machine_dispose(&machine);
}

static void test_arm_pc_return(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_machine before;
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    gbl_trace_record records[4];
    gbl_trace_record records_before[4];
    gbl_trace_buffer trace;
    gbl_trace_buffer trace_before;
    uint64_t digest_before;
    bool loaded;
    bool primed;
    bool one_short;
    bool moved;
    bool subtracted;

    make_empty_image(image);
    write_word(image, 0x00u, UINT32_C(0xE1B0F00E)); /* MOVS PC, LR */
    write_word(image, 0x40u, UINT32_C(0xE1A0A00A));
    write_word(image, 0x44u, UINT32_C(0xE1A0B00B));
    loaded = machine_load(&machine, image);

    machine.cpu.cpsr = TEST_CPSR_SVC | UINT32_C(0x000000C0);
    machine.cpu.spsr[2] = TEST_CPSR_USER | TEST_CPSR_N | TEST_CPSR_C;
    machine.cpu.svc_r13_14[0] = UINT32_C(0x03000100);
    machine.cpu.svc_r13_14[1] = UINT32_C(0x08000040);
    machine.cpu.usr_r8_14[5] = UINT32_C(0x03000200);
    machine.cpu.usr_r8_14[6] = UINT32_C(0x08000070);
    primed = loaded && run_arm_program_unit(&machine, 2u, &result) &&
        result.termination == GBL_RUN_PROGRESS && result.cycles == 2u;
    memset(records, 0xA5, sizeof(records));
    records[0].sequence = UINT64_C(23);
    records[0].guest_cycle = UINT64_C(41);
    records[0].category = GBL_TRACE_BUS;
    records[0].instruction_address = UINT32_C(0x08000008);
    records[0].opcode = UINT32_C(0xE1A00000);
    records[0].address = UINT32_C(0x02000004);
    records[0].value = UINT32_C(0xA1B2C3D4);
    records[0].width = 4u;
    records[0].access_class = GBL_BUS_CLASS_SEQUENTIAL;
    records[0].bus_outcome = GBL_BUS_OUTCOME_RAM;
    records[0].exception_kind = GBL_EXCEPTION_NONE;
    memcpy(records_before, records, sizeof(records));
    trace = (gbl_trace_buffer){ records, 4u, 1u, UINT64_C(23) };
    trace_before = trace;
    before = machine;
    digest_before = gbl_test_canonical_digest(&machine);
    one_short = primed && run_traced_units(
        &machine, 1u, 1u, GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS, &trace,
        &result) && result.termination == GBL_RUN_TIMEOUT &&
        result.reason == GBL_RUN_REASON_CYCLE_BUDGET && result.cycles == 0u &&
        result.instructions == 0u &&
        gbl_test_canonical_equal(&before, &machine) &&
        gbl_test_same_instance_identity_equal(&before, &machine) &&
        digest_before == gbl_test_canonical_digest(&machine) &&
        trace.records == trace_before.records &&
        trace.count == trace_before.count &&
        trace.capacity == trace_before.capacity &&
        trace.next_sequence == trace_before.next_sequence &&
        memcmp(records, records_before, sizeof(records)) == 0;
    test_case(one_short,
              "one-short S+PC refill preserves full state and trace bytes");

    moved = primed && run_arm_program_unit(&machine, 2u, &result) &&
        result.termination == GBL_RUN_PROGRESS &&
        result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
        result.instructions == 1u && result.cycles == 2u &&
        machine.cpu.cpsr == machine.cpu.spsr[2] &&
        machine.cpu.execution_address == UINT32_C(0x08000040) &&
        machine.cpu.pipeline[0].width == 4u &&
        machine.cpu.pipeline[0].address == UINT32_C(0x08000040) &&
        machine.cpu.pipeline[0].opcode == UINT32_C(0xE1A0A00A) &&
        machine.cpu.pipeline[1].address == UINT32_C(0x08000044) &&
        machine.cpu.pipeline[1].opcode == UINT32_C(0xE1A0B00B) &&
        machine.cpu.svc_r13_14[0] == UINT32_C(0x03000100) &&
        machine.cpu.svc_r13_14[1] == UINT32_C(0x08000040) &&
        machine.cpu.usr_r8_14[5] == UINT32_C(0x03000200) &&
        machine.cpu.usr_r8_14[6] == UINT32_C(0x08000070);
    test_case(moved,
              "MOVS PC,LR restores captured SPSR banks and ARM refill state");

    if (loaded) {
        gbl_machine_dispose(&machine);
        make_empty_image(image);
        write_word(image, 0x00u, UINT32_C(0xE25EF004)); /* SUBS PC,LR,#4 */
        write_halfword(image, 0x40u, UINT16_C(0x200A));
        write_halfword(image, 0x42u, UINT16_C(0x210B));
        loaded = machine_load(&machine, image);
        machine.cpu.cpsr = TEST_CPSR_SVC | UINT32_C(0x000000C0);
        machine.cpu.spsr[2] = TEST_CPSR_SYSTEM | UINT32_C(0x00000020) |
                              TEST_CPSR_Z | TEST_CPSR_C;
        machine.cpu.svc_r13_14[0] = UINT32_C(0x03000100);
        machine.cpu.svc_r13_14[1] = UINT32_C(0x08000044);
        machine.cpu.usr_r8_14[5] = UINT32_C(0x03000200);
        machine.cpu.usr_r8_14[6] = UINT32_C(0x08000070);
        subtracted = loaded && run_arm_program_unit(&machine, 2u, &result) &&
            run_arm_program_unit(&machine, 2u, &result) &&
            result.termination == GBL_RUN_PROGRESS &&
            result.instructions == 1u && result.cycles == 2u &&
            machine.cpu.cpsr == machine.cpu.spsr[2] &&
            machine.cpu.execution_address == UINT32_C(0x08000040) &&
            machine.cpu.pipeline[0].width == 2u &&
            machine.cpu.pipeline[0].address == UINT32_C(0x08000040) &&
            machine.cpu.pipeline[0].opcode == UINT32_C(0x200A) &&
            machine.cpu.pipeline[1].address == UINT32_C(0x08000042) &&
            machine.cpu.pipeline[1].opcode == UINT32_C(0x210B) &&
            machine.cpu.svc_r13_14[0] == UINT32_C(0x03000100) &&
            machine.cpu.svc_r13_14[1] == UINT32_C(0x08000044) &&
            machine.cpu.usr_r8_14[5] == UINT32_C(0x03000200) &&
            machine.cpu.usr_r8_14[6] == UINT32_C(0x08000070);
    } else {
        subtracted = false;
    }
    test_case(subtracted,
              "SUBS PC,LR,#4 restores captured SPSR and Thumb refill state");

    gbl_machine_dispose(&machine);
}

static uint32_t encode_arm_mrs(bool saved, uint8_t destination)
{
    return UINT32_C(0xE10F0000) |
           (saved ? UINT32_C(0x00400000) : 0u) |
           ((uint32_t)destination << 12u);
}

static uint32_t encode_arm_msr_register(bool saved, uint8_t fields,
                                        uint8_t source)
{
    return UINT32_C(0xE120F000) |
           (saved ? UINT32_C(0x00400000) : 0u) |
           ((uint32_t)fields << 16u) | (uint32_t)source;
}

static uint32_t encode_arm_msr_immediate(bool saved, uint8_t fields,
                                         uint8_t rotate, uint8_t immediate)
{
    return UINT32_C(0xE320F000) |
           (saved ? UINT32_C(0x00400000) : 0u) |
           ((uint32_t)fields << 16u) | ((uint32_t)rotate << 8u) |
           (uint32_t)immediate;
}

static bool prime_psr_program(gbl_machine *machine, uint8_t image[])
{
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded = machine_load(machine, image);

    if (loaded) {
        machine->cpu.cpsr = TEST_CPSR_SVC;
    }
    return loaded && run_arm_program_unit(machine, 2u, &result) &&
           result.termination == GBL_RUN_PROGRESS && result.cycles == 2u;
}

static bool retire_psr_instruction(gbl_machine *machine,
                                   gbl_run_result *result)
{
    return run_arm_program_unit(machine, 1u, result) &&
           result->termination == GBL_RUN_PROGRESS &&
           result->reason == GBL_RUN_REASON_UNIT_LIMIT &&
           result->instructions == 1u && result->cycles == 1u;
}

static void test_psr_mrs_forms(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    const uint32_t initial_cpsr = TEST_CPSR_SVC | TEST_CPSR_N | TEST_CPSR_C;
    const uint32_t saved_cpsr = TEST_CPSR_USER | UINT32_C(0x20) | TEST_CPSR_Z;
    bool primed;
    bool cpsr_read;
    bool spsr_read;

    make_empty_image(image);
    write_word(image, 0x00u, encode_arm_mrs(false, 0u));
    write_word(image, 0x04u, encode_arm_mrs(true, 1u));
    primed = prime_psr_program(&machine, image);
    machine.cpu.cpsr = initial_cpsr;
    machine.cpu.spsr[2] = saved_cpsr;
    cpsr_read = primed && retire_psr_instruction(&machine, &result) &&
                machine.cpu.r0_7[0] == initial_cpsr;
    spsr_read = cpsr_read && retire_psr_instruction(&machine, &result) &&
                machine.cpu.r0_7[1] == saved_cpsr;
    test_case(spsr_read,
              "MRS CPSR and SPSR read the selected status registers");
    gbl_machine_dispose(&machine);
}

static void test_psr_register_field_masks(void)
{
    static const char *const names[] = {
        "MSR register c mask writes privileged controls and preserves flags",
        "MSR register x mask leaves unallocated extension byte unchanged",
        "MSR register s mask leaves unallocated status byte unchanged",
        "MSR register f mask updates flags and preserves controls"
    };
    static const uint8_t fields[] = { 1u, 2u, 4u, 8u };
    static const uint32_t operands[] = {
        UINT32_C(0x00000092), 0u, 0u, UINT32_C(0xA0000000)
    };
    uint8_t index;

    for (index = 0u; index < (uint8_t)4u; ++index) {
        uint8_t image[TEST_IMAGE_SIZE];
        gbl_machine machine = { 0 };
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        const uint32_t initial_cpsr = TEST_CPSR_SVC | TEST_CPSR_V;
        const uint32_t write_mask = fields[index] == 1u
                                        ? UINT32_C(0x000000DF)
                                        : fields[index] == 8u
                                              ? UINT32_C(0xF0000000)
                                              : 0u;
        bool primed;
        bool retired;
        bool matched;

        make_empty_image(image);
        write_word(image, 0x00u, encode_arm_msr_register(false, fields[index],
                                                         0u));
        primed = prime_psr_program(&machine, image);
        machine.cpu.cpsr = initial_cpsr;
        machine.cpu.r0_7[0] = operands[index];
        retired = primed && retire_psr_instruction(&machine, &result);
        matched = retired && machine.cpu.cpsr ==
                                  ((initial_cpsr & ~write_mask) |
                                   (operands[index] & write_mask));
        test_case(matched, names[index]);
        gbl_machine_dispose(&machine);
    }
}

static void test_psr_immediate_field_masks(void)
{
    static const char *const names[] = {
        "MSR immediate c mask writes valid mode and control bits",
        "MSR immediate x mask leaves unallocated extension byte unchanged",
        "MSR immediate s mask leaves unallocated status byte unchanged",
        "MSR rotated immediate f mask updates the flags byte"
    };
    static const uint8_t fields[] = { 1u, 2u, 4u, 8u };
    static const uint8_t rotates[] = { 0u, 0u, 0u, 4u };
    static const uint8_t immediates[] = { 0x12u, 0x0Bu, 0xCDu, 0xF0u };
    static const uint32_t values[] = {
        UINT32_C(0x00000012), 0u, 0u, UINT32_C(0xF0000000)
    };
    uint8_t index;

    for (index = 0u; index < (uint8_t)4u; ++index) {
        uint8_t image[TEST_IMAGE_SIZE];
        gbl_machine machine = { 0 };
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        const uint32_t initial_cpsr = TEST_CPSR_SVC | TEST_CPSR_N;
        const uint32_t write_mask = fields[index] == 1u
                                        ? UINT32_C(0x000000DF)
                                        : fields[index] == 8u
                                              ? UINT32_C(0xF0000000)
                                              : 0u;
        bool primed;
        bool retired;
        bool matched;

        make_empty_image(image);
        write_word(image, 0x00u,
                   encode_arm_msr_immediate(false, fields[index],
                                            rotates[index], immediates[index]));
        primed = prime_psr_program(&machine, image);
        machine.cpu.cpsr = initial_cpsr;
        retired = primed && retire_psr_instruction(&machine, &result);
        matched = retired && machine.cpu.cpsr ==
                                  ((initial_cpsr & ~write_mask) |
                                   (values[index] & write_mask));
        test_case(matched, names[index]);
        gbl_machine_dispose(&machine);
    }
}

static void test_psr_saved_register_and_immediate_effects(void)
{
    static const uint32_t modes[] = {
        TEST_CPSR_FIQ, TEST_CPSR_IRQ, TEST_CPSR_SVC, TEST_CPSR_ABT,
        TEST_CPSR_UND
    };
    static const uint32_t saved_before[] = {
        UINT32_C(0x80000050), UINT32_C(0x400000BF),
        UINT32_C(0x100000D2), UINT32_C(0x20000073),
        UINT32_C(0xA0000097)
    };
    static const uint32_t register_values[] = {
        UINT32_C(0x0000001F), UINT32_C(0x000000B0),
        UINT32_C(0x00000072), UINT32_C(0x0000009B),
        UINT32_C(0x00000077)
    };
    static const uint32_t saved_after_control[] = {
        UINT32_C(0x8000001F), UINT32_C(0x400000B0),
        UINT32_C(0x10000072), UINT32_C(0x2000009B),
        UINT32_C(0xA0000077)
    };
    static const uint32_t saved_after_flags[] = {
        UINT32_C(0xF000001F), UINT32_C(0xF00000B0),
        UINT32_C(0xF0000072), UINT32_C(0xF000009B),
        UINT32_C(0xF0000077)
    };
    static const char *const mode_names[] = {
        "FIQ", "IRQ", "SVC", "ABT", "UND"
    };
    size_t index;

    for (index = 0u; index < sizeof(modes) / sizeof(modes[0]); ++index) {
        uint8_t image[TEST_IMAGE_SIZE];
        gbl_machine machine = { 0 };
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        uint32_t other_banks[5];
        bool primed;
        bool register_retired;
        bool immediate_retired;
        bool register_matched;
        bool immediate_matched;
        char description[128];

        make_empty_image(image);
        write_word(image, 0x00u, encode_arm_msr_register(true, 1u, 0u));
        write_word(image, 0x04u,
                   encode_arm_msr_immediate(true, 8u, 4u, 0xF0u));
        primed = prime_psr_program(&machine, image);
        machine.cpu.cpsr = modes[index] | UINT32_C(0x00000080) |
                           TEST_CPSR_C;
        memcpy(machine.cpu.spsr, saved_before, sizeof(saved_before));
        machine.cpu.r0_7[0] = register_values[index];
        memcpy(other_banks, saved_before, sizeof(other_banks));
        other_banks[index] = saved_after_control[index];

        register_retired = primed && retire_psr_instruction(&machine, &result);
        register_matched = register_retired &&
                           machine.cpu.cpsr ==
                               (modes[index] | UINT32_C(0x00000080) |
                                TEST_CPSR_C) &&
                           gbl_test_u32_array_equal(machine.cpu.spsr,
                                                    other_banks, 5u);
        (void)snprintf(description, sizeof(description),
                       "MSR SPSR_c register updates only the %s saved control bank",
                       mode_names[index]);
        test_case(register_matched, description);

        other_banks[index] = saved_after_flags[index];
        immediate_retired = register_matched &&
                            retire_psr_instruction(&machine, &result);
        immediate_matched = immediate_retired &&
                            machine.cpu.cpsr ==
                                (modes[index] | UINT32_C(0x00000080) |
                                 TEST_CPSR_C) &&
                            gbl_test_u32_array_equal(machine.cpu.spsr,
                                                     other_banks, 5u);
        (void)snprintf(description, sizeof(description),
                       "MSR SPSR_f rotated immediate updates only the %s saved flags",
                       mode_names[index]);
        test_case(immediate_matched, description);
        gbl_machine_dispose(&machine);
    }
}

static void test_psr_zero_field_masks(void)
{
    static const uint32_t opcodes[] = {
        UINT32_C(0xE120F000), /* MSR CPSR with no selected fields, R0 */
        UINT32_C(0xE320F013), /* MSR CPSR with no selected fields, #0x13 */
        UINT32_C(0xE160F000), /* MSR SPSR with no selected fields, R0 */
        UINT32_C(0xE360F013)  /* MSR SPSR with no selected fields, #0x13 */
    };
    static const uint32_t operands[] = {
        UINT32_C(0x00000013), 0u, UINT32_C(0x00000013), 0u
    };
    static const char *const names[] = {
        "MSR CPSR zero register mask retires with valid R0=0x13 unchanged",
        "MSR CPSR zero immediate mask retires with legal #0x13 unchanged",
        "MSR SPSR zero register mask retires with valid R0=0x13 unchanged",
        "MSR SPSR zero immediate mask retires with legal #0x13 unchanged"
    };
    static const uint32_t saved_status[5] = {
        UINT32_C(0x80000050), UINT32_C(0x400000BF),
        UINT32_C(0x100000D2), UINT32_C(0x20000073),
        UINT32_C(0xA0000097)
    };
    size_t index;

    for (index = 0u; index < sizeof(opcodes) / sizeof(opcodes[0]); ++index) {
        uint8_t image[TEST_IMAGE_SIZE];
        gbl_machine machine = { 0 };
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        const uint32_t active_cpsr = TEST_CPSR_SVC | TEST_CPSR_N |
                                     UINT32_C(0x00000080);
        bool primed;
        bool retired;
        bool unchanged;

        make_empty_image(image);
        write_word(image, 0x00u, opcodes[index]);
        primed = prime_psr_program(&machine, image);
        machine.cpu.cpsr = active_cpsr;
        memcpy(machine.cpu.spsr, saved_status, sizeof(saved_status));
        machine.cpu.r0_7[0] = operands[index];
        retired = primed && retire_psr_instruction(&machine, &result);
        unchanged = retired && machine.cpu.cpsr == active_cpsr &&
                    gbl_test_u32_array_equal(machine.cpu.spsr,
                                             saved_status, 5u) &&
                    machine.cpu.execution_address == UINT32_C(0x08000004);
        test_case(unchanged, names[index]);
        gbl_machine_dispose(&machine);
    }
}

static void test_psr_composed_field_masks(void)
{
    static const uint32_t opcodes[] = {
        UINT32_C(0xE129F000), /* MSR CPSR_c|f, R0 */
        UINT32_C(0xE329F14D), /* MSR CPSR_c|f, ROR(0x4D, 2) */
        UINT32_C(0xE169F000), /* MSR SPSR_c|f, R0 */
        UINT32_C(0xE369F14D)  /* MSR SPSR_c|f, ROR(0x4D, 2) */
    };
    static const uint32_t initial_cpsr[2] = {
        UINT32_C(0x500055D3), UINT32_C(0x9000AA53)
    };
    static const uint32_t initial_spsr[2] = {
        UINT32_C(0x300056F7), UINT32_C(0x600078D3)
    };
    static const uint32_t source_values[] = {
        UINT32_C(0xA0000093), 0u,
        UINT32_C(0xA00000B7), 0u
    };
    static const uint32_t expected_cpsr[2] = {
        UINT32_C(0xA0005593), UINT32_C(0x4000AA13)
    };
    static const uint32_t expected_spsr[2] = {
        UINT32_C(0xA00056B7), UINT32_C(0x40007813)
    };
    static const uint32_t saved_initial[5] = {
        UINT32_C(0x80000050), UINT32_C(0x400000BF),
        UINT32_C(0x100000D2), UINT32_C(0x20000073),
        UINT32_C(0xA0000097)
    };
    static const char *const names[] = {
        "MSR CPSR_c|f register updates flags and controls while preserving x/s",
        "MSR CPSR_c|f rotated immediate updates flags and controls while preserving x/s",
        "MSR SPSR_c|f register updates the selected saved bank and preserves x/s",
        "MSR SPSR_c|f rotated immediate updates the selected saved bank and preserves x/s"
    };
    size_t index;

    for (index = 0u; index < sizeof(opcodes) / sizeof(opcodes[0]); ++index) {
        uint8_t image[TEST_IMAGE_SIZE];
        gbl_machine machine = { 0 };
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        uint32_t expected_saved[5];
        const bool saved_target = (index & 2u) != 0u;
        const uint32_t expected_active = saved_target
                                             ? TEST_CPSR_SVC | TEST_CPSR_V |
                                                   UINT32_C(0x00000080)
                                             : expected_cpsr[index];
        bool primed;
        bool retired;
        bool matched;

        make_empty_image(image);
        write_word(image, 0x00u, opcodes[index]);
        primed = prime_psr_program(&machine, image);
        machine.cpu.cpsr = saved_target
                               ? expected_active
                               : initial_cpsr[index];
        memcpy(machine.cpu.spsr, saved_initial, sizeof(saved_initial));
        if (saved_target) {
            machine.cpu.spsr[2] = initial_spsr[index - 2u];
        }
        machine.cpu.r0_7[0] = source_values[index];
        memcpy(expected_saved, saved_initial, sizeof(expected_saved));
        if (saved_target) {
            expected_saved[2] = expected_spsr[index - 2u];
        }
        retired = primed && retire_psr_instruction(&machine, &result);
        matched = retired && machine.cpu.cpsr == expected_active &&
                  gbl_test_u32_array_equal(machine.cpu.spsr,
                                           expected_saved, 5u);
        test_case(matched, names[index]);
        gbl_machine_dispose(&machine);
    }
}

static void test_psr_mode_reads_and_banks(void)
{
    static const uint32_t modes[] = {
        TEST_CPSR_USER, TEST_CPSR_FIQ, TEST_CPSR_IRQ, TEST_CPSR_SVC,
        TEST_CPSR_ABT, TEST_CPSR_UND, TEST_CPSR_SYSTEM
    };
    uint8_t index;
    bool all_modes_read = true;
    bool all_saved_reads = true;
    bool bank_visibility;

    for (index = 0u; index < (uint8_t)7u; ++index) {
        uint8_t image[TEST_IMAGE_SIZE];
        gbl_machine machine = { 0 };
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        const uint32_t status = modes[index] | TEST_CPSR_Z;
        bool loaded;
        bool primed;
        bool read;

        make_empty_image(image);
        write_word(image, 0x00u, encode_arm_mrs(false, 0u));
        loaded = machine_load(&machine, image);
        machine.cpu.cpsr = status;
        primed = loaded && run_arm_program_unit(&machine, 2u, &result);
        read = primed && retire_psr_instruction(&machine, &result) &&
               machine.cpu.r0_7[0] == status;
        all_modes_read = all_modes_read && read;
        gbl_machine_dispose(&machine);
    }
    for (index = 1u; index < (uint8_t)6u; ++index) {
        uint8_t image[TEST_IMAGE_SIZE];
        gbl_machine machine = { 0 };
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        const uint32_t saved = modes[index] |
                               ((index & 1u) != 0u ? TEST_CPSR_N : 0u) |
                               ((index & 2u) != 0u ? TEST_CPSR_C : 0u) |
                               ((index & 4u) != 0u ? TEST_CPSR_V : 0u);
        bool loaded;
        bool primed;
        bool read;

        make_empty_image(image);
        write_word(image, 0x00u, encode_arm_mrs(true, 0u));
        loaded = machine_load(&machine, image);
        machine.cpu.cpsr = modes[index];
        if (index <= 5u) {
            machine.cpu.spsr[index - 1u] = saved;
        }
        primed = loaded && run_arm_program_unit(&machine, 2u, &result);
        read = primed && retire_psr_instruction(&machine, &result) &&
               machine.cpu.r0_7[0] == saved;
        all_saved_reads = all_saved_reads && read;
        gbl_machine_dispose(&machine);
    }
    test_case(all_modes_read && all_saved_reads,
              "MRS exposes CPSR in all seven modes and each exception SPSR bank");

    {
        uint8_t image[TEST_IMAGE_SIZE];
        gbl_machine machine = { 0 };
        cpu_test_allocator_state allocator_state = { 0 };
        gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
        bool allocation_was_live;
        bool failed_allocation_was_untracked = false;
        bool allocation_was_released;
        bool loaded;
        bool ran;

        make_empty_image(image);
        write_word(image, 0x00u, encode_arm_msr_register(false, 1u, 0u));
        write_word(image, 0x04u, UINT32_C(0xE1A01008)); /* MOV R1, R8 */
        write_word(image, 0x08u, encode_arm_msr_register(false, 1u, 2u));
        write_word(image, 0x0Cu, UINT32_C(0xE1A03008)); /* MOV R3, R8 */
        loaded = machine_load_with_allocator(&machine, image, &allocator_state);
        machine.cpu.cpsr = TEST_CPSR_SVC;
        machine.cpu.r0_7[0] = TEST_CPSR_FIQ;
        machine.cpu.r0_7[2] = TEST_CPSR_SYSTEM;
        machine.cpu.fiq_r8_14[0] = UINT32_C(0xF1F1F1F1);
        machine.cpu.usr_r8_14[0] = UINT32_C(0x51515151);
        ran = loaded && run_arm_program_unit(&machine, 2u, &result) &&
              retire_psr_instruction(&machine, &result) &&
              retire_psr_instruction(&machine, &result) &&
              retire_psr_instruction(&machine, &result) &&
              retire_psr_instruction(&machine, &result) &&
              machine.cpu.r0_7[1] == UINT32_C(0xF1F1F1F1) &&
              machine.cpu.r0_7[3] == UINT32_C(0x51515151) &&
              (machine.cpu.cpsr & TEST_CPSR_MODE_MASK) == TEST_CPSR_SYSTEM;
        bank_visibility = ran;

        allocation_was_live = loaded &&
                              allocator_state.successful_allocation_count == 1u &&
                              allocator_state.live_allocation_count == 1u &&
                              allocator_state.live_bytes == sizeof(image);
        if (loaded) {
            const size_t successful_before_failure =
                allocator_state.successful_allocation_count;
            const size_t live_before_failure =
                allocator_state.live_allocation_count;
            const size_t bytes_before_failure = allocator_state.live_bytes;
            gbl_load_result failed_load;

            allocator_state.fail_next_allocation = true;
            failed_load = gbl_load_rom(&machine, image, sizeof(image));
            failed_allocation_was_untracked =
                failed_load.status == GBL_LOAD_STATUS_RESOURCE_FAILURE &&
                failed_load.reason == GBL_LOAD_REASON_ALLOCATION &&
                allocator_state.successful_allocation_count ==
                    successful_before_failure &&
                allocator_state.live_allocation_count == live_before_failure &&
                allocator_state.live_bytes == bytes_before_failure;
        }
        gbl_machine_dispose(&machine);
        allocation_was_released = allocator_state.live_allocation_count == 0u &&
                                  allocator_state.live_bytes == 0u;
        test_case(allocation_was_live && failed_allocation_was_untracked &&
                      allocation_was_released,
                  "PSR bank-visibility fixture balances owned ROM allocation");
    }
    test_case(bank_visibility,
              "CPSR mode writes expose distinct FIQ and User/System R8 banks");
}

static void test_psr_privilege_and_saved_state(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool user_flags;
    bool saved_t;
    bool condition_failed;

    make_empty_image(image);
    write_word(image, 0x00u, encode_arm_msr_register(false, 15u, 0u));
    write_word(image, 0x04u, encode_arm_msr_immediate(true, 1u, 0u, 0x30u));
    write_word(image, 0x08u, encode_arm_mrs(true, 1u));
    if (machine_load(&machine, image)) {
        machine.cpu.cpsr = TEST_CPSR_USER | UINT32_C(0x00000080);
        machine.cpu.r0_7[0] = UINT32_C(0xA00000F3);
        user_flags = run_arm_program_unit(&machine, 2u, &result) &&
                     retire_psr_instruction(&machine, &result) &&
                     machine.cpu.cpsr == (TEST_CPSR_USER | UINT32_C(0x00000080) |
                                          UINT32_C(0xA0000000));
    } else {
        user_flags = false;
    }
    gbl_machine_dispose(&machine);

    machine = (gbl_machine){ 0 };
    make_empty_image(image);
    write_word(image, 0x00u, encode_arm_msr_immediate(true, 1u, 0u, 0x30u));
    write_word(image, 0x04u, encode_arm_mrs(true, 1u));
    if (machine_load(&machine, image)) {
        machine.cpu.cpsr = TEST_CPSR_SVC;
        machine.cpu.spsr[2] = TEST_CPSR_IRQ | UINT32_C(0x00000040) |
                              TEST_CPSR_N;
        saved_t = run_arm_program_unit(&machine, 2u, &result) &&
                  retire_psr_instruction(&machine, &result) &&
                  machine.cpu.spsr[2] ==
                      (TEST_CPSR_USER | UINT32_C(0x20) | TEST_CPSR_N) &&
                  retire_psr_instruction(&machine, &result) &&
                  machine.cpu.r0_7[1] == machine.cpu.spsr[2];
    } else {
        saved_t = false;
    }
    gbl_machine_dispose(&machine);

    machine = (gbl_machine){ 0 };
    make_empty_image(image);
    write_word(image, 0x00u, UINT32_C(0x0121F000)); /* MSREQ CPSR_c, R0 */
    if (machine_load(&machine, image)) {
        const uint32_t before = TEST_CPSR_SVC | TEST_CPSR_C;
        machine.cpu.cpsr = before;
        machine.cpu.r0_7[0] = TEST_CPSR_USER;
        condition_failed = run_arm_program_unit(&machine, 2u, &result) &&
                           retire_psr_instruction(&machine, &result) &&
                           machine.cpu.cpsr == before &&
                           machine.cpu.execution_address ==
                               UINT32_C(0x08000004);
    } else {
        condition_failed = false;
    }
    test_case(user_flags && saved_t && condition_failed,
              "MSR applies user flags-only writes and saved-state T while failed conditions retire unchanged");
    gbl_machine_dispose(&machine);
}

static void test_psr_unpredictable_refusals(void)
{
    static const char *const names[] = {
        "refusal [MSR CPSR_c, invalid mode, SVC]",
        "refusal [MSR CPSR_c immediate, T set, SVC]",
        "refusal [MRS CPSR, PC destination, SVC]",
        "refusal [MRS SPSR, absent User bank]",
        "refusal [MRS SPSR, absent System bank]",
        "refusal [MSR SPSR_c, absent User bank]",
        "refusal [MSR SPSR_c, absent System bank]",
        "refusal [MSR SPSR_c, invalid mode, SVC]",
        "refusal [MSR CPSR_x, reserved operand bit inside selected byte]",
        "refusal [MSR CPSR zero fields, reserved operand bit outside mask, SVC]",
        "refusal [MSR CPSR_f, StateMask T with c unselected, SVC]"
    };
    static const uint32_t opcodes[] = {
        UINT32_C(0xE121F000), /* MSR CPSR_c, R0 */
        UINT32_C(0xE321F020), /* MSR CPSR_c, #T */
        UINT32_C(0xE10FF000), /* MRS PC, CPSR */
        UINT32_C(0xE14F0000), /* MRS R0, SPSR */
        UINT32_C(0xE14F0000), /* MRS R0, SPSR */
        UINT32_C(0xE161F000), /* MSR SPSR_c, R0 */
        UINT32_C(0xE161F000), /* MSR SPSR_c, R0 */
        UINT32_C(0xE161F000), /* MSR SPSR_c, R0 */
        UINT32_C(0xE122F000), /* MSR CPSR_x, R0 */
        UINT32_C(0xE120F000), /* MSR CPSR with no selected fields, R0 */
        UINT32_C(0xE128F000)  /* MSR CPSR_f, R0 */
    };
    static const uint32_t modes[] = {
        TEST_CPSR_SVC, TEST_CPSR_SVC, TEST_CPSR_SVC, TEST_CPSR_USER,
        TEST_CPSR_SYSTEM, TEST_CPSR_USER, TEST_CPSR_SYSTEM,
        TEST_CPSR_SVC, TEST_CPSR_SVC, TEST_CPSR_SVC, TEST_CPSR_SVC
    };
    static const uint32_t operands[] = {
        UINT32_C(0x15), UINT32_C(0), UINT32_C(0), UINT32_C(0),
        UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0x15),
        UINT32_C(0x00000100), UINT32_C(0x00000113),
        UINT32_C(0x00000033)
    };
    uint8_t index;

    for (index = 0u; index < (uint8_t)11u; ++index) {
        uint8_t image[TEST_IMAGE_SIZE];
        gbl_machine machine = { 0 };
        gbl_machine before;
        gbl_machine expected;
        gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
        gbl_trace_record records[2];
        gbl_trace_record records_before[2];
        gbl_trace_buffer trace;
        gbl_trace_buffer trace_before;
        bool loaded;
        bool primed;
        bool refused;

        make_empty_image(image);
        write_word(image, 0x00u, opcodes[index]);
        loaded = machine_load(&machine, image);
        machine.cpu.cpsr = modes[index];
        machine.cpu.r0_7[0] = operands[index];
        primed = loaded && run_arm_program_unit(&machine, 2u, &result);
        memset(records, 0x5Au, sizeof(records));
        records[0].sequence = UINT64_C(31);
        records[0].guest_cycle = UINT64_C(47);
        records[0].category = GBL_TRACE_BUS;
        records[0].instruction_address = UINT32_C(0x08000004);
        records[0].opcode = opcodes[index];
        records[0].address = UINT32_C(0x02000008);
        records[0].value = UINT32_C(0xA1B2C3D4);
        records[0].width = 4u;
        records[0].access_class = GBL_BUS_CLASS_SEQUENTIAL;
        records[0].bus_outcome = GBL_BUS_OUTCOME_RAM;
        records[0].exception_kind = GBL_EXCEPTION_NONE;
        memcpy(records_before, records, sizeof(records));
        trace = (gbl_trace_buffer){ records, 2u, 1u, UINT64_C(31) };
        trace_before = trace;
        before = machine;
        expected = machine;
        expected.cpu.stopped = true;
        expected.cpu.stop_reason = GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
        refused = primed && run_traced_units(
            &machine, 1u, 1u, GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS,
            &trace, &result) && result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
            result.instructions == 0u && result.cycles == 0u &&
            gbl_test_canonical_equal(&expected, &machine) &&
            gbl_test_same_instance_identity_equal(&before, &machine) &&
            machine.master_cycles == before.master_cycles &&
            trace.records == trace_before.records &&
            trace.count == trace_before.count &&
            trace.capacity == trace_before.capacity &&
            trace.next_sequence == trace_before.next_sequence &&
            memcmp(records, records_before, sizeof(records)) == 0;
        test_case(refused, names[index]);
        gbl_machine_dispose(&machine);
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
        const uint32_t original_svc_spsr = UINT32_C(0x13572468);
        const uint32_t original_svc_lr = UINT32_C(0x24681357);
        bool swi_skipped;
        bool coprocessor_skipped;

        machine.cpu.spsr[2] = original_svc_spsr;
        machine.cpu.svc_r13_14[1] = original_svc_lr;
        ran = loaded && run_arm_instruction(
            &machine, UINT32_C(0x0F000042), 0u, 1u, &result);
        swi_skipped = ran && result.termination == GBL_RUN_PROGRESS &&
            result.instructions == 1u && result.cycles == 1u &&
            machine.cpu.execution_address == UINT32_C(0x08000004) &&
            machine.cpu.pipeline[0].address == UINT32_C(0x08000004) &&
            machine.cpu.cpsr == TEST_CPSR_SYSTEM && !machine.cpu.stopped &&
            machine.cpu.stop_reason == GBL_CPU_STOP_NONE &&
            machine.cpu.spsr[2] == original_svc_spsr &&
            machine.cpu.svc_r13_14[1] == original_svc_lr &&
            machine.cpu.last_exception.kind == GBL_EXCEPTION_NONE;

        ran = loaded && run_arm_instruction(
            &machine, UINT32_C(0x0C000010), 0u, 1u, &result);
        coprocessor_skipped = ran &&
            result.termination == GBL_RUN_PROGRESS &&
            result.instructions == 1u && result.cycles == 1u &&
            machine.cpu.execution_address == UINT32_C(0x08000004) &&
            machine.cpu.pipeline[0].address == UINT32_C(0x08000004) &&
            machine.cpu.cpsr == TEST_CPSR_SYSTEM && !machine.cpu.stopped &&
            machine.cpu.stop_reason == GBL_CPU_STOP_NONE &&
            machine.cpu.spsr[2] == original_svc_spsr &&
            machine.cpu.svc_r13_14[1] == original_svc_lr &&
            machine.cpu.last_exception.kind == GBL_EXCEPTION_NONE;

        test_case(swi_skipped && coprocessor_skipped,
                  "condition-failed ARM SWI and coprocessor instructions advance without exception entry");
    }

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
        const bool unsupported_block_transfer = loaded && run_arm_instruction(
            &machine, UINT32_C(0xE8800002), 0u, 1u, &result) &&
            result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
            machine.cpu.stop_reason == GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
        const bool undefined_encoding = loaded && run_arm_instruction(
            &machine, UINT32_C(0xE7F000F0), 0u, 1u, &result) &&
            result.termination == GBL_RUN_STOPPED &&
            result.reason == GBL_RUN_REASON_GUEST_STOP &&
            machine.cpu.stop_reason == GBL_CPU_STOP_UNDEFINED_NO_BIOS &&
            machine.cpu.spsr[4] == TEST_CPSR_SYSTEM &&
            machine.cpu.execution_address == UINT32_C(4);

        test_case(unsupported_block_transfer && undefined_encoding,
                  "valid unsupported block transfer and architecturally undefined ARM words stop distinctly");
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

    machine.iwram[0x10u] = UINT8_C(0x11);
    machine.iwram[0x11u] = UINT8_C(0x22);
    machine.iwram[0x12u] = UINT8_C(0x33);
    machine.iwram[0x13u] = UINT8_C(0x44);
    {
        static const uint8_t lanes[] = { 1u, 2u, 3u };
        static const uint32_t rotated_values[] = {
            UINT32_C(0x11443322), UINT32_C(0x22114433),
            UINT32_C(0x33221144)
        };
        size_t index;
        bool every_lane_rotated = loaded;

        for (index = 0u; index < sizeof(lanes) / sizeof(lanes[0]); ++index) {
            machine.cpu.r0_7[1] = UINT32_C(0x03000010) + lanes[index];
            every_lane_rotated = every_lane_rotated &&
                run_thumb_instruction(&machine, UINT16_C(0x6808), 0u, 2u,
                                      &result) &&
                machine.cpu.r0_7[0] == rotated_values[index];
        }
        test_case(every_lane_rotated,
                  "Thumb immediate word LDR rotates unaligned reads for lanes one through three");
    }

    machine.cpu.r0_7[1] = UINT32_C(0x0300000D);
    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x6848), 0u,
                                          2u, &result);
    test_case(ran && machine.cpu.r0_7[0] == UINT32_C(0x11443322),
              "Thumb immediate word LDR rotates the base-plus-offset effective address");

    machine.iwram[0x10u] = 0u;
    machine.iwram[0x11u] = 0u;
    machine.iwram[0x12u] = 0u;
    machine.iwram[0x13u] = 0u;
    machine.cpu.r0_7[0] = UINT32_C(0xAABBCCDD);
    machine.cpu.r0_7[1] = UINT32_C(0x0300000D);
    ran = loaded && run_thumb_instruction(&machine, UINT16_C(0x6048), 0u,
                                          2u, &result);
    test_case(ran && machine.iwram[0x10u] == UINT8_C(0xDD) &&
                  machine.iwram[0x11u] == UINT8_C(0xCC) &&
                  machine.iwram[0x12u] == UINT8_C(0xBB) &&
                  machine.iwram[0x13u] == UINT8_C(0xAA),
              "Thumb immediate word STR keeps store byte lanes unchanged");

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
        const gbl_run_options short_options = {
            .cycle_budget = 1u,
            .unit_limit = 1u,
            .category_mask = 0u,
            .trace_buffer = NULL
        };
        const gbl_run_options exact_options = {
            .cycle_budget = 2u,
            .unit_limit = 1u,
            .category_mask = 0u,
            .trace_buffer = NULL
        };
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

static void make_mailbox_guest(uint8_t image[TEST_IMAGE_SIZE],
                               gbl_mailbox_state state, uint32_t case_id)
{
    make_empty_image(image);
    write_word(image, 0x00u, UINT32_C(0xE59F1038)); /* LDR r1,[pc,#0x38] */
    write_word(image, 0x04u, UINT32_C(0xE59F2038)); /* LDR r2,[pc,#0x38] */
    write_word(image, 0x08u, UINT32_C(0xE5812000)); /* STR r2,[r1,#0] */
    write_word(image, 0x0Cu, UINT32_C(0xE59F2034));
    write_word(image, 0x10u, UINT32_C(0xE5812004));
    write_word(image, 0x14u, UINT32_C(0xE59F2030));
    write_word(image, 0x18u, UINT32_C(0xE5812008));
    write_word(image, 0x1Cu, UINT32_C(0xE59F202C));
    write_word(image, 0x20u, UINT32_C(0xE581200C));
    write_word(image, 0x24u, UINT32_C(0xE59F2028));
    write_word(image, 0x28u, UINT32_C(0xE5812010));
    write_word(image, 0x40u, GBL_MAILBOX_ADDRESS);
    write_word(image, 0x44u, GBL_MAILBOX_MAGIC);
    write_word(image, 0x48u, GBL_MAILBOX_VERSION);
    write_word(image, 0x4Cu, (uint32_t)state);
    write_word(image, 0x50u, case_id);
    write_word(image, 0x54u, GBL_MAILBOX_COMPLETION_MARKER);
}

static void write_mailbox(gbl_machine *machine, uint32_t magic,
                          uint32_t version, uint32_t state,
                          uint32_t case_id, uint32_t completion_marker)
{
    write_word(machine->iwram, TEST_MAILBOX_OFFSET, magic);
    write_word(machine->iwram, TEST_MAILBOX_OFFSET + 4u, version);
    write_word(machine->iwram, TEST_MAILBOX_OFFSET + 8u, state);
    write_word(machine->iwram, TEST_MAILBOX_OFFSET + 12u, case_id);
    write_word(machine->iwram, TEST_MAILBOX_OFFSET + 16u,
               completion_marker);
}

static void test_guest_mailbox_and_peek(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result partial_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    gbl_run_result final_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    gbl_mailbox_result pending;
    gbl_mailbox_result passed;
    bool loaded;
    bool partial_ran;
    bool final_ran;

    make_mailbox_guest(image, GBL_MAILBOX_STATE_PASS,
                       TEST_MAILBOX_PASS_CASE);
    loaded = machine_load(&machine, image);
    partial_ran = loaded && run_units(&machine, UINT64_C(22), 0u,
                                      &partial_result);
    pending = loaded ? gbl_mailbox_read(&machine)
                     : (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
    test_case(partial_ran && partial_result.cycles == UINT64_C(22) &&
                  machine.master_cycles == UINT64_C(22) &&
                  machine.iwram[TEST_MAILBOX_OFFSET + 8u] ==
                      (uint8_t)GBL_MAILBOX_STATE_PASS &&
                  machine.iwram[TEST_MAILBOX_OFFSET + 16u] == 0u &&
                  pending.status == GBL_MAILBOX_PENDING,
              "partial terminal guest stores remain pending before final marker");

    final_ran = loaded && run_units(&machine, UINT64_C(2), 0u,
                                    &final_result);
    passed = loaded ? gbl_mailbox_read(&machine)
                    : (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
    test_case(final_ran && final_result.cycles == UINT64_C(2) &&
                  machine.master_cycles == UINT64_C(24) &&
                  final_result.termination == GBL_RUN_TIMEOUT &&
                  passed.status == GBL_MAILBOX_PASS &&
                  passed.case_id == TEST_MAILBOX_PASS_CASE,
              "real guest stores completion last and passes at exact budget boundary");

    seed_nondefault_cpu_state(&machine.cpu);
    machine.master_cycles = UINT64_C(0x123456789);
    machine.open_bus = UINT32_C(0xAABBCCDD);
    machine.dispcnt = UINT16_C(0x1357);
    machine.ewram[0] = UINT8_C(0x11);
    machine.pram[0] = UINT8_C(0x22);
    machine.vram[0] = UINT8_C(0x33);
    machine.oam[0] = UINT8_C(0x44);
    machine.iwram[0] = UINT8_C(0xA5);
    machine.iwram[1] = UINT8_C(0x3C);
    machine.iwram[2] = UINT8_C(0x7E);
    machine.iwram[GBL_IWRAM_SIZE - 1u] = UINT8_C(0x5A);
    {
        struct peek_storage {
            uint8_t leading_guard[2];
            uint8_t output[4];
            uint8_t trace_storage[8];
            uint8_t trailing_guard[2];
        } storage;
        gbl_machine before;
        uint64_t digest_before;
        static const uint8_t trace_before[8] = {
            UINT8_C(0xCC), UINT8_C(0xCC), UINT8_C(0xCC), UINT8_C(0xCC),
            UINT8_C(0xCC), UINT8_C(0xCC), UINT8_C(0xCC), UINT8_C(0xCC)
        };
        bool first;
        bool last;
        bool unaligned;
        gbl_mailbox_result repeated;
        bool guards;

        memset(&storage, 0xCC, sizeof(storage));
        before = machine;
        digest_before = gbl_test_canonical_digest(&machine);
        first = gbl_machine_peek(&machine, UINT32_C(0x03000000),
                                 storage.output, 1u) == GBL_PEEK_STATUS_OK;
        last = gbl_machine_peek(&machine, UINT32_C(0x03007FFF),
                                &storage.output[1], 1u) ==
               GBL_PEEK_STATUS_OK;
        unaligned = gbl_machine_peek(&machine, UINT32_C(0x03000001),
                                     &storage.output[2], 2u) ==
                    GBL_PEEK_STATUS_OK;
        repeated = gbl_mailbox_read(&machine);
        guards = storage.output[0] == UINT8_C(0xA5) &&
            storage.output[1] == UINT8_C(0x5A) &&
            storage.output[2] == UINT8_C(0x3C) &&
            storage.output[3] == UINT8_C(0x7E);
        test_case(first && last && unaligned && guards &&
                      storage.leading_guard[0] == UINT8_C(0xCC) &&
                      storage.leading_guard[1] == UINT8_C(0xCC) &&
                      storage.trailing_guard[0] == UINT8_C(0xCC) &&
                      storage.trailing_guard[1] == UINT8_C(0xCC) &&
                      memcmp(storage.trace_storage, trace_before,
                             sizeof(trace_before)) == 0 &&
                      repeated.status == GBL_MAILBOX_PASS &&
                      repeated.case_id == TEST_MAILBOX_PASS_CASE &&
                      gbl_test_canonical_equal(&before, &machine) &&
                      digest_before == gbl_test_canonical_digest(&machine) &&
                      gbl_test_same_instance_identity_equal(&before, &machine),
                  "peek and mailbox reads leave full state and caller guards unchanged");

        memset(storage.output, 0xA7, sizeof(storage.output));
        {
            const uint8_t output_before[sizeof(storage.output)] = {
                UINT8_C(0xA7), UINT8_C(0xA7), UINT8_C(0xA7), UINT8_C(0xA7)
            };
            const bool zero_end = gbl_machine_peek(
                &machine, UINT32_C(0x03008000), NULL, 0u) ==
                GBL_PEEK_STATUS_OK;
            const bool zero_start = gbl_machine_peek(
                &machine, UINT32_C(0x03000000), NULL, 0u) ==
                GBL_PEEK_STATUS_OK;
            const bool null_output = gbl_machine_peek(
                &machine, UINT32_C(0x03000000), NULL, 1u) ==
                GBL_PEEK_STATUS_INVALID_ARGUMENT;
            const bool outside = gbl_machine_peek(
                &machine, UINT32_C(0x03007FFF), storage.output, 2u) ==
                GBL_PEEK_STATUS_INVALID_SPAN;
            const bool wrapping = gbl_machine_peek(
                &machine, UINT32_MAX - UINT32_C(1), storage.output, 4u) ==
                GBL_PEEK_STATUS_INVALID_SPAN;
            const bool overlap_machine = gbl_machine_peek(
                &machine, UINT32_C(0x03000000), &machine, 4u) ==
                GBL_PEEK_STATUS_OVERLAP;
            const bool overlap_rom = gbl_machine_peek(
                &machine, UINT32_C(0x03000000), machine.rom, 4u) ==
                GBL_PEEK_STATUS_OVERLAP;
            gbl_machine uninitialized = { 0 };
            const bool uninitialized_refused = gbl_machine_peek(
                &uninitialized, UINT32_C(0x03000000), storage.output, 4u) ==
                GBL_PEEK_STATUS_INVALID_MACHINE;
            const bool null_machine = gbl_machine_peek(
                NULL, UINT32_C(0x03000000), storage.output, 4u) ==
                GBL_PEEK_STATUS_INVALID_ARGUMENT;

            test_case(zero_end && zero_start && null_output && outside &&
                          wrapping && overlap_machine && overlap_rom &&
                          uninitialized_refused && null_machine &&
                          memcmp(storage.output, output_before,
                                 sizeof(output_before)) == 0 &&
                          gbl_test_canonical_equal(&before, &machine) &&
                          digest_before ==
                              gbl_test_canonical_digest(&machine) &&
                          gbl_test_same_instance_identity_equal(&before,
                                                                &machine),
                      "peek validates zero-length, wrap, overlap, and null boundaries before effects");
        }
    }

    {
        gbl_machine failure = { 0 };
        gbl_run_result failure_result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool failure_loaded;
        bool failure_ran;
        gbl_mailbox_result failed;

        make_mailbox_guest(image, GBL_MAILBOX_STATE_FAIL,
                           TEST_MAILBOX_FAIL_CASE);
        failure_loaded = machine_load(&failure, image);
        failure_ran = failure_loaded && run_units(
            &failure, UINT64_C(24), 0u, &failure_result);
        failed = failure_loaded ? gbl_mailbox_read(&failure)
                                : (gbl_mailbox_result){
                                      GBL_MAILBOX_MALFORMED, 0u
                                  };
        test_case(failure_ran && failure_result.cycles == UINT64_C(24) &&
                      failed.status == GBL_MAILBOX_FAIL &&
                      failed.case_id == TEST_MAILBOX_FAIL_CASE &&
                      gbl_mailbox_read(&failure).status == GBL_MAILBOX_FAIL &&
                      gbl_mailbox_read(&failure).case_id ==
                          TEST_MAILBOX_FAIL_CASE,
                  "guest FAIL verdict preserves its case ID across completed reads");
        gbl_machine_dispose(&failure);
    }

    gbl_machine_dispose(&machine);
}

static void test_mailbox_statuses(void)
{
    gbl_machine machine = { 0 };
    bool initialized = machine_initialize(&machine, NULL);

    test_case(initialized &&
                  gbl_mailbox_read(&machine).status == GBL_MAILBOX_ABSENT,
              "zero-filled IWRAM reports mailbox absent");
    if (initialized) {
        write_mailbox(&machine, GBL_MAILBOX_MAGIC, GBL_MAILBOX_VERSION,
                      GBL_MAILBOX_STATE_ABSENT, 0u, 0u);
        initialized = gbl_mailbox_read(&machine).status == GBL_MAILBOX_ABSENT;
        write_mailbox(&machine, GBL_MAILBOX_MAGIC ^ UINT32_C(1),
                      GBL_MAILBOX_VERSION, GBL_MAILBOX_STATE_PASS, 7u,
                      GBL_MAILBOX_COMPLETION_MARKER);
        const bool bad_magic =
            gbl_mailbox_read(&machine).status == GBL_MAILBOX_MALFORMED;
        write_mailbox(&machine, GBL_MAILBOX_MAGIC,
                      GBL_MAILBOX_VERSION + UINT32_C(1),
                      GBL_MAILBOX_STATE_PASS, 7u,
                      GBL_MAILBOX_COMPLETION_MARKER);
        test_case(initialized && bad_magic && gbl_mailbox_read(&machine).status ==
                                   GBL_MAILBOX_MALFORMED,
                  "valid absent, wrong magic, and wrong version are classified distinctly");

        write_mailbox(&machine, GBL_MAILBOX_MAGIC, GBL_MAILBOX_VERSION,
                      GBL_MAILBOX_STATE_RUNNING, 7u, 0u);
        const bool running =
            gbl_mailbox_read(&machine).status == GBL_MAILBOX_PENDING;
        write_mailbox(&machine, GBL_MAILBOX_MAGIC, GBL_MAILBOX_VERSION,
                      GBL_MAILBOX_STATE_PASS, 7u,
                      UINT32_C(0x434F4D00));
        test_case(running && gbl_mailbox_read(&machine).status ==
                                 GBL_MAILBOX_PENDING,
                  "running and partially written terminal mailboxes stay pending");

        write_mailbox(&machine, GBL_MAILBOX_MAGIC, GBL_MAILBOX_VERSION,
                      UINT32_C(99), 7u, GBL_MAILBOX_COMPLETION_MARKER);
        const bool invalid_state =
            gbl_mailbox_read(&machine).status == GBL_MAILBOX_MALFORMED;
        write_mailbox(&machine, GBL_MAILBOX_MAGIC, GBL_MAILBOX_VERSION,
                      GBL_MAILBOX_STATE_FAIL, 0u,
                      GBL_MAILBOX_COMPLETION_MARKER);
        const bool fail_without_id =
            gbl_mailbox_read(&machine).status == GBL_MAILBOX_MALFORMED;
        write_mailbox(&machine, GBL_MAILBOX_MAGIC, GBL_MAILBOX_VERSION,
                      GBL_MAILBOX_STATE_PASS, 0u,
                      GBL_MAILBOX_COMPLETION_MARKER);
        test_case(invalid_state && fail_without_id &&
                      gbl_mailbox_read(&machine).status ==
                          GBL_MAILBOX_MALFORMED,
                  "invalid state and completed PASS/FAIL without case IDs are malformed");
    } else {
        test_case(false, "wrong magic and version never produce a verdict");
        test_case(false, "running and partially written terminal mailboxes stay pending");
        test_case(false, "invalid state and completed PASS/FAIL without case IDs are malformed");
    }
    gbl_machine_dispose(&machine);
}

static void seed_trace_store_machine(gbl_machine *machine)
{
    uint8_t image[TEST_IMAGE_SIZE];

    make_empty_image(image);
    write_word(image, 0u, UINT32_C(0xE5810000)); /* STR r0, [r1] */
    (void)machine_load(machine, image);
    machine->cpu.execution_address = UINT32_C(0x08000000);
    machine->cpu.cpsr = TEST_CPSR_SYSTEM;
    machine->cpu.r0_7[0] = UINT32_C(0x12345678);
    machine->cpu.r0_7[1] = UINT32_C(0x02000000);
    machine->cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000000), UINT32_C(0xE5810000), 4u, true
    };
    machine->cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000004), UINT32_C(0xE1A00000), 4u, true
    };
    machine->cpu.next_fetch_sequential = true;
}

static bool trace_record_is(const gbl_trace_record *record,
                            uint64_t sequence, uint64_t guest_cycle,
                            gbl_trace_category category,
                            uint32_t instruction_address, uint32_t opcode,
                            uint32_t address, uint32_t value, uint8_t width,
                            gbl_bus_access_class access_class,
                            gbl_bus_outcome bus_outcome,
                            gbl_exception_kind exception_kind)
{
    return record->sequence == sequence &&
           record->guest_cycle == guest_cycle &&
           record->category == category &&
           record->instruction_address == instruction_address &&
           record->opcode == opcode && record->address == address &&
           record->value == value && record->width == width &&
           record->access_class == access_class &&
           record->bus_outcome == bus_outcome &&
           record->exception_kind == exception_kind;
}

static bool trace_records_match(const gbl_trace_record *actual,
                                const gbl_trace_record *expected,
                                size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        if (!trace_record_is(
                &actual[index], expected[index].sequence,
                expected[index].guest_cycle, expected[index].category,
                expected[index].instruction_address, expected[index].opcode,
                expected[index].address, expected[index].value,
                expected[index].width, expected[index].access_class,
                expected[index].bus_outcome,
                expected[index].exception_kind)) {
            return false;
        }
    }
    return true;
}

static bool prepare_swp_machine(gbl_machine *machine, uint32_t opcode,
                                uint32_t address)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_run_result refill_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    size_t index;

    make_empty_image(image);
    write_word(image, 0x00u, opcode);
    write_word(image, 0x04u, UINT32_C(0xE1A00000));
    write_word(image, 0x08u, UINT32_C(0xE1A00000));
    if (!machine_load(machine, image)) {
        return false;
    }

    machine->iwram[0x40u] = UINT8_C(0x11);
    machine->iwram[0x41u] = UINT8_C(0x22);
    machine->iwram[0x42u] = UINT8_C(0x33);
    machine->iwram[0x43u] = UINT8_C(0x44);
    machine->cpu.cpsr = TEST_CPSR_SYSTEM | TEST_CPSR_N | TEST_CPSR_C;
    machine->cpu.r0_7[0] = address;
    machine->cpu.r0_7[1] = UINT32_C(0xDEADBEEF);
    machine->cpu.r0_7[2] = UINT32_C(0xA1B2C3D4);
    for (index = 3u; index < 8u; ++index) {
        machine->cpu.r0_7[index] = UINT32_C(0x11000000) + (uint32_t)index;
    }
    for (index = 0u; index < 7u; ++index) {
        machine->cpu.usr_r8_14[index] =
            UINT32_C(0x22000000) + (uint32_t)index;
        machine->cpu.fiq_r8_14[index] =
            UINT32_C(0x33000000) + (uint32_t)index;
    }
    for (index = 0u; index < 2u; ++index) {
        machine->cpu.irq_r13_14[index] =
            UINT32_C(0x44000000) + (uint32_t)index;
        machine->cpu.svc_r13_14[index] =
            UINT32_C(0x55000000) + (uint32_t)index;
        machine->cpu.abt_r13_14[index] =
            UINT32_C(0x66000000) + (uint32_t)index;
        machine->cpu.und_r13_14[index] =
            UINT32_C(0x77000000) + (uint32_t)index;
    }
    for (index = 0u; index < 5u; ++index) {
        machine->cpu.spsr[index] = UINT32_C(0x88000000) + (uint32_t)index;
    }

    if (!run_units(machine, 2u, 1u, &refill_result) ||
        refill_result.termination != GBL_RUN_PROGRESS ||
        refill_result.reason != GBL_RUN_REASON_UNIT_LIMIT ||
        refill_result.cycles != 2u || refill_result.instructions != 0u ||
        machine->cpu.pipeline[0].opcode != opcode ||
        machine->cpu.pipeline[1].opcode != UINT32_C(0xE1A00000)) {
        return false;
    }

    /* Keep a non-default latch value so every refusal checks it too. */
    machine->open_bus = UINT32_C(0x13579BDF);
    return true;
}

static bool prepare_word_swp_machine(gbl_machine *machine, uint32_t address)
{
    return prepare_swp_machine(machine, UINT32_C(0xE1001092), address);
}

static const gbl_trace_record word_swp_trace_sentinel = {
    .sequence = UINT64_C(91),
    .guest_cycle = UINT64_C(73),
    .category = GBL_TRACE_BUS,
    .instruction_address = UINT32_C(0x08000070),
    .opcode = UINT32_C(0xE1A00000),
    .address = UINT32_C(0x08000074),
    .value = UINT32_C(0x12345678),
    .width = 4u,
    .access_class = GBL_BUS_CLASS_SEQUENTIAL,
    .bus_outcome = GBL_BUS_OUTCOME_ROM,
    .exception_kind = GBL_EXCEPTION_NONE
};

static bool word_swp_refusal_unchanged(uint64_t cycle_budget,
                                       size_t trace_capacity,
                                       uint64_t next_sequence,
                                       gbl_run_termination termination,
                                       gbl_run_reason reason)
{
    gbl_machine machine = { 0 };
    gbl_trace_record records[5];
    gbl_trace_record prior_records[5];
    gbl_trace_buffer trace = { records, trace_capacity, 1u, next_sequence };
    gbl_run_result result = { GBL_RUN_PROGRESS, GBL_RUN_REASON_NONE, 73u, 91u };
    const gbl_run_options options = {
        .cycle_budget = cycle_budget,
        .unit_limit = 1u,
        .category_mask = GBL_TRACE_CATEGORY_MASK,
        .trace_buffer = &trace
    };
    size_t index;
    bool valid;

    for (index = 0u; index < 5u; ++index) {
        records[index] = word_swp_trace_sentinel;
        prior_records[index] = word_swp_trace_sentinel;
    }
    if (!prepare_word_swp_machine(&machine, TEST_SWP_ADDRESS)) {
        gbl_machine_dispose(&machine);
        return false;
    }
    {
        const gbl_machine before = machine;
        const gbl_trace_buffer before_trace = trace;
        const bool ran = gbl_cpu_run(&machine, &options, &result) ==
                         GBL_RUN_STATUS_OK;

        valid = ran && result.termination == termination &&
                result.reason == reason && result.cycles == 0u &&
                result.instructions == 0u &&
                trace.records == before_trace.records &&
                trace.capacity == before_trace.capacity &&
                trace.count == before_trace.count &&
                trace.next_sequence == before_trace.next_sequence &&
                trace_records_match(records, prior_records, 5u) &&
                gbl_test_canonical_equal(&before, &machine) &&
                gbl_test_canonical_digest(&before) ==
                    gbl_test_canonical_digest(&machine) &&
                gbl_test_same_instance_identity_equal(&before, &machine);
    }
    gbl_machine_dispose(&machine);
    return valid;
}

static void test_word_swp_stream_setup(void)
{
    gbl_machine machine = { 0 };
    const bool valid = prepare_word_swp_machine(&machine, TEST_SWP_ADDRESS) &&
                       machine.cpu.pipeline[0].address ==
                           UINT32_C(0x08000000) &&
                       machine.cpu.pipeline[0].opcode ==
                           UINT32_C(0xE1001092) &&
                       machine.cpu.pipeline[1].address ==
                           UINT32_C(0x08000004) &&
                       machine.master_cycles == 2u;

    test_case(valid, "SWP fixture loads and refills from the original ARM stream");
    gbl_machine_dispose(&machine);
}

static void test_word_swp_read_write_trace(void)
{
    gbl_machine machine = { 0 };
    gbl_trace_record records[5];
    gbl_trace_buffer trace = {
        records, 5u, 1u, UINT64_MAX - UINT64_C(4)
    };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    size_t index;
    bool prepared;
    bool ran;

    for (index = 0u; index < 5u; ++index) {
        records[index] = word_swp_trace_sentinel;
    }
    prepared = prepare_word_swp_machine(&machine, TEST_SWP_ADDRESS);
    ran = prepared && run_traced_units(
        &machine, 3u, 1u, GBL_TRACE_CATEGORY_MASK, &trace, &result);
    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                  result.cycles == 3u && result.instructions == 1u &&
                  machine.master_cycles == 5u &&
                  machine.cpu.r0_7[1] == UINT32_C(0x11443322) &&
                  machine.cpu.r0_7[2] == UINT32_C(0xA1B2C3D4) &&
                  machine.iwram[0x40u] == UINT8_C(0xD4) &&
                  machine.iwram[0x41u] == UINT8_C(0xC3) &&
                  machine.iwram[0x42u] == UINT8_C(0xB2) &&
                  machine.iwram[0x43u] == UINT8_C(0xA1) &&
                  trace.count == 5u && trace.next_sequence == UINT64_MAX &&
                  trace_record_is(
                      &records[0], word_swp_trace_sentinel.sequence,
                      word_swp_trace_sentinel.guest_cycle,
                      word_swp_trace_sentinel.category,
                      word_swp_trace_sentinel.instruction_address,
                      word_swp_trace_sentinel.opcode,
                      word_swp_trace_sentinel.address,
                      word_swp_trace_sentinel.value,
                      word_swp_trace_sentinel.width,
                      word_swp_trace_sentinel.access_class,
                      word_swp_trace_sentinel.bus_outcome,
                      word_swp_trace_sentinel.exception_kind) &&
                  trace_record_is(&records[1], UINT64_MAX - UINT64_C(4),
                                  2u, GBL_TRACE_INSTRUCTION,
                                  UINT32_C(0x08000000), UINT32_C(0xE1001092),
                                  0u, 0u, 0u, GBL_BUS_CLASS_NONSEQUENTIAL,
                                  GBL_BUS_OUTCOME_NONE, GBL_EXCEPTION_NONE) &&
                  trace_record_is(&records[2], UINT64_MAX - UINT64_C(3),
                                  3u, GBL_TRACE_BUS,
                                  UINT32_C(0x08000000), UINT32_C(0xE1001092),
                                  UINT32_C(0x03000040), UINT32_C(0x44332211),
                                  4u, GBL_BUS_CLASS_NONSEQUENTIAL,
                                  GBL_BUS_OUTCOME_RAM, GBL_EXCEPTION_NONE) &&
                  trace_record_is(&records[3], UINT64_MAX - UINT64_C(2),
                                  4u, GBL_TRACE_BUS,
                                  UINT32_C(0x08000000), UINT32_C(0xE1001092),
                                  UINT32_C(0x03000040), 0u, 4u,
                                  GBL_BUS_CLASS_NONSEQUENTIAL,
                                  GBL_BUS_OUTCOME_RAM, GBL_EXCEPTION_NONE) &&
                  trace_record_is(&records[4], UINT64_MAX - UINT64_C(1),
                                  5u, GBL_TRACE_BUS,
                                  UINT32_C(0x08000000), UINT32_C(0xE1001092),
                                  UINT32_C(0x08000008), UINT32_C(0xE1A00000),
                                  4u, GBL_BUS_CLASS_SEQUENTIAL,
                                  GBL_BUS_OUTCOME_ROM, GBL_EXCEPTION_NONE),
              "word SWP reads aligned word, rotates requested lane, then writes captured Rm");
    gbl_machine_dispose(&machine);
}

static void test_word_swp_cycle_refusal(void)
{
    test_case(word_swp_refusal_unchanged(
                  2u, 5u, UINT64_C(17), GBL_RUN_TIMEOUT,
                  GBL_RUN_REASON_CYCLE_BUDGET),
              "word SWP one-short cycle budget leaves machine and trace untouched");
}

static void test_word_swp_trace_refusal(void)
{
    test_case(word_swp_refusal_unchanged(
                  3u, 4u, UINT64_C(17), GBL_RUN_TRACE_OUTPUT_FULL,
                  GBL_RUN_REASON_TRACE_OUTPUT_FULL),
              "word SWP one-short trace capacity leaves machine and records untouched");
}

static void test_word_swp_sequence_refusal(void)
{
    test_case(word_swp_refusal_unchanged(
                  3u, 5u, UINT64_MAX - UINT64_C(3),
                  GBL_RUN_TRACE_OUTPUT_FULL,
                  GBL_RUN_REASON_TRACE_OUTPUT_FULL),
              "word SWP one-short trace sequence leaves machine and records untouched");
}

static bool swp_alias_parity_case(uint32_t opcode, uint32_t address,
                                  uint32_t expected_r0,
                                  uint32_t expected_r1, uint32_t expected_r2,
                                  uint32_t expected_memory)
{
    gbl_machine traced = { 0 };
    gbl_machine plain = { 0 };
    gbl_trace_record records[5] = { { 0 } };
    gbl_trace_buffer trace = { records, 5u, 0u, UINT64_C(100) };
    gbl_run_result traced_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    gbl_run_result plain_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    const bool traced_loaded = prepare_swp_machine(&traced, opcode, address);
    const bool plain_loaded = prepare_swp_machine(&plain, opcode, address);
    const bool traced_ran = traced_loaded && run_traced_units(
        &traced, 3u, 1u, GBL_TRACE_CATEGORY_MASK, &trace, &traced_result);
    const bool plain_ran = plain_loaded &&
                           run_units(&plain, 3u, 1u, &plain_result);
    const bool valid = traced_ran && plain_ran &&
        traced_result.termination == GBL_RUN_PROGRESS &&
        traced_result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
        traced_result.cycles == 3u && traced_result.instructions == 1u &&
        plain_result.termination == traced_result.termination &&
        plain_result.reason == traced_result.reason &&
        plain_result.cycles == traced_result.cycles &&
        plain_result.instructions == traced_result.instructions &&
        traced.cpu.r0_7[0] == expected_r0 &&
        traced.cpu.r0_7[1] == expected_r1 &&
        traced.cpu.r0_7[2] == expected_r2 &&
        traced.iwram[0x40u] == (uint8_t)(expected_memory & UINT32_C(0xFF)) &&
        traced.iwram[0x41u] ==
            (uint8_t)((expected_memory >> 8u) & UINT32_C(0xFF)) &&
        traced.iwram[0x42u] ==
            (uint8_t)((expected_memory >> 16u) & UINT32_C(0xFF)) &&
        traced.iwram[0x43u] ==
            (uint8_t)((expected_memory >> 24u) & UINT32_C(0xFF)) &&
        trace.count == 4u && trace.next_sequence == UINT64_C(104) &&
        gbl_test_canonical_equal(&traced, &plain) &&
        gbl_test_canonical_digest(&traced) ==
            gbl_test_canonical_digest(&plain);

    gbl_machine_dispose(&traced);
    gbl_machine_dispose(&plain);
    return valid;
}

static void test_swp_register_aliases(void)
{
    const bool valid =
        swp_alias_parity_case(UINT32_C(0xE1002092), TEST_SWP_ADDRESS,
                              TEST_SWP_ADDRESS, UINT32_C(0xDEADBEEF),
                              UINT32_C(0x11443322), UINT32_C(0xA1B2C3D4)) &&
        swp_alias_parity_case(UINT32_C(0xE1000092), TEST_SWP_ADDRESS,
                              UINT32_C(0x11443322), UINT32_C(0xDEADBEEF),
                              UINT32_C(0xA1B2C3D4), UINT32_C(0xA1B2C3D4)) &&
        swp_alias_parity_case(UINT32_C(0xE1001090), TEST_SWP_ADDRESS,
                              TEST_SWP_ADDRESS, UINT32_C(0x11443322),
                              UINT32_C(0xA1B2C3D4), TEST_SWP_ADDRESS);

    test_case(valid,
              "SWP captures operands before Rd equals Rm, Rn, or both source roles");
}

static bool word_swp_lane_case(uint32_t address, uint32_t expected_rd)
{
    gbl_machine machine = { 0 };
    gbl_trace_record records[5] = { { 0 } };
    gbl_trace_buffer trace = { records, 5u, 0u, 0u };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    const bool prepared = prepare_word_swp_machine(&machine, address);
    const bool ran = prepared && run_traced_units(
        &machine, 3u, 1u, GBL_TRACE_CATEGORY_MASK, &trace, &result);
    const bool valid = ran &&
        result.termination == GBL_RUN_PROGRESS &&
        result.reason == GBL_RUN_REASON_UNIT_LIMIT && result.cycles == 3u &&
        result.instructions == 1u && machine.cpu.r0_7[0] == address &&
        machine.cpu.r0_7[1] == expected_rd &&
        machine.cpu.r0_7[2] == UINT32_C(0xA1B2C3D4) &&
        machine.iwram[0x40u] == UINT8_C(0xD4) &&
        machine.iwram[0x41u] == UINT8_C(0xC3) &&
        machine.iwram[0x42u] == UINT8_C(0xB2) &&
        machine.iwram[0x43u] == UINT8_C(0xA1) &&
        trace.count == 4u && trace.next_sequence == 4u &&
        trace_record_is(&records[0], 0u, 2u, GBL_TRACE_INSTRUCTION,
                        UINT32_C(0x08000000), UINT32_C(0xE1001092), 0u, 0u,
                        0u, GBL_BUS_CLASS_NONSEQUENTIAL,
                        GBL_BUS_OUTCOME_NONE, GBL_EXCEPTION_NONE) &&
        trace_record_is(&records[1], 1u, 3u, GBL_TRACE_BUS,
                        UINT32_C(0x08000000), UINT32_C(0xE1001092),
                        TEST_SWP_BASE_ADDRESS, UINT32_C(0x44332211), 4u,
                        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_OUTCOME_RAM,
                        GBL_EXCEPTION_NONE) &&
        trace_record_is(&records[2], 2u, 4u, GBL_TRACE_BUS,
                        UINT32_C(0x08000000), UINT32_C(0xE1001092),
                        TEST_SWP_BASE_ADDRESS, 0u, 4u,
                        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_OUTCOME_RAM,
                        GBL_EXCEPTION_NONE) &&
        trace_record_is(&records[3], 3u, 5u, GBL_TRACE_BUS,
                        UINT32_C(0x08000000), UINT32_C(0xE1001092),
                        UINT32_C(0x08000008), UINT32_C(0xE1A00000), 4u,
                        GBL_BUS_CLASS_SEQUENTIAL, GBL_BUS_OUTCOME_ROM,
                        GBL_EXCEPTION_NONE);

    gbl_machine_dispose(&machine);
    return valid;
}

static void test_swp_word_lane_results(void)
{
    static const struct word_swp_lane_vector {
        uint32_t address;
        uint32_t expected_rd;
        const char *description;
    } cases[] = {
        { TEST_SWP_BASE_ADDRESS, UINT32_C(0x44332211),
          "word SWP lane 0 returns the independently pinned word" },
        { TEST_SWP_BASE_ADDRESS + 1u, UINT32_C(0x11443322),
          "word SWP lane 1 returns the independently pinned word" },
        { TEST_SWP_BASE_ADDRESS + 2u, UINT32_C(0x22114433),
          "word SWP lane 2 returns the independently pinned word" },
        { TEST_SWP_BASE_ADDRESS + 3u, UINT32_C(0x33221144),
          "word SWP lane 3 returns the independently pinned word" }
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        test_case(word_swp_lane_case(cases[index].address,
                                     cases[index].expected_rd),
                  cases[index].description);
    }
}

static void test_swpb_read_write_trace(void)
{
    gbl_machine machine = { 0 };
    gbl_trace_record records[5] = { { 0 } };
    gbl_trace_buffer trace = { records, 5u, 1u, UINT64_C(70) };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    const bool prepared = prepare_swp_machine(
        &machine, UINT32_C(0xE1401092), TEST_SWP_ADDRESS);
    const bool ran = prepared && run_traced_units(
        &machine, 3u, 1u, GBL_TRACE_CATEGORY_MASK, &trace, &result);

    test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                  result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                  result.cycles == 3u && result.instructions == 1u &&
                  machine.master_cycles == 5u &&
                  machine.cpu.r0_7[1] == UINT32_C(0x22) &&
                  machine.cpu.r0_7[2] == UINT32_C(0xA1B2C3D4) &&
                  machine.iwram[0x40u] == UINT8_C(0x11) &&
                  machine.iwram[0x41u] == UINT8_C(0xD4) &&
                  machine.iwram[0x42u] == UINT8_C(0x33) &&
                  machine.iwram[0x43u] == UINT8_C(0x44) &&
                  trace.count == 5u && trace.next_sequence == UINT64_C(74) &&
                  trace_record_is(
                      &records[1], UINT64_C(70), 2u, GBL_TRACE_INSTRUCTION,
                      UINT32_C(0x08000000), UINT32_C(0xE1401092), 0u, 0u,
                      0u, GBL_BUS_CLASS_NONSEQUENTIAL,
                      GBL_BUS_OUTCOME_NONE, GBL_EXCEPTION_NONE) &&
                  trace_record_is(
                      &records[2], UINT64_C(71), 3u, GBL_TRACE_BUS,
                      UINT32_C(0x08000000), UINT32_C(0xE1401092),
                      UINT32_C(0x03000041), UINT32_C(0x22), 1u,
                      GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_OUTCOME_RAM,
                      GBL_EXCEPTION_NONE) &&
                  trace_record_is(
                      &records[3], UINT64_C(72), 4u, GBL_TRACE_BUS,
                      UINT32_C(0x08000000), UINT32_C(0xE1401092),
                      UINT32_C(0x03000041), 0u, 1u,
                      GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_OUTCOME_RAM,
                      GBL_EXCEPTION_NONE) &&
                  trace_record_is(
                      &records[4], UINT64_C(73), 5u, GBL_TRACE_BUS,
                      UINT32_C(0x08000000), UINT32_C(0xE1401092),
                      UINT32_C(0x08000008), UINT32_C(0xE1A00000), 4u,
                      GBL_BUS_CLASS_SEQUENTIAL, GBL_BUS_OUTCOME_ROM,
                      GBL_EXCEPTION_NONE),
              "SWPB exchanges the addressed byte with read-before-write trace");
    gbl_machine_dispose(&machine);
}

static bool swpb_alias_case(uint32_t opcode, uint32_t expected_r0,
                            uint32_t expected_r1, uint32_t expected_r2,
                            uint8_t expected_memory_byte)
{
    gbl_machine machine = { 0 };
    gbl_trace_record records[5] = {
        word_swp_trace_sentinel, { 0 }, { 0 }, { 0 }, { 0 }
    };
    const gbl_trace_record before_prefix = word_swp_trace_sentinel;
    gbl_trace_buffer trace = { records, 5u, 1u, UINT64_C(70) };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    const bool prepared = prepare_swp_machine(&machine, opcode,
                                               TEST_SWP_ADDRESS);
    const bool ran = prepared && run_traced_units(
        &machine, 3u, 1u, GBL_TRACE_CATEGORY_MASK, &trace, &result);
    const bool valid = ran &&
        result.termination == GBL_RUN_PROGRESS &&
        result.reason == GBL_RUN_REASON_UNIT_LIMIT && result.cycles == 3u &&
        result.instructions == 1u && machine.cpu.r0_7[0] == expected_r0 &&
        machine.cpu.r0_7[1] == expected_r1 &&
        machine.cpu.r0_7[2] == expected_r2 &&
        machine.iwram[0x40u] == UINT8_C(0x11) &&
        machine.iwram[0x41u] == expected_memory_byte &&
        machine.iwram[0x42u] == UINT8_C(0x33) &&
        machine.iwram[0x43u] == UINT8_C(0x44) &&
        trace.count == 5u && trace.next_sequence == UINT64_C(74) &&
        trace_record_is(&records[0], before_prefix.sequence,
                        before_prefix.guest_cycle, before_prefix.category,
                        before_prefix.instruction_address,
                        before_prefix.opcode, before_prefix.address,
                        before_prefix.value, before_prefix.width,
                        before_prefix.access_class, before_prefix.bus_outcome,
                        before_prefix.exception_kind) &&
        trace_record_is(&records[1], UINT64_C(70), 2u,
                        GBL_TRACE_INSTRUCTION, UINT32_C(0x08000000), opcode,
                        0u, 0u, 0u, GBL_BUS_CLASS_NONSEQUENTIAL,
                        GBL_BUS_OUTCOME_NONE, GBL_EXCEPTION_NONE) &&
        trace_record_is(&records[2], UINT64_C(71), 3u, GBL_TRACE_BUS,
                        UINT32_C(0x08000000), opcode, TEST_SWP_ADDRESS,
                        UINT32_C(0x22), 1u,
                        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_OUTCOME_RAM,
                        GBL_EXCEPTION_NONE) &&
        trace_record_is(&records[3], UINT64_C(72), 4u, GBL_TRACE_BUS,
                        UINT32_C(0x08000000), opcode, TEST_SWP_ADDRESS, 0u,
                        1u, GBL_BUS_CLASS_NONSEQUENTIAL,
                        GBL_BUS_OUTCOME_RAM, GBL_EXCEPTION_NONE) &&
        trace_record_is(&records[4], UINT64_C(73), 5u, GBL_TRACE_BUS,
                        UINT32_C(0x08000000), opcode,
                        UINT32_C(0x08000008), UINT32_C(0xE1A00000), 4u,
                        GBL_BUS_CLASS_SEQUENTIAL, GBL_BUS_OUTCOME_ROM,
                        GBL_EXCEPTION_NONE);

    gbl_machine_dispose(&machine);
    return valid;
}

static void test_swpb_register_aliases(void)
{
    test_case(swpb_alias_case(UINT32_C(0xE1402092), TEST_SWP_ADDRESS,
                              UINT32_C(0xDEADBEEF), UINT32_C(0x22),
                              UINT8_C(0xD4)),
              "SWPB captures byte address and Rm before Rd equals Rm");
    test_case(swpb_alias_case(UINT32_C(0xE1400092), UINT32_C(0x22),
                              UINT32_C(0xDEADBEEF), UINT32_C(0xA1B2C3D4),
                              UINT8_C(0xD4)),
              "SWPB captures byte address and Rm before Rd equals Rn");
    test_case(swpb_alias_case(UINT32_C(0xE1401090), TEST_SWP_ADDRESS,
                              UINT32_C(0x22), UINT32_C(0xA1B2C3D4),
                              UINT8_C(0x41)),
              "SWPB captures aliased Rn/Rm before read and byte write");
}

static void test_swp_pc_restrictions(void)
{
    const uint32_t opcodes[] = {
        UINT32_C(0xE10F1092), /* Rn = PC */
        UINT32_C(0xE100F092), /* Rd = PC */
        UINT32_C(0xE100109F)  /* Rm = PC */
    };
    size_t index;
    bool valid = true;

    for (index = 0u; index < sizeof(opcodes) / sizeof(opcodes[0]); ++index) {
        gbl_machine machine = { 0 };
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool prepared = prepare_swp_machine(&machine, opcodes[index],
                                            TEST_SWP_ADDRESS);

        if (prepared) {
            const gbl_machine before = machine;
            const bool ran = run_units(&machine, 3u, 1u, &result);
            gbl_machine after = machine;

            after.cpu.stopped = before.cpu.stopped;
            after.cpu.stop_reason = before.cpu.stop_reason;
            valid = valid && ran && result.termination == GBL_RUN_STOPPED &&
                result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
                result.cycles == 0u && result.instructions == 0u &&
                machine.master_cycles == before.master_cycles &&
                machine.cpu.retired_instructions ==
                    before.cpu.retired_instructions &&
                machine.cpu.stopped &&
                machine.cpu.stop_reason == GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION &&
                gbl_test_canonical_equal(&before, &after) &&
                gbl_test_canonical_digest(&before) ==
                    gbl_test_canonical_digest(&after);
        } else {
            valid = false;
        }
        gbl_machine_dispose(&machine);
    }
    test_case(valid,
              "SWP classifies PC in Rn, Rd, or Rm as unsupported before effects");
}

static bool swp_refusal_unchanged(uint64_t cycle_budget,
                                  size_t trace_capacity,
                                  uint64_t next_sequence,
                                  uint32_t category_mask,
                                  uint64_t master_cycles,
                                  uint64_t retired_instructions,
                                  gbl_run_termination termination,
                                  gbl_run_reason reason)
{
    gbl_machine machine = { 0 };
    gbl_trace_record records[5];
    gbl_trace_record prior_records[5];
    gbl_trace_buffer trace = { records, trace_capacity, 1u, next_sequence };
    gbl_run_result result = { GBL_RUN_PROGRESS, GBL_RUN_REASON_NONE, 73u, 91u };
    const gbl_run_options options = {
        .cycle_budget = cycle_budget,
        .unit_limit = 1u,
        .category_mask = category_mask,
        .trace_buffer = &trace
    };
    size_t index;
    bool valid;

    for (index = 0u; index < 5u; ++index) {
        records[index] = word_swp_trace_sentinel;
        prior_records[index] = word_swp_trace_sentinel;
    }
    if (!prepare_word_swp_machine(&machine, TEST_SWP_ADDRESS)) {
        gbl_machine_dispose(&machine);
        return false;
    }
    machine.master_cycles = master_cycles;
    machine.cpu.retired_instructions = retired_instructions;
    {
        const gbl_machine before = machine;
        const gbl_trace_buffer before_trace = trace;
        const bool ran = gbl_cpu_run(&machine, &options, &result) ==
                         GBL_RUN_STATUS_OK;

        valid = ran && result.termination == termination &&
                result.reason == reason && result.cycles == 0u &&
                result.instructions == 0u &&
                trace.records == before_trace.records &&
                trace.capacity == before_trace.capacity &&
                trace.count == before_trace.count &&
                trace.next_sequence == before_trace.next_sequence &&
                trace_records_match(records, prior_records, 5u) &&
                gbl_test_canonical_equal(&before, &machine) &&
                gbl_test_canonical_digest(&before) ==
                    gbl_test_canonical_digest(&machine) &&
                gbl_test_same_instance_identity_equal(&before, &machine);
    }
    gbl_machine_dispose(&machine);
    return valid;
}

static void test_swp_selected_trace_boundaries(void)
{
    gbl_machine traced = { 0 };
    gbl_machine plain = { 0 };
    gbl_trace_record records[4] = {
        word_swp_trace_sentinel, { 0 }, { 0 }, { 0 }
    };
    gbl_trace_buffer trace = {
        records, 4u, 1u, UINT64_MAX - UINT64_C(3)
    };
    gbl_run_result traced_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    gbl_run_result plain_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    const bool traced_loaded =
        prepare_word_swp_machine(&traced, TEST_SWP_ADDRESS);
    const bool plain_loaded =
        prepare_word_swp_machine(&plain, TEST_SWP_ADDRESS);
    const bool traced_ran = traced_loaded && run_traced_units(
        &traced, 3u, 1u, (uint32_t)GBL_TRACE_BUS, &trace, &traced_result);
    const bool plain_ran = plain_loaded &&
                           run_units(&plain, 3u, 1u, &plain_result);
    const bool exact = traced_ran && plain_ran &&
        traced_result.termination == GBL_RUN_PROGRESS &&
        traced_result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
        traced_result.cycles == 3u && traced_result.instructions == 1u &&
        plain_result.termination == traced_result.termination &&
        plain_result.reason == traced_result.reason &&
        plain_result.cycles == traced_result.cycles &&
        plain_result.instructions == traced_result.instructions &&
        trace.count == 4u && trace.next_sequence == UINT64_MAX &&
        trace_record_is(
            &records[0], word_swp_trace_sentinel.sequence,
            word_swp_trace_sentinel.guest_cycle,
            word_swp_trace_sentinel.category,
            word_swp_trace_sentinel.instruction_address,
            word_swp_trace_sentinel.opcode, word_swp_trace_sentinel.address,
            word_swp_trace_sentinel.value, word_swp_trace_sentinel.width,
            word_swp_trace_sentinel.access_class,
            word_swp_trace_sentinel.bus_outcome,
            word_swp_trace_sentinel.exception_kind) &&
        trace_record_is(&records[1], UINT64_MAX - UINT64_C(3), 3u,
                        GBL_TRACE_BUS, UINT32_C(0x08000000),
                        UINT32_C(0xE1001092), UINT32_C(0x03000040),
                        UINT32_C(0x44332211), 4u,
                        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_OUTCOME_RAM,
                        GBL_EXCEPTION_NONE) &&
        trace_record_is(&records[2], UINT64_MAX - UINT64_C(2), 4u,
                        GBL_TRACE_BUS, UINT32_C(0x08000000),
                        UINT32_C(0xE1001092), UINT32_C(0x03000040), 0u, 4u,
                        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_OUTCOME_RAM,
                        GBL_EXCEPTION_NONE) &&
        trace_record_is(&records[3], UINT64_MAX - UINT64_C(1), 5u,
                        GBL_TRACE_BUS, UINT32_C(0x08000000),
                        UINT32_C(0xE1001092), UINT32_C(0x08000008),
                        UINT32_C(0xE1A00000), 4u,
                        GBL_BUS_CLASS_SEQUENTIAL, GBL_BUS_OUTCOME_ROM,
                        GBL_EXCEPTION_NONE) &&
        gbl_test_canonical_equal(&traced, &plain) &&
        gbl_test_canonical_digest(&traced) ==
            gbl_test_canonical_digest(&plain);

    test_case(exact,
              "SWP bus-only trace uses exact-fit capacity and sequence after a prefix");
    test_case(swp_refusal_unchanged(
                  3u, 3u, UINT64_C(17), (uint32_t)GBL_TRACE_BUS, 2u, 0u,
                  GBL_RUN_TRACE_OUTPUT_FULL,
                  GBL_RUN_REASON_TRACE_OUTPUT_FULL),
              "SWP one-short selected trace capacity preserves prefix and machine");
    test_case(swp_refusal_unchanged(
                  3u, 4u, UINT64_MAX - UINT64_C(2),
                  (uint32_t)GBL_TRACE_BUS, 2u, 0u,
                  GBL_RUN_TRACE_OUTPUT_FULL,
                  GBL_RUN_REASON_TRACE_OUTPUT_FULL),
              "SWP one-short selected trace sequence preserves prefix and machine");

    gbl_machine_dispose(&traced);
    gbl_machine_dispose(&plain);
}

static void test_swp_counter_headroom(void)
{
    gbl_machine machine = { 0 };
    gbl_trace_record records[4] = {
        word_swp_trace_sentinel, { 0 }, { 0 }, { 0 }
    };
    gbl_trace_buffer trace = { records, 4u, 1u, UINT64_C(30) };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    const bool prepared = prepare_word_swp_machine(&machine, TEST_SWP_ADDRESS);
    bool exact = false;

    if (prepared) {
        const gbl_run_options options = {
            .cycle_budget = 3u,
            .unit_limit = 1u,
            .category_mask = (uint32_t)GBL_TRACE_BUS,
            .trace_buffer = &trace
        };

        machine.master_cycles = UINT64_MAX - UINT64_C(3);
        machine.cpu.retired_instructions = UINT64_MAX - UINT64_C(1);
        exact = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_OK &&
                result.termination == GBL_RUN_PROGRESS &&
                result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                result.cycles == 3u && result.instructions == 1u &&
                machine.master_cycles == UINT64_MAX &&
                machine.cpu.retired_instructions == UINT64_MAX &&
                machine.cpu.r0_7[1] == UINT32_C(0x11443322) &&
                machine.iwram[0x40u] == UINT8_C(0xD4) &&
                trace.count == 4u && trace.next_sequence == UINT64_C(33);
    }
    test_case(exact,
              "SWP retires at exact master-cycle and instruction-counter headroom");
    gbl_machine_dispose(&machine);

    test_case(swp_refusal_unchanged(
                  3u, 4u, UINT64_C(17), (uint32_t)GBL_TRACE_BUS,
                  UINT64_MAX - UINT64_C(2), 0u, GBL_RUN_STOPPED,
                  GBL_RUN_REASON_COUNTER_OVERFLOW),
              "SWP one-short master-cycle headroom refuses before bus effects");
    test_case(swp_refusal_unchanged(
                  3u, 4u, UINT64_C(17), (uint32_t)GBL_TRACE_BUS, 2u,
                  UINT64_MAX, GBL_RUN_STOPPED,
                  GBL_RUN_REASON_COUNTER_OVERFLOW),
              "SWP exhausted retirement counter refuses before bus effects");
}

typedef struct arm_multiply_signed_test_vector {
    const char *description;
    uint32_t opcode;
    uint32_t rm;
    uint32_t rs;
    uint32_t accumulator_low;
    uint32_t accumulator_high;
    uint32_t expected_low;
    uint32_t expected_high;
    uint32_t initial_flags;
    uint32_t expected_flags;
} arm_multiply_signed_test_vector;

typedef struct arm_multiply_timing_sample {
    uint32_t rs;
    uint8_t signed_m;
    uint8_t unsigned_m;
} arm_multiply_timing_sample;

typedef struct arm_multiply_timing_form {
    const char *name;
    uint32_t opcode;
    uint8_t extra_internal_cycles;
    bool long_multiply;
    bool signed_termination;
} arm_multiply_timing_form;

static const gbl_trace_record multiply_trace_sentinel = {
    .sequence = UINT64_C(91),
    .guest_cycle = UINT64_C(73),
    .category = GBL_TRACE_BUS,
    .instruction_address = UINT32_C(0x08000070),
    .opcode = UINT32_C(0xE1A00000),
    .address = UINT32_C(0x08000074),
    .value = UINT32_C(0x12345678),
    .width = 4u,
    .access_class = GBL_BUS_CLASS_SEQUENTIAL,
    .bus_outcome = GBL_BUS_OUTCOME_ROM,
    .exception_kind = GBL_EXCEPTION_NONE
};

static void prime_arm_instruction(gbl_machine *machine, uint32_t opcode,
                                  uint32_t cpsr)
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
}

static bool prepare_arm_multiply_machine(gbl_machine *machine,
                                         uint32_t opcode)
{
    uint8_t image[TEST_IMAGE_SIZE];

    make_program(image);
    if (!machine_load(machine, image)) {
        return false;
    }
    machine->cpu.r0_7[2] = UINT32_C(1);
    machine->cpu.r0_7[3] = UINT32_C(0x12345678);
    prime_arm_instruction(machine, opcode, 0u);
    return true;
}

static void test_multiply_signed_vectors(void)
{
    static const uint32_t nzcv = TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C |
                                 TEST_CPSR_V;
    static const arm_multiply_signed_test_vector vectors[] = {
        { "SMULLS negative extreme times minus one yields positive 64-bit bits",
          UINT32_C(0xE0942391), UINT32_C(0x80000000), UINT32_MAX, 0u, 0u,
          UINT32_C(0x80000000), 0u, nzcv, TEST_CPSR_C | TEST_CPSR_V },
        { "SMULLS negative extreme times one sign-extends into the high word",
          UINT32_C(0xE0942391), UINT32_C(0x80000000), UINT32_C(1), 0u, 0u,
          UINT32_C(0x80000000), UINT32_MAX,
          TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V },
        { "SMLALS accumulates a mixed-sign negative product as 64-bit bits",
          UINT32_C(0xE0B42391), UINT32_C(0xFFFFFFFE), UINT32_C(3), 0u, 0u,
          UINT32_C(0xFFFFFFFA), UINT32_MAX,
          TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V },
        { "SMLALS wraps the full-width accumulator and sets Z on zero",
          UINT32_C(0xE0B42391), UINT32_C(1), UINT32_C(1), UINT32_MAX,
          UINT32_MAX, 0u, 0u, TEST_CPSR_N | TEST_CPSR_C | TEST_CPSR_V,
          TEST_CPSR_Z | TEST_CPSR_C | TEST_CPSR_V }
    };
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;
    size_t index;

    make_program(image);
    loaded = machine_load(&machine, image);
    for (index = 0u; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        const arm_multiply_signed_test_vector *vector = &vectors[index];
        gbl_run_result result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool ran = false;

        machine.cpu.r0_7[1] = vector->rm;
        machine.cpu.r0_7[2] = vector->accumulator_low;
        machine.cpu.r0_7[3] = vector->rs;
        machine.cpu.r0_7[4] = vector->accumulator_high;
        if (loaded) {
            ran = run_arm_instruction(&machine, vector->opcode,
                                      vector->initial_flags, 16u, &result);
        }
        test_case(loaded && ran &&
                      result.termination == GBL_RUN_PROGRESS &&
                      result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      result.instructions == 1u &&
                      machine.cpu.r0_7[2] == vector->expected_low &&
                      machine.cpu.r0_7[4] == vector->expected_high &&
                      (machine.cpu.cpsr & nzcv) == vector->expected_flags,
                  vector->description);
    }
    gbl_machine_dispose(&machine);
}

static bool multiply_unpredictable_refused(uint32_t opcode)
{
    gbl_machine machine = { 0 };
    gbl_trace_record records[3];
    gbl_trace_record prior_records[3];
    gbl_trace_buffer trace = { records, 3u, 1u, UINT64_C(17) };
    gbl_run_result result = {
        GBL_RUN_PROGRESS, GBL_RUN_REASON_NONE, 73u, 91u
    };
    const gbl_run_options options = {
        .cycle_budget = UINT64_C(16),
        .unit_limit = 1u,
        .category_mask = GBL_TRACE_CATEGORY_MASK,
        .trace_buffer = &trace
    };
    size_t index;
    bool valid;

    for (index = 0u; index < 3u; ++index) {
        records[index] = multiply_trace_sentinel;
        prior_records[index] = multiply_trace_sentinel;
    }
    if (!prepare_arm_multiply_machine(&machine, opcode)) {
        gbl_machine_dispose(&machine);
        return false;
    }
    {
        const gbl_machine before = machine;
        gbl_machine expected = before;
        const bool ran = gbl_cpu_run(&machine, &options, &result) ==
                         GBL_RUN_STATUS_OK;

        expected.cpu.stopped = true;
        expected.cpu.stop_reason = GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
        valid = ran && result.termination == GBL_RUN_STOPPED &&
                result.reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION &&
                result.cycles == 0u && result.instructions == 0u &&
                machine.master_cycles == before.master_cycles &&
                machine.cpu.retired_instructions ==
                    before.cpu.retired_instructions &&
                gbl_test_canonical_equal(&expected, &machine) &&
                gbl_test_canonical_digest(&expected) ==
                    gbl_test_canonical_digest(&machine) &&
                gbl_test_same_instance_identity_equal(&before, &machine) &&
                trace.records == records && trace.capacity == 3u &&
                trace.count == 1u && trace.next_sequence == UINT64_C(17) &&
                trace_records_match(records, prior_records, 3u);
    }
    gbl_machine_dispose(&machine);
    return valid;
}

static void test_multiply_operand_restrictions(void)
{
    static const uint32_t nzcv = TEST_CPSR_N | TEST_CPSR_Z | TEST_CPSR_C |
                                 TEST_CPSR_V;
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    bool loaded;
    bool ran = false;

    make_program(image);
    loaded = machine_load(&machine, image);
    if (loaded) {
        machine.cpu.r0_7[1] = UINT32_C(5);
        machine.cpu.r0_7[3] = UINT32_C(7);
        prime_arm_instruction(&machine, UINT32_C(0xE0110391), nzcv);
        ran = run_units(&machine, UINT64_C(4), 1u, &result);
    }
    test_case(loaded && ran && result.termination == GBL_RUN_PROGRESS &&
                  result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                  result.cycles == 2u && result.instructions == 1u &&
                  machine.cpu.r0_7[1] == UINT32_C(35) &&
                  machine.cpu.r0_7[3] == UINT32_C(7) &&
                  (machine.cpu.cpsr & nzcv) == (TEST_CPSR_C | TEST_CPSR_V),
              "ARM7TDMI MUL permits Rd equal to Rm after snapshotting operands");
    gbl_machine_dispose(&machine);

    test_case(multiply_unpredictable_refused(UINT32_C(0xE01F0391)),
              "MUL R15 destination is refused before other effects");
    test_case(multiply_unpredictable_refused(UINT32_C(0xE0C22391)),
              "UMULL equal high and low destinations are refused before effects");
}

static void test_multiply_timing_table(void)
{
    static const arm_multiply_timing_sample samples[] = {
        { 0u, 1u, 1u },
        { UINT32_C(0x000000FF), 1u, 1u },
        { UINT32_C(0x00000100), 2u, 2u },
        { UINT32_C(0x0000FFFF), 2u, 2u },
        { UINT32_C(0x00010000), 3u, 3u },
        { UINT32_C(0x00FFFFFF), 3u, 3u },
        { UINT32_C(0x01000000), 4u, 4u },
        { UINT32_C(0x80000000), 4u, 4u },
        { UINT32_C(0xFFFFFF00), 1u, 4u },
        { UINT32_MAX, 1u, 4u },
        { UINT32_C(0xFFFF0000), 2u, 4u },
        { UINT32_C(0xFF000000), 3u, 4u }
    };
    /* DDI0029G supplies the m thresholds. DDI0100I leaves short MUL/MLA's
     * signed-versus-unsigned termination type implementation-defined, so
     * this profile pins signed termination for those low-word operations;
     * long forms follow their explicit signed/unsigned opcode. */
    static const arm_multiply_timing_form forms[] = {
        { "MUL", UINT32_C(0xE0040392), 0u, false, true },
        { "MLA", UINT32_C(0xE0245392), 1u, false, true },
        { "UMULL", UINT32_C(0xE0C54392), 1u, true, false },
        { "UMLAL", UINT32_C(0xE0E54392), 2u, true, false },
        { "SMULL", UINT32_C(0xE0854392), 1u, true, true },
        { "SMLAL", UINT32_C(0xE0A54392), 2u, true, true }
    };
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    bool loaded;
    size_t form_index;

    make_program(image);
    loaded = machine_load(&machine, image);
    for (form_index = 0u;
         form_index < sizeof(forms) / sizeof(forms[0]); ++form_index) {
        size_t sample_index;

        for (sample_index = 0u;
             sample_index < sizeof(samples) / sizeof(samples[0]);
             ++sample_index) {
            const arm_multiply_timing_form *form = &forms[form_index];
            const arm_multiply_timing_sample *sample = &samples[sample_index];
            const uint8_t algorithm_cycles = form->signed_termination
                ? sample->signed_m
                : sample->unsigned_m;
            const uint64_t expected_cycles = UINT64_C(1) +
                (uint64_t)algorithm_cycles +
                (uint64_t)form->extra_internal_cycles;
            const uint64_t cycles_before = machine.master_cycles;
            const uint64_t retired_before =
                machine.cpu.retired_instructions;
            gbl_run_result result = {
                GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
            };
            char description[112];
            bool ran = false;

            machine.cpu.r0_7[2] = 1u;
            machine.cpu.r0_7[3] = sample->rs;
            machine.cpu.r0_7[4] = 0u;
            machine.cpu.r0_7[5] = 0u;
            if (loaded) {
                ran = run_arm_instruction(&machine, form->opcode, 0u,
                                          expected_cycles, &result);
            }
            (void)snprintf(description, sizeof(description),
                           "%s timing boundary sample %u uses source m",
                           form->name, (unsigned int)sample_index);
            test_case(loaded && ran &&
                          result.termination == GBL_RUN_PROGRESS &&
                          result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                          result.cycles == expected_cycles &&
                          result.instructions == 1u &&
                          machine.master_cycles ==
                              cycles_before + expected_cycles &&
                          machine.cpu.retired_instructions ==
                              retired_before + UINT64_C(1) &&
                          machine.cpu.r0_7[4] == sample->rs &&
                          machine.cpu.r0_7[5] ==
                              (form->long_multiply &&
                                       form->signed_termination &&
                                       (sample->rs & TEST_CPSR_N) != 0u
                                   ? UINT32_MAX
                                   : 0u),
                      description);
        }
    }
    gbl_machine_dispose(&machine);
}

static bool multiply_refusal_unchanged(uint64_t cycle_budget,
                                       uint64_t master_cycles,
                                       uint64_t retired_instructions,
                                       size_t trace_capacity,
                                       uint64_t next_sequence,
                                       gbl_run_termination termination,
                                       gbl_run_reason reason)
{
    const uint32_t opcode = UINT32_C(0xE0A54392); /* SMLAL r4,r5,r2,r3 */
    gbl_machine machine = { 0 };
    gbl_trace_record records[3];
    gbl_trace_record prior_records[3];
    gbl_trace_buffer trace = { records, trace_capacity, 1u, next_sequence };
    gbl_run_result result = {
        GBL_RUN_PROGRESS, GBL_RUN_REASON_NONE, 73u, 91u
    };
    const gbl_run_options options = {
        .cycle_budget = cycle_budget,
        .unit_limit = 1u,
        .category_mask = GBL_TRACE_CATEGORY_MASK,
        .trace_buffer = &trace
    };
    size_t index;
    bool valid;

    for (index = 0u; index < 3u; ++index) {
        records[index] = multiply_trace_sentinel;
        prior_records[index] = multiply_trace_sentinel;
    }
    if (!prepare_arm_multiply_machine(&machine, opcode)) {
        gbl_machine_dispose(&machine);
        return false;
    }
    machine.master_cycles = master_cycles;
    machine.cpu.retired_instructions = retired_instructions;
    {
        const gbl_machine before = machine;
        const gbl_trace_buffer before_trace = trace;
        const bool ran = gbl_cpu_run(&machine, &options, &result) ==
                         GBL_RUN_STATUS_OK;

        valid = ran && result.termination == termination &&
                result.reason == reason && result.cycles == 0u &&
                result.instructions == 0u &&
                trace.records == before_trace.records &&
                trace.capacity == before_trace.capacity &&
                trace.count == before_trace.count &&
                trace.next_sequence == before_trace.next_sequence &&
                trace_records_match(records, prior_records, 3u) &&
                gbl_test_canonical_equal(&before, &machine) &&
                gbl_test_canonical_digest(&before) ==
                    gbl_test_canonical_digest(&machine) &&
                gbl_test_same_instance_identity_equal(&before, &machine);
    }
    gbl_machine_dispose(&machine);
    return valid;
}

static bool multiply_exact_headroom(void)
{
    const uint32_t opcode = UINT32_C(0xE0A54392); /* SMLAL r4,r5,r2,r3 */
    const uint64_t cost = UINT64_C(7);
    gbl_machine machine = { 0 };
    gbl_trace_record records[3];
    gbl_trace_buffer trace = {
        records, 3u, 1u, UINT64_MAX - UINT64_C(2)
    };
    gbl_run_result result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    const gbl_run_options options = {
        .cycle_budget = cost,
        .unit_limit = 1u,
        .category_mask = GBL_TRACE_CATEGORY_MASK,
        .trace_buffer = &trace
    };
    size_t index;
    bool valid;

    for (index = 0u; index < 3u; ++index) {
        records[index] = multiply_trace_sentinel;
    }
    if (!prepare_arm_multiply_machine(&machine, opcode)) {
        gbl_machine_dispose(&machine);
        return false;
    }
    machine.master_cycles = UINT64_MAX - cost;
    machine.cpu.retired_instructions = UINT64_MAX - UINT64_C(1);
    {
        const bool ran = gbl_cpu_run(&machine, &options, &result) ==
                         GBL_RUN_STATUS_OK;

        valid = ran && result.termination == GBL_RUN_PROGRESS &&
                result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                result.cycles == cost && result.instructions == 1u &&
                machine.master_cycles == UINT64_MAX &&
                machine.cpu.retired_instructions == UINT64_MAX &&
                trace.records == records && trace.capacity == 3u &&
                trace.count == 3u && trace.next_sequence == UINT64_MAX &&
                records[0].sequence == multiply_trace_sentinel.sequence &&
                records[0].opcode == multiply_trace_sentinel.opcode &&
                records[1].sequence == UINT64_MAX - UINT64_C(2) &&
                records[1].category == GBL_TRACE_INSTRUCTION &&
                records[1].opcode == opcode &&
                records[2].sequence == UINT64_MAX - UINT64_C(1) &&
                records[2].category == GBL_TRACE_BUS &&
                records[2].opcode == opcode;
    }
    gbl_machine_dispose(&machine);
    return valid;
}

static void test_multiply_preflight_boundaries(void)
{
    const uint64_t cost = UINT64_C(7);

    test_case(multiply_exact_headroom(),
              "SMLAL exact-fit cycles, trace, sequence, and counters reach UINT64_MAX");
    test_case(multiply_refusal_unchanged(
                  cost - UINT64_C(1), 0u, 0u, 3u, UINT64_C(100),
                  GBL_RUN_TIMEOUT, GBL_RUN_REASON_CYCLE_BUDGET),
              "SMLAL one-short cycle budget preserves canonical state and trace buffer");
    test_case(multiply_refusal_unchanged(
                  cost, UINT64_MAX - cost + UINT64_C(1), 0u, 3u,
                  UINT64_C(100), GBL_RUN_STOPPED,
                  GBL_RUN_REASON_COUNTER_OVERFLOW),
              "SMLAL one-short master-cycle headroom refuses before effects");
    test_case(multiply_refusal_unchanged(
                  cost, 0u, UINT64_MAX, 3u, UINT64_C(100),
                  GBL_RUN_STOPPED, GBL_RUN_REASON_COUNTER_OVERFLOW),
              "SMLAL exhausted retired-instruction counter refuses before effects");
    test_case(multiply_refusal_unchanged(
                  cost, 0u, 0u, 2u, UINT64_C(100),
                  GBL_RUN_TRACE_OUTPUT_FULL,
                  GBL_RUN_REASON_TRACE_OUTPUT_FULL),
              "SMLAL one-short selected trace capacity preserves prefix and machine");
    test_case(multiply_refusal_unchanged(
                  cost, 0u, 0u, 3u, UINT64_MAX - UINT64_C(1),
                  GBL_RUN_TRACE_OUTPUT_FULL,
                  GBL_RUN_REASON_TRACE_OUTPUT_FULL),
              "SMLAL one-short trace sequence refuses before effects");
}

typedef struct cpu_maximum_reservation_case {
    const char *description;
    gbl_cpu_test_count_overflow count_overflow;
    uint64_t cycle_budget;
    uint64_t master_cycles;
    uint64_t retired_instructions;
    size_t trace_capacity;
    uint64_t next_sequence;
    bool expected_reserved;
    gbl_run_reason expected_reason;
    size_t expected_records;
} cpu_maximum_reservation_case;

static bool maximum_reservation_case_passes(
    const cpu_maximum_reservation_case *test)
{
    gbl_machine machine = { 0 };
    gbl_machine machine_before;
    gbl_trace_record records[20];
    gbl_trace_record records_before[20];
    gbl_trace_buffer trace;
    gbl_trace_buffer trace_before;
    gbl_run_options options;
    uint64_t cycle_cost = 0u;
    size_t required_records = 0u;
    gbl_run_reason reason = GBL_RUN_REASON_NONE;
    bool reserved;

    machine.initialized = true;
    machine.master_cycles = test->master_cycles;
    machine.open_bus = UINT32_C(0x13579BDF);
    machine.dispcnt = UINT16_C(0x1234);
    machine.cpu.cpsr = TEST_CPSR_SYSTEM;
    machine.cpu.retired_instructions = test->retired_instructions;
    memset(machine.ewram, 0xA5, sizeof(machine.ewram));
    memset(machine.iwram, 0x5A, sizeof(machine.iwram));
    machine_before = machine;

    memset(records, 0xA5, sizeof(records));
    memcpy(records_before, records, sizeof(records));
    memset(&trace, 0, sizeof(trace));
    trace.records = records;
    trace.capacity = test->trace_capacity;
    trace.count = 1u;
    trace.next_sequence = test->next_sequence;
    trace_before = trace;

    memset(&options, 0, sizeof(options));
    options.cycle_budget = test->cycle_budget;
    options.unit_limit = 1u;
    options.category_mask = GBL_TRACE_CATEGORY_MASK;
    options.trace_buffer = &trace;

    reserved = gbl_cpu_test_maximum_preflight(
        &machine, &options, test->count_overflow, &cycle_cost,
        &required_records, &reason);

    return reserved == test->expected_reserved &&
           reason == test->expected_reason && cycle_cost == UINT64_C(18) &&
           required_records == test->expected_records &&
           gbl_test_canonical_equal(&machine_before, &machine) &&
           gbl_test_canonical_digest(&machine_before) ==
               gbl_test_canonical_digest(&machine) &&
           gbl_test_same_instance_identity_equal(&machine_before, &machine) &&
           trace.records == trace_before.records &&
           trace.capacity == trace_before.capacity &&
           trace.count == trace_before.count &&
           trace.next_sequence == trace_before.next_sequence &&
           memcmp(records, records_before, sizeof(records)) == 0;
}

static void test_maximum_unit_reservations(void)
{
    static const cpu_maximum_reservation_case cases[] = {
        { "maximum descriptor reserves exact cycle/counter/trace headroom",
          GBL_CPU_TEST_COUNT_VALID, 18u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX - UINT64_C(1), 20u,
          UINT64_MAX - UINT64_C(19), true, GBL_RUN_REASON_NONE, 19u },
        { "maximum descriptor refuses one-short cycle budget unchanged",
          GBL_CPU_TEST_COUNT_VALID, 17u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX - UINT64_C(1), 20u,
          UINT64_MAX - UINT64_C(19), false, GBL_RUN_REASON_CYCLE_BUDGET, 0u },
        { "maximum descriptor refuses one-short master-cycle headroom",
          GBL_CPU_TEST_COUNT_VALID, 18u,
          UINT64_MAX - UINT64_C(17), UINT64_MAX - UINT64_C(1), 20u,
          UINT64_MAX - UINT64_C(19), false,
          GBL_RUN_REASON_COUNTER_OVERFLOW, 0u },
        { "maximum descriptor refuses one-short retirement headroom",
          GBL_CPU_TEST_COUNT_VALID, 18u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX, 20u,
          UINT64_MAX - UINT64_C(19), false,
          GBL_RUN_REASON_COUNTER_OVERFLOW, 0u },
        { "maximum descriptor refuses one-short trace capacity unchanged",
          GBL_CPU_TEST_COUNT_VALID, 18u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX - UINT64_C(1), 19u,
          UINT64_MAX - UINT64_C(19), false,
          GBL_RUN_REASON_TRACE_OUTPUT_FULL, 19u },
        { "maximum descriptor refuses one-short trace sequence unchanged",
          GBL_CPU_TEST_COUNT_VALID, 18u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX - UINT64_C(1), 20u,
          UINT64_MAX - UINT64_C(18), false,
          GBL_RUN_REASON_TRACE_OUTPUT_FULL, 19u },
        { "maximum descriptor rejects one-over fetch count unchanged",
          GBL_CPU_TEST_FETCH_COUNT_OVERFLOW, 18u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX - UINT64_C(1), 20u,
          UINT64_MAX - UINT64_C(19), false,
          GBL_RUN_REASON_INVALID_CPU_STATE, 0u },
        { "maximum descriptor rejects one-over data count unchanged",
          GBL_CPU_TEST_DATA_COUNT_OVERFLOW, 18u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX - UINT64_C(1), 20u,
          UINT64_MAX - UINT64_C(19), false,
          GBL_RUN_REASON_INVALID_CPU_STATE, 0u },
        { "maximum descriptor rejects one-over result count unchanged",
          GBL_CPU_TEST_RESULT_COUNT_OVERFLOW, 18u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX - UINT64_C(1), 20u,
          UINT64_MAX - UINT64_C(19), false,
          GBL_RUN_REASON_INVALID_CPU_STATE, 0u },
        { "maximum descriptor rejects one-over register-effect count unchanged",
          GBL_CPU_TEST_REGISTER_EFFECT_COUNT_OVERFLOW, 18u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX - UINT64_C(1), 20u,
          UINT64_MAX - UINT64_C(19), false,
          GBL_RUN_REASON_INVALID_CPU_STATE, 0u },
        { "maximum descriptor rejects one-over status-effect count unchanged",
          GBL_CPU_TEST_STATUS_EFFECT_COUNT_OVERFLOW, 18u,
          UINT64_MAX - UINT64_C(18), UINT64_MAX - UINT64_C(1), 20u,
          UINT64_MAX - UINT64_C(19), false,
          GBL_RUN_REASON_INVALID_CPU_STATE, 0u }
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        test_case(maximum_reservation_case_passes(&cases[index]),
                  cases[index].description);
    }
}

static void test_trace_store_capacity(void)
{
    const gbl_trace_record prior = {
        UINT64_C(16), UINT64_C(3), GBL_TRACE_BUS,
        UINT32_C(0x08000040), UINT32_C(0xE1A00000),
        UINT32_C(0x08000044), UINT32_C(0xE1A01000), 4u,
        GBL_BUS_CLASS_SEQUENTIAL, GBL_BUS_OUTCOME_ROM, GBL_EXCEPTION_NONE
    };
    gbl_trace_record short_records[4] = { prior, { 0 } };
    gbl_trace_record exact_records[4] = { prior, { 0 }, { 0 }, { 0 } };
    gbl_trace_buffer short_trace = { short_records, 3u, 1u, UINT64_C(17) };
    gbl_trace_buffer exact_trace = { exact_records, 4u, 1u, UINT64_C(17) };
    gbl_machine short_machine = { 0 };
    gbl_machine exact_machine = { 0 };
    gbl_run_result short_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, UINT64_C(0xA5), UINT64_C(0x5A)
    };
    gbl_run_result exact_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    bool short_loaded;
    bool exact_loaded;

    seed_trace_store_machine(&short_machine);
    seed_trace_store_machine(&exact_machine);
    short_loaded = short_machine.initialized && short_machine.rom != NULL;
    exact_loaded = exact_machine.initialized && exact_machine.rom != NULL;
    if (short_loaded) {
        const gbl_machine before = short_machine;
        const gbl_trace_record prior_before = short_records[0];
        const bool ran = run_traced_units(
            &short_machine, 2u, 1u, GBL_TRACE_CATEGORY_MASK,
            &short_trace, &short_result);

        test_case(ran &&
                      short_result.termination == GBL_RUN_TRACE_OUTPUT_FULL &&
                      short_result.reason ==
                          GBL_RUN_REASON_TRACE_OUTPUT_FULL &&
                      short_result.cycles == 0u &&
                      short_result.instructions == 0u &&
                      short_trace.count == 1u &&
                      short_trace.next_sequence == UINT64_C(17) &&
                      trace_record_is(&short_records[0], prior_before.sequence,
                                      prior_before.guest_cycle,
                                      prior_before.category,
                                      prior_before.instruction_address,
                                      prior_before.opcode, prior_before.address,
                                      prior_before.value, prior_before.width,
                                      prior_before.access_class,
                                      prior_before.bus_outcome,
                                      prior_before.exception_kind) &&
                      gbl_test_canonical_equal(&before, &short_machine) &&
                      gbl_test_canonical_digest(&before) ==
                          gbl_test_canonical_digest(&short_machine) &&
                      gbl_test_same_instance_identity_equal(&before,
                                                            &short_machine),
                  "one-short trace capacity preserves full machine and prior records");
    } else {
        test_case(false,
                  "one-short trace capacity preserves full machine and prior records");
    }

    if (exact_loaded) {
        const bool ran = run_traced_units(
            &exact_machine, 2u, 1u, GBL_TRACE_CATEGORY_MASK,
            &exact_trace, &exact_result);
        const uint8_t *ewram = exact_machine.ewram;

        test_case(ran && exact_result.termination == GBL_RUN_PROGRESS &&
                      exact_result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      exact_result.cycles == 2u &&
                      exact_result.instructions == 1u &&
                      exact_trace.count == 4u &&
                      exact_trace.next_sequence == UINT64_C(20) &&
                      trace_record_is(&exact_records[0], UINT64_C(16), 3u,
                                      GBL_TRACE_BUS, UINT32_C(0x08000040),
                                      UINT32_C(0xE1A00000),
                                      UINT32_C(0x08000044),
                                      UINT32_C(0xE1A01000), 4u,
                                      GBL_BUS_CLASS_SEQUENTIAL,
                                      GBL_BUS_OUTCOME_ROM,
                                      GBL_EXCEPTION_NONE) &&
                      trace_record_is(&exact_records[1], UINT64_C(17), 0u,
                                      GBL_TRACE_INSTRUCTION,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE5810000), 0u, 0u, 0u,
                                      GBL_BUS_CLASS_NONSEQUENTIAL,
                                      GBL_BUS_OUTCOME_NONE,
                                      GBL_EXCEPTION_NONE) &&
                      trace_record_is(&exact_records[2], UINT64_C(18), 1u,
                                      GBL_TRACE_BUS,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE5810000),
                                      UINT32_C(0x02000000),
                                      0u, 4u,
                                      GBL_BUS_CLASS_NONSEQUENTIAL,
                                      GBL_BUS_OUTCOME_RAM,
                                      GBL_EXCEPTION_NONE) &&
                      trace_record_is(&exact_records[3], UINT64_C(19), 2u,
                                      GBL_TRACE_BUS,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE5810000),
                                      UINT32_C(0x08000008), 0u, 4u,
                                      GBL_BUS_CLASS_SEQUENTIAL,
                                      GBL_BUS_OUTCOME_ROM,
                                      GBL_EXCEPTION_NONE) &&
                      ewram[0] == UINT8_C(0x78) &&
                      ewram[1] == UINT8_C(0x56) &&
                      ewram[2] == UINT8_C(0x34) &&
                      ewram[3] == UINT8_C(0x12),
                  "exact trace capacity records instruction, data and refill order");
    } else {
        test_case(false,
                  "exact trace capacity records instruction, data and refill order");
    }

    gbl_machine_dispose(&short_machine);
    gbl_machine_dispose(&exact_machine);
}

static void prime_trace_arm_instruction(gbl_machine *machine,
                                        uint32_t opcode)
{
    const uint32_t address = UINT32_C(0x08000000);

    machine->cpu.execution_address = address;
    machine->cpu.cpsr = TEST_CPSR_SYSTEM;
    machine->cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        address, opcode, 4u, true
    };
    machine->cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        address + UINT32_C(4), UINT32_C(0xE1A00000), 4u, true
    };
    machine->cpu.next_fetch_sequential = true;
}

static gbl_run_result trace_result_sentinel(void)
{
    const gbl_run_result sentinel = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, UINT64_C(0xA5), UINT64_C(0x5A)
    };

    return sentinel;
}

static bool trace_result_is_sentinel(const gbl_run_result *result)
{
    return result->termination == GBL_RUN_TIMEOUT &&
           result->reason == GBL_RUN_REASON_NONE &&
           result->cycles == UINT64_C(0xA5) &&
           result->instructions == UINT64_C(0x5A);
}

static void test_trace_refill_capacity(void)
{
    const gbl_trace_record prior = {
        UINT64_C(91), UINT64_C(73), GBL_TRACE_INSTRUCTION,
        UINT32_C(0x08000020), UINT32_C(0xE1A00000), 0u, 0u, 0u,
        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_OUTCOME_NONE,
        GBL_EXCEPTION_NONE
    };
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_trace_record records[2] = { prior, { 0 } };
    gbl_trace_buffer trace = { records, 1u, 0u, UINT64_C(91) };
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;

    make_empty_image(image);
    write_word(image, 0u, UINT32_C(0xE1A00000));
    write_word(image, 4u, UINT32_C(0xE1A01000));
    loaded = machine_load(&machine, image);
    if (loaded) {
        const gbl_machine before = machine;
        const bool short_run = run_traced_units(
            &machine, 2u, 1u, (uint32_t)GBL_TRACE_BUS, &trace, &result);

        test_case(short_run &&
                      result.termination == GBL_RUN_TRACE_OUTPUT_FULL &&
                      result.reason == GBL_RUN_REASON_TRACE_OUTPUT_FULL &&
                      result.cycles == 0u && result.instructions == 0u &&
                      trace.count == 0u &&
                      trace.next_sequence == UINT64_C(91) &&
                      trace_record_is(&records[0], prior.sequence,
                                      prior.guest_cycle, prior.category,
                                      prior.instruction_address, prior.opcode,
                                      prior.address, prior.value, prior.width,
                                      prior.access_class, prior.bus_outcome,
                                      prior.exception_kind) &&
                      gbl_test_canonical_equal(&before, &machine) &&
                      gbl_test_canonical_digest(&before) ==
                          gbl_test_canonical_digest(&machine),
                  "initial refill refuses one-short bus capacity without effects");
    } else {
        test_case(false,
                  "initial refill refuses one-short bus capacity without effects");
    }

    trace.capacity = 2u;
    if (loaded) {
        const bool exact_run = run_traced_units(
            &machine, 2u, 1u, (uint32_t)GBL_TRACE_BUS, &trace, &result);

        test_case(exact_run && result.termination == GBL_RUN_PROGRESS &&
                      result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                      result.cycles == 2u && result.instructions == 0u &&
                      trace.count == 2u &&
                      trace.next_sequence == UINT64_C(93) &&
                      trace_record_is(&records[0], UINT64_C(91), 1u,
                                      GBL_TRACE_BUS,
                                      UINT32_C(0x08000000), 0u,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE1A00000), 4u,
                                      GBL_BUS_CLASS_NONSEQUENTIAL,
                                      GBL_BUS_OUTCOME_ROM,
                                      GBL_EXCEPTION_NONE) &&
                      trace_record_is(&records[1], UINT64_C(92), 2u,
                                      GBL_TRACE_BUS,
                                      UINT32_C(0x08000000), 0u,
                                      UINT32_C(0x08000004),
                                      UINT32_C(0xE1A01000), 4u,
                                      GBL_BUS_CLASS_SEQUENTIAL,
                                      GBL_BUS_OUTCOME_ROM,
                                      GBL_EXCEPTION_NONE),
                  "initial refill emits both real fetches at exact capacity");
    } else {
        test_case(false,
                  "initial refill emits both real fetches at exact capacity");
    }
    gbl_machine_dispose(&machine);
}

static void test_trace_category_masks(void)
{
    static const struct trace_mask_case {
        uint32_t mask;
        size_t expected_count;
        const char *description;
    } cases[] = {
        { 0u, 0u, "zero trace mask emits no records" },
        { (uint32_t)GBL_TRACE_INSTRUCTION, 1u,
          "instruction-only mask emits only the instruction" },
        { (uint32_t)GBL_TRACE_BUS, 2u,
          "bus-only mask emits only data and sequential fetch" },
        { (uint32_t)GBL_TRACE_EXCEPTION, 0u,
          "exception-only mask emits no phantom event" },
        { (uint32_t)(GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS), 3u,
          "instruction and bus mask preserve their order" },
        { (uint32_t)(GBL_TRACE_INSTRUCTION | GBL_TRACE_EXCEPTION), 1u,
          "instruction and exception mask excludes bus events" },
        { (uint32_t)(GBL_TRACE_BUS | GBL_TRACE_EXCEPTION), 2u,
          "bus and exception mask excludes instruction events" },
        { GBL_TRACE_CATEGORY_MASK, 3u,
          "all trace categories emit their selected events" }
    };
    size_t case_index;

    for (case_index = 0u;
         case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
        gbl_trace_record records[3] = { { 0 } };
        gbl_trace_buffer trace = { records, 3u, 0u, 0u };
        gbl_machine machine = { 0 };
        gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
        gbl_trace_buffer *trace_pointer =
            cases[case_index].mask == 0u ? NULL : &trace;
        bool loaded;
        bool ran;
        bool categories_match = true;
        size_t record_index;
        size_t expected_index = 0u;

        seed_trace_store_machine(&machine);
        loaded = machine.initialized && machine.rom != NULL;
        ran = loaded && run_traced_units(
            &machine, 2u, 1u, cases[case_index].mask, trace_pointer, &result);
        if ((cases[case_index].mask & (uint32_t)GBL_TRACE_INSTRUCTION) != 0u) {
            categories_match = trace.count > expected_index &&
                records[expected_index].category == GBL_TRACE_INSTRUCTION;
            expected_index++;
        }
        if ((cases[case_index].mask & (uint32_t)GBL_TRACE_BUS) != 0u) {
            categories_match = categories_match &&
                trace.count >= expected_index + 2u &&
                records[expected_index].category == GBL_TRACE_BUS &&
                records[expected_index + 1u].category == GBL_TRACE_BUS;
            expected_index += 2u;
        }
        for (record_index = 0u; record_index < trace.count; ++record_index) {
            if ((cases[case_index].mask &
                 (uint32_t)records[record_index].category) == 0u ||
                records[record_index].sequence != (uint64_t)record_index) {
                categories_match = false;
            }
        }
        test_case(ran && result.termination == GBL_RUN_PROGRESS &&
                      result.cycles == 2u &&
                      trace.count == cases[case_index].expected_count &&
                      trace.next_sequence ==
                          (uint64_t)cases[case_index].expected_count &&
                      categories_match &&
                      (trace_pointer != NULL ||
                       (trace.count == 0u && trace.next_sequence == 0u)),
                  cases[case_index].description);
        gbl_machine_dispose(&machine);
    }
}

static void test_trace_control_flow(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_trace_record records[3] = { { 0 } };
    gbl_trace_buffer trace = { records, 3u, 0u, 0u };
    gbl_machine branch_machine = { 0 };
    gbl_run_result branch_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    bool branch_loaded;
    bool branch_ran;

    make_empty_image(image);
    write_word(image, 0u, UINT32_C(0xEA000002));
    write_word(image, 0x10u, UINT32_C(0xE1A00000));
    write_word(image, 0x14u, UINT32_C(0xE1A01000));
    branch_loaded = machine_load(&branch_machine, image);
    if (branch_loaded) {
        prime_trace_arm_instruction(&branch_machine, UINT32_C(0xEA000002));
    }
    branch_ran = branch_loaded && run_traced_units(
        &branch_machine, 2u, 1u, GBL_TRACE_CATEGORY_MASK, &trace,
        &branch_result);
    test_case(branch_ran && branch_result.termination == GBL_RUN_PROGRESS &&
                  branch_result.cycles == 2u &&
                  branch_machine.cpu.execution_address ==
                      UINT32_C(0x08000010) &&
                  trace.count == 3u &&
                  trace_record_is(&records[0], 0u, 0u,
                                  GBL_TRACE_INSTRUCTION,
                                  UINT32_C(0x08000000),
                                  UINT32_C(0xEA000002), 0u, 0u, 0u,
                                  GBL_BUS_CLASS_NONSEQUENTIAL,
                                  GBL_BUS_OUTCOME_NONE,
                                  GBL_EXCEPTION_NONE) &&
                  trace_record_is(&records[1], 1u, 1u, GBL_TRACE_BUS,
                                  UINT32_C(0x08000000),
                                  UINT32_C(0xEA000002),
                                  UINT32_C(0x08000010),
                                  UINT32_C(0xE1A00000), 4u,
                                  GBL_BUS_CLASS_NONSEQUENTIAL,
                                  GBL_BUS_OUTCOME_ROM,
                                  GBL_EXCEPTION_NONE) &&
                  trace_record_is(&records[2], 2u, 2u, GBL_TRACE_BUS,
                                  UINT32_C(0x08000000),
                                  UINT32_C(0xEA000002),
                                  UINT32_C(0x08000014),
                                  UINT32_C(0xE1A01000), 4u,
                                  GBL_BUS_CLASS_SEQUENTIAL,
                                  GBL_BUS_OUTCOME_ROM,
                                  GBL_EXCEPTION_NONE),
              "branch instruction precedes both real refill reads");
    gbl_machine_dispose(&branch_machine);

    {
        gbl_trace_record bx_records[3] = { { 0 } };
        gbl_trace_buffer bx_trace = { bx_records, 3u, 0u, 0u };
        gbl_machine bx_machine = { 0 };
        gbl_run_result bx_result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool bx_loaded;
        bool bx_ran;

        make_empty_image(image);
        write_halfword(image, 0x10u, UINT16_C(0x2001));
        write_halfword(image, 0x12u, UINT16_C(0x2102));
        bx_loaded = machine_load(&bx_machine, image);
        if (bx_loaded) {
            prime_trace_arm_instruction(&bx_machine,
                                        UINT32_C(0xE12FFF10));
            bx_machine.cpu.r0_7[0] = UINT32_C(0x08000011);
        }
        bx_ran = bx_loaded && run_traced_units(
            &bx_machine, 2u, 1u, GBL_TRACE_CATEGORY_MASK, &bx_trace,
            &bx_result);
        test_case(bx_ran && bx_result.termination == GBL_RUN_PROGRESS &&
                      bx_result.cycles == 2u && bx_trace.count == 3u &&
                      bx_machine.cpu.execution_address ==
                          UINT32_C(0x08000010) &&
                      (bx_machine.cpu.cpsr & UINT32_C(0x20)) != 0u &&
                      trace_record_is(&bx_records[0], 0u, 0u,
                                      GBL_TRACE_INSTRUCTION,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE12FFF10), 0u, 0u, 0u,
                                      GBL_BUS_CLASS_NONSEQUENTIAL,
                                      GBL_BUS_OUTCOME_NONE,
                                      GBL_EXCEPTION_NONE) &&
                      trace_record_is(&bx_records[1], 1u, 1u, GBL_TRACE_BUS,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE12FFF10),
                                      UINT32_C(0x08000010),
                                      UINT32_C(0x2001), 2u,
                                      GBL_BUS_CLASS_NONSEQUENTIAL,
                                      GBL_BUS_OUTCOME_ROM,
                                      GBL_EXCEPTION_NONE) &&
                      trace_record_is(&bx_records[2], 2u, 2u, GBL_TRACE_BUS,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE12FFF10),
                                      UINT32_C(0x08000012),
                                      UINT32_C(0x2102), 2u,
                                      GBL_BUS_CLASS_SEQUENTIAL,
                                      GBL_BUS_OUTCOME_ROM,
                                      GBL_EXCEPTION_NONE),
                  "BX state switch records two halfword refill reads");
        gbl_machine_dispose(&bx_machine);
    }

    {
        gbl_trace_record load_records[3] = { { 0 } };
        gbl_trace_buffer load_trace = { load_records, 3u, 0u, 0u };
        gbl_machine load_machine = { 0 };
        gbl_run_result load_result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool load_loaded;
        bool load_ran;

        make_empty_image(image);
        write_word(image, 0u, UINT32_C(0xE5912000));
        load_loaded = machine_load(&load_machine, image);
        if (load_loaded) {
            prime_trace_arm_instruction(&load_machine,
                                        UINT32_C(0xE5912000));
            load_machine.cpu.r0_7[1] = UINT32_C(0x02000000);
            load_machine.ewram[0] = UINT8_C(0x78);
            load_machine.ewram[1] = UINT8_C(0x56);
            load_machine.ewram[2] = UINT8_C(0x34);
            load_machine.ewram[3] = UINT8_C(0x12);
        }
        load_ran = load_loaded && run_traced_units(
            &load_machine, 2u, 1u, GBL_TRACE_CATEGORY_MASK, &load_trace,
            &load_result);
        test_case(load_ran &&
                      load_result.termination == GBL_RUN_PROGRESS &&
                      load_result.cycles == 2u &&
                      load_result.instructions == 1u &&
                      load_machine.cpu.r0_7[2] == UINT32_C(0x12345678) &&
                      load_trace.count == 3u &&
                      trace_record_is(&load_records[0], 0u, 0u,
                                      GBL_TRACE_INSTRUCTION,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE5912000), 0u, 0u, 0u,
                                      GBL_BUS_CLASS_NONSEQUENTIAL,
                                      GBL_BUS_OUTCOME_NONE,
                                      GBL_EXCEPTION_NONE) &&
                      trace_record_is(&load_records[1], 1u, 1u,
                                      GBL_TRACE_BUS,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE5912000),
                                      UINT32_C(0x02000000),
                                      UINT32_C(0x12345678), 4u,
                                      GBL_BUS_CLASS_NONSEQUENTIAL,
                                      GBL_BUS_OUTCOME_RAM,
                                      GBL_EXCEPTION_NONE) &&
                      trace_record_is(&load_records[2], 2u, 2u,
                                      GBL_TRACE_BUS,
                                      UINT32_C(0x08000000),
                                      UINT32_C(0xE5912000),
                                      UINT32_C(0x08000008), 0u, 4u,
                                      GBL_BUS_CLASS_SEQUENTIAL,
                                      GBL_BUS_OUTCOME_ROM,
                                      GBL_EXCEPTION_NONE),
                  "load trace records the real read value and refill order");
        gbl_machine_dispose(&load_machine);
    }
}

static void test_trace_exception_order(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_trace_record records[2] = { { 0 } };
    gbl_trace_buffer trace = { records, 2u, 0u, 0u };
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;
    bool ran;

    make_empty_image(image);
    write_word(image, 0u, UINT32_C(0xEF000000));
    loaded = machine_load(&machine, image);
    if (loaded) {
        prime_trace_arm_instruction(&machine, UINT32_C(0xEF000000));
    }
    ran = loaded && run_traced_units(
        &machine, 1u, 1u, GBL_TRACE_CATEGORY_MASK, &trace, &result);
    test_case(ran && result.termination == GBL_RUN_STOPPED &&
                  result.reason == GBL_RUN_REASON_GUEST_STOP &&
                  result.cycles == 1u && result.instructions == 1u &&
                  machine.cpu.stop_reason == GBL_CPU_STOP_SWI_NO_BIOS &&
                  trace.count == 2u &&
                  trace_record_is(&records[0], 0u, 0u,
                                  GBL_TRACE_INSTRUCTION,
                                  UINT32_C(0x08000000),
                                  UINT32_C(0xEF000000), 0u, 0u, 0u,
                                  GBL_BUS_CLASS_NONSEQUENTIAL,
                                  GBL_BUS_OUTCOME_NONE,
                                  GBL_EXCEPTION_NONE) &&
                  trace_record_is(&records[1], 1u, 1u,
                                  GBL_TRACE_EXCEPTION,
                                  UINT32_C(0x08000000),
                                  UINT32_C(0xEF000000), 0u, 0u, 0u,
                                  GBL_BUS_CLASS_NONSEQUENTIAL,
                                  GBL_BUS_OUTCOME_NONE, GBL_EXCEPTION_SWI),
              "exception record follows entry cycle without vector fetch");
    gbl_machine_dispose(&machine);
}

static void test_trace_after_mailbox_pass(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result completed = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    gbl_trace_buffer trace = { NULL, 0u, 0u, 0u };
    gbl_run_result exhausted = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    gbl_mailbox_result mailbox;
    bool loaded;
    bool completed_run;
    bool exhausted_run;

    make_mailbox_guest(image, GBL_MAILBOX_STATE_PASS,
                       TEST_MAILBOX_PASS_CASE);
    loaded = machine_load(&machine, image);
    completed_run = loaded && run_units(&machine, 24u, 0u, &completed);
    mailbox = loaded ? gbl_mailbox_read(&machine)
                     : (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
    if (loaded) {
        const gbl_machine before = machine;

        exhausted_run = run_traced_units(
            &machine, 4u, 1u, (uint32_t)GBL_TRACE_INSTRUCTION, &trace,
            &exhausted);
        test_case(completed_run && mailbox.status == GBL_MAILBOX_PASS &&
                      mailbox.case_id == TEST_MAILBOX_PASS_CASE &&
                      exhausted_run &&
                      exhausted.termination == GBL_RUN_TRACE_OUTPUT_FULL &&
                      exhausted.reason == GBL_RUN_REASON_TRACE_OUTPUT_FULL &&
                      exhausted.cycles == 0u && exhausted.instructions == 0u &&
                      gbl_mailbox_read(&machine).status == GBL_MAILBOX_PASS &&
                      gbl_mailbox_read(&machine).case_id ==
                          TEST_MAILBOX_PASS_CASE &&
                      gbl_test_canonical_equal(&before, &machine) &&
                      trace.count == 0u && trace.next_sequence == 0u,
                  "trace exhaustion stays visible after a complete PASS mailbox");
    } else {
        test_case(false,
                  "trace exhaustion stays visible after a complete PASS mailbox");
    }
    gbl_machine_dispose(&machine);
}

static void test_trace_invalid_options(void)
{
    const gbl_trace_record marker = {
        UINT64_C(33), UINT64_C(29), GBL_TRACE_BUS,
        UINT32_C(0x08000020), UINT32_C(0xE1A00000),
        UINT32_C(0x08000024), UINT32_C(0xE1A01000), 4u,
        GBL_BUS_CLASS_SEQUENTIAL, GBL_BUS_OUTCOME_ROM, GBL_EXCEPTION_NONE
    };
    gbl_trace_record records[2] = { marker, marker };
    gbl_trace_buffer trace = { records, 2u, 1u, UINT64_C(34) };
    gbl_machine machine = { 0 };
    gbl_run_options options = {
        .cycle_budget = 2u,
        .unit_limit = 1u,
        .category_mask = (uint32_t)GBL_TRACE_INSTRUCTION,
        .trace_buffer = &trace
    };
    gbl_run_result result = trace_result_sentinel();
    bool loaded;

    seed_trace_store_machine(&machine);
    loaded = machine.initialized && machine.rom != NULL;
    if (loaded) {
        const gbl_machine before = machine;
        bool valid;

        options.category_mask = UINT32_C(0x80000000);
        result = trace_result_sentinel();
        valid = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine) &&
                trace.count == 1u && trace.next_sequence == UINT64_C(34) &&
                trace_record_is(&records[0], marker.sequence,
                                marker.guest_cycle, marker.category,
                                marker.instruction_address, marker.opcode,
                                marker.address, marker.value, marker.width,
                                marker.access_class, marker.bus_outcome,
                                marker.exception_kind);
        test_case(valid, "unknown trace category mask is refused before effects");

        options.category_mask = (uint32_t)GBL_TRACE_INSTRUCTION;
        options.trace_buffer = NULL;
        result = trace_result_sentinel();
        valid = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine);
        test_case(valid, "selected trace category requires a buffer");

        options.trace_buffer = &trace;
        trace.capacity = 0u;
        trace.count = 1u;
        result = trace_result_sentinel();
        valid = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine) &&
                trace.count == 1u;
        test_case(valid, "trace count above capacity is refused unchanged");

        trace.records = NULL;
        trace.capacity = 1u;
        trace.count = 0u;
        result = trace_result_sentinel();
        valid = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine) &&
                trace.count == 0u;
        test_case(valid, "null trace storage with positive capacity is refused");

        trace.records = (gbl_trace_record *)(uintptr_t)1u;
        trace.capacity = SIZE_MAX / sizeof(*trace.records) + 1u;
        result = trace_result_sentinel();
        valid = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine);
        test_case(valid, "trace extent multiplication overflow is refused");

        trace.records = (gbl_trace_record *)(uintptr_t)(
            UINTPTR_MAX - sizeof(*trace.records) + 1u);
        trace.capacity = 1u;
        trace.count = 0u;
        result = trace_result_sentinel();
        valid = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine) &&
                trace.count == 0u;
        test_case(valid, "trace pointer extent wrap is refused before effects");

        trace.records = (gbl_trace_record *)(void *)&machine;
        trace.capacity = 1u;
        trace.count = 0u;
        result = trace_result_sentinel();
        valid = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine);
        test_case(valid, "trace records may not alias canonical machine storage");

        options.trace_buffer = (gbl_trace_buffer *)(void *)&machine;
        result = trace_result_sentinel();
        valid = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine);
        test_case(valid, "trace buffer object may not alias machine storage");

        options.trace_buffer = &trace;
        trace.records = (gbl_trace_record *)(void *)&options;
        result = trace_result_sentinel();
        valid = gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine);
        trace.records = (gbl_trace_record *)(void *)&result;
        result = trace_result_sentinel();
        valid = valid &&
                gbl_cpu_run(&machine, &options, &result) ==
                    GBL_RUN_STATUS_INVALID_ARGUMENT &&
                trace_result_is_sentinel(&result) &&
                gbl_test_canonical_equal(&before, &machine);
        test_case(valid, "trace record storage may not alias options or result");
    } else {
        test_case(false, "unknown trace category mask is refused before effects");
        test_case(false, "selected trace category requires a buffer");
        test_case(false, "trace count above capacity is refused unchanged");
        test_case(false, "null trace storage with positive capacity is refused");
        test_case(false, "trace extent multiplication overflow is refused");
        test_case(false, "trace pointer extent wrap is refused before effects");
        test_case(false, "trace records may not alias canonical machine storage");
        test_case(false, "trace buffer object may not alias machine storage");
        test_case(false, "trace record storage may not alias options or result");
    }
    gbl_machine_dispose(&machine);
}

static void test_trace_sequence_and_untraced_parity(void)
{
    const gbl_trace_record marker = {
        UINT64_C(3), UINT64_C(2), GBL_TRACE_INSTRUCTION,
        UINT32_C(0x08000020), UINT32_C(0xE1A00000), 0u, 0u, 0u,
        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_OUTCOME_NONE,
        GBL_EXCEPTION_NONE
    };
    gbl_trace_record records[2] = { marker, { 0 } };
    gbl_trace_buffer trace = { records, 2u, 0u, UINT64_MAX };
    gbl_machine exhausted_machine = { 0 };
    gbl_run_result exhausted = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    bool loaded;

    seed_trace_store_machine(&exhausted_machine);
    loaded = exhausted_machine.initialized && exhausted_machine.rom != NULL;
    if (loaded) {
        const gbl_machine before = exhausted_machine;
        const bool ran = run_traced_units(
            &exhausted_machine, 2u, 1u,
            (uint32_t)GBL_TRACE_INSTRUCTION, &trace, &exhausted);

        test_case(ran &&
                      exhausted.termination == GBL_RUN_TRACE_OUTPUT_FULL &&
                      exhausted.reason == GBL_RUN_REASON_TRACE_OUTPUT_FULL &&
                      exhausted.cycles == 0u && exhausted.instructions == 0u &&
                      trace.count == 0u && trace.next_sequence == UINT64_MAX &&
                      trace_record_is(&records[0], marker.sequence,
                                      marker.guest_cycle, marker.category,
                                      marker.instruction_address, marker.opcode,
                                      marker.address, marker.value, marker.width,
                                      marker.access_class, marker.bus_outcome,
                                      marker.exception_kind) &&
                      gbl_test_canonical_equal(&before, &exhausted_machine),
                  "sequence overflow refuses the whole instruction unit");
    } else {
        test_case(false,
                  "sequence overflow refuses the whole instruction unit");
    }
    gbl_machine_dispose(&exhausted_machine);

    {
        gbl_machine traced = { 0 };
        gbl_machine plain = { 0 };
        gbl_trace_buffer ignored = { NULL, 0u, 0u, UINT64_MAX };
        gbl_run_result traced_result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        gbl_run_result plain_result = {
            GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
        };
        bool traced_loaded;
        bool plain_loaded;
        bool traced_ran;
        bool plain_ran;

        seed_trace_store_machine(&traced);
        seed_trace_store_machine(&plain);
        traced_loaded = traced.initialized && traced.rom != NULL;
        plain_loaded = plain.initialized && plain.rom != NULL;
        traced_ran = traced_loaded && run_traced_units(
            &traced, 2u, 1u, 0u, &ignored, &traced_result);
        plain_ran = plain_loaded && run_units(&plain, 2u, 1u, &plain_result);
        test_case(traced_ran && plain_ran &&
                      traced_result.termination == plain_result.termination &&
                      traced_result.reason == plain_result.reason &&
                      traced_result.cycles == plain_result.cycles &&
                      traced_result.instructions == plain_result.instructions &&
                      gbl_test_canonical_equal(&traced, &plain) &&
                      gbl_test_canonical_digest(&traced) ==
                          gbl_test_canonical_digest(&plain) &&
                      ignored.count == 0u &&
                      ignored.next_sequence == UINT64_MAX,
                  "zero mask matches untraced execution without sequence overflow");
        gbl_machine_dispose(&traced);
        gbl_machine_dispose(&plain);
    }
}

static void test_trace_unit_limit(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_trace_record records[32] = { { 0 } };
    gbl_trace_buffer trace = { records, 32u, 0u, 0u };
    gbl_machine traced = { 0 };
    gbl_machine plain = { 0 };
    gbl_run_result traced_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    gbl_run_result plain_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u
    };
    bool traced_loaded;
    bool plain_loaded;
    bool traced_ran;
    bool plain_ran;

    make_empty_image(image);
    write_word(image, 0u, UINT32_C(0xE5810000));
    write_word(image, 4u, UINT32_C(0xE1A02000));
    write_word(image, 8u, UINT32_C(0xE1A03000));
    traced_loaded = machine_load(&traced, image);
    plain_loaded = machine_load(&plain, image);
    if (traced_loaded) {
        prime_trace_arm_instruction(&traced, UINT32_C(0xE5810000));
        traced.cpu.r0_7[0] = UINT32_C(0x12345678);
        traced.cpu.r0_7[1] = UINT32_C(0x02000000);
    }
    if (plain_loaded) {
        prime_trace_arm_instruction(&plain, UINT32_C(0xE5810000));
        plain.cpu.r0_7[0] = UINT32_C(0x12345678);
        plain.cpu.r0_7[1] = UINT32_C(0x02000000);
    }
    traced_ran = traced_loaded && run_traced_units(
        &traced, 8u, 1u, GBL_TRACE_CATEGORY_MASK, &trace, &traced_result);
    plain_ran = plain_loaded && run_units(&plain, 8u, 1u, &plain_result);
    test_case(traced_ran && plain_ran &&
                  traced_result.termination == GBL_RUN_PROGRESS &&
                  traced_result.reason == GBL_RUN_REASON_UNIT_LIMIT &&
                  traced_result.cycles == 2u &&
                  traced_result.instructions == 1u && trace.count == 3u &&
                  gbl_test_canonical_equal(&traced, &plain) &&
                  gbl_test_canonical_digest(&traced) ==
                      gbl_test_canonical_digest(&plain),
              "unit limit stops after one instruction despite spare cycle budget");
    gbl_machine_dispose(&traced);
    gbl_machine_dispose(&plain);
}

typedef void (*cpu_test_function)(void);

typedef struct cpu_test_group {
    const char *selector;
    unsigned int assertion_count;
    size_t function_count;
    const cpu_test_function *functions;
} cpu_test_group;

#define CPU_TEST_ARRAY_COUNT(functions) \
    (sizeof(functions) / sizeof((functions)[0]))
#define CPU_TEST_GROUP(selector_name, assertions, function_list) \
    { selector_name, assertions, CPU_TEST_ARRAY_COUNT(function_list), function_list }

static const cpu_test_function arm_subword_functions[] = {
    test_arm_subword_address_matrix,
    test_arm_subword_pc_and_alias_rules,
    test_arm_subword_architectural_refusals
};
static const cpu_test_function lifecycle_functions[] = { test_lifecycle };
static const cpu_test_function arm_alu_functions[] = {
    test_arm_adc_carry_in,
    test_arm_alu_vectors,
    test_arm_alu_without_s_preserves_flags
};
static const cpu_test_function multiply_unsigned_functions[] = {
    test_multiply_unsigned
};
static const cpu_test_function multiply_signed_timing_functions[] = {
    test_multiply_signed_vectors,
    test_multiply_operand_restrictions,
    test_multiply_timing_table,
    test_multiply_preflight_boundaries
};
static const cpu_test_function arm_shifts_conditions_functions[] = {
    test_arm_register_shifted_pc_operand_uses_plus_twelve,
    test_arm_operand2_shift_vectors,
    test_arm_condition_predicates,
    test_arm_al_ignores_nzcv,
    test_arm_condition_1111_is_unpredictable,
    test_arm_register_shift_cycle_preflight
};
static const cpu_test_function arm_transfer_functions[] = {
    test_arm_transfer_preindexed_writeback,
    test_arm_transfer_immediate_matrix,
    test_arm_transfer_scaled_register_matrix,
    test_arm_transfer_word_and_byte_lanes,
    test_arm_transfer_pc_contexts,
    test_arm_transfer_store_source_snapshot,
    test_arm_transfer_unpredictable_refusals,
    test_arm_transfer_preflight_boundaries,
    test_arm_transfer_unmodeled_bus_results,
    test_arm_transfer_modulo_address_wrap
};
static const cpu_test_function loaded_pc_functions[] = { test_loaded_pc };
static const cpu_test_function arm_pc_return_functions[] = {
    test_arm_pc_operand_contexts,
    test_thumb_pc_operand_contexts,
    test_pc_return_condition_and_refusal,
    test_arm_pc_return
};
static const cpu_test_function psr_functions[] = {
    test_psr_mrs_forms,
    test_psr_register_field_masks,
    test_psr_immediate_field_masks,
    test_psr_saved_register_and_immediate_effects,
    test_psr_zero_field_masks,
    test_psr_composed_field_masks,
    test_psr_mode_reads_and_banks,
    test_psr_privilege_and_saved_state,
    test_psr_unpredictable_refusals
};
static const cpu_test_function tracer_functions[] = { test_tracer };
static const cpu_test_function exception_functions[] = {
    test_exceptions,
    test_masked_requests,
    test_exception_matrix,
    test_exception_refusals,
    test_exception_restore_and_overwrite
};
static const cpu_test_function modes_functions[] = { test_modes };
static const cpu_test_function no_bios_functions[] = { test_no_bios };
static const cpu_test_function peek_functions[] = { test_guest_mailbox_and_peek };
static const cpu_test_function mailbox_functions[] = { test_mailbox_statuses };
static const cpu_test_function trace_functions[] = {
    test_trace_store_capacity,
    test_trace_refill_capacity,
    test_trace_category_masks,
    test_trace_control_flow,
    test_trace_exception_order,
    test_trace_after_mailbox_pass,
    test_trace_invalid_options,
    test_trace_sequence_and_untraced_parity,
    test_trace_unit_limit
};
static const cpu_test_function trace_capacity_functions[] = {
    test_trace_store_capacity,
    test_trace_refill_capacity
};
static const cpu_test_function budget_functions[] = { test_budget };
static const cpu_test_function arithmetic_functions[] = { test_arithmetic };
static const cpu_test_function thumb_functions[] = { test_thumb };
static const cpu_test_function decode_functions[] = { test_decode };
static const cpu_test_function alignment_functions[] = { test_alignment };
static const cpu_test_function slots_functions[] = {
    test_word_swp_stream_setup,
    test_word_swp_read_write_trace,
    test_word_swp_cycle_refusal,
    test_word_swp_trace_refusal,
    test_word_swp_sequence_refusal
};
static const cpu_test_function slots_bounds_functions[] = {
    test_swp_register_aliases,
    test_swp_word_lane_results,
    test_swpb_read_write_trace,
    test_swpb_register_aliases,
    test_swp_pc_restrictions,
    test_swp_selected_trace_boundaries,
    test_swp_counter_headroom,
    test_maximum_unit_reservations
};

static const cpu_test_group cpu_test_groups[] = {
    CPU_TEST_GROUP("lifecycle", 9u, lifecycle_functions),
    CPU_TEST_GROUP("multiply_signed_timing", 85u,
                   multiply_signed_timing_functions),
    CPU_TEST_GROUP("multiply_unsigned", 6u, multiply_unsigned_functions),
    CPU_TEST_GROUP("arm_alu", 18u, arm_alu_functions),
    CPU_TEST_GROUP("arm_shifts_conditions", 73u,
                   arm_shifts_conditions_functions),
    CPU_TEST_GROUP("arm_transfer", 361u, arm_transfer_functions),
    CPU_TEST_GROUP("arm_subword", 46u, arm_subword_functions),
    CPU_TEST_GROUP("loaded_pc", 17u, loaded_pc_functions),
    CPU_TEST_GROUP("arm_pc_return", 10u, arm_pc_return_functions),
    CPU_TEST_GROUP("psr", 42u, psr_functions),
    CPU_TEST_GROUP("tracer", 4u, tracer_functions),
    CPU_TEST_GROUP("exceptions", 13u, exception_functions),
    CPU_TEST_GROUP("modes", 5u, modes_functions),
    CPU_TEST_GROUP("no_bios", 2u, no_bios_functions),
    CPU_TEST_GROUP("peek", 5u, peek_functions),
    CPU_TEST_GROUP("mailbox", 4u, mailbox_functions),
    CPU_TEST_GROUP("trace", 29u, trace_functions),
    CPU_TEST_GROUP("trace_capacity", 4u, trace_capacity_functions),
    CPU_TEST_GROUP("budget", 9u, budget_functions),
    CPU_TEST_GROUP("arithmetic", 28u, arithmetic_functions),
    CPU_TEST_GROUP("thumb", 8u, thumb_functions),
    CPU_TEST_GROUP("decode", 8u, decode_functions),
    CPU_TEST_GROUP("alignment", 11u, alignment_functions),
    CPU_TEST_GROUP("slots", 5u, slots_functions),
    CPU_TEST_GROUP("slots_bounds", 27u, slots_bounds_functions)
};

#undef CPU_TEST_GROUP
#undef CPU_TEST_ARRAY_COUNT

int main(int argc, char **argv)
{
    const cpu_test_group *selected = NULL;
    size_t group_index;
    size_t function_index;

    if (argc != 2) {
        return 2;
    }
    printf("TAP version 13\n");
    for (group_index = 0u;
         group_index < sizeof(cpu_test_groups) / sizeof(cpu_test_groups[0]);
         ++group_index) {
        if (strcmp(argv[1], cpu_test_groups[group_index].selector) == 0) {
            selected = &cpu_test_groups[group_index];
            break;
        }
    }
    if (selected == NULL) {
        return 2;
    }

    printf("1..%u\n", selected->assertion_count);
    for (function_index = 0u; function_index < selected->function_count;
         ++function_index) {
        selected->functions[function_index]();
    }
    return test_failures == 0u ? 0 : 1;
}
