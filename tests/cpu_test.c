#include "gamebloke/gamebloke.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *gbl_cpu_test_allocate(void *context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void gbl_cpu_test_release(void *context, void *pointer, size_t size)
{
    (void)context;
    (void)size;
    free(pointer);
}

static bool test_loaded_machine_direct_start(void)
{
    uint8_t image[0xC0] = { 0u };
    const gbl_allocator allocator = {
        NULL, gbl_cpu_test_allocate, gbl_cpu_test_release
    };
    gbl_machine machine = { 0 };
    gbl_load_result mutable_load_result;

    const bool initialized =
        gbl_machine_init(&machine, &allocator) == GBL_BUS_STATUS_OK;
    if (!initialized) {
        return false;
    }
    mutable_load_result = gbl_load_rom(&machine, image, sizeof(image));
    {
        const bool passes =
            mutable_load_result.status == GBL_LOAD_STATUS_OK &&
            gbl_rom_size(&machine) == sizeof(image) &&
            machine.cpu.cpsr == UINT32_C(0x000000DF) &&
            machine.cpu.execution_address == UINT32_C(0x08000000) &&
            !machine.cpu.pipeline[0].valid &&
            !machine.cpu.pipeline[1].valid &&
            !machine.cpu.next_fetch_sequential;

        gbl_machine_dispose(&machine);
        return passes;
    }
}

int main(int argc, char **argv)
{
    bool passes;

    if (argc != 2) {
        return 2;
    }
    if (strcmp(argv[1], "lifecycle") == 0) {
        printf("TAP version 13\n1..1\n");
        passes = test_loaded_machine_direct_start();
        printf("%s 1 - loaded machine starts in direct-start ARM/System state\n",
               passes ? "ok" : "not ok");
    } else {
        return 2;
    }
    return passes ? 0 : 1;
}
