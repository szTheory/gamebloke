cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS CASE SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "artifact-case: usage: -D${required}= is required")
  endif()
endforeach()
if(CASE STREQUAL "admission-controls")
  include("${SRC}/tests/gates/diagnostic_admission_controls.cmake")
  return()
endif()
if(NOT CASE STREQUAL "roundtrip" AND
    NOT CASE STREQUAL "compatibility-and-controls")
  message(FATAL_ERROR "artifact-case: unknown case: ${CASE}")
endif()

include("${SRC}/tests/gates/json_mutate.cmake")

function(run_generator out_result out_report manifest artifact_root production_index
    control_index control_manifest)
  set(generator_source_root "${SRC}")
  if(ARGC GREATER 7)
    set(generator_source_root "${ARGV7}")
  endif()
  set(generator_output_root "${BIN}")
  if(ARGC GREATER 8)
    set(generator_output_root "${ARGV8}")
  endif()
  set(command "${CMAKE_COMMAND}"
    "-DGBL_SOURCE_DIR=${generator_source_root}"
    "-DGBL_OUTPUT_ROOT=${generator_output_root}"
    "-DGBL_CONTROL_IMAGE=${control_image}"
    "-DGBL_CONTROL_MANIFEST_OUTPUT=${control_manifest}"
    "-DGBL_MANIFEST=${manifest}"
    "-DGBL_REGISTRY=${SRC}/registry/disputes.json"
    "-DGBL_VOCAB=${SRC}/contracts/evidence.json"
    "-DGBL_OUT_PRODUCTION=${production_index}"
    "-DGBL_OUT_CONTROL=${control_index}")
  if(NOT "${artifact_root}" STREQUAL "")
    if(IS_ABSOLUTE "${artifact_root}")
      list(APPEND command "-DGBL_ARTIFACT_ROOT=${artifact_root}")
    else()
      get_filename_component(absolute_root "${artifact_root}" ABSOLUTE)
      list(APPEND command "-DGBL_ARTIFACT_ROOT=${absolute_root}")
    endif()
  endif()
  list(APPEND command -P "${SRC}/cmake/GenerateFixtureIndex.cmake")
  execute_process(COMMAND ${command} RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  set(${out_result} "${result}" PARENT_SCOPE)
  set(${out_report} "${stdout}${stderr}" PARENT_SCOPE)
endfunction()

function(run_companion_case script selector out_result out_report)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DCASE=${selector}" "-DSRC=${SRC}"
      "-DBIN=${dir}/companion-tests" -P "${SRC}/${script}"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  set(${out_result} "${result}" PARENT_SCOPE)
  set(${out_report} "${stdout}${stderr}" PARENT_SCOPE)
endfunction()

get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)
set(dir "${BIN}/artifact-case/${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}/build-root/diagnostics")

# This file is synthetic metadata evidence, not a GBA diagnostic or admitted ROM.
set(artifact_path "diagnostics/original.gba")
set(artifact_file "${dir}/build-root/${artifact_path}")
file(WRITE "${artifact_file}" "synthetic artifact identity test bytes\n")
file(SHA256 "${artifact_file}" artifact_digest)
set(control_image "${dir}/synthetic-control.gba")
file(WRITE "${control_image}" "separate synthetic control bytes\n")

set(manifest "${dir}/manifest.json")
file(COPY_FILE "${SRC}/contracts/examples/manifest.json" "${manifest}")
string(JSON artifact SET "{}" path "\"${artifact_path}\"")
string(JSON artifact SET "${artifact}" sha256 "\"${artifact_digest}\"")
gbl_json_mutate_set("${manifest}" "${artifact}" fixtures 0 build artifact)

set(production_index "${dir}/generated/production/fixture_index.h")
set(control_index "${dir}/generated/control/fixture_index.h")
set(control_manifest "${dir}/generated/control/manifest.json")
run_generator(generator_result generator_report "${manifest}" "${dir}/build-root"
  "${production_index}" "${control_index}" "${control_manifest}")
if(NOT "${generator_result}" STREQUAL "0")
  message(FATAL_ERROR "artifact-case: fixture-index-generation-failed\n${generator_report}")
endif()
if(NOT EXISTS "${production_index}" OR NOT EXISTS "${control_index}")
  message(FATAL_ERROR "artifact-case: fixture-index-missing")
endif()

file(READ "${production_index}" production_text)
if(NOT "${production_text}" MATCHES "${artifact_digest}")
  message(FATAL_ERROR "artifact-case: artifact-digest-not-projected")
endif()
if("${production_text}" MATCHES "0123456789abcdef0123456789abcdef01234567" OR
    "${production_text}" MATCHES "prebuilt-upstream")
  message(FATAL_ERROR "artifact-case: source-or-tool-identity-projected-as-rom-digest")
endif()

set(result "${dir}/result.json")
file(COPY_FILE "${SRC}/contracts/examples/result-pass.json" "${result}")
gbl_json_mutate_set("${result}" "\"${artifact_digest}\"" fixture rom_sha256)
gbl_json_mutate_set("${result}" "\"${artifact_digest}\"" claim cart rom_sha256)
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DRESULT=${result}"
    "-DMANIFEST=${manifest}"
    "-DREGISTRY=${SRC}/registry/disputes.json"
    "-DVOCAB=${SRC}/contracts/evidence.json"
    -P "${SRC}/cmake/CheckResult.cmake"
  RESULT_VARIABLE result_check
  OUTPUT_VARIABLE result_stdout
  ERROR_VARIABLE result_stderr)
if(NOT "${result_check}" STREQUAL "0" OR
    NOT "${result_stdout}${result_stderr}" MATCHES "result: ok \\(PASS\\)")
  message(FATAL_ERROR
    "artifact-case: canonical-result-check-failed\n${result_stdout}${result_stderr}")
endif()

message("artifact-case: roundtrip source commit, toolchain and artifact identities remain separate")
message("artifact-case: canonical result checker accepted the exact indexed artifact digest")

if(CASE STREQUAL "compatibility-and-controls")
  # Each generated output has its own path boundary, independent of the ROM
  # artifact root. Refusal must precede removal of an outside stale sentinel.
  foreach(role IN ITEMS production control manifest)
    foreach(kind IN ITEMS parent final dangling)
      set(output_root "${dir}/output-${role}-${kind}/root")
      set(outside "${dir}/output-${role}-${kind}/outside")
      file(MAKE_DIRECTORY "${output_root}/production" "${output_root}/control"
        "${output_root}/manifest" "${outside}")
      set(output_production "${output_root}/production/index.h")
      set(output_control "${output_root}/control/index.h")
      set(output_manifest "${output_root}/manifest/control.json")
      set(hostile "${output_${role}}")
      get_filename_component(hostile_parent "${hostile}" DIRECTORY)
      get_filename_component(hostile_name "${hostile}" NAME)
      set(sentinel "${outside}/${hostile_name}")
      if(kind STREQUAL "parent")
        file(REMOVE_RECURSE "${hostile_parent}")
        set(link_target "${outside}")
        set(link_path "${hostile_parent}")
      else()
        set(link_target "${sentinel}")
        set(link_path "${hostile}")
      endif()
      if(NOT kind STREQUAL "dangling")
        file(WRITE "${sentinel}" "synthetic outside output sentinel\n")
        file(SHA256 "${sentinel}" sentinel_before)
      endif()
      file(CREATE_LINK "${link_target}" "${link_path}" SYMBOLIC RESULT link_rc)
      if(NOT link_rc STREQUAL "0")
        message(FATAL_ERROR "artifact-case: output-symlink-control-setup-failed")
      endif()
      run_generator(output_rc output_report "${manifest}" "${dir}/build-root"
        "${output_production}" "${output_control}" "${output_manifest}"
        "${SRC}" "${output_root}")
      if(output_rc STREQUAL "0" OR NOT output_report MATCHES
          "fixture-index: output-(path-escape|dangling-symlink)")
        message(FATAL_ERROR "artifact-case: output-symlink-was-not-refused:${role}:${kind}:${output_report}")
      endif()
      if(kind STREQUAL "dangling")
        if(EXISTS "${sentinel}")
          message(FATAL_ERROR "artifact-case: dangling-output-created-outside-file")
        endif()
      else()
        file(SHA256 "${sentinel}" sentinel_after)
        if(NOT sentinel_after STREQUAL sentinel_before)
          message(FATAL_ERROR "artifact-case: output-symlink-changed-outside-sentinel")
        endif()
      endif()
    endforeach()
  endforeach()
  run_generator(clean_output_rc clean_output_report "${manifest}" "${dir}/build-root"
    "${dir}/output-clean/production/index.h" "${dir}/output-clean/control/index.h"
    "${dir}/output-clean/manifest/control.json" "${SRC}" "${dir}/output-clean")
  if(NOT clean_output_rc STREQUAL "0")
    message(FATAL_ERROR "artifact-case: contained-output-clean-companion-failed:${clean_output_report}")
  endif()

  # Legacy git metadata must still generate an empty runtime digest.
  set(legacy_manifest "${dir}/legacy-manifest.json")
  file(COPY_FILE "${SRC}/contracts/examples/manifest.json" "${legacy_manifest}")
  set(legacy_production "${dir}/legacy-generated/production/fixture_index.h")
  set(legacy_control "${dir}/legacy-generated/control/fixture_index.h")
  set(legacy_control_manifest "${dir}/legacy-generated/control/manifest.json")
  run_generator(legacy_result legacy_report "${legacy_manifest}" ""
    "${legacy_production}" "${legacy_control}" "${legacy_control_manifest}")
  if(NOT "${legacy_result}" STREQUAL "0")
    message(FATAL_ERROR "artifact-case: legacy-git-index-failed\n${legacy_report}")
  endif()
  file(READ "${legacy_production}" legacy_text)
  if(NOT "${legacy_text}" MATCHES
      "\"example-row\", \"\", \"suite-self-checking\"")
    message(FATAL_ERROR "artifact-case: legacy-git-runtime-digest-was-not-empty")
  endif()

  # Metadata-only lanes remain configurable with the pinned output absent,
  # including when they provide an explicit per-lane build root.
  file(REMOVE "${artifact_file}")
  set(absent_root_prod "${dir}/absent-root-generated/production/fixture_index.h")
  set(absent_root_control "${dir}/absent-root-generated/control/fixture_index.h")
  set(absent_root_manifest "${dir}/absent-root-generated/control/manifest.json")
  run_generator(absent_root_result absent_root_report "${manifest}" "${dir}/build-root"
    "${absent_root_prod}" "${absent_root_control}" "${absent_root_manifest}")
  if(NOT "${absent_root_result}" STREQUAL "0" OR
      NOT EXISTS "${absent_root_prod}")
    message(FATAL_ERROR
      "artifact-case: absent-output-with-root-rejected\n${absent_root_report}")
  endif()
  file(READ "${absent_root_prod}" absent_root_text)
  if(NOT "${absent_root_text}" MATCHES "${artifact_digest}")
    message(FATAL_ERROR "artifact-case: absent-output-lost-committed-pin")
  endif()

  set(absent_no_root_prod "${dir}/absent-no-root-generated/production/fixture_index.h")
  set(absent_no_root_control "${dir}/absent-no-root-generated/control/fixture_index.h")
  set(absent_no_root_manifest "${dir}/absent-no-root-generated/control/manifest.json")
  run_generator(absent_no_root_result absent_no_root_report "${manifest}" ""
    "${absent_no_root_prod}" "${absent_no_root_control}" "${absent_no_root_manifest}")
  if(NOT "${absent_no_root_result}" STREQUAL "0" OR
      NOT EXISTS "${absent_no_root_prod}")
    message(FATAL_ERROR
      "artifact-case: absent-output-without-root-rejected\n${absent_no_root_report}")
  endif()
  file(WRITE "${artifact_file}" "synthetic artifact identity test bytes\n")

  # A present output with changed bytes fails, and the generator removes any
  # old headers before returning so stale identity cannot survive a refusal.
  file(WRITE "${artifact_file}" "stale output bytes\n")
  set(mismatch_prod "${dir}/mismatch-generated/production/fixture_index.h")
  set(mismatch_control "${dir}/mismatch-generated/control/fixture_index.h")
  set(mismatch_manifest "${dir}/mismatch-generated/control/manifest.json")
  file(MAKE_DIRECTORY "${dir}/mismatch-generated/production"
    "${dir}/mismatch-generated/control")
  file(WRITE "${mismatch_prod}" "stale production index")
  file(WRITE "${mismatch_control}" "stale control index")
  run_generator(mismatch_result mismatch_report "${manifest}" "${dir}/build-root"
    "${mismatch_prod}" "${mismatch_control}" "${mismatch_manifest}")
  if("${mismatch_result}" STREQUAL "0" OR
      NOT "${mismatch_report}" MATCHES
        "fixture-index: artifact-digest-mismatch: example-row" OR
      EXISTS "${mismatch_prod}" OR EXISTS "${mismatch_control}")
    message(FATAL_ERROR "artifact-case: changed-output-or-stale-index-accepted\n${mismatch_report}")
  endif()
  file(WRITE "${artifact_file}" "synthetic artifact identity test bytes\n")

  # The source revision is separately checked against its pinned source URL.
  set(source_manifest "${dir}/source-bound-manifest.json")
  file(COPY_FILE "${manifest}" "${source_manifest}")
  set(source_dest "phase04/source.S")
  string(JSON fetch SET "{}" pin_dest "\"${source_dest}\"")
  gbl_json_mutate_set("${source_manifest}" "${fetch}" fixtures 0 fetch)
  set(source_commit "0123456789abcdef0123456789abcdef01234567")
  set(source_pin "${dir}/source-pins.txt")
  file(COPY_FILE "${SRC}/pins.txt" "${source_pin}")
  file(APPEND "${source_pin}"
    "${artifact_digest} https://example.invalid/example-suite/raw/${source_commit}/diagnostics.S ${source_dest}\n")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DMODE=all
      "-DMANIFEST=${source_manifest}" "-DPINS=${source_pin}"
      "-DREGISTRY=${SRC}/registry/disputes.json"
      "-DVOCAB=${SRC}/contracts/evidence.json"
      -P "${SRC}/cmake/CheckManifest.cmake"
    RESULT_VARIABLE source_match_result
    OUTPUT_VARIABLE source_match_report ERROR_VARIABLE source_match_error)
  if(NOT "${source_match_result}" STREQUAL "0" OR
      NOT "${source_match_report}${source_match_error}" MATCHES
        "manifest: ok \\(1 rows\\)")
    message(FATAL_ERROR "artifact-case: clean-source-pin-rejected\n${source_match_report}${source_match_error}")
  endif()
  string(REPEAT "e" 40 changed_commit)
  gbl_json_mutate_set("${source_manifest}" "\"${changed_commit}\""
    fixtures 0 source commit)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DMODE=all
      "-DMANIFEST=${source_manifest}" "-DPINS=${source_pin}"
      "-DREGISTRY=${SRC}/registry/disputes.json"
      "-DVOCAB=${SRC}/contracts/evidence.json"
      -P "${SRC}/cmake/CheckManifest.cmake"
    RESULT_VARIABLE source_stale_result
    OUTPUT_VARIABLE source_stale_report ERROR_VARIABLE source_stale_error)
  if("${source_stale_result}" STREQUAL "0" OR
      NOT "${source_stale_report}${source_stale_error}" MATCHES
        "manifest: pin-mismatch: example-row: pinned URL for ${source_dest} lacks the commit")
    message(FATAL_ERROR "artifact-case: stale-source-pin-accepted\n${source_stale_report}${source_stale_error}")
  endif()

  # A symlink that resolves outside the explicit per-lane root is refused.
  set(outside_file "${dir}/outside.gba")
  file(WRITE "${outside_file}" "synthetic external bytes\n")
  file(CREATE_LINK "${outside_file}" "${dir}/build-root/diagnostics/escape.gba"
    SYMBOLIC RESULT link_result)
  if(NOT "${link_result}" STREQUAL "0")
    message(FATAL_ERROR "artifact-case: symlink-control-could-not-be-created")
  endif()
  file(SHA256 "${outside_file}" outside_digest)
  set(escape_manifest "${dir}/escape-manifest.json")
  file(COPY_FILE "${manifest}" "${escape_manifest}")
  gbl_json_mutate_set("${escape_manifest}" "\"diagnostics/escape.gba\""
    fixtures 0 build artifact path)
  gbl_json_mutate_set("${escape_manifest}" "\"${outside_digest}\""
    fixtures 0 build artifact sha256)
  set(escape_prod "${dir}/escape-generated/production/fixture_index.h")
  set(escape_control "${dir}/escape-generated/control/fixture_index.h")
  set(escape_control_manifest "${dir}/escape-generated/control/manifest.json")
  run_generator(escape_result escape_report "${escape_manifest}" "${dir}/build-root"
    "${escape_prod}" "${escape_control}" "${escape_control_manifest}")
  if("${escape_result}" STREQUAL "0" OR
      NOT "${escape_report}" MATCHES "fixture-index: artifact-path-escape: example-row")
    message(FATAL_ERROR "artifact-case: symlink-escape-accepted\n${escape_report}")
  endif()

  # An in-tree manifest path may use a contained symlink alias, but its
  # resolved bytes must remain inside the declared source root.
  set(in_tree_manifest "${dir}/in-tree-manifest.json")
  file(READ "${manifest}" in_tree_doc)
  string(JSON in_tree_row GET "${in_tree_doc}" fixtures 0)
  string(JSON in_tree_row REMOVE "${in_tree_row}" build artifact)
  set(in_tree_source "{}")
  string(JSON in_tree_source SET "${in_tree_source}" kind "\"in-tree\"")
  string(JSON in_tree_source SET "${in_tree_source}" path "\"fixtures/diagnostic.gba\"")
  file(MAKE_DIRECTORY "${dir}/in-tree-source" "${dir}/in-tree-external")
  set(in_tree_external_file "${dir}/in-tree-external/diagnostic.gba")
  file(WRITE "${in_tree_external_file}" "synthetic external in-tree identity bytes\n")
  file(SHA256 "${in_tree_external_file}" in_tree_external_digest)
  string(JSON in_tree_source SET "${in_tree_source}" sha256 "\"${in_tree_external_digest}\"")
  string(JSON in_tree_row SET "${in_tree_row}" source "${in_tree_source}")
  string(JSON in_tree_doc SET "${in_tree_doc}" fixtures "[${in_tree_row}]")
  file(WRITE "${in_tree_manifest}" "${in_tree_doc}\n")
  file(CREATE_LINK "${dir}/in-tree-external" "${dir}/in-tree-source/fixtures"
    SYMBOLIC RESULT in_tree_link)
  if(NOT in_tree_link STREQUAL "0")
    message(FATAL_ERROR "artifact-case: in-tree-symlink-control-could-not-be-created:${in_tree_link}")
  endif()
  set(in_tree_prod "${dir}/in-tree-generated/production/fixture_index.h")
  set(in_tree_control "${dir}/in-tree-generated/control/fixture_index.h")
  set(in_tree_control_manifest "${dir}/in-tree-generated/control/manifest.json")
  get_filename_component(in_tree_prod_dir "${in_tree_prod}" DIRECTORY)
  get_filename_component(in_tree_control_dir "${in_tree_control}" DIRECTORY)
  file(MAKE_DIRECTORY "${in_tree_prod_dir}" "${in_tree_control_dir}")
  file(WRITE "${in_tree_prod}" "stale in-tree production index\n")
  file(WRITE "${in_tree_control}" "stale in-tree control index\n")
  file(SHA256 "${in_tree_external_file}" in_tree_before)
  run_generator(in_tree_result in_tree_report "${in_tree_manifest}" ""
    "${in_tree_prod}" "${in_tree_control}" "${in_tree_control_manifest}"
    "${dir}/in-tree-source")
  file(SHA256 "${in_tree_external_file}" in_tree_after)
  if("${in_tree_result}" STREQUAL "0" OR
      NOT "${in_tree_report}" MATCHES
        "fixture-index: in-tree-path-escape: example-row" OR
      EXISTS "${in_tree_prod}" OR EXISTS "${in_tree_control}" OR
      NOT in_tree_before STREQUAL in_tree_after)
    message(FATAL_ERROR "artifact-case: in-tree-symlink-escape-accepted-or-stale-index-kept\n${in_tree_report}")
  endif()

  # A contained alias has the same contract: the pin is the bytes resolved
  # beneath the repository root, so this clean companion remains admissible.
  set(contained_source_root "${dir}/in-tree-contained")
  file(MAKE_DIRECTORY "${contained_source_root}/fixtures" "${contained_source_root}/owned")
  set(contained_target "${contained_source_root}/owned/diagnostic.gba")
  file(WRITE "${contained_target}" "synthetic contained in-tree identity bytes\n")
  file(SHA256 "${contained_target}" contained_digest)
  file(CREATE_LINK "../owned/diagnostic.gba"
    "${contained_source_root}/fixtures/contained-alias.gba"
    SYMBOLIC RESULT contained_link)
  if(NOT contained_link STREQUAL "0")
    message(FATAL_ERROR "artifact-case: contained-alias-control-could-not-be-created:${contained_link}")
  endif()
  string(JSON contained_source SET "${in_tree_source}" path
    "\"fixtures/contained-alias.gba\"")
  string(JSON contained_source SET "${contained_source}" sha256 "\"${contained_digest}\"")
  string(JSON contained_row SET "${in_tree_row}" source "${contained_source}")
  string(JSON contained_doc SET "${in_tree_doc}" fixtures "[${contained_row}]")
  set(contained_manifest "${dir}/contained-in-tree-manifest.json")
  file(WRITE "${contained_manifest}" "${contained_doc}\n")
  set(contained_prod "${dir}/contained-generated/production/fixture_index.h")
  set(contained_control "${dir}/contained-generated/control/fixture_index.h")
  set(contained_control_manifest "${dir}/contained-generated/control/manifest.json")
  run_generator(contained_result contained_report "${contained_manifest}" ""
    "${contained_prod}" "${contained_control}" "${contained_control_manifest}"
    "${contained_source_root}")
  if(NOT "${contained_result}" STREQUAL "0" OR
      NOT EXISTS "${contained_prod}" OR NOT EXISTS "${contained_control}")
    message(FATAL_ERROR "artifact-case: contained-in-tree-alias-rejected\n${contained_report}")
  endif()

  run_companion_case("tests/gates/manifest_case.cmake" artifact_shape
    manifest_control_result manifest_control_report)
  if(NOT "${manifest_control_result}" STREQUAL "0")
    message(FATAL_ERROR "artifact-case: manifest-shape-controls-failed\n${manifest_control_report}")
  endif()
  run_companion_case("tests/gates/result_case.cmake" artifact_digest
    result_control_result result_control_report)
  if(NOT "${result_control_result}" STREQUAL "0")
    message(FATAL_ERROR "artifact-case: result-binding-controls-failed\n${result_control_report}")
  endif()

  run_companion_case("tests/gates/diagnostic_admission_controls.cmake" path-controls
    admission_path_result admission_path_report)
  if(NOT "${admission_path_result}" STREQUAL "0")
    message(FATAL_ERROR "artifact-case: admission-path-controls-failed\n${admission_path_report}")
  endif()

  message("artifact-case: legacy git digest remains empty; absent output uses metadata pin")
  message("artifact-case: changed source/output/result bindings, symlink escape and stale-index cleanup refused")
  message("artifact-case: manifest shape and result digest controls passed")
endif()
