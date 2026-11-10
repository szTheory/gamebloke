# Phase 3: Bus API, Memory Map, and ROM Loading - Pattern Map

**Mapped:** 2026-11-10  
**Files analyzed:** 15 proposed or implied files (14 table rows; the inventory row names two files)  
**Analogs found:** 12 / 15 files (structural matches; no bus, loader, or machine-state implementation exists)

## File Classification

| New/Modified File | Role | Data Flow | Closest tracked Analog | Match Quality |
|-------------------|------|-----------|------------------------|---------------|
| `include/gamebloke/gamebloke.h` | model / API contract | request-response | `include/gamebloke/gamebloke.h` | role-match (existing declarations only) |
| `src/machine.c` | model | transform / lifecycle | — | none |
| `src/bus.c` | service | request-response | — | none |
| `src/loader.c` | service | transform | — | none |
| `CMakeLists.txt` | config | batch / build | `CMakeLists.txt` | exact |
| `tests/core.cmake` | test | batch / registration | `tests/core.cmake` | exact |
| `tests/bus_test.c` | test | request-response | `tests/version_test.c` | role-match (test structure only) |
| `tests/loader_test.c` | test | transform | `tests/version_test.c` | role-match (test structure only) |
| `profiles/agb.json` | config | transform / declarative data | `profiles/agb.json` | exact |
| `docs/profile-agb.md` | config / documentation | transform / declarative data | `docs/profile-agb.md` | exact |
| `cmake/CheckProfile.cmake` | utility / validator | batch | `cmake/CheckProfile.cmake` | exact |
| `tests/gates.cmake` | test | batch / registration | `tests/gates.cmake` | exact |
| `tests/gates/profile_case.cmake` | test | transform / mutation control | `tests/gates/profile_case.cmake` | exact |
| `tests/inventory/ci.txt`, `tests/inventory/asan.txt` | config | batch / inventory | same lane inventories | exact |

The filenames above follow `03-RESEARCH.md`'s recommended structure and validation map. BUS-03 initialization checks can live in `bus_test.c` under a `machine.*` CTest name; no separate machine test source is required by the research.

## Pattern Assignments

### `include/gamebloke/gamebloke.h` (API contract, request-response)

**Analog:** Existing declarations in `include/gamebloke/gamebloke.h` (tracked).

Lines 1-18 establish the public-header shape: include guard, C++ linkage guard, documented ownership/lifetime, declarations only, and close guards. Lines 8-12 explicitly say no lifecycle API exists yet, so they do not define bus, state, or loader behavior. Keep the Phase 3 surface minimal and do not treat it as a stable embedding ABI.

### `src/machine.c`, `src/bus.c`, and `src/loader.c` (core model/service)

**Analog:** No close production analog exists. The only tracked core translation unit, `src/version.c` lines 1-6, includes the project header and defines a small C function; it has no mutable machine state, memory access, allocator, or error-result pattern. Use it only for source/header inclusion style.

**Core boundary:** The executable purity rule is in `cmake/CheckCoreIncludes.cmake` lines 28-29: allowed angle-bracket headers are exactly `stddef.h`, `stdint.h`, `stdbool.h`, and `limits.h`; source extensions are scanned. `tests/core.cmake` lines 41-68 registers the real scan plus negative and positive controls. Core code must not copy `stdio.h`, `string.h`, `malloc`, `free`, or file APIs from test/host code. Keep allocation explicit through the host boundary and copy bytes with a small loop. This gate does not itself define the allocator callback contract.

**New behavior with no analog:** `machine.c` must explicitly initialize mutable state; `bus.c` must be the only owner of access timing and clock updates; `loader.c` must validate first, prepare a separate ROM candidate, then commit/reset/release on success. Preserve failure atomicity and make each refused-load reason stable. These are phase requirements from `03-CONTEXT.md`/`03-RESEARCH.md`, not patterns observed in existing C source.

### `tests/bus_test.c` and `tests/loader_test.c` (tests)

**Analog:** `tests/version_test.c` lines 1-9 and `tests/check.h` lines 1-35 (tracked). The test source includes `check.h` and the public header, runs checks in `main`, and returns `CHECK_DONE()`. `CHECK` reports the expression and source line; the harness intentionally has no third-party framework. This is only a harness pattern, not behavioral coverage for bus or loader code.

For bus tests, preserve raw address/width inputs and assert returned cycles/class plus clock/state effects. Cover mirrors, width cases, open bus, clock overflow, and deterministic fresh/reset state. For loader tests, use synthetic caller buffers and a controllable host allocator; check validation precedence before allocator/read activity, copied ownership after success, and canonical fields/arrays plus source bytes after every refusal/failure. Do not compare struct padding or pointer identities. Synthetic expected logo windows may exercise comparison code, but cannot verify the real Nintendo-logo digest.

### `CMakeLists.txt` and `tests/core.cmake` (build and test registration)

**Analogs:** `CMakeLists.txt` lines 13-15, 34-50, 56-80; `tests/core.cmake` lines 1-18 and 41-68; all tracked.

The project uses strict C17 with extensions off (top-level lines 13-15). `gamebloke_core` is currently built for `ci` and `asan` from an explicit source list and gets public `include`, private version definition, warning options, and the core-specific compiler flags (lines 43-50). Add the machine/bus/loader sources to that list while preserving the same lane condition and target settings. Tests are included from the top-level directory scope for both lanes; keep `include(tests/core.cmake)` and `gbl_add_test` at that scope because test properties are directory-scoped at the CMake 3.25 floor.

The current test registration pattern is:

```cmake
add_executable(version_test tests/version_test.c)
target_link_libraries(version_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME core.version
  LABELS core unit
  COMMAND $<TARGET_FILE:version_test>)
```

Use separate test executables for bus and loader behavior (or an explicit machine test target if split), link each against `gamebloke_core` and `gbl_warnings`, and register clear labels with `gbl_add_test`.

The canonical local/CI command is `cmake --workflow --preset ci` (`CMakeLists.txt` lines 34-38); plain `ctest` omits the workflow's inventory post-step. Keep any build or test wiring within the existing `ci` and `asan` lanes.

### `tests/inventory/ci.txt` and `tests/inventory/asan.txt` (lane inventories)

**Analog:** Both tracked inventories; `tests/inventory/ci.txt` lines 1-3 and `tests/inventory/asan.txt` lines 1-3.

`cmake/GblTests.cmake` lines 7-15, 25-55 records each test name, sorted/deduplicated labels, and optional `skip-ok`; lines 57-73 compare registrations with `tests/inventory/${GBL_LANE}.txt` and check junit after the test run. Add every registered bus/loader test to both lane inventories in byte-sorted order with labels matching `gbl_add_test`. A test absent from the inventory or silently skipped must remain visible as a configure/post-step failure.

### `profiles/agb.json`, `docs/profile-agb.md`, and `cmake/CheckProfile.cmake` (profile contract)

**Analogs:** The tracked profile, narrative, and validator.

The JSON timing entries at `profiles/agb.json` lines 9-18 pair each numeric `value` with a provenance `source` and citation. `docs/profile-agb.md` lines 3-13 states JSON is authoritative, synchronizes the narrative, and describes source labels. The validator's `gbl__profile_timing` at `cmake/CheckProfile.cmake` lines 78-143 enumerates expected keys, validates value/type/source/citation and disputes, then lines 145-179 checks arithmetic relationships. Extend the schema, documentation table, validator, and corresponding controls together for bus placeholder costs; mark project-selected placeholders as `IMPL` with explicit policy provenance, not hardware `REF` timing. Follow the existing version-bump rule when profile claims change.

### `tests/gates.cmake` and `tests/gates/profile_case.cmake` (profile validation controls)

**Analog:** Tracked profile gate registration in `tests/gates.cmake` lines 207-238. It runs the real JSON/document/registry/vocabulary validator, registers one negative control per reason code, and includes positive coordinated-change/copy controls. The case driver mutates copies of actual inputs. Extend this structure for new timing-profile validation reasons rather than baking bad fixtures into the repository.

## Shared Patterns

### Core purity and host ownership

**Sources:** `cmake/CheckCoreIncludes.cmake` lines 28-29 and `tests/core.cmake` lines 41-68. Apply to `machine.c`, `bus.c`, and `loader.c`. Only the four permitted freestanding headers may use angle brackets. The allocator/storage provider stays on the host side; no core heap or file access. Test helpers may use host headers because they are not core sources.

### Test reporting and required lanes

**Sources:** `tests/check.h` lines 13-35, `cmake/GblTests.cmake` lines 13-55, and both lane inventories. Apply to each new executable and gate. Use the in-tree check macros, register with `gbl_add_test`, and update sorted CI/ASan inventory entries. Keep `cmake --workflow --preset ci` as the canonical required command.

### Profile provenance and visible uncertainty

**Sources:** `profiles/agb.json` lines 9-18; `docs/profile-agb.md` lines 3-13; `cmake/CheckProfile.cmake` lines 78-143; `tests/gates.cmake` lines 207-238. Apply to profile timing changes: keep source labels and narrative synchronized, test validator failures with reason-specific mutations, and describe placeholder costs as project policy.

### LOAD-02 logo gate remains partial

The real logo digest is still owner-pending. `tests/gates.cmake` lines 80-86 shows the existing `SKIPPABLE` owner-gated digest check; `tests/inventory/ci.txt` line 238 explicitly marks it `skip-ok`. Preserve an explicit unchecked/unverified diagnostic when canonical comparison cannot run. Synthetic test-only data can prove comparison mechanics but does not close LOAD-02. Keep the real-logo mismatch acceptance gap visible in phase verification until the owner supplies the digest; do not embed Nintendo logo bytes or manufacture a replacement digest.

## No Analog Found

| File / behavior | Role | Data Flow | Reason |
|-----------------|------|-----------|--------|
| `src/machine.c` initialization/reset | model | transform / lifecycle | No machine-state implementation exists. |
| `src/bus.c` decode, memory effects, and clock | service | request-response | No address-space or timing implementation exists. |
| `src/loader.c` safe copy-on-load | service | transform | No ROM or other input loader exists; callbacks and atomic swap must be designed from phase constraints. |
| Hardware-specific bus and ROM tests | test | request-response / transform | Existing C test source is only a harness example; it does not model bus or loader behavior. |

## Metadata

**Analog search scope:** tracked root C sources, public header, CMake build/test wiring, core purity gate, profile schema/validator, profile test controls, and CI/ASan inventories.  
**Files scanned:** 13 tracked analog files.  
**Pattern extraction date:** 2026-11-10
