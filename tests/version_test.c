#include "check.h"
#include "gamebloke/gamebloke.h"

static gbl_machine machine;
static uint8_t allocation_storage[0xC0u];
static unsigned int allocate_calls;
static unsigned int release_calls;
static size_t allocated_size;

static void *test_allocate(void *context, size_t size)
{
    (void)context;
    ++allocate_calls;
    allocated_size = size;
    if (size != sizeof(allocation_storage)) {
        return NULL;
    }
    return allocation_storage;
}

static void test_release(void *context, void *pointer, size_t size)
{
    (void)context;
    CHECK(pointer == allocation_storage);
    CHECK(size == allocated_size);
    ++release_calls;
}

int main(void)
{
    gbl_bus_result write_result;
    gbl_bus_result read_result;
    gbl_load_result load_result;
    const uint8_t *owned_image;
    uint8_t image[0xC0u];
    uint8_t first_byte;
    size_t index;
    gbl_allocator allocator = { NULL, test_allocate, test_release };

    CHECK(gbl_version() != NULL);
    CHECK_STREQ(gbl_version(), "0.0.0");
    CHECK(gbl_machine_init(&machine, &allocator) == GBL_BUS_STATUS_OK);
    CHECK(machine.initialized);
    CHECK(machine.master_cycles == 0u);
    CHECK(machine.ewram[0] == 0u);
    CHECK(machine.ewram[GBL_EWRAM_SIZE - 1u] == 0u);

    write_result = gbl_bus_write(&machine, 0x02000000u, 4u,
                                 GBL_BUS_CLASS_NONSEQUENTIAL,
                                 0xA1B2C3D4u);
    CHECK(write_result.status == GBL_BUS_STATUS_OK);
    CHECK(write_result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(write_result.cycles == 1u);
    CHECK(write_result.access_class == GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(machine.master_cycles == 1u);

    read_result = gbl_bus_read(&machine, 0x02000000u, 4u,
                               GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(read_result.status == GBL_BUS_STATUS_OK);
    CHECK(read_result.outcome == GBL_BUS_OUTCOME_RAM);
    CHECK(read_result.value == 0xA1B2C3D4u);
    CHECK(read_result.cycles == 1u);
    CHECK(read_result.access_class == GBL_BUS_CLASS_SEQUENTIAL);
    CHECK(machine.master_cycles == 2u);

    for (index = 0u; index < sizeof(image); ++index) {
        image[index] = (uint8_t)(index * 37u + (index >> 2u) + 11u);
    }
    first_byte = image[5u];
    load_result = gbl_load_rom(&machine, image, sizeof(image));
    CHECK(load_result.status == GBL_LOAD_STATUS_OK);
    CHECK(load_result.reason == GBL_LOAD_REASON_NONE);
    CHECK(allocate_calls == 1u);
    CHECK(machine.rom != image);
    owned_image = gbl_rom_data(&machine);
    CHECK(owned_image == allocation_storage);
    CHECK(gbl_rom_size(&machine) == sizeof(image));
    if (owned_image != NULL) {
        for (index = 0u; index < sizeof(image); ++index) {
            CHECK(owned_image[index] == image[index]);
        }
    }
    image[5u] ^= 0x80u;
    if (owned_image != NULL) {
        CHECK(owned_image[5u] == first_byte);
    }

    gbl_machine_dispose(&machine);
    CHECK(release_calls == 1u);
    return CHECK_DONE();
}
