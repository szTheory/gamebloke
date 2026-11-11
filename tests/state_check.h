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
        left->stop_reason != right->stop_reason) {
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

/* Compare canonical fields and contents, excluding struct padding, allocator
 * identity, and ROM pointer identity. */
static bool gbl_test_canonical_equal(const gbl_machine *left,
                                     const gbl_machine *right)
{
    if (left == NULL || right == NULL ||
        left->initialized != right->initialized ||
        left->master_cycles != right->master_cycles ||
        left->open_bus != right->open_bus ||
        left->dispcnt != right->dispcnt ||
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
    digest = gbl_test_digest_u64(digest, machine->master_cycles);
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
