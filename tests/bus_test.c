#include "check.h"
#include "gamebloke/gamebloke.h"

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

static bool bytes_equal(const uint8_t *left, const uint8_t *right,
                        size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
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
        { 0x04000000u, GBL_BUS_OUTCOME_IO_UNIMPLEMENTED, "GBA I/O Map" },
        { 0x04000001u, GBL_BUS_OUTCOME_IO_UNIMPLEMENTED, "GBA I/O Map" },
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

/* Compare canonical content rather than structure padding, allocator
 * callbacks, or pointer identity. ROM bytes are part of canonical state. */
static bool canonical_equal(const gbl_machine *left,
                            const gbl_machine *right)
{
    if (left->initialized != right->initialized ||
        left->master_cycles != right->master_cycles ||
        left->open_bus != right->open_bus ||
        left->dispcnt != right->dispcnt ||
        left->rom_length != right->rom_length ||
        ((left->rom == NULL) != (right->rom == NULL))) {
        return false;
    }
    if (!bytes_equal(left->ewram, right->ewram, GBL_EWRAM_SIZE) ||
        !bytes_equal(left->iwram, right->iwram, GBL_IWRAM_SIZE) ||
        !bytes_equal(left->pram, right->pram, GBL_PRAM_SIZE) ||
        !bytes_equal(left->vram, right->vram, GBL_VRAM_SIZE) ||
        !bytes_equal(left->oam, right->oam, GBL_OAM_SIZE)) {
        return false;
    }
    if (left->rom_length != 0u &&
        (left->rom == NULL || right->rom == NULL ||
         !bytes_equal(left->rom, right->rom, left->rom_length))) {
        return false;
    }
    return true;
}

static uint64_t digest_byte(uint64_t digest, uint8_t byte)
{
    return (digest ^ (uint64_t)byte) * UINT64_C(1099511628211);
}

static uint64_t digest_u64(uint64_t digest, uint64_t value)
{
    unsigned int byte_index;

    for (byte_index = 0u; byte_index < 8u; ++byte_index) {
        digest = digest_byte(digest, (uint8_t)(value & UINT64_C(0xFF)));
        value >>= 8u;
    }
    return digest;
}

static uint64_t digest_bytes(uint64_t digest, const uint8_t *bytes,
                             size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        digest = digest_byte(digest, bytes[index]);
    }
    return digest;
}

/* Test-only streaming FNV-1a over canonical fields in a fixed byte order. */
static uint64_t canonical_digest(const gbl_machine *machine)
{
    uint64_t digest = UINT64_C(14695981039346656037);

    digest = digest_byte(digest, machine->initialized ? 1u : 0u);
    digest = digest_u64(digest, machine->master_cycles);
    digest = digest_u64(digest, machine->open_bus);
    digest = digest_u64(digest, machine->dispcnt);
    digest = digest_bytes(digest, machine->ewram, GBL_EWRAM_SIZE);
    digest = digest_bytes(digest, machine->iwram, GBL_IWRAM_SIZE);
    digest = digest_bytes(digest, machine->pram, GBL_PRAM_SIZE);
    digest = digest_bytes(digest, machine->vram, GBL_VRAM_SIZE);
    digest = digest_bytes(digest, machine->oam, GBL_OAM_SIZE);
    digest = digest_byte(digest, machine->rom == NULL ? 0u : 1u);
    digest = digest_u64(digest, (uint64_t)machine->rom_length);
    if (machine->rom_length != 0u && machine->rom != NULL) {
        digest = digest_bytes(digest, machine->rom, machine->rom_length);
    }
    return digest;
}

static int run_timing(void)
{
    const uint32_t base = 0x02000000u;
    const uint32_t last_ewram = 0x02FFFFFFu;
    gbl_machine *machine = new_machine();
    gbl_machine *uninitialized = calloc(1u, sizeof(*uninitialized));
    gbl_machine *snapshot = malloc(sizeof(*snapshot));
    gbl_bus_result result;
    uint64_t before_digest;

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
    CHECK(result.outcome == GBL_BUS_OUTCOME_UNMAPPED);
    CHECK(result.value == 0x3456u);
    CHECK(result.cycles == 1u);
    CHECK(result.access_class == GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(machine->open_bus == 0x34563456u);
    CHECK(machine->master_cycles == 4u);
    result = gbl_bus_read(machine, 0x03000000u, 4u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.value == 0x34563456u);
    CHECK(machine->open_bus == 0x34563456u);
    CHECK(machine->master_cycles == 5u);

    result = gbl_bus_write(machine, last_ewram, 1u,
                           GBL_BUS_CLASS_SEQUENTIAL, 0x5Au);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.cycles == 1u);
    CHECK(machine->master_cycles == 6u);
    CHECK(machine->open_bus == 0x34563456u);
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
    CHECK(result.outcome == GBL_BUS_OUTCOME_UNMAPPED);
    CHECK(result.value == 0x5Au);
    CHECK(machine->open_bus == 0x5A5A5A5Au);
    CHECK(machine->master_cycles == 8u);

    before_digest = canonical_digest(machine);
    result = gbl_bus_read(machine, base, 3u, GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(canonical_digest(machine) == before_digest);
    result = gbl_bus_write(machine, base, 3u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xFFFFFFFFu);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(canonical_digest(machine) == before_digest);
    result = gbl_bus_write(machine, base, 1u,
                           (gbl_bus_access_class)99, 0xFFu);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(canonical_digest(machine) == before_digest);
    result = gbl_bus_read(machine, UINT32_MAX, 2u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(canonical_digest(machine) == before_digest);
    result = gbl_bus_write(machine, UINT32_MAX, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xFFFFu);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(canonical_digest(machine) == before_digest);
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
    CHECK(canonical_equal(uninitialized, snapshot));

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
    CHECK(canonical_equal(machine, snapshot));
    result = gbl_bus_internal(machine, 0u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.cycles == 0u);
    CHECK(result.access_class == GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(canonical_equal(machine, snapshot));
    result = gbl_bus_read(machine, base + 0x100u, 4u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(canonical_equal(machine, snapshot));
    result = gbl_bus_write(machine, base + 0x100u, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0xFFFFFFFFu);
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    CHECK(canonical_equal(machine, snapshot));

    machine->master_cycles = 1u;
    memcpy(snapshot, machine, sizeof(*snapshot));
    result = gbl_bus_internal(machine, UINT64_MAX);
    CHECK(result.status == GBL_BUS_STATUS_TIME_OVERFLOW);
    CHECK(result.cycles == 0u);
    CHECK(canonical_equal(machine, snapshot));

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

static int run_video(void)
{
    gbl_machine *machine = new_machine();
    gbl_bus_result result;

    if (machine == NULL) {
        fprintf(stderr, "bus.video: host allocation failed\n");
        return 1;
    }

    result = gbl_bus_write(machine, 0x05000020u, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0x1234u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    result = gbl_bus_read(machine, 0x05000420u, 2u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.value == 0x1234u);

    result = gbl_bus_write(machine, 0x07000020u, 4u,
                           GBL_BUS_CLASS_NONSEQUENTIAL, 0x89ABCDEFu);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    result = gbl_bus_read(machine, 0x07000420u, 4u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.value == 0x89ABCDEFu);

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
    CHECK(canonical_equal(machine, fresh));
    CHECK(canonical_digest(machine) == canonical_digest(fresh));
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
        disposed_digest = canonical_digest(raw);
        gbl_machine_dispose(raw);
        CHECK(canonical_digest(raw) == disposed_digest);
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

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: bus_test <timing|state|decode|video>\n");
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
    fprintf(stderr, "bus_test: unknown selector: %s\n", argv[1]);
    return 2;
}
