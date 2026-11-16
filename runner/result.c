#include "run.h"

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

static bool gbl_runner_trace_record_write(FILE *output,
                                          const gbl_trace_record *record)
{
    const int written = fprintf(
        output,
        "{\"sequence\":\"%" PRIu64 "\",\"guest_cycle\":\"%" PRIu64
        "\",\"category\":%u,\"instruction_address\":\"0x%08" PRIx32
        "\",\"opcode\":\"0x%08" PRIx32 "\",\"address\":\"0x%08" PRIx32
        "\",\"value\":\"0x%08" PRIx32 "\",\"width\":%u,"
        "\"access_class\":%u,\"bus_outcome\":%u,\"exception_kind\":%u}\n",
        record->sequence, record->guest_cycle, (unsigned int)record->category,
        record->instruction_address, record->opcode, record->address,
        record->value, (unsigned int)record->width,
        (unsigned int)record->access_class, (unsigned int)record->bus_outcome,
        (unsigned int)record->exception_kind);

    return written >= 0;
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
        "BIOS provider none; DMA, active display and audio, physical input, and host presentation are not modeled",
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

bool gbl_runner_write_json(FILE *output, const gbl_runner_result *result)
{
    const char *reason;
    size_t index;

    if (output == NULL || result == NULL) {
        return false;
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
    if (fputs("],\"labels\":", output) == EOF) return false;
    if (result->metadata != NULL && result->metadata->dispute_count != 0u) {
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
                       "\"},\"timer0\":{\"if_request\":",
                result->bus_cycles, result->internal_cycles) < 0) {
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
    memset(result, 0, sizeof *result);
}
