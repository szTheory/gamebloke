#include "check.h"
#include "gamebloke/gamebloke.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_IMAGE_SIZE 0xC0u
#define TIMER0_IF_BIT UINT16_C(0x0008)
#define TIMER0_MARKER UINT32_C(0x54494D45)
#define TIMER0_MARKER_ADDRESS UINT32_C(0x03000100)
#define TIMER0_IF_ADDRESS UINT32_C(0x04000202)

typedef struct guest_observation {
    gbl_run_result run;
    uint32_t marker;
    uint16_t interrupt_flags;
} guest_observation;

static void *test_allocate(void *context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void test_release(void *context, void *pointer, size_t size)
{
    (void)context;
    (void)size;
    free(pointer);
}

static void write_word(uint8_t *bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)(value & UINT32_C(0xFF));
    bytes[offset + 1u] = (uint8_t)((value >> 8u) & UINT32_C(0xFF));
    bytes[offset + 2u] = (uint8_t)((value >> 16u) & UINT32_C(0xFF));
    bytes[offset + 3u] = (uint8_t)(value >> 24u);
}

static void make_guest_image(uint8_t image[TEST_IMAGE_SIZE], bool enable_timer)
{
    memset(image, 0, TEST_IMAGE_SIZE);

    /* Original ARM instructions: store reload/control, poll IF, and write
     * the semantic marker only after observing Timer 0's request bit. */
    write_word(image, 0x00u, UINT32_C(0xE59F002C)); /* LDR r0, timer base */
    write_word(image, 0x04u, UINT32_C(0xE59F102C)); /* LDR r1, timer setup */
    write_word(image, 0x08u, UINT32_C(0xE5801000)); /* STR r1, [r0] */
    write_word(image, 0x0Cu, UINT32_C(0xE59F2028)); /* LDR r2, IF address */
    write_word(image, 0x10u, UINT32_C(0xE3A04008)); /* MOV r4, #8 */
    write_word(image, 0x14u, UINT32_C(0xE1D230B0)); /* LDRH r3, [r2] */
    write_word(image, 0x18u, UINT32_C(0xE1130004)); /* TST r3, r4 */
    write_word(image, 0x1Cu, UINT32_C(0x1A000000)); /* BNE marker */
    write_word(image, 0x20u, UINT32_C(0xEAFFFFFB)); /* B poll */
    write_word(image, 0x24u, UINT32_C(0xE59F2014)); /* LDR r2, marker addr */
    write_word(image, 0x28u, UINT32_C(0xE59F3014)); /* LDR r3, marker */
    write_word(image, 0x2Cu, UINT32_C(0xE5823000)); /* STR r3, [r2] */
    write_word(image, 0x30u, UINT32_C(0xEAFFFFFE)); /* B stop */

    write_word(image, 0x34u, UINT32_C(0x04000100));
    write_word(image, 0x38u, enable_timer ? UINT32_C(0x00C0FFF0)
                                         : UINT32_C(0x0000FFF0));
    write_word(image, 0x3Cu, TIMER0_IF_ADDRESS);
    write_word(image, 0x40u, TIMER0_MARKER_ADDRESS);
    write_word(image, 0x44u, TIMER0_MARKER);
}

static bool run_guest(bool enable_timer, guest_observation *observation)
{
    uint8_t image[TEST_IMAGE_SIZE];
    gbl_machine *machine;
    gbl_run_options options = { UINT64_C(10000), 0u, 0u, NULL };
    gbl_allocator allocator = { NULL, test_allocate, test_release };
    gbl_bus_result if_read;
    gbl_load_result load;
    uint8_t marker_bytes[sizeof(observation->marker)] = { 0u };
    size_t index;

    if (observation == NULL) {
        return false;
    }
    *observation = (guest_observation){ 0 };
    make_guest_image(image, enable_timer);
    machine = calloc(1u, sizeof(*machine));
    if (machine == NULL) {
        return false;
    }
    if (gbl_machine_init(machine, &allocator) != GBL_BUS_STATUS_OK) {
        free(machine);
        return false;
    }
    load = gbl_load_rom(machine, image, sizeof(image));
    if (load.status != GBL_LOAD_STATUS_OK ||
        load.reason != GBL_LOAD_REASON_NONE) {
        gbl_machine_dispose(machine);
        free(machine);
        return false;
    }
    if (gbl_cpu_run(machine, &options, &observation->run) !=
        GBL_RUN_STATUS_OK) {
        gbl_machine_dispose(machine);
        free(machine);
        return false;
    }
    if (gbl_machine_peek(machine, TIMER0_MARKER_ADDRESS, marker_bytes,
                         sizeof(marker_bytes)) != GBL_PEEK_STATUS_OK) {
        gbl_machine_dispose(machine);
        free(machine);
        return false;
    }
    for (index = 0u; index < sizeof(marker_bytes); ++index) {
        observation->marker |= (uint32_t)marker_bytes[index]
                              << (index * 8u);
    }

    /* Avoid treating a prior instruction fetch on open bus as an IF request. */
    machine->open_bus = 0u;
    if_read = gbl_bus_read(machine, TIMER0_IF_ADDRESS, 2u,
                           GBL_BUS_CLASS_NONSEQUENTIAL);
    if (if_read.status != GBL_BUS_STATUS_OK) {
        gbl_machine_dispose(machine);
        free(machine);
        return false;
    }
    observation->interrupt_flags = (uint16_t)if_read.value;
    gbl_machine_dispose(machine);
    free(machine);
    return true;
}

static int run_tracer(void)
{
    guest_observation first;
    guest_observation second;
    guest_observation disabled;

    CHECK(run_guest(true, &first));
    CHECK(run_guest(true, &second));
    CHECK(run_guest(false, &disabled));

    CHECK(first.marker == TIMER0_MARKER);
    CHECK((first.interrupt_flags & TIMER0_IF_BIT) != 0u);
    CHECK(first.run.cycles > 0u);
    CHECK(second.marker == TIMER0_MARKER);
    CHECK((second.interrupt_flags & TIMER0_IF_BIT) != 0u);
    CHECK(first.run.cycles == second.run.cycles);
    CHECK(first.marker == second.marker);
    CHECK(disabled.marker == 0u);
    CHECK(disabled.run.termination == GBL_RUN_TIMEOUT);
    CHECK(disabled.run.reason == GBL_RUN_REASON_CYCLE_BUDGET);
    CHECK((disabled.interrupt_flags & TIMER0_IF_BIT) == 0u);
    if (disabled.marker == 0u &&
        disabled.run.reason == GBL_RUN_REASON_CYCLE_BUDGET) {
        puts("EXPECTED timer0-overflow-timeout");
    }
    if (CHECK_DONE() == 0) {
        puts("timer0.guest.tracer: semantic overflow and disabled control ok");
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: timer_test tracer\n");
        return 2;
    }
    if (strcmp(argv[1], "tracer") == 0) {
        return run_tracer();
    }
    fprintf(stderr, "timer_test: unknown selector: %s\n", argv[1]);
    return 2;
}
