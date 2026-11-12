#include "gamebloke/gamebloke.h"

#include "cpu_helpers.h"

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
#define GBL_CPU_FETCH_SLOT_MAX 2u
#define GBL_CPU_DATA_SLOT_MAX 16u
#define GBL_CPU_RESULT_SLOT_MAX GBL_CPU_DATA_SLOT_MAX
#define GBL_CPU_REGISTER_EFFECT_MAX 16u
#define GBL_CPU_STATUS_EFFECT_MAX 1u
#define GBL_CPU_TRACE_UNIT_RECORD_MAX \
    (1u + GBL_CPU_DATA_SLOT_MAX + GBL_CPU_FETCH_SLOT_MAX)

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
    GBL_CPU_UNIT_UNDEFINED,
    GBL_CPU_UNIT_EXCEPTION
} gbl_cpu_unit_kind;

typedef enum gbl_cpu_data_operation {
    GBL_CPU_DATA_READ,
    GBL_CPU_DATA_WRITE
} gbl_cpu_data_operation;

typedef struct gbl_cpu_data_slot {
    gbl_cpu_data_operation operation;
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
    GBL_CPU_PC_READ_REGISTER_SHIFT_OPERAND = 1,
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
    uint8_t data_slot_count;
    uint8_t result_count;
    uint8_t register_effect_count;
    uint8_t status_effect_count;
    bool changes_state;
    bool thumb_state;
    bool fetch_address_from_result;
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

static bool gbl_cpu_unit_cost(const gbl_cpu_unit *unit, uint64_t *cost)
{
    uint64_t total;

    if (unit == NULL || cost == NULL ||
        unit->fetch_count > GBL_CPU_FETCH_SLOT_MAX ||
        unit->data_slot_count > GBL_CPU_DATA_SLOT_MAX) {
        return false;
    }
    total = (uint64_t)unit->fetch_count;
    if ((uint64_t)unit->internal_cycles > UINT64_MAX - total) {
        return false;
    }
    total += (uint64_t)unit->internal_cycles;
    if ((uint64_t)unit->data_slot_count > UINT64_MAX - total) {
        return false;
    }
    *cost = total + (uint64_t)unit->data_slot_count;
    return true;
}

static bool gbl_cpu_refresh_cycle_cost(gbl_cpu_unit *unit)
{
    uint64_t cost;

    if (!gbl_cpu_unit_cost(unit, &cost)) {
        return false;
    }
    unit->cycle_cost = cost;
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

/* ARMv4T LDR-to-PC continues in ARM state. Its loaded word result is the
 * source for both refill addresses; the low bits are ignored, and the second
 * address wraps in the 32-bit address space. Preflight validates this bounded
 * symbolic form before the data read, then execution resolves it once from
 * that read's rotated CPU result. */
static bool gbl_cpu_schedule_result_refill(gbl_cpu_unit *unit,
                                           uint8_t result_index)
{
    if (unit == NULL || result_index >= unit->result_count) {
        return false;
    }
    unit->kind = GBL_CPU_UNIT_BRANCH;
    unit->changes_state = false;
    unit->thumb_state = false;
    unit->fetch_address_from_result = true;
    unit->fetch_result_index = result_index;
    unit->fetch_result_mask = ~UINT32_C(3);
    unit->fetch_count = (uint8_t)GBL_CPU_FETCH_SLOT_MAX;
    unit->fetch_width = (uint8_t)GBL_CPU_WORD_WIDTH;
    unit->fetch_result_addend[0] = 0u;
    unit->fetch_result_addend[1] = (uint32_t)GBL_CPU_WORD_WIDTH;
    unit->fetch_class[0] = GBL_BUS_CLASS_NONSEQUENTIAL;
    unit->fetch_class[1] = GBL_BUS_CLASS_SEQUENTIAL;
    return gbl_cpu_refresh_cycle_cost(unit);
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
        /* LDR PC intentionally builds this modulo refill pair. The decoder
         * turns it into the ARM sequential-wrap stop before executing it. */
        return gbl_cpu_pipeline_is_arm_sequential_wrap(cpu);
    }
    return cpu->pipeline[1].address == expected_second_address;
}

static uint32_t gbl_cpu_read_register(const gbl_cpu_state *cpu,
                                      uint8_t index,
                                      uint32_t instruction_address,
                                      gbl_cpu_pc_read_context context)
{
    uint32_t mode;

    if (index == (uint8_t)15u) {
        const bool thumb = (cpu->cpsr & GBL_CPU_CPSR_T) != 0u;

        if (context == GBL_CPU_PC_READ_THUMB_ALIGNED_ADDRESS) {
            return (instruction_address + UINT32_C(4)) & ~UINT32_C(3);
        }
        return instruction_address +
               (thumb ? UINT32_C(4)
                      : context == GBL_CPU_PC_READ_REGISTER_SHIFT_OPERAND ||
                                context == GBL_CPU_PC_READ_STORE_SOURCE
                            ? UINT32_C(12)
                            : UINT32_C(8));
    }
    if (index <= (uint8_t)7u) {
        return cpu->r0_7[index];
    }
    mode = cpu->cpsr & GBL_CPU_CPSR_MODE_MASK;
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

static bool gbl_cpu_is_arm_extension(uint32_t opcode)
{
    return (opcode & UINT32_C(0x0E000090)) == UINT32_C(0x00000090);
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
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if (!immediate && (opcode & UINT32_C(0x00000090)) ==
                           UINT32_C(0x00000090)) {
        unit->kind = GBL_CPU_UNIT_UNDEFINED;
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
        unit->kind = GBL_CPU_UNIT_UNDEFINED;
        return true;
    }
    if (!supported_operation) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if (!immediate && (opcode & UINT32_C(0x00000010)) != 0u &&
        ((opcode >> 8u) & UINT32_C(0x0F)) == 15u) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    condition_passed = gbl_cpu_condition_passed(
        (uint8_t)(opcode >> 28u), cpu->cpsr);
    if (!condition_passed) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        return gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                                 false);
    }

    if (operation != 13u) {
        left = gbl_cpu_read_register(cpu, rn, unit->instruction_address,
                                    GBL_CPU_PC_READ_NORMAL);
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
        const bool register_shift =
            (opcode & UINT32_C(0x00000010)) != 0u;
        const uint8_t form = (uint8_t)(register_shift ? 1u : 0u);
        uint8_t amount;
        const gbl_shift_kind shift_kind =
            (gbl_shift_kind)((opcode >> 5u) & UINT32_C(3));

        operand = gbl_cpu_read_register(
            cpu, rm, unit->instruction_address,
            register_shift ? GBL_CPU_PC_READ_REGISTER_SHIFT_OPERAND
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

static void gbl_cpu_schedule_exception(gbl_cpu_unit *unit,
                                      gbl_exception_kind kind)
{
    unit->kind = GBL_CPU_UNIT_EXCEPTION;
    unit->exception_kind = kind;
    unit->internal_cycles = (uint8_t)1u;
    unit->cycle_cost = UINT64_C(1);
}

static bool gbl_cpu_finalize_decoded(gbl_cpu_unit *unit, bool decoded)
{
    if (!decoded) {
        return false;
    }
    if (unit->kind == GBL_CPU_UNIT_UNDEFINED) {
        gbl_cpu_schedule_exception(unit, GBL_EXCEPTION_UNDEFINED);
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
        (opcode & UINT32_C(0x00400000)) == 0u;
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

    /* Snapshot every source and long accumulator word before creating any
     * destination effects. ARM7TDMI permits source/destination overlap. */
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
         !gbl_cpu_schedule_result_refill(unit, pc_result_index)) ||
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
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
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

static bool gbl_cpu_decode_thumb(const gbl_cpu_state *cpu,
                                 gbl_cpu_unit *unit,
                                 uint32_t next_fetch_address)
{
    const uint16_t opcode = (uint16_t)unit->opcode;
    const uint32_t instruction_address = unit->instruction_address;

    if ((opcode & UINT16_C(0xFF00)) == UINT16_C(0xDF00)) {
        gbl_cpu_schedule_exception(unit, GBL_EXCEPTION_SWI);
        return true;
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

        if (operation == 2u) {
            if (destination == (uint8_t)15u) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
                return true;
            }
            unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
            if (!gbl_cpu_add_literal_register_effect(
                    unit, destination, cpu->cpsr,
                    gbl_cpu_read_register(cpu, source, instruction_address,
                                          GBL_CPU_PC_READ_NORMAL))) {
                unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
                return true;
            }
            return gbl_cpu_schedule_sequential_fetch(unit,
                                                      next_fetch_address, true);
        }
        if (operation == 3u) {
            uint32_t target_value;
            uint32_t target;
            bool thumb;

            if ((opcode & UINT16_C(0x0087)) != 0u) {
                unit->kind = GBL_CPU_UNIT_UNDEFINED;
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

        if (!gbl_cpu_data_span_valid(address,
                                     (uint8_t)GBL_CPU_WORD_WIDTH)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            return true;
        }
        if (!gbl_cpu_prepare_data_transfer(
                unit, address, (uint8_t)GBL_CPU_WORD_WIDTH, true,
                destination, 0u, false, cpu->cpsr) ||
            !gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               true)) {
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
        if (!gbl_cpu_data_span_valid(address, width)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            return true;
        }
        if (!gbl_cpu_prepare_data_transfer(
                unit, address, width, load, destination,
                load ? 0u : cpu->r0_7[destination], load && !byte,
                cpu->cpsr) ||
            !gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address,
                                               true)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        }
        return true;
    }

    unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
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
    if (gbl_cpu_pipeline_is_arm_sequential_wrap(cpu)) {
        /* DDI0100I A2.7.1: executing at FFFFFFFC and then at 0 is
         * unpredictable. Keep the internally generated refill pair valid,
         * then use the existing unpredictable-instruction stop before any
         * instruction, cycle, or trace effect at that boundary. */
        unit->kind = GBL_CPU_UNIT_UNPREDICTABLE;
        return true;
    }
    if (!gbl_cpu_next_sequential_address(cpu, thumb, &next_fetch_address)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
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
        gbl_cpu_schedule_exception(unit, GBL_EXCEPTION_SWI);
        return true;
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
        gbl_cpu_schedule_exception(unit, GBL_EXCEPTION_COPROCESSOR);
        return true;
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
    if ((opcode & UINT32_C(0x0C000000)) == 0u) {
        if (gbl_cpu_decode_data_processing(cpu, unit, next_fetch_address)) {
            return gbl_cpu_finalize_decoded(unit, true);
        }
    }
    if (gbl_cpu_decode_arm_single_transfer(cpu, unit, next_fetch_address)) {
        return gbl_cpu_finalize_decoded(unit, true);
    }

    unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
    return true;
}

static bool gbl_cpu_preflight(const gbl_machine *machine,
                              const gbl_cpu_unit *unit,
                              uint64_t remaining_budget,
                              gbl_run_reason *reason)
{
    const bool has_data_access = unit->data_slot_count != 0u;
    const bool exception = unit->kind == GBL_CPU_UNIT_EXCEPTION;
    uint64_t expected_cost = 0u;
    bool result_seen[GBL_CPU_RESULT_SLOT_MAX] = { false };
    uint8_t index;

    if (reason == NULL) {
        return false;
    }
    *reason = GBL_RUN_REASON_NONE;
    if (!gbl_cpu_unit_cost(unit, &expected_cost) ||
        unit->result_count > GBL_CPU_RESULT_SLOT_MAX ||
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
    if (unit->kind != GBL_CPU_UNIT_REFILL &&
        machine->cpu.retired_instructions == UINT64_MAX) {
        *reason = GBL_RUN_REASON_COUNTER_OVERFLOW;
        return false;
    }
    if (unit->cycle_cost != expected_cost ||
        (unit->fetch_address_from_result &&
         (unit->kind != GBL_CPU_UNIT_BRANCH || unit->changes_state ||
          unit->fetch_count != (uint8_t)GBL_CPU_FETCH_SLOT_MAX ||
          unit->fetch_width != (uint8_t)GBL_CPU_WORD_WIDTH ||
          unit->internal_cycles != 0u ||
          unit->fetch_result_mask != ~UINT32_C(3) ||
          unit->fetch_result_index >= unit->result_count ||
          unit->data_slot_count != 1u || unit->result_count != 1u ||
          unit->data_slots[0].operation != GBL_CPU_DATA_READ ||
          unit->data_slots[0].width != (uint8_t)GBL_CPU_WORD_WIDTH ||
          unit->data_slots[0].result_index != unit->fetch_result_index ||
          !unit->data_slots[0].rotate_word_read ||
          unit->fetch_result_addend[0] != 0u ||
          unit->fetch_result_addend[1] !=
              (uint32_t)GBL_CPU_WORD_WIDTH ||
          unit->fetch_class[0] != GBL_BUS_CLASS_NONSEQUENTIAL ||
          unit->fetch_class[1] != GBL_BUS_CLASS_SEQUENTIAL)) ||
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
        if (slot->operation == GBL_CPU_DATA_READ) {
            const uint8_t transform_count =
                (uint8_t)(slot->rotate_word_read ? 1u : 0u) +
                (uint8_t)(slot->rotate_halfword_read ? 1u : 0u) +
                (uint8_t)(slot->sign_extend_byte_read ? 1u : 0u) +
                (uint8_t)(slot->sign_extend_halfword_read ? 1u : 0u);

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
            /* Preflight has proved this mask/addend form always produces a
             * valid aligned word address. Unsigned addition intentionally
             * wraps the second refill at 2^32. */
            fetch_address =
                (unit->results[unit->fetch_result_index] &
                 unit->fetch_result_mask) + unit->fetch_result_addend[index];
            unit->fetch_address[index] = fetch_address;
            if (index == 0u && unit->kind == GBL_CPU_UNIT_BRANCH) {
                unit->branch_target = fetch_address;
            }
        }
        const gbl_bus_result result = gbl_bus_read(
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

    if (unit->kind != GBL_CPU_UNIT_REFILL &&
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
        !gbl_cpu_test_build_maximum_unit(&unit)) {
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

static gbl_run_reason gbl_cpu_stop_result(gbl_cpu_stop_reason reason)
{
    switch (reason) {
    case GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION:
        return GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION;
    case GBL_CPU_STOP_COUNTER_OVERFLOW:
        return GBL_RUN_REASON_COUNTER_OVERFLOW;
    case GBL_CPU_STOP_RESET_NO_BIOS:
    case GBL_CPU_STOP_UNDEFINED_NO_BIOS:
    case GBL_CPU_STOP_SWI_NO_BIOS:
    case GBL_CPU_STOP_PREFETCH_ABORT_NO_BIOS:
    case GBL_CPU_STOP_DATA_ABORT_NO_BIOS:
    case GBL_CPU_STOP_IRQ_NO_BIOS:
    case GBL_CPU_STOP_FIQ_NO_BIOS:
    case GBL_CPU_STOP_COPROCESSOR_NO_BIOS:
        return GBL_RUN_REASON_GUEST_STOP;
    case GBL_CPU_STOP_NONE:
    case GBL_CPU_STOP_BUS_ERROR:
    default:
        return GBL_RUN_REASON_BUS_ERROR;
    }
}

gbl_run_status gbl_cpu_run(gbl_machine *machine,
                           const gbl_run_options *options,
                           gbl_run_result *result)
{
    gbl_run_options run_options;
    gbl_run_result run_result = {
        GBL_RUN_TIMEOUT, GBL_RUN_REASON_CYCLE_BUDGET, 0u, 0u
    };
    uint64_t completed_units = 0u;

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
        gbl_cpu_unit unit;
        gbl_run_reason preflight_reason;
        gbl_cpu_pipeline_entry fetched[2] = { { 0u, 0u, 0u, false },
                                              { 0u, 0u, 0u, false } };
        gbl_trace_record trace_records[GBL_CPU_TRACE_UNIT_RECORD_MAX] = {
            { 0 }
        };
        size_t trace_record_count = 0u;
        uint64_t unit_start_cycle;

        if (machine->cpu.stopped) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = gbl_cpu_stop_result(machine->cpu.stop_reason);
            break;
        }
        if (run_options.cycle_budget == 0u) {
            break;
        }
        if (run_options.unit_limit != 0u &&
            completed_units >= run_options.unit_limit) {
            run_result.termination = GBL_RUN_PROGRESS;
            run_result.reason = GBL_RUN_REASON_UNIT_LIMIT;
            break;
        }
        if (!gbl_cpu_decode(machine, &unit)) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            break;
        }
        unit_start_cycle = machine->master_cycles;
        if (unit.kind == GBL_CPU_UNIT_UNSUPPORTED ||
            unit.kind == GBL_CPU_UNIT_UNPREDICTABLE) {
            machine->cpu.stopped = true;
            machine->cpu.stop_reason = GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = gbl_cpu_stop_result(machine->cpu.stop_reason);
            break;
        }
        if (!gbl_cpu_preflight(machine, &unit,
                               run_options.cycle_budget - run_result.cycles,
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
        if (unit.kind != GBL_CPU_UNIT_REFILL) {
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
            const gbl_cpu_exception_context context = {
                unit.instruction_address,
                machine->cpu.pipeline[1].address,
                unit.instruction_address,
                (machine->cpu.cpsr & GBL_CPU_CPSR_T) != 0u
            };
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
            machine->cpu.retired_instructions++;
            run_result.instructions++;
            run_result.cycles += unit.cycle_cost;
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
                        GBL_BUS_CLASS_NONSEQUENTIAL);
                    if (data_result.status != GBL_BUS_STATUS_OK) {
                        run_result.termination = GBL_RUN_STOPPED;
                        run_result.reason = GBL_RUN_REASON_BUS_ERROR;
                        machine->cpu.stopped = true;
                        machine->cpu.stop_reason = GBL_CPU_STOP_BUS_ERROR;
                        break;
                    }
                    value = data_result.value;
                    if (slot->rotate_word_read) {
                        const uint8_t rotate =
                            (uint8_t)((uint32_t)slot->alignment_lane * 8u);
                        const gbl_shift_result rotated = gbl_shift(
                            GBL_SHIFT_ROR, value, rotate, false, true);

                        value = rotated.value;
                    }
                    if (slot->rotate_halfword_read &&
                        slot->alignment_lane != 0u) {
                        value = gbl_shift(GBL_SHIFT_ROR, value, 8u, false,
                                          true).value;
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
                    unit.results[slot->result_index] = value;
                } else {
                    data_result = gbl_bus_write(
                        machine, slot->address, slot->width,
                        GBL_BUS_CLASS_NONSEQUENTIAL, slot->write_value);
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
            gbl_cpu_commit_effects(&machine->cpu, &unit);
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
        completed_units++;
        run_result.cycles += unit.cycle_cost;
        gbl_cpu_trace_commit(run_options.trace_buffer, trace_records,
                             trace_record_count);
    }

    *result = run_result;
    return GBL_RUN_STATUS_OK;
}
