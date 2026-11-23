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
string(JSON UNMODELED_COUNT LENGTH "${base_profile}" unmodeled)
math(EXPR NEXT_PROFILE_VERSION "${PROFILE_VERSION}+1")
math(EXPR NEXT_PROFILE_UNMODELED_COUNT "${UNMODELED_COUNT}+1")

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

# PPU mutation controls exercise the same production policy and document
# validators without repeating unrelated timing/CPU profile checks.
function(check_ppu_command out profile doc)
  set(${out}
    "${CMAKE_COMMAND}" "-DPROFILE=${profile}" "-DDOC=${doc}"
    "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}"
    -DGBL_PROFILE_PPU_ONLY=ON -P "${CHECK}"
    PARENT_SCOPE)
endfunction()

# Audio mutation controls exercise the production audio policy validator and
# its synchronized documentation tables without unrelated profile checks.
function(check_audio_command out profile doc)
  set(${out}
    "${CMAKE_COMMAND}" "-DPROFILE=${profile}" "-DDOC=${doc}"
    "-DREGISTRY=${REGISTRY_COPY}" "-DVOCAB=${VOCAB_COPY}"
    -DGBL_PROFILE_AUDIO_ONLY=ON -P "${CHECK}"
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

function(expect_ppu_detail code detail profile)
  if(ARGC GREATER 3)
    set(doc "${ARGV3}")
  else()
    doc_copy(doc unchanged_ppu_detail)
  endif()
  check_ppu_command(cmd "${profile}" "${doc}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "profile: ${code}: ${detail}")
endfunction()

function(expect_audio_detail code detail profile)
  if(ARGC GREATER 3)
    set(doc "${ARGV3}")
  else()
    doc_copy(doc unchanged_audio_detail)
  endif()
  check_audio_command(cmd "${profile}" "${doc}")
  gbl_expect_fail(COMMAND ${cmd} REGEX "profile: ${code}: ${detail}")
endfunction()

if(CASE STREQUAL "copy")
  profile_copy(p copy)
  doc_copy(d copy)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, ${UNMODELED_COUNT} unmodeled\\)")

elseif(CASE STREQUAL "audio_policy")
  profile_copy(p clean_audio_policy)
  doc_copy(d clean_audio_policy)
  check_audio_command(audio_cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${audio_cmd}
    REGEX "profile: ok \\(agb audio policy\\)")

  profile_copy(p missing_psg_first_tick)
  gbl_json_mutate_remove("${p}" audio_policy psg_sequencer_first_tick_cycle)
  expect_audio_detail(audio-policy-value
    "audio_policy.psg_sequencer_first_tick_cycle: expected NUMBER, found MISSING" "${p}")

  profile_copy(p wrong_psg_first_step)
  gbl_json_mutate_set("${p}" "1" audio_policy psg_sequencer_first_step)
  expect_audio_detail(audio-policy-value
    "audio_policy.psg_sequencer_first_step: expected 0" "${p}")

  profile_copy(p wrong_psg_source)
  gbl_json_mutate_set("${p}" [=["unreviewed source interpretation"]=]
    audio_policy psg_ch3_rate_policy)
  expect_audio_detail(audio-policy-value
    "audio_policy.psg_ch3_rate_policy: expected manual-whole-pattern-derived" "${p}")

  profile_copy(p wrong_psg_bank)
  gbl_json_mutate_set("${p}" [=["selected-front-bank"]=]
    audio_policy psg_wave_guest_bank_policy)
  expect_audio_detail(audio-policy-value
    "audio_policy.psg_wave_guest_bank_policy: expected bank-opposite-SOUND3CNT_L-front-bank" "${p}")

  profile_copy(p wrong_psg_noise_mask)
  gbl_json_mutate_set("${p}" [=["15-bit=0x6000,7-bit=0x40"]=]
    audio_policy psg_noise_feedback_masks)
  expect_audio_detail(audio-policy-value
    "audio_policy.psg_noise_feedback_masks: expected 15-bit=0x6000,7-bit=0x60" "${p}")

  profile_copy(p psg_policy_order_drift)
  doc_copy(d psg_policy_order_drift)
  file(READ "${d}" psg_order_before)
  string(REPLACE
    "| `audio_policy.psg_sequencer_period_cycles` | 32768 |\n| `audio_policy.psg_sequencer_rate_hz` | 512 |"
    "| `audio_policy.psg_sequencer_rate_hz` | 512 |\n| `audio_policy.psg_sequencer_period_cycles` | 32768 |"
    psg_order_after "${psg_order_before}")
  if("${psg_order_after}" STREQUAL "${psg_order_before}")
    message(FATAL_ERROR "mutate: no-match: PSG policy table order")
  endif()
  file(WRITE "${d}" "${psg_order_after}")
  expect_audio_detail(audio-policy-doc
    "audio_policy: missing or stale row" "${p}" "${d}")

  profile_copy(p psg_stale_bank_documentation)
  doc_copy(d psg_stale_bank_documentation)
  file(READ "${d}" psg_bank_doc_before)
  string(REPLACE
    "| `audio_policy.psg_wave_guest_bank_policy` | bank-opposite-SOUND3CNT_L-front-bank |"
    "| `audio_policy.psg_wave_guest_bank_policy` | selected-front-bank |"
    psg_bank_doc_after "${psg_bank_doc_before}")
  if("${psg_bank_doc_after}" STREQUAL "${psg_bank_doc_before}")
    message(FATAL_ERROR "mutate: no-match: PSG policy documentation row")
  endif()
  file(WRITE "${d}" "${psg_bank_doc_after}")
  expect_audio_detail(audio-policy-doc
    "audio_policy: missing or stale row" "${p}" "${d}")

  profile_copy(p wrong_audio_first_sample)
  gbl_json_mutate_set("${p}" "513" audio_policy first_sample_cycle)
  expect_audio_detail(audio-policy-value
    "audio_policy\\.first_sample_cycle: expected 512" "${p}")

  profile_copy(p per_operation_audio_scope)
  gbl_json_mutate_set("${p}" [=["per-operation"]=]
    audio_policy capture_bound_scope)
  expect_audio_detail(audio-policy-value
    "audio_policy\\.capture_bound_scope: expected caller-owned-session" "${p}")

  profile_copy(p wrong_audio_byte_bound)
  gbl_json_mutate_set("${p}" "131071" audio_policy max_capture_bytes)
  expect_audio_detail(audio-policy-value
    "audio_policy\\.max_capture_bytes: expected 131072" "${p}")

  profile_copy(p wrong_audio_encoding)
  gbl_json_mutate_set("${p}" [=["signed-s24le"]=]
    audio_policy encoding)
  expect_audio_detail(audio-policy-value
    "audio_policy\\.encoding: expected signed-s16le" "${p}")

  profile_copy(p missing_audio_policy)
  gbl_json_mutate_remove("${p}" audio_policy)
  expect_audio_detail(audio-policy-missing
    "audio_policy: missing or not an object" "${p}")

  profile_copy(p unsupported_audio_qualification)
  gbl_json_mutate_set("${p}" [=["qualified"]=]
    audio_policy hardware_qualification)
  expect_audio_detail(audio-policy-value
    "audio_policy\\.hardware_qualification: expected UNKNOWN" "${p}")

  profile_copy(p wrong_soundcntx_action_mask)
  gbl_json_mutate_set("${p}" [=["0x8800"]=]
    audio_policy registers 12 action_mask)
  expect_audio_detail(audio-policy-value
    "audio_policy.registers: exact register policy differs" "${p}")

  profile_copy(p wrong_channel_length_enable_read_mask)
  gbl_json_mutate_set("${p}" [=["0x0000"]=]
    audio_policy registers 2 read_mask)
  expect_audio_detail(audio-policy-value
    "audio_policy.registers: exact register policy differs" "${p}")

  profile_copy(p documentation_audio_policy)
  doc_copy(d audio_policy_doc_drift)
  file(READ "${d}" audio_doc_before)
  string(REPLACE
    "| `audio_policy.first_sample_cycle` | 512 |"
    "| `audio_policy.first_sample_cycle` | 513 |"
    audio_doc_after "${audio_doc_before}")
  if("${audio_doc_after}" STREQUAL "${audio_doc_before}")
    message(FATAL_ERROR "mutate: no-match: audio policy documentation row")
  endif()
  file(WRITE "${d}" "${audio_doc_after}")
  expect_audio_detail(audio-policy-doc
    "audio_policy: missing or stale row" "${p}" "${d}")

  doc_copy(d audio_register_doc_drift)
  file(READ "${d}" audio_register_doc_before)
  string(REPLACE
    "| `audio_policy.registers.SOUND1CNT_X` | `SOUND1CNT_X` | `0x04000064` | `0x47FF` | `0x47FF` | `0x8000` | length-enable-retained-trigger-self-clears-separate-status |"
    "| `audio_policy.registers.SOUND1CNT_X` | `SOUND1CNT_X` | `0x04000064` | `0x47FF` | `0x0000` | `0x8000` | length-enable-retained-trigger-self-clears-separate-status |"
    audio_register_doc_after "${audio_register_doc_before}")
  if("${audio_register_doc_after}" STREQUAL "${audio_register_doc_before}")
    message(FATAL_ERROR "mutate: no-match: audio register documentation row")
  endif()
  file(WRITE "${d}" "${audio_register_doc_after}")
  expect_audio_detail(audio-policy-doc
    "audio_policy.registers: missing or stale row" "${p}" "${d}")

  doc_copy(d soundcntx_action_mask_doc_drift)
  file(READ "${d}" audio_soundcntx_doc_before)
  string(REPLACE
    "| `audio_policy.registers.SOUNDCNT_X` | `SOUNDCNT_X` | `0x04000084` | `0x0080` | `0x008F` | `0x0000` | master-enable-dynamic-channel-status-disable-clears-channel-state |"
    "| `audio_policy.registers.SOUNDCNT_X` | `SOUNDCNT_X` | `0x04000084` | `0x0080` | `0x008F` | `0x8800` | master-enable-dynamic-channel-status-disable-clears-channel-state |"
    audio_soundcntx_doc_after "${audio_soundcntx_doc_before}")
  if("${audio_soundcntx_doc_after}" STREQUAL "${audio_soundcntx_doc_before}")
    message(FATAL_ERROR "mutate: no-match: SOUNDCNT_X action-mask documentation row")
  endif()
  file(WRITE "${d}" "${audio_soundcntx_doc_after}")
  expect_audio_detail(audio-policy-doc
    "audio_policy.registers: missing or stale row" "${p}" "${d}")

elseif(CASE STREQUAL "audio_mix")
  profile_copy(p clean_audio_mix)
  doc_copy(d clean_audio_mix)
  check_audio_command(audio_cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${audio_cmd}
    REGEX "profile: ok \\(agb audio policy\\)")

  profile_copy(p wrong_psg_mixer_centering)
  gbl_json_mutate_set("${p}" [=["per-channel-(2*level-15)*2048"]=]
    audio_policy psg_source_centering)
  expect_audio_detail(audio-policy-value
    "audio_policy.psg_source_centering: expected per-channel" "${p}")

  profile_copy(p wrong_psg_ratio_weight)
  gbl_json_mutate_set("${p}" [=["SOUNDCNT_H-code-0-half,code-1-quarter,code-2-full,code-3-mutes-PSG"]=]
    audio_policy psg_group_ratio_policy)
  expect_audio_detail(audio-policy-value
    "audio_policy.psg_group_ratio_policy: expected SOUNDCNT_H-code-0-quarter"
    "${p}")

  profile_copy(p wrong_soundbias_cadence)
  gbl_json_mutate_set("${p}" [=["changes-sample-intervals"]=]
    audio_policy soundbias cadence_effect)
  expect_audio_detail(audio-policy-value
    "audio_policy.soundbias.cadence_effect: expected none" "${p}")

  profile_copy(p qualified_audio_policy)
  gbl_json_mutate_set("${p}" [=["qualified-on-hardware"]=]
    audio_policy hardware_qualification)
  expect_audio_detail(audio-policy-value
    "audio_policy.hardware_qualification: expected UNKNOWN" "${p}")

  profile_copy(p audio_mix_doc_source)
  doc_copy(d audio_mix_doc_source)
  file(READ "${d}" doc_before)
  string(REPLACE
    "| `audio_policy.psg_group_ratio_policy` | SOUNDCNT_H-code-0-quarter,code-1-half,code-2-full,code-3-mutes-PSG |"
    "| `audio_policy.psg_group_ratio_policy` | SOUNDCNT_H-code-0-half,code-1-quarter,code-2-full,code-3-mutes-PSG |"
    doc_after "${doc_before}")
  if("${doc_after}" STREQUAL "${doc_before}")
    message(FATAL_ERROR "mutate: no-match: audio group ratio documentation row")
  endif()
  file(WRITE "${d}" "${doc_after}")
  expect_audio_detail(audio-policy-doc
    "audio_policy: missing or stale row" "${p}" "${d}")

  profile_copy(p audio_mix_doc_qualification)
  doc_copy(d audio_mix_doc_qualification)
  file(READ "${d}" qual_before)
  string(REPLACE
    "| `audio_policy.hardware_qualification` | UNKNOWN |"
    "| `audio_policy.hardware_qualification` | qualified-on-hardware |"
    qual_after "${qual_before}")
  if("${qual_after}" STREQUAL "${qual_before}")
    message(FATAL_ERROR "mutate: no-match: audio qualification documentation row")
  endif()
  file(WRITE "${d}" "${qual_after}")
  expect_audio_detail(audio-policy-doc
    "audio_policy: missing or stale row" "${p}" "${d}")

elseif(CASE STREQUAL "dma_sound_trigger")
  profile_copy(p clean_dma_sound_trigger)
  doc_copy(d clean_dma_sound_trigger)
  check_audio_command(audio_cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${audio_cmd}
    REGEX "profile: ok \\(agb audio policy\\)")
  check_command(full_cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${full_cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, ${UNMODELED_COUNT} unmodeled\\)")

  profile_copy(p wrong_direct_sound_threshold)
  gbl_json_mutate_set("${p}" "17"
    audio_policy direct_sound_fifo_request_threshold_bytes)
  expect_audio_detail(audio-policy-value
    "audio_policy.direct_sound_fifo_request_threshold_bytes: expected 16" "${p}")

  profile_copy(p wrong_direct_sound_threshold_type)
  gbl_json_mutate_set("${p}" [=["16"]=]
    audio_policy direct_sound_fifo_request_threshold_bytes)
  expect_audio_detail(audio-policy-value
    "audio_policy.direct_sound_fifo_request_threshold_bytes: expected NUMBER, found STRING" "${p}")

  profile_copy(p stale_dma_trigger_json)
  doc_copy(d stale_dma_trigger_json)
  gbl_json_mutate_set("${p}" [=["old trigger producers are deferred"]=]
    dma_policy trigger_policy deferred_behavior)
  expect_detail(dma-policy "trigger producer and refusal policy is missing or stale"
    "${p}" "${d}")

  profile_copy(p stale_dma_trigger_table)
  doc_copy(d stale_dma_trigger_table)
  file(READ "${d}" trigger_doc_before)
  string(REPLACE
    "Direct Sound FIFO A/B uses DMA1/DMA2 special timing"
    "Direct Sound audio trigger producer remains deferred"
    trigger_doc_after "${trigger_doc_before}")
  if("${trigger_doc_after}" STREQUAL "${trigger_doc_before}")
    message(FATAL_ERROR "mutate: no-match: linked DMA producer table")
  endif()
  file(WRITE "${d}" "${trigger_doc_after}")
  expect_detail(dma-policy-doc
    "dma_policy.trigger_policy: linked producer declaration or citation row is stale"
    "${p}" "${d}")

  profile_copy(p direct_sound_qualification_escalation)
  gbl_json_mutate_set("${p}" [=["qualified"]=]
    audio_policy direct_sound_hardware_qualification)
  expect_audio_detail(audio-policy-value
    "audio_policy.direct_sound_hardware_qualification: expected UNKNOWN" "${p}")

elseif(CASE STREQUAL "ppu_policy")
  if(NOT "${PROFILE_VERSION}" STREQUAL "16")
    message(FATAL_ERROR
      "profile OBJ/composition capability assertion: expected version 16, found ${PROFILE_VERSION}")
  endif()
  string(JSON mode0_status GET "${base_profile}"
    ppu_policy display_capabilities modes 0 implementation_status)
  string(JSON mode1_status GET "${base_profile}"
    ppu_policy display_capabilities modes 1 implementation_status)
  string(JSON mode2_status GET "${base_profile}"
    ppu_policy display_capabilities modes 2 implementation_status)
  string(JSON mode3_status GET "${base_profile}"
    ppu_policy display_capabilities modes 3 implementation_status)
  string(JSON mode4_status GET "${base_profile}"
    ppu_policy display_capabilities modes 4 implementation_status)
  string(JSON mode5_status GET "${base_profile}"
    ppu_policy display_capabilities modes 5 implementation_status)
  if(NOT "${mode0_status}" STREQUAL "partial" OR
      NOT "${mode1_status}" STREQUAL "partial" OR
      NOT "${mode2_status}" STREQUAL "partial" OR
      NOT "${mode3_status}" STREQUAL "partial" OR
      NOT "${mode4_status}" STREQUAL "partial" OR
      NOT "${mode5_status}" STREQUAL "partial")
    message(FATAL_ERROR
      "profile display capability assertion: all six modes must be partial, found ${mode0_status}/${mode1_status}/${mode2_status}/${mode3_status}/${mode4_status}/${mode5_status}")
  endif()

  string(JSON object_type ERROR_VARIABLE object_type_error TYPE
    "${base_profile}" ppu_policy object_policy)
  if(NOT "${object_type_error}" STREQUAL "NOTFOUND" OR
      NOT "${object_type}" STREQUAL "OBJECT")
    message(FATAL_ERROR
      "profile object policy assertion: expected OBJECT, found ${object_type}")
  endif()
  string(JSON object_fields LENGTH "${base_profile}"
    ppu_policy object_policy)
  if(NOT object_fields EQUAL 14)
    message(FATAL_ERROR
      "profile object policy assertion: expected 14 fields, found ${object_fields}")
  endif()
  string(JSON actual_object_policy GET "${base_profile}"
    ppu_policy object_policy)
  set(expected_object_policy [=[{"regular_shapes":"square:8x8,16x16,32x32,64x64;horizontal:16x8,32x8,32x16,64x32;vertical:8x16,8x32,16x32,32x64","character_windows":"modes0-2:[0x10000,0x18000);modes3-5:[0x14000,0x18000);full-tile-span-before-read","index_zero_transparency":true,"affine_quantization":"signed-8.8;wide-products;floor-by-256","affine_field":"centered;double-size-destination-only;out-of-source-transparent","coordinate_wrap":"x-modulo-512;y-modulo-256;clip-240x160","unsupported_modes":"prohibited","work_budget_units":1226,"regular_cost":"source-width","affine_cost":"10+2*destination-field-width","eligibility":"enabled-supported-object;vertical-field-intersects-row;no-horizontal-or-transparency-refund","admission_order":"ascending-oam;full-cost-fit;stop-at-first-nonfit","overflow_policy":"valid-partial-scene;omit-all-later-eligible","observation_scope":"internal-row-capture;selected-mask;selected-count;omitted-count;used-work;no-stable-c-api"}]=])
  string(JSON object_policy_matches EQUAL
    "${actual_object_policy}" "${expected_object_policy}")
  if(NOT object_policy_matches)
    message(FATAL_ERROR
      "profile object policy assertion: differs from exact 14-field policy")
  endif()

  foreach(policy IN ITEMS window_policy effect_policy mosaic_policy forced_blank_policy)
    string(JSON policy_type ERROR_VARIABLE policy_type_error TYPE
      "${base_profile}" ppu_policy ${policy})
    if(NOT "${policy_type_error}" STREQUAL "NOTFOUND" OR
        NOT "${policy_type}" STREQUAL "OBJECT")
      message(FATAL_ERROR "profile ${policy} assertion: expected OBJECT, found ${policy_type}")
    endif()
  endforeach()
  string(JSON actual_window_policy GET "${base_profile}" ppu_policy window_policy)
  set(expected_window_policy [=[{"region_order":"win0;win1;obj-window;outside","rectangle_bounds":"half-open;equal-or-reversed-empty;no-wrap","layer_stage":"filter-before-priority;dispcnt-master-gates","disabled_masters":"bypass-winin-winout","obj_coverage":"admitted-nonzero-source-index;no-color;shared-obj-budget","mask_bits":"bg0-bg3;obj;effects;reserved-ignored"}]=])
  string(JSON window_policy_matches EQUAL "${actual_window_policy}" "${expected_window_policy}")
  if(NOT window_policy_matches)
    message(FATAL_ERROR "profile window policy assertion: differs from exact policy")
  endif()
  string(JSON actual_effect_policy GET "${base_profile}" ppu_policy effect_policy)
  set(expected_effect_policy [=[{"pair":"top-and-immediate-next-visible;no-deeper-search","alpha":"mode1;first-target;second-target;region-effects-enabled","semitransparent_obj":"alpha-with-eligible-next-and-region;ignore-ordinary-first-target-and-mode;no-brightness","coefficient_max":16,"coefficient_input":"low-five-bits;clamp-to-16","channel_arithmetic":"wide-integer;divide16-truncate;clamp0-31","brighten":"c+floor((31-c)*evy/16)","darken":"c-floor(c*evy/16)"}]=])
  string(JSON effect_policy_matches EQUAL "${actual_effect_policy}" "${expected_effect_policy}")
  if(NOT effect_policy_matches)
    message(FATAL_ERROR "profile effect policy assertion: differs from exact policy")
  endif()
  string(JSON actual_mosaic_policy GET "${base_profile}" ppu_policy mosaic_policy)
  set(expected_mosaic_policy [=[{"dimensions":"field-plus-one;independent-bg-obj","anchors":"visible-screen-origin;x-x%w;y-y%h","sampling":"current-row-captured-registers-and-memory;no-old-row-cache","affine_origin":"current-origin-minus-pb-pd-times-row-anchor-delta","stage":"source-before-destination-window-and-effects","obj_field":"existing-destination-field;anchor-outside-transparent;no-field-growth"}]=])
  string(JSON mosaic_policy_matches EQUAL "${actual_mosaic_policy}" "${expected_mosaic_policy}")
  if(NOT mosaic_policy_matches)
    message(FATAL_ERROR "profile mosaic policy assertion: differs from exact policy")
  endif()
  string(JSON actual_forced_blank_policy GET "${base_profile}" ppu_policy forced_blank_policy)
  set(expected_forced_blank_policy [=[{"color_bgr555":32767,"raw_bytes":"FF 7F","transitions":"next-captured-visible-row;no-extra-delay-or-restart","frame_validity":"normal-epoch-row-coverage;publish-line160","precedence":"bypass-scene-decode;retain-structural-transaction-validation","access_policy":"unchanged-existing-bus-behavior"}]=])
  string(JSON forced_blank_policy_matches EQUAL "${actual_forced_blank_policy}" "${expected_forced_blank_policy}")
  if(NOT forced_blank_policy_matches)
    message(FATAL_ERROR "profile forced blank policy assertion: differs from exact policy")
  endif()

  string(JSON composition_type ERROR_VARIABLE composition_type_error TYPE
    "${base_profile}" ppu_policy composition_policy)
  if(NOT "${composition_type_error}" STREQUAL "NOTFOUND" OR
      NOT "${composition_type}" STREQUAL "OBJECT")
    message(FATAL_ERROR
      "profile composition policy assertion: expected OBJECT, found ${composition_type}")
  endif()
  string(JSON composition_fields LENGTH "${base_profile}"
    ppu_policy composition_policy)
  if(NOT composition_fields EQUAL 5)
    message(FATAL_ERROR
      "profile composition policy assertion: expected 5 fields, found ${composition_fields}")
  endif()
  string(JSON actual_composition_policy GET "${base_profile}"
    ppu_policy composition_policy)
  set(expected_composition_policy [=[{"priority_order":"ascending-numeric-0-3","bg_tie":"lower-bg-number","obj_tie":"lower-oam-index","obj_bg_tie":"obj-in-front-project-policy","backdrop":"pram-color-zero;behind-all-enabled-opaque-candidates"}]=])
  string(JSON composition_policy_matches EQUAL
    "${actual_composition_policy}" "${expected_composition_policy}")
  if(NOT composition_policy_matches)
    message(FATAL_ERROR
      "profile composition policy assertion: differs from exact 5-field policy")
  endif()

  string(JSON capability_model GET "${base_profile}"
    ppu_policy display_capabilities model)
  if(NOT "${capability_model}" STREQUAL "gamebloke-display-capabilities-2")
    message(FATAL_ERROR "profile display capability assertion: expected model 2")
  endif()
  string(JSON actual_composition_features GET "${base_profile}"
    ppu_policy display_capabilities composition_features)
  set(expected_composition_features [=[{"rectangular_windows":true,"object_windows":true,"alpha_blending":true,"brightness":true,"bg_mosaic":true,"obj_mosaic":true,"forced_blank":true,"classification":"project-selected","hardware_qualification":"UNKNOWN"}]=])
  string(JSON composition_features_matches EQUAL
    "${actual_composition_features}" "${expected_composition_features}")
  if(NOT composition_features_matches)
    message(FATAL_ERROR "profile composition feature assertion: differs from exact nine-field tuple")
  endif()

  profile_copy(p clean_ppu_policy)
  doc_copy(d clean_ppu_policy)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, ${UNMODELED_COUNT} unmodeled\\)")
  check_ppu_command(ppu_cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${ppu_cmd} REGEX "profile: ok \\(agb PPU policy\\)")

  profile_copy(p wrong_object_work_budget)
  gbl_json_mutate_set("${p}" "1225"
    ppu_policy object_policy work_budget_units)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy\\.work_budget_units" "${p}")

  profile_copy(p stale_profile_version)
  gbl_json_mutate_set("${p}" "14" profile_version)
  expect_ppu_detail(ppu-policy-value "profile_version: expected 15" "${p}")

  profile_copy(p legacy_object_mode_exclusions)
  gbl_json_mutate_set("${p}" [=["prohibited;semitransparent;obj-window;mosaic"]=]
    ppu_policy object_policy unsupported_modes)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy\\.unsupported_modes" "${p}")

  profile_copy(p wrong_window_precedence)
  gbl_json_mutate_set("${p}" [=["win1;win0;obj-window;outside"]=]
    ppu_policy window_policy region_order)
  expect_ppu_detail(ppu-window-policy-value
    "ppu_policy\\.window_policy\\.region_order" "${p}")

  profile_copy(p wrong_window_wrap)
  gbl_json_mutate_set("${p}" [=["half-open;equal-or-reversed-empty;wrap"]=]
    ppu_policy window_policy rectangle_bounds)
  expect_ppu_detail(ppu-window-policy-value
    "ppu_policy\\.window_policy\\.rectangle_bounds" "${p}")

  profile_copy(p missing_window_policy)
  gbl_json_mutate_remove("${p}" ppu_policy window_policy)
  expect_ppu_detail(ppu-window-policy-value "ppu_policy\\.window_policy" "${p}")

  profile_copy(p null_window_policy_field)
  gbl_json_mutate_set("${p}" null ppu_policy window_policy rectangle_bounds)
  expect_ppu_detail(ppu-window-policy-value
    "ppu_policy\\.window_policy\\.rectangle_bounds: expected STRING" "${p}")

  profile_copy(p extra_window_policy_field)
  gbl_json_mutate_set("${p}" [=["unexpected"]=] ppu_policy window_policy unexpected)
  expect_ppu_detail(ppu-window-policy-value
    "ppu_policy\\.window_policy: expected 6 fields" "${p}")

  profile_copy(p deeper_effect_search)
  gbl_json_mutate_set("${p}" [=["top-and-any-later-target;deeper-search"]=]
    ppu_policy effect_policy pair)
  expect_ppu_detail(ppu-effect-policy-value "ppu_policy\\.effect_policy\\.pair" "${p}")

  profile_copy(p unclamped_effect_coefficient)
  gbl_json_mutate_set("${p}" [=["low-five-bits;no-clamp"]=]
    ppu_policy effect_policy coefficient_input)
  expect_ppu_detail(ppu-effect-policy-value
    "ppu_policy\\.effect_policy\\.coefficient_input" "${p}")

  profile_copy(p effect_coefficient_wrong_type)
  gbl_json_mutate_set("${p}" [=["16"]=] ppu_policy effect_policy coefficient_max)
  expect_ppu_detail(ppu-effect-policy-value
    "ppu_policy\\.effect_policy\\.coefficient_max: expected NUMBER" "${p}")

  profile_copy(p missing_effect_policy)
  gbl_json_mutate_remove("${p}" ppu_policy effect_policy)
  expect_ppu_detail(ppu-effect-policy-value "ppu_policy\\.effect_policy" "${p}")

  profile_copy(p extra_effect_policy_field)
  gbl_json_mutate_set("${p}" "true" ppu_policy effect_policy unexpected)
  expect_ppu_detail(ppu-effect-policy-value
    "ppu_policy\\.effect_policy: expected 8 fields" "${p}")

  profile_copy(p stale_row_mosaic)
  gbl_json_mutate_set("${p}" [=["previous-row-cache;anchor-pixel"]=]
    ppu_policy mosaic_policy sampling)
  expect_ppu_detail(ppu-mosaic-policy-value "ppu_policy\\.mosaic_policy\\.sampling" "${p}")

  profile_copy(p wrong_mosaic_anchor)
  gbl_json_mutate_set("${p}" [=["obj-relative;x-x%w;y-y%h"]=]
    ppu_policy mosaic_policy anchors)
  expect_ppu_detail(ppu-mosaic-policy-value "ppu_policy\\.mosaic_policy\\.anchors" "${p}")

  profile_copy(p missing_mosaic_policy)
  gbl_json_mutate_remove("${p}" ppu_policy mosaic_policy)
  expect_ppu_detail(ppu-mosaic-policy-value "ppu_policy\\.mosaic_policy" "${p}")

  profile_copy(p extra_mosaic_policy_field)
  gbl_json_mutate_set("${p}" "true" ppu_policy mosaic_policy unexpected)
  expect_ppu_detail(ppu-mosaic-policy-value
    "ppu_policy\\.mosaic_policy: expected 6 fields" "${p}")

  profile_copy(p invalid_forced_blank_output)
  gbl_json_mutate_set("${p}" "0" ppu_policy forced_blank_policy color_bgr555)
  expect_ppu_detail(ppu-forced-blank-policy-value
    "ppu_policy\\.forced_blank_policy\\.color_bgr555" "${p}")

  profile_copy(p invalid_forced_blank_bytes)
  gbl_json_mutate_set("${p}" [=["00 00"]=]
    ppu_policy forced_blank_policy raw_bytes)
  expect_ppu_detail(ppu-forced-blank-policy-value
    "ppu_policy\\.forced_blank_policy\\.raw_bytes" "${p}")

  profile_copy(p forced_blank_access_change)
  gbl_json_mutate_set("${p}" [=["denied-during-drawing"]=]
    ppu_policy forced_blank_policy access_policy)
  expect_ppu_detail(ppu-forced-blank-policy-value
    "ppu_policy\\.forced_blank_policy\\.access_policy" "${p}")

  profile_copy(p missing_forced_blank_policy)
  gbl_json_mutate_remove("${p}" ppu_policy forced_blank_policy)
  expect_ppu_detail(ppu-forced-blank-policy-value
    "ppu_policy\\.forced_blank_policy" "${p}")

  profile_copy(p extra_forced_blank_policy_field)
  gbl_json_mutate_set("${p}" "true" ppu_policy forced_blank_policy unexpected)
  expect_ppu_detail(ppu-forced-blank-policy-value
    "ppu_policy\\.forced_blank_policy: expected 6 fields" "${p}")

  profile_copy(p wrong_object_admission_order)
  gbl_json_mutate_set("${p}" [=["select-later-fits"]=]
    ppu_policy object_policy admission_order)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy\\.admission_order" "${p}")

  profile_copy(p wrong_object_overflow_policy)
  gbl_json_mutate_set("${p}" [=["invalidate-frame"]=]
    ppu_policy object_policy overflow_policy)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy\\.overflow_policy" "${p}")

  profile_copy(p wrong_object_regular_cost)
  gbl_json_mutate_set("${p}" [=["pixel-count"]=]
    ppu_policy object_policy regular_cost)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy\\.regular_cost" "${p}")

  profile_copy(p wrong_object_affine_cost)
  gbl_json_mutate_set("${p}" [=["source-width"]=]
    ppu_policy object_policy affine_cost)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy\\.affine_cost" "${p}")

  profile_copy(p wrong_object_eligibility)
  gbl_json_mutate_set("${p}" [=["horizontal-intersection-required"]=]
    ppu_policy object_policy eligibility)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy\\.eligibility" "${p}")

  profile_copy(p missing_object_policy)
  gbl_json_mutate_remove("${p}" ppu_policy object_policy)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy" "${p}")

  profile_copy(p malformed_object_policy)
  gbl_json_mutate_set("${p}" [=["policy"]=] ppu_policy object_policy)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy" "${p}")

  profile_copy(p extra_object_policy_field)
  gbl_json_mutate_set("${p}" [=["unexpected"]=]
    ppu_policy object_policy unexpected)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy: expected 14 fields" "${p}")

  profile_copy(p wrong_object_boolean_type)
  gbl_json_mutate_set("${p}" [=["true"]=]
    ppu_policy object_policy index_zero_transparency)
  expect_ppu_detail(ppu-object-policy-value
    "ppu_policy\\.object_policy\\.index_zero_transparency: expected BOOLEAN" "${p}")

  profile_copy(p wrong_composition_tie)
  gbl_json_mutate_set("${p}" [=["bg-in-front-project-policy"]=]
    ppu_policy composition_policy obj_bg_tie)
  expect_ppu_detail(ppu-composition-policy-value
    "ppu_policy\\.composition_policy\\.obj_bg_tie" "${p}")

  profile_copy(p wrong_object_tie)
  gbl_json_mutate_set("${p}" [=["higher-oam-index"]=]
    ppu_policy composition_policy obj_tie)
  expect_ppu_detail(ppu-composition-policy-value
    "ppu_policy\\.composition_policy\\.obj_tie" "${p}")

  profile_copy(p missing_composition_policy)
  gbl_json_mutate_remove("${p}" ppu_policy composition_policy)
  expect_ppu_detail(ppu-composition-policy-value
    "ppu_policy\\.composition_policy" "${p}")

  profile_copy(p extra_composition_policy_field)
  gbl_json_mutate_set("${p}" [=["unexpected"]=]
    ppu_policy composition_policy unexpected)
  expect_ppu_detail(ppu-composition-policy-value
    "ppu_policy\\.composition_policy: expected 5 fields" "${p}")

  profile_copy(p complete_display_mode)
  gbl_json_mutate_set("${p}" [=["complete"]=]
    ppu_policy display_capabilities modes 0 implementation_status)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p removed_hardware_unknown)
  gbl_json_mutate_remove("${p}" ppu_policy hardware_qualification)
  expect_ppu_detail(ppu-policy-value
    "ppu_policy\\.hardware_qualification" "${p}")

  profile_copy(p stale_legacy_supported_controls)
  gbl_json_mutate_set("${p}" [=["exactly one enabled background;OBJ disabled"]=]
    ppu_policy supported_controls)
  expect_ppu_detail(ppu-policy-value
    "ppu_policy\\.supported_controls" "${p}")

  profile_copy(p stale_legacy_unmodeled_controls)
  gbl_json_mutate_set("${p}" [=["BG2 disabled in modes 3-5;BG priority/composition;OBJ disabled"]=]
    ppu_policy unmodeled_controls)
  expect_ppu_detail(ppu-policy-value
    "ppu_policy\\.unmodeled_controls" "${p}")

  profile_copy(p missing_display_capabilities)
  gbl_json_mutate_remove("${p}" ppu_policy display_capabilities)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities" "${p}")

  profile_copy(p malformed_display_capabilities)
  gbl_json_mutate_set("${p}" [=["capabilities"]=] ppu_policy display_capabilities)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities" "${p}")

  profile_copy(p missing_capability_model)
  gbl_json_mutate_remove("${p}" ppu_policy display_capabilities model)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities" "${p}")

  profile_copy(p extra_capability_field)
  gbl_json_mutate_set("${p}" "true" ppu_policy display_capabilities unexpected)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities" "${p}")

  profile_copy(p stale_capability_model)
  gbl_json_mutate_set("${p}" [=["gamebloke-display-capabilities-1"]=]
    ppu_policy display_capabilities model)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.model" "${p}")

  profile_copy(p false_window_feature)
  gbl_json_mutate_set("${p}" "false"
    ppu_policy display_capabilities composition_features rectangular_windows)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.composition_features\\.rectangular_windows" "${p}")

  profile_copy(p missing_forced_blank_feature)
  gbl_json_mutate_remove("${p}"
    ppu_policy display_capabilities composition_features forced_blank)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.composition_features: expected 9 fields" "${p}")

  profile_copy(p extra_composition_feature)
  gbl_json_mutate_set("${p}" "true"
    ppu_policy display_capabilities composition_features scanline_dma)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.composition_features: expected 9 fields" "${p}")

  profile_copy(p qualified_composition_feature)
  gbl_json_mutate_set("${p}" [=["QUALIFIED"]=]
    ppu_policy display_capabilities composition_features hardware_qualification)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.composition_features\\.hardware_qualification" "${p}")

  profile_copy(p wrong_raw_output_type)
  gbl_json_mutate_set("${p}" [=["invalid"]=] ppu_policy display_capabilities raw_output)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.raw_output" "${p}")

  profile_copy(p wrong_raw_format_type)
  gbl_json_mutate_set("${p}" "555" ppu_policy display_capabilities raw_output format)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.raw_output" "${p}")

  profile_copy(p wrong_raw_format)
  gbl_json_mutate_set("${p}" [=["RGB565_LE"]=] ppu_policy display_capabilities raw_output format)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.raw_output" "${p}")

  foreach(field IN ITEMS width height stride_bytes byte_length)
    profile_copy(p wrong_raw_${field})
    if(field STREQUAL "width")
      set(bad_value 239)
    elseif(field STREQUAL "height")
      set(bad_value 159)
    elseif(field STREQUAL "stride_bytes")
      set(bad_value 482)
    else()
      set(bad_value 76801)
    endif()
    gbl_json_mutate_set("${p}" "${bad_value}" ppu_policy display_capabilities raw_output ${field})
    expect_ppu_detail(ppu-capability-value
      "ppu_policy\\.display_capabilities\\.raw_output" "${p}")
  endforeach()

  profile_copy(p wrong_modes_type)
  gbl_json_mutate_set("${p}" [=[{}]=] ppu_policy display_capabilities modes)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p missing_mode_record)
  gbl_json_mutate_remove("${p}" ppu_policy display_capabilities modes 5)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p duplicate_mode_record)
  gbl_json_mutate_set("${p}" "0" ppu_policy display_capabilities modes 1 mode)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  foreach(invalid_mode IN ITEMS 6 7)
    profile_copy(p invalid_mode_${invalid_mode})
    gbl_json_mutate_set("${p}" "${invalid_mode}" ppu_policy display_capabilities modes 5 mode)
    expect_ppu_detail(ppu-capability-value
      "ppu_policy\\.display_capabilities\\.modes" "${p}")
  endforeach()

  profile_copy(p wrong_background_source)
  gbl_json_mutate_set("${p}" [=["affine"]=] ppu_policy display_capabilities modes 0 backgrounds 0 source)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_mode1_background_source)
  gbl_json_mutate_set("${p}" [=["affine"]=] ppu_policy display_capabilities modes 1 backgrounds 0 source)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_mode1_affine_source)
  gbl_json_mutate_set("${p}" [=["text"]=] ppu_policy display_capabilities modes 1 backgrounds 2 source)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_mode2_affine_source)
  gbl_json_mutate_set("${p}" [=["text"]=] ppu_policy display_capabilities modes 2 backgrounds 0 source)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  foreach(mode IN ITEMS 0 1 2 3 4 5)
    profile_copy(p stale_supported_mode_${mode}_status)
    gbl_json_mutate_set("${p}" [=["not-implemented"]=]
      ppu_policy display_capabilities modes ${mode} implementation_status)
    expect_ppu_detail(ppu-capability-value
      "ppu_policy\\.display_capabilities\\.modes" "${p}")
  endforeach()

  profile_copy(p wrong_mode3_bitmap_source)
  gbl_json_mutate_set("${p}" [=["indexed-bitmap"]=]
    ppu_policy display_capabilities modes 3 backgrounds 0 source)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_mode4_bitmap_source)
  gbl_json_mutate_set("${p}" [=["direct-color-bitmap"]=]
    ppu_policy display_capabilities modes 4 backgrounds 0 source)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_mode5_bitmap_source)
  gbl_json_mutate_set("${p}" [=["indexed-bitmap"]=]
    ppu_policy display_capabilities modes 5 backgrounds 0 source)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  foreach(mode IN ITEMS 3 4 5)
    profile_copy(p wrong_mode${mode}_bitmap_dimension)
    if(mode EQUAL 5)
      set(bad_dimension 159)
    else()
      set(bad_dimension 239)
    endif()
    gbl_json_mutate_set("${p}" "${bad_dimension}"
      ppu_policy display_capabilities modes ${mode} backgrounds 0 dimensions 0 width)
    expect_ppu_detail(ppu-capability-value
      "ppu_policy\\.display_capabilities\\.modes" "${p}")
  endforeach()

  profile_copy(p wrong_background_id)
  gbl_json_mutate_set("${p}" "1" ppu_policy display_capabilities modes 0 backgrounds 0 bg)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_source_dimension)
  gbl_json_mutate_set("${p}" "255" ppu_policy display_capabilities modes 0 backgrounds 0 dimensions 0 width)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_mode1_source_dimension)
  gbl_json_mutate_set("${p}" "255" ppu_policy display_capabilities modes 1 backgrounds 0 dimensions 0 width)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_mode1_affine_dimension)
  gbl_json_mutate_set("${p}" "255" ppu_policy display_capabilities modes 1 backgrounds 2 dimensions 0 width)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_mode2_affine_dimension)
  gbl_json_mutate_set("${p}" "255" ppu_policy display_capabilities modes 2 backgrounds 0 dimensions 0 width)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p wrong_dimensions_type)
  gbl_json_mutate_set("${p}" [=["256x256"]=] ppu_policy display_capabilities modes 0 backgrounds 0 dimensions)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p arbitrary_implementation_status)
  gbl_json_mutate_set("${p}" [=["future"]=] ppu_policy display_capabilities modes 0 implementation_status)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p partial_replaced_by_implemented)
  gbl_json_mutate_set("${p}" [=["implemented"]=] ppu_policy display_capabilities modes 3 implementation_status)
  expect_ppu_detail(ppu-capability-value
    "ppu_policy\\.display_capabilities\\.modes" "${p}")

  profile_copy(p added_ppu_stall)
  gbl_json_mutate_set("${p}" "1" ppu_policy video_memory_access ppu_extra_cycles)
  expect_ppu_detail(ppu-access-policy-value
    "ppu_policy\\.video_memory_access" "${p}")

  profile_copy(p added_ppu_denial)
  gbl_json_mutate_set("${p}" [=["denied-during-drawing"]=] ppu_policy video_memory_access availability)
  expect_ppu_detail(ppu-access-policy-value
    "ppu_policy\\.video_memory_access" "${p}")

  profile_copy(p escalated_hardware_qualification)
  gbl_json_mutate_set("${p}" [=["QUALIFIED"]=] ppu_policy video_memory_access hardware_qualification)
  expect_ppu_detail(ppu-access-policy-value
    "ppu_policy\\.video_memory_access" "${p}")

  profile_copy(p wrong_access_array_type)
  gbl_json_mutate_set("${p}" [=["cpu,dma"]=] ppu_policy video_memory_access masters)
  expect_ppu_detail(ppu-access-policy-value
    "ppu_policy\\.video_memory_access" "${p}")

  profile_copy(p extra_access_policy_field)
  gbl_json_mutate_set("${p}" "true" ppu_policy video_memory_access extra_policy)
  expect_ppu_detail(ppu-access-policy-value
    "ppu_policy\\.video_memory_access" "${p}")

  profile_copy(p stale_display_capability_doc)
  doc_copy(d stale_display_capability_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.display_capabilities.modes[3]` | partial | BG2 direct-color-bitmap: 240x160 |"
    "| `ppu_policy.display_capabilities.modes[3]` | not-implemented | BG2 direct-color-bitmap: 240x160 |")
  expect_ppu_detail(ppu-capability-doc
    "ppu_policy\\.display_capabilities: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_mode4_display_capability_doc)
  doc_copy(d stale_mode4_display_capability_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.display_capabilities.modes[4]` | partial | BG2 indexed-bitmap: 240x160 |"
    "| `ppu_policy.display_capabilities.modes[4]` | not-implemented | BG2 indexed-bitmap: 240x160 |")
  expect_ppu_detail(ppu-capability-doc
    "ppu_policy\\.display_capabilities: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_mode5_display_capability_doc)
  doc_copy(d stale_mode5_display_capability_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.display_capabilities.modes[5]` | partial | BG2 direct-color-bitmap: 160x128 |"
    "| `ppu_policy.display_capabilities.modes[5]` | not-implemented | BG2 direct-color-bitmap: 160x128 |")
  expect_ppu_detail(ppu-capability-doc
    "ppu_policy\\.display_capabilities: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_affine_display_capability_doc)
  doc_copy(d stale_affine_display_capability_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.display_capabilities.modes[2]` | partial | BG2 affine: 128x128, 256x256, 512x512, 1024x1024; BG3 affine: 128x128, 256x256, 512x512, 1024x1024 |"
    "| `ppu_policy.display_capabilities.modes[2]` | not-implemented | BG2 affine: 128x128, 256x256, 512x512, 1024x1024; BG3 affine: 128x128, 256x256, 512x512, 1024x1024 |")
  expect_ppu_detail(ppu-capability-doc
    "ppu_policy\\.display_capabilities: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_text_control_doc)
  doc_copy(d stale_text_control_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.supported_controls` | mode 3/4/5 BG2 bitmap sources (mode 3 direct-color 240x160 single page, mode 4 indexed 240x160 two DISPCNT-selected pages, mode 5 direct-color 160x128 two DISPCNT-selected pages), captured BG2 affine transform/reference progression, out-of-extent samples to backdrop, mode-4 index zero transparent and direct-color zero opaque black, non-BG2 background enable bits ignored in bitmap modes, any enabled supported backgrounds per sampled row in modes 0-2 (mode 0 text BG0-BG3, mode 1 text BG0/BG1 or affine BG2, mode 2 affine BG2/BG3), text scroll, four map dimensions, affine transforms and reference progression, 4bpp/8bpp tiles, flips, palette and transparent-index backdrop, regular and affine OBJ rendering, shared BG/OBJ/backdrop priority composition, rectangular WIN0/WIN1 half-open regions, admitted OBJ-window masks and region-specific layer/effect masks, top-and-immediate-next-visible alpha and semitransparent OBJ effects, clamped 5-bit brightness, independent BG/OBJ screen-origin mosaic resampled from the current row, and forced-blank white rows with normal frame coverage |"
    "| `ppu_policy.supported_controls` | bitmap mode-3 only |")
  expect_ppu_detail(ppu-policy-doc
    "ppu_policy: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_unmodeled_controls_doc)
  doc_copy(d stale_unmodeled_controls_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.unmodeled_controls` | prohibited OBJ mode/shape encodings and unsupported background-control encodings |"
    "| `ppu_policy.unmodeled_controls` | bitmap modes 3/4/5 and text scroll |")
  expect_ppu_detail(ppu-policy-doc
    "ppu_policy: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_object_policy_doc)
  doc_copy(d stale_object_policy_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.object_policy.work_budget_units` | 1226 |"
    "| `ppu_policy.object_policy.work_budget_units` | 1225 |")
  expect_ppu_detail(ppu-object-policy-doc
    "ppu_policy\\.object_policy: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_composition_policy_doc)
  doc_copy(d stale_composition_policy_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.composition_policy.obj_bg_tie` | obj-in-front-project-policy |"
    "| `ppu_policy.composition_policy.obj_bg_tie` | bg-in-front-project-policy |")
  expect_ppu_detail(ppu-composition-policy-doc
    "ppu_policy\\.composition_policy: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_raw_output_doc)
  doc_copy(d stale_raw_output_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.display_capabilities.raw_output.width` | 240 |"
    "| `ppu_policy.display_capabilities.raw_output.width` | 239 |")
  expect_ppu_detail(ppu-capability-doc
    "ppu_policy\\.display_capabilities\\.raw_output: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_access_policy_doc)
  doc_copy(d stale_access_policy_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.video_memory_access.matrix` | project-selected | UNKNOWN | cpu, dma | pram, vram, oam | drawing, hblank, vblank, forced-blank | existing-bus-behavior | 0 | `bus_policy.request_cycles` |"
    "| `ppu_policy.video_memory_access.matrix` | project-selected | QUALIFIED | cpu, dma | pram, vram, oam | drawing, hblank, vblank, forced-blank | existing-bus-behavior | 0 | `bus_policy.request_cycles` |")
  expect_ppu_detail(ppu-access-policy-doc
    "ppu_policy\\.video_memory_access: missing or stale row" "${p}" "${d}")

  profile_copy(p missing_ppu_policy)
  gbl_json_mutate_set("${p}" [=[{"placeholder": "temporary control value"}]=] ppu_policy)
  gbl_json_mutate_remove("${p}" ppu_policy)
  expect_ppu_detail(ppu-policy-missing "ppu_policy" "${p}")

  set(ppu_policy_fields
    policy_classification hardware_qualification hardware_source_reference
    geometry line_cycles nominal_visible_dot_cycles recurrence event_priority
    line_transitions
    same_cycle_order dispcnt_writable_mask dispstat_writable_mask
    vcount_write_policy compare_write_policy hblank_event_offset_cycles
    hblank_source hblank_source_reference hblank_provenance
    hblank_dependent_classification hblank_dispute
    hblank_hardware_qualification row_sample_point
    row_same_cycle_write_policy pixel_format frame_validity
    supported_controls unmodeled_controls interrupt_if_bits
    blank_dma_eligibility)
  foreach(field IN LISTS ppu_policy_fields)
    profile_copy(p mutated_${field})
    gbl_json_mutate_set("${p}" [=["control-drift"]=] ppu_policy ${field})
    string(REPLACE "." "\\." expected_path "ppu_policy.${field}")
    expect_ppu_detail(ppu-policy-value "${expected_path}" "${p}")
  endforeach()

  profile_copy(p false_hblank_qualification)
  gbl_json_mutate_set("${p}" [=["QUALIFIED"]=]
    ppu_policy hblank_hardware_qualification)
  expect_ppu_detail(ppu-policy-value
    "ppu_policy\\.hblank_hardware_qualification" "${p}")

  profile_copy(p missing_hblank_dispute)
  gbl_json_mutate_remove("${p}" ppu_policy hblank_dispute)
  expect_ppu_detail(ppu-policy-value "ppu_policy\\.hblank_dispute" "${p}")

  profile_copy(p wrong_hblank_offset)
  gbl_json_mutate_set("${p}" "1007" ppu_policy hblank_event_offset_cycles)
  expect_ppu_detail(ppu-policy-value
    "ppu_policy\\.hblank_event_offset_cycles" "${p}")

  profile_copy(p wrong_row_sample)
  gbl_json_mutate_set("${p}" [=["after-line-end"]=]
    ppu_policy row_sample_point)
  expect_ppu_detail(ppu-policy-value "ppu_policy\\.row_sample_point" "${p}")

  profile_copy(p stale_ppu_doc)
  doc_copy(d stale_ppu_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.row_sample_point` | visible-line-start |"
    "| `ppu_policy.row_sample_point` | after-line-end |")
  expect_ppu_detail(ppu-policy-doc "ppu_policy: missing or stale row" "${p}" "${d}")

  profile_copy(p stale_affine_source_reference_doc)
  doc_copy(d stale_affine_source_reference_doc)
  gbl_text_mutate("${d}"
    "| `ppu_policy.hardware_source_reference` | Nintendo AGB Programming Manual v1.1 §§4.1.1–4.1.2, 5.1.1, 6.1.3–6.1.5, 6.1.7, 6.3.1–6.3.4 and 6.4, printed pp. 58–72 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |"
    "| `ppu_policy.hardware_source_reference` | Nintendo AGB Programming Manual v1.1 §§4.1.1–4.1.2, 5.1.1 and 6.1.3–6.1.5 (https://www.manuallib.com/download/2023-10-17/AGB%20Programming%20Manual.pdf) |")
  expect_ppu_detail(ppu-policy-doc "ppu_policy: missing or stale row" "${p}" "${d}")

elseif(CASE STREQUAL "cpu_policy_value")
  if(NOT PROFILE_VERSION GREATER_EQUAL 6)
    message(FATAL_ERROR "profile-case: expanded CPU contract requires profile version 6 or later")
  endif()
  profile_copy(p positive_cpu_policy)
  doc_copy(d positive_cpu_policy)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, ${UNMODELED_COUNT} unmodeled\\)")

  profile_copy(p missing_cpu_policy)
  gbl_json_mutate_remove("${p}" cpu_policy)
  expect_code(cpu-policy-missing "${p}")

  profile_copy(p stale_version_four)
  gbl_json_mutate_set("${p}" "4" profile_version)
  expect_detail(cpu-policy-value "profile_version: expected 5" "${p}")

  foreach(key IN ITEMS pc_operand_reads internal_work multiply_flag_policy
      loaded_pc_boundary unpredictable_operands extension_encoding address_arithmetic)
    foreach(bad IN ITEMS missing number value)
      profile_copy(p bad_${key}_${bad})
      if(bad STREQUAL "missing")
        gbl_json_mutate_remove("${p}" cpu_policy ${key})
      elseif(bad STREQUAL "number")
        gbl_json_mutate_set("${p}" "5" cpu_policy ${key})
      else()
        gbl_json_mutate_set("${p}" "\"hardware-qualified\"" cpu_policy ${key})
      endif()
      expect_detail(cpu-policy-value "cpu_policy\\.${key}: expected" "${p}")
    endforeach()
  endforeach()
  profile_copy(p extra_policy_field)
  gbl_json_mutate_set("${p}" "true" cpu_policy hardware_qualified)
  expect_detail(cpu-policy-value "cpu_policy: field count" "${p}")

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
  string(JSON supported_count LENGTH "${base_profile}" cpu_policy supported_opcode_operands)
  gbl_json_mutate_set("${p}" "\"Extra instruction family not implemented\""
    cpu_policy supported_opcode_operands ${supported_count})
  expect_code(cpu-policy-value "${p}")

  profile_copy(p omitted_excluded_rows)
  gbl_json_mutate_set("${p}" "[]" cpu_policy excluded_opcode_operands)
  expect_code(cpu-policy-value "${p}")

  profile_copy(p false_bios_claim)
  gbl_json_mutate_set("${p}" "\"Hardware-qualified BIOS and active DMA execution\""
    cpu_policy supported_opcode_operands 0)
  expect_code(cpu-policy-value "${p}")

  profile_copy(p wrong_thumb_format_mapping)
  gbl_json_mutate_set("${p}" "\"Thumb formats 6-14: PC-relative, register/immediate byte/word/halfword and signed loads, SP-relative transfers and PC/SP address formation\""
    cpu_policy supported_opcode_operands 10)
  expect_code(cpu-policy-value "${p}")

elseif(CASE STREQUAL "cpu_policy_doc")
  profile_copy(p positive_cpu_policy_doc)
  doc_copy(d positive_cpu_policy_doc)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, ${UNMODELED_COUNT} unmodeled\\)")

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

  profile_copy(p stale_thumb_format_mapping)
  doc_copy(d stale_thumb_format_mapping)
  gbl_text_mutate("${d}" "| cpu_scope.supported.11 | Thumb formats 6-12:"
    "| cpu_scope.supported.11 | Thumb formats 6-14:")
  expect_code(cpu-policy-doc "${p}" "${d}")

  foreach(key IN ITEMS pc_operand_reads internal_work multiply_flag_policy
      loaded_pc_boundary unpredictable_operands extension_encoding address_arithmetic)
    profile_copy(p stale_${key}_doc)
    doc_copy(d stale_${key}_doc)
    string(JSON value GET "${base_profile}" cpu_policy ${key})
    gbl_text_mutate("${d}" "| cpu_policy.${key} | ${value} |"
      "| cpu_policy.${key} | hardware-qualified |")
    expect_code(cpu-policy-doc "${p}" "${d}")
  endforeach()

  profile_copy(p stop_reason_drift)
  doc_copy(d stop_reason_drift)
  gbl_text_mutate("${d}" "| cpu_stop.unpredictable | unpredictable-instruction |"
    "| cpu_stop.unpredictable | unsupported-instruction |")
  expect_code(cpu-policy-doc "${p}" "${d}")

elseif(CASE STREQUAL "timing_model")
  profile_copy(p timing_model_clean)
  doc_copy(d timing_model_clean)
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, ${UNMODELED_COUNT} unmodeled\\)")

  profile_copy(p timing_model_missing_width_class)
  file(READ "${p}" json)
  string(JSON row_count LENGTH "${json}" timing_model bus_access_costs)
  set(found_row -1)
  math(EXPR last_row "${row_count}-1")
  foreach(i RANGE ${last_row})
    string(JSON key GET "${json}" timing_model bus_access_costs ${i} key)
    if(key STREQUAL "read/bios/4/s/data")
      set(found_row "${i}")
    endif()
  endforeach()
  if(found_row LESS 0)
    message(FATAL_ERROR "profile-case: missing width/class control anchor")
  endif()
  gbl_json_mutate_remove("${p}" timing_model bus_access_costs ${found_row})
  expect_detail(timing-model-missing-key "read/bios/4/s/data" "${p}")

  profile_copy(p timing_model_duplicate_key)
  gbl_json_mutate_set("${p}" "\"read/bios/1/n/fetch\""
    timing_model bus_access_costs 1 key)
  gbl_json_mutate_set("${p}" "\"fetch\""
    timing_model bus_access_costs 1 role)
  expect_detail(timing-model-duplicate-key "read/bios/1/n/fetch" "${p}")

  profile_copy(p timing_model_stale_narrative)
  doc_copy(d timing_model_stale_narrative)
  gbl_text_mutate("${d}"
    "| `timing_model.read/bios/1/n/fetch` | project-selected | 1 |"
    "| `timing_model.read/bios/1/n/fetch` | project-selected | 2 |")
  expect_detail(timing-model-doc "timing_model: missing or stale row" "${p}" "${d}")

  profile_copy(p timing_model_missing_waitcnt)
  file(READ "${p}" json)
  string(JSON gap_count LENGTH "${json}" timing_model unmodeled_hardware)
  set(waitcnt_row -1)
  math(EXPR last_gap "${gap_count}-1")
  foreach(i RANGE ${last_gap})
    string(JSON id GET "${json}" timing_model unmodeled_hardware ${i} id)
    if(id STREQUAL "waitcnt")
      set(waitcnt_row "${i}")
    endif()
  endforeach()
  if(waitcnt_row LESS 0)
    message(FATAL_ERROR "profile-case: WAITCNT control anchor missing")
  endif()
  gbl_json_mutate_remove("${p}" timing_model unmodeled_hardware ${waitcnt_row})
  expect_detail(timing-model-gap "missing required unmodeled hardware gap waitcnt" "${p}")

  profile_copy(p timing_model_missing_prefetch)
  file(READ "${p}" json)
  string(JSON gap_count LENGTH "${json}" timing_model unmodeled_hardware)
  set(prefetch_row -1)
  math(EXPR last_gap "${gap_count}-1")
  foreach(i RANGE ${last_gap})
    string(JSON id GET "${json}" timing_model unmodeled_hardware ${i} id)
    if(id STREQUAL "prefetch")
      set(prefetch_row "${i}")
    endif()
  endforeach()
  if(prefetch_row LESS 0)
    message(FATAL_ERROR "profile-case: prefetch control anchor missing")
  endif()
  gbl_json_mutate_remove("${p}" timing_model unmodeled_hardware ${prefetch_row})
  expect_detail(timing-model-gap "missing required unmodeled hardware gap prefetch" "${p}")

  profile_copy(p timing_model_invented_source)
  gbl_json_mutate_set("${p}" "\"hardware-qualified\""
    timing_model qualification)
  expect_detail(timing-model-qualification "timing_model\\.qualification must be project-model-only" "${p}")

  profile_copy(p timing_model_rom_fixed_cost)
  file(READ "${p}" json)
  string(JSON row_count LENGTH "${json}" timing_model bus_access_costs)
  set(rom_row -1)
  math(EXPR last_row "${row_count}-1")
  foreach(i RANGE ${last_row})
    string(JSON row_key GET "${json}" timing_model bus_access_costs ${i} key)
    if(row_key STREQUAL "read/rom/1/n/fetch")
      set(rom_row "${i}")
    endif()
  endforeach()
  if(rom_row LESS 0)
    message(FATAL_ERROR "profile-case: ROM formula control anchor missing")
  endif()
  gbl_json_mutate_set("${p}" "1" timing_model bus_access_costs ${rom_row} execution cost_cycles)
  expect_detail(timing-model-execution "read/rom/1/n/fetch: expected contextual WAITCNT source-backed beat formula" "${p}")

  profile_copy(p waitcnt_prefetch_modeled)
  gbl_json_mutate_set("${p}" "\"modeled\"" waitcnt_policy prefetch execution)
  expect_detail(waitcnt-policy "prefetch execution must remain unmodeled and UNKNOWN" "${p}")

  profile_copy(p dma_latch_closed)
  gbl_json_mutate_set("${p}" "\"resolved\"" dma_policy latch_policy dispute_status)
  expect_detail(dma-policy "DMA latch dispute must remain open, disputed, and UNKNOWN" "${p}")

elseif(CASE STREQUAL "bus_policy_missing")
  profile_copy(p missing_bus_policy)
  gbl_json_mutate_remove("${p}" bus_policy)
  expect_code(bus-policy-missing "${p}")

elseif(CASE STREQUAL "bus_policy_value")
  profile_copy(p request_cost)
  gbl_json_mutate_set("${p}" "2" bus_policy request_cycles rom)
  expect_detail(bus-policy-value "bus_policy\\.request_cycles\\.rom: expected contextual WAITCNT beat formula" "${p}")
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
      "| bus_policy.internal_cycle_unit | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |"
      "| bus_policy.internal_cycle_unit | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |\n${row}")
    expect_detail(bus-policy-doc "bus_policy: missing policy table" "${p}" "${d}")
  endforeach()
  profile_copy(p stale_value)
  doc_copy(d stale_value)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |"
    "| bus_policy.request_cycles.rom | 2 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |")
  expect_detail(bus-policy-doc "bus_policy: missing or stale row" "${p}" "${d}")
  # A complete but hidden copy must not stand in for the stale rendered table.
  profile_copy(p commented_stale_value)
  doc_copy(d commented_stale_value)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |"
    "| bus_policy.request_cycles.rom | 2 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |")
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
    "| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |"
    "| bus_policy.request_cycles.rom | 2 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |")
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
    "| bus_policy.request_cycles.unmapped | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |"
    "| bus_policy.request_cycles.unmapped | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |\n| bus_policy.request_cycles.extra | 1 | IMPL | test-only row |")
  expect_detail(bus-policy-doc "bus_policy: expected 12 rows, found 13" "${p}" "${d}")
  profile_copy(p unexpected_policy_row)
  doc_copy(d unexpected_policy_row)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.unmapped | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |"
    "| bus_policy.request_cycles.extra | 1 | IMPL | test-only row |")
  expect_detail(bus-policy-doc
    "bus_policy: unexpected policy row bus_policy.request_cycles.extra" "${p}" "${d}")
  profile_copy(p wrong_policy_body_cells)
  doc_copy(d wrong_policy_body_cells)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |"
    "| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. | extra |")
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
    "| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |"
    "| bus_policy.request_cycles.rom | 2 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |")
  gbl_text_mutate("${d}" "## Machine\n"
    "## Machine\n| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |\n")
  expect_detail(bus-policy-doc "bus_policy: missing or stale row" "${p}" "${d}")
  profile_copy(p same_section_fence)
  doc_copy(d same_section_fence)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |\n"
    "")
  gbl_text_mutate("${d}" "## Initialization policy"
    "```text\n| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |\n```\n\n## Initialization policy")
  expect_detail(bus-policy-doc "bus_policy: expected 12 rows, found 11" "${p}" "${d}")
  profile_copy(p same_section_indented)
  doc_copy(d same_section_indented)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |\n"
    "")
  gbl_text_mutate("${d}" "## Initialization policy"
    "    | bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |\n\t| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |\n\n## Initialization policy")
  expect_detail(bus-policy-doc "bus_policy: expected 12 rows, found 11" "${p}" "${d}")
  profile_copy(p duplicate_key)
  doc_copy(d duplicate_key)
  gbl_text_mutate("${d}"
    "| bus_policy.request_cycles.oam | 1 | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |"
    "| bus_policy.request_cycles.rom | contextual-waitcnt-halfword-beats | IMPL | Gamebloke policy: one master cycle per non-ROM request; ROM costs use contextual WAITCNT halfword-beat quotes; prefetch execution is unmodeled. |")
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
    REGEX "profile: ok \\(agb v${PROFILE_VERSION}, 11 timing constants, ${UNMODELED_COUNT} unmodeled\\)")

elseif(CASE STREQUAL "unmodeled_missing")
  # Remove each required entry in turn from a fresh copy; each
  # removal names its own id.
  file(READ "${SRC}/profiles/agb.json" real)
  string(JSON n LENGTH "${real}" unmodeled)
  if(NOT n EQUAL UNMODELED_COUNT)
    message(FATAL_ERROR "profile-case: expected ${UNMODELED_COUNT} unmodeled entries, found ${n}")
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
  # prefetch is entry 1, dma entry 7, sp-micro-differences entry 0.
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
  # hblank-flag-edge (entry 4) must carry disp-hblank-flag-edge, not another dispute id
  # (disp-dma-latch is a registry id, so no dispute-unknown finding can stand in for it).
  profile_copy(p hblank_dispute)
  gbl_json_mutate_set("${p}" "\"disp-dma-latch\"" unmodeled 4 dispute)
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
  set(io_devices_row [=[| `io-devices` | I/O register semantics remain incomplete and hardware-unqualified. Timer, IF/IE/IME, KEYINPUT/KEYCNT, and the VCOUNT/DISPSTAT/DISPCNT subset is described by the timer, `irq_input_power_policy`, and `ppu_policy` entries; unsupported reads still use the project-policy placeholder. |]=])
  profile_copy(p duplicate_row)
  doc_copy(d duplicate_row)
  gbl_text_mutate("${d}" "${io_devices_row}"
    "${io_devices_row}\n${io_devices_row}")
  expect_detail(doc-extra-id "duplicate id io-devices" "${p}" "${d}")
  profile_copy(p duplicate_replaces_row)
  doc_copy(d duplicate_replaces_row)
  gbl_text_mutate("${d}" "${io_devices_row}"
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
    gbl_text_mutate("${d}" "${io_devices_row}"
      "${io_devices_row}\n${row}")
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
    unmodeled ${UNMODELED_COUNT})
  gbl_json_mutate_set("${p}" "${NEXT_PROFILE_VERSION}" profile_version)
  gbl_text_mutate("${d}" "| `backup-media` |"
    "| `serial-link` | The serial link port. |\n| `backup-media` |")
  gbl_text_mutate("${d}" "\nprofile_version: ${PROFILE_VERSION}\n" "\nprofile_version: ${NEXT_PROFILE_VERSION}\n")
  check_command(cmd "${p}" "${d}")
  gbl_expect_pass(COMMAND ${cmd}
    REGEX "profile: ok \\(agb v${NEXT_PROFILE_VERSION}, 11 timing constants, ${NEXT_PROFILE_UNMODELED_COUNT} unmodeled\\)")

elseif(CASE STREQUAL "irq_policy")
  profile_copy(p bad_keycnt_mask)
  gbl_json_mutate_set("${p}" "0" irq_input_power_policy keycnt_mask)
  expect_detail(irq-policy-value "irq_input_power_policy\\.keycnt_mask: expected 50175" "${p}")

  profile_copy(p bad_policy_source)
  gbl_json_mutate_set("${p}" [=["REF"]=] irq_input_power_policy source)
  expect_detail(irq-policy-source "irq_input_power_policy: source/citation" "${p}")

  profile_copy(p wrong_irq_doc)
  doc_copy(d wrong_irq_doc)
  gbl_text_mutate("${d}"
    "| irq_input_power_policy.keypad_event_priority | 4 | IMPL |"
    "| irq_input_power_policy.keypad_event_priority | 5 | IMPL |")
  expect_detail(irq-policy-doc "irq_input_power_policy: missing or stale row" "${p}" "${d}")

  profile_copy(p coordinated_irq_policy_drift)
  doc_copy(d coordinated_irq_policy_drift)
  gbl_json_mutate_set("${p}" [=["before-first-instruction"]=]
    irq_input_power_policy sample_boundary)
  gbl_text_mutate("${d}"
    "| irq_input_power_policy.sample_boundary | completed-instruction-effects-and-events-before-next-instruction |"
    "| irq_input_power_policy.sample_boundary | before-first-instruction |")
  expect_detail(irq-policy-value
    "irq_input_power_policy.sample_boundary: expected completed-instruction-effects-and-events-before-next-instruction"
    "${p}" "${d}")

else()
  message(FATAL_ERROR "profile-case: unknown case: ${CASE}")
endif()
