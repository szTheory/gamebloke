#include "gamebloke/gamebloke_v1.h"

#include "check.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct api_block {
    void *pointer;
    size_t size;
    int active;
} api_block;

typedef struct api_allocator {
    size_t allocate_calls;
    size_t release_calls;
    size_t fail_call;
    api_block blocks[8];
} api_allocator;

static void *api_allocate(void *context, size_t size)
{
    api_allocator *allocator = context;
    size_t index;

    ++allocator->allocate_calls;
    if (allocator->fail_call == allocator->allocate_calls) {
        return NULL;
    }
    for (index = 0u; index < sizeof(allocator->blocks) /
                            sizeof(allocator->blocks[0]); ++index) {
        if (!allocator->blocks[index].active) {
            void *pointer = malloc(size);

            if (pointer == NULL) {
                return NULL;
            }
            allocator->blocks[index].pointer = pointer;
            allocator->blocks[index].size = size;
            allocator->blocks[index].active = 1;
            return pointer;
        }
    }
    return NULL;
}

static void api_release(void *context, void *pointer, size_t size)
{
    api_allocator *allocator = context;
    size_t index;

    ++allocator->release_calls;
    for (index = 0u; index < sizeof(allocator->blocks) /
                            sizeof(allocator->blocks[0]); ++index) {
        api_block *block = &allocator->blocks[index];

        if (block->active && block->pointer == pointer) {
            CHECK(block->size == size);
            block->active = 0;
            block->pointer = NULL;
            block->size = 0u;
            free(pointer);
            return;
        }
    }
    CHECK(0 && "release must match a live caller allocation");
}

static void fill_nops(uint8_t *rom, size_t length)
{
    size_t index;

    memset(rom, 0, length);
    for (index = 0u; index + 3u < length; index += 4u) {
        rom[index] = 0x00u;
        rom[index + 1u] = 0x00u;
        rom[index + 2u] = 0xA0u;
        rom[index + 3u] = 0xE1u;
    }
}

static gbl_v1_config make_config(api_allocator *allocator)
{
    gbl_v1_config config = { 0 };

    config.struct_size = sizeof(config);
    config.profile = GBL_V1_PROFILE_AGB;
    config.start_mode = GBL_V1_START_DIRECT;
    config.cartridge_mode = GBL_V1_CARTRIDGE_ROM_ONLY;
    config.allocator_context = allocator;
    config.allocate = api_allocate;
    config.release = api_release;
    return config;
}

static int run_smoke(void)
{
    api_allocator allocator = { 0 };
    gbl_v1_machine *machine = NULL;
    gbl_v1_config config = make_config(&allocator);
    uint8_t rom_bytes[0xC0u];
    gbl_v1_rom rom = { sizeof(rom), rom_bytes, sizeof(rom_bytes) };
    gbl_v1_advance_options options = { 0 };
    gbl_v1_advance_result result = { 0 };
    gbl_v1_observation observation = { 0 };
    gbl_v1_advance_result refused_before;
    size_t index;

    allocator.fail_call = 1u;
    CHECK(gbl_v1_create(&config, &machine) == GBL_V1_ALLOCATION_FAILED);
    CHECK(machine == NULL);
    CHECK(allocator.release_calls == 0u);
    allocator.fail_call = 0u;
    CHECK(gbl_v1_create(&config, &machine) == GBL_V1_OK);
    CHECK(machine != NULL);
    CHECK(allocator.allocate_calls == 3u);

    fill_nops(rom_bytes, sizeof(rom_bytes));
    allocator.fail_call = allocator.allocate_calls + 1u;
    CHECK(gbl_v1_load_rom(machine, &rom) == GBL_V1_ALLOCATION_FAILED);
    CHECK(gbl_v1_observe(machine, &(gbl_v1_observation){
              .struct_size = sizeof(gbl_v1_observation) }) == GBL_V1_OK);
    allocator.fail_call = 0u;
    CHECK(gbl_v1_load_rom(machine, &rom) == GBL_V1_OK);
    CHECK(allocator.allocate_calls == 5u);
    memset(rom_bytes, 0xFF, sizeof(rom_bytes));

    options.struct_size = sizeof(options);
    options.units = 8u;
    options.cycle_budget = 1u;
    result.struct_size = sizeof(result);
    memset(&result.requested_units, 0xA5,
           sizeof(result) - offsetof(gbl_v1_advance_result, requested_units));
    refused_before = result;
    CHECK(gbl_v1_advance(machine, &options, &result) ==
          GBL_V1_WORK_EXHAUSTED);
    CHECK(memcmp(&result, &refused_before, sizeof(result)) == 0);

    options.cycle_budget = 4096u;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    CHECK(gbl_v1_advance(machine, &options, &result) == GBL_V1_OK);
    CHECK(result.requested_units == options.units);
    CHECK(result.end_cycle > result.start_cycle);
    CHECK(result.elapsed_cycles == result.end_cycle - result.start_cycle);
    CHECK(result.retired_instructions > 0u);
    CHECK(result.stop_reason == GBL_V1_STOP_UNIT_LIMIT);
    observation.struct_size = sizeof(observation);
    CHECK(gbl_v1_observe(machine, &observation) == GBL_V1_OK);
    CHECK(observation.loaded == 1u);
    CHECK(observation.master_cycle == result.end_cycle);
    CHECK(observation.retired_instructions >= result.retired_instructions);
    CHECK(allocator.release_calls == 0u);

    gbl_v1_destroy(machine);
    CHECK(allocator.release_calls == 3u);
    for (index = 0u; index < sizeof(allocator.blocks) /
                            sizeof(allocator.blocks[0]); ++index) {
        CHECK(!allocator.blocks[index].active);
    }
    return CHECK_DONE();
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: api_v1_test smoke\n");
        return 2;
    }
    if (strcmp(argv[1], "smoke") == 0) {
        return run_smoke();
    }
    fprintf(stderr, "api_v1_test: unknown selector: %s\n", argv[1]);
    return 2;
}
