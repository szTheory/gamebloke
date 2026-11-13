cmake_minimum_required(VERSION 3.25)

function(find_preset json collection desired_name out_index)
  string(JSON preset_count LENGTH "${json}" "${collection}")
  set(found_index "")
  if(preset_count GREATER 0)
    math(EXPR last_index "${preset_count} - 1")
    foreach(index RANGE 0 ${last_index})
      string(JSON preset_name GET "${json}" "${collection}" ${index} name)
      if(preset_name STREQUAL desired_name)
        set(found_index "${index}")
        break()
      endif()
    endforeach()
  endif()
  set(${out_index} "${found_index}" PARENT_SCOPE)
endfunction()

function(check_preset_contract source_root)
  file(READ "${source_root}/CMakePresets.json" presets)
  foreach(collection IN ITEMS configurePresets buildPresets testPresets workflowPresets)
    find_preset("${presets}" "${collection}" diagnostics preset_index)
    if("${preset_index}" STREQUAL "")
      message(FATAL_ERROR "toolchain: missing-${collection}-diagnostics-preset")
    endif()
  endforeach()

  find_preset("${presets}" configurePresets diagnostics configure_index)
  foreach(pair IN ITEMS
      "GBL_LANE;diagnostics"
      "GBL_DIAGNOSTIC_STAGE;proof"
      "GBL_DIAGNOSTIC_GROUP;all"
      "GBL_REQUIRE_DIAGNOSTICS;ON"
      "GBL_TOOLCHAIN_ROOT;\${sourceDir}/build/toolchains"
      "CMAKE_BUILD_TYPE;Release"
      "CMAKE_COMPILE_WARNING_AS_ERROR;ON")
    list(GET pair 0 key)
    list(GET pair 1 expected)
    string(JSON actual GET "${presets}" configurePresets ${configure_index}
      cacheVariables "${key}")
    if(NOT "${actual}" STREQUAL "${expected}")
      message(FATAL_ERROR "toolchain: preset-${key}-mismatch: ${actual}")
    endif()
  endforeach()
  foreach(group IN ITEMS arm thumb)
    set(name "diagnostics-${group}")
    find_preset("${presets}" configurePresets "${name}" configured)
    string(JSON actual GET "${presets}" configurePresets ${configured} cacheVariables GBL_DIAGNOSTIC_GROUP)
    string(JSON parent GET "${presets}" configurePresets ${configured} inherits)
    if(NOT actual STREQUAL group OR NOT parent STREQUAL "diagnostics")
      message(FATAL_ERROR "toolchain: grouped-preset-authority-mismatch")
    endif()
    foreach(collection IN ITEMS buildPresets testPresets)
      find_preset("${presets}" "${collection}" "${name}" configured)
      string(JSON actual GET "${presets}" "${collection}" ${configured} configurePreset)
      if(NOT actual STREQUAL name)
        message(FATAL_ERROR "toolchain: grouped-preset-unlinked")
      endif()
    endforeach()
    find_preset("${presets}" testPresets "${name}" configured)
    string(JSON actual GET "${presets}" testPresets ${configured} output outputJUnitFile)
    if(NOT actual STREQUAL "\${sourceDir}/build/${name}/junit.xml")
      message(FATAL_ERROR "toolchain: grouped-preset-junit-mismatch")
    endif()
    find_preset("${presets}" workflowPresets "${name}" configured)
    string(JSON count LENGTH "${presets}" workflowPresets ${configured} steps)
    if(NOT count EQUAL 4)
      message(FATAL_ERROR "toolchain: grouped-preset-step-count")
    endif()
    set(expected "configure:${name}" "build:${name}" "test:${name}" "test:${name}-post")
    foreach(index RANGE 0 3)
      string(JSON type GET "${presets}" workflowPresets ${configured} steps ${index} type)
      string(JSON step GET "${presets}" workflowPresets ${configured} steps ${index} name)
      list(GET expected ${index} required)
      if(NOT "${type}:${step}" STREQUAL required)
        message(FATAL_ERROR "toolchain: grouped-preset-step-mismatch")
      endif()
    endforeach()
  endforeach()

  find_preset("${presets}" buildPresets diagnostics build_index)
  string(JSON build_configure GET "${presets}" buildPresets ${build_index}
    configurePreset)
  if(NOT build_configure STREQUAL "diagnostics")
    message(FATAL_ERROR "toolchain: diagnostics-build-preset-unlinked")
  endif()

  find_preset("${presets}" testPresets diagnostics test_index)
  string(JSON test_configure GET "${presets}" testPresets ${test_index}
    configurePreset)
  string(JSON junit_file GET "${presets}" testPresets ${test_index}
    output outputJUnitFile)
  string(JSON no_tests_action GET "${presets}" testPresets ${test_index}
    execution noTestsAction)
  string(JSON test_verbosity GET "${presets}" testPresets ${test_index}
    output verbosity)
  if(NOT test_configure STREQUAL "diagnostics" OR
     NOT junit_file STREQUAL "\${sourceDir}/build/diagnostics/junit.xml" OR
     NOT no_tests_action STREQUAL "error" OR
     NOT test_verbosity STREQUAL "verbose")
    message(FATAL_ERROR "toolchain: diagnostics-test-preset-contract-mismatch")
  endif()

  find_preset("${presets}" workflowPresets diagnostics workflow_index)
  string(JSON step_count LENGTH "${presets}" workflowPresets ${workflow_index}
    steps)
  if(NOT step_count EQUAL 4)
    message(FATAL_ERROR "toolchain: diagnostics-workflow-step-count")
  endif()
  set(expected_steps
    "configure:diagnostics" "build:diagnostics" "test:diagnostics"
    "test:diagnostics-post")
  foreach(index RANGE 0 3)
    string(JSON step_type GET "${presets}" workflowPresets ${workflow_index}
      steps ${index} type)
    string(JSON step_name GET "${presets}" workflowPresets ${workflow_index}
      steps ${index} name)
    list(GET expected_steps ${index} expected_step)
    if(NOT "${step_type}:${step_name}" STREQUAL "${expected_step}")
      message(FATAL_ERROR "toolchain: diagnostics-workflow-step-${index}-mismatch")
    endif()
  endforeach()
endfunction()

function(check_toolchain source_root toolchain_root host)
  execute_process(
    COMMAND sh "${source_root}/scripts/fetch-toolchain.sh" --check
      --host "${host}" --root "${toolchain_root}"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE check_rc OUTPUT_VARIABLE check_out ERROR_VARIABLE check_error)
  if(NOT "${check_rc}" STREQUAL "0")
    message(FATAL_ERROR
      "toolchain: required-toolchain-invalid: ${check_error}${check_out}")
  endif()
  message(STATUS "toolchain: pinned host install verified (${host})")
endfunction()

function(run_installer_offline_suite case_script source_root bin_root
    result_name report_name)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}" "-DBIN=${bin_root}"
      -DCASE=offline-all -P "${case_script}"
    RESULT_VARIABLE suite_rc OUTPUT_VARIABLE suite_out ERROR_VARIABLE suite_error)
  set(suite_report "${suite_out}\n${suite_error}")
  set(suite_ok FALSE)
  if("${suite_rc}" STREQUAL "0" AND
     suite_report MATCHES
       "toolchain-case valid-redirect PASSED with safe internal links")
    set(suite_ok TRUE)
  endif()
  set(${result_name} "${suite_ok}" PARENT_SCOPE)
  set(${report_name} "${suite_report}" PARENT_SCOPE)
endfunction()

function(report_architecture_attributes output_root host variant name)
  set(attribute_path "${output_root}/proof/${variant}/${name}.attributes.txt")
  if(NOT EXISTS "${attribute_path}" OR IS_DIRECTORY "${attribute_path}" OR
     IS_SYMLINK "${attribute_path}")
    message(FATAL_ERROR
      "toolchain: diagnostic-architecture-attributes-missing-${variant}-${name}")
  endif()
  file(STRINGS "${attribute_path}" attribute_lines
    REGEX "^[ \t]*Tag_(CPU_name|CPU_arch|ARM_ISA_use|THUMB_ISA_use): ")
  set(normalized_attribute_lines "")
  foreach(attribute_line IN LISTS attribute_lines)
    string(STRIP "${attribute_line}" attribute_line)
    list(APPEND normalized_attribute_lines "${attribute_line}")
  endforeach()
  set(attribute_lines "${normalized_attribute_lines}")
  foreach(expected IN ITEMS
      "Tag_CPU_arch: v4T"
      "Tag_ARM_ISA_use: Yes" "Tag_THUMB_ISA_use: Thumb-1")
    list(FIND attribute_lines "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
        "toolchain: diagnostic-architecture-attribute-mismatch-${variant}-${name}")
    endif()
  endforeach()
  string(JOIN "; " attribute_summary ${attribute_lines})
  message(STATUS
    "toolchain: ${host}/${variant}/${name} attributes: ${attribute_summary}")
endfunction()

function(assert_spike_outputs source_root output_root host runner)
  set(receipt_path "${output_root}/proof/build-receipt.json")
  if(NOT EXISTS "${receipt_path}" OR IS_DIRECTORY "${receipt_path}" OR
     IS_SYMLINK "${receipt_path}")
    message(FATAL_ERROR "toolchain: diagnostic-build-receipt-missing")
  endif()
  file(READ "${receipt_path}" build_receipt)
  foreach(field IN ITEMS format host toolchain_release toolchain_check
      signature_scan output_names smoke_build_seconds first_download_seconds
      first_extract_seconds)
    string(JSON receipt_${field} ERROR_VARIABLE field_error
      GET "${build_receipt}" "${field}")
    if(NOT "${field_error}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "toolchain: diagnostic-receipt-field-missing-${field}")
    endif()
  endforeach()
  if(NOT receipt_format STREQUAL "gamebloke.arm-diagnostic-run/1" OR
     NOT receipt_host STREQUAL host OR
     NOT receipt_toolchain_release STREQUAL "15.2.1-1.1" OR
     NOT receipt_toolchain_check STREQUAL "passed" OR
     NOT receipt_signature_scan STREQUAL "passed" OR
     NOT receipt_smoke_build_seconds MATCHES "^[0-9]+$" OR
     receipt_smoke_build_seconds GREATER 180 OR
     NOT receipt_first_download_seconds MATCHES "^[0-9]+$" OR
     NOT receipt_first_extract_seconds MATCHES "^[0-9]+$")
    message(FATAL_ERROR "toolchain: diagnostic-build-receipt-invalid")
  endif()

  file(GLOB rom_entries LIST_DIRECTORIES TRUE "${output_root}/roms/*")
  list(SORT rom_entries)
  set(expected_roms "${output_root}/roms/smoke-broken.gba"
    "${output_root}/roms/smoke.gba")
  if(NOT "${rom_entries}" STREQUAL "${expected_roms}")
    message(FATAL_ERROR "toolchain: diagnostic-rom-output-set-mismatch")
  endif()

  string(JSON output_count LENGTH "${receipt_output_names}")
  if(NOT output_count EQUAL 2)
    message(FATAL_ERROR "toolchain: diagnostic-receipt-output-count")
  endif()
  set(recorded_names "")
  foreach(index RANGE 0 1)
    string(JSON name GET "${receipt_output_names}" ${index})
    list(APPEND recorded_names "${name}")
  endforeach()
  list(SORT recorded_names)
  if(NOT "${recorded_names}" STREQUAL "smoke-broken.gba;smoke.gba")
    message(FATAL_ERROR "toolchain: diagnostic-receipt-output-name-mismatch")
  endif()

  foreach(variant IN ITEMS smoke smoke-broken)
    set(rom_name "${variant}.gba")
    set(rom_path "${output_root}/roms/${rom_name}")
    set(variant_receipt_path
      "${output_root}/proof/${variant}/build-receipt.json")
    if(NOT EXISTS "${rom_path}" OR IS_DIRECTORY "${rom_path}" OR
       IS_SYMLINK "${rom_path}" OR NOT EXISTS "${variant_receipt_path}")
      message(FATAL_ERROR "toolchain: diagnostic-output-missing-${variant}")
    endif()
    foreach(attribute_name IN ITEMS startup smoke elf)
      report_architecture_attributes(
        "${output_root}" "${host}" "${variant}" "${attribute_name}")
    endforeach()
    file(READ "${variant_receipt_path}" variant_receipt)
    string(JSON variant_name GET "${variant_receipt}" variant)
    string(JSON variant_host GET "${variant_receipt}" host)
    string(JSON variant_sha GET "${variant_receipt}" rom_sha256)
    string(JSON broken GET "${variant_receipt}" broken)
    if(NOT variant_name STREQUAL variant OR NOT variant_host STREQUAL host)
      message(FATAL_ERROR "toolchain: diagnostic-variant-identity-${variant}")
    endif()
    if(variant STREQUAL "smoke" AND NOT broken STREQUAL "0")
      message(FATAL_ERROR "toolchain: clean-diagnostic-marked-broken")
    elseif(variant STREQUAL "smoke-broken" AND NOT broken STREQUAL "1")
      message(FATAL_ERROR "toolchain: broken-diagnostic-marked-clean")
    endif()
    file(SHA256 "${rom_path}" actual_sha)
    if(NOT actual_sha STREQUAL variant_sha)
      message(FATAL_ERROR "toolchain: diagnostic-output-digest-mismatch-${variant}")
    endif()

    if(variant STREQUAL "smoke")
      set(expected_state pass)
    else()
      set(expected_state fail)
    endif()
    execute_process(
      COMMAND "${runner}" --rom "${rom_path}" --cycles 1000
      RESULT_VARIABLE runner_rc OUTPUT_VARIABLE runner_json
      ERROR_VARIABLE runner_error)
    if(NOT "${runner_rc}" STREQUAL "1" OR "${runner_json}" STREQUAL "")
      message(FATAL_ERROR
        "toolchain: raw-runner-exit-mismatch-${variant}: ${runner_error}${runner_json}")
    endif()
    string(JSON runner_status ERROR_VARIABLE status_error
      GET "${runner_json}" status)
    string(JSON runner_reason ERROR_VARIABLE reason_error
      GET "${runner_json}" reason)
    string(JSON runner_termination ERROR_VARIABLE termination_error
      GET "${runner_json}" termination)
    string(JSON runner_ran ERROR_VARIABLE ran_error
      GET "${runner_json}" fixture ran)
    string(JSON semantic_state ERROR_VARIABLE state_error
      GET "${runner_json}" semantic state)
    string(JSON semantic_case ERROR_VARIABLE case_error
      GET "${runner_json}" semantic case_id)
    foreach(error IN ITEMS status_error reason_error termination_error
        ran_error state_error case_error)
      if(NOT "${${error}}" STREQUAL "NOTFOUND")
        message(FATAL_ERROR "toolchain: raw-runner-json-invalid-${variant}")
      endif()
    endforeach()
    if(NOT runner_status STREQUAL "GUEST_STOP" OR
       NOT runner_reason STREQUAL "unregistered-content" OR
       NOT runner_termination STREQUAL "unregistered-content" OR
       NOT runner_ran OR NOT semantic_state STREQUAL expected_state OR
       NOT semantic_case STREQUAL "20481")
      message(FATAL_ERROR "toolchain: raw-runner-observation-mismatch-${variant}")
    endif()
    message(STATUS
      "toolchain: ${variant} raw=GUEST_STOP/unregistered-content semantic=${semantic_state}/20481")
  endforeach()

  message(STATUS
    "toolchain: ${host} smoke-build=${receipt_smoke_build_seconds}s setup-download=${receipt_first_download_seconds}s extract=${receipt_first_extract_seconds}s (workflow cap=180s)")
endfunction()

if(DEFINED CASE)
  foreach(required IN ITEMS SRC)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
      message(FATAL_ERROR "toolchain: missing-${required}")
    endif()
  endforeach()
  file(REAL_PATH "${SRC}" source_root)
  if(CASE STREQUAL "preset-contract")
    check_preset_contract("${source_root}")
  elseif(CASE STREQUAL "check-toolchain")
    foreach(required IN ITEMS TOOLCHAIN_ROOT HOST)
      if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "toolchain: missing-${required}")
      endif()
    endforeach()
    check_toolchain("${source_root}" "${TOOLCHAIN_ROOT}" "${HOST}")
  elseif(CASE STREQUAL "verify-spike")
    foreach(required IN ITEMS OUT HOST RUNNER)
      if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "toolchain: missing-${required}")
      endif()
    endforeach()
    assert_spike_outputs("${source_root}" "${OUT}" "${HOST}" "${RUNNER}")
  elseif(CASE STREQUAL "offline-controls")
    if(NOT DEFINED BIN OR "${BIN}" STREQUAL "")
      message(FATAL_ERROR "toolchain: missing-BIN")
    endif()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}" "-DBIN=${BIN}"
        -DCASE=host-detection-controls
        -P "${source_root}/tests/gates/toolchain_case.cmake"
      RESULT_VARIABLE host_controls_rc OUTPUT_VARIABLE host_controls_out
      ERROR_VARIABLE host_controls_error)
    if(NOT "${host_controls_rc}" STREQUAL "0" OR
       NOT "${host_controls_out}${host_controls_error}" MATCHES
         "toolchain-case host-detection-controls PASSED")
      message(FATAL_ERROR
        "toolchain: host-detection-controls-failed: ${host_controls_error}${host_controls_out}")
    endif()
    message("${host_controls_out}")

    set(installer_case_script "${source_root}/tests/gates/toolchain_case.cmake")
    run_installer_offline_suite("${installer_case_script}" "${source_root}"
      "${BIN}" installer_offline_ok installer_offline_report)
    if(NOT installer_offline_ok)
      message(FATAL_ERROR
        "toolchain: installer-offline-suite-failed-or-missing-final-success: ${installer_offline_report}")
    endif()
    message("${installer_offline_report}")
    message(STATUS "toolchain: installer offline refusal suite passed")

    set(installer_failure_dir "${BIN}/controls/installer-propagation")
    file(MAKE_DIRECTORY "${installer_failure_dir}")
    set(installer_failure_script "${installer_failure_dir}/controlled-failure.cmake")
    file(WRITE "${installer_failure_script}"
      "message(STATUS \"toolchain-case valid-redirect PASSED with safe internal links\")\nmessage(FATAL_ERROR \"toolchain-case controlled installer refusal failure\")\n")
    run_installer_offline_suite("${installer_failure_script}" "${source_root}"
      "${BIN}" installer_failure_ok installer_failure_report)
    if(installer_failure_ok OR
       NOT installer_failure_report MATCHES
         "toolchain-case controlled installer refusal failure")
      message(FATAL_ERROR
        "toolchain: installer-refusal-failure-control-missed: ${installer_failure_report}")
    endif()
    message(STATUS "toolchain: installer refusal harness nonzero is propagated")

    execute_process(
      COMMAND "${CMAKE_COMMAND}" -DCASE=offline-controls
        "-DSRC=${source_root}" "-DBIN=${BIN}"
        -P "${source_root}/tests/gates/toolchain_spike.cmake"
      RESULT_VARIABLE offline_rc OUTPUT_VARIABLE offline_out
      ERROR_VARIABLE offline_error)
    if(NOT "${offline_rc}" STREQUAL "0" OR
       NOT "${offline_out}${offline_error}" MATCHES
         "toolchain-spike: offline-controls ok")
      message(FATAL_ERROR
        "toolchain: offline-controls-failed: ${offline_error}${offline_out}")
    endif()
    message("${offline_out}")
    message(STATUS "toolchain: clean and broken offline controls observed")
  else()
    message(FATAL_ERROR "toolchain: unsupported-case: ${CASE}")
  endif()
  return()
endif()

if(NOT GBL_LANE STREQUAL "diagnostics")
  message(FATAL_ERROR "toolchain: diagnostics-lane-required")
endif()

set(GBL_TOOLCHAIN_DRIVER "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}")
if(GBL_DIAGNOSTIC_STAGE STREQUAL "proof")
  gbl_add_test(
    NAME diagnostics.final.outputs
    LABELS diagnostics gate integration
    COMMAND ${GBL_TOOLCHAIN_DRIVER} -DCASE=final-host-set
      "-DBIN=${GBL_FINAL_PROOF_ROOT}"
      "-DPROOF_GROUP=${GBL_DIAGNOSTIC_GROUP}"
      "-DTOOLCHAIN_ROOT=${GBL_TOOLCHAIN_ROOT_ABSOLUTE}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/toolchain_spike.cmake")
  gbl_add_test(
    NAME diagnostics.final.budget
    LABELS control diagnostics
    COMMAND ${GBL_TOOLCHAIN_DRIVER} -DCASE=final-budget-controls
      "-DBIN=${CMAKE_BINARY_DIR}/final-budget-controls"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/toolchain_spike.cmake")
endif()
gbl_add_test(
  NAME diagnostics.presets
  LABELS diagnostics gate
  COMMAND ${GBL_TOOLCHAIN_DRIVER} -DCASE=preset-contract
    -P "${CMAKE_SOURCE_DIR}/tests/toolchain.cmake")
gbl_add_test(
  NAME diagnostics.toolchain
  LABELS diagnostics gate
  COMMAND ${GBL_TOOLCHAIN_DRIVER} -DCASE=check-toolchain
    "-DTOOLCHAIN_ROOT=${GBL_TOOLCHAIN_ROOT_ABSOLUTE}"
    "-DHOST=${GBL_DIAGNOSTIC_HOST}"
    -P "${CMAKE_SOURCE_DIR}/tests/toolchain.cmake")
if(NOT GBL_DIAGNOSTIC_GROUP STREQUAL "thumb")
gbl_add_test(
  NAME diagnostics.spike
  LABELS diagnostics gate integration
  COMMAND ${GBL_TOOLCHAIN_DRIVER} -DCASE=verify-spike
    "-DOUT=${GBL_DIAGNOSTIC_OUTPUT_ROOT}"
    "-DHOST=${GBL_DIAGNOSTIC_HOST}"
    "-DRUNNER=$<TARGET_FILE:gamebloke-run>"
    -P "${CMAKE_SOURCE_DIR}/tests/toolchain.cmake")
endif()
gbl_add_test(
  NAME toolchain.offline
  LABELS control diagnostics
  COMMAND ${GBL_TOOLCHAIN_DRIVER} -DCASE=offline-controls
    "-DBIN=${CMAKE_SOURCE_DIR}/build/phase05-spike"
    -P "${CMAKE_SOURCE_DIR}/tests/toolchain.cmake")
