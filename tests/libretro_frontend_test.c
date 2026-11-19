#include "libretro.h"

#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
typedef size_t (*retro_serialize_size_fn)(void);
typedef bool (*retro_serialize_fn)(void *, size_t);
typedef bool (*retro_unserialize_fn)(const void *, size_t);
typedef void (*retro_cheat_reset_fn)(void);
typedef void (*retro_cheat_set_fn)(unsigned, bool, const char *);
typedef unsigned (*retro_get_region_fn)(void);
typedef void *(*retro_get_memory_data_fn)(unsigned);
typedef size_t (*retro_get_memory_size_fn)(unsigned);
typedef bool (*retro_load_game_special_fn)(unsigned,
                                           const struct retro_game_info *,
                                           size_t);

typedef struct core_api {
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
    retro_serialize_size_fn serialize_size;
    retro_serialize_fn serialize;
    retro_unserialize_fn unserialize;
    retro_cheat_reset_fn cheat_reset;
    retro_cheat_set_fn cheat_set;
    retro_get_region_fn get_region;
    retro_get_memory_data_fn get_memory_data;
    retro_get_memory_size_fn get_memory_size;
    retro_load_game_special_fn load_game_special;
} core_api;

typedef struct frontend_state {
    retro_log_printf_t log;
    bool allow_pixel_format;
    unsigned format_negotiations;
    uint16_t buttons;
    unsigned polls;
    unsigned state_queries;
    unsigned queries_per_key[10];
    unsigned frames;
    unsigned nonzero_pixels;
    uint32_t sampled_pixel;
    uint32_t neutral_marker_pixel;
    uint32_t background_pixel;
    uint64_t frame_digest;
    bool callback_shape_ok;
    bool poll_preceded_queries;
    bool input_descriptors_ok;
} frontend_state;

static frontend_state frontend;

static void test_log(enum retro_log_level level, const char *format, ...)
{
    (void)level;
    (void)format;
}

static bool test_environment(unsigned command, void *data)
{
    if (command == RETRO_ENVIRONMENT_SET_PIXEL_FORMAT) {
        enum retro_pixel_format format;
        if (data == NULL) return false;
        format = *(const enum retro_pixel_format *)data;
        ++frontend.format_negotiations;
        return frontend.allow_pixel_format &&
               format == RETRO_PIXEL_FORMAT_XRGB8888;
    }
    if (command == RETRO_ENVIRONMENT_GET_LOG_INTERFACE) {
        struct retro_log_callback *callback = data;
        if (callback == NULL) return false;
        callback->log = test_log;
        return true;
    }
    if (command == RETRO_ENVIRONMENT_GET_INPUT_BITMASKS) return true;
    if (command == RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS) {
        static const char *const expected_labels[10] = {
            "GBA A", "GBA B", "Select", "Start", "Right", "Left",
            "Up", "Down", "GBA R", "GBA L"
        };
        static const unsigned expected_ids[10] = {
            RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
            RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
            RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_LEFT,
            RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
            RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L
        };
        const struct retro_input_descriptor *descriptors = data;
        unsigned index;
        frontend.input_descriptors_ok = descriptors != NULL;
        for (index = 0u; index < 10u && frontend.input_descriptors_ok; ++index) {
            frontend.input_descriptors_ok = descriptors[index].port == 0u &&
                descriptors[index].device == RETRO_DEVICE_JOYPAD &&
                descriptors[index].index == 0u &&
                descriptors[index].id == expected_ids[index] &&
                descriptors[index].description != NULL &&
                strcmp(descriptors[index].description, expected_labels[index]) == 0;
        }
        return frontend.input_descriptors_ok;
    }
    if (command == RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME) return true;
    return false;
}

static void test_video_refresh(const void *data, unsigned width,
                               unsigned height, size_t pitch)
{
    const uint32_t *pixels = data;
    size_t count;
    size_t index;
    frontend.callback_shape_ok = width == 240u && height == 160u &&
                                 pitch == 240u * sizeof(uint32_t) &&
                                 data != NULL;
    ++frontend.frames;
    if (!frontend.callback_shape_ok) return;
    count = (size_t)width * (size_t)height;
    frontend.nonzero_pixels = 0u;
    frontend.frame_digest = UINT64_C(1469598103934665603);
    for (index = 0u; index < count; ++index) {
        if (pixels[index] != 0u) ++frontend.nonzero_pixels;
        frontend.frame_digest ^= pixels[index];
        frontend.frame_digest *= UINT64_C(1099511628211);
    }
    frontend.sampled_pixel = pixels[(size_t)80u * width + 64u];
    frontend.background_pixel = pixels[0];
    frontend.neutral_marker_pixel = frontend.sampled_pixel;
}

static void test_input_poll(void)
{
    ++frontend.polls;
}

static int16_t test_input_state(unsigned port, unsigned device,
                                unsigned index, unsigned id)
{
    static const unsigned key_ids[10] = {
        RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
        RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
        RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_LEFT,
        RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L
    };
    unsigned bit;
    ++frontend.state_queries;
    if (frontend.polls != frontend.frames + 1u) {
        frontend.poll_preceded_queries = false;
    }
    if (port != 0u || device != RETRO_DEVICE_JOYPAD || index != 0u) return 0;
    for (bit = 0u; bit < 10u; ++bit) {
        if (key_ids[bit] == id) {
            ++frontend.queries_per_key[bit];
            return (frontend.buttons & (uint16_t)(1u << bit)) != 0u ? 1 : 0;
        }
    }
    return 0;
}

static bool load_symbol(void *library, const char *name, void *function,
                        size_t function_size)
{
    void *symbol = dlsym(library, name);
    if (symbol == NULL || function_size != sizeof(symbol)) {
        fprintf(stderr, "libretro-test: missing or incompatible ABI symbol %s\n",
                name);
        return false;
    }
    memcpy(function, &symbol, function_size);
    return true;
}

#define LOAD(api, library, member, symbol) \
    load_symbol((library), (symbol), &(api)->member, sizeof((api)->member))

static bool load_api(void *library, core_api *api)
{
    bool okay = true;
    memset(api, 0, sizeof(*api));
    okay = LOAD(api, library, set_environment, "retro_set_environment") && okay;
    okay = LOAD(api, library, set_video_refresh, "retro_set_video_refresh") && okay;
    okay = LOAD(api, library, set_audio_sample, "retro_set_audio_sample") && okay;
    okay = LOAD(api, library, set_audio_sample_batch, "retro_set_audio_sample_batch") && okay;
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
    okay = LOAD(api, library, serialize_size, "retro_serialize_size") && okay;
    okay = LOAD(api, library, serialize, "retro_serialize") && okay;
    okay = LOAD(api, library, unserialize, "retro_unserialize") && okay;
    okay = LOAD(api, library, cheat_reset, "retro_cheat_reset") && okay;
    okay = LOAD(api, library, cheat_set, "retro_cheat_set") && okay;
    okay = LOAD(api, library, get_region, "retro_get_region") && okay;
    okay = LOAD(api, library, get_memory_data, "retro_get_memory_data") && okay;
    okay = LOAD(api, library, get_memory_size, "retro_get_memory_size") && okay;
    okay = LOAD(api, library, load_game_special, "retro_load_game_special") && okay;
    return okay;
}

static uint8_t *read_content(const char *path, size_t *size_out)
{
    FILE *file = fopen(path, "rb");
    long length;
    uint8_t *bytes;
    if (file == NULL) return NULL;
    if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0 ||
        (uintmax_t)length > UINT32_C(0x02000000) || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }
    bytes = malloc((size_t)length);
    if (bytes == NULL || fread(bytes, 1u, (size_t)length, file) != (size_t)length) {
        free(bytes);
        (void)fclose(file);
        return NULL;
    }
    (void)fclose(file);
    *size_out = (size_t)length;
    return bytes;
}

static int run_tracer(const char *module_path, const char *content_path)
{
    void *library = dlopen(module_path, RTLD_NOW | RTLD_LOCAL);
    core_api api;
    struct retro_system_info system = { 0 };
    struct retro_system_av_info av = { 0 };
    struct retro_game_info game = { 0 };
    struct retro_game_info rejected = { 0 };
    uint8_t *content;
    size_t content_size = 0u;
    unsigned frame;
    uint64_t expected_digest;
    if (library == NULL) {
        fprintf(stderr, "libretro-test: dlopen failed: %s\n", dlerror());
        return 1;
    }
    if (!load_api(library, &api)) {
        (void)dlclose(library);
        return 1;
    }
    if (api.api_version() != RETRO_API_VERSION) {
        fputs("libretro-test: API version mismatch\n", stderr);
        (void)dlclose(library);
        return 1;
    }
    frontend.allow_pixel_format = true;
    api.set_environment(test_environment);
    api.set_video_refresh(test_video_refresh);
    api.set_audio_sample(NULL);
    api.set_audio_sample_batch(NULL);
    api.set_input_poll(test_input_poll);
    api.set_input_state(test_input_state);
    api.get_system_info(&system);
    api.get_system_av_info(&av);
    if (system.library_name == NULL || strcmp(system.library_name, "Gamebloke") != 0 ||
        system.need_fullpath || system.valid_extensions == NULL ||
        strcmp(system.valid_extensions, "gba") != 0 ||
        av.geometry.base_width != 240u || av.geometry.base_height != 160u ||
        av.geometry.max_width != 240u || av.geometry.max_height != 160u ||
        av.geometry.aspect_ratio != 1.5f ||
        av.timing.fps != 16777216.0 / 280896.0) {
        fputs("libretro-test: system identity/geometry/timing mismatch\n", stderr);
        (void)dlclose(library);
        return 1;
    }
    content = read_content(content_path, &content_size);
    if (content == NULL) {
        fprintf(stderr, "libretro-test: cannot read bounded local guest %s\n", content_path);
        (void)dlclose(library);
        return 1;
    }
    game.path = content_path;
    game.data = content;
    game.size = content_size;
    api.init();
    frontend.poll_preceded_queries = true;
    frontend.allow_pixel_format = false;
    api.set_environment(test_environment);
    if (api.load_game(&game)) {
        fputs("libretro-test: accepted rejected XRGB8888 negotiation\n", stderr);
        api.unload_game(); api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    frontend.allow_pixel_format = true;
    api.set_environment(test_environment);
    rejected = game;
    if (api.load_game(NULL) || api.load_game(&(struct retro_game_info){0})) {
        fputs("libretro-test: accepted missing content metadata\n", stderr);
        api.unload_game(); api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    rejected.size = 0u;
    if (api.load_game(&rejected)) {
        fputs("libretro-test: accepted zero-length content\n", stderr);
        api.unload_game(); api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    rejected = game;
    rejected.size = 0xBFu;
    if (api.load_game(&rejected)) {
        fputs("libretro-test: accepted short content\n", stderr);
        api.unload_game(); api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    rejected = game;
    rejected.data = NULL;
    if (api.load_game(&rejected)) {
        fputs("libretro-test: accepted missing content bytes\n", stderr);
        api.unload_game(); api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    rejected = game;
    rejected.size = content_size - 1u;
    if (api.load_game(&rejected)) {
        fputs("libretro-test: accepted odd content size\n", stderr);
        api.unload_game(); api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    rejected = game;
    rejected.size = (size_t)UINT32_C(0x02000001);
    if (api.load_game(&rejected)) {
        fputs("libretro-test: accepted over-limit content size\n", stderr);
        api.unload_game(); api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    if (!api.load_game(&game)) {
        fputs("libretro-test: module refused project-authored GBA content\n", stderr);
        api.deinit();
        free(content);
        (void)dlclose(library);
        return 1;
    }
    for (frame = 0u; frame < 20u; ++frame) api.run();
    if (frontend.frames != 20u || frontend.polls != 20u ||
        frontend.state_queries == 0u || !frontend.callback_shape_ok ||
        frontend.nonzero_pixels == 0u ||
        frontend.neutral_marker_pixel != UINT32_C(0x0000FF00) ||
        frontend.background_pixel != UINT32_C(0x00101010)) {
        fprintf(stderr, "libretro-test: tracer failed frames=%u polls=%u queries=%u nonzero=%u\n",
                frontend.frames, frontend.polls, frontend.state_queries,
                frontend.nonzero_pixels);
        api.unload_game();
        api.deinit();
        free(content);
        (void)dlclose(library);
        return 1;
    }
    for (frame = 0u; frame < 10u; ++frame) {
        if (frontend.queries_per_key[frame] != 20u) {
            fprintf(stderr, "libretro-test: input mapping query missing for GBA bit %u\n",
                    frame);
            api.unload_game();
            api.deinit();
            free(content);
            (void)dlclose(library);
            return 1;
        }
    }
    if (!frontend.poll_preceded_queries || api.serialize_size() != 0u ||
        api.serialize(NULL, 0u) || api.unserialize(NULL, 0u) ||
        api.get_memory_data(RETRO_MEMORY_SAVE_RAM) != NULL ||
        api.get_memory_size(RETRO_MEMORY_SAVE_RAM) != 0u ||
        api.get_region() != RETRO_REGION_NTSC ||
        !frontend.input_descriptors_ok || frontend.format_negotiations < 3u) {
        fputs("libretro-test: unsupported feature or input callback contract mismatch\n",
              stderr);
        api.unload_game();
        api.deinit();
        free(content);
        (void)dlclose(library);
        return 1;
    }
    expected_digest = frontend.frame_digest;
    api.reset();
    for (frame = 0u; frame < 20u; ++frame) api.run();
    if (frontend.frame_digest != expected_digest) {
        fputs("libretro-test: reset/replay changed completed frame pixels\n", stderr);
        api.unload_game(); api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    api.unload_game();
    if (!api.load_game(&game)) {
        fputs("libretro-test: reload failed after unload\n", stderr);
        api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    for (frame = 0u; frame < 20u; ++frame) api.run();
    if (frontend.frame_digest != expected_digest) {
        fputs("libretro-test: fresh reload changed completed frame pixels\n", stderr);
        api.unload_game(); api.deinit(); free(content); (void)dlclose(library);
        return 1;
    }
    printf("PASS libretro.tracer frames=%u nonzero_pixels=%u sample=%08" PRIx32
           " digest=%016" PRIx64 "\n", frontend.frames,
           frontend.nonzero_pixels, frontend.sampled_pixel,
           frontend.frame_digest);
    api.unload_game();
    api.deinit();
    free(content);
    (void)dlclose(library);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "tracer") == 0) {
        return run_tracer(argv[2], argv[3]);
    }
    fprintf(stderr, "usage: %s tracer MODULE CONTENT.gba\n", argv[0]);
    return 2;
}
