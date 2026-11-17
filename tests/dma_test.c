#include "gamebloke/gamebloke.h"
#include "state_check.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int check_count;
static unsigned int failure_count;

static void test_case(bool passed, const char *description)
{
    ++check_count;
    if (passed) {
        printf("ok %u - %s\n", check_count, description);
    } else {
        printf("not ok %u - %s\n", check_count, description);
        ++failure_count;
    }
}

static gbl_machine *new_machine(void)
{
    gbl_machine *machine = calloc(1u, sizeof(*machine));

    if (machine == NULL ||
        gbl_machine_init(machine, NULL) != GBL_BUS_STATUS_OK) {
        free(machine);
        return NULL;
    }
    return machine;
}

static void free_machine(gbl_machine *machine)
{
    if (machine != NULL) {
        gbl_machine_dispose(machine);
        free(machine);
    }
}

static uint32_t dma_channel_base(uint8_t channel)
{
    return UINT32_C(0x040000B0) + (uint32_t)channel * UINT32_C(12);
}

static bool write_register(gbl_machine *machine, uint8_t channel,
                           uint8_t offset, uint8_t width, uint32_t value)
{
    const gbl_bus_result result = gbl_bus_write(
        machine, dma_channel_base(channel) + offset, width,
        GBL_BUS_CLASS_NONSEQUENTIAL, value);

    return result.status == GBL_BUS_STATUS_OK &&
           result.outcome == GBL_BUS_OUTCOME_IO_DMA_SUBSET;
}

static bool read_register(gbl_machine *machine, uint8_t channel,
                          uint8_t offset, uint8_t width, uint32_t expected)
{
    const gbl_bus_result result = gbl_bus_read(
        machine, dma_channel_base(channel) + offset, width,
        GBL_BUS_CLASS_NONSEQUENTIAL);

    return result.status == GBL_BUS_STATUS_OK &&
           result.outcome == GBL_BUS_OUTCOME_IO_DMA_SUBSET &&
           result.value == expected;
}

static bool test_channel_register_masks(void)
{
    gbl_machine *machine = new_machine();
    bool valid = machine != NULL;
    uint8_t channel;

    if (machine == NULL) {
        return false;
    }
    for (channel = 0u; channel < GBL_DMA_CHANNEL_COUNT; ++channel) {
        const uint32_t source = UINT32_C(0xF1234567);
        const uint32_t destination = UINT32_C(0xE2345679);
        const uint32_t source_mask = channel == 0u
                                         ? UINT32_C(0x07FFFFFF)
                                         : UINT32_C(0x0FFFFFFF);
        const uint32_t destination_mask = channel == 3u
                                              ? UINT32_C(0x0FFFFFFF)
                                              : UINT32_C(0x07FFFFFF);
        const uint16_t count = UINT16_C(0xF234);
        const uint16_t count_mask = channel == 3u
                                        ? UINT16_MAX
                                        : UINT16_C(0x3FFF);
        const gbl_dma_channel_state *state = &machine->dma_channels[channel];

        valid = valid && write_register(machine, channel, 0u, 4u, source) &&
                write_register(machine, channel, 4u, 4u, destination) &&
                write_register(machine, channel, 8u, 2u, count) &&
                state->source_address == (source & source_mask) &&
                state->destination_address == (destination & destination_mask) &&
                state->count == (uint16_t)(count & count_mask);
    }
    free_machine(machine);
    return valid;
}

static bool test_programming_widths_and_enable_edges(void)
{
    gbl_machine *machine = new_machine();
    bool valid;
    uint32_t base;

    if (machine == NULL) {
        return false;
    }
    base = dma_channel_base(0u);
    valid = write_register(machine, 0u, 0u, 1u, UINT32_C(0x67)) &&
            write_register(machine, 0u, 1u, 2u, UINT32_C(0x1234)) &&
            write_register(machine, 0u, 0u, 4u, UINT32_C(0xF1234567)) &&
            write_register(machine, 0u, 4u, 1u, UINT32_C(0x79)) &&
            write_register(machine, 0u, 4u, 2u, UINT32_C(0x5678)) &&
            write_register(machine, 0u, 4u, 4u, UINT32_C(0xE2345679)) &&
            machine->dma_channels[0].source_address ==
                (UINT32_C(0xF1234567) & UINT32_C(0x07FFFFFF)) &&
            machine->dma_channels[0].destination_address ==
                (UINT32_C(0xE2345679) & UINT32_C(0x07FFFFFF)) &&
            write_register(machine, 0u, 8u, 2u, 1u) &&
            write_register(machine, 0u, 10u, 2u, UINT32_C(0x8000)) &&
            machine->dma_channels[0].pending &&
            machine->dma_channels[0].current_source ==
                (machine->dma_channels[0].source_address & ~UINT32_C(1)) &&
            machine->dma_channels[0].current_destination ==
                (machine->dma_channels[0].destination_address &
                 ~UINT32_C(1)) &&
            write_register(machine, 0u, 0u, 4u, UINT32_C(0x02001003)) &&
            machine->dma_channels[0].current_source !=
                machine->dma_channels[0].source_address &&
            write_register(machine, 0u, 10u, 2u, 0u) &&
            !machine->dma_channels[0].pending &&
            write_register(machine, 0u, 10u, 2u, UINT32_C(0x8000)) &&
            machine->dma_channels[0].current_source ==
                (machine->dma_channels[0].source_address & ~UINT32_C(1)) &&
            base == UINT32_C(0x040000B0);
    free_machine(machine);
    return valid;
}

static bool test_count_zero_expansion_and_width_alignment(void)
{
    gbl_machine *machine = new_machine();
    bool valid;
    uint32_t channel_three_remaining;

    if (machine == NULL) {
        return false;
    }
    valid = write_register(machine, 0u, 0u, 4u, UINT32_C(0x02000103)) &&
            write_register(machine, 0u, 4u, 4u, UINT32_C(0x02000207)) &&
            write_register(machine, 0u, 8u, 2u, 0u) &&
            write_register(machine, 0u, 10u, 2u, UINT32_C(0x8400)) &&
            machine->dma_channels[0].remaining == UINT32_C(0x4000) &&
            machine->dma_channels[0].current_source == UINT32_C(0x02000100) &&
            machine->dma_channels[0].current_destination == UINT32_C(0x02000204) &&
            write_register(machine, 3u, 0u, 4u, UINT32_C(0x02000103)) &&
            write_register(machine, 3u, 4u, 4u, UINT32_C(0x02000207)) &&
            write_register(machine, 3u, 8u, 2u, 0u) &&
            write_register(machine, 3u, 10u, 2u, UINT32_C(0x8000));
    channel_three_remaining = machine->dma_channels[3].remaining;
    valid = valid && channel_three_remaining == UINT32_C(0x10000) &&
            machine->dma_channels[3].current_source == UINT32_C(0x02000102) &&
            machine->dma_channels[3].current_destination == UINT32_C(0x02000206);
    free_machine(machine);
    return valid;
}

static bool test_delayed_enable_and_write_only_readback(void)
{
    gbl_machine *machine = new_machine();
    bool valid;

    if (machine == NULL) {
        return false;
    }
    valid = write_register(machine, 1u, 0u, 4u, UINT32_C(0x02000010)) &&
            write_register(machine, 1u, 4u, 4u, UINT32_C(0x02000100)) &&
            write_register(machine, 1u, 8u, 2u, 1u) &&
            write_register(machine, 1u, 10u, 2u, UINT32_C(0x9000)) &&
            (machine->dma_channels[1].control & UINT16_C(0x8000)) != 0u &&
            !machine->dma_channels[1].pending &&
            !machine->dma_channels[1].active;
    machine->open_bus = UINT32_C(0xA1B2C3D4);
    valid = valid && read_register(machine, 1u, 0u, 4u,
                                   UINT32_C(0xA1B2C3D4)) &&
            read_register(machine, 1u, 4u, 4u,
                          UINT32_C(0xA1B2C3D4)) &&
            read_register(machine, 1u, 8u, 2u,
                          UINT32_C(0xC3D4)) &&
            read_register(machine, 1u, 10u, 2u,
                          UINT32_C(0x9000));
    free_machine(machine);
    return valid;
}

static bool test_prohibited_control_refuses_atomically(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *before = NULL;
    gbl_bus_result result;
    uint64_t before_digest;
    bool valid;

    if (machine == NULL) {
        return false;
    }
    before = malloc(sizeof(*before));
    if (before == NULL) {
        free_machine(machine);
        return false;
    }
    *before = *machine;
    before_digest = gbl_test_canonical_digest(before);
    result = gbl_bus_write(machine, dma_channel_base(0u) + 10u, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x8060));
    valid = result.status != GBL_BUS_STATUS_OK &&
            gbl_test_canonical_equal(before, machine) &&
            gbl_test_same_instance_identity_equal(before, machine) &&
            gbl_test_canonical_digest(machine) == before_digest;
    free(before);
    free_machine(machine);
    return valid;
}

static bool test_channel_zero_transfer_and_irq(void)
{
    gbl_machine *machine = new_machine();
    gbl_dma_step_result step = { 0 };
    gbl_dma_status startup;
    gbl_dma_status unit;
    bool valid;

    if (machine == NULL) {
        return false;
    }
    machine->ewram[0x20u] = UINT8_C(0x5A);
    machine->ewram[0x21u] = UINT8_C(0xC3);
    valid = write_register(machine, 0u, 0u, 4u, UINT32_C(0x02000020)) &&
            write_register(machine, 0u, 4u, 4u, UINT32_C(0x03000100)) &&
            write_register(machine, 0u, 8u, 2u, 1u) &&
            write_register(machine, 0u, 10u, 2u, UINT32_C(0xC000));
    startup = valid ? gbl_machine_dma_step(machine, GBL_DMA_STARTUP_CYCLES,
                                             &step)
                    : GBL_DMA_STATUS_INVALID_ARGUMENT;
    unit = startup == GBL_DMA_STATUS_OK
               ? gbl_machine_dma_step(machine, 2u, &step)
               : GBL_DMA_STATUS_INVALID_ARGUMENT;
    valid = valid && startup == GBL_DMA_STATUS_OK &&
            unit == GBL_DMA_STATUS_OK && step.unit_completed &&
            machine->iwram[0x100u] == UINT8_C(0x5A) &&
            machine->iwram[0x101u] == UINT8_C(0xC3) &&
            (machine->interrupt_flags & UINT16_C(0x0100)) != 0u &&
            !machine->dma_channels[0].active &&
            (machine->dma_channels[0].control & UINT16_C(0x8000)) == 0u;
    free_machine(machine);
    return valid;
}

static bool configure_dma(gbl_machine *machine, uint8_t channel,
                          uint32_t source, uint32_t destination,
                          uint16_t count, uint16_t control)
{
    return write_register(machine, channel, 0u, 4u, source) &&
           write_register(machine, channel, 4u, 4u, destination) &&
           write_register(machine, channel, 8u, 2u, count) &&
           write_register(machine, channel, 10u, 2u, control);
}

static bool run_dma_unit(gbl_machine *machine, uint8_t channel,
                         gbl_dma_step_result *result)
{
    return gbl_machine_dma_step(machine, GBL_DMA_STARTUP_CYCLES, result) ==
               GBL_DMA_STATUS_OK &&
           !result->unit_completed && result->channel == channel &&
           gbl_machine_dma_step(machine, UINT64_C(64), result) ==
               GBL_DMA_STATUS_OK &&
           result->unit_completed && result->channel == channel;
}

static bool test_dma_latches_are_isolated_and_disclosed(void)
{
    gbl_machine *machine = new_machine();
    gbl_dma_step_result ch0_halfword = { 0 };
    gbl_dma_step_result ch1_word = { 0 };
    gbl_dma_step_result ch0_fallback = { 0 };
    gbl_dma_step_result ch2_fallback = { 0 };
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    machine->ewram[0x100u] = UINT8_C(0x12);
    machine->ewram[0x101u] = UINT8_C(0x34);
    machine->ewram[0x120u] = UINT8_C(0xB0);
    machine->ewram[0x121u] = UINT8_C(0xA1);
    machine->ewram[0x122u] = UINT8_C(0xD2);
    machine->ewram[0x123u] = UINT8_C(0xC3);
    valid = configure_dma(machine, 0u, UINT32_C(0x02000100),
                          UINT32_C(0x03000100), 1u, UINT16_C(0x8000)) &&
            run_dma_unit(machine, 0u, &ch0_halfword) &&
            machine->dma_channels[0].read_latch == UINT32_C(0x34123412) &&
            configure_dma(machine, 1u, UINT32_C(0x02000120),
                          UINT32_C(0x03000120), 1u,
                          UINT16_C(0x8400)) &&
            run_dma_unit(machine, 1u, &ch1_word) &&
            machine->dma_channels[1].read_latch == UINT32_C(0xC3D2A1B0) &&
            machine->dma_channels[0].read_latch == UINT32_C(0x34123412);
    if (valid) {
        const uint32_t global_open_bus = machine->open_bus;

        valid = configure_dma(machine, 0u, 0u, UINT32_C(0x03000140), 1u,
                              UINT16_C(0x8400)) &&
                run_dma_unit(machine, 0u, &ch0_fallback) &&
                ch0_fallback.used_channel_latch &&
                ch0_fallback.value_provenance ==
                    GBL_DMA_VALUE_PROVENANCE_PROJECT_SELECTED &&
                ch0_fallback.result_classification ==
                    GBL_DMA_RESULT_CLASSIFICATION_DISPUTED &&
                ch0_fallback.dispute == GBL_DMA_DISPUTE_LATCH &&
                ch0_fallback.hardware_qualification ==
                    GBL_HARDWARE_QUALIFICATION_UNKNOWN &&
                machine->iwram[0x140u] == UINT8_C(0x12) &&
                machine->iwram[0x141u] == UINT8_C(0x34) &&
                machine->iwram[0x142u] == UINT8_C(0x12) &&
                machine->iwram[0x143u] == UINT8_C(0x34) &&
                machine->open_bus == global_open_bus &&
                configure_dma(machine, 2u, 0u, UINT32_C(0x03000160), 1u,
                              UINT16_C(0x8000)) &&
                run_dma_unit(machine, 2u, &ch2_fallback) &&
                ch2_fallback.used_channel_latch &&
                ch2_fallback.value_provenance ==
                    GBL_DMA_VALUE_PROVENANCE_PROJECT_SELECTED &&
                ch2_fallback.result_classification ==
                    GBL_DMA_RESULT_CLASSIFICATION_DISPUTED &&
                ch2_fallback.dispute == GBL_DMA_DISPUTE_LATCH &&
                ch2_fallback.hardware_qualification ==
                    GBL_HARDWARE_QUALIFICATION_UNKNOWN &&
                machine->dma_channels[2].read_latch == 0u &&
                machine->iwram[0x160u] == 0u &&
                machine->iwram[0x161u] == 0u &&
                machine->dma_channels[0].read_latch ==
                    UINT32_C(0x34123412);
    }
    if (valid) {
        gbl_machine_reset(machine);
        valid = machine->dma_channels[0].read_latch == 0u &&
                machine->dma_channels[1].read_latch == 0u &&
                machine->dma_channels[2].read_latch == 0u &&
                machine->dma_channels[3].read_latch == 0u &&
                machine->event_count == 0u && !machine->dma_stream_valid;
    }
    test_case(valid,
              "retained channel latches isolate halfword replication, word replacement, absent-source fallback, open bus, and reset");
    if (valid) {
        printf("phase9-observation:{\"schema\":\"gamebloke.phase9-observation/1\",\"kind\":\"dma-latch\",\"cases\":[{\"channel\":0,\"mode\":\"halfword-replicated\",\"value\":\"0x34123412\",\"used_channel_latch\":false,\"expected_value_provenance\":\"project-selected\",\"result_classification\":\"disputed\",\"dispute_id\":\"disp-dma-latch\",\"hardware_qualification\":\"UNKNOWN\"},{\"channel\":1,\"mode\":\"word-replaced\",\"value\":\"0xC3D2A1B0\",\"used_channel_latch\":false,\"expected_value_provenance\":\"project-selected\",\"result_classification\":\"disputed\",\"dispute_id\":\"disp-dma-latch\",\"hardware_qualification\":\"UNKNOWN\"},{\"channel\":0,\"mode\":\"absent-source-recall\",\"value\":\"0x34123412\",\"used_channel_latch\":true,\"expected_value_provenance\":\"project-selected\",\"result_classification\":\"disputed\",\"dispute_id\":\"disp-dma-latch\",\"hardware_qualification\":\"UNKNOWN\"},{\"channel\":2,\"mode\":\"reset-latch-zero\",\"value\":\"0x00000000\",\"used_channel_latch\":true,\"expected_value_provenance\":\"project-selected\",\"result_classification\":\"disputed\",\"dispute_id\":\"disp-dma-latch\",\"hardware_qualification\":\"UNKNOWN\"}],\"prefetch\":\"unmodeled\"}\n");
    }
    free_machine(machine);
    return true;
}

static bool test_dma_refusal_transactions_are_atomic(void)
{
    gbl_machine *machine = new_machine();
    gbl_dma_step_result step = { 0 };
    gbl_machine *before = NULL;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    before = malloc(sizeof(*before));
    if (before == NULL) {
        free_machine(machine);
        return false;
    }
    machine->ewram[0x200u] = UINT8_C(0xA7);
    valid = configure_dma(machine, 3u, UINT32_C(0x02000200),
                          UINT32_C(0x04000000), 1u, UINT16_C(0x8000)) &&
            gbl_machine_dma_step(machine, 2u, &step) == GBL_DMA_STATUS_OK;
    if (valid) {
        *before = *machine;
        valid = gbl_machine_dma_step(machine, 64u, &step) ==
                    GBL_DMA_STATUS_UNIT_REFUSAL &&
                gbl_test_canonical_equal(before, machine) &&
                machine->ewram[0x200u] == UINT8_C(0xA7) &&
                machine->dma_channels[3].read_latch == 0u;
    }
    test_case(valid,
              "IO destination refusal preserves the complete source, latch, time, and owner state");
    free_machine(machine);

    machine = new_machine();
    if (machine == NULL) {
        free(before);
        return false;
    }
    valid = configure_dma(machine, 3u, UINT32_C(0x02000220),
                          UINT32_C(0x03000220), 1u, UINT16_C(0x8000)) &&
            gbl_machine_dma_step(machine, 2u, &step) == GBL_DMA_STATUS_OK;
    if (valid) {
        *before = *machine;
        valid = gbl_machine_dma_step(machine, 1u, &step) ==
                    GBL_DMA_STATUS_BUDGET_REFUSAL &&
                gbl_test_canonical_equal(before, machine);
    }
    test_case(valid,
              "one-cycle-short DMA unit budget refuses without consuming startup or a partial unit");
    free_machine(machine);

    machine = new_machine();
    if (machine == NULL) {
        free(before);
        return false;
    }
    valid = configure_dma(machine, 0u, UINT32_C(0x02000240),
                          UINT32_C(0x03000240), 1u, UINT16_C(0x9000));
    if (valid) {
        machine->event_count = GBL_EVENT_CAPACITY;
        *before = *machine;
        valid = gbl_machine_dma_request(
                    machine, 0u, machine->master_cycles + 1u,
                    machine->dma_channels[0].generation) ==
                    GBL_TIME_STATUS_EVENT_CAPACITY &&
                gbl_test_canonical_equal(before, machine);
    }
    test_case(valid,
              "ready-event capacity refusal leaves all channel and scheduler state unchanged");
    free_machine(machine);

    machine = new_machine();
    if (machine == NULL) {
        free(before);
        return false;
    }
    valid = configure_dma(machine, 0u, UINT32_C(0x02000260),
                          UINT32_C(0x03000260), 1u, UINT16_C(0x9000));
    if (valid) {
        *before = *machine;
        valid = gbl_machine_dma_request(
                    machine, 0u, machine->master_cycles + 1u,
                    machine->dma_channels[0].generation + 1u) ==
                    GBL_TIME_STATUS_INVALID_ARGUMENT &&
                gbl_test_canonical_equal(before, machine);
    }
    test_case(valid,
              "stale request generation is refused without creating a readiness edge");
    free_machine(machine);
    free(before);
    return true;
}

static bool test_dma_scheduler_dispatch_limit_is_atomic(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *before = NULL;
    gbl_bus_result timer;
    uint64_t target;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    before = malloc(sizeof(*before));
    if (before == NULL) {
        free_machine(machine);
        return false;
    }
    valid = configure_dma(machine, 0u, UINT32_C(0x02000280),
                          UINT32_C(0x03000280), 1u, UINT16_C(0x9000));
    timer = gbl_bus_write(machine, UINT32_C(0x04000100), 4u,
                          GBL_BUS_CLASS_NONSEQUENTIAL,
                          UINT32_C(0x00C0FFFF));
    target = machine->master_cycles + UINT64_C(5000);
    valid = valid && timer.status == GBL_BUS_STATUS_OK &&
            gbl_machine_dma_request(
                machine, 0u, target,
                machine->dma_channels[0].generation) == GBL_TIME_STATUS_OK;
    if (valid) {
        *before = *machine;
        valid = gbl_machine_advance_to(machine, target) ==
                    GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT &&
                gbl_test_canonical_equal(before, machine);
    }
    test_case(valid,
              "timer dispatch-limit refusal retains the queued DMA request and rejects the whole time advance");
    free_machine(machine);
    free(before);
    return valid;
}

typedef struct gbl_dma_observation_test_value {
    uint32_t transfer_value;
    uint32_t source_address;
    uint64_t unit_cycles;
    gbl_machine_bus_owner owner;
    bool provenance_project_selected;
    bool classified_disputed;
    bool dispute_named;
    bool qualification_unknown;
} gbl_dma_observation_test_value;

static const char *dma_observation_failure(
    const gbl_dma_observation_test_value *expected,
    const gbl_dma_observation_test_value *actual)
{
    if (expected->transfer_value != actual->transfer_value) {
        return "dma-transfer-value";
    }
    if (expected->unit_cycles != actual->unit_cycles) {
        return "dma-unit-cost";
    }
    if (expected->owner != actual->owner) {
        return "dma-owner";
    }
    if (expected->source_address != actual->source_address) {
        return "dma-mask";
    }
    if (expected->provenance_project_selected !=
            actual->provenance_project_selected ||
        expected->classified_disputed != actual->classified_disputed ||
        expected->dispute_named != actual->dispute_named ||
        expected->qualification_unknown != actual->qualification_unknown) {
        return "dma-latch-disclosure";
    }
    return NULL;
}

static bool test_dma_controls_and_semantic_twins(void)
{
    gbl_machine *machine = new_machine();
    gbl_dma_step_result step = { 0 };
    gbl_dma_observation_test_value expected = {
        UINT32_C(0x1234), UINT32_C(0x02000100), 4u,
        GBL_MACHINE_BUS_OWNER_DMA, true, true, true, true
    };
    gbl_dma_observation_test_value actual = expected;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    machine->ewram[0x300u] = 1u;
    machine->ewram[0x302u] = 2u;
    machine->ewram[0x304u] = 3u;
    valid = configure_dma(machine, 3u, UINT32_C(0x02000300),
                          UINT32_C(0x03000300), 2u, UINT16_C(0x8000)) &&
            gbl_machine_dma_step(machine, 2u, &step) == GBL_DMA_STATUS_OK &&
            gbl_machine_dma_step(machine, 2u, &step) == GBL_DMA_STATUS_OK &&
            machine->dma_channels[3].current_source ==
                UINT32_C(0x02000302) &&
            machine->dma_channels[3].current_destination ==
                UINT32_C(0x03000302) &&
            write_register(machine, 3u, 10u, 2u, 0u);
    if (valid) {
        valid = configure_dma(machine, 3u, UINT32_C(0x02000304),
                              UINT32_C(0x03000304), 2u,
                              UINT16_C(0x80A0)) &&
                gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK &&
                gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK &&
                machine->dma_channels[3].current_source ==
                    UINT32_C(0x02000302) &&
                machine->dma_channels[3].current_destination ==
                    UINT32_C(0x03000302) &&
                write_register(machine, 3u, 10u, 2u, 0u);
    }
    if (valid) {
        valid = configure_dma(machine, 3u, UINT32_C(0x02000320),
                              UINT32_C(0x03000320), 2u,
                              UINT16_C(0x8140)) &&
                gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK &&
                gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK &&
                machine->dma_channels[3].current_source ==
                    UINT32_C(0x02000320) &&
                machine->dma_channels[3].current_destination ==
                    UINT32_C(0x03000320) &&
                write_register(machine, 3u, 10u, 2u, 0u);
    }
    if (valid) {
        valid = configure_dma(machine, 3u, UINT32_C(0x02000340),
                              UINT32_C(0x03000340), 1u,
                              UINT16_C(0x9260)) &&
                machine->dma_channels[3].armed &&
                !machine->dma_channels[3].pending;
        if (valid) {
            const uint64_t generation = machine->dma_channels[3].generation;
            const uint64_t request_cycle = machine->master_cycles + 1u;

            valid = gbl_machine_dma_request(machine, 3u, request_cycle,
                                             generation) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_machine_advance_to(machine, request_cycle) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_machine_dma_step(machine, 2u, &step) ==
                        GBL_DMA_STATUS_OK &&
                    gbl_machine_dma_step(machine, 2u, &step) ==
                        GBL_DMA_STATUS_OK &&
                    machine->dma_channels[3].armed &&
                    machine->dma_channels[3].remaining == 1u &&
                    machine->dma_channels[3].current_destination ==
                        UINT32_C(0x03000340) &&
                    machine->dma_channels[3].current_source ==
                        UINT32_C(0x02000342) &&
                    write_register(machine, 3u, 10u, 2u, 0u);
        }
    }
    if (valid) {
        valid = configure_dma(machine, 3u, UINT32_C(0x02000360),
                              UINT32_C(0x03000360), 1u,
                              UINT16_C(0x8200)) &&
                run_dma_unit(machine, 3u, &step) &&
                (machine->dma_channels[3].control & UINT16_C(0x8000)) == 0u &&
                !machine->dma_channels[3].armed &&
                !machine->dma_channels[3].pending;
    }
    if (valid) {
        valid = configure_dma(machine, 1u, UINT32_C(0x02000380),
                              UINT32_C(0x03000380), 1u,
                              UINT16_C(0xA000)) &&
                machine->dma_channels[1].armed &&
                !machine->dma_channels[1].pending &&
                configure_dma(machine, 2u, UINT32_C(0x020003A0),
                              UINT32_C(0x030003A0), 1u,
                              UINT16_C(0xB000)) &&
                machine->dma_channels[2].armed &&
                !machine->dma_channels[2].pending &&
                configure_dma(machine, 0u, UINT32_C(0x020003C0),
                              UINT32_C(0x030003C0), 1u,
                              UINT16_C(0x9000));
    }
    if (valid) {
        const gbl_machine before = *machine;
        const gbl_bus_result invalid = gbl_bus_write(
            machine, dma_channel_base(0u) + 10u, 2u,
            GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xB000));

        valid = invalid.status != GBL_BUS_STATUS_OK &&
                gbl_test_canonical_equal(&before, machine);
    }
    test_case(valid,
              "source and destination address modes progress independently; delayed repeat reloads count and destination while immediate repeat stays one-shot");
    if (valid) {
        printf("phase9-observation:{\"schema\":\"gamebloke.phase9-observation/1\",\"kind\":\"dma-control-policy\",\"immediate_repeat\":\"one-shot\",\"delayed_repeat\":\"armed\",\"destination_reload\":\"channel3\",\"production_triggers\":\"deferred\",\"hardware_qualification\":\"UNKNOWN\"}\n");
    }
    free_machine(machine);

    actual = expected;
    test_case(dma_observation_failure(&expected, &actual) == NULL,
              "clean semantic observation twin passes the named classifier");
    actual.transfer_value ^= 1u;
    test_case(strcmp(dma_observation_failure(&expected, &actual),
                     "dma-transfer-value") == 0,
              "wrong transfer value twin fails as dma-transfer-value");
    actual = expected;
    ++actual.unit_cycles;
    test_case(strcmp(dma_observation_failure(&expected, &actual),
                     "dma-unit-cost") == 0,
              "wrong unit cost twin fails as dma-unit-cost");
    actual = expected;
    actual.owner = GBL_MACHINE_BUS_OWNER_CPU;
    test_case(strcmp(dma_observation_failure(&expected, &actual),
                     "dma-owner") == 0,
              "wrong owner twin fails as dma-owner");
    actual = expected;
    actual.source_address |= UINT32_C(0xF0000000);
    test_case(strcmp(dma_observation_failure(&expected, &actual),
                     "dma-mask") == 0,
              "wrong channel mask twin fails as dma-mask");
    actual = expected;
    actual.dispute_named = false;
    test_case(strcmp(dma_observation_failure(&expected, &actual),
                     "dma-latch-disclosure") == 0,
              "missing latch dispute twin fails as dma-latch-disclosure");
    return true;
}

static bool test_dma_priority_and_resume(void)
{
    gbl_machine *machine = new_machine();
    gbl_dma_step_result step = { 0 };
    uint64_t ready_cycle;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    machine->ewram[0x20u] = UINT8_C(0x10);
    machine->ewram[0x30u] = UINT8_C(0x30);
    machine->iwram[0x20u] = UINT8_C(0xEE);
    machine->iwram[0x30u] = UINT8_C(0xEE);
    valid = configure_dma(machine, 0u, UINT32_C(0x02000020),
                          UINT32_C(0x03000020), 1u, UINT16_C(0x9000)) &&
            configure_dma(machine, 3u, UINT32_C(0x02000030),
                          UINT32_C(0x03000030), 1u, UINT16_C(0x9000));
    ready_cycle = machine->master_cycles + 2u;
    valid = valid && gbl_machine_dma_request(
                         machine, 3u, ready_cycle,
                         machine->dma_channels[3].generation) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_machine_dma_request(
                         machine, 0u, ready_cycle,
                         machine->dma_channels[0].generation) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_machine_advance_to(machine, ready_cycle) ==
                         GBL_TIME_STATUS_OK &&
                     machine->dma_channels[0].pending &&
                     machine->dma_channels[3].pending;
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 0u &&
                step.startup_cycles == 2u && !step.unit_completed;
    }
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 0u &&
                step.unit_completed && machine->iwram[0x20u] == 0x10u &&
                machine->dma_channels[3].pending;
    }
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 3u &&
                step.startup_cycles == 2u;
    }
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 3u &&
                step.unit_completed && machine->iwram[0x30u] == 0x30u &&
                machine->bus_owner == GBL_MACHINE_BUS_OWNER_CPU;
    }
    test_case(valid,
              "same-cycle delayed channels start in DMA0 to DMA3 priority order");
    free_machine(machine);

    machine = new_machine();
    if (machine == NULL) {
        return false;
    }
    machine->ewram[0x40u] = UINT8_C(0x41);
    machine->ewram[0x42u] = UINT8_C(0x42);
    machine->ewram[0x50u] = UINT8_C(0x50);
    machine->iwram[0x40u] = 0u;
    machine->iwram[0x42u] = 0u;
    machine->iwram[0x50u] = 0u;
    valid = configure_dma(machine, 3u, UINT32_C(0x02000040),
                          UINT32_C(0x03000040), 2u, UINT16_C(0x8000)) &&
            configure_dma(machine, 0u, UINT32_C(0x02000050),
                          UINT32_C(0x03000050), 1u, UINT16_C(0x9000));
    valid = valid && gbl_machine_dma_step(machine, 2u, &step) ==
                         GBL_DMA_STATUS_OK && step.channel == 3u &&
                     step.startup_cycles == 2u;
    ready_cycle = machine->master_cycles + 1u;
    valid = valid && gbl_machine_dma_request(
                         machine, 0u, ready_cycle,
                         machine->dma_channels[0].generation) ==
                         GBL_TIME_STATUS_OK;
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 3u &&
                step.unit_completed && machine->dma_channels[0].pending &&
                machine->dma_channels[3].active &&
                machine->dma_channels[3].remaining == 1u &&
                machine->dma_channels[3].current_source ==
                    UINT32_C(0x02000042);
    }
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 0u &&
                step.startup_cycles == 2u;
    }
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 0u &&
                step.unit_completed && machine->iwram[0x50u] == 0x50u &&
                machine->bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
                machine->dma_channels[3].active;
    }
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 3u &&
                step.unit_completed &&
                step.source_sequence == GBL_BUS_CLASS_NONSEQUENTIAL &&
                step.destination_sequence == GBL_BUS_CLASS_NONSEQUENTIAL &&
                machine->iwram[0x42u] == 0x42u &&
                machine->bus_owner == GBL_MACHINE_BUS_OWNER_CPU;
    }
    test_case(valid,
              "a DMA0 request waits for a DMA3 unit then suspends and resumes it at the next boundary");
    if (valid) {
        printf("phase9-observation:{\"schema\":\"gamebloke.phase9-observation/1\",\"kind\":\"dma-arbitration\",\"priority\":[0,1,2,3],\"resume_channel\":3,\"resume_sequence\":\"N\",\"startup_cycles\":2,\"startup_provenance\":\"project-selected\",\"hardware_qualification\":\"UNKNOWN\"}\n");
    }
    free_machine(machine);

    machine = new_machine();
    if (machine == NULL) {
        test_case(false,
                  "same-cycle DMA-ready event is applied before unit completion effects");
        return false;
    }
    valid = configure_dma(machine, 3u, UINT32_C(0x02000080),
                          UINT32_C(0x03000080), 1u, UINT16_C(0xC000)) &&
            configure_dma(machine, 0u, UINT32_C(0x02000090),
                          UINT32_C(0x03000090), 1u, UINT16_C(0x9000));
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 3u &&
                step.startup_cycles == 2u;
    }
    ready_cycle = machine->master_cycles + 2u;
    valid = valid && gbl_machine_dma_request(
                         machine, 0u, ready_cycle,
                         machine->dma_channels[0].generation) ==
                         GBL_TIME_STATUS_OK;
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 3u &&
                step.unit_completed &&
                machine->dma_channels[0].pending &&
                !machine->dma_channels[3].active &&
                (machine->interrupt_flags & UINT16_C(0x0800)) != 0u &&
                machine->bus_owner == GBL_MACHINE_BUS_OWNER_DMA;
    }
    test_case(valid,
              "same-cycle DMA-ready event is applied before unit completion effects");
    free_machine(machine);
    return true;
}

static bool test_dma_events_during_startup_and_unit(void)
{
    gbl_machine *machine = new_machine();
    gbl_dma_step_result step = { 0 };
    gbl_input_event inputs[3];
    uint64_t base_cycle;
    uint64_t generation;
    gbl_bus_result timer_write;
    bool priority_order;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    machine->ewram[0x60u] = UINT8_C(0xA6);
    valid = configure_dma(machine, 3u, UINT32_C(0x02000060),
                          UINT32_C(0x03000060), 2u, UINT16_C(0x8000)) &&
            configure_dma(machine, 0u, UINT32_C(0x02000070),
                          UINT32_C(0x03000070), 1u, UINT16_C(0x9000));
    base_cycle = machine->master_cycles;
    inputs[0] = (gbl_input_event){ base_cycle + 2u, 1u };
    inputs[1] = (gbl_input_event){ base_cycle + 4u, 2u };
    inputs[2] = (gbl_input_event){ base_cycle + 5u, 4u };
    valid = valid && gbl_machine_set_input_timeline(machine, inputs, 3u) ==
                         GBL_TIME_STATUS_OK;
    timer_write = gbl_bus_write(machine, UINT32_C(0x04000100), 4u,
                                GBL_BUS_CLASS_NONSEQUENTIAL,
                                UINT32_C(0x00C0FFFE));
    valid = valid && timer_write.status == GBL_BUS_STATUS_OK;
    generation = machine->dma_channels[0].generation;
    valid = valid && gbl_machine_dma_request(
                         machine, 0u, base_cycle + 4u, generation) ==
                         GBL_TIME_STATUS_OK;
    priority_order = false;
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 3u &&
                step.startup_cycles == 2u && machine->master_cycles ==
                    base_cycle + 3u && machine->pressed_keys == 1u &&
                (machine->interrupt_flags & UINT16_C(0x0008)) != 0u &&
                !machine->dma_channels[0].pending;
        priority_order = machine->event_count >= 2u &&
                         machine->events[0].cycle == base_cycle + 4u &&
                         machine->events[0].kind ==
                             GBL_EVENT_KIND_KEYPAD_INPUT &&
                         machine->events[0].priority == 4u &&
                         machine->events[1].cycle == base_cycle + 4u &&
                         machine->events[1].kind ==
                             GBL_EVENT_KIND_DMA_READY &&
                         machine->events[1].priority == 5u;
        valid = valid && priority_order;
    }
    if (valid) {
        valid = gbl_machine_dma_step(machine, 2u, &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 3u &&
                step.unit_completed && step.source_cycles == 1u &&
                step.destination_cycles == 1u &&
                machine->master_cycles == base_cycle + 5u &&
                machine->pressed_keys == 4u &&
                machine->input_event_cursor == 3u &&
                machine->dma_channels[0].pending &&
                machine->dma_channels[3].completed_units == 1u;
    }
    test_case(valid,
              "timer and keypad events run during DMA startup and both unit intervals; readiness waits for the unit boundary");
    if (valid) {
        printf("phase9-observation:{\"schema\":\"gamebloke.phase9-observation/1\",\"kind\":\"dma-events\",\"event_priority\":[\"timer0-3\",\"keypad\",\"dma-ready0-3\"],\"completion_effects\":\"after-due-events\",\"startup_cycles\":2,\"startup_provenance\":\"project-selected\",\"prefetch\":\"unmodeled\",\"hardware_qualification\":\"UNKNOWN\"}\n");
    }
    free_machine(machine);
    return true;
}

static bool run_registers(void)
{
    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n");
    printf("1..6\n");
    test_case(test_channel_register_masks(),
              "four channels apply source, destination, and count masks");
    test_case(test_programming_widths_and_enable_edges(),
              "byte, halfword, and word lanes program registers and enable edges relatch");
    test_case(test_count_zero_expansion_and_width_alignment(),
              "zero count expands per channel and active addresses align to transfer width");
    test_case(test_delayed_enable_and_write_only_readback(),
              "delayed modes stay armed and write-only registers use open-bus readback");
    test_case(test_prohibited_control_refuses_atomically(),
              "prohibited source control refuses without machine or time mutation");
    test_case(test_channel_zero_transfer_and_irq(),
              "DMA0 performs a timed halfword unit and raises its completion IF bit");
    return failure_count == 0u ? 0 : 1;
}

static bool run_arbitration(void)
{
    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n1..3\n");
    (void)test_dma_priority_and_resume();
    return failure_count == 0u ? 0 : 1;
}

static bool run_events(void)
{
    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n1..1\n");
    (void)test_dma_events_during_startup_and_unit();
    return failure_count == 0u ? 0 : 1;
}

static bool run_latch(void)
{
    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n1..1\n");
    (void)test_dma_latches_are_isolated_and_disclosed();
    return failure_count == 0u ? 0 : 1;
}

static bool run_refusal(void)
{
    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n1..5\n");
    (void)test_dma_refusal_transactions_are_atomic();
    (void)test_dma_scheduler_dispatch_limit_is_atomic();
    return failure_count == 0u ? 0 : 1;
}

static bool run_controls(void)
{
    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n1..7\n");
    (void)test_dma_controls_and_semantic_twins();
    return failure_count == 0u ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "registers") == 0) {
        return run_registers();
    }
    if (argc == 2 && strcmp(argv[1], "arbitration") == 0) {
        return run_arbitration();
    }
    if (argc == 2 && strcmp(argv[1], "events") == 0) {
        return run_events();
    }
    if (argc == 2 && strcmp(argv[1], "latch") == 0) {
        return run_latch();
    }
    if (argc == 2 && strcmp(argv[1], "refusal") == 0) {
        return run_refusal();
    }
    if (argc == 2 && strcmp(argv[1], "controls") == 0) {
        return run_controls();
    }
    fprintf(stderr, "dma_test: unknown selector or argument count\n");
    return 2;
}
