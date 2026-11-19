#include "run.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "sha256.h"

enum {
    GBL_RUNNER_MIN_ROM_BYTES = 0xC0u,
    GBL_RUNNER_ROM_SENTINEL_BYTES = 1u
};

#define GBL_RUNNER_MAILBOX_CONVENTION "five-word-v1-final-marker"
#define GBL_RUNNER_CPSR_T UINT32_C(0x00000020)
#define GBL_RUNNER_CPSR_MODE_MASK UINT32_C(0x0000001F)
#define GBL_RUNNER_MODE_USER UINT32_C(0x00000010)
#define GBL_RUNNER_MODE_FIQ UINT32_C(0x00000011)
#define GBL_RUNNER_MODE_IRQ UINT32_C(0x00000012)
#define GBL_RUNNER_MODE_SVC UINT32_C(0x00000013)
#define GBL_RUNNER_MODE_ABT UINT32_C(0x00000017)
#define GBL_RUNNER_MODE_UND UINT32_C(0x0000001B)
#define GBL_RUNNER_MODE_SYS UINT32_C(0x0000001F)
#define GBL_RUNNER_ROM_BASE UINT32_C(0x08000000)

typedef enum gbl_runner_read_status {
    GBL_RUNNER_READ_OK = 0,
    GBL_RUNNER_READ_MISSING,
    GBL_RUNNER_READ_REFUSED,
    GBL_RUNNER_READ_FAILED
} gbl_runner_read_status;

static void *gbl_runner_default_allocate(void *context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void gbl_runner_default_release(void *context, void *pointer,
                                       size_t size)
{
    (void)context;
    (void)size;
    free(pointer);
}

static bool gbl_runner_allocator_select(const gbl_runner_allocator *requested,
                                       gbl_runner_allocator *selected)
{
    if (requested == NULL || selected == NULL) {
        return false;
    }
    if ((requested->allocate == NULL) != (requested->release == NULL)) {
        return false;
    }
    *selected = *requested;
    if (selected->allocate == NULL) {
        selected->context = NULL;
        selected->allocate = gbl_runner_default_allocate;
        selected->release = gbl_runner_default_release;
    }
    return true;
}

static bool gbl_runner_copy_text(char *destination, size_t capacity,
                                 const char *source)
{
    size_t length = 0u;

    if (destination == NULL || capacity == 0u || source == NULL) {
        return false;
    }
    while (length < capacity && source[length] != '\0') {
        length++;
    }
    if (length == capacity) {
        return false;
    }
    memcpy(destination, source, length + 1u);
    return true;
}

static bool gbl_runner_span_end(const void *pointer, size_t length,
                                uintptr_t *end)
{
    const uintptr_t start = (uintptr_t)pointer;

    if (pointer == NULL || end == NULL || length == 0u ||
        (uintmax_t)length > (uintmax_t)UINTPTR_MAX - (uintmax_t)start) {
        return false;
    }
    *end = start + (uintptr_t)length;
    return true;
}

static bool gbl_runner_spans_overlap(const void *first, size_t first_length,
                                     const void *second,
                                     size_t second_length)
{
    uintptr_t first_end;
    uintptr_t second_end;
    const uintptr_t first_start = (uintptr_t)first;
    const uintptr_t second_start = (uintptr_t)second;

    if (!gbl_runner_span_end(first, first_length, &first_end) ||
        !gbl_runner_span_end(second, second_length, &second_end)) {
        return true;
    }
    return first_start < second_end && second_start < first_end;
}

static bool gbl_runner_input_timeline_copy(
    const gbl_runner_options *options, const gbl_runner_result *result,
    gbl_input_event copied[GBL_INPUT_EVENT_CAPACITY], size_t *copied_count)
{
    size_t span_size;
    size_t index;

    if (options == NULL || copied == NULL || copied_count == NULL) {
        return false;
    }
    *copied_count = 0u;
    if (!options->input_timeline_present) {
        return options->input_events == NULL &&
               options->input_event_count == 0u;
    }
    if (options->input_event_count > GBL_INPUT_EVENT_CAPACITY) {
        return false;
    }
    if (options->input_event_count == 0u) {
        return true;
    }
    if (options->input_events == NULL ||
        options->input_event_count > SIZE_MAX / sizeof(*copied)) {
        return false;
    }
    span_size = options->input_event_count * sizeof(*copied);
    {
        uintptr_t input_end;

        if (!gbl_runner_span_end(options->input_events, span_size,
                                 &input_end) ||
            gbl_runner_spans_overlap(options->input_events, span_size, result,
                                     sizeof(*result))) {
            return false;
        }
        (void)input_end;
    }
    memcpy(copied, options->input_events, span_size);
    for (index = 0u; index < options->input_event_count; ++index) {
        if ((copied[index].pressed_mask & (uint16_t)~UINT16_C(0x03FF)) !=
                0u ||
            (index != 0u &&
             copied[index].cycle < copied[index - 1u].cycle)) {
            return false;
        }
    }
    *copied_count = options->input_event_count;
    return true;
}

static bool gbl_runner_input_timeline_digest(
    const gbl_input_event *events, size_t event_count,
    char output[GBL_SHA256_HEX_CAPACITY])
{
    static const uint8_t domain[] = "gamebloke-input-timeline-1";
    uint8_t canonical[sizeof domain - 1u +
                      GBL_INPUT_EVENT_CAPACITY * 10u];
    size_t offset = sizeof domain - 1u;
    size_t index;

    if (event_count > GBL_INPUT_EVENT_CAPACITY ||
        (event_count != 0u && events == NULL)) {
        return false;
    }
    memcpy(canonical, domain, sizeof domain - 1u);
    for (index = 0u; index < event_count; ++index) {
        unsigned int byte_index;
        const uint64_t cycle = events[index].cycle;
        const uint16_t pressed_mask = events[index].pressed_mask;

        for (byte_index = 0u; byte_index < 8u; ++byte_index) {
            canonical[offset++] =
                (uint8_t)(cycle >> (byte_index * 8u));
        }
        canonical[offset++] = (uint8_t)pressed_mask;
        canonical[offset++] = (uint8_t)(pressed_mask >> 8u);
    }
    return gbl_sha256_span(canonical, offset, output);
}

static void gbl_runner_set_reason(gbl_runner_result *result,
                                  const char *reason)
{
    if (!gbl_runner_copy_text(result->reason, sizeof result->reason, reason)) {
        result->reason[0] = '\0';
    }
}

static void gbl_runner_set_termination(gbl_runner_result *result,
                                      const char *termination)
{
    if (!gbl_runner_copy_text(result->termination,
                              sizeof result->termination, termination)) {
        result->termination[0] = '\0';
    }
}

static void gbl_runner_set_refusal(gbl_runner_result *result,
                                   const char *code,
                                   const char *postcondition)
{
    result->has_refusal = true;
    if (!gbl_runner_copy_text(result->refusal_code,
                              sizeof result->refusal_code, code)) {
        result->refusal_code[0] = '\0';
    }
    if (!gbl_runner_copy_text(result->refusal_postcondition,
                              sizeof result->refusal_postcondition,
                              postcondition)) {
        result->refusal_postcondition[0] = '\0';
    }
}

static bool gbl_runner_copy_fixture_id(gbl_runner_result *result,
                                       const char *fixture_id)
{
    if (fixture_id == NULL) {
        return true;
    }
    if (!gbl_runner_copy_text(result->fixture_id,
                              sizeof result->fixture_id, fixture_id)) {
        return false;
    }
    result->has_fixture_id = true;
    return true;
}

static const gbl_fixture_metadata *gbl_runner_find_metadata(
    const char *fixture_id)
{
    size_t index;

    if (fixture_id == NULL || !gbl_fixture_index_qualified) {
        return NULL;
    }
    for (index = 0u; index < gbl_fixture_metadata_count; ++index) {
        const gbl_fixture_metadata *row = &gbl_fixture_metadata_rows[index];

        if (row->id != NULL && strcmp(row->id, fixture_id) == 0) {
            return row;
        }
    }
    return NULL;
}

static gbl_runner_read_status gbl_runner_read_rom(
    const gbl_runner_options *options, gbl_runner_result *result,
    uint8_t **bytes_out, size_t *length_out)
{
    const size_t capacity = GBL_RUNNER_MAX_ROM_BYTES +
                            (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES;
    FILE *input;
    uint8_t *bytes;
    size_t length = 0u;
    int read_error = 0;
    int close_error;

    *bytes_out = NULL;
    *length_out = 0u;
    input = fopen(options->rom_path, "rb");
    if (input == NULL) {
        if (errno == ENOENT || errno == ENOTDIR) {
            result->status = GBL_RUNNER_MISSING;
            gbl_runner_set_reason(result, "rom-file-not-found");
            gbl_runner_set_termination(result, "not-started");
            return GBL_RUNNER_READ_MISSING;
        }
        result->status = GBL_RUNNER_HARNESS_FAIL;
        gbl_runner_set_reason(result, "rom-open-failed");
        gbl_runner_set_termination(result, "not-started");
        return GBL_RUNNER_READ_FAILED;
    }

    bytes = result->allocator.allocate(result->allocator.context, capacity);
    if (bytes == NULL) {
        (void)fclose(input);
        result->status = GBL_RUNNER_HARNESS_FAIL;
        gbl_runner_set_reason(result, "rom-buffer-allocation-failed");
        gbl_runner_set_termination(result, "not-started");
        return GBL_RUNNER_READ_FAILED;
    }

    while (length < capacity) {
        const size_t amount = fread(bytes + length, 1u, capacity - length,
                                    input);

        length += amount;
        if (amount != 0u) {
            continue;
        }
        if (ferror(input) != 0) {
            read_error = 1;
            break;
        }
        if (feof(input) != 0) {
            break;
        }
        read_error = 1;
        break;
    }

    close_error = fclose(input);
    if (read_error != 0 || close_error != 0) {
        result->allocator.release(result->allocator.context, bytes, capacity);
        result->status = GBL_RUNNER_HARNESS_FAIL;
        gbl_runner_set_reason(result, "rom-read-failed");
        gbl_runner_set_termination(result, "not-started");
        return GBL_RUNNER_READ_FAILED;
    }

    if (length > GBL_RUNNER_MAX_ROM_BYTES) {
        result->allocator.release(result->allocator.context, bytes, capacity);
        result->status = GBL_RUNNER_REFUSED;
        gbl_runner_set_reason(result, "rom-too-large");
        gbl_runner_set_refusal(result, "rom-too-large", "guest-not-executed");
        gbl_runner_set_termination(result, "refused-before-run");
        return GBL_RUNNER_READ_REFUSED;
    }
    if (length == 0u) {
        result->allocator.release(result->allocator.context, bytes, capacity);
        result->status = GBL_RUNNER_REFUSED;
        gbl_runner_set_reason(result, "rom-empty");
        gbl_runner_set_refusal(result, "rom-empty", "guest-not-executed");
        gbl_runner_set_termination(result, "refused-before-run");
        return GBL_RUNNER_READ_REFUSED;
    }
    if (length < (size_t)GBL_RUNNER_MIN_ROM_BYTES) {
        result->allocator.release(result->allocator.context, bytes, capacity);
        result->status = GBL_RUNNER_REFUSED;
        gbl_runner_set_reason(result, "rom-too-short");
        gbl_runner_set_refusal(result, "rom-too-short", "guest-not-executed");
        gbl_runner_set_termination(result, "refused-before-run");
        return GBL_RUNNER_READ_REFUSED;
    }
    if ((length & 1u) != 0u) {
        result->allocator.release(result->allocator.context, bytes, capacity);
        result->status = GBL_RUNNER_REFUSED;
        gbl_runner_set_reason(result, "rom-odd-length");
        gbl_runner_set_refusal(result, "rom-odd-length", "guest-not-executed");
        gbl_runner_set_termination(result, "refused-before-run");
        return GBL_RUNNER_READ_REFUSED;
    }

    *bytes_out = bytes;
    *length_out = length;
    return GBL_RUNNER_READ_OK;
}

static bool gbl_runner_trace_options_valid(const gbl_runner_options *options)
{
    if ((options->trace_mask & ~GBL_TRACE_CATEGORY_MASK) != 0u ||
        options->trace_capacity > GBL_RUNNER_MAX_TRACE_RECORDS ||
        options->trace_capacity > SIZE_MAX / sizeof(gbl_trace_record)) {
        return false;
    }
    return true;
}

static bool gbl_runner_semantic_terminal(gbl_mailbox_result mailbox)
{
    return mailbox.status == GBL_MAILBOX_PASS ||
           mailbox.status == GBL_MAILBOX_FAIL;
}

static bool gbl_runner_source_commit_valid(const char *source_commit)
{
    size_t index;

    if (source_commit == NULL || strlen(source_commit) != 40u) {
        return false;
    }
    for (index = 0u; index < 40u; ++index) {
        const char character = source_commit[index];

        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

static bool gbl_runner_source_path_valid(const char *source_path)
{
    size_t index;

    if (source_path == NULL || source_path[0] == '\0' ||
        source_path[0] == '/' || strstr(source_path, "..") != NULL) {
        return false;
    }
    for (index = 0u; source_path[index] != '\0'; ++index) {
        const char character = source_path[index];
        const bool allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '/' ||
            character == '_' || character == '-' || character == '.';

        if (!allowed) {
            return false;
        }
    }
    return true;
}

static const char *gbl_runner_register_metadata_error(
    const gbl_fixture_metadata *metadata, size_t rom_length)
{
    const char *instruction_set;
    uint32_t alignment;
    uint64_t offset;

    if (metadata == NULL || metadata->result_channel_kind == NULL ||
        strcmp(metadata->result_channel_kind, "register-eval") != 0) {
        return "fixture-result-channel-unsupported";
    }
    if (!gbl_runner_source_commit_valid(metadata->result_source_commit) ||
        !gbl_runner_source_path_valid(metadata->result_source_path)) {
        return "fixture-result-source-invalid";
    }
    instruction_set = metadata->result_instruction_set;
    if (instruction_set == NULL ||
        (strcmp(instruction_set, "arm") != 0 &&
         strcmp(instruction_set, "thumb") != 0)) {
        return "fixture-result-state-invalid";
    }
    alignment = strcmp(instruction_set, "arm") == 0 ? 4u : 2u;
    if (metadata->result_register_index >= 15u) {
        return "fixture-result-register-invalid";
    }
    if (metadata->result_entry_address < GBL_RUNNER_ROM_BASE ||
        metadata->result_entry_address % alignment != 0u) {
        return "fixture-result-entry-misaligned";
    }
    offset = (uint64_t)metadata->result_entry_address -
             (uint64_t)GBL_RUNNER_ROM_BASE;
    if (offset >= (uint64_t)rom_length ||
        (uint64_t)alignment > (uint64_t)rom_length - offset) {
        return "fixture-result-entry-out-of-range";
    }
    return NULL;
}

static bool gbl_runner_register_observation_prepare(
    gbl_runner_result *result, const gbl_fixture_metadata *metadata,
    size_t rom_length)
{
    const char *error = gbl_runner_register_metadata_error(metadata, rom_length);

    if (error != NULL) {
        result->status = GBL_RUNNER_REFUSED;
        gbl_runner_set_reason(result, error);
        gbl_runner_set_refusal(result, error, "guest-not-executed");
        gbl_runner_set_termination(result, "refused-before-run");
        return false;
    }
    result->register_observation.present = true;
    result->register_observation.observed = false;
    result->register_observation.has_value = false;
    result->register_observation.entry_address =
        metadata->result_entry_address;
    result->register_observation.register_index =
        metadata->result_register_index;
    if (!gbl_runner_copy_text(
            result->register_observation.source_commit,
            sizeof result->register_observation.source_commit,
            metadata->result_source_commit) ||
        !gbl_runner_copy_text(result->register_observation.instruction_set,
                              sizeof result->register_observation.instruction_set,
                              metadata->result_instruction_set)) {
        result->register_observation = (gbl_runner_register_observation){ 0 };
        result->status = GBL_RUNNER_REFUSED;
        gbl_runner_set_reason(result, "fixture-result-metadata-invalid");
        gbl_runner_set_refusal(result, "fixture-result-metadata-invalid",
                               "guest-not-executed");
        gbl_runner_set_termination(result, "refused-before-run");
        return false;
    }
    return true;
}

static uint32_t gbl_runner_register_value(const gbl_cpu_state *cpu,
                                          unsigned register_index)
{
    const uint32_t mode = cpu->cpsr & GBL_RUNNER_CPSR_MODE_MASK;
    const unsigned bank_index = register_index - 8u;

    if (register_index < 8u) {
        return cpu->r0_7[register_index];
    }
    if (mode == GBL_RUNNER_MODE_FIQ) {
        return cpu->fiq_r8_14[bank_index];
    }
    if (register_index < 13u) {
        return cpu->usr_r8_14[bank_index];
    }
    switch (mode) {
    case GBL_RUNNER_MODE_IRQ:
        return cpu->irq_r13_14[register_index - 13u];
    case GBL_RUNNER_MODE_SVC:
        return cpu->svc_r13_14[register_index - 13u];
    case GBL_RUNNER_MODE_ABT:
        return cpu->abt_r13_14[register_index - 13u];
    case GBL_RUNNER_MODE_UND:
        return cpu->und_r13_14[register_index - 13u];
    case GBL_RUNNER_MODE_USER:
    case GBL_RUNNER_MODE_SYS:
    default:
        return cpu->usr_r8_14[bank_index];
    }
}

static bool gbl_runner_next_instruction(const gbl_machine *machine,
                                        uint32_t *address_out,
                                        bool *thumb_out)
{
    const gbl_cpu_state *cpu;
    const bool thumb = machine != NULL &&
                       (machine->cpu.cpsr & GBL_RUNNER_CPSR_T) != 0u;
    const uint8_t width = (uint8_t)(thumb ? 2u : 4u);

    if (machine == NULL || address_out == NULL || thumb_out == NULL ||
        machine->cpu.stopped) {
        return false;
    }
    cpu = &machine->cpu;
    if (!cpu->pipeline[0].valid || !cpu->pipeline[1].valid ||
        cpu->pipeline[0].width != width || cpu->pipeline[1].width != width ||
        cpu->execution_address != cpu->pipeline[0].address ||
        cpu->pipeline[0].address % width != 0u) {
        return false;
    }
    *address_out = cpu->pipeline[0].address;
    *thumb_out = thumb;
    return true;
}

static bool gbl_runner_observe_register(gbl_runner_result *result,
                                        const gbl_machine *machine)
{
    uint32_t address;
    bool thumb;
    uint32_t value;
    const bool expected_thumb =
        strcmp(result->register_observation.instruction_set, "thumb") == 0;

    if (!result->register_observation.present ||
        result->register_observation.observed ||
        !gbl_runner_next_instruction(machine, &address, &thumb) ||
        thumb != expected_thumb ||
        address != result->register_observation.entry_address) {
        return false;
    }
    value = gbl_runner_register_value(
        &machine->cpu, result->register_observation.register_index);
    result->register_observation.observed = true;
    result->register_observation.has_value = true;
    result->register_observation.value = value;
    result->status = value == 0u ? GBL_RUNNER_PASS : GBL_RUNNER_GUEST_FAIL;
    gbl_runner_set_reason(result, value == 0u ? "register-eval-pass"
                                              : "register-eval-fail");
    gbl_runner_set_termination(result, "register-eval-complete");
    return true;
}

static void gbl_runner_phase8_observe(gbl_runner_result *result,
                                     const gbl_machine *machine,
                                     const gbl_run_result *run_result)
{
    gbl_runner_phase8_observation *observation = &result->phase8;

    observation->observed = true;
    observation->interrupt_flags = machine->interrupt_flags;
    observation->interrupt_enable = machine->interrupt_enable;
    observation->interrupt_master_enable = machine->interrupt_master_enable;
    observation->has_irq_sample = machine->cpu.irq_observation.sampled;
    if (observation->has_irq_sample) {
        observation->irq_sample = machine->cpu.irq_observation;
    }
    observation->last_exception = machine->cpu.last_exception.kind;
    observation->power_state = machine->power_state;
    if (run_result->power_cause != GBL_CPU_POWER_CAUSE_NONE) {
        observation->power_cause = run_result->power_cause;
    }
    if (run_result->power_cause == GBL_CPU_POWER_CAUSE_WAKE_PENDING ||
        run_result->power_cause == GBL_CPU_POWER_CAUSE_WAKE_EVENT) {
        observation->has_wake = true;
        observation->wake_source_mask = run_result->wake_source_mask;
        observation->wake_cycle = run_result->wake_cycle;
    }
    observation->keyinput = (uint16_t)(~machine->pressed_keys &
                                       UINT16_C(0x03FF));
    observation->key_control = machine->key_control;
    observation->key_condition = machine->key_condition;
    observation->key_irq_condition = machine->key_irq_condition;
    observation->applied_input_events = machine->input_event_cursor;
    observation->pending_input_events =
        machine->input_event_count - machine->input_event_cursor;
    observation->wait_cycles = result->wait_cycles;
}

static void gbl_runner_phase9_observe(gbl_runner_result *result,
                                      const gbl_machine *machine)
{
    gbl_runner_phase9_observation *observation = &result->phase9;
    bool latch_fallback_observed = false;
    size_t index;

    observation->observed = true;
    observation->waitcnt = machine->waitcnt;
    observation->bus_owner = machine->bus_owner;
    observation->machine_cycles = machine->master_cycles;
    memcpy(observation->channels, machine->dma_channels,
           sizeof observation->channels);
    if ((result->trace_mask & (uint32_t)GBL_TRACE_DMA) != 0u) {
        const size_t available = result->trace_count < result->trace_capacity
                                     ? result->trace_count
                                     : result->trace_capacity;

        for (index = 0u; index < available; ++index) {
            const gbl_trace_record *record = &result->trace_records[index];

            if (record->category == GBL_TRACE_DMA &&
                record->dma_used_channel_latch) {
                latch_fallback_observed = true;
                break;
            }
        }
    }
    observation->latch_fallback_known = latch_fallback_observed ||
        (result->trace_complete &&
         (result->trace_mask & (uint32_t)GBL_TRACE_DMA) != 0u);
    observation->latch_fallback_observed = latch_fallback_observed;
}

static void gbl_runner_phase10_observe(gbl_runner_result *result,
                                       const gbl_machine *machine)
{
    gbl_runner_phase10_observation *observation = &result->phase10;
    uint64_t frame_start_cycle;
    uint64_t completion_offset =
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES;
    size_t index;

    observation->observed = true;
    observation->profile_version = GBL_RUNNER_PROFILE_VERSION;
    observation->frame_started = machine->ppu.frame_started;
    observation->line = machine->ppu.line;
    observation->line_cycle = machine->master_cycles >= machine->ppu.line_origin
                                  ? machine->master_cycles -
                                        machine->ppu.line_origin
                                  : 0u;
    observation->machine_cycles = machine->master_cycles;
    observation->frame_epoch = machine->ppu.frame_epoch;
    observation->has_frame_epoch = machine->ppu.frame_started;
    observation->capture_state = machine->ppu.capture_state;
    observation->capture_reason = machine->ppu.capture_reason;
    observation->supported_controls =
        machine->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_NONE ||
        machine->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_INCOMPLETE;
    observation->video_requested = result->phase10.video_requested;

    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        const gbl_dma_channel_state *channel = &machine->dma_channels[index];

        if (channel->trigger_origin == GBL_DMA_TRIGGER_ORIGIN_HBLANK ||
            channel->trigger_dispute == GBL_DMA_DISPUTE_HBLANK_FLAG_EDGE) {
            observation->hblank_trigger_dependency = true;
            break;
        }
    }

    if (machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID &&
        machine->ppu.frame_started &&
        machine->ppu.frame_epoch <=
            (UINT64_MAX - completion_offset) / GBL_PPU_FRAME_CYCLES) {
        frame_start_cycle = machine->ppu.frame_epoch * GBL_PPU_FRAME_CYCLES;
        observation->sample_cycle = frame_start_cycle;
        observation->has_sample_cycle = true;
        observation->completion_cycle = frame_start_cycle + completion_offset;
        observation->has_completion_cycle = true;
    }
}

static void gbl_runner_set_video_error(gbl_runner_result *result,
                                      const char *error)
{
    size_t length = strlen(error);

    if (length >= sizeof result->phase10.video_output_error) {
        return;
    }
    memcpy(result->phase10.video_output_error, error, length + 1u);
}

static bool gbl_runner_phase10_capture_video(
    const gbl_runner_options *options, gbl_runner_result *result,
    const gbl_machine *machine)
{
    gbl_runner_phase10_observation *observation;
    uint64_t expected_sample_cycle;
    const uint64_t completion_offset =
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES;

    gbl_runner_phase10_observe(result, machine);
    observation = &result->phase10;
    if (!result->phase10.video_requested) {
        return true;
    }
    if (!observation->observed || !observation->frame_started ||
        !observation->has_frame_epoch ||
        observation->capture_state != GBL_VIDEO_CAPTURE_VALID ||
        observation->capture_reason != GBL_VIDEO_INVALID_REASON_NONE ||
        !observation->supported_controls ||
        !observation->has_sample_cycle ||
        !observation->has_completion_cycle ||
        observation->frame_epoch > UINT64_MAX / GBL_PPU_FRAME_CYCLES) {
        gbl_runner_set_video_error(result, "video-frame-unavailable");
        return false;
    }
    expected_sample_cycle =
        observation->frame_epoch * GBL_PPU_FRAME_CYCLES;
    if (observation->sample_cycle != expected_sample_cycle ||
        observation->completion_cycle < observation->sample_cycle ||
        observation->completion_cycle - observation->sample_cycle !=
            completion_offset ||
        observation->machine_cycles != machine->master_cycles ||
        observation->machine_cycles < observation->completion_cycle) {
        gbl_runner_set_video_error(result, "video-frame-unavailable");
        return false;
    }
    result->phase10.video_bytes = result->allocator.allocate(
        result->allocator.context, GBL_RUNNER_VIDEO_BYTES);
    if (result->phase10.video_bytes == NULL) {
        gbl_runner_set_video_error(result, "video-buffer-allocation-failed");
        return false;
    }
    result->phase10.video_length = GBL_RUNNER_VIDEO_BYTES;
    memcpy(result->phase10.video_bytes, machine->video_frame,
           GBL_RUNNER_VIDEO_BYTES);
    return gbl_runner_write_video(options->video_output_path, result);
}

static void gbl_runner_video_output_failed(gbl_runner_result *result)
{
    result->status = GBL_RUNNER_HARNESS_FAIL;
    gbl_runner_set_reason(result, "video-output-failed");
}

static bool gbl_runner_u64_add(uint64_t left, uint64_t right,
                               uint64_t *sum_out)
{
    if (sum_out == NULL || right > UINT64_MAX - left) {
        return false;
    }
    *sum_out = left + right;
    return true;
}

static bool gbl_runner_run_accounting(const gbl_run_result *run_result,
                                     uint64_t *cpu_bus_out,
                                     uint64_t *cpu_internal_out)
{
    uint64_t dma_access_cycles;
    uint64_t total_cycles;

    if (run_result == NULL || cpu_bus_out == NULL ||
        cpu_internal_out == NULL ||
        !gbl_runner_u64_add(run_result->dma_source_cycles,
                            run_result->dma_destination_cycles,
                            &dma_access_cycles) ||
        dma_access_cycles > run_result->bus_cycles ||
        run_result->dma_startup_cycles > run_result->internal_cycles) {
        return false;
    }
    *cpu_bus_out = run_result->bus_cycles - dma_access_cycles;
    *cpu_internal_out =
        run_result->internal_cycles - run_result->dma_startup_cycles;
    if (!gbl_runner_u64_add(*cpu_bus_out, *cpu_internal_out, &total_cycles) ||
        !gbl_runner_u64_add(total_cycles, run_result->wait_cycles,
                            &total_cycles) ||
        !gbl_runner_u64_add(total_cycles, run_result->dma_startup_cycles,
                            &total_cycles) ||
        !gbl_runner_u64_add(total_cycles, run_result->dma_source_cycles,
                            &total_cycles) ||
        !gbl_runner_u64_add(total_cycles,
                            run_result->dma_destination_cycles,
                            &total_cycles)) {
        return false;
    }
    return total_cycles == run_result->cycles;
}

void gbl_runner_classify_guest_stop(gbl_runner_result *result,
                                    const gbl_machine *machine,
                                    gbl_run_reason reason)
{
    const char *name = "guest-stop";

    if (reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION) {
        name = "unsupported-instruction";
    } else if (reason == GBL_RUN_REASON_UNPREDICTABLE_INSTRUCTION) {
        name = "unpredictable-instruction";
    } else if (reason == GBL_RUN_REASON_LATER_ARCHITECTURE_INSTRUCTION) {
        name = "later-architecture-instruction";
    } else if (reason == GBL_RUN_REASON_UNDEFINED_INSTRUCTION) {
        name = "undefined-instruction";
    } else if (reason == GBL_RUN_REASON_BUS_ERROR) {
        name = "bus-error";
    } else if (reason == GBL_RUN_REASON_GUEST_STOP) {
        switch (machine->cpu.stop_reason) {
        case GBL_CPU_STOP_RESET_NO_BIOS: name = "no-bios-reset"; break;
        case GBL_CPU_STOP_UNDEFINED_NO_BIOS: name = "no-bios-undefined"; break;
        case GBL_CPU_STOP_SWI_NO_BIOS: name = "no-bios-swi"; break;
        case GBL_CPU_STOP_PREFETCH_ABORT_NO_BIOS:
            name = "no-bios-prefetch-abort";
            break;
        case GBL_CPU_STOP_DATA_ABORT_NO_BIOS:
            name = "no-bios-data-abort";
            break;
        case GBL_CPU_STOP_IRQ_NO_BIOS: name = "no-bios-irq"; break;
        case GBL_CPU_STOP_FIQ_NO_BIOS: name = "no-bios-fiq"; break;
        case GBL_CPU_STOP_STOP_REQUESTED: name = "stop-requested"; break;
        case GBL_CPU_STOP_HALT_DEADLOCK: name = "halt-deadlock"; break;
        case GBL_CPU_STOP_COPROCESSOR_NO_BIOS:
            name = "no-bios-coprocessor";
            break;
        case GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION:
            name = "unsupported-instruction";
            break;
        case GBL_CPU_STOP_UNPREDICTABLE_INSTRUCTION:
            name = "unpredictable-instruction";
            break;
        case GBL_CPU_STOP_LATER_ARCHITECTURE_INSTRUCTION:
            name = "later-architecture-instruction";
            break;
        case GBL_CPU_STOP_BUS_ERROR: name = "bus-error"; break;
        case GBL_CPU_STOP_COUNTER_OVERFLOW: name = "counter-overflow"; break;
        case GBL_CPU_STOP_INTERNAL_ERROR: name = "internal-error"; break;
        case GBL_CPU_STOP_NONE: name = "guest-stop"; break;
        }
    }
    result->status = GBL_RUNNER_GUEST_STOP;
    gbl_runner_set_reason(result, name);
    gbl_runner_set_termination(result, name);
}

static void gbl_runner_classify_terminal(gbl_runner_result *result)
{
    if (result->metadata == NULL) {
        result->status = GBL_RUNNER_GUEST_STOP;
        gbl_runner_set_reason(result, "unregistered-content");
        gbl_runner_set_termination(result, "unregistered-content");
    } else if (result->semantic.status == GBL_MAILBOX_PASS) {
        result->status = GBL_RUNNER_PASS;
        gbl_runner_set_reason(result, "guest-mailbox-pass");
        gbl_runner_set_termination(result, "mailbox-complete");
    } else {
        result->status = GBL_RUNNER_GUEST_FAIL;
        gbl_runner_set_reason(result, "guest-mailbox-fail");
        gbl_runner_set_termination(result, "mailbox-complete");
    }
}

static bool gbl_runner_write_trace_if_requested(
    const gbl_runner_options *options, gbl_runner_result *result)
{
    if (options->trace_output_path == NULL) {
        return true;
    }
    if (!gbl_runner_write_trace(options->trace_output_path, result)) {
        result->status = GBL_RUNNER_HARNESS_FAIL;
        gbl_runner_set_reason(result, "trace-write-failed");
        gbl_runner_set_termination(result, "trace-write-failed");
        return false;
    }
    result->trace_emitted = true;
    return true;
}

gbl_runner_status gbl_runner_execute(const gbl_runner_options *options,
                                     gbl_runner_result *result)
{
    gbl_runner_allocator selected_allocator;
    gbl_machine *machine = NULL;
    gbl_allocator machine_allocator;
    gbl_input_event copied_input_events[GBL_INPUT_EVENT_CAPACITY] = { 0 };
    char requested_fixture_id[GBL_RUNNER_FIXTURE_ID_CAPACITY] = { '\0' };
    char input_timeline_sha256[GBL_SHA256_HEX_CAPACITY];
    size_t copied_input_event_count = 0u;
    bool has_fixture_argument;
    bool options_overlap_result;
    bool input_timeline_valid = false;
    uint8_t *rom_bytes = NULL;
    size_t rom_length = 0u;
    char copied_digest[GBL_SHA256_HEX_CAPACITY];
    gbl_runner_read_status read_status;
    bool has_committed_unit = false;

    if (result == NULL) {
        return GBL_RUNNER_HARNESS_FAIL;
    }
    options_overlap_result =
        options != NULL &&
        gbl_runner_spans_overlap(options, sizeof(*options), result,
                                 sizeof(*result));
    if (options != NULL && !options_overlap_result) {
        input_timeline_valid = gbl_runner_input_timeline_copy(
            options, result, copied_input_events,
            &copied_input_event_count);
    }
    memset(result, 0, sizeof *result);
    result->status = GBL_RUNNER_HARNESS_FAIL;
    result->semantic.status = GBL_MAILBOX_ABSENT;
    result->phase10.profile_version = GBL_RUNNER_PROFILE_VERSION;
    result->trace_complete = true;
    gbl_runner_set_reason(result, "runner-failed");
    gbl_runner_set_termination(result, "not-started");

    if (options == NULL || options_overlap_result ||
        options->rom_path == NULL ||
        options->rom_path[0] == '\0' ||
        !gbl_runner_allocator_select(&options->allocator,
                                     &selected_allocator)) {
        gbl_runner_set_reason(result, "invalid-runner-options");
        return result->status;
    }
    result->allocator = selected_allocator;
    result->budget_cycles = options->cycle_budget;
    result->trace_mask = options->trace_mask;
    result->trace_capacity = options->trace_capacity;
    result->phase10.video_requested = options->video_output_path != NULL;
    has_fixture_argument = options->fixture_id != NULL;
    if ((has_fixture_argument &&
         !gbl_runner_copy_text(requested_fixture_id,
                               sizeof requested_fixture_id,
                               options->fixture_id)) ||
        (options->trace_output_path != NULL &&
         options->trace_output_path[0] == '\0') ||
        (options->video_output_path != NULL &&
         options->video_output_path[0] == '\0') ||
        !gbl_runner_trace_options_valid(options)) {
        if (options->video_output_path != NULL &&
            options->video_output_path[0] == '\0') {
            gbl_runner_set_video_error(result, "video-output-path-empty");
        }
        gbl_runner_set_reason(result, "invalid-runner-options");
        gbl_runner_set_refusal(result, "invalid-runner-options",
                               "guest-not-executed");
        gbl_runner_set_termination(result, "refused-before-run");
        result->status = GBL_RUNNER_REFUSED;
        return result->status;
    }
    if (!input_timeline_valid) {
        result->status = GBL_RUNNER_REFUSED;
        gbl_runner_set_reason(result, "invalid-input-timeline");
        gbl_runner_set_refusal(result, "invalid-input-timeline",
                               "guest-not-executed");
        gbl_runner_set_termination(result, "refused-before-run");
        return result->status;
    }
    if (options->input_timeline_present) {
        if (!gbl_runner_input_timeline_digest(
                copied_input_events, copied_input_event_count,
                input_timeline_sha256)) {
            result->status = GBL_RUNNER_HARNESS_FAIL;
            gbl_runner_set_reason(result, "input-timeline-digest-failed");
            gbl_runner_set_termination(result, "not-started");
            return result->status;
        }
        result->input_timeline_present = true;
        result->has_input_timeline_sha256 = true;
        memcpy(result->input_timeline_sha256, input_timeline_sha256,
               sizeof result->input_timeline_sha256);
    }

    if (has_fixture_argument) {
        const gbl_fixture_metadata *selected_metadata =
            gbl_runner_find_metadata(requested_fixture_id);

        if (selected_metadata != NULL &&
            selected_metadata->bios_requirement != NULL &&
            strcmp(selected_metadata->bios_requirement, "user-official") ==
                0) {
            result->metadata = selected_metadata;
            if (!gbl_runner_copy_fixture_id(result,
                                            selected_metadata->id)) {
                result->metadata = NULL;
                result->status = GBL_RUNNER_REFUSED;
                gbl_runner_set_reason(result, "fixture-id-too-long");
                gbl_runner_set_refusal(result, "fixture-id-too-long",
                                       "guest-not-executed");
                gbl_runner_set_termination(result, "refused-before-run");
                return result->status;
            }
            result->status = GBL_RUNNER_SKIPPED;
            gbl_runner_set_reason(result, "official-bios-required");
            gbl_runner_set_termination(result, "skipped-before-run");
            return result->status;
        }
    }

    read_status = gbl_runner_read_rom(options, result, &rom_bytes,
                                      &rom_length);
    if (read_status != GBL_RUNNER_READ_OK) {
        return result->status;
    }
    if (!gbl_sha256_span(rom_bytes, rom_length, result->rom_sha256)) {
        result->allocator.release(result->allocator.context, rom_bytes,
                                  GBL_RUNNER_MAX_ROM_BYTES +
                                      (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
        result->status = GBL_RUNNER_HARNESS_FAIL;
        gbl_runner_set_reason(result, "rom-digest-failed");
        gbl_runner_set_termination(result, "not-started");
        return result->status;
    }
    result->has_rom_sha256 = true;

    if (has_fixture_argument) {
        result->metadata = gbl_runner_find_metadata(requested_fixture_id);
        if (result->metadata == NULL) {
            result->status = GBL_RUNNER_REFUSED;
            gbl_runner_set_reason(result, "fixture-unknown");
            gbl_runner_set_refusal(result, "fixture-unknown",
                                   "guest-not-executed");
            gbl_runner_set_termination(result, "refused-before-run");
            result->allocator.release(result->allocator.context, rom_bytes,
                                      GBL_RUNNER_MAX_ROM_BYTES +
                                          (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
        if (!gbl_runner_copy_fixture_id(result, result->metadata->id)) {
            result->metadata = NULL;
            result->status = GBL_RUNNER_REFUSED;
            gbl_runner_set_reason(result, "fixture-id-too-long");
            gbl_runner_set_refusal(result, "fixture-id-too-long",
                                   "guest-not-executed");
            gbl_runner_set_termination(result, "refused-before-run");
            result->allocator.release(result->allocator.context, rom_bytes,
                                      GBL_RUNNER_MAX_ROM_BYTES +
                                          (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
        if (result->metadata->rom_sha256 != NULL &&
            result->metadata->rom_sha256[0] != '\0') {
            result->has_expected_sha256 = gbl_runner_copy_text(
                result->expected_sha256, sizeof result->expected_sha256,
                result->metadata->rom_sha256);
        }
        if (!result->has_expected_sha256) {
            result->status = GBL_RUNNER_REFUSED;
            gbl_runner_set_reason(result, "fixture-digest-unavailable");
            gbl_runner_set_refusal(result, "fixture-digest-unavailable",
                                   "guest-not-executed");
            gbl_runner_set_termination(result, "refused-before-run");
            result->allocator.release(result->allocator.context, rom_bytes,
                                      GBL_RUNNER_MAX_ROM_BYTES +
                                          (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
        if (strcmp(result->rom_sha256, result->expected_sha256) != 0) {
            result->status = GBL_RUNNER_REFUSED;
            gbl_runner_set_reason(result, "fixture-digest-mismatch");
            gbl_runner_set_refusal(result, "fixture-digest-mismatch",
                                   "guest-not-executed");
            gbl_runner_set_termination(result, "refused-before-run");
            result->allocator.release(result->allocator.context, rom_bytes,
                                      GBL_RUNNER_MAX_ROM_BYTES +
                                          (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
        if (result->metadata->bios_requirement == NULL ||
            strcmp(result->metadata->bios_requirement, "none") != 0) {
            result->status = GBL_RUNNER_SKIPPED;
            gbl_runner_set_reason(result, "required-bios-unavailable");
            gbl_runner_set_termination(result, "skipped-before-run");
            result->allocator.release(result->allocator.context, rom_bytes,
                                      GBL_RUNNER_MAX_ROM_BYTES +
                                          (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
        if (result->metadata->expected_mailbox_convention == NULL ||
            strcmp(result->metadata->expected_mailbox_convention,
                   GBL_RUNNER_MAILBOX_CONVENTION) != 0) {
            result->status = GBL_RUNNER_REFUSED;
            gbl_runner_set_reason(result, "fixture-mailbox-unsupported");
            gbl_runner_set_refusal(result, "fixture-mailbox-unsupported",
                                   "guest-not-executed");
            gbl_runner_set_termination(result, "refused-before-run");
            result->allocator.release(result->allocator.context, rom_bytes,
                                      GBL_RUNNER_MAX_ROM_BYTES +
                                          (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
        if (result->metadata->result_channel_kind == NULL ||
            strcmp(result->metadata->result_channel_kind, "mailbox") == 0) {
            /* Legacy mailbox fixtures retain the original result path. */
        } else if (strcmp(result->metadata->result_channel_kind,
                          "register-eval") == 0) {
            if (!gbl_runner_register_observation_prepare(
                    result, result->metadata, rom_length)) {
                result->allocator.release(
                    result->allocator.context, rom_bytes,
                    GBL_RUNNER_MAX_ROM_BYTES +
                        (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
                return result->status;
            }
        } else {
            result->status = GBL_RUNNER_REFUSED;
            gbl_runner_set_reason(result, "fixture-result-channel-unsupported");
            gbl_runner_set_refusal(result,
                                   "fixture-result-channel-unsupported",
                                   "guest-not-executed");
            gbl_runner_set_termination(result, "refused-before-run");
            result->allocator.release(
                result->allocator.context, rom_bytes,
                GBL_RUNNER_MAX_ROM_BYTES +
                    (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
    }

    if (options->trace_mask != 0u && options->trace_capacity != 0u) {
        result->trace_records = result->allocator.allocate(
            result->allocator.context,
            options->trace_capacity * sizeof(*result->trace_records));
        if (result->trace_records == NULL) {
            result->status = GBL_RUNNER_HARNESS_FAIL;
            gbl_runner_set_reason(result, "trace-buffer-allocation-failed");
            gbl_runner_set_termination(result, "not-started");
            result->allocator.release(
                result->allocator.context, rom_bytes,
                GBL_RUNNER_MAX_ROM_BYTES +
                    (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
    }

    machine = result->allocator.allocate(result->allocator.context,
                                         sizeof *machine);
    if (machine == NULL) {
        result->status = GBL_RUNNER_HARNESS_FAIL;
        gbl_runner_set_reason(result, "machine-allocation-failed");
        gbl_runner_set_termination(result, "not-started");
        result->allocator.release(result->allocator.context, rom_bytes,
                                  GBL_RUNNER_MAX_ROM_BYTES +
                                      (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
        return result->status;
    }
    machine_allocator.context = result->allocator.context;
    machine_allocator.allocate = result->allocator.allocate;
    machine_allocator.release = result->allocator.release;
    if (gbl_machine_init(machine, &machine_allocator) != GBL_BUS_STATUS_OK) {
        result->allocator.release(result->allocator.context, machine,
                                  sizeof *machine);
        result->allocator.release(result->allocator.context, rom_bytes,
                                  GBL_RUNNER_MAX_ROM_BYTES +
                                      (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
        result->status = GBL_RUNNER_HARNESS_FAIL;
        gbl_runner_set_reason(result, "machine-initialization-failed");
        gbl_runner_set_termination(result, "not-started");
        return result->status;
    }

    {
        const gbl_load_result load_result =
            gbl_load_rom(machine, rom_bytes, rom_length);

        if (load_result.status != GBL_LOAD_STATUS_OK) {
            if (load_result.status == GBL_LOAD_STATUS_REFUSED) {
                result->status = GBL_RUNNER_REFUSED;
                gbl_runner_set_reason(result, "rom-refused-by-loader");
                gbl_runner_set_refusal(result, "rom-refused-by-loader",
                                       "machine-remains-unloaded");
                gbl_runner_set_termination(result, "refused-before-run");
            } else {
                result->status = GBL_RUNNER_HARNESS_FAIL;
                gbl_runner_set_reason(result,
                                      load_result.reason ==
                                              GBL_LOAD_REASON_ALLOCATION
                                          ? "rom-copy-allocation-failed"
                                          : "rom-load-failed");
                gbl_runner_set_termination(result, "not-started");
            }
            gbl_runner_phase9_observe(result, machine);
            (void)gbl_runner_phase10_capture_video(options, result, machine);
            gbl_machine_dispose(machine);
            result->allocator.release(result->allocator.context, machine,
                                      sizeof *machine);
            result->allocator.release(result->allocator.context, rom_bytes,
                                      GBL_RUNNER_MAX_ROM_BYTES +
                                          (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
    }

    if (options->input_timeline_present &&
        gbl_machine_set_input_timeline(machine, copied_input_events,
                                       copied_input_event_count) !=
            GBL_TIME_STATUS_OK) {
        result->status = GBL_RUNNER_REFUSED;
        gbl_runner_set_reason(result, "input-timeline-install-refused");
        gbl_runner_set_refusal(result, "input-timeline-install-refused",
                               "guest-not-run");
        gbl_runner_set_termination(result, "refused-before-run");
        gbl_runner_phase9_observe(result, machine);
        (void)gbl_runner_phase10_capture_video(options, result, machine);
        gbl_machine_dispose(machine);
        result->allocator.release(result->allocator.context, machine,
                                  sizeof *machine);
        result->allocator.release(result->allocator.context, rom_bytes,
                                  GBL_RUNNER_MAX_ROM_BYTES +
                                      (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
        return result->status;
    }

    if (gbl_rom_size(machine) != rom_length ||
        gbl_rom_data(machine) == NULL ||
        memcmp(gbl_rom_data(machine), rom_bytes, rom_length) != 0 ||
        !gbl_sha256_span(gbl_rom_data(machine), gbl_rom_size(machine),
                         copied_digest) ||
        strcmp(result->rom_sha256, copied_digest) != 0) {
        result->status = GBL_RUNNER_HARNESS_FAIL;
        gbl_runner_set_reason(result, "rom-copy-identity-failed");
        gbl_runner_set_termination(result, "not-started");
        gbl_runner_phase9_observe(result, machine);
        (void)gbl_runner_phase10_capture_video(options, result, machine);
        gbl_machine_dispose(machine);
        result->allocator.release(result->allocator.context, machine,
                                  sizeof *machine);
        result->allocator.release(result->allocator.context, rom_bytes,
                                  GBL_RUNNER_MAX_ROM_BYTES +
                                      (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
        return result->status;
    }
    result->ran = true;
    result->status = GBL_RUNNER_TIMEOUT;
    gbl_runner_set_reason(result, "cycle-budget");
    gbl_runner_set_termination(result, "timeout");

    for (;;) {
        gbl_run_options run_options;
        gbl_run_result run_result;
        gbl_trace_buffer trace_buffer;
        uint64_t remaining;
        uint64_t cpu_bus_cycles;
        uint64_t cpu_internal_cycles;
        gbl_run_status run_status;

        result->semantic = gbl_mailbox_read(machine);
        if (!result->register_observation.present &&
            gbl_runner_semantic_terminal(result->semantic)) {
            gbl_runner_classify_terminal(result);
            break;
        }
        if (options->cancelled != NULL &&
            options->cancelled(options->cancel_context)) {
            result->status = GBL_RUNNER_CANCELLED;
            gbl_runner_set_reason(result, "cancelled");
            gbl_runner_set_termination(result, "cancelled");
            break;
        }
        if (result->cycles >= result->budget_cycles) {
            result->status = GBL_RUNNER_TIMEOUT;
            gbl_runner_set_reason(result, "cycle-budget");
            gbl_runner_set_termination(result, "timeout");
            break;
        }
        remaining = result->budget_cycles - result->cycles;
        if (remaining == 0u) {
            result->status = GBL_RUNNER_TIMEOUT;
            gbl_runner_set_reason(result, "cycle-budget");
            gbl_runner_set_termination(result, "timeout");
            break;
        }

        trace_buffer.records = result->trace_records;
        trace_buffer.capacity = result->trace_capacity;
        trace_buffer.count = result->trace_count;
        trace_buffer.next_sequence = (uint64_t)result->trace_count;
        run_options.cycle_budget = remaining;
        run_options.unit_limit = UINT64_C(1);
        run_options.category_mask = result->trace_mask;
        run_options.trace_buffer = result->trace_mask == 0u
                                       ? NULL
                                       : &trace_buffer;
        run_result = (gbl_run_result){ 0 };
        run_status = gbl_cpu_run(machine, &run_options, &run_result);
        if (run_status != GBL_RUN_STATUS_OK ||
            run_result.cycles > remaining ||
            run_result.cycles > UINT64_MAX - result->cycles ||
            run_result.instructions > UINT64_MAX - result->instructions ||
            !gbl_runner_run_accounting(&run_result, &cpu_bus_cycles,
                                       &cpu_internal_cycles) ||
            cpu_bus_cycles > UINT64_MAX - result->bus_cycles ||
            cpu_internal_cycles >
                UINT64_MAX - result->internal_cycles ||
            run_result.wait_cycles > UINT64_MAX - result->wait_cycles ||
            run_result.dma_startup_cycles >
                UINT64_MAX - result->dma_startup_cycles ||
            run_result.dma_source_cycles >
                UINT64_MAX - result->dma_source_cycles ||
            run_result.dma_destination_cycles >
                UINT64_MAX - result->dma_destination_cycles ||
            run_result.dma_completed_units >
                UINT64_MAX - result->dma_completed_units ||
            run_result.dma_pause_cycles >
                UINT64_MAX - result->dma_pause_cycles ||
            (run_result.termination == GBL_RUN_PROGRESS &&
             run_result.cycles == 0u && run_result.instructions == 0u &&
             run_result.wait_cycles == 0u &&
             run_result.power_cause != GBL_CPU_POWER_CAUSE_WAKE_PENDING &&
             run_result.power_cause != GBL_CPU_POWER_CAUSE_WAKE_EVENT)) {
            result->status = GBL_RUNNER_HARNESS_FAIL;
            gbl_runner_set_reason(result, "cpu-run-failed");
            gbl_runner_set_termination(result, "harness-failure");
            break;
        }
        result->cycles += run_result.cycles;
        result->instructions += run_result.instructions;
        result->bus_cycles += cpu_bus_cycles;
        result->internal_cycles += cpu_internal_cycles;
        result->wait_cycles += run_result.wait_cycles;
        result->dma_startup_cycles += run_result.dma_startup_cycles;
        result->dma_source_cycles += run_result.dma_source_cycles;
        result->dma_destination_cycles +=
            run_result.dma_destination_cycles;
        result->dma_completed_units += run_result.dma_completed_units;
        result->dma_pause_cycles += run_result.dma_pause_cycles;
        gbl_runner_phase8_observe(result, machine, &run_result);
        if (run_result.cycles != 0u || run_result.instructions != 0u ||
            run_result.power_cause != GBL_CPU_POWER_CAUSE_NONE) {
            has_committed_unit = true;
        }
        if (result->trace_mask != 0u) {
            result->trace_count = trace_buffer.count;
        }
        result->semantic = gbl_mailbox_read(machine);

        if (run_result.termination == GBL_RUN_TRACE_OUTPUT_FULL) {
            result->status = GBL_RUNNER_HARNESS_FAIL;
            result->trace_complete = false;
            gbl_runner_set_reason(result, "trace-output-full");
            gbl_runner_set_termination(result, "trace-output-full");
            break;
        }
        if (options->cancelled != NULL &&
            options->cancelled(options->cancel_context)) {
            result->status = GBL_RUNNER_CANCELLED;
            gbl_runner_set_reason(result, "cancelled");
            gbl_runner_set_termination(result, "cancelled");
            break;
        }
        if (run_result.termination == GBL_RUN_STOPPED) {
            if (run_result.reason == GBL_RUN_REASON_INVALID_CPU_STATE ||
                run_result.reason == GBL_RUN_REASON_COUNTER_OVERFLOW) {
                result->status = GBL_RUNNER_HARNESS_FAIL;
                gbl_runner_set_reason(result, "cpu-state-failed");
                gbl_runner_set_termination(result, "harness-failure");
            } else if (run_result.reason == GBL_RUN_REASON_TIME_REFUSAL) {
                result->status = GBL_RUNNER_HARNESS_FAIL;
                gbl_runner_set_reason(result, "cpu-time-refused");
                gbl_runner_set_termination(result, "harness-failure");
            } else if (run_result.reason == GBL_RUN_REASON_INTERNAL_ERROR) {
                result->status = GBL_RUNNER_HARNESS_FAIL;
                gbl_runner_set_reason(result, "cpu-internal-error");
                gbl_runner_set_termination(result, "harness-failure");
            } else {
                gbl_runner_classify_guest_stop(result, machine,
                                               run_result.reason);
            }
            break;
        }
        if (run_result.termination != GBL_RUN_PROGRESS &&
            run_result.termination != GBL_RUN_TIMEOUT) {
            result->status = GBL_RUNNER_HARNESS_FAIL;
            gbl_runner_set_reason(result, "cpu-run-termination-invalid");
            gbl_runner_set_termination(result, "harness-failure");
            break;
        }
        if (!result->register_observation.present &&
            gbl_runner_semantic_terminal(result->semantic)) {
            gbl_runner_classify_terminal(result);
            break;
        }
        if (gbl_runner_observe_register(result, machine)) {
            break;
        }
        if (run_result.termination == GBL_RUN_TIMEOUT) {
            result->status = GBL_RUNNER_TIMEOUT;
            gbl_runner_set_reason(result, "cycle-budget");
            gbl_runner_set_termination(result, "timeout");
            break;
        }
    }

    if (result->trace_mask != 0u && result->trace_count > result->trace_capacity) {
        result->status = GBL_RUNNER_HARNESS_FAIL;
        result->trace_complete = false;
        gbl_runner_set_reason(result, "trace-count-invalid");
        gbl_runner_set_termination(result, "harness-failure");
    }
    (void)gbl_runner_write_trace_if_requested(options, result);
    gbl_runner_phase9_observe(result, machine);
    if (!gbl_runner_phase10_capture_video(options, result, machine) &&
        result->ran) {
        gbl_runner_video_output_failed(result);
    }
    if (has_committed_unit) {
        result->has_timer0_if_request = true;
        result->timer0_if_request =
            (machine->interrupt_flags & UINT16_C(0x0008)) != 0u;
    }
    gbl_machine_dispose(machine);
    result->allocator.release(result->allocator.context, machine,
                              sizeof *machine);
    result->allocator.release(result->allocator.context, rom_bytes,
                              GBL_RUNNER_MAX_ROM_BYTES +
                                  (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
    return result->status;
}
