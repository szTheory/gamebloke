#include "gamebloke/gamebloke.h"

#include "audio_internal.h"
#include "cpu_helpers.h"
#include "dma_internal.h"

#include <limits.h>

#define GBL_CPU_WORD_WIDTH 4u
#define GBL_CPU_HALF_WIDTH 2u
#define GBL_CPU_CPSR_N UINT32_C(0x80000000)
#define GBL_CPU_CPSR_Z UINT32_C(0x40000000)
#define GBL_CPU_CPSR_C UINT32_C(0x20000000)
#define GBL_CPU_CPSR_V UINT32_C(0x10000000)
#define GBL_CPU_CPSR_T UINT32_C(0x00000020)
#define GBL_CPU_CPSR_F UINT32_C(0x00000040)
#define GBL_CPU_CPSR_I UINT32_C(0x00000080)
#define GBL_CPU_CPSR_MODE_MASK UINT32_C(0x0000001F)
#define GBL_CPU_CPSR_USER_MASK UINT32_C(0xF0000000)
#define GBL_CPU_CPSR_PRIV_MASK UINT32_C(0x000000DF)
#define GBL_CPU_CPSR_STATE_MASK UINT32_C(0x00000020)
#define GBL_CPU_PSR_UNALLOC_MASK UINT32_C(0x0FFFFF00)
#define GBL_CPU_MODE_FIQ UINT32_C(0x00000011)
#define GBL_CPU_MODE_IRQ UINT32_C(0x00000012)
#define GBL_CPU_MODE_SVC UINT32_C(0x00000013)
#define GBL_CPU_MODE_ABT UINT32_C(0x00000017)
#define GBL_CPU_MODE_UND UINT32_C(0x0000001B)
#define GBL_CPU_RESULT_SLOT_MAX GBL_CPU_DATA_SLOT_MAX
#define GBL_CPU_REGISTER_EFFECT_MAX 16u
#define GBL_CPU_STATUS_EFFECT_MAX 1u
#define GBL_CPU_TRACE_UNIT_RECORD_MAX \
    (1u + GBL_CPU_DATA_SLOT_MAX + GBL_CPU_FETCH_SLOT_MAX)

static bool gbl_cpu_bytes_equal(const uint8_t *left, const uint8_t *right,
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

/* DDI0029G §6.5–6.6: register-specified Operand2 shifts add one internal
 * cycle; immediate Operand2 forms add none. The decode selects this table
 * before the same unit is costed, preflighted, and executed. */
static const uint8_t gbl_cpu_arm_operand2_internal_cycles[2] = { 0u, 1u };

typedef enum gbl_cpu_unit_kind {
    GBL_CPU_UNIT_REFILL,
    GBL_CPU_UNIT_DATA_PROCESSING,
    GBL_CPU_UNIT_TRANSFER,
    GBL_CPU_UNIT_SEQUENTIAL,
    GBL_CPU_UNIT_BRANCH,
    GBL_CPU_UNIT_UNSUPPORTED,
    GBL_CPU_UNIT_UNPREDICTABLE,
    GBL_CPU_UNIT_LATER_ARCHITECTURE,
    GBL_CPU_UNIT_UNDEFINED,
    GBL_CPU_UNIT_EXCEPTION
} gbl_cpu_unit_kind;

typedef enum gbl_cpu_data_operation {
    GBL_CPU_DATA_READ,
    GBL_CPU_DATA_WRITE
} gbl_cpu_data_operation;

typedef struct gbl_cpu_data_slot {
    gbl_cpu_data_operation operation;
    gbl_bus_access_class access_class;
    uint32_t address;
    uint32_t write_value;
    uint8_t width;
    uint8_t result_index;
    uint8_t alignment_lane;
    bool rotate_word_read;
    bool rotate_halfword_read;
    bool sign_extend_byte_read;
    bool sign_extend_halfword_read;
} gbl_cpu_data_slot;

typedef struct gbl_cpu_register_effect {
    uint32_t bank_cpsr;
    uint32_t value;
    uint8_t register_index;
    uint8_t result_index;
    bool from_result;
} gbl_cpu_register_effect;

typedef struct gbl_cpu_status_effect {
    uint32_t mask;
    uint32_t value;
    uint8_t spsr_index;
} gbl_cpu_status_effect;

typedef enum gbl_cpu_pc_read_context {
    GBL_CPU_PC_READ_NORMAL = 0,
    GBL_CPU_PC_READ_REGISTER_SHIFT = 1,
    GBL_CPU_PC_READ_STORE_SOURCE = 2,
    GBL_CPU_PC_READ_THUMB_ALIGNED_ADDRESS = 3
} gbl_cpu_pc_read_context;

/* One ephemeral decode is used both to preflight this unit and to execute it. */
typedef struct gbl_cpu_unit {
    gbl_cpu_unit_kind kind;
    uint32_t instruction_address;
    uint32_t opcode;
    uint32_t branch_target;
    uint32_t value;
    uint8_t fetch_count;
    uint8_t fetch_width;
    uint8_t internal_cycles;
    uint8_t post_data_internal_cycles;
    uint8_t post_data_pipeline_cycles;
    uint8_t data_slot_count;
    uint8_t result_count;
    uint8_t register_effect_count;
    uint8_t status_effect_count;
    bool changes_state;
    bool thumb_state;
    bool asynchronous;
    bool fetch_address_from_result;
    bool fetch_target_projected;
    bool timing_prepared;
    bool multiple_transfer;
    bool restore_cpsr_from_spsr;
    uint8_t fetch_result_index;
    uint32_t fetch_result_mask;
    gbl_exception_kind exception_kind;
    uint32_t fetch_address[GBL_CPU_FETCH_SLOT_MAX];
    uint32_t fetch_result_addend[GBL_CPU_FETCH_SLOT_MAX];
    gbl_bus_access_class fetch_class[GBL_CPU_FETCH_SLOT_MAX];
    gbl_cpu_data_slot data_slots[GBL_CPU_DATA_SLOT_MAX];
    uint32_t results[GBL_CPU_RESULT_SLOT_MAX];
    gbl_cpu_register_effect register_effects[GBL_CPU_REGISTER_EFFECT_MAX];
    gbl_cpu_status_effect status_effects[GBL_CPU_STATUS_EFFECT_MAX];
    uint64_t cycle_cost;
    uint64_t bus_cycle_cost;
    uint64_t internal_cycle_cost;
    uint64_t post_data_pipeline_cost;
} gbl_cpu_unit;

_Static_assert(GBL_CPU_TRACE_UNIT_RECORD_MAX == 19u,
               "trace capacity covers one instruction, sixteen transfers, and two refills");
_Static_assert(GBL_CPU_DATA_SLOT_MAX == 16u &&
                   GBL_CPU_RESULT_SLOT_MAX == GBL_CPU_DATA_SLOT_MAX &&
                   GBL_CPU_REGISTER_EFFECT_MAX == GBL_CPU_DATA_SLOT_MAX,
               "maximum block work keeps data, result, and register effects bounded together");
_Static_assert(GBL_CPU_FETCH_SLOT_MAX == 2u &&
                   GBL_CPU_STATUS_EFFECT_MAX == 1u,
               "maximum block work retains two refill slots and one status effect");
_Static_assert(GBL_TIME_OPERATION_MAX ==
                   GBL_CPU_FETCH_SLOT_MAX + GBL_CPU_DATA_SLOT_MAX + 2u,
               "time operation capacity covers all fetch, data, and internal slots");

static bool gbl_cpu_add_bus_quote(uint64_t *total,
                                  const gbl_machine *machine,
                                  gbl_bus_direction direction,
                                  uint32_t address, uint8_t width,
                                  gbl_bus_access_class sequence,
                                  gbl_bus_access_role role,
                                  bool address_unresolved)
{
    gbl_cycle_cost quote;
    bool quoted;

    if (machine != NULL && !address_unresolved) {
        quoted = gbl_bus_quote(machine, address, direction, width, sequence,
                               role, &quote);
    } else {
        quoted = gbl_bus_cost_for(direction,
                                  address_unresolved
                                      ? GBL_BUS_REGION_UNMAPPED
                                      : gbl_bus_region_for_address(address),
                                  width,
                                  sequence, role, &quote);
    }
    if (total == NULL || !quoted ||
        quote.cycles > UINT64_MAX - *total) {
        return false;
    }
    *total += quote.cycles;
    return true;
}

static bool gbl_cpu_unit_cost(const gbl_machine *machine,
                              const gbl_cpu_unit *unit, uint64_t *cost,
                              uint64_t *bus_cycles,
                              uint64_t *internal_cycles,
                              uint64_t *pipeline_bus_cycles)
{
    uint64_t bus_total = 0u;
    uint64_t pipeline_start;
    uint64_t internal_total;
    uint8_t index;

    if (unit == NULL || cost == NULL || bus_cycles == NULL ||
        internal_cycles == NULL || pipeline_bus_cycles == NULL ||
        unit->fetch_count > GBL_CPU_FETCH_SLOT_MAX ||
        unit->data_slot_count > GBL_CPU_DATA_SLOT_MAX) {
        return false;
    }
    for (index = 0u; index < unit->fetch_count; ++index) {
        const bool unresolved = unit->fetch_address_from_result &&
                                !unit->fetch_target_projected;
        const uint32_t address = unresolved ? 0u
                                             : unit->fetch_address[index];

        /* Decode may create a symbolic loaded-PC refill. It has no final
         * quote until candidate reads resolve its target. Keep this interim
         * cost free of a fictitious one-cycle unmapped reservation; run-time
         * preflight requires the target to be projected first. */
        if (unresolved) {
            continue;
        }
        if (!gbl_cpu_add_bus_quote(&bus_total, machine,
                                   GBL_BUS_DIRECTION_READ, address,
                                   unit->fetch_width,
                                   unit->fetch_class[index],
                                   GBL_BUS_ROLE_FETCH, false)) {
            return false;
        }
    }
    for (index = 0u; index < unit->data_slot_count; ++index) {
        const gbl_cpu_data_slot *slot = &unit->data_slots[index];
        const gbl_bus_direction direction =
            slot->operation == GBL_CPU_DATA_WRITE
                ? GBL_BUS_DIRECTION_WRITE
                : GBL_BUS_DIRECTION_READ;

        if (!gbl_cpu_add_bus_quote(
                &bus_total, machine, direction, slot->address, slot->width,
                slot->access_class, GBL_BUS_ROLE_DATA, false)) {
            return false;
        }
    }
    /* This extra LDM-to-PC S cycle is modeled as fetch/bus work but has no
     * resolved guest address or second memory transaction. Keep it explicit
     * in the cost key instead of folding it into an ARM I-cycle interval. */
    pipeline_start = bus_total;
    for (index = 0u; index < unit->post_data_pipeline_cycles; ++index) {
        if (!gbl_cpu_add_bus_quote(&bus_total, machine,
                                   GBL_BUS_DIRECTION_READ, 0u,
                                   unit->fetch_width,
                                   GBL_BUS_CLASS_SEQUENTIAL,
                                   GBL_BUS_ROLE_FETCH, true)) {
            return false;
        }
    }
    internal_total = (uint64_t)unit->internal_cycles;
    if ((uint64_t)unit->post_data_internal_cycles >
        UINT64_MAX - internal_total) {
        return false;
    }
    internal_total += (uint64_t)unit->post_data_internal_cycles;
    if (internal_total > UINT64_MAX - bus_total) {
        return false;
    }
    *bus_cycles = bus_total;
    *internal_cycles = internal_total;
    *pipeline_bus_cycles = bus_total - pipeline_start;
    *cost = bus_total + internal_total;
    return true;
}

static bool gbl_cpu_refresh_cycle_cost(gbl_cpu_unit *unit)
{
    uint64_t cost;
    uint64_t bus_cycles;
    uint64_t internal_cycles;
    uint64_t pipeline_bus_cycles;

    if (!gbl_cpu_unit_cost(NULL, unit, &cost, &bus_cycles, &internal_cycles,
                           &pipeline_bus_cycles)) {
        return false;
    }
    unit->cycle_cost = cost;
    unit->bus_cycle_cost = bus_cycles;
    unit->internal_cycle_cost = internal_cycles;
    unit->post_data_pipeline_cost = pipeline_bus_cycles;
    return true;
}

static bool gbl_cpu_refresh_cycle_cost_for_machine(
    const gbl_machine *machine, gbl_cpu_unit *unit)
{
    uint64_t cost;
    uint64_t bus_cycles;
    uint64_t internal_cycles;
    uint64_t pipeline_bus_cycles;

    if (machine == NULL || unit == NULL ||
        !gbl_cpu_unit_cost(machine, unit, &cost, &bus_cycles,
                           &internal_cycles, &pipeline_bus_cycles)) {
        return false;
    }
    unit->cycle_cost = cost;
    unit->bus_cycle_cost = bus_cycles;
    unit->internal_cycle_cost = internal_cycles;
    unit->post_data_pipeline_cost = pipeline_bus_cycles;
    return true;
}

static bool gbl_cpu_add_data_read_slot(gbl_cpu_unit *unit, uint32_t address,
                                       uint8_t width, uint8_t alignment_lane,
                                       bool rotate_word_read,
                                       uint8_t *result_index)
{
    uint8_t result;
    gbl_cpu_data_slot *slot;

    if (unit == NULL || result_index == NULL ||
        unit->data_slot_count >= GBL_CPU_DATA_SLOT_MAX ||
        unit->result_count >= GBL_CPU_RESULT_SLOT_MAX) {
        return false;
    }
    result = unit->result_count;
    slot = &unit->data_slots[unit->data_slot_count];
    *slot = (gbl_cpu_data_slot){
        .operation = GBL_CPU_DATA_READ,
        .access_class = GBL_BUS_CLASS_NONSEQUENTIAL,
        .address = address,
        .width = width,
        .result_index = result,
        .alignment_lane = alignment_lane,
        .rotate_word_read = rotate_word_read,
        .rotate_halfword_read = false,
        .sign_extend_byte_read = false,
        .sign_extend_halfword_read = false
    };
    unit->data_slot_count++;
    unit->result_count++;
    *result_index = result;
    return true;
}

static bool gbl_cpu_add_subword_read_slot(
    gbl_cpu_unit *unit, uint32_t address, uint8_t width,
    uint8_t alignment_lane, bool rotate_halfword_read,
    bool sign_extend_byte_read, bool sign_extend_halfword_read,
    uint8_t *result_index)
{
    gbl_cpu_data_slot *slot;

    if (!gbl_cpu_add_data_read_slot(unit, address, width, alignment_lane,
                                    false, result_index)) {
        return false;
    }
    slot = &unit->data_slots[unit->data_slot_count - 1u];
    slot->rotate_halfword_read = rotate_halfword_read;
    slot->sign_extend_byte_read = sign_extend_byte_read;
    slot->sign_extend_halfword_read = sign_extend_halfword_read;
    return true;
}

static bool gbl_cpu_add_data_write_slot(gbl_cpu_unit *unit, uint32_t address,
                                        uint8_t width, uint32_t value)
{
    gbl_cpu_data_slot *slot;

    if (unit == NULL || unit->data_slot_count >= GBL_CPU_DATA_SLOT_MAX) {
        return false;
    }
    slot = &unit->data_slots[unit->data_slot_count];
    *slot = (gbl_cpu_data_slot){
        .operation = GBL_CPU_DATA_WRITE,
        .access_class = GBL_BUS_CLASS_NONSEQUENTIAL,
        .address = address,
        .write_value = value,
        .width = width,
        .result_index = UINT8_MAX
    };
    unit->data_slot_count++;
    return true;
}

static bool gbl_cpu_add_literal_register_effect(gbl_cpu_unit *unit,
                                                uint8_t register_index,
                                                uint32_t bank_cpsr,
                                                uint32_t value)
{
    gbl_cpu_register_effect *effect;

    if (unit == NULL || register_index >= 15u ||
        unit->register_effect_count >= GBL_CPU_REGISTER_EFFECT_MAX) {
        return false;
    }
    effect = &unit->register_effects[unit->register_effect_count];
    *effect = (gbl_cpu_register_effect){
        .bank_cpsr = bank_cpsr,
        .value = value,
        .register_index = register_index,
        .result_index = UINT8_MAX,
        .from_result = false
    };
    unit->register_effect_count++;
    return true;
}

static bool gbl_cpu_add_result_register_effect(gbl_cpu_unit *unit,
                                               uint8_t register_index,
                                               uint32_t bank_cpsr,
                                               uint8_t result_index)
{
    gbl_cpu_register_effect *effect;

    if (unit == NULL || register_index >= 15u ||
        result_index >= GBL_CPU_RESULT_SLOT_MAX ||
        unit->register_effect_count >= GBL_CPU_REGISTER_EFFECT_MAX) {
        return false;
    }
    effect = &unit->register_effects[unit->register_effect_count];
    *effect = (gbl_cpu_register_effect){
        .bank_cpsr = bank_cpsr,
        .register_index = register_index,
        .result_index = result_index,
        .from_result = true
    };
    unit->register_effect_count++;
    return true;
}

static bool gbl_cpu_add_status_effect(gbl_cpu_unit *unit, uint32_t mask,
                                      uint32_t value)
{
    if (unit == NULL || mask == 0u ||
        unit->status_effect_count >= GBL_CPU_STATUS_EFFECT_MAX) {
        return false;
    }
    unit->status_effects[unit->status_effect_count] =
        (gbl_cpu_status_effect){ mask, value, UINT8_MAX };
    unit->status_effect_count++;
    return true;
}

static bool gbl_cpu_add_spsr_status_effect(gbl_cpu_unit *unit,
                                           uint8_t spsr_index,
                                           uint32_t mask, uint32_t value)
{
    if (spsr_index >= 5u ||
        !gbl_cpu_add_status_effect(unit, mask, value)) {
        return false;
    }
    unit->status_effects[(size_t)unit->status_effect_count - 1u].spsr_index =
        spsr_index;
    return true;
}

static bool gbl_cpu_trace_stage(const gbl_run_options *options,
                                gbl_trace_record records[
                                    GBL_CPU_TRACE_UNIT_RECORD_MAX],
                                size_t *record_count,
                                gbl_trace_record record);

static void gbl_cpu_write_register(gbl_cpu_state *cpu, uint8_t index,
                                   uint32_t value);

static bool gbl_cpu_address_add(uint32_t address, uint32_t amount,
                                uint32_t *sum)
{
    if (sum == NULL || address > UINT32_MAX - amount) {
        return false;
    }
    *sum = address + amount;
    return true;
}

static gbl_trace_record gbl_cpu_dma_trace_record(
    const gbl_dma_step_result *step)
{
    gbl_trace_record record = { 0 };

    record.guest_cycle = step->completion_cycle;
    record.category = GBL_TRACE_DMA;
    record.bus_owner = GBL_MACHINE_BUS_OWNER_DMA;
    record.dma_status = step->status;
    record.dma_channel = step->channel;
    record.dma_source_address = step->source_address;
    record.dma_destination_address = step->destination_address;
    record.dma_value = step->value;
    record.dma_width = step->width;
    record.dma_startup_cycles = step->startup_cycles;
    record.dma_source_cycles = step->source_cycles;
    record.dma_destination_cycles = step->destination_cycles;
    record.dma_source_sequence = step->source_sequence;
    record.dma_destination_sequence = step->destination_sequence;
    record.dma_remaining = step->remaining;
    record.dma_completed = step->completed;
    record.dma_used_channel_latch = step->used_channel_latch;
    record.dma_value_provenance = step->value_provenance;
    record.dma_result_classification = step->result_classification;
    record.dma_dispute = step->dispute;
    record.dma_hardware_qualification = step->hardware_qualification;
    return record;
}

static bool gbl_cpu_mode_valid(uint32_t cpsr)
{
    switch (cpsr & GBL_CPU_CPSR_MODE_MASK) {
    case UINT32_C(0x10):
    case GBL_CPU_MODE_FIQ:
    case GBL_CPU_MODE_IRQ:
    case GBL_CPU_MODE_SVC:
    case GBL_CPU_MODE_ABT:
    case GBL_CPU_MODE_UND:
    case UINT32_C(0x1F):
        return true;
    default:
        return false;
    }
}

static bool gbl_cpu_spsr_index(uint32_t mode, size_t *index)
{
    size_t selected;

    if (index == NULL) {
        return false;
    }
    switch (mode & GBL_CPU_CPSR_MODE_MASK) {
    case GBL_CPU_MODE_FIQ: selected = 0u; break;
    case GBL_CPU_MODE_IRQ: selected = 1u; break;
    case GBL_CPU_MODE_SVC: selected = 2u; break;
    case GBL_CPU_MODE_ABT: selected = 3u; break;
    case GBL_CPU_MODE_UND: selected = 4u; break;
    default: return false;
    }
    *index = selected;
    return true;
}

typedef struct gbl_cpu_exception_plan {
    uint32_t mode;
    uint32_t vector;
    uint32_t link_register;
    size_t spsr_index;
    gbl_cpu_stop_reason stop_reason;
    bool reset_policy;
} gbl_cpu_exception_plan;

static bool gbl_cpu_prepare_exception(
    const gbl_machine *machine, gbl_exception_kind kind,
    const gbl_cpu_exception_context *context,
    gbl_cpu_exception_plan *plan)
{
    uint32_t return_address = 0u;
    uint32_t instruction_width;
    uint32_t destination_mode;
    uint32_t vector;
    gbl_cpu_stop_reason stop_reason;
    bool reset_policy = false;
    size_t spsr_index;

    if (machine == NULL || context == NULL || plan == NULL ||
        !machine->initialized) {
        return false;
    }
    if (!gbl_cpu_mode_valid(machine->cpu.cpsr)) {
        return false;
    }
    instruction_width = context->thumb_state ? UINT32_C(2) : UINT32_C(4);
    if (context->thumb_state !=
            ((machine->cpu.cpsr & GBL_CPU_CPSR_T) != 0u) ||
        (context->current_address & (instruction_width - 1u)) != 0u ||
        (context->next_address & (instruction_width - 1u)) != 0u ||
        (context->fault_address & (instruction_width - 1u)) != 0u) {
        return false;
    }

    switch (kind) {
    case GBL_EXCEPTION_RESET:
        destination_mode = GBL_CPU_MODE_SVC;
        vector = 0u;
        return_address = 0u;
        stop_reason = GBL_CPU_STOP_RESET_NO_BIOS;
        reset_policy = true;
        break;
    case GBL_EXCEPTION_UNDEFINED:
        destination_mode = GBL_CPU_MODE_UND;
        vector = 4u;
        stop_reason = GBL_CPU_STOP_UNDEFINED_NO_BIOS;
        if (!gbl_cpu_address_add(context->current_address, instruction_width,
                                 &return_address)) {
            return false;
        }
        break;
    case GBL_EXCEPTION_SWI:
        destination_mode = GBL_CPU_MODE_SVC;
        vector = 8u;
        stop_reason = GBL_CPU_STOP_SWI_NO_BIOS;
        if (!gbl_cpu_address_add(context->current_address, instruction_width,
                                 &return_address)) {
            return false;
        }
        break;
    case GBL_EXCEPTION_PREFETCH_ABORT:
        destination_mode = GBL_CPU_MODE_ABT;
        vector = 12u;
        stop_reason = GBL_CPU_STOP_PREFETCH_ABORT_NO_BIOS;
        if (!gbl_cpu_address_add(context->fault_address, UINT32_C(4),
                                 &return_address)) {
            return false;
        }
        break;
    case GBL_EXCEPTION_DATA_ABORT:
        destination_mode = GBL_CPU_MODE_ABT;
        vector = 16u;
        stop_reason = GBL_CPU_STOP_DATA_ABORT_NO_BIOS;
        if (!gbl_cpu_address_add(context->fault_address, UINT32_C(8),
                                 &return_address)) {
            return false;
        }
        break;
    case GBL_EXCEPTION_IRQ:
        destination_mode = GBL_CPU_MODE_IRQ;
        vector = 24u;
        stop_reason = GBL_CPU_STOP_IRQ_NO_BIOS;
        if (!gbl_cpu_address_add(context->next_address, UINT32_C(4),
                                 &return_address)) {
            return false;
        }
        break;
    case GBL_EXCEPTION_FIQ:
        destination_mode = GBL_CPU_MODE_FIQ;
        vector = 28u;
        stop_reason = GBL_CPU_STOP_FIQ_NO_BIOS;
        if (!gbl_cpu_address_add(context->next_address, UINT32_C(4),
                                 &return_address)) {
            return false;
        }
        break;
    case GBL_EXCEPTION_COPROCESSOR:
        destination_mode = GBL_CPU_MODE_UND;
        vector = 4u;
        stop_reason = GBL_CPU_STOP_COPROCESSOR_NO_BIOS;
        if (!gbl_cpu_address_add(context->current_address, instruction_width,
                                 &return_address)) {
            return false;
        }
        break;
    case GBL_EXCEPTION_NONE:
    default:
        return false;
    }

    if (!gbl_cpu_spsr_index(destination_mode, &spsr_index)) {
        return false;
    }
    plan->mode = destination_mode;
    plan->vector = vector;
    plan->link_register = return_address;
    plan->spsr_index = spsr_index;
    plan->stop_reason = stop_reason;
    plan->reset_policy = reset_policy;
    return true;
}

gbl_cpu_status gbl_cpu_set_cpsr(gbl_machine *machine, uint32_t cpsr)
{
    const bool new_thumb = (cpsr & GBL_CPU_CPSR_T) != 0u;
    bool old_thumb;

    if (machine == NULL || !machine->initialized) {
        return GBL_CPU_STATUS_INVALID_ARGUMENT;
    }
    if (!gbl_cpu_mode_valid(cpsr)) {
        return GBL_CPU_STATUS_INVALID_MODE;
    }
    old_thumb = (machine->cpu.cpsr & GBL_CPU_CPSR_T) != 0u;
    if (old_thumb != new_thumb) {
        const uint32_t alignment = new_thumb ? UINT32_C(1) : UINT32_C(3);

        if ((machine->cpu.execution_address & alignment) != 0u) {
            return GBL_CPU_STATUS_INVALID_CONTEXT;
        }
        machine->cpu.pipeline[0] = (gbl_cpu_pipeline_entry){ 0 };
        machine->cpu.pipeline[1] = (gbl_cpu_pipeline_entry){ 0 };
        machine->cpu.next_fetch_sequential = false;
    }
    machine->cpu.cpsr = cpsr;
    return GBL_CPU_STATUS_OK;
}

gbl_cpu_status gbl_cpu_get_spsr(const gbl_machine *machine,
                                uint32_t *spsr_out)
{
    size_t index;

    if (machine == NULL || spsr_out == NULL || !machine->initialized) {
        return GBL_CPU_STATUS_INVALID_ARGUMENT;
    }
    if (!gbl_cpu_mode_valid(machine->cpu.cpsr)) {
        return GBL_CPU_STATUS_INVALID_MODE;
    }
    if (!gbl_cpu_spsr_index(machine->cpu.cpsr, &index)) {
        return GBL_CPU_STATUS_SPSR_ABSENT;
    }
    *spsr_out = machine->cpu.spsr[index];
    return GBL_CPU_STATUS_OK;
}

gbl_cpu_status gbl_cpu_enter_exception(
    gbl_machine *machine, gbl_exception_kind kind,
    const gbl_cpu_exception_context *context)
{
    gbl_cpu_exception_plan plan;
    gbl_cpu_state *cpu;
    gbl_bus_result internal_result;
    uint32_t saved_cpsr;
    uint32_t next_cpsr;

    if (machine == NULL || context == NULL || !machine->initialized) {
        return GBL_CPU_STATUS_INVALID_ARGUMENT;
    }
    if (kind <= GBL_EXCEPTION_NONE || kind > GBL_EXCEPTION_COPROCESSOR) {
        return GBL_CPU_STATUS_INVALID_EXCEPTION;
    }
    if (!gbl_cpu_mode_valid(machine->cpu.cpsr)) {
        return GBL_CPU_STATUS_INVALID_MODE;
    }
    if (!gbl_cpu_prepare_exception(machine, kind, context, &plan)) {
        return GBL_CPU_STATUS_INVALID_CONTEXT;
    }
    if ((kind == GBL_EXCEPTION_IRQ &&
         (machine->cpu.cpsr & GBL_CPU_CPSR_I) != 0u) ||
        (kind == GBL_EXCEPTION_FIQ &&
         (machine->cpu.cpsr & GBL_CPU_CPSR_F) != 0u)) {
        return GBL_CPU_STATUS_MASKED;
    }
    if (machine->master_cycles == UINT64_MAX) {
        return GBL_CPU_STATUS_COUNTER_OVERFLOW;
    }

    internal_result = gbl_bus_internal(machine, UINT64_C(1));
    if (internal_result.status == GBL_BUS_STATUS_TIME_OVERFLOW) {
        return GBL_CPU_STATUS_COUNTER_OVERFLOW;
    }
    if (internal_result.status != GBL_BUS_STATUS_OK ||
        internal_result.cycles != UINT64_C(1)) {
        return GBL_CPU_STATUS_BUS_ERROR;
    }

    cpu = &machine->cpu;
    saved_cpsr = cpu->cpsr;
    cpu->spsr[plan.spsr_index] = plan.reset_policy ? 0u : saved_cpsr;
    next_cpsr = saved_cpsr & ~(GBL_CPU_CPSR_T | GBL_CPU_CPSR_MODE_MASK);
    next_cpsr |= plan.mode | GBL_CPU_CPSR_I;
    if (kind == GBL_EXCEPTION_RESET || kind == GBL_EXCEPTION_FIQ) {
        next_cpsr |= GBL_CPU_CPSR_F;
    }
    cpu->cpsr = next_cpsr;
    gbl_cpu_write_register(cpu, (uint8_t)14u, plan.link_register);
    cpu->execution_address = plan.vector;
    cpu->pipeline[0] = (gbl_cpu_pipeline_entry){ 0 };
    cpu->pipeline[1] = (gbl_cpu_pipeline_entry){ 0 };
    cpu->next_fetch_sequential = false;
    cpu->last_exception = (gbl_cpu_last_exception){
        kind,
        context->current_address,
        context->next_address,
        context->fault_address,
        context->thumb_state
    };
    cpu->stopped = true;
    cpu->stop_reason = plan.stop_reason;
    return GBL_CPU_STATUS_OK;
}

static bool gbl_cpu_fetch_address_valid(uint32_t address, uint8_t width)
{
    const uint32_t mask = width == (uint8_t)GBL_CPU_WORD_WIDTH
                              ? UINT32_C(3)
                              : UINT32_C(1);

    return (width == (uint8_t)GBL_CPU_WORD_WIDTH ||
            width == (uint8_t)GBL_CPU_HALF_WIDTH) &&
           (address & mask) == 0u &&
           address <= UINT32_MAX - mask;
}

static bool gbl_cpu_schedule_refill(gbl_cpu_unit *unit, uint32_t address,
                                    bool thumb)
{
    const uint8_t width = thumb ? (uint8_t)GBL_CPU_HALF_WIDTH
                                : (uint8_t)GBL_CPU_WORD_WIDTH;
    uint32_t second_address;

    if (unit == NULL || !gbl_cpu_fetch_address_valid(address, width) ||
        !gbl_cpu_address_add(address, (uint32_t)width, &second_address) ||
        !gbl_cpu_fetch_address_valid(second_address, width)) {
        return false;
    }
    unit->fetch_count = (uint8_t)GBL_CPU_FETCH_SLOT_MAX;
    unit->fetch_width = width;
    unit->fetch_address[0] = address;
    unit->fetch_address[1] = second_address;
    unit->fetch_class[0] = GBL_BUS_CLASS_NONSEQUENTIAL;
    unit->fetch_class[1] = GBL_BUS_CLASS_SEQUENTIAL;
    return gbl_cpu_refresh_cycle_cost(unit);
}

/* ARMv4T word loads to PC continue in ARM state. The loaded word result is
 * the source for both refill addresses; the low bits are ignored, and the
 * second address wraps in the 32-bit address space. Preflight validates this
 * bounded symbolic form before the data read, then execution resolves it
 * once from the actual result. Scalar LDR applies its rotation first; LDM
 * uses the aligned block-transfer word unchanged. */
static bool gbl_cpu_schedule_result_refill(gbl_cpu_unit *unit,
                                           uint8_t result_index,
                                           bool thumb)
{
    const uint8_t width = thumb ? (uint8_t)GBL_CPU_HALF_WIDTH
                                : (uint8_t)GBL_CPU_WORD_WIDTH;

    if (unit == NULL || result_index >= unit->result_count) {
        return false;
    }
    unit->kind = GBL_CPU_UNIT_BRANCH;
    unit->changes_state = false;
    unit->thumb_state = thumb;
    unit->fetch_address_from_result = true;
    unit->fetch_result_index = result_index;
    unit->fetch_result_mask = thumb ? ~UINT32_C(1) : ~UINT32_C(3);
    unit->fetch_count = (uint8_t)GBL_CPU_FETCH_SLOT_MAX;
    unit->fetch_width = width;
    unit->fetch_result_addend[0] = 0u;
    unit->fetch_result_addend[1] = (uint32_t)width;
    unit->fetch_class[0] = GBL_BUS_CLASS_NONSEQUENTIAL;
    unit->fetch_class[1] = GBL_BUS_CLASS_SEQUENTIAL;
    return gbl_cpu_refresh_cycle_cost(unit);
}

static bool gbl_cpu_result_refill_shape_valid(const gbl_cpu_unit *unit)
{
    uint8_t index;

    if (unit == NULL || unit->fetch_result_index >= unit->result_count) {
        return false;
    }
    if (!unit->multiple_transfer) {
        return unit->data_slot_count == 1u && unit->result_count == 1u &&
               unit->data_slots[0].operation == GBL_CPU_DATA_READ &&
               unit->data_slots[0].width ==
                   (uint8_t)GBL_CPU_WORD_WIDTH &&
               unit->data_slots[0].result_index ==
                   unit->fetch_result_index &&
               unit->data_slots[0].rotate_word_read;
    }

    if (unit->data_slot_count == 0u ||
        unit->data_slot_count != unit->result_count ||
        unit->fetch_result_index + 1u != unit->data_slot_count) {
        return false;
    }
    for (index = 0u; index < unit->data_slot_count; ++index) {
        const gbl_cpu_data_slot *slot = &unit->data_slots[index];

        if (slot->operation != GBL_CPU_DATA_READ ||
            slot->width != (uint8_t)GBL_CPU_WORD_WIDTH ||
            slot->result_index != index || slot->rotate_word_read ||
            slot->rotate_halfword_read || slot->sign_extend_byte_read ||
            slot->sign_extend_halfword_read) {
            return false;
        }
    }
    return true;
}

static bool gbl_cpu_spsr_return_shape_valid(const gbl_cpu_unit *unit)
{
    return unit != NULL && unit->restore_cpsr_from_spsr &&
           unit->multiple_transfer && unit->fetch_address_from_result &&
           unit->status_effect_count == 1u &&
           unit->status_effects[0].spsr_index == UINT8_MAX &&
           unit->status_effects[0].mask == UINT32_MAX &&
           unit->thumb_state ==
               ((unit->status_effects[0].value & GBL_CPU_CPSR_T) != 0u);
}

static bool gbl_cpu_schedule_sequential_fetch(gbl_cpu_unit *unit,
                                              uint32_t address, bool thumb)
{
    const uint8_t width = thumb ? (uint8_t)GBL_CPU_HALF_WIDTH
                                : (uint8_t)GBL_CPU_WORD_WIDTH;

    if (unit == NULL || !gbl_cpu_fetch_address_valid(address, width)) {
        return false;
    }
    unit->fetch_count = (uint8_t)1u;
    unit->fetch_width = width;
    unit->fetch_address[0] = address;
    unit->fetch_class[0] = GBL_BUS_CLASS_SEQUENTIAL;
    return gbl_cpu_refresh_cycle_cost(unit);
}

static bool gbl_cpu_pipeline_is_empty(const gbl_cpu_state *cpu)
{
    return !cpu->pipeline[0].valid && !cpu->pipeline[1].valid;
}

static bool gbl_cpu_pipeline_is_arm_sequential_wrap(
    const gbl_cpu_state *cpu)
{
    return (cpu->cpsr & GBL_CPU_CPSR_T) == 0u &&
           cpu->pipeline[0].address == UINT32_C(0xFFFFFFFC) &&
           cpu->pipeline[1].address == 0u;
}

static bool gbl_cpu_pipeline_is_thumb_sequential_wrap(
    const gbl_cpu_state *cpu)
{
    return (cpu->cpsr & GBL_CPU_CPSR_T) != 0u &&
           cpu->pipeline[0].address == UINT32_C(0xFFFFFFFE) &&
           cpu->pipeline[1].address == 0u;
}

static bool gbl_cpu_pipeline_is_ready(const gbl_cpu_state *cpu)
{
    const bool thumb = (cpu->cpsr & GBL_CPU_CPSR_T) != 0u;
    const uint8_t width = thumb ? (uint8_t)GBL_CPU_HALF_WIDTH
                                : (uint8_t)GBL_CPU_WORD_WIDTH;
    uint32_t expected_second_address;

    if (!cpu->pipeline[0].valid || !cpu->pipeline[1].valid ||
        cpu->pipeline[0].width != width ||
        cpu->pipeline[1].width != width ||
        !gbl_cpu_fetch_address_valid(cpu->pipeline[0].address, width) ||
        !gbl_cpu_fetch_address_valid(cpu->pipeline[1].address, width) ||
        cpu->execution_address != cpu->pipeline[0].address ||
        !cpu->next_fetch_sequential) {
        return false;
    }
    if (!gbl_cpu_address_add(cpu->pipeline[0].address, (uint32_t)width,
                             &expected_second_address)) {
        /* Result-driven PC loads can form one exact modulo refill pair. The
         * decoder recognizes it explicitly and stops before executing the
         * first instruction at that ARM/Thumb boundary. */
        return thumb ? gbl_cpu_pipeline_is_thumb_sequential_wrap(cpu)
                     : gbl_cpu_pipeline_is_arm_sequential_wrap(cpu);
    }
    return cpu->pipeline[1].address == expected_second_address;
}

static uint32_t gbl_cpu_read_register_in_bank(
    const gbl_cpu_state *cpu, uint8_t index, uint32_t bank_cpsr,
    uint32_t instruction_address, gbl_cpu_pc_read_context context)
{
    uint32_t mode;

    if (index == (uint8_t)15u) {
        const bool thumb = (bank_cpsr & GBL_CPU_CPSR_T) != 0u;

        if (context == GBL_CPU_PC_READ_THUMB_ALIGNED_ADDRESS) {
            return (instruction_address + UINT32_C(4)) & ~UINT32_C(3);
        }
        return instruction_address +
               (thumb ? UINT32_C(4)
                      : context == GBL_CPU_PC_READ_REGISTER_SHIFT ||
                                context == GBL_CPU_PC_READ_STORE_SOURCE
                            ? UINT32_C(12)
                            : UINT32_C(8));
    }
    if (index <= (uint8_t)7u) {
        return cpu->r0_7[index];
    }
    mode = bank_cpsr & GBL_CPU_CPSR_MODE_MASK;
    if (mode == GBL_CPU_MODE_FIQ) {
        return cpu->fiq_r8_14[(size_t)index - 8u];
    }
    if (index <= (uint8_t)12u) {
        return cpu->usr_r8_14[(size_t)index - 8u];
    }
    switch (mode) {
    case GBL_CPU_MODE_IRQ:
        return cpu->irq_r13_14[(size_t)index - 13u];
    case GBL_CPU_MODE_SVC:
        return cpu->svc_r13_14[(size_t)index - 13u];
    case GBL_CPU_MODE_ABT:
        return cpu->abt_r13_14[(size_t)index - 13u];
    case GBL_CPU_MODE_UND:
        return cpu->und_r13_14[(size_t)index - 13u];
    default:
        return cpu->usr_r8_14[(size_t)index - 8u];
    }
}

static uint32_t gbl_cpu_read_register(const gbl_cpu_state *cpu,
                                      uint8_t index,
                                      uint32_t instruction_address,
                                      gbl_cpu_pc_read_context context)
{
    return gbl_cpu_read_register_in_bank(
        cpu, index, cpu->cpsr, instruction_address, context);
}

static void gbl_cpu_write_register_in_bank(gbl_cpu_state *cpu,
                                           uint8_t index,
                                           uint32_t bank_cpsr,
                                           uint32_t value)
{
    const uint32_t mode = bank_cpsr & GBL_CPU_CPSR_MODE_MASK;

    if (index <= (uint8_t)7u) {
        cpu->r0_7[index] = value;
    } else if (mode == GBL_CPU_MODE_FIQ) {
        cpu->fiq_r8_14[(size_t)index - 8u] = value;
    } else if (index <= (uint8_t)12u) {
        cpu->usr_r8_14[(size_t)index - 8u] = value;
    } else {
        switch (mode) {
        case GBL_CPU_MODE_IRQ:
            cpu->irq_r13_14[(size_t)index - 13u] = value;
            break;
        case GBL_CPU_MODE_SVC:
            cpu->svc_r13_14[(size_t)index - 13u] = value;
            break;
        case GBL_CPU_MODE_ABT:
            cpu->abt_r13_14[(size_t)index - 13u] = value;
            break;
        case GBL_CPU_MODE_UND:
            cpu->und_r13_14[(size_t)index - 13u] = value;
            break;
        default:
            cpu->usr_r8_14[(size_t)index - 8u] = value;
            break;
        }
    }
}

static void gbl_cpu_write_register(gbl_cpu_state *cpu, uint8_t index,
                                   uint32_t value)
{
    gbl_cpu_write_register_in_bank(cpu, index, cpu->cpsr, value);
}

static void gbl_cpu_commit_effects(gbl_cpu_state *cpu,
                                   const gbl_cpu_unit *unit)
{
    uint8_t index;

    for (index = 0u; index < unit->register_effect_count; ++index) {
        const gbl_cpu_register_effect *effect =
            &unit->register_effects[index];
        const uint32_t value = effect->from_result
                                   ? unit->results[effect->result_index]
                                   : effect->value;

        gbl_cpu_write_register_in_bank(cpu, effect->register_index,
                                       effect->bank_cpsr, value);
    }
    for (index = 0u; index < unit->status_effect_count; ++index) {
        const gbl_cpu_status_effect *effect = &unit->status_effects[index];

        uint32_t *status = effect->spsr_index == UINT8_MAX
                               ? &cpu->cpsr
                               : &cpu->spsr[effect->spsr_index];

        *status = (*status & ~effect->mask) |
                  (effect->value & effect->mask);
    }
}

static bool gbl_cpu_condition_passed(uint8_t condition, uint32_t cpsr)
{
    const bool n = (cpsr & GBL_CPU_CPSR_N) != 0u;
    const bool z = (cpsr & GBL_CPU_CPSR_Z) != 0u;
    const bool c = (cpsr & GBL_CPU_CPSR_C) != 0u;
    const bool v = (cpsr & GBL_CPU_CPSR_V) != 0u;

    switch (condition) {
    case 0u: return z;
    case 1u: return !z;
    case 2u: return c;
    case 3u: return !c;
    case 4u: return n;
    case 5u: return !n;
    case 6u: return v;
    case 7u: return !v;
    case 8u: return c && !z;
    case 9u: return !c || z;
    case 10u: return n == v;
    case 11u: return n != v;
    case 12u: return !z && n == v;
    case 13u: return z || n != v;
    case 14u: return true;
    default: return false;
    }
}

static uint32_t gbl_cpu_nz_flags(uint32_t value)
{
    return ((value & GBL_CPU_CPSR_N) != 0u ? GBL_CPU_CPSR_N : 0u) |
           (value == 0u ? GBL_CPU_CPSR_Z : 0u);
}

static bool gbl_cpu_next_sequential_address(const gbl_cpu_state *cpu,
                                           bool thumb,
                                           uint32_t *address)
{
    const uint32_t width = thumb ? UINT32_C(2) : UINT32_C(4);

    return gbl_cpu_address_add(cpu->pipeline[1].address, width, address);
}

static bool gbl_cpu_data_span_valid(uint32_t address, uint8_t width)
{
    return (width == (uint8_t)GBL_CPU_HALF_WIDTH ||
            width == (uint8_t)GBL_CPU_WORD_WIDTH || width == 1u) &&
           address <= UINT32_MAX - ((uint32_t)width - UINT32_C(1));
}

static bool gbl_cpu_prepare_data_transfer(gbl_cpu_unit *unit,
                                          uint32_t address, uint8_t width,
                                          bool load, uint8_t destination,
                                          uint32_t store_value,
                                          bool rotate_word_load,
                                          uint32_t bank_cpsr)
{
    uint8_t result_index = UINT8_MAX;
    const uint32_t bus_address =
        width == (uint8_t)GBL_CPU_WORD_WIDTH
            ? address & ~UINT32_C(3)
            : address;

    unit->kind = GBL_CPU_UNIT_TRANSFER;
    if (load) {
        return gbl_cpu_add_data_read_slot(
                   unit, bus_address, width,
                   (uint8_t)(address & UINT32_C(3)), rotate_word_load,
                   &result_index) &&
               gbl_cpu_add_result_register_effect(
                   unit, destination, bank_cpsr, result_index);
    }
    return gbl_cpu_add_data_write_slot(unit, bus_address, width, store_value);
}

/* DDI0100I A6.5 defines the Thumb transfer forms. This builder translates
 * those decoded operands into the existing single-unit bus/result plan. Its
 * odd halfword handling follows the selected GBA ARM7 policy in GBATEK 3.06;
 * the bus remains responsible for the raw value returned by each region. */
static bool gbl_cpu_prepare_thumb_data_transfer(
    gbl_cpu_unit *unit, uint32_t address, uint8_t width, bool load,
    uint8_t destination, uint32_t store_value, bool signed_byte,
    bool signed_halfword, uint32_t bank_cpsr,
    uint32_t next_fetch_address)
{
    uint32_t bus_address = address;
    uint8_t slot_width = width;
    uint8_t result_index = UINT8_MAX;
    uint8_t alignment_lane = (uint8_t)(address & UINT32_C(3));
    bool rotate_word = false;
    bool rotate_halfword = false;
    bool extend_byte = signed_byte;
    bool extend_halfword = signed_halfword;

    if (width == (uint8_t)GBL_CPU_WORD_WIDTH) {
        bus_address &= ~UINT32_C(3);
        rotate_word = load;
    } else if (width == (uint8_t)GBL_CPU_HALF_WIDTH) {
        const bool odd = (address & UINT32_C(1)) != 0u;

        if (load && signed_halfword && odd) {
            /* ARM7/GBA odd LDRSH is the signed byte at the original address. */
            slot_width = 1u;
            extend_byte = true;
            extend_halfword = false;
        } else {
            bus_address &= ~UINT32_C(1);
            alignment_lane = (uint8_t)(address & UINT32_C(1));
            rotate_halfword = load && !signed_halfword && odd;
        }
    }

    if (!gbl_cpu_data_span_valid(bus_address, slot_width)) {
        return false;
    }
    unit->kind = GBL_CPU_UNIT_TRANSFER;
    if (load) {
        const bool use_subword_slot = rotate_halfword || extend_byte ||
                                      extend_halfword;
        const bool slot_added = use_subword_slot
            ? gbl_cpu_add_subword_read_slot(
                  unit, bus_address, slot_width, alignment_lane,
                  rotate_halfword, extend_byte, extend_halfword,
                  &result_index)
            : gbl_cpu_add_data_read_slot(
                  unit, bus_address, slot_width, alignment_lane,
                  rotate_word, &result_index);

        if (!slot_added ||
            !gbl_cpu_add_result_register_effect(
                unit, destination, bank_cpsr, result_index)) {
            return false;
        }
    } else {
        uint32_t masked_value = store_value;

        if (slot_width == 1u) {
            masked_value &= UINT32_C(0x000000FF);
        } else if (slot_width == (uint8_t)GBL_CPU_HALF_WIDTH) {
            masked_value &= UINT32_C(0x0000FFFF);
        } else {
            bus_address = address & ~UINT32_C(3);
        }
        if (!gbl_cpu_add_data_write_slot(unit, bus_address, slot_width,
                                         masked_value)) {
            return false;
        }
    }
    if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address, true)) {
        return false;
    }
    return true;
}

static bool gbl_cpu_is_arm_extension(uint32_t opcode)
{
    /* Allocated later-version encodings in DDI0100I A3.16.2-.4, A4.
     * Reserved holes are not evidence of a supported or later instruction. */
    return (opcode & UINT32_C(0xFE000000)) == UINT32_C(0xFA000000) ||
           (opcode & UINT32_C(0x0FFFFFF0)) == UINT32_C(0x012FFF30) ||
           (opcode & UINT32_C(0x0FFFFFF0)) == UINT32_C(0x012FFF20) ||
           (opcode & UINT32_C(0x0FF000F0)) == UINT32_C(0x01200070) ||
           (opcode & UINT32_C(0x0FFF0FF0)) == UINT32_C(0x016F0F10) ||
           (opcode & UINT32_C(0x0F900FF0)) == UINT32_C(0x01000050) ||
           (opcode & UINT32_C(0x0F900090)) == UINT32_C(0x01000080) ||
           (opcode & UINT32_C(0x0FF000F0)) == UINT32_C(0x00400090) ||
           (opcode & UINT32_C(0x0FE00000)) == UINT32_C(0x0C400000) ||
           (opcode & UINT32_C(0x0FFF0FF0)) == UINT32_C(0x06BF0F30) ||
           (opcode & UINT32_C(0x0FFF0FF0)) == UINT32_C(0x06BF0FB0) ||
           (opcode & UINT32_C(0x0FFF0FF0)) == UINT32_C(0x06FF0FB0);
}

/* ARM ARM DDI0100I A3.16.5 reserves this encoding space as architecturally
 * undefined; ARM7TDMI implements the ARMv4T architecture. */
static bool gbl_cpu_is_arm_undefined_space(uint32_t opcode)
{
    return (opcode & UINT32_C(0x0FF000F0)) == UINT32_C(0x07F000F0);
}

static bool gbl_cpu_schedule_pc_write(gbl_cpu_unit *unit, uint32_t value,
                                      uint32_t restored_cpsr,
                                      bool restore_status);

static bool gbl_cpu_decode_data_processing(const gbl_cpu_state *cpu,
                                           gbl_cpu_unit *unit,
                                           uint32_t next_fetch_address)
{
    const uint32_t opcode = unit->opcode;
    const bool immediate = (opcode & UINT32_C(0x02000000)) != 0u;
    const bool register_shift = !immediate &&
        (opcode & UINT32_C(0x00000010)) != 0u;
    const uint8_t operation = (uint8_t)((opcode >> 21u) & UINT32_C(0x0F));
    const bool set_flags = (opcode & UINT32_C(0x00100000)) != 0u;
    const uint8_t rn = (uint8_t)((opcode >> 16u) & UINT32_C(0x0F));
    const uint8_t rd = (uint8_t)((opcode >> 12u) & UINT32_C(0x0F));
    uint8_t rm = (uint8_t)(opcode & UINT32_C(0x0F));
    uint32_t operand = 0u;
    uint32_t left = 0u;
    uint32_t flags = 0u;
    uint32_t flag_mask = 0u;
    bool writes_destination = true;
    bool supported_operation = false;
    bool is_test = false;
    bool is_arithmetic = false;
    bool condition_passed;
    gbl_shift_result shifted = { 0u, false };
    gbl_arithmetic_result arithmetic = { 0u, false, false };

    if ((opcode & UINT32_C(0x0C000000)) != 0u) {
        return false;
    }
    if (gbl_cpu_is_arm_extension(opcode)) {
        unit->kind = GBL_CPU_UNIT_LATER_ARCHITECTURE;
        return true;
    }
    if ((opcode & UINT32_C(0x01900000)) == UINT32_C(0x01000000) &&
        (immediate || (opcode & UINT32_C(0x00000090)) != UINT32_C(0x00000090))) {
        /* Unrecognized control-space encodings: A3.16 distinguishes a
         * malformed allocated decode from an unallocated decode. Exact PSR
         * and BX encodings were consumed before data-processing dispatch. */
        const uint32_t decode = opcode & UINT32_C(0x0FF000F0);
        const bool allocated = decode == UINT32_C(0x01000000) ||
            decode == UINT32_C(0x01400000) ||
            decode == UINT32_C(0x01200000) ||
            decode == UINT32_C(0x01600000) ||
            decode == UINT32_C(0x01200010) ||
            (opcode & UINT32_C(0x0FB00000)) == UINT32_C(0x03200000);
        unit->kind = allocated ? GBL_CPU_UNIT_UNPREDICTABLE
                               : GBL_CPU_UNIT_UNDEFINED;
        return true;
    }
    if (!immediate && (opcode & UINT32_C(0x00000090)) ==
                           UINT32_C(0x00000090)) {
        const uint32_t decode = opcode & UINT32_C(0x0FF000F0);
        unit->kind = decode == UINT32_C(0x01000090) ||
                     decode == UINT32_C(0x01400090)
                         ? GBL_CPU_UNIT_UNPREDICTABLE
                         : GBL_CPU_UNIT_UNDEFINED;
        return true;
    }

    switch (operation) {
    case 0u:  /* AND */
    case 1u:  /* EOR */
    case 2u:  /* SUB */
    case 3u:  /* RSB */
    case 4u:  /* ADD */
    case 5u:  /* ADC */
    case 6u:  /* SBC */
    case 7u:  /* RSC */
    case 8u:  /* TST */
    case 9u:  /* TEQ */
    case 10u: /* CMP */
    case 11u: /* CMN */
    case 12u: /* ORR */
    case 13u: /* MOV */
    case 14u: /* BIC */
    case 15u: /* MVN */
        supported_operation = true;
        break;
    default:
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }

    is_test = operation >= 8u && operation <= 11u;
    writes_destination = !is_test;
    is_arithmetic = operation == 2u || operation == 3u ||
                    operation == 4u || operation == 5u ||
                    operation == 6u || operation == 7u ||
                    operation == 10u || operation == 11u;
    if ((is_test && (!set_flags || rd != 0u)) ||
        ((operation == 13u || operation == 15u) && rn != 0u)) {
        unit->kind = is_test && !set_flags ? GBL_CPU_UNIT_UNDEFINED
                                         : GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    if (!supported_operation) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if (!immediate && (opcode & UINT32_C(0x00000010)) != 0u &&
        ((opcode >> 8u) & UINT32_C(0x0F)) == 15u) {
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    condition_passed = gbl_cpu_condition_passed(
        (uint8_t)(opcode >> 28u), cpu->cpsr);
    if (!condition_passed) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        return gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                                 false);
    }

    /* ARM7DI Data Sheet §4.4.5: a register-controlled shift observes R15 as
     * instruction address + 12, for either source operand (Rn or Rm). */
    if (operation != 13u) {
        left = gbl_cpu_read_register(cpu, rn, unit->instruction_address,
            register_shift ? GBL_CPU_PC_READ_REGISTER_SHIFT
                           : GBL_CPU_PC_READ_NORMAL);
    }
    if (immediate) {
        const uint8_t rotate =
            (uint8_t)(((opcode >> 8u) & UINT32_C(0x0F)) * 2u);

        operand = opcode & UINT32_C(0xFF);
        if (rotate != 0u) {
            shifted = gbl_shift(GBL_SHIFT_ROR, operand, rotate,
                                (cpu->cpsr & GBL_CPU_CPSR_C) != 0u, true);
            operand = shifted.value;
            shifted.carry = (operand & GBL_CPU_CPSR_N) != 0u;
        } else {
            shifted.value = operand;
            shifted.carry = (cpu->cpsr & GBL_CPU_CPSR_C) != 0u;
        }
    } else {
        const uint8_t form = (uint8_t)(register_shift ? 1u : 0u);
        uint8_t amount;
        const gbl_shift_kind shift_kind =
            (gbl_shift_kind)((opcode >> 5u) & UINT32_C(3));

        operand = gbl_cpu_read_register(
            cpu, rm, unit->instruction_address,
            register_shift ? GBL_CPU_PC_READ_REGISTER_SHIFT
                           : GBL_CPU_PC_READ_NORMAL);
        if (register_shift) {
            const uint8_t rs = (uint8_t)((opcode >> 8u) & UINT32_C(0x0F));

            amount = (uint8_t)(gbl_cpu_read_register(
                cpu, rs, unit->instruction_address,
                GBL_CPU_PC_READ_NORMAL) & UINT32_C(0xFF));
        } else {
            amount = (uint8_t)((opcode >> 7u) & UINT32_C(0x1F));
        }
        unit->internal_cycles = gbl_cpu_arm_operand2_internal_cycles[form];
        shifted = gbl_shift(shift_kind, operand, amount,
                            (cpu->cpsr & GBL_CPU_CPSR_C) != 0u,
                            register_shift);
        operand = shifted.value;
    }

    switch (operation) {
    case 0u: /* AND */
    case 8u: /* TST */
        unit->value = left & operand;
        break;
    case 1u: /* EOR */
    case 9u: /* TEQ */
        unit->value = left ^ operand;
        break;
    case 2u: /* SUB */
    case 10u: /* CMP */
        arithmetic = gbl_sub_flags(left, operand);
        unit->value = arithmetic.value;
        break;
    case 3u: /* RSB */
        arithmetic = gbl_sub_flags(operand, left);
        unit->value = arithmetic.value;
        break;
    case 4u: /* ADD */
    case 11u: /* CMN */
        arithmetic = gbl_add_flags(left, operand);
        unit->value = arithmetic.value;
        break;
    case 5u: /* ADC */
        arithmetic = gbl_add_with_carry_flags(
            left, operand, (cpu->cpsr & GBL_CPU_CPSR_C) != 0u);
        unit->value = arithmetic.value;
        break;
    case 6u: /* SBC */
        arithmetic = gbl_sub_with_carry_flags(
            left, operand, (cpu->cpsr & GBL_CPU_CPSR_C) != 0u);
        unit->value = arithmetic.value;
        break;
    case 7u: /* RSC */
        arithmetic = gbl_sub_with_carry_flags(
            operand, left, (cpu->cpsr & GBL_CPU_CPSR_C) != 0u);
        unit->value = arithmetic.value;
        break;
    case 12u: /* ORR */
        unit->value = left | operand;
        break;
    case 13u: /* MOV */
        unit->value = operand;
        break;
    case 14u: /* BIC */
        unit->value = left & ~operand;
        break;
    case 15u: /* MVN */
        unit->value = ~operand;
        break;
    default:
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }

    if (set_flags) {
        if (is_arithmetic) {
            flags = gbl_cpu_nz_flags(unit->value);
            if (arithmetic.carry) {
                flags |= GBL_CPU_CPSR_C;
            }
            if (arithmetic.overflow) {
                flags |= GBL_CPU_CPSR_V;
            }
            flag_mask = GBL_CPU_CPSR_N | GBL_CPU_CPSR_Z |
                        GBL_CPU_CPSR_C | GBL_CPU_CPSR_V;
        } else {
            flags = gbl_cpu_nz_flags(unit->value);
            if (shifted.carry) {
                flags |= GBL_CPU_CPSR_C;
            }
            flag_mask = GBL_CPU_CPSR_N | GBL_CPU_CPSR_Z | GBL_CPU_CPSR_C;
        }
    }

    if (writes_destination && rd == (uint8_t)15u) {
        uint32_t restored_cpsr = cpu->cpsr;

        if (set_flags) {
            size_t spsr_index;

            if (!gbl_cpu_spsr_index(cpu->cpsr, &spsr_index)) {
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
            restored_cpsr = cpu->spsr[spsr_index];
            if (!gbl_cpu_mode_valid(restored_cpsr)) {
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
        }
        return gbl_cpu_schedule_pc_write(unit, unit->value, restored_cpsr,
                                         set_flags);
    }

    unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
    if ((writes_destination &&
         !gbl_cpu_add_literal_register_effect(unit, rd, cpu->cpsr,
                                              unit->value)) ||
        (set_flags &&
         !gbl_cpu_add_status_effect(unit, flag_mask, flags))) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    return gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address, false);
}

static bool gbl_cpu_schedule_branch(gbl_cpu_unit *unit, uint32_t target,
                                    bool thumb)
{
    unit->kind = GBL_CPU_UNIT_BRANCH;
    unit->branch_target = target;
    unit->changes_state = true;
    unit->thumb_state = thumb;
    if (!gbl_cpu_schedule_refill(unit, target, thumb)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
    }
    return true;
}

static bool gbl_cpu_schedule_pc_write(gbl_cpu_unit *unit, uint32_t value,
                                      uint32_t restored_cpsr,
                                      bool restore_status)
{
    const bool thumb = (restored_cpsr & GBL_CPU_CPSR_T) != 0u;
    uint32_t target = value;

    if (thumb) {
        target &= ~UINT32_C(1);
    } else if ((target & UINT32_C(3)) != 0u) {
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    if (restore_status &&
        !gbl_cpu_add_status_effect(unit, UINT32_MAX, restored_cpsr)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    unit->kind = GBL_CPU_UNIT_BRANCH;
    unit->branch_target = target;
    unit->changes_state = false;
    unit->thumb_state = thumb;
    if (!gbl_cpu_schedule_refill(unit, target, thumb)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
    }
    return true;
}

static bool gbl_cpu_is_arm_psr_transfer(uint32_t opcode)
{
    return (opcode & UINT32_C(0x0FBF0FFF)) == UINT32_C(0x010F0000) ||
           (opcode & UINT32_C(0x0FB0FFF0)) == UINT32_C(0x0120F000) ||
           (opcode & UINT32_C(0x0FB0F000)) == UINT32_C(0x0320F000);
}

/* ARM ARM DDI0100I A4.1.39 defines the complete MSR operation, including the
 * UnallocMask and StateMask checks before applying the selected byte fields. */
static bool gbl_cpu_decode_arm_psr_transfer(const gbl_cpu_state *cpu,
                                           gbl_cpu_unit *unit,
                                           uint32_t next_fetch_address)
{
    const uint32_t opcode = unit->opcode;
    const bool saved = (opcode & UINT32_C(0x00400000)) != 0u;
    const bool immediate = (opcode & UINT32_C(0x02000000)) != 0u;

    if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        return gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                                 false);
    }

    if ((opcode & UINT32_C(0x0FBF0FFF)) == UINT32_C(0x010F0000)) {
        const uint8_t destination =
            (uint8_t)((opcode >> 12u) & UINT32_C(0x0F));
        size_t spsr_index;
        uint32_t status;

        if (destination == (uint8_t)15u) {
            unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
            return true;
        }
        if (saved) {
            if (!gbl_cpu_spsr_index(cpu->cpsr, &spsr_index)) {
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
            status = cpu->spsr[spsr_index];
        } else {
            status = cpu->cpsr;
        }
        unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
        if (!gbl_cpu_add_literal_register_effect(
                unit, destination, cpu->cpsr,
                status & ~GBL_CPU_PSR_UNALLOC_MASK) ||
            !gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               false)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    {
        const uint8_t fields = (uint8_t)((opcode >> 16u) & UINT32_C(0x0F));
        uint32_t operand;
        uint32_t byte_mask = 0u;
        uint32_t write_mask;
        uint32_t target_status;
        size_t spsr_index = 0u;
        uint8_t lane;

        if (immediate) {
            const uint32_t immediate_value = opcode & UINT32_C(0xFF);
            const uint8_t rotation =
                (uint8_t)(((opcode >> 8u) & UINT32_C(0x0F)) * 2u);

            operand = rotation == 0u
                          ? immediate_value
                          : gbl_shift(GBL_SHIFT_ROR, immediate_value,
                                      rotation, false, false)
                                .value;
        } else {
            const uint8_t source = (uint8_t)(opcode & UINT32_C(0x0F));

            operand = gbl_cpu_read_register(
                cpu, source, unit->instruction_address,
                GBL_CPU_PC_READ_NORMAL);
        }

        if ((operand & GBL_CPU_PSR_UNALLOC_MASK) != 0u) {
            unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
            return true;
        }
        for (lane = 0u; lane < 4u; ++lane) {
            if ((fields & (uint8_t)(1u << lane)) != 0u) {
                byte_mask |= UINT32_C(0xFF) << ((uint32_t)lane * 8u);
            }
        }

        if (saved) {
            if (!gbl_cpu_spsr_index(cpu->cpsr, &spsr_index)) {
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
            write_mask = byte_mask &
                         (GBL_CPU_CPSR_USER_MASK | GBL_CPU_CPSR_PRIV_MASK |
                          GBL_CPU_CPSR_STATE_MASK);
            target_status = cpu->spsr[spsr_index];
        } else if ((cpu->cpsr & GBL_CPU_CPSR_MODE_MASK) ==
                   UINT32_C(0x00000010)) {
            write_mask = byte_mask & GBL_CPU_CPSR_USER_MASK;
            target_status = cpu->cpsr;
        } else {
            if ((operand & GBL_CPU_CPSR_STATE_MASK) != 0u) {
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
            write_mask = byte_mask &
                         (GBL_CPU_CPSR_USER_MASK | GBL_CPU_CPSR_PRIV_MASK);
            target_status = cpu->cpsr;
        }

        target_status = (target_status & ~write_mask) |
                        (operand & write_mask);
        if ((write_mask & GBL_CPU_CPSR_MODE_MASK) != 0u &&
            !gbl_cpu_mode_valid(target_status)) {
            unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
            return true;
        }

        unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
        if (write_mask != 0u) {
            const bool added = saved
                                   ? gbl_cpu_add_spsr_status_effect(
                                         unit, (uint8_t)spsr_index,
                                         write_mask, operand)
                                   : gbl_cpu_add_status_effect(
                                         unit, write_mask, operand);

            if (!added) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
                return true;
            }
        }
        if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               false)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }
    return true;
}

static bool gbl_cpu_schedule_exception(gbl_cpu_unit *unit,
                                      gbl_exception_kind kind)
{
    unit->kind = GBL_CPU_UNIT_EXCEPTION;
    unit->exception_kind = kind;
    unit->internal_cycles = (uint8_t)1u;
    return gbl_cpu_refresh_cycle_cost(unit);
}

static bool gbl_cpu_finalize_decoded(gbl_cpu_unit *unit, bool decoded)
{
    if (!decoded) {
        return false;
    }
    if (unit->kind == GBL_CPU_UNIT_UNDEFINED) {
        if (!gbl_cpu_schedule_exception(unit, GBL_EXCEPTION_UNDEFINED)) {
            return false;
        }
    }
    return true;
}

static uint8_t gbl_cpu_arm_multiply_algorithm_cycles(
    uint32_t multiplier, bool signed_termination)
{
    const uint32_t top24 = multiplier & UINT32_C(0xFFFFFF00);
    const uint32_t top16 = multiplier & UINT32_C(0xFFFF0000);
    const uint32_t top8 = multiplier & UINT32_C(0xFF000000);

    if (top24 == 0u ||
        (signed_termination && top24 == UINT32_C(0xFFFFFF00))) {
        return 1u;
    }
    if (top16 == 0u ||
        (signed_termination && top16 == UINT32_C(0xFFFF0000))) {
        return 2u;
    }
    if (top8 == 0u ||
        (signed_termination && top8 == UINT32_C(0xFF000000))) {
        return 3u;
    }
    return 4u;
}

static bool gbl_cpu_decode_arm_multiply(const gbl_cpu_state *cpu,
                                        gbl_cpu_unit *unit,
                                        uint32_t next_fetch_address)
{
    const uint32_t opcode = unit->opcode;
    const bool long_multiply =
        (opcode & UINT32_C(0x0F8000F0)) == UINT32_C(0x00800090);
    const bool short_multiply =
        (opcode & UINT32_C(0x0FC000F0)) == UINT32_C(0x00000090);
    const bool signed_long_multiply = long_multiply &&
        (opcode & UINT32_C(0x00400000)) != 0u;
    const bool accumulate = (opcode & UINT32_C(0x00200000)) != 0u;
    const bool set_flags = (opcode & UINT32_C(0x00100000)) != 0u;
    const uint8_t rd_hi_or_short =
        (uint8_t)((opcode >> 16u) & UINT32_C(0x0F));
    const uint8_t rd_lo_or_accumulator =
        (uint8_t)((opcode >> 12u) & UINT32_C(0x0F));
    const uint8_t rs = (uint8_t)((opcode >> 8u) & UINT32_C(0x0F));
    const uint8_t rm = (uint8_t)(opcode & UINT32_C(0x0F));
    uint32_t rm_value;
    uint32_t rs_value;
    uint32_t short_result;
    uint32_t long_result_low;
    uint32_t long_result_high;
    uint32_t flags;
    uint32_t flag_mask;
    uint64_t product;
    uint8_t algorithm_cycles;

    if (!short_multiply && !long_multiply) {
        return false;
    }
    if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        return gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                                 false);
    }

    if (rm == (uint8_t)15u || rs == (uint8_t)15u ||
        rd_hi_or_short == (uint8_t)15u ||
        (long_multiply &&
         (rd_lo_or_accumulator == (uint8_t)15u ||
          rd_hi_or_short == rd_lo_or_accumulator)) ||
        (!long_multiply && accumulate &&
         rd_lo_or_accumulator == (uint8_t)15u) ||
        (!long_multiply && !accumulate &&
         rd_lo_or_accumulator != 0u)) {
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }

    /* Snapshot sources and accumulator words before destination effects.
     * DDI0100I A4.1.40 records historical UNPREDICTABLE classification for
     * MUL Rd == Rm and says relevant ARMv4/v5 implementations are believed
     * not to require the restriction. DDI0029G does not establish this exact
     * core's alias result; retaining deterministic MUL Rd == Rm behavior is
     * project policy, not an exact-core or silicon claim. */
    rm_value = gbl_cpu_read_register(cpu, rm, unit->instruction_address,
                                     GBL_CPU_PC_READ_NORMAL);
    rs_value = gbl_cpu_read_register(cpu, rs, unit->instruction_address,
                                     GBL_CPU_PC_READ_NORMAL);
    /* DDI0029G §6.6, Tables 6-5–6-8 and §6.20/Table 6-23 define m from
     * original Rs. Short MUL/MLA use the profile's signed termination choice
     * (DDI0100I leaves that choice implementation-defined); long forms use
     * their explicit signed/unsigned opcode. Only this internal work is
     * modeled here; bus wait states and prefetch are outside this profile. */
    algorithm_cycles = gbl_cpu_arm_multiply_algorithm_cycles(
        rs_value, !long_multiply || signed_long_multiply);
    unit->internal_cycles = (uint8_t)(algorithm_cycles +
        (long_multiply ? (accumulate ? 2u : 1u)
                       : (accumulate ? 1u : 0u)));
    if (long_multiply) {
        uint64_t result;
        uint64_t rm_bits = (uint64_t)rm_value;
        uint64_t rs_bits = (uint64_t)rs_value;

        if (signed_long_multiply) {
            /* Sign-extend into unsigned bit vectors, then multiply modulo
             * 2^64. This avoids implementation-defined uint32_t-to-int32_t
             * conversions while preserving the signed product's exact bits. */
            if ((rm_value & UINT32_C(0x80000000)) != 0u) {
                rm_bits |= UINT64_C(0xFFFFFFFF00000000);
            }
            if ((rs_value & UINT32_C(0x80000000)) != 0u) {
                rs_bits |= UINT64_C(0xFFFFFFFF00000000);
            }
        }
        product = rm_bits * rs_bits;
        if (accumulate) {
            const uint32_t accumulator_low = gbl_cpu_read_register(
                cpu, rd_lo_or_accumulator, unit->instruction_address,
                GBL_CPU_PC_READ_NORMAL);
            const uint32_t accumulator_high = gbl_cpu_read_register(
                cpu, rd_hi_or_short, unit->instruction_address,
                GBL_CPU_PC_READ_NORMAL);
            const uint64_t accumulator =
                ((uint64_t)accumulator_high << 32u) |
                (uint64_t)accumulator_low;

            result = accumulator + product;
        } else {
            result = product;
        }
        long_result_low = (uint32_t)result;
        long_result_high = (uint32_t)(result >> 32u);
        short_result = 0u;
        flags = (long_result_high & GBL_CPU_CPSR_N) != 0u
                    ? GBL_CPU_CPSR_N
                    : 0u;
        if (long_result_high == 0u && long_result_low == 0u) {
            flags |= GBL_CPU_CPSR_Z;
        }
        flag_mask = GBL_CPU_CPSR_N | GBL_CPU_CPSR_Z;
    } else {
        const uint32_t accumulator = accumulate
            ? gbl_cpu_read_register(cpu, rd_lo_or_accumulator,
                                    unit->instruction_address,
                                    GBL_CPU_PC_READ_NORMAL)
            : 0u;

        product = (uint64_t)rm_value * (uint64_t)rs_value;
        short_result = (uint32_t)product + accumulator;
        long_result_low = 0u;
        long_result_high = 0u;
        flags = gbl_cpu_nz_flags(short_result);
        /* N/Z are defined, V is defined to be preserved, and C is
         * architecturally unpredictable on ARMv4T. The profile chooses to
         * preserve incoming C; updating only N/Z states that policy directly. */
        flag_mask = GBL_CPU_CPSR_N | GBL_CPU_CPSR_Z;
    }

    unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
    if ((long_multiply &&
         (!gbl_cpu_add_literal_register_effect(
              unit, rd_lo_or_accumulator, cpu->cpsr, long_result_low) ||
          !gbl_cpu_add_literal_register_effect(
              unit, rd_hi_or_short, cpu->cpsr, long_result_high))) ||
        (!long_multiply &&
         !gbl_cpu_add_literal_register_effect(
             unit, rd_hi_or_short, cpu->cpsr, short_result)) ||
        (set_flags &&
         !gbl_cpu_add_status_effect(unit, flag_mask, flags))) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    return gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address, false);
}

static bool gbl_cpu_decode_arm_swap(const gbl_cpu_state *cpu,
                                    gbl_cpu_unit *unit,
                                    uint32_t next_fetch_address)
{
    const uint32_t opcode = unit->opcode;
    const bool byte = (opcode & UINT32_C(0x00400000)) != 0u;
    const uint8_t base_register =
        (uint8_t)((opcode >> 16u) & UINT32_C(0x0F));
    const uint8_t destination =
        (uint8_t)((opcode >> 12u) & UINT32_C(0x0F));
    const uint8_t source = (uint8_t)(opcode & UINT32_C(0x0F));
    uint32_t address;
    uint32_t source_value;
    uint8_t result_index;
    uint8_t width;

    if ((opcode & UINT32_C(0x0FB00FF0)) != UINT32_C(0x01000090)) {
        return false;
    }
    if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               false)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }
    if (base_register == (uint8_t)15u ||
        destination == (uint8_t)15u || source == (uint8_t)15u) {
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }

    width = byte ? 1u : (uint8_t)GBL_CPU_WORD_WIDTH;
    address = gbl_cpu_read_register(cpu, base_register,
                                    unit->instruction_address,
                                    GBL_CPU_PC_READ_NORMAL);
    source_value = gbl_cpu_read_register(cpu, source,
                                         unit->instruction_address,
                                         GBL_CPU_PC_READ_NORMAL);
    if (!gbl_cpu_data_span_valid(address, width)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    unit->kind = GBL_CPU_UNIT_TRANSFER;
    if (!gbl_cpu_add_data_read_slot(
            unit, byte ? address : address & ~UINT32_C(3), width,
            byte ? 0u : (uint8_t)(address & UINT32_C(3)), !byte,
            &result_index) ||
        !gbl_cpu_add_data_write_slot(
            unit, byte ? address : address & ~UINT32_C(3), width,
            source_value) ||
        !gbl_cpu_add_result_register_effect(unit, destination, cpu->cpsr,
                                            result_index) ||
        !gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address, false)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
    }
    return true;
}

static bool gbl_cpu_decode_arm_single_transfer(const gbl_cpu_state *cpu,
                                               gbl_cpu_unit *unit,
                                               uint32_t next_fetch_address)
{
    const uint32_t opcode = unit->opcode;
    const bool register_offset = (opcode & UINT32_C(0x02000000)) != 0u;
    const bool preindexed = (opcode & UINT32_C(0x01000000)) != 0u;
    const bool add_offset = (opcode & UINT32_C(0x00800000)) != 0u;
    const bool byte = (opcode & UINT32_C(0x00400000)) != 0u;
    const bool writeback_bit = (opcode & UINT32_C(0x00200000)) != 0u;
    const bool load = (opcode & UINT32_C(0x00100000)) != 0u;
    const uint8_t base_register =
        (uint8_t)((opcode >> 16u) & UINT32_C(0x0F));
    const uint8_t data_register =
        (uint8_t)((opcode >> 12u) & UINT32_C(0x0F));
    const uint8_t width = byte ? 1u : (uint8_t)GBL_CPU_WORD_WIDTH;
    const bool load_pc = load && !byte && data_register == (uint8_t)15u;
    /* Post-indexed forms always update Rn. W distinguishes the T encoding,
     * whose forced-User bus access has no extra effect under this flat bus. */
    const bool writeback = !preindexed || writeback_bit;
    uint32_t base;
    uint32_t offset;
    uint32_t address;
    uint32_t updated_base;
    uint32_t store_value = 0u;
    uint8_t pc_result_index = UINT8_MAX;

    if ((opcode & UINT32_C(0x0C000000)) != UINT32_C(0x04000000)) {
        return false;
    }
    if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               false)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    /* DDI0100I A4.1.99/A4.1.100: STR/STRB writeback with Rd == Rn is
     * unpredictable, just as the load writeback alias is. */
    if ((register_offset && (opcode & UINT32_C(0x00000010)) != 0u) ||
        (base_register == (uint8_t)15u &&
         (!preindexed || writeback)) ||
        (register_offset &&
         ((opcode & UINT32_C(0x0000000F)) == UINT32_C(15) ||
          ((uint8_t)(opcode & UINT32_C(0x0000000F)) == base_register &&
           writeback))) ||
        (writeback && data_register == base_register) ||
        (load_pc && !preindexed && writeback_bit) ||
        (byte && data_register == (uint8_t)15u)) {
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    base = gbl_cpu_read_register(cpu, base_register,
                                 unit->instruction_address,
                                 GBL_CPU_PC_READ_NORMAL);
    if (register_offset) {
        const uint8_t source_register =
            (uint8_t)(opcode & UINT32_C(0x0000000F));
        const uint8_t shift_amount =
            (uint8_t)((opcode >> 7u) & UINT32_C(0x1F));
        const gbl_shift_kind shift_kind =
            (gbl_shift_kind)((opcode >> 5u) & UINT32_C(0x03));
        const uint32_t source_value = gbl_cpu_read_register(
            cpu, source_register, unit->instruction_address,
            GBL_CPU_PC_READ_NORMAL);
        const gbl_shift_result shifted = gbl_shift(
            shift_kind, source_value, shift_amount,
            (cpu->cpsr & GBL_CPU_CPSR_C) != 0u, false);

        offset = shifted.value;
    } else {
        offset = opcode & UINT32_C(0x00000FFF);
    }
    updated_base = add_offset ? base + offset : base - offset;
    address = preindexed ? updated_base : base;
    if (load_pc && (address & UINT32_C(3)) != 0u) {
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    if (!load) {
        store_value = gbl_cpu_read_register(
            cpu, data_register, unit->instruction_address,
            GBL_CPU_PC_READ_STORE_SOURCE);
    }
    if (!gbl_cpu_data_span_valid(
            width == (uint8_t)GBL_CPU_WORD_WIDTH
                ? address & ~UINT32_C(3)
                : address,
            width)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if (load_pc) {
        if (!gbl_cpu_add_data_read_slot(
                unit, address & ~UINT32_C(3), width,
                (uint8_t)(address & UINT32_C(3)), true,
                &pc_result_index)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            return true;
        }
    } else if (!gbl_cpu_prepare_data_transfer(
                   unit, address, width, load, data_register, store_value,
                   load && !byte, cpu->cpsr)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if (writeback &&
        !gbl_cpu_add_literal_register_effect(
            unit, base_register, cpu->cpsr, updated_base)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if ((load_pc &&
         !gbl_cpu_schedule_result_refill(unit, pc_result_index, false)) ||
        (!load_pc &&
         !gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                            false))) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
    }
    return true;
}

static bool gbl_cpu_decode_arm_subword_transfer(
    const gbl_cpu_state *cpu, gbl_cpu_unit *unit,
    uint32_t next_fetch_address)
{
    const uint32_t opcode = unit->opcode;
    const bool sign = (opcode & UINT32_C(0x00000040)) != 0u;
    const bool halfword = (opcode & UINT32_C(0x00000020)) != 0u;
    const bool register_offset = (opcode & UINT32_C(0x00400000)) == 0u;
    const bool preindexed = (opcode & UINT32_C(0x01000000)) != 0u;
    const bool add_offset = (opcode & UINT32_C(0x00800000)) != 0u;
    const bool writeback_bit = (opcode & UINT32_C(0x00200000)) != 0u;
    const bool load = (opcode & UINT32_C(0x00100000)) != 0u;
    const bool writeback = !preindexed || writeback_bit;
    const uint8_t base_register =
        (uint8_t)((opcode >> 16u) & UINT32_C(0x0F));
    const uint8_t data_register =
        (uint8_t)((opcode >> 12u) & UINT32_C(0x0F));
    const uint8_t offset_register = (uint8_t)(opcode & UINT32_C(0x0F));
    uint32_t base;
    uint32_t offset;
    uint32_t address;
    uint32_t updated_base;
    uint32_t source_value = 0u;
    uint8_t result_index = UINT8_MAX;
    uint8_t width;
    uint32_t bus_address;
    bool rotate_halfword_read = false;
    bool sign_extend_byte_read = false;
    bool sign_extend_halfword_read = false;

    if ((opcode & UINT32_C(0x0E000090)) != UINT32_C(0x00000090) ||
        (!sign && !halfword)) {
        return false;
    }
    if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               false)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    /* DDI0100I A5.3.1 assigns S=1,L=0 to the later LDRD/STRD
     * extension space; ARM7TDMI DDI0029G does not implement those forms. */
    if (sign && !load) {
        unit->kind = GBL_CPU_UNIT_LATER_ARCHITECTURE;
        return true;
    }
    /* Register-offset mode reserves bits[11:8], and Mode 3 requires W=0
     * for post-index addressing. Treat malformed/unpredictable operands as
     * deterministic no-effect stops before creating bus or register slots. */
    if ((register_offset &&
         (opcode & UINT32_C(0x00000F00)) != 0u) ||
        (!preindexed && writeback_bit) || data_register == (uint8_t)15u ||
        (base_register == (uint8_t)15u &&
         (!preindexed || writeback)) ||
        (register_offset && offset_register == (uint8_t)15u) ||
        (writeback && data_register == base_register) ||
        (register_offset && writeback &&
         offset_register == base_register)) {
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }

    base = gbl_cpu_read_register(cpu, base_register,
                                 unit->instruction_address,
                                 GBL_CPU_PC_READ_NORMAL);
    if (register_offset) {
        offset = gbl_cpu_read_register(cpu, offset_register,
                                       unit->instruction_address,
                                       GBL_CPU_PC_READ_NORMAL);
    } else {
        offset = (((opcode >> 4u) & UINT32_C(0xF0)) |
                  (opcode & UINT32_C(0x0F)));
    }
    updated_base = add_offset ? base + offset : base - offset;
    address = preindexed ? updated_base : base;

    if (!load) {
        source_value = gbl_cpu_read_register(
            cpu, data_register, unit->instruction_address,
            GBL_CPU_PC_READ_STORE_SOURCE);
        width = (uint8_t)GBL_CPU_HALF_WIDTH;
        bus_address = address & ~UINT32_C(1);
    } else if (sign && !halfword) {
        width = 1u;
        bus_address = address;
        sign_extend_byte_read = true;
    } else if (sign && halfword && (address & UINT32_C(1)) != 0u) {
        /* GBATEK 3.06, ARM CPU Memory Alignments, ARM7/GBA row: odd
         * LDRSH is the signed-byte operation at the original address. */
        width = 1u;
        bus_address = address;
        sign_extend_byte_read = true;
    } else {
        width = (uint8_t)GBL_CPU_HALF_WIDTH;
        bus_address = address & ~UINT32_C(1);
        if (!sign && (address & UINT32_C(1)) != 0u) {
            /* GBATEK 3.06 reports the ARM7/GBA odd LDRH result as the
             * aligned halfword read rotated right by eight in a word. */
            rotate_halfword_read = true;
        } else if (sign) {
            sign_extend_halfword_read = true;
        }
    }
    if (!gbl_cpu_data_span_valid(bus_address, width)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }

    unit->kind = GBL_CPU_UNIT_TRANSFER;
    if (load) {
        if (!gbl_cpu_add_subword_read_slot(
                unit, bus_address, width, (uint8_t)(address & UINT32_C(1)),
                rotate_halfword_read, sign_extend_byte_read,
                sign_extend_halfword_read, &result_index) ||
            !gbl_cpu_add_result_register_effect(
                unit, data_register, cpu->cpsr, result_index)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            return true;
        }
    } else if (!gbl_cpu_add_data_write_slot(
                   unit, bus_address, width,
                   source_value & UINT32_C(0x0000FFFF))) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if (writeback &&
        !gbl_cpu_add_literal_register_effect(
            unit, base_register, cpu->cpsr, updated_base)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address, false)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
    }
    return true;
}

/* Thumb formats 1, 2 and 4. DDI0100I A7 specifies their flag masks;
 * DDI0029G §6.5/6.6 supplies register-shift and multiply internal work.
 * All source values are captured before effects enter the canonical unit. */
static bool gbl_cpu_decode_thumb_alu(const gbl_cpu_state *cpu,
                                     gbl_cpu_unit *unit,
                                     uint32_t next_fetch_address)
{
    const uint16_t opcode = (uint16_t)unit->opcode;
    const uint8_t rd = (uint8_t)(opcode & 7u);
    const uint8_t rs = (uint8_t)((opcode >> 3u) & 7u);
    uint32_t left = cpu->r0_7[rd];
    const uint32_t right = cpu->r0_7[rs];
    uint32_t mask = GBL_CPU_CPSR_N | GBL_CPU_CPSR_Z;
    uint32_t flags;
    bool write = true;
    bool arithmetic_flags = false;
    gbl_arithmetic_result arithmetic = { 0u, false, false };
    gbl_shift_result shifted = { 0u, false };

    if ((opcode & UINT16_C(0xF800)) == UINT16_C(0x1800)) {
        const bool immediate = (opcode & UINT16_C(0x0400)) != 0u;
        const uint8_t operand = (uint8_t)((opcode >> 6u) & 7u);
        const uint32_t value = immediate ? (uint32_t)operand
                                         : cpu->r0_7[operand];

        left = right;
        arithmetic = (opcode & UINT16_C(0x0200)) != 0u
                         ? gbl_sub_flags(left, value)
                         : gbl_add_flags(left, value);
        arithmetic_flags = true;
    } else if ((opcode & UINT16_C(0xE000)) == 0u) {
        shifted = gbl_shift((gbl_shift_kind)((opcode >> 11u) & 3u),
                            right, (uint8_t)((opcode >> 6u) & 31u),
                            (cpu->cpsr & GBL_CPU_CPSR_C) != 0u, false);
        unit->value = shifted.value;
        mask |= GBL_CPU_CPSR_C;
    } else {
        const uint8_t operation = (uint8_t)((opcode >> 6u) & 15u);

        switch (operation) {
        case 0u: unit->value = left & right; break; /* AND */
        case 1u: unit->value = left ^ right; break; /* EOR */
        case 2u: /* LSL */
        case 3u: /* LSR */
        case 4u: /* ASR */
        case 7u: /* ROR */
            shifted = gbl_shift(
                operation == 7u ? GBL_SHIFT_ROR
                                 : (gbl_shift_kind)(operation - 2u),
                left, (uint8_t)(right & UINT32_C(0xFF)),
                (cpu->cpsr & GBL_CPU_CPSR_C) != 0u, true);
            unit->value = shifted.value;
            mask |= GBL_CPU_CPSR_C;
            unit->internal_cycles = 1u;
            break;
        case 5u: /* ADC */
            arithmetic = gbl_add_with_carry_flags(
                left, right, (cpu->cpsr & GBL_CPU_CPSR_C) != 0u);
            arithmetic_flags = true;
            break;
        case 6u: /* SBC */
            arithmetic = gbl_sub_with_carry_flags(
                left, right, (cpu->cpsr & GBL_CPU_CPSR_C) != 0u);
            arithmetic_flags = true;
            break;
        case 8u: unit->value = left & right; write = false; break; /* TST */
        case 9u: /* NEG */
            arithmetic = gbl_sub_flags(0u, right);
            arithmetic_flags = true;
            break;
        case 10u: /* CMP */
            arithmetic = gbl_sub_flags(left, right);
            arithmetic_flags = true;
            write = false;
            break;
        case 11u: /* CMN */
            arithmetic = gbl_add_flags(left, right);
            arithmetic_flags = true;
            write = false;
            break;
        case 12u: unit->value = left | right; break; /* ORR */
        case 13u: /* MUL: DDI0100I A7.1.45 uses original Rd for termination. */
            if (rd == rs) { /* Identical operands are unpredictable before v6. */
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
            unit->value = left * right;
            /* Signed termination and preserved unpredictable C follow the
             * existing short-multiply project policy. */
            unit->internal_cycles =
                gbl_cpu_arm_multiply_algorithm_cycles(left, true);
            break;
        case 14u: unit->value = left & ~right; break; /* BIC */
        case 15u: unit->value = ~right; break; /* MVN */
        default: return false;
        }
    }
    if (arithmetic_flags) {
        unit->value = arithmetic.value;
        mask |= GBL_CPU_CPSR_C | GBL_CPU_CPSR_V;
    }
    flags = gbl_cpu_nz_flags(unit->value);
    if (arithmetic_flags ? arithmetic.carry : shifted.carry) {
        flags |= GBL_CPU_CPSR_C;
    }
    if (arithmetic_flags && arithmetic.overflow) {
        flags |= GBL_CPU_CPSR_V;
    }
    unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
    return (!write || gbl_cpu_add_literal_register_effect(
                          unit, rd, cpu->cpsr, unit->value)) &&
           gbl_cpu_add_status_effect(unit, mask, flags) &&
           gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address, true);
}

/* DDI0100I A6.6 and A7.1.27/.49/.50/.57 define two distinct Thumb transfer
 * formats. Empty lists are UNPREDICTABLE. LDMIA suppresses writeback when Rn
 * is loaded, so the actual memory result wins; STMIA only defines an in-list
 * base when it is the lowest listed register. Keep these rules separate from
 * ARM block transfers. */
static bool gbl_cpu_prepare_thumb_multiple_transfer(
    const gbl_cpu_state *cpu, gbl_cpu_unit *unit,
    uint32_t next_fetch_address, uint8_t base_register,
    uint8_t register_list, bool load, bool stack_mode, bool extra_register)
{
    uint8_t transfer_count = 0u;
    uint8_t first_register = UINT8_MAX;
    uint8_t register_index;
    uint8_t pc_result_index = UINT8_MAX;
    bool base_in_list;
    uint32_t base;
    uint32_t address;
    uint32_t updated_base;
    uint32_t span;

    base_in_list = !stack_mode &&
        (register_list & (uint8_t)(UINT8_C(1) << base_register)) != 0u;
    for (register_index = 0u; register_index < 8u; ++register_index) {
        if ((register_list & (uint8_t)(UINT8_C(1) << register_index)) != 0u) {
            if (first_register == UINT8_MAX) {
                first_register = register_index;
            }
            transfer_count++;
        }
    }
    if (extra_register) {
        transfer_count++;
        if (first_register == UINT8_MAX) {
            first_register = load ? (uint8_t)15u : (uint8_t)14u;
        }
    }
    if (transfer_count == 0u) {
        /* DDI0100I A7.1.27/.49/.50/.57 specifies non-empty lists. */
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    if (!stack_mode && !load && base_in_list &&
        base_register != first_register) {
        /* DDI0100I A7.1.57 makes only the stored base value unpredictable;
         * Gamebloke's deterministic policy stops this whole unit pre-effect. */
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }

    base = gbl_cpu_read_register(cpu, base_register,
                                 unit->instruction_address,
                                 GBL_CPU_PC_READ_NORMAL);
    span = (uint32_t)transfer_count * UINT32_C(4);
    address = stack_mode && !load ? base - span : base;
    updated_base = stack_mode && !load ? base - span : base + span;
    unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
    unit->multiple_transfer = true;

    for (register_index = 0u; register_index < 8u; ++register_index) {
        const uint8_t register_bit =
            (uint8_t)(UINT8_C(1) << register_index);
        const uint32_t bus_address = address & ~UINT32_C(3);

        if ((register_list & register_bit) != 0u) {
            if (!gbl_cpu_data_span_valid(
                    bus_address, (uint8_t)GBL_CPU_WORD_WIDTH)) {
                return false;
            }
            if (load) {
                uint8_t result_index;

                if (!gbl_cpu_add_data_read_slot(
                        unit, bus_address, (uint8_t)GBL_CPU_WORD_WIDTH, 0u,
                        false, &result_index) ||
                    !gbl_cpu_add_result_register_effect(
                        unit, register_index, cpu->cpsr, result_index)) {
                    return false;
                }
            } else if (!gbl_cpu_add_data_write_slot(
                           unit, bus_address, (uint8_t)GBL_CPU_WORD_WIDTH,
                           gbl_cpu_read_register_in_bank(
                               cpu, register_index, cpu->cpsr,
                               unit->instruction_address,
                               GBL_CPU_PC_READ_NORMAL))) {
                return false;
            }
            address += UINT32_C(4);
        }
    }
    if (extra_register) {
        const uint8_t extra_index = load ? (uint8_t)15u : (uint8_t)14u;
        const uint32_t bus_address = address & ~UINT32_C(3);

        if (!gbl_cpu_data_span_valid(
                bus_address, (uint8_t)GBL_CPU_WORD_WIDTH)) {
            return false;
        }
        if (load) {
            if (!gbl_cpu_add_data_read_slot(
                    unit, bus_address, (uint8_t)GBL_CPU_WORD_WIDTH, 0u,
                    false, &pc_result_index)) {
                return false;
            }
        } else if (!gbl_cpu_add_data_write_slot(
                       unit, bus_address, (uint8_t)GBL_CPU_WORD_WIDTH,
                       gbl_cpu_read_register_in_bank(
                           cpu, extra_index, cpu->cpsr,
                           unit->instruction_address,
                           GBL_CPU_PC_READ_NORMAL))) {
            return false;
        }
    }

    for (register_index = 0u; register_index < unit->data_slot_count;
         ++register_index) {
        unit->data_slots[register_index].access_class =
            register_index == 0u ? GBL_BUS_CLASS_NONSEQUENTIAL
                                 : GBL_BUS_CLASS_SEQUENTIAL;
    }
    unit->post_data_internal_cycles = load ? (uint8_t)1u : 0u;
    unit->post_data_pipeline_cycles =
        load && pc_result_index != UINT8_MAX ? (uint8_t)1u : 0u;

    if ((stack_mode || !load || !base_in_list) &&
        !gbl_cpu_add_literal_register_effect(
            unit, base_register, cpu->cpsr, updated_base)) {
        return false;
    }
    if ((load && pc_result_index != UINT8_MAX &&
         !gbl_cpu_schedule_result_refill(unit, pc_result_index, true)) ||
        (!(load && pc_result_index != UINT8_MAX) &&
         !gbl_cpu_schedule_sequential_fetch(
             unit, next_fetch_address, true))) {
        return false;
    }
    if (!load) {
        /* The transfer stores precede the following fetch in this model. */
        unit->fetch_class[0] = GBL_BUS_CLASS_NONSEQUENTIAL;
    }
    return true;
}

static bool gbl_cpu_decode_thumb(const gbl_cpu_state *cpu,
                                 gbl_cpu_unit *unit,
                                 uint32_t next_fetch_address)
{
    const uint16_t opcode = (uint16_t)unit->opcode;
    const uint32_t instruction_address = unit->instruction_address;

    /* ARMv5 BLX/BKPT encodings are not ARM7TDMI operations. */
    if ((opcode & UINT16_C(0xFF00)) == UINT16_C(0xBE00) ||
        (opcode & UINT16_C(0xFF87)) == UINT16_C(0x4780) ||
        (opcode & UINT16_C(0xF800)) == UINT16_C(0xE800)) {
        unit->kind = GBL_CPU_UNIT_LATER_ARCHITECTURE;
        return true;
    }

    if ((opcode & UINT16_C(0xE000)) == 0u ||
        (opcode & UINT16_C(0xFC00)) == UINT16_C(0x4000)) {
        return gbl_cpu_decode_thumb_alu(cpu, unit, next_fetch_address);
    }

    if ((opcode & UINT16_C(0xFF00)) == UINT16_C(0xDF00)) {
        return gbl_cpu_schedule_exception(unit, GBL_EXCEPTION_SWI);
    }

    /* DDI0100I A7.1.17: BL supplies high and low offsets in two
     * halfwords. Gamebloke retires each independently and uses only banked
     * LR between them, including isolated-half probes. Unsigned offset
     * arithmetic is the project's deterministic policy at unpredictable
     * address-wrap boundaries; existing static refill guards still apply. */
    if ((opcode & UINT16_C(0xF000)) == UINT16_C(0xF000)) {
        uint32_t offset = opcode & UINT16_C(0x07FF);
        uint32_t link;

        if ((opcode & UINT16_C(0x0800)) == 0u) {
            if ((offset & UINT32_C(0x400)) != 0u) {
                offset |= UINT32_C(0xFFFFF800);
            }
            link = instruction_address + UINT32_C(4) + (offset << 12u);
            unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
            return gbl_cpu_add_literal_register_effect(
                       unit, (uint8_t)14u, cpu->cpsr, link) &&
                   gbl_cpu_schedule_sequential_fetch(
                       unit, next_fetch_address, true);
        }
        {
            const uint32_t old_lr = gbl_cpu_read_register(
                cpu, (uint8_t)14u, instruction_address, GBL_CPU_PC_READ_NORMAL);
            const uint32_t target = (old_lr + (offset << 1u)) & ~UINT32_C(1);

            link = (instruction_address + UINT32_C(2)) | UINT32_C(1);
            if (!gbl_cpu_add_literal_register_effect(
                    unit, (uint8_t)14u, cpu->cpsr, link)) {
                return false;
            }
            return gbl_cpu_schedule_branch(unit, target, true);
        }
    }

    if ((opcode & UINT16_C(0xF800)) == UINT16_C(0x2000) ||
        (opcode & UINT16_C(0xF800)) == UINT16_C(0x3000) ||
        (opcode & UINT16_C(0xF800)) == UINT16_C(0x3800) ||
        (opcode & UINT16_C(0xF800)) == UINT16_C(0x2800)) {
        const uint8_t operation = (uint8_t)((opcode >> 11u) & 3u);
        const uint8_t destination = (uint8_t)((opcode >> 8u) & 7u);
        const uint32_t operand = opcode & UINT16_C(0x00FF);
        uint32_t flags;
        uint32_t flag_mask;
        bool writes_destination = false;

        if (operation == 0u) {
            unit->value = operand;
            flags = gbl_cpu_nz_flags(unit->value);
            flag_mask = GBL_CPU_CPSR_N | GBL_CPU_CPSR_Z;
            writes_destination = true;
        } else if (operation == 1u) {
            const gbl_arithmetic_result arithmetic = gbl_sub_flags(
                cpu->r0_7[destination], operand);

            unit->value = arithmetic.value;
            flags = gbl_cpu_nz_flags(unit->value);
            if (arithmetic.carry) {
                flags |= GBL_CPU_CPSR_C;
            }
            if (arithmetic.overflow) {
                flags |= GBL_CPU_CPSR_V;
            }
            flag_mask = GBL_CPU_CPSR_N | GBL_CPU_CPSR_Z |
                        GBL_CPU_CPSR_C | GBL_CPU_CPSR_V;
        } else {
            const bool subtract = operation == 3u;
            const gbl_arithmetic_result arithmetic = subtract
                ? gbl_sub_flags(cpu->r0_7[destination], operand)
                : gbl_add_flags(cpu->r0_7[destination], operand);

            unit->value = arithmetic.value;
            flags = gbl_cpu_nz_flags(unit->value);
            if (arithmetic.carry) {
                flags |= GBL_CPU_CPSR_C;
            }
            if (arithmetic.overflow) {
                flags |= GBL_CPU_CPSR_V;
            }
            flag_mask = GBL_CPU_CPSR_N | GBL_CPU_CPSR_Z |
                        GBL_CPU_CPSR_C | GBL_CPU_CPSR_V;
            writes_destination = true;
        }
        unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
        if ((writes_destination &&
             !gbl_cpu_add_literal_register_effect(
                 unit, destination, cpu->cpsr, unit->value)) ||
            !gbl_cpu_add_status_effect(unit, flag_mask, flags)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            return true;
        }
        if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               true)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    if ((opcode & UINT16_C(0xFC00)) == UINT16_C(0x4400)) {
        const uint8_t operation = (uint8_t)((opcode >> 8u) & 3u);
        const uint8_t source = (uint8_t)((opcode >> 3u) & 15u);
        const uint8_t destination =
            (uint8_t)((opcode & 7u) | ((opcode >> 4u) & 8u));

        if (operation != 3u) {
            const uint32_t right = gbl_cpu_read_register(
                cpu, source, instruction_address, GBL_CPU_PC_READ_NORMAL);
            const uint32_t left = gbl_cpu_read_register(
                cpu, destination, instruction_address, GBL_CPU_PC_READ_NORMAL);

            /* DDI0100I A7.1.6/.23/.44: low/low high forms are
             * unpredictable on ARMv4T, as is CMP with R15 as Rn.
             * In particular do not import ARMv6 CPY into this decoder. */
            if ((source < 8u && destination < 8u) ||
                (operation == 1u && destination == 15u)) {
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
            unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
            if (operation == 1u) {
                const gbl_arithmetic_result arithmetic = gbl_sub_flags(left, right);
                const uint32_t flags = gbl_cpu_nz_flags(arithmetic.value) |
                    (arithmetic.carry ? GBL_CPU_CPSR_C : 0u) |
                    (arithmetic.overflow ? GBL_CPU_CPSR_V : 0u);

                if (!gbl_cpu_add_status_effect(unit,
                        GBL_CPU_CPSR_N | GBL_CPU_CPSR_Z |
                        GBL_CPU_CPSR_C | GBL_CPU_CPSR_V, flags)) {
                    return false;
                }
            } else {
                const uint32_t value = operation == 0u ? left + right : right;

                if (destination == 15u) {
                    /* ADD/MOV retain T even for an even result. Only BX
                     * interworks; existing static refill guards still apply. */
                    return gbl_cpu_schedule_pc_write(unit, value, cpu->cpsr, false);
                }
                if (!gbl_cpu_add_literal_register_effect(
                        unit, destination, cpu->cpsr, value)) {
                    return false;
                }
            }
            return gbl_cpu_schedule_sequential_fetch(unit,
                                                      next_fetch_address, true);
        }
        if (operation == 3u) {
            uint32_t target_value;
            uint32_t target;
            bool thumb;

            if ((opcode & UINT16_C(0x0087)) != 0u) {
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
            target_value = gbl_cpu_read_register(
                cpu, source, instruction_address, GBL_CPU_PC_READ_NORMAL);
            thumb = (target_value & UINT32_C(1)) != 0u;
            target = target_value & ~UINT32_C(1);
            if (!thumb && (target & UINT32_C(2)) != 0u) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
                return true;
            }
            return gbl_cpu_schedule_branch(unit, target, thumb);
        }
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }

    if ((opcode & UINT16_C(0xF000)) == UINT16_C(0xD000)) {
        const uint8_t condition = (uint8_t)((opcode >> 8u) & 15u);

        if (condition == 14u) {
            unit->kind = GBL_CPU_UNIT_UNDEFINED;
            return true;
        }
        if (condition == 15u) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            return true;
        }
        if (!gbl_cpu_condition_passed(condition, cpu->cpsr)) {
            unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
            if (!gbl_cpu_schedule_sequential_fetch(unit,
                                                   next_fetch_address, true)) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            }
            return true;
        }
        {
            uint32_t offset = opcode & UINT16_C(0x00FF);

            if ((offset & UINT32_C(0x80)) != 0u) {
                offset |= UINT32_C(0xFFFFFF00);
            }
            offset <<= 1u;
            return gbl_cpu_schedule_branch(
                unit, instruction_address + UINT32_C(4) + offset, true);
        }
    }

    if ((opcode & UINT16_C(0xF800)) == UINT16_C(0xE000)) {
        uint32_t offset = opcode & UINT16_C(0x07FF);

        if ((offset & UINT32_C(0x400)) != 0u) {
            offset |= UINT32_C(0xFFFFF800);
        }
        offset <<= 1u;
        return gbl_cpu_schedule_branch(
            unit, instruction_address + UINT32_C(4) + offset, true);
    }

    if ((opcode & UINT16_C(0xF800)) == UINT16_C(0x4800)) {
        const uint8_t destination = (uint8_t)((opcode >> 8u) & 7u);
        const uint32_t base = gbl_cpu_read_register(
            cpu, (uint8_t)15u, instruction_address,
            GBL_CPU_PC_READ_THUMB_ALIGNED_ADDRESS);
        const uint32_t address =
            base + ((uint32_t)(opcode & UINT16_C(0x00FF)) << 2u);

        if (!gbl_cpu_prepare_thumb_data_transfer(
                unit, address, (uint8_t)GBL_CPU_WORD_WIDTH, true,
                destination, 0u, false, false, cpu->cpsr,
                next_fetch_address)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    if ((opcode & UINT16_C(0xF000)) == UINT16_C(0x5000)) {
        const uint8_t operation = (uint8_t)((opcode >> 9u) & 7u);
        const uint8_t offset_register = (uint8_t)((opcode >> 6u) & 7u);
        const uint8_t base_register = (uint8_t)((opcode >> 3u) & 7u);
        const uint8_t data_register = (uint8_t)(opcode & 7u);
        const bool load = operation >= 3u;
        const bool signed_byte = operation == 3u;
        const bool signed_halfword = operation == 7u;
        const uint8_t width = (operation == 0u || operation == 4u)
            ? (uint8_t)GBL_CPU_WORD_WIDTH
            : (operation == 1u || operation == 5u || operation == 7u)
                ? (uint8_t)GBL_CPU_HALF_WIDTH
                : 1u;
        /* Snapshot both address operands and the store source before the
         * result/effect records can commit any low-register destination. */
        const uint32_t base = cpu->r0_7[base_register];
        const uint32_t offset = cpu->r0_7[offset_register];
        const uint32_t store_value = cpu->r0_7[data_register];

        if (!gbl_cpu_prepare_thumb_data_transfer(
                unit, base + offset, width, load, data_register,
                store_value, signed_byte, signed_halfword, cpu->cpsr,
                next_fetch_address)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    if ((opcode & UINT16_C(0xE000)) == UINT16_C(0x6000)) {
        const bool byte = (opcode & UINT16_C(0x1000)) != 0u;
        const bool load = (opcode & UINT16_C(0x0800)) != 0u;
        const uint8_t destination = (uint8_t)(opcode & 7u);
        const uint8_t base_register = (uint8_t)((opcode >> 3u) & 7u);
        const uint8_t width = byte ? 1u : (uint8_t)GBL_CPU_WORD_WIDTH;
        uint32_t offset = (uint32_t)((opcode >> 6u) & UINT16_C(0x001F));
        uint32_t address;

        if (!byte) {
            offset <<= 2u;
        }
        address = cpu->r0_7[base_register] + offset;
        if (!gbl_cpu_prepare_thumb_data_transfer(
                unit, address, width, load, destination,
                load ? 0u : cpu->r0_7[destination], false, false,
                cpu->cpsr, next_fetch_address)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    if ((opcode & UINT16_C(0xF000)) == UINT16_C(0x8000)) {
        const bool load = (opcode & UINT16_C(0x0800)) != 0u;
        const uint8_t destination = (uint8_t)(opcode & 7u);
        const uint8_t base_register = (uint8_t)((opcode >> 3u) & 7u);
        const uint32_t offset =
            (uint32_t)((opcode >> 6u) & UINT16_C(0x001F)) << 1u;
        const uint32_t base = cpu->r0_7[base_register];
        const uint32_t store_value = cpu->r0_7[destination];

        if (!gbl_cpu_prepare_thumb_data_transfer(
                unit, base + offset, (uint8_t)GBL_CPU_HALF_WIDTH, load,
                destination, store_value, false, false, cpu->cpsr,
                next_fetch_address)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    if ((opcode & UINT16_C(0xF000)) == UINT16_C(0x9000)) {
        const bool load = (opcode & UINT16_C(0x0800)) != 0u;
        const uint8_t destination = (uint8_t)((opcode >> 8u) & 7u);
        const uint8_t base_register = (uint8_t)13u;
        const uint32_t offset =
            (uint32_t)(opcode & UINT16_C(0x00FF)) << 2u;
        const uint32_t base = gbl_cpu_read_register(
            cpu, base_register, instruction_address, GBL_CPU_PC_READ_NORMAL);
        const uint32_t store_value = cpu->r0_7[destination];

        if (!gbl_cpu_prepare_thumb_data_transfer(
                unit, base + offset, (uint8_t)GBL_CPU_WORD_WIDTH, load,
                destination, store_value, false, false, cpu->cpsr,
                next_fetch_address)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    if ((opcode & UINT16_C(0xF000)) == UINT16_C(0xA000)) {
        const bool use_sp = (opcode & UINT16_C(0x0800)) != 0u;
        const uint8_t destination = (uint8_t)((opcode >> 8u) & 7u);
        const uint8_t base_register = use_sp ? (uint8_t)13u : (uint8_t)15u;
        const uint32_t offset =
            (uint32_t)(opcode & UINT16_C(0x00FF)) << 2u;
        const gbl_cpu_pc_read_context pc_context = use_sp
            ? GBL_CPU_PC_READ_NORMAL
            : GBL_CPU_PC_READ_THUMB_ALIGNED_ADDRESS;
        const uint32_t base = gbl_cpu_read_register(
            cpu, base_register, instruction_address, pc_context);

        unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
        if (!gbl_cpu_add_literal_register_effect(
                unit, destination, cpu->cpsr, base + offset) ||
            !gbl_cpu_schedule_sequential_fetch(
                unit, next_fetch_address, true)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    if ((opcode & UINT16_C(0xFF00)) == UINT16_C(0xB000)) {
        const bool subtract = (opcode & UINT16_C(0x0080)) != 0u;
        const uint32_t offset =
            (uint32_t)(opcode & UINT16_C(0x007F)) << 2u;
        const uint32_t base = gbl_cpu_read_register(
            cpu, (uint8_t)13u, instruction_address,
            GBL_CPU_PC_READ_NORMAL);
        const uint32_t result = subtract ? base - offset : base + offset;

        unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
        if (!gbl_cpu_add_literal_register_effect(
                unit, (uint8_t)13u, cpu->cpsr, result) ||
            !gbl_cpu_schedule_sequential_fetch(
                unit, next_fetch_address, true)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    if ((opcode & UINT16_C(0xF600)) == UINT16_C(0xB400)) {
        const bool load = (opcode & UINT16_C(0x0800)) != 0u;
        const bool extra_register = (opcode & UINT16_C(0x0100)) != 0u;
        const uint8_t register_list = (uint8_t)opcode;

        if (!gbl_cpu_prepare_thumb_multiple_transfer(
                cpu, unit, next_fetch_address, (uint8_t)13u, register_list,
                load, true, extra_register)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    if ((opcode & UINT16_C(0xF000)) == UINT16_C(0xC000)) {
        const bool load = (opcode & UINT16_C(0x0800)) != 0u;
        const uint8_t base_register = (uint8_t)((opcode >> 8u) & 7u);
        const uint8_t register_list = (uint8_t)opcode;

        if (!gbl_cpu_prepare_thumb_multiple_transfer(
                cpu, unit, next_fetch_address, base_register, register_list,
                load, false, false)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    unit->kind = GBL_CPU_UNIT_UNDEFINED;
    return true;
}

static bool gbl_cpu_decode_arm_block_transfer(
    const gbl_cpu_state *cpu, gbl_cpu_unit *unit,
    uint32_t next_fetch_address)
{
    const uint32_t opcode = unit->opcode;
    const bool preindexed = (opcode & UINT32_C(0x01000000)) != 0u;
    const bool add_offset = (opcode & UINT32_C(0x00800000)) != 0u;
    const bool user_bank = (opcode & UINT32_C(0x00400000)) != 0u;
    const bool writeback = (opcode & UINT32_C(0x00200000)) != 0u;
    const bool load = (opcode & UINT32_C(0x00100000)) != 0u;
    const uint8_t base_register =
        (uint8_t)((opcode >> 16u) & UINT32_C(0x0F));
    const uint16_t register_list = (uint16_t)opcode;
    const bool empty_list = register_list == 0u;
    const uint16_t effective_register_list =
        empty_list ? UINT16_C(0x8000) : register_list;
    const bool pc_in_list =
        (effective_register_list & UINT16_C(0x8000)) != 0u;
    const bool base_in_list =
        (effective_register_list &
         (uint16_t)(UINT16_C(1) << base_register)) != 0u;
    uint32_t transfer_count = 0u;
    uint32_t span;
    uint32_t writeback_span;
    uint32_t base;
    uint32_t address;
    uint32_t updated_base;
    uint32_t user_bank_cpsr;
    uint32_t restored_cpsr = 0u;
    size_t spsr_index = 0u;
    bool store_user_bank;
    bool load_user_bank;
    uint8_t pc_result_index = UINT8_MAX;
    uint8_t first_register = UINT8_MAX;
    uint8_t register_index;

    if ((opcode & UINT32_C(0x0E000000)) != UINT32_C(0x08000000)) {
        return false;
    }
    if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               false)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }
    if (base_register == 15u) {
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    /* DDI0100I A5.4 calls an empty list UNPREDICTABLE. The selected GBA ARMv4
     * policy is GBATEK 3.06 §ARM block transfers: `effective_register_list`
     * treats it as one PC transfer and the base adjusts by +/-0x40. The data
     * address follows the normal first-word P/U formula for that one PC slot;
     * this is an explicit model policy, not a claim of physical measurement. */
    user_bank_cpsr =
        (cpu->cpsr & ~GBL_CPU_CPSR_MODE_MASK) | UINT32_C(0x10);
    store_user_bank = user_bank && !load;
    load_user_bank = user_bank && load && !pc_in_list;
    if (user_bank) {
        const uint32_t mode = cpu->cpsr & GBL_CPU_CPSR_MODE_MASK;

        /* ARM ARM DDI0100I A4.1.21/A4.1.22/A4.1.98 defines the S-bit form
         * restrictions: User/System execution is unpredictable, and the
         * User-bank forms require W=0. ARM7TDMI DDI0029G §2.5 identifies User
         * as the sole nonprivileged mode; §2.6.1 confirms System shares the
         * User registers and describes the core's bank topology. Its Table
         * 1-2 distinguishes LDM^PC as “restore CPSR” from LDM^/STM^ as
         * “user registers,” but does not restate those mode restrictions.
         * Thus DDI0100I supplies form legality while DDI0029G cross-checks
         * ARM7TDMI mode and bank identity. LDM (3), selected by S=1 and PC in
         * the list, restores the current mode's captured SPSR after its loads. */
        if (mode == UINT32_C(0x10) || mode == UINT32_C(0x1F) ||
            (writeback && (!load || !pc_in_list))) {
            unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
            return true;
        }
        if (load && pc_in_list) {
            if (!gbl_cpu_spsr_index(cpu->cpsr, &spsr_index)) {
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
            restored_cpsr = cpu->spsr[spsr_index];
            if (!gbl_cpu_mode_valid(restored_cpsr)) {
                unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
                return true;
            }
        }
    }

    base = gbl_cpu_read_register(cpu, base_register,
                                 unit->instruction_address,
                                 GBL_CPU_PC_READ_NORMAL);
    for (register_index = 0u; register_index < 16u; ++register_index) {
        if ((effective_register_list &
             (uint16_t)(UINT16_C(1) << register_index)) != 0u) {
            if (first_register == UINT8_MAX) {
                first_register = register_index;
            }
            transfer_count++;
        }
    }
    span = transfer_count * UINT32_C(4);
    writeback_span = empty_list ? UINT32_C(0x40) : span;
    address = add_offset
                  ? base + (preindexed ? UINT32_C(4) : UINT32_C(0))
                  : base - span +
                        (preindexed ? UINT32_C(0) : UINT32_C(4));
    updated_base = add_offset ? base + writeback_span
                              : base - writeback_span;

    unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
    unit->multiple_transfer = true;
    unit->restore_cpsr_from_spsr = user_bank && load && pc_in_list;
    for (register_index = 0u; register_index < 16u; ++register_index) {
        const uint16_t register_bit =
            (uint16_t)(UINT16_C(1) << register_index);

        if ((effective_register_list & register_bit) != 0u) {
            const uint32_t bus_address = address & ~UINT32_C(3);

            if (!gbl_cpu_data_span_valid(
                    bus_address, (uint8_t)GBL_CPU_WORD_WIDTH)) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
                return true;
            }
            if (load) {
                uint8_t result_index;

                if (!gbl_cpu_add_data_read_slot(
                        unit, bus_address, (uint8_t)GBL_CPU_WORD_WIDTH, 0u,
                        false, &result_index)) {
                    unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
                    return true;
                }
                if (register_index == 15u) {
                    pc_result_index = result_index;
                } else if (!gbl_cpu_add_result_register_effect(
                               unit, register_index,
                               load_user_bank ? user_bank_cpsr : cpu->cpsr,
                               result_index)) {
                    unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
                    return true;
                }
            } else {
                const gbl_cpu_pc_read_context pc_context =
                    register_index == 15u
                        ? GBL_CPU_PC_READ_STORE_SOURCE
                        : GBL_CPU_PC_READ_NORMAL;
                uint32_t store_value = gbl_cpu_read_register_in_bank(
                    cpu, register_index,
                    store_user_bank ? user_bank_cpsr : cpu->cpsr,
                    unit->instruction_address, pc_context);

                if (writeback && base_in_list &&
                    register_index == base_register &&
                    register_index != first_register) {
                    /* GBATEK's ARMv4 policy stores the final base value when
                     * Rn is present after the first transfer slot. */
                    store_value = updated_base;
                }

                if (!gbl_cpu_add_data_write_slot(
                        unit, bus_address,
                        (uint8_t)GBL_CPU_WORD_WIDTH, store_value)) {
                    unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
                    return true;
                }
            }
            address += UINT32_C(4);
        }
    }
    /* DDI0029G section 6.1 says SEQ is pipelined one cycle ahead. In Tables
     * 6-12/6-13, row 1's N applies to transfer 0 and later S rows to following
     * data addresses. */
    {
        uint8_t slot_index;

        for (slot_index = 0u; slot_index < unit->data_slot_count;
             ++slot_index) {
            unit->data_slots[slot_index].access_class =
                slot_index == 0u ? GBL_BUS_CLASS_NONSEQUENTIAL
                                 : GBL_BUS_CLASS_SEQUENTIAL;
        }
    }
    unit->post_data_internal_cycles = load ? (uint8_t)1u : 0u;
    unit->post_data_pipeline_cycles =
        load && pc_result_index != UINT8_MAX ? (uint8_t)1u : 0u;
    if (unit->restore_cpsr_from_spsr &&
        !gbl_cpu_add_status_effect(unit, UINT32_MAX, restored_cpsr)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    /* The ARMv4/GBA rule suppresses the writeback effect for LDM when Rn is
     * loaded from the list. The register-list load itself still commits. */
    if (writeback && !(load && base_in_list) &&
        !gbl_cpu_add_literal_register_effect(
            unit, base_register, cpu->cpsr, updated_base)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if ((load && pc_result_index != UINT8_MAX &&
         !gbl_cpu_schedule_result_refill(
             unit, pc_result_index,
             unit->restore_cpsr_from_spsr &&
                 (restored_cpsr & GBL_CPU_CPSR_T) != 0u)) ||
        (!(load && pc_result_index != UINT8_MAX) &&
         !gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                            false))) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
    } else if (!load) {
        /* ARM7TDMI's STM cycle table signals an N follow-up fetch. */
        unit->fetch_class[0] = GBL_BUS_CLASS_NONSEQUENTIAL;
    }
    return true;
}

static bool gbl_cpu_decode(gbl_machine *machine, gbl_cpu_unit *unit)
{
    const gbl_cpu_state *cpu = &machine->cpu;
    const uint32_t opcode = cpu->pipeline[0].opcode;
    const bool thumb = (cpu->cpsr & GBL_CPU_CPSR_T) != 0u;
    uint32_t next_fetch_address;

    if (unit == NULL) {
        return false;
    }
    *unit = (gbl_cpu_unit){ 0 };
    unit->instruction_address = cpu->execution_address;
    unit->opcode = opcode;

    if (gbl_cpu_pipeline_is_empty(cpu)) {
        if (cpu->next_fetch_sequential ||
            !gbl_cpu_fetch_address_valid(
                cpu->execution_address,
                thumb ? (uint8_t)GBL_CPU_HALF_WIDTH
                      : (uint8_t)GBL_CPU_WORD_WIDTH)) {
            return false;
        }
        unit->kind = GBL_CPU_UNIT_REFILL;
        return gbl_cpu_schedule_refill(unit, cpu->execution_address, thumb);
    }
    if (!gbl_cpu_pipeline_is_ready(cpu)) {
        return false;
    }
    if (gbl_cpu_pipeline_is_arm_sequential_wrap(cpu) ||
        gbl_cpu_pipeline_is_thumb_sequential_wrap(cpu)) {
        /* DDI0100I A2.7.1 classifies sequential address overflow as
         * unpredictable. Keep only the internally generated ARM/Thumb refill
         * pair valid, then stop before any instruction, cycle, or trace effect
         * at that boundary. */
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    if (!gbl_cpu_next_sequential_address(cpu, thumb, &next_fetch_address)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }

    /* ARM conditions 0-14 annul the whole instruction when false. Resolve
     * that before any operand, encoding or extension classification so a
     * failed conditional instruction retires as its sequential no-op. Keep
     * condition 1111 outside this rule: its explicit NV/extension reporting
     * policy is handled below. */
    if (!thumb && (opcode >> 28u) != UINT32_C(0x0F) &&
        !gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               false)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    /* Recognize allocated later encodings before general ARMv4 NV. The
     * distinction is project reporting policy, not execution on ARM7TDMI. */
    if (!thumb && gbl_cpu_is_arm_extension(opcode)) {
        unit->kind = GBL_CPU_UNIT_LATER_ARCHITECTURE;
        return true;
    }
    if ((opcode >> 28u) == UINT32_C(0x0F)) {
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    if (!thumb && gbl_cpu_is_arm_undefined_space(opcode)) {
        if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u),
                                      cpu->cpsr)) {
            unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
            if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                                   false)) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            }
            return true;
        }
        unit->kind = GBL_CPU_UNIT_UNDEFINED;
        return gbl_cpu_finalize_decoded(unit, true);
    }
    if (thumb) {
        return gbl_cpu_finalize_decoded(
            unit, gbl_cpu_decode_thumb(cpu, unit, next_fetch_address));
    }
    if (gbl_cpu_is_arm_psr_transfer(opcode)) {
        return gbl_cpu_finalize_decoded(
            unit, gbl_cpu_decode_arm_psr_transfer(cpu, unit,
                                                  next_fetch_address));
    }
    if ((opcode & UINT32_C(0x0F000000)) == UINT32_C(0x0F000000)) {
        if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
            unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
            if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                                   false)) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            }
            return true;
        }
        return gbl_cpu_schedule_exception(unit, GBL_EXCEPTION_SWI);
    }
    if ((opcode & UINT32_C(0x0E000000)) == UINT32_C(0x0C000000) ||
        (opcode & UINT32_C(0x0F000000)) == UINT32_C(0x0E000000)) {
        if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
            unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
            if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                                   false)) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            }
            return true;
        }
        return gbl_cpu_schedule_exception(unit, GBL_EXCEPTION_COPROCESSOR);
    }

    if ((opcode & UINT32_C(0x0FFFFFF0)) == UINT32_C(0x012FFF10)) {
        const uint8_t source = (uint8_t)(opcode & UINT32_C(0x0F));
        uint32_t target_value;
        uint32_t target;
        bool target_thumb;

        if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
            unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
            if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                                   false)) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            }
            return true;
        }
        target_value = gbl_cpu_read_register(
            cpu, source, unit->instruction_address,
            GBL_CPU_PC_READ_NORMAL);
        target_thumb = (target_value & UINT32_C(1)) != 0u;
        target = target_value & ~UINT32_C(1);
        if (!target_thumb && (target & UINT32_C(2)) != 0u) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            return true;
        }
        return gbl_cpu_schedule_branch(unit, target, target_thumb);
    }

    if ((opcode & UINT32_C(0x0E000000)) == UINT32_C(0x0A000000)) {
        uint32_t offset = opcode & UINT32_C(0x00FFFFFF);
        const bool link = (opcode & UINT32_C(0x01000000)) != 0u;

        if (!gbl_cpu_condition_passed((uint8_t)(opcode >> 28u), cpu->cpsr)) {
            unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
            if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                                   false)) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            }
            return true;
        }
        if ((offset & UINT32_C(0x00800000)) != 0u) {
            offset |= UINT32_C(0xFF000000);
        }
        offset <<= 2u;
        if (link) {
            if (!gbl_cpu_add_literal_register_effect(
                    unit, (uint8_t)14u, cpu->cpsr,
                    unit->instruction_address + UINT32_C(4))) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
                return true;
            }
        }
        return gbl_cpu_schedule_branch(
            unit, unit->instruction_address + UINT32_C(8) + offset, false);
    }
    if (gbl_cpu_decode_arm_subword_transfer(cpu, unit,
                                           next_fetch_address)) {
        return gbl_cpu_finalize_decoded(unit, true);
    }
    if (gbl_cpu_decode_arm_swap(cpu, unit, next_fetch_address)) {
        return gbl_cpu_finalize_decoded(unit, true);
    }
    if (gbl_cpu_decode_arm_multiply(cpu, unit, next_fetch_address)) {
        return gbl_cpu_finalize_decoded(unit, true);
    }
    if (gbl_cpu_decode_arm_block_transfer(cpu, unit,
                                          next_fetch_address)) {
        return gbl_cpu_finalize_decoded(unit, true);
    }
    if ((opcode & UINT32_C(0x0C000000)) == 0u) {
        if (gbl_cpu_decode_data_processing(cpu, unit, next_fetch_address)) {
            return gbl_cpu_finalize_decoded(unit, true);
        }
    }
    /* ARMv4T Mode 2 has bit4 clear for register offsets. This region is
     * reserved in the exact-core encoding map, not a legal missing transfer. */
    if ((opcode & UINT32_C(0x0E000010)) == UINT32_C(0x06000010)) {
        unit->kind = GBL_CPU_UNIT_UNDEFINED;
        return gbl_cpu_finalize_decoded(unit, true);
    }
    if (gbl_cpu_decode_arm_single_transfer(cpu, unit, next_fetch_address)) {
        return gbl_cpu_finalize_decoded(unit, true);
    }

    unit->kind = GBL_CPU_UNIT_UNDEFINED;
    return gbl_cpu_finalize_decoded(unit, true);
}

static bool gbl_cpu_preflight(const gbl_machine *machine,
                              const gbl_cpu_unit *unit,
                              uint64_t remaining_budget,
                              const gbl_time_operation operations[
                                  GBL_TIME_OPERATION_MAX],
                              size_t operation_count,
                              gbl_run_reason *reason)
{
    const bool has_data_access = unit->data_slot_count != 0u;
    const bool exception = unit->kind == GBL_CPU_UNIT_EXCEPTION;
    uint64_t expected_cost = 0u;
    uint64_t expected_bus_cycles = 0u;
    uint64_t expected_internal_cycles = 0u;
    uint64_t expected_pipeline_bus_cycles = 0u;
    bool result_seen[GBL_CPU_RESULT_SLOT_MAX] = { false };
    uint8_t index;

    if (reason == NULL) {
        return false;
    }
    *reason = GBL_RUN_REASON_NONE;
    if (unit->timing_prepared) {
        size_t expected_operation_count =
            (unit->internal_cycles != 0u ? 1u : 0u) +
            (size_t)unit->data_slot_count +
            (unit->post_data_internal_cycles != 0u ? 1u : 0u) +
            (size_t)unit->post_data_pipeline_cycles +
            (size_t)unit->fetch_count;
        size_t operation_index;

        expected_internal_cycles = (uint64_t)unit->internal_cycles +
                                   (uint64_t)unit->post_data_internal_cycles;
        expected_pipeline_bus_cycles =
            (uint64_t)unit->post_data_pipeline_cycles;
        expected_bus_cycles = expected_pipeline_bus_cycles;
        if (unit->fetch_address_from_result &&
            !unit->fetch_target_projected) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
        if (operations == NULL || operation_count != expected_operation_count ||
            operation_count > GBL_TIME_OPERATION_MAX) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
        for (operation_index = 0u; operation_index < operation_count;
             ++operation_index) {
            const gbl_time_operation *operation =
                &operations[operation_index];

            if (operation->kind == GBL_TIME_OPERATION_BUS_READ ||
                operation->kind == GBL_TIME_OPERATION_BUS_WRITE) {
                if (operation->cycles == 0u ||
                    operation->cycles >
                        UINT64_MAX - expected_bus_cycles) {
                    *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
                    return false;
                }
                expected_bus_cycles += operation->cycles;
            } else if (operation->kind != GBL_TIME_OPERATION_INTERNAL) {
                *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
                return false;
            }
        }
        if (expected_internal_cycles > UINT64_MAX - expected_bus_cycles) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
        expected_cost = expected_bus_cycles + expected_internal_cycles;
    } else if (operation_count != 0u ||
               !gbl_cpu_unit_cost(machine, unit, &expected_cost,
                                  &expected_bus_cycles,
                                  &expected_internal_cycles,
                                  &expected_pipeline_bus_cycles)) {
        *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
        return false;
    }
    if (unit->result_count > GBL_CPU_RESULT_SLOT_MAX ||
        unit->register_effect_count > GBL_CPU_REGISTER_EFFECT_MAX ||
        unit->status_effect_count > GBL_CPU_STATUS_EFFECT_MAX) {
        *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
        return false;
    }
    if (unit->cycle_cost > remaining_budget) {
        *reason = GBL_RUN_REASON_CYCLE_BUDGET;
        return false;
    }
    if (unit->cycle_cost > UINT64_MAX - machine->master_cycles) {
        *reason = GBL_RUN_REASON_COUNTER_OVERFLOW;
        return false;
    }
    if (unit->kind != GBL_CPU_UNIT_REFILL && !unit->asynchronous &&
        machine->cpu.retired_instructions == UINT64_MAX) {
        *reason = GBL_RUN_REASON_COUNTER_OVERFLOW;
        return false;
    }
    if (unit->cycle_cost != expected_cost ||
        unit->bus_cycle_cost != expected_bus_cycles ||
        unit->internal_cycle_cost != expected_internal_cycles ||
        unit->post_data_pipeline_cost != expected_pipeline_bus_cycles ||
        (unit->multiple_transfer &&
         ((unit->kind != GBL_CPU_UNIT_SEQUENTIAL &&
           unit->kind != GBL_CPU_UNIT_BRANCH) ||
          unit->internal_cycles != 0u || unit->data_slot_count == 0u ||
          unit->post_data_internal_cycles !=
              (unit->data_slots[0].operation == GBL_CPU_DATA_READ
                   ? 1u
                   : 0u) ||
          unit->post_data_pipeline_cycles !=
              (unit->data_slots[0].operation == GBL_CPU_DATA_READ &&
                       unit->fetch_address_from_result
                   ? 1u
                   : 0u) ||
          (unit->data_slots[0].operation == GBL_CPU_DATA_WRITE &&
           unit->fetch_address_from_result) ||
          (unit->data_slots[0].operation == GBL_CPU_DATA_READ &&
           !unit->fetch_address_from_result &&
           (unit->fetch_count != 1u ||
            unit->fetch_class[0] !=
                (machine->cpu.force_nonsequential_fetch
                     ? GBL_BUS_CLASS_NONSEQUENTIAL
                     : GBL_BUS_CLASS_SEQUENTIAL))) ||
          (unit->data_slots[0].operation == GBL_CPU_DATA_WRITE &&
           (unit->fetch_count != 1u ||
            unit->fetch_class[0] != GBL_BUS_CLASS_NONSEQUENTIAL)) ||
          unit->status_effect_count !=
              (unit->restore_cpsr_from_spsr ? 1u : 0u) ||
          (unit->restore_cpsr_from_spsr &&
           !gbl_cpu_spsr_return_shape_valid(unit)))) ||
        (!unit->multiple_transfer &&
         (unit->post_data_internal_cycles != 0u ||
          unit->post_data_pipeline_cycles != 0u)) ||
        (unit->fetch_address_from_result &&
         (unit->kind != GBL_CPU_UNIT_BRANCH ||
          (unit->changes_state && !unit->restore_cpsr_from_spsr) ||
          unit->fetch_count != (uint8_t)GBL_CPU_FETCH_SLOT_MAX ||
          unit->fetch_width !=
              (unit->thumb_state ? (uint8_t)GBL_CPU_HALF_WIDTH
                                 : (uint8_t)GBL_CPU_WORD_WIDTH) ||
          unit->internal_cycles != 0u ||
          unit->fetch_result_mask !=
              (unit->thumb_state ? ~UINT32_C(1) : ~UINT32_C(3)) ||
          !gbl_cpu_result_refill_shape_valid(unit) ||
          unit->fetch_result_addend[0] != 0u ||
          unit->fetch_result_addend[1] !=
              (uint32_t)unit->fetch_width ||
          (unit->restore_cpsr_from_spsr
               ? !gbl_cpu_spsr_return_shape_valid(unit)
               : (unit->changes_state ||
                  (unit->thumb_state && !unit->multiple_transfer))) ||
          unit->fetch_class[0] != GBL_BUS_CLASS_NONSEQUENTIAL ||
          unit->fetch_class[1] != GBL_BUS_CLASS_SEQUENTIAL)) ||
        (unit->restore_cpsr_from_spsr &&
         (!unit->multiple_transfer || !unit->fetch_address_from_result)) ||
        (exception &&
         (unit->fetch_count != 0u || unit->internal_cycles != 1u ||
          has_data_access || unit->exception_kind <= GBL_EXCEPTION_NONE ||
          unit->exception_kind > GBL_EXCEPTION_COPROCESSOR)) ||
        (!exception &&
         (unit->fetch_count == 0u ||
          (!unit->fetch_address_from_result &&
           !gbl_cpu_fetch_address_valid(unit->fetch_address[0],
                                        unit->fetch_width)))) ||
        (exception && unit->fetch_address_from_result) ||
        (exception && (unit->result_count != 0u ||
                       unit->register_effect_count != 0u ||
                       unit->status_effect_count != 0u))) {
        *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
        return false;
    }
    for (index = 0u; index < unit->fetch_count; ++index) {
        if ((!unit->fetch_address_from_result &&
             !gbl_cpu_fetch_address_valid(unit->fetch_address[index],
                                          unit->fetch_width)) ||
            (unit->fetch_class[index] != GBL_BUS_CLASS_NONSEQUENTIAL &&
             unit->fetch_class[index] != GBL_BUS_CLASS_SEQUENTIAL)) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
    }
    for (index = 0u; index < unit->data_slot_count; ++index) {
        const gbl_cpu_data_slot *slot = &unit->data_slots[index];

        if (!gbl_cpu_data_span_valid(slot->address, slot->width)) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
        if (unit->multiple_transfer &&
            (slot->width != (uint8_t)GBL_CPU_WORD_WIDTH ||
             slot->access_class !=
                 (index == 0u ? GBL_BUS_CLASS_NONSEQUENTIAL
                              : GBL_BUS_CLASS_SEQUENTIAL) ||
             slot->rotate_word_read || slot->rotate_halfword_read ||
             slot->sign_extend_byte_read ||
             slot->sign_extend_halfword_read)) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
        if (slot->access_class != GBL_BUS_CLASS_NONSEQUENTIAL &&
            slot->access_class != GBL_BUS_CLASS_SEQUENTIAL) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
        if (slot->operation == GBL_CPU_DATA_READ) {
            const unsigned int transform_count =
                (slot->rotate_word_read ? 1u : 0u) +
                (slot->rotate_halfword_read ? 1u : 0u) +
                (slot->sign_extend_byte_read ? 1u : 0u) +
                (slot->sign_extend_halfword_read ? 1u : 0u);

            if (slot->result_index >= unit->result_count ||
                result_seen[slot->result_index] || transform_count > 1u ||
                (slot->rotate_word_read && slot->width !=
                                                (uint8_t)GBL_CPU_WORD_WIDTH) ||
                (slot->rotate_halfword_read && slot->width !=
                                                    (uint8_t)GBL_CPU_HALF_WIDTH) ||
                (slot->sign_extend_byte_read && slot->width != 1u) ||
                (slot->sign_extend_halfword_read && slot->width !=
                                                        (uint8_t)GBL_CPU_HALF_WIDTH)) {
                *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
                return false;
            }
            result_seen[slot->result_index] = true;
        } else if (slot->operation == GBL_CPU_DATA_WRITE) {
            if (slot->result_index != UINT8_MAX ||
                slot->rotate_word_read || slot->rotate_halfword_read ||
                slot->sign_extend_byte_read ||
                slot->sign_extend_halfword_read) {
                *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
                return false;
            }
        } else {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
    }
    for (index = 0u; index < unit->result_count; ++index) {
        if (!result_seen[index]) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
    }
    if (unit->fetch_address_from_result &&
        !result_seen[unit->fetch_result_index]) {
        *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
        return false;
    }
    for (index = 0u; index < unit->register_effect_count; ++index) {
        const gbl_cpu_register_effect *effect =
            &unit->register_effects[index];

        if (effect->register_index >= 15u ||
            !gbl_cpu_mode_valid(effect->bank_cpsr) ||
            (effect->from_result
                 ? effect->result_index >= unit->result_count
                 : effect->result_index != UINT8_MAX)) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
    }
    for (index = 0u; index < unit->status_effect_count; ++index) {
        if (unit->status_effects[index].mask == 0u ||
            (unit->status_effects[index].spsr_index != UINT8_MAX &&
             unit->status_effects[index].spsr_index >= 5u)) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
    }
    return true;
}

static bool gbl_cpu_fetch_unit(gbl_machine *machine,
                               gbl_cpu_unit *unit,
                               gbl_cpu_pipeline_entry fetched[2],
                               const gbl_run_options *options,
                               gbl_trace_record trace_records[
                                   GBL_CPU_TRACE_UNIT_RECORD_MAX],
                               size_t *trace_record_count)
{
    const uint64_t saved_cycles = machine->master_cycles;
    const uint32_t saved_open_bus = machine->open_bus;
    uint8_t index;

    for (index = 0u; index < unit->fetch_count; ++index) {
        uint32_t fetch_address = unit->fetch_address[index];

        if (unit->fetch_address_from_result) {
            /* Preflight proves this ARM-word or Thumb-halfword mask/addend
             * form yields aligned requests. Unsigned addition intentionally
             * wraps the second refill at 2^32. */
            fetch_address =
                (unit->results[unit->fetch_result_index] &
                 unit->fetch_result_mask) + unit->fetch_result_addend[index];
            unit->fetch_address[index] = fetch_address;
            if (index == 0u && unit->kind == GBL_CPU_UNIT_BRANCH) {
                unit->branch_target = fetch_address;
            }
        }
        const gbl_bus_result result = gbl_bus_read_fetch(
            machine, fetch_address, unit->fetch_width,
            unit->fetch_class[index]);

        if (result.status != GBL_BUS_STATUS_OK) {
            /* Bus reads only affect time and the open-bus latch. Restore both
             * if an unexpected checked-bus error interrupts this unit. */
            machine->master_cycles = saved_cycles;
            machine->open_bus = saved_open_bus;
            return false;
        }
        fetched[index].address = fetch_address;
        fetched[index].opcode = result.value;
        fetched[index].width = unit->fetch_width;
        fetched[index].valid = true;
        {
            gbl_trace_record record = { 0 };

            record.guest_cycle = machine->master_cycles;
            record.category = GBL_TRACE_BUS;
            record.instruction_address = unit->instruction_address;
            record.opcode = unit->kind == GBL_CPU_UNIT_REFILL
                                ? 0u
                                : unit->opcode;
            record.address = fetch_address;
            record.value = result.value;
            record.width = unit->fetch_width;
            record.access_class = result.access_class;
            record.bus_outcome = result.outcome;
            if (!gbl_cpu_trace_stage(options, trace_records,
                                     trace_record_count, record)) {
                machine->master_cycles = saved_cycles;
                machine->open_bus = saved_open_bus;
                return false;
            }
        }
    }
    return true;
}

static bool gbl_cpu_ranges_overlap(const void *left, size_t left_size,
                                   const void *right, size_t right_size)
{
    const uintptr_t left_start = (uintptr_t)left;
    const uintptr_t right_start = (uintptr_t)right;
    const uintptr_t left_span = (uintptr_t)left_size;
    const uintptr_t right_span = (uintptr_t)right_size;

    if (left_start > UINTPTR_MAX - left_span ||
        right_start > UINTPTR_MAX - right_span) {
        return true;
    }
    return left_start < right_start + right_span &&
           right_start < left_start + left_span;
}

static bool gbl_cpu_trace_options_valid(const gbl_machine *machine,
                                        const gbl_run_options *options,
                                        const gbl_run_result *result)
{
    const uint32_t mask = options->category_mask;
    const gbl_trace_buffer *trace = options->trace_buffer;
    size_t records_size;

    if ((mask & ~GBL_TRACE_CATEGORY_MASK) != 0u) {
        return false;
    }
    if (mask == 0u) {
        return true;
    }
    if (trace == NULL ||
        gbl_cpu_ranges_overlap(trace, sizeof(*trace), machine,
                              sizeof(*machine)) ||
        gbl_cpu_ranges_overlap(trace, sizeof(*trace), result,
                              sizeof(*result)) ||
        gbl_cpu_ranges_overlap(trace, sizeof(*trace), options,
                              sizeof(*options)) ||
        gbl_cpu_ranges_overlap(trace, sizeof(*trace), machine->rom,
                              machine->rom_length) ||
        trace->count > trace->capacity ||
        trace->capacity > SIZE_MAX / sizeof(*trace->records)) {
        return false;
    }
    records_size = trace->capacity * sizeof(*trace->records);
    if (records_size == 0u) {
        return true;
    }
    if (trace->records == NULL ||
        (uintmax_t)records_size > (uintmax_t)UINTPTR_MAX ||
        (uintptr_t)trace->records >
            UINTPTR_MAX - (uintptr_t)records_size ||
        gbl_cpu_ranges_overlap(trace, sizeof(*trace), trace->records,
                              records_size) ||
        gbl_cpu_ranges_overlap(trace->records, records_size, machine,
                              sizeof(*machine)) ||
        gbl_cpu_ranges_overlap(trace->records, records_size, result,
                              sizeof(*result)) ||
        gbl_cpu_ranges_overlap(trace->records, records_size, options,
                              sizeof(*options)) ||
        gbl_cpu_ranges_overlap(trace->records, records_size, machine->rom,
                              machine->rom_length)) {
        return false;
    }
    return true;
}

static size_t gbl_cpu_trace_required_records(const gbl_cpu_unit *unit,
                                             uint32_t category_mask)
{
    size_t records = 0u;

    if (unit->kind != GBL_CPU_UNIT_REFILL && !unit->asynchronous &&
        (category_mask & (uint32_t)GBL_TRACE_INSTRUCTION) != 0u) {
        records++;
    }
    if ((category_mask & (uint32_t)GBL_TRACE_BUS) != 0u) {
        records += (size_t)unit->fetch_count;
        records += (size_t)unit->data_slot_count;
    }
    if (unit->kind == GBL_CPU_UNIT_EXCEPTION &&
        (category_mask & (uint32_t)GBL_TRACE_EXCEPTION) != 0u) {
        records++;
    }
    return records;
}

static bool gbl_cpu_trace_preflight(const gbl_run_options *options,
                                    size_t required_records,
                                    gbl_run_reason *reason)
{
    const gbl_trace_buffer *trace = options->trace_buffer;

    if (reason == NULL) {
        return false;
    }
    if (required_records == 0u) {
        return true;
    }
    if (trace == NULL ||
        required_records > trace->capacity - trace->count ||
        (uint64_t)required_records >
            UINT64_MAX - trace->next_sequence) {
        *reason = GBL_RUN_REASON_TRACE_OUTPUT_FULL;
        return false;
    }
    return true;
}

static bool gbl_cpu_test_build_maximum_unit(gbl_cpu_unit *unit)
{
    uint8_t index;

    if (unit == NULL) {
        return false;
    }
    *unit = (gbl_cpu_unit){
        .kind = GBL_CPU_UNIT_BRANCH,
        .instruction_address = UINT32_C(0x08000000),
        .opcode = UINT32_C(0xEA000000),
        .fetch_count = (uint8_t)GBL_CPU_FETCH_SLOT_MAX,
        .fetch_width = (uint8_t)GBL_CPU_WORD_WIDTH,
        .fetch_address = { UINT32_C(0x08000000), UINT32_C(0x08000004) },
        .fetch_class = { GBL_BUS_CLASS_NONSEQUENTIAL,
                         GBL_BUS_CLASS_SEQUENTIAL }
    };

    for (index = 0u; index < GBL_CPU_DATA_SLOT_MAX; ++index) {
        uint8_t result_index;

        if (!gbl_cpu_add_data_read_slot(
                unit, UINT32_C(0x02000000) + (uint32_t)index * UINT32_C(4),
                (uint8_t)GBL_CPU_WORD_WIDTH, 0u, false, &result_index) ||
            !gbl_cpu_add_result_register_effect(
                unit, (uint8_t)(index % 15u), UINT32_C(0x0000001F),
                result_index)) {
            return false;
        }
    }
    if (!gbl_cpu_add_status_effect(unit, UINT32_C(0xF0000000),
                                   UINT32_C(0xA0000000))) {
        return false;
    }
    return gbl_cpu_refresh_cycle_cost(unit);
}

bool gbl_cpu_test_maximum_preflight(
    const gbl_machine *machine, const gbl_run_options *options,
    gbl_cpu_test_count_overflow count_overflow, uint64_t *cycle_cost,
    size_t *required_records, gbl_run_reason *reason)
{
    gbl_cpu_unit unit;
    gbl_run_reason preflight_reason = GBL_RUN_REASON_NONE;
    size_t selected_records;

    if (cycle_cost != NULL) {
        *cycle_cost = 0u;
    }
    if (required_records != NULL) {
        *required_records = 0u;
    }
    if (reason != NULL) {
        *reason = GBL_RUN_REASON_NONE;
    }
    if (machine == NULL || options == NULL || cycle_cost == NULL ||
        required_records == NULL || reason == NULL ||
        !gbl_cpu_test_build_maximum_unit(&unit) ||
        !gbl_cpu_refresh_cycle_cost_for_machine(machine, &unit)) {
        return false;
    }
    *cycle_cost = unit.cycle_cost;

    switch (count_overflow) {
    case GBL_CPU_TEST_COUNT_VALID:
        break;
    case GBL_CPU_TEST_FETCH_COUNT_OVERFLOW:
        unit.fetch_count++;
        break;
    case GBL_CPU_TEST_DATA_COUNT_OVERFLOW:
        unit.data_slot_count++;
        break;
    case GBL_CPU_TEST_RESULT_COUNT_OVERFLOW:
        unit.result_count++;
        break;
    case GBL_CPU_TEST_REGISTER_EFFECT_COUNT_OVERFLOW:
        unit.register_effect_count++;
        break;
    case GBL_CPU_TEST_STATUS_EFFECT_COUNT_OVERFLOW:
        unit.status_effect_count++;
        break;
    default:
        *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
        return false;
    }

    if (!gbl_cpu_preflight(machine, &unit, options->cycle_budget,
                           NULL, 0u,
                           &preflight_reason)) {
        *reason = preflight_reason;
        return false;
    }
    selected_records = gbl_cpu_trace_required_records(
        &unit, options->category_mask);
    *required_records = selected_records;
    if (!gbl_cpu_trace_preflight(options, selected_records,
                                 &preflight_reason)) {
        *reason = preflight_reason;
        return false;
    }
    return true;
}

static bool gbl_cpu_trace_stage(const gbl_run_options *options,
                                gbl_trace_record records[
                                    GBL_CPU_TRACE_UNIT_RECORD_MAX],
                                size_t *record_count,
                                gbl_trace_record record)
{
    if ((options->category_mask & (uint32_t)record.category) == 0u) {
        return true;
    }
    if (*record_count >= GBL_CPU_TRACE_UNIT_RECORD_MAX) {
        return false;
    }
    records[*record_count] = record;
    (*record_count)++;
    return true;
}

static void gbl_cpu_trace_commit(gbl_trace_buffer *trace,
                                 gbl_trace_record records[
                                     GBL_CPU_TRACE_UNIT_RECORD_MAX],
                                 size_t record_count)
{
    size_t index;

    if (record_count == 0u) {
        return;
    }
    for (index = 0u; index < record_count; ++index) {
        records[index].sequence = trace->next_sequence + (uint64_t)index;
        trace->records[trace->count + index] = records[index];
    }
    trace->count += record_count;
    trace->next_sequence += (uint64_t)record_count;
}

static void gbl_cpu_trace_commit_dma(
    gbl_trace_buffer *trace, const gbl_dma_step_result *observations,
    size_t observation_count)
{
    size_t index;

    for (index = 0u; index < observation_count; ++index) {
        gbl_trace_record record =
            gbl_cpu_dma_trace_record(&observations[index]);

        record.sequence = trace->next_sequence + (uint64_t)index;
        trace->records[trace->count + index] = record;
    }
    trace->count += observation_count;
    trace->next_sequence += (uint64_t)observation_count;
}

static void gbl_cpu_release_dma_observations(
    gbl_machine *machine, gbl_dma_step_result *observations, size_t bytes)
{
    if (observations != NULL && machine != NULL &&
        machine->allocator.release != NULL) {
        machine->allocator.release(machine->allocator.context, observations,
                                   bytes);
    }
}

static gbl_run_reason gbl_cpu_stop_result(gbl_cpu_stop_reason reason)
{
    switch (reason) {
    case GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION:
        return GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION;
    case GBL_CPU_STOP_UNPREDICTABLE_INSTRUCTION:
        return GBL_RUN_REASON_UNPREDICTABLE_INSTRUCTION;
    case GBL_CPU_STOP_LATER_ARCHITECTURE_INSTRUCTION:
        return GBL_RUN_REASON_LATER_ARCHITECTURE_INSTRUCTION;
    case GBL_CPU_STOP_COUNTER_OVERFLOW:
        return GBL_RUN_REASON_COUNTER_OVERFLOW;
    case GBL_CPU_STOP_INTERNAL_ERROR:
        return GBL_RUN_REASON_INTERNAL_ERROR;
    case GBL_CPU_STOP_RESET_NO_BIOS:
    case GBL_CPU_STOP_UNDEFINED_NO_BIOS:
    case GBL_CPU_STOP_SWI_NO_BIOS:
    case GBL_CPU_STOP_PREFETCH_ABORT_NO_BIOS:
    case GBL_CPU_STOP_DATA_ABORT_NO_BIOS:
    case GBL_CPU_STOP_IRQ_NO_BIOS:
    case GBL_CPU_STOP_FIQ_NO_BIOS:
    case GBL_CPU_STOP_COPROCESSOR_NO_BIOS:
    case GBL_CPU_STOP_STOP_REQUESTED:
    case GBL_CPU_STOP_HALT_DEADLOCK:
        return GBL_RUN_REASON_GUEST_STOP;
    case GBL_CPU_STOP_NONE:
    case GBL_CPU_STOP_BUS_ERROR:
    default:
        return GBL_RUN_REASON_BUS_ERROR;
    }
}

static bool gbl_cpu_append_time_operation(
    gbl_time_operation operations[GBL_TIME_OPERATION_MAX], size_t *count,
    gbl_time_operation_kind kind, uint32_t address, uint8_t width,
    uint32_t value, uint64_t cycles)
{
    if (operations == NULL || count == NULL ||
        *count >= GBL_TIME_OPERATION_MAX) {
        return false;
    }
    operations[*count] = (gbl_time_operation){
        kind, width, address, value, cycles
    };
    ++*count;
    return true;
}

static uint32_t gbl_cpu_transform_data_read(
    const gbl_cpu_data_slot *slot, uint32_t value)
{
    if (slot->rotate_word_read) {
        const uint8_t rotate =
            (uint8_t)((uint32_t)slot->alignment_lane * 8u);
        value = gbl_shift(GBL_SHIFT_ROR, value, rotate, false, true).value;
    }
    if (slot->rotate_halfword_read && slot->alignment_lane != 0u) {
        value = gbl_shift(GBL_SHIFT_ROR, value, 8u, false, true).value;
    }
    if (slot->sign_extend_byte_read) {
        value &= UINT32_C(0x000000FF);
        if ((value & UINT32_C(0x00000080)) != 0u) {
            value |= UINT32_C(0xFFFFFF00);
        }
    }
    if (slot->sign_extend_halfword_read) {
        value &= UINT32_C(0x0000FFFF);
        if ((value & UINT32_C(0x00008000)) != 0u) {
            value |= UINT32_C(0xFFFF0000);
        }
    }
    return value;
}

static bool gbl_cpu_prepare_time_prefix(
    const gbl_machine *machine,
    const gbl_time_operation operations[GBL_TIME_OPERATION_MAX],
    size_t count, gbl_machine_time_preview *candidate,
    gbl_time_status *status)
{
    *status = gbl_machine_prepare_operations(machine, operations, count,
                                             candidate);
    return *status == GBL_TIME_STATUS_OK;
}

static bool gbl_cpu_build_time_operations(
    const gbl_machine *machine, gbl_cpu_unit *unit,
    gbl_time_operation operations[GBL_TIME_OPERATION_MAX], size_t *count,
    gbl_machine_time_preview *prepared_time, gbl_time_status *time_status)
{
    gbl_machine_time_preview candidate;
    uint32_t projected_open_bus;
    uint64_t bus_total = 0u;
    uint64_t internal_total = 0u;
    uint64_t pipeline_total = 0u;
    uint8_t index;

    if (time_status != NULL) {
        *time_status = GBL_TIME_STATUS_INVALID_ARGUMENT;
    }
    if (machine == NULL || unit == NULL || operations == NULL || count == NULL ||
        prepared_time == NULL || time_status == NULL ||
        unit->fetch_count > GBL_CPU_FETCH_SLOT_MAX ||
        unit->data_slot_count > GBL_CPU_DATA_SLOT_MAX) {
        return false;
    }
    *count = 0u;
    unit->timing_prepared = false;
    unit->fetch_target_projected = false;
    if (!gbl_cpu_prepare_time_prefix(machine, operations, 0u, &candidate,
                                     time_status)) {
        return false;
    }
    projected_open_bus = machine->open_bus;
    if (unit->internal_cycles != 0u) {
        if (!gbl_cpu_append_time_operation(
                operations, count, GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u,
                unit->internal_cycles)) {
            return false;
        }
        internal_total = unit->internal_cycles;
        if (!gbl_cpu_prepare_time_prefix(machine, operations, *count,
                                         &candidate, time_status)) {
            return false;
        }
    }
    for (index = 0u; index < unit->data_slot_count; ++index) {
        const gbl_cpu_data_slot *slot = &unit->data_slots[index];
        const gbl_time_operation_kind kind =
            slot->operation == GBL_CPU_DATA_WRITE
                ? GBL_TIME_OPERATION_BUS_WRITE
                : GBL_TIME_OPERATION_BUS_READ;
        gbl_cycle_cost quote;

        if (!gbl_bus_quote_waitcnt(
                candidate.waitcnt, slot->address,
                slot->operation == GBL_CPU_DATA_WRITE
                    ? GBL_BUS_DIRECTION_WRITE
                    : GBL_BUS_DIRECTION_READ,
                slot->width, slot->access_class, GBL_BUS_ROLE_DATA, &quote) ||
            quote.cycles > UINT64_MAX - bus_total ||
            !gbl_cpu_append_time_operation(
                operations, count, kind, slot->address, slot->width,
                slot->write_value, quote.cycles)) {
            return false;
        }
        bus_total += quote.cycles;
        if (!gbl_cpu_prepare_time_prefix(machine, operations, *count,
                                         &candidate, time_status)) {
            return false;
        }
        if (slot->operation == GBL_CPU_DATA_READ) {
            gbl_bus_result projected;
            uint32_t next_open_bus;

            if (!gbl_bus_project_read(
                    machine, &candidate, slot->address, slot->width,
                    slot->access_class, GBL_BUS_ROLE_DATA,
                    projected_open_bus, &projected, &next_open_bus) ||
                projected.status != GBL_BUS_STATUS_OK ||
                slot->result_index >= unit->result_count) {
                return false;
            }
            unit->results[slot->result_index] =
                gbl_cpu_transform_data_read(slot, projected.value);
            projected_open_bus = next_open_bus;
        }
    }
    if (unit->post_data_internal_cycles != 0u) {
        if (unit->post_data_internal_cycles >
            UINT64_MAX - internal_total ||
            !gbl_cpu_append_time_operation(
                operations, count, GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u,
                unit->post_data_internal_cycles)) {
            return false;
        }
        internal_total += unit->post_data_internal_cycles;
        if (!gbl_cpu_prepare_time_prefix(machine, operations, *count,
                                         &candidate, time_status)) {
            return false;
        }
    }
    for (index = 0u; index < unit->post_data_pipeline_cycles; ++index) {
        if (bus_total == UINT64_MAX || pipeline_total == UINT64_MAX ||
            !gbl_cpu_append_time_operation(
                operations, count, GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u,
                UINT64_C(1))) {
            return false;
        }
        ++bus_total;
        ++pipeline_total;
        if (!gbl_cpu_prepare_time_prefix(machine, operations, *count,
                                         &candidate, time_status)) {
            return false;
        }
    }
    for (index = 0u; index < unit->fetch_count; ++index) {
        uint32_t address = unit->fetch_address[index];
        gbl_cycle_cost quote;

        if (unit->fetch_address_from_result) {
            address = (unit->results[unit->fetch_result_index] &
                       unit->fetch_result_mask) +
                      unit->fetch_result_addend[index];
            unit->fetch_address[index] = address;
        }
        if (!gbl_bus_quote_waitcnt(
                candidate.waitcnt, address, GBL_BUS_DIRECTION_READ,
                unit->fetch_width, unit->fetch_class[index],
                GBL_BUS_ROLE_FETCH, &quote) ||
            quote.cycles > UINT64_MAX - bus_total ||
            !gbl_cpu_append_time_operation(
                operations, count, GBL_TIME_OPERATION_BUS_READ, address,
                unit->fetch_width, 0u, quote.cycles)) {
            return false;
        }
        bus_total += quote.cycles;
        if (!gbl_cpu_prepare_time_prefix(machine, operations, *count,
                                         &candidate, time_status)) {
            return false;
        }
    }
    if (internal_total > UINT64_MAX - bus_total) {
        *time_status = GBL_TIME_STATUS_CYCLE_OVERFLOW;
        return false;
    }
    unit->cycle_cost = bus_total + internal_total;
    unit->bus_cycle_cost = bus_total;
    unit->internal_cycle_cost = internal_total;
    unit->post_data_pipeline_cost = pipeline_total;
    unit->fetch_target_projected = unit->fetch_address_from_result;
    unit->timing_prepared = true;
    if (candidate.master_cycles < machine->master_cycles ||
        candidate.master_cycles - machine->master_cycles !=
            unit->cycle_cost) {
        *time_status = GBL_TIME_STATUS_INVALID_ARGUMENT;
        unit->timing_prepared = false;
        return false;
    }
    *prepared_time = candidate;
    *time_status = GBL_TIME_STATUS_OK;
    return true;
}

static uint64_t gbl_cpu_ppu_row_digest_byte(uint64_t digest, uint8_t byte)
{
    return (digest ^ (uint64_t)byte) * UINT64_C(1099511628211);
}

static uint64_t gbl_cpu_ppu_row_digest_u64(uint64_t digest, uint64_t value)
{
    unsigned int byte_index;

    for (byte_index = 0u; byte_index < 8u; ++byte_index) {
        digest = gbl_cpu_ppu_row_digest_byte(
            digest, (uint8_t)(value & UINT64_C(0xFF)));
        value >>= 8u;
    }
    return digest;
}

static uint64_t gbl_cpu_ppu_row_digest_bytes(uint64_t digest,
                                             const uint8_t *bytes,
                                             size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        digest = gbl_cpu_ppu_row_digest_byte(digest, bytes[index]);
    }
    return digest;
}

static uint64_t gbl_cpu_ppu_row_digest(
    const uint8_t *bytes, size_t length,
    const gbl_ppu_row_capture *capture)
{
    uint64_t digest = UINT64_C(14695981039346656037);
    size_t affine_index;

    digest = gbl_cpu_ppu_row_digest_bytes(digest, bytes, length);
    digest = gbl_cpu_ppu_row_digest_u64(digest, capture->cycle);
    digest = gbl_cpu_ppu_row_digest_u64(digest, capture->frame_epoch);
    digest = gbl_cpu_ppu_row_digest_u64(digest, capture->line);
    digest = gbl_cpu_ppu_row_digest_u64(digest, capture->dispcnt);
    digest = gbl_cpu_ppu_row_digest_u64(digest, capture->prior_write_count);
    digest = gbl_cpu_ppu_row_digest_u64(
        digest, capture->unsupported_register_bytes);
    digest = gbl_cpu_ppu_row_digest_bytes(
        digest, capture->render_register_bytes,
        sizeof(capture->render_register_bytes));
    digest = gbl_cpu_ppu_row_digest_bytes(
        digest, capture->bg_register_bytes,
        sizeof(capture->bg_register_bytes));
    digest = gbl_cpu_ppu_row_digest_bytes(
        digest, capture->window_register_bytes,
        sizeof(capture->window_register_bytes));
    for (affine_index = 0u;
         affine_index < GBL_PPU_AFFINE_BACKGROUND_COUNT; ++affine_index) {
        const gbl_ppu_affine_state *affine = &capture->affine[affine_index];

        digest = gbl_cpu_ppu_row_digest_u64(digest, (uint16_t)affine->pa);
        digest = gbl_cpu_ppu_row_digest_u64(digest, (uint16_t)affine->pb);
        digest = gbl_cpu_ppu_row_digest_u64(digest, (uint16_t)affine->pc);
        digest = gbl_cpu_ppu_row_digest_u64(digest, (uint16_t)affine->pd);
        digest = gbl_cpu_ppu_row_digest_u64(
            digest, affine->programmed_reference_x);
        digest = gbl_cpu_ppu_row_digest_u64(
            digest, affine->programmed_reference_y);
        digest = gbl_cpu_ppu_row_digest_u64(digest, affine->origin_x);
        digest = gbl_cpu_ppu_row_digest_u64(digest, affine->origin_y);
        digest = gbl_cpu_ppu_row_digest_byte(
            digest, affine->origin_valid ? 1u : 0u);
        digest = gbl_cpu_ppu_row_digest_byte(
            digest, affine->hblank_y_suppressed ? 1u : 0u);
    }
    digest = gbl_cpu_ppu_row_digest_bytes(
        digest, capture->obj_selected_mask,
        sizeof(capture->obj_selected_mask));
    digest = gbl_cpu_ppu_row_digest_u64(digest, capture->obj_selected_count);
    digest = gbl_cpu_ppu_row_digest_u64(digest, capture->obj_omitted_count);
    digest = gbl_cpu_ppu_row_digest_u64(digest, capture->obj_work_used);
    return digest;
}

static bool gbl_cpu_time_preview_matches_machine(
    const gbl_machine *machine,
    const gbl_machine_time_preview *preview)
{
    size_t index;

    if (machine == NULL || preview == NULL ||
        machine->master_cycles != preview->master_cycles ||
        machine->next_event_sequence != preview->next_event_sequence ||
        machine->interrupt_flags != preview->interrupt_flags ||
        machine->interrupt_enable != preview->interrupt_enable ||
        machine->interrupt_master_enable !=
            preview->interrupt_master_enable ||
        machine->waitcnt != preview->waitcnt ||
        !gbl_audio_state_equal(&machine->audio, &preview->audio) ||
        machine->dispcnt != preview->ppu.dispcnt ||
        machine->bus_owner != preview->bus_owner ||
        machine->pressed_keys != preview->pressed_keys ||
        machine->key_control != preview->key_control ||
        machine->key_condition != preview->key_condition ||
        machine->key_irq_condition != preview->key_irq_condition ||
        machine->input_generation != preview->input_generation ||
        machine->input_event_count != preview->input_event_count ||
        machine->input_event_cursor != preview->input_event_cursor ||
        machine->event_count != preview->event_count ||
        machine->ppu.line_origin != preview->ppu.line_origin ||
        machine->ppu.generation != preview->ppu.generation ||
        machine->ppu.frame_epoch != preview->ppu.frame_epoch ||
        machine->ppu.line != preview->ppu.line ||
        machine->ppu.scheduled_line != preview->ppu.scheduled_line ||
        machine->ppu.dispcnt != preview->ppu.dispcnt ||
        machine->ppu.dispstat != preview->ppu.dispstat ||
        machine->ppu.compare != preview->ppu.compare ||
        machine->ppu.captured_rows != preview->ppu.captured_rows ||
        machine->ppu.capture_state != preview->ppu.capture_state ||
        machine->ppu.next_boundary_hblank !=
            preview->ppu.next_boundary_hblank ||
        machine->ppu.frame_started != preview->ppu.frame_started ||
        !gbl_cpu_bytes_equal(machine->ppu.row_coverage,
                             preview->ppu.row_coverage,
                             sizeof(machine->ppu.row_coverage)) ||
        preview->ppu_row_capture_count > GBL_PPU_ROW_CAPTURE_MAX ||
        preview->ppu_memory_write_count > GBL_TIME_OPERATION_MAX ||
        (preview->ppu_clear_video &&
         preview->ppu_row_capture_count != preview->ppu.captured_rows)) {
        return false;
    }
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        const gbl_event *actual = &machine->events[index];
        const gbl_event *expected = &preview->events[index];

        if (actual->cycle != expected->cycle ||
            actual->sequence != expected->sequence ||
            actual->generation != expected->generation ||
            actual->priority != expected->priority ||
            actual->kind != expected->kind ||
            actual->timer_index != expected->timer_index ||
            actual->dma_channel != expected->dma_channel ||
            actual->live != expected->live ||
            actual->input_index != expected->input_index) {
            return false;
        }
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        const gbl_timer_state *actual = &machine->timers[index];
        const gbl_timer_state *expected = &preview->timers[index];

        if (actual->reload != expected->reload ||
            actual->counter != expected->counter ||
            actual->divisor != expected->divisor ||
            actual->phase != expected->phase ||
            actual->anchor_cycle != expected->anchor_cycle ||
            actual->generation != expected->generation ||
            actual->enabled != expected->enabled ||
            actual->irq_enabled != expected->irq_enabled ||
            actual->cascade != expected->cascade) {
            return false;
        }
    }
    for (index = 0u; index < GBL_DMA_CHANNEL_COUNT; ++index) {
        const gbl_dma_channel_state *actual = &machine->dma_channels[index];
        const gbl_dma_channel_state *expected = &preview->dma_channels[index];

        if (actual->source_address != expected->source_address ||
            actual->destination_address != expected->destination_address ||
            actual->current_source != expected->current_source ||
            actual->current_destination != expected->current_destination ||
            actual->read_latch != expected->read_latch ||
            actual->completed_units != expected->completed_units ||
            actual->trigger_cycle != expected->trigger_cycle ||
            actual->trigger_origin != expected->trigger_origin ||
            actual->trigger_dispute != expected->trigger_dispute ||
            actual->count != expected->count ||
            actual->control != expected->control ||
            actual->remaining != expected->remaining ||
            actual->pending != expected->pending ||
            actual->active != expected->active ||
            actual->startup_complete != expected->startup_complete) {
            return false;
        }
    }
    for (index = 0u; index < machine->input_event_count; ++index) {
        const gbl_input_event *actual = &machine->input_events[index];
        const gbl_input_event *expected = &preview->input_events[index];

        if (actual->cycle != expected->cycle ||
            actual->pressed_mask != expected->pressed_mask) {
            return false;
        }
    }
    /* The PPU can capture rows in every display mode; capability status is
     * decided by the PPU policy, while this check validates time and bytes. */
    for (index = 0u; index < preview->ppu_row_capture_count; ++index) {
        const gbl_ppu_row_capture *capture =
            &preview->ppu_row_captures[index];
        const size_t offset = (size_t)capture->line * GBL_PPU_ROW_BYTES;
        const uint64_t line_offset =
            (uint64_t)capture->line * GBL_PPU_LINE_CYCLES;
        uint64_t frame_offset;
        uint64_t expected_cycle;
        uint64_t digest;
        size_t expected_prefix = 0u;
        size_t write_index;

        if (capture->line >= GBL_PPU_VISIBLE_LINES ||
            capture->frame_epoch != preview->ppu.frame_epoch ||
            capture->frame_epoch > UINT64_MAX / GBL_PPU_FRAME_CYCLES ||
            (machine->ppu.row_coverage[capture->line >> 3u] &
             (uint8_t)(UINT8_C(1) << (capture->line & 7u))) == 0u ||
            capture->prior_write_count > preview->ppu_memory_write_count) {
            return false;
        }
        frame_offset = capture->frame_epoch * GBL_PPU_FRAME_CYCLES;
        if (frame_offset > UINT64_MAX - line_offset) {
            return false;
        }
        expected_cycle = frame_offset + line_offset;
        if (capture->cycle != expected_cycle ||
            (index > 0u &&
             (preview->ppu_row_captures[index - 1u].cycle >=
                  capture->cycle ||
              preview->ppu_row_captures[index - 1u].prior_write_count >
                  capture->prior_write_count))) {
            return false;
        }
        for (write_index = 0u;
             write_index < preview->ppu_memory_write_count;
             ++write_index) {
            if (preview->ppu_memory_writes[write_index].cycle <
                capture->cycle) {
                ++expected_prefix;
            }
        }
        if (expected_prefix != (size_t)capture->prior_write_count) {
            return false;
        }
        digest = gbl_cpu_ppu_row_digest(&machine->video_frame[offset],
                                        GBL_PPU_ROW_BYTES, capture);
        if (digest != capture->pixel_digest) {
            return false;
        }
    }
    for (index = 0u; index < preview->ppu_memory_write_count; ++index) {
        const gbl_ppu_memory_write *write =
            &preview->ppu_memory_writes[index];
        uint32_t lane;
        bool touches_video = false;

        if ((write->width != 1u && write->width != 2u &&
             write->width != 4u) ||
            write->address > UINT32_MAX - ((uint32_t)write->width - 1u) ||
            write->cycle > preview->master_cycles ||
            (index > 0u &&
             preview->ppu_memory_writes[index - 1u].cycle >= write->cycle) ||
            write->region !=
                (uint8_t)gbl_bus_region_for_address(write->address)) {
            return false;
        }
        for (lane = 0u; lane < (uint32_t)write->width; ++lane) {
            const gbl_bus_region region =
                gbl_bus_region_for_address(write->address + lane);

            if (region == GBL_BUS_REGION_PRAM ||
                region == GBL_BUS_REGION_VRAM ||
                region == GBL_BUS_REGION_OAM) {
                touches_video = true;
            }
        }
        if (!touches_video) {
            return false;
        }
    }
    if (preview->ppu_clear_video) {
        size_t line;

        for (line = 0u; line < GBL_PPU_VISIBLE_LINES; ++line) {
            const uint8_t mask =
                (uint8_t)(UINT8_C(1) << (line & 7u));
            size_t pixel;

            if ((preview->ppu.row_coverage[line >> 3u] & mask) != 0u) {
                continue;
            }
            for (pixel = 0u; pixel < GBL_PPU_ROW_BYTES; ++pixel) {
                if (machine->video_frame[line * GBL_PPU_ROW_BYTES + pixel] !=
                    0u) {
                    return false;
                }
            }
        }
    }
    return true;
}

static gbl_run_reason gbl_cpu_time_refusal_reason(gbl_time_status status)
{
    if (status == GBL_TIME_STATUS_CYCLE_OVERFLOW) {
        return GBL_RUN_REASON_COUNTER_OVERFLOW;
    }
    if (status == GBL_TIME_STATUS_INVALID_ARGUMENT ||
        status == GBL_TIME_STATUS_BACKWARD) {
        return GBL_RUN_REASON_INVALID_CPU_STATE;
    }
    return GBL_RUN_REASON_TIME_REFUSAL;
}

static bool gbl_cpu_scheduler_event_live(const gbl_machine *machine,
                                         const gbl_event *event)
{
    if (!event->live) {
        return false;
    }
    if (event->kind == (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW) {
        const gbl_timer_state *timer;

        if (event->timer_index >= GBL_TIMER_COUNT) {
            return false;
        }
        timer = &machine->timers[event->timer_index];
        return timer->enabled && !timer->cascade &&
               event->generation == timer->generation;
    }
    if (event->kind == (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT) {
        return event->generation == machine->input_generation &&
               event->input_index == machine->input_event_cursor &&
               machine->input_event_cursor < machine->input_event_count &&
               machine->input_events[machine->input_event_cursor].cycle ==
                   event->cycle;
    }
    if (event->kind == (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY) {
        return machine->ppu.generation != 0u &&
               event->generation == machine->ppu.generation;
    }
    return false;
}

static bool gbl_cpu_next_live_event(const gbl_machine *machine,
                                    uint64_t *cycle_out)
{
    bool found = false;
    size_t index;

    for (index = 0u; index < machine->event_count; ++index) {
        const gbl_event *event = &machine->events[index];

        if (gbl_cpu_scheduler_event_live(machine, event) &&
            (!found || event->cycle < *cycle_out)) {
            *cycle_out = event->cycle;
            found = true;
        }
    }
    return found;
}

static uint64_t gbl_cpu_wait_dispatch_count(const gbl_machine *machine,
                                            uint64_t target)
{
    uint64_t dispatches = 0u;
    size_t index;

    /* Every pending logical input through target will be reinserted and
     * dispatched in order, including multiple records at the same cycle. */
    for (index = machine->input_event_cursor;
         index < machine->input_event_count &&
         machine->input_events[index].cycle <= target;
         ++index) {
        ++dispatches;
    }
    for (index = 0u; index < machine->event_count; ++index) {
        const gbl_event *event = &machine->events[index];

        if (event->cycle <= target &&
            (event->kind != (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT ||
             !gbl_cpu_scheduler_event_live(machine, event))) {
            ++dispatches;
        }
    }
    return dispatches;
}

/* One HALT wait unit preflights and commits only through the next live event
 * or the remaining budget horizon. True means return to the CPU loop. */
static bool gbl_cpu_wait_halted(gbl_machine *machine, uint64_t remaining,
                                uint64_t *event_work,
                                gbl_run_result *run_result)
{
    const uint16_t pending = (uint16_t)(machine->interrupt_flags &
                                        machine->interrupt_enable &
                                        UINT16_C(0x3FFF));
    uint64_t next_cycle = 0u;
    uint64_t horizon;
    uint64_t target;
    uint64_t dispatches;
    uint64_t elapsed;
    gbl_machine_time_preview preview = { 0 };
    gbl_time_status time_status;

    if (machine->event_count > GBL_EVENT_CAPACITY ||
        machine->input_event_count > GBL_INPUT_EVENT_CAPACITY ||
        machine->input_event_cursor > machine->input_event_count) {
        run_result->termination = GBL_RUN_STOPPED;
        run_result->reason = GBL_RUN_REASON_INVALID_CPU_STATE;
        return false;
    }
    if (pending != 0u) {
        machine->power_state = GBL_MACHINE_POWER_RUNNING;
        run_result->wake_cycle = machine->master_cycles;
        run_result->wake_source_mask = pending;
        run_result->power_cause = GBL_CPU_POWER_CAUSE_WAKE_PENDING;
        return true;
    }
    if (!gbl_cpu_next_live_event(machine, &next_cycle)) {
        machine->cpu.stopped = true;
        machine->cpu.stop_reason = GBL_CPU_STOP_HALT_DEADLOCK;
        run_result->termination = GBL_RUN_STOPPED;
        run_result->reason = GBL_RUN_REASON_GUEST_STOP;
        run_result->power_cause = GBL_CPU_POWER_CAUSE_HALT_DEADLOCK;
        return false;
    }

    horizon = remaining > UINT64_MAX - machine->master_cycles
                  ? UINT64_MAX
                  : machine->master_cycles + remaining;
    target = next_cycle <= horizon ? next_cycle : horizon;
    dispatches = gbl_cpu_wait_dispatch_count(machine, target);
    if (dispatches > (uint64_t)GBL_MACHINE_MAX_EVENT_DISPATCH -
                          *event_work) {
        run_result->termination = GBL_RUN_STOPPED;
        run_result->reason = GBL_RUN_REASON_TIME_REFUSAL;
        return false;
    }
    time_status = gbl_machine_prepare_time(machine, target, &preview);
    if (time_status != GBL_TIME_STATUS_OK) {
        run_result->termination = GBL_RUN_STOPPED;
        run_result->reason = gbl_cpu_time_refusal_reason(time_status);
        return false;
    }
    elapsed = target - machine->master_cycles;
    gbl_machine_commit_time(machine, &preview);
    *event_work += dispatches;
    run_result->cycles += elapsed;
    run_result->wait_cycles += elapsed;

    {
        const uint16_t wake_sources = (uint16_t)(
            machine->interrupt_flags & machine->interrupt_enable &
            UINT16_C(0x3FFF));

        if (wake_sources != 0u) {
            machine->power_state = GBL_MACHINE_POWER_RUNNING;
            run_result->wake_cycle = machine->master_cycles;
            run_result->wake_source_mask = wake_sources;
            run_result->power_cause = GBL_CPU_POWER_CAUSE_WAKE_EVENT;
            return true;
        }
    }
    if (target == horizon) {
        run_result->termination = GBL_RUN_TIMEOUT;
        run_result->reason = GBL_RUN_REASON_CYCLE_BUDGET;
        run_result->power_cause = GBL_CPU_POWER_CAUSE_WAIT_TIMEOUT;
        return false;
    }
    return true;
}

gbl_run_status gbl_cpu_run(gbl_machine *machine,
                           const gbl_run_options *options,
                           gbl_run_result *result)
{
    gbl_run_options run_options;
    gbl_run_result run_result = {
        .termination = GBL_RUN_TIMEOUT,
        .reason = GBL_RUN_REASON_CYCLE_BUDGET,
        .wait_cycles = 0u,
        .wake_cycle = 0u,
        .wake_source_mask = 0u,
        .power_cause = GBL_CPU_POWER_CAUSE_NONE
    };
    uint64_t completed_units = 0u;
    uint64_t wait_event_work = 0u;

    if (machine == NULL || options == NULL || result == NULL ||
        !machine->initialized || machine->rom == NULL ||
        machine->rom_length == 0u ||
        gbl_cpu_ranges_overlap(machine, sizeof(*machine), result,
                               sizeof(*result)) ||
        gbl_cpu_ranges_overlap(options, sizeof(*options), result,
                               sizeof(*result)) ||
        gbl_cpu_ranges_overlap(result, sizeof(*result), machine->rom,
                               machine->rom_length)) {
        return GBL_RUN_STATUS_INVALID_ARGUMENT;
    }
    if (!gbl_cpu_trace_options_valid(machine, options, result)) {
        return GBL_RUN_STATUS_INVALID_ARGUMENT;
    }
    run_options = *options;

    for (;;) {
        gbl_cpu_unit unit = { 0 };
        gbl_run_reason preflight_reason;
        gbl_cpu_pipeline_entry fetched[2] = { { 0u, 0u, 0u, false },
                                              { 0u, 0u, 0u, false } };
        gbl_trace_record trace_records[GBL_CPU_TRACE_UNIT_RECORD_MAX] = {
            { 0 }
        };
        gbl_time_operation time_operations[GBL_TIME_OPERATION_MAX] = { { 0 } };
        gbl_machine_time_preview prepared_time = { 0 };
        size_t time_operation_count = 0u;
        size_t trace_record_count = 0u;
        uint64_t unit_start_cycle;
        gbl_cpu_irq_observation boundary_irq = { 0 };
        bool asynchronous_irq = false;
        bool effects_committed_before_fetch = false;
        gbl_time_status time_status = GBL_TIME_STATUS_INVALID_ARGUMENT;

        if (machine->cpu.stopped) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = gbl_cpu_stop_result(machine->cpu.stop_reason);
            if (machine->cpu.stop_reason == GBL_CPU_STOP_STOP_REQUESTED) {
                run_result.power_cause =
                    GBL_CPU_POWER_CAUSE_STOP_REQUESTED;
            } else if (machine->cpu.stop_reason ==
                       GBL_CPU_STOP_HALT_DEADLOCK) {
                run_result.power_cause =
                    GBL_CPU_POWER_CAUSE_HALT_DEADLOCK;
            }
            break;
        }
        if (machine->power_state == GBL_MACHINE_POWER_STOPPED) {
            machine->cpu.stopped = true;
            machine->cpu.stop_reason = GBL_CPU_STOP_STOP_REQUESTED;
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_GUEST_STOP;
            run_result.power_cause = GBL_CPU_POWER_CAUSE_STOP_REQUESTED;
            break;
        }
        if (machine->power_state != GBL_MACHINE_POWER_RUNNING &&
            machine->power_state != GBL_MACHINE_POWER_HALTED) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            break;
        }
        if (run_options.unit_limit != 0u &&
            completed_units >= run_options.unit_limit) {
            run_result.termination = GBL_RUN_PROGRESS;
            run_result.reason = GBL_RUN_REASON_UNIT_LIMIT;
            break;
        }
        if (run_result.cycles >= run_options.cycle_budget) {
            break;
        }
        {
            bool dma_pending =
                machine->bus_owner == GBL_MACHINE_BUS_OWNER_DMA;
            uint8_t channel_index;

            for (channel_index = 0u;
                 channel_index < GBL_DMA_CHANNEL_COUNT; ++channel_index) {
                if (machine->dma_channels[channel_index].pending ||
                    machine->dma_channels[channel_index].active) {
                    dma_pending = true;
                    break;
                }
            }
            if (dma_pending) {
                gbl_dma_step_result dma_step = { 0 };
                gbl_dma_step_result *dma_observations = NULL;
                size_t dma_observation_count = 0u;
                size_t dma_observation_capacity = 0u;
                size_t dma_observation_limit = 0u;
                size_t dma_observation_bytes = 0u;
                gbl_run_reason trace_reason = GBL_RUN_REASON_NONE;
                gbl_dma_status dma_status;
                uint64_t dma_bus_cycles;
                uint64_t dma_cycles;

                if ((run_options.category_mask &
                     (uint32_t)GBL_TRACE_DMA) != 0u &&
                    channel_index < GBL_DMA_CHANNEL_COUNT) {
                    if (!gbl_cpu_trace_preflight(&run_options, 1u,
                                                 &trace_reason)) {
                        run_result.termination = GBL_RUN_TRACE_OUTPUT_FULL;
                        run_result.reason = trace_reason;
                        break;
                    }
                    dma_observation_limit = 1u;
                    if (machine->dma_channels[channel_index].trigger_origin ==
                            GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_A ||
                        machine->dma_channels[channel_index].trigger_origin ==
                            GBL_DMA_TRIGGER_ORIGIN_SOUND_FIFO_B) {
                        dma_observation_limit =
                            (size_t)GBL_DMA_OBSERVATION_CAPACITY;
                    }
                    dma_observation_capacity =
                        run_options.trace_buffer->capacity -
                        run_options.trace_buffer->count;
                    {
                        const uint64_t sequence_capacity =
                            UINT64_MAX -
                            run_options.trace_buffer->next_sequence;

                        if (sequence_capacity <
                            (uint64_t)dma_observation_capacity) {
                            dma_observation_capacity =
                                (size_t)sequence_capacity;
                        }
                    }
                    if (dma_observation_capacity > dma_observation_limit) {
                        dma_observation_capacity =
                            dma_observation_limit;
                    }
                    if (dma_observation_capacity >
                            SIZE_MAX / sizeof(*dma_observations) ||
                        dma_observation_capacity == 0u) {
                        run_result.termination = GBL_RUN_TRACE_OUTPUT_FULL;
                        run_result.reason =
                            GBL_RUN_REASON_TRACE_OUTPUT_FULL;
                        break;
                    }
                    dma_observation_bytes =
                        dma_observation_capacity * sizeof(*dma_observations);
                    if (machine->allocator.allocate == NULL ||
                        machine->allocator.release == NULL) {
                        run_result.termination = GBL_RUN_STOPPED;
                        run_result.reason = GBL_RUN_REASON_TIME_REFUSAL;
                        break;
                    }
                    dma_observations = machine->allocator.allocate(
                        machine->allocator.context, dma_observation_bytes);
                    if (dma_observations == NULL) {
                        run_result.termination = GBL_RUN_STOPPED;
                        run_result.reason = GBL_RUN_REASON_TIME_REFUSAL;
                        break;
                    }
                    dma_status = gbl_machine_dma_step_observed(
                        machine,
                        run_options.cycle_budget - run_result.cycles,
                        &dma_step, dma_observations,
                        dma_observation_capacity,
                        &dma_observation_count);
                } else {
                    dma_status = gbl_machine_dma_step(
                        machine,
                        run_options.cycle_budget - run_result.cycles,
                        &dma_step);
                }
                if (dma_status == GBL_DMA_STATUS_IDLE) {
                    /* No pending work; proceed to the next CPU unit. */
                } else if (dma_status == GBL_DMA_STATUS_BUDGET_REFUSAL) {
                    run_result.termination = GBL_RUN_TIMEOUT;
                    run_result.reason = GBL_RUN_REASON_CYCLE_BUDGET;
                    gbl_cpu_release_dma_observations(
                        machine, dma_observations, dma_observation_bytes);
                    break;
                } else if (dma_status == GBL_DMA_STATUS_OUTPUT_REFUSAL) {
                    run_result.termination = GBL_RUN_TRACE_OUTPUT_FULL;
                    run_result.reason = GBL_RUN_REASON_TRACE_OUTPUT_FULL;
                    gbl_cpu_release_dma_observations(
                        machine, dma_observations, dma_observation_bytes);
                    break;
                } else if (dma_status != GBL_DMA_STATUS_OK) {
                    run_result.termination = GBL_RUN_STOPPED;
                    run_result.reason = dma_status ==
                                                GBL_DMA_STATUS_TIME_OVERFLOW
                                            ? GBL_RUN_REASON_COUNTER_OVERFLOW
                                            : GBL_RUN_REASON_TIME_REFUSAL;
                    gbl_cpu_release_dma_observations(
                        machine, dma_observations, dma_observation_bytes);
                    break;
                } else {
                    dma_bus_cycles = dma_step.source_cycles +
                                     dma_step.destination_cycles;
                    dma_cycles = dma_step.startup_cycles + dma_bus_cycles;
                    if (dma_cycles >
                        run_options.cycle_budget - run_result.cycles) {
                        run_result.termination = GBL_RUN_STOPPED;
                        run_result.reason = GBL_RUN_REASON_COUNTER_OVERFLOW;
                        gbl_cpu_release_dma_observations(
                            machine, dma_observations,
                            dma_observation_bytes);
                        break;
                    }
                    run_result.cycles += dma_cycles;
                    run_result.bus_cycles += dma_bus_cycles;
                    run_result.internal_cycles += dma_step.startup_cycles;
                    run_result.dma_startup_cycles +=
                        dma_step.startup_cycles;
                    run_result.dma_source_cycles += dma_step.source_cycles;
                    run_result.dma_destination_cycles +=
                        dma_step.destination_cycles;
                    run_result.dma_pause_cycles += dma_cycles;
                    if (dma_step.accepted_units != 0u) {
                        run_result.dma_completed_units +=
                            dma_step.accepted_units;
                    } else if (dma_step.unit_completed) {
                        ++run_result.dma_completed_units;
                    }
                    if ((run_options.category_mask &
                         (uint32_t)GBL_TRACE_DMA) != 0u) {
                        gbl_cpu_trace_commit_dma(
                            run_options.trace_buffer, dma_observations,
                            dma_observation_count);
                    }
                    gbl_cpu_release_dma_observations(
                        machine, dma_observations, dma_observation_bytes);
                    continue;
                }
                gbl_cpu_release_dma_observations(
                    machine, dma_observations, dma_observation_bytes);
            }
        }
        if (machine->power_state == GBL_MACHINE_POWER_HALTED) {
            const uint64_t remaining =
                run_options.cycle_budget - run_result.cycles;

            if (gbl_cpu_wait_halted(machine, remaining, &wait_event_work,
                                    &run_result)) {
                continue;
            }
            break;
        }
        if (machine->cpu.pipeline[0].valid &&
            machine->cpu.pipeline[1].valid) {
            boundary_irq.sampled = true;
            boundary_irq.sample_cycle = machine->master_cycles;
            boundary_irq.interrupt_flags =
                (uint16_t)(machine->interrupt_flags & UINT16_C(0x3FFF));
            boundary_irq.interrupt_enable =
                (uint16_t)(machine->interrupt_enable & UINT16_C(0x3FFF));
            boundary_irq.interrupt_master_enable =
                (uint16_t)(machine->interrupt_master_enable & UINT16_C(1));
            boundary_irq.cpsr = machine->cpu.cpsr;
            boundary_irq.next_instruction_address =
                machine->cpu.pipeline[0].address;
            boundary_irq.following_instruction_address =
                machine->cpu.pipeline[1].address;
            boundary_irq.eligible_sources = (uint16_t)(
                boundary_irq.interrupt_flags &
                boundary_irq.interrupt_enable);
            boundary_irq.eligible = boundary_irq.eligible_sources != 0u;
            boundary_irq.master_enabled =
                boundary_irq.interrupt_master_enable != 0u;
            boundary_irq.cpu_masked =
                (machine->cpu.cpsr & GBL_CPU_CPSR_I) != 0u;
            boundary_irq.accepted = boundary_irq.eligible &&
                                    boundary_irq.master_enabled &&
                                    !boundary_irq.cpu_masked;
            if (boundary_irq.accepted) {
                if (!gbl_cpu_schedule_exception(&unit, GBL_EXCEPTION_IRQ)) {
                    run_result.termination = GBL_RUN_STOPPED;
                    run_result.reason = GBL_RUN_REASON_INVALID_CPU_STATE;
                    break;
                }
                unit.asynchronous = true;
                unit.instruction_address =
                    boundary_irq.next_instruction_address;
                unit.thumb_state =
                    (machine->cpu.cpsr & GBL_CPU_CPSR_T) != 0u;
                asynchronous_irq = true;
            }
        }
        if (!asynchronous_irq && !gbl_cpu_decode(machine, &unit)) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            break;
        }
        if (machine->cpu.force_nonsequential_fetch &&
            unit.fetch_count != 0u) {
            unit.fetch_class[0] = GBL_BUS_CLASS_NONSEQUENTIAL;
        }
        unit_start_cycle = machine->master_cycles;
        if (unit.kind == GBL_CPU_UNIT_UNSUPPORTED ||
            unit.kind == GBL_CPU_UNIT_UNPREDICTABLE ||
            unit.kind == GBL_CPU_UNIT_LATER_ARCHITECTURE) {
            machine->cpu.stopped = true;
            machine->cpu.stop_reason =
                unit.kind == GBL_CPU_UNIT_UNPREDICTABLE
                    ? GBL_CPU_STOP_UNPREDICTABLE_INSTRUCTION
                    : unit.kind == GBL_CPU_UNIT_LATER_ARCHITECTURE
                        ? GBL_CPU_STOP_LATER_ARCHITECTURE_INSTRUCTION
                        : GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = gbl_cpu_stop_result(machine->cpu.stop_reason);
            break;
        }
        if (!gbl_cpu_build_time_operations(
                machine, &unit, time_operations, &time_operation_count,
                &prepared_time, &time_status)) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = time_status == GBL_TIME_STATUS_INVALID_ARGUMENT
                                    ? GBL_RUN_REASON_INVALID_CPU_STATE
                                    : gbl_cpu_time_refusal_reason(time_status);
            break;
        }
        if (time_status != GBL_TIME_STATUS_OK) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = gbl_cpu_time_refusal_reason(time_status);
            break;
        }
        if (!gbl_cpu_preflight(machine, &unit,
                               run_options.cycle_budget - run_result.cycles,
                               time_operations, time_operation_count,
                               &preflight_reason)) {
            run_result.termination =
                preflight_reason == GBL_RUN_REASON_CYCLE_BUDGET
                    ? GBL_RUN_TIMEOUT
                    : GBL_RUN_STOPPED;
            run_result.reason = preflight_reason;
            break;
        }
        if (!gbl_cpu_trace_preflight(
                &run_options,
                gbl_cpu_trace_required_records(&unit,
                                               run_options.category_mask),
                &preflight_reason)) {
            run_result.termination = GBL_RUN_TRACE_OUTPUT_FULL;
            run_result.reason = preflight_reason;
            break;
        }
        if (unit.kind != GBL_CPU_UNIT_REFILL && !unit.asynchronous) {
            gbl_trace_record record = { 0 };

            record.guest_cycle = unit_start_cycle;
            record.category = GBL_TRACE_INSTRUCTION;
            record.instruction_address = unit.instruction_address;
            record.opcode = unit.opcode;
            if (!gbl_cpu_trace_stage(&run_options, trace_records,
                                     &trace_record_count, record)) {
                run_result.termination = GBL_RUN_STOPPED;
                run_result.reason = GBL_RUN_REASON_INVALID_CPU_STATE;
                break;
            }
        }
        if (unit.kind == GBL_CPU_UNIT_REFILL &&
            !gbl_cpu_fetch_unit(machine, &unit, fetched, &run_options,
                                trace_records, &trace_record_count)) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_BUS_ERROR;
            break;
        }
        if (unit.kind == GBL_CPU_UNIT_EXCEPTION) {
            const uint32_t resume_address =
                unit.asynchronous
                    ? boundary_irq.next_instruction_address
                    : unit.instruction_address;
            const gbl_cpu_exception_context context =
                unit.asynchronous
                    ? (gbl_cpu_exception_context){
                          resume_address, resume_address, resume_address,
                          unit.thumb_state }
                    : (gbl_cpu_exception_context){
                          unit.instruction_address,
                          machine->cpu.pipeline[1].address,
                          unit.instruction_address,
                          (machine->cpu.cpsr & GBL_CPU_CPSR_T) != 0u };
            const gbl_cpu_status exception_status = gbl_cpu_enter_exception(
                machine, unit.exception_kind, &context);

            if (exception_status != GBL_CPU_STATUS_OK) {
                run_result.termination = GBL_RUN_STOPPED;
                run_result.reason =
                    exception_status == GBL_CPU_STATUS_COUNTER_OVERFLOW
                        ? GBL_RUN_REASON_COUNTER_OVERFLOW
                        : exception_status == GBL_CPU_STATUS_BUS_ERROR
                              ? GBL_RUN_REASON_BUS_ERROR
                              : GBL_RUN_REASON_INVALID_CPU_STATE;
                break;
            }
            {
                gbl_trace_record record = { 0 };

                record.guest_cycle = machine->master_cycles;
                record.category = GBL_TRACE_EXCEPTION;
                record.instruction_address = unit.instruction_address;
                record.opcode = unit.opcode;
                record.exception_kind = unit.exception_kind;
                if (!gbl_cpu_trace_stage(&run_options, trace_records,
                                         &trace_record_count, record)) {
                    run_result.termination = GBL_RUN_STOPPED;
                    run_result.reason = GBL_RUN_REASON_INVALID_CPU_STATE;
                    break;
                }
            }
            if (!unit.asynchronous) {
                machine->cpu.retired_instructions++;
                run_result.instructions++;
            }
            run_result.cycles += unit.cycle_cost;
            run_result.bus_cycles += unit.bus_cycle_cost;
            run_result.internal_cycles += unit.internal_cycle_cost;
            if (!gbl_cpu_time_preview_matches_machine(machine,
                                                      &prepared_time)) {
                machine->cpu.stopped = true;
                machine->cpu.stop_reason = GBL_CPU_STOP_INTERNAL_ERROR;
                run_result.termination = GBL_RUN_STOPPED;
                run_result.reason = GBL_RUN_REASON_INTERNAL_ERROR;
                break;
            }
            if (boundary_irq.sampled) {
                if (unit.asynchronous) {
                    boundary_irq.entered = true;
                    boundary_irq.entry_cycle = machine->master_cycles;
                    boundary_irq.entry_internal_cycles =
                        unit.internal_cycle_cost;
                }
                machine->cpu.irq_observation = boundary_irq;
            }
            gbl_cpu_trace_commit(run_options.trace_buffer, trace_records,
                                 trace_record_count);
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_GUEST_STOP;
            break;
        }
        if (unit.internal_cycles != 0u) {
            const gbl_bus_result internal_result =
                gbl_bus_internal(machine, (uint64_t)unit.internal_cycles);

            if (internal_result.status != GBL_BUS_STATUS_OK ||
                internal_result.cycles != (uint64_t)unit.internal_cycles) {
                run_result.termination = GBL_RUN_STOPPED;
                run_result.reason = GBL_RUN_REASON_BUS_ERROR;
                machine->cpu.stopped = true;
                machine->cpu.stop_reason = GBL_CPU_STOP_BUS_ERROR;
                break;
            }
        }
        {
            uint8_t data_index;

            for (data_index = 0u; data_index < unit.data_slot_count;
                 ++data_index) {
                const gbl_cpu_data_slot *slot =
                    &unit.data_slots[data_index];
                gbl_bus_result data_result;
                gbl_trace_record record = { 0 };

                if (slot->operation == GBL_CPU_DATA_READ) {
                    uint32_t value;

                    data_result = gbl_bus_read(
                        machine, slot->address, slot->width,
                        slot->access_class);
                    if (data_result.status != GBL_BUS_STATUS_OK) {
                        run_result.termination = GBL_RUN_STOPPED;
                        run_result.reason = GBL_RUN_REASON_BUS_ERROR;
                        machine->cpu.stopped = true;
                        machine->cpu.stop_reason = GBL_CPU_STOP_BUS_ERROR;
                        break;
                    }
                    value = gbl_cpu_transform_data_read(slot,
                                                        data_result.value);
                    unit.results[slot->result_index] = value;
                } else {
                    data_result = gbl_bus_write(
                        machine, slot->address, slot->width,
                        slot->access_class, slot->write_value);
                    if (data_result.status != GBL_BUS_STATUS_OK) {
                        run_result.termination = GBL_RUN_STOPPED;
                        run_result.reason = GBL_RUN_REASON_BUS_ERROR;
                        machine->cpu.stopped = true;
                        machine->cpu.stop_reason = GBL_CPU_STOP_BUS_ERROR;
                        break;
                    }
                }

                record.guest_cycle = machine->master_cycles;
                record.category = GBL_TRACE_BUS;
                record.instruction_address = unit.instruction_address;
                record.opcode = unit.opcode;
                record.address = slot->address;
                record.value = data_result.value;
                record.width = slot->width;
                record.access_class = data_result.access_class;
                record.bus_outcome = data_result.outcome;
                if (!gbl_cpu_trace_stage(&run_options, trace_records,
                                         &trace_record_count, record)) {
                    run_result.termination = GBL_RUN_STOPPED;
                    run_result.reason = GBL_RUN_REASON_INVALID_CPU_STATE;
                    break;
                }
            }
            if (data_index < unit.data_slot_count) {
                break;
            }
        }
        if (unit.post_data_internal_cycles != 0u) {
            const gbl_bus_result internal_result = gbl_bus_internal(
                machine, (uint64_t)unit.post_data_internal_cycles);

            if (internal_result.status != GBL_BUS_STATUS_OK ||
                internal_result.cycles !=
                    (uint64_t)unit.post_data_internal_cycles) {
                run_result.termination = GBL_RUN_STOPPED;
                run_result.reason = GBL_RUN_REASON_BUS_ERROR;
                machine->cpu.stopped = true;
                machine->cpu.stop_reason = GBL_CPU_STOP_BUS_ERROR;
                break;
            }
        }
        if (unit.post_data_pipeline_cycles != 0u) {
            /* The selected LDM-to-PC S pipeline interval is bus/model work.
             * The loaded target is already read by the refill path, so this
             * advances time without duplicating a guest memory transaction. */
            const gbl_bus_result pipeline_result = gbl_bus_internal(
                machine, unit.post_data_pipeline_cost);

            if (pipeline_result.status != GBL_BUS_STATUS_OK ||
                pipeline_result.cycles !=
                    unit.post_data_pipeline_cost) {
                run_result.termination = GBL_RUN_STOPPED;
                run_result.reason = GBL_RUN_REASON_BUS_ERROR;
                machine->cpu.stopped = true;
                machine->cpu.stop_reason = GBL_CPU_STOP_BUS_ERROR;
                break;
            }
        }
        if (unit.restore_cpsr_from_spsr) {
            /* The complete read and both refill slots were preflighted
             * against the uniform bus before this point. Preserve ARM's
             * exception-return order: commit original-bank loads, restore
             * CPSR, then issue state-width-correct refill requests. */
            gbl_cpu_commit_effects(&machine->cpu, &unit);
            effects_committed_before_fetch = true;
        }
        if (unit.kind != GBL_CPU_UNIT_REFILL &&
            !gbl_cpu_fetch_unit(machine, &unit, fetched, &run_options,
                                trace_records, &trace_record_count)) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_BUS_ERROR;
            break;
        }

        if (unit.kind == GBL_CPU_UNIT_REFILL) {
            machine->cpu.pipeline[0] = fetched[0];
            machine->cpu.pipeline[1] = fetched[1];
        } else if (unit.kind == GBL_CPU_UNIT_BRANCH) {
            if (!effects_committed_before_fetch) {
                gbl_cpu_commit_effects(&machine->cpu, &unit);
            }
            if (unit.changes_state) {
                machine->cpu.cpsr =
                    (machine->cpu.cpsr & ~GBL_CPU_CPSR_T) |
                    (unit.thumb_state ? GBL_CPU_CPSR_T : 0u);
            }
            machine->cpu.execution_address = unit.branch_target;
            machine->cpu.pipeline[0] = fetched[0];
            machine->cpu.pipeline[1] = fetched[1];
            machine->cpu.retired_instructions++;
            run_result.instructions++;
        } else {
            gbl_cpu_commit_effects(&machine->cpu, &unit);
            machine->cpu.execution_address = machine->cpu.pipeline[1].address;
            machine->cpu.pipeline[0] = machine->cpu.pipeline[1];
            machine->cpu.pipeline[1] = fetched[0];
            machine->cpu.retired_instructions++;
            run_result.instructions++;
        }
        machine->cpu.next_fetch_sequential = true;
        machine->cpu.force_nonsequential_fetch = false;
        if (!gbl_cpu_time_preview_matches_machine(machine, &prepared_time)) {
            machine->cpu.stopped = true;
            machine->cpu.stop_reason = GBL_CPU_STOP_INTERNAL_ERROR;
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_INTERNAL_ERROR;
            break;
        }
        if (boundary_irq.sampled) {
            machine->cpu.irq_observation = boundary_irq;
        }
        completed_units++;
        run_result.cycles += unit.cycle_cost;
        run_result.bus_cycles += unit.bus_cycle_cost;
        run_result.internal_cycles += unit.internal_cycle_cost;
        gbl_cpu_trace_commit(run_options.trace_buffer, trace_records,
                             trace_record_count);
    }

    *result = run_result;
    return GBL_RUN_STATUS_OK;
}

/* Contract scaffold for the capture-aware runner; the capture path is added in
 * the following implementation change. Keeping the seam executable lets the
 * guest tracer assert the missing caller-visible PCM behavior. */
gbl_run_status gbl_cpu_run_audio(gbl_machine *machine,
                                 const gbl_run_options *options,
                                 gbl_run_result *result,
                                 gbl_audio_capture *capture,
                                 uint8_t *staging,
                                 size_t staging_capacity,
                                 uint64_t capture_end_cycle)
{
    (void)capture;
    (void)staging;
    (void)staging_capacity;
    (void)capture_end_cycle;
    return gbl_cpu_run(machine, options, result);
}
