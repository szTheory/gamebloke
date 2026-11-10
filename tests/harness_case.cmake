# Inventory negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/harness_case.cmake
#
# Every junit shape is written at test time into BIN/nc/<case>/ from the bracket
# strings below, so each shape sits next to the case that uses it and no junit
# fixture file is committed. The shapes follow what CTest writes (RESEARCH.md
# Pattern 3): pass is status "run"; a skip is status "notrun" with a <skipped>
# child and the test output in <system-out>; DISABLED is status "disabled"; a
# failure is status "fail" with a <failure> child.
#
# Each control asserts a non-zero exit AND the gate's stated reason code
# through cmake/ExpectFail.cmake.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "harness-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "harness-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")

set(dir "${BIN}/nc/${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(junit "${dir}/junit.xml")
set(inventory "${dir}/inventory.txt")

# Three synthetic tests (one skip-ok) and a `post`-labelled line that the
# exclude regex removes, in the real inventory line format.
file(WRITE "${inventory}" [==[
# synthetic inventory for the junit control cases
syn.alpha unit
syn.beta unit
syn.gamma owner,unit skip-ok
syn.post post
]==])

# ---- junit shapes ---------------------------------------------------------
set(SUITE_HEAD [==[<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="(empty)"
	tests="@COUNT@"
	failures="0"
	disabled="0"
	skipped="0"
	hostname=""
	time="0"
	timestamp="2026-01-01T00:00:00"
	>
]==])
set(SUITE_TAIL [==[</testsuite>
]==])

# pass
set(TC_RUN [==[	<testcase name="@N@" classname="@N@" time="0.01" status="run">
		<properties/>
		<system-out></system-out>
	</testcase>
]==])
# failure
set(TC_FAIL [==[	<testcase name="@N@" classname="@N@" time="0.01" status="fail">
		<failure message="Failed"/>
		<properties/>
		<system-out></system-out>
	</testcase>
]==])
# SKIP_REGULAR_EXPRESSION skip with the reason printed by the test (includes a
# semicolon, which a CMake list would split)
set(TC_SKIP_REASON [==[	<testcase name="@N@" classname="@N@" time="0.01" status="notrun">
		<skipped message="SKIP_REGULAR_EXPRESSION_MATCHED"/>
		<properties/>
		<system-out>SKIPPED: owner file not set; skipping
</system-out>
	</testcase>
]==])
# skip without any SKIPPED: reason line in the output
set(TC_SKIP_BARE [==[	<testcase name="@N@" classname="@N@" time="0.01" status="notrun">
		<skipped message="SKIP_RETURN_CODE=77"/>
		<properties/>
		<system-out>nothing useful here
</system-out>
	</testcase>
]==])
# DISABLED: CTest records no reason
set(TC_DISABLED [==[	<testcase name="@N@" classname="@N@" time="0" status="disabled">
		<properties/>
		<system-out>Disabled</system-out>
	</testcase>
]==])

# tc(<out-var> <template> <name>)
function(tc out template name)
  string(REPLACE "@N@" "${name}" body "${${template}}")
  set(${out} "${body}" PARENT_SCOPE)
endfunction()

# write_junit(<count> <testcase text>)
function(write_junit count body)
  string(REPLACE "@COUNT@" "${count}" head "${SUITE_HEAD}")
  file(WRITE "${junit}" "${head}${body}${SUITE_TAIL}")
endfunction()

tc(RUN_ALPHA TC_RUN syn.alpha)
tc(RUN_BETA TC_RUN syn.beta)
tc(RUN_GAMMA TC_RUN syn.gamma)

set(check_cmd sh "${SRC}/scripts/check-junit.sh" "${junit}" "${inventory}" post)

# ---- cases ----------------------------------------------------------------
if(CASE STREQUAL "pass")
  write_junit(3 "${RUN_ALPHA}${RUN_BETA}${RUN_GAMMA}")
  gbl_expect_pass(COMMAND ${check_cmd}
    REGEX "inventory: expected 3, ran 3, skipped 0 \\(allowed 0\\)")

elseif(CASE STREQUAL "skipped_allowed")
  tc(SKIP_GAMMA TC_SKIP_REASON syn.gamma)
  write_junit(3 "${RUN_ALPHA}${RUN_BETA}${SKIP_GAMMA}")
  gbl_expect_pass(COMMAND ${check_cmd}
    REGEX "SKIPPED syn.gamma: SKIP_REGULAR_EXPRESSION_MATCHED \\| owner file not set")

elseif(CASE STREQUAL "missing")
  write_junit(2 "${RUN_ALPHA}${RUN_GAMMA}")
  gbl_expect_fail(COMMAND ${check_cmd} REGEX "inventory: missing: syn.beta")

elseif(CASE STREQUAL "extra")
  tc(RUN_EXTRA TC_RUN syn.unlisted)
  write_junit(4 "${RUN_ALPHA}${RUN_BETA}${RUN_GAMMA}${RUN_EXTRA}")
  gbl_expect_fail(COMMAND ${check_cmd} REGEX "inventory: extra: syn.unlisted")

elseif(CASE STREQUAL "duplicate")
  write_junit(4 "${RUN_ALPHA}${RUN_ALPHA}${RUN_BETA}${RUN_GAMMA}")
  gbl_expect_fail(COMMAND ${check_cmd} REGEX "inventory: duplicate: syn.alpha")

elseif(CASE STREQUAL "failed")
  tc(FAIL_BETA TC_FAIL syn.beta)
  write_junit(3 "${RUN_ALPHA}${FAIL_BETA}${RUN_GAMMA}")
  gbl_expect_fail(COMMAND ${check_cmd} REGEX "inventory: failed: syn.beta")

elseif(CASE STREQUAL "skipped_not_allowed")
  # syn.beta is not skip-ok, so even a skip with a stated reason fails.
  tc(SKIP_BETA TC_SKIP_REASON syn.beta)
  write_junit(3 "${RUN_ALPHA}${SKIP_BETA}${RUN_GAMMA}")
  gbl_expect_fail(COMMAND ${check_cmd} REGEX "inventory: skipped-not-allowed: syn.beta")

elseif(CASE STREQUAL "skipped_no_reason")
  # syn.gamma is skip-ok, but nothing in its output says why it skipped.
  tc(SKIP_GAMMA TC_SKIP_BARE syn.gamma)
  write_junit(3 "${RUN_ALPHA}${RUN_BETA}${SKIP_GAMMA}")
  gbl_expect_fail(COMMAND ${check_cmd} REGEX "inventory: skipped-no-reason: syn.gamma")

elseif(CASE STREQUAL "disabled")
  tc(OFF_BETA TC_DISABLED syn.beta)
  write_junit(3 "${RUN_ALPHA}${OFF_BETA}${RUN_GAMMA}")
  gbl_expect_fail(COMMAND ${check_cmd} REGEX "inventory: disabled: syn.beta")

elseif(CASE STREQUAL "empty_junit")
  write_junit(0 "")
  gbl_expect_fail(COMMAND ${check_cmd} REGEX "inventory: empty-junit")

elseif(CASE STREQUAL "no_junit")
  gbl_expect_fail(
    COMMAND sh "${SRC}/scripts/check-junit.sh" "${dir}/does-not-exist.xml" "${inventory}" post
    REGEX "inventory: no-junit")

elseif(CASE STREQUAL "configure_mismatch")
  # The registered set lacks syn.gamma that the inventory lists.
  file(WRITE "${dir}/registered.txt" [==[
syn.alpha unit
syn.beta unit
syn.post post
]==])
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DREGISTERED_FILE=${dir}/registered.txt"
      "-DINVENTORY=${inventory}" -P "${SRC}/cmake/InventoryCompare.cmake"
    REGEX "inventory: configure-mismatch")

elseif(CASE STREQUAL "unsorted")
  file(WRITE "${dir}/registered.txt" [==[
syn.alpha unit
syn.beta unit
syn.post post
]==])
  file(WRITE "${dir}/unsorted.txt" [==[
syn.beta unit
syn.alpha unit
syn.post post
]==])
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DREGISTERED_FILE=${dir}/registered.txt"
      "-DINVENTORY=${dir}/unsorted.txt" -P "${SRC}/cmake/InventoryCompare.cmake"
    REGEX "inventory: unsorted")

elseif(CASE STREQUAL "meta")
  # The wrapper itself must fail when the wrapped command succeeds.
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DCMD=${CMAKE_COMMAND}" "-DARGS=-E|true" "-DREGEX=." -P
      "${SRC}/cmake/ExpectFail.cmake"
    REGEX "gate PASSED but must fail")

elseif(CASE STREQUAL "add_test_bad_name" OR CASE STREQUAL "add_test_no_labels")
  # A minimal project with no languages configures in about a second. The
  # refusal is a FATAL_ERROR at the gbl_add_test call, so it fires before the
  # deferred inventory compare and the sandbox needs no inventory file. The
  # positive companion is every real configure of this repository.
  set(proj "${dir}/proj")
  file(MAKE_DIRECTORY "${proj}")
  if(CASE STREQUAL "add_test_bad_name")
    set(registration "gbl_add_test(NAME Bad.Name LABELS unit COMMAND true)")
    set(reason "gbl_add_test: bad-name")
  else()
    set(registration "gbl_add_test(NAME ok.name COMMAND true)")
    set(reason "gbl_add_test: no-labels")
  endif()
  file(WRITE "${proj}/CMakeLists.txt"
"cmake_minimum_required(VERSION 3.25)
project(gbl_nc NONE)
set(GBL_LANE ci)
enable_testing()
include(\"${SRC}/cmake/GblTests.cmake\")
${registration}
")
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -S "${proj}" -B "${proj}/build" -G Ninja
    REGEX "${reason}")

else()
  message(FATAL_ERROR "harness-case: unknown case: ${CASE}")
endif()
