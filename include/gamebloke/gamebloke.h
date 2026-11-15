#ifndef GAMEBLOKE_GAMEBLOKE_H
#define GAMEBLOKE_GAMEBLOKE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the project version string (for example "0.0.0"). The string is
 * static; the caller must not free or modify it. */
const char *gbl_version(void);

/* Canonical bus-backed machine storage. The host owns the machine object and
 * provides ROM allocation callbacks when loading is needed. This internal
 * structure is not a stable embedding ABI. */
#define GBL_EWRAM_SIZE 0x40000u
#define GBL_IWRAM_SIZE 0x8000u
#define GBL_PRAM_SIZE 0x400u
#define GBL_VRAM_SIZE 0x18000u
#define GBL_OAM_SIZE 0x400u

typedef struct gbl_allocator {
    void *context;
    void *(*allocate)(void *context, size_t size);
    void (*release)(void *context, void *pointer, size_t size);
} gbl_allocator;

typedef enum gbl_bus_status {
    GBL_BUS_STATUS_OK = 0,
    GBL_BUS_STATUS_INVALID_ARGUMENT = 1,
    GBL_BUS_STATUS_TIME_OVERFLOW = 2
} gbl_bus_status;

typedef enum gbl_bus_outcome {
    GBL_BUS_OUTCOME_NONE = 0,
    GBL_BUS_OUTCOME_RAM = 1,
    GBL_BUS_OUTCOME_BIOS_ABSENT = 2,
    GBL_BUS_OUTCOME_IO_UNIMPLEMENTED = 3,
    GBL_BUS_OUTCOME_ROM = 4,
    GBL_BUS_OUTCOME_ROM_TAIL = 5,
    GBL_BUS_OUTCOME_UNMAPPED = 6,
    GBL_BUS_OUTCOME_IO_MODE_SUBSET = 7,
    GBL_BUS_OUTCOME_VIDEO_INVALID_MODE_POLICY = 8
} gbl_bus_outcome;

typedef enum gbl_bus_access_class {
    GBL_BUS_CLASS_NONSEQUENTIAL = 0,
    GBL_BUS_CLASS_SEQUENTIAL = 1
} gbl_bus_access_class;

typedef struct gbl_bus_result {
    gbl_bus_status status;
    gbl_bus_outcome outcome;
    uint32_t value;
    uint64_t cycles;
    gbl_bus_access_class access_class;
} gbl_bus_result;

typedef enum gbl_load_status {
    GBL_LOAD_STATUS_OK = 0,
    GBL_LOAD_STATUS_REFUSED = 1,
    GBL_LOAD_STATUS_INVALID_ARGUMENT = 2,
    GBL_LOAD_STATUS_RESOURCE_FAILURE = 3
} gbl_load_status;

typedef enum gbl_load_reason {
    GBL_LOAD_REASON_NONE = 0,
    GBL_LOAD_REASON_ROM_ZERO = 1,
    GBL_LOAD_REASON_ROM_SHORT = 2,
    GBL_LOAD_REASON_ROM_TOO_LARGE = 3,
    GBL_LOAD_REASON_ROM_ODD = 4,
    GBL_LOAD_REASON_ARGUMENT = 5,
    GBL_LOAD_REASON_ALLOCATION = 6
} gbl_load_reason;

typedef struct gbl_load_result {
    gbl_load_status status;
    gbl_load_reason reason;
} gbl_load_result;

typedef enum gbl_header_logo_status {
    GBL_HEADER_LOGO_UNCHECKED = 0,
    GBL_HEADER_LOGO_CHECKED_MATCH = 1,
    GBL_HEADER_LOGO_CHECKED_MISMATCH = 2
} gbl_header_logo_status;

typedef enum gbl_header_logo_reason {
    GBL_HEADER_LOGO_REASON_OWNER_PENDING = 0,
    GBL_HEADER_LOGO_REASON_CHECKED = 1
} gbl_header_logo_reason;

typedef enum gbl_header_logo_window {
    GBL_HEADER_LOGO_WINDOW_ZERO = 0,
    GBL_HEADER_LOGO_WINDOW_NONZERO = 1
} gbl_header_logo_window;

typedef enum gbl_header_checksum {
    GBL_HEADER_CHECKSUM_NOT_LOADED = 0,
    GBL_HEADER_CHECKSUM_OK = 1,
    GBL_HEADER_CHECKSUM_MISMATCH = 2
} gbl_header_checksum;

typedef enum gbl_header_game_code {
    GBL_HEADER_GAME_CODE_NOT_LOADED = 0,
    GBL_HEADER_GAME_CODE_ORDINARY = 1,
    GBL_HEADER_GAME_CODE_UNUSUAL = 2
} gbl_header_game_code;

/* Bounded advisory findings for a successfully loaded direct-start image.
 * A false loaded field is the zero/not-loaded lifecycle representation. */
typedef struct gbl_header_diagnostics {
    bool loaded;
    gbl_header_logo_status canonical_logo;
    gbl_header_logo_reason logo_reason;
    gbl_header_logo_window logo_window;
    gbl_header_checksum checksum;
    gbl_header_game_code game_code;
} gbl_header_diagnostics;

typedef enum gbl_header_compare_result {
    GBL_HEADER_COMPARE_EQUAL = 0,
    GBL_HEADER_COMPARE_DIFFERENT = 1,
    GBL_HEADER_COMPARE_INVALID_ARGUMENT = 2
} gbl_header_compare_result;

typedef enum gbl_cpu_stop_reason {
    GBL_CPU_STOP_NONE = 0,
    GBL_CPU_STOP_UNSUPPORTED_INSTRUCTION = 1,
    GBL_CPU_STOP_BUS_ERROR = 2,
    GBL_CPU_STOP_COUNTER_OVERFLOW = 3,
    GBL_CPU_STOP_RESET_NO_BIOS = 4,
    GBL_CPU_STOP_UNDEFINED_NO_BIOS = 5,
    GBL_CPU_STOP_SWI_NO_BIOS = 6,
    GBL_CPU_STOP_PREFETCH_ABORT_NO_BIOS = 7,
    GBL_CPU_STOP_DATA_ABORT_NO_BIOS = 8,
    GBL_CPU_STOP_IRQ_NO_BIOS = 9,
    GBL_CPU_STOP_FIQ_NO_BIOS = 10,
    GBL_CPU_STOP_COPROCESSOR_NO_BIOS = 11,
    GBL_CPU_STOP_UNPREDICTABLE_INSTRUCTION = 12,
    GBL_CPU_STOP_LATER_ARCHITECTURE_INSTRUCTION = 13
} gbl_cpu_stop_reason;

typedef enum gbl_cpu_status {
    GBL_CPU_STATUS_OK = 0,
    GBL_CPU_STATUS_INVALID_ARGUMENT = 1,
    GBL_CPU_STATUS_INVALID_MODE = 2,
    GBL_CPU_STATUS_SPSR_ABSENT = 3,
    GBL_CPU_STATUS_INVALID_EXCEPTION = 4,
    GBL_CPU_STATUS_INVALID_CONTEXT = 5,
    GBL_CPU_STATUS_COUNTER_OVERFLOW = 6,
    GBL_CPU_STATUS_BUS_ERROR = 7,
    GBL_CPU_STATUS_MASKED = 8
} gbl_cpu_status;

typedef enum gbl_exception_kind {
    GBL_EXCEPTION_NONE = 0,
    GBL_EXCEPTION_RESET = 1,
    GBL_EXCEPTION_UNDEFINED = 2,
    GBL_EXCEPTION_SWI = 3,
    GBL_EXCEPTION_PREFETCH_ABORT = 4,
    GBL_EXCEPTION_DATA_ABORT = 5,
    GBL_EXCEPTION_IRQ = 6,
    GBL_EXCEPTION_FIQ = 7,
    GBL_EXCEPTION_COPROCESSOR = 8
} gbl_exception_kind;

/* Addresses are explicit architectural context, not offsets from an
 * implementation-visible program counter. The fault address identifies the
 * instruction that caused an abort; thumb_state records the pre-entry state. */
typedef struct gbl_cpu_exception_context {
    uint32_t current_address;
    uint32_t next_address;
    uint32_t fault_address;
    bool thumb_state;
} gbl_cpu_exception_context;

typedef struct gbl_cpu_last_exception {
    gbl_exception_kind kind;
    uint32_t current_address;
    uint32_t next_address;
    uint32_t fault_address;
    bool thumb_state;
} gbl_cpu_last_exception;

typedef struct gbl_cpu_pipeline_entry {
    uint32_t address;
    uint32_t opcode;
    uint8_t width;
    bool valid;
} gbl_cpu_pipeline_entry;

/* Per-instance ARM7TDMI state. Direct-start values are deterministic project
 * policy for agb/direct-start with no BIOS provider, not a BIOS handoff. */
typedef struct gbl_cpu_state {
    uint32_t r0_7[8];
    uint32_t usr_r8_14[7];
    uint32_t fiq_r8_14[7];
    uint32_t irq_r13_14[2];
    uint32_t svc_r13_14[2];
    uint32_t abt_r13_14[2];
    uint32_t und_r13_14[2];
    uint32_t spsr[5];
    uint32_t cpsr;
    uint32_t execution_address;
    gbl_cpu_pipeline_entry pipeline[2];
    bool next_fetch_sequential;
    uint64_t retired_instructions;
    bool stopped;
    gbl_cpu_stop_reason stop_reason;
    gbl_cpu_last_exception last_exception;
} gbl_cpu_state;

typedef enum gbl_run_status {
    GBL_RUN_STATUS_OK = 0,
    GBL_RUN_STATUS_INVALID_ARGUMENT = 1
} gbl_run_status;

typedef enum gbl_run_termination {
    GBL_RUN_PROGRESS = 0,
    GBL_RUN_TIMEOUT = 1,
    GBL_RUN_STOPPED = 2,
    GBL_RUN_TRACE_OUTPUT_FULL = 3
} gbl_run_termination;

typedef enum gbl_run_reason {
    GBL_RUN_REASON_NONE = 0,
    GBL_RUN_REASON_UNIT_LIMIT = 1,
    GBL_RUN_REASON_CYCLE_BUDGET = 2,
    GBL_RUN_REASON_UNSUPPORTED_INSTRUCTION = 3,
    GBL_RUN_REASON_BUS_ERROR = 4,
    GBL_RUN_REASON_COUNTER_OVERFLOW = 5,
    GBL_RUN_REASON_INVALID_CPU_STATE = 6,
    GBL_RUN_REASON_UNDEFINED_INSTRUCTION = 7,
    GBL_RUN_REASON_GUEST_STOP = 8,
    GBL_RUN_REASON_TRACE_OUTPUT_FULL = 9,
    GBL_RUN_REASON_UNPREDICTABLE_INSTRUCTION = 10,
    GBL_RUN_REASON_LATER_ARCHITECTURE_INSTRUCTION = 11
} gbl_run_reason;

typedef enum gbl_trace_category {
    GBL_TRACE_INSTRUCTION = 1u << 0,
    GBL_TRACE_BUS = 1u << 1,
    GBL_TRACE_EXCEPTION = 1u << 2
} gbl_trace_category;

#define GBL_TRACE_CATEGORY_MASK \
    ((uint32_t)(GBL_TRACE_INSTRUCTION | GBL_TRACE_BUS | GBL_TRACE_EXCEPTION))

/* All records are zero-initialized before applicable fields are set.
 * Instruction records use guest_cycle at unit start plus instruction_address
 * and opcode. Bus records use completion guest_cycle and the actual bus
 * result's value, width, access class and outcome; their instruction fields
 * identify the current unit. Initial refill records use the refill address
 * and opcode zero. Exception records use completion guest_cycle,
 * instruction identity and exception_kind. Fields not listed for a category
 * are zero. Sequence is assigned from the caller buffer's next_sequence. */
typedef struct gbl_trace_record {
    uint64_t sequence;
    uint64_t guest_cycle;
    gbl_trace_category category;
    uint32_t instruction_address;
    uint32_t opcode;
    uint32_t address;
    uint32_t value;
    uint8_t width;
    gbl_bus_access_class access_class;
    gbl_bus_outcome bus_outcome;
    gbl_exception_kind exception_kind;
} gbl_trace_record;

/* The host owns this append-only output. `count` is the number of valid records
 * in [0, count), and `next_sequence` is assigned to the next appended record.
 * The host owns the storage lifetime and must provide a writable object span
 * for records[0, capacity). */
typedef struct gbl_trace_buffer {
    gbl_trace_record *records;
    size_t capacity;
    size_t count;
    uint64_t next_sequence;
} gbl_trace_buffer;

typedef struct gbl_run_options {
    uint64_t cycle_budget;
    /* Zero allows as many complete units as fit in cycle_budget. */
    uint64_t unit_limit;
    uint32_t category_mask;
    /* Required only when category_mask selects at least one category. */
    gbl_trace_buffer *trace_buffer;
} gbl_run_options;

typedef struct gbl_run_result {
    gbl_run_termination termination;
    gbl_run_reason reason;
    uint64_t cycles;
    uint64_t instructions;
} gbl_run_result;

typedef struct gbl_machine {
    uint64_t master_cycles;
    uint32_t open_bus;
    uint16_t dispcnt;
    bool initialized;
    uint8_t ewram[GBL_EWRAM_SIZE];
    uint8_t iwram[GBL_IWRAM_SIZE];
    uint8_t pram[GBL_PRAM_SIZE];
    uint8_t vram[GBL_VRAM_SIZE];
    uint8_t oam[GBL_OAM_SIZE];
    uint8_t *rom;
    size_t rom_length;
    gbl_allocator allocator;
    gbl_header_diagnostics header_diagnostics;
    gbl_cpu_state cpu;
} gbl_machine;

/* Internal, read-only inspection of the physical IWRAM window. The caller
 * owns machine/out storage and provides synchronization and lifetime. */
typedef enum gbl_peek_status {
    GBL_PEEK_STATUS_OK = 0,
    GBL_PEEK_STATUS_INVALID_ARGUMENT = 1,
    GBL_PEEK_STATUS_INVALID_MACHINE = 2,
    GBL_PEEK_STATUS_INVALID_SPAN = 3,
    GBL_PEEK_STATUS_OVERLAP = 4
} gbl_peek_status;

#define GBL_MAILBOX_ADDRESS UINT32_C(0x03007FC0)
#define GBL_MAILBOX_MAGIC UINT32_C(0x47424C4B)
#define GBL_MAILBOX_VERSION UINT32_C(1)
#define GBL_MAILBOX_COMPLETION_MARKER UINT32_C(0x434F4D50)

/* Five little-endian words at offsets 0, 4, 8, 12, and 16: magic, version,
 * state, case ID, and completion marker. Guest code writes the marker last. */
typedef enum gbl_mailbox_state {
    GBL_MAILBOX_STATE_ABSENT = 0,
    GBL_MAILBOX_STATE_RUNNING = 1,
    GBL_MAILBOX_STATE_PASS = 2,
    GBL_MAILBOX_STATE_FAIL = 3
} gbl_mailbox_state;

typedef enum gbl_mailbox_status {
    GBL_MAILBOX_ABSENT = 0,
    GBL_MAILBOX_PENDING = 1,
    GBL_MAILBOX_PASS = 2,
    GBL_MAILBOX_FAIL = 3,
    GBL_MAILBOX_MALFORMED = 4
} gbl_mailbox_status;

typedef struct gbl_mailbox_result {
    gbl_mailbox_status status;
    uint32_t case_id;
} gbl_mailbox_result;

/* Initialize fresh caller-owned storage. A non-NULL allocator must be a
 * distinct descriptor with both callbacks present or both absent. */
gbl_bus_status gbl_machine_init(gbl_machine *machine,
                                const gbl_allocator *allocator);
void gbl_machine_reset(gbl_machine *machine);
void gbl_machine_dispose(gbl_machine *machine);

/* Copies only a contiguous, byte-aligned-or-unaligned span from physical IWRAM
 * (0x03000000 through 0x03007FFF). Zero-length requests accept addresses
 * through one-past-end and do not access out. Invalid requests leave out and
 * machine unchanged; output may not overlap machine or owned ROM storage. */
gbl_peek_status gbl_machine_peek(const gbl_machine *machine,
                                uint32_t address, void *out, size_t length);

/* Observes the five-word little-endian IWRAM mailbox. A verdict is complete
 * only with the expected magic, version, terminal state, and final marker. */
gbl_mailbox_result gbl_mailbox_read(const gbl_machine *machine);

/* Execute a bounded ARM subset through the canonical bus. One complete
 * refill or instruction is a unit; unit_limit 1 returns progress after one
 * such unit. A zero cycle budget performs no work. Selected records are
 * appended to caller-owned trace storage with zero-based sequence values;
 * instruction cycles are sampled at unit start, bus cycles at completion,
 * and exception cycles after entry. Non-applicable record fields are zero.
 * Capacity or sequence exhaustion returns GBL_RUN_TRACE_OUTPUT_FULL and
 * GBL_RUN_REASON_TRACE_OUTPUT_FULL before the next whole unit. The machine,
 * trace count, sequence and prior records are unchanged for that unit. An
 * unknown mask bit, missing buffer for a nonzero mask, count > capacity,
 * missing storage for nonzero capacity, wrapping record extent, or overlap of
 * trace storage with machine/ROM/options/result returns INVALID_ARGUMENT
 * without changing machine, result or trace output. Pointer lifetime and the
 * actual allocation extent remain the caller's responsibility. A zero mask
 * ignores trace_buffer entirely. */
gbl_run_status gbl_cpu_run(gbl_machine *machine,
                           const gbl_run_options *options,
                           gbl_run_result *result);

/* Internal CPU state operations. These contracts are not a stable embedding
 * ABI. Invalid and absent-bank operations leave state and output untouched.
 * Masked IRQ/FIQ requests return GBL_CPU_STATUS_MASKED without effects. */
gbl_cpu_status gbl_cpu_set_cpsr(gbl_machine *machine, uint32_t cpsr);
gbl_cpu_status gbl_cpu_get_spsr(const gbl_machine *machine,
                                uint32_t *spsr_out);
gbl_cpu_status gbl_cpu_enter_exception(
    gbl_machine *machine, gbl_exception_kind kind,
    const gbl_cpu_exception_context *context);

/* Width is a byte count: 1, 2, or 4. Reads and writes report their requested
 * sequence class and cost exactly one placeholder master cycle. */
gbl_bus_result gbl_bus_read(gbl_machine *machine, uint32_t address,
                            uint8_t width,
                            gbl_bus_access_class access_class);
gbl_bus_result gbl_bus_write(gbl_machine *machine, uint32_t address,
                             uint8_t width,
                             gbl_bus_access_class access_class,
                             uint32_t value);
gbl_bus_result gbl_bus_internal(gbl_machine *machine, uint64_t cycles);

/* Load an owned ROM copy through the host-supplied allocator. This internal
 * result contract is not a stable embedding ABI. */
gbl_load_result gbl_load_rom(gbl_machine *machine, const uint8_t *image,
                             size_t length);
const uint8_t *gbl_rom_data(const gbl_machine *machine);
size_t gbl_rom_size(const gbl_machine *machine);
gbl_header_diagnostics gbl_rom_diagnostics(const gbl_machine *machine);
gbl_header_compare_result gbl_header_compare_bytes(
    const uint8_t *actual, const uint8_t *expected, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* GAMEBLOKE_GAMEBLOKE_H */
