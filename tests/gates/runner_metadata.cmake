cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN PARENT_C_STANDARD
    PARENT_C_STANDARD_REQUIRED PARENT_C_EXTENSIONS)
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
set(production_manifest "${SRC}/tests/fixtures/manifest.json")

function(gbl_read_cache_entry cache_path key output_type output_value)
  file(STRINGS "${cache_path}" entries REGEX "^${key}:[^=]+=")
  list(LENGTH entries entry_count)
  if(NOT entry_count EQUAL 1)
    message(FATAL_ERROR "runner-metadata: parent-cache-entry-missing: ${key}")
  endif()
  list(GET entries 0 entry)
  if(NOT entry MATCHES "^[^:]+:([^=]+)=(.*)$")
    message(FATAL_ERROR "runner-metadata: parent-cache-entry-invalid: ${key}")
  endif()
  set(${output_type} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${output_value} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

set(parent_cache "${BIN}/CMakeCache.txt")
if(NOT EXISTS "${parent_cache}" OR IS_DIRECTORY "${parent_cache}")
  message(FATAL_ERROR "runner-metadata: parent-cache-missing")
endif()
foreach(key IN ITEMS CMAKE_C_COMPILER CMAKE_C_FLAGS CMAKE_EXE_LINKER_FLAGS
    CMAKE_BUILD_TYPE CMAKE_GENERATOR CMAKE_MAKE_PROGRAM)
  gbl_read_cache_entry("${parent_cache}" "${key}"
    "parent_${key}_type" "parent_${key}")
endforeach()
string(TOUPPER "${parent_CMAKE_BUILD_TYPE}" parent_build_type_upper)
foreach(key IN ITEMS CMAKE_C_FLAGS CMAKE_EXE_LINKER_FLAGS)
  set(config_key "${key}_${parent_build_type_upper}")
  gbl_read_cache_entry("${parent_cache}" "${config_key}"
    "parent_${config_key}_type" "parent_${config_key}")
endforeach()

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
if(NOT EXISTS "${production_manifest}" OR IS_DIRECTORY "${production_manifest}")
  message(FATAL_ERROR "runner-metadata: canonical-manifest-missing")
endif()
file(READ "${production_manifest}" production_manifest_text)
string(JSON canonical_fixture_count ERROR_VARIABLE canonical_manifest_error
  LENGTH "${production_manifest_text}" fixtures)
if(NOT "${canonical_manifest_error}" STREQUAL "NOTFOUND")
  message(FATAL_ERROR "runner-metadata: canonical-manifest-invalid")
endif()
string(FIND "${production_text}"
  "gbl_fixture_metadata_count = ${canonical_fixture_count}u;"
  production_count_position)
if(production_count_position EQUAL -1)
  message(FATAL_ERROR "runner-metadata: production-index-count-mismatches-manifest")
endif()
if(canonical_fixture_count GREATER 0)
  math(EXPR canonical_fixture_last "${canonical_fixture_count}-1")
  foreach(index RANGE ${canonical_fixture_last})
    string(JSON canonical_row GET "${production_manifest_text}" fixtures ${index})
    string(JSON canonical_fixture_id ERROR_VARIABLE canonical_id_error
      GET "${canonical_row}" id)
    if(NOT "${canonical_id_error}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "runner-metadata: canonical-fixture-id-invalid")
    endif()
    string(FIND "${production_text}"
      "    \"${canonical_fixture_id}\", \"" canonical_id_position)
    if(canonical_id_position EQUAL -1)
      message(FATAL_ERROR
        "runner-metadata: canonical-fixture-absent-from-production-index: ${canonical_fixture_id}")
    endif()
    string(JSON canonical_digest ERROR_VARIABLE digest_error GET
      "${canonical_row}" build artifact sha256)
    if(digest_error STREQUAL "NOTFOUND")
      string(FIND "${production_text}"
        "\"${canonical_fixture_id}\", \"${canonical_digest}\"," digest_position)
      if(digest_position EQUAL -1)
        message(FATAL_ERROR "runner-metadata: canonical-artifact-pin-mismatches-index:${canonical_fixture_id}")
      endif()
    endif()
  endforeach()
endif()
foreach(synthetic_id IN ITEMS synthetic-control-pass synthetic-control-fail
    synthetic-control-bios-required synthetic-control-pass-zero-case)
  string(FIND "${production_text}" "\"${synthetic_id}\"" synthetic_id_position)
  if(NOT synthetic_id_position EQUAL -1)
    message(FATAL_ERROR "runner-metadata: synthetic-row-in-production-index")
  endif()
endforeach()
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
# The production table must reject the synthetic row identity while retaining
# every row from the canonical manifest; no runtime index-selection switch exists.
if("${production_text}" MATCHES "synthetic-control-pass" OR
    NOT "${control_text}" MATCHES "synthetic-control-pass")
  message(FATAL_ERROR "runner-metadata: wrong-index-accepted")
endif()
message("runner-metadata: canonical production authority verified (${canonical_fixture_count} rows; synthetic rows excluded)")

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
    "-DGBL_OUTPUT_ROOT=${BIN}"
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
    "-DGBL_OUTPUT_ROOT=${BIN}"
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
file(REMOVE_RECURSE "${legacy_runner_build}")
file(MAKE_DIRECTORY "${legacy_index_dir}" "${legacy_control_index_dir}"
  "${legacy_runner_source}")
file(COPY_FILE "${SRC}/contracts/examples/manifest.json" "${legacy_manifest}")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DGBL_SOURCE_DIR=${SRC}"
    "-DGBL_OUTPUT_ROOT=${BIN}"
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
  "set(CMAKE_C_STANDARD ${PARENT_C_STANDARD})\n"
  "set(CMAKE_C_STANDARD_REQUIRED ${PARENT_C_STANDARD_REQUIRED})\n"
  "set(CMAKE_C_EXTENSIONS ${PARENT_C_EXTENSIONS})\n"
  "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n"
  "project(gbl_legacy_runner LANGUAGES C)\n"
  "add_executable(legacy-runner \"${SRC}/runner/main.c\" \"${SRC}/runner/run.c\" \"${SRC}/runner/result.c\" \"${SRC}/runner/sha256.c\")\n"
  "target_include_directories(legacy-runner PRIVATE \"${legacy_index_dir}\" \"${SRC}/runner\" \"${SRC}/include\")\n"
  "get_target_property(legacy_standard legacy-runner C_STANDARD)\n"
  "get_target_property(legacy_standard_required legacy-runner C_STANDARD_REQUIRED)\n"
  "get_target_property(legacy_extensions legacy-runner C_EXTENSIONS)\n"
  "file(WRITE \"${legacy_runner_build}/language-settings.txt\" \"standard=\${legacy_standard}\\nstandard-required=\${legacy_standard_required}\\nextensions=\${legacy_extensions}\\n\")\n"
  "target_link_libraries(legacy-runner PRIVATE \"${BIN}/libgamebloke_core.a\")\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${legacy_runner_source}" -B "${legacy_runner_build}"
    -G "${parent_CMAKE_GENERATOR}"
    "-DCMAKE_MAKE_PROGRAM:${parent_CMAKE_MAKE_PROGRAM_type}=${parent_CMAKE_MAKE_PROGRAM}"
    "-DCMAKE_C_COMPILER:${parent_CMAKE_C_COMPILER_type}=${parent_CMAKE_C_COMPILER}"
    "-DCMAKE_C_FLAGS:${parent_CMAKE_C_FLAGS_type}=${parent_CMAKE_C_FLAGS}"
    "-DCMAKE_C_FLAGS_${parent_build_type_upper}:${parent_CMAKE_C_FLAGS_${parent_build_type_upper}_type}=${parent_CMAKE_C_FLAGS_${parent_build_type_upper}}"
    "-DCMAKE_EXE_LINKER_FLAGS:${parent_CMAKE_EXE_LINKER_FLAGS_type}=${parent_CMAKE_EXE_LINKER_FLAGS}"
    "-DCMAKE_EXE_LINKER_FLAGS_${parent_build_type_upper}:${parent_CMAKE_EXE_LINKER_FLAGS_${parent_build_type_upper}_type}=${parent_CMAKE_EXE_LINKER_FLAGS_${parent_build_type_upper}}"
    "-DCMAKE_BUILD_TYPE:${parent_CMAKE_BUILD_TYPE_type}=${parent_CMAKE_BUILD_TYPE}"
  RESULT_VARIABLE legacy_configure_result
  OUTPUT_VARIABLE legacy_configure_stdout
  ERROR_VARIABLE legacy_configure_stderr)
if(NOT "${legacy_configure_result}" STREQUAL "0")
  message(FATAL_ERROR
    "runner-metadata: legacy-runner-configure-failed: ${legacy_configure_stdout}${legacy_configure_stderr}")
endif()
set(legacy_cache "${legacy_runner_build}/CMakeCache.txt")
foreach(key IN ITEMS CMAKE_C_COMPILER CMAKE_C_FLAGS CMAKE_EXE_LINKER_FLAGS
    CMAKE_BUILD_TYPE "CMAKE_C_FLAGS_${parent_build_type_upper}"
    "CMAKE_EXE_LINKER_FLAGS_${parent_build_type_upper}"
    CMAKE_GENERATOR CMAKE_MAKE_PROGRAM)
  gbl_read_cache_entry("${legacy_cache}" "${key}"
    "legacy_${key}_type" "legacy_${key}")
  if(NOT "${legacy_${key}}" STREQUAL "${parent_${key}}")
    message(FATAL_ERROR
      "runner-metadata: legacy-runner-parent-setting-not-inherited: ${key}")
  endif()
  if(NOT key STREQUAL "CMAKE_C_COMPILER" AND
      NOT "${legacy_${key}_type}" STREQUAL "${parent_${key}_type}")
    message(FATAL_ERROR
      "runner-metadata: legacy-runner-cache-type-not-inherited: ${key}")
  endif()
endforeach()
set(legacy_language_file "${legacy_runner_build}/language-settings.txt")
if(NOT EXISTS "${legacy_language_file}")
  message(FATAL_ERROR "runner-metadata: legacy-runner-language-settings-missing")
endif()
file(STRINGS "${legacy_language_file}" legacy_language_settings)
foreach(setting IN ITEMS
    "standard=${PARENT_C_STANDARD}"
    "standard-required=${PARENT_C_STANDARD_REQUIRED}"
    "extensions=${PARENT_C_EXTENSIONS}")
  if(NOT setting IN_LIST legacy_language_settings)
    message(FATAL_ERROR
      "runner-metadata: legacy-runner-language-setting-not-inherited: ${setting}")
  endif()
endforeach()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${legacy_runner_build}"
    --target legacy-runner --parallel 2 --verbose
  RESULT_VARIABLE legacy_build_result
  OUTPUT_VARIABLE legacy_build_stdout
  ERROR_VARIABLE legacy_build_stderr)
set(legacy_build_stdout_path "${control_dir}/legacy-runner-build.stdout.log")
set(legacy_build_stderr_path "${control_dir}/legacy-runner-build.stderr.log")
file(WRITE "${legacy_build_stdout_path}" "${legacy_build_stdout}")
file(WRITE "${legacy_build_stderr_path}" "${legacy_build_stderr}")
if(NOT "${legacy_build_result}" STREQUAL "0" OR
    NOT EXISTS "${legacy_runner}")
  file(STRINGS "${legacy_build_stderr_path}" legacy_build_preview LIMIT_COUNT 12)
  if(NOT legacy_build_preview)
    file(STRINGS "${legacy_build_stdout_path}" legacy_build_preview LIMIT_COUNT 12)
  endif()
  string(JOIN "\n" legacy_build_preview_text ${legacy_build_preview})
  message(FATAL_ERROR
    "runner-metadata: legacy-runner-build-failed: exit=${legacy_build_result}; full stdout=${legacy_build_stdout_path}; full stderr=${legacy_build_stderr_path}; context=${legacy_build_preview_text}")
endif()
set(legacy_compile_commands_path "${legacy_runner_build}/compile_commands.json")
if(NOT EXISTS "${legacy_compile_commands_path}")
  message(FATAL_ERROR "runner-metadata: legacy-runner-compile-commands-missing")
endif()
file(READ "${legacy_compile_commands_path}" legacy_compile_commands)
string(JSON legacy_compile_count LENGTH "${legacy_compile_commands}")
set(legacy_runner_main_command "")
if(legacy_compile_count GREATER 0)
  math(EXPR legacy_compile_last "${legacy_compile_count} - 1")
  foreach(index RANGE 0 ${legacy_compile_last})
    string(JSON compile_file GET "${legacy_compile_commands}" ${index} file)
    if(compile_file STREQUAL "${SRC}/runner/main.c")
      string(JSON legacy_runner_main_command GET "${legacy_compile_commands}"
        ${index} command)
      break()
    endif()
  endforeach()
endif()
if("${legacy_runner_main_command}" STREQUAL "")
  message(FATAL_ERROR "runner-metadata: legacy-runner-compile-command-missing")
endif()
if(PARENT_C_EXTENSIONS)
  set(expected_c_standard_flag "-std=gnu${PARENT_C_STANDARD}")
else()
  set(expected_c_standard_flag "-std=c${PARENT_C_STANDARD}")
endif()
string(FIND "${legacy_runner_main_command}" "${expected_c_standard_flag}"
  legacy_standard_flag_index)
if(legacy_standard_flag_index LESS 0)
  message(FATAL_ERROR
    "runner-metadata: legacy-runner-compiler-standard-not-inherited")
endif()
if(parent_CMAKE_C_FLAGS MATCHES "(^| )-fsanitize=address")
  string(FIND "${legacy_runner_main_command}" "-fsanitize=address"
    legacy_asan_compile_flag_index)
  string(FIND "${legacy_build_stdout}${legacy_build_stderr}"
    "-fsanitize=address" legacy_asan_link_flag_index)
  if(legacy_asan_compile_flag_index LESS 0 OR legacy_asan_link_flag_index LESS 0)
    message(FATAL_ERROR
      "runner-metadata: legacy-runner-sanitizer-compile-or-link-flag-missing")
  endif()
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

message("runner-metadata: ok (canonical production rows/pins indexed; synthetic controls separate; parity verified)")
