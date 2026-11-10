#include "gamebloke/gamebloke.h"

gbl_load_result gbl_load_rom(gbl_machine *machine, const uint8_t *image,
                             size_t length)
{
    gbl_load_result result = { GBL_LOAD_STATUS_OK, GBL_LOAD_REASON_NONE };
    uint8_t *candidate;
    uint8_t *old_rom;
    size_t old_rom_length;
    gbl_allocator allocator;

    if (machine == NULL || !machine->initialized) {
        result.status = GBL_LOAD_STATUS_INVALID_ARGUMENT;
        result.reason = GBL_LOAD_REASON_ARGUMENT;
        return result;
    }
    if (length == 0u) {
        result.status = GBL_LOAD_STATUS_REFUSED;
        result.reason = GBL_LOAD_REASON_ROM_ZERO;
        return result;
    }
    if (length < 0xC0u) {
        result.status = GBL_LOAD_STATUS_REFUSED;
        result.reason = GBL_LOAD_REASON_ROM_SHORT;
        return result;
    }
    if (length > 0x02000000u) {
        result.status = GBL_LOAD_STATUS_REFUSED;
        result.reason = GBL_LOAD_REASON_ROM_TOO_LARGE;
        return result;
    }
    if ((length & 1u) != 0u) {
        result.status = GBL_LOAD_STATUS_REFUSED;
        result.reason = GBL_LOAD_REASON_ROM_ODD;
        return result;
    }
    if (image == NULL || machine->allocator.allocate == NULL ||
        machine->allocator.release == NULL) {
        result.status = GBL_LOAD_STATUS_INVALID_ARGUMENT;
        result.reason = GBL_LOAD_REASON_ARGUMENT;
        return result;
    }

    allocator = machine->allocator;
    candidate = allocator.allocate(allocator.context, length);
    if (candidate == NULL) {
        result.status = GBL_LOAD_STATUS_RESOURCE_FAILURE;
        result.reason = GBL_LOAD_REASON_ALLOCATION;
        return result;
    }

    {
        size_t index;
        for (index = 0u; index < length; ++index) {
            candidate[index] = image[index];
        }
    }

    old_rom = machine->rom;
    old_rom_length = machine->rom_length;
    machine->rom = candidate;
    machine->rom_length = length;
    gbl_machine_reset(machine);

    if (old_rom != NULL) {
        allocator.release(allocator.context, old_rom, old_rom_length);
    }
    return result;
}

const uint8_t *gbl_rom_data(const gbl_machine *machine)
{
    if (machine == NULL || !machine->initialized) {
        return NULL;
    }
    return machine->rom;
}

size_t gbl_rom_size(const gbl_machine *machine)
{
    if (machine == NULL || !machine->initialized) {
        return 0u;
    }
    return machine->rom_length;
}
