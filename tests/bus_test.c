#include "check.h"
#include "gamebloke/gamebloke.h"
#include "../src/ppu_internal.h"
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

static const gbl_event *find_bus_event(const gbl_machine *machine,
                                       uint8_t kind, uint8_t timer_index)
{
    size_t index;

    if (machine == NULL) {
        return NULL;
    }
    for (index = 0u; index < machine->event_count; ++index) {
        const gbl_event *event = &machine->events[index];

        if (event->live && event->kind == kind &&
            (kind != (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW ||
             event->timer_index == timer_index)) {
            return event;
        }
    }
    return NULL;
}

static bool invalidate_ppu_event_for_synthetic_clock(gbl_machine *machine,
                                                      uint64_t target_cycle)
{
    const gbl_event *event = find_bus_event(
        machine, (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY, 0u);

    if (event == NULL || event->generation != machine->ppu.generation ||
        event->cycle >= target_cycle || machine->ppu.generation == UINT64_MAX) {
        return false;
    }
    /* This test jumps to a synthetic clock boundary. Make the earlier row
     * sample stale explicitly, rather than asking production scheduling to
     * process an unbounded interval. */
    ++machine->ppu.generation;
    machine->master_cycles = target_cycle;
    return event->generation != machine->ppu.generation;
}

static size_t count_bus_events(const gbl_machine *machine, uint8_t kind)
{
    size_t index;
    size_t count = 0u;

    if (machine == NULL) {
        return 0u;
    }
    for (index = 0u; index < machine->event_count; ++index) {
        if (machine->events[index].live && machine->events[index].kind == kind) {
            ++count;
        }
    }
    return count;
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
        const uint64_t expected_cycles =
            cases[case_index].outcome == GBL_BUS_OUTCOME_ROM ||
                    cases[case_index].outcome == GBL_BUS_OUTCOME_ROM_TAIL
                ? 5u
                : 1u;
        gbl_bus_result result;

        CHECK(cases[case_index].reference_section[0] != '\0');
        machine->open_bus = latch;
        result = gbl_bus_read(machine, cases[case_index].address, 1u,
                              GBL_BUS_CLASS_NONSEQUENTIAL);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == cases[case_index].outcome);
        CHECK(result.cycles == expected_cycles);
        CHECK(result.access_class == GBL_BUS_CLASS_NONSEQUENTIAL);
        CHECK(machine->master_cycles == before_cycles + expected_cycles);
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

    gbl_machine_reset(machine);
    CHECK(machine->event_count == 1u && machine->events[0].live &&
          machine->events[0].kind ==
              (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY &&
          machine->events[0].cycle == 0u);
    CHECK(invalidate_ppu_event_for_synthetic_clock(
        machine, UINT64_MAX - UINT64_C(1)));
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
    CHECK(result.cycles == 8u);
    CHECK(machine->master_cycles == 8u);

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
    CHECK((machine->dispcnt & UINT16_C(0x0007)) == (uint16_t)mode);
    result = gbl_bus_read(machine, 0x04000000u, 1u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    CHECK(result.value == (uint32_t)mode);
    CHECK(result.cycles == 1u);
    result = gbl_bus_read(machine, 0x04000001u, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    CHECK(result.value == (uint32_t)(machine->dispcnt >> 8u));

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

    /* DISPCNT retains its full control word; adjacent I/O remains unsupported. */
    result = gbl_bus_write(machine, 0x04000000u, 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xFFu);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    CHECK(machine->dispcnt == UINT16_C(0x00FF));
    result = gbl_bus_write(machine, 0x04000001u, 1u,
                           GBL_BUS_CLASS_SEQUENTIAL, 0xFFu);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_MODE_SUBSET);
    CHECK(machine->dispcnt == UINT16_C(0xFFFF));
    machine->open_bus = 0xAABBCCDDu;
    result = gbl_bus_write(machine, 0x04000000u, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0x44332203u);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);
    CHECK(result.cycles == 1u);
    CHECK(machine->dispcnt == UINT16_C(0x2203));
    CHECK(machine->open_bus == 0xAABBCCDDu);
    result = gbl_bus_read(machine, 0x04000000u, 4u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_IO_UNIMPLEMENTED);
    CHECK(result.value == UINT32_C(0xAABB2203));
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

static int run_ppu_tracer(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *cpu_machine = new_machine();
    gbl_machine *refusal_machine = new_machine();
    gbl_machine *refusal_snapshot = NULL;
    gbl_bus_result result;
    gbl_machine_time_preview preview;
    gbl_machine_time_preview preview_sentinel;
    const gbl_time_operation crossing_write = {
        GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x060001E0),
        UINT32_C(0x4321), UINT64_C(1)
    };
    gbl_run_options options = { 0 };
    gbl_run_result run = { 0 };
    gbl_run_status run_status;
    uint32_t row0_pixel;

    if (machine == NULL || cpu_machine == NULL || refusal_machine == NULL) {
        fprintf(stderr, "ppu.tracer: host allocation failed\n");
        free_machine(refusal_machine);
        free_machine(cpu_machine);
        free_machine(machine);
        return 1;
    }

    /* A fresh machine has exactly one queued line-start event at cycle zero. */
    CHECK(machine->master_cycles == 0u);
    CHECK(machine->event_count == 1u);
    CHECK(machine->events[0].live);
    CHECK(machine->events[0].cycle == 0u);
    CHECK(machine->events[0].kind ==
          (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY);
    CHECK(machine->events[0].priority == 9u);

    /* Start in the supported model before the cycle-zero row sample. */
    machine->dispcnt = UINT16_C(0x0403);
    machine->ppu.dispcnt = UINT16_C(0x0403);
    result = gbl_bus_write(machine, UINT32_C(0x04000000), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x0403));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(machine->dispcnt == UINT16_C(0x0403));
    CHECK(machine->ppu.dispcnt == UINT16_C(0x0403));
    result = gbl_bus_write(machine, UINT32_C(0x060001E0), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x1234));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    result = gbl_bus_write(machine, UINT32_C(0x060003BE), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x9234));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    result = gbl_bus_write(machine, UINT32_C(0x060003C0), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x7C00));
    CHECK(result.status == GBL_BUS_STATUS_OK);

    CHECK(gbl_machine_advance_to(machine, UINT64_C(1005)) ==
          GBL_TIME_STATUS_OK);
    CHECK(machine->ppu.line_origin == 0u);
    CHECK((machine->ppu.dispstat & UINT16_C(0x0002)) == 0u);
    CHECK(machine->event_count == 1u);
    CHECK(machine->events[0].cycle == UINT64_C(1006));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(1006)) ==
          GBL_TIME_STATUS_OK);
    CHECK((machine->ppu.dispstat & UINT16_C(0x0002)) != 0u);
    CHECK(machine->events[0].cycle == UINT64_C(1232));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(1231)) ==
          GBL_TIME_STATUS_OK);

    /* A write completing on the row-start cycle follows the PPU sample. */
    result = gbl_bus_write(machine, UINT32_C(0x060001E0), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x4321));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(machine->master_cycles == UINT64_C(1232));
    CHECK(machine->ppu.line == 1u);
    CHECK(machine->ppu.line_origin == UINT64_C(1232));
    CHECK(machine->ppu.captured_rows == 2u);
    CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_INCOMPLETE);
    CHECK((machine->ppu.row_coverage[0] & UINT8_C(0x03)) == UINT8_C(0x03));
    CHECK(machine->vram[GBL_PPU_ROW_BYTES] == UINT8_C(0x21));
    CHECK(machine->vram[GBL_PPU_ROW_BYTES + 1u] == UINT8_C(0x43));
    CHECK(machine->video_frame[GBL_PPU_ROW_BYTES] == UINT8_C(0x34));
    CHECK(machine->video_frame[GBL_PPU_ROW_BYTES + 1u] == UINT8_C(0x12));
    CHECK(machine->video_frame[2u * GBL_PPU_ROW_BYTES - 2u] ==
          UINT8_C(0x34));
    CHECK(machine->video_frame[2u * GBL_PPU_ROW_BYTES - 1u] ==
          UINT8_C(0x12));

    CHECK(gbl_machine_advance_to(machine, UINT64_C(2237)) ==
          GBL_TIME_STATUS_OK);
    CHECK((machine->ppu.dispstat & UINT16_C(0x0002)) == 0u);
    CHECK(machine->events[0].cycle == UINT64_C(2238));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(2238)) ==
          GBL_TIME_STATUS_OK);
    CHECK((machine->ppu.dispstat & UINT16_C(0x0002)) != 0u);
    CHECK(machine->events[0].cycle == UINT64_C(2464));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(2464)) ==
          GBL_TIME_STATUS_OK);
    CHECK(machine->ppu.line == 2u);
    CHECK(machine->ppu.line_origin == UINT64_C(2464));
    CHECK(machine->ppu.captured_rows == 3u);
    CHECK(machine->video_frame[2u * GBL_PPU_ROW_BYTES] == UINT8_C(0x00));
    CHECK(machine->video_frame[2u * GBL_PPU_ROW_BYTES + 1u] ==
          UINT8_C(0x7C));

    /* A transaction that would sample a row and then exhausts event sequence
     * capacity leaves both canonical video and the caller preview untouched. */
    refusal_machine->dispcnt = UINT16_C(0x0403);
    refusal_machine->ppu.dispcnt = UINT16_C(0x0403);
    result = gbl_bus_write(refusal_machine, UINT32_C(0x04000000), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x0403));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    result = gbl_bus_write(refusal_machine, UINT32_C(0x060001E0), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x1357));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(gbl_machine_advance_to(refusal_machine, UINT64_C(1231)) ==
          GBL_TIME_STATUS_OK);
    refusal_machine->next_event_sequence = UINT64_MAX;
    refusal_snapshot = malloc(sizeof(*refusal_snapshot));
    if (refusal_snapshot == NULL) {
        fprintf(stderr, "ppu.tracer: refusal snapshot allocation failed\n");
        free_machine(refusal_machine);
        free_machine(cpu_machine);
        free_machine(machine);
        return 1;
    }
    memcpy(refusal_snapshot, refusal_machine, sizeof(*refusal_snapshot));
    memset(&preview, 0xA5, sizeof(preview));
    memcpy(&preview_sentinel, &preview, sizeof(preview_sentinel));
    CHECK(gbl_machine_prepare_operations(refusal_machine, &crossing_write, 1u,
                                         &preview) ==
          GBL_TIME_STATUS_SEQUENCE_EXHAUSTED);
    CHECK(memcmp(&preview, &preview_sentinel, sizeof(preview)) == 0);
    CHECK(gbl_test_canonical_equal(refusal_machine, refusal_snapshot));
    CHECK(gbl_test_same_instance_identity_equal(refusal_machine,
                                                refusal_snapshot));
    result = gbl_bus_write(refusal_machine, crossing_write.address,
                           crossing_write.width,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           crossing_write.value);
    CHECK(result.status == GBL_BUS_STATUS_TIME_REFUSAL);
    CHECK(gbl_test_canonical_equal(refusal_machine, refusal_snapshot));
    CHECK(gbl_test_same_instance_identity_equal(refusal_machine,
                                                refusal_snapshot));

    /* The CPU executes a real instruction whose fetch crosses the next
     * visible-line start; its compact preview must commit the same row. */
    cpu_machine->dispcnt = UINT16_C(0x0403);
    cpu_machine->ppu.dispcnt = UINT16_C(0x0403);
    result = gbl_bus_write(cpu_machine, UINT32_C(0x04000000), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x0403));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    result = gbl_bus_write(cpu_machine, UINT32_C(0x060001E0), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x4567));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    cpu_machine->rom = malloc(64u);
    if (cpu_machine->rom == NULL) {
        fprintf(stderr, "ppu.tracer: synthetic ROM allocation failed\n");
        free_machine(cpu_machine);
        free_machine(machine);
        return 1;
    }
    cpu_machine->rom_length = 64u;
    cpu_machine->allocator.context = NULL;
    cpu_machine->allocator.allocate = rom_allocate;
    cpu_machine->allocator.release = rom_release;
    {
        size_t byte_index;

        for (byte_index = 0u; byte_index < cpu_machine->rom_length;
             byte_index += 4u) {
            cpu_machine->rom[byte_index] = UINT8_C(0x00);
            cpu_machine->rom[byte_index + 1u] = UINT8_C(0x00);
            cpu_machine->rom[byte_index + 2u] = UINT8_C(0xA0);
            cpu_machine->rom[byte_index + 3u] = UINT8_C(0xE1);
        }
    }
    CHECK(gbl_machine_advance_to(cpu_machine, UINT64_C(1228)) ==
          GBL_TIME_STATUS_OK);
    cpu_machine->cpu.execution_address = UINT32_C(0x08000000);
    cpu_machine->cpu.cpsr = UINT32_C(0x000000DF);
    cpu_machine->cpu.pipeline[0] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000000), UINT32_C(0xE1A00000), 4u, true
    };
    cpu_machine->cpu.pipeline[1] = (gbl_cpu_pipeline_entry){
        UINT32_C(0x08000004), UINT32_C(0xE1A00000), 4u, true
    };
    cpu_machine->cpu.next_fetch_sequential = true;
    options.cycle_budget = UINT64_C(32);
    options.unit_limit = 1u;
    run_status = gbl_cpu_run(cpu_machine, &options, &run);
    row0_pixel = (uint32_t)cpu_machine->video_frame[GBL_PPU_ROW_BYTES] |
                 ((uint32_t)cpu_machine->video_frame[
                      GBL_PPU_ROW_BYTES + 1u] << 8u);
    CHECK(run_status == GBL_RUN_STATUS_OK);
    CHECK(run.termination == GBL_RUN_PROGRESS);
    CHECK(run.reason == GBL_RUN_REASON_UNIT_LIMIT);
    CHECK(run.instructions == 1u);
    CHECK(cpu_machine->master_cycles > UINT64_C(1232));
    CHECK(cpu_machine->ppu.line == 1u);
    CHECK(cpu_machine->ppu.line_origin == UINT64_C(1232));
    CHECK(row0_pixel == UINT32_C(0x4567));

    printf("{\"type\":\"ppu.tracer\",\"line\":1,\"boundary_cycle\":1232,\"hblank_offset\":1006,\"hblank_provenance\":\"project-selected/REF\",\"hblank_dispute\":\"disp-hblank-flag-edge\",\"hardware_qualification\":\"UNKNOWN\",\"semantic_pixel\":4660,\"cpu_cycle\":%llu}\n",
           (unsigned long long)cpu_machine->master_cycles);

    free_machine(machine);
    free_machine(cpu_machine);
    free(refusal_snapshot);
    free_machine(refusal_machine);
    if (CHECK_DONE() == 0) {
        puts("ppu.tracer: timed bus and CPU paths captured mode-3 rows");
        return 0;
    }
    return 1;
}

static int run_ppu_bg_tracer(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *before = malloc(sizeof(*before));
    gbl_bus_result result;
    gbl_machine_time_preview preview;
    gbl_machine_time_preview changed_preview;
    const gbl_time_operation same_cycle_tile_write = {
        GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x06004004),
        UINT32_C(0x0043), UINT64_C(1)
    };
    static const uint8_t tile_row[4] = {
        UINT8_C(0x21), UINT8_C(0x03), UINT8_C(0x54), UINT8_C(0x06)
    };
    uint32_t row;
    uint32_t byte;
    const uint32_t row_start = GBL_PPU_ROW_BYTES;
    uint16_t actual[4];

    if (machine == NULL || before == NULL) {
        fprintf(stderr, "ppu.bg.tracer: host allocation failed\n");
        free(before);
        free_machine(machine);
        return 1;
    }

    /* Begin with BG0 enabled before the cycle-zero sample. The empty map and
     * tile are transparent, so row zero resolves to palette entry zero. */
    machine->dispcnt = UINT16_C(0x0100);
    machine->ppu.dispcnt = UINT16_C(0x0100);
    machine->pram[0] = UINT8_C(0x1F);
    machine->pram[1] = UINT8_C(0x00);
    machine->pram[2] = UINT8_C(0xE0);
    machine->pram[3] = UINT8_C(0x03);
    machine->pram[4] = UINT8_C(0x00);
    machine->pram[5] = UINT8_C(0x7C);
    machine->pram[6] = UINT8_C(0x10);
    machine->pram[7] = UINT8_C(0x42);
    machine->vram[UINT32_C(0xF800)] = 0u;
    machine->vram[UINT32_C(0xF801)] = 0u;
    for (row = 0u; row < 8u; ++row) {
        for (byte = 0u; byte < sizeof(tile_row); ++byte) {
            machine->vram[UINT32_C(0x4000) + row * 4u + byte] =
                tile_row[byte];
        }
    }

    /* Program BG0's map and character bases through the timed register path.
     * A preceding palette write must be visible at the next line boundary. */
    result = gbl_bus_write(machine, UINT32_C(0x04000008), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x1F04));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(gbl_machine_advance_to(machine, UINT64_C(1000)) ==
          GBL_TIME_STATUS_OK);
    result = gbl_bus_write(machine, UINT32_C(0x05000004), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0x7FFF));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(gbl_machine_advance_to(machine, UINT64_C(1231)) ==
          GBL_TIME_STATUS_OK);

    /* The PPU boundary has priority over a write completing on that cycle.
     * The captured row retains the original tile indices despite this write. */
    CHECK(gbl_machine_prepare_operations(
              machine, &same_cycle_tile_write, 1u, &preview) ==
          GBL_TIME_STATUS_OK);
    CHECK(preview.ppu_row_capture_count == 1u);
    CHECK(preview.ppu_row_captures[0].line == 1u);
    CHECK(preview.ppu_row_captures[0].bg_register_bytes[0] ==
          UINT8_C(0x04));
    CHECK(preview.ppu_row_captures[0].bg_register_bytes[1] ==
          UINT8_C(0x1F));
    changed_preview = preview;
    changed_preview.ppu_row_captures[0].bg_register_bytes[0] ^=
        UINT8_C(0x04);
    *before = *machine;
    gbl_machine_commit_time(machine, &changed_preview);
    CHECK(gbl_test_canonical_equal(machine, before));
    gbl_machine_commit_time(machine, &preview);
    CHECK(machine->master_cycles == UINT64_C(1232));
    CHECK(machine->ppu.line == 1u);
    CHECK(machine->ppu.line_origin == UINT64_C(1232));

    /* BGR555_LE semantic expectations for indices 1, 2, 3 and transparent 0.
     * Index 2 is the new, pre-boundary palette value; index 0 is the backdrop. */
    for (byte = 0u; byte < 4u; ++byte) {
        const uint32_t offset = row_start + byte * 2u;

        actual[byte] = (uint16_t)(
            (uint16_t)machine->video_frame[offset] |
            (uint16_t)((uint16_t)machine->video_frame[offset + 1u] << 8u));
    }
    CHECK(machine->video_frame[row_start] == UINT8_C(0xE0));
    CHECK(machine->video_frame[row_start + 1u] == UINT8_C(0x03));
    CHECK(machine->video_frame[row_start + 2u] == UINT8_C(0xFF));
    CHECK(machine->video_frame[row_start + 3u] == UINT8_C(0x7F));
    CHECK(machine->video_frame[row_start + 4u] == UINT8_C(0x10));
    CHECK(machine->video_frame[row_start + 5u] == UINT8_C(0x42));
    CHECK(machine->video_frame[row_start + 6u] == UINT8_C(0x1F));
    CHECK(machine->video_frame[row_start + 7u] == UINT8_C(0x00));

    printf("{\"type\":\"ppu.bg.tracer\",\"mode\":0,\"bg\":0,\"row\":1,\"expected\":[992,32767,16912,31],\"actual\":[%u,%u,%u,%u],\"before_boundary_palette_write\":\"visible\",\"same_cycle_tile_write\":\"excluded\",\"format\":\"BGR555_LE\",\"hardware_qualification\":\"UNKNOWN\"}\n",
           (unsigned)actual[0], (unsigned)actual[1], (unsigned)actual[2],
           (unsigned)actual[3]);
    free(before);
    free_machine(machine);
    if (CHECK_DONE() == 0) {
        puts("ppu.bg.tracer: mode-0 BG0 row reaches committed raw video");
        return 0;
    }
    return 1;
}

static void ppu_bg_store_u16(uint8_t *bytes, uint32_t offset,
                             uint16_t value)
{
    bytes[offset] = (uint8_t)(value & UINT16_C(0x00FF));
    bytes[offset + 1u] = (uint8_t)(value >> 8u);
}

static uint16_t ppu_bg_text_control(const gbl_machine *machine,
                                    uint8_t background)
{
    const uint32_t offset = (uint32_t)background * 2u;

    return (uint16_t)(
        (uint16_t)machine->ppu.bg_register_bytes[offset] |
        (uint16_t)((uint16_t)machine->ppu.bg_register_bytes[offset + 1u]
                   << 8u));
}

static bool ppu_bg_text_configure(gbl_machine *machine, uint8_t mode,
                                  uint8_t background, uint8_t size_code,
                                  bool color_8bpp, uint16_t horizontal_scroll,
                                  uint16_t vertical_scroll,
                                  uint8_t map_base, uint8_t character_base)
{
    uint16_t dispcnt;
    uint16_t control;
    const uint32_t register_offset = (uint32_t)background * 2u;
    const uint32_t scroll_offset = UINT32_C(8) +
                                   (uint32_t)background * 4u;

    if (machine == NULL || background >= 4u || mode > 1u || size_code > 3u ||
        map_base > 31u || character_base > 3u) {
        return false;
    }
    dispcnt = (uint16_t)(mode |
                         (uint16_t)(UINT16_C(1) << (8u + background)));
    control = (uint16_t)(
        (uint16_t)((uint16_t)character_base << 2u) |
        (color_8bpp ? UINT16_C(0x0080) : 0u) |
        (uint16_t)((uint16_t)map_base << 8u) |
        (uint16_t)((uint16_t)size_code << 14u));
    machine->dispcnt = dispcnt;
    machine->ppu.dispcnt = dispcnt;
    ppu_bg_store_u16(machine->ppu.bg_register_bytes, register_offset,
                     control);
    ppu_bg_store_u16(machine->ppu.bg_register_bytes, scroll_offset,
                     horizontal_scroll);
    ppu_bg_store_u16(machine->ppu.bg_register_bytes, scroll_offset + 2u,
                     vertical_scroll);
    return true;
}

static bool ppu_bg_text_map_offset(uint16_t control, uint32_t tile_x,
                                   uint32_t tile_y, uint32_t *offset_out)
{
    const uint32_t size_code = (uint32_t)(control >> 14u);
    const uint32_t width_tiles = (size_code & 1u) != 0u ? 64u : 32u;
    const uint32_t height_tiles = (size_code & 2u) != 0u ? 64u : 32u;
    const uint32_t blocks_per_row = width_tiles / 32u;
    const uint32_t map_base =
        (uint32_t)((control >> 8u) & UINT16_C(0x001F)) * UINT32_C(0x0800);
    const uint64_t offset = (uint64_t)map_base +
        ((uint64_t)(tile_y / 32u) * blocks_per_row + tile_x / 32u) *
            UINT64_C(0x0800) +
        ((uint64_t)(tile_y & 31u) * UINT64_C(32) + (tile_x & 31u)) *
            UINT64_C(2);

    if (offset_out == NULL || tile_x >= width_tiles ||
        tile_y >= height_tiles || offset > UINT32_MAX ||
        offset > UINT64_C(0x10000) || UINT64_C(2) >
            UINT64_C(0x10000) - offset) {
        return false;
    }
    *offset_out = (uint32_t)offset;
    return true;
}

static bool ppu_bg_text_set_map_entry(gbl_machine *machine,
                                      uint8_t background, uint32_t tile_x,
                                      uint32_t tile_y, uint16_t entry)
{
    uint32_t offset;

    if (machine == NULL || background >= 4u ||
        !ppu_bg_text_map_offset(ppu_bg_text_control(machine, background),
                                tile_x, tile_y, &offset)) {
        return false;
    }
    ppu_bg_store_u16(machine->vram, offset, entry);
    return true;
}

static bool ppu_bg_text_fill_tile(gbl_machine *machine, uint8_t background,
                                  uint16_t tile_index,
                                  const uint8_t pixels[64])
{
    uint16_t control;
    bool color_8bpp;
    uint32_t character_base;
    uint32_t bytes_per_tile;
    uint64_t offset;
    uint32_t y;

    if (machine == NULL || background >= 4u || pixels == NULL) {
        return false;
    }
    control = ppu_bg_text_control(machine, background);
    color_8bpp = (control & UINT16_C(0x0080)) != 0u;
    character_base =
        (uint32_t)((control >> 2u) & UINT16_C(0x0003)) * UINT32_C(0x4000);
    bytes_per_tile = color_8bpp ? 64u : 32u;
    offset = (uint64_t)character_base +
             (uint64_t)tile_index * bytes_per_tile;
    if (offset > UINT64_C(0x10000) || bytes_per_tile >
        UINT64_C(0x10000) - offset) {
        return false;
    }
    for (y = 0u; y < 8u; ++y) {
        uint32_t x;

        if (color_8bpp) {
            for (x = 0u; x < 8u; ++x) {
                machine->vram[(uint32_t)offset + y * 8u + x] =
                    pixels[y * 8u + x];
            }
        } else {
            for (x = 0u; x < 8u; x += 2u) {
                const uint8_t packed = (uint8_t)(
                    (pixels[y * 8u + x] & UINT8_C(0x0F)) |
                    (uint8_t)((pixels[y * 8u + x + 1u] & UINT8_C(0x0F))
                              << 4u));

                machine->vram[(uint32_t)offset + y * 4u + x / 2u] = packed;
            }
        }
    }
    return true;
}

static bool ppu_bg_text_set_color(gbl_machine *machine, uint32_t index,
                                  uint16_t color)
{
    if (machine == NULL || index >= 256u) {
        return false;
    }
    ppu_bg_store_u16(machine->pram, index * 2u,
                     (uint16_t)(color & UINT16_C(0x7FFF)));
    return true;
}

static bool ppu_bg_text_pixel_matches(const uint8_t *frame, uint16_t x,
                                      uint16_t expected)
{
    const size_t offset = (size_t)GBL_PPU_ROW_BYTES + (size_t)x * 2u;
    const uint16_t actual = (uint16_t)(
        (uint16_t)frame[offset] |
        (uint16_t)((uint16_t)frame[offset + 1u] << 8u));

    return actual == expected;
}

static bool ppu_bg_affine_configure(gbl_machine *machine, uint8_t mode,
                                    uint8_t background, uint8_t size_code,
                                    bool wrap, uint8_t map_base,
                                    uint8_t character_base)
{
    uint16_t dispcnt;
    uint16_t control;

    if (machine == NULL || background < 2u || background > 3u ||
        mode < 1u || mode > 2u || size_code > 3u || map_base > 31u ||
        character_base > 3u || (mode == 1u && background != 2u)) {
        return false;
    }
    dispcnt = (uint16_t)(mode |
                         (uint16_t)(UINT16_C(1) << (8u + background)));
    control = (uint16_t)((uint16_t)((uint16_t)character_base << 2u) |
                         (uint16_t)((uint16_t)map_base << 8u) |
                         (wrap ? UINT16_C(0x2000) : 0u) |
                         (uint16_t)((uint16_t)size_code << 14u));
    machine->dispcnt = dispcnt;
    machine->ppu.dispcnt = dispcnt;
    ppu_bg_store_u16(machine->ppu.bg_register_bytes,
                     (uint32_t)background * 2u, control);
    return true;
}

static bool ppu_bg_affine_set_map_entry(gbl_machine *machine,
                                        uint8_t background,
                                        uint32_t source_x,
                                        uint32_t source_y,
                                        uint8_t tile_index)
{
    const uint16_t control = ppu_bg_text_control(machine, background);
    const uint32_t size_code = (uint32_t)(control >> 14u);
    const uint32_t map_width = UINT32_C(128) << size_code;
    const uint32_t map_width_tiles = map_width / UINT32_C(8);
    const uint32_t map_base =
        (uint32_t)((control >> 8u) & UINT16_C(0x001F)) * UINT32_C(0x0800);
    const uint64_t map_offset = (uint64_t)map_base +
        (uint64_t)(source_y >> 3u) * (uint64_t)map_width_tiles +
        (uint64_t)(source_x >> 3u);

    if (machine == NULL || background < 2u || background > 3u ||
        source_x >= map_width || source_y >= map_width ||
        map_offset >= (uint64_t)GBL_VRAM_SIZE || map_offset >= UINT32_MAX) {
        return false;
    }
    machine->vram[(uint32_t)map_offset] = tile_index;
    return true;
}

static bool ppu_bg_affine_fill_tile(gbl_machine *machine,
                                    uint8_t background,
                                    uint8_t tile_index,
                                    const uint8_t pixels[64])
{
    const uint16_t control = ppu_bg_text_control(machine, background);
    const uint32_t character_base =
        (uint32_t)((control >> 2u) & UINT16_C(0x0003)) * UINT32_C(0x4000);
    const uint64_t offset = (uint64_t)character_base +
                            (uint64_t)tile_index * UINT64_C(64);

    if (machine == NULL || background < 2u || background > 3u ||
        pixels == NULL || offset > (uint64_t)GBL_VRAM_SIZE ||
        sizeof(uint8_t) * 64u > (size_t)((uint64_t)GBL_VRAM_SIZE - offset)) {
        return false;
    }
    memcpy(&machine->vram[(uint32_t)offset], pixels, 64u);
    return true;
}

static void ppu_bg_affine_set_transform(gbl_machine *machine,
                                        uint8_t background, int16_t pa,
                                        int16_t pb, int16_t pc, int16_t pd,
                                        int32_t reference_x,
                                        int32_t reference_y)
{
    gbl_ppu_affine_state *affine = &machine->ppu.affine[background - 2u];

    affine->pa = pa;
    affine->pb = pb;
    affine->pc = pc;
    affine->pd = pd;
    affine->programmed_reference_x =
        (uint32_t)reference_x & UINT32_C(0x0FFFFFFF);
    affine->programmed_reference_y =
        (uint32_t)reference_y & UINT32_C(0x0FFFFFFF);
    affine->origin_x = affine->programmed_reference_x;
    affine->origin_y = affine->programmed_reference_y;
    affine->origin_valid = true;
    affine->hblank_y_suppressed = false;
}

static bool ppu_bg_bitmap_configure(gbl_machine *machine, uint8_t mode,
                                    bool page)
{
    uint16_t dispcnt;

    if (machine == NULL || mode < 3u || mode > 5u) {
        return false;
    }
    dispcnt = (uint16_t)(mode | UINT16_C(0x0400) |
                         (page ? UINT16_C(0x0010) : 0u));
    machine->dispcnt = dispcnt;
    machine->ppu.dispcnt = dispcnt;
    return true;
}

static bool ppu_bg_affine_pixel_matches(const uint8_t *frame, uint16_t x,
                                        uint16_t y, uint16_t expected)
{
    const size_t offset = (size_t)y * GBL_PPU_ROW_BYTES + (size_t)x * 2u;
    const uint16_t actual = (uint16_t)(
        (uint16_t)frame[offset] |
        (uint16_t)((uint16_t)frame[offset + 1u] << 8u));

    return actual == expected;
}

static void ppu_obj_store_u16(uint8_t *bytes, size_t offset, uint16_t value)
{
    bytes[offset] = (uint8_t)(value & UINT16_C(0x00FF));
    bytes[offset + 1u] = (uint8_t)(value >> 8u);
}

static bool ppu_composition_prepare_text_scene(
    gbl_machine *machine, uint8_t bg0_priority, uint8_t bg1_priority,
    uint8_t obj_priority, uint8_t obj_color_index, bool enable_backgrounds,
    bool enable_obj)
{
    uint8_t bg0_pixels[64];
    uint8_t bg1_pixels[64];
    size_t object;
    uint16_t dispcnt;

    if (machine == NULL || bg0_priority > 3u || bg1_priority > 3u ||
        obj_priority > 3u || obj_color_index > 15u ||
        !ppu_bg_text_configure(machine, 0u, 0u, 0u, false, 0u, 0u, 1u, 0u) ||
        !ppu_bg_text_configure(machine, 0u, 1u, 0u, true, 0u, 0u, 2u, 1u)) {
        return false;
    }

    memset(bg0_pixels, 1, sizeof(bg0_pixels));
    memset(bg1_pixels, 2, sizeof(bg1_pixels));
    if (!ppu_bg_text_set_map_entry(machine, 0u, 0u, 1u, 1u) ||
        !ppu_bg_text_fill_tile(machine, 0u, 1u, bg0_pixels) ||
        !ppu_bg_text_set_map_entry(machine, 1u, 0u, 1u, 1u) ||
        !ppu_bg_text_fill_tile(machine, 1u, 1u, bg1_pixels) ||
        !ppu_bg_text_set_color(machine, 1u, UINT16_C(0x001F)) ||
        !ppu_bg_text_set_color(machine, 2u, UINT16_C(0x03E0))) {
        return false;
    }

    ppu_bg_store_u16(machine->ppu.bg_register_bytes, 0u,
                     (uint16_t)((ppu_bg_text_control(machine, 0u) &
                                 UINT16_C(0xFFFC)) | bg0_priority));
    ppu_bg_store_u16(machine->ppu.bg_register_bytes, 2u,
                     (uint16_t)((ppu_bg_text_control(machine, 1u) &
                                 UINT16_C(0xFFFC)) | bg1_priority));

    dispcnt = (uint16_t)((enable_backgrounds ? UINT16_C(0x0300) : 0u) |
                         (enable_obj ? UINT16_C(0x1000) : 0u));
    machine->dispcnt = dispcnt;
    machine->ppu.dispcnt = dispcnt;
    ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x4210));
    ppu_bg_store_u16(machine->pram, UINT32_C(0x202), UINT16_C(0x7C00));
    for (object = 0u; object < 128u; ++object) {
        ppu_obj_store_u16(machine->oam, object * 8u, UINT16_C(0x0200));
    }
    ppu_obj_store_u16(machine->oam, 0u, UINT16_C(8));
    ppu_obj_store_u16(machine->oam, 2u, 0u);
    ppu_obj_store_u16(machine->oam, 4u,
                      (uint16_t)(UINT16_C(1) |
                                 (uint16_t)((uint16_t)obj_priority << 10u)));
    memset(&machine->vram[UINT32_C(0x10020)],
           (uint8_t)(obj_color_index | (uint8_t)(obj_color_index << 4u)),
           32u);
    return true;
}

static bool ppu_composition_prepare_text_bg(
    gbl_machine *machine, uint8_t mode, uint8_t background,
    uint8_t priority, uint16_t color)
{
    uint8_t pixels[64];
    const uint8_t map_base = (uint8_t)(background + 1u);

    memset(pixels, 1, sizeof(pixels));
    if (!ppu_bg_text_configure(machine, mode, background, 0u, false,
                               0u, 0u, map_base, 0u) ||
        !ppu_bg_text_set_map_entry(machine, background, 0u, 0u, 1u) ||
        !ppu_bg_text_fill_tile(machine, background, 1u, pixels) ||
        !ppu_bg_text_set_color(machine, 1u, color)) {
        return false;
    }
    ppu_bg_store_u16(
        machine->ppu.bg_register_bytes, (uint32_t)background * 2u,
        (uint16_t)((ppu_bg_text_control(machine, background) &
                    UINT16_C(0xFFFC)) | priority));
    return true;
}

static bool ppu_composition_prepare_affine_bg(
    gbl_machine *machine, uint8_t mode, uint8_t background,
    uint8_t priority, uint16_t color)
{
    uint8_t pixels[64];
    const uint8_t map_base = (uint8_t)(background - 1u);
    const uint8_t character_base = (uint8_t)(background - 1u);

    memset(pixels, 1, sizeof(pixels));
    if (!ppu_bg_affine_configure(machine, mode, background, 0u, false,
                                 map_base, character_base) ||
        !ppu_bg_affine_set_map_entry(machine, background, 0u, 0u, 1u) ||
        !ppu_bg_affine_fill_tile(machine, background, 1u, pixels) ||
        !ppu_bg_text_set_color(machine, 1u, color)) {
        return false;
    }
    ppu_bg_affine_set_transform(machine, background, INT16_C(0x0100), 0,
                                0, INT16_C(0x0100), 0, 0);
    ppu_bg_store_u16(
        machine->ppu.bg_register_bytes, (uint32_t)background * 2u,
        (uint16_t)((ppu_bg_text_control(machine, background) &
                    UINT16_C(0xFFFC)) | priority));
    return true;
}

static bool ppu_composition_prepare_bitmap_bg(
    gbl_machine *machine, uint8_t mode, uint8_t priority,
    uint16_t color)
{
    if (!ppu_bg_bitmap_configure(machine, mode, false)) {
        return false;
    }
    ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                0, INT16_C(0x0100), 0, 0);
    ppu_bg_store_u16(machine->ppu.bg_register_bytes, 4u, priority);
    if (mode == 4u) {
        machine->vram[0] = 1u;
        return ppu_bg_text_set_color(machine, 1u, color);
    }
    ppu_bg_store_u16(machine->vram, 0u, color);
    return true;
}

static bool ppu_composition_prepare_obj_pair(
    gbl_machine *machine, uint8_t bg0_priority, uint8_t bg1_priority,
    uint8_t obj0_priority, uint8_t obj1_priority, uint8_t enabled_bg_mask,
    bool swap_oam_order)
{
    const uint16_t dispcnt = (uint16_t)(UINT16_C(0x1000) |
        (uint16_t)((uint16_t)enabled_bg_mask << 8u));
    const uint8_t obj0_tile = swap_oam_order ? 2u : 1u;
    const uint8_t obj1_tile = swap_oam_order ? 1u : 2u;
    uint16_t attribute2;

    if (!ppu_composition_prepare_text_scene(
            machine, bg0_priority, bg1_priority, obj0_priority, 1u,
            true, true)) {
        return false;
    }
    machine->dispcnt = dispcnt;
    machine->ppu.dispcnt = dispcnt;
    ppu_bg_store_u16(machine->pram, UINT32_C(0x202), UINT16_C(0x001F));
    ppu_bg_store_u16(machine->pram, UINT32_C(0x204), UINT16_C(0x7C00));
    memset(&machine->vram[UINT32_C(0x10020)], UINT8_C(0x11), 32u);
    memset(&machine->vram[UINT32_C(0x10040)], UINT8_C(0x22), 32u);
    ppu_obj_store_u16(machine->oam, 0u, UINT16_C(8));
    ppu_obj_store_u16(machine->oam, 2u, 0u);
    attribute2 = (uint16_t)((uint16_t)obj0_tile |
                            (uint16_t)((uint16_t)obj0_priority << 10u));
    ppu_obj_store_u16(machine->oam, 4u, attribute2);
    ppu_obj_store_u16(machine->oam, 8u, UINT16_C(8));
    ppu_obj_store_u16(machine->oam, 10u, 0u);
    attribute2 = (uint16_t)((uint16_t)obj1_tile |
                            (uint16_t)((uint16_t)obj1_priority << 10u));
    ppu_obj_store_u16(machine->oam, 12u, attribute2);
    return true;
}

static int run_ppu_composition(void)
{
    static const struct {
        uint8_t bg0_priority;
        uint8_t bg1_priority;
        uint8_t obj_priority;
        uint8_t obj_color_index;
        bool enable_backgrounds;
        bool enable_obj;
        uint16_t expected;
    } scenes[] = {
        { 2u, 1u, 3u, 1u, true, true, UINT16_C(0x03E0) },
        { 1u, 1u, 3u, 1u, true, true, UINT16_C(0x001F) },
        { 2u, 1u, 1u, 1u, true, true, UINT16_C(0x7C00) },
        { 2u, 1u, 1u, 0u, true, true, UINT16_C(0x03E0) },
        { 2u, 1u, 3u, 0u, false, false, UINT16_C(0x4210) },
        { 2u, 1u, 1u, 0u, false, true, UINT16_C(0x4210) }
    };
    static const struct {
        uint8_t mode;
        uint8_t background;
        uint16_t expected;
    } mode_sources[] = {
        { 0u, 0u, UINT16_C(0x0111) }, { 0u, 1u, UINT16_C(0x0112) },
        { 0u, 2u, UINT16_C(0x0113) }, { 0u, 3u, UINT16_C(0x0114) },
        { 1u, 0u, UINT16_C(0x0121) }, { 1u, 1u, UINT16_C(0x0122) },
        { 1u, 2u, UINT16_C(0x0123) },
        { 2u, 2u, UINT16_C(0x0131) }, { 2u, 3u, UINT16_C(0x0132) },
        { 3u, 2u, UINT16_C(0x0141) }, { 4u, 2u, UINT16_C(0x0142) },
        { 5u, 2u, UINT16_C(0x0143) }
    };
    static const struct {
        uint8_t obj0_priority;
        uint8_t obj1_priority;
        uint8_t enabled_bg_mask;
        bool swap_oam_order;
        uint16_t expected;
        uint16_t wrong_winner;
    } object_orders[] = {
        { 2u, 0u, 2u, false, UINT16_C(0x7C00), UINT16_C(0x001F) },
        { 0u, 2u, 2u, true, UINT16_C(0x7C00), UINT16_C(0x001F) },
        { 0u, 0u, 0u, false, UINT16_C(0x001F), UINT16_C(0x7C00) },
        { 0u, 0u, 0u, true, UINT16_C(0x7C00), UINT16_C(0x001F) }
    };
    size_t scene;
    size_t source_case;
    size_t object_order;
    uint8_t bg_priority;
    uint8_t obj_priority;

    for (scene = 0u; scene < sizeof(scenes) / sizeof(scenes[0]); ++scene) {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        CHECK(ppu_composition_prepare_text_scene(
            machine, scenes[scene].bg0_priority, scenes[scene].bg1_priority,
            scenes[scene].obj_priority, scenes[scene].obj_color_index,
            scenes[scene].enable_backgrounds, scenes[scene].enable_obj));
        CHECK(gbl_machine_advance_to(
                  machine, (uint64_t)GBL_PPU_LINE_CYCLES * UINT64_C(160)) ==
              GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                          scenes[scene].expected));
        free_machine(machine);
    }

    for (bg_priority = 0u; bg_priority < 4u; ++bg_priority) {
        for (obj_priority = 0u; obj_priority < 4u; ++obj_priority) {
            const uint16_t expected = obj_priority <= bg_priority
                ? UINT16_C(0x7C00)
                : UINT16_C(0x001F);
            gbl_machine *machine = new_machine();

            CHECK(machine != NULL);
            if (machine == NULL) {
                continue;
            }
            CHECK(ppu_composition_prepare_text_scene(
                machine, bg_priority, bg_priority, obj_priority, 1u,
                true, true));
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                              expected));
            CHECK(!ppu_bg_affine_pixel_matches(
                machine->video_frame, 0u, 8u,
                (uint16_t)(expected ^ UINT16_C(1))));
            free_machine(machine);
        }
    }

    for (source_case = 0u;
         source_case < sizeof(mode_sources) / sizeof(mode_sources[0]);
         ++source_case) {
        const uint8_t mode = mode_sources[source_case].mode;
        const uint8_t background = mode_sources[source_case].background;
        const uint16_t expected = mode_sources[source_case].expected;
        const uint8_t priority = (uint8_t)((mode + background) & 3u);
        gbl_machine *machine = new_machine();
        bool prepared = false;

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        if (mode == 0u || (mode == 1u && background < 2u)) {
            prepared = ppu_composition_prepare_text_bg(
                machine, mode, background, priority, expected);
        } else if ((mode == 1u && background == 2u) || mode == 2u) {
            prepared = ppu_composition_prepare_affine_bg(
                machine, mode, background, priority, expected);
        } else {
            prepared = ppu_composition_prepare_bitmap_bg(
                machine, mode, priority, expected);
        }
        CHECK(prepared);
        CHECK(gbl_machine_advance_to(
                  machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                               UINT64_C(160)) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          expected));
        free_machine(machine);
    }

    for (object_order = 0u;
         object_order < sizeof(object_orders) / sizeof(object_orders[0]);
         ++object_order) {
        gbl_machine *machine = new_machine();
        const uint16_t bg0_priority = object_order < 2u ? 2u : 3u;
        const uint16_t bg1_priority = object_order < 2u ? 1u : 3u;

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        CHECK(ppu_composition_prepare_obj_pair(
            machine, (uint8_t)bg0_priority, (uint8_t)bg1_priority,
            object_orders[object_order].obj0_priority,
            object_orders[object_order].obj1_priority,
            object_orders[object_order].enabled_bg_mask,
            object_orders[object_order].swap_oam_order));
        CHECK(gbl_machine_advance_to(
                  machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                               UINT64_C(160)) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        CHECK(ppu_bg_affine_pixel_matches(
            machine->video_frame, 0u, 8u,
            object_orders[object_order].expected));
        CHECK(!ppu_bg_affine_pixel_matches(
            machine->video_frame, 0u, 8u,
            object_orders[object_order].wrong_winner));
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_text_scene(
                machine, 0u, 1u, 3u, 1u, true, false));
            machine->dispcnt = UINT16_C(0x0100);
            machine->ppu.dispcnt = machine->dispcnt;
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                              UINT16_C(0x001F)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        uint8_t pixels[64];

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_text_bg(
                machine, 1u, 0u, 2u, UINT16_C(0x001F)));
            CHECK(ppu_bg_text_set_map_entry(machine, 0u, 0u, 0u,
                                            UINT16_C(0x1001)));
            CHECK(ppu_composition_prepare_affine_bg(
                machine, 1u, 2u, 1u, UINT16_C(0x03E0)));
            CHECK(ppu_bg_text_set_color(machine, 17u, UINT16_C(0x001F)));
            memset(pixels, 0, sizeof(pixels));
            CHECK(ppu_bg_affine_fill_tile(machine, 2u, 1u, pixels));
            machine->dispcnt |= UINT16_C(0x0100);
            machine->ppu.dispcnt = machine->dispcnt;
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x001F)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_text_bg(
                machine, 1u, 0u, 2u, UINT16_C(0x001F)));
            CHECK(ppu_bg_text_set_map_entry(machine, 0u, 0u, 0u,
                                            UINT16_C(0x1001)));
            CHECK(ppu_composition_prepare_affine_bg(
                machine, 1u, 2u, 1u, UINT16_C(0x03E0)));
            CHECK(ppu_bg_text_set_color(machine, 17u, UINT16_C(0x001F)));
            machine->dispcnt |= UINT16_C(0x0100);
            machine->ppu.dispcnt = machine->dispcnt;
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x03E0)));
            CHECK(!ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                               UINT16_C(0x001F)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_text_scene(
                machine, 0u, 1u, 3u, 0u, true, false));
            memset(&machine->vram[32u], 0, 32u);
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                              UINT16_C(0x03E0)));
            CHECK(!ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                               UINT16_C(0x001F)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_obj_pair(machine, 3u, 3u,
                                                   0u, 0u, 0u, false));
            memset(&machine->vram[UINT32_C(0x10020)], 0, 32u);
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                              UINT16_C(0x7C00)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_text_scene(
                machine, 0u, 1u, 0u, 1u, true, true));
            ppu_bg_store_u16(machine->pram, UINT32_C(0x202), 0u);
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                              0u));
            CHECK(!ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                               UINT16_C(0x7C00)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_text_scene(
                machine, 0u, 1u, 3u, 1u, true, false));
            CHECK(ppu_bg_text_set_color(machine, 1u, 0u));
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                              0u));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_text_bg(
                machine, 1u, 0u, 0u, UINT16_C(0x001F)));
            machine->dispcnt |= UINT16_C(0x0800);
            machine->ppu.dispcnt = machine->dispcnt;
            ppu_bg_store_u16(machine->ppu.bg_register_bytes, 6u,
                             UINT16_C(0x0010));
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x001F)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_bitmap_bg(
                machine, 3u, 0u, 0u));
            ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x03E0));
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              0u));
            free_machine(machine);
        }
    }

    for (scene = 3u; scene <= 5u; ++scene) {
        gbl_machine *machine = new_machine();
        const uint16_t dispcnt = (uint16_t)(
            (uint16_t)scene | UINT16_C(0x0B00));

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        machine->dispcnt = dispcnt;
        machine->ppu.dispcnt = dispcnt;
        ppu_bg_store_u16(machine->ppu.bg_register_bytes, 0u,
                         UINT16_C(0xFFFF));
        ppu_bg_store_u16(machine->ppu.bg_register_bytes, 2u,
                         UINT16_C(0xFFFF));
        ppu_bg_store_u16(machine->ppu.bg_register_bytes, 4u,
                         UINT16_C(0x0040));
        ppu_bg_store_u16(machine->ppu.bg_register_bytes, 6u,
                         UINT16_C(0xFFFF));
        memset(machine->vram, UINT8_C(0xFF), UINT32_C(0x10000));
        ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x4210));
        CHECK(gbl_machine_advance_to(
                  machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                               UINT64_C(160)) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          UINT16_C(0x4210)));
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_affine_bg(
                machine, 2u, 2u, 2u, UINT16_C(0x001F)));
            CHECK(ppu_composition_prepare_affine_bg(
                machine, 2u, 3u, 1u, UINT16_C(0x03E0)));
            {
                uint8_t bg2_pixels[64];

                memset(bg2_pixels, 2, sizeof(bg2_pixels));
                CHECK(ppu_bg_affine_fill_tile(machine, 2u, 1u,
                                              bg2_pixels));
                CHECK(ppu_bg_text_set_color(machine, 2u,
                                            UINT16_C(0x001F)));
            }
            machine->dispcnt |= UINT16_C(0x0400);
            machine->ppu.dispcnt = machine->dispcnt;
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x03E0)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_affine_bg(
                machine, 2u, 2u, 1u, UINT16_C(0x001F)));
            CHECK(ppu_composition_prepare_affine_bg(
                machine, 2u, 3u, 1u, UINT16_C(0x03E0)));
            {
                uint8_t bg2_pixels[64];

                memset(bg2_pixels, 2, sizeof(bg2_pixels));
                CHECK(ppu_bg_affine_fill_tile(machine, 2u, 1u,
                                              bg2_pixels));
                CHECK(ppu_bg_text_set_color(machine, 2u,
                                            UINT16_C(0x001F)));
            }
            machine->dispcnt |= UINT16_C(0x0400);
            machine->ppu.dispcnt = machine->dispcnt;
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x001F)));
            CHECK(!ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                               UINT16_C(0x03E0)));
            free_machine(machine);
        }
    }

    if (CHECK_DONE() == 0) {
        puts("ppu.composition: all mode sources, priority matrix, OAM-order policy, transparency, black and backdrop");
        puts("{\"type\":\"ppu.composition\",\"policy\":\"priority first; equal OBJ/BG favors OBJ; lower BG/OAM index ties\",\"hardware_qualification\":\"UNKNOWN\"}");
        return 0;
    }
    return 1;
}

static void ppu_obj_affine_store_matrix(uint8_t *oam, uint8_t matrix,
                                        int16_t pa, int16_t pb,
                                        int16_t pc, int16_t pd)
{
    const size_t offset = (size_t)matrix * 32u;
    const uint16_t coefficients[4] = {
        (uint16_t)pa, (uint16_t)pb, (uint16_t)pc, (uint16_t)pd
    };
    size_t coefficient;

    for (coefficient = 0u; coefficient < 4u; ++coefficient) {
        ppu_obj_store_u16(oam, offset + 6u + coefficient * 8u,
                          coefficients[coefficient]);
    }
}

static bool ppu_obj_affine_prepare_basic(gbl_machine *machine,
                                         bool double_size)
{
    size_t object;

    if (machine == NULL) {
        return false;
    }
    machine->dispcnt = UINT16_C(0x1000);
    machine->ppu.dispcnt = machine->dispcnt;
    for (object = 0u; object < 128u; ++object) {
        ppu_obj_store_u16(machine->oam, object * 8u, UINT16_C(0x0200));
    }
    machine->pram[0] = 0u;
    machine->pram[1] = UINT8_C(0x7C); /* Blue backdrop. */
    machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F); /* Red OBJ index 1. */
    machine->pram[UINT32_C(0x203)] = 0u;
    memset(&machine->vram[UINT32_C(0x10000)], 0, 128u);
    machine->vram[UINT32_C(0x10000)] = 1u;

    /* Matrix 0 is a decoy. The object must select matrix 3 from this OAM
     * view; the matrix words occupy the attribute-3 slots of OAM entries
     * 12 through 15. */
    ppu_obj_affine_store_matrix(machine->oam, 0u, 0, 0, 0, 0);
    ppu_obj_affine_store_matrix(machine->oam, 3u, 256, 0, 0, 256);
    ppu_obj_store_u16(machine->oam, 0u,
                      (uint16_t)(UINT16_C(24) | UINT16_C(0x0100) |
                          (double_size ? UINT16_C(0x0200) : 0u)));
    ppu_obj_store_u16(machine->oam, 2u,
                      (uint16_t)(UINT16_C(0x4000) | UINT16_C(0x0600) |
                          UINT16_C(24)));
    ppu_obj_store_u16(machine->oam, 4u, 0u);
    return true;
}

static void ppu_obj_affine_set_palette_color(gbl_machine *machine,
                                             uint8_t index,
                                             uint16_t color)
{
    const size_t offset = UINT32_C(0x200) + (size_t)index * 2u;

    ppu_obj_store_u16(machine->pram, offset, color);
}

static void ppu_obj_affine_set_4bpp_pixel(gbl_machine *machine,
                                          uint16_t tile_base,
                                          uint8_t source_x,
                                          uint8_t source_y,
                                          uint8_t color_index)
{
    const uint32_t tile_number = (uint32_t)tile_base +
        (uint32_t)(source_y / 8u) * UINT32_C(32) +
        (uint32_t)(source_x / 8u);
    const size_t tile_offset = UINT32_C(0x10000) +
        (size_t)tile_number * 32u +
        (size_t)(source_y & 7u) * 4u + (size_t)((source_x & 7u) / 2u);
    const uint8_t shift = (source_x & 1u) != 0u ? 4u : 0u;
    const uint8_t mask = (uint8_t)(UINT8_C(0x0F) << shift);

    machine->vram[tile_offset] = (uint8_t)(
        (machine->vram[tile_offset] & (uint8_t)~mask) |
        (uint8_t)((color_index & UINT8_C(0x0F)) << shift));
}

static bool ppu_obj_affine_setup(gbl_machine *machine, uint16_t dispcnt,
                                 uint16_t object_x, uint8_t object_y,
                                 uint8_t shape, uint8_t size,
                                 uint8_t matrix, uint16_t tile_base,
                                 bool double_size, int16_t pa, int16_t pb,
                                 int16_t pc, int16_t pd)
{
    size_t object;
    const uint16_t attribute0 = (uint16_t)(
        (uint32_t)object_y | UINT32_C(0x0100) |
        (double_size ? UINT32_C(0x0200) : UINT32_C(0)) |
        ((uint32_t)shape << 14u));
    const uint16_t attribute1 = (uint16_t)(
        ((uint32_t)object_x & UINT32_C(0x01FF)) |
        ((uint32_t)matrix << 9u) | ((uint32_t)size << 14u));

    if (machine == NULL || shape >= 3u || size >= 4u || matrix >= 32u) {
        return false;
    }
    machine->dispcnt = (uint16_t)(dispcnt | UINT16_C(0x1000));
    machine->ppu.dispcnt = machine->dispcnt;
    for (object = 0u; object < 128u; ++object) {
        ppu_obj_store_u16(machine->oam, object * 8u, UINT16_C(0x0200));
    }
    machine->pram[0] = 0u;
    machine->pram[1] = UINT8_C(0x7C); /* Blue backdrop. */
    ppu_obj_affine_set_palette_color(machine, 1u, UINT16_C(0x001F));
    ppu_obj_affine_set_palette_color(machine, 2u, UINT16_C(0x03E0));
    ppu_obj_affine_set_palette_color(machine, 3u, UINT16_C(0x0000));
    ppu_obj_affine_set_palette_color(machine, 4u, UINT16_C(0x7FE0));
    ppu_obj_affine_set_palette_color(machine, 5u, UINT16_C(0x7FFF));
    ppu_obj_affine_store_matrix(machine->oam, matrix, pa, pb, pc, pd);
    ppu_obj_store_u16(machine->oam, 0u, attribute0);
    ppu_obj_store_u16(machine->oam, 2u, attribute1);
    ppu_obj_store_u16(machine->oam, 4u, tile_base);
    return true;
}

static bool ppu_obj_affine_render_to_line(gbl_machine *machine,
                                          uint16_t line)
{
    gbl_machine_time_preview preview;
    const uint64_t target =
        (uint64_t)line * (uint64_t)GBL_PPU_LINE_CYCLES;

    if (machine == NULL ||
        gbl_machine_prepare_time(machine, target, &preview) !=
            GBL_TIME_STATUS_OK) {
        return false;
    }
    gbl_machine_commit_time(machine, &preview);
    return true;
}

static bool ppu_obj_prepare_tracer(gbl_machine *machine)
{
    uint8_t row = 0u;
    size_t object;

    if (machine == NULL) {
        return false;
    }
    machine->dispcnt = UINT16_C(0x1100);
    machine->ppu.dispcnt = machine->dispcnt;
    for (object = 0u; object < 128u; ++object) {
        ppu_obj_store_u16(machine->oam, object * 8u, UINT16_C(0x0200));
    }
    ppu_obj_store_u16(machine->oam, 0u, UINT16_C(8));
    ppu_obj_store_u16(machine->oam, 2u, UINT16_C(16));
    ppu_obj_store_u16(machine->oam, 4u, 0u);

    /* BG0 contains one green tile under the OBJ. The zero-filled map and
     * tile remain transparent, revealing the blue backdrop elsewhere. */
    ppu_obj_store_u16(machine->vram, (1u * 32u + 2u) * 2u, 1u);
    memset(&machine->vram[32u], UINT8_C(0x11), 32u);
    machine->pram[0u] = 0u;
    machine->pram[1u] = UINT8_C(0x7C);
    machine->pram[2u] = UINT8_C(0xE0);
    machine->pram[3u] = UINT8_C(0x03);
    machine->pram[UINT32_C(0x200)] = UINT8_C(0xFF);
    machine->pram[UINT32_C(0x201)] = UINT8_C(0x7F);
    machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F);
    machine->pram[UINT32_C(0x203)] = 0u;
    machine->pram[UINT32_C(0x204)] = 0u;
    machine->pram[UINT32_C(0x205)] = 0u;

    for (row = 0u; row < 8u; ++row) {
        const size_t offset = UINT32_C(0x10000) + (size_t)row * 4u;

        machine->vram[offset] = UINT8_C(0x01);
        machine->vram[offset + 1u] = UINT8_C(0x02);
        machine->vram[offset + 2u] = 0u;
        machine->vram[offset + 3u] = 0u;
    }
    return true;
}

static bool ppu_obj_budget_prepare_base(gbl_machine *machine)
{
    size_t object;
    size_t tile;

    if (machine == NULL) {
        return false;
    }
    machine->dispcnt = UINT16_C(0x1040);
    machine->ppu.dispcnt = machine->dispcnt;
    for (object = 0u; object < 128u; ++object) {
        ppu_obj_store_u16(machine->oam, object * 8u, UINT16_C(0x0200));
    }
    for (tile = 0u; tile < 32u; ++tile) {
        memset(&machine->vram[UINT32_C(0x10000) + tile * 32u],
               UINT8_C(0x11), 32u);
    }
    machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F);
    machine->pram[UINT32_C(0x203)] = 0u;
    machine->pram[UINT32_C(0x222)] = UINT8_C(0xE0);
    machine->pram[UINT32_C(0x223)] = UINT8_C(0x03);
    machine->pram[UINT32_C(0x242)] = UINT8_C(0x1F);
    machine->pram[UINT32_C(0x243)] = UINT8_C(0x7C);
    machine->pram[UINT32_C(0x262)] = UINT8_C(0xFF);
    machine->pram[UINT32_C(0x263)] = UINT8_C(0x7F);
    return true;
}

static void ppu_obj_budget_set_regular_ex(
    gbl_machine *machine, size_t object, uint16_t x, uint8_t y,
    uint8_t shape, uint8_t size, uint8_t palette_bank,
    uint16_t tile_base, uint8_t priority)
{
    const size_t offset = object * 8u;

    ppu_obj_store_u16(machine->oam, offset,
                      (uint16_t)((uint16_t)y |
                                 ((uint32_t)shape << 14u)));
    ppu_obj_store_u16(machine->oam, offset + 2u,
                      (uint16_t)((x & UINT16_C(0x01FF)) |
                                 ((uint16_t)size << 14u)));
    ppu_obj_store_u16(machine->oam, offset + 4u,
                      (uint16_t)((tile_base & UINT16_C(0x03FF)) |
                                 ((uint16_t)priority << 10u) |
                                 ((uint16_t)palette_bank << 12u)));
}

static void ppu_obj_budget_set_regular(gbl_machine *machine, size_t object,
                                       uint16_t x, uint8_t y,
                                       uint8_t shape, uint8_t size,
                                       uint8_t palette_bank)
{
    ppu_obj_budget_set_regular_ex(machine, object, x, y, shape, size,
                                  palette_bank, 0u, 0u);
}

static void ppu_obj_budget_set_affine(gbl_machine *machine, size_t object,
                                      uint16_t x, uint8_t y,
                                      uint8_t shape, uint8_t size,
                                      uint8_t palette_bank, bool double_size)
{
    const size_t offset = object * 8u;

    ppu_obj_store_u16(machine->oam, offset,
                      (uint16_t)((uint16_t)y | UINT16_C(0x0100) |
                                 (double_size ? UINT16_C(0x0200) : 0u) |
                                 ((uint32_t)shape << 14u)));
    ppu_obj_store_u16(machine->oam, offset + 2u,
                      (uint16_t)((x & UINT16_C(0x01FF)) |
                                 ((uint16_t)size << 14u)));
    ppu_obj_store_u16(machine->oam, offset + 4u,
                      (uint16_t)((uint16_t)palette_bank << 12u));
}

static bool ppu_obj_budget_prepare_exact_fit_scene(gbl_machine *machine)
{
    size_t object;

    if (!ppu_obj_budget_prepare_base(machine)) {
        return false;
    }
    for (object = 0u; object < 18u; ++object) {
        ppu_obj_budget_set_regular(machine, object, 0u, 0u, 0u, 3u, 0u);
    }
    ppu_obj_budget_set_affine(machine, 18u, 100u, 0u, 0u, 2u, 1u, false);
    ppu_obj_affine_store_matrix(machine->oam, 0u, 256, 0, 0, 256);
    ppu_obj_budget_set_regular(machine, 19u, 200u, 0u, 0u, 0u, 2u);
    return true;
}

static bool ppu_obj_budget_prepare_first_overflow_scene(
    gbl_machine *machine)
{
    size_t object;

    if (!ppu_obj_budget_prepare_base(machine)) {
        return false;
    }
    for (object = 0u; object < 19u; ++object) {
        ppu_obj_budget_set_regular(machine, object, 0u, 0u, 0u, 3u, 0u);
    }
    ppu_obj_budget_set_regular(machine, 19u, 120u, 0u, 0u, 1u, 1u);
    ppu_obj_budget_set_regular(machine, 20u, 140u, 0u, 0u, 0u, 2u);
    return true;
}

static bool ppu_obj_budget_prepare_no_refund_scene(
    gbl_machine *machine, bool transparent)
{
    size_t object;

    if (!ppu_obj_budget_prepare_base(machine)) {
        return false;
    }
    if (transparent) {
        memset(&machine->vram[UINT32_C(0x10000) + 16u * 32u], 0,
               8u * 32u);
        ppu_obj_budget_set_regular_ex(machine, 0u, 100u, 0u, 0u, 3u,
                                      0u, 16u, 0u);
        for (object = 1u; object < 18u; ++object) {
            ppu_obj_budget_set_regular_ex(machine, object, 0u, 0u, 0u,
                                          3u, 0u, 1u, 0u);
        }
    } else {
        for (object = 0u; object < 17u; ++object) {
            ppu_obj_budget_set_regular_ex(machine, object, 0u, 0u, 0u,
                                          3u, 0u, 1u, 0u);
        }
        ppu_obj_budget_set_regular_ex(machine, 17u, 400u, 0u, 0u, 3u,
                                      1u, 1u, 0u);
    }
    ppu_obj_budget_set_affine(machine, 18u, transparent ? 160u : 100u,
                              0u, 0u, 2u, 1u, false);
    ppu_obj_affine_store_matrix(machine->oam, 0u, 256, 0, 0, 256);
    ppu_obj_budget_set_regular(machine, 19u, 200u, 0u, 0u, 0u, 3u);
    return true;
}

static bool ppu_obj_budget_observation_matches(
    const gbl_ppu_row_capture *capture,
    const uint8_t selected_mask[GBL_PPU_OBJ_SELECTED_MASK_BYTES],
    uint16_t selected_count, uint16_t omitted_count, uint16_t work_used)
{
    return capture != NULL && selected_mask != NULL &&
           capture->obj_selected_count == selected_count &&
           capture->obj_omitted_count == omitted_count &&
           capture->obj_work_used == work_used &&
           memcmp(capture->obj_selected_mask, selected_mask,
                  GBL_PPU_OBJ_SELECTED_MASK_BYTES) == 0;
}

static bool ppu_obj_budget_rows_bounded(
    const gbl_machine_time_preview *preview)
{
    size_t row;

    if (preview == NULL ||
        preview->ppu_row_capture_count != GBL_PPU_VISIBLE_LINES) {
        return false;
    }
    for (row = 0u; row < preview->ppu_row_capture_count; ++row) {
        const gbl_ppu_row_capture *capture = &preview->ppu_row_captures[row];
        uint16_t selected_bits = 0u;
        size_t index;

        for (index = 0u; index < GBL_PPU_OBJ_SELECTED_MASK_BYTES; ++index) {
            uint8_t mask_byte = capture->obj_selected_mask[index];

            while (mask_byte != 0u) {
                selected_bits = (uint16_t)(selected_bits +
                    (uint16_t)(mask_byte & UINT8_C(1)));
                mask_byte = (uint8_t)(mask_byte >> 1u);
            }
        }
        if (capture->obj_work_used > GBL_PPU_OBJ_WORK_BUDGET ||
            selected_bits != capture->obj_selected_count ||
            capture->obj_selected_count + capture->obj_omitted_count >
                128u) {
            return false;
        }
    }
    return true;
}

static bool ppu_obj_budget_mutations_refused(
    gbl_machine *machine, const gbl_machine_time_preview *preview)
{
    gbl_machine *before = malloc(sizeof(*before));
    gbl_machine_time_preview *changed =
        malloc(sizeof(*changed));
    gbl_machine_time_preview *caller_before =
        malloc(sizeof(*caller_before));
    bool valid = before != NULL && changed != NULL && caller_before != NULL;

    if (!valid) {
        free(caller_before);
        free(changed);
        free(before);
        return false;
    }
#define CHECK_OBJ_BUDGET_MUTATION(change)                                  \
    do {                                                                    \
        memcpy(changed, preview, sizeof(*changed));                         \
        change;                                                             \
        if (gbl_test_time_preview_equal(preview, changed)) {                 \
            valid = false;                                                  \
        }                                                                   \
        memcpy(caller_before, changed, sizeof(*caller_before));             \
        *before = *machine;                                                 \
        gbl_machine_commit_time(machine, changed);                          \
        if (!gbl_test_canonical_equal(before, machine) ||                   \
            !gbl_test_same_instance_identity_equal(before, machine) ||      \
            memcmp(caller_before, changed, sizeof(*caller_before)) != 0) {  \
            valid = false;                                                  \
        }                                                                   \
    } while (0)
    CHECK_OBJ_BUDGET_MUTATION(
        changed->ppu_row_captures[0].obj_selected_mask[0] ^= UINT8_C(1));
    CHECK_OBJ_BUDGET_MUTATION(
        ++changed->ppu_row_captures[0].obj_selected_count);
    CHECK_OBJ_BUDGET_MUTATION(
        ++changed->ppu_row_captures[0].obj_omitted_count);
    CHECK_OBJ_BUDGET_MUTATION(
        ++changed->ppu_row_captures[0].obj_work_used);
#undef CHECK_OBJ_BUDGET_MUTATION
    free(caller_before);
    free(changed);
    free(before);
    return valid;
}

static int run_ppu_obj_budget(void)
{
    const uint64_t target =
        (uint64_t)GBL_PPU_LINE_CYCLES * UINT64_C(160);
    uint8_t exact_mask[GBL_PPU_OBJ_SELECTED_MASK_BYTES] = { 0u };
    size_t object;
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;

    for (object = 0u; object < 19u; ++object) {
        exact_mask[object >> 3u] |=
            (uint8_t)(UINT8_C(1) << (object & 7u));
    }
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_obj_budget_prepare_exact_fit_scene(machine));
        CHECK(gbl_machine_prepare_time(machine, target, &preview) ==
              GBL_TIME_STATUS_OK);
        CHECK(preview.ppu_row_capture_count == GBL_PPU_VISIBLE_LINES);
        CHECK(ppu_obj_budget_rows_bounded(&preview));
        CHECK(ppu_obj_budget_observation_matches(
            &preview.ppu_row_captures[0], exact_mask, 19u, 1u,
            GBL_PPU_OBJ_WORK_BUDGET));
        {
            uint8_t wrong_selection[GBL_PPU_OBJ_SELECTED_MASK_BYTES];
            gbl_machine *clean_twin = new_machine();
            gbl_machine_time_preview twin_preview;

            memcpy(wrong_selection, exact_mask, sizeof(wrong_selection));
            wrong_selection[0] ^= UINT8_C(1);
            CHECK(!ppu_obj_budget_observation_matches(
                &preview.ppu_row_captures[0], wrong_selection, 19u, 1u,
                GBL_PPU_OBJ_WORK_BUDGET));
            CHECK(!ppu_obj_budget_observation_matches(
                &preview.ppu_row_captures[0], exact_mask, 19u, 1u,
                (uint16_t)(GBL_PPU_OBJ_WORK_BUDGET - 1u)));
            CHECK(clean_twin != NULL);
            if (clean_twin != NULL) {
                CHECK(ppu_obj_budget_prepare_exact_fit_scene(clean_twin));
                CHECK(gbl_machine_prepare_time(clean_twin, target,
                                               &twin_preview) ==
                      GBL_TIME_STATUS_OK);
                CHECK(gbl_test_time_preview_equal(&preview,
                                                 &twin_preview));
                CHECK(ppu_obj_budget_observation_matches(
                    &twin_preview.ppu_row_captures[0], exact_mask, 19u, 1u,
                    GBL_PPU_OBJ_WORK_BUDGET));
                free_machine(clean_twin);
            }
        }
        CHECK(ppu_obj_budget_mutations_refused(machine, &preview));
        gbl_machine_commit_time(machine, &preview);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        CHECK(machine->master_cycles == target);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          UINT16_C(0x001F)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 100u, 0u,
                                          UINT16_C(0x03E0)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 200u, 0u,
                                          0u));
        free_machine(machine);
    }

    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        gbl_machine *repeated = new_machine();
        gbl_machine *timing_twin = new_machine();
        gbl_machine_time_preview repeated_preview;
        gbl_machine_time_preview timing_preview;
        uint8_t first_overflow_mask[GBL_PPU_OBJ_SELECTED_MASK_BYTES] = { 0u };

        for (object = 0u; object < 19u; ++object) {
            first_overflow_mask[object >> 3u] |=
                (uint8_t)(UINT8_C(1) << (object & 7u));
        }
        CHECK(ppu_obj_budget_prepare_first_overflow_scene(machine));
        CHECK(gbl_machine_prepare_time(machine, target, &preview) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_obj_budget_rows_bounded(&preview));
        CHECK(ppu_obj_budget_observation_matches(
            &preview.ppu_row_captures[0], first_overflow_mask, 19u, 2u,
            1216u));
        CHECK(repeated != NULL);
        if (repeated != NULL) {
            CHECK(ppu_obj_budget_prepare_first_overflow_scene(repeated));
            CHECK(gbl_machine_prepare_time(repeated, target,
                                           &repeated_preview) ==
                  GBL_TIME_STATUS_OK);
            CHECK(gbl_test_time_preview_equal(&preview, &repeated_preview));
        }
        CHECK(timing_twin != NULL);
        if (timing_twin != NULL) {
            CHECK(ppu_obj_budget_prepare_base(timing_twin));
            ppu_obj_budget_set_regular(timing_twin, 0u, 0u, 0u, 0u, 0u,
                                       0u);
            CHECK(gbl_machine_prepare_time(timing_twin, target,
                                           &timing_preview) ==
                  GBL_TIME_STATUS_OK);
            CHECK(timing_twin->dispcnt == machine->dispcnt);
        }
        gbl_machine_commit_time(machine, &preview);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 120u, 0u,
                                          0u));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 140u, 0u,
                                          0u));
        CHECK(machine->master_cycles == target);
        if (repeated != NULL) {
            gbl_machine_commit_time(repeated, &repeated_preview);
            CHECK(repeated->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(memcmp(machine->video_frame, repeated->video_frame,
                         GBL_PPU_VIDEO_BYTES) == 0);
            free_machine(repeated);
        }
        if (timing_twin != NULL) {
            gbl_machine_commit_time(timing_twin, &timing_preview);
            CHECK(gbl_test_time_state_equal(machine, timing_twin));
            CHECK(machine->master_cycles == timing_twin->master_cycles);
            CHECK(machine->next_event_sequence ==
                  timing_twin->next_event_sequence);
            free_machine(timing_twin);
        }
        free_machine(machine);
    }

    {
        uint8_t variant;

        for (variant = 0u; variant < 2u; ++variant) {
            gbl_machine *no_refund = new_machine();

            CHECK(no_refund != NULL);
            if (no_refund != NULL) {
                CHECK(ppu_obj_budget_prepare_no_refund_scene(
                    no_refund, variant == 0u));
                CHECK(gbl_machine_prepare_time(no_refund, target,
                                               &preview) ==
                      GBL_TIME_STATUS_OK);
                CHECK(ppu_obj_budget_observation_matches(
                    &preview.ppu_row_captures[0], exact_mask, 19u, 1u,
                    GBL_PPU_OBJ_WORK_BUDGET));
                gbl_machine_commit_time(no_refund, &preview);
                CHECK(no_refund->ppu.capture_state ==
                      GBL_VIDEO_CAPTURE_VALID);
                CHECK(ppu_bg_affine_pixel_matches(no_refund->video_frame,
                                                  200u, 0u, 0u));
                if (variant == 0u) {
                    CHECK(ppu_bg_affine_pixel_matches(no_refund->video_frame,
                                                      100u, 0u, 0u));
                } else {
                    CHECK(ppu_bg_affine_pixel_matches(no_refund->video_frame,
                                                      400u, 0u, 0u));
                }
                free_machine(no_refund);
            }
        }
    }

    {
        uint8_t double_size;

        for (double_size = 0u; double_size < 2u; ++double_size) {
            gbl_machine *affine = new_machine();
            const uint16_t expected_work = double_size != 0u ? 138u : 74u;
            uint8_t one_selected[GBL_PPU_OBJ_SELECTED_MASK_BYTES] = { 1u };

            CHECK(affine != NULL);
            if (affine != NULL) {
                CHECK(ppu_obj_budget_prepare_base(affine));
                ppu_obj_budget_set_affine(affine, 0u, 100u, 0u, 0u, 2u,
                                          1u, double_size != 0u);
                ppu_obj_affine_store_matrix(affine->oam, 0u, 256, 0, 0,
                                            256);
                CHECK(gbl_machine_prepare_time(affine, target, &preview) ==
                      GBL_TIME_STATUS_OK);
                CHECK(ppu_obj_budget_observation_matches(
                    &preview.ppu_row_captures[0], one_selected, 1u, 0u,
                    expected_work));
                gbl_machine_commit_time(affine, &preview);
                CHECK(affine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
                free_machine(affine);
            }
        }
    }

    {
        gbl_machine *mixed = new_machine();
        const uint8_t mixed_mask[GBL_PPU_OBJ_SELECTED_MASK_BYTES] = { 0x0Fu };

        CHECK(mixed != NULL);
        if (mixed != NULL) {
            CHECK(ppu_obj_budget_prepare_base(mixed));
            ppu_obj_budget_set_regular(mixed, 0u, 0u, 0u, 0u, 0u, 0u);
            ppu_obj_budget_set_regular(mixed, 1u, 16u, 0u, 1u, 0u, 1u);
            ppu_obj_budget_set_regular(mixed, 2u, 32u, 0u, 0u, 2u, 2u);
            ppu_obj_budget_set_affine(mixed, 3u, 80u, 0u, 0u, 1u, 3u,
                                      false);
            ppu_obj_affine_store_matrix(mixed->oam, 0u, 256, 0, 0, 256);
            CHECK(gbl_machine_prepare_time(mixed, target, &preview) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_obj_budget_observation_matches(
                &preview.ppu_row_captures[0], mixed_mask, 4u, 0u, 98u));
            gbl_machine_commit_time(mixed, &preview);
            CHECK(mixed->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            free_machine(mixed);
        }
    }

    {
        gbl_machine *ineligible = new_machine();
        uint8_t ineligible_mask[GBL_PPU_OBJ_SELECTED_MASK_BYTES] = { 0u };

        for (object = 0u; object < 17u; ++object) {
            ineligible_mask[object >> 3u] |=
                (uint8_t)(UINT8_C(1) << (object & 7u));
        }
        ineligible_mask[19u >> 3u] |=
            (uint8_t)(UINT8_C(1) << (19u & 7u));
        ineligible_mask[20u >> 3u] |=
            (uint8_t)(UINT8_C(1) << (20u & 7u));
        CHECK(ineligible != NULL);
        if (ineligible != NULL) {
            CHECK(ppu_obj_budget_prepare_base(ineligible));
            for (object = 0u; object < 17u; ++object) {
                ppu_obj_budget_set_regular(ineligible, object, 0u, 0u, 0u,
                                           3u, 0u);
            }
            ppu_obj_budget_set_regular(ineligible, 18u, 0u, 64u, 0u, 3u,
                                       1u);
            ppu_obj_budget_set_regular(ineligible, 19u, 0u, 0u, 0u, 3u,
                                       0u);
            ppu_obj_budget_set_affine(ineligible, 20u, 100u, 0u, 0u, 2u,
                                      1u, false);
            ppu_obj_affine_store_matrix(ineligible->oam, 0u, 256, 0, 0,
                                        256);
            ppu_obj_budget_set_regular(ineligible, 21u, 200u, 0u, 0u, 0u,
                                       3u);
            CHECK(gbl_machine_prepare_time(ineligible, target, &preview) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_obj_budget_observation_matches(
                &preview.ppu_row_captures[0], ineligible_mask, 19u, 1u,
                GBL_PPU_OBJ_WORK_BUDGET));
            gbl_machine_commit_time(ineligible, &preview);
            CHECK(ineligible->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(ineligible->video_frame, 200u,
                                              0u, 0u));
            free_machine(ineligible);
        }
    }

    {
        gbl_machine *priority = new_machine();
        const uint8_t priority_mask[GBL_PPU_OBJ_SELECTED_MASK_BYTES] = { 3u };

        CHECK(priority != NULL);
        if (priority != NULL) {
            CHECK(ppu_obj_budget_prepare_base(priority));
            ppu_obj_budget_set_regular_ex(priority, 0u, 10u, 0u, 0u, 0u,
                                          1u, 0u, 3u);
            ppu_obj_budget_set_regular_ex(priority, 1u, 10u, 0u, 0u, 0u,
                                          2u, 0u, 0u);
            CHECK(gbl_machine_prepare_time(priority, target, &preview) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_obj_budget_observation_matches(
                &preview.ppu_row_captures[0], priority_mask, 2u, 0u, 16u));
            gbl_machine_commit_time(priority, &preview);
            CHECK(priority->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(priority->video_frame, 10u,
                                              0u, UINT16_C(0x7C1F)));
            free_machine(priority);
        }
    }

    if (CHECK_DONE() == 0) {
        puts("ppu.obj.budget: exact fit, first overflow, no-refund charging, affine costs, eligibility, priority and stable partial output");
        return 0;
    }
    return 1;
}

static int run_ppu_obj_tracer(void)
{
    gbl_machine *machine = new_machine();

    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_obj_prepare_tracer(machine));
        CHECK(gbl_machine_advance_to(
                  machine, (uint64_t)GBL_PPU_LINE_CYCLES * UINT64_C(160)) ==
              GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 15u, 8u,
                                          UINT16_C(0x7C00)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 16u, 8u,
                                          UINT16_C(0x001F)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 17u, 8u,
                                          UINT16_C(0x03E0)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 18u, 8u,
                                          UINT16_C(0x0000)));
        CHECK(!ppu_bg_affine_pixel_matches(machine->video_frame, 16u, 8u,
                                           UINT16_C(0x7C00)));
        free_machine(machine);
    }
    if (CHECK_DONE() == 0) {
        puts("ppu.obj.tracer: sampled OBJ reaches published frame over BG and backdrop");
        return 0;
    }
    return 1;
}

static int run_ppu_obj_regular(void)
{
    static const struct {
        uint8_t shape;
        uint8_t size;
        uint8_t width;
        uint8_t height;
    } shape_sizes[] = {
        { 0u, 0u, 8u, 8u }, { 0u, 1u, 16u, 16u },
        { 0u, 2u, 32u, 32u }, { 0u, 3u, 64u, 64u },
        { 1u, 0u, 16u, 8u }, { 1u, 1u, 32u, 8u },
        { 1u, 2u, 32u, 16u }, { 1u, 3u, 64u, 32u },
        { 2u, 0u, 8u, 16u }, { 2u, 1u, 8u, 32u },
        { 2u, 2u, 16u, 32u }, { 2u, 3u, 32u, 64u }
    };
    size_t shape_case;
    uint8_t mapping;

    for (mapping = 0u; mapping < 2u; ++mapping) {
        for (shape_case = 0u;
             shape_case < sizeof(shape_sizes) / sizeof(shape_sizes[0]);
             ++shape_case) {
            const uint8_t width = shape_sizes[shape_case].width;
            const uint8_t last_tile = (uint8_t)((width - 1u) / 8u);
            const uint16_t expected_right = (last_tile & 1u) != 0u
                ? UINT16_C(0x03E0)
                : UINT16_C(0x001F);
            gbl_machine *machine = new_machine();
            uint16_t attribute0 = (uint16_t)(
                (uint16_t)shape_sizes[shape_case].shape << 14u);
            uint16_t attribute1 = (uint16_t)(
                (uint16_t)shape_sizes[shape_case].size << 14u);
            uint16_t tile;

            CHECK(machine != NULL);
            if (machine == NULL) {
                continue;
            }
            machine->dispcnt = (uint16_t)(UINT16_C(0x1000) |
                (mapping != 0u ? UINT16_C(0x0040) : 0u));
            machine->ppu.dispcnt = machine->dispcnt;
            for (tile = 0u; tile < 64u; ++tile) {
                const uint8_t index = (tile & 1u) != 0u ? 2u : 1u;
                const size_t offset = UINT32_C(0x10000) +
                                      (size_t)tile * 32u;
                const uint8_t packed = (uint8_t)(index | (index << 4u));

                memset(&machine->vram[offset], packed, 32u);
            }
            machine->pram[UINT32_C(0x200) + 2u] = UINT8_C(0x1F);
            machine->pram[UINT32_C(0x200) + 3u] = 0u;
            machine->pram[UINT32_C(0x200) + 4u] = UINT8_C(0xE0);
            machine->pram[UINT32_C(0x200) + 5u] = UINT8_C(0x03);
            for (tile = 0u; tile < 128u; ++tile) {
                ppu_obj_store_u16(machine->oam, (size_t)tile * 8u,
                                  UINT16_C(0x0200));
            }
            attribute0 = (uint16_t)(attribute0 & UINT16_C(0xFFFF));
            attribute1 = (uint16_t)(attribute1 & UINT16_C(0xFFFF));
            ppu_obj_store_u16(machine->oam, 0u, attribute0);
            ppu_obj_store_u16(machine->oam, 2u, attribute1);
            ppu_obj_store_u16(machine->oam, 4u, 0u);
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame,
                                              (uint16_t)(width - 1u), 0u,
                                              expected_right));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, width, 0u,
                                              0u));
            free_machine(machine);
        }
    }

    for (mapping = 0u; mapping < 2u; ++mapping) {
        gbl_machine *machine = new_machine();
        const uint16_t row_tile = mapping != 0u ? 1u : 32u;
        const uint16_t row_index = mapping != 0u ? 2u : 4u;
        size_t tile;

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        machine->dispcnt = (uint16_t)(UINT16_C(0x1000) |
            (mapping != 0u ? UINT16_C(0x0040) : 0u));
        machine->ppu.dispcnt = machine->dispcnt;
        for (tile = 0u; tile < 33u; ++tile) {
            const uint8_t index = tile == 0u ? 1u :
                                  (tile == 1u ? 2u :
                                   (tile == 32u ? 4u : 3u));
            const uint8_t packed = (uint8_t)(index | (index << 4u));

            memset(&machine->vram[UINT32_C(0x10000) + tile * 32u],
                   packed, 32u);
        }
        machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F);
        machine->pram[UINT32_C(0x203)] = 0u;
        machine->pram[UINT32_C(0x204)] = UINT8_C(0xE0);
        machine->pram[UINT32_C(0x205)] = UINT8_C(0x03);
        machine->pram[UINT32_C(0x208)] = UINT8_C(0xFF);
        machine->pram[UINT32_C(0x209)] = UINT8_C(0x03);
        ppu_obj_store_u16(machine->oam, 0u, UINT16_C(0x8000));
        ppu_obj_store_u16(machine->oam, 2u, 0u);
        ppu_obj_store_u16(machine->oam, 4u, 0u);
        CHECK(gbl_machine_advance_to(
                  machine, (uint64_t)GBL_PPU_LINE_CYCLES * UINT64_C(8)) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(
            machine->video_frame, 0u, 8u,
            row_index == 2u ? UINT16_C(0x03E0) : UINT16_C(0x03FF)));
        CHECK(row_tile == (mapping != 0u ? 1u : 32u));
        free_machine(machine);
    }

    {
        uint8_t mode;

        for (mode = 0u; mode <= 2u; ++mode) {
            gbl_machine *machine = new_machine();
            size_t object;

            CHECK(machine != NULL);
            if (machine == NULL) {
                continue;
            }
            if (mode == 0u) {
                machine->dispcnt = UINT16_C(0x1040);
            } else if (mode == 1u) {
                machine->dispcnt = UINT16_C(0x1141);
            } else {
                machine->dispcnt = UINT16_C(0x1442);
                machine->ppu.affine[0].pa = 256;
                machine->ppu.affine[0].pb = 0;
                machine->ppu.affine[0].pc = 0;
                machine->ppu.affine[0].pd = 256;
                machine->ppu.affine[0].origin_x = 0u;
                machine->ppu.affine[0].origin_y = 0u;
                machine->ppu.affine[0].origin_valid = true;
            }
            machine->ppu.dispcnt = machine->dispcnt;
            memset(&machine->vram[UINT32_C(0x10000)], UINT8_C(0x11), 32u);
            machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F);
            machine->pram[UINT32_C(0x203)] = 0u;
            for (object = 0u; object < 128u; ++object) {
                ppu_obj_store_u16(machine->oam, object * 8u,
                                  UINT16_C(0x0200));
            }
            ppu_obj_store_u16(machine->oam, 0u, 0u);
            ppu_obj_store_u16(machine->oam, 2u, 0u);
            ppu_obj_store_u16(machine->oam, 4u, 0u);
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x001F)));
            free_machine(machine);
        }
    }

    {
        static const struct {
            uint16_t flip;
            uint16_t left;
            uint16_t right;
        } flips[] = {
            { UINT16_C(0x1000), UINT16_C(0x03E0), UINT16_C(0x001F) },
            { UINT16_C(0x2000), UINT16_C(0x7C00), UINT16_C(0x03FF) },
            { UINT16_C(0x3000), UINT16_C(0x03FF), UINT16_C(0x7C00) }
        };
        size_t flip;

        for (flip = 0u; flip < sizeof(flips) / sizeof(flips[0]); ++flip) {
            gbl_machine *machine = new_machine();
            size_t tile;

            CHECK(machine != NULL);
            if (machine == NULL) {
                continue;
            }
            machine->dispcnt = UINT16_C(0x1040);
            machine->ppu.dispcnt = machine->dispcnt;
            for (tile = 0u; tile < 4u; ++tile) {
                const uint8_t index = (uint8_t)(tile + 1u);
                const uint8_t packed = (uint8_t)(index | (index << 4u));

                memset(&machine->vram[UINT32_C(0x10000) + tile * 32u],
                       packed, 32u);
                machine->pram[UINT32_C(0x202) + tile * 2u] =
                    tile == 0u ? UINT8_C(0x1F) :
                    (tile == 1u ? UINT8_C(0xE0) :
                     (tile == 2u ? 0u : UINT8_C(0xFF)));
                machine->pram[UINT32_C(0x203) + tile * 2u] =
                    tile == 0u ? 0u :
                    (tile == 1u ? UINT8_C(0x03) :
                     (tile == 2u ? UINT8_C(0x7C) : UINT8_C(0x03)));
            }
            ppu_obj_store_u16(machine->oam, 0u, 0u);
            ppu_obj_store_u16(machine->oam, 2u,
                              (uint16_t)(UINT16_C(0x4000) | flips[flip].flip));
            ppu_obj_store_u16(machine->oam, 4u, 0u);
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              flips[flip].left));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 15u, 0u,
                                              flips[flip].right));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        size_t object;

        CHECK(machine != NULL);
        if (machine != NULL) {
            machine->dispcnt = UINT16_C(0x1040);
            machine->ppu.dispcnt = machine->dispcnt;
            for (object = 0u; object < 128u; ++object) {
                ppu_obj_store_u16(machine->oam, object * 8u,
                                  UINT16_C(0x0200));
            }
            for (object = 0u; object < 8u; ++object) {
                memset(&machine->vram[UINT32_C(0x10000) + 4u +
                                       object * 4u],
                       UINT8_C(0x22), 4u);
            }
            machine->pram[UINT32_C(0x200) + 2u] = UINT8_C(0x1F);
            machine->pram[UINT32_C(0x200) + 3u] = 0u;
            machine->pram[UINT32_C(0x200) + 4u] = UINT8_C(0xE0);
            machine->pram[UINT32_C(0x200) + 5u] = UINT8_C(0x03);
            machine->pram[1] = UINT8_C(0x7C);
            ppu_obj_store_u16(machine->oam, 0u, UINT16_C(0x00FF));
            ppu_obj_store_u16(machine->oam, 2u, UINT16_C(0x01FC));
            ppu_obj_store_u16(machine->oam, 4u, 0u);
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x03E0)));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 3u, 0u,
                                              UINT16_C(0x03E0)));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 4u, 0u,
                                              UINT16_C(0x7C00)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        size_t object;

        CHECK(machine != NULL);
        if (machine != NULL) {
            machine->dispcnt = UINT16_C(0x1040);
            machine->ppu.dispcnt = machine->dispcnt;
            for (object = 0u; object < 128u; ++object) {
                ppu_obj_store_u16(machine->oam, object * 8u,
                                  UINT16_C(0x0200));
            }
            memset(&machine->vram[UINT32_C(0x10000)], UINT8_C(0x11), 32u);
            memset(&machine->vram[UINT32_C(0x10020)], UINT8_C(0x22), 32u);
            machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F);
            machine->pram[UINT32_C(0x203)] = 0u;
            machine->pram[UINT32_C(0x204)] = UINT8_C(0xE0);
            machine->pram[UINT32_C(0x205)] = UINT8_C(0x03);
            machine->pram[1] = UINT8_C(0x7C);
            ppu_obj_store_u16(machine->oam, 0u, 0u);
            ppu_obj_store_u16(machine->oam, 2u, UINT16_C(0x40EC));
            ppu_obj_store_u16(machine->oam, 4u, 0u);
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 235u, 0u,
                                              UINT16_C(0x7C00)));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 236u, 0u,
                                              UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 239u, 0u,
                                              UINT16_C(0x001F)));
            CHECK(machine->video_frame[GBL_PPU_ROW_BYTES] == 0u);
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        size_t object;

        CHECK(machine != NULL);
        if (machine != NULL) {
            machine->dispcnt = UINT16_C(0x1000);
            machine->ppu.dispcnt = machine->dispcnt;
            for (object = 0u; object < 128u; ++object) {
                ppu_obj_store_u16(machine->oam, object * 8u,
                                  UINT16_C(0x0200));
            }
            memset(&machine->vram[UINT32_C(0x10000)], UINT8_C(0x11), 32u);
            machine->pram[1] = UINT8_C(0x7C);
            machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F);
            ppu_obj_store_u16(machine->oam, 0u, UINT16_C(0x0200));
            ppu_obj_store_u16(machine->oam, 2u, 0u);
            ppu_obj_store_u16(machine->oam, 4u, 0u);
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state != GBL_VIDEO_CAPTURE_INVALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x7C00)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        size_t row;

        CHECK(machine != NULL);
        if (machine != NULL) {
            machine->dispcnt = UINT16_C(0x1000);
            machine->ppu.dispcnt = machine->dispcnt;
            for (row = 0u; row < 8u; ++row) {
                uint8_t *tile_row = &machine->vram[
                    UINT32_C(0x10000) + row * 8u];

                memset(tile_row, 9, 8u);
                tile_row[0] = UINT8_C(255);
                tile_row[1] = 2u;
                memset(&machine->vram[UINT32_C(0x10000) + row * 8u],
                       0, 8u);
                machine->vram[UINT32_C(0x10000) + row * 8u] =
                    UINT8_C(255);
                machine->vram[UINT32_C(0x10000) + row * 8u + 1u] = 2u;
            }
            machine->pram[UINT32_C(0x200) + 255u * 2u] = UINT8_C(0xFF);
            machine->pram[UINT32_C(0x201) + 255u * 2u] = UINT8_C(0x7F);
            machine->pram[UINT32_C(0x204)] = UINT8_C(0xE0);
            machine->pram[UINT32_C(0x205)] = UINT8_C(0x03);
            ppu_obj_store_u16(machine->oam, 0u, UINT16_C(0x2000));
            ppu_obj_store_u16(machine->oam, 2u, 0u);
            ppu_obj_store_u16(machine->oam, 4u, 1u);
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x7FFF)));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 1u, 0u,
                                              UINT16_C(0x03E0)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        CHECK(machine != NULL);
        if (machine != NULL) {
            machine->dispcnt = UINT16_C(0x1000);
            machine->ppu.dispcnt = machine->dispcnt;
            memset(&machine->vram[UINT32_C(0x10000)], UINT8_C(0x21), 32u);
            machine->pram[UINT32_C(0x200) + 241u * 2u] = UINT8_C(0xFF);
            machine->pram[UINT32_C(0x201) + 241u * 2u] = UINT8_C(0x7F);
            machine->pram[UINT32_C(0x200) + 242u * 2u] = 0u;
            machine->pram[UINT32_C(0x201) + 242u * 2u] = UINT8_C(0x7C);
            ppu_obj_store_u16(machine->oam, 0u, 0u);
            ppu_obj_store_u16(machine->oam, 2u, 0u);
            ppu_obj_store_u16(machine->oam, 4u, UINT16_C(0xF000));
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x7FFF)));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 1u, 0u,
                                              UINT16_C(0x7C00)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            machine->dispcnt = UINT16_C(0x1000);
            machine->ppu.dispcnt = machine->dispcnt;
            memset(&machine->vram[UINT32_C(0x10000) + 1023u * 32u],
                   UINT8_C(0x11), 32u);
            machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F);
            machine->pram[UINT32_C(0x203)] = 0u;
            machine->pram[0] = 0u;
            machine->pram[1] = UINT8_C(0x7C);
            ppu_obj_store_u16(machine->oam, 0u, UINT16_C(0x8000));
            ppu_obj_store_u16(machine->oam, 2u, 0u);
            ppu_obj_store_u16(machine->oam, 4u, UINT16_C(1023));
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES * UINT64_C(8)) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                              UINT16_C(0x7C00)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        CHECK(machine != NULL);
        if (machine != NULL) {
            machine->dispcnt = UINT16_C(0x1403);
            machine->ppu.dispcnt = machine->dispcnt;
            machine->ppu.affine[0].pa = 256;
            machine->ppu.affine[0].pb = 0;
            machine->ppu.affine[0].pc = 0;
            machine->ppu.affine[0].pd = 256;
            machine->ppu.affine[0].origin_x = 0u;
            machine->ppu.affine[0].origin_y = 0u;
            machine->ppu.affine[0].origin_valid = true;
            machine->vram[0] = UINT8_C(0xE0);
            machine->vram[1] = UINT8_C(0x03);
            machine->vram[UINT32_C(0x0F00)] = UINT8_C(0xE0);
            machine->vram[UINT32_C(0x0F01)] = UINT8_C(0x03);
            memset(&machine->vram[UINT32_C(0x10000) + 1023u * 32u],
                   UINT8_C(0x11), 32u);
            machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F);
            ppu_obj_store_u16(machine->oam, 0u, UINT16_C(0x8000));
            ppu_obj_store_u16(machine->oam, 2u, 0u);
            ppu_obj_store_u16(machine->oam, 4u, UINT16_C(1023));
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES * UINT64_C(8)) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                              UINT16_C(0x03E0)));
            free_machine(machine);
        }
    }

    {
        uint8_t mode;

        for (mode = 3u; mode <= 5u; ++mode) {
            gbl_machine *unavailable = new_machine();
            gbl_machine *available = new_machine();
            uint16_t background_color = UINT16_C(0x03E0);

            CHECK(unavailable != NULL && available != NULL);
            if (unavailable == NULL || available == NULL) {
                free_machine(unavailable);
                free_machine(available);
                continue;
            }
            {
                gbl_machine *scenes[2] = { unavailable, available };
                uint8_t scene;

                for (scene = 0u; scene < 2u; ++scene) {
                    gbl_machine *scene_machine = scenes[scene];
                    const uint16_t tile = scene == 0u ? 511u : 512u;
                    const size_t tile_offset = UINT32_C(0x10000) +
                                               (size_t)tile * 32u;

                    scene_machine->dispcnt = (uint16_t)(mode |
                        UINT16_C(0x1400));
                    scene_machine->ppu.dispcnt = scene_machine->dispcnt;
                    scene_machine->ppu.affine[0].pa = 256;
                    scene_machine->ppu.affine[0].pb = 0;
                    scene_machine->ppu.affine[0].pc = 0;
                    scene_machine->ppu.affine[0].pd = 256;
                    scene_machine->ppu.affine[0].origin_x = 0u;
                    scene_machine->ppu.affine[0].origin_y = 0u;
                    scene_machine->ppu.affine[0].origin_valid = true;
                    if (mode == 4u) {
                        scene_machine->vram[0] = 1u;
                        scene_machine->pram[2] = UINT8_C(0xE0);
                        scene_machine->pram[3] = UINT8_C(0x03);
                    } else {
                        scene_machine->vram[0] = UINT8_C(0xE0);
                        scene_machine->vram[1] = UINT8_C(0x03);
                    }
                    memset(&scene_machine->vram[tile_offset],
                           UINT8_C(0x11), 32u);
                    scene_machine->pram[UINT32_C(0x202)] = UINT8_C(0x1F);
                    scene_machine->pram[UINT32_C(0x203)] = 0u;
                    ppu_obj_store_u16(scene_machine->oam, 0u, 0u);
                    ppu_obj_store_u16(scene_machine->oam, 2u, 0u);
                    ppu_obj_store_u16(scene_machine->oam, 4u, tile);
                    CHECK(gbl_machine_advance_to(scene_machine, 0u) ==
                          GBL_TIME_STATUS_OK);
                    CHECK(ppu_bg_affine_pixel_matches(
                        scene_machine->video_frame, 0u, 0u,
                        scene == 0u ? background_color : UINT16_C(0x001F)));
                }
            }
            free_machine(unavailable);
            free_machine(available);
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            size_t tile;

            machine->dispcnt = UINT16_C(0x1040);
            machine->ppu.dispcnt = machine->dispcnt;
            for (tile = 0u; tile < 4u; ++tile) {
                const uint8_t index = (uint8_t)(tile + 1u);
                const uint8_t packed = (uint8_t)(index | (index << 4u));

                memset(&machine->vram[UINT32_C(0x10000) + tile * 32u],
                       packed, 32u);
                machine->pram[UINT32_C(0x202) + tile * 2u] =
                    tile == 0u ? UINT8_C(0x1F) : UINT8_C(0xE0);
                machine->pram[UINT32_C(0x203) + tile * 2u] =
                    tile == 0u ? 0u : UINT8_C(0x03);
            }
            ppu_obj_store_u16(machine->oam, 127u * 8u, 0u);
            ppu_obj_store_u16(machine->oam, 127u * 8u + 2u, 0u);
            ppu_obj_store_u16(machine->oam, 127u * 8u + 4u, 0u);
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x001F)));
            free_machine(machine);
        }
    }

    {
        static const uint16_t unsupported_attributes[] = {
            UINT16_C(0x0400), UINT16_C(0x0800), UINT16_C(0x0C00),
            UINT16_C(0x1000), UINT16_C(0xC000)
        };
        size_t control;

        for (control = 0u;
             control < sizeof(unsupported_attributes) /
                           sizeof(unsupported_attributes[0]);
             ++control) {
            gbl_machine *machine = new_machine();
            gbl_machine *snapshot = malloc(sizeof(*snapshot));
            gbl_machine_time_preview preview;

            CHECK(machine != NULL && snapshot != NULL);
            if (machine == NULL || snapshot == NULL) {
                free(snapshot);
                free_machine(machine);
                continue;
            }
            machine->dispcnt = UINT16_C(0x1000);
            machine->ppu.dispcnt = machine->dispcnt;
            ppu_obj_store_u16(machine->oam, 0u,
                              unsupported_attributes[control]);
            *snapshot = *machine;
            CHECK(gbl_machine_prepare_time(machine, 0u, &preview) ==
                  GBL_TIME_STATUS_OK);
            CHECK(preview.ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID);
            CHECK(preview.ppu.capture_reason ==
                  GBL_VIDEO_INVALID_REASON_OBJ_ENABLED);
            CHECK(gbl_test_canonical_equal(snapshot, machine));
            free(snapshot);
            free_machine(machine);
        }
    }

    if (CHECK_DONE() == 0) {
        puts("ppu.obj.regular: shape/size, 1D/2D, palette, flip, wrap and physical OBJ bounds");
        return 0;
    }
    return 1;
}

static int run_ppu_obj_affine(void)
{
    static const struct {
        bool double_size;
        uint16_t sample_x;
        uint16_t sample_y;
        uint16_t expected_color;
        uint16_t outside_x;
        uint16_t outside_y;
        uint16_t outside_color;
    } cases[] = {
        { false, 24u, 24u, UINT16_C(0x001F), 40u, 24u,
          UINT16_C(0x7C00) },
        { true, 32u, 32u, UINT16_C(0x001F), 24u, 24u,
          UINT16_C(0x7C00) }
    };
    size_t case_index;

    for (case_index = 0u;
         case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
        gbl_machine *machine = new_machine();
        gbl_machine_time_preview preview;
        const uint64_t target =
            (uint64_t)cases[case_index].sample_y *
            (uint64_t)GBL_PPU_LINE_CYCLES;

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        CHECK(ppu_obj_affine_prepare_basic(
            machine, cases[case_index].double_size));
        CHECK(gbl_machine_prepare_time(machine, target, &preview) ==
              GBL_TIME_STATUS_OK);
        gbl_machine_commit_time(machine, &preview);
        CHECK(ppu_bg_affine_pixel_matches(
            machine->video_frame, cases[case_index].sample_x,
            cases[case_index].sample_y, cases[case_index].expected_color));
        CHECK(ppu_bg_affine_pixel_matches(
            machine->video_frame, cases[case_index].outside_x,
            cases[case_index].outside_y, cases[case_index].outside_color));
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool clean_corner;
        bool wrong_corner;

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 24u, 24u, 0u, 1u, 0u, 0u, false,
                0, -256, 256, 0));
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 1u, 1u, 1u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 14u, 2u, 2u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 2u, 14u, 4u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 14u, 14u, 5u);
            CHECK(ppu_obj_affine_render_to_line(machine, 39u));
            clean_corner = ppu_bg_affine_pixel_matches(
                machine->video_frame, 25u, 39u, UINT16_C(0x001F));
            wrong_corner = ppu_bg_affine_pixel_matches(
                machine->video_frame, 25u, 25u, UINT16_C(0x001F));
            CHECK(clean_corner);
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 26u, 26u, UINT16_C(0x03E0)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 38u, 38u, UINT16_C(0x7FE0)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 38u, 26u, UINT16_C(0x7FFF)));
            CHECK(!wrong_corner);
            if (clean_corner && !wrong_corner) {
                puts("ppu.obj.affine control: wrong quarter-turn corner expectation rejected; clean twin passed");
            }
        }
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 24u, 24u, 0u, 1u, 1u, 0u, false,
                -256, 0, 0, 256));
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 14u, 3u, 1u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 1u, 12u, 2u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 14u, 12u, 4u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 1u, 3u, 5u);
            CHECK(ppu_obj_affine_render_to_line(machine, 36u));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 26u, 27u, UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 39u, 36u, UINT16_C(0x03E0)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 26u, 36u, UINT16_C(0x7FE0)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 39u, 27u, UINT16_C(0x7FFF)));
        }
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 24u, 24u, 0u, 1u, 1u, 0u, false,
                256, 128, 0, 256));
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 4u, 2u, 1u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 12u, 13u, 2u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 4u, 13u, 4u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 12u, 2u, 5u);
            CHECK(ppu_obj_affine_render_to_line(machine, 37u));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 31u, 26u, UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 34u, 37u, UINT16_C(0x03E0)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 26u, 37u, UINT16_C(0x7FE0)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 39u, 26u, UINT16_C(0x7FFF)));
        }
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        bool clean_fraction;
        bool truncation_control;

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 24u, 24u, 0u, 1u, 2u, 0u, false,
                128, 0, 0, 256));
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 7u, 8u, 1u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 8u, 8u, 2u);
            CHECK(ppu_obj_affine_render_to_line(machine, 32u));
            clean_fraction = ppu_bg_affine_pixel_matches(
                machine->video_frame, 31u, 32u, UINT16_C(0x001F));
            truncation_control = ppu_bg_affine_pixel_matches(
                machine->video_frame, 31u, 32u, UINT16_C(0x03E0));
            CHECK(clean_fraction);
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 32u, 32u, UINT16_C(0x03E0)));
            CHECK(!truncation_control);
            if (clean_fraction && !truncation_control) {
                puts("ppu.obj.affine control: negative half-pixel truncation expectation rejected; floor twin passed");
            }
        }
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 24u, 24u, 0u, 1u, 4u, 0u, false,
                257, 0, 0, 257));
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 0u, 8u, 1u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 8u, 0u, 2u);
            CHECK(ppu_obj_affine_render_to_line(machine, 32u));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 24u, 32u, UINT16_C(0x7C00)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 25u, 32u, UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 32u, 24u, UINT16_C(0x7C00)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 32u, 25u, UINT16_C(0x03E0)));
        }
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 24u, 24u, 0u, 1u, 3u, 0u, true,
                256, 0, 0, 256));
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 0u, 0u, 1u);
            memset(&machine->vram[UINT32_C(0x10000) + 2u * 32u],
                   UINT8_C(0x22), 32u);
            CHECK(ppu_obj_affine_render_to_line(machine, 32u));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 32u, 32u, UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 24u, 24u, UINT16_C(0x7C00)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 48u, 32u, UINT16_C(0x7C00)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 56u, 32u, UINT16_C(0x7C00)));
        }
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 24u, 24u, 0u, 1u, 31u, 0u, false,
                INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX));
            ppu_obj_affine_store_matrix(
                machine->oam, 30u, 256, 0, 0, 256);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 8u, 8u, 3u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 9u, 8u, 1u);
            CHECK(ppu_obj_affine_render_to_line(machine, 33u));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 32u, 32u, UINT16_C(0x0000)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 33u, 32u, UINT16_C(0x7C00)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 31u, 32u, UINT16_C(0x7C00)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 32u, 33u, UINT16_C(0x7C00)));
        }
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 508u, 252u, 0u, 0u, 0u, 0u, false,
                256, 0, 0, 256));
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 4u, 4u, 4u);
            CHECK(ppu_obj_affine_render_to_line(machine, 0u));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 0u, 0u, UINT16_C(0x7FE0)));
        }
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 238u, 8u, 0u, 0u, 0u, 0u, false,
                256, 0, 0, 256));
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 0u, 0u, 1u);
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 1u, 0u, 2u);
            CHECK(ppu_obj_affine_render_to_line(machine, 8u));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 238u, 8u, UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 239u, 8u, UINT16_C(0x03E0)));
        }
        free_machine(machine);
    }

    {
        uint8_t mode;
        uint8_t base_case;

        for (mode = 0u; mode <= 5u; ++mode) {
            for (base_case = 0u; base_case < 2u; ++base_case) {
                const uint16_t tile_base = base_case == 0u ? 511u : 512u;
                const uint16_t bg_enable = mode == 0u ? 0u :
                    (mode == 1u ? UINT16_C(0x0100) : UINT16_C(0x0400));
                const uint16_t dispcnt = (uint16_t)mode | bg_enable;
                gbl_machine *machine = new_machine();
                uint16_t expected_color = UINT16_C(0x001F);
                size_t tile_offset = UINT32_C(0x10000) +
                                     (size_t)tile_base * 32u;

                CHECK(machine != NULL);
                if (machine == NULL) {
                    continue;
                }
                CHECK(ppu_obj_affine_setup(
                    machine, dispcnt, 0u, 0u, 0u, 0u, 0u,
                    tile_base, false, 256, 0, 0, 256));
                memset(&machine->vram[tile_offset], UINT8_C(0x11), 32u);
                if (mode == 3u || mode == 5u) {
                    machine->vram[0] = UINT8_C(0xE0);
                    machine->vram[1] = UINT8_C(0x03);
                    expected_color = tile_base == 511u
                        ? UINT16_C(0x03E0)
                        : UINT16_C(0x001F);
                } else if (mode == 4u) {
                    machine->vram[0] = 1u;
                    machine->pram[2] = UINT8_C(0xE0);
                    machine->pram[3] = UINT8_C(0x03);
                    expected_color = tile_base == 511u
                        ? UINT16_C(0x03E0)
                        : UINT16_C(0x001F);
                } else if (mode == 2u && tile_base == 511u) {
                    expected_color = UINT16_C(0x7C00);
                }
                CHECK(ppu_obj_affine_render_to_line(machine, 0u));
                if (tile_base == 511u && mode >= 3u) {
                    CHECK(ppu_bg_affine_pixel_matches(
                        machine->video_frame, 0u, 0u, expected_color));
                } else {
                    CHECK(ppu_bg_affine_pixel_matches(
                        machine->video_frame, 0u, 0u, UINT16_C(0x001F)));
                }
                free_machine(machine);
            }
        }
    }

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_obj_affine_setup(
                machine, 0u, 24u, 24u, 0u, 1u, 0u, 0u, false,
                256, 0, 0, 256));
            ppu_obj_affine_set_4bpp_pixel(machine, 0u, 1u, 0u, 3u);
            CHECK(ppu_obj_affine_render_to_line(machine, 24u));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 24u, 24u, UINT16_C(0x7C00)));
            CHECK(ppu_bg_affine_pixel_matches(
                machine->video_frame, 25u, 24u, UINT16_C(0x0000)));
        }
        free_machine(machine);
    }

    if (CHECK_DONE() == 0) {
        puts("ppu.obj.affine: selected signed matrices, floor policy, source bounds, double-size, clipping and OBJ windows");
        return 0;
    }
    return 1;
}

static bool ppu_bg_affine_prepare_pattern(gbl_machine *machine,
                                          uint8_t mode,
                                          uint8_t background,
                                          uint8_t size_code, bool wrap,
                                          uint8_t map_base,
                                          uint8_t character_base,
                                          uint16_t color)
{
    uint8_t pixels[64];

    memset(pixels, 1, sizeof(pixels));
    return ppu_bg_affine_configure(machine, mode, background, size_code,
                                   wrap, map_base, character_base) &&
           ppu_bg_affine_set_map_entry(machine, background, 0u, 0u, 1u) &&
           ppu_bg_affine_fill_tile(machine, background, 1u, pixels) &&
           ppu_bg_text_set_color(machine, 0u, UINT16_C(0x001F)) &&
           ppu_bg_text_set_color(machine, 1u, color);
}

static int run_ppu_bg_text(void)
{
    static const uint8_t source_cases[][2] = {
        { 0u, 0u }, { 0u, 1u }, { 0u, 2u }, { 0u, 3u },
        { 1u, 0u }, { 1u, 1u }
    };
    size_t source_case;

    for (source_case = 0u;
         source_case < sizeof(source_cases) / sizeof(source_cases[0]);
         ++source_case) {
        const uint8_t mode = source_cases[source_case][0];
        const uint8_t background = source_cases[source_case][1];
        const uint8_t map_base = background == 3u ? 31u
            : (uint8_t)(background * 2u + 1u);
        const uint8_t character_base = background;
        const uint16_t color = (uint16_t)(
            UINT16_C(0x0300) + (uint16_t)source_case + UINT16_C(1));
        uint8_t pixels[64];
        gbl_machine *machine = new_machine();
        size_t pixel;

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        memset(pixels, 1, sizeof(pixels));
        CHECK(ppu_bg_text_configure(machine, mode, background, 0u, false,
                                    0u, 0u, map_base, character_base));
        CHECK(ppu_bg_text_set_color(machine, 0u, UINT16_C(0x001F)));
        CHECK(ppu_bg_text_set_color(machine, 1u, color));
        CHECK(ppu_bg_text_set_map_entry(machine, background, 0u, 0u, 1u));
        CHECK(ppu_bg_text_fill_tile(machine, background, 1u, pixels));
        CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 0u, color));
        for (pixel = 0u; pixel < 4u; ++pixel) {
            CHECK(ppu_bg_text_pixel_matches(
                machine->video_frame, (uint16_t)pixel, color));
        }
        free_machine(machine);
    }

    /* BG2 transform writes do not alter a selected mode-0 or mode-1 text
     * source, so later text rows remain capturable and retain their pixels. */
    {
        static const uint8_t text_cases[][2] = { { 0u, 0u }, { 1u, 1u } };
        size_t text_case;

        for (text_case = 0u;
             text_case < sizeof(text_cases) / sizeof(text_cases[0]);
             ++text_case) {
            const uint8_t mode = text_cases[text_case][0];
            const uint8_t background = text_cases[text_case][1];
            const uint16_t color =
                (uint16_t)(UINT16_C(0x4A00) + (uint16_t)text_case);
            uint8_t pixels[64];
            gbl_machine *text_machine;
            gbl_machine_time_preview preview;
            const gbl_time_operation writes[2] = {
                { GBL_TIME_OPERATION_BUS_WRITE, 2u,
                  UINT32_C(0x04000020), UINT32_C(0x00000100), UINT64_C(1) },
                { GBL_TIME_OPERATION_BUS_WRITE, 4u,
                  UINT32_C(0x04000028), UINT32_C(0x00010000), UINT64_C(1) }
            };

            text_machine = new_machine();
            CHECK(text_machine != NULL);
            if (text_machine == NULL) {
                continue;
            }
            memset(pixels, 1, sizeof(pixels));
            CHECK(ppu_bg_text_configure(text_machine, mode, background, 0u,
                                        false, 0u, 0u, 1u, 1u));
            CHECK(ppu_bg_text_set_color(text_machine, 1u, color));
            CHECK(ppu_bg_text_set_map_entry(text_machine, background, 0u, 0u,
                                            1u));
            CHECK(ppu_bg_text_fill_tile(text_machine, background, 1u, pixels));
            CHECK(gbl_machine_advance_to(text_machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(gbl_machine_prepare_operations(text_machine, writes, 2u,
                                                 &preview) ==
                  GBL_TIME_STATUS_OK);
            gbl_machine_commit_time(text_machine, &preview);
            CHECK(text_machine->ppu.capture_state != GBL_VIDEO_CAPTURE_INVALID);
            CHECK(gbl_machine_advance_to(text_machine,
                                         GBL_PPU_LINE_CYCLES) ==
                  GBL_TIME_STATUS_OK);
            CHECK(text_machine->ppu.capture_state !=
                  GBL_VIDEO_CAPTURE_INVALID);
            CHECK(ppu_bg_text_pixel_matches(
                text_machine->video_frame, 0u, color));
            free_machine(text_machine);
        }
    }

    {
        uint8_t size_code;

        for (size_code = 0u; size_code < 4u; ++size_code) {
            const uint16_t horizontal_scroll =
                (size_code & 1u) != 0u ? UINT16_C(256) : 0u;
            const uint16_t vertical_scroll =
                (size_code & 2u) != 0u ? UINT16_C(255) : 0u;
            const uint32_t tile_x = (uint32_t)horizontal_scroll / 8u;
            const uint32_t tile_y =
                ((uint32_t)vertical_scroll + 1u) / 8u;
            const uint16_t tile_index = (uint16_t)(size_code + 2u);
            const uint8_t color_index = (uint8_t)(size_code + 1u);
            const uint16_t color =
                (uint16_t)(UINT16_C(0x1000) + (uint16_t)size_code * 17u);
            uint8_t pixels[64];
            gbl_machine *machine = new_machine();

            CHECK(machine != NULL);
            if (machine == NULL) {
                continue;
            }
            memset(pixels, color_index, sizeof(pixels));
            CHECK(ppu_bg_text_configure(
                machine, 0u, 0u, size_code, false, horizontal_scroll,
                vertical_scroll, 0u, 0u));
            CHECK(ppu_bg_text_set_color(machine, color_index, color));
            CHECK(ppu_bg_text_set_map_entry(machine, 0u, tile_x, tile_y,
                                            tile_index));
            CHECK(ppu_bg_text_fill_tile(machine, 0u, tile_index, pixels));
            CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 0u, color));
            free_machine(machine);
        }
    }

    {
        uint8_t pixels[64];
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            memset(pixels, 1, sizeof(pixels));
            memset(&pixels[8u * 7u], 2, 8u);
            CHECK(ppu_bg_text_configure(machine, 0u, 0u, 3u, false,
                                        UINT16_C(511), UINT16_C(511),
                                        0u, 0u));
            CHECK(ppu_bg_text_set_color(machine, 1u, UINT16_C(0x0211)));
            CHECK(ppu_bg_text_set_color(machine, 2u, UINT16_C(0x0422)));
            CHECK(ppu_bg_text_set_map_entry(machine, 0u, 63u, 0u, 1u));
            CHECK(ppu_bg_text_set_map_entry(machine, 0u, 0u, 0u, 2u));
            CHECK(ppu_bg_text_fill_tile(machine, 0u, 1u, pixels));
            memset(pixels, 2, sizeof(pixels));
            CHECK(ppu_bg_text_fill_tile(machine, 0u, 2u, pixels));
            CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 0u,
                                            UINT16_C(0x0211)));
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 1u,
                                            UINT16_C(0x0422)));
            free_machine(machine);
        }
    }

    {
        uint8_t pixels[64] = { 0 };
        gbl_machine *machine = new_machine();
        uint32_t y;

        CHECK(machine != NULL);
        if (machine != NULL) {
            for (y = 0u; y < 8u; ++y) {
                uint32_t x;

                for (x = 0u; x < 8u; ++x) {
                    pixels[y * 8u + x] = y == 6u
                        ? (uint8_t)(x + 1u)
                        : (y == 1u ? UINT8_C(9) : UINT8_C(2));
                }
            }
            CHECK(ppu_bg_text_configure(machine, 0u, 0u, 0u, false,
                                        0u, 0u, 2u, 1u));
            CHECK(ppu_bg_text_set_map_entry(
                machine, 0u, 0u, 0u,
                (uint16_t)(3u | UINT16_C(0x3C00))));
            CHECK(ppu_bg_text_fill_tile(machine, 0u, 3u, pixels));
            CHECK(ppu_bg_text_set_color(machine, 56u, UINT16_C(0x0456)));
            CHECK(ppu_bg_text_set_color(machine, 49u, UINT16_C(0x0234)));
            CHECK(ppu_bg_text_set_color(machine, 57u, UINT16_C(0x0567)));
            CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 0u,
                                            UINT16_C(0x0456)));
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 7u,
                                            UINT16_C(0x0234)));
            free_machine(machine);
        }
    }

    {
        static const uint8_t row_pixels[8] = {
            0u, 1u, 16u, 255u, 2u, 3u, 4u, 5u
        };
        uint8_t pixels[64];
        gbl_machine *machine = new_machine();
        uint32_t row;

        CHECK(machine != NULL);
        if (machine != NULL) {
            for (row = 0u; row < 8u; ++row) {
                memcpy(&pixels[row * 8u], row_pixels, sizeof(row_pixels));
            }
            CHECK(ppu_bg_text_configure(machine, 0u, 0u, 0u, true,
                                        0u, 0u, 0u, 0u));
            CHECK(ppu_bg_text_set_map_entry(
                machine, 0u, 0u, 0u,
                (uint16_t)(1u | UINT16_C(0xF000))));
            CHECK(ppu_bg_text_fill_tile(machine, 0u, 1u, pixels));
            CHECK(ppu_bg_text_set_color(machine, 0u, UINT16_C(0x1234)));
            CHECK(ppu_bg_text_set_color(machine, 1u, UINT16_C(0x0001)));
            CHECK(ppu_bg_text_set_color(machine, 16u, UINT16_C(0x1010)));
            CHECK(ppu_bg_text_set_color(machine, 255u, UINT16_C(0x3E00)));
            CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 0u,
                                            UINT16_C(0x1234)));
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 1u,
                                            UINT16_C(0x0001)));
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 2u,
                                            UINT16_C(0x1010)));
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 3u,
                                            UINT16_C(0x3E00)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        gbl_machine_time_preview preview;
        const gbl_time_operation writes[4] = {
            { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x06000000),
              UINT32_C(1), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 1u, UINT32_C(0x06000024),
              UINT32_C(1), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x05000002),
              UINT32_C(0x03E0), UINT64_C(1) },
            { GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1229) }
        };

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_bg_text_configure(machine, 0u, 0u, 0u, false,
                                        0u, 0u, 0u, 0u));
            CHECK(gbl_machine_prepare_operations(machine, writes, 4u,
                                                 &preview) ==
                  GBL_TIME_STATUS_OK);
            CHECK(preview.ppu_row_capture_count == 2u);
            CHECK(preview.ppu_row_captures[1].prior_write_count == 3u);
            gbl_machine_commit_time(machine, &preview);
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 0u,
                                            UINT16_C(0x03E0)));
            free_machine(machine);
        }
    }

    puts("ppu.bg.text: mode-0/1 sources, four map sizes, wrap, flips, 4/8bpp, palette and ordered writes");
    return CHECK_DONE() == 0 ? 0 : 1;
}

static int run_ppu_bg_affine(void)
{
    const uint16_t expected = UINT16_C(0x2A35);
    const uint16_t backdrop = UINT16_C(0x001F);
    gbl_machine *machine = new_machine();
    uint8_t pixels[64];

    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_affine_prepare_pattern(machine, 1u, 2u, 0u, false,
                                            1u, 1u, expected));
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          expected));
        CHECK(!ppu_bg_affine_pixel_matches(
            machine->video_frame, 0u, 0u,
            (uint16_t)(expected ^ UINT16_C(1))));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          expected));
        free_machine(machine);
    }

    /* A caller-mutated final affine origin is refused before either machine
     * timing state or committed pixels can change. The original prepared
     * preview remains valid and can still be committed afterward. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        gbl_machine *snapshot = malloc(sizeof(*snapshot));
        gbl_machine_time_preview preview;
        gbl_machine_time_preview changed_preview;

        CHECK(snapshot != NULL);
        CHECK(ppu_bg_affine_prepare_pattern(machine, 1u, 2u, 0u, false,
                                            1u, 1u, expected));
        CHECK(gbl_machine_advance_to(machine, 0u) ==
              GBL_TIME_STATUS_OK);
        CHECK(gbl_machine_prepare_time(machine, GBL_PPU_LINE_CYCLES,
                                       &preview) == GBL_TIME_STATUS_OK);
        changed_preview = preview;
        ++changed_preview.ppu.affine[0].origin_x;
        if (snapshot != NULL) {
            *snapshot = *machine;
            gbl_machine_commit_time(machine, &changed_preview);
            CHECK(gbl_test_canonical_equal(machine, snapshot));
            CHECK(memcmp(machine->video_frame, snapshot->video_frame,
                         GBL_PPU_VIDEO_BYTES) == 0);
            gbl_machine_commit_time(machine, &preview);
            CHECK(machine->master_cycles == GBL_PPU_LINE_CYCLES);
            CHECK(machine->ppu.line == 1u);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                              expected));
            free(snapshot);
        }
        free_machine(machine);
    }

    /* A vertical affine step can change the scanline within one tile while
     * source X remains in that tile. Each output pixel must use its own
     * source row rather than reusing the prior cached tile row. */
    {
        static const uint16_t row_colors[8] = {
            UINT16_C(0x001F), UINT16_C(0x03E0), UINT16_C(0x7C00),
            UINT16_C(0x4210), UINT16_C(0x2108), UINT16_C(0x1234),
            UINT16_C(0x4567), UINT16_C(0x2ABC)
        };
        uint8_t row_pixels[64];
        uint32_t y;
        uint32_t x;

        machine = new_machine();
        CHECK(machine != NULL);
        if (machine != NULL) {
            for (y = 0u; y < 8u; ++y) {
                for (x = 0u; x < 8u; ++x) {
                    row_pixels[y * 8u + x] = (uint8_t)(y + 1u);
                }
                CHECK(ppu_bg_text_set_color(machine, y + 1u,
                                            row_colors[y]));
            }
            CHECK(ppu_bg_affine_configure(machine, 1u, 2u, 0u, false,
                                          0u, 1u));
            CHECK(ppu_bg_affine_set_map_entry(machine, 2u, 0u, 0u, 1u));
            CHECK(ppu_bg_affine_fill_tile(machine, 2u, 1u, row_pixels));
            ppu_bg_affine_set_transform(machine, 2u, 0, 0,
                                        INT16_C(0x0100),
                                        INT16_C(0x0100), 0, 0);
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_OK);
            for (x = 0u; x < 8u; ++x) {
                CHECK(ppu_bg_affine_pixel_matches(
                    machine->video_frame, (uint16_t)x, 0u, row_colors[x]));
            }
            free_machine(machine);
        }
    }

    /* Mathematical floor keeps negative fractional coordinates outside an
     * unclipped map. Overflow wrap then maps the same coordinate to the last
     * source pixel. */
    {
        gbl_machine *clipped = new_machine();
        gbl_machine *wrapped = new_machine();

        CHECK(clipped != NULL && wrapped != NULL);
        if (clipped != NULL && wrapped != NULL) {
            CHECK(ppu_bg_affine_prepare_pattern(
                clipped, 1u, 2u, 0u, false, 1u, 1u, expected));
            CHECK(ppu_bg_affine_prepare_pattern(
                wrapped, 1u, 2u, 0u, true, 1u, 1u, expected));
            CHECK(ppu_bg_affine_set_map_entry(wrapped, 2u, 127u, 0u, 1u));
            ppu_bg_affine_set_transform(clipped, 2u, INT16_C(0x0100), 0,
                                        0, INT16_C(0x0100), -128, 0);
            ppu_bg_affine_set_transform(wrapped, 2u, INT16_C(0x0100), 0,
                                        0, INT16_C(0x0100), -128, 0);
            CHECK(gbl_machine_advance_to(clipped, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(gbl_machine_advance_to(wrapped, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_affine_pixel_matches(clipped->video_frame, 0u, 0u,
                                              backdrop));
            CHECK(ppu_bg_affine_pixel_matches(clipped->video_frame, 1u, 0u,
                                              expected));
            CHECK(ppu_bg_affine_pixel_matches(wrapped->video_frame, 0u, 0u,
                                              expected));
            CHECK(ppu_bg_affine_pixel_matches(wrapped->video_frame, 1u, 0u,
                                              expected));
        }
        free_machine(clipped);
        free_machine(wrapped);
    }

    /* Mode 2 exposes BG3's independent affine matrix and byte map. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_affine_prepare_pattern(machine, 2u, 3u, 0u, false,
                                            2u, 2u, UINT16_C(0x1555)));
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          UINT16_C(0x1555)));
        free_machine(machine);
    }

    /* The largest affine map and its last 8bpp tile remain inside BG VRAM.
     * Signed matrix extrema exercise the int64 horizontal products. */
    machine = new_machine();
    memset(pixels, 1, sizeof(pixels));
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_affine_configure(machine, 2u, 2u, 3u, false, 8u, 0u));
        CHECK(ppu_bg_affine_set_map_entry(machine, 2u, 1023u, 1023u,
                                          UINT8_MAX));
        CHECK(ppu_bg_affine_fill_tile(machine, 2u, UINT8_MAX, pixels));
        CHECK(ppu_bg_text_set_color(machine, 0u, backdrop));
        CHECK(ppu_bg_text_set_color(machine, 1u, expected));
        ppu_bg_affine_set_transform(
            machine, 2u, INT16_MIN, 0, INT16_MAX, 0,
            (int32_t)(1023u * 256u), (int32_t)(1023u * 256u));
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          expected));
        free_machine(machine);
    }

    /* A map address beyond the physical BG VRAM window refuses the preview
     * without changing any canonical machine state. */
    machine = new_machine();
    if (machine != NULL) {
        gbl_machine *snapshot = malloc(sizeof(*snapshot));

        CHECK(snapshot != NULL);
        CHECK(ppu_bg_affine_configure(machine, 2u, 2u, 3u, false, 31u, 0u));
        ppu_bg_affine_set_transform(
            machine, 2u, INT16_C(0x0100), 0, 0, INT16_C(0x0100),
            (int32_t)(1023u * 256u), (int32_t)(1023u * 256u));
        if (snapshot != NULL) {
            memcpy(snapshot, machine, sizeof(*snapshot));
            CHECK(gbl_machine_advance_to(machine, 0u) ==
                  GBL_TIME_STATUS_INVALID_ARGUMENT);
            CHECK(gbl_test_canonical_equal(machine, snapshot));
            free(snapshot);
        }
        free_machine(machine);
    }

    /* Affine origins are 28-bit modular values and partial, halfword, and
     * word writes update them transactionally. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        gbl_machine_time_preview preview;
        gbl_machine_time_preview sentinel;
        gbl_machine *snapshot = malloc(sizeof(*snapshot));
        const gbl_time_operation write_then_overflow[2] = {
            { GBL_TIME_OPERATION_BUS_WRITE, 4u,
              UINT32_C(0x04000028), UINT32_C(0x00010000), UINT64_C(1) },
            { GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_MAX }
        };
        const gbl_time_operation write_word = {
            GBL_TIME_OPERATION_BUS_WRITE, 4u, UINT32_C(0x04000028),
            UINT32_C(0xF1234567), UINT64_C(1)
        };
        gbl_bus_result result;

        CHECK(ppu_bg_affine_prepare_pattern(machine, 1u, 2u, 0u, false,
                                            1u, 1u, expected));
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(gbl_machine_prepare_operations(machine, &write_word, 1u,
                                             &preview) ==
              GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.affine[0].programmed_reference_x == 0u);
        CHECK(preview.ppu.affine[0].programmed_reference_x ==
              UINT32_C(0x01234567));
        CHECK(preview.ppu.affine[0].origin_x == UINT32_C(0x01234567));
        gbl_machine_commit_time(machine, &preview);
        CHECK(machine->ppu.affine[0].programmed_reference_x ==
              UINT32_C(0x01234567));
        result = gbl_bus_write(machine, UINT32_C(0x04000028), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x80));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(machine->ppu.affine[0].programmed_reference_x ==
              UINT32_C(0x01234580));
        result = gbl_bus_write(machine, UINT32_C(0x0400002A), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0xF080));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(machine->ppu.affine[0].programmed_reference_x ==
              UINT32_C(0x00804580));
        result = gbl_bus_write(machine, UINT32_C(0x0400002C), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0xABCD));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(machine->ppu.affine[0].programmed_reference_y ==
              UINT32_C(0x0000ABCD));
        CHECK(snapshot != NULL);
        if (snapshot != NULL) {
            memcpy(snapshot, machine, sizeof(*snapshot));
            memset(&preview, 0xA5, sizeof(preview));
            memcpy(&sentinel, &preview, sizeof(sentinel));
            CHECK(gbl_machine_prepare_operations(
                      machine, write_then_overflow, 2u, &preview) ==
                  GBL_TIME_STATUS_CYCLE_OVERFLOW);
            CHECK(memcmp(&preview, &sentinel, sizeof(preview)) == 0);
            CHECK(gbl_test_canonical_equal(machine, snapshot));
            free(snapshot);
        }
        free_machine(machine);
    }

    /* The +1006 HBlank event advances PB/PD once. A BGxY write after that
     * event replaces the vertical origin for the next row and records the
     * modeled HBlank exception; the following HBlank advances normally. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        gbl_machine_time_preview preview;
        gbl_bus_result result;

        CHECK(ppu_bg_affine_prepare_pattern(machine, 1u, 2u, 0u, false,
                                            1u, 1u, expected));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100),
                                    INT16_C(0x0100), 0,
                                    -INT16_C(0x0100), INT32_C(0x10000),
                                    INT32_C(0x20000));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_HBLANK_CYCLES) ==
              GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.affine[0].origin_x == UINT32_C(0x10100));
        CHECK(machine->ppu.affine[0].origin_y == UINT32_C(0x1FF00));
        result = gbl_bus_write(machine, UINT32_C(0x0400002C), 4u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x00030000));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(machine->ppu.affine[0].origin_y == UINT32_C(0x30000));
        CHECK(machine->ppu.affine[0].hblank_y_suppressed);
        CHECK(gbl_machine_prepare_time(machine, GBL_PPU_LINE_CYCLES,
                                       &preview) == GBL_TIME_STATUS_OK);
        CHECK(preview.ppu_row_capture_count == 1u);
        CHECK(preview.ppu_row_captures[0].line == 1u);
        CHECK(preview.ppu_row_captures[0].affine[0].origin_x ==
              UINT32_C(0x10100));
        CHECK(preview.ppu_row_captures[0].affine[0].origin_y ==
              UINT32_C(0x30000));
        CHECK(preview.ppu_row_captures[0].affine[0].hblank_y_suppressed);
        gbl_machine_commit_time(machine, &preview);
        CHECK(!machine->ppu.affine[0].hblank_y_suppressed);
        CHECK(gbl_machine_prepare_time(
                  machine, GBL_PPU_LINE_CYCLES * UINT64_C(2), &preview) ==
              GBL_TIME_STATUS_OK);
        CHECK(preview.ppu_row_capture_count == 1u);
        CHECK(preview.ppu_row_captures[0].line == 2u);
        CHECK(preview.ppu_row_captures[0].affine[0].origin_x ==
              UINT32_C(0x10200));
        CHECK(preview.ppu_row_captures[0].affine[0].origin_y ==
              UINT32_C(0x2FF00));
        gbl_machine_commit_time(machine, &preview);
        free_machine(machine);
    }

    /* Frame transition reloads the programmed affine origin after VBlank. */
    {
        static const uint64_t phase_cycles[] = {
            GBL_PPU_LINE_CYCLES - UINT64_C(2),
            GBL_PPU_LINE_CYCLES - UINT64_C(1),
            GBL_PPU_LINE_CYCLES
        };
        size_t phase;

        for (phase = 0u; phase <
             sizeof(phase_cycles) / sizeof(phase_cycles[0]); ++phase) {
            gbl_machine_time_preview preview;
            gbl_machine *timed = new_machine();
            const gbl_time_operation write_ref = {
                GBL_TIME_OPERATION_BUS_WRITE, 4u,
                UINT32_C(0x04000028), UINT32_C(0x00010000), UINT64_C(1)
            };
            const bool at_line_start = phase == 1u;

            CHECK(timed != NULL);
            if (timed == NULL) {
                continue;
            }
            CHECK(ppu_bg_affine_prepare_pattern(
                timed, 1u, 2u, 0u, false, 1u, 1u, expected));
            ppu_bg_affine_set_transform(timed, 2u, INT16_C(0x0100),
                                        INT16_C(0x0100), 0,
                                        INT16_C(0x0100), 0, 0);
            CHECK(gbl_machine_advance_to(timed, phase_cycles[phase]) ==
                  GBL_TIME_STATUS_OK);
            CHECK(gbl_machine_prepare_operations(timed, &write_ref, 1u,
                                                 &preview) ==
                  GBL_TIME_STATUS_OK);
            if (at_line_start) {
                CHECK(preview.ppu_row_capture_count == 1u);
                CHECK(preview.ppu_row_captures[0].line == 1u);
                CHECK(preview.ppu_row_captures[0].affine[0].origin_x ==
                      UINT32_C(0x00000100));
            } else {
                CHECK(preview.ppu_row_capture_count == 0u);
            }
            CHECK(preview.ppu.affine[0].programmed_reference_x ==
                  UINT32_C(0x00010000));
            CHECK(preview.ppu.affine[0].origin_x == UINT32_C(0x00010000));
            gbl_machine_commit_time(timed, &preview);
            if (phase == 0u) {
                CHECK(gbl_machine_prepare_time(
                          timed, GBL_PPU_LINE_CYCLES, &preview) ==
                      GBL_TIME_STATUS_OK);
                CHECK(preview.ppu_row_capture_count == 1u);
                CHECK(preview.ppu_row_captures[0].line == 1u);
                CHECK(preview.ppu_row_captures[0].affine[0].origin_x ==
                      UINT32_C(0x00010000));
                gbl_machine_commit_time(timed, &preview);
            }
            CHECK(gbl_machine_prepare_time(
                      timed, GBL_PPU_LINE_CYCLES * UINT64_C(2), &preview) ==
                  GBL_TIME_STATUS_OK);
            CHECK(preview.ppu_row_capture_count == 1u);
            CHECK(preview.ppu_row_captures[0].line == 2u);
            CHECK(preview.ppu_row_captures[0].affine[0].origin_x ==
                  UINT32_C(0x00010100));
            gbl_machine_commit_time(timed, &preview);
            free_machine(timed);
        }
    }

    /* Frame transition reloads the programmed affine origin after VBlank. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_affine_prepare_pattern(machine, 1u, 2u, 0u, false,
                                            1u, 1u, expected));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100),
                                    INT16_C(0x0100), 0,
                                    -INT16_C(0x0100), INT32_C(0x10000),
                                    INT32_C(0x20000));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_FRAME_CYCLES) ==
              GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.frame_epoch == 1u);
        CHECK(machine->ppu.affine[0].origin_x ==
              machine->ppu.affine[0].programmed_reference_x);
        CHECK(machine->ppu.affine[0].origin_y ==
              machine->ppu.affine[0].programmed_reference_y);
        CHECK(!machine->ppu.affine[0].hblank_y_suppressed);
        free_machine(machine);
    }

    /* A legal mode-1 text BG can coexist with affine BG2. The text layer is
     * transparent at this pixel, so the configured affine sample stays visible. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_affine_prepare_pattern(machine, 1u, 2u, 0u, false,
                                            1u, 1u, expected));
        machine->dispcnt |= UINT16_C(0x0100);
        machine->ppu.dispcnt = machine->dispcnt;
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_INCOMPLETE);
        CHECK(machine->ppu.capture_reason ==
              GBL_VIDEO_INVALID_REASON_INCOMPLETE);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          expected));
        free_machine(machine);
    }

    /* Mode-2 affine BG3 is independently projected beside BG2; its default
     * zero-index tile is transparent and cannot displace the configured BG2. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_affine_prepare_pattern(machine, 2u, 2u, 0u, false,
                                            1u, 1u, expected));
        machine->dispcnt |= UINT16_C(0x0800);
        machine->ppu.dispcnt = machine->dispcnt;
        ppu_bg_affine_set_transform(machine, 3u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100), 0, 0);
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_INCOMPLETE);
        CHECK(machine->ppu.capture_reason ==
              GBL_VIDEO_INVALID_REASON_INCOMPLETE);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          expected));
        free_machine(machine);
    }

    puts("ppu.bg.affine: mode-1/2 sources, signed fixed point, 28-bit wrap, map bounds, ordered references and VBlank reload");
    return CHECK_DONE() == 0 ? 0 : 1;
}

static int run_ppu_bg_bitmap(void)
{
    const uint16_t direct_color = UINT16_C(0x2A35);
    gbl_machine *machine;
    uint8_t wrong_row[GBL_PPU_ROW_BYTES];

    /* The existing mode-3 identity source remains the semantic anchor. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_bitmap_configure(machine, 3u, true));
        ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x001F));
        machine->vram[0] = (uint8_t)(direct_color & UINT16_C(0x00FF));
        machine->vram[1] = (uint8_t)(direct_color >> 8u);
        ppu_bg_store_u16(machine->vram, UINT32_C(0xA000),
                         UINT16_C(0x4567));
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          direct_color));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          UINT16_C(0x0000)));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0, 0,
                                    INT16_C(0x0100), -INT32_C(0x80),
                                    -INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          UINT16_C(0x001F)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 1u, 1u,
                                          direct_color));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 2u, 1u,
                                          UINT16_C(0x0000)));
        ppu_bg_store_u16(machine->vram, UINT32_C(0x12BFE),
                         UINT16_C(0x4567));
        ppu_bg_store_u16(machine->vram, UINT32_C(0x12C00),
                         UINT16_C(0x7C00));
        ppu_bg_affine_set_transform(
            machine, 2u, INT16_C(0x0100), 0, 0, INT16_C(0x0100),
            INT32_C(239) * INT32_C(0x0100),
            INT32_C(158) * INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES *
                                             UINT64_C(2)) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 2u,
                                          UINT16_C(0x4567)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 1u, 2u,
                                          UINT16_C(0x001F)));
        ppu_bg_affine_set_transform(
            machine, 2u, INT16_C(0x0100), 0, 0, INT16_C(0x0100), 0,
            INT32_C(159) * INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES *
                                             UINT64_C(3)) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 3u,
                                          UINT16_C(0x001F)));
        free_machine(machine);
    }

    /* Extreme signed references and coefficients remain clipped before any
     * source offset can wrap into an in-range neighbor. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_bitmap_configure(machine, 3u, false));
        ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x001F));
        ppu_bg_affine_set_transform(machine, 2u, INT16_MAX, 0, INT16_MIN,
                                    INT16_MAX, INT32_MIN, INT32_MAX);
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          UINT16_C(0x001F)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 239u, 0u,
                                          UINT16_C(0x001F)));
        free_machine(machine);
    }

    /* Mode 4 resolves an indexed BG2 sample through palette entry one. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_bitmap_configure(machine, 4u, false));
        machine->vram[0] = 1u;
        machine->vram[1] = 0u;
        ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x001F));
        ppu_bg_store_u16(machine->pram, 2u, UINT16_C(0x1256));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100), 0, 0);
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          UINT16_C(0x1256)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 1u, 0u,
                                          UINT16_C(0x001F)));
        memcpy(wrong_row, machine->video_frame, sizeof(wrong_row));
        wrong_row[0] ^= UINT8_C(1);
        CHECK(!ppu_bg_affine_pixel_matches(wrong_row, 0u, 0u,
                                           UINT16_C(0x1256)));
        free_machine(machine);
    }

    /* The mode-4 last valid byte is sampled, while X/Y just beyond the
     * declared page extent resolve to backdrop even when VRAM has sentinels. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_bitmap_configure(machine, 4u, true));
        ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x001F));
        ppu_bg_store_u16(machine->pram, 2u, UINT16_C(0x1234));
        ppu_bg_store_u16(machine->pram, 4u, UINT16_C(0x7C00));
        machine->vram[UINT32_C(0xA000)] = 1u;
        machine->vram[UINT32_C(0x135FF)] = 1u;
        machine->vram[UINT32_C(0x13600)] = 2u;
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100),
                                    INT32_C(239) * INT32_C(0x0100),
                                    INT32_C(159) * INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          UINT16_C(0x1234)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 1u, 0u,
                                          UINT16_C(0x001F)));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100), 0,
                                    INT32_C(160) * INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          UINT16_C(0x001F)));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100), -INT32_C(0x80),
                                    -INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES *
                                             UINT64_C(2)) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 2u,
                                          UINT16_C(0x001F)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 1u, 2u,
                                          UINT16_C(0x1234)));
        free_machine(machine);
    }

    /* Mode 5 selects its second 160x128 direct-color page at 0xA000. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_bitmap_configure(machine, 5u, true));
        ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x001F));
        machine->vram[UINT32_C(0xA000)] =
            (uint8_t)(direct_color & UINT16_C(0x00FF));
        machine->vram[UINT32_C(0xA001)] =
            (uint8_t)(direct_color >> 8u);
        ppu_bg_store_u16(machine->vram, 0u, UINT16_C(0x4567));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100), 0, 0);
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          direct_color));
        CHECK(ppu_bg_bitmap_configure(machine, 5u, false));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, 0, 0, 0);
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          UINT16_C(0x4567)));
        free_machine(machine);
    }

    /* Mode 5 clips at 160x128 without consuming its unused page tail. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_bitmap_configure(machine, 5u, true));
        ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x001F));
        ppu_bg_store_u16(machine->vram, UINT32_C(0x13FFE),
                         UINT16_C(0x1234));
        ppu_bg_store_u16(machine->vram, UINT32_C(0x14000),
                         UINT16_C(0x7C00));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100),
                                    INT32_C(159) * INT32_C(0x0100),
                                    INT32_C(127) * INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          UINT16_C(0x1234)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 1u, 0u,
                                          UINT16_C(0x001F)));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100), 0,
                                    INT32_C(128) * INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          UINT16_C(0x001F)));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100), 0, 0);
        machine->vram[UINT32_C(0xA000)] = 0u;
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES *
                                             UINT64_C(2)) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 2u,
                                          UINT16_C(0x0000)));
        ppu_bg_store_u16(machine->vram, UINT32_C(0xA000),
                         UINT16_C(0x2345));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100), -INT32_C(0x80),
                                    -INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES *
                                             UINT64_C(3)) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 3u,
                                          UINT16_C(0x001F)));
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 1u, 3u,
                                          UINT16_C(0x2345)));
        free_machine(machine);
    }

    /* The bitmap source and palette are sampled from the row-start write
     * prefix: the pre-boundary palette change is visible, while writes on and
     * after the boundary cannot alter this row. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        gbl_bus_result result;

        CHECK(ppu_bg_bitmap_configure(machine, 4u, false));
        machine->vram[0] = 1u;
        ppu_bg_store_u16(machine->pram, 0u, UINT16_C(0x001F));
        ppu_bg_store_u16(machine->pram, 2u, UINT16_C(0x001F));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, INT16_C(0x0100), 0,
                                    -INT32_C(0x0100));
        CHECK(gbl_machine_advance_to(machine, UINT64_C(1230)) ==
              GBL_TIME_STATUS_OK);
        result = gbl_bus_write(machine, UINT32_C(0x05000002), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x03E0));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        result = gbl_bus_write(machine, UINT32_C(0x06000000), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x0002));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          UINT16_C(0x03E0)));
        result = gbl_bus_write(machine, UINT32_C(0x05000002), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x7C00));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          UINT16_C(0x03E0)));
        CHECK(machine->vram[0] == 2u);
        CHECK(machine->pram[2] == UINT8_C(0x00));
        CHECK(machine->pram[3] == UINT8_C(0x7C));
        free_machine(machine);
    }

    /* DISPCNT page selection is captured at the same row boundary as
     * canonical video memory. The before-boundary page is used by row 1; a
     * same-cycle page write is deferred, and the after-boundary write affects
     * row 2. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        gbl_bus_result result;

        CHECK(ppu_bg_bitmap_configure(machine, 4u, false));
        machine->vram[0] = 1u;
        machine->vram[UINT32_C(0xA000)] = 2u;
        ppu_bg_store_u16(machine->pram, 2u, UINT16_C(0x001F));
        ppu_bg_store_u16(machine->pram, 4u, UINT16_C(0x03E0));
        ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                    0, 0, 0, 0);
        CHECK(gbl_machine_advance_to(machine, UINT64_C(1230)) ==
              GBL_TIME_STATUS_OK);
        result = gbl_bus_write(machine, UINT32_C(0x04000000), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x0414));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        result = gbl_bus_write(machine, UINT32_C(0x04000000), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x0404));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          UINT16_C(0x03E0)));
        result = gbl_bus_write(machine, UINT32_C(0x04000000), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x0414));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(gbl_machine_advance_to(machine, GBL_PPU_LINE_CYCLES *
                                             UINT64_C(2)) ==
              GBL_TIME_STATUS_OK);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 2u,
                                          UINT16_C(0x03E0)));
        free_machine(machine);
    }

    /* Mosaic remains an unsupported source control after bitmap admission. */
    machine = new_machine();
    CHECK(machine != NULL);
    if (machine != NULL) {
        CHECK(ppu_bg_bitmap_configure(machine, 4u, false));
        ppu_bg_store_u16(machine->ppu.bg_register_bytes, 4u,
                         UINT16_C(0x0040));
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID);
        CHECK(machine->ppu.capture_reason ==
              GBL_VIDEO_INVALID_REASON_UNSUPPORTED_MODE);
        free_machine(machine);
    }

    puts("ppu.bg.bitmap: mode-3 identity/clip, mode-4 indexed pages/prefix, mode-5 direct extent, signed bounds and zero policies");
    return CHECK_DONE() == 0 ? 0 : 1;
}

static int run_ppu_bg_controls(void)
{
    gbl_machine *clean = new_machine();
    uint8_t *wrong_frame = malloc(GBL_PPU_VIDEO_BYTES);
    uint8_t pixels[64];
    size_t case_index;
    static const struct {
        uint8_t mode;
        uint8_t background;
        uint16_t extra_enabled;
        uint16_t bg_control_bits;
    } refused[] = {
        { 1u, 2u, 0u, UINT16_C(0x0010) },
        { 0u, 0u, 0u, UINT16_C(0x0010) }
    };

    CHECK(clean != NULL && wrong_frame != NULL);
    if (clean == NULL || wrong_frame == NULL) {
        free(wrong_frame);
        free_machine(clean);
        return 1;
    }
    memset(pixels, 1, sizeof(pixels));
    CHECK(ppu_bg_text_configure(clean, 0u, 0u, 0u, false, 0u, 0u, 0u, 0u));
    CHECK(ppu_bg_text_set_map_entry(clean, 0u, 0u, 0u, 1u));
    CHECK(ppu_bg_text_fill_tile(clean, 0u, 1u, pixels));
    CHECK(ppu_bg_text_set_color(clean, 1u, UINT16_C(0x0432)));
    CHECK(gbl_machine_advance_to(clean, UINT64_C(1232)) ==
          GBL_TIME_STATUS_OK);
    CHECK(ppu_bg_text_pixel_matches(clean->video_frame, 0u,
                                    UINT16_C(0x0432)));
    memcpy(wrong_frame, clean->video_frame, GBL_PPU_VIDEO_BYTES);
    wrong_frame[GBL_PPU_ROW_BYTES] ^= UINT8_C(1);
    CHECK(!ppu_bg_text_pixel_matches(wrong_frame, 0u, UINT16_C(0x0432)));
    puts("ppu.bg.controls control: wrong expected pixel is rejected; clean semantic companion passes");

    {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_composition_prepare_text_scene(
                machine, 2u, 1u, 3u, 0u, true, false));
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 8u,
                                              UINT16_C(0x03E0)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        uint8_t text_pixels[64];
        uint8_t affine_pixels[64];
        uint16_t dispcnt;

        CHECK(machine != NULL);
        if (machine != NULL) {
            memset(text_pixels, 1, sizeof(text_pixels));
            memset(affine_pixels, 2, sizeof(affine_pixels));
            CHECK(ppu_bg_text_configure(machine, 1u, 0u, 0u, false,
                                        0u, 0u, 1u, 0u));
            CHECK(ppu_bg_text_set_map_entry(machine, 0u, 0u, 0u, 1u));
            CHECK(ppu_bg_text_fill_tile(machine, 0u, 1u, text_pixels));
            CHECK(ppu_bg_text_set_color(machine, 1u, UINT16_C(0x001F)));
            CHECK(ppu_bg_affine_configure(machine, 1u, 2u, 0u, false,
                                          2u, 1u));
            CHECK(ppu_bg_affine_set_map_entry(machine, 2u, 0u, 0u, 1u));
            CHECK(ppu_bg_affine_fill_tile(machine, 2u, 1u, affine_pixels));
            CHECK(ppu_bg_text_set_color(machine, 2u, UINT16_C(0x03E0)));
            ppu_bg_affine_set_transform(machine, 2u, INT16_C(0x0100), 0,
                                        0, INT16_C(0x0100), 0, 0);
            dispcnt = (uint16_t)(machine->dispcnt | UINT16_C(0x0100));
            machine->dispcnt = dispcnt;
            machine->ppu.dispcnt = dispcnt;
            ppu_bg_store_u16(machine->ppu.bg_register_bytes, 0u,
                             UINT16_C(0x0001));
            ppu_bg_store_u16(machine->ppu.bg_register_bytes, 4u,
                             (uint16_t)(ppu_bg_text_control(machine, 2u) &
                                        UINT16_C(0xFFFC)));
            CHECK(gbl_machine_advance_to(
                      machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                                   UINT64_C(160)) == GBL_TIME_STATUS_OK);
            CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
            CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                              UINT16_C(0x03E0)));
            free_machine(machine);
        }
    }

    for (case_index = 0u;
         case_index < sizeof(refused) / sizeof(refused[0]); ++case_index) {
        gbl_machine *machine = new_machine();
        uint16_t dispcnt;

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        CHECK(ppu_bg_text_configure(machine, refused[case_index].mode,
                                    refused[case_index].background, 0u, false,
                                    0u, 0u, 0u, 0u));
        dispcnt = (uint16_t)(machine->dispcnt |
                             refused[case_index].extra_enabled);
        machine->dispcnt = dispcnt;
        machine->ppu.dispcnt = dispcnt;
        ppu_bg_store_u16(
            machine->ppu.bg_register_bytes,
            (uint32_t)refused[case_index].background * 2u,
            (uint16_t)(ppu_bg_text_control(machine,
                                           refused[case_index].background) |
                       refused[case_index].bg_control_bits));
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID);
        CHECK(machine->ppu.capture_reason ==
              GBL_VIDEO_INVALID_REASON_UNSUPPORTED_MODE);
        CHECK(machine->ppu.captured_rows == 0u);
        free_machine(machine);
    }

    {
        gbl_machine *machine = new_machine();
        uint8_t edge_pixels[64];

        CHECK(machine != NULL);
        if (machine != NULL) {
            memset(edge_pixels, 1, sizeof(edge_pixels));
            CHECK(ppu_bg_text_configure(machine, 0u, 0u, 0u, false,
                                        UINT16_C(248), UINT16_C(254),
                                        0u, 3u));
            CHECK(ppu_bg_text_set_map_entry(machine, 0u, 31u, 31u,
                                            UINT16_C(511)));
            CHECK(ppu_bg_text_fill_tile(machine, 0u, 511u, edge_pixels));
            CHECK(ppu_bg_text_set_color(machine, 1u, UINT16_C(0x07E0)));
            CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 0u,
                                            UINT16_C(0x07E0)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        uint8_t edge_pixels[64];

        CHECK(machine != NULL);
        if (machine != NULL) {
            memset(edge_pixels, 1, sizeof(edge_pixels));
            CHECK(ppu_bg_text_configure(machine, 0u, 0u, 0u, false,
                                        UINT16_C(248), UINT16_C(254),
                                        31u, 0u));
            CHECK(ppu_bg_text_set_map_entry(machine, 0u, 31u, 31u, 1u));
            CHECK(ppu_bg_text_fill_tile(machine, 0u, 1u, edge_pixels));
            CHECK(ppu_bg_text_set_color(machine, 1u, UINT16_C(0x001F)));
            CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
                  GBL_TIME_STATUS_OK);
            CHECK(ppu_bg_text_pixel_matches(machine->video_frame, 0u,
                                            UINT16_C(0x001F)));
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        gbl_machine *before;

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_bg_text_configure(machine, 0u, 0u, 0u, false,
                                        UINT16_C(248), UINT16_C(254),
                                        31u, 3u));
            CHECK(ppu_bg_text_set_map_entry(machine, 0u, 31u, 31u,
                                            UINT16_C(1023)));
            before = malloc(sizeof(*before));
            CHECK(before != NULL);
            if (before != NULL) {
                *before = *machine;
                CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
                      GBL_TIME_STATUS_INVALID_ARGUMENT);
                CHECK(gbl_test_canonical_equal(machine, before));
                CHECK(memcmp(machine->video_frame, before->video_frame,
                             GBL_PPU_VIDEO_BYTES) == 0);
                free(before);
            }
            free_machine(machine);
        }
    }

    {
        gbl_machine *machine = new_machine();
        gbl_machine *before;

        CHECK(machine != NULL);
        if (machine != NULL) {
            CHECK(ppu_bg_text_configure(machine, 0u, 0u, 1u, false,
                                        UINT16_C(256), 0u, 31u, 0u));
            before = malloc(sizeof(*before));
            CHECK(before != NULL);
            if (before != NULL) {
                *before = *machine;
                CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
                      GBL_TIME_STATUS_INVALID_ARGUMENT);
                CHECK(gbl_test_canonical_equal(machine, before));
                CHECK(memcmp(machine->video_frame, before->video_frame,
                             GBL_PPU_VIDEO_BYTES) == 0);
                free(before);
            }
            free_machine(machine);
        }
    }

    free_machine(clean);
    free(wrong_frame);
    puts("ppu.bg.controls: admission, exact final map/tile spans and crossing-span refusals preserve output state");
    return CHECK_DONE() == 0 ? 0 : 1;
}

static bool prime_ppu_refusal_machine(gbl_machine *machine)
{
    machine->dispcnt = UINT16_C(0x0403);
    machine->ppu.dispcnt = UINT16_C(0x0403);
    machine->vram[GBL_PPU_ROW_BYTES] = UINT8_C(0x34);
    machine->vram[GBL_PPU_ROW_BYTES + 1u] = UINT8_C(0x12);
    return gbl_machine_advance_to(machine, UINT64_C(1231)) ==
           GBL_TIME_STATUS_OK;
}

static bool ppu_prepare_refusal_is_atomic(
    gbl_machine *machine, const gbl_time_operation *operations,
    size_t operation_count, bool prepare_time, uint64_t target,
    gbl_time_status expected)
{
    gbl_machine *before = malloc(sizeof(*before));
    gbl_machine_time_preview preview;
    gbl_machine_time_preview sentinel;
    gbl_time_status status;
    bool valid;

    if (before == NULL) {
        return false;
    }
    *before = *machine;
    memset(&preview, 0xA5, sizeof(preview));
    memcpy(&sentinel, &preview, sizeof(sentinel));
    status = prepare_time
        ? gbl_machine_prepare_time(machine, target, &preview)
        : gbl_machine_prepare_operations(machine, operations,
                                         operation_count, &preview);
    valid = status == expected &&
            memcmp(&preview, &sentinel, sizeof(preview)) == 0 &&
            gbl_test_canonical_equal(before, machine) &&
            gbl_test_same_instance_identity_equal(before, machine);
    free(before);
    return valid;
}

static int run_ppu_refusal(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *clean_twin = new_machine();
    gbl_machine *changed = malloc(sizeof(*changed));
    gbl_machine *before = malloc(sizeof(*before));
    gbl_machine_time_preview preview = { 0 };
    gbl_machine_time_preview repeated;
    gbl_machine_time_preview changed_preview;
    const gbl_time_operation cross_line = {
        GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1)
    };
    bool equality_detects_ppu;
    bool preview_equality_detects_plan = false;

    if (machine == NULL || clean_twin == NULL || changed == NULL ||
        before == NULL) {
        fprintf(stderr, "ppu.refusal: host allocation failed\n");
        free(before);
        free(changed);
        free_machine(clean_twin);
        free_machine(machine);
        return 1;
    }
    CHECK(prime_ppu_refusal_machine(machine));
    equality_detects_ppu = true;
#define CHECK_PPU_MUTATION(change)                                          \
    do {                                                                     \
        memcpy(changed, machine, sizeof(*changed));                          \
        change;                                                              \
        if (gbl_test_canonical_equal(machine, changed) ||                    \
            gbl_test_canonical_digest(machine) ==                            \
                gbl_test_canonical_digest(changed)) {                        \
            equality_detects_ppu = false;                                    \
        }                                                                    \
    } while (0)
    CHECK_PPU_MUTATION(changed->ppu.line_origin++);
    CHECK_PPU_MUTATION(changed->ppu.generation++);
    CHECK_PPU_MUTATION(changed->ppu.frame_epoch++);
    CHECK_PPU_MUTATION(changed->ppu.line++);
    CHECK_PPU_MUTATION(changed->ppu.scheduled_line++);
    CHECK_PPU_MUTATION(changed->ppu.dispcnt ^= UINT16_C(1));
    CHECK_PPU_MUTATION(changed->ppu.dispstat ^= UINT16_C(1));
    CHECK_PPU_MUTATION(changed->ppu.compare++);
    CHECK_PPU_MUTATION(changed->ppu.captured_rows++);
    CHECK_PPU_MUTATION(changed->ppu.unsupported_register_bytes ^=
                       UINT32_C(1));
    CHECK_PPU_MUTATION(changed->ppu.render_register_bytes[0] ^=
                       UINT8_C(1));
    CHECK_PPU_MUTATION(changed->ppu.bg_register_bytes[0] ^= UINT8_C(1));
    CHECK_PPU_MUTATION(changed->ppu.affine[0].programmed_reference_x++);
    CHECK_PPU_MUTATION(changed->ppu.affine[1].pd++);
    CHECK_PPU_MUTATION(changed->ppu.capture_state =
                           GBL_VIDEO_CAPTURE_VALID);
    CHECK_PPU_MUTATION(changed->ppu.capture_reason =
                           GBL_VIDEO_INVALID_REASON_NONE);
    CHECK_PPU_MUTATION(changed->ppu.next_boundary_hblank = true);
    CHECK_PPU_MUTATION(changed->ppu.frame_started = false);
    CHECK_PPU_MUTATION(changed->ppu.row_coverage[0] ^= UINT8_C(0x80));
    CHECK_PPU_MUTATION(changed->events[0].generation++);
    CHECK_PPU_MUTATION(changed->video_frame[0] ^= UINT8_C(0x01));
#undef CHECK_PPU_MUTATION
    CHECK(equality_detects_ppu);

    {
        const gbl_time_operation operations[2] = {
            { GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 2u,
              UINT32_C(0x060001E0), UINT32_C(0xABCD), UINT64_C(1) }
        };

        CHECK(gbl_machine_prepare_operations(machine, operations, 2u,
                                             &preview) ==
              GBL_TIME_STATUS_OK);
        changed_preview = preview;
        if (changed_preview.ppu_row_capture_count != 0u) {
            changed_preview.ppu_row_captures[0].cycle++;
            preview_equality_detects_plan =
                !gbl_test_time_preview_equal(&preview, &changed_preview);
            changed_preview = preview;
            changed_preview.ppu_row_captures[0].dispcnt ^= UINT16_C(1);
            preview_equality_detects_plan = preview_equality_detects_plan &&
                !gbl_test_time_preview_equal(&preview, &changed_preview);
            changed_preview = preview;
            changed_preview.ppu_row_captures[0].render_register_bytes[0] ^=
                UINT8_C(1);
            preview_equality_detects_plan = preview_equality_detects_plan &&
                !gbl_test_time_preview_equal(&preview, &changed_preview);
            changed_preview = preview;
            changed_preview.ppu_row_captures[0].bg_register_bytes[0] ^=
                UINT8_C(1);
            preview_equality_detects_plan = preview_equality_detects_plan &&
                !gbl_test_time_preview_equal(&preview, &changed_preview);
            changed_preview = preview;
            changed_preview.ppu_row_captures[0].affine[0].origin_x++;
            preview_equality_detects_plan = preview_equality_detects_plan &&
                !gbl_test_time_preview_equal(&preview, &changed_preview);
        }
        if (preview.ppu_memory_write_count != 0u) {
            changed_preview = preview;
            changed_preview.ppu_memory_writes[0].value ^= UINT32_C(1);
            preview_equality_detects_plan = preview_equality_detects_plan &&
                !gbl_test_time_preview_equal(&preview, &changed_preview);
            changed_preview = preview;
            changed_preview.ppu_memory_writes[0].cycle++;
            preview_equality_detects_plan = preview_equality_detects_plan &&
                !gbl_test_time_preview_equal(&preview, &changed_preview);
            changed_preview = preview;
            changed_preview.ppu_memory_writes[0].region ^=
                (uint8_t)GBL_BUS_REGION_VRAM;
            preview_equality_detects_plan = preview_equality_detects_plan &&
                !gbl_test_time_preview_equal(&preview, &changed_preview);
        } else {
            preview_equality_detects_plan = false;
        }
        changed_preview = preview;
        changed_preview.ppu.frame_epoch++;
        preview_equality_detects_plan = preview_equality_detects_plan &&
            !gbl_test_time_preview_equal(&preview, &changed_preview);
        changed_preview = preview;
        changed_preview.ppu_clear_video = true;
        preview_equality_detects_plan = preview_equality_detects_plan &&
            !gbl_test_time_preview_equal(&preview, &changed_preview);
        *before = *machine;
        CHECK(gbl_machine_prepare_operations(machine, operations, 2u,
                                             &repeated) ==
              GBL_TIME_STATUS_OK);
        CHECK(gbl_test_time_preview_equal(&preview, &repeated));
        CHECK(gbl_test_canonical_equal(before, machine));
        CHECK(preview_equality_detects_plan);
        gbl_machine_commit_time(machine, &preview);
        CHECK(machine->master_cycles == UINT64_C(1233));
        CHECK(machine->ppu.line == 1u);
        CHECK(machine->ppu.captured_rows == 2u);
        CHECK(machine->video_frame[GBL_PPU_ROW_BYTES] == UINT8_C(0x34));
        CHECK(machine->video_frame[GBL_PPU_ROW_BYTES + 1u] == UINT8_C(0x12));
    }

    CHECK(ppu_prepare_refusal_is_atomic(
        machine, NULL, 0u, true, UINT64_C(1230),
        GBL_TIME_STATUS_BACKWARD));
    {
        const gbl_time_operation overflow = {
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1)
        };
        machine->master_cycles = UINT64_MAX;
        CHECK(ppu_prepare_refusal_is_atomic(
            machine, &overflow, 1u, false, 0u,
            GBL_TIME_STATUS_CYCLE_OVERFLOW));
    }

    free_machine(machine);
    machine = new_machine();
    CHECK(machine != NULL && prime_ppu_refusal_machine(machine));
    if (machine != NULL) {
        const gbl_time_operation advance = {
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1)
        };
        machine->next_event_sequence = UINT64_MAX;
        CHECK(ppu_prepare_refusal_is_atomic(
            machine, &advance, 1u, false, 0u,
            GBL_TIME_STATUS_SEQUENCE_EXHAUSTED));
    }

    free_machine(machine);
    machine = new_machine();
    CHECK(machine != NULL && prime_ppu_refusal_machine(machine));
    if (machine != NULL) {
        const gbl_time_operation advance = {
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1235)
        };
        machine->ppu.generation = UINT64_MAX - UINT64_C(2);
        machine->events[0].generation = machine->ppu.generation;
        CHECK(ppu_prepare_refusal_is_atomic(
            machine, &advance, 1u, false, 0u,
            GBL_TIME_STATUS_GENERATION_EXHAUSTED));
    }

    free_machine(machine);
    machine = new_machine();
    CHECK(machine != NULL && prime_ppu_refusal_machine(machine));
    if (machine != NULL) {
        const gbl_time_operation operations[2] = {
            { GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 2u,
              UINT32_C(0x04000102), UINT32_C(0x0080), UINT64_C(1) }
        };
        gbl_event ppu_event = machine->events[0];
        size_t index;

        machine->event_count = GBL_EVENT_CAPACITY;
        machine->events[0] = ppu_event;
        for (index = 1u; index < GBL_EVENT_CAPACITY; ++index) {
            machine->events[index] = (gbl_event){
                .cycle = UINT64_MAX,
                .sequence = (uint64_t)index,
                .generation = 0u,
                .priority = 1u,
                .kind = (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW,
                .timer_index = 1u,
                .live = true
            };
        }
        machine->next_event_sequence = GBL_EVENT_CAPACITY;
        CHECK(ppu_prepare_refusal_is_atomic(
            machine, operations, 2u, false, 0u,
            GBL_TIME_STATUS_EVENT_CAPACITY));
    }

    free_machine(machine);
    machine = new_machine();
    CHECK(machine != NULL && prime_ppu_refusal_machine(machine));
    if (machine != NULL) {
        const uint64_t target =
            (uint64_t)GBL_PPU_LINE_CYCLES *
            (uint64_t)GBL_PPU_TOTAL_LINES * UINT64_C(10);
        CHECK(ppu_prepare_refusal_is_atomic(
            machine, NULL, 0u, true, target,
            GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT));
    }

    CHECK(prime_ppu_refusal_machine(clean_twin));
    CHECK(gbl_machine_prepare_operations(clean_twin, &cross_line, 1u,
                                         &preview) == GBL_TIME_STATUS_OK);
    CHECK(clean_twin->master_cycles == UINT64_C(1231));
    gbl_machine_commit_time(clean_twin, &preview);
    CHECK(clean_twin->master_cycles == UINT64_C(1232));
    CHECK(clean_twin->ppu.line == 1u);
    CHECK(clean_twin->ppu.captured_rows == 2u);
    CHECK(preview.ppu_row_capture_count == 1u);
    CHECK(preview.ppu_row_captures[0].line == 1u);
    CHECK(preview.ppu_row_captures[0].dispcnt == UINT16_C(0x0403));
    CHECK(preview.ppu_row_captures[0].render_register_bytes[1] == 1u);
    CHECK(preview.ppu_row_captures[0].render_register_bytes[7] == 1u);
    CHECK(clean_twin->video_frame[GBL_PPU_ROW_BYTES] == UINT8_C(0x34));

    free(changed);
    free(before);
    free_machine(machine);
    free_machine(clean_twin);
    if (CHECK_DONE() == 0) {
        puts("ppu.refusal: raster previews preserve machine and output state");
        return 0;
    }
    return 1;
}

static bool ppu_obj_boundary_scene(gbl_machine *machine)
{
    size_t object;

    if (machine == NULL) {
        return false;
    }
    machine->dispcnt = UINT16_C(0x1000);
    machine->ppu.dispcnt = machine->dispcnt;
    machine->pram[0] = 0u;
    machine->pram[1] = UINT8_C(0x7C);
    ppu_obj_affine_set_palette_color(machine, 2u, UINT16_C(0x001F));
    memset(&machine->vram[UINT32_C(0x10000)], UINT8_C(0x22), 32u);
    for (object = 0u; object < 128u; ++object) {
        ppu_obj_store_u16(machine->oam, object * 8u, UINT16_C(0x0200));
    }
    ppu_obj_store_u16(machine->oam, 0u, 0u);
    ppu_obj_store_u16(machine->oam, 2u, 0u);
    ppu_obj_store_u16(machine->oam, 4u, 0u);
    return true;
}

static bool ppu_obj_prepare_timed_write(
    gbl_machine *machine, uint64_t write_cycle, uint64_t target_cycle,
    uint32_t address, uint8_t width, uint32_t value,
    gbl_machine_time_preview *preview)
{
    gbl_time_operation operations[3];
    size_t operation_count = 0u;

    if (machine == NULL || preview == NULL || write_cycle == 0u ||
        write_cycle > target_cycle) {
        return false;
    }
    if (write_cycle > UINT64_C(1)) {
        operations[operation_count++] = (gbl_time_operation){
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, write_cycle - 1u
        };
    }
    operations[operation_count++] = (gbl_time_operation){
        GBL_TIME_OPERATION_BUS_WRITE, width, address, value, UINT64_C(1)
    };
    if (write_cycle < target_cycle) {
        operations[operation_count++] = (gbl_time_operation){
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u,
            target_cycle - write_cycle
        };
    }
    return gbl_machine_prepare_operations(machine, operations,
                                          operation_count, preview) ==
           GBL_TIME_STATUS_OK;
}

static bool ppu_obj_boundary_palette_case(uint64_t write_cycle,
                                          uint16_t expected_line_one,
                                          size_t expected_prior_writes)
{
    const uint64_t boundary = (uint64_t)GBL_PPU_LINE_CYCLES;
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;
    bool valid = machine != NULL;

    if (valid) {
        valid = ppu_obj_boundary_scene(machine) &&
                ppu_obj_prepare_timed_write(
                    machine, write_cycle, boundary * UINT64_C(2),
                    UINT32_C(0x05000204), 2u, UINT32_C(0x03E0), &preview) &&
                preview.ppu_row_capture_count == 3u &&
                preview.ppu_memory_write_count == 1u &&
                preview.ppu_row_captures[1].line == 1u &&
                preview.ppu_row_captures[1].prior_write_count ==
                    expected_prior_writes &&
                preview.ppu_row_captures[2].line == 2u &&
                preview.ppu_row_captures[2].prior_write_count == 1u;
        if (valid) {
            gbl_machine_commit_time(machine, &preview);
            valid = ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                                 expected_line_one) &&
                    ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 2u,
                                                 UINT16_C(0x03E0));
        }
    }
    free_machine(machine);
    return valid;
}

static bool ppu_obj_boundary_attribute_case(uint64_t write_cycle,
                                            bool sampled_before_write)
{
    const uint64_t boundary = (uint64_t)GBL_PPU_LINE_CYCLES;
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;
    bool valid = machine != NULL;

    if (valid) {
        valid = ppu_obj_boundary_scene(machine);
        if (valid) {
            ppu_obj_store_u16(machine->oam, 0u, UINT16_C(0x0200));
            valid = ppu_obj_prepare_timed_write(
                        machine, write_cycle, boundary * UINT64_C(2),
                        UINT32_C(0x07000000), 2u, 0u, &preview) &&
                    preview.ppu_row_capture_count == 3u &&
                    preview.ppu_memory_write_count == 1u &&
                    preview.ppu_row_captures[1].prior_write_count ==
                        (sampled_before_write ? 1u : 0u) &&
                    preview.ppu_row_captures[1].obj_selected_count ==
                        (sampled_before_write ? 1u : 0u) &&
                    preview.ppu_row_captures[2].obj_selected_count == 1u;
        }
        if (valid) {
            gbl_machine_commit_time(machine, &preview);
            valid = ppu_bg_affine_pixel_matches(
                        machine->video_frame, 0u, 1u,
                        sampled_before_write ? UINT16_C(0x001F)
                                             : UINT16_C(0x7C00)) &&
                    ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 2u,
                                                UINT16_C(0x001F));
        }
    }
    free_machine(machine);
    return valid;
}

static bool ppu_obj_boundary_tile_case(uint64_t write_cycle,
                                       bool sampled_before_write)
{
    const uint64_t boundary = (uint64_t)GBL_PPU_LINE_CYCLES;
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;
    bool valid = machine != NULL;

    if (valid) {
        valid = ppu_obj_boundary_scene(machine);
        if (valid) {
            machine->vram[UINT32_C(0x10004)] = 0u;
            valid = ppu_obj_prepare_timed_write(
                        machine, write_cycle, boundary * UINT64_C(2),
                        UINT32_C(0x06010004), 2u, UINT32_C(0x0022),
                        &preview) &&
                    preview.ppu_row_capture_count == 3u &&
                    preview.ppu_memory_write_count == 1u &&
                    preview.ppu_row_captures[1].prior_write_count ==
                        (sampled_before_write ? 1u : 0u) &&
                    preview.ppu_row_captures[2].prior_write_count == 1u;
        }
        if (valid) {
            gbl_machine_commit_time(machine, &preview);
            valid = ppu_bg_affine_pixel_matches(
                        machine->video_frame, 0u, 1u,
                        sampled_before_write ? UINT16_C(0x001F)
                                             : UINT16_C(0x7C00)) &&
                    ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 2u,
                                                UINT16_C(0x001F));
        }
    }
    free_machine(machine);
    return valid;
}

static bool ppu_obj_boundary_bg_priority_case(uint64_t write_cycle,
                                              bool sampled_before_write)
{
    const uint64_t boundary = (uint64_t)GBL_PPU_LINE_CYCLES;
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;
    bool valid = machine != NULL;

    if (valid) {
        valid = ppu_composition_prepare_text_scene(
                    machine, 2u, 3u, 1u, 2u, true, true) &&
                ppu_bg_text_set_map_entry(machine, 0u, 0u, 0u, 1u) &&
                ppu_bg_text_set_color(machine, 1u, UINT16_C(0x03E0));
        if (valid) {
            ppu_obj_affine_set_palette_color(machine, 2u, UINT16_C(0x001F));
            ppu_obj_store_u16(machine->oam, 0u, 0u);
            ppu_obj_store_u16(machine->oam, 2u, 0u);
            valid = ppu_obj_prepare_timed_write(
                        machine, write_cycle, boundary * UINT64_C(2),
                        UINT32_C(0x04000008), 2u, UINT32_C(0x0100),
                        &preview) &&
                    preview.ppu_row_capture_count == 3u &&
                    preview.ppu_memory_write_count == 0u;
        }
        if (valid) {
            const uint16_t row_one_bg_control =
                (uint16_t)preview.ppu_row_captures[1].bg_register_bytes[0] |
                (uint16_t)((uint16_t)preview.ppu_row_captures[1]
                               .bg_register_bytes[1] << 8u);
            const uint16_t row_two_bg_control =
                (uint16_t)preview.ppu_row_captures[2].bg_register_bytes[0] |
                (uint16_t)((uint16_t)preview.ppu_row_captures[2]
                               .bg_register_bytes[1] << 8u);

            valid = row_one_bg_control ==
                        (sampled_before_write ? UINT16_C(0x0100)
                                              : UINT16_C(0x0102)) &&
                    row_two_bg_control == UINT16_C(0x0100);
            if (valid) {
                gbl_machine_commit_time(machine, &preview);
                valid = ppu_bg_affine_pixel_matches(
                            machine->video_frame, 0u, 1u,
                            sampled_before_write ? UINT16_C(0x03E0)
                                                 : UINT16_C(0x001F)) &&
                        ppu_bg_affine_pixel_matches(machine->video_frame,
                                                    0u, 2u,
                                                    UINT16_C(0x03E0));
            }
        }
    }
    free_machine(machine);
    return valid;
}

static bool ppu_obj_boundary_bg_source_case(uint64_t write_cycle,
                                            bool sampled_before_write)
{
    const uint64_t boundary = (uint64_t)GBL_PPU_LINE_CYCLES;
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;
    bool valid = machine != NULL;

    if (valid) {
        valid = ppu_composition_prepare_text_scene(
                    machine, 0u, 3u, 1u, 2u, true, true) &&
                ppu_bg_text_set_map_entry(machine, 0u, 0u, 0u, 1u) &&
                ppu_bg_text_set_color(machine, 1u, UINT16_C(0x03E0));
        if (valid) {
            ppu_obj_affine_set_palette_color(machine, 2u, UINT16_C(0x001F));
            ppu_obj_store_u16(machine->oam, 0u, 0u);
            ppu_obj_store_u16(machine->oam, 2u, 0u);
            valid = ppu_obj_prepare_timed_write(
                        machine, write_cycle, boundary * UINT64_C(2),
                        UINT32_C(0x06000024), 2u, 0u, &preview) &&
                    preview.ppu_row_capture_count == 3u &&
                    preview.ppu_memory_write_count == 1u &&
                    preview.ppu_row_captures[1].prior_write_count ==
                        (sampled_before_write ? 1u : 0u) &&
                    preview.ppu_row_captures[2].prior_write_count == 1u;
        }
        if (valid) {
            gbl_machine_commit_time(machine, &preview);
            valid = ppu_bg_affine_pixel_matches(
                        machine->video_frame, 0u, 1u,
                        sampled_before_write ? UINT16_C(0x001F)
                                             : UINT16_C(0x03E0)) &&
                    ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 2u,
                                                UINT16_C(0x03E0));
        }
    }
    free_machine(machine);
    return valid;
}

static bool ppu_obj_boundary_ordered_writes(void)
{
    const uint64_t boundary = (uint64_t)GBL_PPU_LINE_CYCLES;
    const gbl_time_operation operations[4] = {
        { GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u,
          boundary - UINT64_C(2) },
        { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x05000204),
          UINT32_C(0x03E0), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x05000204),
          UINT32_C(0x7C00), UINT64_C(1) },
        { GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u,
          boundary * UINT64_C(2) - boundary }
    };
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;
    bool valid = machine != NULL;

    if (valid) {
        valid = ppu_obj_boundary_scene(machine) &&
                gbl_machine_prepare_operations(machine, operations, 4u,
                                               &preview) ==
                    GBL_TIME_STATUS_OK &&
                preview.ppu_row_capture_count == 3u &&
                preview.ppu_memory_write_count == 2u &&
                preview.ppu_memory_writes[0].cycle == boundary - 1u &&
                preview.ppu_memory_writes[1].cycle == boundary &&
                preview.ppu_row_captures[1].prior_write_count == 1u &&
                preview.ppu_row_captures[2].prior_write_count == 2u;
        if (valid) {
            gbl_machine_commit_time(machine, &preview);
            valid = ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                                 UINT16_C(0x03E0)) &&
                    ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 2u,
                                                 UINT16_C(0x7C00));
        }
    }
    free_machine(machine);
    return valid;
}

static bool ppu_obj_boundary_matrix_case(uint64_t write_cycle,
                                         bool sampled_before_write)
{
    const uint64_t boundary = (uint64_t)GBL_PPU_LINE_CYCLES * UINT64_C(24);
    const uint64_t target = (uint64_t)GBL_PPU_LINE_CYCLES * UINT64_C(26);
    gbl_machine *machine = new_machine();
    gbl_machine_time_preview preview;
    bool valid = machine != NULL;
    uint8_t source_y;

    if (valid) {
        valid = (write_cycle == boundary - UINT64_C(1) ||
                 write_cycle == boundary || write_cycle == boundary + 1u) &&
                ppu_obj_affine_setup(machine, 0u, 24u, 24u, 0u, 0u, 0u,
                                     0u, false, 0, 0, 0, 256);
        if (valid) {
            for (source_y = 0u; source_y < 8u; ++source_y) {
                ppu_obj_affine_set_4bpp_pixel(machine, 0u, 0u, source_y,
                                               1u);
                ppu_obj_affine_set_4bpp_pixel(machine, 0u, 4u, source_y,
                                               2u);
            }
            valid = ppu_obj_prepare_timed_write(
                        machine, write_cycle, target,
                        UINT32_C(0x07000006), 2u, UINT32_C(0x0100),
                        &preview) &&
                    preview.ppu_row_capture_count == 27u &&
                    preview.ppu_memory_write_count == 1u &&
                    preview.ppu_row_captures[24].line == 24u &&
                    preview.ppu_row_captures[24].prior_write_count ==
                        (sampled_before_write ? 1u : 0u) &&
                    preview.ppu_row_captures[25].line == 25u &&
                    preview.ppu_row_captures[25].prior_write_count == 1u;
        }
        if (valid) {
            gbl_machine_commit_time(machine, &preview);
            valid = ppu_bg_affine_pixel_matches(
                        machine->video_frame, 24u, 24u,
                        sampled_before_write ? UINT16_C(0x001F)
                                             : UINT16_C(0x03E0)) &&
                    ppu_bg_affine_pixel_matches(machine->video_frame,
                                                24u, 25u,
                                                UINT16_C(0x001F));
        }
    }
    free_machine(machine);
    return valid;
}

static bool ppu_obj_controls_mutations_refused(
    gbl_machine *machine, const gbl_machine_time_preview *preview,
    uint32_t write_mutation_mask)
{
    gbl_machine *before = malloc(sizeof(*before));
    gbl_machine *clean_twin = malloc(sizeof(*clean_twin));
    gbl_machine_time_preview *changed = malloc(sizeof(*changed));
    gbl_machine_time_preview *caller_before = malloc(sizeof(*caller_before));
    bool valid = before != NULL && clean_twin != NULL && changed != NULL &&
                 caller_before != NULL;
    size_t byte;

    if (!valid) {
        free(caller_before);
        free(changed);
        free(clean_twin);
        free(before);
        return false;
    }
#define CHECK_OBJ_CAPTURE_MUTATION(change)                                  \
    do {                                                                     \
        memcpy(changed, preview, sizeof(*changed));                          \
        change;                                                              \
        memcpy(before, machine, sizeof(*before));                            \
        memcpy(clean_twin, machine, sizeof(*clean_twin));                    \
        memcpy(caller_before, changed, sizeof(*caller_before));              \
        if (gbl_test_time_preview_equal(preview, changed) ||                 \
            gbl_test_time_preview_ppu_digest(preview) ==                     \
                gbl_test_time_preview_ppu_digest(changed)) {                  \
            valid = false;                                                   \
        }                                                                    \
        gbl_machine_commit_time(machine, changed);                           \
        if (!gbl_test_canonical_equal(before, machine) ||                    \
            !gbl_test_canonical_equal(clean_twin, machine) ||                \
            gbl_test_canonical_digest(before) !=                             \
                gbl_test_canonical_digest(machine) ||                        \
            memcmp(caller_before, changed, sizeof(*changed)) != 0 ||         \
            !gbl_test_same_instance_identity_equal(before, machine)) {       \
            valid = false;                                                   \
        }                                                                    \
    } while (0)
    for (byte = 0u; byte < GBL_PPU_OBJ_SELECTED_MASK_BYTES; ++byte) {
        unsigned bit;

        for (bit = 0u; bit < 8u; ++bit) {
            CHECK_OBJ_CAPTURE_MUTATION(
                changed->ppu_row_captures[0].obj_selected_mask[byte] ^=
                    (uint8_t)(UINT8_C(1) << bit));
        }
    }
    CHECK_OBJ_CAPTURE_MUTATION(
        ++changed->ppu_row_captures[0].obj_selected_count);
    CHECK_OBJ_CAPTURE_MUTATION(
        ++changed->ppu_row_captures[0].obj_omitted_count);
    CHECK_OBJ_CAPTURE_MUTATION(
        ++changed->ppu_row_captures[0].obj_work_used);
    CHECK_OBJ_CAPTURE_MUTATION(
        ++changed->ppu_row_captures[0].prior_write_count);
    CHECK_OBJ_CAPTURE_MUTATION(
        changed->ppu_row_captures[0].pixel_digest ^= UINT64_C(1));
    CHECK_OBJ_CAPTURE_MUTATION(
        changed->ppu_row_captures[0].dispcnt ^= UINT16_C(0x1000));
    if (preview->ppu_memory_write_count == 0u) {
        valid = false;
    } else {
        CHECK_OBJ_CAPTURE_MUTATION(
            changed->ppu_memory_writes[0].value ^= write_mutation_mask);
    }
#undef CHECK_OBJ_CAPTURE_MUTATION
    free(caller_before);
    free(changed);
    free(clean_twin);
    free(before);
    return valid;
}

static bool ppu_obj_late_row_refusal_is_atomic(void)
{
    const uint64_t line_cycles = (uint64_t)GBL_PPU_LINE_CYCLES;
    const uint64_t target = line_cycles * UINT64_C(7);
    gbl_machine *machine = new_machine();
    gbl_machine *before = malloc(sizeof(*before));
    gbl_machine *clean_twin = malloc(sizeof(*clean_twin));
    gbl_machine_time_preview *preview = malloc(sizeof(*preview));
    gbl_machine_time_preview *changed = malloc(sizeof(*changed));
    gbl_machine_time_preview *caller_before = malloc(sizeof(*caller_before));
    gbl_machine_time_preview retry;
    bool valid = machine != NULL && before != NULL && clean_twin != NULL &&
                 preview != NULL && changed != NULL && caller_before != NULL;

    if (!valid) {
        free(caller_before);
        free(changed);
        free(preview);
        free(clean_twin);
        free(before);
        free_machine(machine);
        return false;
    }
    valid = ppu_obj_boundary_scene(machine) &&
            ppu_obj_prepare_timed_write(
                machine, line_cycles - UINT64_C(1), target,
                UINT32_C(0x05000204), 2u, UINT32_C(0x03E0), preview) &&
            preview->ppu_row_capture_count == 8u &&
            preview->ppu_row_captures[7].line == 7u;
    if (valid) {
        *before = *machine;
        *clean_twin = *machine;
        memcpy(changed, preview, sizeof(*changed));
        changed->ppu_row_captures[7].pixel_digest ^= UINT64_C(1);
        memcpy(caller_before, changed, sizeof(*caller_before));
        valid = !gbl_test_time_preview_equal(preview, changed) &&
                gbl_test_time_preview_ppu_digest(preview) !=
                    gbl_test_time_preview_ppu_digest(changed);
        gbl_machine_commit_time(machine, changed);
        valid = valid && gbl_test_canonical_equal(before, machine) &&
                gbl_test_canonical_equal(clean_twin, machine) &&
                gbl_test_canonical_digest(before) ==
                    gbl_test_canonical_digest(machine) &&
                memcmp(caller_before, changed, sizeof(*changed)) == 0 &&
                ppu_obj_prepare_timed_write(
                    machine, line_cycles - UINT64_C(1), target,
                    UINT32_C(0x05000204), 2u, UINT32_C(0x03E0), &retry) &&
                gbl_test_time_preview_equal(preview, &retry);
        if (valid) {
            gbl_machine_commit_time(machine, &retry);
            valid = machine->master_cycles == target &&
                    ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                                 UINT16_C(0x03E0)) &&
                    ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 7u,
                                                 UINT16_C(0x03E0));
        }
    }
    free(caller_before);
    free(changed);
    free(preview);
    free(clean_twin);
    free(before);
    free_machine(machine);
    return valid;
}

static int run_ppu_obj_boundary(void)
{
    const uint64_t line_boundary = (uint64_t)GBL_PPU_LINE_CYCLES;
    const uint64_t matrix_boundary =
        (uint64_t)GBL_PPU_LINE_CYCLES * UINT64_C(24);

    CHECK(ppu_obj_boundary_palette_case(line_boundary - UINT64_C(1),
                                        UINT16_C(0x03E0), 1u));
    CHECK(ppu_obj_boundary_palette_case(line_boundary, UINT16_C(0x001F), 0u));
    CHECK(ppu_obj_boundary_palette_case(line_boundary + UINT64_C(1),
                                        UINT16_C(0x001F), 0u));
    CHECK(ppu_obj_boundary_attribute_case(line_boundary - UINT64_C(1), true));
    CHECK(ppu_obj_boundary_attribute_case(line_boundary, false));
    CHECK(ppu_obj_boundary_attribute_case(line_boundary + UINT64_C(1), false));
    CHECK(ppu_obj_boundary_tile_case(line_boundary - UINT64_C(1), true));
    CHECK(ppu_obj_boundary_tile_case(line_boundary, false));
    CHECK(ppu_obj_boundary_tile_case(line_boundary + UINT64_C(1), false));
    CHECK(ppu_obj_boundary_bg_priority_case(line_boundary - UINT64_C(1), true));
    CHECK(ppu_obj_boundary_bg_priority_case(line_boundary, false));
    CHECK(ppu_obj_boundary_bg_priority_case(line_boundary + UINT64_C(1), false));
    CHECK(ppu_obj_boundary_bg_source_case(line_boundary - UINT64_C(1), true));
    CHECK(ppu_obj_boundary_bg_source_case(line_boundary, false));
    CHECK(ppu_obj_boundary_bg_source_case(line_boundary + UINT64_C(1), false));
    CHECK(ppu_obj_boundary_ordered_writes());
    CHECK(ppu_obj_boundary_matrix_case(matrix_boundary - UINT64_C(1), true));
    CHECK(ppu_obj_boundary_matrix_case(matrix_boundary, false));
    CHECK(ppu_obj_boundary_matrix_case(matrix_boundary + UINT64_C(1), false));
    if (CHECK_DONE() == 0) {
        puts("ppu.obj.boundary: OAM attributes, OBJ matrices/tiles/palette and BG priority obey before/on/after row prefixes");
        return 0;
    }
    return 1;
}

static int run_ppu_obj_controls(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *matrix_machine = new_machine();
    gbl_machine_time_preview preview;
    gbl_machine_time_preview matrix_preview;

    CHECK(machine != NULL && matrix_machine != NULL);
    if (machine != NULL && matrix_machine != NULL) {
        const gbl_time_operation operations[2] = {
            { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x07000000),
              0u, UINT64_C(1) },
            { GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u,
              (uint64_t)GBL_PPU_LINE_CYCLES - UINT64_C(1) }
        };

        CHECK(ppu_obj_boundary_scene(machine));
        CHECK(gbl_machine_prepare_operations(machine, operations, 2u,
                                             &preview) == GBL_TIME_STATUS_OK);
        CHECK(preview.ppu_row_capture_count == 2u);
        if (preview.ppu_row_capture_count > 1u) {
            CHECK(preview.ppu_row_captures[1].line == 1u);
            CHECK(preview.ppu_row_captures[1].obj_selected_count == 1u);
            CHECK(preview.ppu_row_captures[1].prior_write_count == 1u);
        }
        if (!ppu_obj_controls_mutations_refused(machine, &preview,
                                                UINT32_C(0x0200))) {
            CHECK(false);
        } else {
            CHECK(true);
        }
        gbl_machine_commit_time(machine, &preview);
        CHECK(machine->master_cycles == (uint64_t)GBL_PPU_LINE_CYCLES);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 1u,
                                          UINT16_C(0x001F)));
        free_machine(machine);

        {
            uint8_t source_y;
            const uint64_t boundary =
                (uint64_t)GBL_PPU_LINE_CYCLES;
            const uint64_t target = boundary * UINT64_C(2);

            CHECK(ppu_obj_affine_setup(
                matrix_machine, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
                false, 0, 0, 0, 256));
            for (source_y = 0u; source_y < 8u; ++source_y) {
                ppu_obj_affine_set_4bpp_pixel(matrix_machine, 0u, 0u,
                                               source_y, 1u);
                ppu_obj_affine_set_4bpp_pixel(matrix_machine, 0u, 4u,
                                               source_y, 2u);
            }
            CHECK(ppu_obj_prepare_timed_write(
                matrix_machine, boundary - UINT64_C(1), target,
                UINT32_C(0x07000006), 2u, UINT32_C(0x0100),
                &matrix_preview));
            CHECK(matrix_preview.ppu_row_capture_count == 3u);
            CHECK(matrix_preview.ppu_row_captures[1].prior_write_count == 1u);
            CHECK(ppu_obj_controls_mutations_refused(
                matrix_machine, &matrix_preview, UINT32_C(0x0100)));
            gbl_machine_commit_time(matrix_machine, &matrix_preview);
            CHECK(ppu_bg_affine_pixel_matches(matrix_machine->video_frame,
                                              0u, 0u, UINT16_C(0x03E0)));
            CHECK(ppu_bg_affine_pixel_matches(matrix_machine->video_frame,
                                              0u, 1u, UINT16_C(0x001F)));
        }
        CHECK(ppu_obj_late_row_refusal_is_atomic());
        free_machine(matrix_machine);
    } else {
        free_machine(machine);
        free_machine(matrix_machine);
    }
    if (CHECK_DONE() == 0) {
        puts("ppu.obj.controls: selected masks/counters, OAM attributes/matrices, prefixes and pixel digests are sensitive and refusal is atomic");
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

static gbl_bus_result ppu_read_at(gbl_machine *machine, uint32_t address,
                                  uint64_t completion_cycle)
{
    gbl_bus_result result = { 0 };

    if (machine == NULL || completion_cycle == 0u ||
        gbl_machine_advance_to(machine, completion_cycle - 1u) !=
            GBL_TIME_STATUS_OK) {
        result.status = GBL_BUS_STATUS_TIME_REFUSAL;
        return result;
    }
    return gbl_bus_read(machine, address, 2u,
                        GBL_BUS_CLASS_NONSEQUENTIAL);
}

static int run_ppu_boundary(void)
{
    gbl_machine *hblank = new_machine();
    gbl_machine *raster = new_machine();
    gbl_machine *all_lines = new_machine();
    gbl_bus_result result;
    static const struct {
        uint64_t cycle;
        uint16_t line;
        bool vblank;
        bool vcount_match;
    } lines[] = {
        { UINT64_C(1), 0u, false, true },
        { UINT64_C(1232), 1u, false, false },
        { UINT64_C(195888), 159u, false, false },
        { UINT64_C(197120), 160u, true, false },
        { UINT64_C(279664), 227u, true, false },
        { UINT64_C(280896), 0u, false, true }
    };
    size_t index;

    if (hblank == NULL || raster == NULL || all_lines == NULL) {
        fprintf(stderr, "ppu.boundary: host allocation failed\n");
        free_machine(all_lines);
        free_machine(raster);
        free_machine(hblank);
        return 1;
    }

    result = ppu_read_at(hblank, UINT32_C(0x04000004), UINT64_C(1005));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK((result.value & UINT32_C(0x0002)) == 0u);
    result = ppu_read_at(hblank, UINT32_C(0x04000004), UINT64_C(1006));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK((result.value & UINT32_C(0x0002)) != 0u);
    result = ppu_read_at(hblank, UINT32_C(0x04000004), UINT64_C(1007));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK((result.value & UINT32_C(0x0002)) != 0u);

    for (index = 0u; index < sizeof(lines) / sizeof(lines[0]); ++index) {
        result = ppu_read_at(raster, UINT32_C(0x04000006),
                             lines[index].cycle);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.value == lines[index].line);
        result = gbl_bus_read(raster, UINT32_C(0x04000004), 2u,
                              GBL_BUS_CLASS_NONSEQUENTIAL);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(((result.value & UINT32_C(0x0001)) != 0u) ==
              lines[index].vblank);
        CHECK(((result.value & UINT32_C(0x0004)) != 0u) ==
              lines[index].vcount_match);
        if (index == 4u) {
            CHECK(gbl_machine_advance_to(raster, UINT64_C(280669)) ==
                  GBL_TIME_STATUS_OK);
            result = gbl_bus_read(raster, UINT32_C(0x04000004), 2u,
                                  GBL_BUS_CLASS_NONSEQUENTIAL);
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK(raster->ppu.line == 227u);
            CHECK((result.value & UINT32_C(0x0003)) == UINT32_C(0x0003));
        }
    }
    {
        uint16_t line;

        for (line = 0u; line < 228u; ++line) {
            const uint64_t hblank_cycle =
                (uint64_t)line * UINT64_C(1232) + UINT64_C(1006);
            const uint64_t next_line =
                ((uint64_t)line + UINT64_C(1)) * UINT64_C(1232);

            CHECK(gbl_machine_advance_to(all_lines, hblank_cycle) ==
                  GBL_TIME_STATUS_OK);
            CHECK(all_lines->ppu.line == line);
            CHECK((all_lines->ppu.dispstat & UINT16_C(0x0002)) != 0u);
            CHECK(gbl_machine_advance_to(all_lines, next_line) ==
                  GBL_TIME_STATUS_OK);
            CHECK((all_lines->ppu.dispstat & UINT16_C(0x0002)) == 0u);
        }
    }
    printf("{\"type\":\"ppu.boundary\",\"line_cycles\":1232,\"hblank_offset\":1006,\"hblank_provenance\":\"project-selected/REF\",\"hblank_dispute\":\"disp-hblank-flag-edge\",\"hardware_qualification\":\"UNKNOWN\"}\n");

    free_machine(all_lines);
    free_machine(raster);
    free_machine(hblank);
    if (CHECK_DONE() == 0) {
        puts("ppu.boundary: timed status reads follow all raster edges");
        return 0;
    }
    return 1;
}

static int run_ppu_registers(void)
{
    gbl_machine *machine = new_machine();
    gbl_bus_result result;
    size_t index;

    if (machine == NULL) {
        fprintf(stderr, "ppu.registers: host allocation failed\n");
        return 1;
    }

    result = gbl_bus_write(machine, UINT32_C(0x04000000), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFFFF));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(machine->dispcnt == UINT16_C(0xFFFF));
    result = gbl_bus_read(machine, UINT32_C(0x04000000), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.value == UINT32_C(0xFFFF));
    result = gbl_bus_write(machine, UINT32_C(0x04000020), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0xABCD));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(machine->ppu.render_register_bytes[0] == UINT8_C(0xCD));
    CHECK(machine->ppu.render_register_bytes[1] == UINT8_C(0xAB));
    CHECK((machine->ppu.unsupported_register_bytes & UINT32_C(0x0003)) ==
          UINT32_C(0x0003));

    result = gbl_bus_write(machine, UINT32_C(0x04000004), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFF));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK((machine->ppu.dispstat & UINT16_C(0x00C0)) == 0u);
    CHECK((machine->ppu.dispstat & UINT16_C(0x0038)) == UINT16_C(0x0038));
    CHECK((machine->ppu.dispstat & UINT16_C(0x0004)) != 0u);
    result = gbl_bus_write(machine, UINT32_C(0x04000005), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xFF));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(machine->ppu.compare == UINT16_C(0x00FF));
    result = gbl_bus_read(machine, UINT32_C(0x04000004), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK((result.value & UINT32_C(0xFF38)) == UINT32_C(0xFF38));
    CHECK((result.value & UINT32_C(0x00C0)) == 0u);
    CHECK((result.value & UINT32_C(0x0004)) == 0u);

    result = gbl_bus_write(machine, UINT32_C(0x04000006), 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x1234));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    result = gbl_bus_read(machine, UINT32_C(0x04000006), 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.value == machine->ppu.line);

    /* The wide transaction crosses DISPSTAT into write-ignored VCOUNT lanes.
     * Its compare changes to line zero once, after the old transaction state. */
    result = gbl_bus_write(machine, UINT32_C(0x04000004), 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL,
                           UINT32_C(0xDEAD0038));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(machine->ppu.compare == 0u);
    CHECK((machine->ppu.dispstat & UINT16_C(0x0038)) == UINT16_C(0x0038));
    CHECK((machine->ppu.dispstat & UINT16_C(0x0004)) != 0u);
    CHECK((machine->interrupt_flags & UINT16_C(0x0004)) != 0u);
    CHECK(machine->ppu.dispstat ==
          (uint16_t)(machine->ppu.dispstat & UINT16_C(0xFF3F)));

    /* Enable while the current line is already matched: no synthetic edge. */
    machine->interrupt_flags = 0u;
    result = gbl_bus_write(machine, UINT32_C(0x04000004), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x20));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK((machine->ppu.dispstat & UINT16_C(0x0004)) != 0u);
    CHECK((machine->interrupt_flags & UINT16_C(0x0004)) == 0u);
    free_machine(machine);

    {
        gbl_machine *same_cycle = new_machine();

        CHECK(same_cycle != NULL);
        if (same_cycle != NULL) {
            result = gbl_bus_write(same_cycle, UINT32_C(0x04000004), 2u,
                                   GBL_BUS_CLASS_NONSEQUENTIAL,
                                   UINT32_C(0x0100));
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK((same_cycle->ppu.dispstat & UINT16_C(0x0004)) == 0u);
            CHECK(gbl_machine_advance_to(same_cycle, UINT64_C(1231)) ==
                  GBL_TIME_STATUS_OK);
            result = gbl_bus_write(same_cycle, UINT32_C(0x04000004), 1u,
                                   GBL_BUS_CLASS_NONSEQUENTIAL,
                                   UINT32_C(0x0020));
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK(same_cycle->ppu.line == 1u);
            CHECK((same_cycle->ppu.dispstat & UINT16_C(0x0004)) != 0u);
            CHECK((same_cycle->interrupt_flags & UINT16_C(0x0004)) == 0u);
            free_machine(same_cycle);
        }
    }

    for (index = 0u; index < 3u; ++index) {
        gbl_machine *irq_machine = new_machine();
        uint16_t enable = (uint16_t)(UINT16_C(1) << index);
        uint16_t irq_control = (uint16_t)(UINT16_C(1) << (index + 3u));
        uint64_t edge = index == 0u ? UINT64_C(197120)
                          : index == 1u ? UINT64_C(1006)
                                        : UINT64_C(1232);

        if (irq_machine == NULL) {
            fprintf(stderr, "ppu.registers: IRQ machine allocation failed\n");
            return 1;
        }
        if (index == 2u) {
            irq_control = (uint16_t)(irq_control | UINT16_C(0x0100));
        }
        result = gbl_bus_write(irq_machine, UINT32_C(0x04000004), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, irq_control);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(gbl_machine_advance_to(irq_machine, edge) ==
              GBL_TIME_STATUS_OK);
        CHECK((irq_machine->interrupt_flags & enable) != 0u);
        if (index == 0u) {
            CHECK((irq_machine->ppu.dispstat & UINT16_C(0x0001)) != 0u);
        } else if (index == 1u) {
            CHECK((irq_machine->ppu.dispstat & UINT16_C(0x0002)) != 0u);
        } else {
            CHECK((irq_machine->ppu.dispstat & UINT16_C(0x0004)) != 0u);
        }
        free_machine(irq_machine);
    }

    {
        gbl_machine *ack = new_machine();

        CHECK(ack != NULL);
        if (ack != NULL) {
            result = gbl_bus_write(ack, UINT32_C(0x04000004), 2u,
                                   GBL_BUS_CLASS_NONSEQUENTIAL,
                                   UINT32_C(0x0010));
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK(gbl_machine_advance_to(ack, UINT64_C(1006)) ==
                  GBL_TIME_STATUS_OK);
            CHECK((ack->interrupt_flags & UINT16_C(0x0002)) != 0u);
            result = gbl_bus_write(ack, UINT32_C(0x04000202), 2u,
                                   GBL_BUS_CLASS_NONSEQUENTIAL,
                                   UINT32_C(0x0002));
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK((ack->interrupt_flags & UINT16_C(0x0002)) == 0u);
            CHECK(gbl_machine_advance_to(ack, UINT64_C(2238)) ==
                  GBL_TIME_STATUS_OK);
            CHECK((ack->interrupt_flags & UINT16_C(0x0002)) != 0u);
            free_machine(ack);
        }
    }

    if (CHECK_DONE() == 0) {
        puts("ppu.registers: display lanes and requests follow project policy");
        return 0;
    }
    return 1;
}

static uint16_t ppu_pattern_color(uint16_t x, uint16_t y)
{
    static const uint16_t colors[4] = {
        UINT16_C(0x001F), UINT16_C(0x03E0),
        UINT16_C(0x7C00), UINT16_C(0xFFFF)
    };

    return (uint16_t)(colors[(x + y) & 3u] & UINT16_C(0x7FFF));
}

static void ppu_seed_supported_frame(gbl_machine *machine)
{
    uint16_t y;

    machine->dispcnt = UINT16_C(0x0403);
    machine->ppu.dispcnt = UINT16_C(0x0403);
    for (y = 0u; y < GBL_PPU_VISIBLE_LINES; ++y) {
        uint16_t x;

        for (x = 0u; x < GBL_PPU_WIDTH; ++x) {
            const uint16_t color = ppu_pattern_color(x, y);
            const size_t offset =
                (size_t)y * GBL_PPU_ROW_BYTES + (size_t)x * 2u;

            machine->vram[offset] = (uint8_t)color;
            machine->vram[offset + 1u] = (uint8_t)(color >> 8u);
        }
    }
}

static bool ppu_frame_matches_pattern(const uint8_t *frame)
{
    uint16_t y;

    if (frame == NULL) {
        return false;
    }
    for (y = 0u; y < GBL_PPU_VISIBLE_LINES; ++y) {
        uint16_t x;

        for (x = 0u; x < GBL_PPU_WIDTH; ++x) {
            const uint16_t expected = ppu_pattern_color(x, y);
            const size_t offset =
                (size_t)y * GBL_PPU_ROW_BYTES + (size_t)x * 2u;

            if (frame[offset] != (uint8_t)expected ||
                frame[offset + 1u] != (uint8_t)(expected >> 8u)) {
                return false;
            }
        }
    }
    return true;
}

static bool ppu_pixel_matches(const uint8_t *frame, uint16_t y,
                              uint16_t x, uint16_t expected)
{
    const size_t offset =
        (size_t)y * GBL_PPU_ROW_BYTES + (size_t)x * 2u;
    const uint16_t actual = (uint16_t)(
        (uint16_t)frame[offset] |
        (uint16_t)((uint16_t)frame[offset + 1u] << 8u));

    return actual == expected;
}

static int run_ppu_latch(void)
{
    static const uint64_t completion_cycles[3] = {
        UINT64_C(1231), UINT64_C(1232), UINT64_C(1233)
    };
    size_t timing;

    for (timing = 0u; timing < 3u; ++timing) {
        gbl_machine *machine = new_machine();
        const uint64_t sample_cycle = UINT64_C(1232);
        gbl_bus_result result;
        const uint16_t expected =
            timing == 0u ? UINT16_C(0x5A5A) : UINT16_C(0x1234);

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        machine->dispcnt = UINT16_C(0x0403);
        machine->ppu.dispcnt = UINT16_C(0x0403);
        machine->vram[GBL_PPU_ROW_BYTES] = UINT8_C(0x34);
        machine->vram[GBL_PPU_ROW_BYTES + 1u] = UINT8_C(0x12);
        result = gbl_bus_write(machine, UINT32_C(0x04000000), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x0403));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(gbl_machine_advance_to(
                  machine, completion_cycles[timing] - 1u) ==
              GBL_TIME_STATUS_OK);
        if (timing == 2u) {
            CHECK(gbl_machine_advance_to(machine, sample_cycle) ==
                  GBL_TIME_STATUS_OK);
        }
        result = gbl_bus_write(machine, UINT32_C(0x060001E0), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x5A));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(machine->master_cycles == completion_cycles[timing]);
        if (timing == 0u) {
            CHECK(gbl_machine_advance_to(machine, sample_cycle) ==
                  GBL_TIME_STATUS_OK);
        }
        CHECK(ppu_pixel_matches(machine->video_frame, 1u, 0u, expected));
        if (timing == 1u) {
            uint8_t wrong_order_pixel[2] = { UINT8_C(0x5A), UINT8_C(0x5A) };

            CHECK(ppu_pixel_matches(machine->video_frame, 1u, 0u,
                                    UINT16_C(0x1234)));
            CHECK(!ppu_pixel_matches(wrong_order_pixel, 0u, 0u,
                                     UINT16_C(0x1234)));
            puts("ppu.latch control: same-cycle write is rejected as visible");
        }
        free_machine(machine);
    }

    for (timing = 0u; timing < 3u; ++timing) {
        gbl_machine *machine = new_machine();
        const uint64_t sample_cycle = UINT64_C(1232);
        const uint64_t completion = completion_cycles[timing];
        const bool sampled = timing != 0u;
        gbl_bus_result result;

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        machine->dispcnt = UINT16_C(0x0403);
        machine->ppu.dispcnt = UINT16_C(0x0403);
        machine->vram[GBL_PPU_ROW_BYTES] = UINT8_C(0x34);
        machine->vram[GBL_PPU_ROW_BYTES + 1u] = UINT8_C(0x12);
        if (timing == 2u) {
            CHECK(gbl_machine_advance_to(machine, sample_cycle) ==
                  GBL_TIME_STATUS_OK);
        } else {
            CHECK(gbl_machine_advance_to(machine, completion - 1u) ==
                  GBL_TIME_STATUS_OK);
        }
        result = gbl_bus_write(machine, UINT32_C(0x04000000), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x0003));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        if (timing == 0u) {
            CHECK(gbl_machine_advance_to(machine, sample_cycle) ==
                  GBL_TIME_STATUS_OK);
        }
        /* Disabling bitmap BG2 no longer invalidates the row: every timing
         * ordering publishes either the sampled bitmap or the backdrop. */
        CHECK(machine->ppu.captured_rows == 2u);
        CHECK(ppu_pixel_matches(machine->video_frame, 1u, 0u,
                                sampled ? UINT16_C(0x1234) : 0u));
        free_machine(machine);
    }

    {
        gbl_machine *alias = new_machine();
        const size_t row = (size_t)137u * GBL_PPU_ROW_BYTES;
        const uint64_t sample_cycle = UINT64_C(137) * UINT64_C(1232);
        gbl_bus_result result;

        CHECK(alias != NULL);
        if (alias != NULL) {
            ppu_seed_supported_frame(alias);
            alias->vram[row] = 0u;
            alias->vram[row + 1u] = 0u;
            CHECK(gbl_machine_advance_to(alias, sample_cycle - 2u) ==
                  GBL_TIME_STATUS_OK);
            result = gbl_bus_write(alias, UINT32_C(0x060180E0), 2u,
                                   GBL_BUS_CLASS_NONSEQUENTIAL,
                                   UINT32_C(0x4567));
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK(gbl_machine_advance_to(alias, sample_cycle) ==
                  GBL_TIME_STATUS_OK);
            CHECK(alias->vram[row] == UINT8_C(0x67));
            CHECK(alias->vram[row + 1u] == UINT8_C(0x45));
            CHECK(ppu_pixel_matches(alias->video_frame, 137u, 0u,
                                    UINT16_C(0x4567)));
            free_machine(alias);
        }
    }

    /* A same-cycle control write follows the row-start event. The prepared
     * row retains the register bank sampled before either write. */
    {
        gbl_machine *sampled = new_machine();
        gbl_machine_time_preview sampled_preview;
        gbl_bus_result result;
        const gbl_time_operation writes[2] = {
            { GBL_TIME_OPERATION_BUS_WRITE, 1u,
              UINT32_C(0x04000050), UINT32_C(0x30), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 2u,
              UINT32_C(0x04000000), UINT32_C(0x0503), UINT64_C(1) }
        };

        CHECK(sampled != NULL);
        if (sampled != NULL) {
            sampled->dispcnt = UINT16_C(0x0403);
            sampled->ppu.dispcnt = UINT16_C(0x0403);
            sampled->vram[GBL_PPU_ROW_BYTES] = UINT8_C(0x34);
            sampled->vram[GBL_PPU_ROW_BYTES + 1u] = UINT8_C(0x12);
            result = gbl_bus_write(sampled, UINT32_C(0x04000000), 2u,
                                   GBL_BUS_CLASS_NONSEQUENTIAL,
                                   UINT32_C(0x0403));
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK(gbl_machine_advance_to(sampled, UINT64_C(1231)) ==
                  GBL_TIME_STATUS_OK);
            CHECK(gbl_machine_prepare_operations(sampled, writes, 2u,
                                                 &sampled_preview) ==
                  GBL_TIME_STATUS_OK);
            CHECK(sampled_preview.ppu_row_capture_count == 1u);
            CHECK(sampled_preview.ppu_row_captures[0].line == 1u);
            CHECK(sampled_preview.ppu_row_captures[0].dispcnt ==
                  UINT16_C(0x0403));
            CHECK(sampled_preview.ppu_row_captures[0]
                      .render_register_bytes[18] == 0u);
            CHECK(sampled_preview.ppu_row_captures[0]
                      .unsupported_register_bytes == 0u);
            CHECK(sampled_preview.ppu.render_register_bytes[18] ==
                  UINT8_C(0x30));
            CHECK((sampled_preview.ppu.unsupported_register_bytes &
                   (UINT32_C(1) << 18u)) != 0u);
            CHECK(sampled_preview.ppu.dispcnt == UINT16_C(0x0503));
            gbl_machine_commit_time(sampled, &sampled_preview);
            CHECK(ppu_pixel_matches(sampled->video_frame, 1u, 0u,
                                    UINT16_C(0x1234)));
            free_machine(sampled);
        }
    }

    if (CHECK_DONE() == 0) {
        puts("ppu.latch: timed display and VRAM writes follow the row sample");
        return 0;
    }
    return 1;
}

static const uint8_t *ppu_test_video_memory(const gbl_machine *machine,
                                           gbl_bus_region region)
{
    if (region == GBL_BUS_REGION_PRAM) {
        return machine->pram;
    }
    if (region == GBL_BUS_REGION_VRAM) {
        return machine->vram;
    }
    if (region == GBL_BUS_REGION_OAM) {
        return machine->oam;
    }
    return NULL;
}

static bool ppu_sample_matches_bytes(
    const gbl_machine *machine, const gbl_machine_time_preview *preview,
    const gbl_ppu_row_capture *capture, gbl_bus_region region,
    uint32_t source_offset, uint8_t access_width, size_t length,
    const uint8_t *expected)
{
    uint8_t actual[8];

    if (length > sizeof(actual) || expected == NULL) {
        return false;
    }
    memset(actual, UINT8_C(0xA5), sizeof(actual));
    if (gbl_ppu_sample_read(machine, preview, capture, region, source_offset,
                            access_width, length, actual,
                            sizeof(actual)) != GBL_PPU_SAMPLE_STATUS_OK) {
        return false;
    }
    return memcmp(actual, expected, length) == 0;
}

static void ppu_check_sample_prefix_case(
    gbl_bus_region region, uint32_t source_offset,
    const gbl_time_operation *before, const gbl_time_operation *on_boundary,
    const gbl_time_operation *after)
{
    gbl_machine *machine = new_machine();
    gbl_machine *canonical = new_machine();
    const gbl_time_operation operations[3] = {
        *before, *on_boundary, *after
    };
    gbl_machine_time_preview preview;
    gbl_bus_result result;
    const uint8_t *memory;
    uint8_t expected[2];
    uint8_t actual[4];

    CHECK(machine != NULL && canonical != NULL);
    if (machine == NULL || canonical == NULL) {
        free_machine(canonical);
        free_machine(machine);
        return;
    }
    machine->dispcnt = UINT16_C(0x0403);
    machine->ppu.dispcnt = UINT16_C(0x0403);
    canonical->dispcnt = UINT16_C(0x0403);
    canonical->ppu.dispcnt = UINT16_C(0x0403);
    memset(machine->pram, UINT8_C(0xA5), sizeof(machine->pram));
    memset(machine->vram, UINT8_C(0x5A), sizeof(machine->vram));
    memset(machine->oam, UINT8_C(0xA5), sizeof(machine->oam));
    memcpy(canonical->pram, machine->pram, sizeof(machine->pram));
    memcpy(canonical->vram, machine->vram, sizeof(machine->vram));
    memcpy(canonical->oam, machine->oam, sizeof(machine->oam));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(1230)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to(canonical, UINT64_C(1230)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_prepare_operations(machine, operations, 3u, &preview) ==
          GBL_TIME_STATUS_OK);
    CHECK(preview.ppu_row_capture_count == 1u);
    CHECK(preview.ppu_row_captures[0].line == 1u);
    CHECK(preview.ppu_row_captures[0].prior_write_count == 1u);
    CHECK(preview.ppu_memory_write_count == 3u);
    CHECK(preview.ppu_memory_writes[0].cycle == UINT64_C(1231));
    CHECK(preview.ppu_memory_writes[1].cycle == UINT64_C(1232));
    CHECK(preview.ppu_memory_writes[2].cycle == UINT64_C(1233));

    result = gbl_bus_write(canonical, before->address, before->width,
                           GBL_BUS_CLASS_NONSEQUENTIAL, before->value);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.cycles == 1u);
    memory = ppu_test_video_memory(canonical, region);
    CHECK(memory != NULL);
    if (memory != NULL) {
        memcpy(expected, &memory[source_offset], sizeof(expected));
        memset(actual, UINT8_C(0xA5), sizeof(actual));
        CHECK(gbl_ppu_sample_read(
                  machine, &preview, &preview.ppu_row_captures[0], region,
                  source_offset, 2u, sizeof(expected), actual,
                  sizeof(actual)) == GBL_PPU_SAMPLE_STATUS_OK);
        CHECK(memcmp(actual, expected, sizeof(expected)) == 0);
        CHECK(actual[2] == UINT8_C(0xA5));
        CHECK(actual[3] == UINT8_C(0xA5));
    }

    result = gbl_bus_write(canonical, on_boundary->address,
                           on_boundary->width, GBL_BUS_CLASS_NONSEQUENTIAL,
                           on_boundary->value);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.cycles == 1u);
    result = gbl_bus_write(canonical, after->address, after->width,
                           GBL_BUS_CLASS_NONSEQUENTIAL, after->value);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.cycles == 1u);
    if (region == GBL_BUS_REGION_VRAM) {
        const uint16_t expected_pixel = (uint16_t)(
            ((uint16_t)expected[0] |
             (uint16_t)((uint16_t)expected[1] << 8u)) &
            UINT16_C(0x7FFF));

        CHECK(ppu_pixel_matches(canonical->video_frame, 1u, 0u,
                                expected_pixel));
    }
    free_machine(canonical);
    free_machine(machine);
}

static int run_ppu_sample(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *canonical = new_machine();
    gbl_machine_time_preview preview;
    gbl_machine_time_preview invalid_preview;
    const gbl_time_operation operations[11] = {
        { GBL_TIME_OPERATION_BUS_WRITE, 1u, UINT32_C(0x05000021),
          UINT32_C(0x5A), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 1u, UINT32_C(0x05000022),
          UINT32_C(0xBEEF), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 4u, UINT32_C(0x050003FE),
          UINT32_C(0x44332211), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 1u, UINT32_C(0x06000021),
          UINT32_C(0x6B), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x06000022),
          UINT32_C(0xCAFE), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 4u, UINT32_C(0x06017FFE),
          UINT32_C(0x88776655), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 1u, UINT32_C(0x07000021),
          UINT32_C(0x7C), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x07000022),
          UINT32_C(0x1234), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 4u, UINT32_C(0x070003FE),
          UINT32_C(0x88776655), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x060001E0),
          UINT32_C(0x1234), UINT64_C(1) },
        { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x07000024),
          UINT32_C(0x5678), UINT64_C(1) }
    };
    const uint8_t expected_pram[4] = {
        UINT8_C(0x5A), UINT8_C(0x5A), UINT8_C(0xEF), UINT8_C(0xEF)
    };
    const uint8_t expected_vram[4] = {
        UINT8_C(0x6B), UINT8_C(0x6B), UINT8_C(0xFE), UINT8_C(0xCA)
    };
    const uint8_t expected_oam[4] = {
        UINT8_C(0xA0), UINT8_C(0xA1), UINT8_C(0x34), UINT8_C(0x12)
    };
    const uint8_t expected_edge_low[2] = {
        UINT8_C(0x11), UINT8_C(0x22)
    };
    const uint8_t expected_edge_high[2] = {
        UINT8_C(0x33), UINT8_C(0x44)
    };
    const uint8_t expected_vram_folded[2] = {
        UINT8_C(0x55), UINT8_C(0x66)
    };
    const uint8_t expected_vram_mirror[2] = {
        UINT8_C(0x77), UINT8_C(0x88)
    };
    const uint8_t expected_row_sample[2] = {
        UINT8_C(0x5A), UINT8_C(0x5A)
    };
    uint8_t output[4];
    uint8_t sentinel[4];
    gbl_bus_result result;
    size_t index;

    CHECK(machine != NULL && canonical != NULL);
    if (machine == NULL || canonical == NULL) {
        free_machine(canonical);
        free_machine(machine);
        return 1;
    }
    machine->dispcnt = UINT16_C(0x0403);
    machine->ppu.dispcnt = UINT16_C(0x0403);
    canonical->dispcnt = UINT16_C(0x0403);
    canonical->ppu.dispcnt = UINT16_C(0x0403);
    memset(machine->pram, UINT8_C(0xA5), sizeof(machine->pram));
    memset(machine->vram, UINT8_C(0x5A), sizeof(machine->vram));
    memset(machine->oam, UINT8_C(0xA5), sizeof(machine->oam));
    machine->oam[0x20u] = UINT8_C(0xA0);
    machine->oam[0x21u] = UINT8_C(0xA1);
    memcpy(canonical->pram, machine->pram, sizeof(machine->pram));
    memcpy(canonical->vram, machine->vram, sizeof(machine->vram));
    memcpy(canonical->oam, machine->oam, sizeof(machine->oam));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(1222)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to(canonical, UINT64_C(1222)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_prepare_operations(machine, operations, 11u, &preview) ==
          GBL_TIME_STATUS_OK);
    CHECK(preview.master_cycles == UINT64_C(1233));
    CHECK(preview.ppu_row_capture_count == 1u);
    CHECK(preview.ppu_row_captures[0].line == 1u);
    CHECK(preview.ppu_row_captures[0].prior_write_count == 9u);
    CHECK(preview.ppu_memory_write_count == 11u);
        CHECK(preview.ppu_memory_writes[8].cycle == UINT64_C(1231));
        CHECK(preview.ppu_memory_writes[9].cycle == UINT64_C(1232));

    for (index = 0u; index < 9u; ++index) {
        result = gbl_bus_write(canonical, operations[index].address,
                               operations[index].width,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               operations[index].value);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
        CHECK(result.cycles == 1u);
    }
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_PRAM, UINT32_C(0x20), 2u, sizeof(expected_pram),
        canonical->pram + 0x20u));
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_VRAM, UINT32_C(0x20), 2u, sizeof(expected_vram),
        canonical->vram + 0x20u));
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_OAM, UINT32_C(0x20), 2u, sizeof(expected_oam),
        canonical->oam + 0x20u));
    CHECK(memcmp(canonical->pram + 0x20u, expected_pram,
                 sizeof(expected_pram)) == 0);
    CHECK(memcmp(canonical->vram + 0x20u, expected_vram,
                 sizeof(expected_vram)) == 0);
    CHECK(memcmp(canonical->oam + 0x20u, expected_oam,
                 sizeof(expected_oam)) == 0);
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_PRAM, UINT32_C(0x3FE), 2u,
        sizeof(expected_edge_low), canonical->pram + 0x3FEu));
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_PRAM, 0u, 2u, sizeof(expected_edge_high),
        canonical->pram));
    CHECK(memcmp(canonical->pram + 0x3FEu, expected_edge_low,
                 sizeof(expected_edge_low)) == 0);
    CHECK(memcmp(canonical->pram, expected_edge_high,
                 sizeof(expected_edge_high)) == 0);
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_VRAM, UINT32_C(0x17FFE), 2u,
        sizeof(expected_vram_folded), canonical->vram + 0x17FFEu));
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_VRAM, UINT32_C(0x10000), 2u,
        sizeof(expected_vram_mirror), canonical->vram + 0x10000u));
    CHECK(memcmp(canonical->vram + 0x17FFEu, expected_vram_folded,
                 sizeof(expected_vram_folded)) == 0);
    CHECK(memcmp(canonical->vram + 0x10000u, expected_vram_mirror,
                 sizeof(expected_vram_mirror)) == 0);
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_OAM, UINT32_C(0x3FE), 2u,
        sizeof(expected_vram_folded), canonical->oam + 0x3FEu));
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_OAM, 0u, 2u, sizeof(expected_vram_mirror),
        canonical->oam));
    CHECK(memcmp(canonical->oam + 0x3FEu, expected_vram_folded,
                 sizeof(expected_vram_folded)) == 0);
    CHECK(memcmp(canonical->oam, expected_vram_mirror,
                 sizeof(expected_vram_mirror)) == 0);
    CHECK(ppu_sample_matches_bytes(
        machine, &preview, &preview.ppu_row_captures[0],
        GBL_BUS_REGION_VRAM, UINT32_C(0x1E0), 2u,
        sizeof(expected_row_sample), canonical->vram + 0x1E0u));
    CHECK(memcmp(canonical->vram + 0x1E0u, expected_row_sample,
                 sizeof(expected_row_sample)) == 0);

    for (index = 9u; index < 11u; ++index) {
        result = gbl_bus_write(canonical, operations[index].address,
                               operations[index].width,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               operations[index].value);
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
        CHECK(result.cycles == 1u);
    }
    CHECK(canonical->vram[0x1E0u] == UINT8_C(0x34));
    CHECK(canonical->vram[0x1E1u] == UINT8_C(0x12));
    CHECK(canonical->ppu.line == 1u);
    CHECK(ppu_pixel_matches(canonical->video_frame, 1u, 0u,
                            UINT16_C(0x5A5A)));
    CHECK(canonical->oam[0x24u] == UINT8_C(0x78));
    CHECK(canonical->oam[0x25u] == UINT8_C(0x56));

    memset(output, UINT8_C(0xA5), sizeof(output));
    memcpy(sentinel, output, sizeof(output));
    CHECK(gbl_ppu_sample_read(
              machine, &preview, &preview.ppu_row_captures[0],
              GBL_BUS_REGION_EWRAM, 0u, 2u, 2u, output,
              sizeof(output)) == GBL_PPU_SAMPLE_STATUS_INVALID_REGION);
    CHECK(memcmp(output, sentinel, sizeof(output)) == 0);
    CHECK(gbl_ppu_sample_read(
              machine, &preview, &preview.ppu_row_captures[0],
              GBL_BUS_REGION_VRAM, 0u, 2u, 2u, output, 1u) ==
          GBL_PPU_SAMPLE_STATUS_OUTPUT_TOO_SMALL);
    CHECK(memcmp(output, sentinel, sizeof(output)) == 0);
    CHECK(gbl_ppu_sample_read(
              machine, &preview, &preview.ppu_row_captures[0],
              GBL_BUS_REGION_VRAM, UINT32_C(0x17FFF), 2u, 2u, output,
              sizeof(output)) == GBL_PPU_SAMPLE_STATUS_INVALID_SPAN);
    CHECK(memcmp(output, sentinel, sizeof(output)) == 0);
    CHECK(gbl_ppu_sample_read(
              machine, &preview, &preview.ppu_row_captures[0],
              GBL_BUS_REGION_VRAM, 0u, 3u, 3u, output,
              sizeof(output)) == GBL_PPU_SAMPLE_STATUS_INVALID_ARGUMENT);
    CHECK(memcmp(output, sentinel, sizeof(output)) == 0);
    invalid_preview = preview;
    invalid_preview.ppu_row_captures[0].prior_write_count =
        (uint16_t)(preview.ppu_memory_write_count + 1u);
    CHECK(gbl_ppu_sample_read(
              machine, &invalid_preview,
              &invalid_preview.ppu_row_captures[0], GBL_BUS_REGION_PRAM,
              0u, 2u, 2u, output, sizeof(output)) ==
          GBL_PPU_SAMPLE_STATUS_INVALID_PREFIX);
    CHECK(memcmp(output, sentinel, sizeof(output)) == 0);
    invalid_preview = preview;
    invalid_preview.ppu_memory_writes[0].width = 3u;
    CHECK(gbl_ppu_sample_read(
              machine, &invalid_preview,
              &invalid_preview.ppu_row_captures[0], GBL_BUS_REGION_PRAM,
              0u, 2u, 2u, output, sizeof(output)) ==
          GBL_PPU_SAMPLE_STATUS_INVALID_WRITE);
    CHECK(memcmp(output, sentinel, sizeof(output)) == 0);

    {
        const gbl_time_operation pram[3] = {
            { GBL_TIME_OPERATION_BUS_WRITE, 1u, UINT32_C(0x05000021),
              UINT32_C(0x5A), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x05000022),
              UINT32_C(0xBEEF), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 4u, UINT32_C(0x050003FE),
              UINT32_C(0x44332211), UINT64_C(1) }
        };
        const gbl_time_operation vram[3] = {
            { GBL_TIME_OPERATION_BUS_WRITE, 1u, UINT32_C(0x060001E1),
              UINT32_C(0x6B), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x060001E0),
              UINT32_C(0x1234), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 4u, UINT32_C(0x06017FFE),
              UINT32_C(0x88776655), UINT64_C(1) }
        };
        const gbl_time_operation oam[3] = {
            { GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x07000020),
              UINT32_C(0xCAFE), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 1u, UINT32_C(0x07000021),
              UINT32_C(0x77), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 4u, UINT32_C(0x070003FE),
              UINT32_C(0x88776655), UINT64_C(1) }
        };

        ppu_check_sample_prefix_case(GBL_BUS_REGION_PRAM, UINT32_C(0x20),
                                     &pram[0], &pram[1], &pram[2]);
        ppu_check_sample_prefix_case(GBL_BUS_REGION_VRAM,
                                     UINT32_C(0x1E0), &vram[0], &vram[1],
                                     &vram[2]);
        ppu_check_sample_prefix_case(GBL_BUS_REGION_OAM, UINT32_C(0x20),
                                     &oam[0], &oam[1], &oam[2]);
    }

    free_machine(canonical);
    free_machine(machine);
    if (CHECK_DONE() == 0) {
        puts("ppu.sample: PRAM, VRAM and OAM share ordered lane projection");
        return 0;
    }
    return 1;
}

static int run_ppu_bounds(void)
{
    gbl_machine *machine = new_machine();
    gbl_machine *snapshot = malloc(sizeof(*snapshot));
    gbl_time_operation operations[GBL_TIME_OPERATION_MAX + 1u];
    gbl_machine_time_preview preview;
    gbl_machine_time_preview preview_sentinel;
    gbl_machine_time_preview invalid_preview;
    gbl_time_status time_status;
    size_t index;

    CHECK(machine != NULL && snapshot != NULL);
    if (machine == NULL || snapshot == NULL) {
        free(snapshot);
        free_machine(machine);
        return 1;
    }
    memcpy(snapshot, machine, sizeof(*snapshot));
    for (index = 0u; index < GBL_TIME_OPERATION_MAX; ++index) {
        operations[index] = (gbl_time_operation){
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1)
        };
    }
    CHECK(gbl_machine_prepare_operations(machine, operations,
                                         GBL_TIME_OPERATION_MAX,
                                         &preview) == GBL_TIME_STATUS_OK);
    CHECK(preview.master_cycles == GBL_TIME_OPERATION_MAX);
    CHECK(gbl_test_canonical_equal(machine, snapshot));

    memset(&preview, UINT8_C(0xA5), sizeof(preview));
    memcpy(&preview_sentinel, &preview, sizeof(preview));
    operations[GBL_TIME_OPERATION_MAX] = (gbl_time_operation){
        GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1)
    };
    time_status = gbl_machine_prepare_operations(
        machine, operations, GBL_TIME_OPERATION_MAX + 1u, &preview);
    CHECK(time_status == GBL_TIME_STATUS_INVALID_ARGUMENT);
    CHECK(memcmp(&preview, &preview_sentinel, sizeof(preview)) == 0);
    CHECK(gbl_test_canonical_equal(machine, snapshot));

    {
        const gbl_time_operation invalid_tail[2] = {
            { GBL_TIME_OPERATION_BUS_WRITE, 2u,
              UINT32_C(0x060001E0), UINT32_C(0x1234), UINT64_C(1) },
            { GBL_TIME_OPERATION_BUS_WRITE, 3u,
              UINT32_C(0x060001E0), UINT32_C(0x5678), UINT64_C(1) }
        };

        time_status = gbl_machine_prepare_operations(
            machine, invalid_tail, 2u, &preview);
        CHECK(time_status == GBL_TIME_STATUS_INVALID_ARGUMENT);
        CHECK(memcmp(&preview, &preview_sentinel, sizeof(preview)) == 0);
        CHECK(gbl_test_canonical_equal(machine, snapshot));
    }
    free(snapshot);
    free_machine(machine);

    {
        gbl_machine *sampled = new_machine();
        gbl_machine *canonical = new_machine();
        gbl_machine *before = malloc(sizeof(*before));
        gbl_bus_result result;
        uint16_t expected_at_sample = 0u;
        uint8_t sample_sentinel[2] = { UINT8_C(0xA5), UINT8_C(0x5A) };
        uint8_t sample_output[2];

        CHECK(sampled != NULL && canonical != NULL && before != NULL);
        if (sampled == NULL || canonical == NULL || before == NULL) {
            free(before);
            free_machine(canonical);
            free_machine(sampled);
            return 1;
        }
        sampled->dispcnt = UINT16_C(0x0403);
        sampled->ppu.dispcnt = UINT16_C(0x0403);
        canonical->dispcnt = UINT16_C(0x0403);
        canonical->ppu.dispcnt = UINT16_C(0x0403);
        CHECK(gbl_machine_advance_to(sampled, UINT64_C(1220)) ==
              GBL_TIME_STATUS_OK);
        CHECK(gbl_machine_advance_to(canonical, UINT64_C(1220)) ==
              GBL_TIME_STATUS_OK);
        memcpy(before, sampled, sizeof(*before));
        /* The write-log cap equals the admitted-operation cap: a 21st valid
         * video write cannot exist, so the operation over-bound above also
         * proves the derived write bound cannot be exceeded independently. */
        for (index = 0u; index < GBL_TIME_OPERATION_MAX; ++index) {
            const uint16_t color = (uint16_t)(index + 1u);

            operations[index] = (gbl_time_operation){
                GBL_TIME_OPERATION_BUS_WRITE, 2u, UINT32_C(0x060001E0),
                color, UINT64_C(1)
            };
        }
        CHECK(gbl_machine_prepare_operations(
                  sampled, operations, GBL_TIME_OPERATION_MAX, &preview) ==
              GBL_TIME_STATUS_OK);
        CHECK(preview.ppu_memory_write_count == GBL_TIME_OPERATION_MAX);
        CHECK(preview.ppu_row_capture_count == 1u);
        CHECK(preview.ppu_row_captures[0].line == 1u);
        CHECK(preview.ppu_row_captures[0].prior_write_count == 11u);
        CHECK(preview.ppu_memory_writes[10].cycle == UINT64_C(1231));
        CHECK(preview.ppu_memory_writes[11].cycle == UINT64_C(1232));
        CHECK(gbl_test_canonical_equal(sampled, before));

        invalid_preview = preview;
        invalid_preview.ppu_memory_write_count =
            GBL_TIME_OPERATION_MAX + 1u;
        memcpy(sample_output, sample_sentinel, sizeof(sample_output));
        CHECK(gbl_ppu_sample_read(
                  sampled, &invalid_preview,
                  &invalid_preview.ppu_row_captures[0],
                  GBL_BUS_REGION_VRAM, GBL_PPU_ROW_BYTES, 2u,
                  sizeof(sample_output), sample_output,
                  sizeof(sample_output)) ==
              GBL_PPU_SAMPLE_STATUS_INVALID_PREFIX);
        CHECK(memcmp(sample_output, sample_sentinel,
                     sizeof(sample_output)) == 0);

        invalid_preview = preview;
        --invalid_preview.ppu_row_captures[0].prior_write_count;
        memcpy(sample_output, sample_sentinel, sizeof(sample_output));
        CHECK(gbl_ppu_sample_read(
                  sampled, &invalid_preview,
                  &invalid_preview.ppu_row_captures[0],
                  GBL_BUS_REGION_VRAM, GBL_PPU_ROW_BYTES, 2u,
                  sizeof(sample_output), sample_output,
                  sizeof(sample_output)) ==
              GBL_PPU_SAMPLE_STATUS_INVALID_PREFIX);
        CHECK(memcmp(sample_output, sample_sentinel,
                     sizeof(sample_output)) == 0);

        invalid_preview = preview;
        --invalid_preview.ppu_row_captures[0].prior_write_count;
        gbl_machine_commit_time(sampled, &invalid_preview);
        CHECK(gbl_test_canonical_equal(sampled, before));
        invalid_preview = preview;
        invalid_preview.ppu_row_captures[0].prior_write_count =
            (uint16_t)(preview.ppu_memory_write_count + 1u);
        gbl_machine_commit_time(sampled, &invalid_preview);
        CHECK(gbl_test_canonical_equal(sampled, before));
        invalid_preview = preview;
        invalid_preview.ppu_memory_writes[0].width = 3u;
        gbl_machine_commit_time(sampled, &invalid_preview);
        CHECK(gbl_test_canonical_equal(sampled, before));

        gbl_machine_commit_time(sampled, &preview);
        expected_at_sample = UINT16_C(11);
        CHECK(ppu_pixel_matches(sampled->video_frame, 1u, 0u,
                                expected_at_sample));
        CHECK(sampled->vram[GBL_PPU_ROW_BYTES] == 0u);
        for (index = 0u; index < GBL_TIME_OPERATION_MAX; ++index) {
            result = gbl_bus_write(
                canonical, operations[index].address, operations[index].width,
                GBL_BUS_CLASS_NONSEQUENTIAL, operations[index].value);
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
            CHECK(result.cycles == 1u);
        }
        CHECK(canonical->master_cycles == UINT64_C(1240));
        CHECK(ppu_pixel_matches(canonical->video_frame, 1u, 0u,
                                expected_at_sample));
        free(before);
        free_machine(canonical);
        free_machine(sampled);
    }

    {
        gbl_machine *epoch = new_machine();
        gbl_machine *before = malloc(sizeof(*before));
        gbl_time_operation advance_all_rows = {
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(197119)
        };
        gbl_time_operation finish_epoch = {
            GBL_TIME_OPERATION_INTERNAL, 0u, 0u, 0u, UINT64_C(1)
        };
        gbl_bus_result result;

        CHECK(epoch != NULL && before != NULL);
        if (epoch == NULL || before == NULL) {
            free(before);
            free_machine(epoch);
            return 1;
        }
        ppu_seed_supported_frame(epoch);
        memcpy(before, epoch, sizeof(*before));
        CHECK(gbl_machine_prepare_operations(epoch, &advance_all_rows, 1u,
                                             &preview) ==
              GBL_TIME_STATUS_OK);
        CHECK(preview.ppu_row_capture_count == GBL_PPU_ROW_CAPTURE_MAX);
        CHECK(preview.ppu.captured_rows == GBL_PPU_VISIBLE_LINES);
        CHECK(preview.ppu.capture_state == GBL_VIDEO_CAPTURE_INCOMPLETE);
        CHECK(gbl_test_canonical_equal(epoch, before));

        invalid_preview = preview;
        invalid_preview.ppu_row_capture_count =
            GBL_PPU_ROW_CAPTURE_MAX + 1u;
        gbl_machine_commit_time(epoch, &invalid_preview);
        CHECK(gbl_test_canonical_equal(epoch, before));
        invalid_preview = preview;
        invalid_preview.ppu_memory_write_count =
            GBL_TIME_OPERATION_MAX + 1u;
        gbl_machine_commit_time(epoch, &invalid_preview);
        CHECK(gbl_test_canonical_equal(epoch, before));
        invalid_preview = preview;
        invalid_preview.ppu_row_captures[0].prior_write_count = 1u;
        gbl_machine_commit_time(epoch, &invalid_preview);
        CHECK(gbl_test_canonical_equal(epoch, before));
        invalid_preview = preview;
        invalid_preview.ppu_row_captures[0].dispcnt = UINT16_C(0x0402);
        gbl_machine_commit_time(epoch, &invalid_preview);
        CHECK(gbl_test_canonical_equal(epoch, before));
        invalid_preview = preview;
        invalid_preview.ppu_row_captures[0].unsupported_register_bytes =
            UINT32_C(1) << 18u;
        gbl_machine_commit_time(epoch, &invalid_preview);
        CHECK(gbl_test_canonical_equal(epoch, before));

        gbl_machine_commit_time(epoch, &preview);
        CHECK(epoch->ppu.capture_state == GBL_VIDEO_CAPTURE_INCOMPLETE);
        CHECK(epoch->ppu.captured_rows == GBL_PPU_VISIBLE_LINES);
        CHECK(gbl_machine_prepare_operations(epoch, &finish_epoch, 1u,
                                             &preview) ==
              GBL_TIME_STATUS_OK);
        CHECK(preview.ppu_row_capture_count == 0u);
        CHECK(preview.ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        gbl_machine_commit_time(epoch, &preview);
        CHECK(epoch->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);

        CHECK(gbl_machine_advance_to(epoch, GBL_PPU_FRAME_CYCLES) ==
              GBL_TIME_STATUS_OK);
        CHECK(epoch->ppu.frame_epoch == 1u);
        CHECK(epoch->ppu.captured_rows == 1u);
        CHECK(epoch->ppu.capture_state == GBL_VIDEO_CAPTURE_INCOMPLETE);
        result = gbl_bus_write(epoch, UINT32_C(0x04000000), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x0402));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(gbl_machine_advance_to(
                  epoch, GBL_PPU_FRAME_CYCLES +
                             (GBL_PPU_LINE_CYCLES * UINT64_C(2))) ==
              GBL_TIME_STATUS_OK);
        CHECK(epoch->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID);
        CHECK(epoch->ppu.capture_reason ==
              GBL_VIDEO_INVALID_REASON_UNSUPPORTED_MODE);
        result = gbl_bus_write(epoch, UINT32_C(0x04000000), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0x0403));
        CHECK(result.status == GBL_BUS_STATUS_OK);
        CHECK(gbl_machine_advance_to(
                  epoch, (GBL_PPU_FRAME_CYCLES * UINT64_C(2)) +
                             UINT64_C(197120)) == GBL_TIME_STATUS_OK);
        CHECK(epoch->ppu.frame_epoch == 2u);
        CHECK(epoch->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        CHECK(epoch->ppu.captured_rows == GBL_PPU_VISIBLE_LINES);
        CHECK(ppu_frame_matches_pattern(epoch->video_frame));
        free(before);
        free_machine(epoch);
    }

    if (CHECK_DONE() == 0) {
        puts("ppu.bounds: exact-fit prefixes, refusals and fresh epochs are atomic");
        return 0;
    }
    return 1;
}

static int run_ppu_frame(void)
{
    gbl_machine *first = new_machine();
    gbl_machine *second = new_machine();
    uint8_t *control = malloc(GBL_PPU_VIDEO_BYTES);
    uint8_t row_scratch[GBL_PPU_ROW_BYTES];

    if (first == NULL || second == NULL || control == NULL) {
        fprintf(stderr, "ppu.frame: allocation failed\n");
        free(control);
        free_machine(second);
        free_machine(first);
        return 1;
    }
    ppu_seed_supported_frame(first);
    ppu_seed_supported_frame(second);
    CHECK(gbl_machine_advance_to(first, UINT64_C(197119)) ==
          GBL_TIME_STATUS_OK);
    CHECK(first->ppu.captured_rows == GBL_PPU_VISIBLE_LINES);
    CHECK(first->ppu.capture_state == GBL_VIDEO_CAPTURE_INCOMPLETE);
    CHECK(first->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_INCOMPLETE);
    CHECK(gbl_machine_advance_to(second, UINT64_C(100000)) ==
          GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to(first, UINT64_C(197120)) ==
          GBL_TIME_STATUS_OK);
    CHECK(first->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
    CHECK(first->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_NONE);
    CHECK(gbl_machine_advance_to(second, UINT64_C(197120)) ==
          GBL_TIME_STATUS_OK);
    CHECK(second->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
    CHECK(second->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_NONE);
    CHECK(first->ppu.frame_epoch == 0u);
    CHECK(second->ppu.frame_epoch == 0u);
    CHECK(memcmp(first->video_frame, second->video_frame,
                 GBL_PPU_VIDEO_BYTES) == 0);
    CHECK(ppu_frame_matches_pattern(first->video_frame));
    CHECK(first->video_frame[0] == UINT8_C(0x1F));
    CHECK(first->video_frame[1] == 0u);
    CHECK(first->video_frame[2] == UINT8_C(0xE0));
    CHECK(first->video_frame[3] == UINT8_C(0x03));
    CHECK(first->video_frame[GBL_PPU_ROW_BYTES] == UINT8_C(0xE0));
    CHECK(first->video_frame[GBL_PPU_ROW_BYTES + 1u] == UINT8_C(0x03));
    CHECK(first->video_frame[GBL_PPU_VIDEO_BYTES - 2u] == 0u);
    CHECK(first->video_frame[GBL_PPU_VIDEO_BYTES - 1u] == UINT8_C(0x7C));

    memcpy(control, first->video_frame, GBL_PPU_VIDEO_BYTES);
    CHECK(ppu_frame_matches_pattern(control));
    control[0] = UINT8_C(0xE0);
    control[1] = UINT8_C(0x03);
    CHECK(!ppu_frame_matches_pattern(control));
    puts("ppu.frame control: wrong color is rejected by pixel expectations");
    memcpy(control, first->video_frame, GBL_PPU_VIDEO_BYTES);
    memcpy(row_scratch, control, GBL_PPU_ROW_BYTES);
    memcpy(control, control + GBL_PPU_ROW_BYTES, GBL_PPU_ROW_BYTES);
    memcpy(control + GBL_PPU_ROW_BYTES, row_scratch, GBL_PPU_ROW_BYTES);
    CHECK(!ppu_frame_matches_pattern(control));
    puts("ppu.frame control: wrong row address is rejected semantically");
    memcpy(control, first->video_frame, GBL_PPU_VIDEO_BYTES);
    control[GBL_PPU_VIDEO_BYTES - 2u] ^= UINT8_C(0x01);
    CHECK(!ppu_frame_matches_pattern(control));
    puts("ppu.frame control: one raw byte mutation is rejected semantically");

    CHECK(gbl_machine_advance_to(first, UINT64_C(280896)) ==
          GBL_TIME_STATUS_OK);
    CHECK(first->ppu.frame_epoch == 1u);
    CHECK(first->ppu.captured_rows == 1u);
    CHECK(first->ppu.capture_state == GBL_VIDEO_CAPTURE_INCOMPLETE);
    CHECK(first->ppu.capture_reason == GBL_VIDEO_INVALID_REASON_INCOMPLETE);
    CHECK(bytes_are_zero(first->video_frame + GBL_PPU_ROW_BYTES,
                         GBL_PPU_VIDEO_BYTES - GBL_PPU_ROW_BYTES));

    printf("{\"type\":\"ppu.frame\",\"width\":240,\"height\":160,\"format\":\"BGR555_LE\",\"bytes\":76800,\"valid_cycle\":197120}\n");
    free(control);
    free_machine(second);
    free_machine(first);
    if (CHECK_DONE() == 0) {
        puts("ppu.frame: full supported epochs publish exact raw bytes");
        return 0;
    }
    return 1;
}

static int run_ppu_controls(void)
{
    static const struct {
        uint16_t dispcnt;
        gbl_video_invalid_reason reason;
        const char *name;
    } unsupported[] = {
        { UINT16_C(0x0406), GBL_VIDEO_INVALID_REASON_UNSUPPORTED_MODE,
          "mode 6 is unsupported" },
        { UINT16_C(0x0483), GBL_VIDEO_INVALID_REASON_FORCED_BLANK,
          "forced blank is unsupported" },
        { UINT16_C(0x2403), GBL_VIDEO_INVALID_REASON_WINDOW_ENABLED,
          "OBJ window is unsupported" },
        { UINT16_C(0x4403), GBL_VIDEO_INVALID_REASON_WINDOW_ENABLED,
          "window 0 is unsupported" },
        { UINT16_C(0x8403), GBL_VIDEO_INVALID_REASON_WINDOW_ENABLED,
          "window 1 is unsupported" }
    };
    size_t index;

    for (index = 0u; index < sizeof(unsupported) / sizeof(unsupported[0]);
         ++index) {
        gbl_machine *machine = new_machine();

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        fprintf(stderr, "ppu.controls case: %s\n", unsupported[index].name);
        machine->dispcnt = unsupported[index].dispcnt;
        machine->ppu.dispcnt = unsupported[index].dispcnt;
        CHECK(gbl_machine_advance_to(machine, 0u) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID);
        CHECK(machine->ppu.capture_reason == unsupported[index].reason);
        CHECK(machine->ppu.captured_rows == 0u);
        machine->dispcnt = UINT16_C(0x0403);
        machine->ppu.dispcnt = UINT16_C(0x0403);
        CHECK(gbl_machine_advance_to(machine, UINT64_C(1232)) ==
              GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID);
        CHECK(machine->ppu.capture_reason == unsupported[index].reason);

        machine->dispcnt = unsupported[index].dispcnt;
        machine->ppu.dispcnt = unsupported[index].dispcnt;
        CHECK(gbl_machine_advance_to(
                  machine,
                  GBL_PPU_FRAME_CYCLES + UINT64_C(197120)) ==
              GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.frame_epoch == 1u);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID);
        CHECK(machine->ppu.capture_reason == unsupported[index].reason);

        machine->dispcnt = UINT16_C(0x0403);
        machine->ppu.dispcnt = UINT16_C(0x0403);
        CHECK(gbl_machine_advance_to(
                  machine,
                  (GBL_PPU_FRAME_CYCLES * UINT64_C(2)) +
                      UINT64_C(197120)) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.frame_epoch == 2u);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        free_machine(machine);
    }

    for (index = 3u; index <= 5u; ++index) {
        gbl_machine *machine = new_machine();
        const uint16_t backdrop = UINT16_C(0x4210);

        CHECK(machine != NULL);
        if (machine == NULL) {
            continue;
        }
        machine->dispcnt = (uint16_t)index;
        machine->ppu.dispcnt = (uint16_t)index;
        ppu_bg_store_u16(machine->pram, 0u, backdrop);
        CHECK(gbl_machine_advance_to(
                  machine, (uint64_t)GBL_PPU_LINE_CYCLES *
                               UINT64_C(160)) == GBL_TIME_STATUS_OK);
        CHECK(machine->ppu.capture_state == GBL_VIDEO_CAPTURE_VALID);
        CHECK(ppu_bg_affine_pixel_matches(machine->video_frame, 0u, 0u,
                                          backdrop));
        free_machine(machine);
    }

    {
        gbl_machine *effects = new_machine();
        gbl_machine *mosaic = new_machine();
        gbl_machine *affine = new_machine();
        gbl_bus_result result;

        CHECK(effects != NULL && mosaic != NULL && affine != NULL);
        if (effects != NULL) {
            effects->dispcnt = UINT16_C(0x0403);
            effects->ppu.dispcnt = UINT16_C(0x0403);
            CHECK(gbl_machine_advance_to(effects, 0u) ==
                  GBL_TIME_STATUS_OK);
            result = gbl_bus_write(effects, UINT32_C(0x04000050), 2u,
                                   GBL_BUS_CLASS_NONSEQUENTIAL,
                                   UINT32_C(0x0001));
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK(effects->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID);
            CHECK(effects->ppu.capture_reason ==
                  GBL_VIDEO_INVALID_REASON_EFFECT_ENABLED);
            puts("ppu.controls control: blending registers are unsupported");
            free_machine(effects);
        }
        if (mosaic != NULL) {
            mosaic->dispcnt = UINT16_C(0x0403);
            mosaic->ppu.dispcnt = UINT16_C(0x0403);
            CHECK(gbl_machine_advance_to(mosaic, 0u) ==
                  GBL_TIME_STATUS_OK);
            result = gbl_bus_write(mosaic, UINT32_C(0x0400004C), 2u,
                                   GBL_BUS_CLASS_NONSEQUENTIAL,
                                   UINT32_C(0x0001));
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK(mosaic->ppu.capture_state == GBL_VIDEO_CAPTURE_INVALID);
            CHECK(mosaic->ppu.capture_reason ==
                  GBL_VIDEO_INVALID_REASON_EFFECT_ENABLED);
            puts("ppu.controls control: MOSAIC is unsupported");
            free_machine(mosaic);
        }
        if (affine != NULL) {
            affine->dispcnt = UINT16_C(0x0404);
            affine->ppu.dispcnt = UINT16_C(0x0404);
            CHECK(gbl_machine_advance_to(affine, 0u) ==
                  GBL_TIME_STATUS_OK);
            result = gbl_bus_write(affine, UINT32_C(0x04000020), 2u,
                                   GBL_BUS_CLASS_NONSEQUENTIAL,
                                   UINT32_C(0x0180));
            CHECK(result.status == GBL_BUS_STATUS_OK);
            CHECK(affine->ppu.capture_state != GBL_VIDEO_CAPTURE_INVALID);
            CHECK(gbl_machine_advance_to(affine, GBL_PPU_LINE_CYCLES) ==
                  GBL_TIME_STATUS_OK);
            CHECK(affine->ppu.captured_rows == 2u);
            CHECK(affine->ppu.capture_state != GBL_VIDEO_CAPTURE_INVALID);
            puts("ppu.controls control: mode-4 BG2 affine matrix is sampled");
            free_machine(affine);
        }

        {
            gbl_machine *persistent = new_machine();

            CHECK(persistent != NULL);
            if (persistent != NULL) {
                persistent->dispcnt = UINT16_C(0x0403);
                persistent->ppu.dispcnt = UINT16_C(0x0403);
                CHECK(gbl_machine_advance_to(persistent, 0u) ==
                      GBL_TIME_STATUS_OK);
                result = gbl_bus_write(
                    persistent, UINT32_C(0x04000020), 2u,
                    GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x0180));
                CHECK(result.status == GBL_BUS_STATUS_OK);
                result = gbl_bus_write(
                    persistent, UINT32_C(0x04000024), 2u,
                    GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x0080));
                CHECK(result.status == GBL_BUS_STATUS_OK);
                result = gbl_bus_write(
                    persistent, UINT32_C(0x0400002C), 4u,
                    GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x00010000));
                CHECK(result.status == GBL_BUS_STATUS_OK);
                CHECK(persistent->ppu.capture_state !=
                      GBL_VIDEO_CAPTURE_INVALID);
                CHECK(gbl_machine_advance_to(
                          persistent, UINT64_C(197120)) ==
                      GBL_TIME_STATUS_OK);
                CHECK(persistent->ppu.capture_state ==
                      GBL_VIDEO_CAPTURE_VALID);
                CHECK(persistent->ppu.capture_reason ==
                      GBL_VIDEO_INVALID_REASON_NONE);
                puts("ppu.controls control: transformed mode-3 references complete a valid frame");
                free_machine(persistent);
            }
        }
    }

    {
        gbl_machine *inert = new_machine();

        CHECK(inert != NULL);
        if (inert != NULL) {
            inert->dispcnt = UINT16_C(0x0F63);
            inert->ppu.dispcnt = UINT16_C(0x0F63);
            CHECK(gbl_machine_advance_to(inert, 0u) ==
                  GBL_TIME_STATUS_OK);
            CHECK(inert->ppu.capture_state ==
                  GBL_VIDEO_CAPTURE_INCOMPLETE);
            CHECK(inert->ppu.capture_reason ==
                  GBL_VIDEO_INVALID_REASON_INCOMPLETE);
            CHECK(inert->ppu.captured_rows == 1u);
            free_machine(inert);
        }
    }

    if (CHECK_DONE() == 0) {
        puts("ppu.controls: unsupported display controls invalidate the epoch");
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
        const gbl_event *timer_event;
        const gbl_event *ppu_event;

        result = gbl_bus_write(machine, UINT32_C(0x04000102), 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0xC0));
        timer_event = find_bus_event(
            machine, (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW, 0u);
        ppu_event = find_bus_event(
            machine, (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY, 0u);
        CHECK(result.status == GBL_BUS_STATUS_OK && machine->event_count == 2u &&
              count_bus_events(machine,
                  (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY) == 1u &&
              count_bus_events(machine,
                  (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW) == 1u &&
              timer_event != NULL && timer_event->cycle == UINT64_C(3) &&
              timer_event->priority == 0u && ppu_event != NULL &&
              ppu_event->generation == machine->ppu.generation &&
              ppu_event->cycle == UINT64_C(1006) &&
              ppu_event->priority == 9u);
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
    const gbl_event *timer_event;
    const gbl_event *keypad_event;
    const gbl_event *ppu_event;
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
          machine->event_count == 2u);
    pressed.cycle = UINT64_C(2);
    CHECK(gbl_machine_set_input_timeline(machine, &pressed, 1u) ==
          GBL_TIME_STATUS_OK);
    timer_event = find_bus_event(
        machine, (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW, 0u);
    keypad_event = find_bus_event(
        machine, (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT, 0u);
    ppu_event = find_bus_event(
        machine, (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY, 0u);
    CHECK(machine->event_count == 3u &&
          count_bus_events(machine,
              (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY) == 1u &&
          count_bus_events(machine,
              (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW) == 1u &&
          count_bus_events(machine,
              (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT) == 1u &&
          timer_event != NULL &&
          timer_event->cycle == UINT64_C(2) && timer_event->priority == 0u &&
          keypad_event != NULL && keypad_event->cycle == UINT64_C(2) &&
          keypad_event->priority == 4u && ppu_event != NULL &&
          ppu_event->cycle == UINT64_C(1006) && ppu_event->priority == 9u &&
          ppu_event->generation == machine->ppu.generation &&
          timer_event < keypad_event && keypad_event < ppu_event);
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
    timer_event = find_bus_event(
        machine, (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW, 0u);
    keypad_event = find_bus_event(
        machine, (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT, 0u);
    ppu_event = find_bus_event(
        machine, (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY, 0u);
    CHECK(machine->event_count == 3u &&
          count_bus_events(machine,
              (uint8_t)GBL_EVENT_KIND_PPU_BOUNDARY) == 1u &&
          count_bus_events(machine,
              (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW) == 1u &&
          count_bus_events(machine,
              (uint8_t)GBL_EVENT_KIND_KEYPAD_INPUT) == 1u &&
          timer_event != NULL &&
          timer_event->cycle == UINT64_C(3) && timer_event->priority == 0u &&
          keypad_event != NULL && keypad_event->cycle == UINT64_C(3) &&
          keypad_event->priority == 4u && ppu_event != NULL &&
          ppu_event->cycle == UINT64_C(1006) && ppu_event->priority == 9u &&
          ppu_event->generation == machine->ppu.generation &&
          timer_event < keypad_event && keypad_event < ppu_event);
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

static const char *bus_access_sequence_name(gbl_bus_access_class sequence)
{
    return sequence == GBL_BUS_CLASS_SEQUENTIAL
               ? "sequential"
               : "nonsequential";
}

static const char *bus_window_name(uint8_t window)
{
    static const char *const names[3] = { "WS0", "WS1", "WS2" };

    return window < 3u ? names[window] : "none";
}

static void emit_phase9_observation(
    const gbl_phase9_bus_observation *observation)
{
    uint8_t beat;

    if (observation == NULL) {
        return;
    }
    printf("phase9-observation:{\"schema\":\"gamebloke.phase9-observation/1\",\"waitcnt\":\"0x%04X\",\"address\":\"0x%08X\",\"width\":%u,\"direction\":\"%s\",\"role\":\"%s\",\"requested_sequence\":\"%s\",\"window\":\"%s\",\"cycles\":%llu,\"beat_count\":%u,\"beat_cycles\":[",
           (unsigned int)observation->waitcnt,
           (unsigned int)observation->address,
           (unsigned int)observation->width,
           observation->direction == GBL_BUS_DIRECTION_WRITE
               ? "write"
               : "read",
           observation->role == GBL_BUS_ROLE_FETCH ? "fetch" : "data",
           bus_access_sequence_name(observation->requested_sequence),
           bus_window_name(observation->window),
           (unsigned long long)observation->cycles,
           (unsigned int)observation->beat_count);
    for (beat = 0u; beat < observation->beat_count; ++beat) {
        printf("%s%llu", beat == 0u ? "" : ",",
               (unsigned long long)observation->beat_cycles[beat]);
    }
    printf("],\"beat_windows\":[");
    for (beat = 0u; beat < observation->beat_count; ++beat) {
        printf("%s\"%s\"", beat == 0u ? "" : ",",
               bus_window_name(observation->beat_windows[beat]));
    }
    printf("],\"effective_sequences\":[");
    for (beat = 0u; beat < observation->beat_count; ++beat) {
        printf("%s\"%s\"", beat == 0u ? "" : ",",
               bus_access_sequence_name(
                   observation->beat_sequences[beat]));
    }
    printf("],\"cost_provenance\":\"%s\",\"cost_model\":\"%s\",\"boundary_provenance\":\"%s\",\"prefetch\":\"unmodeled\",\"hardware_qualification\":\"UNKNOWN\"}\n",
           observation->cost_provenance == GBL_CYCLE_COST_SOURCE_BACKED
               ? "source-backed"
               : "project-selected",
           observation->cost_model == NULL ? "" : observation->cost_model,
           observation->boundary_provenance ==
                   GBL_BUS_BOUNDARY_PROVENANCE_GBATEK_3_06
               ? "community-gbatek-3.06"
               : "none");
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
                        : window == 0u
                              ? UINT64_C(3)
                              : window == 1u ? UINT64_C(5) : UINT64_C(9);
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
    machine->waitcnt = 0u;
    for (window = 0u; window < 3u; ++window) {
        gbl_phase9_bus_observation observation;
        gbl_cycle_cost cost = { 0u, GBL_CYCLE_COST_PROJECT_SELECTED, NULL };
        bool quoted = gbl_bus_quote_observed(
            machine, window_bases[window] + UINT32_C(0x100),
            GBL_BUS_DIRECTION_READ, 2u, GBL_BUS_CLASS_NONSEQUENTIAL,
            GBL_BUS_ROLE_DATA, &cost, &observation);

        CHECK(quoted);
        if (quoted) {
            CHECK(observation.waitcnt == 0u);
            CHECK(observation.address ==
                  window_bases[window] + UINT32_C(0x100));
            CHECK(observation.width == 2u && observation.beat_count == 1u);
            CHECK(observation.window == window);
            CHECK(observation.beat_windows[0] == window);
            CHECK(observation.beat_cycles[0] == cost.cycles);
            CHECK(observation.beat_sequences[0] ==
                  GBL_BUS_CLASS_NONSEQUENTIAL);
            CHECK(observation.cost_provenance ==
                  GBL_CYCLE_COST_SOURCE_BACKED);
            CHECK(observation.boundary_provenance ==
                  GBL_BUS_BOUNDARY_PROVENANCE_NONE);
            CHECK(!observation.prefetch_modeled &&
                  !observation.hardware_qualified);
            emit_phase9_observation(&observation);
        }

        cost.cycles = 0u;
        quoted = gbl_bus_quote_observed(
            machine, window_bases[window] + UINT32_C(0x1FFFE),
            GBL_BUS_DIRECTION_READ, 4u, GBL_BUS_CLASS_SEQUENTIAL,
            GBL_BUS_ROLE_DATA, &cost, &observation);
        CHECK(quoted);
        if (quoted) {
            CHECK(observation.waitcnt == 0u);
            CHECK(observation.width == 4u && observation.beat_count == 2u);
            CHECK(observation.window == window);
            CHECK(observation.beat_windows[0] == window &&
                  observation.beat_windows[1] == window);
            CHECK(observation.beat_sequences[0] ==
                  GBL_BUS_CLASS_SEQUENTIAL);
            CHECK(observation.beat_sequences[1] ==
                  GBL_BUS_CLASS_NONSEQUENTIAL);
            CHECK(observation.boundary_provenance ==
                  GBL_BUS_BOUNDARY_PROVENANCE_GBATEK_3_06);
            CHECK(observation.cost_provenance ==
                  GBL_CYCLE_COST_SOURCE_BACKED);
            CHECK(!observation.prefetch_modeled &&
                  !observation.hardware_qualified);
            CHECK(cost.cycles == observation.beat_cycles[0] +
                                     observation.beat_cycles[1]);
            emit_phase9_observation(&observation);
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
        UINT64_C(5), UINT64_C(5), UINT64_C(5)
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
    gbl_cycle_cost quote_sentinel = {
        UINT64_C(7), GBL_CYCLE_COST_PROJECT_SELECTED, "sentinel"
    };
    gbl_cycle_cost quote_unchanged = quote_sentinel;
    gbl_phase9_bus_observation observation_sentinel;
    gbl_phase9_bus_observation observation_unchanged;
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

    result = gbl_bus_write(machine, UINT32_C(0x04000204), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x08));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->waitcnt == UINT16_C(0x5F08));
    result = gbl_bus_write(machine, UINT32_C(0x04000205), 1u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, UINT32_C(0x40));
    CHECK(result.status == GBL_BUS_STATUS_OK &&
          machine->waitcnt == UINT16_C(0x4008));
    result = gbl_bus_read(machine, UINT32_C(0x04000204), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == 0x08u);
    result = gbl_bus_read(machine, UINT32_C(0x04000205), 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK && result.value == 0x40u);

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

    memset(&observation_sentinel, 0, sizeof(observation_sentinel));
    observation_sentinel.waitcnt = UINT16_C(0x1234);
    observation_sentinel.address = UINT32_C(0x87654321);
    observation_sentinel.cycles = UINT64_C(9);
    observation_sentinel.cost_model = "sentinel";
    observation_sentinel.prefetch_modeled = true;
    memcpy(&observation_unchanged, &observation_sentinel,
           sizeof(observation_sentinel));
    CHECK(!gbl_bus_quote_observed(
        machine, UINT32_MAX - 1u, GBL_BUS_DIRECTION_READ, 4u,
        GBL_BUS_CLASS_NONSEQUENTIAL, GBL_BUS_ROLE_DATA, &quote_sentinel,
        &observation_sentinel));
    CHECK(quote_sentinel.cycles == quote_unchanged.cycles &&
          quote_sentinel.provenance == quote_unchanged.provenance &&
          quote_sentinel.model == quote_unchanged.model);
    CHECK(memcmp(&observation_sentinel, &observation_unchanged,
                 sizeof(observation_sentinel)) == 0);

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
        fprintf(stderr, "usage: bus_test <timing|state|decode|video|ppu.tracer|ppu.bg.tracer|ppu.bg.text|ppu.bg.affine|ppu.bg.bitmap|ppu.composition|ppu.obj.tracer|ppu.obj.regular|ppu.obj.affine|ppu.obj.budget|ppu.obj.boundary|ppu.obj.controls|ppu.refusal|ppu.boundary|ppu.registers|ppu.latch|ppu.sample|ppu.bounds|ppu.frame|ppu.controls|irq.registers|keypad.registers|waitcnt.matrix|waitcnt.boundary|waitcnt.registers>\n");
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
    if (strcmp(argv[1], "ppu.tracer") == 0) {
        return run_ppu_tracer();
    }
    if (strcmp(argv[1], "ppu.bg.tracer") == 0) {
        return run_ppu_bg_tracer();
    }
    if (strcmp(argv[1], "ppu.bg.text") == 0) {
        return run_ppu_bg_text();
    }
    if (strcmp(argv[1], "ppu.bg.affine") == 0) {
        return run_ppu_bg_affine();
    }
    if (strcmp(argv[1], "ppu.bg.bitmap") == 0) {
        return run_ppu_bg_bitmap();
    }
    if (strcmp(argv[1], "ppu.bg.controls") == 0) {
        return run_ppu_bg_controls();
    }
    if (strcmp(argv[1], "ppu.composition") == 0) {
        return run_ppu_composition();
    }
    if (strcmp(argv[1], "ppu.obj.tracer") == 0) {
        return run_ppu_obj_tracer();
    }
    if (strcmp(argv[1], "ppu.obj.regular") == 0) {
        return run_ppu_obj_regular();
    }
    if (strcmp(argv[1], "ppu.obj.affine") == 0) {
        return run_ppu_obj_affine();
    }
    if (strcmp(argv[1], "ppu.obj.budget") == 0) {
        return run_ppu_obj_budget();
    }
    if (strcmp(argv[1], "ppu.obj.boundary") == 0) {
        return run_ppu_obj_boundary();
    }
    if (strcmp(argv[1], "ppu.obj.controls") == 0) {
        return run_ppu_obj_controls();
    }
    if (strcmp(argv[1], "ppu.refusal") == 0) {
        return run_ppu_refusal();
    }
    if (strcmp(argv[1], "ppu.boundary") == 0) {
        return run_ppu_boundary();
    }
    if (strcmp(argv[1], "ppu.registers") == 0) {
        return run_ppu_registers();
    }
    if (strcmp(argv[1], "ppu.latch") == 0) {
        return run_ppu_latch();
    }
    if (strcmp(argv[1], "ppu.sample") == 0) {
        return run_ppu_sample();
    }
    if (strcmp(argv[1], "ppu.bounds") == 0) {
        return run_ppu_bounds();
    }
    if (strcmp(argv[1], "ppu.frame") == 0) {
        return run_ppu_frame();
    }
    if (strcmp(argv[1], "ppu.controls") == 0) {
        return run_ppu_controls();
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
