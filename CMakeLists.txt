cmake_minimum_required(VERSION 3.25)
project(gamebloke VERSION 0.0.0 LANGUAGES C)

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
  # Host-independent machine core: no host includes, only gbl_version().
  add_library(gamebloke_core STATIC src/version.c)
  target_include_directories(gamebloke_core PUBLIC include)
  target_compile_definitions(gamebloke_core PRIVATE "GBL_VERSION_STRING=\"${PROJECT_VERSION}\"")
  target_compile_options(gamebloke_core PRIVATE
    "$<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-fno-stack-protector;-U_FORTIFY_SOURCE>")
  target_link_libraries(gamebloke_core PRIVATE gbl_warnings)

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
#   tests/gates.cmake   gate tests (hygiene and signature in this phase, and the
#                      Phase 2 `manifest.lint` test that completes VERIF-01's
#                      manifest clause, EVID-02); created by plan 03 (ci and hygiene lanes)
if(GBL_LANE STREQUAL "ci" OR GBL_LANE STREQUAL "asan")
  include(tests/core.cmake)
endif()
if(GBL_LANE STREQUAL "ci")
  include(tests/harness.cmake)
endif()
if(GBL_LANE STREQUAL "ci" OR GBL_LANE STREQUAL "hygiene")
  include(tests/gates.cmake)
endif()
