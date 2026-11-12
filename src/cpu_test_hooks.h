#ifndef GBL_CPU_TEST_HOOKS_H
#define GBL_CPU_TEST_HOOKS_H

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

#endif /* GBL_CPU_TEST_HOOKS_H */
