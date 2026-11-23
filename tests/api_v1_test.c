#include "gamebloke/gamebloke_v1.h"

#include "check.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct api_block {
    void *pointer;
    size_t size;
    int active;
} api_block;

typedef struct api_allocator {
    size_t allocate_calls;
    size_t release_calls;
    size_t fail_call;
    api_block blocks[8];
} api_allocator;

static void *api_allocate(void *context, size_t size)
{
    api_allocator *allocator = context;
    size_t index;

    ++allocator->allocate_calls;
    if (allocator->fail_call == allocator->allocate_calls) {
        return NULL;
    }
    for (index = 0u; index < sizeof(allocator->blocks) /
                            sizeof(allocator->blocks[0]); ++index) {
        if (!allocator->blocks[index].active) {
            void *pointer = malloc(size);

            if (pointer == NULL) {
                return NULL;
            }
            allocator->blocks[index].pointer = pointer;
            allocator->blocks[index].size = size;
            allocator->blocks[index].active = 1;
            return pointer;
        }
    }
    return NULL;
}

static void api_release(void *context, void *pointer, size_t size)
{
    api_allocator *allocator = context;
    size_t index;

    ++allocator->release_calls;
    for (index = 0u; index < sizeof(allocator->blocks) /
                            sizeof(allocator->blocks[0]); ++index) {
        api_block *block = &allocator->blocks[index];

        if (block->active && block->pointer == pointer) {
            CHECK(block->size == size);
            block->active = 0;
            block->pointer = NULL;
            block->size = 0u;
            free(pointer);
            return;
        }
    }
    CHECK(0 && "release must match a live caller allocation");
}

static void fill_nops(uint8_t *rom, size_t length)
{
    size_t index;

    memset(rom, 0, length);
    for (index = 0u; index + 3u < length; index += 4u) {
        rom[index] = 0x00u;
        rom[index + 1u] = 0x00u;
        rom[index + 2u] = 0xA0u;
        rom[index + 3u] = 0xE1u;
    }
}

static gbl_v1_config make_config(api_allocator *allocator)
{
    gbl_v1_config config = { 0 };

    config.struct_size = sizeof(config);
    config.profile = GBL_V1_PROFILE_AGB;
    config.start_mode = GBL_V1_START_DIRECT;
    config.cartridge_mode = GBL_V1_CARTRIDGE_ROM_ONLY;
    config.allocator_context = allocator;
    config.allocate = api_allocate;
    config.release = api_release;
    return config;
}

static int run_smoke(void)
{
    api_allocator allocator = { 0 };
    gbl_v1_machine *machine = NULL;
    gbl_v1_config config = make_config(&allocator);
    uint8_t rom_bytes[0xC0u];
    gbl_v1_rom rom = { sizeof(rom), rom_bytes, sizeof(rom_bytes) };
    gbl_v1_advance_options options = { 0 };
    gbl_v1_advance_result result = { 0 };
    gbl_v1_observation observation = { 0 };
    gbl_v1_advance_result refused_before;
    size_t index;

    allocator.fail_call = 1u;
    CHECK(gbl_v1_create(&config, &machine) == GBL_V1_ALLOCATION_FAILED);
    CHECK(machine == NULL);
    CHECK(allocator.release_calls == 0u);
    allocator.fail_call = 0u;
    CHECK(gbl_v1_create(&config, &machine) == GBL_V1_OK);
    CHECK(machine != NULL);
    CHECK(allocator.allocate_calls == 3u);

    fill_nops(rom_bytes, sizeof(rom_bytes));
    allocator.fail_call = allocator.allocate_calls + 1u;
    CHECK(gbl_v1_load_rom(machine, &rom) == GBL_V1_ALLOCATION_FAILED);
    CHECK(gbl_v1_observe(machine, &(gbl_v1_observation){
              .struct_size = sizeof(gbl_v1_observation) }) == GBL_V1_OK);
    allocator.fail_call = 0u;
    CHECK(gbl_v1_load_rom(machine, &rom) == GBL_V1_OK);
    CHECK(allocator.allocate_calls == 5u);
    memset(rom_bytes, 0xFF, sizeof(rom_bytes));

    options.struct_size = sizeof(options);
    options.units = 8u;
    options.cycle_budget = 1u;
    result.struct_size = sizeof(result);
    memset(&result.requested_units, 0xA5,
           sizeof(result) - offsetof(gbl_v1_advance_result, requested_units));
    refused_before = result;
    CHECK(gbl_v1_advance(machine, &options, &result) ==
          GBL_V1_WORK_EXHAUSTED);
    CHECK(memcmp(&result, &refused_before, sizeof(result)) == 0);

    options.cycle_budget = 4096u;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    CHECK(gbl_v1_advance(machine, &options, &result) == GBL_V1_OK);
    CHECK(result.requested_units == options.units);
    CHECK(result.end_cycle > result.start_cycle);
    CHECK(result.elapsed_cycles == result.end_cycle - result.start_cycle);
    CHECK(result.retired_instructions > 0u);
    CHECK(result.stop_reason == GBL_V1_STOP_UNIT_LIMIT);
    observation.struct_size = sizeof(observation);
    CHECK(gbl_v1_observe(machine, &observation) == GBL_V1_OK);
    CHECK(observation.loaded == 1u);
    CHECK(observation.master_cycle == result.end_cycle);
    CHECK(observation.retired_instructions >= result.retired_instructions);
    CHECK(allocator.release_calls == 0u);

    gbl_v1_destroy(machine);
    CHECK(allocator.release_calls == 3u);
    for (index = 0u; index < sizeof(allocator.blocks) /
                            sizeof(allocator.blocks[0]); ++index) {
        CHECK(!allocator.blocks[index].active);
    }
    return CHECK_DONE();
}

typedef struct api_instance {
    api_allocator allocator;
    gbl_v1_machine *machine;
    uint8_t rom[0xC0u];
} api_instance;

static void put_word(uint8_t *bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1u] = (uint8_t)(value >> 8u);
    bytes[offset + 2u] = (uint8_t)(value >> 16u);
    bytes[offset + 3u] = (uint8_t)(value >> 24u);
}

/* Original synthetic ARM guest: read active-low KEYINPUT, test logical A,
 * then publish a five-word mailbox with case 0xC0DE and the marker last. */
static void make_input_mailbox_guest(uint8_t rom[0xC0u])
{
    static const uint32_t code[] = {
        UINT32_C(0xE59F0040), /* LDR r0,[pc,#0x40] => mailbox */
        UINT32_C(0xE59F1040), /* LDR r1,[pc,#0x40] => KEYINPUT */
        UINT32_C(0xE59F2040), /* LDR r2,[pc,#0x40] => magic */
        UINT32_C(0xE5802000), /* STR r2,[r0] */
        UINT32_C(0xE3A02001), /* MOV r2,#1 */
        UINT32_C(0xE5802004), /* STR r2,[r0,#4] */
        UINT32_C(0xE1D130B0), /* LDRH r3,[r1] */
        UINT32_C(0xE3130001), /* TST r3,#1 (A is pressed when bit clears) */
        UINT32_C(0x1A000001), /* BNE fail */
        UINT32_C(0xE3A02002), /* MOV r2,#2 (pass) */
        UINT32_C(0xEA000000), /* B state store */
        UINT32_C(0xE3A02003), /* fail: MOV r2,#3 */
        UINT32_C(0xE5802008), /* STR r2,[r0,#8] */
        UINT32_C(0xE59F2018), /* LDR r2,[pc,#0x18] => case */
        UINT32_C(0xE580200C), /* STR r2,[r0,#12] */
        UINT32_C(0xE59F2014), /* LDR r2,[pc,#0x14] => completion */
        UINT32_C(0xE5802010), /* STR r2,[r0,#16], marker last */
        UINT32_C(0xEAFFFFFE)  /* B . */
    };
    size_t index;

    memset(rom, 0, 0xC0u);
    for (index = 0u; index < sizeof(code) / sizeof(code[0]); ++index) {
        put_word(rom, index * 4u, code[index]);
    }
    put_word(rom, 0x48u, UINT32_C(0x03007FC0));
    put_word(rom, 0x4Cu, UINT32_C(0x04000130));
    put_word(rom, 0x50u, UINT32_C(0x47424C4B));
    put_word(rom, 0x54u, UINT32_C(0x0000C0DE));
    put_word(rom, 0x58u, UINT32_C(0x434F4D50));
}

static bool instance_init(api_instance *instance, int mailbox_guest)
{
    gbl_v1_config config;
    gbl_v1_rom rom;

    memset(instance, 0, sizeof(*instance));
    config = make_config(&instance->allocator);
    if (gbl_v1_create(&config, &instance->machine) != GBL_V1_OK) {
        return false;
    }
    if (mailbox_guest) {
        make_input_mailbox_guest(instance->rom);
    } else {
        fill_nops(instance->rom, sizeof(instance->rom));
    }
    rom = (gbl_v1_rom){ sizeof(rom), instance->rom, sizeof(instance->rom) };
    return gbl_v1_load_rom(instance->machine, &rom) == GBL_V1_OK;
}

static void instance_destroy(api_instance *instance)
{
    gbl_v1_destroy(instance->machine);
    instance->machine = NULL;
}

static void set_timeline(api_instance *instance,
                         const gbl_v1_input_event *events, size_t count)
{
    gbl_v1_input_timeline timeline = { sizeof(timeline), events, count };

    CHECK(gbl_v1_set_input(instance->machine, &timeline) == GBL_V1_OK);
}

static void observe_instance(api_instance *instance,
                             gbl_v1_observation *observation)
{
    memset(observation, 0, sizeof(*observation));
    observation->struct_size = sizeof(*observation);
    CHECK(gbl_v1_observe(instance->machine, observation) == GBL_V1_OK);
}

static gbl_v1_advance_result advance_instance(api_instance *instance,
                                              uint32_t units,
                                              uint64_t cycle_budget)
{
    gbl_v1_advance_options options = { 0 };
    gbl_v1_advance_result result = { 0 };

    options.struct_size = sizeof(options);
    options.units = units;
    options.cycle_budget = cycle_budget;
    result.struct_size = sizeof(result);
    CHECK(gbl_v1_advance(instance->machine, &options, &result) == GBL_V1_OK);
    return result;
}

static int run_lifecycle(void)
{
    api_instance first;
    api_instance second;
    api_instance first_reference;
    api_instance second_reference;
    gbl_v1_input_event first_events[2] = {
        { 0u, GBL_V1_KEY_A, 0u }, { 0u, GBL_V1_KEY_B, 0u }
    };
    gbl_v1_input_event second_events[2] = {
        { 0u, GBL_V1_KEY_RIGHT, 0u }, { 0u, GBL_V1_KEY_LEFT, 0u }
    };
    gbl_v1_input_event first_reference_events[2] = {
        { 0u, GBL_V1_KEY_A, 0u }, { 0u, GBL_V1_KEY_B, 0u }
    };
    gbl_v1_input_event second_reference_events[2] = {
        { 0u, GBL_V1_KEY_RIGHT, 0u }, { 0u, GBL_V1_KEY_LEFT, 0u }
    };
    gbl_v1_advance_result first_interleaved;
    gbl_v1_advance_result second_interleaved;
    gbl_v1_advance_result first_reference_result;
    gbl_v1_advance_result second_reference_result;
    gbl_v1_observation first_observation;
    gbl_v1_observation second_observation;
    gbl_v1_observation fresh_observation;
    size_t releases_before;

    CHECK(instance_init(&first, 0));
    CHECK(instance_init(&second, 0));
    CHECK(instance_init(&first_reference, 0));
    CHECK(instance_init(&second_reference, 0));
    set_timeline(&first, first_events, 2u);
    set_timeline(&second, second_events, 2u);
    set_timeline(&first_reference, first_reference_events, 2u);
    set_timeline(&second_reference, second_reference_events, 2u);
    first_events[0].pressed_mask = 0u;
    first_events[1].pressed_mask = 0u;
    second_events[0].pressed_mask = 0u;
    second_events[1].pressed_mask = 0u;

    first_interleaved = advance_instance(&first, 16u, 4096u);
    second_interleaved = advance_instance(&second, 16u, 4096u);
    first_interleaved = advance_instance(&first, 16u, 4096u);
    second_interleaved = advance_instance(&second, 16u, 4096u);
    (void)advance_instance(&first_reference, 16u, 4096u);
    first_reference_result =
        advance_instance(&first_reference, 16u, 4096u);
    (void)advance_instance(&second_reference, 16u, 4096u);
    second_reference_result =
        advance_instance(&second_reference, 16u, 4096u);
    first_observation = first_interleaved.observation;
    second_observation = second_interleaved.observation;
    CHECK(memcmp(&first_observation, &first_reference_result.observation,
                 sizeof(first_observation)) == 0);
    CHECK(memcmp(&second_observation, &second_reference_result.observation,
                 sizeof(second_observation)) == 0);
    CHECK(first_observation.logical_input_mask == GBL_V1_KEY_B);
    CHECK(second_observation.logical_input_mask == GBL_V1_KEY_LEFT);
    CHECK(first_observation.applied_input_count == 2u);
    CHECK(first_observation.pending_input_count == 0u);
    CHECK(first_observation.retired_instructions > 0u);
    CHECK(first_observation.master_cycle > 0u);
    CHECK(first_observation.master_cycle != second_observation.master_cycle ||
          first_observation.logical_input_mask !=
              second_observation.logical_input_mask);
    CHECK(first_observation.profile == GBL_V1_PROFILE_AGB);
    CHECK(first_observation.profile_version == 16u);
    CHECK(strcmp(first_observation.profile_id, "agb") == 0);
    CHECK(strcmp(first_observation.bios_id, "none") == 0);
    CHECK(strcmp(first_observation.cartridge_id, "rom-only") == 0);
    CHECK(first_observation.hardware_qualification ==
          GBL_V1_QUALIFICATION_UNKNOWN);

    releases_before = first.allocator.release_calls;
    CHECK(gbl_v1_reset(first.machine) == GBL_V1_OK);
    observe_instance(&first, &fresh_observation);
    CHECK(fresh_observation.loaded == 1u);
    CHECK(fresh_observation.master_cycle == 0u);
    CHECK(fresh_observation.retired_instructions == 0u);
    CHECK(fresh_observation.logical_input_mask == 0u);
    CHECK(fresh_observation.video_state != GBL_V1_VIDEO_VALID);
    CHECK(first.allocator.release_calls == releases_before);
    CHECK(advance_instance(&first, 8u, 4096u).retired_instructions > 0u);

    releases_before = first.allocator.release_calls;
    CHECK(gbl_v1_unload(first.machine) == GBL_V1_OK);
    CHECK(first.allocator.release_calls == releases_before + 1u);
    CHECK(gbl_v1_unload(first.machine) == GBL_V1_OK);
    CHECK(first.allocator.release_calls == releases_before + 1u);
    observe_instance(&first, &fresh_observation);
    CHECK(fresh_observation.loaded == 0u);
    CHECK(fresh_observation.master_cycle == 0u);
    CHECK(gbl_v1_reset(first.machine) == GBL_V1_INVALID_LIFECYCLE);
    {
        gbl_v1_rom rom = { sizeof(rom), first.rom, sizeof(first.rom) };

        fill_nops(first.rom, sizeof(first.rom));
        CHECK(gbl_v1_load_rom(first.machine, &rom) == GBL_V1_OK);
        CHECK(advance_instance(&first, 8u, 4096u).retired_instructions > 0u);
    }

    instance_destroy(&first);
    instance_destroy(&second);
    instance_destroy(&first_reference);
    instance_destroy(&second_reference);
    return CHECK_DONE();
}

static void check_zero_bytes(const uint8_t *bytes, size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        CHECK(bytes[index] == 0u);
    }
}

static int run_media(void)
{
    api_instance media;
    api_instance no_input;
    api_instance audio_reference;
    api_instance audio_retry;
    gbl_v1_input_event input = { 0u, GBL_V1_KEY_A, 0u };
    uint8_t audio[GBL_V1_MAX_AUDIO_BYTES];
    uint8_t reference_audio[GBL_V1_MAX_AUDIO_BYTES];
    uint8_t retry_audio[GBL_V1_MAX_AUDIO_BYTES];
    uint8_t video[GBL_V1_VIDEO_BYTES + 4u];
    uint8_t short_audio[GBL_V1_MAX_AUDIO_BYTES];
    gbl_v1_observation before_video;
    gbl_v1_observation after_reset;
    gbl_v1_observation before_refusal;
    gbl_v1_observation after_refusal;
    gbl_v1_observation no_input_observation;
    gbl_v1_advance_options options = { 0 };
    gbl_v1_advance_result result = { 0 };
    gbl_v1_advance_result no_input_result = { 0 };
    gbl_v1_advance_result reference_result = { 0 };
    gbl_v1_advance_result refused_result;
    gbl_v1_video video_request = { 0 };
    size_t index;

    CHECK(instance_init(&media, 1));
    CHECK(instance_init(&no_input, 1));
    set_timeline(&media, &input, 1u);
    memset(audio, 0xA5, sizeof(audio));
    memset(video, 0xA5, sizeof(video));
    video_request.struct_size = sizeof(video_request);
    video_request.bytes = video;
    video_request.capacity = sizeof(video);
    observe_instance(&media, &before_video);
    CHECK(before_video.loaded == 1u);
    CHECK(before_video.video_state != GBL_V1_VIDEO_VALID);
    CHECK(gbl_v1_read_video(media.machine, &video_request) ==
          GBL_V1_OUTPUT_UNAVAILABLE);
    CHECK(video[0] == 0xA5u && video[sizeof(video) - 1u] == 0xA5u);

    set_timeline(&no_input, NULL, 0u);
    no_input_result = advance_instance(&no_input, 64u, 100000u);
    observe_instance(&no_input, &no_input_observation);
    CHECK(no_input_result.observation.mailbox_state == GBL_V1_MAILBOX_FAIL);
    CHECK(no_input_result.observation.mailbox_case_id == UINT32_C(0xC0DE));
    CHECK(no_input_observation.logical_input_mask == 0u);

    options.struct_size = sizeof(options);
    options.units = 200000u;
    options.cycle_budget = GBL_V1_MAX_ADVANCE_CYCLES;
    options.capture_audio = 1u;
    options.audio_bytes = audio;
    options.audio_capacity = sizeof(audio);
    result.struct_size = sizeof(result);
    CHECK(gbl_v1_advance(media.machine, &options, &result) == GBL_V1_OK);
    CHECK(result.observation.mailbox_state == GBL_V1_MAILBOX_PASS);
    CHECK(result.observation.mailbox_case_id == UINT32_C(0xC0DE));
    CHECK(result.observation.logical_input_mask == GBL_V1_KEY_A);
    CHECK(result.observation.retired_instructions >=
          result.retired_instructions);
    CHECK(result.observation.profile_version == 16u);
    CHECK(result.observation.hardware_qualification ==
          GBL_V1_QUALIFICATION_UNKNOWN);
    CHECK((result.observation.capabilities &
           GBL_V1_CAP_RAW_AUDIO_CAPTURE) != 0u);
    CHECK(result.observation.raw_video_format == GBL_V1_VIDEO_BGR555_LE);
    CHECK(result.observation.raw_audio_format == GBL_V1_AUDIO_S16_LE);
    CHECK(result.observation.audio_sample_rate == GBL_V1_AUDIO_SAMPLE_RATE);
    CHECK(result.observation.audio_cycles_per_frame ==
          GBL_V1_AUDIO_CYCLES_PER_FRAME);
    CHECK(result.observation.model_limit_flags ==
          (GBL_V1_MODEL_LIMIT_TIMING_DISPUTES |
           GBL_V1_MODEL_LIMIT_UNMODELED_PREFETCH |
           GBL_V1_MODEL_LIMIT_PRE_PWM_AUDIO));
    CHECK(result.audio_available == 1u);
    CHECK(result.audio_bytes_written == result.audio_frames_written * 4u);
    CHECK(result.audio_frames_written ==
          result.end_cycle / GBL_V1_AUDIO_CYCLES_PER_FRAME);
    CHECK(result.audio_bytes_written > 0u);
    CHECK(audio[0] == 0u && audio[1] == 0u && audio[2] == 0u &&
          audio[3] == 0u);
    check_zero_bytes(audio, result.audio_bytes_written);
    for (index = result.audio_bytes_written; index < sizeof(audio); ++index) {
        CHECK(audio[index] == 0xA5u);
    }

    observe_instance(&media, &before_video);
    CHECK(before_video.video_state == GBL_V1_VIDEO_VALID);
    CHECK(before_video.video_epoch == result.observation.video_epoch);
    video_request.capacity = GBL_V1_VIDEO_BYTES - 1u;
    CHECK(gbl_v1_read_video(media.machine, &video_request) ==
          GBL_V1_INSUFFICIENT_CAPACITY);
    CHECK(video[0] == 0xA5u && video[GBL_V1_VIDEO_BYTES - 2u] == 0xA5u);
    video_request.capacity = sizeof(video);
    CHECK(gbl_v1_read_video(media.machine, &video_request) == GBL_V1_OK);
    CHECK(video[0] == 0u && video[1] == 0u);
    CHECK(video[GBL_V1_VIDEO_BYTES - 2u] == 0u);
    CHECK(video[GBL_V1_VIDEO_BYTES - 1u] == 0u);
    for (index = GBL_V1_VIDEO_BYTES; index < sizeof(video); ++index) {
        CHECK(video[index] == 0xA5u);
    }

    CHECK(gbl_v1_reset(media.machine) == GBL_V1_OK);
    observe_instance(&media, &after_reset);
    CHECK(after_reset.loaded == 1u);
    CHECK(after_reset.master_cycle == 0u);
    CHECK(after_reset.retired_instructions == 0u);
    CHECK(after_reset.video_state != GBL_V1_VIDEO_VALID);
    video_request.capacity = sizeof(video);
    memset(video, 0xA5, sizeof(video));
    CHECK(gbl_v1_read_video(media.machine, &video_request) ==
          GBL_V1_OUTPUT_UNAVAILABLE);
    CHECK(video[0] == 0xA5u && video[sizeof(video) - 1u] == 0xA5u);

    CHECK(instance_init(&audio_reference, 1));
    CHECK(instance_init(&audio_retry, 1));
    set_timeline(&audio_reference, &input, 1u);
    set_timeline(&audio_retry, &input, 1u);
    options.units = 4096u;
    options.cycle_budget = 1000000u;
    options.audio_bytes = reference_audio;
    options.audio_capacity = sizeof(reference_audio);
    memset(reference_audio, 0xA5, sizeof(reference_audio));
    result = (gbl_v1_advance_result){ 0 };
    result.struct_size = sizeof(result);
    CHECK(gbl_v1_advance(audio_reference.machine, &options, &result) ==
          GBL_V1_OK);
    CHECK(result.audio_bytes_written > 0u);
    reference_result = result;

    options.audio_bytes = short_audio;
    options.audio_capacity = result.audio_bytes_written - 1u;
    memset(short_audio, 0xA5, sizeof(short_audio));
    observe_instance(&audio_retry, &before_refusal);
    refused_result = (gbl_v1_advance_result){ 0 };
    refused_result.struct_size = sizeof(refused_result);
    memset(&refused_result.requested_units, 0x5A,
           sizeof(refused_result) -
               offsetof(gbl_v1_advance_result, requested_units));
    {
        gbl_v1_advance_result refused_before = refused_result;

        CHECK(gbl_v1_advance(audio_retry.machine, &options,
                             &refused_result) ==
              GBL_V1_INSUFFICIENT_CAPACITY);
        CHECK(memcmp(&refused_result, &refused_before,
                     sizeof(refused_result)) == 0);
    }
    observe_instance(&audio_retry, &after_refusal);
    CHECK(memcmp(&before_refusal, &after_refusal,
                 sizeof(before_refusal)) == 0);
    for (index = 0u; index < sizeof(short_audio); ++index) {
        CHECK(short_audio[index] == 0xA5u);
    }

    options.audio_bytes = retry_audio;
    options.audio_capacity = result.audio_bytes_written;
    memset(retry_audio, 0xA5, sizeof(retry_audio));
    result = (gbl_v1_advance_result){ 0 };
    result.struct_size = sizeof(result);
    CHECK(gbl_v1_advance(audio_retry.machine, &options, &result) ==
          GBL_V1_OK);
    CHECK(result.audio_bytes_written == reference_result.audio_bytes_written);
    observe_instance(&audio_retry, &after_refusal);
    observe_instance(&audio_reference, &before_refusal);
    CHECK(memcmp(&after_refusal, &before_refusal,
                 sizeof(after_refusal)) == 0);
    CHECK(memcmp(retry_audio, reference_audio,
                 result.audio_bytes_written) == 0);
    if (result.audio_bytes_written < sizeof(retry_audio)) {
        CHECK(retry_audio[result.audio_bytes_written] == 0xA5u);
    }

    instance_destroy(&media);
    instance_destroy(&no_input);
    instance_destroy(&audio_reference);
    instance_destroy(&audio_retry);
    return CHECK_DONE();
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: api_v1_test smoke|lifecycle|media\n");
        return 2;
    }
    if (strcmp(argv[1], "smoke") == 0) {
        return run_smoke();
    }
    if (strcmp(argv[1], "lifecycle") == 0) {
        return run_lifecycle();
    }
    if (strcmp(argv[1], "media") == 0) {
        return run_media();
    }
    fprintf(stderr, "api_v1_test: unknown selector: %s\n", argv[1]);
    return 2;
}
