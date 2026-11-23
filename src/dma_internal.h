#ifndef GBL_DMA_INTERNAL_H
#define GBL_DMA_INTERNAL_H

#include "gamebloke/gamebloke.h"

/* Checked completion-time source projection for forced Direct Sound words.
 * Unlike ordinary DMA's documented latch fallback, an unreadable lane or a
 * word crossing a mapped-region boundary refuses the complete sound burst. */
bool gbl_bus_dma_read_word_checked(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    uint32_t address, gbl_bus_access_class sequence, uint32_t *value_out);

/* CPU-run transaction seam. Observations are published only if the complete
 * sound request candidate commits and the caller has room for every record. */
gbl_dma_status gbl_machine_dma_step_observed(
    gbl_machine *machine, uint64_t cycle_budget, gbl_dma_step_result *result,
    gbl_dma_step_result *observations, size_t observation_capacity,
    size_t *observation_count);

#endif
