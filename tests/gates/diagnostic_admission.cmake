cmake_minimum_required(VERSION 3.25)
# Production admission is derived from the source ledger, never from synthetic
# runner controls. Keep each fixture ID distinct while retaining paired case IDs.
get_filename_component(source_root "${SRC}" ABSOLUTE)
get_filename_component(binary_root "${BIN}" ABSOLUTE)
include("${source_root}/cmake/PathContainment.cmake")
gbl_resolve_path(binary_root_real binary_root_status "${binary_root}")
if(NOT binary_root_status STREQUAL "ok")
  message(FATAL_ERROR "diagnostics-case: binary-root-${binary_root_status}")
endif()
set(binary_root "${binary_root_real}")
# Check every output before reading, copying or writing. The calling gate
# removes safe stale indexes after any child refusal.
foreach(relative IN ITEMS
    generated/production/fixture_index.h
    generated/control/fixture_index.h
    generated/control/manifest.json)
  gbl_resolve_contained_path(output_real remove_status "${binary_root}"
    "${binary_root}/${relative}")
  if(NOT remove_status STREQUAL "ok")
    message(FATAL_ERROR "diagnostics-case: generated-output-${remove_status}:${relative}")
  endif()
endforeach()
include("${source_root}/cmake/CheckResult.cmake")
gbl_contract_read(V "${source_root}/contracts/evidence.json")
gbl_registry_read(R "${source_root}/registry/disputes.json" V)
gbl_manifest_read(M "${source_root}/tests/fixtures/manifest.json" R V)
gbl_findings_finish(result "admission authorities valid")
include("${source_root}/tests/gates/prepare_diagnostic_artifacts.cmake")

if(PREPARE_ONLY)
  message(STATUS "diagnostics-case: exact source outputs prepared for ${GROUP}")
  return()
endif()

# Project the already canonical-validated current authority, not a cached
# validation result. Exact bytes include qualification, every ID/digest and all
# metadata fields. Preserve a matching header's timestamp and normal build
# dependency checks; write changed/missing metadata before the runner build.
include("${source_root}/cmake/FixtureIndexRender.cmake")
set(GBL_PROOF_METADATA OFF)
gbl_fixture_render_index(expected_index M)
set(index "${binary_root}/generated/production/fixture_index.h")
set(actual_index "")
if(EXISTS "${index}" AND NOT IS_DIRECTORY "${index}")
  file(READ "${index}" actual_index)
endif()
if(NOT actual_index STREQUAL expected_index)
  gbl_write_file_contained(write_status write_reason "${binary_root}"
    "${index}" "${expected_index}")
  if(NOT write_status STREQUAL "ok")
    message(FATAL_ERROR "diagnostics-case: admitted-index-${write_reason}")
  endif()
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${binary_root}" --target gamebloke-run
  RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT rc STREQUAL "0")
  message(FATAL_ERROR "diagnostics-case: admitted-runner-build-failed:${output}${error}")
endif()

set(positive 0)
set(broken 0)
set(stops 0)
foreach(fid IN LISTS selected_ids)
  if(fid MATCHES "^(arm|thumb)-(stop-)?([0-9]+)(-broken)?$")
    set(case_id "${CMAKE_MATCH_3}")
  else()
    set(case_id 20481)
  endif()
  set(status PASS)
  set(state pass)
  set(reason guest-mailbox-pass)
  set(termination mailbox-complete)
  set(exit_code 0)
  if(fid MATCHES "-stop-")
    set(status GUEST_STOP)
    set(state pending)
    set(reason "${reason_${case_id}}")
    set(termination "${reason}")
    set(exit_code 1)
    math(EXPR stops "${stops}+1")
  elseif(fid MATCHES "-broken$")
    set(status GUEST_FAIL)
    set(state fail)
    set(reason guest-mailbox-fail)
    set(exit_code 1)
    math(EXPR broken "${broken}+1")
  else()
    math(EXPR positive "${positive}+1")
  endif()
  execute_process(COMMAND "${binary_root}/gamebloke-run" --rom
    "${binary_root}/${M_ROW_${fid}_ARTIFACT_PATH}" --cycles 1000 --fixture "${fid}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE doc ERROR_VARIABLE error)
  set(result "${result_root}/${fid}.json")
  gbl_write_file_contained(write_status write_reason "${binary_root}"
    "${result}" "${doc}")
  if(NOT write_status STREQUAL "ok")
    message(FATAL_ERROR "diagnostics-case: result-file-${write_reason}:${fid}")
  endif()
  if(NOT rc STREQUAL "${exit_code}")
    message(FATAL_ERROR "diagnostics-case: admitted-exit-mismatch:${fid}:${doc}${error}")
  endif()
  # The canonical function checks each real result against the full authority
  # parsed once above; separate checks below bind exact semantics and progress.
  gbl_result_check("${result}" M R V)
  gbl_findings_finish(result "admitted result valid:${fid}")
  foreach(field IN ITEMS status reason termination)
    string(JSON actual GET "${doc}" ${field})
    if(NOT actual STREQUAL "${${field}}")
      message(FATAL_ERROR "diagnostics-case: admitted-${field}-mismatch:${fid}:${actual}")
    endif()
  endforeach()
  string(JSON actual_state GET "${doc}" semantic state)
  string(JSON actual_id GET "${doc}" semantic case_id)
  string(JSON fixture_id GET "${doc}" fixture id)
  string(JSON ran GET "${doc}" fixture ran)
  string(JSON actual_sha GET "${doc}" fixture rom_sha256)
  string(JSON indexed_sha GET "${doc}" expected_sha256)
  string(JSON claim_sha GET "${doc}" claim cart rom_sha256)
  string(JSON cycles GET "${doc}" time cycles)
  string(JSON instructions GET "${doc}" time instructions)
  string(JSON budget GET "${doc}" time budget_cycles)
  if(NOT actual_state STREQUAL state OR NOT actual_id STREQUAL case_id OR
     actual_id EQUAL 0 OR NOT fixture_id STREQUAL fid OR NOT ran OR
     NOT actual_sha STREQUAL "${M_ROW_${fid}_SHA256}" OR
     NOT indexed_sha STREQUAL actual_sha OR NOT claim_sha STREQUAL actual_sha OR
     NOT budget STREQUAL "1000" OR cycles LESS 1 OR instructions LESS 1 OR cycles GREATER budget)
    message(FATAL_ERROR "diagnostics-case: admitted-semantics-identity-progress-mismatch:${fid}:${doc}")
  endif()
endforeach()
list(LENGTH selected_ids outputs)
gbl_write_file_contained(write_status write_reason "${binary_root}"
  "${result_root}/summary.txt"
  "source.commit=${authority}\noutputs=${outputs}\npositive=${positive}\nbroken=${broken}\nguest-stops=${stops}\ncanonical.result=checked-every-output\nsemantic.case-id=exact-nonzero\nprogress=nonzero-bounded\nloaded.index.claim.digest=exact\n")
if(NOT write_status STREQUAL "ok")
  message(FATAL_ERROR "diagnostics-case: summary-file-${write_reason}")
endif()
message(STATUS "diagnostics-case: ${CASE} passed: ${outputs} production results; positive=${positive}; broken=${broken}; stops=${stops}")
