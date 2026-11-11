#include "gamebloke/gamebloke.h"

#include "cpu_helpers.h"

#include <limits.h>

#define GBL_CPU_WORD_WIDTH 4u
#define GBL_CPU_CPSR_N UINT32_C(0x80000000)
#define GBL_CPU_CPSR_Z UINT32_C(0x40000000)
#define GBL_CPU_CPSR_C UINT32_C(0x20000000)
#define GBL_CPU_CPSR_V UINT32_C(0x10000000)
#define GBL_CPU_CPSR_T UINT32_C(0x00000020)
#define GBL_CPU_CPSR_MODE_MASK UINT32_C(0x0000001F)
#define GBL_CPU_MODE_FIQ UINT32_C(0x00000011)
#define GBL_CPU_MODE_IRQ UINT32_C(0x00000012)
#define GBL_CPU_MODE_SVC UINT32_C(0x00000013)
#define GBL_CPU_MODE_ABT UINT32_C(0x00000017)
#define GBL_CPU_MODE_UND UINT32_C(0x0000001B)

typedef enum gbl_cpu_unit_kind {
    GBL_CPU_UNIT_REFILL,
    GBL_CPU_UNIT_DATA_PROCESSING,
    GBL_CPU_UNIT_SEQUENTIAL,
    GBL_CPU_UNIT_BRANCH,
    GBL_CPU_UNIT_UNSUPPORTED,
    GBL_CPU_UNIT_UNDEFINED
} gbl_cpu_unit_kind;

/* One ephemeral decode is used both to preflight this unit and to execute it. */
typedef struct gbl_cpu_unit {
    gbl_cpu_unit_kind kind;
    uint32_t instruction_address;
    uint32_t opcode;
    uint32_t branch_target;
    uint32_t value;
    uint32_t flags;
    uint32_t flag_mask;
    uint8_t destination;
    uint8_t fetch_count;
    uint8_t internal_cycles;
    bool writes_destination;
    bool writes_flags;
    uint32_t fetch_address[2];
    gbl_bus_access_class fetch_class[2];
    uint64_t cycle_cost;
} gbl_cpu_unit;

static bool gbl_cpu_address_add(uint32_t address, uint32_t amount,
                                uint32_t *sum)
{
    if (sum == NULL || address > UINT32_MAX - amount) {
        return false;
    }
    *sum = address + amount;
    return true;
}

static bool gbl_cpu_fetch_address_valid(uint32_t address)
{
    return (address & UINT32_C(3)) == 0u &&
           address <= UINT32_MAX - UINT32_C(3);
}

static bool gbl_cpu_schedule_refill(gbl_cpu_unit *unit, uint32_t address)
{
    uint32_t second_address;

    if (unit == NULL || !gbl_cpu_fetch_address_valid(address) ||
        !gbl_cpu_address_add(address, UINT32_C(4), &second_address) ||
        !gbl_cpu_fetch_address_valid(second_address)) {
        return false;
    }
    unit->fetch_count = (uint8_t)2u;
    unit->fetch_address[0] = address;
    unit->fetch_address[1] = second_address;
    unit->fetch_class[0] = GBL_BUS_CLASS_NONSEQUENTIAL;
    unit->fetch_class[1] = GBL_BUS_CLASS_SEQUENTIAL;
    unit->cycle_cost = UINT64_C(2) + (uint64_t)unit->internal_cycles;
    return true;
}

static bool gbl_cpu_schedule_sequential_fetch(gbl_cpu_unit *unit,
                                              uint32_t address)
{
    if (unit == NULL || !gbl_cpu_fetch_address_valid(address)) {
        return false;
    }
    unit->fetch_count = (uint8_t)1u;
    unit->fetch_address[0] = address;
    unit->fetch_class[0] = GBL_BUS_CLASS_SEQUENTIAL;
    unit->cycle_cost = UINT64_C(1) + (uint64_t)unit->internal_cycles;
    return true;
}

static bool gbl_cpu_pipeline_is_empty(const gbl_cpu_state *cpu)
{
    return !cpu->pipeline[0].valid && !cpu->pipeline[1].valid;
}

static bool gbl_cpu_pipeline_is_ready(const gbl_cpu_state *cpu)
{
    uint32_t expected_second_address;

    if (!cpu->pipeline[0].valid || !cpu->pipeline[1].valid ||
        cpu->pipeline[0].width != (uint8_t)GBL_CPU_WORD_WIDTH ||
        cpu->pipeline[1].width != (uint8_t)GBL_CPU_WORD_WIDTH ||
        !gbl_cpu_fetch_address_valid(cpu->pipeline[0].address) ||
        !gbl_cpu_fetch_address_valid(cpu->pipeline[1].address) ||
        cpu->execution_address != cpu->pipeline[0].address ||
        !cpu->next_fetch_sequential ||
        !gbl_cpu_address_add(cpu->pipeline[0].address, UINT32_C(4),
                             &expected_second_address)) {
        return false;
    }
    return cpu->pipeline[1].address == expected_second_address;
}

static uint32_t gbl_cpu_read_register(const gbl_cpu_state *cpu,
                                      uint8_t index,
                                      uint32_t instruction_address)
{
    uint32_t mode;

    if (index == (uint8_t)15u) {
        return instruction_address + UINT32_C(8);
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

static void gbl_cpu_write_register(gbl_cpu_state *cpu, uint8_t index,
                                   uint32_t value)
{
    const uint32_t mode = cpu->cpsr & GBL_CPU_CPSR_MODE_MASK;

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

static bool gbl_cpu_is_arm_extension(uint32_t opcode)
{
    return (opcode & UINT32_C(0x0E000090)) == UINT32_C(0x00000090);
}

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
    bool is_compare = false;
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
    case 2u:  /* SUB */
    case 4u:  /* ADD */
    case 12u: /* ORR */
    case 13u: /* MOV */
        supported_operation = true;
        break;
    case 10u: /* CMP */
        supported_operation = true;
        is_compare = true;
        writes_destination = false;
        break;
    default:
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }

    if ((is_compare && (!set_flags || rd != 0u)) ||
        (operation == 13u && rn != 0u)) {
        unit->kind = GBL_CPU_UNIT_UNDEFINED;
        return true;
    }
    if (!supported_operation) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if (rd == (uint8_t)15u || (!immediate &&
        (opcode & UINT32_C(0x00000010)) != 0u &&
        (rm == (uint8_t)15u || ((opcode >> 8u) & UINT32_C(0x0F)) == 15u))) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if (operation == 13u && rd == (uint8_t)15u) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }

    condition_passed = gbl_cpu_condition_passed(
        (uint8_t)(opcode >> 28u), cpu->cpsr);
    if (!condition_passed) {
        unit->kind = GBL_CPU_UNIT_SEQUENTIAL;
        return gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address);
    }

    if (operation != 13u) {
        left = gbl_cpu_read_register(cpu, rn, unit->instruction_address);
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
        uint8_t amount;
        const gbl_shift_kind shift_kind =
            (gbl_shift_kind)((opcode >> 5u) & UINT32_C(3));

        operand = gbl_cpu_read_register(cpu, rm,
                                        unit->instruction_address);
        if (register_shift) {
            const uint8_t rs = (uint8_t)((opcode >> 8u) & UINT32_C(0x0F));

            amount = (uint8_t)(gbl_cpu_read_register(
                cpu, rs, unit->instruction_address) & UINT32_C(0xFF));
            unit->internal_cycles = (uint8_t)1u;
        } else {
            amount = (uint8_t)((opcode >> 7u) & UINT32_C(0x1F));
        }
        shifted = gbl_shift(shift_kind, operand, amount,
                            (cpu->cpsr & GBL_CPU_CPSR_C) != 0u,
                            register_shift);
        operand = shifted.value;
    }

    if (operation == 0u) {
        unit->value = left & operand;
    } else if (operation == 2u || is_compare) {
        arithmetic = gbl_sub_flags(left, operand);
        unit->value = arithmetic.value;
    } else if (operation == 4u) {
        arithmetic = gbl_add_flags(left, operand);
        unit->value = arithmetic.value;
    } else if (operation == 12u) {
        unit->value = left | operand;
    } else {
        unit->value = operand;
    }

    if (set_flags) {
        if (operation == 2u || operation == 4u || is_compare) {
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

    unit->destination = rd;
    unit->writes_destination = writes_destination;
    unit->writes_flags = set_flags;
    unit->flags = flags;
    unit->flag_mask = flag_mask;
    unit->kind = GBL_CPU_UNIT_DATA_PROCESSING;
    return gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address);
}

static bool gbl_cpu_decode(gbl_machine *machine, gbl_cpu_unit *unit)
{
    const gbl_cpu_state *cpu = &machine->cpu;
    const uint32_t opcode = cpu->pipeline[0].opcode;
    uint32_t next_fetch_address;

    if (unit == NULL) {
        return false;
    }
    *unit = (gbl_cpu_unit){ 0 };
    unit->instruction_address = cpu->execution_address;
    unit->opcode = opcode;

    if (gbl_cpu_pipeline_is_empty(cpu)) {
        if (cpu->next_fetch_sequential ||
            !gbl_cpu_fetch_address_valid(cpu->execution_address)) {
            return false;
        }
        unit->kind = GBL_CPU_UNIT_REFILL;
        return gbl_cpu_schedule_refill(unit, cpu->execution_address);
    }
    if (!gbl_cpu_pipeline_is_ready(cpu)) {
        return false;
    }
    if ((cpu->cpsr & GBL_CPU_CPSR_T) != 0u) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }
    if ((opcode >> 28u) == UINT32_C(0x0F)) {
        unit->kind = GBL_CPU_UNIT_UNDEFINED;
        return true;
    }
    if (!gbl_cpu_address_add(cpu->pipeline[1].address, UINT32_C(4),
                             &next_fetch_address)) {
        unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
        return true;
    }

    if ((opcode & UINT32_C(0x0E000000)) == UINT32_C(0x0A000000)) {
        uint32_t offset = opcode & UINT32_C(0x00FFFFFF);

        if ((opcode >> 28u) != UINT32_C(0x0E) ||
            (opcode & UINT32_C(0x01000000)) != 0u) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            return true;
        }
        if ((offset & UINT32_C(0x00800000)) != 0u) {
            offset |= UINT32_C(0xFF000000);
        }
        offset <<= 2u;
        unit->branch_target = unit->instruction_address + UINT32_C(8) + offset;
        unit->kind = GBL_CPU_UNIT_BRANCH;
        return gbl_cpu_schedule_refill(unit, unit->branch_target);
    }
    if ((opcode & UINT32_C(0x0C000000)) == 0u) {
        if (gbl_cpu_decode_data_processing(cpu, unit, next_fetch_address)) {
            return true;
        }
    }

    unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
    return true;
}

static bool gbl_cpu_preflight(const gbl_machine *machine,
                              const gbl_cpu_unit *unit,
                              uint64_t remaining_budget,
                              gbl_run_reason *reason)
{
    uint8_t index;

    if (reason == NULL) {
        return false;
    }
    *reason = GBL_RUN_REASON_NONE;
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
    if (unit->fetch_count == 0u ||
        unit->cycle_cost != (uint64_t)unit->fetch_count +
                                (uint64_t)unit->internal_cycles) {
        *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
        return false;
    }
    for (index = 0u; index < unit->fetch_count; ++index) {
        if (!gbl_cpu_fetch_address_valid(unit->fetch_address[index]) ||
            (unit->fetch_class[index] != GBL_BUS_CLASS_NONSEQUENTIAL &&
             unit->fetch_class[index] != GBL_BUS_CLASS_SEQUENTIAL)) {
            *reason = GBL_RUN_REASON_INVALID_CPU_STATE;
            return false;
        }
    }
    return true;
}

static bool gbl_cpu_fetch_unit(gbl_machine *machine,
                               const gbl_cpu_unit *unit,
                               gbl_cpu_pipeline_entry fetched[2])
{
    const uint64_t saved_cycles = machine->master_cycles;
    const uint32_t saved_open_bus = machine->open_bus;
    uint8_t index;

    for (index = 0u; index < unit->fetch_count; ++index) {
        const gbl_bus_result result = gbl_bus_read(
            machine, unit->fetch_address[index], (uint8_t)GBL_CPU_WORD_WIDTH,
            unit->fetch_class[index]);

        if (result.status != GBL_BUS_STATUS_OK) {
            /* Bus reads only affect time and the open-bus latch. Restore both
             * if an unexpected checked-bus error interrupts this unit. */
            machine->master_cycles = saved_cycles;
            machine->open_bus = saved_open_bus;
            return false;
        }
        fetched[index].address = unit->fetch_address[index];
        fetched[index].opcode = result.value;
        fetched[index].width = (uint8_t)GBL_CPU_WORD_WIDTH;
        fetched[index].valid = true;
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

static gbl_run_reason gbl_cpu_stop_result(gbl_cpu_stop_reason reason)
{
    switch (reason) {
    case GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION:
        return GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION;
    case GBL_CPU_STOP_UNDEFINED_INSTRUCTION:
        return GBL_RUN_REASON_UNDEFINED_INSTRUCTION;
    case GBL_CPU_STOP_COUNTER_OVERFLOW:
        return GBL_RUN_REASON_COUNTER_OVERFLOW;
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
                               sizeof(*result))) {
        return GBL_RUN_STATUS_INVALID_ARGUMENT;
    }
    run_options = *options;

    for (;;) {
        gbl_cpu_unit unit;
        gbl_run_reason preflight_reason;
        gbl_cpu_pipeline_entry fetched[2] = { { 0u, 0u, 0u, false },
                                              { 0u, 0u, 0u, false } };

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
        if (unit.kind == GBL_CPU_UNIT_UNSUPPORTED ||
            unit.kind == GBL_CPU_UNIT_UNDEFINED) {
            machine->cpu.stopped = true;
            machine->cpu.stop_reason =
                unit.kind == GBL_CPU_UNIT_UNDEFINED
                    ? GBL_CPU_STOP_UNDEFINED_INSTRUCTION
                    : GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
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
        if (!gbl_cpu_fetch_unit(machine, &unit, fetched)) {
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_BUS_ERROR;
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

        if (unit.kind == GBL_CPU_UNIT_REFILL) {
            machine->cpu.pipeline[0] = fetched[0];
            machine->cpu.pipeline[1] = fetched[1];
        } else if (unit.kind == GBL_CPU_UNIT_BRANCH) {
            machine->cpu.execution_address = unit.branch_target;
            machine->cpu.pipeline[0] = fetched[0];
            machine->cpu.pipeline[1] = fetched[1];
            machine->cpu.retired_instructions++;
            run_result.instructions++;
        } else {
            if (unit.kind == GBL_CPU_UNIT_DATA_PROCESSING &&
                unit.writes_destination) {
                gbl_cpu_write_register(&machine->cpu, unit.destination,
                                       unit.value);
            }
            if (unit.writes_flags) {
                machine->cpu.cpsr = (machine->cpu.cpsr & ~unit.flag_mask) |
                                    (unit.flags & unit.flag_mask);
            }
            machine->cpu.execution_address = machine->cpu.pipeline[1].address;
            machine->cpu.pipeline[0] = machine->cpu.pipeline[1];
            machine->cpu.pipeline[1] = fetched[0];
            machine->cpu.retired_instructions++;
            run_result.instructions++;
        }
        machine->cpu.next_fetch_sequential = true;
        completed_units++;
        run_result.cycles += unit.cycle_cost;
    }

    *result = run_result;
    return GBL_RUN_STATUS_OK;
}
