#include "gamebloke/gamebloke.h"

#include <limits.h>

#define GBL_CPU_WORD_WIDTH 4u
#define GBL_CPU_CPSR_T UINT32_C(0x00000020)
#define GBL_CPU_CPSR_MODE_MASK UINT32_C(0x0000001F)
#define GBL_CPU_MODE_FIQ UINT32_C(0x00000011)
#define GBL_CPU_MODE_IRQ UINT32_C(0x00000012)
#define GBL_CPU_MODE_SVC UINT32_C(0x00000013)
#define GBL_CPU_MODE_ABT UINT32_C(0x00000017)
#define GBL_CPU_MODE_UND UINT32_C(0x0000001B)

typedef enum gbl_cpu_unit_kind {
    GBL_CPU_UNIT_REFILL,
    GBL_CPU_UNIT_MOV_REGISTER,
    GBL_CPU_UNIT_BRANCH,
    GBL_CPU_UNIT_UNSUPPORTED
} gbl_cpu_unit_kind;

/* One ephemeral decode is used both to preflight this unit and to execute it. */
typedef struct gbl_cpu_unit {
    gbl_cpu_unit_kind kind;
    uint32_t instruction_address;
    uint32_t opcode;
    uint32_t branch_target;
    uint32_t value;
    uint8_t destination;
    uint8_t fetch_count;
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
    unit->cycle_cost = UINT64_C(2);
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
    unit->cycle_cost = UINT64_C(1);
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

    if ((opcode >> 28u) == UINT32_C(0xE) &&
        (opcode & UINT32_C(0x0E000000)) == 0u &&
        (opcode & UINT32_C(0x01E00000)) == UINT32_C(0x01A00000) &&
        (opcode & UINT32_C(0x00100000)) == 0u &&
        (opcode & UINT32_C(0x000F0000)) == 0u &&
        (opcode & UINT32_C(0x00000FF0)) == 0u) {
        const uint8_t destination = (uint8_t)((opcode >> 12u) & 0x0Fu);
        const uint8_t source = (uint8_t)(opcode & 0x0Fu);

        if (destination == (uint8_t)15u ||
            !gbl_cpu_address_add(cpu->pipeline[1].address, UINT32_C(4),
                                 &next_fetch_address)) {
            unit->kind = GBL_CPU_UNIT_UNSUPPORTED;
            return true;
        }
        unit->kind = GBL_CPU_UNIT_MOV_REGISTER;
        unit->destination = destination;
        unit->value = gbl_cpu_read_register(cpu, source,
                                            unit->instruction_address);
        if (!gbl_cpu_schedule_sequential_fetch(unit, next_fetch_address)) {
            return false;
        }
        return true;
    }

    if ((opcode >> 28u) == UINT32_C(0xE) &&
        (opcode & UINT32_C(0x0E000000)) == UINT32_C(0x0A000000) &&
        (opcode & UINT32_C(0x01000000)) == 0u) {
        uint32_t offset = opcode & UINT32_C(0x00FFFFFF);

        if ((offset & UINT32_C(0x00800000)) != 0u) {
            offset |= UINT32_C(0xFF000000);
        }
        offset <<= 2u;
        unit->branch_target = unit->instruction_address + UINT32_C(8) + offset;
        unit->kind = GBL_CPU_UNIT_BRANCH;
        if (!gbl_cpu_schedule_refill(unit, unit->branch_target)) {
            return false;
        }
        return true;
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
        unit->cycle_cost != (uint64_t)unit->fetch_count) {
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
        !machine->initialized ||
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
            run_result.reason =
                machine->cpu.stop_reason ==
                        GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION
                    ? GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION
                    : GBL_RUN_REASON_BUS_ERROR;
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
        if (unit.kind == GBL_CPU_UNIT_UNSUPPORTED) {
            machine->cpu.stopped = true;
            machine->cpu.stop_reason =
                GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION;
            run_result.termination = GBL_RUN_STOPPED;
            run_result.reason = GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION;
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

        if (unit.kind == GBL_CPU_UNIT_REFILL) {
            machine->cpu.pipeline[0] = fetched[0];
            machine->cpu.pipeline[1] = fetched[1];
        } else if (unit.kind == GBL_CPU_UNIT_BRANCH) {
            machine->cpu.execution_address = unit.branch_target;
            machine->cpu.pipeline[0] = fetched[0];
            machine->cpu.pipeline[1] = fetched[1];
            machine->cpu.retired_instructions++;
            run_result.instructions++;
        } else if (unit.kind == GBL_CPU_UNIT_MOV_REGISTER) {
            machine->cpu.r0_7[unit.destination] = unit.value;
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
