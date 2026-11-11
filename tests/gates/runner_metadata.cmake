cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "runner-metadata: usage: -D${required}= is required")
  endif()
endforeach()
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)

set(generated "${BIN}/generated")
set(production_index "${generated}/production/fixture_index.h")
set(control_index "${generated}/control/fixture_index.h")
set(control_manifest "${generated}/control/manifest.json")
set(pass_image "${generated}/fixtures/owned-arm-pass.gba")
set(mutated_image "${generated}/fixtures/owned-arm-pass-mutated.gba")
set(wiring_file "${generated}/runner-target-wiring.txt")
set(generator_script "${SRC}/cmake/GenerateFixtureIndex.cmake")

foreach(path IN ITEMS "${production_index}" "${control_index}"
    "${control_manifest}" "${pass_image}" "${mutated_image}" "${wiring_file}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "runner-metadata: required generated input is missing")
  endif()
endforeach()

file(READ "${production_index}" production_text)
file(READ "${control_index}" control_text)
file(READ "${control_manifest}" control_manifest_text)
if(NOT "${production_text}" MATCHES
    "gbl_fixture_metadata_count = 0u;" OR
    "${production_text}" MATCHES "synthetic-control-pass")
  message(FATAL_ERROR "runner-metadata: production-index-not-empty-authority")
endif()
if(NOT "${control_text}" MATCHES "synthetic-control-pass" OR
    NOT "${control_text}" MATCHES "rom_sha256" OR
    NOT "${control_text}" MATCHES "disp-gbatek-errata" OR
    NOT "${control_text}" MATCHES "five-word-v1-final-marker" OR
    NOT "${control_manifest_text}" MATCHES "LicenseRef-Gamebloke-Synthetic-Control" OR
    NOT "${control_manifest_text}" MATCHES "redistributable\\\": \\\"unknown")
  message(FATAL_ERROR "runner-metadata: synthetic-control-row-incomplete")
endif()
if("${control_manifest_text}" MATCHES "public-ci" OR
    NOT "${control_manifest_text}" MATCHES "local-only" OR
    NOT "${control_manifest_text}" MATCHES "no redistribution grant")
  message(FATAL_ERROR "runner-metadata: synthetic-control-authority-overstated")
endif()

# The independently computed byte digest in CMake must be present in the
# synthetic immutable index, and the generated one-byte variant must not match.
file(SHA256 "${pass_image}" pass_digest)
file(SHA256 "${mutated_image}" mutated_digest)
if("${pass_digest}" STREQUAL "${mutated_digest}" OR
    NOT "${control_text}" MATCHES "${pass_digest}")
  message(FATAL_ERROR "runner-metadata: stale-generated-image-digest-accepted")
endif()

# Production and control targets share their actual source, option, definition,
# and core/warning wiring. Only their private generated-index include paths vary.
file(READ "${wiring_file}" wiring)
set(expected_runner_sources
  "runner/main.c;runner/run.c;runner/result.c;runner/sha256.c")
if(NOT "${wiring}" MATCHES "production.SOURCES=${expected_runner_sources}" OR
    NOT "${wiring}" MATCHES "control.SOURCES=${expected_runner_sources}" OR
    NOT "${wiring}" MATCHES "production.LINK_LIBRARIES=gamebloke_core;gbl_warnings" OR
    NOT "${wiring}" MATCHES "control.LINK_LIBRARIES=gamebloke_core;gbl_warnings" OR
    NOT "${wiring}" MATCHES "production.INCLUDE_DIRECTORIES=${generated}/production" OR
    NOT "${wiring}" MATCHES "control.INCLUDE_DIRECTORIES=${generated}/control" OR
    NOT "${wiring}" MATCHES "common.c_standard=17")
  message(FATAL_ERROR "runner-metadata: runner-source-or-index-wiring-diverged")
endif()
file(STRINGS "${wiring_file}" wiring_lines)
foreach(property IN ITEMS COMPILE_OPTIONS COMPILE_DEFINITIONS LINK_OPTIONS)
  set(production_value "<missing>")
  set(control_value "<missing>")
  foreach(line IN LISTS wiring_lines)
    if(line MATCHES "^production\\.${property}=(.*)$")
      set(production_value "${CMAKE_MATCH_1}")
    elseif(line MATCHES "^control\\.${property}=(.*)$")
      set(control_value "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  if("${production_value}" STREQUAL "<missing>" OR
      NOT "${production_value}" STREQUAL "${control_value}")
    message(FATAL_ERROR "runner-metadata: runner-options-diverged")
  endif()
endforeach()

# Deliberately supply the production index where the control index is expected.
# The production table must reject that row identity because the canonical
# manifest is genuinely empty; no runtime index-selection switch exists.
if("${production_text}" MATCHES "synthetic-control-pass" OR
    NOT "${control_text}" MATCHES "synthetic-control-pass")
  message(FATAL_ERROR "runner-metadata: wrong-index-accepted")
endif()
message("runner-metadata: wrong-index-rejected (production index has no synthetic row)")

set(control_dir "${BIN}/runner-metadata-controls")
file(MAKE_DIRECTORY "${control_dir}")
set(bad_manifest "${control_dir}/malformed-manifest.json")
file(WRITE "${bad_manifest}" "{\"schema\": }\n")
set(bad_prod_index "${control_dir}/malformed-production.h")
set(bad_control_index "${control_dir}/malformed-control.h")
set(bad_manifest_out "${control_dir}/unused-control-manifest.json")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DGBL_SOURCE_DIR=${SRC}"
    "-DGBL_CONTROL_IMAGE=${pass_image}"
    "-DGBL_CONTROL_MANIFEST=${control_manifest}"
    "-DGBL_CONTROL_MANIFEST_OUTPUT=${bad_manifest_out}"
    "-DGBL_MANIFEST=${bad_manifest}"
    "-DGBL_REGISTRY=${SRC}/registry/disputes.json"
    "-DGBL_VOCAB=${SRC}/contracts/evidence.json"
    "-DGBL_OUT_PRODUCTION=${bad_prod_index}"
    "-DGBL_OUT_CONTROL=${bad_control_index}"
    -P "${generator_script}"
  RESULT_VARIABLE malformed_result
  OUTPUT_VARIABLE malformed_stdout
  ERROR_VARIABLE malformed_stderr)
if("${malformed_result}" STREQUAL "0" OR
    EXISTS "${bad_prod_index}" OR EXISTS "${bad_control_index}" OR
    NOT "${malformed_stdout}${malformed_stderr}" MATCHES "json: not-json")
  message(FATAL_ERROR "runner-metadata: malformed-manifest-produced-index")
endif()
message("runner-metadata: malformed-manifest-rejected-before-index-write")

# Keep the original synthetic manifest but substitute the generated one-byte
# image. Its actual bytes no longer match the independently pinned digest.
set(drift_prod_index "${control_dir}/drift-production.h")
set(drift_control_index "${control_dir}/drift-control.h")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DGBL_SOURCE_DIR=${SRC}"
    "-DGBL_CONTROL_IMAGE=${mutated_image}"
    "-DGBL_CONTROL_MANIFEST=${control_manifest}"
    "-DGBL_CONTROL_MANIFEST_OUTPUT=${bad_manifest_out}"
    "-DGBL_MANIFEST=${SRC}/tests/fixtures/manifest.json"
    "-DGBL_REGISTRY=${SRC}/registry/disputes.json"
    "-DGBL_VOCAB=${SRC}/contracts/evidence.json"
    "-DGBL_OUT_PRODUCTION=${drift_prod_index}"
    "-DGBL_OUT_CONTROL=${drift_control_index}"
    -P "${generator_script}"
  RESULT_VARIABLE drift_result
  OUTPUT_VARIABLE drift_stdout
  ERROR_VARIABLE drift_stderr)
if("${drift_result}" STREQUAL "0" OR
    EXISTS "${drift_prod_index}" OR EXISTS "${drift_control_index}" OR
    NOT "${drift_stdout}${drift_stderr}" MATCHES
      "fixture-index: digest-mismatch: synthetic-control-pass")
  message(FATAL_ERROR "runner-metadata: generated-image-digest-drift-accepted")
endif()
message("runner-metadata: stale-image-digest-rejected")

if(DEFINED HASH_TEST AND NOT "${HASH_TEST}" STREQUAL "")
  if(NOT EXISTS "${HASH_TEST}" OR IS_DIRECTORY "${HASH_TEST}")
    message(FATAL_ERROR "runner-metadata: hash-test-executable-missing")
  endif()
  execute_process(
    COMMAND "${HASH_TEST}" --wrong-independent-digest
    RESULT_VARIABLE wrong_digest_result
    OUTPUT_VARIABLE wrong_digest_stdout
    ERROR_VARIABLE wrong_digest_stderr)
  if(NOT "${wrong_digest_result}" STREQUAL "0" OR
      NOT "${wrong_digest_stdout}" MATCHES
        "ok 1 - deliberately wrong independent digest is rejected")
    message(FATAL_ERROR "runner-metadata: wrong-independent-digest-accepted")
  endif()

  execute_process(
    COMMAND "${HASH_TEST}" unknown-selector
    RESULT_VARIABLE unknown_selector_result
    OUTPUT_VARIABLE unknown_selector_stdout
    ERROR_VARIABLE unknown_selector_stderr)
  if(NOT "${unknown_selector_result}" STREQUAL "2" OR
      NOT "${unknown_selector_stderr}" MATCHES
        "usage: hash_test \\[--wrong-independent-digest\\]")
    message(FATAL_ERROR "runner-metadata: hash-test-accepted-unknown-selector")
  endif()
  message("runner-metadata: wrong-hash-expectation-rejected; unknown-selector-rejected")
endif()

message("runner-metadata: ok (production empty, synthetic control indexed, parity verified)")
