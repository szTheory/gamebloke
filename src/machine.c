#include "gamebloke/gamebloke.h"

#define GBL_KEYPAD_INPUT_MASK UINT16_C(0x03FF)
#define GBL_KEYPAD_CONTROL_MASK UINT16_C(0xC3FF)
#define GBL_KEYPAD_IRQ_BIT UINT16_C(0x1000)
#define GBL_KEYPAD_EVENT_PRIORITY 4u

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
    gbl_machine_time_preview *preview, const gbl_event *event)
{
    gbl_timer_state *timer;
    gbl_time_status status;

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
    preview->bus_owner = machine->bus_owner;
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

static void gbl_preview_dma_register_write(
    gbl_machine_time_preview *preview, uint32_t address, uint8_t value)
{
    uint32_t offset;
    uint8_t channel_index;
    uint32_t register_offset;
    gbl_dma_channel_state *channel;
    uint16_t previous_control;

    if (preview == NULL || address < UINT32_C(0x040000B0) ||
        address > UINT32_C(0x040000DF)) {
        return;
    }
    offset = address - UINT32_C(0x040000B0);
    channel_index = (uint8_t)(offset / UINT32_C(12));
    register_offset = offset % UINT32_C(12);
    if (channel_index >= GBL_DMA_CHANNEL_COUNT) {
        return;
    }
    channel = &preview->dma_channels[channel_index];
    if (register_offset < 4u) {
        const uint8_t shift = (uint8_t)((register_offset & 3u) * 8u);
        channel->source_address =
            (channel->source_address & ~(UINT32_C(0xFF) << shift)) |
            ((uint32_t)value << shift);
    } else if (register_offset < 8u) {
        const uint8_t shift = (uint8_t)((register_offset & 3u) * 8u);
        channel->destination_address =
            (channel->destination_address & ~(UINT32_C(0xFF) << shift)) |
            ((uint32_t)value << shift);
    } else if (register_offset < 10u) {
        const uint8_t shift = (uint8_t)((register_offset & 1u) * 8u);
        channel->count = (uint16_t)(
            (channel->count & (uint16_t)~(UINT16_C(0xFF) << shift)) |
            (uint16_t)((uint16_t)value << shift));
    } else {
        const uint8_t shift = (uint8_t)((register_offset & 1u) * 8u);

        previous_control = channel->control;
        channel->control = (uint16_t)(
            (channel->control & (uint16_t)~(UINT16_C(0xFF) << shift)) |
            (uint16_t)((uint16_t)value << shift));
        if ((channel->control & UINT16_C(0x8000)) == 0u) {
            channel->pending = false;
            channel->active = false;
            channel->startup_complete = false;
            channel->remaining = 0u;
        } else if ((previous_control & UINT16_C(0x8000)) == 0u) {
            channel->current_source = channel->source_address;
            channel->current_destination = channel->destination_address;
            channel->remaining = channel->count;
            channel->completed_units = 0u;
            channel->pending = true;
            channel->active = false;
            channel->startup_complete = false;
        }
    }
}

static gbl_time_status gbl_preview_advance_to(
    gbl_machine_time_preview *preview, uint64_t target,
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
        } else if (event.kind != (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        if (event.cycle < candidate.master_cycles) {
            const uint64_t generation =
                event.kind == (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW
                    ? candidate.timers[event.timer_index].generation
                    : candidate.input_generation;

            if (event.generation == generation) {
                return GBL_TIME_STATUS_INVALID_ARGUMENT;
            }
        }
        if (event.cycle > candidate.master_cycles) {
            candidate.master_cycles = event.cycle;
        }
        status = gbl_preview_dispatch_event(&candidate, &event);
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
    status = gbl_preview_advance_to(&candidate, target, &dispatched);
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
    uint16_t key_control = preview->key_control;
    bool key_control_written = false;
    bool waitcnt_written = false;
    uint32_t lane;

    for (lane = 0u; lane < (uint32_t)width; ++lane) {
        const uint32_t lane_address = address + lane;
        const uint8_t byte_value =
            (uint8_t)((value >> (lane * 8u)) & UINT32_C(0xFF));

        if (lane_address >= UINT32_C(0x04000100) &&
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
            gbl_preview_dma_register_write(preview, lane_address,
                                           byte_value);
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
        preview->waitcnt = (uint16_t)(waitcnt & UINT16_C(0x7FFF));
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
    const gbl_time_operation operations[GBL_TIME_OPERATION_MAX],
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
        status = gbl_preview_advance_to(&candidate, target, &dispatched);
        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
        if (operation->kind == GBL_TIME_OPERATION_BUS_WRITE) {
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

void gbl_machine_commit_time(gbl_machine *machine,
                            const gbl_machine_time_preview *preview)
{
    size_t index;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        preview->event_count > GBL_EVENT_CAPACITY ||
        preview->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        preview->input_event_cursor > preview->input_event_count) {
        return;
    }
    machine->master_cycles = preview->master_cycles;
    machine->next_event_sequence = preview->next_event_sequence;
    machine->interrupt_flags = preview->interrupt_flags;
    machine->interrupt_enable = preview->interrupt_enable;
    machine->interrupt_master_enable = preview->interrupt_master_enable;
    machine->waitcnt = preview->waitcnt;
    machine->bus_owner = preview->bus_owner;
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
    if (channel_index != 3u || channel->count != 1u ||
        channel->remaining != 1u ||
        (channel->control & UINT16_C(0x8000)) == 0u ||
        (channel->control & UINT16_C(0x7400)) != 0u) {
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
        *result = step;
        return step.status;
    }
    if (!channel->active || !channel->startup_complete ||
        machine->bus_owner != GBL_MACHINE_BUS_OWNER_DMA) {
        step.status = GBL_DMA_STATUS_UNIT_REFUSAL;
        *result = step;
        return step.status;
    }
    if (channel->remaining == 0u ||
        channel->completed_units == UINT64_MAX) {
        step.status = GBL_DMA_STATUS_UNIT_REFUSAL;
        *result = step;
        return step.status;
    }
    step.status = gbl_bus_dma_transfer_unit(machine, channel_index,
                                             cycle_budget, &step);
    if (step.status == GBL_DMA_STATUS_OK && step.unit_completed) {
        channel = &machine->dma_channels[channel_index];
        --channel->remaining;
        ++channel->completed_units;
        if (channel->remaining == 0u) {
            channel->active = false;
            channel->pending = false;
            channel->control =
                (uint16_t)(channel->control & (uint16_t)~UINT16_C(0x8000));
            channel->startup_complete = false;
            machine->bus_owner = GBL_MACHINE_BUS_OWNER_CPU;
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
    machine->pressed_keys = 0u;
    machine->key_control = 0u;
    machine->key_condition = false;
    machine->key_irq_condition = false;
    machine->event_count = 0u;
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        machine->events[index] = (gbl_event){ 0 };
    }
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
