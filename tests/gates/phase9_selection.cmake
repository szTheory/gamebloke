cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase9-selection: missing-${required}")
  endif()
endforeach()
if(NOT CASE STREQUAL "verify")
  message(FATAL_ERROR "phase9-selection: invalid-case")
endif()

get_filename_component(GBL_PHASE9_DEFAULT_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
get_filename_component(GBL_PHASE9_SRC "${SRC}" REALPATH
  BASE_DIR "${GBL_PHASE9_DEFAULT_ROOT}")
get_filename_component(GBL_PHASE9_BIN "${BIN}" REALPATH
  BASE_DIR "${CMAKE_BINARY_DIR}")
if(NOT IS_DIRECTORY "${GBL_PHASE9_SRC}" OR NOT IS_DIRECTORY "${GBL_PHASE9_BIN}")
  message(FATAL_ERROR "phase9-selection: source-or-build-directory-missing")
endif()
set(GBL_PHASE9_CACHE "${GBL_PHASE9_BIN}/CMakeCache.txt")
if(NOT EXISTS "${GBL_PHASE9_CACHE}")
  message(FATAL_ERROR "phase9-selection: build-cache-missing")
endif()
file(STRINGS "${GBL_PHASE9_CACHE}" GBL_PHASE9_CACHE_HOME
  REGEX "^CMAKE_HOME_DIRECTORY:INTERNAL=")
list(LENGTH GBL_PHASE9_CACHE_HOME GBL_PHASE9_CACHE_HOME_COUNT)
if(NOT GBL_PHASE9_CACHE_HOME_COUNT EQUAL 1)
  message(FATAL_ERROR "phase9-selection: build-source-missing")
endif()
string(REGEX REPLACE "^CMAKE_HOME_DIRECTORY:INTERNAL=" ""
  GBL_PHASE9_CACHE_SOURCE "${GBL_PHASE9_CACHE_HOME}")
file(REAL_PATH "${GBL_PHASE9_CACHE_SOURCE}" GBL_PHASE9_CACHE_SOURCE)
if(NOT GBL_PHASE9_CACHE_SOURCE STREQUAL GBL_PHASE9_SRC)
  message(FATAL_ERROR "phase9-selection: source-build-mismatch")
endif()
set(GBL_PHASE9_EXPECTED_GATE
  "${GBL_PHASE9_SRC}/tests/gates/phase9_selection.cmake")
file(REAL_PATH "${GBL_PHASE9_EXPECTED_GATE}" GBL_PHASE9_EXPECTED_GATE)
file(REAL_PATH "${CMAKE_CURRENT_LIST_FILE}" GBL_PHASE9_GATE)
if(NOT GBL_PHASE9_GATE STREQUAL GBL_PHASE9_EXPECTED_GATE)
  message(FATAL_ERROR "phase9-selection: script-source-mismatch")
endif()

find_program(CTEST_PROGRAM NAMES ctest REQUIRED)
execute_process(
  COMMAND "${CTEST_PROGRAM}" --test-dir "${GBL_PHASE9_BIN}" --show-only=json-v1
  RESULT_VARIABLE ctest_result
  OUTPUT_VARIABLE ctest_json
  ERROR_VARIABLE ctest_error)
if(NOT ctest_result STREQUAL "0")
  message(FATAL_ERROR "phase9-selection: ctest-json-failed\n${ctest_error}")
endif()
string(JSON test_count ERROR_VARIABLE test_error LENGTH "${ctest_json}" tests)
if(NOT "${test_error}" STREQUAL "NOTFOUND")
  message(FATAL_ERROR "phase9-selection: malformed-ctest-json")
endif()

set(expected_setup
  bus.waitcnt.matrix
  bus.waitcnt.boundary
  dma.registers
  dma.events
  phase9.policy
  phase9.dispute
  phase9.selection.control)
set(actual_setup)
if(test_count GREATER 0)
  math(EXPR test_last "${test_count} - 1")
  foreach(test_index RANGE 0 ${test_last})
    string(JSON test_name GET "${ctest_json}" tests ${test_index} name)
    string(JSON property_count LENGTH "${ctest_json}" tests ${test_index} properties)
    if(property_count GREATER 0)
      math(EXPR property_last "${property_count} - 1")
      foreach(property_index RANGE 0 ${property_last})
        string(JSON property_name GET "${ctest_json}" tests ${test_index}
          properties ${property_index} name)
        if(property_name STREQUAL "FIXTURES_SETUP")
          string(JSON fixture_count LENGTH "${ctest_json}" tests ${test_index}
            properties ${property_index} value)
          if(fixture_count GREATER 0)
            math(EXPR fixture_last "${fixture_count} - 1")
            foreach(fixture_index RANGE 0 ${fixture_last})
              string(JSON fixture GET "${ctest_json}" tests ${test_index}
                properties ${property_index} value ${fixture_index})
              if(fixture STREQUAL "phase9_fast")
                list(APPEND actual_setup "${test_name}")
              endif()
            endforeach()
          endif()
        endif()
      endforeach()
    endif()
  endforeach()
endif()
list(SORT expected_setup)
list(SORT actual_setup)
if(NOT "${actual_setup}" STREQUAL "${expected_setup}")
  message(FATAL_ERROR "phase9-selection: setup-set")
endif()
message(STATUS "phase9-selection: exact-seven setup membership verified")
