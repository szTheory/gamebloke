#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include "libretro.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>

#define GBL_TERMINAL_WIDTH 60u
#define GBL_TERMINAL_HEIGHT 20u
#define GBL_GUEST_WIDTH 240u
#define GBL_GUEST_HEIGHT 160u
#define GBL_GUEST_ROW_BYTES ((size_t)GBL_GUEST_WIDTH * sizeof(uint32_t))
#define GBL_MAX_CONTENT_SIZE ((size_t)UINT32_C(0x02000000))
#define GBL_FRAME_LIMIT_MAX 1000u
#define GBL_OUTPUT_CAPACITY 65536u
#define GBL_BUTTON_A ((uint16_t)1u << 0u)
#define GBL_BUTTON_RIGHT ((uint16_t)1u << 4u)

typedef void (*retro_set_environment_fn)(retro_environment_t);
typedef void (*retro_set_video_refresh_fn)(retro_video_refresh_t);
typedef void (*retro_set_audio_sample_fn)(retro_audio_sample_t);
typedef void (*retro_set_audio_sample_batch_fn)(retro_audio_sample_batch_t);
typedef void (*retro_set_input_poll_fn)(retro_input_poll_t);
typedef void (*retro_set_input_state_fn)(retro_input_state_t);
typedef unsigned (*retro_api_version_fn)(void);
typedef void (*retro_get_system_info_fn)(struct retro_system_info *);
typedef void (*retro_get_system_av_info_fn)(struct retro_system_av_info *);
typedef void (*retro_init_fn)(void);
typedef void (*retro_deinit_fn)(void);
typedef bool (*retro_load_game_fn)(const struct retro_game_info *);
typedef void (*retro_unload_game_fn)(void);
typedef void (*retro_run_fn)(void);
typedef void (*retro_reset_fn)(void);

typedef struct terminal_core_api {
    retro_set_environment_fn set_environment;
    retro_set_video_refresh_fn set_video_refresh;
    retro_set_audio_sample_fn set_audio_sample;
    retro_set_audio_sample_batch_fn set_audio_sample_batch;
    retro_set_input_poll_fn set_input_poll;
    retro_set_input_state_fn set_input_state;
    retro_api_version_fn api_version;
    retro_get_system_info_fn get_system_info;
    retro_get_system_av_info_fn get_system_av_info;
    retro_init_fn init;
    retro_deinit_fn deinit;
    retro_load_game_fn load_game;
    retro_unload_game_fn unload_game;
    retro_run_fn run;
    retro_reset_fn reset;
} terminal_core_api;

typedef struct terminal_host {
    bool pixel_format_ready;
    bool frame_valid;
    bool frame_updated;
    bool frame_error;
    bool previous_cells_valid;
    bool initialized;
    bool loaded;
    bool terminal_attributes_saved;
    bool terminal_flags_saved;
    bool terminal_attributes_changed;
    bool terminal_flags_changed;
    bool terminal_screen_active;
    bool signal_actions_saved;
    unsigned terminal_rows;
    unsigned terminal_columns;
    unsigned frames;
    uint16_t buttons;
    int original_file_flags;
    struct termios original_termios;
    uint32_t pixels[GBL_GUEST_WIDTH * GBL_GUEST_HEIGHT];
    uint8_t cells[GBL_TERMINAL_HEIGHT][GBL_TERMINAL_WIDTH][3];
    uint8_t previous_cells[GBL_TERMINAL_HEIGHT][GBL_TERMINAL_WIDTH][3];
    struct sigaction old_sigint;
    struct sigaction old_sigterm;
    struct sigaction old_sighup;
    struct sigaction old_sigwinch;
    struct sigaction old_sigtstp;
} terminal_host;

typedef enum terminal_mode {
    TERMINAL_INTERACTIVE,
    TERMINAL_CHECK,
    TERMINAL_SMOKE
} terminal_mode;

static terminal_host *active_host;
static volatile sig_atomic_t requested_signal;
static volatile sig_atomic_t resize_requested;

static uint64_t monotonic_nanoseconds(void);

static void terminal_signal_handler(int signal_number)
{
    if (signal_number == SIGWINCH) {
        resize_requested = 1;
    } else {
        requested_signal = signal_number;
    }
}

static void terminal_audio_sample(int16_t left, int16_t right)
{
    (void)left;
    (void)right;
}

static size_t terminal_audio_batch(const int16_t *data, size_t frames)
{
    (void)data;
    return frames;
}

static void terminal_log(enum retro_log_level level, const char *format, ...)
{
    (void)level;
    (void)format;
}

static bool terminal_environment(unsigned command, void *data)
{
    if (active_host == NULL) return false;
    if (command == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT) {
        enum retro_pixel_format format;
        if (data == NULL) return false;
        format = *(const enum retro_pixel_format *)data;
        active_host->pixel_format_ready =
            format == RETRO_PIXEL_FORMAT_XRGB8888;
        return active_host->pixel_format_ready;
    }
    if (command == RETRO_ENVIRONMENT_GET_LOG_INTERFACE) {
        struct retro_log_callback *callback = data;
        if (callback == NULL) return false;
        callback->log = terminal_log;
        return true;
    }
    if (command == RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS) return true;
    if (command == RETRO_ENVIRONMENT_GET_INPUT_BITMASKS) return false;
    return false;
}

static void terminal_video_refresh(const void *data, unsigned width,
                                   unsigned height, size_t pitch)
{
    unsigned row;
    if (active_host == NULL) return;
    active_host->frame_updated = false;
    if (data == NULL) return;
    if (width != GBL_GUEST_WIDTH || height != GBL_GUEST_HEIGHT ||
        pitch < GBL_GUEST_ROW_BYTES || pitch > SIZE_MAX / height) {
        active_host->frame_error = true;
        return;
    }
    for (row = 0u; row < GBL_GUEST_HEIGHT; ++row) {
        memcpy(&active_host->pixels[(size_t)row * GBL_GUEST_WIDTH],
               (const uint8_t *)data + (size_t)row * pitch,
               GBL_GUEST_ROW_BYTES);
    }
    active_host->frame_valid = true;
    active_host->frame_updated = true;
}

static void terminal_input_poll(void)
{
}

static int16_t terminal_input_state(unsigned port, unsigned device,
                                    unsigned index, unsigned id)
{
    static const unsigned ids[10] = {
        RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
        RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
        RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_LEFT,
        RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L
    };
    unsigned bit;
    if (active_host == NULL || port != 0u || device != RETRO_DEVICE_JOYPAD ||
        index != 0u) return 0;
    for (bit = 0u; bit < 10u; ++bit) {
        if (ids[bit] == id) {
            return (active_host->buttons & (uint16_t)(1u << bit)) != 0u
                ? 1 : 0;
        }
    }
    return 0;
}

static bool load_symbol(void *library, const char *name, void *function,
                        size_t function_size)
{
    void *symbol = dlsym(library, name);
    if (symbol == NULL || function_size != sizeof(symbol)) return false;
    memcpy(function, &symbol, function_size);
    return true;
}

#define LOAD(api, library, member, symbol) \
    load_symbol((library), (symbol), &(api)->member, sizeof((api)->member))

static bool load_api(void *library, terminal_core_api *api)
{
    bool okay = true;
    memset(api, 0, sizeof(*api));
    okay = LOAD(api, library, set_environment, "retro_set_environment") && okay;
    okay = LOAD(api, library, set_video_refresh, "retro_set_video_refresh") && okay;
    okay = LOAD(api, library, set_audio_sample, "retro_set_audio_sample") && okay;
    okay = LOAD(api, library, set_audio_sample_batch,
                "retro_set_audio_sample_batch") && okay;
    okay = LOAD(api, library, set_input_poll, "retro_set_input_poll") && okay;
    okay = LOAD(api, library, set_input_state, "retro_set_input_state") && okay;
    okay = LOAD(api, library, api_version, "retro_api_version") && okay;
    okay = LOAD(api, library, get_system_info, "retro_get_system_info") && okay;
    okay = LOAD(api, library, get_system_av_info, "retro_get_system_av_info") && okay;
    okay = LOAD(api, library, init, "retro_init") && okay;
    okay = LOAD(api, library, deinit, "retro_deinit") && okay;
    okay = LOAD(api, library, load_game, "retro_load_game") && okay;
    okay = LOAD(api, library, unload_game, "retro_unload_game") && okay;
    okay = LOAD(api, library, run, "retro_run") && okay;
    okay = LOAD(api, library, reset, "retro_reset") && okay;
    return okay;
}

static bool read_content(const char *path, uint8_t **bytes, size_t *size)
{
    int fd;
    struct stat info;
    uint8_t *buffer;
    size_t offset = 0u;
    uint8_t extra;
    ssize_t count;
    *bytes = NULL;
    *size = 0u;
    fd = open(path, O_RDONLY);
    if (fd < 0) return false;
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        (uintmax_t)info.st_size < UINTMAX_C(0xC0) ||
        (uintmax_t)info.st_size > (uintmax_t)GBL_MAX_CONTENT_SIZE ||
        (info.st_size & 1) != 0) {
        (void)close(fd);
        return false;
    }
    *size = (size_t)info.st_size;
    buffer = malloc(*size);
    if (buffer == NULL) {
        *size = 0u;
        (void)close(fd);
        return false;
    }
    while (offset < *size) {
        count = read(fd, buffer + offset, *size - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            free(buffer);
            *size = 0u;
            (void)close(fd);
            return false;
        }
        offset += (size_t)count;
    }
    do {
        count = read(fd, &extra, 1u);
    } while (count < 0 && errno == EINTR);
    if (count != 0) {
        free(buffer);
        *size = 0u;
        (void)close(fd);
        return false;
    }
    if (close(fd) != 0) {
        free(buffer);
        *size = 0u;
        return false;
    }
    *bytes = buffer;
    return true;
}

static bool open_core(const char *path, void **library,
                      terminal_core_api *api, terminal_host *host,
                      const uint8_t *content, size_t content_size)
{
    struct retro_system_info info = { 0 };
    struct retro_system_av_info av_info = { 0 };
    struct retro_game_info game = { 0 };
    *library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (*library == NULL || !load_api(*library, api)) return false;
    if (api->api_version() != RETRO_API_VERSION) return false;

    active_host = host;
    api->set_environment(terminal_environment);
    api->set_video_refresh(terminal_video_refresh);
    api->set_audio_sample(terminal_audio_sample);
    api->set_audio_sample_batch(terminal_audio_batch);
    api->set_input_poll(terminal_input_poll);
    api->set_input_state(terminal_input_state);
    if (!host->pixel_format_ready) return false;

    api->get_system_info(&info);
    api->get_system_av_info(&av_info);
    if (info.library_name == NULL || strcmp(info.library_name, "Gamebloke") != 0 ||
        info.valid_extensions == NULL || strcmp(info.valid_extensions, "gba") != 0 ||
        info.need_fullpath || av_info.geometry.base_width != GBL_GUEST_WIDTH ||
        av_info.geometry.base_height != GBL_GUEST_HEIGHT ||
        av_info.geometry.max_width != GBL_GUEST_WIDTH ||
        av_info.geometry.max_height != GBL_GUEST_HEIGHT ||
        av_info.timing.fps < 50.0 || av_info.timing.fps > 70.0) {
        return false;
    }

    api->init();
    host->initialized = true;
    if (!host->pixel_format_ready) return false;
    game.path = NULL;
    game.data = content;
    game.size = content_size;
    game.meta = NULL;
    if (!api->load_game(&game)) return false;
    host->loaded = true;
    return true;
}

static void close_core(void *library, terminal_core_api *api,
                       terminal_host *host, uint8_t *content)
{
    if (host->loaded) {
        api->unload_game();
        host->loaded = false;
    }
    if (host->initialized) {
        api->deinit();
        host->initialized = false;
    }
    if (library != NULL) (void)dlclose(library);
    active_host = NULL;
    free(content);
}

static bool get_terminal_size(unsigned *rows, unsigned *columns)
{
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0) return false;
    *rows = size.ws_row;
    *columns = size.ws_col;
    return true;
}

static bool terminal_ready(terminal_host *host)
{
    const char *term = getenv("TERM");
    char *input_name;
    char *output_name;
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) ||
        term == NULL || term[0] == '\0' || strcmp(term, "dumb") == 0) {
        return false;
    }
    input_name = ttyname(STDIN_FILENO);
    output_name = ttyname(STDOUT_FILENO);
    if (input_name == NULL || output_name == NULL ||
        strcmp(input_name, output_name) != 0) return false;
    if (!get_terminal_size(&host->terminal_rows, &host->terminal_columns)) {
        return false;
    }
    return host->terminal_columns >= GBL_TERMINAL_WIDTH &&
           host->terminal_rows >= GBL_TERMINAL_HEIGHT + 4u;
}

static bool install_signal_actions(terminal_host *host)
{
    struct sigaction action;
    struct sigaction ignore;
    memset(&action, 0, sizeof(action));
    action.sa_handler = terminal_signal_handler;
    sigemptyset(&action.sa_mask);
    memset(&ignore, 0, sizeof(ignore));
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    if (sigaction(SIGINT, &action, &host->old_sigint) != 0) return false;
    if (sigaction(SIGTERM, &action, &host->old_sigterm) != 0) {
        (void)sigaction(SIGINT, &host->old_sigint, NULL);
        return false;
    }
    if (sigaction(SIGHUP, &action, &host->old_sighup) != 0) {
        (void)sigaction(SIGTERM, &host->old_sigterm, NULL);
        (void)sigaction(SIGINT, &host->old_sigint, NULL);
        return false;
    }
    if (sigaction(SIGWINCH, &action, &host->old_sigwinch) != 0) {
        (void)sigaction(SIGHUP, &host->old_sighup, NULL);
        (void)sigaction(SIGTERM, &host->old_sigterm, NULL);
        (void)sigaction(SIGINT, &host->old_sigint, NULL);
        return false;
    }
    if (sigaction(SIGTSTP, &ignore, &host->old_sigtstp) != 0) {
        (void)sigaction(SIGWINCH, &host->old_sigwinch, NULL);
        (void)sigaction(SIGHUP, &host->old_sighup, NULL);
        (void)sigaction(SIGTERM, &host->old_sigterm, NULL);
        (void)sigaction(SIGINT, &host->old_sigint, NULL);
        return false;
    }
    host->signal_actions_saved = true;
    requested_signal = 0;
    resize_requested = 0;
    return true;
}

static void restore_signal_actions(terminal_host *host)
{
    if (!host->signal_actions_saved) return;
    (void)sigaction(SIGINT, &host->old_sigint, NULL);
    (void)sigaction(SIGTERM, &host->old_sigterm, NULL);
    (void)sigaction(SIGHUP, &host->old_sighup, NULL);
    (void)sigaction(SIGWINCH, &host->old_sigwinch, NULL);
    (void)sigaction(SIGTSTP, &host->old_sigtstp, NULL);
    host->signal_actions_saved = false;
}

static bool write_all(int fd, const char *data, size_t size)
{
    size_t offset = 0u;
    const uint64_t start = monotonic_nanoseconds();
    while (offset < size) {
        if (monotonic_nanoseconds() - start > UINT64_C(1000000000)) {
            return false;
        }
        const ssize_t written = write(fd, data + offset, size - offset);
        if (written > 0) {
            offset += (size_t)written;
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd descriptor = { fd, POLLOUT, 0 };
            if (poll(&descriptor, 1u, 100) > 0 &&
                (descriptor.revents & POLLOUT) != 0) continue;
        }
        if (monotonic_nanoseconds() - start > UINT64_C(1000000000)) {
            return false;
        }
        if (written == 0 || (written < 0 && errno != EINTR)) return false;
    }
    return true;
}

static bool terminal_enter(terminal_host *host)
{
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &host->original_termios) != 0) return false;
    host->terminal_attributes_saved = true;
    host->original_file_flags = fcntl(STDIN_FILENO, F_GETFL);
    if (host->original_file_flags < 0) return false;
    host->terminal_flags_saved = true;
    raw = host->original_termios;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN);
    raw.c_iflag &= (tcflag_t)~(IXON | ICRNL | INLCR | IGNCR);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return false;
    host->terminal_attributes_changed = true;
    if (fcntl(STDIN_FILENO, F_SETFL,
              host->original_file_flags | O_NONBLOCK) != 0) return false;
    host->terminal_flags_changed = true;
    host->terminal_screen_active = true;
    if (!write_all(STDOUT_FILENO, "\033[?1049h\033[?25l\033[2J\033[H",
                   sizeof("\033[?1049h\033[?25l\033[2J\033[H") - 1u)) {
        return false;
    }
    return true;
}

static void terminal_restore(terminal_host *host)
{
    if (host->terminal_screen_active) {
        (void)write_all(STDOUT_FILENO, "\033[0m\033[?25h\033[?1049l",
                        sizeof("\033[0m\033[?25h\033[?1049l") - 1u);
        host->terminal_screen_active = false;
    }
    if (host->terminal_attributes_saved && host->terminal_attributes_changed) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &host->original_termios);
        host->terminal_attributes_changed = false;
    }
    if (host->terminal_flags_saved && host->terminal_flags_changed) {
        (void)fcntl(STDIN_FILENO, F_SETFL, host->original_file_flags);
        host->terminal_flags_changed = false;
    }
    restore_signal_actions(host);
}

static bool append_text(char *output, size_t capacity, size_t *used,
                        const char *format, unsigned a, unsigned b,
                        unsigned c, unsigned d, unsigned e)
{
    int length = snprintf(output + *used, capacity - *used,
                          format, a, b, c, d, e);
    if (length < 0 || (size_t)length >= capacity - *used) return false;
    *used += (size_t)length;
    return true;
}

static bool build_frame_output(terminal_host *host, char *output,
                               size_t capacity, size_t *output_size,
                               double frames_per_second)
{
    unsigned row;
    unsigned column;
    size_t used = 0u;
    bool full = !host->previous_cells_valid;
    char held[16];
    const char *right = (host->buttons & GBL_BUTTON_RIGHT) != 0u ? "DOWN" : "up";
    const char *button_a = (host->buttons & GBL_BUTTON_A) != 0u ? "DOWN" : "up";
    (void)snprintf(held, sizeof(held), "R:%s A:%s", right, button_a);
    {
        char status[96];
        int status_length = snprintf(status, sizeof(status),
            "\033[0m\033[1;1HGamebloke Playable 0  frame=%u  output=%.2f FPS\033[K",
            host->frames, frames_per_second);
        if (status_length < 0 || (size_t)status_length >= sizeof(status) ||
            (size_t)status_length > capacity) return false;
        used = 0u;
        memcpy(output, status, (size_t)status_length);
        used = (size_t)status_length;
    }
    {
        char controls[96];
        int controls_length = snprintf(controls, sizeof(controls),
            "\033[2;1HD=RIGHT F=release J=A K=release Space=all R=reset Q=quit held:%s\033[K",
            held);
        if (controls_length < 0 || (size_t)controls_length >= sizeof(controls) ||
            (size_t)controls_length > capacity - used) return false;
        memcpy(output + used, controls, (size_t)controls_length);
        used += (size_t)controls_length;
    }

    for (row = 0u; row < GBL_TERMINAL_HEIGHT; ++row) {
        for (column = 0u; column < GBL_TERMINAL_WIDTH; ++column) {
            unsigned red = 0u;
            unsigned green = 0u;
            unsigned blue = 0u;
            unsigned sample_y;
            unsigned sample_x;
            const unsigned source_x = column * 4u;
            const unsigned source_y = row * 8u;
            for (sample_y = 0u; sample_y < 8u; ++sample_y) {
                for (sample_x = 0u; sample_x < 4u; ++sample_x) {
                    const uint32_t pixel = host->pixels[
                        (size_t)(source_y + sample_y) * GBL_GUEST_WIDTH +
                        source_x + sample_x];
                    red += (pixel >> 16u) & UINT32_C(0xFF);
                    green += (pixel >> 8u) & UINT32_C(0xFF);
                    blue += pixel & UINT32_C(0xFF);
                }
            }
            host->cells[row][column][0] = (uint8_t)(red / 32u);
            host->cells[row][column][1] = (uint8_t)(green / 32u);
            host->cells[row][column][2] = (uint8_t)(blue / 32u);
            if (full || memcmp(host->cells[row][column],
                               host->previous_cells[row][column], 3u) != 0) {
                if (!append_text(output, capacity, &used,
                    "\033[%u;%uH\033[48;2;%u;%u;%um ",
                    row + 3u, column + 1u,
                    host->cells[row][column][0],
                    host->cells[row][column][1],
                    host->cells[row][column][2])) return false;
            }
        }
    }
    if (used + 4u > capacity) return false;
    memcpy(output + used, "\033[0m", 4u);
    used += 4u;
    memcpy(host->previous_cells, host->cells, sizeof(host->cells));
    host->previous_cells_valid = true;
    *output_size = used;
    return true;
}

static uint64_t monotonic_nanoseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0u;
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
           (uint64_t)now.tv_nsec;
}

static bool render_frame_at(terminal_host *host, uint64_t started_at,
                            char *output, size_t output_capacity)
{
    size_t output_size = 0u;
    const uint64_t now = monotonic_nanoseconds();
    const double elapsed = now > started_at
        ? (double)(now - started_at) / 1000000000.0 : 0.0;
    const double rate = elapsed > 0.0 ? (double)host->frames / elapsed : 0.0;
    if (!host->frame_valid ||
        !build_frame_output(host, output, output_capacity, &output_size, rate)) {
        return false;
    }
    return write_all(STDOUT_FILENO, output, output_size);
}

static bool render_resize_notice(void)
{
    static const char notice[] =
        "\033[0m\033[1;1HTerminal needs at least 60 columns by 24 rows. Resize, then continue.\033[K"
        "\033[2;1HQ quits; R resets.\033[K";
    return write_all(STDOUT_FILENO, notice, sizeof(notice) - 1u);
}

static void process_key(terminal_host *host, terminal_core_api *api,
                        unsigned char key, bool *running)
{
    switch (key) {
    case 'd': case 'D': host->buttons |= GBL_BUTTON_RIGHT; break;
    case 'f': case 'F': host->buttons &= (uint16_t)~GBL_BUTTON_RIGHT; break;
    case 'j': case 'J': host->buttons |= GBL_BUTTON_A; break;
    case 'k': case 'K': host->buttons &= (uint16_t)~GBL_BUTTON_A; break;
    case ' ': host->buttons = 0u; break;
    case 'r': case 'R':
        host->buttons = 0u;
        host->frame_valid = false;
        host->frame_updated = false;
        host->previous_cells_valid = false;
        api->reset();
        break;
    case 'q': case 'Q': case 4u: *running = false; break;
    default: break;
    }
}

static bool read_keys(terminal_host *host, terminal_core_api *api,
                      bool *running)
{
    unsigned char keys[64];
    ssize_t count;
    do {
        count = read(STDIN_FILENO, keys, sizeof(keys));
        if (count > 0) {
            ssize_t index;
            for (index = 0; index < count; ++index) {
                process_key(host, api, keys[index], running);
                if (!*running) return true;
            }
            continue;
        }
        if (count == 0) return true;
        if (errno == EINTR) {
            if (requested_signal != 0) return true;
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
        return false;
    } while (true);
}

static int milliseconds_until(uint64_t due, uint64_t now)
{
    uint64_t remaining;
    if (due <= now) return 0;
    remaining = due - now;
    if (remaining / UINT64_C(1000000) > (uint64_t)INT32_MAX) return INT32_MAX;
    return (int)((remaining + UINT64_C(999999)) / UINT64_C(1000000));
}

static bool run_smoke(terminal_core_api *api, terminal_host *host,
                      unsigned frame_limit, double *elapsed_ms)
{
    unsigned index;
    uint64_t start = monotonic_nanoseconds();
    for (index = 0u; index < frame_limit; ++index) {
        api->run();
        ++host->frames;
        if (host->frame_error) return false;
    }
    {
        uint64_t end = monotonic_nanoseconds();
        if (end <= start) return false;
        *elapsed_ms = (double)(end - start) / 1000000.0;
    }
    return host->frame_valid;
}

static bool run_interactive(terminal_core_api *api, terminal_host *host,
                            unsigned frame_limit)
{
    char output[GBL_OUTPUT_CAPACITY];
    const double fps = 16777216.0 / 280896.0;
    const uint64_t interval = (uint64_t)(1000000000.0 / fps);
    uint64_t started_at = monotonic_nanoseconds();
    uint64_t due = started_at;
    bool running = true;
    bool need_resize_notice = false;

    while (running && requested_signal == 0 &&
           (frame_limit == 0u || host->frames < frame_limit)) {
        struct pollfd input = { STDIN_FILENO, POLLIN, 0 };
        const uint64_t now = monotonic_nanoseconds();
        int poll_result;
        int timeout;

        if (!read_keys(host, api, &running)) break;
        if (!running || requested_signal != 0) break;

        if (resize_requested != 0) {
            resize_requested = 0;
            if (!get_terminal_size(&host->terminal_rows, &host->terminal_columns)) {
                break;
            }
            host->buttons = 0u;
            if (host->terminal_columns < GBL_TERMINAL_WIDTH ||
                host->terminal_rows < GBL_TERMINAL_HEIGHT + 4u) {
                need_resize_notice = true;
                if (!render_resize_notice()) break;
            } else {
                need_resize_notice = false;
                host->previous_cells_valid = false;
                due = monotonic_nanoseconds();
            }
        }

        if (!need_resize_notice && now >= due) {
            api->run();
            ++host->frames;
            if (host->frame_error) return false;
            if (host->frame_updated &&
                !render_frame_at(host, started_at, output, sizeof(output))) {
                return false;
            }
            due += interval;
            if (due <= monotonic_nanoseconds()) {
                due = monotonic_nanoseconds() + interval;
            }
            continue;
        }

        timeout = need_resize_notice ? 200 : milliseconds_until(due, now);
        poll_result = poll(&input, 1u, timeout);
        if (poll_result < 0 && errno != EINTR) return false;
        if (requested_signal != 0) break;
        if (poll_result > 0 && (input.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            if ((input.revents & POLLIN) == 0) break;
        }
    }
    return requested_signal == 0 && !host->frame_error;
}

static bool parse_frame_limit(const char *text, unsigned *value)
{
    char *end = NULL;
    unsigned long parsed;
    if (text == NULL || text[0] == '\0' || text[0] == '-') return false;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0ul ||
        parsed > (unsigned long)GBL_FRAME_LIMIT_MAX) return false;
    *value = (unsigned)parsed;
    return true;
}

static void usage(FILE *stream)
{
    fputs("usage: gamebloke-terminal [--check|--smoke] [--frames COUNT] MODULE CONTENT\n",
          stream);
}

int main(int argc, char **argv)
{
    terminal_host host = { 0 };
    terminal_core_api api = { 0 };
    terminal_mode mode = TERMINAL_INTERACTIVE;
    void *library = NULL;
    uint8_t *content = NULL;
    size_t content_size = 0u;
    unsigned frame_limit = 0u;
    int argument = 1;
    double smoke_elapsed_ms = 0.0;
    bool okay = false;
    bool entered_terminal = false;

    while (argument < argc && argv[argument][0] == '-') {
        if (strcmp(argv[argument], "--check") == 0) {
            mode = TERMINAL_CHECK;
            ++argument;
        } else if (strcmp(argv[argument], "--smoke") == 0) {
            mode = TERMINAL_SMOKE;
            ++argument;
        } else if (strcmp(argv[argument], "--frames") == 0 &&
                   argument + 1 < argc) {
            if (!parse_frame_limit(argv[argument + 1], &frame_limit)) {
                usage(stderr);
                return 2;
            }
            argument += 2;
        } else {
            usage(stderr);
            return 2;
        }
    }
    if (argc - argument != 2) {
        usage(stderr);
        return 2;
    }
    if (!read_content(argv[argument + 1], &content, &content_size)) {
        fputs("gamebloke-terminal: content is missing, malformed, or outside the local 32 MiB bound\n",
              stderr);
        return 1;
    }
    if (!open_core(argv[argument], &library, &api, &host,
                   content, content_size)) {
        fputs("gamebloke-terminal: module is incompatible or refused the GBA guest\n",
              stderr);
        close_core(library, &api, &host, content);
        return 1;
    }

    if (mode == TERMINAL_CHECK) {
        puts("gamebloke-terminal: Gamebloke module and local GBA guest are ready");
        close_core(library, &api, &host, content);
        return 0;
    }
    if (mode == TERMINAL_SMOKE) {
        okay = run_smoke(&api, &host, 30u, &smoke_elapsed_ms);
        if (okay) {
            printf("gamebloke-terminal: smoke frames=%u elapsed_ms=%.3f fps=%.2f\n",
                   host.frames, smoke_elapsed_ms,
                   (double)host.frames * 1000.0 / smoke_elapsed_ms);
        } else {
            fputs("gamebloke-terminal: bounded module smoke produced no valid frame\n",
                  stderr);
        }
        close_core(library, &api, &host, content);
        return okay ? 0 : 1;
    }

    if (!terminal_ready(&host)) {
        fputs("gamebloke-terminal: need one ANSI color terminal on stdin/stdout at least 60 columns by 24 rows (use --check for noninteractive validation)\n",
              stderr);
        close_core(library, &api, &host, content);
        return 1;
    }
    if (!install_signal_actions(&host) || !terminal_enter(&host)) {
        fputs("gamebloke-terminal: could not prepare and safely restore terminal mode\n",
              stderr);
        terminal_restore(&host);
        close_core(library, &api, &host, content);
        return 1;
    }
    entered_terminal = true;
    okay = run_interactive(&api, &host, frame_limit);
    terminal_restore(&host);
    close_core(library, &api, &host, content);
    if (!okay) {
        if (entered_terminal && requested_signal == 0) {
            fputs("gamebloke-terminal: terminal loop stopped after an input, display, or frame error\n",
                  stderr);
        }
        return requested_signal == 0 ? 1 : 0;
    }
    return 0;
}
