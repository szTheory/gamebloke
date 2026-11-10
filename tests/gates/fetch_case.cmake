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

# make_link(<target> <link>): a symbolic link planted in the sandbox.
function(make_link target link)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${target}" "${link}"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "fetch-case: could not create the symlink")
  endif()
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

elseif(CASE STREQUAL "idempotent")
  # Fetching the same pin twice succeeds both times and leaves the verified bytes.
  new_sandbox()
  expect_placed()
  expect_placed()

elseif(CASE STREQUAL "usage")
  # No dest, two dests, and an unknown option. The script documents status 2 for a
  # usage error, but no control pins an exit status.
  new_sandbox()
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -E env "CMAKE=${CMAKE_COMMAND}" "GBL_FETCH_ALLOW_FILE=1"
      sh "${FETCH}" --pins "${PINS}" --root "${ROOT}"
    REGEX "fetch-pin: usage")
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -E env "CMAKE=${CMAKE_COMMAND}" "GBL_FETCH_ALLOW_FILE=1"
      sh "${FETCH}" --pins "${PINS}" --root "${ROOT}" "${DEST}" example/second.bin
    REGEX "fetch-pin: usage")
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -E env "CMAKE=${CMAKE_COMMAND}" "GBL_FETCH_ALLOW_FILE=1"
      sh "${FETCH}" --bogus --pins "${PINS}" --root "${ROOT}" "${DEST}"
    REGEX "fetch-pin: usage")
  assert_clean_root()
  assert_not_placed("${DEST}")

elseif(CASE STREQUAL "missing_pins")
  new_sandbox()
  file(REMOVE "${PINS}")
  expect_fetch_fail(missing-pins "${DEST}")
  assert_not_placed("${DEST}")

elseif(CASE STREQUAL "no_row")
  new_sandbox()
  expect_fetch_fail(no-row example/other.bin)
  assert_not_placed("example/other.bin")
  assert_not_placed("${DEST}")

elseif(CASE STREQUAL "bad_row")
  # A row with two fields (the dest removed) and two rows for one dest.
  new_sandbox()
  gbl_text_mutate("${PINS}" " ${DEST}" "")
  expect_fetch_fail(bad-row "${DEST}")
  assert_not_placed("${DEST}")

  new_sandbox()
  gbl_text_mutate("${PINS}" "${ROW}" "${ROW}\n${ROW}")
  expect_fetch_fail(bad-row "${DEST}")
  assert_not_placed("${DEST}")

elseif(CASE STREQUAL "bad_digest")
  # 63 hex characters, and the same digest in upper case.
  new_sandbox()
  string(SUBSTRING "${DIGEST}" 1 -1 short_digest)
  gbl_text_mutate("${PINS}" "${DIGEST}" "${short_digest}")
  expect_fetch_fail(bad-digest "${DEST}")
  assert_not_placed("${DEST}")

  new_sandbox()
  string(TOUPPER "${DIGEST}" upper_digest)
  gbl_text_mutate("${PINS}" "${DIGEST}" "${upper_digest}")
  expect_fetch_fail(bad-digest "${DEST}")
  assert_not_placed("${DEST}")

elseif(CASE STREQUAL "bad_url")
  # http://, ftp://, file:// without the test variable, and a URL with a double quote.
  new_sandbox()
  gbl_text_mutate("${PINS}" "file://" "http://")
  expect_fetch_fail(bad-url "${DEST}")
  assert_not_placed("${DEST}")

  new_sandbox()
  gbl_text_mutate("${PINS}" "file://" "ftp://")
  expect_fetch_fail(bad-url "${DEST}")
  assert_not_placed("${DEST}")

  new_sandbox()
  expect_fetch_fail(bad-url "${DEST}" NO_FILE_OK)
  assert_not_placed("${DEST}")

  new_sandbox()
  gbl_text_mutate("${PINS}" "/payload.bin " "/pay\"load.bin ")
  expect_fetch_fail(bad-url "${DEST}")
  assert_not_placed("${DEST}")

elseif(CASE STREQUAL "bad_dest")
  # Traversal, an absolute path, a dot component, and a space. The sandbox parent holds
  # a decoy at the traversal target so a missing check could not hide.
  new_sandbox()
  foreach(bad IN ITEMS "../evil.dat" "/abs.dat" "a/./b" "a b")
    expect_fetch_fail(bad-dest "${bad}")
  endforeach()
  assert_not_placed("${DEST}")
  if(EXISTS "${dir}/evil.dat" OR EXISTS "${dir}/abs.dat")
    message(FATAL_ERROR "fetch-case: a refused dest was written")
  endif()

elseif(CASE STREQUAL "no_curl")
  new_sandbox()
  expect_fetch_fail(no-curl "${DEST}" ENV "CURL=${dir}/no-such-curl")
  assert_not_placed("${DEST}")

elseif(CASE STREQUAL "download_failed")
  # The URL names a sandbox file that does not exist.
  new_sandbox()
  gbl_text_mutate("${PINS}" "/payload.bin " "/missing.bin ")
  expect_fetch_fail(download-failed "${DEST}")
  assert_not_placed("${DEST}")

elseif(CASE STREQUAL "symlink")
  # A parent that is a link out of the root, and a link at the destination.
  new_sandbox()
  file(MAKE_DIRECTORY "${dir}/outside")
  make_link("${dir}/outside" "${ROOT}/example")
  expect_fetch_fail(symlink "${DEST}")
  if(EXISTS "${dir}/outside/payload.bin")
    message(FATAL_ERROR "fetch-case: the payload escaped the root")
  endif()

  new_sandbox()
  file(WRITE "${dir}/victim.txt" "untouched\n")
  file(MAKE_DIRECTORY "${ROOT}/example")
  make_link("${dir}/victim.txt" "${ROOT}/${DEST}")
  expect_fetch_fail(symlink "${DEST}")
  file(READ "${dir}/victim.txt" victim)
  if(NOT victim STREQUAL "untouched\n")
    message(FATAL_ERROR "fetch-case: the link target was overwritten")
  endif()

elseif(CASE STREQUAL "dest_is_dir")
  new_sandbox()
  file(MAKE_DIRECTORY "${ROOT}/${DEST}")
  expect_fetch_fail(dest-is-dir "${DEST}")
  file(GLOB inside "${ROOT}/${DEST}/*")
  if(inside)
    message(FATAL_ERROR "fetch-case: something was placed inside the directory")
  endif()

elseif(CASE STREQUAL "not_ignored" OR CASE STREQUAL "tracked"
    OR CASE STREQUAL "not_git" OR CASE STREQUAL "untracked_usage")
  # The untracked-fixtures gate (cmake/CheckFixtureDir.cmake) over sandbox repositories.
  # git runs with the GIT_* state scrubbed, no system config, and HOME and the XDG
  # config home pointed at the sandbox, so a developer's global excludes cannot hide the
  # planted state.
  set(CHECK_FIXTURE "${SRC}/cmake/CheckFixtureDir.cmake")
  set(GIT_ENV "${CMAKE_COMMAND}" -E env
    --unset=GIT_DIR --unset=GIT_WORK_TREE --unset=GIT_INDEX_FILE
    --unset=GIT_CEILING_DIRECTORIES
    "GIT_CONFIG_NOSYSTEM=1" "HOME=${dir}" "XDG_CONFIG_HOME=${dir}")
  set(repo "${dir}/repo")

  function(git_in)
    execute_process(COMMAND ${GIT_ENV} git ${ARGN}
      WORKING_DIRECTORY "${repo}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text)
    if(NOT rc EQUAL 0)
      message(FATAL_ERROR "fetch-case: git ${ARGN} failed (${rc}) in the sandbox:\n${text}")
    endif()
  endfunction()

  if(CASE STREQUAL "not_ignored")
    # A repository with no ignore rule for .fixtures.
    file(MAKE_DIRECTORY "${repo}")
    git_in(init -q)
    gbl_expect_fail(
      COMMAND ${GIT_ENV} "${CMAKE_COMMAND}" "-DROOT=${repo}" -P "${CHECK_FIXTURE}"
      REGEX "fixture-dir: not-ignored")

  elseif(CASE STREQUAL "tracked")
    # .fixtures is ignored, but one file was force-added to the index.
    file(MAKE_DIRECTORY "${repo}/.fixtures")
    git_in(init -q)
    file(WRITE "${repo}/.gitignore" "/.fixtures/\n")
    file(WRITE "${repo}/.fixtures/x.dat" "synthesized\n")
    git_in(add -f .fixtures/x.dat)
    gbl_expect_fail(
      COMMAND ${GIT_ENV} "${CMAKE_COMMAND}" "-DROOT=${repo}" -P "${CHECK_FIXTURE}"
      REGEX "fixture-dir: tracked")

  elseif(CASE STREQUAL "not_git")
    # A plain directory. The ceiling is the PARENT of the sandbox directory, so discovery
    # cannot climb into the repository that holds the build tree (a ceiling equal to the
    # directory itself still finds an enclosing repository).
    set(plain "${dir}/plain")
    file(MAKE_DIRECTORY "${plain}")
    get_filename_component(ceiling "${dir}" DIRECTORY)
    gbl_expect_fail(
      COMMAND "${CMAKE_COMMAND}" -E env "GIT_CEILING_DIRECTORIES=${ceiling}"
        "GIT_CONFIG_NOSYSTEM=1" "HOME=${dir}" "XDG_CONFIG_HOME=${dir}"
        "${CMAKE_COMMAND}" "-DROOT=${plain}" -P "${CHECK_FIXTURE}"
      REGEX "fixture-dir: git-failed")

  else()
    gbl_expect_fail(
      COMMAND "${CMAKE_COMMAND}" -P "${CHECK_FIXTURE}"
      REGEX "fixture-dir: usage")
  endif()

else()
  message(FATAL_ERROR "fetch-case: unknown case: ${CASE}")
endif()
