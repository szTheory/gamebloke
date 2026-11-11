cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS CASE SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "artifact-case: usage: -D${required}= is required")
  endif()
endforeach()
if(NOT CASE STREQUAL "roundtrip")
  message(FATAL_ERROR "artifact-case: unknown case: ${CASE}")
endif()

include("${SRC}/tests/gates/json_mutate.cmake")

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
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DGBL_SOURCE_DIR=${SRC}"
    "-DGBL_ARTIFACT_ROOT=${dir}/build-root"
    "-DGBL_CONTROL_IMAGE=${control_image}"
    "-DGBL_CONTROL_MANIFEST_OUTPUT=${control_manifest}"
    "-DGBL_MANIFEST=${manifest}"
    "-DGBL_REGISTRY=${SRC}/registry/disputes.json"
    "-DGBL_VOCAB=${SRC}/contracts/evidence.json"
    "-DGBL_OUT_PRODUCTION=${production_index}"
    "-DGBL_OUT_CONTROL=${control_index}"
    -P "${SRC}/cmake/GenerateFixtureIndex.cmake"
  RESULT_VARIABLE generator_result
  OUTPUT_VARIABLE generator_stdout
  ERROR_VARIABLE generator_stderr)
if(NOT "${generator_result}" STREQUAL "0")
  message(FATAL_ERROR
    "artifact-case: fixture-index-generation-failed\n${generator_stdout}${generator_stderr}")
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

message("artifact-case: roundtrip source commit, toolchain and artifact identities remain separate")
