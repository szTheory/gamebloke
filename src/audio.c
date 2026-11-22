#include "audio_internal.h"

typedef struct gbl_audio_register_policy {
    uint16_t retain_mask;
    uint16_t read_mask;
    uint16_t action_mask;
} gbl_audio_register_policy;

static const gbl_audio_register_policy gbl_audio_register_policies[
    GBL_AUDIO_REGISTER_COUNT] = {
        { UINT16_C(0x007F), UINT16_C(0x007F), UINT16_C(0x0000) },
        { UINT16_C(0xFFFF), UINT16_C(0xFFC0), UINT16_C(0x0000) },
        { UINT16_C(0x47FF), UINT16_C(0x4000), UINT16_C(0x8000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0xFFFF), UINT16_C(0xFFC0), UINT16_C(0x0000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0x47FF), UINT16_C(0x4000), UINT16_C(0x8000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0x00E0), UINT16_C(0x00E0), UINT16_C(0x0000) },
        { UINT16_C(0xE0FF), UINT16_C(0xE000), UINT16_C(0x0000) },
        { UINT16_C(0x47FF), UINT16_C(0x4000), UINT16_C(0x8000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0xFF3F), UINT16_C(0xFF00), UINT16_C(0x0000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0x40FF), UINT16_C(0x40FF), UINT16_C(0x8000) },
        { UINT16_C(0x0000), UINT16_C(0x0000), UINT16_C(0x0000) },
        { UINT16_C(0xFF77), UINT16_C(0xFF77), UINT16_C(0x0000) },
        { UINT16_C(0x770F), UINT16_C(0x770F), UINT16_C(0x8800) },
        { UINT16_C(0x0080), UINT16_C(0x008F), UINT16_C(0x0000) }
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

uint16_t gbl_audio_read_register(const gbl_audio_state *state,
                                 uint32_t address)
{
    size_t index;
    uint16_t value;

    if (state == NULL || !gbl_audio_register_address(address)) {
        return 0u;
    }
    index = gbl_audio_register_index(address & ~UINT32_C(1));
    value = (uint16_t)(state->registers[index] &
                       gbl_audio_register_policies[index].read_mask);
    if ((address & ~UINT32_C(1)) == UINT32_C(0x04000064) ||
        (address & ~UINT32_C(1)) == UINT32_C(0x0400006C) ||
        (address & ~UINT32_C(1)) == UINT32_C(0x04000074) ||
        (address & ~UINT32_C(1)) == UINT32_C(0x0400007C)) {
        const uint32_t register_address = address & ~UINT32_C(1);
        const uint8_t channel = register_address == UINT32_C(0x04000064)
                                    ? 0u
                                    : register_address == UINT32_C(0x0400006C)
                                          ? 1u
                                          : register_address ==
                                                    UINT32_C(0x04000074)
                                                ? 2u
                                                : 3u;

        if ((state->active_channels & (uint8_t)(1u << channel)) != 0u) {
            value |= UINT16_C(0x4000);
        }
    } else if ((address & ~UINT32_C(1)) == UINT32_C(0x04000084)) {
        value = (uint16_t)((state->registers[index] & UINT16_C(0x0080)) |
                           (uint16_t)(state->active_channels &
                                      UINT8_C(0x0F)));
    }
    return value;
}

bool gbl_audio_state_equal(const gbl_audio_state *left,
                           const gbl_audio_state *right)
{
    size_t index;

    if (left == NULL || right == NULL ||
        left->cycle_anchor != right->cycle_anchor ||
        left->soundbias != right->soundbias ||
        left->active_channels != right->active_channels) {
        return false;
    }
    for (index = 0u; index < GBL_AUDIO_REGISTER_COUNT; ++index) {
        if (left->registers[index] != right->registers[index]) {
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

    if (state == NULL ||
        state->soundbias != (uint16_t)(state->soundbias & UINT16_C(0xC3FF)) ||
        (state->active_channels & UINT8_C(0xF0)) != 0u ||
        (state->registers[18] & (uint16_t)~UINT16_C(0x0080)) != 0u ||
        (!master_enabled && state->active_channels != 0u) ||
        ((state->active_channels & UINT8_C(0x01)) != 0u &&
         (state->registers[1] & UINT16_C(0xF800)) == 0u) ||
        ((state->active_channels & UINT8_C(0x02)) != 0u &&
         (state->registers[4] & UINT16_C(0xF800)) == 0u) ||
        ((state->active_channels & UINT8_C(0x08)) != 0u &&
         (state->registers[12] & UINT16_C(0xF800)) == 0u)) {
        return false;
    }
    for (index = 0u; index < GBL_AUDIO_REGISTER_COUNT - 1u; ++index) {
        if ((state->registers[index] &
             (uint16_t)~gbl_audio_register_policies[index].retain_mask) !=
                0u ||
            ((index == 2u || index == 6u || index == 10u || index == 14u) &&
             (state->registers[index] & UINT16_C(0x4000)) != 0u) ||
            (!master_enabled && state->registers[index] != 0u)) {
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
    if (index == 2u || index == 6u || index == 10u || index == 14u) {
        writable_mask &= (uint16_t)~UINT16_C(0x4000);
    }
    next = (uint16_t)((prior & (uint16_t)~writable_mask) |
                      (merged & writable_mask));
    action = (uint16_t)(merged &
                        gbl_audio_register_policies[index].action_mask);
    state->registers[index] = next;

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
    if (index == 8u && (lane_mask & UINT8_C(1)) != 0u &&
        (merged & UINT16_C(0x0080)) != 0u) {
        state->active_channels |= UINT8_C(0x04);
    }
}

void gbl_audio_sample(const gbl_audio_state *state, uint64_t cycle,
                      int16_t *left, int16_t *right)
{
    /* This is the source-dependent PSG/Direct Sound projection seam. Until
     * those sources are modeled, each fixed-grid pre-PWM sample is signed 0. */
    (void)state;
    (void)cycle;
    if (left != NULL) {
        *left = 0;
    }
    if (right != NULL) {
        *right = 0;
    }
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
        state->cycle_anchor = target;
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

        gbl_audio_write_frame_candidate(candidate, cycle,
                                        staging + *staged_bytes, provider,
                                        provider_context);
        *staged_bytes += GBL_AUDIO_FRAME_BYTES;
    }
    state->cycle_anchor = target;
    return GBL_TIME_STATUS_OK;
}
