# Strict JSON loader and finding accumulator for the evidence validators (D-01, D-02).
#
#   include(cmake/GblJson.cmake)
#   gbl_json_load(<out-var> <file>)           whole-file parse, the only caller of string(JSON) on a file
#   gbl_json_type(<out> <json> <key>...)      JSON type, or MISSING
#   gbl_json_string(<out> <json> <key>...)    non-empty STRING value, else <missing>
#   gbl_json_length(<out> <json> <key>...)    ARRAY length, else -1
#   gbl_finding(<family> <code> <detail>)     prints `<family>: <code>: <detail>`, counts it
#   gbl_findings_finish(<family> <ok-line>)   prints `<family>: <ok-line>` or ends non-zero
#
# Script mode:   cmake -DFILE=<path> -P cmake/GblJson.cmake
#
# CMake's string(JSON) is lenient (probed on 3.25.3 and 4.4.3): it accepts
# comments, trailing commas, content after the root, and duplicate keys (last
# wins). gbl_json_load adds the strictness guard, so a hand-edited authority
# file fails here instead of passing silently. Reason codes this file prints:
#   json: usage             script mode without -DFILE=
#   json: unreadable        the file is absent or is a directory
#   json: not-json          empty, whitespace-only, or not parseable
#   json: not-object        the root is not an object
#   json: comment           a // or /* */ comment outside a string
#   json: trailing-comma    a comma directly before a closing bracket or brace
#   json: trailing-content  anything but whitespace after the root's closer
#   json: duplicate-key     two members of one object share a name
#   json: ok                script mode only: the file loaded cleanly
# Every finding prints the family, code, and path; never file content and never
# the string(JSON) error text (4.x echoes the input there).
#
# Residual leniencies accepted on purpose (harmless: validators compare numeric
# fields as text after a TYPE check): leading-zero and `+1` numbers, raw tabs
# inside strings, a byte-order mark, invalid UTF-8.
#
# Rules for callers: never iterate JSON text with foreach(IN LISTS) (a value
# holding `;` splits), never use if(<var>) on JSON-derived strings (`no` is
# false), check TYPE before GET (GET gives the same empty string for "" and
# null), and guard every index loop with LENGTH GREATER 0 (foreach(RANGE -1)
# iterates twice). Only the string(JSON) subcommands GET, TYPE, MEMBER, LENGTH,
# REMOVE, SET and EQUAL are used: later ones do not exist on the 3.25 floor.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)
include_guard(GLOBAL)

# ---- finding accumulator ------------------------------------------------------
set_property(GLOBAL PROPERTY GBL_FINDINGS 0)

function(gbl_finding family code detail)
  message("${family}: ${code}: ${detail}")
  get_property(n GLOBAL PROPERTY GBL_FINDINGS)
  if("${n}" STREQUAL "")
    set(n 0)
  endif()
  math(EXPR n "${n}+1")
  set_property(GLOBAL PROPERTY GBL_FINDINGS "${n}")
endfunction()

function(gbl_findings_finish family ok_line)
  get_property(n GLOBAL PROPERTY GBL_FINDINGS)
  if("${n}" STREQUAL "")
    set(n 0)
  endif()
  if(n EQUAL 0)
    message("${family}: ${ok_line}")
  else()
    message(FATAL_ERROR "${family}: failed (${n} findings)")
  endif()
endfunction()

# ---- typed accessors (TYPE first, Pitfall 3) ----------------------------------
function(gbl_json_type out json)
  string(JSON ty ERROR_VARIABLE err TYPE "${json}" ${ARGN})
  if(NOT "${err}" STREQUAL "NOTFOUND" AND NOT "${err}" STREQUAL "")
    set(ty MISSING)
  endif()
  set(${out} "${ty}" PARENT_SCOPE)
endfunction()

function(gbl_json_string out json)
  gbl_json_type(ty "${json}" ${ARGN})
  set(value "<missing>")
  if("${ty}" STREQUAL "STRING")
    string(JSON got ERROR_VARIABLE err GET "${json}" ${ARGN})
    if(("${err}" STREQUAL "NOTFOUND" OR "${err}" STREQUAL "") AND NOT "${got}" STREQUAL "")
      set(value "${got}")
    endif()
  endif()
  set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(gbl_json_length out json)
  gbl_json_type(ty "${json}" ${ARGN})
  set(len -1)
  if("${ty}" STREQUAL "ARRAY")
    string(JSON len LENGTH "${json}" ${ARGN})
  endif()
  set(${out} "${len}" PARENT_SCOPE)
endfunction()

# ---- strictness helpers -------------------------------------------------------
# gbl__json_census(<members-out> <objects-out> <arrays-out> <json>): the number
# of object members, objects, and arrays in the parsed document, recursively.
# The member count is compared with the raw count of `"key":` tokens: a
# duplicate key parses as one member but is two tokens.
function(gbl__json_census members_out objects_out arrays_out json)
  set(members 0)
  set(objects 0)
  set(arrays 0)
  string(JSON ty TYPE "${json}")
  if("${ty}" STREQUAL "OBJECT")
    set(objects 1)
    string(JSON len LENGTH "${json}")
    set(members ${len})
    if(len GREATER 0)
      math(EXPR last "${len}-1")
      foreach(i RANGE ${last})
        string(JSON m MEMBER "${json}" ${i})
        string(JSON cty TYPE "${json}" "${m}")
        if("${cty}" STREQUAL "OBJECT" OR "${cty}" STREQUAL "ARRAY")
          string(JSON child GET "${json}" "${m}")
          gbl__json_census(cm co ca "${child}")
          math(EXPR members "${members}+${cm}")
          math(EXPR objects "${objects}+${co}")
          math(EXPR arrays "${arrays}+${ca}")
        endif()
      endforeach()
    endif()
  elseif("${ty}" STREQUAL "ARRAY")
    set(arrays 1)
    string(JSON len LENGTH "${json}")
    if(len GREATER 0)
      math(EXPR last "${len}-1")
      foreach(i RANGE ${last})
        string(JSON cty TYPE "${json}" ${i})
        if("${cty}" STREQUAL "OBJECT" OR "${cty}" STREQUAL "ARRAY")
          string(JSON child GET "${json}" ${i})
          gbl__json_census(cm co ca "${child}")
          math(EXPR members "${members}+${cm}")
          math(EXPR objects "${objects}+${co}")
          math(EXPR arrays "${arrays}+${ca}")
        endif()
      endforeach()
    endif()
  endif()
  set(${members_out} ${members} PARENT_SCOPE)
  set(${objects_out} ${objects} PARENT_SCOPE)
  set(${arrays_out} ${arrays} PARENT_SCOPE)
endfunction()

# gbl__json_root_end(<out> <text>): index of the character that closes the root,
# found by a structural depth walk over text whose strings are already stripped
# (a bracket-class regex under-counts on large documents), or -1 when the depth
# never returns to 0.
function(gbl__json_root_end out text)
  string(LENGTH "${text}" n)
  set(depth 0)
  set(end -1)
  set(i 0)
  while(i LESS n)
    string(SUBSTRING "${text}" ${i} 1 c)
    if("${c}" STREQUAL "{" OR "${c}" STREQUAL "[")
      math(EXPR depth "${depth}+1")
    elseif("${c}" STREQUAL "}" OR "${c}" STREQUAL "]")
      math(EXPR depth "${depth}-1")
      if(depth EQUAL 0)
        set(end ${i})
        break()
      endif()
    endif()
    math(EXPR i "${i}+1")
  endwhile()
  set(${out} ${end} PARENT_SCOPE)
endfunction()

# ---- loader -------------------------------------------------------------------
function(gbl_json_load out file)
  if(NOT EXISTS "${file}" OR IS_DIRECTORY "${file}")
    message("json: unreadable: ${file}")
    message(FATAL_ERROR "json: failed")
  endif()
  file(READ "${file}" text)

  # Parse success is ERROR_VARIABLE NOTFOUND (3.25.3 and 4.4.3) or empty. Empty
  # and whitespace-only text land here as not-json. The error text is never printed.
  string(JSON ty ERROR_VARIABLE err TYPE "${text}")
  if(NOT "${err}" STREQUAL "NOTFOUND" AND NOT "${err}" STREQUAL "")
    message("json: not-json: ${file}")
    message(FATAL_ERROR "json: failed")
  endif()
  if(NOT "${ty}" STREQUAL "OBJECT")
    message("json: not-object: ${file}")
    message(FATAL_ERROR "json: failed")
  endif()

  # Strictness guard. Step 1: every string literal becomes the single letter S.
  string(REGEX REPLACE "\"([^\"\\\\]|\\\\.)*\"" "S" stripped "${text}")
  set(bad FALSE)

  # Step 2: comments. Steps 3-5 assume comment-free text, so skip them.
  if("${stripped}" MATCHES "//|/\\*")
    message("json: comment: ${file}")
    set(bad TRUE)
  else()
    # Whitespace carries no structure; dropping it shortens the walk.
    string(REGEX REPLACE "[ \t\r\n]+" "" compact "${stripped}")

    # Step 3: root walk. Step 4: only whitespace may follow the root's closer.
    gbl__json_root_end(root_end "${compact}")
    string(LENGTH "${compact}" total)
    if(root_end LESS 0)
      message("json: trailing-content: ${file}")
      set(bad TRUE)
      set(region "${compact}")
    else()
      math(EXPR after "${root_end}+1")
      string(SUBSTRING "${compact}" 0 ${after} region)
      if(after LESS total)
        message("json: trailing-content: ${file}")
        set(bad TRUE)
      endif()
    endif()

    # Step 5: on the root region only, so anything after the root is reported
    # once, as trailing-content.
    if("${region}" MATCHES ",[]}]")
      message("json: trailing-comma: ${file}")
      set(bad TRUE)
    endif()
    # A known structural refusal cannot become accepted. Avoid walking the
    # complete parsed tree again; every otherwise acceptable object, including
    # a duplicate-key-only input, still requires the full census below.
    if(NOT bad)
      string(REGEX MATCHALL "S:" raw_keys "${region}")
      list(LENGTH raw_keys raw_count)
      gbl__json_census(parsed_members parsed_objects parsed_arrays "${text}")
      if(NOT raw_count EQUAL parsed_members)
        message("json: duplicate-key: ${file}")
        set(bad TRUE)
      endif()
    endif()
  endif()

  if(bad)
    message(FATAL_ERROR "json: failed")
  endif()
  set(${out} "${text}" PARENT_SCOPE)
endfunction()

# ---- script-mode entry (inert when include()d) --------------------------------
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  if(NOT DEFINED FILE OR "${FILE}" STREQUAL "")
    message(FATAL_ERROR "json: usage: -DFILE= is required")
  endif()
  gbl_json_load(doc "${FILE}")
  message("json: ok: ${FILE}")
endif()
