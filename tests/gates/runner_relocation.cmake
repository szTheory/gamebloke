cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN MODE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "runner-relocation: usage: -D${required}= is required")
  endif()
endforeach()
if(NOT MODE STREQUAL "clean" AND NOT MODE STREQUAL "missing-executable")
  message(FATAL_ERROR "runner-relocation: unknown-mode: ${MODE}")
endif()

get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)
set(build_runner "${BIN}/gamebloke-run")
set(fixture "${BIN}/generated/fixtures/owned-arm-pass.gba")
if(NOT EXISTS "${build_runner}" OR IS_DIRECTORY "${build_runner}")
  message(FATAL_ERROR "runner-relocation: missing-build-executable")
endif()
if(NOT EXISTS "${fixture}" OR IS_DIRECTORY "${fixture}")
  message(FATAL_ERROR "runner-relocation: missing-owned-image")
endif()

set(sandbox "${BIN}/runner-relocation-${MODE}")
file(REMOVE_RECURSE "${sandbox}")
set(original_prefix "${sandbox}/original")
set(moved_prefix "${sandbox}/moved")
set(cwd "${sandbox}/unrelated-cwd")
file(MAKE_DIRECTORY "${original_prefix}" "${cwd}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${BIN}" --prefix "${original_prefix}"
  RESULT_VARIABLE install_result
  OUTPUT_VARIABLE install_stdout
  ERROR_VARIABLE install_stderr)
if(NOT install_result STREQUAL "0")
  message(FATAL_ERROR
    "runner-relocation: install-failed: ${install_stdout}${install_stderr}")
endif()
file(RENAME "${original_prefix}" "${moved_prefix}")

function(gbl_check_relocation prefix tag out_ok out_reason out_detail)
  set(installed_runner "${prefix}/bin/gamebloke-run")
  if(NOT EXISTS "${installed_runner}" OR IS_DIRECTORY "${installed_runner}")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "missing-executable" PARENT_SCOPE)
    set(${out_detail} "${installed_runner}" PARENT_SCOPE)
    return()
  endif()

  set(build_trace "${sandbox}/${tag}-build.jsonl")
  set(installed_trace "${sandbox}/${tag}-installed.jsonl")
  foreach(trace_file IN ITEMS "${build_trace}" "${installed_trace}")
    file(REMOVE "${trace_file}")
  endforeach()

  set(clean_env
    "${CMAKE_COMMAND}" -E env
    --unset=GBL_FIXTURE_INDEX
    --unset=GBL_CONTROL_INDEX
    --unset=GBL_MANIFEST
    --unset=GBL_SOURCE_DIR
    --unset=GBL_PRODUCTION_INDEX)
  execute_process(
    COMMAND ${clean_env} "${build_runner}"
      --rom "${fixture}" --cycles 1000
    WORKING_DIRECTORY "${cwd}"
    RESULT_VARIABLE build_status
    OUTPUT_VARIABLE build_json
    ERROR_VARIABLE build_error)
  execute_process(
    COMMAND ${clean_env} "${build_runner}"
      --rom "${fixture}" --cycles 1000
      --trace-mask 7 --trace-capacity 65536 --trace-output "${build_trace}"
    WORKING_DIRECTORY "${cwd}"
    RESULT_VARIABLE build_trace_status
    OUTPUT_VARIABLE build_trace_json
    ERROR_VARIABLE build_trace_error)
  execute_process(
    COMMAND ${clean_env} "${installed_runner}"
      --rom "${fixture}" --cycles 1000
    WORKING_DIRECTORY "${cwd}"
    RESULT_VARIABLE installed_status
    OUTPUT_VARIABLE installed_json
    ERROR_VARIABLE installed_error)
  execute_process(
    COMMAND ${clean_env} "${installed_runner}"
      --rom "${fixture}" --cycles 1000
      --trace-mask 7 --trace-capacity 65536 --trace-output "${installed_trace}"
    WORKING_DIRECTORY "${cwd}"
    RESULT_VARIABLE installed_trace_status
    OUTPUT_VARIABLE installed_trace_json
    ERROR_VARIABLE installed_trace_error)

  if(NOT "${build_status}" STREQUAL "${installed_status}" OR
      NOT "${build_trace_status}" STREQUAL "${installed_trace_status}")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "exit-status-mismatch" PARENT_SCOPE)
    set(${out_detail}
      "build=${build_status}; installed=${installed_status}; trace-build=${build_trace_status}; trace-installed=${installed_trace_status}"
      PARENT_SCOPE)
    return()
  endif()
  if(NOT build_status STREQUAL "1" OR NOT build_trace_status STREQUAL "1")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "unexpected-raw-exit-status" PARENT_SCOPE)
    set(${out_detail}
      "expected unregistered raw diagnostic exit 1; got ${build_status}/${build_trace_status}"
      PARENT_SCOPE)
    return()
  endif()
  if(NOT "${build_json}" STREQUAL "${installed_json}" OR
      NOT "${build_trace_json}" STREQUAL "${installed_trace_json}")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "json-mismatch" PARENT_SCOPE)
    set(${out_detail} "build-tree and moved installed output bytes differ" PARENT_SCOPE)
    return()
  endif()
  if(NOT "${build_trace_json}" STREQUAL "${installed_trace_json}")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "trace-json-mismatch" PARENT_SCOPE)
    set(${out_detail} "trace-enabled JSON bytes differ" PARENT_SCOPE)
    return()
  endif()
  if(NOT "${build_error}" STREQUAL "" OR
      NOT "${build_trace_error}" STREQUAL "" OR
      NOT "${installed_error}" STREQUAL "" OR
      NOT "${installed_trace_error}" STREQUAL "")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "stderr-not-empty" PARENT_SCOPE)
    set(${out_detail}
      "build=${build_error}; trace-build=${build_trace_error}; installed=${installed_error}; trace-installed=${installed_trace_error}"
      PARENT_SCOPE)
    return()
  endif()

  string(JSON status GET "${build_json}" status)
  string(JSON reason GET "${build_json}" reason)
  string(JSON fixture_type TYPE "${build_json}" fixture id)
  string(JSON semantic_state GET "${build_json}" semantic state)
  string(JSON case_id GET "${build_json}" semantic case_id)
  string(JSON cycles GET "${build_json}" time cycles)
  string(JSON instructions GET "${build_json}" time instructions)
  string(JSON budget GET "${build_json}" time budget_cycles)
  string(JSON termination GET "${build_json}" termination)
  string(JSON trace_count GET "${build_trace_json}" trace count)
  string(JSON trace_complete GET "${build_trace_json}" trace complete)
  if(NOT status STREQUAL "GUEST_STOP" OR
      NOT reason STREQUAL "unregistered-content" OR
      NOT fixture_type STREQUAL "NULL" OR
      NOT semantic_state STREQUAL "pass" OR
      NOT case_id STREQUAL "36865" OR
      NOT cycles MATCHES "^[1-9][0-9]*$" OR
      NOT instructions MATCHES "^[1-9][0-9]*$" OR
      NOT budget STREQUAL "1000" OR
      NOT termination STREQUAL "guest-stop" OR
      NOT trace_count MATCHES "^[1-9][0-9]*$" OR
      NOT trace_complete STREQUAL "true")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "raw-observation-mismatch" PARENT_SCOPE)
    set(${out_detail}
      "status=${status}; reason=${reason}; fixture=${fixture_type}; semantic=${semantic_state}/${case_id}; time=${cycles}/${instructions}/${budget}; termination=${termination}; trace=${trace_count}/${trace_complete}"
      PARENT_SCOPE)
    return()
  endif()
  foreach(path IN ITEMS "${SRC}" "${BIN}" "${original_prefix}" "${moved_prefix}")
    string(FIND "${build_json}" "${path}" path_index)
    if(NOT path_index EQUAL -1)
      set(${out_ok} FALSE PARENT_SCOPE)
      set(${out_reason} "absolute-path-leak" PARENT_SCOPE)
      set(${out_detail} "JSON contains ${path}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  if(NOT EXISTS "${build_trace}" OR NOT EXISTS "${installed_trace}")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "trace-output-missing" PARENT_SCOPE)
    set(${out_detail} "trace output path was not created" PARENT_SCOPE)
    return()
  endif()
  file(SIZE "${build_trace}" build_trace_size)
  file(SIZE "${installed_trace}" installed_trace_size)
  if(build_trace_size LESS 1 OR
      NOT build_trace_size EQUAL installed_trace_size)
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "trace-size-mismatch" PARENT_SCOPE)
    set(${out_detail}
      "build trace bytes=${build_trace_size}; installed trace bytes=${installed_trace_size}"
      PARENT_SCOPE)
    return()
  endif()
  file(READ "${build_trace}" build_trace_content)
  file(READ "${installed_trace}" installed_trace_content)
  if(NOT "${build_trace_content}" STREQUAL "${installed_trace_content}")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "trace-content-mismatch" PARENT_SCOPE)
    set(${out_detail} "trace records differ" PARENT_SCOPE)
    return()
  endif()

  set(${out_ok} TRUE PARENT_SCOPE)
  set(${out_reason} "" PARENT_SCOPE)
  set(${out_detail}
    "${status}/${reason}; case=${case_id}; cycles=${cycles}; instructions=${instructions}; trace-records=${trace_count}"
    PARENT_SCOPE)
endfunction()

if(MODE STREQUAL "clean")
  gbl_check_relocation("${moved_prefix}" "clean" okay reason detail)
  if(NOT okay)
    message(FATAL_ERROR "runner-relocation: ${reason}: ${detail}")
  endif()
  message(STATUS "runner-relocation: clean-pass: ${detail}")
else()
  set(broken_prefix "${sandbox}/broken-copy")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${moved_prefix}" "${broken_prefix}"
    RESULT_VARIABLE copy_result
    OUTPUT_VARIABLE copy_stdout
    ERROR_VARIABLE copy_stderr)
  if(NOT copy_result STREQUAL "0")
    message(FATAL_ERROR
      "runner-relocation: copy-failed: ${copy_stdout}${copy_stderr}")
  endif()
  file(REMOVE "${broken_prefix}/bin/gamebloke-run")
  gbl_check_relocation("${broken_prefix}" "broken" broken_ok broken_reason broken_detail)
  if(broken_ok OR NOT broken_reason STREQUAL "missing-executable")
    message(FATAL_ERROR
      "runner-relocation: missing-executable-control-failed: ${broken_reason}: ${broken_detail}")
  endif()
  gbl_check_relocation("${moved_prefix}" "control-clean" clean_ok clean_reason clean_detail)
  if(NOT clean_ok)
    message(FATAL_ERROR
      "runner-relocation: clean-install-failed: ${clean_reason}: ${clean_detail}")
  endif()
  message(STATUS
    "runner-relocation: missing-executable-control-pass; clean=${clean_detail}")
endif()
