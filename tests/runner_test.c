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
    GBL_RUNNER_TEST_COUNT = 24u,
    GBL_RUNNER_TEST_PATH_CAPACITY = 1024u,
    GBL_RUNNER_TEST_WATCHDOG_TICKS = 300u
};

typedef struct gbl_test_allocator_state {
    size_t calls;
    size_t successes;
    size_t releases;
    size_t fail_at;
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
    } cases[] = {
        { GBL_EXCEPTION_RESET, GBL_CPU_STOP_RESET_NO_BIOS,
          "no-bios-reset" },
        { GBL_EXCEPTION_UNDEFINED, GBL_CPU_STOP_UNDEFINED_NO_BIOS,
          "no-bios-undefined" },
        { GBL_EXCEPTION_SWI, GBL_CPU_STOP_SWI_NO_BIOS, "no-bios-swi" },
        { GBL_EXCEPTION_PREFETCH_ABORT,
          GBL_CPU_STOP_PREFETCH_ABORT_NO_BIOS, "no-bios-prefetch-abort" },
        { GBL_EXCEPTION_DATA_ABORT, GBL_CPU_STOP_DATA_ABORT_NO_BIOS,
          "no-bios-data-abort" },
        { GBL_EXCEPTION_IRQ, GBL_CPU_STOP_IRQ_NO_BIOS, "no-bios-irq" },
        { GBL_EXCEPTION_FIQ, GBL_CPU_STOP_FIQ_NO_BIOS, "no-bios-fiq" },
        { GBL_EXCEPTION_COPROCESSOR,
          GBL_CPU_STOP_COPROCESSOR_NO_BIOS, "no-bios-coprocessor" }
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
            okay = gbl_cpu_set_cpsr(&machine, UINT32_C(0x13)) ==
                       GBL_CPU_STATUS_OK &&
                   gbl_cpu_enter_exception(&machine, cases[index].kind,
                                           &context) == GBL_CPU_STATUS_OK &&
                   machine.cpu.stopped &&
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
        gbl_run_options options = { .cycle_budget = 10u };
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
            run.cycles == 2u + cases[index].instructions;
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
                strstr(json, "profile version 5 direct-start") != NULL &&
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
    }
    return pointer;
}

static void gbl_test_release(void *context, void *pointer, size_t size)
{
    gbl_test_allocator_state *state = context;

    (void)size;
    if (pointer != NULL) {
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
    char output[8192];
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
        "all CPU exception stops share the production GUEST_STOP JSON mapping");
    gbl_test_case(gbl_test_instruction_stop_json(scratch_directory),
        "executed instruction causes reach the shared real JSON writer with claim and time");
    gbl_test_case(gbl_test_cli_cancel(runner_path, path, scratch_directory),
                  "CLI converts SIGINT to CANCELLED before watchdog expiry");

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
    okay = gbl_test_execute(path, "synthetic-control-bios-required",
                            UINT64_C(1000), 0u, 0u, NULL, NULL, NULL,
                            (gbl_runner_allocator){ 0 }, &result) ==
               GBL_RUNNER_SKIPPED &&
           result.status == GBL_RUNNER_SKIPPED && !result.ran &&
           result.cycles == 0u &&
           strcmp(result.reason, "required-bios-unavailable") == 0;
    gbl_runner_result_dispose(&result);
    gbl_test_case(okay, "required BIOS metadata yields SKIPPED before execution");

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
    if (argc != 5 || strcmp(argv[1], "all") != 0 ||
        argv[2][0] == '\0' || argv[3][0] == '\0' || argv[4][0] == '\0') {
        fputs("usage: runner_test all FIXTURE_DIR SCRATCH_DIR RUNNER_PATH | "
              "runner_test prepare DIR\n",
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
