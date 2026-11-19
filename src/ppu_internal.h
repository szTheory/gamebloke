#ifndef GBL_PPU_INTERNAL_H
#define GBL_PPU_INTERNAL_H

#include "gamebloke/gamebloke.h"

typedef enum gbl_ppu_sample_status {
    GBL_PPU_SAMPLE_STATUS_OK = 0,
    GBL_PPU_SAMPLE_STATUS_INVALID_ARGUMENT = 1,
    GBL_PPU_SAMPLE_STATUS_INVALID_REGION = 2,
    GBL_PPU_SAMPLE_STATUS_INVALID_SPAN = 3,
    GBL_PPU_SAMPLE_STATUS_OUTPUT_TOO_SMALL = 4,
    GBL_PPU_SAMPLE_STATUS_INVALID_PREFIX = 5,
    GBL_PPU_SAMPLE_STATUS_INVALID_WRITE = 6
} gbl_ppu_sample_status;

/* Purely projects one physical bus lane into a mapped subwindow. This shares
 * the canonical PRAM/VRAM/OAM lane effects with the sampled PPU overlay. */
gbl_bus_outcome gbl_bus_project_video_lane(
    uint8_t *window, size_t window_length, uint32_t window_offset,
    gbl_bus_region region, uint16_t dispcnt, uint32_t address,
    uint8_t value, uint8_t request_width);

/* Reads the selected physical memory bytes as they appeared at this row's
 * ordered write prefix. Validation completes before destination bytes change. */
gbl_ppu_sample_status gbl_ppu_sample_read(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_row_capture *capture, gbl_bus_region region,
    uint32_t source_offset, uint8_t access_width, size_t length,
    uint8_t *destination, size_t destination_capacity);

#endif
