# Manifest negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/manifest_case.cmake
#
# Every input is a mutated COPY of contracts/examples/manifest.json (with copies of
# tests/fixtures/manifest.json, pins.txt, registry/disputes.json, and
# contracts/evidence.json) under BIN/nc/manifest_<case>; the committed files are never
# modified and no bad example is committed (D-04). Each negative variant asserts a
# non-zero exit AND the linter's stable reason code through cmake/ExpectFail.cmake; a
# stale mutation fails with `mutate: no-match`. A variant changes one field;
# co-reported findings are accepted because the REGEX asserts that the targeted code
# line is present.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "manifest-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "manifest-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/manifest_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(CHECK "${SRC}/cmake/CheckManifest.cmake")
file(COPY_FILE "${SRC}/contracts/examples/manifest.json" "${dir}/manifest.json")
file(COPY_FILE "${SRC}/tests/fixtures/manifest.json" "${dir}/empty.json")
file(COPY_FILE "${SRC}/pins.txt" "${dir}/pins.txt")
file(COPY_FILE "${SRC}/registry/disputes.json" "${dir}/disputes.json")
file(COPY_FILE "${SRC}/contracts/evidence.json" "${dir}/evidence.json")
set(VOCAB_COPY "${dir}/evidence.json")
set(REGISTRY_COPY "${dir}/disputes.json")
set(PINS_COPY "${dir}/pins.txt")

# The example row's commit, used to build variants.
set(COMMIT "0123456789abcdef0123456789abcdef01234567")

# manifest_copy(<out-var> <variant>): a fresh sandbox copy of the example manifest.
function(manifest_copy out variant)
  set(path "${dir}/manifest_${variant}.json")
  file(COPY_FILE "${dir}/manifest.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# manifest_command(<out-var> <mode> <manifest>): the linter command line.
function(manifest_command out mode manifest)
  set(${out}
    "${CMAKE_COMMAND}" "-DMODE=${mode}" "-DMANIFEST=${manifest}" "-DPINS=${PINS_COPY}"
    "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    PARENT_SCOPE)
endfunction()

# expect_code(<code> <manifest>): the linter (MODE=manifest) fails with <code>.
function(expect_code code manifest)
  manifest_command(cmd manifest "${manifest}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "manifest: ${code}")
endfunction()

# pins_copy(<out-var> <variant> <rows-text>): a sandbox pins file, the committed header and
# comments plus <rows-text> appended.
function(pins_copy out variant rows)
  set(path "${dir}/pins_${variant}.txt")
  file(COPY_FILE "${PINS_COPY}" "${path}")
  file(APPEND "${path}" "${rows}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# all_command(<out-var> <manifest> <pins>): the MODE=all command line.
function(all_command out manifest pins)
  set(${out}
    "${CMAKE_COMMAND}" -DMODE=all "-DMANIFEST=${manifest}" "-DPINS=${pins}"
    "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    PARENT_SCOPE)
endfunction()

# append_row(<manifest> <index>): append a copy of row 0 at <index> (one past the end).
function(append_row file index)
  file(READ "${file}" doc)
  string(JSON row GET "${doc}" fixtures 0)
  gbl_json_mutate_set("${file}" "${row}" fixtures ${index})
endfunction()

# sha(<out-var> <hex-fragment> <times>): a synthesized hex string.
function(sha out fragment times)
  string(REPEAT "${fragment}" ${times} value)
  set(${out} "${value}" PARENT_SCOPE)
endfunction()

# set_source(<manifest> <index> <json>): replace a row's source object.
function(set_source file index json)
  gbl_json_mutate_set("${file}" "${json}" fixtures ${index} source)
endfunction()

if(CASE STREQUAL "complete_row")
  manifest_copy(m complete)
  manifest_command(cmd manifest "${m}")
  gbl_expect_pass(COMMAND ${cmd} REGEX "manifest: ok \\(1 rows\\)")

elseif(CASE STREQUAL "unpinned_revision")
  # A branch, HEAD, a short name, a 7-hex short SHA, uppercase hex, and a row with only
  # a tag are all outside a 40-lowercase-hex commit.
  string(TOUPPER "${COMMIT}" upper)
  set(i 0)
  foreach(v IN ITEMS "main" "HEAD" "master" "0123abc" "${upper}")
    manifest_copy(m commit${i})
    gbl_json_mutate_set("${m}" "\"${v}\"" fixtures 0 source commit)
    expect_code(unpinned-revision "${m}")
    math(EXPR i "${i}+1")
  endforeach()
  manifest_copy(m tag_only)
  gbl_json_mutate_remove("${m}" fixtures 0 source commit)
  expect_code(unpinned-revision "${m}")

elseif(CASE STREQUAL "usage")
  # No MODE, an unknown MODE, and MODE=all without PINS.
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DMANIFEST=${dir}/manifest.json" "-DPINS=${PINS_COPY}"
      "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "manifest: usage")
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -DMODE=bogus "-DMANIFEST=${dir}/manifest.json"
      "-DPINS=${PINS_COPY}" "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "manifest: usage")
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -DMODE=all "-DMANIFEST=${dir}/manifest.json"
      "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "manifest: usage")

elseif(CASE STREQUAL "bad_schema")
  manifest_copy(m schema)
  gbl_json_mutate_set("${m}" "\"gamebloke.manifest/0\"" schema)
  expect_code(bad-schema "${m}")

elseif(CASE STREQUAL "missing_field")
  # Every EVID-01 field path: removed; set to null (except the two keys whose null is a
  # legal no-value form); set to an empty string (string paths only); plus the empty
  # notices array and an empty fetch object.
  set(all_paths
    id source source/kind source/url license license/spdx license/notices
    license/redistributable build build/recipe build/toolchain build/toolchain/identity
    build/toolchain/sha256 bios_requirement expected expected/provenance_class
    expected/dispute permitted_use)
  set(null_ok_paths expected/dispute build/toolchain/sha256)
  set(string_paths
    id source/kind source/url license/spdx license/redistributable build/recipe
    build/toolchain/identity build/toolchain/sha256 bios_requirement
    expected/provenance_class expected/dispute)
  foreach(path IN LISTS all_paths)
    string(REPLACE "/" ";" keys "${path}")
    string(REPLACE "/" "_" tag "${path}")
    manifest_copy(m rm_${tag})
    gbl_json_mutate_remove("${m}" fixtures 0 ${keys})
    expect_code(missing-field "${m}")
    if(NOT "${path}" IN_LIST null_ok_paths)
      manifest_copy(m null_${tag})
      gbl_json_mutate_set("${m}" "null" fixtures 0 ${keys})
      expect_code(missing-field "${m}")
    endif()
    if("${path}" IN_LIST string_paths)
      manifest_copy(m empty_${tag})
      gbl_json_mutate_set("${m}" "\"\"" fixtures 0 ${keys})
      expect_code(missing-field "${m}")
    endif()
  endforeach()
  manifest_copy(m notices_empty)
  gbl_json_mutate_set("${m}" "[]" fixtures 0 license notices)
  expect_code(missing-field "${m}")
  manifest_copy(m fetch_empty)
  gbl_json_mutate_set("${m}" "{}" fixtures 0 fetch)
  expect_code(missing-field "${m}")
  manifest_copy(m fixtures_removed)
  gbl_json_mutate_remove("${m}" fixtures)
  expect_code(missing-field "${m}")

elseif(CASE STREQUAL "wrong_type")
  manifest_copy(m fixtures)
  gbl_json_mutate_set("${m}" "{}" fixtures)
  expect_code(wrong-type "${m}")
  manifest_copy(m notices)
  gbl_json_mutate_set("${m}" "\"x\"" fixtures 0 license notices)
  expect_code(wrong-type "${m}")
  manifest_copy(m notice_item)
  gbl_json_mutate_set("${m}" "\"\"" fixtures 0 license notices 0)
  expect_code(wrong-type "${m}")
  manifest_copy(m use)
  gbl_json_mutate_set("${m}" "\"local-only\"" fixtures 0 permitted_use)
  expect_code(wrong-type "${m}")
  manifest_copy(m toolchain_sha)
  gbl_json_mutate_set("${m}" "5" fixtures 0 build toolchain sha256)
  expect_code(wrong-type "${m}")
  manifest_copy(m source_string)
  gbl_json_mutate_set("${m}" "\"x\"" fixtures 0 source)
  expect_code(wrong-type "${m}")

elseif(CASE STREQUAL "duplicate_id")
  manifest_copy(m dup)
  append_row("${m}" 1)
  expect_code(duplicate-id "${m}")

elseif(CASE STREQUAL "bad_id")
  set(i 0)
  foreach(v IN ITEMS "Example Row" "-x")
    manifest_copy(m id${i})
    gbl_json_mutate_set("${m}" "\"${v}\"" fixtures 0 id)
    expect_code(bad-id "${m}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_source_kind")
  set(i 0)
  foreach(v IN ITEMS "svn" "GIT")
    manifest_copy(m kind${i})
    gbl_json_mutate_set("${m}" "\"${v}\"" fixtures 0 source kind)
    expect_code(bad-source-kind "${m}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "missing_digest")
  # A url row with no digest, an in-tree row with a 63-hex digest, and a short toolchain digest.
  sha(short63 "a" 63)
  manifest_copy(m url_no_digest)
  set_source("${m}" 0 [=[{"kind": "url", "url": "https://example.invalid/a.dat"}]=])
  expect_code(missing-digest "${m}")
  manifest_copy(m tree_short)
  set_source("${m}" 0
    "{\"kind\": \"in-tree\", \"path\": \"tests/fixtures/x.dat\", \"sha256\": \"${short63}\"}")
  expect_code(missing-digest "${m}")
  manifest_copy(m toolchain_short)
  gbl_json_mutate_set("${m}" "\"abc\"" fixtures 0 build toolchain sha256)
  expect_code(missing-digest "${m}")

elseif(CASE STREQUAL "bad_url")
  set(i 0)
  foreach(v IN ITEMS "http://example.invalid/r.git" "file:///r.git"
      "ssh://example.invalid/r.git" "https://example.invalid/a b")
    manifest_copy(m url${i})
    gbl_json_mutate_set("${m}" "\"${v}\"" fixtures 0 source url)
    expect_code(bad-url "${m}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_path")
  sha(good64 "ab" 32)
  set(i 0)
  foreach(v IN ITEMS "../x" "/abs" "a//b")
    manifest_copy(m path${i})
    set_source("${m}" 0
      "{\"kind\": \"in-tree\", \"path\": \"${v}\", \"sha256\": \"${good64}\"}")
    expect_code(bad-path "${m}")
    math(EXPR i "${i}+1")
  endforeach()
  manifest_copy(m pin_dest)
  gbl_json_mutate_set("${m}" [=[{"pin_dest": "../escape"}]=] fixtures 0 fetch)
  expect_code(bad-path "${m}")

elseif(CASE STREQUAL "bad_provenance_class")
  set(i 0)
  foreach(v IN ITEMS "emulator-agreement" "Project-Authored")
    manifest_copy(m class${i})
    gbl_json_mutate_set("${m}" "\"${v}\"" fixtures 0 expected provenance_class)
    expect_code(bad-provenance-class "${m}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_bios_requirement")
  set(i 0)
  foreach(v IN ITEMS "official" "hle:CpuSet")
    manifest_copy(m bios${i})
    gbl_json_mutate_set("${m}" "\"${v}\"" fixtures 0 bios_requirement)
    expect_code(bad-bios-requirement "${m}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_permitted_use")
  set(i 0)
  foreach(v IN ITEMS "[]" "[\"public\"]" "[\"local-only\", \"local-only\"]")
    manifest_copy(m use${i})
    gbl_json_mutate_set("${m}" "${v}" fixtures 0 permitted_use)
    expect_code(bad-permitted-use "${m}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_redistributable")
  manifest_copy(m maybe)
  gbl_json_mutate_set("${m}" "\"maybe\"" fixtures 0 license redistributable)
  expect_code(bad-redistributable "${m}")
  manifest_copy(m boolean)
  gbl_json_mutate_set("${m}" "true" fixtures 0 license redistributable)
  expect_code(bad-redistributable "${m}")

elseif(CASE STREQUAL "dispute_unknown")
  manifest_copy(m unknown)
  gbl_json_mutate_set("${m}" "\"disputed\"" fixtures 0 expected provenance_class)
  gbl_json_mutate_set("${m}" "\"disp-nope\"" fixtures 0 expected dispute)
  expect_code(dispute-unknown "${m}")

elseif(CASE STREQUAL "dispute_class_mismatch")
  # Class disputed with no dispute; a registry dispute under a non-disputed class.
  manifest_copy(m no_dispute)
  gbl_json_mutate_set("${m}" "\"disputed\"" fixtures 0 expected provenance_class)
  expect_code(dispute-class-mismatch "${m}")
  manifest_copy(m wrong_class)
  gbl_json_mutate_set("${m}" "\"disp-dma-latch\"" fixtures 0 expected dispute)
  expect_code(dispute-class-mismatch "${m}")

elseif(CASE STREQUAL "pin_mismatch")
  sha(digest_a "ab" 32)
  sha(digest_b "cd" 32)
  # A pin_dest that pins.txt does not hold.
  manifest_copy(m absent)
  set_source("${m}" 0
    "{\"kind\": \"url\", \"url\": \"https://example.invalid/a.dat\", \"sha256\": \"${digest_a}\"}")
  gbl_json_mutate_set("${m}" [=[{"pin_dest": "example/a.dat"}]=] fixtures 0 fetch)
  all_command(cmd "${m}" "${PINS_COPY}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "manifest: pin-mismatch")
  # A url row whose digest differs from its pin.
  manifest_copy(m digest)
  set_source("${m}" 0
    "{\"kind\": \"url\", \"url\": \"https://example.invalid/a.dat\", \"sha256\": \"${digest_a}\"}")
  gbl_json_mutate_set("${m}" [=[{"pin_dest": "example/a.dat"}]=] fixtures 0 fetch)
  pins_copy(p digest "${digest_b} https://example.invalid/a.dat example/a.dat\n")
  all_command(cmd "${m}" "${p}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "manifest: pin-mismatch")
  # A git row whose pinned URL does not contain its commit (a branch URL).
  manifest_copy(m branch_url)
  gbl_json_mutate_set("${m}" [=[{"pin_dest": "example/suite.dat"}]=] fixtures 0 fetch)
  pins_copy(p branch_url "${digest_a} https://example.invalid/raw/main/suite.dat example/suite.dat\n")
  all_command(cmd "${m}" "${p}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "manifest: pin-mismatch")
  # An in-tree row must not carry a fetch.
  manifest_copy(m tree_fetch)
  set_source("${m}" 0
    "{\"kind\": \"in-tree\", \"path\": \"tests/fixtures/x.dat\", \"sha256\": \"${digest_a}\"}")
  gbl_json_mutate_set("${m}" [=[{"pin_dest": "example/x.dat"}]=] fixtures 0 fetch)
  pins_copy(p tree_fetch "${digest_a} https://example.invalid/x.dat example/x.dat\n")
  all_command(cmd "${m}" "${p}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "manifest: pin-mismatch")

elseif(CASE STREQUAL "empty")
  manifest_command(cmd manifest "${dir}/empty.json")
  gbl_expect_pass(COMMAND ${cmd} REGEX "manifest: ok \\(0 rows\\)")

elseif(CASE STREQUAL "source_kinds")
  # A git row (the example), a url row with a matching pin, and an in-tree row.
  sha(digest_a "ab" 32)
  manifest_copy(m kinds)
  append_row("${m}" 1)
  gbl_json_mutate_set("${m}" "\"url-row\"" fixtures 1 id)
  set_source("${m}" 1
    "{\"kind\": \"url\", \"url\": \"https://example.invalid/a.dat\", \"sha256\": \"${digest_a}\"}")
  gbl_json_mutate_set("${m}" [=[{"pin_dest": "example/a.dat"}]=] fixtures 1 fetch)
  append_row("${m}" 2)
  gbl_json_mutate_set("${m}" "\"tree-row\"" fixtures 2 id)
  set_source("${m}" 2
    "{\"kind\": \"in-tree\", \"path\": \"tests/fixtures/tree.dat\", \"sha256\": \"${digest_a}\"}")
  pins_copy(p kinds "${digest_a} https://example.invalid/a.dat example/a.dat\n")
  all_command(cmd "${m}" "${p}")
  gbl_expect_pass(COMMAND ${cmd} REGEX "manifest: ok \\(3 rows\\)")
  gbl_expect_pass(COMMAND ${cmd} REGEX "pins: ok \\(1 rows\\)")

else()
  message(FATAL_ERROR "manifest-case: unknown case: ${CASE}")
endif()
