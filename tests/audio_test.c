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

static unsigned tap_test_number;
static unsigned tap_failure_count;

static void tap_check(const char *name, bool passed)
{
    ++tap_test_number;
    printf("%s %u - %s\n", passed ? "ok" : "not ok", tap_test_number,
           name);
    if (!passed) {
        ++tap_failure_count;
    }
}

static bool snapshot_machine(gbl_machine *machine, gbl_machine **snapshot);
static bool unchanged_machine(const gbl_machine *machine,
                              const gbl_machine *snapshot);

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
    static const uint32_t length_registers[] = {
        UINT32_C(0x04000064), UINT32_C(0x0400006C),
        UINT32_C(0x04000074), UINT32_C(0x0400007C)
    };
    gbl_machine *machine = new_machine();
    gbl_machine *snapshot = NULL;
    gbl_machine_time_preview forged_preview;
    gbl_bus_result result;
    size_t index;

    CHECK(machine != NULL);
    if (machine == NULL) {
        return CHECK_DONE();
    }
    result = write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);

    /* Wave enable permits a trigger but does not itself start channel 3. */
    result = write16(machine, UINT32_C(0x04000070), UINT16_C(0x0080));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK((read16(machine, UINT32_C(0x04000084)).value & UINT32_C(0x0004)) == 0u);
    result = write16(machine, UINT32_C(0x04000074), UINT16_C(0x8000));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK((read16(machine, UINT32_C(0x04000084)).value & UINT32_C(0x0004)) != 0u);
    result = write16(machine, UINT32_C(0x04000070), 0u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK((read16(machine, UINT32_C(0x04000084)).value & UINT32_C(0x0004)) == 0u);

    /* Bit 14 is each channel's retained length-enable flag. It must not
     * mirror the separate active-channel bits in SOUNDCNT_X. */
    for (index = 0u;
         index < sizeof(length_registers) / sizeof(length_registers[0]);
         ++index) {
        result = write16(machine, length_registers[index], UINT16_C(0x4000));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(read16(machine, length_registers[index]).value ==
              UINT16_C(0x4000));
        CHECK((read16(machine, UINT32_C(0x04000084)).value &
               UINT32_C(0x000F)) == 0u);
    }

    for (index = 0u; index < sizeof(policies) / sizeof(policies[0]); ++index) {
        result = write16(machine, policies[index].address, UINT16_MAX);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
        result = read16(machine, policies[index].address);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
        CHECK(result.value == policies[index].read_mask);
    }
    CHECK(read16(machine, UINT32_C(0x04000084)).value == UINT32_C(0x008F));
    result = gbl_bus_read(machine, UINT32_C(0x04000066), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);
    result = write16(machine, UINT32_C(0x04000066), UINT16_MAX);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);

    /* Clearing the DAC clears only channel status; the retained length flag
     * remains independent. Re-enabling and triggering preserves that flag,
     * while SOUNDCNT_X reports the active channel separately. */
    result = write16(machine, UINT32_C(0x04000062), 0u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(read16(machine, UINT32_C(0x04000064)).value == UINT16_C(0x4000));
    CHECK((read16(machine, UINT32_C(0x04000084)).value & UINT32_C(0x0001)) ==
          0u);
    result = write16(machine, UINT32_C(0x04000064), UINT16_C(0x4000));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(read16(machine, UINT32_C(0x04000064)).value == UINT16_C(0x4000));
    CHECK((read16(machine, UINT32_C(0x04000084)).value & UINT32_C(0x0001)) ==
          0u);
    result = gbl_bus_write(machine, UINT32_C(0x04000062), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0xC000F800));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(read16(machine, UINT32_C(0x04000064)).value == UINT32_C(0x4000));
    CHECK((read16(machine, UINT32_C(0x04000084)).value & UINT32_C(0x0001)) !=
          0u);
    result = write16(machine, UINT32_C(0x04000062), 0u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(read16(machine, UINT32_C(0x04000064)).value == UINT32_C(0x4000));
    CHECK((read16(machine, UINT32_C(0x04000084)).value & UINT32_C(0x0001)) ==
          0u);
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
    CHECK(write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status ==
          GBL_BUS_STATUS_OK);
    result = gbl_bus_write(machine, UINT32_C(0x04000066), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0xF800FFFF));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);
    CHECK(read16(machine, UINT32_C(0x04000068)).value == UINT16_C(0xF800));

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
    CHECK(snapshot_machine(machine, &snapshot));
    CHECK(gbl_machine_prepare_time(machine, machine->master_cycles,
                                   &forged_preview) == GBL_TIME_STATUS_OK);
    forged_preview.audio.registers[18] = UINT16_C(0x0080);
    forged_preview.audio.active_channels = UINT8_C(0x04);
    forged_preview.audio_prepared = forged_preview.audio;
    CHECK(!gbl_machine_commit_time_checked(machine, &forged_preview));
    CHECK(unchanged_machine(machine, snapshot));
    free(snapshot);
    free_machine(machine);
    return CHECK_DONE();
}

static int run_psg_square(void)
{
    const uint32_t soundcnt_x = UINT32_C(0x04000084);
    const uint32_t sound2cnt_l = UINT32_C(0x04000068);
    const uint32_t sound2cnt_h = UINT32_C(0x0400006C);
    gbl_machine *machine = new_machine();
    gbl_audio_capture capture;
    uint8_t output[GBL_AUDIO_FRAME_BYTES] = { UINT8_C(0xA5), UINT8_C(0xA5),
                                               UINT8_C(0xA5), UINT8_C(0xA5) };
    uint64_t trigger_cycle = 0u;
    gbl_bus_result write_result;

    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");
    tap_check("machine is initialized", machine != NULL);
    if (machine == NULL) {
        puts("1..1");
        return 1;
    }

    write_result = write16(machine, soundcnt_x, UINT16_C(0x0080));
    tap_check("audio master enable is accepted",
              write_result.status == GBL_BUS_STATUS_OK);
    write_result = write16(machine, sound2cnt_l, UINT16_C(0xF000));
    tap_check("CH2 DAC and one-eighth duty are configured",
              write_result.status == GBL_BUS_STATUS_OK);
    write_result = write16(machine, sound2cnt_h, UINT16_C(0x87FF));
    tap_check("CH2 trigger is accepted at frequency 2047",
              write_result.status == GBL_BUS_STATUS_OK);
    trigger_cycle = machine->master_cycles;
    tap_check("CH2 source is high at trigger phase zero",
              gbl_audio_psg_sample(&machine->audio, 1u) == UINT8_C(15));
    tap_check("CH2 alone is active and the trigger bit self-cleared",
              (read16(machine, soundcnt_x).value & UINT32_C(0x000F)) ==
                      UINT32_C(0x0002) &&
                  (read16(machine, sound2cnt_h).value & UINT32_C(0x8000)) ==
                      0u);

    tap_check("advance reaches duty edge minus one",
              gbl_machine_advance_to(machine, trigger_cycle + 15u) ==
                  GBL_TIME_STATUS_OK);
    tap_check("one-eighth duty remains high at offset 15",
              gbl_audio_psg_sample(&machine->audio, 1u) == UINT8_C(15));
    tap_check("advance reaches the duty edge",
              gbl_machine_advance_to(machine, trigger_cycle + 16u) ==
                  GBL_TIME_STATUS_OK);
    tap_check("one-eighth duty is low at offset 16",
              gbl_audio_psg_sample(&machine->audio, 1u) == 0u);
    tap_check("advance reaches duty edge plus one",
              gbl_machine_advance_to(machine, trigger_cycle + 17u) ==
                  GBL_TIME_STATUS_OK);
    tap_check("one-eighth duty remains low at offset 17",
              gbl_audio_psg_sample(&machine->audio, 1u) == 0u);
    tap_check("advance reaches the first square wrap",
              gbl_machine_advance_to(machine, trigger_cycle + 128u) ==
                  GBL_TIME_STATUS_OK);
    tap_check("CH2 returns high at offset 128",
              gbl_audio_psg_sample(&machine->audio, 1u) == UINT8_C(15));

    memset(&capture, 0, sizeof(capture));
    tap_check("fixed-grid capture begins after direct source checks",
              gbl_audio_capture_begin(machine, &capture, output,
                                     sizeof(output)) == GBL_TIME_STATUS_OK);
    tap_check("capture advances to the next fixed-grid frame",
              gbl_machine_advance_to_audio(
                  machine,
                  ((machine->master_cycles / GBL_AUDIO_SAMPLE_CYCLES) + 1u) *
                      GBL_AUDIO_SAMPLE_CYCLES,
                  &capture) == GBL_TIME_STATUS_OK);
    tap_check("PCM frame remains separate signed-zero projection",
              capture.bytes_written == GBL_AUDIO_FRAME_BYTES &&
                  output[0] == 0u && output[1] == 0u && output[2] == 0u &&
                  output[3] == 0u);

    free_machine(machine);
    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

typedef struct sample_observation {
    size_t calls;
    uint64_t cycle;
    uint16_t flags;
    uint16_t route;
} sample_observation;

static void boundary_sample(const gbl_machine_time_preview *candidate,
                            uint64_t cycle, void *context, int16_t *left,
                            int16_t *right)
{
    sample_observation *observation = context;
    const bool timer_fired =
        (candidate->interrupt_flags & UINT16_C(0x0008)) != 0u;
    const uint16_t route =
        gbl_audio_read_register(&candidate->audio, UINT32_C(0x04000080));

    observation->calls++;
    observation->cycle = cycle;
    observation->flags = candidate->interrupt_flags;
    observation->route = route;
    if (cycle == UINT64_C(512) && timer_fired &&
        route == UINT16_C(0x0100)) {
        *left = INT16_C(0x1234);
        *right = -INT16_C(0x2345);
    } else if (cycle == UINT64_C(512) && timer_fired &&
               route == UINT16_C(0x1000)) {
        *left = INT16_C(0x4567);
        *right = -INT16_C(0x3456);
    } else {
        *left = INT16_C(0x1111);
        *right = -INT16_C(0x1111);
    }
}

static int run_cadence(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *control = new_machine();
    uint8_t output[16];
    uint8_t replay[8];
    gbl_audio_capture capture;
    gbl_audio_capture replay_capture;
    uint64_t target;
    size_t index;

    CHECK(machine != NULL && control != NULL);
    if (machine == NULL || control == NULL) {
        free_machine(machine);
        free_machine(control);
        return CHECK_DONE();
    }
    memset(output, 0xA5, sizeof(output));
    CHECK(gbl_audio_capture_begin(machine, &capture, output,
                                  sizeof(output)) == GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(511), &capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(capture.bytes_written == 0u);
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(512), &capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(capture.bytes_written == 4u);
    CHECK(output[0] == 0u && output[1] == 0u &&
          output[2] == 0u && output[3] == 0u);
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(1024), &capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(capture.bytes_written == 8u);
    for (index = 0u; index < 8u; ++index) {
        CHECK(output[index] == 0u);
    }
    for (index = 8u; index < sizeof(output); ++index) {
        CHECK(output[index] == UINT8_C(0xA5));
    }

    CHECK(gbl_machine_advance_to(control, UINT64_C(1024)) ==
          GBL_TIME_STATUS_OK);
    CHECK(control->event_count == machine->event_count);
    CHECK(control->next_event_sequence == machine->next_event_sequence);
    CHECK(control->master_cycles == machine->master_cycles);

    /* Capture the same boundary again after reset; output is deterministic. */
    gbl_machine_reset(machine);
    memset(replay, 0xCC, sizeof(replay));
    CHECK(gbl_audio_capture_begin(machine, &replay_capture, replay,
                                  sizeof(replay)) == GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(1024),
                                       &replay_capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(replay_capture.bytes_written == 8u);
    CHECK(memcmp(output, replay, sizeof(replay)) == 0);

    /* (512,1024] contains one frame; (500,1024] contains two. */
    gbl_machine_reset(machine);
    memset(replay, 0xCC, sizeof(replay));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(512)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_audio_capture_begin(machine, &replay_capture, replay,
                                  sizeof(replay)) == GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(1024),
                                       &replay_capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(replay_capture.bytes_written == 4u);
    gbl_machine_reset(machine);
    CHECK(gbl_machine_advance_to(machine, UINT64_C(500)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_audio_capture_begin(machine, &replay_capture, replay,
                                  sizeof(replay)) == GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(1024),
                                       &replay_capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(replay_capture.bytes_written == 8u);

    /* Each append is bounded while the caller-owned session totals one second. */
    gbl_machine_reset(machine);
    {
        uint8_t *full_output = malloc(GBL_AUDIO_MAX_CAPTURE_BYTES);
        uint8_t *full_before = malloc(GBL_AUDIO_MAX_CAPTURE_BYTES);
        gbl_machine *full_control = new_machine();
        gbl_machine *full_snapshot = NULL;
        gbl_audio_capture capture_before;

        CHECK(full_output != NULL && full_before != NULL &&
              full_control != NULL);
        if (full_output != NULL && full_before != NULL &&
            full_control != NULL) {
            memset(full_output, 0x5A, GBL_AUDIO_MAX_CAPTURE_BYTES);
            CHECK(gbl_audio_capture_begin(
                      machine, &capture, full_output,
                      GBL_AUDIO_MAX_CAPTURE_BYTES) == GBL_TIME_STATUS_OK);
            for (index = 1u; index <= 16u; ++index) {
                target = (uint64_t)index * UINT64_C(1048576);
                CHECK(gbl_machine_advance_to_audio(machine, target,
                                                   &capture) ==
                      GBL_TIME_STATUS_OK);
                CHECK(machine->master_cycles == target);
                CHECK(capture.initial_cycle == 0u);
                CHECK(capture.last_accepted_cycle == target);
                CHECK(capture.bytes_written == index * 8192u);
                CHECK(gbl_machine_advance_to(full_control, target) ==
                      GBL_TIME_STATUS_OK);
                CHECK(machine->event_count == full_control->event_count);
                CHECK(machine->next_event_sequence ==
                      full_control->next_event_sequence);
            }
            CHECK(capture.last_accepted_cycle ==
                  GBL_AUDIO_MAX_CAPTURE_CYCLES);
            CHECK(capture.bytes_written == GBL_AUDIO_MAX_CAPTURE_BYTES);
            CHECK(capture.initial_cycle == 0u);
            CHECK(snapshot_machine(machine, &full_snapshot));
            memcpy(full_before, full_output, GBL_AUDIO_MAX_CAPTURE_BYTES);
            capture_before = capture;
            CHECK(gbl_machine_advance_to_audio(
                      machine, GBL_AUDIO_MAX_CAPTURE_CYCLES + UINT64_C(1),
                      &capture) == GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT);
            CHECK(unchanged_machine(machine, full_snapshot));
            CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
            CHECK(memcmp(full_output, full_before,
                         GBL_AUDIO_MAX_CAPTURE_BYTES) == 0);
            CHECK(gbl_machine_advance_to_audio(
                      machine, GBL_AUDIO_MAX_CAPTURE_CYCLES +
                                   GBL_AUDIO_SAMPLE_CYCLES,
                      &capture) == GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT);
            CHECK(unchanged_machine(machine, full_snapshot));
            CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
            CHECK(memcmp(full_output, full_before,
                         GBL_AUDIO_MAX_CAPTURE_BYTES) == 0);
        }
        free(full_output);
        free(full_before);
        free(full_snapshot);
        free_machine(full_control);
    }
    free_machine(machine);
    free_machine(control);
    return CHECK_DONE();
}

static int run_boundaries(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;
    gbl_cycle_cost write_cost = { 0 };
    gbl_time_operation operations[2];
    sample_observation observation = { 0 };
    uint8_t staged[4] = { UINT8_C(0xA5), UINT8_C(0xA5),
                          UINT8_C(0xA5), UINT8_C(0xA5) };
    size_t staged_bytes = 0u;
    gbl_bus_result result;

    CHECK(machine != NULL);
    if (machine == NULL) {
        return CHECK_DONE();
    }
    CHECK(write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status ==
          GBL_BUS_STATUS_OK);
    CHECK(write16(machine, UINT32_C(0x04000080), UINT16_C(0x0100)).status ==
          GBL_BUS_STATUS_OK);
    CHECK(write16(machine, UINT32_C(0x04000100), UINT16_C(0xFE04)).status ==
          GBL_BUS_STATUS_OK);
    CHECK(write16(machine, UINT32_C(0x04000102), UINT16_C(0x00C0)).status ==
          GBL_BUS_STATUS_OK);
    CHECK(machine->master_cycles == UINT64_C(4));
    CHECK((machine->interrupt_flags & UINT16_C(0x0008)) == 0u);
    CHECK(machine->timers[0].enabled && machine->timers[0].irq_enabled);
    CHECK(machine->event_count != 0u);

    CHECK(gbl_bus_quote(machine, UINT32_C(0x04000080),
                        GBL_BUS_DIRECTION_WRITE, 2u,
                        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_ROLE_DATA,
                        &write_cost));
    CHECK(write_cost.cycles == 1u);
    operations[0] = (gbl_time_operation){
        .kind = GBL_TIME_OPERATION_INTERNAL,
        .cycles = UINT64_C(507)
    };
    operations[1] = (gbl_time_operation){
        .kind = GBL_TIME_OPERATION_BUS_WRITE,
        .width = 2u,
        .address = UINT32_C(0x04000080),
        .value = UINT32_C(0x1000),
        .cycles = write_cost.cycles
    };
    CHECK(gbl_machine_prepare_operations_audio_with_provider(
              machine, operations, 2u, &preview, staged, sizeof(staged),
              &staged_bytes, boundary_sample, &observation) ==
          GBL_TIME_STATUS_OK);
    CHECK(staged_bytes == sizeof(staged));
    CHECK(observation.calls == 1u);
    CHECK(observation.cycle == UINT64_C(512));
    CHECK((observation.flags & UINT16_C(0x0008)) != 0u);
    CHECK(observation.route == UINT16_C(0x0100));
    CHECK(staged[0] == UINT8_C(0x34) && staged[1] == UINT8_C(0x12) &&
          staged[2] == UINT8_C(0xBB) && staged[3] == UINT8_C(0xDC));
    CHECK(machine->master_cycles == UINT64_C(4));
    CHECK((machine->interrupt_flags & UINT16_C(0x0008)) == 0u);
    CHECK(gbl_machine_commit_time_checked(machine, &preview));
    CHECK(machine->master_cycles == UINT64_C(512));
    CHECK((machine->interrupt_flags & UINT16_C(0x0008)) != 0u);
    CHECK(read16(machine, UINT32_C(0x04000080)).value == UINT16_C(0x1000));
    result = read16(machine, UINT32_C(0x04000080));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.value == UINT16_C(0x1000));
    free_machine(machine);
    return CHECK_DONE();
}

static int run_cpu(void)
{
    gbl_machine *machine = new_machine();
    uint8_t image[UINT32_C(0xC0)] = { 0u };
    gbl_run_result run_result = { 0 };
    const uint32_t store_halfword = UINT32_C(0xE1C100B0);
    const uint32_t load_halfword = UINT32_C(0xE1D120B0);
    gbl_run_options options = {
        .cycle_budget = UINT64_C(64),
        .unit_limit = 1u,
        .category_mask = 0u,
        .trace_buffer = NULL
    };

    CHECK(machine != NULL);
    if (machine == NULL) {
        return CHECK_DONE();
    }
    image[0] = (uint8_t)store_halfword;
    image[1] = (uint8_t)(store_halfword >> 8u);
    image[2] = (uint8_t)(store_halfword >> 16u);
    image[3] = (uint8_t)(store_halfword >> 24u);
    image[4] = (uint8_t)load_halfword;
    image[5] = (uint8_t)(load_halfword >> 8u);
    image[6] = (uint8_t)(load_halfword >> 16u);
    image[7] = (uint8_t)(load_halfword >> 24u);
    /* The instruction image is invocation-local synthetic test data. The
     * machine was initialized without an owning ROM allocator, so disposal
     * will not attempt to free this stack-backed fixture. */
    machine->rom = image;
    machine->rom_length = sizeof(image);
    machine->cpu.execution_address = UINT32_C(0x08000000);
    machine->cpu.r0_7[0] = UINT32_C(0x0080);
    machine->cpu.r0_7[1] = UINT32_C(0x04000084);
    machine->cpu.cpsr = UINT32_C(0x0000001F);
    machine->cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000000), store_halfword, 4u, true
    };
    machine->cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000004), UINT32_C(0xE1A00000), 4u, true
    };
    machine->cpu.next_fetch_sequential = true;
    CHECK(gbl_cpu_run(machine, &options, &run_result) == GBL_RUN_STATUS_OK);
    CHECK(run_result.termination == GBL_RUN_PROGRESS);
    CHECK(run_result.instructions == 1u);
    CHECK((machine->audio.registers[18] & UINT16_C(0x0080)) != 0u);
    machine->cpu.execution_address = UINT32_C(0x08000004);
    machine->cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000004), load_halfword, 4u, true
    };
    machine->cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000008), UINT32_C(0xE1A00000), 4u, true
    };
    machine->cpu.next_fetch_sequential = true;
    options.unit_limit = 1u;
    run_result = (gbl_run_result){ 0 };
    CHECK(gbl_cpu_run(machine, &options, &run_result) == GBL_RUN_STATUS_OK);
    CHECK(run_result.termination == GBL_RUN_PROGRESS);
    CHECK(run_result.instructions == 1u);
    CHECK(machine->cpu.r0_7[2] == UINT32_C(0x0080));
    CHECK(read16(machine, UINT32_C(0x04000084)).value == UINT16_C(0x0080));
    free_machine(machine);
    return CHECK_DONE();
}

static bool snapshot_machine(gbl_machine *machine, gbl_machine **snapshot)
{
    *snapshot = malloc(sizeof(**snapshot));
    if (*snapshot == NULL) {
        return false;
    }
    memcpy(*snapshot, machine, sizeof(**snapshot));
    return true;
}

static bool unchanged_machine(const gbl_machine *machine,
                              const gbl_machine *snapshot)
{
    return memcmp(machine, snapshot, sizeof(*machine)) == 0;
}

static int run_refusal(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *other = new_machine();
    gbl_machine *snapshot = NULL;
    uint8_t output[16];
    uint8_t output_before[sizeof(output)];
    uint8_t dense_output[GBL_AUDIO_MAX_CAPTURE_BYTES];
    gbl_audio_capture capture;
    gbl_audio_capture capture_before;
    gbl_time_operation operations[3];
    gbl_machine_time_preview preview;
    gbl_machine_time_preview preview_before;
    uint8_t staging[4] = { UINT8_C(0xCC), UINT8_C(0xCC),
                           UINT8_C(0xCC), UINT8_C(0xCC) };
    size_t staged_bytes;
    size_t index;

    CHECK(machine != NULL && other != NULL);
    if (machine == NULL || other == NULL) {
        free_machine(machine);
        free_machine(other);
        return CHECK_DONE();
    }
    memset(output, 0xA5, sizeof(output));
    memcpy(output_before, output, sizeof(output));
    memset(&capture, 0x3C, sizeof(capture));
    capture_before = capture;
    CHECK(gbl_audio_capture_begin(machine, NULL, output,
                                  sizeof(output)) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(output, output_before, sizeof(output)) == 0);
    CHECK(gbl_audio_capture_begin(NULL, &capture, output,
                                  sizeof(output)) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(output, output_before, sizeof(output)) == 0);
    CHECK(gbl_audio_capture_begin(machine, &capture, NULL, 1u) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(output, output_before, sizeof(output)) == 0);
    CHECK(gbl_audio_capture_begin(
              machine, &capture,
              (uint8_t *)(uintptr_t)(UINTPTR_MAX - (uintptr_t)1u), 4u) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(output, output_before, sizeof(output)) == 0);

    CHECK(gbl_audio_capture_begin(machine, &capture, output,
                                  sizeof(output)) == GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(0), NULL) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(snapshot_machine(machine, &snapshot));
    memcpy(output_before, output, sizeof(output));
    capture_before = capture;
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(512), &capture) ==
          GBL_TIME_STATUS_OK);
    free(snapshot);
    snapshot = NULL;
    CHECK(snapshot_machine(machine, &snapshot));
    capture_before = capture;
    memcpy(output_before, output, sizeof(output));
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(511), &capture) ==
          GBL_TIME_STATUS_BACKWARD);
    CHECK(unchanged_machine(machine, snapshot));
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(output, output_before, sizeof(output)) == 0);
    free(snapshot);
    snapshot = NULL;

    CHECK(snapshot_machine(machine, &snapshot));
    capture_before = capture;
    memcpy(output_before, output, sizeof(output));
    capture.last_accepted_cycle++;
    capture_before = capture;
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(1024), &capture) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(unchanged_machine(machine, snapshot));
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(output, output_before, sizeof(output)) == 0);
    free(snapshot);
    snapshot = NULL;
    capture = capture_before;

    CHECK(gbl_audio_capture_begin(other, &capture, output,
                                  sizeof(output)) == GBL_TIME_STATUS_OK);
    CHECK(snapshot_machine(machine, &snapshot));
    capture_before = capture;
    memcpy(output_before, output, sizeof(output));
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(1024), &capture) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(unchanged_machine(machine, snapshot));
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(output, output_before, sizeof(output)) == 0);
    free(snapshot);
    snapshot = NULL;

    /* A session cannot reuse spare capacity as if each append were fresh. */
    gbl_machine_reset(machine);
    memset(output, 0xA5, sizeof(output));
    CHECK(gbl_audio_capture_begin(machine, &capture, output, 4u) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(512), &capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(snapshot_machine(machine, &snapshot));
    capture_before = capture;
    memcpy(output_before, output, sizeof(output));
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(1024), &capture) ==
          GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT);
    CHECK(unchanged_machine(machine, snapshot));
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(output, output_before, sizeof(output)) == 0);
    free(snapshot);
    snapshot = NULL;

    /* A null/zero destination may advance before the first requested frame. */
    gbl_machine_reset(machine);
    CHECK(gbl_audio_capture_begin(machine, &capture, NULL, 0u) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(511), &capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(capture.bytes_written == 0u);
    capture_before = capture;
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(512), &capture) ==
          GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT);
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);

    /* An uncaptured clock advance breaks the session's contiguous timeline. */
    gbl_machine_reset(machine);
    CHECK(gbl_audio_capture_begin(machine, &capture, output,
                                  sizeof(output)) == GBL_TIME_STATUS_OK);
    capture_before = capture;
    memcpy(output_before, output, sizeof(output));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(1)) ==
          GBL_TIME_STATUS_OK);
    CHECK(snapshot_machine(machine, &snapshot));
    capture_before = capture;
    memcpy(output_before, output, sizeof(output));
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(512), &capture) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(unchanged_machine(machine, snapshot));
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(output, output_before, sizeof(output)) == 0);

    /* Output, descriptor, machine, and ROM ranges cannot alias. */
    memset(&capture, 0x4D, sizeof(capture));
    capture_before = capture;
    CHECK(gbl_audio_capture_begin(
              machine, &capture, (uint8_t *)(void *)machine, 4u) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(gbl_audio_capture_begin(
              machine, (gbl_audio_capture *)(void *)machine,
              output, sizeof(output)) == GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(gbl_audio_capture_begin(
              machine, &capture, (uint8_t *)(void *)&capture,
              sizeof(output)) == GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    {
        uint8_t synthetic_rom[8] = { 0u };
        gbl_audio_capture *rom_capture = (gbl_audio_capture *)(void *)
            synthetic_rom;

        machine->rom = synthetic_rom;
        machine->rom_length = sizeof(synthetic_rom);
        CHECK(gbl_audio_capture_begin(machine, &capture, synthetic_rom,
                                      sizeof(synthetic_rom)) ==
              GBL_TIME_STATUS_INVALID_ARGUMENT);
        CHECK(gbl_audio_capture_begin(machine, rom_capture, output,
                                      sizeof(output)) ==
              GBL_TIME_STATUS_INVALID_ARGUMENT);
        machine->rom = NULL;
        machine->rom_length = 0u;
    }
    CHECK(unchanged_machine(machine, snapshot));
    free(snapshot);
    snapshot = NULL;

    /* An early staged audio frame followed by a malformed operation leaves
     * both machine and preview outputs unchanged. */
    gbl_machine_reset(machine);
    CHECK(snapshot_machine(machine, &snapshot));
    memset(&preview, 0x69, sizeof(preview));
    preview_before = preview;
    staged_bytes = 77u;
    operations[0] = (gbl_time_operation){
        .kind = GBL_TIME_OPERATION_BUS_WRITE,
        .width = 2u,
        .address = UINT32_C(0x04000084),
        .value = UINT32_C(0x0080),
        .cycles = 1u
    };
    operations[1] = (gbl_time_operation){
        .kind = GBL_TIME_OPERATION_INTERNAL,
        .cycles = UINT64_C(511)
    };
    {
        gbl_time_operation invalid = {
        .kind = (gbl_time_operation_kind)99,
        .cycles = 1u
        };
        operations[2] = invalid;
    }
    CHECK(gbl_machine_prepare_operations_audio_with_provider(
              machine, operations, 3u, &preview, staging, sizeof(staging),
              &staged_bytes, NULL, NULL) ==
          GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(unchanged_machine(machine, snapshot));
    CHECK(memcmp(&preview, &preview_before, sizeof(preview)) == 0);
    CHECK(staged_bytes == 77u);
    CHECK(staging[0] == 0u && staging[1] == 0u &&
          staging[2] == 0u && staging[3] == 0u);
    free(snapshot);
    snapshot = NULL;

    /* Dense timer dispatch and a monolithic software-second advance both hit
     * the unchanged scheduler authority without publishing staged output. */
    gbl_machine_reset(machine);
    memset(dense_output, 0xD7, sizeof(dense_output));
    CHECK(write16(machine, UINT32_C(0x04000100), UINT16_C(0xFFC0)).status ==
          GBL_BUS_STATUS_OK);
    CHECK(write16(machine, UINT32_C(0x04000102), UINT16_C(0x00C0)).status ==
          GBL_BUS_STATUS_OK);
    CHECK(gbl_audio_capture_begin(machine, &capture, dense_output,
                                  sizeof(dense_output)) ==
          GBL_TIME_STATUS_OK);
    CHECK(snapshot_machine(machine, &snapshot));
    capture_before = capture;
    for (index = 0u; index < sizeof(output_before); ++index) {
        output_before[index] = dense_output[index];
    }
    CHECK(gbl_machine_advance_to_audio(machine, UINT64_C(1048576),
                                       &capture) ==
          GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT);
    CHECK(unchanged_machine(machine, snapshot));
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(dense_output, output_before, sizeof(output_before)) == 0);
    for (index = 0u; index < sizeof(dense_output); ++index) {
        CHECK(dense_output[index] == UINT8_C(0xD7));
    }
    free(snapshot);
    snapshot = NULL;
    free_machine(machine);

    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        memset(dense_output, 0xD7, sizeof(dense_output));
        CHECK(gbl_audio_capture_begin(machine, &capture, dense_output,
                                      sizeof(dense_output)) ==
              GBL_TIME_STATUS_OK);
        CHECK(snapshot_machine(machine, &snapshot));
        capture_before = capture;
        memcpy(output_before, dense_output, sizeof(output_before));
        CHECK(gbl_machine_advance_to_audio(
                  machine, GBL_AUDIO_MAX_CAPTURE_CYCLES, &capture) ==
              GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT);
        CHECK(unchanged_machine(machine, snapshot));
        CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
        CHECK(memcmp(dense_output, output_before, sizeof(output_before)) == 0);
        for (index = 0u; index < sizeof(dense_output); ++index) {
            CHECK(dense_output[index] == UINT8_C(0xD7));
        }
    }
    free(snapshot);
    free_machine(machine);
    free_machine(other);
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
    if (strcmp(argv[1], "psg_square") == 0) {
        return run_psg_square();
    }
    if (strcmp(argv[1], "cadence") == 0) {
        return run_cadence();
    }
    if (strcmp(argv[1], "boundaries") == 0) {
        return run_boundaries();
    }
    if (strcmp(argv[1], "cpu") == 0) {
        return run_cpu();
    }
    if (strcmp(argv[1], "refusal") == 0) {
        return run_refusal();
    }
    return 2;
}
