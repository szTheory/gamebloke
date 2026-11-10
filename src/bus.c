#include "gamebloke/gamebloke.h"

#define GBL_EWRAM_MASK 0x3FFFFu
#define GBL_IWRAM_MASK 0x7FFFu
#define GBL_VRAM_MASK 0x1FFFFu
#define GBL_ROM_MASK 0x1FFFFFFu

typedef enum gbl_bus_region {
    GBL_BUS_REGION_BIOS,
    GBL_BUS_REGION_EWRAM,
    GBL_BUS_REGION_IWRAM,
    GBL_BUS_REGION_IO,
    GBL_BUS_REGION_PRAM,
    GBL_BUS_REGION_VRAM,
    GBL_BUS_REGION_OAM,
    GBL_BUS_REGION_ROM,
    GBL_BUS_REGION_UNMAPPED
} gbl_bus_region;

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

static bool gbl_bus_is_special_io(uint32_t address)
{
    uint32_t low_address = address & 0xFFFFu;

    return address >= 0x04000800u && address <= 0x04FFFFFFu &&
           low_address >= 0x0800u && low_address <= 0x0803u;
}

static gbl_bus_region gbl_bus_region_for(uint32_t address)
{
    if (address <= 0x00003FFFu) {
        return GBL_BUS_REGION_BIOS;
    }
    if (address >= 0x02000000u && address <= 0x02FFFFFFu) {
        return GBL_BUS_REGION_EWRAM;
    }
    if (address >= 0x03000000u && address <= 0x03FFFFFFu) {
        return GBL_BUS_REGION_IWRAM;
    }
    if ((address >= 0x04000000u && address <= 0x040003FFu) ||
        gbl_bus_is_special_io(address)) {
        return GBL_BUS_REGION_IO;
    }
    if (address >= 0x05000000u && address <= 0x05FFFFFFu) {
        return GBL_BUS_REGION_PRAM;
    }
    if (address >= 0x06000000u && address <= 0x06FFFFFFu) {
        return GBL_BUS_REGION_VRAM;
    }
    if (address >= 0x07000000u && address <= 0x07FFFFFFu) {
        return GBL_BUS_REGION_OAM;
    }
    if (address >= 0x08000000u && address <= 0x0DFFFFFFu) {
        return GBL_BUS_REGION_ROM;
    }
    return GBL_BUS_REGION_UNMAPPED;
}

static uint32_t gbl_bus_physical_vram_offset(uint32_t address)
{
    uint32_t offset = address & GBL_VRAM_MASK;

    if (offset >= 0x18000u) {
        offset -= 0x8000u;
    }
    return offset;
}

static uint8_t *gbl_bus_memory_byte(gbl_machine *machine,
                                   gbl_bus_region region,
                                   uint32_t address)
{
    switch (region) {
    case GBL_BUS_REGION_EWRAM:
        return &machine->ewram[address & GBL_EWRAM_MASK];
    case GBL_BUS_REGION_IWRAM:
        return &machine->iwram[address & GBL_IWRAM_MASK];
    case GBL_BUS_REGION_PRAM:
        return &machine->pram[address & (GBL_PRAM_SIZE - 1u)];
    case GBL_BUS_REGION_VRAM:
        return &machine->vram[gbl_bus_physical_vram_offset(address)];
    case GBL_BUS_REGION_OAM:
        return &machine->oam[address & (GBL_OAM_SIZE - 1u)];
    case GBL_BUS_REGION_BIOS:
    case GBL_BUS_REGION_IO:
    case GBL_BUS_REGION_ROM:
    case GBL_BUS_REGION_UNMAPPED:
        break;
    }
    return NULL;
}

static uint8_t gbl_bus_latch_byte(uint32_t open_bus, uint32_t lane)
{
    return (uint8_t)((open_bus >> (lane * 8u)) & 0xFFu);
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

static gbl_bus_outcome gbl_bus_fallback_priority(gbl_bus_outcome left,
                                                gbl_bus_outcome right)
{
    if (left == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED ||
        right == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED) {
        return GBL_BUS_OUTCOME_IO_UNIMPLEMENTED;
    }
    if (left == GBL_BUS_OUTCOME_BIOS_ABSENT ||
        right == GBL_BUS_OUTCOME_BIOS_ABSENT) {
        return GBL_BUS_OUTCOME_BIOS_ABSENT;
    }
    if (left == GBL_BUS_OUTCOME_ROM_TAIL ||
        right == GBL_BUS_OUTCOME_ROM_TAIL) {
        return GBL_BUS_OUTCOME_ROM_TAIL;
    }
    if (left == GBL_BUS_OUTCOME_UNMAPPED ||
        right == GBL_BUS_OUTCOME_UNMAPPED) {
        return GBL_BUS_OUTCOME_UNMAPPED;
    }
    return GBL_BUS_OUTCOME_NONE;
}

static gbl_bus_outcome gbl_bus_region_outcome(gbl_bus_region region)
{
    switch (region) {
    case GBL_BUS_REGION_BIOS:
        return GBL_BUS_OUTCOME_BIOS_ABSENT;
    case GBL_BUS_REGION_IO:
        return GBL_BUS_OUTCOME_IO_UNIMPLEMENTED;
    case GBL_BUS_REGION_ROM:
        return GBL_BUS_OUTCOME_ROM;
    case GBL_BUS_REGION_UNMAPPED:
        return GBL_BUS_OUTCOME_UNMAPPED;
    case GBL_BUS_REGION_EWRAM:
    case GBL_BUS_REGION_IWRAM:
    case GBL_BUS_REGION_PRAM:
    case GBL_BUS_REGION_VRAM:
    case GBL_BUS_REGION_OAM:
        return GBL_BUS_OUTCOME_RAM;
    }
    return GBL_BUS_OUTCOME_UNMAPPED;
}

static bool gbl_bus_read_lane(gbl_machine *machine, uint32_t address,
                              uint8_t *value, gbl_bus_outcome *outcome)
{
    gbl_bus_region region = gbl_bus_region_for(address);

    *outcome = gbl_bus_region_outcome(region);
    switch (region) {
    case GBL_BUS_REGION_EWRAM:
    case GBL_BUS_REGION_IWRAM:
    case GBL_BUS_REGION_PRAM:
    case GBL_BUS_REGION_VRAM:
    case GBL_BUS_REGION_OAM:
        *value = *gbl_bus_memory_byte(machine, region, address);
        return true;
    case GBL_BUS_REGION_ROM: {
        uint32_t offset = address & GBL_ROM_MASK;

        if (machine->rom != NULL && (size_t)offset < machine->rom_length) {
            *value = machine->rom[offset];
            return true;
        }
        *value = 0u;
        *outcome = GBL_BUS_OUTCOME_ROM_TAIL;
        return false;
    }
    case GBL_BUS_REGION_BIOS:
    case GBL_BUS_REGION_IO:
    case GBL_BUS_REGION_UNMAPPED:
        *value = 0u;
        return false;
    }
    *value = 0u;
    *outcome = GBL_BUS_OUTCOME_UNMAPPED;
    return false;
}

static gbl_bus_outcome gbl_bus_write_lane(gbl_machine *machine,
                                          uint32_t address, uint8_t value)
{
    gbl_bus_region region = gbl_bus_region_for(address);
    uint8_t *memory_byte;

    switch (region) {
    case GBL_BUS_REGION_EWRAM:
    case GBL_BUS_REGION_IWRAM:
    case GBL_BUS_REGION_PRAM:
    case GBL_BUS_REGION_VRAM:
    case GBL_BUS_REGION_OAM:
        memory_byte = gbl_bus_memory_byte(machine, region, address);
        *memory_byte = value;
        return GBL_BUS_OUTCOME_RAM;
    case GBL_BUS_REGION_BIOS:
        return GBL_BUS_OUTCOME_BIOS_ABSENT;
    case GBL_BUS_REGION_IO:
        return GBL_BUS_OUTCOME_IO_UNIMPLEMENTED;
    case GBL_BUS_REGION_ROM:
        return GBL_BUS_OUTCOME_ROM;
    case GBL_BUS_REGION_UNMAPPED:
        return GBL_BUS_OUTCOME_UNMAPPED;
    }
    return GBL_BUS_OUTCOME_UNMAPPED;
}

gbl_bus_result gbl_bus_read(gbl_machine *machine, uint32_t address,
                            uint8_t width,
                            gbl_bus_access_class access_class)
{
    gbl_bus_status status;
    gbl_bus_result result;
    uint64_t next_cycles;
    uint32_t value = 0u;
    uint32_t lane;
    bool has_fallback = false;
    gbl_bus_outcome first_outcome = GBL_BUS_OUTCOME_NONE;
    gbl_bus_outcome fallback_outcome = GBL_BUS_OUTCOME_NONE;

    status = gbl_bus_validate_request(machine, address, width, access_class);
    if (status != GBL_BUS_STATUS_OK) {
        return gbl_bus_error(status, access_class);
    }
    if (UINT64_C(1) > UINT64_MAX - machine->master_cycles) {
        return gbl_bus_error(GBL_BUS_STATUS_TIME_OVERFLOW, access_class);
    }
    next_cycles = machine->master_cycles + UINT64_C(1);

    for (lane = 0u; lane < (uint32_t)width; ++lane) {
        uint8_t byte_value;
        gbl_bus_outcome lane_outcome;
        bool has_data = gbl_bus_read_lane(machine, address + lane,
                                         &byte_value, &lane_outcome);

        if (lane == 0u) {
            first_outcome = lane_outcome;
        }
        if (!has_data) {
            byte_value = gbl_bus_latch_byte(machine->open_bus, lane);
            has_fallback = true;
            fallback_outcome = gbl_bus_fallback_priority(fallback_outcome,
                                                         lane_outcome);
        }
        value |= (uint32_t)byte_value << (lane * 8u);
    }

    if (!has_fallback) {
        machine->open_bus = gbl_bus_latch_replication(value, width);
    }
    machine->master_cycles = next_cycles;
    result = gbl_bus_make_result(GBL_BUS_STATUS_OK,
                                 has_fallback ? fallback_outcome : first_outcome,
                                 value, 1u, access_class);
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
    uint32_t lane;
    gbl_bus_outcome first_outcome = GBL_BUS_OUTCOME_NONE;
    gbl_bus_outcome fallback_outcome = GBL_BUS_OUTCOME_NONE;

    status = gbl_bus_validate_request(machine, address, width, access_class);
    if (status != GBL_BUS_STATUS_OK) {
        return gbl_bus_error(status, access_class);
    }
    if (UINT64_C(1) > UINT64_MAX - machine->master_cycles) {
        return gbl_bus_error(GBL_BUS_STATUS_TIME_OVERFLOW, access_class);
    }
    next_cycles = machine->master_cycles + UINT64_C(1);

    for (lane = 0u; lane < (uint32_t)width; ++lane) {
        gbl_bus_outcome lane_outcome = gbl_bus_write_lane(
            machine, address + lane,
            (uint8_t)((value >> (lane * 8u)) & 0xFFu));

        if (lane == 0u) {
            first_outcome = lane_outcome;
        }
        fallback_outcome = gbl_bus_fallback_priority(fallback_outcome,
                                                     lane_outcome);
    }

    machine->master_cycles = next_cycles;
    result = gbl_bus_make_result(
        GBL_BUS_STATUS_OK,
        fallback_outcome != GBL_BUS_OUTCOME_NONE ? fallback_outcome
                                                 : first_outcome,
        0u, 1u, access_class);
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
