#include "gamebloke/gamebloke.h"

static gbl_header_diagnostics gbl_header_diagnostics_from_candidate(
    const uint8_t *candidate)
{
    static const uint8_t zero_logo_window[0x9Cu] = { 0 };
    gbl_header_diagnostics diagnostics = { 0 };
    uint8_t checksum = 0u;
    bool ordinary_game_code = true;
    size_t index;

    diagnostics.loaded = true;
    diagnostics.canonical_logo = GBL_HEADER_LOGO_UNCHECKED;
    diagnostics.logo_reason = GBL_HEADER_LOGO_REASON_OWNER_PENDING;
    diagnostics.logo_window =
        gbl_header_compare_bytes(candidate + 0x04u, zero_logo_window,
                                 sizeof(zero_logo_window)) ==
                GBL_HEADER_COMPARE_EQUAL
            ? GBL_HEADER_LOGO_WINDOW_ZERO
            : GBL_HEADER_LOGO_WINDOW_NONZERO;

    for (index = 0xA0u; index <= 0xBCu; ++index) {
        checksum = (uint8_t)(checksum - candidate[index]);
    }
    checksum = (uint8_t)(checksum - 0x19u);
    diagnostics.checksum = candidate[0xBDu] == checksum
                               ? GBL_HEADER_CHECKSUM_OK
                               : GBL_HEADER_CHECKSUM_MISMATCH;

    for (index = 0u; index < 4u; ++index) {
        const uint8_t character = candidate[0xACu + index];
        const bool uppercase = character >= (uint8_t)'A' &&
                               character <= (uint8_t)'Z';
        const bool digit = character >= (uint8_t)'0' &&
                           character <= (uint8_t)'9';

        if (!uppercase && !digit) {
            ordinary_game_code = false;
        }
    }
    diagnostics.game_code = ordinary_game_code
                                ? GBL_HEADER_GAME_CODE_ORDINARY
                                : GBL_HEADER_GAME_CODE_UNUSUAL;
    return diagnostics;
}

gbl_header_compare_result gbl_header_compare_bytes(
    const uint8_t *actual, const uint8_t *expected, size_t length)
{
    size_t index;

    if (actual == NULL || expected == NULL || length == 0u) {
        return GBL_HEADER_COMPARE_INVALID_ARGUMENT;
    }
    for (index = 0u; index < length; ++index) {
        if (actual[index] != expected[index]) {
            return GBL_HEADER_COMPARE_DIFFERENT;
        }
    }
    return GBL_HEADER_COMPARE_EQUAL;
}

gbl_load_result gbl_load_rom(gbl_machine *machine, const uint8_t *image,
                             size_t length)
{
    gbl_load_result result = { GBL_LOAD_STATUS_OK, GBL_LOAD_REASON_NONE };
    uint8_t *candidate;
    uint8_t *old_rom;
    size_t old_rom_length;
    gbl_allocator allocator;
    gbl_header_diagnostics diagnostics;

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
    diagnostics = gbl_header_diagnostics_from_candidate(candidate);

    old_rom = machine->rom;
    old_rom_length = machine->rom_length;
    machine->rom = candidate;
    machine->rom_length = length;
    machine->header_diagnostics = diagnostics;
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

gbl_header_diagnostics gbl_rom_diagnostics(const gbl_machine *machine)
{
    if (machine == NULL || !machine->initialized) {
        return (gbl_header_diagnostics){ 0 };
    }
    return machine->header_diagnostics;
}
