cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    GBL_STAGE GBL_REPO_ROOT GBL_MANIFEST GBL_SOURCE_PATH GBL_ROM_PATH
    GBL_SCRATCH GBL_EVIDENCE_ROOT GBL_COMPILER_IDENTITY GBL_COMPILER_TARGET
    GBL_CMAKE_IDENTITY GBL_NINJA_IDENTITY)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "phase21-manifest: missing required input ${_required}")
  endif()
endforeach()

function(gbl_json_get output error_output json_text)
  string(JSON _value ERROR_VARIABLE _json_error GET "${json_text}" ${ARGN})
  if(_json_error STREQUAL "NOTFOUND")
    set(${output} "${_value}" PARENT_SCOPE)
    set(${error_output} "" PARENT_SCOPE)
  else()
    set(${output} "" PARENT_SCOPE)
    set(${error_output} "missing or invalid manifest field ${ARGN}: ${_json_error}"
        PARENT_SCOPE)
  endif()
endfunction()

function(gbl_json_at output error_output json_text dotted_path)
  string(REPLACE "." ";" _path "${dotted_path}")
  gbl_json_get(_value _error "${json_text}" ${_path})
  set(${output} "${_value}" PARENT_SCOPE)
  set(${error_output} "${_error}" PARENT_SCOPE)
endfunction()

function(gbl_compare_json expected_json expected_path observed_json observed_path
         label output_error)
  gbl_json_at(_expected _expected_error "${expected_json}" "${expected_path}")
  if(NOT _expected_error STREQUAL "")
    set(${output_error} "${_expected_error}" PARENT_SCOPE)
    return()
  endif()
  gbl_json_at(_observed _observed_error "${observed_json}" "${observed_path}")
  if(NOT _observed_error STREQUAL "")
    set(${output_error} "${_observed_error}" PARENT_SCOPE)
    return()
  endif()
  if(NOT "${_expected}" STREQUAL "${_observed}")
    set(${output_error}
        "${label}: expected '${_expected}', observed '${_observed}'"
        PARENT_SCOPE)
    return()
  endif()
  set(${output_error} "" PARENT_SCOPE)
endfunction()

macro(gbl_v_fail reason)
  set(${_gbl_ok_var} FALSE PARENT_SCOPE)
  set(${_gbl_error_var} "${reason}" PARENT_SCOPE)
  return()
endmacro()

macro(gbl_v_get variable)
  gbl_json_get(${variable} _gbl_parse_error "${_gbl_json}" ${ARGN})
  if(NOT _gbl_parse_error STREQUAL "")
    gbl_v_fail("${_gbl_parse_error}")
  endif()
endmacro()

macro(gbl_v_equal variable expected label)
  if(NOT "${${variable}}" STREQUAL "${expected}")
    gbl_v_fail("${label}: expected '${expected}', got '${${variable}}'")
  endif()
endmacro()

function(gbl_validate_manifest manifest_path output_ok output_error)
  set(_gbl_ok_var "${output_ok}")
  set(_gbl_error_var "${output_error}")
  if(NOT EXISTS "${manifest_path}" OR IS_SYMLINK "${manifest_path}")
    set(${output_ok} FALSE PARENT_SCOPE)
    set(${output_error} "manifest is missing or symlinked" PARENT_SCOPE)
    return()
  endif()
  file(READ "${manifest_path}" _gbl_json)

  gbl_v_get(_schema schema)
  gbl_v_equal(_schema "gamebloke.manifest/1" "manifest schema")
  string(JSON _fixture_count ERROR_VARIABLE _gbl_count_error
         LENGTH "${_gbl_json}" fixtures)
  if(NOT _gbl_count_error STREQUAL "NOTFOUND" OR NOT _fixture_count EQUAL 1)
    gbl_v_fail("manifest must contain exactly one fixture")
  endif()
  gbl_v_get(_fixture_id fixtures 0 id)
  gbl_v_equal(_fixture_id "phase21-original-diagnostic" "fixture id")

  gbl_v_get(_source_kind fixtures 0 source kind)
  gbl_v_equal(_source_kind "git" "source kind")
  gbl_v_get(_source_url fixtures 0 source url)
  gbl_v_equal(_source_url "https://github.com/szTheory/gamebloke" "source URL")
  gbl_v_get(_source_path fixtures 0 source path)
  gbl_v_equal(_source_path "tests/fixtures/phase21_diagnostic.c" "source path")
  gbl_v_get(_source_commit fixtures 0 source commit)
  gbl_v_get(_source_sha fixtures 0 source sha256)

  gbl_v_get(_license_status fixtures 0 license status)
  gbl_v_equal(_license_status "no-license-asserted" "license status")
  string(JSON _spdx_type ERROR_VARIABLE _gbl_spdx_error
         TYPE "${_gbl_json}" fixtures 0 license spdx)
  if(NOT _gbl_spdx_error STREQUAL "NOTFOUND" OR NOT _spdx_type STREQUAL "NULL")
    gbl_v_fail("license SPDX must remain unasserted")
  endif()
  gbl_v_get(_redistributable fixtures 0 license redistributable)
  gbl_v_equal(_redistributable "unknown" "redistribution status")
  string(JSON _notice_count ERROR_VARIABLE _gbl_notice_count_error
         LENGTH "${_gbl_json}" fixtures 0 license notices)
  if(NOT _gbl_notice_count_error STREQUAL "NOTFOUND" OR
     NOT _notice_count EQUAL 1)
    gbl_v_fail("manifest must name exactly one rights notice")
  endif()
  gbl_v_get(_notice_relative fixtures 0 license notices 0)
  gbl_v_equal(_notice_relative
      ".planning/phases/21-installed-consumer-proof-and-libfuturo-fit/21-DIAGNOSTIC-NOTICE.md"
      "rights notice path")
  gbl_v_get(_authority fixtures 0 authorization authority)
  gbl_v_equal(_authority "owner-approved Phase 21 context" "authorization authority")
  gbl_v_get(_scope fixtures 0 authorization scope)
  gbl_v_equal(_scope
      "one original BIOS-free diagnostic for local Phase 21 verification only"
      "authorization scope")
  string(JSON _prohibited_count ERROR_VARIABLE _gbl_prohibited_error
         LENGTH "${_gbl_json}" fixtures 0 authorization prohibited_use)
  if(NOT _gbl_prohibited_error STREQUAL "NOTFOUND" OR
     NOT _prohibited_count EQUAL 3)
    gbl_v_fail("authorization must prohibit public CI, upload, and redistribution")
  endif()
  foreach(_index RANGE 0 2)
    gbl_v_get(_prohibited fixtures 0 authorization prohibited_use ${_index})
    if(_index EQUAL 0)
      gbl_v_equal(_prohibited "public-ci" "prohibited use")
    elseif(_index EQUAL 1)
      gbl_v_equal(_prohibited "upload" "prohibited use")
    else()
      gbl_v_equal(_prohibited "redistribution" "prohibited use")
    endif()
  endforeach()
  string(JSON _permitted_count ERROR_VARIABLE _gbl_permitted_error
         LENGTH "${_gbl_json}" fixtures 0 permitted_use)
  if(NOT _gbl_permitted_error STREQUAL "NOTFOUND" OR
     NOT _permitted_count EQUAL 1)
    gbl_v_fail("permission scope is broader than local-only grant")
  endif()
  gbl_v_get(_permitted fixtures 0 permitted_use 0)
  gbl_v_equal(_permitted "local-only" "permitted use")

  gbl_v_get(_profile fixtures 0 machine profile)
  gbl_v_equal(_profile "agb" "profile selector")
  gbl_v_get(_profile_version fixtures 0 machine profile_version)
  gbl_v_equal(_profile_version "16" "profile version")
  gbl_v_get(_start_mode fixtures 0 machine start_mode)
  gbl_v_equal(_start_mode "direct" "start selector")
  gbl_v_get(_cartridge_mode fixtures 0 machine cartridge_mode)
  gbl_v_equal(_cartridge_mode "rom-only" "cartridge selector")
  gbl_v_get(_bios fixtures 0 machine bios_requirement)
  gbl_v_equal(_bios "none" "BIOS requirement")

  gbl_v_get(_generator_source fixtures 0 build recipe generator_source)
  gbl_v_equal(_generator_source "tests/fixtures/phase21_diagnostic.c"
              "generator source")
  gbl_v_get(_language_standard fixtures 0 build recipe language_standard)
  gbl_v_equal(_language_standard "C17" "generator language")
  string(JSON _flag_count ERROR_VARIABLE _gbl_flag_error
         LENGTH "${_gbl_json}" fixtures 0 build recipe compiler_flags)
  if(NOT _gbl_flag_error STREQUAL "NOTFOUND" OR NOT _flag_count EQUAL 11)
    gbl_v_fail("manifest compiler recipe does not have the expected strict flags")
  endif()
  set(_expected_flags
      -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
      -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wcast-qual -Werror)
  set(_flag_index 0)
  foreach(_expected_flag IN LISTS _expected_flags)
    gbl_v_get(_actual_flag fixtures 0 build recipe compiler_flags ${_flag_index})
    gbl_v_equal(_actual_flag "${_expected_flag}" "compiler flag")
    math(EXPR _flag_index "${_flag_index} + 1")
  endforeach()

  gbl_v_get(_toolchain_compiler fixtures 0 build toolchain compiler)
  gbl_v_equal(_toolchain_compiler "${GBL_COMPILER_IDENTITY}" "generator compiler identity")
  gbl_v_get(_toolchain_target fixtures 0 build toolchain target)
  gbl_v_equal(_toolchain_target "${GBL_COMPILER_TARGET}" "generator compiler target")
  gbl_v_get(_toolchain_cmake fixtures 0 build toolchain cmake)
  gbl_v_equal(_toolchain_cmake "${GBL_CMAKE_IDENTITY}" "CMake identity")
  gbl_v_get(_toolchain_ninja fixtures 0 build toolchain ninja)
  gbl_v_equal(_toolchain_ninja "${GBL_NINJA_IDENTITY}" "Ninja identity")

  gbl_v_get(_artifact_path fixtures 0 build artifact path)
  gbl_v_equal(_artifact_path
      "build/phase21-local/fixture/phase21-diagnostic.gba" "artifact path")
  gbl_v_get(_recipe_output fixtures 0 build recipe output_path)
  gbl_v_equal(_recipe_output "${_artifact_path}" "recipe output path")
  gbl_v_get(_artifact_size fixtures 0 build artifact size_bytes)
  gbl_v_equal(_artifact_size "1024" "artifact size")
  gbl_v_get(_artifact_sha fixtures 0 build artifact sha256)

  gbl_v_get(_guest_case fixtures 0 expected guest case_id)
  gbl_v_equal(_guest_case "21001" "expected guest case")
  gbl_v_get(_guest_status fixtures 0 expected guest status)
  gbl_v_equal(_guest_status "pass" "expected guest status")
  gbl_v_get(_stop_reason fixtures 0 expected guest completion_stop_reason)
  gbl_v_equal(_stop_reason "no_bios_swi" "guest completion stop")
  gbl_v_get(_cycle_ceiling fixtures 0 expected guest cycle_ceiling)
  gbl_v_equal(_cycle_ceiling "300000" "cycle ceiling")
  gbl_v_get(_requested_units fixtures 0 expected guest requested_units)
  gbl_v_equal(_requested_units "200000" "requested unit goal")
  string(JSON _input_count ERROR_VARIABLE _gbl_input_error
         LENGTH "${_gbl_json}" fixtures 0 expected timed_inputs)
  if(NOT _gbl_input_error STREQUAL "NOTFOUND" OR NOT _input_count EQUAL 3)
    gbl_v_fail("manifest must bind all three timestamped input events")
  endif()
  set(_expected_cycles 0 1024 2048)
  set(_expected_masks 0 1 2)
  set(_input_index 0)
  foreach(_index RANGE 0 2)
    gbl_v_get(_input_cycle fixtures 0 expected timed_inputs ${_index} cycle)
    gbl_v_get(_input_mask fixtures 0 expected timed_inputs ${_index} pressed_mask)
    list(GET _expected_cycles ${_input_index} _expected_cycle)
    list(GET _expected_masks ${_input_index} _expected_mask)
    gbl_v_equal(_input_cycle "${_expected_cycle}" "timed input cycle")
    gbl_v_equal(_input_mask "${_expected_mask}" "timed input mask")
    math(EXPR _input_index "${_input_index} + 1")
  endforeach()
  gbl_v_get(_control_cycle fixtures 0 expected timing_control cycle)
  gbl_v_equal(_control_cycle "1536" "timing control cycle")
  gbl_v_get(_control_status fixtures 0 expected timing_control status)
  gbl_v_equal(_control_status "fail" "timing control verdict")
  gbl_v_get(_control_case fixtures 0 expected timing_control case_id)
  gbl_v_equal(_control_case "21001" "timing control case")

  gbl_v_get(_video_format fixtures 0 expected video format)
  gbl_v_equal(_video_format "bgr555-le" "video format")
  gbl_v_get(_video_width fixtures 0 expected video width)
  gbl_v_equal(_video_width "240" "video width")
  gbl_v_get(_video_height fixtures 0 expected video height)
  gbl_v_equal(_video_height "160" "video height")
  gbl_v_get(_video_epoch fixtures 0 expected video epoch)
  gbl_v_equal(_video_epoch "0" "first complete video epoch")
  gbl_v_get(_video_bytes fixtures 0 expected video bytes)
  gbl_v_equal(_video_bytes "76800" "video byte count")
  string(JSON _pixel_count ERROR_VARIABLE _gbl_pixel_error
         LENGTH "${_gbl_json}" fixtures 0 expected video pixels)
  if(NOT _gbl_pixel_error STREQUAL "NOTFOUND" OR NOT _pixel_count EQUAL 3)
    gbl_v_fail("manifest must bind three literal video pixels")
  endif()
  foreach(_index RANGE 0 2)
    gbl_v_get(_pixel_x fixtures 0 expected video pixels ${_index} x)
    gbl_v_get(_pixel_y fixtures 0 expected video pixels ${_index} y)
    gbl_v_get(_pixel_value fixtures 0 expected video pixels ${_index} value)
  endforeach()

  gbl_v_get(_audio_format fixtures 0 expected audio format)
  gbl_v_equal(_audio_format "s16le" "audio format")
  gbl_v_get(_audio_rate fixtures 0 expected audio sample_rate)
  gbl_v_equal(_audio_rate "32768" "audio sample rate")
  gbl_v_get(_audio_cadence fixtures 0 expected audio cycles_per_frame)
  gbl_v_equal(_audio_cadence "512" "audio cycle cadence")
  gbl_v_get(_audio_channels fixtures 0 expected audio channels)
  gbl_v_equal(_audio_channels "2" "audio channel count")
  gbl_v_get(_audio_model fixtures 0 expected audio model)
  gbl_v_equal(_audio_model "pre-pwm" "audio model")
  gbl_v_get(_audio_formula fixtures 0 expected audio frame_count_formula)
  gbl_v_equal(_audio_formula "floor(end_cycle/512)-floor(start_cycle/512)"
              "audio frame formula")
  string(JSON _sample_count ERROR_VARIABLE _gbl_sample_error
         LENGTH "${_gbl_json}" fixtures 0 expected audio samples)
  if(NOT _gbl_sample_error STREQUAL "NOTFOUND" OR NOT _sample_count EQUAL 2)
    gbl_v_fail("manifest must bind the two literal audio samples")
  endif()
  foreach(_index RANGE 0 1)
    gbl_v_get(_sample_cycle fixtures 0 expected audio samples ${_index} cycle)
    gbl_v_get(_sample_index fixtures 0 expected audio samples ${_index} index)
    gbl_v_get(_sample_left fixtures 0 expected audio samples ${_index} left)
    gbl_v_get(_sample_right fixtures 0 expected audio samples ${_index} right)
  endforeach()
  gbl_v_get(_qualification fixtures 0 expected hardware_qualification)
  gbl_v_equal(_qualification "unknown" "hardware qualification")

  string(LENGTH "${_source_commit}" _source_commit_length)
  if(NOT _source_commit MATCHES "^[0-9a-f]+$" OR
     NOT _source_commit_length EQUAL 40)
    gbl_v_fail("source revision must be a full Git commit hash")
  endif()
  execute_process(
      COMMAND git -C "${GBL_REPO_ROOT}" cat-file -e "${_source_commit}^{commit}"
      RESULT_VARIABLE _git_commit_result
      OUTPUT_QUIET ERROR_QUIET)
  if(NOT _git_commit_result EQUAL 0)
    gbl_v_fail("source revision cannot be resolved")
  endif()
  file(REAL_PATH "${GBL_REPO_ROOT}/${_source_path}" _manifest_source_real)
  file(REAL_PATH "${GBL_SOURCE_PATH}" _current_source_real)
  if(NOT _manifest_source_real STREQUAL _current_source_real)
    gbl_v_fail("manifest source path does not identify the current generator")
  endif()
  if(NOT EXISTS "${_current_source_real}" OR IS_SYMLINK "${GBL_SOURCE_PATH}")
    gbl_v_fail("current diagnostic source is missing or symlinked")
  endif()
  file(SHA256 "${_current_source_real}" _current_source_sha)
  set(_source_snapshot "${GBL_SCRATCH}/phase21-source-at-commit.c")
  file(REMOVE "${_source_snapshot}")
  execute_process(
      COMMAND git -C "${GBL_REPO_ROOT}" show
              "${_source_commit}:${_source_path}"
      OUTPUT_FILE "${_source_snapshot}"
      RESULT_VARIABLE _git_show_result
      ERROR_VARIABLE _git_show_error)
  if(NOT _git_show_result EQUAL 0 OR NOT EXISTS "${_source_snapshot}")
    gbl_v_fail("source revision does not contain the cited generator path")
  endif()
  file(SHA256 "${_source_snapshot}" _revision_source_sha)
  file(REMOVE "${_source_snapshot}")
  if(NOT _source_sha STREQUAL _current_source_sha OR
     NOT _source_sha STREQUAL _revision_source_sha)
    gbl_v_fail("source SHA-256 mismatch")
  endif()

  file(REAL_PATH "${GBL_REPO_ROOT}/build/phase21-local/fixture/phase21-diagnostic.gba"
       _expected_rom_real)
  file(REAL_PATH "${GBL_ROM_PATH}" _current_rom_real)
  if(NOT _expected_rom_real STREQUAL _current_rom_real OR
     NOT EXISTS "${_current_rom_real}" OR IS_SYMLINK "${GBL_ROM_PATH}")
    gbl_v_fail("generated image is missing, symlinked, or outside its local path")
  endif()
  execute_process(
      COMMAND git -C "${GBL_REPO_ROOT}" check-ignore -q --
              "${_artifact_path}"
      RESULT_VARIABLE _ignored_result
      OUTPUT_QUIET ERROR_QUIET)
  if(NOT _ignored_result EQUAL 0)
    gbl_v_fail("generated image path is not ignored")
  endif()
  execute_process(
      COMMAND git -C "${GBL_REPO_ROOT}" ls-files --error-unmatch --
              "${_artifact_path}"
      RESULT_VARIABLE _tracked_result
      OUTPUT_QUIET ERROR_QUIET)
  if(_tracked_result EQUAL 0)
    gbl_v_fail("generated image unexpectedly appears in tracked files")
  endif()
  file(SIZE "${_current_rom_real}" _current_rom_size)
  file(SHA256 "${_current_rom_real}" _current_rom_sha)
  if(NOT "${_current_rom_size}" STREQUAL "${_artifact_size}" OR
     NOT _current_rom_sha STREQUAL _artifact_sha)
    gbl_v_fail("ROM SHA-256 mismatch or generated image size changed")
  endif()

  file(REAL_PATH "${GBL_REPO_ROOT}/${_notice_relative}" _notice_real)
  if(NOT EXISTS "${_notice_real}" OR IS_SYMLINK "${GBL_REPO_ROOT}/${_notice_relative}")
    gbl_v_fail("rights notice is missing or symlinked")
  endif()
  file(READ "${_notice_real}" _notice_text)
  foreach(_required_text IN ITEMS
      "local Phase 21" "verification only" "No public license is asserted"
      "public CI" "upload" "redistribution")
    string(FIND "${_notice_text}" "${_required_text}" _notice_position)
    if(_notice_position EQUAL -1)
      gbl_v_fail("rights notice is missing required scope text: ${_required_text}")
    endif()
  endforeach()

  set(${output_ok} TRUE PARENT_SCOPE)
  set(${output_error} "" PARENT_SCOPE)
endfunction()

function(gbl_expect_manifest_rejection name expected_reason json_text)
  set(_control_path "${GBL_SCRATCH}/${name}.json")
  file(WRITE "${_control_path}" "${json_text}\n")
  gbl_validate_manifest("${_control_path}" _control_ok _control_error)
  file(REMOVE "${_control_path}")
  if(_control_ok)
    message(FATAL_ERROR "phase21-negative-control: ${name} was incorrectly accepted")
  endif()
  if(NOT _control_error MATCHES "${expected_reason}")
    message(FATAL_ERROR
        "phase21-negative-control: ${name} rejected for the wrong reason: ${_control_error}")
  endif()
  file(APPEND "${GBL_EVIDENCE_ROOT}/manifest-controls.txt"
       "${name}=rejected (${_control_error})\n")
endfunction()

function(gbl_u16le_hex value output)
  math(EXPR _number "${value} & 65535")
  math(EXPR _hex "${_number}" OUTPUT_FORMAT HEXADECIMAL)
  string(REGEX REPLACE "^0x" "" _digits "${_hex}")
  string(LENGTH "${_digits}" _length)
  while(_length LESS 4)
    set(_digits "0${_digits}")
    math(EXPR _length "${_length} + 1")
  endwhile()
  string(SUBSTRING "${_digits}" 2 2 _low)
  string(SUBSTRING "${_digits}" 0 2 _high)
  set(${output} "${_low}${_high}" PARENT_SCOPE)
endfunction()

function(gbl_require_evidence_equal manifest_json manifest_path
         observation_json observation_path label)
  gbl_compare_json("${manifest_json}" "${manifest_path}"
      "${observation_json}" "${observation_path}" "${label}" _error)
  if(NOT _error STREQUAL "")
    message(FATAL_ERROR "phase21-evidence: ${_error}")
  endif()
endfunction()

file(REAL_PATH "${GBL_REPO_ROOT}" _repo_real)
file(REAL_PATH "${GBL_SCRATCH}" _scratch_real)
file(REAL_PATH "${GBL_EVIDENCE_ROOT}" _evidence_real)
if(NOT IS_DIRECTORY "${_scratch_real}" OR IS_SYMLINK "${GBL_SCRATCH}" OR
   NOT IS_DIRECTORY "${_evidence_real}" OR IS_SYMLINK "${GBL_EVIDENCE_ROOT}")
  message(FATAL_ERROR "phase21-manifest: task-owned scratch paths are missing or symlinked")
endif()
string(FIND "${_scratch_real}/" "${_repo_real}/build/phase21-local/work/" _scratch_prefix)
string(FIND "${_evidence_real}/" "${_repo_real}/build/phase21-local/evidence/" _evidence_prefix)
if(NOT _scratch_prefix EQUAL 0 OR NOT _evidence_prefix EQUAL 0)
  message(FATAL_ERROR "phase21-manifest: scratch path escaped its ignored local root")
endif()

gbl_validate_manifest("${GBL_MANIFEST}" _manifest_ok _manifest_error)
if(NOT _manifest_ok)
  message(FATAL_ERROR "phase21-manifest: ${_manifest_error}")
endif()
file(READ "${GBL_MANIFEST}" _manifest_json)

if(GBL_STAGE STREQUAL "preflight")
  if(GBL_NEGATIVE_CONTROLS)
    string(JSON _mutated SET "${_manifest_json}" fixtures 0 source commit
        "\"0000000000000000000000000000000000000000\"")
    gbl_expect_manifest_rejection("source-revision-drift"
        "source revision cannot be resolved" "${_mutated}")
    string(JSON _mutated SET "${_manifest_json}" fixtures 0 source sha256
        "\"0000000000000000000000000000000000000000000000000000000000000000\"")
    gbl_expect_manifest_rejection("source-digest-drift"
        "source SHA-256 mismatch" "${_mutated}")
    string(JSON _mutated SET "${_manifest_json}" fixtures 0 build artifact sha256
        "\"0000000000000000000000000000000000000000000000000000000000000000\"")
    gbl_expect_manifest_rejection("rom-digest-drift"
        "ROM SHA-256 mismatch" "${_mutated}")
    string(JSON _mutated SET "${_manifest_json}" fixtures 0 permitted_use
        "[\"local-only\",\"public-ci\"]")
    gbl_expect_manifest_rejection("broadened-permission"
        "permission scope is broader than local-only grant" "${_mutated}")
  endif()
  file(APPEND "${GBL_EVIDENCE_ROOT}/manifest-controls.txt"
       "manifest=source, rights, and generated image accepted\n")
  message(STATUS "phase21-manifest: provenance, grant, recipe, and image verified")
elseif(GBL_STAGE STREQUAL "evidence")
  foreach(_required IN ITEMS GBL_RUN1_DIR GBL_RUN2_DIR GBL_TIMING_CONTROL_DIR)
    if(NOT DEFINED ${_required} OR NOT IS_DIRECTORY "${${_required}}")
      message(FATAL_ERROR "phase21-evidence: missing consumer output directory ${_required}")
    endif()
  endforeach()
  foreach(_run_dir IN ITEMS "${GBL_RUN1_DIR}" "${GBL_RUN2_DIR}" "${GBL_TIMING_CONTROL_DIR}")
    foreach(_file IN ITEMS observations.json frame.bgr555le audio.s16le)
      if(NOT EXISTS "${_run_dir}/${_file}" OR IS_SYMLINK "${_run_dir}/${_file}")
        message(FATAL_ERROR "phase21-evidence: consumer output is missing: ${_run_dir}/${_file}")
      endif()
    endforeach()
  endforeach()
  file(READ "${GBL_RUN1_DIR}/observations.json" _observations)
  file(READ "${GBL_RUN2_DIR}/observations.json" _observations_second)
  file(READ "${GBL_TIMING_CONTROL_DIR}/observations.json" _timing_observations)
  if(NOT _observations STREQUAL _observations_second)
    message(FATAL_ERROR "phase21-evidence: fresh-run normalized observations differ")
  endif()
  gbl_json_get(_observation_schema _observation_error "${_observations}" schema)
  if(NOT _observation_error STREQUAL "" OR
     NOT _observation_schema STREQUAL "gamebloke.phase21-consumer/1")
    message(FATAL_ERROR "phase21-evidence: installed consumer observation schema is invalid")
  endif()

  foreach(_pair IN ITEMS
      "fixtures.0.machine.profile|profile.id|profile"
      "fixtures.0.machine.profile_version|profile.version|profile version"
      "fixtures.0.machine.bios_requirement|profile.bios|BIOS mode"
      "fixtures.0.expected.hardware_qualification|profile.hardware_qualification|qualification"
      "fixtures.0.expected.guest.case_id|guest.case_id|guest case"
      "fixtures.0.expected.guest.status|guest.status|guest verdict"
      "fixtures.0.expected.guest.status|guest.expected_status|declared guest verdict"
      "fixtures.0.expected.guest.completion_stop_reason|execution.stop_reason|stop reason"
      "fixtures.0.expected.guest.requested_units|execution.requested_units|unit request"
      "fixtures.0.expected.guest.cycle_ceiling|execution.cycle_budget|cycle ceiling"
      "fixtures.0.expected.video.format|video.format|video format"
      "fixtures.0.expected.video.width|video.width|video width"
      "fixtures.0.expected.video.height|video.height|video height"
      "fixtures.0.expected.video.epoch|video.epoch|video epoch"
      "fixtures.0.expected.video.bytes|video.bytes|video byte count"
      "fixtures.0.expected.audio.format|audio.format|audio format"
      "fixtures.0.expected.audio.sample_rate|audio.sample_rate|audio sample rate"
      "fixtures.0.expected.audio.cycles_per_frame|audio.cycles_per_frame|audio cadence"
      "fixtures.0.expected.audio.channels|audio.channels|audio channel count"
      "fixtures.0.expected.audio.model|audio.model|audio model")
    string(REPLACE "|" ";" _parts "${_pair}")
    list(GET _parts 0 _expected_path)
    list(GET _parts 1 _observed_path)
    list(GET _parts 2 _label)
    gbl_require_evidence_equal("${_manifest_json}" "${_expected_path}"
        "${_observations}" "${_observed_path}" "${_label}")
  endforeach()

  gbl_json_get(_cycle_ceiling _error "${_manifest_json}"
      fixtures 0 expected guest cycle_ceiling)
  gbl_json_get(_start_cycle _error "${_observations}" execution start_cycle)
  gbl_json_get(_end_cycle _error "${_observations}" execution end_cycle)
  gbl_json_get(_elapsed_cycles _error "${_observations}" execution elapsed_cycles)
  gbl_json_get(_outcome _error "${_observations}" execution outcome)
  if(NOT _start_cycle STREQUAL "0" OR NOT _end_cycle LESS_EQUAL _cycle_ceiling OR
     NOT _elapsed_cycles STREQUAL _end_cycle OR NOT _outcome STREQUAL "guest_terminal")
    message(FATAL_ERROR "phase21-evidence: bounded guest execution disagrees with manifest")
  endif()
  math(EXPR _expected_frames "${_end_cycle} / 512 - ${_start_cycle} / 512")
  math(EXPR _expected_audio_bytes "${_expected_frames} * 4")
  gbl_json_get(_actual_frames _error "${_observations}" audio frames)
  gbl_json_get(_actual_audio_bytes _error "${_observations}" audio bytes)
  if(NOT _actual_frames STREQUAL "${_expected_frames}" OR
     NOT _actual_audio_bytes STREQUAL "${_expected_audio_bytes}")
    message(FATAL_ERROR "phase21-evidence: audio frame count does not match observed cycle interval")
  endif()

  foreach(_index RANGE 0 2)
    foreach(_field IN ITEMS x y value)
      gbl_require_evidence_equal("${_manifest_json}"
          "fixtures.0.expected.video.pixels.${_index}.${_field}"
          "${_observations}" "pixels.${_index}.${_field}"
          "pixel ${_index} ${_field}")
    endforeach()
    gbl_json_get(_x _error "${_manifest_json}"
        fixtures 0 expected video pixels ${_index} x)
    gbl_json_get(_y _error "${_manifest_json}"
        fixtures 0 expected video pixels ${_index} y)
    gbl_json_get(_pixel _error "${_manifest_json}"
        fixtures 0 expected video pixels ${_index} value)
    gbl_json_get(_width _error "${_manifest_json}"
        fixtures 0 expected video width)
    math(EXPR _pixel_index "${_y} * ${_width} + ${_x}")
    math(EXPR _pixel_offset "${_pixel_index} * 2")
    file(READ "${GBL_RUN1_DIR}/frame.bgr555le" _pixel_hex
         OFFSET "${_pixel_offset}" LIMIT 2 HEX)
    string(TOLOWER "${_pixel_hex}" _pixel_hex)
    gbl_u16le_hex("${_pixel}" _expected_pixel_hex)
    if(NOT _pixel_hex STREQUAL _expected_pixel_hex)
      message(FATAL_ERROR
          "phase21-evidence: literal pixel bytes mismatch at (${_x},${_y})")
    endif()
  endforeach()

  foreach(_index RANGE 0 1)
    foreach(_field IN ITEMS cycle index left right)
      gbl_require_evidence_equal("${_manifest_json}"
          "fixtures.0.expected.audio.samples.${_index}.${_field}"
          "${_observations}" "audio.samples.${_index}.${_field}"
          "audio sample ${_index} ${_field}")
    endforeach()
    gbl_json_get(_frame_index _error "${_manifest_json}"
        fixtures 0 expected audio samples ${_index} index)
    gbl_json_get(_left _error "${_manifest_json}"
        fixtures 0 expected audio samples ${_index} left)
    gbl_json_get(_right _error "${_manifest_json}"
        fixtures 0 expected audio samples ${_index} right)
    math(EXPR _audio_offset "${_frame_index} * 4")
    file(READ "${GBL_RUN1_DIR}/audio.s16le" _audio_hex
         OFFSET "${_audio_offset}" LIMIT 4 HEX)
    string(TOLOWER "${_audio_hex}" _audio_hex)
    gbl_u16le_hex("${_left}" _left_hex)
    gbl_u16le_hex("${_right}" _right_hex)
    if(NOT _audio_hex STREQUAL "${_left_hex}${_right_hex}")
      message(FATAL_ERROR "phase21-evidence: literal signed audio sample bytes mismatch")
    endif()
  endforeach()

  foreach(_pair IN ITEMS
      "fixtures.0.expected.timing_control.case_id|guest.case_id|timing case"
      "fixtures.0.expected.timing_control.status|guest.status|timing verdict"
      "fixtures.0.expected.timing_control.status|guest.expected_status|timing expected verdict"
      "fixtures.0.expected.timing_control.final_mask|input.final_mask|timing final mask")
    string(REPLACE "|" ";" _parts "${_pair}")
    list(GET _parts 0 _expected_path)
    list(GET _parts 1 _observed_path)
    list(GET _parts 2 _label)
    gbl_require_evidence_equal("${_manifest_json}" "${_expected_path}"
        "${_timing_observations}" "${_observed_path}" "${_label}")
  endforeach()
  gbl_require_evidence_equal("${_manifest_json}"
      "fixtures.0.expected.timing_control.cycle"
      "${_timing_observations}" "input.events.1.cycle" "timing input cycle")
  file(SIZE "${GBL_RUN1_DIR}/frame.bgr555le" _video_file_size)
  file(SIZE "${GBL_RUN1_DIR}/audio.s16le" _audio_file_size)
  if(NOT _video_file_size EQUAL 76800 OR
     NOT _audio_file_size EQUAL _expected_audio_bytes)
    message(FATAL_ERROR "phase21-evidence: raw media file extent mismatch")
  endif()
  gbl_json_get(_artifact_sha _error "${_manifest_json}"
      fixtures 0 build artifact sha256)
  file(WRITE "${GBL_EVIDENCE_ROOT}/manifest-validation.txt"
       "provenance=verified\nfixture_sha256="
       "${_artifact_sha}\nconsumer_semantics=verified\nliteral_raw_media=verified\n")
  message(STATUS "phase21-evidence: installed observations and literal raw media verified")
else()
  message(FATAL_ERROR "phase21-manifest: unknown stage '${GBL_STAGE}'")
endif()
