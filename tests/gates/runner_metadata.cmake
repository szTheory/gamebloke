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
set(zero_case_image "${generated}/fixtures/owned-arm-pass-zero-case.gba")
set(fail_image "${generated}/fixtures/owned-arm-fail.gba")
set(mutated_image "${generated}/fixtures/owned-arm-pass-mutated.gba")
set(wiring_file "${generated}/runner-target-wiring.txt")
set(generator_script "${SRC}/cmake/GenerateFixtureIndex.cmake")

foreach(path IN ITEMS "${production_index}" "${control_index}"
    "${control_manifest}" "${pass_image}" "${zero_case_image}" "${fail_image}"
    "${mutated_image}" "${wiring_file}")
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
    NOT "${control_text}" MATCHES "synthetic-control-fail" OR
    NOT "${control_text}" MATCHES "synthetic-control-bios-required" OR
    NOT "${control_text}" MATCHES "synthetic-control-pass-zero-case" OR
    NOT "${control_text}" MATCHES "rom_sha256" OR
    NOT "${control_text}" MATCHES "disp-gbatek-errata" OR
    NOT "${control_text}" MATCHES "five-word-v1-final-marker" OR
    NOT "${control_manifest_text}" MATCHES "LicenseRef-Gamebloke-Synthetic-Control" OR
    NOT "${control_manifest_text}" MATCHES "synthetic-control-pass-zero-case" OR
    NOT "${control_manifest_text}" MATCHES "redistributable\\\": \\\"unknown")
  message(FATAL_ERROR "runner-metadata: synthetic-control-row-incomplete")
endif()
if(NOT "${control_manifest_text}" MATCHES "project-stub" OR
    NOT "${control_manifest_text}" MATCHES "synthetic-control-fail" OR
    "${control_manifest_text}" MATCHES "public-ci" OR
    NOT "${control_manifest_text}" MATCHES "local-only" OR
    NOT "${control_manifest_text}" MATCHES "no redistribution grant")
  message(FATAL_ERROR "runner-metadata: synthetic-control-authority-overstated")
endif()

# The independently computed byte digest in CMake must be present in the
# synthetic immutable index, and the generated one-byte variant must not match.
file(SHA256 "${pass_image}" pass_digest)
file(SHA256 "${zero_case_image}" zero_case_digest)
file(SHA256 "${mutated_image}" mutated_digest)
if("${pass_digest}" STREQUAL "${mutated_digest}" OR
    NOT "${control_text}" MATCHES "${pass_digest}" OR
    NOT "${control_text}" MATCHES "${zero_case_digest}")
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
    "-DGBL_CONTROL_FAIL_IMAGE=${fail_image}"
    "-DGBL_CONTROL_SKIP_IMAGE=${pass_image}"
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
    "-DGBL_CONTROL_FAIL_IMAGE=${fail_image}"
    "-DGBL_CONTROL_SKIP_IMAGE=${pass_image}"
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

# Build a production runner against an isolated index generated from a legacy
# git row. Its absent artifact pin must keep the historical refusal before the
# synthetic input can execute. This compiles the actual runner sources with the
# same core library and uses only the local build-generated control image.
set(legacy_dir "${control_dir}/legacy-git")
set(legacy_index_dir "${legacy_dir}/generated/production")
set(legacy_control_index_dir "${legacy_dir}/generated/control")
set(legacy_manifest "${legacy_dir}/manifest.json")
set(legacy_control_manifest "${legacy_dir}/generated/control/manifest.json")
set(legacy_runner_source "${legacy_dir}/source")
set(legacy_runner_build "${legacy_dir}/build")
set(legacy_runner "${legacy_runner_build}/legacy-runner")
set(legacy_prod_index "${legacy_index_dir}/fixture_index.h")
set(legacy_control_index "${legacy_control_index_dir}/fixture_index.h")
file(MAKE_DIRECTORY "${legacy_index_dir}" "${legacy_control_index_dir}"
  "${legacy_runner_source}")
file(COPY_FILE "${SRC}/contracts/examples/manifest.json" "${legacy_manifest}")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DGBL_SOURCE_DIR=${SRC}"
    "-DGBL_CONTROL_IMAGE=${pass_image}"
    "-DGBL_CONTROL_FAIL_IMAGE=${fail_image}"
    "-DGBL_CONTROL_SKIP_IMAGE=${pass_image}"
    "-DGBL_CONTROL_MANIFEST=${control_manifest}"
    "-DGBL_CONTROL_MANIFEST_OUTPUT=${legacy_control_manifest}"
    "-DGBL_MANIFEST=${legacy_manifest}"
    "-DGBL_REGISTRY=${SRC}/registry/disputes.json"
    "-DGBL_VOCAB=${SRC}/contracts/evidence.json"
    "-DGBL_OUT_PRODUCTION=${legacy_prod_index}"
    "-DGBL_OUT_CONTROL=${legacy_control_index}"
    -P "${generator_script}"
  RESULT_VARIABLE legacy_index_result
  OUTPUT_VARIABLE legacy_index_stdout
  ERROR_VARIABLE legacy_index_stderr)
if(NOT "${legacy_index_result}" STREQUAL "0" OR
    NOT EXISTS "${legacy_prod_index}")
  message(FATAL_ERROR
    "runner-metadata: legacy-index-generation-failed: ${legacy_index_stdout}${legacy_index_stderr}")
endif()
file(READ "${legacy_prod_index}" legacy_index_text)
if(NOT "${legacy_index_text}" MATCHES
    "\"example-row\", \"\", \"suite-self-checking\"")
  message(FATAL_ERROR "runner-metadata: legacy-index-digest-not-empty")
endif()
file(WRITE "${legacy_runner_source}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.25)\n"
  "project(gbl_legacy_runner LANGUAGES C)\n"
  "add_executable(legacy-runner \"${SRC}/runner/main.c\" \"${SRC}/runner/run.c\" \"${SRC}/runner/result.c\" \"${SRC}/runner/sha256.c\")\n"
  "set_property(TARGET legacy-runner PROPERTY C_STANDARD 17)\n"
  "set_property(TARGET legacy-runner PROPERTY C_STANDARD_REQUIRED ON)\n"
  "target_include_directories(legacy-runner PRIVATE \"${legacy_index_dir}\" \"${SRC}/runner\" \"${SRC}/include\")\n"
  "target_link_libraries(legacy-runner PRIVATE \"${BIN}/libgamebloke_core.a\")\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${legacy_runner_source}" -B "${legacy_runner_build}"
  RESULT_VARIABLE legacy_configure_result
  OUTPUT_VARIABLE legacy_configure_stdout
  ERROR_VARIABLE legacy_configure_stderr)
if(NOT "${legacy_configure_result}" STREQUAL "0")
  message(FATAL_ERROR
    "runner-metadata: legacy-runner-configure-failed: ${legacy_configure_stdout}${legacy_configure_stderr}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${legacy_runner_build}" --target legacy-runner --parallel 2
  RESULT_VARIABLE legacy_build_result
  OUTPUT_VARIABLE legacy_build_stdout
  ERROR_VARIABLE legacy_build_stderr)
if(NOT "${legacy_build_result}" STREQUAL "0" OR
    NOT EXISTS "${legacy_runner}")
  message(FATAL_ERROR
    "runner-metadata: legacy-runner-build-failed: ${legacy_build_stdout}${legacy_build_stderr}")
endif()
execute_process(
  COMMAND "${legacy_runner}" --rom "${pass_image}" --fixture example-row --cycles 1000
  RESULT_VARIABLE legacy_run_result
  OUTPUT_VARIABLE legacy_run_json
  ERROR_VARIABLE legacy_run_stderr)
string(JSON legacy_status ERROR_VARIABLE legacy_status_error
  GET "${legacy_run_json}" status)
string(JSON legacy_reason ERROR_VARIABLE legacy_reason_error
  GET "${legacy_run_json}" reason)
string(JSON legacy_ran ERROR_VARIABLE legacy_ran_error
  GET "${legacy_run_json}" fixture ran)
if(NOT "${legacy_run_result}" STREQUAL "1" OR
    NOT "${legacy_status_error}" STREQUAL "NOTFOUND" OR
    NOT "${legacy_reason_error}" STREQUAL "NOTFOUND" OR
    NOT "${legacy_ran_error}" STREQUAL "NOTFOUND" OR
    NOT legacy_status STREQUAL "REFUSED" OR
    NOT legacy_reason STREQUAL "fixture-digest-unavailable" OR legacy_ran)
  message(FATAL_ERROR
    "runner-metadata: legacy-empty-digest-refusal-changed: exit=${legacy_run_result}; json=${legacy_run_json}; stderr=${legacy_run_stderr}")
endif()
message("runner-metadata: legacy git empty digest still refuses before execution")

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
