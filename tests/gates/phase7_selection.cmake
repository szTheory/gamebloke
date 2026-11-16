cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase7-selection: missing-${required}")
  endif()
endforeach()
if(NOT CASE MATCHES "^(verify|control|negative)$")
  message(FATAL_ERROR "phase7-selection: invalid-case")
endif()
find_program(CTEST_PROGRAM NAMES ctest REQUIRED)

# This contract is fixed in source. It is never read from CTest or inventory.
set(GBL_PHASE7_EXPECTED
  time.events.order
  time.events.boundary
  time.events.refusal
  timer0.model.boundary
  phase7.selection.control)
set(GBL_PHASE7_REGEX
  "^(time[.]events[.](order|boundary|refusal)|timer0[.]model[.]boundary|phase7[.]selection[.]control)$")
set(GBL_PHASE7_GATE "${CMAKE_CURRENT_LIST_FILE}")
set(GBL_PHASE7_INVENTORY_ROWS
  "time.events.order core,time,timer,unit"
  "time.events.boundary core,time,timer,unit"
  "time.events.refusal core,time,timer,unit"
  "timer0.model.boundary core,time,timer,unit"
  "timer0.guest.tracer core,integration,timer"
  "timer.devices core,time,timer,unit"
  "phase7.selection.control gate,time,timer")

function(gbl__discover regex output_var)
  execute_process(
    COMMAND "${CTEST_PROGRAM}" --test-dir "${BIN}" --show-only=json-v1
      --fixture-exclude-any phase7_fast -R "${regex}"
    RESULT_VARIABLE result OUTPUT_VARIABLE json ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR
      "phase7-selection: discovery: ${result}\n${error}")
  endif()
  set(${output_var} "${json}" PARENT_SCOPE)
endfunction()

function(gbl__validate_selection json reason_out)
  string(JSON kind ERROR_VARIABLE kind_error GET "${json}" kind)
  if(NOT "${kind_error}" STREQUAL "NOTFOUND" OR
     NOT "${kind}" STREQUAL "ctestInfo")
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
  list(LENGTH GBL_PHASE7_EXPECTED expected_count)
  math(EXPR broad_limit "${expected_count} + 1")
  if(count GREATER broad_limit)
    set(${reason_out} broad PARENT_SCOPE)
    return()
  endif()

  set(actual_names)
  math(EXPR last_index "${count} - 1")
  foreach(index RANGE 0 ${last_index})
    string(JSON name ERROR_VARIABLE name_error GET "${json}" tests ${index}
      name)
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

  foreach(expected IN LISTS GBL_PHASE7_EXPECTED)
    list(FIND actual_names "${expected}" expected_index)
    if(expected_index EQUAL -1)
      set(${reason_out} missing PARENT_SCOPE)
      return()
    endif()
  endforeach()

  set(extra_count 0)
  foreach(actual IN LISTS actual_names)
    list(FIND GBL_PHASE7_EXPECTED "${actual}" expected_index)
    if(expected_index EQUAL -1)
      math(EXPR extra_count "${extra_count} + 1")
    endif()
  endforeach()
  if(extra_count GREATER 0)
    set(${reason_out} extra PARENT_SCOPE)
    return()
  endif()
  list(LENGTH actual_names actual_count)
  list(LENGTH GBL_PHASE7_EXPECTED expected_count)
  if(NOT actual_count EQUAL expected_count)
    set(${reason_out} missing PARENT_SCOPE)
    return()
  endif()
  set(${reason_out} "" PARENT_SCOPE)
endfunction()

function(gbl__write_scratch_inventory json path)
  string(JSON count ERROR_VARIABLE count_error LENGTH "${json}" tests)
  if(NOT "${count_error}" STREQUAL "NOTFOUND")
    file(WRITE "${path}" "malformed selection JSON\n")
    return()
  endif()
  file(WRITE "${path}" "")
  if(count GREATER 0)
    math(EXPR last_index "${count} - 1")
    foreach(index RANGE 0 ${last_index})
      string(JSON name GET "${json}" tests ${index} name)
      file(APPEND "${path}" "${name}\n")
    endforeach()
  endif()
endfunction()

function(gbl__check_inventories)
  foreach(lane IN ITEMS ci asan)
    set(path "${SRC}/tests/inventory/${lane}.txt")
    if(NOT EXISTS "${path}")
      message(FATAL_ERROR "phase7-selection: inventory-missing-${lane}")
    endif()
    foreach(expected_row IN LISTS GBL_PHASE7_INVENTORY_ROWS)
      string(REGEX MATCH "^([^ ]+)" name "${expected_row}")
      string(REPLACE "." "[.]" name_regex "${name}")
      file(STRINGS "${path}" matching_rows REGEX "^${name_regex} ")
      list(LENGTH matching_rows row_count)
      if(NOT row_count EQUAL 1)
        message(FATAL_ERROR
          "phase7-selection: inventory-${lane}-${name}-count-${row_count}")
      endif()
      list(GET matching_rows 0 actual_row)
      if(NOT "${actual_row}" STREQUAL "${expected_row}")
        message(FATAL_ERROR
          "phase7-selection: inventory-${lane}-${name}-labels")
      endif()
    endforeach()
  endforeach()
endfunction()

function(gbl__run_negative fault)
  set(output_dir "${BIN}/phase7-selection-controls/${fault}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${SRC}" "-DBIN=${BIN}"
      -DCASE=negative "-DFAULT=${fault}" "-DOUTPUT_DIR=${output_dir}"
      -P "${GBL_PHASE7_GATE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  set(combined "${output}${error}")
  if("${result}" STREQUAL "0" OR
     NOT "${combined}" MATCHES "phase7-selection: ${fault}([\n]|$)")
    message(FATAL_ERROR
      "phase7-selection-control: ${fault}: wrong-result (${result})\n${combined}")
  endif()
  message(STATUS "phase7-selection-control: ${fault}: causal rejection ok")
endfunction()

function(gbl__run_fault fault output_dir)
  if(NOT DEFINED OUTPUT_DIR OR "${OUTPUT_DIR}" STREQUAL "")
    message(FATAL_ERROR "phase7-selection: missing-output-directory")
  endif()
  file(MAKE_DIRECTORY "${output_dir}")
  if(fault STREQUAL "empty")
    gbl__discover("^$" json)
    gbl__validate_selection("${json}" empty_reason)
    if(NOT "${empty_reason}" STREQUAL "empty")
      message(FATAL_ERROR "phase7-selection: empty-control-wrong-reason")
    endif()
    gbl__discover("^phase7[.]selection[.]nonexistent$" nonexistent_json)
    gbl__validate_selection("${nonexistent_json}" nonexistent_reason)
    if(NOT "${nonexistent_reason}" STREQUAL "empty")
      message(FATAL_ERROR "phase7-selection: nonexistent-control-wrong-reason")
    endif()
    file(WRITE "${output_dir}/nonexistent-selection.json"
      "${nonexistent_json}")
    gbl__write_scratch_inventory("${nonexistent_json}"
      "${output_dir}/nonexistent-inventory.txt")
  elseif(fault STREQUAL "missing")
    gbl__discover(
      "^(time[.]events[.](order|boundary|refusal)|timer0[.]model[.]boundary)$"
      json)
  elseif(fault STREQUAL "extra")
    gbl__discover(
      "^(time[.]events[.](order|boundary|refusal)|timer0[.]model[.]boundary|phase7[.]selection[.]control|bus[.]decode)$"
      json)
  elseif(fault STREQUAL "broad")
    gbl__discover("^.*$" json)
  elseif(fault STREQUAL "duplicate")
    set(json
      "{\"kind\":\"ctestInfo\",\"tests\":[{\"name\":\"time.events.order\"},{\"name\":\"time.events.order\"}]}")
  elseif(fault STREQUAL "malformed")
    set(json "{\"kind\":\"ctestInfo\",\"tests\":[")
  else()
    message(FATAL_ERROR "phase7-selection: unsupported-control")
  endif()
  file(WRITE "${output_dir}/selection.json" "${json}")
  gbl__write_scratch_inventory("${json}" "${output_dir}/inventory.txt")
  gbl__validate_selection("${json}" observed_reason)
  if(NOT "${observed_reason}" STREQUAL "${fault}")
    message(FATAL_ERROR
      "phase7-selection: wrong-reason-expected-${fault}-got-${observed_reason}")
  endif()
  message(FATAL_ERROR "phase7-selection: ${observed_reason}")
endfunction()

if(CASE STREQUAL "negative")
  if(NOT DEFINED FAULT OR "${FAULT}" STREQUAL "")
    message(FATAL_ERROR "phase7-selection: missing-fault")
  endif()
  gbl__run_fault("${FAULT}" "${OUTPUT_DIR}")
else()
  gbl__check_inventories()
  gbl__discover("${GBL_PHASE7_REGEX}" selected_json)
  gbl__validate_selection("${selected_json}" selection_reason)
  if(NOT "${selection_reason}" STREQUAL "")
    message(FATAL_ERROR "phase7-selection: ${selection_reason}")
  endif()
  if(CASE STREQUAL "control")
    foreach(fault IN ITEMS empty missing extra broad duplicate malformed)
      gbl__run_negative("${fault}")
    endforeach()
  endif()
  message(STATUS
    "phase7-selection: exact-five verified; inventory and controls passed")
endif()
