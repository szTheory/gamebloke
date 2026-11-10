# Profile validator (D-01, D-02, D-03, D-05, D-06, D-07, D-08, D-10, PROF-01).
#
# Script mode:   cmake -DPROFILE=<profiles/agb.json> -DDOC=<docs/profile-agb.md> \
#                      -DREGISTRY=<registry/disputes.json> -DVOCAB=<contracts/evidence.json> \
#                      -P cmake/CheckProfile.cmake
#
# profiles/agb.json is the authority for the machine profile; docs/profile-agb.md
# is the narrative. This check reads the vocabulary and the dispute registry
# first (their own findings are reported), loads the profile, validates it, then
# compares the profile policy tables, unmodeled list, and version with the document.
#
# Reason codes (printed as `profile: <code>: <detail>`, every finding reported):
#   usage                  script mode without -DPROFILE=, -DDOC=, -DREGISTRY=, or -DVOCAB=
#   bad-schema             schema is not exactly gamebloke.profile/1
#   bad-version            profile_version is not a number whose text is a positive integer
#   missing-field          schema, profile_version, id, machine, boot, bios_provider, cart,
#                          timing, or unmodeled absent, null, or empty; any of the eleven
#                          timing keys absent or not an object; a numeric timing value absent
#   bad-machine            id or machine is not exactly `agb`
#   bad-boot               boot is not `direct-start`
#   bad-bios-provider      bios_provider fails gbl_bios_provider_valid (cmake/CheckContract.cmake)
#   bad-cart               cart is not an object or its kind is not `rom-only`
#   missing-source         a timing entry without `source`, or a non-UNK entry without a
#                          non-empty `cite`
#   bad-source-label       a source outside the vocabulary labels, or a null value under a
#                          non-UNK label
#   timing-arithmetic      one finding per broken invariant: dots_visible + dots_hblank =
#                          dots_per_line; dots_per_line * cycles_per_dot = cycles_per_line;
#                          lines_visible + lines_vblank = lines_per_frame; cycles_per_line *
#                          lines_per_frame = cycles_per_frame; master_clock_hz = 2^24
#   hblank-edge-hardcoded  hblank_flag_edge_cycle.value is anything but null
#   unmodeled-missing      one finding per required unmodeled id that is absent
#   unmodeled-bad-entry    an entry that is not an object; an id that is not
#                          ^[a-z0-9][a-z0-9-]*$ or is duplicated; a status other than
#                          `unmodeled`; a missing or empty reason; sp-micro-differences
#                          details lacking an item that names 0x04000800 or one that names
#                          the cartridge switch; hblank-flag-edge without its dispute
#   dispute-unknown        a `dispute` value in timing or unmodeled that is not a registry
#                          id, or the hblank_flag_edge_cycle entry with no dispute
#   doc-no-section         the document has no `## Unmodeled` heading or the section has no rows
#   doc-missing-id         a JSON unmodeled id is absent from the document table
#   doc-extra-id           a document table id is absent from the JSON
#   doc-version-mismatch   no `profile_version: N` line in the document, or N differs
#   bus-policy-missing     bus_policy is absent or null
#   bus-policy-value       bus policy structure, request costs, width/class enums, or unit is wrong
#   bus-policy-source      bus policy is not project-authored with its explicit citation
#   bus-policy-doc         bus policy narrative is missing, stale, or has an extra row
#   init-policy            initialization policy structure, provenance, or zero value is wrong
#   read-policy            read policy structure, provenance, or placeholder value is wrong
# The json:, contract:, and registry: codes come from the loader and the vocabulary
# and registry readers this file calls.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CheckRegistry.cmake")

set(GBL_PROFILE_TIMING_KEYS
  master_clock_hz cycles_per_dot dots_visible dots_hblank dots_per_line
  cycles_per_line lines_visible lines_vblank lines_per_frame cycles_per_frame
  hblank_flag_edge_cycle)
set(GBL_PROFILE_REQUIRED_UNMODELED
  sp-micro-differences prefetch hblank-flag-edge ppu audio dma timers
  irq-controller backup-media bios-protection io-devices)
set(GBL_PROFILE_HBLANK_DISPUTE disp-hblank-flag-edge)
set(GBL_PROFILE_BUS_REQUEST_KEYS bios ewram iwram io pram vram oam rom unmapped)
set(GBL_PROFILE_BUS_CITE
  "Gamebloke project policy: one master cycle per raw request; no WAITCNT or prefetch timing claim.")
set(GBL_PROFILE_INIT_CITE
  "Gamebloke deterministic project policy: zero initialize state; hardware power-on RAM values remain unknown.")
set(GBL_PROFILE_READ_CITE
  "Unverified Gamebloke project behavior: current-latch placeholders for absent BIOS, unsupported I/O, and ROM tails.")

# Validate a policy's source and citation as project-authored evidence. Hardware
# claims are not inferred from these placeholders.
function(gbl__profile_policy_source doc policy expected_cite reason)
  gbl_json_string(src "${doc}" ${policy} source)
  gbl_json_string(cite "${doc}" ${policy} cite)
  if(NOT "${src}" STREQUAL "IMPL" OR NOT "${cite}" STREQUAL "${expected_cite}")
    gbl_finding(profile "${reason}" "${policy}: source/citation")
  endif()
endfunction()

# The request-cost table, raw widths/classes, and internal-cycle unit are an
# explicit project policy. Check types and exact values rather than treating
# placeholder values as hardware observations.
function(gbl__profile_bus_policy doc)
  gbl_json_type(pty "${doc}" bus_policy)
  if("${pty}" STREQUAL "MISSING" OR "${pty}" STREQUAL "NULL")
    gbl_finding(profile bus-policy-missing "bus_policy")
    return()
  elseif(NOT "${pty}" STREQUAL "OBJECT")
    gbl_finding(profile bus-policy-value "bus_policy: not an object")
    return()
  endif()

  string(JSON plen LENGTH "${doc}" bus_policy)
  if(NOT plen EQUAL 6)
    gbl_finding(profile bus-policy-value "bus_policy: field count")
  endif()
  gbl__profile_policy_source("${doc}" bus_policy "${GBL_PROFILE_BUS_CITE}" bus-policy-source)

  gbl_json_type(rty "${doc}" bus_policy request_cycles)
  if(NOT "${rty}" STREQUAL "OBJECT")
    gbl_finding(profile bus-policy-value "bus_policy.request_cycles: not an object")
  else()
    string(JSON rlen LENGTH "${doc}" bus_policy request_cycles)
    list(LENGTH GBL_PROFILE_BUS_REQUEST_KEYS rcount)
    if(NOT rlen EQUAL rcount)
      gbl_finding(profile bus-policy-value "bus_policy.request_cycles: field count")
    endif()
    foreach(k IN LISTS GBL_PROFILE_BUS_REQUEST_KEYS)
      gbl_json_type(vty "${doc}" bus_policy request_cycles ${k})
      if(NOT "${vty}" STREQUAL "NUMBER")
        gbl_finding(profile bus-policy-value "bus_policy.request_cycles.${k}: expected 1")
      else()
        string(JSON value GET "${doc}" bus_policy request_cycles ${k})
        if(NOT "${value}" STREQUAL "1")
          gbl_finding(profile bus-policy-value "bus_policy.request_cycles.${k}: expected 1")
        endif()
      endif()
    endforeach()
  endif()

  gbl_json_type(wty "${doc}" bus_policy widths_bytes)
  if(NOT "${wty}" STREQUAL "ARRAY")
    gbl_finding(profile bus-policy-value "bus_policy.widths_bytes: not an array")
  else()
    string(JSON wlen LENGTH "${doc}" bus_policy widths_bytes)
    if(NOT wlen EQUAL 3)
      gbl_finding(profile bus-policy-value "bus_policy.widths_bytes: expected [1, 2, 4]")
    else()
      foreach(i RANGE 0 2)
        gbl_json_type(vty "${doc}" bus_policy widths_bytes ${i})
        string(JSON value GET "${doc}" bus_policy widths_bytes ${i})
        if(i EQUAL 0)
          set(expected 1)
        elseif(i EQUAL 1)
          set(expected 2)
        else()
          set(expected 4)
        endif()
        if(NOT "${vty}" STREQUAL "NUMBER" OR NOT "${value}" STREQUAL "${expected}")
          gbl_finding(profile bus-policy-value "bus_policy.widths_bytes: expected [1, 2, 4]")
        endif()
      endforeach()
    endif()
  endif()

  gbl_json_type(aty "${doc}" bus_policy access_classes)
  if(NOT "${aty}" STREQUAL "ARRAY")
    gbl_finding(profile bus-policy-value "bus_policy.access_classes: not an array")
  else()
    string(JSON alen LENGTH "${doc}" bus_policy access_classes)
    if(NOT alen EQUAL 2)
      gbl_finding(profile bus-policy-value "bus_policy.access_classes: expected [nonsequential, sequential]")
    else()
      string(JSON first GET "${doc}" bus_policy access_classes 0)
      string(JSON second GET "${doc}" bus_policy access_classes 1)
      if(NOT "${first}" STREQUAL "nonsequential" OR NOT "${second}" STREQUAL "sequential")
        gbl_finding(profile bus-policy-value "bus_policy.access_classes: expected [nonsequential, sequential]")
      endif()
    endif()
  endif()

  gbl_json_type(ity "${doc}" bus_policy internal_cycle_unit)
  if(NOT "${ity}" STREQUAL "NUMBER")
    gbl_finding(profile bus-policy-value "bus_policy.internal_cycle_unit: expected 1")
  else()
    string(JSON unit GET "${doc}" bus_policy internal_cycle_unit)
    if(NOT "${unit}" STREQUAL "1")
      gbl_finding(profile bus-policy-value "bus_policy.internal_cycle_unit: expected 1")
    endif()
  endif()
endfunction()

function(gbl__profile_initialization_policy doc)
  gbl_json_type(pty "${doc}" initialization_policy)
  if(NOT "${pty}" STREQUAL "OBJECT")
    gbl_finding(profile init-policy "initialization_policy: missing or not an object")
    return()
  endif()
  string(JSON plen LENGTH "${doc}" initialization_policy)
  if(NOT plen EQUAL 6)
    gbl_finding(profile init-policy "initialization_policy: field count")
  endif()
  gbl__profile_policy_source("${doc}" initialization_policy "${GBL_PROFILE_INIT_CITE}" init-policy)
  foreach(k IN ITEMS memory_fill register_fill master_cycles open_bus)
    gbl_json_type(vty "${doc}" initialization_policy ${k})
    if(NOT "${vty}" STREQUAL "NUMBER")
      gbl_finding(profile init-policy "initialization_policy.${k}: expected 0")
    else()
      string(JSON value GET "${doc}" initialization_policy ${k})
      if(NOT "${value}" STREQUAL "0")
        gbl_finding(profile init-policy "initialization_policy.${k}: expected 0")
      endif()
    endif()
  endforeach()
endfunction()

function(gbl__profile_read_policy doc)
  gbl_json_type(pty "${doc}" read_policy)
  if(NOT "${pty}" STREQUAL "OBJECT")
    gbl_finding(profile read-policy "read_policy: missing or not an object")
    return()
  endif()
  string(JSON plen LENGTH "${doc}" read_policy)
  if(NOT plen EQUAL 7)
    gbl_finding(profile read-policy "read_policy: field count")
  endif()
  gbl__profile_policy_source("${doc}" read_policy "${GBL_PROFILE_READ_CITE}" read-policy)
  foreach(k IN ITEMS absent_bios unsupported_io rom_tail)
    gbl_json_string(value "${doc}" read_policy ${k})
    if(NOT "${value}" STREQUAL "current-open-bus")
      gbl_finding(profile read-policy "read_policy.${k}: expected current-open-bus")
    endif()
  endforeach()
  gbl_json_string(latch "${doc}" read_policy latch_update)
  if(NOT "${latch}" STREQUAL "mapped-read-width-replicated")
    gbl_finding(profile read-policy "read_policy.latch_update: expected mapped-read-width-replicated")
  endif()
  gbl_json_string(write_latch "${doc}" read_policy write_latch)
  if(NOT "${write_latch}" STREQUAL "preserve")
    gbl_finding(profile read-policy "read_policy.write_latch: expected preserve")
  endif()
endfunction()

# gbl__profile_dispute(<doc> <where> <key>...): when a `dispute` member exists at
# <key>..., it must name a registry id (R_IDS in the caller's scope).
function(gbl__profile_dispute doc where)
  gbl_json_type(dty "${doc}" ${ARGN} dispute)
  if("${dty}" STREQUAL "MISSING")
    return()
  endif()
  gbl_json_string(did "${doc}" ${ARGN} dispute)
  if("${did}" STREQUAL "<missing>")
    gbl_finding(profile dispute-unknown "${where}: dispute is not a non-empty string")
  elseif(NOT "${did}" IN_LIST R_IDS)
    gbl_finding(profile dispute-unknown "${where}: ${did}")
  endif()
endfunction()

# gbl__profile_timing(<doc>): the eleven timing entries. Sets T_<key> (integer
# text, or empty when unavailable) in the caller's scope, then checks the
# arithmetic invariants over the values that are available.
function(gbl__profile_timing doc)
  gbl_json_type(tty "${doc}" timing)
  if(NOT "${tty}" STREQUAL "OBJECT")
    return()
  endif()
  foreach(k IN LISTS GBL_PROFILE_TIMING_KEYS)
    set(T_${k} "")
    gbl_json_type(ety "${doc}" timing ${k})
    if(NOT "${ety}" STREQUAL "OBJECT")
      gbl_finding(profile missing-field "timing.${k}")
      continue()
    endif()

    gbl_json_string(src "${doc}" timing ${k} source)
    set(label_ok FALSE)
    if("${src}" STREQUAL "<missing>")
      gbl_finding(profile missing-source "timing.${k}: source")
    elseif("${src}" IN_LIST V_SOURCE_LABELS)
      set(label_ok TRUE)
    else()
      gbl_finding(profile bad-source-label "timing.${k}")
    endif()
    if(NOT "${src}" STREQUAL "UNK" AND NOT "${src}" STREQUAL "<missing>")
      gbl_json_string(cite "${doc}" timing ${k} cite)
      if("${cite}" STREQUAL "<missing>")
        gbl_finding(profile missing-source "timing.${k}: cite")
      endif()
    endif()

    gbl_json_type(vty "${doc}" timing ${k} value)
    if("${k}" STREQUAL "hblank_flag_edge_cycle")
      if("${vty}" STREQUAL "MISSING")
        gbl_finding(profile missing-field "timing.${k}.value")
      elseif(NOT "${vty}" STREQUAL "NULL")
        gbl_finding(profile hblank-edge-hardcoded "timing.${k}.value")
      endif()
      if(NOT "${src}" STREQUAL "<missing>" AND label_ok AND NOT "${src}" STREQUAL "UNK" AND "${vty}" STREQUAL "NULL")
        gbl_finding(profile bad-source-label "timing.${k}: null value under ${src}")
      endif()
      gbl__profile_dispute("${doc}" "timing.${k}" timing ${k})
      gbl_json_type(dty "${doc}" timing ${k} dispute)
      if("${dty}" STREQUAL "MISSING")
        gbl_finding(profile dispute-unknown "timing.${k}: no dispute")
      endif()
      continue()
    endif()

    if("${vty}" STREQUAL "NULL")
      if(label_ok AND NOT "${src}" STREQUAL "UNK")
        gbl_finding(profile bad-source-label "timing.${k}: null value under ${src}")
      endif()
    elseif("${vty}" STREQUAL "NUMBER")
      string(JSON vtext GET "${doc}" timing ${k} value)
      if("${vtext}" MATCHES "^(0|[1-9][0-9]*)$")
        set(T_${k} "${vtext}")
      else()
        gbl_finding(profile timing-arithmetic "timing.${k}: not an integer")
      endif()
    else()
      gbl_finding(profile missing-field "timing.${k}.value")
    endif()
    gbl__profile_dispute("${doc}" "timing.${k}" timing ${k})
  endforeach()

  # Invariants, each only over operands that are available integers.
  if(NOT "${T_dots_visible}" STREQUAL "" AND NOT "${T_dots_hblank}" STREQUAL ""
      AND NOT "${T_dots_per_line}" STREQUAL "")
    math(EXPR sum "${T_dots_visible}+${T_dots_hblank}")
    if(NOT sum EQUAL T_dots_per_line)
      gbl_finding(profile timing-arithmetic "dots_visible + dots_hblank != dots_per_line")
    endif()
  endif()
  if(NOT "${T_dots_per_line}" STREQUAL "" AND NOT "${T_cycles_per_dot}" STREQUAL ""
      AND NOT "${T_cycles_per_line}" STREQUAL "")
    math(EXPR prod "${T_dots_per_line}*${T_cycles_per_dot}")
    if(NOT prod EQUAL T_cycles_per_line)
      gbl_finding(profile timing-arithmetic "dots_per_line * cycles_per_dot != cycles_per_line")
    endif()
  endif()
  if(NOT "${T_lines_visible}" STREQUAL "" AND NOT "${T_lines_vblank}" STREQUAL ""
      AND NOT "${T_lines_per_frame}" STREQUAL "")
    math(EXPR sum "${T_lines_visible}+${T_lines_vblank}")
    if(NOT sum EQUAL T_lines_per_frame)
      gbl_finding(profile timing-arithmetic "lines_visible + lines_vblank != lines_per_frame")
    endif()
  endif()
  if(NOT "${T_cycles_per_line}" STREQUAL "" AND NOT "${T_lines_per_frame}" STREQUAL ""
      AND NOT "${T_cycles_per_frame}" STREQUAL "")
    math(EXPR prod "${T_cycles_per_line}*${T_lines_per_frame}")
    if(NOT prod EQUAL T_cycles_per_frame)
      gbl_finding(profile timing-arithmetic "cycles_per_line * lines_per_frame != cycles_per_frame")
    endif()
  endif()
  if(NOT "${T_master_clock_hz}" STREQUAL "")
    math(EXPR clock "1 << 24")
    if(NOT clock EQUAL T_master_clock_hz)
      gbl_finding(profile timing-arithmetic "master_clock_hz != 2^24")
    endif()
  endif()
endfunction()

# gbl__profile_unmodeled(<out-ids> <doc>): validate the unmodeled list and set
# <out-ids> to the entry ids in file order (duplicates kept).
function(gbl__profile_unmodeled out doc)
  set(ids "")
  gbl_json_length(count "${doc}" unmodeled)
  if(count LESS 1)
    set(${out} "" PARENT_SCOPE)
    return()
  endif()
  set(seen "")
  math(EXPR last "${count}-1")
  foreach(i RANGE ${last})
    gbl_json_type(ety "${doc}" unmodeled ${i})
    if(NOT "${ety}" STREQUAL "OBJECT")
      gbl_finding(profile unmodeled-bad-entry "unmodeled[${i}]: not an object")
      continue()
    endif()
    gbl_json_string(id "${doc}" unmodeled ${i} id)
    if("${id}" STREQUAL "<missing>")
      gbl_finding(profile unmodeled-bad-entry "unmodeled[${i}]: id")
      continue()
    endif()
    list(APPEND ids "${id}")
    if(NOT "${id}" MATCHES "^[a-z0-9][a-z0-9-]*$")
      gbl_finding(profile unmodeled-bad-entry "unmodeled[${i}]: bad id")
    endif()
    if("${id}" IN_LIST seen)
      gbl_finding(profile unmodeled-bad-entry "unmodeled[${i}]: duplicate id ${id}")
    endif()
    list(APPEND seen "${id}")

    gbl_json_string(status "${doc}" unmodeled ${i} status)
    if(NOT "${status}" STREQUAL "unmodeled")
      gbl_finding(profile unmodeled-bad-entry "${id}: status")
    endif()
    gbl_json_string(reason "${doc}" unmodeled ${i} reason)
    if("${reason}" STREQUAL "<missing>")
      gbl_finding(profile unmodeled-bad-entry "${id}: reason")
    endif()
    gbl__profile_dispute("${doc}" "unmodeled.${id}" unmodeled ${i})

    if("${id}" STREQUAL "sp-micro-differences")
      set(has_reg FALSE)
      set(has_sw FALSE)
      gbl_json_length(dlen "${doc}" unmodeled ${i} details)
      if(dlen GREATER 0)
        math(EXPR dlast "${dlen}-1")
        foreach(d RANGE ${dlast})
          gbl_json_string(item "${doc}" unmodeled ${i} details ${d})
          string(FIND "${item}" "0x04000800" p1)
          string(FIND "${item}" "cartridge switch" p2)
          if(NOT p1 EQUAL -1)
            set(has_reg TRUE)
          endif()
          if(NOT p2 EQUAL -1)
            set(has_sw TRUE)
          endif()
        endforeach()
      endif()
      if(NOT has_reg)
        gbl_finding(profile unmodeled-bad-entry "${id}: details lack the 0x04000800 item")
      endif()
      if(NOT has_sw)
        gbl_finding(profile unmodeled-bad-entry "${id}: details lack the cartridge switch item")
      endif()
    elseif("${id}" STREQUAL "hblank-flag-edge")
      gbl_json_string(did "${doc}" unmodeled ${i} dispute)
      if(NOT "${did}" STREQUAL "${GBL_PROFILE_HBLANK_DISPUTE}")
        gbl_finding(profile unmodeled-bad-entry "${id}: dispute")
      endif()
    endif()
  endforeach()

  foreach(req IN LISTS GBL_PROFILE_REQUIRED_UNMODELED)
    if(NOT "${req}" IN_LIST ids)
      gbl_finding(profile unmodeled-missing "${req}")
    endif()
  endforeach()
  set(${out} "${ids}" PARENT_SCOPE)
endfunction()

# Check one narrative policy table against rows built from the profile values.
# Only inspect the section beginning at <heading>; matching rows elsewhere in
# the document must not satisfy a missing or stale policy table.
function(gbl__profile_doc_policy_table text heading prefix expected_count)
  string(FIND "${text}" "\n${heading}\n" heading_at)
  if(heading_at EQUAL -1)
    gbl_finding(profile bus-policy-doc "missing ${heading} section")
    return()
  endif()
  string(LENGTH "\n${heading}\n" heading_length)
  math(EXPR section_start "${heading_at}+${heading_length}")
  string(SUBSTRING "${text}" ${section_start} -1 section)
  string(REGEX MATCH "\n#+[ \t]" next_heading "${section}")
  if(NOT "${next_heading}" STREQUAL "")
    string(FIND "${section}" "${next_heading}" next_heading_at)
    string(SUBSTRING "${section}" 0 ${next_heading_at} section)
  endif()

  string(REGEX MATCHALL "\\| ${prefix}\\.[^|]*\\|" rows "${section}")
  list(LENGTH rows row_count)
  if(NOT row_count EQUAL expected_count)
    gbl_finding(profile bus-policy-doc "${prefix}: expected ${expected_count} rows, found ${row_count}")
  endif()
  foreach(row IN LISTS ARGN)
    string(FIND "${section}" "${row}" at)
    if(at EQUAL -1)
      gbl_finding(profile bus-policy-doc "${prefix}: missing or stale row")
    endif()
  endforeach()
endfunction()

# gbl__profile_doc_sync(<doc-file> <profile-json> <json-ids> <version-text>): the narrative
# document must agree with the JSON. <version-text> is empty when the JSON
# version is not valid (bad-version already reported).
function(gbl__profile_doc_sync docfile profile json_ids version)
  if(NOT EXISTS "${docfile}" OR IS_DIRECTORY "${docfile}")
    gbl_finding(profile doc-no-section "${docfile}: unreadable")
    return()
  endif()
  # file(READ), never file(STRINGS): lines may hold semicolons or brackets.
  file(READ "${docfile}" text)
  set(text "\n${text}")

  string(REGEX MATCH "\nprofile_version: [0-9]+" vline "${text}")
  if("${vline}" STREQUAL "")
    gbl_finding(profile doc-version-mismatch "no profile_version line")
  elseif(NOT "${version}" STREQUAL "")
    string(REGEX REPLACE "^\nprofile_version: " "" dver "${vline}")
    if(NOT "${dver}" STREQUAL "${version}")
      gbl_finding(profile doc-version-mismatch "document ${dver}, profile ${version}")
    endif()
  endif()

  foreach(heading IN ITEMS "## Bus policy" "## Initialization policy" "## Read policy")
    string(FIND "${text}" "${heading}" heading_at)
    if(heading_at EQUAL -1)
      gbl_finding(profile bus-policy-doc "missing ${heading} section")
    endif()
  endforeach()

  gbl_json_string(bus_source "${profile}" bus_policy source)
  gbl_json_string(bus_cite "${profile}" bus_policy cite)
  set(bus_rows)
  foreach(k IN LISTS GBL_PROFILE_BUS_REQUEST_KEYS)
    list(APPEND bus_rows "| bus_policy.request_cycles.${k} | 1 | ${bus_source} | ${bus_cite} |")
  endforeach()
  list(APPEND bus_rows
    "| bus_policy.widths_bytes | [1, 2, 4] | ${bus_source} | ${bus_cite} |"
    "| bus_policy.access_classes | [nonsequential, sequential] | ${bus_source} | ${bus_cite} |"
    "| bus_policy.internal_cycle_unit | 1 | ${bus_source} | ${bus_cite} |")
  gbl__profile_doc_policy_table("${text}" "## Bus policy" bus_policy 12 "${bus_rows}")

  gbl_json_string(init_source "${profile}" initialization_policy source)
  gbl_json_string(init_cite "${profile}" initialization_policy cite)
  set(init_rows)
  foreach(k IN ITEMS memory_fill register_fill master_cycles open_bus)
    list(APPEND init_rows "| initialization_policy.${k} | 0 | ${init_source} | ${init_cite} |")
  endforeach()
  gbl__profile_doc_policy_table("${text}" "## Initialization policy"
    initialization_policy 4 "${init_rows}")

  gbl_json_string(read_source "${profile}" read_policy source)
  gbl_json_string(read_cite "${profile}" read_policy cite)
  set(read_rows
    "| read_policy.absent_bios | current-open-bus | ${read_source} | ${read_cite} |"
    "| read_policy.unsupported_io | current-open-bus | ${read_source} | ${read_cite} |"
    "| read_policy.rom_tail | current-open-bus | ${read_source} | ${read_cite} |"
    "| read_policy.latch_update | mapped-read-width-replicated | ${read_source} | ${read_cite} |"
    "| read_policy.write_latch | preserve | ${read_source} | ${read_cite} |")
  gbl__profile_doc_policy_table("${text}" "## Read policy" read_policy 5 "${read_rows}")

  string(FIND "${text}" "\n## Unmodeled\n" at)
  set(doc_ids "")
  if(NOT at EQUAL -1)
    string(LENGTH "\n## Unmodeled" hlen)
    math(EXPR from "${at}+${hlen}")
    string(SUBSTRING "${text}" ${from} -1 section)
    string(FIND "${section}" "\n## " nxt)
    if(NOT nxt EQUAL -1)
      string(SUBSTRING "${section}" 0 ${nxt} section)
    endif()
    string(REGEX MATCHALL "\n\\| `[^`]+`" rows "${section}")
    foreach(row IN LISTS rows)
      string(REGEX REPLACE "^\n\\| `([^`]+)`$" "\\1" rid "${row}")
      list(APPEND doc_ids "${rid}")
    endforeach()
  endif()
  if("${doc_ids}" STREQUAL "")
    gbl_finding(profile doc-no-section "## Unmodeled")
    return()
  endif()

  list(REMOVE_DUPLICATES json_ids)
  foreach(id IN LISTS json_ids)
    if(NOT "${id}" IN_LIST doc_ids)
      gbl_finding(profile doc-missing-id "${id}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES doc_ids)
  foreach(id IN LISTS doc_ids)
    if(NOT "${id}" IN_LIST json_ids)
      gbl_finding(profile doc-extra-id "${id}")
    endif()
  endforeach()
endfunction()

# ---- script-mode entry (inert when include()d) --------------------------------
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  foreach(var PROFILE DOC REGISTRY VOCAB)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
      message(FATAL_ERROR "profile: usage: -D${var}= is required")
    endif()
  endforeach()

  gbl_contract_read(V "${VOCAB}")
  gbl_registry_read(R "${REGISTRY}" V)
  gbl_json_load(doc "${PROFILE}")

  # Top-level scalar fields.
  gbl_json_string(schema "${doc}" schema)
  if("${schema}" STREQUAL "<missing>")
    gbl_finding(profile missing-field "schema")
  elseif(NOT "${schema}" STREQUAL "gamebloke.profile/1")
    gbl_finding(profile bad-schema "${PROFILE}")
  endif()

  set(version "")
  gbl_json_type(vty "${doc}" profile_version)
  if("${vty}" STREQUAL "MISSING" OR "${vty}" STREQUAL "NULL")
    gbl_finding(profile missing-field "profile_version")
  elseif(NOT "${vty}" STREQUAL "NUMBER")
    gbl_finding(profile bad-version "profile_version")
  else()
    string(JSON vtext GET "${doc}" profile_version)
    if("${vtext}" MATCHES "^[1-9][0-9]*$")
      set(version "${vtext}")
    else()
      gbl_finding(profile bad-version "profile_version")
    endif()
  endif()

  foreach(field id machine)
    gbl_json_string(value "${doc}" ${field})
    if("${value}" STREQUAL "<missing>")
      gbl_finding(profile missing-field "${field}")
    elseif(NOT "${value}" STREQUAL "agb")
      gbl_finding(profile bad-machine "${field}")
    endif()
  endforeach()

  gbl_json_string(boot "${doc}" boot)
  if("${boot}" STREQUAL "<missing>")
    gbl_finding(profile missing-field "boot")
  elseif(NOT "${boot}" STREQUAL "direct-start")
    gbl_finding(profile bad-boot "boot")
  endif()

  gbl_json_string(bios "${doc}" bios_provider)
  if("${bios}" STREQUAL "<missing>")
    gbl_finding(profile missing-field "bios_provider")
  else()
    gbl_bios_provider_valid(bios_ok "${bios}")
    if(NOT bios_ok)
      gbl_finding(profile bad-bios-provider "bios_provider")
    endif()
  endif()

  gbl_json_type(cart_ty "${doc}" cart)
  if("${cart_ty}" STREQUAL "MISSING" OR "${cart_ty}" STREQUAL "NULL")
    gbl_finding(profile missing-field "cart")
  elseif(NOT "${cart_ty}" STREQUAL "OBJECT")
    gbl_finding(profile bad-cart "cart: not an object")
  else()
    string(JSON cart_len LENGTH "${doc}" cart)
    if(cart_len EQUAL 0)
      gbl_finding(profile missing-field "cart")
    endif()
    gbl_json_string(kind "${doc}" cart kind)
    if(NOT "${kind}" STREQUAL "rom-only")
      gbl_finding(profile bad-cart "cart.kind")
    endif()
  endif()

  # Timing and unmodeled.
  gbl_json_type(timing_ty "${doc}" timing)
  if("${timing_ty}" STREQUAL "MISSING" OR "${timing_ty}" STREQUAL "NULL")
    gbl_finding(profile missing-field "timing")
  elseif(NOT "${timing_ty}" STREQUAL "OBJECT")
    gbl_finding(profile missing-field "timing: not an object")
  else()
    string(JSON timing_len LENGTH "${doc}" timing)
    if(timing_len EQUAL 0)
      gbl_finding(profile missing-field "timing")
    endif()
    gbl__profile_timing("${doc}")
  endif()

  gbl__profile_bus_policy("${doc}")
  gbl__profile_initialization_policy("${doc}")
  gbl__profile_read_policy("${doc}")

  set(json_ids "")
  gbl_json_length(um_len "${doc}" unmodeled)
  if(um_len LESS 1)
    gbl_finding(profile missing-field "unmodeled")
    set(um_len 0)
  else()
    gbl__profile_unmodeled(json_ids "${doc}")
  endif()

  gbl__profile_doc_sync("${DOC}" "${doc}" "${json_ids}" "${version}")

  gbl_findings_finish(profile "ok (agb v${version}, 11 timing constants, ${um_len} unmodeled)")
endif()
