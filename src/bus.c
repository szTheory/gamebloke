#include "gamebloke/gamebloke.h"

static bool gbl_bus_width_is_valid(uint8_t width)
{
    return width == 1u || width == 2u || width == 4u;
}

static bool gbl_bus_class_is_valid(gbl_bus_access_class access_class)
{
    return access_class == GBL_BUS_CLASS_NONSEQUENTIAL ||
           access_class == GBL_BUS_CLASS_SEQUENTIAL;
}

static gbl_bus_result gbl_bus_make_result(gbl_bus_status status,
                                          gbl_bus_outcome outcome,
                                          uint32_t value, uint64_t cycles,
                                          gbl_bus_access_class access_class)
{
    gbl_bus_result result;

    result.status = status;
    result.outcome = outcome;
    result.value = value;
    result.cycles = cycles;
    result.access_class = access_class;
    return result;
}

static gbl_bus_result gbl_bus_error(gbl_bus_status status,
                                    gbl_bus_access_class access_class)
{
    if (!gbl_bus_class_is_valid(access_class)) {
        access_class = GBL_BUS_CLASS_NONSEQUENTIAL;
    }
    return gbl_bus_make_result(status, GBL_BUS_OUTCOME_NONE, 0u, 0u,
                               access_class);
}

static gbl_bus_status gbl_bus_validate_request(
    const gbl_machine *machine, uint32_t address, uint8_t width,
    gbl_bus_access_class access_class)
{
    uint32_t span;

    if (machine == NULL || !machine->initialized ||
        !gbl_bus_width_is_valid(width) ||
        !gbl_bus_class_is_valid(access_class)) {
        return GBL_BUS_STATUS_INVALID_ARGUMENT;
    }
    span = (uint32_t)width - 1u;
    if (address > UINT32_MAX - span) {
        return GBL_BUS_STATUS_INVALID_ARGUMENT;
    }
    return GBL_BUS_STATUS_OK;
}

static bool gbl_bus_is_ewram(uint32_t address, uint8_t width)
{
    const uint32_t first = 0x02000000u;
    const uint32_t last = 0x02FFFFFFu;
    const uint32_t final_byte = address + (uint32_t)width - 1u;

    return address >= first && final_byte <= last;
}

static uint32_t gbl_bus_read_latch_value(uint32_t open_bus, uint8_t width)
{
    if (width == 1u) {
        return open_bus & 0xFFu;
    }
    if (width == 2u) {
        return open_bus & 0xFFFFu;
    }
    return open_bus;
}

static uint32_t gbl_bus_latch_replication(uint32_t value, uint8_t width)
{
    if (width == 1u) {
        return (value & 0xFFu) * 0x01010101u;
    }
    if (width == 2u) {
        return (value & 0xFFFFu) * 0x00010001u;
    }
    return value;
}

gbl_bus_result gbl_bus_read(gbl_machine *machine, uint32_t address,
                            uint8_t width,
                            gbl_bus_access_class access_class)
{
    gbl_bus_status status;
    gbl_bus_result result;
    uint64_t next_cycles;

    status = gbl_bus_validate_request(machine, address, width, access_class);
    if (status != GBL_BUS_STATUS_OK) {
        return gbl_bus_error(status, access_class);
    }
    if (UINT64_C(1) > UINT64_MAX - machine->master_cycles) {
        return gbl_bus_error(GBL_BUS_STATUS_TIME_OVERFLOW, access_class);
    }
    next_cycles = machine->master_cycles + UINT64_C(1);

    if (gbl_bus_is_ewram(address, width)) {
        uint32_t value = 0u;
        uint32_t offset = address - 0x02000000u;
        uint32_t byte_index;

        for (byte_index = 0u; byte_index < (uint32_t)width; ++byte_index) {
            uint32_t backing_index = (offset + byte_index) % GBL_EWRAM_SIZE;
            value |= (uint32_t)machine->ewram[backing_index] << (byte_index * 8u);
        }
        machine->open_bus = gbl_bus_latch_replication(value, width);
        result = gbl_bus_make_result(GBL_BUS_STATUS_OK, GBL_BUS_OUTCOME_RAM,
                                     value, 1u, access_class);
    } else {
        result = gbl_bus_make_result(
            GBL_BUS_STATUS_OK, GBL_BUS_OUTCOME_UNMAPPED,
            gbl_bus_read_latch_value(machine->open_bus, width), 1u,
            access_class);
    }

    machine->master_cycles = next_cycles;
    return result;
}

gbl_bus_result gbl_bus_write(gbl_machine *machine, uint32_t address,
                             uint8_t width,
                             gbl_bus_access_class access_class,
                             uint32_t value)
{
    gbl_bus_status status;
    gbl_bus_result result;
    uint64_t next_cycles;

    status = gbl_bus_validate_request(machine, address, width, access_class);
    if (status != GBL_BUS_STATUS_OK) {
        return gbl_bus_error(status, access_class);
    }
    if (UINT64_C(1) > UINT64_MAX - machine->master_cycles) {
        return gbl_bus_error(GBL_BUS_STATUS_TIME_OVERFLOW, access_class);
    }
    next_cycles = machine->master_cycles + UINT64_C(1);

    if (gbl_bus_is_ewram(address, width)) {
        uint32_t offset = address - 0x02000000u;
        uint32_t byte_index;

        for (byte_index = 0u; byte_index < (uint32_t)width; ++byte_index) {
            uint32_t backing_index = (offset + byte_index) % GBL_EWRAM_SIZE;
            machine->ewram[backing_index] =
                (uint8_t)((value >> (byte_index * 8u)) & 0xFFu);
        }
        result = gbl_bus_make_result(GBL_BUS_STATUS_OK, GBL_BUS_OUTCOME_RAM,
                                     0u, 1u, access_class);
    } else {
        result = gbl_bus_make_result(GBL_BUS_STATUS_OK,
                                     GBL_BUS_OUTCOME_UNMAPPED, 0u, 1u,
                                     access_class);
    }

    machine->master_cycles = next_cycles;
    return result;
}

gbl_bus_result gbl_bus_internal(gbl_machine *machine, uint64_t cycles)
{
    gbl_bus_result result;
    uint64_t next_cycles;

    if (machine == NULL || !machine->initialized) {
        return gbl_bus_error(GBL_BUS_STATUS_INVALID_ARGUMENT,
                             GBL_BUS_CLASS_NONSEQUENTIAL);
    }
    if (cycles > UINT64_MAX - machine->master_cycles) {
        return gbl_bus_error(GBL_BUS_STATUS_TIME_OVERFLOW,
                             GBL_BUS_CLASS_NONSEQUENTIAL);
    }
    next_cycles = machine->master_cycles + cycles;
    if (cycles != 0u) {
        machine->master_cycles = next_cycles;
    }
    result = gbl_bus_make_result(GBL_BUS_STATUS_OK, GBL_BUS_OUTCOME_NONE, 0u,
                                 cycles, GBL_BUS_CLASS_NONSEQUENTIAL);
    return result;
}
