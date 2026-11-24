#include <gamebloke/gamebloke_v1.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum {
    PHASE21_CASE_ID = 21001,
    PHASE21_REQUESTED_UNITS = 200000,
    PHASE21_CYCLE_BUDGET = 300000,
    PHASE21_AUDIO_LEFT = 8192,
    PHASE21_AUDIO_RIGHT = -6144,
    PHASE21_AUDIO_INDEX_4096 = 7,
    PHASE21_AUDIO_INDEX_8192 = 15,
    PHASE21_AUDIO_FRAME_BYTES = 4
};

typedef struct allocation_record {
    void *pointer;
    size_t size;
    bool live;
} allocation_record;

typedef struct allocator_state {
    allocation_record records[32];
    size_t live_count;
    bool invalid_release;
} allocator_state;

typedef enum run_mode {
    RUN_EXPECT_PASS,
    RUN_EXPECT_TIMING_FAIL,
    RUN_MUTATE_VIDEO,
    RUN_MUTATE_AUDIO
} run_mode;

static void *consumer_allocate(void *context, size_t size)
{
    allocator_state *state = context;
    size_t index;
    void *pointer;

    if (state == NULL || size == 0u) {
        return NULL;
    }
    for (index = 0u; index < sizeof state->records / sizeof state->records[0];
         ++index) {
        if (!state->records[index].live) {
            pointer = malloc(size);
            if (pointer == NULL) {
                return NULL;
            }
            state->records[index].pointer = pointer;
            state->records[index].size = size;
            state->records[index].live = true;
            ++state->live_count;
            return pointer;
        }
    }
    return NULL;
}

static void consumer_release(void *context, void *pointer, size_t size)
{
    allocator_state *state = context;
    size_t index;

    if (state == NULL || pointer == NULL) {
        if (state != NULL) {
            state->invalid_release = true;
        }
        return;
    }
    for (index = 0u; index < sizeof state->records / sizeof state->records[0];
         ++index) {
        allocation_record *record = &state->records[index];

        if (record->live && record->pointer == pointer) {
            if (record->size != size) {
                state->invalid_release = true;
            }
            record->live = false;
            record->pointer = NULL;
            record->size = 0u;
            --state->live_count;
            free(pointer);
            return;
        }
    }
    state->invalid_release = true;
}

static bool read_rom_file(const char *path, uint8_t **bytes_out,
                          size_t *length_out)
{
    FILE *file;
    long file_length;
    size_t length;
    uint8_t *bytes;
    bool okay;

    if (path == NULL || bytes_out == NULL || length_out == NULL) {
        return false;
    }
    *bytes_out = NULL;
    *length_out = 0u;
    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    okay = fseek(file, 0L, SEEK_END) == 0;
    file_length = okay ? ftell(file) : -1L;
    if (!okay || file_length < (long)0xC0 ||
        file_length > (long)GBL_V1_MAX_ROM_BYTES ||
        (file_length & 1L) != 0L || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return false;
    }
    length = (size_t)file_length;
    bytes = malloc(length);
    if (bytes == NULL) {
        (void)fclose(file);
        return false;
    }
    okay = fread(bytes, 1u, length, file) == length &&
           fgetc(file) == EOF && ferror(file) == 0;
    if (fclose(file) != 0) {
        okay = false;
    }
    if (!okay) {
        free(bytes);
        return false;
    }
    *bytes_out = bytes;
    *length_out = length;
    return true;
}

static bool path_is_directory(const char *path)
{
    struct stat status;

    return path != NULL && stat(path, &status) == 0 &&
           (status.st_mode & S_IFMT) == S_IFDIR;
}

static bool all_bytes_equal(const uint8_t *bytes, size_t length,
                            uint8_t expected)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        if (bytes[index] != expected) {
            return false;
        }
    }
    return true;
}

static bool write_new_file(const char *directory, const char *name,
                           const uint8_t *bytes, size_t length)
{
    char path[1024];
    FILE *file;
    int count;
    bool okay;

    count = snprintf(path, sizeof path, "%s/%s", directory, name);
    if (count < 0 || (size_t)count >= sizeof path) {
        return false;
    }
    file = fopen(path, "wbx");
    if (file == NULL) {
        return false;
    }
    okay = fwrite(bytes, 1u, length, file) == length;
    if (fclose(file) != 0) {
        okay = false;
    }
    if (!okay) {
        (void)remove(path);
    }
    return okay;
}

static const char *mailbox_name(uint32_t state)
{
    switch (state) {
    case GBL_V1_MAILBOX_ABSENT:
        return "absent";
    case GBL_V1_MAILBOX_PENDING:
        return "pending";
    case GBL_V1_MAILBOX_PASS:
        return "pass";
    case GBL_V1_MAILBOX_FAIL:
        return "fail";
    case GBL_V1_MAILBOX_MALFORMED:
        return "malformed";
    default:
        return "unknown";
    }
}

static uint16_t pixel_value(const uint8_t *video, size_t pixel_index)
{
    const size_t offset = pixel_index * 2u;

    return (uint16_t)video[offset] |
           (uint16_t)((uint16_t)video[offset + 1u] << 8u);
}

static int32_t sample_channel(const uint8_t *audio, size_t frame_index,
                              size_t channel)
{
    const size_t offset = frame_index * PHASE21_AUDIO_FRAME_BYTES +
                          channel * 2u;
    const uint16_t bits = (uint16_t)audio[offset] |
                          (uint16_t)((uint16_t)audio[offset + 1u] << 8u);

    return bits <= UINT16_C(0x7FFF)
               ? (int32_t)bits
               : (int32_t)bits - INT32_C(0x10000);
}

static bool sample_pair_is(const uint8_t *audio, size_t frame_index)
{
    return sample_channel(audio, frame_index, 0u) == PHASE21_AUDIO_LEFT &&
           sample_channel(audio, frame_index, 1u) == PHASE21_AUDIO_RIGHT;
}

static bool write_observations(const char *directory,
                               const gbl_v1_observation *observation,
                               const gbl_v1_advance_result *result,
                               const gbl_v1_input_event *events,
                               const uint8_t *video_bytes,
                               const uint8_t *audio_bytes,
                               const char *expected_status)
{
    char json[2048];
    int count;

    count = snprintf(
        json, sizeof json,
        "{\n"
        "  \"schema\": \"gamebloke.phase21-consumer/1\",\n"
        "  \"profile\": {\"id\": \"%s\", \"version\": %u, "
        "\"bios\": \"%s\", \"hardware_qualification\": \"unknown\"},\n"
        "  \"guest\": {\"case_id\": %u, \"status\": \"%s\", "
        "\"expected_status\": \"%s\"},\n"
        "  \"input\": {\"events\": [{\"cycle\": %llu, "
        "\"pressed_mask\": %u}, {\"cycle\": %llu, "
        "\"pressed_mask\": %u}, {\"cycle\": %llu, "
        "\"pressed_mask\": %u}], "
        "\"final_mask\": %u, \"applied\": %u, \"pending\": %u},\n"
        "  \"execution\": {\"outcome\": \"guest_terminal\", "
        "\"stop_reason\": \"no_bios_swi\", \"requested_units\": %u, "
        "\"cycle_budget\": %u, \"start_cycle\": %llu, "
        "\"end_cycle\": %llu, \"elapsed_cycles\": %llu, "
        "\"retired_instructions\": %llu},\n"
        "  \"video\": {\"format\": \"bgr555-le\", \"width\": %u, "
        "\"height\": %u, \"state\": \"valid\", "
        "\"epoch\": %llu, \"bytes\": %u},\n"
        "  \"audio\": {\"format\": \"s16le\", \"sample_rate\": %u, "
        "\"cycles_per_frame\": %u, \"channels\": %u, "
        "\"model\": \"pre-pwm\", \"frames\": %llu, "
        "\"bytes\": %llu, \"samples\": ["
        "{\"cycle\": 4096, \"index\": %u, \"left\": %d, "
        "\"right\": %d}, {\"cycle\": 8192, \"index\": %u, "
        "\"left\": %d, \"right\": %d}]},\n"
        "  \"pixels\": [{\"x\": 0, \"y\": 1, \"value\": %u}, "
        "{\"x\": 1, \"y\": 1, \"value\": %u}, "
        "{\"x\": 2, \"y\": 1, \"value\": %u}]\n"
        "}\n",
        observation->profile_id, observation->profile_version,
        observation->bios_id, observation->mailbox_case_id,
        mailbox_name(observation->mailbox_state),
        expected_status,
        (unsigned long long)events[0].cycle, (unsigned)events[0].pressed_mask,
        (unsigned long long)events[1].cycle, (unsigned)events[1].pressed_mask,
        (unsigned long long)events[2].cycle, (unsigned)events[2].pressed_mask,
        (unsigned)observation->logical_input_mask,
        observation->applied_input_count, observation->pending_input_count,
        result->requested_units, (unsigned)result->requested_cycle_budget,
        (unsigned long long)result->start_cycle,
        (unsigned long long)result->end_cycle,
        (unsigned long long)result->elapsed_cycles,
        (unsigned long long)result->retired_instructions,
        observation->video_width, observation->video_height,
        (unsigned long long)observation->video_epoch,
        GBL_V1_VIDEO_BYTES, observation->audio_sample_rate,
        observation->audio_cycles_per_frame, observation->audio_channels,
        (unsigned long long)result->audio_frames_written,
        (unsigned long long)result->audio_bytes_written,
        PHASE21_AUDIO_INDEX_4096,
        (int)sample_channel(audio_bytes, PHASE21_AUDIO_INDEX_4096, 0u),
        (int)sample_channel(audio_bytes, PHASE21_AUDIO_INDEX_4096, 1u),
        PHASE21_AUDIO_INDEX_8192,
        (int)sample_channel(audio_bytes, PHASE21_AUDIO_INDEX_8192, 0u),
        (int)sample_channel(audio_bytes, PHASE21_AUDIO_INDEX_8192, 1u),
        (unsigned)pixel_value(video_bytes, GBL_V1_VIDEO_WIDTH * 1u),
        (unsigned)pixel_value(video_bytes, GBL_V1_VIDEO_WIDTH * 1u + 1u),
        (unsigned)pixel_value(video_bytes, GBL_V1_VIDEO_WIDTH * 1u + 2u));
    if (count < 0 || (size_t)count >= sizeof json) {
        return false;
    }
    return write_new_file(directory, "observations.json",
                          (const uint8_t *)json, (size_t)count);
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s <fixture.gba> <evidence-directory> "
            "[--timing-control | --expectation-control video|audio]\n",
            program);
}

int main(int argc, char **argv)
{
    run_mode mode = RUN_EXPECT_PASS;
    const char *rom_path;
    const char *evidence_directory;
    const char *failure_category = NULL;
    char failure_detail[256] = "";
    uint8_t *rom_bytes = NULL;
    size_t rom_length = 0u;
    uint8_t video_bytes[GBL_V1_VIDEO_BYTES + 4u];
    uint8_t audio_bytes[GBL_V1_MAX_AUDIO_BYTES];
    allocator_state allocator = { 0 };
    gbl_v1_machine *machine = NULL;
    gbl_v1_config config = { 0 };
    gbl_v1_rom rom = { 0 };
    gbl_v1_input_event events[3] = { 0 };
    gbl_v1_input_timeline timeline = { 0 };
    gbl_v1_advance_options options = { 0 };
    gbl_v1_advance_result result = { 0 };
    gbl_v1_observation observation = { 0 };
    gbl_v1_video video_request = { 0 };
    uint32_t expected_mailbox = GBL_V1_MAILBOX_PASS;
    uint64_t expected_frames;
    size_t expected_audio_bytes;
    bool success = false;
    int exit_code = EXIT_FAILURE;

    if ((argc != 3 && argc != 4 && argc != 5) || argv[1] == NULL ||
        argv[2] == NULL) {
        usage(argv[0]);
        return 2;
    }
    rom_path = argv[1];
    evidence_directory = argv[2];
    if (argc == 4 && strcmp(argv[3], "--timing-control") == 0) {
        mode = RUN_EXPECT_TIMING_FAIL;
        expected_mailbox = GBL_V1_MAILBOX_FAIL;
    } else if (argc == 5 && strcmp(argv[3], "--expectation-control") == 0 &&
               strcmp(argv[4], "video") == 0) {
        mode = RUN_MUTATE_VIDEO;
    } else if (argc == 5 && strcmp(argv[3], "--expectation-control") == 0 &&
               strcmp(argv[4], "audio") == 0) {
        mode = RUN_MUTATE_AUDIO;
    } else if (argc != 3) {
        usage(argv[0]);
        return 2;
    }
    if (!path_is_directory(evidence_directory)) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "evidence directory is missing or not a directory");
        failure_category = "evidence-setup";
        goto cleanup;
    }
    if (!read_rom_file(rom_path, &rom_bytes, &rom_length)) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "fixture is unreadable, changed during read, or outside "
                       "the even 0xC0..0x02000000 byte range");
        failure_category = "fixture-input";
        goto cleanup;
    }

    config.struct_size = sizeof config;
    config.profile = GBL_V1_PROFILE_AGB;
    config.start_mode = GBL_V1_START_DIRECT;
    config.cartridge_mode = GBL_V1_CARTRIDGE_ROM_ONLY;
    config.allocator_context = &allocator;
    config.allocate = consumer_allocate;
    config.release = consumer_release;
    if (gbl_v1_create(&config, &machine) != GBL_V1_OK || machine == NULL) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "public API create failed");
        failure_category = "api-create";
        goto cleanup;
    }
    rom.struct_size = sizeof rom;
    rom.bytes = rom_bytes;
    rom.length = rom_length;
    if (gbl_v1_load_rom(machine, &rom) != GBL_V1_OK) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "public API refused the checked ROM span");
        failure_category = "api-load-rom";
        goto cleanup;
    }
    free(rom_bytes);
    rom_bytes = NULL;
    rom.bytes = NULL;

    events[0] = (gbl_v1_input_event){ 0u, 0u, 0u };
    events[1] = (gbl_v1_input_event){
        mode == RUN_EXPECT_TIMING_FAIL ? UINT64_C(1536) : UINT64_C(1024),
        GBL_V1_KEY_A, 0u
    };
    events[2] = (gbl_v1_input_event){ UINT64_C(2048), GBL_V1_KEY_B, 0u };
    timeline.struct_size = sizeof timeline;
    timeline.events = events;
    timeline.count = sizeof events / sizeof events[0];
    if (gbl_v1_set_input(machine, &timeline) != GBL_V1_OK) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "public API refused the ordered timestamped input");
        failure_category = "api-input";
        goto cleanup;
    }

    memset(video_bytes, 0xA5, sizeof video_bytes);
    memset(audio_bytes, 0xA5, sizeof audio_bytes);
    options.struct_size = sizeof options;
    options.units = PHASE21_REQUESTED_UNITS;
    options.cycle_budget = PHASE21_CYCLE_BUDGET;
    options.capture_audio = 1u;
    options.audio_bytes = audio_bytes;
    options.audio_capacity = sizeof audio_bytes;
    result.struct_size = sizeof result;
    if (gbl_v1_advance(machine, &options, &result) != GBL_V1_OK) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "bounded public advance refused or exhausted its budget");
        failure_category = "api-advance";
        goto cleanup;
    }
    if (result.outcome != GBL_V1_ADVANCE_GUEST_TERMINAL ||
        result.stop_reason != GBL_V1_STOP_GUEST_TERMINAL) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "expected supported guest terminal; outcome=%u stop=%u",
                       result.outcome, result.stop_reason);
        failure_category = "guest-stop-reason";
        goto cleanup;
    }
    if (result.requested_units != PHASE21_REQUESTED_UNITS ||
        result.requested_cycle_budget != PHASE21_CYCLE_BUDGET ||
        result.start_cycle != 0u || result.end_cycle <= result.start_cycle ||
        result.end_cycle > PHASE21_CYCLE_BUDGET ||
        result.elapsed_cycles != result.end_cycle ||
        result.retired_instructions == 0u) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "advance work or cycle observations disagree with the "
                       "fixed bounded request");
        failure_category = "guest-work-observation";
        goto cleanup;
    }
    observation.struct_size = sizeof observation;
    if (gbl_v1_observe(machine, &observation) != GBL_V1_OK) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "public observation failed after advance");
        failure_category = "api-observe";
        goto cleanup;
    }
    if (observation.loaded != 1u || observation.profile != GBL_V1_PROFILE_AGB ||
        observation.profile_version != 16u || observation.bios_mode != 0u ||
        observation.cartridge_mode != GBL_V1_CARTRIDGE_ROM_ONLY ||
        strcmp(observation.profile_id, "agb") != 0 ||
        strcmp(observation.bios_id, "none") != 0 ||
        observation.mailbox_state != expected_mailbox ||
        observation.mailbox_case_id != PHASE21_CASE_ID ||
        observation.logical_input_mask != GBL_V1_KEY_B ||
        observation.applied_input_count != 3u ||
        observation.pending_input_count != 0u ||
        observation.master_cycle != result.end_cycle ||
        observation.retired_instructions != result.retired_instructions) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "guest or logical-input observation mismatch: case=%u "
                       "state=%s mask=%u applied=%u pending=%u",
                       observation.mailbox_case_id,
                       mailbox_name(observation.mailbox_state),
                       (unsigned)observation.logical_input_mask,
                       observation.applied_input_count,
                       observation.pending_input_count);
        failure_category = "guest-case-mismatch";
        goto cleanup;
    }
    if (observation.hardware_qualification !=
            GBL_V1_QUALIFICATION_UNKNOWN ||
        observation.audio_sample_rate != GBL_V1_AUDIO_SAMPLE_RATE ||
        observation.audio_cycles_per_frame !=
            GBL_V1_AUDIO_CYCLES_PER_FRAME ||
        observation.audio_channels != GBL_V1_AUDIO_CHANNELS ||
        observation.raw_audio_format != GBL_V1_AUDIO_S16_LE ||
        observation.raw_video_format != GBL_V1_VIDEO_BGR555_LE ||
        observation.video_width != GBL_V1_VIDEO_WIDTH ||
        observation.video_height != GBL_V1_VIDEO_HEIGHT ||
        observation.video_state != GBL_V1_VIDEO_VALID ||
        observation.video_epoch != 0u || result.audio_available != 1u ||
        result.audio_bytes_written > sizeof audio_bytes ||
        result.audio_frames_written == 0u ||
        result.audio_bytes_written != result.audio_frames_written * 4u) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "raw output metadata, validity, or qualification mismatch");
        failure_category = "media-metadata-mismatch";
        goto cleanup;
    }
    expected_frames = result.end_cycle / GBL_V1_AUDIO_CYCLES_PER_FRAME -
                      result.start_cycle / GBL_V1_AUDIO_CYCLES_PER_FRAME;
    expected_audio_bytes = (size_t)expected_frames * 4u;
    if (result.audio_frames_written != expected_frames ||
        result.audio_bytes_written != expected_audio_bytes ||
        !all_bytes_equal(audio_bytes + expected_audio_bytes,
                         sizeof audio_bytes - expected_audio_bytes, 0xA5u)) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "audio count formula or untouched sentinel tail mismatch");
        failure_category = "audio-extent-mismatch";
        goto cleanup;
    }

    video_request.struct_size = sizeof video_request;
    video_request.bytes = video_bytes;
    video_request.capacity = sizeof video_bytes;
    if (gbl_v1_read_video(machine, &video_request) != GBL_V1_OK) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "public API did not return one complete video frame");
        failure_category = "video-read";
        goto cleanup;
    }
    if (!all_bytes_equal(video_bytes + GBL_V1_VIDEO_BYTES, 4u, 0xA5u)) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "video call changed bytes beyond the complete frame");
        failure_category = "video-tail-mismatch";
        goto cleanup;
    }
    if (mode == RUN_MUTATE_VIDEO) {
        video_bytes[(GBL_V1_VIDEO_WIDTH * 1u) * 2u] ^= UINT8_C(1);
    }
    if (video_bytes[480u] != 0x1Fu || video_bytes[481u] != 0x00u ||
        video_bytes[482u] != 0xE0u || video_bytes[483u] != 0x03u ||
        video_bytes[484u] != 0x00u || video_bytes[485u] != 0x00u) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "row 1 pixels expected red=0x001f green=0x03e0 "
                       "black=0x0000; got %02x%02x %02x%02x %02x%02x",
                       video_bytes[481u], video_bytes[480u],
                       video_bytes[483u], video_bytes[482u],
                       video_bytes[485u], video_bytes[484u]);
        failure_category = mode == RUN_MUTATE_VIDEO
                               ? "expected-video-mismatch"
                               : "video-literal-mismatch";
        goto cleanup;
    }
    if (mode == RUN_MUTATE_AUDIO) {
        audio_bytes[(size_t)PHASE21_AUDIO_INDEX_4096 * 4u] ^=
            UINT8_C(1);
    }
    if (result.audio_frames_written <= PHASE21_AUDIO_INDEX_8192 ||
        !sample_pair_is(audio_bytes, PHASE21_AUDIO_INDEX_4096) ||
        !sample_pair_is(audio_bytes, PHASE21_AUDIO_INDEX_8192)) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "audio samples at cycles 4096 and 8192 must be "
                       "left=%d right=%d",
                       PHASE21_AUDIO_LEFT, PHASE21_AUDIO_RIGHT);
        failure_category = mode == RUN_MUTATE_AUDIO
                               ? "expected-audio-mismatch"
                               : "audio-literal-mismatch";
        goto cleanup;
    }

    gbl_v1_destroy(machine);
    machine = NULL;
    if (allocator.live_count != 0u || allocator.invalid_release) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "allocator callbacks did not release exact live sizes");
        failure_category = "allocator-accounting";
        goto cleanup;
    }
    if (!write_new_file(evidence_directory, "frame.bgr555le", video_bytes,
                        GBL_V1_VIDEO_BYTES) ||
        !write_new_file(evidence_directory, "audio.s16le", audio_bytes,
                        expected_audio_bytes) ||
        !write_observations(evidence_directory, &observation, &result,
                            events, video_bytes, audio_bytes,
                            mailbox_name(expected_mailbox))) {
        (void)snprintf(failure_detail, sizeof failure_detail,
                       "could not publish all asserted local evidence files");
        failure_category = "evidence-write";
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }
    success = true;
    exit_code = EXIT_SUCCESS;

cleanup:
    if (machine != NULL) {
        gbl_v1_destroy(machine);
    }
    free(rom_bytes);
    if (!success) {
        char path[1024];
        int count;

        count = snprintf(path, sizeof path, "%s/frame.bgr555le",
                         evidence_directory);
        if (count >= 0 && (size_t)count < sizeof path) {
            (void)remove(path);
        }
        count = snprintf(path, sizeof path, "%s/audio.s16le",
                         evidence_directory);
        if (count >= 0 && (size_t)count < sizeof path) {
            (void)remove(path);
        }
        count = snprintf(path, sizeof path, "%s/observations.json",
                         evidence_directory);
        if (count >= 0 && (size_t)count < sizeof path) {
            (void)remove(path);
        }
        if (failure_category != NULL) {
            fprintf(stderr, "installed-consumer: %s: %s\n",
                    failure_category, failure_detail);
        }
    } else {
        fprintf(stderr,
                "installed-consumer: %s case %u at %llu cycles; "
                "frame=%u bytes audio=%llu frames\n",
                mailbox_name(observation.mailbox_state), PHASE21_CASE_ID,
                (unsigned long long)result.end_cycle, GBL_V1_VIDEO_BYTES,
                (unsigned long long)result.audio_frames_written);
    }
    return exit_code;
}
