# Core and runner tests. Included from the top-level CMakeLists.txt (never
# add_subdirectory(): see Pitfall 4 note there).
# --version and --help document exit status 0, so these require BOTH the exit
# status and the text (EXPECT=pass). PASS_REGULAR_EXPRESSION alone would make
# CTest ignore the exit status.
gbl_add_test(
  NAME runner.version
  LABELS runner unit
  COMMAND "${CMAKE_COMMAND}" -DEXPECT=pass "-DCMD=$<TARGET_FILE:gamebloke-run>"
    -DARGS=--version "-DREGEX=^gamebloke-run 0\\.0\\.0"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")

add_executable(version_test tests/version_test.c)
target_link_libraries(version_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME core.version
  LABELS core unit
  COMMAND $<TARGET_FILE:version_test>)

add_executable(bus_test tests/bus_test.c)
target_link_libraries(bus_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME bus.decode
  LABELS bus core unit
  COMMAND $<TARGET_FILE:bus_test> decode)
gbl_add_test(
  NAME bus.timing
  LABELS bus core unit
  COMMAND $<TARGET_FILE:bus_test> timing)
gbl_add_test(
  NAME bus.video
  LABELS bus core unit
  COMMAND $<TARGET_FILE:bus_test> video)
gbl_add_test(
  NAME machine.state
  LABELS core machine unit
  COMMAND $<TARGET_FILE:bus_test> state)

add_executable(loader_test tests/loader_test.c)
target_link_libraries(loader_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME loader.lengths
  LABELS core loader unit
  COMMAND $<TARGET_FILE:loader_test> lengths)
gbl_add_test(
  NAME loader.transaction
  LABELS core loader unit
  COMMAND $<TARGET_FILE:loader_test> transaction)

gbl_add_test(
  NAME runner.help
  LABELS runner unit
  COMMAND "${CMAKE_COMMAND}" -DEXPECT=pass "-DCMD=$<TARGET_FILE:gamebloke-run>"
    -DARGS=--help "-DREGEX=exit codes: 0 success, 2 usage error"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")

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

# The core uses no host includes (D-05), with a control that plants one. Further
# controls plant each bypass shape (a quoted system header, a line continuation,
# a comment between the directive and the header, a computed include) and the
# two ways to pass by scanning nothing (an empty and a missing directory); the
# positive companion is an in-tree quoted include plus freestanding headers.
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
foreach(shape IN ITEMS quoted continuation comment computed empty missing)
  gbl_add_test(
    NAME core.nc.${shape}
    LABELS control core
    COMMAND "${CMAKE_COMMAND}" "-DPLANT=${CMAKE_BINARY_DIR}/nc/core_${shape}" "-DSHAPE=${shape}"
      -P "${CMAKE_SOURCE_DIR}/cmake/CheckCoreIncludes.cmake")
endforeach()
gbl_add_test(
  NAME core.pos.in_tree
  LABELS core positive
  COMMAND "${CMAKE_COMMAND}" "-DPLANT=${CMAKE_BINARY_DIR}/nc/core_in_tree" -DSHAPE=in_tree
    -P "${CMAKE_SOURCE_DIR}/cmake/CheckCoreIncludes.cmake")
