#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gamebloke/gamebloke.h"
#include "run.h"

enum { GBL_RUN_USAGE_ERROR = 2 };

static volatile sig_atomic_t gbl_runner_interrupted;

static void gbl_runner_handle_sigint(int signal_number)
{
    (void)signal_number;
    gbl_runner_interrupted = 1;
}

static bool gbl_runner_cancelled(void *context)
{
    (void)context;
    return gbl_runner_interrupted != 0;
}

static bool gbl_runner_install_sigint(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof action);
    action.sa_handler = gbl_runner_handle_sigint;
    if (sigemptyset(&action.sa_mask) != 0) {
        return false;
    }
    return sigaction(SIGINT, &action, NULL) == 0;
}

static int gbl_runner_signal_setup_failure(void)
{
    gbl_runner_result result;
    bool written;

    memset(&result, 0, sizeof result);
    result.status = GBL_RUNNER_HARNESS_FAIL;
    result.semantic.status = GBL_MAILBOX_ABSENT;
    result.trace_complete = true;
    (void)snprintf(result.reason, sizeof result.reason,
                   "sigint-handler-install-failed");
    (void)snprintf(result.termination, sizeof result.termination,
                   "not-started");
    written = gbl_runner_write_json(stdout, &result);
    if (!written) {
        fputs("gamebloke-run: result-write-failed\n", stderr);
    }
    return 1;
}

static bool nonempty_string_fits(const char *text, size_t capacity)
{
    size_t length = 0u;

    if (text == NULL || capacity == 0u || text[0] == '\0') {
        return false;
    }
    while (length < capacity && text[length] != '\0') {
        length++;
    }
    return length < capacity;
}

static void print_usage(FILE *out)
{
    fputs("usage: gamebloke-run --rom PATH (--cycles DECIMAL | --frames DECIMAL) "
          "[--fixture ID] [--trace-mask INTEGER] [--trace-capacity INTEGER] "
          "[--trace-output PATH]\n",
          out);
}

static bool parse_decimal_u64(const char *text, uint64_t *value_out)
{
    uint64_t value = 0u;
    size_t index = 0u;

    if (text == NULL || text[0] == '\0' || value_out == NULL) {
        return false;
    }
    while (text[index] != '\0') {
        const unsigned char character = (unsigned char)text[index];
        uint64_t digit;

        if (character < (unsigned char)'0' ||
            character > (unsigned char)'9') {
            return false;
        }
        digit = (uint64_t)(character - (unsigned char)'0');
        if (value > (UINT64_MAX - digit) / UINT64_C(10)) {
            return false;
        }
        value = value * UINT64_C(10) + digit;
        index++;
    }
    *value_out = value;
    return true;
}

static bool parse_trace_mask(const char *text, uint32_t *mask_out)
{
    uint64_t value = 0u;
    unsigned int base = 10u;
    size_t index = 0u;

    if (text == NULL || text[0] == '\0' || mask_out == NULL) {
        return false;
    }
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16u;
        index = 2u;
        if (text[index] == '\0') {
            return false;
        }
    }
    while (text[index] != '\0') {
        const unsigned char character = (unsigned char)text[index];
        uint64_t digit;

        if (character >= (unsigned char)'0' &&
            character <= (unsigned char)'9') {
            digit = (uint64_t)(character - (unsigned char)'0');
        } else if (base == 16u && character >= (unsigned char)'a' &&
                   character <= (unsigned char)'f') {
            digit = (uint64_t)(character - (unsigned char)'a') +
                    UINT64_C(10);
        } else if (base == 16u && character >= (unsigned char)'A' &&
                   character <= (unsigned char)'F') {
            digit = (uint64_t)(character - (unsigned char)'A') +
                    UINT64_C(10);
        } else {
            return false;
        }
        if (digit >= (uint64_t)base ||
            value > (UINT64_MAX - digit) / (uint64_t)base) {
            return false;
        }
        value = value * (uint64_t)base + digit;
        index++;
    }
    if (value > (uint64_t)UINT32_MAX ||
        (((uint32_t)value) & ~GBL_TRACE_CATEGORY_MASK) != 0u) {
        return false;
    }
    *mask_out = (uint32_t)value;
    return true;
}

static bool parse_options(int argc, char **argv, gbl_runner_options *options)
{
    bool seen_rom = false;
    bool seen_fixture = false;
    bool seen_cycles = false;
    bool seen_frames = false;
    bool seen_trace_mask = false;
    bool seen_trace_capacity = false;
    bool seen_trace_output = false;
    uint64_t bound = 0u;
    int index = 1;

    memset(options, 0, sizeof *options);
    while (index < argc) {
        const char *flag = argv[index];
        const char *value;

        if (strcmp(flag, "--rom") == 0) {
            if (seen_rom || index + 1 >= argc) return false;
            value = argv[++index];
            if (value[0] == '\0') return false;
            options->rom_path = value;
            seen_rom = true;
        } else if (strcmp(flag, "--fixture") == 0) {
            if (seen_fixture || index + 1 >= argc) return false;
            value = argv[++index];
            if (!nonempty_string_fits(value,
                                      GBL_RUNNER_FIXTURE_ID_CAPACITY)) {
                return false;
            }
            options->fixture_id = value;
            seen_fixture = true;
        } else if (strcmp(flag, "--cycles") == 0) {
            if (seen_cycles || seen_frames || index + 1 >= argc ||
                !parse_decimal_u64(argv[++index], &bound)) {
                return false;
            }
            options->cycle_budget = bound;
            seen_cycles = true;
        } else if (strcmp(flag, "--frames") == 0) {
            if (seen_frames || seen_cycles || index + 1 >= argc ||
                !parse_decimal_u64(argv[++index], &bound) ||
                bound > UINT64_MAX / GBL_RUNNER_FRAME_CYCLES) {
                return false;
            }
            options->cycle_budget = bound * GBL_RUNNER_FRAME_CYCLES;
            seen_frames = true;
        } else if (strcmp(flag, "--trace-mask") == 0) {
            if (seen_trace_mask || index + 1 >= argc ||
                !parse_trace_mask(argv[++index], &options->trace_mask)) {
                return false;
            }
            seen_trace_mask = true;
        } else if (strcmp(flag, "--trace-capacity") == 0) {
            uint64_t capacity;

            if (seen_trace_capacity || index + 1 >= argc ||
                !parse_decimal_u64(argv[++index], &capacity) ||
                (uintmax_t)capacity > (uintmax_t)SIZE_MAX ||
                capacity > (uint64_t)GBL_RUNNER_MAX_TRACE_RECORDS) {
                return false;
            }
            options->trace_capacity = (size_t)capacity;
            seen_trace_capacity = true;
        } else if (strcmp(flag, "--trace-output") == 0) {
            if (seen_trace_output || index + 1 >= argc) return false;
            value = argv[++index];
            if (value[0] == '\0') return false;
            options->trace_output_path = value;
            seen_trace_output = true;
        } else {
            return false;
        }
        index++;
    }
    return seen_rom && (seen_cycles || seen_frames);
}

int main(int argc, char **argv)
{
    gbl_runner_options options;
    gbl_runner_result result;
    gbl_runner_status status;

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("gamebloke-run %s\n", gbl_version());
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        fputs("\n"
              "  --rom PATH             load one bounded ROM image\n"
              "  --cycles DECIMAL       run within a master-cycle budget\n"
              "  --frames DECIMAL       run within a profile frame budget\n"
              "  --fixture ID           select validated build-time fixture metadata\n"
              "  --trace-mask INTEGER   select instruction, bus, or exception trace\n"
              "  --trace-capacity N     bound selected trace records (maximum 65536)\n"
              "  --trace-output PATH    write selected trace as JSON Lines\n"
              "  --version              print the gamebloke-run version\n"
              "  --help                 print this help\n"
              "\n"
              "exit codes: 0 success, 2 usage error\n"
              "run result: 1 for any valid non-PASS result\n",
              stdout);
        return 0;
    }
    if (!parse_options(argc, argv, &options)) {
        print_usage(stderr);
        return GBL_RUN_USAGE_ERROR;
    }
    if (!gbl_runner_install_sigint()) {
        return gbl_runner_signal_setup_failure();
    }
    options.cancelled = gbl_runner_cancelled;
    options.cancel_context = NULL;

    status = gbl_runner_execute(&options, &result);
    if (!gbl_runner_write_json(stdout, &result)) {
        fputs("gamebloke-run: result-write-failed\n", stderr);
        gbl_runner_result_dispose(&result);
        return 1;
    }
    gbl_runner_result_dispose(&result);
    return status == GBL_RUNNER_PASS ? 0 : 1;
}
