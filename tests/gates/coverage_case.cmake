cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS CASE SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "coverage-case: missing-${required}")
  endif()
endforeach()
if(NOT CASE STREQUAL "ledger-controls")
  message(FATAL_ERROR "coverage-case: unsupported-case")
endif()

get_filename_component(source_root "${SRC}" ABSOLUTE)
get_filename_component(binary_root "${BIN}" ABSOLUTE)
set(matrix "${source_root}/diagnostics/armv4t/cases.json")
set(checker "${source_root}/cmake/CheckArmCoverage.cmake")
set(builder "${source_root}/cmake/BuildArmDiagnostics.cmake")
set(inputs "${source_root}/diagnostics/armv4t/inputs.txt")
set(build_root "${source_root}/build")
cmake_path(IS_PREFIX build_root "${binary_root}" NORMALIZE binary_under_build)
if(NOT binary_under_build OR IS_SYMLINK "${source_root}/build" OR
   IS_SYMLINK "${binary_root}")
  message(FATAL_ERROR "coverage-case: unsafe-output-root")
endif()

foreach(required_path IN ITEMS "${matrix}" "${checker}" "${builder}" "${inputs}")
  if(NOT EXISTS "${required_path}" OR IS_DIRECTORY "${required_path}" OR
     IS_SYMLINK "${required_path}")
    message(FATAL_ERROR "coverage-case: required-ledger-artifact-missing: ${required_path}")
  endif()
endforeach()

include("${source_root}/cmake/GblJson.cmake")
file(READ "${matrix}" matrix_json)
file(STRINGS "${inputs}" input_lines)
set(input_paths "")
foreach(line IN LISTS input_lines)
  string(STRIP "${line}" line)
  if("${line}" STREQUAL "" OR "${line}" MATCHES "^#")
    continue()
  endif()
  list(APPEND input_paths "${line}")
endforeach()
foreach(required_input IN ITEMS
    cmake/BuildArmDiagnostics.cmake cmake/FixGbaHeader.cmake
    diagnostics/armv4t/NOTICE diagnostics/armv4t/cases.json
    diagnostics/armv4t/inputs.txt diagnostics/armv4t/linker.ld
    diagnostics/armv4t/smoke.S diagnostics/armv4t/startup.S
    scripts/fetch-toolchain.sh scripts/signature-scan.sh
    scripts/lib/signature-common.sh tests/signatures/allowlist.txt
    tests/signatures/bios.sha256 tests/signatures/logo.sha256
    toolchain/arm-none-eabi.lock)
  list(FIND input_paths "${required_input}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "coverage-case: source-input-unbound: ${required_input}")
  endif()
endforeach()
list(FIND input_paths "CMakeLists.txt" cmake_consumer_bound)
if(NOT cmake_consumer_bound EQUAL -1)
  message(FATAL_ERROR "coverage-case: unrelated-consumer-bound: CMakeLists.txt")
endif()
list(FIND input_paths "tests/fixtures/manifest.json" manifest_producer_bound)
if(NOT manifest_producer_bound EQUAL -1)
  message(FATAL_ERROR "coverage-case: manifest-consumer-bound: tests/fixtures/manifest.json")
endif()

set(control_root "${binary_root}/coverage-controls")
file(REMOVE_RECURSE "${control_root}")
file(MAKE_DIRECTORY "${control_root}")

function(find_case_index out family form)
  gbl_json_length(count "${matrix_json}" cases)
  math(EXPR last "${count}-1")
  set(found -1)
  foreach(index RANGE 0 ${last})
    string(JSON row_json GET "${matrix_json}" cases ${index})
    gbl_json_string(row_family "${row_json}" family)
    gbl_json_string(row_form "${row_json}" form)
    if(row_family STREQUAL "${family}" AND row_form STREQUAL "${form}")
      set(found "${index}")
      break()
    endif()
  endforeach()
  set(${out} "${found}" PARENT_SCOPE)
endfunction()

function(checker_failure label mode matrix_path expected_code)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DMODE=${mode}" "-DMATRIX=${matrix_path}"
      "-DMANIFEST=${source_root}/tests/fixtures/manifest.json"
      -P "${checker}"
    RESULT_VARIABLE check_rc OUTPUT_VARIABLE check_out ERROR_VARIABLE check_error)
  set(combined "${check_out}${check_error}")
  if("${check_rc}" STREQUAL "0" OR NOT combined MATCHES "coverage: ${expected_code}:")
    message(FATAL_ERROR "coverage-case: ${label}-control-failed: ${combined}")
  endif()
  message(STATUS "coverage-case: ${label} refusal observed (${expected_code})")
endfunction()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -DMODE=draft "-DMATRIX=${matrix}" -P "${checker}"
  RESULT_VARIABLE clean_rc OUTPUT_VARIABLE clean_out ERROR_VARIABLE clean_error)
set(clean_combined "${clean_out}${clean_error}")
if(NOT "${clean_rc}" STREQUAL "0" OR
   NOT clean_combined MATCHES "coverage: ok" OR
   NOT clean_combined MATCHES "targets=334" OR
   NOT clean_combined MATCHES "pending=356" OR
   NOT clean_combined MATCHES "source-verified=1")
  message(FATAL_ERROR "coverage-case: clean-ledger-control-failed: ${clean_error}${clean_out}")
endif()
message(STATUS "coverage-case: clean draft inventory passed (334 ARM/Thumb forms; formats 1..19)")

set(variant "${control_root}/zero-id.json")
string(JSON changed SET "${matrix_json}" cases 0 id "0")
file(WRITE "${variant}" "${changed}\n")
checker_failure(zero-id draft "${variant}" invalid-id)

find_case_index(smoke_index diagnostic-harness
  smoke-positive-and-real-assertion-broken-same-case-pair)
find_case_index(target_index data-processing AND:rotated-immediate)
if(smoke_index LESS 0 OR target_index LESS 0)
  message(FATAL_ERROR "coverage-case: required-control-case-missing")
endif()
string(JSON smoke_id GET "${matrix_json}" cases ${smoke_index} id)
gbl_json_string(smoke_state "${matrix_json}" cases ${smoke_index} coverage_state)
if(NOT smoke_state STREQUAL "source-verified")
  message(FATAL_ERROR "coverage-case: smoke-source-state-not-admission-independent: ${smoke_state}")
endif()

set(verified_matrix "${control_root}/verified-smoke.json")
string(JSON changed SET "${matrix_json}" cases ${smoke_index} coverage_state "\"verified\"")
file(WRITE "${verified_matrix}" "${changed}\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DMODE=draft "-DMATRIX=${verified_matrix}" -P "${checker}"
  RESULT_VARIABLE verified_rc OUTPUT_VARIABLE verified_out ERROR_VARIABLE verified_error)
set(verified_combined "${verified_out}${verified_error}")
if(NOT "${verified_rc}" STREQUAL "0" OR
   NOT verified_combined MATCHES "verified=1" OR
   NOT verified_combined MATCHES "smoke-pairs=1" OR
   NOT verified_combined MATCHES "pending=356")
  message(FATAL_ERROR "coverage-case: verified-smoke-count-control-failed: ${verified_combined}")
endif()
message(STATUS "coverage-case: verified smoke still counts as exactly one pair in draft mode")

set(variant "${control_root}/malformed-verified-smoke.json")
string(JSON changed SET "${matrix_json}" cases ${smoke_index} coverage_state "\"verified\"")
string(JSON changed SET "${changed}" cases ${smoke_index}
  broken_assertion thumb_broken_operand 6)
file(WRITE "${variant}" "${changed}\n")
checker_failure(malformed-verified-smoke draft "${variant}" broken-assertion-invalid)

set(variant "${control_root}/temporal-smoke-expectation.json")
string(JSON changed SET "${matrix_json}" cases ${smoke_index} expected value
  "\"Final pins await hosted identity evidence.\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(temporal-smoke-expectation draft "${variant}" smoke-temporal-expectation)

file(SHA256 "${matrix}" matrix_sha_before_admission)
file(SHA256 "${inputs}" inputs_sha_before_admission)
set(manifest_variant "${control_root}/manifest-only.json")
file(WRITE "${manifest_variant}"
  "{\n  \"schema\": \"gamebloke.manifest/1\",\n  \"fixtures\": [{\"id\": \"smoke\"}, {\"id\": \"smoke-broken\"}]\n}\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DMODE=smoke-admission "-DMATRIX=${matrix}"
    "-DMANIFEST=${manifest_variant}" -P "${checker}"
  RESULT_VARIABLE admission_rc OUTPUT_VARIABLE admission_out ERROR_VARIABLE admission_error)
set(admission_combined "${admission_out}${admission_error}")
if(NOT "${admission_rc}" STREQUAL "0" OR
   NOT admission_combined MATCHES "smoke-admission: ok" OR
   NOT admission_combined MATCHES "fixture-links=2")
  message(FATAL_ERROR "coverage-case: manifest-only-admission-control-failed: ${admission_combined}")
endif()
file(SHA256 "${matrix}" matrix_sha_after_admission)
file(SHA256 "${inputs}" inputs_sha_after_admission)
if(NOT matrix_sha_before_admission STREQUAL matrix_sha_after_admission OR
   NOT inputs_sha_before_admission STREQUAL inputs_sha_after_admission)
  message(FATAL_ERROR "coverage-case: manifest-only-admission-mutated-bound-input")
endif()
message(STATUS "coverage-case: manifest-only smoke admission passed without changing bound inputs")

file(WRITE "${manifest_variant}"
  "{\n  \"schema\": \"gamebloke.manifest/1\",\n  \"fixtures\": [{\"id\": \"smoke\"}]\n}\n")
checker_failure(missing-smoke-manifest-link smoke-admission "${matrix}"
  smoke-admission-fixture-link-missing)

set(variant "${control_root}/duplicate-id.json")
string(JSON changed SET "${matrix_json}" cases ${target_index} id "${smoke_id}")
file(WRITE "${variant}" "${changed}\n")
checker_failure(duplicate-id draft "${variant}" duplicate-id)

set(variant "${control_root}/missing-form.json")
string(JSON changed SET "${matrix_json}" cases ${target_index} form "\"AND:invented-form\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(missing-form draft "${variant}" missing-form)

set(variant "${control_root}/silent-drop.json")
string(JSON changed REMOVE "${matrix_json}" cases ${target_index})
file(WRITE "${variant}" "${changed}\n")
checker_failure(silent-drop draft "${variant}" missing-form)

set(variant "${control_root}/wrong-variant.json")
string(JSON changed SET "${matrix_json}" cases ${smoke_index} negative_fixture "\"smoke\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(wrong-variant draft "${variant}" wrong-variant)

set(variant "${control_root}/final-with-pending.json")
string(JSON changed SET "${matrix_json}" stage "\"final\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(final-pending-case final "${variant}" pending-case)

gbl_json_length(case_count "${matrix_json}" cases)
math(EXPR case_last "${case_count}-1")
set(pending_case_id "")
foreach(index RANGE 0 ${case_last})
  string(JSON row_json GET "${matrix_json}" cases ${index})
  string(JSON inventory_target GET "${row_json}" inventory_target)
  gbl_json_string(coverage_state "${row_json}" coverage_state)
  if(inventory_target AND coverage_state STREQUAL "pending")
    string(JSON pending_case_id GET "${row_json}" id)
    break()
  endif()
endforeach()
if("${pending_case_id}" STREQUAL "")
  message(FATAL_ERROR "coverage-case: no-pending-case-for-builder-control")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}"
    "-DOUT=${control_root}/pending-case-output"
    "-DTOOLCHAIN_ROOT=${source_root}/build/toolchains"
    -DMODE=case "-DCASE_ID=${pending_case_id}"
    -P "${builder}"
  RESULT_VARIABLE case_rc OUTPUT_VARIABLE case_out ERROR_VARIABLE case_error)
set(case_combined "${case_out}${case_error}")
if("${case_rc}" STREQUAL "0" OR
   NOT case_combined MATCHES "arm-diagnostics: case-not-runnable: pending")
  message(FATAL_ERROR "coverage-case: pending-case-builder-refusal-failed: ${case_combined}")
endif()
message(STATUS "coverage-case: pending case ${pending_case_id} refused honestly")

execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}"
    "-DOUT=${control_root}/all-output"
    "-DTOOLCHAIN_ROOT=${source_root}/build/toolchains" -DMODE=all -P "${builder}"
  RESULT_VARIABLE all_rc OUTPUT_VARIABLE all_out ERROR_VARIABLE all_error)
set(all_combined "${all_out}${all_error}")
if("${all_rc}" STREQUAL "0" OR
   NOT all_combined MATCHES "arm-diagnostics: all-cases-incomplete: pending")
  message(FATAL_ERROR "coverage-case: all-builder-refusal-failed: ${all_combined}")
endif()
message(STATUS "coverage-case: aggregate build refused while cases remain pending")
message("coverage-case: ok (draft/final/admission states, source-input boundary, and mutation controls)")
