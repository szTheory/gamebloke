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
    GBL_HEADER_LOGO_REASON_OWNER_PENDING = 0
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
} gbl_machine;

/* Initialize fresh caller-owned storage. A non-NULL allocator must be a
 * distinct descriptor with both callbacks present or both absent. */
gbl_bus_status gbl_machine_init(gbl_machine *machine,
                                const gbl_allocator *allocator);
void gbl_machine_reset(gbl_machine *machine);
void gbl_machine_dispose(gbl_machine *machine);

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
