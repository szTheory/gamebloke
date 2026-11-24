#include <gamebloke/gamebloke_v1.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
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
    RUN_MUTATE_AUDIO,
    RUN_MUTATE_REFUSAL_CANARY
} run_mode;

typedef struct consumer_instance {
    allocator_state allocator;
    gbl_v1_machine *machine;
} consumer_instance;

typedef struct refusal_record {
    const char *name;
    const char *expected_status;
    uint32_t observed_status;
    bool has_result;
    bool observation_unchanged;
    bool result_unchanged;
    bool caller_bytes_unchanged;
    bool retry_matches;
    size_t descriptor_size;
    size_t requested_capacity;
    size_t caller_bytes;
    size_t retry_audio_bytes;
    size_t retry_video_bytes;
    uint64_t requested_units;
    uint64_t cycle_budget;
    uint64_t committed_cycles;
    uint64_t retry_end_cycle;
    gbl_v1_observation before;
    gbl_v1_observation after;
} refusal_record;

typedef struct refusal_suite {
    refusal_record cases[5];
    size_t case_count;
    uint64_t timing_control_input_cycle;
    uint64_t timing_control_end_cycle;
    uint32_t timing_control_case_id;
    uint32_t timing_control_mailbox_state;
    uint32_t timing_control_final_mask;
    uint32_t timing_control_applied_count;
    uint32_t timing_control_pending_count;
    uint32_t bounded_probe_units;
    uint64_t bounded_probe_budget;
    uint64_t bounded_probe_end_cycle;
    size_t bounded_probe_audio_bytes;
    size_t bounded_probe_audio_frames;
} refusal_suite;

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

static bool observations_equal(const gbl_v1_observation *left,
                               const gbl_v1_observation *right)
{
    return left->struct_size == right->struct_size &&
           left->loaded == right->loaded && left->profile == right->profile &&
           left->profile_version == right->profile_version &&
           left->bios_mode == right->bios_mode &&
           left->cartridge_mode == right->cartridge_mode &&
           left->video_state == right->video_state &&
           left->mailbox_state == right->mailbox_state &&
           left->hardware_qualification == right->hardware_qualification &&
           left->raw_video_format == right->raw_video_format &&
           left->raw_audio_format == right->raw_audio_format &&
           left->audio_sample_rate == right->audio_sample_rate &&
           left->audio_cycles_per_frame == right->audio_cycles_per_frame &&
           left->audio_channels == right->audio_channels &&
           left->logical_input_mask == right->logical_input_mask &&
           left->reserved == right->reserved &&
           left->applied_input_count == right->applied_input_count &&
           left->pending_input_count == right->pending_input_count &&
           left->mailbox_case_id == right->mailbox_case_id &&
           left->maximum_input_events == right->maximum_input_events &&
           left->maximum_rom_bytes == right->maximum_rom_bytes &&
           left->video_width == right->video_width &&
           left->video_height == right->video_height &&
           left->maximum_audio_bytes == right->maximum_audio_bytes &&
           left->model_limit_flags == right->model_limit_flags &&
           left->capabilities == right->capabilities &&
           left->input_generation == right->input_generation &&
           left->master_cycle == right->master_cycle &&
           left->retired_instructions == right->retired_instructions &&
           left->video_epoch == right->video_epoch &&
           left->maximum_advance_cycles == right->maximum_advance_cycles &&
           memcmp(left->profile_id, right->profile_id,
                  sizeof left->profile_id) == 0 &&
           memcmp(left->bios_id, right->bios_id,
                  sizeof left->bios_id) == 0 &&
           memcmp(left->cartridge_id, right->cartridge_id,
                  sizeof left->cartridge_id) == 0;
}

static bool results_equal(const gbl_v1_advance_result *left,
                          const gbl_v1_advance_result *right)
{
    return left->struct_size == right->struct_size &&
           left->requested_units == right->requested_units &&
           left->outcome == right->outcome &&
           left->stop_reason == right->stop_reason &&
           left->audio_available == right->audio_available &&
           left->requested_cycle_budget == right->requested_cycle_budget &&
           left->start_cycle == right->start_cycle &&
           left->end_cycle == right->end_cycle &&
           left->elapsed_cycles == right->elapsed_cycles &&
           left->retired_instructions == right->retired_instructions &&
           left->audio_bytes_written == right->audio_bytes_written &&
           left->audio_frames_written == right->audio_frames_written &&
           observations_equal(&left->observation, &right->observation);
}

static const char *status_name(uint32_t status)
{
    switch (status) {
    case GBL_V1_OK:
        return "ok";
    case GBL_V1_INVALID_ARGUMENT:
        return "invalid_argument";
    case GBL_V1_CONTENT_REFUSED:
        return "content_refused";
    case GBL_V1_INSUFFICIENT_CAPACITY:
        return "insufficient_capacity";
    case GBL_V1_WORK_EXHAUSTED:
        return "work_exhausted";
    default:
        return "other";
    }
}

static bool observe_public(gbl_v1_machine *machine,
                           gbl_v1_observation *observation)
{
    memset(observation, 0, sizeof *observation);
    observation->struct_size = sizeof *observation;
    return gbl_v1_observe(machine, observation) == GBL_V1_OK;
}

static bool instance_from_fixture(const char *rom_path,
                                  const gbl_v1_input_event *events,
                                  size_t event_count,
                                  consumer_instance *instance,
                                  char *failure_category,
                                  char *failure_detail,
                                  size_t failure_detail_size)
{
    uint8_t *rom_bytes = NULL;
    size_t rom_length = 0u;
    gbl_v1_config config = { 0 };
    gbl_v1_rom rom = { 0 };
    gbl_v1_input_timeline timeline = { 0 };

    memset(instance, 0, sizeof *instance);
    if (!read_rom_file(rom_path, &rom_bytes, &rom_length)) {
        (void)snprintf(failure_category, 64u, "refusal-fixture-read");
        (void)snprintf(failure_detail, failure_detail_size,
                       "could not read the admitted fixture for a refusal case");
        return false;
    }
    config.struct_size = sizeof config;
    config.profile = GBL_V1_PROFILE_AGB;
    config.start_mode = GBL_V1_START_DIRECT;
    config.cartridge_mode = GBL_V1_CARTRIDGE_ROM_ONLY;
    config.allocator_context = &instance->allocator;
    config.allocate = consumer_allocate;
    config.release = consumer_release;
    if (gbl_v1_create(&config, &instance->machine) != GBL_V1_OK ||
        instance->machine == NULL) {
        free(rom_bytes);
        (void)snprintf(failure_category, 64u, "refusal-create");
        (void)snprintf(failure_detail, failure_detail_size,
                       "public API create failed in a refusal case");
        return false;
    }
    rom.struct_size = sizeof rom;
    rom.bytes = rom_bytes;
    rom.length = rom_length;
    if (gbl_v1_load_rom(instance->machine, &rom) != GBL_V1_OK) {
        free(rom_bytes);
        gbl_v1_destroy(instance->machine);
        instance->machine = NULL;
        (void)snprintf(failure_category, 64u, "refusal-load");
        (void)snprintf(failure_detail, failure_detail_size,
                       "public API refused the admitted fixture in a refusal case");
        return false;
    }
    free(rom_bytes);
    timeline.struct_size = sizeof timeline;
    timeline.events = events;
    timeline.count = event_count;
    if (gbl_v1_set_input(instance->machine, &timeline) != GBL_V1_OK) {
        gbl_v1_destroy(instance->machine);
        instance->machine = NULL;
        (void)snprintf(failure_category, 64u, "refusal-input");
        (void)snprintf(failure_detail, failure_detail_size,
                       "public API refused copied timestamped input in a refusal case");
        return false;
    }
    return true;
}

static bool instance_destroy_checked(consumer_instance *instance,
                                     char *failure_category,
                                     char *failure_detail,
                                     size_t failure_detail_size)
{
    if (instance->machine != NULL) {
        gbl_v1_destroy(instance->machine);
        instance->machine = NULL;
    }
    if (instance->allocator.live_count != 0u ||
        instance->allocator.invalid_release) {
        (void)snprintf(failure_category, 64u, "refusal-allocator");
        (void)snprintf(failure_detail, failure_detail_size,
                       "refusal instance allocator did not release exact live sizes");
        return false;
    }
    return true;
}

static bool add_refusal_record(refusal_suite *suite, const char *name,
                               const char *expected_status,
                               refusal_record **record)
{
    if (suite->case_count >=
        sizeof suite->cases / sizeof suite->cases[0]) {
        return false;
    }
    *record = &suite->cases[suite->case_count++];
    memset(*record, 0, sizeof **record);
    (*record)->name = name;
    (*record)->expected_status = expected_status;
    (*record)->observation_unchanged = true;
    (*record)->caller_bytes_unchanged = true;
    (*record)->retry_matches = true;
    return true;
}

static bool setup_full_advance(gbl_v1_machine *machine,
                               uint8_t *audio_bytes, size_t audio_capacity,
                               gbl_v1_advance_result *result,
                               uint64_t cycle_budget)
{
    gbl_v1_advance_options options = { 0 };

    memset(audio_bytes, 0xA5, audio_capacity);
    options.struct_size = sizeof options;
    options.units = PHASE21_REQUESTED_UNITS;
    options.cycle_budget = cycle_budget;
    options.capture_audio = 1u;
    options.audio_bytes = audio_bytes;
    options.audio_capacity = audio_capacity;
    memset(result, 0, sizeof *result);
    result->struct_size = sizeof *result;
    return gbl_v1_advance(machine, &options, result) == GBL_V1_OK &&
           result->outcome == GBL_V1_ADVANCE_GUEST_TERMINAL &&
           result->stop_reason == GBL_V1_STOP_GUEST_TERMINAL;
}

static bool retry_matches_reference(gbl_v1_machine *machine,
                                    const gbl_v1_advance_result *reference_result,
                                    const gbl_v1_observation *reference_observation,
                                    const uint8_t *reference_video,
                                    const uint8_t *reference_audio,
                                    refusal_record *record,
                                    char *failure_category,
                                    char *failure_detail,
                                    size_t failure_detail_size)
{
    uint8_t retry_audio[GBL_V1_MAX_AUDIO_BYTES];
    uint8_t retry_video[GBL_V1_VIDEO_BYTES + 4u];
    gbl_v1_advance_result retry_result = { 0 };
    gbl_v1_observation retry_observation = { 0 };
    gbl_v1_video video_request = { 0 };
    bool retry_okay;

    if (!setup_full_advance(machine, retry_audio, sizeof retry_audio,
                            &retry_result, PHASE21_CYCLE_BUDGET) ||
        !observe_public(machine, &retry_observation)) {
        (void)snprintf(failure_category, 64u, "refusal-retry-advance");
        (void)snprintf(failure_detail, failure_detail_size,
                       "corrected full advance did not succeed after refusal");
        return false;
    }
    memset(retry_video, 0xA5, sizeof retry_video);
    video_request.struct_size = sizeof video_request;
    video_request.bytes = retry_video;
    video_request.capacity = sizeof retry_video;
    if (gbl_v1_read_video(machine, &video_request) != GBL_V1_OK) {
        (void)snprintf(failure_category, 64u, "refusal-retry-video");
        (void)snprintf(failure_detail, failure_detail_size,
                       "corrected complete-frame read did not succeed after refusal");
        return false;
    }
    retry_okay = results_equal(&retry_result, reference_result) &&
                 observations_equal(&retry_observation,
                                    reference_observation) &&
                 memcmp(retry_video, reference_video,
                        GBL_V1_VIDEO_BYTES) == 0 &&
                 all_bytes_equal(retry_video + GBL_V1_VIDEO_BYTES, 4u,
                                 0xA5u) &&
                 retry_result.audio_bytes_written ==
                     reference_result->audio_bytes_written &&
                 memcmp(retry_audio, reference_audio,
                        retry_result.audio_bytes_written) == 0 &&
                 all_bytes_equal(retry_audio + retry_result.audio_bytes_written,
                                 sizeof retry_audio -
                                     retry_result.audio_bytes_written,
                                 0xA5u);
    record->retry_matches = retry_okay;
    record->retry_audio_bytes = retry_result.audio_bytes_written;
    record->retry_video_bytes = GBL_V1_VIDEO_BYTES;
    record->retry_end_cycle = retry_result.end_cycle;
    if (!retry_okay) {
        (void)snprintf(failure_category, 64u, "refusal-retry-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "corrected retry did not match the clean installed reference");
        return false;
    }
    return true;
}

typedef enum advance_refusal_kind {
    REFUSE_ADVANCE_SIZE,
    REFUSE_AUDIO_CAPACITY,
    REFUSE_WORK_BUDGET
} advance_refusal_kind;

static bool run_advance_refusal(
    const char *rom_path, const gbl_v1_input_event *events, size_t event_count,
    const gbl_v1_advance_result *reference_result,
    const gbl_v1_observation *reference_observation,
    const uint8_t *reference_video, const uint8_t *reference_audio,
    refusal_suite *suite, advance_refusal_kind kind, bool mutate_canary,
    char *failure_category, char *failure_detail,
    size_t failure_detail_size)
{
    consumer_instance instance;
    refusal_record *record;
    uint8_t audio_bytes[GBL_V1_MAX_AUDIO_BYTES];
    gbl_v1_advance_options options = { 0 };
    gbl_v1_advance_result result;
    uint8_t result_before[sizeof(gbl_v1_advance_result)];
    gbl_v1_observation after;
    uint32_t status;
    bool okay = false;
    bool result_unchanged;
    bool audio_unchanged;
    const char *case_name;
    const char *expected_status;

    if (kind == REFUSE_ADVANCE_SIZE) {
        case_name = "advance-size";
        expected_status = "invalid_argument";
    } else if (kind == REFUSE_AUDIO_CAPACITY) {
        case_name = "audio-capacity";
        expected_status = "insufficient_capacity";
    } else {
        case_name = "work-budget";
        expected_status = "work_exhausted";
    }
    if (!add_refusal_record(suite, case_name, expected_status, &record)) {
        (void)snprintf(failure_category, 64u, "refusal-record-capacity");
        (void)snprintf(failure_detail, failure_detail_size,
                       "too many refusal cases in normalized proof");
        return false;
    }
    if (!instance_from_fixture(rom_path, events, event_count, &instance,
                               failure_category, failure_detail,
                               failure_detail_size)) {
        return false;
    }
    memset(audio_bytes, 0x5A, sizeof audio_bytes);
    options.struct_size = sizeof options;
    options.units = PHASE21_REQUESTED_UNITS;
    options.cycle_budget = PHASE21_CYCLE_BUDGET;
    options.capture_audio = 1u;
    options.audio_bytes = audio_bytes;
    options.audio_capacity = sizeof audio_bytes;
    if (kind == REFUSE_ADVANCE_SIZE) {
        options.struct_size = offsetof(gbl_v1_advance_options, cycle_budget) +
                              sizeof options.cycle_budget - 1u;
    } else if (kind == REFUSE_AUDIO_CAPACITY) {
        if (reference_result->audio_bytes_written == 0u) {
            (void)snprintf(failure_category, 64u, "refusal-reference-audio");
            (void)snprintf(failure_detail, failure_detail_size,
                           "clean reference contains no audio bytes for capacity refusal");
            goto cleanup;
        }
        options.audio_capacity = reference_result->audio_bytes_written - 1u;
    } else {
        options.cycle_budget = UINT64_C(2048);
    }
    record->has_result = true;
    record->descriptor_size = options.struct_size;
    record->requested_capacity = options.audio_capacity;
    record->caller_bytes = sizeof audio_bytes;
    record->requested_units = options.units;
    record->cycle_budget = options.cycle_budget;
    memset(&result, 0xA5, sizeof result);
    result.struct_size = sizeof result;
    memcpy(result_before, &result, sizeof result);
    if (!observe_public(instance.machine, &record->before)) {
        (void)snprintf(failure_category, 64u, "refusal-observe-before");
        (void)snprintf(failure_detail, failure_detail_size,
                       "could not capture public state before %s refusal",
                       case_name);
        goto cleanup;
    }
    status = (uint32_t)gbl_v1_advance(instance.machine, &options, &result);
    if (mutate_canary && kind == REFUSE_AUDIO_CAPACITY) {
        audio_bytes[0] ^= UINT8_C(1);
    }
    record->observed_status = status;
    if (!observe_public(instance.machine, &after)) {
        (void)snprintf(failure_category, 64u, "refusal-observe-after");
        (void)snprintf(failure_detail, failure_detail_size,
                       "could not capture public state after %s refusal",
                       case_name);
        goto cleanup;
    }
    record->after = after;
    record->committed_cycles = after.master_cycle >= record->before.master_cycle
                                   ? after.master_cycle -
                                         record->before.master_cycle
                                   : UINT64_MAX;
    record->observation_unchanged =
        observations_equal(&record->before, &record->after);
    result_unchanged = memcmp(&result, result_before, sizeof result) == 0;
    audio_unchanged = all_bytes_equal(audio_bytes, sizeof audio_bytes, 0x5Au);
    record->result_unchanged = result_unchanged;
    record->caller_bytes_unchanged = audio_unchanged;
    if (status != (uint32_t)(kind == REFUSE_ADVANCE_SIZE
                                 ? GBL_V1_INVALID_ARGUMENT
                                 : kind == REFUSE_AUDIO_CAPACITY
                                       ? GBL_V1_INSUFFICIENT_CAPACITY
                                       : GBL_V1_WORK_EXHAUSTED)) {
        (void)snprintf(failure_category, 64u, "refusal-status-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "%s expected %s, observed %s", case_name,
                       expected_status, status_name(status));
        goto cleanup;
    }
    if (!record->observation_unchanged || record->committed_cycles != 0u) {
        (void)snprintf(failure_category, 64u, "refusal-observation-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "%s changed one or more public observation fields",
                       case_name);
        goto cleanup;
    }
    if (!result_unchanged) {
        (void)snprintf(failure_category, 64u, "refusal-result-canary-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "%s changed the result canary", case_name);
        goto cleanup;
    }
    if (!audio_unchanged) {
        (void)snprintf(failure_category, 64u, "refusal-audio-canary-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "%s changed caller audio bytes after refusal", case_name);
        goto cleanup;
    }
    if (!retry_matches_reference(instance.machine, reference_result,
                                 reference_observation, reference_video,
                                 reference_audio, record, failure_category,
                                 failure_detail, failure_detail_size)) {
        goto cleanup;
    }
    okay = true;

cleanup:
    if (!instance_destroy_checked(&instance, failure_category,
                                  failure_detail, failure_detail_size)) {
        okay = false;
    }
    return okay;
}

static bool run_content_refusal(
    const char *rom_path, const gbl_v1_input_event *events, size_t event_count,
    const gbl_v1_advance_result *reference_result,
    const gbl_v1_observation *reference_observation,
    const uint8_t *reference_video, const uint8_t *reference_audio,
    refusal_suite *suite, char *failure_category, char *failure_detail,
    size_t failure_detail_size)
{
    consumer_instance instance;
    refusal_record *record;
    uint8_t short_rom_bytes[0xBEu];
    gbl_v1_rom short_rom = { 0 };
    gbl_v1_observation after;
    uint32_t status;
    bool okay = false;

    if (!add_refusal_record(suite, "short-rom", "content_refused", &record)) {
        (void)snprintf(failure_category, 64u, "refusal-record-capacity");
        (void)snprintf(failure_detail, failure_detail_size,
                       "too many refusal cases in normalized proof");
        return false;
    }
    if (!instance_from_fixture(rom_path, events, event_count, &instance,
                               failure_category, failure_detail,
                               failure_detail_size)) {
        return false;
    }
    memset(short_rom_bytes, 0x3C, sizeof short_rom_bytes);
    short_rom.struct_size = sizeof short_rom;
    short_rom.bytes = short_rom_bytes;
    short_rom.length = sizeof short_rom_bytes;
    record->descriptor_size = short_rom.struct_size;
    record->requested_capacity = short_rom.length;
    record->caller_bytes = sizeof short_rom_bytes;
    if (!observe_public(instance.machine, &record->before)) {
        (void)snprintf(failure_category, 64u, "refusal-observe-before");
        (void)snprintf(failure_detail, failure_detail_size,
                       "could not capture public state before short-ROM refusal");
        goto cleanup;
    }
    status = (uint32_t)gbl_v1_load_rom(instance.machine, &short_rom);
    record->observed_status = status;
    if (!observe_public(instance.machine, &after)) {
        (void)snprintf(failure_category, 64u, "refusal-observe-after");
        (void)snprintf(failure_detail, failure_detail_size,
                       "could not capture public state after short-ROM refusal");
        goto cleanup;
    }
    record->after = after;
    record->committed_cycles = after.master_cycle >= record->before.master_cycle
                                   ? after.master_cycle -
                                         record->before.master_cycle
                                   : UINT64_MAX;
    record->observation_unchanged =
        observations_equal(&record->before, &record->after);
    record->caller_bytes_unchanged =
        all_bytes_equal(short_rom_bytes, sizeof short_rom_bytes, 0x3Cu);
    if (status != (uint32_t)GBL_V1_CONTENT_REFUSED) {
        (void)snprintf(failure_category, 64u, "refusal-status-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "short-rom expected content_refused, observed %s",
                       status_name(status));
        goto cleanup;
    }
    if (!record->observation_unchanged || record->committed_cycles != 0u) {
        (void)snprintf(failure_category, 64u, "refusal-observation-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "short-ROM refusal changed loaded identity or public state");
        goto cleanup;
    }
    if (!record->caller_bytes_unchanged) {
        (void)snprintf(failure_category, 64u, "refusal-rom-canary-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "short-ROM input bytes changed after refusal");
        goto cleanup;
    }
    if (!retry_matches_reference(instance.machine, reference_result,
                                 reference_observation, reference_video,
                                 reference_audio, record, failure_category,
                                 failure_detail, failure_detail_size)) {
        goto cleanup;
    }
    okay = true;

cleanup:
    if (!instance_destroy_checked(&instance, failure_category,
                                  failure_detail, failure_detail_size)) {
        okay = false;
    }
    return okay;
}

static bool run_video_refusal(
    const char *rom_path, const gbl_v1_input_event *events, size_t event_count,
    const gbl_v1_advance_result *reference_result,
    const gbl_v1_observation *reference_observation,
    const uint8_t *reference_video, const uint8_t *reference_audio,
    refusal_suite *suite, char *failure_category, char *failure_detail,
    size_t failure_detail_size)
{
    consumer_instance instance;
    refusal_record *record;
    uint8_t retry_audio[GBL_V1_MAX_AUDIO_BYTES];
    uint8_t video_bytes[GBL_V1_VIDEO_BYTES + 4u];
    gbl_v1_advance_result retry_result = { 0 };
    gbl_v1_observation advanced_observation = { 0 };
    gbl_v1_observation after;
    gbl_v1_video request = { 0 };
    uint32_t status;
    bool okay = false;

    if (!add_refusal_record(suite, "video-capacity", "insufficient_capacity",
                            &record)) {
        (void)snprintf(failure_category, 64u, "refusal-record-capacity");
        (void)snprintf(failure_detail, failure_detail_size,
                       "too many refusal cases in normalized proof");
        return false;
    }
    if (!instance_from_fixture(rom_path, events, event_count, &instance,
                               failure_category, failure_detail,
                               failure_detail_size)) {
        return false;
    }
    if (!setup_full_advance(instance.machine, retry_audio, sizeof retry_audio,
                            &retry_result, PHASE21_CYCLE_BUDGET) ||
        !observe_public(instance.machine, &advanced_observation) ||
        !results_equal(&retry_result, reference_result) ||
        !observations_equal(&advanced_observation, reference_observation) ||
        retry_result.audio_bytes_written != reference_result->audio_bytes_written ||
        memcmp(retry_audio, reference_audio,
               retry_result.audio_bytes_written) != 0) {
        (void)snprintf(failure_category, 64u, "video-refusal-setup-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "video refusal precondition differs from the clean complete-frame reference");
        goto cleanup;
    }
    memset(video_bytes, 0x5A, sizeof video_bytes);
    request.struct_size = sizeof request;
    request.bytes = video_bytes;
    request.capacity = GBL_V1_VIDEO_BYTES - 1u;
    record->descriptor_size = request.struct_size;
    record->requested_capacity = request.capacity;
    record->caller_bytes = sizeof video_bytes;
    record->has_result = false;
    if (!observe_public(instance.machine, &record->before)) {
        (void)snprintf(failure_category, 64u, "refusal-observe-before");
        (void)snprintf(failure_detail, failure_detail_size,
                       "could not capture public state before video-capacity refusal");
        goto cleanup;
    }
    status = (uint32_t)gbl_v1_read_video(instance.machine, &request);
    record->observed_status = status;
    if (!observe_public(instance.machine, &after)) {
        (void)snprintf(failure_category, 64u, "refusal-observe-after");
        (void)snprintf(failure_detail, failure_detail_size,
                       "could not capture public state after video-capacity refusal");
        goto cleanup;
    }
    record->after = after;
    record->committed_cycles = after.master_cycle >= record->before.master_cycle
                                   ? after.master_cycle -
                                         record->before.master_cycle
                                   : UINT64_MAX;
    record->observation_unchanged =
        observations_equal(&record->before, &record->after);
    record->caller_bytes_unchanged =
        all_bytes_equal(video_bytes, sizeof video_bytes, 0x5Au);
    if (status != (uint32_t)GBL_V1_INSUFFICIENT_CAPACITY) {
        (void)snprintf(failure_category, 64u, "refusal-status-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "video-capacity expected insufficient_capacity, observed %s",
                       status_name(status));
        goto cleanup;
    }
    if (!record->observation_unchanged || record->committed_cycles != 0u) {
        (void)snprintf(failure_category, 64u, "refusal-observation-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "video-capacity refusal changed public state");
        goto cleanup;
    }
    if (!record->caller_bytes_unchanged) {
        (void)snprintf(failure_category, 64u, "refusal-video-canary-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "video-capacity refusal changed caller video bytes");
        goto cleanup;
    }
    request.capacity = sizeof video_bytes;
    status = (uint32_t)gbl_v1_read_video(instance.machine, &request);
    record->retry_matches = status == (uint32_t)GBL_V1_OK &&
                            memcmp(video_bytes, reference_video,
                                   GBL_V1_VIDEO_BYTES) == 0 &&
                            all_bytes_equal(video_bytes + GBL_V1_VIDEO_BYTES,
                                            4u, 0x5Au) &&
                            observe_public(instance.machine, &after) &&
                            observations_equal(&after,
                                               reference_observation);
    record->retry_video_bytes = GBL_V1_VIDEO_BYTES;
    record->retry_end_cycle = retry_result.end_cycle;
    if (!record->retry_matches) {
        (void)snprintf(failure_category, 64u, "refusal-retry-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "complete video retry did not match the clean reference frame");
        goto cleanup;
    }
    okay = true;

cleanup:
    if (!instance_destroy_checked(&instance, failure_category,
                                  failure_detail, failure_detail_size)) {
        okay = false;
    }
    return okay;
}

static bool run_bounded_probe(const char *rom_path,
                              const gbl_v1_input_event *events,
                              size_t event_count, refusal_suite *suite,
                              char *failure_category, char *failure_detail,
                              size_t failure_detail_size)
{
    consumer_instance instance;
    uint8_t audio_bytes[GBL_V1_MAX_AUDIO_BYTES];
    gbl_v1_advance_options options = { 0 };
    gbl_v1_advance_result result = { 0 };
    gbl_v1_observation observation = { 0 };
    uint32_t status;
    bool okay = false;

    if (!instance_from_fixture(rom_path, events, event_count, &instance,
                               failure_category, failure_detail,
                               failure_detail_size)) {
        return false;
    }
    memset(audio_bytes, 0xA5, sizeof audio_bytes);
    options.struct_size = sizeof options;
    options.units = 128u;
    options.cycle_budget = UINT64_C(2048);
    options.capture_audio = 1u;
    options.audio_bytes = audio_bytes;
    options.audio_capacity = sizeof audio_bytes;
    result.struct_size = sizeof result;
    status = (uint32_t)gbl_v1_advance(instance.machine, &options, &result);
    if (status != (uint32_t)GBL_V1_OK ||
        result.outcome != GBL_V1_ADVANCE_COMPLETED ||
        result.end_cycle <= GBL_V1_AUDIO_CYCLES_PER_FRAME ||
        result.end_cycle > options.cycle_budget ||
        result.audio_frames_written == 0u ||
        result.audio_bytes_written != result.audio_frames_written * 4u ||
        !all_bytes_equal(audio_bytes + result.audio_bytes_written,
                         sizeof audio_bytes - result.audio_bytes_written,
                         0xA5u) ||
        !observe_public(instance.machine, &observation)) {
        (void)snprintf(failure_category, 64u, "refusal-work-probe-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "positive 2048-cycle probe did not complete work and emit an audio sample");
        goto cleanup;
    }
    suite->bounded_probe_units = options.units;
    suite->bounded_probe_budget = options.cycle_budget;
    suite->bounded_probe_end_cycle = result.end_cycle;
    suite->bounded_probe_audio_bytes = result.audio_bytes_written;
    suite->bounded_probe_audio_frames = result.audio_frames_written;
    okay = true;

cleanup:
    if (!instance_destroy_checked(&instance, failure_category,
                                  failure_detail, failure_detail_size)) {
        okay = false;
    }
    return okay;
}

static bool run_timing_control(const char *rom_path,
                               const gbl_v1_input_event *events,
                               size_t event_count, refusal_suite *suite,
                               char *failure_category, char *failure_detail,
                               size_t failure_detail_size)
{
    consumer_instance instance;
    gbl_v1_input_event control_events[3];
    uint8_t audio_bytes[GBL_V1_MAX_AUDIO_BYTES];
    gbl_v1_advance_result result = { 0 };
    gbl_v1_observation observation = { 0 };
    bool okay = false;

    if (event_count != sizeof control_events / sizeof control_events[0]) {
        (void)snprintf(failure_category, 64u, "timing-control-input-count");
        (void)snprintf(failure_detail, failure_detail_size,
                       "timing control requires the declared three-event input");
        return false;
    }
    memcpy(control_events, events, sizeof control_events);
    control_events[1].cycle = UINT64_C(1536);
    if (!instance_from_fixture(rom_path, control_events, event_count, &instance,
                               failure_category, failure_detail,
                               failure_detail_size)) {
        return false;
    }
    if (!setup_full_advance(instance.machine, audio_bytes, sizeof audio_bytes,
                            &result, PHASE21_CYCLE_BUDGET) ||
        !observe_public(instance.machine, &observation) ||
        observation.mailbox_state != GBL_V1_MAILBOX_FAIL ||
        observation.mailbox_case_id != PHASE21_CASE_ID ||
        observation.logical_input_mask != GBL_V1_KEY_B ||
        observation.applied_input_count != 3u ||
        observation.pending_input_count != 0u) {
        (void)snprintf(failure_category, 64u, "timing-control-mismatch");
        (void)snprintf(failure_detail, failure_detail_size,
                       "shifted A input did not produce the expected guest FAIL with the final B mask");
        goto cleanup;
    }
    suite->timing_control_input_cycle = control_events[1].cycle;
    suite->timing_control_end_cycle = result.end_cycle;
    suite->timing_control_case_id = observation.mailbox_case_id;
    suite->timing_control_mailbox_state = observation.mailbox_state;
    suite->timing_control_final_mask = observation.logical_input_mask;
    suite->timing_control_applied_count = observation.applied_input_count;
    suite->timing_control_pending_count = observation.pending_input_count;
    okay = true;

cleanup:
    if (!instance_destroy_checked(&instance, failure_category,
                                  failure_detail, failure_detail_size)) {
        okay = false;
    }
    return okay;
}

static bool run_refusal_suite(
    const char *rom_path, const gbl_v1_input_event *events, size_t event_count,
    const gbl_v1_advance_result *reference_result,
    const gbl_v1_observation *reference_observation,
    const uint8_t *reference_video, const uint8_t *reference_audio,
    bool mutate_canary, refusal_suite *suite,
    char *failure_category, char *failure_detail,
    size_t failure_detail_size)
{
    memset(suite, 0, sizeof *suite);
    if (!run_timing_control(rom_path, events, event_count, suite,
                            failure_category, failure_detail,
                            failure_detail_size) ||
        !run_content_refusal(rom_path, events, event_count, reference_result,
                             reference_observation, reference_video,
                             reference_audio, suite, failure_category,
                             failure_detail, failure_detail_size) ||
        !run_advance_refusal(rom_path, events, event_count, reference_result,
                             reference_observation, reference_video,
                             reference_audio, suite, REFUSE_ADVANCE_SIZE, false,
                             failure_category, failure_detail,
                             failure_detail_size) ||
        !run_advance_refusal(rom_path, events, event_count, reference_result,
                             reference_observation, reference_video,
                             reference_audio, suite, REFUSE_AUDIO_CAPACITY,
                             mutate_canary, failure_category,
                             failure_detail, failure_detail_size) ||
        !run_bounded_probe(rom_path, events, event_count, suite,
                           failure_category, failure_detail,
                           failure_detail_size) ||
        !run_advance_refusal(rom_path, events, event_count, reference_result,
                             reference_observation, reference_video,
                             reference_audio, suite, REFUSE_WORK_BUDGET, false,
                             failure_category, failure_detail,
                             failure_detail_size) ||
        !run_video_refusal(rom_path, events, event_count, reference_result,
                           reference_observation, reference_video,
                           reference_audio, suite, failure_category,
                           failure_detail, failure_detail_size)) {
        return false;
    }
    return suite->case_count ==
           sizeof suite->cases / sizeof suite->cases[0];
}

static bool append_jsonf(char *buffer, size_t capacity, size_t *used,
                         const char *format, ...)
{
    va_list arguments;
    int count;

    if (*used >= capacity) {
        return false;
    }
    va_start(arguments, format);
    count = vsnprintf(buffer + *used, capacity - *used, format, arguments);
    va_end(arguments);
    if (count < 0 || (size_t)count >= capacity - *used) {
        return false;
    }
    *used += (size_t)count;
    return true;
}

static bool append_observation_json(char *buffer, size_t capacity,
                                    size_t *used,
                                    const gbl_v1_observation *observation)
{
    return append_jsonf(
               buffer, capacity, used,
               "{\"struct_size\":%llu,\"loaded\":%u,\"profile\":%u,"
               "\"profile_version\":%u,\"bios_mode\":%u,"
               "\"cartridge_mode\":%u,\"video_state\":%u,"
               "\"mailbox_state\":%u,\"hardware_qualification\":%u,"
               "\"raw_video_format\":%u,\"raw_audio_format\":%u,"
               "\"audio_sample_rate\":%u,\"audio_cycles_per_frame\":%u,"
               "\"audio_channels\":%u,\"logical_input_mask\":%u,"
               "\"reserved\":%u,\"applied_input_count\":%u,"
               "\"pending_input_count\":%u,\"mailbox_case_id\":%u,"
               "\"maximum_input_events\":%u,\"maximum_rom_bytes\":%u,"
               "\"video_width\":%u,\"video_height\":%u,"
               "\"maximum_audio_bytes\":%u,\"model_limit_flags\":%u,"
               "\"capabilities\":%llu,\"input_generation\":%llu,"
               "\"master_cycle\":%llu,\"retired_instructions\":%llu,"
               "\"video_epoch\":%llu,\"maximum_advance_cycles\":%llu,"
               "\"profile_id\":\"%s\",\"bios_id\":\"%s\","
               "\"cartridge_id\":\"%s\"}",
               (unsigned long long)observation->struct_size,
               (unsigned)observation->loaded, (unsigned)observation->profile,
               (unsigned)observation->profile_version,
               (unsigned)observation->bios_mode,
               (unsigned)observation->cartridge_mode,
               (unsigned)observation->video_state,
               (unsigned)observation->mailbox_state,
               (unsigned)observation->hardware_qualification,
               (unsigned)observation->raw_video_format,
               (unsigned)observation->raw_audio_format,
               (unsigned)observation->audio_sample_rate,
               (unsigned)observation->audio_cycles_per_frame,
               (unsigned)observation->audio_channels,
               (unsigned)observation->logical_input_mask,
               (unsigned)observation->reserved,
               (unsigned)observation->applied_input_count,
               (unsigned)observation->pending_input_count,
               (unsigned)observation->mailbox_case_id,
               (unsigned)observation->maximum_input_events,
               (unsigned)observation->maximum_rom_bytes,
               (unsigned)observation->video_width,
               (unsigned)observation->video_height,
               (unsigned)observation->maximum_audio_bytes,
               (unsigned)observation->model_limit_flags,
               (unsigned long long)observation->capabilities,
               (unsigned long long)observation->input_generation,
               (unsigned long long)observation->master_cycle,
               (unsigned long long)observation->retired_instructions,
               (unsigned long long)observation->video_epoch,
               (unsigned long long)observation->maximum_advance_cycles,
               observation->profile_id, observation->bios_id,
               observation->cartridge_id);
}

static bool append_refusal_proof(char *buffer, size_t capacity,
                                 const refusal_suite *suite)
{
    size_t used = 0u;
    size_t index;

    if (!append_jsonf(buffer, capacity, &used,
                      "{\"timing_control\":{\"input_cycle\":%llu,"
                      "\"end_cycle\":%llu,\"case_id\":%u,"
                      "\"status\":\"%s\",\"final_mask\":%u,"
                      "\"applied\":%u,\"pending\":%u},"
                      "\"bounded_probe\":{\"units\":%u,"
                      "\"cycle_budget\":%llu,\"end_cycle\":%llu,"
                      "\"audio_bytes\":%llu,\"audio_frames\":%llu},"
                      "\"cases\":[",
                      (unsigned long long)suite->timing_control_input_cycle,
                      (unsigned long long)suite->timing_control_end_cycle,
                      (unsigned)suite->timing_control_case_id,
                      mailbox_name(suite->timing_control_mailbox_state),
                      (unsigned)suite->timing_control_final_mask,
                      (unsigned)suite->timing_control_applied_count,
                      (unsigned)suite->timing_control_pending_count,
                      (unsigned)suite->bounded_probe_units,
                      (unsigned long long)suite->bounded_probe_budget,
                      (unsigned long long)suite->bounded_probe_end_cycle,
                      (unsigned long long)suite->bounded_probe_audio_bytes,
                      (unsigned long long)suite->bounded_probe_audio_frames)) {
        return false;
    }
    for (index = 0u; index < suite->case_count; ++index) {
        const refusal_record *record = &suite->cases[index];
        const char *sentinel = strcmp(record->name, "short-rom") == 0
                                   ? "0x3c"
                                   : "0x5a";

        if (index > 0u && !append_jsonf(buffer, capacity, &used, ",")) {
            return false;
        }
        if (!append_jsonf(
                buffer, capacity, &used,
                "{\"name\":\"%s\",\"expected_status\":\"%s\","
                "\"observed_status\":\"%s\",\"descriptor_size\":%llu,"
                "\"requested_units\":%llu,\"cycle_budget\":%llu,"
                "\"requested_capacity\":%llu,\"committed_cycles\":%llu,"
                "\"caller_buffer\":{\"length\":%llu,"
                "\"sentinel_before\":\"%s\",\"unchanged\":%s},"
                "\"result_canary_unchanged\":%s,"
                "\"observation_unchanged\":%s,"
                "\"retry\":{\"matches_clean_reference\":%s,"
                "\"audio_bytes\":%llu,\"video_bytes\":%llu,"
                "\"end_cycle\":%llu},\"before\":",
                record->name, record->expected_status,
                status_name(record->observed_status),
                (unsigned long long)record->descriptor_size,
                (unsigned long long)record->requested_units,
                (unsigned long long)record->cycle_budget,
                (unsigned long long)record->requested_capacity,
                (unsigned long long)record->committed_cycles,
                (unsigned long long)record->caller_bytes, sentinel,
                record->caller_bytes_unchanged ? "true" : "false",
                record->has_result
                    ? (record->result_unchanged ? "true" : "false")
                    : "null",
                record->observation_unchanged ? "true" : "false",
                record->retry_matches ? "true" : "false",
                (unsigned long long)record->retry_audio_bytes,
                (unsigned long long)record->retry_video_bytes,
                (unsigned long long)record->retry_end_cycle) ||
            !append_observation_json(buffer, capacity, &used,
                                     &record->before) ||
            !append_jsonf(buffer, capacity, &used, ",\"after\":") ||
            !append_observation_json(buffer, capacity, &used,
                                     &record->after) ||
            !append_jsonf(buffer, capacity, &used, "}")) {
            return false;
        }
    }
    return append_jsonf(buffer, capacity, &used, "]}");
}

static bool write_observations(const char *directory,
                               const gbl_v1_observation *observation,
                               const gbl_v1_advance_result *result,
                               const gbl_v1_input_event *events,
                               const uint8_t *video_bytes,
                               const uint8_t *audio_bytes,
                               const char *expected_status,
                               const refusal_suite *refusals)
{
    char json[32768];
    char refusal_json[24576];
    size_t refusal_json_size = 0u;
    int count;

    if (!append_refusal_proof(refusal_json, sizeof refusal_json, refusals)) {
        return false;
    }
    refusal_json_size = strlen(refusal_json);

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
        "{\"x\": 2, \"y\": 1, \"value\": %u}],\n"
        "  \"refusal_proof\": %s\n"
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
        (unsigned)pixel_value(video_bytes, GBL_V1_VIDEO_WIDTH * 1u + 2u),
        refusal_json);
    if (count < 0 || (size_t)count >= sizeof json ||
        refusal_json_size >= sizeof refusal_json) {
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
    refusal_suite refusals = { 0 };
    uint32_t expected_mailbox = GBL_V1_MAILBOX_PASS;
    uint64_t expected_frames;
    size_t expected_audio_bytes;
    char refusal_failure_category[64] = "";
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
    } else if (argc == 4 &&
               strcmp(argv[3], "--refusal-canary-control") == 0) {
        mode = RUN_MUTATE_REFUSAL_CANARY;
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

    if (!run_refusal_suite(
            rom_path, events, sizeof events / sizeof events[0], &result,
            &observation, video_bytes, audio_bytes,
            mode == RUN_MUTATE_REFUSAL_CANARY, &refusals,
            refusal_failure_category, failure_detail,
            sizeof failure_detail)) {
        failure_category = refusal_failure_category;
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
                            mailbox_name(expected_mailbox), &refusals)) {
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
