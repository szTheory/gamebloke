cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase8-policy: missing-${required}")
  endif()
endforeach()
get_filename_component(GBL_PHASE8_DEFAULT_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
get_filename_component(GBL_PHASE8_SRC "${SRC}" REALPATH
  BASE_DIR "${GBL_PHASE8_DEFAULT_ROOT}")
get_filename_component(GBL_PHASE8_BIN "${BIN}" REALPATH
  BASE_DIR "${GBL_PHASE8_SRC}")
if(NOT IS_DIRECTORY "${GBL_PHASE8_SRC}" OR NOT IS_DIRECTORY "${GBL_PHASE8_BIN}")
  message(FATAL_ERROR "phase8-policy: source-or-build-directory-missing")
endif()
set(GBL_PHASE8_CHECKER "${GBL_PHASE8_SRC}/cmake/CheckProfile.cmake")
set(GBL_PHASE8_POLICY_GATE "${CMAKE_CURRENT_LIST_FILE}")
find_program(CMAKE_PROGRAM NAMES cmake REQUIRED)

function(gbl_phase8_json_get out json)
  string(JSON value ERROR_VARIABLE error GET "${json}" ${ARGN})
  if(NOT "${error}" STREQUAL "NOTFOUND" AND NOT "${error}" STREQUAL "")
    set(value "")
  endif()
  set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(gbl_phase8_read_observation selector executable expected_type out_var)
  execute_process(
    COMMAND "${executable}" "${selector}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "0" OR
      NOT output MATCHES "TAP version 13" OR
      NOT output MATCHES "1\\..[0-9]+")
    message(FATAL_ERROR
      "phase8-policy: runtime-selector-${selector}-failed (${result})\n${output}${error}")
  endif()
  string(FIND "${output}" "\"type\":\"${expected_type}\"" type_at)
  string(FIND "${output}" "{" object_at)
  if(type_at EQUAL -1 OR object_at EQUAL -1)
    message(FATAL_ERROR "phase8-policy: runtime-observation-${selector}-missing")
  endif()
  string(SUBSTRING "${output}" ${object_at} -1 object_tail)
  string(FIND "${object_tail}" "}" object_end)
  if(object_end EQUAL -1)
    message(FATAL_ERROR "phase8-policy: runtime-observation-${selector}-malformed")
  endif()
  math(EXPR object_length "${object_end}+1")
  string(SUBSTRING "${object_tail}" 0 ${object_length} observation)
  gbl_phase8_json_get(actual_type "${observation}" type)
  if(NOT actual_type STREQUAL expected_type)
    message(FATAL_ERROR
      "phase8-policy: runtime-observation-${selector}-wrong-type-${actual_type}")
  endif()
  set(${out_var} "${observation}" PARENT_SCOPE)
endfunction()

function(gbl_phase8_compare_policy profile record policy_field reason_out)
  gbl_phase8_json_get(expected "${profile}" irq_input_power_policy "${policy_field}")
  gbl_phase8_json_get(actual "${record}" ${ARGN})
  if(NOT "${actual}" STREQUAL "${expected}")
    set(${reason_out} "${policy_field}-runtime-mismatch" PARENT_SCOPE)
  else()
    set(${reason_out} "" PARENT_SCOPE)
  endif()
endfunction()

function(gbl_phase8_check_runtime profile cpu keypad power reason_out)
  set(reason "")
  foreach(pair IN ITEMS
      "timer0_if_bit|if"
      "timer0_if_bit|ie"
      "ime_mask|ime"
      "entry_internal_cycles|internal_cycles")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 policy_field)
    list(GET fields 1 runtime_field)
    gbl_phase8_compare_policy("${profile}" "${cpu}" "${policy_field}"
      reason "${runtime_field}")
    if(NOT reason STREQUAL "")
      set(${reason_out} "cpu-${reason}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  gbl_phase8_json_get(sample_cycle "${cpu}" sample_cycle)
  gbl_phase8_json_get(entry_cycle "${cpu}" entry_cycle)
  gbl_phase8_json_get(next_address "${cpu}" next_address)
  gbl_phase8_json_get(following_address "${cpu}" following_address)
  gbl_phase8_json_get(mask_if "${profile}" irq_input_power_policy if_ie_mask)
  gbl_phase8_json_get(mask_ime "${profile}" irq_input_power_policy ime_mask)
  if(NOT sample_cycle STREQUAL "1" OR
      NOT next_address STREQUAL "134217732" OR
      NOT following_address STREQUAL "134217736")
    set(${reason_out} cpu-sample-boundary-runtime-mismatch PARENT_SCOPE)
    return()
  endif()
  math(EXPR entry_delta "${entry_cycle}-${sample_cycle}")
  gbl_phase8_json_get(entry_cycles "${profile}" irq_input_power_policy entry_internal_cycles)
  if(NOT entry_delta EQUAL entry_cycles)
    set(${reason_out} cpu-entry-cycle-runtime-mismatch PARENT_SCOPE)
    return()
  endif()
  gbl_phase8_json_get(cpu_if "${cpu}" if)
  gbl_phase8_json_get(cpu_ime "${cpu}" ime)
  math(EXPR if_outside_mask "${cpu_if} & ~${mask_if}")
  if(NOT if_outside_mask EQUAL 0 OR cpu_ime GREATER mask_ime)
    set(${reason_out} cpu-register-mask-runtime-mismatch PARENT_SCOPE)
    return()
  endif()
  foreach(pair IN ITEMS "mask_if_clear|0" "mask_ie_clear|0"
      "mask_ime_clear|0" "mask_cpsr_i|1")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 key)
    list(GET fields 1 expected)
    gbl_phase8_json_get(actual "${cpu}" "${key}")
    if(NOT actual STREQUAL expected)
      set(${reason_out} cpu-mask-control-runtime-mismatch PARENT_SCOPE)
      return()
    endif()
  endforeach()

  gbl_phase8_compare_policy("${profile}" "${keypad}" input_event_capacity
    reason capacity)
  if(NOT reason STREQUAL "")
    set(${reason_out} "keypad-${reason}" PARENT_SCOPE)
    return()
  endif()
  gbl_phase8_json_get(keyinput_mask "${profile}" irq_input_power_policy keyinput_mask)
  foreach(index RANGE 0 2)
    string(JSON key_value GET "${keypad}" keyinput_reads ${index})
    if(index EQUAL 1)
      math(EXPR pressed_key_value "${keyinput_mask}-1")
      if(NOT key_value EQUAL pressed_key_value)
        set(${reason_out} keypad-active-low-runtime-mismatch PARENT_SCOPE)
        return()
      endif()
    elseif(NOT key_value EQUAL keyinput_mask)
      set(${reason_out} keypad-active-low-runtime-mismatch PARENT_SCOPE)
      return()
    endif()
  endforeach()
  gbl_phase8_json_get(empty_selection_false "${keypad}" empty_selection_false)
  if(NOT empty_selection_false STREQUAL "ON" AND
      NOT empty_selection_false STREQUAL "true")
    set(${reason_out} keypad-empty-selection-runtime-mismatch PARENT_SCOPE)
    return()
  endif()
  gbl_phase8_json_get(tie_order "${keypad}" tie_order)
  gbl_phase8_json_get(tie_cycle "${keypad}" tie_cycle)
  gbl_phase8_json_get(write_complete "${keypad}" tie_write_complete_cycle)
  gbl_phase8_json_get(write_start "${keypad}" tie_write_start_cycle)
  gbl_phase8_json_get(keypad_priority "${profile}" irq_input_power_policy keypad_event_priority)
  string(JSON timer_priority_0 GET "${profile}" irq_input_power_policy timer_event_priorities 0)
  string(JSON timer_priority_3 GET "${profile}" irq_input_power_policy timer_event_priorities 3)
  if(NOT tie_order STREQUAL "timer0>keypad" OR NOT tie_cycle STREQUAL "2" OR
      NOT write_start STREQUAL "1" OR NOT write_complete STREQUAL tie_cycle OR
      NOT timer_priority_0 STREQUAL "0" OR
      timer_priority_3 GREATER_EQUAL keypad_priority)
    set(${reason_out} keypad-priority-runtime-mismatch PARENT_SCOPE)
    return()
  endif()
  gbl_phase8_json_get(keypad_if_bit "${profile}" irq_input_power_policy keypad_if_bit)
  gbl_phase8_json_get(timer0_if_bit "${profile}" irq_input_power_policy timer0_if_bit)
  math(EXPR both_if_bits "${keypad_if_bit} | ${timer0_if_bit}")
  foreach(pair IN ITEMS "timer_keypad_if|timer_keypad_if"
      "held_if|held_if" "reasserted_if|reasserted_if")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 policy)
    list(GET fields 1 runtime)
    gbl_phase8_json_get(actual "${keypad}" "${runtime}")
    if(policy STREQUAL "timer_keypad_if")
      set(expected ${both_if_bits})
    else()
      set(expected ${keypad_if_bit})
    endif()
    if(NOT actual STREQUAL expected)
      set(${reason_out} keypad-if-runtime-mismatch PARENT_SCOPE)
      return()
    endif()
  endforeach()
  gbl_phase8_json_get(acknowledged_held_if "${keypad}" acknowledged_held_if)
  if(NOT acknowledged_held_if STREQUAL "0")
    set(${reason_out} keypad-held-ack-runtime-mismatch PARENT_SCOPE)
    return()
  endif()

  foreach(pair IN ITEMS
      "timer0_if_bit|pending_if"
      "timer0_if_bit|pending_ie"
      "timer0_if_bit|timer_if"
      "timer0_if_bit|accepted_if"
      "timer0_if_bit|accepted_ie"
      "keypad_if_bit|input_if"
      "stop_cause_code|stop_cause"
      "deadlock_cause_code|deadlock_cause"
      "timeout_reason_code|timeout_reason"
      "entry_internal_cycles|internal_cycles")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 policy_field)
    list(GET fields 1 runtime_field)
    gbl_phase8_compare_policy("${profile}" "${power}" "${policy_field}"
      reason "${runtime_field}")
    if(NOT reason STREQUAL "")
      set(${reason_out} "power-${reason}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  gbl_phase8_json_get(tie_if "${power}" tie_if)
  gbl_phase8_json_get(tie_ie "${power}" tie_ie)
  gbl_phase8_json_get(sample_cycle "${power}" sample_cycle)
  gbl_phase8_json_get(entry_cycle "${power}" entry_cycle)
  gbl_phase8_json_get(bus_cycles "${power}" bus_cycles)
  gbl_phase8_json_get(power_wait_cycles "${power}" wait_cycles)
  gbl_phase8_json_get(power_wake_cycle "${power}" wake_cycle)
  gbl_phase8_json_get(keypad_wake_cycle "${power}" input_wake_cycle)
  if(NOT tie_if STREQUAL both_if_bits OR NOT tie_ie STREQUAL both_if_bits OR
      NOT sample_cycle STREQUAL "3" OR NOT entry_cycle STREQUAL "4" OR
      NOT bus_cycles STREQUAL "0" OR NOT power_wait_cycles STREQUAL "2" OR
      NOT power_wake_cycle STREQUAL sample_cycle OR
      NOT keypad_wake_cycle STREQUAL "2")
    set(${reason_out} power-wake-runtime-mismatch PARENT_SCOPE)
    return()
  endif()

  gbl_phase8_json_get(stop_policy "${profile}" irq_input_power_policy stop_policy)
  gbl_phase8_json_get(deadlock_policy "${profile}" irq_input_power_policy deadlock_policy)
  gbl_phase8_json_get(timeout_policy "${profile}" irq_input_power_policy timeout_policy)
  if(NOT stop_policy STREQUAL "terminal-guest-stop-no-bios-wake" OR
      NOT deadlock_policy STREQUAL "distinct-guest-stop" OR
      NOT timeout_policy STREQUAL "cycle-budget-timeout")
    set(reason power-stop-policy-mismatch)
  endif()
  set(${reason_out} "${reason}" PARENT_SCOPE)
endfunction()

function(gbl_phase8_run_cmake_check description)
  execute_process(
    COMMAND "${CMAKE_PROGRAM}" ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "phase8-policy: ${description} failed\n${output}${error}")
  endif()
endfunction()

if(CASE STREQUAL "prepare-red")
  set(scratch_root "${GBL_PHASE8_BIN}/phase8-policy-red")
  set(source_dir "${scratch_root}/source")
  set(build_dir "${scratch_root}/build")
  file(MAKE_DIRECTORY "${source_dir}")
  file(WRITE "${source_dir}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.25)\nproject(phase8_policy_red NONE)\nenable_testing()\nadd_test(NAME phase8.policy.missing_policy COMMAND \"${CMAKE_COMMAND}\" -DSRC=${GBL_PHASE8_SRC} -DBIN=${GBL_PHASE8_BIN} -DCASE=missing-policy -P \"${GBL_PHASE8_POLICY_GATE}\")\n")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${source_dir}" -B "${build_dir}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
  if(NOT "${configure_result}" STREQUAL "0")
    message(FATAL_ERROR
      "phase8-policy: red-project-configure\n${configure_output}${configure_error}")
  endif()
  message(STATUS "phase8-policy: scratch RED selector prepared")

elseif(CASE STREQUAL "missing-policy")
  set(work_dir "${GBL_PHASE8_BIN}/phase8-policy-controls/missing-policy")
  file(REMOVE_RECURSE "${work_dir}")
  file(MAKE_DIRECTORY "${work_dir}")
  file(COPY_FILE "${GBL_PHASE8_SRC}/profiles/agb.json"
    "${work_dir}/profile.json")
  file(READ "${work_dir}/profile.json" profile_json)
  string(JSON policy_type ERROR_VARIABLE policy_error TYPE
    "${profile_json}" irq_input_power_policy)
  if("${policy_error}" STREQUAL "NOTFOUND")
    string(JSON profile_json REMOVE "${profile_json}" irq_input_power_policy)
    file(WRITE "${work_dir}/profile.json" "${profile_json}\n")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DPROFILE=${work_dir}/profile.json"
      "-DDOC=${GBL_PHASE8_SRC}/docs/profile-agb.md"
      "-DREGISTRY=${GBL_PHASE8_SRC}/registry/disputes.json"
      "-DVOCAB=${GBL_PHASE8_SRC}/contracts/evidence.json"
      -P "${GBL_PHASE8_CHECKER}"
    RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
  set(check_combined "${check_output}${check_error}")
  if("${check_result}" STREQUAL "0")
    message(FATAL_ERROR "phase8-policy: missing-policy-accepted")
  endif()
  if(NOT check_combined MATCHES "profile: irq-policy-missing: irq_input_power_policy")
    message(FATAL_ERROR
      "phase8-policy: missing-policy-wrong-reason\n${check_combined}")
  endif()
  message(STATUS "phase8-policy: missing policy table rejected")

elseif(NOT DEFINED CASE OR "${CASE}" STREQUAL "")
  execute_process(
    COMMAND "${CMAKE_PROGRAM}"
      "-DPROFILE=${GBL_PHASE8_SRC}/profiles/agb.json"
      "-DDOC=${GBL_PHASE8_SRC}/docs/profile-agb.md"
      "-DREGISTRY=${GBL_PHASE8_SRC}/registry/disputes.json"
      "-DVOCAB=${GBL_PHASE8_SRC}/contracts/evidence.json"
      -P "${GBL_PHASE8_CHECKER}"
    RESULT_VARIABLE profile_result OUTPUT_VARIABLE profile_output
    ERROR_VARIABLE profile_error)
  if(NOT "${profile_result}" STREQUAL "0")
    message(FATAL_ERROR
      "phase8-policy: profile-check-failed\n${profile_output}${profile_error}")
  endif()

  if(NOT EXISTS "${GBL_PHASE8_BIN}/cpu_test" OR
      NOT EXISTS "${GBL_PHASE8_BIN}/timer_test")
    message(FATAL_ERROR "phase8-policy: focused-runtime-binaries-missing")
  endif()
  file(READ "${GBL_PHASE8_SRC}/profiles/agb.json" profile_json)
  gbl_phase8_read_observation(irq.boundary "${GBL_PHASE8_BIN}/cpu_test"
    phase8-observation cpu_json)
  gbl_phase8_read_observation(keypad.timeline "${GBL_PHASE8_BIN}/timer_test"
    keypad keypad_json)
  gbl_phase8_read_observation(time.halt.boundary "${GBL_PHASE8_BIN}/timer_test"
    phase8-observation power_json)
  gbl_phase8_check_runtime("${profile_json}" "${cpu_json}" "${keypad_json}"
    "${power_json}" runtime_reason)
  if(NOT runtime_reason STREQUAL "")
    message(FATAL_ERROR "phase8-policy: ${runtime_reason}")
  endif()

  gbl_phase8_run_cmake_check("profile-drift-controls"
    "-DSRC=${GBL_PHASE8_SRC}" "-DBIN=${GBL_PHASE8_BIN}" -DCASE=irq_policy
    -P "${GBL_PHASE8_SRC}/tests/gates/profile_case.cmake")
  gbl_phase8_run_cmake_check("missing-policy-control"
    "-DSRC=${GBL_PHASE8_SRC}" "-DBIN=${GBL_PHASE8_BIN}"
    -DCASE=missing-policy -P "${GBL_PHASE8_POLICY_GATE}")
  message(STATUS
    "phase8-policy: runtime observations, profile/document policy, and causal drift controls passed")

else()
  message(FATAL_ERROR "phase8-policy: invalid-case")
endif()
