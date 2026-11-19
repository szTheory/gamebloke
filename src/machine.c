#include "gamebloke/gamebloke.h"
#include "ppu_internal.h"

#include <string.h>

#define GBL_KEYPAD_INPUT_MASK UINT16_C(0x03FF)
#define GBL_KEYPAD_CONTROL_MASK UINT16_C(0xC3FF)
#define GBL_KEYPAD_IRQ_BIT UINT16_C(0x1000)
#define GBL_KEYPAD_EVENT_PRIORITY 4u
#define GBL_DMA_EVENT_PRIORITY_BASE 5u
#define GBL_PPU_EVENT_PRIORITY 9u
#define GBL_PPU_TRANSFORM_REGISTER_BYTES_MASK UINT32_C(0x0000FFFF)
#define GBL_PPU_EFFECT_REGISTER_BYTES_MASK UINT32_C(0x00FF0000)

static bool gbl_event_precedes(const gbl_event *left,
                               const gbl_event *right)
{
    if (left->cycle != right->cycle) {
        return left->cycle < right->cycle;
    }
    if (left->priority != right->priority) {
        return left->priority < right->priority;
    }
    return left->sequence < right->sequence;
}

static void gbl_preview_remove_event(gbl_machine_time_preview *preview,
                                     size_t index)
{
    size_t cursor;

    for (cursor = index + 1u; cursor < preview->event_count; ++cursor) {
        preview->events[cursor - 1u] = preview->events[cursor];
    }
    if (preview->event_count > 0u) {
        --preview->event_count;
        preview->events[preview->event_count] = (gbl_event){ 0 };
    }
}

static void gbl_preview_remove_timer_event(
    gbl_machine_time_preview *preview, uint8_t timer_index)
{
    size_t index = 0u;

    while (index < preview->event_count) {
        const gbl_event *event = &preview->events[index];

        if (event->kind == (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW &&
            event->timer_index == timer_index) {
            gbl_preview_remove_event(preview, index);
        } else {
            ++index;
        }
    }
}

static void gbl_preview_remove_input_event(
    gbl_machine_time_preview *preview)
{
    size_t index = 0u;

    while (index < preview->event_count) {
        if (preview->events[index].kind ==
            (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT) {
            gbl_preview_remove_event(preview, index);
        } else {
            ++index;
        }
    }
}

static void gbl_preview_remove_dma_events(
    gbl_machine_time_preview *preview, uint8_t channel_index)
{
    size_t index = 0u;

    while (index < preview->event_count) {
        const gbl_event *event = &preview->events[index];

        if (event->kind == (uint8_t)GBL_EVENT_KIND_DMA_READY &&
            event->dma_channel == channel_index) {
            gbl_preview_remove_event(preview, index);
        } else {
            ++index;
        }
    }
}

static gbl_time_status gbl_preview_insert_timer_event(
    gbl_machine_time_preview *preview, uint8_t timer_index, uint64_t cycle)
{
    gbl_event event = { 0 };
    size_t position;

    if (preview->event_count >= GBL_EVENT_CAPACITY) {
        return GBL_TIME_STATUS_EVENT_CAPACITY;
    }
    if (preview->next_event_sequence == UINT64_MAX) {
        return GBL_TIME_STATUS_SEQUENCE_EXHAUSTED;
    }
    event.cycle = cycle;
    event.sequence = preview->next_event_sequence;
    event.generation = preview->timers[timer_index].generation;
    event.priority = timer_index;
    event.kind = (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW;
    event.timer_index = timer_index;
    event.input_index = 0u;
    event.live = true;
    ++preview->next_event_sequence;

    position = preview->event_count;
    while (position > 0u &&
           gbl_event_precedes(&event, &preview->events[position - 1u])) {
        preview->events[position] = preview->events[position - 1u];
        --position;
    }
    preview->events[position] = event;
    ++preview->event_count;
    return GBL_TIME_STATUS_OK;
}

static gbl_time_status gbl_preview_insert_input_event(
    gbl_machine_time_preview *preview, size_t input_index)
{
    gbl_event event = { 0 };
    size_t position;

    if (preview->event_count >= GBL_EVENT_CAPACITY) {
        return GBL_TIME_STATUS_EVENT_CAPACITY;
    }
    if (preview->next_event_sequence == UINT64_MAX) {
        return GBL_TIME_STATUS_SEQUENCE_EXHAUSTED;
    }
    if (input_index >= preview->input_event_count ||
        input_index > UINT16_MAX) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    event.cycle = preview->input_events[input_index].cycle;
    event.sequence = preview->next_event_sequence;
    event.generation = preview->input_generation;
    event.priority = GBL_KEYPAD_EVENT_PRIORITY;
    event.kind = (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT;
    event.input_index = (uint16_t)input_index;
    event.live = true;
    ++preview->next_event_sequence;

    position = preview->event_count;
    while (position > 0u &&
           gbl_event_precedes(&event, &preview->events[position - 1u])) {
        preview->events[position] = preview->events[position - 1u];
        --position;
    }
    preview->events[position] = event;
    ++preview->event_count;
    return GBL_TIME_STATUS_OK;
}

static gbl_time_status gbl_preview_insert_dma_event(
    gbl_machine_time_preview *preview, uint8_t channel_index,
    uint64_t cycle, uint64_t generation,
    gbl_dma_trigger_origin trigger_origin,
    gbl_dma_dispute_id trigger_dispute)
{
    gbl_event event = { 0 };
    size_t position;
    size_t index;

    if (preview->event_count >= GBL_EVENT_CAPACITY) {
        return GBL_TIME_STATUS_EVENT_CAPACITY;
    }
    if (preview->next_event_sequence == UINT64_MAX) {
        return GBL_TIME_STATUS_SEQUENCE_EXHAUSTED;
    }
    if (channel_index >= GBL_DMA_CHANNEL_COUNT ||
        cycle < preview->master_cycles ||
        preview->dma_channels[channel_index].generation != generation ||
        !preview->dma_channels[channel_index].armed) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < preview->event_count; ++index) {
        const gbl_event *existing = &preview->events[index];

        if (existing->kind == (uint8_t)GBL_EVENT_KIND_DMA_READY &&
            existing->dma_channel == channel_index &&
            existing->generation == generation) {
            return GBL_TIME_STATUS_INPUT_OVERLAP;
        }
    }
    event.cycle = cycle;
    event.sequence = preview->next_event_sequence;
    event.generation = generation;
    event.priority = (uint8_t)(GBL_DMA_EVENT_PRIORITY_BASE + channel_index);
    event.kind = (uint8_t)GBL_EVENT_KIND_DMA_READY;
    event.dma_channel = channel_index;
    event.live = true;
    ++preview->next_event_sequence;

    position = preview->event_count;
    while (position > 0u &&
           gbl_event_precedes(&event, &preview->events[position - 1u])) {
        preview->events[position] = preview->events[position - 1u];
        --position;
    }
    preview->events[position] = event;
    ++preview->event_count;
    preview->dma_channels[channel_index].trigger_origin = trigger_origin;
    preview->dma_channels[channel_index].trigger_cycle = cycle;
    preview->dma_channels[channel_index].trigger_dispute = trigger_dispute;
    return GBL_TIME_STATUS_OK;
}

static gbl_time_status gbl_preview_queue_ppu_dma(
    gbl_machine_time_preview *preview, uint64_t cycle, bool vblank)
{
    const uint8_t start_mode = vblank ? 1u : 2u;
    const gbl_dma_trigger_origin origin =
        vblank ? GBL_DMA_TRIGGER_ORIGIN_VBLANK
               : GBL_DMA_TRIGGER_ORIGIN_HBLANK;
    const gbl_dma_dispute_id dispute =
        vblank ? GBL_DMA_DISPUTE_NONE
               : GBL_DMA_DISPUTE_HBLANK_FLAG_EDGE;
    uint8_t channel_index;

    for (channel_index = 0u; channel_index < GBL_DMA_CHANNEL_COUNT;
         ++channel_index) {
        const gbl_dma_channel_state *channel =
            &preview->dma_channels[channel_index];
        gbl_time_status status;

        if (!channel->armed ||
            (channel->control & UINT16_C(0x8000)) == 0u ||
            ((channel->control >> 12u) & 3u) != start_mode) {
            continue;
        }
        status = gbl_preview_insert_dma_event(
            preview, channel_index, cycle, channel->generation, origin,
            dispute);
        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
    }
    return GBL_TIME_STATUS_OK;
}

static gbl_time_status gbl_preview_insert_ppu_event(
    gbl_machine_time_preview *preview, uint64_t cycle)
{
    gbl_event event = { 0 };
    size_t position;
    size_t index;

    if (preview->event_count >= GBL_EVENT_CAPACITY) {
        return GBL_TIME_STATUS_EVENT_CAPACITY;
    }
    if (preview->next_event_sequence == UINT64_MAX) {
        return GBL_TIME_STATUS_SEQUENCE_EXHAUSTED;
    }
    if (cycle < preview->master_cycles || preview->ppu.generation == 0u) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0u; index < preview->event_count; ++index) {
        const gbl_event *existing = &preview->events[index];

        if (existing->kind == (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY &&
            existing->generation == preview->ppu.generation) {
            return GBL_TIME_STATUS_INPUT_OVERLAP;
        }
    }
    event.cycle = cycle;
    event.sequence = preview->next_event_sequence;
    event.generation = preview->ppu.generation;
    event.priority = GBL_PPU_EVENT_PRIORITY;
    event.kind = (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY;
    event.live = true;
    ++preview->next_event_sequence;

    position = preview->event_count;
    while (position > 0u &&
           gbl_event_precedes(&event, &preview->events[position - 1u])) {
        preview->events[position] = preview->events[position - 1u];
        --position;
    }
    preview->events[position] = event;
    ++preview->event_count;
    return GBL_TIME_STATUS_OK;
}

static uint64_t gbl_ppu_row_digest(const uint8_t *bytes, size_t length)
{
    uint64_t digest = UINT64_C(14695981039346656037);
    size_t index;

    for (index = 0u; index < length; ++index) {
        digest ^= bytes[index];
        digest *= UINT64_C(1099511628211);
    }
    return digest;
}

static void gbl_ppu_apply_status(gbl_machine_time_preview *preview,
                                 uint16_t next_status)
{
    const uint16_t old_status =
        (uint16_t)(preview->ppu.dispstat & UINT16_C(0x0007));
    const uint16_t enables =
        (uint16_t)(preview->ppu.dispstat & UINT16_C(0x0038));

    if ((old_status & UINT16_C(0x0001)) == 0u &&
        (next_status & UINT16_C(0x0001)) != 0u &&
        (enables & UINT16_C(0x0008)) != 0u) {
        preview->interrupt_flags |= UINT16_C(0x0001);
    }
    if ((old_status & UINT16_C(0x0002)) == 0u &&
        (next_status & UINT16_C(0x0002)) != 0u &&
        (enables & UINT16_C(0x0010)) != 0u) {
        preview->interrupt_flags |= UINT16_C(0x0002);
    }
    if ((old_status & UINT16_C(0x0004)) == 0u &&
        (next_status & UINT16_C(0x0004)) != 0u &&
        (enables & UINT16_C(0x0020)) != 0u) {
        preview->interrupt_flags |= UINT16_C(0x0004);
    }
    preview->ppu.dispstat =
        (uint16_t)(enables | (next_status & UINT16_C(0x0007)));
}

static void gbl_ppu_refresh_compare_status(
    gbl_machine_time_preview *preview)
{
    uint16_t next_status =
        (uint16_t)(preview->ppu.dispstat & UINT16_C(0x0007));

    next_status &= (uint16_t)~UINT16_C(0x0004);
    if (preview->ppu.line == preview->ppu.compare) {
        next_status |= UINT16_C(0x0004);
    }
    gbl_ppu_apply_status(preview, next_status);
}

static const uint8_t *gbl_ppu_video_memory(
    const gbl_machine *machine, gbl_bus_region region, size_t *size_out)
{
    if (size_out == NULL) {
        return NULL;
    }
    switch (region) {
    case GBL_BUS_REGION_PRAM:
        *size_out = GBL_PRAM_SIZE;
        return machine->pram;
    case GBL_BUS_REGION_VRAM:
        *size_out = GBL_VRAM_SIZE;
        return machine->vram;
    case GBL_BUS_REGION_OAM:
        *size_out = GBL_OAM_SIZE;
        return machine->oam;
    case GBL_BUS_REGION_BIOS:
    case GBL_BUS_REGION_EWRAM:
    case GBL_BUS_REGION_IWRAM:
    case GBL_BUS_REGION_IO:
    case GBL_BUS_REGION_ROM:
    case GBL_BUS_REGION_UNMAPPED:
    case GBL_BUS_REGION_COUNT:
        break;
    }
    return NULL;
}

static bool gbl_ppu_operation_touches_video(uint32_t address, uint8_t width);

static bool gbl_ppu_write_log_valid(
    const gbl_machine *machine, const gbl_machine_time_preview *preview)
{
    size_t index;

    if (machine == NULL || preview == NULL ||
        machine->master_cycles > preview->master_cycles ||
        preview->ppu_memory_write_count > GBL_TIME_OPERATION_MAX) {
        return false;
    }
    for (index = 0u; index < preview->ppu_memory_write_count; ++index) {
        const gbl_ppu_memory_write *write =
            &preview->ppu_memory_writes[index];

        if ((write->width != 1u && write->width != 2u &&
             write->width != 4u) ||
            write->address > UINT32_MAX - ((uint32_t)write->width - 1u) ||
            write->cycle < machine->master_cycles ||
            write->cycle > preview->master_cycles ||
            (index > 0u && preview->ppu_memory_writes[index - 1u].cycle >=
                               write->cycle) ||
            write->region !=
                (uint8_t)gbl_bus_region_for_address(write->address) ||
            !gbl_ppu_operation_touches_video(write->address, write->width)) {
            return false;
        }
    }
    return true;
}

static bool gbl_ppu_capture_prefix_valid(
    const gbl_machine_time_preview *preview,
    const gbl_ppu_row_capture *capture)
{
    size_t expected_prefix = 0u;
    size_t index;

    if (preview == NULL || capture == NULL ||
        preview->ppu_memory_write_count > GBL_TIME_OPERATION_MAX ||
        capture->prior_write_count > preview->ppu_memory_write_count) {
        return false;
    }
    for (index = 0u; index < preview->ppu_memory_write_count; ++index) {
        if (preview->ppu_memory_writes[index].cycle < capture->cycle) {
            ++expected_prefix;
        }
    }
    return expected_prefix == (size_t)capture->prior_write_count;
}

gbl_ppu_sample_status gbl_ppu_sample_read(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_row_capture *capture, gbl_bus_region region,
    uint32_t source_offset, uint8_t access_width, size_t length,
    uint8_t *destination, size_t destination_capacity)
{
    const uint8_t *source;
    size_t source_size = 0u;
    size_t write_index;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        capture == NULL || destination == NULL || length == 0u ||
        (access_width != 1u && access_width != 2u && access_width != 4u) ||
        length % (size_t)access_width != 0u) {
        return GBL_PPU_SAMPLE_STATUS_INVALID_ARGUMENT;
    }
    if (region != GBL_BUS_REGION_PRAM && region != GBL_BUS_REGION_VRAM &&
        region != GBL_BUS_REGION_OAM) {
        return GBL_PPU_SAMPLE_STATUS_INVALID_REGION;
    }
    if (destination_capacity < length) {
        return GBL_PPU_SAMPLE_STATUS_OUTPUT_TOO_SMALL;
    }
    if (capture->line >= GBL_PPU_VISIBLE_LINES ||
        capture->frame_epoch != preview->ppu.frame_epoch ||
        length - 1u > (size_t)(UINT32_MAX - source_offset)) {
        return GBL_PPU_SAMPLE_STATUS_INVALID_SPAN;
    }
    if (preview->ppu_memory_write_count > GBL_TIME_OPERATION_MAX ||
        capture->prior_write_count > preview->ppu_memory_write_count) {
        return GBL_PPU_SAMPLE_STATUS_INVALID_PREFIX;
    }
    if (!gbl_ppu_write_log_valid(machine, preview)) {
        return GBL_PPU_SAMPLE_STATUS_INVALID_WRITE;
    }
    if (!gbl_ppu_capture_prefix_valid(preview, capture)) {
        return GBL_PPU_SAMPLE_STATUS_INVALID_PREFIX;
    }
    source = gbl_ppu_video_memory(machine, region, &source_size);
    if (source == NULL || (size_t)source_offset > source_size ||
        length > source_size - (size_t)source_offset) {
        return GBL_PPU_SAMPLE_STATUS_INVALID_SPAN;
    }
    memcpy(destination, &source[source_offset], length);
    for (write_index = 0u;
         write_index < (size_t)capture->prior_write_count;
         ++write_index) {
        const gbl_ppu_memory_write *write =
            &preview->ppu_memory_writes[write_index];
        uint32_t lane;

        for (lane = 0u; lane < (uint32_t)write->width; ++lane) {
            const uint32_t address = write->address + lane;

            if (gbl_bus_region_for_address(address) == region) {
                (void)gbl_bus_project_video_lane(
                    destination, length, source_offset, region,
                    write->dispcnt, address,
                    (uint8_t)((write->value >> (lane * 8u)) &
                              UINT32_C(0xFF)),
                    write->width);
            }
        }
    }
    return GBL_PPU_SAMPLE_STATUS_OK;
}

static bool gbl_ppu_project_row(const gbl_machine *machine,
                               const gbl_machine_time_preview *preview,
                               const gbl_ppu_row_capture *capture,
                               uint8_t row[GBL_PPU_ROW_BYTES])
{
    const uint16_t line = capture->line;
    const uint32_t row_offset = (uint32_t)line * GBL_PPU_ROW_BYTES;
    uint32_t pixel;

    if ((capture->dispcnt & UINT16_C(0x0007)) != UINT16_C(3) ||
        (capture->dispcnt & UINT16_C(0x0400)) == 0u) {
        memset(row, 0, GBL_PPU_ROW_BYTES);
        return true;
    }
    if (gbl_ppu_sample_read(
            machine, preview, capture, GBL_BUS_REGION_VRAM, row_offset, 2u,
            GBL_PPU_ROW_BYTES, row, GBL_PPU_ROW_BYTES) !=
        GBL_PPU_SAMPLE_STATUS_OK) {
        return false;
    }
    for (pixel = 0u; pixel < GBL_PPU_WIDTH; ++pixel) {
        const uint32_t offset = pixel * 2u;
        const uint16_t color = (uint16_t)(
            ((uint16_t)row[offset]) |
            (uint16_t)((uint16_t)row[offset + 1u] << 8u));
        const uint16_t visible_color = (uint16_t)(color & UINT16_C(0x7FFF));

        row[offset] = (uint8_t)(visible_color & UINT16_C(0x00FF));
        row[offset + 1u] = (uint8_t)(visible_color >> 8u);
    }
    return true;
}

static gbl_video_invalid_reason gbl_ppu_dispcnt_invalid_reason(
    uint16_t dispcnt)
{
    if ((dispcnt & UINT16_C(0x0007)) != UINT16_C(3)) {
        return GBL_VIDEO_INVALID_REASON_UNSUPPORTED_MODE;
    }
    if ((dispcnt & UINT16_C(0x0400)) == 0u) {
        return GBL_VIDEO_INVALID_REASON_BG2_DISABLED;
    }
    if ((dispcnt & UINT16_C(0x0080)) != 0u) {
        return GBL_VIDEO_INVALID_REASON_FORCED_BLANK;
    }
    if ((dispcnt & UINT16_C(0x1000)) != 0u) {
        return GBL_VIDEO_INVALID_REASON_OBJ_ENABLED;
    }
    if ((dispcnt & UINT16_C(0xE000)) != 0u) {
        return GBL_VIDEO_INVALID_REASON_WINDOW_ENABLED;
    }
    return GBL_VIDEO_INVALID_REASON_NONE;
}

static gbl_video_invalid_reason gbl_ppu_active_invalid_reason(
    const gbl_ppu_timing_state *ppu)
{
    const gbl_video_invalid_reason dispcnt_reason =
        gbl_ppu_dispcnt_invalid_reason(ppu->dispcnt);

    if (dispcnt_reason != GBL_VIDEO_INVALID_REASON_NONE) {
        return dispcnt_reason;
    }
    if ((ppu->unsupported_register_bytes &
         GBL_PPU_TRANSFORM_REGISTER_BYTES_MASK) != 0u) {
        return GBL_VIDEO_INVALID_REASON_BG2_TRANSFORM_UNMODELED;
    }
    if ((ppu->unsupported_register_bytes &
         GBL_PPU_EFFECT_REGISTER_BYTES_MASK) != 0u) {
        return GBL_VIDEO_INVALID_REASON_EFFECT_ENABLED;
    }
    return GBL_VIDEO_INVALID_REASON_NONE;
}

static uint8_t gbl_ppu_transform_neutral_byte(uint32_t bit_index)
{
    if (bit_index == UINT32_C(1) || bit_index == UINT32_C(7)) {
        return UINT8_C(1);
    }
    return 0u;
}

static void gbl_ppu_update_register_byte_deviation(
    uint32_t *mask, uint32_t bit_index, uint8_t value, uint8_t neutral_value)
{
    const uint32_t bit = UINT32_C(1) << bit_index;

    if (value == neutral_value) {
        *mask &= ~bit;
    } else {
        *mask |= bit;
    }
}

static void gbl_ppu_invalidate_capture(
    gbl_machine_time_preview *preview,
    gbl_video_invalid_reason reason)
{
    if (reason == GBL_VIDEO_INVALID_REASON_NONE) {
        return;
    }
    if (preview->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_NONE ||
        preview->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_INCOMPLETE) {
        preview->ppu.capture_reason = reason;
    }
    preview->ppu.capture_state = GBL_VIDEO_CAPTURE_INVALID;
}

static bool gbl_ppu_all_rows_captured(const gbl_ppu_timing_state *ppu)
{
    size_t index;

    if (ppu->captured_rows != GBL_PPU_VISIBLE_LINES) {
        return false;
    }
    for (index = 0u; index < GBL_PPU_ROW_COVERAGE_BYTES; ++index) {
        if (ppu->row_coverage[index] != UINT8_MAX) {
            return false;
        }
    }
    return true;
}

static gbl_time_status gbl_ppu_capture_row(
    const gbl_machine *machine, gbl_machine_time_preview *preview)
{
    gbl_ppu_row_capture *capture;
    uint8_t row[GBL_PPU_ROW_BYTES];
    const uint16_t line = preview->ppu.line;
    const uint8_t mask = (uint8_t)(UINT8_C(1) << (line & 7u));
    const size_t coverage_index = (size_t)(line >> 3u);
    const gbl_video_invalid_reason reason =
        gbl_ppu_active_invalid_reason(&preview->ppu);

    if (line >= GBL_PPU_VISIBLE_LINES) {
        return GBL_TIME_STATUS_OK;
    }
    if (reason != GBL_VIDEO_INVALID_REASON_NONE) {
        gbl_ppu_invalidate_capture(preview, reason);
        return GBL_TIME_STATUS_OK;
    }
    if (preview->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID &&
        preview->ppu.capture_reason != GBL_VIDEO_INVALID_REASON_NONE) {
        return GBL_TIME_STATUS_OK;
    }
    if ((preview->ppu.row_coverage[coverage_index] & mask) != 0u) {
        return GBL_TIME_STATUS_OK;
    }
    if (preview->ppu_row_capture_count >= GBL_PPU_ROW_CAPTURE_MAX ||
        preview->ppu_memory_write_count > GBL_TIME_OPERATION_MAX) {
        return GBL_TIME_STATUS_EVENT_CAPACITY;
    }
    capture = &preview->ppu_row_captures[preview->ppu_row_capture_count];
    capture->cycle = preview->ppu.line_origin;
    capture->frame_epoch = preview->ppu.frame_epoch;
    capture->line = line;
    capture->dispcnt = preview->ppu.dispcnt;
    capture->prior_write_count =
        (uint16_t)preview->ppu_memory_write_count;
    capture->unsupported_register_bytes =
        preview->ppu.unsupported_register_bytes;
    memcpy(capture->render_register_bytes,
           preview->ppu.render_register_bytes,
           sizeof(capture->render_register_bytes));
    if (!gbl_ppu_project_row(machine, preview, capture, row)) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    capture->pixel_digest = gbl_ppu_row_digest(row, sizeof(row));
    ++preview->ppu_row_capture_count;
    preview->ppu.row_coverage[coverage_index] |= mask;
    ++preview->ppu.captured_rows;
    if (preview->ppu.capture_state != GBL_VIDEO_CAPTURE_INVALID) {
        preview->ppu.capture_state = GBL_VIDEO_CAPTURE_INCOMPLETE;
        if (preview->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_NONE) {
            preview->ppu.capture_reason = GBL_VIDEO_INVALID_REASON_INCOMPLETE;
        }
    }
    return GBL_TIME_STATUS_OK;
}

static void gbl_ppu_publish_if_complete(
    gbl_machine_time_preview *preview)
{
    if (preview->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID &&
        preview->ppu.capture_reason != GBL_VIDEO_INVALID_REASON_NONE) {
        return;
    }
    if (gbl_ppu_all_rows_captured(&preview->ppu)) {
        preview->ppu.capture_state = GBL_VIDEO_CAPTURE_VALID;
        preview->ppu.capture_reason = GBL_VIDEO_INVALID_REASON_NONE;
    } else {
        preview->ppu.capture_state = GBL_VIDEO_CAPTURE_INCOMPLETE;
        if (preview->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_NONE) {
            preview->ppu.capture_reason = GBL_VIDEO_INVALID_REASON_INCOMPLETE;
        }
    }
}

static gbl_time_status gbl_preview_dispatch_ppu_event(
    const gbl_machine *machine, gbl_machine_time_preview *preview,
    const gbl_event *event)
{
    uint64_t next_cycle;
    gbl_time_status status;

    if (!event->live || event->generation != preview->ppu.generation) {
        return GBL_TIME_STATUS_OK;
    }
    if (preview->ppu.generation == UINT64_MAX) {
        return GBL_TIME_STATUS_GENERATION_EXHAUSTED;
    }
    if (!preview->ppu.next_boundary_hblank) {
        const uint16_t line = preview->ppu.scheduled_line;

        if (line >= GBL_PPU_TOTAL_LINES) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        if (line == 0u && preview->ppu.frame_started) {
            if (preview->ppu.frame_epoch == UINT64_MAX) {
                return GBL_TIME_STATUS_GENERATION_EXHAUSTED;
            }
            ++preview->ppu.frame_epoch;
            preview->ppu_clear_video = true;
            preview->ppu_row_capture_count = 0u;
            preview->ppu.captured_rows = 0u;
            preview->ppu.capture_state = GBL_VIDEO_CAPTURE_INCOMPLETE;
            preview->ppu.capture_reason =
                GBL_VIDEO_INVALID_REASON_INCOMPLETE;
            memset(preview->ppu.row_coverage, 0,
                   sizeof(preview->ppu.row_coverage));
        }
        preview->ppu.frame_started = true;
        preview->ppu.line = line;
        preview->ppu.line_origin = event->cycle;
        {
            uint16_t next_status = (uint16_t)(
                preview->ppu.dispstat & UINT16_C(0x0007));

            next_status &= (uint16_t)~UINT16_C(0x0007);
            if (line >= GBL_PPU_VISIBLE_LINES) {
                next_status |= UINT16_C(0x0001);
            }
            if (line == preview->ppu.compare) {
                next_status |= UINT16_C(0x0004);
            }
            gbl_ppu_apply_status(preview, next_status);
        }
        if (line == GBL_PPU_VISIBLE_LINES) {
            status = gbl_preview_queue_ppu_dma(preview, event->cycle, true);
            if (status != GBL_TIME_STATUS_OK) {
                return status;
            }
        }
        status = gbl_ppu_capture_row(machine, preview);
        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
        if (line == GBL_PPU_VISIBLE_LINES) {
            gbl_ppu_publish_if_complete(preview);
        }
        if (event->cycle > UINT64_MAX - GBL_PPU_HBLANK_CYCLES) {
            return GBL_TIME_STATUS_CYCLE_OVERFLOW;
        }
        next_cycle = event->cycle + GBL_PPU_HBLANK_CYCLES;
        preview->ppu.next_boundary_hblank = true;
    } else {
        if (preview->ppu.line >= GBL_PPU_TOTAL_LINES) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        gbl_ppu_apply_status(
            preview,
            (uint16_t)(preview->ppu.dispstat | UINT16_C(0x0002)));
        if (preview->ppu.line < GBL_PPU_VISIBLE_LINES) {
            status = gbl_preview_queue_ppu_dma(preview, event->cycle, false);
            if (status != GBL_TIME_STATUS_OK) {
                return status;
            }
        }
        preview->ppu.scheduled_line = (uint16_t)(
            (preview->ppu.line + 1u) % GBL_PPU_TOTAL_LINES);
        if (event->cycle > UINT64_MAX -
                               (GBL_PPU_LINE_CYCLES -
                                GBL_PPU_HBLANK_CYCLES)) {
            return GBL_TIME_STATUS_CYCLE_OVERFLOW;
        }
        next_cycle = event->cycle +
                     (GBL_PPU_LINE_CYCLES - GBL_PPU_HBLANK_CYCLES);
        preview->ppu.next_boundary_hblank = false;
    }
    ++preview->ppu.generation;
    return gbl_preview_insert_ppu_event(preview, next_cycle);
}

static gbl_time_status gbl_preview_schedule_next_input(
    gbl_machine_time_preview *preview)
{
    if (preview->input_event_cursor >= preview->input_event_count) {
        return GBL_TIME_STATUS_OK;
    }
    return gbl_preview_insert_input_event(preview,
                                          preview->input_event_cursor);
}

static gbl_time_status gbl_timer_counter_at(
    const gbl_timer_state *timer, uint64_t cycle, uint16_t *counter_out,
    uint16_t *phase_out)
{
    uint64_t elapsed;
    uint64_t scaled_elapsed;
    uint64_t ticks;
    uint64_t total;

    if (timer == NULL || counter_out == NULL || phase_out == NULL ||
        timer->divisor == 0u || cycle < timer->anchor_cycle) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    elapsed = cycle - timer->anchor_cycle;
    if (elapsed > UINT64_MAX - timer->phase) {
        return GBL_TIME_STATUS_CYCLE_OVERFLOW;
    }
    scaled_elapsed = elapsed + timer->phase;
    ticks = timer->enabled && !timer->cascade
                ? scaled_elapsed / timer->divisor
                : 0u;
    total = (uint64_t)timer->counter + ticks;
    *counter_out = (uint16_t)(total & UINT64_C(0xFFFF));
    *phase_out = timer->enabled && !timer->cascade
                     ? (uint16_t)(scaled_elapsed % timer->divisor)
                     : timer->phase;
    return GBL_TIME_STATUS_OK;
}

static gbl_time_status gbl_preview_sync_timer(
    gbl_machine_time_preview *preview, uint8_t timer_index)
{
    gbl_timer_state *timer = &preview->timers[timer_index];
    uint16_t counter;
    uint16_t phase;
    gbl_time_status status;

    status = gbl_timer_counter_at(timer, preview->master_cycles, &counter,
                                  &phase);
    if (status != GBL_TIME_STATUS_OK) {
        return status;
    }
    timer->counter = counter;
    timer->phase = phase;
    timer->anchor_cycle = preview->master_cycles;
    return GBL_TIME_STATUS_OK;
}

static gbl_time_status gbl_preview_schedule_timer(
    gbl_machine_time_preview *preview, uint8_t timer_index)
{
    const gbl_timer_state *timer = &preview->timers[timer_index];
    uint64_t remaining_ticks;
    uint64_t cycle_delta;
    uint64_t deadline;

    if (!timer->enabled || timer->cascade) {
        return GBL_TIME_STATUS_OK;
    }
    if (timer->divisor == 0u || timer->phase >= timer->divisor) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    remaining_ticks = UINT64_C(0x10000) - timer->counter;
    cycle_delta = remaining_ticks * timer->divisor;
    if (cycle_delta <= timer->phase) {
        return GBL_TIME_STATUS_CYCLE_OVERFLOW;
    }
    cycle_delta -= timer->phase;
    if (cycle_delta > UINT64_MAX - timer->anchor_cycle) {
        return GBL_TIME_STATUS_CYCLE_OVERFLOW;
    }
    deadline = timer->anchor_cycle + cycle_delta;
    gbl_preview_remove_timer_event(preview, timer_index);
    return gbl_preview_insert_timer_event(preview, timer_index, deadline);
}

static gbl_time_status gbl_preview_increment_cascade(
    gbl_machine_time_preview *preview, uint8_t source_index, uint64_t cycle)
{
    uint8_t index;

    for (index = (uint8_t)(source_index + 1u); index < GBL_TIMER_COUNT;
         ++index) {
        gbl_timer_state *timer = &preview->timers[index];

        if (!timer->enabled || !timer->cascade) {
            return GBL_TIME_STATUS_OK;
        }
        if (timer->counter != UINT16_MAX) {
            ++timer->counter;
            timer->anchor_cycle = cycle;
            timer->phase = 0u;
            return GBL_TIME_STATUS_OK;
        }
        timer->counter = timer->reload;
        timer->anchor_cycle = cycle;
        timer->phase = 0u;
        if (timer->irq_enabled) {
            preview->interrupt_flags = (uint16_t)(
                preview->interrupt_flags |
                (uint16_t)(UINT16_C(1) << (3u + index)));
        }
        /* A cascade overflow is delivered synchronously as the same carry
         * continues into the next enabled cascade timer. It is not a queued
         * prescaler event and consumes no event sequence number. */
    }
    return GBL_TIME_STATUS_OK;
}

static void gbl_preview_evaluate_keypad(gbl_machine_time_preview *preview)
{
    uint16_t selected;
    uint16_t selected_pressed;
    bool condition;
    bool effective;

    selected = (uint16_t)(preview->key_control & GBL_KEYPAD_INPUT_MASK);
    selected_pressed = (uint16_t)(preview->pressed_keys & selected);
    if (selected == 0u) {
        condition = false;
    } else if ((preview->key_control & UINT16_C(0x8000)) != 0u) {
        condition = selected_pressed == selected;
    } else {
        condition = selected_pressed != 0u;
    }
    effective =
        (preview->key_control & UINT16_C(0x4000)) != 0u && condition;
    if (!preview->key_irq_condition && effective) {
        preview->interrupt_flags = (uint16_t)(
            preview->interrupt_flags | GBL_KEYPAD_IRQ_BIT);
    }
    preview->key_condition = condition;
    preview->key_irq_condition = effective;
}

static gbl_time_status gbl_preview_dispatch_event(
    const gbl_machine *machine, gbl_machine_time_preview *preview,
    const gbl_event *event)
{
    gbl_timer_state *timer;
    gbl_time_status status;

    if (event->kind == (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY) {
        return gbl_preview_dispatch_ppu_event(machine, preview, event);
    }
    if (event->kind == (uint8_t)GBL_EVENT_KIND_DMA_READY) {
        gbl_dma_channel_state *channel;

        if (event->dma_channel >= GBL_DMA_CHANNEL_COUNT) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        channel = &preview->dma_channels[event->dma_channel];
        if (!event->live || event->generation != channel->generation ||
            !channel->armed ||
            (channel->control & UINT16_C(0x8000)) == 0u) {
            return GBL_TIME_STATUS_OK;
        }
        channel->armed = false;
        channel->pending = true;
        return GBL_TIME_STATUS_OK;
    }

    if (event->kind == (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT) {
        size_t input_index = event->input_index;

        if (!event->live || event->generation != preview->input_generation) {
            return GBL_TIME_STATUS_OK;
        }
        if (input_index >= preview->input_event_count ||
            input_index != preview->input_event_cursor ||
            preview->input_events[input_index].cycle != event->cycle) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        preview->pressed_keys = (uint16_t)(
            preview->input_events[input_index].pressed_mask &
            GBL_KEYPAD_INPUT_MASK);
        gbl_preview_evaluate_keypad(preview);
        ++preview->input_event_cursor;
        return gbl_preview_schedule_next_input(preview);
    }
    if (event->kind != (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW ||
        event->timer_index >= GBL_TIMER_COUNT) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    timer = &preview->timers[event->timer_index];
    if (!event->live || event->generation != timer->generation ||
        !timer->enabled || timer->cascade) {
        return GBL_TIME_STATUS_OK;
    }
    timer->counter = timer->reload;
    timer->anchor_cycle = event->cycle;
    timer->phase = 0u;
    if (timer->irq_enabled) {
        preview->interrupt_flags = (uint16_t)(
            preview->interrupt_flags |
            (uint16_t)(UINT16_C(1) << (3u + event->timer_index)));
    }
    status = gbl_preview_increment_cascade(preview, event->timer_index,
                                           event->cycle);
    if (status != GBL_TIME_STATUS_OK) {
        return status;
    }
    return gbl_preview_schedule_timer(preview, event->timer_index);
}

static void gbl_machine_time_copy_preview(
    const gbl_machine *machine, gbl_machine_time_preview *preview)
{
    size_t index;

    *preview = (gbl_machine_time_preview){ 0 };
    preview->master_cycles = machine->master_cycles;
    preview->next_event_sequence = machine->next_event_sequence;
    preview->interrupt_flags = machine->interrupt_flags;
    preview->interrupt_enable = machine->interrupt_enable;
    preview->interrupt_master_enable = machine->interrupt_master_enable;
    preview->waitcnt = machine->waitcnt;
    preview->ppu = machine->ppu;
    preview->ppu.dispcnt = machine->dispcnt;
    preview->bus_owner = machine->bus_owner;
    preview->dma_last_channel = machine->dma_last_channel;
    preview->dma_last_width = machine->dma_last_width;
    preview->dma_last_source = machine->dma_last_source;
    preview->dma_last_destination = machine->dma_last_destination;
    preview->dma_stream_valid = machine->dma_stream_valid;
    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        preview->dma_channels[index] = machine->dma_channels[index];
    }
    preview->pressed_keys = machine->pressed_keys;
    preview->key_control = machine->key_control;
    preview->key_condition = machine->key_condition;
    preview->key_irq_condition = machine->key_irq_condition;
    preview->event_count = machine->event_count;
    for (index = 0u; index < machine->event_count; ++index) {
        preview->events[index] = machine->events[index];
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        preview->timers[index] = machine->timers[index];
    }
    preview->input_generation = machine->input_generation;
    preview->input_event_count = machine->input_event_count;
    preview->input_event_cursor = machine->input_event_cursor;
    for (index = 0u; index < machine->input_event_count; ++index) {
        preview->input_events[index] = machine->input_events[index];
    }
}

static uint32_t gbl_dma_source_mask(uint8_t channel)
{
    return channel == 0u ? UINT32_C(0x07FFFFFF)
                        : UINT32_C(0x0FFFFFFF);
}

static uint32_t gbl_dma_destination_mask(uint8_t channel)
{
    return channel == 3u ? UINT32_C(0x0FFFFFFF)
                         : UINT32_C(0x07FFFFFF);
}

static uint16_t gbl_dma_count_mask(uint8_t channel)
{
    return channel == 3u ? UINT16_MAX : UINT16_C(0x3FFF);
}

static bool gbl_dma_control_valid(uint8_t channel, uint16_t control)
{
    const uint8_t source_mode = (uint8_t)((control >> 7u) & 3u);
    const uint8_t destination_mode = (uint8_t)((control >> 5u) & 3u);
    const bool drq = (control & UINT16_C(0x0800)) != 0u;
    const uint8_t start_mode = (uint8_t)((control >> 12u) & 3u);

    return source_mode != 3u &&
           (destination_mode != 3u || channel == 3u) &&
           (!drq || channel == 3u) && !(start_mode == 3u && channel == 0u);
}

static void gbl_dma_activate(gbl_dma_channel_state *channel,
                             uint8_t channel_index)
{
    const uint32_t width = (channel->control & UINT16_C(0x0400)) != 0u
                               ? 4u
                               : 2u;
    const uint16_t count = (uint16_t)(channel->count &
                                      gbl_dma_count_mask(channel_index));
    const uint32_t source_mask = gbl_dma_source_mask(channel_index);
    const uint32_t destination_mask =
        gbl_dma_destination_mask(channel_index);
    const uint32_t encoded_count = count == 0u
                                       ? (channel_index == 3u
                                              ? UINT32_C(0x10000)
                                              : UINT32_C(0x4000))
                                       : count;

    channel->trigger_origin = GBL_DMA_TRIGGER_ORIGIN_NONE;
    channel->trigger_cycle = 0u;
    channel->trigger_dispute = GBL_DMA_DISPUTE_NONE;
    channel->current_source =
        (channel->source_address & source_mask) & ~(width - 1u);
    channel->current_destination =
        (channel->destination_address & destination_mask) & ~(width - 1u);
    channel->remaining = encoded_count;
    channel->completed_units = 0u;
    channel->active = false;
    channel->startup_complete = false;
    channel->pending = ((channel->control >> 12u) & 3u) == 0u;
    channel->armed = !channel->pending;
}

static gbl_time_status gbl_preview_dma_register_write(
    gbl_machine_time_preview *preview, uint32_t address, uint8_t value)
{
    uint32_t offset;
    uint8_t channel_index;
    uint32_t register_offset;
    gbl_dma_channel_state *channel;
    uint16_t previous_control;

    if (preview == NULL || address < UINT32_C(0x040000B0) ||
        address > UINT32_C(0x040000DF)) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    offset = address - UINT32_C(0x040000B0);
    channel_index = (uint8_t)(offset / UINT32_C(12));
    register_offset = offset % UINT32_C(12);
    if (channel_index >= GBL_DMA_CHANNEL_COUNT) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    channel = &preview->dma_channels[channel_index];
    if (register_offset < 4u) {
        const uint8_t shift = (uint8_t)((register_offset & 3u) * 8u);
        channel->source_address =
            (channel->source_address & ~(UINT32_C(0xFF) << shift)) |
            ((uint32_t)value << shift);
        channel->source_address &= gbl_dma_source_mask(channel_index);
    } else if (register_offset < 8u) {
        const uint8_t shift = (uint8_t)((register_offset & 3u) * 8u);
        channel->destination_address =
            (channel->destination_address & ~(UINT32_C(0xFF) << shift)) |
            ((uint32_t)value << shift);
        channel->destination_address &=
            gbl_dma_destination_mask(channel_index);
    } else if (register_offset < 10u) {
        const uint8_t shift = (uint8_t)((register_offset & 1u) * 8u);
        channel->count = (uint16_t)(
            (channel->count & (uint16_t)~(UINT16_C(0xFF) << shift)) |
            (uint16_t)((uint16_t)value << shift));
        channel->count &= gbl_dma_count_mask(channel_index);
    } else {
        const uint8_t shift = (uint8_t)((register_offset & 1u) * 8u);

        previous_control = channel->control;
        channel->control = (uint16_t)(
            (channel->control & (uint16_t)~(UINT16_C(0xFF) << shift)) |
            (uint16_t)((uint16_t)value << shift));
        channel->control &= UINT16_C(0xFFE0);
        if (!gbl_dma_control_valid(channel_index, channel->control)) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        if ((channel->control & UINT16_C(0x8000)) == 0u) {
            if ((previous_control & UINT16_C(0x8000)) != 0u) {
                if (channel->generation == UINT64_MAX) {
                    return GBL_TIME_STATUS_GENERATION_EXHAUSTED;
                }
                gbl_preview_remove_dma_events(preview, channel_index);
                ++channel->generation;
            }
            channel->pending = false;
            channel->armed = false;
            channel->active = false;
            channel->startup_complete = false;
            channel->remaining = 0u;
            channel->trigger_origin = GBL_DMA_TRIGGER_ORIGIN_NONE;
            channel->trigger_cycle = 0u;
            channel->trigger_dispute = GBL_DMA_DISPUTE_NONE;
        } else if ((previous_control & UINT16_C(0x8000)) == 0u) {
            if (channel->generation == UINT64_MAX) {
                return GBL_TIME_STATUS_GENERATION_EXHAUSTED;
            }
            gbl_preview_remove_dma_events(preview, channel_index);
            ++channel->generation;
            gbl_dma_activate(channel, channel_index);
        }
    }
    return GBL_TIME_STATUS_OK;
}

static gbl_time_status gbl_preview_advance_to(
    const gbl_machine *machine, gbl_machine_time_preview *preview,
    uint64_t target,
    size_t *dispatched)
{
    gbl_machine_time_preview candidate;
    size_t index;

    if (preview == NULL || dispatched == NULL ||
        preview->event_count > GBL_EVENT_CAPACITY ||
        preview->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        preview->input_event_cursor > preview->input_event_count) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (target < preview->master_cycles) {
        return GBL_TIME_STATUS_BACKWARD;
    }
    candidate = *preview;

    while (candidate.event_count > 0u) {
        size_t next_index = 0u;
        gbl_event event;
        gbl_time_status status;

        for (index = 1u; index < candidate.event_count; ++index) {
            if (gbl_event_precedes(&candidate.events[index],
                                  &candidate.events[next_index])) {
                next_index = index;
            }
        }
        if (candidate.events[next_index].cycle > target) {
            break;
        }
        if (*dispatched >= GBL_MACHINE_MAX_EVENT_DISPATCH) {
            return GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT;
        }
        event = candidate.events[next_index];
        gbl_preview_remove_event(&candidate, next_index);
        if (event.kind == (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW) {
            if (event.timer_index >= GBL_TIMER_COUNT) {
                return GBL_TIME_STATUS_INVALID_ARGUMENT;
            }
        } else if (event.kind == (uint8_t)GBL_EVENT_KIND_DMA_READY) {
            if (event.dma_channel >= GBL_DMA_CHANNEL_COUNT) {
                return GBL_TIME_STATUS_INVALID_ARGUMENT;
            }
        } else if (event.kind != (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT &&
                   event.kind != (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        if (event.cycle < candidate.master_cycles) {
            const uint64_t generation =
                event.kind == (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW
                    ? candidate.timers[event.timer_index].generation
                    : event.kind == (uint8_t)GBL_EVENT_KIND_DMA_READY
                          ? candidate.dma_channels[event.dma_channel].generation
                          : event.kind ==
                                    (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY
                                ? candidate.ppu.generation
                          : candidate.input_generation;

            if (event.generation == generation) {
                return GBL_TIME_STATUS_INVALID_ARGUMENT;
            }
        }
        if (event.cycle > candidate.master_cycles) {
            candidate.master_cycles = event.cycle;
        }
        status = gbl_preview_dispatch_event(machine, &candidate, &event);
        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
        ++(*dispatched);
    }
    candidate.master_cycles = target;
    *preview = candidate;
    return GBL_TIME_STATUS_OK;
}

gbl_time_status gbl_machine_prepare_time(
    const gbl_machine *machine, uint64_t target,
    gbl_machine_time_preview *preview)
{
    gbl_machine_time_preview candidate;
    gbl_time_status status;
    size_t dispatched = 0u;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        machine->event_count > GBL_EVENT_CAPACITY ||
        machine->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        machine->input_event_cursor > machine->input_event_count) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    gbl_machine_time_copy_preview(machine, &candidate);
    status = gbl_preview_advance_to(machine, &candidate, target, &dispatched);
    if (status != GBL_TIME_STATUS_OK) {
        return status;
    }
    *preview = candidate;
    return GBL_TIME_STATUS_OK;
}

static bool gbl_preview_bus_span_valid(uint32_t address, uint8_t width)
{
    return (width == 1u || width == 2u || width == 4u) &&
           address <= UINT32_MAX - ((uint32_t)width - 1u);
}

static bool gbl_ppu_operation_touches_video(uint32_t address, uint8_t width)
{
    uint32_t lane;

    for (lane = 0u; lane < (uint32_t)width; ++lane) {
        const gbl_bus_region region =
            gbl_bus_region_for_address(address + lane);

        if (region == GBL_BUS_REGION_PRAM ||
            region == GBL_BUS_REGION_VRAM ||
            region == GBL_BUS_REGION_OAM) {
            return true;
        }
    }
    return false;
}

static gbl_time_status gbl_preview_bus_write(
    gbl_machine_time_preview *preview, uint32_t address, uint8_t width,
    uint32_t value)
{
    uint8_t timer_lane_masks[GBL_TIMER_COUNT] = { 0u };
    uint32_t timer_values[GBL_TIMER_COUNT] = { 0u };
    uint16_t if_clear_mask = 0u;
    uint16_t interrupt_enable = preview->interrupt_enable;
    uint16_t interrupt_master_enable = preview->interrupt_master_enable;
    uint16_t waitcnt = preview->waitcnt;
    uint16_t dispcnt = preview->ppu.dispcnt;
    uint16_t dispstat_control =
        (uint16_t)(preview->ppu.dispstat & UINT16_C(0x0038));
    uint16_t compare = preview->ppu.compare;
    uint16_t key_control = preview->key_control;
    bool key_control_written = false;
    bool waitcnt_written = false;
    bool dispstat_written = false;
    bool dispcnt_written = false;
    bool unsupported_effect_write = false;
    bool unsupported_transform_write = false;
    bool window_register_write = false;
    uint32_t unsupported_register_bytes =
        preview->ppu.unsupported_register_bytes;
    uint8_t render_register_bytes[GBL_PPU_RENDER_REGISTER_BYTES];
    uint32_t lane;

    memcpy(render_register_bytes, preview->ppu.render_register_bytes,
           sizeof(render_register_bytes));

    for (lane = 0u; lane < (uint32_t)width; ++lane) {
        const uint32_t lane_address = address + lane;
        const uint8_t byte_value =
            (uint8_t)((value >> (lane * 8u)) & UINT32_C(0xFF));

        if (lane_address == UINT32_C(0x04000000)) {
            dispcnt = (uint16_t)(
                (dispcnt & UINT16_C(0xFF00)) |
                (uint16_t)byte_value);
            dispcnt_written = true;
        } else if (lane_address == UINT32_C(0x04000001)) {
            dispcnt = (uint16_t)(
                (dispcnt & UINT16_C(0x00FF)) |
                (uint16_t)((uint16_t)byte_value << 8u));
            dispcnt_written = true;
        } else if (lane_address >= UINT32_C(0x04000020) &&
                   lane_address <= UINT32_C(0x0400002F)) {
            const uint32_t register_byte =
                lane_address - UINT32_C(0x04000020);

            gbl_ppu_update_register_byte_deviation(
                &unsupported_register_bytes,
                register_byte, byte_value,
                gbl_ppu_transform_neutral_byte(register_byte));
            render_register_bytes[register_byte] = byte_value;
            unsupported_transform_write = true;
        } else if (lane_address >= UINT32_C(0x04000040) &&
                   lane_address <= UINT32_C(0x0400004B)) {
            window_register_write = true;
        } else if (lane_address >= UINT32_C(0x0400004C) &&
                   lane_address <= UINT32_C(0x0400004D)) {
            gbl_ppu_update_register_byte_deviation(
                &unsupported_register_bytes,
                UINT32_C(16) + lane_address - UINT32_C(0x0400004C),
                byte_value, 0u);
            render_register_bytes[
                16u + (lane_address - UINT32_C(0x0400004C))] = byte_value;
            if (byte_value != 0u) {
                unsupported_effect_write = true;
            }
        } else if (lane_address >= UINT32_C(0x04000050) &&
                   lane_address <= UINT32_C(0x04000055)) {
            gbl_ppu_update_register_byte_deviation(
                &unsupported_register_bytes,
                UINT32_C(18) + lane_address - UINT32_C(0x04000050),
                byte_value, 0u);
            render_register_bytes[
                18u + (lane_address - UINT32_C(0x04000050))] = byte_value;
            if (byte_value != 0u) {
                unsupported_effect_write = true;
            }
        } else if (lane_address == UINT32_C(0x04000004)) {
            dispstat_control = (uint16_t)(
                (dispstat_control & (uint16_t)~UINT16_C(0x0038)) |
                (uint16_t)(byte_value & UINT8_C(0x38)));
            dispstat_written = true;
        } else if (lane_address == UINT32_C(0x04000005)) {
            compare = byte_value;
            dispstat_written = true;
        } else if (lane_address >= UINT32_C(0x04000100) &&
            lane_address <= UINT32_C(0x0400010F)) {
            const uint32_t offset =
                lane_address - UINT32_C(0x04000100);
            const uint8_t timer_index = (uint8_t)(offset / 4u);
            const uint32_t timer_lane = offset % 4u;

            if (timer_lane == 0u) {
                timer_lane_masks[timer_index] |= UINT8_C(0x01);
                timer_values[timer_index] =
                    (timer_values[timer_index] & UINT32_C(0xFFFFFF00)) |
                    byte_value;
            } else if (timer_lane == 1u) {
                timer_lane_masks[timer_index] |= UINT8_C(0x02);
                timer_values[timer_index] =
                    (timer_values[timer_index] & UINT32_C(0xFFFF00FF)) |
                    ((uint32_t)byte_value << 8u);
            } else if (timer_lane == 2u) {
                timer_lane_masks[timer_index] |= UINT8_C(0x04);
                timer_values[timer_index] =
                    (timer_values[timer_index] & UINT32_C(0xFF00FFFF)) |
                    ((uint32_t)byte_value << 16u);
            }
        } else if (lane_address == UINT32_C(0x04000200)) {
            interrupt_enable = (uint16_t)(
                (interrupt_enable & UINT16_C(0xFF00)) | byte_value);
        } else if (lane_address == UINT32_C(0x04000201)) {
            interrupt_enable = (uint16_t)(
                (interrupt_enable & UINT16_C(0x00FF)) |
                (uint16_t)((uint16_t)byte_value << 8u));
        } else if (lane_address == UINT32_C(0x04000202)) {
            if_clear_mask = (uint16_t)(if_clear_mask | byte_value);
        } else if (lane_address == UINT32_C(0x04000203)) {
            if_clear_mask = (uint16_t)(
                if_clear_mask | (uint16_t)((uint16_t)byte_value << 8u));
        } else if (lane_address == UINT32_C(0x04000208)) {
            interrupt_master_enable = (uint16_t)(
                (interrupt_master_enable & UINT16_C(0xFF00)) | byte_value);
        } else if (lane_address == UINT32_C(0x04000209)) {
            interrupt_master_enable = (uint16_t)(
                (interrupt_master_enable & UINT16_C(0x00FF)) |
                (uint16_t)((uint16_t)byte_value << 8u));
        } else if (lane_address == UINT32_C(0x04000204)) {
            waitcnt = (uint16_t)((waitcnt & UINT16_C(0xFF00)) | byte_value);
            waitcnt_written = true;
        } else if (lane_address == UINT32_C(0x04000205)) {
            waitcnt = (uint16_t)((waitcnt & UINT16_C(0x00FF)) |
                                 (uint16_t)((uint16_t)byte_value << 8u));
            waitcnt_written = true;
        } else if (lane_address == UINT32_C(0x04000132)) {
            key_control = (uint16_t)((key_control & UINT16_C(0xFF00)) |
                                     byte_value);
            key_control_written = true;
        } else if (lane_address == UINT32_C(0x04000133)) {
            key_control = (uint16_t)((key_control & UINT16_C(0x00FF)) |
                                     (uint16_t)((uint16_t)byte_value << 8u));
            key_control_written = true;
        } else if (lane_address >= UINT32_C(0x040000B0) &&
                   lane_address <= UINT32_C(0x040000DF)) {
            const gbl_time_status status =
                gbl_preview_dma_register_write(preview, lane_address,
                                               byte_value);

            if (status != GBL_TIME_STATUS_OK) {
                return status;
            }
        }
    }
    for (lane = 0u; lane < GBL_TIMER_COUNT; ++lane) {
        if (timer_lane_masks[lane] != 0u) {
            const gbl_time_status status = gbl_timer_write(
                preview, (uint8_t)lane, timer_lane_masks[lane],
                timer_values[lane]);

            if (status != GBL_TIME_STATUS_OK) {
                return status;
            }
        }
    }
    preview->interrupt_flags = (uint16_t)(
        preview->interrupt_flags &
        (uint16_t)~(if_clear_mask & UINT16_C(0x3FFF)));
    preview->interrupt_enable =
        (uint16_t)(interrupt_enable & UINT16_C(0x3FFF));
    preview->interrupt_master_enable =
        (uint16_t)(interrupt_master_enable & UINT16_C(0x0001));
    if (waitcnt_written) {
        preview->waitcnt = (uint16_t)(waitcnt & UINT16_C(0x5FFF));
    }
    preview->ppu.unsupported_register_bytes = unsupported_register_bytes;
    memcpy(preview->ppu.render_register_bytes, render_register_bytes,
           sizeof(render_register_bytes));
    preview->ppu.dispcnt = dispcnt;
    if (dispcnt_written) {
        gbl_ppu_invalidate_capture(
            preview, gbl_ppu_dispcnt_invalid_reason(dispcnt));
    }
    if (unsupported_transform_write) {
        gbl_ppu_invalidate_capture(
            preview, GBL_VIDEO_INVALID_REASON_BG2_TRANSFORM_UNMODELED);
    } else if (unsupported_effect_write) {
        gbl_ppu_invalidate_capture(
            preview, GBL_VIDEO_INVALID_REASON_EFFECT_ENABLED);
    } else if (window_register_write &&
               (dispcnt & UINT16_C(0xE000)) != 0u) {
        gbl_ppu_invalidate_capture(
            preview, GBL_VIDEO_INVALID_REASON_WINDOW_ENABLED);
    }
    if (dispstat_written) {
        preview->ppu.dispstat = (uint16_t)(
            (preview->ppu.dispstat & UINT16_C(0x0007)) |
            dispstat_control);
        preview->ppu.compare = compare;
        gbl_ppu_refresh_compare_status(preview);
    }
    if (key_control_written) {
        preview->key_control =
            (uint16_t)(key_control & GBL_KEYPAD_CONTROL_MASK);
        gbl_preview_evaluate_keypad(preview);
    }
    return GBL_TIME_STATUS_OK;
}

gbl_time_status gbl_machine_prepare_operations(
    const gbl_machine *machine,
    const gbl_time_operation *operations,
    size_t operation_count, gbl_machine_time_preview *preview)
{
    gbl_machine_time_preview candidate;
    size_t dispatched = 0u;
    size_t index;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        machine->event_count > GBL_EVENT_CAPACITY ||
        machine->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        machine->input_event_cursor > machine->input_event_count ||
        operation_count > GBL_TIME_OPERATION_MAX ||
        (operation_count != 0u && operations == NULL)) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    gbl_machine_time_copy_preview(machine, &candidate);
    for (index = 0u; index < operation_count; ++index) {
        const gbl_time_operation *operation = &operations[index];
        uint64_t target;
        gbl_time_status status;

        if (operation->kind == GBL_TIME_OPERATION_INTERNAL) {
            if (operation->cycles == 0u) {
                continue;
            }
        } else if (operation->kind == GBL_TIME_OPERATION_BUS_READ ||
                   operation->kind == GBL_TIME_OPERATION_BUS_WRITE) {
            if (operation->cycles == 0u ||
                !gbl_preview_bus_span_valid(operation->address,
                                             operation->width)) {
                return GBL_TIME_STATUS_INVALID_ARGUMENT;
            }
        } else {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        if (operation->cycles > UINT64_MAX - candidate.master_cycles) {
            return GBL_TIME_STATUS_CYCLE_OVERFLOW;
        }
        target = candidate.master_cycles + operation->cycles;
        status = gbl_preview_advance_to(machine, &candidate, target,
                                        &dispatched);
        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
        if (operation->kind == GBL_TIME_OPERATION_BUS_WRITE) {
            if (gbl_ppu_operation_touches_video(operation->address,
                                                operation->width)) {
                if (candidate.ppu_memory_write_count >=
                    GBL_TIME_OPERATION_MAX) {
                    return GBL_TIME_STATUS_EVENT_CAPACITY;
                }
                candidate.ppu_memory_writes[
                    candidate.ppu_memory_write_count++] =
                    (gbl_ppu_memory_write){
                        candidate.master_cycles, operation->address,
                        operation->value,
                        candidate.ppu.dispcnt, operation->width,
                        (uint8_t)gbl_bus_region_for_address(
                            operation->address)
                    };
            }
            status = gbl_preview_bus_write(&candidate, operation->address,
                                           operation->width,
                                           operation->value);
            if (status != GBL_TIME_STATUS_OK) {
                return status;
            }
        }
    }
    *preview = candidate;
    return GBL_TIME_STATUS_OK;
}

static void gbl_ppu_materialize_capture_plan(
    gbl_machine *machine, const gbl_machine_time_preview *preview)
{
    size_t index;

    if (preview->ppu_clear_video) {
        memset(machine->video_frame, 0, sizeof(machine->video_frame));
    }
    for (index = 0u; index < preview->ppu_row_capture_count; ++index) {
        const gbl_ppu_row_capture *capture =
            &preview->ppu_row_captures[index];
        uint8_t *row;

        if (capture->line >= GBL_PPU_VISIBLE_LINES ||
            capture->prior_write_count > preview->ppu_memory_write_count) {
            continue;
        }
        row = &machine->video_frame[
            (size_t)capture->line * GBL_PPU_ROW_BYTES];
        if (!gbl_ppu_project_row(machine, preview, capture, row)) {
            continue;
        }
    }
}

static bool gbl_ppu_commit_preview_valid(
    const gbl_machine *machine, const gbl_machine_time_preview *preview)
{
    bool seen_rows[GBL_PPU_VISIBLE_LINES] = { false };
    size_t index;

    if (machine == NULL || preview == NULL ||
        preview->ppu_row_capture_count > GBL_PPU_ROW_CAPTURE_MAX ||
        preview->ppu_memory_write_count > GBL_TIME_OPERATION_MAX ||
        !gbl_ppu_write_log_valid(machine, preview)) {
        return false;
    }
    for (index = 0u; index < preview->ppu_row_capture_count; ++index) {
        const gbl_ppu_row_capture *capture =
            &preview->ppu_row_captures[index];
        const uint8_t row_mask =
            (uint8_t)(UINT8_C(1) << (capture->line & 7u));
        const size_t coverage_index = (size_t)(capture->line >> 3u);
        gbl_ppu_timing_state sampled_ppu = preview->ppu;

        if (capture->line >= GBL_PPU_VISIBLE_LINES ||
            capture->cycle > preview->master_cycles ||
            capture->frame_epoch != preview->ppu.frame_epoch ||
            capture->prior_write_count > preview->ppu_memory_write_count ||
            !gbl_ppu_capture_prefix_valid(preview, capture) ||
            seen_rows[capture->line] ||
            (preview->ppu.row_coverage[coverage_index] & row_mask) == 0u ||
            (capture->unsupported_register_bytes &
             ~((UINT32_C(1) << GBL_PPU_RENDER_REGISTER_BYTES) - 1u)) != 0u) {
            return false;
        }
        sampled_ppu.dispcnt = capture->dispcnt;
        sampled_ppu.unsupported_register_bytes =
            capture->unsupported_register_bytes;
        memcpy(sampled_ppu.render_register_bytes,
               capture->render_register_bytes,
               sizeof(sampled_ppu.render_register_bytes));
        if (gbl_ppu_active_invalid_reason(&sampled_ppu) !=
            GBL_VIDEO_INVALID_REASON_NONE) {
            return false;
        }
        seen_rows[capture->line] = true;
    }
    return true;
}

void gbl_machine_commit_time(gbl_machine *machine,
                            const gbl_machine_time_preview *preview)
{
    size_t index;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        preview->event_count > GBL_EVENT_CAPACITY ||
        preview->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        preview->input_event_cursor > preview->input_event_count ||
        preview->ppu_row_capture_count > GBL_PPU_ROW_CAPTURE_MAX ||
        preview->ppu_memory_write_count > GBL_TIME_OPERATION_MAX ||
        !gbl_ppu_commit_preview_valid(machine, preview)) {
        return;
    }
    gbl_ppu_materialize_capture_plan(machine, preview);
    machine->master_cycles = preview->master_cycles;
    machine->next_event_sequence = preview->next_event_sequence;
    machine->interrupt_flags = preview->interrupt_flags;
    machine->interrupt_enable = preview->interrupt_enable;
    machine->interrupt_master_enable = preview->interrupt_master_enable;
    machine->waitcnt = preview->waitcnt;
    machine->dispcnt = preview->ppu.dispcnt;
    machine->ppu = preview->ppu;
    machine->bus_owner = preview->bus_owner;
    machine->dma_last_channel = preview->dma_last_channel;
    machine->dma_last_width = preview->dma_last_width;
    machine->dma_last_source = preview->dma_last_source;
    machine->dma_last_destination = preview->dma_last_destination;
    machine->dma_stream_valid = preview->dma_stream_valid;
    machine->pressed_keys = preview->pressed_keys;
    machine->key_control = preview->key_control;
    machine->key_condition = preview->key_condition;
    machine->key_irq_condition = preview->key_irq_condition;
    machine->event_count = preview->event_count;
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        machine->events[index] = preview->events[index];
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        machine->timers[index] = preview->timers[index];
    }
    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        machine->dma_channels[index] = preview->dma_channels[index];
    }
    machine->input_generation = preview->input_generation;
    machine->input_event_count = preview->input_event_count;
    machine->input_event_cursor = preview->input_event_cursor;
    for (index = 0u; index < GBL_INPUT_EVENT_CAPACITY; ++index) {
        machine->input_events[index] = preview->input_events[index];
    }
}

static gbl_dma_status gbl_dma_status_from_time(gbl_time_status status)
{
    if (status == GBL_TIME_STATUS_CYCLE_OVERFLOW) {
        return GBL_DMA_STATUS_TIME_OVERFLOW;
    }
    if (status == GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT) {
        return GBL_DMA_STATUS_EVENT_DISPATCH_LIMIT;
    }
    return GBL_DMA_STATUS_UNIT_REFUSAL;
}

static void gbl_dma_project_trigger_observation(
    const gbl_dma_channel_state *channel, gbl_dma_step_result *step)
{
    step->trigger_origin = channel->trigger_origin;
    step->trigger_cycle = channel->trigger_cycle;
    step->trigger_dispute = channel->trigger_dispute;
    if (channel->trigger_dispute != GBL_DMA_DISPUTE_NONE) {
        step->result_classification = GBL_DMA_RESULT_CLASSIFICATION_DISPUTED;
        step->hardware_qualification = GBL_HARDWARE_QUALIFICATION_UNKNOWN;
    }
}

static bool gbl_dma_other_channel_ready(const gbl_machine *machine,
                                       uint8_t excluded_channel)
{
    uint8_t index;

    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        if (index != excluded_channel &&
            (machine->dma_channels[index].pending ||
             machine->dma_channels[index].active)) {
            return true;
        }
    }
    return false;
}

gbl_time_status gbl_machine_dma_request(gbl_machine *machine,
                                       uint8_t channel_index, uint64_t cycle,
                                       uint64_t generation)
{
    gbl_dma_channel_state *channel;
    gbl_machine_time_preview candidate;
    gbl_time_status status;

    if (machine == NULL || !machine->initialized ||
        channel_index >= GBL_DMA_CHANNEL_COUNT ||
        machine->event_count > GBL_EVENT_CAPACITY) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    channel = &machine->dma_channels[channel_index];
    if (cycle < machine->master_cycles) {
        return GBL_TIME_STATUS_BACKWARD;
    }
    if (channel->generation != generation ||
        (channel->control & UINT16_C(0x8000)) == 0u ||
        ((channel->control >> 12u) & 3u) == 0u || !channel->armed) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    gbl_machine_time_copy_preview(machine, &candidate);
    status = gbl_preview_insert_dma_event(
        &candidate, channel_index, cycle, generation,
        GBL_DMA_TRIGGER_ORIGIN_EXPLICIT_REQUEST, GBL_DMA_DISPUTE_NONE);
    if (status != GBL_TIME_STATUS_OK) {
        return status;
    }
    gbl_machine_commit_time(machine, &candidate);
    return GBL_TIME_STATUS_OK;
}

gbl_dma_status gbl_machine_dma_step(gbl_machine *machine,
                                   uint64_t cycle_budget,
                                   gbl_dma_step_result *result)
{
    uint8_t channel_index;
    gbl_dma_channel_state *channel;
    gbl_dma_step_result step = { 0 };
    gbl_time_status time_status;

    if (machine == NULL || result == NULL || !machine->initialized ||
        machine->event_count > GBL_EVENT_CAPACITY ||
        machine->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        machine->input_event_cursor > machine->input_event_count) {
        step.status = GBL_DMA_STATUS_INVALID_ARGUMENT;
        if (result != NULL) {
            *result = step;
        }
        return step.status;
    }
    step.owner = machine->bus_owner;
    step.completion_cycle = machine->master_cycles;
    for (channel_index = 0u; channel_index < GBL_DMA_CHANNEL_COUNT;
         ++channel_index) {
        if (machine->dma_channels[channel_index].pending ||
            machine->dma_channels[channel_index].active) {
            break;
        }
    }
    if (channel_index == GBL_DMA_CHANNEL_COUNT) {
        step.status = GBL_DMA_STATUS_IDLE;
        *result = step;
        return step.status;
    }
    channel = &machine->dma_channels[channel_index];
    step.channel = channel_index;
    step.remaining = channel->remaining;
    step.completed = channel->completed_units;
    gbl_dma_project_trigger_observation(channel, &step);
    if ((channel->control & UINT16_C(0x8000)) == 0u ||
        channel->remaining == 0u ||
        !gbl_dma_control_valid(channel_index, channel->control)) {
        step.status = GBL_DMA_STATUS_UNSUPPORTED;
        *result = step;
        return step.status;
    }
    if (channel->pending) {
        const gbl_time_operation startup = {
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u,
            GBL_DMA_STARTUP_CYCLES
        };
        gbl_machine_time_preview preview;
        if (cycle_budget < GBL_DMA_STARTUP_CYCLES) {
            step.status = GBL_DMA_STATUS_BUDGET_REFUSAL;
            *result = step;
            return step.status;
        }
        time_status = gbl_machine_prepare_operations(
            machine, &startup, 1u, &preview);
        if (time_status != GBL_TIME_STATUS_OK) {
            step.status = gbl_dma_status_from_time(time_status);
            *result = step;
            return step.status;
        }
        preview.bus_owner = GBL_MACHINE_BUS_OWNER_DMA;
        preview.dma_channels[channel_index].pending = false;
        preview.dma_channels[channel_index].active = true;
        preview.dma_channels[channel_index].startup_complete = true;
        gbl_machine_commit_time(machine, &preview);
        step.status = GBL_DMA_STATUS_OK;
        step.owner = GBL_MACHINE_BUS_OWNER_DMA;
        step.startup_cycles = GBL_DMA_STARTUP_CYCLES;
        step.remaining = machine->dma_channels[channel_index].remaining;
        step.completed = machine->dma_channels[channel_index].completed_units;
        step.completion_cycle = machine->master_cycles;
        gbl_dma_project_trigger_observation(
            &machine->dma_channels[channel_index], &step);
        *result = step;
        return step.status;
    }
    if (!channel->active || !channel->startup_complete) {
        step.status = GBL_DMA_STATUS_UNIT_REFUSAL;
        *result = step;
        return step.status;
    }
    if (machine->bus_owner != GBL_MACHINE_BUS_OWNER_DMA) {
        machine->bus_owner = GBL_MACHINE_BUS_OWNER_DMA;
        machine->dma_stream_valid = false;
    }
    if (channel->remaining == 0u ||
        channel->completed_units == UINT64_MAX ||
        (channel->remaining == 1u &&
         !((channel->control & UINT16_C(0x0200)) != 0u &&
           ((channel->control >> 12u) & 3u) != 0u) &&
         channel->generation == UINT64_MAX)) {
        step.status = GBL_DMA_STATUS_UNIT_REFUSAL;
        *result = step;
        return step.status;
    }
    step.status = gbl_bus_dma_transfer_unit(machine, channel_index,
                                             cycle_budget, &step);
    channel = &machine->dma_channels[channel_index];
    gbl_dma_project_trigger_observation(channel, &step);
    if (step.status == GBL_DMA_STATUS_OK && step.unit_completed) {
        const uint32_t width =
            (channel->control & UINT16_C(0x0400)) != 0u ? 4u : 2u;
        const uint8_t source_mode =
            (uint8_t)((channel->control >> 7u) & 3u);
        const uint8_t destination_mode =
            (uint8_t)((channel->control >> 5u) & 3u);

        channel = &machine->dma_channels[channel_index];
        --channel->remaining;
        ++channel->completed_units;
        if (source_mode == 0u) {
            channel->current_source =
                (channel->current_source + width) &
                gbl_dma_source_mask(channel_index);
        } else if (source_mode == 1u) {
            channel->current_source =
                (channel->current_source - width) &
                gbl_dma_source_mask(channel_index);
        }
        if (destination_mode == 0u || destination_mode == 3u) {
            channel->current_destination =
                (channel->current_destination + width) &
                gbl_dma_destination_mask(channel_index);
        } else if (destination_mode == 1u) {
            channel->current_destination =
                (channel->current_destination - width) &
                gbl_dma_destination_mask(channel_index);
        }
        if (channel->remaining == 0u) {
            channel->active = false;
            channel->pending = false;
            channel->startup_complete = false;
            if ((channel->control & UINT16_C(0x0200)) != 0u &&
                ((channel->control >> 12u) & 3u) != 0u) {
                channel->remaining =
                    (channel->count & gbl_dma_count_mask(channel_index));
                if (channel->remaining == 0u) {
                    channel->remaining = channel_index == 3u
                                             ? UINT32_C(0x10000)
                                             : UINT32_C(0x4000);
                }
                if (destination_mode == 3u) {
                    channel->current_destination =
                        channel->destination_address &
                        gbl_dma_destination_mask(channel_index) & ~(width - 1u);
                }
                channel->armed = true;
            } else {
                channel->control = (uint16_t)(
                    channel->control & (uint16_t)~UINT16_C(0x8000));
                channel->armed = false;
                ++channel->generation;
            }
            if ((channel->control & UINT16_C(0x4000)) != 0u) {
                machine->interrupt_flags = (uint16_t)(
                    machine->interrupt_flags |
                    (uint16_t)(UINT16_C(0x0100) << channel_index));
            }
        }
        if (!channel->pending && !channel->active &&
            machine->bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
            !gbl_dma_other_channel_ready(machine, channel_index)) {
            machine->bus_owner = GBL_MACHINE_BUS_OWNER_CPU;
            machine->cpu.force_nonsequential_fetch = true;
            machine->dma_stream_valid = false;
        }
        step.owner = GBL_MACHINE_BUS_OWNER_DMA;
        step.remaining = channel->remaining;
        step.completed = channel->completed_units;
        step.completion_cycle = machine->master_cycles;
    }
    *result = step;
    return step.status;
}

gbl_time_status gbl_machine_advance_to(gbl_machine *machine,
                                       uint64_t target)
{
    gbl_machine_time_preview preview;
    gbl_time_status status =
        gbl_machine_prepare_time(machine, target, &preview);

    if (status == GBL_TIME_STATUS_OK) {
        gbl_machine_commit_time(machine, &preview);
    }
    return status;
}

gbl_time_status gbl_machine_set_input_timeline(
    gbl_machine *machine, const gbl_input_event *events, size_t count)
{
    gbl_machine_time_preview candidate;
    uintptr_t events_start;
    uintptr_t events_end;
    uintptr_t machine_start;
    uintptr_t machine_end;
    size_t span_size;
    size_t index;
    gbl_time_status status;

    if (machine == NULL || !machine->initialized ||
        machine->event_count > GBL_EVENT_CAPACITY ||
        machine->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        machine->input_event_cursor > machine->input_event_count) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (count > GBL_INPUT_EVENT_CAPACITY) {
        return GBL_TIME_STATUS_INPUT_CAPACITY;
    }
    if (count != 0u && events == NULL) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (machine->input_generation == UINT64_MAX) {
        return GBL_TIME_STATUS_GENERATION_EXHAUSTED;
    }

    if (count != 0u) {
        span_size = count * sizeof(*events);
        events_start = (uintptr_t)events;
        if ((uintptr_t)span_size > UINTPTR_MAX - events_start) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        events_end = events_start + (uintptr_t)span_size;
        machine_start = (uintptr_t)machine;
        if ((uintptr_t)sizeof(*machine) > UINTPTR_MAX - machine_start) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        machine_end = machine_start + (uintptr_t)sizeof(*machine);
        if (events_start < machine_end && machine_start < events_end) {
            return GBL_TIME_STATUS_INPUT_OVERLAP;
        }
        if (machine->rom != NULL && machine->rom_length != 0u) {
            uintptr_t rom_start = (uintptr_t)machine->rom;
            uintptr_t rom_end;

            if ((uintptr_t)machine->rom_length > UINTPTR_MAX - rom_start) {
                return GBL_TIME_STATUS_INVALID_ARGUMENT;
            }
            rom_end = rom_start + (uintptr_t)machine->rom_length;
            if (events_start < rom_end && rom_start < events_end) {
                return GBL_TIME_STATUS_INPUT_OVERLAP;
            }
        }
        for (index = 0u; index < count; ++index) {
            if ((events[index].pressed_mask &
                 (uint16_t)~GBL_KEYPAD_INPUT_MASK) != 0u) {
                return GBL_TIME_STATUS_INPUT_MASK;
            }
            if (events[index].cycle < machine->master_cycles ||
                (index != 0u &&
                 events[index].cycle < events[index - 1u].cycle)) {
                return GBL_TIME_STATUS_BACKWARD;
            }
        }
    }

    gbl_machine_time_copy_preview(machine, &candidate);
    gbl_preview_remove_input_event(&candidate);
    ++candidate.input_generation;
    candidate.input_event_count = count;
    candidate.input_event_cursor = 0u;
    for (index = 0u; index < GBL_INPUT_EVENT_CAPACITY; ++index) {
        candidate.input_events[index] = (gbl_input_event){ 0 };
    }
    for (index = 0u; index < count; ++index) {
        candidate.input_events[index] = events[index];
    }
    status = gbl_preview_schedule_next_input(&candidate);
    if (status != GBL_TIME_STATUS_OK) {
        return status;
    }
    gbl_machine_commit_time(machine, &candidate);
    return GBL_TIME_STATUS_OK;
}

uint8_t gbl_timer_read(const gbl_machine_time_preview *preview,
                       uint32_t address)
{
    uint32_t offset;
    uint8_t timer_index;
    uint32_t lane;
    const gbl_timer_state *timer;
    uint16_t counter;
    uint16_t phase;
    uint8_t control;

    if (preview == NULL || address < UINT32_C(0x04000100) ||
        address > UINT32_C(0x0400010F)) {
        return 0u;
    }
    offset = address - UINT32_C(0x04000100);
    timer_index = (uint8_t)(offset / 4u);
    lane = offset % 4u;
    timer = &preview->timers[timer_index];
    counter = timer->counter;
    phase = timer->phase;
    if (timer->enabled && !timer->cascade &&
        gbl_timer_counter_at(timer, preview->master_cycles, &counter,
                             &phase) != GBL_TIME_STATUS_OK) {
        return 0u;
    }
    if (lane == 0u) {
        return (uint8_t)(counter & UINT16_C(0x00FF));
    }
    if (lane == 1u) {
        return (uint8_t)(counter >> 8u);
    }
    if (lane == 3u) {
        return 0u;
    }
    control = (uint8_t)((timer->enabled ? UINT8_C(0x80) : 0u) |
                        (timer->irq_enabled ? UINT8_C(0x40) : 0u) |
                        (timer->cascade ? UINT8_C(0x04) : 0u));
    if (timer->divisor == 64u) {
        control |= UINT8_C(0x01);
    } else if (timer->divisor == 256u) {
        control |= UINT8_C(0x02);
    } else if (timer->divisor == 1024u) {
        control |= UINT8_C(0x03);
    }
    return control;
}

gbl_time_status gbl_timer_write(gbl_machine_time_preview *preview,
                               uint8_t timer_index, uint8_t lane_mask,
                               uint32_t value)
{
    static const uint16_t divisors[4] = { 1u, 64u, 256u, 1024u };
    gbl_machine_time_preview candidate;
    gbl_timer_state *timer;
    bool control_changed = false;
    bool was_enabled;
    uint8_t control;
    uint8_t prescaler;
    uint16_t next_divisor;
    bool next_enabled;
    bool next_irq_enabled;
    bool next_cascade;
    gbl_time_status status;

    if (preview == NULL || timer_index >= GBL_TIMER_COUNT ||
        lane_mask == 0u || (lane_mask & (uint8_t)~UINT8_C(0x07)) != 0u ||
        preview->event_count > GBL_EVENT_CAPACITY) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    candidate = *preview;
    timer = &candidate.timers[timer_index];
    was_enabled = timer->enabled;
    if ((lane_mask & UINT8_C(0x04)) != 0u) {
        control = (uint8_t)((value >> 16u) & UINT32_C(0xFF));
        prescaler = (uint8_t)(control & UINT8_C(0x03));
        next_divisor = divisors[prescaler];
        next_enabled = (control & UINT8_C(0x80)) != 0u;
        next_irq_enabled = (control & UINT8_C(0x40)) != 0u;
        next_cascade = timer_index != 0u &&
                       (control & UINT8_C(0x04)) != 0u;
        control_changed = timer->divisor != next_divisor ||
                          timer->enabled != next_enabled ||
                          timer->irq_enabled != next_irq_enabled ||
                          timer->cascade != next_cascade;
    }
    if (was_enabled && control_changed) {
        status = gbl_preview_sync_timer(&candidate, timer_index);
        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
    }
    if (control_changed && timer->generation == UINT64_MAX) {
        return GBL_TIME_STATUS_GENERATION_EXHAUSTED;
    }

    if ((lane_mask & UINT8_C(0x01)) != 0u) {
        timer->reload = (uint16_t)((timer->reload & UINT16_C(0xFF00)) |
                                   (uint16_t)(value & UINT32_C(0xFF)));
    }
    if ((lane_mask & UINT8_C(0x02)) != 0u) {
        timer->reload = (uint16_t)((timer->reload & UINT16_C(0x00FF)) |
                                   (uint16_t)(value & UINT32_C(0xFF00)));
    }
    if (!control_changed) {
        *preview = candidate;
        return GBL_TIME_STATUS_OK;
    }

    gbl_preview_remove_timer_event(&candidate, timer_index);
    ++timer->generation;
    timer->enabled = next_enabled;
    timer->irq_enabled = next_irq_enabled;
    timer->cascade = next_cascade;
    timer->divisor = next_divisor;
    if (!was_enabled && next_enabled) {
        /* Start is the one control edge that loads the programmable reload.
         * A disabled reload write alone leaves the retained count untouched. */
        timer->counter = timer->reload;
    }
    timer->phase = 0u;
    timer->anchor_cycle = candidate.master_cycles;
    if (next_enabled && !next_cascade) {
        status = gbl_preview_schedule_timer(&candidate, timer_index);
        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
    }
    *preview = candidate;
    return GBL_TIME_STATUS_OK;
}

static void gbl_machine_clear_mutable_state(gbl_machine *machine)
{
    size_t index;

    machine->master_cycles = 0u;
    machine->next_event_sequence = 0u;
    machine->power_state = GBL_MACHINE_POWER_RUNNING;
    machine->open_bus = 0u;
    machine->dispcnt = 0u;
    machine->interrupt_flags = 0u;
    machine->interrupt_enable = 0u;
    machine->interrupt_master_enable = 0u;
    machine->waitcnt = 0u;
    machine->bus_owner = GBL_MACHINE_BUS_OWNER_CPU;
    machine->dma_last_channel = 0u;
    machine->dma_last_width = 0u;
    machine->dma_last_source = 0u;
    machine->dma_last_destination = 0u;
    machine->dma_stream_valid = false;
    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        machine->dma_channels[index] = (gbl_dma_channel_state){ 0 };
    }
    machine->pressed_keys = 0u;
    machine->key_control = 0u;
    machine->key_condition = false;
    machine->key_irq_condition = false;
    machine->event_count = 0u;
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        machine->events[index] = (gbl_event){ 0 };
    }
    machine->ppu = (gbl_ppu_timing_state){ 0 };
    machine->ppu.generation = UINT64_C(1);
    machine->ppu.capture_state = GBL_VIDEO_CAPTURE_INCOMPLETE;
    machine->ppu.capture_reason = GBL_VIDEO_INVALID_REASON_INCOMPLETE;
    machine->ppu.render_register_bytes[1] =
        gbl_ppu_transform_neutral_byte(UINT32_C(1));
    machine->ppu.render_register_bytes[7] =
        gbl_ppu_transform_neutral_byte(UINT32_C(7));
    machine->events[0] = (gbl_event){
        .cycle = 0u,
        .sequence = 0u,
        .generation = UINT64_C(1),
        .priority = GBL_PPU_EVENT_PRIORITY,
        .kind = (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY,
        .live = true
    };
    machine->event_count = 1u;
    machine->next_event_sequence = 1u;
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        machine->timers[index] = (gbl_timer_state){ 0 };
        machine->timers[index].divisor = 1u;
    }
    machine->input_generation = 0u;
    machine->input_event_count = 0u;
    machine->input_event_cursor = 0u;
    for (index = 0u; index < GBL_INPUT_EVENT_CAPACITY; ++index) {
        machine->input_events[index] = (gbl_input_event){ 0 };
    }
    machine->cpu = (gbl_cpu_state){ 0 };
    machine->cpu.cpsr = UINT32_C(0x000000DF);
    machine->cpu.execution_address = UINT32_C(0x08000000);
    for (index = 0u; index < GBL_EWRAM_SIZE; ++index) {
        machine->ewram[index] = 0u;
    }
    for (index = 0u; index < GBL_IWRAM_SIZE; ++index) {
        machine->iwram[index] = 0u;
    }
    for (index = 0u; index < GBL_PRAM_SIZE; ++index) {
        machine->pram[index] = 0u;
    }
    for (index = 0u; index < GBL_VRAM_SIZE; ++index) {
        machine->vram[index] = 0u;
    }
    for (index = 0u; index < GBL_PPU_VIDEO_BYTES; ++index) {
        machine->video_frame[index] = 0u;
    }
    for (index = 0u; index < GBL_OAM_SIZE; ++index) {
        machine->oam[index] = 0u;
    }
}

static bool gbl_machine_allocator_aliases_storage(
    const gbl_machine *machine, const gbl_allocator *allocator)
{
    const unsigned char *storage = (const unsigned char *)machine;
    size_t index;

    for (index = 0u; index < sizeof(*machine); ++index) {
        if ((const void *)(storage + index) == (const void *)allocator) {
            return true;
        }
    }
    return false;
}

gbl_bus_status gbl_machine_init(gbl_machine *machine,
                                const gbl_allocator *allocator)
{
    gbl_allocator supplied = { NULL, NULL, NULL };

    if (machine == NULL) {
        return GBL_BUS_STATUS_INVALID_ARGUMENT;
    }
    if (allocator != NULL) {
        if (gbl_machine_allocator_aliases_storage(machine, allocator)) {
            return GBL_BUS_STATUS_INVALID_ARGUMENT;
        }
        supplied = *allocator;
        if ((supplied.allocate == NULL) != (supplied.release == NULL)) {
            return GBL_BUS_STATUS_INVALID_ARGUMENT;
        }
    }

    machine->initialized = false;
    machine->rom = NULL;
    machine->rom_length = 0u;
    machine->header_diagnostics = (gbl_header_diagnostics){ 0 };
    machine->allocator = supplied;
    gbl_machine_clear_mutable_state(machine);
    machine->initialized = true;
    return GBL_BUS_STATUS_OK;
}

void gbl_machine_reset(gbl_machine *machine)
{
    if (machine == NULL || !machine->initialized) {
        return;
    }
    gbl_machine_clear_mutable_state(machine);
}

void gbl_machine_dispose(gbl_machine *machine)
{
    uint8_t *detached_rom;
    size_t detached_rom_length;
    gbl_allocator detached_allocator;

    if (machine == NULL || !machine->initialized) {
        return;
    }

    detached_rom = machine->rom;
    detached_rom_length = machine->rom_length;
    detached_allocator = machine->allocator;
    machine->rom = NULL;
    machine->rom_length = 0u;
    machine->header_diagnostics = (gbl_header_diagnostics){ 0 };
    machine->allocator.context = NULL;
    machine->allocator.allocate = NULL;
    machine->allocator.release = NULL;
    gbl_machine_clear_mutable_state(machine);
    machine->initialized = false;

    if (detached_rom != NULL && detached_allocator.release != NULL) {
        detached_allocator.release(detached_allocator.context, detached_rom,
                                   detached_rom_length);
    }
}

gbl_time_status gbl_machine_set_power_state(
    gbl_machine *machine, gbl_machine_power_state state)
{
    if (machine == NULL || !machine->initialized ||
        (state != GBL_MACHINE_POWER_RUNNING &&
         state != GBL_MACHINE_POWER_HALTED &&
         state != GBL_MACHINE_POWER_STOPPED)) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    machine->power_state = state;
    return GBL_TIME_STATUS_OK;
}
