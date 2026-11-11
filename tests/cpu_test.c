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

static unsigned int test_number;
static unsigned int test_failures;

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
    (void)context;
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

static bool machine_load(gbl_machine *machine,
                         const uint8_t image[TEST_IMAGE_SIZE])
{
    const gbl_allocator allocator = {
        NULL, cpu_test_allocate, cpu_test_release
    };
    gbl_load_result load_result;

    if (gbl_machine_init(machine, &allocator) != GBL_BUS_STATUS_OK) {
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

static bool cpu_is_direct_start(const gbl_cpu_state *cpu)
{
    size_t index;

    if (cpu->cpsr != UINT32_C(0x000000DF) ||
        cpu->execution_address != UINT32_C(0x08000000) ||
        cpu->pipeline[0].address != 0u || cpu->pipeline[0].opcode != 0u ||
        cpu->pipeline[0].width != 0u || cpu->pipeline[0].valid ||
        cpu->pipeline[1].address != 0u || cpu->pipeline[1].opcode != 0u ||
        cpu->pipeline[1].width != 0u || cpu->pipeline[1].valid ||
        cpu->next_fetch_sequential || cpu->retired_instructions != 0u ||
        cpu->stopped || cpu->stop_reason != GBL_CPU_STOP_NONE) {
        return false;
    }
    for (index = 0u; index < 8u; ++index) {
        if (cpu->r0_7[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < 7u; ++index) {
        if (cpu->usr_r8_14[index] != 0u || cpu->fiq_r8_14[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < 2u; ++index) {
        if (cpu->irq_r13_14[index] != 0u || cpu->svc_r13_14[index] != 0u ||
            cpu->abt_r13_14[index] != 0u || cpu->und_r13_14[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < 5u; ++index) {
        if (cpu->spsr[index] != 0u) {
            return false;
        }
    }
    return true;
}

static bool run_units(gbl_machine *machine, uint64_t cycles, uint64_t units,
                      gbl_run_result *result)
{
    const gbl_run_options options = { cycles, units };

    return gbl_cpu_run(machine, &options, result) == GBL_RUN_STATUS_OK;
}

static void test_lifecycle(void)
{
    uint8_t image[TEST_IMAGE_SIZE];
    uint8_t replacement[TEST_IMAGE_SIZE];
    gbl_machine machine = { 0 };
    gbl_run_result result = { GBL_RUN_TIMEOUT, GBL_RUN_REASON_NONE, 0u, 0u };
    bool loaded;

    make_program(image);
    make_program(replacement);
    loaded = machine_load(&machine, image);
    test_case(loaded && machine.initialized && cpu_is_direct_start(&machine.cpu),
              "load initializes the complete direct-start CPU state");
    if (!loaded) {
        test_case(false, "reset clears CPU banks and restores the direct-start policy");
        test_case(false, "successful replacement load cold-resets pipeline and counters");
        test_case(false, "canonical equality and digest cover every CPU field");
        test_case(false, "invalid run arguments preserve machine and output storage");
        test_case(false, "refused ROM load preserves every persistent CPU field");
        return;
    }

    machine.cpu.r0_7[0] = UINT32_C(0xA5A5A5A5);
    machine.cpu.pipeline[0].valid = true;
    machine.master_cycles = 33u;
    gbl_machine_reset(&machine);
    test_case(cpu_is_direct_start(&machine.cpu) && machine.master_cycles == 0u,
              "reset clears CPU banks and restores the direct-start policy");

    (void)run_units(&machine, 2u, 1u, &result);
    (void)gbl_load_rom(&machine, replacement, sizeof(replacement));
    test_case(cpu_is_direct_start(&machine.cpu) && machine.master_cycles == 0u,
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

        test_case(all_fields_detected,
                  "canonical equality and digest cover every CPU field");
    }

    {
        const gbl_machine before = machine;
        const gbl_run_result output_before = {
            GBL_RUN_PROGRESS, GBL_RUN_REASON_UNIT_LIMIT, 77u, 88u
        };
        gbl_run_result output = output_before;
        const bool refused =
            gbl_cpu_run(&machine, NULL, &output) ==
            GBL_RUN_STATUS_INVALID_ARGUMENT;

        test_case(refused && gbl_test_canonical_equal(&before, &machine) &&
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
        const gbl_load_result refused = gbl_load_rom(&machine, image, 0u);

        test_case(refused.status == GBL_LOAD_STATUS_REFUSED &&
                      refused.reason == GBL_LOAD_REASON_ROM_ZERO &&
                      gbl_test_canonical_equal(&before, &machine) &&
                      gbl_test_same_instance_identity_equal(&before, &machine) &&
                      digest_before == gbl_test_canonical_digest(&machine),
                  "refused ROM load preserves every persistent CPU field");
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

int main(int argc, char **argv)
{
    if (argc != 2) {
        return 2;
    }
    printf("TAP version 13\n");
    if (strcmp(argv[1], "lifecycle") == 0) {
        printf("1..6\n");
        test_lifecycle();
    } else if (strcmp(argv[1], "tracer") == 0) {
        printf("1..4\n");
        test_tracer();
    } else if (strcmp(argv[1], "budget") == 0) {
        printf("1..8\n");
        test_budget();
    } else {
        return 2;
    }
    return test_failures == 0u ? 0 : 1;
}
