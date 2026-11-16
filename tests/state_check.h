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
           left->live == right->live;
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

static inline bool gbl_test_time_state_equal(const gbl_machine *left,
                                             const gbl_machine *right)
{
    size_t index;

    if (left == NULL || right == NULL ||
        left->master_cycles != right->master_cycles ||
        left->next_event_sequence != right->next_event_sequence ||
        left->interrupt_flags != right->interrupt_flags ||
        left->interrupt_enable != right->interrupt_enable ||
        left->interrupt_master_enable != right->interrupt_master_enable ||
        left->event_count != right->event_count ||
        left->event_count > GBL_EVENT_CAPACITY) {
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
        left->event_count != right->event_count) {
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
    return true;
}

static inline bool gbl_test_time_preview_matches_machine(
    const gbl_machine_time_preview *preview, const gbl_machine *machine)
{
    gbl_machine_time_preview machine_time = { 0 };
    size_t index;

    if (preview == NULL || machine == NULL ||
        machine->event_count > GBL_EVENT_CAPACITY) {
        return false;
    }
    machine_time.master_cycles = machine->master_cycles;
    machine_time.next_event_sequence = machine->next_event_sequence;
    machine_time.interrupt_flags = machine->interrupt_flags;
    machine_time.interrupt_enable = machine->interrupt_enable;
    machine_time.interrupt_master_enable = machine->interrupt_master_enable;
    machine_time.event_count = machine->event_count;
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        machine_time.events[index] = machine->events[index];
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        machine_time.timers[index] = machine->timers[index];
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
        cpu->next_fetch_sequential || cpu->retired_instructions != 0u ||
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
            a->exception_kind != b->exception_kind) {
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
    digest = gbl_test_digest_u64(digest, machine->interrupt_flags);
    digest = gbl_test_digest_u64(digest, machine->interrupt_enable);
    digest = gbl_test_digest_u64(digest, machine->interrupt_master_enable);
    digest = gbl_test_digest_u64(digest, (uint64_t)machine->event_count);
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        const gbl_event *event = &machine->events[index];

        digest = gbl_test_digest_u64(digest, event->cycle);
        digest = gbl_test_digest_u64(digest, event->sequence);
        digest = gbl_test_digest_u64(digest, event->generation);
        digest = gbl_test_digest_byte(digest, event->priority);
        digest = gbl_test_digest_byte(digest, event->kind);
        digest = gbl_test_digest_byte(digest, event->timer_index);
        digest = gbl_test_digest_byte(digest, event->live ? 1u : 0u);
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
    return digest;
}

static uint64_t gbl_test_time_state_digest(const gbl_machine *machine)
{
    return machine == NULL
               ? gbl_test_time_state_digest_at(NULL, 0u)
               : gbl_test_time_state_digest_at(machine,
                                                machine->master_cycles);
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
