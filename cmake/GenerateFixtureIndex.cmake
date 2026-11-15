cmake_minimum_required(VERSION 3.25)

# Build-only metadata generator. The production index is derived solely from the
# canonical manifest. The separate control index is derived from a generated
# synthetic manifest and never grants real fixture rights.
include("${CMAKE_CURRENT_LIST_DIR}/CheckManifest.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/CheckDiagnosticAuthority.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/PathContainment.cmake")

# Explicit pre-admission projection preserves historical immutable metadata for
# host builds, but never grants runtime fixture authority. Default generation
# continues to require the current producer inputs to match every source pin.
if(NOT DEFINED GBL_PROOF_METADATA)
  set(GBL_PROOF_METADATA OFF)
endif()
if(NOT "${GBL_PROOF_METADATA}" MATCHES "^(ON|OFF)$")
  message(FATAL_ERROR "fixture-index: invalid-proof-metadata-mode")
endif()

foreach(required IN ITEMS GBL_SOURCE_DIR GBL_OUTPUT_ROOT GBL_CONTROL_IMAGE
    GBL_CONTROL_MANIFEST_OUTPUT GBL_MANIFEST GBL_REGISTRY GBL_VOCAB
    GBL_OUT_PRODUCTION GBL_OUT_CONTROL)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "fixture-index: usage: -D${required}= is required")
  endif()
endforeach()
if(NOT IS_ABSOLUTE "${GBL_OUTPUT_ROOT}")
  message(FATAL_ERROR "fixture-index: output-root-not-absolute")
endif()
get_filename_component(GBL_OUTPUT_ROOT "${GBL_OUTPUT_ROOT}" ABSOLUTE)

get_filename_component(GBL_SOURCE_DIR "${GBL_SOURCE_DIR}" ABSOLUTE)
gbl_resolve_path(source_root_real source_root_status "${GBL_SOURCE_DIR}")
if(NOT source_root_status STREQUAL "ok" OR NOT IS_DIRECTORY "${source_root_real}")
  message(FATAL_ERROR "fixture-index: source-root-${source_root_status}")
endif()
set(GBL_SOURCE_DIR "${source_root_real}")
if(DEFINED GBL_ARTIFACT_ROOT AND NOT "${GBL_ARTIFACT_ROOT}" STREQUAL "")
  if(NOT IS_ABSOLUTE "${GBL_ARTIFACT_ROOT}")
    message(FATAL_ERROR "fixture-index: artifact-root-not-absolute")
  endif()
  get_filename_component(GBL_ARTIFACT_ROOT "${GBL_ARTIFACT_ROOT}" ABSOLUTE)
  if(EXISTS "${GBL_ARTIFACT_ROOT}" AND NOT IS_DIRECTORY "${GBL_ARTIFACT_ROOT}")
    message(FATAL_ERROR "fixture-index: artifact-root-not-directory")
  endif()
endif()
get_filename_component(GBL_CONTROL_IMAGE "${GBL_CONTROL_IMAGE}" ABSOLUTE)
if(DEFINED GBL_CONTROL_FAIL_IMAGE AND NOT "${GBL_CONTROL_FAIL_IMAGE}" STREQUAL "")
  get_filename_component(GBL_CONTROL_FAIL_IMAGE "${GBL_CONTROL_FAIL_IMAGE}" ABSOLUTE)
endif()
if(DEFINED GBL_CONTROL_SKIP_IMAGE AND NOT "${GBL_CONTROL_SKIP_IMAGE}" STREQUAL "")
  get_filename_component(GBL_CONTROL_SKIP_IMAGE "${GBL_CONTROL_SKIP_IMAGE}" ABSOLUTE)
endif()
if(DEFINED GBL_CONTROL_ZERO_CASE_IMAGE AND
    NOT "${GBL_CONTROL_ZERO_CASE_IMAGE}" STREQUAL "")
  get_filename_component(GBL_CONTROL_ZERO_CASE_IMAGE
    "${GBL_CONTROL_ZERO_CASE_IMAGE}" ABSOLUTE)
endif()
foreach(register_image IN ITEMS GBL_CONTROL_REGISTER_ARM_ZERO_IMAGE
    GBL_CONTROL_REGISTER_ARM_FAIL_IMAGE GBL_CONTROL_REGISTER_ARM_STOP_IMAGE
    GBL_CONTROL_REGISTER_THUMB_ZERO_IMAGE GBL_CONTROL_REGISTER_THUMB_FAIL_IMAGE)
  if(DEFINED ${register_image} AND NOT "${${register_image}}" STREQUAL "")
    get_filename_component(${register_image} "${${register_image}}" ABSOLUTE)
  endif()
endforeach()
get_filename_component(GBL_MANIFEST "${GBL_MANIFEST}" ABSOLUTE)
get_filename_component(GBL_REGISTRY "${GBL_REGISTRY}" ABSOLUTE)
get_filename_component(GBL_VOCAB "${GBL_VOCAB}" ABSOLUTE)
get_filename_component(GBL_OUT_PRODUCTION "${GBL_OUT_PRODUCTION}" ABSOLUTE)
get_filename_component(GBL_OUT_CONTROL "${GBL_OUT_CONTROL}" ABSOLUTE)
get_filename_component(GBL_CONTROL_MANIFEST_OUTPUT
  "${GBL_CONTROL_MANIFEST_OUTPUT}" ABSOLUTE)

# Check every output before any removal, directory creation, or write. The
# caller supplies this root independently of output paths and ROM identities.
set(output_error "")
foreach(output IN ITEMS GBL_OUT_PRODUCTION GBL_OUT_CONTROL GBL_CONTROL_MANIFEST_OUTPUT)
  gbl_resolve_contained_path(output_real output_status_${output}
    "${GBL_OUTPUT_ROOT}" "${${output}}")
  if(NOT output_status_${output} STREQUAL "ok")
    set(output_error "fixture-index: output-${output_status_${output}}:${output}")
  elseif(EXISTS "${${output}}" AND IS_DIRECTORY "${${output}}")
    set(output_status_${output} "not-file")
    set(output_error "fixture-index: output-not-file:${output}")
  endif()
endforeach()
# A failed rerun clears safe stale headers; an escaping target stays untouched.
foreach(output IN ITEMS GBL_OUT_PRODUCTION GBL_OUT_CONTROL)
  if(output_status_${output} STREQUAL "ok")
    gbl_remove_file_contained(remove_status "${GBL_OUTPUT_ROOT}" "${${output}}")
    if(NOT remove_status STREQUAL "ok")
      set(output_error "fixture-index: output-${remove_status}:${output}")
    endif()
  endif()
endforeach()
if(NOT output_error STREQUAL "")
  message(FATAL_ERROR "${output_error}")
endif()
get_filename_component(production_dir "${GBL_OUT_PRODUCTION}" DIRECTORY)
get_filename_component(control_dir "${GBL_OUT_CONTROL}" DIRECTORY)
get_filename_component(control_manifest_dir
  "${GBL_CONTROL_MANIFEST_OUTPUT}" DIRECTORY)
foreach(directory IN ITEMS "${production_dir}" "${control_dir}" "${control_manifest_dir}")
  gbl_ensure_directory_contained(directory_status "${GBL_OUTPUT_ROOT}" "${directory}")
  if(NOT directory_status STREQUAL "ok")
    message(FATAL_ERROR "fixture-index: output-directory-${directory_status}")
  endif()
endforeach()

function(gbl_fixture_write_output output contents)
  gbl_write_file_contained(write_status write_reason
    "${GBL_OUTPUT_ROOT}" "${output}" "${contents}")
  if(NOT write_status STREQUAL "ok")
    message(FATAL_ERROR "fixture-index: output-${write_reason}")
  endif()
endfunction()

if(NOT DEFINED GBL_CONTROL_MANIFEST OR "${GBL_CONTROL_MANIFEST}" STREQUAL "")
  if(NOT EXISTS "${GBL_CONTROL_IMAGE}" OR IS_DIRECTORY "${GBL_CONTROL_IMAGE}")
    message(FATAL_ERROR "fixture-index: control-image-missing")
  endif()
  # These rows are artificial controls for the runner's classification and
  # refusal paths. Their generated bytes and metadata remain build-only and
  # do not grant rights to any external content.
  set(control_rows "")
  set(control_ids synthetic-control-pass synthetic-control-fail
    synthetic-control-bios-required synthetic-control-register-arm
    synthetic-control-register-thumb)
  if(DEFINED GBL_CONTROL_SKIP_IMAGE AND
      NOT "${GBL_CONTROL_SKIP_IMAGE}" STREQUAL "")
    list(APPEND control_ids synthetic-control-official-bios)
  endif()
  if(DEFINED GBL_CONTROL_REGISTER_ARM_ZERO_IMAGE AND
      DEFINED GBL_CONTROL_REGISTER_ARM_FAIL_IMAGE AND
      DEFINED GBL_CONTROL_REGISTER_ARM_STOP_IMAGE AND
      DEFINED GBL_CONTROL_REGISTER_THUMB_ZERO_IMAGE AND
      DEFINED GBL_CONTROL_REGISTER_THUMB_FAIL_IMAGE)
    list(APPEND control_ids
      synthetic-control-register-arm-zero
      synthetic-control-register-arm-fail
      synthetic-control-register-arm-stop
      synthetic-control-register-arm-never-entry
      synthetic-control-register-thumb-zero
      synthetic-control-register-thumb-fail
      synthetic-control-register-thumb-wrong-state)
  endif()
  if(DEFINED GBL_CONTROL_ZERO_CASE_IMAGE AND
      NOT "${GBL_CONTROL_ZERO_CASE_IMAGE}" STREQUAL "")
    list(APPEND control_ids synthetic-control-pass-zero-case)
  endif()
  foreach(control_id IN LISTS control_ids)
    if(control_id STREQUAL "synthetic-control-pass")
      set(control_image "${GBL_CONTROL_IMAGE}")
      set(control_source_path "generated/fixtures/owned-arm-pass.gba")
      set(control_bios "none")
    elseif(control_id STREQUAL "synthetic-control-fail")
      if(NOT DEFINED GBL_CONTROL_FAIL_IMAGE OR
          "${GBL_CONTROL_FAIL_IMAGE}" STREQUAL "")
        continue()
      endif()
      set(control_image "${GBL_CONTROL_FAIL_IMAGE}")
      set(control_source_path "generated/fixtures/owned-arm-fail.gba")
      set(control_bios "none")
    elseif(control_id STREQUAL "synthetic-control-bios-required")
      if(NOT DEFINED GBL_CONTROL_SKIP_IMAGE OR
          "${GBL_CONTROL_SKIP_IMAGE}" STREQUAL "")
        continue()
      endif()
      set(control_image "${GBL_CONTROL_SKIP_IMAGE}")
      set(control_source_path "generated/fixtures/owned-arm-pass.gba")
      set(control_bios "project-stub")
    elseif(control_id STREQUAL "synthetic-control-official-bios")
      set(control_image "${GBL_CONTROL_SKIP_IMAGE}")
      set(control_source_path "generated/fixtures/owned-arm-pass.gba")
      set(control_bios "user-official")
    elseif(control_id STREQUAL "synthetic-control-register-arm")
      set(control_image "${GBL_CONTROL_IMAGE}")
      set(control_source_path "generated/fixtures/owned-arm-pass.gba")
      set(control_bios "none")
    elseif(control_id STREQUAL "synthetic-control-register-thumb")
      set(control_image "${GBL_CONTROL_IMAGE}")
      set(control_source_path "generated/fixtures/owned-arm-pass.gba")
      set(control_bios "none")
    elseif(control_id STREQUAL "synthetic-control-register-arm-zero" OR
        control_id STREQUAL "synthetic-control-register-arm-never-entry")
      set(control_image "${GBL_CONTROL_REGISTER_ARM_ZERO_IMAGE}")
      set(control_source_path "generated/fixtures/owned-register-arm-zero.gba")
      set(control_bios "none")
    elseif(control_id STREQUAL "synthetic-control-register-arm-fail")
      set(control_image "${GBL_CONTROL_REGISTER_ARM_FAIL_IMAGE}")
      set(control_source_path "generated/fixtures/owned-register-arm-fail.gba")
      set(control_bios "none")
    elseif(control_id STREQUAL "synthetic-control-register-arm-stop")
      set(control_image "${GBL_CONTROL_REGISTER_ARM_STOP_IMAGE}")
      set(control_source_path "generated/fixtures/owned-register-arm-stop.gba")
      set(control_bios "none")
    elseif(control_id STREQUAL "synthetic-control-register-thumb-zero")
      set(control_image "${GBL_CONTROL_REGISTER_THUMB_ZERO_IMAGE}")
      set(control_source_path "generated/fixtures/owned-register-thumb-zero.gba")
      set(control_bios "none")
    elseif(control_id STREQUAL "synthetic-control-register-thumb-fail")
      set(control_image "${GBL_CONTROL_REGISTER_THUMB_FAIL_IMAGE}")
      set(control_source_path "generated/fixtures/owned-register-thumb-fail.gba")
      set(control_bios "none")
    elseif(control_id STREQUAL "synthetic-control-register-thumb-wrong-state")
      set(control_image "${GBL_CONTROL_REGISTER_ARM_ZERO_IMAGE}")
      set(control_source_path "generated/fixtures/owned-register-arm-zero.gba")
      set(control_bios "none")
    else()
      set(control_image "${GBL_CONTROL_ZERO_CASE_IMAGE}")
      set(control_source_path "generated/fixtures/owned-arm-pass-zero-case.gba")
      set(control_bios "none")
    endif()
    if(NOT EXISTS "${control_image}" OR IS_DIRECTORY "${control_image}")
      message(FATAL_ERROR "fixture-index: control-image-missing: ${control_id}")
    endif()
    # This CMake digest is independent of the generator's byte recipe.
    file(SHA256 "${control_image}" control_digest)
    if(control_id MATCHES "^synthetic-control-register-")
      set(control_source "{}")
      string(JSON control_source SET "${control_source}" kind "\"git\"")
      string(JSON control_source SET "${control_source}" url
        "\"https://github.com/jsmolka/gba-tests\"")
      string(JSON control_source SET "${control_source}" commit
        "\"a7113b67e63f83a9b321696ddd7042ccfad6c881\"")

      set(control_toolchain "{}")
      string(JSON control_toolchain SET "${control_toolchain}" identity
        "\"native C17 synthetic-control generator\"")
      string(JSON control_toolchain SET "${control_toolchain}" sha256 null)
      set(control_build "{}")
      string(JSON control_build SET "${control_build}" recipe
        "\"owned synthetic register-channel control bytes\"")
      string(JSON control_build SET "${control_build}" toolchain
        "${control_toolchain}")
      set(control_artifact "{}")
      string(JSON control_artifact SET "${control_artifact}" path
        "\"${control_source_path}\"")
      string(JSON control_artifact SET "${control_artifact}" sha256
        "\"${control_digest}\"")
      string(JSON control_build SET "${control_build}" artifact
        "${control_artifact}")

      set(control_channel "{}")
      string(JSON control_channel SET "${control_channel}" kind
        "\"register-eval\"")
      string(JSON control_channel SET "${control_channel}" source_commit
        "\"a7113b67e63f83a9b321696ddd7042ccfad6c881\"")
      if(control_id MATCHES "register-arm")
        string(JSON control_channel SET "${control_channel}" source_path
          "\"arm/arm.asm\"")
        string(JSON control_channel SET "${control_channel}" register_index 12)
        string(JSON control_channel SET "${control_channel}" instruction_set
          "\"arm\"")
        if(control_id STREQUAL "synthetic-control-register-arm")
          set(entry_address "0x08001D4C")
        elseif(control_id STREQUAL "synthetic-control-register-arm-never-entry")
          set(entry_address "0x080000C8")
        else()
          set(entry_address "0x080000C4")
        endif()
      else()
        string(JSON control_channel SET "${control_channel}" source_path
          "\"thumb/thumb.asm\"")
        string(JSON control_channel SET "${control_channel}" register_index 7)
        if(control_id STREQUAL "synthetic-control-register-thumb")
          set(entry_address "0x08000934")
        elseif(control_id STREQUAL "synthetic-control-register-thumb-wrong-state")
          set(entry_address "0x080000C4")
        else()
          set(entry_address "0x080000D2")
        endif()
        string(JSON control_channel SET "${control_channel}" instruction_set
          "\"thumb\"")
      endif()
      string(JSON control_channel SET "${control_channel}" entry_address
        "\"${entry_address}\"")
      set(control_channel_member ",\n      \"result_channel\": ${control_channel}")
    else()
      set(control_source "{}")
      string(JSON control_source SET "${control_source}" kind "\"in-tree\"")
      string(JSON control_source SET "${control_source}" path
        "\"${control_source_path}\"")
      string(JSON control_source SET "${control_source}" sha256
        "\"${control_digest}\"")
      set(control_toolchain "{}")
      string(JSON control_toolchain SET "${control_toolchain}" identity
        "\"native C17 test generator\"")
      string(JSON control_toolchain SET "${control_toolchain}" sha256 null)
      set(control_build "{}")
      string(JSON control_build SET "${control_build}" recipe
        "\"original Gamebloke instruction image generator\"")
      string(JSON control_build SET "${control_build}" toolchain
        "${control_toolchain}")
      set(control_channel_member "")
    endif()
    if(NOT "${control_rows}" STREQUAL "")
      string(APPEND control_rows ",\n")
    endif()
    string(APPEND control_rows
      "    {\n"
      "      \"id\": \"${control_id}\",\n"
      "      \"source\": ${control_source},\n"
      "      \"license\": {\"spdx\": \"LicenseRef-Gamebloke-Synthetic-Control\", \"notices\": [\"Artificial validator input for generated test bytes; no redistribution grant.\"], \"redistributable\": \"unknown\"},\n"
      "      \"build\": ${control_build},\n"
      "      \"bios_requirement\": \"${control_bios}\",\n"
      "      \"expected\": {\"provenance_class\": \"disputed\", \"dispute\": \"disp-gbatek-errata\"},\n"
      "      \"permitted_use\": [\"local-only\"]${control_channel_member}\n"
      "    }")
  endforeach()
  string(CONCAT control_manifest
    "{\n"
    "  \"schema\": \"gamebloke.manifest/1\",\n"
    "  \"note\": \"Synthetic generated contract controls only; not real fixtures or rights grants.\",\n"
    "  \"fixtures\": [\n"
    "${control_rows}\n"
    "  ]\n"
    "}\n")
  gbl_fixture_write_output("${GBL_CONTROL_MANIFEST_OUTPUT}" "${control_manifest}")
  set(control_manifest_path "${GBL_CONTROL_MANIFEST_OUTPUT}")
else()
  get_filename_component(control_manifest_path "${GBL_CONTROL_MANIFEST}" ABSOLUTE)
endif()

# Run the existing authority readers. Do not generate either header unless all
# vocabulary, registry, canonical-manifest, and synthetic-control findings are
# clear. The generated control's artificial license fields only test schema and
# dispute propagation; they do not approve any real input for redistribution.
gbl_contract_read(V "${GBL_VOCAB}")
gbl_registry_read(R "${GBL_REGISTRY}" V)
gbl_manifest_read(C "${control_manifest_path}" R V)
get_property(findings GLOBAL PROPERTY GBL_FINDINGS)
if(findings GREATER 0)
  message(FATAL_ERROR "fixture-index: validation failed (${findings} findings)")
endif()

set(control_checks
  "synthetic-control-pass|${GBL_CONTROL_IMAGE}")
if(DEFINED GBL_CONTROL_FAIL_IMAGE AND
    NOT "${GBL_CONTROL_FAIL_IMAGE}" STREQUAL "")
  list(APPEND control_checks
    "synthetic-control-fail|${GBL_CONTROL_FAIL_IMAGE}")
endif()
if(DEFINED GBL_CONTROL_SKIP_IMAGE AND
    NOT "${GBL_CONTROL_SKIP_IMAGE}" STREQUAL "")
  list(APPEND control_checks
    "synthetic-control-bios-required|${GBL_CONTROL_SKIP_IMAGE}")
endif()
if(DEFINED GBL_CONTROL_ZERO_CASE_IMAGE AND
    NOT "${GBL_CONTROL_ZERO_CASE_IMAGE}" STREQUAL "")
  list(APPEND control_checks
    "synthetic-control-pass-zero-case|${GBL_CONTROL_ZERO_CASE_IMAGE}")
endif()
list(APPEND control_checks
  "synthetic-control-register-arm|${GBL_CONTROL_IMAGE}"
  "synthetic-control-register-thumb|${GBL_CONTROL_IMAGE}")
if(DEFINED GBL_CONTROL_REGISTER_ARM_ZERO_IMAGE AND
    NOT "${GBL_CONTROL_REGISTER_ARM_ZERO_IMAGE}" STREQUAL "")
  list(APPEND control_checks
    "synthetic-control-register-arm-zero|${GBL_CONTROL_REGISTER_ARM_ZERO_IMAGE}"
    "synthetic-control-register-arm-fail|${GBL_CONTROL_REGISTER_ARM_FAIL_IMAGE}"
    "synthetic-control-register-arm-stop|${GBL_CONTROL_REGISTER_ARM_STOP_IMAGE}"
    "synthetic-control-register-arm-never-entry|${GBL_CONTROL_REGISTER_ARM_ZERO_IMAGE}"
    "synthetic-control-register-thumb-zero|${GBL_CONTROL_REGISTER_THUMB_ZERO_IMAGE}"
    "synthetic-control-register-thumb-fail|${GBL_CONTROL_REGISTER_THUMB_FAIL_IMAGE}"
    "synthetic-control-register-thumb-wrong-state|${GBL_CONTROL_REGISTER_ARM_ZERO_IMAGE}")
endif()
foreach(control_check IN LISTS control_checks)
  string(REPLACE "|" ";" control_pair "${control_check}")
  list(GET control_pair 0 control_id)
  list(GET control_pair 1 control_image)
  if(NOT EXISTS "${control_image}" OR IS_DIRECTORY "${control_image}")
    gbl_finding(fixture-index control-image-missing "${control_id}")
  else()
    file(SHA256 "${control_image}" control_actual_digest)
    if(NOT control_id IN_LIST C_IDS)
      gbl_finding(fixture-index control-row-missing "${control_id}")
    else()
      set(control_expected_digest "${C_ROW_${control_id}_SHA256}")
      if(NOT "${control_expected_digest}" STREQUAL "${control_actual_digest}")
        gbl_finding(fixture-index digest-mismatch "${control_id}")
      endif()
    endif()
  endif()
endforeach()

# A known invalid synthetic image cannot produce an index. Refuse it before
# loading the large independent production authority; every successful path
# still runs the identical canonical validator before any header write.
get_property(findings GLOBAL PROPERTY GBL_FINDINGS)
if(findings GREATER 0)
  message(FATAL_ERROR "fixture-index: validation failed (${findings} findings)")
endif()
gbl_manifest_read(M "${GBL_MANIFEST}" R V)
get_property(findings GLOBAL PROPERTY GBL_FINDINGS)
if(findings GREATER 0)
  message(FATAL_ERROR "fixture-index: validation failed (${findings} findings)")
endif()

# Admitted builds may prepare their exact artifacts from these current parsed
# rows before the existing artifact/source identity checks below. The hook
# cannot replace any reader, validation or render step and is never serialized.
if(COMMAND gbl_fixture_prepare_admitted_artifacts)
  gbl_fixture_prepare_admitted_artifacts()
endif()

# For canonical in-tree rows, verify that the manifest digest still identifies
# bytes at its validated source path. For git artifacts, validate a present file
# against the committed output pin when this lane supplies its explicit build root.
# A missing root/output is allowed for metadata-only lanes; the runtime runner
# still compares every executed ROM against the immutable digest in the index.
function(gbl_fixture_resolve_path out path)
  # REAL_PATH warns on absent outputs in newer CMake. Resolve the closest
  # existing ancestor, retaining symlink containment checks for missing leaves.
  set(ancestor "${path}")
  while(NOT EXISTS "${ancestor}" AND NOT IS_SYMLINK "${ancestor}")
    get_filename_component(parent "${ancestor}" DIRECTORY)
    if(parent STREQUAL ancestor)
      message(FATAL_ERROR "fixture-index: artifact-path-unresolvable")
    endif()
    set(ancestor "${parent}")
  endwhile()
  if(IS_SYMLINK "${ancestor}" AND NOT EXISTS "${ancestor}")
    message(FATAL_ERROR "fixture-index: artifact-path-escape: dangling symlink")
  endif()
  file(REAL_PATH "${ancestor}" resolved_ancestor)
  file(RELATIVE_PATH suffix "${ancestor}" "${path}")
  if(suffix STREQUAL "")
    set(resolved "${resolved_ancestor}")
  else()
    set(resolved "${resolved_ancestor}/${suffix}")
  endif()
  cmake_path(NORMAL_PATH resolved)
  set(${out} "${resolved}" PARENT_SCOPE)
endfunction()
if(M_COUNT GREATER 0)
  math(EXPR production_last "${M_COUNT}-1")
  foreach(index RANGE ${production_last})
    list(GET M_IDS ${index} row_id)
    set(row_doc "${M_ROW_${row_id}_JSON}")
    if("${M_ROW_${row_id}_URL}" STREQUAL "https://github.com/szTheory/gamebloke" AND
       "${M_ROW_${row_id}_ARTIFACT_PATH}" MATCHES "^generated/diagnostics/armv4t/")
      set(revision "${M_ROW_${row_id}_COMMIT}")
      if(NOT revision IN_LIST checked_diagnostic_revisions)
        if(NOT GBL_PROOF_METADATA)
          gbl_check_diagnostic_authority("${GBL_SOURCE_DIR}" "${revision}")
        endif()
        list(APPEND checked_diagnostic_revisions "${revision}")
      endif()
    endif()
    gbl_json_string(source_kind "${row_doc}" source kind)
    if("${source_kind}" STREQUAL "in-tree")
      gbl_json_string(relative_path "${row_doc}"
        source path)
      gbl_json_string(expected_digest "${row_doc}"
        source sha256)
      gbl__manifest_path_ok(source_path_ok "${relative_path}")
      if(NOT source_path_ok)
        # CheckManifest.cmake already recorded the malformed manifest path.
        # Do not resolve it or let it influence a filesystem read.
      else()
        set(source_path "${GBL_SOURCE_DIR}/${relative_path}")
        gbl_resolve_contained_path(source_real source_status
          "${GBL_SOURCE_DIR}" "${source_path}")
      endif()
      if(NOT source_path_ok)
        # The manifest finding is authoritative for spelling errors.
      elseif(source_status STREQUAL "path-escape")
        gbl_finding(fixture-index in-tree-path-escape "${row_id}")
      elseif(source_status STREQUAL "dangling-symlink")
        gbl_finding(fixture-index in-tree-path-dangling-symlink "${row_id}")
      elseif(NOT source_status STREQUAL "ok")
        gbl_finding(fixture-index in-tree-path-unresolvable "${row_id}")
      elseif(NOT EXISTS "${source_real}" OR IS_DIRECTORY "${source_real}")
        gbl_finding(fixture-index in-tree-file-missing "${row_id}")
      else()
        file(SHA256 "${source_real}" actual_digest)
        if(NOT "${expected_digest}" STREQUAL "${actual_digest}")
          gbl_finding(fixture-index digest-mismatch "${row_id}")
        endif()
      endif()
    elseif("${source_kind}" STREQUAL "git" AND
        "${M_ROW_${row_id}_ARTIFACT_OK}" STREQUAL "TRUE" AND
        DEFINED GBL_ARTIFACT_ROOT AND NOT "${GBL_ARTIFACT_ROOT}" STREQUAL "")
      set(relative_artifact "${M_ROW_${row_id}_ARTIFACT_PATH}")
      set(expected_artifact_digest "${M_ROW_${row_id}_ARTIFACT_SHA256}")
      set(artifact_path "${GBL_ARTIFACT_ROOT}/${relative_artifact}")
      gbl_fixture_resolve_path(artifact_root_real "${GBL_ARTIFACT_ROOT}")
      gbl_fixture_resolve_path(artifact_real "${artifact_path}")
      if("${artifact_root_real}" STREQUAL "/")
        set(artifact_root_prefix "/")
      else()
        set(artifact_root_prefix "${artifact_root_real}/")
      endif()
      string(FIND "${artifact_real}" "${artifact_root_prefix}" artifact_root_at)
      if(NOT artifact_root_at EQUAL 0)
        gbl_finding(fixture-index artifact-path-escape "${row_id}")
      elseif(EXISTS "${artifact_path}" AND IS_DIRECTORY "${artifact_path}")
        gbl_finding(fixture-index artifact-output-not-file "${row_id}")
      elseif(EXISTS "${artifact_path}")
        file(SHA256 "${artifact_path}" actual_artifact_digest)
        if(NOT "${expected_artifact_digest}" STREQUAL "${actual_artifact_digest}")
          gbl_finding(fixture-index artifact-digest-mismatch "${row_id}")
        endif()
      endif()
    endif()
  endforeach()
endif()

get_property(findings GLOBAL PROPERTY GBL_FINDINGS)
if(findings GREATER 0)
  message(FATAL_ERROR "fixture-index: identity validation failed (${findings} findings)")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/FixtureIndexRender.cmake")
function(gbl_fixture_emit_index output prefix)
  gbl_fixture_render_index(contents "${prefix}")
  gbl_fixture_write_output("${output}" "${contents}")
endfunction()

# Project the exact row objects parsed by the canonical validator above.
gbl_fixture_emit_index("${GBL_OUT_PRODUCTION}" M)
gbl_fixture_emit_index("${GBL_OUT_CONTROL}" C)
message("fixture-index: ok (production ${M_COUNT} rows; synthetic control ${C_COUNT} rows)")
