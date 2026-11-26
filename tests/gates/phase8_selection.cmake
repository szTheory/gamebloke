cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase8-selection: missing-${required}")
  endif()
endforeach()
if(NOT CASE MATCHES "^(verify|control|negative|order)$")
  message(FATAL_ERROR "phase8-selection: invalid-case")
endif()

get_filename_component(GBL_PHASE8_DEFAULT_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
get_filename_component(GBL_PHASE8_RUNNING_BIN "${CMAKE_BINARY_DIR}" REALPATH)
get_filename_component(GBL_PHASE8_BIN "${BIN}" REALPATH
  BASE_DIR "${GBL_PHASE8_RUNNING_BIN}")
if(NOT IS_DIRECTORY "${GBL_PHASE8_RUNNING_BIN}" OR
    NOT IS_DIRECTORY "${GBL_PHASE8_BIN}")
  message(FATAL_ERROR "phase8-selection: source-or-build-directory-missing")
endif()
if(NOT GBL_PHASE8_BIN STREQUAL GBL_PHASE8_RUNNING_BIN)
  message(FATAL_ERROR "phase8-selection: build-directory-mismatch")
endif()
set(GBL_PHASE8_CACHE "${GBL_PHASE8_BIN}/CMakeCache.txt")
if(NOT EXISTS "${GBL_PHASE8_CACHE}")
  message(FATAL_ERROR "phase8-selection: build-cache-missing")
endif()
file(STRINGS "${GBL_PHASE8_CACHE}" GBL_PHASE8_CACHE_HOME
  REGEX "^CMAKE_HOME_DIRECTORY:INTERNAL=")
list(LENGTH GBL_PHASE8_CACHE_HOME GBL_PHASE8_CACHE_HOME_COUNT)
if(NOT GBL_PHASE8_CACHE_HOME_COUNT EQUAL 1)
  message(FATAL_ERROR "phase8-selection: build-source-missing")
endif()
string(REGEX REPLACE "^CMAKE_HOME_DIRECTORY:INTERNAL=" ""
  GBL_PHASE8_CACHE_SOURCE "${GBL_PHASE8_CACHE_HOME}")
file(REAL_PATH "${GBL_PHASE8_CACHE_SOURCE}" GBL_PHASE8_CACHE_SOURCE)
get_filename_component(GBL_PHASE8_REQUESTED_SRC "${SRC}" REALPATH
  BASE_DIR "${GBL_PHASE8_DEFAULT_ROOT}")
if(NOT GBL_PHASE8_CACHE_SOURCE STREQUAL GBL_PHASE8_REQUESTED_SRC)
  message(FATAL_ERROR "phase8-selection: source-build-mismatch")
endif()
set(GBL_PHASE8_SRC "${GBL_PHASE8_CACHE_SOURCE}")
if(NOT IS_DIRECTORY "${GBL_PHASE8_SRC}")
  message(FATAL_ERROR "phase8-selection: source-or-build-directory-missing")
endif()
set(GBL_PHASE8_EXPECTED_GATE "${GBL_PHASE8_SRC}/tests/gates/phase8_selection.cmake")
if(NOT EXISTS "${GBL_PHASE8_EXPECTED_GATE}")
  message(FATAL_ERROR "phase8-selection: source-gate-missing")
endif()
file(REAL_PATH "${GBL_PHASE8_EXPECTED_GATE}" GBL_PHASE8_EXPECTED_GATE)
file(REAL_PATH "${CMAKE_CURRENT_LIST_FILE}" GBL_PHASE8_GATE)
if(NOT GBL_PHASE8_GATE STREQUAL GBL_PHASE8_EXPECTED_GATE)
  message(FATAL_ERROR "phase8-selection: script-source-mismatch")
endif()
find_program(CTEST_PROGRAM NAMES ctest REQUIRED)
find_program(CMAKE_PROGRAM NAMES cmake REQUIRED)

set(GBL_PHASE8_EXPECTED
  cpu.irq.boundary
  keypad.timeline
  keypad.refusal
  time.halt.boundary
  phase8.selection.control)
set(GBL_PHASE8_REGEX
  "^(cpu[.]irq[.]boundary|keypad[.](timeline|refusal)|time[.]halt[.]boundary|phase8[.]selection[.]control)$")
set(GBL_PHASE8_INVENTORY_ROWS
  "cpu.irq.boundary core,cpu,unit"
  "keypad.timeline core,time,timer,unit"
  "keypad.refusal core,time,timer,unit"
  "time.halt.boundary core,time,timer,unit"
  "timer0.guest.tracer core,integration,timer"
  "phase8.selection.control gate,time,timer"
  "phase8.order gate,time,timer"
  "phase8.policy gate,profile,time")

function(gbl_phase8_json_property json test_index wanted property_out found_out)
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
      if(property_name STREQUAL wanted)
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
        elseif(value_type STREQUAL "STRING")
          string(JSON value GET "${json}" tests ${test_index}
            properties ${property_index} value)
        endif()
        set(found TRUE)
        break()
      endif()
    endforeach()
  endif()
  set(${property_out} "${value}" PARENT_SCOPE)
  set(${found_out} "${found}" PARENT_SCOPE)
endfunction()

function(gbl_phase8_expected_command name json index reason_out)
  string(JSON command_count ERROR_VARIABLE command_error LENGTH
    "${json}" tests ${index} command)
  if(NOT "${command_error}" STREQUAL "NOTFOUND" OR command_count LESS 1)
    set(${reason_out} wrong-command PARENT_SCOPE)
    return()
  endif()
  string(JSON command_program GET "${json}" tests ${index} command 0)
  if(name STREQUAL "cpu.irq.boundary")
    set(expected_program "${GBL_PHASE8_BIN}/cpu_test${CMAKE_EXECUTABLE_SUFFIX}")
    set(expected_arg irq.boundary)
  elseif(name STREQUAL "keypad.timeline")
    set(expected_program "${GBL_PHASE8_BIN}/timer_test${CMAKE_EXECUTABLE_SUFFIX}")
    set(expected_arg keypad.timeline)
  elseif(name STREQUAL "keypad.refusal")
    set(expected_program "${GBL_PHASE8_BIN}/timer_test${CMAKE_EXECUTABLE_SUFFIX}")
    set(expected_arg keypad.refusal)
  elseif(name STREQUAL "time.halt.boundary")
    set(expected_program "${GBL_PHASE8_BIN}/timer_test${CMAKE_EXECUTABLE_SUFFIX}")
    set(expected_arg time.halt.boundary)
  elseif(name STREQUAL "phase8.selection.control")
    if(NOT command_count EQUAL 6)
      set(${reason_out} wrong-command PARENT_SCOPE)
      return()
    endif()
    string(JSON command_program GET "${json}" tests ${index} command 0)
    file(REAL_PATH "${command_program}" actual_cmake)
    file(REAL_PATH "${CMAKE_COMMAND}" expected_cmake)
    if(NOT actual_cmake STREQUAL expected_cmake)
      set(${reason_out} wrong-command PARENT_SCOPE)
      return()
    endif()
    set(expected_control_args
      "-DSRC=${GBL_PHASE8_SRC}"
      "-DBIN=${GBL_PHASE8_BIN}"
      "-DCASE=control"
      "-P"
      "${GBL_PHASE8_GATE}")
    foreach(command_index RANGE 1 5)
      string(JSON actual_arg GET "${json}" tests ${index} command ${command_index})
      math(EXPR expected_index "${command_index} - 1")
      list(GET expected_control_args ${expected_index} expected_arg)
      if(NOT actual_arg STREQUAL expected_arg)
        set(${reason_out} wrong-command PARENT_SCOPE)
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
    set(${reason_out} wrong-command PARENT_SCOPE)
    return()
  endif()
  set(${reason_out} "" PARENT_SCOPE)
endfunction()

function(gbl_phase8_validate_selection json reason_out)
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
  list(LENGTH GBL_PHASE8_EXPECTED expected_count)
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

  foreach(expected IN LISTS GBL_PHASE8_EXPECTED)
    list(FIND actual_names "${expected}" expected_index)
    if(expected_index EQUAL -1)
      set(${reason_out} missing PARENT_SCOPE)
      return()
    endif()
  endforeach()
  foreach(index RANGE 0 ${last_index})
    string(JSON name GET "${json}" tests ${index} name)
    list(FIND GBL_PHASE8_EXPECTED "${name}" expected_index)
    if(expected_index EQUAL -1)
      set(${reason_out} extra PARENT_SCOPE)
      return()
    endif()
    gbl_phase8_expected_command("${name}" "${json}" ${index} command_reason)
    if(NOT "${command_reason}" STREQUAL "")
      set(${reason_out} "${command_reason}" PARENT_SCOPE)
      return()
    endif()
    gbl_phase8_json_property("${json}" ${index} LABELS labels has_labels)
    if(NOT has_labels)
      set(${reason_out} wrong-label PARENT_SCOPE)
      return()
    endif()
    if(name STREQUAL "cpu.irq.boundary")
      set(expected_labels cpu core unit)
    elseif(name STREQUAL "phase8.selection.control")
      set(expected_labels gate time timer)
    else()
      set(expected_labels core time timer unit)
    endif()
    set(actual_labels ${labels})
    list(SORT actual_labels)
    list(SORT expected_labels)
    if(NOT "${actual_labels}" STREQUAL "${expected_labels}")
      set(${reason_out} wrong-label PARENT_SCOPE)
      return()
    endif()
    if(name STREQUAL "phase8.selection.control")
      gbl_phase8_json_property("${json}" ${index} WORKING_DIRECTORY
        working_directory has_working_directory)
      if(NOT has_working_directory)
        set(${reason_out} wrong-working-directory PARENT_SCOPE)
        return()
      endif()
      file(REAL_PATH "${working_directory}" working_directory_real)
      if(NOT working_directory_real STREQUAL GBL_PHASE8_BIN)
        set(${reason_out} wrong-working-directory PARENT_SCOPE)
        return()
      endif()
    endif()
  endforeach()
  list(LENGTH actual_names actual_count)
  if(NOT actual_count EQUAL expected_count)
    set(${reason_out} missing PARENT_SCOPE)
    return()
  endif()
  set(${reason_out} "" PARENT_SCOPE)
endfunction()

function(gbl_phase8_discover build_dir regex output_var)
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${build_dir}" --show-only=json-v1
      --fixture-exclude-any "^(phase7_fast|phase8_fast)$" -R "${regex}"
    RESULT_VARIABLE result OUTPUT_VARIABLE json ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "phase8-selection: discovery: ${result}\n${error}")
  endif()
  set(${output_var} "${json}" PARENT_SCOPE)
endfunction()

function(gbl_phase8_write_scratch_project source_dir build_dir)
  file(MAKE_DIRECTORY "${source_dir}")
  set(project_text "cmake_minimum_required(VERSION 3.25)\nproject(phase8_selection_scratch NONE)\nenable_testing()\n")
  string(APPEND project_text
    "add_test(NAME cpu.irq.boundary COMMAND \"${GBL_PHASE8_BIN}/cpu_test\" irq.boundary)\n")
  string(APPEND project_text
    "add_test(NAME keypad.timeline COMMAND \"${GBL_PHASE8_BIN}/timer_test\" keypad.timeline)\n")
  string(APPEND project_text
    "add_test(NAME keypad.refusal COMMAND \"${GBL_PHASE8_BIN}/timer_test\" keypad.refusal)\n")
  string(APPEND project_text
    "add_test(NAME time.halt.boundary COMMAND \"${GBL_PHASE8_BIN}/timer_test\" time.halt.boundary)\n")
  string(APPEND project_text
    "add_test(NAME phase8.selection.control COMMAND \"${CMAKE_PROGRAM}\" -DSRC=${GBL_PHASE8_SRC} -DBIN=${GBL_PHASE8_BIN} -DCASE=control -P \"${GBL_PHASE8_GATE}\")\n")
  string(APPEND project_text
    "add_test(NAME phase8.unrelated COMMAND \"${CMAKE_PROGRAM}\" -E true)\n")
  string(APPEND project_text
    "add_test(NAME phase8.unrelated.second COMMAND \"${CMAKE_PROGRAM}\" -E true)\n")
  string(APPEND project_text
    "set_tests_properties(cpu.irq.boundary PROPERTIES LABELS \"cpu;core;unit\" FIXTURES_SETUP phase8_fast)\n")
  string(APPEND project_text
    "set_tests_properties(keypad.timeline keypad.refusal time.halt.boundary PROPERTIES LABELS \"core;time;timer;unit\" FIXTURES_SETUP phase8_fast)\n")
  string(APPEND project_text
    "set_tests_properties(phase8.selection.control PROPERTIES LABELS \"gate;time;timer\" FIXTURES_SETUP phase8_fast WORKING_DIRECTORY \"${GBL_PHASE8_BIN}\")\n")
  file(WRITE "${source_dir}/CMakeLists.txt" "${project_text}")
  execute_process(
    COMMAND "${CMAKE_PROGRAM}" -S "${source_dir}" -B "${build_dir}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
  if(NOT "${configure_result}" STREQUAL "0")
    message(FATAL_ERROR
      "phase8-selection: scratch-configure\n${configure_output}${configure_error}")
  endif()
endfunction()

function(gbl_phase8_expect_invocation_rejection name expected_marker)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE invocation_result
    OUTPUT_VARIABLE invocation_output
    ERROR_VARIABLE invocation_error)
  set(invocation_text "${invocation_output}${invocation_error}")
  string(FIND "${invocation_text}" "${expected_marker}" marker_index)
  if("${invocation_result}" STREQUAL "0" OR marker_index EQUAL -1)
    message(FATAL_ERROR
      "phase8-selection-control: ${name}: expected-${expected_marker}; result=${invocation_result}\n${invocation_text}")
  endif()
  message(STATUS "phase8-selection-control: ${name}: causal rejection ok")
endfunction()

function(gbl_phase8_check_selection_controls scratch_root)
  set(source_dir "${scratch_root}/selection/source")
  set(build_dir "${scratch_root}/selection/build")
  gbl_phase8_write_scratch_project("${source_dir}" "${build_dir}")
  gbl_phase8_discover("${build_dir}" "${GBL_PHASE8_REGEX}" clean_json)
  gbl_phase8_validate_selection("${clean_json}" clean_reason)
  if(NOT "${clean_reason}" STREQUAL "")
    message(FATAL_ERROR "phase8-selection-control: clean-twin-${clean_reason}")
  endif()

  set(faults empty missing extra broad duplicate malformed wrong-command
    wrong-control-script wrong-control-source wrong-control-build
    wrong-control-executable wrong-control-case wrong-control-case-override
    wrong-control-working-directory wrong-label)
  foreach(fault IN LISTS faults)
    if(fault STREQUAL "empty")
      set(fault_json "{\"kind\":\"ctestInfo\",\"tests\":[]}")
    elseif(fault STREQUAL "missing")
      gbl_phase8_discover("${build_dir}"
        "^(cpu[.]irq[.]boundary|keypad[.](timeline|refusal)|time[.]halt[.]boundary)$"
        fault_json)
    elseif(fault STREQUAL "extra")
      gbl_phase8_discover("${build_dir}"
        "^(cpu[.]irq[.]boundary|keypad[.](timeline|refusal)|time[.]halt[.]boundary|phase8[.]selection[.]control|phase8[.]unrelated)$"
        fault_json)
    elseif(fault STREQUAL "broad")
      gbl_phase8_discover("${build_dir}" "^.*$" fault_json)
    elseif(fault STREQUAL "malformed")
      set(fault_json "{\"kind\":\"ctestInfo\",\"tests\":[")
    elseif(fault STREQUAL "duplicate")
      set(fault_json "${clean_json}")
      string(JSON test_count LENGTH "${fault_json}" tests)
      string(JSON duplicate_test GET "${fault_json}" tests 0)
      string(JSON fault_json SET "${fault_json}" tests ${test_count}
        "${duplicate_test}")
    else()
      set(fault_json "${clean_json}")
      string(JSON test_count LENGTH "${fault_json}" tests)
      set(target_index -1)
      math(EXPR test_last "${test_count} - 1")
      if(fault MATCHES "^wrong-control-")
        set(target_name phase8.selection.control)
      else()
        set(target_name cpu.irq.boundary)
      endif()
      foreach(index RANGE 0 ${test_last})
        string(JSON test_name GET "${fault_json}" tests ${index} name)
        if(test_name STREQUAL target_name)
          set(target_index ${index})
          break()
        endif()
      endforeach()
      if(target_index LESS 0)
        message(FATAL_ERROR "phase8-selection-control: target-missing")
      endif()
      if(fault STREQUAL "wrong-command")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 1
          "\"not-the-boundary-selector\"")
      elseif(fault STREQUAL "wrong-control-executable")
        file(MAKE_DIRECTORY "${scratch_root}/other")
        file(WRITE "${scratch_root}/other/cmake" "not a CMake executable\n")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 0
          "\"${scratch_root}/other/cmake\"")
      elseif(fault STREQUAL "wrong-control-case")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 3
          "\"-DCASE=verify\"")
      elseif(fault STREQUAL "wrong-control-case-override")
        string(JSON fault_json SET "${fault_json}" tests ${target_index} command 6
          "\"-DCASE=verify\"")
      elseif(fault STREQUAL "wrong-control-working-directory")
        file(MAKE_DIRECTORY "${scratch_root}/other-working-directory")
        string(JSON property_count LENGTH "${fault_json}" tests ${target_index} properties)
        math(EXPR property_last "${property_count} - 1")
        foreach(property_index RANGE 0 ${property_last})
          string(JSON property_name GET "${fault_json}" tests ${target_index}
            properties ${property_index} name)
          if(property_name STREQUAL "WORKING_DIRECTORY")
            string(JSON fault_json SET "${fault_json}" tests ${target_index}
              properties ${property_index} value
              "\"${scratch_root}/other-working-directory\"")
            break()
          endif()
        endforeach()
      elseif(fault STREQUAL "wrong-control-script" OR
          fault STREQUAL "wrong-control-source" OR
          fault STREQUAL "wrong-control-build")
        string(JSON command_count LENGTH "${fault_json}" tests ${target_index} command)
        math(EXPR command_last "${command_count} - 1")
        set(command_index -1)
        foreach(index RANGE 1 ${command_last})
          string(JSON command_arg GET "${fault_json}" tests ${target_index}
            command ${index})
          if((fault STREQUAL "wrong-control-script" AND command_arg STREQUAL "-P") OR
              (fault STREQUAL "wrong-control-source" AND command_arg MATCHES "^-DSRC=") OR
              (fault STREQUAL "wrong-control-build" AND command_arg MATCHES "^-DBIN="))
            set(command_index ${index})
            break()
          endif()
        endforeach()
        if(command_index LESS 0)
          message(FATAL_ERROR "phase8-selection-control: command-argument-missing-${fault}")
        endif()
        if(fault STREQUAL "wrong-control-script")
          set(alternate_script "${scratch_root}/other/phase8_selection.cmake")
          file(MAKE_DIRECTORY "${scratch_root}/other")
          file(COPY_FILE "${GBL_PHASE8_GATE}" "${alternate_script}")
          math(EXPR command_index "${command_index} + 1")
          string(JSON fault_json SET "${fault_json}" tests ${target_index}
            command ${command_index} "\"${alternate_script}\"")
        elseif(fault STREQUAL "wrong-control-source")
          string(JSON fault_json SET "${fault_json}" tests ${target_index}
            command ${command_index}
            "\"-DSRC=${scratch_root}/other/source\"")
        else()
          string(JSON fault_json SET "${fault_json}" tests ${target_index}
            command ${command_index}
            "\"-DBIN=${scratch_root}/other/build\"")
        endif()
      elseif(fault STREQUAL "wrong-label")
        string(JSON property_count LENGTH "${fault_json}" tests ${target_index} properties)
        math(EXPR property_last "${property_count} - 1")
        foreach(property_index RANGE 0 ${property_last})
          string(JSON property_name GET "${fault_json}" tests ${target_index}
            properties ${property_index} name)
          if(property_name STREQUAL "LABELS")
            string(JSON fault_json SET "${fault_json}" tests ${target_index}
              properties ${property_index} value 0 "\"wrong-label\"")
            break()
          endif()
        endforeach()
      endif()
    endif()
    gbl_phase8_validate_selection("${fault_json}" observed_reason)
    if(fault STREQUAL "wrong-control-working-directory")
      set(expected_reason wrong-working-directory)
    elseif(fault MATCHES "^wrong-control-")
      set(expected_reason wrong-command)
    else()
      set(expected_reason "${fault}")
    endif()
    if(NOT observed_reason STREQUAL expected_reason)
      message(FATAL_ERROR
        "phase8-selection-control: ${fault}: expected-${expected_reason}-got-${observed_reason}")
    endif()
    message(STATUS "phase8-selection-control: ${fault}: causal rejection ok")
  endforeach()
  set(foreign_gate "${scratch_root}/foreign/tests/gates/phase8_selection.cmake")
  file(MAKE_DIRECTORY "${scratch_root}/foreign/tests/gates")
  file(COPY_FILE "${GBL_PHASE8_GATE}" "${foreign_gate}")
  gbl_phase8_expect_invocation_rejection(foreign-script "script-source-mismatch"
    "${CMAKE_PROGRAM}" "-DSRC=${GBL_PHASE8_SRC}" "-DBIN=${GBL_PHASE8_BIN}"
    -DCASE=verify -P "${foreign_gate}")
  gbl_phase8_expect_invocation_rejection(foreign-build "build-directory-mismatch"
    "${CMAKE_PROGRAM}" "-DSRC=${GBL_PHASE8_SRC}" "-DBIN=${build_dir}"
    -DCASE=verify -P "${GBL_PHASE8_GATE}")
  set(foreign_source "${scratch_root}/foreign-checkout/source")
  set(foreign_build "${scratch_root}/foreign-checkout/build")
  file(MAKE_DIRECTORY "${foreign_source}/tests/gates")
  file(COPY_FILE "${GBL_PHASE8_GATE}"
    "${foreign_source}/tests/gates/phase8_selection.cmake")
  gbl_phase8_write_scratch_project("${foreign_source}" "${foreign_build}")
  gbl_phase8_expect_invocation_rejection(foreign-checkout
    "build-directory-mismatch" "${CMAKE_PROGRAM}"
    "-DSRC=${foreign_source}" "-DBIN=${foreign_build}" -DCASE=verify
    -P "${foreign_source}/tests/gates/phase8_selection.cmake")
  message(STATUS "phase8-selection: scratch exact-five and selection controls passed")
endfunction()

function(gbl_phase8_check_fixture_controls scratch_root)
  set(fail_source "${scratch_root}/fixture/fail/source")
  set(fail_build "${scratch_root}/fixture/fail/build")
  set(fail_sentinel "${scratch_root}/fixture/fail/long-case.ran")
  set(pass_source "${scratch_root}/fixture/pass/source")
  set(pass_build "${scratch_root}/fixture/pass/build")
  set(pass_sentinel "${scratch_root}/fixture/pass/long-case.ran")
  foreach(mode IN ITEMS fail pass)
    if(mode STREQUAL "fail")
      set(source_dir "${fail_source}")
      set(build_dir "${fail_build}")
      set(sentinel "${fail_sentinel}")
      set(setup_body "message(FATAL_ERROR \"phase8-order: intentional setup fault\")")
    else()
      set(source_dir "${pass_source}")
      set(build_dir "${pass_build}")
      set(sentinel "${pass_sentinel}")
      set(setup_body "message(STATUS \"phase8-order: setup succeeded\")")
    endif()
    file(MAKE_DIRECTORY "${source_dir}")
    file(WRITE "${source_dir}/fixture_setup.cmake" "${setup_body}\n")
    file(WRITE "${source_dir}/write_sentinel.cmake"
      "file(WRITE \"${sentinel}\" \"ran\")\n")
    file(WRITE "${source_dir}/CMakeLists.txt"
      "cmake_minimum_required(VERSION 3.25)\nproject(phase8_fixture_${mode} NONE)\nenable_testing()\nadd_test(NAME cheap_setup COMMAND \"${CMAKE_PROGRAM}\" -P \"${source_dir}/fixture_setup.cmake\")\nset_tests_properties(cheap_setup PROPERTIES FIXTURES_SETUP phase8_fast)\nadd_test(NAME long_sentinel COMMAND \"${CMAKE_PROGRAM}\" -P \"${source_dir}/write_sentinel.cmake\")\nset_tests_properties(long_sentinel PROPERTIES FIXTURES_REQUIRED phase8_fast)\n")
    execute_process(
      COMMAND "${CMAKE_PROGRAM}" -S "${source_dir}" -B "${build_dir}"
      RESULT_VARIABLE configure_result
      OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
    if(NOT "${configure_result}" STREQUAL "0")
      message(FATAL_ERROR "phase8-order: scratch-configure-${mode}\n${configure_output}${configure_error}")
    endif()
  endforeach()
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${fail_build}" --show-only=json-v1
      --fixture-exclude-any "^phase8_fast$" -R "^long_sentinel$"
    RESULT_VARIABLE discovery_result OUTPUT_VARIABLE discovery_json ERROR_VARIABLE discovery_error)
  if(NOT "${discovery_result}" STREQUAL "0")
    message(FATAL_ERROR "phase8-order: fixture-excluded-discovery-failed\n${discovery_error}")
  endif()
  string(JSON discovered_count ERROR_VARIABLE discovered_error
    LENGTH "${discovery_json}" tests)
  if(NOT "${discovered_error}" STREQUAL "NOTFOUND" OR
      NOT discovered_count EQUAL 1)
    message(FATAL_ERROR "phase8-order: fixture-excluded-discovery-count")
  endif()
  string(JSON discovered_name GET "${discovery_json}" tests 0 name)
  if(NOT discovered_name STREQUAL "long_sentinel")
    message(FATAL_ERROR "phase8-order: fixture-excluded-discovery-selected-setup")
  endif()
  file(REMOVE "${fail_sentinel}" "${pass_sentinel}")
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${fail_build}" --output-on-failure
      -R "^long_sentinel$"
    RESULT_VARIABLE fail_result OUTPUT_VARIABLE fail_output ERROR_VARIABLE fail_error)
  set(fail_combined "${fail_output}${fail_error}")
  if("${fail_result}" STREQUAL "0" OR EXISTS "${fail_sentinel}" OR
      NOT fail_combined MATCHES "Not Run")
    message(FATAL_ERROR "phase8-order: failing-setup-did-not-block-sentinel\n${fail_combined}")
  endif()
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${pass_build}" --output-on-failure
      -R "^long_sentinel$"
    RESULT_VARIABLE pass_result OUTPUT_VARIABLE pass_output ERROR_VARIABLE pass_error)
  if(NOT "${pass_result}" STREQUAL "0" OR NOT EXISTS "${pass_sentinel}")
    message(FATAL_ERROR
      "phase8-order: clean-fixture-did-not-permit-sentinel\n${pass_output}${pass_error}")
  endif()
  message(STATUS "phase8-order-control: failed setup blocks sentinel; clean setup permits it")
endfunction()

function(gbl_phase8_check_inventories)
  foreach(lane IN ITEMS ci asan)
    set(path "${GBL_PHASE8_SRC}/tests/inventory/${lane}.txt")
    if(NOT EXISTS "${path}")
      message(FATAL_ERROR "phase8-selection: inventory-missing-${lane}")
    endif()
    foreach(expected_row IN LISTS GBL_PHASE8_INVENTORY_ROWS)
      string(REGEX MATCH "^([^ ]+)" name "${expected_row}")
      string(REPLACE "." "[.]" name_regex "${name}")
      file(STRINGS "${path}" matching_rows REGEX "^${name_regex} ")
      list(LENGTH matching_rows row_count)
      if(NOT row_count EQUAL 1)
        message(FATAL_ERROR "phase8-selection: inventory-${lane}-${name}-count-${row_count}")
      endif()
      list(GET matching_rows 0 actual_row)
      if(NOT actual_row STREQUAL expected_row)
        message(FATAL_ERROR "phase8-selection: inventory-${lane}-${name}-labels")
      endif()
    endforeach()
  endforeach()
endfunction()

function(gbl_phase8_check_order_graph)
  set(setup_names
    time.events.order time.events.boundary time.events.refusal
    timer0.model.boundary phase7.selection.control
    cpu.irq.boundary keypad.timeline keypad.refusal time.halt.boundary
    phase8.selection.control)
  set(required_names
    phase7.order
    cpu.irq.refusal cpu.power.boundary bus.irq.registers bus.keypad.registers
    keypad.condition time.halt.refusal runner.input.cli runner.unit
    runner.result_contract determinism.phase8 determinism.phase8.controls
    phase8.policy phase8.order)
  # Only Phase 7/8 setup and dependent tests participate in this graph. Avoid
  # reparsing the full CI inventory for each property lookup; that made this
  # focused gate take over 160 seconds on the configured 1,100-test graph.
  set(order_names ${setup_names} ${required_names})
  set(order_regex_parts)
  foreach(name IN LISTS order_names)
    string(REPLACE "." "[.]" name_regex "${name}")
    list(APPEND order_regex_parts "${name_regex}")
  endforeach()
  list(APPEND order_regex_parts
    "phase8[.].*" "cpu[.]irq[.].*" "cpu[.]power[.].*"
    "bus[.](irq|keypad)[.].*" "keypad[.].*" "time[.]halt[.].*")
  list(REMOVE_DUPLICATES order_regex_parts)
  list(JOIN order_regex_parts "|" order_regex)
  gbl_phase8_discover("${GBL_PHASE8_BIN}" "^(${order_regex})$" json)
  string(JSON count ERROR_VARIABLE error LENGTH "${json}" tests)
  if(NOT "${error}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR "phase8-order: malformed-discovery")
  endif()
  set(actual_setups)
  if(count GREATER 0)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
      string(JSON name GET "${json}" tests ${index} name)
      gbl_phase8_json_property("${json}" ${index} FIXTURES_SETUP fixtures is_setup)
      if(is_setup AND "phase8_fast" IN_LIST fixtures)
        list(APPEND actual_setups "${name}")
      endif()
    endforeach()
  endif()
  set(expected_setups
    cpu.irq.boundary keypad.timeline keypad.refusal time.halt.boundary
    phase8.selection.control)
  list(SORT actual_setups)
  list(SORT expected_setups)
  if(NOT actual_setups STREQUAL expected_setups)
    message(FATAL_ERROR "phase8-order: setup-set")
  endif()
  foreach(required_name IN LISTS required_names)
    set(found FALSE)
    foreach(index RANGE 0 ${last})
      string(JSON name GET "${json}" tests ${index} name)
      if(name STREQUAL required_name)
        set(found TRUE)
        break()
      endif()
    endforeach()
    if(NOT found)
      message(FATAL_ERROR "phase8-order: missing-registration-${required_name}")
    endif()
  endforeach()
  foreach(index RANGE 0 ${last})
    string(JSON name GET "${json}" tests ${index} name)
    gbl_phase8_json_property("${json}" ${index} FIXTURES_SETUP fixtures is_setup)
    gbl_phase8_json_property("${json}" ${index} FIXTURES_REQUIRED required_fixtures requires_phase8)
    if(is_setup AND "phase8_fast" IN_LIST fixtures)
      if("phase8_fast" IN_LIST required_fixtures)
        message(FATAL_ERROR "phase8-order: setup-self-cycle-${name}")
      endif()
    endif()
    if(name MATCHES "^(cpu[.]irq[.]refusal|cpu[.]power[.]boundary|bus[.](irq|keypad)[.]registers|keypad[.]condition|time[.]halt[.]refusal|runner[.](input[.]cli|unit|result_contract)|determinism[.]phase8([.]controls)?|phase8[.]policy|phase8[.]order)$")
      if(NOT "phase8_fast" IN_LIST required_fixtures)
        message(FATAL_ERROR "phase8-order: missing-phase8-predecessor-${name}")
      endif()
    endif()
    if(name MATCHES "^(time[.]events[.]|timer0[.]model[.]|phase7[.]selection[.]control)" AND NOT name STREQUAL "phase7.order")
      if("phase8_fast" IN_LIST required_fixtures)
        message(FATAL_ERROR "phase8-order: phase7-setup-depends-on-phase8-${name}")
      endif()
    endif()
  endforeach()
  message(STATUS "phase8-order: exact Phase 8 predecessor graph and scratch controls passed")
endfunction()

if(CASE STREQUAL "negative")
  if(NOT DEFINED FAULT OR "${FAULT}" STREQUAL "")
    message(FATAL_ERROR "phase8-selection: missing-fault")
  endif()
  message(FATAL_ERROR "phase8-selection: ${FAULT}")
elseif(CASE STREQUAL "control")
  gbl_phase8_check_selection_controls("${GBL_PHASE8_BIN}/phase8-selection-controls")
  gbl_phase8_check_fixture_controls("${GBL_PHASE8_BIN}/phase8-order-controls")
elseif(CASE STREQUAL "order")
  gbl_phase8_check_order_graph()
elseif(CASE STREQUAL "verify")
  gbl_phase8_check_inventories()
  gbl_phase8_discover("${GBL_PHASE8_BIN}" "${GBL_PHASE8_REGEX}" selected_json)
  gbl_phase8_validate_selection("${selected_json}" selection_reason)
  if(NOT selection_reason STREQUAL "")
    message(FATAL_ERROR "phase8-selection: ${selection_reason}")
  endif()
  message(STATUS "phase8-selection: exact-five registered selection and inventory passed")
endif()
