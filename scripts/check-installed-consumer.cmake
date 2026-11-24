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

function(gbl_require_exact_binding lines key expected label)
  set(_binding_count 0)
  set(_binding_value "")
  foreach(_line IN LISTS lines)
    if(_line MATCHES "^${key}=(.*)$")
      math(EXPR _binding_count "${_binding_count} + 1")
      set(_binding_value "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  if(NOT _binding_count EQUAL 1 OR
     NOT "${_binding_value}" STREQUAL "${expected}")
    message(FATAL_ERROR "phase21-fit: ${label} binding mismatch")
  endif()
endfunction()

function(gbl_require_exact_file_binding lines relative_path expected_sha)
  set(_binding_count 0)
  set(_binding_sha "")
  foreach(_line IN LISTS lines)
    string(REPLACE "|" ";" _parts "${_line}")
    list(LENGTH _parts _part_count)
    if(_part_count GREATER_EQUAL 2)
      list(GET _parts 0 _kind)
      list(GET _parts 1 _path)
      if(_kind STREQUAL "file_sha256" AND _path STREQUAL "${relative_path}")
        math(EXPR _binding_count "${_binding_count} + 1")
        if(_part_count EQUAL 3)
          list(GET _parts 2 _binding_sha)
        else()
          set(_binding_sha "")
        endif()
      endif()
    endif()
  endforeach()
  if(NOT _binding_count EQUAL 1 OR
     NOT "${_binding_sha}" STREQUAL "${expected_sha}")
    message(FATAL_ERROR
        "phase21-fit: reviewed file binding mismatch: ${relative_path}")
  endif()
endfunction()

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
elseif(GBL_STAGE STREQUAL "fit")
  foreach(_required IN ITEMS
      GBL_PROOF_PATH GBL_PACKAGE_INPUTS GBL_FIT_REPORT GBL_LIBFUTURO_CHECKOUT)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
      message(FATAL_ERROR "phase21-fit: missing required input ${_required}")
    endif()
  endforeach()
  foreach(_path IN ITEMS "${GBL_PROOF_PATH}" "${GBL_PACKAGE_INPUTS}"
      "${GBL_FIT_REPORT}")
    if(NOT EXISTS "${_path}" OR IS_SYMLINK "${_path}")
      message(FATAL_ERROR "phase21-fit: required fit evidence is missing or symlinked: ${_path}")
    endif()
  endforeach()
  if(NOT IS_DIRECTORY "${GBL_LIBFUTURO_CHECKOUT}" OR
     IS_SYMLINK "${GBL_LIBFUTURO_CHECKOUT}")
    message(FATAL_ERROR "phase21-fit: LibFuturo checkout is missing or symlinked")
  endif()

  file(STRINGS "${GBL_FIT_REPORT}" _fit_report_lines)
  file(READ "${GBL_PROOF_PATH}" _fit_proof)
  file(READ "${GBL_PACKAGE_INPUTS}" _package_inputs)
  file(STRINGS "${GBL_PACKAGE_INPUTS}" _package_input_lines)
  file(SHA256 "${GBL_PROOF_PATH}" _proof_sha)
  set(_proof_binding_count 0)
  set(_proof_binding_value "")
  foreach(_fit_line IN LISTS _fit_report_lines)
    if(_fit_line MATCHES "^normalized_proof_sha256=(.*)$")
      math(EXPR _proof_binding_count "${_proof_binding_count} + 1")
      set(_proof_binding_value "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  string(LENGTH "${_proof_binding_value}" _proof_binding_length)
  if(NOT _proof_binding_count EQUAL 1 OR
     NOT _proof_binding_length EQUAL 64 OR
     NOT _proof_binding_value MATCHES "^[0-9a-fA-F]+$" OR
     NOT _proof_binding_value STREQUAL _proof_sha)
    message(FATAL_ERROR "phase21-fit: normalized proof digest mismatch")
  endif()

  gbl_json_get(_fixture_sha _fit_error "${_manifest_json}"
      fixtures 0 build artifact sha256)
  gbl_json_get(_fixture_source_commit _fit_error "${_manifest_json}"
      fixtures 0 source commit)
  gbl_json_get(_fixture_source_sha _fit_error "${_manifest_json}"
      fixtures 0 source sha256)
  file(SHA256 "${GBL_ROM_PATH}" _actual_fixture_sha)
  file(SHA256 "${GBL_SOURCE_PATH}" _actual_source_sha)
  if(NOT _fixture_sha STREQUAL _actual_fixture_sha)
    message(FATAL_ERROR "phase21-fit: local fixture identity differs from the manifest")
  endif()
  if(NOT _fixture_source_sha STREQUAL _actual_source_sha)
    message(FATAL_ERROR "phase21-fit: diagnostic source identity differs from the manifest")
  endif()
  gbl_require_exact_binding("${_fit_report_lines}" fixture_sha256
      "${_fixture_sha}" "fixture identity")
  gbl_require_exact_binding("${_fit_report_lines}" fixture_source_commit
      "${_fixture_source_commit}" "fixture source revision")
  gbl_require_exact_binding("${_fit_report_lines}" fixture_source_sha256
      "${_fixture_source_sha}" "fixture source identity")

  file(SHA256 "${GBL_REPO_ROOT}/examples/installed-consumer/consumer.c"
       _consumer_source_sha)
  file(SHA256 "${GBL_REPO_ROOT}/include/gamebloke/gamebloke_v1.h"
       _public_header_sha)
  gbl_require_exact_binding("${_fit_report_lines}" consumer_source_sha256
      "${_consumer_source_sha}" "installed consumer identity")
  gbl_require_exact_binding("${_fit_report_lines}" public_header_sha256
      "${_public_header_sha}" "public API header identity")
  foreach(_tool_binding IN ITEMS
      "toolchain_compiler|GBL_COMPILER_IDENTITY"
      "toolchain_target|GBL_COMPILER_TARGET"
      "toolchain_cmake|GBL_CMAKE_IDENTITY"
      "toolchain_ninja|GBL_NINJA_IDENTITY")
    string(REPLACE "|" ";" _tool_binding_parts "${_tool_binding}")
    list(GET _tool_binding_parts 0 _tool_key)
    list(GET _tool_binding_parts 1 _tool_value_variable)
    gbl_require_exact_binding("${_package_input_lines}" "${_tool_key}"
        "${${_tool_value_variable}}" "package toolchain identity")
    gbl_require_exact_binding("${_fit_report_lines}" "${_tool_key}"
        "${${_tool_value_variable}}" "fit report toolchain identity")
  endforeach()

  gbl_json_get(_profile _fit_error "${_fit_proof}" profile id)
  gbl_json_get(_profile_version _fit_error "${_fit_proof}" profile version)
  gbl_json_get(_bios _fit_error "${_fit_proof}" profile bios)
  gbl_json_get(_qualification _fit_error "${_fit_proof}"
      profile hardware_qualification)
  gbl_json_get(_guest_status _fit_error "${_fit_proof}" guest status)
  gbl_json_get(_guest_case _fit_error "${_fit_proof}" guest case_id)
  gbl_json_get(_cycle_budget _fit_error "${_fit_proof}"
      execution cycle_budget)
  gbl_json_get(_end_cycle _fit_error "${_fit_proof}" execution end_cycle)
  gbl_json_get(_retired _fit_error "${_fit_proof}"
      execution retired_instructions)
  gbl_json_get(_audio_frames _fit_error "${_fit_proof}" audio frames)
  gbl_json_get(_audio_bytes _fit_error "${_fit_proof}" audio bytes)
  gbl_json_get(_video_bytes _fit_error "${_fit_proof}" video bytes)
  if(NOT _profile STREQUAL "agb" OR NOT _profile_version STREQUAL "16" OR
     NOT _bios STREQUAL "none" OR NOT _qualification STREQUAL "unknown" OR
     NOT _guest_status STREQUAL "pass" OR NOT _guest_case STREQUAL "21001" OR
     NOT _cycle_budget STREQUAL "300000" OR NOT _end_cycle STREQUAL "197285" OR
     NOT _retired STREQUAL "17957" OR NOT _audio_frames STREQUAL "385" OR
     NOT _audio_bytes STREQUAL "1540" OR NOT _video_bytes STREQUAL "76800")
    message(FATAL_ERROR "phase21-fit: normalized proof is incomplete or changed")
  endif()

  gbl_json_get(_timing_cycle _fit_error "${_fit_proof}"
      refusal_proof timing_control input_cycle)
  gbl_json_get(_timing_status _fit_error "${_fit_proof}"
      refusal_proof timing_control status)
  gbl_json_get(_timing_mask _fit_error "${_fit_proof}"
      refusal_proof timing_control final_mask)
  gbl_json_get(_timing_applied _fit_error "${_fit_proof}"
      refusal_proof timing_control applied)
  gbl_json_get(_timing_pending _fit_error "${_fit_proof}"
      refusal_proof timing_control pending)
  if(NOT _timing_cycle STREQUAL "1536" OR
     NOT _timing_status STREQUAL "fail" OR NOT _timing_mask STREQUAL "2" OR
     NOT _timing_applied STREQUAL "3" OR NOT _timing_pending STREQUAL "0")
    message(FATAL_ERROR "phase21-fit: measured timing control is missing or changed")
  endif()

  string(JSON _refusal_count ERROR_VARIABLE _refusal_error
      LENGTH "${_fit_proof}" refusal_proof cases)
  if(NOT _refusal_error STREQUAL "NOTFOUND" OR NOT _refusal_count EQUAL 5)
    message(FATAL_ERROR "phase21-fit: proof must contain all five refusal cases")
  endif()
  set(_expected_names short-rom advance-size audio-capacity work-budget video-capacity)
  set(_expected_statuses content_refused invalid_argument
      insufficient_capacity work_exhausted insufficient_capacity)
  foreach(_index RANGE 0 4)
    list(GET _expected_names ${_index} _expected_name)
    list(GET _expected_statuses ${_index} _expected_status)
    gbl_json_get(_name _fit_error "${_fit_proof}"
        refusal_proof cases ${_index} name)
    gbl_json_get(_expected _fit_error "${_fit_proof}"
        refusal_proof cases ${_index} expected_status)
    gbl_json_get(_observed _fit_error "${_fit_proof}"
        refusal_proof cases ${_index} observed_status)
    gbl_json_get(_unchanged _fit_error "${_fit_proof}"
        refusal_proof cases ${_index} caller_buffer unchanged)
    gbl_json_get(_observation_unchanged _fit_error "${_fit_proof}"
        refusal_proof cases ${_index} observation_unchanged)
    gbl_json_get(_committed_cycles _fit_error "${_fit_proof}"
        refusal_proof cases ${_index} committed_cycles)
    gbl_json_get(_retry_matches _fit_error "${_fit_proof}"
        refusal_proof cases ${_index} retry matches_clean_reference)
    string(JSON _before ERROR_VARIABLE _before_error GET "${_fit_proof}"
        refusal_proof cases ${_index} before)
    string(JSON _after ERROR_VARIABLE _after_error GET "${_fit_proof}"
        refusal_proof cases ${_index} after)
    if(NOT _before_error STREQUAL "NOTFOUND" OR
       NOT _after_error STREQUAL "NOTFOUND" OR
       NOT _before STREQUAL _after)
      message(FATAL_ERROR "phase21-fit: refusal ${_expected_name} does not preserve every public observation field")
    endif()
    if(NOT _name STREQUAL _expected_name OR
       NOT _expected STREQUAL _expected_status OR
       NOT _observed STREQUAL _expected_status OR NOT _unchanged OR
       NOT _observation_unchanged OR NOT _committed_cycles STREQUAL "0" OR
       NOT _retry_matches)
      message(FATAL_ERROR "phase21-fit: refusal ${_expected_name} status, canary, state, or retry proof failed")
    endif()
    if(_expected_name STREQUAL "advance-size" OR
       _expected_name STREQUAL "audio-capacity" OR
       _expected_name STREQUAL "work-budget")
      gbl_json_get(_result_unchanged _fit_error "${_fit_proof}"
          refusal_proof cases ${_index} result_canary_unchanged)
      if(NOT _result_unchanged)
        message(FATAL_ERROR "phase21-fit: refusal ${_expected_name} changed the result canary")
      endif()
    endif()
  endforeach()
  gbl_json_get(_probe_units _fit_error "${_fit_proof}"
      refusal_proof bounded_probe units)
  gbl_json_get(_probe_budget _fit_error "${_fit_proof}"
      refusal_proof bounded_probe cycle_budget)
  gbl_json_get(_probe_end _fit_error "${_fit_proof}"
      refusal_proof bounded_probe end_cycle)
  gbl_json_get(_probe_frames _fit_error "${_fit_proof}"
      refusal_proof bounded_probe audio_frames)
  if(NOT _probe_units STREQUAL "128" OR
     NOT _probe_budget STREQUAL "2048" OR NOT _probe_end GREATER 512 OR
     NOT _probe_end LESS_EQUAL _probe_budget OR NOT _probe_frames GREATER 0)
    message(FATAL_ERROR "phase21-fit: positive bounded probe did not prove guest work and an audio sample")
  endif()

  set(_libfuturo_paths
      docs/contract.md
      docs/gabbaboy-profile.md
      docs/muhtari-profile.md
      docs/nesturbator-profile.md
      docs/synthetic-profile.md
      include/libfuturo/libfuturo.h
      src/gabbaboy_adapter.c
      src/muhtari_adapter.c
      src/nesturbator_adapter.c
      src/synthetic.c)
  execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env GIT_OPTIONAL_LOCKS=0
              git -C "${GBL_LIBFUTURO_CHECKOUT}" rev-parse HEAD
      RESULT_VARIABLE _head_result OUTPUT_VARIABLE _libfuturo_head
      ERROR_VARIABLE _head_error OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _head_result EQUAL 0)
    message(FATAL_ERROR "phase21-fit: could not refresh LibFuturo HEAD: ${_head_error}")
  endif()
  gbl_require_exact_binding("${_fit_report_lines}" libfuturo_head
      "${_libfuturo_head}" "LibFuturo revision")
  execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env GIT_OPTIONAL_LOCKS=0
              git -C "${GBL_LIBFUTURO_CHECKOUT}" status --short -- ${_libfuturo_paths}
      RESULT_VARIABLE _status_result OUTPUT_VARIABLE _libfuturo_status
      ERROR_VARIABLE _status_error OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT _status_result EQUAL 0)
    message(FATAL_ERROR "phase21-fit: could not refresh relevant LibFuturo status: ${_status_error}")
  endif()
  string(REPLACE "\n" ";" _status_lines "${_libfuturo_status}")
  set(_normalized_status_lines)
  foreach(_status_line IN LISTS _status_lines)
    string(STRIP "${_status_line}" _status_line)
    if(NOT _status_line STREQUAL "")
      list(APPEND _normalized_status_lines "${_status_line}")
    endif()
  endforeach()
  list(JOIN _normalized_status_lines "|" _normalized_status)
  gbl_require_exact_binding("${_fit_report_lines}"
      libfuturo_relevant_status "${_normalized_status}"
      "LibFuturo relevant working-tree status")
  foreach(_relative_path IN LISTS _libfuturo_paths)
    set(_absolute_path "${GBL_LIBFUTURO_CHECKOUT}/${_relative_path}")
    if(NOT EXISTS "${_absolute_path}" OR IS_SYMLINK "${_absolute_path}")
      message(FATAL_ERROR "phase21-fit: reviewed LibFuturo source is missing or symlinked: ${_relative_path}")
    endif()
    file(SHA256 "${_absolute_path}" _current_sha)
    gbl_require_exact_file_binding("${_fit_report_lines}"
        "${_relative_path}" "${_current_sha}")
  endforeach()
  file(READ "${GBL_LIBFUTURO_CHECKOUT}/src/synthetic.c" _profile_registry)
  foreach(_declared_profile IN ITEMS "synthetic-v1" "nesturbator-ntsc-v1"
      "muhtari-ntsc-v1" "gabbaboy-dmg-v1")
    string(FIND "${_profile_registry}" "${_declared_profile}" _declared_position)
    if(_declared_position EQUAL -1)
      message(FATAL_ERROR "phase21-fit: expected LibFuturo profile declaration is absent: ${_declared_profile}")
    endif()
  endforeach()
  string(FIND "${_profile_registry}" "\"agb\"" _gba_profile_position)
  if(NOT _gba_profile_position EQUAL -1)
    message(FATAL_ERROR "phase21-fit: source registry now declares an agb profile; refresh the fit report")
  endif()

  set(_fit_headings
      "Input timing" "Bounded work" "Output ownership" "Evidence semantics")
  set(_fit_profile_references
      "docs/muhtari-profile.md"
      "docs/gabbaboy-profile.md|docs/nesturbator-profile.md"
      "docs/muhtari-profile.md|docs/gabbaboy-profile.md|docs/nesturbator-profile.md"
      "docs/synthetic-profile.md|docs/muhtari-profile.md|docs/gabbaboy-profile.md|docs/nesturbator-profile.md|src/synthetic.c")
  set(_fit_measurement_references
      "timing_control|1,536"
      "bounded_probe|2,048"
      "audio-capacity|video-capacity|76,800"
      "timing_control|mailbox|unknown")
  set(_fit_ordered_headings ${_fit_headings} Recommendation)
  set(_fit_heading_count 0)
  set(_fit_active_section_index -1)
  set(_fit_section_text "")
  set(_fit_inside_fence FALSE)
  set(_fit_fence_character "")
  set(_fit_fence_length 0)
  set(_fit_inside_comment FALSE)
  foreach(_fit_line IN LISTS _fit_report_lines)
    if(_fit_inside_fence)
      if(_fit_line MATCHES "^ *(`+|~+)(.*)$")
        set(_fit_candidate_fence_run "${CMAKE_MATCH_1}")
        set(_fit_candidate_fence_tail "${CMAKE_MATCH_2}")
        string(LENGTH "${_fit_candidate_fence_run}" _fit_candidate_fence_length)
        string(SUBSTRING "${_fit_candidate_fence_run}" 0 1
            _fit_candidate_fence_character)
        if(_fit_candidate_fence_character STREQUAL _fit_fence_character AND
           _fit_candidate_fence_length GREATER_EQUAL _fit_fence_length AND
           _fit_candidate_fence_tail MATCHES "^[ \t]*$")
          set(_fit_inside_fence FALSE)
          set(_fit_fence_character "")
          set(_fit_fence_length 0)
        endif()
      endif()
      continue()
    endif()
    if(_fit_inside_comment)
      if(_fit_line MATCHES "-->")
        set(_fit_inside_comment FALSE)
      endif()
      continue()
    endif()
    if(_fit_line MATCHES "^ *(`+|~+)(.*)$")
      set(_fit_candidate_fence_run "${CMAKE_MATCH_1}")
      set(_fit_candidate_fence_tail "${CMAKE_MATCH_2}")
      string(LENGTH "${_fit_candidate_fence_run}" _fit_candidate_fence_length)
      string(SUBSTRING "${_fit_candidate_fence_run}" 0 1
          _fit_candidate_fence_character)
      set(_fit_valid_opening_tail FALSE)
      if(_fit_candidate_fence_tail STREQUAL "" OR
         _fit_candidate_fence_tail MATCHES "^[ \t]")
        set(_fit_valid_opening_tail TRUE)
      endif()
      if(_fit_candidate_fence_length GREATER_EQUAL 3 AND
         _fit_valid_opening_tail)
        set(_fit_valid_backtick_info TRUE)
        if(_fit_candidate_fence_character STREQUAL "`" AND
           _fit_candidate_fence_tail MATCHES "`")
          set(_fit_valid_backtick_info FALSE)
        endif()
        if(_fit_valid_backtick_info)
          set(_fit_inside_fence TRUE)
          set(_fit_fence_character "${_fit_candidate_fence_character}")
          set(_fit_fence_length ${_fit_candidate_fence_length})
          continue()
        endif()
      endif()
    endif()
    if(_fit_line MATCHES "<!--")
      if(NOT _fit_line MATCHES "-->")
        set(_fit_inside_comment TRUE)
      endif()
      continue()
    endif()
    if(_fit_line MATCHES "^## (Input timing|Bounded work|Output ownership|Evidence semantics|Recommendation)$")
      if(_fit_heading_count GREATER_EQUAL 5)
        message(FATAL_ERROR "phase21-fit: fit report repeats a required heading")
      endif()
      list(GET _fit_ordered_headings ${_fit_heading_count} _expected_heading)
      set(_observed_heading "${CMAKE_MATCH_1}")
      if(NOT _observed_heading STREQUAL _expected_heading)
        list(FIND _fit_ordered_headings "${_observed_heading}"
            _observed_heading_index)
        if(_observed_heading_index GREATER _fit_heading_count AND
           _fit_heading_count LESS 4)
          message(FATAL_ERROR
              "phase21-fit: missing required fit dimension: ${_expected_heading}")
        endif()
        message(FATAL_ERROR "phase21-fit: malformed fit heading order")
      endif()
      if(_fit_active_section_index GREATER_EQUAL 0)
        set(_fit_section_${_fit_active_section_index} "${_fit_section_text}")
      endif()
      set(_fit_active_section_index ${_fit_heading_count})
      math(EXPR _fit_heading_count "${_fit_heading_count} + 1")
      set(_fit_section_text "")
    elseif(_fit_active_section_index GREATER_EQUAL 0)
      string(APPEND _fit_section_text "${_fit_line}\n")
    endif()
  endforeach()
  if(_fit_active_section_index GREATER_EQUAL 0)
    set(_fit_section_${_fit_active_section_index} "${_fit_section_text}")
  endif()
  if(NOT _fit_heading_count EQUAL 5)
    if(_fit_heading_count LESS 4)
      list(GET _fit_headings ${_fit_heading_count} _missing_heading)
      message(FATAL_ERROR
          "phase21-fit: missing required fit dimension: ${_missing_heading}")
    endif()
    message(FATAL_ERROR "phase21-fit: missing required Recommendation heading")
  endif()
  foreach(_index RANGE 0 3)
    list(GET _fit_headings ${_index} _heading)
    set(_section_variable "_fit_section_${_index}")
    set(_section_text "${${_section_variable}}")
    foreach(_source_marker IN ITEMS "proof.json" "docs/core-api-v1.md"
        "include/libfuturo/libfuturo.h" "docs/contract.md")
      string(FIND "${_section_text}" "${_source_marker}" _source_position)
      if(_source_position EQUAL -1)
        message(FATAL_ERROR "phase21-fit: ${_heading} lacks source or measurement reference ${_source_marker}")
      endif()
    endforeach()
    list(GET _fit_profile_references ${_index} _profile_reference_list)
    string(REPLACE "|" " " _profile_reference_list "${_profile_reference_list}")
    separate_arguments(_profile_reference_list)
    foreach(_profile_reference IN LISTS _profile_reference_list)
      string(FIND "${_section_text}" "${_profile_reference}" _profile_position)
      if(_profile_position EQUAL -1)
        message(FATAL_ERROR "phase21-fit: ${_heading} lacks profile/registry source ${_profile_reference}")
      endif()
    endforeach()
    list(GET _fit_measurement_references ${_index} _measurement_reference_list)
    string(REPLACE "|" " " _measurement_reference_list "${_measurement_reference_list}")
    separate_arguments(_measurement_reference_list)
    foreach(_measurement_reference IN LISTS _measurement_reference_list)
      string(FIND "${_section_text}" "${_measurement_reference}" _measurement_position)
      if(_measurement_position EQUAL -1)
        message(FATAL_ERROR "phase21-fit: ${_heading} lacks measured reference ${_measurement_reference}")
      endif()
    endforeach()
    string(FIND "${_section_text}" "**Follow-up.**" _followup_position)
    if(_followup_position EQUAL -1)
      message(FATAL_ERROR "phase21-fit: ${_heading} lacks a concrete follow-up")
    endif()
  endforeach()
  message(STATUS "phase21-fit: report dimensions, proof, fixture, toolchain, and read-only LibFuturo state verified")
else()
  message(FATAL_ERROR "phase21-manifest: unknown stage '${GBL_STAGE}'")
endif()
