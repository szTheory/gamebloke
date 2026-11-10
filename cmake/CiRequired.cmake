# Aggregator predicate behind the `CI required` check (D-11, Pitfall 6).
#
#   env NEEDS  the workflow's toJSON(needs)
#   env HEAVY  needs.changes.outputs.heavy ("true", "false", or empty)
#   env EVENT  github.event_name
#   cmake -P cmake/CiRequired.cmake
#
# A needed job passes when its result is `success`. Otherwise it passes only
# when it is `skipped` AND one of:
#   - HEAVY is exactly "false" and the job is not changes, hygiene, or title
#     (a .planning/-only pull request skips the heavy lanes on purpose);
#   - the job is `title` and EVENT is not pull_request (the title job only
#     exists on pull requests).
# An unset or empty HEAVY is not "false", so a heavy job skipped because
# `changes` failed fails the check instead of passing it.
#
# Reason codes this file prints:
#   ci-required: needs is empty
#   ci-required: needs is not valid json
#   ci-required: not allowed: <job>=<result> ...
#   ci-required: ok (<n> jobs, heavy=<HEAVY>, event=<EVENT>)
cmake_minimum_required(VERSION 3.25)

set(needs "$ENV{NEEDS}")
if("${needs}" STREQUAL "")
  message(FATAL_ERROR "ci-required: needs is empty")
endif()
string(JSON count ERROR_VARIABLE jerr LENGTH "${needs}")
if(NOT "${jerr}" STREQUAL "NOTFOUND")
  message(FATAL_ERROR "ci-required: needs is not valid json: ${jerr}")
endif()
if(count EQUAL 0)
  message(FATAL_ERROR "ci-required: needs is empty")
endif()

set(heavy "$ENV{HEAVY}")
set(event "$ENV{EVENT}")
set(bad "")
math(EXPR last "${count} - 1")
foreach(i RANGE ${last})
  string(JSON name MEMBER "${needs}" ${i})
  string(JSON result ERROR_VARIABLE rerr GET "${needs}" "${name}" result)
  if(NOT "${rerr}" STREQUAL "NOTFOUND")
    set(result "missing")
  endif()
  if(result STREQUAL "success")
    continue()
  endif()
  if(result STREQUAL "skipped" AND heavy STREQUAL "false"
      AND NOT name MATCHES "^(changes|hygiene|title)$")
    continue()
  endif()
  if(result STREQUAL "skipped" AND name STREQUAL "title"
      AND NOT event STREQUAL "pull_request")
    continue()
  endif()
  list(APPEND bad "${name}=${result}")
endforeach()

if(bad)
  list(JOIN bad " " bad_text)
  message(FATAL_ERROR "ci-required: not allowed: ${bad_text}")
endif()
message("ci-required: ok (${count} jobs, heavy=${heavy}, event=${event})")
