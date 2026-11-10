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
if(GBL_LANE STREQUAL "ci")
  # Classifier case table (D-11): heavy=false only for a complete, .planning/-only listing.
  gbl_add_test(
    NAME ci.classify.cases
    LABELS ci unit
    COMMAND sh tests/ci/classify_cases.sh
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
