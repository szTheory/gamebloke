cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN TOOLCHAIN_ROOT CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "diagnostics-case: missing-${required}")
  endif()
endforeach()
if(NOT CASE STREQUAL "smoke-pair")
  message(FATAL_ERROR "diagnostics-case: unsupported-case")
endif()

get_filename_component(source_root "${SRC}" ABSOLUTE)
get_filename_component(binary_root "${BIN}" ABSOLUTE)
get_filename_component(toolchain_root "${TOOLCHAIN_ROOT}" ABSOLUTE)
set(manifest "${source_root}/tests/fixtures/manifest.json")
set(registry "${source_root}/registry/disputes.json")
set(vocab "${source_root}/contracts/evidence.json")
set(notice "${source_root}/diagnostics/armv4t/NOTICE")
set(inputs "${source_root}/diagnostics/armv4t/inputs.txt")
set(runner "${binary_root}/gamebloke-run")
set(artifact_root "${binary_root}/generated/diagnostics/armv4t")
set(result_root "${binary_root}/diagnostics-results")

foreach(path IN ITEMS "${manifest}" "${registry}" "${vocab}" "${notice}"
    "${inputs}" "${source_root}/cmake/GenerateFixtureIndex.cmake"
    "${source_root}/cmake/CheckResult.cmake")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}" OR IS_SYMLINK "${path}")
    message(FATAL_ERROR "diagnostics-case: required-input-missing: ${path}")
  endif()
endforeach()
if(NOT EXISTS "${runner}" OR IS_DIRECTORY "${runner}" OR IS_SYMLINK "${runner}")
  message(FATAL_ERROR "diagnostics-case: production-runner-missing")
endif()

include("${source_root}/cmake/CheckManifest.cmake")
gbl_contract_read(V "${vocab}")
gbl_registry_read(R "${registry}" V)
gbl_manifest_read(M "${manifest}" R V)
gbl_json_load(manifest_doc "${manifest}")

function(gbl_diagnostics_find_row out doc wanted_id)
  gbl_json_length(row_count "${doc}" fixtures)
  set(found -1)
  if(row_count GREATER 0)
    math(EXPR row_last "${row_count}-1")
    foreach(index RANGE ${row_last})
      gbl_json_string(row_id "${doc}" fixtures ${index} id)
      if("${row_id}" STREQUAL "${wanted_id}")
        set(found "${index}")
        break()
      endif()
    endforeach()
  endif()
  set(${out} "${found}" PARENT_SCOPE)
endfunction()

set(fixture_ids smoke smoke-broken)
set(source_commit "")
foreach(fixture_id IN LISTS fixture_ids)
  gbl_diagnostics_find_row(row_index "${manifest_doc}" "${fixture_id}")
  if(row_index LESS 0)
    message(FATAL_ERROR
      "diagnostics-case: missing-production-fixture-row: ${fixture_id}")
  endif()

  gbl_json_string(source_kind "${manifest_doc}" fixtures ${row_index} source kind)
  gbl_json_string(row_commit "${manifest_doc}" fixtures ${row_index} source commit)
  gbl_json_string(artifact_path "${manifest_doc}" fixtures ${row_index}
    build artifact path)
  gbl_json_string(artifact_sha "${manifest_doc}" fixtures ${row_index}
    build artifact sha256)
  gbl_json_string(license_id "${manifest_doc}" fixtures ${row_index}
    license spdx)
  gbl_json_string(redistributable "${manifest_doc}" fixtures ${row_index}
    license redistributable)
  gbl_json_string(bios_requirement "${manifest_doc}" fixtures ${row_index}
    bios_requirement)
  gbl_json_string(provenance_class "${manifest_doc}" fixtures ${row_index}
    expected provenance_class)
  gbl_json_string(recipe "${manifest_doc}" fixtures ${row_index} build recipe)
  gbl_json_string(toolchain_identity "${manifest_doc}" fixtures ${row_index}
    build toolchain identity)
  gbl_json_type(toolchain_sha_type "${manifest_doc}" fixtures ${row_index}
    build toolchain sha256)
  gbl__manifest_is_hex(commit_is_hex "${row_commit}" 40)
  gbl__manifest_is_hex(artifact_is_hex "${artifact_sha}" 64)
  set(toolchain_sha "")
  if("${toolchain_sha_type}" STREQUAL "STRING")
    gbl_json_string(toolchain_sha "${manifest_doc}" fixtures ${row_index}
      build toolchain sha256)
    gbl__manifest_is_hex(toolchain_sha_is_hex "${toolchain_sha}" 64)
  else()
    set(toolchain_sha_is_hex FALSE)
  endif()

  set(expected_artifact_path
    "generated/diagnostics/armv4t/roms/${fixture_id}.gba")
  if(NOT "${source_kind}" STREQUAL "git" OR
     NOT commit_is_hex OR
     NOT "${artifact_path}" STREQUAL "${expected_artifact_path}" OR
     NOT artifact_is_hex OR
     NOT "${license_id}" MATCHES "^LicenseRef-Gamebloke-" OR
     NOT "${redistributable}" STREQUAL "yes" OR
     NOT "${bios_requirement}" STREQUAL "none" OR
     NOT "${provenance_class}" STREQUAL "project-authored" OR
     NOT "${toolchain_identity}" MATCHES "xPack GNU Arm Embedded GCC 15\.2\.1-1\.1" OR
     (NOT "${toolchain_sha_type}" STREQUAL "NULL" AND
      NOT toolchain_sha_is_hex) OR
     ("${toolchain_sha_type}" STREQUAL "STRING" AND
      "${toolchain_sha}" STREQUAL "${artifact_sha}"))
    message(FATAL_ERROR "diagnostics-case: fixture-authority-invalid: ${fixture_id}")
  endif()
  if(NOT "${source_commit}" STREQUAL "" AND
     NOT "${source_commit}" STREQUAL "${row_commit}")
    message(FATAL_ERROR "diagnostics-case: source-revision-mismatch")
  endif()
  set(source_commit "${row_commit}")

  gbl_json_length(notice_count "${manifest_doc}" fixtures ${row_index}
    license notices)
  set(notice_listed FALSE)
  if(notice_count GREATER 0)
    math(EXPR notice_last "${notice_count}-1")
    foreach(notice_index RANGE ${notice_last})
      gbl_json_string(notice_ref "${manifest_doc}" fixtures ${row_index}
        license notices ${notice_index})
      if("${notice_ref}" STREQUAL "diagnostics/armv4t/NOTICE")
        set(notice_listed TRUE)
      endif()
    endforeach()
  endif()
  if(NOT notice_listed)
    message(FATAL_ERROR "diagnostics-case: fixture-notice-missing: ${fixture_id}")
  endif()

  gbl_json_length(use_count "${manifest_doc}" fixtures ${row_index} permitted_use)
  set(public_ci_allowed FALSE)
  if(use_count GREATER 0)
    math(EXPR use_last "${use_count}-1")
    foreach(use_index RANGE ${use_last})
      gbl_json_string(permitted_use "${manifest_doc}" fixtures ${row_index}
        permitted_use ${use_index})
      if("${permitted_use}" STREQUAL "public-ci")
        set(public_ci_allowed TRUE)
      endif()
    endforeach()
  endif()
  if(NOT public_ci_allowed)
    message(FATAL_ERROR "diagnostics-case: public-ci-use-missing: ${fixture_id}")
  endif()

  set(expected_sha_name "expected_sha_${fixture_id}")
  set(${expected_sha_name} "${artifact_sha}")
endforeach()

if("${expected_sha_smoke}" STREQUAL "${expected_sha_smoke-broken}")
  message(FATAL_ERROR "diagnostics-case: positive-and-broken-identical")
endif()

set(build_output_root "${source_root}/build/phase05-spike/05-05-admission")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${source_root}"
    "-DOUT=${build_output_root}"
    "-DTOOLCHAIN_ROOT=${toolchain_root}"
    -DMODE=case
    -DCASE_ID=20481
    "-DSOURCE_COMMIT=${source_commit}"
    -P "${source_root}/cmake/BuildArmDiagnostics.cmake"
  WORKING_DIRECTORY "${source_root}"
  RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_error)
if(NOT "${build_rc}" STREQUAL "0")
  message(FATAL_ERROR
    "diagnostics-case: source-build-failed: ${build_error}${build_out}")
endif()
set(common_startup_object "${build_output_root}/proof/common/startup.o")
set(common_identity "${build_output_root}/proof/common/object-identity.txt")
if(NOT EXISTS "${common_startup_object}" OR IS_DIRECTORY "${common_startup_object}" OR
   NOT EXISTS "${common_identity}")
  message(FATAL_ERROR "diagnostics-case: common-startup-object-missing")
endif()
file(STRINGS "${common_identity}" common_identity_lines)
if(NOT common_identity_lines MATCHES "Reused for the clean and assertion-broken")
  message(FATAL_ERROR "diagnostics-case: common-startup-reuse-unproven")
endif()
set(common_map_line
  "project-object build/phase05-spike/05-05-admission/proof/common/startup.o")
foreach(variant IN ITEMS smoke smoke-broken)
  set(map_inputs "${build_output_root}/proof/${variant}/map-inputs.txt")
  if(NOT EXISTS "${map_inputs}")
    message(FATAL_ERROR "diagnostics-case: map-input-proof-missing: ${variant}")
  endif()
  file(STRINGS "${map_inputs}" map_lines)
  list(FIND map_lines "${common_map_line}" common_map_found)
  if(common_map_found EQUAL -1)
    message(FATAL_ERROR "diagnostics-case: common-object-not-shared: ${variant}")
  endif()
endforeach()
file(MAKE_DIRECTORY "${artifact_root}/roms")
foreach(fixture_id IN LISTS fixture_ids)
  set(source_image "${build_output_root}/roms/${fixture_id}.gba")
  set(artifact_image "${binary_root}/generated/diagnostics/armv4t/roms/${fixture_id}.gba")
  set(expected_sha_name "expected_sha_${fixture_id}")
  set(expected_sha "${${expected_sha_name}}")
  if(NOT EXISTS "${source_image}" OR IS_DIRECTORY "${source_image}" OR
     IS_SYMLINK "${source_image}")
    message(FATAL_ERROR "diagnostics-case: built-image-missing: ${fixture_id}")
  endif()
  file(SHA256 "${source_image}" source_sha)
  if(NOT "${source_sha}" STREQUAL "${expected_sha}")
    message(FATAL_ERROR
      "diagnostics-case: built-image-digest-mismatch: ${fixture_id}")
  endif()
  if(IS_SYMLINK "${artifact_image}" OR
     (EXISTS "${artifact_image}" AND IS_DIRECTORY "${artifact_image}"))
    message(FATAL_ERROR "diagnostics-case: unsafe-artifact-path: ${fixture_id}")
  endif()
  set(copy_artifact TRUE)
  if(EXISTS "${artifact_image}")
    file(SHA256 "${artifact_image}" actual_artifact_sha)
    if("${actual_artifact_sha}" STREQUAL "${expected_sha}")
      set(copy_artifact FALSE)
    endif()
  endif()
  if(copy_artifact)
    file(COPY_FILE "${source_image}" "${artifact_image}" ONLY_IF_DIFFERENT)
  endif()
endforeach()
file(MAKE_DIRECTORY "${result_root}")
foreach(fixture_id IN LISTS fixture_ids)
  set(artifact_image
    "${binary_root}/generated/diagnostics/armv4t/roms/${fixture_id}.gba")
  set(expected_sha_name "expected_sha_${fixture_id}")
  set(expected_sha "${${expected_sha_name}}")
  file(SHA256 "${artifact_image}" actual_artifact_sha)
  if(NOT "${actual_artifact_sha}" STREQUAL "${expected_sha}")
    message(FATAL_ERROR
      "diagnostics-case: artifact-digest-mismatch: ${fixture_id}")
  endif()
endforeach()

# Regenerate the canonical production index with its explicit artifact root,
# then rebuild the production runner so it consumes that immutable header.
set(generated "${binary_root}/generated")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DGBL_SOURCE_DIR=${source_root}"
    "-DGBL_ARTIFACT_ROOT=${binary_root}"
    "-DGBL_CONTROL_IMAGE=${generated}/fixtures/owned-arm-pass.gba"
    "-DGBL_CONTROL_FAIL_IMAGE=${generated}/fixtures/owned-arm-fail.gba"
    "-DGBL_CONTROL_SKIP_IMAGE=${generated}/fixtures/owned-arm-pass.gba"
    "-DGBL_CONTROL_ZERO_CASE_IMAGE=${generated}/fixtures/owned-arm-pass-zero-case.gba"
    "-DGBL_CONTROL_MANIFEST_OUTPUT=${generated}/control/manifest.json"
    "-DGBL_MANIFEST=${manifest}"
    "-DGBL_REGISTRY=${registry}"
    "-DGBL_VOCAB=${vocab}"
    "-DGBL_OUT_PRODUCTION=${generated}/production/fixture_index.h"
    "-DGBL_OUT_CONTROL=${generated}/control/fixture_index.h"
    -P "${source_root}/cmake/GenerateFixtureIndex.cmake"
  WORKING_DIRECTORY "${source_root}"
  RESULT_VARIABLE index_rc OUTPUT_VARIABLE index_out ERROR_VARIABLE index_error)
if(NOT "${index_rc}" STREQUAL "0")
  message(FATAL_ERROR "diagnostics-case: production-index-failed: ${index_error}${index_out}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${binary_root}" --target gamebloke-run
  WORKING_DIRECTORY "${source_root}"
  RESULT_VARIABLE runner_build_rc OUTPUT_VARIABLE runner_build_out
  ERROR_VARIABLE runner_build_error)
if(NOT "${runner_build_rc}" STREQUAL "0")
  message(FATAL_ERROR
    "diagnostics-case: production-runner-build-failed: ${runner_build_error}${runner_build_out}")
endif()

foreach(fixture_id IN LISTS fixture_ids)
  if(fixture_id STREQUAL "smoke")
    set(expected_status PASS)
    set(expected_semantic pass)
    set(expected_reason guest-mailbox-pass)
    set(expected_exit 0)
  else()
    set(expected_status GUEST_FAIL)
    set(expected_semantic fail)
    set(expected_reason guest-mailbox-fail)
    set(expected_exit 1)
  endif()
  set(rom "${binary_root}/generated/diagnostics/armv4t/roms/${fixture_id}.gba")
  set(result_file "${result_root}/${fixture_id}.json")
  execute_process(
    COMMAND "${runner}" --rom "${rom}" --cycles 1000 --fixture "${fixture_id}"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE runner_rc OUTPUT_VARIABLE runner_json ERROR_VARIABLE runner_error)
  if(NOT "${runner_rc}" STREQUAL "${expected_exit}" OR
     "${runner_json}" STREQUAL "")
    message(FATAL_ERROR
      "diagnostics-case: runner-exit-mismatch-${fixture_id}: ${runner_error}${runner_json}")
  endif()
  file(WRITE "${result_file}" "${runner_json}")

  string(JSON status ERROR_VARIABLE status_error GET "${runner_json}" status)
  string(JSON reason ERROR_VARIABLE reason_error GET "${runner_json}" reason)
  string(JSON ran ERROR_VARIABLE ran_error GET "${runner_json}" fixture ran)
  string(JSON loaded_sha ERROR_VARIABLE loaded_error GET "${runner_json}"
    fixture rom_sha256)
  string(JSON semantic ERROR_VARIABLE semantic_error GET "${runner_json}"
    semantic state)
  string(JSON case_id ERROR_VARIABLE case_error GET "${runner_json}"
    semantic case_id)
  string(JSON termination ERROR_VARIABLE termination_error GET "${runner_json}"
    termination)
  string(JSON expected_sha ERROR_VARIABLE expected_error GET "${runner_json}"
    expected_sha256)
  string(JSON cycles ERROR_VARIABLE cycles_error GET "${runner_json}"
    time cycles)
  string(JSON instructions ERROR_VARIABLE instructions_error GET "${runner_json}"
    time instructions)
  string(JSON budget_cycles ERROR_VARIABLE budget_error GET "${runner_json}"
    time budget_cycles)
  foreach(error IN ITEMS status_error reason_error ran_error loaded_error
      semantic_error case_error termination_error expected_error cycles_error
      instructions_error budget_error)
    if(NOT "${${error}}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "diagnostics-case: runner-result-invalid-${fixture_id}")
    endif()
  endforeach()
  set(expected_sha_name "expected_sha_${fixture_id}")
  set(manifest_sha "${${expected_sha_name}}")
  if(NOT "${status}" STREQUAL "${expected_status}" OR
     NOT "${reason}" STREQUAL "${expected_reason}" OR
     NOT "${ran}" STREQUAL "ON" OR
     NOT "${loaded_sha}" STREQUAL "${manifest_sha}" OR
     NOT "${expected_sha}" STREQUAL "${manifest_sha}" OR
     NOT "${semantic}" STREQUAL "${expected_semantic}" OR
     NOT "${case_id}" STREQUAL "20481" OR
     NOT "${termination}" STREQUAL "mailbox-complete" OR
     NOT "${cycles}" MATCHES "^[0-9]+$" OR
     NOT "${instructions}" MATCHES "^[0-9]+$" OR
     NOT "${budget_cycles}" STREQUAL "1000" OR
     cycles LESS 1 OR instructions LESS 1 OR cycles GREATER budget_cycles)
    message(FATAL_ERROR
      "diagnostics-case: semantic-or-identity-mismatch-${fixture_id}: ${runner_json}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DRESULT=${result_file}"
      "-DMANIFEST=${manifest}"
      "-DREGISTRY=${registry}"
      "-DVOCAB=${vocab}"
      -P "${source_root}/cmake/CheckResult.cmake"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE result_rc OUTPUT_VARIABLE result_out ERROR_VARIABLE result_error)
  if(NOT "${result_rc}" STREQUAL "0")
    message(FATAL_ERROR
      "diagnostics-case: canonical-result-rejected-${fixture_id}: ${result_error}${result_out}")
  endif()
  message(STATUS
    "diagnostics-case: ${fixture_id} ${status} semantic=${semantic}/${case_id} sha256=${loaded_sha}")
endforeach()

message("diagnostics-case: ok (production smoke pair; exact PASS/GUEST_FAIL)")
