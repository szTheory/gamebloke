#include "check.h"
#include "gamebloke/gamebloke.h"
#include "../src/audio_internal.h"

#include <stdlib.h>

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

int main(int argc, char **argv)
{
    if (argc != 2 || argv[1] == NULL) {
        return 2;
    }
    if (strcmp(argv[1], "tracer") == 0) {
        return run_tracer();
    }
    return 2;
}
