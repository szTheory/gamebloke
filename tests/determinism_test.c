#define _POSIX_C_SOURCE 200809L

#include "run.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static void gbl_interleave_dispose(gbl_interleave_instance *instance)
{
    if (instance == NULL) return;
    if (instance->initialized) gbl_machine_dispose(&instance->machine);
    free(instance->records);
    free(instance->serialized_trace);
    memset(instance, 0, sizeof *instance);
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
        UINT64_MAX - instance->instructions < result.instructions) {
        *failure = "interleave-progress-overflow";
        return false;
    }
    instance->cycles += result.cycles;
    instance->instructions += result.instructions;
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

    if (instance == NULL || instance->trace.count == 0u ||
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

static bool gbl_interleave_cpu_equal(const gbl_cpu_state *first,
                                    const gbl_cpu_state *second)
{
    size_t index;

    if (memcmp(first->r0_7, second->r0_7, sizeof first->r0_7) != 0 ||
        memcmp(first->usr_r8_14, second->usr_r8_14,
               sizeof first->usr_r8_14) != 0 ||
        memcmp(first->fiq_r8_14, second->fiq_r8_14,
               sizeof first->fiq_r8_14) != 0 ||
        memcmp(first->irq_r13_14, second->irq_r13_14,
               sizeof first->irq_r13_14) != 0 ||
        memcmp(first->svc_r13_14, second->svc_r13_14,
               sizeof first->svc_r13_14) != 0 ||
        memcmp(first->abt_r13_14, second->abt_r13_14,
               sizeof first->abt_r13_14) != 0 ||
        memcmp(first->und_r13_14, second->und_r13_14,
               sizeof first->und_r13_14) != 0 ||
        memcmp(first->spsr, second->spsr, sizeof first->spsr) != 0 ||
        first->cpsr != second->cpsr ||
        first->execution_address != second->execution_address ||
        first->next_fetch_sequential != second->next_fetch_sequential ||
        first->retired_instructions != second->retired_instructions ||
        first->stopped != second->stopped ||
        first->stop_reason != second->stop_reason ||
        first->last_exception.kind != second->last_exception.kind ||
        first->last_exception.current_address !=
            second->last_exception.current_address ||
        first->last_exception.next_address !=
            second->last_exception.next_address ||
        first->last_exception.fault_address !=
            second->last_exception.fault_address ||
        first->last_exception.thumb_state !=
            second->last_exception.thumb_state) {
        return false;
    }
    for (index = 0u; index < 2u; ++index) {
        if (first->pipeline[index].address != second->pipeline[index].address ||
            first->pipeline[index].opcode != second->pipeline[index].opcode ||
            first->pipeline[index].width != second->pipeline[index].width ||
            first->pipeline[index].valid != second->pipeline[index].valid) {
            return false;
        }
    }
    return true;
}

static bool gbl_interleave_machine_equal(const gbl_machine *first,
                                         const gbl_machine *second)
{
    return first->initialized == second->initialized &&
           first->master_cycles == second->master_cycles &&
           first->open_bus == second->open_bus &&
           first->dispcnt == second->dispcnt &&
           gbl_interleave_cpu_equal(&first->cpu, &second->cpu) &&
           first->header_diagnostics.loaded ==
               second->header_diagnostics.loaded &&
           first->header_diagnostics.canonical_logo ==
               second->header_diagnostics.canonical_logo &&
           first->header_diagnostics.logo_reason ==
               second->header_diagnostics.logo_reason &&
           first->header_diagnostics.logo_window ==
               second->header_diagnostics.logo_window &&
           first->header_diagnostics.checksum ==
               second->header_diagnostics.checksum &&
           first->header_diagnostics.game_code ==
               second->header_diagnostics.game_code &&
           first->rom_length == second->rom_length &&
           ((first->rom == NULL) == (second->rom == NULL)) &&
           memcmp(first->ewram, second->ewram, sizeof first->ewram) == 0 &&
           memcmp(first->iwram, second->iwram, sizeof first->iwram) == 0 &&
           memcmp(first->pram, second->pram, sizeof first->pram) == 0 &&
           memcmp(first->vram, second->vram, sizeof first->vram) == 0 &&
           memcmp(first->oam, second->oam, sizeof first->oam) == 0 &&
           (first->rom_length == 0u ||
            (first->rom != NULL && second->rom != NULL &&
             memcmp(first->rom, second->rom, first->rom_length) == 0));
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
    gbl_interleave_instance isolated_arm = { 0 };
    gbl_interleave_instance isolated_thumb = { 0 };
    gbl_interleave_instance interleaved_arm = { 0 };
    gbl_interleave_instance interleaved_thumb = { 0 };
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
    passed = true;

done:
    gbl_interleave_dispose(&isolated_arm);
    gbl_interleave_dispose(&isolated_thumb);
    gbl_interleave_dispose(&interleaved_arm);
    gbl_interleave_dispose(&interleaved_thumb);
    free(arm_image.data);
    free(thumb_image.data);
    if (passed) {
        puts("PASS determinism.interleave: isolated and alternating ARM pass / ARM-Thumb fail executions match per-instance state and canonical traces");
        return 0;
    }
    fprintf(stderr, "FAIL determinism.interleave: %s\n", failure);
    return 1;
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

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "interleave") == 0) {
        return gbl_test_interleave_main();
    }
    if (argc != 1) {
        fputs("usage: determinism_test [interleave]\n", stderr);
        return 2;
    }
    return gbl_test_repeat_main();
}
