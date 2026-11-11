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

    if (fixture_id == NULL) {
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

static void gbl_runner_set_guest_stop(gbl_runner_result *result,
                                      const gbl_machine *machine,
                                      gbl_run_reason reason)
{
    const char *name = "guest-stop";

    if (reason == GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION) {
        name = "unsupported-instruction";
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
        case GBL_CPU_STOP_COPROCESSOR_NO_BIOS:
            name = "no-bios-coprocessor";
            break;
        case GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION:
            name = "unsupported-instruction";
            break;
        case GBL_CPU_STOP_BUS_ERROR: name = "bus-error"; break;
        case GBL_CPU_STOP_COUNTER_OVERFLOW: name = "counter-overflow"; break;
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
    char requested_fixture_id[GBL_RUNNER_FIXTURE_ID_CAPACITY] = { '\0' };
    bool has_fixture_argument;
    uint8_t *rom_bytes = NULL;
    size_t rom_length = 0u;
    char copied_digest[GBL_SHA256_HEX_CAPACITY];
    gbl_runner_read_status read_status;

    if (result == NULL) {
        return GBL_RUNNER_HARNESS_FAIL;
    }
    memset(result, 0, sizeof *result);
    result->status = GBL_RUNNER_HARNESS_FAIL;
    result->semantic.status = GBL_MAILBOX_ABSENT;
    result->trace_complete = true;
    gbl_runner_set_reason(result, "runner-failed");
    gbl_runner_set_termination(result, "not-started");

    if (options == NULL || options->rom_path == NULL ||
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
    has_fixture_argument = options->fixture_id != NULL;
    if ((has_fixture_argument &&
         !gbl_runner_copy_text(requested_fixture_id,
                               sizeof requested_fixture_id,
                               options->fixture_id)) ||
        (options->trace_output_path != NULL &&
         options->trace_output_path[0] == '\0') ||
        !gbl_runner_trace_options_valid(options)) {
        gbl_runner_set_reason(result, "invalid-runner-options");
        gbl_runner_set_refusal(result, "invalid-runner-options",
                               "guest-not-executed");
        gbl_runner_set_termination(result, "refused-before-run");
        result->status = GBL_RUNNER_REFUSED;
        return result->status;
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
            gbl_machine_dispose(machine);
            result->allocator.release(result->allocator.context, machine,
                                      sizeof *machine);
            result->allocator.release(result->allocator.context, rom_bytes,
                                      GBL_RUNNER_MAX_ROM_BYTES +
                                          (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
            return result->status;
        }
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
        gbl_run_status run_status;

        result->semantic = gbl_mailbox_read(machine);
        if (gbl_runner_semantic_terminal(result->semantic)) {
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
            (run_result.termination == GBL_RUN_PROGRESS &&
             run_result.cycles == 0u && run_result.instructions == 0u)) {
            result->status = GBL_RUNNER_HARNESS_FAIL;
            gbl_runner_set_reason(result, "cpu-run-failed");
            gbl_runner_set_termination(result, "harness-failure");
            break;
        }
        result->cycles += run_result.cycles;
        result->instructions += run_result.instructions;
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
        if (gbl_runner_semantic_terminal(result->semantic)) {
            gbl_runner_classify_terminal(result);
            break;
        }
        if (run_result.termination == GBL_RUN_STOPPED) {
            if (run_result.reason == GBL_RUN_REASON_INVALID_CPU_STATE ||
                run_result.reason == GBL_RUN_REASON_COUNTER_OVERFLOW) {
                result->status = GBL_RUNNER_HARNESS_FAIL;
                gbl_runner_set_reason(result, "cpu-state-failed");
                gbl_runner_set_termination(result, "harness-failure");
            } else {
                gbl_runner_set_guest_stop(result, machine,
                                          run_result.reason);
            }
            break;
        }
        if (run_result.termination == GBL_RUN_TIMEOUT) {
            result->status = GBL_RUNNER_TIMEOUT;
            gbl_runner_set_reason(result, "cycle-budget");
            gbl_runner_set_termination(result, "timeout");
            break;
        }
        if (run_result.termination != GBL_RUN_PROGRESS) {
            result->status = GBL_RUNNER_HARNESS_FAIL;
            gbl_runner_set_reason(result, "cpu-run-termination-invalid");
            gbl_runner_set_termination(result, "harness-failure");
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
    gbl_machine_dispose(machine);
    result->allocator.release(result->allocator.context, machine,
                              sizeof *machine);
    result->allocator.release(result->allocator.context, rom_bytes,
                              GBL_RUNNER_MAX_ROM_BYTES +
                                  (size_t)GBL_RUNNER_ROM_SENTINEL_BYTES);
    return result->status;
}
