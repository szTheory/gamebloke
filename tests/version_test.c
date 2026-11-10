#include "check.h"
#include "gamebloke/gamebloke.h"

static gbl_machine machine;

int main(void)
{
    gbl_bus_result write_result;
    gbl_bus_result read_result;

    CHECK(gbl_version() != NULL);
    CHECK_STREQ(gbl_version(), "0.0.0");
    CHECK(gbl_machine_init(&machine, NULL) == GBL_BUS_STATUS_OK);
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
    gbl_machine_dispose(&machine);
    return CHECK_DONE();
}
