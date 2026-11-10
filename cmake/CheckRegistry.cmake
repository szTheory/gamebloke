# Dispute registry validator (D-01, D-02, D-05, D-12, EVID-06).
#
#   include(cmake/CheckRegistry.cmake)
#   gbl_registry_read(<prefix> <registry-file> <vocab-prefix>)
#
# Script mode:   cmake -DREGISTRY=<registry/disputes.json> -DVOCAB=<contracts/evidence.json> \
#                      -P cmake/CheckRegistry.cmake
#
# registry/disputes.json records known disagreements between references,
# implementations, and reports, so a result whose expected value is disputed is
# never presented as settled. Later validators (profile, manifest, result)
# include this file and resolve dispute ids through gbl_registry_read.
# <vocab-prefix> names the prefix a prior gbl_contract_read(<vocab-prefix> ...)
# call used, so source labels come from contracts/evidence.json and are not
# repeated here.
#
# Reason codes (printed as `registry: <code>: <detail>`, every finding reported):
#   usage             script mode without -DREGISTRY= or without -DVOCAB=
#   bad-schema        schema is not exactly gamebloke.disputes/1
#   missing-field     entries absent or not an array; an entry that is not an
#                     object or lacks a non-empty id, summary, status, or affects;
#                     an affects item that is not a non-empty string; a source
#                     that is not an object or lacks a non-empty label, ref, or note
#   bad-id            id does not match ^disp-[a-z0-9][a-z0-9-]*$
#   duplicate-id      two entries share an id
#   bad-status        status is anything but `open` (no other status exists yet)
#   no-sources        sources absent, not an array, or empty
#   bad-source-label  a source label that is not in <vocab-prefix>_SOURCE_LABELS
#   seed-missing      one of the six pinned seed ids is absent (one finding per id)
# The json: codes come from cmake/GblJson.cmake (the loader).
#
# On success gbl_registry_read sets in the caller's scope <prefix>_IDS (the entry
# ids, in file order) and <prefix>_COUNT. Callers decide when to finish.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CheckContract.cmake")

# The seed ids are stable once chosen. Pinning them here means deleting or
# renaming a seed fails the gate instead of passing as a smaller registry.
set(GBL_PIN_SEED_DISPUTES
  disp-dma-latch disp-gbatek-errata disp-hblank-flag-edge
  disp-mgba-suite-22 disp-mgba-suite-24 disp-mgba-suite-27)

# gbl__registry_check_sources(<doc> <index> <vocab-prefix>): one entry's sources.
function(gbl__registry_check_sources doc idx vocab)
  gbl_json_length(slen "${doc}" entries ${idx} sources)
  if(slen LESS 1)
    gbl_finding(registry no-sources "entries[${idx}]")
    return()
  endif()
  math(EXPR slast "${slen}-1")
  foreach(s RANGE ${slast})
    gbl_json_type(sty "${doc}" entries ${idx} sources ${s})
    if(NOT "${sty}" STREQUAL "OBJECT")
      gbl_finding(registry missing-field "entries[${idx}].sources[${s}]")
      continue()
    endif()
    foreach(field label ref note)
      gbl_json_string(value "${doc}" entries ${idx} sources ${s} ${field})
      if("${value}" STREQUAL "<missing>")
        gbl_finding(registry missing-field "entries[${idx}].sources[${s}].${field}")
      elseif("${field}" STREQUAL "label")
        if(NOT "${value}" IN_LIST ${vocab}_SOURCE_LABELS)
          gbl_finding(registry bad-source-label "entries[${idx}].sources[${s}]")
        endif()
      endif()
    endforeach()
  endforeach()
endfunction()

function(gbl_registry_read prefix registry vocab)
  gbl_json_load(doc "${registry}")

  gbl_json_string(schema "${doc}" schema)
  if(NOT "${schema}" STREQUAL "gamebloke.disputes/1")
    gbl_finding(registry bad-schema "${registry}")
  endif()

  set(ids "")
  gbl_json_length(count "${doc}" entries)
  if(count LESS 0)
    gbl_finding(registry missing-field "entries")
    set(count 0)
  elseif(count GREATER 0)
    math(EXPR last "${count}-1")
    foreach(i RANGE ${last})
      gbl_json_type(ety "${doc}" entries ${i})
      if(NOT "${ety}" STREQUAL "OBJECT")
        gbl_finding(registry missing-field "entries[${i}]")
        continue()
      endif()

      gbl_json_string(id "${doc}" entries ${i} id)
      if("${id}" STREQUAL "<missing>")
        gbl_finding(registry missing-field "entries[${i}].id")
      else()
        if(NOT "${id}" MATCHES "^disp-[a-z0-9][a-z0-9-]*$")
          gbl_finding(registry bad-id "entries[${i}]")
        endif()
        if("${id}" IN_LIST ids)
          gbl_finding(registry duplicate-id "entries[${i}]")
        endif()
        list(APPEND ids "${id}")
      endif()

      gbl_json_string(summary "${doc}" entries ${i} summary)
      if("${summary}" STREQUAL "<missing>")
        gbl_finding(registry missing-field "entries[${i}].summary")
      endif()

      gbl_json_string(status "${doc}" entries ${i} status)
      if("${status}" STREQUAL "<missing>")
        gbl_finding(registry missing-field "entries[${i}].status")
      elseif(NOT "${status}" STREQUAL "open")
        gbl_finding(registry bad-status "entries[${i}]")
      endif()

      gbl_json_length(alen "${doc}" entries ${i} affects)
      if(alen LESS 1)
        gbl_finding(registry missing-field "entries[${i}].affects")
      else()
        math(EXPR alast "${alen}-1")
        foreach(a RANGE ${alast})
          gbl_json_string(item "${doc}" entries ${i} affects ${a})
          if("${item}" STREQUAL "<missing>")
            gbl_finding(registry missing-field "entries[${i}].affects[${a}]")
          endif()
        endforeach()
      endif()

      gbl__registry_check_sources("${doc}" ${i} ${vocab})
    endforeach()
  endif()

  foreach(seed IN LISTS GBL_PIN_SEED_DISPUTES)
    if(NOT "${seed}" IN_LIST ids)
      gbl_finding(registry seed-missing "${seed}")
    endif()
  endforeach()

  set(${prefix}_IDS "${ids}" PARENT_SCOPE)
  set(${prefix}_COUNT "${count}" PARENT_SCOPE)
endfunction()

# ---- script-mode entry (inert when include()d) --------------------------------
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  foreach(var REGISTRY VOCAB)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
      message(FATAL_ERROR "registry: usage: -D${var}= is required")
    endif()
  endforeach()
  gbl_contract_read(V "${VOCAB}")
  gbl_registry_read(R "${REGISTRY}" V)
  gbl_findings_finish(registry "ok (${R_COUNT} entries)")
endif()
