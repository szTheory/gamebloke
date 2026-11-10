# Core purity gate (D-05): the machine core uses no host includes. Only the
# freestanding headers below may be included with angle brackets. A quoted
# include is in-tree only when the named file exists under a scanned directory
# (a quoted "stdio.h" resolves through the system include path, so it is a host
# include), and an include whose target is not a literal header (a macro) is
# refused because it cannot be checked.
#
#   cmake -DDIRS=<dir>|<dir> -P cmake/CheckCoreIncludes.cmake
#
# Reason codes:
#   core: host-include: <file>:<line>: <header>   a host, unresolved quoted, or computed include
#   core: no-files: <dir>                         a scanned directory is absent or holds no source file
#                                                  (a renamed directory must not pass by scanning nothing)
#
# A backslash-newline continuation is joined before matching (the finding reports
# the line the directive starts on), and a one-line block comment is dropped.
#
# Control mode (the negative and positive controls):
#   cmake -DPLANT=<dir> [-DSHAPE=<shape>] -P cmake/CheckCoreIncludes.cmake
# plants the shape under <dir>, runs this gate on it, and requires the stated
# result. Shapes: angle (default), quoted, continuation, comment, computed,
# empty, missing, in_tree (the one that must pass).

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

set(GBL_FREESTANDING_HEADERS stddef.h stdint.h stdbool.h limits.h)
set(GBL_CORE_EXTENSIONS c h inc def cc cpp cxx hpp hh S s)

# gbl__core_check_directive(<out-bad> <file> <line> <text> <real dirs>...)
# <text> is one logical line (continuations already joined).
function(gbl__core_check_directive out file lineno text)
  set(real_dirs ${ARGN})
  set(${out} 0 PARENT_SCOPE)
  string(REGEX REPLACE "/\\*[^*]*\\*/" " " text "${text}")
  if(NOT text MATCHES "^[ \t]*#[ \t]*(include_next|include|import)[ \t]*(.*)$")
    return()
  endif()
  set(operand "${CMAKE_MATCH_2}")

  if(operand MATCHES "^<([^>]*)>")
    set(header "${CMAKE_MATCH_1}")
    if(NOT header IN_LIST GBL_FREESTANDING_HEADERS)
      message(SEND_ERROR "core: host-include: ${file}:${lineno}: <${header}>")
      set(${out} 1 PARENT_SCOPE)
    endif()
  elseif(operand MATCHES "^\"([^\"]*)\"")
    set(header "${CMAKE_MATCH_1}")
    set(found 0)
    get_filename_component(here "${file}" DIRECTORY)
    foreach(base IN ITEMS "${here}" ${real_dirs})
      get_filename_component(resolved "${base}/${header}" REALPATH)
      if(EXISTS "${resolved}")
        foreach(rd IN LISTS real_dirs)
          string(FIND "${resolved}" "${rd}/" pos)
          if(pos EQUAL 0)
            set(found 1)
          endif()
        endforeach()
      endif()
    endforeach()
    if(NOT found AND NOT header IN_LIST GBL_FREESTANDING_HEADERS)
      message(SEND_ERROR "core: host-include: ${file}:${lineno}: \"${header}\"")
      set(${out} 1 PARENT_SCOPE)
    endif()
  else()
    message(SEND_ERROR "core: host-include: ${file}:${lineno}: computed include")
    set(${out} 1 PARENT_SCOPE)
  endif()
endfunction()

function(gbl_check_core_includes)
  set(bad 0)

  set(real_dirs "")
  foreach(dir IN LISTS ARGN)
    if(IS_DIRECTORY "${dir}")
      get_filename_component(rd "${dir}" REALPATH)
      list(APPEND real_dirs "${rd}")
    endif()
  endforeach()

  foreach(dir IN LISTS ARGN)
    set(patterns "")
    foreach(ext IN LISTS GBL_CORE_EXTENSIONS)
      list(APPEND patterns "${dir}/*.${ext}")
    endforeach()
    set(files "")
    if(IS_DIRECTORY "${dir}")
      file(GLOB_RECURSE files ${patterns})
      list(REMOVE_DUPLICATES files)
    endif()
    if(NOT files)
      message(SEND_ERROR "core: no-files: ${dir}")
      set(bad 1)
      continue()
    endif()

    foreach(file IN LISTS files)
      file(READ "${file}" content)
      string(REPLACE "\r" "" content "${content}")
      # A backslash before the list separator would escape it and glue two
      # lines together, so spell it out for the continuation test below.
      string(REPLACE "\\" "@@BS@@" content "${content}")
      # Neutralize list metacharacters, then prefix every line with "x" so an
      # empty line is still a list element and the index is the line number.
      string(REGEX REPLACE "[][;]" "_" content "${content}")
      string(REPLACE "\n" ";x" content "x${content}")
      set(lineno 0)
      set(joining 0)
      set(logical "")
      set(start 0)
      foreach(raw IN LISTS content)
        math(EXPR lineno "${lineno} + 1")
        string(SUBSTRING "${raw}" 1 -1 text)
        if(NOT joining)
          set(logical "")
          set(start ${lineno})
        endif()
        if(text MATCHES "^(.*)@@BS@@[ \t]*$")
          string(APPEND logical "${CMAKE_MATCH_1}")
          set(joining 1)
          continue()
        endif()
        string(APPEND logical "${text}")
        set(joining 0)
        gbl__core_check_directive(line_bad "${file}" ${start} "${logical}" ${real_dirs})
        if(line_bad)
          set(bad 1)
        endif()
      endforeach()
      if(joining)
        gbl__core_check_directive(line_bad "${file}" ${start} "${logical}" ${real_dirs})
        if(line_bad)
          set(bad 1)
        endif()
      endif()
    endforeach()
  endforeach()

  if(bad)
    message(FATAL_ERROR "core: host-include: the core must not use host headers, and must scan source")
  endif()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  if(DEFINED PLANT AND NOT "${PLANT}" STREQUAL "")
    include("${CMAKE_CURRENT_LIST_DIR}/ExpectFail.cmake")
    if(NOT DEFINED SHAPE OR "${SHAPE}" STREQUAL "")
      set(SHAPE angle)
    endif()
    file(REMOVE_RECURSE "${PLANT}")
    file(MAKE_DIRECTORY "${PLANT}")
    set(dirs "${PLANT}")
    set(expect "core: host-include:[ \n]+[^ \n]*x\\.c:1:[ \n]+<stdio\\.h>")
    set(body "")
    if(SHAPE STREQUAL "angle")
      set(body "#include <stdio.h>\nint x(void) { return 0; }\n")
    elseif(SHAPE STREQUAL "quoted")
      # Resolves through the system include path, not the tree.
      set(body "#include \"stdio.h\"\nint x(void) { return 0; }\n")
      set(expect "core: host-include:[ \n]+[^ \n]*x\\.c:1:[ \n]+\"stdio\\.h\"")
    elseif(SHAPE STREQUAL "continuation")
      set(body "#include \\\n  <stdio.h>\nint x(void) { return 0; }\n")
    elseif(SHAPE STREQUAL "comment")
      set(body "#include /* c */ <stdio.h>\nint x(void) { return 0; }\n")
    elseif(SHAPE STREQUAL "computed")
      set(body "#define HOST_HEADER <stdio.h>\n#include HOST_HEADER\nint x(void) { return 0; }\n")
      set(expect "core: host-include:[ \n]+[^ \n]*x\\.c:2:[ \n]+computed[ \n]+include")
    elseif(SHAPE STREQUAL "empty")
      set(expect "core: no-files")
    elseif(SHAPE STREQUAL "missing")
      set(dirs "${PLANT}/absent")
      set(expect "core: no-files")
    elseif(SHAPE STREQUAL "in_tree")
      file(MAKE_DIRECTORY "${PLANT}/src" "${PLANT}/inc")
      file(WRITE "${PLANT}/inc/a.h" "int a(void);\n")
      file(WRITE "${PLANT}/src/a.c"
        "#include \"a.h\"\n#include \\\n  <stdint.h>\n#include /* ok */ <stddef.h>\nint a(void) { return 0; }\n")
      set(dirs "${PLANT}/src|${PLANT}/inc")
    else()
      message(FATAL_ERROR "core-includes: unknown SHAPE '${SHAPE}'")
    endif()
    if(NOT "${body}" STREQUAL "")
      file(WRITE "${PLANT}/x.c" "${body}")
    endif()
    if(SHAPE STREQUAL "in_tree")
      gbl_expect_pass(COMMAND "${CMAKE_COMMAND}" "-DDIRS=${dirs}" -P "${CMAKE_CURRENT_LIST_FILE}"
        REGEX "core: ok")
    else()
      gbl_expect_fail(
        COMMAND "${CMAKE_COMMAND}" "-DDIRS=${dirs}" -P "${CMAKE_CURRENT_LIST_FILE}"
        REGEX "${expect}")
    endif()
  else()
    if(NOT DEFINED DIRS OR "${DIRS}" STREQUAL "")
      message(FATAL_ERROR "core-includes: -DDIRS= is required")
    endif()
    string(REPLACE "|" ";" dir_list "${DIRS}")
    gbl_check_core_includes(${dir_list})
    message("core: ok")
  endif()
endif()
