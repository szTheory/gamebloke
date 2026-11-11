# Phase 04: CPU Execution Core and Headless Runner - Pattern Map

**Mapped:** 2026-11-10
**Files analyzed:** 16 planned or modified file roles
**Analogs found:** 14 / 16 (two have no close implementation analog)

Names marked **proposed** are planning targets; they do not exist yet. Exact
private module and generated-file names remain planner choices.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `include/gamebloke/gamebloke.h` | public header / model | transform | same file | exact |
| proposed `src/cpu.c` and private CPU header | service / model | transform | `src/machine.c`, `src/bus.c` | partial; no CPU exists |
| `src/machine.c` | model / lifecycle | CRUD | same file | exact |
| `src/bus.c` | service / clock and trace seam | request-response, event-driven | same file | exact |
| `tests/state_check.h` | test utility / canonical state | transform | same file | exact |
| proposed `tests/cpu_test.c` | test / CPU behavior | transform | `tests/bus_test.c`, `tests/check.h` | role-match |
| `runner/main.c` | controller / CLI | request-response, file-I/O | same file | role-match; currently help/version only |
| proposed runner span-hash helper | utility / loaded-byte digest | transform | `src/logo_sha256.h` | partial; fixed-window only |
| `CMakeLists.txt` | config / core and runner build | transform | same file | exact |
| `cmake/CheckManifest.cmake`, `cmake/CheckResult.cmake` | config validators / authority and output contract | transform | same files | exact |
| proposed fixture-index generation | config / compiled metadata | transform | `CheckManifest.cmake`, `CMakeLists.txt` | role-match |
| `tests/core.cmake` | test registration / lane wiring | event-driven | same file | exact |
| `tests/gates/core_mutation.cmake` | test control / scratch compile | transform | same file | exact |
| profile files and gates (`profiles/agb.json`, `docs/profile-agb.md`, `CheckProfile.cmake`, profile tests) | config/docs / versioned profile | transform | `tests/gates/profile_case.cmake` | role-match |
| `docs/evidence-contract.md`, result producer checks | docs/test contract | request-response | `CheckResult.cmake`, `tests/gates/result_case.cmake` | role-match |
| proposed install-relocation test; `tests/inventory/{ci,asan,hygiene}.txt` | test/config / installed consumer and sorted inventories | file-I/O, batch | `tests/core.cmake`, `InventoryCompare.cmake` | partial; no install route |

## Pattern Assignments

### CPU state, lifecycle, and canonical comparison

**Sources:** `include/gamebloke/gamebloke.h`, `src/machine.c`,
`tests/state_check.h` (tracked).

Keep CPU architectural and pending-pipeline state per machine instance. The
public header already describes `gbl_machine` as caller-owned and not a stable
embedding ABI (lines 16-18); keep private decode helpers out of the public
contract. Extend machine clear/init/reset/dispose together and enumerate every
CPU field in the test equality and fixed-order digest. Preserve the current
separate comparisons for canonical content and same-instance ownership.

**Machine state excerpt** (`include/gamebloke/gamebloke.h`, lines 129-143):

```c
typedef struct gbl_machine {
    uint64_t master_cycles;
    uint32_t open_bus;
    uint16_t dispcnt;
    bool initialized;
    uint8_t ewram[GBL_EWRAM_SIZE];
    uint8_t iwram[GBL_IWRAM_SIZE];
    /* ROM, allocator and diagnostics fields follow. */
} gbl_machine;
```

**Lifecycle excerpt** (`src/machine.c`, lines 59-66):

```c
    machine->initialized = false;
    machine->rom = NULL;
    machine->rom_length = 0u;
    machine->header_diagnostics = (gbl_header_diagnostics){ 0 };
    machine->allocator = supplied;
    gbl_machine_clear_mutable_state(machine);
    machine->initialized = true;
```

**State check excerpt** (`tests/state_check.h`, lines 39-47):

```c
    if (left == NULL || right == NULL ||
        left->initialized != right->initialized ||
        left->master_cycles != right->master_cycles ||
        left->open_bus != right->open_bus ||
        left->dispcnt != right->dispcnt ||
        !gbl_test_diagnostics_equal(&left->header_diagnostics,
                                    &right->header_diagnostics) ||
        left->rom_length != right->rom_length ||
```

For mailbox peek, assert full equality, latch, trace count, and cycles before
and after. A digest alone is not the postcondition.

### Bus clock and trace seam; bounded execution

**Source:** `src/bus.c`, public read/write/internal operations (lines 325-437).

The bus is the existing single owner of emulated time. It validates and checks
overflow before effects, and commits cycles through successful bus access or
`gbl_bus_internal`. Route instruction fetch and data access through it; do not
add a CPU memory shortcut. Add trace observation around the real operation so
records describe bus activity and use committed outcomes/cycles. There is no
current trace API or buffer.

For hard cycle and trace bounds, preflight the whole next instruction/refill
unit and required record capacity before effects. If it cannot fit, stop before
that unit rather than partially applying it or dropping records. Preserve
explicit TIMEOUT / output-full semantics from context.

```c
    if (UINT64_C(1) > UINT64_MAX - machine->master_cycles) {
        return gbl_bus_error(GBL_BUS_STATUS_TIME_OVERFLOW, access_class);
    }
    next_cycles = machine->master_cycles + UINT64_C(1);
```

### CPU assertions and scratch compilation

**Sources:** `tests/check.h`, `tests/bus_test.c`, `tests/core.cmake`,
`tests/gates/core_mutation.cmake` (tracked).

Use the tiny `CHECK` harness and a selector-based C executable for CPU vectors.
Assert semantic state and cycle outcomes, including shift/carry boundaries,
bank and exception restoration, budget refusal, mailbox completion, and trace
capacity. The bus test provides machine setup/teardown and refusal checks that
compare both canonical content and instance identity (`bus_test.c`, lines
8-27, 298-323). Register via `gbl_add_test()` in `tests/core.cmake`.

**Assertion excerpt** (`tests/check.h`, lines 13-20):

```c
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
                    #cond);                                                  \
            gbl_check_failures++;                                            \
        }                                                                    \
    } while (0)
```

`CMakeLists.txt` has one shared `GBL_CORE_SOURCES` list for both configured
core targets (lines 64-76). The scratch mutation driver separately names
copied source/header files and compiler inputs (`core_mutation.cmake`, lines
53-90); update both lists and new private headers when adding CPU sources.
Otherwise mutation controls may compile a stale or different core.

### Manifest authority, generated fixture metadata, and runner JSON

**Sources:** `cmake/CheckManifest.cmake`, `cmake/CheckResult.cmake`,
`tests/gates.cmake`, `runner/main.c` (tracked).

Extend `runner/main.c` as a thin host adapter: bound file reads, hash exactly
the loaded bytes, escape emitted strings, and keep machine-local paths out of
JSON. Keep `gamebloke.result/1`, the closed status vocabulary, and claim tuple.
PASS must be bound to a validated fixture row, the actual ROM digest, and a
complete versioned mailbox verdict.

`gbl_manifest_read` exports parsed fixture ids and row fields (including class
and dispute) to its caller (`CheckManifest.cmake`, lines 449-490). For compiled
immutable metadata, use those validator outputs and registry/contract reads;
generate only after findings are made fatal/clear. The read function itself is
not the final validation gate. `CheckResult.cmake` already checks row identity,
digest/status rules, claim fields, and that manifest dispute ids and the
`disputed` label are disclosed (lines 111-169, 178-211, 380-455). Route actual
producer output through this existing checker and its real-input gates.

**Manifest reader excerpt** (`cmake/CheckManifest.cmake`, lines 449-458):

```cmake
function(gbl_manifest_read prefix manifest registry vocab)
  gbl_json_load(doc "${manifest}")

  gbl_json_string(schema "${doc}" schema)
  if(NOT "${schema}" STREQUAL "gamebloke.manifest/1")
    gbl_finding(manifest bad-schema "${manifest}")
  endif()

  set(ids "")
  set(count 0)
```

`src/logo_sha256.h` is tracked but its implementation hashes exactly a 156-byte
header window (`lines 7-13, 81-82`); it is not a whole-ROM helper. No tracked C
source offers bounded arbitrary-span SHA-256. CMake `file(SHA256)` is configure-
time only, not a runner implementation.

### Profile, test inventories, and relocation

CPU startup policy changes affect profile claims: sync profile version,
concrete fields, narrative, `cmake/CheckProfile.cmake`, and profile positive and
negative controls. Existing registrations are in `tests/gates.cmake` (lines
207-237) and profile checks finish through `gbl_findings_finish`.

Register new tests through `gbl_add_test` at top-level scope and update each
applicable sorted `tests/inventory/*.txt`. `cmake/GblTests.cmake` sorts labels
and compares registered names to the selected inventory (lines 13-63);
`cmake/InventoryCompare.cmake` rejects unsorted or mismatched inventories
(lines 21-59).

There is no tracked `install()` rule or installed-consumer test analog. Add a
relocation check that installs under a relative prefix, moves it, runs from an
unrelated working directory using explicit input paths, and compares output
with the build-tree runner. Generated metadata must travel with or be compiled
into the executable; runtime must not need source paths or CMake.

## Shared Patterns

- **State and refusal:** centralize clearing in `src/machine.c`; compare every
  state field explicitly in `tests/state_check.h`; assert documented no-effect
  refusal postconditions.
- **Time and traces:** route guest work through `src/bus.c`; preflight the next
  bounded unit and trace capacity before any effects.
- **Build and checks:** maintain the common core list plus scratch compile
  lists, and register every test in the byte-sorted lane inventories.
- **Evidence:** use validated manifest/registry/vocabulary data for generated
  fixture metadata and validate the actual producer JSON with
  `cmake/CheckResult.cmake`.

## No Analog Found

| File/role | Reason |
|---|---|
| Proposed runner whole-span SHA-256 helper | Existing C helper is fixed-window; CMake hashing is configure-time only. |
| Proposed install-relocation test | No current install rules or installed-consumer tests exist. |

## Metadata

**Analog search scope:** tracked `include/`, `src/`, `runner/`, `tests/`,
`cmake/`, root `CMakeLists.txt`, profile, contract, and docs.
**Tracked-source gate:** every named analog was verified with `git ls-files`;
no ignored runtime/install mirror path is named.
**Pattern extraction date:** 2026-11-10

## PATTERN MAPPING COMPLETE
