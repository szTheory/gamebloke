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
    GBL_INTERLEAVE_TRACE_LINE_CAPACITY = 256u,
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
    uint64_t cycles;
    uint64_t instructions;
    uint64_t wait_cycles;
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
    if (UINT64_MAX - instance->cycles < result.cycles ||
        UINT64_MAX - instance->instructions < result.instructions ||
        UINT64_MAX - instance->wait_cycles < result.wait_cycles) {
        *failure = "interleave-progress-overflow";
        return false;
    }
    instance->cycles += result.cycles;
    instance->instructions += result.instructions;
    instance->wait_cycles += result.wait_cycles;
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
            "\"exception_kind\":%u}\n",
            record->sequence, record->guest_cycle,
            (unsigned int)record->category, record->instruction_address,
            record->opcode, record->address, record->value,
            (unsigned int)record->width, (unsigned int)record->access_class,
            (unsigned int)record->bus_outcome,
            (unsigned int)record->exception_kind);

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
    size_t index;

    for (index = 0u; index < count; ++index) {
        if (first[index].sequence != second[index].sequence ||
            first[index].guest_cycle != second[index].guest_cycle ||
            first[index].category != second[index].category ||
            first[index].instruction_address !=
                second[index].instruction_address ||
            first[index].opcode != second[index].opcode ||
            first[index].address != second[index].address ||
            first[index].value != second[index].value ||
            first[index].width != second[index].width ||
            first[index].access_class != second[index].access_class ||
            first[index].bus_outcome != second[index].bus_outcome ||
            first[index].exception_kind != second[index].exception_kind) {
            return false;
        }
    }
    return true;
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
        expected->machine.master_cycles != actual->machine.master_cycles ||
        expected->machine.cpu.retired_instructions !=
            actual->machine.cpu.retired_instructions) {
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
        instance->machine.master_cycles != instance->cycles ||
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
    instance->cycles = 0u;
    instance->instructions = 0u;
    instance->wait_cycles = 0u;
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
    machine->interrupt_flags |= UINT16_C(1) << 3u;
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
        isolated_thumb.machine.input_event_cursor != 2u) {
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
    gbl_trace_record expected = { 0 };
    gbl_trace_record actual;

    expected.category = GBL_TRACE_DMA;
    expected.bus_owner = GBL_MACHINE_BUS_OWNER_DMA;
    expected.dma_dispute = GBL_DMA_DISPUTE_LATCH;
    actual = expected;
    actual.dma_dispute = GBL_DMA_DISPUTE_NONE;
    if (gbl_interleave_trace_fields_equal(&expected, &actual, 1u)) {
        fputs("FAIL determinism.phase9: dma-dispute-trace-field-ignored\n",
              stderr);
        return 1;
    }
    puts("PASS determinism.phase9: DMA trace comparison observes dispute metadata");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "phase9") == 0) {
        return gbl_test_phase9_main();
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
