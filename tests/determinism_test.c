#define _POSIX_C_SOURCE 200809L

#include "run.h"

#include <errno.h>
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

int main(void)
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
