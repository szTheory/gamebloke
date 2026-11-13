get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)
set(dir "${BIN}/artifact-case/admission-controls")
if(DEFINED COPY_CASE)
  include("${SRC}/cmake/PathContainment.cmake")
  gbl_copy_file_contained(copy_status copy_reason
    "${COPY_SOURCE}" "${COPY_ROOT}" "${COPY_DESTINATION}")
  if(NOT copy_status STREQUAL "ok")
    message(FATAL_ERROR "artifact-case: ${copy_reason}")
  endif()
  message(STATUS "artifact-case: contained-copy-passed:${COPY_CASE}")
  return()
endif()

if(CASE STREQUAL "path-controls")
  include("${SRC}/cmake/PathContainment.cmake")
  set(path_dir "${BIN}/artifact-case/admission-path-controls")
  file(MAKE_DIRECTORY "${path_dir}")
  set(copy_source "${path_dir}/synthetic-copy-source.bin")
  file(WRITE "${copy_source}" "synthetic diagnostic admission control bytes\n")

  function(run_copy_control label root destination expected)
    execute_process(COMMAND "${CMAKE_COMMAND}"
      "-DSRC=${SRC}" "-DBIN=${BIN}" "-DCOPY_CASE=${label}"
      "-DCOPY_SOURCE=${copy_source}" "-DCOPY_ROOT=${root}"
      "-DCOPY_DESTINATION=${destination}"
      -P "${SRC}/tests/gates/diagnostic_admission_controls.cmake"
      RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(report "${output}${error}")
    if(expected STREQUAL "ok")
      if(NOT rc STREQUAL "0" OR NOT EXISTS "${destination}")
        message(FATAL_ERROR "artifact-case: contained-copy-control-failed:${label}:${report}")
      endif()
      file(SHA256 "${copy_source}" source_digest)
      file(SHA256 "${destination}" copy_digest)
      if(NOT source_digest STREQUAL copy_digest)
        message(FATAL_ERROR "artifact-case: contained-copy-bytes-mismatch:${label}")
      endif()
    elseif(rc STREQUAL "0" OR NOT report MATCHES "${expected}")
      message(FATAL_ERROR "artifact-case: escaping-copy-was-not-refused:${label}:${report}")
    endif()
  endfunction()

  set(clean_copy_root "${path_dir}/copy-contained/root")
  run_copy_control(contained "${clean_copy_root}"
    "${clean_copy_root}/generated/diagnostics/clean.bin" ok)

  set(nested_copy_root "${path_dir}/copy-nested/root")
  set(nested_outside "${path_dir}/copy-nested-external")
  set(nested_sentinel "${nested_outside}/sentinel.bin")
  file(MAKE_DIRECTORY "${nested_copy_root}/generated/diagnostics" "${nested_outside}")
  file(WRITE "${nested_sentinel}" "synthetic nested-symlink sentinel\n")
  file(SHA256 "${nested_sentinel}" nested_before)
  file(CREATE_LINK "${nested_outside}"
    "${nested_copy_root}/generated/diagnostics/escape" SYMBOLIC RESULT nested_link)
  if(NOT nested_link STREQUAL "0")
    message(FATAL_ERROR "artifact-case: nested-symlink-control-could-not-be-created:${nested_link}")
  endif()
  run_copy_control(nested-directory-escape "${nested_copy_root}"
    "${nested_copy_root}/generated/diagnostics/escape/sentinel.bin"
    "copy-destination-path-escape")
  file(SHA256 "${nested_sentinel}" nested_after)
  if(NOT nested_before STREQUAL nested_after)
    message(FATAL_ERROR "artifact-case: nested-symlink-overwrote-external-sentinel")
  endif()

  set(final_copy_root "${path_dir}/copy-final/root")
  set(final_outside "${path_dir}/copy-final-external/sentinel.bin")
  file(MAKE_DIRECTORY "${final_copy_root}/generated/diagnostics" "${path_dir}/copy-final-external")
  file(WRITE "${final_outside}" "synthetic final-symlink sentinel\n")
  file(SHA256 "${final_outside}" final_before)
  file(CREATE_LINK "${final_outside}"
    "${final_copy_root}/generated/diagnostics/escape.bin" SYMBOLIC RESULT final_link)
  if(NOT final_link STREQUAL "0")
    message(FATAL_ERROR "artifact-case: final-symlink-control-could-not-be-created:${final_link}")
  endif()
  run_copy_control(final-file-escape "${final_copy_root}"
    "${final_copy_root}/generated/diagnostics/escape.bin"
    "copy-destination-path-escape")
  file(SHA256 "${final_outside}" final_after)
  if(NOT final_before STREQUAL final_after)
    message(FATAL_ERROR "artifact-case: final-symlink-overwrote-external-sentinel")
  endif()

  set(dangling_copy_root "${path_dir}/copy-dangling/root")
  set(dangling_target "${path_dir}/copy-dangling-external/not-created.bin")
  file(MAKE_DIRECTORY "${dangling_copy_root}/generated/diagnostics"
    "${path_dir}/copy-dangling-external")
  file(CREATE_LINK "${dangling_target}"
    "${dangling_copy_root}/generated/diagnostics/dangling.bin"
    SYMBOLIC RESULT dangling_link)
  if(NOT dangling_link STREQUAL "0")
    message(FATAL_ERROR "artifact-case: dangling-symlink-control-could-not-be-created:${dangling_link}")
  endif()
  run_copy_control(final-dangling-symlink "${dangling_copy_root}"
    "${dangling_copy_root}/generated/diagnostics/dangling.bin"
    "copy-destination-dangling-symlink")
  if(EXISTS "${dangling_target}")
    message(FATAL_ERROR "artifact-case: dangling-symlink-created-external-target")
  endif()

  message(STATUS "artifact-case: admission path-boundary controls passed")
  return()
endif()

file(MAKE_DIRECTORY "${dir}")

include("${SRC}/cmake/CheckResult.cmake")
gbl_contract_read(V "${SRC}/contracts/evidence.json")
gbl_registry_read(R "${SRC}/registry/disputes.json" V)
gbl_manifest_read(M "${SRC}/tests/fixtures/manifest.json" R V)
gbl_findings_finish(result "admission-control authorities valid")
set(row "${M_ROW_smoke_JSON}")
set(pin "${M_ROW_smoke_SHA256}")
set(path "${M_ROW_smoke_ARTIFACT_PATH}")
set(image "${BIN}/${path}")
if(NOT EXISTS "${image}")
  message(FATAL_ERROR "artifact-case: run-all-admitted-first")
endif()
set(manifest "${dir}/manifest.json")
file(WRITE "${manifest}" "{\"schema\":\"gamebloke.manifest/1\",\"fixtures\":[${row}]}\n")
set(artifact_root "${dir}/root")
get_filename_component(image_dir "${artifact_root}/${path}" DIRECTORY)
file(MAKE_DIRECTORY "${image_dir}")
file(COPY_FILE "${image}" "${artifact_root}/${path}")
set(source "${SRC}")

function(generate label expected)
  set(prod "${dir}/${label}/production.h")
  set(control "${dir}/${label}/control.h")
  file(MAKE_DIRECTORY "${dir}/${label}")
  # A refusal must remove old outputs as well as report the concrete cause.
  file(WRITE "${prod}" "stale production index\n")
  file(WRITE "${control}" "stale control index\n")
  execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DGBL_SOURCE_DIR=${source}" "-DGBL_ARTIFACT_ROOT=${artifact_root}"
    "-DGBL_CONTROL_IMAGE=${BIN}/generated/fixtures/owned-arm-pass.gba"
    "-DGBL_CONTROL_MANIFEST_OUTPUT=${dir}/${label}/control.json"
    "-DGBL_MANIFEST=${manifest}" "-DGBL_REGISTRY=${SRC}/registry/disputes.json"
    "-DGBL_VOCAB=${SRC}/contracts/evidence.json"
    "-DGBL_OUT_PRODUCTION=${prod}" "-DGBL_OUT_CONTROL=${control}"
    -P "${SRC}/cmake/GenerateFixtureIndex.cmake"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
  file(WRITE "${dir}/${label}/generator.log" "${output}${error}")
  if(expected STREQUAL "ok")
    if(NOT rc STREQUAL "0" OR NOT EXISTS "${prod}" OR NOT EXISTS "${control}")
      message(FATAL_ERROR "artifact-case: clean-admission-companion-failed:${label}:${output}${error}")
    endif()
  elseif(rc STREQUAL "0" OR NOT "${output}${error}" MATCHES "${expected}" OR
         EXISTS "${prod}" OR EXISTS "${control}")
    message(FATAL_ERROR "artifact-case: admission-control-wrong-cause:${label}:${output}${error}")
  endif()
endfunction()

function(run_smoke label expected_status expected_reason expected_rc)
  execute_process(COMMAND "${BIN}/gamebloke-run" --rom "${image}"
    --fixture smoke --cycles 1000 RESULT_VARIABLE rc OUTPUT_VARIABLE doc ERROR_VARIABLE error)
  file(WRITE "${dir}/${label}.json" "${doc}")
  string(JSON status GET "${doc}" status)
  string(JSON reason GET "${doc}" reason)
  string(JSON ran GET "${doc}" fixture ran)
  string(JSON cycles GET "${doc}" time cycles)
  string(JSON instructions GET "${doc}" time instructions)
  gbl_result_check("${dir}/${label}.json" M R V)
  gbl_findings_finish(result "admission runtime control valid:${label}")
  if(NOT rc STREQUAL expected_rc OR NOT status STREQUAL expected_status OR
     NOT reason STREQUAL expected_reason)
    message(FATAL_ERROR "artifact-case: runtime-control-wrong-cause:${label}:${doc}${error}")
  endif()
  if(expected_status STREQUAL "REFUSED" AND (ran OR NOT cycles EQUAL 0 OR NOT instructions EQUAL 0))
    message(FATAL_ERROR "artifact-case: refusal-executed-guest:${label}")
  elseif(expected_status STREQUAL "PASS" AND (NOT ran OR cycles LESS 1 OR instructions LESS 1))
    message(FATAL_ERROR "artifact-case: clean-companion-did-not-execute:${label}")
  endif()
endfunction()

function(run_copy_control label root destination expected)
  execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${SRC}" "-DBIN=${BIN}" "-DCOPY_CASE=${label}"
    "-DCOPY_SOURCE=${copy_source}" "-DCOPY_ROOT=${root}"
    "-DCOPY_DESTINATION=${destination}"
    -P "${SRC}/tests/gates/diagnostic_admission_controls.cmake"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
  set(report "${output}${error}")
  if(expected STREQUAL "ok")
    if(NOT rc STREQUAL "0" OR NOT EXISTS "${destination}")
      message(FATAL_ERROR "artifact-case: contained-copy-control-failed:${label}:${report}")
    endif()
    file(SHA256 "${copy_source}" source_digest)
    file(SHA256 "${destination}" copy_digest)
    if(NOT source_digest STREQUAL copy_digest)
      message(FATAL_ERROR "artifact-case: contained-copy-bytes-mismatch:${label}")
    endif()
  elseif(rc STREQUAL "0" OR NOT report MATCHES "${expected}")
    message(FATAL_ERROR "artifact-case: escaping-copy-was-not-refused:${label}:${report}")
  endif()
endfunction()

# Exercise the exact copy helper used by diagnostic_admission.cmake with
# synthetic bytes. Every external sentinel lives beside, not inside, its root.
set(copy_source "${dir}/synthetic-copy-source.bin")
file(WRITE "${copy_source}" "synthetic diagnostic admission control bytes\n")

set(clean_copy_root "${dir}/copy-contained/root")
set(clean_copy_dest "${clean_copy_root}/generated/diagnostics/clean.bin")
run_copy_control(contained "${clean_copy_root}" "${clean_copy_dest}" ok)

set(nested_copy_root "${dir}/copy-nested/root")
set(nested_outside "${dir}/copy-nested-external")
set(nested_sentinel "${nested_outside}/sentinel.bin")
file(MAKE_DIRECTORY "${nested_copy_root}/generated/diagnostics" "${nested_outside}")
file(WRITE "${nested_sentinel}" "synthetic nested-symlink sentinel\n")
file(SHA256 "${nested_sentinel}" nested_before)
file(CREATE_LINK "${nested_outside}"
  "${nested_copy_root}/generated/diagnostics/escape" SYMBOLIC RESULT nested_link)
if(NOT nested_link STREQUAL "0")
  message(FATAL_ERROR "artifact-case: nested-symlink-control-could-not-be-created:${nested_link}")
endif()
run_copy_control(nested-directory-escape "${nested_copy_root}"
  "${nested_copy_root}/generated/diagnostics/escape/sentinel.bin"
  "copy-destination-path-escape")
file(SHA256 "${nested_sentinel}" nested_after)
if(NOT nested_before STREQUAL nested_after)
  message(FATAL_ERROR "artifact-case: nested-symlink-overwrote-external-sentinel")
endif()

set(final_copy_root "${dir}/copy-final/root")
set(final_outside "${dir}/copy-final-external/sentinel.bin")
file(MAKE_DIRECTORY "${final_copy_root}/generated/diagnostics" "${dir}/copy-final-external")
file(WRITE "${final_outside}" "synthetic final-symlink sentinel\n")
file(SHA256 "${final_outside}" final_before)
file(CREATE_LINK "${final_outside}"
  "${final_copy_root}/generated/diagnostics/escape.bin" SYMBOLIC RESULT final_link)
if(NOT final_link STREQUAL "0")
  message(FATAL_ERROR "artifact-case: final-symlink-control-could-not-be-created:${final_link}")
endif()
run_copy_control(final-file-escape "${final_copy_root}"
  "${final_copy_root}/generated/diagnostics/escape.bin"
  "copy-destination-path-escape")
file(SHA256 "${final_outside}" final_after)
if(NOT final_before STREQUAL final_after)
  message(FATAL_ERROR "artifact-case: final-symlink-overwrote-external-sentinel")
endif()

set(dangling_copy_root "${dir}/copy-dangling/root")
set(dangling_target "${dir}/copy-dangling-external/not-created.bin")
file(MAKE_DIRECTORY "${dangling_copy_root}/generated/diagnostics"
  "${dir}/copy-dangling-external")
file(CREATE_LINK "${dangling_target}"
  "${dangling_copy_root}/generated/diagnostics/dangling.bin"
  SYMBOLIC RESULT dangling_link)
if(NOT dangling_link STREQUAL "0")
  message(FATAL_ERROR "artifact-case: dangling-symlink-control-could-not-be-created:${dangling_link}")
endif()
run_copy_control(final-dangling-symlink "${dangling_copy_root}"
  "${dangling_copy_root}/generated/diagnostics/dangling.bin"
  "copy-destination-dangling-symlink")
if(EXISTS "${dangling_target}")
  message(FATAL_ERROR "artifact-case: dangling-symlink-created-external-target")
endif()

generate(clean ok)
run_smoke(clean PASS guest-mailbox-pass 0)
file(APPEND "${artifact_root}/${path}" "x")
generate(output "fixture-index: artifact-digest-mismatch: smoke")
file(COPY_FILE "${image}" "${artifact_root}/${path}")
generate(output-clean ok)
string(REPEAT "0" 64 wrong_pin)
string(JSON changed SET "${row}" build artifact sha256 "\"${wrong_pin}\"")
file(WRITE "${manifest}" "{\"schema\":\"gamebloke.manifest/1\",\"fixtures\":[${changed}]}\n")
generate(pin "fixture-index: artifact-digest-mismatch: smoke")
file(WRITE "${manifest}" "{\"schema\":\"gamebloke.manifest/1\",\"fixtures\":[${row}]}\n")
generate(pin-clean ok)

# Only tracked, public source enters this isolated clone; private/ignored bytes
# are never copied. A producer newline must invalidate the source authority.
set(source "${dir}/source")
if(NOT EXISTS "${source}/.git")
  execute_process(COMMAND git clone --quiet --no-local "${SRC}" "${source}"
    RESULT_VARIABLE clone_rc ERROR_VARIABLE error)
  if(NOT clone_rc STREQUAL "0")
    message(FATAL_ERROR "artifact-case: source-control-clone-failed:${error}")
  endif()
endif()
generate(source-clean ok)
set(input "${source}/diagnostics/armv4t/smoke.S")
file(READ "${input}" clean_source)
file(APPEND "${input}" "\n")
generate(source "fixture-index: stale-diagnostic-source-input")
file(WRITE "${input}" "${clean_source}")
generate(source-restored ok)

# Compile the production target against a deliberately wrong immutable index.
# Restore/rebuild before evaluating observations, including on failed builds.
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BIN}" --target gamebloke-run
  RESULT_VARIABLE baseline_build_rc OUTPUT_VARIABLE baseline_output ERROR_VARIABLE baseline_error)
if(NOT baseline_build_rc STREQUAL "0")
  message(FATAL_ERROR "artifact-case: index-control-baseline-build-failed:${baseline_output}${baseline_error}")
endif()
set(index "${BIN}/generated/production/fixture_index.h")
file(READ "${index}" clean_index)
string(REPLACE "\"${pin}\"" "\"${wrong_pin}\"" changed_index "${clean_index}")
if(changed_index STREQUAL clean_index)
  message(FATAL_ERROR "artifact-case: generated-index-control-did-not-mutate")
endif()
file(WRITE "${index}" "${changed_index}")
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BIN}" --target gamebloke-run
  RESULT_VARIABLE bad_build_rc OUTPUT_VARIABLE bad_output ERROR_VARIABLE bad_error)
if(bad_build_rc STREQUAL "0")
  execute_process(COMMAND "${BIN}/gamebloke-run" --rom "${image}" --fixture smoke --cycles 1000
    RESULT_VARIABLE bad_rc OUTPUT_VARIABLE bad_doc ERROR_VARIABLE bad_run_error)
endif()
file(WRITE "${index}" "${clean_index}")
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BIN}" --target gamebloke-run
  RESULT_VARIABLE restore_rc OUTPUT_VARIABLE restore_output ERROR_VARIABLE restore_error)
if(NOT bad_build_rc STREQUAL "0" OR NOT restore_rc STREQUAL "0")
  message(FATAL_ERROR "artifact-case: index-control-build-failed:${bad_output}${bad_error}${restore_output}${restore_error}")
endif()
file(WRITE "${dir}/index.json" "${bad_doc}")
string(JSON status GET "${bad_doc}" status)
string(JSON reason GET "${bad_doc}" reason)
string(JSON ran GET "${bad_doc}" fixture ran)
string(JSON cycles GET "${bad_doc}" time cycles)
string(JSON instructions GET "${bad_doc}" time instructions)
if(NOT bad_rc STREQUAL "1" OR NOT status STREQUAL "REFUSED" OR
   NOT reason STREQUAL "fixture-digest-mismatch" OR ran OR NOT cycles EQUAL 0 OR NOT instructions EQUAL 0)
  message(FATAL_ERROR "artifact-case: stale-generated-index-executed:${bad_doc}${bad_run_error}")
endif()
run_smoke(index-clean PASS guest-mailbox-pass 0)
file(WRITE "${dir}/summary.txt"
  "source-input=refused-before-index-write\noutput-byte=refused-before-index-write\npin=refused-before-index-write\ngenerated-index=runtime-REFUSED-fixture-digest-mismatch-ran-false-zero-progress\nclean-companions=PASS\n")
message(STATUS "artifact-case: admission-controls passed; independent source/output/pin/index refusals and clean production companions")
