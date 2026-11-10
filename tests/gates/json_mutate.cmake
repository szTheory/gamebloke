# Mutation and output-assertion helpers for the evidence control drivers.
#
#   include(tests/gates/json_mutate.cmake)
#   gbl_json_mutate_set(<file> <json-value> <key>...)   SET a member; a string value is passed with its quotes
#   gbl_json_mutate_remove(<file> <key>...)             REMOVE a member
#   gbl_text_mutate(<file> <find> <replace>)            replace the first occurrence of a literal
#   gbl_assert_no_echo(<marker> COMMAND <cmd>...)       the command's output must not contain <marker>
#
# string(JSON REMOVE) of an absent member succeeds silently and returns the
# document unchanged, so a misspelled target would leave a control testing an
# unmutated copy. Every helper therefore asserts that it changed something:
#   mutate: no-match: <file>        the document or text is unchanged
#   no-echo: leaked: <n> chars      the marker (never printed) is in the output
#   no-echo: ok                     the marker is absent from the output
# gbl_assert_no_echo does not judge the exit status; the caller pairs it with
# gbl_expect_fail on the same command.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)
include_guard(GLOBAL)

function(gbl__json_write_if_changed file before after)
  string(JSON same ERROR_VARIABLE err EQUAL "${before}" "${after}")
  if(NOT "${err}" STREQUAL "NOTFOUND" AND NOT "${err}" STREQUAL "")
    message(FATAL_ERROR "mutate: bad-document: ${file}")
  endif()
  if("${same}" STREQUAL "ON")
    message(FATAL_ERROR "mutate: no-match: ${file}")
  endif()
  file(WRITE "${file}" "${after}\n")
endfunction()

function(gbl_json_mutate_set file value)
  file(READ "${file}" before)
  string(JSON after ERROR_VARIABLE err SET "${before}" ${ARGN} "${value}")
  if(NOT "${err}" STREQUAL "NOTFOUND" AND NOT "${err}" STREQUAL "")
    message(FATAL_ERROR "mutate: bad-path: ${file}")
  endif()
  gbl__json_write_if_changed("${file}" "${before}" "${after}")
endfunction()

function(gbl_json_mutate_remove file)
  file(READ "${file}" before)
  string(JSON after ERROR_VARIABLE err REMOVE "${before}" ${ARGN})
  if(NOT "${err}" STREQUAL "NOTFOUND" AND NOT "${err}" STREQUAL "")
    message(FATAL_ERROR "mutate: bad-path: ${file}")
  endif()
  gbl__json_write_if_changed("${file}" "${before}" "${after}")
endfunction()

function(gbl_text_mutate file find replace)
  file(READ "${file}" content)
  string(FIND "${content}" "${find}" pos)
  if(pos EQUAL -1 OR "${find}" STREQUAL "${replace}")
    message(FATAL_ERROR "mutate: no-match: ${file}")
  endif()
  string(LENGTH "${find}" find_len)
  math(EXPR tail_at "${pos}+${find_len}")
  string(SUBSTRING "${content}" 0 ${pos} head)
  string(SUBSTRING "${content}" ${tail_at} -1 tail)
  file(WRITE "${file}" "${head}${replace}${tail}")
endfunction()

function(gbl_assert_no_echo marker)
  cmake_parse_arguments(PARSE_ARGV 1 X "" "" "COMMAND")
  if("${marker}" STREQUAL "" OR NOT X_COMMAND)
    message(FATAL_ERROR "no-echo: usage: marker and COMMAND are required")
  endif()
  execute_process(COMMAND ${X_COMMAND}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  string(FIND "${out}" "${marker}" pos)
  if(NOT pos EQUAL -1)
    string(LENGTH "${marker}" len)
    message(FATAL_ERROR "no-echo: leaked: ${len} chars")
  endif()
  message("no-echo: ok")
endfunction()
