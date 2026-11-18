# Determinism tests use the same production runner modules and generated
# synthetic-control authority as runner_test.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/determinism-test")
add_executable(determinism_test tests/determinism_test.c runner/run.c
  runner/result.c runner/sha256.c)
target_include_directories(determinism_test PRIVATE runner
  "${GBL_CONTROL_INDEX_DIR}")
target_compile_definitions(determinism_test PRIVATE
  "GBL_DETERMINISM_FIXTURE_DIR=\"${GBL_FIXTURE_DIR}\""
  "GBL_DETERMINISM_SCRATCH_DIR=\"${CMAKE_BINARY_DIR}/determinism-test\"")
target_link_libraries(determinism_test PRIVATE gamebloke_core gbl_warnings)
add_dependencies(determinism_test gbl_fixture_indexes)

if(GBL_LANE STREQUAL "asan")
  # Keep the command-dispatch tests in separate frames. At -O1, the ASan ARM64
  # build inlines enough of this large test translation unit into main to
  # exceed the runner's default stack before any test starts.
  target_compile_options(determinism_test PRIVATE -fno-inline-functions)
endif()

if(GBL_LANE STREQUAL "ci" OR GBL_LANE STREQUAL "asan")
  gbl_add_test(
    NAME determinism.interleave.controls
    LABELS determinism gate integration
    COMMAND $<TARGET_FILE:determinism_test> interleave-controls)
  gbl_add_test(
    NAME determinism.phase8.controls
    LABELS determinism gate integration
    COMMAND $<TARGET_FILE:determinism_test> phase8-controls)
  gbl_add_test(
    NAME determinism.phase8
    LABELS determinism gate integration
    COMMAND $<TARGET_FILE:determinism_test> phase8)
  gbl_add_test(
    NAME determinism.phase9.controls
    LABELS determinism gate integration
    COMMAND $<TARGET_FILE:determinism_test> phase9-controls)
  gbl_add_test(
    NAME determinism.phase9
    LABELS determinism gate integration
    COMMAND $<TARGET_FILE:determinism_test> phase9)

  foreach(case IN ITEMS repeat-process controls)
    if(case STREQUAL "controls")
      set(test_name determinism.repeat.controls)
    else()
      set(test_name determinism.repeat.process)
    endif()
    gbl_add_test(
      NAME "${test_name}"
      LABELS determinism gate integration
      COMMAND "${CMAKE_COMMAND}"
        "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
        "-DCASE=${case}"
        -P "${CMAKE_SOURCE_DIR}/tests/gates/determinism_case.cmake")
  endforeach()
elseif(GBL_LANE STREQUAL "local-timer0")
  # These checks execute the generated Timer 0 polling guest, which the fixture
  # notice restricts to the explicit local-timer0 lane.
  foreach(case IN ITEMS clean control)
    gbl_add_test(
      NAME "bus.time_authority.${case}"
      LABELS bus determinism gate integration
      COMMAND "${CMAKE_COMMAND}"
        "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
        "-DCASE=${case}" "-DCANDIDATE=$<TARGET_FILE:determinism_test>"
        -P "${CMAKE_SOURCE_DIR}/tests/gates/time_authority.cmake")
  endforeach()
  set_tests_properties(bus.time_authority.clean bus.time_authority.control
    PROPERTIES RUN_SERIAL TRUE)
  gbl_add_test(
    NAME determinism.repeat.in_process
    LABELS determinism gate integration
    COMMAND $<TARGET_FILE:determinism_test>)
  gbl_add_test(
    NAME determinism.interleave
    LABELS determinism gate integration
    COMMAND $<TARGET_FILE:determinism_test> interleave)
elseif(GBL_LANE STREQUAL "diagnostics")
  gbl_add_test(
    NAME diagnostics.determinism
    LABELS diagnostics gate determinism
    COMMAND "${CMAKE_COMMAND}"
      "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      "-DMODE=${GBL_DETERMINISM_MODE}"
      "-DGROUP=${GBL_DIAGNOSTIC_GROUP}"
      -P "${CMAKE_SOURCE_DIR}/cmake/RunDeterminismMatrix.cmake")
  set_tests_properties(diagnostics.determinism PROPERTIES
    FIXTURES_REQUIRED diagnostic_execution RUN_SERIAL TRUE)
endif()
