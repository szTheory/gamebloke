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
set(GBL_LANE "ci" CACHE STRING "Verification lane: ci | asan | hygiene")
set_property(CACHE GBL_LANE PROPERTY STRINGS ci asan hygiene)
if(NOT GBL_LANE MATCHES "^(ci|asan|hygiene)$")
  message(FATAL_ERROR "gamebloke: unknown-lane: '${GBL_LANE}' (expected ci, asan, or hygiene)")
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

if(GBL_LANE STREQUAL "ci" OR GBL_LANE STREQUAL "asan")
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
    src/cpu.c)
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

  add_executable(gamebloke-run runner/main.c)
  target_link_libraries(gamebloke-run PRIVATE gamebloke_core gbl_warnings)
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
