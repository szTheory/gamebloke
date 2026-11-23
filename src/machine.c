#include "gamebloke/gamebloke.h"
#include "audio_internal.h"
#include "dma_internal.h"
#include "ppu_internal.h"

#define GBL_KEYPAD_INPUT_MASK UINT16_C(0x03FF)
#define GBL_KEYPAD_CONTROL_MASK UINT16_C(0xC3FF)
#define GBL_KEYPAD_IRQ_BIT UINT16_C(0x1000)
#define GBL_KEYPAD_EVENT_PRIORITY 4u
#define GBL_DMA_EVENT_PRIORITY_BASE 5u
#define GBL_PPU_EVENT_PRIORITY 9u
#define GBL_PPU_BG_MEMORY_BYTES UINT32_C(0x00010000)
#define GBL_PPU_BG_REGISTER_SCROLL_OFFSET UINT32_C(8)
#define GBL_PPU_TEXT_BG_UNSUPPORTED_CONTROL_MASK UINT16_C(0x2010)
#define GBL_PPU_AFFINE_REFERENCE_MASK UINT32_C(0x0FFFFFFF)
#define GBL_PPU_AFFINE_REFERENCE_SIGN UINT32_C(0x08000000)

typedef struct gbl_ppu_bg_sample {
    uint16_t color;
    uint8_t color_index;
    uint8_t background;
    uint8_t priority;
    bool transparent;
} gbl_ppu_bg_sample;

typedef enum gbl_ppu_layer_class {
    GBL_PPU_LAYER_CLASS_OBJ = 0,
    GBL_PPU_LAYER_CLASS_BG = 1,
    GBL_PPU_LAYER_CLASS_BACKDROP = 2
} gbl_ppu_layer_class;

typedef struct gbl_ppu_pixel_candidate {
    uint16_t color;
    uint8_t source_index;
    uint8_t layer_class;
    uint8_t priority;
    uint8_t source_id;
    uint16_t obj_mode;
    bool transparent;
} gbl_ppu_pixel_candidate;

typedef struct gbl_ppu_candidate_pair {
    gbl_ppu_pixel_candidate top;
    gbl_ppu_pixel_candidate next;
} gbl_ppu_candidate_pair;

static void gbl_machine_copy_bytes(void *destination, const void *source,
                                   size_t length)
{
    uint8_t *destination_bytes = destination;
    const uint8_t *source_bytes = source;
    size_t index;

    for (index = 0u; index < length; ++index) {
        destination_bytes[index] = source_bytes[index];
    }
}

static bool gbl_machine_bytes_equal(const void *left, const void *right,
                                    size_t length)
{
    const uint8_t *left_bytes = left;
    const uint8_t *right_bytes = right;
    size_t index;

    for (index = 0u; index < length; ++index) {
        if (left_bytes[index] != right_bytes[index]) {
            return false;
        }
    }
    return true;
}

static void gbl_machine_clear_bytes(void *destination, size_t length)
{
    uint8_t *destination_bytes = destination;
    size_t index;

    for (index = 0u; index < length; ++index) {
        destination_bytes[index] = 0u;
    }
}

static void *gbl_machine_allocate_scratch(gbl_machine *machine,
                                          size_t size)
{
    if (machine == NULL || size == 0u ||
        machine->allocator.allocate == NULL ||
        machine->allocator.release == NULL) {
        return NULL;
    }
    return machine->allocator.allocate(machine->allocator.context, size);
}

static void gbl_machine_release_scratch(gbl_machine *machine,
                                        void *pointer, size_t size)
{
    if (machine != NULL && pointer != NULL &&
        machine->allocator.release != NULL) {
        machine->allocator.release(machine->allocator.context, pointer,
                                   size);
    }
}

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

static void gbl_preview_cancel_sound_dma(
    gbl_machine_time_preview *preview, uint8_t fifo_reset_mask)
{
    unsigned fifo_index;

    for (fifo_index = 0u; fifo_index < GBL_AUDIO_FIFO_COUNT; ++fifo_index) {
        const uint8_t channel_index = (uint8_t)(fifo_index + 1u);
        const gbl_dma_trigger_origin origin =
            fifo_index == 0u ? GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A
                             : GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B;
        gbl_dma_channel_state *channel;
        size_t event_index = 0u;

        if ((fifo_reset_mask & (uint8_t)(UINT8_C(1) << fifo_index)) == 0u) {
            continue;
        }
        channel = &preview->dma_channels[channel_index];
        if (channel->trigger_origin != origin) {
            continue;
        }
        while (event_index < preview->event_count) {
            const gbl_event *event = &preview->events[event_index];

            if (event->kind == (uint8_t)GBL_EVENT_KIND_DMA_READY &&
                event->dma_channel == channel_index &&
                event->generation == channel->generation) {
                gbl_preview_remove_event(preview, event_index);
            } else {
                ++event_index;
            }
        }
        channel->pending = false;
        channel->active = false;
        channel->startup_complete = false;
        channel->remaining = 0u;
        channel->armed = (channel->control & UINT16_C(0x8000)) != 0u &&
                         ((channel->control >> 12u) & 3u) == 3u;
        channel->trigger_origin = GBL_DMA_TRIGGER_ORIGIN_NONE;
        channel->trigger_cycle = 0u;
        channel->trigger_dispute = GBL_DMA_DISPUTE_NONE;
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

static gbl_time_status gbl_preview_queue_sound_dma(
    gbl_machine_time_preview *preview, uint8_t timer_index,
    uint64_t cycle, uint8_t refill_mask)
{
    unsigned fifo_index;

    if (preview == NULL || timer_index > 1u ||
        (refill_mask & (uint8_t)~UINT8_C(0x03)) != 0u) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    for (fifo_index = 0u; fifo_index < GBL_AUDIO_FIFO_COUNT; ++fifo_index) {
        const uint8_t channel_index = (uint8_t)(fifo_index + 1u);
        const gbl_dma_trigger_origin origin =
            fifo_index == 0u ? GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A
                             : GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B;
        gbl_dma_channel_state *channel;
        size_t event_index;
        bool duplicate = false;
        gbl_time_status status;

        if ((refill_mask & (uint8_t)(UINT8_C(1) << fifo_index)) == 0u) {
            continue;
        }
        channel = &preview->dma_channels[channel_index];
        if (!channel->armed || channel->pending || channel->active ||
            (channel->control & UINT16_C(0x8000)) == 0u ||
            ((channel->control >> 12u) & 3u) != 3u) {
            continue;
        }
        for (event_index = 0u; event_index < preview->event_count;
             ++event_index) {
            const gbl_event *event = &preview->events[event_index];

            if (event->kind == (uint8_t)GBL_EVENT_KIND_DMA_READY &&
                event->dma_channel == channel_index &&
                event->generation == channel->generation) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        status = gbl_preview_insert_dma_event(
            preview, channel_index, cycle, channel->generation, origin,
            GBL_DMA_DISPUTE_NONE);
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

static uint64_t gbl_ppu_row_digest_byte(uint64_t digest, uint8_t byte)
{
    return (digest ^ (uint64_t)byte) * UINT64_C(1099511628211);
}

static uint64_t gbl_ppu_row_digest_u64(uint64_t digest, uint64_t value)
{
    unsigned int byte_index;

    for (byte_index = 0u; byte_index < 8u; ++byte_index) {
        digest = gbl_ppu_row_digest_byte(
            digest, (uint8_t)(value & UINT64_C(0xFF)));
        value >>= 8u;
    }
    return digest;
}

static uint64_t gbl_ppu_row_digest_bytes(uint64_t digest,
                                        const uint8_t *bytes, size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        digest = gbl_ppu_row_digest_byte(digest, bytes[index]);
    }
    return digest;
}

static uint64_t gbl_ppu_row_digest(const uint8_t *bytes, size_t length,
                                   const gbl_ppu_row_capture *capture)
{
    uint64_t digest = UINT64_C(14695981039346656037);
    size_t affine_index;

    digest = gbl_ppu_row_digest_bytes(digest, bytes, length);
    digest = gbl_ppu_row_digest_u64(digest, capture->cycle);
    digest = gbl_ppu_row_digest_u64(digest, capture->frame_epoch);
    digest = gbl_ppu_row_digest_u64(digest, capture->line);
    digest = gbl_ppu_row_digest_u64(digest, capture->dispcnt);
    digest = gbl_ppu_row_digest_u64(digest, capture->prior_write_count);
    digest = gbl_ppu_row_digest_u64(
        digest, capture->unsupported_register_bytes);
    digest = gbl_ppu_row_digest_bytes(
        digest, capture->render_register_bytes,
        sizeof(capture->render_register_bytes));
    digest = gbl_ppu_row_digest_bytes(
        digest, capture->bg_register_bytes,
        sizeof(capture->bg_register_bytes));
    digest = gbl_ppu_row_digest_bytes(
        digest, capture->window_register_bytes,
        sizeof(capture->window_register_bytes));
    for (affine_index = 0u;
         affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT; ++affine_index) {
        const gbl_ppu_affine_state *affine = &capture->affine[affine_index];

        digest = gbl_ppu_row_digest_u64(digest, (uint16_t)affine->pa);
        digest = gbl_ppu_row_digest_u64(digest, (uint16_t)affine->pb);
        digest = gbl_ppu_row_digest_u64(digest, (uint16_t)affine->pc);
        digest = gbl_ppu_row_digest_u64(digest, (uint16_t)affine->pd);
        digest = gbl_ppu_row_digest_u64(
            digest, affine->programmed_reference_x);
        digest = gbl_ppu_row_digest_u64(
            digest, affine->programmed_reference_y);
        digest = gbl_ppu_row_digest_u64(digest, affine->origin_x);
        digest = gbl_ppu_row_digest_u64(digest, affine->origin_y);
        digest = gbl_ppu_row_digest_byte(
            digest, affine->origin_valid ? 1u : 0u);
        digest = gbl_ppu_row_digest_byte(
            digest, affine->hblank_y_suppressed ? 1u : 0u);
    }
    digest = gbl_ppu_row_digest_bytes(
        digest, capture->obj_selected_mask,
        sizeof(capture->obj_selected_mask));
    digest = gbl_ppu_row_digest_u64(digest, capture->obj_selected_count);
    digest = gbl_ppu_row_digest_u64(digest, capture->obj_omitted_count);
    digest = gbl_ppu_row_digest_u64(digest, capture->obj_work_used);
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
    gbl_machine_copy_bytes(destination, &source[source_offset], length);
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

static uint16_t gbl_ppu_register_u16(const uint8_t *register_bytes,
                                     uint32_t offset)
{
    return (uint16_t)(
        (uint16_t)register_bytes[offset] |
        (uint16_t)((uint16_t)register_bytes[offset + 1u] << 8u));
}

static uint32_t gbl_ppu_bg_mosaic_size(
    const gbl_ppu_row_capture *capture, uint16_t control, bool vertical)
{
    uint32_t shift;
    uint32_t field;

    if ((control & UINT16_C(0x0040)) == 0u) {
        return UINT32_C(1);
    }
    shift = vertical ? UINT32_C(4) : UINT32_C(0);
    field = ((uint32_t)capture->render_register_bytes[16] >> shift) &
            UINT32_C(0x0F);
    return field + UINT32_C(1);
}

static uint32_t gbl_ppu_obj_mosaic_size(
    const gbl_ppu_row_capture *capture, uint16_t attribute0,
    bool vertical)
{
    uint32_t shift;
    uint32_t field;

    if ((attribute0 & UINT16_C(0x1000)) == 0u) {
        return UINT32_C(1);
    }
    shift = vertical ? UINT32_C(4) : UINT32_C(0);
    field = ((uint32_t)capture->render_register_bytes[17] >> shift) &
            UINT32_C(0x0F);
    return field + UINT32_C(1);
}

static uint32_t gbl_ppu_mosaic_anchor(uint32_t coordinate, uint32_t size)
{
    return coordinate - coordinate % size;
}

static int16_t gbl_ppu_signed_u16(uint16_t bits)
{
    if ((bits & UINT16_C(0x8000)) != 0u) {
        const uint16_t magnitude = (uint16_t)(~bits + UINT16_C(1));

        return (int16_t)(-(int32_t)magnitude);
    }
    return (int16_t)bits;
}

static int32_t gbl_ppu_sign_extend_reference(uint32_t bits)
{
    bits &= GBL_PPU_AFFINE_REFERENCE_MASK;
    if ((bits & GBL_PPU_AFFINE_REFERENCE_SIGN) != 0u) {
        const uint32_t magnitude =
            ((~bits) & GBL_PPU_AFFINE_REFERENCE_MASK) + UINT32_C(1);

        return -(int32_t)magnitude;
    }
    return (int32_t)bits;
}

static uint32_t gbl_ppu_affine_add_reference(uint32_t origin,
                                             int16_t increment)
{
    return (origin + (uint32_t)(int32_t)increment) &
           GBL_PPU_AFFINE_REFERENCE_MASK;
}

static uint32_t gbl_ppu_affine_back_project(uint32_t origin,
                                           int16_t increment,
                                           uint32_t rows)
{
    const int64_t projected = (int64_t)origin -
        (int64_t)increment * (int64_t)rows;

    return (uint32_t)projected & GBL_PPU_AFFINE_REFERENCE_MASK;
}

static void gbl_ppu_affine_write_register(gbl_ppu_affine_state *affine,
                                          uint32_t register_byte,
                                          uint8_t value,
                                          bool in_hblank)
{
    int16_t *matrix = NULL;

    if (affine == NULL || register_byte >= UINT32_C(16)) {
        return;
    }
    switch (register_byte / 2u) {
    case 0u: matrix = &affine->pa; break;
    case 1u: matrix = &affine->pb; break;
    case 2u: matrix = &affine->pc; break;
    case 3u: matrix = &affine->pd; break;
    default: break;
    }
    if (matrix != NULL) {
        uint16_t bits = (uint16_t)(int32_t)*matrix;

        if ((register_byte & 1u) == 0u) {
            bits = (uint16_t)((bits & UINT16_C(0xFF00)) | value);
        } else {
            bits = (uint16_t)((bits & UINT16_C(0x00FF)) |
                              ((uint16_t)value << 8u));
        }
        *matrix = gbl_ppu_signed_u16(bits);
        return;
    }
    if (register_byte < UINT32_C(12)) {
        const uint32_t lane = register_byte - UINT32_C(8);
        const uint32_t shift = lane * UINT32_C(8);
        uint32_t bits = affine->programmed_reference_x;

        bits = (bits & ~(UINT32_C(0xFF) << shift)) |
               ((uint32_t)value << shift);
        affine->programmed_reference_x =
            bits & GBL_PPU_AFFINE_REFERENCE_MASK;
        affine->origin_x = affine->programmed_reference_x;
        affine->origin_valid = true;
        return;
    }
    {
        const uint32_t lane = register_byte - UINT32_C(12);
        const uint32_t shift = lane * UINT32_C(8);
        uint32_t bits = affine->programmed_reference_y;

        bits = (bits & ~(UINT32_C(0xFF) << shift)) |
               ((uint32_t)value << shift);
        affine->programmed_reference_y =
            bits & GBL_PPU_AFFINE_REFERENCE_MASK;
        affine->origin_y = affine->programmed_reference_y;
        affine->hblank_y_suppressed = in_hblank;
        affine->origin_valid = true;
    }
}

static int64_t gbl_ppu_floor_fixed_8_8(int64_t value)
{
    int64_t quotient = value / INT64_C(256);

    if (value < 0 && value % INT64_C(256) != 0) {
        --quotient;
    }
    return quotient;
}

static void gbl_ppu_affine_advance_hblank(gbl_ppu_affine_state *affine)
{
    if (affine == NULL) {
        return;
    }
    if (!affine->origin_valid) {
        affine->origin_x = affine->programmed_reference_x;
        affine->origin_y = affine->programmed_reference_y;
        affine->origin_valid = true;
    }
    affine->origin_x = gbl_ppu_affine_add_reference(affine->origin_x,
                                                    affine->pb);
    affine->origin_y = gbl_ppu_affine_add_reference(affine->origin_y,
                                                    affine->pd);
}

static uint8_t gbl_ppu_legal_background_mask(uint16_t mode)
{
    if (mode == 0u) {
        return UINT8_C(0x0F);
    }
    if (mode == 1u) {
        return UINT8_C(0x07);
    }
    if (mode == 2u) {
        return UINT8_C(0x0C);
    }
    if (mode >= 3u && mode <= 5u) {
        return UINT8_C(0x04);
    }
    return 0u;
}

static bool gbl_ppu_sample_bg_vram(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_row_capture *capture, uint32_t source_offset,
    size_t length, uint8_t *destination, size_t destination_capacity)
{
    if (source_offset > GBL_PPU_BG_MEMORY_BYTES ||
        length > (size_t)(GBL_PPU_BG_MEMORY_BYTES - source_offset)) {
        return false;
    }
    return gbl_ppu_sample_read(
               machine, preview, capture, GBL_BUS_REGION_VRAM, source_offset,
               2u, length, destination, destination_capacity) ==
           GBL_PPU_SAMPLE_STATUS_OK;
}

static bool gbl_ppu_project_affine_row(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_row_capture *capture, uint8_t background,
    uint8_t row[GBL_PPU_ROW_BYTES],
    gbl_ppu_bg_sample samples[GBL_PPU_WIDTH])
{
    const uint8_t affine_index = (uint8_t)(background - 2u);
    const uint32_t control_offset = (uint32_t)background * 2u;
    const uint16_t control =
        gbl_ppu_register_u16(capture->bg_register_bytes, control_offset);
    const gbl_ppu_affine_state *affine = &capture->affine[affine_index];
    const uint32_t size_code = (uint32_t)(control >> 14u);
    const uint32_t map_width = UINT32_C(128) << size_code;
    const uint32_t map_width_tiles = map_width / UINT32_C(8);
    const uint32_t map_base =
        (uint32_t)((control >> 8u) & UINT16_C(0x001F)) * UINT32_C(0x0800);
    const uint32_t character_base =
        (uint32_t)((control >> 2u) & UINT16_C(0x0003)) * UINT32_C(0x4000);
    const bool wrap = (control & UINT16_C(0x2000)) != 0u;
    const uint32_t mosaic_width =
        gbl_ppu_bg_mosaic_size(capture, control, false);
    const uint32_t mosaic_height =
        gbl_ppu_bg_mosaic_size(capture, control, true);
    const uint32_t anchor_line =
        gbl_ppu_mosaic_anchor(capture->line, mosaic_height);
    const uint32_t anchor_delta = capture->line - anchor_line;
    const uint32_t anchor_origin_x = gbl_ppu_affine_back_project(
        affine->origin_x, affine->pb, anchor_delta);
    const uint32_t anchor_origin_y = gbl_ppu_affine_back_project(
        affine->origin_y, affine->pd, anchor_delta);
    uint8_t palette_bytes[512];
    uint8_t tile_row[8];
    uint32_t previous_tile_x = UINT32_MAX;
    uint32_t previous_tile_y = UINT32_MAX;
    uint32_t previous_source_row = UINT32_MAX;
    uint32_t pixel;

    if (!affine->origin_valid ||
        (control & UINT16_C(0x0030)) != 0u ||
        gbl_ppu_sample_read(
            machine, preview, capture, GBL_BUS_REGION_PRAM, 0u, 2u,
            sizeof(palette_bytes), palette_bytes, sizeof(palette_bytes)) !=
            GBL_PPU_SAMPLE_STATUS_OK) {
        return false;
    }

    for (pixel = 0u; pixel < GBL_PPU_WIDTH; ++pixel) {
        const uint32_t anchor_x =
            gbl_ppu_mosaic_anchor(pixel, mosaic_width);
        const int64_t source_x_fixed =
            (int64_t)gbl_ppu_sign_extend_reference(anchor_origin_x) +
            (int64_t)affine->pa * (int64_t)anchor_x;
        const int64_t source_y_fixed =
            (int64_t)gbl_ppu_sign_extend_reference(anchor_origin_y) +
            (int64_t)affine->pc * (int64_t)anchor_x;
        int64_t source_x = gbl_ppu_floor_fixed_8_8(source_x_fixed);
        int64_t source_y = gbl_ppu_floor_fixed_8_8(source_y_fixed);
        uint8_t color_index = 0u;
        uint32_t palette_index;
        uint16_t color;

        if (wrap) {
            source_x = (int64_t)((uint64_t)source_x &
                                 (uint64_t)(map_width - 1u));
            source_y = (int64_t)((uint64_t)source_y &
                                 (uint64_t)(map_width - 1u));
        }
        if (source_x >= 0 && source_y >= 0 &&
            source_x < (int64_t)map_width &&
            source_y < (int64_t)map_width) {
            const uint32_t source_x_unsigned = (uint32_t)source_x;
            const uint32_t source_y_unsigned = (uint32_t)source_y;
            const uint32_t tile_x = source_x_unsigned >> 3u;
            const uint32_t tile_y = source_y_unsigned >> 3u;

            if (tile_x != previous_tile_x || tile_y != previous_tile_y ||
                (source_y_unsigned & 7u) != previous_source_row) {
                const uint64_t map_offset_wide = (uint64_t)map_base +
                    (uint64_t)tile_y * (uint64_t)map_width_tiles +
                    (uint64_t)tile_x;
                uint8_t map_entry;
                uint32_t tile_offset;

                if (map_offset_wide > (uint64_t)GBL_PPU_BG_MEMORY_BYTES ||
                    UINT64_C(1) >
                        (uint64_t)GBL_PPU_BG_MEMORY_BYTES - map_offset_wide ||
                    gbl_ppu_sample_read(
                        machine, preview, capture, GBL_BUS_REGION_VRAM,
                        (uint32_t)map_offset_wide, 1u, sizeof(map_entry),
                        &map_entry, sizeof(map_entry)) !=
                        GBL_PPU_SAMPLE_STATUS_OK) {
                    return false;
                }
                {
                    const uint64_t character_offset_wide =
                        (uint64_t)character_base +
                        (uint64_t)map_entry * UINT64_C(64) +
                        (uint64_t)(source_y_unsigned & 7u) * UINT64_C(8);

                    if (character_offset_wide >
                            (uint64_t)GBL_PPU_BG_MEMORY_BYTES ||
                        sizeof(tile_row) >
                            (size_t)((uint64_t)GBL_PPU_BG_MEMORY_BYTES -
                                     character_offset_wide) ||
                        character_offset_wide > (uint64_t)UINT32_MAX) {
                        return false;
                    }
                    tile_offset = (uint32_t)character_offset_wide;
                }
                if (!gbl_ppu_sample_bg_vram(
                        machine, preview, capture, tile_offset,
                        sizeof(tile_row), tile_row, sizeof(tile_row))) {
                    return false;
                }
                previous_tile_x = tile_x;
                previous_tile_y = tile_y;
                previous_source_row = source_y_unsigned & 7u;
            }
            color_index = tile_row[source_x_unsigned & 7u];
        }
        palette_index = color_index;
        color = (uint16_t)(
            (uint16_t)palette_bytes[palette_index * 2u] |
            (uint16_t)((uint16_t)palette_bytes[palette_index * 2u + 1u]
                       << 8u));
        color &= UINT16_C(0x7FFF);
        if (samples != NULL) {
            samples[pixel] = (gbl_ppu_bg_sample){
                .color = color,
                .color_index = color_index,
                .background = background,
                .priority = (uint8_t)(control & UINT16_C(0x0003)),
                .transparent = color_index == 0u
            };
        }
        row[pixel * 2u] = (uint8_t)(color & UINT16_C(0x00FF));
        row[pixel * 2u + 1u] = (uint8_t)(color >> 8u);
    }
    return true;
}

static bool gbl_ppu_project_bitmap_row(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_row_capture *capture, uint8_t mode,
    uint8_t row[GBL_PPU_ROW_BYTES],
    gbl_ppu_bg_sample samples[GBL_PPU_WIDTH], uint16_t *backdrop_color)
{
    const gbl_ppu_affine_state *affine = &capture->affine[0];
    const uint16_t control =
        gbl_ppu_register_u16(capture->bg_register_bytes, UINT32_C(4));
    const uint8_t priority = (uint8_t)(control & UINT16_C(0x0003));
    const uint32_t mosaic_width =
        gbl_ppu_bg_mosaic_size(capture, control, false);
    const uint32_t mosaic_height =
        gbl_ppu_bg_mosaic_size(capture, control, true);
    const uint32_t anchor_line =
        gbl_ppu_mosaic_anchor(capture->line, mosaic_height);
    const uint32_t anchor_delta = capture->line - anchor_line;
    const uint32_t anchor_origin_x = gbl_ppu_affine_back_project(
        affine->origin_x, affine->pb, anchor_delta);
    const uint32_t anchor_origin_y = gbl_ppu_affine_back_project(
        affine->origin_y, affine->pd, anchor_delta);
    uint32_t source_width;
    uint32_t source_height;
    uint32_t bytes_per_pixel;
    uint32_t page_extent;
    uint32_t page_base = 0u;
    uint32_t source_row_bytes;
    uint8_t palette_bytes[512];
    uint8_t source_row[GBL_PPU_ROW_BYTES];
    bool constant_source_row;
    bool source_row_valid = false;
    int64_t cached_source_y = -1;
    uint32_t pixel;

    if (mode == 3u) {
        source_width = UINT32_C(240);
        source_height = UINT32_C(160);
        bytes_per_pixel = UINT32_C(2);
        page_extent = UINT32_C(0x12C00);
    } else if (mode == 4u) {
        source_width = UINT32_C(240);
        source_height = UINT32_C(160);
        bytes_per_pixel = UINT32_C(1);
        page_extent = UINT32_C(0x09600);
        if ((capture->dispcnt & UINT16_C(0x0010)) != 0u) {
            page_base = UINT32_C(0x0000A000);
        }
    } else if (mode == 5u) {
        source_width = UINT32_C(160);
        source_height = UINT32_C(128);
        bytes_per_pixel = UINT32_C(2);
        page_extent = UINT32_C(0x0A000);
        if ((capture->dispcnt & UINT16_C(0x0010)) != 0u) {
            page_base = UINT32_C(0x0000A000);
        }
    } else {
        return false;
    }
    source_row_bytes = source_width * bytes_per_pixel;
    if (!affine->origin_valid || source_row_bytes > sizeof(source_row) ||
        (uint64_t)page_base + (uint64_t)page_extent >
            (uint64_t)GBL_VRAM_SIZE ||
        (uint64_t)source_row_bytes * (uint64_t)source_height !=
            (uint64_t)page_extent ||
        gbl_ppu_sample_read(
            machine, preview, capture, GBL_BUS_REGION_PRAM, 0u, 2u,
            sizeof(palette_bytes), palette_bytes, sizeof(palette_bytes)) !=
            GBL_PPU_SAMPLE_STATUS_OK) {
        return false;
    }
    if (backdrop_color != NULL) {
        *backdrop_color = (uint16_t)(
            (uint16_t)palette_bytes[0] |
            (uint16_t)((uint16_t)palette_bytes[1] << 8u));
        *backdrop_color &= UINT16_C(0x7FFF);
    }
    constant_source_row = affine->pc == 0;
    if (constant_source_row) {
        const int64_t source_y = gbl_ppu_floor_fixed_8_8(
            (int64_t)gbl_ppu_sign_extend_reference(anchor_origin_y));

        if (source_y >= 0 && source_y < (int64_t)source_height) {
            const uint64_t source_offset_wide = (uint64_t)page_base +
                (uint64_t)source_y * (uint64_t)source_row_bytes;

            if (source_offset_wide > (uint64_t)UINT32_MAX ||
                (uint64_t)source_row_bytes >
                    (uint64_t)page_base + (uint64_t)page_extent -
                        source_offset_wide ||
                gbl_ppu_sample_read(
                    machine, preview, capture, GBL_BUS_REGION_VRAM,
                    (uint32_t)source_offset_wide,
                    (uint8_t)bytes_per_pixel, source_row_bytes, source_row,
                    sizeof(source_row)) != GBL_PPU_SAMPLE_STATUS_OK) {
                return false;
            }
            source_row_valid = true;
            cached_source_y = source_y;
        }
    }

    for (pixel = 0u; pixel < GBL_PPU_WIDTH; ++pixel) {
        const uint32_t anchor_x =
            gbl_ppu_mosaic_anchor(pixel, mosaic_width);
        const int64_t source_x_fixed =
            (int64_t)gbl_ppu_sign_extend_reference(anchor_origin_x) +
            (int64_t)affine->pa * (int64_t)anchor_x;
        const int64_t source_y_fixed =
            (int64_t)gbl_ppu_sign_extend_reference(anchor_origin_y) +
            (int64_t)affine->pc * (int64_t)anchor_x;
        const int64_t source_x = gbl_ppu_floor_fixed_8_8(source_x_fixed);
        const int64_t source_y = gbl_ppu_floor_fixed_8_8(source_y_fixed);
        uint16_t color;
        gbl_ppu_bg_sample background_sample = {
            .color = 0u,
            .color_index = 0u,
            .background = 2u,
            .priority = priority,
            .transparent = true
        };

        if (source_x >= 0 && source_y >= 0 &&
            source_x < (int64_t)source_width &&
            source_y < (int64_t)source_height) {
            const uint32_t source_x_unsigned = (uint32_t)source_x;
            const uint32_t source_y_unsigned = (uint32_t)source_y;
            uint8_t pixel_bytes[2] = { 0u, 0u };
            const uint8_t *sample = pixel_bytes;

            if (constant_source_row) {
                if (!source_row_valid || cached_source_y != source_y) {
                    return false;
                }
                sample = &source_row[source_x_unsigned * bytes_per_pixel];
            } else {
                const uint64_t local_offset_wide =
                    (uint64_t)source_y_unsigned *
                        (uint64_t)source_row_bytes +
                    (uint64_t)source_x_unsigned *
                        (uint64_t)bytes_per_pixel;
                const uint64_t source_offset_wide =
                    (uint64_t)page_base + local_offset_wide;

                if (local_offset_wide > (uint64_t)page_extent ||
                    (uint64_t)bytes_per_pixel >
                        (uint64_t)page_extent - local_offset_wide ||
                    source_offset_wide > (uint64_t)UINT32_MAX ||
                    gbl_ppu_sample_read(
                        machine, preview, capture, GBL_BUS_REGION_VRAM,
                        (uint32_t)source_offset_wide,
                        (uint8_t)bytes_per_pixel,
                        (size_t)bytes_per_pixel, pixel_bytes,
                        sizeof(pixel_bytes)) != GBL_PPU_SAMPLE_STATUS_OK) {
                    return false;
                }
            }
            if (mode == 4u) {
                const uint8_t color_index = sample[0];
                const uint32_t palette_offset =
                    (uint32_t)color_index * UINT32_C(2);

                color = (uint16_t)(
                    (uint16_t)palette_bytes[palette_offset] |
                    (uint16_t)((uint16_t)palette_bytes[palette_offset + 1u]
                               << 8u));
                background_sample.color_index = color_index;
                background_sample.transparent = color_index == 0u;
            } else {
                color = (uint16_t)(
                    (uint16_t)sample[0] |
                    (uint16_t)((uint16_t)sample[1] << 8u));
                color &= UINT16_C(0x7FFF);
                background_sample.color_index = UINT8_MAX;
                background_sample.transparent = false;
            }
        } else {
            color = (uint16_t)(
                (uint16_t)palette_bytes[0] |
                (uint16_t)((uint16_t)palette_bytes[1] << 8u));
        }
        color &= UINT16_C(0x7FFF);
        background_sample.color = color;
        if (samples != NULL) {
            samples[pixel] = background_sample;
        }
        row[pixel * 2u] = (uint8_t)(color & UINT16_C(0x00FF));
        row[pixel * 2u + 1u] = (uint8_t)(color >> 8u);
    }
    return true;
}

static bool gbl_ppu_project_text_row(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_row_capture *capture, uint8_t background,
    uint8_t row[GBL_PPU_ROW_BYTES],
    gbl_ppu_bg_sample samples[GBL_PPU_WIDTH], uint16_t *backdrop_color)
{
    const uint32_t control_offset = (uint32_t)background * 2u;
    const uint32_t scroll_offset = GBL_PPU_BG_REGISTER_SCROLL_OFFSET +
                                   (uint32_t)background * 4u;
    const uint16_t control =
        gbl_ppu_register_u16(capture->bg_register_bytes, control_offset);
    const uint16_t horizontal_scroll = gbl_ppu_register_u16(
        capture->bg_register_bytes, scroll_offset);
    const uint16_t vertical_scroll = gbl_ppu_register_u16(
        capture->bg_register_bytes, scroll_offset + 2u);
    const uint32_t size_code = (uint32_t)(control >> 14u);
    const uint32_t map_width = (size_code & 1u) != 0u ? 512u : 256u;
    const uint32_t map_height = (size_code & 2u) != 0u ? 512u : 256u;
    const uint32_t map_blocks_per_row = map_width / 256u;
    const uint32_t map_base =
        (uint32_t)((control >> 8u) & UINT16_C(0x001F)) * UINT32_C(0x0800);
    const uint32_t character_base =
        (uint32_t)((control >> 2u) & UINT16_C(0x0003)) * UINT32_C(0x4000);
    const uint32_t mosaic_width =
        gbl_ppu_bg_mosaic_size(capture, control, false);
    const uint32_t mosaic_height =
        gbl_ppu_bg_mosaic_size(capture, control, true);
    const uint32_t anchor_line =
        gbl_ppu_mosaic_anchor(capture->line, mosaic_height);
    const bool color_8bpp = (control & UINT16_C(0x0080)) != 0u;
    const uint8_t tile_bytes_per_row = color_8bpp ? 8u : 4u;
    const uint32_t tile_bytes_per_tile = color_8bpp ? 64u : 32u;
    uint8_t palette_bytes[512];
    uint8_t tile_row[8];
    uint32_t previous_tile_x = UINT32_MAX;
    uint32_t previous_tile_y = UINT32_MAX;
    uint16_t map_entry = 0u;
    uint8_t palette_bank = 0u;
    uint32_t pixel;

    if (gbl_ppu_sample_read(
            machine, preview, capture, GBL_BUS_REGION_PRAM, 0u, 2u,
            sizeof(palette_bytes), palette_bytes, sizeof(palette_bytes)) !=
        GBL_PPU_SAMPLE_STATUS_OK) {
        return false;
    }
    if (backdrop_color != NULL) {
        *backdrop_color = (uint16_t)(
            (uint16_t)palette_bytes[0] |
            (uint16_t)((uint16_t)palette_bytes[1] << 8u));
        *backdrop_color &= UINT16_C(0x7FFF);
    }

    for (pixel = 0u; pixel < GBL_PPU_WIDTH; ++pixel) {
        const uint32_t anchor_x =
            gbl_ppu_mosaic_anchor(pixel, mosaic_width);
        const uint32_t source_x =
            (anchor_x + horizontal_scroll) & (map_width - 1u);
        const uint32_t source_y =
            (anchor_line + vertical_scroll) & (map_height - 1u);
        const uint32_t tile_x = source_x >> 3u;
        const uint32_t tile_y = source_y >> 3u;
        gbl_ppu_bg_sample sample = { 0 };
        uint32_t palette_index;

        if (tile_x != previous_tile_x || tile_y != previous_tile_y) {
            const uint32_t screen_block =
                (tile_y / 32u) * map_blocks_per_row + tile_x / 32u;
            const uint64_t map_offset_wide = (uint64_t)map_base +
                (uint64_t)screen_block * UINT64_C(0x0800) +
                ((uint64_t)(tile_y & 31u) * UINT64_C(32) +
                 (uint64_t)(tile_x & 31u)) * UINT64_C(2);
            uint32_t map_offset;
            uint8_t map_bytes[2];
            uint32_t tile_index;
            uint64_t tile_offset_wide;
            uint32_t tile_offset;
            uint8_t tile_y_offset;

            if (map_offset_wide > (uint64_t)UINT32_MAX ||
                map_offset_wide > (uint64_t)GBL_PPU_BG_MEMORY_BYTES ||
                sizeof(map_bytes) >
                    (size_t)((uint64_t)GBL_PPU_BG_MEMORY_BYTES -
                             map_offset_wide)) {
                return false;
            }
            map_offset = (uint32_t)map_offset_wide;
            if (!gbl_ppu_sample_bg_vram(
                    machine, preview, capture, map_offset, sizeof(map_bytes),
                    map_bytes, sizeof(map_bytes))) {
                return false;
            }
            map_entry = (uint16_t)(
                (uint16_t)map_bytes[0] |
                (uint16_t)((uint16_t)map_bytes[1] << 8u));
            palette_bank = (uint8_t)((map_entry >> 12u) & UINT16_C(0x000F));
            tile_index = (uint32_t)(map_entry & UINT16_C(0x03FF));
            tile_y_offset = (uint8_t)(source_y & 7u);
            if ((map_entry & UINT16_C(0x0800)) != 0u) {
                tile_y_offset = (uint8_t)(7u - tile_y_offset);
            }
            tile_offset_wide = (uint64_t)character_base +
                (uint64_t)tile_index * (uint64_t)tile_bytes_per_tile +
                (uint64_t)tile_y_offset * (uint64_t)tile_bytes_per_row;
            if (tile_offset_wide > (uint64_t)UINT32_MAX ||
                tile_offset_wide > (uint64_t)GBL_PPU_BG_MEMORY_BYTES ||
                tile_bytes_per_row >
                    (size_t)((uint64_t)GBL_PPU_BG_MEMORY_BYTES -
                             tile_offset_wide)) {
                return false;
            }
            tile_offset = (uint32_t)tile_offset_wide;
            if (!gbl_ppu_sample_bg_vram(
                    machine, preview, capture, tile_offset,
                    tile_bytes_per_row, tile_row, sizeof(tile_row))) {
                return false;
            }
            previous_tile_x = tile_x;
            previous_tile_y = tile_y;
        }

        {
            uint8_t horizontal_in_tile = (uint8_t)(source_x & 7u);

            if ((map_entry & UINT16_C(0x0400)) != 0u) {
                horizontal_in_tile = (uint8_t)(7u - horizontal_in_tile);
            }
            if (color_8bpp) {
                sample.color_index = tile_row[horizontal_in_tile];
            } else {
                const uint8_t packed = tile_row[horizontal_in_tile >> 1u];

                sample.color_index = (horizontal_in_tile & 1u) != 0u
                    ? (uint8_t)(packed >> 4u)
                    : (uint8_t)(packed & UINT8_C(0x0F));
            }
        }

        sample.background = background;
        sample.priority = (uint8_t)(control & UINT16_C(0x0003));
        sample.transparent = sample.color_index == 0u;
        palette_index = sample.transparent
            ? 0u
            : (color_8bpp
                   ? (uint32_t)sample.color_index
                   : (uint32_t)palette_bank * 16u + sample.color_index);
        sample.color = (uint16_t)(
            (uint16_t)palette_bytes[palette_index * 2u] |
            (uint16_t)((uint16_t)palette_bytes[palette_index * 2u + 1u]
                       << 8u));
        sample.color &= UINT16_C(0x7FFF);
        if (samples != NULL) {
            samples[pixel] = sample;
        }
        row[pixel * 2u] = (uint8_t)(sample.color & UINT16_C(0x00FF));
        row[pixel * 2u + 1u] = (uint8_t)(sample.color >> 8u);
    }
    return true;
}

static uint16_t gbl_ppu_memory_u16(const uint8_t *bytes, size_t offset)
{
    return (uint16_t)(
        (uint16_t)bytes[offset] |
        (uint16_t)((uint16_t)bytes[offset + 1u] << 8u));
}

static int64_t gbl_ppu_floor_div_256(int64_t value)
{
    int64_t quotient = value / INT64_C(256);

    if (value < 0 && value % INT64_C(256) != 0) {
        --quotient;
    }
    return quotient;
}

static bool gbl_ppu_candidate_precedes(
    const gbl_ppu_pixel_candidate *candidate,
    const gbl_ppu_pixel_candidate *current)
{
    if (candidate->transparent) {
        return false;
    }
    if (current->transparent || candidate->priority < current->priority) {
        return true;
    }
    if (candidate->priority > current->priority) {
        return false;
    }
    if (candidate->layer_class != current->layer_class) {
        return candidate->layer_class < current->layer_class;
    }
    return candidate->source_id < current->source_id;
}

static bool gbl_ppu_candidate_same_source(
    const gbl_ppu_pixel_candidate *left,
    const gbl_ppu_pixel_candidate *right)
{
    return !left->transparent && !right->transparent &&
           left->layer_class == right->layer_class &&
           left->source_id == right->source_id;
}

static void gbl_ppu_offer_candidate(gbl_ppu_candidate_pair *pair,
                                    const gbl_ppu_pixel_candidate *candidate)
{
    if (candidate->transparent ||
        gbl_ppu_candidate_same_source(candidate, &pair->top) ||
        gbl_ppu_candidate_same_source(candidate, &pair->next)) {
        return;
    }
    if (gbl_ppu_candidate_precedes(candidate, &pair->top)) {
        pair->next = pair->top;
        pair->top = *candidate;
    } else if (gbl_ppu_candidate_precedes(candidate, &pair->next)) {
        pair->next = *candidate;
    }
}

static const uint8_t gbl_ppu_object_dimensions[3][4][2] = {
    { { 8u, 8u }, { 16u, 16u }, { 32u, 32u }, { 64u, 64u } },
    { { 16u, 8u }, { 32u, 8u }, { 32u, 16u }, { 64u, 32u } },
    { { 8u, 16u }, { 8u, 32u }, { 16u, 32u }, { 32u, 64u } }
};

typedef struct gbl_ppu_obj_admission {
    uint8_t selected_mask[GBL_PPU_OBJ_SELECTED_MASK_BYTES];
    uint16_t selected_count;
    uint16_t omitted_count;
    uint16_t work_used;
} gbl_ppu_obj_admission;

static bool gbl_ppu_build_obj_admission(
    const uint8_t oam_bytes[GBL_OAM_SIZE], bool obj_enabled,
    bool obj_window_enabled, uint16_t line,
    gbl_ppu_obj_admission *admission)
{
    bool accepting = true;
    size_t object;

    if (oam_bytes == NULL || admission == NULL ||
        line >= GBL_PPU_VISIBLE_LINES) {
        return false;
    }
    *admission = (gbl_ppu_obj_admission){ 0 };
    if (!obj_enabled && !obj_window_enabled) {
        return true;
    }
    for (object = 0u; object < 128u; ++object) {
        const size_t offset = object * 8u;
        const uint16_t attribute0 = gbl_ppu_memory_u16(oam_bytes, offset);
        const uint16_t attribute1 =
            gbl_ppu_memory_u16(oam_bytes, offset + 2u);
        const bool affine = (attribute0 & UINT16_C(0x0100)) != 0u;
        const bool double_size = affine &&
            (attribute0 & UINT16_C(0x0200)) != 0u;
        const bool disabled = !affine &&
            (attribute0 & UINT16_C(0x0200)) != 0u;
        const uint16_t object_mode =
            (uint16_t)(attribute0 & UINT16_C(0x0C00));
        const uint16_t shape = (uint16_t)(attribute0 >> 14u);
        const uint16_t size = (uint16_t)(attribute1 >> 14u);
        const uint32_t object_y =
            (uint32_t)(attribute0 & UINT16_C(0x00FF));
        const uint32_t relative_y =
            ((uint32_t)line - object_y) & UINT32_C(0x00FF);
        uint32_t width;
        uint32_t height;
        uint32_t field_width;
        uint32_t field_height;
        uint32_t cost;

        if (disabled) {
            continue;
        }
        if ((object_mode != 0u && object_mode != UINT16_C(0x0400) &&
             !(obj_window_enabled && object_mode == UINT16_C(0x0800))) ||
            shape >= 3u) {
            return false;
        }
        width = gbl_ppu_object_dimensions[shape][size][0];
        height = gbl_ppu_object_dimensions[shape][size][1];
        field_width = double_size ? width * 2u : width;
        field_height = double_size ? height * 2u : height;
        if (relative_y >= field_height) {
            continue;
        }
        if (!accepting) {
            ++admission->omitted_count;
            continue;
        }
        cost = affine ? 10u + 2u * field_width : width;
        if (cost > GBL_PPU_OBJ_WORK_BUDGET - admission->work_used) {
            accepting = false;
            ++admission->omitted_count;
            continue;
        }
        admission->selected_mask[object >> 3u] |=
            (uint8_t)(UINT8_C(1) << (object & 7u));
        ++admission->selected_count;
        admission->work_used = (uint16_t)(admission->work_used + cost);
    }
    return true;
}

static bool gbl_ppu_obj_is_selected(
    const gbl_ppu_obj_admission *admission, size_t object)
{
    return admission != NULL && object < 128u &&
           (admission->selected_mask[object >> 3u] &
            (uint8_t)(UINT8_C(1) << (object & 7u))) != 0u;
}

static bool gbl_ppu_obj_admission_matches_capture(
    const gbl_ppu_obj_admission *admission,
    const gbl_ppu_row_capture *capture)
{
    size_t index;

    if (admission == NULL || capture == NULL ||
        admission->selected_count != capture->obj_selected_count ||
        admission->omitted_count != capture->obj_omitted_count ||
        admission->work_used != capture->obj_work_used) {
        return false;
    }
    for (index = 0u; index < sizeof(admission->selected_mask); ++index) {
        if (admission->selected_mask[index] !=
            capture->obj_selected_mask[index]) {
            return false;
        }
    }
    return true;
}

static bool gbl_ppu_window_contains(const gbl_ppu_row_capture *capture,
                                   size_t horizontal_offset,
                                   size_t vertical_offset,
                                   uint32_t x, uint32_t y)
{
    const uint32_t right =
        capture->window_register_bytes[horizontal_offset];
    const uint32_t left =
        capture->window_register_bytes[horizontal_offset + 1u];
    const uint32_t bottom =
        capture->window_register_bytes[vertical_offset];
    const uint32_t top =
        capture->window_register_bytes[vertical_offset + 1u];

    return left < right && top < bottom && x >= left && x < right &&
           y >= top && y < bottom;
}

static uint8_t gbl_ppu_window_permissions(
    const gbl_ppu_row_capture *capture, uint32_t x,
    bool obj_window_pixel)
{
    const uint16_t masters = (uint16_t)(capture->dispcnt & UINT16_C(0xE000));

    if (masters == 0u) {
        return UINT8_C(0x3F);
    }
    if ((masters & UINT16_C(0x2000)) != 0u &&
        gbl_ppu_window_contains(capture, 0u, 4u, x, capture->line)) {
        return (uint8_t)(capture->window_register_bytes[8u] &
                         UINT8_C(0x3F));
    }
    if ((masters & UINT16_C(0x4000)) != 0u &&
        gbl_ppu_window_contains(capture, 2u, 6u, x, capture->line)) {
        return (uint8_t)(capture->window_register_bytes[9u] &
                         UINT8_C(0x3F));
    }
    if ((masters & UINT16_C(0x8000)) != 0u && obj_window_pixel) {
        return (uint8_t)(capture->window_register_bytes[11u] &
                         UINT8_C(0x3F));
    }
    return (uint8_t)(capture->window_register_bytes[10u] & UINT8_C(0x3F));
}

static uint16_t gbl_ppu_candidate_target_bit(
    const gbl_ppu_pixel_candidate *candidate)
{
    if (candidate->layer_class == GBL_PPU_LAYER_CLASS_BG &&
        candidate->source_id < 4u) {
        return (uint16_t)(UINT16_C(1) << candidate->source_id);
    }
    if (candidate->layer_class == GBL_PPU_LAYER_CLASS_OBJ) {
        return UINT16_C(0x0010);
    }
    if (candidate->layer_class == GBL_PPU_LAYER_CLASS_BACKDROP) {
        return UINT16_C(0x0020);
    }
    return 0u;
}

static uint8_t gbl_ppu_effect_coefficient(uint16_t value)
{
    const uint8_t coefficient = (uint8_t)(value & UINT16_C(0x001F));

    return coefficient > 16u ? 16u : coefficient;
}

static uint16_t gbl_ppu_alpha_blend(uint16_t source, uint16_t target,
                                    uint8_t eva, uint8_t evb)
{
    uint16_t result = 0u;
    uint32_t shift;

    for (shift = 0u; shift < 15u; shift += 5u) {
        const uint32_t source_channel =
            (uint32_t)((source >> shift) & UINT16_C(0x001F));
        const uint32_t target_channel =
            (uint32_t)((target >> shift) & UINT16_C(0x001F));
        uint32_t channel =
            (source_channel * (uint32_t)eva +
             target_channel * (uint32_t)evb) /
            UINT32_C(16);

        if (channel > 31u) {
            channel = 31u;
        }
        result = (uint16_t)(result | (uint16_t)(channel << shift));
    }
    return (uint16_t)(result & UINT16_C(0x7FFF));
}

static uint16_t gbl_ppu_brightness(uint16_t source, uint8_t effect_mode,
                                   uint8_t evy)
{
    uint16_t result = 0u;
    uint32_t shift;

    for (shift = 0u; shift < 15u; shift += 5u) {
        const uint32_t source_channel =
            (uint32_t)((source >> shift) & UINT16_C(0x001F));
        uint32_t channel;

        if (effect_mode == 2u) {
            channel = source_channel +
                ((UINT32_C(31) - source_channel) * (uint32_t)evy) /
                    UINT32_C(16);
        } else {
            channel = source_channel -
                (source_channel * (uint32_t)evy) / UINT32_C(16);
        }
        if (channel > 31u) {
            channel = 31u;
        }
        result = (uint16_t)(result | (uint16_t)(channel << shift));
    }
    return (uint16_t)(result & UINT16_C(0x7FFF));
}

static uint16_t gbl_ppu_apply_pixel_effect(
    const gbl_ppu_row_capture *capture, uint8_t region_permissions,
    const gbl_ppu_candidate_pair *pair)
{
    const uint16_t blend_control =
        gbl_ppu_memory_u16(capture->render_register_bytes, 18u);
    const uint16_t blend_alpha =
        gbl_ppu_memory_u16(capture->render_register_bytes, 20u);
    const uint16_t blend_y =
        gbl_ppu_memory_u16(capture->render_register_bytes, 22u);
    const uint8_t effect_mode = (uint8_t)(
        (blend_control >> 6u) & UINT16_C(0x0003));
    const uint16_t first_target =
        gbl_ppu_candidate_target_bit(&pair->top);
    const uint16_t second_target =
        gbl_ppu_candidate_target_bit(&pair->next);

    if (pair->top.layer_class == GBL_PPU_LAYER_CLASS_OBJ &&
        pair->top.obj_mode == UINT16_C(0x0400)) {
        if ((region_permissions & UINT8_C(0x20)) != 0u &&
            !pair->next.transparent &&
            (blend_control & (uint16_t)(second_target << 8u)) != 0u) {
            return gbl_ppu_alpha_blend(
                (uint16_t)(pair->top.color & UINT16_C(0x7FFF)),
                (uint16_t)(pair->next.color & UINT16_C(0x7FFF)),
                gbl_ppu_effect_coefficient(blend_alpha),
                gbl_ppu_effect_coefficient(
                    (uint16_t)(blend_alpha >> 8u)));
        }
        return (uint16_t)(pair->top.color & UINT16_C(0x7FFF));
    }
    if ((region_permissions & UINT8_C(0x20)) != 0u &&
        effect_mode == 1u && !pair->next.transparent &&
        (blend_control & first_target) != 0u &&
        (blend_control & (uint16_t)(second_target << 8u)) != 0u) {
        return gbl_ppu_alpha_blend(
            (uint16_t)(pair->top.color & UINT16_C(0x7FFF)),
            (uint16_t)(pair->next.color & UINT16_C(0x7FFF)),
            gbl_ppu_effect_coefficient(blend_alpha),
            gbl_ppu_effect_coefficient(
                (uint16_t)(blend_alpha >> 8u)));
    }
    if ((region_permissions & UINT8_C(0x20)) != 0u &&
        (effect_mode == 2u || effect_mode == 3u) &&
        (blend_control & first_target) != 0u) {
        return gbl_ppu_brightness(
            (uint16_t)(pair->top.color & UINT16_C(0x7FFF)), effect_mode,
            gbl_ppu_effect_coefficient(blend_y));
    }
    return (uint16_t)(pair->top.color & UINT16_C(0x7FFF));
}

static bool gbl_ppu_project_composed_row(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_row_capture *capture,
    uint8_t row[GBL_PPU_ROW_BYTES],
    gbl_ppu_obj_admission *admission_out)
{
    uint8_t oam_bytes[GBL_OAM_SIZE];
    uint8_t obj_palette[512];
    uint8_t backdrop_bytes[2];
    uint8_t background_row[GBL_PPU_ROW_BYTES];
    gbl_ppu_bg_sample background_samples[GBL_PPU_WIDTH];
    gbl_ppu_bg_sample sampled_backgrounds[4][GBL_PPU_WIDTH];
    bool background_active[4] = { false };
    gbl_ppu_candidate_pair candidates[GBL_PPU_WIDTH];
    gbl_ppu_candidate_pair obj_candidates[GBL_PPU_WIDTH];
    uint8_t region_permissions[GBL_PPU_WIDTH];
    uint8_t obj_window_mask[GBL_PPU_WIDTH] = { 0u };
    gbl_ppu_obj_admission admission;
    const uint16_t mode = (uint16_t)(capture->dispcnt & UINT16_C(0x0007));
    const uint8_t enabled_backgrounds =
        (uint8_t)((capture->dispcnt >> 8u) & UINT16_C(0x000F));
    const uint8_t legal_backgrounds = gbl_ppu_legal_background_mask(mode);
    const bool obj_enabled =
        (capture->dispcnt & UINT16_C(0x1000)) != 0u;
    const bool obj_window_enabled =
        (capture->dispcnt & UINT16_C(0x8000)) != 0u;
    const bool obj_sampling_enabled = obj_enabled || obj_window_enabled;
    uint16_t backdrop_color;
    uint8_t background;
    uint32_t pixel;
    size_t object;

    if ((capture->dispcnt & UINT16_C(0x0080)) != 0u) {
        const gbl_ppu_obj_admission blank_admission = { 0 };

        if (admission_out != NULL) {
            *admission_out = blank_admission;
        } else if (!gbl_ppu_obj_admission_matches_capture(
                       &blank_admission, capture)) {
            return false;
        }
        for (pixel = 0u; pixel < GBL_PPU_WIDTH; ++pixel) {
            row[pixel * 2u] = UINT8_C(0xFF);
            row[pixel * 2u + 1u] = UINT8_C(0x7F);
        }
        return true;
    }

    if (gbl_ppu_sample_read(
            machine, preview, capture, GBL_BUS_REGION_PRAM, 0u, 2u,
            sizeof(backdrop_bytes), backdrop_bytes,
            sizeof(backdrop_bytes)) != GBL_PPU_SAMPLE_STATUS_OK) {
        return false;
    }
    backdrop_color = (uint16_t)(gbl_ppu_memory_u16(backdrop_bytes, 0u) &
                                UINT16_C(0x7FFF));
    if ((obj_sampling_enabled &&
         gbl_ppu_sample_read(
             machine, preview, capture, GBL_BUS_REGION_OAM, 0u, 2u,
             sizeof(oam_bytes), oam_bytes, sizeof(oam_bytes)) !=
             GBL_PPU_SAMPLE_STATUS_OK) ||
        (obj_enabled &&
         gbl_ppu_sample_read(
             machine, preview, capture, GBL_BUS_REGION_PRAM,
             UINT32_C(0x200), 2u, sizeof(obj_palette), obj_palette,
             sizeof(obj_palette)) != GBL_PPU_SAMPLE_STATUS_OK)) {
        return false;
    }
    if (!gbl_ppu_build_obj_admission(
            oam_bytes, obj_enabled, obj_window_enabled, capture->line,
            &admission) ||
        (admission_out == NULL &&
         !gbl_ppu_obj_admission_matches_capture(&admission, capture))) {
        return false;
    }
    if (admission_out != NULL) {
        *admission_out = admission;
    }
    for (pixel = 0u; pixel < GBL_PPU_WIDTH; ++pixel) {
        obj_candidates[pixel] = (gbl_ppu_candidate_pair){
            .top = { .transparent = true },
            .next = { .transparent = true }
        };
    }

    for (background = 0u; background < 4u; ++background) {
        const uint8_t background_bit =
            (uint8_t)(UINT8_C(1) << background);
        bool projected = false;

        if ((enabled_backgrounds & legal_backgrounds & background_bit) ==
            0u) {
            continue;
        }
        if (mode == 0u || (mode == 1u && background < 2u)) {
            projected = gbl_ppu_project_text_row(
                machine, preview, capture, background, background_row,
                background_samples, NULL);
        } else if ((mode == 1u && background == 2u) || mode == 2u) {
            projected = gbl_ppu_project_affine_row(
                machine, preview, capture, background, background_row,
                background_samples);
        } else if (mode >= 3u && mode <= 5u && background == 2u) {
            projected = gbl_ppu_project_bitmap_row(
                machine, preview, capture, (uint8_t)mode, background_row,
                background_samples, NULL);
        }
        if (!projected) {
            return false;
        }
        background_active[background] = true;
        for (pixel = 0u; pixel < GBL_PPU_WIDTH; ++pixel) {
            sampled_backgrounds[background][pixel] =
                background_samples[pixel];
        }
    }

    for (object = 0u; object < (obj_sampling_enabled ? 128u : 0u); ++object) {
        const size_t oam_offset = object * 8u;
        const uint16_t attribute0 =
            gbl_ppu_memory_u16(oam_bytes, oam_offset);
        const uint16_t attribute1 =
            gbl_ppu_memory_u16(oam_bytes, oam_offset + 2u);
        const uint16_t attribute2 =
            gbl_ppu_memory_u16(oam_bytes, oam_offset + 4u);
        const bool affine = (attribute0 & UINT16_C(0x0100)) != 0u;
        const bool double_size = affine &&
            (attribute0 & UINT16_C(0x0200)) != 0u;
        const bool disabled = !affine &&
            (attribute0 & UINT16_C(0x0200)) != 0u;
        const uint16_t object_mode =
            (uint16_t)(attribute0 & UINT16_C(0x0C00));
        const uint16_t shape = (uint16_t)(attribute0 >> 14u);
        const uint16_t size = (uint16_t)(attribute1 >> 14u);
        const uint32_t object_y = (uint32_t)(attribute0 & UINT16_C(0x00FF));
        const uint32_t relative_y =
            ((uint32_t)capture->line - object_y) & UINT32_C(0x00FF);
        const bool color_8bpp =
            (attribute0 & UINT16_C(0x2000)) != 0u;
        const bool horizontal_flip = !affine &&
            (attribute1 & UINT16_C(0x1000)) != 0u;
        const bool vertical_flip = !affine &&
            (attribute1 & UINT16_C(0x2000)) != 0u;
        const uint32_t width = shape < 3u
            ? gbl_ppu_object_dimensions[shape][size][0]
            : 0u;
        const uint32_t height = shape < 3u
            ? gbl_ppu_object_dimensions[shape][size][1]
            : 0u;
        const uint32_t field_width = double_size ? width * 2u : width;
        const uint32_t field_height = double_size ? height * 2u : height;
        const uint32_t tile_units = color_8bpp ? 2u : 1u;
        const uint32_t tile_width = width / 8u;
        const uint32_t tile_base =
            ((uint32_t)(attribute2 & UINT16_C(0x03FF))) &
            (color_8bpp ? ~UINT32_C(1) : UINT32_MAX);
        const bool one_dimensional =
            (capture->dispcnt & UINT16_C(0x0040)) != 0u;
        const uint32_t tile_row_bytes = color_8bpp ? 8u : 4u;
        const uint32_t tile_bytes = color_8bpp ? 64u : 32u;
        const uint32_t object_x =
            (uint32_t)(attribute1 & UINT16_C(0x01FF));
        const uint64_t obj_window_start = mode >= 3u
            ? UINT64_C(0x14000)
            : UINT64_C(0x10000);
        uint64_t cached_tile_number = UINT64_MAX;
        uint32_t cached_tile_row = UINT32_MAX;
        bool cached_tile_valid = false;
        uint8_t tile_row[8];
        uint32_t local_x;
        int16_t pa = 0;
        int16_t pb = 0;
        int16_t pc = 0;
        int16_t pd = 0;

        if (!gbl_ppu_obj_is_selected(&admission, object)) {
            continue;
        }
        if (disabled) {
            continue;
        }
        if ((object_mode != 0u && object_mode != UINT16_C(0x0400) &&
             !(obj_window_enabled &&
               object_mode == UINT16_C(0x0800))) ||
            shape >= 3u) {
            return false;
        }
        if (object_mode == 0u && !obj_enabled) {
            continue;
        }
        if (relative_y >= field_height) {
            continue;
        }
        if (affine) {
            const uint32_t matrix =
                (uint32_t)((attribute1 >> 9u) & UINT16_C(0x001F));
            const size_t matrix_offset = (size_t)matrix * 32u + 6u;

            pa = gbl_ppu_signed_u16(
                gbl_ppu_memory_u16(oam_bytes, matrix_offset));
            pb = gbl_ppu_signed_u16(
                gbl_ppu_memory_u16(oam_bytes, matrix_offset + 8u));
            pc = gbl_ppu_signed_u16(
                gbl_ppu_memory_u16(oam_bytes, matrix_offset + 16u));
            pd = gbl_ppu_signed_u16(
                gbl_ppu_memory_u16(oam_bytes, matrix_offset + 24u));
        }

        for (local_x = 0u; local_x < field_width; ++local_x) {
            const uint32_t screen_x =
                (object_x + local_x) &
                UINT32_C(0x01FF);
            uint32_t sample_local_x = local_x;
            uint32_t sample_relative_y = relative_y;
            uint32_t source_x;
            uint32_t source_y;
            uint32_t source_tile_row;
            uint32_t tile_y;
            uint64_t tile_number;
            uint8_t color_index;
            uint32_t palette_index;
            uint16_t color;
            gbl_ppu_pixel_candidate candidate;

            if (screen_x >= GBL_PPU_WIDTH) {
                continue;
            }
            if ((attribute0 & UINT16_C(0x1000)) != 0u) {
                const uint32_t mosaic_width =
                    gbl_ppu_obj_mosaic_size(capture, attribute0, false);
                const uint32_t mosaic_height =
                    gbl_ppu_obj_mosaic_size(capture, attribute0, true);
                const uint32_t anchor_x =
                    gbl_ppu_mosaic_anchor(screen_x, mosaic_width);
                const uint32_t anchor_y =
                    gbl_ppu_mosaic_anchor(capture->line, mosaic_height);

                sample_local_x =
                    (anchor_x - object_x) & UINT32_C(0x01FF);
                sample_relative_y =
                    (anchor_y - object_y) & UINT32_C(0x00FF);
                if (sample_local_x >= field_width ||
                    sample_relative_y >= field_height) {
                    continue;
                }
            }
            if (affine) {
                const int64_t dx =
                    (int64_t)sample_local_x -
                    (int64_t)(field_width / 2u);
                const int64_t dy =
                    (int64_t)sample_relative_y -
                    (int64_t)(field_height / 2u);
                const int64_t transformed_x =
                    (int64_t)(width / 2u) + gbl_ppu_floor_div_256(
                        (int64_t)pa * dx + (int64_t)pb * dy);
                const int64_t transformed_y =
                    (int64_t)(height / 2u) + gbl_ppu_floor_div_256(
                        (int64_t)pc * dx + (int64_t)pd * dy);

                if (transformed_x < 0 || transformed_y < 0 ||
                    transformed_x >= (int64_t)width ||
                    transformed_y >= (int64_t)height) {
                    continue;
                }
                source_x = (uint32_t)transformed_x;
                source_y = (uint32_t)transformed_y;
            } else {
                source_x = horizontal_flip
                    ? width - 1u - sample_local_x
                    : sample_local_x;
                source_y = vertical_flip && height > sample_relative_y
                    ? height - 1u - sample_relative_y
                    : sample_relative_y;
            }
            source_tile_row = source_y & 7u;
            tile_y = source_y >> 3u;
            tile_number = (uint64_t)tile_base +
                (uint64_t)tile_y * (one_dimensional
                    ? (uint64_t)tile_width * (uint64_t)tile_units
                    : UINT64_C(32)) +
                (uint64_t)(source_x >> 3u) * (uint64_t)tile_units;
            if (tile_number != cached_tile_number ||
                source_tile_row != cached_tile_row) {
                cached_tile_number = tile_number;
                cached_tile_row = source_tile_row;
                cached_tile_valid = false;
                if ((mode < 3u || tile_number >= UINT64_C(512)) &&
                    tile_number <= (uint64_t)UINT32_MAX) {
                    const uint64_t tile_offset_wide = UINT64_C(0x10000) +
                        tile_number * UINT64_C(32);

                    if (tile_offset_wide >= obj_window_start &&
                        tile_offset_wide <= UINT64_C(0x18000) &&
                        (uint64_t)tile_bytes <=
                            UINT64_C(0x18000) - tile_offset_wide &&
                        tile_offset_wide <= (uint64_t)UINT32_MAX) {
                        const uint64_t row_offset_wide = tile_offset_wide +
                            (uint64_t)source_tile_row *
                                (uint64_t)tile_row_bytes;

                        if (row_offset_wide > (uint64_t)UINT32_MAX ||
                            gbl_ppu_sample_read(
                                machine, preview, capture,
                                GBL_BUS_REGION_VRAM,
                                (uint32_t)row_offset_wide, 1u,
                                (size_t)tile_row_bytes, tile_row,
                                sizeof(tile_row)) !=
                                GBL_PPU_SAMPLE_STATUS_OK) {
                            return false;
                        }
                        cached_tile_valid = true;
                    }
                }
            }
            if (!cached_tile_valid) {
                continue;
            }
            if (color_8bpp) {
                color_index = tile_row[source_x & 7u];
                palette_index = (uint32_t)color_index;
            } else {
                const uint8_t packed = tile_row[(source_x & 7u) >> 1u];

                color_index = ((source_x & 1u) != 0u)
                    ? (uint8_t)(packed >> 4u)
                    : (uint8_t)(packed & UINT8_C(0x0F));
                palette_index =
                    (uint32_t)((attribute2 >> 12u) & UINT16_C(0x000F)) *
                        16u +
                    (uint32_t)color_index;
            }
            if (color_index == 0u) {
                continue;
            }
            if (object_mode == UINT16_C(0x0800)) {
                obj_window_mask[screen_x] = UINT8_C(1);
                continue;
            }
            if (!obj_enabled) {
                continue;
            }
            color = (uint16_t)(
                (uint16_t)obj_palette[palette_index * 2u] |
                (uint16_t)((uint16_t)obj_palette[palette_index * 2u + 1u]
                           << 8u));
            candidate = (gbl_ppu_pixel_candidate){
                .color = (uint16_t)(color & UINT16_C(0x7FFF)),
                .source_index = color_index,
                .layer_class = GBL_PPU_LAYER_CLASS_OBJ,
                .priority = (uint8_t)((attribute2 >> 10u) &
                                      UINT16_C(0x0003)),
                .source_id = (uint8_t)object,
                .obj_mode = object_mode,
                .transparent = false
            };
            gbl_ppu_offer_candidate(&obj_candidates[screen_x], &candidate);
        }
    }

    for (pixel = 0u; pixel < GBL_PPU_WIDTH; ++pixel) {
        candidates[pixel] = (gbl_ppu_candidate_pair){
            .top = {
                .color = backdrop_color,
                .source_index = 0u,
                .layer_class = GBL_PPU_LAYER_CLASS_BACKDROP,
                .priority = 4u,
                .source_id = UINT8_MAX,
                .transparent = false
            },
            .next = { .transparent = true }
        };
        region_permissions[pixel] = gbl_ppu_window_permissions(
            capture, pixel, obj_window_mask[pixel] != 0u);
        for (background = 0u; background < 4u; ++background) {
            const uint8_t background_bit =
                (uint8_t)(UINT8_C(1) << background);
            const gbl_ppu_bg_sample *sample;
            gbl_ppu_pixel_candidate background_candidate;

            if (!background_active[background] ||
                (region_permissions[pixel] & background_bit) == 0u) {
                continue;
            }
            sample = &sampled_backgrounds[background][pixel];
            background_candidate = (gbl_ppu_pixel_candidate){
                .color = sample->color,
                .source_index = sample->color_index,
                .layer_class = GBL_PPU_LAYER_CLASS_BG,
                .priority = sample->priority,
                .source_id = sample->background,
                .transparent = sample->transparent
            };
            gbl_ppu_offer_candidate(&candidates[pixel],
                                    &background_candidate);
        }
        if ((region_permissions[pixel] & UINT8_C(0x10)) != 0u) {
            if (!obj_candidates[pixel].top.transparent) {
                gbl_ppu_offer_candidate(&candidates[pixel],
                                        &obj_candidates[pixel].top);
            }
            if (!obj_candidates[pixel].next.transparent) {
                gbl_ppu_offer_candidate(&candidates[pixel],
                                        &obj_candidates[pixel].next);
            }
        }
        {
            const uint16_t color = gbl_ppu_apply_pixel_effect(
                capture, region_permissions[pixel], &candidates[pixel]);

            row[pixel * 2u] = (uint8_t)(color & UINT16_C(0x00FF));
            row[pixel * 2u + 1u] = (uint8_t)(color >> 8u);
        }
    }
    return true;
}

static bool gbl_ppu_project_row(const gbl_machine *machine,
                               const gbl_machine_time_preview *preview,
                               const gbl_ppu_row_capture *capture,
                               uint8_t row[GBL_PPU_ROW_BYTES])
{
    return gbl_ppu_project_composed_row(machine, preview, capture, row, NULL);
}

static gbl_video_invalid_reason gbl_ppu_dispcnt_invalid_reason(
    uint16_t dispcnt)
{
    const uint16_t mode = (uint16_t)(dispcnt & UINT16_C(0x0007));

    if ((dispcnt & UINT16_C(0x0080)) != 0u) {
        return GBL_VIDEO_INVALID_REASON_NONE;
    }
    if (mode != 0u && mode != UINT16_C(1) && mode != UINT16_C(2) &&
        mode != UINT16_C(3) && mode != UINT16_C(4) &&
        mode != UINT16_C(5)) {
        return GBL_VIDEO_INVALID_REASON_UNSUPPORTED_MODE;
    }
    return GBL_VIDEO_INVALID_REASON_NONE;
}

static gbl_video_invalid_reason gbl_ppu_obj_control_invalid_reason(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_timing_state *ppu,
    const gbl_ppu_row_capture *capture)
{
    uint8_t oam_bytes[GBL_OAM_SIZE];
    const bool obj_enabled = (ppu->dispcnt & UINT16_C(0x1000)) != 0u;
    const bool obj_window_enabled =
        (ppu->dispcnt & UINT16_C(0x8000)) != 0u;
    size_t object;

    if (!obj_enabled && !obj_window_enabled) {
        return GBL_VIDEO_INVALID_REASON_NONE;
    }
    if (gbl_ppu_sample_read(
            machine, preview, capture, GBL_BUS_REGION_OAM, 0u, 2u,
            sizeof(oam_bytes), oam_bytes, sizeof(oam_bytes)) !=
            GBL_PPU_SAMPLE_STATUS_OK) {
        return GBL_VIDEO_INVALID_REASON_OBJ_ENABLED;
    }
    for (object = 0u; object < 128u; ++object) {
        const size_t offset = object * 8u;
        const uint16_t attribute0 = gbl_ppu_memory_u16(oam_bytes, offset);
        const bool affine = (attribute0 & UINT16_C(0x0100)) != 0u;
        const bool disabled = !affine &&
            (attribute0 & UINT16_C(0x0200)) != 0u;
        const uint16_t object_mode =
            (uint16_t)(attribute0 & UINT16_C(0x0C00));

        if (disabled) {
            continue;
        }
        if ((object_mode == UINT16_C(0x0800) && !obj_window_enabled) ||
            object_mode == UINT16_C(0x0C00) ||
            (attribute0 >> 14u) == UINT16_C(3)) {
            return GBL_VIDEO_INVALID_REASON_OBJ_ENABLED;
        }
    }
    return GBL_VIDEO_INVALID_REASON_NONE;
}

static gbl_video_invalid_reason gbl_ppu_active_invalid_reason(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_timing_state *ppu,
    const gbl_ppu_row_capture *capture)
{
    uint16_t mode;
    uint8_t enabled_backgrounds;
    uint8_t legal_backgrounds;
    gbl_video_invalid_reason dispcnt_reason;
    gbl_video_invalid_reason obj_reason;
    uint8_t background;

    if ((ppu->dispcnt & UINT16_C(0x0080)) != 0u) {
        return GBL_VIDEO_INVALID_REASON_NONE;
    }
    mode = (uint16_t)(ppu->dispcnt & UINT16_C(0x0007));
    enabled_backgrounds =
        (uint8_t)((ppu->dispcnt >> 8u) & UINT16_C(0x000F));
    legal_backgrounds = gbl_ppu_legal_background_mask(mode);
    dispcnt_reason = gbl_ppu_dispcnt_invalid_reason(ppu->dispcnt);
    obj_reason =
        gbl_ppu_obj_control_invalid_reason(machine, preview, ppu, capture);
    if (dispcnt_reason != GBL_VIDEO_INVALID_REASON_NONE) {
        return dispcnt_reason;
    }
    if (obj_reason != GBL_VIDEO_INVALID_REASON_NONE) {
        return obj_reason;
    }
    for (background = 0u; background < 4u; ++background) {
        const uint8_t background_bit =
            (uint8_t)(UINT8_C(1) << background);
        uint16_t control;
        uint16_t unsupported_mask;

        if ((enabled_backgrounds & legal_backgrounds & background_bit) ==
            0u) {
            continue;
        }
        control = gbl_ppu_register_u16(
            ppu->bg_register_bytes, (uint32_t)background * 2u);
        if (mode == 0u || (mode == 1u && background < 2u)) {
            unsupported_mask = GBL_PPU_TEXT_BG_UNSUPPORTED_CONTROL_MASK;
        } else if (mode == 1u || mode == 2u) {
            unsupported_mask = UINT16_C(0x0030);
        } else {
            unsupported_mask = 0u;
        }
        if ((control & unsupported_mask) != 0u) {
            return GBL_VIDEO_INVALID_REASON_UNSUPPORTED_MODE;
        }
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
    gbl_ppu_obj_admission admission;
    const uint16_t line = preview->ppu.line;
    const uint8_t mask = (uint8_t)(UINT8_C(1) << (line & 7u));
    const size_t coverage_index = (size_t)(line >> 3u);
    gbl_video_invalid_reason reason;

    if (line >= GBL_PPU_VISIBLE_LINES) {
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
    *capture = (gbl_ppu_row_capture){ 0 };
    capture->cycle = preview->ppu.line_origin;
    capture->frame_epoch = preview->ppu.frame_epoch;
    capture->line = line;
    capture->dispcnt = preview->ppu.dispcnt;
    capture->prior_write_count =
        (uint16_t)preview->ppu_memory_write_count;
    capture->unsupported_register_bytes =
        preview->ppu.unsupported_register_bytes;
    gbl_machine_copy_bytes(capture->render_register_bytes,
                           preview->ppu.render_register_bytes,
                           sizeof(capture->render_register_bytes));
    gbl_machine_copy_bytes(capture->bg_register_bytes,
                           preview->ppu.bg_register_bytes,
                           sizeof(capture->bg_register_bytes));
    gbl_machine_copy_bytes(capture->window_register_bytes,
                           preview->ppu.window_register_bytes,
                           sizeof(capture->window_register_bytes));
    {
        size_t affine_index;

        for (affine_index = 0u;
             affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT;
             ++affine_index) {
            capture->affine[affine_index] =
                preview->ppu.affine[affine_index];
        }
    }
    reason = gbl_ppu_active_invalid_reason(machine, preview, &preview->ppu,
                                          capture);
    if (reason != GBL_VIDEO_INVALID_REASON_NONE) {
        gbl_ppu_invalidate_capture(preview, reason);
        return GBL_TIME_STATUS_OK;
    }
    if (!gbl_ppu_project_composed_row(machine, preview, capture, row,
                                      &admission)) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    gbl_machine_copy_bytes(capture->obj_selected_mask,
                           admission.selected_mask,
                           sizeof(capture->obj_selected_mask));
    capture->obj_selected_count = admission.selected_count;
    capture->obj_omitted_count = admission.omitted_count;
    capture->obj_work_used = admission.work_used;
    capture->pixel_digest = gbl_ppu_row_digest(row, sizeof(row), capture);
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
            gbl_machine_clear_bytes(preview->ppu.row_coverage,
                                    sizeof(preview->ppu.row_coverage));
            {
                size_t affine_index;

                for (affine_index = 0u;
                     affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT;
                     ++affine_index) {
                    gbl_ppu_affine_state *affine =
                        &preview->ppu.affine[affine_index];

                    affine->origin_x = affine->programmed_reference_x;
                    affine->origin_y = affine->programmed_reference_y;
                    affine->origin_valid = true;
                    affine->hblank_y_suppressed = false;
                }
            }
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
        {
            size_t affine_index;

            for (affine_index = 0u;
                 affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT;
                 ++affine_index) {
                preview->ppu.affine[affine_index].hblank_y_suppressed = false;
            }
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
            {
                size_t affine_index;

                for (affine_index = 0u;
                     affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT;
                     ++affine_index) {
                    gbl_ppu_affine_advance_hblank(
                        &preview->ppu.affine[affine_index]);
                }
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
        if (index <= 1u) {
            const uint8_t refill_mask =
                gbl_audio_fifo_clock_timer(&preview->audio, index);

            const gbl_time_status status =
                gbl_preview_queue_sound_dma(preview, index, cycle,
                                            refill_mask);

            if (status != GBL_TIME_STATUS_OK) {
                return status;
            }
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
        if (channel->trigger_cycle != event->cycle ||
            (channel->trigger_origin ==
                 GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A &&
             (event->dma_channel != 1u ||
              ((channel->control >> 12u) & 3u) != 3u)) ||
            (channel->trigger_origin ==
                 GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B &&
             (event->dma_channel != 2u ||
              ((channel->control >> 12u) & 3u) != 3u))) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
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
    if (event->timer_index <= 1u) {
        const uint8_t refill_mask = gbl_audio_fifo_clock_timer(
            &preview->audio, event->timer_index);

        status = gbl_preview_queue_sound_dma(
            preview, event->timer_index, event->cycle, refill_mask);
        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
    }
    status = gbl_preview_increment_cascade(preview, event->timer_index,
                                           event->cycle);
    if (status != GBL_TIME_STATUS_OK) {
        return status;
    }
    return gbl_preview_schedule_timer(preview, event->timer_index);
}

static void gbl_ppu_bind_prepared_registers(
    gbl_machine_time_preview *preview)
{
    preview->ppu_dispcnt_prepared = preview->ppu.dispcnt;
    preview->ppu_unsupported_register_bytes_prepared =
        preview->ppu.unsupported_register_bytes;
    gbl_machine_copy_bytes(preview->ppu_render_register_bytes_prepared,
                           preview->ppu.render_register_bytes,
                           sizeof(preview->ppu_render_register_bytes_prepared));
    gbl_machine_copy_bytes(preview->ppu_bg_register_bytes_prepared,
                           preview->ppu.bg_register_bytes,
                           sizeof(preview->ppu_bg_register_bytes_prepared));
    gbl_machine_copy_bytes(preview->ppu_window_register_bytes_prepared,
                           preview->ppu.window_register_bytes,
                           sizeof(preview->ppu_window_register_bytes_prepared));
}

static bool gbl_ppu_prepared_registers_valid(
    const gbl_machine_time_preview *preview)
{
    return preview != NULL &&
           preview->ppu.dispcnt == preview->ppu_dispcnt_prepared &&
           preview->ppu.unsupported_register_bytes ==
               preview->ppu_unsupported_register_bytes_prepared &&
           gbl_machine_bytes_equal(
               preview->ppu.render_register_bytes,
               preview->ppu_render_register_bytes_prepared,
               sizeof(preview->ppu_render_register_bytes_prepared)) &&
           gbl_machine_bytes_equal(
               preview->ppu.bg_register_bytes,
               preview->ppu_bg_register_bytes_prepared,
               sizeof(preview->ppu_bg_register_bytes_prepared)) &&
           gbl_machine_bytes_equal(
               preview->ppu.window_register_bytes,
               preview->ppu_window_register_bytes_prepared,
               sizeof(preview->ppu_window_register_bytes_prepared));
}

static bool gbl_dma_channel_state_equal(
    const gbl_dma_channel_state *left, const gbl_dma_channel_state *right)
{
    return left != NULL && right != NULL &&
           left->source_address == right->source_address &&
           left->destination_address == right->destination_address &&
           left->current_source == right->current_source &&
           left->current_destination == right->current_destination &&
           left->read_latch == right->read_latch &&
           left->completed_units == right->completed_units &&
           left->generation == right->generation &&
           left->trigger_cycle == right->trigger_cycle &&
           left->trigger_origin == right->trigger_origin &&
           left->trigger_dispute == right->trigger_dispute &&
           left->count == right->count && left->control == right->control &&
           left->remaining == right->remaining &&
           left->pending == right->pending && left->armed == right->armed &&
           left->active == right->active &&
           left->startup_complete == right->startup_complete;
}

static bool gbl_dma_event_identity_equal(const gbl_event *left,
                                         const gbl_event *right)
{
    return left != NULL && right != NULL && left->cycle == right->cycle &&
           left->sequence == right->sequence &&
           left->generation == right->generation &&
           left->priority == right->priority && left->kind == right->kind &&
           left->timer_index == right->timer_index &&
           left->dma_channel == right->dma_channel &&
           left->live == right->live &&
           left->input_index == right->input_index;
}

static void gbl_dma_bind_sound_channels(
    gbl_machine_time_preview *preview)
{
    size_t slot;

    for (slot = 0u; slot < 2u; ++slot) {
        const uint8_t channel_index = (uint8_t)(slot + 1u);
        size_t event_index;

        preview->dma_sound_channels_prepared[slot] =
            preview->dma_channels[channel_index];
        preview->dma_sound_event_present_prepared[slot] = false;
        preview->dma_sound_events_prepared[slot] = (gbl_event){ 0 };
        for (event_index = 0u; event_index < preview->event_count;
             ++event_index) {
            const gbl_event *event = &preview->events[event_index];
            const gbl_dma_channel_state *channel =
                &preview->dma_channels[channel_index];

            if (event->kind == (uint8_t)GBL_EVENT_KIND_DMA_READY &&
                event->dma_channel == channel_index &&
                event->generation == channel->generation) {
                preview->dma_sound_event_present_prepared[slot] = true;
                preview->dma_sound_events_prepared[slot] = *event;
                break;
            }
        }
    }
}

static bool gbl_dma_sound_binding_valid(
    const gbl_machine_time_preview *preview)
{
    size_t slot;

    if (preview == NULL || preview->event_count > GBL_EVENT_CAPACITY ||
        preview->dma_transaction_active !=
            preview->dma_transaction_active_prepared ||
        preview->dma_transaction_dispatched !=
            preview->dma_transaction_dispatched_prepared ||
        (preview->dma_transaction_active &&
         preview->dma_transaction_dispatched >
             GBL_MACHINE_MAX_EVENT_DISPATCH) ||
        (!preview->dma_transaction_active &&
         preview->dma_transaction_dispatched != 0u)) {
        return false;
    }
    for (slot = 0u; slot < 2u; ++slot) {
        const uint8_t channel_index = (uint8_t)(slot + 1u);
        const gbl_dma_channel_state *channel =
            &preview->dma_channels[channel_index];
        size_t matching_count = 0u;
        size_t event_index;

        if (!gbl_dma_channel_state_equal(
                channel, &preview->dma_sound_channels_prepared[slot])) {
            return false;
        }
        for (event_index = 0u; event_index < preview->event_count;
             ++event_index) {
            const gbl_event *event = &preview->events[event_index];

            if (event->kind == (uint8_t)GBL_EVENT_KIND_DMA_READY &&
                event->dma_channel == channel_index &&
                event->generation == channel->generation) {
                ++matching_count;
                if (!preview->dma_sound_event_present_prepared[slot] ||
                    !gbl_dma_event_identity_equal(
                        event, &preview->dma_sound_events_prepared[slot])) {
                    return false;
                }
            }
        }
        if (matching_count > 1u ||
            (matching_count == 1u) !=
                preview->dma_sound_event_present_prepared[slot]) {
            return false;
        }
        if (channel->trigger_origin ==
                GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A ||
            channel->trigger_origin ==
                GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B) {
            const gbl_dma_trigger_origin expected_origin =
                channel_index == 1u
                    ? GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A
                    : GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B;

            if (channel->trigger_origin != expected_origin ||
                ((channel->control >> 12u) & 3u) != 3u ||
                (channel->control & UINT16_C(0x8000)) == 0u ||
                (channel->active && (channel->pending || channel->armed)) ||
                (channel->pending && channel->armed) ||
                (matching_count == 1u &&
                 (!channel->armed || channel->pending || channel->active ||
                  preview->dma_sound_events_prepared[slot].cycle !=
                      channel->trigger_cycle))) {
                return false;
            }
        }
    }
    return true;
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
    preview->audio = machine->audio;
    preview->audio_prepared = machine->audio;
    preview->ppu = machine->ppu;
    preview->ppu.dispcnt = machine->dispcnt;
    gbl_machine_copy_bytes(preview->ppu_affine_prepared,
                           preview->ppu.affine,
                           sizeof(preview->ppu_affine_prepared));
    gbl_ppu_bind_prepared_registers(preview);
    preview->bus_owner = machine->bus_owner;
    preview->dma_last_channel = machine->dma_last_channel;
    preview->dma_last_width = machine->dma_last_width;
    preview->dma_last_source = machine->dma_last_source;
    preview->dma_last_destination = machine->dma_last_destination;
    preview->dma_stream_valid = machine->dma_stream_valid;
    preview->dma_transaction_active = machine->dma_transaction_active;
    preview->dma_transaction_dispatched =
        machine->dma_transaction_dispatched;
    preview->dma_transaction_active_prepared =
        machine->dma_transaction_active;
    preview->dma_transaction_dispatched_prepared =
        machine->dma_transaction_dispatched;
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
    gbl_dma_bind_sound_channels(preview);
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
           (destination_mode != 3u || channel == 3u ||
            ((channel == 1u || channel == 2u) && start_mode == 3u)) &&
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
    gbl_machine_time_preview *preview, uint32_t address, uint8_t value,
    bool defer_control_validation)
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
        if (!defer_control_validation &&
            !gbl_dma_control_valid(channel_index, channel->control)) {
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
    uint64_t target, size_t *dispatched, uint8_t *staging,
    size_t staging_capacity, size_t *staged_bytes,
    gbl_audio_sample_provider provider, void *provider_context,
    uint64_t capture_end_cycle)
{
    gbl_machine_time_preview candidate;
    size_t index;

    if (preview == NULL || dispatched == NULL || staged_bytes == NULL ||
        (preview->dma_transaction_active &&
         *dispatched != preview->dma_transaction_dispatched) ||
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
        uint64_t event_cycle;

        for (index = 1u; index < candidate.event_count; ++index) {
            if (gbl_event_precedes(&candidate.events[index],
                                  &candidate.events[next_index])) {
                next_index = index;
            }
        }
        if (candidate.events[next_index].cycle > target) {
            break;
        }
        event_cycle = candidate.events[next_index].cycle;
        if (event_cycle > candidate.master_cycles) {
            const uint64_t before_event = event_cycle - 1u;
            gbl_time_status status = gbl_audio_advance_span_until(
                &candidate, before_event, staging, staging_capacity,
                staged_bytes, provider, provider_context, capture_end_cycle);

            if (status != GBL_TIME_STATUS_OK) {
                return status;
            }
            candidate.master_cycles = before_event;
        }
        for (;;) {
            gbl_event event;
            gbl_time_status status;

            next_index = 0u;
            for (index = 1u; index < candidate.event_count; ++index) {
                if (gbl_event_precedes(&candidate.events[index],
                                       &candidate.events[next_index])) {
                    next_index = index;
                }
            }
            if (candidate.event_count == 0u ||
                candidate.events[next_index].cycle != event_cycle) {
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
            } else if (event.kind !=
                           (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT &&
                       event.kind !=
                           (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY) {
                return GBL_TIME_STATUS_INVALID_ARGUMENT;
            }
            if (event.cycle < candidate.master_cycles) {
                const uint64_t generation =
                    event.kind == (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW
                        ? candidate.timers[event.timer_index].generation
                        : event.kind == (uint8_t)GBL_EVENT_KIND_DMA_READY
                              ? candidate.dma_channels[event.dma_channel]
                                    .generation
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
        if (event_cycle > candidate.master_cycles) {
            candidate.master_cycles = event_cycle;
        }
        {
            const uint64_t audio_target = candidate.master_cycles > event_cycle
                                              ? candidate.master_cycles
                                              : event_cycle;
            gbl_time_status status = gbl_audio_advance_span_until(
                &candidate, audio_target, staging, staging_capacity,
                staged_bytes, provider, provider_context, capture_end_cycle);

            if (status != GBL_TIME_STATUS_OK) {
                return status;
            }
        }
    }
    {
        gbl_time_status status = gbl_audio_advance_span_until(
            &candidate, target, staging, staging_capacity, staged_bytes,
            provider, provider_context, capture_end_cycle);

        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
    }
    candidate.master_cycles = target;
    if (candidate.dma_transaction_active) {
        candidate.dma_transaction_dispatched = *dispatched;
    }
    candidate.dma_transaction_active_prepared =
        candidate.dma_transaction_active;
    candidate.dma_transaction_dispatched_prepared =
        candidate.dma_transaction_dispatched;
    gbl_dma_bind_sound_channels(&candidate);
    *preview = candidate;
    return GBL_TIME_STATUS_OK;
}

gbl_time_status gbl_machine_prepare_time(
    const gbl_machine *machine, uint64_t target,
    gbl_machine_time_preview *preview)
{
    gbl_machine_time_preview candidate;
    gbl_time_status status;
    size_t dispatched = machine != NULL && machine->dma_transaction_active
                            ? machine->dma_transaction_dispatched
                            : 0u;
    size_t staged_bytes = 0u;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        machine->event_count > GBL_EVENT_CAPACITY ||
        machine->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        machine->input_event_cursor > machine->input_event_count) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    gbl_machine_time_copy_preview(machine, &candidate);
    status = gbl_preview_advance_to(machine, &candidate, target, &dispatched,
                                    NULL, 0u, &staged_bytes, NULL, NULL,
                                    UINT64_MAX);
    if (status != GBL_TIME_STATUS_OK) {
        return status;
    }
    gbl_machine_copy_bytes(candidate.ppu_affine_prepared,
                           candidate.ppu.affine,
                           sizeof(candidate.ppu_affine_prepared));
    gbl_ppu_bind_prepared_registers(&candidate);
    candidate.audio_prepared = candidate.audio;
    candidate.audio_fifo_reset_mask_prepared =
        candidate.audio_fifo_reset_mask;
    gbl_dma_bind_sound_channels(&candidate);
    *preview = candidate;
    return GBL_TIME_STATUS_OK;
}

static bool gbl_preview_bus_span_valid(uint32_t address, uint8_t width)
{
    return (width == 1u || width == 2u || width == 4u) &&
           address <= UINT32_MAX - ((uint32_t)width - 1u) &&
           (!gbl_audio_wave_span_overlaps(address, width) ||
            gbl_audio_wave_span_valid(address, width));
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
    uint8_t audio_lane_masks[GBL_AUDIO_REGISTER_COUNT] = { 0u };
    uint16_t audio_lane_values[GBL_AUDIO_REGISTER_COUNT] = { 0u };
    uint8_t audio_fifo_reset_mask = 0u;
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
    uint32_t unsupported_register_bytes =
        preview->ppu.unsupported_register_bytes;
    uint8_t render_register_bytes[GBL_PPU_RENDER_REGISTER_BYTES];
    uint8_t bg_register_bytes[GBL_PPU_BG_REGISTER_BYTES];
    uint8_t window_register_bytes[GBL_PPU_WINDOW_REGISTER_BYTES];
    gbl_ppu_affine_state affine[GBL_PPU_AFFINE_BACKGROUND_COUNT];
    uint32_t lane;

    if (gbl_audio_fifo_span_overlaps(address, width)) {
        if (!gbl_audio_fifo_span_valid(address, width) ||
            !gbl_audio_fifo_write(&preview->audio, address, width, value)) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        return GBL_TIME_STATUS_OK;
    }

    if (gbl_audio_wave_span_overlaps(address, width)) {
        return gbl_audio_wave_span_valid(address, width) &&
                       gbl_audio_wave_write(&preview->audio, address, width,
                                            value)
                   ? GBL_TIME_STATUS_OK
                   : GBL_TIME_STATUS_INVALID_ARGUMENT;
    }

    gbl_machine_copy_bytes(render_register_bytes,
                           preview->ppu.render_register_bytes,
                           sizeof(render_register_bytes));
    gbl_machine_copy_bytes(bg_register_bytes,
                           preview->ppu.bg_register_bytes,
                           sizeof(bg_register_bytes));
    gbl_machine_copy_bytes(window_register_bytes,
                           preview->ppu.window_register_bytes,
                           sizeof(window_register_bytes));
    {
        size_t affine_index;

        for (affine_index = 0u;
             affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT;
             ++affine_index) {
            affine[affine_index] = preview->ppu.affine[affine_index];
        }
    }

    for (lane = 0u; lane < (uint32_t)width; ++lane) {
        const uint32_t lane_address = address + lane;
        const uint8_t byte_value =
            (uint8_t)((value >> (lane * 8u)) & UINT32_C(0xFF));

        if (lane_address == UINT32_C(0x04000088) ||
            lane_address == UINT32_C(0x04000089)) {
            gbl_audio_write_soundbias(&preview->audio, lane_address,
                                      byte_value);
        } else if (gbl_audio_register_address(lane_address)) {
            const uint32_t register_address = lane_address & ~UINT32_C(1);
            const size_t register_index = (size_t)(
                (register_address - GBL_AUDIO_REGISTER_FIRST) / 2u);
            const uint8_t lane_index =
                (uint8_t)(lane_address & UINT32_C(1));

            audio_lane_masks[register_index] |=
                (uint8_t)(UINT8_C(1) << lane_index);
            audio_lane_values[register_index] |=
                (uint16_t)((uint16_t)byte_value << (lane_index * 8u));
        } else if (lane_address == UINT32_C(0x04000000)) {
            dispcnt = (uint16_t)(
                (dispcnt & UINT16_C(0xFF00)) |
                (uint16_t)byte_value);
        } else if (lane_address == UINT32_C(0x04000001)) {
            dispcnt = (uint16_t)(
                (dispcnt & UINT16_C(0x00FF)) |
                (uint16_t)((uint16_t)byte_value << 8u));
        } else if (lane_address >= UINT32_C(0x04000008) &&
                   lane_address <= UINT32_C(0x0400001F)) {
            const uint32_t register_byte =
                lane_address <= UINT32_C(0x0400000F)
                    ? lane_address - UINT32_C(0x04000008)
                    : GBL_PPU_BG_REGISTER_SCROLL_OFFSET +
                          lane_address - UINT32_C(0x04000010);

            bg_register_bytes[register_byte] = byte_value;
        } else if (lane_address >= UINT32_C(0x04000020) &&
                   lane_address <= UINT32_C(0x0400002F)) {
            const uint32_t register_byte =
                lane_address - UINT32_C(0x04000020);

            gbl_ppu_update_register_byte_deviation(
                &unsupported_register_bytes,
                register_byte, byte_value,
                gbl_ppu_transform_neutral_byte(register_byte));
            render_register_bytes[register_byte] = byte_value;
            gbl_ppu_affine_write_register(
                &affine[0], register_byte, byte_value,
                !preview->ppu.next_boundary_hblank);
        } else if (lane_address >= UINT32_C(0x04000030) &&
                   lane_address <= UINT32_C(0x0400003F)) {
            const uint32_t register_byte =
                lane_address - UINT32_C(0x04000030);

            gbl_ppu_affine_write_register(
                &affine[1], register_byte, byte_value,
                !preview->ppu.next_boundary_hblank);
        } else if (lane_address >= UINT32_C(0x04000040) &&
                   lane_address <= UINT32_C(0x0400004B)) {
            window_register_bytes[lane_address - UINT32_C(0x04000040)] =
                byte_value;
        } else if (lane_address >= UINT32_C(0x0400004C) &&
                   lane_address <= UINT32_C(0x0400004D)) {
            render_register_bytes[
                16u + (lane_address - UINT32_C(0x0400004C))] = byte_value;
        } else if (lane_address >= UINT32_C(0x04000050) &&
                   lane_address <= UINT32_C(0x04000055)) {
            gbl_ppu_update_register_byte_deviation(
                &unsupported_register_bytes,
                UINT32_C(18) + lane_address - UINT32_C(0x04000050),
                byte_value, 0u);
            render_register_bytes[
                18u + (lane_address - UINT32_C(0x04000050))] = byte_value;
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
            const uint32_t dma_offset =
                lane_address - UINT32_C(0x040000B0);
            const bool control_low_lane =
                dma_offset % UINT32_C(12) == UINT32_C(10);
            const bool defer_control_validation =
                control_low_lane && lane + 1u < (uint32_t)width;
            const gbl_time_status status =
                gbl_preview_dma_register_write(preview, lane_address,
                                               byte_value,
                                               defer_control_validation);

            if (status != GBL_TIME_STATUS_OK) {
                return status;
            }
        }
    }
    {
        size_t audio_index;

        for (audio_index = 0u; audio_index < GBL_AUDIO_REGISTER_COUNT;
             ++audio_index) {
            if (audio_lane_masks[audio_index] != 0u) {
                const uint32_t register_address =
                    GBL_AUDIO_REGISTER_FIRST +
                    (uint32_t)(audio_index * 2u);
                const uint8_t reset_mask = gbl_audio_apply_register_write(
                    &preview->audio, register_address,
                    audio_lane_values[audio_index],
                    audio_lane_masks[audio_index]);

                audio_fifo_reset_mask |= reset_mask;
                gbl_preview_cancel_sound_dma(preview, reset_mask);
            }
        }
    }
    preview->audio_fifo_reset_mask |= audio_fifo_reset_mask;
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
    gbl_machine_copy_bytes(preview->ppu.render_register_bytes,
                           render_register_bytes,
                           sizeof(render_register_bytes));
    gbl_machine_copy_bytes(preview->ppu.bg_register_bytes,
                           bg_register_bytes, sizeof(bg_register_bytes));
    gbl_machine_copy_bytes(preview->ppu.window_register_bytes,
                           window_register_bytes,
                           sizeof(window_register_bytes));
    {
        size_t affine_index;

        for (affine_index = 0u;
             affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT;
             ++affine_index) {
            preview->ppu.affine[affine_index] = affine[affine_index];
        }
    }
    preview->ppu.dispcnt = dispcnt;
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

static gbl_time_status gbl_machine_prepare_operations_audio_internal(
    const gbl_machine *machine,
    const gbl_time_operation *operations,
    size_t operation_count, gbl_machine_time_preview *preview,
    uint8_t *staging, size_t staging_capacity, size_t *staged_bytes_out,
    gbl_audio_sample_provider provider, void *provider_context,
    uint64_t capture_end_cycle)
{
    gbl_machine_time_preview candidate;
    size_t dispatched = machine != NULL && machine->dma_transaction_active
                            ? machine->dma_transaction_dispatched
                            : 0u;
    size_t staged_bytes = 0u;
    size_t index;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        machine->event_count > GBL_EVENT_CAPACITY ||
        machine->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        machine->input_event_cursor > machine->input_event_count ||
        operation_count > GBL_TIME_OPERATION_MAX ||
        (operation_count != 0u && operations == NULL) ||
        (staging == NULL && staging_capacity != 0u)) {
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
        if (operation->kind == GBL_TIME_OPERATION_BUS_WRITE &&
            gbl_audio_fifo_span_overlaps(operation->address,
                                         operation->width) &&
            !gbl_audio_fifo_span_valid(operation->address,
                                       operation->width)) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        if (operation->cycles > UINT64_MAX - candidate.master_cycles) {
            return GBL_TIME_STATUS_CYCLE_OVERFLOW;
        }
        target = candidate.master_cycles + operation->cycles;
        status = gbl_preview_advance_to(machine, &candidate, target,
                                        &dispatched, staging,
                                        staging_capacity, &staged_bytes,
                                        provider, provider_context,
                                        capture_end_cycle);
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
    gbl_machine_copy_bytes(candidate.ppu_affine_prepared,
                           candidate.ppu.affine,
                           sizeof(candidate.ppu_affine_prepared));
    gbl_ppu_bind_prepared_registers(&candidate);
    candidate.audio_prepared = candidate.audio;
    candidate.audio_fifo_reset_mask_prepared =
        candidate.audio_fifo_reset_mask;
    gbl_dma_bind_sound_channels(&candidate);
    *preview = candidate;
    if (staged_bytes_out != NULL) {
        *staged_bytes_out = staged_bytes;
    }
    return GBL_TIME_STATUS_OK;
}

gbl_time_status gbl_machine_prepare_operations_audio_with_provider(
    const gbl_machine *machine,
    const gbl_time_operation *operations,
    size_t operation_count, gbl_machine_time_preview *preview,
    uint8_t *staging, size_t staging_capacity, size_t *staged_bytes_out,
    gbl_audio_sample_provider provider, void *provider_context)
{
    return gbl_machine_prepare_operations_audio_internal(
        machine, operations, operation_count, preview, staging,
        staging_capacity, staged_bytes_out, provider, provider_context,
        UINT64_MAX);
}

gbl_time_status gbl_machine_prepare_operations_audio_until(
    const gbl_machine *machine, const gbl_time_operation *operations,
    size_t operation_count, gbl_machine_time_preview *preview,
    uint8_t *staging, size_t staging_capacity, size_t *staged_bytes_out,
    uint64_t capture_end_cycle)
{
    return gbl_machine_prepare_operations_audio_internal(
        machine, operations, operation_count, preview, staging,
        staging_capacity, staged_bytes_out, NULL, NULL, capture_end_cycle);
}

gbl_time_status gbl_machine_prepare_operations_audio(
    const gbl_machine *machine,
    const gbl_time_operation *operations,
    size_t operation_count, gbl_machine_time_preview *preview,
    uint8_t *staging, size_t staging_capacity, size_t *staged_bytes_out)
{
    return gbl_machine_prepare_operations_audio_with_provider(
        machine, operations, operation_count, preview, staging,
        staging_capacity, staged_bytes_out, NULL, NULL);
}

gbl_time_status gbl_machine_prepare_operations(
    const gbl_machine *machine, const gbl_time_operation *operations,
    size_t operation_count, gbl_machine_time_preview *preview)
{
    return gbl_machine_prepare_operations_audio(
        machine, operations, operation_count, preview, NULL, 0u, NULL);
}

static bool gbl_audio_extent(const void *pointer, size_t length,
                             uintptr_t *begin, uintptr_t *end)
{
    uintptr_t address;

    if (begin == NULL || end == NULL || (pointer == NULL && length != 0u)) {
        return false;
    }
    address = (uintptr_t)pointer;
    if (length > UINTPTR_MAX - address) {
        return false;
    }
    *begin = address;
    *end = address + (uintptr_t)length;
    return true;
}

static bool gbl_audio_extents_overlap(uintptr_t left_begin,
                                      uintptr_t left_end,
                                      uintptr_t right_begin,
                                      uintptr_t right_end)
{
    return left_begin < right_end && right_begin < left_end;
}

static bool gbl_audio_capture_ranges_valid(
    const gbl_machine *machine, const gbl_audio_capture *capture,
    uint8_t *destination, size_t capacity)
{
    uintptr_t machine_begin;
    uintptr_t machine_end;
    uintptr_t capture_begin;
    uintptr_t capture_end;
    uintptr_t destination_begin;
    uintptr_t destination_end;
    uintptr_t rom_begin = 0u;
    uintptr_t rom_end = 0u;

    if (machine == NULL || capture == NULL ||
        !gbl_audio_extent(machine, sizeof(*machine), &machine_begin,
                          &machine_end) ||
        !gbl_audio_extent(capture, sizeof(*capture), &capture_begin,
                          &capture_end) ||
        !gbl_audio_extent(destination, capacity, &destination_begin,
                          &destination_end) ||
        gbl_audio_extents_overlap(capture_begin, capture_end, machine_begin,
                                  machine_end) ||
        gbl_audio_extents_overlap(destination_begin, destination_end,
                                  machine_begin, machine_end) ||
        gbl_audio_extents_overlap(destination_begin, destination_end,
                                  capture_begin, capture_end)) {
        return false;
    }
    if (machine->rom != NULL && machine->rom_length != 0u) {
        if (!gbl_audio_extent(machine->rom, machine->rom_length, &rom_begin,
                              &rom_end) ||
            gbl_audio_extents_overlap(capture_begin, capture_end, rom_begin,
                                      rom_end) ||
            gbl_audio_extents_overlap(destination_begin, destination_end,
                                      rom_begin, rom_end)) {
            return false;
        }
    }
    return true;
}

bool gbl_audio_capture_validate(
    const gbl_machine *machine, const gbl_audio_capture *capture)
{
    uint64_t frame_count;
    size_t expected_bytes;

    if (machine == NULL || capture == NULL ||
        capture->machine != machine ||
        (capture->destination == NULL && capture->capacity != 0u) ||
        capture->initial_cycle > capture->last_accepted_cycle ||
        capture->last_accepted_cycle - capture->initial_cycle >
            GBL_AUDIO_MAX_CAPTURE_CYCLES ||
        !gbl_audio_capture_ranges_valid(machine, capture,
                                        capture->destination,
                                        capture->capacity)) {
        return false;
    }
    frame_count = capture->last_accepted_cycle / GBL_AUDIO_SAMPLE_CYCLES -
                  capture->initial_cycle / GBL_AUDIO_SAMPLE_CYCLES;
    if (frame_count > GBL_AUDIO_MAX_CAPTURE_FRAMES ||
        frame_count > (uint64_t)(SIZE_MAX / GBL_AUDIO_FRAME_BYTES)) {
        return false;
    }
    expected_bytes = (size_t)frame_count * GBL_AUDIO_FRAME_BYTES;
    return capture->bytes_written == expected_bytes &&
           capture->bytes_written <= capture->capacity &&
           capture->bytes_written <= GBL_AUDIO_MAX_CAPTURE_BYTES;
}

gbl_time_status gbl_audio_capture_begin(gbl_machine *machine,
                                        gbl_audio_capture *capture,
                                        uint8_t *destination,
                                        size_t capacity)
{
    gbl_audio_capture candidate;

    if (machine == NULL || !machine->initialized || capture == NULL ||
        (destination == NULL && capacity != 0u) ||
        machine->audio.cycle_anchor != machine->master_cycles) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    candidate = (gbl_audio_capture){
        .machine = machine,
        .destination = destination,
        .capacity = capacity,
        .initial_cycle = machine->master_cycles,
        .last_accepted_cycle = machine->master_cycles,
        .bytes_written = 0u
    };
    if (!gbl_audio_capture_ranges_valid(machine, capture, destination,
                                        capacity)) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    *capture = candidate;
    return GBL_TIME_STATUS_OK;
}

gbl_time_status gbl_machine_advance_to_audio(gbl_machine *machine,
                                              uint64_t target,
                                              gbl_audio_capture *capture)
{
    gbl_machine_time_preview preview;
    uint8_t staging[GBL_AUDIO_MAX_CAPTURE_BYTES];
    gbl_time_operation operation;
    size_t staged_bytes = 0u;
    size_t append_bytes;
    uint64_t prior_frame;
    uint64_t final_frame;
    uint64_t total_frames;
    gbl_time_status status;

    if (machine == NULL || !machine->initialized || capture == NULL) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (target < machine->master_cycles) {
        return GBL_TIME_STATUS_BACKWARD;
    }
    if (!gbl_audio_capture_validate(machine, capture) ||
        capture->last_accepted_cycle != machine->master_cycles ||
        machine->audio.cycle_anchor != machine->master_cycles) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (target - capture->initial_cycle > GBL_AUDIO_MAX_CAPTURE_CYCLES) {
        return GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT;
    }
    prior_frame = capture->last_accepted_cycle / GBL_AUDIO_SAMPLE_CYCLES;
    final_frame = target / GBL_AUDIO_SAMPLE_CYCLES;
    total_frames = final_frame -
                   capture->initial_cycle / GBL_AUDIO_SAMPLE_CYCLES;
    if (total_frames > GBL_AUDIO_MAX_CAPTURE_FRAMES ||
        total_frames > (uint64_t)(SIZE_MAX / GBL_AUDIO_FRAME_BYTES)) {
        return GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT;
    }
    append_bytes = (size_t)(final_frame - prior_frame) *
                   GBL_AUDIO_FRAME_BYTES;
    if (total_frames * GBL_AUDIO_FRAME_BYTES >
            (uint64_t)GBL_AUDIO_MAX_CAPTURE_BYTES ||
        append_bytes > capture->capacity - capture->bytes_written) {
        return GBL_TIME_STATUS_AUDIO_CAPTURE_LIMIT;
    }
    operation = (gbl_time_operation){
        .kind = GBL_TIME_OPERATION_INTERNAL,
        .cycles = target - machine->master_cycles
    };
    status = gbl_machine_prepare_operations_audio(
        machine, &operation, 1u, &preview,
        append_bytes == 0u ? NULL : staging, append_bytes, &staged_bytes);
    if (status != GBL_TIME_STATUS_OK) {
        return status;
    }
    if (staged_bytes != append_bytes) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (!gbl_machine_commit_time_checked(machine, &preview)) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (append_bytes != 0u) {
        gbl_machine_copy_bytes(capture->destination + capture->bytes_written,
                               staging, append_bytes);
    }
    capture->bytes_written += append_bytes;
    capture->last_accepted_cycle = target;
    return GBL_TIME_STATUS_OK;
}

static void gbl_ppu_materialize_capture_plan(
    gbl_machine *machine, const gbl_machine_time_preview *preview)
{
    size_t index;

    if (preview->ppu_clear_video) {
        gbl_machine_clear_bytes(machine->video_frame,
                                sizeof(machine->video_frame));
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
        preview->audio.cycle_anchor != preview->master_cycles ||
        (preview->audio_fifo_reset_mask & (uint8_t)~UINT8_C(0x03)) != 0u ||
        preview->audio_fifo_reset_mask !=
            preview->audio_fifo_reset_mask_prepared ||
        !gbl_audio_state_valid(&preview->audio) ||
        !gbl_audio_state_equal(&preview->audio, &preview->audio_prepared) ||
        !gbl_dma_sound_binding_valid(preview) ||
        !gbl_ppu_prepared_registers_valid(preview) ||
        !gbl_ppu_write_log_valid(machine, preview)) {
        return false;
    }
    for (index = 0u; index < GBL_PPU_AFFINE_BACKGROUND_COUNT; ++index) {
        const gbl_ppu_affine_state *candidate =
            &preview->ppu.affine[index];
        const gbl_ppu_affine_state *prepared =
            &preview->ppu_affine_prepared[index];

        if (candidate->pa != prepared->pa ||
            candidate->pb != prepared->pb ||
            candidate->pc != prepared->pc ||
            candidate->pd != prepared->pd ||
            candidate->programmed_reference_x !=
                prepared->programmed_reference_x ||
            candidate->programmed_reference_y !=
                prepared->programmed_reference_y ||
            candidate->origin_x != prepared->origin_x ||
            candidate->origin_y != prepared->origin_y ||
            candidate->origin_valid != prepared->origin_valid ||
            candidate->hblank_y_suppressed !=
                prepared->hblank_y_suppressed) {
            return false;
        }
    }
    for (index = 0u; index < preview->ppu_row_capture_count; ++index) {
        const gbl_ppu_row_capture *capture =
            &preview->ppu_row_captures[index];
        uint8_t projected_row[GBL_PPU_ROW_BYTES];
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
            (machine->ppu.frame_epoch == capture->frame_epoch &&
             (machine->ppu.row_coverage[coverage_index] & row_mask) != 0u) ||
            (preview->ppu.row_coverage[coverage_index] & row_mask) == 0u ||
            (capture->unsupported_register_bytes &
             ~((UINT32_C(1) << GBL_PPU_RENDER_REGISTER_BYTES) - 1u)) != 0u) {
            return false;
        }
        sampled_ppu.dispcnt = capture->dispcnt;
        sampled_ppu.unsupported_register_bytes =
            capture->unsupported_register_bytes;
        gbl_machine_copy_bytes(sampled_ppu.render_register_bytes,
                               capture->render_register_bytes,
                               sizeof(sampled_ppu.render_register_bytes));
        gbl_machine_copy_bytes(sampled_ppu.bg_register_bytes,
                               capture->bg_register_bytes,
                               sizeof(sampled_ppu.bg_register_bytes));
        gbl_machine_copy_bytes(sampled_ppu.window_register_bytes,
                               capture->window_register_bytes,
                               sizeof(sampled_ppu.window_register_bytes));
        {
            size_t affine_index;

            for (affine_index = 0u;
                 affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT;
                 ++affine_index) {
                sampled_ppu.affine[affine_index] =
                    capture->affine[affine_index];
            }
        }
        if (gbl_ppu_active_invalid_reason(machine, preview, &sampled_ppu,
                                          capture) !=
                GBL_VIDEO_INVALID_REASON_NONE ||
            !gbl_ppu_project_row(machine, preview, capture, projected_row) ||
            gbl_ppu_row_digest(projected_row, sizeof(projected_row), capture) !=
                capture->pixel_digest) {
            return false;
        }
        seen_rows[capture->line] = true;
    }
    return true;
}

bool gbl_machine_commit_time_checked(
    gbl_machine *machine, const gbl_machine_time_preview *preview)
{
    size_t index;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        preview->event_count > GBL_EVENT_CAPACITY ||
        preview->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        preview->input_event_cursor > preview->input_event_count ||
        (preview->dma_transaction_active &&
         preview->dma_transaction_dispatched >
             GBL_MACHINE_MAX_EVENT_DISPATCH) ||
        (!preview->dma_transaction_active &&
         preview->dma_transaction_dispatched != 0u) ||
        preview->ppu_row_capture_count > GBL_PPU_ROW_CAPTURE_MAX ||
        preview->ppu_memory_write_count > GBL_TIME_OPERATION_MAX ||
        !gbl_ppu_commit_preview_valid(machine, preview)) {
        return false;
    }
    gbl_ppu_materialize_capture_plan(machine, preview);
    machine->master_cycles = preview->master_cycles;
    machine->next_event_sequence = preview->next_event_sequence;
    machine->interrupt_flags = preview->interrupt_flags;
    machine->interrupt_enable = preview->interrupt_enable;
    machine->interrupt_master_enable = preview->interrupt_master_enable;
    machine->waitcnt = preview->waitcnt;
    machine->audio = preview->audio;
    machine->dispcnt = preview->ppu.dispcnt;
    machine->ppu = preview->ppu;
    machine->bus_owner = preview->bus_owner;
    machine->dma_last_channel = preview->dma_last_channel;
    machine->dma_last_width = preview->dma_last_width;
    machine->dma_last_source = preview->dma_last_source;
    machine->dma_last_destination = preview->dma_last_destination;
    machine->dma_stream_valid = preview->dma_stream_valid;
    machine->dma_transaction_active = preview->dma_transaction_active;
    machine->dma_transaction_dispatched =
        preview->dma_transaction_dispatched;
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
    return true;
}

void gbl_machine_commit_time(gbl_machine *machine,
                            const gbl_machine_time_preview *preview)
{
    (void)gbl_machine_commit_time_checked(machine, preview);
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
    gbl_dma_bind_sound_channels(&candidate);
    gbl_machine_commit_time(machine, &candidate);
    return GBL_TIME_STATUS_OK;
}

static bool gbl_dma_preview_other_ready(
    const gbl_machine_time_preview *preview, uint8_t excluded_channel)
{
    uint8_t channel_index;

    for (channel_index = 0u; channel_index < GBL_DMA_CHANNEL_COUNT;
         ++channel_index) {
        if (channel_index != excluded_channel &&
            (preview->dma_channels[channel_index].pending ||
             preview->dma_channels[channel_index].active)) {
            return true;
        }
    }
    return false;
}

static gbl_bus_access_class gbl_dma_sound_sequence(
    const gbl_machine_time_preview *preview, uint8_t channel_index,
    uint32_t last_address, uint32_t address)
{
    if (preview->dma_stream_valid &&
        preview->dma_last_channel == channel_index &&
        preview->dma_last_width == 4u &&
        last_address <= UINT32_MAX - UINT32_C(4) &&
        last_address + UINT32_C(4) == address) {
        return GBL_BUS_CLASS_SEQUENTIAL;
    }
    return GBL_BUS_CLASS_NONSEQUENTIAL;
}

static gbl_dma_status gbl_dma_sound_advance(
    const gbl_machine *machine, gbl_machine_time_preview *preview,
    uint64_t cycles, uint64_t initial_cycle, uint64_t cycle_budget,
    size_t *dispatched, size_t *staged_bytes, uint8_t *audio_staging,
    size_t audio_staging_capacity, uint64_t capture_end_cycle)
{
    uint64_t elapsed;
    uint64_t target;
    gbl_time_status status;

    if (preview == NULL || dispatched == NULL || staged_bytes == NULL ||
        preview->master_cycles < initial_cycle) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    elapsed = preview->master_cycles - initial_cycle;
    if (elapsed > cycle_budget || cycles > cycle_budget - elapsed) {
        return GBL_DMA_STATUS_BUDGET_REFUSAL;
    }
    if (cycles > UINT64_MAX - preview->master_cycles) {
        return GBL_DMA_STATUS_TIME_OVERFLOW;
    }
    target = preview->master_cycles + cycles;
    status = gbl_preview_advance_to(
        machine, preview, target, dispatched, audio_staging,
        audio_staging_capacity, staged_bytes, NULL, NULL,
        capture_end_cycle);
    if (status != GBL_TIME_STATUS_OK) {
        return gbl_dma_status_from_time(status);
    }
    return GBL_DMA_STATUS_OK;
}

static bool gbl_dma_observation_append(
    gbl_dma_step_result *observations, size_t observation_capacity,
    size_t *observation_count, const gbl_dma_step_result *observation)
{
    if (observations == NULL) {
        return true;
    }
    if (observation_count == NULL || observation == NULL ||
        *observation_count >= observation_capacity) {
        return false;
    }
    observations[(*observation_count)++] = *observation;
    return true;
}

static uint8_t gbl_dma_higher_priority_channel(
    const gbl_machine *machine, uint8_t channel_index)
{
    uint8_t index;

    for (index = 0u; index < channel_index; ++index) {
        if (machine->dma_channels[index].pending ||
            machine->dma_channels[index].active) {
            return index;
        }
    }
    return GBL_DMA_CHANNEL_COUNT;
}

typedef struct gbl_dma_sound_frame {
    uint8_t channel;
    unsigned words_completed;
    bool startup_complete;
    uint32_t first_source;
    uint32_t last_value;
    gbl_bus_access_class first_source_sequence;
    gbl_bus_access_class first_destination_sequence;
} gbl_dma_sound_frame;

static void gbl_dma_sound_bind_prepared(
    gbl_machine_time_preview *candidate)
{
    gbl_machine_copy_bytes(candidate->ppu_affine_prepared,
                           candidate->ppu.affine,
                           sizeof(candidate->ppu_affine_prepared));
    gbl_ppu_bind_prepared_registers(candidate);
    candidate->audio_prepared = candidate->audio;
    candidate->audio_fifo_reset_mask_prepared =
        candidate->audio_fifo_reset_mask;
    gbl_dma_bind_sound_channels(candidate);
}

static bool gbl_dma_sound_accumulate(
    gbl_dma_step_result *aggregate, const gbl_dma_step_result *part)
{
    uint64_t accepted_units;

    if (aggregate == NULL || part == NULL ||
        part->startup_cycles > UINT64_MAX - aggregate->startup_cycles ||
        part->source_cycles > UINT64_MAX - aggregate->source_cycles ||
        part->destination_cycles >
            UINT64_MAX - aggregate->destination_cycles) {
        return false;
    }
    accepted_units = part->accepted_units;
    if (accepted_units == 0u && part->unit_completed) {
        accepted_units = 1u;
    }
    if (accepted_units > UINT64_MAX - aggregate->accepted_units) {
        return false;
    }
    aggregate->startup_cycles += part->startup_cycles;
    aggregate->source_cycles += part->source_cycles;
    aggregate->destination_cycles += part->destination_cycles;
    aggregate->accepted_units += accepted_units;
    return true;
}

static gbl_dma_status gbl_dma_sound_startup(
    gbl_machine *machine, gbl_dma_sound_frame *frame,
    uint64_t initial_cycle, uint64_t cycle_budget, size_t *dispatched,
    size_t *staged_bytes, uint8_t *audio_staging,
    size_t audio_staging_capacity, uint64_t capture_end_cycle,
    gbl_dma_step_result *observation)
{
    gbl_machine_time_preview candidate;
    gbl_dma_channel_state *channel;
    gbl_dma_step_result startup = { 0 };
    const gbl_dma_channel_state *initial_channel;
    const uint8_t channel_index = frame->channel;
    const uint8_t fifo_index = (uint8_t)(channel_index - 1u);
    const uint8_t source_mode =
        (uint8_t)((machine->dma_channels[channel_index].control >> 7u) & 3u);
    const gbl_dma_trigger_origin expected_origin =
        fifo_index == 0u ? GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A
                         : GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B;
    gbl_dma_status advance_status;

    initial_channel = &machine->dma_channels[channel_index];
    if (initial_channel->trigger_origin != expected_origin ||
        !initial_channel->pending || initial_channel->active ||
        (initial_channel->control & UINT16_C(0x8000)) == 0u ||
        ((initial_channel->control >> 12u) & 3u) != 3u ||
        source_mode > 2u ||
        initial_channel->completed_units > UINT64_MAX - UINT64_C(4) ||
        !gbl_dma_control_valid(
            channel_index,
            (uint16_t)(initial_channel->control & (uint16_t)~UINT16_C(0x0060)))) {
        return GBL_DMA_STATUS_UNSUPPORTED;
    }
    if (cycle_budget < GBL_DMA_STARTUP_CYCLES) {
        return GBL_DMA_STATUS_BUDGET_REFUSAL;
    }

    gbl_machine_time_copy_preview(machine, &candidate);
    channel = &candidate.dma_channels[channel_index];
    channel->remaining = 4u;
    channel->current_source = channel->current_source &
                              gbl_dma_source_mask(channel_index) &
                              ~UINT32_C(3);
    channel->current_destination =
        UINT32_C(0x040000A0) + (uint32_t)fifo_index * 4u;
    channel->pending = false;
    channel->active = true;
    channel->armed = false;
    channel->startup_complete = false;
    if (candidate.bus_owner != GBL_MACHINE_BUS_OWNER_DMA) {
        candidate.dma_stream_valid = false;
    }
    candidate.bus_owner = GBL_MACHINE_BUS_OWNER_DMA;

    advance_status = gbl_dma_sound_advance(
        machine, &candidate, GBL_DMA_STARTUP_CYCLES, initial_cycle,
        cycle_budget, dispatched, staged_bytes, audio_staging,
        audio_staging_capacity, capture_end_cycle);
    if (advance_status != GBL_DMA_STATUS_OK) {
        return advance_status;
    }
    channel = &candidate.dma_channels[channel_index];
    channel->startup_complete = true;
    startup.status = GBL_DMA_STATUS_OK;
    startup.owner = GBL_MACHINE_BUS_OWNER_DMA;
    startup.channel = channel_index;
    startup.trigger_origin = channel->trigger_origin;
    startup.trigger_cycle = channel->trigger_cycle;
    startup.trigger_dispute = channel->trigger_dispute;
    startup.startup_cycles = GBL_DMA_STARTUP_CYCLES;
    startup.remaining = channel->remaining;
    startup.completed = channel->completed_units;
    startup.completion_cycle = candidate.master_cycles;
    gbl_dma_sound_bind_prepared(&candidate);
    if (!gbl_machine_commit_time_checked(machine, &candidate)) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    *observation = startup;
    return GBL_DMA_STATUS_OK;
}

static gbl_dma_status gbl_dma_sound_word(
    gbl_machine *machine, gbl_dma_sound_frame *frame,
    uint64_t initial_cycle, uint64_t cycle_budget, size_t *dispatched,
    size_t *staged_bytes, uint8_t *audio_staging,
    size_t audio_staging_capacity, uint64_t capture_end_cycle,
    gbl_dma_step_result *observation)
{
    gbl_machine_time_preview candidate;
    gbl_dma_channel_state *channel;
    gbl_dma_step_result word = { 0 };
    const uint8_t channel_index = frame->channel;
    const uint8_t fifo_index = (uint8_t)(channel_index - 1u);
    const uint32_t fifo_address =
        UINT32_C(0x040000A0) + (uint32_t)fifo_index * 4u;
    uint32_t source_address;
    uint8_t source_mode;
    gbl_bus_access_class source_sequence;
    gbl_bus_access_class destination_sequence;
    gbl_cycle_cost source_cost;
    gbl_cycle_cost destination_cost;
    uint32_t value;
    gbl_time_status write_status;
    gbl_dma_status advance_status;

    gbl_machine_time_copy_preview(machine, &candidate);
    channel = &candidate.dma_channels[channel_index];
    if (!channel->active || !channel->startup_complete ||
        channel->remaining == 0u ||
        channel->completed_units == UINT64_MAX) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    source_address = channel->current_source;
    source_mode = (uint8_t)((channel->control >> 7u) & 3u);
    if (source_mode > 2u) {
        return GBL_DMA_STATUS_UNSUPPORTED;
    }
    if (candidate.bus_owner != GBL_MACHINE_BUS_OWNER_DMA) {
        candidate.bus_owner = GBL_MACHINE_BUS_OWNER_DMA;
        candidate.dma_stream_valid = false;
    }
    source_sequence = gbl_dma_sound_sequence(
        &candidate, channel_index, candidate.dma_last_source,
        source_address);
    destination_sequence = gbl_dma_sound_sequence(
        &candidate, channel_index, candidate.dma_last_destination,
        fifo_address);
    if (!gbl_bus_quote_waitcnt(candidate.waitcnt, source_address,
                               GBL_BUS_DIRECTION_READ, 4u, source_sequence,
                               GBL_BUS_ROLE_DATA, &source_cost) ||
        !gbl_bus_quote_waitcnt(candidate.waitcnt, fifo_address,
                               GBL_BUS_DIRECTION_WRITE, 4u,
                               destination_sequence, GBL_BUS_ROLE_DATA,
                               &destination_cost) ||
        source_cost.cycles > UINT64_MAX - destination_cost.cycles) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }

    advance_status = gbl_dma_sound_advance(
        machine, &candidate, source_cost.cycles, initial_cycle, cycle_budget,
        dispatched, staged_bytes, audio_staging, audio_staging_capacity,
        capture_end_cycle);
    if (advance_status != GBL_DMA_STATUS_OK) {
        return advance_status;
    }
    if (!gbl_bus_dma_read_word_checked(machine, &candidate, source_address,
                                       source_sequence, &value)) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    advance_status = gbl_dma_sound_advance(
        machine, &candidate, destination_cost.cycles, initial_cycle,
        cycle_budget, dispatched, staged_bytes, audio_staging,
        audio_staging_capacity, capture_end_cycle);
    if (advance_status != GBL_DMA_STATUS_OK) {
        return advance_status;
    }
    write_status =
        gbl_preview_bus_write(&candidate, fifo_address, 4u, value);
    if (write_status != GBL_TIME_STATUS_OK) {
        return gbl_dma_status_from_time(write_status);
    }

    channel = &candidate.dma_channels[channel_index];
    channel->read_latch = value;
    if (source_mode == 0u) {
        channel->current_source =
            (channel->current_source + 4u) &
            gbl_dma_source_mask(channel_index);
    } else if (source_mode == 1u) {
        channel->current_source =
            (channel->current_source - 4u) &
            gbl_dma_source_mask(channel_index);
    }
    channel->current_destination = fifo_address;
    --channel->remaining;
    ++channel->completed_units;
    candidate.dma_last_channel = channel_index;
    candidate.dma_last_width = 4u;
    candidate.dma_last_source = source_address;
    candidate.dma_last_destination = fifo_address;
    candidate.dma_stream_valid = true;

    word.status = GBL_DMA_STATUS_OK;
    word.owner = GBL_MACHINE_BUS_OWNER_DMA;
    word.channel = channel_index;
    word.trigger_origin = channel->trigger_origin;
    word.trigger_cycle = channel->trigger_cycle;
    word.trigger_dispute = channel->trigger_dispute;
    word.source_address = source_address;
    word.destination_address = fifo_address;
    word.value = value;
    word.width = 4u;
    word.source_cycles = source_cost.cycles;
    word.destination_cycles = destination_cost.cycles;
    word.source_sequence = source_sequence;
    word.destination_sequence = destination_sequence;
    word.remaining = channel->remaining;
    word.completed = channel->completed_units;
    word.completion_cycle = candidate.master_cycles;
    word.unit_completed = true;
    word.accepted_units = UINT64_C(1);

    gbl_dma_sound_bind_prepared(&candidate);
    if (!gbl_machine_commit_time_checked(machine, &candidate)) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    machine->open_bus = value;
    if (machine->bus_owner == GBL_MACHINE_BUS_OWNER_CPU) {
        machine->cpu.force_nonsequential_fetch = true;
    }
    if (frame->words_completed == 0u) {
        frame->first_source = source_address;
        frame->first_source_sequence = source_sequence;
        frame->first_destination_sequence = destination_sequence;
    }
    frame->last_value = value;
    ++frame->words_completed;
    *observation = word;
    return GBL_DMA_STATUS_OK;
}

static gbl_dma_status gbl_dma_sound_finish(
    gbl_machine *machine, uint8_t channel_index)
{
    gbl_machine_time_preview candidate;
    gbl_dma_channel_state *channel;

    gbl_machine_time_copy_preview(machine, &candidate);
    channel = &candidate.dma_channels[channel_index];
    if (!channel->active || channel->remaining != 0u) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    channel->active = false;
    channel->pending = false;
    channel->startup_complete = false;
    channel->remaining = 0u;
    channel->armed = true;
    channel->trigger_origin = GBL_DMA_TRIGGER_ORIGIN_NONE;
    channel->trigger_cycle = 0u;
    channel->trigger_dispute = GBL_DMA_DISPUTE_NONE;
    if ((channel->control & UINT16_C(0x4000)) != 0u) {
        candidate.interrupt_flags = (uint16_t)(
            candidate.interrupt_flags |
            (uint16_t)(UINT16_C(0x0100) << channel_index));
    }
    if (!gbl_dma_preview_other_ready(&candidate, channel_index)) {
        candidate.bus_owner = GBL_MACHINE_BUS_OWNER_CPU;
        candidate.dma_stream_valid = false;
    }
    gbl_dma_sound_bind_prepared(&candidate);
    if (!gbl_machine_commit_time_checked(machine, &candidate)) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    if (machine->bus_owner == GBL_MACHINE_BUS_OWNER_CPU) {
        machine->cpu.force_nonsequential_fetch = true;
    }
    return GBL_DMA_STATUS_OK;
}

static gbl_dma_status gbl_machine_dma_step_single(
    gbl_machine *machine, uint64_t cycle_budget,
    gbl_dma_step_result *result, gbl_audio_capture *capture,
    uint8_t *audio_staging, size_t audio_staging_capacity,
    size_t *staged_audio_bytes, uint64_t capture_end_cycle);

static gbl_dma_status gbl_machine_dma_stage_internal_audio(
    const gbl_machine *machine, uint64_t cycles, gbl_audio_capture *capture,
    uint8_t *audio_staging, size_t audio_staging_capacity,
    size_t *staged_audio_bytes, uint64_t capture_end_cycle)
{
    const gbl_time_operation operation = {
        GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, cycles
    };
    gbl_machine_time_preview preview;
    gbl_time_status time_status;
    uint64_t final_cycle;
    uint64_t expected_frames;
    size_t prior_staged_bytes;
    size_t append_capacity;
    size_t appended_bytes = 0u;

    if (capture == NULL) {
        return GBL_DMA_STATUS_OK;
    }
    if (machine == NULL || staged_audio_bytes == NULL ||
        audio_staging == NULL ||
        audio_staging_capacity > GBL_AUDIO_MAX_CAPTURE_BYTES ||
        capture->bytes_written > capture->capacity ||
        capture->bytes_written > GBL_AUDIO_MAX_CAPTURE_BYTES) {
        return GBL_DMA_STATUS_INVALID_ARGUMENT;
    }
    prior_staged_bytes = *staged_audio_bytes;
    if (prior_staged_bytes > audio_staging_capacity ||
        prior_staged_bytes > capture->capacity - capture->bytes_written ||
        prior_staged_bytes > GBL_AUDIO_MAX_CAPTURE_BYTES -
                                 capture->bytes_written) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    append_capacity = audio_staging_capacity - prior_staged_bytes;
    if (append_capacity > capture->capacity - capture->bytes_written -
                              prior_staged_bytes) {
        append_capacity = capture->capacity - capture->bytes_written -
                          prior_staged_bytes;
    }
    if (append_capacity > GBL_AUDIO_MAX_CAPTURE_BYTES -
                              capture->bytes_written -
                              prior_staged_bytes) {
        append_capacity = GBL_AUDIO_MAX_CAPTURE_BYTES -
                          capture->bytes_written - prior_staged_bytes;
    }
    time_status = gbl_machine_prepare_operations_audio_until(
        machine, &operation, 1u, &preview,
        audio_staging + prior_staged_bytes, append_capacity,
        &appended_bytes, capture_end_cycle);
    if (time_status != GBL_TIME_STATUS_OK) {
        return gbl_dma_status_from_time(time_status);
    }
    if (appended_bytes > append_capacity ||
        appended_bytes > SIZE_MAX - prior_staged_bytes) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    *staged_audio_bytes = prior_staged_bytes + appended_bytes;
    final_cycle = preview.master_cycles < capture_end_cycle
                      ? preview.master_cycles
                      : capture_end_cycle;
    if (final_cycle < capture->last_accepted_cycle) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    expected_frames = final_cycle / GBL_AUDIO_SAMPLE_CYCLES -
                      capture->last_accepted_cycle /
                          GBL_AUDIO_SAMPLE_CYCLES;
    if (*staged_audio_bytes > capture->capacity - capture->bytes_written ||
        *staged_audio_bytes > GBL_AUDIO_MAX_CAPTURE_BYTES -
                                  capture->bytes_written ||
        expected_frames > (uint64_t)(SIZE_MAX / GBL_AUDIO_FRAME_BYTES) ||
        *staged_audio_bytes !=
            (size_t)expected_frames * GBL_AUDIO_FRAME_BYTES) {
        return GBL_DMA_STATUS_UNIT_REFUSAL;
    }
    return GBL_DMA_STATUS_OK;
}

static gbl_dma_status gbl_machine_dma_sound_burst(
    gbl_machine *machine, uint8_t channel_index, uint64_t cycle_budget,
    gbl_dma_step_result *result, gbl_dma_step_result *observations,
    size_t observation_capacity, size_t *observation_count,
    gbl_audio_capture *capture, uint8_t *audio_staging,
    size_t audio_staging_capacity, uint64_t capture_end_cycle,
    size_t *staged_bytes_out)
{
    gbl_dma_sound_frame frames[2] = { { 0 } };
    gbl_dma_step_result step = { 0 };
    size_t frame_count = 1u;
    uint64_t initial_cycle;
    size_t dispatched;
    size_t staged_bytes = 0u;

    if (machine == NULL || result == NULL ||
        channel_index < 1u || channel_index > 2u ||
        machine->event_count > GBL_EVENT_CAPACITY ||
        machine->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        machine->input_event_cursor > machine->input_event_count) {
        return GBL_DMA_STATUS_INVALID_ARGUMENT;
    }
    initial_cycle = machine->master_cycles;
    dispatched = machine->dma_transaction_active
                     ? machine->dma_transaction_dispatched
                     : 0u;
    frames[0].channel = channel_index;
    step.channel = channel_index;
    step.trigger_origin = machine->dma_channels[channel_index].trigger_origin;
    step.trigger_cycle = machine->dma_channels[channel_index].trigger_cycle;
    step.trigger_dispute = machine->dma_channels[channel_index].trigger_dispute;

    while (frame_count > 0u) {
        gbl_dma_sound_frame *frame = &frames[frame_count - 1u];

        if (!frame->startup_complete) {
            gbl_dma_step_result startup = { 0 };
            gbl_dma_status status = gbl_dma_sound_startup(
                machine, frame, initial_cycle, cycle_budget, &dispatched,
                &staged_bytes, audio_staging, audio_staging_capacity,
                capture_end_cycle, &startup);

            if (status != GBL_DMA_STATUS_OK) {
                return status;
            }
            if (!gbl_dma_observation_append(
                    observations, observation_capacity, observation_count,
                    &startup)) {
                return GBL_DMA_STATUS_OUTPUT_REFUSAL;
            }
            if (!gbl_dma_sound_accumulate(&step, &startup)) {
                return GBL_DMA_STATUS_TIME_OVERFLOW;
            }
            frame->startup_complete = true;
            continue;
        }

        {
            const uint8_t higher_channel =
                gbl_dma_higher_priority_channel(machine, frame->channel);

            if (higher_channel < GBL_DMA_CHANNEL_COUNT) {
                const gbl_dma_trigger_origin origin =
                    machine->dma_channels[higher_channel].trigger_origin;

                if (origin == GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A ||
                    origin == GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B) {
                    size_t stack_index;

                    if (higher_channel < 1u || higher_channel > 2u) {
                        return GBL_DMA_STATUS_UNSUPPORTED;
                    }
                    if (frame_count >=
                        sizeof(frames) / sizeof(frames[0])) {
                        return GBL_DMA_STATUS_UNSUPPORTED;
                    }
                    for (stack_index = 0u; stack_index < frame_count;
                         ++stack_index) {
                        if (frames[stack_index].channel == higher_channel) {
                            return GBL_DMA_STATUS_UNSUPPORTED;
                        }
                    }
                    frames[frame_count++] =
                        (gbl_dma_sound_frame){ .channel = higher_channel };
                    continue;
                }
                {
                    gbl_dma_step_result interleaved = { 0 };
                    const uint64_t elapsed =
                        machine->master_cycles - initial_cycle;
                    gbl_dma_status status;

                    if (elapsed > cycle_budget) {
                        return GBL_DMA_STATUS_BUDGET_REFUSAL;
                    }
                    status = gbl_machine_dma_step_single(
                        machine, cycle_budget - elapsed, &interleaved,
                        capture, audio_staging, audio_staging_capacity,
                        &staged_bytes, capture_end_cycle);
                    if (status != GBL_DMA_STATUS_OK) {
                        return status;
                    }
                    dispatched = machine->dma_transaction_dispatched;
                    if (!gbl_dma_observation_append(
                            observations, observation_capacity,
                            observation_count, &interleaved)) {
                        return GBL_DMA_STATUS_OUTPUT_REFUSAL;
                    }
                    if (!gbl_dma_sound_accumulate(&step, &interleaved)) {
                        return GBL_DMA_STATUS_TIME_OVERFLOW;
                    }
                    continue;
                }
            }
        }

        if (frame->words_completed == 4u) {
            const gbl_dma_status status =
                gbl_dma_sound_finish(machine, frame->channel);

            if (status != GBL_DMA_STATUS_OK) {
                return status;
            }
            --frame_count;
            continue;
        }
        {
            gbl_dma_step_result word = { 0 };
            const gbl_dma_status status = gbl_dma_sound_word(
                machine, frame, initial_cycle, cycle_budget, &dispatched,
                &staged_bytes, audio_staging, audio_staging_capacity,
                capture_end_cycle, &word);

            if (status != GBL_DMA_STATUS_OK) {
                return status;
            }
            if (!gbl_dma_observation_append(
                    observations, observation_capacity, observation_count,
                    &word)) {
                return GBL_DMA_STATUS_OUTPUT_REFUSAL;
            }
            if (!gbl_dma_sound_accumulate(&step, &word)) {
                return GBL_DMA_STATUS_TIME_OVERFLOW;
            }
        }
    }

    step.status = GBL_DMA_STATUS_OK;
    step.owner = GBL_MACHINE_BUS_OWNER_DMA;
    step.source_address = frames[0].first_source;
    step.destination_address =
        UINT32_C(0x040000A0) +
        (uint32_t)(channel_index - 1u) * 4u;
    step.value = frames[0].last_value;
    step.width = 4u;
    step.unit_completed = true;
    step.remaining = 0u;
    step.completed = machine->dma_channels[channel_index].completed_units;
    step.source_sequence = frames[0].first_source_sequence;
    step.destination_sequence = frames[0].first_destination_sequence;
    step.completion_cycle = machine->master_cycles;
    *result = step;
    if (staged_bytes_out != NULL) {
        *staged_bytes_out = staged_bytes;
    }
    return GBL_DMA_STATUS_OK;
}

static gbl_dma_status gbl_machine_dma_step_single(gbl_machine *machine,
                                                  uint64_t cycle_budget,
                                                  gbl_dma_step_result *result,
                                                  gbl_audio_capture *capture,
                                                  uint8_t *audio_staging,
                                                  size_t audio_staging_capacity,
                                                  size_t *staged_audio_bytes,
                                                  uint64_t capture_end_cycle)
{
    uint8_t channel_index;
    gbl_dma_channel_state *channel;
    gbl_dma_step_result step = { 0 };
    gbl_time_status time_status;
    gbl_cycle_cost audio_source_cost;
    gbl_cycle_cost audio_destination_cost;
    gbl_bus_access_class audio_source_sequence =
        GBL_BUS_CLASS_NONSEQUENTIAL;
    gbl_bus_access_class audio_destination_sequence =
        GBL_BUS_CLASS_NONSEQUENTIAL;
    uint64_t audio_cycles = 0u;

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
    if (channel->trigger_origin ==
            GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A ||
        channel->trigger_origin ==
            GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B) {
        /* Sound bursts own the iterative arbitration stack in
         * gbl_machine_dma_sound_burst. Reaching this branch would recurse
         * through the ordinary single-unit selector and lose its observation
         * stream. */
        step.channel = channel_index;
        step.status = GBL_DMA_STATUS_UNSUPPORTED;
        *result = step;
        return step.status;
    }
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
        step.status = gbl_machine_dma_stage_internal_audio(
            machine, GBL_DMA_STARTUP_CYCLES, capture, audio_staging,
            audio_staging_capacity, staged_audio_bytes, capture_end_cycle);
        if (step.status != GBL_DMA_STATUS_OK) {
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
        gbl_dma_bind_sound_channels(&preview);
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
    if (capture != NULL) {
        const uint8_t width =
            (channel->control & UINT16_C(0x0400)) != 0u ? 4u : 2u;

        if (machine->bus_owner == GBL_MACHINE_BUS_OWNER_DMA &&
            machine->dma_stream_valid &&
            machine->dma_last_channel == channel_index &&
            machine->dma_last_width == width) {
            if (machine->dma_last_source <= UINT32_MAX - width &&
                machine->dma_last_source + width == channel->current_source) {
                audio_source_sequence = GBL_BUS_CLASS_SEQUENTIAL;
            }
            if (machine->dma_last_destination <= UINT32_MAX - width &&
                machine->dma_last_destination + width ==
                    channel->current_destination) {
                audio_destination_sequence = GBL_BUS_CLASS_SEQUENTIAL;
            }
        }
        if (!gbl_bus_quote(machine, channel->current_source,
                           GBL_BUS_DIRECTION_READ, width,
                           audio_source_sequence, GBL_BUS_ROLE_DATA,
                           &audio_source_cost) ||
            !gbl_bus_quote(machine, channel->current_destination,
                           GBL_BUS_DIRECTION_WRITE, width,
                           audio_destination_sequence, GBL_BUS_ROLE_DATA,
                           &audio_destination_cost) ||
            audio_source_cost.cycles >
                UINT64_MAX - audio_destination_cost.cycles) {
            step.status = GBL_DMA_STATUS_UNIT_REFUSAL;
            *result = step;
            return step.status;
        }
        audio_cycles = audio_source_cost.cycles +
                       audio_destination_cost.cycles;
        if (audio_cycles > cycle_budget) {
            step.status = GBL_DMA_STATUS_BUDGET_REFUSAL;
            *result = step;
            return step.status;
        }
        step.status = gbl_machine_dma_stage_internal_audio(
            machine, audio_cycles, capture, audio_staging,
            audio_staging_capacity, staged_audio_bytes, capture_end_cycle);
        if (step.status != GBL_DMA_STATUS_OK) {
            *result = step;
            return step.status;
        }
    }
    if (machine->bus_owner != GBL_MACHINE_BUS_OWNER_DMA) {
        machine->bus_owner = GBL_MACHINE_BUS_OWNER_DMA;
        machine->dma_stream_valid = false;
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

static bool gbl_machine_dma_audio_publishable(
    const gbl_audio_capture *capture, size_t staged_bytes,
    uint64_t accepted_cycle, uint64_t capture_end_cycle)
{
    uint64_t final_cycle;
    uint64_t frames;

    if (capture == NULL) {
        return staged_bytes == 0u;
    }
    if (capture->bytes_written > GBL_AUDIO_MAX_CAPTURE_BYTES) {
        return false;
    }
    final_cycle = accepted_cycle < capture_end_cycle
                      ? accepted_cycle
                      : capture_end_cycle;
    if (final_cycle < capture->last_accepted_cycle) {
        return false;
    }
    frames = final_cycle / GBL_AUDIO_SAMPLE_CYCLES -
             capture->last_accepted_cycle / GBL_AUDIO_SAMPLE_CYCLES;
    return frames <= (uint64_t)(SIZE_MAX / GBL_AUDIO_FRAME_BYTES) &&
           staged_bytes == (size_t)frames * GBL_AUDIO_FRAME_BYTES &&
           capture->bytes_written <= capture->capacity &&
           staged_bytes <= capture->capacity - capture->bytes_written &&
           staged_bytes <= GBL_AUDIO_MAX_CAPTURE_BYTES -
                               capture->bytes_written;
}

static void gbl_machine_dma_audio_publish(
    gbl_audio_capture *capture, const uint8_t *staging, size_t staged_bytes,
    uint64_t accepted_cycle, uint64_t capture_end_cycle)
{
    uint64_t final_cycle;

    if (capture == NULL) {
        return;
    }
    if (staged_bytes != 0u) {
        gbl_machine_copy_bytes(capture->destination + capture->bytes_written,
                               staging, staged_bytes);
    }
    capture->bytes_written += staged_bytes;
    final_cycle = accepted_cycle < capture_end_cycle
                      ? accepted_cycle
                      : capture_end_cycle;
    capture->last_accepted_cycle = final_cycle;
}

gbl_dma_status gbl_machine_dma_step_observed_audio(
    gbl_machine *machine, uint64_t cycle_budget, gbl_dma_step_result *result,
    gbl_dma_step_result *observations, size_t observation_capacity,
    size_t *observation_count, gbl_audio_capture *capture,
    uint8_t *audio_staging, size_t audio_staging_capacity,
    uint64_t capture_end_cycle)
{
    gbl_dma_step_result step = { 0 };
    gbl_dma_step_result *staged_observations = NULL;
    gbl_machine *candidate;
    size_t staged_count = 0u;
    size_t staged_observation_bytes = 0u;
    size_t staged_audio_bytes = 0u;
    uint8_t channel_index;
    gbl_dma_status status;

    if (machine == NULL || result == NULL || !machine->initialized ||
        (observations != NULL && observation_count == NULL) ||
        (capture == NULL &&
         (audio_staging != NULL || audio_staging_capacity != 0u)) ||
        (capture != NULL &&
         (audio_staging == NULL || audio_staging_capacity == 0u ||
          audio_staging_capacity > GBL_AUDIO_MAX_CAPTURE_BYTES ||
          !gbl_audio_capture_validate(machine, capture) ||
          capture_end_cycle < capture->initial_cycle ||
          capture_end_cycle - capture->initial_cycle >
              GBL_AUDIO_MAX_CAPTURE_CYCLES ||
          capture->last_accepted_cycle !=
              (machine->master_cycles < capture_end_cycle
                   ? machine->master_cycles
                   : capture_end_cycle)))) {
        return GBL_DMA_STATUS_INVALID_ARGUMENT;
    }
    for (channel_index = 0u; channel_index < GBL_DMA_CHANNEL_COUNT;
         ++channel_index) {
        if (machine->dma_channels[channel_index].pending ||
            machine->dma_channels[channel_index].active) {
            break;
        }
    }
    if (channel_index == GBL_DMA_CHANNEL_COUNT) {
        return gbl_machine_dma_step_single(
            machine, cycle_budget, result, capture, audio_staging,
            audio_staging_capacity, &staged_audio_bytes, capture_end_cycle);
    }
    if (observations != NULL) {
        if (observation_capacity == 0u ||
            observation_capacity > GBL_DMA_OBSERVATION_CAPACITY ||
            observation_capacity > SIZE_MAX / sizeof(*staged_observations)) {
            return GBL_DMA_STATUS_OUTPUT_REFUSAL;
        }
        staged_observation_bytes =
            observation_capacity * sizeof(*staged_observations);
        staged_observations =
            gbl_machine_allocate_scratch(machine,
                                         staged_observation_bytes);
        if (staged_observations == NULL) {
            return GBL_DMA_STATUS_UNIT_REFUSAL;
        }
    }
    if (channel_index < GBL_DMA_CHANNEL_COUNT &&
        (machine->dma_channels[channel_index].trigger_origin ==
             GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A ||
         machine->dma_channels[channel_index].trigger_origin ==
             GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B)) {
        candidate = gbl_machine_allocate_scratch(machine, sizeof(*candidate));

        if (candidate == NULL) {
            gbl_machine_release_scratch(machine, staged_observations,
                                        staged_observation_bytes);
            return GBL_DMA_STATUS_UNIT_REFUSAL;
        }
        gbl_machine_copy_bytes((uint8_t *)candidate, (const uint8_t *)machine,
                               sizeof(*candidate));
        const bool owns_transaction = !candidate->dma_transaction_active;

        if (owns_transaction) {
            candidate->dma_transaction_active = true;
            candidate->dma_transaction_dispatched = 0u;
        }
        status = gbl_machine_dma_sound_burst(
            candidate, channel_index, cycle_budget, &step,
            staged_observations, observation_capacity, &staged_count,
            capture, audio_staging, audio_staging_capacity,
            capture_end_cycle, &staged_audio_bytes);
        if (status == GBL_DMA_STATUS_OK) {
            if (!gbl_machine_dma_audio_publishable(
                    capture, staged_audio_bytes, candidate->master_cycles,
                    capture_end_cycle)) {
                status = GBL_DMA_STATUS_UNIT_REFUSAL;
            }
        }
        if (status == GBL_DMA_STATUS_OK) {
            if (owns_transaction) {
                candidate->dma_transaction_active = false;
                candidate->dma_transaction_dispatched = 0u;
            }
            /* The private candidate contains all ordered device and memory
             * effects. A single structure publication keeps a late refusal
             * from exposing any prefix on the caller's machine. */
            gbl_machine_copy_bytes((uint8_t *)machine,
                                   (const uint8_t *)candidate,
                                   sizeof(*machine));
            gbl_machine_dma_audio_publish(
                capture, audio_staging, staged_audio_bytes,
                machine->master_cycles, capture_end_cycle);
            *result = step;
            if (observations != NULL) {
                gbl_machine_copy_bytes((uint8_t *)observations,
                                       (const uint8_t *)staged_observations,
                                       staged_count * sizeof(*observations));
                *observation_count = staged_count;
            }
        }
        gbl_machine_release_scratch(machine, candidate, sizeof(*candidate));
        gbl_machine_release_scratch(machine, staged_observations,
                                    staged_observation_bytes);
        return status;
    }

    status = gbl_machine_dma_step_single(
        machine, cycle_budget, &step, capture, audio_staging,
        audio_staging_capacity, &staged_audio_bytes, capture_end_cycle);
    if (status == GBL_DMA_STATUS_OK && capture != NULL) {
        if (!gbl_machine_dma_audio_publishable(
                capture, staged_audio_bytes, machine->master_cycles,
                capture_end_cycle)) {
            status = GBL_DMA_STATUS_UNIT_REFUSAL;
        } else {
            gbl_machine_dma_audio_publish(
                capture, audio_staging, staged_audio_bytes,
                machine->master_cycles, capture_end_cycle);
        }
    }
    if (status == GBL_DMA_STATUS_OK && observations != NULL) {
        step.accepted_units = step.unit_completed ? UINT64_C(1) : 0u;
        staged_observations[staged_count++] = step;
        gbl_machine_copy_bytes((uint8_t *)observations,
                               (const uint8_t *)staged_observations,
                               sizeof(*observations));
        *observation_count = staged_count;
    }
    *result = step;
    gbl_machine_release_scratch(machine, staged_observations,
                                staged_observation_bytes);
    return status;
}

gbl_dma_status gbl_machine_dma_step_observed(
    gbl_machine *machine, uint64_t cycle_budget, gbl_dma_step_result *result,
    gbl_dma_step_result *observations, size_t observation_capacity,
    size_t *observation_count)
{
    return gbl_machine_dma_step_observed_audio(
        machine, cycle_budget, result, observations, observation_capacity,
        observation_count, NULL, NULL, 0u, UINT64_MAX);
}

gbl_dma_status gbl_machine_dma_step(gbl_machine *machine,
                                    uint64_t cycle_budget,
                                    gbl_dma_step_result *result)
{
    return gbl_machine_dma_step_observed(machine, cycle_budget, result, NULL,
                                         0u, NULL);
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
    machine->audio = (gbl_audio_state){
        .cycle_anchor = 0u,
        .soundbias = UINT16_C(0x0200)
    };
    machine->bus_owner = GBL_MACHINE_BUS_OWNER_CPU;
    machine->dma_last_channel = 0u;
    machine->dma_last_width = 0u;
    machine->dma_last_source = 0u;
    machine->dma_last_destination = 0u;
    machine->dma_stream_valid = false;
    machine->dma_transaction_active = false;
    machine->dma_transaction_dispatched = 0u;
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
    machine->ppu.affine[0].pa = INT16_C(0x0100);
    machine->ppu.affine[0].pd = INT16_C(0x0100);
    machine->ppu.affine[0].origin_valid = true;
    machine->ppu.affine[1].pa = INT16_C(0x0100);
    machine->ppu.affine[1].pd = INT16_C(0x0100);
    machine->ppu.affine[1].origin_valid = true;
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
