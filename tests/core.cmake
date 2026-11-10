# Core and runner tests. Included from the top-level CMakeLists.txt (never
# add_subdirectory(): see Pitfall 4 note there).
gbl_add_test(
  NAME runner.version
  LABELS runner unit
  COMMAND $<TARGET_FILE:gamebloke-run> --version
  PASS_REGULAR_EXPRESSION "^gamebloke-run 0\\.0\\.0")

add_executable(version_test tests/version_test.c)
target_link_libraries(version_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME core.version
  LABELS core unit
  COMMAND $<TARGET_FILE:version_test>)

gbl_add_test(
  NAME runner.help
  LABELS runner unit
  COMMAND $<TARGET_FILE:gamebloke-run> --help
  PASS_REGULAR_EXPRESSION "exit codes: 0 success, 2 usage error")

# The documented usage-error exit code is exactly 2 (unlike a sanitizer abort).
gbl_add_test(
  NAME runner.usage_error
  LABELS runner unit
  COMMAND "${CMAKE_COMMAND}" "-DCMD=$<TARGET_FILE:gamebloke-run>" -DARGS=--bogus
    -DEXIT=2 "-DREGEX=usage: gamebloke-run"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")
gbl_add_test(
  NAME runner.no_args
  LABELS runner unit
  COMMAND "${CMAKE_COMMAND}" "-DCMD=$<TARGET_FILE:gamebloke-run>"
    -DEXIT=2 "-DREGEX=usage: gamebloke-run"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")

# The core uses no host includes (D-05), with a control that plants one.
gbl_add_test(
  NAME core.no_host_includes
  LABELS core gate
  COMMAND "${CMAKE_COMMAND}"
    "-DDIRS=${CMAKE_SOURCE_DIR}/src|${CMAKE_SOURCE_DIR}/include"
    -P "${CMAKE_SOURCE_DIR}/cmake/CheckCoreIncludes.cmake")
gbl_add_test(
  NAME core.nc.host_include
  LABELS control core
  COMMAND "${CMAKE_COMMAND}" "-DPLANT=${CMAKE_BINARY_DIR}/nc/host_include"
    -P "${CMAKE_SOURCE_DIR}/cmake/CheckCoreIncludes.cmake")
