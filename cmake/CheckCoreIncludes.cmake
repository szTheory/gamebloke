# Core purity gate (D-05): the machine core uses no host includes. Only the
# freestanding headers below may be included with angle brackets; quoted
# in-tree includes are fine.
#
#   cmake -DDIRS=<dir>|<dir> -P cmake/CheckCoreIncludes.cmake
#
# Reason code: core: host-include: <file>:<line>
#
# Control mode (the negative control core.nc.host_include):
#   cmake -DPLANT=<dir> -P cmake/CheckCoreIncludes.cmake
# plants a stdio include in <dir> and requires this gate to refuse it.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

set(GBL_FREESTANDING_HEADERS stddef.h stdint.h stdbool.h limits.h)

function(gbl_check_core_includes)
  set(bad 0)
  foreach(dir IN LISTS ARGN)
    file(GLOB_RECURSE files "${dir}/*.c" "${dir}/*.h")
    foreach(file IN LISTS files)
      file(READ "${file}" content)
      # Neutralize list metacharacters, then prefix every line with "x" so an
      # empty line is still a list element and the index is the line number.
      string(REGEX REPLACE "[][;]" "_" content "${content}")
      string(REPLACE "\n" ";x" content "x${content}")
      set(lineno 0)
      foreach(line IN LISTS content)
        math(EXPR lineno "${lineno} + 1")
        if(line MATCHES "^x[ \t]*#[ \t]*include[ \t]*<([^>]*)>")
          set(header "${CMAKE_MATCH_1}")
          if(NOT header IN_LIST GBL_FREESTANDING_HEADERS)
            message(SEND_ERROR "core: host-include: ${file}:${lineno}: <${header}>")
            set(bad 1)
          endif()
        endif()
      endforeach()
    endforeach()
  endforeach()
  if(bad)
    message(FATAL_ERROR "core: host-include: the core must not use host headers")
  endif()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  if(DEFINED PLANT AND NOT "${PLANT}" STREQUAL "")
    include("${CMAKE_CURRENT_LIST_DIR}/ExpectFail.cmake")
    file(REMOVE_RECURSE "${PLANT}")
    file(MAKE_DIRECTORY "${PLANT}")
    file(WRITE "${PLANT}/x.c" "#include <stdio.h>\nint x(void) { return 0; }\n")
    gbl_expect_fail(
      COMMAND "${CMAKE_COMMAND}" "-DDIRS=${PLANT}" -P "${CMAKE_CURRENT_LIST_FILE}"
      REGEX "core: host-include")
  else()
    if(NOT DEFINED DIRS OR "${DIRS}" STREQUAL "")
      message(FATAL_ERROR "core-includes: -DDIRS= is required")
    endif()
    string(REPLACE "|" ";" dir_list "${DIRS}")
    gbl_check_core_includes(${dir_list})
    message("core: ok")
  endif()
endif()
