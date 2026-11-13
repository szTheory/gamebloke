cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS CASE SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "coverage-case: missing-${required}")
  endif()
endforeach()
if(NOT CASE MATCHES "^(ledger-controls|thumb-ledger-controls|arm-matrix-build)$")
  message(FATAL_ERROR "coverage-case: unsupported-case")
endif()

get_filename_component(source_root "${SRC}" ABSOLUTE)
get_filename_component(binary_root "${BIN}" ABSOLUTE)
set(matrix "${source_root}/diagnostics/armv4t/cases.json")
set(checker "${source_root}/cmake/CheckArmCoverage.cmake")
set(builder "${source_root}/cmake/BuildArmDiagnostics.cmake")
set(inputs "${source_root}/diagnostics/armv4t/inputs.txt")
include("${source_root}/cmake/GblJson.cmake")
file(READ "${matrix}" matrix_json)
string(JSON inventory_rows LENGTH "${matrix_json}" cases)
math(EXPR inventory_last "${inventory_rows}-1")
set(expected_targets 0)
set(expected_semantics 0)
set(expected_pending 0)
foreach(index RANGE 0 ${inventory_last})
  string(JSON row GET "${matrix_json}" cases ${index})
  string(JSON target GET "${row}" inventory_target)
  string(JSON state GET "${row}" coverage_state)
  string(JSON kind ERROR_VARIABLE kind_error GET "${row}" source_test kind)
  string(JSON isa GET "${row}" isa)
  if(target)
    math(EXPR expected_targets "${expected_targets}+1")
  endif()
  if(state STREQUAL "pending")
    math(EXPR expected_pending "${expected_pending}+1")
  endif()
  if(kind STREQUAL "semantic" AND isa STREQUAL "arm")
    math(EXPR expected_semantics "${expected_semantics}+1")
  endif()
endforeach()
math(EXPR expected_outputs "${expected_semantics}*2+4")
set(build_root "${source_root}/build")
cmake_path(IS_PREFIX build_root "${binary_root}" NORMALIZE binary_under_build)
if(NOT binary_under_build OR IS_SYMLINK "${source_root}/build" OR
   IS_SYMLINK "${binary_root}")
  message(FATAL_ERROR "coverage-case: unsafe-output-root")
endif()

if(CASE STREQUAL "arm-matrix-build")
  foreach(required IN ITEMS ARTIFACT_ROOT TOOLCHAIN_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
      message(FATAL_ERROR "coverage-case: missing-${required}")
    endif()
  endforeach()
  set(artifact_root "${source_root}/${ARTIFACT_ROOT}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}"
      "-DOUT=${artifact_root}" "-DTOOLCHAIN_ROOT=${source_root}/${TOOLCHAIN_ROOT}"
      -DMODE=all -DISA=arm
      -P "${builder}"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_error)
  if(NOT "${build_rc}" STREQUAL "0")
    message(FATAL_ERROR
      "coverage-case: arm-matrix-build-failed: ${build_error}${build_out}")
  endif()
  set(receipt "${artifact_root}/proof/build-receipt.json")
  if(NOT EXISTS "${receipt}" OR IS_DIRECTORY "${receipt}" OR IS_SYMLINK "${receipt}")
    message(FATAL_ERROR "coverage-case: arm-matrix-receipt-missing")
  endif()
  file(READ "${receipt}" receipt_json)
  string(JSON receipt_format ERROR_VARIABLE format_error GET "${receipt_json}" format)
  string(JSON receipt_isa ERROR_VARIABLE isa_error GET "${receipt_json}" isa)
  string(JSON semantic_count ERROR_VARIABLE semantic_error GET "${receipt_json}" semantic_pairs)
  string(JSON stop_count ERROR_VARIABLE stop_error GET "${receipt_json}" stop_witnesses)
  string(JSON rom_count ERROR_VARIABLE rom_error LENGTH "${receipt_json}" output_names)
  foreach(error IN ITEMS format_error isa_error semantic_error stop_error rom_error)
    if(NOT "${${error}}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "coverage-case: arm-matrix-receipt-invalid:${error}")
    endif()
  endforeach()
  if(NOT receipt_format STREQUAL "gamebloke.arm-diagnostic-matrix/1" OR
     NOT receipt_isa STREQUAL "arm" OR NOT semantic_count EQUAL expected_semantics OR
     NOT stop_count EQUAL 4 OR NOT rom_count EQUAL expected_outputs)
    message(FATAL_ERROR "coverage-case: arm-matrix-output-count-mismatch")
  endif()
  message("coverage-case: ok (${expected_semantics} ARM semantic pairs; 4 named stop witnesses)")
  return()
endif()

foreach(required_path IN ITEMS "${matrix}" "${checker}" "${builder}" "${inputs}")
  if(NOT EXISTS "${required_path}" OR IS_DIRECTORY "${required_path}" OR
     IS_SYMLINK "${required_path}")
    message(FATAL_ERROR "coverage-case: required-ledger-artifact-missing: ${required_path}")
  endif()
endforeach()

include("${source_root}/cmake/GblJson.cmake")
file(READ "${matrix}" matrix_json)
file(STRINGS "${inputs}" input_lines)
set(input_paths "")
foreach(line IN LISTS input_lines)
  string(STRIP "${line}" line)
  if("${line}" STREQUAL "" OR "${line}" MATCHES "^#")
    continue()
  endif()
  list(APPEND input_paths "${line}")
endforeach()
foreach(required_input IN ITEMS
    cmake/BuildArmDiagnostics.cmake cmake/FixGbaHeader.cmake
    cmake/CheckArmCoverage.cmake cmake/GblJson.cmake
    diagnostics/armv4t/NOTICE diagnostics/armv4t/assertions.inc
    diagnostics/armv4t/cases-arm.S diagnostics/armv4t/cases-thumb.S diagnostics/armv4t/cases.json
    diagnostics/armv4t/inputs.txt diagnostics/armv4t/linker.ld
    diagnostics/armv4t/smoke.S diagnostics/armv4t/startup.S
    scripts/fetch-toolchain.sh scripts/signature-scan.sh
    scripts/lib/signature-common.sh tests/signatures/allowlist.txt
    tests/signatures/bios.sha256 tests/signatures/logo.sha256
    toolchain/arm-none-eabi.lock)
  list(FIND input_paths "${required_input}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "coverage-case: source-input-unbound: ${required_input}")
  endif()
endforeach()
list(FIND input_paths "CMakeLists.txt" cmake_consumer_bound)
if(NOT cmake_consumer_bound EQUAL -1)
  message(FATAL_ERROR "coverage-case: unrelated-consumer-bound: CMakeLists.txt")
endif()
list(FIND input_paths "tests/fixtures/manifest.json" manifest_producer_bound)
if(NOT manifest_producer_bound EQUAL -1)
  message(FATAL_ERROR "coverage-case: manifest-consumer-bound: tests/fixtures/manifest.json")
endif()

set(control_root "${binary_root}/coverage-controls")
file(REMOVE_RECURSE "${control_root}")
file(MAKE_DIRECTORY "${control_root}")

function(find_case_index out family form)
  gbl_json_length(count "${matrix_json}" cases)
  math(EXPR last "${count}-1")
  set(found -1)
  foreach(index RANGE 0 ${last})
    string(JSON row_json GET "${matrix_json}" cases ${index})
    gbl_json_string(row_family "${row_json}" family)
    gbl_json_string(row_form "${row_json}" form)
    if(row_family STREQUAL "${family}" AND row_form STREQUAL "${form}")
      set(found "${index}")
      break()
    endif()
  endforeach()
  set(${out} "${found}" PARENT_SCOPE)
endfunction()

function(find_case_id_index out wanted_id)
  gbl_json_length(count "${matrix_json}" cases)
  math(EXPR last "${count}-1")
  set(found -1)
  foreach(index RANGE 0 ${last})
    string(JSON row_id GET "${matrix_json}" cases ${index} id)
    if(row_id STREQUAL "${wanted_id}")
      set(found "${index}")
      break()
    endif()
  endforeach()
  set(${out} "${found}" PARENT_SCOPE)
endfunction()

function(checker_failure label mode matrix_path expected_code)
  set(manifest_path "${source_root}/tests/fixtures/manifest.json")
  if(ARGC GREATER 4)
    set(manifest_path "${ARGV4}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DMODE=${mode}" "-DMATRIX=${matrix_path}"
      "-DMANIFEST=${manifest_path}"
      -P "${checker}"
    RESULT_VARIABLE check_rc OUTPUT_VARIABLE check_out ERROR_VARIABLE check_error)
  set(combined "${check_out}${check_error}")
  if("${check_rc}" STREQUAL "0" OR NOT combined MATCHES "coverage: ${expected_code}:")
    message(FATAL_ERROR "coverage-case: ${label}-control-failed: ${combined}")
  endif()
  message(STATUS "coverage-case: ${label} refusal observed (${expected_code})")
endfunction()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -DMODE=draft "-DMATRIX=${matrix}" -P "${checker}"
  RESULT_VARIABLE clean_rc OUTPUT_VARIABLE clean_out ERROR_VARIABLE clean_error)
set(clean_combined "${clean_out}${clean_error}")
if(NOT "${clean_rc}" STREQUAL "0" OR
   NOT clean_combined MATCHES "coverage: ok" OR
   NOT clean_combined MATCHES "targets=${expected_targets};" OR
   NOT clean_combined MATCHES "pending=${expected_pending};" OR
   NOT clean_combined MATCHES "source-verified=1" OR
   NOT clean_combined MATCHES "semantic-sources=${expected_semantics};" OR
   NOT clean_combined MATCHES "guest-stops=4" OR
   NOT clean_combined MATCHES "cpu-api=19")
  message(FATAL_ERROR "coverage-case: clean-ledger-control-failed: ${clean_error}${clean_out}")
endif()
message(STATUS "coverage-case: clean draft inventory passed (${expected_targets} ARM/Thumb forms; formats 1..19)")

# Each mutation starts from the clean ledger above. These reject the former
# semantic mismatches, and independently protect operands, bases and flags.
function(reject_program_mutation label row_index before after reason)
  string(JSON original_program GET "${matrix_json}" cases ${row_index} source_test program)
  string(FIND "${original_program}" "${before}" mutation_at)
  if(mutation_at LESS 0)
    message(FATAL_ERROR "coverage-case: ${label}-mutation-not-applied")
  endif()
  string(REPLACE "${before}" "${after}" changed_program "${original_program}")
  string(REPLACE "\\" "\\\\" changed_program "${changed_program}")
  string(REPLACE "\n" "\\n" changed_program "${changed_program}")
  string(JSON changed SET "${matrix_json}" cases ${row_index} source_test program "\"${changed_program}\"")
  set(variant "${control_root}/${label}.json")
  file(WRITE "${variant}" "${changed}\n")
  checker_failure("${label}" draft "${variant}" "${reason}")
endfunction()

find_case_id_index(pc_operand_index 507654410)
reject_program_mutation(pc-old-rn-form ${pc_operand_index}
  ".inst 0xE082031F" "add r0,pc,r2,lsl #1" semantic-pc-operand-invalid)
reject_program_mutation(pc-immediate-shift ${pc_operand_index}
  ".inst 0xE082031F" ".inst 0xE082008F" semantic-pc-operand-invalid)
reject_program_mutation(pc-plus-eight-oracle ${pc_operand_index}
  "add r1,r1,#12" "add r1,r1,#8" semantic-pc-operand-invalid)

foreach(block_id IN ITEMS 416055714 355734903 495708335 314309498
    344471309 469844796 489728888 516644361)
  find_case_id_index(block_index "${block_id}")
  reject_program_mutation("block-writeback-${block_id}" ${block_index}
    "r4, {r0,r1}" "r4!, {r0,r1}" semantic-block-writeback-invalid)
endforeach()
reject_program_mutation(block-base-oracle ${block_index}
  "cmp r4,r6" "cmp r4,r7" semantic-block-writeback-invalid)
string(JSON changed SET "${matrix_json}" cases ${block_index}
  source_test independent_expectation writeback true)
set(variant "${control_root}/block-writeback-metadata.json")
file(WRITE "${variant}" "${changed}\n")
checker_failure(block-writeback-metadata draft "${variant}" semantic-block-writeback-invalid)

# Reintroduce each removed Mode 3 form using its ordinary post-index producer:
# a duplicate legal instruction must never count as force-user coverage.
foreach(op IN ITEMS LDRH STRH LDRSB LDRSH)
  foreach(offset IN ITEMS split-nibble-immediate register-offset)
    find_case_index(mode3_index halfword-signed-transfer "${op}:${offset}:post-index")
    string(JSON changed SET "${matrix_json}" cases ${mode3_index} form
      "\"${op}:${offset}:post-index-force-user\"")
    set(variant "${control_root}/mode3-${op}-${offset}.json")
    file(WRITE "${variant}" "${changed}\n")
    checker_failure("mode3-${op}-${offset}" draft "${variant}" semantic-mode3-form-invalid)
  endforeach()
endforeach()

foreach(boundary_id IN ITEMS 293321832 292551648)
  find_case_id_index(boundary_index "${boundary_id}")
  foreach(vector IN ITEMS 0x00000000 0x00000001 0x80000000 0xFFFFFFFF)
    reject_program_mutation("boundary-${boundary_id}-${vector}" ${boundary_index}
      "ldr r1,=${vector}" "ldr r1,=0x80000001" semantic-boundary-vector-invalid)
  endforeach()
  reject_program_mutation("boundary-c-zero-${boundary_id}" ${boundary_index}
    "ldr r5,=0x00000000" "ldr r5,=0x20000000" semantic-boundary-vector-invalid)
  reject_program_mutation("boundary-result-${boundary_id}" ${boundary_index}
    "GBL_ASSERT_EQ r0, 0xFFFFFFFF, 0xFFFFFFFE"
    "GBL_ASSERT_EQ r0, 0xFFFFFFFE, 0xFFFFFFFF" semantic-boundary-vector-invalid)
  reject_program_mutation("boundary-flags-${boundary_id}" ${boundary_index}
    "GBL_ASSERT_MASKED r3, 0xF0000000, 0x60000000, 0x40000000"
    "GBL_ASSERT_MASKED r3, 0xF0000000, 0x40000000, 0x60000000" semantic-boundary-vector-invalid)
endforeach()

# Thumb controls start from the same clean ledger and preserve all existing
# ARM controls below. They exercise distinct omission/meaning failure classes.
foreach(wanted_id IN ITEMS 1800200001 1800200002 1800200003 1800200004
    555469226 747712347 638430992)
  find_case_id_index(thumb_index "${wanted_id}")
  string(JSON changed REMOVE "${matrix_json}" cases ${thumb_index})
  set(variant "${control_root}/thumb-missing-${wanted_id}.json")
  file(WRITE "${variant}" "${changed}\n")
  checker_failure("thumb-missing-${wanted_id}" draft "${variant}" missing-form)
endforeach()
find_case_id_index(thumb_index 631794229)
foreach(control IN ITEMS missing-source missing-probes wrong-operation weak-mask missing-label wrong-format)
  set(changed "${matrix_json}")
  if(control STREQUAL "missing-source")
    string(JSON changed REMOVE "${changed}" cases ${thumb_index} source_test)
    set(finding arm-source-test-missing)
  elseif(control STREQUAL "missing-probes")
    string(JSON changed SET "${changed}" cases ${thumb_index} source_test encoding_checks "[]")
    set(finding thumb-encoding-check-missing)
  elseif(control STREQUAL "wrong-operation")
    string(JSON changed SET "${changed}" cases ${thumb_index} source_test encoding_checks 0 value "\"0x0808\"")
    set(finding thumb-operation-encoding-invalid)
  elseif(control STREQUAL "weak-mask")
    string(JSON changed SET "${changed}" cases ${thumb_index} source_test encoding_checks 0 mask "\"0x0000\"")
    set(finding thumb-operation-encoding-invalid)
  elseif(control STREQUAL "missing-label")
    string(JSON changed SET "${changed}" cases ${thumb_index} source_test encoding_checks 0 symbol "\"gbl_absent\"")
    set(finding thumb-encoding-check-invalid)
  else()
    string(JSON changed SET "${changed}" cases ${thumb_index} format 2)
    set(finding missing-form)
  endif()
  set(variant "${control_root}/thumb-${control}.json")
  file(WRITE "${variant}" "${changed}\n")
  checker_failure("thumb-${control}" draft "${variant}" "${finding}")
endforeach()
foreach(vector IN ITEMS 0x00000000 0x00000001 0x80000000 0xFFFFFFFF)
  reject_program_mutation("thumb-vector-${vector}" ${thumb_index}
    "ldr r1,=${vector}" "ldr r1,=0x80000009" thumb-boundary-vector-invalid)
endforeach()
reject_program_mutation(thumb-vector-carry-zero ${thumb_index}
  "GBL_SEED_FLAGS 0x00000000" "GBL_SEED_FLAGS 0x20000000" thumb-boundary-vector-invalid)
reject_program_mutation(thumb-broken-mask-bit ${thumb_index}
  "GBL_ASSERT_MASKED r3, 0xF0000000, 0x40000000, 0x60000000"
  "GBL_ASSERT_MASKED r3, 0xD0000000, 0x40000000, 0x60000000" thumb-boundary-vector-invalid)
foreach(bl_id IN ITEMS 772491234 555469226 747712347)
  find_case_id_index(bl_index "${bl_id}")
  reject_program_mutation("thumb-bl-${bl_id}-actual-read" ${bl_index}
    "ldr r0,[r4,#8]" "movs r0,#0" thumb-bl-source-invalid)
endforeach()
find_case_index(cpu7_index cpu-quirk "CPU-7 Thumb BL boundary")
string(JSON changed SET "${matrix_json}" cases ${cpu7_index} source_test evidence "\"generic CPU API test\"")
set(variant "${control_root}/thumb-generic-irq-link.json")
file(WRITE "${variant}" "${changed}\n")
checker_failure(thumb-generic-irq-link draft "${variant}" api-irq-boundary-invalid)
find_case_id_index(other_thumb_index 742650290)
string(JSON first_thumb GET "${matrix_json}" cases ${thumb_index})
string(JSON other_thumb GET "${matrix_json}" cases ${other_thumb_index})
string(JSON changed SET "${matrix_json}" cases ${thumb_index} "${other_thumb}")
string(JSON changed SET "${changed}" cases ${other_thumb_index} "${first_thumb}")
set(variant "${control_root}/thumb-reordered-ids.json")
file(WRITE "${variant}" "${changed}\n")
checker_failure(thumb-reordered-ids draft "${variant}" thumb-stable-order-invalid)

# Coordinated source + supplied-opcode changes used to evade the independent
# format crosswalk. Replace the labeled instruction with its exact altered
# encoding, so this cannot fail merely because metadata disagrees with source.
function(reject_thumb_encoding_mutation row_index probe flip_mask weak_mask)
  string(JSON row GET "${matrix_json}" cases ${row_index})
  string(JSON id GET "${row}" id)
  string(JSON original GET "${row}" source_test program)
  string(JSON symbol GET "${row}" source_test encoding_checks ${probe} symbol)
  string(JSON value GET "${row}" source_test encoding_checks ${probe} value)
  math(EXPR altered "${value}^${flip_mask}" OUTPUT_FORMAT HEXADECIMAL)
  string(REGEX REPLACE "(${symbol}:\n)[^\n]+" "\\1.inst.n ${altered}" program "${original}")
  if(program STREQUAL original)
    message(FATAL_ERROR "coverage-case: coordinated-mutation-not-applied:${id}")
  endif()
  string(REPLACE "\n" "\\n" program "${program}")
  string(JSON changed SET "${matrix_json}" cases ${row_index} source_test program "\"${program}\"")
  string(JSON changed SET "${changed}" cases ${row_index} source_test encoding_checks ${probe} value "\"${altered}\"")
  set(label "thumb-coordinated-${id}-${probe}-${flip_mask}")
  set(variant "${control_root}/${label}.json")
  file(WRITE "${variant}" "${changed}\n")
  checker_failure("${label}" draft "${variant}" thumb-operation-encoding-invalid)
  # Same boundary with a correct value but a mask omitting its selector bits.
  string(JSON changed SET "${matrix_json}" cases ${row_index} source_test encoding_checks ${probe} mask "\"${weak_mask}\"")
  set(variant "${control_root}/${label}-mask.json")
  file(WRITE "${variant}" "${changed}\n")
  checker_failure("${label}-mask" draft "${variant}" thumb-operation-encoding-invalid)
endfunction()
foreach(id IN ITEMS 654394787 695071734 677986452 1800200004
    1800200005 1800200006 1800200007 1800200008 1800200009)
  find_case_id_index(i "${id}")
  # Flip either selector without inventing an invalid low-low high form.
  string(JSON form GET "${matrix_json}" cases ${i} form)
  if(form MATCHES "high-low$")
    set(flip 0x40)
  else()
    set(flip 0x80)
  endif()
  reject_thumb_encoding_mutation(${i} 0 ${flip} 0xff00)
endforeach()
foreach(id IN ITEMS 1800200011 1800200012 1800200013 1800200014 1800200015 1800200010)
  find_case_id_index(i "${id}")
  string(JSON form GET "${matrix_json}" cases ${i} form)
  if(form MATCHES "PC-source$")
    set(flip 0x8)
  elseif(form MATCHES "PC-destination$")
    set(flip 0x1)
  else()
    set(flip 0x40)
  endif()
  reject_thumb_encoding_mutation(${i} 0 ${flip} 0xff00)
endforeach()
# The unrestricted BX form must still allow either legal Rm high selector.
find_case_id_index(i 663506278)
string(JSON program GET "${matrix_json}" cases ${i} source_test program)
string(REGEX REPLACE "(gbl_probe_0:\n)[^\n]+" "\\1.inst.n 0x4700" program "${program}")
string(REPLACE "\n" "\\n" program "${program}")
string(JSON changed SET "${matrix_json}" cases ${i} source_test program "\"${program}\"")
string(JSON changed SET "${changed}" cases ${i} source_test encoding_checks 0 value "\"0x4700\"")
set(variant "${control_root}/thumb-legal-low-bx.json")
file(WRITE "${variant}" "${changed}\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=draft "-DMATRIX=${variant}" -P "${checker}"
  RESULT_VARIABLE legal_bx_rc OUTPUT_VARIABLE legal_bx_out ERROR_VARIABLE legal_bx_error)
if(NOT legal_bx_rc STREQUAL "0")
  message(FATAL_ERROR "coverage-case: legal-low-bx-refused:${legal_bx_out}${legal_bx_error}")
endif()
foreach(id IN ITEMS 571259220 654000269 785410767 766592286 650578773
    569802504 1800200016 1800200033 1800200034)
  find_case_id_index(i "${id}")
  reject_thumb_encoding_mutation(${i} 0 0x100 0xfe00)
  if(id STREQUAL "571259220")
    reject_thumb_encoding_mutation(${i} 1 0x100 0xfe00)
  endif()
  if(id MATCHES "^(569802504|1800200016|1800200033|1800200034)$")
    reject_thumb_encoding_mutation(${i} 0 0x1 0xff00)
  endif()
endforeach()
foreach(id IN ITEMS 539564381 737278802 1800200018 1800200019 1800200035 1800200036)
  find_case_id_index(i "${id}")
  if(id STREQUAL "1800200019")
    set(flip 0x4)
  elseif(id MATCHES "180020003[56]")
    set(flip 0x80)
  elseif(id STREQUAL "1800200018")
    set(flip 0x2)
  else()
    set(flip 0x1)
  endif()
  reject_thumb_encoding_mutation(${i} 0 ${flip} 0xf800)
endforeach()
# Concrete carry vectors, real instructions, assertions and their probe lists
# each have clean-first omission/meaning controls.
foreach(id IN ITEMS 742650290 1800200023 1800200027 1800200028 1800200037)
  find_case_id_index(i "${id}")
  reject_program_mutation("thumb-shift-${id}-c-zero" ${i}
    "GBL_SEED_FLAGS 0x00000000" "GBL_SEED_FLAGS 0x20000000" thumb-shift-vector-invalid)
  reject_program_mutation("thumb-shift-${id}-c-one" ${i}
    "GBL_SEED_FLAGS 0x20000000" "GBL_SEED_FLAGS 0x00000000" thumb-shift-vector-invalid)
  reject_program_mutation("thumb-shift-${id}-instruction" ${i}
    "gbl_probe_0:\n" "gbl_probe_0:\nmovs r0,#0\n" thumb-shift-vector-invalid)
  reject_program_mutation("thumb-shift-${id}-mask" ${i}
    "GBL_ASSERT_MASKED r3, 0xF0000000," "GBL_ASSERT_MASKED r3, 0xD0000000," thumb-shift-vector-invalid)
  string(JSON value GET "${matrix_json}" cases ${i} source_test encoding_checks 0 value)
  math(EXPR wrong "${value}^0x8" OUTPUT_FORMAT HEXADECIMAL)
  string(JSON changed SET "${matrix_json}" cases ${i} source_test encoding_checks 0 value "\"${wrong}\"")
  set(variant "${control_root}/thumb-shift-${id}-encoding.json")
  file(WRITE "${variant}" "${changed}\n")
  checker_failure("thumb-shift-${id}-encoding" draft "${variant}" thumb-shift-vector-invalid)
  string(JSON changed REMOVE "${matrix_json}" cases ${i} source_test encoding_checks 0)
  set(variant "${control_root}/thumb-shift-${id}-probe-omission.json")
  file(WRITE "${variant}" "${changed}\n")
  checker_failure("thumb-shift-${id}-probe-omission" draft "${variant}" thumb-shift-vector-invalid)
endforeach()
find_case_id_index(i 1800200037)
reject_program_mutation(thumb-ror-count33 ${i}
  "ldr r1,=33" "ldr r1,=32" thumb-shift-vector-invalid)
reject_program_mutation(thumb-ror-bit0 ${i}
  "ldr r0,=0x80000000" "ldr r0,=0x80000001" thumb-shift-vector-invalid)
reject_program_mutation(thumb-ror-result ${i}
  "GBL_ASSERT_EQ r0, 0xC0000000, 0xC0000001" "GBL_ASSERT_EQ r0, 0x80000001, 0x80000000" thumb-shift-vector-invalid)
reject_program_mutation(thumb-ror-flags ${i}
  "GBL_ASSERT_MASKED r3, 0xF0000000, 0x00000000, 0x20000000"
  "GBL_ASSERT_MASKED r3, 0xF0000000, 0x20000000, 0x00000000" thumb-shift-vector-invalid)
string(JSON changed REMOVE "${matrix_json}" cases ${i})
set(variant "${control_root}/thumb-ror-boundary-omitted.json")
file(WRITE "${variant}" "${changed}\n")
checker_failure(thumb-ror-boundary-omitted draft "${variant}" missing-form)
if(CASE STREQUAL "thumb-ledger-controls")
  message(STATUS "coverage-case: Thumb omission/meaning controls passed")
  return()
endif()

set(variant "${control_root}/zero-id.json")
string(JSON changed SET "${matrix_json}" cases 0 id "0")
file(WRITE "${variant}" "${changed}\n")
checker_failure(zero-id draft "${variant}" invalid-id)

find_case_index(smoke_index diagnostic-harness
  smoke-positive-and-real-assertion-broken-same-case-pair)
find_case_index(target_index data-processing AND:rotated-immediate)
find_case_index(stop_index condition NV-1111)
find_case_index(cpu9_index cpu-quirk "CPU-9 exception decode")
find_case_index(cpu7_index cpu-quirk "CPU-7 Thumb BL boundary")
find_case_id_index(short_multiply_policy_index 1147774326)
find_case_id_index(long_multiply_policy_index 1263725929)
find_case_id_index(data_wrap_policy_index 1275797271)
find_case_id_index(loaded_pc_policy_index 1214630623)
find_case_id_index(sequential_wrap_policy_index 1232880025)
find_case_id_index(timing_policy_index 1129470292)
find_case_id_index(mul_source_index 419836810)
find_case_id_index(data_wrap_source_index 529906570)
find_case_id_index(odd_ldrh_source_index 363662911)
if(smoke_index LESS 0 OR target_index LESS 0 OR stop_index LESS 0 OR
   cpu9_index LESS 0 OR cpu7_index LESS 0 OR
   short_multiply_policy_index LESS 0 OR long_multiply_policy_index LESS 0 OR
   data_wrap_policy_index LESS 0 OR loaded_pc_policy_index LESS 0 OR
   sequential_wrap_policy_index LESS 0 OR timing_policy_index LESS 0 OR
   mul_source_index LESS 0 OR data_wrap_source_index LESS 0 OR odd_ldrh_source_index LESS 0)
  message(FATAL_ERROR "coverage-case: required-control-case-missing")
endif()
string(JSON smoke_id GET "${matrix_json}" cases ${smoke_index} id)
gbl_json_string(smoke_state "${matrix_json}" cases ${smoke_index} coverage_state)
if(NOT smoke_state STREQUAL "source-verified")
  message(FATAL_ERROR "coverage-case: smoke-source-state-not-admission-independent: ${smoke_state}")
endif()

set(verified_matrix "${control_root}/verified-smoke.json")
string(JSON changed SET "${matrix_json}" cases ${smoke_index} coverage_state "\"verified\"")
file(WRITE "${verified_matrix}" "${changed}\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DMODE=draft "-DMATRIX=${verified_matrix}" -P "${checker}"
  RESULT_VARIABLE verified_rc OUTPUT_VARIABLE verified_out ERROR_VARIABLE verified_error)
set(verified_combined "${verified_out}${verified_error}")
if(NOT "${verified_rc}" STREQUAL "0" OR
   NOT verified_combined MATCHES "verified=1" OR
   NOT verified_combined MATCHES "smoke-pairs=1" OR
   NOT verified_combined MATCHES "pending=${expected_pending};")
  message(FATAL_ERROR "coverage-case: verified-smoke-count-control-failed: ${verified_combined}")
endif()
message(STATUS "coverage-case: verified smoke still counts as exactly one pair in draft mode")

set(variant "${control_root}/malformed-verified-smoke.json")
string(JSON changed SET "${matrix_json}" cases ${smoke_index} coverage_state "\"verified\"")
string(JSON changed SET "${changed}" cases ${smoke_index}
  broken_assertion thumb_broken_operand 6)
file(WRITE "${variant}" "${changed}\n")
checker_failure(malformed-verified-smoke draft "${variant}" broken-assertion-invalid)

set(variant "${control_root}/missing-arm-source.json")
string(JSON changed REMOVE "${matrix_json}" cases ${target_index} source_test)
file(WRITE "${variant}" "${changed}\n")
checker_failure(missing-arm-source draft "${variant}" arm-source-test-missing)

set(variant "${control_root}/same-arm-expectations.json")
string(JSON target_expected GET "${matrix_json}" cases ${target_index}
  source_test independent_expectation expected_value)
string(JSON changed SET "${matrix_json}" cases ${target_index}
  source_test independent_expectation broken_expected "\"${target_expected}\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(same-arm-expectations draft "${variant}" semantic-source-invalid)

set(variant "${control_root}/missing-stop-boundary.json")
string(JSON changed REMOVE "${matrix_json}" cases ${stop_index} execution_boundary)
file(WRITE "${variant}" "${changed}\n")
checker_failure(missing-stop-boundary draft "${variant}" missing-execution-boundary)

set(variant "${control_root}/false-stop-status.json")
string(JSON changed SET "${matrix_json}" cases ${stop_index} expected_status
  "\"GUEST_STOP/unregistered-content; semantic PASS/FAIL\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(false-stop-status draft "${variant}" guest-stop-source-invalid)

set(variant "${control_root}/false-stop-operation.json")
string(JSON changed SET "${matrix_json}" cases ${stop_index} source_test operation
  "\"mov r0,#0\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(false-stop-operation draft "${variant}" guest-stop-source-invalid)

set(variant "${control_root}/false-api-boundary.json")
string(JSON changed SET "${matrix_json}" cases ${cpu9_index} expected_status
  "\"GUEST_STOP/no-bios-undefined\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(false-api-boundary draft "${variant}" cpu-api-boundary-invalid)

set(variant "${control_root}/missing-cpu9-stop-link.json")
string(JSON changed REMOVE "${matrix_json}" cases ${cpu9_index}
  companion_case_ids 0)
file(WRITE "${variant}" "${changed}\n")
checker_failure(missing-cpu9-stop-link draft "${variant}" cpu9-stop-companion-missing)

set(variant "${control_root}/duplicate-companion.json")
string(JSON first_companion GET "${matrix_json}" cases ${cpu9_index}
  companion_case_ids 0)
string(JSON changed SET "${matrix_json}" cases ${cpu9_index}
  companion_case_ids 1 ${first_companion})
file(WRITE "${variant}" "${changed}\n")
checker_failure(duplicate-companion draft "${variant}" duplicate-companion-case-id)

set(variant "${control_root}/missing-api-irq-lower-boundary.json")
string(JSON changed SET "${matrix_json}" cases ${cpu7_index} expected value
  "\"API-only observation\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(missing-api-irq-lower-boundary draft "${variant}" api-irq-boundary-invalid)

set(variant "${control_root}/missing-short-multiply-policy-companion.json")
string(JSON changed REMOVE "${matrix_json}" cases ${short_multiply_policy_index}
  companion_case_ids 0)
file(WRITE "${variant}" "${changed}\n")
checker_failure(missing-short-multiply-policy-companion draft "${variant}"
  policy-companion-map-invalid)

set(variant "${control_root}/missing-s-multiply-assertion.json")
string(JSON mul_program GET "${matrix_json}" cases ${mul_source_index} source_test program)
string(REPLACE "muls r0,r1,r2" "mul r0,r1,r2" broken_mul_program "${mul_program}")
string(REPLACE "\n" "\\n" encoded_mul_program "${broken_mul_program}")
string(JSON changed SET "${matrix_json}" cases ${mul_source_index} source_test program
  "\"${encoded_mul_program}\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(missing-s-multiply-assertion draft "${variant}"
  semantic-multiply-flags-invalid)

set(variant "${control_root}/missing-data-wrap-policy-companion.json")
string(JSON changed REMOVE "${matrix_json}" cases ${data_wrap_policy_index}
  companion_case_ids 0)
file(WRITE "${variant}" "${changed}\n")
checker_failure(missing-data-wrap-policy-companion draft "${variant}"
  policy-companion-map-invalid)

set(variant "${control_root}/generic-loaded-pc-policy-companion.json")
string(JSON changed SET "${matrix_json}" cases ${loaded_pc_policy_index}
  companion_case_ids 0 529906570)
file(WRITE "${variant}" "${changed}\n")
checker_failure(generic-loaded-pc-policy-companion draft "${variant}"
  policy-companion-map-invalid)

set(variant "${control_root}/aligned-only-ldrh-source.json")
string(JSON odd_ldrh_program GET "${matrix_json}" cases ${odd_ldrh_source_index}
  source_test program)
string(REPLACE "ldrh r0, [r4, #5]" "ldrh r0, [r4, #4]"
  aligned_ldrh_program "${odd_ldrh_program}")
string(REPLACE "\n" "\\n" encoded_aligned_ldrh_program "${aligned_ldrh_program}")
string(JSON changed SET "${matrix_json}" cases ${odd_ldrh_source_index}
  source_test program "\"${encoded_aligned_ldrh_program}\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(aligned-only-ldrh-source draft "${variant}"
  semantic-source-edge-invalid)

set(variant "${control_root}/temporal-smoke-expectation.json")
string(JSON changed SET "${matrix_json}" cases ${smoke_index} expected value
  "\"Final pins await hosted identity evidence.\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(temporal-smoke-expectation draft "${variant}" smoke-temporal-expectation)

file(SHA256 "${matrix}" matrix_sha_before_admission)
file(SHA256 "${inputs}" inputs_sha_before_admission)
set(manifest_variant "${control_root}/manifest-only.json")
file(WRITE "${manifest_variant}"
  "{\n  \"schema\": \"gamebloke.manifest/1\",\n  \"fixtures\": [{\"id\": \"smoke\"}, {\"id\": \"smoke-broken\"}]\n}\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DMODE=smoke-admission "-DMATRIX=${matrix}"
    "-DMANIFEST=${manifest_variant}" -P "${checker}"
  RESULT_VARIABLE admission_rc OUTPUT_VARIABLE admission_out ERROR_VARIABLE admission_error)
set(admission_combined "${admission_out}${admission_error}")
if(NOT "${admission_rc}" STREQUAL "0" OR
   NOT admission_combined MATCHES "smoke-admission: ok" OR
   NOT admission_combined MATCHES "fixture-links=2")
  message(FATAL_ERROR "coverage-case: manifest-only-admission-control-failed: ${admission_combined}")
endif()
file(SHA256 "${matrix}" matrix_sha_after_admission)
file(SHA256 "${inputs}" inputs_sha_after_admission)
if(NOT matrix_sha_before_admission STREQUAL matrix_sha_after_admission OR
   NOT inputs_sha_before_admission STREQUAL inputs_sha_after_admission)
  message(FATAL_ERROR "coverage-case: manifest-only-admission-mutated-bound-input")
endif()
message(STATUS "coverage-case: manifest-only smoke admission passed without changing bound inputs")

file(WRITE "${manifest_variant}"
  "{\n  \"schema\": \"gamebloke.manifest/1\",\n  \"fixtures\": [{\"id\": \"smoke\"}]\n}\n")
checker_failure(missing-smoke-manifest-link smoke-admission "${matrix}"
  smoke-admission-fixture-link-missing "${manifest_variant}")

set(variant "${control_root}/duplicate-id.json")
string(JSON changed SET "${matrix_json}" cases ${target_index} id "${smoke_id}")
file(WRITE "${variant}" "${changed}\n")
checker_failure(duplicate-id draft "${variant}" duplicate-id)

set(variant "${control_root}/missing-form.json")
string(JSON changed SET "${matrix_json}" cases ${target_index} form "\"AND:invented-form\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(missing-form draft "${variant}" missing-form)

set(variant "${control_root}/silent-drop.json")
string(JSON changed REMOVE "${matrix_json}" cases ${target_index})
file(WRITE "${variant}" "${changed}\n")
checker_failure(silent-drop draft "${variant}" missing-form)

set(variant "${control_root}/wrong-variant.json")
string(JSON changed SET "${matrix_json}" cases ${smoke_index} negative_fixture "\"smoke\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(wrong-variant draft "${variant}" wrong-variant)

set(variant "${control_root}/final-with-pending.json")
string(JSON changed SET "${matrix_json}" stage "\"final\"")
file(WRITE "${variant}" "${changed}\n")
checker_failure(final-pending-case final "${variant}" pending-case)

gbl_json_length(case_count "${matrix_json}" cases)
math(EXPR case_last "${case_count}-1")
set(pending_case_id "")
foreach(index RANGE 0 ${case_last})
  string(JSON row_json GET "${matrix_json}" cases ${index})
  string(JSON inventory_target GET "${row_json}" inventory_target)
  string(JSON row_isa GET "${row_json}" isa)
  gbl_json_string(coverage_state "${row_json}" coverage_state)
  string(JSON source_kind ERROR_VARIABLE source_kind_error GET
    "${row_json}" source_test kind)
  if(coverage_state STREQUAL "pending" AND
     "${source_kind_error}" STREQUAL "NOTFOUND" AND
     source_kind STREQUAL "cpu-api")
    string(JSON pending_case_id GET "${row_json}" id)
    break()
  endif()
endforeach()
if("${pending_case_id}" STREQUAL "")
  message(FATAL_ERROR "coverage-case: no-pending-case-for-builder-control")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}"
    "-DOUT=${control_root}/pending-case-output"
    "-DTOOLCHAIN_ROOT=${source_root}/build/toolchains"
    -DMODE=case -DISA=arm "-DCASE_ID=${pending_case_id}"
    -P "${builder}"
  RESULT_VARIABLE case_rc OUTPUT_VARIABLE case_out ERROR_VARIABLE case_error)
set(case_combined "${case_out}${case_error}")
if("${case_rc}" STREQUAL "0" OR
   NOT case_combined MATCHES "arm-diagnostics: case-not-runnable: pending")
  message(FATAL_ERROR "coverage-case: pending-case-builder-refusal-failed: ${case_combined}")
endif()
message(STATUS "coverage-case: pending case ${pending_case_id} refused honestly")

execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}"
    "-DOUT=${control_root}/all-output"
    "-DTOOLCHAIN_ROOT=${source_root}/build/toolchains" -DMODE=all -P "${builder}"
  RESULT_VARIABLE all_rc OUTPUT_VARIABLE all_out ERROR_VARIABLE all_error)
set(all_combined "${all_out}${all_error}")
if("${all_rc}" STREQUAL "0" OR
   NOT all_combined MATCHES "arm-diagnostics: all-output-must-be-build/phase05-matrix/arm")
  message(FATAL_ERROR "coverage-case: all-builder-refusal-failed: ${all_combined}")
endif()
message(STATUS "coverage-case: aggregate builder refuses an unsafe output root")

# Parent iteration can supply a row-location hint, but literal CASE_ID remains
# the identity. Invalid hints must fail before deleting or creating output.
set(hint_output "${control_root}/index-hint-output")
file(MAKE_DIRECTORY "${hint_output}")
file(WRITE "${hint_output}/preserve.txt" "prior-output\n")
foreach(control IN ITEMS wrong-id out-of-range malformed external-hint)
  set(hint "${target_index}")
  set(hint_id 1)
  set(child 1)
  set(expected_code matrix-case-index-id-mismatch)
  if(control STREQUAL "out-of-range")
    set(hint "${case_count}")
    set(expected_code invalid-matrix-case-index)
  elseif(control STREQUAL "malformed")
    set(hint invalid)
    set(expected_code invalid-matrix-case-index)
  elseif(control STREQUAL "external-hint")
    set(child 0)
    set(expected_code invalid-matrix-case-index)
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${source_root}" "-DOUT=${hint_output}"
      "-DTOOLCHAIN_ROOT=${source_root}/build/toolchains"
      -DMODE=case -DISA=arm "-DCASE_ID=${hint_id}" "-DMATRIX_CHILD=${child}"
      "-DMATRIX_CASE_INDEX=${hint}" -P "${builder}"
    RESULT_VARIABLE hint_rc OUTPUT_VARIABLE hint_out ERROR_VARIABLE hint_error)
  if(hint_rc STREQUAL "0" OR
     NOT "${hint_out}${hint_error}" MATCHES "arm-diagnostics: ${expected_code}" OR
     NOT EXISTS "${hint_output}/preserve.txt")
    message(FATAL_ERROR "coverage-case: ${control}-hint-control-failed: ${hint_out}${hint_error}")
  endif()
  message(STATUS "coverage-case: ${control} hint refused before output mutation")
endforeach()
message("coverage-case: ok (draft/final/admission states, source-input boundary, and mutation controls)")
