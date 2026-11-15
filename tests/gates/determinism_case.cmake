cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "determinism-case: usage: -D${required}= is required")
  endif()
endforeach()
if(NOT CASE STREQUAL "repeat-process" AND NOT CASE STREQUAL "controls")
  message(FATAL_ERROR "determinism-case: unknown CASE=${CASE}")
endif()

get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)
set(runner "${BIN}/gamebloke-run-control")
set(fixtures "${BIN}/generated/fixtures")
set(control_manifest "${BIN}/generated/control/manifest.json")
set(registry "${SRC}/registry/disputes.json")
set(vocabulary "${SRC}/contracts/evidence.json")
set(result_checker "${SRC}/cmake/CheckResult.cmake")
set(work_root "${BIN}/determinism-process")

foreach(path IN ITEMS "${runner}" "${fixtures}/owned-arm-pass.gba"
    "${fixtures}/owned-arm-fail.gba" "${fixtures}/owned-arm-loop.gba"
    "${fixtures}/owned-arm-swi.gba" "${control_manifest}"
    "${registry}" "${vocabulary}" "${result_checker}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "determinism-case: required input is missing: ${path}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${work_root}")
string(RANDOM LENGTH 20 ALPHABET 0123456789abcdef run_id)
set(work "${work_root}/${CASE}.${run_id}")
if(EXISTS "${work}")
  message(FATAL_ERROR "determinism-case: exclusive scratch path already exists")
endif()
file(MAKE_DIRECTORY "${work}")

function(run_process json_out trace_out cycles_out instructions_out semantic_out
    image fixture_id budget expected_exit expected_status expected_reason
    expected_semantic expected_termination validate_authority output_stem)
  set(rom "${fixtures}/${image}")
  set(json_path "${work}/${output_stem}.result.json")
  set(trace_path "${work}/${output_stem}.trace.jsonl")
  if(NOT EXISTS "${rom}" OR EXISTS "${json_path}" OR EXISTS "${trace_path}")
    message(FATAL_ERROR "determinism-case: input missing or output path is not exclusive")
  endif()

  set(command "${runner}" --rom "${rom}")
  if(NOT "${fixture_id}" STREQUAL "")
    list(APPEND command --fixture "${fixture_id}")
  endif()
  list(APPEND command --cycles "${budget}" --trace-mask 7
    --trace-capacity 65536 --trace-output "${trace_path}")
  execute_process(
    COMMAND ${command}
    RESULT_VARIABLE process_result
    OUTPUT_FILE "${json_path}"
    ERROR_VARIABLE process_error)
  if(NOT "${process_result}" STREQUAL "${expected_exit}")
    message(FATAL_ERROR
      "determinism-case: ${output_stem} exited ${process_result}, expected ${expected_exit}; stderr=${process_error}")
  endif()
  foreach(output IN ITEMS "${json_path}" "${trace_path}")
    if(NOT EXISTS "${output}" OR IS_DIRECTORY "${output}")
      message(FATAL_ERROR "determinism-case: output missing: ${output}")
    endif()
    file(SIZE "${output}" output_size)
    if(output_size LESS 1)
      message(FATAL_ERROR "determinism-case: empty output: ${output}")
    endif()
  endforeach()

  file(READ "${json_path}" document)
  string(REGEX MATCH "^[{][^\n]*[}]\n$" one_json_line "${document}")
  if(NOT "${one_json_line}" STREQUAL "${document}")
    message(FATAL_ERROR "determinism-case: ${output_stem} did not emit one JSON line")
  endif()
  string(FIND "${document}" "${SRC}" source_path_offset)
  string(FIND "${document}" "${BIN}" build_path_offset)
  if(NOT source_path_offset EQUAL -1 OR NOT build_path_offset EQUAL -1)
    message(FATAL_ERROR "determinism-case: result exposed a host path")
  endif()

  string(JSON actual_status ERROR_VARIABLE status_error GET "${document}" status)
  string(JSON actual_reason ERROR_VARIABLE reason_error GET "${document}" reason)
  string(JSON actual_semantic ERROR_VARIABLE semantic_error GET "${document}" semantic state)
  string(JSON actual_termination ERROR_VARIABLE termination_error GET "${document}" termination)
  string(JSON actual_ran ERROR_VARIABLE ran_error GET "${document}" fixture ran)
  string(JSON cycles ERROR_VARIABLE cycles_error GET "${document}" time cycles)
  string(JSON instructions ERROR_VARIABLE instructions_error GET "${document}" time instructions)
  string(JSON reported_budget ERROR_VARIABLE budget_error GET "${document}" time budget_cycles)
  string(JSON trace_complete ERROR_VARIABLE trace_complete_error GET "${document}" trace complete)
  string(JSON trace_emitted ERROR_VARIABLE trace_emitted_error GET "${document}" trace emitted)
  string(JSON trace_count ERROR_VARIABLE trace_count_error GET "${document}" trace count)
  foreach(error IN ITEMS "${status_error}" "${reason_error}"
      "${semantic_error}" "${termination_error}" "${ran_error}"
      "${cycles_error}" "${instructions_error}" "${budget_error}"
      "${trace_complete_error}" "${trace_emitted_error}" "${trace_count_error}")
    if(NOT "${error}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "determinism-case: malformed result JSON: ${error}")
    endif()
  endforeach()
  if(NOT "${actual_status}" STREQUAL "${expected_status}" OR
      NOT "${actual_reason}" STREQUAL "${expected_reason}" OR
      NOT "${actual_semantic}" STREQUAL "${expected_semantic}" OR
      NOT "${actual_termination}" STREQUAL "${expected_termination}" OR
      NOT actual_ran OR NOT trace_complete OR NOT trace_emitted OR
      trace_count LESS 1 OR cycles LESS 1 OR instructions LESS 1 OR
      cycles GREATER budget OR NOT "${reported_budget}" STREQUAL "${budget}")
    message(FATAL_ERROR
      "determinism-case: ${output_stem} has unexpected status, semantic state, completion, or guest progress")
  endif()
  file(READ "${trace_path}" trace_document)
  string(ASCII 10 newline)
  string(LENGTH "${trace_document}" trace_length)
  if(trace_length LESS 1)
    message(FATAL_ERROR "determinism-case: ${output_stem} trace is empty")
  endif()
  math(EXPR trace_last_index "${trace_length} - 1")
  string(SUBSTRING "${trace_document}" ${trace_last_index} 1 trace_last_character)
  string(REGEX MATCHALL "${newline}" trace_lines "${trace_document}")
  list(LENGTH trace_lines trace_line_count)
  if(NOT "${trace_last_character}" STREQUAL "${newline}" OR
      NOT "${trace_line_count}" STREQUAL "${trace_count}")
    message(FATAL_ERROR
      "determinism-case: ${output_stem} trace is truncated or disagrees with its record count")
  endif()

  if(validate_authority)
    execute_process(
      COMMAND "${CMAKE_COMMAND}"
        "-DRESULT=${json_path}" "-DMANIFEST=${control_manifest}"
        "-DREGISTRY=${registry}" "-DVOCAB=${vocabulary}"
        -P "${result_checker}"
      RESULT_VARIABLE validator_result
      OUTPUT_VARIABLE validator_output
      ERROR_VARIABLE validator_error)
    if(NOT "${validator_result}" STREQUAL "0")
      message(FATAL_ERROR
        "determinism-case: canonical result validation failed for ${output_stem}; ${validator_output}${validator_error}")
    endif()
  endif()

  set(${json_out} "${json_path}" PARENT_SCOPE)
  set(${trace_out} "${trace_path}" PARENT_SCOPE)
  set(${cycles_out} "${cycles}" PARENT_SCOPE)
  set(${instructions_out} "${instructions}" PARENT_SCOPE)
  set(${semantic_out} "${actual_semantic}" PARENT_SCOPE)
endfunction()

function(compare_pair json_a trace_a json_b trace_b reason_out)
  foreach(path IN ITEMS "${json_a}" "${json_b}")
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
      set(${reason_out} "result-output-missing" PARENT_SCOPE)
      return()
    endif()
    file(SIZE "${path}" size)
    if(size LESS 1)
      set(${reason_out} "result-output-empty" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  foreach(path IN ITEMS "${trace_a}" "${trace_b}")
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
      set(${reason_out} "trace-output-missing" PARENT_SCOPE)
      return()
    endif()
    file(SIZE "${path}" size)
    if(size LESS 1)
      set(${reason_out} "trace-output-empty" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${json_a}" "${json_b}"
    RESULT_VARIABLE result_compare)
  if(NOT "${result_compare}" STREQUAL "0")
    set(${reason_out} "result-bytes-differ" PARENT_SCOPE)
    return()
  endif()
  file(SHA256 "${trace_a}" trace_hash_a)
  file(SHA256 "${trace_b}" trace_hash_b)
  if(NOT "${trace_hash_a}" STREQUAL "${trace_hash_b}")
    set(${reason_out} "trace-hash-differs" PARENT_SCOPE)
    return()
  endif()

  file(READ "${json_a}" doc_a)
  file(READ "${json_b}" doc_b)
  foreach(field IN ITEMS "status" "semantic;state" "time;cycles"
      "time;instructions")
    string(JSON value_a ERROR_VARIABLE error_a GET "${doc_a}" ${field})
    string(JSON value_b ERROR_VARIABLE error_b GET "${doc_b}" ${field})
    if(NOT "${error_a}" STREQUAL "NOTFOUND" OR
        NOT "${error_b}" STREQUAL "NOTFOUND")
      set(${reason_out} "result-fields-invalid" PARENT_SCOPE)
      return()
    endif()
    if(NOT "${value_a}" STREQUAL "${value_b}")
      set(${reason_out} "semantic-or-progress-differs" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${reason_out} "ok" PARENT_SCOPE)
endfunction()

function(require_pair_reason expected json_a trace_a json_b trace_b label)
  compare_pair("${json_a}" "${trace_a}" "${json_b}" "${trace_b}" actual)
  if(NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "determinism-case: ${label} returned ${actual}, expected ${expected}")
  endif()
  message(STATUS "determinism-case: ${label} -> ${actual}")
endfunction()

if(CASE STREQUAL "repeat-process")
  run_process(pass_json_a pass_trace_a pass_cycles_a pass_instructions_a
    pass_semantic_a owned-arm-pass.gba synthetic-control-pass 1000 0 PASS
    guest-mailbox-pass pass mailbox-complete TRUE pass_a)
  run_process(pass_json_b pass_trace_b pass_cycles_b pass_instructions_b
    pass_semantic_b owned-arm-pass.gba synthetic-control-pass 1000 0 PASS
    guest-mailbox-pass pass mailbox-complete TRUE pass_b)
  compare_pair("${pass_json_a}" "${pass_trace_a}" "${pass_json_b}"
    "${pass_trace_b}" pass_reason)
  if(NOT "${pass_reason}" STREQUAL "ok" OR
      NOT "${pass_cycles_a}" STREQUAL "${pass_cycles_b}" OR
      NOT "${pass_instructions_a}" STREQUAL "${pass_instructions_b}" OR
      NOT "${pass_semantic_a}" STREQUAL "${pass_semantic_b}")
    message(FATAL_ERROR "determinism-case: pass process pair differed: ${pass_reason}")
  endif()

  run_process(fail_json_a fail_trace_a fail_cycles_a fail_instructions_a
    fail_semantic_a owned-arm-fail.gba synthetic-control-fail 1000 1 GUEST_FAIL
    guest-mailbox-fail fail mailbox-complete TRUE fail_a)
  run_process(fail_json_b fail_trace_b fail_cycles_b fail_instructions_b
    fail_semantic_b owned-arm-fail.gba synthetic-control-fail 1000 1 GUEST_FAIL
    guest-mailbox-fail fail mailbox-complete TRUE fail_b)
  compare_pair("${fail_json_a}" "${fail_trace_a}" "${fail_json_b}"
    "${fail_trace_b}" fail_reason)
  if(NOT "${fail_reason}" STREQUAL "ok" OR
      NOT "${fail_cycles_a}" STREQUAL "${fail_cycles_b}" OR
      NOT "${fail_instructions_a}" STREQUAL "${fail_instructions_b}" OR
      NOT "${fail_semantic_a}" STREQUAL "${fail_semantic_b}")
    message(FATAL_ERROR "determinism-case: mailbox-fail pair differed: ${fail_reason}")
  endif()

  run_process(stop_json_a stop_trace_a stop_cycles_a stop_instructions_a
    stop_semantic_a owned-arm-swi.gba "" 1000 1 GUEST_STOP no-bios-swi absent
    no-bios-swi FALSE stop_a)
  run_process(stop_json_b stop_trace_b stop_cycles_b stop_instructions_b
    stop_semantic_b owned-arm-swi.gba "" 1000 1 GUEST_STOP no-bios-swi absent
    no-bios-swi FALSE stop_b)
  compare_pair("${stop_json_a}" "${stop_trace_a}" "${stop_json_b}"
    "${stop_trace_b}" stop_reason)
  if(NOT "${stop_reason}" STREQUAL "ok" OR
      NOT "${stop_cycles_a}" STREQUAL "${stop_cycles_b}" OR
      NOT "${stop_instructions_a}" STREQUAL "${stop_instructions_b}" OR
      NOT "${stop_semantic_a}" STREQUAL "${stop_semantic_b}")
    message(FATAL_ERROR "determinism-case: raw guest-stop pair differed: ${stop_reason}")
  endif()

  run_process(timeout_json_a timeout_trace_a timeout_cycles_a
    timeout_instructions_a timeout_semantic_a owned-arm-loop.gba "" 32 1
    TIMEOUT cycle-budget absent timeout FALSE timeout_a)
  run_process(timeout_json_b timeout_trace_b timeout_cycles_b
    timeout_instructions_b timeout_semantic_b owned-arm-loop.gba "" 32 1
    TIMEOUT cycle-budget absent timeout FALSE timeout_b)
  compare_pair("${timeout_json_a}" "${timeout_trace_a}" "${timeout_json_b}"
    "${timeout_trace_b}" timeout_reason)
  if(NOT "${timeout_reason}" STREQUAL "ok" OR
      NOT "${timeout_cycles_a}" STREQUAL "${timeout_cycles_b}" OR
      NOT "${timeout_instructions_a}" STREQUAL "${timeout_instructions_b}" OR
      NOT "${timeout_semantic_a}" STREQUAL "${timeout_semantic_b}")
    message(FATAL_ERROR "determinism-case: bounded timeout pair differed: ${timeout_reason}")
  endif()
  message(STATUS
    "determinism-case: PASS, GUEST_FAIL, GUEST_STOP and TIMEOUT agree across fresh processes")
elseif(CASE STREQUAL "controls")
  run_process(clean_json_a clean_trace_a clean_cycles_a clean_instructions_a
    clean_semantic_a owned-arm-pass.gba synthetic-control-pass 1000 0 PASS
    guest-mailbox-pass pass mailbox-complete TRUE controls_a)
  run_process(clean_json_b clean_trace_b clean_cycles_b clean_instructions_b
    clean_semantic_b owned-arm-pass.gba synthetic-control-pass 1000 0 PASS
    guest-mailbox-pass pass mailbox-complete TRUE controls_b)
  require_pair_reason(ok "${clean_json_a}" "${clean_trace_a}"
    "${clean_json_b}" "${clean_trace_b}" clean-companion-before-controls)

  file(READ "${clean_json_a}" clean_json_text)
  string(REPLACE "\"status\":\"PASS\"" "\"status\":\"XASS\""
    tampered_json_text "${clean_json_text}")
  string(LENGTH "${clean_json_text}" clean_json_length)
  string(LENGTH "${tampered_json_text}" tampered_json_length)
  if("${tampered_json_text}" STREQUAL "${clean_json_text}" OR
      NOT clean_json_length EQUAL tampered_json_length)
    message(FATAL_ERROR "determinism-case: could not perform one-byte result mutation")
  endif()
  set(tampered_json "${work}/tampered.result.json")
  file(WRITE "${tampered_json}" "${tampered_json_text}")
  require_pair_reason(result-bytes-differ "${tampered_json}" "${clean_trace_a}"
    "${clean_json_b}" "${clean_trace_b}" result-byte-mutation)

  file(READ "${clean_trace_a}" clean_trace_text)
  string(REPLACE "\"sequence\":\"0\"" "\"sequence\":\"1\""
    tampered_trace_text "${clean_trace_text}")
  string(LENGTH "${clean_trace_text}" clean_trace_length)
  string(LENGTH "${tampered_trace_text}" tampered_trace_length)
  if("${tampered_trace_text}" STREQUAL "${clean_trace_text}" OR
      NOT clean_trace_length EQUAL tampered_trace_length)
    message(FATAL_ERROR "determinism-case: could not perform one-byte trace mutation")
  endif()
  set(tampered_trace "${work}/tampered.trace.jsonl")
  file(WRITE "${tampered_trace}" "${tampered_trace_text}")
  require_pair_reason(trace-hash-differs "${clean_json_a}" "${tampered_trace}"
    "${clean_json_b}" "${clean_trace_b}" trace-byte-mutation)

  set(empty_json "${work}/empty.result.json")
  file(WRITE "${empty_json}" "")
  require_pair_reason(result-output-empty "${empty_json}" "${clean_trace_a}"
    "${clean_json_b}" "${clean_trace_b}" empty-result-control)

  set(missing_trace "${work}/missing.trace.jsonl")
  require_pair_reason(trace-output-missing "${clean_json_a}" "${missing_trace}"
    "${clean_json_b}" "${clean_trace_b}" missing-trace-control)
  require_pair_reason(ok "${clean_json_a}" "${clean_trace_a}"
    "${clean_json_b}" "${clean_trace_b}" clean-companion-after-controls)
  message(STATUS
    "determinism-case: result-byte, trace-byte, empty-result and missing-trace controls rejected")
endif()

file(REMOVE_RECURSE "${work}")
