# Result document validator (D-02, D-09, D-10, D-11, D-12, PROF-02, EVID-05, EVID-06).
#
#   include(cmake/CheckResult.cmake)
#   gbl_result_check(<result-file> <manifest-prefix> <registry-prefix> <vocab-prefix>)
#
# Script mode:   cmake -DRESULT=<result.json> -DMANIFEST=<manifest.json> \
#                      -DREGISTRY=<registry/disputes.json> -DVOCAB=<contracts/evidence.json> \
#                      -P cmake/CheckResult.cmake
#
# A result says what one fixture run proved and under which claim tuple. The document is
# schema gamebloke.result/1:
#   fixture   {id, ran (boolean), rom_sha256 (64 lowercase hex or null)}
#   status    one of the states in the vocabulary file
#   reason    text; required by the state rows that say so
#   refusal   {code, postcondition}; required by the state rows that say so
#   claim     the six claim fields: machine, boot, bios_provider, cart, input_timeline,
#             evidence
#   disputes  registry ids; labels  result labels; limits  what the result does not prove
#
# The per-state rules (ran, digest, reason, refusal) and the closed vocabularies come from
# contracts/evidence.json through gbl_contract_read, never from this file. The one state
# spelled here is PASS, because two of the reason codes are named after it. The one label
# spelled here is `disputed`: its meaning is the disclosure rule below, so it is not a
# vocabulary entry, and a new label would arrive with its own rule and a gamebloke.result/2
# bump (I2). Phase 4 adds required semantic, time, termination, and trace observations
# without changing the schema, status vocabulary, or label vocabulary.
#
# Reason codes (printed as `result: <code>: <detail>`, every finding reported):
#   usage                 script mode with a missing -D input
#   bad-schema            schema is not exactly gamebloke.result/1
#   missing-field         schema, fixture, fixture.id, status, claim, disputes, labels, or
#                         limits absent or null; disputes or labels not an array;
#                         fixture.ran not a boolean; fixture.rom_sha256 absent or neither a
#                         string nor null; a claim field absent or null
#   unknown-status        status is not a vocabulary state (exact case)
#   pass-without-run      PASS with fixture.ran false
#   pass-without-digest   PASS without a 64-lowercase-hex fixture.rom_sha256
#   ran-state-mismatch    fixture.ran contradicts a `true` or `false` state row
#   missing-digest        a state row that requires a digest has null or a malformed one; an
#                         optional digest that is a malformed string
#   missing-reason        a state row that requires a reason has none or an empty one
#   missing-refusal       a state row that requires a refusal lacks a non-empty refusal.code
#                         or refusal.postcondition
#   bad-machine           claim.machine is not agb
#   bad-boot              claim.boot is not direct-start
#   bad-bios-provider     claim.bios_provider is outside the R1 grammar
#   bad-cart              claim.cart is not an object, its kind is not rom-only, or its
#                         rom_sha256 is neither null nor 64 lowercase hex
#   bad-input-timeline    kind other than none or script, or script without a 64-hex sha256
#   bad-evidence          claim.evidence not an array, empty, an item without fixture or
#                         provenance_class, a class outside the vocabulary, or a class that
#                         differs from the fixture's manifest row
#   claim-mismatch        claim.cart.rom_sha256 differs from fixture.rom_sha256 (two nulls
#                         are equal); no evidence item names fixture.id; fixture.rom_sha256
#                         differs from the digest of a url or in-tree manifest row (a git
#                         row pins a commit, so no digest tie exists for it yet)
#   unknown-fixture       fixture.id or an evidence fixture is not a manifest row id
#   unknown-dispute       a disputes item that is not a registry id
#   undisclosed-dispute   a manifest row for fixture.id or an evidence fixture carries a
#                         dispute that disputes lacks; disputes non-empty without the
#                         `disputed` label
#   label-without-dispute `disputed` label with an empty disputes list
#   bad-label             a label other than `disputed`, or a duplicate label
#   empty-limits          limits not an array, empty, or holding an item that is not a
#                         non-empty string
#   bad-semantic          semantic state/case_id has a noncanonical type or value
#   status-semantic-mismatch PASS is not a completed PASS mailbox, or GUEST_FAIL is not a
#                         completed FAIL mailbox
#   bad-time              time object/field is not a canonical decimal string
#   time-overflow         a time decimal string exceeds UINT64_MAX
#   cycles-exceed-budget  reported cycles are greater than budget_cycles
#   bad-termination       termination is missing, unnamed, or inconsistent with status
#   bad-trace             trace is not null or lacks a boolean complete field
#   incomplete-trace      a PASS or trace-output-full result claims an incomplete trace
# The json:, contract:, registry:, and manifest: codes come from the loader and the readers
# this file calls. On success the script prints `result: ok (<STATUS>)`.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CheckManifest.cmake")

# gbl__result_digest(<kind-out> <value-out> <doc> <key>...): classifies a digest slot.
# kind is ABSENT (key missing), NULL, HEX (64 lowercase hex), TEXT (any other string), or
# OTHER (any other type); value is the string, else empty.
function(gbl__result_digest kind_out value_out doc)
  gbl_json_type(ty "${doc}" ${ARGN})
  set(kind OTHER)
  set(value "")
  if("${ty}" STREQUAL "MISSING")
    set(kind ABSENT)
  elseif("${ty}" STREQUAL "NULL")
    set(kind NULL)
  elseif("${ty}" STREQUAL "STRING")
    string(JSON value GET "${doc}" ${ARGN})
    gbl__manifest_is_hex(is_hex "${value}" 64)
    if(is_hex)
      set(kind HEX)
    else()
      set(kind TEXT)
    endif()
  endif()
  set(${kind_out} "${kind}" PARENT_SCOPE)
  set(${value_out} "${value}" PARENT_SCOPE)
endfunction()

# gbl__result_present(<out> <doc> <key>...): TRUE when the key holds a value other than
# null.
function(gbl__result_present out doc)
  gbl_json_type(ty "${doc}" ${ARGN})
  if("${ty}" STREQUAL "MISSING" OR "${ty}" STREQUAL "NULL")
    set(${out} FALSE PARENT_SCOPE)
  else()
    set(${out} TRUE PARENT_SCOPE)
  endif()
endfunction()

# gbl__result_uint64(<out-valid> <out-value> <doc> <key>...): accepts only a JSON string
# containing a canonical base-10 uint64. No CMake math is performed on the value.
function(gbl__result_uint64 valid_out value_out doc)
  gbl_json_type(ty "${doc}" ${ARGN})
  set(valid FALSE)
  set(value "")
  if("${ty}" STREQUAL "STRING")
    string(JSON value GET "${doc}" ${ARGN})
    if("${value}" MATCHES "^(0|[1-9][0-9]*)$")
      string(LENGTH "${value}" digits)
      if(digits LESS 20)
        set(valid TRUE)
      elseif(digits EQUAL 20 AND
          NOT "${value}" STRGREATER "18446744073709551615")
        set(valid TRUE)
      endif()
    endif()
  endif()
  set(${valid_out} "${valid}" PARENT_SCOPE)
  set(${value_out} "${value}" PARENT_SCOPE)
endfunction()

# gbl__result_decimal_le(<out> <left> <right>): compare canonical nonnegative decimal
# strings by length and then lexicographically, avoiding CMake's signed integer range.
function(gbl__result_decimal_le out left right)
  string(LENGTH "${left}" left_digits)
  string(LENGTH "${right}" right_digits)
  if(left_digits LESS right_digits)
    set(${out} TRUE PARENT_SCOPE)
  elseif(left_digits GREATER right_digits)
    set(${out} FALSE PARENT_SCOPE)
  elseif("${left}" STREQUAL "${right}")
    set(${out} TRUE PARENT_SCOPE)
  elseif("${left}" STRLESS "${right}")
    set(${out} TRUE PARENT_SCOPE)
  else()
    set(${out} FALSE PARENT_SCOPE)
  endif()
endfunction()

function(gbl_result_check file manifest registry vocab)
  gbl_json_load(doc "${file}")
  set(m_ids "${${manifest}_IDS}")
  set(r_ids "${${registry}_IDS}")
  set(classes "${${vocab}_CLASSES}")
  set(states "${${vocab}_STATES}")
  set(status "")

  # ---- required top-level keys -------------------------------------------------
  foreach(key IN ITEMS schema fixture status claim disputes labels limits semantic time
      termination)
    gbl__result_present(has_${key} "${doc}" ${key})
    if(NOT has_${key})
      gbl_finding(result missing-field "${key}")
    endif()
  endforeach()
  gbl_json_type(trace_top_type "${doc}" trace)
  if("${trace_top_type}" STREQUAL "MISSING")
    set(has_trace FALSE)
    gbl_finding(result missing-field "trace")
  else()
    set(has_trace TRUE)
  endif()

  if(has_schema)
    gbl_json_string(schema "${doc}" schema)
    if(NOT "${schema}" STREQUAL "gamebloke.result/1")
      gbl_finding(result bad-schema "${file}")
    endif()
  endif()

  # ---- guest semantic observation ---------------------------------------------
  set(semantic_state "")
  set(semantic_case_id "")
  set(semantic_case_kind "MISSING")
  if(has_semantic)
    gbl_json_type(semantic_type "${doc}" semantic)
    if(NOT "${semantic_type}" STREQUAL "OBJECT")
      gbl_finding(result bad-semantic "semantic: not an object")
    else()
      gbl_json_type(semantic_state_type "${doc}" semantic state)
      if("${semantic_state_type}" STREQUAL "STRING")
        string(JSON semantic_state GET "${doc}" semantic state)
        if(NOT "${semantic_state}" MATCHES "^(absent|pending|pass|fail|malformed)$")
          gbl_finding(result bad-semantic "semantic.state")
        endif()
      elseif("${semantic_state_type}" STREQUAL "MISSING" OR
          "${semantic_state_type}" STREQUAL "NULL")
        gbl_finding(result missing-field "semantic.state")
      else()
        gbl_finding(result bad-semantic "semantic.state must be a string")
      endif()

      gbl_json_type(semantic_case_kind "${doc}" semantic case_id)
      if("${semantic_case_kind}" STREQUAL "STRING")
        string(JSON semantic_case_id GET "${doc}" semantic case_id)
        if(NOT "${semantic_case_id}" MATCHES "^(0|[1-9][0-9]*)$")
          gbl_finding(result bad-semantic "semantic.case_id")
        else()
          string(LENGTH "${semantic_case_id}" semantic_case_digits)
          if(semantic_case_digits GREATER 10 OR
              (semantic_case_digits EQUAL 10 AND
               "${semantic_case_id}" STRGREATER "4294967295"))
            gbl_finding(result bad-semantic "semantic.case_id exceeds uint32")
          endif()
        endif()
      elseif(NOT "${semantic_case_kind}" STREQUAL "NULL" AND
          NOT "${semantic_case_kind}" STREQUAL "MISSING")
        gbl_finding(result bad-semantic "semantic.case_id must be a string or null")
      elseif("${semantic_case_kind}" STREQUAL "MISSING")
        gbl_finding(result missing-field "semantic.case_id")
      endif()

      if("${semantic_state}" STREQUAL "absent" AND
          NOT "${semantic_case_kind}" STREQUAL "NULL" AND
          NOT ("${semantic_case_kind}" STREQUAL "STRING" AND
               "${semantic_case_id}" STREQUAL "0"))
        gbl_finding(result bad-semantic "absent mailbox needs a null or zero case_id")
      elseif("${semantic_state}" MATCHES "^(pending|pass|fail|malformed)$" AND
          NOT "${semantic_case_kind}" STREQUAL "STRING")
        gbl_finding(result bad-semantic "${semantic_state} mailbox needs a string case_id")
      elseif("${semantic_state}" MATCHES "^(pass|fail)$" AND
          "${semantic_case_id}" STREQUAL "0")
        gbl_finding(result bad-semantic "completed mailbox needs a nonzero case_id")
      endif()
    endif()
  endif()

  # ---- exact machine time -------------------------------------------------------
  set(time_cycles "")
  set(time_instructions "")
  set(time_budget "")
  set(time_valid FALSE)
  if(has_time)
    gbl_json_type(time_type "${doc}" time)
    if(NOT "${time_type}" STREQUAL "OBJECT")
      gbl_finding(result bad-time "time: not an object")
    else()
      set(time_valid TRUE)
      foreach(field IN ITEMS cycles instructions budget_cycles)
        gbl__result_present(has_time_${field} "${doc}" time ${field})
        if(NOT has_time_${field})
          gbl_finding(result missing-field "time.${field}")
          set(time_valid FALSE)
          continue()
        endif()
        gbl__result_uint64(time_${field}_valid time_${field} "${doc}" time ${field})
        if(NOT time_${field}_valid)
          gbl_json_type(time_${field}_type "${doc}" time ${field})
          if("${time_${field}_type}" STREQUAL "STRING")
            string(JSON invalid_decimal GET "${doc}" time ${field})
            if("${invalid_decimal}" MATCHES "^(0|[1-9][0-9]*)$")
              string(LENGTH "${invalid_decimal}" invalid_digits)
              if(invalid_digits GREATER 20 OR
                  (invalid_digits EQUAL 20 AND
                   "${invalid_decimal}" STRGREATER "18446744073709551615"))
                gbl_finding(result time-overflow "time.${field}")
              else()
                gbl_finding(result bad-time "time.${field}")
              endif()
            else()
              gbl_finding(result bad-time "time.${field}")
            endif()
          else()
            gbl_finding(result bad-time "time.${field} must be a decimal string")
          endif()
          set(time_valid FALSE)
        endif()
      endforeach()
      if(time_valid)
        set(time_budget "${time_budget_cycles}")
        gbl__result_decimal_le(cycles_within_budget "${time_cycles}"
          "${time_budget}")
        if(NOT cycles_within_budget)
          gbl_finding(result cycles-exceed-budget
            "time.cycles exceeds time.budget_cycles")
        endif()
      endif()
    endif()
  endif()

  # ---- termination and trace observations -------------------------------------
  set(termination "")
  if(has_termination)
    gbl_json_type(termination_type "${doc}" termination)
    if("${termination_type}" STREQUAL "STRING")
      string(JSON termination GET "${doc}" termination)
      if(NOT "${termination}" MATCHES "^[a-z0-9]+(-[a-z0-9]+)*$")
        gbl_finding(result bad-termination "termination must be a named token")
      endif()
    else()
      gbl_finding(result bad-termination "termination must be a string")
    endif()
  endif()

  set(trace_complete_kind "MISSING")
  set(trace_complete "")
  if(has_trace)
    gbl_json_type(trace_type "${doc}" trace)
    if("${trace_type}" STREQUAL "OBJECT")
      gbl_json_type(trace_complete_kind "${doc}" trace complete)
      if("${trace_complete_kind}" STREQUAL "BOOLEAN")
        string(JSON trace_complete GET "${doc}" trace complete)
      else()
        gbl_finding(result bad-trace "trace.complete must be a boolean")
      endif()
    elseif(NOT "${trace_type}" STREQUAL "NULL")
      gbl_finding(result bad-trace "trace must be an object or null")
    endif()
  endif()

  # ---- fixture block -----------------------------------------------------------
  set(fixture_id "")
  set(ran "")
  set(fix_kind ABSENT)
  set(fix_value "")
  if(has_fixture)
    gbl_json_type(fty "${doc}" fixture)
    if(NOT "${fty}" STREQUAL "OBJECT")
      gbl_finding(result missing-field "fixture: not an object")
    else()
      gbl_json_string(fixture_id "${doc}" fixture id)
      if("${fixture_id}" STREQUAL "<missing>")
        set(fixture_id "")
        gbl_finding(result missing-field "fixture.id")
      elseif(NOT "${fixture_id}" IN_LIST m_ids)
        gbl_finding(result unknown-fixture "fixture.id ${fixture_id}")
      endif()
      gbl_json_type(rty "${doc}" fixture ran)
      if("${rty}" STREQUAL "BOOLEAN")
        string(JSON ran GET "${doc}" fixture ran)
      else()
        gbl_finding(result missing-field "fixture.ran")
      endif()
      gbl__result_digest(fix_kind fix_value "${doc}" fixture rom_sha256)
      if("${fix_kind}" STREQUAL "ABSENT" OR "${fix_kind}" STREQUAL "OTHER")
        gbl_finding(result missing-field "fixture.rom_sha256")
      endif()
      # Tie the digest to the manifest row (WR-04): a digest the row pins must be the one
      # the result reports. A git row pins a commit, not a digest, so nothing ties there.
      if("${fix_kind}" STREQUAL "HEX" AND NOT "${fixture_id}" STREQUAL ""
          AND "${fixture_id}" IN_LIST m_ids)
        set(row_digest "${${manifest}_ROW_${fixture_id}_SHA256}")
        if(NOT "${row_digest}" STREQUAL "" AND NOT "${fix_value}" STREQUAL "${row_digest}")
          gbl_finding(result claim-mismatch
            "fixture.rom_sha256 differs from the manifest digest for ${fixture_id}")
        endif()
      endif()
    endif()
  endif()

  # ---- status and the state rules ----------------------------------------------
  if(has_status)
    gbl_json_string(status "${doc}" status)
    if("${status}" STREQUAL "<missing>" OR NOT "${status}" IN_LIST states)
      gbl_finding(result unknown-status "status")
      set(status "")
    endif()
  endif()
  if(NOT "${status}" STREQUAL "")
    set(ran_rule "${${vocab}_STATE_${status}_RAN}")
    set(digest_rule "${${vocab}_STATE_${status}_DIGEST}")
    set(reason_rule "${${vocab}_STATE_${status}_REASON}")
    set(refusal_rule "${${vocab}_STATE_${status}_REFUSAL}")
    set(fix_known FALSE)
    if("${fix_kind}" STREQUAL "NULL" OR "${fix_kind}" STREQUAL "HEX" OR "${fix_kind}" STREQUAL "TEXT")
      set(fix_known TRUE)
    endif()
    if("${status}" STREQUAL "PASS")
      if("${ran}" STREQUAL "OFF")
        gbl_finding(result pass-without-run "fixture.ran is false")
      endif()
      if(fix_known AND NOT "${fix_kind}" STREQUAL "HEX")
        gbl_finding(result pass-without-digest "fixture.rom_sha256")
      endif()
    else()
      if("${ran_rule}" STREQUAL "true" AND "${ran}" STREQUAL "OFF")
        gbl_finding(result ran-state-mismatch "${status} needs fixture.ran true")
      elseif("${ran_rule}" STREQUAL "false" AND "${ran}" STREQUAL "ON")
        gbl_finding(result ran-state-mismatch "${status} needs fixture.ran false")
      endif()
      if("${digest_rule}" STREQUAL "required" AND fix_known AND NOT "${fix_kind}" STREQUAL "HEX")
        gbl_finding(result missing-digest "${status} needs fixture.rom_sha256")
      elseif("${digest_rule}" STREQUAL "optional" AND "${fix_kind}" STREQUAL "TEXT")
        gbl_finding(result missing-digest "fixture.rom_sha256 is malformed")
      endif()
    endif()
    if("${reason_rule}" STREQUAL "required")
      gbl_json_string(reason "${doc}" reason)
      if("${reason}" STREQUAL "<missing>")
        gbl_finding(result missing-reason "${status} needs reason")
      endif()
    endif()
    if("${refusal_rule}" STREQUAL "required")
      gbl_json_string(rcode "${doc}" refusal code)
      gbl_json_string(rpost "${doc}" refusal postcondition)
      if("${rcode}" STREQUAL "<missing>")
        gbl_finding(result missing-refusal "${status} needs refusal.code")
      endif()
      if("${rpost}" STREQUAL "<missing>")
        gbl_finding(result missing-refusal "${status} needs refusal.postcondition")
      endif()
    endif()
  endif()

  # Bind evidence status to the actual guest mailbox and the bounded run's termination.
  gbl_json_string(result_reason "${doc}" reason)
  if("${status}" STREQUAL "PASS")
    if(NOT "${semantic_state}" STREQUAL "pass")
      gbl_finding(result status-semantic-mismatch "PASS requires semantic.state pass")
    endif()
    if("${semantic_case_kind}" STREQUAL "STRING" AND
        "${semantic_case_id}" STREQUAL "0")
      gbl_finding(result status-semantic-mismatch
        "PASS requires a nonzero semantic.case_id")
    endif()
    if(NOT "${termination}" STREQUAL "mailbox-complete")
      gbl_finding(result bad-termination "PASS requires mailbox-complete")
    endif()
    if(time_valid AND ("${time_cycles}" STREQUAL "0" OR
        "${time_instructions}" STREQUAL "0"))
      gbl_finding(result status-semantic-mismatch "PASS requires executed guest progress")
    endif()
  elseif("${status}" STREQUAL "GUEST_FAIL")
    if(NOT "${semantic_state}" STREQUAL "fail")
      gbl_finding(result status-semantic-mismatch
        "GUEST_FAIL requires semantic.state fail")
    endif()
    if(NOT "${termination}" STREQUAL "mailbox-complete")
      gbl_finding(result bad-termination "GUEST_FAIL requires mailbox-complete")
    endif()
  elseif("${status}" STREQUAL "GUEST_STOP")
    if("${result_reason}" MATCHES "^no-bios-" OR
        "${termination}" MATCHES "^no-bios-")
      if(NOT "${result_reason}" MATCHES "^no-bios-" OR
          NOT "${termination}" STREQUAL "${result_reason}")
        gbl_finding(result bad-termination
          "no-BIOS GUEST_STOP termination must match reason")
      endif()
    endif()
  elseif("${status}" STREQUAL "TIMEOUT" AND
      NOT "${termination}" STREQUAL "timeout")
    gbl_finding(result bad-termination "TIMEOUT requires timeout termination")
  elseif("${status}" STREQUAL "CANCELLED" AND
      NOT "${termination}" STREQUAL "cancelled")
    gbl_finding(result bad-termination "CANCELLED requires cancelled termination")
  endif()

  set(trace_output_full FALSE)
  if("${status}" STREQUAL "HARNESS_FAIL" AND
      "${result_reason}" STREQUAL "trace-output-full")
    set(trace_output_full TRUE)
    if(NOT "${termination}" STREQUAL "trace-output-full")
      gbl_finding(result bad-termination
        "trace-output-full HARNESS_FAIL requires matching termination")
    endif()
    if(NOT "${trace_complete_kind}" STREQUAL "BOOLEAN" OR
        NOT "${trace_complete}" STREQUAL "OFF")
      gbl_finding(result incomplete-trace
        "trace-output-full HARNESS_FAIL requires trace.complete false")
    endif()
  endif()
  if("${termination}" STREQUAL "trace-output-full" AND NOT trace_output_full)
    gbl_finding(result bad-termination
      "trace-output-full requires HARNESS_FAIL with matching reason")
  endif()
  if("${trace_complete_kind}" STREQUAL "BOOLEAN" AND
      "${trace_complete}" STREQUAL "OFF" AND NOT trace_output_full)
    if("${status}" STREQUAL "PASS")
      gbl_finding(result incomplete-trace "PASS cannot have an incomplete trace")
    else()
      gbl_finding(result incomplete-trace
        "incomplete trace requires trace-output-full HARNESS_FAIL")
    endif()
  endif()

  # ---- claim tuple -------------------------------------------------------------
  set(evid_fixtures "")
  if(has_claim)
    gbl_json_type(cty "${doc}" claim)
    if(NOT "${cty}" STREQUAL "OBJECT")
      gbl_finding(result missing-field "claim: not an object")
    else()
      foreach(field IN LISTS ${vocab}_CLAIM_FIELDS)
        gbl__result_present(has_claim_${field} "${doc}" claim ${field})
        if(NOT has_claim_${field})
          gbl_finding(result missing-field "claim.${field}")
        endif()
      endforeach()

      if(has_claim_machine)
        gbl_json_string(machine "${doc}" claim machine)
        if(NOT "${machine}" STREQUAL "agb")
          gbl_finding(result bad-machine "claim.machine")
        endif()
      endif()
      if(has_claim_boot)
        gbl_json_string(boot "${doc}" claim boot)
        if(NOT "${boot}" STREQUAL "direct-start")
          gbl_finding(result bad-boot "claim.boot")
        endif()
      endif()
      if(has_claim_bios_provider)
        gbl_json_string(bios "${doc}" claim bios_provider)
        gbl_bios_provider_valid(bios_ok "${bios}")
        if(NOT bios_ok)
          gbl_finding(result bad-bios-provider "claim.bios_provider")
        endif()
      endif()

      if(has_claim_cart)
        gbl_json_type(cart_ty "${doc}" claim cart)
        if(NOT "${cart_ty}" STREQUAL "OBJECT")
          gbl_finding(result bad-cart "claim.cart: not an object")
        else()
          gbl_json_string(cart_kind "${doc}" claim cart kind)
          if(NOT "${cart_kind}" STREQUAL "rom-only")
            gbl_finding(result bad-cart "claim.cart.kind")
          endif()
          gbl__result_digest(cart_dk cart_dv "${doc}" claim cart rom_sha256)
          if(NOT "${cart_dk}" STREQUAL "NULL" AND NOT "${cart_dk}" STREQUAL "HEX")
            gbl_finding(result bad-cart "claim.cart.rom_sha256")
          endif()
          set(cart_cmp FALSE)
          set(fix_cmp FALSE)
          if("${cart_dk}" STREQUAL "NULL" OR "${cart_dk}" STREQUAL "HEX" OR "${cart_dk}" STREQUAL "TEXT")
            set(cart_cmp TRUE)
          endif()
          if("${fix_kind}" STREQUAL "NULL" OR "${fix_kind}" STREQUAL "HEX" OR "${fix_kind}" STREQUAL "TEXT")
            set(fix_cmp TRUE)
          endif()
          if(cart_cmp AND fix_cmp)
            set(cart_token "${cart_dv}")
            set(fix_token "${fix_value}")
            if("${cart_dk}" STREQUAL "NULL")
              set(cart_token "<null>")
            endif()
            if("${fix_kind}" STREQUAL "NULL")
              set(fix_token "<null>")
            endif()
            if(NOT "${cart_token}" STREQUAL "${fix_token}")
              gbl_finding(result claim-mismatch "claim.cart.rom_sha256 differs from fixture.rom_sha256")
            endif()
          endif()
        endif()
      endif()

      if(has_claim_input_timeline)
        gbl_json_type(it_ty "${doc}" claim input_timeline)
        if(NOT "${it_ty}" STREQUAL "OBJECT")
          gbl_finding(result bad-input-timeline "claim.input_timeline: not an object")
        else()
          gbl_json_string(it_kind "${doc}" claim input_timeline kind)
          if("${it_kind}" STREQUAL "script")
            gbl__result_digest(it_dk it_dv "${doc}" claim input_timeline sha256)
            if(NOT "${it_dk}" STREQUAL "HEX")
              gbl_finding(result bad-input-timeline "claim.input_timeline.sha256")
            endif()
          elseif(NOT "${it_kind}" STREQUAL "none")
            gbl_finding(result bad-input-timeline "claim.input_timeline.kind")
          endif()
        endif()
      endif()

      if(has_claim_evidence)
        gbl_json_length(elen "${doc}" claim evidence)
        if(elen LESS 0)
          gbl_finding(result bad-evidence "claim.evidence: not an array")
        elseif(elen EQUAL 0)
          gbl_finding(result bad-evidence "claim.evidence: empty")
        else()
          math(EXPR elast "${elen}-1")
          foreach(i RANGE ${elast})
            gbl_json_type(ity "${doc}" claim evidence ${i})
            if(NOT "${ity}" STREQUAL "OBJECT")
              gbl_finding(result bad-evidence "claim.evidence[${i}]: not an object")
              continue()
            endif()
            gbl_json_string(ef "${doc}" claim evidence ${i} fixture)
            gbl_json_string(ec "${doc}" claim evidence ${i} provenance_class)
            set(class_ok FALSE)
            if("${ec}" STREQUAL "<missing>")
              gbl_finding(result bad-evidence "claim.evidence[${i}]: provenance_class")
            elseif(NOT "${ec}" IN_LIST classes)
              gbl_finding(result bad-evidence "claim.evidence[${i}]: class ${ec}")
            else()
              set(class_ok TRUE)
            endif()
            if("${ef}" STREQUAL "<missing>")
              gbl_finding(result bad-evidence "claim.evidence[${i}]: fixture")
              continue()
            endif()
            list(APPEND evid_fixtures "${ef}")
            if(NOT "${ef}" IN_LIST m_ids)
              gbl_finding(result unknown-fixture "claim.evidence[${i}] ${ef}")
            elseif(class_ok AND NOT "${ec}" STREQUAL "${${manifest}_ROW_${ef}_CLASS}")
              gbl_finding(result bad-evidence "claim.evidence[${i}]: class differs from the manifest row for ${ef}")
            endif()
          endforeach()
          if(NOT "${fixture_id}" STREQUAL "" AND NOT "${fixture_id}" IN_LIST evid_fixtures)
            gbl_finding(result claim-mismatch "no evidence item names ${fixture_id}")
          endif()
        endif()
      endif()
    endif()
  endif()

  # ---- disputes, labels, and disclosure ----------------------------------------
  set(listed "")
  set(disputes_count 0)
  set(disputes_ok FALSE)
  if(has_disputes)
    gbl_json_length(disputes_count "${doc}" disputes)
    if(disputes_count LESS 0)
      gbl_finding(result missing-field "disputes: not an array")
      set(disputes_count 0)
    else()
      set(disputes_ok TRUE)
      if(disputes_count GREATER 0)
        math(EXPR dlast "${disputes_count}-1")
        foreach(i RANGE ${dlast})
          gbl_json_string(did "${doc}" disputes ${i})
          if("${did}" STREQUAL "<missing>" OR NOT "${did}" IN_LIST r_ids)
            gbl_finding(result unknown-dispute "disputes[${i}]")
          else()
            list(APPEND listed "${did}")
          endif()
        endforeach()
      endif()
    endif()
  endif()

  set(has_disputed_label FALSE)
  set(labels_ok FALSE)
  if(has_labels)
    gbl_json_length(labels_count "${doc}" labels)
    if(labels_count LESS 0)
      gbl_finding(result missing-field "labels: not an array")
    else()
      set(labels_ok TRUE)
      set(seen "")
      if(labels_count GREATER 0)
        math(EXPR llast "${labels_count}-1")
        foreach(i RANGE ${llast})
          gbl_json_string(lab "${doc}" labels ${i})
          if("${lab}" STREQUAL "<missing>" OR NOT "${lab}" STREQUAL "disputed")
            gbl_finding(result bad-label "labels[${i}]")
          elseif("${lab}" IN_LIST seen)
            gbl_finding(result bad-label "labels[${i}]: duplicate ${lab}")
          else()
            list(APPEND seen "${lab}")
            set(has_disputed_label TRUE)
          endif()
        endforeach()
      endif()
    endif()
  endif()

  if(disputes_ok AND labels_ok)
    set(need "")
    foreach(fid IN ITEMS "${fixture_id}" ${evid_fixtures})
      if(NOT "${fid}" STREQUAL "" AND "${fid}" IN_LIST m_ids)
        set(row_dispute "${${manifest}_ROW_${fid}_DISPUTE}")
        if(NOT "${row_dispute}" STREQUAL "")
          list(APPEND need "${row_dispute}")
        endif()
      endif()
    endforeach()
    list(REMOVE_DUPLICATES need)
    foreach(n IN LISTS need)
      if(NOT "${n}" IN_LIST listed)
        gbl_finding(result undisclosed-dispute "${n} is not in disputes")
      endif()
    endforeach()
    if(disputes_count GREATER 0 AND NOT has_disputed_label)
      gbl_finding(result undisclosed-dispute "disputes is non-empty but labels lacks disputed")
    endif()
    if(has_disputed_label AND disputes_count EQUAL 0)
      gbl_finding(result label-without-dispute "disputed label with no dispute id")
    endif()
  endif()

  # ---- limits ------------------------------------------------------------------
  if(has_limits)
    gbl_json_length(limits_count "${doc}" limits)
    if(limits_count LESS 1)
      gbl_finding(result empty-limits "limits")
    else()
      math(EXPR limlast "${limits_count}-1")
      foreach(i RANGE ${limlast})
        gbl_json_string(lim "${doc}" limits ${i})
        if("${lim}" STREQUAL "<missing>")
          gbl_finding(result empty-limits "limits[${i}]")
        endif()
      endforeach()
    endif()
  endif()

  set(GBL_RESULT_STATUS "${status}" PARENT_SCOPE)
endfunction()

# ---- script-mode entry (inert when include()d) --------------------------------
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  foreach(var RESULT MANIFEST REGISTRY VOCAB)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
      message(FATAL_ERROR "result: usage: -D${var}= is required")
    endif()
  endforeach()
  gbl_contract_read(V "${VOCAB}")
  gbl_registry_read(R "${REGISTRY}" V)
  gbl_manifest_read(M "${MANIFEST}" R V)
  gbl_result_check("${RESULT}" M R V)
  gbl_findings_finish(result "ok (${GBL_RESULT_STATUS})")
endif()
