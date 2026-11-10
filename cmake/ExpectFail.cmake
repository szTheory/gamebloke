# Negative-control wrapper. A control passes only when the gate command fails
# AND its combined output carries the stated reason code, so a gate that dies
# for the wrong reason, or that passes, makes the control fail.
#
#   include(cmake/ExpectFail.cmake)
#   gbl_expect_fail(COMMAND <cmd>... REGEX <re> [EXIT <n>] [WORKING_DIRECTORY <dir>])
#   gbl_expect_pass(COMMAND <cmd>... [REGEX <re>] [WORKING_DIRECTORY <dir>])
#
# Script mode (a ';' inside one add_test argument would be split, so the
# program is -DCMD= and extra arguments are -DARGS= separated by '|'):
#   cmake -DCMD=<program> [-DARGS=a|b] -DREGEX=<re> [-DEXIT=<n>] -P cmake/ExpectFail.cmake
#
# EXIT exists only for documented program exit codes (the runner's 2). Never
# assert an exact exit code for a sanitizer control: macOS aborts with 134 and
# Linux exits 1.
#
# Reason codes this file prints:
#   expect-fail: gate PASSED but must fail
#   expect-fail: wrong exit
#   expect-fail: failed for the wrong reason
#   expect-fail: ok
#   expect-pass: gate FAILED on clean input
#   expect-pass: ok

# Controls run gates that abort on purpose; leak reports would only add noise.
function(gbl__expect_env)
  if("$ENV{ASAN_OPTIONS}" STREQUAL "")
    set(ENV{ASAN_OPTIONS} "detect_leaks=0")
  else()
    set(ENV{ASAN_OPTIONS} "$ENV{ASAN_OPTIONS}:detect_leaks=0")
  endif()
endfunction()

function(gbl_expect_fail)
  cmake_parse_arguments(PARSE_ARGV 0 X "" "REGEX;EXIT;WORKING_DIRECTORY" "COMMAND")
  if(NOT X_COMMAND OR "${X_REGEX}" STREQUAL "")
    message(FATAL_ERROR "expect-fail: COMMAND and REGEX are required")
  endif()
  gbl__expect_env()
  set(wd "")
  if(X_WORKING_DIRECTORY)
    set(wd WORKING_DIRECTORY "${X_WORKING_DIRECTORY}")
  endif()
  execute_process(COMMAND ${X_COMMAND} ${wd}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)

  if("${rc}" STREQUAL "0")
    message(FATAL_ERROR
      "expect-fail: gate PASSED but must fail (negative control broken)\n${out}")
  endif()
  if(NOT "${X_EXIT}" STREQUAL "" AND NOT "${rc}" STREQUAL "${X_EXIT}")
    message(FATAL_ERROR
      "expect-fail: wrong exit: wanted ${X_EXIT}, got ${rc}\n${out}")
  endif()
  if(NOT "${out}" MATCHES "${X_REGEX}")
    message(FATAL_ERROR
      "expect-fail: failed for the wrong reason; wanted /${X_REGEX}/ got exit ${rc}:\n${out}")
  endif()
  message("${out}")
  message("expect-fail: ok: /${X_REGEX}/ (exit ${rc})")
endfunction()

# Positive control: the same gate on clean input must pass. REGEX, when given,
# must also match the output (for example a reported skip reason).
function(gbl_expect_pass)
  cmake_parse_arguments(PARSE_ARGV 0 X "" "REGEX;WORKING_DIRECTORY" "COMMAND")
  if(NOT X_COMMAND)
    message(FATAL_ERROR "expect-pass: COMMAND is required")
  endif()
  gbl__expect_env()
  set(wd "")
  if(X_WORKING_DIRECTORY)
    set(wd WORKING_DIRECTORY "${X_WORKING_DIRECTORY}")
  endif()
  execute_process(COMMAND ${X_COMMAND} ${wd}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)

  if(NOT "${rc}" STREQUAL "0")
    message(FATAL_ERROR
      "expect-pass: gate FAILED on clean input (exit ${rc})\n${out}")
  endif()
  if(NOT "${X_REGEX}" STREQUAL "" AND NOT "${out}" MATCHES "${X_REGEX}")
    message(FATAL_ERROR
      "expect-pass: output lacks /${X_REGEX}/:\n${out}")
  endif()
  message("${out}")
  message("expect-pass: ok")
endfunction()

# Script-mode entry (inert when this file is include()d by another script).
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  foreach(var CMD REGEX)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
      message(FATAL_ERROR "expect-fail: -D${var}= is required")
    endif()
  endforeach()
  set(arg_list "")
  if(DEFINED ARGS AND NOT "${ARGS}" STREQUAL "")
    string(REPLACE "|" ";" arg_list "${ARGS}")
  endif()
  set(extra "")
  if(DEFINED EXIT AND NOT "${EXIT}" STREQUAL "")
    set(extra EXIT "${EXIT}")
  endif()
  gbl_expect_fail(COMMAND "${CMD}" ${arg_list} REGEX "${REGEX}" ${extra})
endif()
