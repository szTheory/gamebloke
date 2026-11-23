cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "audio-guest: missing required argument ${required}")
  endif()
endforeach()
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)

set(runner "${BIN}/gamebloke-run")
set(audio_image "${BIN}/generated/fixtures/owned-audio-mix.gba")
set(audio_generator "${BIN}/gbl_audio_guest_image")
set(audio_source "${SRC}/tests/fixtures/audio_image.c")
set(audio_manifest_file "${SRC}/tests/fixtures/audio-manifest.json")
set(merged_manifest_file "${BIN}/generated/production/audio-manifest.json")
set(toolchain_receipt "${BIN}/generated/local-audio-toolchain.txt")
set(scratch "${BIN}/nc/audio_guest")
set(expected_pcm_hex "db660000db66007fdb66007fdb660080")
file(MAKE_DIRECTORY "${scratch}")

foreach(path IN ITEMS "${runner}" "${audio_image}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "audio-guest: required local input is missing")
  endif()
endforeach()
if(NOT "${audio_image}" STREQUAL
    "${BIN}/generated/fixtures/owned-audio-mix.gba")
  message(FATAL_ERROR "audio-guest: generated image escaped its build path")
endif()

function(expect_json_field json expected)
  string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "audio-guest: expected ${ARGN}=${expected}; got ${actual}; ${json_error}")
  endif()
endfunction()

function(expect_json_type json expected)
  string(JSON actual ERROR_VARIABLE json_error TYPE "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR
      "audio-guest: expected ${ARGN} type ${expected}; got ${actual}; ${json_error}")
  endif()
endfunction()

function(expect_json_boolean json expected)
  string(JSON actual ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT "${json_error}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR
      "audio-guest: could not read boolean ${ARGN}; ${json_error}")
  endif()
  if("${expected}" STREQUAL "true" AND NOT actual)
    message(FATAL_ERROR "audio-guest: expected ${ARGN}=true")
  elseif("${expected}" STREQUAL "false" AND actual)
    message(FATAL_ERROR "audio-guest: expected ${ARGN}=false")
  endif()
endfunction()

function(validate_audio_authority doc output_valid output_reason)
  set(valid TRUE)
  set(reason "")
  string(JSON schema ERROR_VARIABLE schema_error GET "${doc}" schema)
  string(JSON row_count ERROR_VARIABLE rows_error LENGTH "${doc}" fixtures)
  if(NOT "${schema_error}" STREQUAL "NOTFOUND" OR
      NOT schema STREQUAL "gamebloke.manifest/1" OR
      NOT "${rows_error}" STREQUAL "NOTFOUND" OR NOT row_count EQUAL 1)
    set(valid FALSE)
    set(reason "manifest-schema-or-row-count")
  endif()
  if(valid)
    string(JSON row GET "${doc}" fixtures 0)
    string(JSON id GET "${row}" id)
    string(JSON source_kind GET "${row}" source kind)
    string(JSON source_url GET "${row}" source url)
    string(JSON source_commit GET "${row}" source commit)
    string(LENGTH "${source_commit}" source_commit_length)
    string(JSON spdx GET "${row}" license spdx)
    string(JSON redistributable GET "${row}" license redistributable)
    string(JSON notice_count LENGTH "${row}" license notices)
    string(JSON notice GET "${row}" license notices 0)
    string(JSON toolchain_identity GET "${row}" build toolchain identity)
    string(JSON toolchain_sha_type TYPE "${row}" build toolchain sha256)
    string(JSON artifact_path GET "${row}" build artifact path)
    string(JSON artifact_sha GET "${row}" build artifact sha256)
    string(LENGTH "${artifact_sha}" artifact_sha_length)
    string(JSON bios GET "${row}" bios_requirement)
    string(JSON provenance GET "${row}" expected provenance_class)
    string(JSON dispute_type TYPE "${row}" expected dispute)
    string(JSON use_count LENGTH "${row}" permitted_use)
    string(JSON permitted_use GET "${row}" permitted_use 0)
    if(NOT id STREQUAL "owned-audio-mix" OR
        NOT source_kind STREQUAL "git" OR
        NOT source_url STREQUAL "https://github.com/szTheory/gamebloke" OR
        NOT source_commit_length EQUAL 40 OR
        NOT source_commit MATCHES "^[0-9a-f]+$" OR
        NOT spdx STREQUAL "LicenseRef-Gamebloke-Audio-Diagnostic-Local-Use" OR
        NOT redistributable STREQUAL "unknown" OR
        NOT notice_count EQUAL 1 OR
        NOT notice STREQUAL "tests/fixtures/audio-NOTICE.md" OR
        NOT toolchain_identity MATCHES "^.+; CMake .+; C17 host generator$" OR
        NOT toolchain_sha_type STREQUAL "NULL" OR
        NOT artifact_path STREQUAL "generated/fixtures/owned-audio-mix.gba" OR
        NOT artifact_sha_length EQUAL 64 OR
        NOT artifact_sha MATCHES "^[0-9a-f]+$" OR
        NOT bios STREQUAL "none" OR
        NOT provenance STREQUAL "project-authored" OR
        NOT dispute_type STREQUAL "NULL" OR
        NOT use_count EQUAL 1 OR NOT permitted_use STREQUAL "local-only")
      set(valid FALSE)
      set(reason "manifest-authority-fields")
    endif()
  endif()
  if(valid)
    file(SHA256 "${audio_source}" current_source_sha)
    string(JSON recipe GET "${doc}" fixtures 0 build recipe)
    string(FIND "${recipe}" "source_sha256=${current_source_sha}" recipe_digest_at)
    string(FIND "${recipe}" "gbl_audio_guest_image" recipe_generator_at)
    string(FIND "${recipe}" "local-audio" recipe_lane_at)
    if(recipe_digest_at EQUAL -1 OR recipe_generator_at EQUAL -1 OR
        recipe_lane_at EQUAL -1)
      set(valid FALSE)
      set(reason "source-digest-or-recipe-drift")
    endif()
  endif()
  if(valid)
    execute_process(COMMAND git show
        "${source_commit}:tests/fixtures/audio_image.c"
      WORKING_DIRECTORY "${SRC}" RESULT_VARIABLE git_source_result
      OUTPUT_VARIABLE committed_source ERROR_VARIABLE git_source_error
      TIMEOUT 15)
    if(NOT "${git_source_result}" STREQUAL "0")
      set(valid FALSE)
      set(reason "source-revision-unavailable")
    else()
      string(SHA256 committed_source_sha "${committed_source}")
      file(SHA256 "${audio_source}" working_source_sha)
      execute_process(COMMAND git merge-base --is-ancestor "${source_commit}" HEAD
        WORKING_DIRECTORY "${SRC}" RESULT_VARIABLE ancestor_result
        OUTPUT_QUIET ERROR_QUIET TIMEOUT 15)
      if(NOT committed_source_sha STREQUAL working_source_sha OR
          NOT committed_source_sha STREQUAL current_source_sha OR
          NOT "${ancestor_result}" STREQUAL "0")
        set(valid FALSE)
        set(reason "source-revision-or-bytes-drift")
      endif()
    endif()
  endif()
  if(valid)
    file(READ "${toolchain_receipt}" receipt_identity)
    string(STRIP "${receipt_identity}" receipt_identity)
    if(NOT receipt_identity STREQUAL toolchain_identity)
      set(valid FALSE)
      set(reason "toolchain-identity-drift")
    endif()
  endif()
  if(valid)
    set(notice_path "${SRC}/${notice}")
    if(NOT EXISTS "${notice_path}" OR IS_DIRECTORY "${notice_path}")
      set(valid FALSE)
      set(reason "notice-missing")
    else()
      file(READ "${notice_path}" notice_text)
      string(FIND "${notice_text}" "local-only" notice_local_at)
      string(FIND "${notice_text}" "redistribution status remain **unknown**" notice_unknown_at)
      string(FIND "${notice_text}" "D-19-08" notice_decision_at)
      if(notice_local_at EQUAL -1 OR notice_unknown_at EQUAL -1 OR
          notice_decision_at EQUAL -1)
        set(valid FALSE)
        set(reason "notice-scope-or-rights-drift")
      endif()
    endif()
  endif()
  if(valid)
    set(artifact "${BIN}/${artifact_path}")
    if(NOT EXISTS "${artifact}" OR IS_DIRECTORY "${artifact}")
      set(valid FALSE)
      set(reason "artifact-missing")
    else()
      file(SHA256 "${artifact}" actual_artifact_sha)
      if(NOT actual_artifact_sha STREQUAL artifact_sha)
        set(valid FALSE)
        set(reason "artifact-digest-drift")
      endif()
    endif()
  endif()
  set(${output_valid} ${valid} PARENT_SCOPE)
  set(${output_reason} "${reason}" PARENT_SCOPE)
endfunction()

function(expect_audio_authority_rejection label doc)
  validate_audio_authority("${doc}" valid reason)
  if(valid)
    message(FATAL_ERROR
      "audio-guest: ${label} authority mutation was accepted")
  endif()
  message(STATUS "audio-guest: ${label} mutation rejected (${reason})")
endfunction()

function(read_raw_run output_variable name audio_path trace_path)
  file(REMOVE "${audio_path}" "${trace_path}")
  execute_process(
    COMMAND "${runner}" --rom "${audio_image}" --cycles 20000
      --audio-output "${audio_path}" --audio-cycles 2048
      --trace-mask 15 --trace-capacity 65536 --trace-output "${trace_path}"
    RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error TIMEOUT 30)
  if(NOT "${run_result}" STREQUAL "1")
    message(FATAL_ERROR
      "audio-guest: ${name} raw runner exit ${run_result}, expected unregistered exit 1; ${run_error}${run_output}")
  endif()
  string(REGEX MATCH "^[{][^\n]*[}]\n$" one_json_line "${run_output}")
  if(NOT "${one_json_line}" STREQUAL "${run_output}")
    message(FATAL_ERROR "audio-guest: ${name} did not emit one JSON line")
  endif()
  expect_json_field("${run_output}" GUEST_STOP status)
  expect_json_field("${run_output}" unregistered-content reason)
  expect_json_type("${run_output}" NULL fixture id)
  expect_json_type("${run_output}" BOOLEAN fixture ran)
  expect_json_boolean("${run_output}" true fixture ran)
  expect_json_field("${run_output}" pass semantic state)
  expect_json_field("${run_output}" 19001 semantic case_id)
  expect_json_field("${run_output}" unregistered-content termination)
  string(JSON claim_count LENGTH "${run_output}" claim evidence)
  if(NOT claim_count EQUAL 0)
    message(FATAL_ERROR "audio-guest: raw unregistered observation gained evidence claims")
  endif()

  expect_json_field("${run_output}" gamebloke-audio-model-4 audio model)
  expect_json_boolean("${run_output}" true audio requested)
  expect_json_field("${run_output}" complete audio capture_state)
  expect_json_boolean("${run_output}" true audio interval_complete)
  expect_json_field("${run_output}" 4 audio frame_count)
  expect_json_field("${run_output}" 16 audio byte_count)
  expect_json_field("${run_output}" emitted audio artifact state)
  expect_json_field("${run_output}" 0 audio actual_interval start_cycle)
  expect_json_field("${run_output}" 2048 audio actual_interval end_cycle)

  if(NOT EXISTS "${audio_path}" OR IS_DIRECTORY "${audio_path}" OR
      NOT EXISTS "${trace_path}" OR IS_DIRECTORY "${trace_path}")
    message(FATAL_ERROR "audio-guest: ${name} raw audio or trace output is missing")
  endif()
  file(SIZE "${audio_path}" actual_pcm_size)
  file(READ "${audio_path}" actual_pcm_hex HEX)
  file(SHA256 "${audio_path}" actual_pcm_sha)
  expect_json_field("${run_output}" "${actual_pcm_sha}" audio artifact sha256)
  if(NOT actual_pcm_size EQUAL 16 OR
      NOT "${actual_pcm_hex}" STREQUAL "${expected_pcm_hex}")
    message(FATAL_ERROR
      "audio-guest: ${name} PCM mismatch: bytes=${actual_pcm_size}, hex=${actual_pcm_hex}")
  endif()

  string(JSON cycles GET "${run_output}" time cycles)
  string(JSON dma_completed GET "${run_output}" time dma_completed_units)
  string(JSON channel_dma_completed GET "${run_output}" phase9 channels 1 completed_units)
  if(NOT cycles MATCHES "^[1-9][0-9]*$" OR cycles GREATER 20000 OR
      NOT dma_completed MATCHES "^[1-9][0-9]*$" OR
      NOT channel_dma_completed MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
      "audio-guest: ${name} lacks bounded guest progress or FIFO DMA accounting")
  endif()

  file(STRINGS "${trace_path}" trace_lines ENCODING UTF-8)
  set(refill_found FALSE)
  foreach(line IN LISTS trace_lines)
    string(JSON category ERROR_VARIABLE category_error GET "${line}" category)
    if("${category_error}" STREQUAL "NOTFOUND" AND category EQUAL 8)
      string(JSON channel GET "${line}" dma channel)
      string(JSON dma_status GET "${line}" dma status)
      string(JSON source_address GET "${line}" dma source_address)
      string(JSON destination_address GET "${line}" dma destination_address)
      string(JSON width GET "${line}" dma width)
      string(JSON guest_cycle GET "${line}" guest_cycle)
      if(channel EQUAL 1 AND dma_status STREQUAL "ok" AND
          source_address STREQUAL "0x08000300" AND
          destination_address STREQUAL "0x040000A0" AND width EQUAL 4 AND
          guest_cycle LESS 2048)
        set(refill_found TRUE)
      endif()
    endif()
  endforeach()
  if(NOT refill_found)
    message(FATAL_ERROR
      "audio-guest: ${name} trace has no DMA1 FIFO-A transfer from the original payload inside the capture interval")
  endif()
  set(${output_variable} "${run_output}" PARENT_SCOPE)
endfunction()

function(run_registered_guest output_variable name validate_receipt audio_path trace_path)
  file(REMOVE "${audio_path}" "${trace_path}")
  execute_process(
    COMMAND "${runner}" --rom "${audio_image}" --fixture owned-audio-mix
      --cycles 20000 --audio-output "${audio_path}" --audio-cycles 2048
      --trace-mask 15 --trace-capacity 65536 --trace-output "${trace_path}"
    RESULT_VARIABLE run_result OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error TIMEOUT 30)
  if(NOT "${run_result}" STREQUAL "0")
    message(FATAL_ERROR
      "audio-guest: ${name} registered runner exit ${run_result}, expected PASS; ${run_error}${run_output}")
  endif()
  expect_json_field("${run_output}" PASS status)
  expect_json_field("${run_output}" guest-mailbox-pass reason)
  expect_json_field("${run_output}" mailbox-complete termination)
  expect_json_field("${run_output}" owned-audio-mix fixture id)
  expect_json_boolean("${run_output}" true fixture ran)
  expect_json_field("${run_output}" pass semantic state)
  expect_json_field("${run_output}" 19001 semantic case_id)
  string(JSON evidence_count LENGTH "${run_output}" claim evidence)
  if(NOT evidence_count EQUAL 1)
    message(FATAL_ERROR "audio-guest: ${name} evidence does not name one admitted fixture")
  endif()
  expect_json_field("${run_output}" owned-audio-mix claim evidence 0 fixture)
  expect_json_field("${run_output}" project-authored claim evidence 0 provenance_class)
  file(SHA256 "${audio_image}" expected_rom_sha)
  expect_json_field("${run_output}" "${expected_rom_sha}" fixture rom_sha256)
  expect_json_field("${run_output}" "${expected_rom_sha}" claim cart rom_sha256)
  expect_json_field("${run_output}" "${expected_rom_sha}" expected_sha256)
  expect_json_field("${run_output}" complete audio capture_state)
  expect_json_field("${run_output}" 4 audio frame_count)
  expect_json_field("${run_output}" 16 audio byte_count)
  expect_json_field("${run_output}" 0 audio actual_interval start_cycle)
  expect_json_field("${run_output}" 2048 audio actual_interval end_cycle)
  if(NOT EXISTS "${audio_path}" OR IS_DIRECTORY "${audio_path}" OR
      NOT EXISTS "${trace_path}" OR IS_DIRECTORY "${trace_path}")
    message(FATAL_ERROR "audio-guest: ${name} registered audio or trace output is missing")
  endif()
  file(READ "${audio_path}" registered_pcm_hex HEX)
  file(SHA256 "${audio_path}" registered_pcm_sha)
  file(SIZE "${audio_path}" registered_pcm_size)
  expect_json_field("${run_output}" "${registered_pcm_sha}" audio artifact sha256)
  if(NOT registered_pcm_hex STREQUAL expected_pcm_hex OR
      NOT registered_pcm_size EQUAL 16)
    message(FATAL_ERROR "audio-guest: ${name} registered literal PCM mismatch")
  endif()
  string(JSON dma_completed GET "${run_output}" time dma_completed_units)
  string(JSON channel_dma_completed GET "${run_output}" phase9 channels 1 completed_units)
  if(NOT dma_completed MATCHES "^[1-9][0-9]*$" OR
      NOT channel_dma_completed MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "audio-guest: ${name} registered result lost FIFO DMA accounting")
  endif()

  if(validate_receipt)
    set(result_file "${scratch}/${name}.result.json")
    file(WRITE "${result_file}" "${run_output}\n")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" "-DRESULT=${result_file}"
        "-DMANIFEST=${merged_manifest_file}"
        "-DREGISTRY=${SRC}/registry/disputes.json"
        "-DVOCAB=${SRC}/contracts/evidence.json"
        -DGBL_RESULT_REQUIRE_AUDIO=ON -P "${SRC}/cmake/CheckResult.cmake"
      RESULT_VARIABLE result_check OUTPUT_VARIABLE result_output
      ERROR_VARIABLE result_error TIMEOUT 30)
    if(NOT "${result_check}" STREQUAL "0" OR
        NOT "${result_output}${result_error}" MATCHES "result: ok \\(PASS\\)")
      message(FATAL_ERROR
        "audio-guest: ${name} producer audio receipt was not accepted; ${result_output}${result_error}")
    endif()
  endif()
  set(${output_variable} "${run_output}" PARENT_SCOPE)
endfunction()

# Local-only fixtures must be refused before CMake can create build targets or
# generated bytes in a CI environment.
set(ci_refusal_dir "${scratch}/ci-refusal")
file(REMOVE_RECURSE "${ci_refusal_dir}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env CI=true "${CMAKE_COMMAND}"
    -S "${SRC}" -B "${ci_refusal_dir}" -DGBL_LANE=local-audio
  RESULT_VARIABLE ci_result OUTPUT_VARIABLE ci_output ERROR_VARIABLE ci_error
  TIMEOUT 30)
if("${ci_result}" STREQUAL "0" OR
    NOT "${ci_output}${ci_error}" MATCHES
      "local-audio lane is prohibited when CI=true")
  message(FATAL_ERROR
    "audio-guest: CI guard did not refuse local-audio; exit=${ci_result}; ${ci_error}${ci_output}")
endif()
if(EXISTS "${ci_refusal_dir}/generated/fixtures/owned-audio-mix.gba")
  message(FATAL_ERROR "audio-guest: CI refusal left a local audio image")
endif()

foreach(path IN ITEMS "${audio_manifest_file}" "${merged_manifest_file}"
    "${toolchain_receipt}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "audio-guest: local authority build input is missing")
  endif()
endforeach()
file(READ "${audio_manifest_file}" audio_manifest_text)
validate_audio_authority("${audio_manifest_text}" authority_valid authority_reason)
if(NOT authority_valid)
  message(FATAL_ERROR
    "audio-guest: local source/build/rights authority rejected (${authority_reason})")
endif()
file(READ "${merged_manifest_file}" merged_manifest_text)
string(JSON merged_row_count LENGTH "${merged_manifest_text}" fixtures)
set(merged_audio_row_count 0)
if(merged_row_count GREATER 0)
  math(EXPR merged_row_last "${merged_row_count}-1")
  foreach(index RANGE 0 ${merged_row_last})
    string(JSON merged_id GET "${merged_manifest_text}" fixtures ${index} id)
    if(merged_id STREQUAL "owned-audio-mix")
      math(EXPR merged_audio_row_count "${merged_audio_row_count}+1")
    endif()
  endforeach()
endif()
if(NOT merged_audio_row_count EQUAL 1)
  message(FATAL_ERROR
    "audio-guest: local merged production manifest does not contain exactly one audio row")
endif()

# Mutate each authority-bearing field independently and require the local
# admission validator to reject the drift before a production run is accepted.
set(zero_commit "0000000000000000000000000000000000000000")
string(REPEAT "0" 64 zero_digest)
string(JSON changed_revision SET "${audio_manifest_text}" fixtures 0 source commit
  "\"${zero_commit}\"")
expect_audio_authority_rejection("source-revision" "${changed_revision}")
string(JSON original_recipe GET "${audio_manifest_text}" fixtures 0 build recipe)
file(SHA256 "${audio_source}" original_source_digest)
string(REPLACE "source_sha256=${original_source_digest}"
  "source_sha256=${zero_digest}" changed_recipe "${original_recipe}")
string(JSON changed_source_digest SET "${audio_manifest_text}" fixtures 0
  build recipe "\"${changed_recipe}\"")
expect_audio_authority_rejection("source-digest" "${changed_source_digest}")
string(JSON changed_artifact_digest SET "${audio_manifest_text}" fixtures 0
  build artifact sha256 "\"${zero_digest}\"")
expect_audio_authority_rejection("artifact-digest" "${changed_artifact_digest}")
string(JSON changed_notice SET "${audio_manifest_text}" fixtures 0 license notices 0
  "\"tests/fixtures/missing-audio-NOTICE.md\"")
expect_audio_authority_rejection("notice" "${changed_notice}")
string(JSON changed_use SET "${audio_manifest_text}" fixtures 0 permitted_use 0
  "\"public-ci\"")
expect_audio_authority_rejection("permitted-use" "${changed_use}")

# A CI configure uses the standard manifest and targets. The local fixture
# stays absent from its inventory, merged manifest, and generated target graph.
set(ci_selection_dir "${scratch}/ci-selection")
file(REMOVE_RECURSE "${ci_selection_dir}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env CI=true "${CMAKE_COMMAND}"
    -S "${SRC}" -B "${ci_selection_dir}" -DGBL_LANE=ci
  RESULT_VARIABLE ci_selection_result
  OUTPUT_VARIABLE ci_selection_output ERROR_VARIABLE ci_selection_error
  TIMEOUT 60)
if(NOT "${ci_selection_result}" STREQUAL "0")
  message(FATAL_ERROR
    "audio-guest: standard CI configure failed (${ci_selection_result}); ${ci_selection_error}${ci_selection_output}")
endif()
if(EXISTS "${ci_selection_dir}/generated/production/audio-manifest.json" OR
    EXISTS "${ci_selection_dir}/CMakeFiles/gbl_audio_guest_image.dir")
  message(FATAL_ERROR "audio-guest: standard CI selected the local audio fixture")
endif()
file(READ "${SRC}/tests/fixtures/manifest.json" standard_manifest_text)
string(JSON standard_row_count LENGTH "${standard_manifest_text}" fixtures)
if(standard_row_count GREATER 0)
  math(EXPR standard_row_last "${standard_row_count}-1")
  foreach(index RANGE 0 ${standard_row_last})
    string(JSON standard_id GET "${standard_manifest_text}" fixtures ${index} id)
    if(standard_id STREQUAL "owned-audio-mix")
      message(FATAL_ERROR "audio-guest: default manifest selects local audio")
    endif()
  endforeach()
endif()
file(STRINGS "${SRC}/tests/inventory/ci.txt" ci_inventory_lines)
foreach(line IN LISTS ci_inventory_lines)
  if(line MATCHES "^audio[.]guest[.]local([ \t]|$)")
    message(FATAL_ERROR "audio-guest: CI inventory selects the local audio test")
  endif()
endforeach()

set(audio_one "${scratch}/run-one.raw")
set(trace_one "${scratch}/run-one.trace.jsonl")
set(audio_two "${scratch}/run-two.raw")
set(trace_two "${scratch}/run-two.trace.jsonl")
read_raw_run(run_one "first" "${audio_one}" "${trace_one}")
read_raw_run(run_two "repeat" "${audio_two}" "${trace_two}")
if(NOT "${run_one}" STREQUAL "${run_two}")
  message(FATAL_ERROR "audio-guest: fresh production runner result JSON differs")
endif()
file(READ "${trace_one}" trace_one_text)
file(READ "${trace_two}" trace_two_text)
file(READ "${audio_one}" audio_one_hex HEX)
file(READ "${audio_two}" audio_two_hex HEX)
file(SHA256 "${audio_one}" audio_one_sha)
file(SHA256 "${audio_two}" audio_two_sha)
if(NOT "${trace_one_text}" STREQUAL "${trace_two_text}" OR
    NOT "${audio_one_hex}" STREQUAL "${audio_two_hex}" OR
    NOT "${audio_one_sha}" STREQUAL "${audio_two_sha}")
  message(FATAL_ERROR
    "audio-guest: fresh trace, PCM or artifact identity differs")
endif()

set(registered_audio_one "${scratch}/registered-one.raw")
set(registered_trace_one "${scratch}/registered-one.trace.jsonl")
set(registered_audio_two "${scratch}/registered-two.raw")
set(registered_trace_two "${scratch}/registered-two.trace.jsonl")
run_registered_guest(registered_one "registered first run" TRUE
  "${registered_audio_one}" "${registered_trace_one}")
run_registered_guest(registered_two "registered repeat run" FALSE
  "${registered_audio_two}" "${registered_trace_two}")
if(NOT "${registered_one}" STREQUAL "${registered_two}")
  message(FATAL_ERROR
    "audio-guest: fresh registered production runner result JSON differs")
endif()
file(READ "${registered_trace_one}" registered_trace_one_text)
file(READ "${registered_trace_two}" registered_trace_two_text)
file(READ "${registered_audio_one}" registered_audio_one_hex HEX)
file(READ "${registered_audio_two}" registered_audio_two_hex HEX)
file(SHA256 "${registered_audio_one}" registered_audio_one_sha)
file(SHA256 "${registered_audio_two}" registered_audio_two_sha)
if(NOT registered_trace_one_text STREQUAL registered_trace_two_text OR
    NOT registered_audio_one_hex STREQUAL registered_audio_two_hex OR
    NOT registered_audio_one_sha STREQUAL registered_audio_two_sha OR
    NOT registered_audio_one_hex STREQUAL expected_pcm_hex OR
    NOT registered_audio_one_hex STREQUAL audio_one_hex OR
    NOT registered_trace_one_text STREQUAL trace_one_text)
  message(FATAL_ERROR
    "audio-guest: fresh registered trace, PCM or artifact identity differs")
endif()

function(pcm_expectation_matches expected actual output_variable)
  if("${expected}" STREQUAL "${actual}")
    set(${output_variable} TRUE PARENT_SCOPE)
  else()
    set(${output_variable} FALSE PARENT_SCOPE)
  endif()
endfunction()
pcm_expectation_matches("${expected_pcm_hex}" "${audio_one_hex}"
  correct_pcm_match)
pcm_expectation_matches("db660000db66007fdb66007fdb66007f" "${audio_one_hex}"
  altered_pcm_match)
if(NOT correct_pcm_match OR altered_pcm_match)
  message(FATAL_ERROR "audio-guest: literal PCM negative control did not reject drift")
endif()

# The same original generator can make a build-only altered mailbox marker for
# a negative control. It is excluded from every manifest and lane inventory.
set(negative_marker_image
  "${BIN}/generated/fixtures/owned-audio-mix-invalid-marker.gba")
execute_process(
  COMMAND "${BIN}/gbl_audio_guest_image" "${BIN}/generated/fixtures"
    --negative-completion-marker
  RESULT_VARIABLE negative_generate_result
  OUTPUT_VARIABLE negative_generate_output ERROR_VARIABLE negative_generate_error
  TIMEOUT 30)
if(NOT "${negative_generate_result}" STREQUAL "0" OR
    NOT EXISTS "${negative_marker_image}" OR IS_DIRECTORY "${negative_marker_image}")
  message(FATAL_ERROR
    "audio-guest: could not create bounded mailbox negative control; ${negative_generate_error}${negative_generate_output}")
endif()
file(SHA256 "${negative_marker_image}" negative_marker_sha)
file(SHA256 "${audio_image}" expected_marker_sha)
if(negative_marker_sha STREQUAL expected_marker_sha)
  message(FATAL_ERROR "audio-guest: negative marker image did not differ")
endif()
execute_process(
  COMMAND "${runner}" --rom "${negative_marker_image}" --cycles 8000
  RESULT_VARIABLE negative_marker_result
  OUTPUT_VARIABLE negative_marker_output ERROR_VARIABLE negative_marker_error
  TIMEOUT 30)
if(NOT "${negative_marker_result}" STREQUAL "1")
  message(FATAL_ERROR
    "audio-guest: wrong completion marker runner exit ${negative_marker_result}; ${negative_marker_error}${negative_marker_output}")
endif()
expect_json_field("${negative_marker_output}" TIMEOUT status)
expect_json_field("${negative_marker_output}" cycle-budget reason)
expect_json_field("${negative_marker_output}" pending semantic state)
expect_json_field("${negative_marker_output}" 19001 semantic case_id)
message(STATUS
  "audio-guest: wrong completion marker remained pending; altered literal PCM expectation rejected")
message(STATUS
  "audio-guest: registered and raw BIOS-free guest reached case 19001; 4-frame literal PCM and actual FIFO-A DMA refill passed twice; local-only CI guard refused selection")
