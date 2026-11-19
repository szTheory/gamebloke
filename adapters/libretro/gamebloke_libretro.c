#include "gamebloke/gamebloke.h"
#include "libretro.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GBL_RETRO_ROM_MAX ((size_t)UINT32_C(0x02000000))
#define GBL_RETRO_FRAME_PIXELS ((size_t)GBL_PPU_WIDTH * GBL_PPU_VISIBLE_LINES)

static retro_environment_t gbl_retro_environment;
static retro_video_refresh_t gbl_retro_video;
static retro_input_poll_t gbl_retro_input_poll;
static retro_input_state_t gbl_retro_input_state;
static retro_log_printf_t gbl_retro_log;
static gbl_machine *gbl_retro_machine;
static uint32_t gbl_retro_pixels[GBL_RETRO_FRAME_PIXELS];
static uint64_t gbl_retro_next_deadline;
static bool gbl_retro_pixel_format_ready;
static bool gbl_retro_loaded;
static bool gbl_retro_have_frame;
static bool gbl_retro_terminal_reported;

static void *gbl_retro_allocate(void *context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void gbl_retro_release(void *context, void *pointer, size_t size)
{
    (void)context;
    (void)size;
    free(pointer);
}

static struct retro_input_descriptor gbl_retro_input_descriptors[] = {
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_A, "GBA A" },
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_B, "GBA B" },
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_R, "GBA R" },
    { 0u, RETRO_DEVICE_JOYPAD, 0u, RETRO_DEVICE_ID_JOYPAD_L, "GBA L" },
    { 0u, 0u, 0u, 0u, NULL }
};

static void gbl_retro_message(enum retro_log_level level, const char *format,
                              ...)
{
    char message[256];
    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    if (gbl_retro_log != NULL) {
        gbl_retro_log(level, "%s", message);
    } else if (gbl_retro_environment != NULL) {
        struct retro_message notice = { message, 120u };
        (void)gbl_retro_environment(RETRO_ENVIRONMENT_SET_MESSAGE, &notice);
    }
}

static bool gbl_retro_machine_create(void)
{
    const gbl_allocator allocator = {
        NULL, gbl_retro_allocate, gbl_retro_release
    };
    if (gbl_retro_machine != NULL) return true;
    gbl_retro_machine = calloc(1u, sizeof(*gbl_retro_machine));
    if (gbl_retro_machine == NULL) return false;
    if (gbl_machine_init(gbl_retro_machine, &allocator) != GBL_BUS_STATUS_OK) {
        free(gbl_retro_machine);
        gbl_retro_machine = NULL;
        return false;
    }
    return true;
}

static void gbl_retro_machine_destroy(void)
{
    if (gbl_retro_machine != NULL) {
        gbl_machine_dispose(gbl_retro_machine);
        free(gbl_retro_machine);
        gbl_retro_machine = NULL;
    }
    gbl_retro_loaded = false;
    gbl_retro_next_deadline = (uint64_t)GBL_PPU_VISIBLE_LINES *
                              GBL_PPU_LINE_CYCLES;
    gbl_retro_have_frame = false;
    gbl_retro_terminal_reported = false;
    memset(gbl_retro_pixels, 0, sizeof(gbl_retro_pixels));
}

static uint8_t gbl_retro_expand_5(uint16_t component)
{
    return (uint8_t)((component << 3u) | (component >> 2u));
}

static void gbl_retro_copy_completed_frame(void)
{
    size_t index;
    const uint8_t *source;

    if (gbl_retro_machine == NULL ||
        gbl_retro_machine->ppu.capture_state != GBL_VIDEO_CAPTURE_VALID ||
        gbl_retro_machine->ppu.captured_rows != GBL_PPU_VISIBLE_LINES) {
        return;
    }
    source = gbl_retro_machine->video_frame;
    for (index = 0u; index < GBL_RETRO_FRAME_PIXELS; ++index) {
        const size_t byte_index = index * 2u;
        const uint16_t bgr555 = (uint16_t)(source[byte_index] |
            ((uint16_t)source[byte_index + 1u] << 8u));
        const uint8_t red = gbl_retro_expand_5((uint16_t)(bgr555 & 31u));
        const uint8_t green = gbl_retro_expand_5(
            (uint16_t)((bgr555 >> 5u) & 31u));
        const uint8_t blue = gbl_retro_expand_5(
            (uint16_t)((bgr555 >> 10u) & 31u));
        gbl_retro_pixels[index] = ((uint32_t)red << 16u) |
                                  ((uint32_t)green << 8u) | blue;
    }
    gbl_retro_have_frame = true;
}

static uint16_t gbl_retro_sample_buttons(void)
{
    static const unsigned ids[10] = {
        RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B,
        RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
        RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_LEFT,
        RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L
    };
    uint16_t pressed = 0u;
    unsigned index;

    if (gbl_retro_input_state == NULL) return 0u;
    for (index = 0u; index < 10u; ++index) {
        if (gbl_retro_input_state(0u, RETRO_DEVICE_JOYPAD, 0u, ids[index]) != 0) {
            pressed |= (uint16_t)(1u << index);
        }
    }
    return pressed;
}

RETRO_API void retro_set_environment(retro_environment_t callback)
{
    struct retro_log_callback log_callback = { 0 };
    enum retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
    gbl_retro_environment = callback;
    gbl_retro_log = NULL;
    if (callback == NULL) {
        gbl_retro_pixel_format_ready = false;
        return;
    }
    if (callback(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_callback)) {
        gbl_retro_log = log_callback.log;
    }
    gbl_retro_pixel_format_ready =
        callback(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format);
    if (!gbl_retro_pixel_format_ready) {
        gbl_retro_message(RETRO_LOG_ERROR,
                          "Gamebloke requires frontend XRGB8888 support.");
    }
    (void)callback(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS,
                   gbl_retro_input_descriptors);
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t callback)
{
    gbl_retro_video = callback;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t callback)
{
    (void)callback;
}

RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t callback)
{
    (void)callback;
}

RETRO_API void retro_set_input_poll(retro_input_poll_t callback)
{
    gbl_retro_input_poll = callback;
}

RETRO_API void retro_set_input_state(retro_input_state_t callback)
{
    gbl_retro_input_state = callback;
}

RETRO_API unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info *info)
{
    if (info == NULL) return;
    info->library_name = "Gamebloke";
    info->library_version = gbl_version();
    info->valid_extensions = "gba";
    info->need_fullpath = false;
    info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
    if (info == NULL) return;
    info->geometry.base_width = GBL_PPU_WIDTH;
    info->geometry.base_height = GBL_PPU_VISIBLE_LINES;
    info->geometry.max_width = GBL_PPU_WIDTH;
    info->geometry.max_height = GBL_PPU_VISIBLE_LINES;
    info->geometry.aspect_ratio = (float)GBL_PPU_WIDTH /
                                  (float)GBL_PPU_VISIBLE_LINES;
    info->timing.fps = 16777216.0 / (double)GBL_PPU_FRAME_CYCLES;
    info->timing.sample_rate = 32768.0;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
    (void)port;
    (void)device;
}

RETRO_API void retro_init(void)
{
    if (!gbl_retro_machine_create()) {
        gbl_retro_message(RETRO_LOG_ERROR, "Gamebloke could not allocate machine state.");
    }
}

RETRO_API void retro_deinit(void)
{
    gbl_retro_machine_destroy();
    gbl_retro_environment = NULL;
    gbl_retro_video = NULL;
    gbl_retro_input_poll = NULL;
    gbl_retro_input_state = NULL;
    gbl_retro_log = NULL;
    gbl_retro_pixel_format_ready = false;
}

RETRO_API void retro_reset(void)
{
    if (gbl_retro_loaded && gbl_retro_machine != NULL) {
        gbl_machine_reset(gbl_retro_machine);
        gbl_retro_next_deadline = (uint64_t)GBL_PPU_VISIBLE_LINES *
                                  GBL_PPU_LINE_CYCLES;
        gbl_retro_have_frame = false;
        gbl_retro_terminal_reported = false;
        memset(gbl_retro_pixels, 0, sizeof(gbl_retro_pixels));
    }
}

RETRO_API void retro_run(void)
{
    gbl_input_event input_event;
    gbl_run_options options = { 0 };
    gbl_run_result result = { 0 };
    gbl_run_status run_status;
    uint64_t machine_cycle;
    uint64_t budget_slack = 0u;

    if (gbl_retro_input_poll != NULL) gbl_retro_input_poll();
    if (!gbl_retro_loaded || gbl_retro_machine == NULL) {
        if (gbl_retro_video != NULL) {
            gbl_retro_video(gbl_retro_pixels, GBL_PPU_WIDTH,
                            GBL_PPU_VISIBLE_LINES,
                            (size_t)GBL_PPU_WIDTH * sizeof(uint32_t));
        }
        return;
    }

    machine_cycle = gbl_retro_machine->master_cycles;
    input_event.cycle = machine_cycle;
    input_event.pressed_mask = gbl_retro_sample_buttons();
    if (gbl_machine_set_input_timeline(gbl_retro_machine, &input_event, 1u) !=
        GBL_TIME_STATUS_OK) {
        if (!gbl_retro_terminal_reported) {
            gbl_retro_message(RETRO_LOG_ERROR,
                              "Gamebloke could not install input at the current emulated cycle.");
            gbl_retro_terminal_reported = true;
        }
        goto deliver;
    }

    if (gbl_retro_next_deadline <= machine_cycle) {
        if (gbl_retro_next_deadline > UINT64_MAX - GBL_PPU_FRAME_CYCLES) {
            gbl_retro_terminal_reported = true;
            gbl_retro_message(RETRO_LOG_ERROR, "Gamebloke frame deadline overflow.");
            goto deliver;
        }
        gbl_retro_next_deadline += GBL_PPU_FRAME_CYCLES;
    }
    while (gbl_retro_machine->master_cycles < gbl_retro_next_deadline) {
        const uint64_t remaining = gbl_retro_next_deadline -
                                   gbl_retro_machine->master_cycles;
        if (remaining > UINT64_MAX - budget_slack) {
            gbl_retro_terminal_reported = true;
            gbl_retro_message(RETRO_LOG_ERROR,
                              "Gamebloke run budget overflow.");
            goto deliver;
        }
        options.cycle_budget = remaining + budget_slack;
        run_status = gbl_cpu_run(gbl_retro_machine, &options, &result);
        if (run_status != GBL_RUN_STATUS_OK ||
            result.termination == GBL_RUN_STOPPED) {
            if (!gbl_retro_terminal_reported) {
                gbl_retro_message(RETRO_LOG_ERROR,
                    "Gamebloke guest stopped (run=%u termination=%u reason=%u cycles=%" PRIu64 ").",
                    (unsigned)run_status, (unsigned)result.termination,
                    (unsigned)result.reason, result.cycles);
                gbl_retro_terminal_reported = true;
            }
            goto deliver;
        }
        if (result.cycles == 0u) {
            if (budget_slack >= UINT64_C(512)) {
                gbl_retro_terminal_reported = true;
                gbl_retro_message(RETRO_LOG_ERROR,
                                  "Gamebloke CPU made no progress at the frame deadline.");
                goto deliver;
            }
            ++budget_slack;
        }
    }
    if (gbl_retro_next_deadline <= UINT64_MAX - GBL_PPU_FRAME_CYCLES) {
        gbl_retro_next_deadline += GBL_PPU_FRAME_CYCLES;
    } else {
        gbl_retro_message(RETRO_LOG_ERROR, "Gamebloke frame deadline exhausted.");
        gbl_retro_terminal_reported = true;
    }
    gbl_retro_copy_completed_frame();

deliver:
    if (gbl_retro_video != NULL) {
        if (gbl_retro_terminal_reported && gbl_retro_have_frame) {
            gbl_retro_video(NULL, GBL_PPU_WIDTH, GBL_PPU_VISIBLE_LINES,
                            (size_t)GBL_PPU_WIDTH * sizeof(uint32_t));
        } else {
            gbl_retro_video(gbl_retro_pixels, GBL_PPU_WIDTH,
                            GBL_PPU_VISIBLE_LINES,
                            (size_t)GBL_PPU_WIDTH * sizeof(uint32_t));
        }
    }
}

RETRO_API size_t retro_serialize_size(void)
{
    return 0u;
}

RETRO_API bool retro_serialize(void *data, size_t size)
{
    (void)data;
    (void)size;
    return false;
}

RETRO_API bool retro_unserialize(const void *data, size_t size)
{
    (void)data;
    (void)size;
    return false;
}

RETRO_API void retro_cheat_reset(void)
{
}

RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void)index;
    (void)enabled;
    (void)code;
}

RETRO_API bool retro_load_game(const struct retro_game_info *game)
{
    gbl_load_result load;

    if (game == NULL || game->data == NULL || game->size < 0xC0u ||
        game->size > GBL_RETRO_ROM_MAX || (game->size & 1u) != 0u ||
        !gbl_retro_pixel_format_ready) {
        gbl_retro_machine_destroy();
        return false;
    }
    if (!gbl_retro_machine_create()) {
        return false;
    }
    gbl_machine_reset(gbl_retro_machine);
    load = gbl_load_rom(gbl_retro_machine, game->data, game->size);
    if (load.status != GBL_LOAD_STATUS_OK) {
        gbl_retro_machine_destroy();
        return false;
    }
    gbl_retro_loaded = true;
    gbl_retro_next_deadline = (uint64_t)GBL_PPU_VISIBLE_LINES *
                              GBL_PPU_LINE_CYCLES;
    gbl_retro_have_frame = false;
    gbl_retro_terminal_reported = false;
    memset(gbl_retro_pixels, 0, sizeof(gbl_retro_pixels));
    gbl_retro_message(RETRO_LOG_INFO,
        "Gamebloke local BIOS-free .gba content loaded (%zu bytes); audio, saves, and serialization are unsupported.",
        game->size);
    return true;
}

RETRO_API bool retro_load_game_special(unsigned game_type,
                                       const struct retro_game_info *info,
                                       size_t num_info)
{
    (void)game_type;
    (void)info;
    (void)num_info;
    return false;
}

RETRO_API void retro_unload_game(void)
{
    gbl_retro_machine_destroy();
}

RETRO_API unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

RETRO_API void *retro_get_memory_data(unsigned id)
{
    (void)id;
    return NULL;
}

RETRO_API size_t retro_get_memory_size(unsigned id)
{
    (void)id;
    return 0u;
}
