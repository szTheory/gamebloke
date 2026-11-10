# Inventory controls (ci lane). Included from the top-level CMakeLists.txt.
# Each case is run by tests/harness_case.cmake, which synthesizes its junit
# shape under the build tree at test time and asserts the gate's reason code.
set(GBL_HARNESS_DRIVER
  "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}")

foreach(case IN ITEMS
    missing extra duplicate failed skipped_not_allowed skipped_no_reason
    disabled empty_junit no_junit configure_mismatch unsorted
    add_test_bad_name add_test_no_labels label_substring)
  gbl_add_test(
    NAME inventory.nc.${case}
    LABELS control inventory
    COMMAND ${GBL_HARNESS_DRIVER} "-DCASE=${case}"
      -P "${CMAKE_SOURCE_DIR}/tests/harness_case.cmake")
endforeach()

# Positive companions: the same gate on clean input passes, and a permitted
# skip is reported with its reason.
gbl_add_test(
  NAME inventory.pos.pass
  LABELS inventory positive
  COMMAND ${GBL_HARNESS_DRIVER} -DCASE=pass
    -P "${CMAKE_SOURCE_DIR}/tests/harness_case.cmake")
gbl_add_test(
  NAME inventory.pos.skip_reported
  LABELS inventory positive
  COMMAND ${GBL_HARNESS_DRIVER} -DCASE=skipped_allowed
    -P "${CMAKE_SOURCE_DIR}/tests/harness_case.cmake")

# Meta-control: the negative-control wrapper fails when its gate passes.
gbl_add_test(
  NAME nc.meta.passing_gate_fails_wrapper
  LABELS control meta
  COMMAND ${GBL_HARNESS_DRIVER} -DCASE=meta
    -P "${CMAKE_SOURCE_DIR}/tests/harness_case.cmake")

# Meta-control: the pass wrapper that the runner tests use requires exit status 0
# as well as the output text.
gbl_add_test(
  NAME nc.meta.pass_requires_exit_status
  LABELS control meta
  COMMAND ${GBL_HARNESS_DRIVER} -DCASE=meta_pass_exit
    -P "${CMAKE_SOURCE_DIR}/tests/harness_case.cmake")
