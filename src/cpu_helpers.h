#ifndef GAMEBLOKE_CPU_HELPERS_H
#define GAMEBLOKE_CPU_HELPERS_H

#include "gamebloke/gamebloke.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Test-only count perturbations for the private maximum-unit reservation
 * probe. These do not describe guest instructions or extend the public API. */
typedef enum gbl_cpu_test_count_overflow {
    GBL_CPU_TEST_COUNT_VALID,
    GBL_CPU_TEST_FETCH_COUNT_OVERFLOW,
    GBL_CPU_TEST_DATA_COUNT_OVERFLOW,
    GBL_CPU_TEST_RESULT_COUNT_OVERFLOW,
    GBL_CPU_TEST_REGISTER_EFFECT_COUNT_OVERFLOW,
    GBL_CPU_TEST_STATUS_EFFECT_COUNT_OVERFLOW
} gbl_cpu_test_count_overflow;

/* Build a synthetic unit at the private descriptor's declared maxima and run
 * the same cost, machine-preflight, selected-trace-count, and trace-reservation
 * checks used by gbl_cpu_run. No bus operation or effect is executed. */
bool gbl_cpu_test_maximum_preflight(
    const gbl_machine *machine, const gbl_run_options *options,
    gbl_cpu_test_count_overflow count_overflow, uint64_t *cycle_cost,
    size_t *required_records, gbl_run_reason *reason);

typedef enum gbl_shift_kind {
    GBL_SHIFT_LSL = 0,
    GBL_SHIFT_LSR = 1,
    GBL_SHIFT_ASR = 2,
    GBL_SHIFT_ROR = 3
} gbl_shift_kind;

typedef struct gbl_shift_result {
    uint32_t value;
    bool carry;
} gbl_shift_result;

typedef struct gbl_arithmetic_result {
    uint32_t value;
    bool carry;
    bool overflow;
} gbl_arithmetic_result;

/* register_shift distinguishes a register-specified zero count from an
 * immediate encoding whose zero count has an ARM-specific meaning. */
static inline gbl_shift_result gbl_shift(gbl_shift_kind kind, uint32_t value,
                                         uint8_t amount, bool carry_in,
                                         bool register_shift)
{
    gbl_shift_result result = { value, carry_in };
    uint32_t effective_amount = (uint32_t)amount;

    if (register_shift && effective_amount == 0u) {
        return result;
    }
    if (!register_shift && effective_amount == 0u) {
        if (kind == GBL_SHIFT_LSL) {
            return result;
        }
        if (kind == GBL_SHIFT_LSR || kind == GBL_SHIFT_ASR) {
            effective_amount = 32u;
        } else {
            result.value = ((uint32_t)carry_in << 31u) | (value >> 1u);
            result.carry = (value & UINT32_C(1)) != 0u;
            return result;
        }
    }

    switch (kind) {
    case GBL_SHIFT_LSL:
        if (effective_amount < 32u) {
            result.value = value << effective_amount;
            result.carry =
                ((value >> (32u - effective_amount)) & UINT32_C(1)) != 0u;
        } else if (effective_amount == 32u) {
            result.value = 0u;
            result.carry = (value & UINT32_C(1)) != 0u;
        } else {
            result.value = 0u;
            result.carry = false;
        }
        break;
    case GBL_SHIFT_LSR:
        if (effective_amount < 32u) {
            result.value = value >> effective_amount;
            result.carry =
                ((value >> (effective_amount - 1u)) & UINT32_C(1)) != 0u;
        } else if (effective_amount == 32u) {
            result.value = 0u;
            result.carry = (value & UINT32_C(0x80000000)) != 0u;
        } else {
            result.value = 0u;
            result.carry = false;
        }
        break;
    case GBL_SHIFT_ASR:
        if (effective_amount < 32u) {
            const bool sign = (value & UINT32_C(0x80000000)) != 0u;
            const uint32_t fill = sign
                                      ? UINT32_MAX << (32u - effective_amount)
                                      : 0u;

            result.value = (value >> effective_amount) | fill;
            result.carry =
                ((value >> (effective_amount - 1u)) & UINT32_C(1)) != 0u;
        } else {
            result.carry = (value & UINT32_C(0x80000000)) != 0u;
            result.value = result.carry ? UINT32_MAX : 0u;
        }
        break;
    case GBL_SHIFT_ROR: {
        const uint32_t rotation = effective_amount & UINT32_C(31);

        if (rotation == 0u) {
            result.value = value;
            result.carry = (value & UINT32_C(0x80000000)) != 0u;
        } else {
            result.value = (value >> rotation) |
                           (value << (32u - rotation));
            result.carry =
                (result.value & UINT32_C(0x80000000)) != 0u;
        }
        break;
    }
    }
    return result;
}

static inline gbl_arithmetic_result gbl_add_flags(uint32_t left,
                                                   uint32_t right)
{
    gbl_arithmetic_result result;

    result.value = left + right;
    result.carry = result.value < left;
    result.overflow = ((~(left ^ right) & (left ^ result.value) &
                        UINT32_C(0x80000000)) != 0u);
    return result;
}

static inline gbl_arithmetic_result gbl_sub_flags(uint32_t left,
                                                   uint32_t right)
{
    gbl_arithmetic_result result;

    result.value = left - right;
    result.carry = left >= right;
    result.overflow = (((left ^ right) & (left ^ result.value) &
                        UINT32_C(0x80000000)) != 0u);
    return result;
}

#endif
