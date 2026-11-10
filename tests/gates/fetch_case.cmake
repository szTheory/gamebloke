# Pinned-fetch and fixture-directory control cases (EVID-03, D-04, D-15, D-16).
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/fetch_case.cmake
#
# Every case builds its own sandbox under BIN/nc/fetch_<case>: a synthesized payload
# (a fixed fragment repeated, never real fixture bytes), a pins.txt whose single row
# points at that payload through a file:// URL, and a fetch root. No test touches the
# network (D-16): file:// is accepted by scripts/fetch-pin.sh only under
# GBL_FETCH_ALLOW_FILE=1, which the driver sets for every run except the one control
# that proves the refusal. Each negative case asserts a non-zero exit AND the stable
# reason text through cmake/ExpectFail.cmake (never an exact exit status), then asserts
# that nothing was placed at the destination. A stale pins mutation fails with
# `mutate: no-match`. The committed pins.txt and the repository's .fixtures/ are never
# touched.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "fetch-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "fetch-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/fetch_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")

set(FETCH "${SRC}/scripts/fetch-pin.sh")
set(PINS "${dir}/pins.txt")
set(ROOT "${dir}/root")
set(DEST "example/payload.bin")

# ---- sandbox ----------------------------------------------------------------
# new_sandbox(): a synthesized payload, its digest, a one-row pins file, and an empty
# root. Calling it again resets the pins file and the root (variants of one case).
function(new_sandbox)
  file(REMOVE_RECURSE "${ROOT}")
  file(MAKE_DIRECTORY "${ROOT}")
  string(REPEAT "synthesized-fetch-fragment;" 40 body)
  file(WRITE "${dir}/payload.bin" "${body}\n")
  file(SHA256 "${dir}/payload.bin" digest)
  string(REPLACE " " "%20" url_path "${dir}/payload.bin")
  file(WRITE "${PINS}" "# synthesized pins\n${digest} file://${url_path} ${DEST}\n")
  set(DIGEST "${digest}" PARENT_SCOPE)
  set(URL_PATH "${url_path}" PARENT_SCOPE)
  set(ROW "${digest} file://${url_path} ${DEST}" PARENT_SCOPE)
endfunction()

# fetch_cmd(<out-var> <dest> [NO_FILE_OK] [ENV <var=value>...]): the full command line.
function(fetch_cmd out dest)
  cmake_parse_arguments(PARSE_ARGV 2 X "NO_FILE_OK" "" "ENV")
  set(env "CMAKE=${CMAKE_COMMAND}")
  if(NOT X_NO_FILE_OK)
    list(APPEND env "GBL_FETCH_ALLOW_FILE=1")
  endif()
  list(APPEND env ${X_ENV})
  set(${out} "${CMAKE_COMMAND}" -E env ${env}
    sh "${FETCH}" --pins "${PINS}" --root "${ROOT}" "${dest}" PARENT_SCOPE)
endfunction()

# assert_clean_root(): no temporary directory is left behind.
function(assert_clean_root)
  file(GLOB leftovers LIST_DIRECTORIES true "${ROOT}/.fetch.*")
  if(leftovers)
    message(FATAL_ERROR "fetch-case: temporary directory left behind: ${leftovers}")
  endif()
endfunction()

# assert_not_placed(<relative-path>): nothing exists at <root>/<path> (a link included).
function(assert_not_placed rel)
  if(EXISTS "${ROOT}/${rel}" OR IS_SYMLINK "${ROOT}/${rel}")
    message(FATAL_ERROR "fetch-case: something was placed at ${rel}")
  endif()
endfunction()

# expect_fetch_fail(<code> <dest> [NO_FILE_OK] [ENV ...]): non-zero exit plus the reason,
# then a clean root.
function(expect_fetch_fail code dest)
  fetch_cmd(cmd "${dest}" ${ARGN})
  gbl_expect_fail(COMMAND ${cmd} REGEX "fetch-pin: ${code}")
  assert_clean_root()
endfunction()

# expect_placed(): the fetch succeeds and the placed file carries the pinned digest.
function(expect_placed)
  fetch_cmd(cmd "${DEST}")
  gbl_expect_pass(COMMAND ${cmd} REGEX "fetch-pin: ok: example/payload.bin")
  file(SHA256 "${ROOT}/${DEST}" placed)
  if(NOT placed STREQUAL DIGEST)
    message(FATAL_ERROR "fetch-case: placed file digest differs from the pin")
  endif()
  assert_clean_root()
endfunction()

# ---- cases ------------------------------------------------------------------
if(CASE STREQUAL "fetch_ok")
  new_sandbox()
  expect_placed()

elseif(CASE STREQUAL "digest_mismatch")
  # The pin names the digest of different synthesized bytes.
  new_sandbox()
  string(REPEAT "other-synthesized-fragment;" 40 other_body)
  file(WRITE "${dir}/other.bin" "${other_body}\n")
  file(SHA256 "${dir}/other.bin" other_digest)
  gbl_text_mutate("${PINS}" "${DIGEST}" "${other_digest}")
  expect_fetch_fail(digest-mismatch "${DEST}")
  assert_not_placed("${DEST}")

else()
  message(FATAL_ERROR "fetch-case: unknown case: ${CASE}")
endif()
