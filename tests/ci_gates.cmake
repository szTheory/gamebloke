# CI routing gates (VERIF-05). Included from the top-level CMakeLists.txt for
# BOTH the `ci` and `hygiene` lanes, so lane membership has one rule:
#
#   Every registration below is wrapped in if(GBL_LANE STREQUAL "ci") unless a
#   comment marks it hygiene-safe (no build targets, cheap, meaningful on a
#   .planning/-only pull request) AND it is also listed in
#   tests/inventory/hygiene.txt.
#
# The hygiene lane builds nothing, so any test that needs a build tree or a
# compiler stays in the ci guard.
# hygiene-safe: cheap, no build targets, and meaningful on a .planning/-only
# pull request. Every action in the workflow is pinned to a commit SHA with a
# version comment (D-10); runs in both lanes and is listed in both inventories.
gbl_add_test(
  NAME ci.actions.pinned
  LABELS ci gate
  COMMAND sh scripts/check-action-pins.sh .github/workflows/ci.yml
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

if(GBL_LANE STREQUAL "ci")
  # Classifier case table (D-11): heavy=false only for a complete, .planning/-only listing.
  gbl_add_test(
    NAME ci.classify.cases
    LABELS ci unit
    COMMAND sh tests/ci/classify_cases.sh
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  # Aggregator predicate (D-11, Pitfall 6): a heavy job skipped while HEAVY is
  # not exactly false, or any failed or cancelled lane, must fail `CI required`.
  gbl_add_test(
    NAME ci.required.cases
    LABELS ci unit
    COMMAND "${CMAKE_COMMAND}" -DCASE=needs "-DSRC=${CMAKE_SOURCE_DIR}"
      -P "${CMAKE_SOURCE_DIR}/tests/ci/ci_cases.cmake")

  # Pull request title rule (D-14): Conventional Commits, at most 100 characters.
  gbl_add_test(
    NAME ci.title.cases
    LABELS ci unit
    COMMAND sh tests/ci/title_cases.sh
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

  # The compiler-identity guard must stay live: a wrong GBL_EXPECT_C_ID fails
  # configure, so the Linux GCC leg is proven rather than assumed.
  gbl_add_test(
    NAME build.nc.compiler_id_guard
    LABELS build control
    COMMAND "${CMAKE_COMMAND}" -DCASE=compiler_id_guard "-DSRC=${CMAKE_SOURCE_DIR}"
      "-DBIN=${CMAKE_BINARY_DIR}"
      -P "${CMAKE_SOURCE_DIR}/tests/ci/ci_cases.cmake")

  # Pin-gate controls: each mutates a copy of the real ci.yml (the case driver
  # stops with `mutate: no-match` when its anchor is gone), so a gate that
  # stopped checking pins fails here.
  gbl_add_test(
    NAME ci.actions.nc.unpinned
    LABELS ci control
    COMMAND "${CMAKE_COMMAND}" -DCASE=pins_unpinned "-DSRC=${CMAKE_SOURCE_DIR}"
      "-DBIN=${CMAKE_BINARY_DIR}"
      -P "${CMAKE_SOURCE_DIR}/tests/ci/workflow_cases.cmake")
  gbl_add_test(
    NAME ci.actions.nc.no_version_comment
    LABELS ci control
    COMMAND "${CMAKE_COMMAND}" -DCASE=pins_no_version_comment "-DSRC=${CMAKE_SOURCE_DIR}"
      "-DBIN=${CMAKE_BINARY_DIR}"
      -P "${CMAKE_SOURCE_DIR}/tests/ci/workflow_cases.cmake")
endif()
