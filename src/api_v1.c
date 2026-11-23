#include "gamebloke/gamebloke_v1.h"

#include "gamebloke/gamebloke.h"
#include "audio_internal.h"

#include <stdbool.h>
#include <stdint.h>

#define GBL_V1_MAGIC UINT64_C(0x47424C4150495631)
#define GBL_V1_AUDIO_SCRATCH_BYTES GBL_AUDIO_MAX_CAPTURE_BYTES
#define GBL_V1_CORE_ALLOCATION_CAPACITY 8u
#define GBL_V1_PROTECTED_SPAN_CAPACITY 4u
#ifndef GBL_AGB_PROFILE_VERSION
#define GBL_AGB_PROFILE_VERSION 1
#endif

struct gbl_v1_machine {
    uint64_t magic;
    void *allocator_context;
    gbl_v1_allocate_fn allocate;
    gbl_v1_release_fn release;
    void *scratch_allocation;
    size_t scratch_size;
    gbl_machine *machine;
    gbl_machine *candidate;
    uint8_t *audio_capture;
    uint8_t *audio_staging;
    size_t handle_size;
    struct {
        void *pointer;
        size_t size;
        bool active;
    } core_blocks[GBL_V1_CORE_ALLOCATION_CAPACITY];
    struct {
        uintptr_t first;
        uintptr_t last;
    } protected_spans[GBL_V1_PROTECTED_SPAN_CAPACITY];
    size_t protected_span_count;
};

typedef struct gbl_v1_range {
    uintptr_t first;
    uintptr_t last;
} gbl_v1_range;

static void gbl_v1_copy(void *destination, const void *source, size_t length)
{
    uint8_t *to = destination;
    const uint8_t *from = source;
    size_t index;

    for (index = 0u; index < length; ++index) {
        to[index] = from[index];
    }
}

static bool gbl_v1_range_make(const void *pointer, size_t length,
                              gbl_v1_range *range)
{
    uintptr_t first;

    if (range == NULL || (pointer == NULL && length != 0u)) {
        return false;
    }
    first = (uintptr_t)pointer;
    if ((uintptr_t)length > UINTPTR_MAX - first) {
        return false;
    }
    range->first = first;
    range->last = first + (uintptr_t)length;
    return true;
}

static bool gbl_v1_ranges_overlap(gbl_v1_range left, gbl_v1_range right)
{
    return left.first < right.last && right.first < left.last;
}

static bool gbl_v1_add_overlaps(const void *pointer, size_t length,
                                const void *other, size_t other_length)
{
    gbl_v1_range left;
    gbl_v1_range right;

    if (!gbl_v1_range_make(pointer, length, &left) ||
        !gbl_v1_range_make(other, other_length, &right)) {
        return true;
    }
    return gbl_v1_ranges_overlap(left, right);
}

static bool gbl_v1_is_aligned(const void *pointer, size_t alignment)
{
    return pointer != NULL && alignment != 0u &&
           (uintptr_t)pointer % (uintptr_t)alignment == 0u;
}

static bool gbl_v1_live(const gbl_v1_machine *machine)
{
    return machine != NULL && machine->magic == GBL_V1_MAGIC &&
           machine->machine != NULL && machine->machine->initialized;
}

static bool gbl_v1_span_disjoint(const gbl_v1_machine *machine,
                                const void *pointer, size_t length)
{
    const gbl_machine *core;

    if (!gbl_v1_live(machine) || (pointer == NULL && length != 0u)) {
        return false;
    }
    core = machine->machine;
    if (gbl_v1_add_overlaps(pointer, length, machine,
                            machine->handle_size) ||
        gbl_v1_add_overlaps(pointer, length, machine->scratch_allocation,
                            machine->scratch_size)) {
        return false;
    }
    if (core->rom != NULL && core->rom_length != 0u &&
        gbl_v1_add_overlaps(pointer, length, core->rom, core->rom_length)) {
        return false;
    }
    return true;
}

static gbl_v1_status gbl_v1_map_load(gbl_load_result result)
{
    if (result.status == GBL_LOAD_STATUS_OK) {
        return GBL_V1_OK;
    }
    if (result.status == GBL_LOAD_STATUS_INVALID_ARGUMENT) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (result.status == GBL_LOAD_STATUS_RESOURCE_FAILURE) {
        return GBL_V1_ALLOCATION_FAILED;
    }
    return GBL_V1_CONTENT_REFUSED;
}

static size_t gbl_v1_output_size(size_t declared, size_t known)
{
    return declared < known ? declared : known;
}

static bool gbl_v1_protect(gbl_v1_machine *machine, const void *pointer,
                           size_t length)
{
    gbl_v1_range span;
    size_t index;

    if (machine->protected_span_count >=
            GBL_V1_PROTECTED_SPAN_CAPACITY ||
        !gbl_v1_range_make(pointer, length, &span)) {
        return false;
    }
    index = machine->protected_span_count++;
    machine->protected_spans[index].first = span.first;
    machine->protected_spans[index].last = span.last;
    return true;
}

static void *gbl_v1_core_allocate(void *context, size_t size)
{
    gbl_v1_machine *machine = context;
    gbl_v1_range requested;
    gbl_v1_range handle_range;
    gbl_v1_range scratch_range;
    size_t free_slot = GBL_V1_CORE_ALLOCATION_CAPACITY;
    size_t index;
    void *pointer;
    bool overlaps_known = false;

    if (!gbl_v1_live(machine) || size == 0u ||
        !gbl_v1_range_make(machine, machine->handle_size, &handle_range) ||
        !gbl_v1_range_make(machine->scratch_allocation, machine->scratch_size,
                           &scratch_range)) {
        return NULL;
    }
    for (index = 0u; index < GBL_V1_CORE_ALLOCATION_CAPACITY; ++index) {
        if (!machine->core_blocks[index].active &&
            free_slot == GBL_V1_CORE_ALLOCATION_CAPACITY) {
            free_slot = index;
        }
    }
    if (free_slot == GBL_V1_CORE_ALLOCATION_CAPACITY) {
        return NULL;
    }
    pointer = machine->allocate(machine->allocator_context, size);
    if (pointer == NULL) {
        return NULL;
    }
    if (!gbl_v1_range_make(pointer, size, &requested)) {
        machine->release(machine->allocator_context, pointer, size);
        return NULL;
    }
    if (gbl_v1_ranges_overlap(requested, handle_range) ||
        gbl_v1_ranges_overlap(requested, scratch_range)) {
        return NULL;
    }
    for (index = 0u; index < GBL_V1_CORE_ALLOCATION_CAPACITY; ++index) {
        const size_t block_size = machine->core_blocks[index].size;
        gbl_v1_range block_range;

        if (machine->core_blocks[index].active &&
            gbl_v1_range_make(machine->core_blocks[index].pointer, block_size,
                              &block_range) &&
            gbl_v1_ranges_overlap(requested, block_range)) {
            overlaps_known = true;
            break;
        }
    }
    for (index = 0u; index < machine->protected_span_count; ++index) {
        const gbl_v1_range protected = {
            machine->protected_spans[index].first,
            machine->protected_spans[index].last
        };

        if (gbl_v1_ranges_overlap(requested, protected)) {
            overlaps_known = true;
            break;
        }
    }
    if (overlaps_known) {
        return NULL;
    }
    if (!gbl_v1_is_aligned(pointer, _Alignof(max_align_t))) {
        machine->release(machine->allocator_context, pointer, size);
        return NULL;
    }
    machine->core_blocks[free_slot].pointer = pointer;
    machine->core_blocks[free_slot].size = size;
    machine->core_blocks[free_slot].active = true;
    return pointer;
}

static void gbl_v1_core_release(void *context, void *pointer, size_t size)
{
    gbl_v1_machine *machine = context;
    size_t index;

    if (machine == NULL || pointer == NULL) {
        return;
    }
    for (index = 0u; index < GBL_V1_CORE_ALLOCATION_CAPACITY; ++index) {
        if (machine->core_blocks[index].active &&
            machine->core_blocks[index].pointer == pointer &&
            machine->core_blocks[index].size == size) {
            machine->core_blocks[index].active = false;
            machine->core_blocks[index].pointer = NULL;
            machine->core_blocks[index].size = 0u;
            machine->release(machine->allocator_context, pointer, size);
            return;
        }
    }
}

static bool gbl_v1_field_available(size_t size, size_t offset,
                                   size_t field_size)
{
    return offset <= size && field_size <= size - offset;
}

static void gbl_v1_observation_from_core(const gbl_machine *core,
                                         gbl_v1_observation *observation)
{
    const gbl_mailbox_result mailbox = gbl_mailbox_read(core);
    size_t index;

    *observation = (gbl_v1_observation){ 0 };
    observation->struct_size = sizeof(*observation);
    observation->loaded = core->rom != NULL ? 1u : 0u;
    observation->profile = GBL_V1_PROFILE_AGB;
    observation->profile_version = GBL_AGB_PROFILE_VERSION;
    observation->bios_mode = 0u;
    observation->cartridge_mode = GBL_V1_CARTRIDGE_ROM_ONLY;
    switch (core->ppu.capture_state) {
    case GBL_VIDEO_CAPTURE_INVALID:
        observation->video_state = GBL_V1_VIDEO_INVALID;
        break;
    case GBL_VIDEO_CAPTURE_INCOMPLETE:
        observation->video_state = GBL_V1_VIDEO_INCOMPLETE;
        break;
    case GBL_VIDEO_CAPTURE_VALID:
        observation->video_state = GBL_V1_VIDEO_VALID;
        break;
    default:
        observation->video_state = GBL_V1_VIDEO_INVALID;
        break;
    }
    switch (mailbox.status) {
    case GBL_MAILBOX_ABSENT:
        observation->mailbox_state = GBL_V1_MAILBOX_ABSENT;
        break;
    case GBL_MAILBOX_PENDING:
        observation->mailbox_state = GBL_V1_MAILBOX_PENDING;
        break;
    case GBL_MAILBOX_PASS:
        observation->mailbox_state = GBL_V1_MAILBOX_PASS;
        break;
    case GBL_MAILBOX_FAIL:
        observation->mailbox_state = GBL_V1_MAILBOX_FAIL;
        break;
    case GBL_MAILBOX_MALFORMED:
        observation->mailbox_state = GBL_V1_MAILBOX_MALFORMED;
        break;
    default:
        observation->mailbox_state = GBL_V1_MAILBOX_MALFORMED;
        break;
    }
    observation->hardware_qualification = GBL_V1_QUALIFICATION_UNKNOWN;
    observation->raw_video_format = GBL_V1_VIDEO_BGR555_LE;
    observation->raw_audio_format = GBL_V1_AUDIO_S16_LE;
    observation->audio_sample_rate = GBL_V1_AUDIO_SAMPLE_RATE;
    observation->audio_cycles_per_frame = GBL_V1_AUDIO_CYCLES_PER_FRAME;
    observation->audio_channels = GBL_V1_AUDIO_CHANNELS;
    observation->logical_input_mask = core->pressed_keys;
    observation->applied_input_count = (uint32_t)core->input_event_cursor;
    observation->pending_input_count =
        (uint32_t)(core->input_event_count - core->input_event_cursor);
    observation->mailbox_case_id = mailbox.case_id;
    observation->maximum_input_events = GBL_V1_MAX_INPUT_EVENTS;
    observation->maximum_rom_bytes = GBL_V1_MAX_ROM_BYTES;
    observation->video_width = GBL_V1_VIDEO_WIDTH;
    observation->video_height = GBL_V1_VIDEO_HEIGHT;
    observation->maximum_audio_bytes = GBL_V1_MAX_AUDIO_BYTES;
    observation->model_limit_flags =
        GBL_V1_MODEL_LIMIT_TIMING_DISPUTES |
        GBL_V1_MODEL_LIMIT_UNMODELED_PREFETCH |
        GBL_V1_MODEL_LIMIT_PRE_PWM_AUDIO;
    observation->capabilities = GBL_V1_CAP_TIMED_LOGICAL_INPUT |
                                GBL_V1_CAP_MAILBOX_OBSERVATION |
                                GBL_V1_CAP_RAW_VIDEO_CAPTURE |
                                GBL_V1_CAP_RAW_AUDIO_CAPTURE;
    observation->input_generation = core->input_generation;
    observation->master_cycle = core->master_cycles;
    observation->retired_instructions = core->cpu.retired_instructions;
    observation->video_epoch = core->ppu.frame_epoch;
    observation->maximum_advance_cycles = GBL_V1_MAX_ADVANCE_CYCLES;
    for (index = 0u; index < 4u; ++index) {
        static const char profile_id[4] = { 'a', 'g', 'b', '\0' };

        observation->profile_id[index] = profile_id[index];
    }
    observation->bios_id[0] = 'n';
    observation->bios_id[1] = 'o';
    observation->bios_id[2] = 'n';
    observation->bios_id[3] = 'e';
    observation->cartridge_id[0] = 'r';
    observation->cartridge_id[1] = 'o';
    observation->cartridge_id[2] = 'm';
    observation->cartridge_id[3] = '-';
    observation->cartridge_id[4] = 'o';
    observation->cartridge_id[5] = 'n';
    observation->cartridge_id[6] = 'l';
    observation->cartridge_id[7] = 'y';
}

gbl_v1_status gbl_v1_create(const gbl_v1_config *config,
                            gbl_v1_machine **out_machine)
{
    const size_t minimum_config = sizeof(gbl_v1_config);
    const size_t handle_size = sizeof(gbl_v1_machine);
    const size_t machine_size = sizeof(gbl_machine);
    const size_t audio_size = (size_t)GBL_V1_AUDIO_SCRATCH_BYTES;
    size_t scratch_size;
    gbl_v1_config supplied;
    gbl_allocator core_allocator;
    gbl_v1_machine *created;
    uint8_t *scratch;
    gbl_v1_range config_range;
    gbl_v1_range output_range;
    gbl_v1_range handle_range;
    gbl_v1_range scratch_range;
    size_t offset;

    if (config == NULL || out_machine == NULL ||
        !gbl_v1_is_aligned(config, _Alignof(gbl_v1_config)) ||
        !gbl_v1_is_aligned(out_machine, _Alignof(gbl_v1_machine *)) ||
        config->struct_size < minimum_config ||
        !gbl_v1_range_make(config, config->struct_size, &config_range) ||
        !gbl_v1_range_make(out_machine, sizeof(*out_machine),
                           &output_range) ||
        gbl_v1_add_overlaps(config, config->struct_size, out_machine,
                            sizeof(*out_machine))) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (config->profile != GBL_V1_PROFILE_AGB ||
        config->start_mode != GBL_V1_START_DIRECT ||
        config->cartridge_mode != GBL_V1_CARTRIDGE_ROM_ONLY ||
        config->reserved != 0u) {
        return GBL_V1_UNSUPPORTED;
    }
    if ((config->allocate == NULL) != (config->release == NULL) ||
        config->allocate == NULL) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (machine_size > (SIZE_MAX - 2u * audio_size) / 2u) {
        return GBL_V1_INTERNAL_REFUSAL;
    }
    scratch_size = 2u * machine_size + 2u * audio_size;
    supplied = (gbl_v1_config){ 0 };
    gbl_v1_copy(&supplied, config,
                gbl_v1_output_size(config->struct_size, sizeof(supplied)));

    created = supplied.allocate(supplied.allocator_context, handle_size);
    if (created == NULL) {
        return GBL_V1_ALLOCATION_FAILED;
    }
    if (!gbl_v1_is_aligned(created, _Alignof(gbl_v1_machine)) ||
        !gbl_v1_range_make(created, handle_size, &handle_range)) {
        supplied.release(supplied.allocator_context, created, handle_size);
        return GBL_V1_ALLOCATION_FAILED;
    }
    if (gbl_v1_ranges_overlap(handle_range, config_range) ||
        gbl_v1_ranges_overlap(handle_range, output_range)) {
        supplied.release(supplied.allocator_context, created, handle_size);
        return GBL_V1_ALLOCATION_FAILED;
    }
    scratch = supplied.allocate(supplied.allocator_context, scratch_size);
    if (scratch == NULL) {
        supplied.release(supplied.allocator_context, created, handle_size);
        return GBL_V1_ALLOCATION_FAILED;
    }
    if (!gbl_v1_is_aligned(scratch, _Alignof(gbl_machine)) ||
        !gbl_v1_range_make(scratch, scratch_size, &scratch_range)) {
        if (scratch == (uint8_t *)created) {
            supplied.release(supplied.allocator_context, scratch,
                             scratch_size);
        } else {
            supplied.release(supplied.allocator_context, scratch,
                             scratch_size);
            supplied.release(supplied.allocator_context, created,
                             handle_size);
        }
        return GBL_V1_ALLOCATION_FAILED;
    }
    if (gbl_v1_ranges_overlap(scratch_range, config_range) ||
        gbl_v1_ranges_overlap(scratch_range, output_range) ||
        gbl_v1_ranges_overlap(scratch_range, handle_range)) {
        if (scratch == (uint8_t *)created) {
            supplied.release(supplied.allocator_context, scratch,
                             scratch_size);
        } else {
            supplied.release(supplied.allocator_context, scratch,
                             scratch_size);
            supplied.release(supplied.allocator_context, created,
                             handle_size);
        }
        return GBL_V1_ALLOCATION_FAILED;
    }

    *created = (gbl_v1_machine){ 0 };
    created->allocator_context = supplied.allocator_context;
    created->allocate = supplied.allocate;
    created->release = supplied.release;
    created->scratch_allocation = scratch;
    created->scratch_size = scratch_size;
    created->handle_size = handle_size;
    created->machine = (gbl_machine *)scratch;
    offset = machine_size;
    created->candidate = (gbl_machine *)(scratch + offset);
    offset += machine_size;
    created->audio_capture = scratch + offset;
    offset += audio_size;
    created->audio_staging = scratch + offset;
    created->magic = GBL_V1_MAGIC;
    core_allocator.context = created;
    core_allocator.allocate = gbl_v1_core_allocate;
    core_allocator.release = gbl_v1_core_release;
    if (gbl_machine_init(created->machine, &core_allocator) !=
        GBL_BUS_STATUS_OK) {
        created->magic = 0u;
        created->release(created->allocator_context, scratch, scratch_size);
        created->release(created->allocator_context, created, handle_size);
        return GBL_V1_INTERNAL_REFUSAL;
    }
    *out_machine = created;
    return GBL_V1_OK;
}

void gbl_v1_destroy(gbl_v1_machine *machine)
{
    gbl_v1_release_fn release;
    void *context;
    void *scratch;
    size_t scratch_size;
    size_t handle_size;

    if (machine == NULL || machine->magic != GBL_V1_MAGIC) {
        return;
    }
    release = machine->release;
    context = machine->allocator_context;
    scratch = machine->scratch_allocation;
    scratch_size = machine->scratch_size;
    handle_size = machine->handle_size;
    gbl_machine_dispose(machine->machine);
    machine->magic = 0u;
    release(context, scratch, scratch_size);
    release(context, machine, handle_size);
}

gbl_v1_status gbl_v1_load_rom(gbl_v1_machine *machine,
                              const gbl_v1_rom *rom)
{
    gbl_load_result loaded;

    if (!gbl_v1_live(machine) || rom == NULL ||
        !gbl_v1_is_aligned(rom, _Alignof(gbl_v1_rom)) ||
        rom->struct_size < sizeof(*rom) ||
        !gbl_v1_span_disjoint(machine, rom, rom->struct_size) ||
        (rom->length != 0u && rom->bytes == NULL)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (rom->length > GBL_V1_MAX_ROM_BYTES) {
        return GBL_V1_CONTENT_REFUSED;
    }
    if (!gbl_v1_span_disjoint(machine, rom->bytes, rom->length)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (gbl_v1_add_overlaps(rom, rom->struct_size, rom->bytes,
                            rom->length)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    machine->protected_span_count = 0u;
    if (!gbl_v1_protect(machine, rom, rom->struct_size) ||
        !gbl_v1_protect(machine, rom->bytes, rom->length)) {
        machine->protected_span_count = 0u;
        return GBL_V1_INVALID_ARGUMENT;
    }
    loaded = gbl_load_rom(machine->machine, rom->bytes, rom->length);
    machine->protected_span_count = 0u;
    return gbl_v1_map_load(loaded);
}

gbl_v1_status gbl_v1_reset(gbl_v1_machine *machine)
{
    if (!gbl_v1_live(machine)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (machine->machine->rom == NULL) {
        return GBL_V1_INVALID_LIFECYCLE;
    }
    gbl_machine_reset(machine->machine);
    return GBL_V1_OK;
}

gbl_v1_status gbl_v1_unload(gbl_v1_machine *machine)
{
    gbl_allocator allocator;

    if (!gbl_v1_live(machine)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (machine->machine->rom == NULL) {
        return GBL_V1_OK;
    }
    allocator = machine->machine->allocator;
    gbl_machine_dispose(machine->machine);
    if (gbl_machine_init(machine->machine, &allocator) != GBL_BUS_STATUS_OK) {
        return GBL_V1_INTERNAL_REFUSAL;
    }
    return GBL_V1_OK;
}

static gbl_v1_status gbl_v1_map_input_status(gbl_time_status status)
{
    switch (status) {
    case GBL_TIME_STATUS_OK:
        return GBL_V1_OK;
    case GBL_TIME_STATUS_INPUT_CAPACITY:
        return GBL_V1_UNSUPPORTED;
    case GBL_TIME_STATUS_INVALID_ARGUMENT:
    case GBL_TIME_STATUS_BACKWARD:
    case GBL_TIME_STATUS_INPUT_OVERLAP:
    case GBL_TIME_STATUS_INPUT_MASK:
        return GBL_V1_INVALID_ARGUMENT;
    default:
        return GBL_V1_INTERNAL_REFUSAL;
    }
}

gbl_v1_status gbl_v1_set_input(gbl_v1_machine *machine,
                               const gbl_v1_input_timeline *timeline)
{
    const size_t event_size = sizeof(gbl_v1_input_event);
    const gbl_v1_input_event *events;
    gbl_input_event translated[GBL_V1_MAX_INPUT_EVENTS];
    size_t event_bytes;
    size_t index;
    gbl_time_status status;

    if (!gbl_v1_live(machine) || timeline == NULL ||
        !gbl_v1_is_aligned(timeline, _Alignof(gbl_v1_input_timeline)) ||
        timeline->struct_size < sizeof(*timeline) ||
        !gbl_v1_span_disjoint(machine, timeline, timeline->struct_size)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (machine->machine->rom == NULL) {
        return GBL_V1_INVALID_LIFECYCLE;
    }
    if (timeline->count > GBL_V1_MAX_INPUT_EVENTS) {
        return GBL_V1_UNSUPPORTED;
    }
    if (timeline->count > SIZE_MAX / event_size) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    event_bytes = timeline->count * event_size;
    events = timeline->events;
    if ((timeline->count != 0u && events == NULL) ||
        (timeline->count != 0u &&
         (!gbl_v1_is_aligned(events, _Alignof(gbl_v1_input_event)) ||
          !gbl_v1_span_disjoint(machine, events, event_bytes) ||
          gbl_v1_add_overlaps(timeline, timeline->struct_size, events,
                              event_bytes)))) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    for (index = 0u; index < timeline->count; ++index) {
        if (events[index].reserved != 0u ||
            (events[index].pressed_mask &
             (uint16_t)~GBL_V1_INPUT_MASK) != 0u ||
            events[index].cycle < machine->machine->master_cycles ||
            (index != 0u &&
             events[index].cycle < events[index - 1u].cycle)) {
            return GBL_V1_INVALID_ARGUMENT;
        }
        translated[index].cycle = events[index].cycle;
        translated[index].pressed_mask = events[index].pressed_mask;
    }
    status = gbl_machine_set_input_timeline(
        machine->machine, timeline->count == 0u ? NULL : translated,
        timeline->count);
    return gbl_v1_map_input_status(status);
}

gbl_v1_status gbl_v1_advance(gbl_v1_machine *machine,
                             const gbl_v1_advance_options *options,
                             gbl_v1_advance_result *result)
{
    const size_t minimum_options =
        offsetof(gbl_v1_advance_options, cycle_budget) +
        sizeof(options->cycle_budget);
    const size_t minimum_result =
        offsetof(gbl_v1_advance_result, retired_instructions) +
        sizeof(result->retired_instructions);
    gbl_v1_advance_options request;
    gbl_run_options core_options;
    gbl_run_result core_result = { 0 };
    gbl_v1_advance_result published = { 0 };
    gbl_audio_capture capture = { 0 };
    bool capture_audio = false;
    uint64_t start_cycle;
    uint64_t end_cycle;
    size_t result_size;

    if (!gbl_v1_live(machine) || options == NULL || result == NULL ||
        !gbl_v1_is_aligned(options, _Alignof(gbl_v1_advance_options)) ||
        !gbl_v1_is_aligned(result, _Alignof(gbl_v1_advance_result)) ||
        options->struct_size < minimum_options ||
        result->struct_size < minimum_result ||
        !gbl_v1_span_disjoint(machine, options, options->struct_size) ||
        !gbl_v1_span_disjoint(machine, result, result->struct_size) ||
        gbl_v1_add_overlaps(options, options->struct_size, result,
                            result->struct_size)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (machine->machine->rom == NULL) {
        return GBL_V1_INVALID_LIFECYCLE;
    }
    request = (gbl_v1_advance_options){ 0 };
    gbl_v1_copy(&request, options,
                gbl_v1_output_size(options->struct_size, sizeof(request)));
    if (request.units == 0u || request.cycle_budget == 0u) {
        return GBL_V1_WORK_EXHAUSTED;
    }
    if (request.cycle_budget > GBL_V1_MAX_ADVANCE_CYCLES) {
        return GBL_V1_UNSUPPORTED;
    }
    if (gbl_v1_field_available(request.struct_size,
                               offsetof(gbl_v1_advance_options,
                                        capture_audio),
                               sizeof(request.capture_audio))) {
        if (request.capture_audio > 1u) {
            return GBL_V1_INVALID_ARGUMENT;
        }
        capture_audio = request.capture_audio != 0u;
    }
    if (capture_audio) {
        const size_t fields_size =
            offsetof(gbl_v1_advance_options, audio_capacity) +
            sizeof(request.audio_capacity) -
            offsetof(gbl_v1_advance_options, audio_bytes);

        if (!gbl_v1_field_available(request.struct_size,
                                    offsetof(gbl_v1_advance_options,
                                             audio_bytes),
                                    fields_size) ||
            request.audio_capacity > GBL_V1_MAX_AUDIO_BYTES ||
            !gbl_v1_span_disjoint(machine, request.audio_bytes,
                                  request.audio_capacity) ||
            gbl_v1_add_overlaps(options, options->struct_size,
                                request.audio_bytes,
                                request.audio_capacity) ||
            gbl_v1_add_overlaps(result, result->struct_size,
                                request.audio_bytes,
                                request.audio_capacity)) {
            return GBL_V1_INVALID_ARGUMENT;
        }
    } else if (gbl_v1_field_available(
                   request.struct_size,
                   offsetof(gbl_v1_advance_options, audio_capacity),
                   sizeof(request.audio_capacity)) &&
               (request.audio_bytes != NULL || request.audio_capacity != 0u)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    start_cycle = machine->machine->master_cycles;
    if (request.cycle_budget > UINT64_MAX - start_cycle) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    end_cycle = start_cycle + request.cycle_budget;

    /* The candidate owns its complete mutable machine state and borrows only
     * the mounted immutable ROM. No loader/disposer runs on this copy. */
    gbl_v1_copy(machine->candidate, machine->machine, sizeof(gbl_machine));
    core_options = (gbl_run_options){ request.cycle_budget, request.units, 0u,
                                      NULL };
    {
        gbl_run_status run_status;

        machine->protected_span_count = 0u;
        if (!gbl_v1_protect(machine, options, options->struct_size) ||
            !gbl_v1_protect(machine, result, result->struct_size) ||
            (capture_audio &&
             !gbl_v1_protect(machine, request.audio_bytes,
                             request.audio_capacity))) {
            machine->protected_span_count = 0u;
            return GBL_V1_INVALID_ARGUMENT;
        }
        if (capture_audio) {
            if (gbl_audio_capture_begin(machine->candidate, &capture,
                                        machine->audio_capture,
                                        GBL_V1_MAX_AUDIO_BYTES) !=
                GBL_TIME_STATUS_OK) {
                machine->protected_span_count = 0u;
                return GBL_V1_INTERNAL_REFUSAL;
            }
            run_status = gbl_cpu_run_audio(
                machine->candidate, &core_options, &core_result, &capture,
                machine->audio_staging, GBL_V1_MAX_AUDIO_BYTES, end_cycle);
        } else {
            run_status = gbl_cpu_run(machine->candidate, &core_options,
                                     &core_result);
        }
        machine->protected_span_count = 0u;
        if (run_status != GBL_RUN_STATUS_OK) {
            return GBL_V1_INTERNAL_REFUSAL;
        }
    }
    if (machine->candidate->master_cycles < start_cycle ||
        machine->candidate->master_cycles > end_cycle) {
        return GBL_V1_INTERNAL_REFUSAL;
    }
    if (core_result.reason == GBL_RUN_REASON_CYCLE_BUDGET ||
        core_result.termination == GBL_RUN_TIMEOUT) {
        return GBL_V1_WORK_EXHAUSTED;
    }
    if (core_result.reason != GBL_RUN_REASON_UNIT_LIMIT &&
        core_result.reason != GBL_RUN_REASON_GUEST_STOP &&
        core_result.reason != GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION) {
        return GBL_V1_INTERNAL_REFUSAL;
    }
    if (capture_audio &&
        (capture.bytes_written > request.audio_capacity ||
         capture.bytes_written > GBL_V1_MAX_AUDIO_BYTES ||
         capture.bytes_written % 4u != 0u)) {
        return GBL_V1_INSUFFICIENT_CAPACITY;
    }

    published.struct_size = result->struct_size;
    published.requested_units = request.units;
    published.outcome = core_result.reason == GBL_RUN_REASON_UNIT_LIMIT
                             ? GBL_V1_ADVANCE_COMPLETED
                             : GBL_V1_ADVANCE_GUEST_TERMINAL;
    published.stop_reason = core_result.reason == GBL_RUN_REASON_UNIT_LIMIT
                                ? GBL_V1_STOP_UNIT_LIMIT
                                : core_result.reason ==
                                          GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION
                                      ? GBL_V1_STOP_INSTRUCTION_UNSUPPORTED
                                      : GBL_V1_STOP_GUEST_TERMINAL;
    published.requested_cycle_budget = request.cycle_budget;
    published.start_cycle = start_cycle;
    published.end_cycle = machine->candidate->master_cycles;
    published.elapsed_cycles = published.end_cycle - start_cycle;
    published.retired_instructions = core_result.instructions;
    if (capture_audio) {
        published.audio_available = 1u;
        published.audio_bytes_written = capture.bytes_written;
        published.audio_frames_written = capture.bytes_written / 4u;
    }
    gbl_v1_observation_from_core(machine->candidate,
                                 &published.observation);
    if (capture_audio && capture.bytes_written != 0u) {
        gbl_v1_copy(request.audio_bytes, machine->audio_capture,
                    capture.bytes_written);
    }
    gbl_v1_copy(machine->machine, machine->candidate, sizeof(gbl_machine));
    result_size = gbl_v1_output_size(result->struct_size, sizeof(published));
    gbl_v1_copy(result, &published, result_size);
    return GBL_V1_OK;
}

static bool gbl_v1_video_complete(const gbl_machine *core)
{
    size_t row;

    if (core->ppu.capture_state != GBL_VIDEO_CAPTURE_VALID ||
        core->ppu.captured_rows != GBL_PPU_VISIBLE_LINES) {
        return false;
    }
    for (row = 0u; row < GBL_PPU_VISIBLE_LINES; ++row) {
        const uint8_t mask = (uint8_t)(UINT8_C(1) << (row & 7u));

        if ((core->ppu.row_coverage[row >> 3u] & mask) == 0u) {
            return false;
        }
    }
    return true;
}

gbl_v1_status gbl_v1_read_video(const gbl_v1_machine *machine,
                                const gbl_v1_video *video)
{
    if (!gbl_v1_live(machine) || video == NULL ||
        !gbl_v1_is_aligned(video, _Alignof(gbl_v1_video)) ||
        video->struct_size < sizeof(*video) ||
        !gbl_v1_span_disjoint(machine, video, video->struct_size) ||
        (video->capacity != 0u && video->bytes == NULL) ||
        !gbl_v1_span_disjoint(machine, video->bytes, video->capacity) ||
        gbl_v1_add_overlaps(video, video->struct_size, video->bytes,
                            video->capacity)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    if (machine->machine->rom == NULL) {
        return GBL_V1_INVALID_LIFECYCLE;
    }
    if (!gbl_v1_video_complete(machine->machine)) {
        return GBL_V1_OUTPUT_UNAVAILABLE;
    }
    if (video->capacity < GBL_V1_VIDEO_BYTES) {
        return GBL_V1_INSUFFICIENT_CAPACITY;
    }
    gbl_v1_copy(video->bytes, machine->machine->video_frame,
                GBL_V1_VIDEO_BYTES);
    return GBL_V1_OK;
}

gbl_v1_status gbl_v1_observe(const gbl_v1_machine *machine,
                             gbl_v1_observation *observation)
{
    const size_t minimum = offsetof(gbl_v1_observation, profile_id);
    gbl_v1_observation published = { 0 };
    size_t output_size;

    if (!gbl_v1_live(machine) || observation == NULL ||
        !gbl_v1_is_aligned(observation, _Alignof(gbl_v1_observation)) ||
        observation->struct_size < minimum ||
        !gbl_v1_span_disjoint(machine, observation,
                              observation->struct_size)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    gbl_v1_observation_from_core(machine->machine, &published);
    published.struct_size = observation->struct_size;
    output_size = gbl_v1_output_size(observation->struct_size,
                                     sizeof(published));
    gbl_v1_copy(observation, &published, output_size);
    return GBL_V1_OK;
}
