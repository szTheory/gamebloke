#include "gamebloke/gamebloke.h"

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
}

static bool run_units(gbl_machine *machine, uint64_t cycles, uint64_t units,
                      gbl_run_result *result)
{
    const gbl_run_options options = { cycles, units };

    return gbl_cpu_run(machine, &options, result) == GBL_RUN_STATUS_OK;
}

static bool run_single_instruction(gbl_machine *machine, uint32_t opcode,
                                   uint32_t cpsr, gbl_run_result *result)
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
    return run_units(machine, 1u, 1u, result);
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
    machine.cpu.r0_7[1] = 0u;
    ran = loaded && run_single_instruction(&machine, UINT32_C(0xE1B02110),
                                            TEST_CPSR_C, &result);
    test_case(ran && machine.cpu.r0_7[2] == UINT32_C(0x80000001) &&
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "register shift count zero preserves the value and old carry");

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
                  (machine.cpu.cpsr & TEST_CPSR_C) != 0u,
              "register LSL count 32 produces zero and old bit zero carry");

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
    } else if (strcmp(argv[1], "budget") == 0) {
        printf("1..9\n");
        test_budget();
    } else if (strcmp(argv[1], "arithmetic") == 0) {
        printf("1..13\n");
        test_arithmetic();
    } else {
        return 2;
    }
    return test_failures == 0u ? 0 : 1;
}
