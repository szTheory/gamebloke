#include "gamebloke/gamebloke.h"

#define GBL_EWRAM_MASK 0x3FFFFu
#define GBL_IWRAM_MASK 0x7FFFu
#define GBL_VRAM_MASK 0x1FFFFu
#define GBL_ROM_MASK 0x1FFFFFFu

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

gbl_bus_region gbl_bus_region_for_address(uint32_t address)
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

static uint8_t gbl_bus_wait_window(uint32_t address)
{
    if (address >= UINT32_C(0x08000000) &&
        address <= UINT32_C(0x09FFFFFF)) {
        return 0u;
    }
    if (address >= UINT32_C(0x0A000000) &&
        address <= UINT32_C(0x0BFFFFFF)) {
        return 1u;
    }
    if (address >= UINT32_C(0x0C000000) &&
        address <= UINT32_C(0x0DFFFFFF)) {
        return 2u;
    }
    return UINT8_MAX;
}

static uint64_t gbl_bus_gamepak_beat_cycles(
    uint16_t waitcnt, uint32_t address, gbl_bus_access_class sequence)
{
    static const uint8_t first_access_waits[4] = { 4u, 3u, 2u, 8u };
    uint8_t window = gbl_bus_wait_window(address);
    uint8_t wait;

    if (window == UINT8_MAX) {
        return 0u;
    }
    if (sequence == GBL_BUS_CLASS_NONSEQUENTIAL) {
        const uint8_t shift = (uint8_t)(2u + window * 3u);
        wait = first_access_waits[(waitcnt >> shift) & UINT16_C(3)];
    } else {
        const uint8_t shift = (uint8_t)(4u + window * 3u);
        const bool short_wait = ((waitcnt >> shift) & UINT16_C(1)) != 0u;
        wait = short_wait ? 1u : (window == 2u ? 8u : 2u);
    }
    return (uint64_t)wait + UINT64_C(1);
}

static bool gbl_bus_quote_with_waitcnt(
    uint16_t waitcnt, uint32_t address, gbl_bus_region region,
    gbl_bus_direction direction, uint8_t width,
    gbl_bus_access_class sequence, gbl_bus_access_role role,
    gbl_cycle_cost *cost_out)
{
    gbl_cycle_cost cost;

    if (cost_out == NULL ||
        (direction != GBL_BUS_DIRECTION_READ &&
         direction != GBL_BUS_DIRECTION_WRITE) ||
        region < GBL_BUS_REGION_BIOS || region >= GBL_BUS_REGION_COUNT ||
        !gbl_bus_width_is_valid(width) ||
        !gbl_bus_class_is_valid(sequence) ||
        (role != GBL_BUS_ROLE_FETCH && role != GBL_BUS_ROLE_DATA) ||
        (direction == GBL_BUS_DIRECTION_WRITE && role == GBL_BUS_ROLE_FETCH) ||
        address > UINT32_MAX - ((uint32_t)width - 1u)) {
        return false;
    }
    if (region == GBL_BUS_REGION_ROM) {
        uint64_t cycles = 0u;
        uint8_t beat_count = width == 4u ? 2u : 1u;
        uint8_t beat;

        for (beat = 0u; beat < beat_count; ++beat) {
            const uint32_t beat_address = address + (uint32_t)beat * 2u;
            const gbl_bus_access_class beat_sequence =
                beat == 0u ? sequence : GBL_BUS_CLASS_SEQUENTIAL;
            const uint64_t beat_cycles = gbl_bus_gamepak_beat_cycles(
                waitcnt, beat_address, beat_sequence);

            if (beat_cycles == 0u || beat_cycles > UINT64_MAX - cycles) {
                return false;
            }
            cycles += beat_cycles;
        }
        cost = (gbl_cycle_cost){
            cycles, GBL_CYCLE_COST_SOURCE_BACKED,
            "nintendo-agb-programming-manual-v1.1"
        };
    } else {
        cost = (gbl_cycle_cost){
            UINT64_C(1), GBL_CYCLE_COST_PROJECT_SELECTED,
            GBL_TIMER_MODEL_NAME
        };
    }
    *cost_out = cost;
    return true;
}

bool gbl_bus_quote(const gbl_machine *machine, uint32_t address,
                   gbl_bus_direction direction, uint8_t width,
                   gbl_bus_access_class sequence,
                   gbl_bus_access_role role, gbl_cycle_cost *cost_out)
{
    if (machine == NULL) {
        return false;
    }
    return gbl_bus_quote_with_waitcnt(
        machine->waitcnt, address, gbl_bus_region_for_address(address),
        direction, width, sequence, role, cost_out);
}

bool gbl_bus_cost_for(gbl_bus_direction direction, gbl_bus_region region,
                      uint8_t width, gbl_bus_access_class sequence,
                      gbl_bus_access_role role, gbl_cycle_cost *cost_out)
{
    uint32_t address = 0u;

    if (region == GBL_BUS_REGION_ROM) {
        address = UINT32_C(0x08000000);
    }
    return gbl_bus_quote_with_waitcnt(0u, address, region, direction, width,
                                      sequence, role, cost_out);
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
    case GBL_BUS_REGION_COUNT:
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
    case GBL_BUS_REGION_COUNT:
        return GBL_BUS_OUTCOME_UNMAPPED;
    }
    return GBL_BUS_OUTCOME_UNMAPPED;
}

static bool gbl_bus_is_dispcnt_byte(uint32_t address)
{
    return address == 0x04000000u || address == 0x04000001u;
}

static bool gbl_bus_is_timer_byte(uint32_t address)
{
    return address >= UINT32_C(0x04000100) &&
           address <= UINT32_C(0x0400010F);
}

static bool gbl_bus_is_keypad_byte(uint32_t address)
{
    return address >= UINT32_C(0x04000130) &&
           address <= UINT32_C(0x04000133);
}

static bool gbl_bus_is_interrupt_byte(uint32_t address)
{
    return (address >= UINT32_C(0x04000200) &&
            address <= UINT32_C(0x04000203)) ||
           address == UINT32_C(0x04000208) ||
           address == UINT32_C(0x04000209);
}

static bool gbl_bus_is_dma_byte(uint32_t address)
{
    return address >= UINT32_C(0x040000B0) &&
           address <= UINT32_C(0x040000DF);
}

static bool gbl_bus_is_waitcnt_byte(uint32_t address)
{
    return address == UINT32_C(0x04000204) ||
           address == UINT32_C(0x04000205);
}

static uint8_t gbl_bus_dma_register_byte(
    const gbl_machine_time_preview *preview, uint32_t address)
{
    const uint32_t offset = address - UINT32_C(0x040000B0);
    const uint8_t channel_index = (uint8_t)(offset / UINT32_C(12));
    const uint32_t register_offset = offset % UINT32_C(12);
    const gbl_dma_channel_state *channel;
    uint32_t value;

    if (preview == NULL || channel_index >= GBL_DMA_CHANNEL_COUNT) {
        return 0u;
    }
    channel = &preview->dma_channels[channel_index];
    if (register_offset < 4u) {
        value = channel->source_address;
    } else if (register_offset < 8u) {
        value = channel->destination_address;
    } else if (register_offset < 10u) {
        value = channel->count;
    } else {
        value = channel->control;
    }
    if (register_offset >= 10u) {
        return (uint8_t)(value >> ((register_offset - 10u) * 8u));
    }
    return (uint8_t)(value >> ((register_offset % 4u) * 8u));
}

static gbl_bus_status gbl_bus_status_from_time(gbl_time_status status)
{
    if (status == GBL_TIME_STATUS_OK) {
        return GBL_BUS_STATUS_OK;
    }
    if (status == GBL_TIME_STATUS_CYCLE_OVERFLOW) {
        return GBL_BUS_STATUS_TIME_OVERFLOW;
    }
    if (status == GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT) {
        return GBL_BUS_STATUS_EVENT_DISPATCH_LIMIT;
    }
    return GBL_BUS_STATUS_TIME_REFUSAL;
}

static bool gbl_bus_read_lane(
    gbl_machine *machine, const gbl_machine_time_preview *time_preview,
    uint32_t address, uint8_t *value, gbl_bus_outcome *outcome)
{
    gbl_bus_region region = gbl_bus_region_for_address(address);

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
    case GBL_BUS_REGION_IO:
        if (gbl_bus_is_dispcnt_byte(address)) {
            *outcome = GBL_BUS_OUTCOME_IO_MODE_SUBSET;
            *value = address == 0x04000000u
                         ? (uint8_t)(machine->dispcnt & 0x0007u)
                         : (uint8_t)(machine->dispcnt >> 8u);
            return true;
        }
        if (gbl_bus_is_dma_byte(address)) {
            *outcome = GBL_BUS_OUTCOME_IO_DMA_SUBSET;
            *value = gbl_bus_dma_register_byte(time_preview, address);
            return true;
        }
        if (gbl_bus_is_waitcnt_byte(address)) {
            *outcome = GBL_BUS_OUTCOME_IO_WAITCNT_SUBSET;
            *value = (uint8_t)(time_preview->waitcnt >>
                               ((address & UINT32_C(1)) * 8u));
            return true;
        }
        if (gbl_bus_is_timer_byte(address)) {
            *outcome = GBL_BUS_OUTCOME_IO_TIMER_SUBSET;
            *value = gbl_timer_read(time_preview, address);
            return true;
        }
        if (gbl_bus_is_keypad_byte(address)) {
            uint16_t register_value;

            *outcome = GBL_BUS_OUTCOME_IO_KEYPAD_SUBSET;
            if (address <= UINT32_C(0x04000131)) {
                register_value = (uint16_t)(
                    (uint16_t)~time_preview->pressed_keys &
                    UINT16_C(0x03FF));
            } else {
                register_value = time_preview->key_control;
            }
            *value = (uint8_t)(register_value >>
                               ((address & UINT32_C(1)) * 8u));
            return true;
        }
        if (gbl_bus_is_interrupt_byte(address)) {
            *outcome = GBL_BUS_OUTCOME_IO_INTERRUPT_SUBSET;
            if (address <= UINT32_C(0x04000201)) {
                *value = (uint8_t)(
                    (time_preview->interrupt_enable & UINT16_C(0x3FFF)) >>
                    ((address & UINT32_C(1)) * 8u));
            } else if (address <= UINT32_C(0x04000203)) {
                *value = (uint8_t)(
                    (time_preview->interrupt_flags & UINT16_C(0x3FFF)) >>
                    ((address & UINT32_C(1)) * 8u));
            } else {
                *value = (uint8_t)(
                    (time_preview->interrupt_master_enable &
                     UINT16_C(0x0001)) >>
                    ((address & UINT32_C(1)) * 8u));
            }
            return true;
        }
        *value = 0u;
        return false;
    case GBL_BUS_REGION_BIOS:
    case GBL_BUS_REGION_UNMAPPED:
    case GBL_BUS_REGION_COUNT:
        *value = 0u;
        return false;
    }
    *value = 0u;
    *outcome = GBL_BUS_OUTCOME_UNMAPPED;
    return false;
}

static gbl_bus_outcome gbl_bus_write_lane(gbl_machine *machine,
                                          uint32_t address, uint8_t value,
                                          uint8_t request_width)
{
    gbl_bus_region region = gbl_bus_region_for_address(address);
    uint8_t *memory_byte;

    switch (region) {
    case GBL_BUS_REGION_EWRAM:
    case GBL_BUS_REGION_IWRAM:
        memory_byte = gbl_bus_memory_byte(machine, region, address);
        *memory_byte = value;
        return GBL_BUS_OUTCOME_RAM;
    case GBL_BUS_REGION_PRAM:
        memory_byte = gbl_bus_memory_byte(machine, region, address);
        if (request_width == 1u) {
            uint32_t index = address & (GBL_PRAM_SIZE - 1u);
            uint32_t halfword_index = index & ~1u;

            machine->pram[halfword_index] = value;
            machine->pram[halfword_index + 1u] = value;
        } else {
            *memory_byte = value;
        }
        return GBL_BUS_OUTCOME_RAM;
    case GBL_BUS_REGION_VRAM:
        memory_byte = gbl_bus_memory_byte(machine, region, address);
        if (request_width == 1u) {
            uint32_t mode = machine->dispcnt & 0x0007u;
            uint32_t obj_base = mode <= 2u ? 0x10000u : 0x14000u;
            uint32_t index = gbl_bus_physical_vram_offset(address);
            gbl_bus_outcome outcome =
                mode >= 6u ? GBL_BUS_OUTCOME_VIDEO_INVALID_MODE_POLICY
                           : GBL_BUS_OUTCOME_RAM;

            if (index < obj_base) {
                uint32_t halfword_index = index & ~1u;

                machine->vram[halfword_index] = value;
                machine->vram[halfword_index + 1u] = value;
            }
            return outcome;
        }
        *memory_byte = value;
        return GBL_BUS_OUTCOME_RAM;
    case GBL_BUS_REGION_OAM:
        if (request_width == 1u) {
            return GBL_BUS_OUTCOME_RAM;
        }
        memory_byte = gbl_bus_memory_byte(machine, region, address);
        *memory_byte = value;
        return GBL_BUS_OUTCOME_RAM;
    case GBL_BUS_REGION_BIOS:
        return GBL_BUS_OUTCOME_BIOS_ABSENT;
    case GBL_BUS_REGION_IO:
        if (gbl_bus_is_dispcnt_byte(address)) {
            if (address == 0x04000000u) {
                machine->dispcnt = (uint16_t)(value & 0x07u);
            }
            return GBL_BUS_OUTCOME_IO_MODE_SUBSET;
        }
        if (gbl_bus_is_timer_byte(address)) {
            return GBL_BUS_OUTCOME_IO_TIMER_SUBSET;
        }
        if (gbl_bus_is_keypad_byte(address)) {
            return GBL_BUS_OUTCOME_IO_KEYPAD_SUBSET;
        }
        if (gbl_bus_is_interrupt_byte(address)) {
            return GBL_BUS_OUTCOME_IO_INTERRUPT_SUBSET;
        }
        if (gbl_bus_is_dma_byte(address)) {
            return GBL_BUS_OUTCOME_IO_DMA_SUBSET;
        }
        if (gbl_bus_is_waitcnt_byte(address)) {
            return GBL_BUS_OUTCOME_IO_WAITCNT_SUBSET;
        }
        return GBL_BUS_OUTCOME_IO_UNIMPLEMENTED;
    case GBL_BUS_REGION_ROM:
        return GBL_BUS_OUTCOME_ROM;
    case GBL_BUS_REGION_UNMAPPED:
    case GBL_BUS_REGION_COUNT:
        return GBL_BUS_OUTCOME_UNMAPPED;
    }
    return GBL_BUS_OUTCOME_UNMAPPED;
}

static gbl_bus_result gbl_bus_read_role(gbl_machine *machine,
                                        uint32_t address, uint8_t width,
                                        gbl_bus_access_class access_class,
                                        gbl_bus_access_role role)
{
    gbl_bus_status status;
    gbl_bus_result result;
    gbl_cycle_cost cost;
    gbl_machine_time_preview time_preview;
    gbl_time_status time_status;
    gbl_time_operation time_operation;
    uint32_t value = 0u;
    uint32_t lane;
    bool has_fallback = false;
    gbl_bus_outcome first_outcome = GBL_BUS_OUTCOME_NONE;
    gbl_bus_outcome fallback_outcome = GBL_BUS_OUTCOME_NONE;

    status = gbl_bus_validate_request(machine, address, width, access_class);
    if (status != GBL_BUS_STATUS_OK) {
        return gbl_bus_error(status, access_class);
    }
    if (!gbl_bus_quote(machine, address, GBL_BUS_DIRECTION_READ, width,
                       access_class, role, &cost)) {
        return gbl_bus_error(GBL_BUS_STATUS_INVALID_ARGUMENT,
                             access_class);
    }
    time_operation = (gbl_time_operation){
        GBL_TIME_OPERATION_BUS_READ, width, address, 0u, cost.cycles
    };
    time_status = gbl_machine_prepare_operations(
        machine, &time_operation, 1u, &time_preview);
    if (time_status != GBL_TIME_STATUS_OK) {
        return gbl_bus_error(gbl_bus_status_from_time(time_status),
                             access_class);
    }

    for (lane = 0u; lane < (uint32_t)width; ++lane) {
        uint8_t byte_value;
        gbl_bus_outcome lane_outcome;
        bool has_data = gbl_bus_read_lane(machine, &time_preview,
                                          address + lane, &byte_value,
                                          &lane_outcome);

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
    gbl_machine_commit_time(machine, &time_preview);
    result = gbl_bus_make_result(GBL_BUS_STATUS_OK,
                                 has_fallback ? fallback_outcome : first_outcome,
                                 value, cost.cycles, access_class);
    return result;
}

gbl_bus_result gbl_bus_read(gbl_machine *machine, uint32_t address,
                            uint8_t width,
                            gbl_bus_access_class access_class)
{
    return gbl_bus_read_role(machine, address, width, access_class,
                             GBL_BUS_ROLE_DATA);
}

gbl_bus_result gbl_bus_read_fetch(gbl_machine *machine, uint32_t address,
                                  uint8_t width,
                                  gbl_bus_access_class access_class)
{
    return gbl_bus_read_role(machine, address, width, access_class,
                             GBL_BUS_ROLE_FETCH);
}

gbl_bus_result gbl_bus_write(gbl_machine *machine, uint32_t address,
                             uint8_t width,
                             gbl_bus_access_class access_class,
                             uint32_t value)
{
    gbl_bus_status status;
    gbl_bus_result result;
    gbl_cycle_cost cost;
    gbl_machine_time_preview time_preview;
    gbl_time_status time_status;
    gbl_time_operation time_operation;
    uint32_t lane;
    gbl_bus_outcome first_outcome = GBL_BUS_OUTCOME_NONE;
    gbl_bus_outcome fallback_outcome = GBL_BUS_OUTCOME_NONE;

    status = gbl_bus_validate_request(machine, address, width, access_class);
    if (status != GBL_BUS_STATUS_OK) {
        return gbl_bus_error(status, access_class);
    }
    if (!gbl_bus_quote(machine, address, GBL_BUS_DIRECTION_WRITE, width,
                       access_class, GBL_BUS_ROLE_DATA, &cost)) {
        return gbl_bus_error(GBL_BUS_STATUS_INVALID_ARGUMENT,
                             access_class);
    }
    time_operation = (gbl_time_operation){
        GBL_TIME_OPERATION_BUS_WRITE, width, address, value, cost.cycles
    };
    time_status = gbl_machine_prepare_operations(
        machine, &time_operation, 1u, &time_preview);
    if (time_status != GBL_TIME_STATUS_OK) {
        return gbl_bus_error(gbl_bus_status_from_time(time_status),
                             access_class);
    }
    gbl_machine_commit_time(machine, &time_preview);

    for (lane = 0u; lane < (uint32_t)width; ++lane) {
        gbl_bus_outcome lane_outcome = gbl_bus_write_lane(
            machine, address + lane,
            (uint8_t)((value >> (lane * 8u)) & 0xFFu), width);

        if (lane == 0u) {
            first_outcome = lane_outcome;
        }
        fallback_outcome = gbl_bus_fallback_priority(fallback_outcome,
                                                     lane_outcome);
    }

    result = gbl_bus_make_result(
        GBL_BUS_STATUS_OK,
        fallback_outcome != GBL_BUS_OUTCOME_NONE ? fallback_outcome
                                                 : first_outcome,
        0u, cost.cycles, access_class);
    return result;
}

gbl_dma_status gbl_bus_dma_transfer_unit(
    gbl_machine *machine, uint8_t channel, uint64_t cycle_budget,
    gbl_dma_step_result *result)
{
    const gbl_dma_channel_state *dma;
    gbl_cycle_cost source_cost;
    gbl_cycle_cost destination_cost;
    gbl_time_operation source_operation;
    gbl_time_operation operations[GBL_TIME_OPERATION_MAX] = { { 0 } };
    gbl_machine_time_preview source_preview;
    gbl_machine_time_preview unit_preview;
    gbl_time_status time_status;
    uint32_t value = 0u;
    uint32_t source_address;
    uint32_t destination_address;
    uint32_t lane;
    bool has_fallback = false;

    if (machine == NULL || result == NULL || !machine->initialized ||
        channel >= GBL_DMA_CHANNEL_COUNT) {
        return GBL_DMA_STATUS_INVALID_ARGUMENT;
    }
    dma = &machine->dma_channels[channel];
    source_address = dma->current_source;
    destination_address = dma->current_destination;
    if (!dma->active || !dma->startup_complete ||
        machine->bus_owner != GBL_MACHINE_BUS_OWNER_DMA ||
        dma->remaining == 0u || dma->completed_units == UINT64_MAX ||
        gbl_bus_validate_request(machine, source_address, 2u,
                                 GBL_BUS_CLASS_NONSEQUENTIAL) !=
            GBL_BUS_STATUS_OK ||
        gbl_bus_validate_request(machine, destination_address, 2u,
                                 GBL_BUS_CLASS_NONSEQUENTIAL) !=
            GBL_BUS_STATUS_OK ||
        !gbl_bus_quote(machine, source_address, GBL_BUS_DIRECTION_READ,
                       2u, GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_ROLE_DATA,
                       &source_cost) ||
        !gbl_bus_quote(machine, destination_address,
                       GBL_BUS_DIRECTION_WRITE, 2u,
                       GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_ROLE_DATA,
                       &destination_cost) ||
        source_cost.cycles > UINT64_MAX - destination_cost.cycles) {
        result->status = GBL_DMA_STATUS_UNIT_REFUSAL;
        return result->status;
    }
    if (source_cost.cycles + destination_cost.cycles > cycle_budget) {
        result->status = GBL_DMA_STATUS_BUDGET_REFUSAL;
        return result->status;
    }
    source_operation = (gbl_time_operation){
        GBL_TIME_OPERATION_BUS_READ, 2u, source_address, 0u,
        source_cost.cycles
    };
    time_status = gbl_machine_prepare_operations(
        machine, &source_operation, 1u, &source_preview);
    if (time_status != GBL_TIME_STATUS_OK) {
        result->status = time_status == GBL_TIME_STATUS_CYCLE_OVERFLOW
                             ? GBL_DMA_STATUS_TIME_OVERFLOW
                             : time_status ==
                                       GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT
                                   ? GBL_DMA_STATUS_EVENT_DISPATCH_LIMIT
                                   : GBL_DMA_STATUS_UNIT_REFUSAL;
        return result->status;
    }
    for (lane = 0u; lane < 2u; ++lane) {
        uint8_t byte_value;
        gbl_bus_outcome lane_outcome;

        if (!gbl_bus_read_lane(machine, &source_preview,
                               source_address + lane, &byte_value,
                               &lane_outcome)) {
            byte_value = gbl_bus_latch_byte(machine->open_bus, lane);
            has_fallback = true;
        }
        value |= (uint32_t)byte_value << (lane * 8u);
    }
    if (gbl_bus_region_for_address(destination_address) ==
        GBL_BUS_REGION_IO) {
        result->status = GBL_DMA_STATUS_UNIT_REFUSAL;
        return result->status;
    }
    operations[0] = source_operation;
    operations[1] = (gbl_time_operation){
        GBL_TIME_OPERATION_BUS_WRITE, 2u, destination_address, value,
        destination_cost.cycles
    };
    time_status = gbl_machine_prepare_operations(machine, operations, 2u,
                                                 &unit_preview);
    if (time_status != GBL_TIME_STATUS_OK) {
        result->status = time_status == GBL_TIME_STATUS_CYCLE_OVERFLOW
                             ? GBL_DMA_STATUS_TIME_OVERFLOW
                             : time_status ==
                                       GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT
                                   ? GBL_DMA_STATUS_EVENT_DISPATCH_LIMIT
                                   : GBL_DMA_STATUS_UNIT_REFUSAL;
        return result->status;
    }

    gbl_machine_commit_time(machine, &unit_preview);
    if (!has_fallback) {
        machine->open_bus = gbl_bus_latch_replication(value, 2u);
    }
    machine->dma_channels[channel].read_latch = value;
    for (lane = 0u; lane < 2u; ++lane) {
        (void)gbl_bus_write_lane(
            machine, destination_address + lane,
            (uint8_t)((value >> (lane * 8u)) & UINT32_C(0xFF)), 2u);
    }
    result->status = GBL_DMA_STATUS_OK;
    result->owner = GBL_MACHINE_BUS_OWNER_DMA;
    result->channel = channel;
    result->source_cycles = source_cost.cycles;
    result->destination_cycles = destination_cost.cycles;
    result->source_sequence = GBL_BUS_CLASS_NONSEQUENTIAL;
    result->destination_sequence = GBL_BUS_CLASS_NONSEQUENTIAL;
    result->completion_cycle = machine->master_cycles;
    result->unit_completed = true;
    return result->status;
}

gbl_bus_result gbl_bus_internal(gbl_machine *machine, uint64_t cycles)
{
    gbl_bus_result result;
    gbl_time_status time_status;
    gbl_machine_time_preview time_preview;
    const gbl_time_operation time_operation = {
        GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, cycles
    };

    if (machine == NULL || !machine->initialized) {
        return gbl_bus_error(GBL_BUS_STATUS_INVALID_ARGUMENT,
                             GBL_BUS_CLASS_NONSEQUENTIAL);
    }
    time_status = gbl_machine_prepare_operations(
        machine, &time_operation, 1u, &time_preview);
    if (time_status != GBL_TIME_STATUS_OK) {
        return gbl_bus_error(gbl_bus_status_from_time(time_status),
                             GBL_BUS_CLASS_NONSEQUENTIAL);
    }
    gbl_machine_commit_time(machine, &time_preview);
    result = gbl_bus_make_result(GBL_BUS_STATUS_OK, GBL_BUS_OUTCOME_NONE, 0u,
                                 cycles, GBL_BUS_CLASS_NONSEQUENTIAL);
    return result;
}
