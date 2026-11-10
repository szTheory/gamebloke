# Vocabulary contract validator (D-02, D-11, EVID-05, PROF-02 claim fields).
#
#   include(cmake/CheckContract.cmake)
#   gbl_contract_read(<prefix> <vocab-file>)
#
# Script mode:   cmake -DVOCAB=<contracts/evidence.json> -P cmake/CheckContract.cmake
#
# contracts/evidence.json is the one file every later validator reads for the
# closed vocabularies (source labels, provenance classes, claim fields, and the
# nine result states with their rules). This file pins the same sets and the
# locked state rules as an independent oracle, so editing the data to weaken
# `a fixture that did not run is never PASS` fails here instead of passing.
#
# Reason codes (printed as `contract: <code>: <detail>`, every finding reported):
#   usage           script mode without -DVOCAB=
#   bad-schema      schema is not exactly gamebloke.evidence/1
#   missing-field   source_labels, provenance_classes, claim_fields, or states is
#                   absent or not an array of strings (states: of objects)
#   label-set       source_labels is not exactly REF, SRC, IMPL, INF, UNK
#   class-set       provenance_classes is not exactly the five pinned classes
#   claim-set       claim_fields is not exactly the six pinned claim fields
#   state-set       the state names are not exactly the nine pinned names
#   bad-state-rule  a rule value outside its allowed set, a refusal key on any
#                   state but REFUSED, or a break of a pinned locked rule
# The json: codes come from cmake/GblJson.cmake (the loader).
#
# On success gbl_contract_read sets in the caller's scope <prefix>_SOURCE_LABELS,
# <prefix>_CLASSES, <prefix>_CLAIM_FIELDS, <prefix>_STATES, and per state
# <prefix>_STATE_<NAME>_RAN|DIGEST|REASON|REFUSAL. Callers decide when to finish.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/GblJson.cmake")

# The oracle: pinned sets and locked rules. None contains a semicolon.
set(GBL_PIN_SOURCE_LABELS REF SRC IMPL INF UNK)
set(GBL_PIN_CLASSES project-authored hardware-observed reference-emulator
  suite-self-checking disputed)
set(GBL_PIN_CLAIM_FIELDS machine boot bios_provider cart input_timeline evidence)
set(GBL_PIN_STATES PASS GUEST_FAIL GUEST_STOP HARNESS_FAIL TIMEOUT CANCELLED
  MISSING SKIPPED REFUSED)
# <STATE> ran digest reason refusal; `-` means the state carries no rule there
# beyond the allowed-value check. Only the locked rows are pinned (R-1: GUEST_STOP
# needs no reason until Phase 4).
set(GBL_PIN_RULE_PASS true required - -)
set(GBL_PIN_RULE_MISSING false - required -)
set(GBL_PIN_RULE_SKIPPED false - required -)
set(GBL_PIN_RULE_REFUSED false - - required)

# gbl__contract_strings(<out> <doc> <field>): the string members
# of an array field as a list; a finding when the field is not an array of
# strings. Index loop, never foreach over JSON text.
function(gbl__contract_strings out doc field)
  set(items "")
  gbl_json_length(len "${doc}" ${field})
  if(len LESS 0)
    gbl_finding(contract missing-field "${field}")
  elseif(len GREATER 0)
    math(EXPR last "${len}-1")
    foreach(i RANGE ${last})
      gbl_json_string(item "${doc}" ${field} ${i})
      if("${item}" STREQUAL "<missing>")
        gbl_finding(contract missing-field "${field}[${i}]")
      else()
        list(APPEND items "${item}")
      endif()
    endforeach()
  else()
    gbl_finding(contract missing-field "${field}")
  endif()
  set(${out} "${items}" PARENT_SCOPE)
endfunction()

# gbl__contract_same_set(<out-ok> <expected-list> <actual-list>)
function(gbl__contract_same_set out expected actual)
  set(ok TRUE)
  list(LENGTH expected want)
  list(LENGTH actual got)
  if(NOT want EQUAL got)
    set(ok FALSE)
  endif()
  foreach(x IN LISTS expected)
    if(NOT "${x}" IN_LIST actual)
      set(ok FALSE)
    endif()
  endforeach()
  set(uniq "${actual}")
  list(REMOVE_DUPLICATES uniq)
  list(LENGTH uniq distinct)
  if(NOT distinct EQUAL got)
    set(ok FALSE)
  endif()
  set(${out} ${ok} PARENT_SCOPE)
endfunction()

# gbl_bios_provider_valid(<out> <value>): sets <out> to TRUE or FALSE.
#
# The BIOS provider grammar (D-10, R1), whole-string anchored so an embedded or
# trailing newline cannot slip through:
#   none
#   project-stub
#   hle:<name>(,<name>)*              <name> is [A-Za-z0-9_]+; the list is not empty,
#                                     has no empty member, and has no other rule
#                                     (no ordering, duplicates allowed)
#   replacement:<name>@<40 lowercase hex>   <name> is [a-z0-9][a-z0-9._-]*
#   user-official@<64 lowercase hex>
# SWI names inside hle: are not checked against a table of BIOS calls, and the
# retired milestone labels (M0, M1, M1b, M2) match none of the forms.
function(gbl_bios_provider_valid out value)
  string(REPEAT "[0-9a-f]" 40 hex40)
  string(REPEAT "[0-9a-f]" 64 hex64)
  set(ok FALSE)
  if("${value}" MATCHES "^none$"
      OR "${value}" MATCHES "^project-stub$"
      OR "${value}" MATCHES "^hle:[A-Za-z0-9_]+(,[A-Za-z0-9_]+)*$"
      OR "${value}" MATCHES "^replacement:[a-z0-9][a-z0-9._-]*@${hex40}$"
      OR "${value}" MATCHES "^user-official@${hex64}$")
    set(ok TRUE)
  endif()
  set(${out} ${ok} PARENT_SCOPE)
endfunction()

function(gbl_contract_read prefix vocab)
  gbl_json_load(doc "${vocab}")

  gbl_json_string(schema "${doc}" schema)
  if(NOT "${schema}" STREQUAL "gamebloke.evidence/1")
    gbl_finding(contract bad-schema "${vocab}")
  endif()

  gbl__contract_strings(labels "${doc}" source_labels)
  gbl__contract_same_set(ok "${GBL_PIN_SOURCE_LABELS}" "${labels}")
  if(NOT ok)
    gbl_finding(contract label-set "source_labels")
  endif()

  gbl__contract_strings(classes "${doc}" provenance_classes)
  gbl__contract_same_set(ok "${GBL_PIN_CLASSES}" "${classes}")
  if(NOT ok)
    gbl_finding(contract class-set "provenance_classes")
  endif()

  gbl__contract_strings(claims "${doc}" claim_fields)
  gbl__contract_same_set(ok "${GBL_PIN_CLAIM_FIELDS}" "${claims}")
  if(NOT ok)
    gbl_finding(contract claim-set "claim_fields")
  endif()

  # states: an array of objects, each with a name and the rule keys.
  set(names "")
  gbl_json_length(count "${doc}" states)
  if(count LESS 1)
    gbl_finding(contract missing-field "states")
  else()
    math(EXPR last "${count}-1")
    foreach(i RANGE ${last})
      gbl_json_type(rowty "${doc}" states ${i})
      if(NOT "${rowty}" STREQUAL "OBJECT")
        gbl_finding(contract missing-field "states[${i}]")
        continue()
      endif()
      gbl_json_string(name "${doc}" states ${i} name)
      list(APPEND names "${name}")
      gbl_json_string(ran "${doc}" states ${i} ran)
      gbl_json_string(digest "${doc}" states ${i} digest)
      gbl_json_string(reason "${doc}" states ${i} reason)
      gbl_json_type(refusal_ty "${doc}" states ${i} refusal)
      gbl_json_string(refusal "${doc}" states ${i} refusal)
      if(NOT "${refusal_ty}" STREQUAL "MISSING")
        if(NOT "${name}" STREQUAL "REFUSED")
          gbl_finding(contract bad-state-rule "${name}: refusal key")
        elseif(NOT "${refusal}" STREQUAL "required")
          gbl_finding(contract bad-state-rule "${name}: refusal")
        endif()
      endif()
      # Allowed-value sets, for every row.
      if(NOT "${ran}" STREQUAL "true" AND NOT "${ran}" STREQUAL "false"
          AND NOT "${ran}" STREQUAL "either")
        gbl_finding(contract bad-state-rule "${name}: ran")
      endif()
      if(NOT "${digest}" STREQUAL "required" AND NOT "${digest}" STREQUAL "optional")
        gbl_finding(contract bad-state-rule "${name}: digest")
      endif()
      if(NOT "${reason}" STREQUAL "required" AND NOT "${reason}" STREQUAL "optional")
        gbl_finding(contract bad-state-rule "${name}: reason")
      endif()
      # Locked rules, for the pinned names.
      if(DEFINED GBL_PIN_RULE_${name})
        list(GET GBL_PIN_RULE_${name} 0 want_ran)
        list(GET GBL_PIN_RULE_${name} 1 want_digest)
        list(GET GBL_PIN_RULE_${name} 2 want_reason)
        list(GET GBL_PIN_RULE_${name} 3 want_refusal)
        if(NOT "${ran}" STREQUAL "${want_ran}")
          gbl_finding(contract bad-state-rule "${name}: ran must be ${want_ran}")
        endif()
        if(NOT "${want_digest}" STREQUAL "-" AND NOT "${digest}" STREQUAL "${want_digest}")
          gbl_finding(contract bad-state-rule "${name}: digest must be ${want_digest}")
        endif()
        if(NOT "${want_reason}" STREQUAL "-" AND NOT "${reason}" STREQUAL "${want_reason}")
          gbl_finding(contract bad-state-rule "${name}: reason must be ${want_reason}")
        endif()
        if(NOT "${want_refusal}" STREQUAL "-" AND NOT "${refusal}" STREQUAL "${want_refusal}")
          gbl_finding(contract bad-state-rule "${name}: refusal must be ${want_refusal}")
        endif()
      endif()
      set(${prefix}_STATE_${name}_RAN "${ran}" PARENT_SCOPE)
      set(${prefix}_STATE_${name}_DIGEST "${digest}" PARENT_SCOPE)
      set(${prefix}_STATE_${name}_REASON "${reason}" PARENT_SCOPE)
      set(${prefix}_STATE_${name}_REFUSAL "${refusal}" PARENT_SCOPE)
    endforeach()
  endif()
  gbl__contract_same_set(ok "${GBL_PIN_STATES}" "${names}")
  if(NOT ok)
    gbl_finding(contract state-set "states")
  endif()

  set(${prefix}_SOURCE_LABELS "${labels}" PARENT_SCOPE)
  set(${prefix}_CLASSES "${classes}" PARENT_SCOPE)
  set(${prefix}_CLAIM_FIELDS "${claims}" PARENT_SCOPE)
  set(${prefix}_STATES "${names}" PARENT_SCOPE)
endfunction()

# ---- script-mode entry (inert when include()d) --------------------------------
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  if(NOT DEFINED VOCAB OR "${VOCAB}" STREQUAL "")
    message(FATAL_ERROR "contract: usage: -DVOCAB= is required")
  endif()
  gbl_contract_read(V "${VOCAB}")
  gbl_findings_finish(contract "ok (9 states, 5 labels, 5 classes, 6 claim fields)")
endif()
