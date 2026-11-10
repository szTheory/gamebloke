# Controls for the signal handling of the scan scripts (CR-01): the static trap
# gate (scripts/check-traps.sh) and the signal probe (tests/gates/signal_probe.sh).
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/signal_case.cmake
#
# Each negative case plants its bad input in a sandbox under BIN/nc/<case> and
# asserts a non-zero exit AND the gate's stable reason code through
# cmake/ExpectFail.cmake. The offending trap line is assembled from fragments
# so this file never carries the idiom the static gate refuses.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "signal-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "signal-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")

set(dir "${BIN}/nc/signal_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}/scripts")

set(TRAPS sh "${SRC}/scripts/check-traps.sh")

# The cleanup action, written without a literal that would match the gate.
set(cleanup "rm -rf \"\$tmp\"")
string(CONCAT exit_trap "tr" "ap '${cleanup}' EXIT")
string(CONCAT old_idiom "${exit_trap}" " HUP INT TERM")

if(CASE STREQUAL "exit_idiom")
  file(WRITE "${dir}/scripts/bad.sh" "#!/bin/sh\n${old_idiom}\n")
  gbl_expect_fail(COMMAND ${TRAPS} "${dir}" REGEX "traps: exit-with-signals: scripts/bad\\.sh:2")

elseif(CASE STREQUAL "no_handler")
  # An EXIT trap alone leaves a signal with the default action, so the script
  # ends but the cleanup contract is half written; the gate wants all three.
  file(WRITE "${dir}/scripts/bad.sh" "#!/bin/sh\n${exit_trap}\ntrap 'exit 143' TERM\n")
  gbl_expect_fail(COMMAND ${TRAPS} "${dir}" REGEX "traps: no-signal-handler: scripts/bad\\.sh: HUP")

elseif(CASE STREQUAL "no_files")
  # Nothing to scan must fail, never print ok.
  gbl_expect_fail(COMMAND ${TRAPS} "${dir}/absent" REGEX "traps: usage: not a directory" EXIT 2)
  file(REMOVE_RECURSE "${dir}/scripts")
  gbl_expect_fail(COMMAND ${TRAPS} "${dir}" REGEX "traps: no-files")

elseif(CASE STREQUAL "clean")
  file(WRITE "${dir}/scripts/good.sh"
    "#!/bin/sh\n${exit_trap}\ntrap 'exit 129' HUP\ntrap 'exit 130' INT\ntrap 'exit 143' TERM\n")
  file(WRITE "${dir}/scripts/none.sh" "#!/bin/sh\n# no trap here: ${old_idiom}\n")
  gbl_expect_pass(COMMAND ${TRAPS} "${dir}" REGEX "traps: ok \\(2 files\\)")

elseif(CASE STREQUAL "resumes")
  # The probe must notice a scan that resumes after a signal: run it on a copy of
  # the real hygiene script with the pre-fix idiom put back.
  file(READ "${SRC}/scripts/hygiene.sh" content)
  string(CONCAT fixed "${exit_trap}" "\ntrap 'exit 129' HUP\ntrap 'exit 130' INT\ntrap 'exit 143' TERM\n")
  string(REPLACE "${fixed}" "${old_idiom}\n" mutated "${content}")
  if(mutated STREQUAL content)
    message(FATAL_ERROR "mutate: no-match: the real hygiene script has no fixed trap block")
  endif()
  file(WRITE "${dir}/hygiene.sh" "${mutated}")
  gbl_expect_fail(
    COMMAND sh "${SRC}/tests/gates/signal_probe.sh" "${dir}/hygiene.sh" "hygiene: home-path" "${dir}/probe"
    REGEX "signal-probe: resumed")

else()
  message(FATAL_ERROR "signal-case: unknown case: ${CASE}")
endif()
