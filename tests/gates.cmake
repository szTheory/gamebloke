# Gate tests (ci and hygiene lanes). Included from the top-level CMakeLists.txt.
# Each gate has a negative control that must fail for its stated reason, and a
# positive control on otherwise identical clean input (D-03).
set(GBL_HYGIENE_DRIVER
  "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}")
set(GBL_HYGIENE_CASE "${CMAKE_SOURCE_DIR}/tests/gates/hygiene_case.cmake")

# ---- hygiene (VERIF-07) ---------------------------------------------------
# The real tree must be clean. Runs from the source root because --tree lists
# files with git.
gbl_add_test(
  NAME hygiene.tree
  LABELS gate hygiene
  COMMAND sh scripts/hygiene.sh --tree
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

foreach(case IN ITEMS
    credential private_key email home_path home_path_encoded newline_name)
  gbl_add_test(
    NAME hygiene.nc.${case}
    LABELS control hygiene
    COMMAND ${GBL_HYGIENE_DRIVER} "-DCASE=${case}" -P "${GBL_HYGIENE_CASE}")
endforeach()

gbl_add_test(
  NAME hygiene.pos.clean
  LABELS hygiene positive
  COMMAND ${GBL_HYGIENE_DRIVER} -DCASE=clean -P "${GBL_HYGIENE_CASE}")

gbl_add_test(
  NAME hygiene.usage
  LABELS hygiene unit
  COMMAND ${GBL_HYGIENE_DRIVER} -DCASE=usage -P "${GBL_HYGIENE_CASE}")
