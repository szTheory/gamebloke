#include "check.h"
#include "gamebloke/gamebloke.h"
#include "state_check.h"

#include <stdlib.h>
#include <string.h>

static gbl_machine *new_machine(void)
{
    gbl_machine *machine = calloc(1u, sizeof(*machine));

    if (machine == NULL) {
        return NULL;
    }
    if (gbl_machine_init(machine, NULL) != GBL_BUS_STATUS_OK) {
        free(machine);
        return NULL;
    }
    return machine;
}

static void free_machine(gbl_machine *machine)
{
    if (machine != NULL) {
        gbl_machine_dispose(machine);
        free(machine);
    }
}

static void *test_allocate(void *context, size_t size)
{
    (void)context;
    (void)size;
    return NULL;
}

static void *rom_allocate(void *context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void rom_release(void *context, void *pointer, size_t size)
{
    (void)context;
    (void)size;
    free(pointer);
}

static void test_release(void *context, void *pointer, size_t size)
{
    unsigned int *release_count = context;

    (void)pointer;
    (void)size;
    ++(*release_count);
}

static bool bytes_are_zero(const uint8_t *bytes, size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        if (bytes[index] != 0u) {
            return false;
        }
    }
    return true;
}

typedef struct decode_boundary_case {
    uint32_t address;
    gbl_bus_outcome outcome;
    const char *reference_section;
} decode_boundary_case;

static uint32_t width_value_mask(uint8_t width)
{
    if (width == 1u) {
        return 0xFFu;
    }
    if (width == 2u) {
        return 0xFFFFu;
    }
    return UINT32_MAX;
}

static void check_ram_widths(gbl_machine *machine, uint32_t base,
                             uint32_t mirror_delta, bool byte_writes)
{
    const uint8_t widths[] = { 1u, 2u, 4u };
    size_t width_index;

    for (width_index = 0u; width_index < sizeof(widths); ++width_index) {
        uint8_t width = widths[width_index];
        uint32_t value = 0xD6B4A291u;
        uint32_t address = base + 0x121u + (uint32_t)(width_index * 8u);
        gbl_bus_result result;

        if (width == 1u && !byte_writes) {
            continue;
        }
        result = gbl_bus_write(machine, address, width,
                               GBL_BUS_CLASS_NONSEQUENTIAL, value);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
        CHECK(result.cycles == 1u);
        result = gbl_bus_read(machine, address + mirror_delta, width,
                              GBL_BUS_CLASS_SEQUENTIAL);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
        CHECK(result.value == (value & width_value_mask(width)));
        CHECK(result.cycles == 1u);
        CHECK(result.access_class == GBL_BUS_CLASS_SEQUENTIAL);
    }
}

static void check_decode_boundaries(gbl_machine *machine)
{
    /* Section names are attached per case. The GBATEK memory map says SRAM
     * exists at 0x0E000000; the current profile deliberately leaves all
     * backup memory unmapped because no save device is implemented. */
    static const decode_boundary_case cases[] = {
        { 0x00000000u, GBL_BUS_OUTCOME_BIOS_ABSENT,
          "Reading from BIOS/Unused Memory" },
        { 0x00003FFFu, GBL_BUS_OUTCOME_BIOS_ABSENT,
          "Reading from BIOS/Unused Memory" },
        { 0x00004000u, GBL_BUS_OUTCOME_UNMAPPED, "GBA Memory Map" },
        { 0x01000000u, GBL_BUS_OUTCOME_UNMAPPED, "GBA Memory Map" },
        { 0x02000000u, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x02FFFFFFu, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x03000000u, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x03FFFFFFu, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x04000000u, GBL_BUS_OUTCOME_IO_MODE_SUBSET, "GBA I/O Map" },
        { 0x04000001u, GBL_BUS_OUTCOME_IO_MODE_SUBSET, "GBA I/O Map" },
        { 0x04000002u, GBL_BUS_OUTCOME_IO_UNIMPLEMENTED, "GBA I/O Map" },
        { 0x040003FFu, GBL_BUS_OUTCOME_IO_UNIMPLEMENTED, "GBA I/O Map" },
        { 0x04000400u, GBL_BUS_OUTCOME_UNMAPPED, "GBA I/O Map" },
        { 0x04000800u, GBL_BUS_OUTCOME_IO_UNIMPLEMENTED,
          "Memory Mirrors" },
        { 0x04010800u, GBL_BUS_OUTCOME_IO_UNIMPLEMENTED,
          "Memory Mirrors" },
        { 0x04FF0800u, GBL_BUS_OUTCOME_IO_UNIMPLEMENTED,
          "Memory Mirrors" },
        { 0x05000000u, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x05FFFFFFu, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x06000000u, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x06017FFFu, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x06018000u, GBL_BUS_OUTCOME_RAM,
          "Memory Mirrors" },
        { 0x06020000u, GBL_BUS_OUTCOME_RAM,
          "Memory Mirrors" },
        { 0x06FFFFFFu, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x07000000u, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x070003FFu, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x07000400u, GBL_BUS_OUTCOME_RAM,
          "Memory Mirrors" },
        { 0x07FFFFFFu, GBL_BUS_OUTCOME_RAM,
          "GBA Memory Map; Memory Mirrors" },
        { 0x08000000u, GBL_BUS_OUTCOME_ROM, "GBA Memory Map" },
        { 0x09FFFFFFu, GBL_BUS_OUTCOME_ROM_TAIL, "GBA Memory Map" },
        { 0x0A000000u, GBL_BUS_OUTCOME_ROM, "GBA Memory Map" },
        { 0x0BFFFFFFu, GBL_BUS_OUTCOME_ROM_TAIL, "GBA Memory Map" },
        { 0x0C000000u, GBL_BUS_OUTCOME_ROM, "GBA Memory Map" },
        { 0x0DFFFFFFu, GBL_BUS_OUTCOME_ROM_TAIL, "GBA Memory Map" },
        { 0x0E000000u, GBL_BUS_OUTCOME_UNMAPPED, "GBA Memory Map" },
        { 0x0E00FFFFu, GBL_BUS_OUTCOME_UNMAPPED, "GBA Memory Map" },
        { 0x0E010000u, GBL_BUS_OUTCOME_UNMAPPED, "GBA Memory Map" },
        { 0x0FFFFFFFu, GBL_BUS_OUTCOME_UNMAPPED, "GBA Memory Map" },
        { 0x10000000u, GBL_BUS_OUTCOME_UNMAPPED, "GBA Memory Map" },
    };
    size_t case_index;

    for (case_index = 0u; case_index < sizeof(cases) / sizeof(cases[0]);
         ++case_index) {
        const uint32_t latch = 0xA1B2C3D4u;
        uint64_t before_cycles = machine->master_cycles;
        gbl_bus_result result;

        CHECK(cases[case_index].reference_section[0] != '\0');
        machine->open_bus = latch;
        result = gbl_bus_read(machine, cases[case_index].address, 1u,
                              GBL_BUS_CLASS_NONSEQUENTIAL);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == cases[case_index].outcome);
        CHECK(result.cycles == 1u);
        CHECK(result.access_class == GBL_BUS_CLASS_NONSEQUENTIAL);
        CHECK(machine->master_cycles == before_cycles + 1u);
        if (cases[case_index].outcome == GBL_BUS_OUTCOME_BIOS_ABSENT ||
            cases[case_index].outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED ||
            cases[case_index].outcome == GBL_BUS_OUTCOME_ROM_TAIL ||
            cases[case_index].outcome == GBL_BUS_OUTCOME_UNMAPPED) {
            CHECK(result.value == 0xD4u);
            CHECK(machine->open_bus == latch);
        } else {
            CHECK(machine->open_bus ==
                  ((result.value & 0xFFu) * 0x01010101u));
        }
    }
}

static int run_timing(void)
{
    const uint32_t base = 0x02000000u;
    const uint32_t last_ewram = 0x02FFFFFFu;
    gbl_machine *machine = new_machine();
    gbl_machine *uninitialized = calloc(1u, sizeof(*uninitialized));
    gbl_machine *snapshot = malloc(sizeof(*snapshot));
    gbl_bus_result result;

    if (machine == NULL || uninitialized == NULL || snapshot == NULL) {
        fprintf(stderr, "bus.timing: host allocation failed\n");
        free_machine(machine);
        free(uninitialized);
        free(snapshot);
        return 1;
    }

    result = gbl_bus_write(machine, base + 0x100u, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0x12345678u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.cycles == 1u);
    CHECK(result.access_class == GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(machine->master_cycles == 1u);
    CHECK(machine->open_bus == 0u);

    result = gbl_bus_read(machine, base + 0x100u, 4u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.value == 0x12345678u);
    CHECK(result.cycles == 1u);
    CHECK(result.access_class == GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(machine->master_cycles == 2u);
    CHECK(machine->open_bus == 0x12345678u);

    result = gbl_bus_read(machine, base + 0x101u, 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.value == 0x3456u);
    CHECK(result.cycles == 1u);
    CHECK(result.access_class == GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(machine->master_cycles == 3u);
    CHECK(machine->open_bus == 0x34563456u);

    result = gbl_bus_read(machine, 0x03000000u, 2u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 1u);
    CHECK(result.access_class == GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(machine->open_bus == 0u);
    CHECK(machine->master_cycles == 4u);
    result = gbl_bus_read(machine, 0x03000000u, 4u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.value == 0u);
    CHECK(machine->open_bus == 0u);
    CHECK(machine->master_cycles == 5u);

    result = gbl_bus_write(machine, last_ewram, 1u,
                           GBL_BUS_CLASS_SEQUENTIAL, 0x5Au);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.cycles == 1u);
    CHECK(machine->master_cycles == 6u);
    CHECK(machine->open_bus == 0u);
    result = gbl_bus_read(machine, last_ewram, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.value == 0x5Au);
    CHECK(machine->open_bus == 0x5A5A5A5Au);
    CHECK(machine->master_cycles == 7u);
    result = gbl_bus_read(machine, 0x03000000u, 1u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.value == 0u);
    CHECK(machine->open_bus == 0u);
    CHECK(machine->master_cycles == 8u);

    memcpy(snapshot, machine, sizeof(*snapshot));
    result = gbl_bus_read(machine, base, 3u, GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));
    result = gbl_bus_write(machine, base, 3u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xFFFFFFFFu);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));
    result = gbl_bus_write(machine, base, 1u,
                           (gbl_bus_access_class)99, 0xFFu);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));
    result = gbl_bus_read(machine, UINT32_MAX, 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));
    result = gbl_bus_write(machine, UINT32_MAX, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xFFFFu);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));
    result = gbl_bus_read(NULL, base, 1u, GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    result = gbl_bus_write(NULL, base, 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xFFu);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    memcpy(snapshot, uninitialized, sizeof(*snapshot));
    result = gbl_bus_read(uninitialized, base, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(uninitialized, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(uninitialized, snapshot));

    machine->master_cycles = UINT64_MAX - 1u;
    result = gbl_bus_write(machine, base + 0x200u, 1u,
                           GBL_BUS_CLASS_SEQUENTIAL, 0x77u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.cycles == 1u);
    CHECK(result.access_class == GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(machine->master_cycles == UINT64_MAX);
    CHECK(machine->ewram[0x200u] == 0x77u);

    memcpy(snapshot, machine, sizeof(*snapshot));
    result = gbl_bus_read(machine, base + 0x200u, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));
    result = gbl_bus_internal(machine, 0u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.cycles == 0u);
    CHECK(result.access_class == GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));
    result = gbl_bus_read(machine, base + 0x100u, 4u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));
    result = gbl_bus_write(machine, base + 0x100u, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xFFFFFFFFu);
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));

    machine->master_cycles = 1u;
    memcpy(snapshot, machine, sizeof(*snapshot));
    result = gbl_bus_internal(machine, UINT64_MAX);
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));

    free_machine(machine);
    free(uninitialized);
    free(snapshot);
    if (CHECK_DONE() == 0) {
        puts("bus: timing ok");
        return 0;
    }
    return 1;
}

static int run_decode(void)
{
    uint8_t image[0xC0u];
    gbl_allocator allocator = { NULL, rom_allocate, rom_release };
    gbl_machine *machine = calloc(1u, sizeof(*machine));
    gbl_load_result load;
    gbl_bus_result result;
    size_t index;

    if (machine == NULL) {
        fprintf(stderr, "bus.decode: host allocation failed\n");
        return 1;
    }
    for (index = 0u; index < sizeof(image); ++index) {
        image[index] = (uint8_t)(index ^ 0xA5u);
    }
    if (gbl_machine_init(machine, &allocator) != GBL_BUS_STATUS_OK) {
        fprintf(stderr, "bus.decode: machine initialization failed\n");
        free(machine);
        return 1;
    }

    load = gbl_load_rom(machine, image, sizeof(image));
    CHECK(load.status == GBL_LOAD_STATUS_OK);
    CHECK(gbl_rom_data(machine) != image);
    CHECK(gbl_rom_size(machine) == sizeof(image));

    result = gbl_bus_read(machine, 0x08000000u, 4u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_ROM);
    CHECK(result.value == ((uint32_t)image[0] | ((uint32_t)image[1] << 8u) |
                           ((uint32_t)image[2] << 16u) |
                           ((uint32_t)image[3] << 24u)));
    CHECK(result.cycles == 1u);
    CHECK(machine->master_cycles == 1u);

    result = gbl_bus_read(machine, 0x0A000020u, 1u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_ROM);
    CHECK(result.value == image[0x20u]);
    result = gbl_bus_read(machine, 0x0C000040u, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_ROM);
    CHECK(result.value == image[0x40u]);

    (void)gbl_bus_write(machine, 0x02000000u, 1u,
                        GBL_BUS_CLASS_NONSEQUENTIAL, 0x5Au);
    result = gbl_bus_read(machine, 0x02000000u, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    result = gbl_bus_read(machine, 0x080000C0u, 1u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_ROM_TAIL);
    CHECK(result.value == 0x5Au);
    CHECK(machine->open_bus == 0x5A5A5A5Au);

    check_ram_widths(machine, 0x02000000u, GBL_EWRAM_SIZE, true);
    check_ram_widths(machine, 0x03000000u, GBL_IWRAM_SIZE, true);
    check_ram_widths(machine, 0x05000000u, GBL_PRAM_SIZE, true);
    check_ram_widths(machine, 0x06000000u, 0x00020000u, true);
    check_ram_widths(machine, 0x07000000u, GBL_OAM_SIZE, false);
    check_decode_boundaries(machine);

    free_machine(machine);
    if (CHECK_DONE() == 0) {
        puts("bus: decode ok");
        return 0;
    }
    return 1;
}

static void check_video_mode_threshold(gbl_machine *machine, uint8_t mode,
                                       uint32_t threshold)
{
    /* The 0x10000 OBJ base for modes 0-2 and 0x14000 base for modes 3-5
     * follow GBATEK 3.06 "Writing 8bit Data to Video Memory" and
     * "LCD VRAM Overview". Modes 6-7 are an explicit Gamebloke policy. */
    gbl_bus_result result;
    gbl_bus_outcome byte_outcome =
        mode >= 6u ? GBL_BUS_OUTCOME_VIDEO_INVALID_MODE_POLICY
                   : GBL_BUS_OUTCOME_RAM;
    static const uint32_t boundaries[] = { 0x10000u, 0x14000u };
    static const int32_t deltas[] = { -1, 0, 1 };
    static const uint8_t widths[] = { 1u, 2u, 4u };
    size_t boundary_number;

    result = gbl_bus_write(machine, 0x04000000u, 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, mode);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    CHECK(result.cycles == 1u);
    CHECK(machine->dispcnt == (uint16_t)mode);
    result = gbl_bus_read(machine, 0x04000000u, 1u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    CHECK(result.value == (uint32_t)mode);
    CHECK(result.cycles == 1u);
    result = gbl_bus_read(machine, 0x04000001u, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    CHECK(result.value == 0u);

    for (boundary_number = 0u;
         boundary_number < sizeof(boundaries) / sizeof(boundaries[0]);
         ++boundary_number) {
        size_t delta_index;

        for (delta_index = 0u; delta_index < sizeof(deltas) / sizeof(deltas[0]);
             ++delta_index) {
            uint32_t offset =
                (uint32_t)((int64_t)boundaries[boundary_number] +
                           (int64_t)deltas[delta_index]);
            size_t width_index;

            for (width_index = 0u;
                 width_index < sizeof(widths) / sizeof(widths[0]);
                 ++width_index) {
                uint8_t width = widths[width_index];
                uint32_t address = 0x06000000u + offset;
                uint32_t value = 0xD6B4A291u;

                if (width == 1u) {
                    uint32_t halfword_address =
                        0x06000000u + (offset & ~1u);
                    uint32_t physical = offset & 0x1FFFFu;
                    bool should_write = physical < threshold;
                    uint32_t before = 0xBEEFu;
                    uint32_t expected = should_write ? 0x5A5Au : before;

                    result = gbl_bus_write(
                        machine, halfword_address, 2u,
                        GBL_BUS_CLASS_NONSEQUENTIAL, before);
                    CHECK(result.status == GBL_BUS_STATUS_OK);
                    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
                    result = gbl_bus_write(machine, address, 1u,
                                           GBL_BUS_CLASS_SEQUENTIAL, 0x5Au);
                    CHECK(result.status == GBL_BUS_STATUS_OK);
                    CHECK(result.outcome == byte_outcome);
                    CHECK(result.cycles == 1u);
                    result = gbl_bus_read(machine, halfword_address, 2u,
                                          GBL_BUS_CLASS_NONSEQUENTIAL);
                    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
                    CHECK(result.value == expected);
                } else {
                    result = gbl_bus_write(machine, address, width,
                                           GBL_BUS_CLASS_NONSEQUENTIAL, value);
                    CHECK(result.status == GBL_BUS_STATUS_OK);
                    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
                    CHECK(result.cycles == 1u);
                    result = gbl_bus_read(machine, address, width,
                                          GBL_BUS_CLASS_SEQUENTIAL);
                    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
                    CHECK(result.value ==
                          (value & width_value_mask(width)));
                    CHECK(result.cycles == 1u);
                }
            }
        }
    }

    /* A VRAM logical mirror is translated to physical storage before the
     * mode threshold decides whether an 8-bit store is ignored. */
    result = gbl_bus_write(machine, 0x06010000u, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xC35Au);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    result = gbl_bus_write(machine, 0x06018000u, 1u,
                           GBL_BUS_CLASS_SEQUENTIAL, 0x11u);
    CHECK(result.outcome == byte_outcome);
    result = gbl_bus_read(machine, 0x06010000u, 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.value == ((0x10000u < threshold) ? 0x1111u : 0xC35Au));

    result = gbl_bus_write(machine, 0x06017FFFu, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xA6B7u);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    result = gbl_bus_read(machine, 0x06017FFFu, 1u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.value == 0xB7u);
    result = gbl_bus_read(machine, 0x0601FFFFu, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.value == 0xB7u);
    result = gbl_bus_read(machine, 0x06018000u, 1u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.value == 0xA6u);
    result = gbl_bus_write(machine, 0x06020042u, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xCDEFu);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    result = gbl_bus_read(machine, 0x06000042u, 2u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.value == 0xCDEFu);
}

static int run_video(void)
{
    typedef struct video_mode_case {
        uint8_t mode;
        uint32_t obj_threshold;
        const char *evidence;
    } video_mode_case;
    static const video_mode_case mode_cases[] = {
        { 0u, 0x10000u, "GBATEK 3.06: Writing 8bit Data to Video Memory" },
        { 1u, 0x10000u, "GBATEK 3.06: Writing 8bit Data to Video Memory" },
        { 2u, 0x10000u, "GBATEK 3.06: Writing 8bit Data to Video Memory" },
        { 3u, 0x14000u, "GBATEK 3.06: LCD VRAM Overview" },
        { 4u, 0x14000u, "GBATEK 3.06: LCD VRAM Overview" },
        { 5u, 0x14000u, "GBATEK 3.06: LCD VRAM Overview" },
        { 6u, 0x14000u, "Gamebloke IMPL invalid-mode threshold policy" },
        { 7u, 0x14000u, "Gamebloke IMPL invalid-mode threshold policy" },
    };
    gbl_machine *machine = new_machine();
    gbl_machine *snapshot = malloc(sizeof(*snapshot));
    gbl_bus_result result;
    size_t mode_index;

    if (machine == NULL || snapshot == NULL) {
        fprintf(stderr, "bus.video: host allocation failed\n");
        free_machine(machine);
        free(snapshot);
        return 1;
    }

    /* GBATEK 3.06: Writing 8bit Data to Video Memory. */
    result = gbl_bus_write(machine, 0x05000021u, 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0x5Au);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.cycles == 1u);
    result = gbl_bus_read(machine, 0x05000420u, 2u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.value == 0x5A5Au);

    result = gbl_bus_write(machine, 0x07000020u, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xBEEFu);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    result = gbl_bus_write(machine, 0x07000421u, 1u,
                           GBL_BUS_CLASS_SEQUENTIAL, 0x11u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.cycles == 1u);
    result = gbl_bus_read(machine, 0x07000020u, 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.value == 0xBEEFu);

    result = gbl_bus_write(machine, 0x07000020u, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0x89ABCDEFu);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    result = gbl_bus_read(machine, 0x07000420u, 4u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.value == 0x89ABCDEFu);

    /* The mode subset retains only DISPCNT mode bits. Addresses above its
     * two-byte portion remain distinct unsupported I/O. */
    result = gbl_bus_write(machine, 0x04000000u, 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xFFu);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    CHECK(machine->dispcnt == 7u);
    result = gbl_bus_write(machine, 0x04000001u, 1u,
                           GBL_BUS_CLASS_SEQUENTIAL, 0xFFu);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    CHECK(machine->dispcnt == 7u);
    machine->open_bus = 0xAABBCCDDu;
    result = gbl_bus_write(machine, 0x04000000u, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0x44332203u);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);
    CHECK(result.cycles == 1u);
    CHECK(machine->dispcnt == 3u);
    CHECK(machine->open_bus == 0xAABBCCDDu);
    result = gbl_bus_read(machine, 0x04000000u, 4u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);
    CHECK(result.value == 0xAABB0003u);
    CHECK(machine->open_bus == 0xAABBCCDDu);

    /* GBATEK 3.06 "LCD I/O Display Control" locates DISPCNT at 0x04000000. */
    for (mode_index = 0u;
         mode_index < sizeof(mode_cases) / sizeof(mode_cases[0]);
         ++mode_index) {
        CHECK(mode_cases[mode_index].evidence[0] != '\0');
        check_video_mode_threshold(machine, mode_cases[mode_index].mode,
                                   mode_cases[mode_index].obj_threshold);
    }

    /* Overflow is checked before any register, video, latch, or clock effect. */
    result = gbl_bus_write(machine, 0x04000000u, 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 2u);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    machine->open_bus = 0x12345678u;
    machine->master_cycles = UINT64_MAX;
    memcpy(snapshot, machine, sizeof(*snapshot));
    result = gbl_bus_write(machine, 0x04000000u, 1u,
                           GBL_BUS_CLASS_SEQUENTIAL, 3u);
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));
    result = gbl_bus_write(machine, 0x06000020u, 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xAAu);
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));

    free(snapshot);
    free_machine(machine);
    if (CHECK_DONE() == 0) {
        puts("bus: video ok");
        return 0;
    }
    return 1;
}

static int run_state(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *other = new_machine();
    gbl_machine *fresh = new_machine();
    gbl_bus_result result;
    size_t index;

    if (machine == NULL || other == NULL || fresh == NULL) {
        fprintf(stderr, "machine.state: host allocation failed\n");
        free_machine(machine);
        free_machine(other);
        free_machine(fresh);
        return 1;
    }

    result = gbl_bus_write(machine, 0x02000100u, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xAABBCCDDu);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    result = gbl_bus_write(other, 0x02000100u, 4u,
                           GBL_BUS_CLASS_SEQUENTIAL, 0x11223344u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    result = gbl_bus_read(machine, 0x02000100u, 4u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.value == 0xAABBCCDDu);
    result = gbl_bus_read(other, 0x02000100u, 4u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.value == 0x11223344u);
    CHECK(machine->ewram[0x100u] != other->ewram[0x100u]);

    machine->dispcnt = 0x1234u;
    machine->open_bus = 0x89ABCDEFu;
    machine->master_cycles = 0x1020304050607080u;
    for (index = 0u; index < GBL_EWRAM_SIZE; ++index) {
        machine->ewram[index] = 0xA5u;
    }
    for (index = 0u; index < GBL_IWRAM_SIZE; ++index) {
        machine->iwram[index] = 0xA5u;
    }
    for (index = 0u; index < GBL_PRAM_SIZE; ++index) {
        machine->pram[index] = 0xA5u;
    }
    for (index = 0u; index < GBL_VRAM_SIZE; ++index) {
        machine->vram[index] = 0xA5u;
    }
    for (index = 0u; index < GBL_OAM_SIZE; ++index) {
        machine->oam[index] = 0xA5u;
    }

    gbl_machine_reset(machine);
    gbl_machine_reset(machine);
    CHECK(gbl_test_canonical_equal(machine, fresh));
    CHECK(gbl_test_canonical_digest(machine) ==
          gbl_test_canonical_digest(fresh));
    CHECK(machine->initialized);
    CHECK(machine->master_cycles == 0u);
    CHECK(machine->open_bus == 0u);
    CHECK(machine->dispcnt == 0u);
    CHECK(machine->rom == NULL);
    CHECK(machine->rom_length == 0u);
    CHECK(bytes_are_zero(machine->ewram, GBL_EWRAM_SIZE));
    CHECK(bytes_are_zero(machine->iwram, GBL_IWRAM_SIZE));
    CHECK(bytes_are_zero(machine->pram, GBL_PRAM_SIZE));
    CHECK(bytes_are_zero(machine->vram, GBL_VRAM_SIZE));
    CHECK(bytes_are_zero(machine->oam, GBL_OAM_SIZE));

    {
        gbl_machine *raw = malloc(sizeof(*raw));
        gbl_machine *raw_copy = malloc(sizeof(*raw_copy));
        gbl_allocator incomplete = { NULL, test_allocate, NULL };
        gbl_allocator no_callbacks = { NULL, NULL, NULL };
        unsigned int release_count = 0u;
        uint64_t disposed_digest;

        if (raw == NULL || raw_copy == NULL) {
            fprintf(stderr, "machine.state: host allocation failed\n");
            free(raw);
            free(raw_copy);
            free_machine(machine);
            free_machine(other);
            free_machine(fresh);
            return 1;
        }
        memset(raw, 0xA5, sizeof(*raw));
        memcpy(raw_copy, raw, sizeof(*raw));
        CHECK(gbl_machine_init(raw, &incomplete) ==
              GBL_BUS_STATUS_INVALID_ARGUMENT);
        CHECK(memcmp(raw, raw_copy, sizeof(*raw)) == 0);
        CHECK(gbl_machine_init(raw, &raw->allocator) ==
              GBL_BUS_STATUS_INVALID_ARGUMENT);
        CHECK(memcmp(raw, raw_copy, sizeof(*raw)) == 0);

        CHECK(gbl_machine_init(raw, &no_callbacks) == GBL_BUS_STATUS_OK);
        CHECK(raw->initialized);
        CHECK(raw->allocator.allocate == NULL);
        CHECK(raw->allocator.release == NULL);
        CHECK(raw->master_cycles == 0u);
        CHECK(raw->open_bus == 0u);
        CHECK(raw->dispcnt == 0u);
        CHECK(bytes_are_zero(raw->ewram, GBL_EWRAM_SIZE));
        CHECK(bytes_are_zero(raw->iwram, GBL_IWRAM_SIZE));
        CHECK(bytes_are_zero(raw->pram, GBL_PRAM_SIZE));
        CHECK(bytes_are_zero(raw->vram, GBL_VRAM_SIZE));
        CHECK(bytes_are_zero(raw->oam, GBL_OAM_SIZE));
        gbl_machine_dispose(raw);
        CHECK(!raw->initialized);
        CHECK(raw->rom == NULL);
        CHECK(raw->rom_length == 0u);
        CHECK(raw->allocator.context == NULL);
        CHECK(raw->allocator.allocate == NULL);
        CHECK(raw->allocator.release == NULL);
        CHECK(raw->master_cycles == 0u);
        CHECK(raw->open_bus == 0u);
        CHECK(raw->dispcnt == 0u);
        CHECK(bytes_are_zero(raw->ewram, GBL_EWRAM_SIZE));
        CHECK(bytes_are_zero(raw->iwram, GBL_IWRAM_SIZE));
        CHECK(bytes_are_zero(raw->pram, GBL_PRAM_SIZE));
        CHECK(bytes_are_zero(raw->vram, GBL_VRAM_SIZE));
        CHECK(bytes_are_zero(raw->oam, GBL_OAM_SIZE));
        disposed_digest = gbl_test_canonical_digest(raw);
        gbl_machine_dispose(raw);
        CHECK(gbl_test_canonical_digest(raw) == disposed_digest);
        free(raw);
        free(raw_copy);

        {
            gbl_machine *tracked = calloc(1u, sizeof(*tracked));
            gbl_allocator tracking = { &release_count, test_allocate,
                                       test_release };

            if (tracked == NULL) {
                fprintf(stderr, "machine.state: host allocation failed\n");
                free_machine(machine);
                free_machine(other);
                free_machine(fresh);
                return 1;
            }
            CHECK(gbl_machine_init(tracked, &tracking) == GBL_BUS_STATUS_OK);
            tracked->ewram[0] = 0xFFu;
            tracked->master_cycles = 9u;
            gbl_machine_reset(tracked);
            CHECK(tracked->initialized);
            CHECK(tracked->allocator.context == &release_count);
            CHECK(tracked->allocator.allocate == test_allocate);
            CHECK(tracked->allocator.release == test_release);
            CHECK(tracked->master_cycles == 0u);
            CHECK(tracked->ewram[0] == 0u);
            gbl_machine_dispose(tracked);
            CHECK(release_count == 0u);
            CHECK(tracked->allocator.context == NULL);
            free(tracked);
        }
    }

    free_machine(machine);
    free_machine(other);
    free_machine(fresh);
    if (CHECK_DONE() == 0) {
        puts("machine: state ok");
        return 0;
    }
    return 1;
}

static int run_irq_registers(void)
{
    gbl_machine *machine = new_machine();
    gbl_bus_result result;

    CHECK(machine != NULL);
    if (machine == NULL) {
        return 1;
    }

    result = gbl_bus_write(machine, UINT32_C(0x04000200), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x05));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          result.outcome == GBL_BUS_OUTCOME_IO_INTERRUPT_SUBSET);
    result = gbl_bus_write(machine, UINT32_C(0x04000201), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFF));
    CHECK(result.status == GBL_BUS_STATUS_OK && machine->interrupt_enable ==
          UINT16_C(0x3F05));
    result = gbl_bus_read(machine, UINT32_C(0x04000200), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == UINT32_C(0x3F05));

    machine->interrupt_flags = UINT16_C(0x000C);
    result = gbl_bus_write(machine, UINT32_C(0x04000202), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x04));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->interrupt_flags == UINT16_C(0x0008));
    machine->interrupt_flags = UINT16_C(0x000C);
    result = gbl_bus_write(machine, UINT32_C(0x04000202), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x0004));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->interrupt_flags == UINT16_C(0x0008));
    result = gbl_bus_read(machine, UINT32_C(0x04000202), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == 8u);
    result = gbl_bus_write(machine, UINT32_C(0x04000200), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0xFFFF0005));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->interrupt_enable == UINT16_C(0x0005) &&
          machine->interrupt_flags == 0u);
    result = gbl_bus_read(machine, UINT32_C(0x04000200), 4u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == 5u);

    result = gbl_bus_write(machine, UINT32_C(0x04000208), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFE));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->interrupt_master_enable == 0u);
    result = gbl_bus_write(machine, UINT32_C(0x04000209), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFF));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->interrupt_master_enable == 0u);
    result = gbl_bus_write(machine, UINT32_C(0x04000208), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(1));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->interrupt_master_enable == 1u);
    result = gbl_bus_read(machine, UINT32_C(0x04000208), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == 1u);
    CHECK((machine->interrupt_enable & UINT16_C(0xC000)) == 0u);
    CHECK((machine->interrupt_flags & UINT16_C(0xC000)) == 0u);
    gbl_machine_reset(machine);
    CHECK(machine->interrupt_enable == 0u && machine->interrupt_flags == 0u &&
          machine->interrupt_master_enable == 0u);

    free_machine(machine);

    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        result = gbl_bus_write(machine, UINT32_C(0x04000100), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFFFF));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        result = gbl_bus_write(machine, UINT32_C(0x04000102), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xC0));
        CHECK(result.status == GBL_BUS_STATUS_OK && machine->event_count == 1u);
        result = gbl_bus_write(machine, UINT32_C(0x04000202), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x08));
        CHECK(result.status == GBL_BUS_STATUS_OK &&
              machine->master_cycles == UINT64_C(3) &&
              machine->interrupt_flags == 0u);
        result = gbl_bus_write(machine, UINT32_C(0x04000102), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x80));
        CHECK(result.status == GBL_BUS_STATUS_OK &&
              (machine->interrupt_flags & UINT16_C(0x0008)) != 0u);
        CHECK((machine->interrupt_enable & UINT16_C(0x0008)) == 0u);
        free_machine(machine);
    }

    if (CHECK_DONE() == 0) {
        puts("bus.irq.registers: IE/IF/IME lanes, masks and event ordering ok");
        return 0;
    }
    return 1;
}

static int run_keypad_registers(void)
{
    gbl_machine *machine = new_machine();
    gbl_bus_result result;
    gbl_input_event pressed = { UINT64_C(2), UINT16_C(0x0001) };
    gbl_cpu_state cpu_before;

    CHECK(machine != NULL);
    if (machine == NULL) {
        return 1;
    }

    result = gbl_bus_read(machine, UINT32_C(0x04000130), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          result.outcome == GBL_BUS_OUTCOME_IO_KEYPAD_SUBSET &&
          result.value == UINT32_C(0x03FF));
    result = gbl_bus_read(machine, UINT32_C(0x04000130), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == UINT32_C(0xFF));
    result = gbl_bus_read(machine, UINT32_C(0x04000131), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == UINT32_C(0x03));
    result = gbl_bus_write(machine, UINT32_C(0x04000130), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0u);
    CHECK(result.status == GBL_BUS_STATUS_OK && machine->pressed_keys == 0u);
    result = gbl_bus_read(machine, UINT32_C(0x04000130), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          result.value == UINT32_C(0x03FF));

    result = gbl_bus_write(machine, UINT32_C(0x04000132), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x01));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          result.outcome == GBL_BUS_OUTCOME_IO_KEYPAD_SUBSET);
    result = gbl_bus_write(machine, UINT32_C(0x04000133), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x40));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->key_control == UINT16_C(0x4001));
    result = gbl_bus_read(machine, UINT32_C(0x04000132), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == UINT32_C(0x01));
    result = gbl_bus_read(machine, UINT32_C(0x04000133), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == UINT32_C(0x40));
    result = gbl_bus_write(machine, UINT32_C(0x04000130), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0xFFFFC3FF));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          result.outcome == GBL_BUS_OUTCOME_IO_KEYPAD_SUBSET &&
          machine->key_control == UINT16_C(0xC3FF) &&
          machine->pressed_keys == 0u);
    result = gbl_bus_write(machine, UINT32_C(0x04000130), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFF));
    CHECK(result.status == GBL_BUS_STATUS_OK && machine->pressed_keys == 0u);
    result = gbl_bus_read(machine, UINT32_C(0x04000130), 4u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          result.outcome == GBL_BUS_OUTCOME_IO_KEYPAD_SUBSET &&
          result.value == UINT32_C(0xC3FF03FF));

    gbl_machine_reset(machine);
    result = gbl_bus_write(machine, UINT32_C(0x04000100), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x00C0FFFF));
    CHECK(result.status == GBL_BUS_STATUS_OK && machine->master_cycles == 1u &&
          machine->event_count == 1u &&
          machine->events[0].cycle == UINT64_C(2));
    pressed.cycle = UINT64_C(2);
    CHECK(gbl_machine_set_input_timeline(machine, &pressed, 1u) ==
          GBL_TIME_STATUS_OK);
    CHECK(machine->event_count == 2u &&
          machine->events[0].kind ==
              (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW &&
          machine->events[0].priority == 0u &&
          machine->events[1].kind ==
              (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT &&
          machine->events[1].priority == 4u);
    cpu_before = machine->cpu;
    result = gbl_bus_write(machine, UINT32_C(0x04000132), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x4001));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->master_cycles == UINT64_C(2) &&
          machine->pressed_keys == UINT16_C(0x0001) &&
          machine->key_condition && machine->key_irq_condition &&
          machine->interrupt_flags == UINT16_C(0x1008));
    CHECK(machine->cpu.cpsr == cpu_before.cpsr &&
          machine->cpu.execution_address == cpu_before.execution_address &&
          machine->cpu.retired_instructions ==
              cpu_before.retired_instructions &&
          machine->cpu.last_exception.kind == GBL_EXCEPTION_NONE &&
          !machine->cpu.stopped);
    result = gbl_bus_read(machine, UINT32_C(0x04000130), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          result.outcome == GBL_BUS_OUTCOME_IO_KEYPAD_SUBSET &&
          result.value == UINT32_C(0x03FE) &&
          machine->master_cycles == UINT64_C(3));

    gbl_machine_reset(machine);
    result = gbl_bus_write(machine, UINT32_C(0x04000132), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x4001));
    CHECK(result.status == GBL_BUS_STATUS_OK && machine->master_cycles == 1u);
    result = gbl_bus_write(machine, UINT32_C(0x04000100), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x00C0FFFF));
    CHECK(result.status == GBL_BUS_STATUS_OK && machine->master_cycles == 2u);
    pressed.cycle = UINT64_C(3);
    CHECK(gbl_machine_set_input_timeline(machine, &pressed, 1u) ==
          GBL_TIME_STATUS_OK);
    CHECK(machine->event_count == 2u &&
          machine->events[0].kind ==
              (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW &&
          machine->events[0].priority == 0u &&
          machine->events[1].kind ==
              (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT &&
          machine->events[1].priority == 4u);
    result = gbl_bus_write(machine, UINT32_C(0x04000202), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x1000));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->master_cycles == UINT64_C(3) &&
          machine->pressed_keys == UINT16_C(0x0001) &&
          machine->key_irq_condition &&
          machine->interrupt_flags == UINT16_C(0x0008));
    result = gbl_bus_read(machine, UINT32_C(0x04000130), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          result.value == UINT32_C(0x03FE));

    gbl_machine_reset(machine);
    result = gbl_bus_write(machine, UINT32_C(0x04000132), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x4001));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    {
        const gbl_input_event equal_cycle[] = {
            { UINT64_C(2), 0u },
            { UINT64_C(2), UINT16_C(0x0001) },
            { UINT64_C(2), 0u },
            { UINT64_C(2), UINT16_C(0x0001) }
        };

        CHECK(gbl_machine_set_input_timeline(
                  machine, equal_cycle,
                  sizeof(equal_cycle) / sizeof(equal_cycle[0])) ==
              GBL_TIME_STATUS_OK);
        CHECK(gbl_machine_advance_to(machine, UINT64_C(2)) ==
              GBL_TIME_STATUS_OK);
        CHECK(machine->input_event_cursor == 4u &&
              machine->pressed_keys == UINT16_C(0x0001) &&
              machine->key_condition && machine->key_irq_condition &&
              machine->interrupt_flags == UINT16_C(0x1000));
    }

    free_machine(machine);
    if (CHECK_DONE() == 0) {
        puts("bus.keypad.registers: register lanes, event ties and IF order ok");
        return 0;
    }
    return 1;
}

static int run_waitcnt_matrix(void)
{
    static const uint32_t window_bases[3] = {
        UINT32_C(0x08000000), UINT32_C(0x0A000000),
        UINT32_C(0x0C000000)
    };
    static const uint8_t first_waits[4] = { 4u, 3u, 2u, 8u };
    static const uint8_t widths[3] = { 1u, 2u, 4u };
    gbl_machine *machine = new_machine();
    size_t window;
    size_t first_selector;
    size_t short_selector;
    size_t width_index;
    unsigned int sequence_index;
    unsigned int direction_index;
    unsigned int role_index;

    if (machine == NULL) {
        fprintf(stderr, "waitcnt.matrix: machine allocation failed\n");
        return 1;
    }
    for (window = 0u; window < 3u; ++window) {
        for (first_selector = 0u; first_selector < 4u; ++first_selector) {
            for (short_selector = 0u; short_selector < 2u;
                 ++short_selector) {
                const uint8_t first_shift =
                    (uint8_t)(2u + (uint8_t)(window * 3u));
                const uint8_t sequential_shift =
                    (uint8_t)(4u + (uint8_t)(window * 3u));
                const uint64_t sequential_cycles =
                    short_selector != 0u
                        ? UINT64_C(2)
                        : window == 2u ? UINT64_C(9) : UINT64_C(3);
                const uint64_t nonsequential_cycles =
                    (uint64_t)first_waits[first_selector] + UINT64_C(1);

                machine->waitcnt = (uint16_t)(
                    ((uint16_t)first_selector << first_shift) |
                    ((uint16_t)short_selector << sequential_shift));
                for (width_index = 0u; width_index < 3u; ++width_index) {
                    for (sequence_index = 0u; sequence_index < 2u;
                         ++sequence_index) {
                        const gbl_bus_access_class sequence =
                            sequence_index == 0u
                                ? GBL_BUS_CLASS_NONSEQUENTIAL
                                : GBL_BUS_CLASS_SEQUENTIAL;
                        const uint64_t first_cycles =
                            sequence == GBL_BUS_CLASS_NONSEQUENTIAL
                                ? nonsequential_cycles
                                : sequential_cycles;
                        const uint64_t expected_cycles =
                            widths[width_index] == 4u
                                ? first_cycles + sequential_cycles
                                : first_cycles;

                        for (direction_index = 0u; direction_index < 2u;
                             ++direction_index) {
                            const gbl_bus_direction direction =
                                direction_index == 0u
                                    ? GBL_BUS_DIRECTION_READ
                                    : GBL_BUS_DIRECTION_WRITE;

                            for (role_index = 0u; role_index < 2u;
                                 ++role_index) {
                                const gbl_bus_access_role role =
                                    role_index == 0u
                                        ? GBL_BUS_ROLE_FETCH
                                        : GBL_BUS_ROLE_DATA;
                                gbl_cycle_cost cost = {
                                    0u, GBL_CYCLE_COST_PROJECT_SELECTED,
                                    NULL
                                };

                                if (direction == GBL_BUS_DIRECTION_WRITE &&
                                    role == GBL_BUS_ROLE_FETCH) {
                                    continue;
                                }
                                CHECK(gbl_bus_quote(
                                    machine, window_bases[window] +
                                                 UINT32_C(0x100),
                                    direction, widths[width_index],
                                    sequence, role, &cost));
                                CHECK(cost.cycles == expected_cycles);
                                CHECK(cost.provenance ==
                                      GBL_CYCLE_COST_SOURCE_BACKED);
                                CHECK_STREQ(
                                    cost.model,
                                    "nintendo-agb-programming-manual-v1.1");
                            }
                        }
                    }
                }
            }
        }
    }
    free_machine(machine);
    if (CHECK_DONE() == 0) {
        puts("waitcnt.matrix: all windows, selectors, widths, roles and directions ok");
        return 0;
    }
    return 1;
}

static int run_waitcnt_boundary(void)
{
    static const uint32_t window_bases[3] = {
        UINT32_C(0x08000000), UINT32_C(0x0A000000),
        UINT32_C(0x0C000000)
    };
    static const uint64_t nonsequential_cycles[3] = {
        UINT64_C(5), UINT64_C(4), UINT64_C(3)
    };
    static const uint64_t sequential_cycles[3] = {
        UINT64_C(3), UINT64_C(5), UINT64_C(9)
    };
    gbl_machine *machine = new_machine();
    size_t window;
    bool passed = true;

    if (machine == NULL) {
        puts("TAP version 13");
        puts("1..1");
        puts("not ok 1 - waitcnt.boundary forces nonsequential beats at 128 KiB thresholds");
        return 1;
    }
    for (window = 0u; window < 3u; ++window) {
        const uint32_t boundary =
            window_bases[window] + UINT32_C(0x20000);
        const uint32_t byte_addresses[3] = {
            boundary - 1u, boundary, boundary + 1u
        };
        const uint32_t halfword_addresses[3] = {
            boundary - 2u, boundary, boundary + 2u
        };
        const uint32_t word_addresses[3] = {
            boundary - 2u, boundary, boundary + 4u
        };
        size_t neighbor;

        for (neighbor = 0u; neighbor < 3u; ++neighbor) {
            gbl_cycle_cost cost = { 0u, GBL_CYCLE_COST_PROJECT_SELECTED,
                                    NULL };
            const uint64_t expected =
                neighbor == 1u ? nonsequential_cycles[window]
                               : sequential_cycles[window];
            bool quoted;

            quoted = gbl_bus_quote(machine, byte_addresses[neighbor],
                                   GBL_BUS_DIRECTION_READ, 1u,
                                   GBL_BUS_CLASS_SEQUENTIAL,
                                   GBL_BUS_ROLE_DATA, &cost);
            passed = passed && quoted && cost.cycles == expected;

            cost.cycles = 0u;
            quoted = gbl_bus_quote(machine, halfword_addresses[neighbor],
                                   GBL_BUS_DIRECTION_READ, 2u,
                                   GBL_BUS_CLASS_SEQUENTIAL,
                                   GBL_BUS_ROLE_DATA, &cost);
            passed = passed && quoted && cost.cycles == expected;

            cost.cycles = 0u;
            quoted = gbl_bus_quote(machine, word_addresses[neighbor],
                                   GBL_BUS_DIRECTION_READ, 4u,
                                   GBL_BUS_CLASS_SEQUENTIAL,
                                   GBL_BUS_ROLE_DATA, &cost);
            passed = passed && quoted &&
                     cost.cycles ==
                         (neighbor == 0u
                              ? sequential_cycles[window] +
                                    nonsequential_cycles[window]
                              : neighbor == 1u
                                    ? nonsequential_cycles[window] +
                                          sequential_cycles[window]
                                    : sequential_cycles[window] *
                                          UINT64_C(2));
        }
    }
    free_machine(machine);
    puts("TAP version 13");
    puts("1..1");
    puts(passed
             ? "ok 1 - waitcnt.boundary forces nonsequential beats at 128 KiB thresholds"
             : "not ok 1 - waitcnt.boundary forces nonsequential beats at 128 KiB thresholds");
    return passed ? 0 : 1;
}

static int run_waitcnt_registers(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *snapshot = calloc(1u, sizeof(*snapshot));
    gbl_machine_time_preview preview;
    gbl_time_operation operation = {
        GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x04000204),
        UINT32_C(0x0008), UINT64_C(1)
    };
    gbl_cycle_cost before = { 0u, GBL_CYCLE_COST_PROJECT_SELECTED, NULL };
    gbl_cycle_cost after = { 0u, GBL_CYCLE_COST_PROJECT_SELECTED, NULL };
    gbl_bus_result result;

    if (machine == NULL || snapshot == NULL) {
        fprintf(stderr, "waitcnt.registers: machine allocation failed\n");
        free_machine(machine);
        free(snapshot);
        return 1;
    }
    CHECK(machine->waitcnt == 0u);
    CHECK(gbl_bus_quote(machine, UINT32_C(0x08000000),
                        GBL_BUS_DIRECTION_READ, 2u,
                        GBL_BUS_CLASS_NONSEQUENTIAL,
                        GBL_BUS_ROLE_DATA, &before));
    CHECK(before.cycles == UINT64_C(5));

    result = gbl_bus_write(machine, UINT32_C(0x04000204), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_MAX);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(machine->waitcnt == UINT16_C(0x5FFF));
    result = gbl_bus_read(machine, UINT32_C(0x04000204), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.value == UINT32_C(0x5FFF));

    result = gbl_bus_write(machine, UINT32_C(0x04000204), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x1FFF));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->waitcnt == UINT16_C(0x1FFF));
    CHECK(gbl_bus_quote(machine, UINT32_C(0x08000000),
                        GBL_BUS_DIRECTION_READ, 2u,
                        GBL_BUS_CLASS_NONSEQUENTIAL,
                        GBL_BUS_ROLE_DATA, &before));
    result = gbl_bus_write(machine, UINT32_C(0x04000204), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x5FFF));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->waitcnt == UINT16_C(0x5FFF));
    CHECK(gbl_bus_quote(machine, UINT32_C(0x08000000),
                        GBL_BUS_DIRECTION_READ, 2u,
                        GBL_BUS_CLASS_NONSEQUENTIAL,
                        GBL_BUS_ROLE_DATA, &after));
    CHECK(before.cycles == after.cycles);

    CHECK(gbl_machine_prepare_operations(machine, &operation, 1u,
                                         &preview) == GBL_TIME_STATUS_OK);
    CHECK(machine->waitcnt == UINT16_C(0x5FFF));
    CHECK(preview.waitcnt == UINT16_C(0x0008));
    gbl_machine_commit_time(machine, &preview);
    CHECK(machine->waitcnt == UINT16_C(0x0008));
    CHECK(gbl_bus_quote(machine, UINT32_C(0x08000000),
                        GBL_BUS_DIRECTION_READ, 2u,
                        GBL_BUS_CLASS_NONSEQUENTIAL,
                        GBL_BUS_ROLE_DATA, &after));
    CHECK(after.cycles == UINT64_C(3));

    machine->open_bus = UINT32_C(0x76543210);
    result = gbl_bus_read_fetch(machine, 0u, 4u,
                                GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_BIOS_ABSENT);
    CHECK(result.value == UINT32_C(0x76543210));

    machine->master_cycles = UINT64_MAX;
    memcpy(snapshot, machine, sizeof(*snapshot));
    result = gbl_bus_write(machine, UINT32_C(0x04000204), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFFFF));
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.cycles == 0u);
    CHECK(gbl_test_canonical_equal(machine, snapshot));
    CHECK(gbl_test_same_instance_identity_equal(machine, snapshot));

    free_machine(machine);
    free(snapshot);
    if (CHECK_DONE() == 0) {
        puts("waitcnt.registers: lanes, mask, prefetch invariance and atomic writes ok");
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: bus_test <timing|state|decode|video|irq.registers|keypad.registers|waitcnt.matrix|waitcnt.boundary|waitcnt.registers>\n");
        return 2;
    }
    if (strcmp(argv[1], "timing") == 0) {
        return run_timing();
    }
    if (strcmp(argv[1], "state") == 0) {
        return run_state();
    }
    if (strcmp(argv[1], "decode") == 0) {
        return run_decode();
    }
    if (strcmp(argv[1], "video") == 0) {
        return run_video();
    }
    if (strcmp(argv[1], "irq.registers") == 0) {
        return run_irq_registers();
    }
    if (strcmp(argv[1], "keypad.registers") == 0) {
        return run_keypad_registers();
    }
    if (strcmp(argv[1], "waitcnt.matrix") == 0) {
        return run_waitcnt_matrix();
    }
    if (strcmp(argv[1], "waitcnt.boundary") == 0) {
        return run_waitcnt_boundary();
    }
    if (strcmp(argv[1], "waitcnt.registers") == 0) {
        return run_waitcnt_registers();
    }
    fprintf(stderr, "bus_test: unknown selector: %s\n", argv[1]);
    return 2;
}
