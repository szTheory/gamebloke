cmake_minimum_required(VERSION 3.25)
project(gamebloke VERSION 0.0.0 LANGUAGES C)

# Compiler identity guard. CI sets GBL_EXPECT_C_ID (GNU, Clang, AppleClang) so
# "Linux GCC" and "macOS arm64" are proven by configure, not assumed from a
# runner label. Unset or empty means no expectation (local builds).
if(NOT "$ENV{GBL_EXPECT_C_ID}" STREQUAL ""
    AND NOT "$ENV{GBL_EXPECT_C_ID}" STREQUAL "${CMAKE_C_COMPILER_ID}")
  message(FATAL_ERROR
    "gamebloke: compiler-id-mismatch: expected $ENV{GBL_EXPECT_C_ID} got ${CMAKE_C_COMPILER_ID}")
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Lane selection. Each lane has its own sorted test inventory under
# tests/inventory/<lane>.txt. Later plans add each lane's content.
set(GBL_LANE "ci" CACHE STRING
  "Verification lane: ci | asan | hygiene | diagnostics")
set_property(CACHE GBL_LANE PROPERTY STRINGS ci asan hygiene diagnostics)
if(NOT GBL_LANE MATCHES "^(ci|asan|hygiene|diagnostics)$")
  message(FATAL_ERROR
    "gamebloke: unknown-lane: '${GBL_LANE}' (expected ci, asan, hygiene, or diagnostics)")
endif()

set(GBL_DIAGNOSTIC_STAGE "admitted" CACHE STRING
  "Diagnostic fixture authority stage: proof | admitted")
set_property(CACHE GBL_DIAGNOSTIC_STAGE PROPERTY STRINGS proof admitted)
if(NOT GBL_DIAGNOSTIC_STAGE MATCHES "^(proof|admitted)$")
  message(FATAL_ERROR
    "gamebloke: unknown-diagnostic-stage: '${GBL_DIAGNOSTIC_STAGE}'")
endif()
set(GBL_TOOLCHAIN_ROOT "${CMAKE_SOURCE_DIR}/build/toolchains" CACHE PATH
  "Ignored cache root for the pinned ARM diagnostics toolchain")
set(GBL_DIAGNOSTIC_GROUP "all" CACHE STRING "Diagnostic output ownership: all | arm | thumb")
if(NOT GBL_DIAGNOSTIC_GROUP MATCHES "^(all|arm|thumb)$")
  message(FATAL_ERROR "gamebloke: invalid-diagnostic-group")
endif()
option(GBL_REQUIRE_DIAGNOSTICS
  "Require a verified host ARM diagnostics toolchain during configure" OFF)

if(GBL_LANE MATCHES "^(ci|asan|diagnostics)$" OR GBL_REQUIRE_DIAGNOSTICS)
  execute_process(COMMAND uname -s RESULT_VARIABLE gbl_uname_system_rc
    OUTPUT_VARIABLE gbl_uname_system OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND uname -m RESULT_VARIABLE gbl_uname_machine_rc
    OUTPUT_VARIABLE gbl_uname_machine OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${gbl_uname_system_rc}" STREQUAL "0" OR
     NOT "${gbl_uname_machine_rc}" STREQUAL "0")
    message(FATAL_ERROR "gamebloke: diagnostics-host-detection-failed")
  endif()
  if(gbl_uname_system STREQUAL "Darwin" AND
     gbl_uname_machine MATCHES "^(arm64|aarch64)$")
    set(GBL_DIAGNOSTIC_HOST darwin-arm64)
    set(GBL_DIAGNOSTIC_OUTPUT_LEAF native)
  elseif(gbl_uname_system STREQUAL "Linux" AND
     gbl_uname_machine MATCHES "^(x86_64|amd64)$")
    set(GBL_DIAGNOSTIC_HOST linux-x64)
    set(GBL_DIAGNOSTIC_OUTPUT_LEAF hosted-linux-x64)
  elseif(gbl_uname_system STREQUAL "Linux" AND
     gbl_uname_machine MATCHES "^(aarch64|arm64)$")
    set(GBL_DIAGNOSTIC_HOST linux-arm64)
    set(GBL_DIAGNOSTIC_OUTPUT_LEAF linux-arm64)
  else()
    message(FATAL_ERROR
      "gamebloke: unsupported-diagnostics-host: ${gbl_uname_system}/${gbl_uname_machine}")
  endif()
  cmake_path(ABSOLUTE_PATH GBL_TOOLCHAIN_ROOT BASE_DIRECTORY "${CMAKE_SOURCE_DIR}"
    NORMALIZE OUTPUT_VARIABLE GBL_TOOLCHAIN_ROOT_ABSOLUTE)
  if(GBL_REQUIRE_DIAGNOSTICS)
    execute_process(
      COMMAND sh "${CMAKE_SOURCE_DIR}/scripts/fetch-toolchain.sh" --check
        --host "${GBL_DIAGNOSTIC_HOST}" --root "${GBL_TOOLCHAIN_ROOT_ABSOLUTE}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      RESULT_VARIABLE gbl_toolchain_check_rc
      OUTPUT_VARIABLE gbl_toolchain_check_out
      ERROR_VARIABLE gbl_toolchain_check_error)
    if(NOT "${gbl_toolchain_check_rc}" STREQUAL "0")
      message(FATAL_ERROR
        "gamebloke: required-diagnostics-toolchain-invalid: ${gbl_toolchain_check_error}${gbl_toolchain_check_out}")
    endif()
  endif()
endif()

# Warning set (Pitfall 11). Warnings become errors through the preset cache
# variable CMAKE_COMPILE_WARNING_AS_ERROR, not a hardcoded -Werror.
add_library(gbl_warnings INTERFACE)
target_compile_options(gbl_warnings INTERFACE
  "$<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wshadow;-Wstrict-prototypes;-Wmissing-prototypes;-Wundef;-Wcast-qual>")

# Not include(CTest): that adds Dart/dashboard targets we do not want.
enable_testing()

# NOTE: running ctest alone is NOT the verify command. The verify command is
#   cmake --workflow --preset ci
# which reconfigures (deleting any stale junit.xml), builds, runs the tests, and
# then runs the post step that checks the junit inventory.
# A stale junit.xml must never satisfy the post step (Pitfall 5).
file(REMOVE "${CMAKE_BINARY_DIR}/junit.xml")

include(cmake/GblTests.cmake)

if(GBL_LANE STREQUAL "ci" OR GBL_LANE STREQUAL "asan" OR
   GBL_LANE STREQUAL "diagnostics")
  include(cmake/ReadLogoDigest.cmake)
  set(GBL_LOGO_AUTHORITY "${CMAKE_SOURCE_DIR}/tests/signatures/logo.sha256")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${GBL_LOGO_AUTHORITY}")
  gbl_read_logo_digest("${GBL_LOGO_AUTHORITY}" GBL_LOGO_EXPECTED_SHA256)

  set(GBL_LOGO_PATTERN_FILE "${CMAKE_BINARY_DIR}/logo-digest/pattern1through156.bin")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/logo-digest")
  set(GBL_LOGO_PATTERN_BYTES "")
  foreach(value RANGE 1 156)
    string(ASCII ${value} character)
    string(APPEND GBL_LOGO_PATTERN_BYTES "${character}")
  endforeach()
  file(WRITE "${GBL_LOGO_PATTERN_FILE}" "${GBL_LOGO_PATTERN_BYTES}")
  file(SHA256 "${GBL_LOGO_PATTERN_FILE}" GBL_LOGO_SYNTHETIC_SHA256)
  if(NOT GBL_LOGO_SYNTHETIC_SHA256 STREQUAL
      "8ec8d30af0acb1e6ff15d8ea332a0e6b60ab60ab41cef968e07c7706b349d9d1")
    message(FATAL_ERROR "gamebloke: logo-digest: synthetic-vector-mismatch")
  endif()

  # Both targets compile the same host-independent core sources and options;
  # only their private configure-time expectation differs.
  set(GBL_CORE_SOURCES src/version.c src/machine.c src/bus.c src/loader.c
    src/cpu.c src/inspect.c)
  function(gbl__add_core_target target expected_digest)
    add_library(${target} STATIC ${GBL_CORE_SOURCES})
    target_include_directories(${target} PUBLIC include)
    target_compile_definitions(${target} PRIVATE
      "GBL_VERSION_STRING=\"${PROJECT_VERSION}\"")
    target_compile_definitions(${target} PRIVATE
      "GBL_LOGO_EXPECTED_SHA256=\"${expected_digest}\"")
    target_compile_options(${target} PRIVATE
      "$<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-fno-stack-protector;-U_FORTIFY_SOURCE>")
    target_link_libraries(${target} PRIVATE gbl_warnings)
  endfunction()

  gbl__add_core_target(gamebloke_core "${GBL_LOGO_EXPECTED_SHA256}")
  gbl__add_core_target(gamebloke_core_logo_synthetic
    "${GBL_LOGO_SYNTHETIC_SHA256}")
  if(GBL_LOGO_EXPECTED_SHA256 STREQUAL "")
    set(GBL_PRODUCTION_LOGO_TEST_MODE 0)
  else()
    set(GBL_PRODUCTION_LOGO_TEST_MODE 1)
  endif()

  # Both runners compile the same producer and canonical core. Their only
  # distinction is the private immutable fixture-index include directory.
  set(GBL_RUNNER_SOURCES runner/main.c runner/run.c runner/result.c
    runner/sha256.c)
  set(GBL_GENERATED_ROOT "${CMAKE_BINARY_DIR}/generated")
  file(MAKE_DIRECTORY "${GBL_GENERATED_ROOT}")
  set(GBL_FIXTURE_DIR "${GBL_GENERATED_ROOT}/fixtures")
  set(GBL_PRODUCTION_INDEX_DIR "${GBL_GENERATED_ROOT}/production")
  set(GBL_CONTROL_INDEX_DIR "${GBL_GENERATED_ROOT}/control")
  set(GBL_PRODUCTION_INDEX "${GBL_PRODUCTION_INDEX_DIR}/fixture_index.h")
  set(GBL_CONTROL_INDEX "${GBL_CONTROL_INDEX_DIR}/fixture_index.h")
  set(GBL_CONTROL_MANIFEST "${GBL_CONTROL_INDEX_DIR}/manifest.json")

  add_executable(gbl_instruction_image tests/fixtures/instruction_image.c)
  target_link_libraries(gbl_instruction_image PRIVATE gbl_warnings)

  set(GBL_OWNED_FIXTURE_NAMES
    owned-arm-pass.gba
    owned-arm-pass-zero-case.gba
    owned-arm-fail.gba
    owned-arm-partial.gba
    owned-arm-loop.gba
    owned-arm-swi.gba
    owned-arm-undefined.gba
    owned-arm-coprocessor.gba
    owned-arm-thumb-bx-pass.gba
    owned-arm-pass-mutated.gba)
  set(GBL_OWNED_FIXTURE_FILES "")
  foreach(fixture IN LISTS GBL_OWNED_FIXTURE_NAMES)
    list(APPEND GBL_OWNED_FIXTURE_FILES "${GBL_FIXTURE_DIR}/${fixture}")
  endforeach()
  add_custom_command(
    OUTPUT ${GBL_OWNED_FIXTURE_FILES}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GBL_FIXTURE_DIR}"
    COMMAND "$<TARGET_FILE:gbl_instruction_image>" "${GBL_FIXTURE_DIR}"
    DEPENDS gbl_instruction_image
    COMMENT "Generating original owned instruction images under the build tree"
    VERBATIM)
  add_custom_target(gbl_owned_instruction_images
    DEPENDS ${GBL_OWNED_FIXTURE_FILES})

  set(GBL_DIAGNOSTIC_INDEX_DEPENDS "")
  if(GBL_LANE STREQUAL "diagnostics")
    set(GBL_DIAGNOSTIC_OUTPUT_ROOT
      "${CMAKE_SOURCE_DIR}/build/phase05-spike/${GBL_DIAGNOSTIC_OUTPUT_LEAF}")
    if(GBL_DIAGNOSTIC_STAGE STREQUAL "proof")
      set(GBL_DIAGNOSTIC_OUTPUT_ROOT
        "${CMAKE_SOURCE_DIR}/build/phase05-spike/final-smoke")
    endif()
    set(GBL_DIAGNOSTIC_RAW_ROMS
      "${GBL_DIAGNOSTIC_OUTPUT_ROOT}/roms/smoke.gba"
      "${GBL_DIAGNOSTIC_OUTPUT_ROOT}/roms/smoke-broken.gba")
    set(GBL_DIAGNOSTIC_SOURCE_DEPENDS
      "${CMAKE_SOURCE_DIR}/diagnostics/armv4t/startup.S"
      "${CMAKE_SOURCE_DIR}/diagnostics/armv4t/smoke.S"
      "${CMAKE_SOURCE_DIR}/diagnostics/armv4t/linker.ld"
      "${CMAKE_SOURCE_DIR}/diagnostics/armv4t/inputs.txt"
      "${CMAKE_SOURCE_DIR}/diagnostics/armv4t/NOTICE"
      "${CMAKE_SOURCE_DIR}/cmake/BuildArmDiagnostics.cmake"
      "${CMAKE_SOURCE_DIR}/cmake/FixGbaHeader.cmake"
      "${CMAKE_SOURCE_DIR}/scripts/fetch-toolchain.sh"
      "${CMAKE_SOURCE_DIR}/scripts/signature-scan.sh"
      "${CMAKE_SOURCE_DIR}/scripts/lib/signature-common.sh"
      "${CMAKE_SOURCE_DIR}/tests/signatures/allowlist.txt"
      "${CMAKE_SOURCE_DIR}/tests/signatures/bios.sha256"
      "${CMAKE_SOURCE_DIR}/tests/signatures/logo.sha256"
      "${CMAKE_SOURCE_DIR}/toolchain/arm-none-eabi.lock")
    if(EXISTS "${CMAKE_SOURCE_DIR}/diagnostics/armv4t/cases.json")
      list(APPEND GBL_DIAGNOSTIC_SOURCE_DEPENDS
        "${CMAKE_SOURCE_DIR}/diagnostics/armv4t/cases.json")
    endif()
    if(GBL_DIAGNOSTIC_STAGE STREQUAL "proof")
      file(STRINGS "${CMAKE_SOURCE_DIR}/diagnostics/armv4t/inputs.txt"
        GBL_FINAL_PRODUCER_INPUTS REGEX "^[A-Za-z0-9]")
      list(TRANSFORM GBL_FINAL_PRODUCER_INPUTS PREPEND "${CMAKE_SOURCE_DIR}/")
      set(GBL_FINAL_PROOF_ROOT "${CMAKE_SOURCE_DIR}/build/final-proof")
      if(NOT GBL_DIAGNOSTIC_GROUP STREQUAL "all")
        set(GBL_FINAL_PROOF_ROOT "${CMAKE_SOURCE_DIR}/build/final-proof-${GBL_DIAGNOSTIC_GROUP}")
      endif()
      set(GBL_FINAL_PROOF_RECEIPT "${GBL_FINAL_PROOF_ROOT}/complete-host.json")
      add_custom_command(
        OUTPUT "${GBL_FINAL_PROOF_RECEIPT}"
        COMMAND "${CMAKE_COMMAND}" -DCASE=final-host-build
          "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${GBL_FINAL_PROOF_ROOT}"
          "-DPROOF_GROUP=${GBL_DIAGNOSTIC_GROUP}"
          "-DTOOLCHAIN_ROOT=${GBL_TOOLCHAIN_ROOT_ABSOLUTE}"
          -P "${CMAKE_SOURCE_DIR}/tests/gates/toolchain_spike.cmake"
        DEPENDS ${GBL_FINAL_PRODUCER_INPUTS}
          "${CMAKE_SOURCE_DIR}/tests/gates/toolchain_spike.cmake"
          "${CMAKE_SOURCE_DIR}/tests/gates/final_diagnostic_proof.cmake"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Proving every final original diagnostic output before admission"
        VERBATIM)
      add_custom_target(gbl_diagnostics ALL DEPENDS "${GBL_FINAL_PROOF_RECEIPT}")
    else()
      file(STRINGS "${CMAKE_SOURCE_DIR}/diagnostics/armv4t/inputs.txt"
        GBL_ADMITTED_PRODUCER_INPUTS REGEX "^[A-Za-z0-9]")
      list(TRANSFORM GBL_ADMITTED_PRODUCER_INPUTS PREPEND "${CMAKE_SOURCE_DIR}/")
      set(GBL_DIAGNOSTIC_PREPARED
        "${CMAKE_BINARY_DIR}/generated/diagnostics/prepared-${GBL_DIAGNOSTIC_GROUP}.stamp")
      add_custom_command(
      OUTPUT "${GBL_DIAGNOSTIC_PREPARED}"
      COMMAND "${CMAKE_COMMAND}"
        -DCASE=all-admitted -DPREPARE_ONLY=1
        "-DSRC=${CMAKE_SOURCE_DIR}"
        "-DBIN=${CMAKE_BINARY_DIR}"
        "-DGROUP=${GBL_DIAGNOSTIC_GROUP}"
        "-DTOOLCHAIN_ROOT=${GBL_TOOLCHAIN_ROOT_ABSOLUTE}"
        -P "${CMAKE_SOURCE_DIR}/tests/gates/diagnostics_case.cmake"
      COMMAND "${CMAKE_COMMAND}" -E touch "${GBL_DIAGNOSTIC_PREPARED}"
      DEPENDS ${GBL_ADMITTED_PRODUCER_INPUTS}
        "${CMAKE_SOURCE_DIR}/tests/fixtures/manifest.json"
        "${CMAKE_SOURCE_DIR}/tests/gates/diagnostic_admission.cmake"
        "${CMAKE_SOURCE_DIR}/tests/gates/diagnostics_case.cmake"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "Building and placing the exact complete admitted diagnostic group"
      VERBATIM)
    add_custom_target(gbl_diagnostics ALL DEPENDS "${GBL_DIAGNOSTIC_PREPARED}")
    endif()

    if(GBL_DIAGNOSTIC_STAGE STREQUAL "admitted")
      list(APPEND GBL_DIAGNOSTIC_INDEX_DEPENDS "${GBL_DIAGNOSTIC_PREPARED}")
    endif()
  endif()

  add_custom_command(
    OUTPUT "${GBL_PRODUCTION_INDEX}" "${GBL_CONTROL_INDEX}"
      "${GBL_CONTROL_MANIFEST}"
    COMMAND "${CMAKE_COMMAND}"
      "-DGBL_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
      "-DGBL_PROOF_METADATA=$<IF:$<STREQUAL:${GBL_DIAGNOSTIC_STAGE},proof>,ON,OFF>"
      "-DGBL_OUTPUT_ROOT=${CMAKE_BINARY_DIR}"
      "-DGBL_ARTIFACT_ROOT=${CMAKE_BINARY_DIR}"
      "-DGBL_CONTROL_IMAGE=${GBL_FIXTURE_DIR}/owned-arm-pass.gba"
      "-DGBL_CONTROL_FAIL_IMAGE=${GBL_FIXTURE_DIR}/owned-arm-fail.gba"
      "-DGBL_CONTROL_SKIP_IMAGE=${GBL_FIXTURE_DIR}/owned-arm-pass.gba"
      "-DGBL_CONTROL_ZERO_CASE_IMAGE=${GBL_FIXTURE_DIR}/owned-arm-pass-zero-case.gba"
      "-DGBL_CONTROL_MANIFEST_OUTPUT=${GBL_CONTROL_MANIFEST}"
      "-DGBL_MANIFEST=${CMAKE_SOURCE_DIR}/tests/fixtures/manifest.json"
      "-DGBL_REGISTRY=${CMAKE_SOURCE_DIR}/registry/disputes.json"
      "-DGBL_VOCAB=${CMAKE_SOURCE_DIR}/contracts/evidence.json"
      "-DGBL_OUT_PRODUCTION=${GBL_PRODUCTION_INDEX}"
      "-DGBL_OUT_CONTROL=${GBL_CONTROL_INDEX}"
      -P "${CMAKE_SOURCE_DIR}/cmake/GenerateFixtureIndex.cmake"
    DEPENDS ${GBL_OWNED_FIXTURE_FILES} ${GBL_DIAGNOSTIC_INDEX_DEPENDS}
      "${CMAKE_SOURCE_DIR}/tests/fixtures/manifest.json"
      "${CMAKE_SOURCE_DIR}/cmake/CheckDiagnosticAuthority.cmake"
      "${CMAKE_SOURCE_DIR}/cmake/PathContainment.cmake"
      "${CMAKE_SOURCE_DIR}/registry/disputes.json"
      "${CMAKE_SOURCE_DIR}/contracts/evidence.json"
      "${CMAKE_SOURCE_DIR}/cmake/GenerateFixtureIndex.cmake"
      "${CMAKE_SOURCE_DIR}/cmake/CheckManifest.cmake"
      "${CMAKE_SOURCE_DIR}/cmake/CheckRegistry.cmake"
      "${CMAKE_SOURCE_DIR}/cmake/CheckContract.cmake"
      "${CMAKE_SOURCE_DIR}/cmake/GblJson.cmake"
    COMMENT "Validating fixture manifests and generating immutable build indexes"
    VERBATIM)
  add_custom_target(gbl_fixture_indexes
    DEPENDS "${GBL_PRODUCTION_INDEX}" "${GBL_CONTROL_INDEX}"
      "${GBL_CONTROL_MANIFEST}")

  add_executable(gamebloke-run ${GBL_RUNNER_SOURCES})
  target_include_directories(gamebloke-run PRIVATE "${GBL_PRODUCTION_INDEX_DIR}")
  target_link_libraries(gamebloke-run PRIVATE gamebloke_core gbl_warnings)
  add_dependencies(gamebloke-run gbl_fixture_indexes)
  install(TARGETS gamebloke-run RUNTIME DESTINATION bin)

  add_executable(gamebloke-run-control ${GBL_RUNNER_SOURCES})
  target_include_directories(gamebloke-run-control PRIVATE "${GBL_CONTROL_INDEX_DIR}")
  target_link_libraries(gamebloke-run-control PRIVATE gamebloke_core gbl_warnings)
  add_dependencies(gamebloke-run-control gbl_fixture_indexes)

  # Exercise the actual host runner modules against the build-only synthetic
  # index. The production CLI remains bound to the canonical empty index.
  add_executable(runner_test tests/runner_test.c runner/run.c runner/result.c
    runner/sha256.c)
  target_include_directories(runner_test PRIVATE runner
    "${GBL_CONTROL_INDEX_DIR}")
  target_compile_definitions(runner_test PRIVATE
    "GBL_RUNNER_TEST_FIXTURE_DIR=\"${GBL_FIXTURE_DIR}\""
    "GBL_RUNNER_TEST_SCRATCH_DIR=\"${CMAKE_BINARY_DIR}/runner-test\""
    "GBL_RUNNER_TEST_CLI=\"$<TARGET_FILE:gamebloke-run-control>\"")
  target_link_libraries(runner_test PRIVATE gamebloke_core gbl_warnings)
  add_dependencies(runner_test gbl_fixture_indexes)

  # Store actual target properties for the hermetic metadata gate. Target
  # properties also make accidental detached producer wiring visible in tests.
  function(gbl__runner_property out target property)
    get_target_property(value "${target}" "${property}")
    if(NOT value OR "${value}" MATCHES "-NOTFOUND$")
      set(value "")
    endif()
    list(JOIN value ";" joined)
    set(${out} "${joined}" PARENT_SCOPE)
  endfunction()
  file(WRITE "${GBL_GENERATED_ROOT}/runner-target-wiring.txt" "")
  foreach(target IN ITEMS gamebloke-run gamebloke-run-control)
    if(target STREQUAL "gamebloke-run")
      set(prefix production)
    else()
      set(prefix control)
    endif()
    foreach(property IN ITEMS SOURCES COMPILE_OPTIONS COMPILE_DEFINITIONS
        LINK_OPTIONS LINK_LIBRARIES INCLUDE_DIRECTORIES)
      gbl__runner_property(value "${target}" "${property}")
      file(APPEND "${GBL_GENERATED_ROOT}/runner-target-wiring.txt"
        "${prefix}.${property}=${value}\n")
    endforeach()
  endforeach()
  file(APPEND "${GBL_GENERATED_ROOT}/runner-target-wiring.txt"
    "common.c_standard=${CMAKE_C_STANDARD}\n"
    "common.c_flags=${CMAKE_C_FLAGS}\n"
    "common.exe_linker_flags=${CMAKE_EXE_LINKER_FLAGS}\n")
endif()

# ---- Test registration (lane includes) ------------------------------------
# Every test registers from THIS directory scope via include(), never
# add_subdirectory(): the TESTS directory property is directory-scoped at the
# CMake 3.25 floor (Pitfall 4).
# Extension points:
#   tests/core.cmake    core and runner tests
#   tests/harness.cmake inventory controls (ci lane only)
#   tests/sanitizer.cmake UBSan and ASan controls (asan lane only)
#   tests/ci_gates.cmake CI routing gates; ci and hygiene lanes, each test guarded by lane
#   tests/gates.cmake   gate tests (hygiene and signature, plus `manifest.lint`, which
#                      completes VERIF-01's manifest clause, EVID-02, and the Phase 2
#                      validator gates and controls in its evidence section); ci and
#                      hygiene lanes
if(GBL_LANE STREQUAL "ci" OR GBL_LANE STREQUAL "asan")
  include(tests/core.cmake)
endif()
if(GBL_LANE STREQUAL "ci")
  include(tests/harness.cmake)
endif()
if(GBL_LANE STREQUAL "asan")
  include(tests/sanitizer.cmake)
endif()
if(GBL_LANE STREQUAL "ci" OR GBL_LANE STREQUAL "hygiene")
  include(tests/gates.cmake)
  include(tests/ci_gates.cmake)
endif()
if(GBL_LANE STREQUAL "diagnostics")
  if(GBL_DIAGNOSTIC_STAGE STREQUAL "admitted")
    add_dependencies(gbl_fixture_indexes gbl_diagnostics)
  endif()
  include(tests/toolchain.cmake)
endif()
if(GBL_LANE MATCHES "^(ci|asan|diagnostics)$")
  include(tests/diagnostics.cmake)
endif()
