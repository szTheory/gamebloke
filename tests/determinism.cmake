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

gbl_add_test(
  NAME determinism.repeat.in_process
  LABELS determinism gate integration
  COMMAND $<TARGET_FILE:determinism_test>)
gbl_add_test(
  NAME determinism.interleave
  LABELS determinism gate integration
  COMMAND $<TARGET_FILE:determinism_test> interleave)
gbl_add_test(
  NAME determinism.interleave.controls
  LABELS determinism gate integration
  COMMAND $<TARGET_FILE:determinism_test> interleave-controls)

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
