#include "audio_internal.h"

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

gbl_time_status gbl_audio_advance_span(
    gbl_audio_state *state, uint64_t target, uint8_t *staging,
    size_t staging_capacity, size_t *staged_bytes)
{
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

        gbl_audio_write_frame(state, cycle, staging + *staged_bytes);
        *staged_bytes += GBL_AUDIO_FRAME_BYTES;
    }
    state->cycle_anchor = target;
    return GBL_TIME_STATUS_OK;
}
