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
    return block->image;
}

static void host_release(void *context, void *pointer, size_t size)
{
    host_storage *storage = context;
    size_t index;

    ++storage->release_calls;
    storage->last_release_size = size;
    storage->last_released_address = (uintptr_t)pointer;
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

    if (CHECK_DONE() == 0) {
        puts("loader: transaction ok");
    }
    return CHECK_DONE();
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: loader_test lengths|transaction\n");
        return 2;
    }
    if (strcmp(argv[1], "lengths") == 0) {
        return run_lengths();
    }
    if (strcmp(argv[1], "transaction") == 0) {
        return run_transaction();
    }
    fprintf(stderr, "loader_test: unknown selector: %s\n", argv[1]);
    return 2;
}
