#include "gamebloke/gamebloke_v1.h"

#include "gamebloke/gamebloke.h"
#include "audio_internal.h"

#include <stdbool.h>
#include <stdint.h>

#define GBL_V1_MAGIC UINT64_C(0x47424C4150495631)
#define GBL_V1_MAX_ADVANCE_CYCLES UINT64_C(16777216)
#define GBL_V1_AUDIO_SCRATCH_BYTES GBL_AUDIO_MAX_CAPTURE_BYTES

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
    size_t offset;

    if (config == NULL || out_machine == NULL ||
        !gbl_v1_is_aligned(config, _Alignof(gbl_v1_config)) ||
        !gbl_v1_is_aligned(out_machine, _Alignof(gbl_v1_machine *)) ||
        config->struct_size < minimum_config ||
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
    if (!gbl_v1_is_aligned(created, _Alignof(gbl_v1_machine))) {
        supplied.release(supplied.allocator_context, created, handle_size);
        return GBL_V1_ALLOCATION_FAILED;
    }
    scratch = supplied.allocate(supplied.allocator_context, scratch_size);
    if (scratch == NULL) {
        supplied.release(supplied.allocator_context, created, handle_size);
        return GBL_V1_ALLOCATION_FAILED;
    }
    if (!gbl_v1_is_aligned(scratch, _Alignof(gbl_machine))) {
        supplied.release(supplied.allocator_context, scratch, scratch_size);
        supplied.release(supplied.allocator_context, created, handle_size);
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
    core_allocator.allocate = NULL;
    core_allocator.release = NULL;
    /* Core ROM allocation forwards through the caller's snapshotted tuple. */
    core_allocator.context = created->allocator_context;
    core_allocator.allocate = (void *(*)(void *, size_t))created->allocate;
    core_allocator.release = created->release;
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
    if (rom->length > 0x02000000u ||
        !gbl_v1_span_disjoint(machine, rom->bytes, rom->length)) {
        return GBL_V1_CONTENT_REFUSED;
    }
    loaded = gbl_load_rom(machine->machine, rom->bytes, rom->length);
    return gbl_v1_map_load(loaded);
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
    request = (gbl_v1_advance_options){ 0 };
    gbl_v1_copy(&request, options,
                gbl_v1_output_size(options->struct_size, sizeof(request)));
    if (request.units == 0u || request.cycle_budget == 0u) {
        return GBL_V1_WORK_EXHAUSTED;
    }
    if (request.cycle_budget > GBL_V1_MAX_ADVANCE_CYCLES) {
        return GBL_V1_UNSUPPORTED;
    }
    if (request.struct_size >= sizeof(request) && request.capture_audio != 0u) {
        return GBL_V1_UNSUPPORTED;
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
    if (gbl_cpu_run(machine->candidate, &core_options, &core_result) !=
        GBL_RUN_STATUS_OK) {
        return GBL_V1_INTERNAL_REFUSAL;
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

    gbl_v1_copy(machine->machine, machine->candidate, sizeof(gbl_machine));
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
    published.end_cycle = machine->machine->master_cycles;
    published.elapsed_cycles = published.end_cycle - start_cycle;
    published.retired_instructions = core_result.instructions;
    result_size = gbl_v1_output_size(result->struct_size, sizeof(published));
    gbl_v1_copy(result, &published, result_size);
    return GBL_V1_OK;
}

gbl_v1_status gbl_v1_observe(const gbl_v1_machine *machine,
                             gbl_v1_observation *observation)
{
    const size_t minimum = offsetof(gbl_v1_observation,
                                    hardware_qualification_known) +
                           sizeof(observation->hardware_qualification_known);
    const gbl_machine *core;
    gbl_mailbox_result mailbox;
    gbl_v1_observation published = { 0 };
    size_t output_size;

    if (!gbl_v1_live(machine) || observation == NULL ||
        !gbl_v1_is_aligned(observation, _Alignof(gbl_v1_observation)) ||
        observation->struct_size < minimum ||
        !gbl_v1_span_disjoint(machine, observation,
                              observation->struct_size)) {
        return GBL_V1_INVALID_ARGUMENT;
    }
    core = machine->machine;
    mailbox = gbl_mailbox_read(core);
    published.struct_size = observation->struct_size;
    published.loaded = core->rom != NULL ? 1u : 0u;
    published.profile = GBL_V1_PROFILE_AGB;
    published.profile_version = 1u;
    published.bios_mode = 0u;
    published.cartridge_mode = GBL_V1_CARTRIDGE_ROM_ONLY;
    published.video_state = (uint32_t)core->ppu.capture_state;
    published.mailbox_state = (uint32_t)mailbox.status;
    published.hardware_qualification_known = 0u;
    published.raw_video_format = 1u;
    published.raw_audio_format = 1u;
    published.audio_sample_rate = (uint32_t)GBL_AUDIO_SAMPLE_RATE;
    published.audio_cycles_per_frame = (uint32_t)GBL_AUDIO_SAMPLE_CYCLES;
    published.audio_channels = 2u;
    published.logical_input_mask = core->pressed_keys;
    published.applied_input_count =
        (uint32_t)core->input_event_cursor;
    published.pending_input_count =
        (uint32_t)(core->input_event_count - core->input_event_cursor);
    published.mailbox_case_id = mailbox.case_id;
    published.input_generation = core->input_generation;
    published.master_cycle = core->master_cycles;
    published.retired_instructions = core->cpu.retired_instructions;
    published.video_epoch = core->ppu.frame_epoch;
    published.maximum_advance_cycles = GBL_V1_MAX_ADVANCE_CYCLES;
    output_size = gbl_v1_output_size(observation->struct_size,
                                     sizeof(published));
    gbl_v1_copy(observation, &published, output_size);
    return GBL_V1_OK;
}
