# Sanitizer controls (asan lane only). Included from the top-level
# CMakeLists.txt when GBL_LANE is asan. Each control is built with the same
# flags as the code it proves, and passes only when the program exits non-zero
# AND prints the sanitizer's reason text. No exact exit code is asserted: macOS
# aborts with 134 and Linux exits 1.
add_executable(ub_control tests/ub_control.c)
target_link_libraries(ub_control PRIVATE gbl_warnings)
add_executable(asan_control tests/asan_control.c)
target_link_libraries(asan_control PRIVATE gbl_warnings)

gbl_add_test(
  NAME sanitizer.ubsan.shift32
  LABELS sanitizer control
  COMMAND "${CMAKE_COMMAND}" "-DCMD=$<TARGET_FILE:ub_control>"
    "-DREGEX=runtime error: shift exponent 32 is too large for 32-bit type"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")

gbl_add_test(
  NAME sanitizer.asan.heap_overflow
  LABELS sanitizer control
  COMMAND "${CMAKE_COMMAND}" "-DCMD=$<TARGET_FILE:asan_control>"
    "-DREGEX=AddressSanitizer: heap-buffer-overflow"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")
