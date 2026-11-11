#include "gamebloke/gamebloke.h"

static void gbl_machine_clear_mutable_state(gbl_machine *machine)
{
    size_t index;

    machine->master_cycles = 0u;
    machine->open_bus = 0u;
    machine->dispcnt = 0u;
    machine->cpu = (gbl_cpu_state){ 0 };
    machine->cpu.cpsr = UINT32_C(0x000000DF);
    machine->cpu.execution_address = UINT32_C(0x08000000);
    for (index = 0u; index < GBL_EWRAM_SIZE; ++index) {
        machine->ewram[index] = 0u;
    }
    for (index = 0u; index < GBL_IWRAM_SIZE; ++index) {
        machine->iwram[index] = 0u;
    }
    for (index = 0u; index < GBL_PRAM_SIZE; ++index) {
        machine->pram[index] = 0u;
    }
    for (index = 0u; index < GBL_VRAM_SIZE; ++index) {
        machine->vram[index] = 0u;
    }
    for (index = 0u; index < GBL_OAM_SIZE; ++index) {
        machine->oam[index] = 0u;
    }
}

static bool gbl_machine_allocator_aliases_storage(
    const gbl_machine *machine, const gbl_allocator *allocator)
{
    const unsigned char *storage = (const unsigned char *)machine;
    size_t index;

    for (index = 0u; index < sizeof(*machine); ++index) {
        if ((const void *)(storage + index) == (const void *)allocator) {
            return true;
        }
    }
    return false;
}

gbl_bus_status gbl_machine_init(gbl_machine *machine,
                                const gbl_allocator *allocator)
{
    gbl_allocator supplied = { NULL, NULL, NULL };

    if (machine == NULL) {
        return GBL_BUS_STATUS_INVALID_ARGUMENT;
    }
    if (allocator != NULL) {
        if (gbl_machine_allocator_aliases_storage(machine, allocator)) {
            return GBL_BUS_STATUS_INVALID_ARGUMENT;
        }
        supplied = *allocator;
        if ((supplied.allocate == NULL) != (supplied.release == NULL)) {
            return GBL_BUS_STATUS_INVALID_ARGUMENT;
        }
    }

    machine->initialized = false;
    machine->rom = NULL;
    machine->rom_length = 0u;
    machine->header_diagnostics = (gbl_header_diagnostics){ 0 };
    machine->allocator = supplied;
    gbl_machine_clear_mutable_state(machine);
    machine->initialized = true;
    return GBL_BUS_STATUS_OK;
}

void gbl_machine_reset(gbl_machine *machine)
{
    if (machine == NULL || !machine->initialized) {
        return;
    }
    gbl_machine_clear_mutable_state(machine);
}

void gbl_machine_dispose(gbl_machine *machine)
{
    uint8_t *detached_rom;
    size_t detached_rom_length;
    gbl_allocator detached_allocator;

    if (machine == NULL || !machine->initialized) {
        return;
    }

    detached_rom = machine->rom;
    detached_rom_length = machine->rom_length;
    detached_allocator = machine->allocator;
    machine->rom = NULL;
    machine->rom_length = 0u;
    machine->header_diagnostics = (gbl_header_diagnostics){ 0 };
    machine->allocator.context = NULL;
    machine->allocator.allocate = NULL;
    machine->allocator.release = NULL;
    gbl_machine_clear_mutable_state(machine);
    machine->initialized = false;

    if (detached_rom != NULL && detached_allocator.release != NULL) {
        detached_allocator.release(detached_allocator.context, detached_rom,
                                   detached_rom_length);
    }
}
