# Core and runner tests. Included from the top-level CMakeLists.txt (never
# add_subdirectory(): see Pitfall 4 note there).
gbl_add_test(
  NAME runner.version
  LABELS runner unit
  COMMAND $<TARGET_FILE:gamebloke-run> --version
  PASS_REGULAR_EXPRESSION "^gamebloke-run 0\\.0\\.0")
