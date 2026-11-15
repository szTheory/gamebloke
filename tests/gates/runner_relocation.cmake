cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN MODE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "runner-relocation: usage: -D${required}= is required")
  endif()
endforeach()
if(NOT MODE MATCHES "^(clean|admitted|missing-executable)$")
  message(FATAL_ERROR "runner-relocation: unknown-mode: ${MODE}")
endif()

get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)
if(MODE STREQUAL "admitted" AND (OPTIONAL OR GBL_REQUIRE_DIAGNOSTICS))
  file(READ "${BIN}/diagnostic-execution/run.json" setup)
  string(JSON tools GET "${setup}" tools)
  if(tools STREQUAL "absent")
    if(GBL_REQUIRE_DIAGNOSTICS)
      message(FATAL_ERROR "runner-relocation: required-admitted-toolchain-absent")
    endif()
    message("SKIPPED: arm-toolchain-absent")
    return()
  endif()
endif()
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

file(GLOB_RECURSE installed_files LIST_DIRECTORIES FALSE
  RELATIVE "${moved_prefix}" "${moved_prefix}/*")
if(NOT installed_files STREQUAL "bin/gamebloke-run")
  message(FATAL_ERROR
    "runner-relocation: unexpected-install-assets: ${installed_files}")
endif()

function(gbl_check_relocation prefix tag out_ok out_reason out_detail)
  set(installed_runner "${prefix}/bin/gamebloke-run")
  if(NOT EXISTS "${installed_runner}" OR IS_DIRECTORY "${installed_runner}")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "missing-executable" PARENT_SCOPE)
    set(${out_detail} "${installed_runner}" PARENT_SCOPE)
    return()
  endif()
  file(SHA256 "${build_runner}" build_runner_digest)
  file(SHA256 "${installed_runner}" installed_runner_digest)
  if(NOT build_runner_digest STREQUAL installed_runner_digest)
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "executable-identity-mismatch" PARENT_SCOPE)
    set(${out_detail}
      "build=${build_runner_digest}; installed=${installed_runner_digest}"
      PARENT_SCOPE)
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
  string(JSON observed_digest GET "${build_json}" fixture rom_sha256)
  string(JSON cycles GET "${build_json}" time cycles)
  string(JSON instructions GET "${build_json}" time instructions)
  string(JSON budget GET "${build_json}" time budget_cycles)
  string(JSON termination GET "${build_json}" termination)
  string(JSON trace_count GET "${build_trace_json}" trace count)
  string(JSON trace_complete GET "${build_trace_json}" trace complete)
  file(SHA256 "${fixture}" expected_digest)
  if(NOT status STREQUAL "GUEST_STOP" OR
      NOT reason STREQUAL "unregistered-content" OR
      NOT fixture_type STREQUAL "NULL" OR
      NOT semantic_state STREQUAL "pass" OR
      NOT case_id STREQUAL "36865" OR
      NOT observed_digest STREQUAL expected_digest OR
      NOT cycles MATCHES "^[1-9][0-9]*$" OR
      NOT instructions MATCHES "^[1-9][0-9]*$" OR
      NOT budget STREQUAL "1000" OR
      NOT termination STREQUAL "unregistered-content" OR
      NOT trace_count MATCHES "^[1-9][0-9]*$" OR
      NOT trace_complete STREQUAL "ON")
    set(${out_ok} FALSE PARENT_SCOPE)
    set(${out_reason} "raw-observation-mismatch" PARENT_SCOPE)
    set(${out_detail}
      "status=${status}; reason=${reason}; fixture=${fixture_type}; semantic=${semantic_state}/${case_id}; time=${cycles}/${instructions}/${budget}; termination=${termination}; trace=${trace_count}/${trace_complete}"
      PARENT_SCOPE)
    return()
  endif()
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
  foreach(path IN ITEMS "${SRC}" "${BIN}" "${original_prefix}" "${moved_prefix}")
    foreach(output IN ITEMS
        "${build_json}" "${build_trace_json}" "${build_trace_content}")
      string(FIND "${output}" "${path}" path_index)
      if(NOT path_index EQUAL -1)
        set(${out_ok} FALSE PARENT_SCOPE)
        set(${out_reason} "absolute-path-leak" PARENT_SCOPE)
        set(${out_detail} "result or trace contains ${path}" PARENT_SCOPE)
        return()
      endif()
    endforeach()
  endforeach()

  set(${out_ok} TRUE PARENT_SCOPE)
  set(${out_reason} "" PARENT_SCOPE)
  set(${out_detail}
    "${status}/${reason}; case=${case_id}; cycles=${cycles}; instructions=${instructions}; trace-records=${trace_count}"
    PARENT_SCOPE)
endfunction()

function(gbl_check_admitted prefix)
  set(installed_runner "${prefix}/bin/gamebloke-run")
  file(SHA256 "${build_runner}" build_digest)
  file(SHA256 "${installed_runner}" installed_digest)
  if(NOT build_digest STREQUAL installed_digest)
    message(FATAL_ERROR "runner-relocation: admitted-executable-identity-mismatch")
  endif()
  # These fixed witnesses exercise actual production admission, a semantic
  # assertion's broken companion, and an honest no-BIOS guest stop. Metadata is
  # read by this test only; neither executable receives a manifest/index path.
  file(READ "${SRC}/tests/fixtures/manifest.json" manifest)
  string(JSON rows LENGTH "${manifest}" fixtures)
  math(EXPR last "${rows}-1")
  foreach(witness IN ITEMS
      "arm-343721884|PASS|guest-mailbox-pass|pass|343721884|0|mailbox-complete"
      "arm-343721884-broken|GUEST_FAIL|guest-mailbox-fail|fail|343721884|1|mailbox-complete"
      "arm-stop-1765902634|GUEST_STOP|no-bios-swi|pending|1765902634|1|no-bios-swi")
    string(REPLACE "|" ";" fields "${witness}")
    list(GET fields 0 id)
    list(GET fields 1 expected_status)
    list(GET fields 2 expected_reason)
    list(GET fields 3 expected_semantic)
    list(GET fields 4 expected_case)
    list(GET fields 5 expected_exit)
    list(GET fields 6 expected_termination)
    set(found FALSE)
    foreach(i RANGE 0 ${last})
      string(JSON row_id GET "${manifest}" fixtures ${i} id)
      if(row_id STREQUAL id)
        string(JSON artifact GET "${manifest}" fixtures ${i} build artifact path)
        string(JSON expected_digest GET "${manifest}" fixtures ${i} build artifact sha256)
        set(found TRUE)
        break()
      endif()
    endforeach()
    if(NOT found OR NOT EXISTS "${BIN}/${artifact}")
      message(FATAL_ERROR "runner-relocation: admitted-witness-missing:${id}")
    endif()
    file(MAKE_DIRECTORY "${sandbox}/owned-input")
    set(input "${sandbox}/owned-input/${id}.gba")
    file(COPY_FILE "${BIN}/${artifact}" "${input}")
    file(SHA256 "${input}" actual_digest)
    if(NOT actual_digest STREQUAL expected_digest)
      message(FATAL_ERROR "runner-relocation: admitted-input-identity-mismatch:${id}")
    endif()
    foreach(traced IN ITEMS plain trace)
      foreach(kind IN ITEMS build installed)
        set(executable "${build_runner}")
        if(kind STREQUAL "installed")
          set(executable "${installed_runner}")
        endif()
        set(trace "${sandbox}/${id}-${kind}.jsonl")
        set(trace_args "")
        if(traced STREQUAL "trace")
          set(trace_args --trace-mask 7 --trace-capacity 65536 --trace-output "${trace}")
        endif()
        execute_process(COMMAND "${CMAKE_COMMAND}" -E env
          --unset=GBL_FIXTURE_INDEX --unset=GBL_CONTROL_INDEX
          --unset=GBL_MANIFEST --unset=GBL_SOURCE_DIR --unset=GBL_PRODUCTION_INDEX
          "${executable}" --fixture "${id}" --rom "${input}" --cycles 1000 ${trace_args}
          WORKING_DIRECTORY "${cwd}"
          RESULT_VARIABLE rc OUTPUT_VARIABLE json ERROR_VARIABLE error)
        if(NOT "${rc}" STREQUAL "${expected_exit}" OR NOT error STREQUAL "")
          message(FATAL_ERROR "runner-relocation: admitted-exit:${id}:${kind}:${traced}:${rc}:${error}")
        endif()
        string(JSON status GET "${json}" status)
        string(JSON reason GET "${json}" reason)
        string(JSON fixture_id GET "${json}" fixture id)
        string(JSON ran GET "${json}" fixture ran)
        string(JSON sha GET "${json}" fixture rom_sha256)
        string(JSON pinned_sha GET "${json}" expected_sha256)
        string(JSON state GET "${json}" semantic state)
        string(JSON case_id GET "${json}" semantic case_id)
        string(JSON termination GET "${json}" termination)
        string(JSON cycles GET "${json}" time cycles)
        string(JSON instructions GET "${json}" time instructions)
        string(JSON budget GET "${json}" time budget_cycles)
        if(NOT status STREQUAL expected_status OR NOT reason STREQUAL expected_reason OR
            NOT fixture_id STREQUAL id OR NOT ran OR NOT sha STREQUAL expected_digest OR
            NOT pinned_sha STREQUAL expected_digest OR NOT state STREQUAL expected_semantic OR
            NOT case_id STREQUAL expected_case OR NOT termination STREQUAL expected_termination OR
            NOT cycles MATCHES "^[1-9][0-9]*$" OR cycles GREATER 1000 OR
            NOT instructions MATCHES "^[1-9][0-9]*$" OR NOT budget STREQUAL "1000")
          message(FATAL_ERROR "runner-relocation: admitted-observation-mismatch:${id}:${kind}:${traced}:${json}")
        endif()
        if(traced STREQUAL "trace")
          string(JSON count GET "${json}" trace count)
          string(JSON complete GET "${json}" trace complete)
          if(NOT count MATCHES "^[1-9][0-9]*$" OR NOT complete OR NOT EXISTS "${trace}")
            message(FATAL_ERROR "runner-relocation: admitted-trace-missing:${id}:${kind}")
          endif()
          file(READ "${trace}" content)
          if(content STREQUAL "")
            message(FATAL_ERROR "runner-relocation: admitted-trace-empty:${id}:${kind}")
          endif()
          set(${kind}_trace "${content}")
        endif()
        foreach(path IN ITEMS "${SRC}" "${BIN}" "${sandbox}")
          string(FIND "${json};${content}" "${path}" leaked)
          if(NOT leaked EQUAL -1)
            message(FATAL_ERROR "runner-relocation: admitted-absolute-path-leak:${id}")
          endif()
        endforeach()
        set(${kind}_json "${json}")
      endforeach()
      if(NOT build_json STREQUAL installed_json OR
          (traced STREQUAL "trace" AND NOT build_trace STREQUAL installed_trace))
        message(FATAL_ERROR "runner-relocation: admitted-output-mismatch:${id}:${traced}")
      endif()
    endforeach()
    message(STATUS "runner-relocation: admitted-witness-pass:${id}:${expected_status}:${expected_reason}; exact JSON and trace")
  endforeach()
endfunction()

if(MODE STREQUAL "clean" OR MODE STREQUAL "admitted")
  gbl_check_relocation("${moved_prefix}" "clean" okay reason detail)
  if(NOT okay)
    message(FATAL_ERROR "runner-relocation: ${reason}: ${detail}")
  endif()
  message(STATUS "runner-relocation: clean-pass: ${detail}")
  if(MODE STREQUAL "admitted")
    gbl_check_admitted("${moved_prefix}")
  endif()
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
