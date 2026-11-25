#include <gamebloke/gamebloke_v1.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    CALL_LIMIT = 512,
    CALL_UNITS = 512,
    CALL_BUDGET = 8192,
    CYCLE_LIMIT = 210000,
    AUDIO_CALL_CAPACITY = 4096,
    AUDIO_TOTAL_CAPACITY = 2048,
    VIDEO_STORAGE_CAPACITY = GBL_V1_VIDEO_BYTES + 4
};

typedef struct allocation_slot {
    void *pointer;
    size_t size;
} allocation_slot;

typedef struct allocator_state {
    allocation_slot slots[32];
    size_t live_count;
    bool invalid_release;
} allocator_state;

typedef struct call_record {
    uint64_t requested_budget;
    uint64_t start_cycle;
    uint64_t end_cycle;
    uint64_t elapsed_cycles;
    uint64_t retired_instructions;
    uint64_t observed_retired_total;
    uint32_t requested_units;
    uint32_t mailbox_state;
    uint32_t mailbox_case;
    uint32_t audio_frames;
    uint32_t audio_bytes;
    uint16_t logical_mask;
    uint32_t applied_events;
    uint32_t pending_events;
} call_record;

typedef struct session_mode {
    bool shift_first_input;
    bool wrong_video_literal;
    bool refusal_retry;
} session_mode;

static void *consumer_allocate(void *context, size_t size)
{
    allocator_state *allocator = context;
    size_t index;

    for (index = 0u; index < sizeof allocator->slots / sizeof allocator->slots[0];
         ++index) {
        if (allocator->slots[index].pointer == NULL) {
            void *pointer = malloc(size);

            if (pointer == NULL) {
                return NULL;
            }
            allocator->slots[index].pointer = pointer;
            allocator->slots[index].size = size;
            allocator->live_count += 1u;
            return pointer;
        }
    }
    return NULL;
}

static void consumer_release(void *context, void *pointer, size_t size)
{
    allocator_state *allocator = context;
    size_t index;

    for (index = 0u; index < sizeof allocator->slots / sizeof allocator->slots[0];
         ++index) {
        allocation_slot *slot = &allocator->slots[index];

        if (slot->pointer == pointer) {
            if (slot->size != size) {
                allocator->invalid_release = true;
            }
            free(slot->pointer);
            slot->pointer = NULL;
            slot->size = 0u;
            allocator->live_count -= 1u;
            return;
        }
    }
    allocator->invalid_release = true;
}

static bool all_bytes_are(const uint8_t *bytes, size_t length, uint8_t value)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        if (bytes[index] != value) {
            return false;
        }
    }
    return true;
}

static bool output_path(char *destination, size_t capacity,
                        const char *directory, const char *name)
{
    int length = snprintf(destination, capacity, "%s/%s", directory, name);

    return length > 0 && (size_t)length < capacity;
}

static bool write_new_file(const char *directory, const char *name,
                           const void *bytes, size_t length)
{
    char path[4096];
    FILE *file;
    bool okay;

    if (!output_path(path, sizeof path, directory, name)) {
        return false;
    }
    file = fopen(path, "wbx");
    if (file == NULL) {
        return false;
    }
    okay = fwrite(bytes, 1u, length, file) == length && fflush(file) == 0;
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay;
}

static uint64_t host_elapsed_ns(struct timespec start, struct timespec end)
{
    int64_t seconds = (int64_t)end.tv_sec - (int64_t)start.tv_sec;
    int64_t nanoseconds = (int64_t)end.tv_nsec - (int64_t)start.tv_nsec;

    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += INT64_C(1000000000);
    }
    if (seconds < 0) {
        return 0u;
    }
    return (uint64_t)seconds * UINT64_C(1000000000) +
           (uint64_t)nanoseconds;
}

static bool observation_is_profile(const gbl_v1_observation *observation,
                                   uint32_t loaded)
{
    return observation->loaded == loaded &&
           observation->profile == GBL_V1_PROFILE_AGB &&
           observation->profile_version == 16u &&
           observation->bios_mode == 0u &&
           observation->cartridge_mode == GBL_V1_CARTRIDGE_ROM_ONLY &&
           memcmp(observation->profile_id, "agb", 4u) == 0 &&
           memcmp(observation->bios_id, "none", 5u) == 0 &&
           observation->hardware_qualification ==
               GBL_V1_QUALIFICATION_UNKNOWN;
}

static bool observations_equal(const gbl_v1_observation *left,
                              const gbl_v1_observation *right)
{
#define SAME_FIELD(field) (left->field == right->field)
    return SAME_FIELD(loaded) && SAME_FIELD(profile) &&
           SAME_FIELD(profile_version) && SAME_FIELD(bios_mode) &&
           SAME_FIELD(cartridge_mode) && SAME_FIELD(video_state) &&
           SAME_FIELD(mailbox_state) && SAME_FIELD(hardware_qualification) &&
           SAME_FIELD(raw_video_format) && SAME_FIELD(raw_audio_format) &&
           SAME_FIELD(audio_sample_rate) &&
           SAME_FIELD(audio_cycles_per_frame) && SAME_FIELD(audio_channels) &&
           SAME_FIELD(logical_input_mask) &&
           SAME_FIELD(applied_input_count) &&
           SAME_FIELD(pending_input_count) &&
           SAME_FIELD(mailbox_case_id) &&
           SAME_FIELD(maximum_input_events) &&
           SAME_FIELD(maximum_rom_bytes) && SAME_FIELD(video_width) &&
           SAME_FIELD(video_height) && SAME_FIELD(maximum_audio_bytes) &&
           SAME_FIELD(model_limit_flags) && SAME_FIELD(capabilities) &&
           SAME_FIELD(input_generation) && SAME_FIELD(master_cycle) &&
           SAME_FIELD(retired_instructions) && SAME_FIELD(video_epoch) &&
           SAME_FIELD(maximum_advance_cycles) &&
           memcmp(left->profile_id, right->profile_id, sizeof left->profile_id) == 0 &&
           memcmp(left->bios_id, right->bios_id, sizeof left->bios_id) == 0 &&
           memcmp(left->cartridge_id, right->cartridge_id,
                  sizeof left->cartridge_id) == 0;
#undef SAME_FIELD
}

static bool create_session(const uint8_t *rom_bytes, size_t rom_length,
                           bool shift_first, allocator_state *allocator,
                           gbl_v1_machine **machine,
                           gbl_v1_input_event events[3])
{
    gbl_v1_config config = { 0 };
    gbl_v1_rom rom = { 0 };
    gbl_v1_input_timeline timeline = { 0 };
    gbl_v1_observation observation = { 0 };

    config.struct_size = sizeof config;
    config.profile = GBL_V1_PROFILE_AGB;
    config.start_mode = GBL_V1_START_DIRECT;
    config.cartridge_mode = GBL_V1_CARTRIDGE_ROM_ONLY;
    config.allocator_context = allocator;
    config.allocate = consumer_allocate;
    config.release = consumer_release;
    if (gbl_v1_create(&config, machine) != GBL_V1_OK || *machine == NULL) {
        fputs("phase23: setup:create failed\n", stderr);
        return false;
    }
    observation.struct_size = sizeof observation;
    if (gbl_v1_observe(*machine, &observation) != GBL_V1_OK ||
        !observation_is_profile(&observation, 0u)) {
        fprintf(stderr,
            "phase23: setup:unloaded observation mismatch loaded=%u profile=%u version=%u bios=%u cart=%u qualification=%u id=%.4s\n",
            observation.loaded, observation.profile, observation.profile_version,
            observation.bios_mode, observation.cartridge_mode,
            observation.hardware_qualification, observation.profile_id);
        return false;
    }
    rom.struct_size = sizeof rom;
    rom.bytes = rom_bytes;
    rom.length = rom_length;
    if (gbl_v1_load_rom(*machine, &rom) != GBL_V1_OK) {
        fputs("phase23: setup:load ROM failed\n", stderr);
        return false;
    }
    events[0] = (gbl_v1_input_event){
        shift_first ? UINT64_C(8624) : UINT64_C(2464), GBL_V1_KEY_A, 0u
    };
    events[1] = (gbl_v1_input_event){ UINT64_C(12320), GBL_V1_KEY_LEFT, 0u };
    events[2] = (gbl_v1_input_event){ UINT64_C(24640), 0u, 0u };
    timeline.struct_size = sizeof timeline;
    timeline.events = events;
    timeline.count = 3u;
    if (gbl_v1_set_input(*machine, &timeline) != GBL_V1_OK) {
        fputs("phase23: setup:input timeline failed\n", stderr);
        return false;
    }
    memset(&observation, 0, sizeof observation);
    observation.struct_size = sizeof observation;
    if (gbl_v1_observe(*machine, &observation) != GBL_V1_OK ||
        !observation_is_profile(&observation, 1u) ||
        observation.mailbox_state != GBL_V1_MAILBOX_ABSENT ||
        observation.master_cycle != 0u ||
        observation.applied_input_count != 0u ||
        observation.pending_input_count != 3u) {
        fprintf(stderr,
            "phase23: setup:loaded observation mismatch loaded=%u version=%u mailbox=%u cycle=%llu applied=%u pending=%u id=%.4s bios=%.5s\n",
            observation.loaded, observation.profile_version,
            observation.mailbox_state,
            (unsigned long long)observation.master_cycle,
            observation.applied_input_count, observation.pending_input_count,
            observation.profile_id, observation.bios_id);
        return false;
    }
    return true;
}

static uint16_t expected_logical_mask(uint64_t cycle, bool shift_first)
{
    uint64_t first_cycle = shift_first ? UINT64_C(8624) : UINT64_C(2464);

    if (cycle < first_cycle) {
        return 0u;
    }
    if (cycle < UINT64_C(12320)) {
        return GBL_V1_KEY_A;
    }
    if (cycle < UINT64_C(24640)) {
        return GBL_V1_KEY_LEFT;
    }
    return 0u;
}

static uint32_t expected_applied_events(uint64_t cycle, bool shift_first)
{
    uint64_t first_cycle = shift_first ? UINT64_C(8624) : UINT64_C(2464);

    if (cycle < first_cycle) return 0u;
    if (cycle < UINT64_C(12320)) return 1u;
    if (cycle < UINT64_C(24640)) return 2u;
    return 3u;
}

static void append_call(char *trace, size_t capacity, size_t *used,
                        const call_record *record)
{
    int length = snprintf(trace + *used, capacity - *used,
        "{\"requested_units\":%u,\"requested_budget\":%llu,"
        "\"start_cycle\":%llu,\"end_cycle\":%llu,"
        "\"elapsed_cycles\":%llu,\"retired_instructions\":%llu,"
        "\"observed_retired_total\":%llu,"
        "\"audio_frames\":%u,\"audio_bytes\":%u,"
        "\"logical_input_mask\":%u,\"applied_input_count\":%u,"
        "\"pending_input_count\":%u,\"mailbox_state\":%u,"
        "\"mailbox_case_id\":%u}\n",
        record->requested_units,
        (unsigned long long)record->requested_budget,
        (unsigned long long)record->start_cycle,
        (unsigned long long)record->end_cycle,
        (unsigned long long)record->elapsed_cycles,
        (unsigned long long)record->retired_instructions,
        (unsigned long long)record->observed_retired_total,
        record->audio_frames, record->audio_bytes,
        (unsigned)record->logical_mask, record->applied_events,
        record->pending_events, record->mailbox_state, record->mailbox_case);

    if (length <= 0 || (size_t)length >= capacity - *used) {
        *used = capacity;
        return;
    }
    *used += (size_t)length;
}

static bool check_video(const uint8_t *bytes, size_t capacity,
                        bool wrong_literal, size_t *bad_offset)
{
    size_t offset;

    if (capacity < VIDEO_STORAGE_CAPACITY ||
        !all_bytes_are(bytes, 480u, 0u) ||
        !all_bytes_are(bytes + GBL_V1_VIDEO_BYTES, 4u, 0xa5u)) {
        *bad_offset = capacity < GBL_V1_VIDEO_BYTES ? capacity : 0u;
        return false;
    }
    for (offset = 480u; offset < GBL_V1_VIDEO_BYTES; offset += 2u) {
        const uint8_t expected_low =
            (wrong_literal && offset == 480u) ? UINT8_C(0xab) : UINT8_C(0xaa);

        if (bytes[offset] != expected_low || bytes[offset + 1u] != 0x56u) {
            *bad_offset = offset;
            return false;
        }
    }
    return true;
}

static bool check_audio(const uint8_t *bytes, size_t length)
{
    size_t frames;
    size_t index;
    bool steady_seen = false;
    unsigned transitions = 0u;

    if ((length % 4u) != 0u || length < 5u * 4u) {
        return false;
    }
    frames = length / 4u;
    for (index = 0u; index < frames; ++index) {
        const uint8_t left_low = bytes[index * 4u];
        const uint8_t left_high = bytes[index * 4u + 1u];
        const uint8_t right_low = bytes[index * 4u + 2u];
        const uint8_t right_high = bytes[index * 4u + 3u];
        const bool is_zero = left_low == 0u && left_high == 0u;
        const bool is_steady = left_low == 0u && left_high == 0xdbu;

        if ((!is_zero && !is_steady) || right_low != 0u || right_high != 0u) {
            return false;
        }
        if (is_steady && !steady_seen) {
            steady_seen = true;
            ++transitions;
        } else if (is_zero && steady_seen) {
            ++transitions;
        }
        if (index < 2u && !is_zero) {
            return false;
        }
        if (index >= 3u && !is_steady) {
            return false;
        }
    }
    return steady_seen && transitions == 1u;
}

static bool result_matches_record(const gbl_v1_advance_result *result,
                                  const call_record *record,
                                  const gbl_v1_observation *observation)
{
    return observations_equal(&result->observation, observation) &&
           result->outcome == GBL_V1_ADVANCE_COMPLETED &&
           result->stop_reason == GBL_V1_STOP_UNIT_LIMIT &&
           result->requested_units == record->requested_units &&
           result->requested_cycle_budget == record->requested_budget &&
           result->start_cycle == record->start_cycle &&
           result->end_cycle == record->end_cycle &&
           result->elapsed_cycles == record->elapsed_cycles &&
           result->retired_instructions == record->retired_instructions &&
           result->observation.retired_instructions ==
               record->observed_retired_total &&
           result->audio_frames_written == record->audio_frames &&
           result->audio_bytes_written == record->audio_bytes &&
           result->observation.master_cycle == record->end_cycle &&
           result->observation.mailbox_state == record->mailbox_state &&
           result->observation.mailbox_case_id == record->mailbox_case &&
           result->observation.logical_input_mask == record->logical_mask &&
           result->observation.applied_input_count == record->applied_events &&
           result->observation.pending_input_count == record->pending_events;
}

static bool run_refusal_retry(gbl_v1_machine *machine,
                              const uint8_t *rom_bytes, size_t rom_length,
                              const call_record *prefix,
                              const uint8_t *prefix_audio,
                              uint8_t *retry_audio, size_t retry_capacity,
                              call_record *retry_record,
                              gbl_v1_observation *retry_observation,
                              uint32_t refusal_statuses[2],
                              uint64_t *retry_host_ns,
                              uint8_t *reference_audio_total,
                              size_t reference_audio_capacity,
                              size_t *reference_audio_used,
                              char *reference_trace,
                              size_t reference_trace_capacity,
                              size_t *reference_trace_used,
                              uint8_t *reference_video,
                              size_t reference_video_capacity,
                              gbl_v1_observation *reference_final_observation)
{
    allocator_state reference_allocator = { 0 };
    gbl_v1_machine *reference = NULL;
    gbl_v1_input_event reference_events[3] = { { 0u, 0u, 0u } };
    uint8_t reference_audio[AUDIO_CALL_CAPACITY];
    uint8_t refused_audio[GBL_V1_MAX_AUDIO_BYTES];
    uint8_t reference_retry_audio[AUDIO_CALL_CAPACITY];
    gbl_v1_advance_options options = { 0 };
    gbl_v1_advance_result reference_result = { 0 };
    gbl_v1_advance_result main_result = { 0 };
    gbl_v1_observation before = { 0 };
    gbl_v1_observation after = { 0 };
    gbl_v1_observation reference_observation = { 0 };
    size_t expected_bytes;
    size_t attempt;
    size_t reference_calls = 1u;
    uint64_t reference_cycle = 0u;
    bool reference_completed = false;
    bool success = false;
    const char *failure_stage = "prefix validation";

    memset(reference_audio_total, 0xa5, reference_audio_capacity);
    memset(reference_video, 0xa5, reference_video_capacity);
    *reference_audio_used = 0u;
    *reference_trace_used = 0u;
    memset(reference_final_observation, 0,
           sizeof *reference_final_observation);
    before.struct_size = sizeof before;
    if (prefix->mailbox_state != GBL_V1_MAILBOX_ABSENT ||
        gbl_v1_observe(machine, &before) != GBL_V1_OK ||
        before.master_cycle != prefix->end_cycle ||
        before.mailbox_state != GBL_V1_MAILBOX_ABSENT ||
        !create_session(rom_bytes, rom_length, false, &reference_allocator,
                        &reference, reference_events)) {
        goto cleanup;
    }
    memset(reference_audio, 0xa5, sizeof reference_audio);
    options.struct_size = sizeof options;
    failure_stage = "fresh reference prefix advance";
    options.units = prefix->requested_units;
    options.cycle_budget = prefix->requested_budget;
    options.capture_audio = 1u;
    options.audio_bytes = reference_audio;
    options.audio_capacity = sizeof reference_audio;
    reference_result.struct_size = sizeof reference_result;
    if (gbl_v1_advance(reference, &options, &reference_result) != GBL_V1_OK) {
        goto cleanup;
    }
    expected_bytes = (size_t)prefix->audio_bytes;
    if (expected_bytes > sizeof reference_audio ||
        memcmp(reference_audio, prefix_audio, expected_bytes) != 0) {
        goto cleanup;
    }
    if (expected_bytes > reference_audio_capacity) {
        goto cleanup;
    }
    memcpy(reference_audio_total, reference_audio, expected_bytes);
    *reference_audio_used = expected_bytes;
    append_call(reference_trace, reference_trace_capacity,
                reference_trace_used, prefix);
    if (*reference_trace_used >= reference_trace_capacity) {
        goto cleanup;
    }
    reference_observation.struct_size = sizeof reference_observation;
    if (gbl_v1_observe(reference, &reference_observation) != GBL_V1_OK ||
        !observations_equal(&before, &reference_observation) ||
        !result_matches_record(&reference_result, prefix,
                               &reference_observation)) {
        goto cleanup;
    }

    for (attempt = 0u; attempt < 2u; ++attempt) {
        gbl_v1_advance_result refused_result;
        gbl_v1_advance_result refused_snapshot;

        memset(refused_audio, 0xa5, sizeof refused_audio);
        memset(&refused_result, 0xa5, sizeof refused_result);
        failure_stage = "positive-budget refusal status and output preservation";
        refused_result.struct_size = sizeof refused_result;
        memcpy(&refused_snapshot, &refused_result, sizeof refused_result);
        options.struct_size = sizeof options;
        options.units = CALL_UNITS;
        options.cycle_budget = 512u;
        options.capture_audio = 1u;
        options.audio_bytes = refused_audio;
        options.audio_capacity = sizeof refused_audio;
        refusal_statuses[attempt] = (uint32_t)gbl_v1_advance(
            machine, &options, &refused_result);
        if (refusal_statuses[attempt] != GBL_V1_WORK_EXHAUSTED ||
            memcmp(&refused_result, &refused_snapshot,
                   sizeof refused_result) != 0 ||
            !all_bytes_are(refused_audio, sizeof refused_audio, 0xa5u)) {
            goto cleanup;
        }
        memset(&after, 0, sizeof after);
        after.struct_size = sizeof after;
        if (gbl_v1_observe(machine, &after) != GBL_V1_OK ||
            !observations_equal(&before, &after)) {
            goto cleanup;
        }
    }

    failure_stage = "corrected retry equivalence";
    memset(retry_audio, 0xa5, retry_capacity);
    memset(reference_retry_audio, 0xa5, sizeof reference_retry_audio);
    options.struct_size = sizeof options;
    options.units = CALL_UNITS;
    options.cycle_budget = CALL_BUDGET;
    options.capture_audio = 1u;
    options.audio_bytes = retry_audio;
    options.audio_capacity = retry_capacity;
    main_result.struct_size = sizeof main_result;
    {
        struct timespec start = { 0 };
        struct timespec end = { 0 };

        (void)timespec_get(&start, TIME_UTC);
        if (gbl_v1_advance(machine, &options, &main_result) != GBL_V1_OK) {
            goto cleanup;
        }
        (void)timespec_get(&end, TIME_UTC);
        *retry_host_ns = host_elapsed_ns(start, end);
    }
    if (main_result.outcome != GBL_V1_ADVANCE_COMPLETED ||
        main_result.requested_units != CALL_UNITS ||
        main_result.requested_cycle_budget != CALL_BUDGET) goto cleanup;
    options.audio_bytes = reference_retry_audio;
    options.audio_capacity = sizeof reference_retry_audio;
    reference_result.struct_size = sizeof reference_result;
    if (gbl_v1_advance(reference, &options, &reference_result) != GBL_V1_OK ||
        reference_result.outcome != GBL_V1_ADVANCE_COMPLETED ||
        main_result.requested_units != reference_result.requested_units ||
        main_result.requested_cycle_budget != reference_result.requested_cycle_budget ||
        main_result.start_cycle != reference_result.start_cycle ||
        main_result.end_cycle != reference_result.end_cycle ||
        main_result.elapsed_cycles != reference_result.elapsed_cycles ||
        main_result.retired_instructions != reference_result.retired_instructions ||
        main_result.audio_frames_written != reference_result.audio_frames_written ||
        main_result.audio_bytes_written != reference_result.audio_bytes_written ||
        !observations_equal(&main_result.observation,
                            &reference_result.observation) ||
        main_result.audio_bytes_written > retry_capacity ||
        memcmp(retry_audio, reference_retry_audio,
               main_result.audio_bytes_written) != 0 ||
        !all_bytes_are(retry_audio + main_result.audio_bytes_written,
                       retry_capacity - main_result.audio_bytes_written, 0xa5u)) {
        goto cleanup;
    }
    memset(retry_observation, 0, sizeof *retry_observation);
    retry_observation->struct_size = sizeof *retry_observation;
    if (gbl_v1_observe(machine, retry_observation) != GBL_V1_OK ||
        !observations_equal(retry_observation,
                            &reference_result.observation)) {
        goto cleanup;
    }
    retry_record->requested_budget = main_result.requested_cycle_budget;
    retry_record->start_cycle = main_result.start_cycle;
    retry_record->end_cycle = main_result.end_cycle;
    retry_record->elapsed_cycles = main_result.elapsed_cycles;
    retry_record->retired_instructions = main_result.retired_instructions;
    retry_record->observed_retired_total =
        retry_observation->retired_instructions;
    retry_record->requested_units = main_result.requested_units;
    retry_record->mailbox_state = retry_observation->mailbox_state;
    retry_record->mailbox_case = retry_observation->mailbox_case_id;
    retry_record->audio_frames = (uint32_t)main_result.audio_frames_written;
    retry_record->audio_bytes = (uint32_t)main_result.audio_bytes_written;
    retry_record->logical_mask = retry_observation->logical_input_mask;
    retry_record->applied_events = retry_observation->applied_input_count;
    retry_record->pending_events = retry_observation->pending_input_count;
    expected_bytes = (size_t)reference_result.audio_bytes_written;
    if (expected_bytes > reference_audio_capacity - *reference_audio_used ||
        expected_bytes > sizeof reference_retry_audio) {
        goto cleanup;
    }
    memcpy(reference_audio_total + *reference_audio_used,
           reference_retry_audio, expected_bytes);
    *reference_audio_used += expected_bytes;
    append_call(reference_trace, reference_trace_capacity,
                reference_trace_used, retry_record);
    if (*reference_trace_used >= reference_trace_capacity) {
        goto cleanup;
    }

    reference_cycle = reference_result.end_cycle;
    reference_calls = 2u;
    failure_stage = "fresh reference completion";
    while (reference_calls < CALL_LIMIT && reference_cycle < CYCLE_LIMIT) {
        uint8_t next_audio[AUDIO_CALL_CAPACITY];
        uint64_t budget = CALL_BUDGET;
        uint64_t expected_frames;
        size_t next_bytes;
        size_t bad_offset = 0u;
        gbl_v1_advance_result next_result = { 0 };
        gbl_v1_observation next_observation = { 0 };
        call_record next_record = { 0 };

        if (budget > CYCLE_LIMIT - reference_cycle) {
            budget = CYCLE_LIMIT - reference_cycle;
        }
        if (budget == 0u) {
            break;
        }
        memset(next_audio, 0xa5, sizeof next_audio);
        options.struct_size = sizeof options;
        options.units = CALL_UNITS;
        options.cycle_budget = budget;
        options.capture_audio = 1u;
        options.audio_bytes = next_audio;
        options.audio_capacity = sizeof next_audio;
        next_result.struct_size = sizeof next_result;
        if (gbl_v1_advance(reference, &options, &next_result) != GBL_V1_OK ||
            next_result.requested_units != CALL_UNITS ||
            next_result.requested_cycle_budget != budget ||
            next_result.start_cycle != reference_cycle ||
            next_result.end_cycle <= next_result.start_cycle ||
            next_result.end_cycle > next_result.start_cycle + budget ||
            next_result.elapsed_cycles !=
                next_result.end_cycle - next_result.start_cycle ||
            next_result.end_cycle > CYCLE_LIMIT ||
            next_result.retired_instructions == 0u) {
            goto cleanup;
        }
        expected_frames = next_result.end_cycle /
                              GBL_V1_AUDIO_CYCLES_PER_FRAME -
                          next_result.start_cycle /
                              GBL_V1_AUDIO_CYCLES_PER_FRAME;
        next_bytes = (size_t)expected_frames * 4u;
        if (next_result.audio_available != 1u ||
            next_result.audio_frames_written != expected_frames ||
            next_result.audio_bytes_written != next_bytes ||
            next_bytes > sizeof next_audio ||
            !all_bytes_are(next_audio + next_bytes,
                           sizeof next_audio - next_bytes, 0xa5u) ||
            next_bytes > reference_audio_capacity - *reference_audio_used) {
            goto cleanup;
        }
        memcpy(reference_audio_total + *reference_audio_used,
               next_audio, next_bytes);
        *reference_audio_used += next_bytes;
        next_observation.struct_size = sizeof next_observation;
        if (gbl_v1_observe(reference, &next_observation) != GBL_V1_OK ||
            !observations_equal(&next_observation,
                                &next_result.observation) ||
            next_observation.master_cycle != next_result.end_cycle ||
            next_observation.logical_input_mask !=
                expected_logical_mask(next_result.end_cycle, false) ||
            next_observation.applied_input_count !=
                expected_applied_events(next_result.end_cycle, false) ||
            next_observation.pending_input_count !=
                3u - next_observation.applied_input_count) {
            goto cleanup;
        }
        next_record.requested_budget = budget;
        next_record.start_cycle = next_result.start_cycle;
        next_record.end_cycle = next_result.end_cycle;
        next_record.elapsed_cycles = next_result.elapsed_cycles;
        next_record.retired_instructions = next_result.retired_instructions;
        next_record.observed_retired_total =
            next_observation.retired_instructions;
        next_record.requested_units = next_result.requested_units;
        next_record.mailbox_state = next_observation.mailbox_state;
        next_record.mailbox_case = next_observation.mailbox_case_id;
        next_record.audio_frames = (uint32_t)next_result.audio_frames_written;
        next_record.audio_bytes = (uint32_t)next_result.audio_bytes_written;
        next_record.logical_mask = next_observation.logical_input_mask;
        next_record.applied_events = next_observation.applied_input_count;
        next_record.pending_events = next_observation.pending_input_count;
        append_call(reference_trace, reference_trace_capacity,
                    reference_trace_used, &next_record);
        if (*reference_trace_used >= reference_trace_capacity) {
            goto cleanup;
        }
        reference_cycle = next_result.end_cycle;
        reference_calls += 1u;
        if (next_observation.mailbox_state == GBL_V1_MAILBOX_FAIL) {
            goto cleanup;
        }
        if (next_observation.mailbox_state == GBL_V1_MAILBOX_PASS) {
            gbl_v1_video request = { 0 };

            if (next_result.outcome != GBL_V1_ADVANCE_COMPLETED ||
                next_result.stop_reason != GBL_V1_STOP_UNIT_LIMIT ||
                next_observation.mailbox_case_id != UINT32_C(0x23010001) ||
                next_observation.video_state != GBL_V1_VIDEO_VALID ||
                next_observation.video_epoch != 0u ||
                reference_cycle < UINT64_C(197120) ||
                reference_cycle >= UINT64_C(280896)) {
                goto cleanup;
            }
            request.struct_size = sizeof request;
            request.bytes = reference_video;
            request.capacity = reference_video_capacity;
            if (gbl_v1_read_video(reference, &request) != GBL_V1_OK ||
                !check_video(reference_video, reference_video_capacity,
                             false, &bad_offset) ||
                !check_audio(reference_audio_total,
                             *reference_audio_used)) {
                goto cleanup;
            }
            *reference_final_observation = next_observation;
            reference_completed = true;
            break;
        }
    }
    if (!reference_completed || reference_calls >= CALL_LIMIT ||
        reference_cycle >= CYCLE_LIMIT) {
        goto cleanup;
    }
    success = true;

cleanup:
    if (!success) {
        fprintf(stderr,
            "phase23: refusal-proof failed at %s main=%llu reference=%llu calls=%zu statuses=%u,%u\n",
            failure_stage, (unsigned long long)before.master_cycle,
            (unsigned long long)reference_cycle, reference_calls,
            refusal_statuses[0], refusal_statuses[1]);
    }
    if (reference != NULL) {
        gbl_v1_destroy(reference);
    }
    if (reference_allocator.live_count != 0u ||
        reference_allocator.invalid_release) {
        success = false;
    }
    return success;
}

static bool parse_mode(int argc, char **argv, session_mode *mode)
{
    int index;

    for (index = 3; index < argc; ++index) {
        if (strcmp(argv[index], "--shift-first-input") == 0) {
            mode->shift_first_input = true;
        } else if (strcmp(argv[index], "--wrong-video-literal") == 0) {
            mode->wrong_video_literal = true;
        } else if (strcmp(argv[index], "--refusal-retry") == 0) {
            mode->refusal_retry = true;
        } else {
            return false;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    session_mode mode = { false, false, false };
    allocator_state allocator = { 0 };
    gbl_v1_machine *machine = NULL;
    gbl_v1_input_event events[3] = { { 0u, 0u, 0u } };
    uint8_t *rom_bytes = NULL;
    size_t rom_length = 0u;
    uint8_t audio_total[AUDIO_TOTAL_CAPACITY];
    uint8_t reference_audio_total[AUDIO_TOTAL_CAPACITY];
    uint8_t call_audio[AUDIO_CALL_CAPACITY];
    uint8_t video[VIDEO_STORAGE_CAPACITY];
    uint8_t reference_video[VIDEO_STORAGE_CAPACITY];
    char trace[CALL_LIMIT * 512];
    char reference_trace[CALL_LIMIT * 512];
    char host_times[CALL_LIMIT * 96];
    size_t trace_used = 0u;
    size_t host_used = 0u;
    size_t audio_used = 0u;
    size_t reference_audio_used = 0u;
    size_t reference_trace_used = 0u;
    size_t calls = 0u;
    size_t bad_offset = 0u;
    size_t input_checkpoint_calls[3] = { SIZE_MAX, SIZE_MAX, SIZE_MAX };
    uint32_t refusal_statuses[2] = { 0u, 0u };
    uint64_t prior_cycle = 0u;
    uint64_t retry_host_ns = 0u;
    bool refusal_checked = false;
    gbl_v1_observation reference_final_observation = { 0 };
    gbl_v1_observation final_observation = { 0 };
    int exit_code = EXIT_FAILURE;
    FILE *rom_file = NULL;

    if (argc < 3 || !parse_mode(argc, argv, &mode)) {
        fputs("usage: gamebloke-phase23-host-session ROM OUTPUT_DIR "
              "[--shift-first-input] [--wrong-video-literal] "
              "[--refusal-retry]\n", stderr);
        return 2;
    }
    rom_file = fopen(argv[1], "rb");
    if (rom_file == NULL || fseek(rom_file, 0L, SEEK_END) != 0) {
        fputs("phase23: fixture open failed\n", stderr);
        goto cleanup;
    }
    {
        long length = ftell(rom_file);

        if (length < 0L || (unsigned long)length > GBL_V1_MAX_ROM_BYTES ||
            (length & 1L) != 0L || fseek(rom_file, 0L, SEEK_SET) != 0) {
            fputs("phase23: fixture extent is invalid\n", stderr);
            goto cleanup;
        }
        rom_length = (size_t)length;
    }
    rom_bytes = malloc(rom_length);
    if (rom_bytes == NULL || fread(rom_bytes, 1u, rom_length, rom_file) != rom_length ||
        fclose(rom_file) != 0) {
        rom_file = NULL;
        fputs("phase23: fixture read failed\n", stderr);
        goto cleanup;
    }
    rom_file = NULL;
    memset(audio_total, 0xa5, sizeof audio_total);
    memset(reference_audio_total, 0xa5, sizeof reference_audio_total);
    memset(video, 0xa5, sizeof video);
    memset(reference_video, 0xa5, sizeof reference_video);
    if (!create_session(rom_bytes, rom_length, mode.shift_first_input,
                        &allocator, &machine, events)) {
        fputs("phase23: public API create/load/input setup failed\n", stderr);
        goto cleanup;
    }

    while (calls < CALL_LIMIT && prior_cycle < CYCLE_LIMIT) {
        gbl_v1_advance_options options = { 0 };
        gbl_v1_advance_result result = { 0 };
        gbl_v1_observation observation = { 0 };
        call_record record = { 0 };
        struct timespec host_start = { 0 };
        struct timespec host_end = { 0 };
        uint64_t budget = CALL_BUDGET;
        uint64_t expected_frames;
        size_t expected_bytes;
        uint16_t expected_mask;
        int timing_length;
        gbl_v1_status status;

        if (budget > CYCLE_LIMIT - prior_cycle) {
            budget = CYCLE_LIMIT - prior_cycle;
        }
        if (budget == 0u) {
            break;
        }
        memset(call_audio, 0xa5, sizeof call_audio);
        options.struct_size = sizeof options;
        options.units = CALL_UNITS;
        options.cycle_budget = budget;
        options.capture_audio = 1u;
        options.audio_bytes = call_audio;
        options.audio_capacity = sizeof call_audio;
        result.struct_size = sizeof result;
        (void)timespec_get(&host_start, TIME_UTC);
        status = gbl_v1_advance(machine, &options, &result);
        (void)timespec_get(&host_end, TIME_UTC);
        if (status != GBL_V1_OK) {
            fprintf(stderr, "phase23: API advance refused status=%u\n",
                    (unsigned)status);
            goto cleanup;
        }
        if (result.requested_units != CALL_UNITS ||
            result.requested_cycle_budget != budget ||
            result.start_cycle != prior_cycle ||
            result.end_cycle <= result.start_cycle ||
            result.end_cycle > result.start_cycle + budget ||
            result.elapsed_cycles != result.end_cycle - result.start_cycle ||
            result.end_cycle > CYCLE_LIMIT ||
            result.retired_instructions == 0u ||
            result.outcome > GBL_V1_ADVANCE_GUEST_TERMINAL) {
            fputs("phase23: bounded advance interval mismatch\n", stderr);
            goto cleanup;
        }
        expected_frames = result.end_cycle / GBL_V1_AUDIO_CYCLES_PER_FRAME -
                          result.start_cycle / GBL_V1_AUDIO_CYCLES_PER_FRAME;
        expected_bytes = (size_t)expected_frames * 4u;
        if (result.audio_available != 1u ||
            result.audio_frames_written != expected_frames ||
            result.audio_bytes_written != expected_bytes ||
            expected_bytes > sizeof call_audio ||
            !all_bytes_are(call_audio + expected_bytes,
                           sizeof call_audio - expected_bytes, 0xa5u) ||
            expected_bytes > sizeof audio_total - audio_used) {
            fputs("phase23: audio extent or untouched tail mismatch\n", stderr);
            goto cleanup;
        }
        memcpy(audio_total + audio_used, call_audio, expected_bytes);
        audio_used += expected_bytes;
        expected_mask = expected_logical_mask(result.end_cycle,
                                              mode.shift_first_input);
        observation.struct_size = sizeof observation;
        if (gbl_v1_observe(machine, &observation) != GBL_V1_OK) {
            fputs("phase23: public observation failed after advance\n", stderr);
            goto cleanup;
        }
        if (observation.master_cycle != result.end_cycle ||
            observation.retired_instructions !=
                result.observation.retired_instructions ||
            !observations_equal(&observation, &result.observation) ||
            observation.logical_input_mask != expected_mask ||
            observation.applied_input_count !=
                expected_applied_events(result.end_cycle,
                                        mode.shift_first_input) ||
            observation.applied_input_count > 3u ||
            observation.pending_input_count !=
                3u - observation.applied_input_count) {
            fprintf(stderr,
                "phase23: input/cycle observation mismatch call=%zu result-cycle=%llu obs-cycle=%llu result-retired-total=%llu obs-retired-total=%llu mask=%u expected-mask=%u applied=%u pending=%u\n",
                calls, (unsigned long long)result.end_cycle,
                (unsigned long long)observation.master_cycle,
                (unsigned long long)result.observation.retired_instructions,
                (unsigned long long)observation.retired_instructions,
                (unsigned)observation.logical_input_mask,
                (unsigned)expected_mask, observation.applied_input_count,
                observation.pending_input_count);
            goto cleanup;
        }
        if (observation.applied_input_count > 0u &&
            observation.applied_input_count <= 3u &&
            input_checkpoint_calls[observation.applied_input_count - 1u] == SIZE_MAX) {
            input_checkpoint_calls[observation.applied_input_count - 1u] = calls;
        }
        record.requested_budget = budget;
        record.start_cycle = result.start_cycle;
        record.end_cycle = result.end_cycle;
        record.elapsed_cycles = result.elapsed_cycles;
        record.retired_instructions = result.retired_instructions;
        record.observed_retired_total = observation.retired_instructions;
        record.requested_units = result.requested_units;
        record.mailbox_state = observation.mailbox_state;
        record.mailbox_case = observation.mailbox_case_id;
        record.audio_frames = (uint32_t)result.audio_frames_written;
        record.audio_bytes = (uint32_t)result.audio_bytes_written;
        record.logical_mask = observation.logical_input_mask;
        record.applied_events = observation.applied_input_count;
        record.pending_events = observation.pending_input_count;
        append_call(trace, sizeof trace, &trace_used, &record);
        if (trace_used >= sizeof trace) {
            fputs("phase23: call trace capacity exceeded\n", stderr);
            goto cleanup;
        }
        timing_length = snprintf(host_times + host_used,
            sizeof host_times - host_used,
            "{\"call\":%zu,\"host_elapsed_ns\":%llu}\n", calls,
            (unsigned long long)host_elapsed_ns(host_start, host_end));
        if (timing_length <= 0 ||
            (size_t)timing_length >= sizeof host_times - host_used) {
            fputs("phase23: host timing trace capacity exceeded\n", stderr);
            goto cleanup;
        }
        host_used += (size_t)timing_length;
        calls += 1u;
        prior_cycle = result.end_cycle;
        if (observation.mailbox_state == GBL_V1_MAILBOX_FAIL) {
            fprintf(stderr,
                "phase23: guest-case-mismatch expected PASS case=0x23010001 "
                "observed FAIL case=0x%08x\n", observation.mailbox_case_id);
            goto cleanup;
        }
        if (mode.refusal_retry && calls == 1u) {
            call_record retry_record = { 0 };
            gbl_v1_observation retry_observation = { 0 };
            uint8_t retry_audio[AUDIO_CALL_CAPACITY];
            size_t retry_audio_bytes;
            int retry_timing_length;

            if (record.mailbox_state != GBL_V1_MAILBOX_ABSENT ||
                !run_refusal_retry(machine, rom_bytes, rom_length,
                    &record, call_audio, retry_audio, sizeof retry_audio,
                    &retry_record, &retry_observation, refusal_statuses,
                    &retry_host_ns, reference_audio_total,
                    sizeof reference_audio_total, &reference_audio_used,
                    reference_trace, sizeof reference_trace,
                    &reference_trace_used, reference_video,
                    sizeof reference_video, &reference_final_observation)) {
                fputs("phase23: atomic refusal/retry reference mismatch\n",
                      stderr);
                goto cleanup;
            }
            retry_audio_bytes = retry_record.audio_bytes;
            if (retry_audio_bytes > sizeof audio_total - audio_used) {
                fputs("phase23: retry audio extent exceeds capture bound\n", stderr);
                goto cleanup;
            }
            memcpy(audio_total + audio_used, retry_audio, retry_audio_bytes);
            audio_used += retry_audio_bytes;
            append_call(trace, sizeof trace, &trace_used, &retry_record);
            if (trace_used >= sizeof trace) {
                fputs("phase23: call trace capacity exceeded after retry\n", stderr);
                goto cleanup;
            }
            retry_timing_length = snprintf(host_times + host_used,
                sizeof host_times - host_used,
                "{\"call\":%zu,\"host_elapsed_ns\":%llu}\n", calls,
                (unsigned long long)retry_host_ns);
            if (retry_timing_length <= 0 ||
                (size_t)retry_timing_length >= sizeof host_times - host_used) {
                fputs("phase23: host timing trace capacity exceeded after retry\n",
                      stderr);
                goto cleanup;
            }
            host_used += (size_t)retry_timing_length;
            calls += 1u;
            prior_cycle = retry_record.end_cycle;
            observation = retry_observation;
            if (observation.applied_input_count > 0u &&
                observation.applied_input_count <= 3u &&
                input_checkpoint_calls[observation.applied_input_count - 1u] == SIZE_MAX) {
                input_checkpoint_calls[observation.applied_input_count - 1u] = calls - 1u;
            }
            if (observation.mailbox_state != GBL_V1_MAILBOX_ABSENT) {
                fputs("phase23: corrected retry crossed the guest terminal boundary\n",
                      stderr);
                goto cleanup;
            }
            refusal_checked = true;
        }
        if (observation.mailbox_state == GBL_V1_MAILBOX_PASS) {
            if (observation.mailbox_case_id != UINT32_C(0x23010001) ||
                result.outcome != GBL_V1_ADVANCE_COMPLETED ||
                result.stop_reason != GBL_V1_STOP_UNIT_LIMIT ||
                observation.video_state != GBL_V1_VIDEO_VALID ||
                observation.video_epoch != 0u ||
                prior_cycle < UINT64_C(197120) ||
                prior_cycle >= UINT64_C(280896) ||
                input_checkpoint_calls[0] == SIZE_MAX ||
                input_checkpoint_calls[1] == SIZE_MAX ||
                input_checkpoint_calls[2] == SIZE_MAX ||
                !(input_checkpoint_calls[0] < input_checkpoint_calls[1] &&
                  input_checkpoint_calls[1] < input_checkpoint_calls[2])) {
                fprintf(stderr,
                    "phase23: terminal witness mismatch state=%u case=%08x outcome=%u stop=%u video=%u epoch=%llu cycles=%llu input_calls=%zu,%zu,%zu\n",
                    observation.mailbox_state, observation.mailbox_case_id,
                    result.outcome, result.stop_reason, observation.video_state,
                    (unsigned long long)observation.video_epoch,
                    (unsigned long long)prior_cycle, input_checkpoint_calls[0],
                    input_checkpoint_calls[1], input_checkpoint_calls[2]);
                goto cleanup;
            }
            final_observation = observation;
            break;
        }
    }
    if (calls == 0u || calls >= CALL_LIMIT ||
        prior_cycle >= CYCLE_LIMIT || machine == NULL) {
        fputs("phase23: guest completion call/cycle cap reached\n", stderr);
        goto cleanup;
    }
    if (!check_audio(audio_total, audio_used)) {
        fputs("phase23: audio literal mismatch (expected zero then -9472/0)\n",
              stderr);
        goto cleanup;
    }
    {
        gbl_v1_video request = { 0 };

        request.struct_size = sizeof request;
        request.bytes = video;
        request.capacity = sizeof video;
        if (gbl_v1_read_video(machine, &request) != GBL_V1_OK ||
            !check_video(video, sizeof video, mode.wrong_video_literal,
                         &bad_offset)) {
            fprintf(stderr,
                "phase23: video-literal-mismatch row=%zu pixel=%zu offset=%zu\n",
                bad_offset / 480u, (bad_offset % 480u) / 2u, bad_offset);
            goto cleanup;
        }
    }
    if (mode.refusal_retry &&
        (!refusal_checked || !observations_equal(&final_observation,
                                                  &reference_final_observation) ||
         trace_used != reference_trace_used ||
         memcmp(trace, reference_trace, trace_used) != 0 ||
         audio_used != reference_audio_used ||
         memcmp(audio_total, reference_audio_total, audio_used) != 0 ||
         memcmp(video, reference_video, GBL_V1_VIDEO_BYTES) != 0)) {
        fputs("phase23: final refusal retry state/media differs from fresh reference\n",
              stderr);
        goto cleanup;
    }
    gbl_v1_destroy(machine);
    machine = NULL;
    if (allocator.live_count != 0u || allocator.invalid_release) {
        fputs("phase23: caller allocator release accounting mismatch\n", stderr);
        goto cleanup;
    }
    {
        char summary[1024];
        char refusal_summary[768];
        int summary_length = snprintf(summary, sizeof summary,
            "{\"api_status\":\"OK\",\"guest_mailbox\":\"PASS\","
            "\"case_id\":%u,\"start_cycle\":0,"
            "\"guest_end_cycle\":%llu,\"call_count\":%zu,"
            "\"requested_units_per_call\":%u,"
            "\"requested_budget_per_call\":%u,"
            "\"input_events\":[{\"name\":\"press-a\",\"cycle\":2464},"
            "{\"name\":\"switch-left\",\"cycle\":12320},"
            "{\"name\":\"release\",\"cycle\":24640}],"
            "\"input_observations\":[{\"call\":%zu,\"logical_mask\":1,\"applied_count\":1},"
            "{\"call\":%zu,\"logical_mask\":16,\"applied_count\":2},"
            "{\"call\":%zu,\"logical_mask\":0,\"applied_count\":3}],"
            "\"audio_frames\":%zu,\"audio_bytes\":%zu,"
            "\"audio_channel_order\":\"left,right\","
            "\"video_format\":\"BGR555LE\",\"video_epoch\":0,"
            "\"audio_format\":\"S16LE-pre-PWM\","
            "\"host_presentation\":\"outside-session-evidence\"}\n",
            UINT32_C(0x23010001), (unsigned long long)prior_cycle,
            calls, CALL_UNITS, CALL_BUDGET, input_checkpoint_calls[0],
            input_checkpoint_calls[1], input_checkpoint_calls[2],
            audio_used / 4u, audio_used);
        int refusal_summary_length = 0;

        if (mode.refusal_retry) {
            refusal_summary_length = snprintf(refusal_summary,
                sizeof refusal_summary,
                "{\"attempts\":[{\"status\":%u,\"units\":%u,\"budget\":512,\"result_unchanged\":true,\"audio_unchanged\":true,\"observation_unchanged\":true},{\"status\":%u,\"units\":%u,\"budget\":512,\"result_unchanged\":true,\"audio_unchanged\":true,\"observation_unchanged\":true}],\"corrected_budget\":%u,\"retry_matches_fresh_reference\":true}\n",
                refusal_statuses[0], CALL_UNITS, refusal_statuses[1],
                CALL_UNITS, CALL_BUDGET);
        }

        if (summary_length <= 0 || (size_t)summary_length >= sizeof summary ||
            !write_new_file(argv[2], "trace.jsonl", trace, trace_used) ||
            !write_new_file(argv[2], "host-timing.jsonl", host_times,
                            host_used) ||
            !write_new_file(argv[2], "video.bgr555le", video,
                            GBL_V1_VIDEO_BYTES) ||
            !write_new_file(argv[2], "audio.s16le", audio_total, audio_used) ||
            (mode.refusal_retry &&
             (!refusal_checked || refusal_summary_length <= 0 ||
              (size_t)refusal_summary_length >= sizeof refusal_summary ||
              !write_new_file(argv[2], "refusal.json", refusal_summary,
                              (size_t)refusal_summary_length))) ||
            !write_new_file(argv[2], "run.json", summary,
                            (size_t)summary_length)) {
            fputs("phase23: evidence write failed\n", stderr);
            goto cleanup;
        }
    }
    puts("phase23: installed session verified");
    exit_code = EXIT_SUCCESS;

cleanup:
    if (machine != NULL) {
        gbl_v1_destroy(machine);
    }
    if (rom_file != NULL) {
        (void)fclose(rom_file);
    }
    free(rom_bytes);
    if (exit_code == EXIT_SUCCESS && mode.refusal_retry) {
        puts("phase23: atomic refusal and retry verified");
    }
    return exit_code;
}
