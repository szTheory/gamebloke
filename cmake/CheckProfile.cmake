# Profile validator (D-01, D-02, D-03, D-05, D-06, D-07, D-08, D-10, PROF-01).
#
# Script mode:   cmake -DPROFILE=<profiles/agb.json> -DDOC=<docs/profile-agb.md> \
#                      -DREGISTRY=<registry/disputes.json> -DVOCAB=<contracts/evidence.json> \
#                      -P cmake/CheckProfile.cmake
#
# profiles/agb.json is the authority for the machine profile; docs/profile-agb.md
# is the narrative. This check reads the vocabulary and the dispute registry
# first (their own findings are reported), loads the profile, validates it, then
# compares the unmodeled list and the version with the document.
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
  irq-controller backup-media)
set(GBL_PROFILE_HBLANK_DISPUTE disp-hblank-flag-edge)

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

# gbl__profile_doc_sync(<doc-file> <json-ids> <version-text>): the narrative
# document must agree with the JSON. <version-text> is empty when the JSON
# version is not valid (bad-version already reported).
function(gbl__profile_doc_sync docfile json_ids version)
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

  set(json_ids "")
  gbl_json_length(um_len "${doc}" unmodeled)
  if(um_len LESS 1)
    gbl_finding(profile missing-field "unmodeled")
    set(um_len 0)
  else()
    gbl__profile_unmodeled(json_ids "${doc}")
  endif()

  gbl__profile_doc_sync("${DOC}" "${json_ids}" "${version}")

  gbl_findings_finish(profile "ok (agb v${version}, 11 timing constants, ${um_len} unmodeled)")
endif()
