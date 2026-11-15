cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS CASE SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "result-channel: usage: -D${required}= is required")
  endif()
endforeach()
if(NOT "${CASE}" MATCHES "^(metadata|roundtrip|results)$")
  message(FATAL_ERROR "result-channel: unknown CASE=${CASE}")
endif()

include("${SRC}/tests/gates/json_mutate.cmake")
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)
set(dir "${BIN}/result-channel/${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
set(commit "a7113b67e63f83a9b321696ddd7042ccfad6c881")
set(digest "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
set(base "${dir}/base.json")
file(COPY_FILE "${SRC}/contracts/examples/manifest.json" "${base}")
gbl_json_mutate_set("${base}" "\"${commit}\"" fixtures 0 source commit)
string(JSON artifact SET "{}" path "\"build/owned-synthetic.gba\"")
string(JSON artifact SET "${artifact}" sha256 "\"${digest}\"")
gbl_json_mutate_set("${base}" "${artifact}" fixtures 0 build artifact)

function(make_channel out kind source_revision source_path address instruction_set register)
  set(channel "{}")
  string(JSON channel SET "${channel}" kind "\"${kind}\"")
  string(JSON channel SET "${channel}" source_commit "\"${source_revision}\"")
  string(JSON channel SET "${channel}" source_path "\"${source_path}\"")
  string(JSON channel SET "${channel}" entry_address "\"${address}\"")
  string(JSON channel SET "${channel}" instruction_set "\"${instruction_set}\"")
  string(JSON channel SET "${channel}" register_index "${register}")
  set(${out} "${channel}" PARENT_SCOPE)
endfunction()

function(copy_base out variant)
  set(path "${dir}/${variant}.json")
  file(COPY_FILE "${base}" "${path}")
  set(${out} "${path}" PARENT_SCOPE)
endfunction()

function(set_channel file channel)
  gbl_json_mutate_set("${file}" "${channel}" fixtures 0 result_channel)
endfunction()

function(set_channel_field file field json_value)
  gbl_json_mutate_set("${file}" "${json_value}" fixtures 0 result_channel "${field}")
endfunction()

function(remove_channel_field file field)
  file(READ "${file}" doc)
  string(JSON row GET "${doc}" fixtures 0)
  string(JSON channel GET "${row}" result_channel)
  string(JSON channel REMOVE "${channel}" "${field}")
  string(JSON row SET "${row}" result_channel "${channel}")
  gbl_json_mutate_set("${file}" "${row}" fixtures 0)
endfunction()

function(run_manifest out_result out_report manifest)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DMODE=manifest "-DMANIFEST=${manifest}"
      "-DREGISTRY=${SRC}/registry/disputes.json"
      "-DVOCAB=${SRC}/contracts/evidence.json"
      -P "${SRC}/cmake/CheckManifest.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  set(${out_result} "${result}" PARENT_SCOPE)
  set(${out_report} "${stdout}${stderr}" PARENT_SCOPE)
endfunction()

function(expect_reason reason manifest)
  run_manifest(result report "${manifest}")
  if("${result}" STREQUAL "0" OR
      NOT "${report}" MATCHES "manifest: ${reason}: example-row")
    message(FATAL_ERROR
      "result-channel: expected ${reason} refusal for ${manifest}\n${report}")
  endif()
endfunction()

function(expect_pass manifest expected_rows)
  run_manifest(result report "${manifest}")
  if(NOT "${result}" STREQUAL "0" OR
      NOT "${report}" MATCHES "manifest: ok \\(${expected_rows} rows\\)")
    message(FATAL_ERROR "result-channel: clean manifest rejected\n${report}")
  endif()
endfunction()

function(make_source out revision)
  set(source "{}")
  string(JSON source SET "${source}" kind "\"git\"")
  string(JSON source SET "${source}" url "\"https://github.com/jsmolka/gba-tests\"")
  string(JSON source SET "${source}" commit "\"${revision}\"")
  set(${out} "${source}" PARENT_SCOPE)
endfunction()

function(append_register_row manifest id source artifact channel)
  file(READ "${manifest}" doc)
  string(JSON row GET "${doc}" fixtures 0)
  string(JSON row SET "${row}" id "\"${id}\"")
  string(JSON row SET "${row}" source "${source}")
  string(JSON row SET "${row}" build artifact "${artifact}")
  string(JSON row SET "${row}" result_channel "${channel}")
  string(JSON count LENGTH "${doc}" fixtures)
  gbl_json_mutate_set("${manifest}" "${row}" fixtures ${count})
endfunction()

function(run_generator out_result out_report manifest artifact_root
    production_index control_index control_manifest control_image)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DGBL_SOURCE_DIR=${SRC}"
      "-DGBL_OUTPUT_ROOT=${dir}"
      "-DGBL_ARTIFACT_ROOT=${artifact_root}"
      "-DGBL_CONTROL_IMAGE=${control_image}"
      "-DGBL_CONTROL_MANIFEST_OUTPUT=${control_manifest}"
      "-DGBL_MANIFEST=${manifest}"
      "-DGBL_REGISTRY=${SRC}/registry/disputes.json"
      "-DGBL_VOCAB=${SRC}/contracts/evidence.json"
      "-DGBL_OUT_PRODUCTION=${production_index}"
      "-DGBL_OUT_CONTROL=${control_index}"
      -P "${SRC}/cmake/GenerateFixtureIndex.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  set(${out_result} "${result}" PARENT_SCOPE)
  set(${out_report} "${stdout}${stderr}" PARENT_SCOPE)
endfunction()

function(compile_and_run name include_dir source_file)
  set(executable "${dir}/${name}")
  execute_process(
    COMMAND "${C_COMPILER}" -std=c17 -Wall -Wextra -Werror
      "-I${include_dir}" "${source_file}" -o "${executable}"
    RESULT_VARIABLE compile_result OUTPUT_VARIABLE compile_out
    ERROR_VARIABLE compile_error)
  if(NOT "${compile_result}" STREQUAL "0")
    message(FATAL_ERROR
      "result-channel: ${name} compile failed\n${compile_out}${compile_error}")
  endif()
  execute_process(COMMAND "${executable}"
    RESULT_VARIABLE run_result OUTPUT_VARIABLE run_out ERROR_VARIABLE run_error)
  if(NOT "${run_result}" STREQUAL "0")
    message(FATAL_ERROR
      "result-channel: ${name} probe failed (${run_result})\n${run_out}${run_error}")
  endif()
endfunction()

function(run_result_checker out_result out_report result_file manifest)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DRESULT=${result_file}"
      "-DMANIFEST=${manifest}" "-DREGISTRY=${SRC}/registry/disputes.json"
      "-DVOCAB=${SRC}/contracts/evidence.json"
      -P "${SRC}/cmake/CheckResult.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  set(${out_result} "${result}" PARENT_SCOPE)
  set(${out_report} "${stdout}${stderr}" PARENT_SCOPE)
endfunction()

function(expect_result_pass result_file manifest)
  run_result_checker(result report "${result_file}" "${manifest}")
  if(NOT "${result}" STREQUAL "0" OR
      NOT "${report}" MATCHES "result: ok ")
    message(FATAL_ERROR
      "result-channel: valid result rejected: ${result_file}\n${report}")
  endif()
endfunction()

function(expect_result_reason reason result_file manifest)
  run_result_checker(result report "${result_file}" "${manifest}")
  if("${result}" STREQUAL "0" OR
      NOT "${report}" MATCHES "result: ${reason}:")
    message(FATAL_ERROR
      "result-channel: expected ${reason} refusal: ${result_file}\n${report}")
  endif()
endfunction()

function(run_control_result out_file out_status fixture image)
  set(path "${dir}/${out_file}.json")
  execute_process(
    COMMAND "${BIN}/gamebloke-run-control" --rom "${image}"
      --fixture "${fixture}" --cycles 100
    RESULT_VARIABLE status OUTPUT_VARIABLE result_json ERROR_VARIABLE stderr)
  if(NOT "${status}" STREQUAL "0" AND NOT "${status}" STREQUAL "1")
    message(FATAL_ERROR
      "result-channel: synthetic runner failed (${status}) for ${fixture}\n${result_json}${stderr}")
  endif()
  file(WRITE "${path}" "${result_json}")
  set(${out_file}_PATH "${path}" PARENT_SCOPE)
  set(${out_status} "${status}" PARENT_SCOPE)
endfunction()

function(assert_json_scalar file expected_type expected_value)
  file(READ "${file}" document)
  string(JSON actual_type ERROR_VARIABLE type_error TYPE "${document}" ${ARGN})
  if(NOT "${type_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual_type}" STREQUAL "${expected_type}")
    message(FATAL_ERROR
      "result-channel: ${file} has the wrong JSON type at ${ARGN}: ${actual_type}")
  endif()
  string(JSON actual_value ERROR_VARIABLE value_error GET "${document}" ${ARGN})
  if(NOT "${value_error}" STREQUAL "NOTFOUND" OR
      NOT "${actual_value}" STREQUAL "${expected_value}")
    message(FATAL_ERROR
      "result-channel: ${file} has the wrong JSON value at ${ARGN}: ${actual_value}")
  endif()
endfunction()

function(assert_no_root_member file key)
  file(READ "${file}" document)
  string(JSON key_count LENGTH "${document}")
  if(key_count GREATER 0)
    math(EXPR key_last "${key_count}-1")
    foreach(key_index RANGE ${key_last})
      string(JSON actual_key MEMBER "${document}" ${key_index})
      if("${actual_key}" STREQUAL "${key}")
        message(FATAL_ERROR
          "result-channel: ${file} unexpectedly contains ${key}")
      endif()
    endforeach()
  endif()
endfunction()

if(CASE STREQUAL "results")
  set(manifest "${BIN}/generated/control/manifest.json")
  if(NOT EXISTS "${manifest}" OR
      NOT EXISTS "${BIN}/gamebloke-run-control")
    message(FATAL_ERROR
      "result-channel: build the ci preset before the results contract")
  endif()

  # Exercise real JSON from the local generated runner controls. The generated
  # bytes are artificial, build-only input; this gate never fetches upstream ROMs.
  run_control_result(arm_zero arm_zero_status
    synthetic-control-register-arm-zero
    "${BIN}/generated/fixtures/owned-register-arm-zero.gba")
  run_control_result(arm_fail arm_fail_status
    synthetic-control-register-arm-fail
    "${BIN}/generated/fixtures/owned-register-arm-fail.gba")
  run_control_result(thumb_zero thumb_zero_status
    synthetic-control-register-thumb-zero
    "${BIN}/generated/fixtures/owned-register-thumb-zero.gba")
  run_control_result(thumb_fail thumb_fail_status
    synthetic-control-register-thumb-fail
    "${BIN}/generated/fixtures/owned-register-thumb-fail.gba")
  run_control_result(never_entry never_entry_status
    synthetic-control-register-arm-never-entry
    "${BIN}/generated/fixtures/owned-register-arm-zero.gba")
  run_control_result(mailbox mailbox_status synthetic-control-pass
    "${BIN}/generated/fixtures/owned-arm-pass.gba")
  if(NOT "${arm_zero_status}" STREQUAL "0" OR
      NOT "${thumb_zero_status}" STREQUAL "0" OR
      NOT "${arm_fail_status}" STREQUAL "1" OR
      NOT "${thumb_fail_status}" STREQUAL "1" OR
      NOT "${never_entry_status}" STREQUAL "1" OR
      NOT "${mailbox_status}" STREQUAL "0")
    message(FATAL_ERROR "result-channel: synthetic control status mismatch")
  endif()

  foreach(pair IN ITEMS
      "${arm_zero_PATH}|arm|12|0|0x080000C4"
      "${arm_fail_PATH}|arm|12|1|0x080000C4"
      "${thumb_zero_PATH}|thumb|7|0|0x080000D2"
      "${thumb_fail_PATH}|thumb|7|1|0x080000D2")
    string(REPLACE "|" ";" fields "${pair}")
    list(GET fields 0 result_file)
    list(GET fields 1 instruction_set)
    list(GET fields 2 register_index)
    list(GET fields 3 raw_value)
    list(GET fields 4 entry_address)
    assert_json_scalar("${result_file}" STRING "register-eval" observation kind)
    assert_json_scalar("${result_file}" BOOLEAN ON observation observed)
    assert_json_scalar("${result_file}" STRING
      "a7113b67e63f83a9b321696ddd7042ccfad6c881" observation source_commit)
    assert_json_scalar("${result_file}" STRING "${entry_address}"
      observation entry_address)
    assert_json_scalar("${result_file}" STRING "${instruction_set}"
      observation instruction_set)
    assert_json_scalar("${result_file}" NUMBER "${register_index}"
      observation register_index)
    assert_json_scalar("${result_file}" NUMBER "${raw_value}" observation value)
    assert_json_scalar("${result_file}" STRING absent semantic state)
  endforeach()
  assert_json_scalar("${never_entry_PATH}" BOOLEAN OFF observation observed)
  assert_json_scalar("${never_entry_PATH}" NULL "" observation value)
  assert_json_scalar("${never_entry_PATH}" STRING absent semantic state)
  assert_no_root_member("${mailbox_PATH}" observation)
  assert_json_scalar("${mailbox_PATH}" STRING pass semantic state)
  assert_json_scalar("${mailbox_PATH}" STRING 36865 semantic case_id)

  foreach(result_file IN ITEMS "${arm_zero_PATH}" "${arm_fail_PATH}"
      "${thumb_zero_PATH}" "${thumb_fail_PATH}" "${never_entry_PATH}"
      "${mailbox_PATH}")
    expect_result_pass("${result_file}" "${manifest}")
  endforeach()

  # A register outcome is bound to every canonical channel field, the exact
  # runtime ROM digest, its raw value, and the named observation termination.
  set(tamper_base "${arm_zero_PATH}")
  function(copy_result out name)
    set(path "${dir}/${name}.json")
    file(COPY_FILE "${tamper_base}" "${path}")
    set(${out} "${path}" PARENT_SCOPE)
  endfunction()
  copy_result(tampered "missing-observation")
  gbl_json_mutate_set("${tampered}" null observation)
  expect_result_reason(bad-observation "${tampered}" "${manifest}")

  foreach(field IN ITEMS kind source_commit entry_address instruction_set
      register_index observed value)
    copy_result(tampered "wrong-${field}")
    if(field STREQUAL "kind")
      gbl_json_mutate_set("${tampered}" "\"mailbox\"" observation kind)
    elseif(field STREQUAL "source_commit")
      gbl_json_mutate_set("${tampered}"
        "\"0123456789abcdef0123456789abcdef01234567\""
        observation source_commit)
    elseif(field STREQUAL "entry_address")
      gbl_json_mutate_set("${tampered}" "\"0x080000C8\""
        observation entry_address)
    elseif(field STREQUAL "instruction_set")
      gbl_json_mutate_set("${tampered}" "\"thumb\""
        observation instruction_set)
    elseif(field STREQUAL "register_index")
      gbl_json_mutate_set("${tampered}" 11 observation register_index)
    elseif(field STREQUAL "observed")
      gbl_json_mutate_set("${tampered}" false observation observed)
    elseif(field STREQUAL "value")
      gbl_json_mutate_set("${tampered}" 1 observation value)
    endif()
    expect_result_reason(bad-observation "${tampered}" "${manifest}")

    copy_result(missing "missing-${field}")
    gbl_json_mutate_remove("${missing}" observation "${field}")
    expect_result_reason(bad-observation "${missing}" "${manifest}")
  endforeach()

  copy_result(tampered "bad-status")
  gbl_json_mutate_set("${tampered}" "\"GUEST_FAIL\"" status)
  expect_result_reason(bad-observation "${tampered}" "${manifest}")
  copy_result(tampered "wrong-digest")
  gbl_json_mutate_set("${tampered}"
    "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\""
    fixture rom_sha256)
  expect_result_reason(claim-mismatch "${tampered}" "${manifest}")
  copy_result(tampered "wrong-termination")
  gbl_json_mutate_set("${tampered}" "\"mailbox-complete\"" termination)
  expect_result_reason(bad-observation "${tampered}" "${manifest}")
  copy_result(tampered "incomplete-trace")
  gbl_json_mutate_set("${tampered}" "{\"complete\":false}" trace)
  expect_result_reason(incomplete-trace "${tampered}" "${manifest}")

  # Pending is valid evidence for an interrupted register-channel run, but it
  # cannot be promoted to a completed PASS. The mailbox route retains its old
  # completed nonzero-case-id requirement and rejects any unbound observation.
  copy_result(tampered "pending-pass")
  gbl_json_mutate_set("${tampered}" false observation observed)
  gbl_json_mutate_set("${tampered}" null observation value)
  expect_result_reason(bad-observation "${tampered}" "${manifest}")
  copy_result(tampered "mailbox-with-observation")
  file(COPY_FILE "${mailbox_PATH}" "${tampered}")
  gbl_json_mutate_set("${tampered}" "{\"kind\":\"register-eval\"}" observation)
  expect_result_reason(bad-observation "${tampered}" "${manifest}")

  # A legacy mailbox result still requires the actual complete mailbox, and is
  # checked through its unchanged PASS rule.
  file(COPY_FILE "${mailbox_PATH}" "${dir}/mailbox-no-case.json")
  gbl_json_mutate_set("${dir}/mailbox-no-case.json" "\"0\"" semantic case_id)
  expect_result_reason(bad-semantic "${dir}/mailbox-no-case.json" "${manifest}")
  message("result-channel: actual register/mailbox results and tamper controls passed")
  return()
endif()

if(CASE STREQUAL "metadata")
  # Both legacy rows and complete source-bound register rows use the same
  # canonical manifest reader and retain local-only rights metadata.
  expect_pass("${base}" 1)
  copy_base(valid "valid-register")
  make_channel(channel register-eval "${commit}" "arm/arm.asm"
    "0x08001D4C" arm 12)
  set_channel("${valid}" "${channel}")
  expect_pass("${valid}" 1)

  copy_base(url_bound "url-revision")
  set("url_source" "{}")
  string(JSON url_source SET "${url_source}" kind "\"url\"")
  string(JSON url_source SET "${url_source}" url
    "\"https://example.invalid/raw/${commit}/arm.gba\"")
  string(JSON url_source SET "${url_source}" sha256 "\"${digest}\"")
  gbl_json_mutate_set("${url_bound}" "${url_source}" fixtures 0 source)
  file(READ "${url_bound}" url_doc)
  string(JSON url_row GET "${url_doc}" fixtures 0)
  string(JSON url_row REMOVE "${url_row}" build artifact)
  gbl_json_mutate_set("${url_bound}" "${url_row}" fixtures 0)
  set_channel("${url_bound}" "${channel}")
  expect_pass("${url_bound}" 1)

  foreach(field IN ITEMS kind source_commit source_path entry_address instruction_set register_index)
    copy_base(missing "missing-${field}")
    set_channel("${missing}" "${channel}")
    remove_channel_field("${missing}" "${field}")
    expect_reason(result-channel-missing-field "${missing}")
  endforeach()

  copy_base(wrong_object "wrong-object")
  gbl_json_mutate_set("${wrong_object}" "[]" fixtures 0 result_channel)
  expect_reason(result-channel-wrong-type "${wrong_object}")

  foreach(field IN ITEMS kind source_commit source_path entry_address instruction_set)
    copy_base(wrong_type "wrong-type-${field}")
    set_channel("${wrong_type}" "${channel}")
    set_channel_field("${wrong_type}" "${field}" true)
    expect_reason(result-channel-wrong-type "${wrong_type}")
  endforeach()
  copy_base(wrong_type_register "wrong-type-register")
  set_channel("${wrong_type_register}" "${channel}")
  set_channel_field("${wrong_type_register}" register_index "\"12\"")
  expect_reason(result-channel-wrong-type "${wrong_type_register}")

  copy_base(unknown "unknown-key")
  string(JSON unknown_channel SET "${channel}" observer_mode "\"unreviewed\"")
  set_channel("${unknown}" "${unknown_channel}")
  expect_reason(result-channel-bad-key "${unknown}")

  copy_base(bad_kind "bad-kind")
  set_channel("${bad_kind}" "${channel}")
  set_channel_field("${bad_kind}" kind "\"mailbox-plus-register\"")
  expect_reason(result-channel-bad-kind "${bad_kind}")

  copy_base(bad_revision "bad-revision")
  set_channel("${bad_revision}" "${channel}")
  set_channel_field("${bad_revision}" source_commit "\"A7113b67e63f83a9b321696ddd7042ccfad6c881\"")
  expect_reason(result-channel-bad-source-revision "${bad_revision}")
  copy_base(mismatched_revision "mismatched-revision")
  set_channel("${mismatched_revision}" "${channel}")
  set_channel_field("${mismatched_revision}" source_commit
    "\"0123456789abcdef0123456789abcdef01234567\"")
  expect_reason(result-channel-source-revision-mismatch "${mismatched_revision}")

  copy_base(bad_path "bad-source-path")
  set_channel("${bad_path}" "${channel}")
  set_channel_field("${bad_path}" source_path "\"../arm.asm\"")
  expect_reason(result-channel-bad-source-path "${bad_path}")

  copy_base(bad_address "bad-address")
  set_channel("${bad_address}" "${channel}")
  set_channel_field("${bad_address}" entry_address "\"08001D4C\"")
  expect_reason(result-channel-bad-entry-address "${bad_address}")
  copy_base(outside_rom "outside-rom")
  set_channel("${outside_rom}" "${channel}")
  set_channel_field("${outside_rom}" entry_address "\"0x0A000000\"")
  expect_reason(result-channel-bad-entry-address "${outside_rom}")
  copy_base(bad_state "bad-instruction-set")
  set_channel("${bad_state}" "${channel}")
  set_channel_field("${bad_state}" instruction_set "\"armv4t\"")
  expect_reason(result-channel-bad-instruction-set "${bad_state}")
  copy_base(bad_arm_alignment "bad-arm-alignment")
  set_channel("${bad_arm_alignment}" "${channel}")
  set_channel_field("${bad_arm_alignment}" entry_address "\"0x08001D4E\"")
  expect_reason(result-channel-bad-alignment "${bad_arm_alignment}")
  copy_base(bad_thumb_alignment "bad-thumb-alignment")
  make_channel(thumb_channel register-eval "${commit}" "thumb/thumb.asm"
    "0x08000935" thumb 7)
  set_channel("${bad_thumb_alignment}" "${thumb_channel}")
  expect_reason(result-channel-bad-alignment "${bad_thumb_alignment}")

  foreach(invalid_register IN ITEMS 15 -1 1.5)
    copy_base(bad_register "bad-register-${invalid_register}")
    set_channel("${bad_register}" "${channel}")
    set_channel_field("${bad_register}" register_index "${invalid_register}")
    expect_reason(result-channel-bad-register "${bad_register}")
  endforeach()

  copy_base(missing_digest "missing-runtime-digest")
  file(READ "${base}" base_doc)
  string(JSON no_artifact_row GET "${base_doc}" fixtures 0)
  string(JSON no_artifact_row REMOVE "${no_artifact_row}" build artifact)
  gbl_json_mutate_set("${missing_digest}" "${no_artifact_row}" fixtures 0)
  set_channel("${missing_digest}" "${channel}")
  expect_reason(result-channel-missing-digest "${missing_digest}")
  message("result-channel: metadata positives and refusal controls passed")
  return()
endif()

find_program(C_COMPILER NAMES clang gcc cc)
if(NOT C_COMPILER)
  message(FATAL_ERROR "result-channel: no C17 compiler found for generated-header probe")
endif()

# Build-only synthetic bytes large enough to contain all three metadata entry
# offsets. They are locally created, never fetched, committed, or executed.
set(artifact_root "${dir}/artifacts")
file(MAKE_DIRECTORY "${artifact_root}/generated/fixtures")
string(REPEAT "S" 16384 synthetic_rom)
set(rom_path "generated/fixtures/owned-register-control.gba")
set(rom_file "${artifact_root}/${rom_path}")
file(WRITE "${rom_file}" "${synthetic_rom}")
file(SHA256 "${rom_file}" rom_digest)
string(JSON rom_artifact SET "{}" path "\"${rom_path}\"")
string(JSON rom_artifact SET "${rom_artifact}" sha256 "\"${rom_digest}\"")
make_source(source "${commit}")
set(manifest "${dir}/roundtrip-manifest.json")
file(COPY_FILE "${base}" "${manifest}")
gbl_json_mutate_set("${manifest}" "\"synthetic-legacy\"" fixtures 0 id)
gbl_json_mutate_set("${manifest}" "${source}" fixtures 0 source)

make_channel(arm_channel register-eval "${commit}" "arm/arm.asm"
  "0x08001D4C" arm 12)
append_register_row("${manifest}" synthetic-arm "${source}"
  "${rom_artifact}" "${arm_channel}")
make_channel(thumb_channel register-eval "${commit}" "thumb/thumb.asm"
  "0x08000934" thumb 7)
append_register_row("${manifest}" synthetic-thumb "${source}"
  "${rom_artifact}" "${thumb_channel}")
make_channel(memory_channel register-eval "${commit}" "memory/memory.asm"
  "0x08000350" arm 12)
append_register_row("${manifest}" synthetic-memory "${source}"
  "${rom_artifact}" "${memory_channel}")

set(control_image "${artifact_root}/generated/fixtures/owned-arm-pass.gba")
file(WRITE "${control_image}" "owned synthetic register control image\n")
set(production_index "${dir}/generated/production/fixture_index.h")
set(control_index "${dir}/generated/control/fixture_index.h")
set(control_manifest "${dir}/generated/control/manifest.json")
run_generator(generator_result generator_report "${manifest}" "${artifact_root}"
  "${production_index}" "${control_index}" "${control_manifest}"
  "${control_image}")
if(NOT "${generator_result}" STREQUAL "0" OR
    NOT EXISTS "${production_index}" OR NOT EXISTS "${control_index}")
  message(FATAL_ERROR
    "result-channel: initial index generation failed\n${generator_report}")
endif()

set(production_probe "${dir}/production_probe.c")
file(WRITE "${production_probe}" "#include <string.h>\n#include \"fixture_index.h\"\n"
  "int main(void) {\n"
  "  if (gbl_fixture_metadata_count != 4u) return 1;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[0].expected_mailbox_convention, \"five-word-v1-final-marker\") != 0) return 2;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[0].result_channel_kind, \"mailbox\") != 0) return 3;\n"
  "  if (gbl_fixture_metadata_rows[0].result_entry_address != 0u) return 4;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[1].result_channel_kind, \"register-eval\") != 0) return 5;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[1].result_source_commit, \"${commit}\") != 0) return 6;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[1].result_source_path, \"arm/arm.asm\") != 0) return 7;\n"
  "  if (gbl_fixture_metadata_rows[1].result_entry_address != 0x08001D4Cu) return 8;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[1].result_instruction_set, \"arm\") != 0) return 9;\n"
  "  if (gbl_fixture_metadata_rows[1].result_register_index != 12u) return 10;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[2].result_source_path, \"thumb/thumb.asm\") != 0 || gbl_fixture_metadata_rows[2].result_entry_address != 0x08000934u || gbl_fixture_metadata_rows[2].result_register_index != 7u) return 11;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[3].result_source_path, \"memory/memory.asm\") != 0 || gbl_fixture_metadata_rows[3].result_entry_address != 0x08000350u || gbl_fixture_metadata_rows[3].result_register_index != 12u) return 12;\n"
  "  return 0;\n}\n")
compile_and_run(production-probe "${dir}/generated/production" "${production_probe}")

set(control_probe "${dir}/control_probe.c")
file(WRITE "${control_probe}" "#include <string.h>\n#include \"fixture_index.h\"\n"
  "int main(void) {\n"
  "  if (gbl_fixture_metadata_count != 3u) return 1;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[1].id, \"synthetic-control-register-arm\") != 0 || gbl_fixture_metadata_rows[1].result_register_index != 12u || gbl_fixture_metadata_rows[1].result_entry_address != 0x08001D4Cu) return 2;\n"
  "  if (strcmp(gbl_fixture_metadata_rows[2].id, \"synthetic-control-register-thumb\") != 0 || gbl_fixture_metadata_rows[2].result_register_index != 7u || gbl_fixture_metadata_rows[2].result_entry_address != 0x08000934u) return 3;\n"
  "  return 0;\n}\n")
compile_and_run(control-probe "${dir}/generated/control" "${control_probe}")

# The generator clears safe stale headers before validating current metadata.
set(invalid_manifest "${dir}/invalid-manifest.json")
file(COPY_FILE "${manifest}" "${invalid_manifest}")
gbl_json_mutate_set("${invalid_manifest}"
  "\"0123456789abcdef0123456789abcdef01234567\""
  fixtures 1 result_channel source_commit)
run_generator(invalid_result invalid_report "${invalid_manifest}"
  "${artifact_root}" "${production_index}" "${control_index}"
  "${control_manifest}" "${control_image}")
if("${invalid_result}" STREQUAL "0" OR
    NOT "${invalid_report}" MATCHES "result-channel-source-revision-mismatch" OR
    EXISTS "${production_index}" OR EXISTS "${control_index}")
  message(FATAL_ERROR
    "result-channel: invalid regeneration accepted or left stale usable indexes\n${invalid_report}")
endif()
message("result-channel: manifest-to-C-header roundtrip and stale-index refusal passed")
