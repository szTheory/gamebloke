#include "check.h"
#include "gamebloke/gamebloke.h"
#include "state_check.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GBL_TEST_MAX_ROM_SIZE 0x02000000u
#define GBL_TEST_MIN_ROM_SIZE 0xC0u
#define GBL_TEST_IMAGE_CAPACITY ((size_t)GBL_TEST_MAX_ROM_SIZE + 1u)
#define GBL_TEST_ALLOCATIONS 4u

typedef struct host_block {
    uint8_t *raw;
    uint8_t *image;
    size_t length;
    bool active;
} host_block;

typedef struct host_storage {
    size_t allocate_calls;
    size_t release_calls;
    size_t last_allocate_size;
    size_t last_release_size;
    uintptr_t last_released_address;
    uint8_t *last_allocated_image;
    gbl_machine *observe_machine;
    bool inspect_next_release;
    bool saw_committed_replacement;
    bool fail_next;
    host_block blocks[GBL_TEST_ALLOCATIONS];
} host_storage;

typedef struct expected_load {
    size_t length;
    gbl_load_status status;
    gbl_load_reason reason;
} expected_load;

static uint8_t synthetic_byte(size_t index)
{
    uint32_t mixed = (uint32_t)index * 29u ^ (uint32_t)(index >> 8u) ^
                     UINT32_C(0xA7);

    return (uint8_t)mixed;
}

static void fill_synthetic(uint8_t *image, size_t length, uint8_t salt)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        image[index] = (uint8_t)(synthetic_byte(index) ^ salt);
    }
}

static void *host_allocate(void *context, size_t size)
{
    host_storage *storage = context;
    host_block *block = NULL;
    uint8_t *raw;
    size_t index;

    ++storage->allocate_calls;
    storage->last_allocate_size = size;
    if (storage->fail_next) {
        storage->fail_next = false;
        return NULL;
    }
    for (index = 0u; index < GBL_TEST_ALLOCATIONS; ++index) {
        if (!storage->blocks[index].active) {
            block = &storage->blocks[index];
            break;
        }
    }
    if (block == NULL || size > SIZE_MAX - 2u) {
        return NULL;
    }
    raw = malloc(size + 2u);
    if (raw == NULL) {
        return NULL;
    }

    raw[0] = 0xD3u;
    raw[size + 1u] = 0x7Cu;
    block->raw = raw;
    block->image = raw + 1u;
    block->length = size;
    block->active = true;
    storage->last_allocated_image = block->image;
    return block->image;
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

static bool machine_is_cold_with_candidate(const host_storage *storage)
{
    const gbl_machine *machine = storage->observe_machine;

    return machine != NULL && machine->initialized &&
           machine->rom == storage->last_allocated_image &&
           machine->rom_length == storage->last_allocate_size &&
           machine->master_cycles == 0u && machine->open_bus == 0u &&
           machine->dispcnt == 0u &&
           bytes_are_zero(machine->ewram, GBL_EWRAM_SIZE) &&
           bytes_are_zero(machine->iwram, GBL_IWRAM_SIZE) &&
           bytes_are_zero(machine->pram, GBL_PRAM_SIZE) &&
           bytes_are_zero(machine->vram, GBL_VRAM_SIZE) &&
           bytes_are_zero(machine->oam, GBL_OAM_SIZE);
}

static void host_release(void *context, void *pointer, size_t size)
{
    host_storage *storage = context;
    size_t index;

    ++storage->release_calls;
    storage->last_release_size = size;
    storage->last_released_address = (uintptr_t)pointer;
    if (storage->inspect_next_release) {
        storage->saw_committed_replacement =
            machine_is_cold_with_candidate(storage);
        CHECK(storage->saw_committed_replacement);
        CHECK(storage->last_allocated_image != pointer);
        storage->inspect_next_release = false;
    }
    for (index = 0u; index < GBL_TEST_ALLOCATIONS; ++index) {
        host_block *block = &storage->blocks[index];

        if (block->active && block->image == pointer) {
            CHECK(block->length == size);
            CHECK(block->raw[0] == 0xD3u);
            CHECK(block->image[block->length] == 0x7Cu);
            free(block->raw);
            block->raw = NULL;
            block->image = NULL;
            block->length = 0u;
            block->active = false;
            return;
        }
    }
    CHECK(false);
}

static gbl_machine *new_machine(host_storage *storage)
{
    gbl_allocator allocator = { storage, host_allocate, host_release };
    gbl_machine *machine = calloc(1u, sizeof(*machine));

    if (machine == NULL) {
        return NULL;
    }
    if (gbl_machine_init(machine, &allocator) != GBL_BUS_STATUS_OK) {
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

static void set_nondefault_state(gbl_machine *machine)
{
    machine->master_cycles = UINT64_C(0x123456789);
    machine->open_bus = UINT32_C(0xA1B2C3D4);
    machine->dispcnt = UINT16_C(0x0123);
    machine->ewram[0u] = 0x12u;
    machine->ewram[GBL_EWRAM_SIZE - 1u] = 0x34u;
    machine->iwram[0u] = 0x56u;
    machine->iwram[GBL_IWRAM_SIZE - 1u] = 0x78u;
    machine->pram[0u] = 0x9Au;
    machine->pram[GBL_PRAM_SIZE - 1u] = 0xBCu;
    machine->vram[0u] = 0xDEu;
    machine->vram[GBL_VRAM_SIZE - 1u] = 0xF0u;
    machine->oam[0u] = 0x11u;
    machine->oam[GBL_OAM_SIZE - 1u] = 0x22u;
}

static void check_identity_comparison(host_storage *storage)
{
    uint8_t identical_roms[2][4] = {
        { 0x10u, 0x20u, 0x30u, 0x40u },
        { 0x10u, 0x20u, 0x30u, 0x40u }
    };
    gbl_machine before = { 0 };
    gbl_machine after;

    before.rom = identical_roms[0];
    before.rom_length = sizeof(identical_roms[0]);
    before.allocator.context = storage;
    before.allocator.allocate = host_allocate;
    before.allocator.release = host_release;
    after = before;
    CHECK(gbl_test_canonical_equal(&before, &after));
    CHECK(gbl_test_same_instance_identity_equal(&before, &after));

    after.rom = identical_roms[1];
    CHECK(gbl_test_canonical_equal(&before, &after));
    CHECK(!gbl_test_same_instance_identity_equal(&before, &after));

    after = before;
    --after.rom_length;
    CHECK(!gbl_test_same_instance_identity_equal(&before, &after));

    after = before;
    after.allocator.context = &after;
    CHECK(gbl_test_canonical_equal(&before, &after));
    CHECK(!gbl_test_same_instance_identity_equal(&before, &after));

    after = before;
    after.allocator.allocate = NULL;
    CHECK(gbl_test_canonical_equal(&before, &after));
    CHECK(!gbl_test_same_instance_identity_equal(&before, &after));

    after = before;
    after.allocator.release = NULL;
    CHECK(gbl_test_canonical_equal(&before, &after));
    CHECK(!gbl_test_same_instance_identity_equal(&before, &after));
}

static void expect_failure_unchanged(
    gbl_machine *machine, host_storage *storage, const uint8_t *image,
    const uint8_t *original, size_t caller_capacity, size_t length,
    gbl_load_status expected_status, gbl_load_reason expected_reason,
    size_t expected_allocate_delta)
{
    gbl_machine *before = malloc(sizeof(*before));
    gbl_load_result result;
    uint64_t digest_before;
    size_t allocate_before;
    size_t release_before;

    CHECK(before != NULL);
    if (before == NULL) {
        return;
    }
    *before = *machine;
    digest_before = gbl_test_canonical_digest(machine);
    allocate_before = storage->allocate_calls;
    release_before = storage->release_calls;

    result = gbl_load_rom(machine, image, length);
    CHECK(result.status == expected_status);
    CHECK(result.reason == expected_reason);
    CHECK(gbl_test_same_instance_identity_equal(before, machine));
    CHECK(storage->allocate_calls == allocate_before + expected_allocate_delta);
    CHECK(storage->release_calls == release_before);
    if (storage->release_calls == release_before) {
        CHECK(gbl_test_canonical_equal(before, machine));
        CHECK(gbl_test_canonical_digest(machine) == digest_before);
    }
    if (image != NULL && original != NULL && caller_capacity != 0u) {
        CHECK(gbl_test_bytes_equal(image, original, caller_capacity));
    }
    free(before);
}

static bool host_has_no_live_allocations(const host_storage *storage)
{
    size_t index;

    for (index = 0u; index < GBL_TEST_ALLOCATIONS; ++index) {
        if (storage->blocks[index].active) {
            return false;
        }
    }
    return true;
}

static int run_lengths(void)
{
    const expected_load invalid_lengths[] = {
        { 0u, GBL_LOAD_STATUS_REFUSED, GBL_LOAD_REASON_ROM_ZERO },
        { 1u, GBL_LOAD_STATUS_REFUSED, GBL_LOAD_REASON_ROM_SHORT },
        { 0xBFu, GBL_LOAD_STATUS_REFUSED, GBL_LOAD_REASON_ROM_SHORT },
        { 0xC1u, GBL_LOAD_STATUS_REFUSED, GBL_LOAD_REASON_ROM_ODD },
        { GBL_TEST_MAX_ROM_SIZE + 1u, GBL_LOAD_STATUS_REFUSED,
          GBL_LOAD_REASON_ROM_TOO_LARGE }
    };
    host_storage storage = { 0 };
    gbl_machine *machine = new_machine(&storage);
    uint8_t *image = malloc(GBL_TEST_IMAGE_CAPACITY);
    uint8_t *original = malloc(GBL_TEST_IMAGE_CAPACITY);
    size_t allocation_before;
    size_t release_before;
    size_t index;
    gbl_load_result result;

    if (machine == NULL || image == NULL || original == NULL) {
        fprintf(stderr, "loader.lengths: host setup allocation failed\n");
        free_machine(machine);
        free(image);
        free(original);
        return 1;
    }
    check_identity_comparison(&storage);
    fill_synthetic(image, GBL_TEST_IMAGE_CAPACITY, 0x31u);
    for (index = 0u; index < GBL_TEST_IMAGE_CAPACITY; ++index) {
        original[index] = image[index];
    }
    set_nondefault_state(machine);

    for (index = 0u; index < sizeof(invalid_lengths) / sizeof(invalid_lengths[0]);
         ++index) {
        const expected_load *expected = &invalid_lengths[index];

        expect_failure_unchanged(machine, &storage, image, original,
                                 GBL_TEST_IMAGE_CAPACITY, expected->length,
                                 expected->status, expected->reason, 0u);
        expect_failure_unchanged(machine, &storage, NULL, NULL, 0u,
                                 expected->length, expected->status,
                                 expected->reason, 0u);
    }

    expect_failure_unchanged(machine, &storage, NULL, NULL, 0u, 0xC0u,
                             GBL_LOAD_STATUS_INVALID_ARGUMENT,
                             GBL_LOAD_REASON_ARGUMENT, 0u);
    allocation_before = storage.allocate_calls;
    machine->allocator.allocate = NULL;
    expect_failure_unchanged(machine, &storage, image, original,
                             GBL_TEST_IMAGE_CAPACITY, 0xC0u,
                             GBL_LOAD_STATUS_INVALID_ARGUMENT,
                             GBL_LOAD_REASON_ARGUMENT, 0u);
    machine->allocator.allocate = host_allocate;
    CHECK(storage.allocate_calls == allocation_before);

    result = gbl_load_rom(machine, image, 0xC0u);
    CHECK(result.status == GBL_LOAD_STATUS_OK);
    CHECK(result.reason == GBL_LOAD_REASON_NONE);
    CHECK(storage.last_allocate_size == 0xC0u);
    CHECK(machine->rom != image);
    CHECK(gbl_rom_size(machine) == 0xC0u);
    if (gbl_rom_data(machine) != NULL) {
        CHECK(gbl_test_bytes_equal(gbl_rom_data(machine), image, 0xC0u));
    }

    release_before = storage.release_calls;
    result = gbl_load_rom(machine, image, GBL_TEST_MAX_ROM_SIZE);
    CHECK(result.status == GBL_LOAD_STATUS_OK);
    CHECK(result.reason == GBL_LOAD_REASON_NONE);
    CHECK(storage.last_allocate_size == GBL_TEST_MAX_ROM_SIZE);
    CHECK(storage.release_calls == release_before + 1u);
    CHECK(gbl_rom_data(machine) != image);
    CHECK(gbl_rom_size(machine) == GBL_TEST_MAX_ROM_SIZE);
    if (gbl_rom_data(machine) != NULL) {
        CHECK(gbl_test_bytes_equal(gbl_rom_data(machine), image,
                                   GBL_TEST_MAX_ROM_SIZE));
    }
    CHECK(gbl_test_bytes_equal(image, original, GBL_TEST_IMAGE_CAPACITY));
    gbl_machine_dispose(machine);
    release_before = storage.release_calls;
    gbl_machine_dispose(machine);
    CHECK(storage.release_calls == release_before);
    CHECK(host_has_no_live_allocations(&storage));
    free(machine);
    free(image);
    free(original);

    if (CHECK_DONE() == 0) {
        puts("loader: lengths ok");
    }
    return CHECK_DONE();
}

static void exercise_refusals(gbl_machine *machine, host_storage *storage,
                              const uint8_t *image, const uint8_t *original)
{
    const expected_load invalid_lengths[] = {
        { 0u, GBL_LOAD_STATUS_REFUSED, GBL_LOAD_REASON_ROM_ZERO },
        { 1u, GBL_LOAD_STATUS_REFUSED, GBL_LOAD_REASON_ROM_SHORT },
        { 0xBFu, GBL_LOAD_STATUS_REFUSED, GBL_LOAD_REASON_ROM_SHORT },
        { 0xC1u, GBL_LOAD_STATUS_REFUSED, GBL_LOAD_REASON_ROM_ODD },
        { GBL_TEST_MAX_ROM_SIZE + 1u, GBL_LOAD_STATUS_REFUSED,
          GBL_LOAD_REASON_ROM_TOO_LARGE }
    };
    size_t index;

    for (index = 0u; index < sizeof(invalid_lengths) / sizeof(invalid_lengths[0]);
         ++index) {
        const expected_load *expected = &invalid_lengths[index];

        expect_failure_unchanged(machine, storage, image, original,
                                 GBL_TEST_IMAGE_CAPACITY, expected->length,
                                 expected->status, expected->reason, 0u);
        expect_failure_unchanged(machine, storage, NULL, NULL, 0u,
                                 expected->length, expected->status,
                                 expected->reason, 0u);
    }
    expect_failure_unchanged(machine, storage, NULL, NULL, 0u, 0xC0u,
                             GBL_LOAD_STATUS_INVALID_ARGUMENT,
                             GBL_LOAD_REASON_ARGUMENT, 0u);

    {
        gbl_allocator original_allocator = machine->allocator;

        machine->allocator.allocate = NULL;
        expect_failure_unchanged(machine, storage, image, original,
                                 GBL_TEST_IMAGE_CAPACITY, 0xC0u,
                                 GBL_LOAD_STATUS_INVALID_ARGUMENT,
                                 GBL_LOAD_REASON_ARGUMENT, 0u);
        machine->allocator = original_allocator;
        machine->allocator.release = NULL;
        expect_failure_unchanged(machine, storage, image, original,
                                 GBL_TEST_IMAGE_CAPACITY, 0xC0u,
                                 GBL_LOAD_STATUS_INVALID_ARGUMENT,
                                 GBL_LOAD_REASON_ARGUMENT, 0u);
        machine->allocator = original_allocator;
    }

    storage->fail_next = true;
    expect_failure_unchanged(machine, storage, image, original,
                             GBL_TEST_IMAGE_CAPACITY, 0xC0u,
                             GBL_LOAD_STATUS_RESOURCE_FAILURE,
                             GBL_LOAD_REASON_ALLOCATION, 1u);
}

static void set_synthetic_game_code(uint8_t *image, const char code[4])
{
    size_t index;

    for (index = 0u; index < 4u; ++index) {
        image[0xACu + index] = (uint8_t)code[index];
    }
}

static int run_diagnostics(void)
{
    host_storage storage = { 0 };
    gbl_machine *machine = new_machine(&storage);
    uint8_t image[GBL_TEST_MIN_ROM_SIZE];
    uint8_t original[GBL_TEST_MIN_ROM_SIZE];
    uint8_t checksum_probe[GBL_TEST_MIN_ROM_SIZE] = { 0 };
    uint8_t synthetic_actual[4] = { 0x13u, 0x37u, 0xA5u, 0x5Au };
    uint8_t synthetic_expected[4] = { 0x13u, 0x37u, 0xA5u, 0x5Au };
    gbl_header_diagnostics diagnostics;
    gbl_load_result load;
    const uint8_t *owned_rom;
    size_t index;

    if (machine == NULL) {
        fprintf(stderr, "loader.diagnostics: host setup allocation failed\n");
        return 1;
    }

    diagnostics = gbl_rom_diagnostics(machine);
    CHECK(!diagnostics.loaded);
    CHECK(diagnostics.canonical_logo == GBL_HEADER_LOGO_UNCHECKED);
    CHECK(diagnostics.logo_reason == GBL_HEADER_LOGO_REASON_OWNER_PENDING);
    CHECK(diagnostics.checksum == GBL_HEADER_CHECKSUM_NOT_LOADED);
    CHECK(diagnostics.game_code == GBL_HEADER_GAME_CODE_NOT_LOADED);
    CHECK(gbl_header_compare_bytes(synthetic_actual, synthetic_expected,
                                   sizeof(synthetic_actual)) ==
          GBL_HEADER_COMPARE_EQUAL);
    synthetic_expected[2] ^= 1u;
    CHECK(gbl_header_compare_bytes(synthetic_actual, synthetic_expected,
                                   sizeof(synthetic_actual)) ==
          GBL_HEADER_COMPARE_DIFFERENT);
    CHECK(gbl_header_compare_bytes(synthetic_actual, synthetic_expected, 0u) ==
          GBL_HEADER_COMPARE_INVALID_ARGUMENT);
    CHECK(gbl_header_compare_bytes(NULL, synthetic_expected,
                                   sizeof(synthetic_actual)) ==
          GBL_HEADER_COMPARE_INVALID_ARGUMENT);
    CHECK(gbl_header_compare_bytes(synthetic_actual, NULL,
                                   sizeof(synthetic_actual)) ==
          GBL_HEADER_COMPARE_INVALID_ARGUMENT);

    /* Zero checksum bytes produce 0xE7; byte A0 = 1 decrements it to 0xE6. */
    checksum_probe[0xBDu] = 0xE7u;
    load = gbl_load_rom(machine, checksum_probe, sizeof(checksum_probe));
    CHECK(load.status == GBL_LOAD_STATUS_OK);
    diagnostics = gbl_rom_diagnostics(machine);
    CHECK(diagnostics.loaded);
    CHECK(diagnostics.canonical_logo == GBL_HEADER_LOGO_UNCHECKED);
    CHECK(diagnostics.logo_reason == GBL_HEADER_LOGO_REASON_OWNER_PENDING);
    CHECK(diagnostics.logo_window == GBL_HEADER_LOGO_WINDOW_ZERO);
    CHECK(diagnostics.checksum == GBL_HEADER_CHECKSUM_OK);
    CHECK(diagnostics.game_code == GBL_HEADER_GAME_CODE_UNUSUAL);
    checksum_probe[0xA0u] = 1u;
    checksum_probe[0xBDu] = 0xE6u;
    load = gbl_load_rom(machine, checksum_probe, sizeof(checksum_probe));
    CHECK(load.status == GBL_LOAD_STATUS_OK);
    diagnostics = gbl_rom_diagnostics(machine);
    CHECK(diagnostics.checksum == GBL_HEADER_CHECKSUM_OK);

    memset(image, 0, sizeof(image));
    image[0x04u] = 0x72u;
    set_synthetic_game_code(image, "AB19");
    /* AB19 plus one byte at A0 has the fixed checksum 0xF9. */
    image[0xA0u] = 1u;
    image[0xBDu] = 0xF9u;
    for (index = 0u; index < sizeof(image); ++index) {
        original[index] = image[index];
    }
    load = gbl_load_rom(machine, image, sizeof(image));
    CHECK(load.status == GBL_LOAD_STATUS_OK);
    CHECK(load.reason == GBL_LOAD_REASON_NONE);
    diagnostics = gbl_rom_diagnostics(machine);
    CHECK(diagnostics.loaded);
    CHECK(diagnostics.canonical_logo == GBL_HEADER_LOGO_UNCHECKED);
    CHECK(diagnostics.logo_reason == GBL_HEADER_LOGO_REASON_OWNER_PENDING);
    CHECK(diagnostics.logo_window == GBL_HEADER_LOGO_WINDOW_NONZERO);
    CHECK(diagnostics.checksum == GBL_HEADER_CHECKSUM_OK);
    CHECK(diagnostics.game_code == GBL_HEADER_GAME_CODE_ORDINARY);

    set_nondefault_state(machine);
    storage.fail_next = true;
    expect_failure_unchanged(machine, &storage, image, original, sizeof(image),
                             sizeof(image), GBL_LOAD_STATUS_RESOURCE_FAILURE,
                             GBL_LOAD_REASON_ALLOCATION, 1u);
    expect_failure_unchanged(machine, &storage, image, original, sizeof(image),
                             0xBFu, GBL_LOAD_STATUS_REFUSED,
                             GBL_LOAD_REASON_ROM_SHORT, 0u);
    gbl_machine_reset(machine);
    diagnostics = gbl_rom_diagnostics(machine);
    CHECK(diagnostics.loaded);
    CHECK(diagnostics.canonical_logo == GBL_HEADER_LOGO_UNCHECKED);
    CHECK(diagnostics.logo_reason == GBL_HEADER_LOGO_REASON_OWNER_PENDING);
    CHECK(diagnostics.logo_window == GBL_HEADER_LOGO_WINDOW_NONZERO);
    CHECK(diagnostics.checksum == GBL_HEADER_CHECKSUM_OK);
    CHECK(diagnostics.game_code == GBL_HEADER_GAME_CODE_ORDINARY);

    memset(image, 0, sizeof(image));
    set_synthetic_game_code(image, "ab?1");
    /* ab?1 plus byte A0 = 1 expects 0xB3; 0xB4 is one off. */
    image[0xA0u] = 1u;
    image[0xBDu] = 0xB4u;
    for (index = 0u; index < sizeof(image); ++index) {
        original[index] = image[index];
    }
    load = gbl_load_rom(machine, image, sizeof(image));
    CHECK(load.status == GBL_LOAD_STATUS_OK);
    CHECK(load.reason == GBL_LOAD_REASON_NONE);
    diagnostics = gbl_rom_diagnostics(machine);
    CHECK(diagnostics.loaded);
    CHECK(diagnostics.canonical_logo == GBL_HEADER_LOGO_UNCHECKED);
    CHECK(diagnostics.logo_reason == GBL_HEADER_LOGO_REASON_OWNER_PENDING);
    CHECK(diagnostics.logo_window == GBL_HEADER_LOGO_WINDOW_ZERO);
    CHECK(diagnostics.checksum == GBL_HEADER_CHECKSUM_MISMATCH);
    CHECK(diagnostics.game_code == GBL_HEADER_GAME_CODE_UNUSUAL);
    owned_rom = gbl_rom_data(machine);
    CHECK(owned_rom != NULL);
    if (owned_rom != NULL) {
        CHECK(gbl_test_bytes_equal(owned_rom, original, sizeof(image)));
    }

    gbl_machine_dispose(machine);
    CHECK(!machine->header_diagnostics.loaded);
    CHECK(machine->header_diagnostics.canonical_logo ==
          GBL_HEADER_LOGO_UNCHECKED);
    CHECK(machine->header_diagnostics.logo_reason ==
          GBL_HEADER_LOGO_REASON_OWNER_PENDING);
    CHECK(!gbl_rom_diagnostics(machine).loaded);
    CHECK(host_has_no_live_allocations(&storage));
    free(machine);

    if (CHECK_DONE() == 0) {
        puts("loader: diagnostics ok");
    }
    return CHECK_DONE();
}

static int run_logo_digest(void)
{
    host_storage storage = { 0 };
    gbl_machine *machine = new_machine(&storage);
    uint8_t image[GBL_TEST_MIN_ROM_SIZE] = { 0 };
    size_t index;
    gbl_load_result load;
    gbl_header_diagnostics diagnostics;

    if (machine == NULL) {
        fprintf(stderr, "loader.logo-digest: host setup allocation failed\n");
        return 1;
    }
    for (index = 0u; index < 0x9Cu; ++index) {
        image[0x04u + index] = (uint8_t)(index + 1u);
    }

    load = gbl_load_rom(machine, image, sizeof(image));
    CHECK(load.status == GBL_LOAD_STATUS_OK);
    CHECK(load.reason == GBL_LOAD_REASON_NONE);
    diagnostics = gbl_rom_diagnostics(machine);
    CHECK(diagnostics.loaded);
    CHECK(diagnostics.canonical_logo == GBL_HEADER_LOGO_CHECKED_MATCH);

    gbl_machine_dispose(machine);
    CHECK(host_has_no_live_allocations(&storage));
    free(machine);

    if (CHECK_DONE() == 0) {
        puts("loader: logo-digest ok");
    }
    return CHECK_DONE();
}

static int run_transaction(void)
{
    host_storage fresh_storage = { 0 };
    host_storage loaded_storage = { 0 };
    host_storage reference_storage = { 0 };
    gbl_machine *fresh = new_machine(&fresh_storage);
    gbl_machine *loaded = new_machine(&loaded_storage);
    gbl_machine *reference = NULL;
    uint8_t *image = malloc(GBL_TEST_IMAGE_CAPACITY);
    uint8_t *original = malloc(GBL_TEST_IMAGE_CAPACITY);
    uint8_t *replacement = malloc(GBL_TEST_MIN_ROM_SIZE);
    uint8_t replacement_original[GBL_TEST_MIN_ROM_SIZE];
    uintptr_t previous_rom_address;
    size_t releases_before;
    size_t index;
    int diagnostics_result;
    gbl_load_result result;

    if (fresh == NULL || loaded == NULL || image == NULL || original == NULL ||
        replacement == NULL) {
        fprintf(stderr, "loader.transaction: host setup allocation failed\n");
        free_machine(fresh);
        free_machine(loaded);
        free(image);
        free(original);
        free(replacement);
        return 1;
    }
    fill_synthetic(image, GBL_TEST_IMAGE_CAPACITY, 0x53u);
    for (index = 0u; index < GBL_TEST_IMAGE_CAPACITY; ++index) {
        original[index] = image[index];
    }
    fill_synthetic(replacement, GBL_TEST_MIN_ROM_SIZE, 0xA6u);
    for (index = 0u; index < GBL_TEST_MIN_ROM_SIZE; ++index) {
        replacement_original[index] = replacement[index];
    }

    result = gbl_load_rom(loaded, image, GBL_TEST_MIN_ROM_SIZE);
    CHECK(result.status == GBL_LOAD_STATUS_OK);
    CHECK(result.reason == GBL_LOAD_REASON_NONE);
    set_nondefault_state(fresh);
    set_nondefault_state(loaded);

    exercise_refusals(fresh, &fresh_storage, image, original);
    exercise_refusals(loaded, &loaded_storage, image, original);

    previous_rom_address = (uintptr_t)loaded->rom;
    releases_before = loaded_storage.release_calls;
    loaded_storage.observe_machine = loaded;
    loaded_storage.inspect_next_release = true;
    reference = new_machine(&reference_storage);
    if (reference == NULL) {
        fprintf(stderr, "loader.transaction: reference machine allocation failed\n");
        free_machine(fresh);
        free_machine(loaded);
        free(image);
        free(original);
        return 1;
    }
    result = gbl_load_rom(reference, replacement, GBL_TEST_MIN_ROM_SIZE);
    CHECK(result.status == GBL_LOAD_STATUS_OK);
    result = gbl_load_rom(loaded, replacement, GBL_TEST_MIN_ROM_SIZE);
    CHECK(result.status == GBL_LOAD_STATUS_OK);
    CHECK(loaded_storage.saw_committed_replacement);
    CHECK(loaded_storage.release_calls == releases_before + 1u);
    CHECK(loaded_storage.last_released_address == previous_rom_address);
    CHECK(loaded->rom != replacement);
    CHECK(gbl_rom_size(loaded) == GBL_TEST_MIN_ROM_SIZE);
    if (gbl_rom_data(loaded) != NULL) {
        CHECK(gbl_test_bytes_equal(gbl_rom_data(loaded), replacement_original,
                                   sizeof(replacement_original)));
    }

    replacement[5u] ^= 0x80u;
    if (gbl_rom_data(loaded) != NULL) {
        CHECK(gbl_test_bytes_equal(gbl_rom_data(loaded), replacement_original,
                                   sizeof(replacement_original)));
    }
    free(replacement);
    replacement = NULL;
    if (gbl_rom_data(loaded) != NULL) {
        CHECK(gbl_test_bytes_equal(gbl_rom_data(loaded), replacement_original,
                                   sizeof(replacement_original)));
    }
    gbl_machine_reset(loaded);
    gbl_machine_reset(loaded);
    CHECK(gbl_rom_size(loaded) == sizeof(replacement_original));
    CHECK(gbl_test_canonical_equal(loaded, reference));
    CHECK(gbl_test_canonical_digest(loaded) ==
          gbl_test_canonical_digest(reference));

    gbl_machine_dispose(loaded);
    releases_before = loaded_storage.release_calls;
    gbl_machine_dispose(loaded);
    CHECK(loaded_storage.release_calls == releases_before);
    free(loaded);
    free_machine(fresh);
    free_machine(reference);
    CHECK(loaded_storage.release_calls == 2u);
    CHECK(reference_storage.release_calls == 1u);
    CHECK(fresh_storage.release_calls == 0u);
    CHECK(host_has_no_live_allocations(&loaded_storage));
    CHECK(host_has_no_live_allocations(&fresh_storage));
    CHECK(host_has_no_live_allocations(&reference_storage));
    free(image);
    free(original);
    free(replacement);

    diagnostics_result = run_diagnostics();
    if (diagnostics_result != 0) {
        return diagnostics_result;
    }

    if (CHECK_DONE() == 0) {
        puts("loader: transaction ok");
    }
    return CHECK_DONE();
}

static uint32_t integration_rom_value(const uint8_t *image, size_t offset,
                                      uint8_t width)
{
    uint32_t value = image[offset];

    if (width > 1u) {
        value |= (uint32_t)image[offset + 1u] << 8u;
    }
    if (width > 2u) {
        value |= (uint32_t)image[offset + 2u] << 16u;
        value |= (uint32_t)image[offset + 3u] << 24u;
    }
    return value;
}

static void check_integration_bus_result(const gbl_machine *machine,
                                        gbl_bus_result result,
                                        gbl_bus_outcome outcome,
                                        uint32_t value, uint64_t cycles,
                                        gbl_bus_access_class access_class,
                                        uint64_t previous_cycles)
{
    CHECK(result.status == GBL_BUS_STATUS_OK);
    CHECK(result.outcome == outcome);
    CHECK(result.value == value);
    CHECK(result.cycles == cycles);
    CHECK(result.access_class == access_class);
    CHECK(machine->master_cycles == previous_cycles + cycles);
}

static void run_integration_bus_step(gbl_machine *machine,
                                     const uint8_t *image, unsigned int step)
{
    static const uint32_t rom_bases[] = {
        0x08000000u, 0x0A000000u, 0x0C000000u
    };
    const uint32_t vram_address = 0x06012000u;
    uint64_t previous_cycles = machine->master_cycles;
    gbl_bus_result result;

    switch (step) {
    case 0u:
        result = gbl_bus_read(machine, rom_bases[0] + 0x20u, 4u,
                              GBL_BUS_CLASS_NONSEQUENTIAL);
        check_integration_bus_result(
            machine, result, GBL_BUS_OUTCOME_ROM,
            integration_rom_value(image, 0x20u, 4u), 1u,
            GBL_BUS_CLASS_NONSEQUENTIAL, previous_cycles);
        break;
    case 1u:
        result = gbl_bus_read(machine, rom_bases[1] + 0x20u, 2u,
                              GBL_BUS_CLASS_SEQUENTIAL);
        check_integration_bus_result(
            machine, result, GBL_BUS_OUTCOME_ROM,
            integration_rom_value(image, 0x20u, 2u), 1u,
            GBL_BUS_CLASS_SEQUENTIAL, previous_cycles);
        break;
    case 2u:
        result = gbl_bus_read(machine, rom_bases[2] + 0x20u, 1u,
                              GBL_BUS_CLASS_NONSEQUENTIAL);
        check_integration_bus_result(
            machine, result, GBL_BUS_OUTCOME_ROM,
            integration_rom_value(image, 0x20u, 1u), 1u,
            GBL_BUS_CLASS_NONSEQUENTIAL, previous_cycles);
        break;
    case 3u:
        result = gbl_bus_write(machine, 0x02000100u, 4u,
                               GBL_BUS_CLASS_SEQUENTIAL, 0x89ABCDEFu);
        check_integration_bus_result(machine, result, GBL_BUS_OUTCOME_RAM,
                                     0u, 1u, GBL_BUS_CLASS_SEQUENTIAL,
                                     previous_cycles);
        break;
    case 4u:
        result = gbl_bus_read(machine, 0x02400100u, 4u,
                              GBL_BUS_CLASS_NONSEQUENTIAL);
        check_integration_bus_result(machine, result, GBL_BUS_OUTCOME_RAM,
                                     0x89ABCDEFu, 1u,
                                     GBL_BUS_CLASS_NONSEQUENTIAL,
                                     previous_cycles);
        break;
    case 5u:
        result = gbl_bus_write(machine, 0x03000220u, 4u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, 0x76543210u);
        check_integration_bus_result(machine, result, GBL_BUS_OUTCOME_RAM,
                                     0u, 1u, GBL_BUS_CLASS_NONSEQUENTIAL,
                                     previous_cycles);
        break;
    case 6u:
        result = gbl_bus_read(machine, 0x03008220u, 4u,
                              GBL_BUS_CLASS_SEQUENTIAL);
        check_integration_bus_result(machine, result, GBL_BUS_OUTCOME_RAM,
                                     0x76543210u, 1u,
                                     GBL_BUS_CLASS_SEQUENTIAL,
                                     previous_cycles);
        break;
    case 7u:
        result = gbl_bus_write(machine, 0x04000000u, 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, 3u);
        check_integration_bus_result(
            machine, result, GBL_BUS_OUTCOME_IO_MODE_SUBSET, 0u, 1u,
            GBL_BUS_CLASS_NONSEQUENTIAL, previous_cycles);
        break;
    case 8u:
        result = gbl_bus_read(machine, 0x04000000u, 2u,
                              GBL_BUS_CLASS_SEQUENTIAL);
        check_integration_bus_result(
            machine, result, GBL_BUS_OUTCOME_IO_MODE_SUBSET, 3u, 1u,
            GBL_BUS_CLASS_SEQUENTIAL, previous_cycles);
        break;
    case 9u:
        result = gbl_bus_write(machine, vram_address, 1u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, 0x5Au);
        check_integration_bus_result(machine, result, GBL_BUS_OUTCOME_RAM,
                                     0u, 1u, GBL_BUS_CLASS_NONSEQUENTIAL,
                                     previous_cycles);
        break;
    case 10u:
        result = gbl_bus_read(machine, vram_address, 2u,
                              GBL_BUS_CLASS_SEQUENTIAL);
        check_integration_bus_result(machine, result, GBL_BUS_OUTCOME_RAM,
                                     0x5A5Au, 1u,
                                     GBL_BUS_CLASS_SEQUENTIAL,
                                     previous_cycles);
        break;
    case 11u:
        result = gbl_bus_write(machine, 0x04000000u, 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL, 0u);
        check_integration_bus_result(
            machine, result, GBL_BUS_OUTCOME_IO_MODE_SUBSET, 0u, 1u,
            GBL_BUS_CLASS_NONSEQUENTIAL, previous_cycles);
        break;
    case 12u:
        result = gbl_bus_write(machine, vram_address, 1u,
                               GBL_BUS_CLASS_SEQUENTIAL, 0xC3u);
        check_integration_bus_result(machine, result, GBL_BUS_OUTCOME_RAM,
                                     0u, 1u, GBL_BUS_CLASS_SEQUENTIAL,
                                     previous_cycles);
        break;
    case 13u:
        result = gbl_bus_read(machine, vram_address, 2u,
                              GBL_BUS_CLASS_NONSEQUENTIAL);
        check_integration_bus_result(machine, result, GBL_BUS_OUTCOME_RAM,
                                     0x5A5Au, 1u,
                                     GBL_BUS_CLASS_NONSEQUENTIAL,
                                     previous_cycles);
        break;
    case 14u:
        result = gbl_bus_internal(machine, 7u);
        check_integration_bus_result(machine, result, GBL_BUS_OUTCOME_NONE,
                                     0u, 7u,
                                     GBL_BUS_CLASS_NONSEQUENTIAL,
                                     previous_cycles);
        break;
    default:
        CHECK(false);
        break;
    }
}

static int run_integration(void)
{
    enum { MACHINE_COUNT = 4, ROM_SIZE = 0x100, BUS_STEP_COUNT = 15 };
    host_storage storage[MACHINE_COUNT] = { 0 };
    gbl_machine *machines[MACHINE_COUNT] = { NULL };
    uint8_t inputs[MACHINE_COUNT][ROM_SIZE];
    uint8_t expected[ROM_SIZE];
    size_t index;
    unsigned int step;
    int transaction_result;

    transaction_result = run_transaction();
    if (transaction_result != 0) {
        return transaction_result;
    }

    for (index = 0u; index < MACHINE_COUNT; ++index) {
        fill_synthetic(inputs[index], sizeof(inputs[index]), 0x39u);
    }
    memcpy(expected, inputs[0], sizeof(expected));
    for (index = 1u; index < MACHINE_COUNT; ++index) {
        CHECK(memcmp(inputs[0], inputs[index], sizeof(expected)) == 0);
        CHECK(inputs[0] != inputs[index]);
    }

    for (index = 0u; index < MACHINE_COUNT; ++index) {
        gbl_load_result load;
        gbl_header_diagnostics diagnostics;

        machines[index] = new_machine(&storage[index]);
        if (machines[index] == NULL) {
            fprintf(stderr, "loader.integration: machine setup failed\n");
            break;
        }
        load = gbl_load_rom(machines[index], inputs[index], sizeof(inputs[index]));
        CHECK(load.status == GBL_LOAD_STATUS_OK);
        CHECK(load.reason == GBL_LOAD_REASON_NONE);
        CHECK(machines[index]->master_cycles == 0u);
        CHECK(gbl_rom_size(machines[index]) == sizeof(expected));
        CHECK(gbl_rom_data(machines[index]) != inputs[index]);
        CHECK(gbl_test_bytes_equal(gbl_rom_data(machines[index]), expected,
                                   sizeof(expected)));
        diagnostics = gbl_rom_diagnostics(machines[index]);
        CHECK(diagnostics.loaded);
        CHECK(diagnostics.canonical_logo == GBL_HEADER_LOGO_UNCHECKED);
        CHECK(diagnostics.logo_reason == GBL_HEADER_LOGO_REASON_OWNER_PENDING);
    }
    if (index != MACHINE_COUNT) {
        for (index = 0u; index < MACHINE_COUNT; ++index) {
            free_machine(machines[index]);
        }
        return 1;
    }

    for (index = 0u; index < MACHINE_COUNT; ++index) {
        CHECK(gbl_rom_data(machines[index]) != gbl_rom_data(machines[0]) ||
              index == 0u);
        inputs[index][7u] ^= (uint8_t)(0x10u << index);
        CHECK(gbl_rom_data(machines[index])[7u] == expected[7u]);
    }
    CHECK(gbl_test_canonical_equal(machines[0], machines[1]));
    CHECK(gbl_test_canonical_equal(machines[0], machines[2]));
    CHECK(gbl_test_canonical_equal(machines[0], machines[3]));

    for (step = 0u; step < BUS_STEP_COUNT; ++step) {
        run_integration_bus_step(machines[0], expected, step);
    }
    for (step = 0u; step < BUS_STEP_COUNT; ++step) {
        run_integration_bus_step(machines[1], expected, step);
        run_integration_bus_step(machines[2], expected, step);
    }
    CHECK(machines[0]->master_cycles == 21u);
    CHECK(machines[1]->master_cycles == 21u);
    CHECK(machines[2]->master_cycles == 21u);
    CHECK(gbl_test_canonical_equal(machines[0], machines[1]));
    CHECK(gbl_test_canonical_equal(machines[0], machines[2]));
    CHECK(gbl_test_canonical_digest(machines[0]) ==
          gbl_test_canonical_digest(machines[1]));
    CHECK(gbl_test_canonical_digest(machines[0]) ==
          gbl_test_canonical_digest(machines[2]));

    gbl_machine_reset(machines[1]);
    gbl_machine_reset(machines[1]);
    CHECK(gbl_rom_size(machines[1]) == sizeof(expected));
    CHECK(gbl_test_bytes_equal(gbl_rom_data(machines[1]), expected,
                               sizeof(expected)));
    CHECK(gbl_test_canonical_equal(machines[1], machines[3]));
    CHECK(gbl_test_canonical_digest(machines[1]) ==
          gbl_test_canonical_digest(machines[3]));

    gbl_machine_dispose(machines[0]);
    CHECK(storage[0].release_calls == 1u);
    CHECK(host_has_no_live_allocations(&storage[0]));
    CHECK(gbl_rom_data(machines[1]) != NULL);
    CHECK(gbl_rom_data(machines[2]) != NULL);
    CHECK(gbl_rom_data(machines[3]) != NULL);
    for (index = 1u; index < MACHINE_COUNT; ++index) {
        gbl_machine_dispose(machines[index]);
        CHECK(storage[index].release_calls == 1u);
        CHECK(host_has_no_live_allocations(&storage[index]));
        free(machines[index]);
    }
    gbl_machine_dispose(machines[0]);
    CHECK(storage[0].release_calls == 1u);
    free(machines[0]);

    if (CHECK_DONE() == 0) {
        puts("loader: integration ok");
    }
    return CHECK_DONE();
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr,
                "usage: loader_test lengths|transaction|diagnostics|integration|logo-digest\n");
        return 2;
    }
    if (strcmp(argv[1], "lengths") == 0) {
        return run_lengths();
    }
    if (strcmp(argv[1], "transaction") == 0) {
        return run_transaction();
    }
    if (strcmp(argv[1], "diagnostics") == 0) {
        return run_diagnostics();
    }
    if (strcmp(argv[1], "integration") == 0) {
        return run_integration();
    }
    if (strcmp(argv[1], "logo-digest") == 0) {
        return run_logo_digest();
    }
    fprintf(stderr, "loader_test: unknown selector: %s\n", argv[1]);
    return 2;
}
