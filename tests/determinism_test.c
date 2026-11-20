#define _POSIX_C_SOURCE 200809L

#include "run.h"
#include "state_check.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum {
    GBL_DETERMINISM_PATH_CAPACITY = 1024u,
    GBL_DETERMINISM_TRACE_CAPACITY = 4096u
};

typedef struct gbl_test_bytes {
    uint8_t *data;
    size_t length;
} gbl_test_bytes;

typedef struct gbl_test_observation {
    gbl_runner_result result;
    gbl_test_bytes json;
    gbl_test_bytes trace;
    gbl_test_bytes execution_trace;
} gbl_test_observation;

static bool gbl_test_path(char *output, size_t capacity, const char *directory,
                          const char *name)
{
    const int written = snprintf(output, capacity, "%s/%s", directory, name);

    return written >= 0 && (size_t)written < capacity;
}

static bool gbl_test_create_directory(char *output, size_t capacity)
{
    unsigned int attempt;

    for (attempt = 0u; attempt < 1000u; ++attempt) {
        const int written = snprintf(
            output, capacity, "%s/determinism.%ld.%u",
            GBL_DETERMINISM_SCRATCH_DIR, (long)getpid(), attempt);

        if (written < 0 || (size_t)written >= capacity) return false;
        if (mkdir(output, 0700) == 0) return true;
        if (errno != EEXIST) return false;
    }
    return false;
}

static bool gbl_test_read_file(const char *path, gbl_test_bytes *output)
{
    FILE *file = fopen(path, "rb");
    long end;
    size_t length;
    bool okay = false;

    if (file == NULL || output == NULL) {
        if (file != NULL) (void)fclose(file);
        return false;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (end = ftell(file)) <= 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return false;
    }
    length = (size_t)end;
    output->data = malloc(length);
    if (output->data != NULL &&
        fread(output->data, 1u, length, file) == length &&
        ferror(file) == 0) {
        output->length = length;
        okay = true;
    }
    if (fclose(file) != 0) okay = false;
    if (!okay) {
        free(output->data);
        output->data = NULL;
        output->length = 0u;
    }
    return okay;
}

static bool gbl_test_json(const gbl_runner_result *result,
                          gbl_test_bytes *output)
{
    FILE *file = tmpfile();
    long end;
    size_t length;
    bool okay = false;

    if (file == NULL || output == NULL) {
        if (file != NULL) (void)fclose(file);
        return false;
    }
    if (!gbl_runner_write_json(file, result) || fflush(file) != 0 ||
        fseek(file, 0L, SEEK_END) != 0 || (end = ftell(file)) <= 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return false;
    }
    length = (size_t)end;
    output->data = malloc(length);
    if (output->data != NULL &&
        fread(output->data, 1u, length, file) == length &&
        ferror(file) == 0) {
        output->length = length;
        okay = true;
    }
    if (fclose(file) != 0) okay = false;
    if (!okay) {
        free(output->data);
        output->data = NULL;
        output->length = 0u;
    }
    return okay;
}

static size_t gbl_test_newline_count(const gbl_test_bytes *bytes)
{
    size_t index;
    size_t count = 0u;

    for (index = 0u; index < bytes->length; ++index) {
        if (bytes->data[index] == (uint8_t)'\n') count++;
    }
    return count;
}

static bool gbl_test_observe(const char *directory, const char *trace_name,
                             const char *serialized_trace_name,
                             gbl_test_observation *observation,
                             const char **failure)
{
    char rom_path[GBL_DETERMINISM_PATH_CAPACITY];
    char execution_trace_path[GBL_DETERMINISM_PATH_CAPACITY];
    char serialized_trace_path[GBL_DETERMINISM_PATH_CAPACITY];
    gbl_runner_options options = { 0 };
    gbl_runner_status status;

    if (!gbl_test_path(rom_path, sizeof rom_path,
                       GBL_DETERMINISM_FIXTURE_DIR, "owned-arm-pass.gba") ||
        !gbl_test_path(execution_trace_path, sizeof execution_trace_path,
                       directory, trace_name) ||
        !gbl_test_path(serialized_trace_path, sizeof serialized_trace_path,
                       directory, serialized_trace_name)) {
        *failure = "temporary-path-invalid";
        return false;
    }
    options.rom_path = rom_path;
    options.fixture_id = "synthetic-control-pass";
    options.cycle_budget = UINT64_C(1000);
    options.trace_mask = GBL_TRACE_CATEGORY_MASK;
    options.trace_capacity = GBL_DETERMINISM_TRACE_CAPACITY;
    options.trace_output_path = execution_trace_path;

    status = gbl_runner_execute(&options, &observation->result);
    if (status != GBL_RUNNER_PASS ||
        observation->result.status != GBL_RUNNER_PASS ||
        !observation->result.ran || observation->result.metadata == NULL ||
        observation->result.semantic.status != GBL_MAILBOX_PASS ||
        strcmp(observation->result.termination, "mailbox-complete") != 0) {
        *failure = "runner-did-not-complete-semantic-pass";
        return false;
    }
    if (observation->result.cycles == 0u ||
        observation->result.instructions == 0u ||
        observation->result.trace_count == 0u ||
        observation->result.trace_count > observation->result.trace_capacity ||
        observation->result.trace_records == NULL ||
        !observation->result.trace_complete ||
        !observation->result.trace_emitted) {
        *failure = "guest-progress-or-complete-trace-missing";
        return false;
    }
    if (!gbl_runner_write_trace(serialized_trace_path,
                                &observation->result) ||
        !gbl_test_read_file(serialized_trace_path, &observation->trace) ||
        !gbl_test_read_file(execution_trace_path,
                            &observation->execution_trace) ||
        !gbl_test_json(&observation->result, &observation->json)) {
        *failure = "result-or-trace-serialization-failed";
        return false;
    }
    if (observation->trace.length == 0u ||
        observation->execution_trace.length == 0u ||
        observation->json.length == 0u ||
        observation->trace.data[observation->trace.length - 1u] !=
            (uint8_t)'\n' ||
        gbl_test_newline_count(&observation->trace) !=
            observation->result.trace_count ||
        observation->trace.length != observation->execution_trace.length ||
        memcmp(observation->trace.data, observation->execution_trace.data,
               observation->trace.length) != 0) {
        *failure = "trace-is-empty-incomplete-or-noncanonical";
        return false;
    }
    return true;
}

static bool gbl_test_hash(const gbl_test_bytes *bytes, char *output)
{
    return bytes->length != 0u &&
           gbl_sha256_span(bytes->data, bytes->length, output);
}

static bool gbl_test_equal(const gbl_test_observation *first,
                           const gbl_test_observation *second,
                           const char **failure)
{
    char first_json_hash[GBL_SHA256_HEX_CAPACITY];
    char second_json_hash[GBL_SHA256_HEX_CAPACITY];
    char first_trace_hash[GBL_SHA256_HEX_CAPACITY];
    char second_trace_hash[GBL_SHA256_HEX_CAPACITY];

    if (!gbl_test_hash(&first->json, first_json_hash) ||
        !gbl_test_hash(&second->json, second_json_hash) ||
        !gbl_test_hash(&first->trace, first_trace_hash) ||
        !gbl_test_hash(&second->trace, second_trace_hash)) {
        *failure = "evidence-hash-failed";
        return false;
    }
    if (first->result.cycles != second->result.cycles ||
        first->result.instructions != second->result.instructions) {
        *failure = "guest-progress-differs";
        return false;
    }
    if (first->json.length != second->json.length ||
        memcmp(first->json.data, second->json.data, first->json.length) != 0 ||
        strcmp(first_json_hash, second_json_hash) != 0) {
        *failure = "result-json-bytes-differ";
        return false;
    }
    if (first->trace.length != second->trace.length ||
        memcmp(first->trace.data, second->trace.data, first->trace.length) != 0 ||
        strcmp(first_trace_hash, second_trace_hash) != 0) {
        *failure = "trace-bytes-differ";
        return false;
    }
    return true;
}

static void gbl_test_dispose(gbl_test_observation *observation)
{
    gbl_runner_result_dispose(&observation->result);
    free(observation->json.data);
    free(observation->trace.data);
    free(observation->execution_trace.data);
    memset(observation, 0, sizeof *observation);
}

enum {
    GBL_INTERLEAVE_TRACE_CAPACITY = 4096u,
    GBL_INTERLEAVE_TRACE_LINE_CAPACITY = 1024u,
    GBL_INTERLEAVE_MAX_UNITS = 10000u
};

typedef struct gbl_interleave_instance {
    gbl_machine machine;
    gbl_trace_record *records;
    gbl_trace_buffer trace;
    gbl_mailbox_result mailbox;
    gbl_run_termination termination;
    gbl_run_reason reason;
    uint64_t budget;
    uint64_t base_cycles;
    uint64_t cycles;
    uint64_t instructions;
    uint64_t bus_cycles;
    uint64_t internal_cycles;
    uint64_t wait_cycles;
    uint64_t dma_startup_cycles;
    uint64_t dma_source_cycles;
    uint64_t dma_destination_cycles;
    uint64_t dma_completed_units;
    uint64_t dma_pause_cycles;
    uint64_t wake_cycle;
    uint16_t wake_source_mask;
    gbl_cpu_power_cause power_cause;
    char *serialized_trace;
    size_t serialized_trace_length;
    bool initialized;
    bool complete;
} gbl_interleave_instance;

static void *gbl_interleave_allocate(void *context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void gbl_interleave_release(void *context, void *pointer, size_t size)
{
    (void)context;
    (void)size;
    free(pointer);
}

static bool gbl_interleave_load(gbl_interleave_instance *instance,
                                const uint8_t *image, size_t image_length,
                                uint64_t budget)
{
    gbl_load_result loaded;
    const gbl_allocator allocator = {
        NULL, gbl_interleave_allocate, gbl_interleave_release
    };

    if (instance == NULL || image == NULL || image_length == 0u ||
        budget == 0u) {
        return false;
    }
    memset(instance, 0, sizeof *instance);
    instance->records = calloc(GBL_INTERLEAVE_TRACE_CAPACITY,
                               sizeof *instance->records);
    if (instance->records == NULL) {
        fprintf(stderr, "determinism.interleave: trace allocation failed\n");
        return false;
    }
    if (gbl_machine_init(&instance->machine, &allocator) !=
        GBL_BUS_STATUS_OK) {
        free(instance->records);
        instance->records = NULL;
        fprintf(stderr, "determinism.interleave: machine init failed\n");
        return false;
    }
    instance->initialized = true;
    loaded = gbl_load_rom(&instance->machine, image, image_length);
    if (loaded.status != GBL_LOAD_STATUS_OK ||
        loaded.reason != GBL_LOAD_REASON_NONE) {
        fprintf(stderr,
                "determinism.interleave: ROM load failed (status=%u reason=%u length=%zu)\n",
                (unsigned int)loaded.status, (unsigned int)loaded.reason,
                image_length);
        gbl_machine_dispose(&instance->machine);
        free(instance->records);
        memset(instance, 0, sizeof *instance);
        return false;
    }
    instance->trace = (gbl_trace_buffer){
        instance->records, GBL_INTERLEAVE_TRACE_CAPACITY, 0u, 0u
    };
    instance->budget = budget;
    instance->mailbox = gbl_mailbox_read(&instance->machine);
    return true;
}

static bool gbl_interleave_set_timeline(
    gbl_interleave_instance *instance, const gbl_input_event *events,
    size_t count)
{
    return instance != NULL && instance->initialized &&
           gbl_machine_set_input_timeline(&instance->machine, events, count) ==
               GBL_TIME_STATUS_OK;
}

static bool gbl_interleave_dispose(gbl_interleave_instance *instance)
{
    bool disposed;

    if (instance == NULL) return false;
    if (instance->initialized) gbl_machine_dispose(&instance->machine);
    disposed = !instance->machine.initialized && instance->machine.rom == NULL &&
               instance->machine.rom_length == 0u &&
               instance->machine.allocator.allocate == NULL &&
               instance->machine.allocator.release == NULL;
    free(instance->records);
    free(instance->serialized_trace);
    memset(instance, 0, sizeof *instance);
    return disposed;
}

static bool gbl_interleave_step(gbl_interleave_instance *instance,
                                const char **failure)
{
    gbl_run_options options;
    gbl_run_result result = { 0 };
    uint64_t remaining;
    uint64_t component_cycles;

    if (instance == NULL || !instance->initialized || instance->complete ||
        failure == NULL) {
        if (failure != NULL) *failure = "interleave-instance-not-runnable";
        return false;
    }
    if (instance->cycles >= instance->budget) {
        *failure = "interleave-budget-exhausted-before-terminal-observation";
        return false;
    }
    remaining = instance->budget - instance->cycles;
    options = (gbl_run_options){
        remaining, UINT64_C(1), GBL_TRACE_CATEGORY_MASK, &instance->trace
    };
    if (gbl_cpu_run(&instance->machine, &options, &result) !=
        GBL_RUN_STATUS_OK) {
        *failure = "interleave-canonical-run-failed";
        return false;
    }
    if (UINT64_MAX - result.bus_cycles < result.internal_cycles ||
        UINT64_MAX - result.bus_cycles - result.internal_cycles <
            result.wait_cycles) {
        *failure = "interleave-cycle-components-overflow";
        return false;
    }
    component_cycles = result.bus_cycles + result.internal_cycles +
                       result.wait_cycles;
    if (component_cycles != result.cycles ||
        UINT64_MAX - instance->cycles < result.cycles ||
        UINT64_MAX - instance->instructions < result.instructions ||
        UINT64_MAX - instance->bus_cycles < result.bus_cycles ||
        UINT64_MAX - instance->internal_cycles < result.internal_cycles ||
        UINT64_MAX - instance->wait_cycles < result.wait_cycles ||
        UINT64_MAX - instance->dma_startup_cycles <
            result.dma_startup_cycles ||
        UINT64_MAX - instance->dma_source_cycles < result.dma_source_cycles ||
        UINT64_MAX - instance->dma_destination_cycles <
            result.dma_destination_cycles ||
        UINT64_MAX - instance->dma_completed_units <
            result.dma_completed_units ||
        UINT64_MAX - instance->dma_pause_cycles < result.dma_pause_cycles) {
        *failure = "interleave-progress-overflow-or-cycle-accounting";
        return false;
    }
    instance->cycles += result.cycles;
    instance->instructions += result.instructions;
    instance->bus_cycles += result.bus_cycles;
    instance->internal_cycles += result.internal_cycles;
    instance->wait_cycles += result.wait_cycles;
    instance->dma_startup_cycles += result.dma_startup_cycles;
    instance->dma_source_cycles += result.dma_source_cycles;
    instance->dma_destination_cycles += result.dma_destination_cycles;
    instance->dma_completed_units += result.dma_completed_units;
    instance->dma_pause_cycles += result.dma_pause_cycles;
    if (instance->base_cycles > UINT64_MAX - instance->cycles ||
        instance->machine.master_cycles !=
            instance->base_cycles + instance->cycles) {
        *failure = "interleave-machine-time-accounting-mismatch";
        return false;
    }
    if (result.wake_cycle != 0u || result.wake_source_mask != 0u ||
        result.power_cause != GBL_CPU_POWER_CAUSE_NONE) {
        instance->wake_cycle = result.wake_cycle;
        instance->wake_source_mask = result.wake_source_mask;
        instance->power_cause = result.power_cause;
    }
    instance->termination = result.termination;
    instance->reason = result.reason;
    instance->mailbox = gbl_mailbox_read(&instance->machine);
    if (result.termination == GBL_RUN_TRACE_OUTPUT_FULL) {
        *failure = "interleave-trace-capacity-exhausted";
        return false;
    }
    if (instance->mailbox.status == GBL_MAILBOX_PASS ||
        instance->mailbox.status == GBL_MAILBOX_FAIL ||
        result.termination == GBL_RUN_STOPPED ||
        result.termination == GBL_RUN_TIMEOUT) {
        instance->complete = true;
    }
    return true;
}

static bool gbl_interleave_run_isolated(gbl_interleave_instance *instance,
                                        const char **failure)
{
    unsigned int unit;

    for (unit = 0u; unit < GBL_INTERLEAVE_MAX_UNITS && !instance->complete;
         ++unit) {
        if (!gbl_interleave_step(instance, failure)) return false;
    }
    if (!instance->complete) {
        *failure = "interleave-unit-limit-reached";
        return false;
    }
    return true;
}

static bool gbl_interleave_run_alternating(
    gbl_interleave_instance *first, gbl_interleave_instance *second,
    const char **failure)
{
    unsigned int unit;

    for (unit = 0u; unit < GBL_INTERLEAVE_MAX_UNITS &&
                    (!first->complete || !second->complete); ++unit) {
        if (!first->complete && !gbl_interleave_step(first, failure)) {
            return false;
        }
        if (!second->complete && !gbl_interleave_step(second, failure)) {
            return false;
        }
    }
    if (!first->complete || !second->complete) {
        *failure = "interleave-unit-limit-reached";
        return false;
    }
    return true;
}

static bool gbl_interleave_serialize_trace(
    gbl_interleave_instance *instance)
{
    size_t capacity;
    size_t index;
    size_t offset = 0u;

    if (instance == NULL) return false;
    free(instance->serialized_trace);
    instance->serialized_trace = NULL;
    instance->serialized_trace_length = 0u;
    if (instance->trace.count == 0u ||
        instance->trace.count > instance->trace.capacity ||
        instance->trace.records == NULL ||
        instance->trace.count >
            (SIZE_MAX - 1u) / GBL_INTERLEAVE_TRACE_LINE_CAPACITY) {
        return false;
    }
    capacity = instance->trace.count * GBL_INTERLEAVE_TRACE_LINE_CAPACITY + 1u;
    instance->serialized_trace = malloc(capacity);
    if (instance->serialized_trace == NULL) return false;
    for (index = 0u; index < instance->trace.count; ++index) {
        const gbl_trace_record *record = &instance->trace.records[index];
        const int written = snprintf(
            instance->serialized_trace + offset, capacity - offset,
            "{\"sequence\":\"%" PRIu64 "\",\"guest_cycle\":\"%"
            PRIu64 "\",\"category\":%u,\"instruction_address\":\"0x%08"
            PRIx32 "\",\"opcode\":\"0x%08" PRIx32
            "\",\"address\":\"0x%08" PRIx32
            "\",\"value\":\"0x%08" PRIx32
            "\",\"width\":%u,\"access_class\":%u,\"bus_outcome\":%u,"
            "\"exception_kind\":%u,\"bus_owner\":%u,\"dma_status\":%u,"
            "\"dma_channel\":%u,\"dma_startup_cycles\":\"%" PRIu64
            "\",\"dma_source_cycles\":\"%" PRIu64
            "\",\"dma_destination_cycles\":\"%" PRIu64
            "\",\"dma_source_sequence\":%u,\"dma_destination_sequence\":%u,"
            "\"dma_remaining\":%u,\"dma_completed\":\"%" PRIu64
            "\",\"dma_used_channel_latch\":%s,\"dma_value_provenance\":%u,"
            "\"dma_result_classification\":%u,\"dma_dispute\":%u,"
            "\"dma_hardware_qualification\":%u,"
            "\"dma_source_address\":\"0x%08" PRIx32
            "\",\"dma_destination_address\":\"0x%08" PRIx32
            "\",\"dma_value\":\"0x%08" PRIx32
            "\",\"dma_width\":%u}\n",
            record->sequence, record->guest_cycle,
            (unsigned int)record->category, record->instruction_address,
            record->opcode, record->address, record->value,
            (unsigned int)record->width, (unsigned int)record->access_class,
            (unsigned int)record->bus_outcome,
            (unsigned int)record->exception_kind,
            (unsigned int)record->bus_owner,
            (unsigned int)record->dma_status,
            (unsigned int)record->dma_channel,
            record->dma_startup_cycles, record->dma_source_cycles,
            record->dma_destination_cycles,
            (unsigned int)record->dma_source_sequence,
            (unsigned int)record->dma_destination_sequence,
            record->dma_remaining, record->dma_completed,
            record->dma_used_channel_latch ? "true" : "false",
            (unsigned int)record->dma_value_provenance,
            (unsigned int)record->dma_result_classification,
            (unsigned int)record->dma_dispute,
            (unsigned int)record->dma_hardware_qualification,
            record->dma_source_address, record->dma_destination_address,
            record->dma_value, (unsigned int)record->dma_width);

        if (written < 0 || (size_t)written >= capacity - offset) return false;
        offset += (size_t)written;
    }
    instance->serialized_trace_length = offset;
    return offset != 0u;
}

static bool gbl_interleave_machine_equal(const gbl_machine *first,
                                         const gbl_machine *second)
{
    return gbl_test_canonical_equal(first, second);
}

static bool gbl_interleave_trace_fields_equal(
    const gbl_trace_record *first, const gbl_trace_record *second,
    size_t count)
{
    return gbl_test_trace_equal(first, second, count);
}

static bool gbl_interleave_trace_has_instruction(
    const gbl_interleave_instance *instance, uint32_t address,
    uint32_t opcode)
{
    size_t index;

    for (index = 0u; index < instance->trace.count; ++index) {
        const gbl_trace_record *record = &instance->trace.records[index];

        if (record->category == GBL_TRACE_INSTRUCTION &&
            record->instruction_address == address && record->opcode == opcode) {
            return true;
        }
    }
    return false;
}

static const char *gbl_interleave_compare(
    const gbl_interleave_instance *expected,
    const gbl_interleave_instance *actual)
{
    if (expected->mailbox.status != actual->mailbox.status ||
        expected->mailbox.case_id != actual->mailbox.case_id ||
        expected->termination != actual->termination ||
        expected->reason != actual->reason ||
        expected->machine.cpu.stopped != actual->machine.cpu.stopped ||
        expected->machine.cpu.stop_reason != actual->machine.cpu.stop_reason) {
        return "interleave-outcome-mismatch";
    }
    if (expected->cycles != actual->cycles ||
        expected->instructions != actual->instructions ||
        expected->wait_cycles != actual->wait_cycles ||
        expected->wake_cycle != actual->wake_cycle ||
        expected->wake_source_mask != actual->wake_source_mask ||
        expected->power_cause != actual->power_cause ||
        expected->base_cycles != actual->base_cycles ||
        expected->machine.master_cycles != actual->machine.master_cycles ||
        expected->machine.cpu.retired_instructions !=
            actual->machine.cpu.retired_instructions ||
        expected->bus_cycles != actual->bus_cycles ||
        expected->internal_cycles != actual->internal_cycles ||
        expected->dma_startup_cycles != actual->dma_startup_cycles ||
        expected->dma_source_cycles != actual->dma_source_cycles ||
        expected->dma_destination_cycles != actual->dma_destination_cycles ||
        expected->dma_completed_units != actual->dma_completed_units ||
        expected->dma_pause_cycles != actual->dma_pause_cycles) {
        return "interleave-progress-mismatch";
    }
    if (!gbl_interleave_machine_equal(&expected->machine, &actual->machine)) {
        return "interleave-state-mismatch";
    }
    if (expected->trace.count != actual->trace.count ||
        !gbl_interleave_trace_fields_equal(expected->trace.records,
                                           actual->trace.records,
                                           expected->trace.count) ||
        expected->serialized_trace_length != actual->serialized_trace_length ||
        memcmp(expected->serialized_trace, actual->serialized_trace,
               expected->serialized_trace_length) != 0) {
        return "interleave-trace-mismatch";
    }
    return "ok";
}

typedef struct gbl_interleave_pair {
    gbl_interleave_instance isolated_first;
    gbl_interleave_instance isolated_second;
    gbl_interleave_instance alternating_first;
    gbl_interleave_instance alternating_second;
} gbl_interleave_pair;

static bool gbl_interleave_pair_prepare(
    gbl_interleave_pair *pair, const uint8_t *first_image,
    size_t first_length, uint64_t first_budget, const uint8_t *second_image,
    size_t second_length, uint64_t second_budget, const char **failure)
{
    if (!gbl_interleave_load(&pair->isolated_first, first_image, first_length,
                             first_budget)) {
        *failure = "isolated-first-machine-load-failed";
        return false;
    }
    if (!gbl_interleave_load(&pair->isolated_second, second_image,
                             second_length, second_budget)) {
        *failure = "isolated-second-machine-load-failed";
        return false;
    }
    if (!gbl_interleave_run_isolated(&pair->isolated_first, failure) ||
        !gbl_interleave_run_isolated(&pair->isolated_second, failure) ||
        !gbl_interleave_serialize_trace(&pair->isolated_first) ||
        !gbl_interleave_serialize_trace(&pair->isolated_second)) {
        if (*failure == NULL) *failure = "isolated-run-or-trace-failed";
        return false;
    }
    if (!gbl_interleave_load(&pair->alternating_first, first_image,
                             first_length, first_budget)) {
        *failure = "alternating-first-machine-load-failed";
        return false;
    }
    if (!gbl_interleave_load(&pair->alternating_second, second_image,
                             second_length, second_budget)) {
        *failure = "alternating-second-machine-load-failed";
        return false;
    }
    if (!gbl_interleave_run_alternating(&pair->alternating_first,
                                        &pair->alternating_second, failure) ||
        !gbl_interleave_serialize_trace(&pair->alternating_first) ||
        !gbl_interleave_serialize_trace(&pair->alternating_second)) {
        if (*failure == NULL) *failure = "alternating-run-or-trace-failed";
        return false;
    }
    if (strcmp(gbl_interleave_compare(&pair->isolated_first,
                                      &pair->alternating_first), "ok") != 0 ||
        strcmp(gbl_interleave_compare(&pair->isolated_second,
                                      &pair->alternating_second), "ok") != 0) {
        *failure = "isolated-and-alternating-pair-differed";
        return false;
    }
    return true;
}

static bool gbl_interleave_isolated_prepare(
    gbl_interleave_instance *instance, const uint8_t *image, size_t length,
    uint64_t budget, const char **failure)
{
    if (!gbl_interleave_load(instance, image, length, budget)) {
        *failure = "isolated-machine-load-failed";
        return false;
    }
    if (!gbl_interleave_run_isolated(instance, failure) ||
        !gbl_interleave_serialize_trace(instance)) {
        if (*failure == NULL) *failure = "isolated-run-or-trace-failed";
        return false;
    }
    return true;
}

static bool gbl_interleave_timer0_complete(
    const gbl_interleave_instance *instance)
{
    size_t index;
    bool live_timer0_event = false;

    if (instance == NULL || instance->mailbox.status != GBL_MAILBOX_PASS ||
        instance->mailbox.case_id != UINT32_C(7001) ||
        instance->cycles == 0u || instance->cycles > UINT64_C(10000) ||
        instance->base_cycles > UINT64_MAX - instance->cycles ||
        instance->machine.master_cycles !=
            instance->base_cycles + instance->cycles ||
        (instance->machine.interrupt_flags & (UINT16_C(1) << 3u)) == 0u ||
        !instance->machine.timers[0].enabled ||
        !instance->machine.timers[0].irq_enabled ||
        instance->machine.event_count > GBL_EVENT_CAPACITY) {
        return false;
    }
    for (index = 0u; index < instance->machine.event_count; ++index) {
        const gbl_event *event = &instance->machine.events[index];

        if (event->live &&
            event->kind == (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW &&
            event->timer_index == 0u &&
            event->generation == instance->machine.timers[0].generation &&
            event->cycle > instance->machine.master_cycles) {
            live_timer0_event = true;
        }
    }
    return live_timer0_event;
}

static bool gbl_interleave_pair_dispose(gbl_interleave_pair *pair)
{
    bool disposed = true;

    disposed = gbl_interleave_dispose(&pair->isolated_first) && disposed;
    disposed = gbl_interleave_dispose(&pair->isolated_second) && disposed;
    disposed = gbl_interleave_dispose(&pair->alternating_first) && disposed;
    disposed = gbl_interleave_dispose(&pair->alternating_second) && disposed;
    return disposed;
}

static bool gbl_interleave_pair_totals_equal(const gbl_interleave_pair *pair)
{
    return pair->isolated_first.cycles + pair->isolated_second.cycles ==
               pair->alternating_first.cycles +
                   pair->alternating_second.cycles &&
           pair->isolated_first.instructions +
                   pair->isolated_second.instructions ==
               pair->alternating_first.instructions +
                   pair->alternating_second.instructions;
}

static bool gbl_interleave_reset_reload(gbl_interleave_instance *instance,
                                        const uint8_t *image,
                                        size_t image_length,
                                        uint64_t budget)
{
    gbl_load_result loaded;

    if (instance == NULL || !instance->initialized || image == NULL ||
        image_length == 0u || budget == 0u) {
        return false;
    }
    gbl_machine_reset(&instance->machine);
    loaded = gbl_load_rom(&instance->machine, image, image_length);
    if (loaded.status != GBL_LOAD_STATUS_OK ||
        loaded.reason != GBL_LOAD_REASON_NONE) {
        return false;
    }
    free(instance->serialized_trace);
    instance->serialized_trace = NULL;
    instance->serialized_trace_length = 0u;
    instance->trace.count = 0u;
    instance->trace.next_sequence = 0u;
    instance->mailbox = gbl_mailbox_read(&instance->machine);
    instance->termination = GBL_RUN_PROGRESS;
    instance->reason = GBL_RUN_REASON_NONE;
    instance->budget = budget;
    instance->base_cycles = 0u;
    instance->cycles = 0u;
    instance->instructions = 0u;
    instance->bus_cycles = 0u;
    instance->internal_cycles = 0u;
    instance->wait_cycles = 0u;
    instance->dma_startup_cycles = 0u;
    instance->dma_source_cycles = 0u;
    instance->dma_destination_cycles = 0u;
    instance->dma_completed_units = 0u;
    instance->dma_pause_cycles = 0u;
    instance->wake_cycle = 0u;
    instance->wake_source_mask = 0u;
    instance->power_cause = GBL_CPU_POWER_CAUSE_NONE;
    instance->complete = false;
    return true;
}

static bool gbl_interleave_trace_has_difference(
    const gbl_interleave_instance *first,
    const gbl_interleave_instance *second, size_t *index_out)
{
    const size_t limit = first->trace.count < second->trace.count
                             ? first->trace.count
                             : second->trace.count;
    size_t index;

    for (index = 0u; index < limit; ++index) {
        if (!gbl_interleave_trace_fields_equal(&first->trace.records[index],
                                              &second->trace.records[index],
                                              1u)) {
            *index_out = index;
            return true;
        }
    }
    return false;
}

static bool gbl_interleave_fixture(gbl_test_bytes *bytes, const char *name)
{
    char path[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };

    if (!gbl_test_path(path, sizeof path, GBL_DETERMINISM_FIXTURE_DIR, name)) {
        fprintf(stderr, "determinism.interleave: fixture path is too long (%s)\n",
                name);
        return false;
    }
    if (!gbl_test_read_file(path, bytes)) {
        fprintf(stderr,
                "determinism.interleave: fixture read failed (%s, errno=%d)\n",
                path, errno);
        return false;
    }
    return true;
}

static int gbl_test_interleave_main(void)
{
    gbl_test_bytes arm_image = { 0 };
    gbl_test_bytes thumb_image = { 0 };
    gbl_test_bytes timer0_image = { 0 };
    gbl_interleave_instance isolated_arm = { 0 };
    gbl_interleave_instance isolated_thumb = { 0 };
    gbl_interleave_instance interleaved_arm = { 0 };
    gbl_interleave_instance interleaved_thumb = { 0 };
    gbl_interleave_pair timer0_pair = { 0 };
    const char *failure = "interleave-fixture-load-failed";
    bool passed = false;

    if (!gbl_interleave_fixture(&arm_image, "owned-arm-pass.gba") ||
        !gbl_interleave_fixture(&thumb_image,
                                "owned-arm-thumb-bx-pass.gba")) {
        goto done;
    }
    /* The owned Thumb-bearing control image contains the result literal at
     * 0x17C; changing only its test result makes the second guest fail after
     * its ARM-to-Thumb-to-ARM path has executed. */
    if (thumb_image.length <= 0x17Fu ||
        thumb_image.data[0x17Cu] != UINT8_C(2) ||
        thumb_image.data[0x17Du] != 0u || thumb_image.data[0x17Eu] != 0u ||
        thumb_image.data[0x17Fu] != 0u) {
        failure = "owned-thumb-fixture-result-literal-changed";
        goto done;
    }
    thumb_image.data[0x17Cu] = UINT8_C(3);

    if (!gbl_interleave_load(&isolated_arm, arm_image.data, arm_image.length,
                             UINT64_C(1000))) {
        failure = "isolated-arm-machine-load-failed";
        goto done;
    }
    if (!gbl_interleave_load(&isolated_thumb, thumb_image.data,
                             thumb_image.length, UINT64_C(1000))) {
        failure = "isolated-thumb-machine-load-failed";
        goto done;
    }
    if (!gbl_interleave_run_isolated(&isolated_arm, &failure) ||
        !gbl_interleave_run_isolated(&isolated_thumb, &failure) ||
        !gbl_interleave_serialize_trace(&isolated_arm) ||
        !gbl_interleave_serialize_trace(&isolated_thumb)) {
        if (failure == NULL) failure = "isolated-execution-or-trace-failed";
        goto done;
    }
    if (isolated_arm.mailbox.status != GBL_MAILBOX_PASS ||
        isolated_arm.mailbox.case_id != UINT32_C(0x9001) ||
        isolated_thumb.mailbox.status != GBL_MAILBOX_FAIL ||
        isolated_thumb.mailbox.case_id != UINT32_C(0x9005) ||
        !gbl_interleave_trace_has_instruction(&isolated_arm,
                                              UINT32_C(0x080000C0),
                                              UINT32_C(0xE59F1038)) ||
        !gbl_interleave_trace_has_instruction(&isolated_thumb,
                                              UINT32_C(0x080001A0),
                                              UINT32_C(0x0000222A)) ||
        isolated_thumb.machine.cpu.stopped) {
        failure = "isolated-arm-thumb-outcomes-not-distinct";
        goto done;
    }
    if (!gbl_interleave_load(&interleaved_arm, arm_image.data,
                             arm_image.length, UINT64_C(1000)) ||
        !gbl_interleave_load(&interleaved_thumb, thumb_image.data,
                             thumb_image.length, UINT64_C(1000)) ||
        !gbl_interleave_run_alternating(&interleaved_arm, &interleaved_thumb,
                                        &failure) ||
        !gbl_interleave_serialize_trace(&interleaved_arm) ||
        !gbl_interleave_serialize_trace(&interleaved_thumb)) {
        if (failure == NULL) failure = "interleaved-trace-serialization-failed";
        goto done;
    }
    if (strcmp(gbl_interleave_compare(&isolated_arm, &interleaved_arm), "ok") !=
            0 ||
        strcmp(gbl_interleave_compare(&isolated_thumb, &interleaved_thumb),
               "ok") != 0) {
        failure = "isolated-and-alternating-observations-differ";
        goto done;
    }
    if (!gbl_interleave_fixture(&timer0_image, "owned-timer0-pass.gba") ||
        !gbl_interleave_pair_prepare(
            &timer0_pair, timer0_image.data, timer0_image.length,
            UINT64_C(10000), arm_image.data, arm_image.length,
            UINT64_C(1000), &failure) ||
        !gbl_interleave_timer0_complete(&timer0_pair.isolated_first) ||
        !gbl_interleave_timer0_complete(&timer0_pair.alternating_first) ||
        !gbl_test_time_state_equal(&timer0_pair.isolated_first.machine,
                                   &timer0_pair.alternating_first.machine) ||
        strcmp(gbl_interleave_compare(&timer0_pair.isolated_first,
                                      &timer0_pair.alternating_first),
               "ok") != 0) {
        failure = "timer0-isolated-and-alternating-observations-differ";
        goto done;
    }
    if (!gbl_interleave_pair_dispose(&timer0_pair)) {
        failure = "timer0-interleave-pair-disposal-failed";
        goto done;
    }
    passed = true;

done:
    {
        bool disposed = true;

        disposed = gbl_interleave_dispose(&isolated_arm) && disposed;
        disposed = gbl_interleave_dispose(&isolated_thumb) && disposed;
        disposed = gbl_interleave_dispose(&interleaved_arm) && disposed;
        disposed = gbl_interleave_dispose(&interleaved_thumb) && disposed;
        disposed = gbl_interleave_pair_dispose(&timer0_pair) && disposed;
        if (!disposed) {
            failure = "machine-disposal-postcondition-failed";
            passed = false;
        }
    }
    free(arm_image.data);
    free(thumb_image.data);
    free(timer0_image.data);
    if (passed) {
        puts("PASS determinism.interleave: isolated and alternating ARM, ARM-Thumb and Timer 0 guests match canonical machine state and traces");
        return 0;
    }
    fprintf(stderr, "FAIL determinism.interleave: %s\n", failure);
    return 1;
}

static void gbl_interleave_seed_live_timer(gbl_machine *machine)
{
    gbl_timer_state *timer = &machine->timers[0];

    *timer = (gbl_timer_state){
        UINT16_MAX, UINT16_MAX, UINT16_C(1), 0u,
        machine->master_cycles, UINT64_C(1), true, true, false
    };
    machine->interrupt_flags &= (uint16_t)~(UINT16_C(1) << 3u);
    machine->event_count = 1u;
    machine->next_event_sequence = UINT64_C(1);
    machine->events[0] = (gbl_event){
        .cycle = machine->master_cycles + UINT64_C(1),
        .sequence = UINT64_C(1),
        .generation = UINT64_C(1),
        .priority = 0u,
        .kind = (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW,
        .timer_index = 0u,
        .dma_channel = 0u,
        .live = true,
        .input_index = 0u
    };
}

static bool gbl_phase9_write(gbl_machine *machine, uint32_t address,
                             uint8_t width, uint32_t value)
{
    const gbl_bus_result result = gbl_bus_write(
        machine, address, width, GBL_BUS_CLASS_NONSEQUENTIAL, value);

    return result.status == GBL_BUS_STATUS_OK;
}

static bool gbl_phase9_configure(gbl_interleave_instance *instance,
                                 bool variant, const char **failure);

static bool gbl_phase9_configure_dma(gbl_machine *machine, uint8_t channel,
                                     uint32_t source, uint32_t destination,
                                     uint16_t count, uint16_t control)
{
    const uint32_t base = UINT32_C(0x040000B0) +
                          (uint32_t)channel * UINT32_C(12);

    return gbl_phase9_write(machine, base, 4u, source) &&
           gbl_phase9_write(machine, base + 4u, 4u, destination) &&
           gbl_phase9_write(machine, base + 8u, 2u, count) &&
           gbl_phase9_write(machine, base + 10u, 2u, control);
}

static bool gbl_phase9_prepare(gbl_interleave_instance *instance,
                               const uint8_t *image, size_t image_length,
                               bool variant, const char **failure)
{
    if (!gbl_interleave_load(instance, image, image_length, UINT64_C(1000))) {
        *failure = "phase9-synthetic-machine-load-failed";
        return false;
    }
    return gbl_phase9_configure(instance, variant, failure);
}

static bool gbl_phase9_configure(gbl_interleave_instance *instance,
                                 bool variant, const char **failure)
{
    const uint16_t values_a[3] = {
        UINT16_C(0x1234), UINT16_C(0x5678), UINT16_C(0xABCD)
    };
    const uint16_t values_b[3] = {
        UINT16_C(0xA1B2), UINT16_C(0xC3D4), UINT16_C(0xE5F6)
    };
    const uint16_t *values = variant ? values_b : values_a;
    gbl_input_event input;
    uint64_t ready_cycle;

    instance->machine.ewram[0] = (uint8_t)(values[0] & UINT16_C(0xFF));
    instance->machine.ewram[1] = (uint8_t)(values[0] >> 8u);
    instance->machine.ewram[2] = (uint8_t)(values[1] & UINT16_C(0xFF));
    instance->machine.ewram[3] = (uint8_t)(values[1] >> 8u);
    instance->machine.ewram[0x10u] =
        (uint8_t)(values[2] & UINT16_C(0xFF));
    instance->machine.ewram[0x11u] = (uint8_t)(values[2] >> 8u);
    memset(&instance->machine.iwram[0xFEu], 0xEE, 4u);
    memset(&instance->machine.iwram[0x104u], 0xEE, 4u);
    memset(&instance->machine.iwram[0x1FEu], 0xEE, 4u);
    memset(&instance->machine.iwram[0x202u], 0xEE, 4u);
    if (!gbl_phase9_write(&instance->machine, UINT32_C(0x04000204), 2u,
                          UINT16_C(0x0014)) ||
        !gbl_phase9_configure_dma(&instance->machine, 3u,
                                  UINT32_C(0x02000000),
                                  UINT32_C(0x03000100), 2u,
                                  UINT16_C(0x8000)) ||
        !gbl_phase9_configure_dma(&instance->machine, 0u,
                                  UINT32_C(0x02000010),
                                  UINT32_C(0x03000200), 1u,
                                  UINT16_C(0x9000))) {
        *failure = "phase9-register-configuration-failed";
        return false;
    }
    gbl_interleave_seed_live_timer(&instance->machine);
    instance->machine.key_control = UINT16_C(0x4001);
    input = (gbl_input_event){ instance->machine.master_cycles + 4u,
                               UINT16_C(1) };
    if (gbl_machine_set_input_timeline(&instance->machine, &input, 1u) !=
        GBL_TIME_STATUS_OK) {
        *failure = "phase9-input-timeline-setup-failed";
        return false;
    }
    instance->base_cycles = instance->machine.master_cycles;
    instance->budget = UINT64_C(1000);
    ready_cycle = instance->machine.master_cycles;
    if (!instance->machine.dma_channels[3].pending ||
        gbl_machine_dma_request(
            &instance->machine, 0u, ready_cycle + UINT64_C(4),
            instance->machine.dma_channels[0].generation) !=
            GBL_TIME_STATUS_OK) {
        *failure = "phase9-dma-ready-event-setup-failed";
        return false;
    }
    return true;
}

static bool gbl_phase9_reset_instance(gbl_interleave_instance *instance)
{
    if (instance == NULL || !instance->initialized || instance->records == NULL) {
        return false;
    }
    free(instance->serialized_trace);
    instance->serialized_trace = NULL;
    instance->serialized_trace_length = 0u;
    memset(instance->records, 0,
           GBL_INTERLEAVE_TRACE_CAPACITY * sizeof *instance->records);
    instance->trace.count = 0u;
    instance->trace.next_sequence = 0u;
    instance->mailbox = gbl_mailbox_read(&instance->machine);
    instance->termination = GBL_RUN_PROGRESS;
    instance->reason = GBL_RUN_REASON_NONE;
    instance->base_cycles = 0u;
    instance->cycles = 0u;
    instance->instructions = 0u;
    instance->bus_cycles = 0u;
    instance->internal_cycles = 0u;
    instance->wait_cycles = 0u;
    instance->dma_startup_cycles = 0u;
    instance->dma_source_cycles = 0u;
    instance->dma_destination_cycles = 0u;
    instance->dma_completed_units = 0u;
    instance->dma_pause_cycles = 0u;
    instance->wake_cycle = 0u;
    instance->wake_source_mask = 0u;
    instance->power_cause = GBL_CPU_POWER_CAUSE_NONE;
    instance->complete = false;
    return true;
}

static bool gbl_phase9_run_slice(gbl_interleave_instance *instance,
                                uint64_t budget, uint64_t unit_limit,
                                const char **failure)
{
    const gbl_run_options options = {
        budget, unit_limit, GBL_TRACE_CATEGORY_MASK, &instance->trace
    };
    gbl_run_result result = { 0 };
    uint64_t component_cycles;

    if (instance == NULL || !instance->initialized || instance->complete ||
        budget == 0u || failure == NULL ||
        gbl_cpu_run(&instance->machine, &options, &result) !=
            GBL_RUN_STATUS_OK) {
        if (failure != NULL) *failure = "phase9-canonical-run-slice-failed";
        return false;
    }
    if (UINT64_MAX - result.bus_cycles < result.internal_cycles ||
        UINT64_MAX - result.bus_cycles - result.internal_cycles <
            result.wait_cycles) {
        *failure = "phase9-cycle-component-overflow";
        return false;
    }
    component_cycles = result.bus_cycles + result.internal_cycles +
                       result.wait_cycles;
    if (component_cycles != result.cycles ||
        UINT64_MAX - instance->cycles < result.cycles ||
        UINT64_MAX - instance->instructions < result.instructions ||
        UINT64_MAX - instance->bus_cycles < result.bus_cycles ||
        UINT64_MAX - instance->internal_cycles < result.internal_cycles ||
        UINT64_MAX - instance->wait_cycles < result.wait_cycles ||
        UINT64_MAX - instance->dma_startup_cycles <
            result.dma_startup_cycles ||
        UINT64_MAX - instance->dma_source_cycles < result.dma_source_cycles ||
        UINT64_MAX - instance->dma_destination_cycles <
            result.dma_destination_cycles ||
        UINT64_MAX - instance->dma_completed_units <
            result.dma_completed_units ||
        UINT64_MAX - instance->dma_pause_cycles < result.dma_pause_cycles) {
        *failure = "phase9-cycle-accounting-overflow-or-mismatch";
        return false;
    }
    instance->cycles += result.cycles;
    instance->instructions += result.instructions;
    instance->bus_cycles += result.bus_cycles;
    instance->internal_cycles += result.internal_cycles;
    instance->wait_cycles += result.wait_cycles;
    instance->dma_startup_cycles += result.dma_startup_cycles;
    instance->dma_source_cycles += result.dma_source_cycles;
    instance->dma_destination_cycles += result.dma_destination_cycles;
    instance->dma_completed_units += result.dma_completed_units;
    instance->dma_pause_cycles += result.dma_pause_cycles;
    if (instance->base_cycles > UINT64_MAX - instance->cycles ||
        instance->machine.master_cycles !=
            instance->base_cycles + instance->cycles) {
        *failure = "phase9-master-cycle-accounting-mismatch";
        return false;
    }
    instance->termination = result.termination;
    instance->reason = result.reason;
    instance->mailbox = gbl_mailbox_read(&instance->machine);
    if (result.wake_cycle != 0u || result.wake_source_mask != 0u ||
        result.power_cause != GBL_CPU_POWER_CAUSE_NONE) {
        instance->wake_cycle = result.wake_cycle;
        instance->wake_source_mask = result.wake_source_mask;
        instance->power_cause = result.power_cause;
    }
    if (result.termination == GBL_RUN_TRACE_OUTPUT_FULL) {
        *failure = "phase9-trace-capacity-exhausted";
        return false;
    }
    if (instance->mailbox.status == GBL_MAILBOX_PASS ||
        instance->mailbox.status == GBL_MAILBOX_FAIL ||
        result.termination == GBL_RUN_STOPPED) {
        instance->complete = true;
    }
    return true;
}

static bool gbl_phase9_semantics(const gbl_interleave_instance *instance,
                                bool variant)
{
    const uint16_t first = variant ? UINT16_C(0xA1B2) : UINT16_C(0x1234);
    const uint16_t second = variant ? UINT16_C(0xC3D4) : UINT16_C(0x5678);
    const uint16_t high = variant ? UINT16_C(0xE5F6) : UINT16_C(0xABCD);
    const gbl_dma_channel_state *dma0 = &instance->machine.dma_channels[0];
    const gbl_dma_channel_state *dma3 = &instance->machine.dma_channels[3];

#define GBL_PHASE9_REQUIRE(condition, label)                                 \
    do {                                                                     \
        if (!(condition)) return false;                                      \
    } while (0)
    GBL_PHASE9_REQUIRE(instance->complete && instance->machine.cpu.stopped,
                       "synthetic guest did not stop");
    GBL_PHASE9_REQUIRE(instance->machine.cpu.r0_7[7] == UINT32_C(0x42) &&
                           instance->instructions >= UINT64_C(2),
                       "synthetic CPU progress differs");
    GBL_PHASE9_REQUIRE(instance->machine.iwram[0x100u] == (uint8_t)first &&
                           instance->machine.iwram[0x101u] ==
                               (uint8_t)(first >> 8u) &&
                           instance->machine.iwram[0x102u] == (uint8_t)second &&
                           instance->machine.iwram[0x103u] ==
                               (uint8_t)(second >> 8u) &&
                           instance->machine.iwram[0x200u] == (uint8_t)high &&
                           instance->machine.iwram[0x201u] ==
                               (uint8_t)(high >> 8u),
                       "DMA destination values differ");
    GBL_PHASE9_REQUIRE(instance->machine.iwram[0xFEu] == UINT8_C(0xEE) &&
                           instance->machine.iwram[0xFFu] == UINT8_C(0xEE) &&
                           instance->machine.iwram[0x104u] == UINT8_C(0xEE) &&
                           instance->machine.iwram[0x105u] == UINT8_C(0xEE) &&
                           instance->machine.iwram[0x1FEu] == UINT8_C(0xEE) &&
                           instance->machine.iwram[0x1FFu] == UINT8_C(0xEE) &&
                           instance->machine.iwram[0x202u] == UINT8_C(0xEE) &&
                           instance->machine.iwram[0x203u] == UINT8_C(0xEE),
                       "DMA overwrote a neighboring address");
    GBL_PHASE9_REQUIRE(
        dma3->completed_units == UINT64_C(2) && dma3->remaining == 0u &&
            dma3->current_source == UINT32_C(0x02000004) &&
            dma3->current_destination == UINT32_C(0x03000104) &&
            dma3->read_latch ==
                ((uint32_t)second | ((uint32_t)second << 16u)) &&
            dma0->completed_units == UINT64_C(1) && dma0->remaining == 0u &&
            dma0->current_source == UINT32_C(0x02000012) &&
            dma0->current_destination == UINT32_C(0x03000202) &&
            dma0->read_latch == ((uint32_t)high | ((uint32_t)high << 16u)),
        "DMA channel progress or retained latch differs");
    GBL_PHASE9_REQUIRE(instance->machine.bus_owner ==
                               GBL_MACHINE_BUS_OWNER_CPU &&
                           instance->machine.waitcnt == UINT16_C(0x0014),
                       "bus ownership or WAITCNT differs");
    GBL_PHASE9_REQUIRE(
        (instance->machine.interrupt_flags & (UINT16_C(1) << 3u)) != 0u &&
            (instance->machine.interrupt_flags & (UINT16_C(1) << 12u)) !=
                0u &&
            instance->machine.pressed_keys == UINT16_C(1) &&
            instance->machine.key_condition,
        "timer or input event state differs");
    GBL_PHASE9_REQUIRE(instance->dma_startup_cycles == UINT64_C(4) &&
                           instance->dma_completed_units == UINT64_C(3) &&
                           instance->dma_source_cycles == UINT64_C(3) &&
                           instance->dma_destination_cycles == UINT64_C(3) &&
                           instance->dma_pause_cycles == UINT64_C(10),
                       "DMA cost contributions differ");
    GBL_PHASE9_REQUIRE(instance->cycles == instance->bus_cycles +
                                              instance->internal_cycles +
                                              instance->wait_cycles,
                       "run cycle totals do not equal named contributions");
#undef GBL_PHASE9_REQUIRE
    (void)sizeof "synthetic semantic checks use named failure groups";
    return true;
}

static bool gbl_phase9_trace_order(const gbl_interleave_instance *instance)
{
    uint8_t channels[5] = { 0u };
    size_t index;
    size_t count = 0u;

    for (index = 0u; index < instance->trace.count; ++index) {
        const gbl_trace_record *record = &instance->trace.records[index];

        if (record->category == GBL_TRACE_DMA &&
            record->dma_status == GBL_DMA_STATUS_OK) {
            if (count < sizeof channels) channels[count] = record->dma_channel;
            ++count;
        }
    }
    return count >= 5u && channels[0] == 3u && channels[1] == 3u &&
           channels[2] == 0u && channels[3] == 0u && channels[4] == 3u;
}

static const char *gbl_phase9_observation_compare(
    const gbl_interleave_instance *expected,
    const gbl_interleave_instance *actual)
{
    const size_t destination_offsets[] = {
        0x100u, 0x101u, 0x102u, 0x103u, 0x200u, 0x201u
    };
    size_t index;

    for (index = 0u;
         index < sizeof destination_offsets / sizeof destination_offsets[0];
         ++index) {
        const size_t offset = destination_offsets[index];
        if (expected->machine.iwram[offset] != actual->machine.iwram[offset]) {
            return "dma-transfer-value";
        }
    }
    if (expected->cycles != expected->bus_cycles + expected->internal_cycles +
                               expected->wait_cycles ||
        actual->cycles != actual->bus_cycles + actual->internal_cycles +
                              actual->wait_cycles ||
        expected->cycles != actual->cycles ||
        expected->bus_cycles != actual->bus_cycles ||
        expected->internal_cycles != actual->internal_cycles ||
        expected->wait_cycles != actual->wait_cycles ||
        expected->dma_startup_cycles != actual->dma_startup_cycles ||
        expected->dma_source_cycles != actual->dma_source_cycles ||
        expected->dma_destination_cycles != actual->dma_destination_cycles ||
        expected->dma_completed_units != actual->dma_completed_units ||
        expected->dma_pause_cycles != actual->dma_pause_cycles) {
        return "dma-cycle-accounting";
    }
    if (expected->trace.count != actual->trace.count ||
        !gbl_test_trace_equal(expected->trace.records, actual->trace.records,
                              expected->trace.count)) {
        return "dma-event-order";
    }
    if (!gbl_test_canonical_equal(&expected->machine, &actual->machine) ||
        gbl_test_canonical_digest(&expected->machine) !=
            gbl_test_canonical_digest(&actual->machine)) {
        return "dma-state-contamination";
    }
    if (expected->instructions != actual->instructions ||
        expected->termination != actual->termination ||
        expected->reason != actual->reason ||
        expected->mailbox.status != actual->mailbox.status ||
        expected->mailbox.case_id != actual->mailbox.case_id) {
        return "dma-run-outcome";
    }
    return "ok";
}

static bool gbl_phase9_canonical_field_controls(const gbl_machine *clean)
{
    gbl_machine mutant;
    size_t channel_index;

#define GBL_PHASE9_MACHINE_MUTATION(statement)                               \
    do {                                                                     \
        mutant = *clean;                                                     \
        statement;                                                           \
        if (gbl_test_canonical_equal(clean, &mutant) ||                      \
            gbl_test_canonical_digest(clean) ==                              \
                gbl_test_canonical_digest(&mutant)) {                        \
            return false;                                                    \
        }                                                                    \
    } while (0)
#define GBL_PHASE9_CHANNEL_MUTATION(member)                                  \
    do {                                                                     \
        for (channel_index = 0u; channel_index < GBL_DMA_CHANNEL_COUNT;      \
             ++channel_index) {                                              \
            mutant = *clean;                                                 \
            mutant.dma_channels[channel_index].member ^= 1u;                 \
            if (gbl_test_canonical_equal(clean, &mutant) ||                  \
                gbl_test_canonical_digest(clean) ==                          \
                    gbl_test_canonical_digest(&mutant)) {                    \
                return false;                                                \
            }                                                                \
        }                                                                    \
    } while (0)
#define GBL_PHASE9_EVENT_MUTATION(member)                                    \
    do {                                                                     \
        mutant = *clean;                                                     \
        mutant.events[0].member ^= 1u;                                       \
        if (gbl_test_canonical_equal(clean, &mutant) ||                      \
            gbl_test_canonical_digest(clean) ==                              \
                gbl_test_canonical_digest(&mutant)) {                        \
            return false;                                                    \
        }                                                                    \
    } while (0)
    if (clean == NULL) return false;
    GBL_PHASE9_MACHINE_MUTATION(mutant.waitcnt ^= 1u);
    GBL_PHASE9_MACHINE_MUTATION(
        mutant.bus_owner = mutant.bus_owner == GBL_MACHINE_BUS_OWNER_CPU
                               ? GBL_MACHINE_BUS_OWNER_DMA
                               : GBL_MACHINE_BUS_OWNER_CPU);
    GBL_PHASE9_MACHINE_MUTATION(mutant.dma_last_channel ^= 1u);
    GBL_PHASE9_MACHINE_MUTATION(mutant.dma_last_width ^= 1u);
    GBL_PHASE9_MACHINE_MUTATION(mutant.dma_last_source ^= 1u);
    GBL_PHASE9_MACHINE_MUTATION(mutant.dma_last_destination ^= 1u);
    GBL_PHASE9_MACHINE_MUTATION(mutant.dma_stream_valid = !mutant.dma_stream_valid);
    GBL_PHASE9_MACHINE_MUTATION(mutant.next_event_sequence ^= 1u);
    GBL_PHASE9_MACHINE_MUTATION(mutant.event_count ^= 1u);
    GBL_PHASE9_MACHINE_MUTATION(mutant.input_generation ^= 1u);
    GBL_PHASE9_MACHINE_MUTATION(mutant.input_event_count ^= 1u);
    GBL_PHASE9_MACHINE_MUTATION(mutant.input_event_cursor ^= 1u);
    GBL_PHASE9_CHANNEL_MUTATION(source_address);
    GBL_PHASE9_CHANNEL_MUTATION(destination_address);
    GBL_PHASE9_CHANNEL_MUTATION(current_source);
    GBL_PHASE9_CHANNEL_MUTATION(current_destination);
    GBL_PHASE9_CHANNEL_MUTATION(read_latch);
    GBL_PHASE9_CHANNEL_MUTATION(completed_units);
    GBL_PHASE9_CHANNEL_MUTATION(generation);
    GBL_PHASE9_CHANNEL_MUTATION(count);
    GBL_PHASE9_CHANNEL_MUTATION(control);
    GBL_PHASE9_CHANNEL_MUTATION(remaining);
    GBL_PHASE9_CHANNEL_MUTATION(pending);
    GBL_PHASE9_CHANNEL_MUTATION(armed);
    GBL_PHASE9_CHANNEL_MUTATION(active);
    GBL_PHASE9_CHANNEL_MUTATION(startup_complete);
    GBL_PHASE9_EVENT_MUTATION(cycle);
    GBL_PHASE9_EVENT_MUTATION(sequence);
    GBL_PHASE9_EVENT_MUTATION(generation);
    GBL_PHASE9_EVENT_MUTATION(priority);
    GBL_PHASE9_EVENT_MUTATION(kind);
    GBL_PHASE9_EVENT_MUTATION(timer_index);
    GBL_PHASE9_EVENT_MUTATION(dma_channel);
    GBL_PHASE9_EVENT_MUTATION(live);
    GBL_PHASE9_EVENT_MUTATION(input_index);
#undef GBL_PHASE9_EVENT_MUTATION
#undef GBL_PHASE9_CHANNEL_MUTATION
#undef GBL_PHASE9_MACHINE_MUTATION
    return true;
}

static bool gbl_phase9_trace_field_controls(void)
{
    gbl_trace_record clean = { 0 };
    gbl_trace_record mutant;

#define GBL_PHASE9_TRACE_MUTATION(member)                                    \
    do {                                                                     \
        mutant = clean;                                                      \
        mutant.member ^= 1u;                                                 \
        if (gbl_test_trace_equal(&clean, &mutant, 1u)) return false;         \
    } while (0)
    GBL_PHASE9_TRACE_MUTATION(bus_owner);
    GBL_PHASE9_TRACE_MUTATION(dma_status);
    GBL_PHASE9_TRACE_MUTATION(dma_channel);
    GBL_PHASE9_TRACE_MUTATION(dma_startup_cycles);
    GBL_PHASE9_TRACE_MUTATION(dma_source_cycles);
    GBL_PHASE9_TRACE_MUTATION(dma_destination_cycles);
    GBL_PHASE9_TRACE_MUTATION(dma_source_sequence);
    GBL_PHASE9_TRACE_MUTATION(dma_destination_sequence);
    GBL_PHASE9_TRACE_MUTATION(dma_remaining);
    GBL_PHASE9_TRACE_MUTATION(dma_completed);
    GBL_PHASE9_TRACE_MUTATION(dma_used_channel_latch);
    GBL_PHASE9_TRACE_MUTATION(dma_value_provenance);
    GBL_PHASE9_TRACE_MUTATION(dma_result_classification);
    GBL_PHASE9_TRACE_MUTATION(dma_dispute);
    GBL_PHASE9_TRACE_MUTATION(dma_hardware_qualification);
    GBL_PHASE9_TRACE_MUTATION(dma_source_address);
    GBL_PHASE9_TRACE_MUTATION(dma_destination_address);
    GBL_PHASE9_TRACE_MUTATION(dma_value);
    GBL_PHASE9_TRACE_MUTATION(dma_width);
#undef GBL_PHASE9_TRACE_MUTATION
    return true;
}

static bool gbl_phase9_boundary_case(uint16_t waitcnt,
                                    uint64_t *cycles_out,
                                    uint64_t *source_cycles_out)
{
    const size_t image_length = 0x20002u;
    uint8_t *image = malloc(image_length);
    gbl_interleave_instance instance = { 0 };
    const char *failure = "phase9-boundary-setup-failed";
    size_t index;
    size_t source_records = 0u;
    uint64_t observed_source_cycles = 0u;
    bool okay = false;

    if (image == NULL) return false;
    for (index = 0u; index < image_length; ++index) {
        image[index] = (uint8_t)((index * 13u + 7u) & 0xFFu);
    }
    image[0] = UINT8_C(0x42);
    image[1] = UINT8_C(0x70);
    image[2] = UINT8_C(0xA0);
    image[3] = UINT8_C(0xE3);
    image[4] = 0u;
    image[5] = 0u;
    image[6] = 0u;
    image[7] = UINT8_C(0xEF);
    image[0x1FFFEu] = UINT8_C(0x34);
    image[0x1FFFFu] = UINT8_C(0x12);
    image[0x20000u] = UINT8_C(0x78);
    image[0x20001u] = UINT8_C(0x56);
    if (!gbl_interleave_load(&instance, image, image_length,
                             UINT64_C(1000)) ||
        !gbl_phase9_write(&instance.machine, UINT32_C(0x04000204), 2u,
                          waitcnt)) {
        goto done;
    }
    memset(&instance.machine.ewram[0x3FEu], 0xEE, 8u);
    if (!gbl_phase9_configure_dma(&instance.machine, 3u,
                                  UINT32_C(0x0801FFFE),
                                  UINT32_C(0x02000400), 2u,
                                  UINT16_C(0x8000))) {
        goto done;
    }
    instance.base_cycles = instance.machine.master_cycles;
    instance.budget = UINT64_C(1000);
    if (!instance.machine.dma_channels[3].pending ||
        !gbl_phase9_run_slice(&instance, UINT64_C(1000), 0u, &failure) ||
        !instance.complete || !instance.machine.cpu.stopped ||
        instance.machine.ewram[0x400u] != UINT8_C(0x34) ||
        instance.machine.ewram[0x401u] != UINT8_C(0x12) ||
        instance.machine.ewram[0x402u] != UINT8_C(0x78) ||
        instance.machine.ewram[0x403u] != UINT8_C(0x56) ||
        instance.machine.ewram[0x3FEu] != UINT8_C(0xEE) ||
        instance.machine.ewram[0x3FFu] != UINT8_C(0xEE) ||
        instance.machine.ewram[0x404u] != UINT8_C(0xEE) ||
        instance.machine.ewram[0x405u] != UINT8_C(0xEE) ||
        instance.dma_completed_units != UINT64_C(2) ||
        instance.dma_startup_cycles != GBL_DMA_STARTUP_CYCLES ||
        instance.cycles != instance.bus_cycles + instance.internal_cycles +
                               instance.wait_cycles) {
        goto done;
    }
    for (index = 0u; index < instance.trace.count; ++index) {
        const gbl_trace_record *record = &instance.trace.records[index];

        if (record->category == GBL_TRACE_DMA &&
            record->dma_channel == 3u &&
            record->dma_source_address >= UINT32_C(0x0801FFFE) &&
            record->dma_source_address <= UINT32_C(0x08020000)) {
            const uint32_t expected_source =
                source_records == 0u ? UINT32_C(0x0801FFFE)
                                     : UINT32_C(0x08020000);
            const gbl_bus_access_class requested_sequence =
                source_records == 0u ? GBL_BUS_CLASS_NONSEQUENTIAL
                                     : GBL_BUS_CLASS_SEQUENTIAL;
            gbl_cycle_cost observed_cost = { 0 };
            gbl_phase9_bus_observation observation = { 0 };

            if (record->dma_source_address != expected_source ||
                !gbl_bus_quote_observed(
                    &instance.machine, record->dma_source_address,
                    GBL_BUS_DIRECTION_READ, 2u, requested_sequence,
                    GBL_BUS_ROLE_DATA, &observed_cost, &observation) ||
                record->dma_source_cycles != observed_cost.cycles ||
                observation.beat_sequences[0] !=
                    GBL_BUS_CLASS_NONSEQUENTIAL ||
                observation.hardware_qualified ||
                observation.prefetch_modeled ||
                (source_records == 0u &&
                 observation.boundary_provenance !=
                     GBL_BUS_BOUNDARY_PROVENANCE_NONE) ||
                (source_records == 1u &&
                 observation.boundary_provenance !=
                     GBL_BUS_BOUNDARY_PROVENANCE_GBATEK_3_06) ||
                UINT64_MAX - observed_source_cycles < observed_cost.cycles) {
                goto done;
            }
            observed_source_cycles += observed_cost.cycles;
            ++source_records;
        }
    }
    if (source_records != 2u ||
        observed_source_cycles != instance.dma_source_cycles) {
        goto done;
    }
    *cycles_out = instance.cycles;
    *source_cycles_out = instance.dma_source_cycles;
    okay = true;

done:
    if (!okay) {
        fprintf(stderr,
                "phase9 boundary diagnostic: waitcnt=%04x cycles=%" PRIu64
                " source=%" PRIu64 " units=%" PRIu64 " bytes=%02x%02x%02x%02x "
                "trace=%zu failure=%s\n",
                (unsigned int)waitcnt, instance.cycles,
                instance.dma_source_cycles, instance.dma_completed_units,
                instance.machine.ewram[0x403u],
                instance.machine.ewram[0x402u],
                instance.machine.ewram[0x401u],
                instance.machine.ewram[0x400u], instance.trace.count, failure);
        for (index = 0u; index < instance.trace.count; ++index) {
            const gbl_trace_record *record = &instance.trace.records[index];
            if (record->category == GBL_TRACE_DMA) {
                fprintf(stderr, "boundary trace[%zu]=addr=%08" PRIx32
                                " seq=%u done=%" PRIu64 " status=%u\n",
                        index, record->dma_source_address,
                        (unsigned int)record->dma_source_sequence,
                        record->dma_completed,
                        (unsigned int)record->dma_status);
            }
        }
    }
    (void)gbl_interleave_dispose(&instance);
    free(image);
    return okay;
}

static int gbl_test_interleave_controls_main(void)
{
    gbl_test_bytes pass_image = { 0 };
    gbl_test_bytes thumb_image = { 0 };
    gbl_test_bytes loop_image = { 0 };
    gbl_test_bytes stop_image = { 0 };
    gbl_interleave_pair pair = { 0 };
    const char *failure = "interleave-control-fixture-load-failed";
    bool passed = false;
    bool pair_disposed;

    if (!gbl_interleave_fixture(&pass_image, "owned-arm-pass.gba") ||
        !gbl_interleave_fixture(&thumb_image,
                                "owned-arm-thumb-bx-pass.gba") ||
        !gbl_interleave_fixture(&loop_image, "owned-arm-loop.gba") ||
        !gbl_interleave_fixture(&stop_image, "owned-arm-swi.gba")) {
        goto done;
    }
    if (thumb_image.length <= 0x17Fu ||
        thumb_image.data[0x17Cu] != UINT8_C(2) ||
        thumb_image.data[0x17Du] != 0u || thumb_image.data[0x17Eu] != 0u ||
        thumb_image.data[0x17Fu] != 0u) {
        failure = "owned-thumb-fixture-result-literal-changed";
        goto done;
    }
    thumb_image.data[0x17Cu] = UINT8_C(3);

    /* Clean pass/fail companion pair, followed by a trace copied from the
     * other instance. The pair totals stay fixed while per-instance evidence
     * changes, and the comparator must report the trace-specific mismatch. */
    if (!gbl_interleave_pair_prepare(&pair, pass_image.data,
                                     pass_image.length, UINT64_C(1000),
                                     thumb_image.data, thumb_image.length,
                                     UINT64_C(1000), &failure)) {
        goto done;
    }
    if (pair.isolated_first.mailbox.status != GBL_MAILBOX_PASS ||
        pair.isolated_second.mailbox.status != GBL_MAILBOX_FAIL ||
        !gbl_interleave_pair_totals_equal(&pair)) {
        failure = "pass-fail-pair-or-aggregate-totals-invalid";
        goto done;
    }
    {
        size_t different_record = 0u;
        gbl_trace_record original;
        bool copied_trace_rejected;
        bool clean_companion_restored;

        if (!gbl_interleave_trace_has_difference(&pair.alternating_first,
                                                 &pair.alternating_second,
                                                 &different_record)) {
            failure = "pass-fail-traces-have-no-contamination-witness";
            goto done;
        }
        original = pair.alternating_second.records[different_record];
        pair.alternating_second.records[different_record] =
            pair.alternating_first.records[different_record];
        if (!gbl_interleave_pair_totals_equal(&pair) ||
            !gbl_interleave_serialize_trace(&pair.alternating_second)) {
            failure = "trace-contamination-control-setup-failed";
            goto done;
        }
        copied_trace_rejected = strcmp(
            gbl_interleave_compare(&pair.isolated_second,
                                   &pair.alternating_second),
            "interleave-trace-mismatch") == 0;
        pair.alternating_second.records[different_record] = original;
        if (!gbl_interleave_serialize_trace(&pair.alternating_second)) {
            failure = "trace-contamination-companion-restore-failed";
            goto done;
        }
        clean_companion_restored =
            strcmp(gbl_interleave_compare(&pair.isolated_first,
                                          &pair.alternating_first), "ok") == 0 &&
            strcmp(gbl_interleave_compare(&pair.isolated_second,
                                          &pair.alternating_second), "ok") ==
                0;
        if (!copied_trace_rejected || !clean_companion_restored ||
            !gbl_interleave_pair_totals_equal(&pair)) {
            failure = "trace-contamination-not-specific-or-clean-control-failed";
            goto done;
        }
    }
    gbl_interleave_seed_live_timer(&pair.isolated_first.machine);
    gbl_interleave_seed_live_timer(&pair.alternating_first.machine);
    pair.alternating_first.machine.timers[0].generation = UINT64_C(2);
    pair.alternating_first.machine.events[0].generation = UINT64_C(2);
    if (!gbl_interleave_trace_fields_equal(
            pair.isolated_first.trace.records,
            pair.alternating_first.trace.records,
            pair.isolated_first.trace.count) ||
        strcmp(gbl_interleave_compare(&pair.isolated_first,
                                      &pair.alternating_first),
               "interleave-state-mismatch") != 0) {
        failure = "live-timer-event-state-mutation-was-accepted";
        goto done;
    }
    if (!gbl_interleave_pair_dispose(&pair)) {
        failure = "pass-fail-pair-disposal-failed";
        goto done;
    }

    /* Different per-instance budgets end as a stopped guest and a still-
     * running guest whose own cycle budget expires. */
    if (!gbl_interleave_pair_prepare(&pair, stop_image.data,
                                     stop_image.length, UINT64_C(1000),
                                     loop_image.data, loop_image.length,
                                     UINT64_C(31), &failure)) {
        goto done;
    }
    if (pair.isolated_first.budget == pair.isolated_second.budget ||
        pair.isolated_first.termination != GBL_RUN_STOPPED ||
        !pair.isolated_first.machine.cpu.stopped ||
        pair.isolated_second.termination != GBL_RUN_TIMEOUT ||
        pair.isolated_second.machine.cpu.stopped ||
        pair.alternating_first.termination != GBL_RUN_STOPPED ||
        pair.alternating_second.termination != GBL_RUN_TIMEOUT) {
        failure = "stopped-running-unequal-budget-pair-invalid";
        goto done;
    }
    if (!gbl_interleave_pair_dispose(&pair)) {
        failure = "stopped-running-pair-disposal-failed";
        goto done;
    }

    /* One machine starts a loop, then resets and reloads the pass image while
     * its partner is partway through the Thumb-bearing failure stream. */
    if (!gbl_interleave_isolated_prepare(&pair.isolated_first,
                                         pass_image.data, pass_image.length,
                                         UINT64_C(1000), &failure) ||
        !gbl_interleave_isolated_prepare(&pair.isolated_second,
                                         thumb_image.data, thumb_image.length,
                                         UINT64_C(1000), &failure) ||
        !gbl_interleave_load(&pair.alternating_first, loop_image.data,
                             loop_image.length, UINT64_C(31)) ||
        !gbl_interleave_load(&pair.alternating_second, thumb_image.data,
                             thumb_image.length, UINT64_C(1000))) {
        failure = "reset-reload-pair-initialization-failed";
        goto done;
    }
    if (!gbl_interleave_step(&pair.alternating_first, &failure) ||
        !gbl_interleave_step(&pair.alternating_second, &failure)) {
        goto done;
    }
    {
        const gbl_machine partner_before_reset =
            pair.alternating_second.machine;
        const size_t partner_trace_count =
            pair.alternating_second.trace.count;
        const uint64_t partner_cycles = pair.alternating_second.cycles;
        const uint64_t partner_instructions =
            pair.alternating_second.instructions;

        if (partner_cycles == 0u ||
            !gbl_interleave_reset_reload(&pair.alternating_first,
                                         pass_image.data, pass_image.length,
                                         UINT64_C(1000))) {
            failure = "reset-reload-action-failed";
            goto done;
        }
        if (!gbl_interleave_machine_equal(&partner_before_reset,
                                          &pair.alternating_second.machine) ||
            pair.alternating_second.trace.count != partner_trace_count ||
            pair.alternating_second.cycles != partner_cycles ||
            pair.alternating_second.instructions != partner_instructions) {
            failure = "reset-reload-changed-partner-observation";
            goto done;
        }
    }
    if (!gbl_interleave_run_alternating(&pair.alternating_first,
                                        &pair.alternating_second, &failure) ||
        !gbl_interleave_serialize_trace(&pair.alternating_first) ||
        !gbl_interleave_serialize_trace(&pair.alternating_second)) {
        goto done;
    }
    if (strcmp(gbl_interleave_compare(&pair.isolated_first,
                                      &pair.alternating_first), "ok") != 0 ||
        strcmp(gbl_interleave_compare(&pair.isolated_second,
                                      &pair.alternating_second), "ok") != 0 ||
        pair.alternating_first.budget != UINT64_C(1000)) {
        failure = "reset-reload-isolation-comparison-failed";
        goto done;
    }
    if (!gbl_interleave_pair_dispose(&pair)) {
        failure = "reset-reload-pair-disposal-failed";
        goto done;
    }

    /* The short-lived stopped instance is compared, disposed, and then the
     * surviving pass guest continues alone to its isolated terminal result. */
    if (!gbl_interleave_isolated_prepare(&pair.isolated_first,
                                         pass_image.data, pass_image.length,
                                         UINT64_C(1000), &failure) ||
        !gbl_interleave_isolated_prepare(&pair.isolated_second,
                                         stop_image.data, stop_image.length,
                                         UINT64_C(1000), &failure) ||
        !gbl_interleave_load(&pair.alternating_first, pass_image.data,
                             pass_image.length, UINT64_C(1000)) ||
        !gbl_interleave_load(&pair.alternating_second, stop_image.data,
                             stop_image.length, UINT64_C(1000))) {
        failure = "dispose-continue-pair-initialization-failed";
        goto done;
    }
    {
        unsigned int unit;

        for (unit = 0u; unit < GBL_INTERLEAVE_MAX_UNITS &&
                        !pair.alternating_second.complete;
             ++unit) {
            if (!gbl_interleave_step(&pair.alternating_first, &failure) ||
                !gbl_interleave_step(&pair.alternating_second, &failure)) {
                goto done;
            }
        }
        if (!pair.alternating_second.complete ||
            pair.alternating_first.complete ||
            pair.alternating_second.termination != GBL_RUN_STOPPED ||
            !gbl_interleave_serialize_trace(&pair.alternating_second) ||
            strcmp(gbl_interleave_compare(&pair.isolated_second,
                                          &pair.alternating_second), "ok") !=
                0) {
            failure = "disposed-partner-did-not-stop-before-survivor";
            goto done;
        }
    }
    if (!gbl_interleave_dispose(&pair.alternating_second)) {
        failure = "disposed-partner-postcondition-failed";
        goto done;
    }
    if (!gbl_interleave_run_isolated(&pair.alternating_first, &failure) ||
        !gbl_interleave_serialize_trace(&pair.alternating_first) ||
        strcmp(gbl_interleave_compare(&pair.isolated_first,
                                      &pair.alternating_first), "ok") != 0) {
        failure = "survivor-changed-after-partner-disposal";
        goto done;
    }
    if (!gbl_interleave_pair_dispose(&pair)) {
        failure = "dispose-continue-pair-disposal-failed";
        goto done;
    }
    passed = true;

done:
    pair_disposed = gbl_interleave_pair_dispose(&pair);
    free(pass_image.data);
    free(thumb_image.data);
    free(loop_image.data);
    free(stop_image.data);
    if (!pair_disposed) {
        failure = "machine-disposal-postcondition-failed";
        passed = false;
    }
    if (passed) {
        puts("PASS determinism.interleave.controls: pass/fail, stopped/running, unequal budgets, reset/reload, dispose/continue and copied-trace mismatch preserve per-instance isolation");
        return 0;
    }
    fprintf(stderr, "FAIL determinism.interleave.controls: %s\n", failure);
    return 1;
}

static bool gbl_test_timer_cascade_setup(gbl_machine *machine)
{
    static const struct {
        uint8_t timer_index;
        uint16_t reload;
        uint8_t control;
    } writes[] = {
        { 3u, UINT16_MAX, UINT8_C(0xC4) },
        { 2u, UINT16_MAX, UINT8_C(0xC4) },
        { 1u, UINT16_MAX, UINT8_C(0x84) },
        { 0u, UINT16_C(0xFFFE), UINT8_C(0xC0) }
    };
    size_t index;

    if (machine == NULL) return false;
    for (index = 0u; index < sizeof writes / sizeof writes[0]; ++index) {
        const uint32_t address =
            UINT32_C(0x04000100) +
            (uint32_t)writes[index].timer_index * UINT32_C(4);
        const uint32_t value =
            ((uint32_t)writes[index].control << 16u) |
            writes[index].reload;
        const gbl_bus_result result = gbl_bus_write(
            machine, address, 4u, GBL_BUS_CLASS_NONSEQUENTIAL, value);

        if (result.status != GBL_BUS_STATUS_OK) return false;
    }
    return gbl_machine_advance_to(machine, UINT64_C(6)) ==
               GBL_TIME_STATUS_OK &&
           machine->master_cycles == UINT64_C(6) &&
           machine->interrupt_flags == UINT16_C(0x0068) &&
           machine->timers[1].cascade && machine->timers[2].cascade &&
           machine->timers[3].cascade &&
           machine->timers[1].counter == UINT16_MAX &&
           machine->timers[2].counter == UINT16_MAX &&
           machine->timers[3].counter == UINT16_MAX &&
           machine->event_count == 1u && machine->events[0].live &&
           machine->events[0].timer_index == 0u &&
           machine->cpu.last_exception.kind == GBL_EXCEPTION_NONE &&
           !machine->cpu.stopped;
}

static bool gbl_test_timer_repeat_and_cascade(const char **failure)
{
    gbl_test_bytes timer0_image = { 0 };
    gbl_interleave_pair timer0_pair = { 0 };
    gbl_interleave_instance fresh_machine = { 0 };
    gbl_machine cascade_first = { 0 };
    gbl_machine cascade_second = { 0 };
    gbl_machine cascade_initial = { 0 };
    const gbl_allocator allocator = {
        NULL, gbl_interleave_allocate, gbl_interleave_release
    };
    bool cascade_first_initialized = false;
    bool cascade_second_initialized = false;
    bool cascade_initial_initialized = false;
    bool passed = false;

    *failure = "timer0-repeat-or-cascade-failed";
    if (!gbl_interleave_fixture(&timer0_image, "owned-timer0-pass.gba")) {
        *failure = "timer0-repeat-fixture-load-failed";
        goto cleanup;
    }
    if (!gbl_interleave_pair_prepare(
            &timer0_pair, timer0_image.data, timer0_image.length,
            UINT64_C(10000), timer0_image.data, timer0_image.length,
            UINT64_C(10000), failure)) {
        goto cleanup;
    }
    if (!gbl_interleave_timer0_complete(&timer0_pair.isolated_first) ||
        !gbl_interleave_timer0_complete(&timer0_pair.isolated_second) ||
        !gbl_interleave_timer0_complete(&timer0_pair.alternating_first) ||
        !gbl_test_time_state_equal(&timer0_pair.isolated_first.machine,
                                   &timer0_pair.isolated_second.machine) ||
        strcmp(gbl_interleave_compare(&timer0_pair.isolated_first,
                                      &timer0_pair.isolated_second),
               "ok") != 0) {
        *failure = "timer0-fresh-observations-differ";
        goto cleanup;
    }
    if (!gbl_interleave_load(&fresh_machine, timer0_image.data,
                             timer0_image.length, UINT64_C(10000))) {
        *failure = "timer0-fresh-reset-companion-load-failed";
        goto cleanup;
    }
    if (!gbl_interleave_reset_reload(&timer0_pair.alternating_first,
                                     timer0_image.data, timer0_image.length,
                                     UINT64_C(10000)) ||
        !gbl_test_canonical_equal(&fresh_machine.machine,
                                  &timer0_pair.alternating_first.machine) ||
        !gbl_test_time_state_equal(&fresh_machine.machine,
                                   &timer0_pair.alternating_first.machine) ||
        !gbl_interleave_run_isolated(&timer0_pair.alternating_first,
                                     failure) ||
        !gbl_interleave_serialize_trace(&timer0_pair.alternating_first) ||
        strcmp(gbl_interleave_compare(&timer0_pair.isolated_first,
                                      &timer0_pair.alternating_first),
               "ok") != 0) {
        *failure = "timer0-reset-rerun-differs-from-fresh";
        goto cleanup;
    }

    if (gbl_machine_init(&cascade_first, &allocator) != GBL_BUS_STATUS_OK) {
        *failure = "cascade-first-machine-init-failed";
        goto cleanup;
    }
    cascade_first_initialized = true;
    if (gbl_machine_init(&cascade_second, &allocator) != GBL_BUS_STATUS_OK) {
        *failure = "cascade-second-machine-init-failed";
        goto cleanup;
    }
    cascade_second_initialized = true;
    if (gbl_machine_init(&cascade_initial, &allocator) != GBL_BUS_STATUS_OK) {
        *failure = "cascade-initial-machine-init-failed";
        goto cleanup;
    }
    cascade_initial_initialized = true;
    if (!gbl_test_timer_cascade_setup(&cascade_first) ||
        !gbl_test_timer_cascade_setup(&cascade_second) ||
        !gbl_test_time_state_equal(&cascade_first, &cascade_second) ||
        !gbl_test_canonical_equal(&cascade_first, &cascade_second) ||
        gbl_test_canonical_digest(&cascade_first) !=
            gbl_test_canonical_digest(&cascade_second)) {
        *failure = "cascade-fresh-observations-differ";
        goto cleanup;
    }
    gbl_machine_reset(&cascade_second);
    if (!gbl_test_canonical_equal(&cascade_initial, &cascade_second) ||
        !gbl_test_time_state_equal(&cascade_initial, &cascade_second) ||
        !gbl_test_timer_cascade_setup(&cascade_second) ||
        !gbl_test_canonical_equal(&cascade_first, &cascade_second) ||
        gbl_test_canonical_digest(&cascade_first) !=
            gbl_test_canonical_digest(&cascade_second)) {
        *failure = "cascade-reset-rerun-differs-from-fresh";
        goto cleanup;
    }
    passed = true;

cleanup:
    if (!gbl_interleave_dispose(&fresh_machine)) {
        *failure = "timer0-fresh-machine-disposal-failed";
        passed = false;
    }
    if (!gbl_interleave_pair_dispose(&timer0_pair)) {
        *failure = "timer0-pair-disposal-failed";
        passed = false;
    }
    if (cascade_first_initialized) gbl_machine_dispose(&cascade_first);
    if (cascade_second_initialized) gbl_machine_dispose(&cascade_second);
    if (cascade_initial_initialized) gbl_machine_dispose(&cascade_initial);
    free(timer0_image.data);
    if (passed) return true;
    if (*failure == NULL) *failure = "timer0-repeat-or-cascade-failed";
    return false;
}

static int gbl_test_repeat_main(void)
{
    char directory_template[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char execution_trace_a[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char execution_trace_b[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char serialized_trace_a[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char serialized_trace_b[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    gbl_test_observation first = { 0 };
    gbl_test_observation second = { 0 };
    const char *failure = "temporary-directory-create-failed";
    bool directory_created = false;
    bool okay = false;

    if (!gbl_test_timer_repeat_and_cascade(&failure)) goto done;

    if (!gbl_test_create_directory(directory_template,
                                   sizeof directory_template)) {
        goto done;
    }
    directory_created = true;
    if (!gbl_test_path(execution_trace_a, sizeof execution_trace_a,
                       directory_template, "first.execution.jsonl") ||
        !gbl_test_path(execution_trace_b, sizeof execution_trace_b,
                       directory_template, "second.execution.jsonl") ||
        !gbl_test_path(serialized_trace_a, sizeof serialized_trace_a,
                       directory_template, "first.serialized.jsonl") ||
        !gbl_test_path(serialized_trace_b, sizeof serialized_trace_b,
                       directory_template, "second.serialized.jsonl")) {
        failure = "temporary-path-invalid";
        goto done;
    }
    if (!gbl_test_observe(directory_template, "first.execution.jsonl",
                          "first.serialized.jsonl", &first, &failure) ||
        !gbl_test_observe(directory_template, "second.execution.jsonl",
                          "second.serialized.jsonl", &second, &failure) ||
        !gbl_test_equal(&first, &second, &failure)) {
        goto done;
    }
    okay = true;

done:
    gbl_test_dispose(&first);
    gbl_test_dispose(&second);
    if (directory_created) {
        (void)unlink(execution_trace_a);
        (void)unlink(execution_trace_b);
        (void)unlink(serialized_trace_a);
        (void)unlink(serialized_trace_b);
        (void)rmdir(directory_template);
    }
    if (okay) {
        puts("PASS determinism.repeat.in_process: equivalent fresh runner executions produced identical complete JSON and trace bytes");
        return 0;
    }
    fprintf(stderr, "FAIL determinism.repeat.in_process: %s\n", failure);
    return 1;
}

enum {
    GBL_MATRIX_MAX_ROWS = 1024u,
    GBL_MATRIX_LINE_CAPACITY = 4096u,
    GBL_MATRIX_RECORD_CAPACITY = 2048u,
    GBL_MATRIX_CYCLE_BUDGET = 1000u,
    GBL_MATRIX_TRACE_CAPACITY = 4096u
};

typedef struct gbl_matrix_row {
    char id[GBL_RUNNER_FIXTURE_ID_CAPACITY];
    char relative_path[GBL_DETERMINISM_PATH_CAPACITY];
    char group[16];
    char digest[GBL_SHA256_HEX_CAPACITY];
    char expected_status[24];
    char expected_reason[96];
    char expected_semantic[16];
    uint32_t expected_case_id;
    uint64_t expected_cycles;
    uint64_t expected_instructions;
} gbl_matrix_row;

static bool gbl_matrix_copy(char *output, size_t capacity, const char *input)
{
    int written;

    if (input == NULL) return false;
    written = snprintf(output, capacity, "%s", input);
    return written >= 0 && (size_t)written < capacity;
}

static bool gbl_matrix_parse_u64(const char *text, uint64_t *output)
{
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || text[0] == '\0' || output == NULL) return false;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return false;
    *output = (uint64_t)value;
    return (unsigned long long)*output == value;
}

static bool gbl_matrix_sha256_text(const char *value)
{
    size_t index;

    if (value == NULL || strlen(value) != 64u) return false;
    for (index = 0u; index < 64u; ++index) {
        const char character = value[index];
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

static bool gbl_matrix_safe_relative_path(const char *path)
{
    const char *cursor;
    const char *component = path;

    if (path == NULL || path[0] == '\0' || path[0] == '/') return false;
    for (cursor = path; ; ++cursor) {
        const unsigned char character = (unsigned char)*cursor;
        if (character == '\0' || character == '/') {
            const size_t length = (size_t)(cursor - component);
            if (length == 0u ||
                (length == 1u && component[0] == '.') ||
                (length == 2u && component[0] == '.' &&
                 component[1] == '.')) {
                return false;
            }
            if (character == '\0') break;
            component = cursor + 1;
        } else if (!((character >= 'a' && character <= 'z') ||
                     (character >= 'A' && character <= 'Z') ||
                     (character >= '0' && character <= '9') ||
                     character == '.' || character == '_' ||
                     character == '-')) {
            return false;
        }
    }
    return true;
}

static const char *gbl_matrix_validate_rows(const gbl_matrix_row *rows,
                                           size_t count,
                                           size_t declared_count)
{
    size_t index;
    size_t arm_count = 0u;
    size_t thumb_count = 0u;
    size_t smoke_count = 0u;

    if (count < declared_count) return "missing-row";
    if (count > declared_count) return "unexpected-row";
    if (count == 0u || rows == NULL) return "empty-corpus";
    for (index = 0u; index < count; ++index) {
        const gbl_matrix_row *row = &rows[index];
        size_t prior;

        if (row->id[0] == '\0' || !gbl_matrix_safe_relative_path(
                                       row->relative_path) ||
            !gbl_matrix_sha256_text(row->digest) ||
            row->expected_case_id == 0u || row->expected_cycles == 0u ||
            row->expected_instructions == 0u ||
            row->expected_cycles > GBL_MATRIX_CYCLE_BUDGET) {
            return "invalid-row";
        }
        if ((strcmp(row->expected_status, "PASS") == 0 &&
             (strcmp(row->expected_semantic, "pass") != 0 ||
              strcmp(row->expected_reason, "guest-mailbox-pass") != 0)) ||
            (strcmp(row->expected_status, "GUEST_FAIL") == 0 &&
             (strcmp(row->expected_semantic, "fail") != 0 ||
              strcmp(row->expected_reason, "guest-mailbox-fail") != 0)) ||
            (strcmp(row->expected_status, "GUEST_STOP") == 0 &&
             (strcmp(row->expected_semantic, "pending") != 0 ||
              row->expected_reason[0] == '\0')) ||
            (strcmp(row->expected_status, "PASS") != 0 &&
             strcmp(row->expected_status, "GUEST_FAIL") != 0 &&
             strcmp(row->expected_status, "GUEST_STOP") != 0)) {
            return "wrong-outcome";
        }
        if (strcmp(row->group, "arm") == 0) {
            arm_count++;
        } else if (strcmp(row->group, "thumb") == 0) {
            thumb_count++;
        } else if (strcmp(row->group, "smoke") == 0) {
            smoke_count++;
        } else {
            return "invalid-group";
        }
        if (index != 0u && strcmp(rows[index - 1u].id, row->id) >= 0) {
            for (prior = 0u; prior < index; ++prior) {
                if (strcmp(rows[prior].id, row->id) == 0) {
                    return "duplicate-row";
                }
            }
            return "unsorted-rows";
        }
    }
    if ((arm_count & 1u) != 0u || (thumb_count & 1u) != 0u ||
        (smoke_count & 1u) != 0u) {
        return "incomplete-interleave-pair";
    }
    return "ok";
}

static const char *gbl_matrix_validate_digest(const char *expected,
                                              const char *actual)
{
    if (!gbl_matrix_sha256_text(expected) ||
        !gbl_matrix_sha256_text(actual) || strcmp(expected, actual) != 0) {
        return "stale-digest";
    }
    return "ok";
}

static const char *gbl_matrix_semantic_name(gbl_mailbox_status status)
{
    switch (status) {
    case GBL_MAILBOX_ABSENT: return "absent";
    case GBL_MAILBOX_PENDING: return "pending";
    case GBL_MAILBOX_PASS: return "pass";
    case GBL_MAILBOX_FAIL: return "fail";
    case GBL_MAILBOX_MALFORMED: return "malformed";
    }
    return "invalid";
}

static const char *gbl_matrix_guest_status(gbl_mailbox_status status)
{
    if (status == GBL_MAILBOX_PASS) return "PASS";
    if (status == GBL_MAILBOX_FAIL) return "GUEST_FAIL";
    return "GUEST_STOP";
}

static const char *gbl_matrix_guest_reason(const gbl_runner_result *result)
{
    if (result->semantic.status == GBL_MAILBOX_PASS) {
        return "guest-mailbox-pass";
    }
    if (result->semantic.status == GBL_MAILBOX_FAIL) {
        return "guest-mailbox-fail";
    }
    return result->reason;
}

static const char *gbl_matrix_validate_observation(
    const gbl_matrix_row *row, const gbl_runner_result *result)
{
    const char *status;
    const char *reason;

    if (result == NULL || !result->ran || !result->has_rom_sha256 ||
        result->has_fixture_id || result->metadata != NULL) {
        return "outcome-mismatch";
    }
    {
        const char *digest_finding =
            gbl_matrix_validate_digest(row->digest, result->rom_sha256);
        if (strcmp(digest_finding, "ok") != 0) return digest_finding;
    }
    status = gbl_matrix_guest_status(result->semantic.status);
    reason = gbl_matrix_guest_reason(result);
    if (strcmp(status, row->expected_status) != 0 ||
        strcmp(reason, row->expected_reason) != 0 ||
        strcmp(gbl_matrix_semantic_name(result->semantic.status),
               row->expected_semantic) != 0 ||
        result->semantic.case_id != row->expected_case_id ||
        result->cycles != row->expected_cycles ||
        result->instructions != row->expected_instructions) {
        return "wrong-outcome";
    }
    if (result->semantic.status == GBL_MAILBOX_PASS ||
        result->semantic.status == GBL_MAILBOX_FAIL) {
        if (result->status != GBL_RUNNER_GUEST_STOP ||
            strcmp(result->reason, "unregistered-content") != 0 ||
            strcmp(result->termination, "unregistered-content") != 0) {
            return "wrong-outcome";
        }
    } else if (result->status != GBL_RUNNER_GUEST_STOP ||
               strcmp(result->termination, row->expected_reason) != 0) {
        return "wrong-outcome";
    }
    return "ok";
}

static const char *gbl_matrix_validate_trace(
    const gbl_runner_result *result, const gbl_test_observation *observation)
{
    if (result == NULL || observation == NULL || !result->trace_complete ||
        !result->trace_emitted || result->trace_mask != GBL_TRACE_CATEGORY_MASK ||
        result->trace_capacity != GBL_MATRIX_TRACE_CAPACITY ||
        result->trace_count == 0u ||
        result->trace_count > result->trace_capacity ||
        result->trace_records == NULL || observation->trace.length == 0u ||
        observation->execution_trace.length != observation->trace.length ||
        memcmp(observation->trace.data, observation->execution_trace.data,
               observation->trace.length) != 0 ||
        observation->trace.data[observation->trace.length - 1u] !=
            (uint8_t)'\n' ||
        gbl_test_newline_count(&observation->trace) != result->trace_count) {
        return "incomplete-trace";
    }
    return "ok";
}

static const char *gbl_matrix_record_equal(const char *first,
                                           const char *second)
{
    return first != NULL && second != NULL && strcmp(first, second) == 0
               ? "ok"
               : "changed-record";
}

static unsigned int gbl_matrix_variant_mask(const char *csv, bool *valid)
{
    char copy[256];
    char *save = NULL;
    char *variant;
    unsigned int mask = 0u;

    *valid = false;
    if (!gbl_matrix_copy(copy, sizeof copy, csv)) return 0u;
    variant = strtok_r(copy, ",", &save);
    if (variant == NULL) return 0u;
    while (variant != NULL) {
        unsigned int bit;

        if (strcmp(variant, "host-O0") == 0) bit = 1u;
        else if (strcmp(variant, "host-O2") == 0) bit = 2u;
        else if (strcmp(variant, "gnu-O0") == 0) bit = 4u;
        else if (strcmp(variant, "gnu-O2") == 0) bit = 8u;
        else if (strcmp(variant, "clang-O0") == 0) bit = 16u;
        else if (strcmp(variant, "clang-O2") == 0) bit = 32u;
        else return 0u;
        if ((mask & bit) != 0u) return 0u;
        mask |= bit;
        variant = strtok_r(NULL, ",", &save);
    }
    *valid = true;
    return mask;
}

static const char *gbl_matrix_validate_variants(const char *expected,
                                                const char *actual)
{
    bool expected_valid;
    bool actual_valid;
    const unsigned int expected_mask =
        gbl_matrix_variant_mask(expected, &expected_valid);
    const unsigned int actual_mask =
        gbl_matrix_variant_mask(actual, &actual_valid);

    if (!expected_valid || !actual_valid || expected_mask != actual_mask) {
        return "omitted-variant";
    }
    return "ok";
}

static const char *gbl_matrix_read_corpus(const char *path,
                                          gbl_matrix_row *rows,
                                          size_t *row_count)
{
    FILE *file;
    char line[GBL_MATRIX_LINE_CAPACITY];
    size_t declared_count;
    size_t count = 0u;
    bool header = false;

    if (path == NULL || rows == NULL || row_count == NULL) {
        return "invalid-corpus-path";
    }
    file = fopen(path, "rb");
    if (file == NULL) return "corpus-missing";
    while (fgets(line, sizeof line, file) != NULL) {
        size_t length = strlen(line);
        char *fields[10];
        char *save = NULL;
        char *field;
        size_t field_count = 0u;
        uint64_t parsed;

        if (length == 0u || line[length - 1u] != '\n') {
            if (!feof(file)) {
                (void)fclose(file);
                return "corpus-line-too-long";
            }
        } else {
            line[--length] = '\0';
        }
        if (length != 0u && line[length - 1u] == '\r') {
            line[--length] = '\0';
        }
        if (!header) {
            int parsed_length = 0;
            if (sscanf(line, "gamebloke.determinism-corpus/1\t%zu%n",
                       &declared_count, &parsed_length) != 1 ||
                line[parsed_length] != '\0' ||
                declared_count == 0u || declared_count > GBL_MATRIX_MAX_ROWS) {
                (void)fclose(file);
                return "invalid-corpus-header";
            }
            header = true;
            continue;
        }
        if (line[0] == '\0' || count >= GBL_MATRIX_MAX_ROWS) {
            (void)fclose(file);
            return "invalid-row-count";
        }
        field = strtok_r(line, "\t", &save);
        while (field != NULL && field_count < 10u) {
            fields[field_count++] = field;
            field = strtok_r(NULL, "\t", &save);
        }
        if (field != NULL || field_count != 10u) {
            (void)fclose(file);
            return "invalid-row-fields";
        }
        memset(&rows[count], 0, sizeof rows[count]);
        if (!gbl_matrix_copy(rows[count].id, sizeof rows[count].id,
                             fields[0]) ||
            !gbl_matrix_copy(rows[count].relative_path,
                             sizeof rows[count].relative_path, fields[1]) ||
            !gbl_matrix_copy(rows[count].group, sizeof rows[count].group,
                             fields[2]) ||
            !gbl_matrix_copy(rows[count].digest, sizeof rows[count].digest,
                             fields[3]) ||
            !gbl_matrix_copy(rows[count].expected_status,
                             sizeof rows[count].expected_status, fields[4]) ||
            !gbl_matrix_copy(rows[count].expected_reason,
                             sizeof rows[count].expected_reason, fields[5]) ||
            !gbl_matrix_copy(rows[count].expected_semantic,
                             sizeof rows[count].expected_semantic, fields[6]) ||
        !gbl_matrix_parse_u64(fields[7], &parsed) ||
            parsed > UINT32_MAX) {
            (void)fclose(file);
            return "invalid-row-fields";
        }
        rows[count].expected_case_id = (uint32_t)parsed;
        if (!gbl_matrix_parse_u64(fields[8],
                                  &rows[count].expected_cycles) ||
            !gbl_matrix_parse_u64(fields[9],
                                  &rows[count].expected_instructions)) {
            (void)fclose(file);
            return "invalid-row-fields";
        }
        count++;
    }
    if (ferror(file) != 0) {
        (void)fclose(file);
        return "corpus-read-failed";
    }
    if (fclose(file) != 0) return "corpus-read-failed";
    if (!header) return "invalid-corpus-header";
    *row_count = count;
    return gbl_matrix_validate_rows(rows, count, declared_count);
}

static bool gbl_matrix_path(char *output, size_t capacity,
                            const char *root, const char *relative)
{
    if (!gbl_matrix_safe_relative_path(relative)) return false;
    return gbl_test_path(output, capacity, root, relative);
}

static const char *gbl_matrix_write_record(
    char *output, size_t capacity, const gbl_matrix_row *row,
    const gbl_test_observation *observation)
{
    char trace_hash[GBL_SHA256_HEX_CAPACITY];
    const gbl_runner_result *result = &observation->result;
    int written;

    if (!gbl_test_hash(&observation->trace, trace_hash)) {
        return "trace-hash-failed";
    }
    written = snprintf(
        output, capacity,
        "%s\tgamebloke.profile/1\tagb\tdirect-start\tnone\tnone\t%s\t%s\t%s\t%" PRIu32 "\t%" PRIu64 "\t%" PRIu64 "\t%zu\t%s\n",
        row->id, row->expected_status, row->expected_reason,
        gbl_matrix_semantic_name(result->semantic.status),
        result->semantic.case_id, result->cycles, result->instructions,
        result->trace_count, trace_hash);
    if (written < 0 || (size_t)written >= capacity) return "record-too-long";
    return "ok";
}

static bool gbl_matrix_observe(const gbl_matrix_row *row,
                               const char *artifact_root,
                               const char *trace_path,
                               gbl_test_observation *observation,
                               const char **failure)
{
    char rom_path[GBL_DETERMINISM_PATH_CAPACITY];
    char execution_trace_path[GBL_DETERMINISM_PATH_CAPACITY];
    gbl_runner_options options = { 0 };
    gbl_runner_status status;

    if (!gbl_matrix_path(rom_path, sizeof rom_path, artifact_root,
                         row->relative_path) ||
        !gbl_matrix_copy(execution_trace_path,
                         sizeof execution_trace_path, trace_path)) {
        *failure = "artifact-path-invalid";
        return false;
    }
    options.rom_path = rom_path;
    options.cycle_budget = GBL_MATRIX_CYCLE_BUDGET;
    options.trace_mask = GBL_TRACE_CATEGORY_MASK;
    options.trace_capacity = GBL_MATRIX_TRACE_CAPACITY;
    options.trace_output_path = execution_trace_path;
    status = gbl_runner_execute(&options, &observation->result);
    if (status == GBL_RUNNER_MISSING) {
        *failure = "artifact-missing";
        return false;
    }
    {
        const char *finding =
            gbl_matrix_validate_observation(row, &observation->result);
        if (strcmp(finding, "ok") != 0) {
            *failure = finding;
            return false;
        }
    }
    if (!gbl_test_read_file(execution_trace_path, &observation->trace) ||
        !gbl_test_read_file(execution_trace_path,
                            &observation->execution_trace) ||
        !gbl_test_json(&observation->result, &observation->json)) {
        *failure = "observation-serialization-failed";
        return false;
    }
    {
        const char *finding =
            gbl_matrix_validate_trace(&observation->result, observation);
        if (strcmp(finding, "ok") != 0) {
            *failure = finding;
            return false;
        }
    }
    return true;
}

static const char *gbl_matrix_instance_matches_runner(
    const gbl_interleave_instance *instance,
    const gbl_test_observation *observation)
{
    const gbl_runner_result *result = &observation->result;

    if (instance->mailbox.status != result->semantic.status ||
        instance->mailbox.case_id != result->semantic.case_id) {
        return "interleave-outcome-mismatch";
    }
    if (instance->cycles != result->cycles ||
        instance->instructions != result->instructions) {
        return "interleave-progress-mismatch";
    }
    if (instance->trace.count != result->trace_count ||
        !gbl_interleave_trace_fields_equal(instance->trace.records,
                                           result->trace_records,
                                           result->trace_count) ||
        instance->serialized_trace_length != observation->trace.length ||
        memcmp(instance->serialized_trace, observation->trace.data,
               observation->trace.length) != 0) {
        return "interleave-trace-mismatch";
    }
    return "ok";
}

static const char *gbl_matrix_compare_records(const char *first_path,
                                             const char *second_path,
                                             size_t expected_count)
{
    FILE *first = fopen(first_path, "rb");
    FILE *second = fopen(second_path, "rb");
    char first_line[GBL_MATRIX_RECORD_CAPACITY];
    char second_line[GBL_MATRIX_RECORD_CAPACITY];
    char previous_id[GBL_RUNNER_FIXTURE_ID_CAPACITY] = { 0 };
    size_t count = 0u;
    const char *finding = "ok";
    bool first_close_ok;
    bool second_close_ok;

    if (first == NULL || second == NULL) {
        if (first != NULL) (void)fclose(first);
        if (second != NULL) (void)fclose(second);
        return "missing-artifact";
    }
    if (fgets(first_line, sizeof first_line, first) == NULL ||
        fgets(second_line, sizeof second_line, second) == NULL ||
        strcmp(first_line, "gamebloke.determinism-records/1\n") != 0 ||
        strcmp(second_line, "gamebloke.determinism-records/1\n") != 0) {
        finding = "invalid-record-header";
        goto done;
    }
    for (;;) {
        char first_id[GBL_RUNNER_FIXTURE_ID_CAPACITY];
        char second_id[GBL_RUNNER_FIXTURE_ID_CAPACITY];
        char *first_sep;
        char *second_sep;
        char *first_end;
        char *second_end;

        if (fgets(first_line, sizeof first_line, first) == NULL) {
            if (fgets(second_line, sizeof second_line, second) != NULL) {
                finding = "missing-row";
            }
            break;
        }
        if (fgets(second_line, sizeof second_line, second) == NULL) {
            finding = "missing-row";
            break;
        }
        {
            const size_t first_length = strlen(first_line);
            const size_t second_length = strlen(second_line);
            if (first_length == 0u || second_length == 0u ||
                first_line[first_length - 1u] != '\n' ||
                second_line[second_length - 1u] != '\n') {
                finding = "invalid-record";
                break;
            }
            first_line[first_length - 1u] = '\0';
            second_line[second_length - 1u] = '\0';
        }
        if (strcmp(first_line, second_line) != 0) {
            finding = "changed-record";
            break;
        }
        first_sep = strchr(first_line, '\t');
        second_sep = strchr(second_line, '\t');
        if (first_sep == NULL || second_sep == NULL) {
            finding = "invalid-record";
            break;
        }
        *first_sep = '\0';
        *second_sep = '\0';
        if (!gbl_matrix_copy(first_id, sizeof first_id, first_line) ||
            !gbl_matrix_copy(second_id, sizeof second_id, second_line)) {
            finding = "invalid-record";
            break;
        }
        first_end = strpbrk(first_id, "\r\n");
        second_end = strpbrk(second_id, "\r\n");
        if (first_end != NULL) *first_end = '\0';
        if (second_end != NULL) *second_end = '\0';
        if (strcmp(first_id, second_id) != 0) {
            finding = "missing-row";
            break;
        }
        if (count != 0u && strcmp(previous_id, first_id) >= 0) {
            finding = strcmp(previous_id, first_id) == 0 ? "duplicate-row"
                                                         : "unsorted-rows";
            break;
        }
        if (!gbl_matrix_copy(previous_id, sizeof previous_id, first_id)) {
            finding = "invalid-record";
            break;
        }
        count++;
    }
    if (strcmp(finding, "ok") == 0 && count != expected_count) {
        finding = "missing-row";
    }
done:
    if (ferror(first) != 0 || ferror(second) != 0) {
        if (strcmp(finding, "ok") == 0) finding = "record-read-failed";
    }
    first_close_ok = fclose(first) == 0;
    second_close_ok = fclose(second) == 0;
    if ((!first_close_ok || !second_close_ok) &&
        strcmp(finding, "ok") == 0) {
        finding = "record-close-failed";
    }
    return finding;
}

static bool gbl_matrix_compare_pair(const gbl_matrix_row *first_row,
                                    const gbl_matrix_row *second_row,
                                    const char *artifact_root,
                                    const char *work,
                                    char *first_record,
                                    size_t first_record_capacity,
                                    char *second_record,
                                    size_t second_record_capacity,
                                    const char **failure)
{
    char first_path[GBL_DETERMINISM_PATH_CAPACITY];
    char second_path[GBL_DETERMINISM_PATH_CAPACITY];
    char first_trace[GBL_DETERMINISM_PATH_CAPACITY];
    char second_trace[GBL_DETERMINISM_PATH_CAPACITY];
    gbl_test_bytes first_image = { 0 };
    gbl_test_bytes second_image = { 0 };
    gbl_interleave_pair pair = { 0 };
    gbl_test_observation first_observation = { 0 };
    gbl_test_observation second_observation = { 0 };
    bool okay = false;

    if (!gbl_matrix_path(first_path, sizeof first_path, artifact_root,
                         first_row->relative_path) ||
        !gbl_matrix_path(second_path, sizeof second_path, artifact_root,
                         second_row->relative_path) ||
        !gbl_test_path(first_trace, sizeof first_trace, work,
                       "interleave-first.trace.jsonl") ||
        !gbl_test_path(second_trace, sizeof second_trace, work,
                       "interleave-second.trace.jsonl") ||
        !gbl_test_read_file(first_path, &first_image) ||
        !gbl_test_read_file(second_path, &second_image)) {
        *failure = "interleave-artifact-missing";
        goto done;
    }
    if (!gbl_interleave_pair_prepare(
            &pair, first_image.data, first_image.length,
            GBL_MATRIX_CYCLE_BUDGET, second_image.data, second_image.length,
            GBL_MATRIX_CYCLE_BUDGET, failure)) {
        goto done;
    }
    if (!gbl_matrix_observe(first_row, artifact_root, first_trace,
                            &first_observation, failure) ||
        !gbl_matrix_observe(second_row, artifact_root, second_trace,
                            &second_observation, failure)) {
        goto done;
    }
    if (strcmp(gbl_matrix_instance_matches_runner(
                   &pair.isolated_first, &first_observation), "ok") != 0 ||
        strcmp(gbl_matrix_instance_matches_runner(
                   &pair.alternating_first, &first_observation), "ok") != 0 ||
        strcmp(gbl_matrix_instance_matches_runner(
                   &pair.isolated_second, &second_observation), "ok") != 0 ||
        strcmp(gbl_matrix_instance_matches_runner(
                   &pair.alternating_second, &second_observation), "ok") != 0) {
        *failure = "interleave-observation-mismatch";
        goto done;
    }
    if (strcmp(gbl_matrix_write_record(first_record, first_record_capacity,
                                       first_row, &first_observation),
               "ok") != 0 ||
        strcmp(gbl_matrix_write_record(second_record, second_record_capacity,
                                       second_row, &second_observation),
               "ok") != 0) {
        *failure = "record-generation-failed";
        goto done;
    }
    okay = true;

done:
    if (!gbl_interleave_pair_dispose(&pair)) {
        *failure = "interleave-dispose-failed";
        okay = false;
    }
    gbl_test_dispose(&first_observation);
    gbl_test_dispose(&second_observation);
    free(first_image.data);
    free(second_image.data);
    (void)unlink(first_trace);
    (void)unlink(second_trace);
    return okay;
}

static int gbl_test_matrix_main(const char *corpus_path,
                                const char *artifact_root,
                                const char *record_path)
{
    gbl_matrix_row *rows = NULL;
    char (*records)[GBL_MATRIX_RECORD_CAPACITY] = NULL;
    int pending[3] = { -1, -1, -1 };
    FILE *records_file = NULL;
    char work[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    size_t row_count = 0u;
    size_t index;
    struct timespec start = { 0 };
    struct timespec finish = { 0 };
    const char *failure = "matrix-observation-failed";
    bool work_created = false;
    int exit_code = 1;

    rows = calloc(GBL_MATRIX_MAX_ROWS, sizeof *rows);
    records = calloc(GBL_MATRIX_MAX_ROWS, sizeof *records);
    if (rows == NULL || records == NULL) {
        failure = "matrix-allocation-failed";
        goto done;
    }
    failure = gbl_matrix_read_corpus(corpus_path, rows, &row_count);
    if (strcmp(failure, "ok") != 0) goto done;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0 ||
        !gbl_test_create_directory(work, sizeof work)) {
        failure = "matrix-work-root-create-failed";
        goto done;
    }
    work_created = true;

    for (index = 0u; index < row_count; ++index) {
        char first_trace[GBL_DETERMINISM_PATH_CAPACITY];
        char second_trace[GBL_DETERMINISM_PATH_CAPACITY];
        gbl_test_observation first = { 0 };
        gbl_test_observation second = { 0 };

        if (snprintf(first_trace, sizeof first_trace,
                     "%s/repeat-%zu-first.trace.jsonl", work, index) < 0 ||
            snprintf(second_trace, sizeof second_trace,
                     "%s/repeat-%zu-second.trace.jsonl", work, index) < 0 ||
            !gbl_matrix_observe(&rows[index], artifact_root, first_trace,
                                &first, &failure) ||
            !gbl_matrix_observe(&rows[index], artifact_root, second_trace,
                                &second, &failure) ||
            !gbl_test_equal(&first, &second, &failure)) {
            gbl_test_dispose(&first);
            gbl_test_dispose(&second);
            (void)unlink(first_trace);
            (void)unlink(second_trace);
            goto done;
        }
        if (strcmp(gbl_matrix_write_record(records[index],
                                           GBL_MATRIX_RECORD_CAPACITY,
                                           &rows[index], &first),
                   "ok") != 0) {
            failure = "record-generation-failed";
            gbl_test_dispose(&first);
            gbl_test_dispose(&second);
            (void)unlink(first_trace);
            (void)unlink(second_trace);
            goto done;
        }
        gbl_test_dispose(&first);
        gbl_test_dispose(&second);
        (void)unlink(first_trace);
        (void)unlink(second_trace);
    }

    for (index = 0u; index < row_count; ++index) {
        unsigned int group_index;
        char first_record[GBL_MATRIX_RECORD_CAPACITY];
        char second_record[GBL_MATRIX_RECORD_CAPACITY];

        if (strcmp(rows[index].group, "arm") == 0) group_index = 0u;
        else if (strcmp(rows[index].group, "thumb") == 0) group_index = 1u;
        else group_index = 2u;
        if (pending[group_index] < 0) {
            pending[group_index] = (int)index;
            continue;
        }
        if (!gbl_matrix_compare_pair(
                &rows[(size_t)pending[group_index]], &rows[index],
                artifact_root, work, first_record, sizeof first_record,
                second_record, sizeof second_record, &failure)) {
            goto done;
        }
        if (strcmp(gbl_matrix_record_equal(
                       records[(size_t)pending[group_index]], first_record),
                   "ok") != 0 ||
            strcmp(gbl_matrix_record_equal(records[index], second_record),
                   "ok") != 0) {
            failure = "changed-record";
            goto done;
        }
        pending[group_index] = -1;
    }
    if (pending[0] >= 0 || pending[1] >= 0 || pending[2] >= 0) {
        failure = "incomplete-interleave-pair";
        goto done;
    }
    records_file = fopen(record_path, "wx");
    if (records_file == NULL ||
        fputs("gamebloke.determinism-records/1\n", records_file) == EOF) {
        failure = "record-output-create-failed";
        goto done;
    }
    for (index = 0u; index < row_count; ++index) {
        if (fputs(records[index], records_file) == EOF) {
            failure = "record-output-write-failed";
            goto done;
        }
    }
    if (fclose(records_file) != 0) {
        records_file = NULL;
        failure = "record-output-close-failed";
        goto done;
    }
    records_file = NULL;
    if (clock_gettime(CLOCK_MONOTONIC, &finish) != 0) {
        failure = "matrix-clock-failed";
        goto done;
    }
    {
        const double elapsed =
            (double)(finish.tv_sec - start.tv_sec) +
            ((double)finish.tv_nsec - (double)start.tv_nsec) / 1000000000.0;
        printf("PASS determinism.matrix: %zu sorted rows; same-process repeat and isolated/interleaved equality\n",
               row_count);
        printf("HOST guest-observation-seconds=%.6f rows=%zu trace-mask=%u budget=%u\n",
               elapsed, row_count, (unsigned int)GBL_TRACE_CATEGORY_MASK,
               (unsigned int)GBL_MATRIX_CYCLE_BUDGET);
    }
    failure = "ok";
    exit_code = 0;

done:
    if (records_file != NULL) (void)fclose(records_file);
    if (work_created) {
        (void)rmdir(work);
    }
    if (exit_code != 0) {
        fprintf(stderr, "FAIL determinism.matrix: %s\n", failure);
    }
    free(rows);
    free(records);
    return exit_code;
}

static int gbl_test_matrix_compare_main(const char *first_path,
                                        const char *second_path,
                                        const char *expected_count_text)
{
    uint64_t expected_count;
    const char *finding;

    if (!gbl_matrix_parse_u64(expected_count_text, &expected_count) ||
        expected_count > GBL_MATRIX_MAX_ROWS) {
        fputs("FAIL determinism.matrix.compare: invalid-row-count\n", stderr);
        return 2;
    }
    finding = gbl_matrix_compare_records(first_path, second_path,
                                         (size_t)expected_count);
    if (strcmp(finding, "ok") != 0) {
        fprintf(stderr, "FAIL determinism.matrix.compare: %s\n", finding);
        return 1;
    }
    printf("PASS determinism.matrix.compare: %" PRIu64 " sorted fixture records match\n",
           expected_count);
    return 0;
}

static int gbl_test_matrix_variants_main(const char *expected,
                                         const char *actual)
{
    const char *finding = gbl_matrix_validate_variants(expected, actual);

    if (strcmp(finding, "ok") != 0) {
        fprintf(stderr, "FAIL determinism.matrix.variants: %s\n", finding);
        return 1;
    }
    printf("PASS determinism.matrix.variants: %s\n", actual);
    return 0;
}

static bool gbl_matrix_write_control_records(const char *path, bool changed)
{
    FILE *file = fopen(path, "wb");
    bool okay;

    if (file == NULL) return false;
    okay = fputs("gamebloke.determinism-records/1\n"
                 "arm-0001\tsemantic-a\n", file) != EOF;
    okay = okay && fputs(changed
                    ? "arm-0002\tsemantic-changed\n"
                    : "arm-0002\tsemantic-b\n", file) != EOF;
    if (fclose(file) != 0) okay = false;
    return okay;
}

static int gbl_test_matrix_controls_main(void)
{
    gbl_matrix_row rows[2] = { 0 };
    gbl_runner_result wrong_result = { 0 };
    gbl_runner_result incomplete_result = { 0 };
    gbl_test_observation incomplete_observation = { 0 };
    char control_directory[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char first_records_path[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char second_records_path[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    const char *missing;
    const char *duplicate;
    const char *stale_digest;
    const char *wrong_outcome;
    const char *incomplete_trace;
    const char *changed_record;
    const char *omitted_variant;
    bool clean = true;

    (void)gbl_matrix_copy(rows[0].id, sizeof rows[0].id, "arm-0001");
    (void)gbl_matrix_copy(rows[0].relative_path,
                          sizeof rows[0].relative_path,
                          "generated/arm/one.gba");
    (void)gbl_matrix_copy(rows[0].group, sizeof rows[0].group, "arm");
    (void)gbl_matrix_copy(rows[0].digest, sizeof rows[0].digest,
                          "0000000000000000000000000000000000000000000000000000000000000001");
    (void)gbl_matrix_copy(rows[0].expected_status,
                          sizeof rows[0].expected_status, "PASS");
    (void)gbl_matrix_copy(rows[0].expected_reason,
                          sizeof rows[0].expected_reason,
                          "guest-mailbox-pass");
    (void)gbl_matrix_copy(rows[0].expected_semantic,
                          sizeof rows[0].expected_semantic, "pass");
    rows[0].expected_case_id = 1u;
    rows[0].expected_cycles = 10u;
    rows[0].expected_instructions = 5u;
    rows[1] = rows[0];
    (void)gbl_matrix_copy(rows[1].id, sizeof rows[1].id, "arm-0002");
    (void)gbl_matrix_copy(rows[1].relative_path,
                          sizeof rows[1].relative_path,
                          "generated/arm/two.gba");
    rows[1].expected_case_id = 2u;

    missing = gbl_matrix_validate_rows(rows, 1u, 2u);
    duplicate = gbl_matrix_validate_rows((gbl_matrix_row[2]){ rows[0], rows[0] },
                                        2u, 2u);
    stale_digest = gbl_matrix_validate_digest(rows[0].digest,
        "0000000000000000000000000000000000000000000000000000000000000002");
    wrong_result.ran = true;
    wrong_result.has_rom_sha256 = true;
    (void)gbl_matrix_copy(wrong_result.rom_sha256,
                          sizeof wrong_result.rom_sha256, rows[0].digest);
    wrong_result.semantic.status = GBL_MAILBOX_FAIL;
    wrong_result.semantic.case_id = rows[0].expected_case_id;
    wrong_result.status = GBL_RUNNER_GUEST_STOP;
    (void)gbl_matrix_copy(wrong_result.reason, sizeof wrong_result.reason,
                          "unregistered-content");
    (void)gbl_matrix_copy(wrong_result.termination,
                          sizeof wrong_result.termination,
                          "unregistered-content");
    wrong_result.cycles = rows[0].expected_cycles;
    wrong_result.instructions = rows[0].expected_instructions;
    wrong_outcome = gbl_matrix_validate_observation(&rows[0], &wrong_result);
    incomplete_trace = gbl_matrix_validate_trace(&incomplete_result,
                                                  &incomplete_observation);
    if (!gbl_test_create_directory(control_directory,
                                   sizeof control_directory) ||
        !gbl_test_path(first_records_path, sizeof first_records_path,
                       control_directory, "first.records") ||
        !gbl_test_path(second_records_path, sizeof second_records_path,
                       control_directory, "second.records")) {
        clean = false;
    }
    if (clean) {
        clean = gbl_matrix_write_control_records(first_records_path, false) &&
                gbl_matrix_write_control_records(second_records_path, true);
    }
    changed_record = clean
        ? gbl_matrix_compare_records(first_records_path, second_records_path,
                                     2u)
        : "record-control-setup-failed";
    omitted_variant = gbl_matrix_validate_variants(
        "gnu-O0,gnu-O2,clang-O0,clang-O2",
        "gnu-O0,gnu-O2,clang-O0");
    if (strcmp(gbl_matrix_validate_rows(rows, 2u, 2u), "ok") != 0 ||
        strcmp(missing, "missing-row") != 0 ||
        strcmp(duplicate, "duplicate-row") != 0 ||
        strcmp(stale_digest, "stale-digest") != 0 ||
        strcmp(wrong_outcome, "wrong-outcome") != 0 ||
        strcmp(incomplete_trace, "incomplete-trace") != 0 ||
        strcmp(changed_record, "changed-record") != 0 ||
        strcmp(omitted_variant, "omitted-variant") != 0) {
        clean = false;
    }
    (void)unlink(first_records_path);
    (void)unlink(second_records_path);
    if (control_directory[0] != '\0') (void)rmdir(control_directory);
    if (!clean) {
        fprintf(stderr,
                "FAIL determinism.matrix.controls: missing=%s duplicate=%s digest=%s outcome=%s trace=%s changed=%s variant=%s\n",
                missing, duplicate, stale_digest, wrong_outcome,
                incomplete_trace, changed_record, omitted_variant);
        return 1;
    }
    puts("PASS determinism.matrix.controls: missing-row duplicate-row stale-digest wrong-outcome incomplete-trace changed-record omitted-variant are rejected; clean corpus and complete variant set pass");
    return 0;
}

static void gbl_test_write_word(uint8_t *bytes, size_t offset, uint32_t word)
{
    bytes[offset] = (uint8_t)(word & UINT32_C(0xFF));
    bytes[offset + 1u] = (uint8_t)((word >> 8u) & UINT32_C(0xFF));
    bytes[offset + 2u] = (uint8_t)((word >> 16u) & UINT32_C(0xFF));
    bytes[offset + 3u] = (uint8_t)((word >> 24u) & UINT32_C(0xFF));
}

static bool gbl_test_time_authority_has_instruction(
    const gbl_trace_buffer *trace, uint32_t address, uint32_t opcode)
{
    size_t index;

    for (index = 0u; index < trace->count; ++index) {
        const gbl_trace_record *record = &trace->records[index];

        if (record->category == GBL_TRACE_INSTRUCTION &&
            record->instruction_address == address &&
            record->opcode == opcode) {
            return true;
        }
    }
    return false;
}

static bool gbl_test_time_authority_has_bus_write(
    const gbl_trace_buffer *trace)
{
    size_t index;

    for (index = 0u; index < trace->count; ++index) {
        const gbl_trace_record *record = &trace->records[index];

        if (record->category == GBL_TRACE_BUS &&
            record->instruction_address == UINT32_C(0x08000008) &&
            record->opcode == UINT32_C(0xE5801000) &&
            record->address == UINT32_C(0x03000000) &&
            record->width == 4u &&
            record->bus_outcome == GBL_BUS_OUTCOME_RAM) {
            return true;
        }
    }
    return false;
}

static void *gbl_test_time_authority_allocate(void *context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void gbl_test_time_authority_release(void *context, void *pointer,
                                             size_t size)
{
    (void)context;
    (void)size;
    free(pointer);
}

static bool gbl_test_time_authority_timer_observe(const char *mode,
                                                  const char *output_path,
                                                  const char **failure)
{
    enum { GBL_TIMER_OBSERVATION_CYCLE_BUDGET = 10000u };
    gbl_test_bytes image = { 0 };
    gbl_test_bytes trace_bytes = { 0 };
    gbl_trace_record trace_records[GBL_DETERMINISM_TRACE_CAPACITY] = { { 0 } };
    gbl_trace_buffer trace = {
        trace_records, GBL_DETERMINISM_TRACE_CAPACITY, 0u, 0u
    };
    gbl_run_options options = {
        GBL_TIMER_OBSERVATION_CYCLE_BUDGET, UINT64_C(1),
        GBL_TRACE_CATEGORY_MASK, &trace
    };
    gbl_runner_result trace_result = { 0 };
    const gbl_allocator allocator = {
        NULL, gbl_test_time_authority_allocate,
        gbl_test_time_authority_release
    };
    gbl_machine machine = { 0 };
    gbl_machine clean_machine = { 0 };
    gbl_load_result load_result;
    gbl_mailbox_result mailbox = { 0 };
    char image_path[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char work[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char trace_path[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char header[768];
    char *canonical = NULL;
    uint64_t cycles = 0u;
    uint64_t instructions = 0u;
    uint64_t clean_time_digest;
    uint64_t time_digest;
    size_t steps = 0u;
    int header_length;
    bool initialized = false;
    bool inject_mutation = strcmp(mode, "timer-mutant") == 0;
    bool timer_mode = strcmp(mode, "timer") == 0;
    bool okay = false;
    FILE *output = NULL;

    if ((!timer_mode && !inject_mutation) || output_path == NULL ||
        output_path[0] == '\0') {
        *failure = "timer-observation-arguments-invalid";
        return false;
    }
    if (!gbl_test_path(image_path, sizeof image_path,
                       GBL_DETERMINISM_FIXTURE_DIR,
                       "owned-timer0-pass.gba") ||
        !gbl_test_read_file(image_path, &image)) {
        *failure = "timer0-guest-fixture-read-failed";
        goto cleanup;
    }
    if (!gbl_test_create_directory(work, sizeof work) ||
        !gbl_test_path(trace_path, sizeof trace_path, work,
                       "timer0-observation.trace.jsonl")) {
        *failure = "timer-observation-scratch-path-invalid";
        goto cleanup;
    }
    if (gbl_machine_init(&machine, &allocator) != GBL_BUS_STATUS_OK) {
        *failure = "timer-observation-machine-initialization-failed";
        goto cleanup;
    }
    initialized = true;
    load_result = gbl_load_rom(&machine, image.data, image.length);
    if (load_result.status != GBL_LOAD_STATUS_OK ||
        load_result.reason != GBL_LOAD_REASON_NONE) {
        *failure = "timer0-guest-load-failed";
        goto cleanup;
    }

    while (steps < GBL_TIMER_OBSERVATION_CYCLE_BUDGET &&
           cycles < GBL_TIMER_OBSERVATION_CYCLE_BUDGET) {
        gbl_run_result part = { 0 };
        const uint64_t remaining =
            GBL_TIMER_OBSERVATION_CYCLE_BUDGET - cycles;

        options.cycle_budget = remaining;
        if (gbl_cpu_run(&machine, &options, &part) != GBL_RUN_STATUS_OK ||
            UINT64_MAX - cycles < part.cycles ||
            UINT64_MAX - instructions < part.instructions) {
            *failure = "timer-observation-canonical-run-failed";
            goto cleanup;
        }
        cycles += part.cycles;
        instructions += part.instructions;
        steps++;
        if (part.termination == GBL_RUN_TRACE_OUTPUT_FULL ||
            (part.termination != GBL_RUN_PROGRESS &&
             part.termination != GBL_RUN_STOPPED)) {
            *failure = "timer-observation-did-not-complete-whole-unit";
            goto cleanup;
        }
        mailbox = gbl_mailbox_read(&machine);
        if (mailbox.status == GBL_MAILBOX_PASS ||
            mailbox.status == GBL_MAILBOX_FAIL) {
            break;
        }
    }
    if (mailbox.status != GBL_MAILBOX_PASS ||
        mailbox.case_id != UINT32_C(7001) || cycles == 0u ||
        cycles > GBL_TIMER_OBSERVATION_CYCLE_BUDGET || instructions == 0u ||
        machine.master_cycles != cycles ||
        (machine.interrupt_flags & (UINT16_C(1) << 3u)) == 0u ||
        !machine.timers[0].enabled || !machine.timers[0].irq_enabled ||
        machine.event_count == 0u ||
        machine.event_count > GBL_EVENT_CAPACITY ||
        !machine.events[0].live ||
        machine.events[0].kind !=
            (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW ||
        machine.events[0].timer_index != 0u ||
        machine.events[0].generation != machine.timers[0].generation) {
        *failure = "timer0-guest-semantic-or-live-event-observation-invalid";
        goto cleanup;
    }

    trace_result.trace_mask = GBL_TRACE_CATEGORY_MASK;
    trace_result.trace_capacity = trace.capacity;
    trace_result.trace_count = trace.count;
    trace_result.trace_complete = true;
    trace_result.trace_emitted = true;
    trace_result.trace_records = trace.records;
    if (trace.count == 0u || trace.count > trace.capacity ||
        !gbl_runner_write_trace(trace_path, &trace_result) ||
        !gbl_test_read_file(trace_path, &trace_bytes) ||
        trace_bytes.data[trace_bytes.length - 1u] != (uint8_t)'\n' ||
        gbl_test_newline_count(&trace_bytes) != trace.count) {
        *failure = "timer-observation-trace-serialization-failed";
        goto cleanup;
    }

    clean_machine = machine;
    clean_time_digest = gbl_test_time_state_digest(&clean_machine);
    if (inject_mutation) {
        machine.timers[0].generation++;
        machine.interrupt_flags &= (uint16_t)~(UINT16_C(1) << 3u);
        if (gbl_test_time_state_equal(&clean_machine, &machine) ||
            gbl_test_canonical_equal(&clean_machine, &machine)) {
            *failure = "time-authority-timer-state-mutation-not-detected";
            goto cleanup;
        }
        puts("EXPECTED time-authority-timer-state-mismatch: Timer 0 generation and IF request changed after the clean guest while trace records stayed fixed");
    } else if (!gbl_test_canonical_equal(&clean_machine, &machine)) {
        *failure = "clean-timer-observation-state-changed";
        goto cleanup;
    }
    time_digest = gbl_test_time_state_digest(&machine);
    if (inject_mutation && time_digest == clean_time_digest) {
        *failure = "time-authority-timer-state-digest-did-not-change";
        goto cleanup;
    }
    header_length = snprintf(
        header, sizeof header,
        "{\"schema\":\"gamebloke.time-authority/1\",\"model\":\"gamebloke-timer-model-1\",\"qualification\":\"project-model-only\",\"termination\":\"mailbox-complete\",\"semantic_state\":\"pass\",\"case_id\":\"7001\",\"cycles\":\"%" PRIu64 "\",\"instructions\":\"%" PRIu64 "\",\"master_cycles\":\"%" PRIu64 "\",\"interrupt_flags\":\"%u\",\"if_request\":%s,\"timer0_generation\":\"%" PRIu64 "\",\"timer0_counter\":\"%u\",\"timer0_enabled\":%s,\"timer0_irq_enabled\":%s,\"event_count\":\"%zu\",\"event0_live\":%s,\"event0_generation\":\"%" PRIu64 "\",\"time_state_digest\":\"%016" PRIx64 "\",\"trace_count\":%zu}\n",
        cycles, instructions, machine.master_cycles,
        (unsigned int)machine.interrupt_flags,
        (machine.interrupt_flags & (UINT16_C(1) << 3u)) != 0u ? "true" :
                                                                  "false",
        machine.timers[0].generation,
        (unsigned int)machine.timers[0].counter,
        machine.timers[0].enabled ? "true" : "false",
        machine.timers[0].irq_enabled ? "true" : "false",
        machine.event_count, machine.events[0].live ? "true" : "false",
        machine.events[0].generation, time_digest, trace.count);
    if (header_length < 0 || (size_t)header_length >= sizeof header ||
        (size_t)header_length > SIZE_MAX - trace_bytes.length) {
        *failure = "timer-observation-header-failed";
        goto cleanup;
    }
    canonical = malloc((size_t)header_length + trace_bytes.length);
    if (canonical == NULL) {
        *failure = "timer-observation-allocation-failed";
        goto cleanup;
    }
    memcpy(canonical, header, (size_t)header_length);
    memcpy(canonical + (size_t)header_length, trace_bytes.data,
           trace_bytes.length);
    output = fopen(output_path, "wbx");
    if (output == NULL) {
        *failure = "timer-observation-write-failed";
        goto cleanup;
    }
    if (fwrite(canonical, 1u, (size_t)header_length + trace_bytes.length,
               output) != (size_t)header_length + trace_bytes.length) {
        (void)fclose(output);
        output = NULL;
        *failure = "timer-observation-write-failed";
        goto cleanup;
    }
    if (fclose(output) != 0) {
        output = NULL;
        *failure = "timer-observation-write-failed";
        goto cleanup;
    }
    output = NULL;
    okay = true;

cleanup:
    if (output != NULL) (void)fclose(output);
    free(canonical);
    free(trace_bytes.data);
    free(image.data);
    if (trace_path[0] != '\0') (void)unlink(trace_path);
    if (work[0] != '\0') (void)rmdir(work);
    if (initialized) gbl_machine_dispose(&machine);
    if (okay) {
        printf("PASS determinism.time_authority: mode=%s termination=mailbox-complete semantic=pass case_id=7001 cycles=%" PRIu64 " instructions=%" PRIu64 " master_cycles=%" PRIu64 " trace_count=%zu time_state_digest=%016" PRIx64 "\n",
               mode, cycles, instructions, clean_machine.master_cycles,
               trace.count, time_digest);
    }
    return okay;
}

static bool gbl_test_time_authority_observe(const char *mode,
                                            const char *output_path,
                                            const char **failure)
{
    uint8_t image[0xC0u] = { 0 };
    gbl_trace_record trace_records[GBL_DETERMINISM_TRACE_CAPACITY] = { { 0 } };
    gbl_trace_buffer trace = {
        trace_records, GBL_DETERMINISM_TRACE_CAPACITY, 0u, 0u
    };
    gbl_run_result total = {
        .termination = GBL_RUN_PROGRESS,
        .reason = GBL_RUN_REASON_UNIT_LIMIT
    };
    gbl_run_result part = {
        .termination = GBL_RUN_PROGRESS,
        .reason = GBL_RUN_REASON_UNIT_LIMIT
    };
    gbl_run_options options = {
        UINT64_C(1000), UINT64_C(1), GBL_TRACE_CATEGORY_MASK, &trace
    };
    const gbl_allocator allocator = {
        NULL, gbl_test_time_authority_allocate,
        gbl_test_time_authority_release
    };
    gbl_runner_result trace_result = { 0 };
    gbl_machine machine;
    gbl_machine machine_before;
    gbl_load_result load_result;
    gbl_test_bytes trace_bytes = { 0 };
    gbl_test_bytes canonical = { 0 };
    gbl_test_bytes alternate_canonical = { 0 };
    char work[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char trace_path[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    char header[256];
    char alternate_header[256];
    char digest[GBL_SHA256_HEX_CAPACITY];
    char alternate_digest[GBL_SHA256_HEX_CAPACITY];
    uint64_t time_digest;
    uint64_t alternate_time_digest;
    uint8_t *rom_before = NULL;
    size_t rom_length = 0u;
    size_t steps = 0u;
    size_t index;
    uint32_t categories = 0u;
    int header_length;
    int alternate_header_length;
    bool initialized = false;
    bool final_mode = strcmp(mode, "final") == 0;
    bool reached_boundary = false;
    bool okay = false;
    FILE *output = NULL;

    if (strcmp(mode, "timer") == 0 || strcmp(mode, "timer-mutant") == 0) {
        return gbl_test_time_authority_timer_observe(mode, output_path,
                                                     failure);
    }
    if (strcmp(mode, "full") != 0 && !final_mode) {
        *failure = "invalid-observation-mode";
        return false;
    }
    if (output_path == NULL || output_path[0] == '\0') {
        *failure = "observation-output-path-invalid";
        return false;
    }

    gbl_test_write_word(image, 0x00u, UINT32_C(0xE3A00403)); /* MOV r0,#IWRAM */
    gbl_test_write_word(image, 0x04u, UINT32_C(0xE3A0105A)); /* MOV r1,#0x5A */
    gbl_test_write_word(image, 0x08u, UINT32_C(0xE5801000)); /* STR r1,[r0] */
    gbl_test_write_word(image, 0x0Cu, UINT32_C(0xE3A03001)); /* MOV r3,#1 */
    gbl_test_write_word(image, 0x10u, UINT32_C(0xE1A02311)); /* MOV r2,r1,LSL r3 */
    gbl_test_write_word(image, 0x14u, UINT32_C(0xEA000000)); /* B 0x0800001C */
    gbl_test_write_word(image, 0x18u, UINT32_C(0xE1A05005)); /* skipped NOP */
    gbl_test_write_word(image, 0x1Cu, UINT32_C(0xE1A04002)); /* MOV r4,r2 */
    gbl_test_write_word(image, 0x20u, UINT32_C(0xEF000000)); /* SWI, no BIOS */

    if (!gbl_test_create_directory(work, sizeof work) ||
        !gbl_test_path(trace_path, sizeof trace_path, work,
                       "time-authority.trace.jsonl")) {
        *failure = "time-authority-scratch-path-invalid";
        goto cleanup;
    }
    if (gbl_machine_init(&machine, &allocator) != GBL_BUS_STATUS_OK) {
        *failure = "machine-initialization-failed";
        goto cleanup;
    }
    initialized = true;
    load_result = gbl_load_rom(&machine, image, sizeof image);
    if (load_result.status != GBL_LOAD_STATUS_OK ||
        load_result.reason != GBL_LOAD_REASON_NONE) {
        *failure = "owned-sequence-load-failed";
        goto cleanup;
    }

    while (steps < 32u) {
        if (final_mode && machine.cpu.retired_instructions == UINT64_C(7) &&
            machine.cpu.execution_address == UINT32_C(0x08000020)) {
            reached_boundary = true;
            break;
        }
        if (gbl_cpu_run(&machine, &options, &part) !=
            GBL_RUN_STATUS_OK) {
            *failure = "canonical-cpu-run-failed";
            goto cleanup;
        }
        if (UINT64_MAX - total.cycles < part.cycles ||
            UINT64_MAX - total.instructions < part.instructions) {
            *failure = "observation-progress-overflow";
            goto cleanup;
        }
        total.cycles += part.cycles;
        total.instructions += part.instructions;
        total.termination = part.termination;
        total.reason = part.reason;
        steps++;
        if (part.termination == GBL_RUN_STOPPED) {
            reached_boundary = !final_mode;
            break;
        }
        if (part.termination != GBL_RUN_PROGRESS ||
            part.reason != GBL_RUN_REASON_UNIT_LIMIT) {
            *failure = "canonical-cpu-run-did-not-complete-unit";
            goto cleanup;
        }
    }
    if (!reached_boundary || total.instructions < UINT64_C(7) ||
        total.cycles == 0u || machine.master_cycles == 0u ||
        trace.count == 0u || trace.count > trace.capacity) {
        *failure = "owned-sequence-did-not-reach-a-complete-boundary";
        goto cleanup;
    }
    if (machine.iwram[0] != UINT8_C(0x5A) || machine.iwram[1] != 0u ||
        machine.iwram[2] != 0u || machine.iwram[3] != 0u) {
        *failure = "owned-transfer-did-not-write-expected-iwram-value";
        goto cleanup;
    }
    if (!gbl_test_time_authority_has_instruction(
            &trace, UINT32_C(0x08000000), UINT32_C(0xE3A00403)) ||
        !gbl_test_time_authority_has_instruction(
            &trace, UINT32_C(0x08000008), UINT32_C(0xE5801000)) ||
        !gbl_test_time_authority_has_instruction(
            &trace, UINT32_C(0x08000010), UINT32_C(0xE1A02311)) ||
        !gbl_test_time_authority_has_instruction(
            &trace, UINT32_C(0x08000014), UINT32_C(0xEA000000)) ||
        !gbl_test_time_authority_has_instruction(
            &trace, UINT32_C(0x0800001C), UINT32_C(0xE1A04002))) {
        *failure = "required-alu-transfer-internal-or-branch-record-missing";
        goto cleanup;
    }
    if (!gbl_test_time_authority_has_bus_write(&trace)) {
        *failure = "expected-iwram-bus-write-record-missing";
        goto cleanup;
    }
    if ((!final_mode &&
         (total.termination != GBL_RUN_STOPPED ||
          total.reason != GBL_RUN_REASON_GUEST_STOP ||
          !gbl_test_time_authority_has_instruction(
              &trace, UINT32_C(0x08000020), UINT32_C(0xEF000000)))) ||
        (final_mode &&
         (total.termination != GBL_RUN_PROGRESS ||
          total.reason != GBL_RUN_REASON_UNIT_LIMIT || machine.cpu.stopped))) {
        *failure = "terminal-status-or-final-boundary-invalid";
        goto cleanup;
    }

    for (index = 0u; index < trace.count; ++index) {
        const gbl_trace_record *record = &trace.records[index];

        if (record->sequence != (uint64_t)index ||
            (index != 0u && record->guest_cycle <
                                trace.records[index - 1u].guest_cycle)) {
            *failure = "trace-order-or-time-invalid";
            goto cleanup;
        }
        categories |= (uint32_t)record->category;
    }
    if ((categories & (uint32_t)(GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS)) !=
        (uint32_t)(GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS) ||
        (!final_mode &&
         (categories & (uint32_t)GBL_TRACE_EXCEPTION) == 0u)) {
        *failure = "trace-categories-incomplete";
        goto cleanup;
    }

    trace_result.trace_mask = GBL_TRACE_CATEGORY_MASK;
    trace_result.trace_capacity = trace.capacity;
    trace_result.trace_count = trace.count;
    trace_result.trace_complete = true;
    trace_result.trace_emitted = true;
    trace_result.trace_records = trace.records;
    if (!gbl_runner_write_trace(trace_path, &trace_result) ||
        !gbl_test_read_file(trace_path, &trace_bytes) ||
        trace_bytes.length == 0u ||
        trace_bytes.data[trace_bytes.length - 1u] != (uint8_t)'\n' ||
        gbl_test_newline_count(&trace_bytes) != trace.count) {
        *failure = "canonical-trace-serialization-failed";
        goto cleanup;
    }

    memcpy(&machine_before, &machine, sizeof machine);
    rom_length = machine.rom_length;
    rom_before = malloc(rom_length);
    if (rom_before == NULL || machine.rom == NULL ||
        rom_length > SIZE_MAX - trace_bytes.length - sizeof header) {
        *failure = "state-snapshot-or-canonical-size-failed";
        goto cleanup;
    }
    memcpy(rom_before, machine.rom, rom_length);
    time_digest = gbl_test_time_state_digest(&machine);
    header_length = snprintf(
        header, sizeof header,
        "{\"schema\":\"gamebloke.time-authority/1\",\"termination\":%u,\"reason\":%u,\"cycles\":\"%" PRIu64 "\",\"instructions\":\"%" PRIu64 "\",\"master_cycles\":\"%" PRIu64 "\",\"time_state_digest\":\"%016" PRIx64 "\",\"trace_count\":%zu}\n",
        (unsigned int)total.termination, (unsigned int)total.reason,
        total.cycles, total.instructions, machine.master_cycles,
        time_digest, trace.count);
    if (header_length < 0 || (size_t)header_length >= sizeof header) {
        *failure = "canonical-header-failed";
        goto cleanup;
    }
    canonical.length = (size_t)header_length + trace_bytes.length;
    canonical.data = malloc(canonical.length);
    if (canonical.data == NULL) {
        *failure = "canonical-observation-allocation-failed";
        goto cleanup;
    }
    memcpy(canonical.data, header, (size_t)header_length);
    memcpy(canonical.data + (size_t)header_length, trace_bytes.data,
           trace_bytes.length);
    if (!gbl_test_hash(&canonical, digest)) {
        *failure = "time-authority-hash-failed";
        goto cleanup;
    }
    if (machine.master_cycles == UINT64_MAX) {
        *failure = "final-cycle-boundary-overflow";
        goto cleanup;
    }
    alternate_time_digest = gbl_test_time_state_digest_at(
        &machine, machine.master_cycles + UINT64_C(1));
    alternate_header_length = snprintf(
        alternate_header, sizeof alternate_header,
        "{\"schema\":\"gamebloke.time-authority/1\",\"termination\":%u,\"reason\":%u,\"cycles\":\"%" PRIu64 "\",\"instructions\":\"%" PRIu64 "\",\"master_cycles\":\"%" PRIu64 "\",\"time_state_digest\":\"%016" PRIx64 "\",\"trace_count\":%zu}\n",
        (unsigned int)total.termination, (unsigned int)total.reason,
        total.cycles, total.instructions, machine.master_cycles + UINT64_C(1),
        alternate_time_digest, trace.count);
    if (alternate_header_length < 0 ||
        (size_t)alternate_header_length >= sizeof alternate_header ||
        (size_t)alternate_header_length >
            SIZE_MAX - trace_bytes.length) {
        *failure = "alternate-final-cycle-header-failed";
        goto cleanup;
    }
    alternate_canonical.length =
        (size_t)alternate_header_length + trace_bytes.length;
    alternate_canonical.data = malloc(alternate_canonical.length);
    if (alternate_canonical.data == NULL) {
        *failure = "alternate-final-cycle-allocation-failed";
        goto cleanup;
    }
    memcpy(alternate_canonical.data, alternate_header,
           (size_t)alternate_header_length);
    memcpy(alternate_canonical.data + (size_t)alternate_header_length,
           trace_bytes.data, trace_bytes.length);
    if (!gbl_test_hash(&alternate_canonical, alternate_digest) ||
        strcmp(digest, alternate_digest) == 0) {
        *failure = "final-master-cycles-not-bound-to-digest";
        goto cleanup;
    }

    if (!gbl_test_canonical_equal(&machine_before, &machine) ||
        !gbl_test_same_instance_identity_equal(&machine_before, &machine) ||
        memcmp(machine.rom, rom_before, rom_length) != 0) {
        *failure = "observation-mutated-machine-state-or-time";
        goto cleanup;
    }
    output = fopen(output_path, "wbx");
    if (output == NULL) {
        *failure = "canonical-observation-write-failed";
        goto cleanup;
    }
    if (fwrite(canonical.data, 1u, canonical.length, output) !=
        canonical.length) {
        (void)fclose(output);
        output = NULL;
        *failure = "canonical-observation-write-failed";
        goto cleanup;
    }
    if (fclose(output) != 0) {
        output = NULL;
        *failure = "canonical-observation-write-failed";
        goto cleanup;
    }
    output = NULL;
    okay = true;

cleanup:
    if (output != NULL) (void)fclose(output);
    free(rom_before);
    free(trace_bytes.data);
    free(canonical.data);
    free(alternate_canonical.data);
    if (trace_path[0] != '\0') (void)unlink(trace_path);
    if (work[0] != '\0') (void)rmdir(work);
    if (initialized) gbl_machine_dispose(&machine);
    if (okay) {
        printf("PASS determinism.time_authority: mode=%s termination=%u reason=%u cycles=%" PRIu64 " instructions=%" PRIu64 " master_cycles=%" PRIu64 " trace_count=%zu digest=%s\n",
               mode, (unsigned int)total.termination,
               (unsigned int)total.reason, total.cycles, total.instructions,
               machine_before.master_cycles, trace.count, digest);
    }
    return okay;
}

static int gbl_test_time_authority_main(const char *mode,
                                        const char *output_path)
{
    const char *failure = "unknown";

    if (gbl_test_time_authority_observe(mode, output_path, &failure)) {
        return 0;
    }
    fprintf(stderr, "FAIL determinism.time_authority: %s\n", failure);
    return 1;
}

static bool gbl_test_run_result_equal(const gbl_run_result *left,
                                      const gbl_run_result *right)
{
    return left->termination == right->termination &&
           left->reason == right->reason && left->cycles == right->cycles &&
           left->instructions == right->instructions &&
           left->wait_cycles == right->wait_cycles &&
           left->wake_cycle == right->wake_cycle &&
           left->wake_source_mask == right->wake_source_mask &&
           left->power_cause == right->power_cause;
}

static int gbl_test_phase8_controls_main(void)
{
    gbl_machine clean = { 0 };
    gbl_machine mutant;
    gbl_run_result clean_result = { 0 };
    gbl_run_result wake_mutant;

    clean.power_state = GBL_MACHINE_POWER_RUNNING;
    mutant = clean;
    mutant.power_state = GBL_MACHINE_POWER_HALTED;
    if (gbl_test_canonical_equal(&clean, &mutant) ||
        gbl_test_canonical_digest(&clean) ==
            gbl_test_canonical_digest(&mutant)) {
        fputs("FAIL determinism.phase8.controls: power-state-mutation-accepted\n",
              stderr);
        return 1;
    }

    clean.input_event_count = 2u;
    clean.input_events[0] = (gbl_input_event){ UINT64_C(10), UINT16_C(1) };
    clean.input_events[1] = (gbl_input_event){ UINT64_C(20), UINT16_C(0) };
    mutant = clean;
    mutant.input_event_cursor = 1u;
    if (gbl_test_canonical_equal(&clean, &mutant) ||
        gbl_test_canonical_digest(&clean) ==
            gbl_test_canonical_digest(&mutant)) {
        fputs("FAIL determinism.phase8.controls: timeline-cursor-mutation-accepted\n",
              stderr);
        return 1;
    }
    mutant = clean;
    mutant.pressed_keys ^= UINT16_C(1);
    if (gbl_test_canonical_equal(&clean, &mutant)) {
        fputs("FAIL determinism.phase8.controls: key-bit-mutation-accepted\n",
              stderr);
        return 1;
    }
    mutant = clean;
    mutant.cpu.irq_observation.sampled = true;
    mutant.cpu.irq_observation.sample_cycle = UINT64_C(10);
    mutant.cpu.irq_observation.interrupt_flags = UINT16_C(8);
    if (gbl_test_canonical_equal(&clean, &mutant)) {
        fputs("FAIL determinism.phase8.controls: irq-observation-mutation-accepted\n",
              stderr);
        return 1;
    }
    clean_result.termination = GBL_RUN_TIMEOUT;
    clean_result.reason = GBL_RUN_REASON_CYCLE_BUDGET;
    clean_result.wait_cycles = UINT64_C(4);
    wake_mutant = clean_result;
    wake_mutant.wake_cycle = UINT64_C(4);
    wake_mutant.wake_source_mask = UINT16_C(8);
    if (gbl_test_run_result_equal(&clean_result, &wake_mutant)) {
        fputs("FAIL determinism.phase8.controls: wake-cycle-mutation-accepted\n",
              stderr);
        return 1;
    }
    puts("PASS determinism.phase8.controls: power, key, timeline, IRQ and wake mutations rejected");
    return 0;
}

typedef struct gbl_test_cancel_after {
    unsigned int calls;
    unsigned int cancel_on;
} gbl_test_cancel_after;

static bool gbl_test_cancel_at_boundary(void *context)
{
    gbl_test_cancel_after *cancel = context;

    cancel->calls++;
    return cancel->calls >= cancel->cancel_on;
}

static bool gbl_test_phase8_cancel_run(
    const char *rom_path, const gbl_input_event *events, size_t event_count,
    unsigned int cancel_on, gbl_runner_result *result,
    gbl_test_bytes *json)
{
    gbl_test_cancel_after cancel = { 0u, cancel_on };
    gbl_runner_options options = { 0 };

    options.rom_path = rom_path;
    options.fixture_id = "synthetic-control-pass";
    options.cycle_budget = UINT64_C(1000);
    options.trace_mask = GBL_TRACE_CATEGORY_MASK;
    options.trace_capacity = GBL_DETERMINISM_TRACE_CAPACITY;
    options.cancelled = gbl_test_cancel_at_boundary;
    options.cancel_context = &cancel;
    options.input_timeline_present = true;
    options.input_events = events;
    options.input_event_count = event_count;
    return gbl_runner_execute(&options, result) == GBL_RUNNER_CANCELLED &&
           result->status == GBL_RUNNER_CANCELLED &&
           strcmp(result->termination, "cancelled") == 0 &&
           gbl_test_json(result, json);
}

static bool gbl_test_phase8_replacement(const uint8_t *image,
                                       size_t image_length)
{
    static const gbl_input_event pending_a[] = {
        { UINT64_C(30), UINT16_C(1) }, { UINT64_C(40), UINT16_C(0) }
    };
    static const gbl_input_event pending_b[] = {
        { UINT64_C(31), UINT16_C(2) }, { UINT64_C(41), UINT16_C(0) }
    };
    static const gbl_input_event replacement[] = {
        { UINT64_C(10), UINT16_C(1) },
        { UINT64_C(10), UINT16_C(2) },
        { UINT64_C(20), UINT16_C(0) }
    };
    gbl_interleave_instance first = { 0 };
    gbl_interleave_instance second = { 0 };
    const bool okay =
        gbl_interleave_load(&first, image, image_length, UINT64_C(1000)) &&
        gbl_interleave_load(&second, image, image_length, UINT64_C(1000)) &&
        gbl_interleave_set_timeline(&first, pending_a,
                                    sizeof pending_a / sizeof pending_a[0]) &&
        gbl_interleave_set_timeline(&second, pending_b,
                                    sizeof pending_b / sizeof pending_b[0]) &&
        gbl_machine_advance_to(&first.machine, UINT64_C(10)) ==
            GBL_TIME_STATUS_OK &&
        gbl_machine_advance_to(&second.machine, UINT64_C(10)) ==
            GBL_TIME_STATUS_OK &&
        gbl_interleave_set_timeline(
            &first, replacement, sizeof replacement / sizeof replacement[0]) &&
        gbl_interleave_set_timeline(
            &second, replacement,
            sizeof replacement / sizeof replacement[0]) &&
        gbl_machine_advance_to(&first.machine, UINT64_C(10)) ==
            GBL_TIME_STATUS_OK &&
        gbl_machine_advance_to(&second.machine, UINT64_C(10)) ==
            GBL_TIME_STATUS_OK &&
        first.machine.pressed_keys == UINT16_C(2) &&
        first.machine.input_event_cursor == 2u &&
        gbl_machine_advance_to(&first.machine, UINT64_C(20)) ==
            GBL_TIME_STATUS_OK &&
        gbl_machine_advance_to(&second.machine, UINT64_C(20)) ==
            GBL_TIME_STATUS_OK &&
        gbl_test_canonical_equal(&first.machine, &second.machine);
    const bool disposed = gbl_interleave_dispose(&first) &&
                          gbl_interleave_dispose(&second);

    return okay && disposed;
}

static bool gbl_test_phase8_cancellation(const uint8_t *image,
                                         size_t image_length)
{
    static const gbl_input_event events[] = {
        { UINT64_C(0), UINT16_C(1) },
        { UINT64_C(100), UINT16_C(0) }
    };
    char rom_path[GBL_DETERMINISM_PATH_CAPACITY] = { 0 };
    gbl_runner_result before = { 0 };
    gbl_runner_result after = { 0 };
    gbl_runner_result after_twin = { 0 };
    gbl_test_bytes before_json = { 0 };
    gbl_test_bytes after_json = { 0 };
    gbl_test_bytes after_twin_json = { 0 };
    bool okay = false;

    (void)image;
    (void)image_length;
    if (!gbl_test_path(rom_path, sizeof rom_path,
                       GBL_DETERMINISM_FIXTURE_DIR, "owned-arm-pass.gba") ||
        !gbl_test_phase8_cancel_run(rom_path, events,
                                    sizeof events / sizeof events[0], 1u,
                                    &before, &before_json) ||
        !gbl_test_phase8_cancel_run(rom_path, events,
                                    sizeof events / sizeof events[0], 2u,
                                    &after, &after_json) ||
        !gbl_test_phase8_cancel_run(rom_path, events,
                                    sizeof events / sizeof events[0], 2u,
                                    &after_twin, &after_twin_json)) {
        goto done;
    }
    okay = before.cycles == 0u && before.phase8.applied_input_events == 0u &&
           after.cycles > 0u && after.phase8.applied_input_events == 1u &&
           after.phase8.keyinput == UINT16_C(0x03FE) &&
           after_twin.cycles == after.cycles &&
           after_twin.phase8.applied_input_events ==
               after.phase8.applied_input_events &&
           after_twin.phase8.keyinput == after.phase8.keyinput &&
           after_json.length == after_twin_json.length &&
           memcmp(after_json.data, after_twin_json.data, after_json.length) ==
               0;
    if (!okay) {
        fprintf(stderr,
                "phase8 cancel evidence: before cycles=%" PRIu64
                " applied=%zu; after cycles=%" PRIu64
                " applied=%zu keyinput=%u; twin cycles=%" PRIu64
                " applied=%zu keyinput=%u; json=%zu/%zu\n",
                before.cycles, before.phase8.applied_input_events,
                after.cycles, after.phase8.applied_input_events,
                (unsigned int)after.phase8.keyinput, after_twin.cycles,
                after_twin.phase8.applied_input_events,
                (unsigned int)after_twin.phase8.keyinput, after_json.length,
                after_twin_json.length);
    }

done:
    gbl_runner_result_dispose(&before);
    gbl_runner_result_dispose(&after);
    gbl_runner_result_dispose(&after_twin);
    free(before_json.data);
    free(after_json.data);
    free(after_twin_json.data);
    return okay;
}

static bool gbl_test_phase8_wait_instance(
    gbl_interleave_instance *instance, const uint8_t *image,
    size_t image_length)
{
    static const gbl_input_event event = {
        UINT64_C(4), UINT16_C(1)
    };
    gbl_bus_result timer_write;
    gbl_bus_result keypad_write;

    if (!gbl_interleave_load(instance, image, image_length,
                             UINT64_C(1000))) {
        return false;
    }
    instance->machine.cpu.cpsr = UINT32_C(0x0000001F);
    instance->machine.cpu.execution_address = UINT32_C(0x08000000);
    instance->machine.cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000000), UINT32_C(0xE1A00000), 4u, true
    };
    instance->machine.cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000004), UINT32_C(0xE1A00000), 4u, true
    };
    instance->machine.cpu.next_fetch_sequential = true;
    timer_write = gbl_bus_write(
        &instance->machine, UINT32_C(0x04000100), 4u,
        GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x00C0FFFD));
    keypad_write = gbl_bus_write(
        &instance->machine, UINT32_C(0x04000132), 2u,
        GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x4001));
    if (timer_write.status != GBL_BUS_STATUS_OK ||
        keypad_write.status != GBL_BUS_STATUS_OK ||
        instance->machine.master_cycles != UINT64_C(2) ||
        instance->machine.timers[0].enabled == false ||
        instance->machine.interrupt_flags != 0u) {
        return false;
    }
    instance->machine.interrupt_enable = UINT16_C(0x1008);
    instance->machine.interrupt_master_enable = 0u;
    if (gbl_machine_set_input_timeline(&instance->machine, &event, 1u) !=
            GBL_TIME_STATUS_OK ||
        gbl_machine_set_power_state(&instance->machine,
                                    GBL_MACHINE_POWER_HALTED) !=
            GBL_TIME_STATUS_OK) {
        return false;
    }
    return true;
}

static bool gbl_test_phase8_wait_resume(const uint8_t *image,
                                        size_t image_length)
{
    gbl_interleave_instance before_wake = { 0 };
    gbl_interleave_instance split = { 0 };
    gbl_interleave_instance unsplit = { 0 };
    const gbl_run_options unit_budget = {
        UINT64_C(1), 0u, 0u, NULL
    };
    const gbl_run_options combined_budget = {
        UINT64_C(2), 0u, 0u, NULL
    };
    gbl_run_result cancelled_before_wake = { 0 };
    gbl_run_result split_timeout = { 0 };
    gbl_run_result split_wake = { 0 };
    gbl_run_result unsplit_wake = { 0 };
    gbl_run_result split_total = { 0 };
    bool okay = false;
    bool disposed;

    if (!gbl_test_phase8_wait_instance(&before_wake, image, image_length) ||
        !gbl_test_phase8_wait_instance(&split, image, image_length) ||
        !gbl_test_phase8_wait_instance(&unsplit, image, image_length) ||
        gbl_cpu_run(&before_wake.machine, &unit_budget,
                    &cancelled_before_wake) != GBL_RUN_STATUS_OK ||
        gbl_cpu_run(&split.machine, &unit_budget, &split_timeout) !=
            GBL_RUN_STATUS_OK ||
        cancelled_before_wake.termination != GBL_RUN_TIMEOUT ||
        cancelled_before_wake.reason != GBL_RUN_REASON_CYCLE_BUDGET ||
        cancelled_before_wake.wait_cycles != UINT64_C(1) ||
        before_wake.machine.power_state != GBL_MACHINE_POWER_HALTED ||
        before_wake.machine.input_event_cursor != 0u ||
        before_wake.machine.interrupt_flags != 0u ||
        !gbl_test_canonical_equal(&before_wake.machine, &split.machine) ||
        gbl_cpu_run(&split.machine, &unit_budget, &split_wake) !=
            GBL_RUN_STATUS_OK ||
        gbl_cpu_run(&unsplit.machine, &combined_budget, &unsplit_wake) !=
            GBL_RUN_STATUS_OK) {
        goto done;
    }
    split_total = split_wake;
    if (UINT64_MAX - split_total.cycles < split_timeout.cycles ||
        UINT64_MAX - split_total.wait_cycles < split_timeout.wait_cycles) {
        goto done;
    }
    split_total.cycles += split_timeout.cycles;
    split_total.wait_cycles += split_timeout.wait_cycles;
    okay = gbl_test_run_result_equal(&split_total, &unsplit_wake) &&
           split.machine.power_state == GBL_MACHINE_POWER_RUNNING &&
           split.machine.master_cycles == UINT64_C(4) &&
           split.machine.input_event_cursor == 1u &&
           split.machine.pressed_keys == UINT16_C(1) &&
           split.machine.interrupt_flags == UINT16_C(0x1008) &&
           split_wake.power_cause == GBL_CPU_POWER_CAUSE_WAKE_EVENT &&
           split_wake.wake_cycle == UINT64_C(4) &&
           split_wake.wake_source_mask == UINT16_C(0x1008) &&
           gbl_test_canonical_equal(&split.machine, &unsplit.machine);

done:
    disposed = gbl_interleave_dispose(&before_wake);
    disposed = gbl_interleave_dispose(&split) && disposed;
    disposed = gbl_interleave_dispose(&unsplit) && disposed;
    return okay && disposed;
}

static bool gbl_test_phase8_replay(const uint8_t *arm_image,
                                   size_t arm_length,
                                   const uint8_t *thumb_image,
                                   size_t thumb_length,
                                   const char **failure)
{
    static const gbl_input_event arm_timeline[] = {
        { UINT64_C(0), UINT16_C(1) },
        { UINT64_C(10), UINT16_C(0) },
        { UINT64_C(200), UINT16_C(2) }
    };
    static const gbl_input_event thumb_timeline[] = {
        { UINT64_C(0), UINT16_C(4) },
        { UINT64_C(10), UINT16_C(0) },
        { UINT64_C(200), UINT16_C(8) }
    };
    gbl_interleave_instance fresh = { 0 };
    gbl_interleave_instance split = { 0 };
    gbl_interleave_instance reset = { 0 };
    gbl_interleave_instance isolated_arm = { 0 };
    gbl_interleave_instance isolated_thumb = { 0 };
    gbl_interleave_instance alternating_arm = { 0 };
    gbl_interleave_instance alternating_thumb = { 0 };
    unsigned int unit;
    bool okay = false;

    if (!gbl_interleave_load(&fresh, arm_image, arm_length, UINT64_C(1000)) ||
        !gbl_interleave_set_timeline(&fresh, arm_timeline,
                                     sizeof arm_timeline /
                                         sizeof arm_timeline[0]) ||
        !gbl_interleave_run_isolated(&fresh, failure) ||
        !gbl_interleave_serialize_trace(&fresh) ||
        fresh.mailbox.status != GBL_MAILBOX_PASS ||
        fresh.mailbox.case_id != UINT32_C(0x9001) ||
        fresh.machine.cpu.stopped || fresh.machine.input_event_cursor != 2u ||
        fresh.machine.pressed_keys != 0u) {
        fprintf(stderr,
                "phase8 fresh evidence: mailbox=%u/%" PRIu32
                " stopped=%u cursor=%zu keys=%u cycles=%" PRIu64 "\n",
                (unsigned int)fresh.mailbox.status, fresh.mailbox.case_id,
                fresh.machine.cpu.stopped ? 1u : 0u,
                fresh.machine.input_event_cursor,
                (unsigned int)fresh.machine.pressed_keys, fresh.cycles);
        *failure = "fresh-input-replay-failed";
        goto done;
    }
    if (!gbl_interleave_load(&split, arm_image, arm_length, UINT64_C(1000)) ||
        !gbl_interleave_set_timeline(&split, arm_timeline,
                                     sizeof arm_timeline /
                                         sizeof arm_timeline[0])) {
        *failure = "split-input-replay-setup-failed";
        goto done;
    }
    for (unit = 0u; unit < 2u; ++unit) {
        if (!gbl_interleave_step(&split, failure)) {
            *failure = "split-input-prefix-failed";
            goto done;
        }
    }
    if (split.machine.input_event_cursor == 0u ||
        !gbl_interleave_run_isolated(&split, failure) ||
        !gbl_interleave_serialize_trace(&split) ||
        strcmp(gbl_interleave_compare(&fresh, &split), "ok") != 0) {
        *failure = "split-input-replay-differs-from-fresh";
        goto done;
    }

    if (!gbl_interleave_load(&reset, arm_image, arm_length, UINT64_C(1000)) ||
        !gbl_interleave_set_timeline(&reset, arm_timeline,
                                     sizeof arm_timeline /
                                         sizeof arm_timeline[0]) ||
        !gbl_interleave_step(&reset, failure) ||
        reset.machine.input_event_cursor == 0u ||
        !gbl_interleave_reset_reload(&reset, arm_image, arm_length,
                                     UINT64_C(1000)) ||
        !gbl_interleave_set_timeline(&reset, arm_timeline,
                                     sizeof arm_timeline /
                                         sizeof arm_timeline[0]) ||
        !gbl_interleave_run_isolated(&reset, failure) ||
        !gbl_interleave_serialize_trace(&reset) ||
        strcmp(gbl_interleave_compare(&fresh, &reset), "ok") != 0) {
        *failure = "partial-timeline-reset-differs-from-fresh";
        goto done;
    }

    if (!gbl_interleave_load(&isolated_arm, arm_image, arm_length,
                             UINT64_C(1000)) ||
        !gbl_interleave_load(&alternating_arm, arm_image, arm_length,
                             UINT64_C(1000)) ||
        !gbl_interleave_set_timeline(&isolated_arm, arm_timeline,
                                     sizeof arm_timeline /
                                         sizeof arm_timeline[0]) ||
        !gbl_interleave_set_timeline(&alternating_arm, arm_timeline,
                                     sizeof arm_timeline /
                                         sizeof arm_timeline[0]) ||
        !gbl_interleave_load(&isolated_thumb, thumb_image, thumb_length,
                             UINT64_C(1000)) ||
        !gbl_interleave_load(&alternating_thumb, thumb_image, thumb_length,
                             UINT64_C(1000)) ||
        !gbl_interleave_set_timeline(&isolated_thumb, thumb_timeline,
                                     sizeof thumb_timeline /
                                         sizeof thumb_timeline[0]) ||
        !gbl_interleave_set_timeline(&alternating_thumb, thumb_timeline,
                                     sizeof thumb_timeline /
                                         sizeof thumb_timeline[0]) ||
        !gbl_interleave_run_isolated(&isolated_arm, failure) ||
        !gbl_interleave_run_isolated(&isolated_thumb, failure) ||
        !gbl_interleave_run_alternating(&alternating_arm, &alternating_thumb,
                                        failure) ||
        !gbl_interleave_serialize_trace(&isolated_arm) ||
        !gbl_interleave_serialize_trace(&isolated_thumb) ||
        !gbl_interleave_serialize_trace(&alternating_arm) ||
        !gbl_interleave_serialize_trace(&alternating_thumb) ||
        strcmp(gbl_interleave_compare(&isolated_arm, &alternating_arm),
               "ok") != 0 ||
        strcmp(gbl_interleave_compare(&isolated_thumb, &alternating_thumb),
               "ok") != 0 ||
        isolated_arm.mailbox.status != GBL_MAILBOX_PASS ||
        isolated_arm.mailbox.case_id != UINT32_C(0x9001) ||
        isolated_thumb.mailbox.status != GBL_MAILBOX_PASS ||
        isolated_thumb.mailbox.case_id != UINT32_C(0x9005) ||
        isolated_arm.machine.cpu.stopped || isolated_thumb.machine.cpu.stopped ||
        isolated_arm.machine.input_event_cursor != 2u ||
        isolated_thumb.machine.input_event_cursor != 3u) {
        *failure = "distinct-input-timelines-leaked-between-instances";
        goto done;
    }
    okay = true;

done:
    {
        bool disposed = true;

        disposed = gbl_interleave_dispose(&fresh) && disposed;
        disposed = gbl_interleave_dispose(&split) && disposed;
        disposed = gbl_interleave_dispose(&reset) && disposed;
        disposed = gbl_interleave_dispose(&isolated_arm) && disposed;
        disposed = gbl_interleave_dispose(&isolated_thumb) && disposed;
        disposed = gbl_interleave_dispose(&alternating_arm) && disposed;
        disposed = gbl_interleave_dispose(&alternating_thumb) && disposed;
        if (!disposed) {
            *failure = "phase8-replay-disposal-failed";
            return false;
        }
    }
    return okay;
}

static int gbl_test_phase8_main(void)
{
    gbl_test_bytes arm_image = { 0 };
    gbl_test_bytes thumb_image = { 0 };
    const char *failure = "phase8-fixture-read-failed";
    bool okay = false;

    if (!gbl_interleave_fixture(&arm_image, "owned-arm-pass.gba") ||
        !gbl_interleave_fixture(&thumb_image,
                                "owned-arm-thumb-bx-pass.gba")) {
        failure = "phase8-fixture-read-failed";
    } else if (!gbl_test_phase8_replay(arm_image.data, arm_image.length,
                                      thumb_image.data, thumb_image.length,
                                      &failure)) {
        /* Replay helper supplies its own stable failure reason. */
    } else if (!gbl_test_phase8_replacement(arm_image.data,
                                            arm_image.length)) {
        failure = "same-cycle-timeline-replacement-differs";
    } else if (!gbl_test_phase8_cancellation(arm_image.data,
                                             arm_image.length)) {
        failure = "input-cancellation-boundary-differs";
    } else if (!gbl_test_phase8_wait_resume(arm_image.data,
                                            arm_image.length)) {
        failure = "halt-timeout-resume-or-budget-boundary-wake-differs";
    } else {
        okay = true;
    }
    free(arm_image.data);
    free(thumb_image.data);
    if (!okay) {
        fprintf(stderr, "FAIL determinism.phase8: %s\n", failure);
        return 1;
    }
    puts("PASS determinism.phase8: fresh, split, reset and interleaved ARM/Thumb input timelines match named state and traces");
    return 0;
}

static int gbl_test_phase9_main(void)
{
    uint8_t image[0xC0u] = { 0 };
    gbl_interleave_instance whole = { 0 };
    gbl_interleave_instance split = { 0 };
    gbl_interleave_instance reset = { 0 };
    gbl_interleave_instance isolated_b = { 0 };
    gbl_interleave_instance alternating_a = { 0 };
    gbl_interleave_instance alternating_b = { 0 };
    const char *failure = "phase9-synthetic-image-setup-failed";
    uint64_t boundary_default_cycles = 0u;
    uint64_t boundary_default_source_cycles = 0u;
    uint64_t boundary_waitcnt_cycles = 0u;
    uint64_t boundary_waitcnt_source_cycles = 0u;
    bool okay = false;
    bool disposed = true;

    /* MOV r7,#0x42; SWI 0. The image has no external fixture or ROM bytes. */
    image[0] = UINT8_C(0x42);
    image[1] = UINT8_C(0x70);
    image[2] = UINT8_C(0xA0);
    image[3] = UINT8_C(0xE3);
    image[4] = UINT8_C(0x00);
    image[5] = UINT8_C(0x00);
    image[6] = UINT8_C(0x00);
    image[7] = UINT8_C(0xEF);

    if (!gbl_phase9_prepare(&whole, image, sizeof image, false, &failure) ||
        !gbl_phase9_prepare(&split, image, sizeof image, false, &failure) ||
        !gbl_phase9_prepare(&reset, image, sizeof image, false, &failure) ||
        !gbl_phase9_prepare(&isolated_b, image, sizeof image, true, &failure) ||
        !gbl_phase9_prepare(&alternating_a, image, sizeof image, false,
                            &failure) ||
        !gbl_phase9_prepare(&alternating_b, image, sizeof image, true,
                            &failure)) {
        goto done;
    }
    if (!gbl_phase9_run_slice(&whole, UINT64_C(1000), 0u, &failure)) {
        goto done;
    }
    if (!whole.complete) {
        failure = "phase9-whole-run-did-not-reach-terminal";
        goto done;
    }
    if (!gbl_interleave_serialize_trace(&whole)) {
        failure = "phase9-whole-trace-serialization-failed";
        goto done;
    }
    if (!gbl_phase9_semantics(&whole, false)) {
        failure = "phase9-whole-run-semantic-or-timing-mismatch";
        goto done;
    }
    if (!gbl_phase9_trace_order(&whole)) {
        failure = "phase9-whole-dma-trace-order-mismatch";
        goto done;
    }

    /* Resume at one cycle below startup, exact startup, and one source plus
     * destination unit. The higher-priority DMA event is due at that unit
     * boundary, so DMA3 must retain its progress and resume after DMA0. */
    if (!gbl_phase9_run_slice(&split, UINT64_C(1), 0u, &failure) ||
        split.cycles != 0u || !split.machine.dma_channels[3].pending ||
        !gbl_phase9_run_slice(&split, UINT64_C(2), 0u, &failure) ||
        split.cycles != UINT64_C(2) ||
        !split.machine.dma_channels[3].startup_complete ||
        !gbl_phase9_run_slice(&split, UINT64_C(2), 0u, &failure) ||
        split.cycles != UINT64_C(4) ||
        split.dma_completed_units != UINT64_C(1) ||
        split.dma_pause_cycles != UINT64_C(4) ||
        !split.machine.dma_channels[3].active ||
        split.machine.dma_channels[3].remaining != 1u ||
        !split.machine.dma_channels[0].pending ||
        !gbl_phase9_run_slice(&split, UINT64_C(1000), 0u, &failure) ||
        !split.complete || !gbl_interleave_serialize_trace(&split) ||
        !gbl_phase9_semantics(&split, false) ||
        strcmp(gbl_interleave_compare(&whole, &split), "ok") != 0) {
        failure = "phase9-split-startup-or-resume-mismatch";
        goto done;
    }

    /* Reset while one channel is ready and the lower-priority channel is
     * armed. Reset must clear scheduled/mutable state while retaining the
     * loaded ROM allocation, then replay the same transfer as a fresh run. */
    {
        const uint8_t *rom_identity = reset.machine.rom;
        gbl_machine_reset(&reset.machine);
        if (reset.machine.rom != rom_identity ||
            reset.machine.event_count != 1u ||
            reset.machine.next_event_sequence != 1u ||
            reset.machine.events[0].cycle != 0u ||
            reset.machine.events[0].sequence != 0u ||
            reset.machine.events[0].generation !=
                reset.machine.ppu.generation ||
            reset.machine.events[0].kind !=
                (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY ||
            !reset.machine.events[0].live ||
            reset.machine.waitcnt != 0u ||
            reset.machine.bus_owner != GBL_MACHINE_BUS_OWNER_CPU ||
            reset.machine.interrupt_flags != 0u ||
            reset.machine.dma_channels[0].armed ||
            reset.machine.dma_channels[0].pending ||
            reset.machine.dma_channels[3].armed ||
            reset.machine.dma_channels[3].pending ||
            !gbl_phase9_reset_instance(&reset) ||
            !gbl_phase9_configure(&reset, false, &failure) ||
            !gbl_phase9_run_slice(&reset, UINT64_C(1000), 0u, &failure) ||
            !reset.complete || !gbl_interleave_serialize_trace(&reset) ||
            !gbl_phase9_semantics(&reset, false) ||
            strcmp(gbl_interleave_compare(&whole, &reset), "ok") != 0) {
            failure = "phase9-reset-state-or-instance-identity-leaked";
            goto done;
        }
    }

    if (!gbl_interleave_run_isolated(&isolated_b, &failure) ||
        !gbl_interleave_run_alternating(&alternating_a, &alternating_b,
                                        &failure) ||
        !gbl_interleave_serialize_trace(&isolated_b) ||
        !gbl_interleave_serialize_trace(&alternating_a) ||
        !gbl_interleave_serialize_trace(&alternating_b) ||
        !gbl_phase9_semantics(&isolated_b, true) ||
        !gbl_phase9_semantics(&alternating_a, false) ||
        !gbl_phase9_semantics(&alternating_b, true) ||
        strcmp(gbl_interleave_compare(&isolated_b, &alternating_b), "ok") !=
            0 ||
        strcmp(gbl_interleave_compare(&whole, &alternating_a), "ok") != 0) {
        failure = "phase9-interleaved-instances-contaminated-each-other";
        goto done;
    }
    if (!gbl_phase9_boundary_case(UINT16_C(0x0000),
                                 &boundary_default_cycles,
                                 &boundary_default_source_cycles) ||
        !gbl_phase9_boundary_case(UINT16_C(0x0014),
                                 &boundary_waitcnt_cycles,
                                 &boundary_waitcnt_source_cycles) ||
        boundary_default_cycles == boundary_waitcnt_cycles ||
        boundary_default_source_cycles == boundary_waitcnt_source_cycles) {
        failure = "phase9-128k-community-boundary-or-waitcnt-window-mismatch";
        goto done;
    }
    okay = true;

done:
    disposed = gbl_interleave_dispose(&whole) && disposed;
    disposed = gbl_interleave_dispose(&split) && disposed;
    disposed = gbl_interleave_dispose(&reset) && disposed;
    disposed = gbl_interleave_dispose(&isolated_b) && disposed;
    disposed = gbl_interleave_dispose(&alternating_a) && disposed;
    disposed = gbl_interleave_dispose(&alternating_b) && disposed;
    if (!disposed) {
        failure = "phase9-machine-disposal-failed";
        okay = false;
    }
    if (!okay) {
        fprintf(stderr, "FAIL determinism.phase9: %s\n", failure);
        return 1;
    }
    printf("PASS determinism.phase9.boundary: 128 KiB source transition forces N under WAITCNT 0x0000/0x0014; provenance=GBATEK-3.06 hardware=UNKNOWN prefetch=unmodeled (%" PRIu64 "/%" PRIu64 " source cycles)\n",
           boundary_default_source_cycles, boundary_waitcnt_source_cycles);
    puts("PASS determinism.phase9: fresh, split, reset and interleaved synthetic DMA runs preserve semantics, timing and disputed evidence labels");
    return 0;
}

static int gbl_test_phase9_controls_main(void)
{
    uint8_t image[0xC0u] = { 0 };
    gbl_interleave_instance clean = { 0 };
    gbl_interleave_instance twin = { 0 };
    gbl_interleave_instance mutant;
    gbl_trace_record *mutant_records = NULL;
    const char *failure = "phase9-control-setup-failed";
    bool okay = false;

    image[0] = UINT8_C(0x42);
    image[1] = UINT8_C(0x70);
    image[2] = UINT8_C(0xA0);
    image[3] = UINT8_C(0xE3);
    image[7] = UINT8_C(0xEF);
    if (!gbl_phase9_prepare(&clean, image, sizeof image, false, &failure) ||
        !gbl_phase9_prepare(&twin, image, sizeof image, false, &failure) ||
        !gbl_phase9_run_slice(&clean, UINT64_C(1000), 0u, &failure) ||
        !gbl_phase9_run_slice(&twin, UINT64_C(1000), 0u, &failure) ||
        !clean.complete || !twin.complete ||
        !gbl_interleave_serialize_trace(&clean) ||
        !gbl_interleave_serialize_trace(&twin) ||
        !gbl_phase9_semantics(&clean, false) ||
        !gbl_phase9_semantics(&twin, false) ||
        !gbl_phase9_trace_order(&clean) || !gbl_phase9_trace_order(&twin) ||
        strcmp(gbl_phase9_observation_compare(&clean, &twin), "ok") != 0) {
        failure = "phase9-control-clean-twin-setup-or-semantic-run-failed";
        goto done;
    }
    if (!gbl_phase9_canonical_field_controls(&clean.machine)) {
        failure = "phase9-canonical-dma-waitcnt-or-event-field-mutation-accepted";
        goto done;
    }
    if (!gbl_phase9_trace_field_controls()) {
        failure = "phase9-dma-trace-field-mutation-accepted";
        goto done;
    }

    mutant = twin;
    mutant.machine.iwram[0x100u] ^= UINT8_C(1);
    if (strcmp(gbl_phase9_observation_compare(&clean, &mutant),
               "dma-transfer-value") != 0) {
        failure = "dma-transfer-value-control-not-causal";
        goto done;
    }
    {
        gbl_interleave_instance stable_wrong_first = clean;
        gbl_interleave_instance stable_wrong_second = twin;

        stable_wrong_first.machine.iwram[0x100u] ^= UINT8_C(1);
        stable_wrong_second.machine.iwram[0x100u] ^= UINT8_C(1);
        if (strcmp(gbl_phase9_observation_compare(&stable_wrong_first,
                                                 &stable_wrong_second),
                   "ok") != 0 ||
            gbl_phase9_semantics(&stable_wrong_first, false) ||
            gbl_phase9_semantics(&stable_wrong_second, false)) {
            failure = "stable-wrong-transfer-was-accepted-by-semantic-oracle";
            goto done;
        }
    }

    mutant = twin;
    mutant_records = malloc(clean.trace.count * sizeof *mutant_records);
    if (mutant_records == NULL) {
        failure = "dma-event-order-control-allocation-failed";
        goto done;
    }
    memcpy(mutant_records, twin.trace.records,
           twin.trace.count * sizeof *mutant_records);
    mutant.trace.records = mutant_records;
    mutant.trace.records[0].sequence ^= UINT64_C(1);
    if (strcmp(gbl_phase9_observation_compare(&clean, &mutant),
               "dma-event-order") != 0) {
        failure = "dma-event-order-control-not-causal";
        goto done;
    }
    free(mutant_records);
    mutant_records = NULL;

    mutant = twin;
    mutant.machine.dma_channels[3].read_latch ^= UINT32_C(1);
    if (strcmp(gbl_phase9_observation_compare(&clean, &mutant),
               "dma-state-contamination") != 0) {
        failure = "dma-state-contamination-control-not-causal";
        goto done;
    }
    mutant = twin;
    mutant.dma_source_cycles += UINT64_C(1);
    if (strcmp(gbl_phase9_observation_compare(&clean, &mutant),
               "dma-cycle-accounting") != 0 ||
        strcmp(gbl_phase9_observation_compare(&clean, &twin), "ok") != 0) {
        failure = "dma-cycle-accounting-control-not-causal-or-clean-twin-failed";
        goto done;
    }
    okay = true;

done:
    free(mutant_records);
    (void)gbl_interleave_dispose(&clean);
    (void)gbl_interleave_dispose(&twin);
    if (!okay) {
        fprintf(stderr, "FAIL determinism.phase9.controls: %s\n", failure);
        return 1;
    }
    puts("PASS determinism.phase9.controls: semantic, trace, DMA state and cycle faults fail causally; clean twins and named field mutations are checked");
    return 0;
}

static uint16_t gbl_phase10_pattern_pixel(size_t pixel, bool variant)
{
    const size_t base = variant ? (size_t)UINT16_C(0x1778)
                                : (size_t)UINT16_C(0x1357);

    return (uint16_t)((pixel * 37u + base) & (size_t)UINT16_C(0x7FFF));
}

static bool gbl_phase10_seed(gbl_machine *machine, bool variant,
                             const char **failure)
{
    const gbl_input_event input[2] = {
        { UINT64_C(4), UINT16_C(1) },
        { UINT64_C(12), UINT16_C(0) }
    };
    gbl_dma_channel_state *channel;
    size_t pixel;

    if (machine == NULL || !machine->initialized || failure == NULL) {
        if (failure != NULL) *failure = "phase10-machine-not-initialized";
        return false;
    }
    machine->dispcnt = UINT16_C(0x0403);
    machine->ppu.dispcnt = UINT16_C(0x0403);
    for (pixel = 0u; pixel < GBL_PPU_VIDEO_BYTES / 2u; ++pixel) {
        const uint16_t color = gbl_phase10_pattern_pixel(pixel, variant);

        machine->vram[pixel * 2u] = (uint8_t)(color & UINT16_C(0x00FF));
        machine->vram[pixel * 2u + 1u] = (uint8_t)(color >> 8u);
    }
    machine->ewram[0x20u] = UINT8_C(0x6D);
    machine->ewram[0x21u] = UINT8_C(0xA2);
    channel = &machine->dma_channels[0];
    channel->generation = UINT64_C(1);
    channel->source_address = UINT32_C(0x02000020);
    channel->destination_address = UINT32_C(0x03000100);
    channel->current_source = channel->source_address;
    channel->current_destination = channel->destination_address;
    channel->count = 1u;
    channel->control = UINT16_C(0x9000);
    channel->remaining = 1u;
    channel->armed = true;
    if (gbl_machine_set_input_timeline(machine, input,
                                       sizeof input / sizeof input[0]) !=
        GBL_TIME_STATUS_OK) {
        *failure = "phase10-logical-input-setup-failed";
        return false;
    }
    return true;
}

static gbl_machine *gbl_phase10_new_machine(bool variant,
                                             const char **failure)
{
    gbl_machine *machine = calloc(1u, sizeof *machine);

    if (machine == NULL) {
        *failure = "phase10-machine-allocation-failed";
        return NULL;
    }
    if (gbl_machine_init(machine, NULL) != GBL_BUS_STATUS_OK) {
        free(machine);
        *failure = "phase10-machine-init-failed";
        return NULL;
    }
    if (!gbl_phase10_seed(machine, variant, failure)) {
        gbl_machine_dispose(machine);
        free(machine);
        return NULL;
    }
    return machine;
}

static void gbl_phase10_free_machine(gbl_machine **machine)
{
    if (machine == NULL || *machine == NULL) return;
    gbl_machine_dispose(*machine);
    free(*machine);
    *machine = NULL;
}

static bool gbl_phase10_advance(gbl_machine *machine, uint64_t target,
                                const char **failure)
{
    if (machine == NULL || target < machine->master_cycles ||
        gbl_machine_advance_to(machine, target) != GBL_TIME_STATUS_OK) {
        *failure = "phase10-scheduled-advance-failed";
        return false;
    }
    return true;
}

static bool gbl_phase10_frame_semantics(const gbl_machine *machine,
                                        bool variant)
{
    const uint64_t frame_end =
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES;
    size_t pixel;

    if (machine == NULL || machine->master_cycles != frame_end ||
        machine->ppu.frame_epoch != 0u ||
        machine->ppu.line != GBL_PPU_VISIBLE_LINES ||
        machine->ppu.captured_rows != GBL_PPU_VISIBLE_LINES ||
        machine->ppu.capture_state != GBL_VIDEO_CAPTURE_VALID ||
        machine->ppu.capture_reason != GBL_VIDEO_INVALID_REASON_NONE ||
        machine->ppu.dispcnt != UINT16_C(0x0403) ||
        machine->input_event_cursor != 2u || machine->pressed_keys != 0u ||
        !machine->dma_channels[0].pending ||
        machine->dma_channels[0].armed ||
        machine->dma_channels[0].trigger_origin !=
            GBL_DMA_TRIGGER_ORIGIN_VBLANK ||
        machine->dma_channels[0].trigger_cycle != frame_end ||
        machine->dma_channels[0].trigger_dispute != GBL_DMA_DISPUTE_NONE) {
        return false;
    }
    for (pixel = 0u; pixel < GBL_PPU_ROW_COVERAGE_BYTES; ++pixel) {
        if (machine->ppu.row_coverage[pixel] != UINT8_MAX) return false;
    }
    for (pixel = 0u; pixel < GBL_PPU_VIDEO_BYTES / 2u; ++pixel) {
        const uint16_t expected = gbl_phase10_pattern_pixel(pixel, variant);
        if (machine->video_frame[pixel * 2u] !=
                (uint8_t)(expected & UINT16_C(0x00FF)) ||
            machine->video_frame[pixel * 2u + 1u] !=
                (uint8_t)(expected >> 8u)) {
            return false;
        }
    }
    return true;
}

static bool gbl_phase10_same_machine(const gbl_machine *left,
                                     const gbl_machine *right)
{
    return gbl_test_canonical_equal(left, right) &&
           gbl_test_canonical_digest(left) ==
               gbl_test_canonical_digest(right);
}

static bool gbl_phase10_prepare_idempotency(gbl_machine *whole,
                                            gbl_machine *split,
                                            const char **failure)
{
    gbl_machine_time_preview *first =
        malloc(sizeof *first);
    gbl_machine_time_preview *second =
        malloc(sizeof *second);
    gbl_machine *snapshot = malloc(sizeof *snapshot);
    const uint64_t target = GBL_PPU_HBLANK_CYCLES;
    bool okay = false;

    if (first == NULL || second == NULL || snapshot == NULL) {
        *failure = "phase10-preview-allocation-failed";
        goto done;
    }
    memcpy(snapshot, whole, sizeof *snapshot);
    if (gbl_machine_prepare_time(whole, target, first) != GBL_TIME_STATUS_OK ||
        gbl_machine_prepare_time(whole, target, second) != GBL_TIME_STATUS_OK ||
        !gbl_test_time_preview_equal(first, second) ||
        memcmp(whole, snapshot, sizeof *whole) != 0) {
        *failure = "phase10-repeated-prepare-mutated-or-differed";
        goto done;
    }
    gbl_machine_commit_time(whole, first);
    if (!gbl_phase10_advance(split, target, failure) ||
        !gbl_phase10_same_machine(whole, split)) {
        *failure = "phase10-prepared-commit-differs-from-direct-advance";
        goto done;
    }
    memcpy(snapshot, whole, sizeof *snapshot);
    if (gbl_machine_prepare_time(whole, target + 1u, first) !=
            GBL_TIME_STATUS_OK ||
        gbl_machine_prepare_time(whole, target + 1u, second) !=
            GBL_TIME_STATUS_OK ||
        !gbl_test_time_preview_equal(first, second) ||
        memcmp(whole, snapshot, sizeof *whole) != 0) {
        *failure = "phase10-post-read-prepare-not-idempotent";
        goto done;
    }
    okay = true;

done:
    free(first);
    free(second);
    free(snapshot);
    return okay;
}

static bool gbl_phase10_run_split(gbl_machine *machine,
                                  const char **failure)
{
    const uint64_t targets[] = {
        UINT64_C(1005), UINT64_C(1006), UINT64_C(1231), UINT64_C(1232),
        UINT64_C(1233), UINT64_C(8192), UINT64_C(65536),
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES
    };
    size_t index;

    for (index = 0u; index < sizeof targets / sizeof targets[0]; ++index) {
        if (targets[index] > machine->master_cycles &&
            !gbl_phase10_advance(machine, targets[index], failure)) {
            return false;
        }
    }
    return true;
}

static bool gbl_phase10_run_interleaved(gbl_machine *first,
                                        gbl_machine *second,
                                        const char **failure)
{
    const uint64_t targets[] = {
        UINT64_C(1005), UINT64_C(1006), UINT64_C(1231), UINT64_C(1232),
        UINT64_C(1233), UINT64_C(8192), UINT64_C(65536),
        UINT64_C(120000),
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES
    };
    size_t index;

    for (index = 0u; index < sizeof targets / sizeof targets[0]; ++index) {
        if (!gbl_phase10_advance(first, targets[index], failure) ||
            !gbl_phase10_advance(second, targets[index], failure)) {
            return false;
        }
    }
    return true;
}

static bool gbl_phase10_preview_controls(const char **failure)
{
    gbl_machine *first_machine = NULL;
    gbl_machine *second_machine = NULL;
    gbl_machine_time_preview *first = NULL;
    gbl_machine_time_preview *mutant = NULL;
    bool okay = false;

    first_machine = gbl_phase10_new_machine(false, failure);
    second_machine = gbl_phase10_new_machine(false, failure);
    first = malloc(sizeof *first);
    mutant = malloc(sizeof *mutant);
    if (first_machine == NULL || second_machine == NULL || first == NULL ||
        mutant == NULL) {
        *failure = "phase10-preview-control-allocation-failed";
        goto done;
    }
    if (gbl_machine_prepare_time(first_machine,
                                GBL_PPU_HBLANK_CYCLES,
                                 first) != GBL_TIME_STATUS_OK) {
        *failure = "phase10-preview-control-prepare-failed";
        goto done;
    }
    {
        gbl_machine_time_preview *second = malloc(sizeof *second);
        if (second == NULL) {
            *failure = "phase10-preview-control-allocation-failed";
            goto done;
        }
        if (gbl_machine_prepare_time(second_machine,
                                     GBL_PPU_HBLANK_CYCLES, second) !=
                GBL_TIME_STATUS_OK ||
            !gbl_test_time_preview_equal(first, second)) {
            free(second);
            *failure = "phase10-preview-clean-twins-differ";
            goto done;
        }
        free(second);
    }
#define GBL_PHASE10_PREVIEW_MUTATION(label, statement)                       \
    do {                                                                      \
        *mutant = *first;                                                     \
        statement;                                                            \
        if (gbl_test_time_preview_equal(first, mutant)) {                      \
            *failure = (label);                                               \
            goto done;                                                        \
        }                                                                     \
    } while (0)
    GBL_PHASE10_PREVIEW_MUTATION("phase10-preview-frame-epoch-accepted",
                                 mutant->ppu.frame_epoch++);
    GBL_PHASE10_PREVIEW_MUTATION("phase10-preview-coverage-accepted",
                                 mutant->ppu.row_coverage[0] ^= UINT8_C(1));
    GBL_PHASE10_PREVIEW_MUTATION("phase10-preview-status-accepted",
                                 mutant->ppu.capture_state =
                                     GBL_VIDEO_CAPTURE_INVALID);
    GBL_PHASE10_PREVIEW_MUTATION("phase10-preview-reason-accepted",
                                 mutant->ppu.capture_reason =
                                     GBL_VIDEO_INVALID_REASON_FORCED_BLANK);
    if (first->ppu_row_capture_count == 0u) {
        *failure = "phase10-preview-row-capture-missing";
        goto done;
    }
    GBL_PHASE10_PREVIEW_MUTATION(
        "phase10-preview-capture-epoch-accepted",
        mutant->ppu_row_captures[0].frame_epoch++);
    GBL_PHASE10_PREVIEW_MUTATION(
        "phase10-preview-bg-register-accepted",
        mutant->ppu.bg_register_bytes[0] ^= UINT8_C(1));
    GBL_PHASE10_PREVIEW_MUTATION(
        "phase10-preview-captured-bg-register-accepted",
        mutant->ppu_row_captures[0].bg_register_bytes[0] ^= UINT8_C(1));
    GBL_PHASE10_PREVIEW_MUTATION(
        "phase10-preview-pixel-digest-accepted",
        mutant->ppu_row_captures[0].pixel_digest ^= UINT64_C(1));
#undef GBL_PHASE10_PREVIEW_MUTATION
    {
        const gbl_bus_result first_read = gbl_bus_read(
            first_machine, UINT32_C(0x04000004), 2u,
            GBL_BUS_CLASS_NONSEQUENTIAL);
        const gbl_bus_result second_read = gbl_bus_read(
            second_machine, UINT32_C(0x04000004), 2u,
            GBL_BUS_CLASS_NONSEQUENTIAL);
        if (first_read.status != GBL_BUS_STATUS_OK ||
            second_read.status != GBL_BUS_STATUS_OK ||
            first_read.value != second_read.value ||
            !gbl_phase10_same_machine(first_machine, second_machine)) {
            *failure = "phase10-read-only-clean-twins-differ";
            goto done;
        }
    }
    okay = true;

done:
    gbl_phase10_free_machine(&first_machine);
    gbl_phase10_free_machine(&second_machine);
    free(first);
    free(mutant);
    return okay;
}

static bool gbl_phase10_refusal_controls(const char **failure)
{
    static const gbl_time_operation capture_operations[2] = {
        { GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x060001E0),
          UINT32_C(0x4321), UINT64_C(1) }
    };
    static const gbl_time_operation dma_operations[2] = {
        { GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x04000004),
          UINT32_C(0x0008), UINT64_C(1) }
    };
    const uint64_t frame_end =
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES;
    const uint64_t targets[] = { UINT64_C(1231), frame_end - UINT64_C(1) };
    const gbl_time_operation *operations[] = {
        capture_operations, dma_operations
    };
    const char *failure_labels[] = {
        "phase10-multi-operation-capture-refusal-leaked-state",
        "phase10-dma-trigger-refusal-leaked-state"
    };
    size_t test_index;
    bool okay = true;

    for (test_index = 0u; test_index < 2u; ++test_index) {
        gbl_machine *machine = gbl_phase10_new_machine(false, failure);
        gbl_machine *snapshot = malloc(sizeof *snapshot);
        gbl_machine_time_preview *preview = malloc(sizeof *preview);
        gbl_machine_time_preview *preview_before =
            malloc(sizeof *preview_before);
        gbl_time_status status = GBL_TIME_STATUS_OK;

        if (machine == NULL || snapshot == NULL || preview == NULL ||
            preview_before == NULL) {
            *failure = "phase10-refusal-control-allocation-failed";
            okay = false;
        } else if (!gbl_phase10_advance(machine, targets[test_index],
                                       failure)) {
            okay = false;
        } else {
            if (test_index == 1u &&
                (!machine->dma_channels[0].armed ||
                 machine->dma_channels[0].control != UINT16_C(0x9000))) {
                *failure = "phase10-vblank-dma-not-armed-before-refusal";
                okay = false;
            }
            machine->next_event_sequence = UINT64_MAX;
            memcpy(snapshot, machine, sizeof *snapshot);
            memset(preview, 0xA5, sizeof *preview);
            memcpy(preview_before, preview, sizeof *preview_before);
            status = gbl_machine_prepare_operations(
                machine, operations[test_index], 2u, preview);
            if (okay &&
                (status != GBL_TIME_STATUS_SEQUENCE_EXHAUSTED ||
                 memcmp(machine, snapshot, sizeof *machine) != 0 ||
                 memcmp(preview, preview_before, sizeof *preview) != 0 ||
                 !gbl_test_canonical_equal(machine, snapshot) ||
                 !gbl_test_same_instance_identity_equal(machine, snapshot))) {
                *failure = failure_labels[test_index];
                okay = false;
            }
        }
        gbl_phase10_free_machine(&machine);
        free(snapshot);
        free(preview);
        free(preview_before);
        if (!okay) return false;
    }
    return true;
}

static gbl_event *gbl_phase10_ppu_event(gbl_machine *machine)
{
    size_t index;

    for (index = 0u; index < machine->event_count; ++index) {
        if (machine->events[index].kind ==
            (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY) {
            return &machine->events[index];
        }
    }
    return NULL;
}

static bool gbl_phase10_mutate_ppu_event(gbl_machine *machine,
                                         unsigned int field)
{
    gbl_event *event = gbl_phase10_ppu_event(machine);

    if (event == NULL) return false;
    switch (field) {
    case 0u: event->cycle++; break;
    case 1u: event->sequence++; break;
    case 2u: event->generation++; break;
    case 3u: event->priority++; break;
    case 4u: event->kind ^= UINT8_C(1); break;
    case 5u: event->timer_index ^= UINT8_C(1); break;
    case 6u: event->dma_channel ^= UINT8_C(1); break;
    case 7u: event->live = !event->live; break;
    case 8u: event->input_index ^= UINT16_C(1); break;
    default: return false;
    }
    return true;
}

static int gbl_test_phase10_main(void)
{
    gbl_machine *whole = NULL;
    gbl_machine *split = NULL;
    gbl_machine *reset = NULL;
    gbl_machine *isolated_variant = NULL;
    gbl_machine *alternating_a = NULL;
    gbl_machine *alternating_b = NULL;
    const char *failure = "phase10-replay-setup-failed";
    const uint64_t frame_end =
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES;
    bool okay = false;

    whole = gbl_phase10_new_machine(false, &failure);
    split = gbl_phase10_new_machine(false, &failure);
    reset = gbl_phase10_new_machine(false, &failure);
    isolated_variant = gbl_phase10_new_machine(true, &failure);
    alternating_a = gbl_phase10_new_machine(false, &failure);
    alternating_b = gbl_phase10_new_machine(true, &failure);
    if (whole == NULL || split == NULL || reset == NULL ||
        isolated_variant == NULL || alternating_a == NULL ||
        alternating_b == NULL) {
        goto done;
    }
    if (!gbl_phase10_prepare_idempotency(whole, split, &failure) ||
        !gbl_phase10_advance(whole, frame_end, &failure) ||
        !gbl_phase10_run_split(split, &failure) ||
        !gbl_phase10_run_split(reset, &failure) ||
        !gbl_phase10_frame_semantics(whole, false) ||
        !gbl_phase10_frame_semantics(split, false) ||
        !gbl_phase10_same_machine(whole, split)) {
        failure = "phase10-fresh-or-split-replay-differs";
        goto done;
    }

    gbl_machine_reset(reset);
    if (!gbl_phase10_seed(reset, false, &failure) ||
        !gbl_phase10_advance(reset, frame_end, &failure) ||
        !gbl_phase10_frame_semantics(reset, false) ||
        !gbl_phase10_same_machine(whole, reset)) {
        failure = "phase10-reset-replay-differs-from-fresh";
        goto done;
    }
    if (!gbl_phase10_advance(isolated_variant, frame_end, &failure) ||
        !gbl_phase10_run_interleaved(alternating_a, alternating_b,
                                     &failure) ||
        !gbl_phase10_frame_semantics(isolated_variant, true) ||
        !gbl_phase10_frame_semantics(alternating_a, false) ||
        !gbl_phase10_frame_semantics(alternating_b, true) ||
        !gbl_phase10_same_machine(whole, alternating_a) ||
        !gbl_phase10_same_machine(isolated_variant, alternating_b) ||
        memcmp(whole->video_frame, isolated_variant->video_frame,
               GBL_PPU_VIDEO_BYTES) == 0) {
        failure = "phase10-interleaved-instance-or-pattern-isolation-failed";
        goto done;
    }
    if (!gbl_phase10_preview_controls(&failure)) goto done;
    if (!gbl_phase10_refusal_controls(&failure)) goto done;
    okay = true;

done:
    gbl_phase10_free_machine(&whole);
    gbl_phase10_free_machine(&split);
    gbl_phase10_free_machine(&reset);
    gbl_phase10_free_machine(&isolated_variant);
    gbl_phase10_free_machine(&alternating_a);
    gbl_phase10_free_machine(&alternating_b);
    if (!okay) {
        fprintf(stderr, "FAIL determinism.phase10: %s\n", failure);
        return 1;
    }
    puts("PASS determinism.phase10: fresh, reset and interleaved synthetic mode-3 replays match complete PPU state, trigger timing and all raw pixels");
    return 0;
}

static int gbl_test_phase10_controls_main(void)
{
    gbl_machine *clean = NULL;
    gbl_machine *twin = NULL;
    gbl_machine *mutant = NULL;
    const char *failure = "phase10-control-setup-failed";
    const uint64_t frame_end =
        (uint64_t)GBL_PPU_VISIBLE_LINES * GBL_PPU_LINE_CYCLES;
    gbl_trace_record clean_trace = { 0 };
    gbl_trace_record mutant_trace;
    bool okay = false;

    clean = gbl_phase10_new_machine(false, &failure);
    twin = gbl_phase10_new_machine(false, &failure);
    mutant = malloc(sizeof *mutant);
    if (clean == NULL || twin == NULL || mutant == NULL) {
        failure = "phase10-control-allocation-failed";
        goto done;
    }
    if (!gbl_phase10_advance(clean, frame_end, &failure) ||
        !gbl_phase10_advance(twin, frame_end, &failure) ||
        !gbl_phase10_frame_semantics(clean, false) ||
        !gbl_phase10_frame_semantics(twin, false) ||
        !gbl_phase10_same_machine(clean, twin)) {
        failure = "phase10-control-clean-twin-mismatch";
        goto done;
    }
#define GBL_PHASE10_MACHINE_MUTATION(label, statement)                       \
    do {                                                                      \
        memcpy(mutant, clean, sizeof *mutant);                                \
        statement;                                                            \
        if (gbl_test_canonical_equal(clean, mutant) ||                        \
            gbl_test_canonical_digest(clean) ==                              \
                gbl_test_canonical_digest(mutant)) {                          \
            failure = (label);                                               \
            goto done;                                                        \
        }                                                                     \
    } while (0)
#define GBL_PHASE10_PPU_MUTATION(label, statement)                            \
    do {                                                                      \
        memcpy(mutant, clean, sizeof *mutant);                                \
        statement;                                                            \
        if (gbl_test_ppu_video_equal(clean, mutant) ||                        \
            gbl_test_canonical_equal(clean, mutant) ||                        \
            gbl_test_canonical_digest(clean) ==                              \
                gbl_test_canonical_digest(mutant)) {                          \
            failure = (label);                                               \
            goto done;                                                        \
        }                                                                     \
    } while (0)
    GBL_PHASE10_PPU_MUTATION("phase10-line-origin-control-accepted",
                             mutant->ppu.line_origin++);
    GBL_PHASE10_PPU_MUTATION("phase10-generation-control-accepted",
                             mutant->ppu.generation++);
    GBL_PHASE10_PPU_MUTATION("phase10-frame-epoch-control-accepted",
                             mutant->ppu.frame_epoch++);
    GBL_PHASE10_PPU_MUTATION("phase10-line-control-accepted",
                             mutant->ppu.line++);
    GBL_PHASE10_PPU_MUTATION("phase10-scheduled-line-control-accepted",
                             mutant->ppu.scheduled_line++);
    GBL_PHASE10_PPU_MUTATION("phase10-dispcnt-control-accepted",
                             mutant->ppu.dispcnt ^= UINT16_C(1));
    GBL_PHASE10_PPU_MUTATION("phase10-dispstat-control-accepted",
                             mutant->ppu.dispstat ^= UINT16_C(1));
    GBL_PHASE10_PPU_MUTATION("phase10-compare-control-accepted",
                             mutant->ppu.compare++);
    GBL_PHASE10_PPU_MUTATION("phase10-captured-rows-control-accepted",
                             mutant->ppu.captured_rows--);
    GBL_PHASE10_PPU_MUTATION("phase10-coverage-control-accepted",
                             mutant->ppu.row_coverage[0] ^= UINT8_C(1));
    GBL_PHASE10_PPU_MUTATION("phase10-capture-state-control-accepted",
                             mutant->ppu.capture_state =
                                 GBL_VIDEO_CAPTURE_INVALID);
    GBL_PHASE10_PPU_MUTATION("phase10-capture-reason-control-accepted",
                             mutant->ppu.capture_reason =
                                 GBL_VIDEO_INVALID_REASON_FORCED_BLANK);
    GBL_PHASE10_PPU_MUTATION("phase10-next-boundary-control-accepted",
                             mutant->ppu.next_boundary_hblank =
                                 !mutant->ppu.next_boundary_hblank);
    GBL_PHASE10_PPU_MUTATION("phase10-frame-started-control-accepted",
                             mutant->ppu.frame_started =
                                 !mutant->ppu.frame_started);
    GBL_PHASE10_PPU_MUTATION("phase10-first-raw-byte-control-accepted",
                             mutant->video_frame[0] ^= UINT8_C(1));
    GBL_PHASE10_PPU_MUTATION(
        "phase10-last-raw-byte-control-accepted",
        mutant->video_frame[GBL_PPU_VIDEO_BYTES - 1u] ^= UINT8_C(1));
#define GBL_PHASE10_EVENT_MUTATION(label, field)                              \
    GBL_PHASE10_MACHINE_MUTATION(                                            \
        (label),                                                             \
        (void)gbl_phase10_mutate_ppu_event(mutant, (field)))
    GBL_PHASE10_EVENT_MUTATION("phase10-event-cycle-control-accepted", 0u);
    GBL_PHASE10_EVENT_MUTATION("phase10-event-sequence-control-accepted", 1u);
    GBL_PHASE10_EVENT_MUTATION("phase10-event-generation-control-accepted", 2u);
    GBL_PHASE10_EVENT_MUTATION("phase10-event-priority-control-accepted", 3u);
    GBL_PHASE10_EVENT_MUTATION("phase10-event-kind-control-accepted", 4u);
    GBL_PHASE10_EVENT_MUTATION("phase10-event-timer-index-control-accepted", 5u);
    GBL_PHASE10_EVENT_MUTATION("phase10-event-dma-channel-control-accepted", 6u);
    GBL_PHASE10_EVENT_MUTATION("phase10-event-live-control-accepted", 7u);
    GBL_PHASE10_EVENT_MUTATION("phase10-event-input-index-control-accepted", 8u);
#undef GBL_PHASE10_EVENT_MUTATION
    GBL_PHASE10_MACHINE_MUTATION("phase10-trigger-origin-control-accepted",
                                 mutant->dma_channels[0].trigger_origin =
                                     GBL_DMA_TRIGGER_ORIGIN_HBLANK);
    GBL_PHASE10_MACHINE_MUTATION("phase10-trigger-cycle-control-accepted",
                                 mutant->dma_channels[0].trigger_cycle++);
    GBL_PHASE10_MACHINE_MUTATION("phase10-trigger-dispute-control-accepted",
                                 mutant->dma_channels[0].trigger_dispute =
                                     GBL_DMA_DISPUTE_HBLANK_FLAG_EDGE);
    GBL_PHASE10_MACHINE_MUTATION("phase10-trigger-generation-control-accepted",
                                 mutant->dma_channels[0].generation++);
    GBL_PHASE10_MACHINE_MUTATION("phase10-latch-state-control-accepted",
                                 mutant->dma_channels[0].read_latch ^=
                                     UINT32_C(1));
#undef GBL_PHASE10_PPU_MUTATION
#undef GBL_PHASE10_MACHINE_MUTATION
    mutant_trace = clean_trace;
    mutant_trace.dma_used_channel_latch = true;
    if (gbl_test_trace_equal(&clean_trace, &mutant_trace, 1u)) {
        failure = "phase10-latch-marker-control-accepted";
        goto done;
    }
    if (!gbl_phase10_preview_controls(&failure)) goto done;
    if (!gbl_phase10_refusal_controls(&failure)) goto done;
    okay = true;

done:
    gbl_phase10_free_machine(&clean);
    gbl_phase10_free_machine(&twin);
    free(mutant);
    if (!okay) {
        fprintf(stderr, "FAIL determinism.phase10.controls: %s\n", failure);
        return 1;
    }
    puts("PASS determinism.phase10.controls: named PPU state, event, validity, trigger, latch and raw-byte mutations are detected independently");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "phase10") == 0) {
        return gbl_test_phase10_main();
    }
    if (argc == 2 && strcmp(argv[1], "phase10-controls") == 0) {
        return gbl_test_phase10_controls_main();
    }
    if (argc == 2 && strcmp(argv[1], "phase9") == 0) {
        return gbl_test_phase9_main();
    }
    if (argc == 2 && strcmp(argv[1], "phase9-controls") == 0) {
        return gbl_test_phase9_controls_main();
    }
    if (argc == 2 && strcmp(argv[1], "phase8") == 0) {
        return gbl_test_phase8_main();
    }
    if (argc == 2 && strcmp(argv[1], "phase8-controls") == 0) {
        return gbl_test_phase8_controls_main();
    }
    if (argc == 4 && strcmp(argv[1], "time-authority") == 0) {
        return gbl_test_time_authority_main(argv[2], argv[3]);
    }
    if (argc == 2 && strcmp(argv[1], "interleave") == 0) {
        return gbl_test_interleave_main();
    }
    if (argc == 2 && strcmp(argv[1], "interleave-controls") == 0) {
        return gbl_test_interleave_controls_main();
    }
    if (argc == 2 && strcmp(argv[1], "matrix-controls") == 0) {
        return gbl_test_matrix_controls_main();
    }
    if (argc == 4 && strcmp(argv[1], "matrix-variants") == 0) {
        return gbl_test_matrix_variants_main(argv[2], argv[3]);
    }
    if (argc == 5 && strcmp(argv[1], "matrix-compare") == 0) {
        return gbl_test_matrix_compare_main(argv[2], argv[3], argv[4]);
    }
    if (argc == 5 && strcmp(argv[1], "matrix") == 0) {
        return gbl_test_matrix_main(argv[2], argv[3], argv[4]);
    }
    if (argc != 1) {
        fputs("usage: determinism_test [time-authority <full|final> <output>|interleave|interleave-controls|matrix-controls|matrix-variants|matrix-compare|matrix]\n",
              stderr);
        return 2;
    }
    return gbl_test_repeat_main();
}
