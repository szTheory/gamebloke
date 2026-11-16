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

typedef struct time_snapshot {
    uint64_t master_cycles;
    uint64_t next_event_sequence;
    uint16_t interrupt_flags;
    uint32_t open_bus;
    size_t event_count;
    gbl_event events[GBL_EVENT_CAPACITY];
    gbl_timer_state timers[GBL_TIMER_COUNT];
} time_snapshot;

static gbl_machine *new_empty_machine(void)
{
    gbl_machine *machine = calloc(1u, sizeof(*machine));

    if (machine == NULL ||
        gbl_machine_init(machine, NULL) != GBL_BUS_STATUS_OK) {
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

static bool event_equal(const gbl_event *left, const gbl_event *right)
{
    return left->cycle == right->cycle &&
           left->sequence == right->sequence &&
           left->generation == right->generation &&
           left->priority == right->priority && left->kind == right->kind &&
           left->timer_index == right->timer_index &&
           left->live == right->live;
}

static bool timer_equal(const gbl_timer_state *left,
                        const gbl_timer_state *right)
{
    return left->reload == right->reload && left->counter == right->counter &&
           left->divisor == right->divisor && left->phase == right->phase &&
           left->anchor_cycle == right->anchor_cycle &&
           left->generation == right->generation &&
           left->enabled == right->enabled &&
           left->irq_enabled == right->irq_enabled &&
           left->cascade == right->cascade;
}

static void take_time_snapshot(const gbl_machine *machine,
                               time_snapshot *snapshot)
{
    size_t index;

    snapshot->master_cycles = machine->master_cycles;
    snapshot->next_event_sequence = machine->next_event_sequence;
    snapshot->interrupt_flags = machine->interrupt_flags;
    snapshot->open_bus = machine->open_bus;
    snapshot->event_count = machine->event_count;
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        snapshot->events[index] = machine->events[index];
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        snapshot->timers[index] = machine->timers[index];
    }
}

static bool time_matches_snapshot(const gbl_machine *machine,
                                  const time_snapshot *snapshot)
{
    size_t index;

    if (machine->master_cycles != snapshot->master_cycles ||
        machine->next_event_sequence != snapshot->next_event_sequence ||
        machine->interrupt_flags != snapshot->interrupt_flags ||
        machine->open_bus != snapshot->open_bus ||
        machine->event_count != snapshot->event_count) {
        return false;
    }
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        if (!event_equal(&machine->events[index], &snapshot->events[index])) {
            return false;
        }
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        if (!timer_equal(&machine->timers[index], &snapshot->timers[index])) {
            return false;
        }
    }
    return true;
}

static bool preview_equal(const gbl_machine_time_preview *left,
                          const gbl_machine_time_preview *right)
{
    size_t index;

    if (left->master_cycles != right->master_cycles ||
        left->next_event_sequence != right->next_event_sequence ||
        left->interrupt_flags != right->interrupt_flags ||
        left->event_count != right->event_count) {
        return false;
    }
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        if (!event_equal(&left->events[index], &right->events[index])) {
            return false;
        }
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        if (!timer_equal(&left->timers[index], &right->timers[index])) {
            return false;
        }
    }
    return true;
}

static gbl_bus_result write_timer_word(gbl_machine *machine,
                                       uint8_t timer_index, uint16_t reload,
                                       uint8_t control)
{
    uint32_t address = UINT32_C(0x04000100) +
                       ((uint32_t)timer_index * UINT32_C(4));
    uint32_t value = ((uint32_t)control << 16u) | reload;

    return gbl_bus_write(machine, address, 4u, GBL_BUS_CLASS_NONSEQUENTIAL,
                         value);
}

static int run_event_order(void)
{
    gbl_machine *first = new_empty_machine();
    gbl_machine *second = new_empty_machine();
    gbl_bus_result write_result;
    bool valid = first != NULL && second != NULL;

    if (valid) {
        write_result = write_timer_word(first, 0u, UINT16_C(0xFFFE),
                                        UINT8_C(0xC0));
        valid = valid && write_result.status == GBL_BUS_STATUS_OK;
        write_result = write_timer_word(first, 1u, UINT16_C(0xFFFF),
                                        UINT8_C(0xC0));
        valid = valid && write_result.status == GBL_BUS_STATUS_OK;
        write_result = write_timer_word(second, 0u, UINT16_C(0xFFFE),
                                        UINT8_C(0xC0));
        valid = valid && write_result.status == GBL_BUS_STATUS_OK;
        write_result = write_timer_word(second, 1u, UINT16_C(0xFFFF),
                                        UINT8_C(0xC0));
        valid = valid && write_result.status == GBL_BUS_STATUS_OK;
    }
    CHECK(valid);
    CHECK(first->event_count == 2u);
    CHECK(first->events[0].cycle == UINT64_C(3));
    CHECK(first->events[1].cycle == UINT64_C(3));
    CHECK(first->events[0].priority == 0u);
    CHECK(first->events[1].priority == 1u);
    CHECK(first->events[0].sequence < first->events[1].sequence);
    CHECK(gbl_machine_advance_to(first, UINT64_C(3)) == GBL_TIME_STATUS_OK);
    CHECK(gbl_machine_advance_to(second, UINT64_C(3)) == GBL_TIME_STATUS_OK);
    CHECK(first->interrupt_flags == UINT16_C(0x0018));
    CHECK(first->interrupt_flags == second->interrupt_flags);
    CHECK(first->master_cycles == second->master_cycles);
    CHECK(first->event_count == second->event_count);
    CHECK(first->next_event_sequence == second->next_event_sequence);
    CHECK(event_equal(&first->events[0], &second->events[0]));
    CHECK(event_equal(&first->events[1], &second->events[1]));
    free_machine(first);
    free_machine(second);
    if (CHECK_DONE() == 0) {
        puts("time.events.order: deterministic equal-cycle priority order ok");
        return 0;
    }
    return 1;
}

static int run_event_boundary(void)
{
    gbl_machine *machine = new_empty_machine();
    gbl_machine_time_preview preview = { 0 };
    gbl_bus_result write_result;
    gbl_bus_result read_result;
    bool valid = machine != NULL;

    if (valid) {
        write_result = write_timer_word(machine, 0u, UINT16_C(0xFFFE),
                                        UINT8_C(0xC0));
        valid = write_result.status == GBL_BUS_STATUS_OK;
    }
    CHECK(valid);
    CHECK(machine->master_cycles == UINT64_C(1));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(2)) ==
          GBL_TIME_STATUS_OK);
    CHECK(machine->interrupt_flags == 0u);
    CHECK(gbl_machine_prepare_time(machine, UINT64_C(3), &preview) ==
          GBL_TIME_STATUS_OK);
    CHECK(machine->master_cycles == UINT64_C(2));
    CHECK(machine->interrupt_flags == 0u);
    CHECK(preview.interrupt_flags == TIMER0_IF_BIT);
    read_result = gbl_bus_read(machine, TIMER0_IF_ADDRESS, 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(read_result.status == GBL_BUS_STATUS_OK);
    CHECK((read_result.value & TIMER0_IF_BIT) != 0u);
    CHECK(machine->master_cycles == UINT64_C(3));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(4)) ==
          GBL_TIME_STATUS_OK);
    CHECK((machine->interrupt_flags & TIMER0_IF_BIT) != 0u);

    free_machine(machine);
    machine = new_empty_machine();
    CHECK(machine != NULL);
    write_result = write_timer_word(machine, 0u, UINT16_C(0xFFFF),
                                    UINT8_C(0xC0));
    CHECK(write_result.status == GBL_BUS_STATUS_OK);
    CHECK(machine->event_count == 1u);
    machine->events[0].generation = 0u;
    CHECK(gbl_machine_advance_to(machine, UINT64_C(2)) ==
          GBL_TIME_STATUS_OK);
    CHECK(machine->interrupt_flags == 0u);
    CHECK(machine->event_count == 0u);
    gbl_machine_reset(machine);
    CHECK(machine->master_cycles == 0u);
    CHECK(machine->interrupt_flags == 0u);
    CHECK(machine->event_count == 0u);
    CHECK(machine->next_event_sequence == 0u);
    CHECK(!machine->timers[0].enabled);
    CHECK(machine->timers[0].generation == 0u);
    free_machine(machine);
    if (CHECK_DONE() == 0) {
        puts("time.events.boundary: before/at/after, stale event, and reset ok");
        return 0;
    }
    return 1;
}

static int run_event_refusal(void)
{
    gbl_machine *machine = new_empty_machine();
    gbl_machine_time_preview preview = { 0 };
    gbl_machine_time_preview sentinel = { 0 };
    time_snapshot snapshot;
    gbl_bus_result result;
    uint8_t index;

    CHECK(machine != NULL);
    result = write_timer_word(machine, 0u, UINT16_C(0xFFFF),
                              UINT8_C(0xC0));
    CHECK(result.status == GBL_BUS_STATUS_OK);
    sentinel.master_cycles = UINT64_C(17);
    sentinel.next_event_sequence = UINT64_C(23);
    sentinel.event_count = 2u;
    sentinel.events[0].cycle = UINT64_C(99);
    sentinel.timers[0].reload = UINT16_C(0x1234);
    preview = sentinel;
    CHECK(gbl_machine_prepare_time(machine, UINT64_C(6000), &preview) ==
          GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT);
    CHECK(preview_equal(&preview, &sentinel));
    take_time_snapshot(machine, &snapshot);
    CHECK(gbl_machine_advance_to(machine, UINT64_C(6000)) ==
          GBL_TIME_STATUS_EVENT_DISPATCH_LIMIT);
    CHECK(time_matches_snapshot(machine, &snapshot));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(0)) ==
          GBL_TIME_STATUS_BACKWARD);
    CHECK(time_matches_snapshot(machine, &snapshot));
    free_machine(machine);

    machine = new_empty_machine();
    CHECK(machine != NULL);
    machine->next_event_sequence = UINT64_MAX;
    take_time_snapshot(machine, &snapshot);
    result = write_timer_word(machine, 0u, UINT16_C(0xFFFF),
                              UINT8_C(0xC0));
    CHECK(result.status == GBL_BUS_STATUS_TIME_REFUSAL);
    CHECK(time_matches_snapshot(machine, &snapshot));
    free_machine(machine);

    machine = new_empty_machine();
    CHECK(machine != NULL);
    machine->timers[0].generation = UINT64_MAX;
    take_time_snapshot(machine, &snapshot);
    result = write_timer_word(machine, 0u, UINT16_C(0xFFFF),
                              UINT8_C(0xC0));
    CHECK(result.status == GBL_BUS_STATUS_TIME_REFUSAL);
    CHECK(time_matches_snapshot(machine, &snapshot));
    free_machine(machine);

    machine = new_empty_machine();
    CHECK(machine != NULL);
    machine->event_count = GBL_EVENT_CAPACITY;
    machine->next_event_sequence = GBL_EVENT_CAPACITY;
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        machine->events[index] = (gbl_event){
            UINT64_MAX, index, 0u, 1u,
            (uint8_t)GBL_EVENT_KIND_TIMER_OVERFLOW, 1u, true
        };
    }
    take_time_snapshot(machine, &snapshot);
    result = write_timer_word(machine, 0u, UINT16_C(0xFFFF),
                              UINT8_C(0xC0));
    CHECK(result.status == GBL_BUS_STATUS_TIME_REFUSAL);
    CHECK(time_matches_snapshot(machine, &snapshot));
    free_machine(machine);

    if (CHECK_DONE() == 0) {
        puts("time.events.refusal: named limits preserve machine and output state");
        return 0;
    }
    return 1;
}

static uint16_t timer_counter_from_preview(
    const gbl_machine_time_preview *preview, uint8_t timer_index)
{
    uint32_t address = UINT32_C(0x04000100) +
                       ((uint32_t)timer_index * UINT32_C(4));
    uint16_t low = gbl_timer_read(preview, address);
    uint16_t high = gbl_timer_read(preview, address + 1u);

    return (uint16_t)(low | (uint16_t)(high << 8u));
}

#define TIMER_CHECK(name, condition)                                        \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "timer.devices[%s]: failed: %s\n", name,     \
                    #condition);                                           \
            gbl_check_failures++;                                           \
        }                                                                   \
    } while (0)

static bool current_timer_counter(gbl_machine *machine, uint8_t timer_index,
                                 uint16_t *counter)
{
    gbl_machine_time_preview preview = { 0 };

    if (machine == NULL || counter == NULL || timer_index >= GBL_TIMER_COUNT ||
        gbl_machine_prepare_time(machine, machine->master_cycles, &preview) !=
            GBL_TIME_STATUS_OK) {
        return false;
    }
    *counter = timer_counter_from_preview(&preview, timer_index);
    return true;
}

static bool time_snapshot_equal(const time_snapshot *left,
                                const time_snapshot *right)
{
    size_t index;

    if (left->master_cycles != right->master_cycles ||
        left->next_event_sequence != right->next_event_sequence ||
        left->interrupt_flags != right->interrupt_flags ||
        left->open_bus != right->open_bus ||
        left->event_count != right->event_count) {
        return false;
    }
    for (index = 0u; index < GBL_EVENT_CAPACITY; ++index) {
        if (!event_equal(&left->events[index], &right->events[index])) {
            return false;
        }
    }
    for (index = 0u; index < GBL_TIMER_COUNT; ++index) {
        if (!timer_equal(&left->timers[index], &right->timers[index])) {
            return false;
        }
    }
    return true;
}

static int run_timer_model_boundary(void)
{
    gbl_machine *machine = new_empty_machine();
    gbl_machine_time_preview preview = { 0 };
    gbl_bus_result write_result;
    gbl_bus_result read_result;

    CHECK(machine != NULL);
    write_result = write_timer_word(machine, 0u, UINT16_C(0xFFF0),
                                    UINT8_C(0xC1));
    CHECK(write_result.status == GBL_BUS_STATUS_OK);
    CHECK(gbl_machine_prepare_time(machine, UINT64_C(64), &preview) ==
          GBL_TIME_STATUS_OK);
    CHECK(timer_counter_from_preview(&preview, 0u) == UINT16_C(0xFFF0));
    CHECK(gbl_machine_prepare_time(machine, UINT64_C(65), &preview) ==
          GBL_TIME_STATUS_OK);
    CHECK(timer_counter_from_preview(&preview, 0u) == UINT16_C(0xFFF1));
    CHECK(machine->master_cycles == UINT64_C(1));
    CHECK(gbl_machine_advance_to(machine, UINT64_C(32)) ==
          GBL_TIME_STATUS_OK);
    read_result = gbl_bus_write(machine, UINT32_C(0x04000102), 1u,
                                GBL_BUS_CLASS_NONSEQUENTIAL,
                                UINT32_C(0xC1));
    CHECK(read_result.status == GBL_BUS_STATUS_OK);
    CHECK(machine->master_cycles == UINT64_C(33));
    CHECK(gbl_machine_prepare_time(machine, UINT64_C(96), &preview) ==
          GBL_TIME_STATUS_OK);
    CHECK(timer_counter_from_preview(&preview, 0u) == UINT16_C(0xFFF0));
    CHECK(gbl_machine_prepare_time(machine, UINT64_C(97), &preview) ==
          GBL_TIME_STATUS_OK);
    CHECK(timer_counter_from_preview(&preview, 0u) == UINT16_C(0xFFF1));
    free_machine(machine);

    machine = new_empty_machine();
    CHECK(machine != NULL);
    write_result = write_timer_word(machine, 0u, UINT16_C(0xFFFE),
                                    UINT8_C(0xC0));
    CHECK(write_result.status == GBL_BUS_STATUS_OK);
    write_result = gbl_bus_write(machine, UINT32_C(0x04000100), 2u,
                                 GBL_BUS_CLASS_NONSEQUENTIAL,
                                 UINT32_C(0xFFF0));
    CHECK(write_result.status == GBL_BUS_STATUS_OK);
    read_result = gbl_bus_read(machine, UINT32_C(0x04000100), 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL);
    CHECK(read_result.status == GBL_BUS_STATUS_OK);
    CHECK((uint16_t)read_result.value == UINT16_C(0xFFF0));
    free_machine(machine);
    if (CHECK_DONE() == 0) {
        puts("timer0.model.boundary: start phase, control reset, reload policy ok");
        return 0;
    }
    return 1;
}

static int run_timer_devices(void)
{
    static const uint16_t reloads[] = {
        UINT16_C(0x0000), UINT16_C(0x0001), UINT16_C(0xFFFE),
        UINT16_C(0xFFFF)
    };
    static const uint16_t divisors[] = { 1u, 64u, 256u, 1024u };
    gbl_machine *machine;
    gbl_bus_result result;
    size_t timer_index;
    size_t reload_index;
    size_t divisor_index;

    /* Exercise every reload/divisor pair through each timer's real register
     * decode and the shared machine event queue. */
    for (timer_index = 0u; timer_index < GBL_TIMER_COUNT; ++timer_index) {
        for (reload_index = 0u;
             reload_index < sizeof(reloads) / sizeof(reloads[0]);
             ++reload_index) {
            for (divisor_index = 0u;
                 divisor_index < sizeof(divisors) / sizeof(divisors[0]);
                 ++divisor_index) {
                const uint8_t control = (uint8_t)(UINT8_C(0x80) |
                                                   (uint8_t)divisor_index);
                const uint64_t interval =
                    (UINT64_C(0x10000) - reloads[reload_index]) *
                    divisors[divisor_index];
                const uint64_t deadline = UINT64_C(1) + interval;
                uint16_t counter = 0u;

                machine = new_empty_machine();
                TIMER_CHECK("all-reload-divisor-setup", machine != NULL);
                if (machine == NULL) {
                    continue;
                }
                result = write_timer_word(machine, (uint8_t)timer_index,
                                          reloads[reload_index], control);
                TIMER_CHECK("all-reload-divisor-write",
                            result.status == GBL_BUS_STATUS_OK);
                TIMER_CHECK("one-next-overflow-event",
                            machine->event_count == 1u &&
                                machine->events[0].cycle == deadline);
                TIMER_CHECK("pre-overflow-boundary",
                            gbl_machine_advance_to(machine, deadline - 1u) ==
                                GBL_TIME_STATUS_OK &&
                                current_timer_counter(machine,
                                    (uint8_t)timer_index, &counter) &&
                                counter == UINT16_MAX);
                TIMER_CHECK("overflow-reloads-counter",
                            gbl_machine_advance_to(machine, deadline) ==
                                GBL_TIME_STATUS_OK &&
                                current_timer_counter(machine,
                                    (uint8_t)timer_index, &counter) &&
                                counter == reloads[reload_index]);
                TIMER_CHECK("next-overflow-is-strictly-later",
                            machine->event_count == 1u &&
                                machine->events[0].cycle ==
                                    deadline + interval);
                free_machine(machine);
            }
        }
    }

    machine = new_empty_machine();
    TIMER_CHECK("live-policy-setup", machine != NULL);
    if (machine != NULL) {
        uint64_t generation;
        uint16_t counter = 0u;

        result = write_timer_word(machine, 0u, UINT16_MAX, UINT8_C(0x81));
        TIMER_CHECK("prescaler-start", result.status == GBL_BUS_STATUS_OK);
        generation = machine->timers[0].generation;
        TIMER_CHECK("same-control-write", gbl_machine_advance_to(
                        machine, UINT64_C(20)) == GBL_TIME_STATUS_OK &&
                        gbl_bus_write(machine, UINT32_C(0x04000102), 1u,
                                      GBL_BUS_CLASS_NONSEQUENTIAL,
                                      UINT32_C(0x81)).status ==
                            GBL_BUS_STATUS_OK &&
                        machine->timers[0].generation == generation &&
                        machine->events[0].cycle == UINT64_C(65));
        TIMER_CHECK("live-reload-only-next-overflow",
                    gbl_bus_write(machine, UINT32_C(0x04000100), 2u,
                                  GBL_BUS_CLASS_NONSEQUENTIAL,
                                  UINT32_C(0xFF00)).status ==
                            GBL_BUS_STATUS_OK &&
                        machine->events[0].cycle == UINT64_C(65) &&
                        gbl_machine_advance_to(machine, UINT64_C(65)) ==
                            GBL_TIME_STATUS_OK &&
                        current_timer_counter(machine, 0u, &counter) &&
                        counter == UINT16_C(0xFF00));
        free_machine(machine);
    }

    machine = new_empty_machine();
    TIMER_CHECK("disable-policy-setup", machine != NULL);
    if (machine != NULL) {
        uint16_t counter = 0u;

        result = write_timer_word(machine, 0u, UINT16_C(0xFFFC),
                                  UINT8_C(0x80));
        TIMER_CHECK("disable-policy-start", result.status ==
                        GBL_BUS_STATUS_OK);
        TIMER_CHECK("disable-retains-synchronized-count",
                    gbl_machine_advance_to(machine, UINT64_C(3)) ==
                            GBL_TIME_STATUS_OK &&
                        gbl_bus_write(machine, UINT32_C(0x04000102), 1u,
                                      GBL_BUS_CLASS_NONSEQUENTIAL, 0u).status ==
                            GBL_BUS_STATUS_OK &&
                        current_timer_counter(machine, 0u, &counter) &&
                        counter == UINT16_MAX && machine->event_count == 0u);
        TIMER_CHECK("disabled-reload-does-not-replace-count",
                    gbl_bus_write(machine, UINT32_C(0x04000100), 2u,
                                  GBL_BUS_CLASS_NONSEQUENTIAL,
                                  UINT32_C(0xF000)).status ==
                            GBL_BUS_STATUS_OK &&
                        current_timer_counter(machine, 0u, &counter) &&
                        counter == UINT16_MAX);
        TIMER_CHECK("reenable-loads-latest-reload",
                    gbl_bus_write(machine, UINT32_C(0x04000102), 1u,
                                  GBL_BUS_CLASS_NONSEQUENTIAL,
                                  UINT32_C(0x80)).status ==
                            GBL_BUS_STATUS_OK &&
                        current_timer_counter(machine, 0u, &counter) &&
                        counter == UINT16_C(0xF000));
        free_machine(machine);
    }

    machine = new_empty_machine();
    TIMER_CHECK("cascade-policy-setup", machine != NULL);
    if (machine != NULL) {
        const gbl_cpu_state cpu_before = machine->cpu;

        TIMER_CHECK("cascade-timer3", write_timer_word(
                        machine, 3u, UINT16_MAX, UINT8_C(0xC4)).status ==
                        GBL_BUS_STATUS_OK);
        TIMER_CHECK("cascade-timer2", write_timer_word(
                        machine, 2u, UINT16_MAX, UINT8_C(0xC4)).status ==
                        GBL_BUS_STATUS_OK);
        TIMER_CHECK("cascade-timer1-irq-disabled", write_timer_word(
                        machine, 1u, UINT16_MAX, UINT8_C(0x84)).status ==
                        GBL_BUS_STATUS_OK);
        TIMER_CHECK("cascade-source", write_timer_word(
                        machine, 0u, UINT16_C(0xFFFE), UINT8_C(0xC0)).status ==
                        GBL_BUS_STATUS_OK);
        TIMER_CHECK("cascade-has-only-independent-event",
                    machine->event_count == 1u &&
                        machine->events[0].timer_index == 0u &&
                        machine->next_event_sequence == 1u);
        TIMER_CHECK("four-timer-synchronous-cascade",
                    gbl_machine_advance_to(machine, UINT64_C(6)) ==
                            GBL_TIME_STATUS_OK &&
                        machine->interrupt_flags == UINT16_C(0x0068) &&
                        machine->timers[1].counter == UINT16_MAX &&
                        machine->timers[2].counter == UINT16_MAX &&
                        machine->timers[3].counter == UINT16_MAX &&
                        machine->event_count == 1u &&
                        machine->events[0].timer_index == 0u &&
                        machine->events[0].cycle == UINT64_C(8) &&
                        machine->next_event_sequence == 2u);
        TIMER_CHECK("device-request-does-not-enter-cpu",
                    machine->cpu.cpsr == cpu_before.cpsr &&
                        machine->cpu.execution_address ==
                            cpu_before.execution_address &&
                        machine->cpu.retired_instructions ==
                            cpu_before.retired_instructions &&
                        machine->cpu.last_exception.kind ==
                            GBL_EXCEPTION_NONE && !machine->cpu.stopped);
        free_machine(machine);
    }

    machine = new_empty_machine();
    TIMER_CHECK("tie-policy-setup", machine != NULL);
    if (machine != NULL) {
        const gbl_cpu_state cpu_before = machine->cpu;

        result = write_timer_word(machine, 0u, UINT16_MAX, UINT8_C(0xC0));
        TIMER_CHECK("tie-policy-start", result.status == GBL_BUS_STATUS_OK);
        result = gbl_bus_write(machine, TIMER0_IF_ADDRESS, 2u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               TIMER0_IF_BIT);
        TIMER_CHECK("if-clear-follows-tied-overflow",
                    result.status == GBL_BUS_STATUS_OK &&
                        machine->master_cycles == UINT64_C(2) &&
                        machine->interrupt_flags == 0u);
        TIMER_CHECK("masked-cpu-keeps-device-request-separate",
                    machine->cpu.cpsr == cpu_before.cpsr &&
                        machine->cpu.execution_address ==
                            cpu_before.execution_address &&
                        machine->cpu.last_exception.kind ==
                            GBL_EXCEPTION_NONE && !machine->cpu.stopped);
        free_machine(machine);
    }

    machine = new_empty_machine();
    TIMER_CHECK("crossing-policy-setup", machine != NULL);
    if (machine != NULL) {
        result = gbl_bus_write(machine, UINT32_C(0x04000103), 4u,
                               GBL_BUS_CLASS_NONSEQUENTIAL,
                               UINT32_C(0xC0FFFE00));
        TIMER_CHECK("cross-timer-boundary-lanes",
                    result.status == GBL_BUS_STATUS_OK &&
                        machine->timers[1].reload == UINT16_C(0xFFFE) &&
                        machine->timers[1].enabled &&
                        machine->timers[1].irq_enabled &&
                        machine->event_count == 1u &&
                        machine->events[0].timer_index == 1u &&
                        machine->events[0].cycle == UINT64_C(3));
        free_machine(machine);
    }

    {
        gbl_machine *first = new_empty_machine();
        gbl_machine *second = new_empty_machine();
        time_snapshot first_snapshot;
        time_snapshot second_snapshot;

        TIMER_CHECK("interleaved-policy-setup",
                    first != NULL && second != NULL);
        if (first != NULL && second != NULL) {
            TIMER_CHECK("interleaved-first-start", write_timer_word(
                            first, 0u, UINT16_C(0xFFFE), UINT8_C(0xC0)).status ==
                            GBL_BUS_STATUS_OK);
            TIMER_CHECK("interleaved-second-start", write_timer_word(
                            second, 0u, UINT16_C(0xFFFE), UINT8_C(0xC0)).status ==
                            GBL_BUS_STATUS_OK);
            TIMER_CHECK("interleaved-advance-a",
                        gbl_machine_advance_to(first, UINT64_C(3)) ==
                            GBL_TIME_STATUS_OK);
            TIMER_CHECK("interleaved-advance-b",
                        gbl_machine_advance_to(second, UINT64_C(2)) ==
                            GBL_TIME_STATUS_OK);
            TIMER_CHECK("interleaved-advance-a-again",
                        gbl_machine_advance_to(first, UINT64_C(8)) ==
                            GBL_TIME_STATUS_OK);
            TIMER_CHECK("interleaved-advance-b-again",
                        gbl_machine_advance_to(second, UINT64_C(8)) ==
                            GBL_TIME_STATUS_OK);
            take_time_snapshot(first, &first_snapshot);
            take_time_snapshot(second, &second_snapshot);
            TIMER_CHECK("interleaved-machines-match-isolated-state",
                        time_snapshot_equal(&first_snapshot,
                                            &second_snapshot));
        }
        free_machine(first);
        free_machine(second);
    }

    if (CHECK_DONE() == 0) {
        puts("timer.devices: four-timer event, cascade, IF and lane policies ok");
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
    if (strcmp(argv[1], "time.events.order") == 0) {
        return run_event_order();
    }
    if (strcmp(argv[1], "time.events.boundary") == 0) {
        return run_event_boundary();
    }
    if (strcmp(argv[1], "time.events.refusal") == 0) {
        return run_event_refusal();
    }
    if (strcmp(argv[1], "timer0.model.boundary") == 0) {
        return run_timer_model_boundary();
    }
    if (strcmp(argv[1], "timer.devices") == 0) {
        return run_timer_devices();
    }
    fprintf(stderr, "timer_test: unknown selector: %s\n", argv[1]);
    return 2;
}
