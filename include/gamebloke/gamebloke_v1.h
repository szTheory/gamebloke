#ifndef GAMEBLOKE_GAMEBLOKE_V1_H
#define GAMEBLOKE_GAMEBLOKE_V1_H

/* Stable, opaque C embedding API.  This is a native ABI for a single target
 * architecture and compiler ABI; it is not a portable wire format. */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gbl_v1_machine gbl_v1_machine;

#define GBL_V1_MAX_ADVANCE_CYCLES UINT64_C(16777216)
#define GBL_V1_MAX_ROM_BYTES UINT32_C(0x02000000)
#define GBL_V1_MAX_INPUT_EVENTS UINT32_C(256)
#define GBL_V1_VIDEO_WIDTH UINT32_C(240)
#define GBL_V1_VIDEO_HEIGHT UINT32_C(160)
#define GBL_V1_VIDEO_BYTES UINT32_C(76800)
#define GBL_V1_MAX_AUDIO_BYTES UINT32_C(131072)
#define GBL_V1_AUDIO_SAMPLE_RATE UINT32_C(32768)
#define GBL_V1_AUDIO_CYCLES_PER_FRAME UINT32_C(512)
#define GBL_V1_AUDIO_CHANNELS UINT32_C(2)
#define GBL_V1_INPUT_MASK UINT16_C(0x03FF)

enum {
    GBL_V1_KEY_A = UINT16_C(1) << 0,
    GBL_V1_KEY_B = UINT16_C(1) << 1,
    GBL_V1_KEY_SELECT = UINT16_C(1) << 2,
    GBL_V1_KEY_START = UINT16_C(1) << 3,
    GBL_V1_KEY_RIGHT = UINT16_C(1) << 4,
    GBL_V1_KEY_LEFT = UINT16_C(1) << 5,
    GBL_V1_KEY_UP = UINT16_C(1) << 6,
    GBL_V1_KEY_DOWN = UINT16_C(1) << 7,
    GBL_V1_KEY_R = UINT16_C(1) << 8,
    GBL_V1_KEY_L = UINT16_C(1) << 9
};

typedef enum gbl_v1_status {
    GBL_V1_OK = 0,
    GBL_V1_INVALID_ARGUMENT = 1,
    GBL_V1_UNSUPPORTED = 2,
    GBL_V1_INVALID_LIFECYCLE = 3,
    GBL_V1_CONTENT_REFUSED = 4,
    GBL_V1_ALLOCATION_FAILED = 5,
    GBL_V1_INSUFFICIENT_CAPACITY = 6,
    GBL_V1_WORK_EXHAUSTED = 7,
    GBL_V1_OUTPUT_UNAVAILABLE = 8,
    GBL_V1_INTERNAL_REFUSAL = 9
} gbl_v1_status;

typedef void *(*gbl_v1_allocate_fn)(void *context, size_t size);
typedef void (*gbl_v1_release_fn)(void *context, void *pointer, size_t size);

typedef enum gbl_v1_profile {
    GBL_V1_PROFILE_AGB = 1
} gbl_v1_profile;

typedef enum gbl_v1_start_mode {
    GBL_V1_START_DIRECT = 1
} gbl_v1_start_mode;

typedef enum gbl_v1_cartridge_mode {
    GBL_V1_CARTRIDGE_ROM_ONLY = 1
} gbl_v1_cartridge_mode;

typedef struct gbl_v1_config {
    size_t struct_size;
    uint32_t profile;
    uint32_t start_mode;
    uint32_t cartridge_mode;
    uint32_t reserved;
    void *allocator_context;
    gbl_v1_allocate_fn allocate;
    gbl_v1_release_fn release;
} gbl_v1_config;

typedef struct gbl_v1_rom {
    size_t struct_size;
    const uint8_t *bytes;
    size_t length;
} gbl_v1_rom;

typedef struct gbl_v1_input_event {
    uint64_t cycle;
    uint16_t pressed_mask;
    uint16_t reserved;
} gbl_v1_input_event;

typedef struct gbl_v1_input_timeline {
    size_t struct_size;
    const gbl_v1_input_event *events;
    size_t count;
} gbl_v1_input_timeline;

typedef enum gbl_v1_advance_outcome {
    GBL_V1_ADVANCE_COMPLETED = 0,
    GBL_V1_ADVANCE_GUEST_TERMINAL = 1
} gbl_v1_advance_outcome;

typedef enum gbl_v1_stop_reason {
    GBL_V1_STOP_NONE = 0,
    GBL_V1_STOP_UNIT_LIMIT = 1,
    GBL_V1_STOP_GUEST_TERMINAL = 2,
    GBL_V1_STOP_INSTRUCTION_UNSUPPORTED = 3
} gbl_v1_stop_reason;

typedef enum gbl_v1_mailbox_state {
    GBL_V1_MAILBOX_ABSENT = 0,
    GBL_V1_MAILBOX_PENDING = 1,
    GBL_V1_MAILBOX_PASS = 2,
    GBL_V1_MAILBOX_FAIL = 3,
    GBL_V1_MAILBOX_MALFORMED = 4
} gbl_v1_mailbox_state;

typedef enum gbl_v1_video_state {
    GBL_V1_VIDEO_INVALID = 0,
    GBL_V1_VIDEO_INCOMPLETE = 1,
    GBL_V1_VIDEO_VALID = 2
} gbl_v1_video_state;

typedef enum gbl_v1_video_format {
    GBL_V1_VIDEO_BGR555_LE = 1
} gbl_v1_video_format;

typedef enum gbl_v1_audio_format {
    GBL_V1_AUDIO_S16_LE = 1
} gbl_v1_audio_format;

typedef enum gbl_v1_hardware_qualification {
    GBL_V1_QUALIFICATION_UNKNOWN = 1
} gbl_v1_hardware_qualification;

enum {
    GBL_V1_CAP_TIMED_LOGICAL_INPUT = UINT64_C(1) << 0,
    GBL_V1_CAP_MAILBOX_OBSERVATION = UINT64_C(1) << 1,
    GBL_V1_CAP_RAW_VIDEO_CAPTURE = UINT64_C(1) << 2,
    GBL_V1_CAP_RAW_AUDIO_CAPTURE = UINT64_C(1) << 3
};

enum {
    GBL_V1_MODEL_LIMIT_TIMING_DISPUTES = UINT32_C(1) << 0,
    GBL_V1_MODEL_LIMIT_UNMODELED_PREFETCH = UINT32_C(1) << 1,
    GBL_V1_MODEL_LIMIT_PRE_PWM_AUDIO = UINT32_C(1) << 2
};

typedef struct gbl_v1_observation {
    size_t struct_size;
    uint32_t loaded;
    uint32_t profile;
    uint32_t profile_version;
    uint32_t bios_mode;
    uint32_t cartridge_mode;
    uint32_t video_state;
    uint32_t mailbox_state;
    uint32_t hardware_qualification;
    uint32_t raw_video_format;
    uint32_t raw_audio_format;
    uint32_t audio_sample_rate;
    uint32_t audio_cycles_per_frame;
    uint32_t audio_channels;
    uint16_t logical_input_mask;
    uint16_t reserved;
    uint32_t applied_input_count;
    uint32_t pending_input_count;
    uint32_t mailbox_case_id;
    uint32_t maximum_input_events;
    uint32_t maximum_rom_bytes;
    uint32_t video_width;
    uint32_t video_height;
    uint32_t maximum_audio_bytes;
    uint32_t model_limit_flags;
    uint64_t capabilities;
    uint64_t input_generation;
    uint64_t master_cycle;
    uint64_t retired_instructions;
    uint64_t video_epoch;
    uint64_t maximum_advance_cycles;
    char profile_id[4];
    char bios_id[5];
    char cartridge_id[9];
} gbl_v1_observation;

typedef struct gbl_v1_advance_options {
    size_t struct_size;
    uint32_t units;
    uint32_t reserved;
    uint64_t cycle_budget;
    /* Optional suffix. Omit it by passing the prefix ending after cycle_budget. */
    uint32_t capture_audio;
    uint32_t reserved2;
    uint8_t *audio_bytes;
    size_t audio_capacity;
} gbl_v1_advance_options;

typedef struct gbl_v1_advance_result {
    size_t struct_size;
    uint32_t requested_units;
    uint32_t outcome;
    uint32_t stop_reason;
    uint32_t audio_available;
    uint64_t requested_cycle_budget;
    uint64_t start_cycle;
    uint64_t end_cycle;
    uint64_t elapsed_cycles;
    uint64_t retired_instructions;
    size_t audio_bytes_written;
    size_t audio_frames_written;
    gbl_v1_observation observation;
} gbl_v1_advance_result;

typedef struct gbl_v1_video {
    size_t struct_size;
    uint8_t *bytes;
    size_t capacity;
} gbl_v1_video;

gbl_v1_status gbl_v1_create(const gbl_v1_config *config,
                            gbl_v1_machine **out_machine);
void gbl_v1_destroy(gbl_v1_machine *machine);
gbl_v1_status gbl_v1_load_rom(gbl_v1_machine *machine,
                              const gbl_v1_rom *rom);
gbl_v1_status gbl_v1_reset(gbl_v1_machine *machine);
gbl_v1_status gbl_v1_unload(gbl_v1_machine *machine);
gbl_v1_status gbl_v1_set_input(gbl_v1_machine *machine,
                               const gbl_v1_input_timeline *timeline);
gbl_v1_status gbl_v1_advance(gbl_v1_machine *machine,
                             const gbl_v1_advance_options *options,
                             gbl_v1_advance_result *result);
gbl_v1_status gbl_v1_observe(const gbl_v1_machine *machine,
                             gbl_v1_observation *observation);
gbl_v1_status gbl_v1_read_video(const gbl_v1_machine *machine,
                                const gbl_v1_video *video);

#ifdef __cplusplus
}
#endif

#endif
