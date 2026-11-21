# Result document validator (D-02, D-05, D-09, D-10, D-11, D-12, PROF-02, EVID-05, EVID-06).
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
#                         differs from the runtime ROM digest declared by a url/in-tree
#                         source.sha256 or an optional git build.artifact.sha256
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
#   bad-time-model        the timing model is not gamebloke-timer-model-1
#   bad-time-qualification the timing qualification is not project-model-only
#   time-overflow         a time decimal string exceeds UINT64_MAX
#   time-sum-overflow     bus/internal components sum above UINT64_MAX
#   bad-time-components   time.cycles differs from the component sum
#   partial-phase9-extension one or more time.dma_* members exists without phase9
#   bad-phase9-*           phase9 model, types, channel progress, timing, provenance,
#                          ownership, latch disclosure, or Prefetch statement is invalid
#   partial-phase10-extension `phase10` and `raw_video` are not both absent or both present
#   bad-phase10-*           PPU/video model, profile-bound capability tuple, frame validity,
#                          policy provenance, output state, digest, or HBlank-dependent
#                          dispute disclosure is inconsistent
#   bad-timer0-request    Timer 0 IF observation has an invalid type or run-state
#   cycles-exceed-budget  reported cycles are greater than budget_cycles
#   bad-termination       termination is missing, unnamed, or inconsistent with status
#   bad-trace             trace is not null or lacks a boolean complete field
#   incomplete-trace      a PASS or trace-output-full result claims an incomplete trace
#   bad-observation       register observation is missing, malformed, unbound, or disagrees
#                         with the manifest channel, runtime digest, status, value, or time
# The json:, contract:, registry:, and manifest: codes come from the loader and the readers
# this file calls. On success the script prints `result: ok (<STATUS>)`.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CheckManifest.cmake")
set(GBL_RESULT_AGB_PROFILE_PATH "${CMAKE_CURRENT_LIST_DIR}/../profiles/agb.json")

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

# gbl__result_uint16(<out-valid> <out-value> <doc> <maximum> <key>...): accepts a
# canonical nonnegative JSON integer within the explicitly bounded hardware field.
# These fields are small enough for CMake's signed integer math after lexical checks.
function(gbl__result_uint16 valid_out value_out doc maximum)
  gbl_json_type(ty "${doc}" ${ARGN})
  set(valid FALSE)
  set(value "")
  if("${ty}" STREQUAL "NUMBER")
    string(JSON value GET "${doc}" ${ARGN})
    if("${value}" MATCHES "^(0|[1-9][0-9]*)$")
      string(LENGTH "${value}" digits)
      string(LENGTH "${maximum}" maximum_digits)
      if(digits LESS maximum_digits OR
          (digits EQUAL maximum_digits AND NOT "${value}" STRGREATER "${maximum}"))
        set(valid TRUE)
      endif()
    endif()
  endif()
  set(${valid_out} "${valid}" PARENT_SCOPE)
  set(${value_out} "${value}" PARENT_SCOPE)
endfunction()

# gbl__result_uint32(<out-valid> <out-value> <doc> <maximum> <key>...): bounded
# JSON integer parser for channel progress counters. Lexical comparison keeps values
# outside CMake's signed range from reaching math(EXPR).
function(gbl__result_uint32 valid_out value_out doc maximum)
  gbl_json_type(ty "${doc}" ${ARGN})
  set(valid FALSE)
  set(value "")
  if("${ty}" STREQUAL "NUMBER")
    string(JSON value GET "${doc}" ${ARGN})
    if("${value}" MATCHES "^(0|[1-9][0-9]*)$")
      string(LENGTH "${value}" digits)
      string(LENGTH "${maximum}" maximum_digits)
      if(digits LESS maximum_digits OR
          (digits EQUAL maximum_digits AND NOT "${value}" STRGREATER "${maximum}"))
        set(valid TRUE)
      endif()
    endif()
  endif()
  set(${valid_out} "${valid}" PARENT_SCOPE)
  set(${value_out} "${value}" PARENT_SCOPE)
endfunction()

function(gbl__result_phase9_decimal valid_out value_out doc label)
  gbl_json_type(ty "${doc}" ${ARGN})
  set(valid FALSE)
  set(value "")
  if("${ty}" STREQUAL "MISSING" OR "${ty}" STREQUAL "NULL")
    gbl_finding(result missing-field "${label}")
  else()
    gbl__result_uint64(valid value "${doc}" ${ARGN})
    if(NOT valid)
      if("${ty}" STREQUAL "STRING")
        string(JSON invalid_decimal GET "${doc}" ${ARGN})
        if("${invalid_decimal}" MATCHES "^(0|[1-9][0-9]*)$")
          string(LENGTH "${invalid_decimal}" digits)
          if(digits GREATER 20 OR
              (digits EQUAL 20 AND
               "${invalid_decimal}" STRGREATER "18446744073709551615"))
            gbl_finding(result time-overflow "${label}")
          else()
            gbl_finding(result bad-phase9-type
              "${label} must be a canonical uint64 decimal string")
          endif()
        else()
          gbl_finding(result bad-phase9-type
            "${label} must be a canonical uint64 decimal string")
        endif()
      else()
        gbl_finding(result bad-phase9-type
          "${label} must be a canonical uint64 decimal string")
      endif()
    endif()
  endif()
  set(${valid_out} "${valid}" PARENT_SCOPE)
  set(${value_out} "${value}" PARENT_SCOPE)
endfunction()

# Validate the additive DMA observation and return its timing components for the
# machine-time equation. Absence is the complete historical result/1 path; any DMA
# time member without phase9 is a partial extension and fails closed.
function(gbl__result_phase9_check doc time_cycles present_out valid_out)
  set(phase9_present FALSE)
  set(dma_valid TRUE)
  set(dma_startup_cycles "0")
  set(dma_source_cycles "0")
  set(dma_destination_cycles "0")
  set(dma_completed_units "0")
  set(dma_pause_cycles "0")
  gbl_json_type(phase9_type "${doc}" phase9)
  if(NOT "${phase9_type}" STREQUAL "MISSING")
    set(phase9_present TRUE)
  endif()

  set(time_dma_present FALSE)
  foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
      dma_destination_cycles dma_completed_units dma_pause_cycles)
    gbl_json_type(time_${field}_type "${doc}" time ${field})
    if(NOT "${time_${field}_type}" STREQUAL "MISSING")
      set(time_dma_present TRUE)
    endif()
  endforeach()
  if(NOT phase9_present)
    if(time_dma_present)
      gbl_finding(result partial-phase9-extension
        "time DMA fields require phase9")
      set(dma_valid FALSE)
    endif()
    set(${present_out} FALSE PARENT_SCOPE)
    set(${valid_out} "${dma_valid}" PARENT_SCOPE)
    set(phase9_dma_startup_cycles "0" PARENT_SCOPE)
    set(phase9_dma_source_cycles "0" PARENT_SCOPE)
    set(phase9_dma_destination_cycles "0" PARENT_SCOPE)
    set(phase9_dma_completed_units "0" PARENT_SCOPE)
    set(phase9_dma_pause_cycles "0" PARENT_SCOPE)
    return()
  endif()

  foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
      dma_destination_cycles dma_completed_units dma_pause_cycles)
    gbl__result_phase9_decimal(time_${field}_valid time_${field} "${doc}"
      "time.${field}" time ${field})
    if(NOT time_${field}_valid)
      set(dma_valid FALSE)
    endif()
    set(${field} "${time_${field}}")
  endforeach()
  if(time_dma_pause_cycles_valid)
    gbl__result_decimal_le(pause_within_elapsed "${time_dma_pause_cycles}"
      "${time_cycles}")
    if(NOT pause_within_elapsed)
      gbl_finding(result bad-phase9-timing
        "time.dma_pause_cycles exceeds time.cycles")
      set(dma_valid FALSE)
    endif()
  endif()
  if(time_dma_pause_cycles_valid AND time_dma_startup_cycles_valid AND
      time_dma_source_cycles_valid AND time_dma_destination_cycles_valid)
    gbl__result_decimal_add(dma_pause_components
      "${time_dma_startup_cycles}" "${time_dma_source_cycles}")
    gbl__result_decimal_add(dma_pause_components "${dma_pause_components}"
      "${time_dma_destination_cycles}")
    if(NOT "${time_dma_pause_cycles}" STREQUAL "${dma_pause_components}")
      gbl_finding(result bad-phase9-timing
        "time.dma_pause_cycles differs from DMA timing components")
      set(dma_valid FALSE)
    endif()
  endif()

  if(NOT "${phase9_type}" STREQUAL "OBJECT")
    gbl_finding(result bad-phase9-type "phase9 must be an object")
    set(dma_valid FALSE)
    set(${present_out} TRUE PARENT_SCOPE)
    set(${valid_out} FALSE PARENT_SCOPE)
    foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
        dma_destination_cycles dma_completed_units dma_pause_cycles)
      set(phase9_${field} "${${field}}" PARENT_SCOPE)
    endforeach()
    return()
  endif()

  foreach(field IN ITEMS model observed waitcnt bus_owner cpu_paused
      machine_cycles channels timing waitcnt_policy startup_policy latch_policy
      prefetch)
    gbl_json_type(phase9_${field}_type "${doc}" phase9 ${field})
    if("${phase9_${field}_type}" STREQUAL "MISSING" OR
        (("${field}" STREQUAL "model" OR
          "${field}" STREQUAL "observed" OR
          "${field}" STREQUAL "timing" OR
          "${field}" STREQUAL "waitcnt_policy" OR
          "${field}" STREQUAL "startup_policy" OR
          "${field}" STREQUAL "latch_policy" OR
          "${field}" STREQUAL "prefetch") AND
         "${phase9_${field}_type}" STREQUAL "NULL"))
      gbl_finding(result missing-field "phase9.${field}")
      set(dma_valid FALSE)
    endif()
  endforeach()

  gbl_json_type(model_type "${doc}" phase9 model)
  if("${model_type}" STREQUAL "STRING")
    string(JSON model GET "${doc}" phase9 model)
    if(NOT "${model}" STREQUAL "gamebloke-dma-observation-1")
      gbl_finding(result bad-phase9-model "phase9.model")
      set(dma_valid FALSE)
    endif()
  elseif(NOT "${model_type}" STREQUAL "MISSING" AND
      NOT "${model_type}" STREQUAL "NULL")
    gbl_finding(result bad-phase9-type "phase9.model must be a string")
    set(dma_valid FALSE)
  endif()

  set(observed_valid FALSE)
  set(observed FALSE)
  gbl_json_type(observed_type "${doc}" phase9 observed)
  if("${observed_type}" STREQUAL "BOOLEAN")
    set(observed_valid TRUE)
    string(JSON observed GET "${doc}" phase9 observed)
  else()
    gbl_finding(result bad-phase9-type "phase9.observed must be boolean")
    set(dma_valid FALSE)
  endif()

  gbl_json_type(timing_type "${doc}" phase9 timing)
  if(NOT "${timing_type}" STREQUAL "OBJECT")
    gbl_finding(result bad-phase9-type "phase9.timing must be an object")
    set(dma_valid FALSE)
  else()
    foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
        dma_destination_cycles dma_completed_units dma_pause_cycles)
      gbl__result_phase9_decimal(phase9_${field}_valid phase9_${field} "${doc}"
        "phase9.timing.${field}" phase9 timing ${field})
      if(NOT phase9_${field}_valid)
        set(dma_valid FALSE)
      elseif(time_${field}_valid AND
          NOT "${phase9_${field}}" STREQUAL "${time_${field}}")
        gbl_finding(result bad-phase9-timing
          "phase9.timing.${field} differs from time.${field}")
        set(dma_valid FALSE)
      endif()
    endforeach()
  endif()

  # WAITCNT costs have manual provenance for the documented Game Pak wait states;
  # the 128 KiB boundary remains community documentation and all hardware claims
  # remain UNKNOWN. Other region values are explicitly selected project policy.
  gbl_json_type(waitcnt_policy_type "${doc}" phase9 waitcnt_policy)
  if(NOT "${waitcnt_policy_type}" STREQUAL "OBJECT")
    gbl_finding(result bad-phase9-policy "phase9.waitcnt_policy must be an object")
    set(dma_valid FALSE)
  else()
    foreach(pair IN ITEMS
        "manual_waits;nintendo-agb-programming-manual-v1.1-sections-3.3-3.3.2"
        "other_region_costs;project-selected"
        "128k_boundary_source;gbatek-3.06-community-documentation"
        "hardware_qualification;UNKNOWN")
      list(GET pair 0 field)
      list(GET pair 1 expected)
      gbl_json_type(type "${doc}" phase9 waitcnt_policy ${field})
      if(NOT "${type}" STREQUAL "STRING")
        gbl_finding(result bad-phase9-policy
          "phase9.waitcnt_policy.${field} must be a string")
        set(dma_valid FALSE)
      else()
        string(JSON actual GET "${doc}" phase9 waitcnt_policy ${field})
        if(NOT "${actual}" STREQUAL "${expected}")
          gbl_finding(result bad-phase9-policy
            "phase9.waitcnt_policy.${field}")
          set(dma_valid FALSE)
        endif()
      endif()
    endforeach()
  endif()

  gbl_json_type(startup_policy_type "${doc}" phase9 startup_policy)
  if(NOT "${startup_policy_type}" STREQUAL "OBJECT")
    gbl_finding(result bad-phase9-policy "phase9.startup_policy must be an object")
    set(dma_valid FALSE)
  else()
    foreach(pair IN ITEMS
        "cycles_per_request;2" "provenance;project-selected"
        "hardware_qualification;UNKNOWN")
      list(GET pair 0 field)
      list(GET pair 1 expected)
      gbl_json_type(type "${doc}" phase9 startup_policy ${field})
      if(NOT "${type}" STREQUAL "STRING")
        gbl_finding(result bad-phase9-policy
          "phase9.startup_policy.${field} must be a string")
        set(dma_valid FALSE)
      else()
        string(JSON actual GET "${doc}" phase9 startup_policy ${field})
        if(NOT "${actual}" STREQUAL "${expected}")
          gbl_finding(result bad-phase9-policy
            "phase9.startup_policy.${field}")
          set(dma_valid FALSE)
        endif()
      endif()
    endforeach()
    gbl_json_type(activation_delay_type "${doc}"
      phase9 startup_policy manual_activation_delay)
    if(NOT "${activation_delay_type}" STREQUAL "STRING")
      gbl_finding(result bad-phase9-policy
        "phase9.startup_policy.manual_activation_delay must be a string")
      set(dma_valid FALSE)
    else()
      string(JSON activation_delay GET "${doc}"
        phase9 startup_policy manual_activation_delay)
      if(NOT "${activation_delay}" STREQUAL
          "described; conversion is selected")
        gbl_finding(result bad-phase9-policy
          "phase9.startup_policy.manual_activation_delay")
        set(dma_valid FALSE)
      endif()
    endif()
  endif()
  if(time_dma_startup_cycles_valid AND
      NOT "${time_dma_startup_cycles}" MATCHES "[02468]$")
    gbl_finding(result bad-phase9-timing
      "time.dma_startup_cycles must use the selected two-cycle request cost")
    set(dma_valid FALSE)
  endif()

  gbl_json_type(prefetch_type "${doc}" phase9 prefetch)
  if(NOT "${prefetch_type}" STREQUAL "STRING")
    gbl_finding(result bad-phase9-prefetch "phase9.prefetch must be a string")
    set(dma_valid FALSE)
  else()
    string(JSON prefetch GET "${doc}" phase9 prefetch)
    if(NOT "${prefetch}" STREQUAL "unmodeled")
      gbl_finding(result bad-phase9-prefetch "phase9.prefetch")
      set(dma_valid FALSE)
    endif()
  endif()

  # Observations are either all present for a run or all null when no machine
  # execution took place. This prevents a partial object from looking complete.
  set(channels_valid FALSE)
  set(any_active FALSE)
  set(any_active_or_pending FALSE)
  set(completed_sum "0")
  if(observed_valid AND observed)
    gbl_json_type(waitcnt_type "${doc}" phase9 waitcnt)
    gbl__result_uint16(waitcnt_valid waitcnt "${doc}" "65535" phase9 waitcnt)
    if(NOT waitcnt_valid)
      gbl_finding(result bad-phase9-type "phase9.waitcnt must be a uint16 integer")
      set(dma_valid FALSE)
    endif()

    gbl_json_type(owner_type "${doc}" phase9 bus_owner)
    if(NOT "${owner_type}" STREQUAL "STRING")
      gbl_finding(result bad-phase9-owner "phase9.bus_owner must be cpu or dma")
      set(dma_valid FALSE)
    else()
      string(JSON owner GET "${doc}" phase9 bus_owner)
      if(NOT "${owner}" MATCHES "^(cpu|dma)$")
        gbl_finding(result bad-phase9-owner "phase9.bus_owner")
        set(dma_valid FALSE)
      endif()
    endif()

    gbl_json_type(paused_type "${doc}" phase9 cpu_paused)
    if(NOT "${paused_type}" STREQUAL "BOOLEAN")
      gbl_finding(result bad-phase9-type "phase9.cpu_paused must be boolean")
      set(dma_valid FALSE)
    else()
      string(JSON paused GET "${doc}" phase9 cpu_paused)
      if(("${owner}" STREQUAL "dma" AND NOT paused) OR
          ("${owner}" STREQUAL "cpu" AND paused))
        gbl_finding(result bad-phase9-owner
          "phase9.cpu_paused must match the bus owner")
        set(dma_valid FALSE)
      endif()
    endif()

    gbl__result_phase9_decimal(machine_cycles_valid machine_cycles "${doc}"
      "phase9.machine_cycles" phase9 machine_cycles)
    if(NOT machine_cycles_valid)
      set(dma_valid FALSE)
    elseif(NOT "${machine_cycles}" STREQUAL "${time_cycles}")
      gbl_finding(result bad-phase9-timing
        "phase9.machine_cycles differs from time.cycles")
      set(dma_valid FALSE)
    endif()

    gbl_json_length(channel_count "${doc}" phase9 channels)
    if(NOT channel_count EQUAL 4)
      gbl_finding(result bad-phase9-channel
        "phase9.channels must contain exactly four channels")
      set(dma_valid FALSE)
    else()
      set(channels_valid TRUE)
      foreach(index RANGE 0 3)
        set(channel_valid TRUE)
        gbl_json_type(channel_type "${doc}" phase9 channels ${index})
        if(NOT "${channel_type}" STREQUAL "OBJECT")
          gbl_finding(result bad-phase9-channel
            "phase9.channels[${index}] must be an object")
          set(channel_valid FALSE)
        endif()

        gbl__result_uint16(channel_id_valid channel_id "${doc}" "3"
          phase9 channels ${index} channel)
        if(NOT channel_id_valid OR NOT "${channel_id}" STREQUAL "${index}")
          gbl_finding(result bad-phase9-channel
            "phase9.channels[${index}].channel must match its index")
          set(channel_valid FALSE)
        endif()

        foreach(address IN ITEMS source_address destination_address
            current_source current_destination read_latch)
          gbl_json_type(address_type "${doc}" phase9 channels ${index} ${address})
          if(NOT "${address_type}" STREQUAL "STRING")
            gbl_finding(result bad-phase9-type
              "phase9.channels[${index}].${address} must be an eight-digit hex string")
            set(channel_valid FALSE)
          else()
            string(JSON address_value GET "${doc}" phase9 channels ${index} ${address})
            if(NOT "${address_value}" MATCHES "^0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]$")
              gbl_finding(result bad-phase9-type
                "phase9.channels[${index}].${address} must be an eight-digit hex string")
              set(channel_valid FALSE)
            endif()
          endif()
        endforeach()

        if(index EQUAL 3)
          set(count_maximum "65535")
          set(zero_count_limit "65536")
        else()
          set(count_maximum "16383")
          set(zero_count_limit "16384")
        endif()
        gbl__result_uint16(count_valid channel_count_value "${doc}"
          "${count_maximum}" phase9 channels ${index} count)
        gbl__result_uint16(control_valid control_value "${doc}" "65535"
          phase9 channels ${index} control)
        if(NOT count_valid OR NOT control_valid)
          gbl_finding(result bad-phase9-type
            "phase9.channels[${index}] count/control must be bounded uint16 integers")
          set(channel_valid FALSE)
        endif()
        if(NOT count_valid)
          set(channel_count_value "0")
        endif()
        if("${count_valid}" AND "${channel_count_value}" STREQUAL "0")
          set(remaining_maximum "${zero_count_limit}")
        else()
          set(remaining_maximum "${channel_count_value}")
        endif()
        gbl__result_uint32(remaining_valid remaining "${doc}"
          "${remaining_maximum}" phase9 channels ${index} remaining)
        gbl__result_phase9_decimal(completed_valid completed "${doc}"
          "phase9.channels[${index}].completed_units"
          phase9 channels ${index} completed_units)
        if(NOT remaining_valid OR NOT completed_valid)
          if(NOT remaining_valid)
            gbl_finding(result bad-phase9-progress
              "phase9.channels[${index}].remaining is outside its transfer bound")
          endif()
          set(channel_valid FALSE)
        endif()

        foreach(flag IN ITEMS pending armed active startup_complete)
          gbl_json_type(flag_${flag}_type "${doc}" phase9 channels ${index} ${flag})
          if(NOT "${flag_${flag}_type}" STREQUAL "BOOLEAN")
            gbl_finding(result bad-phase9-type
              "phase9.channels[${index}].${flag} must be boolean")
            set(channel_valid FALSE)
          else()
            string(JSON flag_${flag} GET "${doc}" phase9 channels ${index} ${flag})
          endif()
        endforeach()

        if(channel_valid)
          math(EXPR control_enable "${control_value} & 32768")
          math(EXPR start_mode "(${control_value} >> 12) & 3")
          if((flag_active AND ("${remaining}" STREQUAL "0" OR
                  NOT flag_startup_complete)) OR
              (flag_startup_complete AND NOT flag_active) OR
              (flag_pending AND ("${remaining}" STREQUAL "0" OR flag_active OR
                  flag_armed OR flag_startup_complete OR NOT start_mode EQUAL 0)) OR
              (flag_armed AND ("${remaining}" STREQUAL "0" OR flag_active OR
                  flag_pending OR flag_startup_complete OR start_mode EQUAL 0)) OR
              ("${remaining}" GREATER 0 AND NOT flag_active AND NOT flag_pending AND
                  NOT flag_armed) OR
              ((flag_active OR flag_pending OR flag_armed) AND
                  NOT control_enable EQUAL 32768) OR
              ("${remaining}" EQUAL 0 AND
                  (flag_active OR flag_pending OR flag_armed OR flag_startup_complete)))
            gbl_finding(result bad-phase9-progress
              "phase9.channels[${index}] has inconsistent remaining and activity flags")
            set(channel_valid FALSE)
          endif()
          if(flag_active)
            set(any_active TRUE)
          endif()
          if(flag_active OR flag_pending)
            set(any_active_or_pending TRUE)
          endif()
          gbl__result_decimal_add(completed_sum "${completed_sum}" "${completed}")
        else()
          set(channels_valid FALSE)
          set(dma_valid FALSE)
        endif()
      endforeach()
    endif()
    if(channels_valid AND time_dma_completed_units_valid)
      gbl__result_decimal_le(channel_completions_within_run
        "${completed_sum}" "${time_dma_completed_units}")
      if(NOT channel_completions_within_run)
        gbl_finding(result bad-phase9-progress
          "final channel activation completions exceed time.dma_completed_units")
        set(dma_valid FALSE)
      endif()
    endif()
    if("${owner}" STREQUAL "dma" AND NOT any_active_or_pending)
      gbl_finding(result bad-phase9-owner
        "DMA owner requires a pending or active channel")
      set(dma_valid FALSE)
    elseif("${owner}" STREQUAL "cpu" AND any_active)
      gbl_finding(result bad-phase9-owner
        "CPU owner conflicts with an active channel")
      set(dma_valid FALSE)
    endif()
  elseif(observed_valid)
    foreach(path IN ITEMS waitcnt bus_owner cpu_paused machine_cycles channels)
      gbl_json_type(value_type "${doc}" phase9 ${path})
      if(NOT "${value_type}" STREQUAL "NULL")
        gbl_finding(result bad-phase9-not-observed
          "unobserved phase9 requires null ${path}")
        set(dma_valid FALSE)
      endif()
    endforeach()
    foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
        dma_destination_cycles dma_completed_units dma_pause_cycles)
      if(time_${field}_valid AND NOT "${time_${field}}" STREQUAL "0")
        gbl_finding(result bad-phase9-not-observed
          "unobserved phase9 requires zero ${field}")
        set(dma_valid FALSE)
      endif()
    endforeach()
  endif()

  if(NOT observed_valid)
    set(dma_valid FALSE)
  endif()

  gbl_json_type(latch_type "${doc}" phase9 latch_policy)
  if(NOT "${latch_type}" STREQUAL "OBJECT")
    gbl_finding(result bad-phase9-policy "phase9.latch_policy must be an object")
    set(dma_valid FALSE)
  else()
    foreach(pair IN ITEMS
        "policy;per-channel-retained-latch"
        "provenance;project-selected"
        "hardware_qualification;UNKNOWN")
      list(GET pair 0 field)
      list(GET pair 1 expected)
      gbl_json_type(type "${doc}" phase9 latch_policy ${field})
      if(NOT "${type}" STREQUAL "STRING")
        gbl_finding(result bad-phase9-policy
          "phase9.latch_policy.${field} must be a string")
        set(dma_valid FALSE)
      else()
        string(JSON actual GET "${doc}" phase9 latch_policy ${field})
        if(NOT "${actual}" STREQUAL "${expected}")
          gbl_finding(result bad-phase9-policy "phase9.latch_policy.${field}")
          set(dma_valid FALSE)
        endif()
      endif()
    endforeach()
    gbl_json_type(fallback_type "${doc}" phase9 latch_policy fallback_observed)
    gbl_json_type(classification_type "${doc}" phase9 latch_policy classification)
    gbl_json_type(dispute_id_type "${doc}" phase9 latch_policy dispute_id)
    if("${fallback_type}" STREQUAL "BOOLEAN")
      string(JSON fallback_observed GET "${doc}" phase9 latch_policy fallback_observed)
    elseif(NOT "${fallback_type}" STREQUAL "NULL")
      gbl_finding(result bad-phase9-latch
        "latch fallback observation must be boolean or null")
      set(dma_valid FALSE)
      set(fallback_observed FALSE)
    else()
      set(fallback_observed FALSE)
    endif()
    if(fallback_observed)
      if(NOT observed OR NOT "${classification_type}" STREQUAL "STRING" OR
          NOT "${dispute_id_type}" STREQUAL "STRING")
        gbl_finding(result bad-phase9-latch
          "latch fallback must remain disputed and UNKNOWN")
        set(dma_valid FALSE)
      else()
        string(JSON classification GET "${doc}" phase9 latch_policy classification)
        string(JSON dispute_id GET "${doc}" phase9 latch_policy dispute_id)
        if(NOT "${classification}" STREQUAL "disputed" OR
            NOT "${dispute_id}" STREQUAL "disp-dma-latch")
          gbl_finding(result bad-phase9-latch
            "latch fallback must remain disputed and UNKNOWN")
          set(dma_valid FALSE)
        endif()
      endif()
      gbl_json_length(dispute_count "${doc}" disputes)
      set(latch_dispute_listed FALSE)
      if(dispute_count GREATER 0)
        math(EXPR dispute_last "${dispute_count}-1")
        foreach(index RANGE 0 ${dispute_last})
          gbl_json_string(dispute_id "${doc}" disputes ${index})
          if("${dispute_id}" STREQUAL "disp-dma-latch")
            set(latch_dispute_listed TRUE)
          endif()
        endforeach()
      endif()
      if(NOT latch_dispute_listed)
        gbl_finding(result undisclosed-dispute "disp-dma-latch is not in disputes")
        set(dma_valid FALSE)
      endif()
      gbl_json_type(trace_type "${doc}" trace)
      gbl__result_phase9_decimal(trace_mask_valid trace_mask "${doc}"
        "trace.mask" trace mask)
      set(trace_mask_within_range FALSE)
      if(trace_mask_valid)
        gbl__result_decimal_le(trace_mask_within_range "${trace_mask}"
          "4294967295")
      endif()
      if(NOT "${trace_type}" STREQUAL "OBJECT" OR NOT trace_mask_valid OR
          NOT trace_mask_within_range)
        gbl_finding(result bad-phase9-latch
          "latch fallback requires a DMA trace record")
        set(dma_valid FALSE)
      else()
        math(EXPR trace_has_dma "${trace_mask} & 8")
        gbl__result_phase9_decimal(trace_count_valid trace_count "${doc}"
          "trace.count" trace count)
        gbl__result_phase9_decimal(trace_capacity_valid trace_capacity "${doc}"
          "trace.capacity" trace capacity)
        if(NOT trace_has_dma EQUAL 8 OR NOT trace_count_valid OR
            NOT trace_capacity_valid OR "${trace_count}" STREQUAL "0")
          gbl_finding(result bad-phase9-latch
            "latch fallback requires a captured DMA trace record")
          set(dma_valid FALSE)
        else()
          gbl__result_decimal_le(trace_within_capacity "${trace_count}"
            "${trace_capacity}")
          if(NOT trace_within_capacity)
            gbl_finding(result bad-phase9-latch
              "DMA trace count exceeds its capacity")
            set(dma_valid FALSE)
          endif()
        endif()
      endif()
    else()
      if(NOT "${classification_type}" STREQUAL "NULL" OR
          NOT "${dispute_id_type}" STREQUAL "NULL")
        gbl_finding(result bad-phase9-latch
          "unobserved latch fallback cannot carry a classification or dispute")
        set(dma_valid FALSE)
      endif()
      if(observed_valid AND NOT observed AND
          NOT "${fallback_type}" STREQUAL "NULL")
        gbl_finding(result bad-phase9-not-observed
          "unobserved phase9 requires null latch fallback")
        set(dma_valid FALSE)
      endif()
    endif()
  endif()

  set(${present_out} TRUE PARENT_SCOPE)
  set(${valid_out} "${dma_valid}" PARENT_SCOPE)
  foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
      dma_destination_cycles dma_completed_units dma_pause_cycles)
    set(phase9_${field} "${${field}}" PARENT_SCOPE)
  endforeach()
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

# gbl__result_decimal_add(<out> <left> <right>): add canonical nonnegative decimal
# strings a digit at a time. Each math expression stays below 20, so uint64 values
# never pass through CMake's signed integer representation.
function(gbl__result_decimal_add out left right)
  string(LENGTH "${left}" left_digits)
  string(LENGTH "${right}" right_digits)
  if(left_digits GREATER right_digits)
    set(digits "${left_digits}")
  else()
    set(digits "${right_digits}")
  endif()
  set(offset 0)
  set(carry 0)
  set(sum "")
  while(offset LESS digits)
    set(left_digit 0)
    set(right_digit 0)
    if(offset LESS left_digits)
      math(EXPR left_index "${left_digits}-${offset}-1")
      string(SUBSTRING "${left}" ${left_index} 1 left_digit)
    endif()
    if(offset LESS right_digits)
      math(EXPR right_index "${right_digits}-${offset}-1")
      string(SUBSTRING "${right}" ${right_index} 1 right_digit)
    endif()
    math(EXPR column "${left_digit}+${right_digit}+${carry}")
    math(EXPR output_digit "${column}%10")
    math(EXPR carry "${column}/10")
    string(PREPEND sum "${output_digit}")
    math(EXPR offset "${offset}+1")
  endwhile()
  if(carry GREATER 0)
    string(PREPEND sum "${carry}")
  endif()
  set(${out} "${sum}" PARENT_SCOPE)
endfunction()

function(gbl__result_decimal_mul_small out value factor)
  string(LENGTH "${value}" digits)
  set(offset 0)
  set(carry 0)
  set(product "")
  while(offset LESS digits)
    math(EXPR index "${digits}-${offset}-1")
    string(SUBSTRING "${value}" ${index} 1 digit)
    math(EXPR column "${digit}*${factor}+${carry}")
    math(EXPR output_digit "${column}%10")
    math(EXPR carry "${column}/10")
    string(PREPEND product "${output_digit}")
    math(EXPR offset "${offset}+1")
  endwhile()
  while(carry GREATER 0)
    math(EXPR output_digit "${carry}%10")
    math(EXPR carry "${carry}/10")
    string(PREPEND product "${output_digit}")
  endwhile()
  if("${product}" STREQUAL "")
    set(product "0")
  endif()
  set(${out} "${product}" PARENT_SCOPE)
endfunction()

function(gbl__result_phase10_require_type doc label expected code)
  gbl_json_type(actual_type "${doc}" ${ARGN})
  if("${actual_type}" STREQUAL "MISSING" OR
      "${actual_type}" STREQUAL "NULL")
    gbl_finding(result missing-field "${label}")
  elseif(NOT "${actual_type}" STREQUAL "${expected}")
    gbl_finding(result "${code}" "${label} must be ${expected}")
  endif()
endfunction()

function(gbl__result_phase10_expect_string doc label expected code)
  gbl_json_type(actual_type "${doc}" ${ARGN})
  if("${actual_type}" STREQUAL "MISSING" OR
      "${actual_type}" STREQUAL "NULL")
    gbl_finding(result missing-field "${label}")
  elseif(NOT "${actual_type}" STREQUAL "STRING")
    gbl_finding(result "${code}" "${label} must be a string")
  else()
    string(JSON actual GET "${doc}" ${ARGN})
    if(NOT "${actual}" STREQUAL "${expected}")
      gbl_finding(result "${code}" "${label} differs from the selected policy")
    endif()
  endif()
endfunction()

function(gbl__result_phase10_capabilities_check doc profile_doc required)
  foreach(field IN ITEMS profile_version display_capabilities video_memory_access)
    gbl_json_type(capability_${field}_type "${doc}" phase10 ${field})
  endforeach()

  set(any_capability_field FALSE)
  set(complete_capability_tuple TRUE)
  foreach(field IN ITEMS profile_version display_capabilities video_memory_access)
    if(NOT "${capability_${field}_type}" STREQUAL "MISSING")
      set(any_capability_field TRUE)
    else()
      set(complete_capability_tuple FALSE)
    endif()
  endforeach()
  if(NOT required AND NOT any_capability_field)
    return()
  endif()
  if(NOT complete_capability_tuple)
    gbl_finding(result bad-phase10-capability
      "phase10 capability metadata must be a complete profile tuple")
    return()
  endif()

  string(JSON profile_version GET "${profile_doc}" profile_version)
  if(NOT "${capability_profile_version_type}" STREQUAL "NUMBER")
    gbl_finding(result bad-phase10-capability
      "phase10.profile_version must be an integer matching the profile")
  else()
    string(JSON capability_profile_version GET "${doc}" phase10 profile_version)
    if(NOT "${capability_profile_version}" MATCHES "^(0|[1-9][0-9]*)$" OR
        NOT "${capability_profile_version}" STREQUAL "${profile_version}")
      gbl_finding(result bad-phase10-capability
        "phase10.profile_version differs from the selected profile")
    endif()
  endif()

  string(JSON expected_display_capabilities GET "${profile_doc}"
    ppu_policy display_capabilities)
  if(NOT "${capability_display_capabilities_type}" STREQUAL "OBJECT")
    gbl_finding(result bad-phase10-capability
      "phase10.display_capabilities must be an object")
  else()
    string(JSON actual_display_capabilities GET "${doc}"
      phase10 display_capabilities)
    string(JSON display_matches ERROR_VARIABLE display_error EQUAL
      "${actual_display_capabilities}" "${expected_display_capabilities}")
    if((NOT "${display_error}" STREQUAL "NOTFOUND" AND
        NOT "${display_error}" STREQUAL "") OR NOT display_matches)
      gbl_finding(result bad-phase10-capability
        "phase10.display_capabilities must exactly match the selected profile")
    endif()
  endif()

  string(JSON expected_video_memory_access GET "${profile_doc}"
    ppu_policy video_memory_access)
  if(NOT "${capability_video_memory_access_type}" STREQUAL "OBJECT")
    gbl_finding(result bad-phase10-capability
      "phase10.video_memory_access must be an object")
  else()
    string(JSON actual_video_memory_access GET "${doc}"
      phase10 video_memory_access)
    string(JSON access_matches ERROR_VARIABLE access_error EQUAL
      "${actual_video_memory_access}" "${expected_video_memory_access}")
    if((NOT "${access_error}" STREQUAL "NOTFOUND" AND
        NOT "${access_error}" STREQUAL "") OR NOT access_matches)
      gbl_finding(result bad-phase10-capability
        "phase10.video_memory_access must exactly match the selected profile")
    endif()
  endif()
endfunction()

# Phase 10's additive result extension is all-or-nothing for result/1 records.
# The accepted timing and row-sampling claims are read from the checked-in AGB
# profile, while frame and byte-layout constraints describe the public producer.
function(gbl__result_phase10_check doc time_cycles result_status result_ran
    result_reason)
  gbl_json_type(phase10_type "${doc}" phase10)
  gbl_json_type(raw_video_type "${doc}" raw_video)
  if("${phase10_type}" STREQUAL "MISSING" AND
      "${raw_video_type}" STREQUAL "MISSING")
    return()
  elseif("${phase10_type}" STREQUAL "MISSING" OR
      "${raw_video_type}" STREQUAL "MISSING")
    gbl_finding(result partial-phase10-extension
      "phase10 and raw_video must be wholly present or absent")
    return()
  endif()

  if(NOT "${phase10_type}" STREQUAL "OBJECT")
    gbl_finding(result bad-phase10-type "phase10 must be an object")
    return()
  endif()
  if(NOT "${raw_video_type}" STREQUAL "OBJECT")
    gbl_finding(result bad-phase10-type "raw_video must be an object")
    return()
  endif()

  file(READ "${GBL_RESULT_AGB_PROFILE_PATH}" profile_doc)
  string(JSON profile_hblank GET "${profile_doc}"
    ppu_policy hblank_event_offset_cycles)
  string(JSON profile_line_cycles GET "${profile_doc}" timing
    cycles_per_line value)
  string(JSON profile_hblank_source GET "${profile_doc}"
    ppu_policy hblank_source)
  string(JSON profile_timing_policy GET "${profile_doc}"
    ppu_policy policy_classification)
  string(JSON profile_hblank_classification GET "${profile_doc}"
    ppu_policy hblank_dependent_classification)
  string(JSON profile_hblank_dispute GET "${profile_doc}"
    ppu_policy hblank_dispute)
  string(JSON profile_hardware_qualification GET "${profile_doc}"
    ppu_policy hblank_hardware_qualification)
  math(EXPR profile_last_line_cycle "${profile_line_cycles}-1")

  gbl_json_type(p10_model_type "${doc}" phase10 model)
  set(p10_model "")
  if(NOT "${p10_model_type}" STREQUAL "STRING")
    gbl_finding(result bad-phase10-type "phase10.model must be a string")
  else()
    string(JSON p10_model GET "${doc}" phase10 model)
  endif()
  if("${p10_model}" STREQUAL "gamebloke-ppu-observation-2")
    gbl__result_phase10_capabilities_check("${doc}" "${profile_doc}" TRUE)
  elseif("${p10_model}" STREQUAL "gamebloke-ppu-observation-1")
    gbl__result_phase10_capabilities_check("${doc}" "${profile_doc}" FALSE)
  elseif("${p10_model_type}" STREQUAL "STRING")
    gbl_finding(result bad-phase10-type
      "phase10.model is an unknown PPU observation model")
  endif()
  gbl__result_phase10_require_type("${doc}" "phase10.observed" BOOLEAN
    bad-phase10-type phase10 observed)
  gbl__result_phase10_require_type("${doc}" "phase10.timing" OBJECT
    bad-phase10-type phase10 timing)
  gbl__result_phase10_expect_string("${doc}" "phase10.timing.model"
    "gamebloke-ppu-model-1" bad-phase10-policy phase10 timing model)
  gbl__result_phase10_expect_string("${doc}"
    "phase10.timing.hblank_offset_cycles" "${profile_hblank}"
    bad-phase10-policy phase10 timing hblank_offset_cycles)
  gbl__result_phase10_expect_string("${doc}" "phase10.timing.policy"
    "${profile_timing_policy}" bad-phase10-policy phase10 timing policy)
  gbl__result_phase10_expect_string("${doc}"
    "phase10.timing.evidence_class" "${profile_hblank_source}"
    bad-phase10-policy phase10 timing evidence_class)
  gbl__result_phase10_expect_string("${doc}"
    "phase10.timing.classification" "${profile_hblank_classification}"
    bad-phase10-policy phase10 timing classification)
  gbl__result_phase10_expect_string("${doc}" "phase10.timing.dispute_id"
    "${profile_hblank_dispute}" bad-phase10-policy phase10 timing dispute_id)
  gbl__result_phase10_expect_string("${doc}"
    "phase10.timing.hardware_qualification"
    "${profile_hardware_qualification}" bad-phase10-policy
    phase10 timing hardware_qualification)

  gbl_json_type(p10_observed_type "${doc}" phase10 observed)
  set(p10_observed FALSE)
  if("${p10_observed_type}" STREQUAL "BOOLEAN")
    string(JSON p10_observed GET "${doc}" phase10 observed)
  endif()
  if(p10_observed)
    foreach(field IN ITEMS raster frame controls hblank_trigger_dependency)
      gbl__result_phase10_require_type("${doc}" "phase10.${field}" OBJECT
        bad-phase10-type phase10 ${field})
    endforeach()
  else()
    foreach(field IN ITEMS raster frame controls hblank_trigger_dependency)
      gbl_json_type(p10_null_type "${doc}" phase10 ${field})
      if(NOT "${p10_null_type}" STREQUAL "NULL")
        gbl_finding(result bad-phase10-not-observed
          "unobserved PPU fields must be null")
      endif()
    endforeach()
  endif()

  # The reference note is present only once a PPU observation was made. It is
  # descriptive provenance and never a hardware qualification.
  gbl_json_type(timing_evidence_type "${doc}" phase10 timing evidence)
  if(p10_observed)
    if(NOT "${timing_evidence_type}" STREQUAL "STRING")
      gbl_finding(result bad-phase10-policy
        "observed PPU timing needs its reference evidence note")
    endif()
  elseif(NOT "${timing_evidence_type}" STREQUAL "MISSING" AND
      NOT "${timing_evidence_type}" STREQUAL "NULL")
    gbl_finding(result bad-phase10-policy
      "unobserved PPU timing cannot carry an evidence note")
  endif()

  if(p10_observed)
    foreach(field IN ITEMS line line_cycle machine_cycle frame_epoch)
      gbl_json_type(raster_${field}_type "${doc}" phase10 raster ${field})
      if("${raster_${field}_type}" STREQUAL "MISSING")
        gbl_finding(result missing-field "phase10.raster.${field}")
      endif()
    endforeach()
    gbl__result_uint32(raster_line_valid raster_line "${doc}" 227
      phase10 raster line)
    if(NOT raster_line_valid)
      gbl_finding(result bad-phase10-raster "phase10.raster.line must be 0..227")
    endif()
    gbl__result_uint64(raster_line_cycle_valid raster_line_cycle "${doc}"
      phase10 raster line_cycle)
    if(NOT raster_line_cycle_valid)
      gbl_finding(result bad-phase10-raster
        "phase10.raster.line_cycle must be a uint64 decimal string")
    else()
      gbl__result_decimal_le(line_cycle_within_line "${raster_line_cycle}"
        "${profile_last_line_cycle}")
      if(NOT line_cycle_within_line)
        gbl_finding(result bad-phase10-raster
          "phase10.raster.line_cycle exceeds one line")
      endif()
    endif()
    gbl__result_uint64(raster_machine_valid raster_machine "${doc}"
      phase10 raster machine_cycle)
    if(NOT raster_machine_valid)
      gbl_finding(result bad-phase10-type
        "phase10.raster.machine_cycle must be a uint64 decimal string")
    elseif(NOT "${raster_machine}" STREQUAL "${time_cycles}")
      gbl_finding(result bad-phase10-machine-time
        "phase10.raster.machine_cycle differs from time.cycles")
    endif()
    gbl_json_type(raster_epoch_type "${doc}" phase10 raster frame_epoch)
    if("${raster_epoch_type}" STREQUAL "STRING")
      gbl__result_uint64(raster_epoch_valid raster_epoch "${doc}"
        phase10 raster frame_epoch)
      if(NOT raster_epoch_valid)
        gbl_finding(result bad-phase10-raster
          "phase10.raster.frame_epoch must be a uint64 decimal string or null")
      endif()
    elseif(NOT "${raster_epoch_type}" STREQUAL "NULL")
      gbl_finding(result bad-phase10-raster
        "phase10.raster.frame_epoch must be a uint64 decimal string or null")
    endif()

    gbl__result_phase10_require_type("${doc}" "phase10.controls.supported"
      BOOLEAN bad-phase10-type phase10 controls supported)
    gbl__result_phase10_require_type("${doc}"
      "phase10.frame.supported_controls" BOOLEAN bad-phase10-type
      phase10 frame supported_controls)
    gbl_json_type(controls_supported_type "${doc}" phase10 controls supported)
    gbl_json_type(frame_supported_type "${doc}" phase10 frame supported_controls)
    if("${controls_supported_type}" STREQUAL "BOOLEAN" AND
        "${frame_supported_type}" STREQUAL "BOOLEAN")
      string(JSON controls_supported GET "${doc}" phase10 controls supported)
      string(JSON frame_supported GET "${doc}" phase10 frame supported_controls)
      if(NOT controls_supported STREQUAL frame_supported)
        gbl_finding(result bad-phase10-frame
          "controls.supported and frame.supported_controls differ")
      endif()
    endif()

    foreach(pair IN ITEMS "width;240" "height;160" "stride_bytes;480"
        "byte_length;76800")
      list(GET pair 0 field)
      list(GET pair 1 expected)
      gbl_json_type(frame_${field}_type "${doc}" phase10 frame ${field})
      if("${frame_${field}_type}" STREQUAL "MISSING" OR
          "${frame_${field}_type}" STREQUAL "NULL")
        gbl_finding(result missing-field "phase10.frame.${field}")
      elseif(NOT "${frame_${field}_type}" STREQUAL "NUMBER")
        gbl_finding(result bad-phase10-type
          "phase10.frame.${field} must be an integer")
      else()
        string(JSON frame_${field} GET "${doc}" phase10 frame ${field})
        if(NOT "${frame_${field}}" MATCHES "^(0|[1-9][0-9]*)$" OR
            NOT "${frame_${field}}" STREQUAL "${expected}")
          gbl_finding(result bad-phase10-frame
            "phase10.frame.${field} must be ${expected}")
        endif()
      endif()
    endforeach()
    gbl__result_phase10_expect_string("${doc}" "phase10.frame.format"
      "bgr555-le" bad-phase10-frame phase10 frame format)
    gbl__result_phase10_expect_string("${doc}" "phase10.frame.source"
      "canonical-vram" bad-phase10-frame phase10 frame source)
    gbl__result_phase10_expect_string("${doc}" "phase10.frame.latch_policy"
      "row-start" bad-phase10-policy phase10 frame latch_policy)

    gbl_json_type(frame_state_type "${doc}" phase10 frame state)
    gbl_json_type(frame_reason_type "${doc}" phase10 frame reason)
    if(NOT "${frame_state_type}" STREQUAL "STRING")
      gbl_finding(result bad-phase10-type "phase10.frame.state must be a string")
    endif()
    if(NOT "${frame_reason_type}" STREQUAL "STRING")
      gbl_finding(result bad-phase10-type "phase10.frame.reason must be a string")
    endif()
    set(frame_state "")
    set(frame_reason "")
    if("${frame_state_type}" STREQUAL "STRING")
      string(JSON frame_state GET "${doc}" phase10 frame state)
    endif()
    if("${frame_reason_type}" STREQUAL "STRING")
      string(JSON frame_reason GET "${doc}" phase10 frame reason)
    endif()
    if(NOT "${frame_state}" MATCHES "^(invalid|incomplete|valid)$")
      gbl_finding(result bad-phase10-frame "phase10.frame.state is unknown")
    endif()
    if(NOT "${frame_reason}" MATCHES
        "^(none|incomplete|unsupported-mode|bg2-disabled|forced-blank|obj-enabled|unsupported-obj-control|window-enabled|effect-enabled|bg2-transform-unmodeled)$")
      gbl_finding(result bad-phase10-frame "phase10.frame.reason is unknown")
    endif()

    foreach(field IN ITEMS epoch sample_cycle completion_cycle)
      gbl_json_type(frame_${field}_type "${doc}" phase10 frame ${field})
      if("${frame_${field}_type}" STREQUAL "MISSING")
        gbl_finding(result missing-field "phase10.frame.${field}")
      endif()
    endforeach()
    if("${frame_state}" STREQUAL "valid")
      if(NOT "${frame_reason}" STREQUAL "none" OR
          NOT controls_supported OR NOT frame_supported OR
          NOT "${raster_epoch_type}" STREQUAL "STRING")
        gbl_finding(result bad-phase10-frame-validity
          "valid frame needs supported controls, a started epoch, and reason none")
      endif()
      foreach(field IN ITEMS epoch sample_cycle completion_cycle)
        if(NOT "${frame_${field}_type}" STREQUAL "STRING")
          gbl_finding(result bad-phase10-frame-validity
            "valid frame needs ${field}")
        else()
          gbl__result_uint64(frame_${field}_valid frame_${field} "${doc}"
            phase10 frame ${field})
          if(NOT frame_${field}_valid)
            gbl_finding(result bad-phase10-frame-validity
              "phase10.frame.${field} must be a uint64 decimal string")
          endif()
        endif()
      endforeach()
      if("${frame_epoch_type}" STREQUAL "STRING" AND
          "${raster_epoch_type}" STREQUAL "STRING" AND
          NOT "${frame_epoch}" STREQUAL "${raster_epoch}")
        gbl_finding(result bad-phase10-frame-validity
          "frame epoch differs from the raster epoch")
      endif()
      if(frame_sample_cycle_valid AND frame_completion_cycle_valid)
        math(EXPR completion_offset "160 * ${profile_line_cycles}")
        string(JSON profile_frame_cycles GET "${profile_doc}" timing
          cycles_per_frame value)
        gbl__result_decimal_mul_small(expected_sample "${frame_epoch}"
          "${profile_frame_cycles}")
        if(NOT "${frame_sample_cycle}" STREQUAL "${expected_sample}")
          gbl_finding(result bad-phase10-frame-validity
            "sample_cycle must start the named frame epoch")
        endif()
        gbl__result_decimal_add(expected_completion "${frame_sample_cycle}"
          "${completion_offset}")
        if(NOT "${frame_completion_cycle}" STREQUAL "${expected_completion}")
          gbl_finding(result bad-phase10-frame-validity
            "completion_cycle must follow 160 row samples")
        endif()
        gbl__result_decimal_le(completion_within_time
          "${frame_completion_cycle}" "${time_cycles}")
        if(NOT completion_within_time)
          gbl_finding(result bad-phase10-frame-validity
            "completion_cycle is later than time.cycles")
        endif()
      endif()
    else()
      if("${frame_state}" STREQUAL "incomplete")
        if(NOT "${frame_reason}" STREQUAL "incomplete" OR
            NOT controls_supported OR NOT frame_supported)
          gbl_finding(result bad-phase10-frame-validity
            "incomplete frame must retain supported controls and reason incomplete")
        endif()
      elseif("${frame_state}" STREQUAL "invalid" AND
          ("${frame_reason}" STREQUAL "none" OR
           "${frame_reason}" STREQUAL "incomplete" OR
           controls_supported OR frame_supported))
        gbl_finding(result bad-phase10-frame-validity
          "invalid frame must name an unsupported control")
      endif()
      foreach(field IN ITEMS epoch sample_cycle completion_cycle)
        if(NOT "${frame_${field}_type}" STREQUAL "NULL")
          gbl_finding(result bad-phase10-frame-validity
            "non-valid frames must keep ${field} null")
        endif()
      endforeach()
    endif()

    gbl_json_type(dependency_type "${doc}" phase10 hblank_trigger_dependency observed)
    if(NOT "${dependency_type}" STREQUAL "BOOLEAN")
      gbl_finding(result bad-phase10-type
        "phase10.hblank_trigger_dependency.observed must be boolean")
      set(dependency_observed FALSE)
    else()
      string(JSON dependency_observed GET "${doc}"
        phase10 hblank_trigger_dependency observed)
    endif()
    gbl__result_phase10_expect_string("${doc}"
      "phase10.hblank_trigger_dependency.hardware_qualification"
      "${profile_hardware_qualification}" bad-phase10-policy
      phase10 hblank_trigger_dependency hardware_qualification)
    gbl_json_type(dependency_class_type "${doc}"
      phase10 hblank_trigger_dependency classification)
    gbl_json_type(dependency_dispute_type "${doc}"
      phase10 hblank_trigger_dependency dispute_id)
    if(dependency_observed)
      if(NOT "${dependency_class_type}" STREQUAL "STRING" OR
          NOT "${dependency_dispute_type}" STREQUAL "STRING")
        gbl_finding(result bad-phase10-dependency
          "observed HBlank dependency needs its disputed classification and id")
      else()
        string(JSON dependency_class GET "${doc}"
          phase10 hblank_trigger_dependency classification)
        string(JSON dependency_dispute GET "${doc}"
          phase10 hblank_trigger_dependency dispute_id)
        if(NOT "${dependency_class}" STREQUAL "${profile_hblank_classification}" OR
            NOT "${dependency_dispute}" STREQUAL "${profile_hblank_dispute}")
          gbl_finding(result bad-phase10-dependency
            "HBlank dependency must retain the profile dispute")
        endif()
        gbl_json_length(disputes_count "${doc}" disputes)
        set(dispute_disclosed FALSE)
        if(disputes_count GREATER 0)
          math(EXPR disputes_last "${disputes_count}-1")
          foreach(i RANGE ${disputes_last})
            gbl_json_string(dispute_item "${doc}" disputes ${i})
            if("${dispute_item}" STREQUAL "${profile_hblank_dispute}")
              set(dispute_disclosed TRUE)
            endif()
          endforeach()
        endif()
        gbl_json_length(labels_count "${doc}" labels)
        set(disputed_label FALSE)
        if(labels_count GREATER 0)
          math(EXPR labels_last "${labels_count}-1")
          foreach(i RANGE ${labels_last})
            gbl_json_string(label_item "${doc}" labels ${i})
            if("${label_item}" STREQUAL "disputed")
              set(disputed_label TRUE)
            endif()
          endforeach()
        endif()
        if(NOT dispute_disclosed OR NOT disputed_label)
          gbl_finding(result bad-phase10-dependency
            "observed HBlank dependency must disclose its dispute")
        endif()
      endif()
    elseif(NOT "${dependency_class_type}" STREQUAL "NULL" OR
        NOT "${dependency_dispute_type}" STREQUAL "NULL")
      gbl_finding(result bad-phase10-dependency
        "unobserved HBlank dependency must keep classification and dispute null")
    endif()
  endif()

  # Artifact metadata is an independent host observation. Its digest is over
  # raw video bytes, never the ROM, fixture, or logical input timeline.
  foreach(field IN ITEMS requested status emitted byte_length sha256 failure)
    gbl_json_type(raw_${field}_type "${doc}" raw_video ${field})
    if("${raw_${field}_type}" STREQUAL "MISSING")
      gbl_finding(result missing-field "raw_video.${field}")
    endif()
  endforeach()
  gbl__result_phase10_require_type("${doc}" "raw_video.requested" BOOLEAN
    bad-phase10-type raw_video requested)
  gbl__result_phase10_require_type("${doc}" "raw_video.emitted" BOOLEAN
    bad-phase10-type raw_video emitted)
  gbl_json_type(raw_format_type "${doc}" raw_video format)
  if("${p10_model}" STREQUAL "gamebloke-ppu-observation-2" OR
      NOT "${raw_format_type}" STREQUAL "MISSING")
    if(NOT "${raw_format_type}" STREQUAL "STRING")
      gbl_finding(result bad-phase10-video-output
        "raw_video.format must be bgr555-le")
    else()
      string(JSON raw_format GET "${doc}" raw_video format)
      if(NOT "${raw_format}" STREQUAL "bgr555-le")
        gbl_finding(result bad-phase10-video-output
          "raw_video.format must be bgr555-le")
      endif()
    endif()
  endif()
  gbl_json_type(raw_requested_type "${doc}" raw_video requested)
  gbl_json_type(raw_emitted_type "${doc}" raw_video emitted)
  set(raw_requested FALSE)
  set(raw_emitted FALSE)
  if("${raw_requested_type}" STREQUAL "BOOLEAN")
    string(JSON raw_requested GET "${doc}" raw_video requested)
  endif()
  if("${raw_emitted_type}" STREQUAL "BOOLEAN")
    string(JSON raw_emitted GET "${doc}" raw_video emitted)
  endif()
  gbl_json_type(raw_status_type "${doc}" raw_video status)
  set(raw_status "")
  if("${raw_status_type}" STREQUAL "STRING")
    string(JSON raw_status GET "${doc}" raw_video status)
  else()
    gbl_finding(result bad-phase10-type "raw_video.status must be a string")
  endif()
  if(NOT "${raw_status}" MATCHES "^(not-requested|emitted|failed|not-emitted|invalid|incomplete)$")
    gbl_finding(result bad-phase10-video-output "raw_video.status is unknown")
  endif()
  gbl_json_type(raw_length_type "${doc}" raw_video byte_length)
  gbl_json_type(raw_digest_type "${doc}" raw_video sha256)
  gbl_json_type(raw_failure_type "${doc}" raw_video failure)
  set(raw_length "")
  set(raw_digest "")
  set(raw_failure "")
  if("${raw_length_type}" STREQUAL "NUMBER")
    string(JSON raw_length GET "${doc}" raw_video byte_length)
  endif()
  if("${raw_digest_type}" STREQUAL "STRING")
    string(JSON raw_digest GET "${doc}" raw_video sha256)
  endif()
  if("${raw_failure_type}" STREQUAL "STRING")
    string(JSON raw_failure GET "${doc}" raw_video failure)
  endif()
  if(raw_requested_type STREQUAL "BOOLEAN" AND NOT raw_requested)
    if(NOT "${raw_status}" STREQUAL "not-requested" OR raw_emitted OR
        NOT "${raw_length_type}" STREQUAL "NULL" OR
        NOT "${raw_digest_type}" STREQUAL "NULL" OR
        NOT "${raw_failure_type}" STREQUAL "NULL")
      gbl_finding(result bad-phase10-video-output
        "unrequested raw video must have no output artifact or failure")
    endif()
  elseif(raw_requested_type STREQUAL "BOOLEAN" AND raw_requested)
    if("${raw_status}" STREQUAL "emitted")
      if(NOT raw_emitted OR NOT p10_observed OR
          NOT "${frame_state}" STREQUAL "valid" OR
          NOT "${raw_length_type}" STREQUAL "NUMBER" OR
          NOT "${raw_length}" STREQUAL "76800" OR
          NOT "${raw_failure_type}" STREQUAL "NULL")
        gbl_finding(result bad-phase10-video-output
          "emitted video needs a valid frame, 76800 bytes, and no failure")
      endif()
      set(raw_digest_valid FALSE)
      if("${raw_digest_type}" STREQUAL "STRING")
        gbl__manifest_is_hex(raw_digest_valid "${raw_digest}" 64)
      endif()
      if(NOT raw_digest_valid)
        gbl_finding(result bad-phase10-video-digest
          "emitted raw video needs a lowercase SHA-256 digest")
      endif()
    elseif("${raw_status}" STREQUAL "failed")
      if(raw_emitted OR NOT "${raw_length_type}" STREQUAL "NULL" OR
          NOT "${raw_failure_type}" STREQUAL "STRING" OR
          NOT "${result_status}" STREQUAL "HARNESS_FAIL" OR
          NOT "${result_reason}" STREQUAL "video-output-failed")
        gbl_finding(result bad-phase10-video-output
          "failed video output must be a reported harness failure")
      endif()
      if(NOT "${raw_failure}" MATCHES
          "^(video-output-path-null|video-output-path-empty|video-frame-unavailable|video-frame-invalid|video-buffer-allocation-failed|video-digest-failed|video-output-already-exists|video-output-open-failed|video-output-write-failed|video-output-close-failed|video-output-cleanup-failed)$")
        gbl_finding(result bad-phase10-video-output
          "raw_video.failure is not a named writer error")
      endif()
      if(NOT "${raw_digest_type}" STREQUAL "NULL")
        set(raw_digest_valid FALSE)
        if("${raw_digest_type}" STREQUAL "STRING")
          gbl__manifest_is_hex(raw_digest_valid "${raw_digest}" 64)
        endif()
        if(NOT raw_digest_valid)
          gbl_finding(result bad-phase10-video-digest
            "failed-output digest must be null or lowercase SHA-256")
        endif()
      endif()
    elseif("${raw_status}" STREQUAL "not-emitted")
      if(raw_emitted OR NOT p10_observed OR
          NOT "${frame_state}" STREQUAL "valid" OR
          NOT "${raw_length_type}" STREQUAL "NULL" OR
          NOT "${raw_digest_type}" STREQUAL "NULL" OR
          NOT "${raw_failure_type}" STREQUAL "NULL")
        gbl_finding(result bad-phase10-video-output
          "not-emitted requires a valid frame and no artifact metadata")
      endif()
    elseif("${raw_status}" STREQUAL "invalid" OR
        "${raw_status}" STREQUAL "incomplete")
      if(raw_emitted OR NOT p10_observed OR
          NOT "${frame_state}" STREQUAL "${raw_status}" OR
          NOT "${raw_length_type}" STREQUAL "NULL" OR
          NOT "${raw_digest_type}" STREQUAL "NULL" OR
          NOT "${raw_failure_type}" STREQUAL "NULL")
        gbl_finding(result bad-phase10-video-output
          "unavailable frame status must match the PPU frame and carry no artifact")
      endif()
    endif()
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

  # Historical result/1 documents may omit the entire timing-model extension.
  # Once any extension member appears, the complete model/component/IF tuple is
  # required and is validated independently from guest semantic success.
  set(time_extension_present FALSE)
  foreach(field IN ITEMS model qualification bus_cycles internal_cycles
      dma_startup_cycles dma_source_cycles dma_destination_cycles
      dma_completed_units dma_pause_cycles)
    gbl_json_type(extension_${field}_type "${doc}" time ${field})
    if(NOT "${extension_${field}_type}" STREQUAL "MISSING")
      set(time_extension_present TRUE)
    endif()
  endforeach()
  gbl_json_type(timer0_type "${doc}" timer0)
  if(NOT "${timer0_type}" STREQUAL "MISSING")
    set(time_extension_present TRUE)
  endif()
  gbl_json_type(phase8_type "${doc}" phase8)
  set(phase8_present FALSE)
  if(NOT "${phase8_type}" STREQUAL "MISSING")
    set(phase8_present TRUE)
    set(time_extension_present TRUE)
  endif()
  gbl_json_type(phase9_type "${doc}" phase9)
  if(NOT "${phase9_type}" STREQUAL "MISSING")
    set(time_extension_present TRUE)
  endif()

  if(time_extension_present)
    set(time_components_valid TRUE)
    set(phase8_wait_valid TRUE)
    set(phase8_wait_for_sum "0")

    gbl_json_type(time_model_type "${doc}" time model)
    if("${time_model_type}" STREQUAL "MISSING" OR
        "${time_model_type}" STREQUAL "NULL")
      gbl_finding(result missing-field "time.model")
    elseif(NOT "${time_model_type}" STREQUAL "STRING")
      gbl_finding(result bad-time-model "time.model must be a string")
    else()
      string(JSON time_model GET "${doc}" time model)
      if(NOT "${time_model}" STREQUAL "gamebloke-timer-model-1")
        gbl_finding(result bad-time-model "time.model")
      endif()
    endif()

    gbl_json_type(time_qualification_type "${doc}" time qualification)
    if("${time_qualification_type}" STREQUAL "MISSING" OR
        "${time_qualification_type}" STREQUAL "NULL")
      gbl_finding(result missing-field "time.qualification")
    elseif(NOT "${time_qualification_type}" STREQUAL "STRING")
      gbl_finding(result bad-time-qualification
        "time.qualification must be a string")
    else()
      string(JSON time_qualification GET "${doc}" time qualification)
      if(NOT "${time_qualification}" STREQUAL "project-model-only")
        gbl_finding(result bad-time-qualification "time.qualification")
      endif()
    endif()

    foreach(field IN ITEMS bus_cycles internal_cycles)
      gbl_json_type(time_${field}_type "${doc}" time ${field})
      if("${time_${field}_type}" STREQUAL "MISSING" OR
          "${time_${field}_type}" STREQUAL "NULL")
        gbl_finding(result missing-field "time.${field}")
        set(time_components_valid FALSE)
      else()
        gbl__result_uint64(time_${field}_valid time_${field} "${doc}" time ${field})
        if(NOT time_${field}_valid)
          if("${time_${field}_type}" STREQUAL "STRING")
            string(JSON invalid_component GET "${doc}" time ${field})
            if("${invalid_component}" MATCHES "^(0|[1-9][0-9]*)$")
              string(LENGTH "${invalid_component}" invalid_digits)
              if(invalid_digits GREATER 20 OR
                  (invalid_digits EQUAL 20 AND
                   "${invalid_component}" STRGREATER "18446744073709551615"))
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
          set(time_components_valid FALSE)
        endif()
      endif()
    endforeach()

    gbl_json_type(timer0_type "${doc}" timer0)
    if("${timer0_type}" STREQUAL "MISSING")
      gbl_finding(result missing-field "timer0.if_request")
    elseif(NOT "${timer0_type}" STREQUAL "OBJECT")
      gbl_finding(result bad-timer0-request "timer0 must be an object")
    else()
      gbl_json_type(timer0_request_type "${doc}" timer0 if_request)
      if("${timer0_request_type}" STREQUAL "MISSING")
        gbl_finding(result missing-field "timer0.if_request")
      elseif(NOT "${timer0_request_type}" STREQUAL "BOOLEAN" AND
          NOT "${timer0_request_type}" STREQUAL "NULL")
        gbl_finding(result bad-timer0-request
          "timer0.if_request must be boolean or null")
      else()
        gbl_json_type(extension_ran_type "${doc}" fixture ran)
        if("${extension_ran_type}" STREQUAL "BOOLEAN" AND time_valid)
          string(JSON extension_ran GET "${doc}" fixture ran)
          if(NOT extension_ran)
            if(NOT "${timer0_request_type}" STREQUAL "NULL")
              gbl_finding(result bad-timer0-request
                "not-run results require null timer0.if_request")
            endif()
          elseif("${time_cycles}" STREQUAL "0" AND
              "${time_instructions}" STREQUAL "0")
            if(NOT "${timer0_request_type}" STREQUAL "NULL")
              gbl_finding(result bad-timer0-request
                "zero-budget results require null timer0.if_request")
            endif()
          elseif(NOT "${timer0_request_type}" STREQUAL "BOOLEAN")
            gbl_finding(result bad-timer0-request
              "executed results require boolean timer0.if_request")
          endif()
        endif()
      endif()
    endif()

    # The phase 8 block is an additive result/1 extension. Historical documents
    # without it keep the existing bus+internal timing contract. Its observations
    # are model evidence, not hardware qualification or proof of a BIOS handler.
    if(phase8_present)
      if(NOT "${phase8_type}" STREQUAL "OBJECT")
        gbl_finding(result bad-phase8-type "phase8 must be an object")
        set(time_components_valid FALSE)
      else()
        foreach(field IN ITEMS model qualification observed irq last_exception power keypad wait_cycles)
          gbl_json_type(phase8_${field}_type "${doc}" phase8 ${field})
          if("${phase8_${field}_type}" STREQUAL "MISSING")
            gbl_finding(result missing-field "phase8.${field}")
            if(field STREQUAL "wait_cycles")
              set(phase8_wait_valid FALSE)
              set(time_components_valid FALSE)
            endif()
          endif()
        endforeach()

        gbl_json_type(phase8_model_type "${doc}" phase8 model)
        if("${phase8_model_type}" STREQUAL "STRING")
          string(JSON phase8_model GET "${doc}" phase8 model)
          if(NOT "${phase8_model}" STREQUAL "gamebloke-irq-input-model-1")
            gbl_finding(result bad-phase8-model "phase8.model")
          endif()
        elseif(NOT "${phase8_model_type}" STREQUAL "MISSING" AND
            NOT "${phase8_model_type}" STREQUAL "NULL")
          gbl_finding(result bad-phase8-type "phase8.model must be a string")
        endif()

        gbl_json_type(phase8_qualification_type "${doc}" phase8 qualification)
        if("${phase8_qualification_type}" STREQUAL "STRING")
          string(JSON phase8_qualification GET "${doc}" phase8 qualification)
          if(NOT "${phase8_qualification}" STREQUAL "project-model-only")
            gbl_finding(result bad-phase8-qualification "phase8.qualification")
          endif()
        elseif(NOT "${phase8_qualification_type}" STREQUAL "MISSING" AND
            NOT "${phase8_qualification_type}" STREQUAL "NULL")
          gbl_finding(result bad-phase8-type
            "phase8.qualification must be a string")
        endif()

        set(phase8_observed_valid FALSE)
        set(phase8_observed FALSE)
        gbl_json_type(phase8_observed_type "${doc}" phase8 observed)
        if("${phase8_observed_type}" STREQUAL "BOOLEAN")
          set(phase8_observed_valid TRUE)
          string(JSON phase8_observed GET "${doc}" phase8 observed)
        elseif(NOT "${phase8_observed_type}" STREQUAL "MISSING" AND
            NOT "${phase8_observed_type}" STREQUAL "NULL")
          gbl_finding(result bad-phase8-type "phase8.observed must be a boolean")
        endif()

        if("${phase8_observed_type}" STREQUAL "BOOLEAN")
          gbl_json_type(phase8_ran_type "${doc}" fixture ran)
          set(phase8_not_run FALSE)
          if("${phase8_ran_type}" STREQUAL "BOOLEAN")
            string(JSON phase8_ran GET "${doc}" fixture ran)
            if(NOT phase8_ran)
              set(phase8_not_run TRUE)
            endif()
          endif()
          if(time_valid AND "${time_budget_cycles}" STREQUAL "0")
            set(phase8_not_run TRUE)
          endif()

          if(phase8_not_run AND phase8_observed)
            gbl_finding(result bad-phase8-not-run
              "not-run or zero-budget result cannot observe phase8")
          endif()

          if(NOT phase8_observed)
            foreach(path IN ITEMS irq/if irq/ie irq/ime irq/sample last_exception
                power/state power/cause power/wake_source_mask power/wake_cycle
                keypad/keyinput keypad/keycnt keypad/condition keypad/irq_condition
                keypad/applied_events keypad/pending_events wait_cycles)
              string(REPLACE "/" ";" keys "${path}")
              gbl_json_type(null_type "${doc}" phase8 ${keys})
              if("${null_type}" STREQUAL "MISSING")
                gbl_finding(result missing-field "phase8.${path}")
              elseif(NOT "${null_type}" STREQUAL "NULL")
                gbl_finding(result bad-phase8-not-run
                  "unobserved phase8 requires null observations")
              endif()
            endforeach()
            gbl_json_type(wait_type "${doc}" phase8 wait_cycles)
            if("${wait_type}" STREQUAL "NULL")
              set(phase8_wait_for_sum "0")
              set(phase8_wait_valid TRUE)
            else()
              set(phase8_wait_valid FALSE)
              set(time_components_valid FALSE)
            endif()
          else()
            if(phase8_not_run)
              gbl_finding(result bad-phase8-not-run
                "not-run or zero-budget result cannot observe phase8")
            endif()

            # Current raw IRQ registers are deliberately independent of the
            # historical boundary sample below.
            foreach(field IN ITEMS if ie ime)
              gbl_json_type(raw_${field}_type "${doc}" phase8 irq ${field})
              if("${raw_${field}_type}" STREQUAL "MISSING" OR
                  "${raw_${field}_type}" STREQUAL "NULL")
                gbl_finding(result missing-field "phase8.irq.${field}")
              else()
                if(field STREQUAL "ime")
                  set(raw_${field}_maximum "1")
                else()
                  set(raw_${field}_maximum "16383")
                endif()
                gbl__result_uint16(raw_${field}_valid raw_${field} "${doc}"
                  "${raw_${field}_maximum}" phase8 irq ${field})
                if(NOT raw_${field}_valid)
                  gbl_finding(result bad-phase8-type
                    "phase8.irq.${field} must be a bounded integer")
                endif()
              endif()
            endforeach()

            gbl_json_type(sample_type "${doc}" phase8 irq sample)
            if("${sample_type}" STREQUAL "MISSING")
              gbl_finding(result missing-field "phase8.irq.sample")
            elseif(NOT "${sample_type}" STREQUAL "NULL" AND
                NOT "${sample_type}" STREQUAL "OBJECT")
              gbl_finding(result bad-phase8-type "phase8.irq.sample must be object or null")
            elseif("${sample_type}" STREQUAL "OBJECT")
              set(sample_valid TRUE)
              foreach(field IN ITEMS cycle if ie ime eligible_sources)
                gbl_json_type(sample_${field}_type "${doc}" phase8 irq sample ${field})
                if("${sample_${field}_type}" STREQUAL "MISSING" OR
                    "${sample_${field}_type}" STREQUAL "NULL")
                  gbl_finding(result missing-field "phase8.irq.sample.${field}")
                  set(sample_valid FALSE)
                elseif(field STREQUAL "cycle")
                  gbl__result_uint64(sample_cycle_valid sample_cycle "${doc}"
                    phase8 irq sample cycle)
                  if(NOT sample_cycle_valid)
                    gbl_finding(result bad-phase8-type
                      "phase8.irq.sample.cycle must be a uint64 decimal string")
                    set(sample_valid FALSE)
                  endif()
                else()
                  if(field STREQUAL "ime")
                    set(sample_${field}_maximum "1")
                  else()
                    set(sample_${field}_maximum "16383")
                  endif()
                  gbl__result_uint16(sample_${field}_valid sample_${field} "${doc}"
                    "${sample_${field}_maximum}" phase8 irq sample ${field})
                  if(NOT sample_${field}_valid)
                    gbl_finding(result bad-phase8-type
                      "phase8.irq.sample.${field} must be a bounded integer")
                    set(sample_valid FALSE)
                  endif()
                endif()
              endforeach()
              foreach(field IN ITEMS eligible cpsr_i_masked accepted entered)
                gbl_json_type(sample_${field}_type "${doc}" phase8 irq sample ${field})
                if("${sample_${field}_type}" STREQUAL "MISSING" OR
                    "${sample_${field}_type}" STREQUAL "NULL")
                  gbl_finding(result missing-field "phase8.irq.sample.${field}")
                  set(sample_valid FALSE)
                elseif(NOT "${sample_${field}_type}" STREQUAL "BOOLEAN")
                  gbl_finding(result bad-phase8-type
                    "phase8.irq.sample.${field} must be boolean")
                  set(sample_valid FALSE)
                else()
                  string(JSON sample_${field} GET "${doc}" phase8 irq sample ${field})
                endif()
              endforeach()
              if(sample_valid)
                math(EXPR expected_sources "${sample_if} & ${sample_ie}")
                if(NOT "${sample_eligible_sources}" STREQUAL "${expected_sources}" OR
                    (expected_sources GREATER 0 AND NOT sample_eligible) OR
                    (expected_sources EQUAL 0 AND sample_eligible))
                  gbl_finding(result bad-phase8-eligibility
                    "eligible_sources and eligible conflict with sampled IF/IE")
                endif()
                if(sample_eligible AND sample_ime EQUAL 1 AND
                    NOT sample_cpsr_i_masked)
                  set(sample_expected_accepted TRUE)
                else()
                  set(sample_expected_accepted FALSE)
                endif()
                if((sample_accepted AND NOT sample_expected_accepted) OR
                    (NOT sample_accepted AND sample_expected_accepted))
                  gbl_finding(result bad-phase8-eligibility
                    "accepted conflicts with sampled IF/IE/IME/CPSR")
                endif()
                if(sample_entered AND NOT sample_accepted)
                  gbl_finding(result bad-phase8-entry
                    "entered requires accepted IRQ exception")
                endif()
              endif()
            endif()

            gbl_json_type(exception_type "${doc}" phase8 last_exception)
            if("${exception_type}" STREQUAL "MISSING" OR
                "${exception_type}" STREQUAL "NULL")
              gbl_finding(result missing-field "phase8.last_exception")
            elseif(NOT "${exception_type}" STREQUAL "STRING")
              gbl_finding(result bad-phase8-type
                "phase8.last_exception must be string")
            else()
              string(JSON phase8_exception GET "${doc}" phase8 last_exception)
              if(NOT "${phase8_exception}" MATCHES
                  "^(none|reset|undefined|swi|prefetch_abort|data_abort|irq|fiq|coprocessor)$")
                gbl_finding(result bad-phase8-value "phase8.last_exception")
              endif()
              if("${sample_type}" STREQUAL "OBJECT" AND sample_valid AND
                  sample_entered AND NOT "${phase8_exception}" STREQUAL "irq")
                gbl_finding(result bad-phase8-entry
                  "entered requires accepted IRQ exception")
              endif()
              if("${sample_type}" STREQUAL "OBJECT" AND sample_valid AND
                  sample_entered AND "${phase8_exception}" STREQUAL "irq")
                gbl_json_type(phase8_status_type "${doc}" status)
                gbl_json_type(phase8_reason_type "${doc}" reason)
                gbl_json_type(phase8_termination_type "${doc}" termination)
                set(phase8_status "")
                set(phase8_reason "")
                set(phase8_termination "")
                if("${phase8_status_type}" STREQUAL "STRING")
                  string(JSON phase8_status GET "${doc}" status)
                endif()
                if("${phase8_reason_type}" STREQUAL "STRING")
                  string(JSON phase8_reason GET "${doc}" reason)
                endif()
                if("${phase8_termination_type}" STREQUAL "STRING")
                  string(JSON phase8_termination GET "${doc}" termination)
                endif()
                if(NOT "${phase8_status}" STREQUAL "GUEST_STOP" OR
                    NOT "${phase8_reason}" STREQUAL "no-bios-irq" OR
                    NOT "${phase8_termination}" STREQUAL "no-bios-irq")
                  gbl_finding(result bad-phase8-handler-claim
                    "entered IRQ must remain GUEST_STOP with no-bios-irq")
                endif()
              endif()
            endif()

            gbl_json_type(power_type "${doc}" phase8 power)
            if("${power_type}" STREQUAL "MISSING")
              gbl_finding(result missing-field "phase8.power")
            elseif(NOT "${power_type}" STREQUAL "OBJECT")
              gbl_finding(result bad-phase8-type "phase8.power must be object")
            else()
              gbl_json_type(power_state_type "${doc}" phase8 power state)
              gbl_json_type(power_cause_type "${doc}" phase8 power cause)
              foreach(field IN ITEMS state cause)
                if("${power_${field}_type}" STREQUAL "MISSING" OR
                    "${power_${field}_type}" STREQUAL "NULL")
                  gbl_finding(result missing-field "phase8.power.${field}")
                elseif(NOT "${power_${field}_type}" STREQUAL "STRING")
                  gbl_finding(result bad-phase8-type
                    "phase8.power.${field} must be a string")
                endif()
              endforeach()
              if("${power_state_type}" STREQUAL "STRING")
                string(JSON power_state GET "${doc}" phase8 power state)
                if(NOT "${power_state}" MATCHES "^(running|halted|stopped)$")
                  gbl_finding(result bad-phase8-value "phase8.power.state")
                endif()
              endif()
              if("${power_cause_type}" STREQUAL "STRING")
                string(JSON power_cause GET "${doc}" phase8 power cause)
                if(NOT "${power_cause}" MATCHES
                    "^(none|wake_pending|wake_event|stop_requested|halt_deadlock|wait_timeout)$")
                  gbl_finding(result bad-phase8-value "phase8.power.cause")
                endif()
              endif()
              gbl_json_type(wake_source_type "${doc}" phase8 power wake_source_mask)
              gbl_json_type(wake_cycle_type "${doc}" phase8 power wake_cycle)
              if("${power_cause_type}" STREQUAL "STRING" AND
                  "${power_cause}" MATCHES "^(wake_pending|wake_event)$")
                if(NOT "${wake_source_type}" STREQUAL "NUMBER" OR
                    NOT "${wake_cycle_type}" STREQUAL "STRING")
                  gbl_finding(result bad-phase8-wake
                    "wake cause requires source and cycle")
                else()
                  gbl__result_uint16(wake_source_valid wake_source "${doc}" "16383"
                    phase8 power wake_source_mask)
                  gbl__result_uint64(wake_cycle_valid wake_cycle "${doc}"
                    phase8 power wake_cycle)
                  if(NOT wake_source_valid OR "${wake_source}" STREQUAL "0" OR
                      NOT wake_cycle_valid)
                    gbl_finding(result bad-phase8-wake
                      "wake cause requires source and cycle")
                  endif()
                endif()
              elseif(NOT "${wake_source_type}" STREQUAL "NULL" OR
                  NOT "${wake_cycle_type}" STREQUAL "NULL")
                gbl_finding(result bad-phase8-wake
                  "non-wake cause requires null source and cycle")
              endif()
            endif()

            gbl_json_type(keypad_type "${doc}" phase8 keypad)
            if("${keypad_type}" STREQUAL "MISSING")
              gbl_finding(result missing-field "phase8.keypad")
            elseif(NOT "${keypad_type}" STREQUAL "OBJECT")
              gbl_finding(result bad-phase8-type "phase8.keypad must be object")
            else()
              foreach(field IN ITEMS condition irq_condition)
                gbl_json_type(keypad_${field}_type "${doc}" phase8 keypad ${field})
                if("${keypad_${field}_type}" STREQUAL "MISSING" OR
                    "${keypad_${field}_type}" STREQUAL "NULL")
                  gbl_finding(result missing-field "phase8.keypad.${field}")
                elseif(NOT "${keypad_${field}_type}" STREQUAL "BOOLEAN")
                  gbl_finding(result bad-phase8-type
                    "phase8.keypad.${field} must be boolean")
                endif()
              endforeach()
              foreach(field IN ITEMS keyinput keycnt applied_events pending_events)
                gbl_json_type(keypad_${field}_type "${doc}" phase8 keypad ${field})
                if("${keypad_${field}_type}" STREQUAL "MISSING" OR
                    "${keypad_${field}_type}" STREQUAL "NULL")
                  gbl_finding(result missing-field "phase8.keypad.${field}")
                else()
                  if(field STREQUAL "keyinput")
                    set(keypad_${field}_maximum "1023")
                  elseif(field STREQUAL "keycnt")
                    set(keypad_${field}_maximum "65535")
                  else()
                    set(keypad_${field}_maximum "256")
                  endif()
                  gbl__result_uint16(keypad_${field}_valid keypad_${field} "${doc}"
                    "${keypad_${field}_maximum}" phase8 keypad ${field})
                  if(NOT keypad_${field}_valid)
                    gbl_finding(result bad-phase8-type
                      "phase8.keypad.${field} must be a bounded integer")
                  endif()
                endif()
              endforeach()
            endif()

            gbl_json_type(wait_type "${doc}" phase8 wait_cycles)
            if("${wait_type}" STREQUAL "MISSING" OR
                "${wait_type}" STREQUAL "NULL")
              if("${wait_type}" STREQUAL "NULL")
                gbl_finding(result bad-phase8-not-run
                  "observed phase8 requires non-null wait_cycles")
              endif()
              set(phase8_wait_valid FALSE)
              set(time_components_valid FALSE)
            else()
              gbl__result_uint64(phase8_wait_valid phase8_wait_for_sum "${doc}"
                phase8 wait_cycles)
              if(NOT phase8_wait_valid)
                gbl_json_type(wait_type "${doc}" phase8 wait_cycles)
                if("${wait_type}" STREQUAL "STRING")
                  string(JSON invalid_wait GET "${doc}" phase8 wait_cycles)
                  if("${invalid_wait}" MATCHES "^(0|[1-9][0-9]*)$")
                    string(LENGTH "${invalid_wait}" wait_digits)
                    if(wait_digits GREATER 20 OR
                        (wait_digits EQUAL 20 AND
                         "${invalid_wait}" STRGREATER "18446744073709551615"))
                      gbl_finding(result time-overflow "phase8.wait_cycles")
                    else()
                      gbl_finding(result bad-phase8-type
                        "phase8.wait_cycles must be a uint64 decimal string")
                    endif()
                  else()
                    gbl_finding(result bad-phase8-type
                      "phase8.wait_cycles must be a uint64 decimal string")
                  endif()
                else()
                  gbl_finding(result bad-phase8-type
                    "phase8.wait_cycles must be a uint64 decimal string")
                endif()
                set(time_components_valid FALSE)
              endif()
            endif()
          endif()
        endif()
      endif()
    endif()

    gbl__result_phase9_check("${doc}" "${time_cycles}"
      phase9_present phase9_dma_valid)
    if(NOT phase9_dma_valid)
      set(time_components_valid FALSE)
    endif()
    if(phase9_present)
      if(NOT phase8_present)
        gbl_finding(result missing-field "phase8.wait_cycles")
        set(phase9_dma_valid FALSE)
        set(time_components_valid FALSE)
      endif()
      gbl__result_phase9_decimal(time_wait_valid time_wait_cycles "${doc}"
        "time.wait_cycles" time wait_cycles)
      if(NOT time_wait_valid)
        set(phase9_dma_valid FALSE)
        set(time_components_valid FALSE)
      elseif(phase8_wait_valid AND
          NOT "${time_wait_cycles}" STREQUAL "${phase8_wait_for_sum}")
        gbl_finding(result bad-phase9-timing
          "time.wait_cycles differs from phase8.wait_cycles")
        set(phase9_dma_valid FALSE)
        set(time_components_valid FALSE)
      endif()
    endif()

    if(time_valid AND time_components_valid)
      gbl__result_decimal_add(time_component_sum "${time_bus_cycles}"
        "${time_internal_cycles}")
      if(phase8_present)
        gbl__result_decimal_add(time_component_sum "${time_component_sum}"
          "${phase8_wait_for_sum}")
      endif()
      if(phase9_present)
        foreach(field IN ITEMS dma_startup_cycles dma_source_cycles
            dma_destination_cycles)
          gbl__result_decimal_add(time_component_sum "${time_component_sum}"
            "${phase9_${field}}")
        endforeach()
      endif()
      gbl__result_decimal_le(time_component_sum_in_range "${time_component_sum}"
        "18446744073709551615")
      if(NOT time_component_sum_in_range)
        if(phase9_present)
          gbl_finding(result time-sum-overflow
            "CPU, HALT, and DMA timing components exceed UINT64_MAX")
        elseif(phase8_present)
          gbl_finding(result time-sum-overflow
            "time.bus_cycles + time.internal_cycles + phase8.wait_cycles exceeds UINT64_MAX")
        else()
          gbl_finding(result time-sum-overflow
            "time.bus_cycles + time.internal_cycles exceeds UINT64_MAX")
        endif()
      elseif(NOT "${time_component_sum}" STREQUAL "${time_cycles}")
        if(phase9_present)
          gbl_finding(result bad-time-components
            "time.cycles does not equal time.bus_cycles + time.internal_cycles + phase8.wait_cycles + time.dma_startup_cycles + time.dma_source_cycles + time.dma_destination_cycles")
        elseif(phase8_present)
          gbl_finding(result bad-time-components
            "time.cycles does not equal time.bus_cycles + time.internal_cycles + phase8.wait_cycles")
        else()
          gbl_finding(result bad-time-components
            "time.cycles does not equal time.bus_cycles + time.internal_cycles")
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
      # Tie the runtime ROM identity to the manifest row (WR-04): URL/in-tree rows
      # use source.sha256; source-built git rows use optional build.artifact.sha256.
      # Legacy git rows have no output pin and intentionally retain no digest tie.
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

  # ---- manifest-selected result channel ---------------------------------------
  # A result cannot opt into the more permissive register branch by adding JSON.
  # Only the canonical fixture row selects that channel; all legacy rows keep
  # the original mailbox status contract below.
  set(register_channel FALSE)
  set(row_channel_kind "mailbox")
  set(row_channel_source_commit "")
  set(row_channel_entry_address "")
  set(row_channel_instruction_set "")
  set(row_channel_register_index "")
  set(row_channel_digest "")
  if(NOT "${fixture_id}" STREQUAL "" AND "${fixture_id}" IN_LIST m_ids)
    set(row_channel_kind "${${manifest}_ROW_${fixture_id}_CHANNEL_KIND}")
    set(row_channel_source_commit
      "${${manifest}_ROW_${fixture_id}_CHANNEL_SOURCE_COMMIT}")
    set(row_channel_entry_address
      "${${manifest}_ROW_${fixture_id}_CHANNEL_ENTRY_ADDRESS}")
    set(row_channel_instruction_set
      "${${manifest}_ROW_${fixture_id}_CHANNEL_INSTRUCTION_SET}")
    set(row_channel_register_index
      "${${manifest}_ROW_${fixture_id}_CHANNEL_REGISTER_INDEX}")
    set(row_channel_digest "${${manifest}_ROW_${fixture_id}_SHA256}")
    if("${row_channel_kind}" STREQUAL "register-eval")
      set(register_channel TRUE)
    endif()
  endif()

  gbl_json_type(observation_type "${doc}" observation)
  set(observation_observed OFF)
  set(observation_value "")
  set(observation_value_valid FALSE)
  if(NOT register_channel)
    if(NOT "${observation_type}" STREQUAL "MISSING" AND
        NOT "${observation_type}" STREQUAL "NULL")
      gbl_finding(result bad-observation
        "mailbox fixture cannot carry an unbound register observation")
    endif()
  elseif(NOT "${observation_type}" STREQUAL "OBJECT")
    if(NOT "${observation_type}" STREQUAL "MISSING" AND
        NOT "${observation_type}" STREQUAL "NULL")
      gbl_finding(result bad-observation "observation must be an object")
    elseif("${status}" MATCHES "^(PASS|GUEST_FAIL)$" OR
        ("${ran}" STREQUAL "ON" AND "${fix_kind}" STREQUAL "HEX" AND
         NOT "${row_channel_digest}" STREQUAL "" AND
         "${fix_value}" STREQUAL "${row_channel_digest}"))
      gbl_finding(result bad-observation
        "a run with the bound register channel needs its observation object")
    endif()
  else()
    if(NOT "${ran}" STREQUAL "ON" OR NOT "${fix_kind}" STREQUAL "HEX" OR
        "${row_channel_digest}" STREQUAL "" OR
        NOT "${fix_value}" STREQUAL "${row_channel_digest}")
      gbl_finding(result bad-observation
        "register observation needs a run with the exact manifest ROM digest")
    endif()

    string(JSON observation_key_count LENGTH "${doc}" observation)
    if(NOT observation_key_count EQUAL 7)
      gbl_finding(result bad-observation
        "observation must contain exactly its seven contract fields")
    endif()
    if(observation_key_count GREATER 0)
      math(EXPR observation_key_last "${observation_key_count}-1")
      foreach(observation_key_index RANGE ${observation_key_last})
        string(JSON observation_key MEMBER "${doc}" observation
          ${observation_key_index})
        if(NOT "${observation_key}" MATCHES
            "^(kind|observed|source_commit|entry_address|instruction_set|register_index|value)$")
          gbl_finding(result bad-observation
            "observation has unknown field ${observation_key}")
        endif()
      endforeach()
    endif()

    gbl_json_type(observation_kind_type "${doc}" observation kind)
    if(NOT "${observation_kind_type}" STREQUAL "STRING")
      gbl_finding(result bad-observation "observation.kind must be a string")
    else()
      string(JSON observation_kind GET "${doc}" observation kind)
      if(NOT "${observation_kind}" STREQUAL "register-eval")
        gbl_finding(result bad-observation "observation.kind differs from the manifest channel")
      endif()
    endif()
    gbl_json_type(observation_observed_type "${doc}" observation observed)
    if(NOT "${observation_observed_type}" STREQUAL "BOOLEAN")
      gbl_finding(result bad-observation "observation.observed must be a boolean")
    else()
      string(JSON observation_observed GET "${doc}" observation observed)
    endif()

    foreach(field IN ITEMS source_commit entry_address instruction_set)
      gbl_json_type(observation_${field}_type "${doc}" observation ${field})
      if(NOT "${observation_${field}_type}" STREQUAL "STRING")
        gbl_finding(result bad-observation "observation.${field} must be a string")
      else()
        string(JSON observation_${field} GET "${doc}" observation ${field})
        if(NOT "${observation_${field}}" STREQUAL "${row_channel_${field}}")
          gbl_finding(result bad-observation
            "observation.${field} differs from the canonical manifest")
        endif()
      endif()
    endforeach()

    gbl_json_type(observation_register_type "${doc}" observation register_index)
    if(NOT "${observation_register_type}" STREQUAL "NUMBER")
      gbl_finding(result bad-observation "observation.register_index must be an integer")
    else()
      string(JSON observation_register GET "${doc}" observation register_index)
      if(NOT "${observation_register}" MATCHES "^(0|[1-9][0-9]*)$" OR
          NOT "${observation_register}" STREQUAL "${row_channel_register_index}")
        gbl_finding(result bad-observation
          "observation.register_index differs from the canonical manifest")
      endif()
    endif()

    gbl_json_type(observation_value_type "${doc}" observation value)
    if("${observation_observed}" STREQUAL "ON")
      if(NOT "${observation_value_type}" STREQUAL "NUMBER")
        gbl_finding(result bad-observation
          "an observed register value must be a uint32 JSON number")
      else()
        string(JSON observation_value GET "${doc}" observation value)
        if(NOT "${observation_value}" MATCHES "^(0|[1-9][0-9]*)$")
          gbl_finding(result bad-observation
            "observation.value is not a canonical uint32")
        else()
          string(LENGTH "${observation_value}" observation_value_digits)
          if(observation_value_digits GREATER 10 OR
              (observation_value_digits EQUAL 10 AND
               "${observation_value}" STRGREATER "4294967295"))
            gbl_finding(result bad-observation "observation.value exceeds uint32")
          else()
            set(observation_value_valid TRUE)
          endif()
        endif()
      endif()
    elseif(NOT "${observation_value_type}" STREQUAL "NULL")
      gbl_finding(result bad-observation
        "an unobserved register value must be null")
    endif()

    if("${observation_observed}" STREQUAL "ON" AND
        observation_value_valid)
      if(NOT "${termination}" STREQUAL "register-eval-complete")
        gbl_finding(result bad-observation
          "an observed register requires register-eval-complete termination")
      endif()
      if("${observation_value}" STREQUAL "0")
        if(NOT "${status}" STREQUAL "PASS")
          gbl_finding(result bad-observation
            "zero register result requires PASS")
        endif()
      elseif(NOT "${status}" STREQUAL "GUEST_FAIL")
        gbl_finding(result bad-observation
          "nonzero register result requires GUEST_FAIL")
      endif()
      if(NOT time_valid OR "${time_cycles}" STREQUAL "0" OR
          "${time_instructions}" STREQUAL "0")
        gbl_finding(result bad-observation
          "register result requires executed guest progress")
      endif()
    elseif("${observation_observed}" STREQUAL "OFF" AND
        "${status}" MATCHES "^(PASS|GUEST_FAIL)$")
      gbl_finding(result bad-observation
        "pending register observation cannot produce PASS or GUEST_FAIL")
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

  # Bind mailbox rows to the actual mailbox, or register rows to the observation
  # bound above. The two routes stay separate so adding JSON cannot relax the
  # historical mailbox contract.
  gbl_json_string(result_reason "${doc}" reason)
  if("${status}" STREQUAL "PASS")
    if(NOT register_channel)
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
    endif()
    if(time_valid AND ("${time_cycles}" STREQUAL "0" OR
        "${time_instructions}" STREQUAL "0"))
      gbl_finding(result status-semantic-mismatch "PASS requires executed guest progress")
    endif()
  elseif("${status}" STREQUAL "GUEST_FAIL")
    if(NOT register_channel)
      if(NOT "${semantic_state}" STREQUAL "fail")
        gbl_finding(result status-semantic-mismatch
          "GUEST_FAIL requires semantic.state fail")
      endif()
      if(NOT "${termination}" STREQUAL "mailbox-complete")
        gbl_finding(result bad-termination "GUEST_FAIL requires mailbox-complete")
      endif()
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

  gbl__result_phase10_check("${doc}" "${time_cycles}" "${status}" "${ran}"
    "${result_reason}")

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
