#ifndef GAMEBLOKE_RUNNER_RUN_H
#define GAMEBLOKE_RUNNER_RUN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "fixture_index.h"
#include "gamebloke/gamebloke.h"
#include "sha256.h"

#define GBL_RUNNER_MAX_ROM_BYTES ((size_t)UINT32_C(0x02000000))
#define GBL_RUNNER_MAX_TRACE_RECORDS ((size_t)65536u)
#define GBL_RUNNER_FIXTURE_ID_CAPACITY 128u
#define GBL_RUNNER_SOURCE_COMMIT_CAPACITY 41u
#define GBL_RUNNER_PROFILE_VERSION UINT32_C(16)
#define GBL_RUNNER_FRAME_CYCLES UINT64_C(280896)
#define GBL_RUNNER_VIDEO_BYTES ((size_t)GBL_PPU_VIDEO_BYTES)
#define GBL_RUNNER_AUDIO_MAX_BYTES ((size_t)UINT32_C(131072))

typedef void *(*gbl_runner_allocate_fn)(void *context, size_t size);
typedef void (*gbl_runner_release_fn)(void *context, void *pointer,
                                      size_t size);
typedef bool (*gbl_runner_cancelled_fn)(void *context);

typedef struct gbl_runner_allocator {
    void *context;
    gbl_runner_allocate_fn allocate;
    gbl_runner_release_fn release;
} gbl_runner_allocator;

typedef struct gbl_runner_options {
    const char *rom_path;
    const char *fixture_id;
    uint64_t cycle_budget;
    uint32_t trace_mask;
    size_t trace_capacity;
    const char *trace_output_path;
    gbl_runner_cancelled_fn cancelled;
    void *cancel_context;
    /* Logical keypad records are copied before any ROM I/O. When absent, the
     * pointer and count must both be empty; a present empty script is valid. */
    bool input_timeline_present;
    const gbl_input_event *input_events;
    size_t input_event_count;
    /* A non-NULL path requests one valid raw mode-3 frame. */
    const char *video_output_path;
    /* A non-NULL path requests a bounded reset-anchored PCM interval. */
    const char *audio_output_path;
    /* Zero selects the validated cycle budget when audio output is requested. */
    uint64_t audio_end_cycles;
    /* Both callbacks must be supplied together. Both NULL selects malloc/free. */
    gbl_runner_allocator allocator;
} gbl_runner_options;

typedef struct gbl_runner_register_observation {
    bool present;
    bool observed;
    char source_commit[GBL_RUNNER_SOURCE_COMMIT_CAPACITY];
    uint32_t entry_address;
    char instruction_set[6];
    unsigned register_index;
    bool has_value;
    uint32_t value;
} gbl_runner_register_observation;

typedef struct gbl_runner_phase8_observation {
    bool observed;
    uint16_t interrupt_flags;
    uint16_t interrupt_enable;
    uint16_t interrupt_master_enable;
    bool has_irq_sample;
    gbl_cpu_irq_observation irq_sample;
    gbl_exception_kind last_exception;
    gbl_machine_power_state power_state;
    gbl_cpu_power_cause power_cause;
    bool has_wake;
    uint16_t wake_source_mask;
    uint64_t wake_cycle;
    uint16_t keyinput;
    uint16_t key_control;
    bool key_condition;
    bool key_irq_condition;
    size_t applied_input_events;
    size_t pending_input_events;
    uint64_t wait_cycles;
} gbl_runner_phase8_observation;

typedef struct gbl_runner_phase9_observation {
    bool observed;
    uint16_t waitcnt;
    gbl_machine_bus_owner bus_owner;
    uint64_t machine_cycles;
    gbl_dma_channel_state channels[GBL_DMA_CHANNEL_COUNT];
    bool latch_fallback_known;
    bool latch_fallback_observed;
} gbl_runner_phase9_observation;

typedef struct gbl_runner_phase10_observation {
    bool observed;
    uint32_t profile_version;
    bool frame_started;
    uint16_t line;
    uint64_t line_cycle;
    uint64_t machine_cycles;
    uint64_t frame_epoch;
    bool has_frame_epoch;
    bool supported_controls;
    gbl_video_capture_state capture_state;
    gbl_video_invalid_reason capture_reason;
    uint64_t sample_cycle;
    bool has_sample_cycle;
    uint64_t completion_cycle;
    bool has_completion_cycle;
    bool hblank_trigger_dependency;
    bool video_requested;
    bool video_emitted;
    bool has_video_sha256;
    char video_sha256[GBL_SHA256_HEX_CAPACITY];
    char video_output_error[64];
    uint8_t *video_bytes;
    size_t video_length;
} gbl_runner_phase10_observation;

typedef enum gbl_runner_audio_capture_state {
    GBL_RUNNER_AUDIO_CAPTURE_NOT_REQUESTED = 0,
    GBL_RUNNER_AUDIO_CAPTURE_UNAVAILABLE,
    GBL_RUNNER_AUDIO_CAPTURE_INCOMPLETE,
    GBL_RUNNER_AUDIO_CAPTURE_COMPLETE,
    GBL_RUNNER_AUDIO_CAPTURE_FAILED
} gbl_runner_audio_capture_state;

typedef enum gbl_runner_audio_artifact_state {
    GBL_RUNNER_AUDIO_ARTIFACT_NOT_REQUESTED = 0,
    GBL_RUNNER_AUDIO_ARTIFACT_NOT_WRITTEN,
    GBL_RUNNER_AUDIO_ARTIFACT_FAILED,
    GBL_RUNNER_AUDIO_ARTIFACT_EMITTED
} gbl_runner_audio_artifact_state;

typedef struct gbl_runner_audio_observation {
    bool requested;
    gbl_runner_audio_capture_state capture_state;
    bool has_requested_interval;
    uint64_t requested_start_cycle;
    uint64_t requested_end_cycle;
    bool has_actual_interval;
    uint64_t actual_start_cycle;
    uint64_t actual_end_cycle;
    bool interval_complete;
    bool has_frame_count;
    uint64_t frame_count;
    bool has_byte_count;
    size_t byte_count;
    uint8_t *pcm_bytes;
    size_t pcm_capacity;
    uint8_t *scratch_bytes;
    size_t scratch_capacity;
    gbl_runner_audio_artifact_state artifact_state;
    bool artifact_emitted;
    bool has_artifact_byte_count;
    size_t artifact_byte_count;
    bool has_sha256;
    char sha256[GBL_SHA256_HEX_CAPACITY];
    char artifact_error[64];
} gbl_runner_audio_observation;

typedef enum gbl_runner_status {
    GBL_RUNNER_PASS = 0,
    GBL_RUNNER_GUEST_FAIL,
    GBL_RUNNER_GUEST_STOP,
    GBL_RUNNER_HARNESS_FAIL,
    GBL_RUNNER_TIMEOUT,
    GBL_RUNNER_CANCELLED,
    GBL_RUNNER_MISSING,
    GBL_RUNNER_SKIPPED,
    GBL_RUNNER_REFUSED
} gbl_runner_status;

typedef struct gbl_runner_result {
    gbl_runner_status status;
    char reason[96];
    bool has_refusal;
    char refusal_code[64];
    char refusal_postcondition[96];

    bool has_fixture_id;
    char fixture_id[GBL_RUNNER_FIXTURE_ID_CAPACITY];
    const gbl_fixture_metadata *metadata;
    bool ran;
    bool has_rom_sha256;
    char rom_sha256[GBL_SHA256_HEX_CAPACITY];
    bool has_expected_sha256;
    char expected_sha256[GBL_SHA256_HEX_CAPACITY];

    gbl_mailbox_result semantic;
    gbl_runner_register_observation register_observation;
    uint64_t cycles;
    uint64_t instructions;
    uint64_t budget_cycles;
    uint64_t bus_cycles;
    uint64_t internal_cycles;
    uint64_t wait_cycles;
    uint64_t dma_startup_cycles;
    uint64_t dma_source_cycles;
    uint64_t dma_destination_cycles;
    uint64_t dma_completed_units;
    uint64_t dma_pause_cycles;
    bool input_timeline_present;
    bool has_input_timeline_sha256;
    char input_timeline_sha256[GBL_SHA256_HEX_CAPACITY];
    gbl_runner_phase8_observation phase8;
    gbl_runner_phase9_observation phase9;
    gbl_runner_phase10_observation phase10;
    gbl_runner_audio_observation audio;
    bool has_timer0_if_request;
    bool timer0_if_request;
    char termination[64];

    uint32_t trace_mask;
    size_t trace_capacity;
    size_t trace_count;
    bool trace_complete;
    bool trace_emitted;
    gbl_trace_record *trace_records;

    gbl_runner_allocator allocator;
} gbl_runner_result;

/* Always returns a result document state for a non-NULL result pointer.
 * A cycle/frame budget is a hard whole-unit bound. The callback is sampled
 * between committed canonical CPU units, and the guest mailbox is peeked at
 * the same boundary. The caller owns result and must dispose it after output. */
gbl_runner_status gbl_runner_execute(const gbl_runner_options *options,
                                     gbl_runner_result *result);

/* Internal host-test seam for the production CPU-stop classification path.
 * This is not a stable embedding ABI. */
void gbl_runner_classify_guest_stop(gbl_runner_result *result,
                                    const gbl_machine *machine,
                                    gbl_run_reason reason);

/* Writes one gamebloke.result/1 JSON object followed by one newline. */
bool gbl_runner_write_json(FILE *output, const gbl_runner_result *result);

/* Writes selected records as deterministic JSON Lines to a newly created
 * explicit path. Existing files are never truncated. */
bool gbl_runner_write_trace(const char *path,
                            const gbl_runner_result *result);

/* Writes one exact valid raw BGR555 frame to a newly created explicit path. */
bool gbl_runner_write_video(const char *path,
                            gbl_runner_result *result);

/* Writes one exact complete stereo S16LE capture to a new explicit path. */
bool gbl_runner_write_audio(const char *path,
                            gbl_runner_result *result);

void gbl_runner_result_dispose(gbl_runner_result *result);

#endif /* GAMEBLOKE_RUNNER_RUN_H */
