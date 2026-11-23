#include "gamebloke/gamebloke.h"
#include "../src/ppu_internal.h"
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

static bool write_sound_fifo(gbl_machine *machine, uint8_t width,
                             uint32_t value)
{
    const gbl_bus_result result = gbl_bus_write(
        machine, UINT32_C(0x040000A0), width,
        GBL_BUS_CLASS_NONSEQUENTIAL, value);

    return result.status == GBL_BUS_STATUS_OK &&
           result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET;
}

static uint64_t next_timer_overflow(const gbl_machine *machine,
                                    uint8_t timer_index)
{
    size_t event_index;

    for (event_index = 0u; event_index < machine->event_count;
         ++event_index) {
        const gbl_event *event = &machine->events[event_index];

        if (event->live &&
            event->kind == (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW &&
            event->timer_index == timer_index &&
            event->generation == machine->timers[timer_index].generation) {
            return event->cycle;
        }
    }
    return UINT64_MAX;
}

static bool test_sound_refill_from_timer_threshold(void)
{
    static const uint32_t initial_words[4] = {
        UINT32_C(0x01020304), UINT32_C(0x11121314),
        UINT32_C(0x21222324), UINT32_C(0x31323334)
    };
    static const uint32_t source_words[4] = {
        UINT32_C(0xA1A2A3A4), UINT32_C(0xB1B2B3B4),
        UINT32_C(0xC1C2C3C4), UINT32_C(0xD1D2D3D4)
    };
    gbl_machine *machine = new_machine();
    gbl_dma_step_result step = { 0 };
    uint64_t deadline = UINT64_MAX;
    uint32_t lane;
    bool setup;
    bool valid;

    if (machine == NULL) {
        return false;
    }
    machine->iwram[UINT32_C(0x100)] = UINT8_C(0x6A);
    for (lane = 0u; lane < 4u; ++lane) {
        const uint32_t value = source_words[lane];
        const size_t offset = (size_t)UINT32_C(0x100) +
                              (size_t)lane * 4u;

        machine->ewram[offset] = (uint8_t)value;
        machine->ewram[offset + 1u] = (uint8_t)(value >> 8u);
        machine->ewram[offset + 2u] = (uint8_t)(value >> 16u);
        machine->ewram[offset + 3u] = (uint8_t)(value >> 24u);
    }
    setup = write_sound_fifo(machine, 4u, initial_words[0]) &&
            write_sound_fifo(machine, 4u, initial_words[1]) &&
            write_sound_fifo(machine, 4u, initial_words[2]) &&
            write_sound_fifo(machine, 4u, initial_words[3]) &&
            write_sound_fifo(machine, 1u, UINT32_C(0x7E)) &&
            write_register(machine, 1u, 0u, 4u,
                           UINT32_C(0x02000100)) &&
            write_register(machine, 1u, 4u, 4u,
                           UINT32_C(0x03000100)) &&
            write_register(machine, 1u, 8u, 2u, 1u) &&
            write_register(machine, 1u, 10u, 2u, UINT32_C(0xB000)) &&
            machine->audio.fifo[0].count == 20u &&
            machine->dma_channels[1].armed &&
            !machine->dma_channels[1].pending &&
            gbl_bus_write(machine, UINT32_C(0x04000100), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL,
                          UINT32_C(0xFFFC)).status == GBL_BUS_STATUS_OK &&
            gbl_bus_write(machine, UINT32_C(0x04000102), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL,
                          UINT32_C(0x0080)).status == GBL_BUS_STATUS_OK;
    if (setup) {
        deadline = next_timer_overflow(machine, 0u);
    }
    setup = setup && deadline != UINT64_MAX && deadline <= UINT64_MAX - 12u &&
            gbl_machine_advance_to(machine, deadline + 12u) ==
                GBL_TIME_STATUS_OK;
    valid = setup && machine->audio.fifo[0].count == 16u &&
            machine->audio.fifo[0].sample == 1 &&
            gbl_machine_dma_step(machine, UINT64_C(1024), &step) ==
                GBL_DMA_STATUS_OK;
    printf("# sound refill setup=%d deadline=%llu fifo_count=%u "
           "dma_status=%u channel=%u completed=%llu remaining=%u\n",
           setup ? 1 : 0, (unsigned long long)deadline,
           machine->audio.fifo[0].count, (unsigned int)step.status,
           (unsigned int)step.channel, (unsigned long long)step.completed,
           step.remaining);
    if (valid) {
        valid = step.channel == 1u && step.completed == 4u &&
                step.unit_completed &&
                machine->audio.fifo[0].count == 30u &&
                machine->audio.fifo[0].read_cursor == 6u &&
                machine->audio.fifo[0].write_cursor == 4u &&
                machine->audio.fifo[0].sample == UINT8_C(0x13) &&
                machine->dma_channels[1].source_address ==
                    UINT32_C(0x02000100) &&
                machine->dma_channels[1].destination_address ==
                    UINT32_C(0x03000100) &&
                machine->dma_channels[1].control == UINT16_C(0xB000) &&
                machine->dma_channels[1].count == 1u &&
                machine->dma_channels[1].current_source ==
                    UINT32_C(0x02000110) &&
                machine->iwram[UINT32_C(0x100)] == UINT8_C(0x6A);
        for (lane = 0u; valid && lane < 16u; ++lane) {
            const uint32_t source_word = source_words[lane / 4u];
            const uint8_t expected = (uint8_t)(
                source_word >> ((lane & 3u) * 8u));

            valid = machine->audio.fifo[0].bytes[(20u + lane) & 31u] ==
                    expected;
        }
    }
    free_machine(machine);
    return valid;
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
                machine->event_count == 1u &&
                machine->events[0].kind ==
                    (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY &&
                !machine->dma_stream_valid;
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

static bool test_ppu_blank_triggers_and_video_row(void)
{
    gbl_machine *vblank = new_machine();
    gbl_machine *hblank = new_machine();
    gbl_machine *unsupported = new_machine();
    gbl_machine *nonvisible = new_machine();
    gbl_dma_step_result step = { 0 };
    uint8_t channel;
    bool valid = vblank != NULL && hblank != NULL && unsupported != NULL &&
                 nonvisible != NULL;

    if (!valid) {
        free_machine(vblank);
        free_machine(hblank);
        free_machine(unsupported);
        free_machine(nonvisible);
        return false;
    }
    for (channel = 0u; channel < GBL_DMA_CHANNEL_COUNT; ++channel) {
        const uint32_t source = UINT32_C(0x02000020) +
                                (uint32_t)channel * 2u;
        const uint32_t destination = UINT32_C(0x03000100) +
                                     (uint32_t)channel * 2u;

        vblank->ewram[0x20u + (size_t)channel * 2u] =
            (uint8_t)(UINT8_C(0x30) + channel);
        valid = valid && configure_dma(vblank, channel, source, destination,
                                       1u, UINT16_C(0x9000));
    }
    valid = valid &&
        gbl_machine_advance_to(
            vblank,
            (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES) ==
            GBL_TIME_STATUS_OK;
    for (channel = 0u; channel < GBL_DMA_CHANNEL_COUNT; ++channel) {
        valid = valid && vblank->dma_channels[channel].pending &&
                !vblank->dma_channels[channel].armed &&
                vblank->dma_channels[channel].trigger_origin ==
                    GBL_DMA_TRIGGER_ORIGIN_VBLANK &&
                vblank->dma_channels[channel].trigger_cycle ==
                    (uint64_t)GBL_PPU_VISIBLE_LINES *
                        GBL_PPU_LINE_CYCLES &&
                vblank->dma_channels[channel].trigger_dispute ==
                    GBL_DMA_DISPUTE_NONE;
    }

    hblank->dispcnt = UINT16_C(0x0403);
    hblank->ppu.dispcnt = hblank->dispcnt;
    hblank->ewram[0x40u] = UINT8_C(0x10);
    hblank->ewram[0x41u] = UINT8_C(0x42);
    for (channel = 0u; channel < GBL_DMA_CHANNEL_COUNT; ++channel) {
        const uint32_t source = channel == 0u
                                    ? UINT32_C(0x02000040)
                                    : UINT32_C(0x02000020) +
                                          (uint32_t)channel * 2u;
        const uint32_t destination = channel == 0u
                                         ? UINT32_C(0x060001E0)
                                         : UINT32_C(0x03000200) +
                                               (uint32_t)channel * 2u;
        const uint16_t control = channel == 0u
                                     ? UINT16_C(0xA200)
                                     : UINT16_C(0xA000);

        valid = valid && configure_dma(hblank, channel, source, destination,
                                       1u, control);
    }
    valid = valid &&
            gbl_machine_advance_to(hblank, GBL_PPU_HBLANK_CYCLES) ==
                GBL_TIME_STATUS_OK;
    for (channel = 0u; channel < GBL_DMA_CHANNEL_COUNT; ++channel) {
        valid = valid && hblank->dma_channels[channel].pending &&
                !hblank->dma_channels[channel].armed &&
                hblank->dma_channels[channel].trigger_origin ==
                    GBL_DMA_TRIGGER_ORIGIN_HBLANK &&
                hblank->dma_channels[channel].trigger_cycle ==
                    GBL_PPU_HBLANK_CYCLES &&
                hblank->dma_channels[channel].trigger_dispute ==
                    GBL_DMA_DISPUTE_HBLANK_FLAG_EDGE;
    }
    valid = valid && hblank->ppu.line == 0u &&
            (hblank->ppu.dispstat & UINT16_C(0x0002)) != 0u &&
            hblank->event_count == 1u &&
            gbl_machine_advance_to(hblank, GBL_PPU_HBLANK_CYCLES) ==
                GBL_TIME_STATUS_OK && hblank->event_count == 1u &&
            hblank->dma_channels[0].pending;
    valid = valid &&
            gbl_machine_dma_step(hblank, GBL_DMA_STARTUP_CYCLES, &step) ==
                GBL_DMA_STATUS_OK &&
            step.channel == 0u && !step.unit_completed &&
            step.startup_cycles == GBL_DMA_STARTUP_CYCLES &&
            step.trigger_origin == GBL_DMA_TRIGGER_ORIGIN_HBLANK &&
            step.trigger_cycle == GBL_PPU_HBLANK_CYCLES &&
            step.trigger_dispute == GBL_DMA_DISPUTE_HBLANK_FLAG_EDGE &&
            step.result_classification ==
                GBL_DMA_RESULT_CLASSIFICATION_DISPUTED &&
            step.hardware_qualification ==
                GBL_HARDWARE_QUALIFICATION_UNKNOWN &&
            gbl_machine_dma_step(hblank, UINT64_C(64), &step) ==
                GBL_DMA_STATUS_OK && step.channel == 0u &&
            step.unit_completed && step.value == UINT32_C(0x4210) &&
            hblank->dma_channels[0].armed &&
            gbl_machine_advance_to(hblank, GBL_PPU_LINE_CYCLES) ==
                GBL_TIME_STATUS_OK &&
            hblank->video_frame[GBL_PPU_ROW_BYTES] == UINT8_C(0x10) &&
            hblank->video_frame[GBL_PPU_ROW_BYTES + 1u] == UINT8_C(0x42) &&
            gbl_machine_advance_to(
                hblank, GBL_PPU_LINE_CYCLES + GBL_PPU_HBLANK_CYCLES) ==
                GBL_TIME_STATUS_OK &&
            hblank->dma_channels[0].pending &&
            !hblank->dma_channels[0].armed &&
            hblank->dma_channels[0].completed_units == 1u;

    for (channel = 1u; channel < GBL_DMA_CHANNEL_COUNT; ++channel) {
        valid = valid && configure_dma(
                            unsupported, channel,
                            UINT32_C(0x02000060) +
                                (uint32_t)channel * 2u,
                            UINT32_C(0x03000300) +
                                (uint32_t)channel * 2u,
                            1u, UINT16_C(0xB000));
    }
    valid = valid &&
            gbl_machine_advance_to(
                unsupported,
                (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES +
                    GBL_PPU_HBLANK_CYCLES) == GBL_TIME_STATUS_OK;
    for (channel = 1u; channel < GBL_DMA_CHANNEL_COUNT; ++channel) {
        valid = valid && unsupported->dma_channels[channel].armed &&
                !unsupported->dma_channels[channel].pending &&
                unsupported->dma_channels[channel].trigger_origin ==
                    GBL_DMA_TRIGGER_ORIGIN_NONE;
    }
    valid = valid &&
            gbl_machine_advance_to(
                nonvisible,
                (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES) ==
                GBL_TIME_STATUS_OK &&
            configure_dma(nonvisible, 0u, UINT32_C(0x02000080),
                          UINT32_C(0x03000380), 1u, UINT16_C(0xA000)) &&
            gbl_machine_advance_to(
                nonvisible,
                (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES +
                    GBL_PPU_HBLANK_CYCLES) == GBL_TIME_STATUS_OK &&
            nonvisible->dma_channels[0].armed &&
            !nonvisible->dma_channels[0].pending &&
            nonvisible->dma_channels[0].trigger_origin ==
                GBL_DMA_TRIGGER_ORIGIN_NONE;

    test_case(valid,
              "VBlank and visible HBlank trigger all channels once, retain timed startup, update a later VRAM row, and re-arm repeats");
    free_machine(vblank);
    free_machine(hblank);
    free_machine(unsupported);
    free_machine(nonvisible);
    return valid;
}

static bool test_ppu_same_cycle_input_order(void)
{
    gbl_machine *machine = new_machine();
    gbl_input_event input = { GBL_PPU_HBLANK_CYCLES, UINT16_C(0x0020) };
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    valid = configure_dma(machine, 0u, UINT32_C(0x02000020),
                          UINT32_C(0x03000100), 1u, UINT16_C(0xA000)) &&
            gbl_machine_set_input_timeline(machine, &input, 1u) ==
                GBL_TIME_STATUS_OK &&
            gbl_machine_advance_to(machine, GBL_PPU_HBLANK_CYCLES) ==
                GBL_TIME_STATUS_OK &&
            machine->pressed_keys == UINT16_C(0x0020) &&
            machine->input_event_cursor == 1u &&
            machine->dma_channels[0].pending &&
            machine->dma_channels[0].trigger_origin ==
                GBL_DMA_TRIGGER_ORIGIN_HBLANK &&
            machine->dma_channels[0].trigger_cycle ==
                GBL_PPU_HBLANK_CYCLES &&
            machine->dma_channels[0].trigger_dispute ==
                GBL_DMA_DISPUTE_HBLANK_FLAG_EDGE &&
            (machine->ppu.dispstat & UINT16_C(0x0002)) != 0u &&
            machine->dma_channels[0].completed_units == 0u;
    test_case(valid,
              "same-cycle keypad input precedes the PPU cause and resulting DMA readiness without consuming a transfer unit");
    free_machine(machine);
    return valid;
}

static void ppu_refusal_arm_channel(gbl_machine *machine,
                                    uint8_t channel_index,
                                    uint8_t start_mode)
{
    gbl_dma_channel_state *channel =
        &machine->dma_channels[channel_index];

    channel->generation = 1u;
    channel->source_address = UINT32_C(0x02000020) +
                              (uint32_t)channel_index * 2u;
    channel->destination_address = UINT32_C(0x03000100) +
                                  (uint32_t)channel_index * 2u;
    channel->current_source = channel->source_address;
    channel->current_destination = channel->destination_address;
    channel->count = 1u;
    channel->control = (uint16_t)(UINT16_C(0x8000) |
                                  ((uint16_t)start_mode << 12u));
    channel->remaining = 1u;
    channel->armed = true;
}

static void ppu_refusal_seed_boundary(gbl_machine *machine,
                                     uint64_t master_cycle,
                                     uint64_t event_cycle,
                                     uint16_t line, bool hblank,
                                     uint64_t generation,
                                     uint64_t next_sequence)
{
    size_t index;

    machine->master_cycles = master_cycle;
    machine->ppu.line_origin = hblank
                                   ? event_cycle - GBL_PPU_HBLANK_CYCLES
                                   : event_cycle;
    machine->ppu.generation = generation;
    machine->ppu.frame_epoch = event_cycle / GBL_PPU_FRAME_CYCLES;
    machine->ppu.line = line;
    machine->ppu.scheduled_line = line;
    machine->ppu.dispcnt = machine->dispcnt;
    machine->ppu.compare = line;
    machine->ppu.dispstat = (uint16_t)(machine->ppu.dispstat &
                                       UINT16_C(0x0038));
    if (line >= GBL_PPU_VISIBLE_LINES) {
        machine->ppu.dispstat |= UINT16_C(0x0001);
    }
    if (line == machine->ppu.compare) {
        machine->ppu.dispstat |= UINT16_C(0x0004);
    }
    machine->ppu.captured_rows = 0u;
    machine->ppu.capture_state = GBL_VIDEO_CAPTURE_INCOMPLETE;
    machine->ppu.capture_reason = GBL_VIDEO_INVALID_REASON_INCOMPLETE;
    machine->ppu.next_boundary_hblank = hblank;
    machine->ppu.frame_started = true;
    memset(machine->ppu.row_coverage, 0, sizeof(machine->ppu.row_coverage));
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        machine->events[index] = (gbl_event){ 0 };
    }
    machine->events[0] = (gbl_event){
        .cycle = event_cycle,
        .sequence = 0u,
        .generation = generation,
        .priority = 9u,
        .kind = (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY,
        .live = true
    };
    machine->event_count = 1u;
    machine->next_event_sequence = next_sequence;
}

static void ppu_refusal_fill_future_events(gbl_machine *machine,
                                           size_t count,
                                           uint64_t cycle)
{
    size_t index;

    for (index = 1u; index < count; ++index) {
        machine->events[index] = (gbl_event){
            .cycle = cycle + 1u,
            .sequence = (uint64_t)index,
            .generation = 0u,
            .priority = 0u,
            .kind = (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW,
            .timer_index = 0u,
            .live = true
        };
    }
    machine->event_count = count;
}

static bool ppu_refusal_preview_is_atomic(gbl_machine *machine,
                                         uint64_t target,
                                         gbl_time_status expected)
{
    gbl_machine_time_preview preview = { 0 };
    gbl_machine_time_preview sentinel = { 0 };
    gbl_machine *before;
    gbl_time_status actual;
    bool valid;

    before = malloc(sizeof(*before));
    if (before == NULL) {
        return false;
    }
    *before = *machine;
    sentinel.master_cycles = UINT64_C(0xAABBCCDD);
    sentinel.ppu.line = UINT16_C(37);
    sentinel.dma_channels[0].trigger_cycle = UINT64_C(0x11223344);
    preview = sentinel;
    actual = gbl_machine_prepare_time(machine, target, &preview);
    valid = actual == expected &&
            gbl_test_time_preview_equal(&preview, &sentinel) &&
            gbl_test_canonical_equal(before, machine);
    free(before);
    return valid;
}

static bool ppu_request_capacity_is_atomic(void)
{
    gbl_machine *machine = new_machine();
    uint8_t channel;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    for (channel = 0u; channel < GBL_DMA_CHANNEL_COUNT; ++channel) {
        ppu_refusal_arm_channel(machine, channel, 2u);
    }
    ppu_refusal_seed_boundary(machine, GBL_PPU_HBLANK_CYCLES,
                              GBL_PPU_HBLANK_CYCLES, 0u, true, 1u, 20u);
    ppu_refusal_fill_future_events(machine, 15u,
                                   GBL_PPU_HBLANK_CYCLES);
    valid = ppu_refusal_preview_is_atomic(
        machine, GBL_PPU_HBLANK_CYCLES, GBL_TIME_STATUS_EVENT_CAPACITY);
    free_machine(machine);
    return valid;
}

static bool ppu_recurrence_capacity_is_atomic(void)
{
    gbl_machine *machine = new_machine();
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    ppu_refusal_arm_channel(machine, 0u, 2u);
    ppu_refusal_seed_boundary(machine, GBL_PPU_HBLANK_CYCLES,
                              GBL_PPU_HBLANK_CYCLES, 0u, true, 1u, 20u);
    ppu_refusal_fill_future_events(machine, GBL_EVENT_CAPACITY,
                                   GBL_PPU_HBLANK_CYCLES);
    valid = ppu_refusal_preview_is_atomic(
        machine, GBL_PPU_HBLANK_CYCLES, GBL_TIME_STATUS_EVENT_CAPACITY);
    free_machine(machine);
    return valid;
}

static bool ppu_request_sequence_is_atomic(void)
{
    gbl_machine *machine = new_machine();
    uint8_t channel;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    for (channel = 0u; channel < GBL_DMA_CHANNEL_COUNT; ++channel) {
        ppu_refusal_arm_channel(machine, channel, 2u);
    }
    ppu_refusal_seed_boundary(machine, GBL_PPU_HBLANK_CYCLES,
                              GBL_PPU_HBLANK_CYCLES, 0u, true, 1u,
                              UINT64_MAX - UINT64_C(2));
    valid = ppu_refusal_preview_is_atomic(
        machine, GBL_PPU_HBLANK_CYCLES,
        GBL_TIME_STATUS_SEQUENCE_EXHAUSTED);
    free_machine(machine);
    return valid;
}

static bool ppu_recurrence_sequence_is_atomic(void)
{
    gbl_machine *machine = new_machine();
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    ppu_refusal_arm_channel(machine, 0u, 2u);
    ppu_refusal_seed_boundary(machine, GBL_PPU_HBLANK_CYCLES,
                              GBL_PPU_HBLANK_CYCLES, 0u, true, 1u,
                              UINT64_MAX - UINT64_C(1));
    valid = ppu_refusal_preview_is_atomic(
        machine, GBL_PPU_HBLANK_CYCLES,
        GBL_TIME_STATUS_SEQUENCE_EXHAUSTED);
    free_machine(machine);
    return valid;
}

static bool ppu_generation_recurrence_is_atomic(void)
{
    gbl_machine *machine = new_machine();
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    ppu_refusal_arm_channel(machine, 0u, 2u);
    ppu_refusal_seed_boundary(machine, GBL_PPU_HBLANK_CYCLES,
                              GBL_PPU_HBLANK_CYCLES, 0u, true,
                              UINT64_MAX - UINT64_C(1), 20u);
    valid = ppu_refusal_preview_is_atomic(
        machine, GBL_PPU_LINE_CYCLES, GBL_TIME_STATUS_GENERATION_EXHAUSTED);
    free_machine(machine);
    return valid;
}

static bool ppu_cycle_overflow_is_atomic(void)
{
    gbl_machine *machine = new_machine();
    const uint64_t cycle = UINT64_MAX - UINT64_C(100);
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    ppu_refusal_arm_channel(machine, 0u, 2u);
    ppu_refusal_seed_boundary(machine, cycle, cycle, 0u, true, 1u, 20u);
    valid = ppu_refusal_preview_is_atomic(
        machine, UINT64_MAX, GBL_TIME_STATUS_CYCLE_OVERFLOW);
    free_machine(machine);
    return valid;
}

static bool ppu_dispatch_budget_is_atomic(void)
{
    gbl_machine *machine = new_machine();
    const uint64_t target =
        (uint64_t)GBL_MACHINE_MAX_EVENT_DISPATCH * GBL_PPU_LINE_CYCLES;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    ppu_refusal_arm_channel(machine, 0u, 2u);
    ppu_refusal_seed_boundary(machine, 0u, 0u, 0u, false, 1u, 1u);
    valid = ppu_refusal_preview_is_atomic(
        machine, target, GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT);
    free_machine(machine);
    return valid;
}

static bool ppu_prepare_is_idempotent_and_instance_local(void)
{
    gbl_machine *first = new_machine();
    gbl_machine *second = new_machine();
    gbl_machine *before_first = NULL;
    gbl_machine *before_second = NULL;
    gbl_machine *after_commit = NULL;
    gbl_machine_time_preview preview_a = { 0 };
    gbl_machine_time_preview preview_b = { 0 };
    bool valid = first != NULL && second != NULL;

    if (!valid) {
        free_machine(first);
        free_machine(second);
        return false;
    }
    before_first = malloc(sizeof(*before_first));
    before_second = malloc(sizeof(*before_second));
    after_commit = malloc(sizeof(*after_commit));
    if (before_first == NULL || before_second == NULL ||
        after_commit == NULL) {
        free(before_first);
        free(before_second);
        free(after_commit);
        free_machine(first);
        free_machine(second);
        return false;
    }
    ppu_refusal_arm_channel(first, 0u, 2u);
    ppu_refusal_arm_channel(second, 0u, 2u);
    first->video_frame[0] = UINT8_C(0x11);
    second->video_frame[0] = UINT8_C(0x22);
    ppu_refusal_seed_boundary(first, GBL_PPU_HBLANK_CYCLES - 1u,
                              GBL_PPU_HBLANK_CYCLES, 0u, true, 1u, 10u);
    ppu_refusal_seed_boundary(second, GBL_PPU_HBLANK_CYCLES - 1u,
                              GBL_PPU_HBLANK_CYCLES, 0u, true, 1u, 10u);
    *before_first = *first;
    *before_second = *second;
    valid = gbl_machine_prepare_time(first, GBL_PPU_HBLANK_CYCLES,
                                     &preview_a) == GBL_TIME_STATUS_OK &&
            gbl_machine_prepare_time(first, GBL_PPU_HBLANK_CYCLES,
                                     &preview_b) == GBL_TIME_STATUS_OK &&
            gbl_test_time_preview_equal(&preview_a, &preview_b) &&
            preview_a.dma_channels[0].pending &&
            preview_a.dma_channels[0].trigger_origin ==
                GBL_DMA_TRIGGER_ORIGIN_HBLANK &&
            preview_a.dma_channels[0].trigger_cycle ==
                GBL_PPU_HBLANK_CYCLES &&
            gbl_test_canonical_equal(before_first, first) &&
            gbl_test_canonical_equal(before_second, second);
    if (valid) {
        gbl_machine_commit_time(first, &preview_a);
        *after_commit = *first;
        valid = first->dma_channels[0].pending &&
                second->dma_channels[0].armed &&
                !second->dma_channels[0].pending &&
                second->video_frame[0] == UINT8_C(0x22) &&
                gbl_machine_advance_to(first, GBL_PPU_HBLANK_CYCLES) ==
                    GBL_TIME_STATUS_OK &&
                gbl_test_canonical_equal(after_commit, first);
    }
    free(before_first);
    free(before_second);
    free(after_commit);
    free_machine(first);
    free_machine(second);
    return valid;
}

static bool ppu_failed_unit_preserves_pixels_and_memory(void)
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
    machine->ewram[0x20u] = UINT8_C(0x5A);
    machine->ewram[0x21u] = UINT8_C(0xA5);
    valid = configure_dma(machine, 0u, UINT32_C(0x02000020),
                          UINT32_C(0x04000000), 1u, UINT16_C(0xA000)) &&
            gbl_machine_advance_to(machine, GBL_PPU_HBLANK_CYCLES) ==
                GBL_TIME_STATUS_OK &&
            machine->dma_channels[0].pending &&
            gbl_machine_dma_step(machine, GBL_DMA_STARTUP_CYCLES, &step) ==
                GBL_DMA_STATUS_OK;
    if (valid) {
        *before = *machine;
        valid = gbl_machine_dma_step(machine, UINT64_C(64), &step) ==
                    GBL_DMA_STATUS_UNIT_REFUSAL &&
                gbl_test_canonical_equal(before, machine);
    }
    free(before);
    free_machine(machine);
    return valid;
}

static bool ppu_provenance_comparator_detects_each_field(void)
{
    gbl_machine *base = new_machine();
    gbl_machine *changed = NULL;
    bool valid = base != NULL;

    if (base == NULL) {
        return false;
    }
    changed = malloc(sizeof(*changed));
    if (changed == NULL) {
        free_machine(base);
        return false;
    }
    base->dma_channels[0].trigger_origin = GBL_DMA_TRIGGER_ORIGIN_HBLANK;
    base->dma_channels[0].trigger_cycle = UINT64_C(1006);
    base->dma_channels[0].trigger_dispute =
        GBL_DMA_DISPUTE_HBLANK_FLAG_EDGE;
    *changed = *base;
    changed->dma_channels[0].trigger_origin =
        GBL_DMA_TRIGGER_ORIGIN_VBLANK;
    valid = !gbl_test_canonical_equal(base, changed);
    *changed = *base;
    ++changed->dma_channels[0].trigger_cycle;
    valid = valid && !gbl_test_canonical_equal(base, changed);
    *changed = *base;
    changed->dma_channels[0].trigger_dispute = GBL_DMA_DISPUTE_NONE;
    valid = valid && !gbl_test_canonical_equal(base, changed);
    free(changed);
    free_machine(base);
    return valid;
}

static bool ppu_same_cycle_timer_input_match_and_ack(void)
{
    gbl_machine *machine = new_machine();
    gbl_time_operation acknowledgement = {
        GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x04000202), 1u, 1u
    };
    gbl_machine_time_preview preview = { 0 };
    gbl_machine *before = NULL;
    const uint64_t cycle =
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    before = malloc(sizeof(*before));
    if (before == NULL) {
        free_machine(machine);
        return false;
    }
    ppu_refusal_arm_channel(machine, 0u, 1u);
    ppu_refusal_seed_boundary(machine, cycle - 1u, cycle,
                              GBL_PPU_VISIBLE_LINES, false, 1u, 4u);
    machine->ppu.line = (uint16_t)(GBL_PPU_VISIBLE_LINES - 1u);
    machine->ppu.dispstat = UINT16_C(0x0038);
    machine->ppu.compare = GBL_PPU_VISIBLE_LINES;
    machine->timers[0] = (gbl_timer_state){
        .reload = UINT16_MAX,
        .counter = UINT16_MAX,
        .divisor = 1u,
        .anchor_cycle = cycle - 1u,
        .generation = 1u,
        .enabled = true,
        .irq_enabled = true
    };
    machine->input_generation = 1u;
    machine->input_event_count = 1u;
    machine->input_events[0] = (gbl_input_event){ cycle, UINT16_C(0x0020) };
    machine->event_count = 3u;
    machine->events[0] = (gbl_event){
        .cycle = cycle,
        .sequence = 0u,
        .generation = 1u,
        .priority = 0u,
        .kind = (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW,
        .timer_index = 0u,
        .live = true
    };
    machine->events[1] = (gbl_event){
        .cycle = cycle,
        .sequence = 1u,
        .generation = 1u,
        .priority = 4u,
        .kind = (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT,
        .input_index = 0u,
        .live = true
    };
    machine->events[2] = (gbl_event){
        .cycle = cycle,
        .sequence = 2u,
        .generation = 1u,
        .priority = 9u,
        .kind = (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY,
        .live = true
    };
    *before = *machine;
    valid = gbl_machine_prepare_operations(
                machine, &acknowledgement, 1u, &preview) ==
                GBL_TIME_STATUS_OK &&
            preview.master_cycles == cycle &&
            (preview.interrupt_flags & UINT16_C(0x000D)) ==
                UINT16_C(0x000C) &&
            preview.pressed_keys == UINT16_C(0x0020) &&
            preview.input_event_cursor == 1u &&
            preview.ppu.line == GBL_PPU_VISIBLE_LINES &&
            (preview.ppu.dispstat & UINT16_C(0x0005)) ==
                UINT16_C(0x0005) &&
            preview.dma_channels[0].pending &&
            preview.dma_channels[0].trigger_origin ==
                GBL_DMA_TRIGGER_ORIGIN_VBLANK &&
            preview.dma_channels[0].trigger_cycle == cycle &&
            gbl_test_canonical_equal(before, machine);
    if (valid) {
        gbl_machine_commit_time(machine, &preview);
        valid = machine->interrupt_flags == UINT16_C(0x000C) &&
                machine->pressed_keys == UINT16_C(0x0020) &&
                machine->dma_channels[0].pending;
    }
    free(before);
    free_machine(machine);
    return valid;
}

static bool ppu_dma_control_writes_follow_boundary(void)
{
    gbl_machine *enable = new_machine();
    gbl_machine *disable = new_machine();
    gbl_time_operation enable_write = {
        GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x040000BA),
        UINT16_C(0xA000), 1u
    };
    gbl_time_operation disable_write = {
        GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x040000BA), 0u, 1u
    };
    gbl_machine_time_preview enable_preview = { 0 };
    gbl_machine_time_preview disable_preview = { 0 };
    const uint64_t cycle = GBL_PPU_HBLANK_CYCLES;
    bool valid = enable != NULL && disable != NULL;

    if (!valid) {
        free_machine(enable);
        free_machine(disable);
        return false;
    }
    ppu_refusal_seed_boundary(enable, cycle - 1u, cycle, 0u, true, 1u, 10u);
    ppu_refusal_seed_boundary(disable, cycle - 1u, cycle, 0u, true, 1u, 10u);
    ppu_refusal_arm_channel(disable, 0u, 2u);
    valid = gbl_machine_prepare_operations(
                enable, &enable_write, 1u, &enable_preview) ==
                GBL_TIME_STATUS_OK &&
            enable_preview.dma_channels[0].armed &&
            !enable_preview.dma_channels[0].pending &&
            enable_preview.dma_channels[0].trigger_origin ==
                GBL_DMA_TRIGGER_ORIGIN_NONE &&
            gbl_machine_prepare_operations(
                disable, &disable_write, 1u, &disable_preview) ==
                GBL_TIME_STATUS_OK &&
            !disable_preview.dma_channels[0].armed &&
            !disable_preview.dma_channels[0].pending &&
            (disable_preview.dma_channels[0].control & UINT16_C(0x8000)) == 0u &&
            disable_preview.dma_channels[0].generation == 2u &&
            disable_preview.dma_channels[0].trigger_origin ==
                GBL_DMA_TRIGGER_ORIGIN_NONE;
    free_machine(enable);
    free_machine(disable);
    return valid;
}

static bool ppu_dma_transfer_completion_collision(void)
{
    gbl_machine *machine = new_machine();
    gbl_dma_step_result step = { 0 };
    uint64_t target;
    bool valid = machine != NULL;

    if (machine == NULL) {
        return false;
    }
    machine->iwram[0x20u] = UINT8_C(0xA6);
    machine->iwram[0x21u] = UINT8_C(0x5C);
    valid = configure_dma(machine, 0u, UINT32_C(0x02000040),
                          UINT32_C(0x03000100), 1u, UINT16_C(0xA000)) &&
            configure_dma(machine, 3u, UINT32_C(0x03000020),
                          UINT32_C(0x03000040), 1u, UINT16_C(0x8000)) &&
            gbl_machine_dma_step(machine, GBL_DMA_STARTUP_CYCLES, &step) ==
                GBL_DMA_STATUS_OK && step.channel == 3u;
    target = machine->master_cycles + 2u;
    if (valid) {
        const uint64_t generation = machine->ppu.generation + 1u;

        ppu_refusal_seed_boundary(machine, machine->master_cycles, target,
                                  0u, true, generation,
                                  machine->next_event_sequence);
        valid = gbl_machine_dma_step(machine, UINT64_C(2), &step) ==
                    GBL_DMA_STATUS_OK && step.channel == 3u &&
                step.unit_completed && step.completion_cycle == target &&
                machine->iwram[0x40u] == UINT8_C(0xA6) &&
                machine->iwram[0x41u] == UINT8_C(0x5C) &&
                machine->dma_channels[3].completed_units == 1u &&
                machine->dma_channels[0].pending &&
                machine->dma_channels[0].trigger_origin ==
                    GBL_DMA_TRIGGER_ORIGIN_HBLANK &&
                machine->dma_channels[0].trigger_cycle == target &&
                machine->bus_owner == GBL_MACHINE_BUS_OWNER_DMA;
    }
    free_machine(machine);
    return valid;
}

static uint32_t dma_ppu_destination(uint8_t region, uint8_t width)
{
    if (region == (uint8_t)GBL_BUS_REGION_PRAM) {
        return UINT32_C(0x05000020);
    }
    if (region == (uint8_t)GBL_BUS_REGION_OAM) {
        return UINT32_C(0x07000020);
    }
    return UINT32_C(0x060001E0) + (width == 4u ? UINT32_C(4) : 0u);
}

static bool dma_ppu_prepare_video_machine(gbl_machine *machine,
                                          uint8_t region, uint8_t width)
{
    const uint32_t value = width == 4u ? UINT32_C(0xA1B2C3D4)
                                        : UINT32_C(0x00002345);
    const uint32_t destination = dma_ppu_destination(region, width);
    const uint16_t control = width == 4u ? UINT16_C(0x8400)
                                          : UINT16_C(0x8000);

    machine->dispcnt = UINT16_C(0x0403);
    machine->ppu.dispcnt = machine->dispcnt;
    machine->iwram[0x20u] = (uint8_t)(value & UINT32_C(0xFF));
    machine->iwram[0x21u] = (uint8_t)((value >> 8u) & UINT32_C(0xFF));
    machine->iwram[0x22u] = (uint8_t)((value >> 16u) & UINT32_C(0xFF));
    machine->iwram[0x23u] = (uint8_t)((value >> 24u) & UINT32_C(0xFF));
    machine->vram[GBL_PPU_ROW_BYTES] = UINT8_C(0x34);
    machine->vram[GBL_PPU_ROW_BYTES + 1u] = UINT8_C(0x12);
    machine->vram[GBL_PPU_ROW_BYTES + 4u] = UINT8_C(0x78);
    machine->vram[GBL_PPU_ROW_BYTES + 5u] = UINT8_C(0x56);
    machine->vram[GBL_PPU_ROW_BYTES + 6u] = UINT8_C(0xBC);
    machine->vram[GBL_PPU_ROW_BYTES + 7u] = UINT8_C(0x9A);
    machine->pram[0x20u] = UINT8_C(0x11);
    machine->pram[0x21u] = UINT8_C(0x22);
    machine->pram[0x22u] = UINT8_C(0x33);
    machine->pram[0x23u] = UINT8_C(0x44);
    machine->oam[0x20u] = UINT8_C(0x55);
    machine->oam[0x21u] = UINT8_C(0x66);
    machine->oam[0x22u] = UINT8_C(0x77);
    machine->oam[0x23u] = UINT8_C(0x88);
    return configure_dma(machine, 0u, UINT32_C(0x03000020), destination,
                         1u, control);
}

static bool dma_ppu_unit_quotes(const gbl_machine *machine, uint8_t width,
                                uint32_t destination,
                                gbl_cycle_cost *source_cost,
                                gbl_cycle_cost *destination_cost)
{
    return gbl_bus_quote(machine, UINT32_C(0x03000020),
                         GBL_BUS_DIRECTION_READ, width,
                         GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_ROLE_DATA,
                         source_cost) &&
           gbl_bus_quote(machine, destination, GBL_BUS_DIRECTION_WRITE,
                         width, GBL_BUS_CLASS_NONSEQUENTIAL,
                         GBL_BUS_ROLE_DATA, destination_cost) &&
           source_cost->cycles != 0u && destination_cost->cycles != 0u;
}

static uint8_t *dma_ppu_access_memory(gbl_machine *machine, uint8_t region,
                                      uint32_t address);
static bool dma_ppu_write_dispcnt(gbl_machine *machine, uint16_t value);
static const char *dma_ppu_region_name(uint8_t region);

static uint32_t dma_ppu_boundary_destination(uint8_t region, uint8_t width)
{
    if (region == (uint8_t)GBL_BUS_REGION_PRAM) {
        return width == 4u ? UINT32_C(0x05000000)
                           : UINT32_C(0x05000002);
    }
    if (region == (uint8_t)GBL_BUS_REGION_VRAM) {
        return UINT32_C(0x060001E0);
    }
    return UINT32_C(0x07000000);
}

static uint32_t dma_ppu_boundary_value(uint8_t region, uint8_t width)
{
    if (region == (uint8_t)GBL_BUS_REGION_PRAM) {
        return width == 4u ? UINT32_C(0x62341234)
                           : UINT32_C(0x00002345);
    }
    if (region == (uint8_t)GBL_BUS_REGION_VRAM) {
        return width == 4u ? UINT32_C(0x21234321)
                           : UINT32_C(0x00002345);
    }
    return UINT32_C(10);
}

static bool dma_ppu_prepare_boundary_scene(gbl_machine *machine,
                                           uint8_t region)
{
    size_t index;

    if (region == (uint8_t)GBL_BUS_REGION_VRAM) {
        machine->ppu.affine[0].pa = 256;
        machine->ppu.affine[0].pb = 0;
        machine->ppu.affine[0].pc = 0;
        machine->ppu.affine[0].pd = 0;
        machine->ppu.affine[0].programmed_reference_x = 0u;
        machine->ppu.affine[0].programmed_reference_y = 0u;
        machine->ppu.affine[0].origin_x = 0u;
        machine->ppu.affine[0].origin_y = 0u;
        machine->ppu.affine[0].origin_valid = true;
        if (!dma_ppu_write_dispcnt(machine, UINT16_C(0x0403))) {
            return false;
        }
        machine->vram[GBL_PPU_ROW_BYTES] = UINT8_C(0x34);
        machine->vram[GBL_PPU_ROW_BYTES + 1u] = UINT8_C(0x12);
        return true;
    }

    if (!dma_ppu_write_dispcnt(machine, UINT16_C(0x0100))) {
        return false;
    }
    machine->ppu.bg_register_bytes[0] = 0u;
    machine->ppu.bg_register_bytes[1] = UINT8_C(0x01);
    machine->vram[UINT32_C(0x0800)] = UINT8_C(1);
    machine->vram[UINT32_C(0x0801)] = 0u;
    machine->pram[0] = UINT8_C(0x10);
    machine->pram[1] = UINT8_C(0x42);
    machine->pram[2] = UINT8_C(0x34);
    machine->pram[3] = UINT8_C(0x12);
    machine->pram[4] = UINT8_C(0x78);
    machine->pram[5] = UINT8_C(0x56);
    for (index = 0u; index < 8u; ++index) {
        machine->vram[UINT32_C(0x0020) + index * 4u] = UINT8_C(0x11);
        machine->vram[UINT32_C(0x0040) + index * 4u] = UINT8_C(0x22);
        machine->vram[UINT32_C(0x2020) + index * 4u] = UINT8_C(0x22);
    }
    if (region == (uint8_t)GBL_BUS_REGION_OAM) {
        for (index = 0u; index < 128u; ++index) {
            machine->oam[index * 8u] = 0u;
            machine->oam[index * 8u + 1u] = UINT8_C(0x02);
        }
        if (!dma_ppu_write_dispcnt(machine, UINT16_C(0x1140))) {
            return false;
        }
        machine->pram[2] = UINT8_C(0x10);
        machine->pram[3] = UINT8_C(0x42);
        machine->pram[UINT32_C(0x0202)] = 0u;
        machine->pram[UINT32_C(0x0203)] = UINT8_C(0x7C);
        machine->oam[0] = UINT8_C(1);
        machine->oam[1] = 0u;
        machine->oam[2] = 0u;
        machine->oam[3] = 0u;
        machine->oam[4] = UINT8_C(1);
        machine->oam[5] = 0u;
        memset(&machine->vram[UINT32_C(0x10020)], UINT8_C(0x11), 32u);
    }
    return true;
}

static bool dma_ppu_boundary_case(uint8_t region, uint8_t width,
                                  int8_t offset, uint16_t *row1_out,
                                  uint16_t *row2_out)
{
    gbl_machine *machine = new_machine();
    gbl_dma_step_result startup = { 0 };
    gbl_dma_step_result step = { 0 };
    gbl_cycle_cost source_cost = { 0 };
    gbl_cycle_cost destination_cost = { 0 };
    const uint32_t destination = dma_ppu_boundary_destination(region, width);
    const uint32_t value = dma_ppu_boundary_value(region, width);
    const uint64_t row_start = GBL_PPU_LINE_CYCLES;
    const uint64_t completion = (uint64_t)((int64_t)row_start + offset);
    const uint16_t width_control = width == 4u ? UINT16_C(0x0400) : 0u;
    const uint8_t expected_bytes[4] = {
        (uint8_t)(value & UINT32_C(0xFF)),
        (uint8_t)((value >> 8u) & UINT32_C(0xFF)),
        (uint8_t)((value >> 16u) & UINT32_C(0xFF)),
        (uint8_t)((value >> 24u) & UINT32_C(0xFF))
    };
    uint64_t unit_cycles = 0u;
    uint16_t row1_pixel = UINT16_MAX;
    uint16_t row2_pixel = UINT16_MAX;
    const uint16_t old_pixel = region == (uint8_t)GBL_BUS_REGION_OAM
                                   ? UINT16_C(0x7C00)
                                   : UINT16_C(0x1234);
    const uint16_t new_pixel = region == (uint8_t)GBL_BUS_REGION_OAM
                                   ? UINT16_C(0x4210)
                               : region == (uint8_t)GBL_BUS_REGION_PRAM
                                   ? (width == 4u ? UINT16_C(0x6234)
                                                  : UINT16_C(0x2345))
                               : width == 4u ? UINT16_C(0x4321)
                                             : UINT16_C(0x2345);
    bool valid = machine != NULL;
    size_t index;

    if (!valid) {
        return false;
    }
    machine->iwram[0x20u] = expected_bytes[0];
    machine->iwram[0x21u] = expected_bytes[1];
    machine->iwram[0x22u] = expected_bytes[2];
    machine->iwram[0x23u] = expected_bytes[3];
    valid = dma_ppu_prepare_boundary_scene(machine, region) &&
            configure_dma(machine, 0u, UINT32_C(0x03000020), destination,
                          1u, (uint16_t)(UINT16_C(0x8000) | width_control)) &&
            dma_ppu_unit_quotes(machine, width, destination, &source_cost,
                                &destination_cost) &&
            source_cost.cycles == UINT64_C(1) &&
            destination_cost.cycles == UINT64_C(1) &&
            gbl_machine_dma_step(machine, GBL_DMA_STARTUP_CYCLES,
                                 &startup) == GBL_DMA_STATUS_OK &&
            !startup.unit_completed &&
            startup.startup_cycles == GBL_DMA_STARTUP_CYCLES;
    unit_cycles = source_cost.cycles + destination_cost.cycles;
    if (valid) {
        ppu_refusal_seed_boundary(
            machine, completion - unit_cycles, row_start, 1u, false,
            machine->ppu.generation + 1u, machine->next_event_sequence);
        /* This synthetic boundary skips line-0 HBlank. For the direct-color
         * affine scene, set row 1's source origin explicitly; PB/PD remain
         * zero so row 2 samples the same source pixel. */
        machine->ppu.affine[0].origin_y = UINT32_C(0x00000100);
        valid = machine->master_cycles == completion - unit_cycles &&
                machine->ppu.line == 1u &&
                machine->ppu.scheduled_line == 1u &&
                !machine->ppu.next_boundary_hblank &&
                machine->ppu.line_origin == row_start &&
                machine->event_count == 1u && machine->events[0].live &&
                machine->events[0].cycle == row_start &&
                gbl_machine_dma_step(machine, unit_cycles, &step) ==
                    GBL_DMA_STATUS_OK && step.unit_completed &&
                step.completion_cycle == completion &&
                machine->master_cycles == completion &&
                step.source_address == UINT32_C(0x03000020) &&
                step.destination_address == destination &&
                step.value == value && step.width == width &&
                step.source_cycles == source_cost.cycles &&
                step.destination_cycles == destination_cost.cycles;
    }
    if (valid && completion < row_start) {
        valid = gbl_machine_advance_to(machine, row_start) ==
                GBL_TIME_STATUS_OK;
    }
    if (valid) {
        row1_pixel = (uint16_t)(
            (uint16_t)machine->video_frame[GBL_PPU_ROW_BYTES] |
            (uint16_t)((uint16_t)machine->video_frame[
                           GBL_PPU_ROW_BYTES + 1u] << 8u));
        valid = gbl_machine_advance_to(
                    machine, UINT64_C(2) * GBL_PPU_LINE_CYCLES) ==
                    GBL_TIME_STATUS_OK &&
                (machine->ppu.row_coverage[0] & UINT8_C(0x06)) ==
                    UINT8_C(0x06);
        if (valid) {
            const size_t row2 = (size_t)GBL_PPU_ROW_BYTES * 2u;

            row2_pixel = (uint16_t)(
                (uint16_t)machine->video_frame[row2] |
                (uint16_t)((uint16_t)machine->video_frame[row2 + 1u] << 8u));
            valid = row1_pixel == (offset < 0 ? new_pixel : old_pixel) &&
                    row2_pixel == new_pixel;
            if (row1_out != NULL) {
                *row1_out = row1_pixel;
            }
            if (row2_out != NULL) {
                *row2_out = row2_pixel;
            }
        }
        if (valid) {
            uint8_t *memory = dma_ppu_access_memory(
                machine, region, destination);

            valid = memory != NULL;
            for (index = 0u; valid && index < width; ++index) {
                valid = memory[index] == expected_bytes[index];
            }
        }
    }
    free_machine(machine);
    return valid;
}

static bool dma_ppu_sample_memory(gbl_machine *machine,
                                  gbl_bus_region region,
                                  uint32_t source_offset, uint8_t width,
                                  const uint8_t *expected)
{
    gbl_machine_time_preview preview = { 0 };
    gbl_ppu_row_capture capture = { 0 };
    uint8_t sample[4] = { 0u, 0u, 0u, 0u };
    size_t index;

    if (gbl_machine_prepare_time(machine, machine->master_cycles,
                                 &preview) != GBL_TIME_STATUS_OK) {
        return false;
    }
    capture.cycle = GBL_PPU_LINE_CYCLES;
    capture.frame_epoch = preview.ppu.frame_epoch;
    capture.line = 1u;
    if (gbl_ppu_sample_read(machine, &preview, &capture, region,
                            source_offset, 2u, width, sample,
                            sizeof(sample)) != GBL_PPU_SAMPLE_STATUS_OK) {
        return false;
    }
    for (index = 0u; index < width; ++index) {
        if (sample[index] != expected[index]) {
            return false;
        }
    }
    return true;
}

static bool dma_ppu_video_lane_case(uint8_t region, uint8_t width)
{
    gbl_machine *machine = new_machine();
    gbl_dma_step_result step = { 0 };
    gbl_cycle_cost source_cost = { 0 };
    gbl_cycle_cost destination_cost = { 0 };
    const uint32_t destination = dma_ppu_destination(region, width);
    const uint32_t value = width == 4u ? UINT32_C(0xA1B2C3D4)
                                        : UINT32_C(0x00002345);
    const uint8_t expected[4] = {
        (uint8_t)(value & UINT32_C(0xFF)),
        (uint8_t)((value >> 8u) & UINT32_C(0xFF)),
        (uint8_t)((value >> 16u) & UINT32_C(0xFF)),
        (uint8_t)((value >> 24u) & UINT32_C(0xFF))
    };
    const uint8_t sample_region = region == (uint8_t)GBL_BUS_REGION_PRAM
                                      ? (uint8_t)GBL_BUS_REGION_PRAM
                                      : (uint8_t)GBL_BUS_REGION_OAM;
    bool valid = machine != NULL;

    if (!valid) {
        return false;
    }
    valid = dma_ppu_prepare_video_machine(machine, region, width) &&
            dma_ppu_unit_quotes(machine, width, destination, &source_cost,
                                &destination_cost) &&
            gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES) ==
                GBL_TIME_STATUS_OK &&
            run_dma_unit(machine, 0u, &step) && step.width == width &&
            step.value == value && step.source_cycles == source_cost.cycles &&
            step.destination_cycles == destination_cost.cycles;
    if (valid) {
        const uint8_t *memory = NULL;
        size_t offset = 0u;
        size_t index;

        if (region == (uint8_t)GBL_BUS_REGION_PRAM) {
            memory = machine->pram;
            offset = destination & (GBL_PRAM_SIZE - 1u);
        } else if (region == (uint8_t)GBL_BUS_REGION_VRAM) {
            memory = machine->vram;
            offset = destination - UINT32_C(0x06000000);
        } else {
            memory = machine->oam;
            offset = destination & (GBL_OAM_SIZE - 1u);
        }
        for (index = 0u; index < width; ++index) {
            valid = valid && memory[offset + index] == expected[index];
        }
        if (region == (uint8_t)GBL_BUS_REGION_PRAM ||
            region == (uint8_t)GBL_BUS_REGION_OAM) {
            valid = valid && dma_ppu_sample_memory(
                machine, (gbl_bus_region)sample_region,
                destination & UINT32_C(0x0000FFFF), width, expected);
        }
        if (region == (uint8_t)GBL_BUS_REGION_VRAM) {
            const uint32_t pixel_offset = width == 4u ? 4u : 0u;
            valid = valid &&
                machine->video_frame[GBL_PPU_ROW_BYTES + pixel_offset] ==
                    (width == 4u ? UINT8_C(0x78) : UINT8_C(0x34)) &&
                machine->video_frame[GBL_PPU_ROW_BYTES + pixel_offset + 1u] ==
                    (width == 4u ? UINT8_C(0x56) : UINT8_C(0x12));
        } else {
            valid = valid &&
                machine->video_frame[GBL_PPU_ROW_BYTES] == UINT8_C(0x34) &&
                machine->video_frame[GBL_PPU_ROW_BYTES + 1u] ==
                    UINT8_C(0x12);
        }
    }
    free_machine(machine);
    return valid;
}

static bool dma_ppu_video_lane_matrix(void)
{
    static const uint8_t regions[] = {
        (uint8_t)GBL_BUS_REGION_PRAM,
        (uint8_t)GBL_BUS_REGION_VRAM,
        (uint8_t)GBL_BUS_REGION_OAM
    };
    size_t region_index;

    for (region_index = 0u;
         region_index < sizeof(regions) / sizeof(regions[0]);
         ++region_index) {
        if (!dma_ppu_video_lane_case(regions[region_index], 2u) ||
            !dma_ppu_video_lane_case(regions[region_index], 4u)) {
            return false;
        }
    }
    return true;
}

typedef enum dma_ppu_access_state {
    DMA_PPU_STATE_DRAWING = 0,
    DMA_PPU_STATE_HBLANK = 1,
    DMA_PPU_STATE_VBLANK = 2,
    DMA_PPU_STATE_FORCED_BLANK = 3
} dma_ppu_access_state;

typedef struct dma_ppu_access_evidence {
    uint8_t state;
    uint8_t region;
    bool video_source;
    uint8_t width;
    uint64_t source_cycles;
    uint64_t destination_cycles;
    uint8_t lane_mask;
} dma_ppu_access_evidence;

typedef enum dma_ppu_access_verdict {
    DMA_PPU_ACCESS_ACCEPT = 0,
    DMA_PPU_ACCESS_INVALID_CELL = 1,
    DMA_PPU_ACCESS_DUPLICATE_CELL = 2,
    DMA_PPU_ACCESS_MISSING_CELL = 3,
    DMA_PPU_ACCESS_COST_MISMATCH = 4,
    DMA_PPU_ACCESS_LANE_MISMATCH = 5
} dma_ppu_access_verdict;

static const char *dma_ppu_state_name(uint8_t state)
{
    static const char *const names[] = {
        "drawing", "hblank", "vblank", "forced-blank"
    };

    return state < sizeof(names) / sizeof(names[0]) ? names[state]
                                                     : "invalid";
}

static const char *dma_ppu_region_name(uint8_t region)
{
    switch ((gbl_bus_region)region) {
    case GBL_BUS_REGION_PRAM: return "pram";
    case GBL_BUS_REGION_VRAM: return "vram";
    case GBL_BUS_REGION_OAM: return "oam";
    default: return "invalid";
    }
}

static uint32_t dma_ppu_access_address(uint8_t region)
{
    switch ((gbl_bus_region)region) {
    case GBL_BUS_REGION_PRAM: return UINT32_C(0x05000020);
    case GBL_BUS_REGION_VRAM: return UINT32_C(0x060001E0);
    case GBL_BUS_REGION_OAM: return UINT32_C(0x07000020);
    default: return 0u;
    }
}

static uint8_t *dma_ppu_access_memory(gbl_machine *machine, uint8_t region,
                                      uint32_t address)
{
    if (region == (uint8_t)GBL_BUS_REGION_PRAM) {
        return &machine->pram[address & (GBL_PRAM_SIZE - 1u)];
    }
    if (region == (uint8_t)GBL_BUS_REGION_VRAM) {
        return &machine->vram[address - UINT32_C(0x06000000)];
    }
    if (region == (uint8_t)GBL_BUS_REGION_OAM) {
        return &machine->oam[address & (GBL_OAM_SIZE - 1u)];
    }
    return NULL;
}

static bool dma_ppu_write_dispcnt(gbl_machine *machine, uint16_t value)
{
    const gbl_bus_result result = gbl_bus_write(
        machine, UINT32_C(0x04000000), 2u,
        GBL_BUS_CLASS_NONSEQUENTIAL, value);

    return result.status == GBL_BUS_STATUS_OK &&
           result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET &&
           machine->dispcnt == value && machine->ppu.dispcnt == value;
}

static bool dma_ppu_matrix_state_matches(const gbl_machine *machine,
                                        uint8_t state)
{
    uint16_t expected_flags = 0u;
    uint16_t expected_line = 0u;
    const bool expected_forced = state == DMA_PPU_STATE_FORCED_BLANK;

    if (state == DMA_PPU_STATE_HBLANK) {
        expected_flags = UINT16_C(0x0002);
    } else if (state == DMA_PPU_STATE_VBLANK) {
        expected_flags = UINT16_C(0x0001);
        expected_line = GBL_PPU_VISIBLE_LINES;
    }
    return machine->ppu.line == expected_line &&
           (machine->ppu.dispstat & UINT16_C(0x0003)) == expected_flags &&
           ((machine->dispcnt & UINT16_C(0x0080)) != 0u) == expected_forced &&
           machine->ppu.dispcnt == machine->dispcnt;
}

static bool dma_ppu_prepare_matrix_state(gbl_machine *machine,
                                         uint8_t state)
{
    uint64_t target = UINT64_C(8);
    uint16_t dispcnt = UINT16_C(0x0403);

    if (state == DMA_PPU_STATE_HBLANK) {
        target = GBL_PPU_HBLANK_CYCLES + UINT64_C(8);
    } else if (state == DMA_PPU_STATE_VBLANK) {
        target = (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES +
                 UINT64_C(8);
    } else if (state == DMA_PPU_STATE_FORCED_BLANK) {
        dispcnt |= UINT16_C(0x0080);
    }
    if (!dma_ppu_write_dispcnt(machine, dispcnt) ||
        gbl_machine_advance_to(machine, target) != GBL_TIME_STATUS_OK) {
        return false;
    }
    return dma_ppu_matrix_state_matches(machine, state);
}

static uint32_t dma_ppu_declared_value(uint8_t region, bool video_source,
                                       uint8_t width)
{
    if (video_source) {
        return width == 4u ? UINT32_C(0x54433221)
                           : UINT32_C(0x00003221);
    }
    (void)region;
    return width == 4u ? UINT32_C(0xA1B2C3D4)
                       : UINT32_C(0x0000C3D4);
}

static bool dma_ppu_access_matrix_cell(uint8_t state, uint8_t region,
                                       bool video_source, uint8_t width,
                                       dma_ppu_access_evidence *evidence,
                                       uint64_t *startup_cycles,
                                       uint64_t *completion_cycle)
{
    const uint32_t video_address = dma_ppu_access_address(region);
    const uint32_t iwram_source = UINT32_C(0x03000020);
    const uint32_t iwram_destination = UINT32_C(0x03000200);
    const uint32_t source_address = video_source ? video_address : iwram_source;
    const uint32_t destination_address =
        video_source ? iwram_destination : video_address;
    const uint32_t value = dma_ppu_declared_value(region, video_source, width);
    const uint8_t expected_bytes[4] = {
        (uint8_t)(value & UINT32_C(0xFF)),
        (uint8_t)((value >> 8u) & UINT32_C(0xFF)),
        (uint8_t)((value >> 16u) & UINT32_C(0xFF)),
        (uint8_t)((value >> 24u) & UINT32_C(0xFF))
    };
    gbl_machine *machine = new_machine();
    gbl_dma_step_result startup = { 0 };
    gbl_dma_step_result step = { 0 };
    gbl_cycle_cost source_quote = { 0 };
    gbl_cycle_cost destination_quote = { 0 };
    uint8_t *video_bytes;
    uint8_t *destination_bytes;
    uint64_t start_cycle;
    uint64_t expected_cycles;
    size_t index;
    uint8_t lane_mask = 0u;
    bool valid = machine != NULL;

    if (!valid) {
        return false;
    }
    video_bytes = dma_ppu_access_memory(machine, region, video_address);
    if (video_bytes == NULL) {
        free_machine(machine);
        return false;
    }
    video_bytes[0] = video_source ? UINT8_C(0x21) : UINT8_C(0x5A);
    video_bytes[1] = video_source ? UINT8_C(0x32) : UINT8_C(0x6B);
    video_bytes[2] = video_source ? UINT8_C(0x43) : UINT8_C(0x7C);
    video_bytes[3] = video_source ? UINT8_C(0x54) : UINT8_C(0x8D);
    machine->iwram[0x20u] = UINT8_C(0xD4);
    machine->iwram[0x21u] = UINT8_C(0xC3);
    machine->iwram[0x22u] = UINT8_C(0xB2);
    machine->iwram[0x23u] = UINT8_C(0xA1);
    if (!dma_ppu_prepare_matrix_state(machine, state)) {
        free_machine(machine);
        return false;
    }
    if (!gbl_bus_quote(machine, source_address, GBL_BUS_DIRECTION_READ,
                       width, GBL_BUS_CLASS_NONSEQUENTIAL,
                       GBL_BUS_ROLE_DATA, &source_quote) ||
        !gbl_bus_quote(machine, destination_address,
                       GBL_BUS_DIRECTION_WRITE, width,
                       GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_ROLE_DATA,
                       &destination_quote)) {
        free_machine(machine);
        return false;
    }
    /* Every video and IWRAM data lane in this selected profile is one cycle;
     * DMA startup is separately charged by GBL_DMA_STARTUP_CYCLES. */
    valid = source_quote.cycles == UINT64_C(1) &&
            destination_quote.cycles == UINT64_C(1) &&
            configure_dma(machine, 0u, source_address, destination_address,
                          1u, (uint16_t)(UINT16_C(0x8000) |
                                         (width == 4u ? UINT16_C(0x0400)
                                                      : 0u)));
    start_cycle = machine->master_cycles;
    if (valid) {
        valid = gbl_machine_dma_step(machine, GBL_DMA_STARTUP_CYCLES,
                                     &startup) == GBL_DMA_STATUS_OK &&
                startup.channel == 0u && !startup.unit_completed &&
                startup.startup_cycles == GBL_DMA_STARTUP_CYCLES &&
                startup.completion_cycle ==
                    start_cycle + GBL_DMA_STARTUP_CYCLES &&
                dma_ppu_matrix_state_matches(machine, state);
    }
    expected_cycles = source_quote.cycles + destination_quote.cycles;
    if (valid) {
        valid = gbl_machine_dma_step(machine, expected_cycles, &step) ==
                    GBL_DMA_STATUS_OK && step.unit_completed &&
                step.channel == 0u && step.owner == GBL_MACHINE_BUS_OWNER_DMA &&
                step.source_address == source_address &&
                step.destination_address == destination_address &&
                step.width == width && step.value == value &&
                step.startup_cycles == 0u &&
                step.source_cycles == source_quote.cycles &&
                step.destination_cycles == destination_quote.cycles &&
                step.source_sequence == GBL_BUS_CLASS_NONSEQUENTIAL &&
                step.destination_sequence == GBL_BUS_CLASS_NONSEQUENTIAL &&
                step.completion_cycle == start_cycle +
                    GBL_DMA_STARTUP_CYCLES + expected_cycles &&
                machine->master_cycles == step.completion_cycle &&
                dma_ppu_matrix_state_matches(machine, state) &&
                !step.used_channel_latch;
    }
    if (valid) {
        destination_bytes = video_source
                               ? &machine->iwram[0x200u]
                               : dma_ppu_access_memory(machine, region,
                                                       destination_address);
        valid = destination_bytes != NULL;
        for (index = 0u; valid && index < width; ++index) {
            if (destination_bytes[index] == expected_bytes[index]) {
                lane_mask = (uint8_t)(lane_mask |
                                      (uint8_t)(1u << index));
            }
        }
        valid = valid && lane_mask == (uint8_t)((1u << width) - 1u);
    }
    if (evidence != NULL && valid) {
        *evidence = (dma_ppu_access_evidence){
            state, region, video_source, width, step.source_cycles,
            step.destination_cycles, lane_mask
        };
    }
    if (startup_cycles != NULL) {
        *startup_cycles = startup.startup_cycles;
    }
    if (completion_cycle != NULL) {
        *completion_cycle = step.completion_cycle;
    }
    free_machine(machine);
    return valid;
}

static dma_ppu_access_verdict dma_ppu_evaluate_access_matrix(
    const dma_ppu_access_evidence *rows, size_t row_count)
{
    bool seen[4][3][2][2] = { false };
    size_t index;

    if (rows == NULL && row_count != 0u) {
        return DMA_PPU_ACCESS_INVALID_CELL;
    }
    for (index = 0u; index < row_count; ++index) {
        const dma_ppu_access_evidence *row = &rows[index];
        const size_t direction = row->video_source ? 0u : 1u;
        const size_t width_index = row->width == 2u ? 0u
                                  : row->width == 4u ? 1u : 2u;

        if (row->state >= 4u ||
            (row->region != (uint8_t)GBL_BUS_REGION_PRAM &&
             row->region != (uint8_t)GBL_BUS_REGION_VRAM &&
             row->region != (uint8_t)GBL_BUS_REGION_OAM) ||
            width_index == 2u) {
            return DMA_PPU_ACCESS_INVALID_CELL;
        }
        if (seen[row->state][row->region == (uint8_t)GBL_BUS_REGION_PRAM
                                 ? 0u
                                 : row->region == (uint8_t)GBL_BUS_REGION_VRAM
                                       ? 1u : 2u]
                 [direction][width_index]) {
            return DMA_PPU_ACCESS_DUPLICATE_CELL;
        }
        seen[row->state][row->region == (uint8_t)GBL_BUS_REGION_PRAM
                             ? 0u
                             : row->region == (uint8_t)GBL_BUS_REGION_VRAM
                                   ? 1u : 2u]
             [direction][width_index] = true;
        if (row->source_cycles != UINT64_C(1) ||
            row->destination_cycles != UINT64_C(1)) {
            return DMA_PPU_ACCESS_COST_MISMATCH;
        }
        if (row->lane_mask != (uint8_t)((1u << row->width) - 1u)) {
            return DMA_PPU_ACCESS_LANE_MISMATCH;
        }
    }
    {
        size_t state;

        for (state = 0u; state < 4u; ++state) {
            size_t region;

            for (region = 0u; region < 3u; ++region) {
                size_t direction;

                for (direction = 0u; direction < 2u; ++direction) {
                    size_t width;

                    for (width = 0u; width < 2u; ++width) {
                        if (!seen[state][region][direction][width]) {
                            return DMA_PPU_ACCESS_MISSING_CELL;
                        }
                    }
                }
            }
        }
    }
    return row_count == 48u ? DMA_PPU_ACCESS_ACCEPT
                            : DMA_PPU_ACCESS_MISSING_CELL;
}

static bool dma_ppu_boundary_pixels_match(uint16_t actual_row1,
                                          uint16_t actual_row2,
                                          uint16_t expected_row1,
                                          uint16_t expected_row2)
{
    return actual_row1 == expected_row1 && actual_row2 == expected_row2;
}

static bool dma_ppu_access_controls(void)
{
    static const uint8_t states[] = {
        DMA_PPU_STATE_DRAWING, DMA_PPU_STATE_HBLANK,
        DMA_PPU_STATE_VBLANK, DMA_PPU_STATE_FORCED_BLANK
    };
    static const uint8_t regions[] = {
        (uint8_t)GBL_BUS_REGION_PRAM,
        (uint8_t)GBL_BUS_REGION_VRAM,
        (uint8_t)GBL_BUS_REGION_OAM
    };
    static const uint8_t widths[] = { 2u, 4u };
    dma_ppu_access_evidence rows[48];
    dma_ppu_access_evidence mutant[48];
    size_t row_count = 0u;
    size_t state;
    uint16_t row1 = 0u;
    uint16_t row2 = 0u;
    bool clean_boundary;

    for (state = 0u; state < sizeof(states) / sizeof(states[0]); ++state) {
        size_t region;

        for (region = 0u; region < sizeof(regions) / sizeof(regions[0]);
             ++region) {
            size_t direction;

            for (direction = 0u; direction < 2u; ++direction) {
                size_t width;

                for (width = 0u; width < sizeof(widths) / sizeof(widths[0]);
                     ++width) {
                    rows[row_count++] = (dma_ppu_access_evidence){
                        states[state], regions[region], direction == 0u,
                        widths[width], UINT64_C(1), UINT64_C(1),
                        (uint8_t)((1u << widths[width]) - 1u)
                    };
                }
            }
        }
    }
    check_count = 0u;
    failure_count = 0u;
    test_case(row_count == 48u &&
                  dma_ppu_evaluate_access_matrix(rows, row_count) ==
                      DMA_PPU_ACCESS_ACCEPT,
              "clean DMA access matrix evidence is accepted");
    test_case(dma_ppu_evaluate_access_matrix(rows, row_count - 1u) ==
                  DMA_PPU_ACCESS_MISSING_CELL,
              "one missing DMA access cell is rejected");
    memcpy(mutant, rows, sizeof(rows));
    mutant[47] = mutant[46];
    test_case(dma_ppu_evaluate_access_matrix(mutant, row_count) ==
                  DMA_PPU_ACCESS_DUPLICATE_CELL,
              "one duplicate DMA access cell is rejected");
    memcpy(mutant, rows, sizeof(rows));
    mutant[0].source_cycles = UINT64_C(2);
    test_case(dma_ppu_evaluate_access_matrix(mutant, row_count) ==
                  DMA_PPU_ACCESS_COST_MISMATCH,
              "one incorrect DMA data-lane cost is rejected");
    memcpy(mutant, rows, sizeof(rows));
    mutant[0].lane_mask &= (uint8_t)~UINT8_C(0x02);
    test_case(dma_ppu_evaluate_access_matrix(mutant, row_count) ==
                  DMA_PPU_ACCESS_LANE_MISMATCH,
              "one missing DMA destination lane is rejected");
    clean_boundary = dma_ppu_boundary_case(
        (uint8_t)GBL_BUS_REGION_VRAM, 2u, 0, &row1, &row2);
    test_case(clean_boundary && dma_ppu_boundary_pixels_match(
                  row1, row2, UINT16_C(0x1234), UINT16_C(0x2345)),
              "clean same-cycle DMA pixels match the literal old and later-row colors");
    test_case(clean_boundary && !dma_ppu_boundary_pixels_match(
                  row1, row2, UINT16_C(0x1235), UINT16_C(0x5678)),
              "one wrong expected DMA pixel is rejected semantically");
    return failure_count == 0u;
}

static int run_ppu_access_controls(void)
{
    printf("TAP version 13\n1..7\n");
    return dma_ppu_access_controls() ? 0 : 1;
}

static bool dma_ppu_access_matrix(void)
{
    static const uint8_t states[] = {
        DMA_PPU_STATE_DRAWING, DMA_PPU_STATE_HBLANK,
        DMA_PPU_STATE_VBLANK, DMA_PPU_STATE_FORCED_BLANK
    };
    static const uint8_t regions[] = {
        (uint8_t)GBL_BUS_REGION_PRAM,
        (uint8_t)GBL_BUS_REGION_VRAM,
        (uint8_t)GBL_BUS_REGION_OAM
    };
    static const uint8_t widths[] = { 2u, 4u };
    dma_ppu_access_evidence rows[48];
    size_t row_count = 0u;
    size_t state_index;

    check_count = 0u;
    failure_count = 0u;
    for (state_index = 0u;
         state_index < sizeof(states) / sizeof(states[0]); ++state_index) {
        size_t region_index;

        for (region_index = 0u;
             region_index < sizeof(regions) / sizeof(regions[0]);
             ++region_index) {
            size_t direction;

            for (direction = 0u; direction < 2u; ++direction) {
                size_t width_index;

                for (width_index = 0u;
                     width_index < sizeof(widths) / sizeof(widths[0]);
                     ++width_index) {
                    dma_ppu_access_evidence row = { 0 };
                    uint64_t startup_cycles = 0u;
                    uint64_t completion_cycle = 0u;
                    const bool video_source = direction == 0u;
                    const bool passed = dma_ppu_access_matrix_cell(
                        states[state_index], regions[region_index],
                        video_source, widths[width_index], &row,
                        &startup_cycles, &completion_cycle);
                    char description[160];

                    (void)snprintf(
                        description, sizeof(description),
                        "DMA %s %s %s width %u keeps one-cycle lanes and reports actual completion",
                        dma_ppu_state_name(states[state_index]),
                        dma_ppu_region_name(regions[region_index]),
                        video_source ? "video-source" : "video-destination",
                        (unsigned int)widths[width_index]);
                    test_case(passed, description);
                    (void)printf(
                        "# ppu.access state=%s region=%s direction=%s width=%u startup=%llu source=%llu destination=%llu completion=%llu lanes=0x%X\n",
                        dma_ppu_state_name(states[state_index]),
                        dma_ppu_region_name(regions[region_index]),
                        video_source ? "read" : "write",
                        (unsigned int)widths[width_index],
                        (unsigned long long)startup_cycles,
                        (unsigned long long)row.source_cycles,
                        (unsigned long long)row.destination_cycles,
                        (unsigned long long)completion_cycle,
                        (unsigned int)row.lane_mask);
                    if (passed && row_count < sizeof(rows) / sizeof(rows[0])) {
                        rows[row_count++] = row;
                    }
                }
            }
        }
    }
    test_case(dma_ppu_evaluate_access_matrix(rows, row_count) ==
                  DMA_PPU_ACCESS_ACCEPT,
              "DMA PPU access matrix contains each of the 48 supported cells exactly once");
    return failure_count == 0u;
}

static int run_ppu_access(void)
{
    printf("TAP version 13\n1..49\n");
    return dma_ppu_access_matrix() ? 0 : 1;
}

static bool dma_ppu_overflow_refusal_is_atomic(void)
{
    gbl_machine *refused = new_machine();
    gbl_machine *clean_twin = new_machine();
    gbl_machine *before = NULL;
    gbl_dma_step_result refused_step = { 0 };
    gbl_dma_step_result clean_step = { 0 };
    gbl_cycle_cost source_cost = { 0 };
    gbl_cycle_cost destination_cost = { 0 };
    gbl_machine_time_preview preview = { 0 };
    gbl_ppu_row_capture capture = { 0 };
    uint8_t sample_sentinel[2] = { UINT8_C(0xA5), UINT8_C(0x5A) };
    const uint64_t row_start = GBL_PPU_LINE_CYCLES;
    uint64_t unit_cycles = 0u;
    bool valid = refused != NULL && clean_twin != NULL;

    if (!valid) {
        free_machine(refused);
        free_machine(clean_twin);
        return false;
    }
    valid = dma_ppu_prepare_video_machine(
                refused, (uint8_t)GBL_BUS_REGION_VRAM, 2u) &&
            dma_ppu_prepare_video_machine(
                clean_twin, (uint8_t)GBL_BUS_REGION_VRAM, 2u) &&
            gbl_machine_dma_step(refused, GBL_DMA_STARTUP_CYCLES,
                                 &refused_step) == GBL_DMA_STATUS_OK &&
            gbl_machine_dma_step(clean_twin, GBL_DMA_STARTUP_CYCLES,
                                 &clean_step) == GBL_DMA_STATUS_OK &&
            dma_ppu_unit_quotes(refused, 2u, UINT32_C(0x060001E0),
                                &source_cost, &destination_cost);
    unit_cycles = source_cost.cycles + destination_cost.cycles;
    if (valid) {
        memset(refused->events, 0, sizeof(refused->events));
        refused->event_count = 0u;
        refused->master_cycles = UINT64_MAX - UINT64_C(1);
        before = malloc(sizeof(*before));
        valid = before != NULL;
    }
    if (valid) {
        *before = *refused;
        valid = gbl_machine_dma_step(refused, UINT64_C(64), &refused_step) ==
                    GBL_DMA_STATUS_TIME_OVERFLOW &&
                refused_step.status == GBL_DMA_STATUS_TIME_OVERFLOW &&
                !refused_step.unit_completed &&
                refused_step.source_address == 0u &&
                refused_step.destination_address == 0u &&
                refused_step.value == 0u && refused_step.width == 0u &&
                refused_step.source_cycles == 0u &&
                refused_step.destination_cycles == 0u &&
                gbl_test_canonical_equal(before, refused) &&
                refused->open_bus == before->open_bus &&
                refused->dma_channels[0].read_latch ==
                    before->dma_channels[0].read_latch &&
                gbl_test_bytes_equal(refused->pram, before->pram,
                                     GBL_PRAM_SIZE) &&
                gbl_test_bytes_equal(refused->vram, before->vram,
                                     GBL_VRAM_SIZE) &&
                gbl_test_bytes_equal(refused->oam, before->oam,
                                     GBL_OAM_SIZE) &&
                gbl_test_bytes_equal(refused->video_frame,
                                     before->video_frame,
                                     GBL_PPU_VIDEO_BYTES) &&
                refused->ppu.capture_state == before->ppu.capture_state &&
                refused->ppu.capture_reason == before->ppu.capture_reason &&
                refused->ppu.captured_rows == before->ppu.captured_rows &&
                memcmp(refused->ppu.row_coverage,
                       before->ppu.row_coverage,
                       sizeof(refused->ppu.row_coverage)) == 0 &&
                gbl_machine_prepare_time(refused, refused->master_cycles,
                                         &preview) == GBL_TIME_STATUS_OK;
    }
    if (valid) {
        capture.frame_epoch = preview.ppu.frame_epoch;
        capture.line = 0u;
        valid = gbl_ppu_sample_read(
                    refused, &preview, &capture, GBL_BUS_REGION_PRAM, 0u,
                    2u, 2u, sample_sentinel, 1u) ==
                    GBL_PPU_SAMPLE_STATUS_OUTPUT_TOO_SMALL &&
                sample_sentinel[0] == UINT8_C(0xA5) &&
                sample_sentinel[1] == UINT8_C(0x5A);
    }
    if (valid) {
        clean_twin->ppu.affine[0].pa = 256;
        clean_twin->ppu.affine[0].pb = 0;
        clean_twin->ppu.affine[0].pc = 0;
        clean_twin->ppu.affine[0].pd = 0;
        ppu_refusal_seed_boundary(
            clean_twin, row_start - unit_cycles, row_start, 1u, false,
            clean_twin->ppu.generation + 1u,
            clean_twin->next_event_sequence);
        /* This synthetic jump starts at line 1 without replaying line-0
         * HBlank, so seed the identity transform's row-1 origin explicitly. */
        clean_twin->ppu.affine[0].origin_y = UINT32_C(0x00000100);
        valid = gbl_machine_dma_step(clean_twin, unit_cycles, &clean_step) ==
                    GBL_DMA_STATUS_OK && clean_step.unit_completed &&
                clean_step.completion_cycle == row_start &&
                clean_step.value == UINT32_C(0x2345) &&
                clean_twin->vram[GBL_PPU_ROW_BYTES] == UINT8_C(0x45) &&
                clean_twin->vram[GBL_PPU_ROW_BYTES + 1u] == UINT8_C(0x23) &&
                clean_twin->video_frame[GBL_PPU_ROW_BYTES] == UINT8_C(0x34) &&
                clean_twin->video_frame[GBL_PPU_ROW_BYTES + 1u] ==
                    UINT8_C(0x12) &&
                gbl_machine_advance_to(
                    clean_twin, UINT64_C(2) * GBL_PPU_LINE_CYCLES) ==
                    GBL_TIME_STATUS_OK &&
                clean_twin->video_frame[2u * GBL_PPU_ROW_BYTES] ==
                    UINT8_C(0x45) &&
                clean_twin->video_frame[2u * GBL_PPU_ROW_BYTES + 1u] ==
                    UINT8_C(0x23);
    }
    free(before);
    free_machine(refused);
    free_machine(clean_twin);
    return valid;
}

static bool run_ppu_refusal(void)
{
    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n1..12\n");
    test_case(ppu_request_capacity_is_atomic(),
              "blank request capacity refusal preserves PPU, DMA, memory, and pixels");
    test_case(ppu_recurrence_capacity_is_atomic(),
              "recurrence capacity refusal rolls back an inserted blank request");
    test_case(ppu_request_sequence_is_atomic(),
              "blank request sequence exhaustion leaves every channel unchanged");
    test_case(ppu_recurrence_sequence_is_atomic(),
              "recurrence sequence exhaustion rolls back readiness and provenance");
    test_case(ppu_generation_recurrence_is_atomic(),
              "PPU generation exhaustion after blank readiness preserves canonical state");
    test_case(ppu_cycle_overflow_is_atomic(),
              "cycle overflow after HBlank request insertion preserves raw state");
    test_case(ppu_dispatch_budget_is_atomic(),
              "bounded dispatch refusal rolls back earlier raster and DMA trigger work");
    test_case(ppu_prepare_is_idempotent_and_instance_local(),
              "repeated prepare is idempotent and PPU captures remain instance-local");
    test_case(ppu_failed_unit_preserves_pixels_and_memory(),
              "failed blank-triggered transfer unit preserves canonical memory and raw video");
    test_case(ppu_provenance_comparator_detects_each_field(),
              "canonical equality detects trigger origin, cycle, and dispute changes");
    test_case(ppu_same_cycle_timer_input_match_and_ack() &&
                  ppu_dma_control_writes_follow_boundary() &&
                  ppu_dma_transfer_completion_collision(),
              "timer, input, PPU match, IF acknowledgement, DMA writes, and transfer completion follow one causal clock");
    test_case(dma_ppu_overflow_refusal_is_atomic(),
              "DMA source candidate followed by destination-time overflow leaves latches, memory, frame, and preview output untouched; exact twin commits");
    return failure_count == 0u ? 0 : 1;
}

static bool run_ppu_triggers(void)
{
    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n1..2\n");
    (void)test_ppu_blank_triggers_and_video_row();
    test_case(dma_ppu_video_lane_matrix(),
              "halfword and word DMA units preserve PRAM, VRAM, OAM lanes, private samples, and sampled pixels");
    return failure_count == 0u ? 0 : 1;
}

static bool run_ppu_order(void)
{
    static const int8_t offsets[] = { -1, 0, 1 };
    static const uint8_t regions[] = {
        (uint8_t)GBL_BUS_REGION_PRAM,
        (uint8_t)GBL_BUS_REGION_VRAM,
        (uint8_t)GBL_BUS_REGION_OAM
    };
    static const uint8_t widths[] = { 2u, 4u };
    static const char *const timing_names[] = { "before", "on", "after" };
    size_t region_index;

    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n1..20\n");
    (void)test_ppu_same_cycle_input_order();
    for (region_index = 0u;
         region_index < sizeof(regions) / sizeof(regions[0]);
         ++region_index) {
        size_t width_index;

        for (width_index = 0u;
             width_index < sizeof(widths) / sizeof(widths[0]);
             ++width_index) {
            size_t timing_index;

            for (timing_index = 0u;
                 timing_index < sizeof(offsets) / sizeof(offsets[0]);
                 ++timing_index) {
                const bool passed = dma_ppu_boundary_case(
                    regions[region_index], widths[width_index],
                    offsets[timing_index], NULL, NULL);
                const uint64_t completion = (uint64_t)(
                    (int64_t)GBL_PPU_LINE_CYCLES +
                    (int64_t)offsets[timing_index]);
                char description[170];

                (void)snprintf(
                    description, sizeof(description),
                    "DMA %s width %u completing %s line start samples literal row 1 and row 2 pixels",
                    dma_ppu_region_name(regions[region_index]),
                    (unsigned int)widths[width_index],
                    timing_names[timing_index]);
                test_case(passed, description);
                (void)printf(
                    "# ppu.order region=%s width=%u timing=%s completion=%llu\n",
                    dma_ppu_region_name(regions[region_index]),
                    (unsigned int)widths[width_index],
                    timing_names[timing_index],
                    (unsigned long long)completion);
            }
        }
    }
    test_case(check_count == 19u,
              "DMA boundary evidence reports all 18 region/width/timing cases plus same-cycle ordering exactly once");
    return failure_count == 0u ? 0 : 1;
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

static bool run_sound_refill(void)
{
    check_count = 0u;
    failure_count = 0u;
    printf("TAP version 13\n1..1\n");
    test_case(test_sound_refill_from_timer_threshold(),
              "Timer 0 post-pop threshold refills FIFO A through four forced DMA words while preserving programmed registers");
    return failure_count == 0u ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "sound_refill") == 0) {
        return run_sound_refill();
    }
    if (argc == 2 && strcmp(argv[1], "ppu.access") == 0) {
        return run_ppu_access();
    }
    if (argc == 2 && strcmp(argv[1], "ppu.access-controls") == 0) {
        return run_ppu_access_controls();
    }
    if (argc == 2 && strcmp(argv[1], "ppu.refusal") == 0) {
        return run_ppu_refusal();
    }
    if (argc == 2 && strcmp(argv[1], "ppu.triggers") == 0) {
        return run_ppu_triggers();
    }
    if (argc == 2 && strcmp(argv[1], "ppu.order") == 0) {
        return run_ppu_order();
    }
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
