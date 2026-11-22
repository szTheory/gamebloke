#include "check.h"
#include "gamebloke/gamebloke.h"
#include "../src/audio_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static gbl_machine *new_machine(void)
{
    gbl_machine *machine = calloc(1u, sizeof(*machine));

    if (machine == NULL) {
        return NULL;
    }
    if (gbl_machine_init(machine, NULL) != GBL_BUS_STATUS_OK) {
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

static gbl_bus_result write16(gbl_machine *machine, uint32_t address,
                              uint16_t value)
{
    return gbl_bus_write(machine, address, 2u,
                         GBL_BUS_CLASS_NONSEQUENTIAL, value);
}

static gbl_bus_result read16(gbl_machine *machine, uint32_t address)
{
    return gbl_bus_read(machine, address, 2u,
                        GBL_BUS_CLASS_NONSEQUENTIAL);
}

static int run_tracer(void)
{
    const uint32_t soundbias_address = UINT32_C(0x04000088);
    gbl_machine *machine = new_machine();
    gbl_audio_capture capture;
    uint8_t output[8];
    gbl_bus_result result;
    uint64_t before;
    size_t index;

    CHECK(machine != NULL);
    if (machine == NULL) {
        return CHECK_DONE();
    }
    CHECK(machine->audio.soundbias == UINT16_C(0x0200));
    result = gbl_bus_read(machine, soundbias_address, 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
    CHECK(result.value == UINT32_C(0x0200));

    result = gbl_bus_write(machine, soundbias_address, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFFFF));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
    CHECK(machine->audio.soundbias == UINT16_C(0xC3FF));
    CHECK(result.cycles == 1u);
    result = gbl_bus_read(machine, soundbias_address, 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
    CHECK(result.value == UINT32_C(0xC3FF));
    CHECK(machine->audio.soundbias == UINT16_C(0xC3FF));

    for (index = 0u; index < sizeof(output); ++index) {
        output[index] = UINT8_C(0xA5);
    }
    CHECK(gbl_audio_capture_begin(machine, &capture, output,
                                  sizeof(output)) == GBL_TIME_STATUS_OK);
    CHECK(machine->master_cycles < UINT64_C(511));
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(511), &capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(machine->master_cycles == UINT64_C(511));
    CHECK(capture.bytes_written == 0u);
    for (index = 0u; index < sizeof(output); ++index) {
        CHECK(output[index] == UINT8_C(0xA5));
    }
    before = machine->master_cycles;
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(512), &capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(machine->master_cycles - before == UINT64_C(1));
    CHECK(capture.bytes_written == GBL_AUDIO_FRAME_BYTES);
    CHECK(output[0] == 0u && output[1] == 0u &&
          output[2] == 0u && output[3] == 0u);
    CHECK(output[4] == UINT8_C(0xA5) && output[5] == UINT8_C(0xA5) &&
          output[6] == UINT8_C(0xA5) && output[7] == UINT8_C(0xA5));
    CHECK(machine->audio.cycle_anchor == machine->master_cycles);
    free_machine(machine);
    return CHECK_DONE();
}

static int run_registers(void)
{
    static const struct {
        uint32_t address;
        uint16_t retain_mask;
        uint16_t read_mask;
    } policies[] = {
        { UINT32_C(0x04000060), UINT16_C(0x007F), UINT16_C(0x007F) },
        { UINT32_C(0x04000062), UINT16_C(0xFFFF), UINT16_C(0xFFC0) },
        { UINT32_C(0x04000064), UINT16_C(0x47FF), UINT16_C(0x4000) },
        { UINT32_C(0x04000068), UINT16_C(0xFFFF), UINT16_C(0xFFC0) },
        { UINT32_C(0x0400006C), UINT16_C(0x47FF), UINT16_C(0x4000) },
        { UINT32_C(0x04000070), UINT16_C(0x00E0), UINT16_C(0x00E0) },
        { UINT32_C(0x04000072), UINT16_C(0xE0FF), UINT16_C(0xE000) },
        { UINT32_C(0x04000074), UINT16_C(0x47FF), UINT16_C(0x4000) },
        { UINT32_C(0x04000078), UINT16_C(0xFF3F), UINT16_C(0xFF00) },
        { UINT32_C(0x0400007C), UINT16_C(0x40FF), UINT16_C(0x40FF) },
        { UINT32_C(0x04000080), UINT16_C(0xFF77), UINT16_C(0xFF77) },
        { UINT32_C(0x04000082), UINT16_C(0x770F), UINT16_C(0x770F) }
    };
    gbl_machine *machine = new_machine();
    gbl_machine *snapshot = NULL;
    gbl_bus_result result;
    size_t index;

    CHECK(machine != NULL);
    if (machine == NULL) {
        return CHECK_DONE();
    }
    result = write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
    for (index = 0u; index < sizeof(policies) / sizeof(policies[0]); ++index) {
        const uint16_t expected =
            policies[index].address == UINT32_C(0x04000064) ||
                    policies[index].address == UINT32_C(0x0400006C) ||
                    policies[index].address == UINT32_C(0x04000074)
                ? UINT16_C(0x4000)
                : policies[index].read_mask;

        result = write16(machine, policies[index].address, UINT16_MAX);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
        result = read16(machine, policies[index].address);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
        CHECK(result.value == expected);
    }
    CHECK(read16(machine, UINT32_C(0x04000084)).value == UINT32_C(0x008F));
    result = gbl_bus_read(machine, UINT32_C(0x04000066), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);
    result = write16(machine, UINT32_C(0x04000066), UINT16_MAX);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);

    /* A single word write updates the envelope before applying its adjacent
     * trigger action. The trigger is self-clearing and the status is read-only. */
    result = gbl_bus_write(machine, UINT32_C(0x04000062), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x8000F800));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(read16(machine, UINT32_C(0x04000064)).value == UINT32_C(0x4000));
    result = write16(machine, UINT32_C(0x04000064), UINT16_C(0x4000));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(read16(machine, UINT32_C(0x04000064)).value == UINT32_C(0x4000));
    result = gbl_bus_write(machine, UINT32_C(0x04000060), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0xFFFF0005));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(read16(machine, UINT32_C(0x04000060)).value == UINT32_C(0x0005));
    CHECK(read16(machine, UINT32_C(0x04000062)).value == UINT32_C(0xFFC0));

    result = write16(machine, UINT32_C(0x04000088), UINT16_C(0xC3FF));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    result = write16(machine, UINT32_C(0x04000084), 0u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(read16(machine, UINT32_C(0x04000084)).value == 0u);
    for (index = 0u; index < sizeof(policies) / sizeof(policies[0]); ++index) {
        result = read16(machine, policies[index].address);
        CHECK(result.value == 0u);
    }
    CHECK(read16(machine, UINT32_C(0x04000088)).value == UINT32_C(0xC3FF));
    result = write16(machine, UINT32_C(0x04000062), UINT16_MAX);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(read16(machine, UINT32_C(0x04000062)).value == 0u);

    /* Overflow refusal occurs before either the register or the clock moves. */
    snapshot = malloc(sizeof(*snapshot));
    CHECK(snapshot != NULL);
    if (snapshot != NULL) {
        machine->master_cycles = UINT64_MAX;
        machine->audio.cycle_anchor = UINT64_MAX;
        memcpy(snapshot, machine, sizeof(*snapshot));
        result = write16(machine, UINT32_C(0x04000060), UINT16_C(0x005A));
        CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
        CHECK(memcmp(snapshot, machine, sizeof(*snapshot)) == 0);
        free(snapshot);
    }

    gbl_machine_reset(machine);
    CHECK(machine->audio.soundbias == UINT16_C(0x0200));
    CHECK(read16(machine, UINT32_C(0x04000084)).value == 0u);
    free_machine(machine);
    return CHECK_DONE();
}

int main(int argc, char **argv)
{
    if (argc != 2 || argv[1] == NULL) {
        return 2;
    }
    if (strcmp(argv[1], "tracer") == 0) {
        return run_tracer();
    }
    if (strcmp(argv[1], "registers") == 0) {
        return run_registers();
    }
    return 2;
}
