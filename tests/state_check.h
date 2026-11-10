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

/* Compare canonical fields and contents, excluding struct padding, allocator
 * identity, and ROM pointer identity. Extend this list if diagnostics become
 * canonical machine fields in a later loader plan. */
static bool gbl_test_canonical_equal(const gbl_machine *left,
                                     const gbl_machine *right)
{
    if (left == NULL || right == NULL ||
        left->initialized != right->initialized ||
        left->master_cycles != right->master_cycles ||
        left->open_bus != right->open_bus ||
        left->dispcnt != right->dispcnt ||
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
    digest = gbl_test_digest_bytes(digest, machine->ewram, GBL_EWRAM_SIZE);
    digest = gbl_test_digest_bytes(digest, machine->iwram, GBL_IWRAM_SIZE);
    digest = gbl_test_digest_bytes(digest, machine->pram, GBL_PRAM_SIZE);
    digest = gbl_test_digest_bytes(digest, machine->vram, GBL_VRAM_SIZE);
    digest = gbl_test_digest_bytes(digest, machine->oam, GBL_OAM_SIZE);
    digest = gbl_test_digest_byte(digest, machine->rom == NULL ? 0u : 1u);
    digest = gbl_test_digest_u64(digest, (uint64_t)machine->rom_length);
    if (machine->rom != NULL && machine->rom_length != 0u) {
        digest = gbl_test_digest_bytes(digest, machine->rom,
                                       machine->rom_length);
    }
    return digest;
}

#endif /* GBL_TESTS_STATE_CHECK_H */
