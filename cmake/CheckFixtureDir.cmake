# Untracked fixture directory gate (EVID-03, D-15).
#
#   cmake -DROOT=<repository root> -P cmake/CheckFixtureDir.cmake
#
# Pinned third-party fixtures are fetched by scripts/fetch-pin.sh into .fixtures/ at
# the repository root. They are rights-bearing bytes and must never reach source
# control, so this gate proves two facts about the repository at <ROOT>: git ignores
# .fixtures/, and `git ls-files` lists nothing under it. A forced add (`git add -f`)
# passes the ignore rule, which is why the second check exists.
#
# Reason codes (printed as `fixture-dir: <code>[: <detail>]`; no path name is printed):
#   usage         -DROOT= is missing or empty
#   git-failed    <ROOT> is not inside a git work tree, or git itself failed
#   not-ignored   git does not ignore .fixtures/ (the `/.fixtures/` .gitignore line is gone)
#   tracked       one or more paths under .fixtures/ are in the index (the count is printed)
# Success prints `fixture-dir: ok`.
#
# ls-files runs without -z because a CMake variable cannot hold the NUL separators; the
# paths are only counted, never printed.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ROOT OR "${ROOT}" STREQUAL "")
  message(FATAL_ERROR "fixture-dir: usage: -DROOT= is required")
endif()

set(findings 0)

execute_process(COMMAND git -C "${ROOT}" rev-parse --is-inside-work-tree
  RESULT_VARIABLE rc OUTPUT_VARIABLE inside ERROR_QUIET)
string(STRIP "${inside}" inside)
if(NOT rc EQUAL 0 OR NOT inside STREQUAL "true")
  message("fixture-dir: git-failed")
  message(FATAL_ERROR "fixture-dir: failed (1 findings)")
endif()

# check-ignore -q exits 0 when the path is ignored, 1 when it is not, and 128 on an error.
execute_process(COMMAND git -C "${ROOT}" check-ignore -q --no-index .fixtures/probe.dat
  RESULT_VARIABLE rc ERROR_QUIET OUTPUT_QUIET)
if(rc EQUAL 1)
  message("fixture-dir: not-ignored")
  math(EXPR findings "${findings}+1")
elseif(NOT rc EQUAL 0)
  message("fixture-dir: git-failed")
  message(FATAL_ERROR "fixture-dir: failed (1 findings)")
endif()

execute_process(COMMAND git -C "${ROOT}" -c core.quotepath=off ls-files -- .fixtures
  RESULT_VARIABLE rc OUTPUT_VARIABLE listed ERROR_QUIET)
if(NOT rc EQUAL 0)
  message("fixture-dir: git-failed")
  message(FATAL_ERROR "fixture-dir: failed (1 findings)")
endif()
string(REGEX MATCHALL "[^\n]+" paths "${listed}")
list(LENGTH paths path_count)
if(path_count GREATER 0)
  message("fixture-dir: tracked: ${path_count} paths")
  math(EXPR findings "${findings}+1")
endif()

if(findings EQUAL 0)
  message("fixture-dir: ok")
else()
  message(FATAL_ERROR "fixture-dir: failed (${findings} findings)")
endif()
