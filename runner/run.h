#ifndef GAMEBLOKE_RUNNER_RUN_H
#define GAMEBLOKE_RUNNER_RUN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "fixture_index.h"
#include "gamebloke/gamebloke.h"
#include "sha256.h"

#define GBL_RUNNER_MAX_ROM_BYTES ((size_t)UINT32_C(0x02000000))
#define GBL_RUNNER_MAX_TRACE_RECORDS ((size_t)65536u)
#define GBL_RUNNER_FIXTURE_ID_CAPACITY 128u
#define GBL_RUNNER_FRAME_CYCLES UINT64_C(280896)

typedef void *(*gbl_runner_allocate_fn)(void *context, size_t size);
typedef void (*gbl_runner_release_fn)(void *context, void *pointer,
                                      size_t size);
typedef bool (*gbl_runner_cancelled_fn)(void *context);

typedef struct gbl_runner_allocator {
    void *context;
    gbl_runner_allocate_fn allocate;
    gbl_runner_release_fn release;
} gbl_runner_allocator;

typedef struct gbl_runner_options {
    const char *rom_path;
    const char *fixture_id;
    uint64_t cycle_budget;
    uint32_t trace_mask;
    size_t trace_capacity;
    const char *trace_output_path;
    gbl_runner_cancelled_fn cancelled;
    void *cancel_context;
    /* Both callbacks must be supplied together. Both NULL selects malloc/free. */
    gbl_runner_allocator allocator;
} gbl_runner_options;

typedef enum gbl_runner_status {
    GBL_RUNNER_PASS = 0,
    GBL_RUNNER_GUEST_FAIL,
    GBL_RUNNER_GUEST_STOP,
    GBL_RUNNER_HARNESS_FAIL,
    GBL_RUNNER_TIMEOUT,
    GBL_RUNNER_CANCELLED,
    GBL_RUNNER_MISSING,
    GBL_RUNNER_SKIPPED,
    GBL_RUNNER_REFUSED
} gbl_runner_status;

typedef struct gbl_runner_result {
    gbl_runner_status status;
    char reason[96];
    bool has_refusal;
    char refusal_code[64];
    char refusal_postcondition[96];

    bool has_fixture_id;
    char fixture_id[GBL_RUNNER_FIXTURE_ID_CAPACITY];
    const gbl_fixture_metadata *metadata;
    bool ran;
    bool has_rom_sha256;
    char rom_sha256[GBL_SHA256_HEX_CAPACITY];
    bool has_expected_sha256;
    char expected_sha256[GBL_SHA256_HEX_CAPACITY];

    gbl_mailbox_result semantic;
    uint64_t cycles;
    uint64_t instructions;
    uint64_t budget_cycles;
    char termination[64];

    uint32_t trace_mask;
    size_t trace_capacity;
    size_t trace_count;
    bool trace_complete;
    bool trace_emitted;
    gbl_trace_record *trace_records;

    gbl_runner_allocator allocator;
} gbl_runner_result;

/* Always returns a result document state for a non-NULL result pointer.
 * A cycle/frame budget is a hard whole-unit bound. The callback is sampled
 * between committed canonical CPU units, and the guest mailbox is peeked at
 * the same boundary. The caller owns result and must dispose it after output. */
gbl_runner_status gbl_runner_execute(const gbl_runner_options *options,
                                     gbl_runner_result *result);

/* Internal host-test seam for the production CPU-stop classification path.
 * This is not a stable embedding ABI. */
void gbl_runner_classify_guest_stop(gbl_runner_result *result,
                                    const gbl_machine *machine,
                                    gbl_run_reason reason);

/* Writes one gamebloke.result/1 JSON object followed by one newline. */
bool gbl_runner_write_json(FILE *output, const gbl_runner_result *result);

/* Writes selected records as deterministic JSON Lines to a newly created
 * explicit path. Existing files are never truncated. */
bool gbl_runner_write_trace(const char *path,
                            const gbl_runner_result *result);

void gbl_runner_result_dispose(gbl_runner_result *result);

#endif /* GAMEBLOKE_RUNNER_RUN_H */
