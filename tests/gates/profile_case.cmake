# Profile negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/profile_case.cmake
#
# Every input is a mutated COPY of profiles/agb.json or docs/profile-agb.md (with
# copies of registry/disputes.json and contracts/evidence.json) under
# BIN/nc/profile_<case>; the committed files are never modified and no bad example
# is committed (D-04). `copy` proves the unmutated copies are accepted, so every
# negative case fails only for its planted reason. Each negative variant asserts a
# non-zero exit AND the validator's stable reason code through
# cmake/ExpectFail.cmake; a stale mutation fails with `mutate: no-match`. A variant
# changes one field; co-reported findings are accepted because the REGEX asserts
# that the targeted code line is present.

# Script mode starts with every policy OLD; pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "profile-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "profile-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")
include("${SRC}/tests/gates/json_mutate.cmake")

set(dir "${BIN}/nc/profile_${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(CHECK "${SRC}/cmake/CheckProfile.cmake")
file(COPY_FILE "${SRC}/contracts/evidence.json" "${dir}/evidence.json")
file(COPY_FILE "${SRC}/registry/disputes.json" "${dir}/disputes.json")
set(VOCAB_COPY "${dir}/evidence.json")
set(REGISTRY_COPY "${dir}/disputes.json")

# profile_copy(<out-var> <variant>): a fresh sandbox copy of the real profile.
function(profile_copy out variant)
  set(path "${dir}/profile_${variant}.json")
  file(COPY_FILE "${SRC}/profiles/agb.json" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# doc_copy(<out-var> <variant>): a fresh sandbox copy of the real document.
function(doc_copy out variant)
  set(path "${dir}/doc_${variant}.md")
  file(COPY_FILE "${SRC}/docs/profile-agb.md" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

# doc_drop_row(<file> <id>): delete the table row whose first backticked token is
# <id>; a stale anchor fails with `mutate: no-match`.
function(doc_drop_row file id)
  file(READ "${file}" content)
  string(REGEX REPLACE "\n\\| `${id}`[^\n]*" "" after "${content}")
  if("${after}" STREQUAL "${content}")
    message(FATAL_ERROR "mutate: no-match: ${file}")
  endif()
  file(WRITE "${file}" "${after}")
endfunction()

# check_command(<out-var> <profile> <doc>): the validator command line.
function(check_command out profile doc)
  set(${out}
    "${CMAKE_COMMAND}" "-DPROFILE=${profile}" "-DDOC=${doc}"
    "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    PARENT_SCOPE)
endfunction()

# expect_code(<code> <profile> [<doc>]): the validator fails with <code>. The
# document defaults to an unmodified copy.
function(expect_code code profile)
  if(ARGC GREATER 2)
    set(doc "${ARGV2}")
  else()
    doc_copy(doc unchanged_${code})
  endif()
  check_command(cmd "${profile}" "${doc}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "profile: ${code}")
endfunction()

# expect_detail(<code> <detail-regex> <profile>): fails with <code> and that finding
# detail (findings are printed unwrapped). One variant per invariant or trigger: a bare
# code can come from another check, so the control names the detail it targets.
function(expect_detail code detail profile)
  doc_copy(doc unchanged_detail)
  check_command(cmd "${profile}" "${doc}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "profile: ${code}: ${detail}")
endfunction()

if(CASE STREQUAL "copy")
  profile_copy(p copy)
  doc_copy(d copy)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v2, 11 timing constants, 11 unmodeled\\)")

elseif(CASE STREQUAL "bus_policy_missing")
  profile_copy(p missing_bus_policy)
  gbl_json_mutate_remove("${p}" bus_policy)
  expect_code(bus-policy-missing "${p}")

elseif(CASE STREQUAL "unmodeled_missing")
  # Remove each of the nine required entries in turn from a fresh copy; each
  # removal names its own id.
  file(READ "${SRC}/profiles/agb.json" real)
  string(JSON n LENGTH "${real}" unmodeled)
  if(NOT n EQUAL 9)
    message(FATAL_ERROR "profile-case: expected 9 unmodeled entries, found ${n}")
  endif()
  math(EXPR last "${n}-1")
  foreach(i RANGE ${last})
    string(JSON id GET "${real}" unmodeled ${i} id)
    profile_copy(p missing${i})
    gbl_json_mutate_remove("${p}" unmodeled ${i})
    expect_code("unmodeled-missing: ${id}" "${p}")
  endforeach()

elseif(CASE STREQUAL "usage")
  doc_copy(d usage)
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" "-DDOC=${d}" "-DREGISTRY=${REGISTRY_COPY}"
      "-DVOCAB=${VOCAB_COPY}" -P "${CHECK}"
    REGEX "profile: usage")

elseif(CASE STREQUAL "bad_schema")
  profile_copy(p schema)
  gbl_json_mutate_set("${p}" "\"gamebloke.profile/2\"" schema)
  expect_code(bad-schema "${p}")

elseif(CASE STREQUAL "bad_version")
  # Zero, negative, fractional, and a string are all outside a positive integer.
  set(i 0)
  foreach(v IN ITEMS "0" "-1" "1.5" "\"1\"")
    profile_copy(p version${i})
    gbl_json_mutate_set("${p}" "${v}" profile_version)
    expect_code(bad-version "${p}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "missing_field")
  # Each of the nine required top-level fields, then one timing key.
  set(i 0)
  foreach(field IN ITEMS
      schema profile_version id machine boot bios_provider cart timing unmodeled)
    profile_copy(p field${i})
    gbl_json_mutate_remove("${p}" ${field})
    expect_code(missing-field "${p}")
    math(EXPR i "${i}+1")
  endforeach()
  profile_copy(p timing_key)
  gbl_json_mutate_remove("${p}" timing cycles_per_line)
  expect_code(missing-field "${p}")

elseif(CASE STREQUAL "bad_machine")
  profile_copy(p machine)
  gbl_json_mutate_set("${p}" "\"agb-micro\"" machine)
  expect_code(bad-machine "${p}")
  profile_copy(p id)
  gbl_json_mutate_set("${p}" "\"ags\"" id)
  expect_code(bad-machine "${p}")

elseif(CASE STREQUAL "bad_boot")
  profile_copy(p boot)
  gbl_json_mutate_set("${p}" "\"bios\"" boot)
  expect_code(bad-boot "${p}")

elseif(CASE STREQUAL "bad_bios_provider")
  # Empty and malformed hle lists, a branch name where a commit is required, a
  # short digest, a retired milestone label, and a wrong-case keyword.
  set(i 0)
  foreach(v IN ITEMS
      "hle:" "hle:a,,b" "hle:a," "replacement:x@main" "user-official@abc" "M0" "NONE")
    profile_copy(p bios${i})
    gbl_json_mutate_set("${p}" "\"${v}\"" bios_provider)
    expect_code(bad-bios-provider "${p}")
    math(EXPR i "${i}+1")
  endforeach()

elseif(CASE STREQUAL "bad_cart")
  profile_copy(p kind)
  gbl_json_mutate_set("${p}" "\"flash\"" cart kind)
  expect_code(bad-cart "${p}")
  profile_copy(p string)
  gbl_json_mutate_set("${p}" "\"rom-only\"" cart)
  expect_code(bad-cart "${p}")

elseif(CASE STREQUAL "missing_source")
  profile_copy(p source)
  gbl_json_mutate_remove("${p}" timing dots_visible source)
  expect_code(missing-source "${p}")
  profile_copy(p cite)
  gbl_json_mutate_remove("${p}" timing lines_visible cite)
  expect_code(missing-source "${p}")

elseif(CASE STREQUAL "bad_source_label")
  # A reserved later-phase label, a lower-case variant, and two labels in one string.
  set(i 0)
  foreach(v IN ITEMS "calibrated:arm-basic" "ref" "REF,SRC")
    profile_copy(p label${i})
    gbl_json_mutate_set("${p}" "\"${v}\"" timing master_clock_hz source)
    expect_code(bad-source-label "${p}")
    math(EXPR i "${i}+1")
  endforeach()
  # A label that claims a source over a null value is refused, for an ordinary constant
  # and for the disputed edge (only UNK may stand over no value).
  profile_copy(p null_value)
  gbl_json_mutate_set("${p}" "null" timing master_clock_hz value)
  expect_detail(bad-source-label "timing.master_clock_hz: null value under REF" "${p}")
  profile_copy(p null_edge)
  gbl_json_mutate_set("${p}" "\"REF\"" timing hblank_flag_edge_cycle source)
  expect_detail(bad-source-label "timing.hblank_flag_edge_cycle: null value under REF" "${p}")

elseif(CASE STREQUAL "timing_arithmetic")
  # One variant per invariant, each changing ONE input so that exactly that invariant
  # breaks, with the finding detail asserted (deleting any one invariant fails here).
  # Real values: 240 + 68 = 308 dots; 308 * 4 = 1232 cycles/line; 160 + 68 = 228 lines;
  # 1232 * 228 = 280896 cycles/frame; clock 2^24.
  profile_copy(p dots)
  gbl_json_mutate_set("${p}" "241" timing dots_visible value)
  expect_detail(timing-arithmetic "dots_visible \\+ dots_hblank != dots_per_line" "${p}")
  profile_copy(p cycles_dot)
  gbl_json_mutate_set("${p}" "5" timing cycles_per_dot value)
  expect_detail(timing-arithmetic "dots_per_line \\* cycles_per_dot != cycles_per_line" "${p}")
  profile_copy(p lines)
  gbl_json_mutate_set("${p}" "161" timing lines_visible value)
  expect_detail(timing-arithmetic "lines_visible \\+ lines_vblank != lines_per_frame" "${p}")
  profile_copy(p frame)
  gbl_json_mutate_set("${p}" "280897" timing cycles_per_frame value)
  expect_detail(timing-arithmetic "cycles_per_line \\* lines_per_frame != cycles_per_frame" "${p}")
  profile_copy(p clock)
  gbl_json_mutate_set("${p}" "16780000" timing master_clock_hz value)
  expect_detail(timing-arithmetic "master_clock_hz != 2\\^24" "${p}")
  # The pre-existing mixed variants: one edit that breaks two invariants.
  profile_copy(p line)
  gbl_json_mutate_set("${p}" "1231" timing cycles_per_line value)
  expect_code(timing-arithmetic "${p}")
  profile_copy(p vblank)
  gbl_json_mutate_set("${p}" "67" timing lines_vblank value)
  expect_code(timing-arithmetic "${p}")

elseif(CASE STREQUAL "hblank_edge_hardcoded")
  # 960 is the visible drawing time; 1006 is one of the disputed edge values.
  foreach(v IN ITEMS 960 1006)
    profile_copy(p edge${v})
    gbl_json_mutate_set("${p}" "${v}" timing hblank_flag_edge_cycle value)
    expect_code(hblank-edge-hardcoded "${p}")
  endforeach()

elseif(CASE STREQUAL "unmodeled_bad_entry")
  # prefetch is entry 1, dma entry 5, sp-micro-differences entry 0.
  profile_copy(p status)
  gbl_json_mutate_set("${p}" "\"todo\"" unmodeled 1 status)
  expect_code(unmodeled-bad-entry "${p}")
  profile_copy(p reason)
  gbl_json_mutate_set("${p}" "\"\"" unmodeled 1 reason)
  expect_code(unmodeled-bad-entry "${p}")
  profile_copy(p duplicate)
  gbl_json_mutate_set("${p}"
    [=[{"id": "dma", "status": "unmodeled", "reason": "A duplicated entry used only by this control."}]=]
    unmodeled 9)
  expect_code(unmodeled-bad-entry "${p}")
  profile_copy(p details)
  gbl_json_mutate_set("${p}" [=["The Micro has no cartridge switch."]=] unmodeled 0 details 0)
  expect_code(unmodeled-bad-entry "${p}")
  # hblank-flag-edge (entry 2) must carry disp-hblank-flag-edge, not another dispute id
  # (disp-dma-latch is a registry id, so no dispute-unknown finding can stand in for it).
  profile_copy(p hblank_dispute)
  gbl_json_mutate_set("${p}" "\"disp-dma-latch\"" unmodeled 2 dispute)
  expect_detail(unmodeled-bad-entry "hblank-flag-edge: dispute" "${p}")

elseif(CASE STREQUAL "dispute_unknown")
  # An id the registry does not hold, on the unmodeled entry and on the timing entry.
  profile_copy(p unmodeled)
  gbl_json_mutate_set("${p}" "\"disp-nope\"" unmodeled 2 dispute)
  expect_code(dispute-unknown "${p}")
  profile_copy(p timing)
  gbl_json_mutate_set("${p}" "\"disp-nope\"" timing hblank_flag_edge_cycle dispute)
  expect_code(dispute-unknown "${p}")

elseif(CASE STREQUAL "doc_missing_id")
  profile_copy(p missing)
  doc_copy(d missing)
  doc_drop_row("${d}" prefetch)
  expect_code(doc-missing-id "${p}" "${d}")

elseif(CASE STREQUAL "doc_extra_id")
  profile_copy(p extra)
  doc_copy(d extra)
  gbl_text_mutate("${d}" "| `backup-media` |"
    "| `cgb-mode` | A row used only by this control. |\n| `backup-media` |")
  expect_code(doc-extra-id "${p}" "${d}")

elseif(CASE STREQUAL "doc_version_mismatch")
  profile_copy(p bumped)
  doc_copy(d bumped)
  gbl_text_mutate("${d}" "\nprofile_version: 1\n" "\nprofile_version: 2\n")
  expect_code(doc-version-mismatch "${p}" "${d}")
  profile_copy(p deleted)
  doc_copy(d deleted)
  gbl_text_mutate("${d}" "\nprofile_version: 1\n" "\n")
  expect_code(doc-version-mismatch "${p}" "${d}")

elseif(CASE STREQUAL "doc_no_section")
  # A renamed heading, then the right heading with every table row removed: an
  # empty parse is a failure, not a pass.
  profile_copy(p renamed)
  doc_copy(d renamed)
  gbl_text_mutate("${d}" "\n## Unmodeled\n" "\n## Not modeled\n")
  expect_code(doc-no-section "${p}" "${d}")
  profile_copy(p rows)
  doc_copy(d rows)
  file(READ "${d}" content)
  string(FIND "${content}" "| Id | What" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "mutate: no-match: ${d}")
  endif()
  string(SUBSTRING "${content}" 0 ${at} head)
  file(WRITE "${d}" "${head}")
  expect_code(doc-no-section "${p}" "${d}")

elseif(CASE STREQUAL "coordinated_change")
  # A new unmodeled entry and a version bump, made in both files, is accepted.
  profile_copy(p coordinated)
  doc_copy(d coordinated)
  gbl_json_mutate_set("${p}"
    [=[{"id": "serial-link", "status": "unmodeled", "reason": "Not emulated in v1.0; no result may claim serial link behavior."}]=]
    unmodeled 9)
  gbl_json_mutate_set("${p}" "2" profile_version)
  gbl_text_mutate("${d}" "| `backup-media` |"
    "| `serial-link` | The serial link port. |\n| `backup-media` |")
  gbl_text_mutate("${d}" "\nprofile_version: 1\n" "\nprofile_version: 2\n")
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v2, 11 timing constants, 10 unmodeled\\)")

else()
  message(FATAL_ERROR "profile-case: unknown case: ${CASE}")
endif()
