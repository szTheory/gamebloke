# Pins negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/pins_case.cmake
#
# Every input is a mutated COPY of pins.txt (the committed header and comments) with two
# good rows appended, under BIN/nc/pins_<case>; the committed file is never modified and
# no bad example is committed (D-04). `rows` proves the unmutated sandbox is accepted, so
# every negative case fails only for its planted reason. Each negative variant asserts a
# non-zero exit AND the linter's stable reason code through cmake/ExpectFail.cmake; a
# stale mutation fails with `mutate: no-match`. A variant changes one thing; co-reported
# findings are accepted because the REGEX asserts that the targeted code line is present.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "pins-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "pins-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/pins_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(CHECK "${SRC}/cmake/CheckManifest.cmake")
set(HEADER "# gamebloke.pins/1 <sha256> <url> <dest>")

# Synthesized digests, built from fragments.
string(REPEAT "ab" 32 DIGEST_A)
string(REPEAT "cd" 32 DIGEST_B)
set(ROW_A "${DIGEST_A} https://example.invalid/a.dat example/a.dat")
set(ROW_B "${DIGEST_B} https://example.invalid/b.dat example/b.dat")

# pins_copy(<out-var> <variant>): a fresh sandbox copy: the committed pins file plus two
# good rows.
function(pins_copy out variant)
  set(path "${dir}/pins_${variant}.txt")
  file(COPY_FILE "${SRC}/pins.txt" "${path}")
  file(APPEND "${path}" "${ROW_A}\n${ROW_B}\n")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# expect_code(<code> <pins-file>): MODE=pins fails with `pins: <code>`.
function(expect_code code pins)
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -DMODE=pins "-DPINS=${pins}" -P "${CHECK}"
    REGEX "pins: ${code}")
endfunction()

if(CASE STREQUAL "rows")
  # The header, two good rows, a blank line, and a comment.
  pins_copy(p rows)
  file(APPEND "${p}" "\n# a trailing comment\n")
  gbl_expect_pass(
    COMMAND "${CMAKE_COMMAND}" -DMODE=pins "-DPINS=${p}" -P "${CHECK}"
    REGEX "pins: ok \\(2 rows\\)")

elseif(CASE STREQUAL "missing_file")
  expect_code(missing-file "${dir}/absent.txt")

elseif(CASE STREQUAL "missing_header")
  # A zero-byte file, the header line deleted, and the header version changed.
  set(p "${dir}/pins_empty.txt")
  file(WRITE "${p}" "")
  expect_code(missing-header "${p}")
  pins_copy(p deleted)
  gbl_text_mutate("${p}" "${HEADER}\n" "")
  expect_code(missing-header "${p}")
  pins_copy(p version)
  gbl_text_mutate("${p}" "# gamebloke.pins/1 " "# gamebloke.pins/2 ")
  expect_code(missing-header "${p}")

elseif(CASE STREQUAL "bad_row")
  # Two fields, four fields, and a semicolon.
  pins_copy(p two)
  gbl_text_mutate("${p}" " example/a.dat" "")
  expect_code(bad-row "${p}")
  pins_copy(p four)
  gbl_text_mutate("${p}" "example/a.dat" "example/a.dat extra")
  expect_code(bad-row "${p}")
  pins_copy(p semicolon)
  gbl_text_mutate("${p}" "example/a.dat" "example/a;b.dat")
  expect_code(bad-row "${p}")

elseif(CASE STREQUAL "bad_digest")
  # A 63-hex digest and an uppercase digest.
  string(REPEAT "a" 63 short63)
  string(TOUPPER "${DIGEST_A}" upper)
  pins_copy(p short)
  gbl_text_mutate("${p}" "${DIGEST_A}" "${short63}")
  expect_code(bad-digest "${p}")
  pins_copy(p upper)
  gbl_text_mutate("${p}" "${DIGEST_A}" "${upper}")
  expect_code(bad-digest "${p}")

elseif(CASE STREQUAL "bad_url")
  # http, file, and a URL holding a double quote.
  set(i 0)
  foreach(v IN ITEMS "http://example.invalid/a.dat" "file:///a" "https://example.invalid/a\"b")
    pins_copy(p url${i})
    gbl_text_mutate("${p}" "https://example.invalid/a.dat" "${v}")
    expect_code(bad-url "${p}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_dest")
  # Parent traversal, an absolute path, a `.` component, and a character outside the set.
  set(i 0)
  foreach(v IN ITEMS "../x" "/abs" "a/./b" "a\$b")
    pins_copy(p dest${i})
    gbl_text_mutate("${p}" "example/a.dat" "${v}")
    expect_code(bad-dest "${p}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "duplicate_dest")
  pins_copy(p dup)
  file(APPEND "${p}" "${DIGEST_B} https://example.invalid/c.dat example/a.dat\n")
  expect_code(duplicate-dest "${p}")

else()
  message(FATAL_ERROR "pins-case: unknown case: ${CASE}")
endif()
