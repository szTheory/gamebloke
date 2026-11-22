#include "audio_internal.h"

#define GBL_AUDIO_NOISE_MAX_PERIOD UINT32_C(3670016)

typedef struct gbl_audio_register_policy {
    uint16_t retain_mask;
    uint16_t read_mask;
    uint16_t action_mask;
} gbl_audio_register_policy;

static const gbl_audio_register_policy gbl_audio_register_policies[
    GBL_AUDIO_REGISTER_COUNT] = {
        { UINT16_C(0x007F), UINT16_C(0x007F), UINT16_C(0x0000) },
        { UINT16_C(0xFFFF), UINT16_C(0xFFC0), UINT16_C(0x0000) },
        { UINT16_C(0x47FF), UINT16_C(0x47FF), UINT16_C(0x8000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0xFFFF), UINT16_C(0xFFC0), UINT16_C(0x0000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0x47FF), UINT16_C(0x47FF), UINT16_C(0x8000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0x00E0), UINT16_C(0x00E0), UINT16_C(0x0000) },
        { UINT16_C(0xE0FF), UINT16_C(0xE000), UINT16_C(0x0000) },
        { UINT16_C(0x47FF), UINT16_C(0x47FF), UINT16_C(0x8000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0xFF3F), UINT16_C(0xFF00), UINT16_C(0x0000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0x40FF), UINT16_C(0x40FF), UINT16_C(0x8000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0xFF77), UINT16_C(0xFF77), UINT16_C(0x0000) },
        { UINT16_C(0x770F), UINT16_C(0x770F), UINT16_C(0x8800) },
        { UINT16_C(0x0080), UINT16_C(0x008F), UINT16_C(0x0000) }
    };

static const size_t gbl_audio_frequency_registers[4] = { 2u, 6u, 10u, 14u };
static const size_t gbl_audio_length_registers[4] = { 1u, 4u, 9u, 12u };
static const uint16_t gbl_audio_length_maxima[4] = { 64u, 64u, 256u, 64u };
static const uint16_t gbl_audio_length_masks[4] = {
    UINT16_C(0x003F), UINT16_C(0x003F), UINT16_C(0x00FF), UINT16_C(0x003F)
};

static size_t gbl_audio_register_index(uint32_t address)
{
    return (size_t)((address - GBL_AUDIO_REGISTER_FIRST) / 2u);
}

uint16_t gbl_audio_read_soundbias(const gbl_audio_state *state)
{
    return state == NULL ? 0u
                         : (uint16_t)(state->soundbias & UINT16_C(0xC3FF));
}

void gbl_audio_write_soundbias(gbl_audio_state *state, uint32_t address,
                               uint8_t value)
{
    if (state == NULL || (address != UINT32_C(0x04000088) &&
                          address != UINT32_C(0x04000089))) {
        return;
    }
    if (address == UINT32_C(0x04000088)) {
        state->soundbias = (uint16_t)((state->soundbias & UINT16_C(0xFF00)) |
                                      (uint16_t)value);
    } else {
        state->soundbias = (uint16_t)((state->soundbias & UINT16_C(0x00FF)) |
                                      ((uint16_t)value << 8u));
    }
    state->soundbias &= UINT16_C(0xC3FF);
}

bool gbl_audio_register_address(uint32_t address)
{
    size_t index;

    if (address < GBL_AUDIO_REGISTER_FIRST ||
        address > GBL_AUDIO_REGISTER_LAST) {
        return false;
    }
    index = gbl_audio_register_index(address & ~UINT32_C(1));
    return gbl_audio_register_policies[index].retain_mask != 0u ||
           gbl_audio_register_policies[index].read_mask != 0u ||
           gbl_audio_register_policies[index].action_mask != 0u;
}

bool gbl_audio_wave_span_overlaps(uint32_t address, uint8_t width)
{
    uint32_t last;

    if ((width != 1u && width != 2u && width != 4u) ||
        address > UINT32_MAX - ((uint32_t)width - 1u)) {
        return false;
    }
    last = address + (uint32_t)width - 1u;
    return address <= GBL_AUDIO_WAVE_LAST && last >= GBL_AUDIO_WAVE_FIRST;
}

bool gbl_audio_wave_span_valid(uint32_t address, uint8_t width)
{
    return gbl_audio_wave_span_overlaps(address, width) &&
           address >= GBL_AUDIO_WAVE_FIRST &&
           address <= GBL_AUDIO_WAVE_LAST - ((uint32_t)width - 1u);
}

bool gbl_audio_wave_read_byte(const gbl_audio_state *state,
                              uint32_t address, uint8_t *value)
{
    const unsigned selected_bank =
        state == NULL ? 0u : (unsigned)((state->registers[8] >> 6u) & 1u);

    if (state == NULL || value == NULL ||
        address < GBL_AUDIO_WAVE_FIRST || address > GBL_AUDIO_WAVE_LAST) {
        return false;
    }
    *value = state->wave_ram[selected_bank ^ 1u]
                           [address - GBL_AUDIO_WAVE_FIRST];
    return true;
}

bool gbl_audio_wave_write(gbl_audio_state *state, uint32_t address,
                          uint8_t width, uint32_t value)
{
    const unsigned selected_bank =
        state == NULL ? 0u : (unsigned)((state->registers[8] >> 6u) & 1u);
    unsigned lane;

    if (state == NULL || !gbl_audio_wave_span_valid(address, width)) {
        return false;
    }
    for (lane = 0u; lane < width; ++lane) {
        state->wave_ram[selected_bank ^ 1u]
                        [address - GBL_AUDIO_WAVE_FIRST + lane] =
            (uint8_t)(value >> (lane * 8u));
    }
    return true;
}

uint16_t gbl_audio_read_register(const gbl_audio_state *state,
                                 uint32_t address)
{
    size_t index;
    uint16_t value;

    if (state == NULL || !gbl_audio_register_address(address)) {
        return 0u;
    }
    index = gbl_audio_register_index(address & ~UINT32_C(1));
    /* Length-enable bit 14 is register state; active status is in SOUNDCNT_X. */
    value = (uint16_t)(state->registers[index] &
                       gbl_audio_register_policies[index].read_mask);
    if ((address & ~UINT32_C(1)) == UINT32_C(0x04000084)) {
        value = (uint16_t)((state->registers[index] & UINT16_C(0x0080)) |
                           (uint16_t)(state->active_channels &
                                      UINT8_C(0x0F)));
    }
    return value;
}

static uint32_t gbl_audio_square_period(const gbl_audio_state *state,
                                       unsigned channel_index)
{
    const size_t frequency_register = channel_index == 0u ? 2u : 6u;
    const uint32_t frequency =
        channel_index == 0u
            ? state->sweep_frequency
            : (uint32_t)(state->registers[frequency_register] &
                         UINT16_C(0x07FF));

    return UINT32_C(128) * (UINT32_C(2048) - frequency);
}

static uint32_t gbl_audio_wave_nibble_period(const gbl_audio_state *state)
{
    const uint32_t frequency =
        (uint32_t)(state->registers[10] & UINT16_C(0x07FF));

    return UINT32_C(4) * (UINT32_C(2048) - frequency);
}

static uint8_t gbl_audio_wave_pattern_length(const gbl_audio_state *state)
{
    return (state->registers[8] & UINT16_C(0x0020)) != 0u ? 64u : 32u;
}

static bool gbl_audio_noise_period(const gbl_audio_state *state,
                                   uint32_t *period)
{
    const uint16_t control = state->registers[14];
    const uint32_t shift = (uint32_t)((control >> 4u) & UINT16_C(0x000F));
    const uint32_t ratio = (uint32_t)(control & UINT16_C(0x0007));
    const uint32_t ratio_scale =
        ratio == 0u ? UINT32_C(32) : UINT32_C(64) * ratio;

    if (period == NULL || shift >= 14u) {
        return false;
    }
    *period = ratio_scale << shift;
    return true;
}

static uint16_t gbl_audio_noise_seed(const gbl_audio_state *state)
{
    return (state->registers[14] & UINT16_C(0x0008)) != 0u
               ? UINT16_C(0x0040)
               : UINT16_C(0x4000);
}

static uint16_t gbl_audio_noise_step(uint16_t lfsr, bool width_7bit)
{
    const uint16_t feedback_mask =
        width_7bit ? UINT16_C(0x0060) : UINT16_C(0x6000);
    const uint16_t width_mask =
        width_7bit ? UINT16_C(0x007F) : UINT16_C(0x7FFF);
    const bool feedback = (lfsr & UINT16_C(0x0001)) != 0u;

    lfsr >>= 1u;
    if (feedback) {
        lfsr ^= feedback_mask;
    }
    return (uint16_t)(lfsr & width_mask);
}

static void gbl_audio_advance_noise_steps(gbl_audio_state *state,
                                          uint64_t steps, bool width_7bit)
{
    const uint32_t orbit_period = width_7bit ? UINT32_C(127)
                                              : UINT32_C(32767);

    steps %= orbit_period;
    while (steps != 0u) {
        state->noise_lfsr = gbl_audio_noise_step(state->noise_lfsr,
                                                  width_7bit);
        --steps;
    }
}

static void gbl_audio_advance_noise_cycles(gbl_audio_state *state,
                                            uint64_t elapsed)
{
    uint32_t period;
    uint64_t steps;
    uint32_t remainder;

    if ((state->active_channels & UINT8_C(0x08)) == 0u ||
        !gbl_audio_noise_period(state, &period)) {
        return;
    }
    steps = elapsed / period;
    remainder = (uint32_t)(elapsed % period) +
                state->noise_cycle_remainder;
    if (remainder >= period) {
        remainder -= period;
        ++steps;
    }
    state->noise_cycle_remainder = remainder;
    gbl_audio_advance_noise_steps(
        state, steps,
        (state->registers[14] & UINT16_C(0x0008)) != 0u);
}

static void gbl_audio_advance_wave_cycles(gbl_audio_state *state,
                                          uint64_t elapsed)
{
    uint64_t steps;
    uint32_t period;
    uint32_t remainder;
    uint8_t pattern_length;

    if ((state->active_channels & UINT8_C(0x04)) == 0u) {
        return;
    }
    period = gbl_audio_wave_nibble_period(state);
    pattern_length = gbl_audio_wave_pattern_length(state);
    steps = elapsed / period;
    remainder = (uint32_t)(elapsed % period) +
                state->wave_cycle_remainder;
    if (remainder >= period) {
        remainder -= period;
        ++steps;
    }
    state->wave_cycle_remainder = remainder;
    state->wave_phase = (uint8_t)(
        (state->wave_phase + (uint8_t)(steps % pattern_length)) %
        pattern_length);
}

static bool gbl_audio_square_high_for_phase(const gbl_audio_state *state,
                                             unsigned channel_index)
{
    static const uint8_t duty_numerators[4] = { 1u, 2u, 4u, 6u };
    const size_t duty_register = channel_index == 0u ? 1u : 4u;
    const uint32_t period = gbl_audio_square_period(state, channel_index);
    const uint32_t duty =
        (uint32_t)((state->registers[duty_register] >> 6u) & UINT16_C(0x0003));
    const uint32_t high_cycles =
        (period * duty_numerators[duty]) / UINT32_C(8);

    return state->square_phase[channel_index] < high_cycles;
}

static void gbl_audio_refresh_square_level(gbl_audio_state *state,
                                            unsigned channel_index)
{
    state->square_high[channel_index] =
        gbl_audio_square_high_for_phase(state, channel_index);
}

static bool gbl_audio_channel_length_pending(const gbl_audio_state *state,
                                             unsigned channel_index)
{
    const uint16_t length_enable =
        (uint16_t)(state->registers[
                       gbl_audio_frequency_registers[channel_index]] &
                   UINT16_C(0x4000));

    return (state->active_channels &
            (uint8_t)(UINT8_C(1) << channel_index)) != 0u &&
           state->length_remaining[channel_index] != 0u &&
           length_enable != 0u;
}

static bool gbl_audio_channel_envelope_pending(
    const gbl_audio_state *state, unsigned channel_index)
{
    const bool active =
        (state->active_channels &
         (uint8_t)(UINT8_C(1) << channel_index)) != 0u;
    const bool at_endpoint = state->envelope_increase[channel_index]
                                 ? state->channel_volume[channel_index] >= 15u
                                 : state->channel_volume[channel_index] == 0u;

    return active && channel_index != 2u &&
           state->envelope_period[channel_index] != 0u &&
           state->envelope_timer[channel_index] != 0u && !at_endpoint;
}

static bool gbl_audio_has_pending_sequencer_clock(
    const gbl_audio_state *state, bool *length_pending,
    bool *envelope_pending, bool *sweep_pending)
{
    unsigned channel_index;

    *length_pending = false;
    *envelope_pending = false;
    *sweep_pending =
        (state->active_channels & UINT8_C(0x01)) != 0u &&
        state->sweep_period != 0u;
    for (channel_index = 0u; channel_index < 4u; ++channel_index) {
        *length_pending = *length_pending ||
                          gbl_audio_channel_length_pending(state,
                                                           channel_index);
        *envelope_pending = *envelope_pending ||
                            gbl_audio_channel_envelope_pending(
                                state, channel_index);
    }
    return *length_pending || *envelope_pending || *sweep_pending;
}

static bool gbl_audio_next_sequencer_clock(
    const gbl_audio_state *state, uint64_t target, uint64_t *clock_cycle,
    uint8_t *clock_step)
{
    const uint64_t quotient =
        state->cycle_anchor / GBL_AUDIO_SEQUENCER_CYCLES;
    const uint64_t last_quotient =
        UINT64_MAX / GBL_AUDIO_SEQUENCER_CYCLES;
    const uint8_t first_step = (uint8_t)(quotient & UINT64_C(7));
    const uint64_t first_cycle =
        quotient >= last_quotient
            ? 0u
            : (quotient + UINT64_C(1)) * GBL_AUDIO_SEQUENCER_CYCLES;
    bool length_pending;
    bool envelope_pending;
    bool sweep_pending;
    uint8_t offset;

    if (clock_cycle == NULL || clock_step == NULL || first_cycle == 0u ||
        !gbl_audio_has_pending_sequencer_clock(state, &length_pending,
                                               &envelope_pending,
                                               &sweep_pending)) {
        return false;
    }
    for (offset = 0u; offset < 8u; ++offset) {
        const uint8_t step = (uint8_t)((first_step + offset) & UINT8_C(7));
        const bool length_clock = length_pending && (step & UINT8_C(1)) == 0u;
        const bool envelope_clock = envelope_pending && step == UINT8_C(7);
        const bool sweep_clock =
            sweep_pending && (step == UINT8_C(2) || step == UINT8_C(6));
        const uint64_t cycle_delta =
            (uint64_t)offset * GBL_AUDIO_SEQUENCER_CYCLES;
        uint64_t cycle;

        if (!length_clock && !envelope_clock && !sweep_clock) {
            continue;
        }
        if (first_cycle > UINT64_MAX - cycle_delta) {
            return false;
        }
        cycle = first_cycle + cycle_delta;
        if (cycle > target) {
            return false;
        }
        *clock_cycle = cycle;
        *clock_step = step;
        return true;
    }
    return false;
}

static void gbl_audio_clock_lengths(gbl_audio_state *state)
{
    unsigned channel_index;

    for (channel_index = 0u; channel_index < 4u; ++channel_index) {
        if (gbl_audio_channel_length_pending(state, channel_index)) {
            --state->length_remaining[channel_index];
            if (state->length_remaining[channel_index] == 0u) {
                state->active_channels &=
                    (uint8_t)~(UINT8_C(1) << channel_index);
            }
        }
    }
}

static void gbl_audio_clock_sweep(gbl_audio_state *state)
{
    int32_t next_frequency;
    int32_t delta;

    if ((state->active_channels & UINT8_C(0x01)) == 0u ||
        state->sweep_period == 0u) {
        return;
    }
    ++state->sweep_timer;
    if (state->sweep_timer < state->sweep_period) {
        return;
    }
    state->sweep_timer = 0u;
    if (state->sweep_shift == 0u) {
        return;
    }

    delta = (int32_t)(state->sweep_frequency >> state->sweep_shift);
    next_frequency = (int32_t)state->sweep_frequency;
    next_frequency += state->sweep_decrease ? -delta : delta;
    if (next_frequency > INT32_C(2047)) {
        state->active_channels &= (uint8_t)~UINT8_C(0x01);
        return;
    }

    state->sweep_frequency = (uint16_t)next_frequency;
    state->square_phase[0] %= gbl_audio_square_period(state, 0u);
    gbl_audio_refresh_square_level(state, 0u);
}

static void gbl_audio_clock_envelopes(gbl_audio_state *state)
{
    unsigned channel_index;

    for (channel_index = 0u; channel_index < 4u; ++channel_index) {
        if (!gbl_audio_channel_envelope_pending(state, channel_index)) {
            continue;
        }
        --state->envelope_timer[channel_index];
        if (state->envelope_timer[channel_index] != 0u) {
            continue;
        }
        if (state->envelope_increase[channel_index]) {
            ++state->channel_volume[channel_index];
        } else {
            --state->channel_volume[channel_index];
        }
        if (state->channel_volume[channel_index] == 0u ||
            state->channel_volume[channel_index] == UINT8_C(15)) {
            state->envelope_timer[channel_index] = 0u;
        } else {
            state->envelope_timer[channel_index] =
                state->envelope_period[channel_index];
        }
    }
}

static void gbl_audio_advance_square_cycles(gbl_audio_state *state,
                                            uint64_t elapsed)
{
    unsigned channel_index;

    for (channel_index = 0u; channel_index < 2u; ++channel_index) {
        const uint8_t channel_mask = (uint8_t)(UINT8_C(1) << channel_index);

        if ((state->active_channels & channel_mask) != 0u) {
            const uint32_t period =
                gbl_audio_square_period(state, channel_index);
            const uint32_t phase_delta = (uint32_t)(elapsed % period);

            state->square_phase[channel_index] =
                (state->square_phase[channel_index] + phase_delta) % period;
            gbl_audio_refresh_square_level(state, channel_index);
        }
    }
}

static void gbl_audio_advance_to(gbl_audio_state *state, uint64_t target)
{
    uint64_t clock_cycle;
    uint8_t clock_step;

    while (gbl_audio_next_sequencer_clock(state, target, &clock_cycle,
                                          &clock_step)) {
        const uint64_t elapsed = clock_cycle - state->cycle_anchor;

        gbl_audio_advance_square_cycles(state, elapsed);
        gbl_audio_advance_wave_cycles(state, elapsed);
        gbl_audio_advance_noise_cycles(state, elapsed);
        state->cycle_anchor = clock_cycle;
        if ((clock_step & UINT8_C(1)) == 0u) {
            gbl_audio_clock_lengths(state);
        }
        if (clock_step == UINT8_C(2) || clock_step == UINT8_C(6)) {
            gbl_audio_clock_sweep(state);
        }
        if (clock_step == UINT8_C(7)) {
            gbl_audio_clock_envelopes(state);
        }
    }
    gbl_audio_advance_square_cycles(state, target - state->cycle_anchor);
    gbl_audio_advance_wave_cycles(state, target - state->cycle_anchor);
    gbl_audio_advance_noise_cycles(state, target - state->cycle_anchor);
    state->cycle_anchor = target;
}

static void gbl_audio_reset_channel(gbl_audio_state *state,
                                    unsigned channel_index)
{
    state->active_channels &=
        (uint8_t)~(UINT8_C(1) << channel_index);
    if (channel_index < 2u) {
        state->square_phase[channel_index] = 0u;
        state->square_high[channel_index] = false;
    } else if (channel_index == 2u) {
        state->wave_phase = 0u;
        state->wave_cycle_remainder = 0u;
    }
    state->channel_volume[channel_index] = 0u;
    state->length_remaining[channel_index] = 0u;
    state->envelope_period[channel_index] = 0u;
    state->envelope_timer[channel_index] = 0u;
    state->envelope_increase[channel_index] = false;
    if (channel_index == 0u) {
        state->sweep_frequency = 0u;
        state->sweep_period = 0u;
        state->sweep_timer = 0u;
        state->sweep_shift = 0u;
        state->sweep_decrease = false;
    } else if (channel_index == 3u) {
        state->noise_lfsr = 0u;
        state->noise_cycle_remainder = 0u;
    }
}

static void gbl_audio_trigger_channel(gbl_audio_state *state,
                                      unsigned channel_index)
{
    const size_t length_register =
        gbl_audio_length_registers[channel_index];
    const uint16_t length_setting =
        (uint16_t)(state->registers[length_register] &
                   gbl_audio_length_masks[channel_index]);

    state->length_remaining[channel_index] =
        (uint16_t)(gbl_audio_length_maxima[channel_index] - length_setting);
    if (channel_index != 2u) {
        const size_t envelope_register = length_register;
        const uint16_t control = state->registers[envelope_register];

        state->channel_volume[channel_index] =
            (uint8_t)((control >> 12u) & UINT16_C(0x000F));
        state->envelope_period[channel_index] =
            (uint8_t)((control >> 8u) & UINT16_C(0x0007));
        state->envelope_timer[channel_index] =
            state->envelope_period[channel_index];
        state->envelope_increase[channel_index] =
            (control & UINT16_C(0x0800)) != 0u;
    } else {
        state->channel_volume[channel_index] = 0u;
        state->envelope_period[channel_index] = 0u;
        state->envelope_timer[channel_index] = 0u;
        state->envelope_increase[channel_index] = false;
    }
    if (channel_index == 0u) {
        const uint16_t sweep = state->registers[0];

        state->sweep_frequency =
            (uint16_t)(state->registers[2] & UINT16_C(0x07FF));
        state->sweep_period =
            (uint8_t)((sweep >> 4u) & UINT16_C(0x0007));
        state->sweep_timer = 0u;
        state->sweep_shift = (uint8_t)(sweep & UINT16_C(0x0007));
        state->sweep_decrease = (sweep & UINT16_C(0x0008)) != 0u;
    } else if (channel_index == 3u) {
        state->noise_lfsr = gbl_audio_noise_seed(state);
        state->noise_cycle_remainder = 0u;
    } else if (channel_index == 2u) {
        state->wave_phase = 0u;
        state->wave_cycle_remainder = 0u;
    }
    if (channel_index < 2u) {
        state->square_phase[channel_index] = 0u;
        gbl_audio_refresh_square_level(state, channel_index);
    }
}

bool gbl_audio_state_equal(const gbl_audio_state *left,
                           const gbl_audio_state *right)
{
    size_t index;

    if (left == NULL || right == NULL ||
        left->cycle_anchor != right->cycle_anchor ||
        left->soundbias != right->soundbias ||
        left->active_channels != right->active_channels ||
        left->sweep_frequency != right->sweep_frequency ||
        left->sweep_period != right->sweep_period ||
        left->sweep_timer != right->sweep_timer ||
        left->sweep_shift != right->sweep_shift ||
        left->sweep_decrease != right->sweep_decrease ||
        left->noise_lfsr != right->noise_lfsr ||
        left->noise_cycle_remainder != right->noise_cycle_remainder ||
        left->wave_phase != right->wave_phase ||
        left->wave_cycle_remainder != right->wave_cycle_remainder) {
        return false;
    }
    for (index = 0u; index < GBL_AUDIO_REGISTER_COUNT; ++index) {
        if (left->registers[index] != right->registers[index]) {
            return false;
        }
    }
    for (index = 0u; index < sizeof(left->wave_ram); ++index) {
        if (((const uint8_t *)left->wave_ram)[index] !=
            ((const uint8_t *)right->wave_ram)[index]) {
            return false;
        }
    }
    for (index = 0u; index < 2u; ++index) {
        if (left->square_phase[index] != right->square_phase[index] ||
            left->square_high[index] != right->square_high[index]) {
            return false;
        }
    }
    for (index = 0u; index < 4u; ++index) {
        if (left->channel_volume[index] != right->channel_volume[index]) {
            return false;
        }
        if (left->length_remaining[index] !=
                right->length_remaining[index] ||
            left->envelope_period[index] != right->envelope_period[index] ||
            left->envelope_timer[index] != right->envelope_timer[index] ||
            left->envelope_increase[index] !=
                right->envelope_increase[index]) {
            return false;
        }
    }
    return true;
}

bool gbl_audio_state_valid(const gbl_audio_state *state)
{
    size_t index;
    const bool master_enabled =
        state != NULL && (state->registers[18] & UINT16_C(0x0080)) != 0u;
    const bool noise_width_7bit =
        state != NULL && (state->registers[14] & UINT16_C(0x0008)) != 0u;
    const uint8_t wave_pattern_length =
        state == NULL ? 0u : gbl_audio_wave_pattern_length(state);
    const uint32_t wave_period =
        state == NULL ? 0u : gbl_audio_wave_nibble_period(state);
    const uint16_t noise_width_mask =
        noise_width_7bit ? UINT16_C(0x007F) : UINT16_C(0x7FFF);
    uint32_t noise_period = 0u;
    const bool noise_period_valid =
        state != NULL && gbl_audio_noise_period(state, &noise_period);

    if (state == NULL ||
        state->soundbias != (uint16_t)(state->soundbias & UINT16_C(0xC3FF)) ||
        (state->active_channels & UINT8_C(0xF0)) != 0u ||
        (state->registers[18] & (uint16_t)~UINT16_C(0x0080)) != 0u ||
        state->sweep_frequency > UINT16_C(2047) ||
        state->sweep_period > UINT8_C(7) ||
        state->sweep_shift > UINT8_C(7) ||
        (state->sweep_period == 0u && state->sweep_timer != 0u) ||
        (state->sweep_period != 0u &&
         state->sweep_timer >= state->sweep_period) ||
        state->noise_lfsr > noise_width_mask ||
        state->noise_cycle_remainder >= GBL_AUDIO_NOISE_MAX_PERIOD ||
        (state != NULL &&
         (state->wave_phase >= wave_pattern_length ||
          state->wave_cycle_remainder >= wave_period)) ||
        (noise_period_valid &&
         state->noise_cycle_remainder >= noise_period) ||
        ((state->active_channels & UINT8_C(0x08)) != 0u &&
         state->noise_lfsr == 0u) ||
        (!master_enabled &&
         (state->active_channels != 0u || state->sweep_frequency != 0u ||
          state->sweep_period != 0u || state->sweep_timer != 0u ||
          state->sweep_shift != 0u || state->sweep_decrease ||
          state->noise_lfsr != 0u ||
          state->noise_cycle_remainder != 0u || state->wave_phase != 0u ||
          state->wave_cycle_remainder != 0u)) ||
        ((state->active_channels & UINT8_C(0x01)) != 0u &&
         (state->registers[1] & UINT16_C(0xF800)) == 0u) ||
        ((state->active_channels & UINT8_C(0x02)) != 0u &&
         (state->registers[4] & UINT16_C(0xF800)) == 0u) ||
        ((state->active_channels & UINT8_C(0x04)) != 0u &&
         (state->registers[8] & UINT16_C(0x0080)) == 0u) ||
        ((state->active_channels & UINT8_C(0x08)) != 0u &&
         (state->registers[12] & UINT16_C(0xF800)) == 0u)) {
        return false;
    }
    for (index = 0u; index < GBL_AUDIO_REGISTER_COUNT - 1u; ++index) {
        if ((state->registers[index] &
             (uint16_t)~gbl_audio_register_policies[index].retain_mask) !=
                0u ||
            (!master_enabled && state->registers[index] != 0u)) {
            return false;
        }
    }
    for (index = 0u; index < 2u; ++index) {
        const uint8_t channel_mask = (uint8_t)(UINT8_C(1) << index);
        const uint32_t period =
            gbl_audio_square_period(state, (unsigned)index);

        if (state->square_phase[index] >= period ||
            (!master_enabled &&
             (state->square_phase[index] != 0u ||
              state->square_high[index])) ||
            ((state->active_channels & channel_mask) != 0u &&
             state->square_high[index] !=
                 gbl_audio_square_high_for_phase(state, (unsigned)index))) {
            return false;
        }
    }
    for (index = 0u; index < 4u; ++index) {
        const uint8_t channel_mask = (uint8_t)(UINT8_C(1) << index);

        if (state->channel_volume[index] > UINT8_C(15) ||
            state->length_remaining[index] >
                gbl_audio_length_maxima[index] ||
            ((state->active_channels & channel_mask) != 0u &&
             state->length_remaining[index] == 0u) ||
            state->envelope_period[index] > UINT8_C(7) ||
            state->envelope_timer[index] > state->envelope_period[index] ||
            (state->envelope_period[index] == 0u &&
             state->envelope_timer[index] != 0u) ||
            (!master_enabled &&
             (state->channel_volume[index] != 0u ||
              state->length_remaining[index] != 0u ||
              state->envelope_period[index] != 0u ||
              state->envelope_timer[index] != 0u ||
              state->envelope_increase[index]))) {
            return false;
        }
    }
    return true;
}

static void gbl_audio_clear_channels(gbl_audio_state *state)
{
    size_t index;

    for (index = 0u; index < GBL_AUDIO_REGISTER_COUNT - 1u; ++index) {
        state->registers[index] = 0u;
    }
    state->active_channels = 0u;
    for (index = 0u; index < 4u; ++index) {
        gbl_audio_reset_channel(state, (unsigned)index);
    }
}

void gbl_audio_apply_register_write(gbl_audio_state *state,
                                    uint32_t address, uint16_t value,
                                    uint8_t lane_mask)
{
    const uint32_t register_address = address & ~UINT32_C(1);
    size_t index;
    uint16_t prior;
    uint16_t merged;
    uint16_t writable_mask;
    uint16_t next;
    uint16_t action;
    bool master_enabled;
    unsigned channel_index;

    if (state == NULL || !gbl_audio_register_address(register_address) ||
        lane_mask == 0u) {
        return;
    }
    index = gbl_audio_register_index(register_address);
    prior = state->registers[index];
    merged = prior;
    if ((lane_mask & UINT8_C(1)) != 0u) {
        merged = (uint16_t)((merged & UINT16_C(0xFF00)) |
                            (value & UINT16_C(0x00FF)));
    }
    if ((lane_mask & UINT8_C(2)) != 0u) {
        merged = (uint16_t)((merged & UINT16_C(0x00FF)) |
                            (value & UINT16_C(0xFF00)));
    }
    master_enabled = (state->registers[18] & UINT16_C(0x0080)) != 0u;
    if (index == 18u) {
        const bool was_enabled = master_enabled;
        const bool next_enabled =
            (merged & UINT16_C(0x0080)) != 0u;

        state->registers[18] = next_enabled ? UINT16_C(0x0080) : 0u;
        if (was_enabled && !next_enabled) {
            gbl_audio_clear_channels(state);
        }
        return;
    }
    if (!master_enabled) {
        return;
    }

    writable_mask = gbl_audio_register_policies[index].retain_mask;
    next = (uint16_t)((prior & (uint16_t)~writable_mask) |
                      (merged & writable_mask));
    action = (uint16_t)(merged &
                        gbl_audio_register_policies[index].action_mask);
    state->registers[index] = next;

    if (index == 8u &&
        ((prior ^ next) & UINT16_C(0x0020)) != 0u) {
        state->wave_phase = (uint8_t)(
            state->wave_phase % gbl_audio_wave_pattern_length(state));
    }

    if (index == 14u) {
        const bool previous_width_7bit =
            (prior & UINT16_C(0x0008)) != 0u;
        const bool next_width_7bit =
            (next & UINT16_C(0x0008)) != 0u;
        uint32_t noise_period;

        if (previous_width_7bit != next_width_7bit) {
            state->noise_lfsr = gbl_audio_noise_seed(state);
            state->noise_cycle_remainder = 0u;
        } else if (gbl_audio_noise_period(state, &noise_period)) {
            state->noise_cycle_remainder %= noise_period;
        }
    }

    if (index == 2u && (action & UINT16_C(0x8000)) != 0u &&
        (state->registers[1] & UINT16_C(0xF800)) != 0u) {
        state->active_channels |= UINT8_C(0x01);
    } else if (index == 6u && (action & UINT16_C(0x8000)) != 0u &&
               (state->registers[4] & UINT16_C(0xF800)) != 0u) {
        state->active_channels |= UINT8_C(0x02);
    } else if (index == 10u && (action & UINT16_C(0x8000)) != 0u &&
               (state->registers[8] & UINT16_C(0x0080)) != 0u) {
        state->active_channels |= UINT8_C(0x04);
    } else if (index == 14u && (action & UINT16_C(0x8000)) != 0u &&
               (state->registers[12] & UINT16_C(0xF800)) != 0u) {
        state->active_channels |= UINT8_C(0x08);
    }

    if (index == 1u && (next & UINT16_C(0xF800)) == 0u) {
        state->active_channels &= (uint8_t)~UINT8_C(0x01);
    } else if (index == 4u && (next & UINT16_C(0xF800)) == 0u) {
        state->active_channels &= (uint8_t)~UINT8_C(0x02);
    } else if (index == 8u && (lane_mask & UINT8_C(1)) != 0u &&
               (merged & UINT16_C(0x0080)) == 0u) {
        state->active_channels &= (uint8_t)~UINT8_C(0x04);
    } else if (index == 12u && (next & UINT16_C(0xF800)) == 0u) {
        state->active_channels &= (uint8_t)~UINT8_C(0x08);
    }

    if (index == 1u && (next & UINT16_C(0xF800)) == 0u) {
        gbl_audio_reset_channel(state, 0u);
    } else if (index == 4u && (next & UINT16_C(0xF800)) == 0u) {
        gbl_audio_reset_channel(state, 1u);
    } else if (index == 8u && (lane_mask & UINT8_C(1)) != 0u &&
               (merged & UINT16_C(0x0080)) == 0u) {
        gbl_audio_reset_channel(state, 2u);
    } else if (index == 12u && (next & UINT16_C(0xF800)) == 0u) {
        gbl_audio_reset_channel(state, 3u);
    } else if (index == 1u &&
               (state->active_channels & UINT8_C(0x01)) != 0u) {
        gbl_audio_refresh_square_level(state, 0u);
    } else if (index == 4u &&
               (state->active_channels & UINT8_C(0x02)) != 0u) {
        gbl_audio_refresh_square_level(state, 1u);
    }

    for (channel_index = 0u; channel_index < 4u; ++channel_index) {
        const uint8_t channel_mask =
            (uint8_t)(UINT8_C(1) << channel_index);

        if (index != gbl_audio_frequency_registers[channel_index]) {
            continue;
        }
        if (channel_index == 0u &&
            ((action & UINT16_C(0x8000)) == 0u ||
             (state->active_channels & channel_mask) == 0u)) {
            state->sweep_frequency =
                (uint16_t)(next & UINT16_C(0x07FF));
        }
        if ((action & UINT16_C(0x8000)) != 0u &&
            (state->active_channels & channel_mask) != 0u) {
            gbl_audio_trigger_channel(state, channel_index);
        }
        if (channel_index < 2u) {
            if ((action & UINT16_C(0x8000)) == 0u ||
                (state->active_channels & channel_mask) == 0u) {
                state->square_phase[channel_index] %=
                    gbl_audio_square_period(state, channel_index);
            }
            if ((state->active_channels & channel_mask) != 0u) {
                gbl_audio_refresh_square_level(state, channel_index);
            }
        } else if (channel_index == 2u &&
                   (action & UINT16_C(0x8000)) == 0u) {
            state->wave_cycle_remainder %=
                gbl_audio_wave_nibble_period(state);
        }
    }
}

void gbl_audio_sample(const gbl_audio_state *state, uint64_t cycle,
                      int16_t *left, int16_t *right)
{
    /* PSG channel state is modeled separately; PCM mixing remains deferred. */
    (void)state;
    (void)cycle;
    if (left != NULL) {
        *left = 0;
    }
    if (right != NULL) {
        *right = 0;
    }
}

uint8_t gbl_audio_psg_sample(const gbl_audio_state *state,
                             unsigned channel_index)
{
    if (state == NULL || channel_index >= 4u ||
        (state->active_channels & (uint8_t)(UINT8_C(1) << channel_index)) ==
            0u) {
        return 0u;
    }
    if (channel_index == 3u) {
        return (state->noise_lfsr & UINT16_C(0x0001)) == 0u
                   ? state->channel_volume[channel_index]
                   : 0u;
    }
    if (channel_index == 2u) {
        const uint16_t wave_control = state->registers[8];
        const uint16_t level_control = state->registers[9];
        const uint8_t phase = state->wave_phase;
        const uint8_t pattern_length =
            gbl_audio_wave_pattern_length(state);
        unsigned front_bank;
        unsigned bank;
        unsigned nibble_index;
        uint8_t packed;
        uint8_t sample;
        unsigned volume;

        if (phase >= pattern_length) {
            return 0u;
        }
        front_bank = (unsigned)((wave_control >> 6u) & UINT16_C(1));
        bank = front_bank ^ (unsigned)(phase >> 5u);
        nibble_index = (unsigned)(phase & UINT8_C(31));
        packed = state->wave_ram[bank][nibble_index >> 1u];
        sample = (nibble_index & 1u) == 0u
                     ? (uint8_t)(packed >> 4u)
                     : (uint8_t)(packed & UINT8_C(0x0F));
        volume = (unsigned)((level_control >> 13u) & UINT16_C(3));
        if ((level_control & UINT16_C(0x8000)) != 0u) {
            return (uint8_t)(((unsigned)sample * 3u) / 4u);
        }
        if (volume == 0u) {
            return 0u;
        }
        return (uint8_t)(sample >> (volume - 1u));
    }
    if (!state->square_high[channel_index]) {
        return 0u;
    }
    return state->channel_volume[channel_index];
}

static void gbl_audio_write_s16le(uint8_t *destination, int16_t value)
{
    const uint16_t bits = (uint16_t)value;

    destination[0] = (uint8_t)bits;
    destination[1] = (uint8_t)(bits >> 8u);
}

void gbl_audio_write_frame(const gbl_audio_state *state, uint64_t cycle,
                           uint8_t *destination)
{
    int16_t left;
    int16_t right;

    if (destination == NULL) {
        return;
    }
    gbl_audio_sample(state, cycle, &left, &right);
    gbl_audio_write_s16le(destination, left);
    gbl_audio_write_s16le(destination + 2u, right);
}

static void gbl_audio_write_frame_candidate(
    const gbl_machine_time_preview *candidate, uint64_t cycle,
    uint8_t *destination, gbl_audio_sample_provider provider,
    void *provider_context)
{
    int16_t left = 0;
    int16_t right = 0;

    if (provider == NULL) {
        gbl_audio_sample(&candidate->audio, cycle, &left, &right);
    } else {
        provider(candidate, cycle, provider_context, &left, &right);
    }
    gbl_audio_write_s16le(destination, left);
    gbl_audio_write_s16le(destination + 2u, right);
}

gbl_time_status gbl_audio_advance_span(
    gbl_machine_time_preview *candidate, uint64_t target, uint8_t *staging,
    size_t staging_capacity, size_t *staged_bytes,
    gbl_audio_sample_provider provider, void *provider_context)
{
    gbl_audio_state *state = candidate == NULL ? NULL : &candidate->audio;
    const uint64_t first_frame = state == NULL
                                     ? 0u
                                     : state->cycle_anchor /
                                           GBL_AUDIO_SAMPLE_CYCLES;
    uint64_t final_frame;
    uint64_t frame_count;
    size_t byte_count;
    uint64_t frame;

    if (state == NULL || staged_bytes == NULL ||
        (staging == NULL && staging_capacity != 0u)) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (target < state->cycle_anchor) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    final_frame = target / GBL_AUDIO_SAMPLE_CYCLES;
    frame_count = final_frame - first_frame;
    if (staging == NULL) {
        gbl_audio_advance_to(state, target);
        return GBL_TIME_STATUS_OK;
    }
    if (frame_count > (uint64_t)(SIZE_MAX / GBL_AUDIO_FRAME_BYTES)) {
        return GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT;
    }
    byte_count = (size_t)frame_count * GBL_AUDIO_FRAME_BYTES;
    if (*staged_bytes > staging_capacity ||
        byte_count > staging_capacity - *staged_bytes) {
        return GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT;
    }
    for (frame = first_frame + 1u; frame <= final_frame; ++frame) {
        const uint64_t cycle = frame * GBL_AUDIO_SAMPLE_CYCLES;

        gbl_audio_advance_to(state, cycle);
        gbl_audio_write_frame_candidate(candidate, cycle,
                                        staging + *staged_bytes, provider,
                                        provider_context);
        *staged_bytes += GBL_AUDIO_FRAME_BYTES;
    }
    gbl_audio_advance_to(state, target);
    return GBL_TIME_STATUS_OK;
}
