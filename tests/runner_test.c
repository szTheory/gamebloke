#define _POSIX_C_SOURCE 200809L

#include "run.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

enum {
    GBL_RUNNER_TEST_COUNT = 40u,
    GBL_RUNNER_TEST_PATH_CAPACITY = 1024u,
    GBL_RUNNER_TEST_WATCHDOG_TICKS = 300u
};

typedef struct gbl_test_allocator_state {
    size_t calls;
    size_t successes;
    size_t releases;
    size_t fail_at;
    gbl_machine *tracked_machine;
    gbl_machine *machine_snapshot;
    bool machine_snapshot_available;
} gbl_test_allocator_state;

typedef struct gbl_test_cancel_state {
    size_t calls;
    size_t cancel_at;
} gbl_test_cancel_state;

static unsigned int gbl_test_number;
static unsigned int gbl_test_failures;
static uint64_t gbl_pass_cycles;
static uint64_t gbl_pass_instructions;

static bool gbl_test_path(char *output, size_t capacity, const char *directory,
                          const char *name)
{
    const int written = snprintf(output, capacity, "%s/%s", directory,
                                 name);

    return written >= 0 && (size_t)written < capacity;
}

static void gbl_test_case(bool passed, const char *description)
{
    gbl_test_number++;
    printf("%s %u - %s\n", passed ? "ok" : "not ok", gbl_test_number,
           description);
    if (!passed) {
        gbl_test_failures++;
    }
}

static bool gbl_test_write_file(const char *path, size_t length,
                                unsigned char fill)
{
    FILE *file = fopen(path, "wb");
    bool okay = false;

    if (file == NULL) {
        return false;
    }
    if (length == 0u) {
        okay = true;
    } else if (length <= 4096u) {
        size_t index;

        okay = true;
        for (index = 0u; index < length; ++index) {
            if (fputc((int)fill, file) == EOF) {
                okay = false;
                break;
            }
        }
    } else {
        if (fseeko(file, (off_t)(length - 1u), SEEK_SET) == 0 &&
            fputc((int)fill, file) != EOF) {
            okay = true;
        }
    }
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay;
}

static bool gbl_test_guest_stop_json_matrix(void)
{
    static const struct {
        gbl_exception_kind kind;
        gbl_cpu_stop_reason stop_reason;
        const char *json_reason;
        bool direct_stop;
    } cases[] = {
        { GBL_EXCEPTION_RESET, GBL_CPU_STOP_RESET_NO_BIOS,
          "no-bios-reset", false },
        { GBL_EXCEPTION_UNDEFINED, GBL_CPU_STOP_UNDEFINED_NO_BIOS,
          "no-bios-undefined", false },
        { GBL_EXCEPTION_SWI, GBL_CPU_STOP_SWI_NO_BIOS, "no-bios-swi", false },
        { GBL_EXCEPTION_PREFETCH_ABORT,
          GBL_CPU_STOP_PREFETCH_ABORT_NO_BIOS, "no-bios-prefetch-abort",
          false },
        { GBL_EXCEPTION_DATA_ABORT, GBL_CPU_STOP_DATA_ABORT_NO_BIOS,
          "no-bios-data-abort", false },
        { GBL_EXCEPTION_IRQ, GBL_CPU_STOP_IRQ_NO_BIOS, "no-bios-irq", false },
        { GBL_EXCEPTION_FIQ, GBL_CPU_STOP_FIQ_NO_BIOS, "no-bios-fiq", false },
        { GBL_EXCEPTION_COPROCESSOR,
          GBL_CPU_STOP_COPROCESSOR_NO_BIOS, "no-bios-coprocessor", false },
        { GBL_EXCEPTION_NONE, GBL_CPU_STOP_STOP_REQUESTED,
          "stop-requested", true },
        { GBL_EXCEPTION_NONE, GBL_CPU_STOP_HALT_DEADLOCK,
          "halt-deadlock", true }
    };
    const gbl_cpu_exception_context context = {
        UINT32_C(0x08000100), UINT32_C(0x08000104), UINT32_C(0x08000200),
        false
    };
    size_t index;

    for (index = 0u; index < sizeof cases / sizeof cases[0]; ++index) {
        gbl_machine machine = { 0 };
        gbl_runner_result result = { 0 };
        char json[2048];
        size_t json_length;
        FILE *output;
        bool okay = gbl_machine_init(&machine, NULL) == GBL_BUS_STATUS_OK;

        if (okay) {
            if (cases[index].direct_stop) {
                machine.cpu.stopped = true;
                machine.cpu.stop_reason = cases[index].stop_reason;
            } else {
                okay = gbl_cpu_set_cpsr(&machine, UINT32_C(0x13)) ==
                           GBL_CPU_STATUS_OK &&
                       gbl_cpu_enter_exception(&machine, cases[index].kind,
                                               &context) ==
                           GBL_CPU_STATUS_OK;
            }
            okay = okay && machine.cpu.stopped &&
                   machine.cpu.stop_reason == cases[index].stop_reason;
        }
        output = okay ? tmpfile() : NULL;
        if (output == NULL) {
            okay = false;
        }
        if (okay) {
            gbl_runner_classify_guest_stop(&result, &machine,
                                           GBL_RUN_REASON_GUEST_STOP);
            okay = result.status == GBL_RUNNER_GUEST_STOP &&
                   strcmp(result.reason, cases[index].json_reason) == 0 &&
                   strcmp(result.termination, cases[index].json_reason) == 0 &&
                   gbl_runner_write_json(output, &result) &&
                   fflush(output) == 0 && fseek(output, 0L, SEEK_SET) == 0 &&
                   (json_length = fread(json, 1u, sizeof json - 1u, output)) >
                       0u;
        }
        if (okay) {
            json[json_length] = '\0';
            okay = strstr(json, "\"status\":\"GUEST_STOP\"") != NULL &&
                   strstr(json, cases[index].json_reason) != NULL;
        }
        if (output != NULL) {
            (void)fclose(output);
        }
        if (machine.initialized) {
            gbl_machine_dispose(&machine);
        }
        if (!okay) {
            return false;
        }
    }
    return true;
}

static void *gbl_test_allocate(void *context, size_t size);
static void gbl_test_release(void *context, void *pointer, size_t size);

static bool gbl_test_instruction_stop_json(const char *directory)
{
    static const struct {
        uint32_t opcode;
        bool thumb;
        const char *reason;
        uint64_t instructions;
    } cases[] = {
        { 0xF1A00000u, false, "unpredictable-instruction", 0u },
        { 0xE12FFF30u, false, "later-architecture-instruction", 0u },
        { 0x0000BE42u, true, "later-architecture-instruction", 0u },
        { 0xEFABCDEFu, false, "no-bios-swi", 1u },
        { 0x0000DFA5u, true, "no-bios-swi", 1u },
        { 0xE7F000F0u, false, "no-bios-undefined", 1u },
        { 0xEE000010u, false, "no-bios-coprocessor", 1u }
    };
    size_t index;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint8_t image[192] = { 0 };
        gbl_machine machine = { 0 };
        gbl_run_result run = { 0 };
        gbl_run_options options = { .cycle_budget = 100u };
        gbl_runner_result result = { 0 };
        gbl_test_allocator_state allocator_state = { 0 };
        const gbl_allocator allocator = {
            &allocator_state, gbl_test_allocate, gbl_test_release
        };
        char path[GBL_RUNNER_TEST_PATH_CAPACITY];
        char json[4096];
        char time[128];
        FILE *output;
        size_t count;
        bool okay;
        image[0] = (uint8_t)(cases[index].opcode & 0xFFu);
        image[1] = (uint8_t)((cases[index].opcode >> 8u) & 0xFFu);
        image[2] = (uint8_t)((cases[index].opcode >> 16u) & 0xFFu);
        image[3] = (uint8_t)(cases[index].opcode >> 24u);
        okay = gbl_machine_init(&machine, &allocator) == GBL_BUS_STATUS_OK &&
            gbl_load_rom(&machine, image, sizeof(image)).status == GBL_LOAD_STATUS_OK &&
            gbl_cpu_set_cpsr(&machine, cases[index].thumb ? 0x7Fu : 0x5Fu) ==
                GBL_CPU_STATUS_OK &&
            gbl_cpu_run(&machine, &options, &run) == GBL_RUN_STATUS_OK &&
            run.termination == GBL_RUN_STOPPED &&
            run.instructions == cases[index].instructions &&
            run.cycles == run.bus_cycles + run.internal_cycles +
                run.wait_cycles + run.dma_startup_cycles +
                run.dma_source_cycles + run.dma_destination_cycles;
        if (!okay) { gbl_machine_dispose(&machine); return false; }
        result.ran = true;
        result.trace_complete = true;
        result.cycles = run.cycles;
        result.instructions = run.instructions;
        result.budget_cycles = options.cycle_budget;
        result.has_rom_sha256 = true;
        okay = gbl_sha256_span(image, sizeof(image), result.rom_sha256);
        gbl_runner_classify_guest_stop(&result, &machine, run.reason);
        (void)snprintf(time, sizeof(time),
            "\"cycles\":\"%" PRIu64 "\",\"instructions\":\"%" PRIu64 "\"",
            run.cycles, run.instructions);
        (void)snprintf(json, sizeof(json), "instruction-stop-%u.json", (unsigned int)index);
        okay = okay && gbl_test_path(path, sizeof(path), directory, json);
        output = okay ? fopen(path, "w+b") : NULL;
        okay = output != NULL && gbl_runner_write_json(output, &result) &&
            fflush(output) == 0 && fseek(output, 0L, SEEK_SET) == 0;
        if (okay) {
            count = fread(json, 1u, sizeof(json) - 1u, output);
            json[count] = '\0';
            okay = result.status == GBL_RUNNER_GUEST_STOP &&
                strcmp(result.reason, cases[index].reason) == 0 &&
                strcmp(result.termination, cases[index].reason) == 0 &&
                strstr(json, "\"status\":\"GUEST_STOP\"") != NULL &&
                strstr(json, cases[index].reason) != NULL &&
                strstr(json, time) != NULL &&
                strstr(json, "\"machine\":\"agb\",\"boot\":\"direct-start\",\"bios_provider\":\"none\"") != NULL &&
                strstr(json, "profile version 6 direct-start") != NULL &&
                strstr(json, "full original diagnostics pending") != NULL &&
                strstr(json, "hardware accuracy or broad game compatibility") != NULL;
        }
        if (output != NULL) (void)fclose(output);
        gbl_machine_dispose(&machine);
        if (!okay) return false;
    }
    return true;
}

static void *gbl_test_allocate(void *context, size_t size)
{
    gbl_test_allocator_state *state = context;
    void *pointer;

    state->calls++;
    if (state->fail_at != 0u && state->calls == state->fail_at) {
        return NULL;
    }
    pointer = malloc(size);
    if (pointer != NULL) {
        state->successes++;
        if (size == sizeof(gbl_machine) && state->machine_snapshot != NULL) {
            state->tracked_machine = pointer;
        }
    }
    return pointer;
}

static void gbl_test_release(void *context, void *pointer, size_t size)
{
    gbl_test_allocator_state *state = context;

    (void)size;
    if (pointer != NULL) {
        if (pointer == state->tracked_machine &&
            state->machine_snapshot != NULL) {
            memcpy(state->machine_snapshot, pointer, sizeof(gbl_machine));
            state->machine_snapshot_available = true;
        }
        state->releases++;
        free(pointer);
    }
}

static bool gbl_test_cancel_at_boundary(void *context)
{
    gbl_test_cancel_state *state = context;

    state->calls++;
    return state->calls >= state->cancel_at;
}

static gbl_runner_status gbl_test_execute(
    const char *rom_path, const char *fixture_id, uint64_t budget,
    uint32_t trace_mask, size_t trace_capacity, const char *trace_path,
    gbl_runner_cancelled_fn cancelled, void *cancel_context,
    gbl_runner_allocator allocator, gbl_runner_result *result)
{
    gbl_runner_options options;

    memset(&options, 0, sizeof options);
    options.rom_path = rom_path;
    options.fixture_id = fixture_id;
    options.cycle_budget = budget;
    options.trace_mask = trace_mask;
    options.trace_capacity = trace_capacity;
    options.trace_output_path = trace_path;
    options.cancelled = cancelled;
    options.cancel_context = cancel_context;
    options.allocator = allocator;
    return gbl_runner_execute(&options, result);
}

static gbl_runner_status gbl_test_execute_timeline(
    const char *rom_path, const char *fixture_id, uint64_t budget,
    bool timeline_present, const gbl_input_event *events, size_t event_count,
    gbl_runner_cancelled_fn cancelled, void *cancel_context,
    gbl_runner_allocator allocator, gbl_runner_result *result)
{
    gbl_runner_options options;

    memset(&options, 0, sizeof options);
    options.rom_path = rom_path;
    options.fixture_id = fixture_id;
    options.cycle_budget = budget;
    options.input_timeline_present = timeline_present;
    options.input_events = events;
    options.input_event_count = event_count;
    options.cancelled = cancelled;
    options.cancel_context = cancel_context;
    options.allocator = allocator;
    return gbl_runner_execute(&options, result);
}

typedef struct gbl_test_timer0_observations {
    bool semantic_completed;
    bool request_observed;
    bool model_contract;
    bool component_sum;
    bool disabled_timeout;
    bool not_run_request_null;
} gbl_test_timer0_observations;

static void gbl_test_write_le32(uint8_t *bytes, size_t offset,
                                uint32_t value)
{
    bytes[offset] = (uint8_t)(value & UINT32_C(0xFF));
    bytes[offset + 1u] = (uint8_t)((value >> 8u) & UINT32_C(0xFF));
    bytes[offset + 2u] = (uint8_t)((value >> 16u) & UINT32_C(0xFF));
    bytes[offset + 3u] = (uint8_t)(value >> 24u);
}

static bool gbl_test_serialize_result(const gbl_runner_result *result,
                                      char *json, size_t capacity);
static bool gbl_test_read_file(const char *path, char *output,
                               size_t capacity);

static void gbl_test_emit_arm_word(uint8_t *image, size_t *offset,
                                   uint32_t opcode)
{
    gbl_test_write_le32(image, *offset, opcode);
    *offset += 4u;
}

static void gbl_test_emit_arm_ldr_literal(uint8_t *image, size_t *offset,
                                          unsigned int destination,
                                          size_t literal_offset)
{
    const size_t instruction_offset = *offset;
    const size_t pc = instruction_offset + 8u;
    const uint32_t displacement = (uint32_t)(literal_offset - pc);

    gbl_test_emit_arm_word(image, offset,
                           UINT32_C(0xE59F0000) |
                               ((uint32_t)destination << 12u) |
                               displacement);
}

static void gbl_test_emit_dma_start(uint8_t *image, size_t *offset,
                                    size_t literal_offset,
                                    size_t source_index,
                                    size_t destination_index)
{
    gbl_test_emit_arm_ldr_literal(image, offset, 0u,
                                  literal_offset + source_index * 4u);
    gbl_test_emit_arm_ldr_literal(image, offset, 1u,
                                  literal_offset + destination_index * 4u);
    gbl_test_emit_arm_ldr_literal(image, offset, 2u,
                                  literal_offset + 2u * 4u);
    gbl_test_emit_arm_word(image, offset, UINT32_C(0xE5800000) |
                                            (2u << 16u));
    gbl_test_emit_arm_ldr_literal(image, offset, 2u,
                                  literal_offset + 3u * 4u);
    gbl_test_emit_arm_word(image, offset, UINT32_C(0xE5800000) |
                                            (2u << 16u) | (1u << 12u));
    gbl_test_emit_arm_ldr_literal(image, offset, 0u,
                                  literal_offset + 4u * 4u);
    gbl_test_emit_arm_ldr_literal(image, offset, 2u,
                                  literal_offset + 5u * 4u);
    gbl_test_emit_arm_word(image, offset, UINT32_C(0xE1C000B0) |
                                            (2u << 16u));
    gbl_test_emit_arm_ldr_literal(image, offset, 0u,
                                  literal_offset + 6u * 4u);
    gbl_test_emit_arm_ldr_literal(image, offset, 2u,
                                  literal_offset + 7u * 4u);
    gbl_test_emit_arm_word(image, offset, UINT32_C(0xE1C000B0) |
                                            (2u << 16u));
}

static void gbl_test_emit_mailbox(uint8_t *image, size_t *offset,
                                  size_t literal_offset,
                                  size_t state_index)
{
    gbl_test_emit_arm_ldr_literal(image, offset, 0u,
                                  literal_offset + 9u * 4u);
    gbl_test_emit_arm_ldr_literal(image, offset, 1u,
                                  literal_offset + 10u * 4u);
    gbl_test_emit_arm_word(image, offset, UINT32_C(0xE5801000));
    gbl_test_emit_arm_ldr_literal(image, offset, 1u,
                                  literal_offset + 11u * 4u);
    gbl_test_emit_arm_word(image, offset, UINT32_C(0xE5801004));
    gbl_test_emit_arm_ldr_literal(image, offset, 1u,
                                  literal_offset + state_index * 4u);
    gbl_test_emit_arm_word(image, offset, UINT32_C(0xE5801008));
    gbl_test_emit_arm_ldr_literal(image, offset, 1u,
                                  literal_offset + 13u * 4u);
    gbl_test_emit_arm_word(image, offset, UINT32_C(0xE580100C));
    gbl_test_emit_arm_ldr_literal(image, offset, 1u,
                                  literal_offset + 14u * 4u);
    gbl_test_emit_arm_word(image, offset, UINT32_C(0xE5801010));
}

static bool gbl_test_write_dma_guest(const char *path)
{
    uint8_t image[0x220u] = { 0u };
    static const uint32_t literals[] = {
        UINT32_C(0x08000200), UINT32_C(0x03000020), UINT32_C(0x040000D4),
        UINT32_C(0x040000D8), UINT32_C(1), UINT32_C(0x040000DC),
        UINT32_C(0x8000), UINT32_C(0x040000DE), UINT32_C(0xBEEF),
        UINT32_C(0x03007FC0), UINT32_C(0x47424C4B), UINT32_C(1),
        UINT32_C(2), UINT32_C(9), UINT32_C(0x434F4D50), UINT32_C(3),
        UINT32_C(0), UINT32_C(0x03000022)
    };
    const size_t literal_offset = 0x100u;
    size_t offset = 0u;
    size_t index;
    FILE *file;
    bool okay;

    /* Program DMA3 twice: a ROM halfword seeds its latch, then source zero
     * exercises the explicitly disputed retained-channel fallback. */
    gbl_test_emit_dma_start(image, &offset, literal_offset, 0u, 1u);
    gbl_test_emit_dma_start(image, &offset, literal_offset, 16u, 17u);
    gbl_test_emit_arm_ldr_literal(image, &offset, 0u,
                                  literal_offset + 1u * 4u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xE1D010B0));
    gbl_test_emit_arm_ldr_literal(image, &offset, 2u,
                                  literal_offset + 8u * 4u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xE1510002));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0x1A00000B));
    gbl_test_emit_mailbox(image, &offset, literal_offset, 12u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xEA00000B));
    gbl_test_emit_mailbox(image, &offset, literal_offset, 15u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xEAFFFFFF));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xEAFFFFFE));

    for (index = 0u; index < sizeof literals / sizeof literals[0]; ++index) {
        gbl_test_write_le32(image, literal_offset + index * 4u,
                            literals[index]);
    }
    image[0x200u] = UINT8_C(0xEF);
    image[0x201u] = UINT8_C(0xBE);

    file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    okay = fwrite(image, 1u, sizeof image, file) == sizeof image;
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay;
}

static bool gbl_test_write_ppu_guest(const char *path)
{
    uint8_t image[0xC0u] = { 0u };
    const size_t literal_offset = 0x80u;
    const uint32_t literals[] = {
        UINT32_C(0x04000000), UINT32_C(0x0403),
        UINT32_C(0x04000130), UINT32_C(0x06000000),
        UINT32_C(0x001F), UINT32_C(0x03E0)
    };
    size_t offset = 0u;
    size_t index;
    FILE *file;
    bool okay;

    gbl_test_emit_arm_ldr_literal(image, &offset, 0u, literal_offset);
    gbl_test_emit_arm_ldr_literal(image, &offset, 1u, literal_offset + 4u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xE5801000));
    gbl_test_emit_arm_ldr_literal(image, &offset, 0u, literal_offset + 8u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xE5900000));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xE3100001));
    gbl_test_emit_arm_ldr_literal(image, &offset, 3u, literal_offset + 12u);
    gbl_test_emit_arm_ldr_literal(image, &offset, 1u, literal_offset + 16u);
    gbl_test_emit_arm_ldr_literal(image, &offset, 2u, literal_offset + 20u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0x15831000));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0x05832000));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xEAFFFFFE));
    for (index = 0u; index < sizeof literals / sizeof literals[0]; ++index) {
        gbl_test_write_le32(image, literal_offset + index * 4u,
                            literals[index]);
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    okay = fwrite(image, 1u, sizeof image, file) == sizeof image;
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay;
}

static bool gbl_test_write_ppu_transition_guest(const char *path)
{
    uint8_t image[0xC0u] = { 0u };
    const size_t literal_offset = 0xA0u;
    const uint32_t literals[] = {
        UINT32_C(0x04000000), UINT32_C(0x0403),
        UINT32_C(0x04000130), UINT32_C(0x06000000),
        UINT32_C(0x001F), UINT32_C(0x03E0)
    };
    size_t offset = 0u;
    size_t index;
    FILE *file;
    bool okay;

    gbl_test_emit_arm_ldr_literal(image, &offset, 0u, literal_offset);
    gbl_test_emit_arm_ldr_literal(image, &offset, 1u, literal_offset + 4u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xE5801000));
    gbl_test_emit_arm_ldr_literal(image, &offset, 0u, literal_offset + 8u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xE5900000));
    gbl_test_emit_arm_ldr_literal(image, &offset, 4u, literal_offset);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xE3100002));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0x03A01003));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0x05841000));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xE3100001));
    gbl_test_emit_arm_ldr_literal(image, &offset, 3u,
                                  literal_offset + 12u);
    gbl_test_emit_arm_ldr_literal(image, &offset, 1u,
                                  literal_offset + 16u);
    gbl_test_emit_arm_ldr_literal(image, &offset, 2u,
                                  literal_offset + 20u);
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0x15831000));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0x05832000));
    gbl_test_emit_arm_word(image, &offset, UINT32_C(0xEAFFFFF2));
    for (index = 0u; index < sizeof literals / sizeof literals[0]; ++index) {
        gbl_test_write_le32(image, literal_offset + index * 4u,
                            literals[index]);
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    okay = fwrite(image, 1u, sizeof image, file) == sizeof image;
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay;
}

static bool gbl_test_runner_ppu_observation(const char *scratch_directory)
{
    const gbl_input_event input_event = { 0u, UINT16_C(0x0001) };
    gbl_runner_options options = { 0 };
    gbl_runner_result result = { 0 };
    char rom_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char json[16384];
    size_t json_length = 0u;
    bool okay;

    if (!gbl_test_path(rom_path, sizeof rom_path, scratch_directory,
                       "ppu-observation.gba") ||
        !gbl_test_write_ppu_guest(rom_path)) {
        return false;
    }
    options.rom_path = rom_path;
    options.cycle_budget = UINT64_C(500000);
    options.input_timeline_present = true;
    options.input_events = &input_event;
    options.input_event_count = 1u;
    okay = gbl_runner_execute(&options, &result) == result.status &&
           result.ran && result.cycles > 0u &&
           result.phase8.observed &&
           result.phase8.applied_input_events == 1u &&
           result.semantic.status != GBL_MAILBOX_PASS &&
           gbl_test_serialize_result(&result, json, sizeof json) &&
           (json_length = strlen(json)) > 0u;
    if (okay) {
        json[json_length] = '\0';
        okay = strstr(json, "\"phase10\":{") != NULL &&
               strstr(json,
                      "\"model\":\"gamebloke-ppu-observation-2\",\"profile_version\":12") != NULL &&
               strstr(json, "\"line_cycle\":") != NULL &&
               strstr(json, "\"frame_epoch\":") != NULL &&
               strstr(json, "\"state\":\"valid\"") != NULL &&
               strstr(json, "\"completion_cycle\":\"") != NULL &&
               strstr(json,
                      "\"display_capabilities\":{\"model\":\"gamebloke-display-capabilities-1\",\"raw_output\":{\"format\":\"BGR555_LE\",\"width\":240,\"height\":160,\"stride_bytes\":480,\"byte_length\":76800}") != NULL &&
               strstr(json,
                      "\"mode\":0,\"implementation_status\":\"partial\",\"backgrounds\":[{\"bg\":0,\"source\":\"text\"") != NULL &&
               strstr(json,
                      "\"mode\":1,\"implementation_status\":\"partial\",\"backgrounds\":[{\"bg\":0,\"source\":\"text\",\"dimensions\":[{\"width\":256,\"height\":256},{\"width\":512,\"height\":256},{\"width\":256,\"height\":512},{\"width\":512,\"height\":512}]},{\"bg\":1,\"source\":\"text\",\"dimensions\":[{\"width\":256,\"height\":256},{\"width\":512,\"height\":256},{\"width\":256,\"height\":512},{\"width\":512,\"height\":512}]},{\"bg\":2,\"source\":\"affine\",\"dimensions\":[{\"width\":128,\"height\":128},{\"width\":256,\"height\":256},{\"width\":512,\"height\":512},{\"width\":1024,\"height\":1024}]}]") != NULL &&
               strstr(json,
                      "\"mode\":2,\"implementation_status\":\"partial\",\"backgrounds\":[{\"bg\":2,\"source\":\"affine\",\"dimensions\":[{\"width\":128,\"height\":128},{\"width\":256,\"height\":256},{\"width\":512,\"height\":512},{\"width\":1024,\"height\":1024}]},{\"bg\":3,\"source\":\"affine\"") != NULL &&
               strstr(json,
                      "\"mode\":3,\"implementation_status\":\"partial\",\"backgrounds\":[{\"bg\":2,\"source\":\"direct-color-bitmap\",\"dimensions\":[{\"width\":240,\"height\":160}]") != NULL &&
               strstr(json,
                      "\"mode\":4,\"implementation_status\":\"not-implemented\",\"backgrounds\":[{\"bg\":2,\"source\":\"indexed-bitmap\"") != NULL &&
               strstr(json,
                      "\"mode\":5,\"implementation_status\":\"not-implemented\",\"backgrounds\":[{\"bg\":2,\"source\":\"direct-color-bitmap\",\"dimensions\":[{\"width\":160,\"height\":128}]") != NULL &&
               strstr(json,
                      "\"video_memory_access\":{\"classification\":\"project-selected\",\"hardware_qualification\":\"UNKNOWN\",\"masters\":[\"cpu\",\"dma\"],\"regions\":[\"pram\",\"vram\",\"oam\"],\"states\":[\"drawing\",\"hblank\",\"vblank\",\"forced-blank\"],\"availability\":\"existing-bus-behavior\",\"ppu_extra_cycles\":0,\"cost_reference\":\"bus_policy.request_cycles\"}") != NULL &&
               strstr(json, "\"classification\":\"disputed\"") != NULL &&
               strstr(json, "\"dispute_id\":\"disp-hblank-flag-edge\"") != NULL &&
               strstr(json, "\"hardware_qualification\":\"UNKNOWN\"") != NULL &&
               strstr(json, "\"raw_video\":") != NULL &&
               strstr(json, "\"cycles\":") != NULL &&
               strstr(json,
                      "\"input_timeline\":{\"kind\":\"script\",\"sha256\":") != NULL;
    }
    gbl_runner_result_dispose(&result);
    return okay;
}

static gbl_runner_status gbl_test_execute_ppu(
    const char *rom_path, const char *video_path,
    const gbl_input_event *events, size_t event_count,
    bool timeline_present, uint64_t budget,
    gbl_runner_allocator allocator, gbl_runner_result *result)
{
    gbl_runner_options options = { 0 };

    options.rom_path = rom_path;
    options.cycle_budget = budget;
    options.input_timeline_present = timeline_present;
    options.input_events = events;
    options.input_event_count = event_count;
    options.video_output_path = video_path;
    options.allocator = allocator;
    return gbl_runner_execute(&options, result);
}

static bool gbl_test_video_file_matches(const char *path,
                                       const uint8_t *expected,
                                       size_t expected_length)
{
    FILE *file;
    uint8_t *actual;
    size_t length;
    int extra;
    bool okay;

    if (path == NULL || expected == NULL || expected_length == 0u) {
        return false;
    }
    actual = malloc(expected_length);
    if (actual == NULL) {
        return false;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        free(actual);
        return false;
    }
    length = fread(actual, 1u, expected_length, file);
    extra = fgetc(file);
    okay = length == expected_length && extra == EOF && ferror(file) == 0 &&
           memcmp(actual, expected, expected_length) == 0;
    if (fclose(file) != 0) {
        okay = false;
    }
    free(actual);
    return okay;
}

static bool gbl_test_ppu_write_limit(const char *rom_path,
                                     const char *video_path)
{
    const gbl_input_event pressed_a = { 0u, UINT16_C(0x0001) };
    struct rlimit limit;
    pid_t child;
    int child_status;

    (void)remove(video_path);
    child = fork();
    if (child < 0) {
        return false;
    }
    if (child == 0) {
        gbl_runner_result result = { 0 };
        gbl_runner_status status;
        bool okay;

        limit.rlim_cur = (rlim_t)0;
        limit.rlim_max = (rlim_t)0;
        if (setrlimit(RLIMIT_FSIZE, &limit) != 0 ||
            signal(SIGXFSZ, SIG_IGN) == SIG_ERR) {
            _exit(2);
        }
        status = gbl_test_execute_ppu(
            rom_path, video_path, &pressed_a, 1u, true, UINT64_C(500000),
            (gbl_runner_allocator){ 0 }, &result);
        okay = status == GBL_RUNNER_HARNESS_FAIL &&
               result.status == GBL_RUNNER_HARNESS_FAIL &&
               result.phase10.capture_state == GBL_VIDEO_CAPTURE_VALID &&
               !result.phase10.video_emitted &&
               (strcmp(result.phase10.video_output_error,
                       "video-output-write-failed") == 0 ||
                strcmp(result.phase10.video_output_error,
                       "video-output-close-failed") == 0) &&
               access(video_path, F_OK) != 0 && errno == ENOENT;
        gbl_runner_result_dispose(&result);
        _exit(okay ? 0 : 1);
    }
    if (waitpid(child, &child_status, 0) != child) {
        return false;
    }
    return WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0;
}

static bool gbl_test_runner_ppu_output(const char *scratch_directory)
{
    const gbl_input_event pressed_a = { 0u, UINT16_C(0x0001) };
    const gbl_input_event pressed_b = { 0u, UINT16_C(0x0002) };
    gbl_runner_result first = { 0 };
    gbl_runner_result repeated = { 0 };
    gbl_runner_result changed = { 0 };
    char rom_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char first_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char repeated_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char changed_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char json[16384];
    char digest[GBL_SHA256_HEX_CAPACITY];
    bool okay;

    if (!gbl_test_path(rom_path, sizeof rom_path, scratch_directory,
                       "ppu-output.gba") ||
        !gbl_test_path(first_path, sizeof first_path, scratch_directory,
                       "ppu-output-first.raw") ||
        !gbl_test_path(repeated_path, sizeof repeated_path, scratch_directory,
                       "ppu-output-repeat.raw") ||
        !gbl_test_path(changed_path, sizeof changed_path, scratch_directory,
                       "ppu-output-changed.raw") ||
        !gbl_test_write_ppu_guest(rom_path)) {
        return false;
    }
    (void)remove(first_path);
    (void)remove(repeated_path);
    (void)remove(changed_path);
    okay = gbl_test_execute_ppu(rom_path, first_path, &pressed_a, 1u, true,
                                UINT64_C(500000),
                                (gbl_runner_allocator){ 0 }, &first) ==
               first.status &&
           first.ran && first.phase10.capture_state == GBL_VIDEO_CAPTURE_VALID &&
           first.phase10.video_requested && first.phase10.video_emitted &&
           first.phase10.video_bytes != NULL &&
           first.phase10.video_length == GBL_RUNNER_VIDEO_BYTES &&
           first.phase10.has_video_sha256 && first.phase8.observed &&
           first.phase8.applied_input_events == 1u &&
           first.semantic.status != GBL_MAILBOX_PASS &&
           first.phase10.video_bytes[0] == UINT8_C(0xE0) &&
           first.phase10.video_bytes[1] == UINT8_C(0x03) &&
           gbl_test_video_file_matches(first_path, first.phase10.video_bytes,
                                       GBL_RUNNER_VIDEO_BYTES) &&
           gbl_sha256_span(first.phase10.video_bytes,
                           first.phase10.video_length, digest) &&
           strcmp(digest, first.phase10.video_sha256) == 0 &&
           gbl_test_serialize_result(&first, json, sizeof json) &&
           strstr(json, "\"raw_video\":{\"requested\":true,"
                        "\"status\":\"emitted\",\"emitted\":true,") != NULL &&
           strstr(json,
                  "\"failure\":null,\"format\":\"bgr555-le\"}") != NULL &&
           strstr(json, first.phase10.video_sha256) != NULL &&
           strstr(json, "\"pixels\":") == NULL;

    okay = okay &&
           gbl_test_execute_ppu(rom_path, repeated_path, &pressed_a, 1u, true,
                                UINT64_C(500000),
                                (gbl_runner_allocator){ 0 }, &repeated) ==
               repeated.status &&
           repeated.phase10.video_emitted &&
           repeated.phase10.video_bytes != NULL &&
           repeated.phase10.video_length == GBL_RUNNER_VIDEO_BYTES &&
           repeated.phase10.has_video_sha256 &&
           strcmp(first.phase10.video_sha256,
                  repeated.phase10.video_sha256) == 0 &&
           memcmp(first.phase10.video_bytes, repeated.phase10.video_bytes,
                  GBL_RUNNER_VIDEO_BYTES) == 0 &&
           gbl_test_video_file_matches(repeated_path,
                                       repeated.phase10.video_bytes,
                                       GBL_RUNNER_VIDEO_BYTES);

    okay = okay &&
           gbl_test_execute_ppu(rom_path, changed_path, &pressed_b, 1u, true,
                                UINT64_C(500000),
                                (gbl_runner_allocator){ 0 }, &changed) ==
               changed.status &&
           changed.phase10.video_emitted &&
           changed.phase10.video_bytes != NULL &&
           changed.phase10.video_bytes[0] == UINT8_C(0x1F) &&
           changed.phase10.video_bytes[1] == UINT8_C(0x00) &&
           changed.phase10.has_video_sha256 &&
           strcmp(first.phase10.video_sha256,
                  changed.phase10.video_sha256) != 0 &&
           memcmp(first.phase10.video_bytes, changed.phase10.video_bytes,
                  GBL_RUNNER_VIDEO_BYTES) != 0 &&
           gbl_test_video_file_matches(changed_path,
                                       changed.phase10.video_bytes,
                                       GBL_RUNNER_VIDEO_BYTES);

    gbl_runner_result_dispose(&first);
    gbl_runner_result_dispose(&repeated);
    gbl_runner_result_dispose(&changed);
    return okay;
}

static bool gbl_test_runner_ppu_output_controls(const char *scratch_directory)
{
    const gbl_input_event pressed_a = { 0u, UINT16_C(0x0001) };
    const gbl_input_event unsupported_events[] = {
        { 0u, UINT16_C(0x0001) },
        { UINT64_C(220000), UINT16_C(0x0003) }
    };
    const uint8_t sentinel[] = { UINT8_C(0x71), UINT8_C(0x22), UINT8_C(0xA5) };
    uint8_t caller_storage[] = { UINT8_C(0x31), UINT8_C(0x72), UINT8_C(0xA4) };
    gbl_runner_result existing = { 0 };
    gbl_runner_result bad_parent = { 0 };
    gbl_runner_result allocation = { 0 };
    gbl_runner_result incomplete = { 0 };
    gbl_runner_result next_epoch_incomplete = { 0 };
    gbl_runner_result unsupported = { 0 };
    gbl_runner_result fresh = { 0 };
    gbl_runner_result invalid_path = { 0 };
    gbl_runner_result not_requested = { 0 };
    gbl_runner_result owned = { 0 };
    gbl_runner_result writer_guard = { 0 };
    gbl_test_allocator_state allocator_state = { 0 };
    gbl_runner_allocator allocator;
    char rom_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char existing_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char bad_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char allocation_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char incomplete_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char next_epoch_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char unsupported_rom_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char unsupported_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char fresh_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char writer_guard_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char write_limit_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char owned_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char json[16384];
    char existing_bytes[8] = { 0 };
    FILE *file;
    bool okay;

    if (!gbl_test_path(rom_path, sizeof rom_path, scratch_directory,
                       "ppu-output-controls.gba") ||
        !gbl_test_path(existing_path, sizeof existing_path, scratch_directory,
                       "ppu-output-existing.raw") ||
        !gbl_test_path(bad_path, sizeof bad_path, scratch_directory,
                       "ppu-output-no-parent/output.raw") ||
        !gbl_test_path(allocation_path, sizeof allocation_path,
                       scratch_directory, "ppu-output-allocation.raw") ||
        !gbl_test_path(incomplete_path, sizeof incomplete_path,
                       scratch_directory, "ppu-output-incomplete.raw") ||
        !gbl_test_path(next_epoch_path, sizeof next_epoch_path,
                       scratch_directory, "ppu-output-next-epoch.raw") ||
        !gbl_test_path(unsupported_rom_path, sizeof unsupported_rom_path,
                       scratch_directory, "ppu-output-unsupported.gba") ||
        !gbl_test_path(unsupported_path, sizeof unsupported_path,
                       scratch_directory, "ppu-output-unsupported.raw") ||
        !gbl_test_path(fresh_path, sizeof fresh_path, scratch_directory,
                       "ppu-output-fresh.raw") ||
        !gbl_test_path(writer_guard_path, sizeof writer_guard_path,
                       scratch_directory, "ppu-output-writer-guard.raw") ||
        !gbl_test_path(write_limit_path, sizeof write_limit_path,
                       scratch_directory, "ppu-output-write-limit.raw") ||
        !gbl_test_path(owned_path, sizeof owned_path, scratch_directory,
                       "ppu-output-owned.raw") ||
        !gbl_test_write_ppu_guest(rom_path) ||
        !gbl_test_write_ppu_transition_guest(unsupported_rom_path)) {
        return false;
    }
    (void)remove(existing_path);
    (void)remove(bad_path);
    (void)remove(allocation_path);
    (void)remove(incomplete_path);
    (void)remove(next_epoch_path);
    (void)remove(unsupported_path);
    (void)remove(fresh_path);
    (void)remove(writer_guard_path);
    (void)remove(write_limit_path);
    (void)remove(owned_path);
    if (!gbl_test_write_file(existing_path, 0u, 0u)) {
        return false;
    }
    file = fopen(existing_path, "wb");
    if (file == NULL || fwrite(sentinel, 1u, sizeof sentinel, file) !=
                            sizeof sentinel) {
        if (file != NULL) (void)fclose(file);
        return false;
    }
    if (fclose(file) != 0) return false;

    okay = gbl_test_execute_ppu(rom_path, existing_path, &pressed_a, 1u, true,
                                UINT64_C(500000),
                                (gbl_runner_allocator){ 0 }, &existing) ==
               GBL_RUNNER_HARNESS_FAIL &&
           existing.status == GBL_RUNNER_HARNESS_FAIL &&
           strcmp(existing.reason, "video-output-failed") == 0 &&
           strcmp(existing.termination, "timeout") == 0 &&
           strcmp(existing.phase10.video_output_error,
                  "video-output-already-exists") == 0 &&
           existing.phase10.capture_state == GBL_VIDEO_CAPTURE_VALID &&
           !existing.phase10.video_emitted &&
           gbl_test_read_file(existing_path, existing_bytes,
                              sizeof existing_bytes) &&
           memcmp(existing_bytes, sentinel, sizeof sentinel) == 0;

    okay = okay &&
           gbl_test_execute_ppu(rom_path, bad_path, &pressed_a, 1u, true,
                                UINT64_C(500000),
                                (gbl_runner_allocator){ 0 }, &bad_parent) ==
               GBL_RUNNER_HARNESS_FAIL &&
           strcmp(bad_parent.phase10.video_output_error,
                  "video-output-open-failed") == 0 &&
           !bad_parent.phase10.video_emitted &&
           access(bad_path, F_OK) != 0 && errno == ENOENT;

    memset(&allocator_state, 0, sizeof allocator_state);
    allocator_state.fail_at = 4u;
    allocator = (gbl_runner_allocator){ &allocator_state, gbl_test_allocate,
                                        gbl_test_release };
    okay = okay &&
           gbl_test_execute_ppu(rom_path, allocation_path, &pressed_a, 1u,
                                true, UINT64_C(500000), allocator,
                                &allocation) == GBL_RUNNER_HARNESS_FAIL &&
           strcmp(allocation.phase10.video_output_error,
                  "video-buffer-allocation-failed") == 0 &&
           allocation.phase10.capture_state == GBL_VIDEO_CAPTURE_VALID &&
           !allocation.phase10.video_emitted &&
           access(allocation_path, F_OK) != 0 && errno == ENOENT &&
           allocator_state.successes == allocator_state.releases;

    okay = okay &&
           gbl_test_execute_ppu(rom_path, incomplete_path, &pressed_a, 1u,
                                true, UINT64_C(100),
                                (gbl_runner_allocator){ 0 }, &incomplete) ==
               GBL_RUNNER_HARNESS_FAIL &&
           strcmp(incomplete.phase10.video_output_error,
                  "video-frame-unavailable") == 0 &&
           !incomplete.phase10.video_emitted &&
           access(incomplete_path, F_OK) != 0 && errno == ENOENT;

    okay = okay &&
           gbl_test_execute_ppu(rom_path, next_epoch_path, &pressed_a, 1u,
                                true, UINT64_C(300000),
                                (gbl_runner_allocator){ 0 },
                                &next_epoch_incomplete) ==
               GBL_RUNNER_HARNESS_FAIL &&
           next_epoch_incomplete.phase10.frame_epoch == UINT64_C(1) &&
           next_epoch_incomplete.phase10.capture_state ==
               GBL_VIDEO_CAPTURE_INCOMPLETE &&
           next_epoch_incomplete.phase10.capture_reason ==
               GBL_VIDEO_INVALID_REASON_INCOMPLETE &&
           next_epoch_incomplete.phase10.video_bytes == NULL &&
           next_epoch_incomplete.phase10.video_length == 0u &&
           !next_epoch_incomplete.phase10.video_emitted &&
           strcmp(next_epoch_incomplete.phase10.video_output_error,
                  "video-frame-unavailable") == 0 &&
           access(next_epoch_path, F_OK) != 0 && errno == ENOENT &&
           gbl_test_serialize_result(&next_epoch_incomplete, json,
                                     sizeof json) &&
           strstr(json,
                  "\"frame\":{\"state\":\"incomplete\","
                  "\"reason\":\"incomplete\"") != NULL &&
           strstr(json,
                  "\"video_memory_access\":{\"classification\":"
                  "\"project-selected\"") != NULL &&
           strstr(json,
                  "\"raw_video\":{\"requested\":true,"
                  "\"status\":\"failed\",\"emitted\":false,"
                  "\"byte_length\":null,\"sha256\":null,"
                  "\"failure\":\"video-frame-unavailable\","
                  "\"format\":\"bgr555-le\"}") != NULL;

    okay = okay &&
           gbl_test_execute_ppu(unsupported_rom_path, unsupported_path,
                                unsupported_events,
                                sizeof unsupported_events /
                                    sizeof unsupported_events[0],
                                true, UINT64_C(500000),
                                (gbl_runner_allocator){ 0 }, &unsupported) ==
               GBL_RUNNER_HARNESS_FAIL &&
           unsupported.phase10.frame_epoch == UINT64_C(1) &&
           unsupported.phase10.capture_state == GBL_VIDEO_CAPTURE_INVALID &&
           unsupported.phase10.capture_reason ==
               GBL_VIDEO_INVALID_REASON_BG2_DISABLED &&
           unsupported.phase10.video_bytes == NULL &&
           unsupported.phase10.video_length == 0u &&
           !unsupported.phase10.video_emitted &&
           strcmp(unsupported.phase10.video_output_error,
                  "video-frame-unavailable") == 0 &&
           access(unsupported_path, F_OK) != 0 && errno == ENOENT &&
           gbl_test_serialize_result(&unsupported, json, sizeof json) &&
           strstr(json,
                  "\"frame\":{\"state\":\"invalid\","
                  "\"reason\":\"bg2-disabled\"") != NULL &&
           strstr(json,
                  "\"mode\":3,\"implementation_status\":\"partial\"") != NULL &&
           strstr(json,
                  "\"raw_video\":{\"requested\":true,"
                  "\"status\":\"failed\",\"emitted\":false,"
                  "\"byte_length\":null,\"sha256\":null,"
                  "\"failure\":\"video-frame-unavailable\","
                  "\"format\":\"bgr555-le\"}") != NULL;

    okay = okay &&
           gbl_test_execute_ppu(rom_path, fresh_path, &pressed_a, 1u, true,
                                UINT64_C(500000),
                                (gbl_runner_allocator){ 0 }, &fresh) ==
               GBL_RUNNER_TIMEOUT &&
           fresh.phase10.capture_state == GBL_VIDEO_CAPTURE_VALID &&
           fresh.phase10.video_emitted &&
           fresh.phase10.video_bytes != NULL &&
           fresh.phase10.video_length == GBL_RUNNER_VIDEO_BYTES &&
           gbl_test_video_file_matches(fresh_path, fresh.phase10.video_bytes,
                                       GBL_RUNNER_VIDEO_BYTES);

    writer_guard.phase10.video_requested = true;
    writer_guard.phase10.capture_state = GBL_VIDEO_CAPTURE_VALID;
    writer_guard.phase10.capture_reason = GBL_VIDEO_INVALID_REASON_NONE;
    writer_guard.phase10.frame_started = true;
    writer_guard.phase10.has_frame_epoch = true;
    writer_guard.phase10.supported_controls = true;
    writer_guard.phase10.machine_cycles = UINT64_C(197120);
    writer_guard.phase10.completion_cycle = UINT64_C(197120);
    writer_guard.phase10.sample_cycle = 0u;
    writer_guard.phase10.frame_epoch = 0u;
    writer_guard.phase10.has_sample_cycle = true;
    writer_guard.phase10.has_completion_cycle = true;
    writer_guard.phase10.video_bytes = caller_storage;
    writer_guard.phase10.video_length = sizeof caller_storage;
    okay = okay && !gbl_runner_write_video(writer_guard_path, &writer_guard) &&
           strcmp(writer_guard.phase10.video_output_error,
                  "video-frame-unavailable") == 0 &&
           !writer_guard.phase10.has_video_sha256 &&
           memcmp(caller_storage,
                  (const uint8_t[]){ UINT8_C(0x31), UINT8_C(0x72),
                                     UINT8_C(0xA4) },
                  sizeof caller_storage) == 0 &&
           access(writer_guard_path, F_OK) != 0 && errno == ENOENT;

    writer_guard.phase10.observed = true;
    writer_guard.phase10.has_sample_cycle = false;
    okay = okay && !gbl_runner_write_video(writer_guard_path, &writer_guard) &&
           strcmp(writer_guard.phase10.video_output_error,
                  "video-frame-unavailable") == 0 &&
           access(writer_guard_path, F_OK) != 0 && errno == ENOENT;

    writer_guard.phase10.has_sample_cycle = true;
    writer_guard.phase10.has_completion_cycle = false;
    okay = okay && !gbl_runner_write_video(writer_guard_path, &writer_guard) &&
           strcmp(writer_guard.phase10.video_output_error,
                  "video-frame-unavailable") == 0 &&
           access(writer_guard_path, F_OK) != 0 && errno == ENOENT;

    writer_guard.phase10.has_completion_cycle = true;
    writer_guard.phase10.video_length = sizeof caller_storage - 1u;
    okay = okay && !gbl_runner_write_video(writer_guard_path, &writer_guard) &&
           strcmp(writer_guard.phase10.video_output_error,
                  "video-frame-invalid") == 0 &&
           memcmp(caller_storage,
                  (const uint8_t[]){ UINT8_C(0x31), UINT8_C(0x72),
                                     UINT8_C(0xA4) },
                  sizeof caller_storage) == 0 &&
           access(writer_guard_path, F_OK) != 0 && errno == ENOENT;

    {
        gbl_runner_options options = { 0 };
        options.rom_path = rom_path;
        options.cycle_budget = 100u;
        options.video_output_path = "";
        okay = okay &&
               gbl_runner_execute(&options, &invalid_path) ==
                   GBL_RUNNER_REFUSED &&
               invalid_path.status == GBL_RUNNER_REFUSED &&
               strcmp(invalid_path.reason, "invalid-runner-options") == 0 &&
               strcmp(invalid_path.phase10.video_output_error,
                      "video-output-path-empty") == 0 &&
               !invalid_path.ran;
    }
    okay = okay &&
           !gbl_runner_write_video(NULL, &invalid_path) &&
           strcmp(invalid_path.phase10.video_output_error,
                  "video-output-path-null") == 0 &&
           !gbl_runner_write_video("", &invalid_path) &&
           strcmp(invalid_path.phase10.video_output_error,
                  "video-output-path-empty") == 0;

    memset(&allocator_state, 0, sizeof allocator_state);
    allocator_state.fail_at = 4u;
    allocator = (gbl_runner_allocator){ &allocator_state, gbl_test_allocate,
                                        gbl_test_release };
    okay = okay &&
           gbl_test_execute_ppu(rom_path, NULL, &pressed_a, 1u, true,
                                UINT64_C(100), allocator,
                                &not_requested) == GBL_RUNNER_TIMEOUT &&
           not_requested.ran && !not_requested.phase10.video_requested &&
           not_requested.phase10.video_bytes == NULL &&
           allocator_state.calls == 3u;
    gbl_runner_result_dispose(&not_requested);
    okay = okay &&
           allocator_state.successes == allocator_state.releases &&
           gbl_test_ppu_write_limit(rom_path, write_limit_path);

    memset(&allocator_state, 0, sizeof allocator_state);
    allocator = (gbl_runner_allocator){ &allocator_state, gbl_test_allocate,
                                        gbl_test_release };
    okay = okay &&
           gbl_test_execute_ppu(rom_path, owned_path, &pressed_a, 1u, true,
                                UINT64_C(500000), allocator, &owned) ==
               GBL_RUNNER_TIMEOUT &&
           owned.phase10.video_emitted && owned.phase10.video_bytes != NULL &&
           allocator_state.successes == allocator_state.releases + 1u;
    gbl_runner_result_dispose(&owned);
    {
        const size_t releases_after_dispose = allocator_state.releases;

        okay = okay && allocator_state.successes == releases_after_dispose &&
               owned.phase10.video_bytes == NULL;
        gbl_runner_result_dispose(&owned);
        okay = okay && allocator_state.releases == releases_after_dispose;
    }

    okay = okay && !existing.phase10.video_emitted &&
           !bad_parent.phase10.video_emitted &&
           !allocation.phase10.video_emitted &&
           !incomplete.phase10.video_emitted &&
           !next_epoch_incomplete.phase10.video_emitted &&
           !unsupported.phase10.video_emitted;

    gbl_runner_result_dispose(&existing);
    gbl_runner_result_dispose(&bad_parent);
    gbl_runner_result_dispose(&allocation);
    gbl_runner_result_dispose(&incomplete);
    gbl_runner_result_dispose(&next_epoch_incomplete);
    gbl_runner_result_dispose(&unsupported);
    gbl_runner_result_dispose(&fresh);
    gbl_runner_result_dispose(&invalid_path);
    return okay;
}

#ifdef GBL_RUNNER_TEST_LOCAL_PLAYABLE0
enum {
    GBL_PLAYABLE0_JSON_CAPACITY = 16384u,
    GBL_PLAYABLE0_WATCHDOG_TICKS = 3000u,
    GBL_PLAYABLE0_TEST_CYCLES = 2808960u
};

typedef struct gbl_playable0_run {
    char json[GBL_PLAYABLE0_JSON_CAPACITY];
    char status[32];
    char reason[96];
    char fixture_id[GBL_RUNNER_FIXTURE_ID_CAPACITY];
    char semantic_state[32];
    char frame_state[32];
    char video_sha256[GBL_SHA256_HEX_CAPACITY];
    char input_sha256[GBL_SHA256_HEX_CAPACITY];
    uint64_t case_id;
    uint64_t cycles;
    uint64_t instructions;
    uint64_t applied_events;
    uint64_t pending_events;
    uint64_t keyinput;
    uint64_t frame_epoch;
    uint64_t sample_cycle;
    uint64_t completion_cycle;
    uint64_t frame_length;
    int exit_code;
    uint8_t *frame;
    size_t frame_size;
} gbl_playable0_run;

static bool gbl_playable0_json_string(const char *json, const char *marker,
                                      char *output, size_t capacity)
{
    const char *value = strstr(json, marker);
    const char *end;
    size_t length;

    if (value == NULL || output == NULL || capacity == 0u) {
        return false;
    }
    value += strlen(marker);
    end = strchr(value, '"');
    if (end == NULL) {
        return false;
    }
    length = (size_t)(end - value);
    if (length >= capacity) {
        return false;
    }
    memcpy(output, value, length);
    output[length] = '\0';
    return true;
}

static bool gbl_playable0_json_unsigned(const char *json, const char *marker,
                                       bool quoted, uint64_t *output)
{
    const char *value = strstr(json, marker);
    char *end = NULL;
    unsigned long long parsed;

    if (value == NULL || output == NULL) {
        return false;
    }
    value += strlen(marker);
    if (quoted) {
        if (*value != '"') {
            return false;
        }
        value++;
    }
    if (*value < '0' || *value > '9') {
        return false;
    }
    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno != 0 || end == value || (quoted && *end != '"') ||
        (!quoted && *end != ',' && *end != '}')) {
        return false;
    }
    *output = (uint64_t)parsed;
    return true;
}

static bool gbl_playable0_parse_result(gbl_playable0_run *run)
{
    const char *fixture = strstr(run->json, "\"fixture\":{");
    const char *semantic = strstr(run->json, "\"semantic\":{");
    const char *time = strstr(run->json, "\"time\":{");
    const char *keypad = strstr(run->json, "\"keypad\":{");
    const char *raster = strstr(run->json, "\"raster\":{");
    const char *frame = strstr(run->json, "\"frame\":{");
    const char *video = strstr(run->json, "\"raw_video\":{");
    const char *input = strstr(run->json, "\"input_timeline\":{");

    if (strstr(run->json, "\"schema\":\"gamebloke.result/1\"") == NULL ||
        !gbl_playable0_json_string(run->json, "\"status\":\"",
                                   run->status, sizeof run->status) ||
        !gbl_playable0_json_string(run->json, "\"reason\":\"",
                                   run->reason, sizeof run->reason)) {
        return false;
    }
    if (fixture != NULL &&
        !gbl_playable0_json_string(fixture, "\"id\":\"",
                                   run->fixture_id,
                                   sizeof run->fixture_id)) {
        run->fixture_id[0] = '\0';
    }
    if (semantic != NULL) {
        (void)gbl_playable0_json_string(semantic, "\"state\":\"",
                                       run->semantic_state,
                                       sizeof run->semantic_state);
        (void)gbl_playable0_json_unsigned(semantic, "\"case_id\":", true,
                                          &run->case_id);
    }
    if (time != NULL) {
        (void)gbl_playable0_json_unsigned(time, "\"cycles\":", true,
                                          &run->cycles);
        (void)gbl_playable0_json_unsigned(time, "\"instructions\":", true,
                                          &run->instructions);
    }
    if (keypad != NULL) {
        (void)gbl_playable0_json_unsigned(keypad, "\"applied_events\":",
                                          false, &run->applied_events);
        (void)gbl_playable0_json_unsigned(keypad, "\"pending_events\":",
                                          false, &run->pending_events);
        (void)gbl_playable0_json_unsigned(keypad, "\"keyinput\":", false,
                                          &run->keyinput);
    }
    if (raster != NULL) {
        (void)gbl_playable0_json_unsigned(raster, "\"frame_epoch\":", true,
                                          &run->frame_epoch);
    }
    if (frame != NULL) {
        (void)gbl_playable0_json_string(frame, "\"state\":\"",
                                       run->frame_state,
                                       sizeof run->frame_state);
        (void)gbl_playable0_json_unsigned(frame, "\"byte_length\":", false,
                                          &run->frame_length);
        (void)gbl_playable0_json_unsigned(frame, "\"sample_cycle\":", true,
                                          &run->sample_cycle);
        (void)gbl_playable0_json_unsigned(frame, "\"completion_cycle\":",
                                          true, &run->completion_cycle);
    }
    if (video != NULL) {
        (void)gbl_playable0_json_string(video, "\"sha256\":\"",
                                       run->video_sha256,
                                       sizeof run->video_sha256);
    }
    if (input != NULL) {
        (void)gbl_playable0_json_string(input, "\"sha256\":\"",
                                       run->input_sha256,
                                       sizeof run->input_sha256);
    }
    return true;
}

static bool gbl_playable0_write_timeline(const char *path, bool final_right)
{
    FILE *file = fopen(path, "wb");
    bool okay;

    if (file == NULL) {
        return false;
    }
    okay = fputs(final_right
                     ? "0 0\n1450000 16\n1730000 17\n2010000 1\n2290000 16\n"
                     : "0 0\n1450000 16\n1730000 17\n2010000 1\n2290000 0\n",
                 file) >= 0;
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay;
}

static bool gbl_playable0_read_frame(gbl_playable0_run *run,
                                     const char *path)
{
    FILE *file = fopen(path, "rb");
    size_t length;
    int extra;
    bool okay;

    if (file == NULL) {
        return false;
    }
    run->frame = malloc(GBL_RUNNER_VIDEO_BYTES);
    if (run->frame == NULL) {
        (void)fclose(file);
        return false;
    }
    length = fread(run->frame, 1u, GBL_RUNNER_VIDEO_BYTES, file);
    extra = fgetc(file);
    okay = length == GBL_RUNNER_VIDEO_BYTES && extra == EOF &&
           ferror(file) == 0;
    if (fclose(file) != 0) {
        okay = false;
    }
    if (!okay) {
        free(run->frame);
        run->frame = NULL;
        return false;
    }
    run->frame_size = length;
    return true;
}

static bool gbl_playable0_run_guest(const char *scratch_directory,
                                   const char *run_name,
                                   const char *fixture_id,
                                   const char *rom_name, bool final_right,
                                   gbl_playable0_run *run)
{
    char rom_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char input_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char result_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char error_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char video_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    struct timespec poll_wait = { 0, 10000000L };
    pid_t child;
    int child_status = 0;
    size_t tick;

    memset(run, 0, sizeof *run);
    if (!gbl_test_path(rom_path, sizeof rom_path, GBL_RUNNER_TEST_FIXTURE_DIR,
                       rom_name) ||
        !gbl_test_path(input_path, sizeof input_path, scratch_directory,
                       run_name) ||
        !gbl_test_path(result_path, sizeof result_path, scratch_directory,
                       run_name) ||
        !gbl_test_path(error_path, sizeof error_path, scratch_directory,
                       run_name) ||
        !gbl_test_path(video_path, sizeof video_path, scratch_directory,
                       run_name) ||
        strlen(input_path) + sizeof ".input" >= sizeof input_path ||
        strlen(result_path) + sizeof ".json" >= sizeof result_path ||
        strlen(error_path) + sizeof ".stderr" >= sizeof error_path ||
        strlen(video_path) + sizeof ".raw" >= sizeof video_path) {
        return false;
    }
    strcat(input_path, ".input");
    strcat(result_path, ".json");
    strcat(error_path, ".stderr");
    strcat(video_path, ".raw");
    if (!gbl_playable0_write_timeline(input_path, final_right)) {
        return false;
    }
    (void)remove(result_path);
    (void)remove(error_path);
    (void)remove(video_path);

    child = fork();
    if (child < 0) {
        return false;
    }
    if (child == 0) {
        const int out_fd = open(result_path, O_CREAT | O_TRUNC | O_WRONLY,
                                0600);
        const int err_fd = open(error_path, O_CREAT | O_TRUNC | O_WRONLY,
                                0600);

        if (out_fd < 0 || err_fd < 0 || dup2(out_fd, STDOUT_FILENO) < 0 ||
            dup2(err_fd, STDERR_FILENO) < 0) {
            _exit(126);
        }
        (void)close(out_fd);
        (void)close(err_fd);
        execl(GBL_RUNNER_TEST_PLAYABLE_CLI, "gamebloke-run", "--rom",
              rom_path, "--fixture", fixture_id, "--cycles", "2808960",
              "--input-script", input_path, "--video-output", video_path,
              (char *)NULL);
        _exit(127);
    }

    for (tick = 0u; tick < GBL_PLAYABLE0_WATCHDOG_TICKS; ++tick) {
        const pid_t waited = waitpid(child, &child_status, WNOHANG);

        if (waited == child) {
            if (!WIFEXITED(child_status) ||
                (WEXITSTATUS(child_status) != 0 &&
                 WEXITSTATUS(child_status) != 1) ||
                !gbl_test_read_file(result_path, run->json,
                                   sizeof run->json)) {
                return false;
            }
            run->exit_code = WEXITSTATUS(child_status);
            return gbl_playable0_parse_result(run);
        }
        if (waited < 0 && errno != EINTR) {
            (void)kill(child, SIGKILL);
            (void)waitpid(child, &child_status, 0);
            return false;
        }
        (void)nanosleep(&poll_wait, NULL);
    }
    (void)kill(child, SIGKILL);
    (void)waitpid(child, &child_status, 0);
    return false;
}

static uint16_t gbl_playable0_pixel(const uint8_t *frame, unsigned x,
                                   unsigned y)
{
    const size_t offset = ((size_t)y * 240u + (size_t)x) * 2u;

    return (uint16_t)((uint16_t)frame[offset] |
                      ((uint16_t)frame[offset + 1u] << 8u));
}

static bool gbl_playable0_region_is(const uint8_t *frame, unsigned x,
                                    unsigned y, uint16_t color)
{
    unsigned row;
    unsigned column;

    if (frame == NULL || x > 232u || y > 152u) {
        return false;
    }
    for (row = 0u; row < 8u; ++row) {
        for (column = 0u; column < 8u; ++column) {
            if (gbl_playable0_pixel(frame, x + column, y + row) != color) {
                return false;
            }
        }
    }
    return true;
}

static bool gbl_playable0_find_region(const uint8_t *frame, uint16_t color,
                                      unsigned *found_x,
                                      unsigned *found_y)
{
    unsigned y;

    if (frame == NULL || found_x == NULL || found_y == NULL) {
        return false;
    }
    for (y = 0u; y <= 152u; ++y) {
        unsigned x;

        for (x = 0u; x <= 232u; ++x) {
            if (gbl_playable0_pixel(frame, x, y) == color &&
                gbl_playable0_region_is(frame, x, y, color)) {
                *found_x = x;
                *found_y = y;
                return true;
            }
        }
    }
    return false;
}

static size_t gbl_playable0_difference_count(const uint8_t *left,
                                            const uint8_t *right,
                                            size_t length)
{
    size_t index;
    size_t differences = 0u;

    for (index = 0u; index < length; ++index) {
        if (left[index] != right[index]) {
            differences++;
        }
    }
    return differences;
}

static const char *gbl_playable0_evaluate(const gbl_playable0_run *run,
                                         const char *expected_fixture,
                                         const uint8_t *frame,
                                         uint64_t expected_case,
                                         unsigned expected_x,
                                         uint16_t expected_color,
                                         uint64_t expected_keyinput)
{
    unsigned actual_x;
    unsigned actual_y;

    if (run->exit_code != 0 || strcmp(run->status, "PASS") != 0 ||
        strcmp(run->fixture_id, expected_fixture) != 0) {
        return "fixture-status-or-registration";
    }
    if (strcmp(run->semantic_state, "pass") != 0) {
        return "mailbox-not-pass";
    }
    actual_x = (unsigned)((run->case_id >> 13u) & UINT64_C(0xFF));
    if (actual_x != expected_x) {
        return "guest-position";
    }
    if (run->case_id != expected_case) {
        return "mailbox-progress";
    }
    if (run->applied_events != 5u || run->pending_events != 0u ||
        run->keyinput != expected_keyinput) {
        return "input-observation";
    }
    if (run->cycles == 0u || run->cycles > GBL_PLAYABLE0_TEST_CYCLES ||
        strcmp(run->frame_state, "valid") != 0 ||
        run->frame_length != GBL_RUNNER_VIDEO_BYTES ||
        run->frame_size != GBL_RUNNER_VIDEO_BYTES || frame == NULL) {
        return "complete-raw-frame";
    }
    if (!gbl_playable0_region_is(frame, expected_x, 80u, expected_color)) {
        if (gbl_playable0_find_region(frame, expected_color, &actual_x,
                                      &actual_y)) {
            return actual_x != expected_x || actual_y != 80u
                       ? "guest-position"
                       : "raw-frame-color";
        }
        return "raw-frame-color";
    }
    return NULL;
}

static bool gbl_playable0_hash_matches(const gbl_playable0_run *run)
{
    char actual[GBL_SHA256_HEX_CAPACITY];

    return run->frame != NULL &&
           gbl_sha256_span(run->frame, run->frame_size, actual) &&
           strcmp(actual, run->video_sha256) == 0;
}

static bool gbl_playable0_replay_matches(const gbl_playable0_run *left,
                                         const gbl_playable0_run *right)
{
    return strcmp(left->status, right->status) == 0 &&
           strcmp(left->reason, right->reason) == 0 &&
           strcmp(left->fixture_id, right->fixture_id) == 0 &&
           strcmp(left->semantic_state, right->semantic_state) == 0 &&
           left->case_id == right->case_id && left->cycles == right->cycles &&
           left->instructions == right->instructions &&
           left->applied_events == right->applied_events &&
           left->pending_events == right->pending_events &&
           left->keyinput == right->keyinput &&
           left->frame_epoch == right->frame_epoch &&
           left->sample_cycle == right->sample_cycle &&
           left->completion_cycle == right->completion_cycle &&
           left->frame_length == right->frame_length &&
           strcmp(left->input_sha256, right->input_sha256) == 0 &&
           strcmp(left->video_sha256, right->video_sha256) == 0 &&
           left->frame_size == right->frame_size && left->frame != NULL &&
           right->frame != NULL &&
           memcmp(left->frame, right->frame, left->frame_size) == 0;
}

static void gbl_playable0_dispose(gbl_playable0_run *run)
{
    free(run->frame);
    run->frame = NULL;
    run->frame_size = 0u;
}

static bool gbl_test_runner_playable0_verify(const char *scratch_directory)
{
    gbl_playable0_run neutral = { 0 };
    gbl_playable0_run repeated = { 0 };
    gbl_playable0_run changed = { 0 };
    gbl_playable0_run interleaved = { 0 };
    char path[GBL_RUNNER_TEST_PATH_CAPACITY];
    const char *neutral_error;
    const char *changed_error;
    bool setup;
    bool replay;

    setup = gbl_playable0_run_guest(scratch_directory, "neutral",
                                    "owned-playable0", "owned-playable0.gba",
                                    false, &neutral) &&
            gbl_playable0_run_guest(scratch_directory, "repeat",
                                    "owned-playable0", "owned-playable0.gba",
                                    false, &repeated) &&
            gbl_playable0_run_guest(scratch_directory, "changed",
                                    "owned-playable0", "owned-playable0.gba",
                                    true, &changed) &&
            gbl_playable0_run_guest(scratch_directory, "interleaved",
                                    "owned-playable0", "owned-playable0.gba",
                                    false, &interleaved);
    if (!setup) {
        gbl_playable0_dispose(&neutral);
        gbl_playable0_dispose(&repeated);
        gbl_playable0_dispose(&changed);
        gbl_playable0_dispose(&interleaved);
        return false;
    }
    (void)gbl_test_path(path, sizeof path, scratch_directory, "neutral.raw");
    if (!gbl_playable0_read_frame(&neutral, path)) {
        neutral.frame = NULL;
    }
    (void)gbl_test_path(path, sizeof path, scratch_directory, "repeat.raw");
    if (!gbl_playable0_read_frame(&repeated, path)) {
        repeated.frame = NULL;
    }
    (void)gbl_test_path(path, sizeof path, scratch_directory, "changed.raw");
    if (!gbl_playable0_read_frame(&changed, path)) {
        changed.frame = NULL;
    }
    (void)gbl_test_path(path, sizeof path, scratch_directory,
                        "interleaved.raw");
    if (!gbl_playable0_read_frame(&interleaved, path)) {
        interleaved.frame = NULL;
    }

    neutral_error = gbl_playable0_evaluate(
        &neutral, "owned-playable0", neutral.frame, UINT64_C(0xABE80000), 64u,
        UINT16_C(0x03E0), UINT64_C(1023));
    gbl_test_case(neutral_error == NULL,
                  "registered local guest returns PASS with five-word mailbox progress");
    gbl_test_case(neutral_error == NULL && neutral.applied_events == 5u &&
                      neutral.pending_events == 0u && neutral.keyinput == 1023u &&
                      neutral.cycles <= GBL_PLAYABLE0_TEST_CYCLES,
                  "neutral/right/hold/release input observations and cycle budget are separate");

    replay = gbl_playable0_hash_matches(&neutral) &&
             gbl_playable0_replay_matches(&neutral, &repeated) &&
             gbl_playable0_replay_matches(&neutral, &interleaved);
    gbl_test_case(replay,
                  "fresh and interleaved baseline runs match metadata and all 76,800 raw bytes");
    changed_error = gbl_playable0_evaluate(
        &changed, "owned-playable0", changed.frame,
        UINT64_C(0xABF00000), 128u,
        UINT16_C(0x03E0), UINT64_C(1007));
    gbl_test_case(changed_error == NULL && gbl_playable0_hash_matches(&changed) &&
                      changed.frame != NULL && neutral.frame != NULL &&
                      strcmp(neutral.video_sha256, changed.video_sha256) != 0 &&
                      gbl_playable0_difference_count(
                          neutral.frame, changed.frame,
                          GBL_RUNNER_VIDEO_BYTES) == 256u,
                  "final RIGHT moves the marker to x=128 and changes its two 8x8 regions");

    gbl_playable0_dispose(&neutral);
    gbl_playable0_dispose(&repeated);
    gbl_playable0_dispose(&changed);
    gbl_playable0_dispose(&interleaved);
    return true;
}

static bool gbl_test_runner_playable0_control(const char *scratch_directory)
{
    gbl_playable0_run clean = { 0 };
    gbl_playable0_run input_disabled = { 0 };
    gbl_playable0_run wrong_color = { 0 };
    gbl_playable0_run wrong_address = { 0 };
    const char *input_reason;
    const char *color_reason;
    const char *address_reason;
    uint8_t *tampered = NULL;
    bool setup;
    bool tampered_rejected = false;
    char path[GBL_RUNNER_TEST_PATH_CAPACITY];
    size_t pixel_offset;

    setup = gbl_playable0_run_guest(scratch_directory, "clean",
                                    "owned-playable0", "owned-playable0.gba",
                                    false, &clean) &&
            gbl_playable0_run_guest(
                scratch_directory, "input-disabled",
                "owned-playable0-input-disabled",
                "owned-playable0-input-disabled.gba", true,
                &input_disabled) &&
            gbl_playable0_run_guest(
                scratch_directory, "wrong-color", "owned-playable0-wrong-color",
                "owned-playable0-wrong-color.gba", false, &wrong_color) &&
            gbl_playable0_run_guest(
                scratch_directory, "wrong-address",
                "owned-playable0-wrong-address",
                "owned-playable0-wrong-address.gba", false, &wrong_address);
    if (!setup) {
        gbl_playable0_dispose(&clean);
        gbl_playable0_dispose(&input_disabled);
        gbl_playable0_dispose(&wrong_color);
        gbl_playable0_dispose(&wrong_address);
        return false;
    }
    (void)gbl_test_path(path, sizeof path, scratch_directory, "clean.raw");
    setup = gbl_playable0_read_frame(&clean, path);
    (void)gbl_test_path(path, sizeof path, scratch_directory,
                        "input-disabled.raw");
    setup = setup && gbl_playable0_read_frame(&input_disabled, path);
    (void)gbl_test_path(path, sizeof path, scratch_directory, "wrong-color.raw");
    setup = setup && gbl_playable0_read_frame(&wrong_color, path);
    (void)gbl_test_path(path, sizeof path, scratch_directory,
                        "wrong-address.raw");
    setup = setup && gbl_playable0_read_frame(&wrong_address, path);
    if (!setup) {
        gbl_playable0_dispose(&clean);
        gbl_playable0_dispose(&input_disabled);
        gbl_playable0_dispose(&wrong_color);
        gbl_playable0_dispose(&wrong_address);
        return false;
    }

    gbl_test_case(
        gbl_playable0_evaluate(&clean, "owned-playable0", clean.frame,
                               UINT64_C(0xABE80000), 64u, UINT16_C(0x03E0),
                               UINT64_C(1023)) == NULL,
        "clean baseline twin passes the source-bound mailbox and pixel evaluator");
    input_reason = gbl_playable0_evaluate(
        &input_disabled, "owned-playable0-input-disabled",
        input_disabled.frame, UINT64_C(0xABF00000), 128u, UINT16_C(0x03E0),
        UINT64_C(1007));
    gbl_test_case(input_reason != NULL &&
                      strcmp(input_reason, "guest-position") == 0,
                  "input-disabled guest is rejected for missing right-key movement");
    color_reason = gbl_playable0_evaluate(
        &wrong_color, "owned-playable0-wrong-color", wrong_color.frame,
        UINT64_C(0xABE80000), 64u, UINT16_C(0x03E0), UINT64_C(1023));
    gbl_test_case(color_reason != NULL &&
                      strcmp(color_reason, "raw-frame-color") == 0,
                  "wrong-color guest is rejected by the named raw pixel check");
    address_reason = gbl_playable0_evaluate(
        &wrong_address, "owned-playable0-wrong-address", wrong_address.frame,
        UINT64_C(0xABE80000), 64u, UINT16_C(0x03E0), UINT64_C(1023));
    gbl_test_case(address_reason != NULL &&
                      strcmp(address_reason, "guest-position") == 0,
                  "wrong-address guest is rejected by the named marker position check");

    tampered = malloc(GBL_RUNNER_VIDEO_BYTES);
    if (tampered != NULL) {
        memcpy(tampered, clean.frame, GBL_RUNNER_VIDEO_BYTES);
        pixel_offset = ((size_t)80u * 240u + 64u) * 2u;
        tampered[pixel_offset] ^= UINT8_C(1);
        tampered_rejected =
            strcmp(gbl_playable0_evaluate(
                       &clean, "owned-playable0", tampered,
                       UINT64_C(0xABE80000), 64u, UINT16_C(0x03E0),
                       UINT64_C(1023)),
                   "raw-frame-color") == 0;
    }
    gbl_test_case(tampered_rejected,
                  "one-byte frame tamper is rejected by semantic pixel inspection");

    free(tampered);
    gbl_playable0_dispose(&clean);
    gbl_playable0_dispose(&input_disabled);
    gbl_playable0_dispose(&wrong_color);
    gbl_playable0_dispose(&wrong_address);
    return true;
}
#endif

static bool gbl_test_runner_dma_observation(const char *scratch_directory)
{
    char rom_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char trace_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char hblank_trace_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char json[8192];
    char trace[4096];
    char hblank_trace[1024];
    gbl_runner_result result = { 0 };
    gbl_runner_result hblank_trace_result = { 0 };
    gbl_runner_result full_trace_result = { 0 };
    gbl_runner_result partial_latch_result = { 0 };
    gbl_runner_result incomplete_trace_result = { 0 };
    gbl_runner_result timeout_result = { 0 };
    gbl_runner_result cancelled_result = { 0 };
    gbl_runner_status status;
    gbl_test_cancel_state cancel_probe = { 0u, SIZE_MAX };
    gbl_test_cancel_state cancel_state = { 0u, 0u };
    gbl_input_event input_event;
    gbl_trace_record hblank_record = { 0 };
    uint64_t total;
    bool okay;
    size_t latch_trace_index = SIZE_MAX;
    size_t index;

    if (!gbl_test_path(rom_path, sizeof rom_path, scratch_directory,
                       "runner-dma-observation.gba") ||
        !gbl_test_path(trace_path, sizeof trace_path, scratch_directory,
                       "runner-dma-observation.jsonl") ||
        !gbl_test_write_dma_guest(rom_path)) {
        return false;
    }
    (void)remove(trace_path);
    status = gbl_test_execute(rom_path, NULL, UINT64_C(1000),
                              (uint32_t)GBL_TRACE_DMA, 64u, trace_path,
                              NULL, NULL, (gbl_runner_allocator){ 0 },
                              &result);
    okay = status == GBL_RUNNER_GUEST_STOP &&
           result.status == GBL_RUNNER_GUEST_STOP && result.ran &&
           result.semantic.status == GBL_MAILBOX_PASS &&
           result.trace_count == 4u &&
           result.trace_records[0].category == GBL_TRACE_DMA &&
           result.trace_records[0].bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
           result.trace_records[1].dma_source_address ==
               UINT32_C(0x08000200) &&
           result.trace_records[1].dma_destination_address ==
               UINT32_C(0x03000020) &&
           result.trace_records[1].dma_value == UINT32_C(0xBEEF) &&
           result.trace_records[1].dma_width == 2u &&
           result.trace_records[1].bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
           result.trace_records[3].dma_source_address == 0u &&
           result.trace_records[3].dma_destination_address ==
               UINT32_C(0x03000022) &&
           result.trace_records[3].dma_value == UINT32_C(0xBEEF) &&
           result.trace_records[3].dma_width == 2u &&
           result.trace_records[2].bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
           result.trace_records[3].bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
           result.trace_records[3].dma_used_channel_latch &&
           result.trace_records[3].dma_value_provenance ==
               GBL_DMA_VALUE_PROVENANCE_PROJECT_SELECTED &&
           result.trace_records[3].dma_result_classification ==
               GBL_DMA_RESULT_CLASSIFICATION_DISPUTED &&
           result.trace_records[3].dma_dispute == GBL_DMA_DISPUTE_LATCH &&
           result.trace_records[3].dma_hardware_qualification ==
               GBL_HARDWARE_QUALIFICATION_UNKNOWN &&
           result.phase9.observed && result.phase9.waitcnt == 0u &&
           result.phase9.bus_owner == GBL_MACHINE_BUS_OWNER_CPU &&
           result.phase9.machine_cycles == result.cycles &&
           result.phase9.channels[3].completed_units == 1u &&
           result.phase9.channels[3].remaining == 0u &&
           result.phase9.channels[3].source_address == 0u &&
           result.phase9.channels[3].destination_address ==
               UINT32_C(0x03000022) &&
           result.phase9.channels[3].read_latch == UINT32_C(0xBEEFBEEF) &&
           result.dma_startup_cycles == 4u &&
           result.dma_source_cycles == 6u &&
           result.dma_destination_cycles == 2u &&
           result.dma_completed_units == 2u && result.dma_pause_cycles == 12u &&
           result.bus_cycles <= UINT64_MAX - result.internal_cycles &&
           (total = result.bus_cycles + result.internal_cycles,
            total <= UINT64_MAX - result.wait_cycles) &&
           (total += result.wait_cycles,
            total <= UINT64_MAX - result.dma_startup_cycles) &&
           (total += result.dma_startup_cycles,
            total <= UINT64_MAX - result.dma_source_cycles) &&
           (total += result.dma_source_cycles,
            total <= UINT64_MAX - result.dma_destination_cycles) &&
           (total += result.dma_destination_cycles, total == result.cycles) &&
           gbl_test_serialize_result(&result, json, sizeof json) &&
           strstr(json, "\"phase9\"") != NULL &&
           strstr(json,
                  "\"fallback_observed\":true,\"classification\":\"disputed\",\"dispute_id\":\"disp-dma-latch\",\"hardware_qualification\":\"UNKNOWN\"") != NULL &&
           strstr(json, "\"disputes\":[\"disp-dma-latch\"]") != NULL &&
           strstr(json, "\"labels\":[\"disputed\"]") != NULL &&
           gbl_test_read_file(trace_path, trace, sizeof trace) &&
           strstr(trace,
                  "\"source_address\":\"0x00000000\",\"destination_address\":\"0x03000022\",\"value\":\"0x0000BEEF\",\"width\":2") != NULL &&
           strstr(trace,
                  "\"used_channel_latch\":true,\"value_provenance\":\"project-selected\",\"classification\":\"disputed\",\"dispute_id\":\"disp-dma-latch\",\"hardware_qualification\":\"UNKNOWN\"") != NULL;
    {
        bool hblank_trace_okay = result.trace_records != NULL &&
            result.trace_count > 3u &&
            gbl_test_path(hblank_trace_path, sizeof hblank_trace_path,
                          scratch_directory, "hblank-dma-dispute.jsonl");

        if (hblank_trace_okay) {
            hblank_record = result.trace_records[3];
            hblank_record.dma_dispute = GBL_DMA_DISPUTE_HBLANK_FLAG_EDGE;
            hblank_trace_result.trace_records = &hblank_record;
            hblank_trace_result.trace_capacity = 1u;
            hblank_trace_result.trace_count = 1u;
            (void)remove(hblank_trace_path);
            hblank_trace_okay = gbl_runner_write_trace(
                                    hblank_trace_path,
                                    &hblank_trace_result) &&
                gbl_test_read_file(hblank_trace_path, hblank_trace,
                                   sizeof hblank_trace) &&
                strstr(hblank_trace,
                    "\"dispute_id\":\"disp-hblank-flag-edge\",\"hardware_qualification\":\"UNKNOWN\"") != NULL;
        }
        okay = okay && hblank_trace_okay;
    }
    status = gbl_test_execute(rom_path, NULL, UINT64_C(1000),
                              (uint32_t)GBL_TRACE_DMA, 1u, NULL, NULL, NULL,
                              (gbl_runner_allocator){ 0 },
                              &incomplete_trace_result);
    okay = okay && status == GBL_RUNNER_HARNESS_FAIL &&
           !incomplete_trace_result.trace_complete &&
           incomplete_trace_result.phase9.observed &&
           !incomplete_trace_result.phase9.latch_fallback_known &&
           gbl_test_serialize_result(&incomplete_trace_result, json,
                                     sizeof json) &&
           strstr(json, "\"fallback_observed\":null") != NULL &&
           strstr(json, "\"disputes\":[]") != NULL;
    gbl_runner_result_dispose(&incomplete_trace_result);

    status = gbl_test_execute(
        rom_path, NULL, UINT64_C(1000),
        (uint32_t)(GBL_TRACE_DMA | GBL_TRACE_INSTRUCTION), 256u, NULL, NULL,
        NULL, (gbl_runner_allocator){ 0 }, &full_trace_result);
    for (index = 0u; index < full_trace_result.trace_count; ++index) {
        if (full_trace_result.trace_records[index].category == GBL_TRACE_DMA &&
            full_trace_result.trace_records[index].dma_used_channel_latch) {
            latch_trace_index = index;
            break;
        }
    }
    okay = okay && status == GBL_RUNNER_GUEST_STOP &&
           full_trace_result.trace_complete && latch_trace_index != SIZE_MAX &&
           full_trace_result.trace_count > latch_trace_index + 1u;
    gbl_runner_result_dispose(&full_trace_result);

    if (latch_trace_index != SIZE_MAX && latch_trace_index < SIZE_MAX - 1u) {
        status = gbl_test_execute(
            rom_path, NULL, UINT64_C(1000),
            (uint32_t)(GBL_TRACE_DMA | GBL_TRACE_INSTRUCTION),
            latch_trace_index + 1u, NULL, NULL, NULL,
            (gbl_runner_allocator){ 0 }, &partial_latch_result);
        okay = okay && status == GBL_RUNNER_HARNESS_FAIL &&
               !partial_latch_result.trace_complete &&
               partial_latch_result.phase9.latch_fallback_known &&
               partial_latch_result.phase9.latch_fallback_observed &&
               gbl_test_serialize_result(&partial_latch_result, json,
                                         sizeof json) &&
               strstr(json,
                      "\"fallback_observed\":true,\"classification\":\"disputed\",\"dispute_id\":\"disp-dma-latch\"") != NULL &&
               strstr(json, "\"disputes\":[\"disp-dma-latch\"]") != NULL;
    } else {
        okay = false;
    }
    gbl_runner_result_dispose(&partial_latch_result);
    input_event = (gbl_input_event){ result.trace_records[1].guest_cycle, 1u };
    status = gbl_test_execute_timeline(
        rom_path, NULL, UINT64_C(1000), true, &input_event, 1u, NULL, NULL,
        (gbl_runner_allocator){ 0 }, &timeout_result);
    okay = okay && status == GBL_RUNNER_GUEST_STOP &&
           timeout_result.phase8.applied_input_events == 1u &&
           timeout_result.phase8.keyinput == UINT16_C(0x03FE);
    gbl_runner_result_dispose(&timeout_result);

    status = gbl_test_execute(rom_path, NULL,
                              result.trace_records[0].guest_cycle,
                              (uint32_t)GBL_TRACE_DMA, 64u, NULL, NULL, NULL,
                              (gbl_runner_allocator){ 0 }, &timeout_result);
    okay = okay && status == GBL_RUNNER_TIMEOUT &&
           timeout_result.phase9.observed &&
           timeout_result.phase9.bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
           timeout_result.phase9.channels[3].active &&
           timeout_result.phase9.channels[3].startup_complete &&
           timeout_result.phase9.channels[3].remaining == 1u &&
           timeout_result.phase9.channels[3].completed_units == 0u &&
           timeout_result.trace_count == 1u;
    gbl_runner_result_dispose(&timeout_result);

    status = gbl_test_execute(rom_path, NULL,
                              result.trace_records[0].guest_cycle,
                              (uint32_t)GBL_TRACE_DMA, 64u, NULL,
                              gbl_test_cancel_at_boundary, &cancel_probe,
                              (gbl_runner_allocator){ 0 },
                              &cancelled_result);
    okay = okay && status == GBL_RUNNER_TIMEOUT && cancel_probe.calls > 0u &&
           cancelled_result.phase9.bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
           cancelled_result.phase9.channels[3].active &&
           cancelled_result.phase9.channels[3].remaining == 1u &&
           cancelled_result.trace_count == 1u;
    gbl_runner_result_dispose(&cancelled_result);

    cancel_state.cancel_at = cancel_probe.calls;
    status = gbl_test_execute(rom_path, NULL,
                              result.trace_records[0].guest_cycle,
                              (uint32_t)GBL_TRACE_DMA, 64u, NULL,
                              gbl_test_cancel_at_boundary, &cancel_state,
                              (gbl_runner_allocator){ 0 },
                              &cancelled_result);
    okay = okay && status == GBL_RUNNER_CANCELLED &&
           cancelled_result.phase9.observed &&
           cancelled_result.phase9.bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
           cancelled_result.phase9.channels[3].active &&
           cancelled_result.phase9.channels[3].remaining == 1u &&
           cancelled_result.phase9.channels[3].completed_units == 0u &&
           cancelled_result.trace_count == 1u;
    gbl_runner_result_dispose(&cancelled_result);
    gbl_runner_result_dispose(&result);
    return okay;
}

static bool gbl_test_write_timer0_guest(const char *path, bool enabled)
{
    uint8_t image[0xC0u] = { 0u };
    FILE *file;
    bool okay;

    /* Original ARM program: start Timer 0, poll its IF request, then write
     * the five-word semantic mailbox with the completion marker last. */
    gbl_test_write_le32(image, 0x00u, UINT32_C(0xE59F0054));
    gbl_test_write_le32(image, 0x04u, UINT32_C(0xE59F1054));
    gbl_test_write_le32(image, 0x08u, UINT32_C(0xE5801000));
    gbl_test_write_le32(image, 0x0Cu, UINT32_C(0xE59F2050));
    gbl_test_write_le32(image, 0x10u, UINT32_C(0xE3A04008));
    gbl_test_write_le32(image, 0x14u, UINT32_C(0xE1D230B0));
    gbl_test_write_le32(image, 0x18u, UINT32_C(0xE1130004));
    gbl_test_write_le32(image, 0x1Cu, UINT32_C(0x1A000000));
    gbl_test_write_le32(image, 0x20u, UINT32_C(0xEAFFFFFB));
    gbl_test_write_le32(image, 0x24u, UINT32_C(0xE59F0028));
    gbl_test_write_le32(image, 0x28u, UINT32_C(0xE59F1028));
    gbl_test_write_le32(image, 0x2Cu, UINT32_C(0xE5801000));
    gbl_test_write_le32(image, 0x30u, UINT32_C(0xE3A01001));
    gbl_test_write_le32(image, 0x34u, UINT32_C(0xE5801004));
    gbl_test_write_le32(image, 0x38u, UINT32_C(0xE3A01002));
    gbl_test_write_le32(image, 0x3Cu, UINT32_C(0xE5801008));
    gbl_test_write_le32(image, 0x40u, UINT32_C(0xE3A01001));
    gbl_test_write_le32(image, 0x44u, UINT32_C(0xE580100C));
    gbl_test_write_le32(image, 0x48u, UINT32_C(0xE59F1018));
    gbl_test_write_le32(image, 0x4Cu, UINT32_C(0xE5801010));
    gbl_test_write_le32(image, 0x50u, UINT32_C(0xEAFFFFFE));
    gbl_test_write_le32(image, 0x54u, UINT32_C(0x03007FC0));
    gbl_test_write_le32(image, 0x58u, UINT32_C(0x47424C4B));
    gbl_test_write_le32(image, 0x5Cu, UINT32_C(0x04000100));
    gbl_test_write_le32(image, 0x60u,
                        enabled ? UINT32_C(0x00C0FFF0)
                                : UINT32_C(0x0000FFF0));
    gbl_test_write_le32(image, 0x64u, UINT32_C(0x04000202));
    gbl_test_write_le32(image, 0x68u, UINT32_C(0x434F4D50));

    file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    okay = fwrite(image, 1u, sizeof image, file) == sizeof image;
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay;
}

static bool gbl_test_serialize_result(const gbl_runner_result *result,
                                      char *json, size_t capacity)
{
    FILE *stream;
    size_t length;
    bool okay;

    if (result == NULL || json == NULL || capacity < 2u) {
        return false;
    }
    stream = tmpfile();
    if (stream == NULL) {
        return false;
    }
    okay = gbl_runner_write_json(stream, result) && fflush(stream) == 0 &&
           fseek(stream, 0L, SEEK_SET) == 0 &&
           (length = fread(json, 1u, capacity - 1u, stream)) > 0u;
    if (okay) {
        json[length] = '\0';
    }
    (void)fclose(stream);
    return okay;
}

static bool gbl_test_json_u64(const char *json, const char *member,
                              uint64_t *value)
{
    char needle[64];
    const char *digits;
    char *end;
    unsigned long long parsed;
    int written;

    if (json == NULL || member == NULL || value == NULL) {
        return false;
    }
    written = snprintf(needle, sizeof needle, "\"%s\":\"", member);
    if (written < 0 || (size_t)written >= sizeof needle) {
        return false;
    }
    digits = strstr(json, needle);
    if (digits == NULL) {
        return false;
    }
    digits += (size_t)written;
    if (digits[0] < '0' || digits[0] > '9' ||
        (digits[0] == '0' && digits[1] != '\"')) {
        return false;
    }
    errno = 0;
    parsed = strtoull(digits, &end, 10);
    if (errno != 0 || end == digits || *end != '\"' ||
        parsed > (unsigned long long)UINT64_MAX) {
        return false;
    }
    *value = (uint64_t)parsed;
    return true;
}

static bool gbl_test_timer0_runner_observe(
    const char *scratch_directory, gbl_test_timer0_observations *observed)
{
    char enabled_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char disabled_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char empty_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char json[8192];
    gbl_runner_result result;
    gbl_runner_status status;
    uint64_t cycles;
    uint64_t bus_cycles;
    uint64_t internal_cycles;
    bool okay = true;

    if (observed == NULL ||
        !gbl_test_path(enabled_path, sizeof enabled_path, scratch_directory,
                       "runner-timer0-enabled.gba") ||
        !gbl_test_path(disabled_path, sizeof disabled_path, scratch_directory,
                       "runner-timer0-disabled.gba") ||
        !gbl_test_path(empty_path, sizeof empty_path, scratch_directory,
                       "runner-timer0-empty.gba") ||
        !gbl_test_write_timer0_guest(enabled_path, true) ||
        !gbl_test_write_timer0_guest(disabled_path, false) ||
        !gbl_test_write_file(empty_path, 0u, 0u)) {
        return false;
    }
    *observed = (gbl_test_timer0_observations){ 0 };

    memset(&result, 0, sizeof result);
    status = gbl_test_execute(enabled_path, NULL, UINT64_C(10000), 0u, 0u,
                              NULL, NULL, NULL,
                              (gbl_runner_allocator){ 0 }, &result);
    observed->semantic_completed = status == GBL_RUNNER_GUEST_STOP &&
        result.status == GBL_RUNNER_GUEST_STOP && result.ran &&
        result.semantic.status == GBL_MAILBOX_PASS &&
        result.semantic.case_id == UINT32_C(1) && result.cycles > 0u &&
        strcmp(result.reason, "unregistered-content") == 0;
    okay = gbl_test_serialize_result(&result, json, sizeof json);
    observed->request_observed = okay &&
        strstr(json, "\"timer0\":{\"if_request\":true}") != NULL;
    observed->model_contract = okay &&
        strstr(json, "\"model\":\"" GBL_TIMER_MODEL_NAME "\"") != NULL &&
        strstr(json, "\"qualification\":\"project-model-only\"") != NULL;
    observed->component_sum = okay &&
        gbl_test_json_u64(json, "cycles", &cycles) &&
        gbl_test_json_u64(json, "bus_cycles", &bus_cycles) &&
        gbl_test_json_u64(json, "internal_cycles", &internal_cycles) &&
        bus_cycles <= UINT64_MAX - internal_cycles &&
        cycles == bus_cycles + internal_cycles;
    gbl_runner_result_dispose(&result);

    memset(&result, 0, sizeof result);
    status = gbl_test_execute(disabled_path, NULL, UINT64_C(10000), 0u, 0u,
                              NULL, NULL, NULL,
                              (gbl_runner_allocator){ 0 }, &result);
    okay = okay && gbl_test_serialize_result(&result, json, sizeof json);
    observed->disabled_timeout = status == GBL_RUNNER_TIMEOUT &&
        result.status == GBL_RUNNER_TIMEOUT && result.ran &&
        result.semantic.status != GBL_MAILBOX_PASS &&
        strcmp(result.reason, "cycle-budget") == 0 &&
        strstr(json, "\"timer0\":{\"if_request\":false}") != NULL;
    gbl_runner_result_dispose(&result);

    memset(&result, 0, sizeof result);
    status = gbl_test_execute(enabled_path, NULL, 0u, 0u, 0u, NULL, NULL,
                              NULL, (gbl_runner_allocator){ 0 }, &result);
    okay = okay && gbl_test_serialize_result(&result, json, sizeof json);
    observed->not_run_request_null = status == GBL_RUNNER_TIMEOUT &&
        result.status == GBL_RUNNER_TIMEOUT && result.ran &&
        result.cycles == 0u &&
        strstr(json, "\"timer0\":{\"if_request\":null}") != NULL;
    gbl_runner_result_dispose(&result);

    memset(&result, 0, sizeof result);
    status = gbl_test_execute(empty_path, NULL, UINT64_C(100), 0u, 0u, NULL,
                              NULL, NULL, (gbl_runner_allocator){ 0 },
                              &result);
    okay = okay && gbl_test_serialize_result(&result, json, sizeof json);
    observed->not_run_request_null = observed->not_run_request_null &&
        status == GBL_RUNNER_REFUSED && result.status == GBL_RUNNER_REFUSED &&
        !result.ran && strstr(json, "\"timer0\":{\"if_request\":null}") !=
                           NULL;
    gbl_runner_result_dispose(&result);
    return okay;
}

static bool gbl_test_register_result_is(const gbl_runner_result *result,
                                        gbl_runner_status expected_status,
                                        uint32_t expected_value)
{
    return result->status == expected_status && result->ran &&
           result->register_observation.present &&
           result->register_observation.observed &&
           result->register_observation.has_value &&
           result->register_observation.value == expected_value &&
           strcmp(result->termination, "register-eval-complete") == 0 &&
           result->semantic.status == GBL_MAILBOX_ABSENT;
}

static bool gbl_test_machine_state_equal(const gbl_machine *left,
                                         const gbl_machine *right)
{
    return left->master_cycles == right->master_cycles &&
           left->open_bus == right->open_bus &&
           left->dispcnt == right->dispcnt &&
           left->initialized == right->initialized &&
           memcmp(&left->header_diagnostics, &right->header_diagnostics,
                  sizeof left->header_diagnostics) == 0 &&
           memcmp(&left->cpu, &right->cpu, sizeof left->cpu) == 0 &&
           memcmp(left->ewram, right->ewram, sizeof left->ewram) == 0 &&
           memcmp(left->iwram, right->iwram, sizeof left->iwram) == 0 &&
           memcmp(left->pram, right->pram, sizeof left->pram) == 0 &&
           memcmp(left->vram, right->vram, sizeof left->vram) == 0 &&
           memcmp(left->oam, right->oam, sizeof left->oam) == 0;
}

static bool gbl_test_register_fixture(
    const char *fixture_directory, const char *rom_name,
    const char *fixture_id, gbl_runner_status expected_status,
    uint32_t expected_value)
{
    char path[GBL_RUNNER_TEST_PATH_CAPACITY];
    gbl_runner_result discovered;
    gbl_runner_result exact;
    gbl_runner_result one_short;
    gbl_runner_result baseline;
    gbl_test_allocator_state exact_allocator_state = { 0 };
    gbl_test_allocator_state baseline_allocator_state = { 0 };
    uint64_t boundary_cycles;
    bool okay;

    if (!gbl_test_path(path, sizeof path, fixture_directory, rom_name)) {
        return false;
    }
    memset(&discovered, 0, sizeof discovered);
    if (gbl_test_execute(path, fixture_id, UINT64_C(100), 0u, 0u, NULL,
                        NULL, NULL, (gbl_runner_allocator){ 0 },
                        &discovered) != expected_status ||
        !gbl_test_register_result_is(&discovered, expected_status,
                                     expected_value)) {
        gbl_runner_result_dispose(&discovered);
        return false;
    }
    boundary_cycles = discovered.cycles;
    gbl_runner_result_dispose(&discovered);
    if (boundary_cycles == 0u) {
        return false;
    }

    exact_allocator_state.machine_snapshot =
        calloc(1u, sizeof *exact_allocator_state.machine_snapshot);
    baseline_allocator_state.machine_snapshot =
        calloc(1u, sizeof *baseline_allocator_state.machine_snapshot);
    if (exact_allocator_state.machine_snapshot == NULL ||
        baseline_allocator_state.machine_snapshot == NULL) {
        free(exact_allocator_state.machine_snapshot);
        free(baseline_allocator_state.machine_snapshot);
        return false;
    }
    memset(&exact, 0, sizeof exact);
    okay = gbl_test_execute(
               path, fixture_id, boundary_cycles, 0u, 0u, NULL, NULL, NULL,
               (gbl_runner_allocator){ &exact_allocator_state,
                                       gbl_test_allocate, gbl_test_release },
               &exact) == expected_status &&
           gbl_test_register_result_is(&exact, expected_status,
                                       expected_value) &&
           exact.cycles == boundary_cycles &&
           exact.budget_cycles == boundary_cycles &&
           exact_allocator_state.machine_snapshot_available;
    gbl_runner_result_dispose(&exact);

    memset(&baseline, 0, sizeof baseline);
    okay = okay &&
           gbl_test_execute(
               path, NULL, boundary_cycles, 0u, 0u, NULL, NULL, NULL,
               (gbl_runner_allocator){ &baseline_allocator_state,
                                       gbl_test_allocate, gbl_test_release },
               &baseline) == GBL_RUNNER_TIMEOUT &&
           baseline.status == GBL_RUNNER_TIMEOUT &&
           baseline.cycles == boundary_cycles &&
           baseline.semantic.status == GBL_MAILBOX_ABSENT &&
           baseline_allocator_state.machine_snapshot_available &&
           gbl_test_machine_state_equal(
               exact_allocator_state.machine_snapshot,
               baseline_allocator_state.machine_snapshot);
    gbl_runner_result_dispose(&baseline);

    memset(&one_short, 0, sizeof one_short);
    okay = okay && boundary_cycles > 1u &&
           gbl_test_execute(path, fixture_id, boundary_cycles - 1u, 0u, 0u,
                            NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &one_short) ==
               GBL_RUNNER_TIMEOUT &&
           one_short.status == GBL_RUNNER_TIMEOUT &&
           !one_short.register_observation.observed &&
           !one_short.register_observation.has_value &&
           one_short.cycles <= one_short.budget_cycles &&
           one_short.semantic.status == GBL_MAILBOX_ABSENT;
    gbl_runner_result_dispose(&one_short);
    free(exact_allocator_state.machine_snapshot);
    free(baseline_allocator_state.machine_snapshot);
    return okay;
}

static bool gbl_test_register_rejection_controls(
    const char *fixture_directory)
{
    char path[GBL_RUNNER_TEST_PATH_CAPACITY];
    gbl_runner_result result;
    bool okay = true;

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-arm-pass.gba")) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, "synthetic-control-register-arm",
                            UINT64_C(100), 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_REFUSED &&
           strcmp(result.reason, "fixture-result-entry-out-of-range") == 0 &&
           !result.ran && !result.register_observation.observed;
    gbl_runner_result_dispose(&result);

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-register-arm-zero.gba")) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = okay &&
           gbl_test_execute(path, "synthetic-control-register-arm",
                            UINT64_C(100), 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_REFUSED &&
           strcmp(result.reason, "fixture-digest-mismatch") == 0 &&
           !result.register_observation.observed;
    gbl_runner_result_dispose(&result);

    memset(&result, 0, sizeof result);
    okay = okay &&
           gbl_test_execute(path,
                            "synthetic-control-register-thumb-wrong-state",
                            UINT64_C(100), 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_TIMEOUT &&
           result.status == GBL_RUNNER_TIMEOUT &&
           !result.register_observation.observed &&
           !result.register_observation.has_value;
    gbl_runner_result_dispose(&result);

    memset(&result, 0, sizeof result);
    okay = okay &&
           gbl_test_execute(path,
                            "synthetic-control-register-arm-never-entry",
                            UINT64_C(100), 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_TIMEOUT &&
           result.status == GBL_RUNNER_TIMEOUT &&
           !result.register_observation.observed &&
           !result.register_observation.has_value;
    gbl_runner_result_dispose(&result);
    return okay;
}

static bool gbl_test_register_terminal_precedence(
    const char *fixture_directory)
{
    char path[GBL_RUNNER_TEST_PATH_CAPACITY];
    gbl_runner_result result;
    gbl_test_cancel_state cancel_state = { 0u, 5u };
    gbl_test_allocator_state allocator_state = { 0 };
    gbl_runner_allocator allocator;
    bool okay;

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-register-arm-stop.gba")) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, "synthetic-control-register-arm-stop",
                            UINT64_C(100), 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_GUEST_STOP &&
           result.status == GBL_RUNNER_GUEST_STOP &&
           !result.register_observation.observed;
    gbl_runner_result_dispose(&result);

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-register-arm-zero.gba")) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = okay &&
           gbl_test_execute(path, "synthetic-control-register-arm-zero",
                            UINT64_C(100), 0u, 0u, NULL,
                            gbl_test_cancel_at_boundary, &cancel_state,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_CANCELLED &&
           result.status == GBL_RUNNER_CANCELLED && result.cycles > 0u &&
           !result.register_observation.observed;
    gbl_runner_result_dispose(&result);

    allocator_state.fail_at = 2u;
    allocator = (gbl_runner_allocator){ &allocator_state, gbl_test_allocate,
                                        gbl_test_release };
    memset(&result, 0, sizeof result);
    okay = okay &&
           gbl_test_execute(path, "synthetic-control-register-arm-zero",
                            UINT64_C(100), 0u, 0u, NULL, NULL, NULL,
                            allocator, &result) == GBL_RUNNER_HARNESS_FAIL &&
           result.status == GBL_RUNNER_HARNESS_FAIL && !result.ran &&
           !result.register_observation.observed &&
           !result.register_observation.has_value;
    gbl_runner_result_dispose(&result);

    memset(&result, 0, sizeof result);
    okay = okay &&
           gbl_test_execute(path, "synthetic-control-register-arm-zero",
                            UINT64_C(100), UINT32_C(1), 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_HARNESS_FAIL &&
           result.status == GBL_RUNNER_HARNESS_FAIL &&
           !result.trace_complete &&
           !result.register_observation.observed;
    gbl_runner_result_dispose(&result);
    return okay;
}

static bool gbl_test_input_timeline_observations(
    const char *fixture_directory)
{
    char pass_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char loop_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char first_digest[GBL_SHA256_HEX_CAPACITY];
    char repeated_digest[GBL_SHA256_HEX_CAPACITY];
    char released_digest[GBL_SHA256_HEX_CAPACITY];
    char empty_digest[GBL_SHA256_HEX_CAPACITY];
    const gbl_input_event pressed = { UINT64_C(0), UINT16_C(1) };
    const gbl_input_event released = { UINT64_C(0), UINT16_C(0) };
    gbl_runner_result result = { 0 };
    gbl_runner_status status;
    bool okay;

    if (!gbl_test_path(pass_path, sizeof pass_path, fixture_directory,
                       "owned-arm-pass.gba") ||
        !gbl_test_path(loop_path, sizeof loop_path, fixture_directory,
                       "owned-arm-loop.gba")) {
        return false;
    }

    status = gbl_test_execute_timeline(
        pass_path, "synthetic-control-pass", UINT64_C(1000), true,
        &pressed, 1u, NULL, NULL, (gbl_runner_allocator){ 0 }, &result);
    okay = status == GBL_RUNNER_PASS && result.phase8.observed &&
           result.phase8.keyinput == UINT16_C(0x03FE) &&
           result.phase8.applied_input_events == 1u &&
           result.phase8.pending_input_events == 0u &&
           result.input_timeline_present &&
           result.has_input_timeline_sha256;
    if (okay) {
        memcpy(first_digest, result.input_timeline_sha256,
               sizeof first_digest);
        okay = strcmp(first_digest,
                      "c2cd3f19cdb7b22c37e53a6654b45c8a56f5dfb81040d984385e6e4ef2cdd328") == 0;
    }
    gbl_runner_result_dispose(&result);
    if (!okay) {
        return false;
    }

    status = gbl_test_execute_timeline(
        loop_path, NULL, UINT64_C(100), true, &pressed, 1u, NULL, NULL,
        (gbl_runner_allocator){ 0 }, &result);
    okay = status != GBL_RUNNER_MISSING && result.phase8.observed &&
           result.has_input_timeline_sha256 &&
           strcmp(first_digest, result.input_timeline_sha256) == 0;
    if (okay) {
        memcpy(repeated_digest, result.input_timeline_sha256,
               sizeof repeated_digest);
    }
    gbl_runner_result_dispose(&result);
    if (!okay) {
        return false;
    }

    status = gbl_test_execute_timeline(
        pass_path, "synthetic-control-pass", UINT64_C(1000), true,
        &released, 1u, NULL, NULL, (gbl_runner_allocator){ 0 }, &result);
    okay = status == GBL_RUNNER_PASS && result.phase8.observed &&
           result.phase8.keyinput == UINT16_C(0x03FF) &&
           result.has_input_timeline_sha256 &&
           strcmp(first_digest, result.input_timeline_sha256) != 0;
    if (okay) {
        memcpy(released_digest, result.input_timeline_sha256,
               sizeof released_digest);
    }
    gbl_runner_result_dispose(&result);
    if (!okay) {
        return false;
    }

    status = gbl_test_execute_timeline(
        pass_path, "synthetic-control-pass", UINT64_C(1000), false, NULL,
        0u, NULL, NULL, (gbl_runner_allocator){ 0 }, &result);
    okay = status == GBL_RUNNER_PASS && result.phase8.observed &&
           result.phase8.keyinput == UINT16_C(0x03FF) &&
           !result.input_timeline_present &&
           !result.has_input_timeline_sha256;
    gbl_runner_result_dispose(&result);
    if (!okay) {
        return false;
    }

    status = gbl_test_execute_timeline(
        pass_path, "synthetic-control-pass", UINT64_C(1000), true, NULL, 0u,
        NULL, NULL, (gbl_runner_allocator){ 0 }, &result);
    okay = status == GBL_RUNNER_PASS && result.phase8.observed &&
           result.phase8.keyinput == UINT16_C(0x03FF) &&
           result.input_timeline_present &&
           result.has_input_timeline_sha256 &&
           strcmp(released_digest, result.input_timeline_sha256) != 0 &&
           strcmp(first_digest, result.input_timeline_sha256) != 0;
    if (okay) {
        memcpy(empty_digest, result.input_timeline_sha256,
               sizeof empty_digest);
    }
    gbl_runner_result_dispose(&result);
    return okay && strcmp(repeated_digest, first_digest) == 0 &&
           strcmp(empty_digest, first_digest) != 0;
}

static bool gbl_test_input_timeline_preflight(void)
{
    const gbl_input_event reversed[2] = {
        { UINT64_C(5), UINT16_C(0) },
        { UINT64_C(4), UINT16_C(0) }
    };
    gbl_test_allocator_state allocator_state = { 0 };
    const gbl_runner_allocator allocator = {
        &allocator_state, gbl_test_allocate, gbl_test_release
    };
    gbl_runner_result result = { 0 };
    gbl_runner_status status;
    bool okay;

    status = gbl_test_execute_timeline(
        "/path/not-read/invalid-timeline.gba", NULL, UINT64_C(100), true,
        reversed, 2u, NULL, NULL, allocator, &result);
    okay = status == GBL_RUNNER_REFUSED && !result.ran &&
           result.has_refusal &&
           strcmp(result.refusal_code, "invalid-input-timeline") == 0 &&
           allocator_state.calls == 0u;
    gbl_runner_result_dispose(&result);
    if (!okay) {
        return false;
    }

    memset(&allocator_state, 0, sizeof allocator_state);
    status = gbl_test_execute_timeline(
        "/path/not-read/absent-with-pointer.gba", NULL, UINT64_C(100), false,
        reversed, 1u, NULL, NULL, allocator, &result);
    okay = status == GBL_RUNNER_REFUSED && !result.ran &&
           result.has_refusal &&
           strcmp(result.refusal_code, "invalid-input-timeline") == 0 &&
           allocator_state.calls == 0u;
    gbl_runner_result_dispose(&result);
    return okay;
}

static bool gbl_test_input_timeline_cancellation(
    const char *fixture_directory)
{
    char path[GBL_RUNNER_TEST_PATH_CAPACITY];
    const gbl_input_event pressed = { UINT64_C(0), UINT16_C(1) };
    gbl_test_cancel_state cancel_state = { 0u, 2u };
    gbl_runner_result result = { 0 };
    bool okay;

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-arm-pass.gba")) {
        return false;
    }
    okay = gbl_test_execute_timeline(
               path, "synthetic-control-pass", UINT64_C(1000), true,
               &pressed, 1u, gbl_test_cancel_at_boundary, &cancel_state,
               (gbl_runner_allocator){ 0 }, &result) == GBL_RUNNER_CANCELLED &&
           result.status == GBL_RUNNER_CANCELLED && result.phase8.observed &&
           result.phase8.keyinput == UINT16_C(0x03FE) &&
           result.phase8.applied_input_events == 1u;
    gbl_runner_result_dispose(&result);
    return okay;
}

static bool gbl_test_read_file(const char *path, char *output, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    size_t length;
    int read_error;
    int close_error;

    if (file == NULL || capacity == 0u) {
        if (file != NULL) {
            (void)fclose(file);
        }
        return false;
    }
    length = fread(output, 1u, capacity - 1u, file);
    read_error = ferror(file);
    close_error = fclose(file);
    if (read_error != 0 || close_error != 0) {
        return false;
    }
    output[length] = '\0';
    return true;
}

static bool gbl_test_cli_cancel_budget(const char *runner_path,
                                       const char *loop_path,
                                       const char *directory,
                                       const char *budget_option,
                                       const char *budget_value,
                                       const char *expected_budget,
                                       const char *expected_status,
                                       const char *expected_reason,
                                       const char *trace_output_path)
{
    char output_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char error_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char expected_budget_json[128];
    char output[8192];
    struct timespec startup_wait = { 0, 250000000L };
    struct timespec poll_wait = { 0, 10000000L };
    pid_t child;
    int status = 0;
    int budget_text_length;
    size_t tick;
    bool exited;
    int out_fd;
    int err_fd;

    if (!gbl_test_path(output_path, sizeof output_path, directory,
                       "signal-cancel.json") ||
        !gbl_test_path(error_path, sizeof error_path, directory,
                       "signal-cancel.stderr")) {
        return false;
    }
    child = fork();
    if (child < 0) {
        return false;
    }
    if (child == 0) {
        out_fd = open(output_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        err_fd = open(error_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (out_fd < 0 || err_fd < 0 || dup2(out_fd, STDOUT_FILENO) < 0 ||
            dup2(err_fd, STDERR_FILENO) < 0) {
            _exit(126);
        }
        (void)close(out_fd);
        (void)close(err_fd);
        /* exec preserves ignored signals. A cold executable may still be
         * starting when the first probe arrives; only the CLI's installed
         * handler should turn this test's SIGINT into cancellation. */
        (void)signal(SIGINT, SIG_IGN);
        if (trace_output_path == NULL) {
            execl(runner_path, "gamebloke-run-control", "--rom", loop_path,
                  budget_option, budget_value, (char *)NULL);
        } else {
            execl(runner_path, "gamebloke-run-control", "--rom", loop_path,
                  budget_option, budget_value, "--trace-mask", "0",
                  "--trace-capacity", "0", "--trace-output",
                  trace_output_path, (char *)NULL);
        }
        _exit(127);
    }

    budget_text_length =
        snprintf(expected_budget_json, sizeof expected_budget_json,
                 "\"budget_cycles\":\"%s\"", expected_budget);
    if (budget_text_length < 0 ||
        (size_t)budget_text_length >= sizeof expected_budget_json) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, &status, 0);
        return false;
    }

    /* Give the real CLI time to install its handler and enter its bounded loop. */
    (void)nanosleep(&startup_wait, NULL);
    exited = waitpid(child, &status, WNOHANG) == child;
    if (exited) {
        return false;
    }
    if (kill(child, SIGINT) != 0) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, &status, 0);
        return false;
    }
    for (tick = 0u; tick < GBL_RUNNER_TEST_WATCHDOG_TICKS; ++tick) {
        const pid_t waited = waitpid(child, &status, WNOHANG);

        if (waited == child) {
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 1 ||
                !gbl_test_read_file(output_path, output, sizeof output)) {
                return false;
            }
            char status_fragment[96];
            char reason_fragment[128];
            const int status_length =
                snprintf(status_fragment, sizeof status_fragment,
                         "\"status\":\"%s\"", expected_status);
            const int reason_length =
                snprintf(reason_fragment, sizeof reason_fragment,
                         "\"reason\":\"%s\"", expected_reason);

            return status_length > 0 &&
                   (size_t)status_length < sizeof status_fragment &&
                   reason_length > 0 &&
                   (size_t)reason_length < sizeof reason_fragment &&
                   strstr(output, status_fragment) != NULL &&
                   strstr(output, reason_fragment) != NULL &&
                   strstr(output, expected_budget_json) != NULL;
        }
        if (waited < 0 && errno != EINTR) {
            (void)kill(child, SIGKILL);
            (void)waitpid(child, &status, 0);
            return false;
        }
        if (tick % 10u == 0u) {
            (void)kill(child, SIGINT);
        }
        (void)nanosleep(&poll_wait, NULL);
    }
    (void)kill(child, SIGKILL);
    (void)waitpid(child, &status, 0);
    return false;
}

static bool gbl_test_cli_cancel(const char *runner_path,
                                const char *loop_path,
                                const char *directory)
{
    char trace_path[GBL_RUNNER_TEST_PATH_CAPACITY];

    if (!gbl_test_path(trace_path, sizeof trace_path, directory,
                       "signal-trace-existing.jsonl") ||
        !gbl_test_write_file(trace_path, 0u, 0u)) {
        return false;
    }
    return gbl_test_cli_cancel_budget(
               runner_path, loop_path, directory, "--cycles",
               "18446744073709551615", "18446744073709551615", "CANCELLED",
               "cancelled", NULL) &&
           gbl_test_cli_cancel_budget(
               runner_path, loop_path, directory, "--frames",
               "65671081374279", "18446744073709473984", "CANCELLED",
               "cancelled", NULL) &&
           gbl_test_cli_cancel_budget(
               runner_path, loop_path, directory, "--cycles",
               "18446744073709551615", "18446744073709551615", "HARNESS_FAIL",
               "trace-write-failed", trace_path);
}

static bool gbl_test_cli_trace_write_limit(const char *runner_path,
                                           const char *pass_path,
                                           const char *directory)
{
    char trace_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char output[16384];
    struct rlimit limit;
    struct timespec poll_wait = { 0, 10000000L };
    pid_t child;
    int pipe_fds[2];
    int status = 0;
    size_t tick;
    size_t output_length = 0u;
    bool finished = false;

    if (!gbl_test_path(trace_path, sizeof trace_path, directory,
                       "trace-write-limit.jsonl")) {
        return false;
    }
    (void)remove(trace_path);
    if (pipe(pipe_fds) != 0) {
        return false;
    }
    child = fork();
    if (child < 0) {
        (void)close(pipe_fds[0]);
        (void)close(pipe_fds[1]);
        return false;
    }
    if (child == 0) {
        (void)close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) {
            _exit(126);
        }
        (void)close(pipe_fds[1]);
        (void)signal(SIGXFSZ, SIG_IGN);
        limit.rlim_cur = (rlim_t)1u;
        limit.rlim_max = (rlim_t)1u;
        if (setrlimit(RLIMIT_FSIZE, &limit) != 0) {
            _exit(126);
        }
        execl(runner_path, "gamebloke-run-control", "--rom", pass_path,
              "--fixture", "synthetic-control-pass", "--cycles", "1000",
              "--trace-mask", "1", "--trace-capacity", "65536",
              "--trace-output", trace_path, (char *)NULL);
        _exit(127);
    }
    (void)close(pipe_fds[1]);
    for (tick = 0u; tick < GBL_RUNNER_TEST_WATCHDOG_TICKS; ++tick) {
        const pid_t waited = waitpid(child, &status, WNOHANG);

        if (waited == child) {
            finished = true;
            break;
        }
        if (waited < 0 && errno != EINTR) {
            (void)kill(child, SIGKILL);
            (void)waitpid(child, &status, 0);
            (void)close(pipe_fds[0]);
            return false;
        }
        (void)nanosleep(&poll_wait, NULL);
    }
    if (!finished) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, &status, 0);
        (void)close(pipe_fds[0]);
        return false;
    }
    for (;;) {
        const ssize_t amount = read(pipe_fds[0], output + output_length,
                                    sizeof output - 1u - output_length);

        if (amount == 0) {
            break;
        }
        if (amount < 0) {
            if (errno == EINTR) {
                continue;
            }
            (void)close(pipe_fds[0]);
            return false;
        }
        output_length += (size_t)amount;
        if (output_length >= sizeof output - 1u) {
            (void)close(pipe_fds[0]);
            return false;
        }
    }
    (void)close(pipe_fds[0]);
    output[output_length] = '\0';
    return WIFEXITED(status) && WEXITSTATUS(status) == 1 &&
           strstr(output, "\"status\":\"HARNESS_FAIL\"") != NULL &&
           strstr(output, "\"reason\":\"trace-write-failed\"") != NULL &&
           strstr(output, "\"state\":\"pass\"") != NULL;
}

static bool gbl_test_registered_result(const char *path, const char *fixture,
                                       gbl_runner_status expected,
                                       gbl_mailbox_status semantic)
{
    gbl_runner_result result;
    const gbl_runner_status status = gbl_test_execute(
        path, fixture, UINT64_MAX, 0u, 0u, NULL, NULL, NULL,
        (gbl_runner_allocator){ 0 }, &result);
    bool okay = status == expected && result.status == expected &&
                result.semantic.status == semantic && result.ran;

    if (okay && expected == GBL_RUNNER_PASS) {
        gbl_pass_cycles = result.cycles;
        gbl_pass_instructions = result.instructions;
    }
    gbl_runner_result_dispose(&result);
    return okay;
}

static bool gbl_test_execute_all(const char *fixture_directory,
                                 const char *scratch_directory,
                                 const char *runner_path)
{
    char path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char scratch[GBL_RUNNER_TEST_PATH_CAPACITY];
    char trace_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char existing_trace_path[GBL_RUNNER_TEST_PATH_CAPACITY];
    char json[8192];
    struct stat info;
    gbl_runner_result result;
    gbl_test_allocator_state allocator_state;
    gbl_test_cancel_state cancel_state;
    gbl_test_timer0_observations timer0_observations = { 0 };
    gbl_runner_allocator allocator;
    FILE *stream;
    size_t json_length;
    bool okay;

    if (mkdir(scratch_directory, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    if (stat(scratch_directory, &info) != 0 || !S_ISDIR(info.st_mode)) {
        return false;
    }
    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-arm-loop.gba")) {
        return false;
    }

    gbl_test_case(
        gbl_test_guest_stop_json_matrix(),
        "IRQ, STOP, and deadlock share distinct production GUEST_STOP reasons");
    gbl_test_case(
        gbl_test_input_timeline_observations(fixture_directory),
        "timeline identity is canonical and cycle-zero A state differs from release and absence");
    gbl_test_case(gbl_test_input_timeline_preflight(),
                  "malformed and inconsistent timeline options refuse before allocation or file access");
    gbl_test_case(
        gbl_test_input_timeline_cancellation(fixture_directory),
        "cancellation retains the committed logical input observation");
    gbl_test_case(gbl_test_instruction_stop_json(scratch_directory),
        "executed instruction causes reach the shared real JSON writer with claim and time");
    gbl_test_case(gbl_test_cli_cancel(runner_path, path, scratch_directory),
                  "CLI converts SIGINT to CANCELLED before watchdog expiry");

    okay = gbl_test_timer0_runner_observe(scratch_directory,
                                          &timer0_observations);
    gbl_test_case(okay && timer0_observations.semantic_completed,
                  "timer0-runner-semantic: overflow mailbox is observed while raw input remains GUEST_STOP");
    gbl_test_case(okay && timer0_observations.request_observed,
                  "timer0-runner-request: Timer 0 IF request is independently true");
    gbl_test_case(okay && timer0_observations.model_contract,
                  "timer0-runner-model: time names the selected model and project-model-only qualification");
    gbl_test_case(okay && timer0_observations.component_sum,
                  "timer0-runner-components: bus and internal cycles equal total exactly");
    gbl_test_case(okay && timer0_observations.disabled_timeout,
                  "timer0-overflow-timeout: disabled timer cannot complete semantic mailbox PASS");
    gbl_test_case(okay && timer0_observations.not_run_request_null,
                  "timer0-not-run: zero-budget and pre-read refusal requests remain null");

    gbl_test_case(gbl_test_runner_dma_observation(scratch_directory),
                  "runner-dma-production: guest mailbox follows a real DMA transfer and result exposes phase9");

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-arm-pass.gba")) {
        return false;
    }

    okay = gbl_test_registered_result(path, "synthetic-control-pass",
                                      GBL_RUNNER_PASS, GBL_MAILBOX_PASS) &&
           gbl_pass_cycles != 0u && gbl_pass_instructions != 0u;
    gbl_test_case(okay, "registered synthetic mailbox pass remains PASS");

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-arm-fail.gba")) {
        return false;
    }
    gbl_test_case(gbl_test_registered_result(
                      path, "synthetic-control-fail", GBL_RUNNER_GUEST_FAIL,
                      GBL_MAILBOX_FAIL),
                  "registered synthetic mailbox failure remains GUEST_FAIL");

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-arm-pass.gba")) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path,
                            "synthetic-control-register-mailbox-pass",
                            UINT64_C(1000), 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_TIMEOUT &&
           result.status == GBL_RUNNER_TIMEOUT &&
           result.semantic.status == GBL_MAILBOX_PASS;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay,
                  "register channel cannot use a mailbox PASS away from eval");

    okay = gbl_test_register_fixture(
               fixture_directory, "owned-register-arm-zero.gba",
               "synthetic-control-register-arm-zero", GBL_RUNNER_PASS, 0u) &&
           gbl_test_register_fixture(
               fixture_directory, "owned-register-arm-fail.gba",
               "synthetic-control-register-arm-fail",
               GBL_RUNNER_GUEST_FAIL, 1u);
    gbl_test_case(okay,
                  "ARM/R12 observes zero and nonzero only at exact eval entry");

    okay = gbl_test_register_fixture(
               fixture_directory, "owned-register-thumb-zero.gba",
               "synthetic-control-register-thumb-zero", GBL_RUNNER_PASS,
               0u) &&
           gbl_test_register_fixture(
               fixture_directory, "owned-register-thumb-fail.gba",
               "synthetic-control-register-thumb-fail",
               GBL_RUNNER_GUEST_FAIL, 1u);
    gbl_test_case(okay,
                  "Thumb/R7 observes zero and nonzero in Thumb pipeline state");

    gbl_test_case(gbl_test_register_rejection_controls(fixture_directory),
                  "digest, extent, state, and unreached-entry controls cannot PASS");
    gbl_test_case(gbl_test_register_terminal_precedence(fixture_directory),
                  "stop, cancellation, and trace failure precede register PASS");

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-arm-pass.gba")) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, "synthetic-control-bios-required",
                            UINT64_C(1000), 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_SKIPPED &&
           result.status == GBL_RUNNER_SKIPPED && !result.ran &&
           result.cycles == 0u &&
           strcmp(result.reason, "required-bios-unavailable") == 0;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "required BIOS metadata yields SKIPPED before execution");

    if (!gbl_test_path(scratch, sizeof scratch, scratch_directory,
                       "official-bios-not-present.gba")) {
        return false;
    }
    (void)remove(scratch);
    memset(&allocator_state, 0, sizeof allocator_state);
    allocator = (gbl_runner_allocator){ &allocator_state, gbl_test_allocate,
                                        gbl_test_release };
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(
               scratch, "synthetic-control-official-bios", UINT64_C(100),
               0u, 0u, NULL, NULL, NULL, allocator, &result) ==
               GBL_RUNNER_SKIPPED &&
           result.status == GBL_RUNNER_SKIPPED && !result.ran &&
           strcmp(result.reason, "official-bios-required") == 0 &&
           strcmp(result.termination, "skipped-before-run") == 0 &&
           result.has_fixture_id &&
           !result.has_rom_sha256 && !result.has_expected_sha256 &&
           result.semantic.status == GBL_MAILBOX_ABSENT &&
           result.cycles == 0u && allocator_state.calls == 0u;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay,
                  "official BIOS row skips before opening absent content or claiming a digest");

    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, NULL, UINT64_C(1000), 0u, 0u, NULL, NULL,
                            NULL, (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_GUEST_STOP &&
           result.status == GBL_RUNNER_GUEST_STOP &&
           result.semantic.status == GBL_MAILBOX_PASS &&
           strcmp(result.reason, "unregistered-content") == 0 && result.ran;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "unregistered raw mailbox pass stays diagnostic GUEST_STOP");

    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, "synthetic-control-pass", 0u, 0u, 0u,
                            NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_TIMEOUT &&
           result.status == GBL_RUNNER_TIMEOUT && result.ran &&
           result.cycles == 0u && result.instructions == 0u &&
           result.semantic.status == GBL_MAILBOX_ABSENT;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "zero cycle budget times out without guest progress");

    memset(&result, 0, sizeof result);
    okay = gbl_pass_cycles != 0u &&
           gbl_test_execute(path, "synthetic-control-pass", gbl_pass_cycles,
                            0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_PASS &&
           result.status == GBL_RUNNER_PASS &&
           result.cycles == gbl_pass_cycles &&
           result.budget_cycles == gbl_pass_cycles;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "completed mailbox at exact cycle budget beats TIMEOUT");

    memset(&result, 0, sizeof result);
    okay = gbl_pass_cycles > 0u &&
           gbl_test_execute(path, "synthetic-control-pass",
                            gbl_pass_cycles - UINT64_C(1), 0u, 0u, NULL,
                            NULL, NULL, (gbl_runner_allocator){ 0 },
                            &result) == GBL_RUNNER_TIMEOUT &&
           result.status == GBL_RUNNER_TIMEOUT &&
           result.cycles <= result.budget_cycles &&
           result.semantic.status != GBL_MAILBOX_PASS;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "one-short cycle budget stops before mailbox completion");

    if (!gbl_test_path(scratch, sizeof scratch, scratch_directory,
                       "missing.gba")) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(scratch, NULL, 0u, 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_MISSING &&
           result.status == GBL_RUNNER_MISSING && !result.ran &&
           strcmp(result.reason, "rom-file-not-found") == 0;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "missing input stays MISSING before execution");

    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(scratch_directory, NULL, 1u, 0u, 0u, NULL,
                            NULL, NULL, (gbl_runner_allocator){ 0 },
                            &result) == GBL_RUNNER_HARNESS_FAIL &&
           result.status == GBL_RUNNER_HARNESS_FAIL && !result.ran &&
           (strcmp(result.reason, "rom-read-failed") == 0 ||
            strcmp(result.reason, "rom-open-failed") == 0);
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "host directory read failure stays HARNESS_FAIL");

    if (!gbl_test_path(scratch, sizeof scratch, scratch_directory,
                       "empty.gba") ||
        !gbl_test_write_file(scratch, 0u, 0u)) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(scratch, NULL, 1u, 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_REFUSED &&
           strcmp(result.reason, "rom-empty") == 0 && !result.ran;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "empty image is REFUSED before guest execution");

    if (!gbl_test_path(scratch, sizeof scratch, scratch_directory,
                       "short.gba") ||
        !gbl_test_write_file(scratch, 0xBFu, (unsigned char)'x')) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(scratch, NULL, 1u, 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_REFUSED &&
           strcmp(result.reason, "rom-too-short") == 0 && !result.ran;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "short image is REFUSED before guest execution");

    if (!gbl_test_path(scratch, sizeof scratch, scratch_directory,
                       "odd.gba") ||
        !gbl_test_write_file(scratch, 0xC1u, (unsigned char)'x')) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(scratch, NULL, 1u, 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_REFUSED &&
           strcmp(result.reason, "rom-odd-length") == 0 && !result.ran;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "odd in-range image is REFUSED before guest execution");

    if (!gbl_test_path(scratch, sizeof scratch, scratch_directory,
                       "oversized.gba") ||
        !gbl_test_write_file(scratch, GBL_RUNNER_MAX_ROM_BYTES + 1u,
                             (unsigned char)'x')) {
        return false;
    }
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(scratch, NULL, 1u, 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_REFUSED &&
           strcmp(result.reason, "rom-too-large") == 0 && !result.ran;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "overlarge image is refused at the sentinel bound");

    memset(&allocator_state, 0, sizeof allocator_state);
    allocator_state.fail_at = 1u;
    allocator = (gbl_runner_allocator){ &allocator_state, gbl_test_allocate,
                                        gbl_test_release };
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, NULL, 1u, 0u, 0u, NULL, NULL, NULL,
                            allocator, &result) == GBL_RUNNER_HARNESS_FAIL &&
           strcmp(result.reason, "rom-buffer-allocation-failed") == 0;
    gbl_runner_result_dispose(&result);
    okay = okay && allocator_state.successes == allocator_state.releases;

    memset(&allocator_state, 0, sizeof allocator_state);
    allocator_state.fail_at = 2u;
    allocator.context = &allocator_state;
    memset(&result, 0, sizeof result);
    okay = okay &&
           gbl_test_execute(path, NULL, 1u, 0u, 0u, NULL, NULL, NULL,
                            allocator, &result) == GBL_RUNNER_HARNESS_FAIL &&
           strcmp(result.reason, "machine-allocation-failed") == 0;
    gbl_runner_result_dispose(&result);
    okay = okay && allocator_state.successes == allocator_state.releases;

    memset(&allocator_state, 0, sizeof allocator_state);
    allocator_state.fail_at = 3u;
    allocator.context = &allocator_state;
    memset(&result, 0, sizeof result);
    okay = okay &&
           gbl_test_execute(path, NULL, 1u, 0u, 0u, NULL, NULL, NULL,
                            allocator, &result) == GBL_RUNNER_HARNESS_FAIL &&
           strcmp(result.reason, "rom-copy-allocation-failed") == 0;
    gbl_runner_result_dispose(&result);
    okay = okay && allocator_state.successes == allocator_state.releases;
    gbl_test_case(okay, "allocator failures retain HARNESS_FAIL and release all owned memory");

    memset(&allocator_state, 0, sizeof allocator_state);
    allocator_state.fail_at = 2u;
    allocator.context = &allocator_state;
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, NULL, 1u, UINT32_C(1), 1u, NULL, NULL,
                            NULL, allocator, &result) ==
               GBL_RUNNER_HARNESS_FAIL &&
           strcmp(result.reason, "trace-buffer-allocation-failed") == 0;
    gbl_runner_result_dispose(&result);
    okay = okay && allocator_state.successes == allocator_state.releases;
    gbl_test_case(okay, "trace allocation refusal releases input and owns no leaked block");

    memset(&allocator_state, 0, sizeof allocator_state);
    allocator.context = &allocator_state;
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, NULL, 0u, 0u,
                            GBL_RUNNER_MAX_TRACE_RECORDS + 1u, NULL, NULL,
                            NULL, allocator, &result) == GBL_RUNNER_REFUSED &&
           strcmp(result.reason, "invalid-runner-options") == 0 &&
           allocator_state.calls == 0u;
    gbl_runner_result_dispose(&result);
    okay = okay && allocator_state.successes == allocator_state.releases;
    gbl_test_case(okay, "trace ceiling rejects before allocation");

    if (!gbl_test_path(trace_path, sizeof trace_path, scratch_directory,
                       "trace-output.jsonl") ||
        !gbl_test_path(existing_trace_path, sizeof existing_trace_path,
                       scratch_directory, "trace-existing.jsonl") ||
        !gbl_test_write_file(existing_trace_path, 0u, 0u)) {
        return false;
    }
    (void)remove(trace_path);
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, "synthetic-control-pass", UINT64_MAX,
                            UINT32_C(1), GBL_RUNNER_MAX_TRACE_RECORDS,
                            existing_trace_path, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_HARNESS_FAIL &&
           result.status == GBL_RUNNER_HARNESS_FAIL &&
           strcmp(result.reason, "trace-write-failed") == 0 &&
           result.semantic.status == GBL_MAILBOX_PASS && result.ran &&
           !result.trace_emitted;
    gbl_runner_result_dispose(&result);
    okay = okay && gbl_test_cli_trace_write_limit(
                       runner_path, path, scratch_directory);
    gbl_test_case(okay, "trace open and close failures override completed guest PASS");

    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, "synthetic-control-pass", UINT64_MAX,
                            UINT32_C(1), 1u, trace_path, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_HARNESS_FAIL &&
           strcmp(result.reason, "trace-output-full") == 0 &&
           !result.trace_complete &&
           result.status == GBL_RUNNER_HARNESS_FAIL;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "trace buffer exhaustion stays HARNESS_FAIL and incomplete");

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-arm-loop.gba")) {
        return false;
    }
    memset(&cancel_state, 0, sizeof cancel_state);
    cancel_state.cancel_at = 2u;
    memset(&result, 0, sizeof result);
    okay = gbl_test_execute(path, NULL, UINT64_MAX, 0u, 0u, NULL,
                            gbl_test_cancel_at_boundary, &cancel_state,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_CANCELLED &&
           result.status == GBL_RUNNER_CANCELLED && result.ran &&
           result.cycles > 0u &&
           result.cycles <= result.budget_cycles;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "callback cancellation is observed after one committed CPU unit");

    if (!gbl_test_path(path, sizeof path, fixture_directory,
                       "owned-arm-pass.gba")) {
        return false;
    }
    memset(&cancel_state, 0, sizeof cancel_state);
    cancel_state.cancel_at =
        (size_t)((gbl_pass_instructions + UINT64_C(1)) * UINT64_C(2));
    memset(&result, 0, sizeof result);
    okay = cancel_state.cancel_at > 0u &&
           gbl_test_execute(path, "synthetic-control-pass", UINT64_MAX, 0u,
                            0u, NULL, gbl_test_cancel_at_boundary,
                            &cancel_state, (gbl_runner_allocator){ 0 },
                            &result) == GBL_RUNNER_CANCELLED &&
           result.status == GBL_RUNNER_CANCELLED &&
           result.semantic.status == GBL_MAILBOX_PASS && result.ran;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "cancellation at mailbox completion wins over semantic PASS");

    memset(&result, 0, sizeof result);
    stream = tmpfile();
    if (stream == NULL) {
        return false;
    }
    result.status = GBL_RUNNER_GUEST_FAIL;
    result.reason[0] = 'o';
    result.reason[1] = 'k';
    result.reason[2] = '\0';
    result.has_fixture_id = true;
    memcpy(result.fixture_id, "quote\" slash\\ line\n tab\t ctl\001",
           sizeof "quote\" slash\\ line\n tab\t ctl\001");
    okay = gbl_runner_write_json(stream, &result) && fflush(stream) == 0 &&
           fseek(stream, 0L, SEEK_SET) == 0 &&
           (json_length = fread(json, 1u, sizeof json - 1u, stream)) > 0u;
    if (okay) {
        json[json_length] = '\0';
        okay = strstr(json, "quote\\\" slash\\\\ line\\n tab\\t ctl\\u0001") !=
                   NULL &&
               strchr(json, '\n') == json + json_length - 1u &&
               strstr(json, "runner_test") == NULL;
    }
    (void)fclose(stream);
    gbl_test_case(okay, "JSON output escapes control bytes and does not expose host paths");

    return true;
}

static bool gbl_test_prepare_refusal_inputs(const char *directory)
{
    char path[GBL_RUNNER_TEST_PATH_CAPACITY];

    if (mkdir(directory, 0700) != 0 && errno != EEXIST) {
        return false;
    }
    return gbl_test_path(path, sizeof path, directory, "empty.gba") &&
           gbl_test_write_file(path, 0u, 0u) &&
           gbl_test_path(path, sizeof path, directory, "short.gba") &&
           gbl_test_write_file(path, 0xBFu, (unsigned char)'x') &&
           gbl_test_path(path, sizeof path, directory, "odd.gba") &&
           gbl_test_write_file(path, 0xC1u, (unsigned char)'x') &&
           gbl_test_path(path, sizeof path, directory, "oversized.gba") &&
           gbl_test_write_file(path, GBL_RUNNER_MAX_ROM_BYTES + 1u,
                               (unsigned char)'x');
}

int main(int argc, char **argv)
{
    struct stat info;

    if (argc == 1) {
        printf("1..%u\n", GBL_RUNNER_TEST_COUNT);
        if (!gbl_test_execute_all(GBL_RUNNER_TEST_FIXTURE_DIR,
                                 GBL_RUNNER_TEST_SCRATCH_DIR,
                                 GBL_RUNNER_TEST_CLI)) {
            fputs("runner_test: test setup failed\n", stderr);
            return 2;
        }
        return gbl_test_failures == 0u &&
                       gbl_test_number == GBL_RUNNER_TEST_COUNT
                   ? 0
                   : 1;
    }
    if (argc == 3 && strcmp(argv[1], "prepare") == 0 &&
        argv[2][0] != '\0') {
        return gbl_test_prepare_refusal_inputs(argv[2]) ? 0 : 1;
    }
#ifdef GBL_RUNNER_TEST_LOCAL_PLAYABLE0
    if (argc == 4 && strcmp(argv[1], "playable0") == 0 &&
        (strcmp(argv[2], "verify") == 0 ||
         strcmp(argv[2], "control") == 0) &&
        argv[3][0] != '\0') {
        bool setup;

        if (mkdir(argv[3], 0700) != 0 && errno != EEXIST) {
            fputs("runner_test: Playable 0 scratch setup failed\n", stderr);
            return 2;
        }
        if (stat(argv[3], &info) != 0 || !S_ISDIR(info.st_mode)) {
            fputs("runner_test: Playable 0 scratch path is not a directory\n",
                  stderr);
            return 2;
        }
        if (strcmp(argv[2], "verify") == 0) {
            printf("1..4\n");
            setup = gbl_test_runner_playable0_verify(argv[3]);
            if (!setup) {
                fputs("runner_test: Playable 0 verification setup failed\n",
                      stderr);
                return 2;
            }
            return gbl_test_failures == 0u && gbl_test_number == 4u ? 0 : 1;
        }
        printf("1..5\n");
        setup = gbl_test_runner_playable0_control(argv[3]);
        if (!setup) {
            fputs("runner_test: Playable 0 control setup failed\n", stderr);
            return 2;
        }
        return gbl_test_failures == 0u && gbl_test_number == 5u ? 0 : 1;
    }
#endif
    if (argc == 3 && strcmp(argv[1], "dma") == 0 && argv[2][0] != '\0') {
        if (mkdir(argv[2], 0700) != 0 && errno != EEXIST) {
            fputs("runner_test: DMA scratch directory setup failed\n", stderr);
            return 2;
        }
        printf("1..1\n");
        gbl_test_case(
            gbl_test_runner_dma_observation(argv[2]),
            "runner-dma-production: guest mailbox follows a real DMA transfer and result exposes phase9");
        return gbl_test_failures == 0u ? 0 : 1;
    }
    if (argc == 3 && strcmp(argv[1], "ppu.observation") == 0 &&
        argv[2][0] != '\0') {
        if (mkdir(argv[2], 0700) != 0 && errno != EEXIST) {
            fputs("runner_test: PPU scratch directory setup failed\n", stderr);
            return 2;
        }
        printf("1..1\n");
        gbl_test_case(gbl_test_runner_ppu_observation(argv[2]),
                      "runner-ppu-observation: emits profile-bound capabilities beside observed frame state");
        return gbl_test_failures == 0u ? 0 : 1;
    }
    if (argc == 3 && strcmp(argv[1], "ppu.output") == 0 &&
        argv[2][0] != '\0') {
        if (mkdir(argv[2], 0700) != 0 && errno != EEXIST) {
            fputs("runner_test: PPU output scratch directory setup failed\n",
                  stderr);
            return 2;
        }
        printf("1..1\n");
        gbl_test_case(gbl_test_runner_ppu_output(argv[2]),
                      "runner-ppu-output: exact frame bytes persist after disposal, repeat identically, and respond to input");
        return gbl_test_failures == 0u ? 0 : 1;
    }
    if (argc == 3 && strcmp(argv[1], "ppu.output-controls") == 0 &&
        argv[2][0] != '\0') {
        if (mkdir(argv[2], 0700) != 0 && errno != EEXIST) {
            fputs("runner_test: PPU output controls scratch setup failed\n",
                  stderr);
            return 2;
        }
        printf("1..1\n");
        gbl_test_case(
            gbl_test_runner_ppu_output_controls(argv[2]),
            "runner-ppu-output-controls: refusals preserve files, capture state, allocation balance, and emitted=false");
        return gbl_test_failures == 0u ? 0 : 1;
    }
    if (argc != 5 || strcmp(argv[1], "all") != 0 ||
        argv[2][0] == '\0' || argv[3][0] == '\0' || argv[4][0] == '\0') {
        fputs("usage: runner_test all FIXTURE_DIR SCRATCH_DIR RUNNER_PATH | "
              "runner_test prepare DIR | runner_test dma SCRATCH_DIR | "
              "runner_test playable0 {verify|control} SCRATCH_DIR | "
              "runner_test ppu.observation SCRATCH_DIR | "
              "runner_test ppu.output SCRATCH_DIR | "
              "runner_test ppu.output-controls SCRATCH_DIR\n",
              stderr);
        return 2;
    }
    if (stat(argv[2], &info) != 0 || !S_ISDIR(info.st_mode) ||
        stat(argv[4], &info) != 0 || S_ISDIR(info.st_mode)) {
        fputs("runner_test: required generated fixture or runner is missing\n",
              stderr);
        return 2;
    }
    printf("1..%u\n", GBL_RUNNER_TEST_COUNT);
    if (!gbl_test_execute_all(argv[2], argv[3], argv[4])) {
        fputs("runner_test: test setup failed\n", stderr);
        return 2;
    }
    return gbl_test_failures == 0u && gbl_test_number == GBL_RUNNER_TEST_COUNT
               ? 0
               : 1;
}
