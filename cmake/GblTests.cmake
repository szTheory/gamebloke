include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/InventoryCompare.cmake")

set_property(GLOBAL PROPERTY GBL_REGISTERED_TESTS "")

# gbl_add_test(NAME <name> LABELS <label>... COMMAND <cmd>... [SKIPPABLE]
#              [WORKING_DIRECTORY <dir>] [PASS_REGULAR_EXPRESSION <re>])
#
# Registers a CTest test and records "<name> <labels>[ skip-ok]" (labels sorted
# and comma-joined) for the configure-time inventory compare. Call it only at
# the top-level directory scope (via include()), never from add_subdirectory().
function(gbl_add_test)
  cmake_parse_arguments(PARSE_ARGV 0 T "SKIPPABLE"
    "NAME;WORKING_DIRECTORY;PASS_REGULAR_EXPRESSION" "LABELS;COMMAND")

  # Names stay free of XML metacharacters so the junit check needs no unescaping.
  if(NOT T_NAME MATCHES "^[a-z0-9][a-z0-9._-]*$")
    message(FATAL_ERROR "gbl_add_test: bad-name: ${T_NAME}")
  endif()
  if(NOT T_LABELS)
    message(FATAL_ERROR "gbl_add_test: no-labels: ${T_NAME}")
  endif()

  add_test(NAME "${T_NAME}" COMMAND ${T_COMMAND})

  # LABELS accepts separate arguments or comma-joined ones.
  set(labels "")
  foreach(label IN LISTS T_LABELS)
    string(REPLACE "," ";" label "${label}")
    list(APPEND labels ${label})
  endforeach()
  list(SORT labels)
  list(REMOVE_DUPLICATES labels)

  set_tests_properties("${T_NAME}" PROPERTIES LABELS "${labels}")
  if(T_WORKING_DIRECTORY)
    set_tests_properties("${T_NAME}" PROPERTIES WORKING_DIRECTORY "${T_WORKING_DIRECTORY}")
  endif()
  if(T_PASS_REGULAR_EXPRESSION)
    set_tests_properties("${T_NAME}" PROPERTIES PASS_REGULAR_EXPRESSION "${T_PASS_REGULAR_EXPRESSION}")
  endif()
  if(T_SKIPPABLE)
    # A skipping test prints "SKIPPED: <reason>"; CMake -P scripts cannot set
    # exit 77 at the 3.25 floor.
    set_tests_properties("${T_NAME}" PROPERTIES SKIP_REGULAR_EXPRESSION "SKIPPED: ")
  endif()

  list(JOIN labels "," joined)
  set(line "${T_NAME} ${joined}")
  if(T_SKIPPABLE)
    string(APPEND line " skip-ok")
  endif()
  set_property(GLOBAL APPEND PROPERTY GBL_REGISTERED_TESTS "${line}")
endfunction()

# Runs after all registration, once the top-level directory finishes.
function(gbl__check_inventory)
  get_property(registered GLOBAL PROPERTY GBL_REGISTERED_TESTS)
  gbl_inventory_compare(registered
    "${CMAKE_SOURCE_DIR}/tests/inventory/${GBL_LANE}.txt")
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gbl__check_inventory)

# The post step: reads the junit that the main ctest step wrote and checks it
# against the inventory. Label `post` keeps it out of the main step (the test
# preset excludes it) and out of its own comparison.
gbl_add_test(
  NAME inventory.junit
  LABELS post
  COMMAND sh "${CMAKE_SOURCE_DIR}/scripts/check-junit.sh"
    "${CMAKE_BINARY_DIR}/junit.xml"
    "${CMAKE_SOURCE_DIR}/tests/inventory/${GBL_LANE}.txt" "^post$")
