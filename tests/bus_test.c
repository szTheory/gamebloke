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

/* Compare canonical content rather than structure padding, allocator
 * callbacks, or pointer identity. ROM bytes are part of canonical state. */
static bool canonical_equal(const gbl_machine *left,
                            const gbl_machine *right)
{
    if (left->initialized != right->initialized ||
        left->master_cycles != right->master_cycles ||
        left->open_bus != right->open_bus ||
        left->dispcnt != right->dispcnt ||
        left->rom_length != right->rom_length) {
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

    result = gbl_bus_write(machine, last_ewram, 1u,
                           GBL_BUS_CLASS_SEQUENTIAL, 0x5Au);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.cycles == 1u);
    CHECK(machine->master_cycles == 4u);
    CHECK(machine->open_bus == 0x34563456u);
    result = gbl_bus_read(machine, last_ewram, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(result.value == 0x5Au);
    CHECK(machine->open_bus == 0x5A5A5A5Au);
    CHECK(machine->master_cycles == 5u);
    result = gbl_bus_read(machine, 0x03000000u, 1u,
                          GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == GBL_BUS_OUTCOME_UNMAPPED);
    CHECK(result.value == 0x5Au);
    CHECK(machine->open_bus == 0x5A5A5A5Au);
    CHECK(machine->master_cycles == 6u);

    before_digest = canonical_digest(machine);
    result = gbl_bus_read(machine, base, 3u, GBL_BUS_CLASS_NONSEQUENTIAL);
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
    result = gbl_bus_read(NULL, base, 1u, GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);
    result = gbl_bus_read(uninitialized, base, 1u,
                          GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(result.status == GBL_BUS_STATUS_INVALID_ARGUMENT);
    CHECK(result.value == 0u);
    CHECK(result.cycles == 0u);

    machine->master_cycles = UINT64_MAX - 1u;
    result = gbl_bus_internal(machine, 1u);
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.cycles == 1u);
    CHECK(result.access_class == GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(machine->master_cycles == UINT64_MAX);

    memcpy(snapshot, machine, sizeof(*snapshot));
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
        fprintf(stderr, "usage: bus_test <timing|state>\n");
        return 2;
    }
    if (strcmp(argv[1], "timing") == 0) {
        return run_timing();
    }
    if (strcmp(argv[1], "state") == 0) {
        return run_state();
    }
    fprintf(stderr, "bus_test: unknown selector: %s\n", argv[1]);
    return 2;
}
