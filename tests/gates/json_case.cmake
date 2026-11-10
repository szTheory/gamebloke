# Strict-loader negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/json_case.cmake
#
# Every input is synthesized at test time under BIN/nc/json_<case>: a mutated
# COPY of contracts/evidence.json, a tiny `{"a":1}` base, or a copy of a real
# committed document. No bad example is committed (D-04). Each negative variant
# asserts a non-zero exit AND the loader's stable reason code through
# cmake/ExpectFail.cmake. A variant whose planted text holds the marker below
# also asserts that the marker never reaches the output (no content echo).
#
# Internal cases `mutate_probe` and `no_echo_probe` are run only as children of
# `mutate_no_match` and `no_echo_guard`; they are never registered.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "json-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "json-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(SELF "${CMAKE_CURRENT_LIST_FILE}")
set(dir "${BIN}/nc/json_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(LOADER "${SRC}/cmake/GblJson.cmake")

# One token, assembled from fragments: it occurs in no reason code, case name,
# or path segment, so finding it in output means file content was echoed.
string(CONCAT MARKER "zq" "xv" "7731")

# copy_vocab(<out-var> <name>): a sandbox copy of the real vocabulary file.
function(copy_vocab out name)
  set(path "${dir}/${name}.json")
  file(COPY_FILE "${SRC}/contracts/evidence.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# expect_code(<code> <file> [QUIET]): the loader fails on <file> with
# `json: <code>`; with QUIET the output must also lack the marker.
function(expect_code code file)
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DFILE=${file}" -P "${LOADER}"
    REGEX "json: ${code}")
  if("${ARGN}" STREQUAL "QUIET")
    gbl_assert_no_echo("${MARKER}"
      COMMAND "${CMAKE_COMMAND}" "-DFILE=${file}" -P "${LOADER}")
  endif()
endfunction()

# The fixed set of real committed documents (B1). It stops the case when the
# set shrinks, so a trailing-content control cannot pass over nothing.
function(real_docs out)
  file(GLOB a "${SRC}/contracts/*.json")
  file(GLOB b "${SRC}/contracts/examples/*.json")
  file(GLOB c "${SRC}/profiles/*.json")
  file(GLOB d "${SRC}/registry/*.json")
  file(GLOB e "${SRC}/tests/fixtures/*.json")
  set(all ${a} ${b} ${c} ${d} ${e}
    "${SRC}/CMakePresets.json" "${SRC}/.github/rulesets/main.json")
  list(REMOVE_DUPLICATES all)
  list(SORT all)
  list(LENGTH all n)
  list(FIND all "${SRC}/contracts/evidence.json" at)
  if(at EQUAL -1 OR n LESS 3)
    message(FATAL_ERROR "json-case: real document set shrank (${n} documents)")
  endif()
  set(${out} "${all}" PARENT_SCOPE)
endfunction()

# tail_text(<out> <name>): the seven trailing tails by name (a bare `;` cannot
# live in a list).
function(tail_text out name)
  if(name STREQUAL "rbrace")
    set(t "}")
  elseif(name STREQUAL "comma")
    set(t ",")
  elseif(name STREQUAL "rbracket")
    set(t "]")
  elseif(name STREQUAL "colon")
    set(t ":")
  elseif(name STREQUAL "semicolon")
    set(t ";")
  elseif(name STREQUAL "word")
    set(t " ${MARKER}")
  elseif(name STREQUAL "root")
    set(t "{\"${MARKER}\":1}")
  else()
    message(FATAL_ERROR "json-case: unknown tail: ${name}")
  endif()
  set(${out} "${t}" PARENT_SCOPE)
endfunction()

# ---- negative cases ----------------------------------------------------------
if(CASE STREQUAL "unreadable")
  expect_code(unreadable "${dir}/absent.json")

elseif(CASE STREQUAL "not_json")
  file(WRITE "${dir}/empty.json" "")
  expect_code(not-json "${dir}/empty.json")
  file(WRITE "${dir}/blank.json" "  \n\t\n  \n")
  expect_code(not-json "${dir}/blank.json")
  file(WRITE "${dir}/text.json" "${MARKER} is not json at all\n")
  expect_code(not-json "${dir}/text.json" QUIET)

elseif(CASE STREQUAL "not_object")
  file(WRITE "${dir}/array.json" "[1, 2]\n")
  expect_code(not-object "${dir}/array.json")
  file(WRITE "${dir}/string.json" "\"x\"\n")
  expect_code(not-object "${dir}/string.json")
  file(WRITE "${dir}/number.json" "12\n")
  expect_code(not-object "${dir}/number.json")

elseif(CASE STREQUAL "comment")
  copy_vocab(f line)
  gbl_text_mutate("${f}" "{\n  \"schema\"" "// ${MARKER}\n{\n  \"schema\"")
  expect_code(comment "${f}" QUIET)
  copy_vocab(f block)
  gbl_text_mutate("${f}" "\"gamebloke.evidence/1\"," "\"gamebloke.evidence/1\", /* ${MARKER} */")
  expect_code(comment "${f}" QUIET)

elseif(CASE STREQUAL "trailing_comma")
  copy_vocab(f brace)
  gbl_text_mutate("${f}" "\"refusal\": \"required\"}" "\"refusal\": \"required\",}")
  expect_code(trailing-comma "${f}")
  copy_vocab(f bracket)
  gbl_text_mutate("${f}" "\"UNK\"]" "\"UNK\",]")
  expect_code(trailing-comma "${f}")

elseif(CASE STREQUAL "trailing_content")
  # Seven tails on the tiny base and on a copy of every real document. The
  # tails append to the file, so each variant always differs from its base.
  real_docs(docs)
  file(WRITE "${dir}/base_tiny.json" "{\"a\":1}\n")
  set(bases "${dir}/base_tiny.json")
  set(n 0)
  foreach(doc IN LISTS docs)
    file(COPY_FILE "${doc}" "${dir}/base_${n}.json")
    list(APPEND bases "${dir}/base_${n}.json")
    math(EXPR n "${n}+1")
  endforeach()
  foreach(base IN LISTS bases)
    foreach(tail IN ITEMS rbrace comma rbracket colon semicolon word root)
      get_filename_component(stem "${base}" NAME_WE)
      set(f "${dir}/${stem}_${tail}.json")
      file(COPY_FILE "${base}" "${f}")
      tail_text(text "${tail}")
      file(APPEND "${f}" "${text}")
      if(tail STREQUAL "word" OR tail STREQUAL "root")
        expect_code(trailing-content "${f}" QUIET)
      else()
        expect_code(trailing-content "${f}")
      endif()
    endforeach()
  endforeach()

elseif(CASE STREQUAL "duplicate_key")
  copy_vocab(f top)
  gbl_text_mutate("${f}" "\"schema\": \"gamebloke.evidence/1\","
    "\"schema\": \"gamebloke.evidence/1\", \"${MARKER}\": 1, \"${MARKER}\": 2,")
  expect_code(duplicate-key "${f}" QUIET)
  copy_vocab(f nested)
  gbl_text_mutate("${f}" "{\"name\": \"PASS\","
    "{\"name\": \"PASS\", \"${MARKER}\": 1, \"${MARKER}\": 2,")
  expect_code(duplicate-key "${f}" QUIET)

elseif(CASE STREQUAL "usage")
  gbl_expect_fail(COMMAND "${CMAKE_COMMAND}" -P "${LOADER}" REGEX "json: usage")

elseif(CASE STREQUAL "mutate_no_match")
  # One no-op variant per mutation helper, each in a child run of mutate_probe.
  foreach(mode IN ITEMS set remove text)
    gbl_expect_fail(
      COMMAND "${CMAKE_COMMAND}" -DCASE=mutate_probe "-DMODE=${mode}"
        "-DSRC=${SRC}" "-DBIN=${BIN}" -P "${SELF}"
      REGEX "mutate: no-match")
  endforeach()

elseif(CASE STREQUAL "mutate_probe")
  if(NOT DEFINED MODE OR "${MODE}" STREQUAL "")
    message(FATAL_ERROR "json-case: mutate_probe needs -DMODE=")
  endif()
  copy_vocab(f probe)
  if(MODE STREQUAL "set")
    # The value the member already holds.
    gbl_json_mutate_set("${f}" "\"gamebloke.evidence/1\"" schema)
  elseif(MODE STREQUAL "remove")
    gbl_json_mutate_remove("${f}" no_such_member)
  elseif(MODE STREQUAL "text")
    string(CONCAT absent "no" "_such" "_literal")
    gbl_text_mutate("${f}" "${absent}" "x")
  else()
    message(FATAL_ERROR "json-case: unknown mutate mode: ${MODE}")
  endif()

elseif(CASE STREQUAL "no_echo_guard")
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -DCASE=no_echo_probe
      "-DSRC=${SRC}" "-DBIN=${BIN}" -P "${SELF}"
    REGEX "no-echo: leaked")

elseif(CASE STREQUAL "no_echo_probe")
  # A script that prints the marker and fails: the assertion must trip.
  file(WRITE "${dir}/probe.cmake"
    "message(\"leak ${MARKER}\")\nmessage(FATAL_ERROR \"probe\")\n")
  gbl_assert_no_echo("${MARKER}" COMMAND "${CMAKE_COMMAND}" -P "${dir}/probe.cmake")

# ---- positive cases ----------------------------------------------------------
elseif(CASE STREQUAL "strict_clean")
  # Colons and slashes inside strings, an escaped quote then a colon, a value
  # ending in an escaped backslash, brackets and braces inside a string, an
  # empty object and array, and an array of objects reusing a key per row.
  file(WRITE "${dir}/clean.json" [==[
{
  "colon": "x:y",
  "url": "https://x/y",
  "esc_quote_colon": "say \"a\": b",
  "ends_backslash": "ends with \\",
  "brackets": "text with ] } [ { , ] inside",
  "comma_bracket": "a,]",
  "empty_obj": {},
  "empty_arr": [],
  "rows": [{"k": 1}, {"k": 2}, {"k": 3}],
  "nested": {"a": {"b": [1, 2, {"c": "d"}]}}
}
]==])
  gbl_expect_pass(
    COMMAND "${CMAKE_COMMAND}" "-DFILE=${dir}/clean.json" -P "${LOADER}"
    REGEX "json: ok: ")

elseif(CASE STREQUAL "real_docs")
  real_docs(docs)
  set(n 0)
  foreach(doc IN LISTS docs)
    gbl_expect_pass(
      COMMAND "${CMAKE_COMMAND}" "-DFILE=${doc}" -P "${LOADER}"
      REGEX "json: ok: ")
    math(EXPR n "${n}+1")
  endforeach()
  message("json-real: ${n} documents")

else()
  message(FATAL_ERROR "json-case: unknown case: ${CASE}")
endif()
