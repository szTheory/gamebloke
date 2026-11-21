#include "run.h"

#include <errno.h>
#include <inttypes.h>
#include <string.h>

static const char *gbl_runner_status_name(gbl_runner_status status)
{
    switch (status) {
    case GBL_RUNNER_PASS: return "PASS";
    case GBL_RUNNER_GUEST_FAIL: return "GUEST_FAIL";
    case GBL_RUNNER_GUEST_STOP: return "GUEST_STOP";
    case GBL_RUNNER_HARNESS_FAIL: return "HARNESS_FAIL";
    case GBL_RUNNER_TIMEOUT: return "TIMEOUT";
    case GBL_RUNNER_CANCELLED: return "CANCELLED";
    case GBL_RUNNER_MISSING: return "MISSING";
    case GBL_RUNNER_SKIPPED: return "SKIPPED";
    case GBL_RUNNER_REFUSED: return "REFUSED";
    }
    return "HARNESS_FAIL";
}

static const char *gbl_runner_mailbox_name(gbl_mailbox_status status)
{
    switch (status) {
    case GBL_MAILBOX_ABSENT: return "absent";
    case GBL_MAILBOX_PENDING: return "pending";
    case GBL_MAILBOX_PASS: return "pass";
    case GBL_MAILBOX_FAIL: return "fail";
    case GBL_MAILBOX_MALFORMED: return "malformed";
    }
    return "malformed";
}

static const char *gbl_runner_exception_name(gbl_exception_kind kind)
{
    switch (kind) {
    case GBL_EXCEPTION_NONE: return "none";
    case GBL_EXCEPTION_RESET: return "reset";
    case GBL_EXCEPTION_UNDEFINED: return "undefined";
    case GBL_EXCEPTION_SWI: return "swi";
    case GBL_EXCEPTION_PREFETCH_ABORT: return "prefetch_abort";
    case GBL_EXCEPTION_DATA_ABORT: return "data_abort";
    case GBL_EXCEPTION_IRQ: return "irq";
    case GBL_EXCEPTION_FIQ: return "fiq";
    case GBL_EXCEPTION_COPROCESSOR: return "coprocessor";
    }
    return NULL;
}

static const char *gbl_runner_power_state_name(gbl_machine_power_state state)
{
    switch (state) {
    case GBL_MACHINE_POWER_RUNNING: return "running";
    case GBL_MACHINE_POWER_HALTED: return "halted";
    case GBL_MACHINE_POWER_STOPPED: return "stopped";
    }
    return NULL;
}

static const char *gbl_runner_power_cause_name(gbl_cpu_power_cause cause)
{
    switch (cause) {
    case GBL_CPU_POWER_CAUSE_NONE: return "none";
    case GBL_CPU_POWER_CAUSE_WAKE_PENDING: return "wake_pending";
    case GBL_CPU_POWER_CAUSE_WAKE_EVENT: return "wake_event";
    case GBL_CPU_POWER_CAUSE_STOP_REQUESTED: return "stop_requested";
    case GBL_CPU_POWER_CAUSE_HALT_DEADLOCK: return "halt_deadlock";
    case GBL_CPU_POWER_CAUSE_WAIT_TIMEOUT: return "wait_timeout";
    }
    return NULL;
}

static const char *gbl_runner_bus_owner_name(gbl_machine_bus_owner owner)
{
    switch (owner) {
    case GBL_MACHINE_BUS_OWNER_CPU: return "cpu";
    case GBL_MACHINE_BUS_OWNER_DMA: return "dma";
    }
    return NULL;
}

static const char *gbl_runner_dma_status_name(gbl_dma_status status)
{
    switch (status) {
    case GBL_DMA_STATUS_NONE: return "none";
    case GBL_DMA_STATUS_OK: return "ok";
    case GBL_DMA_STATUS_IDLE: return "idle";
    case GBL_DMA_STATUS_BUDGET_REFUSAL: return "budget_refusal";
    case GBL_DMA_STATUS_INVALID_ARGUMENT: return "invalid_argument";
    case GBL_DMA_STATUS_UNSUPPORTED: return "unsupported";
    case GBL_DMA_STATUS_TIME_OVERFLOW: return "time_overflow";
    case GBL_DMA_STATUS_EVENT_DISPATCH_LIMIT: return "event_dispatch_limit";
    case GBL_DMA_STATUS_UNIT_REFUSAL: return "unit_refusal";
    }
    return NULL;
}

static const char *gbl_runner_bus_class_name(gbl_bus_access_class access_class)
{
    switch (access_class) {
    case GBL_BUS_CLASS_NONSEQUENTIAL: return "nonsequential";
    case GBL_BUS_CLASS_SEQUENTIAL: return "sequential";
    }
    return NULL;
}

static const char *gbl_runner_dma_provenance_name(
    gbl_dma_value_provenance provenance)
{
    switch (provenance) {
    case GBL_DMA_VALUE_PROVENANCE_NONE: return "none";
    case GBL_DMA_VALUE_PROVENANCE_PROJECT_SELECTED:
        return "project-selected";
    }
    return NULL;
}

static const char *gbl_runner_dma_classification_name(
    gbl_dma_result_classification classification)
{
    switch (classification) {
    case GBL_DMA_RESULT_CLASSIFICATION_NONE: return "none";
    case GBL_DMA_RESULT_CLASSIFICATION_DISPUTED: return "disputed";
    }
    return NULL;
}

static const char *gbl_runner_dma_dispute_name(gbl_dma_dispute_id dispute)
{
    switch (dispute) {
    case GBL_DMA_DISPUTE_NONE: return NULL;
    case GBL_DMA_DISPUTE_LATCH: return "disp-dma-latch";
    case GBL_DMA_DISPUTE_HBLANK_FLAG_EDGE:
        return "disp-hblank-flag-edge";
    }
    return NULL;
}

static const char *gbl_runner_qualification_name(
    gbl_hardware_qualification qualification)
{
    switch (qualification) {
    case GBL_HARDWARE_QUALIFICATION_UNSPECIFIED: return "UNSPECIFIED";
    case GBL_HARDWARE_QUALIFICATION_UNKNOWN: return "UNKNOWN";
    case GBL_HARDWARE_QUALIFICATION_QUALIFIED: return "QUALIFIED";
    }
    return NULL;
}

static bool gbl_runner_json_string(FILE *output, const char *value)
{
    static const char hex[] = "0123456789abcdef";
    const unsigned char *cursor = (const unsigned char *)value;

    if (fputc('"', output) == EOF) {
        return false;
    }
    while (*cursor != 0u) {
        const unsigned char character = *cursor++;

        if (character == (unsigned char)'"' ||
            character == (unsigned char)'\\') {
            if (fputc('\\', output) == EOF ||
                fputc((int)character, output) == EOF) {
                return false;
            }
        } else if (character == (unsigned char)'\b') {
            if (fputs("\\b", output) == EOF) return false;
        } else if (character == (unsigned char)'\f') {
            if (fputs("\\f", output) == EOF) return false;
        } else if (character == (unsigned char)'\n') {
            if (fputs("\\n", output) == EOF) return false;
        } else if (character == (unsigned char)'\r') {
            if (fputs("\\r", output) == EOF) return false;
        } else if (character == (unsigned char)'\t') {
            if (fputs("\\t", output) == EOF) return false;
        } else if (character < UINT8_C(0x20) || character >= UINT8_C(0x80)) {
            if (fputs("\\u00", output) == EOF ||
                fputc(hex[character >> 4u], output) == EOF ||
                fputc(hex[character & UINT8_C(0x0F)], output) == EOF) {
                return false;
            }
        } else if (fputc((int)character, output) == EOF) {
            return false;
        }
    }
    return fputc('"', output) != EOF;
}

typedef struct gbl_runner_capability_dimension {
    unsigned int width;
    unsigned int height;
} gbl_runner_capability_dimension;

typedef struct gbl_runner_capability_background {
    unsigned int bg;
    const char *source;
    const gbl_runner_capability_dimension *dimensions;
    size_t dimension_count;
} gbl_runner_capability_background;

typedef struct gbl_runner_display_mode {
    unsigned int mode;
    const char *implementation_status;
    const gbl_runner_capability_background *backgrounds;
    size_t background_count;
} gbl_runner_display_mode;

static const gbl_runner_capability_dimension gbl_runner_text_dimensions[] = {
    { 256u, 256u }, { 512u, 256u }, { 256u, 512u }, { 512u, 512u }
};
static const gbl_runner_capability_dimension gbl_runner_affine_dimensions[] = {
    { 128u, 128u }, { 256u, 256u }, { 512u, 512u }, { 1024u, 1024u }
};
static const gbl_runner_capability_dimension gbl_runner_bitmap_dimensions[] = {
    { 240u, 160u }
};
static const gbl_runner_capability_dimension gbl_runner_mode5_dimensions[] = {
    { 160u, 128u }
};

#define GBL_RUNNER_CAPABILITY_COUNT(items) \
    (sizeof(items) / sizeof((items)[0]))

static const gbl_runner_capability_background gbl_runner_mode0_backgrounds[] = {
    { 0u, "text", gbl_runner_text_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_text_dimensions) },
    { 1u, "text", gbl_runner_text_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_text_dimensions) },
    { 2u, "text", gbl_runner_text_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_text_dimensions) },
    { 3u, "text", gbl_runner_text_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_text_dimensions) }
};
static const gbl_runner_capability_background gbl_runner_mode1_backgrounds[] = {
    { 0u, "text", gbl_runner_text_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_text_dimensions) },
    { 1u, "text", gbl_runner_text_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_text_dimensions) },
    { 2u, "affine", gbl_runner_affine_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_affine_dimensions) }
};
static const gbl_runner_capability_background gbl_runner_mode2_backgrounds[] = {
    { 2u, "affine", gbl_runner_affine_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_affine_dimensions) },
    { 3u, "affine", gbl_runner_affine_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_affine_dimensions) }
};
static const gbl_runner_capability_background gbl_runner_mode3_backgrounds[] = {
    { 2u, "direct-color-bitmap", gbl_runner_bitmap_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_bitmap_dimensions) }
};
static const gbl_runner_capability_background gbl_runner_mode4_backgrounds[] = {
    { 2u, "indexed-bitmap", gbl_runner_bitmap_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_bitmap_dimensions) }
};
static const gbl_runner_capability_background gbl_runner_mode5_backgrounds[] = {
    { 2u, "direct-color-bitmap", gbl_runner_mode5_dimensions,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_mode5_dimensions) }
};

static const gbl_runner_display_mode gbl_runner_display_modes[] = {
    { 0u, "partial", gbl_runner_mode0_backgrounds,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_mode0_backgrounds) },
    { 1u, "partial", gbl_runner_mode1_backgrounds,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_mode1_backgrounds) },
    { 2u, "partial", gbl_runner_mode2_backgrounds,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_mode2_backgrounds) },
    { 3u, "partial", gbl_runner_mode3_backgrounds,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_mode3_backgrounds) },
    { 4u, "partial", gbl_runner_mode4_backgrounds,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_mode4_backgrounds) },
    { 5u, "partial", gbl_runner_mode5_backgrounds,
      GBL_RUNNER_CAPABILITY_COUNT(gbl_runner_mode5_backgrounds) }
};

#undef GBL_RUNNER_CAPABILITY_COUNT

static bool gbl_runner_write_phase10_capabilities(FILE *output)
{
    size_t mode_index;

    if (fputs(",\"display_capabilities\":{"
              "\"model\":\"gamebloke-display-capabilities-1\","
              "\"raw_output\":{\"format\":\"BGR555_LE\","
              "\"width\":240,\"height\":160,\"stride_bytes\":480,"
              "\"byte_length\":76800},\"modes\":[",
              output) == EOF) {
        return false;
    }
    for (mode_index = 0u;
         mode_index < sizeof gbl_runner_display_modes /
                          sizeof gbl_runner_display_modes[0];
         ++mode_index) {
        const gbl_runner_display_mode *mode =
            &gbl_runner_display_modes[mode_index];
        size_t background_index;

        if (fprintf(output, "%s{\"mode\":%u,\"implementation_status\":",
                    mode_index == 0u ? "" : ",", mode->mode) < 0 ||
            !gbl_runner_json_string(output, mode->implementation_status) ||
            fputs(",\"backgrounds\":[", output) == EOF) {
            return false;
        }
        for (background_index = 0u;
             background_index < mode->background_count;
             ++background_index) {
            const gbl_runner_capability_background *background =
                &mode->backgrounds[background_index];
            size_t dimension_index;

            if (fprintf(output, "%s{\"bg\":%u,\"source\":",
                        background_index == 0u ? "" : ",",
                        background->bg) < 0 ||
                !gbl_runner_json_string(output, background->source) ||
                fputs(",\"dimensions\":[", output) == EOF) {
                return false;
            }
            for (dimension_index = 0u;
                 dimension_index < background->dimension_count;
                 ++dimension_index) {
                const gbl_runner_capability_dimension *dimension =
                    &background->dimensions[dimension_index];

                if (fprintf(output,
                            "%s{\"width\":%u,\"height\":%u}",
                            dimension_index == 0u ? "" : ",",
                            dimension->width, dimension->height) < 0) {
                    return false;
                }
            }
            if (fputs("]}", output) == EOF) {
                return false;
            }
        }
        if (fputs("]}", output) == EOF) {
            return false;
        }
    }
    return fputs("]},\"video_memory_access\":{"
                 "\"classification\":\"project-selected\","
                 "\"hardware_qualification\":\"UNKNOWN\","
                 "\"masters\":[\"cpu\",\"dma\"],"
                 "\"regions\":[\"pram\",\"vram\",\"oam\"],"
                 "\"states\":[\"drawing\",\"hblank\",\"vblank\","
                 "\"forced-blank\"],"
                 "\"availability\":\"existing-bus-behavior\","
                 "\"ppu_extra_cycles\":0,"
                 "\"cost_reference\":\"bus_policy.request_cycles\"}}",
                 output) != EOF;
}

static bool gbl_runner_trace_record_write(FILE *output,
                                          const gbl_trace_record *record)
{
    const char *dma_status = gbl_runner_dma_status_name(record->dma_status);
    const char *owner = gbl_runner_bus_owner_name(record->bus_owner);
    const char *source_sequence =
        gbl_runner_bus_class_name(record->dma_source_sequence);
    const char *destination_sequence =
        gbl_runner_bus_class_name(record->dma_destination_sequence);
    const char *provenance =
        gbl_runner_dma_provenance_name(record->dma_value_provenance);
    const char *classification =
        gbl_runner_dma_classification_name(
            record->dma_result_classification);
    const char *dispute = gbl_runner_dma_dispute_name(record->dma_dispute);
    const char *qualification = gbl_runner_qualification_name(
        record->dma_hardware_qualification);
    const int written = fprintf(
        output,
        "{\"sequence\":\"%" PRIu64 "\",\"guest_cycle\":\"%" PRIu64
        "\",\"category\":%u,\"instruction_address\":\"0x%08" PRIx32
        "\",\"opcode\":\"0x%08" PRIx32 "\",\"address\":\"0x%08" PRIx32
        "\",\"value\":\"0x%08" PRIx32 "\",\"width\":%u,"
        "\"access_class\":%u,\"bus_outcome\":%u,\"exception_kind\":%u",
        record->sequence, record->guest_cycle, (unsigned int)record->category,
        record->instruction_address, record->opcode, record->address,
        record->value, (unsigned int)record->width,
        (unsigned int)record->access_class, (unsigned int)record->bus_outcome,
        (unsigned int)record->exception_kind);

    if (written < 0) {
        return false;
    }
    if (record->category == GBL_TRACE_DMA) {
        if (owner == NULL || dma_status == NULL || source_sequence == NULL ||
            destination_sequence == NULL || provenance == NULL ||
            classification == NULL || qualification == NULL ||
            record->dma_channel >= GBL_DMA_CHANNEL_COUNT ||
            fprintf(output,
                    ",\"dma\":{\"owner\":\"%s\",\"channel\":%u,"
                    "\"status\":\"%s\",\"source_address\":\"0x%08" PRIX32
                    "\",\"destination_address\":\"0x%08" PRIX32
                    "\",\"value\":\"0x%08" PRIX32
                    "\",\"width\":%u,\"startup_cycles\":\"%" PRIu64
                    "\",\"source_cycles\":\"%" PRIu64
                    "\",\"destination_cycles\":\"%" PRIu64
                    "\",\"source_sequence\":\"%s\","
                    "\"destination_sequence\":\"%s\","
                    "\"remaining\":%" PRIu32 ",\"completed\":\"%" PRIu64
                    "\",\"used_channel_latch\":%s,"
                    "\"value_provenance\":\"%s\","
                    "\"classification\":\"%s\",\"dispute_id\":",
                    owner, (unsigned int)record->dma_channel, dma_status,
                    record->dma_source_address,
                    record->dma_destination_address, record->dma_value,
                    (unsigned int)record->dma_width,
                    record->dma_startup_cycles, record->dma_source_cycles,
                    record->dma_destination_cycles, source_sequence,
                    destination_sequence, record->dma_remaining,
                    record->dma_completed,
                    record->dma_used_channel_latch ? "true" : "false",
                    provenance, classification) < 0) {
            return false;
        }
        if (dispute == NULL) {
            if (fputs("null", output) == EOF) return false;
        } else if (!gbl_runner_json_string(output, dispute)) {
            return false;
        }
        if (fputs(",\"hardware_qualification\":", output) == EOF ||
            !gbl_runner_json_string(output, qualification) ||
            fputc('}', output) == EOF) {
            return false;
        }
    }
    return fputs("}\n", output) != EOF;
}

bool gbl_runner_write_trace(const char *path,
                            const gbl_runner_result *result)
{
    FILE *output;
    size_t index;
    bool okay = true;

    if (path == NULL || path[0] == '\0' || result == NULL ||
        result->trace_count > result->trace_capacity ||
        (result->trace_count != 0u && result->trace_records == NULL)) {
        return false;
    }
    output = fopen(path, "wbx");
    if (output == NULL) {
        return false;
    }
    for (index = 0u; index < result->trace_count; ++index) {
        if (!gbl_runner_trace_record_write(output,
                                           &result->trace_records[index])) {
            okay = false;
            break;
        }
    }
    if (fclose(output) != 0) {
        okay = false;
    }
    return okay;
}

static void gbl_runner_set_video_error(gbl_runner_result *result,
                                      const char *error)
{
    const size_t length = strlen(error);

    if (length >= sizeof result->phase10.video_output_error) {
        return;
    }
    memcpy(result->phase10.video_output_error, error, length + 1u);
}

bool gbl_runner_write_video(const char *path, gbl_runner_result *result)
{
    FILE *output;
    bool write_okay;
    bool close_okay;
    const char *failure;
    const uint64_t completion_offset =
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES;
    uint64_t expected_sample_cycle;

    if (result == NULL) {
        return false;
    }
    result->phase10.video_emitted = false;
    result->phase10.has_video_sha256 = false;
    result->phase10.video_output_error[0] = '\0';
    if (path == NULL) {
        gbl_runner_set_video_error(result, "video-output-path-null");
        return false;
    }
    if (path[0] == '\0') {
        gbl_runner_set_video_error(result, "video-output-path-empty");
        return false;
    }
    if (!result->phase10.video_requested || !result->phase10.observed ||
        !result->phase10.frame_started ||
        !result->phase10.has_frame_epoch ||
        result->phase10.capture_state != GBL_VIDEO_CAPTURE_VALID ||
        result->phase10.capture_reason != GBL_VIDEO_INVALID_REASON_NONE ||
        !result->phase10.supported_controls ||
        !result->phase10.has_sample_cycle ||
        !result->phase10.has_completion_cycle ||
        result->phase10.frame_epoch > UINT64_MAX / GBL_PPU_FRAME_CYCLES) {
        gbl_runner_set_video_error(result, "video-frame-unavailable");
        return false;
    }
    expected_sample_cycle = result->phase10.frame_epoch *
                            GBL_PPU_FRAME_CYCLES;
    if (result->phase10.sample_cycle != expected_sample_cycle ||
        result->phase10.completion_cycle < result->phase10.sample_cycle ||
        result->phase10.completion_cycle - result->phase10.sample_cycle !=
            completion_offset ||
        result->phase10.machine_cycles < result->phase10.completion_cycle) {
        gbl_runner_set_video_error(result, "video-frame-unavailable");
        return false;
    }
    if (result->phase10.video_bytes == NULL ||
        result->phase10.video_length != GBL_RUNNER_VIDEO_BYTES) {
        gbl_runner_set_video_error(result, "video-frame-invalid");
        return false;
    }
    if (!gbl_sha256_span(result->phase10.video_bytes,
                         result->phase10.video_length,
                         result->phase10.video_sha256)) {
        result->phase10.has_video_sha256 = false;
        gbl_runner_set_video_error(result, "video-digest-failed");
        return false;
    }
    result->phase10.has_video_sha256 = true;

    errno = 0;
    output = fopen(path, "wbx");
    if (output == NULL) {
        gbl_runner_set_video_error(
            result, errno == EEXIST ? "video-output-already-exists"
                                    : "video-output-open-failed");
        return false;
    }
    write_okay = fwrite(result->phase10.video_bytes, 1u,
                        result->phase10.video_length, output) ==
                     result->phase10.video_length &&
                 ferror(output) == 0;
    if (write_okay && fflush(output) != 0) {
        write_okay = false;
    }
    close_okay = fclose(output) == 0;
    if (!write_okay || !close_okay) {
        failure = write_okay ? "video-output-close-failed"
                             : "video-output-write-failed";
        if (remove(path) != 0) {
            gbl_runner_set_video_error(result,
                                       "video-output-cleanup-failed");
        } else {
            gbl_runner_set_video_error(result, failure);
        }
        return false;
    }
    result->phase10.video_emitted = true;
    return true;
}

static bool gbl_runner_write_evidence(FILE *output,
                                     const gbl_runner_result *result)
{
    if (fputs("[", output) == EOF) {
        return false;
    }
    if (result->metadata != NULL) {
        if (fputs("{\"fixture\":", output) == EOF ||
            !gbl_runner_json_string(output, result->metadata->id) ||
            fputs(",\"provenance_class\":", output) == EOF ||
            !gbl_runner_json_string(output,
                                    result->metadata->provenance_class) ||
            fputs("}", output) == EOF) {
            return false;
        }
    }
    return fputs("]", output) != EOF;
}

static bool gbl_runner_write_limits(FILE *output,
                                    const gbl_runner_result *result)
{
    static const char *const base_limits[] = {
        "profile version 6 direct-start defaults are project policy, not a BIOS handoff",
        "ARM7TDMI ARMv4T instruction families implemented with explicit project policies; full original diagnostics pending",
        "bus costs and Timer 0 behavior use project-selected model policy, not hardware-qualified timing",
        "Timer 0 IF is a device request; CPU IRQ samples and entry are project-model observations, while BIOS handlers and exact GBA IRQ latency are unqualified",
        "BIOS provider none; DMA is limited to the selected immediate model, while PPU/display, audio, physical input, and host presentation are not modeled",
        "WAITCNT Game Pak rules are manual-backed; other region costs are project-selected, the 128 KiB boundary is community-sourced, and hardware qualification is UNKNOWN",
        "Immediate DMA is modeled; PPU/audio trigger producers and Prefetch are unmodeled, while startup conversion and retained-channel-latch behavior remain project-selected",
        "result does not establish hardware accuracy or broad game compatibility",
        "SHA-256 establishes byte identity only, not permission or redistribution rights"
    };
    size_t index;
    bool needs_comma = false;

    if (fprintf(output, "[\"%s\"", base_limits[0]) < 0) {
        return false;
    }
    needs_comma = true;
    for (index = 1u; index < sizeof base_limits / sizeof base_limits[0];
         ++index) {
        if (needs_comma && fputc(',', output) == EOF) return false;
        if (!gbl_runner_json_string(output, base_limits[index])) return false;
        needs_comma = true;
    }
    if (result->metadata == NULL) {
        if (needs_comma && fputc(',', output) == EOF) return false;
        if (!gbl_runner_json_string(
                output,
                "diagnostic-unregistered input is never manifest-validated evidence PASS")) {
            return false;
        }
        needs_comma = true;
    } else if (result->metadata->dispute_count != 0u) {
        if (needs_comma && fputc(',', output) == EOF) return false;
        if (!gbl_runner_json_string(
                output,
                "fixture metadata is disputed; listed dispute ids disclose the unresolved source claim")) {
            return false;
        }
        needs_comma = true;
    }
    if (result->trace_mask != 0u) {
        if (needs_comma && fputc(',', output) == EOF) return false;
        if (!gbl_runner_json_string(
                output,
                "trace covers selected implemented categories and is incomplete when output capacity is exhausted")) {
            return false;
        }
    }
    return fputc(']', output) != EOF;
}

static bool gbl_runner_write_phase8(FILE *output,
                                    const gbl_runner_phase8_observation *obs)
{
    const char *exception = obs->observed
                                ? gbl_runner_exception_name(obs->last_exception)
                                : NULL;
    const char *power_state = obs->observed
                                  ? gbl_runner_power_state_name(obs->power_state)
                                  : NULL;
    const char *power_cause = obs->observed
                                  ? gbl_runner_power_cause_name(obs->power_cause)
                                  : NULL;

    if (obs->observed &&
        (exception == NULL || power_state == NULL || power_cause == NULL)) {
        return false;
    }
    if (fputs("{\"model\":\"gamebloke-irq-input-model-1\","
              "\"qualification\":\"project-model-only\",\"observed\":",
              output) == EOF ||
        fputs(obs->observed ? "true" : "false", output) == EOF ||
        fputs(",\"irq\":{\"if\":", output) == EOF) {
        return false;
    }
    if (obs->observed) {
        if (fprintf(output, "%u,\"ie\":%u,\"ime\":%u,\"sample\":",
                    (unsigned int)obs->interrupt_flags,
                    (unsigned int)obs->interrupt_enable,
                    (unsigned int)obs->interrupt_master_enable) < 0) {
            return false;
        }
    } else if (fputs("null,\"ie\":null,\"ime\":null,\"sample\":",
                     output) == EOF) {
        return false;
    }
    if (obs->observed && obs->has_irq_sample) {
        const gbl_cpu_irq_observation *sample = &obs->irq_sample;

        if (fprintf(output,
                    "{\"cycle\":\"%" PRIu64 "\",\"if\":%u,"
                    "\"ie\":%u,\"ime\":%u,\"eligible_sources\":%u,"
                    "\"eligible\":%s,\"cpsr_i_masked\":%s,"
                    "\"accepted\":%s,\"entered\":%s}",
                    sample->sample_cycle,
                    (unsigned int)sample->interrupt_flags,
                    (unsigned int)sample->interrupt_enable,
                    (unsigned int)sample->interrupt_master_enable,
                    (unsigned int)sample->eligible_sources,
                    sample->eligible ? "true" : "false",
                    sample->cpu_masked ? "true" : "false",
                    sample->accepted ? "true" : "false",
                    sample->entered ? "true" : "false") < 0) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs("},\"last_exception\":", output) == EOF) {
        return false;
    }
    if (obs->observed) {
        if (!gbl_runner_json_string(output, exception)) return false;
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"power\":{\"state\":", output) == EOF) {
        return false;
    }
    if (obs->observed) {
        if (!gbl_runner_json_string(output, power_state) ||
            fputs(",\"cause\":", output) == EOF ||
            !gbl_runner_json_string(output, power_cause)) {
            return false;
        }
    } else if (fputs("null,\"cause\":null", output) == EOF) {
        return false;
    }
    if (fputs(",\"wake_source_mask\":", output) == EOF) return false;
    if (obs->observed && obs->has_wake) {
        if (fprintf(output, "%u,\"wake_cycle\":\"%" PRIu64 "\"",
                    (unsigned int)obs->wake_source_mask,
                    obs->wake_cycle) < 0) {
            return false;
        }
    } else if (fputs("null,\"wake_cycle\":null", output) == EOF) {
        return false;
    }
    if (fputs("},\"keypad\":{\"keyinput\":", output) == EOF) {
        return false;
    }
    if (obs->observed) {
        if (fprintf(output, "%u,\"keycnt\":%u,\"condition\":%s,"
                            "\"irq_condition\":%s,\"applied_events\":%zu,"
                            "\"pending_events\":%zu},\"wait_cycles\":\"%"
                            PRIu64 "\"}",
                    (unsigned int)obs->keyinput,
                    (unsigned int)obs->key_control,
                    obs->key_condition ? "true" : "false",
                    obs->key_irq_condition ? "true" : "false",
                    obs->applied_input_events, obs->pending_input_events,
                    obs->wait_cycles) < 0) {
            return false;
        }
    } else if (fputs("null,\"keycnt\":null,\"condition\":null,"
                     "\"irq_condition\":null,\"applied_events\":null,"
                     "\"pending_events\":null},\"wait_cycles\":null}",
                     output) == EOF) {
        return false;
    }
    return true;
}

static bool gbl_runner_write_phase9(
    FILE *output, const gbl_runner_result *result)
{
    const gbl_runner_phase9_observation *obs = &result->phase9;
    size_t index;

    if (fputs("{\"model\":\"gamebloke-dma-observation-1\",\"observed\":",
              output) == EOF ||
        fputs(obs->observed ? "true" : "false", output) == EOF ||
        fputs(",\"waitcnt\":", output) == EOF) {
        return false;
    }
    if (obs->observed) {
        if (fprintf(output, "%u", (unsigned int)obs->waitcnt) < 0) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"bus_owner\":", output) == EOF) return false;
    if (obs->observed) {
        const char *owner = gbl_runner_bus_owner_name(obs->bus_owner);

        if (owner == NULL || !gbl_runner_json_string(output, owner)) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"cpu_paused\":", output) == EOF) return false;
    if (obs->observed) {
        if (fputs(obs->bus_owner == GBL_MACHINE_BUS_OWNER_DMA ? "true"
                                                               : "false",
                  output) == EOF ||
            fprintf(output, ",\"machine_cycles\":\"%" PRIu64 "\"",
                    obs->machine_cycles) < 0) {
            return false;
        }
    } else if (fputs("null,\"machine_cycles\":null", output) == EOF) {
        return false;
    }
    if (fputs(",\"channels\":", output) == EOF) return false;
    if (obs->observed) {
        if (fputc('[', output) == EOF) return false;
        for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
            const gbl_dma_channel_state *channel = &obs->channels[index];

            if ((index != 0u && fputc(',', output) == EOF) ||
                fprintf(output,
                        "{\"channel\":%zu,\"source_address\":\"0x%08" PRIX32
                        "\",\"destination_address\":\"0x%08" PRIX32
                        "\",\"current_source\":\"0x%08" PRIX32
                        "\",\"current_destination\":\"0x%08" PRIX32
                        "\",\"read_latch\":\"0x%08" PRIX32
                        "\",\"count\":%u,\"control\":%u,"
                        "\"remaining\":%" PRIu32
                        ",\"completed_units\":\"%" PRIu64
                        "\",\"pending\":%s,\"armed\":%s,"
                        "\"active\":%s,\"startup_complete\":%s}",
                        index, channel->source_address,
                        channel->destination_address, channel->current_source,
                        channel->current_destination, channel->read_latch,
                        (unsigned int)channel->count,
                        (unsigned int)channel->control, channel->remaining,
                        channel->completed_units,
                        channel->pending ? "true" : "false",
                        channel->armed ? "true" : "false",
                        channel->active ? "true" : "false",
                        channel->startup_complete ? "true" : "false") < 0) {
                return false;
            }
        }
        if (fputc(']', output) == EOF) return false;
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fprintf(output,
                ",\"timing\":{\"dma_startup_cycles\":\"%" PRIu64
                "\",\"dma_source_cycles\":\"%" PRIu64
                "\",\"dma_destination_cycles\":\"%" PRIu64
                "\",\"dma_completed_units\":\"%" PRIu64
                "\",\"dma_pause_cycles\":\"%" PRIu64 "\"},"
                "\"waitcnt_policy\":{\"manual_waits\":"
                "\"nintendo-agb-programming-manual-v1.1-sections-3.3-3.3.2\","
                "\"other_region_costs\":\"project-selected\","
                "\"128k_boundary_source\":\"gbatek-3.06-community-documentation\","
                "\"hardware_qualification\":\"UNKNOWN\"},"
                "\"startup_policy\":{\"cycles_per_request\":\"2\","
                "\"provenance\":\"project-selected\","
                "\"manual_activation_delay\":\"described; conversion is selected\","
                "\"hardware_qualification\":\"UNKNOWN\"},"
                "\"latch_policy\":{\"policy\":\"per-channel-retained-latch\","
                "\"provenance\":\"project-selected\","
                "\"fallback_observed\":",
                result->dma_startup_cycles, result->dma_source_cycles,
                result->dma_destination_cycles, result->dma_completed_units,
                result->dma_pause_cycles) < 0) {
        return false;
    }
    if (obs->latch_fallback_known) {
        if (fputs(obs->latch_fallback_observed ? "true" : "false",
                  output) == EOF) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"classification\":", output) == EOF) return false;
    if (obs->latch_fallback_known && obs->latch_fallback_observed) {
        if (fputs("\"disputed\",\"dispute_id\":\"disp-dma-latch\","
                  "\"hardware_qualification\":\"UNKNOWN\"},"
                  "\"prefetch\":\"unmodeled\"}",
                  output) == EOF) {
            return false;
        }
    } else if (fputs("null,\"dispute_id\":null,"
                     "\"hardware_qualification\":\"UNKNOWN\"},"
                     "\"prefetch\":\"unmodeled\"}",
                     output) == EOF) {
        return false;
    }
    return true;
}

static const char *gbl_runner_video_state_name(gbl_video_capture_state state)
{
    switch (state) {
    case GBL_VIDEO_CAPTURE_INVALID: return "invalid";
    case GBL_VIDEO_CAPTURE_INCOMPLETE: return "incomplete";
    case GBL_VIDEO_CAPTURE_VALID: return "valid";
    }
    return "invalid";
}

static const char *gbl_runner_video_reason_name(gbl_video_invalid_reason reason)
{
    switch (reason) {
    case GBL_VIDEO_INVALID_REASON_NONE: return "none";
    case GBL_VIDEO_INVALID_REASON_INCOMPLETE: return "incomplete";
    case GBL_VIDEO_INVALID_REASON_UNSUPPORTED_MODE: return "unsupported-mode";
    case GBL_VIDEO_INVALID_REASON_BG2_DISABLED: return "bg2-disabled";
    case GBL_VIDEO_INVALID_REASON_FORCED_BLANK: return "forced-blank";
    case GBL_VIDEO_INVALID_REASON_OBJ_ENABLED:
        return "unsupported-obj-control";
    case GBL_VIDEO_INVALID_REASON_WINDOW_ENABLED: return "window-enabled";
    case GBL_VIDEO_INVALID_REASON_EFFECT_ENABLED: return "effect-enabled";
    case GBL_VIDEO_INVALID_REASON_BG2_TRANSFORM_UNMODELED:
        return "bg2-transform-unmodeled";
    }
    return "unknown";
}

static bool gbl_runner_write_phase10(FILE *output,
                                     const gbl_runner_result *result)
{
    const gbl_runner_phase10_observation *obs = &result->phase10;

    if (fprintf(output,
                "{\"model\":\"gamebloke-ppu-observation-2\","
                "\"profile_version\":%u,\"observed\":",
                (unsigned int)obs->profile_version) < 0 ||
        fputs(obs->observed ? "true" : "false", output) == EOF) {
        return false;
    }
    if (!obs->observed) {
        if (fputs(",\"raster\":null,\"frame\":null,\"controls\":null,"
                  "\"timing\":{\"model\":\"gamebloke-ppu-model-1\","
                  "\"hblank_offset_cycles\":\"1006\","
                  "\"policy\":\"project-selected\","
                  "\"evidence_class\":\"REF\","
                  "\"classification\":\"disputed\","
                  "\"dispute_id\":\"disp-hblank-flag-edge\","
                  "\"hardware_qualification\":\"UNKNOWN\"},"
                  "\"hblank_trigger_dependency\":null",
                  output) == EOF) {
            return false;
        }
        return gbl_runner_write_phase10_capabilities(output);
    }
    if (fprintf(output,
                ",\"raster\":{\"line\":%u,\"line_cycle\":\"%" PRIu64
                "\",\"machine_cycle\":\"%" PRIu64 "\",\"frame_epoch\":",
                (unsigned int)obs->line, obs->line_cycle,
                obs->machine_cycles) < 0) {
        return false;
    }
    if (obs->has_frame_epoch) {
        if (fprintf(output, "\"%" PRIu64 "\"", obs->frame_epoch) < 0) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fprintf(output,
                "},\"frame\":{\"state\":\"%s\",\"reason\":\"%s\","
                "\"supported_controls\":%s,\"epoch\":",
                gbl_runner_video_state_name(obs->capture_state),
                gbl_runner_video_reason_name(obs->capture_reason),
                obs->supported_controls ? "true" : "false") < 0) {
        return false;
    }
    if (obs->capture_state == GBL_VIDEO_CAPTURE_VALID &&
        obs->has_frame_epoch) {
        if (fprintf(output, "\"%" PRIu64 "\"", obs->frame_epoch) < 0) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"sample_cycle\":", output) == EOF) return false;
    if (obs->has_sample_cycle) {
        if (fprintf(output, "\"%" PRIu64 "\"", obs->sample_cycle) < 0) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"completion_cycle\":", output) == EOF) return false;
    if (obs->has_completion_cycle) {
        if (fprintf(output, "\"%" PRIu64 "\"", obs->completion_cycle) < 0) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"width\":240,\"height\":160,\"stride_bytes\":480,"
              "\"format\":\"bgr555-le\",\"byte_length\":76800,"
              "\"source\":\"canonical-vram\","
              "\"latch_policy\":\"row-start\"},\"controls\":{"
              "\"supported\":",
              output) == EOF ||
        fputs(obs->supported_controls ? "true" : "false", output) == EOF ||
        fputs("},\"timing\":{\"model\":\"gamebloke-ppu-model-1\","
              "\"hblank_offset_cycles\":\"1006\","
              "\"policy\":\"project-selected\","
              "\"evidence_class\":\"REF\","
              "\"evidence\":\"GBATEK direct reference candidate; exact hardware edge unresolved\","
              "\"classification\":\"disputed\","
              "\"dispute_id\":\"disp-hblank-flag-edge\","
              "\"hardware_qualification\":\"UNKNOWN\"},"
              "\"hblank_trigger_dependency\":{\"observed\":",
              output) == EOF ||
        fputs(obs->hblank_trigger_dependency ? "true" : "false", output) == EOF) {
        return false;
    }
    if (obs->hblank_trigger_dependency) {
        if (fputs(",\"classification\":\"disputed\","
                  "\"dispute_id\":\"disp-hblank-flag-edge\","
                  "\"hardware_qualification\":\"UNKNOWN\"}",
                  output) == EOF) {
            return false;
        }
    } else if (fputs(",\"classification\":null,\"dispute_id\":null,"
                     "\"hardware_qualification\":\"UNKNOWN\"}",
                     output) == EOF) {
        return false;
    }
    return gbl_runner_write_phase10_capabilities(output);
}

static bool gbl_runner_write_raw_video(FILE *output,
                                       const gbl_runner_result *result)
{
    const gbl_runner_phase10_observation *obs = &result->phase10;
    const char *status = !obs->video_requested
                             ? "not-requested"
                             : (obs->video_output_error[0] != '\0'
                                    ? "failed"
                                    : (obs->video_emitted
                                           ? "emitted"
                                           : (obs->capture_state ==
                                                      GBL_VIDEO_CAPTURE_VALID
                                                  ? "not-emitted"
                                                  : gbl_runner_video_state_name(
                                                        obs->capture_state))));

    if (fputs("{\"requested\":", output) == EOF ||
        fputs(obs->video_requested ? "true" : "false", output) == EOF ||
        fputs(",\"status\":", output) == EOF ||
        !gbl_runner_json_string(output, status) ||
        fputs(",\"emitted\":", output) == EOF ||
        fputs(obs->video_emitted ? "true" : "false", output) == EOF ||
        fputs(",\"byte_length\":", output) == EOF) {
        return false;
    }
    if (obs->video_emitted) {
        if (fprintf(output, "%zu", obs->video_length) < 0) return false;
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"sha256\":", output) == EOF) return false;
    if (obs->has_video_sha256) {
        if (!gbl_runner_json_string(output, obs->video_sha256)) return false;
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"failure\":", output) == EOF) return false;
    if (obs->video_output_error[0] != '\0') {
        if (!gbl_runner_json_string(output, obs->video_output_error)) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    return fputs(",\"format\":\"bgr555-le\"}", output) != EOF;
}

bool gbl_runner_write_json(FILE *output, const gbl_runner_result *result)
{
    const char *reason;
    size_t index;
    bool latch_dispute;
    bool metadata_has_latch_dispute = false;

    if (output == NULL || result == NULL) {
        return false;
    }
    latch_dispute = result->phase9.latch_fallback_known &&
                    result->phase9.latch_fallback_observed;
    if (result->metadata != NULL) {
        for (index = 0u; index < result->metadata->dispute_count; ++index) {
            if (strcmp(result->metadata->dispute_ids[index],
                       "disp-dma-latch") == 0) {
                metadata_has_latch_dispute = true;
                break;
            }
        }
    }
    reason = result->reason[0] == '\0' ? NULL : result->reason;
    if (fputs("{\"schema\":\"gamebloke.result/1\",\"fixture\":{\"id\":",
              output) == EOF) {
        return false;
    }
    if (result->has_fixture_id) {
        if (!gbl_runner_json_string(output, result->fixture_id)) return false;
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fprintf(output, ",\"ran\":%s,\"rom_sha256\":",
                result->ran ? "true" : "false") < 0) {
        return false;
    }
    if (result->has_rom_sha256) {
        if (!gbl_runner_json_string(output, result->rom_sha256)) return false;
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs("},\"status\":", output) == EOF ||
        !gbl_runner_json_string(output,
                                gbl_runner_status_name(result->status)) ||
        fputs(",\"reason\":", output) == EOF) {
        return false;
    }
    if (reason != NULL) {
        if (!gbl_runner_json_string(output, reason)) return false;
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"refusal\":", output) == EOF) return false;
    if (result->has_refusal) {
        if (fputs("{\"code\":", output) == EOF ||
            !gbl_runner_json_string(output, result->refusal_code) ||
            fputs(",\"postcondition\":", output) == EOF ||
            !gbl_runner_json_string(output,
                                    result->refusal_postcondition) ||
            fputc('}', output) == EOF) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs(",\"claim\":{\"machine\":\"agb\",\"boot\":\"direct-start\","
              "\"bios_provider\":\"none\",\"cart\":{\"kind\":\"rom-only\","
              "\"rom_sha256\":",
              output) == EOF) {
        return false;
    }
    if (result->has_rom_sha256) {
        if (!gbl_runner_json_string(output, result->rom_sha256)) return false;
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs("},\"input_timeline\":", output) == EOF) {
        return false;
    }
    if (result->input_timeline_present) {
        if (!result->has_input_timeline_sha256 ||
            fputs("{\"kind\":\"script\",\"sha256\":", output) == EOF ||
            !gbl_runner_json_string(output,
                                    result->input_timeline_sha256) ||
            fputc('}', output) == EOF) {
            return false;
        }
    } else if (fputs("{\"kind\":\"none\"}", output) == EOF) {
        return false;
    }
    if (fputs(",\"evidence\":", output) == EOF ||
        !gbl_runner_write_evidence(output, result) ||
        fputs("},\"disputes\":[", output) == EOF) {
        return false;
    }
    if (result->metadata != NULL) {
        for (index = 0u; index < result->metadata->dispute_count; ++index) {
            if (index != 0u && fputc(',', output) == EOF) return false;
            if (!gbl_runner_json_string(output,
                                        result->metadata->dispute_ids[index])) {
                return false;
            }
        }
    }
    if (latch_dispute && !metadata_has_latch_dispute) {
        if ((result->metadata != NULL &&
             result->metadata->dispute_count != 0u &&
             fputc(',', output) == EOF) ||
            !gbl_runner_json_string(output, "disp-dma-latch")) {
            return false;
        }
    }
    if (fputs("],\"labels\":", output) == EOF) return false;
    if ((result->metadata != NULL &&
         result->metadata->dispute_count != 0u) || latch_dispute) {
        if (fputs("[\"disputed\"]", output) == EOF) return false;
    } else if (fputs("[]", output) == EOF) {
        return false;
    }
    if (fputs(",\"limits\":", output) == EOF ||
        !gbl_runner_write_limits(output, result) ||
        ferror(output) != 0) {
        return false;
    }
    if (result->register_observation.present) {
        const gbl_runner_register_observation *observation =
            &result->register_observation;

        if (fputs(",\"observation\":{\"kind\":\"register-eval\","
                  "\"observed\":",
                  output) == EOF ||
            fputs(observation->observed ? "true" : "false", output) == EOF ||
            fputs(",\"source_commit\":", output) == EOF ||
            !gbl_runner_json_string(output, observation->source_commit) ||
            fprintf(output, ",\"entry_address\":\"0x%08" PRIX32
                            "\",\"instruction_set\":",
                    observation->entry_address) < 0 ||
            !gbl_runner_json_string(output, observation->instruction_set) ||
            fprintf(output, ",\"register_index\":%u,\"value\":",
                    (unsigned int)observation->register_index) < 0) {
            return false;
        }
        if (observation->has_value) {
            if (fprintf(output, "%" PRIu32, observation->value) < 0) {
                return false;
            }
        } else if (fputs("null", output) == EOF) {
            return false;
        }
        if (fputc('}', output) == EOF) {
            return false;
        }
    }
    if (fputs(",\"semantic\":{\"state\":", output) == EOF ||
        !gbl_runner_json_string(
            output, gbl_runner_mailbox_name(result->semantic.status)) ||
        fprintf(output, ",\"case_id\":\"%" PRIu32 "\"},\"time\":{"
                       "\"cycles\":\"%" PRIu64 "\",\"instructions\":\"%" PRIu64
                       "\",\"budget_cycles\":\"%" PRIu64 "\",\"model\":",
                result->semantic.case_id, result->cycles,
                result->instructions, result->budget_cycles) < 0 ||
        !gbl_runner_json_string(output, GBL_TIMER_MODEL_NAME) ||
        fputs(",\"qualification\":", output) == EOF ||
        !gbl_runner_json_string(output, "project-model-only") ||
        fprintf(output, ",\"bus_cycles\":\"%" PRIu64
                       "\",\"internal_cycles\":\"%" PRIu64
                       "\",\"wait_cycles\":\"%" PRIu64
                       "\",\"dma_startup_cycles\":\"%" PRIu64
                       "\",\"dma_source_cycles\":\"%" PRIu64
                       "\",\"dma_destination_cycles\":\"%" PRIu64
                       "\",\"dma_completed_units\":\"%" PRIu64
                       "\",\"dma_pause_cycles\":\"%" PRIu64
                       "\"},\"timer0\":{\"if_request\":",
                result->bus_cycles, result->internal_cycles,
                result->wait_cycles, result->dma_startup_cycles,
                result->dma_source_cycles, result->dma_destination_cycles,
                result->dma_completed_units, result->dma_pause_cycles) < 0) {
        return false;
    }
    if (result->has_timer0_if_request) {
        if (fputs(result->timer0_if_request ? "true" : "false", output) ==
            EOF) {
            return false;
        }
    } else if (fputs("null", output) == EOF) {
        return false;
    }
    if (fputs("},\"phase8\":", output) == EOF ||
        !gbl_runner_write_phase8(output, &result->phase8) ||
        fputs(",\"phase9\":", output) == EOF ||
        !gbl_runner_write_phase9(output, result) ||
        fputs(",\"phase10\":", output) == EOF ||
        !gbl_runner_write_phase10(output, result) ||
        fputs(",\"raw_video\":", output) == EOF ||
        !gbl_runner_write_raw_video(output, result) ||
        fputs(",\"termination\":", output) == EOF ||
        !gbl_runner_json_string(output, result->termination)) {
        return false;
    }
    if (result->has_expected_sha256) {
        if (fputs(",\"expected_sha256\":", output) == EOF ||
            !gbl_runner_json_string(output, result->expected_sha256)) {
            return false;
        }
    }
    if (result->trace_mask != 0u) {
        if (fprintf(output, ",\"trace\":{\"mask\":\"%" PRIu32
                           "\",\"count\":\"%" PRIuMAX
                           "\",\"capacity\":\"%" PRIuMAX
                           "\",\"complete\":%s,\"emitted\":%s}",
                    result->trace_mask, (uintmax_t)result->trace_count,
                    (uintmax_t)result->trace_capacity,
                    result->trace_complete ? "true" : "false",
                    result->trace_emitted ? "true" : "false") < 0) {
            return false;
        }
    } else if (fputs(",\"trace\":null", output) == EOF) {
        return false;
    }
    return fputs("}\n", output) != EOF && fflush(output) == 0 &&
           ferror(output) == 0;
}

void gbl_runner_result_dispose(gbl_runner_result *result)
{
    if (result == NULL) {
        return;
    }
    if (result->trace_records != NULL && result->allocator.release != NULL) {
        result->allocator.release(
            result->allocator.context, result->trace_records,
            result->trace_capacity * sizeof(*result->trace_records));
    }
    if (result->phase10.video_bytes != NULL &&
        result->allocator.release != NULL) {
        result->allocator.release(result->allocator.context,
                                  result->phase10.video_bytes,
                                  result->phase10.video_length);
    }
    memset(result, 0, sizeof *result);
}
