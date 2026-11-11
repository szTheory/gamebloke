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
file(READ "${SRC}/profiles/agb.json" base_profile)
string(JSON PROFILE_VERSION GET "${base_profile}" profile_version)
math(EXPR NEXT_PROFILE_VERSION "${PROFILE_VERSION}+1")

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
  if(ARGC GREATER 3)
    set(doc "${ARGV3}")
  else()
    doc_copy(doc unchanged_detail)
  endif()
  check_command(cmd "${profile}" "${doc}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "profile: ${code}: ${detail}")
endfunction()

if(CASE STREQUAL "copy")
  profile_copy(p copy)
  doc_copy(d copy)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, 11 unmodeled\\)")

elseif(CASE STREQUAL "cpu_policy_value")
  profile_copy(p positive_cpu_policy)
  doc_copy(d positive_cpu_policy)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, 11 unmodeled\\)")

  profile_copy(p missing_cpu_policy)
  gbl_json_mutate_remove("${p}" cpu_policy)
  expect_code(cpu-policy-missing "${p}")

  profile_copy(p wrong_entry)
  gbl_json_mutate_set("${p}" "134217732" cpu_policy entry_address)
  expect_detail(cpu-policy-value "cpu_policy\\.entry_address: expected 134217728" "${p}")

  profile_copy(p wrong_mode)
  gbl_json_mutate_set("${p}" "211" cpu_policy cpsr)
  expect_detail(cpu-policy-value "cpu_policy\\.cpsr: expected 223" "${p}")

  profile_copy(p wrong_flags)
  gbl_json_mutate_set("${p}" "2147483871" cpu_policy cpsr)
  expect_detail(cpu-policy-value "cpu_policy\\.cpsr: expected 223" "${p}")

  profile_copy(p wrong_arm_pc)
  gbl_json_mutate_set("${p}" "4" cpu_policy arm_pc_offset)
  expect_detail(cpu-policy-value "cpu_policy\\.arm_pc_offset: expected 8" "${p}")

  profile_copy(p fake_user_spsr)
  gbl_json_mutate_set("${p}" "\"present\"" cpu_policy user_system_spsr)
  expect_detail(cpu-policy-value "cpu_policy\\.user_system_spsr: expected absent" "${p}")

  profile_copy(p wrong_stop_edge)
  gbl_json_mutate_set("${p}" "\"after-vector-fetch\"" cpu_policy bios_none_stop)
  expect_detail(cpu-policy-value "cpu_policy\\.bios_none_stop: expected after-entry-before-vector-fetch" "${p}")

  profile_copy(p inflated_policy_source)
  gbl_json_mutate_set("${p}" "\"REF\"" cpu_policy source)
  expect_code(cpu-policy-source "${p}")

  profile_copy(p inflated_arch_source)
  gbl_json_mutate_set("${p}" "\"SRC\"" cpu_policy architecture_source)
  expect_code(cpu-policy-source "${p}")

  profile_copy(p omitted_supported_rows)
  gbl_json_mutate_set("${p}" "[]" cpu_policy supported_opcode_operands)
  expect_code(cpu-policy-value "${p}")

  profile_copy(p excess_supported_rows)
  gbl_text_mutate("${p}"
    "\"Thumb transfer: PC-relative word LDR at aligned(current+4), plus immediate word/byte LDR/STR using low registers\"\n    ],"
    "\"Thumb transfer: PC-relative word LDR at aligned(current+4), plus immediate word/byte LDR/STR using low registers\",\n      \"Extra instruction family not implemented\"\n    ],")
  expect_code(cpu-policy-value "${p}")

  profile_copy(p omitted_excluded_rows)
  gbl_json_mutate_set("${p}" "[]" cpu_policy excluded_opcode_operands)
  expect_code(cpu-policy-value "${p}")

elseif(CASE STREQUAL "cpu_policy_doc")
  profile_copy(p positive_cpu_policy_doc)
  doc_copy(d positive_cpu_policy_doc)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, 11 unmodeled\\)")

  set(entry_row "| cpu_policy.entry_address | 134217728 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |")
  profile_copy(p stale_cpu_value)
  doc_copy(d stale_cpu_value)
  gbl_text_mutate("${d}" "${entry_row}"
    "| cpu_policy.entry_address | 134217732 | IMPL | Gamebloke deterministic project policy: direct-start CPU initialization, one preflighted interpreter path, and explicit BIOS-none exception stops. |")
  expect_code(cpu-policy-doc "${p}" "${d}")

  profile_copy(p duplicated_cpu_row)
  doc_copy(d duplicated_cpu_row)
  gbl_text_mutate("${d}" "${entry_row}" "${entry_row}\n${entry_row}")
  expect_code(cpu-policy-doc "${p}" "${d}")

  profile_copy(p hidden_cpu_row)
  doc_copy(d hidden_cpu_row)
  gbl_text_mutate("${d}" "${entry_row}"
    "<!--\n${entry_row}\n-->\n```markdown\n${entry_row}\n```")
  expect_code(cpu-policy-doc "${p}" "${d}")

  profile_copy(p inflated_cpu_doc_source)
  doc_copy(d inflated_cpu_doc_source)
  gbl_text_mutate("${d}" "| cpu_policy.entry_address | 134217728 | IMPL |"
    "| cpu_policy.entry_address | 134217728 | REF |")
  expect_code(cpu-policy-doc "${p}" "${d}")

  profile_copy(p stale_cpu_scope)
  doc_copy(d stale_cpu_scope)
  gbl_text_mutate("${d}" "ARM control flow: B, BL, BX"
    "ARM control flow: B, BL, BX, and extra opcode")
  expect_code(cpu-policy-doc "${p}" "${d}")

elseif(CASE STREQUAL "bus_policy_missing")
  profile_copy(p missing_bus_policy)
  gbl_json_mutate_remove("${p}" bus_policy)
  expect_code(bus-policy-missing "${p}")

elseif(CASE STREQUAL "bus_policy_value")
  profile_copy(p request_cost)
  gbl_json_mutate_set("${p}" "2" bus_policy request_cycles rom)
  expect_detail(bus-policy-value "bus_policy\\.request_cycles\\.rom: expected 1" "${p}")
  profile_copy(p widths)
  gbl_json_mutate_set("${p}" "[1, 2]" bus_policy widths_bytes)
  expect_detail(bus-policy-value "bus_policy\\.widths_bytes: expected \\[1, 2, 4\\]" "${p}")
  profile_copy(p classes)
  gbl_json_mutate_set("${p}" "[\"nonsequential\"]" bus_policy access_classes)
  expect_detail(bus-policy-value "bus_policy\\.access_classes: expected \\[nonsequential, sequential\\]" "${p}")
  profile_copy(p internal_unit)
  gbl_json_mutate_set("${p}" "2" bus_policy internal_cycle_unit)
  expect_detail(bus-policy-value "bus_policy\\.internal_cycle_unit: expected 1" "${p}")

elseif(CASE STREQUAL "bus_policy_source")
  profile_copy(p bad_label)
  gbl_json_mutate_set("${p}" "\"REF\"" bus_policy source)
  expect_code(bus-policy-source "${p}")
  profile_copy(p empty_citation)
  gbl_json_mutate_set("${p}" "\"\"" bus_policy cite)
  expect_code(bus-policy-source "${p}")

elseif(CASE STREQUAL "bus_policy_doc")
  foreach(shape IN ITEMS left right neither hash)
    profile_copy(p unwrapped_${shape})
    doc_copy(d unwrapped_${shape})
    set(row "bus_policy.request_cycles.extra | 1 | IMPL | test-only row")
    if(shape STREQUAL "left")
      set(row "| ${row}")
    elseif(shape STREQUAL "right")
      set(row "${row} |")
    elseif(shape STREQUAL "hash")
      set(row "#${row}")
    endif()
    gbl_text_mutate("${d}"
      "| bus_policy.internal_cycle_unit | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |"
      "| bus_policy.internal_cycle_unit | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |\n${row}")
    expect_detail(bus-policy-doc "bus_policy: missing policy table" "${p}" "${d}")
  endforeach()
  profile_copy(p stale_value)
  doc_copy(d stale_value)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |"
    "| bus_policy.request_cycles.rom | 2 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |")
  expect_detail(bus-policy-doc "bus_policy: missing or stale row" "${p}" "${d}")
  # A complete but hidden copy must not stand in for the stale rendered table.
  profile_copy(p commented_stale_value)
  doc_copy(d commented_stale_value)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |"
    "| bus_policy.request_cycles.rom | 2 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |")
  file(READ "${SRC}/docs/profile-agb.md" source_doc)
  string(FIND "${source_doc}" "| Setting | Value | Label | Citation |" table_start)
  string(FIND "${source_doc}" "\n## Initialization policy" table_end)
  if(table_start EQUAL -1 OR table_end EQUAL -1 OR table_end LESS table_start)
    message(FATAL_ERROR "mutate: no-match: source bus policy table")
  endif()
  math(EXPR table_length "${table_end}-${table_start}")
  string(SUBSTRING "${source_doc}" ${table_start} ${table_length} valid_table)
  gbl_text_mutate("${d}" "| Setting | Value | Label | Citation |"
    "<!--\n${valid_table}\n-->\n| Setting | Value | Label | Citation |")
  expect_detail(bus-policy-doc "bus_policy: missing or stale row" "${p}" "${d}")
  profile_copy(p raw_html_table)
  doc_copy(d raw_html_table)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |"
    "| bus_policy.request_cycles.rom | 2 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |")
  file(READ "${SRC}/docs/profile-agb.md" source_doc)
  string(FIND "${source_doc}" "| Setting | Value | Label | Citation |" table_start)
  string(FIND "${source_doc}" "\n## Initialization policy" table_end)
  if(table_start EQUAL -1 OR table_end EQUAL -1 OR table_end LESS table_start)
    message(FATAL_ERROR "mutate: no-match: source bus policy table")
  endif()
  math(EXPR table_length "${table_end}-${table_start}")
  string(SUBSTRING "${source_doc}" ${table_start} ${table_length} valid_table)
  gbl_text_mutate("${d}" "| Setting | Value | Label | Citation |"
    "<pre>\n${valid_table}\n</pre>\n| Setting | Value | Label | Citation |")
  expect_detail(bus-policy-doc
    "unsupported raw HTML element <pre>" "${p}" "${d}")
  profile_copy(p hidden_bus_heading)
  doc_copy(d hidden_bus_heading)
  gbl_text_mutate("${d}" "\n## Bus policy\n"
    "\n<!--\n## Bus policy\n-->\n")
  expect_detail(bus-policy-doc "missing ## Bus policy section" "${p}" "${d}")
  profile_copy(p multiline_inline_hidden_version)
  doc_copy(d multiline_inline_hidden_version)
  gbl_text_mutate("${d}" "\nprofile_version: ${PROFILE_VERSION}\n"
    "\n``\nprofile_version: ${PROFILE_VERSION}\n``\nprofile_version: ${NEXT_PROFILE_VERSION}\n")
  expect_detail(doc-version-mismatch "document ${NEXT_PROFILE_VERSION}, profile ${PROFILE_VERSION}" "${p}" "${d}")
  profile_copy(p html_crosses_policy_sections)
  doc_copy(d html_crosses_policy_sections)
  gbl_text_mutate("${d}" "\n## Bus policy\n"
    "\n<pre>\n## Bus policy\n")
  file(APPEND "${d}" "\n<!-- </pre> -->\n")
  expect_detail(bus-policy-doc "unsupported raw HTML element <pre>" "${p}" "${d}")
  profile_copy(p raw_html_section)
  doc_copy(d raw_html_section)
  gbl_text_mutate("${d}" "## Bus policy\n"
    "<section>\n## Bus policy\n")
  file(APPEND "${d}" "\n</section>\n")
  expect_detail(bus-policy-doc
    "unsupported raw HTML element <section>" "${p}" "${d}")
  profile_copy(p raw_html_custom_element)
  doc_copy(d raw_html_custom_element)
  gbl_text_mutate("${d}" "## Bus policy\n"
    "<gamebloke-profile>\n## Bus policy\n")
  file(APPEND "${d}" "\n</gamebloke-profile>\n")
  expect_detail(bus-policy-doc
    "unsupported raw HTML element <gamebloke-profile>" "${p}" "${d}")
  profile_copy(p raw_html_processing_instruction)
  doc_copy(d raw_html_processing_instruction)
  gbl_text_mutate("${d}" "## Bus policy\n"
    "<?profile\n## Bus policy\n")
  file(APPEND "${d}" "\n?>\n")
  expect_detail(bus-policy-doc
    "unsupported raw HTML declaration or processing instruction" "${p}" "${d}")
  profile_copy(p raw_html_cdata)
  doc_copy(d raw_html_cdata)
  gbl_text_mutate("${d}" "## Bus policy\n"
    "<![CDATA[\n## Bus policy\n")
  file(APPEND "${d}" "\n]]>\n")
  expect_detail(bus-policy-doc
    "unsupported raw HTML declaration or processing instruction" "${p}" "${d}")
  profile_copy(p wrong_policy_header)
  doc_copy(d wrong_policy_header)
  gbl_text_mutate("${d}" "| Setting | Value | Label | Citation |"
    "| Setting | Value | Label | Evidence |")
  expect_detail(bus-policy-doc "bus_policy: missing policy table" "${p}" "${d}")
  profile_copy(p colon_only_policy_delimiter)
  doc_copy(d colon_only_policy_delimiter)
  gbl_text_mutate("${d}" "| ------- | ----- | ----- | -------- |"
    "| : | : | : | : |")
  expect_detail(bus-policy-doc "bus_policy: missing policy table" "${p}" "${d}")
  profile_copy(p missing_policy_hyphen)
  doc_copy(d missing_policy_hyphen)
  gbl_text_mutate("${d}" "| ------- | ----- | ----- | -------- |"
    "| : | ----- | ----- | -------- |")
  expect_detail(bus-policy-doc "bus_policy: missing policy table" "${p}" "${d}")
  profile_copy(p wrong_policy_separator_columns)
  doc_copy(d wrong_policy_separator_columns)
  gbl_text_mutate("${d}" "| ------- | ----- | ----- | -------- |"
    "| ------- | ----- | ----- |")
  expect_detail(bus-policy-doc "bus_policy: missing policy table" "${p}" "${d}")
  profile_copy(p malformed_unmodeled_separator)
  doc_copy(d malformed_unmodeled_separator)
  gbl_text_mutate("${d}" "| -- | ------------------- |" "| : | : |")
  expect_detail(doc-no-section "## Unmodeled" "${p}" "${d}")
  profile_copy(p extra_row)
  doc_copy(d extra_row)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.unmapped | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |"
    "| bus_policy.request_cycles.unmapped | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |\n| bus_policy.request_cycles.extra | 1 | IMPL | test-only row |")
  expect_detail(bus-policy-doc "bus_policy: expected 12 rows, found 13" "${p}" "${d}")
  profile_copy(p unexpected_policy_row)
  doc_copy(d unexpected_policy_row)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.unmapped | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |"
    "| bus_policy.request_cycles.extra | 1 | IMPL | test-only row |")
  expect_detail(bus-policy-doc
    "bus_policy: unexpected policy row bus_policy.request_cycles.extra" "${p}" "${d}")
  profile_copy(p wrong_policy_body_cells)
  doc_copy(d wrong_policy_body_cells)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |"
    "| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. | extra |")
  expect_detail(bus-policy-doc
    "bus_policy: expected 4 cells per body row, found 5" "${p}" "${d}")
  profile_copy(p wrong_unmodeled_body_cells)
  doc_copy(d wrong_unmodeled_body_cells)
  gbl_text_mutate("${d}"
    "| `sp-micro-differences` | SP and Micro differences. Includes the EWRAM wait register at 0x04000800 (its fastest setting is reported to lock up the Micro, REF) and the Micro's missing cartridge switch (REF). |"
    "| `sp-micro-differences` | SP and Micro differences. Includes the EWRAM wait register at 0x04000800 (its fastest setting is reported to lock up the Micro, REF) and the Micro's missing cartridge switch (REF). | extra |")
  expect_detail(doc-no-section "## Unmodeled" "${p}" "${d}")
  profile_copy(p misplaced_unmodeled_id)
  doc_copy(d misplaced_unmodeled_id)
  gbl_text_mutate("${d}"
    "| `sp-micro-differences` | SP and Micro differences. Includes the EWRAM wait register at 0x04000800 (its fastest setting is reported to lock up the Micro, REF) and the Micro's missing cartridge switch (REF). |"
    "| id `sp-micro-differences` | SP and Micro differences. Includes the EWRAM wait register at 0x04000800 (its fastest setting is reported to lock up the Micro, REF) and the Micro's missing cartridge switch (REF). |")
  expect_detail(doc-no-section "## Unmodeled" "${p}" "${d}")
  profile_copy(p section_scope)
  doc_copy(d section_scope)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |"
    "| bus_policy.request_cycles.rom | 2 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |")
  gbl_text_mutate("${d}" "## Machine\n"
    "## Machine\n| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |\n")
  expect_detail(bus-policy-doc "bus_policy: missing or stale row" "${p}" "${d}")
  profile_copy(p same_section_fence)
  doc_copy(d same_section_fence)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |\n"
    "")
  gbl_text_mutate("${d}" "## Initialization policy"
    "```text\n| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |\n```\n\n## Initialization policy")
  expect_detail(bus-policy-doc "bus_policy: expected 12 rows, found 11" "${p}" "${d}")
  profile_copy(p same_section_indented)
  doc_copy(d same_section_indented)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |\n"
    "")
  gbl_text_mutate("${d}" "## Initialization policy"
    "    | bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |\n\t| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |\n\n## Initialization policy")
  expect_detail(bus-policy-doc "bus_policy: expected 12 rows, found 11" "${p}" "${d}")
  profile_copy(p duplicate_key)
  doc_copy(d duplicate_key)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.oam | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |"
    "| bus_policy.request_cycles.rom | 1 | IMPL | Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim. |")
  expect_detail(bus-policy-doc "bus_policy: expected key bus_policy\\.request_cycles\\.rom exactly once in policy table, found 2" "${p}" "${d}")

elseif(CASE STREQUAL "init_policy")
  profile_copy(p missing)
  gbl_json_mutate_remove("${p}" initialization_policy)
  expect_code(init-policy "${p}")
  profile_copy(p structure)
  gbl_json_mutate_set("${p}" "\"zero\"" initialization_policy)
  expect_code(init-policy "${p}")
  foreach(field IN ITEMS memory_fill register_fill master_cycles open_bus)
    profile_copy(p "bad_${field}")
    gbl_json_mutate_set("${p}" "1" initialization_policy ${field})
    expect_code(init-policy "${p}")
  endforeach()
  profile_copy(p bad_source)
  gbl_json_mutate_set("${p}" "\"REF\"" initialization_policy source)
  expect_code(init-policy "${p}")
  profile_copy(p empty_citation)
  gbl_json_mutate_set("${p}" "\"\"" initialization_policy cite)
  expect_code(init-policy "${p}")

elseif(CASE STREQUAL "read_policy")
  profile_copy(p missing)
  gbl_json_mutate_remove("${p}" read_policy)
  expect_code(read-policy "${p}")
  profile_copy(p structure)
  gbl_json_mutate_set("${p}" "\"unverified\"" read_policy)
  expect_code(read-policy "${p}")
  foreach(field IN ITEMS absent_bios unsupported_io rom_tail)
    profile_copy(p "bad_${field}")
    gbl_json_mutate_set("${p}" "\"zero\"" read_policy ${field})
    expect_code(read-policy "${p}")
  endforeach()
  profile_copy(p bad_latch)
  gbl_json_mutate_set("${p}" "\"last-byte\"" read_policy latch_update)
  expect_code(read-policy "${p}")
  profile_copy(p bad_write_latch)
  gbl_json_mutate_set("${p}" "\"update\"" read_policy write_latch)
  expect_code(read-policy "${p}")
  profile_copy(p bad_source)
  gbl_json_mutate_set("${p}" "\"REF\"" read_policy source)
  expect_code(read-policy "${p}")
  profile_copy(p empty_citation)
  gbl_json_mutate_set("${p}" "\"\"" read_policy cite)
  expect_code(read-policy "${p}")

elseif(CASE STREQUAL "bus_policy_copy")
  profile_copy(p bus_policy)
  doc_copy(d bus_policy_fenced_pre)
  gbl_text_mutate("${d}" "This canonical profile document uses plain Markdown only."
    "This canonical profile document uses plain Markdown only. The code span ``<section>`` is literal text. The multiline code example begins with ``\n<section>\n`` and is ignored.")
  file(READ "${SRC}/docs/profile-agb.md" source_doc)
  string(FIND "${source_doc}" "| Setting | Value | Label | Citation |" table_start)
  string(FIND "${source_doc}" "\n## Initialization policy" table_end)
  if(table_start EQUAL -1 OR table_end EQUAL -1 OR table_end LESS table_start)
    message(FATAL_ERROR "mutate: no-match: source bus policy table")
  endif()
  math(EXPR table_length "${table_end}-${table_start}")
  string(SUBSTRING "${source_doc}" ${table_start} ${table_length} valid_table)
  gbl_text_mutate("${d}" "## Bus policy\n"
    "## Bus policy\n\n```html\n<pre>\n${valid_table}\n</pre>\n```\n")
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, 11 unmodeled\\)")

elseif(CASE STREQUAL "unmodeled_missing")
  # Remove each of the eleven required entries in turn from a fresh copy; each
  # removal names its own id.
  file(READ "${SRC}/profiles/agb.json" real)
  string(JSON n LENGTH "${real}" unmodeled)
  if(NOT n EQUAL 11)
    message(FATAL_ERROR "profile-case: expected 11 unmodeled entries, found ${n}")
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
  # Signed-bound operands are accepted only when each operation also fits.
  profile_copy(p product_overflow)
  gbl_json_mutate_set("${p}" "4611686018427387904" timing dots_visible value)
  gbl_json_mutate_set("${p}" "0" timing dots_hblank value)
  gbl_json_mutate_set("${p}" "4611686018427387904" timing dots_per_line value)
  gbl_json_mutate_set("${p}" "0" timing cycles_per_line value)
  gbl_json_mutate_set("${p}" "0" timing cycles_per_frame value)
  expect_detail(timing-arithmetic
    "dots_per_line \\* cycles_per_dot overflows signed arithmetic range" "${p}")
  profile_copy(p sum_overflow)
  gbl_json_mutate_set("${p}" "9223372036854775807" timing dots_visible value)
  gbl_json_mutate_set("${p}" "1" timing dots_hblank value)
  gbl_json_mutate_set("${p}" "9223372036854775807" timing dots_per_line value)
  gbl_json_mutate_set("${p}" "0" timing cycles_per_dot value)
  gbl_json_mutate_set("${p}" "0" timing cycles_per_line value)
  expect_detail(timing-arithmetic
    "dots_visible \\+ dots_hblank overflows signed arithmetic range" "${p}")
  profile_copy(p operand_out_of_range)
  gbl_json_mutate_set("${p}" "9223372036854775808" timing dots_visible value)
  expect_detail(timing-arithmetic
    "timing.dots_visible: exceeds signed arithmetic range" "${p}")

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
  profile_copy(p duplicate_row)
  doc_copy(d duplicate_row)
  gbl_text_mutate("${d}" "| `io-devices` | I/O device register semantics. |"
    "| `io-devices` | I/O device register semantics. |\n| `io-devices` | I/O device register semantics. |")
  expect_detail(doc-extra-id "duplicate id io-devices" "${p}" "${d}")
  profile_copy(p duplicate_replaces_row)
  doc_copy(d duplicate_replaces_row)
  gbl_text_mutate("${d}" "| `io-devices` | I/O device register semantics. |"
    "| `prefetch` | The cartridge prefetch buffer and cartridge wait-state timing. |")
  expect_detail(doc-extra-id "duplicate id prefetch" "${p}" "${d}")
  foreach(shape IN ITEMS left right neither hash)
    profile_copy(p unwrapped_${shape})
    doc_copy(d unwrapped_${shape})
    set(row "extra-device | test-only row")
    if(shape STREQUAL "left")
      set(row "| ${row}")
    elseif(shape STREQUAL "right")
      set(row "${row} |")
    elseif(shape STREQUAL "hash")
      set(row "#${row}")
    endif()
    gbl_text_mutate("${d}" "| `io-devices` | I/O device register semantics. |"
      "| `io-devices` | I/O device register semantics. |\n${row}")
    expect_code(doc-no-section "${p}" "${d}")
  endforeach()

elseif(CASE STREQUAL "doc_version_mismatch")
  profile_copy(p bumped)
  doc_copy(d bumped)
  gbl_text_mutate("${d}" "\nprofile_version: ${PROFILE_VERSION}\n" "\nprofile_version: ${NEXT_PROFILE_VERSION}\n")
  expect_code(doc-version-mismatch "${p}" "${d}")
  profile_copy(p deleted)
  doc_copy(d deleted)
  gbl_text_mutate("${d}" "\nprofile_version: ${PROFILE_VERSION}\n" "\n")
  expect_code(doc-version-mismatch "${p}" "${d}")
  profile_copy(p commented_version)
  doc_copy(d commented_version)
  gbl_text_mutate("${d}" "\nprofile_version: ${PROFILE_VERSION}\n"
    "\n<!--\nprofile_version: ${PROFILE_VERSION}\n-->\n")
  expect_detail(doc-version-mismatch "no profile_version line" "${p}" "${d}")
  profile_copy(p duplicate_version)
  doc_copy(d duplicate_version)
  gbl_text_mutate("${d}" "\nprofile_version: ${PROFILE_VERSION}\n"
    "\nprofile_version: ${PROFILE_VERSION}\nprofile_version: ${NEXT_PROFILE_VERSION}\n")
  expect_detail(doc-version-mismatch
    "expected exactly one profile_version line, found 2" "${p}" "${d}")

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
  profile_copy(p commented_unmodeled)
  doc_copy(d commented_unmodeled)
  file(READ "${d}" content)
  string(FIND "${content}" "| Id | What is not modeled |" table_start)
  if(table_start EQUAL -1)
    message(FATAL_ERROR "mutate: no-match: unmodeled table")
  endif()
  string(SUBSTRING "${content}" 0 ${table_start} head)
  string(SUBSTRING "${content}" ${table_start} -1 table)
  file(WRITE "${d}" "${head}<!--\n${table}-->\n")
  expect_code(doc-no-section "${p}" "${d}")
  profile_copy(p hidden_unmodeled_heading)
  doc_copy(d hidden_unmodeled_heading)
  gbl_text_mutate("${d}" "\n## Unmodeled\n"
    "\n<!--\n## Unmodeled\n-->\n")
  expect_code(doc-no-section "${p}" "${d}")

elseif(CASE STREQUAL "coordinated_change")
  # The next profile version plus a coordinated unmodeled addition is accepted while every
  # bus, initialization, and read policy value remains unchanged.
  profile_copy(p coordinated)
  doc_copy(d coordinated)
  gbl_json_mutate_set("${p}"
    [=[{"id": "serial-link", "status": "unmodeled", "reason": "Not emulated in v1.0; no result may claim serial link behavior."}]=]
    unmodeled 11)
  gbl_json_mutate_set("${p}" "${NEXT_PROFILE_VERSION}" profile_version)
  gbl_text_mutate("${d}" "| `backup-media` |"
    "| `serial-link` | The serial link port. |\n| `backup-media` |")
  gbl_text_mutate("${d}" "\nprofile_version: ${PROFILE_VERSION}\n" "\nprofile_version: ${NEXT_PROFILE_VERSION}\n")
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${NEXT_PROFILE_VERSION}, 11 timing constants, 12 unmodeled\\)")

else()
  message(FATAL_ERROR "profile-case: unknown case: ${CASE}")
endif()
