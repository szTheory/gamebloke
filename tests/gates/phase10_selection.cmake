cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase10-selection: missing-${required}")
  endif()
endforeach()
if(NOT CASE MATCHES "^(verify|control|order)$")
  message(FATAL_ERROR "phase10-selection: invalid-case")
endif()

get_filename_component(GBL_PHASE10_DEFAULT_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
get_filename_component(GBL_PHASE10_BIN "${BIN}" REALPATH
  BASE_DIR "${CMAKE_BINARY_DIR}")
if(NOT IS_DIRECTORY "${GBL_PHASE10_DEFAULT_ROOT}" OR
    NOT IS_DIRECTORY "${GBL_PHASE10_BIN}")
  message(FATAL_ERROR "phase10-selection: source-or-build-directory-missing")
endif()
get_filename_component(GBL_PHASE10_REQUESTED_SRC "${SRC}" REALPATH
  BASE_DIR "${GBL_PHASE10_DEFAULT_ROOT}")
set(GBL_PHASE10_CACHE "${GBL_PHASE10_BIN}/CMakeCache.txt")
if(NOT EXISTS "${GBL_PHASE10_CACHE}")
  message(FATAL_ERROR "phase10-selection: build-cache-missing")
endif()
file(STRINGS "${GBL_PHASE10_CACHE}" GBL_PHASE10_CACHE_HOME
  REGEX "^CMAKE_HOME_DIRECTORY:INTERNAL=")
list(LENGTH GBL_PHASE10_CACHE_HOME GBL_PHASE10_CACHE_HOME_COUNT)
if(NOT GBL_PHASE10_CACHE_HOME_COUNT EQUAL 1)
  message(FATAL_ERROR "phase10-selection: build-source-missing")
endif()
string(REGEX REPLACE "^CMAKE_HOME_DIRECTORY:INTERNAL=" ""
  GBL_PHASE10_CACHE_SOURCE "${GBL_PHASE10_CACHE_HOME}")
file(REAL_PATH "${GBL_PHASE10_CACHE_SOURCE}" GBL_PHASE10_CACHE_SOURCE)
if(NOT GBL_PHASE10_CACHE_SOURCE STREQUAL GBL_PHASE10_REQUESTED_SRC)
  message(FATAL_ERROR "phase10-selection: source-build-mismatch")
endif()
set(GBL_PHASE10_SRC "${GBL_PHASE10_CACHE_SOURCE}")
set(GBL_PHASE10_EXPECTED_GATE "${GBL_PHASE10_SRC}/tests/gates/phase10_selection.cmake")
if(NOT EXISTS "${GBL_PHASE10_EXPECTED_GATE}")
  message(FATAL_ERROR "phase10-selection: source-gate-missing")
endif()
file(REAL_PATH "${GBL_PHASE10_EXPECTED_GATE}" GBL_PHASE10_EXPECTED_GATE)
file(REAL_PATH "${CMAKE_CURRENT_LIST_FILE}" GBL_PHASE10_GATE)
if(NOT GBL_PHASE10_GATE STREQUAL GBL_PHASE10_EXPECTED_GATE)
  message(FATAL_ERROR "phase10-selection: script-source-mismatch")
endif()
find_program(CTEST_PROGRAM NAMES ctest REQUIRED)
find_program(CMAKE_PROGRAM NAMES cmake REQUIRED)

# These are the exact inexpensive Phase 10 predecessors. Long frame, runner,
# DMA, and synthetic replay checks are intentionally outside this set.
set(GBL_PHASE10_EXPECTED
  phase10.policy
  phase10.policy.control
  ppu.boundary
  ppu.registers
  ppu.refusal
  phase10.selection.control)
set(GBL_PHASE10_REGEX
  "^(ppu[.](boundary|registers|refusal)|phase10[.](policy([.]control)?|selection[.]control))$")
set(GBL_PHASE10_COMMON_INVENTORY_ROWS
  "cpu.ppu.dma core,cpu,ppu,unit"
  "cpu.ppu.irq core,cpu,ppu,unit"
  "cpu.ppu.latch core,cpu,ppu,unit"
  "cpu.ppu.refusal core,cpu,ppu,unit"
  "determinism.phase10 determinism,gate,integration"
  "determinism.phase10.controls determinism,gate,integration"
  "dma.ppu.order core,dma,ppu,unit"
  "dma.ppu.refusal core,dma,ppu,unit"
  "dma.ppu.triggers core,dma,ppu,unit"
  "phase10.policy gate,ppu,profile,time"
  "phase10.policy.control control,ppu,profile,time"
  "phase10.selection.control gate,ppu,time"
  "phase10.selection.order gate,ppu,time"
  "phase10.selection.verify gate,ppu,time"
  "ppu.boundary bus,core,ppu,unit"
  "ppu.controls bus,core,ppu,unit"
  "ppu.frame bus,core,integration,ppu"
  "ppu.latch bus,core,ppu,unit"
  "ppu.refusal bus,core,ppu,unit"
  "ppu.registers bus,core,ppu,unit"
  "ppu.tracer bus,core,integration,ppu"
  "runner.ppu.observation integration,ppu,runner"
  "runner.ppu.output integration,ppu,runner"
  "runner.ppu.output-controls integration,ppu,runner")
set(GBL_PHASE10_CI_ONLY_INVENTORY_ROWS
  "phase10.cli gate,integration,ppu,runner"
  "phase10.cli.control control,ppu,runner"
  "result.nc.phase10 control,result"
  "result.pos.phase10 positive,result")

function(gbl_phase10_json_property json test_index wanted property_out found_out)
  set(found FALSE)
  set(value)
  string(JSON property_count ERROR_VARIABLE property_error LENGTH
    "${json}" tests ${test_index} properties)
  if(NOT "${property_error}" STREQUAL "NOTFOUND")
    set(${property_out} "" PARENT_SCOPE)
    set(${found_out} FALSE PARENT_SCOPE)
    return()
  endif()
  if(property_count GREATER 0)
    math(EXPR property_last "${property_count} - 1")
    foreach(property_index RANGE 0 ${property_last})
      string(JSON property_name GET "${json}" tests ${test_index}
        properties ${property_index} name)
      if(NOT property_name STREQUAL wanted)
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
            string(JSON item GET "${json}" tests ${test_index}
              properties ${property_index} value ${value_index})
            list(APPEND value "${item}")
          endforeach()
        endif()
      elseif(value_type STREQUAL "STRING" OR value_type STREQUAL "BOOLEAN")
        string(JSON value GET "${json}" tests ${test_index}
          properties ${property_index} value)
      endif()
      set(found TRUE)
      break()
    endforeach()
  endif()
  set(${property_out} "${value}" PARENT_SCOPE)
  set(${found_out} "${found}" PARENT_SCOPE)
endfunction()

function(gbl_phase10_find_test json wanted index_out found_out)
  set(found FALSE)
  set(found_index -1)
  string(JSON count ERROR_VARIABLE error LENGTH "${json}" tests)
  if("${error}" STREQUAL "NOTFOUND" AND count GREATER 0)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
      string(JSON name GET "${json}" tests ${index} name)
      if(name STREQUAL wanted)
        set(found TRUE)
        set(found_index ${index})
        break()
      endif()
    endforeach()
  endif()
  set(${index_out} "${found_index}" PARENT_SCOPE)
  set(${found_out} "${found}" PARENT_SCOPE)
endfunction()

function(gbl_phase10_expected_command name json index reason_out)
  string(JSON command_count ERROR_VARIABLE command_error LENGTH
    "${json}" tests ${index} command)
  if(NOT "${command_error}" STREQUAL "NOTFOUND" OR command_count LESS 1)
    set(${reason_out} wrong-command PARENT_SCOPE)
    return()
  endif()
  string(JSON command_program GET "${json}" tests ${index} command 0)
  if(name MATCHES "^ppu[.]" )
    set(expected_program "${GBL_PHASE10_BIN}/bus_test${CMAKE_EXECUTABLE_SUFFIX}")
    set(expected_arg "${name}")
  elseif(name MATCHES "^dma[.]" )
    set(expected_program "${GBL_PHASE10_BIN}/dma_test${CMAKE_EXECUTABLE_SUFFIX}")
    string(REPLACE "dma." "" expected_arg "${name}")
  elseif(name MATCHES "^phase10[.](policy([.]control)?|selection[.](control|verify|order))$")
    if(NOT command_count EQUAL 6)
      set(${reason_out} wrong-command PARENT_SCOPE)
      return()
    endif()
    file(REAL_PATH "${command_program}" actual_cmake)
    file(REAL_PATH "${CMAKE_COMMAND}" expected_cmake)
    if(NOT actual_cmake STREQUAL expected_cmake)
      set(${reason_out} wrong-command PARENT_SCOPE)
      return()
    endif()
    if(name STREQUAL "phase10.policy")
      set(expected_gate "${GBL_PHASE10_SRC}/tests/gates/phase10_policy.cmake")
      set(expected_case verify)
    elseif(name STREQUAL "phase10.policy.control")
      set(expected_gate "${GBL_PHASE10_SRC}/tests/gates/phase10_policy.cmake")
      set(expected_case control)
    elseif(name STREQUAL "phase10.selection.order")
      set(expected_gate "${GBL_PHASE10_GATE}")
      set(expected_case order)
    elseif(name STREQUAL "phase10.selection.verify")
      set(expected_gate "${GBL_PHASE10_GATE}")
      set(expected_case verify)
    else()
      set(expected_gate "${GBL_PHASE10_GATE}")
      set(expected_case control)
    endif()
    file(REAL_PATH "${expected_gate}" expected_gate)
    set(expected_args
      "-DSRC=${GBL_PHASE10_SRC}"
      "-DBIN=${GBL_PHASE10_BIN}"
      "-DCASE=${expected_case}"
      "-P"
      "${expected_gate}")
    foreach(command_index RANGE 1 5)
      string(JSON actual_arg GET "${json}" tests ${index}
        command ${command_index})
      math(EXPR expected_index "${command_index} - 1")
      list(GET expected_args ${expected_index} expected_arg)
      if(NOT actual_arg STREQUAL expected_arg)
        if(command_index EQUAL 4 OR command_index EQUAL 5)
          set(${reason_out} wrong-command PARENT_SCOPE)
        else()
          set(${reason_out} wrong-argument PARENT_SCOPE)
        endif()
        return()
      endif()
    endforeach()
    set(${reason_out} "" PARENT_SCOPE)
    return()
  else()
    set(${reason_out} wrong-command PARENT_SCOPE)
    return()
  endif()
  file(REAL_PATH "${command_program}" actual_program)
  file(REAL_PATH "${expected_program}" expected_program)
  if(NOT actual_program STREQUAL expected_program OR NOT command_count EQUAL 2)
    set(${reason_out} wrong-command PARENT_SCOPE)
    return()
  endif()
  string(JSON command_arg GET "${json}" tests ${index} command 1)
  if(NOT command_arg STREQUAL expected_arg)
    set(${reason_out} wrong-argument PARENT_SCOPE)
    return()
  endif()
  set(${reason_out} "" PARENT_SCOPE)
endfunction()

function(gbl_phase10_validate_selection json reason_out)
  string(JSON kind ERROR_VARIABLE kind_error GET "${json}" kind)
  if(NOT "${kind_error}" STREQUAL "NOTFOUND" OR NOT kind STREQUAL "ctestInfo")
    set(${reason_out} malformed PARENT_SCOPE)
    return()
  endif()
  string(JSON count ERROR_VARIABLE count_error LENGTH "${json}" tests)
  if(NOT "${count_error}" STREQUAL "NOTFOUND")
    set(${reason_out} malformed PARENT_SCOPE)
    return()
  endif()
  if(count EQUAL 0)
    set(${reason_out} empty PARENT_SCOPE)
    return()
  endif()
  list(LENGTH GBL_PHASE10_EXPECTED expected_count)
  math(EXPR broad_limit "${expected_count} + 1")
  if(count GREATER broad_limit)
    set(${reason_out} broad PARENT_SCOPE)
    return()
  endif()
  set(actual_names)
  math(EXPR last_index "${count} - 1")
  foreach(index RANGE 0 ${last_index})
    string(JSON name ERROR_VARIABLE name_error GET "${json}" tests ${index} name)
    if(NOT "${name_error}" STREQUAL "NOTFOUND" OR "${name}" STREQUAL "")
      set(${reason_out} malformed PARENT_SCOPE)
      return()
    endif()
    list(FIND actual_names "${name}" duplicate_index)
    if(NOT duplicate_index EQUAL -1)
      set(${reason_out} duplicate PARENT_SCOPE)
      return()
    endif()
    list(APPEND actual_names "${name}")
  endforeach()
  foreach(expected IN LISTS GBL_PHASE10_EXPECTED)
    list(FIND actual_names "${expected}" expected_index)
    if(expected_index EQUAL -1)
      set(${reason_out} missing PARENT_SCOPE)
      return()
    endif()
  endforeach()

  foreach(index RANGE 0 ${last_index})
    string(JSON name GET "${json}" tests ${index} name)
    list(FIND GBL_PHASE10_EXPECTED "${name}" expected_index)
    if(expected_index EQUAL -1)
      set(${reason_out} extra PARENT_SCOPE)
      return()
    endif()
    gbl_phase10_expected_command("${name}" "${json}" ${index} command_reason)
    if(NOT "${command_reason}" STREQUAL "")
      set(${reason_out} "${command_reason}" PARENT_SCOPE)
      return()
    endif()
    gbl_phase10_json_property("${json}" ${index} LABELS labels has_labels)
    if(NOT has_labels)
      set(${reason_out} wrong-label PARENT_SCOPE)
      return()
    endif()
    if(name MATCHES "^ppu[.]" )
      set(expected_labels bus core ppu unit)
    elseif(name MATCHES "^dma[.]" )
      set(expected_labels core dma ppu unit)
    elseif(name STREQUAL "phase10.policy")
      set(expected_labels gate ppu profile time)
    elseif(name STREQUAL "phase10.policy.control")
      set(expected_labels control ppu profile time)
    elseif(name STREQUAL "phase10.selection.control")
      set(expected_labels gate ppu time)
    endif()
    set(actual_labels ${labels})
    list(SORT actual_labels)
    list(SORT expected_labels)
    if(NOT "${actual_labels}" STREQUAL "${expected_labels}")
      set(${reason_out} wrong-label PARENT_SCOPE)
      return()
    endif()
    gbl_phase10_json_property("${json}" ${index} WORKING_DIRECTORY
      working_directory has_working_directory)
    if(NOT has_working_directory)
      set(${reason_out} wrong-working-directory PARENT_SCOPE)
      return()
    endif()
    file(REAL_PATH "${working_directory}" working_directory_real)
    if(NOT working_directory_real STREQUAL GBL_PHASE10_BIN)
      set(${reason_out} wrong-working-directory PARENT_SCOPE)
      return()
    endif()
    gbl_phase10_json_property("${json}" ${index} SKIP_REGULAR_EXPRESSION
      skip_expression has_skip_expression)
    if(has_skip_expression)
      set(${reason_out} skippable PARENT_SCOPE)
      return()
    endif()
    gbl_phase10_json_property("${json}" ${index} DISABLED
      disabled_value has_disabled)
    if(has_disabled AND disabled_value)
      set(${reason_out} disabled PARENT_SCOPE)
      return()
    endif()
    gbl_phase10_json_property("${json}" ${index} FIXTURES_SETUP
      setup_fixtures has_setup)
    if(NOT has_setup OR NOT "phase10_fast" IN_LIST setup_fixtures)
      set(${reason_out} setup-set PARENT_SCOPE)
      return()
    endif()
  endforeach()
  if(NOT count EQUAL expected_count)
    set(${reason_out} extra PARENT_SCOPE)
    return()
  endif()
  set(${reason_out} "" PARENT_SCOPE)
endfunction()

function(gbl_phase10_discover build_dir regex output_var)
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${build_dir}" --show-only=json-v1
      --fixture-exclude-any "^(phase7_fast|phase8_fast|phase9_fast)$" -R "${regex}"
    RESULT_VARIABLE result OUTPUT_VARIABLE json ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "phase10-selection: discovery: ${result}\n${error}")
  endif()
  set(${output_var} "${json}" PARENT_SCOPE)
endfunction()

function(gbl_phase10_write_scratch_project source_dir build_dir)
  file(MAKE_DIRECTORY "${source_dir}")
  set(project_text
    "cmake_minimum_required(VERSION 3.25)\nproject(phase10_selection_scratch NONE)\nenable_testing()\n")
  string(APPEND project_text
    "add_test(NAME ppu.boundary COMMAND \"${GBL_PHASE10_BIN}/bus_test\" ppu.boundary)\n")
  string(APPEND project_text
    "add_test(NAME ppu.registers COMMAND \"${GBL_PHASE10_BIN}/bus_test\" ppu.registers)\n")
  string(APPEND project_text
    "add_test(NAME ppu.refusal COMMAND \"${GBL_PHASE10_BIN}/bus_test\" ppu.refusal)\n")
  string(APPEND project_text
    "add_test(NAME phase10.policy COMMAND \"${CMAKE_PROGRAM}\" -DSRC=${GBL_PHASE10_SRC} -DBIN=${GBL_PHASE10_BIN} -DCASE=verify -P \"${GBL_PHASE10_SRC}/tests/gates/phase10_policy.cmake\")\n")
  string(APPEND project_text
    "add_test(NAME phase10.policy.control COMMAND \"${CMAKE_PROGRAM}\" -DSRC=${GBL_PHASE10_SRC} -DBIN=${GBL_PHASE10_BIN} -DCASE=control -P \"${GBL_PHASE10_SRC}/tests/gates/phase10_policy.cmake\")\n")
  string(APPEND project_text
    "add_test(NAME phase10.selection.control COMMAND \"${CMAKE_PROGRAM}\" -DSRC=${GBL_PHASE10_SRC} -DBIN=${GBL_PHASE10_BIN} -DCASE=control -P \"${GBL_PHASE10_GATE}\")\n")
  string(APPEND project_text
    "add_test(NAME phase10.unrelated COMMAND \"${CMAKE_PROGRAM}\" -E true)\n")
  string(APPEND project_text
    "set_tests_properties(ppu.boundary ppu.registers ppu.refusal PROPERTIES LABELS \"bus;core;ppu;unit\" FIXTURES_SETUP phase10_fast WORKING_DIRECTORY \"${GBL_PHASE10_BIN}\")\n")
  string(APPEND project_text
    "set_tests_properties(phase10.policy PROPERTIES LABELS \"gate;ppu;profile;time\" FIXTURES_SETUP phase10_fast WORKING_DIRECTORY \"${GBL_PHASE10_BIN}\")\n")
  string(APPEND project_text
    "set_tests_properties(phase10.policy.control PROPERTIES LABELS \"control;ppu;profile;time\" FIXTURES_SETUP phase10_fast WORKING_DIRECTORY \"${GBL_PHASE10_BIN}\")\n")
  string(APPEND project_text
    "set_tests_properties(phase10.selection.control PROPERTIES LABELS \"gate;ppu;time\" FIXTURES_SETUP phase10_fast WORKING_DIRECTORY \"${GBL_PHASE10_BIN}\")\n")
  file(WRITE "${source_dir}/CMakeLists.txt" "${project_text}")
  execute_process(
    COMMAND "${CMAKE_PROGRAM}" -S "${source_dir}" -B "${build_dir}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
  if(NOT "${configure_result}" STREQUAL "0")
    message(FATAL_ERROR
      "phase10-selection: scratch-configure\n${configure_output}${configure_error}")
  endif()
endfunction()

function(gbl_phase10_expect_invocation_rejection name expected_marker)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE invocation_result
    OUTPUT_VARIABLE invocation_output ERROR_VARIABLE invocation_error)
  set(invocation_text "${invocation_output}${invocation_error}")
  string(FIND "${invocation_text}" "${expected_marker}" marker_index)
  if("${invocation_result}" STREQUAL "0" OR marker_index EQUAL -1)
    message(FATAL_ERROR
      "phase10-selection-control: ${name}: expected-${expected_marker}; result=${invocation_result}\n${invocation_text}")
  endif()
  message(STATUS "phase10-selection-control: ${name}: causal rejection ok")
endfunction()

function(gbl_phase10_check_selection_controls scratch_root)
  set(source_dir "${scratch_root}/selection/source")
  set(build_dir "${scratch_root}/selection/build")
  gbl_phase10_write_scratch_project("${source_dir}" "${build_dir}")
  gbl_phase10_discover("${build_dir}" "${GBL_PHASE10_REGEX}" clean_json)
  gbl_phase10_validate_selection("${clean_json}" clean_reason)
  if(NOT "${clean_reason}" STREQUAL "")
    message(FATAL_ERROR "phase10-selection-control: clean-twin-${clean_reason}")
  endif()

  set(faults empty missing extra wrong-command wrong-argument wrong-source
    skipped disabled wrong-fixture
    wrong-control-executable wrong-control-script wrong-control-source
    wrong-control-build wrong-control-case wrong-working-directory)
  foreach(fault IN LISTS faults)
    if(fault STREQUAL "empty")
      set(fault_json "{\"kind\":\"ctestInfo\",\"tests\":[]}")
    elseif(fault STREQUAL "missing")
      string(JSON fault_json REMOVE "${clean_json}" tests 0)
    elseif(fault STREQUAL "extra")
      set(fault_json "${clean_json}")
      string(JSON test_count LENGTH "${fault_json}" tests)
      string(JSON extra_test GET "${fault_json}" tests 0)
      string(JSON extra_test SET "${extra_test}" name "\"phase10.unrelated\"")
      string(JSON fault_json SET "${fault_json}" tests ${test_count} "${extra_test}")
    else()
      set(fault_json "${clean_json}")
      if(fault MATCHES "^wrong-control-" OR fault STREQUAL "wrong-source")
        set(target_name phase10.selection.control)
      else()
        set(target_name ppu.boundary)
      endif()
      gbl_phase10_find_test("${fault_json}" "${target_name}" target_index target_found)
      if(NOT target_found)
        message(FATAL_ERROR "phase10-selection-control: target-missing-${target_name}")
      endif()
      if(fault STREQUAL "wrong-command")
        file(MAKE_DIRECTORY "${scratch_root}/missing")
        file(WRITE "${scratch_root}/missing/bus_test" "not a test executable\n")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 0
          "\"${scratch_root}/missing/bus_test\"")
      elseif(fault STREQUAL "wrong-argument")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 1
          "\"wrong.selector\"")
      elseif(fault STREQUAL "skipped")
        string(JSON property_count LENGTH "${fault_json}" tests ${target_index} properties)
        set(skip_property "{\"name\":\"SKIP_REGULAR_EXPRESSION\",\"value\":\"skip\"}")
        string(JSON fault_json SET "${fault_json}" tests ${target_index}
          properties ${property_count} "${skip_property}")
      elseif(fault STREQUAL "disabled")
        string(JSON property_count LENGTH "${fault_json}" tests ${target_index} properties)
        math(EXPR property_last "${property_count} - 1")
        set(disabled_index -1)
        foreach(property_index RANGE 0 ${property_last})
          string(JSON property_name GET "${fault_json}" tests ${target_index}
            properties ${property_index} name)
          if(property_name STREQUAL "DISABLED")
            set(disabled_index ${property_index})
            break()
          endif()
        endforeach()
        if(disabled_index EQUAL -1)
          set(disabled_property "{\"name\":\"DISABLED\",\"value\":true}")
          string(JSON fault_json SET "${fault_json}" tests ${target_index}
            properties ${property_count} "${disabled_property}")
        else()
          string(JSON fault_json SET "${fault_json}" tests ${target_index}
            properties ${disabled_index} value true)
        endif()
      elseif(fault STREQUAL "wrong-fixture")
        string(JSON property_count LENGTH "${fault_json}" tests ${target_index} properties)
        math(EXPR property_last "${property_count} - 1")
        set(fixture_index -1)
        foreach(property_index RANGE 0 ${property_last})
          string(JSON property_name GET "${fault_json}" tests ${target_index}
            properties ${property_index} name)
          if(property_name STREQUAL "FIXTURES_SETUP")
            set(fixture_index ${property_index})
            break()
          endif()
        endforeach()
        if(fixture_index EQUAL -1)
          message(FATAL_ERROR "phase10-selection-control: missing-fixture-property")
        endif()
        string(JSON fault_json SET "${fault_json}" tests ${target_index}
          properties ${fixture_index} value "\"phase10_wrong\"")
      elseif(fault STREQUAL "wrong-control-executable")
        file(MAKE_DIRECTORY "${scratch_root}/missing")
        file(WRITE "${scratch_root}/missing/cmake" "not a CMake executable\n")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 0
          "\"${scratch_root}/missing/cmake\"")
      elseif(fault STREQUAL "wrong-control-script")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 5
          "\"${scratch_root}/missing/phase10_selection.cmake\"")
      elseif(fault STREQUAL "wrong-source" OR fault STREQUAL "wrong-control-source")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 1
          "\"-DSRC=${scratch_root}/foreign\"")
      elseif(fault STREQUAL "wrong-control-build")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 2
          "\"-DBIN=${scratch_root}/foreign-build\"")
      elseif(fault STREQUAL "wrong-control-case")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 3
          "\"-DCASE=verify\"")
      elseif(fault STREQUAL "wrong-working-directory")
        file(MAKE_DIRECTORY "${scratch_root}/wrong-working-directory")
        string(JSON property_count LENGTH "${fault_json}" tests ${target_index} properties)
        math(EXPR property_last "${property_count} - 1")
        foreach(property_index RANGE 0 ${property_last})
          string(JSON property_name GET "${fault_json}" tests ${target_index}
            properties ${property_index} name)
          if(property_name STREQUAL "WORKING_DIRECTORY")
            string(JSON fault_json SET "${fault_json}" tests ${target_index}
              properties ${property_index} value
              "\"${scratch_root}/wrong-working-directory\"")
            break()
          endif()
        endforeach()
      endif()
    endif()
    gbl_phase10_validate_selection("${fault_json}" observed_reason)
    if(fault STREQUAL "wrong-command" OR fault STREQUAL "wrong-control-executable" OR
        fault STREQUAL "wrong-control-script")
      set(expected_reason wrong-command)
      elseif(fault STREQUAL "wrong-argument" OR fault STREQUAL "wrong-source" OR
          fault MATCHES "^wrong-control-(source|build|case)$")
      set(expected_reason wrong-argument)
    elseif(fault STREQUAL "skipped")
      set(expected_reason skippable)
    elseif(fault STREQUAL "disabled")
      set(expected_reason disabled)
    elseif(fault STREQUAL "wrong-fixture")
      set(expected_reason setup-set)
    elseif(fault STREQUAL "wrong-working-directory")
      set(expected_reason wrong-working-directory)
    else()
      set(expected_reason "${fault}")
    endif()
    if(NOT observed_reason STREQUAL expected_reason)
      message(FATAL_ERROR
        "phase10-selection-control: ${fault}: expected-${expected_reason}-got-${observed_reason}")
    endif()
    message(STATUS "phase10-selection-control: ${fault}: causal rejection ok")
  endforeach()

  set(foreign_root "${scratch_root}/foreign-root")
  file(MAKE_DIRECTORY "${foreign_root}")
  gbl_phase10_expect_invocation_rejection(foreign-root "source-build-mismatch"
    "${CMAKE_PROGRAM}" "-DSRC=${foreign_root}" "-DBIN=${GBL_PHASE10_BIN}"
    -DCASE=verify -P "${GBL_PHASE10_GATE}")
  set(foreign_script "${scratch_root}/foreign-script/phase10_selection.cmake")
  get_filename_component(foreign_script_dir "${foreign_script}" DIRECTORY)
  file(MAKE_DIRECTORY "${foreign_script_dir}")
  file(COPY_FILE "${GBL_PHASE10_GATE}" "${foreign_script}")
  gbl_phase10_expect_invocation_rejection(foreign-script "script-source-mismatch"
    "${CMAKE_PROGRAM}" "-DSRC=${GBL_PHASE10_SRC}" "-DBIN=${GBL_PHASE10_BIN}"
    -DCASE=verify -P "${foreign_script}")
  gbl_phase10_expect_invocation_rejection(foreign-build "source-build-mismatch"
    "${CMAKE_PROGRAM}" "-DSRC=${GBL_PHASE10_SRC}" "-DBIN=${build_dir}"
    -DCASE=verify -P "${GBL_PHASE10_GATE}")
  set(empty_build "${scratch_root}/empty-build")
  file(MAKE_DIRECTORY "${empty_build}")
  gbl_phase10_expect_invocation_rejection(empty-build "build-cache-missing"
    "${CMAKE_PROGRAM}" "-DSRC=${GBL_PHASE10_SRC}" "-DBIN=${empty_build}"
    -DCASE=verify -P "${GBL_PHASE10_GATE}")
  message(STATUS "phase10-selection: exact-six selection and root controls passed")
endfunction()

function(gbl_phase10_check_fixture_controls scratch_root)
  set(fail_source "${scratch_root}/order/fail/source")
  set(fail_build "${scratch_root}/order/fail/build")
  set(fail_sentinel "${scratch_root}/order/fail/long-case.ran")
  set(pass_source "${scratch_root}/order/pass/source")
  set(pass_build "${scratch_root}/order/pass/build")
  set(pass_sentinel "${scratch_root}/order/pass/long-case.ran")
  foreach(mode IN ITEMS fail pass)
    if(mode STREQUAL "fail")
      set(source_dir "${fail_source}")
      set(build_dir "${fail_build}")
      set(sentinel "${fail_sentinel}")
      set(setup_body "message(FATAL_ERROR \"phase10-order: intentional setup fault\")")
    else()
      set(source_dir "${pass_source}")
      set(build_dir "${pass_build}")
      set(sentinel "${pass_sentinel}")
      set(setup_body "message(STATUS \"phase10-order: setup succeeded\")")
    endif()
    file(MAKE_DIRECTORY "${source_dir}")
    file(WRITE "${source_dir}/fixture_setup.cmake" "${setup_body}\n")
    file(WRITE "${source_dir}/write_sentinel.cmake"
      "file(WRITE \"${sentinel}\" \"ran\")\n")
    file(WRITE "${source_dir}/CMakeLists.txt"
      "cmake_minimum_required(VERSION 3.25)\nproject(phase10_fixture_${mode} NONE)\nenable_testing()\nadd_test(NAME cheap_setup COMMAND \"${CMAKE_PROGRAM}\" -P \"${source_dir}/fixture_setup.cmake\")\nset_tests_properties(cheap_setup PROPERTIES FIXTURES_SETUP phase10_fast)\nadd_test(NAME long_sentinel COMMAND \"${CMAKE_PROGRAM}\" -P \"${source_dir}/write_sentinel.cmake\")\nset_tests_properties(long_sentinel PROPERTIES FIXTURES_REQUIRED phase10_fast)\n")
    execute_process(
      COMMAND "${CMAKE_PROGRAM}" -S "${source_dir}" -B "${build_dir}"
      RESULT_VARIABLE configure_result
      OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
    if(NOT "${configure_result}" STREQUAL "0")
      message(FATAL_ERROR
        "phase10-order: scratch-configure-${mode}\n${configure_output}${configure_error}")
    endif()
  endforeach()
  file(REMOVE "${fail_sentinel}" "${pass_sentinel}")
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${fail_build}" --output-on-failure
      -R "^long_sentinel$"
    RESULT_VARIABLE fail_result OUTPUT_VARIABLE fail_output ERROR_VARIABLE fail_error)
  set(fail_combined "${fail_output}${fail_error}")
  if("${fail_result}" STREQUAL "0" OR EXISTS "${fail_sentinel}" OR
      NOT fail_combined MATCHES "Not Run")
    message(FATAL_ERROR "phase10-order: failing-setup-did-not-block-sentinel\n${fail_combined}")
  endif()
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${pass_build}" --output-on-failure
      -R "^long_sentinel$"
    RESULT_VARIABLE pass_result OUTPUT_VARIABLE pass_output ERROR_VARIABLE pass_error)
  if(NOT "${pass_result}" STREQUAL "0" OR NOT EXISTS "${pass_sentinel}")
    message(FATAL_ERROR
      "phase10-order: clean-fixture-did-not-permit-sentinel\n${pass_output}${pass_error}")
  endif()

  set(repeat_source "${scratch_root}/order/repeat/source")
  set(repeat_build "${scratch_root}/order/repeat/build")
  set(repeat_count "${scratch_root}/order/repeat/setup-count.txt")
  set(repeat_one "${scratch_root}/order/repeat/long-one.ran")
  set(repeat_two "${scratch_root}/order/repeat/long-two.ran")
  set(unrelated_sentinel "${scratch_root}/order/repeat/unrelated.ran")
  file(MAKE_DIRECTORY "${repeat_source}")
  file(WRITE "${repeat_count}" "0\n")
  file(WRITE "${repeat_source}/fixture_setup.cmake"
    "file(READ \"${repeat_count}\" count)\nstring(STRIP \"\${count}\" count)\nmath(EXPR count \"\${count} + 1\")\nfile(WRITE \"${repeat_count}\" \"\${count}\")\n")
  file(WRITE "${repeat_source}/write_one.cmake"
    "file(WRITE \"${repeat_one}\" \"ran\")\n")
  file(WRITE "${repeat_source}/write_two.cmake"
    "file(WRITE \"${repeat_two}\" \"ran\")\n")
  file(WRITE "${repeat_source}/write_unrelated.cmake"
    "file(WRITE \"${unrelated_sentinel}\" \"ran\")\n")
  set(repeat_project
    "cmake_minimum_required(VERSION 3.25)\nproject(phase10_fixture_repeat NONE)\nenable_testing()\n")
  string(APPEND repeat_project
    "add_test(NAME phase10_setup COMMAND \"${CMAKE_PROGRAM}\" -P \"${repeat_source}/fixture_setup.cmake\")\n")
  string(APPEND repeat_project
    "set_tests_properties(phase10_setup PROPERTIES FIXTURES_SETUP phase10_fast)\n")
  string(APPEND repeat_project
    "add_test(NAME long_one COMMAND \"${CMAKE_PROGRAM}\" -P \"${repeat_source}/write_one.cmake\")\n")
  string(APPEND repeat_project
    "add_test(NAME long_two COMMAND \"${CMAKE_PROGRAM}\" -P \"${repeat_source}/write_two.cmake\")\n")
  string(APPEND repeat_project
    "set_tests_properties(long_one long_two PROPERTIES FIXTURES_REQUIRED phase10_fast)\n")
  string(APPEND repeat_project
    "add_test(NAME unrelated_setup COMMAND \"${CMAKE_PROGRAM}\" -P \"${repeat_source}/write_unrelated.cmake\")\n")
  string(APPEND repeat_project
    "set_tests_properties(unrelated_setup PROPERTIES FIXTURES_SETUP unrelated_fixture)\n")
  file(WRITE "${repeat_source}/CMakeLists.txt" "${repeat_project}")
  execute_process(
    COMMAND "${CMAKE_PROGRAM}" -S "${repeat_source}" -B "${repeat_build}"
    RESULT_VARIABLE repeat_configure
    OUTPUT_VARIABLE repeat_configure_output ERROR_VARIABLE repeat_configure_error)
  if(NOT "${repeat_configure}" STREQUAL "0")
    message(FATAL_ERROR
      "phase10-order: scratch-configure-repeat\n${repeat_configure_output}${repeat_configure_error}")
  endif()
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${repeat_build}" --output-on-failure
      -R "^long_(one|two)$"
    RESULT_VARIABLE repeat_result
    OUTPUT_VARIABLE repeat_output ERROR_VARIABLE repeat_error)
  file(READ "${repeat_count}" repeat_setup_count)
  string(STRIP "${repeat_setup_count}" repeat_setup_count)
  if(NOT "${repeat_result}" STREQUAL "0" OR
      NOT repeat_setup_count STREQUAL "1" OR
      NOT EXISTS "${repeat_one}" OR NOT EXISTS "${repeat_two}" OR
      EXISTS "${unrelated_sentinel}")
    message(FATAL_ERROR
      "phase10-order: fixture-closure-count-or-unrelated-setup\n${repeat_output}${repeat_error}")
  endif()
  message(STATUS "phase10-order: failing setup blocks long work; shared setup runs once; unrelated setup stays idle")
endfunction()

function(gbl_phase10_check_inventories)
  foreach(lane IN ITEMS ci asan)
    set(path "${GBL_PHASE10_SRC}/tests/inventory/${lane}.txt")
    if(NOT EXISTS "${path}")
      message(FATAL_ERROR "phase10-selection: inventory-missing-${lane}")
    endif()
    file(STRINGS "${path}" local_guest_rows REGEX "^playable0")
    if(local_guest_rows)
      message(FATAL_ERROR "phase10-selection: local-playable0-in-${lane}-inventory")
    endif()
    set(expected_rows ${GBL_PHASE10_COMMON_INVENTORY_ROWS})
    if(lane STREQUAL "ci")
      list(APPEND expected_rows ${GBL_PHASE10_CI_ONLY_INVENTORY_ROWS})
    endif()
    foreach(expected_row IN LISTS expected_rows)
      string(REGEX MATCH "^([^ ]+)" name "${expected_row}")
      string(REPLACE "." "[.]" name_regex "${name}")
      file(STRINGS "${path}" matching_rows REGEX "^${name_regex} ")
      list(LENGTH matching_rows row_count)
      if(NOT row_count EQUAL 1)
        message(FATAL_ERROR
          "phase10-selection: inventory-${lane}-${name}-count-${row_count}")
      endif()
      list(GET matching_rows 0 actual_row)
      if(NOT actual_row STREQUAL expected_row)
        message(FATAL_ERROR "phase10-selection: inventory-${lane}-${name}-labels")
      endif()
    endforeach()
  endforeach()
endfunction()

function(gbl_phase10_check_order_graph)
  set(phase7_setup_names
    time.events.order time.events.boundary time.events.refusal
    timer0.model.boundary phase7.selection.control)
  set(phase8_setup_names
    cpu.irq.boundary keypad.timeline keypad.refusal time.halt.boundary
    phase8.selection.control)
  set(phase9_setup_names
    phase9.policy phase9.dispute bus.waitcnt.matrix bus.waitcnt.boundary
    dma.registers dma.events phase9.selection.control)
  set(phase10_setup_names ${GBL_PHASE10_EXPECTED})
  set(phase7_required_names
    phase7.order timer.costs timer.costs.contract timer.devices
    timer0.guest.tracer bus.timing)
  set(phase8_required_names
    cpu.irq.refusal cpu.power.boundary bus.irq.registers bus.keypad.registers
    keypad.condition time.halt.refusal runner.input.cli runner.unit
    runner.result_contract determinism.phase8 determinism.phase8.controls
    phase8.policy phase8.order)
  set(phase9_required_names
    bus.waitcnt.registers cpu.waitcnt cpu.dma.waitcnt cpu.dma.cpu
    cpu.dma.tracer cpu.dma.refusal dma.arbitration dma.latch dma.refusal
    dma.controls phase9.policy.control phase9.dispute.control phase9.order
    determinism.phase9 determinism.phase9.controls)
  set(phase10_required_names
    ppu.tracer ppu.latch ppu.frame ppu.controls
    cpu.ppu.irq cpu.ppu.latch cpu.ppu.refusal cpu.ppu.dma
    dma.ppu.triggers dma.ppu.order dma.ppu.refusal
    runner.ppu.observation runner.ppu.output runner.ppu.output-controls
    phase10.cli phase10.cli.control phase10.selection.verify
    phase10.selection.order determinism.phase10 determinism.phase10.controls)
  file(STRINGS "${GBL_PHASE10_CACHE}" lane_line
    REGEX "^GBL_LANE:STRING=")
  string(REPLACE "GBL_LANE:STRING=" "" lane "${lane_line}")
  if(lane STREQUAL "ci")
    list(APPEND phase9_required_names result.nc.phase9 result.pos.phase9)
    list(APPEND phase10_required_names result.nc.phase10 result.pos.phase10)
  endif()

  set(order_names ${phase7_setup_names} ${phase8_setup_names}
    ${phase9_setup_names} ${phase10_setup_names} ${phase7_required_names}
    ${phase8_required_names} ${phase9_required_names} ${phase10_required_names})
  set(order_regex_parts)
  foreach(name IN LISTS order_names)
    string(REPLACE "." "[.]" name_regex "${name}")
    list(APPEND order_regex_parts "${name_regex}")
  endforeach()
  list(APPEND order_regex_parts
    "phase9[.].*" "phase10[.].*" "bus[.]waitcnt[.].*"
    "cpu[.]dma[.].*" "dma[.].*" "ppu[.].*" "cpu[.]ppu[.].*"
    "dma[.]ppu[.].*" "runner[.]ppu[.].*"
    "determinism[.]phase(9|10)([.]controls)?"
    "result[.](nc|pos)[.]phase(9|10)")
  list(REMOVE_DUPLICATES order_regex_parts)
  list(JOIN order_regex_parts "|" order_regex)
  gbl_phase10_discover("${GBL_PHASE10_BIN}" "^(${order_regex})$" json)
  string(JSON count ERROR_VARIABLE error LENGTH "${json}" tests)
  if(NOT "${error}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR "phase10-order: malformed-discovery")
  endif()

  foreach(fixture IN ITEMS phase7_fast phase8_fast phase9_fast phase10_fast)
    set(actual_setups)
    if(count GREATER 0)
      math(EXPR last "${count} - 1")
      foreach(index RANGE 0 ${last})
        string(JSON name GET "${json}" tests ${index} name)
        gbl_phase10_json_property("${json}" ${index} FIXTURES_SETUP
          fixtures is_setup)
        if(is_setup AND "${fixture}" IN_LIST fixtures)
          list(APPEND actual_setups "${name}")
        endif()
      endforeach()
    endif()
    if(fixture STREQUAL "phase7_fast")
      set(expected_setups ${phase7_setup_names})
    elseif(fixture STREQUAL "phase8_fast")
      set(expected_setups ${phase8_setup_names})
    elseif(fixture STREQUAL "phase9_fast")
      set(expected_setups ${phase9_setup_names})
    else()
      set(expected_setups ${phase10_setup_names})
    endif()
    list(SORT actual_setups)
    list(SORT expected_setups)
    if(NOT "${actual_setups}" STREQUAL "${expected_setups}")
      message(FATAL_ERROR "phase10-order: ${fixture}-setup-set")
    endif()
  endforeach()

  foreach(required_name IN LISTS phase7_required_names phase8_required_names
      phase9_required_names phase10_required_names)
    gbl_phase10_find_test("${json}" "${required_name}" ignored_index found)
    if(NOT found)
      message(FATAL_ERROR "phase10-order: missing-registration-${required_name}")
    endif()
  endforeach()
  gbl_phase10_find_test("${json}" phase10.selection.order phase10_order_index found)
  if(NOT found)
    message(FATAL_ERROR "phase10-order: missing-registration-phase10.selection.order")
  endif()
  gbl_phase10_expected_command(phase10.selection.order "${json}" ${phase10_order_index}
    order_command_reason)
  if(NOT "${order_command_reason}" STREQUAL "")
    message(FATAL_ERROR "phase10-order: order-command-${order_command_reason}")
  endif()
  gbl_phase10_json_property("${json}" ${phase10_order_index} WORKING_DIRECTORY
    order_working_directory has_order_working_directory)
  if(NOT has_order_working_directory)
    message(FATAL_ERROR "phase10-order: order-working-directory-missing")
  endif()
  file(REAL_PATH "${order_working_directory}" order_working_directory_real)
  if(NOT order_working_directory_real STREQUAL GBL_PHASE10_BIN)
    message(FATAL_ERROR "phase10-order: order-working-directory-mismatch")
  endif()
  if(count GREATER 0)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
      string(JSON name GET "${json}" tests ${index} name)
      gbl_phase10_json_property("${json}" ${index} FIXTURES_SETUP
        setup_fixtures is_setup)
      gbl_phase10_json_property("${json}" ${index} FIXTURES_REQUIRED
        required_fixtures has_required)
      if(is_setup AND "phase10_fast" IN_LIST setup_fixtures AND
          "phase10_fast" IN_LIST required_fixtures)
        message(FATAL_ERROR "phase10-order: setup-self-cycle-${name}")
      endif()
      if(name IN_LIST phase10_setup_names)
        if("phase10_fast" IN_LIST required_fixtures)
          message(FATAL_ERROR "phase10-order: phase10-setup-self-cycle-${name}")
        endif()
      elseif(name IN_LIST phase10_required_names)
        if(NOT "phase10_fast" IN_LIST required_fixtures)
          message(FATAL_ERROR "phase10-order: missing-phase10-predecessor-${name}")
        endif()
      endif()
      if(name IN_LIST phase7_setup_names AND
          ("phase8_fast" IN_LIST required_fixtures OR
           "phase9_fast" IN_LIST required_fixtures OR
           "phase10_fast" IN_LIST required_fixtures))
        message(FATAL_ERROR "phase10-order: earlier-setup-depends-on-later-${name}")
      endif()
      if(name IN_LIST phase8_setup_names AND
          ("phase9_fast" IN_LIST required_fixtures OR
           "phase10_fast" IN_LIST required_fixtures))
        message(FATAL_ERROR "phase10-order: phase8-setup-depends-on-later-${name}")
      endif()
      if(name IN_LIST phase9_setup_names AND
          "phase10_fast" IN_LIST required_fixtures)
        message(FATAL_ERROR "phase10-order: phase9-setup-depends-on-phase10-${name}")
      endif()
    endforeach()
  endif()
  gbl_phase10_check_fixture_controls("${GBL_PHASE10_BIN}/phase10-order-controls")
  message(STATUS "phase10-order: Phase 7/8/9 closure and exact Phase 10 fixture graph passed")
endfunction()

if(CASE STREQUAL "control")
  gbl_phase10_check_selection_controls("${GBL_PHASE10_BIN}/phase10-selection-controls")
elseif(CASE STREQUAL "order")
  gbl_phase10_check_order_graph()
elseif(CASE STREQUAL "verify")
  gbl_phase10_check_inventories()
  gbl_phase10_discover("${GBL_PHASE10_BIN}" "${GBL_PHASE10_REGEX}" selected_json)
  gbl_phase10_validate_selection("${selected_json}" selection_reason)
  if(NOT "${selection_reason}" STREQUAL "")
    execute_process(COMMAND "${CMAKE_PROGRAM}" -E echo "TAP version 13")
    execute_process(COMMAND "${CMAKE_PROGRAM}" -E echo "1..1")
    execute_process(COMMAND "${CMAKE_PROGRAM}" -E echo
      "not ok 1 - exact Phase 10 cheap selector fixture membership")
    execute_process(COMMAND "${CMAKE_PROGRAM}" -E echo
      "# observed ${selection_reason}; expected all six selectors to set up phase10_fast")
    message(FATAL_ERROR "phase10-selection: ${selection_reason}")
  endif()
  execute_process(COMMAND "${CMAKE_PROGRAM}" -E echo "TAP version 13")
  execute_process(COMMAND "${CMAKE_PROGRAM}" -E echo "1..1")
  execute_process(COMMAND "${CMAKE_PROGRAM}" -E echo
    "ok 1 - exact Phase 10 cheap selector fixture membership")
  message(STATUS "phase10-selection: exact-six selection, commands, roots and inventories passed")
endif()
