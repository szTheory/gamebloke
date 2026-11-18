cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase7-order: missing-${required}")
  endif()
  if(NOT IS_ABSOLUTE "${${required}}")
    message(FATAL_ERROR "phase7-order: nonabsolute-${required}")
  endif()
endforeach()
if(NOT IS_DIRECTORY "${SRC}" OR NOT IS_DIRECTORY "${BIN}")
  message(FATAL_ERROR "phase7-order: source-or-build-directory-missing")
endif()
file(REAL_PATH "${SRC}" GBL_PHASE7_SRC)
file(REAL_PATH "${BIN}" GBL_PHASE7_BIN)
find_program(CTEST_PROGRAM NAMES ctest REQUIRED)

set(GBL_PHASE7_SETUP_NAMES
  time.events.order
  time.events.boundary
  time.events.refusal
  timer0.model.boundary
  phase7.selection.control)
set(GBL_PHASE7_REQUIRED_NAMES
  phase7.order
  timer.costs
  timer.costs.contract
  timer.devices
  timer0.guest.tracer
  bus.timing)
# These checks execute the rights-restricted Timer 0 guest and are local-only.
# phase7.order runs in CI/ASAN, where these names must remain absent.
set(GBL_PHASE7_LOCAL_ONLY_NAMES
  timer0.guest.cli
  bus.time_authority.clean
  bus.time_authority.control
  determinism.repeat.in_process
  determinism.interleave)

function(gbl_phase7_json_has_value json test_index property wanted output_var)
  set(found FALSE)
  string(JSON property_count ERROR_VARIABLE property_error LENGTH
    "${json}" tests ${test_index} properties)
  if(NOT "${property_error}" STREQUAL "NOTFOUND")
    set(${output_var} FALSE PARENT_SCOPE)
    return()
  endif()
  if(property_count GREATER 0)
    math(EXPR property_last "${property_count} - 1")
    foreach(property_index RANGE 0 ${property_last})
      string(JSON property_name GET "${json}" tests ${test_index}
        properties ${property_index} name)
      if(NOT property_name STREQUAL property)
        continue()
      endif()
      string(JSON value_type TYPE "${json}" tests ${test_index}
        properties ${property_index} value)
      if(value_type STREQUAL "ARRAY")
        string(JSON value_count LENGTH "${json}" tests ${test_index}
          properties ${property_index} value)
        if(value_count GREATER 0)
          math(EXPR value_last "${value_count} - 1")
          foreach(value_index RANGE 0 ${value_last})
            string(JSON value GET "${json}" tests ${test_index}
              properties ${property_index} value ${value_index})
            if(value STREQUAL wanted)
              set(found TRUE)
            endif()
          endforeach()
        endif()
      elseif(value_type STREQUAL "STRING")
        string(JSON value GET "${json}" tests ${test_index}
          properties ${property_index} value)
        if(value STREQUAL wanted)
          set(found TRUE)
        endif()
      endif()
    endforeach()
  endif()
  set(${output_var} "${found}" PARENT_SCOPE)
endfunction()

function(gbl_phase7_find_test json wanted output_index output_found)
  set(found FALSE)
  set(found_index -1)
  string(JSON test_count ERROR_VARIABLE test_error LENGTH "${json}" tests)
  if("${test_error}" STREQUAL "NOTFOUND" AND test_count GREATER 0)
    math(EXPR test_last "${test_count} - 1")
    foreach(test_index RANGE 0 ${test_last})
      string(JSON test_name GET "${json}" tests ${test_index} name)
      if(test_name STREQUAL wanted)
        set(found TRUE)
        set(found_index ${test_index})
        break()
      endif()
    endforeach()
  endif()
  set(${output_index} "${found_index}" PARENT_SCOPE)
  set(${output_found} "${found}" PARENT_SCOPE)
endfunction()

function(gbl_phase7_graph_reason json output_reason)
  set(reason "")
  string(JSON kind ERROR_VARIABLE kind_error GET "${json}" kind)
  if(NOT "${kind_error}" STREQUAL "NOTFOUND" OR
      NOT kind STREQUAL "ctestInfo")
    set(${output_reason} malformed PARENT_SCOPE)
    return()
  endif()
  string(JSON test_count ERROR_VARIABLE test_error LENGTH "${json}" tests)
  if(NOT "${test_error}" STREQUAL "NOTFOUND")
    set(${output_reason} malformed PARENT_SCOPE)
    return()
  endif()

  foreach(local_only_name IN LISTS GBL_PHASE7_LOCAL_ONLY_NAMES)
    gbl_phase7_find_test("${json}" "${local_only_name}" ignored_index
      local_only_found)
    if(local_only_found)
      set(${output_reason} local-only-registration PARENT_SCOPE)
      return()
    endif()
  endforeach()

  set(actual_setup_names)
  if(test_count GREATER 0)
    math(EXPR test_last "${test_count} - 1")
    foreach(test_index RANGE 0 ${test_last})
      string(JSON test_name GET "${json}" tests ${test_index} name)
      gbl_phase7_json_has_value("${json}" ${test_index} FIXTURES_SETUP
        phase7_fast is_setup)
      if(is_setup)
        list(APPEND actual_setup_names "${test_name}")
      endif()
    endforeach()
  endif()
  set(expected_setup_names ${GBL_PHASE7_SETUP_NAMES})
  list(SORT actual_setup_names)
  list(SORT expected_setup_names)
  if(NOT "${actual_setup_names}" STREQUAL "${expected_setup_names}")
    set(${output_reason} setup-set PARENT_SCOPE)
    return()
  endif()

  foreach(required_name IN LISTS GBL_PHASE7_REQUIRED_NAMES)
    gbl_phase7_find_test("${json}" "${required_name}" ignored_index
      required_found)
    if(NOT required_found)
      set(${output_reason} missing-registration PARENT_SCOPE)
      return()
    endif()
  endforeach()

  foreach(test_index RANGE 0 ${test_last})
    string(JSON test_name GET "${json}" tests ${test_index} name)
    gbl_phase7_json_has_value("${json}" ${test_index} FIXTURES_SETUP
      phase7_fast is_setup)
    gbl_phase7_json_has_value("${json}" ${test_index} FIXTURES_REQUIRED
      phase7_fast requires_fast)
    if(is_setup AND requires_fast)
      set(${output_reason} self-cycle PARENT_SCOPE)
      return()
    endif()
    if(test_name MATCHES "^(cpu|runner|determinism|timer|timer0)[.]"
        OR test_name MATCHES "^time[.]events[.]"
        OR test_name STREQUAL "bus.timing"
        OR test_name MATCHES "^bus[.]time_authority[.]"
        OR test_name STREQUAL "phase7.order")
      if(NOT is_setup AND NOT requires_fast)
        set(${output_reason} missing-predecessor PARENT_SCOPE)
        return()
      endif()
    endif()
    if(test_name STREQUAL "inventory.junit")
      if(requires_fast)
        set(${output_reason} self-cycle PARENT_SCOPE)
        return()
      endif()
    endif()
    gbl_phase7_json_has_value("${json}" ${test_index} LABELS post
      has_post_label)
    if(has_post_label AND requires_fast)
      set(${output_reason} self-cycle PARENT_SCOPE)
      return()
    endif()
  endforeach()

  gbl_phase7_find_test("${json}" runner.relocation_admitted
    relocation_index relocation_found)
  if(relocation_found)
    gbl_phase7_json_has_value("${json}" ${relocation_index}
      FIXTURES_REQUIRED diagnostic_execution has_diagnostic_fixture)
    if(NOT has_diagnostic_fixture)
      set(${output_reason} replaced-existing-fixture PARENT_SCOPE)
      return()
    endif()
  endif()
  set(${output_reason} "" PARENT_SCOPE)
endfunction()

function(gbl_phase7_write_scratch_project mode source_dir build_dir
    sentinel_file)
  file(MAKE_DIRECTORY "${source_dir}")
  if(mode STREQUAL "fail")
    set(setup_script "${source_dir}/fixture_setup.cmake")
    file(WRITE "${setup_script}"
      "message(FATAL_ERROR \"phase7-order: intentional setup fault\")\n")
  else()
    set(setup_script "${source_dir}/fixture_setup.cmake")
    file(WRITE "${setup_script}"
      "message(STATUS \"phase7-order: setup succeeded\")\n")
  endif()
  set(sentinel_script "${source_dir}/write_sentinel.cmake")
  file(WRITE "${sentinel_script}"
    "file(WRITE \"${sentinel_file}\" \"ran\")\n")
  set(project_text
    "cmake_minimum_required(VERSION 3.25)\nproject(phase7_order_${mode} NONE)\nenable_testing()\nadd_test(NAME cheap_setup COMMAND \"${CMAKE_COMMAND}\" -P \"${setup_script}\")\nset_tests_properties(cheap_setup PROPERTIES FIXTURES_SETUP phase7_fast)\nadd_test(NAME long_sentinel COMMAND \"${CMAKE_COMMAND}\" -P \"${sentinel_script}\")\nset_tests_properties(long_sentinel PROPERTIES FIXTURES_REQUIRED phase7_fast)\n")
  file(WRITE "${source_dir}/CMakeLists.txt" "${project_text}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${source_dir}" -B "${build_dir}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
  if(NOT "${configure_result}" STREQUAL "0")
    message(FATAL_ERROR
      "phase7-order: scratch-configure-${mode}\n${configure_output}${configure_error}")
  endif()
endfunction()

function(gbl_phase7_check_scratch_graph scratch_root)
  set(fail_source "${scratch_root}/fail/source")
  set(fail_build "${scratch_root}/fail/build")
  set(fail_sentinel "${scratch_root}/fail/long-case.ran")
  gbl_phase7_write_scratch_project(fail "${fail_source}" "${fail_build}"
    "${fail_sentinel}")
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${fail_build}"
      --output-on-failure -R "^long_sentinel$"
    RESULT_VARIABLE fail_result
    OUTPUT_VARIABLE fail_output ERROR_VARIABLE fail_error)
  set(fail_combined "${fail_output}${fail_error}")
  if("${fail_result}" STREQUAL "0" OR EXISTS "${fail_sentinel}" OR
      NOT fail_combined MATCHES "long_sentinel" OR
      NOT fail_combined MATCHES "Not Run")
    message(FATAL_ERROR
      "phase7-order: failing-setup-did-not-block-sentinel\n${fail_combined}")
  endif()

  set(pass_source "${scratch_root}/pass/source")
  set(pass_build "${scratch_root}/pass/build")
  set(pass_sentinel "${scratch_root}/pass/long-case.ran")
  gbl_phase7_write_scratch_project(pass "${pass_source}" "${pass_build}"
    "${pass_sentinel}")
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${pass_build}"
      --output-on-failure -R "^long_sentinel$"
    RESULT_VARIABLE pass_result
    OUTPUT_VARIABLE pass_output ERROR_VARIABLE pass_error)
  if(NOT "${pass_result}" STREQUAL "0" OR NOT EXISTS "${pass_sentinel}")
    message(FATAL_ERROR
      "phase7-order: successful-setup-did-not-run-sentinel\n${pass_output}${pass_error}")
  endif()
endfunction()

file(READ "${GBL_PHASE7_SRC}/tests/gates/phase7_selection.cmake"
  selection_gate)
if(NOT selection_gate MATCHES
    "--fixture-exclude-any[ \t\r\n]+phase7_fast[ \t\r\n]+-R")
  message(FATAL_ERROR "phase7-order: selector-fixture-exclusion-missing")
endif()

execute_process(
  COMMAND "${CTEST_PROGRAM}" --test-dir "${GBL_PHASE7_BIN}"
    --show-only=json-v1 -R
    "^(cpu[.]|runner[.]|determinism[.]|timer[.]|timer0[.]|time[.]events[.]|bus[.]timing$|bus[.]time_authority[.]|phase7[.]order$|phase7[.]selection[.]control$|inventory[.]junit$)"
  RESULT_VARIABLE ctest_result OUTPUT_VARIABLE ctest_json
  ERROR_VARIABLE ctest_error)
if(NOT "${ctest_result}" STREQUAL "0")
  message(FATAL_ERROR "phase7-order: ctest-discovery-failed: ${ctest_error}")
endif()
gbl_phase7_graph_reason("${ctest_json}" graph_reason)
if(NOT graph_reason STREQUAL "")
  message(FATAL_ERROR "phase7-order: ${graph_reason}")
endif()

# Negative control: prove the hosted graph gate rejects the local-only guest
# if a future lane change accidentally registers it alongside CI/ASan tests.
gbl_phase7_find_test("${ctest_json}" phase7.selection.control
  local_only_control_index local_only_control_found)
if(NOT local_only_control_found)
  message(FATAL_ERROR "phase7-order: missing-local-only-control-anchor")
endif()
string(JSON local_only_control_json SET "${ctest_json}" tests
  ${local_only_control_index} name "\"timer0.guest.cli\"")
gbl_phase7_graph_reason("${local_only_control_json}" local_only_control_reason)
if(NOT local_only_control_reason STREQUAL "local-only-registration")
  message(FATAL_ERROR
    "phase7-order: local-only-control-wrong-result: ${local_only_control_reason}")
endif()

set(scratch_root "${GBL_PHASE7_BIN}/phase7-order-controls")
file(REMOVE_RECURSE "${scratch_root}")
file(MAKE_DIRECTORY "${scratch_root}")
file(WRITE "${scratch_root}/actual-ctest-inventory.json" "${ctest_json}")
gbl_phase7_find_test("${ctest_json}" timer.costs timer_costs_index
  timer_costs_found)
if(NOT timer_costs_found)
  message(FATAL_ERROR "phase7-order: missing-registration")
endif()
string(JSON property_count ERROR_VARIABLE property_error LENGTH
  "${ctest_json}" tests ${timer_costs_index} properties)
if(NOT "${property_error}" STREQUAL "NOTFOUND")
  message(FATAL_ERROR "phase7-order: malformed-properties")
endif()
set(required_property_index -1)
if(property_count GREATER 0)
  math(EXPR property_last "${property_count} - 1")
  foreach(property_index RANGE 0 ${property_last})
    string(JSON property_name GET "${ctest_json}" tests ${timer_costs_index}
      properties ${property_index} name)
    if(property_name STREQUAL "FIXTURES_REQUIRED")
      gbl_phase7_json_has_value("${ctest_json}" ${timer_costs_index}
        FIXTURES_REQUIRED phase7_fast has_timer_predecessor)
      if(has_timer_predecessor)
        set(required_property_index ${property_index})
      endif()
    endif()
  endforeach()
endif()
if(required_property_index LESS 0)
  message(FATAL_ERROR "phase7-order: missing-predecessor")
endif()
string(JSON missing_predecessor_json REMOVE "${ctest_json}" tests
  ${timer_costs_index} properties ${required_property_index})
file(WRITE "${scratch_root}/missing-predecessor-inventory.json"
  "${missing_predecessor_json}")
gbl_phase7_graph_reason("${missing_predecessor_json}" missing_reason)
if(NOT missing_reason STREQUAL "missing-predecessor")
  message(FATAL_ERROR
    "phase7-order: missing-predecessor-control-wrong-result: ${missing_reason}")
endif()

gbl_phase7_check_scratch_graph("${scratch_root}/scratch")
message(STATUS
  "phase7-order: exact setup set, required predecessors, and causal controls passed")
