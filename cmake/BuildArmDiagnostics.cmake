cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC OUT TOOLCHAIN_ROOT MODE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "arm-diagnostics: missing-${required}")
  endif()
endforeach()
if(NOT MODE MATCHES "^(smoke|case|all)$")
  message(FATAL_ERROR "arm-diagnostics: unsupported-mode: ${MODE}")
endif()
if(MODE STREQUAL "case" AND
   (NOT DEFINED CASE_ID OR NOT "${CASE_ID}" MATCHES "^[1-9][0-9]*$"))
  message(FATAL_ERROR "arm-diagnostics: missing-or-invalid-case-id")
endif()

file(REAL_PATH "${SRC}" source_root)
if(NOT IS_DIRECTORY "${source_root}")
  message(FATAL_ERROR "arm-diagnostics: source-root-missing")
endif()

set(matrix_file "${source_root}/diagnostics/armv4t/cases.json")
set(coverage_checker "${source_root}/cmake/CheckArmCoverage.cmake")
set(inputs_file "${source_root}/diagnostics/armv4t/inputs.txt")
set(is_matrix_child FALSE)
if(MODE STREQUAL "case" AND MATRIX_CHILD STREQUAL "1")
  set(is_matrix_child TRUE)
else()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DMODE=draft "-DMATRIX=${matrix_file}"
      -P "${coverage_checker}"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE coverage_rc OUTPUT_VARIABLE coverage_output
    ERROR_VARIABLE coverage_error)
  if(NOT "${coverage_rc}" STREQUAL "0")
    message(FATAL_ERROR
      "arm-diagnostics: coverage-ledger-invalid: ${coverage_error}${coverage_output}")
  endif()
endif()

if(MODE STREQUAL "case")
  file(READ "${matrix_file}" matrix_json)
  string(JSON case_count LENGTH "${matrix_json}" cases)
  math(EXPR case_last "${case_count}-1")
  set(selected_index -1)
  if(DEFINED MATRIX_CASE_INDEX)
    if(NOT is_matrix_child OR NOT MATRIX_CASE_INDEX MATCHES "^(0|[1-9][0-9]*)$" OR
       MATRIX_CASE_INDEX GREATER_EQUAL case_count)
      message(FATAL_ERROR "arm-diagnostics: invalid-matrix-case-index")
    endif()
    string(JSON selected_row GET "${matrix_json}" cases ${MATRIX_CASE_INDEX})
    string(JSON hinted_id GET "${selected_row}" id)
    if(NOT hinted_id STREQUAL "${CASE_ID}")
      message(FATAL_ERROR "arm-diagnostics: matrix-case-index-id-mismatch")
    endif()
    set(selected_index "${MATRIX_CASE_INDEX}")
  else()
    foreach(index RANGE 0 ${case_last})
      string(JSON row_json GET "${matrix_json}" cases ${index})
      string(JSON row_id GET "${row_json}" id)
      if("${row_id}" STREQUAL "${CASE_ID}")
        set(selected_index "${index}")
        set(selected_row "${row_json}")
        break()
      endif()
    endforeach()
  endif()
  if(selected_index LESS 0)
    message(FATAL_ERROR "arm-diagnostics: case-id-unknown: ${CASE_ID}")
  endif()
  string(JSON selected_state GET "${selected_row}" coverage_state)
  if(selected_state STREQUAL "pending" AND "${CASE_ID}" STREQUAL "20481")
    message(FATAL_ERROR "arm-diagnostics: case-not-runnable: pending id=${CASE_ID}")
  endif()
  string(JSON selected_family GET "${selected_row}" family)
  string(JSON selected_target GET "${selected_row}" inventory_target)
  string(JSON selected_kind ERROR_VARIABLE selected_kind_error GET
    "${selected_row}" source_test kind)
  if(NOT "${CASE_ID}" STREQUAL "20481" AND
     NOT "${selected_kind_error}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR "arm-diagnostics: case-not-runnable: pending id=${CASE_ID}")
  endif()
  if("${CASE_ID}" STREQUAL "20481")
    if(NOT selected_state MATCHES "^(source-verified|verified)$" OR
       NOT selected_family STREQUAL "diagnostic-harness" OR selected_target)
      message(FATAL_ERROR "arm-diagnostics: case-builder-unimplemented: ${CASE_ID}")
    endif()
    set(selected_kind smoke)
  else()
    if(NOT DEFINED ISA OR NOT ISA STREQUAL "arm" OR
       NOT selected_state STREQUAL "pending" OR
       (selected_target AND NOT selected_family MATCHES "^(data-processing|condition|pc-control|shifter-boundary|multiply|status-register|single-transfer|halfword-signed-transfer|swap|block-transfer|block-transfer-edge)$"))
      message(FATAL_ERROR "arm-diagnostics: arm-case-selection-invalid: ${CASE_ID}")
    endif()
    string(JSON selected_isa GET "${selected_row}" isa)
    string(JSON selected_source_program GET "${selected_row}" source_test program)
    if(NOT selected_isa STREQUAL "arm" OR
       NOT selected_kind MATCHES "^(semantic|guest-stop)$" OR
       "${selected_source_program}" STREQUAL "")
      message(FATAL_ERROR "arm-diagnostics: arm-case-source-missing: ${CASE_ID}")
    endif()
  endif()
  if(selected_kind STREQUAL "smoke")
    string(JSON selected_positive_fixture GET "${selected_row}" positive_fixture)
    string(JSON selected_negative_fixture GET "${selected_row}" negative_fixture)
    string(JSON selected_fail_case GET "${selected_row}" expected_fail_case_id)
    string(JSON broken_source GET "${selected_row}" broken_assertion source)
    string(JSON arm_clean_operand GET "${selected_row}" broken_assertion arm_clean_operand)
    string(JSON arm_broken_operand GET "${selected_row}" broken_assertion arm_broken_operand)
    string(JSON thumb_clean_operand GET "${selected_row}" broken_assertion thumb_clean_operand)
    string(JSON thumb_broken_operand GET "${selected_row}" broken_assertion thumb_broken_operand)
    string(JSON broken_case_id GET "${selected_row}" broken_assertion fail_case_id)
    if(NOT selected_positive_fixture STREQUAL "smoke" OR
       NOT selected_negative_fixture STREQUAL "smoke-broken" OR
       NOT "${selected_fail_case}" STREQUAL "${CASE_ID}" OR
       NOT "${broken_case_id}" STREQUAL "${CASE_ID}" OR
       NOT broken_source STREQUAL "diagnostics/armv4t/smoke.S GBL_BROKEN comparison operands")
      message(FATAL_ERROR "arm-diagnostics: case-variant-metadata-invalid: ${CASE_ID}")
    endif()
    file(READ "${source_root}/diagnostics/armv4t/smoke.S" smoke_source)
    foreach(expected_operand IN ITEMS
        "${arm_clean_operand}" "${arm_broken_operand}"
        "${thumb_clean_operand}" "${thumb_broken_operand}")
      string(FIND "${smoke_source}" "cmp r2, #${expected_operand}" operand_found)
      if(operand_found EQUAL -1)
        message(FATAL_ERROR
          "arm-diagnostics: broken-assertion-source-mismatch: expected operand ${expected_operand}")
      endif()
    endforeach()
    if("${arm_clean_operand}" STREQUAL "${arm_broken_operand}" OR
       "${thumb_clean_operand}" STREQUAL "${thumb_broken_operand}")
      message(FATAL_ERROR "arm-diagnostics: broken-assertion-not-distinct: ${CASE_ID}")
    endif()
  endif()
endif()

set(out_request "${OUT}")
cmake_path(ABSOLUTE_PATH out_request BASE_DIRECTORY "${source_root}"
  NORMALIZE OUTPUT_VARIABLE output_root)
set(spike_root "${source_root}/build/phase05-spike")
set(matrix_root "${source_root}/build/phase05-matrix")
if(MODE STREQUAL "all")
  cmake_path(IS_PREFIX matrix_root "${output_root}" NORMALIZE output_in_matrix_root)
  file(RELATIVE_PATH output_leaf "${matrix_root}" "${output_root}")
  if(NOT output_in_matrix_root OR NOT output_leaf STREQUAL "arm")
    message(FATAL_ERROR "arm-diagnostics: all-output-must-be-build/phase05-matrix/arm")
  endif()
elseif(MODE STREQUAL "case" AND MATRIX_CHILD STREQUAL "1")
  cmake_path(IS_PREFIX matrix_root "${output_root}" NORMALIZE output_in_matrix_root)
  file(RELATIVE_PATH output_leaf "${matrix_root}/arm/cases" "${output_root}")
  if(NOT output_in_matrix_root OR NOT output_leaf MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "arm-diagnostics: internal-case-output-invalid")
  endif()
else()
  cmake_path(IS_PREFIX spike_root "${output_root}" NORMALIZE output_in_spike_root)
  file(RELATIVE_PATH output_leaf "${spike_root}" "${output_root}")
  if(NOT output_in_spike_root OR "${output_root}" STREQUAL "${spike_root}" OR
     "${output_leaf}" MATCHES "/" OR "${output_leaf}" MATCHES "^\\.\\.")
    message(FATAL_ERROR "arm-diagnostics: output-outside-ignored-spike-root")
  endif()
endif()
if(IS_SYMLINK "${source_root}/build" OR IS_SYMLINK "${source_root}/build/phase05-spike" OR
   IS_SYMLINK "${source_root}/build/phase05-matrix" OR IS_SYMLINK "${output_root}")
  message(FATAL_ERROR "arm-diagnostics: output-root-symlink")
endif()
file(RELATIVE_PATH output_relative "${source_root}" "${output_root}")
if(output_relative MATCHES "^\\.\\.")
  message(FATAL_ERROR "arm-diagnostics: output-relative-path-escapes-source")
endif()

# Clear this task's isolated output root before any input/tool validation;
# an old ROM can never be returned as the result of a failed run.
file(REMOVE_RECURSE "${output_root}")
file(MAKE_DIRECTORY "${output_root}/proof" "${output_root}/roms"
  "${output_root}/proof/source-inputs")

set(source_commit "${SOURCE_COMMIT}")
if("${source_commit}" STREQUAL "")
  execute_process(
    COMMAND git -C "${source_root}" rev-parse HEAD
    RESULT_VARIABLE commit_rc OUTPUT_VARIABLE source_commit
    ERROR_VARIABLE commit_error OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${commit_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: source-revision-unavailable: ${commit_error}")
  endif()
endif()
string(LENGTH "${source_commit}" source_commit_length)
if(NOT source_commit_length EQUAL 40 OR NOT "${source_commit}" MATCHES "^[0-9a-f]+$")
  message(FATAL_ERROR "arm-diagnostics: invalid-source-revision: ${source_commit}")
endif()
execute_process(
  COMMAND git -C "${source_root}" rev-parse --verify "${source_commit}^{commit}"
  RESULT_VARIABLE source_commit_rc OUTPUT_VARIABLE resolved_source_commit
  ERROR_VARIABLE source_commit_error OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT "${source_commit_rc}" STREQUAL "0" OR
   NOT "${resolved_source_commit}" STREQUAL "${source_commit}")
  message(FATAL_ERROR "arm-diagnostics: source-revision-invalid: ${source_commit_error}")
endif()

if(NOT EXISTS "${inputs_file}" OR IS_DIRECTORY "${inputs_file}" OR IS_SYMLINK "${inputs_file}")
  message(FATAL_ERROR "arm-diagnostics: source-input-list-missing")
endif()
file(STRINGS "${inputs_file}" source_input_lines)
set(source_inputs "")
set(source_input_digest_material "")
foreach(input_line IN LISTS source_input_lines)
  string(STRIP "${input_line}" input_line)
  if("${input_line}" STREQUAL "" OR "${input_line}" MATCHES "^#")
    continue()
  endif()
  if(NOT "${input_line}" MATCHES "^[A-Za-z0-9._/-]+$" OR
     "${input_line}" MATCHES "(^|/)(\\.\\.?)(/|$)" OR
     "${input_line}" MATCHES "^/")
    message(FATAL_ERROR "arm-diagnostics: source-input-path-invalid: ${input_line}")
  endif()
  list(FIND source_inputs "${input_line}" input_seen)
  if(NOT input_seen EQUAL -1)
    message(FATAL_ERROR "arm-diagnostics: source-input-duplicate: ${input_line}")
  endif()
  list(APPEND source_inputs "${input_line}")
  set(input_path "${source_root}/${input_line}")
  if(NOT EXISTS "${input_path}" OR IS_DIRECTORY "${input_path}" OR IS_SYMLINK "${input_path}")
    message(FATAL_ERROR "arm-diagnostics: source-input-missing: ${input_line}")
  endif()
  get_filename_component(expected_input_path "${input_path}" ABSOLUTE)
  file(REAL_PATH "${input_path}" real_input_path)
  if(NOT "${real_input_path}" STREQUAL "${expected_input_path}")
    message(FATAL_ERROR "arm-diagnostics: source-input-symlink: ${input_line}")
  endif()
  file(SHA256 "${input_path}" current_input_sha)
  string(APPEND source_input_digest_material
    "${input_line}=${current_input_sha}\n")
  if(is_matrix_child)
    continue()
  endif()
  execute_process(
    COMMAND git -C "${source_root}" ls-files --error-unmatch -- "${input_line}"
    RESULT_VARIABLE tracked_rc OUTPUT_QUIET ERROR_QUIET)
  if(NOT "${tracked_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: source-input-untracked: ${input_line}")
  endif()
  execute_process(
    COMMAND git -C "${source_root}" diff --quiet "${source_commit}" -- "${input_line}"
    RESULT_VARIABLE tracked_diff_rc)
  if(NOT "${tracked_diff_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: source-input-mode-or-content-mismatch: ${input_line}")
  endif()
  set(tracked_copy "${output_root}/proof/source-inputs/${input_line}")
  get_filename_component(tracked_copy_dir "${tracked_copy}" DIRECTORY)
  file(MAKE_DIRECTORY "${tracked_copy_dir}")
  execute_process(
    COMMAND git -C "${source_root}" show "${source_commit}:${input_line}"
    RESULT_VARIABLE show_rc OUTPUT_FILE "${tracked_copy}" ERROR_VARIABLE show_error)
  if(NOT "${show_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: source-input-absent-from-revision: ${input_line}: ${show_error}")
  endif()
  file(SHA256 "${tracked_copy}" pinned_input_sha)
  if(NOT "${current_input_sha}" STREQUAL "${pinned_input_sha}")
    message(FATAL_ERROR "arm-diagnostics: source-input-mismatch: ${input_line}")
  endif()
endforeach()
string(SHA256 source_input_digest "${source_input_digest_material}")
if(is_matrix_child)
  set(matrix_parent_receipt "${matrix_root}/arm/proof/matrix-parent.json")
  if(NOT EXISTS "${matrix_parent_receipt}" OR
     IS_DIRECTORY "${matrix_parent_receipt}" OR IS_SYMLINK "${matrix_parent_receipt}")
    message(FATAL_ERROR "arm-diagnostics: matrix-parent-verification-missing")
  endif()
  file(READ "${matrix_parent_receipt}" matrix_parent_json)
  string(JSON matrix_parent_commit ERROR_VARIABLE parent_commit_error
    GET "${matrix_parent_json}" source_commit)
  string(JSON matrix_parent_digest ERROR_VARIABLE parent_digest_error
    GET "${matrix_parent_json}" source_input_sha256)
  if(NOT "${parent_commit_error}" STREQUAL "NOTFOUND" OR
     NOT "${parent_digest_error}" STREQUAL "NOTFOUND" OR
     NOT "${matrix_parent_commit}" STREQUAL "${source_commit}" OR
     NOT "${matrix_parent_digest}" STREQUAL "${source_input_digest}")
    message(FATAL_ERROR "arm-diagnostics: matrix-parent-input-identity-mismatch")
  endif()
endif()

foreach(required_input IN ITEMS
    cmake/BuildArmDiagnostics.cmake cmake/CheckArmCoverage.cmake
    cmake/FixGbaHeader.cmake cmake/GblJson.cmake
    diagnostics/armv4t/NOTICE diagnostics/armv4t/assertions.inc
    diagnostics/armv4t/cases-arm.S diagnostics/armv4t/cases.json
    diagnostics/armv4t/inputs.txt diagnostics/armv4t/linker.ld
    diagnostics/armv4t/smoke.S diagnostics/armv4t/startup.S
    scripts/fetch-toolchain.sh scripts/lib/signature-common.sh
    scripts/signature-scan.sh tests/signatures/allowlist.txt
    tests/signatures/bios.sha256 tests/signatures/logo.sha256
    toolchain/arm-none-eabi.lock)
  list(FIND source_inputs "${required_input}" required_input_found)
  if(required_input_found EQUAL -1)
    message(FATAL_ERROR "arm-diagnostics: source-input-unbound: ${required_input}")
  endif()
endforeach()
list(FIND source_inputs "CMakeLists.txt" consumer_cmake_bound)
if(NOT consumer_cmake_bound EQUAL -1)
  message(FATAL_ERROR "arm-diagnostics: unrelated-consumer-bound: CMakeLists.txt")
endif()
set(sorted_source_inputs "${source_inputs}")
list(SORT sorted_source_inputs)
if(NOT "${sorted_source_inputs}" STREQUAL "${source_inputs}")
  message(FATAL_ERROR "arm-diagnostics: source-input-list-not-sorted")
endif()
file(WRITE "${output_root}/proof/source-inputs.txt"
  "source.commit=${source_commit}\nsource.input_sha256=${source_input_digest}\ntracked inputs=${source_inputs}\n")
message(STATUS
  "arm-diagnostics: verified ${source_inputs} against source revision ${source_commit}")

foreach(input IN ITEMS
    "${source_root}/diagnostics/armv4t/startup.S"
    "${source_root}/diagnostics/armv4t/smoke.S"
    "${source_root}/diagnostics/armv4t/linker.ld"
    "${source_root}/cmake/FixGbaHeader.cmake")
  if(NOT EXISTS "${input}" OR IS_DIRECTORY "${input}" OR IS_SYMLINK "${input}")
    message(FATAL_ERROR "arm-diagnostics: missing-original-input: ${input}")
  endif()
endforeach()

set(toolchain_root_request "${TOOLCHAIN_ROOT}")
cmake_path(ABSOLUTE_PATH toolchain_root_request BASE_DIRECTORY "${source_root}"
  NORMALIZE OUTPUT_VARIABLE toolchain_root)
execute_process(COMMAND uname -s RESULT_VARIABLE uname_system_rc
  OUTPUT_VARIABLE uname_system OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND uname -m RESULT_VARIABLE uname_machine_rc
  OUTPUT_VARIABLE uname_machine OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT "${uname_system_rc}" STREQUAL "0" OR
   NOT "${uname_machine_rc}" STREQUAL "0")
  message(FATAL_ERROR "arm-diagnostics: host-detection-failed")
endif()
if(uname_system STREQUAL "Darwin" AND
   uname_machine MATCHES "^(arm64|aarch64)$")
  set(actual_host darwin-arm64)
elseif(uname_system STREQUAL "Linux" AND
       uname_machine MATCHES "^(x86_64|amd64)$")
  set(actual_host linux-x64)
elseif(uname_system STREQUAL "Linux" AND
       uname_machine MATCHES "^(aarch64|arm64)$")
  set(actual_host linux-arm64)
else()
  message(FATAL_ERROR "arm-diagnostics: unsupported-host: ${uname_system}/${uname_machine}")
endif()
if(DEFINED HOST AND NOT "${HOST}" STREQUAL "" AND NOT "${HOST}" STREQUAL "${actual_host}")
  message(FATAL_ERROR "arm-diagnostics: host-identity-mismatch: ${HOST}/${actual_host}")
endif()
set(HOST "${actual_host}")

set(toolchain_install "${toolchain_root}/arm-none-eabi/${HOST}-15.2.1-1.1/install.json")
if(is_matrix_child)
  string(JSON matrix_parent_host ERROR_VARIABLE parent_host_error
    GET "${matrix_parent_json}" host)
  string(JSON matrix_parent_toolchain_sha ERROR_VARIABLE parent_toolchain_error
    GET "${matrix_parent_json}" toolchain_install_sha256)
  if(NOT "${parent_host_error}" STREQUAL "NOTFOUND" OR
     NOT "${parent_toolchain_error}" STREQUAL "NOTFOUND" OR
     NOT "${matrix_parent_host}" STREQUAL "${HOST}" OR
     NOT EXISTS "${toolchain_install}" OR IS_SYMLINK "${toolchain_install}")
    message(FATAL_ERROR "arm-diagnostics: matrix-parent-toolchain-identity-mismatch")
  endif()
  file(SHA256 "${toolchain_install}" current_toolchain_sha)
  if(NOT "${current_toolchain_sha}" STREQUAL "${matrix_parent_toolchain_sha}")
    message(FATAL_ERROR "arm-diagnostics: matrix-parent-toolchain-changed")
  endif()
  file(WRITE "${output_root}/proof/toolchain-check.txt"
    "Reused parent-verified xPack GNU Arm Embedded 15.2.1-1.1 receipt: ${current_toolchain_sha}\n")
  file(COPY_FILE "${matrix_root}/arm/proof/tool-versions.txt"
    "${output_root}/proof/tool-versions.txt")
else()
  execute_process(
    COMMAND "${source_root}/scripts/fetch-toolchain.sh" --check
      --host "${HOST}" --root "${toolchain_root}"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE tool_check_rc
    OUTPUT_VARIABLE tool_check_output
    ERROR_VARIABLE tool_check_error)
  if(NOT "${tool_check_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: toolchain-receipt-invalid: ${tool_check_error}${tool_check_output}")
  endif()
  file(WRITE "${output_root}/proof/toolchain-check.txt"
    "${tool_check_output}${tool_check_error}")
endif()

set(tool_bin "${toolchain_root}/arm-none-eabi/${HOST}-15.2.1-1.1/bin")
foreach(tool IN ITEMS as ld objcopy readelf objdump nm)
  set(tool_path "${tool_bin}/arm-none-eabi-${tool}")
  if(NOT EXISTS "${tool_path}" OR IS_DIRECTORY "${tool_path}")
    message(FATAL_ERROR "arm-diagnostics: tool-missing-or-unusable: ${tool}")
  endif()
  if(NOT is_matrix_child)
    execute_process(COMMAND "${tool_path}" --version
      RESULT_VARIABLE version_rc OUTPUT_VARIABLE version_output
      ERROR_VARIABLE version_error)
    if(NOT "${version_rc}" STREQUAL "0")
      message(FATAL_ERROR "arm-diagnostics: tool-version-failed: ${tool}: ${version_error}")
    endif()
    string(REGEX MATCH "^[^\r\n]+" version_line "${version_output}")
    file(APPEND "${output_root}/proof/tool-versions.txt"
      "arm-none-eabi-${tool}: ${version_line}\n")
  endif()
endforeach()
if(NOT is_matrix_child)
  file(SHA256 "${toolchain_install}" toolchain_install_sha)
  set(matrix_parent_receipt "{}")
  string(JSON matrix_parent_receipt SET "${matrix_parent_receipt}" source_commit
    "\"${source_commit}\"")
  string(JSON matrix_parent_receipt SET "${matrix_parent_receipt}" source_input_sha256
    "\"${source_input_digest}\"")
  string(JSON matrix_parent_receipt SET "${matrix_parent_receipt}" toolchain_install_sha256
    "\"${toolchain_install_sha}\"")
  string(JSON matrix_parent_receipt SET "${matrix_parent_receipt}" host "\"${HOST}\"")
  file(WRITE "${output_root}/proof/matrix-parent.json"
    "${matrix_parent_receipt}\n")
endif()

function(require_v4t tool_path object_path output_file)
  execute_process(COMMAND "${tool_path}" -A "${object_path}"
    RESULT_VARIABLE readelf_rc OUTPUT_VARIABLE attributes
    ERROR_VARIABLE readelf_error)
  if(NOT "${readelf_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: readelf-failed: ${object_path}: ${readelf_error}")
  endif()
  string(REGEX MATCHALL "Tag_CPU_arch: v4T" v4t_tags "${attributes}")
  list(LENGTH v4t_tags v4t_count)
  if(NOT v4t_count EQUAL 1 OR NOT attributes MATCHES "Tag_THUMB_ISA_use: Thumb-1")
    message(FATAL_ERROR "arm-diagnostics: architecture-attribute-mismatch: ${object_path}")
  endif()
  file(WRITE "${output_file}" "${attributes}")
endfunction()

function(write_variant_receipt output_file variant broken
    startup_sha smoke_sha linker_sha rom_sha rom_bytes elapsed_seconds)
  set(receipt "{}")
  string(JSON receipt SET "${receipt}" format
    "\"gamebloke.arm-diagnostic-build/1\"")
  string(JSON receipt SET "${receipt}" host "\"${HOST}\"")
  string(JSON receipt SET "${receipt}" source_commit "\"${source_commit}\"")
  string(JSON receipt SET "${receipt}" release "\"15.2.1-1.1\"")
  string(JSON receipt SET "${receipt}" variant "\"${variant}\"")
  string(JSON receipt SET "${receipt}" broken "${broken}")
  string(JSON receipt SET "${receipt}" assembler_flags
    "\"common startup: -mcpu=arm7tdmi; smoke variant: -mcpu=arm7tdmi --defsym GBL_BROKEN=${broken}\"")
  string(JSON receipt SET "${receipt}" linker_flags
    "\"-T diagnostics/armv4t/linker.ld -Map project objects only\"")
  string(JSON receipt SET "${receipt}" startup_sha256 "\"${startup_sha}\"")
  string(JSON receipt SET "${receipt}" smoke_sha256 "\"${smoke_sha}\"")
  string(JSON receipt SET "${receipt}" linker_sha256 "\"${linker_sha}\"")
  string(JSON receipt SET "${receipt}" rom_sha256 "\"${rom_sha}\"")
  string(JSON receipt SET "${receipt}" rom_bytes "${rom_bytes}")
  string(JSON receipt SET "${receipt}" build_seconds "${elapsed_seconds}")
  string(JSON receipt SET "${receipt}" build_timing_resolution_seconds "1")
  file(WRITE "${output_file}" "${receipt}\n")
endfunction()

function(epoch_seconds output_name)
  execute_process(COMMAND date +%s RESULT_VARIABLE date_rc
    OUTPUT_VARIABLE epoch OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${date_rc}" STREQUAL "0" OR NOT epoch MATCHES "^[0-9]+$")
    message(FATAL_ERROR "arm-diagnostics: clock-read-failed")
  endif()
  set(${output_name} "${epoch}" PARENT_SCOPE)
endfunction()

if(MODE STREQUAL "case" AND selected_kind STREQUAL "smoke")
epoch_seconds(plan_started)
set(common_relative "${output_relative}/proof/common")
set(common_root "${output_root}/proof/common")
file(MAKE_DIRECTORY "${common_root}")
execute_process(
  COMMAND "${tool_bin}/arm-none-eabi-as" -mcpu=arm7tdmi
    -o "${common_relative}/startup.o" diagnostics/armv4t/startup.S
  WORKING_DIRECTORY "${source_root}"
  RESULT_VARIABLE common_startup_rc OUTPUT_VARIABLE common_startup_output
  ERROR_VARIABLE common_startup_error)
if(NOT "${common_startup_rc}" STREQUAL "0")
  message(FATAL_ERROR
    "arm-diagnostics: assembler-common-startup-failed: ${common_startup_error}${common_startup_output}")
endif()
require_v4t("${tool_bin}/arm-none-eabi-readelf"
  "${common_root}/startup.o" "${common_root}/startup.attributes.txt")
file(READ "${common_root}/startup.attributes.txt" common_startup_attributes)
file(WRITE "${common_root}/object-identity.txt"
  "Reused for the clean and assertion-broken member of each case pair.\n")
endif()

file(SHA256 "${source_root}/diagnostics/armv4t/startup.S" startup_sha)
file(SHA256 "${source_root}/diagnostics/armv4t/smoke.S" smoke_sha)
file(SHA256 "${source_root}/diagnostics/armv4t/linker.ld" linker_sha)

function(build_variant variant broken rom_name)
  set(variant_relative "${output_relative}/proof/${variant}")
  set(variant_root "${output_root}/proof/${variant}")
  file(MAKE_DIRECTORY "${variant_root}")
  # Retain the earlier per-variant proof path while linking the shared object.
  file(WRITE "${variant_root}/startup.attributes.txt"
    "${common_startup_attributes}")
  epoch_seconds(variant_started)

  execute_process(
    COMMAND "${tool_bin}/arm-none-eabi-as" -mcpu=arm7tdmi --defsym
      "GBL_BROKEN=${broken}" -o "${variant_relative}/smoke.o"
      diagnostics/armv4t/smoke.S
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE smoke_rc OUTPUT_VARIABLE smoke_output
    ERROR_VARIABLE smoke_error)
  if(NOT "${smoke_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: assembler-smoke-failed: ${smoke_error}${smoke_output}")
  endif()

  execute_process(
    COMMAND "${tool_bin}/arm-none-eabi-ld"
      -T diagnostics/armv4t/linker.ld
      "-Map=${variant_relative}/smoke.map"
      -o "${variant_relative}/smoke.elf"
      "${common_relative}/startup.o" "${variant_relative}/smoke.o"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE link_rc OUTPUT_VARIABLE link_output
    ERROR_VARIABLE link_error)
  if(NOT "${link_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: linker-failed: ${link_error}${link_output}")
  endif()

  set(expected_objects
    "${common_relative}/startup.o" "${variant_relative}/smoke.o")
  set(actual_objects "")
  set(synthetic_linker_stubs 0)
  file(STRINGS "${variant_root}/smoke.map" map_lines)
  foreach(line IN LISTS map_lines)
    if(line MATCHES "^LOAD ")
      string(REGEX REPLACE "^LOAD +" "" loaded "${line}")
      if(loaded STREQUAL "linker stubs")
        math(EXPR synthetic_linker_stubs "${synthetic_linker_stubs} + 1")
      else()
        list(APPEND actual_objects "${loaded}")
      endif()
    endif()
  endforeach()
  list(LENGTH actual_objects actual_count)
  list(LENGTH expected_objects expected_count)
  if(NOT actual_count EQUAL expected_count)
    message(FATAL_ERROR "arm-diagnostics: map-input-count-mismatch: ${variant}")
  endif()
  foreach(expected IN LISTS expected_objects)
    list(FIND actual_objects "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "arm-diagnostics: map-input-missing: ${expected}")
    endif()
  endforeach()
  string(TOLOWER "${map_lines}" map_lower)
  if(map_lower MATCHES "(libgcc\\.a|libc\\.a|newlib|crt[0-9a-z_.-]*\\.o)")
    message(FATAL_ERROR "arm-diagnostics: runtime-input-in-map: ${variant}")
  endif()
  set(map_inputs "")
  foreach(expected IN LISTS expected_objects)
    string(APPEND map_inputs "project-object ${expected}\n")
  endforeach()
  string(APPEND map_inputs
    "linker-generated linker stubs: ${synthetic_linker_stubs}\n")
  file(WRITE "${variant_root}/map-inputs.txt" "${map_inputs}")

  require_v4t("${tool_bin}/arm-none-eabi-readelf"
    "${variant_root}/smoke.o" "${variant_root}/smoke.attributes.txt")
  require_v4t("${tool_bin}/arm-none-eabi-readelf"
    "${variant_root}/smoke.elf" "${variant_root}/elf.attributes.txt")

  execute_process(
    COMMAND "${tool_bin}/arm-none-eabi-readelf" -SW
      "${variant_root}/smoke.elf"
    RESULT_VARIABLE sections_rc OUTPUT_VARIABLE section_output
    ERROR_VARIABLE sections_error)
  if(NOT "${sections_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: section-inventory-failed: ${sections_error}")
  endif()
  file(WRITE "${variant_root}/elf-sections.txt" "${section_output}")
  file(STRINGS "${variant_root}/elf-sections.txt" section_lines)
  set(executable_sections "")
  foreach(line IN LISTS section_lines)
    if(line MATCHES "^  \\[[ ]*[0-9]+\\]" AND line MATCHES " [A-Z]*X[ ]")
      string(REGEX REPLACE "^  \\[[ ]*[0-9]+\\][ ]+" "" section_tail "${line}")
      string(REGEX MATCH "^[^ ]+" section_name "${section_tail}")
      list(APPEND executable_sections "${section_name}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES executable_sections)
  list(SORT executable_sections)
  set(expected_executable_sections ".gba_header" ".text")
  if(NOT "${executable_sections}" STREQUAL "${expected_executable_sections}")
    message(FATAL_ERROR
      "arm-diagnostics: executable-section-set-mismatch: ${variant}: ${executable_sections}")
  endif()

  execute_process(
    COMMAND "${tool_bin}/arm-none-eabi-objdump" -d
      -j .gba_header -j .text
      "${variant_root}/smoke.elf"
    RESULT_VARIABLE disasm_rc OUTPUT_VARIABLE disassembly
    ERROR_VARIABLE disasm_error)
  if(NOT "${disasm_rc}" STREQUAL "0" OR "${disassembly}" STREQUAL "" OR
     NOT "${disassembly}" MATCHES "Disassembly of section [.]gba_header" OR
     NOT "${disassembly}" MATCHES "Disassembly of section [.]text" OR
     NOT "${disassembly}" MATCHES "_start" OR
     NOT "${disassembly}" MATCHES "bx")
    message(FATAL_ERROR "arm-diagnostics: executable-disassembly-missing: ${disasm_error}")
  endif()
  file(WRITE "${variant_root}/executable.disasm.txt" "${disassembly}")

  execute_process(
    COMMAND "${tool_bin}/arm-none-eabi-nm" -u "${variant_root}/smoke.elf"
    RESULT_VARIABLE nm_rc OUTPUT_VARIABLE undefined_symbols
    ERROR_VARIABLE nm_error)
  string(STRIP "${undefined_symbols}" undefined_symbols)
  if(NOT "${nm_rc}" STREQUAL "0" OR NOT "${undefined_symbols}" STREQUAL "")
    message(FATAL_ERROR "arm-diagnostics: unresolved-symbols: ${nm_error}${undefined_symbols}")
  endif()
  file(WRITE "${variant_root}/unresolved-symbols.txt"
    "No unresolved symbols. Direct ld inputs are captured in map-inputs.txt.\n")

  set(raw_rom "${variant_root}/rom.unfixed")
  set(rom_relative "${output_relative}/roms/${rom_name}")
  execute_process(
    COMMAND "${tool_bin}/arm-none-eabi-objcopy" -O binary
      "${variant_root}/smoke.elf" "${raw_rom}"
    RESULT_VARIABLE objcopy_rc OUTPUT_VARIABLE objcopy_output
    ERROR_VARIABLE objcopy_error)
  if(NOT "${objcopy_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: objcopy-failed: ${objcopy_error}${objcopy_output}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DINPUT=${raw_rom}" "-DOUTPUT=${output_root}/roms/${rom_name}"
      -P "${source_root}/cmake/FixGbaHeader.cmake"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE header_rc OUTPUT_VARIABLE header_output
    ERROR_VARIABLE header_error)
  if(NOT "${header_rc}" STREQUAL "0")
    message(FATAL_ERROR "arm-diagnostics: header-fix-failed: ${header_error}${header_output}")
  endif()
  file(WRITE "${variant_root}/header-fix.txt" "${header_output}${header_error}")

  file(SHA256 "${output_root}/roms/${rom_name}" rom_sha)
  file(SIZE "${output_root}/roms/${rom_name}" rom_bytes)
  epoch_seconds(variant_finished)
  math(EXPR variant_seconds "${variant_finished} - ${variant_started}")
  set(receipt_path "${variant_root}/build-receipt.json")
  write_variant_receipt("${receipt_path}" "${variant}" "${broken}"
    "${startup_sha}" "${smoke_sha}" "${linker_sha}" "${rom_sha}"
    "${rom_bytes}" "${variant_seconds}")
  message(STATUS "arm-diagnostics: built ${rom_name} host=${HOST} bytes=${rom_bytes} sha256=${rom_sha} seconds=${variant_seconds}")
endfunction()

if(MODE STREQUAL "all")
  file(READ "${matrix_file}" matrix_json)
  string(JSON case_count LENGTH "${matrix_json}" cases)
  math(EXPR case_last "${case_count}-1")
  set(semantic_count 0)
  set(stop_count 0)
  set(output_names "")
  set(allowlist "")
  foreach(index RANGE 0 ${case_last})
    string(JSON row_json GET "${matrix_json}" cases ${index})
    string(JSON row_id GET "${row_json}" id)
    string(JSON row_isa GET "${row_json}" isa)
    string(JSON row_state GET "${row_json}" coverage_state)
    string(JSON source_kind ERROR_VARIABLE source_kind_error GET
      "${row_json}" source_test kind)
    if(NOT "${source_kind_error}" STREQUAL "NOTFOUND")
      continue()
    endif()
    if(NOT source_kind MATCHES "^(semantic|guest-stop)$")
      continue()
    endif()
    if(NOT row_isa STREQUAL "arm" OR NOT row_state STREQUAL "pending")
      message(FATAL_ERROR
        "arm-diagnostics: source-row-not-runnable: id=${row_id} isa=${row_isa} state=${row_state}")
    endif()
    set(child_output "${output_root}/cases/${row_id}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}"
        "-DOUT=${child_output}" "-DTOOLCHAIN_ROOT=${toolchain_root}"
        -DMODE=case -DISA=arm -DMATRIX_CHILD=1
        "-DMATRIX_CASE_INDEX=${index}"
        "-DCASE_ID=${row_id}" "-DSOURCE_COMMIT=${source_commit}"
        -DHOST=${HOST} -P "${source_root}/cmake/BuildArmDiagnostics.cmake"
      WORKING_DIRECTORY "${source_root}"
      RESULT_VARIABLE child_rc OUTPUT_VARIABLE child_output_text
      ERROR_VARIABLE child_error)
    if(NOT "${child_rc}" STREQUAL "0")
      message(FATAL_ERROR
        "arm-diagnostics: matrix-child-failed: id=${row_id}: ${child_error}${child_output_text}")
    endif()
    set(child_receipt "${child_output}/proof/build-receipt.json")
    if(NOT EXISTS "${child_receipt}" OR IS_DIRECTORY "${child_receipt}" OR
       IS_SYMLINK "${child_receipt}")
      message(FATAL_ERROR "arm-diagnostics: matrix-child-receipt-missing: ${row_id}")
    endif()
    file(READ "${child_receipt}" child_json)
    string(JSON child_id ERROR_VARIABLE child_id_error GET "${child_json}" case_id)
    string(JSON child_names_length ERROR_VARIABLE child_names_error
      LENGTH "${child_json}" output_names)
    if(NOT "${child_id_error}" STREQUAL "NOTFOUND" OR
       NOT "${child_names_error}" STREQUAL "NOTFOUND" OR
       NOT "${child_id}" STREQUAL "${row_id}")
      message(FATAL_ERROR "arm-diagnostics: matrix-child-identity-invalid: ${row_id}")
    endif()
    if(source_kind STREQUAL "semantic")
      string(JSON child_semantic ERROR_VARIABLE child_semantic_error GET
        "${child_json}" semantic_pairs)
      string(JSON child_stops ERROR_VARIABLE child_stops_error GET
        "${child_json}" stop_witnesses)
      if(NOT "${child_semantic_error}" STREQUAL "NOTFOUND" OR
         NOT "${child_stops_error}" STREQUAL "NOTFOUND" OR
         NOT child_semantic EQUAL 1 OR NOT child_stops EQUAL 0 OR
         NOT child_names_length EQUAL 2)
        message(FATAL_ERROR "arm-diagnostics: semantic-child-receipt-invalid: ${row_id}")
      endif()
      math(EXPR semantic_count "${semantic_count}+1")
    else()
      string(JSON child_semantic ERROR_VARIABLE child_semantic_error GET
        "${child_json}" semantic_pairs)
      string(JSON child_stops ERROR_VARIABLE child_stops_error GET
        "${child_json}" stop_witnesses)
      string(JSON expected_reason ERROR_VARIABLE reason_error GET
        "${row_json}" source_test expected_reason)
      string(JSON receipt_reason ERROR_VARIABLE receipt_reason_error GET
        "${child_json}" expected_reason)
      if(NOT "${child_semantic_error}" STREQUAL "NOTFOUND" OR
         NOT "${child_stops_error}" STREQUAL "NOTFOUND" OR
         NOT "${reason_error}" STREQUAL "NOTFOUND" OR
         NOT "${receipt_reason_error}" STREQUAL "NOTFOUND" OR
         NOT child_semantic EQUAL 0 OR NOT child_stops EQUAL 1 OR
         NOT child_names_length EQUAL 1 OR
         NOT "${expected_reason}" STREQUAL "${receipt_reason}")
        message(FATAL_ERROR "arm-diagnostics: stop-child-receipt-invalid: ${row_id}")
      endif()
      math(EXPR stop_count "${stop_count}+1")
    endif()
    math(EXPR child_names_last "${child_names_length}-1")
    foreach(name_index RANGE 0 ${child_names_last})
      string(JSON rom_name GET "${child_json}" output_names ${name_index})
      list(FIND output_names "${rom_name}" name_seen)
      if(NOT name_seen EQUAL -1)
        message(FATAL_ERROR "arm-diagnostics: duplicate-output-name: ${rom_name}")
      endif()
      set(child_rom "${child_output}/roms/${rom_name}")
      if(NOT EXISTS "${child_rom}" OR IS_DIRECTORY "${child_rom}" OR
         IS_SYMLINK "${child_rom}")
        message(FATAL_ERROR "arm-diagnostics: matrix-child-rom-missing: ${rom_name}")
      endif()
      file(COPY_FILE "${child_rom}" "${output_root}/roms/${rom_name}")
      list(APPEND output_names "${rom_name}")
      string(APPEND allowlist "${rom_name}\n")
    endforeach()
  endforeach()
  if(NOT semantic_count EQUAL 244 OR NOT stop_count EQUAL 4)
    message(FATAL_ERROR
      "arm-diagnostics: matrix-count-mismatch: semantic=${semantic_count} stops=${stop_count}")
  endif()
  list(LENGTH output_names output_count)
  if(NOT output_count EQUAL 492)
    message(FATAL_ERROR "arm-diagnostics: matrix-output-count-mismatch: ${output_count}")
  endif()
  set(sorted_names "${output_names}")
  list(SORT sorted_names)
  set(output_names "${sorted_names}")
  set(allowlist "")
  foreach(name IN LISTS output_names)
    string(APPEND allowlist "${name}\n")
  endforeach()
  file(WRITE "${output_root}/proof/signature-allowlist.txt" "${allowlist}")
  execute_process(
    COMMAND sh "${source_root}/scripts/signature-scan.sh" --dir
      "${output_root}/roms" --allowlist
      "${output_root}/proof/signature-allowlist.txt"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE matrix_signature_rc OUTPUT_VARIABLE matrix_signature_out
    ERROR_VARIABLE matrix_signature_error)
  if(NOT "${matrix_signature_rc}" STREQUAL "0" OR
     NOT matrix_signature_out MATCHES "signature: ok")
    message(FATAL_ERROR
      "arm-diagnostics: matrix-signature-scan-failed: ${matrix_signature_error}${matrix_signature_out}")
  endif()
  file(WRITE "${output_root}/proof/signature-scan.txt"
    "${matrix_signature_out}${matrix_signature_error}")
  set(names_json "[]")
  set(name_index 0)
  foreach(name IN LISTS output_names)
    string(JSON names_json SET "${names_json}" ${name_index} "\"${name}\"")
    math(EXPR name_index "${name_index}+1")
  endforeach()
  set(matrix_receipt "{}")
  string(JSON matrix_receipt SET "${matrix_receipt}" format
    "\"gamebloke.arm-diagnostic-matrix/1\"")
  string(JSON matrix_receipt SET "${matrix_receipt}" isa "\"arm\"")
  string(JSON matrix_receipt SET "${matrix_receipt}" source_commit
    "\"${source_commit}\"")
  string(JSON matrix_receipt SET "${matrix_receipt}" host "\"${HOST}\"")
  string(JSON matrix_receipt SET "${matrix_receipt}" semantic_pairs
    "${semantic_count}")
  string(JSON matrix_receipt SET "${matrix_receipt}" stop_witnesses
    "${stop_count}")
  string(JSON matrix_receipt SET "${matrix_receipt}" output_names "${names_json}")
  string(JSON matrix_receipt SET "${matrix_receipt}" signature_scan "\"passed\"")
  file(WRITE "${output_root}/proof/build-receipt.json"
    "${matrix_receipt}\n")
  message(STATUS
    "arm-diagnostics: built all ARM outputs semantic=${semantic_count} stops=${stop_count} names=${output_count}")
  return()
endif()

if(MODE STREQUAL "case" AND selected_kind MATCHES "^(semantic|guest-stop)$")
  math(EXPR case_id_hex "${CASE_ID}" OUTPUT_FORMAT HEXADECIMAL)
  if(selected_kind STREQUAL "semantic")
    string(JSON expected_value GET "${selected_row}" source_test
      independent_expectation expected_value)
    string(JSON broken_expected GET "${selected_row}" source_test
      independent_expectation broken_expected)
    if("${expected_value}" STREQUAL "${broken_expected}" OR
       NOT "${selected_source_program}" MATCHES "GBL_ASSERT_(EQ|MASKED)")
      message(FATAL_ERROR "arm-diagnostics: arm-case-assertion-invalid: ${CASE_ID}")
    endif()
    set(variant_labels "arm-${CASE_ID}" "arm-${CASE_ID}-broken")
    set(rom_names "arm-${CASE_ID}.gba" "arm-${CASE_ID}-broken.gba")
    set(broken_values 0 1)
  else()
    string(JSON stop_operation GET "${selected_row}" source_test operation)
    string(JSON stop_reason GET "${selected_row}" source_test expected_reason)
    string(JSON witness_id GET "${selected_row}" source_test witness_id)
    if(NOT "${witness_id}" STREQUAL "${CASE_ID}" OR
       NOT "${stop_reason}" MATCHES "^(no-bios-swi|no-bios-undefined|no-bios-coprocessor|unpredictable-instruction)$" OR
       NOT "${selected_source_program}" MATCHES "0x47424C4B" OR
       NOT "${selected_source_program}" MATCHES "${stop_operation}" OR
       NOT "${selected_source_program}" MATCHES "str r2,\\[r1,#4\\]" OR
       NOT "${selected_source_program}" MATCHES "str r2,\\[r1,#12\\]")
      message(FATAL_ERROR "arm-diagnostics: guest-stop-source-invalid: ${CASE_ID}")
    endif()
    set(variant_labels "arm-stop-${CASE_ID}")
    set(rom_names "arm-stop-${CASE_ID}.gba")
    set(broken_values 0)
  endif()
  set(variant_index 0)
  foreach(variant IN LISTS variant_labels)
    list(GET rom_names ${variant_index} rom_name)
    list(GET broken_values ${variant_index} broken)
    set(variant_relative "${output_relative}/proof/${variant}")
    set(variant_root "${output_root}/proof/${variant}")
    file(MAKE_DIRECTORY "${variant_root}")
    file(WRITE "${variant_root}/arm-case.inc" "${selected_source_program}")

    file(READ "${source_root}/diagnostics/armv4t/startup.S" startup_source)
    string(REPLACE "0x00005001" "${case_id_hex}" case_startup_source
      "${startup_source}")
    if("${case_startup_source}" STREQUAL "${startup_source}")
      message(FATAL_ERROR "arm-diagnostics: case-id-mailbox-patch-missing")
    endif()
    file(WRITE "${variant_root}/startup.S" "${case_startup_source}")
    file(SHA256 "${variant_root}/startup.S" derived_startup_sha)
    file(SHA256 "${source_root}/diagnostics/armv4t/cases-arm.S" case_source_sha)
    file(SHA256 "${variant_root}/arm-case.inc" generated_case_sha)

    execute_process(
      COMMAND "${tool_bin}/arm-none-eabi-as" -mcpu=arm7tdmi
        -o "${variant_relative}/startup.o" "${variant_relative}/startup.S"
      WORKING_DIRECTORY "${source_root}"
      RESULT_VARIABLE case_startup_rc OUTPUT_VARIABLE case_startup_output
      ERROR_VARIABLE case_startup_error)
    if(NOT "${case_startup_rc}" STREQUAL "0")
      message(FATAL_ERROR "arm-diagnostics: arm-case-startup-failed: ${case_startup_error}${case_startup_output}")
    endif()
    execute_process(
      COMMAND "${tool_bin}/arm-none-eabi-as" -mcpu=arm7tdmi
        -I "${variant_root}" --defsym "GBL_BROKEN=${broken}"
        -o "${variant_relative}/case.o" diagnostics/armv4t/cases-arm.S
      WORKING_DIRECTORY "${source_root}"
      RESULT_VARIABLE case_asm_rc OUTPUT_VARIABLE case_asm_output
      ERROR_VARIABLE case_asm_error)
    if(NOT "${case_asm_rc}" STREQUAL "0")
      message(FATAL_ERROR "arm-diagnostics: arm-case-assembler-failed: ${case_asm_error}${case_asm_output}")
    endif()
    execute_process(
      COMMAND "${tool_bin}/arm-none-eabi-ld" -T diagnostics/armv4t/linker.ld
        "-Map=${variant_relative}/case.map" -o "${variant_relative}/case.elf"
        "${variant_relative}/startup.o" "${variant_relative}/case.o"
      WORKING_DIRECTORY "${source_root}"
      RESULT_VARIABLE case_link_rc OUTPUT_VARIABLE case_link_output
      ERROR_VARIABLE case_link_error)
    if(NOT "${case_link_rc}" STREQUAL "0")
      message(FATAL_ERROR "arm-diagnostics: arm-case-link-failed: ${case_link_error}${case_link_output}")
    endif()

    file(STRINGS "${variant_root}/case.map" case_map_lines)
    set(case_map_objects "")
    foreach(line IN LISTS case_map_lines)
      if(line MATCHES "^LOAD ")
        string(REGEX REPLACE "^LOAD +" "" loaded "${line}")
        if(NOT loaded STREQUAL "linker stubs")
          list(APPEND case_map_objects "${loaded}")
        endif()
      endif()
    endforeach()
    set(expected_case_objects "${variant_relative}/startup.o" "${variant_relative}/case.o")
    if(NOT "${case_map_objects}" STREQUAL "${expected_case_objects}")
      message(FATAL_ERROR "arm-diagnostics: arm-case-map-inputs-invalid: ${variant}")
    endif()
    string(TOLOWER "${case_map_lines}" case_map_lower)
    if(case_map_lower MATCHES "(libgcc[.]a|libc[.]a|newlib|crt[0-9a-z_.-]*[.]o)")
      message(FATAL_ERROR "arm-diagnostics: arm-case-runtime-input: ${variant}")
    endif()
    file(WRITE "${variant_root}/map-inputs.txt"
      "project-object ${variant_relative}/startup.o\nproject-object ${variant_relative}/case.o\n")

    require_v4t("${tool_bin}/arm-none-eabi-readelf"
      "${variant_root}/startup.o" "${variant_root}/startup.attributes.txt")
    require_v4t("${tool_bin}/arm-none-eabi-readelf"
      "${variant_root}/case.o" "${variant_root}/case.attributes.txt")
    require_v4t("${tool_bin}/arm-none-eabi-readelf"
      "${variant_root}/case.elf" "${variant_root}/elf.attributes.txt")
    execute_process(
      COMMAND "${tool_bin}/arm-none-eabi-objdump" -d -j .gba_header -j .text
        "${variant_root}/case.elf"
      RESULT_VARIABLE case_disasm_rc OUTPUT_VARIABLE case_disassembly
      ERROR_VARIABLE case_disasm_error)
    if(NOT "${case_disasm_rc}" STREQUAL "0" OR
       NOT "${case_disassembly}" MATCHES "diagnostic_main" OR
       NOT "${case_disassembly}" MATCHES "bx")
      message(FATAL_ERROR "arm-diagnostics: arm-case-disassembly-invalid: ${case_disasm_error}")
    endif()
    file(WRITE "${variant_root}/case.disasm.txt" "${case_disassembly}")
    execute_process(
      COMMAND "${tool_bin}/arm-none-eabi-objcopy" -O binary
        "${variant_root}/case.elf" "${variant_root}/case.unfixed"
      RESULT_VARIABLE case_copy_rc OUTPUT_VARIABLE case_copy_output
      ERROR_VARIABLE case_copy_error)
    if(NOT "${case_copy_rc}" STREQUAL "0")
      message(FATAL_ERROR "arm-diagnostics: arm-case-objcopy-failed: ${case_copy_error}${case_copy_output}")
    endif()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" "-DINPUT=${variant_root}/case.unfixed"
        "-DOUTPUT=${output_root}/roms/${rom_name}"
        -P "${source_root}/cmake/FixGbaHeader.cmake"
      WORKING_DIRECTORY "${source_root}"
      RESULT_VARIABLE case_header_rc OUTPUT_VARIABLE case_header_output
      ERROR_VARIABLE case_header_error)
    if(NOT "${case_header_rc}" STREQUAL "0")
      message(FATAL_ERROR "arm-diagnostics: arm-case-header-fix-failed: ${case_header_error}${case_header_output}")
    endif()
    file(SHA256 "${output_root}/roms/${rom_name}" rom_sha)
    file(SIZE "${output_root}/roms/${rom_name}" rom_bytes)
    set(case_receipt "{}")
    string(JSON case_receipt SET "${case_receipt}" format
      "\"gamebloke.arm-diagnostic-case/1\"")
    string(JSON case_receipt SET "${case_receipt}" source_commit
      "\"${source_commit}\"")
    string(JSON case_receipt SET "${case_receipt}" case_id "${CASE_ID}")
    string(JSON case_receipt SET "${case_receipt}" broken "${broken}")
    string(JSON case_receipt SET "${case_receipt}" isa "\"arm\"")
    string(JSON case_receipt SET "${case_receipt}" source_kind
      "\"${selected_kind}\"")
    string(JSON case_receipt SET "${case_receipt}" startup_source_sha256
      "\"${startup_sha}\"")
    string(JSON case_receipt SET "${case_receipt}" derived_startup_sha256
      "\"${derived_startup_sha}\"")
    string(JSON case_receipt SET "${case_receipt}" source_sha256
      "\"${case_source_sha}\"")
    string(JSON case_receipt SET "${case_receipt}" program_sha256
      "\"${generated_case_sha}\"")
    string(JSON case_receipt SET "${case_receipt}" rom_sha256 "\"${rom_sha}\"")
    string(JSON case_receipt SET "${case_receipt}" rom_bytes "${rom_bytes}")
    if(selected_kind STREQUAL "guest-stop")
      string(JSON case_receipt SET "${case_receipt}" expected_reason
        "\"${stop_reason}\"")
      string(JSON case_receipt SET "${case_receipt}" witness_id "${witness_id}")
    endif()
    file(WRITE "${variant_root}/build-receipt.json" "${case_receipt}\n")
    math(EXPR variant_index "${variant_index}+1")
  endforeach()

  set(case_allowlist "")
  foreach(rom_name IN LISTS rom_names)
    string(APPEND case_allowlist "${rom_name}\n")
  endforeach()
  file(WRITE "${output_root}/proof/signature-allowlist.txt" "${case_allowlist}")
  execute_process(
    COMMAND sh "${source_root}/scripts/signature-scan.sh" --dir
      "${output_root}/roms" --allowlist
      "${output_root}/proof/signature-allowlist.txt"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE case_signature_rc OUTPUT_VARIABLE case_signature_output
    ERROR_VARIABLE case_signature_error)
  if(NOT "${case_signature_rc}" STREQUAL "0" OR
     NOT case_signature_output MATCHES "signature: ok")
    message(FATAL_ERROR "arm-diagnostics: arm-case-signature-scan-failed: ${case_signature_error}${case_signature_output}")
  endif()
  file(WRITE "${output_root}/proof/signature-scan.txt"
    "${case_signature_output}${case_signature_error}")
  file(GLOB case_rom_entries LIST_DIRECTORIES TRUE "${output_root}/roms/*")
  list(SORT case_rom_entries)
  set(expected_case_roms "")
  foreach(rom_name IN LISTS rom_names)
    list(APPEND expected_case_roms "${output_root}/roms/${rom_name}")
  endforeach()
  list(SORT expected_case_roms)
  if(NOT "${case_rom_entries}" STREQUAL "${expected_case_roms}")
    message(FATAL_ERROR "arm-diagnostics: arm-case-rom-name-set-mismatch")
  endif()
  set(case_summary "{}")
  if(selected_kind STREQUAL "semantic")
    set(case_summary_format "gamebloke.arm-diagnostic-case-pair/1")
    set(case_semantic_count 1)
    set(case_stop_count 0)
  else()
    set(case_summary_format "gamebloke.arm-diagnostic-stop-witness/1")
    set(case_semantic_count 0)
    set(case_stop_count 1)
  endif()
  string(JSON case_summary SET "${case_summary}" format
    "\"${case_summary_format}\"")
  string(JSON case_summary SET "${case_summary}" source_commit
    "\"${source_commit}\"")
  string(JSON case_summary SET "${case_summary}" case_id "${CASE_ID}")
  string(JSON case_summary SET "${case_summary}" isa "\"arm\"")
  string(JSON case_summary SET "${case_summary}" semantic_pairs
    "${case_semantic_count}")
  string(JSON case_summary SET "${case_summary}" stop_witnesses
    "${case_stop_count}")
  set(case_names_json "[]")
  set(case_name_index 0)
  foreach(rom_name IN LISTS rom_names)
    string(JSON case_names_json SET "${case_names_json}"
      ${case_name_index} "\"${rom_name}\"")
    math(EXPR case_name_index "${case_name_index}+1")
  endforeach()
  string(JSON case_summary SET "${case_summary}" output_names
    "${case_names_json}")
  if(selected_kind STREQUAL "guest-stop")
    string(JSON case_summary SET "${case_summary}" expected_reason
      "\"${stop_reason}\"")
    string(JSON case_summary SET "${case_summary}" witness_id "${witness_id}")
  endif()
  file(WRITE "${output_root}/proof/build-receipt.json" "${case_summary}\n")
  message(STATUS "arm-diagnostics: built selected ARM ${selected_kind} id=${CASE_ID}")
  return()
endif()

if(MODE STREQUAL "case")
build_variant(smoke 0 smoke.gba)
build_variant(smoke-broken 1 smoke-broken.gba)

file(WRITE "${output_root}/proof/signature-allowlist.txt"
  "smoke.gba\nsmoke-broken.gba\n")
execute_process(
  COMMAND sh "${source_root}/scripts/signature-scan.sh" --dir
    "${output_root}/roms" --allowlist
    "${output_root}/proof/signature-allowlist.txt"
  WORKING_DIRECTORY "${source_root}"
  RESULT_VARIABLE signature_rc OUTPUT_VARIABLE signature_output
  ERROR_VARIABLE signature_error)
if(NOT "${signature_rc}" STREQUAL "0" OR
   NOT signature_output MATCHES "signature: ok")
  message(FATAL_ERROR "arm-diagnostics: signature-scan-failed: ${signature_error}${signature_output}")
endif()
file(WRITE "${output_root}/proof/signature-scan.txt"
  "${signature_output}${signature_error}")

file(GLOB rom_entries LIST_DIRECTORIES TRUE "${output_root}/roms/*")
list(SORT rom_entries)
set(expected_roms "${output_root}/roms/smoke-broken.gba"
  "${output_root}/roms/smoke.gba")
if(NOT "${rom_entries}" STREQUAL "${expected_roms}")
  message(FATAL_ERROR "arm-diagnostics: rom-name-set-mismatch")
endif()

file(READ "${toolchain_root}/arm-none-eabi/${HOST}-15.2.1-1.1/install.json"
  tool_receipt)
string(JSON tool_archive_bytes GET "${tool_receipt}" archive_bytes)
string(JSON tool_archive_sha GET "${tool_receipt}" archive_sha256)
string(JSON tool_download_seconds GET "${tool_receipt}" download_seconds)
string(JSON tool_extract_seconds GET "${tool_receipt}" extract_seconds)
epoch_seconds(plan_finished)
math(EXPR plan_seconds "${plan_finished} - ${plan_started}")
set(summary "{}")
string(JSON summary SET "${summary}" format
  "\"gamebloke.arm-diagnostic-run/1\"")
string(JSON summary SET "${summary}" mode "\"${MODE}\"")
string(JSON summary SET "${summary}" host "\"${HOST}\"")
string(JSON summary SET "${summary}" source_commit "\"${source_commit}\"")
string(JSON summary SET "${summary}" uname_system "\"${uname_system}\"")
string(JSON summary SET "${summary}" uname_machine "\"${uname_machine}\"")
string(JSON summary SET "${summary}" toolchain_release "\"15.2.1-1.1\"")
string(JSON summary SET "${summary}" toolchain_archive_bytes "${tool_archive_bytes}")
string(JSON summary SET "${summary}" toolchain_archive_sha256 "\"${tool_archive_sha}\"")
string(JSON summary SET "${summary}" first_download_seconds "${tool_download_seconds}")
string(JSON summary SET "${summary}" first_extract_seconds "${tool_extract_seconds}")
string(JSON summary SET "${summary}" smoke_build_seconds "${plan_seconds}")
string(JSON summary SET "${summary}" smoke_build_timing_resolution_seconds "1")
string(JSON summary SET "${summary}" source "\"diagnostics/armv4t\"")
string(JSON summary SET "${summary}" toolchain_check "\"passed\"")
string(JSON summary SET "${summary}" signature_scan "\"passed\"")
string(JSON summary SET "${summary}" output_names "[\"smoke.gba\",\"smoke-broken.gba\"]")
file(WRITE "${output_root}/proof/build-receipt.json" "${summary}\n")

message(STATUS "arm-diagnostics: smoke build receipt ${output_root}/proof/build-receipt.json")
endif()
