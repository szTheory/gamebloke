#include "gamebloke/gamebloke.h"

gbl_peek_status gbl_machine_peek(const gbl_machine *machine,
                                uint32_t address, void *out, size_t length)
{
    (void)machine;
    (void)address;
    (void)out;
    (void)length;
    return GBL_PEEK_STATUS_INVALID_ARGUMENT;
}

gbl_mailbox_result gbl_mailbox_read(const gbl_machine *machine)
{
    (void)machine;
    return (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
}
