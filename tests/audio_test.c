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
    bool configured;

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
    configured = configure_wave(machine, 0u, false, UINT16_C(0x2000), 0u,
                                UINT16_C(2047), false);
    trigger_cycle = machine->master_cycles;
    tap_check("selected 32-step front bank triggers at phase zero",
              configured &&
                  (read16(machine, UINT32_C(0x04000084)).value & 7u) == 4u &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 3u);
    tap_check("manual whole-pattern rate uses a four-cycle nibble step",
              gbl_machine_advance_to(machine, trigger_cycle + 3u) ==
                      GBL_TIME_STATUS_OK &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 3u &&
                  gbl_machine_advance_to(machine, trigger_cycle + 4u) ==
                      GBL_TIME_STATUS_OK &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 2u);
    tap_check("32-step playback wraps to its selected front bank",
              gbl_machine_advance_to(machine, trigger_cycle + 127u) ==
                      GBL_TIME_STATUS_OK &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 2u &&
                  gbl_machine_advance_to(machine, trigger_cycle + 128u) ==
                      GBL_TIME_STATUS_OK &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 3u &&
                  gbl_machine_advance_to(machine, trigger_cycle + 129u) ==
                      GBL_TIME_STATUS_OK &&
                  gbl_audio_psg_sample(&machine->audio, 2u) == 3u);
    free_machine(machine);

    {
        static const uint16_t volume_controls[5] = {
            UINT16_C(0x0000), UINT16_C(0x2000), UINT16_C(0x4000),
            UINT16_C(0x6000), UINT16_C(0xE000)
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
                    gbl_machine_advance_to(probe, anchor + 124u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 2u &&
                    gbl_machine_advance_to(probe, anchor + 127u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 2u &&
                    gbl_machine_advance_to(probe, anchor + 128u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 5u &&
                    gbl_machine_advance_to(probe, anchor + 132u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 4u &&
                    gbl_machine_advance_to(probe, anchor + 252u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 4u &&
                    gbl_machine_advance_to(probe, anchor + 255u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 4u &&
                    gbl_machine_advance_to(probe, anchor + 256u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 3u &&
                    gbl_machine_advance_to(probe, anchor + 260u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 2u;
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
                    gbl_machine_advance_to(probe, anchor + 512u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 5u;
        }
        tap_check("CH3 uses eight-cycle steps at frequency 2046 and bank 1",
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
                    configure_wave(probe, 0u, false, UINT16_C(0x2000), 0u,
                                   UINT16_C(2046), false);
            anchor = probe->master_cycles;
            valid = valid &&
                    gbl_machine_advance_to(probe, anchor + 3u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 3u &&
                    write16(probe, UINT32_C(0x04000074),
                            UINT16_C(0x07FF)).status == GBL_BUS_STATUS_OK &&
                    probe->audio.wave_cycle_remainder == 0u &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 3u &&
                    gbl_machine_advance_to(probe, anchor + 7u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 3u &&
                    gbl_machine_advance_to(probe, anchor + 8u) ==
                        GBL_TIME_STATUS_OK &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 2u;
        }
        tap_check("live frequency normalizes fractional nibble phase",
                  valid);
        free_machine(probe);
    }

    {
        gbl_machine *probe = new_machine();
        uint8_t bank1_first = 0u;
        uint8_t bank0_first = 0u;
        bool valid = probe != NULL;

        if (valid) {
            valid = load_wave_bank(probe, 0u, UINT8_C(0x32)) &&
                    load_wave_bank(probe, 1u, UINT8_C(0x54)) &&
                    configure_wave(probe, 0u, false, UINT16_C(0x2000), 0u,
                                   UINT16_C(2047), false) &&
                    gbl_machine_advance_to(probe,
                                           probe->master_cycles + 24u) ==
                        GBL_TIME_STATUS_OK &&
                    write16(probe, UINT32_C(0x04000074),
                            UINT16_C(0xFFFF)).status == GBL_BUS_STATUS_OK &&
                    probe->audio.wave_phase == 0u &&
                    gbl_audio_psg_sample(&probe->audio, 2u) == 3u &&
                    gbl_audio_wave_read_byte(&probe->audio,
                                             UINT32_C(0x04000090),
                                             &bank1_first) &&
                    bank1_first == UINT8_C(0x54) &&
                    write16(probe, UINT32_C(0x04000070),
                            UINT16_C(0x00C0)).status == GBL_BUS_STATUS_OK &&
                    gbl_audio_wave_read_byte(&probe->audio,
                                             UINT32_C(0x04000090),
                                             &bank0_first) &&
                    bank0_first == UINT8_C(0x32) &&
                    write16(probe, UINT32_C(0x04000084), 0u).status ==
                        GBL_BUS_STATUS_OK &&
                    probe->audio.active_channels == 0u &&
                    probe->audio.registers[8] == 0u &&
                    probe->audio.wave_ram[0][0] == UINT8_C(0x32) &&
                    probe->audio.wave_ram[1][0] == UINT8_C(0x54);
            if (valid) {
                gbl_machine_reset(probe);
                valid = probe->audio.wave_ram[0][0] == 0u &&
                        probe->audio.wave_ram[1][0] == 0u;
            }
        }
        tap_check("retrigger and master-off preserve Wave RAM; reset clears it",
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

    {
        gbl_machine *probe = new_machine();
        bool valid = probe != NULL;

        if (valid) {
            valid = load_wave_bank(probe, 0u, UINT8_C(0x32)) &&
                    load_wave_bank(probe, 1u, UINT8_C(0x54)) &&
                    configure_wave(probe, 0u, false, UINT16_C(0x2000), 0u,
                                   UINT16_C(2047), true) &&
                    probe->audio.length_remaining[2] == UINT16_C(256) &&
                    gbl_machine_advance_to(probe, UINT64_C(32768)) ==
                        GBL_TIME_STATUS_OK &&
                    probe->audio.length_remaining[2] == UINT16_C(255) &&
                    (probe->audio.active_channels & UINT8_C(0x04)) != 0u;
        }
        tap_check("CH3 zero length setting loads all 256 counts", valid);
        free_machine(probe);
    }

    {
        gbl_machine *probe = new_machine();
        bool valid = probe != NULL;

        if (valid) {
            valid = load_wave_bank(probe, 0u, UINT8_C(0x32)) &&
                    load_wave_bank(probe, 1u, UINT8_C(0x54)) &&
                    configure_wave(probe, 0u, false, UINT16_C(0x2000), 255u,
                                   UINT16_C(2047), false) &&
                    gbl_machine_advance_to(probe, UINT64_C(327680)) ==
                        GBL_TIME_STATUS_OK &&
                    probe->audio.length_remaining[2] == 1u &&
                    (probe->audio.active_channels & UINT8_C(0x04)) != 0u;
        }
        tap_check("disabled CH3 length remains continuous across sequencer clocks",
                  valid);
        free_machine(probe);
    }

    {
        gbl_machine *probe = new_machine();
        uint8_t guest_byte = 0u;
        bool valid = probe != NULL;

        if (valid) {
            valid = load_wave_bank(probe, 0u, UINT8_C(0x32)) &&
                    load_wave_bank(probe, 1u, UINT8_C(0x54)) &&
                    configure_wave(probe, 0u, false, UINT16_C(0x2000), 0u,
                                   UINT16_C(2047), false) &&
                    gbl_machine_advance_to(probe,
                                           probe->master_cycles + 12u) ==
                        GBL_TIME_STATUS_OK &&
                    write16(probe, UINT32_C(0x04000070),
                            UINT16_C(0x0000)).status == GBL_BUS_STATUS_OK &&
                    (probe->audio.active_channels & UINT8_C(0x04)) == 0u &&
                    probe->audio.wave_phase == 0u &&
                    gbl_audio_wave_read_byte(&probe->audio,
                                             UINT32_C(0x04000090),
                                             &guest_byte) &&
                    guest_byte == UINT8_C(0x54) &&
                    gbl_audio_wave_write(&probe->audio,
                                         UINT32_C(0x04000090), 1u,
                                         UINT32_C(0xA7)) &&
                    probe->audio.wave_ram[1][0] == UINT8_C(0xA7);
        }
        tap_check("DAC-off clears CH3 while keeping opposite-bank Wave access",
                  valid);
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

static bool capture_mix_ch2_route(uint16_t route, uint8_t output[4])
{
    gbl_machine *machine = new_machine();
    gbl_audio_capture capture;
    bool ready = machine != NULL;

    if (ready) {
        ready = configure_ch2(machine, 0u, 3u, 15u, false, 0u,
                              UINT16_C(2040), false) &&
                write16(machine, UINT32_C(0x04000080), route).status ==
                    GBL_BUS_STATUS_OK &&
                write16(machine, UINT32_C(0x04000082), UINT16_C(0x0002))
                        .status == GBL_BUS_STATUS_OK &&
                gbl_audio_psg_sample(&machine->audio, 1u) == UINT8_C(15) &&
                gbl_audio_capture_begin(machine, &capture, output, 4u) ==
                    GBL_TIME_STATUS_OK &&
                gbl_machine_advance_to_audio(machine, UINT64_C(512),
                                             &capture) ==
                    GBL_TIME_STATUS_OK &&
                capture.bytes_written == GBL_AUDIO_FRAME_BYTES &&
                machine->master_cycles == UINT64_C(512);
    }
    free_machine(machine);
    return ready;
}

static int run_mix_tracer(void)
{
    static const uint8_t left_expected[4] = {
        UINT8_C(0x00), UINT8_C(0x78), UINT8_C(0x00), UINT8_C(0x00)
    };
    static const uint8_t right_expected[4] = {
        UINT8_C(0x00), UINT8_C(0x00), UINT8_C(0x00), UINT8_C(0x78)
    };
    uint8_t left_output[4] = { 0u };
    uint8_t right_output[4] = { 0u };
    const bool left_ready = capture_mix_ch2_route(UINT16_C(0x2070),
                                                  left_output);
    const bool right_ready = capture_mix_ch2_route(UINT16_C(0x0207),
                                                   right_output);

    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");
    tap_check("left-route guest writes reach the on-grid audio capture",
              left_ready);
    tap_check("left-only CH2 route emits literal S16LE frame 00 78 00 00",
              left_ready && memcmp(left_output, left_expected,
                                   sizeof(left_expected)) == 0);
    tap_check("right-only CH2 route emits literal S16LE frame 00 00 00 78",
              right_ready && memcmp(right_output, right_expected,
                                    sizeof(right_expected)) == 0);
    tap_check("reversing the route changes the literal stereo frame",
              left_ready && right_ready &&
                  memcmp(left_output, right_output,
                         sizeof(left_output)) != 0);
    printf("# left frame: %02X %02X %02X %02X\n", left_output[0],
           left_output[1], left_output[2], left_output[3]);
    printf("# right frame: %02X %02X %02X %02X\n", right_output[0],
           right_output[1], right_output[2], right_output[3]);
    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

typedef struct mix_vector {
    const char *name;
    uint8_t psg_channels;
    uint8_t psg_value;
    uint8_t psg_left_routes;
    uint8_t psg_right_routes;
    uint8_t psg_left_level;
    uint8_t psg_right_level;
    uint8_t psg_ratio;
    uint8_t fifo_left_routes;
    uint8_t fifo_right_routes;
    uint8_t fifo_full_gain;
    int8_t fifo_sample[2];
    bool master_enabled;
    uint8_t expected[GBL_AUDIO_FRAME_BYTES];
} mix_vector;

static void mix_vector_set_psg_source(gbl_audio_state *state,
                                      unsigned channel, uint8_t value)
{
    state->active_channels |= (uint8_t)(UINT8_C(1) << channel);
    state->channel_volume[channel] = value;
    if (channel < 2u) {
        state->square_high[channel] = true;
    } else if (channel == 2u) {
        state->registers[8] = UINT16_C(0x0080);
        state->registers[9] = UINT16_C(0x2000);
        state->wave_ram[0][0] = (uint8_t)(value << 4u);
        state->wave_phase = 0u;
    } else {
        state->noise_lfsr = value == 0u ? UINT16_C(1) : UINT16_C(0);
    }
}

static void mix_vector_prepare(gbl_audio_state *state,
                               const mix_vector *vector)
{
    unsigned channel;
    unsigned fifo_index;

    memset(state, 0, sizeof(*state));
    state->registers[18] = vector->master_enabled ? UINT16_C(0x0080) : 0u;
    state->registers[16] =
        (uint16_t)(((uint16_t)vector->psg_left_level << 4u) |
                   (uint16_t)vector->psg_right_level);
    state->registers[17] = (uint16_t)(vector->psg_ratio & UINT8_C(3));
    for (channel = 0u; channel < 4u; ++channel) {
        const uint8_t channel_mask = (uint8_t)(UINT8_C(1) << channel);

        if ((vector->psg_channels & channel_mask) != 0u) {
            mix_vector_set_psg_source(state, channel, vector->psg_value);
        }
        if ((vector->psg_left_routes & channel_mask) != 0u) {
            state->registers[16] |=
                (uint16_t)(UINT16_C(1) << (12u + channel));
        }
        if ((vector->psg_right_routes & channel_mask) != 0u) {
            state->registers[16] |=
                (uint16_t)(UINT16_C(1) << (8u + channel));
        }
    }
    for (fifo_index = 0u; fifo_index < 2u; ++fifo_index) {
        const uint8_t fifo_mask = (uint8_t)(UINT8_C(1) << fifo_index);

        state->fifo[fifo_index].sample = vector->fifo_sample[fifo_index];
        if ((vector->fifo_left_routes & fifo_mask) != 0u) {
            state->registers[17] |=
                (uint16_t)(UINT16_C(1) << (9u + fifo_index * 4u));
        }
        if ((vector->fifo_right_routes & fifo_mask) != 0u) {
            state->registers[17] |=
                (uint16_t)(UINT16_C(1) << (8u + fifo_index * 4u));
        }
        if ((vector->fifo_full_gain & fifo_mask) != 0u) {
            state->registers[17] |=
                (uint16_t)(UINT16_C(1) << (2u + fifo_index));
        }
    }
}

static const mix_vector mix_vectors[] = {
    { .name = "CH1 routes left at full PSG level",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 7u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x00u, 0x78u, 0x00u, 0x00u } },
    { .name = "CH2 routes right at full PSG level",
      .psg_channels = UINT8_C(0x02), .psg_value = 15u,
      .psg_right_routes = UINT8_C(0x02), .psg_right_level = 7u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x00u, 0x00u, 0x00u, 0x78u } },
    { .name = "CH3 routes left at full PSG level",
      .psg_channels = UINT8_C(0x04), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x04), .psg_left_level = 7u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x00u, 0x78u, 0x00u, 0x00u } },
    { .name = "CH4 routes right at full PSG level",
      .psg_channels = UINT8_C(0x08), .psg_value = 15u,
      .psg_right_routes = UINT8_C(0x08), .psg_right_level = 7u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x00u, 0x00u, 0x00u, 0x78u } },
    { .name = "routed PSG zero level is centered below zero",
      .psg_channels = UINT8_C(0x01), .psg_value = 0u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 7u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x00u, 0x88u, 0x00u, 0x00u } },
    { .name = "unrouted active PSG source contributes no signal",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_level = 7u, .psg_right_level = 7u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x00u, 0x00u, 0x00u, 0x00u } },
    { .name = "left and right routes use independent side levels",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01),
      .psg_right_routes = UINT8_C(0x01), .psg_left_level = 7u,
      .psg_right_level = 3u, .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x00u, 0x78u, 0x6Eu, 0x33u } },
    { .name = "zero PSG side levels silence routed sources",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01),
      .psg_right_routes = UINT8_C(0x01), .psg_ratio = 2u,
      .master_enabled = true,
      .expected = { 0x00u, 0x00u, 0x00u, 0x00u } },
    { .name = "PSG ratio code 0 applies one-quarter policy",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 7u,
      .psg_ratio = 0u, .master_enabled = true,
      .expected = { 0x00u, 0x1Eu, 0x00u, 0x00u } },
    { .name = "PSG ratio code 1 applies one-half policy",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 7u,
      .psg_ratio = 1u, .master_enabled = true,
      .expected = { 0x00u, 0x3Cu, 0x00u, 0x00u } },
    { .name = "PSG ratio code 2 applies full policy",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 7u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x00u, 0x78u, 0x00u, 0x00u } },
    { .name = "reserved PSG ratio code 3 fails closed",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 7u,
      .psg_ratio = 3u, .master_enabled = true,
      .expected = { 0x00u, 0x00u, 0x00u, 0x00u } },
    { .name = "reserved PSG ratio leaves Direct Sound routes active",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 7u,
      .psg_ratio = 3u, .fifo_right_routes = UINT8_C(0x01),
      .fifo_full_gain = UINT8_C(0x01), .fifo_sample = { 1, 0 },
      .master_enabled = true,
      .expected = { 0x00u, 0x00u, 0x00u, 0x01u } },
    { .name = "FIFO A full gain maps signed negative S8 to left S16",
      .fifo_left_routes = UINT8_C(0x01),
      .fifo_full_gain = UINT8_C(0x01), .fifo_sample = { -128, 0 },
      .master_enabled = true,
      .expected = { 0x00u, 0x80u, 0x00u, 0x00u } },
    { .name = "FIFO A half gain maps signed negative S8 to left S16",
      .fifo_left_routes = UINT8_C(0x01),
      .fifo_sample = { -128, 0 }, .master_enabled = true,
      .expected = { 0x00u, 0xC0u, 0x00u, 0x00u } },
    { .name = "FIFO A full gain maps positive S8 to left S16",
      .fifo_left_routes = UINT8_C(0x01),
      .fifo_full_gain = UINT8_C(0x01), .fifo_sample = { 127, 0 },
      .master_enabled = true,
      .expected = { 0x00u, 0x7Fu, 0x00u, 0x00u } },
    { .name = "FIFO B independently routes positive S8 to right S16",
      .fifo_right_routes = UINT8_C(0x02),
      .fifo_full_gain = UINT8_C(0x02), .fifo_sample = { 0, 127 },
      .master_enabled = true,
      .expected = { 0x00u, 0x00u, 0x00u, 0x7Fu } },
    { .name = "FIFO A full and FIFO B half gain are independent",
      .fifo_left_routes = UINT8_C(0x01),
      .fifo_right_routes = UINT8_C(0x02),
      .fifo_full_gain = UINT8_C(0x01), .fifo_sample = { 1, 1 },
      .master_enabled = true,
      .expected = { 0x00u, 0x01u, 0x80u, 0x00u } },
    { .name = "FIFO A half and FIFO B full gain are independent",
      .fifo_left_routes = UINT8_C(0x01),
      .fifo_right_routes = UINT8_C(0x02),
      .fifo_full_gain = UINT8_C(0x02), .fifo_sample = { 1, 1 },
      .master_enabled = true,
      .expected = { 0x80u, 0x00u, 0x00u, 0x01u } },
    { .name = "master disable silences PSG and Direct Sound",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 7u,
      .psg_ratio = 2u, .fifo_left_routes = UINT8_C(0x01),
      .fifo_full_gain = UINT8_C(0x01), .fifo_sample = { 127, 0 },
      .master_enabled = false,
      .expected = { 0x00u, 0x00u, 0x00u, 0x00u } },
    { .name = "positive PSG sum saturates at signed S16 maximum",
      .psg_channels = UINT8_C(0x0F), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x0F), .psg_left_level = 7u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0xFFu, 0x7Fu, 0x00u, 0x00u } },
    { .name = "negative PSG sum saturates at signed S16 minimum",
      .psg_channels = UINT8_C(0x0F), .psg_value = 0u,
      .psg_left_routes = UINT8_C(0x0F), .psg_left_level = 7u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x00u, 0x80u, 0x00u, 0x00u } },
    { .name = "positive fractional numerator rounds down to nearest",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 1u,
      .psg_ratio = 1u, .master_enabled = true,
      .expected = { 0x92u, 0x08u, 0x00u, 0x00u } },
    { .name = "positive fractional numerator rounds up to nearest",
      .psg_channels = UINT8_C(0x01), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 1u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0x25u, 0x11u, 0x00u, 0x00u } },
    { .name = "negative fractional numerator rounds toward nearest zero",
      .psg_channels = UINT8_C(0x01), .psg_value = 0u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 1u,
      .psg_ratio = 1u, .master_enabled = true,
      .expected = { 0x6Eu, 0xF7u, 0x00u, 0x00u } },
    { .name = "negative fractional numerator rounds away from zero",
      .psg_channels = UINT8_C(0x01), .psg_value = 0u,
      .psg_left_routes = UINT8_C(0x01), .psg_left_level = 1u,
      .psg_ratio = 2u, .master_enabled = true,
      .expected = { 0xDBu, 0xEEu, 0x00u, 0x00u } },
    { .name = "combined fractional sources round once after summation",
      .psg_channels = UINT8_C(0x03), .psg_value = 15u,
      .psg_left_routes = UINT8_C(0x03), .psg_left_level = 1u,
      .psg_ratio = 1u, .master_enabled = true,
      .expected = { 0x25u, 0x11u, 0x00u, 0x00u } }
};

static bool mix_fifo_byte_vector(unsigned fifo_index, uint32_t fifo_byte,
                                 uint16_t soundcnt_h,
                                 const uint8_t expected[4])
{
    gbl_machine *machine = new_machine();
    uint8_t frame[GBL_AUDIO_FRAME_BYTES] = { 0u };
    bool valid = machine != NULL;

    if (valid) {
        const uint32_t fifo_address =
            GBL_AUDIO_FIFO_FIRST + (uint32_t)(fifo_index * 4u);
        valid = fifo_index < 2u &&
                write16(machine, UINT32_C(0x04000084),
                        UINT16_C(0x0080)).status == GBL_BUS_STATUS_OK &&
                write16(machine, UINT32_C(0x04000082), soundcnt_h).status ==
                    GBL_BUS_STATUS_OK &&
                gbl_audio_fifo_write(&machine->audio, fifo_address, 4u,
                                     fifo_byte) &&
                gbl_audio_fifo_clock_timer(&machine->audio, 0u) == 0u;
        if (valid) {
            gbl_audio_write_frame(&machine->audio, UINT64_C(512), frame);
        }
        valid = valid && memcmp(frame, expected, sizeof(frame)) == 0;
    }
    free_machine(machine);
    return valid;
}

static int run_mix_vectors(void)
{
    static const uint8_t fifo_a_negative[4] = {
        0x00u, 0x80u, 0x00u, 0x00u
    };
    static const uint8_t fifo_b_positive[4] = {
        0x00u, 0x00u, 0x00u, 0x7Fu
    };
    size_t index;
    bool source_values_valid = true;

    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");
    for (index = 0u; index < 4u; ++index) {
        unsigned value_index;

        for (value_index = 0u; value_index < 2u; ++value_index) {
            const uint8_t value = value_index == 0u ? 0u : UINT8_C(15);
            gbl_audio_state state = { 0 };

            mix_vector_set_psg_source(&state, (unsigned)index, value);
            source_values_valid =
                source_values_valid &&
                gbl_audio_psg_sample(&state, (unsigned)index) == value;
        }
    }
    tap_check("all four PSG projections expose literal zero and fifteen inputs",
              source_values_valid);

    /* Every PSG numerator is divisible by four because 2048 mod 28 is 8;
     * each Direct Sound numerator is divisible by 28. The accumulated
     * remainder can therefore never be the half-denominator value 14. */
    for (index = 0u; index <
                    sizeof(mix_vectors) / sizeof(mix_vectors[0]);
         ++index) {
        gbl_audio_state state = { 0 };
        uint8_t frame[GBL_AUDIO_FRAME_BYTES] = { 0u };
        bool sources_valid = true;
        unsigned channel;

        mix_vector_prepare(&state, &mix_vectors[index]);
        for (channel = 0u; channel < 4u; ++channel) {
            const uint8_t channel_mask = (uint8_t)(UINT8_C(1) << channel);

            if ((mix_vectors[index].psg_channels & channel_mask) != 0u) {
                sources_valid =
                    sources_valid &&
                    gbl_audio_psg_sample(&state, channel) ==
                        mix_vectors[index].psg_value;
            }
        }
        gbl_audio_write_frame(&state, UINT64_C(512), frame);
        tap_check(mix_vectors[index].name,
                  sources_valid &&
                      memcmp(frame, mix_vectors[index].expected,
                             sizeof(frame)) == 0);
        if (memcmp(frame, mix_vectors[index].expected, sizeof(frame)) != 0) {
            printf("# actual frame: %02X %02X %02X %02X\n", frame[0],
                   frame[1], frame[2], frame[3]);
        }
    }

    tap_check("FIFO byte 0x80 latches signed negative 128",
              mix_fifo_byte_vector(0u, UINT32_C(0x00000080),
                                   UINT16_C(0x0206), fifo_a_negative));
    tap_check("FIFO B byte 0x7F routes to the independent right side",
              mix_fifo_byte_vector(1u, UINT32_C(0x0000007F),
                                   UINT16_C(0x100B), fifo_b_positive));
    {
        gbl_audio_state state = { 0 };
        int16_t left = INT16_C(1);
        int16_t right = INT16_C(1);

        mix_vector_prepare(&state, &mix_vectors[0]);
        gbl_audio_sample(&state, UINT64_C(512), &left, NULL);
        gbl_audio_sample(&state, UINT64_C(512), NULL, &right);
        gbl_audio_sample(NULL, UINT64_C(512), NULL, NULL);
        tap_check("audio sample keeps nullable mono output pointers",
                  left == INT16_C(30720) && right == 0);
    }
    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

static bool configure_mix_bias_machine(gbl_machine *machine)
{
    return configure_ch2(machine, 0u, 3u, 15u, false, 0u,
                         UINT16_C(2037), false) &&
           write16(machine, UINT32_C(0x04000080), UINT16_C(0x2070)).status ==
               GBL_BUS_STATUS_OK &&
           write16(machine, UINT32_C(0x04000082), UINT16_C(0x0002)).status ==
               GBL_BUS_STATUS_OK;
}

static bool prepare_mix_bias_write(gbl_machine *machine, uint64_t start,
                                   uint8_t *staging,
                                   size_t staging_capacity,
                                   size_t *staged_bytes)
{
    const uint32_t address = UINT32_C(0x04000088);
    gbl_cycle_cost write_cost = { 0 };
    gbl_time_operation operation;
    gbl_machine_time_preview preview;

    if (machine == NULL || staged_bytes == NULL ||
        gbl_machine_advance_to(machine, start) != GBL_TIME_STATUS_OK ||
        !gbl_bus_quote(machine, address, GBL_BUS_DIRECTION_WRITE, 2u,
                       GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_ROLE_DATA,
                       &write_cost) ||
        write_cost.cycles != 1u) {
        return false;
    }
    operation = (gbl_time_operation){
        .kind = GBL_TIME_OPERATION_BUS_WRITE,
        .width = 2u,
        .address = address,
        .value = UINT32_C(0xFFFF),
        .cycles = write_cost.cycles
    };
    if (gbl_machine_prepare_operations_audio(
            machine, &operation, 1u, &preview, staging, staging_capacity,
            staged_bytes) != GBL_TIME_STATUS_OK ||
        !gbl_machine_commit_time_checked(machine, &preview)) {
        return false;
    }
    return true;
}

static bool run_mix_bias_timing(unsigned timing, uint8_t output[8],
                                size_t *operation_staged_bytes,
                                uint64_t *last_capture_cycle,
                                uint16_t *readback)
{
    gbl_machine *machine = new_machine();
    gbl_audio_capture capture;
    uint8_t operation_staging[GBL_AUDIO_FRAME_BYTES] = { 0u };
    size_t operation_bytes = 0u;
    size_t expected_capture_bytes = 0u;
    size_t prior_capture_bytes = 0u;
    bool machine_at_expected_cycle;
    bool valid = machine != NULL && configure_mix_bias_machine(machine);

    memset(output, 0xA5, 8u);
    if (valid && timing == 0u) {
        valid = gbl_machine_advance_to(machine, UINT64_C(510)) ==
                    GBL_TIME_STATUS_OK &&
                prepare_mix_bias_write(machine, UINT64_C(510), NULL, 0u,
                                       &operation_bytes) &&
                operation_bytes == 0u &&
                gbl_audio_capture_begin(machine, &capture, output, 8u) ==
                    GBL_TIME_STATUS_OK &&
                gbl_machine_advance_to_audio(machine, UINT64_C(1024),
                                             &capture) ==
                    GBL_TIME_STATUS_OK;
        expected_capture_bytes = 8u;
    } else if (valid && timing == 1u) {
        valid = gbl_machine_advance_to(machine, UINT64_C(511)) ==
                    GBL_TIME_STATUS_OK &&
                prepare_mix_bias_write(machine, UINT64_C(511),
                                       operation_staging,
                                       sizeof(operation_staging),
                                       &operation_bytes) &&
                operation_bytes == GBL_AUDIO_FRAME_BYTES;
        if (valid) {
            memcpy(output, operation_staging, GBL_AUDIO_FRAME_BYTES);
            valid = gbl_audio_capture_begin(
                        machine, &capture, output + GBL_AUDIO_FRAME_BYTES,
                        GBL_AUDIO_FRAME_BYTES) == GBL_TIME_STATUS_OK &&
                    gbl_machine_advance_to_audio(machine, UINT64_C(1024),
                                                 &capture) ==
                        GBL_TIME_STATUS_OK;
        }
        expected_capture_bytes = GBL_AUDIO_FRAME_BYTES;
    } else if (valid && timing == 2u) {
        valid = gbl_audio_capture_begin(machine, &capture, output, 8u) ==
                    GBL_TIME_STATUS_OK &&
                gbl_machine_advance_to_audio(machine, UINT64_C(512),
                                             &capture) ==
                    GBL_TIME_STATUS_OK &&
                capture.bytes_written == GBL_AUDIO_FRAME_BYTES &&
                (prior_capture_bytes = capture.bytes_written) ==
                    GBL_AUDIO_FRAME_BYTES &&
                prepare_mix_bias_write(machine, UINT64_C(512), NULL, 0u,
                                       &operation_bytes) &&
                operation_bytes == 0u &&
                gbl_audio_capture_begin(
                    machine, &capture, output + GBL_AUDIO_FRAME_BYTES,
                    GBL_AUDIO_FRAME_BYTES) == GBL_TIME_STATUS_OK &&
                gbl_machine_advance_to_audio(machine, UINT64_C(1024),
                                             &capture) ==
                    GBL_TIME_STATUS_OK;
        expected_capture_bytes = GBL_AUDIO_FRAME_BYTES;
    } else if (timing > 2u) {
        valid = false;
    }

    machine_at_expected_cycle =
        valid && machine->audio.cycle_anchor == UINT64_C(1024);
    if (valid) {
        const size_t total_bytes = prior_capture_bytes + operation_bytes +
                                   capture.bytes_written;
        const gbl_bus_result bias_result =
            read16(machine, UINT32_C(0x04000088));

        *operation_staged_bytes = operation_bytes;
        *last_capture_cycle = capture.last_accepted_cycle;
        *readback = (uint16_t)bias_result.value;
        valid = total_bytes == 8u &&
                expected_capture_bytes + prior_capture_bytes +
                        operation_bytes ==
                    8u &&
                machine_at_expected_cycle &&
                *last_capture_cycle == UINT64_C(1024) &&
                bias_result.status == GBL_BUS_STATUS_OK &&
                *readback == UINT16_C(0xC3FF);
    }
    free_machine(machine);
    return valid;
}

static int run_mix_bias(void)
{
    static const uint8_t expected_frames[8] = {
        0x00u, 0x78u, 0x00u, 0x00u, 0x00u, 0x78u, 0x00u, 0x00u
    };
    gbl_machine *master_off = new_machine();
    gbl_machine *baseline_machine = new_machine();
    gbl_audio_capture baseline_capture;
    uint8_t baseline[8] = { 0u };
    uint8_t before[8] = { 0u };
    uint8_t at_edge[8] = { 0u };
    uint8_t after[8] = { 0u };
    size_t before_staged = 0u;
    size_t edge_staged = 0u;
    size_t after_staged = 0u;
    uint64_t before_cycle = 0u;
    uint64_t edge_cycle = 0u;
    uint64_t after_cycle = 0u;
    uint16_t before_readback = 0u;
    uint16_t edge_readback = 0u;
    uint16_t after_readback = 0u;
    bool before_ok;
    bool edge_ok;
    bool after_ok;
    bool baseline_ok = baseline_machine != NULL &&
                       configure_mix_bias_machine(baseline_machine) &&
                       gbl_audio_capture_begin(baseline_machine,
                                               &baseline_capture, baseline,
                                               sizeof(baseline)) ==
                           GBL_TIME_STATUS_OK &&
                       gbl_machine_advance_to_audio(
                           baseline_machine, UINT64_C(1024),
                           &baseline_capture) == GBL_TIME_STATUS_OK &&
                       baseline_capture.bytes_written == sizeof(baseline) &&
                       baseline_capture.last_accepted_cycle ==
                           UINT64_C(1024) &&
                       baseline_machine->audio.cycle_anchor ==
                           UINT64_C(1024);

    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");
    tap_check("SOUNDBIAS resets and reads back its preset with master off",
              master_off != NULL &&
                  master_off->audio.registers[18] == 0u &&
                  master_off->audio.soundbias == UINT16_C(0x0200) &&
                  read16(master_off, UINT32_C(0x04000088)).value ==
                      UINT32_C(0x0200));
    if (master_off != NULL) {
        const gbl_bus_result write_result =
            write16(master_off, UINT32_C(0x04000088), UINT16_C(0xFFFF));
        const gbl_bus_result read_result =
            read16(master_off, UINT32_C(0x04000088));

        tap_check("SOUNDBIAS keeps its mask and readback while master is off",
                  write_result.status == GBL_BUS_STATUS_OK &&
                      read_result.status == GBL_BUS_STATUS_OK &&
                      read_result.value == UINT32_C(0xC3FF) &&
                      master_off->audio.soundbias == UINT16_C(0xC3FF) &&
                      master_off->audio.registers[18] == 0u);
        gbl_machine_reset(master_off);
        tap_check("machine reset restores SOUNDBIAS without enabling audio",
                  master_off->audio.soundbias == UINT16_C(0x0200) &&
                      master_off->audio.registers[18] == 0u);
    } else {
        tap_check("SOUNDBIAS keeps its mask and readback while master is off",
                  false);
        tap_check("machine reset restores SOUNDBIAS without enabling audio",
                  false);
    }
    tap_check("default bias captures two nonzero frames on the fixed grid",
              baseline_ok &&
                  memcmp(baseline, expected_frames, sizeof(baseline)) == 0);

    before_ok = run_mix_bias_timing(0u, before, &before_staged, &before_cycle,
                                    &before_readback);
    tap_check("SOUNDBIAS write before the edge leaves PCM and cadence intact",
              before_ok && before_staged == 0u &&
                  memcmp(before, expected_frames, sizeof(before)) == 0);
    edge_ok = run_mix_bias_timing(1u, at_edge, &edge_staged, &edge_cycle,
                                  &edge_readback);
    tap_check("SOUNDBIAS write completing at the edge samples old routing first",
              edge_ok && edge_staged == GBL_AUDIO_FRAME_BYTES &&
                  memcmp(at_edge, expected_frames, sizeof(at_edge)) == 0);
    after_ok = run_mix_bias_timing(2u, after, &after_staged, &after_cycle,
                                   &after_readback);
    tap_check("SOUNDBIAS write after the edge leaves PCM and cadence intact",
              after_ok && after_staged == 0u &&
                  memcmp(after, expected_frames, sizeof(after)) == 0);
    tap_check("bias rate-bit twins preserve output bytes and accepted cycles",
              baseline_ok && before_cycle == UINT64_C(1024) &&
                  edge_cycle == UINT64_C(1024) &&
                  after_cycle == UINT64_C(1024) &&
                  before_readback == UINT16_C(0xC3FF) &&
                  edge_readback == UINT16_C(0xC3FF) &&
                  after_readback == UINT16_C(0xC3FF) &&
                  memcmp(baseline, before, sizeof(baseline)) == 0 &&
                  memcmp(baseline, at_edge, sizeof(baseline)) == 0 &&
                  memcmp(baseline, after, sizeof(baseline)) == 0);
    if (!baseline_ok || !before_ok || !edge_ok || !after_ok) {
        printf("# baseline: %02X %02X %02X %02X %02X %02X %02X %02X\n",
               baseline[0], baseline[1], baseline[2], baseline[3],
               baseline[4], baseline[5], baseline[6], baseline[7]);
        printf("# before: %02X %02X %02X %02X %02X %02X %02X %02X\n",
               before[0], before[1], before[2], before[3], before[4],
               before[5], before[6], before[7]);
        printf("# at edge: %02X %02X %02X %02X %02X %02X %02X %02X\n",
               at_edge[0], at_edge[1], at_edge[2], at_edge[3], at_edge[4],
               at_edge[5], at_edge[6], at_edge[7]);
        printf("# after: %02X %02X %02X %02X %02X %02X %02X %02X\n",
               after[0], after[1], after[2], after[3], after[4], after[5],
               after[6], after[7]);
    }

    free_machine(master_off);
    free_machine(baseline_machine);
    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

static bool fifo_lane_vector(gbl_machine *machine, unsigned fifo_index,
                             uint8_t width, uint8_t lane_offset,
                             uint32_t value)
{
    uint8_t expected[4] = {
        UINT8_C(0x10), UINT8_C(0x20), UINT8_C(0x30), UINT8_C(0x40)
    };
    const uint32_t port = GBL_AUDIO_FIFO_FIRST +
                          (uint32_t)(fifo_index * 4u);
    gbl_audio_fifo_state *fifo;
    gbl_bus_result result;
    unsigned lane;

    if (machine == NULL || fifo_index >= GBL_AUDIO_FIFO_COUNT) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
        GBL_BUS_STATUS_OK) {
        return false;
    }
    fifo = &machine->audio.fifo[fifo_index];
    for (lane = 0u; lane < sizeof(expected); ++lane) {
        fifo->bytes[lane] = expected[lane];
    }
    if (!gbl_audio_state_valid(&machine->audio)) {
        return false;
    }
    for (lane = 0u; lane < width; ++lane) {
        expected[lane_offset + lane] =
            (uint8_t)(value >> (lane * 8u));
    }
    result = gbl_bus_write(machine, port + lane_offset, width,
                           GBL_BUS_CLASS_NONSEQUENTIAL, value);
    if (result.status != GBL_BUS_STATUS_OK ||
        result.outcome != GBL_BUS_OUTCOME_IO_AUDIO_SUBSET ||
        fifo->count != 4u || fifo->read_cursor != 0u ||
        fifo->write_cursor != 4u) {
        return false;
    }
    for (lane = 0u; lane < sizeof(expected); ++lane) {
        if (fifo->bytes[lane] != expected[lane]) {
            return false;
        }
    }
    return gbl_audio_state_valid(&machine->audio);
}

static bool fifo_fill_full_queue(gbl_machine *machine)
{
    unsigned index;

    if (machine == NULL ||
        write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    for (index = 0u; index < 8u; ++index) {
        const uint32_t value = index == 0u
                                   ? UINT32_C(0x807F01FE)
                                   : UINT32_C(0x10203040) + index;
        const gbl_bus_result result = gbl_bus_write(
            machine, GBL_AUDIO_FIFO_FIRST, 4u,
            GBL_BUS_CLASS_NONSEQUENTIAL, value);

        if (result.status != GBL_BUS_STATUS_OK ||
            result.outcome != GBL_BUS_OUTCOME_IO_AUDIO_SUBSET) {
            return false;
        }
    }
    return machine->audio.fifo[0].count == GBL_AUDIO_FIFO_CAPACITY &&
           machine->audio.fifo[0].read_cursor == 0u &&
           machine->audio.fifo[0].write_cursor == 0u;
}

static bool fifo_full_overflow_vector(gbl_machine *machine, bool pop_odd)
{
    gbl_audio_fifo_state *fifo;
    gbl_bus_result result;
    int8_t held_sample;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (!fifo_fill_full_queue(machine)) {
        return false;
    }
    fifo = &machine->audio.fifo[0];
    if (pop_odd) {
        (void)gbl_audio_fifo_clock_timer(&machine->audio, 0u);
        if (fifo->count != 31u || fifo->read_cursor != 1u ||
            fifo->sample != -2) {
            return false;
        }
    } else {
        fifo->sample = -37;
    }
    held_sample = fifo->sample;
    result = gbl_bus_write(machine, GBL_AUDIO_FIFO_FIRST, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x44332211));
    return result.status == GBL_BUS_STATUS_OK &&
           result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET &&
           fifo->count == 4u && fifo->read_cursor == 0u &&
           fifo->write_cursor == 4u && fifo->sample == held_sample &&
           fifo->bytes[0] == UINT8_C(0x11) &&
           fifo->bytes[1] == UINT8_C(0x22) &&
           fifo->bytes[2] == UINT8_C(0x33) &&
           fifo->bytes[3] == UINT8_C(0x44) &&
           gbl_audio_state_valid(&machine->audio);
}

static bool fifo_wrap_narrow_write_vector(gbl_machine *machine)
{
    gbl_audio_fifo_state *fifo;
    unsigned index;
    gbl_bus_result result;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (!fifo_fill_full_queue(machine)) {
        return false;
    }
    for (index = 0u; index < 4u; ++index) {
        (void)gbl_audio_fifo_clock_timer(&machine->audio, 0u);
    }
    fifo = &machine->audio.fifo[0];
    if (fifo->count != 28u || fifo->read_cursor != 4u ||
        fifo->write_cursor != 0u || fifo->sample != -128) {
        return false;
    }
    result = gbl_bus_write(machine, GBL_AUDIO_FIFO_FIRST + 1u, 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0xEE));
    return result.status == GBL_BUS_STATUS_OK &&
           fifo->count == 32u && fifo->read_cursor == 4u &&
           fifo->write_cursor == 4u && fifo->bytes[0] == UINT8_C(0xFE) &&
           fifo->bytes[1] == UINT8_C(0xEE) &&
           fifo->bytes[2] == UINT8_C(0x7F) &&
           fifo->bytes[3] == UINT8_C(0x80) &&
           gbl_audio_state_valid(&machine->audio);
}

static bool fifo_read_port_is_unimplemented(gbl_machine *machine)
{
    gbl_audio_fifo_state before;
    const gbl_audio_fifo_state *after;
    gbl_bus_result result;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        gbl_bus_write(machine, GBL_AUDIO_FIFO_FIRST, 4u,
                      GBL_BUS_CLASS_NONSEQUENTIAL,
                      UINT32_C(0x44332211)).status != GBL_BUS_STATUS_OK) {
        return false;
    }
    before = machine->audio.fifo[0];
    result = gbl_bus_read(machine, GBL_AUDIO_FIFO_FIRST, 4u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    after = &machine->audio.fifo[0];
    return result.status == GBL_BUS_STATUS_OK &&
           result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED &&
           memcmp(before.bytes, after->bytes, sizeof(before.bytes)) == 0 &&
           before.read_cursor == after->read_cursor &&
           before.write_cursor == after->write_cursor &&
           before.count == after->count && before.sample == after->sample;
}

static bool fifo_is_reset(const gbl_audio_fifo_state *fifo)
{
    size_t index;

    if (fifo == NULL || fifo->read_cursor != 0u ||
        fifo->write_cursor != 0u || fifo->count != 0u || fifo->sample != 0) {
        return false;
    }
    for (index = 0u; index < GBL_AUDIO_FIFO_CAPACITY; ++index) {
        if (fifo->bytes[index] != 0u) {
            return false;
        }
    }
    return true;
}

static bool audio_fifo_equality_vector(const gbl_audio_state *state)
{
    gbl_audio_state candidate;
    unsigned fifo_index;
    size_t index;

    if (state == NULL) {
        return false;
    }
    for (fifo_index = 0u; fifo_index < GBL_AUDIO_FIFO_COUNT; ++fifo_index) {
        for (index = 0u; index < GBL_AUDIO_FIFO_CAPACITY; ++index) {
            candidate = *state;
            candidate.fifo[fifo_index].bytes[index] ^= UINT8_C(0x01);
            if (gbl_audio_state_equal(state, &candidate)) {
                return false;
            }
        }
        candidate = *state;
        candidate.fifo[fifo_index].read_cursor ^= UINT8_C(0x01);
        if (gbl_audio_state_equal(state, &candidate)) {
            return false;
        }
        candidate = *state;
        candidate.fifo[fifo_index].write_cursor ^= UINT8_C(0x04);
        if (gbl_audio_state_equal(state, &candidate)) {
            return false;
        }
        candidate = *state;
        candidate.fifo[fifo_index].count ^= UINT8_C(0x01);
        if (gbl_audio_state_equal(state, &candidate)) {
            return false;
        }
        candidate = *state;
        candidate.fifo[fifo_index].sample =
            candidate.fifo[fifo_index].sample == INT8_MAX
                ? INT8_MIN
                : (int8_t)(candidate.fifo[fifo_index].sample + 1);
        if (gbl_audio_state_equal(state, &candidate)) {
            return false;
        }
    }
    return true;
}

static int run_ds_fifo(void)
{
    static const struct {
        unsigned fifo_index;
        uint8_t width;
        uint8_t lane_offset;
        uint32_t value;
    } lane_cases[] = {
        { 0u, 1u, 0u, UINT32_C(0xA1) },
        { 0u, 1u, 1u, UINT32_C(0xB2) },
        { 0u, 1u, 2u, UINT32_C(0xC3) },
        { 0u, 1u, 3u, UINT32_C(0xD4) },
        { 0u, 2u, 0u, UINT32_C(0xA1B2) },
        { 0u, 2u, 1u, UINT32_C(0xB2C3) },
        { 0u, 2u, 2u, UINT32_C(0xC3D4) },
        { 0u, 4u, 0u, UINT32_C(0x44332211) },
        { 1u, 1u, 0u, UINT32_C(0xA1) },
        { 1u, 1u, 1u, UINT32_C(0xB2) },
        { 1u, 1u, 2u, UINT32_C(0xC3) },
        { 1u, 1u, 3u, UINT32_C(0xD4) },
        { 1u, 2u, 0u, UINT32_C(0xA1B2) },
        { 1u, 2u, 1u, UINT32_C(0xB2C3) },
        { 1u, 2u, 2u, UINT32_C(0xC3D4) },
        { 1u, 4u, 0u, UINT32_C(0x88776655) }
    };
    uint64_t first_overflow = 0u;
    gbl_machine *machine = new_machine();
    gbl_bus_result result;
    gbl_bus_result timer_read;
    gbl_machine_time_preview preview;
    gbl_time_operation operation;
    gbl_audio_state audio_before;
    uint64_t cycles_before;
    size_t lane_case_index;
    char lane_name[112];

    tap_test_number = 0u;
    tap_failure_count = 0u;
    tap_check("fresh machine is available", machine != NULL);
    if (machine != NULL) {
        tap_check("sound master can be enabled",
                  write16(machine, UINT32_C(0x04000084),
                          UINT16_C(0x0080)).status == GBL_BUS_STATUS_OK);
        tap_check("Timer 0 reload is programmed before enable",
                  write16(machine, UINT32_C(0x04000100),
                          UINT16_C(0xFFFC)).status == GBL_BUS_STATUS_OK);
        result = gbl_bus_write(machine, UINT32_C(0x040000A0), 4u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x807F01FE));
        tap_check("FIFO A guest word write is accepted",
                  result.status == GBL_BUS_STATUS_OK);
        tap_check("FIFO A write is classified as modeled audio I/O",
                  result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
        if (result.outcome != GBL_BUS_OUTCOME_IO_AUDIO_SUBSET) {
            printf("# observed bus outcome: %u\n",
                   (unsigned)result.outcome);
        }
        tap_check("Timer 0 is enabled after the FIFO write",
                  write16(machine, UINT32_C(0x04000102),
                          UINT16_C(0x0080)).status == GBL_BUS_STATUS_OK);
        first_overflow = machine->timers[0].anchor_cycle +
                         (UINT64_C(0x10000) -
                          machine->timers[0].counter) *
                             machine->timers[0].divisor -
                         machine->timers[0].phase;
        tap_check("word lanes reach FIFO A in increasing address order",
                  machine->audio.fifo[0].bytes[0] == UINT8_C(0xFE) &&
                      machine->audio.fifo[0].bytes[1] == UINT8_C(0x01) &&
                      machine->audio.fifo[0].bytes[2] == UINT8_C(0x7F) &&
                      machine->audio.fifo[0].bytes[3] == UINT8_C(0x80) &&
                      machine->audio.fifo[0].count == 4u &&
                      machine->audio.fifo[0].sample == 0);
        timer_read = read16(machine, UINT32_C(0x04000100));
        tap_check("Timer 0 read before its edge leaves the FIFO unchanged",
                  timer_read.status == GBL_BUS_STATUS_OK &&
                      machine->master_cycles < first_overflow &&
                      machine->audio.fifo[0].count == 4u &&
                      machine->audio.fifo[0].sample == 0);
        tap_check("advance immediately before overflow preserves FIFO state",
                  gbl_machine_advance_to(machine, first_overflow - 1u) ==
                          GBL_TIME_STATUS_OK &&
                      machine->audio.fifo[0].count == 4u &&
                      machine->audio.fifo[0].sample == 0);
        tap_check("Timer 0 overflow consumes and sign extends one byte",
                  gbl_machine_advance_to(machine, first_overflow) ==
                          GBL_TIME_STATUS_OK &&
                      machine->audio.fifo[0].count == 3u &&
                      machine->audio.fifo[0].sample == -2 &&
                      machine->audio.fifo[1].count == 0u);
        tap_check("FIFO state remains valid after its first sample edge",
                  gbl_audio_state_valid(&machine->audio));

        audio_before = machine->audio;
        cycles_before = machine->master_cycles;
        operation = (gbl_time_operation){
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, 1u
        };
        tap_check("prepare one cycle for the forged-state refusal vector",
                  gbl_machine_prepare_operations(machine, &operation, 1u,
                                                 &preview) ==
                      GBL_TIME_STATUS_OK);
        preview.audio.fifo[0].bytes[1] ^= UINT8_C(0x01);
        tap_check("altered prepared FIFO backing byte is refused",
                  !gbl_machine_commit_time_checked(machine, &preview));
        tap_check("refused prepared FIFO state leaves machine unchanged",
                  machine->master_cycles == cycles_before &&
                      gbl_audio_state_equal(&machine->audio, &audio_before));
    } else {
        tap_check("FIFO A guest word write is accepted", false);
        tap_check("FIFO A write is classified as modeled audio I/O", false);
        tap_check("sound master can be enabled", false);
        tap_check("Timer 0 reload is programmed before enable", false);
        tap_check("Timer 0 is enabled after the FIFO write", false);
        tap_check("word lanes reach FIFO A in increasing address order", false);
        tap_check("Timer 0 read before its edge leaves the FIFO unchanged",
                  false);
        tap_check("advance immediately before overflow preserves FIFO state",
                  false);
        tap_check("Timer 0 overflow consumes and sign extends one byte", false);
        tap_check("FIFO state remains valid after its first sample edge", false);
        tap_check("prepare one cycle for the forged-state refusal vector", false);
        tap_check("altered prepared FIFO backing byte is refused", false);
        tap_check("refused prepared FIFO state leaves machine unchanged", false);
    }
    for (lane_case_index = 0u;
         lane_case_index < sizeof(lane_cases) / sizeof(lane_cases[0]);
         ++lane_case_index) {
        const unsigned fifo_index = lane_cases[lane_case_index].fifo_index;

        (void)snprintf(lane_name, sizeof(lane_name),
                       "FIFO %c width %u lane %u merges addressed lanes",
                       fifo_index == 0u ? 'A' : 'B',
                       (unsigned)lane_cases[lane_case_index].width,
                       (unsigned)lane_cases[lane_case_index].lane_offset);
        tap_check(lane_name,
                  machine != NULL && fifo_lane_vector(
                                         machine, fifo_index,
                                         lane_cases[lane_case_index].width,
                                         lane_cases[lane_case_index].lane_offset,
                                         lane_cases[lane_case_index].value));
    }
    tap_check("full FIFO write flushes and accepts one complete word",
              fifo_full_overflow_vector(machine, false));
    tap_check("overflow after an odd pop resets cursors atomically",
              fifo_full_overflow_vector(machine, true));
    tap_check("wrapped narrow write preserves reused backing lanes",
              fifo_wrap_narrow_write_vector(machine));
    tap_check("FIFO reads keep unsupported-I/O behavior without mutation",
              fifo_read_port_is_unimplemented(machine));
    free_machine(machine);
    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

static int run_ds_transaction(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *snapshot = NULL;
    gbl_machine_time_preview preview;
    gbl_machine_time_preview preview_before;
    gbl_time_operation operation;
    gbl_audio_state audio_before;
    gbl_bus_result result;
    gbl_bus_result readback;
    uint8_t staging[4];
    uint8_t staging_before[sizeof(staging)];
    size_t staged_bytes;
    uint64_t cycles_before;

    tap_test_number = 0u;
    tap_failure_count = 0u;
    tap_check("fresh machine is available", machine != NULL);
    if (machine != NULL) {
        tap_check("sound master can be enabled",
                  write16(machine, UINT32_C(0x04000084),
                          UINT16_C(0x0080)).status == GBL_BUS_STATUS_OK);
        tap_check("SOUNDCNT_H low policy bits can be programmed",
                  write16(machine, UINT32_C(0x04000082),
                          UINT16_C(0x0005)).status == GBL_BUS_STATUS_OK);
        tap_check("FIFO A accepts its initial word",
                  gbl_bus_write(machine, UINT32_C(0x040000A0), 4u,
                                GBL_BUS_CLASS_NONSEQUENTIAL,
                                UINT32_C(0x44332211)).status ==
                      GBL_BUS_STATUS_OK);
        tap_check("FIFO B accepts its initial word",
                  gbl_bus_write(machine, UINT32_C(0x040000A4), 4u,
                                GBL_BUS_CLASS_NONSEQUENTIAL,
                                UINT32_C(0x88776655)).status ==
                      GBL_BUS_STATUS_OK);
        tap_check("both FIFOs contain four queued bytes",
                  machine->audio.fifo[0].count == 4u &&
                      machine->audio.fifo[1].count == 4u);
        tap_check("audio equality binds every FIFO byte and field",
                  audio_fifo_equality_vector(&machine->audio));
        (void)gbl_audio_fifo_clock_timer(&machine->audio, 0u);
        tap_check("both FIFOs retain their current signed sample",
                  machine->audio.fifo[0].count == 3u &&
                      machine->audio.fifo[0].sample == 0x11 &&
                      machine->audio.fifo[1].count == 3u &&
                      machine->audio.fifo[1].sample == 0x55);
        tap_check("Timer 0 can run while FIFO controls change",
                  write16(machine, UINT32_C(0x04000100),
                          UINT16_C(0xFFF0)).status == GBL_BUS_STATUS_OK &&
                      write16(machine, UINT32_C(0x04000102),
                              UINT16_C(0x0080)).status == GBL_BUS_STATUS_OK);
        result = gbl_bus_write(machine, UINT32_C(0x04000082), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x0D));
        tap_check("SOUNDCNT_H low-lane bits do not pulse FIFO reset",
                  result.status == GBL_BUS_STATUS_OK &&
                      machine->audio.fifo[0].count == 3u &&
                      machine->audio.fifo[1].count == 3u);
        result = gbl_bus_write(machine, UINT32_C(0x04000083), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x08));
        tap_check("SOUNDCNT_H FIFO A action resets only FIFO A",
                  result.status == GBL_BUS_STATUS_OK &&
                      machine->audio.fifo[0].count == 0u &&
                      machine->audio.fifo[0].read_cursor == 0u &&
                      machine->audio.fifo[0].write_cursor == 0u &&
                      machine->audio.fifo[0].sample == 0 &&
                      fifo_is_reset(&machine->audio.fifo[0]));
        tap_check("FIFO B is unchanged by FIFO A reset",
                  machine->audio.fifo[1].count == 3u &&
                      machine->audio.fifo[1].bytes[0] == UINT8_C(0x55) &&
                      machine->audio.fifo[1].sample == 0x55);

        operation = (gbl_time_operation){
            GBL_TIME_OPERATION_BUS_WRITE, 1u, UINT32_C(0x04000083),
            UINT32_C(0x80), 1u
        };
        tap_check("prepared FIFO B reset records its transaction mask",
                  gbl_machine_prepare_operations(machine, &operation, 1u,
                                                 &preview) ==
                          GBL_TIME_STATUS_OK &&
                      preview.audio_fifo_reset_mask == UINT8_C(0x02) &&
                      preview.audio_fifo_reset_mask_prepared ==
                          UINT8_C(0x02) &&
                      fifo_is_reset(&preview.audio.fifo[1]) &&
                      fifo_is_reset(&preview.audio.fifo[0]));
        {
            gbl_machine_time_preview forged = preview;

            forged.audio_fifo_reset_mask ^= UINT8_C(0x01);
            tap_check("altered prepared reset mask is refused",
                      !gbl_machine_commit_time_checked(machine, &forged) &&
                          machine->audio.fifo[1].count == 3u);
        }
        tap_check("prepared FIFO B reset commits through checked state",
                  gbl_machine_commit_time_checked(machine, &preview) &&
                      fifo_is_reset(&machine->audio.fifo[1]) &&
                      fifo_is_reset(&machine->audio.fifo[0]));
        readback = read16(machine, UINT32_C(0x04000082));
        tap_check("SOUNDCNT_H action bits stay out of readback",
                  readback.status == GBL_BUS_STATUS_OK &&
                      (readback.value & UINT32_C(0x8800)) == 0u &&
                      (readback.value & UINT32_C(0x000F)) ==
                          UINT32_C(0x000D));
        tap_check("explicit FIFO resets leave Timer 0 running",
                  machine->timers[0].enabled);

        tap_check("both FIFOs can be refilled before master disable",
                  gbl_bus_write(machine, UINT32_C(0x040000A0), 4u,
                                GBL_BUS_CLASS_NONSEQUENTIAL,
                                UINT32_C(0x807F01FE)).status ==
                          GBL_BUS_STATUS_OK &&
                      gbl_bus_write(machine, UINT32_C(0x040000A4), 4u,
                                    GBL_BUS_CLASS_NONSEQUENTIAL,
                                    UINT32_C(0x88776655)).status ==
                          GBL_BUS_STATUS_OK);
        (void)gbl_audio_fifo_clock_timer(&machine->audio, 0u);
        result = write16(machine, UINT32_C(0x04000084), 0u);
        tap_check("master disable clears both complete FIFO states",
                  result.status == GBL_BUS_STATUS_OK &&
                      fifo_is_reset(&machine->audio.fifo[0]) &&
                      fifo_is_reset(&machine->audio.fifo[1]) &&
                      machine->timers[0].enabled &&
                      gbl_audio_state_valid(&machine->audio));

        tap_check("snapshot supports FIFO crossing refusal checks",
                  snapshot_machine(machine, &snapshot));
        result = gbl_bus_write(machine, UINT32_C(0x040000A3), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0xBBAA));
        tap_check("direct FIFO A-to-B crossing span is refused atomically",
                  result.status == GBL_BUS_STATUS_TIME_REFUSAL &&
                      snapshot != NULL && unchanged_machine(machine, snapshot));

        memset(&preview, 0x5A, sizeof(preview));
        preview_before = preview;
        memset(staging, 0xA5, sizeof(staging));
        memcpy(staging_before, staging, sizeof(staging));
        staged_bytes = 77u;
        operation = (gbl_time_operation){
            GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x040000A3),
            UINT32_C(0xBBAA), GBL_AUDIO_SAMPLE_CYCLES
        };
        tap_check("prepared FIFO crossing rejects before time or output staging",
                  gbl_machine_prepare_operations_audio(
                      machine, &operation, 1u, &preview, staging,
                      sizeof(staging), &staged_bytes) ==
                          GBL_TIME_STATUS_INVALID_ARGUMENT &&
                      memcmp(&preview, &preview_before, sizeof(preview)) ==
                          0 &&
                      memcmp(staging, staging_before, sizeof(staging)) == 0 &&
                      staged_bytes == 77u && snapshot != NULL &&
                      unchanged_machine(machine, snapshot));
        free(snapshot);
        snapshot = NULL;

        machine->audio.fifo[0].write_cursor = 1u;
        audio_before = machine->audio;
        tap_check("malformed FIFO cursor fails validation and write",
                  !gbl_audio_state_valid(&machine->audio) &&
                      !gbl_audio_fifo_write(&machine->audio,
                                            UINT32_C(0x040000A0), 4u,
                                            UINT32_C(0x44332211)) &&
                      gbl_audio_state_equal(&machine->audio, &audio_before));
        machine->audio.fifo[0].write_cursor = 0u;

        gbl_machine_reset(machine);
        (void)write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080));
        (void)gbl_bus_write(machine, UINT32_C(0x040000A0), 4u,
                            GBL_BUS_CLASS_NONSEQUENTIAL,
                            UINT32_C(0x44332211));
        audio_before = machine->audio;
        cycles_before = machine->master_cycles;
        operation = (gbl_time_operation){
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, 1u
        };
        tap_check("prepare one cycle for the forged-latch refusal vector",
                  gbl_machine_prepare_operations(machine, &operation, 1u,
                                                 &preview) ==
                      GBL_TIME_STATUS_OK);
        preview.audio.fifo[0].sample = 1;
        tap_check("altered prepared FIFO latch is refused",
                  !gbl_machine_commit_time_checked(machine, &preview));
        tap_check("forged latch refusal leaves machine time and queue intact",
                  machine->master_cycles == cycles_before &&
                      gbl_audio_state_equal(&machine->audio, &audio_before));
    } else {
        tap_check("sound master can be enabled", false);
        tap_check("SOUNDCNT_H low policy bits can be programmed", false);
        tap_check("FIFO A accepts its initial word", false);
        tap_check("FIFO B accepts its initial word", false);
        tap_check("both FIFOs contain four queued bytes", false);
        tap_check("both FIFOs retain their current signed sample", false);
        tap_check("Timer 0 can run while FIFO controls change", false);
        tap_check("SOUNDCNT_H low-lane bits do not pulse FIFO reset", false);
        tap_check("SOUNDCNT_H FIFO A action resets only FIFO A", false);
        tap_check("FIFO B is unchanged by FIFO A reset", false);
    }
    free_machine(machine);
    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
}

static uint64_t timer_next_overflow_cycle(const gbl_timer_state *timer)
{
    const uint64_t remaining =
        timer == NULL ? 0u : UINT64_C(0x10000) - timer->counter;
    uint64_t delta;

    if (timer == NULL || !timer->enabled || timer->cascade ||
        timer->divisor == 0u || timer->phase >= timer->divisor) {
        return UINT64_MAX;
    }
    delta = remaining * timer->divisor;
    if (delta <= timer->phase || delta - timer->phase >
                                     UINT64_MAX - timer->anchor_cycle) {
        return UINT64_MAX;
    }
    return timer->anchor_cycle + delta - timer->phase;
}

static bool write_fifo_word(gbl_machine *machine, unsigned fifo_index,
                            uint32_t value)
{
    uint32_t address;
    gbl_bus_result result;

    if (machine == NULL || fifo_index >= GBL_AUDIO_FIFO_COUNT) {
        return false;
    }
    address = GBL_AUDIO_FIFO_FIRST + (uint32_t)(fifo_index * 4u);
    result = gbl_bus_write(machine, address, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, value);
    return result.status == GBL_BUS_STATUS_OK &&
           result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET;
}

static bool fifo_timer_cascade_vector(gbl_machine *machine)
{
    uint64_t first_overflow;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        gbl_bus_write(machine, UINT32_C(0x04000083), 1u,
                      GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x04)).status !=
            GBL_BUS_STATUS_OK ||
        !write_fifo_word(machine, 0u, UINT32_C(0x807F01FE)) ||
        !write_fifo_word(machine, 1u, UINT32_C(0x807F01FE)) ||
        write16(machine, UINT32_C(0x04000104), UINT16_C(0xFFFE)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000106), UINT16_C(0x0084)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000100), UINT16_C(0xFFFE)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000102), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    first_overflow = timer_next_overflow_cycle(&machine->timers[0]);
    if (first_overflow == UINT64_MAX ||
        gbl_machine_advance_to(machine, first_overflow - 1u) !=
            GBL_TIME_STATUS_OK ||
        machine->audio.fifo[0].count != 4u ||
        machine->audio.fifo[1].count != 4u) {
        return false;
    }
    if (gbl_machine_advance_to(machine, first_overflow) !=
            GBL_TIME_STATUS_OK ||
        machine->audio.fifo[0].count != 4u ||
        machine->audio.fifo[1].count != 3u ||
        machine->audio.fifo[1].sample != -2 ||
        machine->timers[1].counter != UINT16_MAX) {
        return false;
    }
    if (gbl_machine_advance_to(machine, first_overflow + 1u) !=
            GBL_TIME_STATUS_OK ||
        machine->audio.fifo[0].count != 4u ||
        machine->audio.fifo[1].count != 3u) {
        return false;
    }
    return gbl_machine_advance_to(machine, first_overflow + 2u) ==
               GBL_TIME_STATUS_OK &&
           machine->audio.fifo[0].count == 3u &&
           machine->audio.fifo[0].sample == -2 &&
           machine->audio.fifo[1].count == 2u &&
           machine->audio.fifo[1].sample == 1 &&
           machine->timers[1].counter == UINT16_C(0xFFFE) &&
           gbl_audio_state_valid(&machine->audio);
}

static bool fifo_same_timer_vector(gbl_machine *machine)
{
    uint64_t deadline;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        !write_fifo_word(machine, 0u, UINT32_C(0x807F01FE)) ||
        !write_fifo_word(machine, 1u, UINT32_C(0x807F01FE)) ||
        write16(machine, UINT32_C(0x04000100), UINT16_C(0xFFFE)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000102), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    deadline = timer_next_overflow_cycle(&machine->timers[0]);
    return deadline != UINT64_MAX &&
           gbl_machine_advance_to(machine, deadline) == GBL_TIME_STATUS_OK &&
           machine->audio.fifo[0].count == 3u &&
           machine->audio.fifo[0].sample == -2 &&
           machine->audio.fifo[1].count == 3u &&
           machine->audio.fifo[1].sample == -2;
}

static bool fifo_coincident_timer_vector(gbl_machine *machine)
{
    uint64_t timer0_deadline;
    uint64_t timer1_deadline;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        gbl_bus_write(machine, UINT32_C(0x04000083), 1u,
                      GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x40)).status !=
            GBL_BUS_STATUS_OK ||
        !write_fifo_word(machine, 0u, UINT32_C(0x807F01FE)) ||
        !write_fifo_word(machine, 1u, UINT32_C(0x807F01FE)) ||
        write16(machine, UINT32_C(0x04000100), UINT16_C(0xFFFC)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000102), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000104), UINT16_C(0xFFFE)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000106), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    timer0_deadline = timer_next_overflow_cycle(&machine->timers[0]);
    timer1_deadline = timer_next_overflow_cycle(&machine->timers[1]);
    return timer0_deadline != UINT64_MAX &&
           timer0_deadline == timer1_deadline &&
           gbl_machine_advance_to(machine, timer0_deadline - 1u) ==
               GBL_TIME_STATUS_OK &&
           machine->audio.fifo[0].count == 4u &&
           machine->audio.fifo[1].count == 4u &&
           gbl_machine_advance_to(machine, timer0_deadline) ==
               GBL_TIME_STATUS_OK &&
           machine->audio.fifo[0].count == 3u &&
           machine->audio.fifo[0].sample == -2 &&
           machine->audio.fifo[1].count == 3u &&
           machine->audio.fifo[1].sample == -2;
}

static bool fifo_empty_latch_vector(gbl_machine *machine)
{
    uint64_t deadline;
    unsigned pop;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        !write_fifo_word(machine, 0u, UINT32_C(0x807F01FE)) ||
        write16(machine, UINT32_C(0x04000100), UINT16_C(0xFFFC)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000102), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    deadline = timer_next_overflow_cycle(&machine->timers[0]);
    if (deadline == UINT64_MAX) {
        return false;
    }
    for (pop = 0u; pop < 4u; ++pop) {
        if (gbl_machine_advance_to(machine, deadline + pop * 4u) !=
            GBL_TIME_STATUS_OK) {
            return false;
        }
    }
    return machine->audio.fifo[0].count == 0u &&
           machine->audio.fifo[0].sample == -128 &&
           gbl_machine_advance_to(machine, deadline + 16u) ==
               GBL_TIME_STATUS_OK &&
           machine->audio.fifo[0].count == 0u &&
           machine->audio.fifo[0].sample == -128;
}

static bool fifo_reset_before_overflow_vector(gbl_machine *machine)
{
    uint64_t deadline;
    gbl_bus_result reset_result;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        !write_fifo_word(machine, 0u, UINT32_C(0x807F01FE)) ||
        write16(machine, UINT32_C(0x04000100), UINT16_C(0xFFFE)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000102), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    deadline = timer_next_overflow_cycle(&machine->timers[0]);
    reset_result = gbl_bus_write(machine, UINT32_C(0x04000083), 1u,
                                 GBL_BUS_CLASS_NONSEQUENTIAL,
                                 UINT32_C(0x08));
    return deadline != UINT64_MAX && reset_result.status == GBL_BUS_STATUS_OK &&
           machine->timers[0].enabled &&
           gbl_machine_advance_to(machine, deadline) == GBL_TIME_STATUS_OK &&
           machine->audio.fifo[0].count == 0u &&
           machine->audio.fifo[0].sample == 0 &&
           machine->timers[0].enabled;
}

static bool fifo_stale_timer_vector(gbl_machine *machine)
{
    uint64_t deadline;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        !write_fifo_word(machine, 0u, UINT32_C(0x807F01FE)) ||
        write16(machine, UINT32_C(0x04000100), UINT16_C(0xFFFC)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000102), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    deadline = timer_next_overflow_cycle(&machine->timers[0]);
    if (deadline == UINT64_MAX ||
        write16(machine, UINT32_C(0x04000102), 0u).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    return gbl_machine_advance_to(machine, deadline) == GBL_TIME_STATUS_OK &&
           machine->audio.fifo[0].count == 4u &&
           machine->audio.fifo[0].sample == 0 &&
           !machine->timers[0].enabled;
}

typedef struct fifo_sample_observation {
    unsigned calls;
    uint64_t cycle;
    uint8_t fifo_count;
    int8_t fifo_sample;
    uint16_t soundcnt_h;
} fifo_sample_observation;

static void observe_fifo_sample(
    const gbl_machine_time_preview *candidate, uint64_t cycle,
    void *context, int16_t *left, int16_t *right)
{
    fifo_sample_observation *observation = context;

    if (candidate != NULL && observation != NULL) {
        ++observation->calls;
        observation->cycle = cycle;
        observation->fifo_count = candidate->audio.fifo[0].count;
        observation->fifo_sample = candidate->audio.fifo[0].sample;
        observation->soundcnt_h = candidate->audio.registers[17];
    }
    if (left != NULL) {
        *left = 0;
    }
    if (right != NULL) {
        *right = 0;
    }
}

static bool fifo_capture_edge_order_vector(gbl_machine *machine)
{
    gbl_machine_time_preview preview;
    gbl_time_operation operation;
    fifo_sample_observation observation = { 0 };
    uint8_t staging[GBL_AUDIO_FRAME_BYTES] = { 0u };
    size_t staged_bytes = 0u;
    uint64_t deadline;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK ||
        !write_fifo_word(machine, 0u, UINT32_C(0x807F01FE)) ||
        write16(machine, UINT32_C(0x04000100), UINT16_C(0xFE04)).status !=
            GBL_BUS_STATUS_OK ||
        write16(machine, UINT32_C(0x04000102), UINT16_C(0x0080)).status !=
            GBL_BUS_STATUS_OK) {
        return false;
    }
    deadline = timer_next_overflow_cycle(&machine->timers[0]);
    if (deadline != UINT64_C(512) ||
        gbl_machine_advance_to(machine, deadline - 1u) !=
            GBL_TIME_STATUS_OK) {
        return false;
    }
    operation = (gbl_time_operation){
        GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x04000082),
        UINT32_C(0x4000), 1u
    };
    return gbl_machine_prepare_operations_audio_with_provider(
               machine, &operation, 1u, &preview, staging, sizeof(staging),
               &staged_bytes, observe_fifo_sample, &observation) ==
               GBL_TIME_STATUS_OK &&
           observation.calls == 1u && observation.cycle == deadline &&
           observation.fifo_count == 3u && observation.fifo_sample == -2 &&
           observation.soundcnt_h == 0u && staged_bytes == sizeof(staging) &&
           preview.audio.registers[17] == UINT16_C(0x4000) &&
           preview.audio.fifo[0].count == 3u &&
           preview.audio.fifo[0].sample == -2 &&
           gbl_machine_commit_time_checked(machine, &preview) &&
           machine->master_cycles == deadline &&
           machine->audio.fifo[0].count == 3u;
}

static bool fifo_refill_threshold_vector(gbl_machine *machine)
{
    uint8_t readiness;
    unsigned index;

    if (machine == NULL) {
        return false;
    }
    gbl_machine_reset(machine);
    if (write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status !=
        GBL_BUS_STATUS_OK) {
        return false;
    }
    for (index = 0u; index < 5u; ++index) {
        if (!write_fifo_word(machine, 0u, UINT32_C(0x01020304) + index)) {
            return false;
        }
    }
    for (index = 0u; index < 3u; ++index) {
        if (gbl_audio_fifo_clock_timer(&machine->audio, 0u) != 0u) {
            return false;
        }
    }
    if (machine->audio.fifo[0].count != 17u) {
        return false;
    }
    readiness = gbl_audio_fifo_clock_timer(&machine->audio, 0u);
    if (readiness != UINT8_C(0x01) ||
        machine->audio.fifo[0].count != 16u) {
        return false;
    }
    readiness = gbl_audio_fifo_clock_timer(&machine->audio, 0u);
    return readiness == 0u && machine->audio.fifo[0].count == 15u;
}

static int run_ds_timer(void)
{
    gbl_machine *machine = new_machine();

    tap_test_number = 0u;
    tap_failure_count = 0u;
    tap_check("fresh machine is available", machine != NULL);
    tap_check("Timer 1 cascade carry pops only on actual wrap",
              fifo_timer_cascade_vector(machine));
    tap_check("A and B each pop on one shared Timer 0 edge",
              fifo_same_timer_vector(machine));
    tap_check("coincident direct Timer 0 and Timer 1 edges both pop",
              fifo_coincident_timer_vector(machine));
    tap_check("empty FIFO retains its final signed sample",
              fifo_empty_latch_vector(machine));
    tap_check("FIFO reset before an edge leaves the timer running",
              fifo_reset_before_overflow_vector(machine));
    tap_check("disabled timer's stale deadline cannot consume a sample",
              fifo_stale_timer_vector(machine));
    tap_check("sample provider observes post-pop state before the write",
              fifo_capture_edge_order_vector(machine));
    tap_check("readiness is emitted only at the post-pop 16-byte edge",
              fifo_refill_threshold_vector(machine));
    free_machine(machine);
    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
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
        { UINT32_C(0x04000064), UINT16_C(0x47FF), UINT16_C(0x47FF) },
        { UINT32_C(0x04000068), UINT16_C(0xFFFF), UINT16_C(0xFFC0) },
        { UINT32_C(0x0400006C), UINT16_C(0x47FF), UINT16_C(0x47FF) },
        { UINT32_C(0x04000070), UINT16_C(0x00E0), UINT16_C(0x00E0) },
        { UINT32_C(0x04000072), UINT16_C(0xE0FF), UINT16_C(0xE000) },
        { UINT32_C(0x04000074), UINT16_C(0x47FF), UINT16_C(0x47FF) },
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
    CHECK(read16(machine, UINT32_C(0x04000064)).value == UINT16_C(0x47FF));
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
        tap_check("CH1 guest frequency readback starts at its programmed value",
                  passed && read16(machine, UINT32_C(0x04000064)).value ==
                                UINT16_C(1024));
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
                      read16(machine, UINT32_C(0x04000064)).value ==
                          UINT16_C(1024) &&
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
    gbl_machine *wave_snapshot = NULL;
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

    CHECK(load_wave_bank(refused, 0u, UINT8_C(0x32)));
    CHECK(load_wave_bank(refused, 1u, UINT8_C(0x54)));
    CHECK(configure_wave(refused, 0u, false, UINT16_C(0x2000), 0u,
                         UINT16_C(2047), true));
    CHECK(gbl_audio_state_valid(&refused->audio));
    CHECK(snapshot_machine(refused, &wave_snapshot));
    CHECK(gbl_machine_prepare_time(refused, refused->master_cycles + 16u,
                                   &preview) == GBL_TIME_STATUS_OK);
    preview.audio.wave_ram[1][0] ^= UINT8_C(0x01);
    CHECK(gbl_audio_state_valid(&preview.audio));
    CHECK(!gbl_audio_state_equal(&preview.audio, &preview.audio_prepared));
    CHECK(!gbl_machine_commit_time_checked(refused, &preview));
    CHECK(unchanged_machine(refused, wave_snapshot));
    free(wave_snapshot);
    wave_snapshot = NULL;

    CHECK(snapshot_machine(refused, &wave_snapshot));
    CHECK(gbl_machine_prepare_time(refused, refused->master_cycles + 16u,
                                   &preview) == GBL_TIME_STATUS_OK);
    preview.audio.wave_phase = UINT8_C(32);
    preview.audio_prepared.wave_phase = UINT8_C(32);
    CHECK(gbl_audio_state_equal(&preview.audio, &preview.audio_prepared));
    CHECK(!gbl_audio_state_valid(&preview.audio));
    CHECK(!gbl_machine_commit_time_checked(refused, &preview));
    CHECK(unchanged_machine(refused, wave_snapshot));

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
    free(wave_snapshot);
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

typedef struct wave_bus_observation {
    size_t calls;
    uint64_t cycle;
    uint8_t source;
} wave_bus_observation;

static void wave_bus_sample(const gbl_machine_time_preview *candidate,
                            uint64_t cycle, void *context, int16_t *left,
                            int16_t *right)
{
    wave_bus_observation *observation = context;

    observation->calls++;
    observation->cycle = cycle;
    observation->source = gbl_audio_psg_sample(&candidate->audio, 2u);
    *left = 0;
    *right = 0;
}

static int run_psg_wave_bus(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *snapshot = NULL;
    gbl_bus_result result;
    bool passed = machine != NULL;

    tap_test_number = 0u;
    tap_failure_count = 0u;
    puts("TAP version 13");
    tap_check("machine is initialized", passed);
    if (!passed) {
        puts("1..1");
        return 1;
    }
    passed = write16(machine, UINT32_C(0x04000084), UINT16_C(0x0080)).status ==
                 GBL_BUS_STATUS_OK &&
             write16(machine, UINT32_C(0x04000070), UINT16_C(0x00C0)).status ==
                 GBL_BUS_STATUS_OK;
    tap_check("master enables Wave RAM and selects bank zero for guest access",
              passed);
    result = gbl_bus_write(machine, UINT32_C(0x04000090), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xAB));
    tap_check("guest byte write is reported as supported audio IO",
              result.status == GBL_BUS_STATUS_OK &&
                  result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET);
    result = gbl_bus_read(machine, UINT32_C(0x04000090), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    tap_check("guest byte lane reads its packed Wave RAM value",
              result.status == GBL_BUS_STATUS_OK &&
                  result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET &&
                  result.value == UINT32_C(0xAB));

    {
        const gbl_bus_result half_write = gbl_bus_write(
            machine, UINT32_C(0x04000092), 2u,
            GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xBEEF));
        const gbl_bus_result half_read = gbl_bus_read(
            machine, UINT32_C(0x04000092), 2u,
            GBL_BUS_CLASS_NONSEQUENTIAL);
        const gbl_bus_result word_write = gbl_bus_write(
            machine, UINT32_C(0x04000094), 4u,
            GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x76543210));
        const gbl_bus_result word_read = gbl_bus_read(
            machine, UINT32_C(0x04000094), 4u,
            GBL_BUS_CLASS_NONSEQUENTIAL);
        const gbl_bus_result edge_write = gbl_bus_write(
            machine, UINT32_C(0x0400009E), 2u,
            GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x5A69));
        const gbl_bus_result edge_read = gbl_bus_read(
            machine, UINT32_C(0x0400009E), 2u,
            GBL_BUS_CLASS_NONSEQUENTIAL);

        tap_check("guest halfword and word lanes preserve little-endian order",
                  half_write.status == GBL_BUS_STATUS_OK &&
                      half_write.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET &&
                      half_read.value == UINT32_C(0xBEEF) &&
                      half_read.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET &&
                      word_write.status == GBL_BUS_STATUS_OK &&
                      word_write.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET &&
                      word_read.value == UINT32_C(0x76543210) &&
                      word_read.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET &&
                      edge_write.status == GBL_BUS_STATUS_OK &&
                      edge_read.value == UINT32_C(0x5A69));
    }
    result = gbl_bus_write(machine, UINT32_C(0x0400009F), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x37));
    tap_check("final Wave RAM byte is addressable without wrapping",
              result.status == GBL_BUS_STATUS_OK &&
                  result.outcome == GBL_BUS_OUTCOME_IO_AUDIO_SUBSET &&
                  gbl_bus_read(machine, UINT32_C(0x0400009F), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL).value ==
                      UINT32_C(0x37));

    passed = write16(machine, UINT32_C(0x04000070), UINT16_C(0x0080)).status ==
                 GBL_BUS_STATUS_OK &&
             gbl_bus_write(machine, UINT32_C(0x04000090), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0xCD)).outcome ==
                 GBL_BUS_OUTCOME_IO_AUDIO_SUBSET &&
             write16(machine, UINT32_C(0x04000070), UINT16_C(0x00C0)).status ==
                 GBL_BUS_STATUS_OK &&
             gbl_bus_read(machine, UINT32_C(0x04000090), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL).value ==
                 UINT32_C(0xAB) &&
             write16(machine, UINT32_C(0x04000070), UINT16_C(0x0080)).status ==
                 GBL_BUS_STATUS_OK &&
             gbl_bus_read(machine, UINT32_C(0x04000090), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL).value ==
                 UINT32_C(0xCD);
    tap_check("guest aperture follows selected front bank to expose the other bank",
              passed);

    tap_check("pre-access snapshot is available",
              snapshot_machine(machine, &snapshot));
    result = gbl_bus_write(machine, UINT32_C(0x0400008F), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFFFF));
    tap_check("request crossing the aperture start is refused atomically",
              result.status == GBL_BUS_STATUS_INVALID_ARGUMENT &&
                  snapshot != NULL && unchanged_machine(machine, snapshot));
    result = gbl_bus_write(machine, UINT32_C(0x0400009F), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFFFF));
    tap_check("request crossing the aperture end is refused atomically",
              result.status == GBL_BUS_STATUS_INVALID_ARGUMENT &&
                  snapshot != NULL && unchanged_machine(machine, snapshot));
    result = gbl_bus_read(machine, UINT32_C(0x0400008F), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    tap_check("mixed read is refused without advancing time or open bus",
              result.status == GBL_BUS_STATUS_INVALID_ARGUMENT &&
                  snapshot != NULL && unchanged_machine(machine, snapshot));
    free(snapshot);
    snapshot = NULL;

    {
        gbl_machine_time_preview preview;
        gbl_machine_time_preview preview_before;
        gbl_time_operation operation = {
            .kind = GBL_TIME_OPERATION_BUS_WRITE,
            .width = 2u,
            .address = UINT32_C(0x0400009F),
            .value = UINT32_C(0xFFFF),
            .cycles = UINT64_C(1)
        };
        uint8_t staging[GBL_AUDIO_FRAME_BYTES] = {
            UINT8_C(0xD7), UINT8_C(0xD7), UINT8_C(0xD7), UINT8_C(0xD7)
        };
        size_t staged_bytes = 77u;

        memset(&preview, UINT8_C(0xC3), sizeof(preview));
        preview_before = preview;
        passed = gbl_machine_prepare_operations_audio_with_provider(
                     machine, &operation, 1u, &preview, staging,
                     sizeof(staging), &staged_bytes, NULL, NULL) ==
                     GBL_TIME_STATUS_INVALID_ARGUMENT &&
                 memcmp(&preview, &preview_before, sizeof(preview)) == 0 &&
                 staged_bytes == 77u && staging[0] == UINT8_C(0xD7) &&
                 staging[1] == UINT8_C(0xD7) &&
                 staging[2] == UINT8_C(0xD7) &&
                 staging[3] == UINT8_C(0xD7);
        tap_check("timed raw prepare refuses mixed aperture before staging",
                  passed);
    }

    result = gbl_bus_write(machine, UINT32_C(0x0400008F), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xA5));
    tap_check("ordinary unsupported IO immediately outside aperture is unchanged",
              result.status == GBL_BUS_STATUS_OK &&
                  result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED &&
                  gbl_bus_read(machine, UINT32_C(0x0400008F), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL).outcome ==
                      GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);

    passed = write16(machine, UINT32_C(0x04000084), 0u).status ==
                 GBL_BUS_STATUS_OK &&
             gbl_bus_read(machine, UINT32_C(0x04000090), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL).outcome ==
                 GBL_BUS_OUTCOME_IO_AUDIO_SUBSET &&
             gbl_bus_read(machine, UINT32_C(0x04000090), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL).value ==
                 UINT32_C(0xCD);
    tap_check("Wave RAM persists and remains guest-accessible with master off",
              passed);
    free_machine(machine);

    {
        gbl_machine *integration = new_machine();
        gbl_machine_time_preview preview = { 0 };
        gbl_time_operation operation = {
            .kind = GBL_TIME_OPERATION_BUS_WRITE,
            .width = 1u,
            .address = UINT32_C(0x04000090),
            .value = UINT32_C(0xF2),
            .cycles = UINT64_C(1)
        };
        uint8_t staging[GBL_AUDIO_FRAME_BYTES] = { 0u };
        size_t staged_bytes = 0u;
        wave_bus_observation observation = { 0 };
        size_t offset;
        bool integrated = integration != NULL;

        if (integrated) {
            integrated =
                write16(integration, UINT32_C(0x04000084),
                        UINT16_C(0x0080)).status == GBL_BUS_STATUS_OK &&
                write16(integration, UINT32_C(0x04000070),
                        UINT16_C(0x00C0)).status == GBL_BUS_STATUS_OK;
        }
        for (offset = 0u; offset < 16u && integrated; offset += 4u) {
            integrated = gbl_bus_write(
                             integration,
                             UINT32_C(0x04000090) + (uint32_t)offset, 4u,
                             GBL_BUS_CLASS_NONSEQUENTIAL,
                             UINT32_C(0x22222222)).status == GBL_BUS_STATUS_OK;
        }
        if (integrated) {
            integrated =
                write16(integration, UINT32_C(0x04000070),
                        UINT16_C(0x00A0)).status == GBL_BUS_STATUS_OK;
        }
        for (offset = 0u; offset < 16u && integrated; offset += 4u) {
            integrated = gbl_bus_write(
                             integration,
                             UINT32_C(0x04000090) + (uint32_t)offset, 4u,
                             GBL_BUS_CLASS_NONSEQUENTIAL,
                             UINT32_C(0x33333333)).status == GBL_BUS_STATUS_OK;
        }
        if (integrated) {
            integrated =
                write16(integration, UINT32_C(0x04000070),
                        UINT16_C(0x00A0)).status == GBL_BUS_STATUS_OK &&
                write16(integration, UINT32_C(0x04000072),
                        UINT16_C(0x2000)).status == GBL_BUS_STATUS_OK &&
                write16(integration, UINT32_C(0x04000074),
                        UINT16_C(0x07FF)).status == GBL_BUS_STATUS_OK &&
                gbl_machine_advance_to(integration, UINT64_C(383)) ==
                    GBL_TIME_STATUS_OK &&
                write16(integration, UINT32_C(0x04000074),
                        UINT16_C(0x87FF)).status == GBL_BUS_STATUS_OK &&
                integration->master_cycles == UINT64_C(384) &&
                gbl_machine_advance_to(integration, UINT64_C(511)) ==
                    GBL_TIME_STATUS_OK;
        }
        if (integrated) {
            integrated = gbl_machine_prepare_operations_audio_with_provider(
                             integration, &operation, 1u, &preview, staging,
                             sizeof(staging), &staged_bytes, wave_bus_sample,
                             &observation) == GBL_TIME_STATUS_OK;
        }
        integrated = integrated && observation.calls == 1u &&
                     observation.cycle == UINT64_C(512) &&
                     observation.source == 3u &&
                     staged_bytes == GBL_AUDIO_FRAME_BYTES &&
                     integration->audio.wave_ram[1][0] == UINT8_C(0x33) &&
                     preview.audio.wave_ram[1][0] == UINT8_C(0xF2) &&
                     gbl_machine_commit_time_checked(integration, &preview) &&
                     gbl_audio_psg_sample(&integration->audio, 2u) == 15u;
        tap_check("sample observes old selected-bank nibble before completing Wave write",
                  integrated);
        free_machine(integration);
    }

    printf("1..%u\n", tap_test_number);
    return tap_failure_count == 0u ? 0 : 1;
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
    if (strcmp(argv[1], "mix_tracer") == 0) {
        return run_mix_tracer();
    }
    if (strcmp(argv[1], "mix_vectors") == 0) {
        return run_mix_vectors();
    }
    if (strcmp(argv[1], "mix_bias") == 0) {
        return run_mix_bias();
    }
    if (strcmp(argv[1], "ds_fifo") == 0) {
        return run_ds_fifo();
    }
    if (strcmp(argv[1], "ds_transaction") == 0) {
        return run_ds_transaction();
    }
    if (strcmp(argv[1], "ds_timer") == 0) {
        return run_ds_timer();
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
    if (strcmp(argv[1], "psg_wave_bus") == 0) {
        return run_psg_wave_bus();
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
