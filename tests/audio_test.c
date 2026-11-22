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

static bool configure_ch2(gbl_machine *machine, uint8_t length_setting,
                          unsigned duty, unsigned volume, bool increase,
                          unsigned envelope_period, uint16_t frequency,
                          bool length_enable)
{
    const uint16_t control =
        (uint16_t)((uint16_t)(length_setting & UINT8_C(0x3F)) |
                   (uint16_t)((duty & 3u) << 6u) |
                   (uint16_t)((envelope_period & 7u) << 8u) |
                   (increase ? UINT16_C(0x0800) : 0u) |
                   (uint16_t)((volume & 15u) << 12u));
    const uint16_t frequency_control =
        (uint16_t)((frequency & UINT16_C(0x07FF)) |
                   (length_enable ? UINT16_C(0x4000) : 0u) |
                   UINT16_C(0x8000));

    return machine != NULL &&
           write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x04000068), control).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x0400006C), frequency_control).status ==
               GBL_BUS_STATUS_OK;
}

static bool configure_ch1(gbl_machine *machine, uint8_t length_setting,
                          unsigned duty, unsigned volume, bool increase,
                          unsigned envelope_period, unsigned sweep_time,
                          bool sweep_decrease, unsigned sweep_shift,
                          uint16_t frequency, bool length_enable)
{
    const uint16_t sweep =
        (uint16_t)((sweep_time & 7u) << 4u) |
        (sweep_decrease ? UINT16_C(0x0008) : 0u) |
        (uint16_t)(sweep_shift & 7u);
    const uint16_t control =
        (uint16_t)(length_setting & UINT8_C(0x3F)) |
        (uint16_t)((duty & 3u) << 6u) |
        (uint16_t)((envelope_period & 7u) << 8u) |
        (increase ? UINT16_C(0x0800) : 0u) |
        (uint16_t)((volume & 15u) << 12u);
    const uint16_t frequency_control =
        (uint16_t)(frequency & UINT16_C(0x07FF)) |
        (length_enable ? UINT16_C(0x4000) : 0u) | UINT16_C(0x8000);

    return machine != NULL &&
           write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x04000060), sweep).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x04000062), control).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x04000064), frequency_control).status ==
               GBL_BUS_STATUS_OK;
}

static bool configure_ch4_noise(gbl_machine *machine,
                                uint8_t length_setting, unsigned volume,
                                bool increase, unsigned envelope_period,
                                unsigned shift, bool width_7bit,
                                unsigned ratio, bool length_enable)
{
    const uint16_t control =
        (uint16_t)(length_setting & UINT8_C(0x3F)) |
        (uint16_t)((envelope_period & 7u) << 8u) |
        (increase ? UINT16_C(0x0800) : 0u) |
        (uint16_t)((volume & 15u) << 12u);
    const uint16_t noise_control =
        (uint16_t)(ratio & 7u) |
        (width_7bit ? UINT16_C(0x0008) : 0u) |
        (uint16_t)((shift & 15u) << 4u) |
        (length_enable ? UINT16_C(0x4000) : 0u) | UINT16_C(0x8000);

    return machine != NULL &&
           write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x04000078), control).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x0400007C), noise_control).status ==
               GBL_BUS_STATUS_OK;
}

static bool load_wave_bank(gbl_machine *machine, unsigned bank,
                           uint8_t packed_byte)
{
    const uint16_t selected_front = (uint16_t)(UINT16_C(0x0080) |
                                               (bank == 0u
                                                    ? UINT16_C(0x0040)
                                                    : 0u));
    unsigned offset;

    if (machine == NULL || bank > 1u ||
        write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000070), selected_front).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    for (offset = 0u; offset < 16u; offset += 4u) {
        const uint32_t packed = (uint32_t)packed_byte |
                                ((uint32_t)packed_byte << 8u) |
                                ((uint32_t)packed_byte << 16u) |
                                ((uint32_t)packed_byte << 24u);

        if (!gbl_audio_wave_write(&machine->audio,
                                  UINT32_C(0x04000090) + offset, 4u,
                                  packed)) {
            return false;
        }
    }
    return true;
}

static bool configure_wave(gbl_machine *machine, unsigned front_bank,
                           bool dual_bank, uint16_t volume_control,
                           uint8_t length, uint16_t frequency,
                           bool length_enable)
{
    const uint16_t wave_control =
        (uint16_t)(UINT16_C(0x0080) |
                   (front_bank != 0u ? UINT16_C(0x0040) : 0u) |
                   (dual_bank ? UINT16_C(0x0020) : 0u));
    const uint16_t wave_length_volume =
        (uint16_t)((uint16_t)length | volume_control);
    const uint16_t wave_frequency =
        (uint16_t)(frequency | (length_enable ? UINT16_C(0x4000) : 0u) |
                   UINT16_C(0x8000));

    return machine != NULL &&
           write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x04000070), wave_control).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x04000072), wave_length_volume).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x04000074), wave_frequency).status ==
               GBL_BUS_STATUS_OK;
}

static int run_psg_wave(void)
{
    gbl_machine *machine = new_machine();
    uint64_t trigger_cycle = 0u;
    uint8_t byte_value = 0u;

    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");
    tap_check("machine is initialized", machine != NULL);
    if (machine == NULL) {
        puts("1..1");
        return 1;
    }
    tap_check("both packed Wave RAM banks accept bounded private loads",
              load_wave_bank(machine, 0u, UINT8_C(0x32)) &&
                  load_wave_bank(machine, 1u, UINT8_C(0x54)) &&
                  gbl_audio_wave_read_byte(&machine->audio,
                                            UINT32_C(0x04000090),
                                            &byte_value) &&
                  byte_value == UINT8_C(0x54) &&
                  write16(machine, UINT32_C(0x04000070),
                          UINT16_C(0x00C0)).status == GBL_BUS_STATUS_OK &&
                  gbl_audio_wave_read_byte(&machine->audio,
                                            UINT32_C(0x04000090),
                                            &byte_value) &&
                  byte_value == UINT8_C(0x32));
    tap_check("selected 32-step front bank triggers at phase zero",
              configure_wave(machine, 0u, false, UINT16_C(0x2000), 0u,
                             UINT16_C(2047), false) &&
                  (read16(machine, UINT32_C(0x04000084)).value & 7u) == 4u &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 3u);
    trigger_cycle = machine->master_cycles;
    tap_check("manual whole-pattern rate uses a four-cycle nibble step",
              gbl_machine_advance_to(machine, trigger_cycle + 3u) ==
                      GBL_TIME_STATUS_OK &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 3u &&
                  gbl_machine_advance_to(machine, trigger_cycle + 4u) ==
                      GBL_TIME_STATUS_OK &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 2u);
    tap_check("32-step playback wraps to its selected front bank",
              gbl_machine_advance_to(machine, trigger_cycle + 128u) ==
                      GBL_TIME_STATUS_OK &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 3u);
    free_machine(machine);

    {
        static const uint16_t volume_controls[5] = {
            UINT16_C(0x0000), UINT16_C(0x2000), UINT16_C(0x4000),
            UINT16_C(0x6000), UINT16_C(0x8000)
        };
        static const uint8_t expected_levels[5] = { 0u, 3u, 1u, 0u, 2u };
        unsigned volume;

        for (volume = 0u; volume < 5u; ++volume) {
            gbl_machine *probe = new_machine();
            bool valid = probe != NULL;

            if (valid) {
                valid = load_wave_bank(probe, 0u, UINT8_C(0x32)) &&
                        load_wave_bank(probe, 1u, UINT8_C(0x54)) &&
                        configure_wave(probe, 0u, false,
                                       volume_controls[volume], 0u,
                                       UINT16_C(2047), false) &&
                        gbl_audio_psg_sample(&probe->audio, 2u) ==
                            expected_levels[volume];
            }
            tap_check("CH3 applies mute/full/half/quarter/forced volume",
                      valid);
            free_machine(probe);
        }
    }

    {
        gbl_machine *probe = new_machine();
        uint64_t anchor = 0u;
        bool valid = probe != NULL;

        if (valid) {
            valid = load_wave_bank(probe, 0u, UINT8_C(0x32)) &&
                    load_wave_bank(probe, 1u, UINT8_C(0x54)) &&
                    configure_wave(probe, 0u, true, UINT16_C(0x2000), 0u,
                                   UINT16_C(2047), false);
            anchor = probe->master_cycles;
            valid = valid &&
                    gbl_machine_advance_to(probe, anchor + 127u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 2u &&
                    gbl_machine_advance_to(probe, anchor + 128u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 5u &&
                    gbl_machine_advance_to(probe, anchor + 132u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 4u &&
                    gbl_machine_advance_to(probe, anchor + 256u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 3u;
        }
        tap_check("64-step playback concatenates selected and other banks",
                  valid);
        free_machine(probe);
    }

    {
        gbl_machine *probe = new_machine();
        uint64_t anchor = 0u;
        bool valid = probe != NULL;

        if (valid) {
            valid = load_wave_bank(probe, 0u, UINT8_C(0x32)) &&
                    load_wave_bank(probe, 1u, UINT8_C(0x54)) &&
                    configure_wave(probe, 1u, true, UINT16_C(0x2000), 0u,
                                   UINT16_C(2046), false);
            anchor = probe->master_cycles;
            valid = valid &&
                    gbl_machine_advance_to(probe, anchor + 7u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 5u &&
                    gbl_machine_advance_to(probe, anchor + 8u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 4u &&
                    gbl_machine_advance_to(probe, anchor + 256u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 5u;
        }
        tap_check("CH3 uses eight-cycle steps at frequency 2046 and bank 1",
                  valid);
        free_machine(probe);
    }

    {
        gbl_machine *probe = new_machine();
        bool valid = probe != NULL;

        if (valid) {
            valid = load_wave_bank(probe, 0u, UINT8_C(0x32)) &&
                    load_wave_bank(probe, 1u, UINT8_C(0x54)) &&
                    configure_wave(probe, 0u, false, UINT16_C(0x2000), 255u,
                                   UINT16_C(2047), true) &&
                    configure_ch2(probe, 0u, 0u, 15u, false, 0u,
                                  UINT16_C(2047), false);
            valid = valid && probe->audio.length_remaining[2] == 1u &&
                    gbl_machine_advance_to(probe, UINT64_C(32767)) ==
                        GBL_TIME_STATUS_OK &&
                    (probe->audio.active_channels & 6u) == 6u &&
                    gbl_machine_advance_to(probe, UINT64_C(32768)) ==
                        GBL_TIME_STATUS_OK &&
                    probe->audio.length_remaining[2] == 0u &&
                    (probe->audio.active_channels & 6u) == 2u;
        }
        tap_check("CH3 length expires its own status and preserves CH2", valid);
        free_machine(probe);
    }

    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

static bool advance_audio_without_capture(gbl_audio_state *audio,
                                          uint64_t target)
{
    gbl_machine_time_preview preview = { 0 };
    size_t staged_bytes = 0u;

    if (audio == NULL) {
        return false;
    }
    preview.audio = *audio;
    if (gbl_audio_advance_span(&preview, target, NULL, 0u, &staged_bytes,
                               NULL, NULL) != GBL_TIME_STATUS_OK) {
        return false;
    }
    *audio = preview.audio;
    return true;
}

static uint16_t reference_noise_step(uint16_t lfsr, bool width_7bit)
{
    const uint16_t mask = width_7bit ? UINT16_C(0x0060) : UINT16_C(0x6000);
    const uint16_t width_mask =
        width_7bit ? UINT16_C(0x007F) : UINT16_C(0x7FFF);
    const uint16_t feedback = (uint16_t)(lfsr & UINT16_C(0x0001));

    lfsr = (uint16_t)(lfsr >> 1u);
    return (uint16_t)((lfsr ^ (feedback != 0u ? mask : 0u)) & width_mask);
}

static bool reference_noise_advance(const gbl_audio_state *initial,
                                   uint64_t target, uint32_t period,
                                   bool width_7bit, uint16_t *lfsr,
                                   uint32_t *remainder)
{
    uint64_t elapsed;
    uint64_t steps;
    uint32_t fraction;
    uint32_t orbit_period;

    if (initial == NULL || lfsr == NULL || remainder == NULL || period == 0u ||
        target < initial->cycle_anchor) {
        return false;
    }
    elapsed = target - initial->cycle_anchor;
    steps = elapsed / period;
    fraction = (uint32_t)(elapsed % period) +
               initial->noise_cycle_remainder;
    if (fraction >= period) {
        fraction -= period;
        ++steps;
    }
    orbit_period = width_7bit ? UINT32_C(127) : UINT32_C(32767);
    steps %= orbit_period;
    *lfsr = initial->noise_lfsr;
    while (steps != 0u) {
        *lfsr = reference_noise_step(*lfsr, width_7bit);
        --steps;
    }
    *remainder = fraction;
    return true;
}

static bool verify_noise_orbit(bool width_7bit)
{
    bool visited[32768] = { false };
    const uint16_t seed = width_7bit ? UINT16_C(0x0040)
                                      : UINT16_C(0x4000);
    const uint32_t mask = width_7bit ? UINT32_C(0x007F)
                                      : UINT32_C(0x7FFF);
    const uint32_t period = width_7bit ? UINT32_C(127)
                                        : UINT32_C(32767);
    uint16_t lfsr = seed;
    uint32_t index;

    for (index = 0u; index < period; ++index) {
        if (lfsr == 0u || visited[lfsr]) {
            return false;
        }
        visited[lfsr] = true;
        lfsr = reference_noise_step(lfsr, width_7bit);
    }
    if (lfsr != seed) {
        return false;
    }
    for (index = 1u; index <= mask; ++index) {
        if (!visited[index]) {
            return false;
        }
    }
    return true;
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

    {
        unsigned duty;
        static const uint32_t high_cycles[4] = { 16u, 32u, 64u, 96u };

        for (duty = 0u; duty < 4u; ++duty) {
            gbl_machine *probe = new_machine();
            bool valid = probe != NULL;

            if (valid) {
                const uint64_t anchor = probe->master_cycles + 3u;

                valid = configure_ch2(probe, 0u, duty, 15u, false, 0u,
                                      UINT16_C(2047), false) &&
                        gbl_machine_advance_to(
                            probe, anchor + high_cycles[duty] - 1u) ==
                            GBL_TIME_STATUS_OK &&
                        gbl_audio_psg_sample(&probe->audio, 1u) == 15u &&
                        gbl_machine_advance_to(
                            probe, anchor + high_cycles[duty]) ==
                            GBL_TIME_STATUS_OK &&
                        gbl_audio_psg_sample(&probe->audio, 1u) == 0u &&
                        gbl_machine_advance_to(
                            probe, anchor + high_cycles[duty] + 1u) ==
                            GBL_TIME_STATUS_OK &&
                        gbl_audio_psg_sample(&probe->audio, 1u) == 0u &&
                        gbl_machine_advance_to(probe, anchor + 127u) ==
                            GBL_TIME_STATUS_OK &&
                        gbl_audio_psg_sample(&probe->audio, 1u) == 0u &&
                        gbl_machine_advance_to(probe, anchor + 128u) ==
                            GBL_TIME_STATUS_OK &&
                        gbl_audio_psg_sample(&probe->audio, 1u) == 15u &&
                        gbl_machine_advance_to(probe, anchor + 129u) ==
                            GBL_TIME_STATUS_OK &&
                        gbl_audio_psg_sample(&probe->audio, 1u) == 15u;
            }
            tap_check("each duty ratio has its inclusive edge and wrap",
                      valid);
            free_machine(probe);
        }
    }

    {
        gbl_machine *probe = new_machine();
        bool valid = probe != NULL;

        if (valid) {
            const uint64_t anchor = probe->master_cycles + 3u;
            const uint32_t high_cycles = UINT32_C(196608);

            valid = configure_ch2(probe, 0u, 3u, 15u, false, 0u, 0u,
                                  false) &&
                    gbl_machine_advance_to(probe, anchor + high_cycles - 1u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 1u) == 15u &&
                    gbl_machine_advance_to(probe, anchor + high_cycles) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 1u) == 0u &&
                    gbl_machine_advance_to(probe, anchor + UINT32_C(262144)) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 1u) == 15u;
        }
        tap_check("frequency zero uses the maximum period and wraps", valid);
        free_machine(probe);
    }

    {
        gbl_machine *probe = new_machine();
        bool valid = probe != NULL;

        if (valid) {
            const uint64_t anchor = probe->master_cycles + 3u;
            gbl_bus_result result;

            valid = configure_ch2(probe, 0u, 3u, 15u, false, 0u,
                                  UINT16_C(2046), false) &&
                    gbl_machine_advance_to(probe, anchor + 200u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 1u) == 0u;
            result = write16(probe, UINT32_C(0x0400006C),
                             UINT16_C(0x07FF));
            valid = valid && result.status == GBL_BUS_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 1u) == 15u;
        }
        tap_check("live frequency changes preserve elapsed phase modulo period",
                  valid);
        free_machine(probe);
    }
    {
        const gbl_audio_state inactive = { 0 };

        tap_check("null and unsupported channel samples are zero",
                  gbl_audio_psg_sample(NULL, 1u) == 0u &&
                      gbl_audio_psg_sample(&inactive, 2u) == 0u &&
                      gbl_audio_psg_sample(&inactive, 4u) == 0u);
    }
    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

static int run_psg_noise(void)
{
    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;
        uint64_t trigger_cycle = 0u;

        if (passed) {
            passed = configure_ch4_noise(machine, 0u, 15u, false, 0u,
                                         0u, false, 0u, false);
            trigger_cycle = machine->master_cycles;
        }
        tap_check("15-bit noise trigger configures CH4", passed);
        tap_check("15-bit seed projects high through complement of LFSR bit zero",
                  passed && gbl_audio_psg_sample(&machine->audio, 3u) == 15u &&
                      machine->audio.noise_lfsr == UINT16_C(0x4000) &&
                      (gbl_audio_read_register(
                           &machine->audio, UINT32_C(0x04000084)) & 8u) !=
                          0u);
        tap_check("first 15-bit noise edge is unchanged at edge minus one",
                  passed && gbl_machine_advance_to(
                                machine, trigger_cycle + 31u) ==
                                GBL_TIME_STATUS_OK &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u);
        tap_check("first 15-bit noise edge is applied at the divider period",
                  passed && gbl_machine_advance_to(
                                machine, trigger_cycle + 32u) ==
                                GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x2000) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u);
        tap_check("15-bit noise state holds immediately after its first edge",
                  passed && gbl_machine_advance_to(
                                machine, trigger_cycle + 33u) ==
                                GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x2000) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u);
        tap_check("15-bit first feedback uses the selected right-shift mask",
                  passed && gbl_machine_advance_to(
                                machine, trigger_cycle + 479u) ==
                                GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x0001) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 0u &&
                      gbl_machine_advance_to(
                          machine, trigger_cycle + 480u) ==
                          GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x6000) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u &&
                      gbl_machine_advance_to(
                          machine, trigger_cycle + 481u) ==
                          GBL_TIME_STATUS_OK &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;
        uint64_t trigger_cycle = 0u;

        if (passed) {
            passed = configure_ch4_noise(machine, 0u, 15u, false, 0u,
                                         0u, true, 0u, false);
            trigger_cycle = machine->master_cycles;
        }
        tap_check("7-bit noise trigger configures CH4", passed);
        tap_check("7-bit seed projects high through complement of bit zero",
                  passed && gbl_audio_psg_sample(&machine->audio, 3u) == 15u &&
                      machine->audio.noise_lfsr == UINT16_C(0x0040) &&
                      (gbl_audio_read_register(
                           &machine->audio, UINT32_C(0x04000084)) & 8u) !=
                          0u);
        tap_check("7-bit first noise edge is unchanged one cycle before",
                  passed && gbl_machine_advance_to(
                                machine, trigger_cycle + 31u) ==
                                GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x0040) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u);
        tap_check("7-bit first noise edge is applied at its divider period",
                  passed && gbl_machine_advance_to(
                                machine, trigger_cycle + 32u) ==
                                GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x0020) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u &&
                      gbl_machine_advance_to(
                          machine, trigger_cycle + 33u) ==
                          GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x0020));
        tap_check("7-bit feedback edge has adjacent cycle expectations",
                  passed && gbl_machine_advance_to(
                                machine, trigger_cycle + 191u) ==
                                GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x0002) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u &&
                      gbl_machine_advance_to(
                          machine, trigger_cycle + 192u) ==
                          GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x0001) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 0u &&
                      gbl_machine_advance_to(
                          machine, trigger_cycle + 223u) ==
                          GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x0001) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 0u &&
                      gbl_machine_advance_to(
                          machine, trigger_cycle + 224u) ==
                          GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x0060) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u &&
                      gbl_machine_advance_to(
                          machine, trigger_cycle + 225u) ==
                          GBL_TIME_STATUS_OK &&
                      machine->audio.noise_lfsr == UINT16_C(0x0060) &&
                      gbl_audio_psg_sample(&machine->audio, 3u) == 15u);
        free_machine(machine);
    }

    tap_check("independent 7-bit LFSR orbit returns after 127 unique nonzero states",
              verify_noise_orbit(true));
    tap_check("independent 15-bit LFSR orbit returns after 32767 unique nonzero states",
              verify_noise_orbit(false));

    {
        bool passed = true;
        bool width_7bit;

        for (width_7bit = false; ; width_7bit = true) {
            gbl_machine *machine = new_machine();
            gbl_audio_state state = { 0 };
            const uint16_t expected =
                width_7bit ? UINT16_C(0x0060) : UINT16_C(0x6000);
            bool step_passed = machine != NULL;

            if (step_passed) {
                step_passed = configure_ch4_noise(
                                  machine, 0u, 15u, false, 0u, 0u,
                                  width_7bit, 0u, false);
                state = machine->audio;
                state.noise_lfsr = 1u;
                state.noise_cycle_remainder = 31u;
                step_passed = step_passed &&
                              gbl_audio_state_valid(&state) &&
                              advance_audio_without_capture(
                                  &state, state.cycle_anchor + 1u) &&
                              state.noise_lfsr == expected &&
                              state.noise_cycle_remainder == 0u &&
                              gbl_audio_psg_sample(&state, 3u) == 15u;
            }
            passed = passed && step_passed;
            free_machine(machine);
            if (width_7bit) {
                break;
            }
        }
        tap_check("feedback from state 0x01 applies 0x6000 and 0x60 width masks",
                  passed);
    }

    {
        gbl_machine *ratio = new_machine();
        gbl_machine *maximum_shift = new_machine();
        bool passed = ratio != NULL && maximum_shift != NULL;

        if (passed) {
            passed = configure_ch4_noise(ratio, 0u, 15u, false, 0u, 0u,
                                         false, 3u, false) &&
                     configure_ch4_noise(maximum_shift, 0u, 15u, false,
                                         0u, 13u, false, 7u, false) &&
                     advance_audio_without_capture(
                         &ratio->audio,
                         ratio->audio.cycle_anchor + UINT64_C(191)) &&
                     ratio->audio.noise_lfsr == UINT16_C(0x4000) &&
                     ratio->audio.noise_cycle_remainder == UINT32_C(191) &&
                     advance_audio_without_capture(
                         &ratio->audio,
                         ratio->audio.cycle_anchor + UINT64_C(1)) &&
                     ratio->audio.noise_lfsr == UINT16_C(0x2000) &&
                     ratio->audio.noise_cycle_remainder == 0u &&
                     advance_audio_without_capture(
                         &maximum_shift->audio,
                         maximum_shift->audio.cycle_anchor +
                             UINT64_C(3670015)) &&
                     maximum_shift->audio.noise_lfsr == UINT16_C(0x4000) &&
                     maximum_shift->audio.noise_cycle_remainder ==
                         UINT32_C(3670015) &&
                     advance_audio_without_capture(
                         &maximum_shift->audio,
                         maximum_shift->audio.cycle_anchor + UINT64_C(1)) &&
                     maximum_shift->audio.noise_lfsr == UINT16_C(0x2000) &&
                     maximum_shift->audio.noise_cycle_remainder == 0u &&
                     gbl_audio_state_valid(&maximum_shift->audio);
        }
        tap_check("ratio code 3 and maximum allowed shift 13 use source divider periods",
                  passed);
        free_machine(ratio);
        free_machine(maximum_shift);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch4_noise(machine, 0u, 15u, false, 0u,
                                         0u, false, 0u, false) &&
                     advance_audio_without_capture(
                         &machine->audio,
                         machine->audio.cycle_anchor + UINT64_C(15)) &&
                     machine->audio.noise_cycle_remainder == 15u;
            gbl_audio_apply_register_write(&machine->audio,
                                           UINT32_C(0x0400007C),
                                           UINT16_C(0x0008), UINT8_C(3));
            passed = passed &&
                     machine->audio.noise_lfsr == UINT16_C(0x0040) &&
                     machine->audio.noise_cycle_remainder == 0u &&
                     advance_audio_without_capture(
                         &machine->audio,
                         machine->audio.cycle_anchor + UINT64_C(31)) &&
                     machine->audio.noise_cycle_remainder == 31u;
            gbl_audio_apply_register_write(&machine->audio,
                                           UINT32_C(0x0400007C),
                                           UINT16_C(0x0009), UINT8_C(3));
            passed = passed &&
                     machine->audio.noise_lfsr == UINT16_C(0x0040) &&
                     machine->audio.noise_cycle_remainder == 31u &&
                     advance_audio_without_capture(
                         &machine->audio,
                         machine->audio.cycle_anchor + UINT64_C(16)) &&
                     machine->audio.noise_cycle_remainder == 47u;
            gbl_audio_apply_register_write(&machine->audio,
                                           UINT32_C(0x0400007C),
                                           UINT16_C(0x0008), UINT8_C(3));
            passed = passed &&
                     machine->audio.noise_cycle_remainder == 15u &&
                     machine->audio.noise_lfsr == UINT16_C(0x0040);
            gbl_audio_apply_register_write(&machine->audio,
                                           UINT32_C(0x0400007C),
                                           UINT16_C(0x00E8), UINT8_C(3));
            passed = passed &&
                     machine->audio.noise_cycle_remainder == 15u &&
                     advance_audio_without_capture(
                         &machine->audio,
                         machine->audio.cycle_anchor + UINT64_C(1000)) &&
                     machine->audio.noise_cycle_remainder == 15u &&
                     machine->audio.noise_lfsr == UINT16_C(0x0040) &&
                     gbl_audio_state_valid(&machine->audio);
            gbl_audio_apply_register_write(&machine->audio,
                                           UINT32_C(0x0400007C),
                                           UINT16_C(0x0008), UINT8_C(3));
            passed = passed &&
                     machine->audio.noise_cycle_remainder == 15u &&
                     advance_audio_without_capture(
                         &machine->audio,
                         machine->audio.cycle_anchor + UINT64_C(16)) &&
                     machine->audio.noise_lfsr == UINT16_C(0x0040) &&
                     machine->audio.noise_cycle_remainder == 31u &&
                     advance_audio_without_capture(
                         &machine->audio,
                         machine->audio.cycle_anchor + UINT64_C(1)) &&
                     machine->audio.noise_lfsr == UINT16_C(0x0020) &&
                     machine->audio.noise_cycle_remainder == 0u;
            gbl_audio_apply_register_write(&machine->audio,
                                           UINT32_C(0x0400007C), 0u,
                                           UINT8_C(3));
            passed = passed &&
                     machine->audio.noise_lfsr == UINT16_C(0x4000) &&
                     machine->audio.noise_cycle_remainder == 0u &&
                     gbl_audio_state_valid(&machine->audio);
        }
        tap_check("width transitions reseed while allowed and stopped divider changes preserve then normalize phase",
                  passed);
        free_machine(machine);
    }

    {
        gbl_machine *rising = new_machine();
        gbl_machine *falling = new_machine();
        bool passed = rising != NULL && falling != NULL;

        if (passed) {
            passed = configure_ch4_noise(rising, 63u, 14u, true, 1u,
                                         14u, false, 0u, false) &&
                     configure_ch4_noise(falling, 63u, 1u, false, 1u,
                                         15u, true, 0u, false) &&
                     gbl_machine_advance_to(rising,
                                            UINT64_C(262143)) ==
                         GBL_TIME_STATUS_OK &&
                     rising->audio.channel_volume[3] == 14u &&
                     gbl_machine_advance_to(rising,
                                            UINT64_C(262144)) ==
                         GBL_TIME_STATUS_OK &&
                     rising->audio.channel_volume[3] == 15u &&
                     rising->audio.noise_lfsr == UINT16_C(0x4000) &&
                     gbl_machine_advance_to(rising,
                                            UINT64_C(524288)) ==
                         GBL_TIME_STATUS_OK &&
                     rising->audio.channel_volume[3] == 15u &&
                     rising->audio.length_remaining[3] == 1u &&
                     (gbl_audio_read_register(
                          &rising->audio, UINT32_C(0x04000084)) & 8u) !=
                         0u &&
                     gbl_machine_advance_to(falling,
                                            UINT64_C(262144)) ==
                         GBL_TIME_STATUS_OK &&
                     falling->audio.channel_volume[3] == 0u &&
                     gbl_machine_advance_to(falling,
                                            UINT64_C(524288)) ==
                         GBL_TIME_STATUS_OK &&
                     falling->audio.channel_volume[3] == 0u &&
                     falling->audio.noise_lfsr == UINT16_C(0x0040);
        }
        tap_check("envelopes saturate both directions while prohibited noise holds and disabled length stays loaded",
                  passed);
        free_machine(rising);
        free_machine(falling);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch4_noise(machine, 63u, 5u, false, 0u,
                                         0u, false, 0u, true) &&
                     gbl_machine_advance_to(machine,
                                            UINT64_C(32767)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.length_remaining[3] == 1u &&
                     (gbl_audio_read_register(
                          &machine->audio, UINT32_C(0x04000084)) & 8u) !=
                         0u &&
                     gbl_machine_advance_to(machine, UINT64_C(32768)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.length_remaining[3] == 0u &&
                     (gbl_audio_read_register(
                          &machine->audio, UINT32_C(0x04000084)) & 8u) ==
                         0u &&
                     gbl_audio_psg_sample(&machine->audio, 3u) == 0u;
        }
        tap_check("enabled one-clock CH4 length expires activity and source output",
                  passed);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        gbl_machine *near_limit = new_machine();
        gbl_audio_state whole = { 0 };
        gbl_audio_state split = { 0 };
        gbl_audio_state near = { 0 };
        const uint64_t long_target = UINT64_C(1000000000000);
        uint16_t expected_lfsr = 0u;
        uint32_t expected_remainder = 0u;
        bool passed = machine != NULL && near_limit != NULL;

        if (passed) {
            passed = configure_ch4_noise(machine, 0u, 15u, false, 0u,
                                         0u, false, 0u, false) &&
                     configure_ch4_noise(near_limit, 0u, 15u, false,
                                         0u, 0u, false, 0u, false);
            whole = machine->audio;
            split = machine->audio;
            near = near_limit->audio;
        }
        passed = passed &&
                 reference_noise_advance(&whole, long_target, 32u, false,
                                         &expected_lfsr,
                                         &expected_remainder) &&
                 advance_audio_without_capture(&whole, long_target) &&
                 whole.noise_lfsr == expected_lfsr &&
                 whole.noise_cycle_remainder == expected_remainder &&
                 advance_audio_without_capture(
                     &split, split.cycle_anchor + UINT64_C(123456789)) &&
                 advance_audio_without_capture(
                     &split, split.cycle_anchor + UINT64_C(543210987654)) &&
                 advance_audio_without_capture(&split, long_target) &&
                 gbl_audio_state_equal(&whole, &split) &&
                 gbl_audio_state_valid(&whole);
        tap_check("long no-capture noise progression matches the independent orbit oracle across irregular partitions",
                  passed);

        expected_lfsr = 0u;
        expected_remainder = 0u;
        passed = near_limit != NULL &&
                 reference_noise_advance(&near, UINT64_MAX, 32u, false,
                                         &expected_lfsr,
                                         &expected_remainder) &&
                 advance_audio_without_capture(&near, UINT64_MAX) &&
                 near.cycle_anchor == UINT64_MAX &&
                 near.noise_lfsr == expected_lfsr &&
                 near.noise_cycle_remainder == expected_remainder &&
                 gbl_audio_state_valid(&near);
        tap_check("noise arithmetic remains bounded and valid near UINT64_MAX",
                  passed);
        free_machine(machine);
        free_machine(near_limit);
    }

    {
        gbl_machine *machine = new_machine();
        gbl_machine *snapshot = NULL;
        gbl_machine_time_preview preview = { 0 };
        bool prepared = machine != NULL;
        bool mismatch_refused = false;
        bool out_of_width_refused = false;
        bool invalid_remainder_refused = false;

        if (prepared) {
            prepared = configure_ch4_noise(machine, 0u, 15u, false, 0u,
                                           0u, true, 0u, false) &&
                       gbl_machine_advance_to(
                           machine, machine->master_cycles + UINT64_C(15)) ==
                           GBL_TIME_STATUS_OK &&
                       snapshot_machine(machine, &snapshot) &&
                       gbl_machine_prepare_time(
                           machine, machine->master_cycles + UINT64_C(1),
                           &preview) == GBL_TIME_STATUS_OK;
        }
        if (prepared) {
            preview.audio.noise_lfsr ^= UINT16_C(1);
            mismatch_refused =
                !gbl_audio_state_equal(&preview.audio,
                                       &preview.audio_prepared);
            preview.audio = preview.audio_prepared;
            preview.audio.noise_cycle_remainder ^= UINT32_C(1);
            mismatch_refused = mismatch_refused &&
                !gbl_audio_state_equal(&preview.audio,
                                       &preview.audio_prepared);
            mismatch_refused = mismatch_refused &&
                !gbl_machine_commit_time_checked(machine, &preview) &&
                snapshot != NULL && unchanged_machine(machine, snapshot);
        }
        tap_check("legal LFSR and divider-phase tampering cannot commit or change canonical state",
                  prepared && mismatch_refused);

        prepared = machine != NULL &&
                   gbl_machine_prepare_time(
                       machine, machine->master_cycles + UINT64_C(1),
                       &preview) == GBL_TIME_STATUS_OK;
        if (prepared) {
            preview.audio.noise_lfsr = UINT16_C(0x0080);
            preview.audio_prepared.noise_lfsr = UINT16_C(0x0080);
            out_of_width_refused =
                gbl_audio_state_equal(&preview.audio,
                                      &preview.audio_prepared) &&
                !gbl_audio_state_valid(&preview.audio) &&
                !gbl_machine_commit_time_checked(machine, &preview) &&
                snapshot != NULL && unchanged_machine(machine, snapshot);
        }
        tap_check("matching prepared copies with an out-of-width LFSR are refused",
                  out_of_width_refused);

        prepared = machine != NULL &&
                   gbl_machine_prepare_time(
                       machine, machine->master_cycles + UINT64_C(1),
                       &preview) == GBL_TIME_STATUS_OK;
        if (prepared) {
            preview.audio.noise_cycle_remainder = UINT32_MAX;
            preview.audio_prepared.noise_cycle_remainder = UINT32_MAX;
            invalid_remainder_refused =
                gbl_audio_state_equal(&preview.audio,
                                      &preview.audio_prepared) &&
                !gbl_audio_state_valid(&preview.audio) &&
                !gbl_machine_commit_time_checked(machine, &preview) &&
                snapshot != NULL && unchanged_machine(machine, snapshot);
        }
        tap_check("matching prepared copies with an out-of-range noise phase are refused",
                  invalid_remainder_refused);
        free(snapshot);
        free_machine(machine);
    }

    {
        unsigned shift;

        for (shift = 14u; shift <= 15u; ++shift) {
            gbl_machine *machine = new_machine();
            bool configured = machine != NULL;
            bool held = false;
            bool disabled = false;
            bool resumed = false;
            uint32_t paused_remainder = 0u;

            if (configured) {
                uint64_t trigger_cycle;
                const uint16_t control = (uint16_t)(shift << 4u);
                const uint64_t would_be_period = UINT64_C(32) << shift;

                configured = configure_ch4_noise(machine, 0u, 14u, true,
                                                 1u, 0u, false, 0u, false) &&
                             gbl_machine_advance_to(
                                 machine, machine->master_cycles + 15u) ==
                                 GBL_TIME_STATUS_OK;
                configured = configured &&
                             write16(machine, UINT32_C(0x0400007C), control)
                                     .status == GBL_BUS_STATUS_OK;
                trigger_cycle = machine->master_cycles;
                if (configured) {
                    paused_remainder =
                        machine->audio.noise_cycle_remainder;
                }
                held = configured &&
                       gbl_audio_read_register(
                           &machine->audio, UINT32_C(0x0400007C)) == control &&
                       paused_remainder != 0u && paused_remainder < 32u &&
                       machine->audio.noise_cycle_remainder ==
                           paused_remainder &&
                       machine->audio.noise_lfsr == UINT16_C(0x4000) &&
                       gbl_machine_advance_to(
                           machine, trigger_cycle + would_be_period) ==
                           GBL_TIME_STATUS_OK &&
                       machine->audio.noise_cycle_remainder ==
                           paused_remainder &&
                       machine->audio.noise_lfsr == UINT16_C(0x4000) &&
                       machine->audio.channel_volume[3] == 15u &&
                       gbl_audio_psg_sample(&machine->audio, 3u) == 15u &&
                       (gbl_audio_read_register(
                            &machine->audio, UINT32_C(0x04000084)) & 8u) !=
                           0u;
            }
            tap_check(shift == 14u
                          ? "prohibited shift 14 retains state through its would-be clock"
                          : "prohibited shift 15 retains state through its would-be clock",
                      held);

            if (configured) {
                disabled = write16(machine, UINT32_C(0x04000078), 0u).status ==
                               GBL_BUS_STATUS_OK &&
                           (gbl_audio_read_register(
                                &machine->audio, UINT32_C(0x04000084)) & 8u) ==
                               0u &&
                           machine->audio.noise_lfsr == 0u &&
                           gbl_audio_psg_sample(&machine->audio, 3u) == 0u;
                disabled = disabled &&
                           write16(machine, UINT32_C(0x04000078),
                                   UINT16_C(0xF000)).status ==
                               GBL_BUS_STATUS_OK &&
                           write16(machine, UINT32_C(0x0400007C),
                                   UINT16_C(0x8000)).status ==
                               GBL_BUS_STATUS_OK &&
                           machine->audio.noise_lfsr == UINT16_C(0x4000) &&
                           machine->audio.noise_cycle_remainder == 0u;
                if (disabled) {
                    const uint64_t first_edge =
                        machine->master_cycles + UINT64_C(32);

                    disabled = gbl_machine_advance_to(machine, first_edge) ==
                                   GBL_TIME_STATUS_OK &&
                               machine->audio.noise_lfsr ==
                                   UINT16_C(0x2000);
                }
                if (disabled) {
                    disabled = write16(machine, UINT32_C(0x0400007C),
                                       UINT16_C(0x8000)).status ==
                                   GBL_BUS_STATUS_OK &&
                               machine->audio.noise_lfsr ==
                                   UINT16_C(0x4000) &&
                               gbl_machine_advance_to(
                                   machine,
                                   machine->master_cycles + UINT64_C(32)) ==
                                   GBL_TIME_STATUS_OK &&
                               machine->audio.noise_lfsr ==
                                   UINT16_C(0x2000);
                }
                resumed = disabled;
            }
            tap_check("DAC disable clears noise state and source output",
                      disabled);
            tap_check("valid resume and retrigger replay the same 15-bit sequence",
                      resumed);
            free_machine(machine);
        }
    }

    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

static int run_psg_sweep(void)
{
    static const uint32_t high_cycles[4] = { 16u, 32u, 64u, 96u };
    unsigned duty;

    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;
        const uint64_t sweep_cycle = UINT64_C(98304);

        if (passed) {
            passed = configure_ch1(machine, 0u, 0u, 15u, false, 0u,
                                   1u, false, 1u, UINT16_C(1024), false);
        }
        tap_check("CH1 sweep setup and trigger are accepted", passed);
        tap_check("CH1 SOUND1CNT_X preserves the declared read mask",
                  passed && read16(machine, UINT32_C(0x04000064)).value ==
                                0u);
        tap_check("CH1 is active before the first step-2 sweep clock",
                  passed && gbl_machine_advance_to(
                                machine, sweep_cycle - 1u) ==
                                GBL_TIME_STATUS_OK &&
                      (gbl_audio_read_register(
                           &machine->audio, UINT32_C(0x04000084)) & 1u) !=
                          0u);
        tap_check("step-2 sweep changes the effective frequency at its exact clock",
                  passed && gbl_machine_advance_to(machine, sweep_cycle) ==
                                GBL_TIME_STATUS_OK &&
                      machine->audio.sweep_frequency == UINT16_C(1536) &&
                      machine->audio.sweep_timer == 0u &&
                      (gbl_audio_read_register(
                           &machine->audio, UINT32_C(0x04000084)) & 1u) !=
                          0u);
        tap_check("step-2 sweep preserves the inclusive high edge minus one",
                  passed && gbl_machine_advance_to(
                                machine, sweep_cycle + 40963u) ==
                                GBL_TIME_STATUS_OK &&
                      gbl_audio_psg_sample(&machine->audio, 0u) == 15u);
        tap_check("step-2 sweep applies addition at the new duty edge",
                  passed && gbl_machine_advance_to(
                                machine, sweep_cycle + 40964u) ==
                                GBL_TIME_STATUS_OK &&
                      gbl_audio_psg_sample(&machine->audio, 0u) == 0u);
        tap_check("step-2 sweep keeps the source low after the edge",
                  passed && gbl_machine_advance_to(
                                machine, sweep_cycle + 40965u) ==
                                GBL_TIME_STATUS_OK &&
                      gbl_audio_psg_sample(&machine->audio, 0u) == 0u &&
                      machine->audio.sweep_frequency == UINT16_C(1536));
        free_machine(machine);
    }

    for (duty = 0u; duty < 4u; ++duty) {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            uint64_t trigger_cycle;
            const uint32_t edge = high_cycles[duty];
            const uint32_t period = 128u;

            passed = configure_ch1(machine, 0u, duty, 15u, false, 0u,
                                   0u, false, 1u, UINT16_C(2047), false);
            trigger_cycle = machine->master_cycles;
            passed = passed &&
                     gbl_machine_advance_to(machine,
                                            trigger_cycle + edge - 1u) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 15u &&
                     gbl_machine_advance_to(machine, trigger_cycle + edge) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 0u &&
                     gbl_machine_advance_to(machine,
                                            trigger_cycle + edge + 1u) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 0u &&
                     gbl_machine_advance_to(machine,
                                            trigger_cycle + period - 1u) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 0u &&
                     gbl_machine_advance_to(machine,
                                            trigger_cycle + period) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 15u &&
                     gbl_machine_advance_to(
                         machine, trigger_cycle + period + edge - 1u) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 15u &&
                     gbl_machine_advance_to(
                         machine, trigger_cycle + period + edge) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 0u &&
                     gbl_machine_advance_to(
                         machine, trigger_cycle + period + edge + 1u) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 0u;
        }
        tap_check("each CH1 duty has edge-minus-one, edge, edge-plus-one, and wrap",
                  passed);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            uint64_t trigger_cycle;

            passed = configure_ch1(machine, 0u, 0u, 15u, false, 0u,
                                   0u, false, 1u, 0u, false);
            trigger_cycle = machine->master_cycles;
            passed = passed &&
                     gbl_machine_advance_to(machine,
                                            trigger_cycle + 32767u) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 15u &&
                     gbl_machine_advance_to(machine,
                                            trigger_cycle + 32768u) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 0u &&
                     gbl_machine_advance_to(
                         machine, trigger_cycle + 32769u) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 0u;
        }
        tap_check("frequency zero uses the longest CH1 period edge", passed);
        free_machine(machine);
    }

    {
        gbl_machine *at_limit = new_machine();
        gbl_machine *over_limit = new_machine();
        bool passed = at_limit != NULL && over_limit != NULL;

        if (passed) {
            passed = configure_ch1(at_limit, 0u, 0u, 15u, false, 0u,
                                   1u, false, 2u, UINT16_C(1638), false) &&
                     configure_ch1(over_limit, 0u, 0u, 15u, false, 0u,
                                   1u, false, 2u, UINT16_C(1639), false) &&
                     gbl_machine_advance_to(at_limit, UINT64_C(98304)) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_machine_advance_to(over_limit,
                                            UINT64_C(98303)) ==
                         GBL_TIME_STATUS_OK &&
                     (gbl_audio_read_register(
                          &over_limit->audio, UINT32_C(0x04000084)) &
                      1u) != 0u &&
                     gbl_machine_advance_to(over_limit,
                                            UINT64_C(98304)) ==
                         GBL_TIME_STATUS_OK &&
                     (gbl_audio_read_register(
                          &over_limit->audio, UINT32_C(0x04000084)) &
                      1u) == 0u &&
                     (gbl_audio_read_register(
                          &at_limit->audio, UINT32_C(0x04000084)) &
                      1u) != 0u &&
                     at_limit->audio.sweep_frequency == UINT16_C(2047) &&
                     over_limit->audio.sweep_frequency == UINT16_C(1639);
        }
        tap_check("addition at 2047 remains active and above it overflows only at step 2",
                  passed);
        free_machine(at_limit);
        free_machine(over_limit);
    }

    {
        gbl_machine *zero_time = new_machine();
        gbl_machine *zero_shift = new_machine();
        bool passed = zero_time != NULL && zero_shift != NULL;

        if (passed) {
            passed = configure_ch1(zero_time, 0u, 0u, 15u, false, 0u,
                                   0u, false, 1u, UINT16_C(2040), false) &&
                     configure_ch1(zero_shift, 0u, 0u, 15u, false, 0u,
                                   1u, false, 0u, UINT16_C(2047), false) &&
                     gbl_machine_advance_to(zero_time,
                                            UINT64_C(98372)) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&zero_time->audio, 0u) == 15u &&
                     gbl_machine_advance_to(zero_shift,
                                            UINT64_C(98324)) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&zero_shift->audio, 0u) == 0u &&
                     zero_shift->audio.sweep_frequency == UINT16_C(2047) &&
                     (read16(zero_shift, UINT32_C(0x04000084)).value & 1u) !=
                         0u;
        }
        tap_check("zero sweep time disables sweep and zero shift leaves frequency unchanged",
                  passed);
        free_machine(zero_time);
        free_machine(zero_shift);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch1(machine, 0u, 0u, 15u, false, 0u,
                                   1u, true, 1u, UINT16_C(2040), false) &&
                     gbl_machine_advance_to(machine,
                                            UINT64_C(98304)) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_machine_advance_to(machine,
                                            UINT64_C(98435)) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 15u &&
                     gbl_machine_advance_to(machine,
                                            UINT64_C(98436)) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_audio_psg_sample(&machine->audio, 0u) == 15u &&
                     machine->audio.sweep_frequency == UINT16_C(1020);
        }
        tap_check("subtract sweep applies on its eligible clock without underflow",
                  passed);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch1(machine, 0u, 0u, 15u, false, 0u,
                                   1u, true, 1u, 0u, false) &&
                     gbl_machine_advance_to(machine, UINT64_C(98303)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.sweep_frequency == 0u &&
                     gbl_machine_advance_to(machine, UINT64_C(98304)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.sweep_frequency == 0u &&
                     (gbl_audio_read_register(
                          &machine->audio, UINT32_C(0x04000084)) & 1u) !=
                         0u;
        }
        tap_check("subtract sweep at frequency zero stays at the lower bound",
                  passed);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch1(machine, 0u, 0u, 15u, false, 0u,
                                   2u, false, 1u, UINT16_C(1024), false) &&
                     gbl_machine_advance_to(machine, UINT64_C(98303)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.sweep_timer == 0u &&
                     machine->audio.sweep_frequency == UINT16_C(1024) &&
                     gbl_machine_advance_to(machine, UINT64_C(98304)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.sweep_timer == 1u &&
                     machine->audio.sweep_frequency == UINT16_C(1024) &&
                     gbl_machine_advance_to(machine,
                                            UINT64_C(229375)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.sweep_timer == 1u &&
                     gbl_machine_advance_to(machine,
                                            UINT64_C(229376)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.sweep_timer == 0u &&
                     machine->audio.sweep_frequency == UINT16_C(1536);
        }
        tap_check("sweep interval two counts step 2 then updates at step 6",
                  passed);
        free_machine(machine);
    }

    {
        gbl_machine *whole = new_machine();
        gbl_machine *split = new_machine();
        bool passed = whole != NULL && split != NULL;

        if (passed) {
            passed = configure_ch1(whole, 0u, 2u, 13u, false, 2u,
                                   2u, false, 1u, UINT16_C(1024), false) &&
                     configure_ch1(split, 0u, 2u, 13u, false, 2u,
                                   2u, false, 1u, UINT16_C(1024), false) &&
                     gbl_machine_advance_to(whole, UINT64_C(300000)) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_machine_advance_to(split, UINT64_C(98304)) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_machine_advance_to(split, UINT64_C(229375)) ==
                         GBL_TIME_STATUS_OK &&
                     gbl_machine_advance_to(split, UINT64_C(300000)) ==
                         GBL_TIME_STATUS_OK &&
                     whole->master_cycles == split->master_cycles &&
                     gbl_audio_state_equal(&whole->audio, &split->audio) &&
                     gbl_audio_psg_sample(&whole->audio, 0u) ==
                         gbl_audio_psg_sample(&split->audio, 0u);
        }
        tap_check("irregular replay preserves CH1 sweep and source state",
                  passed);
        free_machine(whole);
        free_machine(split);
    }

    {
        gbl_machine *machine = new_machine();
        gbl_machine *snapshot = NULL;
        gbl_machine_time_preview preview = { 0 };
        bool prepared = false;
        bool fields_bound = false;
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch1(machine, 0u, 0u, 15u, false, 0u,
                                   2u, false, 1u, UINT16_C(1024), false) &&
                     gbl_machine_advance_to(machine,
                                            UINT64_C(98304)) ==
                         GBL_TIME_STATUS_OK &&
                     snapshot_machine(machine, &snapshot);
        }
        prepared = passed &&
                   gbl_machine_prepare_time(machine,
                                            machine->master_cycles + 1u,
                                            &preview) == GBL_TIME_STATUS_OK;
        if (prepared) {
            preview.audio.sweep_frequency ^= UINT16_C(1);
            fields_bound = !gbl_audio_state_equal(
                &preview.audio, &preview.audio_prepared);
            preview.audio = preview.audio_prepared;
            preview.audio.sweep_period ^= UINT8_C(1);
            fields_bound = fields_bound && !gbl_audio_state_equal(
                                               &preview.audio,
                                               &preview.audio_prepared);
            preview.audio = preview.audio_prepared;
            preview.audio.sweep_timer ^= UINT8_C(1);
            fields_bound = fields_bound && !gbl_audio_state_equal(
                                               &preview.audio,
                                               &preview.audio_prepared);
            preview.audio = preview.audio_prepared;
            preview.audio.sweep_shift ^= UINT8_C(1);
            fields_bound = fields_bound && !gbl_audio_state_equal(
                                               &preview.audio,
                                               &preview.audio_prepared);
            preview.audio = preview.audio_prepared;
            preview.audio.sweep_decrease = !preview.audio.sweep_decrease;
            fields_bound = fields_bound && !gbl_audio_state_equal(
                                               &preview.audio,
                                               &preview.audio_prepared);
            preview.audio = preview.audio_prepared;
            preview.audio.sweep_frequency ^= UINT16_C(1);
            fields_bound = fields_bound &&
                           !gbl_machine_commit_time_checked(machine, &preview) &&
                           snapshot != NULL &&
                           unchanged_machine(machine, snapshot);
        }
        tap_check("all sweep fields bind a prepared preview and mismatch refusal preserves state",
                  prepared && fields_bound);

        prepared = machine != NULL &&
                   gbl_machine_prepare_time(
                       machine, machine->master_cycles + 1u, &preview) ==
                       GBL_TIME_STATUS_OK;
        if (prepared) {
            preview.audio.sweep_frequency = UINT16_C(2048);
            preview.audio_prepared.sweep_frequency = UINT16_C(2048);
        }
        tap_check("out-of-range effective sweep frequency is rejected after matching prepared copies",
                  prepared &&
                      gbl_audio_state_equal(&preview.audio,
                                            &preview.audio_prepared) &&
                      !gbl_audio_state_valid(&preview.audio) &&
                      !gbl_machine_commit_time_checked(machine, &preview) &&
                      snapshot != NULL &&
                      unchanged_machine(machine, snapshot));
        free(snapshot);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch1(machine, 63u, 0u, 14u, true, 1u,
                                   0u, false, 0u, UINT16_C(2047), true) &&
                     machine->audio.length_remaining[0] == 1u &&
                     gbl_machine_advance_to(machine, UINT64_C(32767)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.channel_volume[0] == 14u &&
                     gbl_machine_advance_to(machine, UINT64_C(32768)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.channel_volume[0] == 14u &&
                     machine->audio.length_remaining[0] == 0u &&
                     (read16(machine, UINT32_C(0x04000084)).value & 1u) ==
                         0u;
        }
        tap_check("single-clock length expires CH1 while trigger loads envelope",
                  passed);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        gbl_machine_time_preview span = { 0 };
        size_t staged_bytes = 0u;
        const uint64_t expiry_cycle = UINT64_C(4161536);
        bool configured = machine != NULL;
        bool before_expiry = false;
        bool expired = false;

        if (configured) {
            configured = configure_ch1(machine, 0u, 0u, 15u, false, 0u,
                                       0u, false, 0u, UINT16_C(2047), true);
        }
        tap_check("maximum CH1 length configuration is accepted", configured);
        if (configured) {
            span.audio = machine->audio;
            before_expiry =
                gbl_audio_advance_span(&span, expiry_cycle - 1u, NULL, 0u,
                                       &staged_bytes, NULL, NULL) ==
                    GBL_TIME_STATUS_OK &&
                span.audio.length_remaining[0] == 1u &&
                (gbl_audio_read_register(&span.audio,
                                         UINT32_C(0x04000084)) & 1u) != 0u;
        }
        tap_check("maximum CH1 length remains active one cycle before tick 64",
                  before_expiry);
        if (before_expiry) {
            expired = gbl_audio_advance_span(
                          &span, expiry_cycle, NULL, 0u, &staged_bytes, NULL,
                          NULL) == GBL_TIME_STATUS_OK &&
                      span.audio.length_remaining[0] == 0u &&
                      (gbl_audio_read_register(&span.audio,
                                               UINT32_C(0x04000084)) &
                       1u) == 0u;
        }
        tap_check("maximum CH1 length expires on its 64th eligible clock",
                  expired);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch1(machine, 0u, 0u, 14u, true, 1u,
                                   0u, false, 0u, UINT16_C(2047), false) &&
                     gbl_machine_advance_to(machine, UINT64_C(262143)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.channel_volume[0] == 14u &&
                     gbl_machine_advance_to(machine, UINT64_C(262144)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.channel_volume[0] == 15u &&
                     gbl_machine_advance_to(machine, UINT64_C(1048576)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.channel_volume[0] == 15u;
        }
        tap_check("CH1 envelope advances at its sequencer edge and saturates",
                  passed);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch1(machine, 0u, 0u, 15u, false, 0u,
                                   0u, false, 0u, UINT16_C(2040), false) &&
                     gbl_machine_advance_to(machine, UINT64_C(98304)) ==
                         GBL_TIME_STATUS_OK &&
                     machine->audio.square_phase[0] < 1024u &&
                     machine->audio.sweep_period == 0u &&
                     machine->audio.sweep_shift == 0u &&
                     write16(machine, UINT32_C(0x04000060),
                             UINT16_C(0x0023)).status == GBL_BUS_STATUS_OK &&
                     machine->audio.sweep_period == 0u &&
                     machine->audio.sweep_shift == 0u &&
                     write16(machine, UINT32_C(0x04000064),
                             UINT16_C(0x87F8)).status == GBL_BUS_STATUS_OK &&
                     machine->audio.square_phase[0] == 0u &&
                     machine->audio.channel_volume[0] == 15u &&
                     machine->audio.length_remaining[0] == 64u &&
                     machine->audio.sweep_period == 2u &&
                     machine->audio.sweep_timer == 0u &&
                     machine->audio.sweep_shift == 3u &&
                     !machine->audio.sweep_decrease &&
                     machine->audio.sweep_frequency == UINT16_C(2040);
        }
        tap_check("retrigger reloads CH1 oscillator, envelope, length, and sweep configuration",
                  passed);
        if (passed) {
            passed = write16(machine, UINT32_C(0x04000062), 0u).status ==
                         GBL_BUS_STATUS_OK &&
                     (read16(machine, UINT32_C(0x04000084)).value & 1u) ==
                         0u &&
                     machine->audio.sweep_frequency == 0u &&
                     machine->audio.sweep_period == 0u &&
                     machine->audio.sweep_timer == 0u &&
                     machine->audio.sweep_shift == 0u &&
                     !machine->audio.sweep_decrease;
        }
        tap_check("disabling the CH1 DAC clears activity", passed);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool passed = machine != NULL;

        if (passed) {
            passed = configure_ch2(machine, 0u, 0u, 15u, false, 0u,
                                   UINT16_C(2047), false) &&
                     write16(machine, UINT32_C(0x04000060),
                             UINT16_C(0x0023)).status == GBL_BUS_STATUS_OK &&
                     write16(machine, UINT32_C(0x04000062),
                             UINT16_C(0xF03F)).status == GBL_BUS_STATUS_OK &&
                     write16(machine, UINT32_C(0x04000064),
                             UINT16_C(0xC7FF)).status == GBL_BUS_STATUS_OK &&
                     gbl_machine_advance_to(machine, UINT64_C(32768)) ==
                         GBL_TIME_STATUS_OK &&
                     (read16(machine, UINT32_C(0x04000084)).value & 3u) ==
                         2u &&
                     machine->audio.sweep_frequency == UINT16_C(2047) &&
                     machine->audio.sweep_period == 2u &&
                     machine->audio.sweep_shift == 3u;
        }
        tap_check("CH1-only length expiry keeps CH2 running", passed);
        if (passed) {
            passed = write16(machine, UINT32_C(0x04000084), 0u).status ==
                         GBL_BUS_STATUS_OK &&
                     read16(machine, UINT32_C(0x04000084)).value == 0u &&
                     machine->audio.sweep_frequency == 0u &&
                     machine->audio.sweep_period == 0u &&
                     machine->audio.sweep_timer == 0u &&
                     machine->audio.sweep_shift == 0u &&
                     !machine->audio.sweep_decrease;
        }
        tap_check("master-off disables both active square channels", passed);
        free_machine(machine);
    }

    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

static bool length_boundary_case(uint64_t setup_cycle,
                                 uint64_t expiry_cycle)
{
    gbl_machine *machine = new_machine();
    bool passed = machine != NULL;

    if (passed) {
        passed = gbl_machine_advance_to(machine, setup_cycle) ==
                     GBL_TIME_STATUS_OK &&
                 configure_ch2(machine, UINT8_C(63), 0u, 5u, false, 0u,
                               UINT16_C(2047), true) &&
                 machine->master_cycles == setup_cycle + 3u &&
                 machine->audio.length_remaining[1] == 1u &&
                 (machine->audio.active_channels & UINT8_C(0x02)) != 0u &&
                 gbl_machine_advance_to(machine, expiry_cycle - 1u) ==
                     GBL_TIME_STATUS_OK &&
                 machine->audio.length_remaining[1] == 1u &&
                 (machine->audio.active_channels & UINT8_C(0x02)) != 0u &&
                 gbl_machine_advance_to(machine, expiry_cycle) ==
                     GBL_TIME_STATUS_OK &&
                 machine->audio.length_remaining[1] == 0u &&
                 (machine->audio.active_channels & UINT8_C(0x02)) == 0u;
    }
    free_machine(machine);
    return passed;
}

static int run_psg_sequencer(void)
{
    static const uint64_t step_cycles[8] = {
        UINT64_C(32768), UINT64_C(65536), UINT64_C(98304), UINT64_C(131072),
        UINT64_C(163840), UINT64_C(196608), UINT64_C(229376), UINT64_C(262144)
    };
    static const uint16_t expected_lengths[8] = {
        63u, 63u, 62u, 62u, 61u, 61u, 60u, 60u
    };
    gbl_machine *machine = new_machine();
    size_t step;

    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");
    tap_check("machine is initialized", machine != NULL);
    if (machine == NULL) {
        puts("1..1");
        return 1;
    }

    tap_check("CH2 can be configured with length and envelope clocks",
              configure_ch2(machine, 0u, 0u, 5u, true, 1u,
                            UINT16_C(2047), true));
    tap_check("zero length loads the full 64-clock duration",
              machine->audio.length_remaining[1] == 64u);
    tap_check("trigger latches a one-tick rising envelope",
              machine->audio.envelope_period[1] == 1u &&
                  machine->audio.envelope_timer[1] == 1u &&
                  machine->audio.envelope_increase[1]);

    for (step = 0u; step < 8u; ++step) {
        tap_check("advance reaches the reset-anchored sequencer boundary",
                  gbl_machine_advance_to(machine, step_cycles[step]) ==
                      GBL_TIME_STATUS_OK);
        tap_check("length clocks only on steps zero two four and six",
                  machine->audio.length_remaining[1] ==
                      expected_lengths[step]);
        tap_check("envelope changes only on step seven",
                  machine->audio.channel_volume[1] ==
                      (step == 7u ? UINT8_C(6) : UINT8_C(5)));
    }
    tap_check("sequencer step wrap clocks length on the next step zero",
              gbl_machine_advance_to(machine, UINT64_C(294912)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.length_remaining[1] == 59u &&
                  machine->audio.channel_volume[1] == 6u);
    tap_check("step seven after wrap clocks the next envelope period",
              gbl_machine_advance_to(machine, UINT64_C(524288)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.channel_volume[1] == 7u);
    free_machine(machine);

    machine = new_machine();
    tap_check("zero envelope period configures", machine != NULL &&
              configure_ch2(machine, UINT8_C(63), 0u, 5u, true, 0u,
                            UINT16_C(2047), false));
    if (machine != NULL) {
        tap_check("zero envelope period never changes volume",
                  gbl_machine_advance_to(machine, UINT64_C(524288)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.channel_volume[1] == 5u &&
                  machine->audio.envelope_timer[1] == 0u);
    }
    free_machine(machine);

    machine = new_machine();
    tap_check("multi-clock rising envelope configures", machine != NULL &&
              configure_ch2(machine, UINT8_C(63), 0u, 14u, true, 2u,
                            UINT16_C(2047), false));
    if (machine != NULL) {
        tap_check("rising envelope waits for its second step-seven tick",
                  gbl_machine_advance_to(machine, UINT64_C(262144)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.channel_volume[1] == 14u);
        tap_check("rising envelope saturates at volume fifteen",
                  gbl_machine_advance_to(machine, UINT64_C(524288)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.channel_volume[1] == 15u &&
                  gbl_machine_advance_to(machine, UINT64_C(786432)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.channel_volume[1] == 15u);
    }
    free_machine(machine);

    machine = new_machine();
    tap_check("multi-clock falling envelope configures", machine != NULL &&
              configure_ch2(machine, UINT8_C(63), 0u, 1u, false, 2u,
                            UINT16_C(2047), false));
    if (machine != NULL) {
        tap_check("falling envelope saturates at volume zero",
                  gbl_machine_advance_to(machine, UINT64_C(524288)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.channel_volume[1] == 0u &&
                  gbl_machine_advance_to(machine, UINT64_C(786432)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.channel_volume[1] == 0u);
    }
    free_machine(machine);

    machine = new_machine();
    tap_check("disabled length counter configures", machine != NULL &&
              configure_ch2(machine, UINT8_C(63), 0u, 5u, false, 0u,
                            UINT16_C(2047), false));
    if (machine != NULL) {
        tap_check("disabled length remains loaded across clock steps",
                  gbl_machine_advance_to(machine, UINT64_C(262144)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.length_remaining[1] == 1u &&
                  (machine->audio.active_channels & UINT8_C(0x02)) != 0u);
    }
    free_machine(machine);

    machine = new_machine();
    tap_check("live length-enable case configures", machine != NULL &&
              configure_ch2(machine, UINT8_C(63), 0u, 5u, false, 0u,
                            UINT16_C(2047), false));
    if (machine != NULL) {
        tap_check("length enable remains independently writable",
                  write16(machine, UINT32_C(0x0400006C),
                          UINT16_C(0x47FF)).status == GBL_BUS_STATUS_OK &&
                      gbl_machine_advance_to(machine, UINT64_C(32768)) ==
                          GBL_TIME_STATUS_OK &&
                      machine->audio.length_remaining[1] == 0u &&
                      (machine->audio.active_channels & UINT8_C(0x02)) == 0u);
    }
    free_machine(machine);

    tap_check("trigger before the first eligible boundary expires there",
              length_boundary_case(UINT64_C(32764), UINT64_C(32768)));
    tap_check("trigger on a boundary loads after its completed clock",
              length_boundary_case(UINT64_C(32765), UINT64_C(98304)));
    tap_check("trigger after a boundary waits for the next eligible clock",
              length_boundary_case(UINT64_C(32766), UINT64_C(98304)));

    machine = new_machine();
    tap_check("master-off elapsed cycles preserve the reset anchor",
              machine != NULL &&
                  gbl_machine_advance_to(machine, UINT64_C(32768)) ==
                      GBL_TIME_STATUS_OK &&
                  configure_ch2(machine, UINT8_C(63), 0u, 5u, false, 0u,
                                UINT16_C(2047), true) &&
                  gbl_machine_advance_to(machine, UINT64_C(98303)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.length_remaining[1] == 1u &&
                  gbl_machine_advance_to(machine, UINT64_C(98304)) ==
                      GBL_TIME_STATUS_OK &&
                  machine->audio.length_remaining[1] == 0u &&
                  machine->audio.active_channels == 0u);
    free_machine(machine);

    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

typedef struct psg_order_observation {
    size_t calls;
    uint64_t cycle;
    uint16_t interrupt_flags;
    uint8_t source;
    uint16_t control_before_write;
    bool timer_reloaded;
} psg_order_observation;

static void psg_order_sample(const gbl_machine_time_preview *candidate,
                             uint64_t cycle, void *context, int16_t *left,
                             int16_t *right)
{
    psg_order_observation *observation = context;

    observation->calls++;
    observation->cycle = cycle;
    observation->interrupt_flags = candidate->interrupt_flags;
    observation->source = gbl_audio_psg_sample(&candidate->audio, 1u);
    observation->control_before_write = candidate->audio.registers[4];
    observation->timer_reloaded =
        candidate->timers[0].counter == UINT16_C(0xFFFF);
    *left = 0;
    *right = 0;
}

static int run_psg_order(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;
    gbl_time_operation operation;
    gbl_cycle_cost write_cost = { 0 };
    psg_order_observation observation = { 0 };
    uint8_t staging[GBL_AUDIO_FRAME_BYTES] = {
        UINT8_C(0xA5), UINT8_C(0xA5), UINT8_C(0xA5), UINT8_C(0xA5)
    };
    size_t staged_bytes = 0u;

    CHECK(machine != NULL);
    if (machine == NULL) {
        return CHECK_DONE();
    }
    CHECK(gbl_machine_advance_to(machine, UINT64_C(493)) ==
          GBL_TIME_STATUS_OK);
    CHECK(configure_ch2(machine, 0u, 0u, 15u, false, 0u,
                        UINT16_C(2047), false));
    CHECK(machine->master_cycles == UINT64_C(496));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(509)) ==
          GBL_TIME_STATUS_OK);
    CHECK(write16(machine, UINT32_C(0x04000100), UINT16_C(0xFFFF)).status ==
          GBL_BUS_STATUS_OK);
    CHECK(write16(machine, UINT32_C(0x04000102), UINT16_C(0x00C0)).status ==
          GBL_BUS_STATUS_OK);
    CHECK(machine->master_cycles == UINT64_C(511));
    CHECK(machine->event_count != 0u);
    CHECK(gbl_bus_quote(machine, UINT32_C(0x04000068),
                        GBL_BUS_DIRECTION_WRITE, 2u,
                        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_ROLE_DATA,
                        &write_cost));
    operation = (gbl_time_operation){
        .kind = GBL_TIME_OPERATION_BUS_WRITE,
        .width = 2u,
        .address = UINT32_C(0x04000068),
        .value = UINT32_C(0xF0C0),
        .cycles = write_cost.cycles
    };
    CHECK(gbl_machine_prepare_operations_audio_with_provider(
              machine, &operation, 1u, &preview, staging, sizeof(staging),
              &staged_bytes, psg_order_sample, &observation) ==
          GBL_TIME_STATUS_OK);
    CHECK(observation.calls == 1u);
    CHECK(observation.cycle == UINT64_C(512));
    CHECK(observation.timer_reloaded);
    CHECK((observation.interrupt_flags & UINT16_C(0x0008)) != 0u);
    CHECK(observation.control_before_write == UINT16_C(0xF000));
    CHECK(observation.source == 0u);
    CHECK(staged_bytes == GBL_AUDIO_FRAME_BYTES);
    CHECK(staging[0] == 0u && staging[1] == 0u && staging[2] == 0u &&
          staging[3] == 0u);
    CHECK(machine->master_cycles == UINT64_C(511));
    CHECK(machine->audio.registers[4] == UINT16_C(0xF000));
    CHECK(preview.audio.registers[4] == UINT16_C(0xF0C0));
    CHECK(gbl_machine_commit_time_checked(machine, &preview));
    CHECK(machine->master_cycles == UINT64_C(512));
    CHECK(machine->audio.registers[4] == UINT16_C(0xF0C0));
    CHECK(gbl_audio_psg_sample(&machine->audio, 1u) == UINT8_C(15));
    free_machine(machine);
    return CHECK_DONE();
}

static int run_psg_transaction(void)
{
    gbl_machine *whole = new_machine();
    gbl_machine *split = new_machine();
    gbl_machine *captured = new_machine();
    gbl_machine *captured_split = new_machine();
    gbl_machine *refused = new_machine();
    gbl_machine *in_range_snapshot = NULL;
    gbl_machine *invalid_snapshot = NULL;
    gbl_machine *active_empty_snapshot = NULL;
    gbl_machine *refusal_snapshot = NULL;
    gbl_audio_capture capture;
    gbl_audio_capture capture_before;
    gbl_machine_time_preview preview;
    gbl_machine_time_preview preview_before;
    uint8_t whole_output[32];
    uint8_t split_output[32];
    uint8_t refusal_output[8];
    uint8_t refusal_output_before[sizeof(refusal_output)];
    size_t index;
    const uint64_t target = UINT64_C(4099);

    CHECK(whole != NULL && split != NULL && captured != NULL &&
          captured_split != NULL && refused != NULL);
    if (whole == NULL || split == NULL || captured == NULL ||
        captured_split == NULL || refused == NULL) {
        free_machine(whole);
        free_machine(split);
        free_machine(captured);
        free_machine(captured_split);
        free_machine(refused);
        return CHECK_DONE();
    }
    CHECK(configure_ch2(whole, 0u, 0u, 15u, false, 0u,
                        UINT16_C(2047), false));
    CHECK(configure_ch2(split, 0u, 0u, 15u, false, 0u,
                        UINT16_C(2047), false));
    CHECK(configure_ch2(captured, 0u, 0u, 15u, false, 0u,
                        UINT16_C(2047), false));
    CHECK(configure_ch2(captured_split, 0u, 0u, 15u, false, 0u,
                        UINT16_C(2047), false));
    CHECK(configure_ch2(refused, 0u, 0u, 15u, false, 0u,
                        UINT16_C(2047), false));
    CHECK(gbl_machine_advance_to(whole, target) == GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to(split, UINT64_C(127)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to(split, UINT64_C(512)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to(split, UINT64_C(1024)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to(split, target) == GBL_TIME_STATUS_OK);
    memset(whole_output, UINT8_C(0xA5), sizeof(whole_output));
    memset(split_output, UINT8_C(0xA5), sizeof(split_output));
    CHECK(gbl_audio_capture_begin(captured, &capture, whole_output,
                                  sizeof(whole_output)) == GBL_TIME_STATUS_OK);
    CHECK(gbl_audio_capture_begin(captured_split, &capture_before,
                                  split_output, sizeof(split_output)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(captured, target, &capture) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(captured_split, UINT64_C(512),
                                      &capture_before) == GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(captured_split, UINT64_C(2048),
                                      &capture_before) == GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to_audio(captured_split, target,
                                      &capture_before) == GBL_TIME_STATUS_OK);
    CHECK(capture.bytes_written == 32u &&
          capture_before.bytes_written == 32u);
    CHECK(memcmp(whole_output, split_output, 32u) == 0);
    for (index = 0u; index < 32u; ++index) {
        CHECK(whole_output[index] == 0u);
    }
    CHECK(gbl_audio_state_equal(&whole->audio, &split->audio));
    CHECK(gbl_audio_state_equal(&whole->audio, &captured->audio));
    CHECK(gbl_audio_state_equal(&whole->audio, &captured_split->audio));

    memset(refusal_output, UINT8_C(0xD7), sizeof(refusal_output));
    memcpy(refusal_output_before, refusal_output, sizeof(refusal_output));
    CHECK(gbl_audio_capture_begin(refused, &capture, refusal_output, 4u) ==
          GBL_TIME_STATUS_OK);
    CHECK(snapshot_machine(refused, &refusal_snapshot));
    capture_before = capture;
    CHECK(gbl_machine_advance_to_audio(refused, UINT64_C(1024), &capture) ==
          GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT);
    CHECK(unchanged_machine(refused, refusal_snapshot));
    CHECK(memcmp(&capture, &capture_before, sizeof(capture)) == 0);
    CHECK(memcmp(refusal_output, refusal_output_before,
                 sizeof(refusal_output)) == 0);
    memset(&preview, UINT8_C(0xC3), sizeof(preview));
    preview_before = preview;
    CHECK(gbl_machine_prepare_time(refused, refused->master_cycles - 1u,
                                   &preview) == GBL_TIME_STATUS_BACKWARD);
    CHECK(memcmp(&preview, &preview_before, sizeof(preview)) == 0);
    CHECK(unchanged_machine(refused, refusal_snapshot));

    CHECK(snapshot_machine(refused, &in_range_snapshot));
    CHECK(gbl_machine_prepare_time(refused, refused->master_cycles + 16u,
                                   &preview) == GBL_TIME_STATUS_OK);
    preview.audio.length_remaining[1] = 63u;
    CHECK(gbl_audio_state_valid(&preview.audio));
    CHECK(!gbl_machine_commit_time_checked(refused, &preview));
    CHECK(unchanged_machine(refused, in_range_snapshot));

    CHECK(snapshot_machine(refused, &invalid_snapshot));
    CHECK(gbl_machine_prepare_time(refused, refused->master_cycles + 16u,
                                   &preview) == GBL_TIME_STATUS_OK);
    preview.audio.length_remaining[1] = 65u;
    CHECK(!gbl_audio_state_valid(&preview.audio));
    CHECK(!gbl_machine_commit_time_checked(refused, &preview));
    CHECK(unchanged_machine(refused, invalid_snapshot));

    CHECK(snapshot_machine(refused, &active_empty_snapshot));
    CHECK(gbl_machine_prepare_time(refused, refused->master_cycles + 16u,
                                   &preview) == GBL_TIME_STATUS_OK);
    preview.audio.length_remaining[1] = 0u;
    preview.audio_prepared.length_remaining[1] = 0u;
    CHECK(gbl_audio_state_equal(&preview.audio, &preview.audio_prepared));
    CHECK(!gbl_audio_state_valid(&preview.audio));
    CHECK(!gbl_machine_commit_time_checked(refused, &preview));
    CHECK(unchanged_machine(refused, active_empty_snapshot));

    {
        gbl_machine *near_limit = new_machine();
        gbl_machine_time_preview far_preview = { 0 };
        size_t far_staged_bytes = 0u;

        CHECK(near_limit != NULL);
        if (near_limit != NULL) {
            CHECK(configure_ch2(near_limit, UINT8_C(63), 0u, 5u, false, 0u,
                                UINT16_C(2047), true));
            far_preview.audio = near_limit->audio;
            far_preview.audio.cycle_anchor = UINT64_MAX - UINT64_C(1);
            CHECK(gbl_audio_advance_span(
                      &far_preview, UINT64_MAX, NULL, 0u, &far_staged_bytes,
                      NULL, NULL) == GBL_TIME_STATUS_OK);
            CHECK(far_preview.audio.cycle_anchor == UINT64_MAX);
            CHECK(far_preview.audio.length_remaining[1] == 1u);
            CHECK(far_preview.audio.square_phase[1] == 1u);
            CHECK(gbl_audio_state_valid(&far_preview.audio));

            far_preview.audio = near_limit->audio;
            far_staged_bytes = 0u;
            CHECK(gbl_audio_advance_span(
                      &far_preview, UINT64_MAX, NULL, 0u, &far_staged_bytes,
                      NULL, NULL) == GBL_TIME_STATUS_OK);
            CHECK(far_preview.audio.cycle_anchor == UINT64_MAX);
            CHECK(far_preview.audio.length_remaining[1] == 0u);
            CHECK(far_preview.audio.active_channels == 0u);
            CHECK(gbl_audio_state_valid(&far_preview.audio));
        }
        free_machine(near_limit);
    }

    free(refusal_snapshot);
    free(in_range_snapshot);
    free(invalid_snapshot);
    free(active_empty_snapshot);
    free_machine(whole);
    free_machine(split);
    free_machine(captured);
    free_machine(captured_split);
    free_machine(refused);
    return CHECK_DONE();
}

static int run_psg_validation(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *snapshot = NULL;
    gbl_machine_time_preview preview = { 0 };
    bool prepared = false;

    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");
    tap_check("machine is initialized", machine != NULL);
    if (machine == NULL) {
        puts("1..1");
        return 1;
    }
    tap_check("active CH2 is configured with a positive length",
              configure_ch2(machine, 0u, 0u, 5u, false, 0u,
                            UINT16_C(2047), true) &&
                  machine->audio.length_remaining[1] == 64u);
    tap_check("machine snapshot is available",
              snapshot_machine(machine, &snapshot));
    prepared = gbl_machine_prepare_time(machine, machine->master_cycles + 16u,
                                        &preview) == GBL_TIME_STATUS_OK;
    tap_check("valid time preview is prepared", prepared);
    if (prepared) {
        preview.audio.length_remaining[1] = 0u;
        preview.audio_prepared.length_remaining[1] = 0u;
        tap_check("forged candidate still matches its prepared copy",
                  gbl_audio_state_equal(&preview.audio,
                                        &preview.audio_prepared));
        tap_check("active channel with empty length is invalid",
                  !gbl_audio_state_valid(&preview.audio));
        tap_check("checked commit refuses the impossible state",
                  !gbl_machine_commit_time_checked(machine, &preview));
        tap_check("refused commit preserves the machine exactly",
                  snapshot != NULL && unchanged_machine(machine, snapshot));
    }
    free(snapshot);
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
    uint64_t trigger_cycle;
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
    CHECK(configure_ch2(machine, 0u, 0u, 15u, false, 0u,
                        UINT16_C(2047), false));
    trigger_cycle = machine->master_cycles;
    CHECK(gbl_audio_psg_sample(&machine->audio, 1u) == UINT8_C(15));
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
    CHECK(machine->cpu.r0_7[2] == UINT32_C(0x0082));
    CHECK(machine->master_cycles > trigger_cycle);
    CHECK(machine->audio.square_phase[1] ==
          (uint32_t)((machine->master_cycles - trigger_cycle) % UINT64_C(128)));
    CHECK(gbl_audio_psg_sample(&machine->audio, 1u) ==
          (machine->audio.square_phase[1] < 16u ? UINT8_C(15) : 0u));
    CHECK(read16(machine, UINT32_C(0x04000084)).value == UINT16_C(0x0082));
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
    if (strcmp(argv[1], "psg_sweep") == 0) {
        return run_psg_sweep();
    }
    if (strcmp(argv[1], "psg_noise") == 0) {
        return run_psg_noise();
    }
    if (strcmp(argv[1], "psg_wave") == 0) {
        return run_psg_wave();
    }
    if (strcmp(argv[1], "psg_sequencer") == 0) {
        return run_psg_sequencer();
    }
    if (strcmp(argv[1], "psg_order") == 0) {
        return run_psg_order();
    }
    if (strcmp(argv[1], "psg_transaction") == 0) {
        return run_psg_transaction();
    }
    if (strcmp(argv[1], "psg_validation") == 0) {
        return run_psg_validation();
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
