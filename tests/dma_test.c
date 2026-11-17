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
                machine->dma_channels[0].source_address &&
            machine->dma_channels[0].current_destination ==
                machine->dma_channels[0].destination_address &&
            write_register(machine, 0u, 0u, 4u, UINT32_C(0x02001003)) &&
            machine->dma_channels[0].current_source !=
                machine->dma_channels[0].source_address &&
            write_register(machine, 0u, 10u, 2u, 0u) &&
            !machine->dma_channels[0].pending &&
            write_register(machine, 0u, 10u, 2u, UINT32_C(0x8000)) &&
            machine->dma_channels[0].current_source ==
                machine->dma_channels[0].source_address &&
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
            write_register(machine, 0u, 4u, 4u, UINT32_C(0x02000100)) &&
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

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "registers") == 0) {
        return run_registers();
    }
    fprintf(stderr, "dma_test: unknown selector or argument count\n");
    return 2;
}
