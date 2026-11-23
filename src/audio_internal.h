#ifndef GBL_AUDIO_INTERNAL_H
#define GBL_AUDIO_INTERNAL_H

#include "gamebloke/gamebloke.h"

#define GBL_AUDIO_SAMPLE_RATE UINT32_C(32768)
#define GBL_AUDIO_SAMPLE_CYCLES UINT64_C(512)
#define GBL_AUDIO_SEQUENCER_CYCLES UINT64_C(32768)
#define GBL_AUDIO_FRAME_BYTES 4u
#define GBL_AUDIO_MAX_CAPTURE_CYCLES UINT64_C(16777216)
#define GBL_AUDIO_MAX_CAPTURE_FRAMES UINT64_C(32768)
#define GBL_AUDIO_MAX_CAPTURE_BYTES ((size_t)131072u)
#define GBL_AUDIO_REGISTER_FIRST UINT32_C(0x04000060)
#define GBL_AUDIO_REGISTER_LAST UINT32_C(0x04000085)
#define GBL_AUDIO_REGISTER_COUNT 19u
#define GBL_AUDIO_WAVE_FIRST UINT32_C(0x04000090)
#define GBL_AUDIO_WAVE_LAST UINT32_C(0x0400009F)
#define GBL_AUDIO_FIFO_FIRST UINT32_C(0x040000A0)
#define GBL_AUDIO_FIFO_LAST UINT32_C(0x040000A7)
#define GBL_AUDIO_FIFO_CAPACITY 32u
#define GBL_AUDIO_FIFO_COUNT 2u

typedef void (*gbl_audio_sample_provider)(
    const gbl_machine_time_preview *candidate, uint64_t cycle,
    void *context, int16_t *left, int16_t *right);

typedef struct gbl_audio_capture {
    gbl_machine *machine;
    uint8_t *destination;
    size_t capacity;
    uint64_t initial_cycle;
    uint64_t last_accepted_cycle;
    size_t bytes_written;
} gbl_audio_capture;

/* Capture storage remains caller-owned and must be writable for capacity
 * bytes for the session lifetime. A new begin explicitly starts a new bounded
 * session; callers must end it before resetting the machine. */
gbl_time_status gbl_audio_capture_begin(gbl_machine *machine,
                                        gbl_audio_capture *capture,
                                        uint8_t *destination,
                                        size_t capacity);
gbl_time_status gbl_machine_advance_to_audio(gbl_machine *machine,
                                              uint64_t target,
                                              gbl_audio_capture *capture);
bool gbl_audio_capture_validate(const gbl_machine *machine,
                                const gbl_audio_capture *capture);
gbl_run_status gbl_cpu_run_audio(gbl_machine *machine,
                                 const gbl_run_options *options,
                                 gbl_run_result *result,
                                 gbl_audio_capture *capture,
                                 uint8_t *staging,
                                 size_t staging_capacity,
                                 uint64_t capture_end_cycle);

/* Optional staging seam shared by capture and later CPU/DMA operations. */
gbl_time_status gbl_machine_prepare_operations_audio(
    const gbl_machine *machine, const gbl_time_operation *operations,
    size_t operation_count, gbl_machine_time_preview *preview,
    uint8_t *staging, size_t staging_capacity, size_t *staged_bytes);
gbl_time_status gbl_machine_prepare_operations_audio_with_provider(
    const gbl_machine *machine, const gbl_time_operation *operations,
    size_t operation_count, gbl_machine_time_preview *preview,
    uint8_t *staging, size_t staging_capacity, size_t *staged_bytes,
    gbl_audio_sample_provider provider, void *provider_context);
bool gbl_machine_commit_time_checked(
    gbl_machine *machine, const gbl_machine_time_preview *preview);

uint16_t gbl_audio_read_soundbias(const gbl_audio_state *state);
bool gbl_audio_register_address(uint32_t address);
bool gbl_audio_wave_span_overlaps(uint32_t address, uint8_t width);
bool gbl_audio_wave_span_valid(uint32_t address, uint8_t width);
bool gbl_audio_wave_read_byte(const gbl_audio_state *state,
                              uint32_t address, uint8_t *value);
bool gbl_audio_wave_write(gbl_audio_state *state, uint32_t address,
                          uint8_t width, uint32_t value);
bool gbl_audio_fifo_address(uint32_t address);
bool gbl_audio_fifo_span_overlaps(uint32_t address, uint8_t width);
bool gbl_audio_fifo_span_valid(uint32_t address, uint8_t width);
bool gbl_audio_fifo_write(gbl_audio_state *state, uint32_t address,
                          uint8_t width, uint32_t value);
uint8_t gbl_audio_fifo_clock_timer(gbl_audio_state *state,
                                   uint8_t timer_index);
uint16_t gbl_audio_read_register(const gbl_audio_state *state,
                                 uint32_t address);
bool gbl_audio_state_equal(const gbl_audio_state *left,
                           const gbl_audio_state *right);
bool gbl_audio_state_valid(const gbl_audio_state *state);
uint8_t gbl_audio_apply_register_write(gbl_audio_state *state,
                                       uint32_t address, uint16_t value,
                                       uint8_t lane_mask);
void gbl_audio_write_soundbias(gbl_audio_state *state, uint32_t address,
                               uint8_t value);
void gbl_audio_sample(const gbl_audio_state *state, uint64_t cycle,
                      int16_t *left, int16_t *right);
uint8_t gbl_audio_psg_sample(const gbl_audio_state *state,
                             unsigned channel_index);
void gbl_audio_write_frame(const gbl_audio_state *state, uint64_t cycle,
                           uint8_t *destination);
gbl_time_status gbl_audio_advance_span(
    gbl_machine_time_preview *candidate, uint64_t target, uint8_t *staging,
    size_t staging_capacity, size_t *staged_bytes,
    gbl_audio_sample_provider provider, void *provider_context);

#endif
