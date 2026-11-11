#include "gamebloke/gamebloke.h"

#include <string.h>

static bool gbl_inspect_span_valid(uint32_t address, size_t length)
{
    const uint64_t iwram_start = UINT64_C(0x03000000);
    const uint64_t iwram_end = iwram_start + (uint64_t)GBL_IWRAM_SIZE;
    const uint64_t request_start = (uint64_t)address;
    uint64_t request_length;

    if (length == 0u) {
        return request_start >= iwram_start && request_start <= iwram_end;
    }
    if (length > GBL_IWRAM_SIZE || request_start < iwram_start ||
        request_start >= iwram_end) {
        return false;
    }
    request_length = (uint64_t)length;
    if (request_length > (uint64_t)UINT32_MAX - request_start) {
        return false;
    }
    return request_length <= iwram_end - request_start;
}

static bool gbl_inspect_pointer_range(const void *pointer, size_t length,
                                      uintptr_t *start, uintptr_t *end)
{
    uintptr_t first;

    if (pointer == NULL || start == NULL || end == NULL || length == 0u) {
        return false;
    }
    first = (uintptr_t)pointer;
    if ((uintmax_t)length > (uintmax_t)(UINTPTR_MAX - first)) {
        return false;
    }
    *start = first;
    *end = first + (uintptr_t)length;
    return true;
}

static bool gbl_inspect_ranges_overlap(uintptr_t left_start,
                                      uintptr_t left_end,
                                      uintptr_t right_start,
                                      uintptr_t right_end)
{
    return left_start < right_end && right_start < left_end;
}

static uint32_t gbl_inspect_read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) |
           ((uint32_t)bytes[3] << 24u);
}

static bool gbl_inspect_all_zero(const uint8_t *bytes, size_t length)
{
    size_t index;

    for (index = 0u; index < length; ++index) {
        if (bytes[index] != 0u) {
            return false;
        }
    }
    return true;
}

gbl_peek_status gbl_machine_peek(const gbl_machine *machine,
                                uint32_t address, void *out, size_t length)
{
    uintptr_t output_start;
    uintptr_t output_end;
    uintptr_t machine_start;
    uintptr_t machine_end;
    uintptr_t rom_start = 0u;
    uintptr_t rom_end = 0u;
    const uint32_t iwram_start = UINT32_C(0x03000000);
    const size_t offset = (size_t)(address - iwram_start);

    if (machine == NULL) {
        return GBL_PEEK_STATUS_INVALID_ARGUMENT;
    }
    if (!machine->initialized) {
        return GBL_PEEK_STATUS_INVALID_MACHINE;
    }
    if (!gbl_inspect_span_valid(address, length)) {
        return GBL_PEEK_STATUS_INVALID_SPAN;
    }
    if (length == 0u) {
        return GBL_PEEK_STATUS_OK;
    }
    if (out == NULL) {
        return GBL_PEEK_STATUS_INVALID_ARGUMENT;
    }
    if (!gbl_inspect_pointer_range(out, length, &output_start, &output_end) ||
        !gbl_inspect_pointer_range(machine, sizeof(*machine),
                                   &machine_start, &machine_end)) {
        return GBL_PEEK_STATUS_INVALID_SPAN;
    }
    if ((machine->rom == NULL) != (machine->rom_length == 0u)) {
        return GBL_PEEK_STATUS_INVALID_MACHINE;
    }
    if (machine->rom_length != 0u &&
        !gbl_inspect_pointer_range(machine->rom, machine->rom_length,
                                   &rom_start, &rom_end)) {
        return GBL_PEEK_STATUS_INVALID_MACHINE;
    }
    if (gbl_inspect_ranges_overlap(output_start, output_end, machine_start,
                                   machine_end) ||
        (machine->rom_length != 0u &&
         gbl_inspect_ranges_overlap(output_start, output_end, rom_start,
                                    rom_end))) {
        return GBL_PEEK_STATUS_OVERLAP;
    }

    memcpy(out, &machine->iwram[offset], length);
    return GBL_PEEK_STATUS_OK;
}

gbl_mailbox_result gbl_mailbox_read(const gbl_machine *machine)
{
    uint8_t mailbox[20];
    uint32_t magic;
    uint32_t version;
    uint32_t state;
    uint32_t case_id;
    uint32_t completion_marker;

    if (gbl_machine_peek(machine, GBL_MAILBOX_ADDRESS, mailbox,
                         sizeof(mailbox)) != GBL_PEEK_STATUS_OK) {
        return (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
    }
    if (gbl_inspect_all_zero(mailbox, sizeof(mailbox))) {
        return (gbl_mailbox_result){ GBL_MAILBOX_ABSENT, 0u };
    }

    magic = gbl_inspect_read_le32(&mailbox[0]);
    version = gbl_inspect_read_le32(&mailbox[4]);
    state = gbl_inspect_read_le32(&mailbox[8]);
    case_id = gbl_inspect_read_le32(&mailbox[12]);
    completion_marker = gbl_inspect_read_le32(&mailbox[16]);
    if (magic != GBL_MAILBOX_MAGIC || version != GBL_MAILBOX_VERSION) {
        return (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
    }

    if (state == (uint32_t)GBL_MAILBOX_STATE_ABSENT) {
        if (case_id == 0u && completion_marker == 0u) {
            return (gbl_mailbox_result){ GBL_MAILBOX_ABSENT, 0u };
        }
        return (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
    }
    if (state == (uint32_t)GBL_MAILBOX_STATE_RUNNING) {
        return (gbl_mailbox_result){ GBL_MAILBOX_PENDING, case_id };
    }
    if (state != (uint32_t)GBL_MAILBOX_STATE_PASS &&
        state != (uint32_t)GBL_MAILBOX_STATE_FAIL) {
        return (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
    }
    if (completion_marker != GBL_MAILBOX_COMPLETION_MARKER) {
        return (gbl_mailbox_result){ GBL_MAILBOX_PENDING, case_id };
    }
    if (state == (uint32_t)GBL_MAILBOX_STATE_FAIL && case_id == 0u) {
        return (gbl_mailbox_result){ GBL_MAILBOX_MALFORMED, 0u };
    }
    return (gbl_mailbox_result){
        state == (uint32_t)GBL_MAILBOX_STATE_PASS
            ? GBL_MAILBOX_PASS
            : GBL_MAILBOX_FAIL,
        case_id
    };
}
