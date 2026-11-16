#include "gamebloke/gamebloke.h"

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

static gbl_time_status gbl_preview_dispatch_event(
    gbl_machine_time_preview *preview, const gbl_event *event)
{
    gbl_timer_state *timer;
    gbl_time_status status;

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

gbl_time_status gbl_machine_prepare_time(
    const gbl_machine *machine, uint64_t target,
    gbl_machine_time_preview *preview)
{
    gbl_machine_time_preview candidate = { 0 };
    size_t dispatched = 0u;
    size_t index;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        machine->event_count > GBL_EVENT_CAPACITY) {
        return GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (target < machine->master_cycles) {
        return GBL_TIME_STATUS_BACKWARD;
    }
    candidate.master_cycles = machine->master_cycles;
    candidate.next_event_sequence = machine->next_event_sequence;
    candidate.interrupt_flags = machine->interrupt_flags;
    candidate.event_count = machine->event_count;
    for (index = 0u; index < machine->event_count; ++index) {
        candidate.events[index] = machine->events[index];
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        candidate.timers[index] = machine->timers[index];
    }

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
        if (dispatched >= GBL_MACHINE_MAX_EVENT_DISPATCH) {
            return GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT;
        }
        event = candidate.events[next_index];
        gbl_preview_remove_event(&candidate, next_index);
        if (event.timer_index >= GBL_TIMER_COUNT ||
            event.kind != (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        if (event.cycle < candidate.master_cycles &&
            event.generation == candidate.timers[event.timer_index].generation) {
            return GBL_TIME_STATUS_INVALID_ARGUMENT;
        }
        if (event.cycle > candidate.master_cycles) {
            candidate.master_cycles = event.cycle;
        }
        status = gbl_preview_dispatch_event(&candidate, &event);
        if (status != GBL_TIME_STATUS_OK) {
            return status;
        }
        ++dispatched;
    }
    candidate.master_cycles = target;
    *preview = candidate;
    return GBL_TIME_STATUS_OK;
}

void gbl_machine_commit_time(gbl_machine *machine,
                            const gbl_machine_time_preview *preview)
{
    size_t index;

    if (machine == NULL || !machine->initialized || preview == NULL ||
        preview->event_count > GBL_EVENT_CAPACITY) {
        return;
    }
    machine->master_cycles = preview->master_cycles;
    machine->next_event_sequence = preview->next_event_sequence;
    machine->interrupt_flags = preview->interrupt_flags;
    machine->event_count = preview->event_count;
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        machine->events[index] = preview->events[index];
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        machine->timers[index] = preview->timers[index];
    }
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
    machine->open_bus = 0u;
    machine->dispcnt = 0u;
    machine->interrupt_flags = 0u;
    machine->event_count = 0u;
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        machine->events[index] = (gbl_event){ 0 };
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        machine->timers[index] = (gbl_timer_state){ 0 };
        machine->timers[index].divisor = 1u;
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
