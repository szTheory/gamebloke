#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "gamebloke/gamebloke.h"
#include "run.h"

enum { GBL_RUN_USAGE_ERROR = 2 };
enum {
    GBL_RUNNER_INPUT_SCRIPT_MAX_BYTES = 16 * 1024,
    GBL_RUNNER_INPUT_SCRIPT_PROBE_BYTES = 1,
    GBL_RUNNER_INPUT_SCRIPT_MAX_LINE_BYTES = 25
};

static volatile sig_atomic_t gbl_runner_interrupted;

typedef enum gbl_runner_script_load_status {
    GBL_RUNNER_SCRIPT_LOADED = 0,
    GBL_RUNNER_SCRIPT_MISSING,
    GBL_RUNNER_SCRIPT_IO_ERROR,
    GBL_RUNNER_SCRIPT_INVALID
} gbl_runner_script_load_status;

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
          "[--fixture ID] [--input-script PATH] [--trace-mask INTEGER] "
          "[--trace-capacity INTEGER] [--trace-output PATH] "
          "[--video-output PATH]\n",
          out);
}

static bool gbl_runner_normalize_path(const char *path,
                                      char normalized[PATH_MAX])
{
    char absolute[PATH_MAX];
    char candidate[PATH_MAX];
    char parent_path[PATH_MAX];
    char resolved_parent[PATH_MAX];
    char current_directory[PATH_MAX];
    char *slash;
    char *token;
    char *save = NULL;
    const char *base_name;
    size_t path_length;
    size_t output_length = 1u;

    if (path == NULL || path[0] == '\0') {
        return false;
    }
    path_length = strlen(path);
    if (path_length >= sizeof absolute) {
        return false;
    }
    if (realpath(path, normalized) != NULL) {
        return true;
    }
    if (path[0] == '/') {
        memcpy(absolute, path, path_length + 1u);
    } else {
        size_t cwd_length;

        if (getcwd(current_directory, sizeof current_directory) == NULL) {
            return false;
        }
        cwd_length = strlen(current_directory);
        if (cwd_length + 1u + path_length >= sizeof absolute) {
            return false;
        }
        memcpy(absolute, current_directory, cwd_length);
        absolute[cwd_length] = '/';
        memcpy(absolute + cwd_length + 1u, path, path_length + 1u);
    }

    slash = strrchr(absolute, '/');
    if (slash == NULL) {
        return false;
    }
    base_name = slash + 1;
    if (slash == absolute) {
        memcpy(parent_path, "/", 2u);
    } else {
        *slash = '\0';
        if (strlen(absolute) >= sizeof parent_path) {
            return false;
        }
        memcpy(parent_path, absolute, strlen(absolute) + 1u);
    }
    if (base_name[0] != '\0' &&
        realpath(parent_path, resolved_parent) != NULL) {
        int written;

        if (strcmp(resolved_parent, "/") == 0) {
            written = snprintf(candidate, sizeof candidate, "/%s", base_name);
        } else {
            written = snprintf(candidate, sizeof candidate, "%s/%s",
                               resolved_parent, base_name);
        }
        if (written < 0 || (size_t)written >= sizeof candidate) {
            return false;
        }
    } else if (base_name[0] == '\0') {
        size_t absolute_length = strlen(absolute);

        if (absolute_length >= sizeof candidate) {
            return false;
        }
        memcpy(candidate, absolute, absolute_length + 1u);
    } else {
        /* The parent may not exist yet. Keep an absolute lexical path so
         * dot-segment aliases still compare before either output writer runs. */
        if (path[0] == '/') {
            if (path_length >= sizeof candidate) {
                return false;
            }
            memcpy(candidate, path, path_length + 1u);
        } else {
            size_t cwd_length = strlen(current_directory);

            if (cwd_length + 1u + path_length >= sizeof candidate) {
                return false;
            }
            memcpy(candidate, current_directory, cwd_length);
            candidate[cwd_length] = '/';
            memcpy(candidate + cwd_length + 1u, path, path_length + 1u);
        }
    }

    normalized[0] = '/';
    normalized[1] = '\0';
    for (token = strtok_r(candidate, "/", &save); token != NULL;
         token = strtok_r(NULL, "/", &save)) {
        size_t token_length;

        if (strcmp(token, ".") == 0 || token[0] == '\0') {
            continue;
        }
        if (strcmp(token, "..") == 0) {
            if (output_length > 1u) {
                while (output_length > 1u &&
                       normalized[output_length - 1u] != '/') {
                    output_length--;
                }
                if (output_length > 1u) {
                    output_length--;
                }
                normalized[output_length] = '\0';
            }
            continue;
        }
        token_length = strlen(token);
        if (output_length + (output_length > 1u ? 1u : 0u) + token_length >=
            PATH_MAX) {
            return false;
        }
        if (output_length > 1u) {
            normalized[output_length++] = '/';
        }
        memcpy(normalized + output_length, token, token_length);
        output_length += token_length;
        normalized[output_length] = '\0';
    }
    return true;
}

static bool gbl_runner_paths_alias(const char *left, const char *right)
{
    struct stat left_stat;
    struct stat right_stat;
    char left_normalized[PATH_MAX];
    char right_normalized[PATH_MAX];

    if (left == NULL || right == NULL) {
        return false;
    }
    if (strcmp(left, right) == 0) {
        return true;
    }
    if (stat(left, &left_stat) == 0 && stat(right, &right_stat) == 0 &&
        left_stat.st_dev == right_stat.st_dev &&
        left_stat.st_ino == right_stat.st_ino) {
        return true;
    }
    return gbl_runner_normalize_path(left, left_normalized) &&
           gbl_runner_normalize_path(right, right_normalized) &&
           strcmp(left_normalized, right_normalized) == 0;
}

static bool gbl_runner_video_path_is_safe(
    const gbl_runner_options *options, const char *input_script_path)
{
    struct stat video_stat;
    struct stat result_stat;

    if (options->video_output_path == NULL) {
        return true;
    }
    if (gbl_runner_paths_alias(options->video_output_path, options->rom_path) ||
        gbl_runner_paths_alias(options->video_output_path, input_script_path) ||
        gbl_runner_paths_alias(options->video_output_path,
                                options->trace_output_path) ||
        gbl_runner_paths_alias(options->video_output_path, "/dev/stdout") ||
        gbl_runner_paths_alias(options->video_output_path, "/dev/fd/1") ||
        gbl_runner_paths_alias(options->video_output_path,
                                "/proc/self/fd/1")) {
        return false;
    }
    /* JSON result metadata is written to stdout; refuse an explicit pathname
     * that resolves to the same host file/device before guest execution. */
    if (stat(options->video_output_path, &video_stat) == 0 &&
        fstat(STDOUT_FILENO, &result_stat) == 0 &&
        video_stat.st_dev == result_stat.st_dev &&
        video_stat.st_ino == result_stat.st_ino) {
        return false;
    }
    return true;
}

static bool gbl_runner_ascii_whitespace(unsigned char character)
{
    return character == (unsigned char)' ' ||
           character == (unsigned char)'\t' ||
           character == (unsigned char)'\n' ||
           character == (unsigned char)'\r' ||
           character == (unsigned char)'\v' ||
           character == (unsigned char)'\f';
}

static bool gbl_runner_parse_decimal_bytes(const unsigned char *bytes,
                                           size_t length, uint64_t *value_out,
                                           bool *overflow_out)
{
    uint64_t value = 0u;
    size_t index;

    if (bytes == NULL || length == 0u || value_out == NULL ||
        overflow_out == NULL) {
        return false;
    }
    *overflow_out = false;
    for (index = 0u; index < length; ++index) {
        uint64_t digit;

        if (bytes[index] < (unsigned char)'0' ||
            bytes[index] > (unsigned char)'9') {
            return false;
        }
        digit = (uint64_t)(bytes[index] - (unsigned char)'0');
        if (value > (UINT64_MAX - digit) / UINT64_C(10)) {
            *overflow_out = true;
            return false;
        }
        value = value * UINT64_C(10) + digit;
    }
    *value_out = value;
    return true;
}

static bool gbl_runner_parse_input_script(
    const unsigned char *bytes, size_t length,
    gbl_input_event events[GBL_INPUT_EVENT_CAPACITY],
    size_t *event_count_out, const char **reason_out)
{
    size_t offset = 0u;
    size_t event_count = 0u;

    if (bytes == NULL || events == NULL || event_count_out == NULL ||
        reason_out == NULL) {
        return false;
    }
    *event_count_out = 0u;
    *reason_out = "input-script-invalid-grammar";
    while (offset < length) {
        const size_t line_start = offset;
        size_t line_end;
        size_t line_length;
        size_t separator = SIZE_MAX;
        size_t index;
        uint64_t cycle;
        uint64_t pressed_mask;
        bool overflow = false;
        bool whitespace_only = true;

        while (offset < length && bytes[offset] != (unsigned char)'\n') {
            offset++;
        }
        line_end = offset;
        if (offset < length) {
            offset++;
            if (line_end > line_start &&
                bytes[line_end - 1u] == (unsigned char)'\r') {
                line_end--;
            }
        }
        line_length = line_end - line_start;
        if (line_length > GBL_RUNNER_INPUT_SCRIPT_MAX_LINE_BYTES) {
            *reason_out = "input-script-line-too-long";
            return false;
        }
        for (index = line_start; index < line_end; ++index) {
            if (bytes[index] == 0u) {
                *reason_out = "input-script-nul-byte";
                return false;
            }
            if (!gbl_runner_ascii_whitespace(bytes[index])) {
                whitespace_only = false;
            }
        }
        if (line_length == 0u || whitespace_only) {
            *reason_out = "input-script-blank-line";
            return false;
        }
        if (event_count == GBL_INPUT_EVENT_CAPACITY) {
            *reason_out = "input-script-too-many-records";
            return false;
        }
        for (index = line_start; index < line_end; ++index) {
            if (bytes[index] == (unsigned char)' ') {
                if (separator != SIZE_MAX) {
                    *reason_out = "input-script-invalid-grammar";
                    return false;
                }
                separator = index;
            }
        }
        if (separator == SIZE_MAX || separator == line_start ||
            separator + 1u == line_end) {
            *reason_out = "input-script-invalid-grammar";
            return false;
        }
        if (!gbl_runner_parse_decimal_bytes(
                bytes + line_start, separator - line_start, &cycle,
                &overflow)) {
            *reason_out = overflow ? "input-script-cycle-overflow"
                                   : "input-script-invalid-grammar";
            return false;
        }
        if (!gbl_runner_parse_decimal_bytes(
                bytes + separator + 1u, line_end - separator - 1u,
                &pressed_mask, &overflow)) {
            *reason_out = overflow ? "input-script-key-mask-out-of-range"
                                   : "input-script-invalid-grammar";
            return false;
        }
        if (pressed_mask > UINT64_C(0x03FF)) {
            *reason_out = "input-script-key-mask-out-of-range";
            return false;
        }
        if (event_count != 0u &&
            cycle < events[event_count - 1u].cycle) {
            *reason_out = "input-script-cycle-decreasing";
            return false;
        }
        events[event_count].cycle = cycle;
        events[event_count].pressed_mask = (uint16_t)pressed_mask;
        event_count++;
    }
    *event_count_out = event_count;
    return true;
}

static gbl_runner_script_load_status gbl_runner_load_input_script(
    const char *path, gbl_input_event events[GBL_INPUT_EVENT_CAPACITY],
    size_t *event_count_out, const char **reason_out)
{
    unsigned char bytes[GBL_RUNNER_INPUT_SCRIPT_MAX_BYTES +
                        GBL_RUNNER_INPUT_SCRIPT_PROBE_BYTES];
    FILE *input;
    size_t length;
    bool read_failed;
    int saved_errno;

    if (reason_out == NULL) {
        return GBL_RUNNER_SCRIPT_INVALID;
    }
    if (path == NULL || path[0] == '\0' || event_count_out == NULL) {
        *reason_out = "input-script-invalid-grammar";
        return GBL_RUNNER_SCRIPT_INVALID;
    }
    input = fopen(path, "rb");
    if (input == NULL) {
        saved_errno = errno;
        if (saved_errno == ENOENT || saved_errno == ENOTDIR) {
            *reason_out = "input-script-file-not-found";
            return GBL_RUNNER_SCRIPT_MISSING;
        }
        *reason_out = "input-script-open-failed";
        return GBL_RUNNER_SCRIPT_IO_ERROR;
    }
    length = fread(bytes, 1u, sizeof bytes, input);
    read_failed = ferror(input) != 0;
    saved_errno = errno;
    if (length > GBL_RUNNER_INPUT_SCRIPT_MAX_BYTES) {
        (void)fclose(input);
        *reason_out = "input-script-too-large";
        return GBL_RUNNER_SCRIPT_INVALID;
    }
    if (read_failed) {
        (void)fclose(input);
        errno = saved_errno;
        *reason_out = "input-script-read-failed";
        return GBL_RUNNER_SCRIPT_IO_ERROR;
    }
    if (fclose(input) != 0) {
        *reason_out = "input-script-read-failed";
        return GBL_RUNNER_SCRIPT_IO_ERROR;
    }
    if (!gbl_runner_parse_input_script(bytes, length, events,
                                       event_count_out, reason_out)) {
        return GBL_RUNNER_SCRIPT_INVALID;
    }
    return GBL_RUNNER_SCRIPT_LOADED;
}

static int gbl_runner_script_io_result(gbl_runner_status status,
                                       const char *reason)
{
    gbl_runner_result result;

    memset(&result, 0, sizeof result);
    result.status = status;
    result.semantic.status = GBL_MAILBOX_ABSENT;
    result.trace_complete = true;
    (void)snprintf(result.reason, sizeof result.reason, "%s", reason);
    (void)snprintf(result.termination, sizeof result.termination,
                   "not-started");
    if (!gbl_runner_write_json(stdout, &result)) {
        fputs("gamebloke-run: result-write-failed\n", stderr);
    }
    return 1;
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

static bool parse_options(int argc, char **argv, gbl_runner_options *options,
                          const char **input_script_path_out)
{
    bool seen_rom = false;
    bool seen_fixture = false;
    bool seen_cycles = false;
    bool seen_frames = false;
    bool seen_trace_mask = false;
    bool seen_trace_capacity = false;
    bool seen_trace_output = false;
    bool seen_video_output = false;
    bool seen_input_script = false;
    uint64_t bound = 0u;
    int index = 1;

    if (input_script_path_out == NULL) {
        return false;
    }
    memset(options, 0, sizeof *options);
    *input_script_path_out = NULL;
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
        } else if (strcmp(flag, "--video-output") == 0) {
            if (seen_video_output || index + 1 >= argc) return false;
            value = argv[++index];
            if (value[0] == '\0') return false;
            options->video_output_path = value;
            seen_video_output = true;
        } else if (strcmp(flag, "--input-script") == 0) {
            if (seen_input_script || index + 1 >= argc) return false;
            value = argv[++index];
            if (value[0] == '\0') return false;
            *input_script_path_out = value;
            seen_input_script = true;
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
    gbl_input_event input_events[GBL_INPUT_EVENT_CAPACITY] = { 0 };
    size_t input_event_count = 0u;
    const char *input_script_path = NULL;
    const char *input_script_reason = NULL;
    gbl_runner_script_load_status script_status;

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
              "  --input-script PATH    read at most 16 KiB of cycle-based key events\n"
              "  --trace-mask INTEGER   select instruction, bus, or exception trace\n"
              "  --trace-capacity N     bound selected trace records (maximum 65536)\n"
              "  --trace-output PATH    write selected trace as JSON Lines\n"
              "  --video-output PATH    write one complete raw 240x160 BGR555 frame\n"
              "  --version              print the gamebloke-run version\n"
              "  --help                 print this help\n"
              "\n"
              "exit codes: 0 success, 2 usage error\n"
              "run result: 1 for any valid non-PASS result\n",
              stdout);
        return 0;
    }
    if (!parse_options(argc, argv, &options, &input_script_path)) {
        print_usage(stderr);
        return GBL_RUN_USAGE_ERROR;
    }
    if (!gbl_runner_video_path_is_safe(&options, input_script_path)) {
        print_usage(stderr);
        fputs("gamebloke-run: video-output-path-conflict\n", stderr);
        return GBL_RUN_USAGE_ERROR;
    }
    if (input_script_path != NULL) {
        script_status = gbl_runner_load_input_script(
            input_script_path, input_events, &input_event_count,
            &input_script_reason);
        if (script_status == GBL_RUNNER_SCRIPT_INVALID) {
            print_usage(stderr);
            fprintf(stderr, "gamebloke-run: %s\n", input_script_reason);
            return GBL_RUN_USAGE_ERROR;
        }
        if (script_status == GBL_RUNNER_SCRIPT_MISSING) {
            return gbl_runner_script_io_result(GBL_RUNNER_MISSING,
                                               input_script_reason);
        }
        if (script_status == GBL_RUNNER_SCRIPT_IO_ERROR) {
            return gbl_runner_script_io_result(GBL_RUNNER_HARNESS_FAIL,
                                               input_script_reason);
        }
        options.input_timeline_present = true;
        options.input_events = input_event_count == 0u ? NULL : input_events;
        options.input_event_count = input_event_count;
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
