#ifndef GBL_TESTS_STATE_CHECK_H
#define GBL_TESTS_STATE_CHECK_H

#include "gamebloke/gamebloke.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool gbl_test_bytes_equal(const uint8_t *left, const uint8_t *right,
                                 size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static bool gbl_test_u32_array_equal(const uint32_t *left,
                                     const uint32_t *right, size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static bool gbl_test_diagnostics_equal(const gbl_header_diagnostics *left,
                                       const gbl_header_diagnostics *right)
{
    return left->loaded == right->loaded &&
           left->canonical_logo == right->canonical_logo &&
           left->logo_reason == right->logo_reason &&
           left->logo_window == right->logo_window &&
           left->checksum == right->checksum &&
           left->game_code == right->game_code;
}

static bool gbl_test_event_equal(const gbl_event *left,
                                const gbl_event *right)
{
    return left->cycle == right->cycle &&
           left->sequence == right->sequence &&
           left->generation == right->generation &&
           left->priority == right->priority && left->kind == right->kind &&
           left->timer_index == right->timer_index &&
           left->dma_channel == right->dma_channel &&
           left->live == right->live &&
           left->input_index == right->input_index;
}

static bool gbl_test_input_event_equal(const gbl_input_event *left,
                                       const gbl_input_event *right)
{
    return left->cycle == right->cycle &&
           left->pressed_mask == right->pressed_mask;
}

static bool gbl_test_ppu_affine_equal(const gbl_ppu_affine_state *left,
                                     const gbl_ppu_affine_state *right)
{
    return left->pa == right->pa && left->pb == right->pb &&
           left->pc == right->pc && left->pd == right->pd &&
           left->programmed_reference_x == right->programmed_reference_x &&
           left->programmed_reference_y == right->programmed_reference_y &&
           left->origin_x == right->origin_x &&
           left->origin_y == right->origin_y &&
           left->origin_valid == right->origin_valid &&
           left->hblank_y_suppressed == right->hblank_y_suppressed;
}

static bool gbl_test_ppu_affine_array_equal(
    const gbl_ppu_affine_state left[GBL_PPU_AFFINE_BACKGROUND_COUNT],
    const gbl_ppu_affine_state right[GBL_PPU_AFFINE_BACKGROUND_COUNT])
{
    size_t index;

    for (index = 0u; index < GBL_PPU_AFFINE_BACKGROUND_COUNT; ++index) {
        if (!gbl_test_ppu_affine_equal(&left[index], &right[index])) {
            return false;
        }
    }
    return true;
}

static bool gbl_test_ppu_equal(const gbl_ppu_timing_state *left,
                               const gbl_ppu_timing_state *right)
{
    return left->line_origin == right->line_origin &&
           left->generation == right->generation &&
           left->frame_epoch == right->frame_epoch &&
           left->line == right->line &&
           left->scheduled_line == right->scheduled_line &&
           left->dispcnt == right->dispcnt &&
           left->dispstat == right->dispstat &&
           left->compare == right->compare &&
           left->captured_rows == right->captured_rows &&
           left->unsupported_register_bytes ==
               right->unsupported_register_bytes &&
           gbl_test_bytes_equal(left->render_register_bytes,
                                right->render_register_bytes,
                                sizeof(left->render_register_bytes)) &&
           gbl_test_bytes_equal(left->bg_register_bytes,
                                right->bg_register_bytes,
                                sizeof(left->bg_register_bytes)) &&
           gbl_test_bytes_equal(left->window_register_bytes,
                                right->window_register_bytes,
                                sizeof(left->window_register_bytes)) &&
           gbl_test_ppu_affine_array_equal(left->affine, right->affine) &&
           left->capture_state == right->capture_state &&
           left->capture_reason == right->capture_reason &&
           left->next_boundary_hblank == right->next_boundary_hblank &&
           left->frame_started == right->frame_started &&
           gbl_test_bytes_equal(left->row_coverage, right->row_coverage,
                                sizeof(left->row_coverage));
}

static bool gbl_test_ppu_video_equal(const gbl_machine *left,
                                     const gbl_machine *right)
{
    return left != NULL && right != NULL &&
           gbl_test_ppu_equal(&left->ppu, &right->ppu) &&
           gbl_test_bytes_equal(left->video_frame, right->video_frame,
                                GBL_PPU_VIDEO_BYTES);
}

static bool gbl_test_ppu_row_capture_equal(
    const gbl_ppu_row_capture *left, const gbl_ppu_row_capture *right)
{
    return left->cycle == right->cycle &&
           left->frame_epoch == right->frame_epoch &&
           left->pixel_digest == right->pixel_digest &&
           left->line == right->line &&
           left->dispcnt == right->dispcnt &&
           left->prior_write_count == right->prior_write_count &&
           left->unsupported_register_bytes ==
               right->unsupported_register_bytes &&
           gbl_test_bytes_equal(left->render_register_bytes,
                                right->render_register_bytes,
                                sizeof(left->render_register_bytes)) &&
           gbl_test_bytes_equal(left->bg_register_bytes,
                                right->bg_register_bytes,
                                sizeof(left->bg_register_bytes)) &&
           gbl_test_bytes_equal(left->window_register_bytes,
                                right->window_register_bytes,
                                sizeof(left->window_register_bytes)) &&
           gbl_test_ppu_affine_array_equal(left->affine, right->affine) &&
           gbl_test_bytes_equal(left->obj_selected_mask,
                                right->obj_selected_mask,
                                sizeof(left->obj_selected_mask)) &&
           left->obj_selected_count == right->obj_selected_count &&
           left->obj_omitted_count == right->obj_omitted_count &&
           left->obj_work_used == right->obj_work_used;
}

static bool gbl_test_ppu_memory_write_equal(
    const gbl_ppu_memory_write *left, const gbl_ppu_memory_write *right)
{
    return left->cycle == right->cycle &&
           left->address == right->address &&
           left->value == right->value &&
           left->dispcnt == right->dispcnt &&
           left->width == right->width &&
           left->region == right->region;
}

static bool gbl_test_timer_equal(const gbl_timer_state *left,
                                const gbl_timer_state *right)
{
    return left->reload == right->reload && left->counter == right->counter &&
           left->divisor == right->divisor && left->phase == right->phase &&
           left->anchor_cycle == right->anchor_cycle &&
           left->generation == right->generation &&
           left->enabled == right->enabled &&
           left->irq_enabled == right->irq_enabled &&
           left->cascade == right->cascade;
}

static bool gbl_test_dma_channel_equal(const gbl_dma_channel_state *left,
                                      const gbl_dma_channel_state *right)
{
    return left->source_address == right->source_address &&
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

static inline bool gbl_test_time_state_equal(const gbl_machine *left,
                                             const gbl_machine *right)
{
    size_t index;

    if (left == NULL || right == NULL ||
        left->master_cycles != right->master_cycles ||
        left->next_event_sequence != right->next_event_sequence ||
        left->power_state != right->power_state ||
        left->interrupt_flags != right->interrupt_flags ||
        left->interrupt_enable != right->interrupt_enable ||
        left->interrupt_master_enable != right->interrupt_master_enable ||
        left->waitcnt != right->waitcnt ||
        !gbl_test_ppu_equal(&left->ppu, &right->ppu) ||
        left->bus_owner != right->bus_owner ||
        left->dma_last_channel != right->dma_last_channel ||
        left->dma_last_width != right->dma_last_width ||
        left->dma_last_source != right->dma_last_source ||
        left->dma_last_destination != right->dma_last_destination ||
        left->dma_stream_valid != right->dma_stream_valid ||
        left->cpu.force_nonsequential_fetch !=
            right->cpu.force_nonsequential_fetch ||
        left->pressed_keys != right->pressed_keys ||
        left->key_control != right->key_control ||
        left->key_condition != right->key_condition ||
        left->key_irq_condition != right->key_irq_condition ||
        left->event_count != right->event_count ||
        left->event_count > GBL_EVENT_CAPACITY ||
        left->input_generation != right->input_generation ||
        left->input_event_count != right->input_event_count ||
        left->input_event_cursor != right->input_event_cursor ||
        left->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        left->input_event_cursor > left->input_event_count) {
        return false;
    }
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        if (!gbl_test_event_equal(&left->events[index],
                                  &right->events[index])) {
            return false;
        }
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        if (!gbl_test_timer_equal(&left->timers[index],
                                  &right->timers[index])) {
            return false;
        }
    }
    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        if (!gbl_test_dma_channel_equal(&left->dma_channels[index],
                                        &right->dma_channels[index])) {
            return false;
        }
    }
    for (index = 0u; index < GBL_INPUT_EVENT_CAPACITY; ++index) {
        if (!gbl_test_input_event_equal(&left->input_events[index],
                                        &right->input_events[index])) {
            return false;
        }
    }
    return true;
}

static bool gbl_test_time_preview_equal(
    const gbl_machine_time_preview *left,
    const gbl_machine_time_preview *right)
{
    size_t index;

    if (left == NULL || right == NULL ||
        left->master_cycles != right->master_cycles ||
        left->next_event_sequence != right->next_event_sequence ||
        left->interrupt_flags != right->interrupt_flags ||
        left->interrupt_enable != right->interrupt_enable ||
        left->interrupt_master_enable != right->interrupt_master_enable ||
        left->waitcnt != right->waitcnt ||
        !gbl_test_ppu_equal(&left->ppu, &right->ppu) ||
        !gbl_test_ppu_affine_array_equal(left->ppu_affine_prepared,
                                         right->ppu_affine_prepared) ||
        left->ppu_dispcnt_prepared != right->ppu_dispcnt_prepared ||
        left->ppu_unsupported_register_bytes_prepared !=
            right->ppu_unsupported_register_bytes_prepared ||
        !gbl_test_bytes_equal(
            left->ppu_render_register_bytes_prepared,
            right->ppu_render_register_bytes_prepared,
            sizeof(left->ppu_render_register_bytes_prepared)) ||
        !gbl_test_bytes_equal(
            left->ppu_bg_register_bytes_prepared,
            right->ppu_bg_register_bytes_prepared,
            sizeof(left->ppu_bg_register_bytes_prepared)) ||
        !gbl_test_bytes_equal(
            left->ppu_window_register_bytes_prepared,
            right->ppu_window_register_bytes_prepared,
            sizeof(left->ppu_window_register_bytes_prepared)) ||
        left->bus_owner != right->bus_owner ||
        left->dma_last_channel != right->dma_last_channel ||
        left->dma_last_width != right->dma_last_width ||
        left->dma_last_source != right->dma_last_source ||
        left->dma_last_destination != right->dma_last_destination ||
        left->dma_stream_valid != right->dma_stream_valid ||
        left->pressed_keys != right->pressed_keys ||
        left->key_control != right->key_control ||
        left->key_condition != right->key_condition ||
        left->key_irq_condition != right->key_irq_condition ||
        left->event_count != right->event_count ||
        left->event_count > GBL_EVENT_CAPACITY ||
        left->input_generation != right->input_generation ||
        left->input_event_count != right->input_event_count ||
        left->input_event_cursor != right->input_event_cursor ||
        left->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        left->input_event_cursor > left->input_event_count ||
        left->ppu_row_capture_count > GBL_PPU_ROW_CAPTURE_MAX ||
        right->ppu_row_capture_count > GBL_PPU_ROW_CAPTURE_MAX ||
        left->ppu_memory_write_count > GBL_TIME_OPERATION_MAX ||
        right->ppu_memory_write_count > GBL_TIME_OPERATION_MAX ||
        left->ppu_clear_video != right->ppu_clear_video ||
        left->ppu_row_capture_count != right->ppu_row_capture_count ||
        left->ppu_memory_write_count != right->ppu_memory_write_count) {
        return false;
    }
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        if (!gbl_test_event_equal(&left->events[index],
                                  &right->events[index])) {
            return false;
        }
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        if (!gbl_test_timer_equal(&left->timers[index],
                                  &right->timers[index])) {
            return false;
        }
    }
    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        if (!gbl_test_dma_channel_equal(&left->dma_channels[index],
                                        &right->dma_channels[index])) {
            return false;
        }
    }
    for (index = 0u; index < GBL_INPUT_EVENT_CAPACITY; ++index) {
        if (!gbl_test_input_event_equal(&left->input_events[index],
                                        &right->input_events[index])) {
            return false;
        }
    }
    for (index = 0u; index < left->ppu_row_capture_count; ++index) {
        if (!gbl_test_ppu_row_capture_equal(&left->ppu_row_captures[index],
                                            &right->ppu_row_captures[index])) {
            return false;
        }
    }
    for (index = 0u; index < left->ppu_memory_write_count; ++index) {
        if (!gbl_test_ppu_memory_write_equal(
                &left->ppu_memory_writes[index],
                &right->ppu_memory_writes[index])) {
            return false;
        }
    }
    return true;
}

static inline bool gbl_test_time_preview_matches_machine(
    const gbl_machine_time_preview *preview, const gbl_machine *machine)
{
    gbl_machine_time_preview machine_time = { 0 };
    size_t index;

    if (preview == NULL || machine == NULL ||
        machine->event_count > GBL_EVENT_CAPACITY ||
        machine->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        machine->input_event_cursor > machine->input_event_count) {
        return false;
    }
    machine_time.master_cycles = machine->master_cycles;
    machine_time.next_event_sequence = machine->next_event_sequence;
    machine_time.interrupt_flags = machine->interrupt_flags;
    machine_time.interrupt_enable = machine->interrupt_enable;
    machine_time.interrupt_master_enable = machine->interrupt_master_enable;
    machine_time.waitcnt = machine->waitcnt;
    machine_time.ppu = machine->ppu;
    for (index = 0u; index < GBL_PPU_AFFINE_BACKGROUND_COUNT; ++index) {
        machine_time.ppu_affine_prepared[index] = machine->ppu.affine[index];
    }
    machine_time.ppu_dispcnt_prepared = machine->dispcnt;
    machine_time.ppu_unsupported_register_bytes_prepared =
        machine->ppu.unsupported_register_bytes;
    for (index = 0u; index < GBL_PPU_RENDER_REGISTER_BYTES; ++index) {
        machine_time.ppu_render_register_bytes_prepared[index] =
            machine->ppu.render_register_bytes[index];
    }
    for (index = 0u; index < GBL_PPU_BG_REGISTER_BYTES; ++index) {
        machine_time.ppu_bg_register_bytes_prepared[index] =
            machine->ppu.bg_register_bytes[index];
    }
    for (index = 0u; index < GBL_PPU_WINDOW_REGISTER_BYTES; ++index) {
        machine_time.ppu_window_register_bytes_prepared[index] =
            machine->ppu.window_register_bytes[index];
    }
    machine_time.bus_owner = machine->bus_owner;
    machine_time.dma_last_channel = machine->dma_last_channel;
    machine_time.dma_last_width = machine->dma_last_width;
    machine_time.dma_last_source = machine->dma_last_source;
    machine_time.dma_last_destination = machine->dma_last_destination;
    machine_time.dma_stream_valid = machine->dma_stream_valid;
    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        machine_time.dma_channels[index] = machine->dma_channels[index];
    }
    machine_time.pressed_keys = machine->pressed_keys;
    machine_time.key_control = machine->key_control;
    machine_time.key_condition = machine->key_condition;
    machine_time.key_irq_condition = machine->key_irq_condition;
    machine_time.event_count = machine->event_count;
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        machine_time.events[index] = machine->events[index];
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        machine_time.timers[index] = machine->timers[index];
    }
    machine_time.input_generation = machine->input_generation;
    machine_time.input_event_count = machine->input_event_count;
    machine_time.input_event_cursor = machine->input_event_cursor;
    for (index = 0u; index < GBL_INPUT_EVENT_CAPACITY; ++index) {
        machine_time.input_events[index] = machine->input_events[index];
    }
    return gbl_test_time_preview_equal(preview, &machine_time);
}

static bool gbl_test_cpu_equal(const gbl_cpu_state *left,
                               const gbl_cpu_state *right)
{
    size_t index;

    if (!gbl_test_u32_array_equal(left->r0_7, right->r0_7, 8u) ||
        !gbl_test_u32_array_equal(left->usr_r8_14, right->usr_r8_14, 7u) ||
        !gbl_test_u32_array_equal(left->fiq_r8_14, right->fiq_r8_14, 7u) ||
        !gbl_test_u32_array_equal(left->irq_r13_14, right->irq_r13_14, 2u) ||
        !gbl_test_u32_array_equal(left->svc_r13_14, right->svc_r13_14, 2u) ||
        !gbl_test_u32_array_equal(left->abt_r13_14, right->abt_r13_14, 2u) ||
        !gbl_test_u32_array_equal(left->und_r13_14, right->und_r13_14, 2u) ||
        !gbl_test_u32_array_equal(left->spsr, right->spsr, 5u) ||
        left->cpsr != right->cpsr ||
        left->execution_address != right->execution_address ||
        left->next_fetch_sequential != right->next_fetch_sequential ||
        left->force_nonsequential_fetch !=
            right->force_nonsequential_fetch ||
        left->retired_instructions != right->retired_instructions ||
        left->stopped != right->stopped ||
        left->stop_reason != right->stop_reason ||
        left->last_exception.kind != right->last_exception.kind ||
        left->last_exception.current_address !=
            right->last_exception.current_address ||
        left->last_exception.next_address !=
            right->last_exception.next_address ||
        left->last_exception.fault_address !=
            right->last_exception.fault_address ||
        left->last_exception.thumb_state !=
            right->last_exception.thumb_state ||
        left->irq_observation.sample_cycle !=
            right->irq_observation.sample_cycle ||
        left->irq_observation.entry_cycle !=
            right->irq_observation.entry_cycle ||
        left->irq_observation.entry_internal_cycles !=
            right->irq_observation.entry_internal_cycles ||
        left->irq_observation.cpsr != right->irq_observation.cpsr ||
        left->irq_observation.next_instruction_address !=
            right->irq_observation.next_instruction_address ||
        left->irq_observation.following_instruction_address !=
            right->irq_observation.following_instruction_address ||
        left->irq_observation.interrupt_flags !=
            right->irq_observation.interrupt_flags ||
        left->irq_observation.interrupt_enable !=
            right->irq_observation.interrupt_enable ||
        left->irq_observation.eligible_sources !=
            right->irq_observation.eligible_sources ||
        left->irq_observation.interrupt_master_enable !=
            right->irq_observation.interrupt_master_enable ||
        left->irq_observation.sampled !=
            right->irq_observation.sampled ||
        left->irq_observation.eligible !=
            right->irq_observation.eligible ||
        left->irq_observation.master_enabled !=
            right->irq_observation.master_enabled ||
        left->irq_observation.cpu_masked !=
            right->irq_observation.cpu_masked ||
        left->irq_observation.accepted !=
            right->irq_observation.accepted ||
        left->irq_observation.entered !=
            right->irq_observation.entered) {
        return false;
    }
    for (index = 0u; index < 2u; ++index) {
        if (left->pipeline[index].address != right->pipeline[index].address ||
            left->pipeline[index].opcode != right->pipeline[index].opcode ||
            left->pipeline[index].width != right->pipeline[index].width ||
            left->pipeline[index].valid != right->pipeline[index].valid) {
            return false;
        }
    }
    return true;
}

static inline bool gbl_test_cpu_is_direct_start(const gbl_cpu_state *cpu)
{
    size_t index;

    if (cpu->cpsr != UINT32_C(0x000000DF) ||
        cpu->execution_address != UINT32_C(0x08000000) ||
        cpu->pipeline[0].address != 0u || cpu->pipeline[0].opcode != 0u ||
        cpu->pipeline[0].width != 0u || cpu->pipeline[0].valid ||
        cpu->pipeline[1].address != 0u || cpu->pipeline[1].opcode != 0u ||
        cpu->pipeline[1].width != 0u || cpu->pipeline[1].valid ||
        cpu->next_fetch_sequential || cpu->force_nonsequential_fetch ||
        cpu->retired_instructions != 0u ||
        cpu->stopped || cpu->stop_reason != GBL_CPU_STOP_NONE ||
        cpu->last_exception.kind != GBL_EXCEPTION_NONE ||
        cpu->last_exception.current_address != 0u ||
        cpu->last_exception.next_address != 0u ||
        cpu->last_exception.fault_address != 0u ||
        cpu->last_exception.thumb_state ||
        cpu->irq_observation.sample_cycle != 0u ||
        cpu->irq_observation.entry_cycle != 0u ||
        cpu->irq_observation.entry_internal_cycles != 0u ||
        cpu->irq_observation.cpsr != 0u ||
        cpu->irq_observation.next_instruction_address != 0u ||
        cpu->irq_observation.following_instruction_address != 0u ||
        cpu->irq_observation.interrupt_flags != 0u ||
        cpu->irq_observation.interrupt_enable != 0u ||
        cpu->irq_observation.eligible_sources != 0u ||
        cpu->irq_observation.interrupt_master_enable != 0u ||
        cpu->irq_observation.sampled || cpu->irq_observation.eligible ||
        cpu->irq_observation.master_enabled ||
        cpu->irq_observation.cpu_masked || cpu->irq_observation.accepted ||
        cpu->irq_observation.entered) {
        return false;
    }
    for (index = 0u; index < 8u; ++index) {
        if (cpu->r0_7[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < 7u; ++index) {
        if (cpu->usr_r8_14[index] != 0u || cpu->fiq_r8_14[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < 2u; ++index) {
        if (cpu->irq_r13_14[index] != 0u || cpu->svc_r13_14[index] != 0u ||
            cpu->abt_r13_14[index] != 0u || cpu->und_r13_14[index] != 0u) {
            return false;
        }
    }
    for (index = 0u; index < 5u; ++index) {
        if (cpu->spsr[index] != 0u) {
            return false;
        }
    }
    return true;
}

/* Compare canonical fields and contents, excluding struct padding, allocator
 * identity, and ROM pointer identity. */
static bool gbl_test_canonical_equal(const gbl_machine *left,
                                     const gbl_machine *right)
{
    if (left == NULL || right == NULL ||
        left->initialized != right->initialized ||
        left->open_bus != right->open_bus ||
        left->dispcnt != right->dispcnt ||
        !gbl_test_time_state_equal(left, right) ||
        !gbl_test_ppu_video_equal(left, right) ||
        !gbl_test_cpu_equal(&left->cpu, &right->cpu) ||
        !gbl_test_diagnostics_equal(&left->header_diagnostics,
                                    &right->header_diagnostics) ||
        left->rom_length != right->rom_length ||
        ((left->rom == NULL) != (right->rom == NULL))) {
        return false;
    }
    if (!gbl_test_bytes_equal(left->ewram, right->ewram, GBL_EWRAM_SIZE) ||
        !gbl_test_bytes_equal(left->iwram, right->iwram, GBL_IWRAM_SIZE) ||
        !gbl_test_bytes_equal(left->pram, right->pram, GBL_PRAM_SIZE) ||
        !gbl_test_bytes_equal(left->vram, right->vram, GBL_VRAM_SIZE) ||
        !gbl_test_bytes_equal(left->oam, right->oam, GBL_OAM_SIZE)) {
        return false;
    }
    if (left->rom_length != 0u &&
        (left->rom == NULL || right->rom == NULL ||
         !gbl_test_bytes_equal(left->rom, right->rom, left->rom_length))) {
        return false;
    }
    return true;
}

/* Compare ownership bindings only between a machine and its own snapshot.
 * Canonical equality and its digest intentionally remain portable across
 * independent machine instances. */
static bool gbl_test_same_instance_identity_equal(
    const gbl_machine *before, const gbl_machine *after)
{
    return before != NULL && after != NULL &&
           before->rom == after->rom &&
           before->rom_length == after->rom_length &&
           before->allocator.context == after->allocator.context &&
           before->allocator.allocate == after->allocator.allocate &&
           before->allocator.release == after->allocator.release;
}

/* Trace records contain padding on some ABIs. Compare their public fields,
 * including caller prefix/guard records, rather than their object bytes. */
static inline bool gbl_test_trace_equal(const gbl_trace_record *left,
                                        const gbl_trace_record *right,
                                        size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        const gbl_trace_record *a = &left[index];
        const gbl_trace_record *b = &right[index];

        if (a->sequence != b->sequence || a->guest_cycle != b->guest_cycle ||
            a->category != b->category ||
            a->instruction_address != b->instruction_address ||
            a->opcode != b->opcode || a->address != b->address ||
            a->value != b->value || a->width != b->width ||
            a->access_class != b->access_class ||
            a->bus_outcome != b->bus_outcome ||
            a->exception_kind != b->exception_kind ||
            a->bus_owner != b->bus_owner || a->dma_status != b->dma_status ||
            a->dma_channel != b->dma_channel ||
            a->dma_startup_cycles != b->dma_startup_cycles ||
            a->dma_source_cycles != b->dma_source_cycles ||
            a->dma_destination_cycles != b->dma_destination_cycles ||
            a->dma_source_sequence != b->dma_source_sequence ||
            a->dma_destination_sequence != b->dma_destination_sequence ||
            a->dma_remaining != b->dma_remaining ||
            a->dma_completed != b->dma_completed ||
            !gbl_test_bytes_equal(
                (const uint8_t *)&a->dma_used_channel_latch,
                (const uint8_t *)&b->dma_used_channel_latch,
                sizeof a->dma_used_channel_latch) ||
            a->dma_value_provenance != b->dma_value_provenance ||
            a->dma_result_classification !=
                b->dma_result_classification ||
            a->dma_dispute != b->dma_dispute ||
            a->dma_hardware_qualification !=
                b->dma_hardware_qualification ||
            a->dma_source_address != b->dma_source_address ||
            a->dma_destination_address != b->dma_destination_address ||
            a->dma_value != b->dma_value ||
            a->dma_width != b->dma_width) {
            return false;
        }
    }
    return true;
}

static uint64_t gbl_test_digest_byte(uint64_t digest, uint8_t byte)
{
    return (digest ^ (uint64_t)byte) * UINT64_C(1099511628211);
}

static uint64_t gbl_test_digest_u64(uint64_t digest, uint64_t value)
{
    unsigned int byte_index;

    for (byte_index = 0u; byte_index < 8u; ++byte_index) {
        digest = gbl_test_digest_byte(digest,
                                     (uint8_t)(value & UINT64_C(0xFF)));
        value >>= 8u;
    }
    return digest;
}

static uint64_t gbl_test_digest_bytes(uint64_t digest, const uint8_t *bytes,
                                      size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        digest = gbl_test_digest_byte(digest, bytes[index]);
    }
    return digest;
}

static uint64_t gbl_test_digest_u32_array(uint64_t digest,
                                          const uint32_t *values,
                                          size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        digest = gbl_test_digest_u64(digest, values[index]);
    }
    return digest;
}

static uint64_t gbl_test_digest_cpu(uint64_t digest,
                                    const gbl_cpu_state *cpu)
{
    size_t index;

    digest = gbl_test_digest_u32_array(digest, cpu->r0_7, 8u);
    digest = gbl_test_digest_u32_array(digest, cpu->usr_r8_14, 7u);
    digest = gbl_test_digest_u32_array(digest, cpu->fiq_r8_14, 7u);
    digest = gbl_test_digest_u32_array(digest, cpu->irq_r13_14, 2u);
    digest = gbl_test_digest_u32_array(digest, cpu->svc_r13_14, 2u);
    digest = gbl_test_digest_u32_array(digest, cpu->abt_r13_14, 2u);
    digest = gbl_test_digest_u32_array(digest, cpu->und_r13_14, 2u);
    digest = gbl_test_digest_u32_array(digest, cpu->spsr, 5u);
    digest = gbl_test_digest_u64(digest, cpu->cpsr);
    digest = gbl_test_digest_u64(digest, cpu->execution_address);
    for (index = 0u; index < 2u; ++index) {
        digest = gbl_test_digest_u64(digest, cpu->pipeline[index].address);
        digest = gbl_test_digest_u64(digest, cpu->pipeline[index].opcode);
        digest = gbl_test_digest_byte(digest, cpu->pipeline[index].width);
        digest = gbl_test_digest_byte(
            digest, cpu->pipeline[index].valid ? 1u : 0u);
    }
    digest = gbl_test_digest_byte(
        digest, cpu->next_fetch_sequential ? 1u : 0u);
    digest = gbl_test_digest_byte(
        digest, cpu->force_nonsequential_fetch ? 1u : 0u);
    digest = gbl_test_digest_u64(digest, cpu->retired_instructions);
    digest = gbl_test_digest_byte(digest, cpu->stopped ? 1u : 0u);
    digest = gbl_test_digest_u64(digest, (uint64_t)cpu->stop_reason);
    digest = gbl_test_digest_u64(digest,
                                 (uint64_t)cpu->last_exception.kind);
    digest = gbl_test_digest_u64(
        digest, cpu->last_exception.current_address);
    digest = gbl_test_digest_u64(digest, cpu->last_exception.next_address);
    digest = gbl_test_digest_u64(digest, cpu->last_exception.fault_address);
    digest = gbl_test_digest_byte(
        digest, cpu->last_exception.thumb_state ? 1u : 0u);
    digest = gbl_test_digest_u64(
        digest, cpu->irq_observation.sample_cycle);
    digest = gbl_test_digest_u64(
        digest, cpu->irq_observation.entry_cycle);
    digest = gbl_test_digest_u64(
        digest, cpu->irq_observation.entry_internal_cycles);
    digest = gbl_test_digest_u64(digest, cpu->irq_observation.cpsr);
    digest = gbl_test_digest_u64(
        digest, cpu->irq_observation.next_instruction_address);
    digest = gbl_test_digest_u64(
        digest, cpu->irq_observation.following_instruction_address);
    digest = gbl_test_digest_u64(
        digest, cpu->irq_observation.interrupt_flags);
    digest = gbl_test_digest_u64(
        digest, cpu->irq_observation.interrupt_enable);
    digest = gbl_test_digest_u64(
        digest, cpu->irq_observation.eligible_sources);
    digest = gbl_test_digest_u64(
        digest, cpu->irq_observation.interrupt_master_enable);
    digest = gbl_test_digest_byte(
        digest, cpu->irq_observation.sampled ? 1u : 0u);
    digest = gbl_test_digest_byte(
        digest, cpu->irq_observation.eligible ? 1u : 0u);
    digest = gbl_test_digest_byte(
        digest, cpu->irq_observation.master_enabled ? 1u : 0u);
    digest = gbl_test_digest_byte(
        digest, cpu->irq_observation.cpu_masked ? 1u : 0u);
    digest = gbl_test_digest_byte(
        digest, cpu->irq_observation.accepted ? 1u : 0u);
    digest = gbl_test_digest_byte(
        digest, cpu->irq_observation.entered ? 1u : 0u);
    return digest;
}

static uint64_t gbl_test_time_state_digest_at(const gbl_machine *machine,
                                              uint64_t master_cycles)
{
    uint64_t digest = UINT64_C(14695981039346656037);
    size_t index;

    if (machine == NULL) return gbl_test_digest_byte(digest, 0xFFu);
    digest = gbl_test_digest_u64(digest, master_cycles);
    digest = gbl_test_digest_u64(digest, machine->next_event_sequence);
    digest = gbl_test_digest_byte(digest, (uint8_t)machine->power_state);
    digest = gbl_test_digest_u64(digest, machine->interrupt_flags);
    digest = gbl_test_digest_u64(digest, machine->interrupt_enable);
    digest = gbl_test_digest_u64(digest, machine->interrupt_master_enable);
    digest = gbl_test_digest_u64(digest, machine->waitcnt);
    digest = gbl_test_digest_byte(digest, (uint8_t)machine->bus_owner);
    digest = gbl_test_digest_byte(digest, machine->dma_last_channel);
    digest = gbl_test_digest_byte(digest, machine->dma_last_width);
    digest = gbl_test_digest_u64(digest, machine->dma_last_source);
    digest = gbl_test_digest_u64(digest, machine->dma_last_destination);
    digest = gbl_test_digest_byte(digest,
                                  machine->dma_stream_valid ? 1u : 0u);
    digest = gbl_test_digest_u64(digest, machine->ppu.line_origin);
    digest = gbl_test_digest_u64(digest, machine->ppu.generation);
    digest = gbl_test_digest_u64(digest, machine->ppu.frame_epoch);
    digest = gbl_test_digest_u64(digest, machine->ppu.line);
    digest = gbl_test_digest_u64(digest, machine->ppu.scheduled_line);
    digest = gbl_test_digest_u64(digest, machine->ppu.dispcnt);
    digest = gbl_test_digest_u64(digest, machine->ppu.dispstat);
    digest = gbl_test_digest_u64(digest, machine->ppu.compare);
    digest = gbl_test_digest_u64(digest, machine->ppu.captured_rows);
    digest = gbl_test_digest_u64(
        digest, machine->ppu.unsupported_register_bytes);
    digest = gbl_test_digest_bytes(
        digest, machine->ppu.render_register_bytes,
        sizeof(machine->ppu.render_register_bytes));
    digest = gbl_test_digest_bytes(
        digest, machine->ppu.bg_register_bytes,
        sizeof(machine->ppu.bg_register_bytes));
    digest = gbl_test_digest_bytes(
        digest, machine->ppu.window_register_bytes,
        sizeof(machine->ppu.window_register_bytes));
    for (index = 0u; index < GBL_PPU_AFFINE_BACKGROUND_COUNT; ++index) {
        const gbl_ppu_affine_state *affine = &machine->ppu.affine[index];

        digest = gbl_test_digest_u64(digest, (uint16_t)affine->pa);
        digest = gbl_test_digest_u64(digest, (uint16_t)affine->pb);
        digest = gbl_test_digest_u64(digest, (uint16_t)affine->pc);
        digest = gbl_test_digest_u64(digest, (uint16_t)affine->pd);
        digest = gbl_test_digest_u64(digest,
                                     affine->programmed_reference_x);
        digest = gbl_test_digest_u64(digest,
                                     affine->programmed_reference_y);
        digest = gbl_test_digest_u64(digest, affine->origin_x);
        digest = gbl_test_digest_u64(digest, affine->origin_y);
        digest = gbl_test_digest_byte(
            digest, affine->origin_valid ? 1u : 0u);
        digest = gbl_test_digest_byte(
            digest, affine->hblank_y_suppressed ? 1u : 0u);
    }
    digest = gbl_test_digest_u64(digest,
                                 (uint64_t)machine->ppu.capture_state);
    digest = gbl_test_digest_u64(digest,
                                 (uint64_t)machine->ppu.capture_reason);
    digest = gbl_test_digest_byte(
        digest, machine->ppu.next_boundary_hblank ? 1u : 0u);
    digest = gbl_test_digest_byte(
        digest, machine->ppu.frame_started ? 1u : 0u);
    digest = gbl_test_digest_bytes(digest, machine->ppu.row_coverage,
                                   sizeof(machine->ppu.row_coverage));
    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        const gbl_dma_channel_state *dma = &machine->dma_channels[index];

        digest = gbl_test_digest_u64(digest, dma->source_address);
        digest = gbl_test_digest_u64(digest, dma->destination_address);
        digest = gbl_test_digest_u64(digest, dma->current_source);
        digest = gbl_test_digest_u64(digest, dma->current_destination);
        digest = gbl_test_digest_u64(digest, dma->read_latch);
        digest = gbl_test_digest_u64(digest, dma->completed_units);
        digest = gbl_test_digest_u64(digest, dma->generation);
        digest = gbl_test_digest_u64(digest, dma->trigger_cycle);
        digest = gbl_test_digest_byte(digest,
                                      (uint8_t)dma->trigger_origin);
        digest = gbl_test_digest_byte(digest,
                                      (uint8_t)dma->trigger_dispute);
        digest = gbl_test_digest_u64(digest, dma->count);
        digest = gbl_test_digest_u64(digest, dma->control);
        digest = gbl_test_digest_u64(digest, dma->remaining);
        digest = gbl_test_digest_byte(digest, dma->pending ? 1u : 0u);
        digest = gbl_test_digest_byte(digest, dma->armed ? 1u : 0u);
        digest = gbl_test_digest_byte(digest, dma->active ? 1u : 0u);
        digest = gbl_test_digest_byte(digest,
                                      dma->startup_complete ? 1u : 0u);
    }
    digest = gbl_test_digest_u64(digest, machine->pressed_keys);
    digest = gbl_test_digest_u64(digest, machine->key_control);
    digest = gbl_test_digest_byte(digest, machine->key_condition ? 1u : 0u);
    digest = gbl_test_digest_byte(
        digest, machine->key_irq_condition ? 1u : 0u);
    digest = gbl_test_digest_u64(digest, (uint64_t)machine->event_count);
    digest = gbl_test_digest_u64(digest, machine->input_generation);
    digest = gbl_test_digest_u64(digest,
                                 (uint64_t)machine->input_event_count);
    digest = gbl_test_digest_u64(digest,
                                 (uint64_t)machine->input_event_cursor);
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        const gbl_event *event = &machine->events[index];

        digest = gbl_test_digest_u64(digest, event->cycle);
        digest = gbl_test_digest_u64(digest, event->sequence);
        digest = gbl_test_digest_u64(digest, event->generation);
        digest = gbl_test_digest_byte(digest, event->priority);
        digest = gbl_test_digest_byte(digest, event->kind);
        digest = gbl_test_digest_byte(digest, event->timer_index);
        digest = gbl_test_digest_byte(digest, event->dma_channel);
        digest = gbl_test_digest_byte(digest, event->live ? 1u : 0u);
        digest = gbl_test_digest_u64(digest, event->input_index);
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        const gbl_timer_state *timer = &machine->timers[index];

        digest = gbl_test_digest_u64(digest, timer->reload);
        digest = gbl_test_digest_u64(digest, timer->counter);
        digest = gbl_test_digest_u64(digest, timer->divisor);
        digest = gbl_test_digest_u64(digest, timer->phase);
        digest = gbl_test_digest_u64(digest, timer->anchor_cycle);
        digest = gbl_test_digest_u64(digest, timer->generation);
        digest = gbl_test_digest_byte(digest, timer->enabled ? 1u : 0u);
        digest = gbl_test_digest_byte(digest, timer->irq_enabled ? 1u : 0u);
        digest = gbl_test_digest_byte(digest, timer->cascade ? 1u : 0u);
    }
    for (index = 0u; index < GBL_INPUT_EVENT_CAPACITY; ++index) {
        const gbl_input_event *event = &machine->input_events[index];

        digest = gbl_test_digest_u64(digest, event->cycle);
        digest = gbl_test_digest_u64(digest, event->pressed_mask);
    }
    return digest;
}

static uint64_t gbl_test_time_state_digest(const gbl_machine *machine)
{
    return machine == NULL
               ? gbl_test_time_state_digest_at(NULL, 0u)
               : gbl_test_time_state_digest_at(machine,
                                                machine->master_cycles);
}

static inline uint64_t gbl_test_ppu_row_capture_digest(
    const gbl_ppu_row_capture *capture)
{
    uint64_t digest = UINT64_C(14695981039346656037);
    size_t affine_index;

    if (capture == NULL) {
        return gbl_test_digest_byte(digest, UINT8_MAX);
    }
    digest = gbl_test_digest_u64(digest, capture->cycle);
    digest = gbl_test_digest_u64(digest, capture->frame_epoch);
    digest = gbl_test_digest_u64(digest, capture->pixel_digest);
    digest = gbl_test_digest_u64(digest, capture->line);
    digest = gbl_test_digest_u64(digest, capture->dispcnt);
    digest = gbl_test_digest_u64(digest, capture->prior_write_count);
    digest = gbl_test_digest_u64(digest,
                                 capture->unsupported_register_bytes);
    digest = gbl_test_digest_bytes(digest, capture->render_register_bytes,
                                   sizeof(capture->render_register_bytes));
    digest = gbl_test_digest_bytes(digest, capture->bg_register_bytes,
                                   sizeof(capture->bg_register_bytes));
    digest = gbl_test_digest_bytes(digest, capture->window_register_bytes,
                                   sizeof(capture->window_register_bytes));
    for (affine_index = 0u;
         affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT; ++affine_index) {
        const gbl_ppu_affine_state *affine = &capture->affine[affine_index];

        digest = gbl_test_digest_u64(digest, (uint16_t)affine->pa);
        digest = gbl_test_digest_u64(digest, (uint16_t)affine->pb);
        digest = gbl_test_digest_u64(digest, (uint16_t)affine->pc);
        digest = gbl_test_digest_u64(digest, (uint16_t)affine->pd);
        digest = gbl_test_digest_u64(digest,
                                     affine->programmed_reference_x);
        digest = gbl_test_digest_u64(digest,
                                     affine->programmed_reference_y);
        digest = gbl_test_digest_u64(digest, (uint32_t)affine->origin_x);
        digest = gbl_test_digest_u64(digest, (uint32_t)affine->origin_y);
        digest = gbl_test_digest_byte(digest,
                                      affine->origin_valid ? 1u : 0u);
        digest = gbl_test_digest_byte(
            digest, affine->hblank_y_suppressed ? 1u : 0u);
    }
    digest = gbl_test_digest_bytes(digest, capture->obj_selected_mask,
                                   sizeof(capture->obj_selected_mask));
    digest = gbl_test_digest_u64(digest, capture->obj_selected_count);
    digest = gbl_test_digest_u64(digest, capture->obj_omitted_count);
    digest = gbl_test_digest_u64(digest, capture->obj_work_used);
    return digest;
}

static inline uint64_t gbl_test_time_preview_ppu_digest(
    const gbl_machine_time_preview *preview)
{
    uint64_t digest = UINT64_C(14695981039346656037);
    size_t index;

    if (preview == NULL ||
        preview->ppu_row_capture_count > GBL_PPU_ROW_CAPTURE_MAX ||
        preview->ppu_memory_write_count > GBL_TIME_OPERATION_MAX) {
        return gbl_test_digest_byte(digest, UINT8_MAX);
    }
    digest = gbl_test_digest_u64(digest, preview->ppu.dispcnt);
    digest = gbl_test_digest_u64(
        digest, preview->ppu.unsupported_register_bytes);
    digest = gbl_test_digest_bytes(
        digest, preview->ppu.render_register_bytes,
        sizeof(preview->ppu.render_register_bytes));
    digest = gbl_test_digest_bytes(
        digest, preview->ppu.bg_register_bytes,
        sizeof(preview->ppu.bg_register_bytes));
    digest = gbl_test_digest_bytes(
        digest, preview->ppu.window_register_bytes,
        sizeof(preview->ppu.window_register_bytes));
    digest = gbl_test_digest_u64(digest, preview->ppu_dispcnt_prepared);
    digest = gbl_test_digest_u64(
        digest, preview->ppu_unsupported_register_bytes_prepared);
    digest = gbl_test_digest_bytes(
        digest, preview->ppu_render_register_bytes_prepared,
        sizeof(preview->ppu_render_register_bytes_prepared));
    digest = gbl_test_digest_bytes(
        digest, preview->ppu_bg_register_bytes_prepared,
        sizeof(preview->ppu_bg_register_bytes_prepared));
    digest = gbl_test_digest_bytes(
        digest, preview->ppu_window_register_bytes_prepared,
        sizeof(preview->ppu_window_register_bytes_prepared));
    digest = gbl_test_digest_u64(digest, preview->ppu_row_capture_count);
    for (index = 0u; index < preview->ppu_row_capture_count; ++index) {
        digest = gbl_test_digest_u64(
            digest,
            gbl_test_ppu_row_capture_digest(&preview->ppu_row_captures[index]));
    }
    digest = gbl_test_digest_u64(digest, preview->ppu_memory_write_count);
    for (index = 0u; index < preview->ppu_memory_write_count; ++index) {
        const gbl_ppu_memory_write *write = &preview->ppu_memory_writes[index];

        digest = gbl_test_digest_u64(digest, write->cycle);
        digest = gbl_test_digest_u64(digest, write->address);
        digest = gbl_test_digest_u64(digest, write->value);
        digest = gbl_test_digest_u64(digest, write->dispcnt);
        digest = gbl_test_digest_byte(digest, write->width);
        digest = gbl_test_digest_byte(digest, write->region);
    }
    return digest;
}

/* Test-only streaming FNV-1a over canonical fields in fixed byte order. */
static uint64_t gbl_test_canonical_digest(const gbl_machine *machine)
{
    uint64_t digest = UINT64_C(14695981039346656037);

    if (machine == NULL) {
        return gbl_test_digest_byte(digest, 0xFFu);
    }
    digest = gbl_test_digest_byte(digest, machine->initialized ? 1u : 0u);
    digest = gbl_test_digest_u64(digest,
                                 gbl_test_time_state_digest(machine));
    digest = gbl_test_digest_u64(digest, machine->open_bus);
    digest = gbl_test_digest_u64(digest, machine->dispcnt);
    digest = gbl_test_digest_cpu(digest, &machine->cpu);
    digest = gbl_test_digest_byte(digest,
                                  machine->header_diagnostics.loaded ? 1u : 0u);
    digest = gbl_test_digest_u64(
        digest, (uint64_t)machine->header_diagnostics.canonical_logo);
    digest = gbl_test_digest_u64(
        digest, (uint64_t)machine->header_diagnostics.logo_reason);
    digest = gbl_test_digest_u64(
        digest, (uint64_t)machine->header_diagnostics.logo_window);
    digest = gbl_test_digest_u64(
        digest, (uint64_t)machine->header_diagnostics.checksum);
    digest = gbl_test_digest_u64(
        digest, (uint64_t)machine->header_diagnostics.game_code);
    digest = gbl_test_digest_bytes(digest, machine->ewram, GBL_EWRAM_SIZE);
    digest = gbl_test_digest_bytes(digest, machine->iwram, GBL_IWRAM_SIZE);
    digest = gbl_test_digest_bytes(digest, machine->pram, GBL_PRAM_SIZE);
    digest = gbl_test_digest_bytes(digest, machine->vram, GBL_VRAM_SIZE);
    digest = gbl_test_digest_bytes(digest, machine->video_frame,
                                   GBL_PPU_VIDEO_BYTES);
    digest = gbl_test_digest_bytes(digest, machine->oam, GBL_OAM_SIZE);
    digest = gbl_test_digest_byte(
        digest, (uint8_t)(machine->rom == NULL ? 0u : 1u));
    digest = gbl_test_digest_u64(digest, (uint64_t)machine->rom_length);
    if (machine->rom != NULL && machine->rom_length != 0u) {
        digest = gbl_test_digest_bytes(digest, machine->rom,
                                       machine->rom_length);
    }
    return digest;
}

#endif /* GBL_TESTS_STATE_CHECK_H */
