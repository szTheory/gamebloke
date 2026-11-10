#include "gamebloke/gamebloke.h"

gbl_load_result gbl_load_rom(gbl_machine *machine, const uint8_t *image,
                             size_t length)
{
    gbl_load_result result = { GBL_LOAD_STATUS_INVALID_ARGUMENT,
                               GBL_LOAD_REASON_ARGUMENT };

    (void)machine;
    (void)image;
    (void)length;
    return result;
}

const uint8_t *gbl_rom_data(const gbl_machine *machine)
{
    (void)machine;
    return NULL;
}

size_t gbl_rom_size(const gbl_machine *machine)
{
    (void)machine;
    return 0u;
}
