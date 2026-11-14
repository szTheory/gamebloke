cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "diagnostics-case: missing-${required}")
  endif()
endforeach()
if(CASE MATCHES "^(all-admitted|admitted-pair)$")
  include("${SRC}/tests/gates/diagnostic_admission.cmake")
  return()
endif()
if(CASE STREQUAL "thumb-boundary-tracer")
  execute_process(COMMAND "${CMAKE_COMMAND}" -DCASE=thumb-pre-admission
    "-DSRC=${SRC}" "-DBIN=${BIN}" -DARTIFACT_ROOT=build/phase05-matrix/thumb
    -P "${SRC}/tests/gates/diagnostics_case.cmake"
    WORKING_DIRECTORY "${SRC}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "diagnostics-case: thumb-boundary-contract: every raw pair/stop and separate BL observation must be checked: ${output}${error}")
  endif()
  message(STATUS "diagnostics-case: thumb-boundary-tracer passed")
  return()
endif()
if(CASE STREQUAL "thumb-producer-tracer")
  # Narrow executable contract: the first stable Thumb ID must be selectable
  # through the production builder, not merely named in the draft inventory.
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DSRC=${SRC}"
    -DOUT=build/phase05-spike/thumb-tracer -DTOOLCHAIN_ROOT=build/toolchains
    -DMODE=case -DISA=thumb -DCASE_ID=631794229
    -P "${SRC}/cmake/BuildArmDiagnostics.cmake"
    WORKING_DIRECTORY "${SRC}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "diagnostics-case: thumb-producer-contract: selected original pair must build: ${output}${error}")
  endif()
  foreach(name IN ITEMS thumb-631794229.gba thumb-631794229-broken.gba)
    if(NOT EXISTS "${SRC}/build/phase05-spike/thumb-tracer/roms/${name}")
      message(FATAL_ERROR "diagnostics-case: thumb-producer-contract: missing ${name}")
    endif()
  endforeach()
  message(STATUS "diagnostics-case: thumb-producer-tracer passed")
  return()
endif()
if(CASE MATCHES "^(arm|thumb)-pre-admission$")
  set(diagnostic_isa "${CMAKE_MATCH_1}")
  if(diagnostic_isa STREQUAL "arm")
    set(expected_stops 4)
    set(expected_controls 25)
  else()
    set(expected_stops 8)
    #13 common controls,2 for each stop, and12 BL controls per clean row.
    set(expected_controls 65)
  endif()
  if(NOT DEFINED ARTIFACT_ROOT OR "${ARTIFACT_ROOT}" STREQUAL "")
    message(FATAL_ERROR "diagnostics-case: missing-ARTIFACT_ROOT")
  endif()
  get_filename_component(source_root "${SRC}" ABSOLUTE)
  get_filename_component(binary_root "${BIN}" ABSOLUTE)
  get_filename_component(artifact_root "${ARTIFACT_ROOT}" ABSOLUTE
    BASE_DIR "${source_root}")
  set(build_root "${source_root}/build")
  cmake_path(IS_PREFIX build_root "${binary_root}" NORMALIZE binary_under_build)
  if(NOT binary_under_build OR binary_root STREQUAL build_root OR
     IS_SYMLINK "${build_root}" OR IS_SYMLINK "${binary_root}")
    message(FATAL_ERROR "diagnostics-case: unsafe-arm-result-root")
  endif()
  set(expected_artifact_root "${source_root}/build/phase05-matrix/${diagnostic_isa}")
  set(lane_artifact_root "${binary_root}/generated/diagnostics/proof/${diagnostic_isa}")
  if((NOT "${artifact_root}" STREQUAL "${expected_artifact_root}" AND
      NOT "${artifact_root}" STREQUAL "${lane_artifact_root}") OR
     IS_SYMLINK "${artifact_root}")
    message(FATAL_ERROR "diagnostics-case: unsafe-arm-artifact-root")
  endif()
  set(matrix "${source_root}/diagnostics/armv4t/cases.json")
  set(matrix_receipt "${artifact_root}/proof/build-receipt.json")
  set(runner "${binary_root}/gamebloke-run")
  foreach(path IN ITEMS "${matrix}" "${matrix_receipt}" "${runner}")
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}" OR IS_SYMLINK "${path}")
      message(FATAL_ERROR "diagnostics-case: arm-pre-admission-input-missing: ${path}")
    endif()
  endforeach()
  file(READ "${matrix}" matrix_json)
  # Require the architecture-derived ledger before deriving runtime totals.
  execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=draft "-DMATRIX=${matrix}"
    -P "${source_root}/cmake/CheckArmCoverage.cmake"
    RESULT_VARIABLE ledger_rc OUTPUT_VARIABLE ledger_out ERROR_VARIABLE ledger_err)
  if(NOT ledger_rc STREQUAL "0")
    message(FATAL_ERROR "diagnostics-case: invalid-arm-ledger:${ledger_out}${ledger_err}")
  endif()
  string(JSON inventory_rows LENGTH "${matrix_json}" cases)
  math(EXPR inventory_last "${inventory_rows}-1")
  set(expected_semantics 0)
  foreach(index RANGE 0 ${inventory_last})
    string(JSON kind ERROR_VARIABLE kind_error GET "${matrix_json}" cases ${index} source_test kind)
    string(JSON row_isa GET "${matrix_json}" cases ${index} isa)
    if(kind STREQUAL "semantic" AND row_isa STREQUAL diagnostic_isa)
      math(EXPR expected_semantics "${expected_semantics}+1")
    endif()
  endforeach()
  math(EXPR expected_outputs "${expected_semantics}*2+${expected_stops}")
  file(READ "${matrix_receipt}" matrix_build_json)
  string(JSON receipt_format ERROR_VARIABLE format_error GET "${matrix_build_json}" format)
  string(JSON receipt_isa ERROR_VARIABLE isa_error GET "${matrix_build_json}" isa)
  string(JSON receipt_commit ERROR_VARIABLE commit_error GET "${matrix_build_json}" source_commit)
  string(JSON receipt_semantic ERROR_VARIABLE semantic_count_error GET "${matrix_build_json}" semantic_pairs)
  string(JSON receipt_stops ERROR_VARIABLE stop_count_error GET "${matrix_build_json}" stop_witnesses)
  string(JSON receipt_output_count ERROR_VARIABLE output_count_error LENGTH "${matrix_build_json}" output_names)
  foreach(error IN ITEMS format_error isa_error commit_error semantic_count_error
      stop_count_error output_count_error)
    if(NOT "${${error}}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "diagnostics-case: invalid-arm-matrix-receipt:${error}")
    endif()
  endforeach()
  string(LENGTH "${receipt_commit}" receipt_commit_length)
  if(NOT receipt_format STREQUAL "gamebloke.arm-diagnostic-matrix/1" OR
     NOT receipt_isa STREQUAL diagnostic_isa OR NOT receipt_semantic EQUAL expected_semantics OR
     NOT receipt_stops EQUAL expected_stops OR NOT receipt_output_count EQUAL expected_outputs OR
     NOT receipt_commit_length EQUAL 40 OR
     NOT receipt_commit MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "diagnostics-case: arm-matrix-receipt-mismatch")
  endif()

  file(STRINGS "${source_root}/diagnostics/armv4t/inputs.txt" producer_inputs)
  foreach(input IN LISTS producer_inputs)
    if(input STREQUAL "" OR input MATCHES "^#")
      continue()
    endif()
    execute_process(COMMAND git diff --quiet "${receipt_commit}" -- "${input}"
      WORKING_DIRECTORY "${source_root}" RESULT_VARIABLE input_rc)
    if(NOT input_rc STREQUAL "0")
      message(FATAL_ERROR "diagnostics-case: stale-arm-producer-input:${input}")
    endif()
  endforeach()

  set(expected_names "")
  set(semantic_positive_count 0)
  set(semantic_broken_count 0)
  set(stop_witness_count 0)
  set(result_root "${binary_root}/diagnostics-results/${diagnostic_isa}-pre-admission")
  file(REMOVE_RECURSE "${result_root}")
  file(MAKE_DIRECTORY "${result_root}")
  include("${source_root}/tests/gates/diagnostic_result_authority.cmake")
  gbl_diagnostic_result_authority("${source_root}")

  # BL state is exposed by actual guest data reads after stores. Bus write
  # trace values describe bus return values and are never used as stored data.
  function(check_thumb_bl out records state)
    set(prefix_count 0)
    set(suffix_count 0)
    set(retired 0)
    set(prefix_fetches 0)
    set(suffix_fetches 0)
    set(prefix_unit_fetches 0)
    set(suffix_unit_fetches 0)
    set(capture_count 0)
    foreach(record IN LISTS records)
      foreach(field IN ITEMS category opcode instruction_address address value width access_class bus_outcome sequence guest_cycle)
        string(JSON r_${field} GET "${record}" ${field})
      endforeach()
      math(EXPR opcode "${r_opcode}")
      math(EXPR family "${opcode} & 0xf800")
      if(r_category EQUAL 1)
        math(EXPR retired "${retired}+1")
        if(opcode EQUAL 0xf000)
          math(EXPR prefix_count "${prefix_count}+1")
          math(EXPR prefix "${r_instruction_address}")
          set(prefix_retired "${retired}")
          set(prefix_cycle "${r_guest_cycle}")
        elseif(opcode LESS 65536 AND family EQUAL 0xf800)
          math(EXPR suffix_count "${suffix_count}+1")
          math(EXPR suffix "${r_instruction_address}")
          math(EXPR target "${prefix}+4+((${opcode} & 0x7ff)*2)")
          set(suffix_retired "${retired}")
          set(suffix_cycle "${r_guest_cycle}")
        endif()
        if(DEFINED prefix_retired)
          math(EXPR prefix_next "${prefix_retired}+1")
          if(retired EQUAL prefix_next)
            math(EXPR next_address "${r_instruction_address}")
            math(EXPR expected_address "${prefix}+2")
            if(NOT next_address EQUAL expected_address)
              set(${out} bl-prefix-next PARENT_SCOPE)
              return()
            endif()
          endif()
        endif()
        if(DEFINED suffix_retired)
          math(EXPR suffix_next "${suffix_retired}+1")
          if(retired EQUAL suffix_next)
            math(EXPR next_address "${r_instruction_address}")
            if(NOT next_address EQUAL target)
              set(${out} bl-suffix-next PARENT_SCOPE)
              return()
            endif()
          endif()
        endif()
      elseif(r_category EQUAL 2)
        if(opcode EQUAL 0xf000)
          if(NOT DEFINED prefix)
            set(${out} bl-prefix-retirement PARENT_SCOPE)
            return()
          endif()
          math(EXPR expected_address "${prefix}+4")
          math(EXPR address "${r_address}")
          if(NOT address EQUAL expected_address OR NOT r_width EQUAL 2 OR
             NOT r_access_class EQUAL 1 OR NOT r_bus_outcome EQUAL 4)
            set(${out} bl-prefix-fetch PARENT_SCOPE)
            return()
          endif()
          math(EXPR prefix_unit_fetches "${prefix_unit_fetches}+1")
        elseif(opcode LESS 65536 AND family EQUAL 0xf800)
          if(NOT DEFINED suffix)
            set(${out} bl-suffix-retirement PARENT_SCOPE)
            return()
          endif()
          math(EXPR address "${r_address}")
          math(EXPR expected_address "${target}+${suffix_unit_fetches}*2")
          if(NOT address EQUAL expected_address OR NOT r_width EQUAL 2 OR
             NOT r_access_class EQUAL suffix_unit_fetches OR NOT r_bus_outcome EQUAL 4)
            set(${out} bl-suffix-fetch PARENT_SCOPE)
            return()
          endif()
          math(EXPR suffix_unit_fetches "${suffix_unit_fetches}+1")
        endif()
        # Only LDR data-read observations can establish captured register data.
        if(family EQUAL 0x6800 AND r_address MATCHES "^0x030060(00|04|08|0c)$")
          if(NOT r_width EQUAL 4 OR NOT r_bus_outcome EQUAL 1)
            set(${out} bl-capture PARENT_SCOPE)
            return()
          endif()
          math(EXPR address "${r_address}")
          math(EXPR lane "(${address}-0x03006000)/4")
          if(NOT lane EQUAL capture_count)
            set(${out} bl-capture-order PARENT_SCOPE)
            return()
          endif()
          if(lane EQUAL 0)
            math(EXPR expected "${prefix}+4")
          elseif(lane EQUAL 1)
            math(EXPR expected "${prefix}+8")
          elseif(lane EQUAL 2)
            math(EXPR expected "(${suffix}+2)|1")
          else()
            math(EXPR expected "${target}+6")
          endif()
          math(EXPR value "${r_value}")
          if(NOT value EQUAL expected)
            set(${out} "bl-capture-${lane}" PARENT_SCOPE)
            return()
          endif()
          set(capture_${lane} "${r_value}")
          math(EXPR capture_count "${capture_count}+1")
        endif()
      endif()
    endforeach()
    if(NOT prefix_count EQUAL 1 OR NOT prefix_unit_fetches EQUAL 1)
      set(${out} bl-prefix-retirement PARENT_SCOPE)
      return()
    endif()
    if(state STREQUAL "pass")
      if(NOT suffix_count EQUAL 1 OR NOT suffix_unit_fetches EQUAL 2 OR
         NOT capture_count EQUAL 4 OR suffix_retired LESS_EQUAL prefix_retired OR
         suffix_cycle LESS_EQUAL prefix_cycle OR suffix EQUAL prefix)
        set(${out} bl-suffix-retirement PARENT_SCOPE)
        return()
      endif()
    elseif(NOT suffix_count EQUAL 0 OR NOT capture_count EQUAL 2)
      # Broken companion stops at the first prefix assertion, before suffix.
      set(${out} bl-broken-boundary PARENT_SCOPE)
      return()
    endif()
    set(bl_receipt "{}")
    foreach(field IN ITEMS prefix prefix_retired prefix_cycle suffix suffix_retired suffix_cycle target capture_0 capture_1 capture_2 capture_3)
      if(DEFINED ${field})
        string(JSON bl_receipt SET "${bl_receipt}" "${field}" "\"${${field}}\"")
      else()
        string(JSON bl_receipt SET "${bl_receipt}" "${field}" null)
      endif()
    endforeach()
    string(JSON bl_receipt SET "${bl_receipt}" source "\"actual-production-trace-and-guest-data-reads\"")
    string(JSON bl_receipt SET "${bl_receipt}" semantic_state "\"${state}\"")
    set_property(GLOBAL PROPERTY THUMB_BL_LAST_RECEIPT "${bl_receipt}")
    set(${out} ok PARENT_SCOPE)
  endfunction()

  function(check_thumb_bl_controls doc records row_id actual_sha)
    check_thumb_bl(clean "${records}" pass)
    if(NOT clean STREQUAL "ok")
      message(FATAL_ERROR "diagnostics-case: bl-control-clean-failed:${row_id}:${clean}")
    endif()
    foreach(control IN ITEMS prefix-retirement suffix-retirement prefix-address suffix-address prefix-fetch suffix-fetch-first suffix-fetch-second capture-0 capture-1 capture-2 capture-3 fused-retirement)
      set(changed "")
      set(suffix_fetch_index 0)
      foreach(record IN LISTS records)
        foreach(field IN ITEMS category opcode address access_class)
          string(JSON c_${field} GET "${record}" ${field})
        endforeach()
        math(EXPR op "${c_opcode}")
        math(EXPR family "${op}&0xf800")
        set(is_prefix FALSE)
        set(is_suffix FALSE)
        if(op EQUAL 0xf000)
          set(is_prefix TRUE)
        elseif(op LESS 65536 AND family EQUAL 0xf800)
          set(is_suffix TRUE)
        endif()
        if(c_category EQUAL 1 AND
           ((control STREQUAL "prefix-retirement" AND is_prefix) OR
            (control STREQUAL "suffix-retirement" AND is_suffix)))
          string(JSON record SET "${record}" opcode "\"0x000046c0\"")
        elseif(c_category EQUAL 1 AND
           ((control STREQUAL "prefix-address" AND is_prefix) OR
            (control STREQUAL "suffix-address" AND is_suffix)))
          string(JSON record SET "${record}" instruction_address "\"0x08000000\"")
        elseif(control STREQUAL "fused-retirement" AND c_category EQUAL 1 AND is_prefix)
          string(JSON record SET "${record}" opcode "\"0xf000f800\"")
        elseif(control STREQUAL "prefix-fetch" AND c_category EQUAL 2 AND is_prefix)
          string(JSON record SET "${record}" access_class 0)
        elseif(c_category EQUAL 2 AND is_suffix)
          if((control STREQUAL "suffix-fetch-first" AND suffix_fetch_index EQUAL 0) OR
             (control STREQUAL "suffix-fetch-second" AND suffix_fetch_index EQUAL 1))
            string(JSON record SET "${record}" access_class 9)
          endif()
          math(EXPR suffix_fetch_index "${suffix_fetch_index}+1")
        elseif(control MATCHES "^capture-([0-3])$" AND c_category EQUAL 2 AND family EQUAL 0x6800)
          math(EXPR wanted "0x03006000+${CMAKE_MATCH_1}*4")
          math(EXPR address "${c_address}")
          if(address EQUAL wanted)
            string(JSON record SET "${record}" value "\"0x00000000\"")
          endif()
        endif()
        list(APPEND changed "${record}")
      endforeach()
      check_thumb_bl(finding "${changed}" pass)
      if(control MATCHES "^capture-([0-3])$")
        set(expected_finding "bl-capture-${CMAKE_MATCH_1}")
      elseif(control MATCHES "^(prefix-retirement|fused-retirement)$")
        set(expected_finding bl-prefix-retirement)
      elseif(control STREQUAL "suffix-retirement")
        set(expected_finding bl-suffix-retirement)
      elseif(control STREQUAL "suffix-address")
        set(expected_finding bl-capture-2)
      elseif(control MATCHES "^suffix-fetch-")
        set(expected_finding bl-suffix-fetch)
      else()
        set(expected_finding bl-prefix-fetch)
      endif()
      if(NOT finding STREQUAL expected_finding)
        message(FATAL_ERROR "diagnostics-case: bl-control-wrong-cause:${row_id}:${control}: expected=${expected_finding} actual=${finding}")
      endif()
      file(APPEND "${result_root}/controls.txt" "${row_id}-${control}=${finding}\n")
    endforeach()
  endfunction()

  # One validator consumes real observations and the clean-first controls below.
  # Runner status, mailbox semantics, byte identity and trace are independent.
  function(check_arm_observation out doc trace_rows row_id row_kind
      expected_state expected_reason actual_sha)
    foreach(field IN ITEMS status reason termination)
      string(JSON value ERROR_VARIABLE error GET "${doc}" ${field})
      if(NOT error STREQUAL "NOTFOUND")
        set(${out} json PARENT_SCOPE)
        return()
      endif()
      set(observed_${field} "${value}")
    endforeach()
    string(JSON id_type TYPE "${doc}" fixture id)
    string(JSON ran_type TYPE "${doc}" fixture ran)
    string(JSON ran GET "${doc}" fixture ran)
    string(JSON fixture_sha GET "${doc}" fixture rom_sha256)
    string(JSON claim_sha GET "${doc}" claim cart rom_sha256)
    string(JSON machine GET "${doc}" claim machine)
    string(JSON boot GET "${doc}" claim boot)
    string(JSON bios GET "${doc}" claim bios_provider)
    string(JSON state GET "${doc}" semantic state)
    string(JSON id GET "${doc}" semantic case_id)
    string(JSON cycles GET "${doc}" time cycles)
    string(JSON instructions GET "${doc}" time instructions)
    string(JSON budget GET "${doc}" time budget_cycles)
    if(NOT observed_status STREQUAL "GUEST_STOP" OR
       NOT observed_reason STREQUAL "${expected_reason}" OR
       NOT observed_termination STREQUAL "${expected_reason}")
      set(${out} status PARENT_SCOPE)
      return()
    endif()
    if(NOT id_type STREQUAL "NULL" OR NOT ran_type STREQUAL "BOOLEAN" OR NOT ran OR
       NOT fixture_sha STREQUAL "${actual_sha}" OR
       NOT claim_sha STREQUAL "${actual_sha}" OR NOT machine STREQUAL "agb" OR
       NOT boot STREQUAL "direct-start" OR NOT bios STREQUAL "none")
      set(${out} identity PARENT_SCOPE)
      return()
    endif()
    if(NOT state STREQUAL "${expected_state}" OR NOT id STREQUAL "${row_id}")
      set(${out} semantic PARENT_SCOPE)
      return()
    endif()
    if(NOT cycles MATCHES "^[0-9]+$" OR NOT instructions MATCHES "^[0-9]+$" OR
       NOT budget STREQUAL "1000" OR cycles LESS 1 OR instructions LESS 1 OR
       cycles GREATER budget)
      set(${out} time PARENT_SCOPE)
      return()
    endif()
    foreach(field IN ITEMS mask count capacity complete emitted)
      string(JSON trace_${field} ERROR_VARIABLE error GET "${doc}" trace ${field})
      if(NOT error STREQUAL "NOTFOUND")
        set(${out} trace-metadata PARENT_SCOPE)
        return()
      endif()
    endforeach()
    list(LENGTH trace_rows trace_count)
    string(JSON reported_trace_count GET "${doc}" trace count)
    if(NOT trace_mask STREQUAL "7" OR NOT trace_capacity STREQUAL "4096" OR
       NOT trace_complete OR NOT trace_emitted OR trace_count LESS 1 OR
       NOT reported_trace_count STREQUAL "${trace_count}")
      set(${out} trace-metadata PARENT_SCOPE)
      return()
    endif()
    set(sequence 0)
    set(last_cycle 0)
    set(retired 0)
    set(exceptions 0)
    set(stop_fetches 0)
    set(expected_opcode "")
    set(expected_exception 0)
    set(stop_width 4)
    set(loaded_pc_opcode "")
    set(loaded_pc_refills 0)
    set(loaded_pc_targets 0)
    if(row_id STREQUAL "314196076")
      set(loaded_pc_opcode "0xe594f000")
    elseif(row_id STREQUAL "433134728")
      set(loaded_pc_opcode "0xe8948000")
    endif()
    if(row_kind STREQUAL "guest-stop")
      if(row_id STREQUAL "1765902634")
        set(expected_opcode "0xefabcdef")
        set(expected_exception 3)
      elseif(row_id STREQUAL "1632756083")
        set(expected_opcode "0xe7f000f0")
        set(expected_exception 2)
      elseif(row_id STREQUAL "1724321294")
        set(expected_opcode "0xee000010")
        set(expected_exception 8)
      elseif(row_id STREQUAL "448472880")
        set(expected_opcode "0xf0000000")
      elseif(diagnostic_isa STREQUAL "thumb")
        set(stop_width 2)
        if(row_id STREQUAL "638430992")
          set(expected_opcode "0x0000dfa7")
          set(expected_exception 3)
        elseif(row_id STREQUAL "553969564")
          set(expected_opcode "0x0000de00")
          set(expected_exception 2)
        elseif(row_id STREQUAL "569802504")
          set(expected_opcode "0x0000b400")
        elseif(row_id STREQUAL "1800200016")
          set(expected_opcode "0x0000bc00")
        elseif(row_id STREQUAL "539564381")
          set(expected_opcode "0x0000c000")
        elseif(row_id STREQUAL "737278802")
          set(expected_opcode "0x0000c800")
        elseif(row_id STREQUAL "1800200018")
          set(expected_opcode "0x0000c107")
        elseif(row_id STREQUAL "1800200019")
          set(expected_opcode "0x0000c207")
        else()
          set(${out} unknown-stop PARENT_SCOPE)
          return()
        endif()
      else()
        set(${out} unknown-stop PARENT_SCOPE)
        return()
      endif()
    endif()
    foreach(record IN LISTS trace_rows)
      foreach(field IN ITEMS sequence guest_cycle category instruction_address
          opcode address value width access_class bus_outcome exception_kind)
        string(JSON r_${field} ERROR_VARIABLE error GET "${record}" ${field})
        if(NOT error STREQUAL "NOTFOUND")
          set(${out} trace-record PARENT_SCOPE)
          return()
        endif()
      endforeach()
      if(NOT r_sequence STREQUAL "${sequence}" OR
         NOT r_guest_cycle MATCHES "^[0-9]+$" OR
         r_guest_cycle LESS last_cycle OR r_guest_cycle GREATER cycles)
        set(${out} trace-order PARENT_SCOPE)
        return()
      endif()
      if(r_category EQUAL 2)
        math(EXPR bus_address "${r_address}")
        if(bus_address LESS 32)
          if(row_kind STREQUAL "guest-stop")
            set(${out} vector-fetch PARENT_SCOPE)
          else()
            set(${out} low-address-bus PARENT_SCOPE)
          endif()
          return()
        endif()
        if(NOT loaded_pc_opcode STREQUAL "" AND r_opcode STREQUAL loaded_pc_opcode)
          if(r_value MATCHES "^0x08" AND r_width EQUAL 4 AND r_bus_outcome EQUAL 4)
            math(EXPR target_lane "${r_value} & 3")
            if(target_lane EQUAL 3)
              math(EXPR loaded_pc_target "${r_value} & 0xfffffffc")
            endif()
          endif()
          if(DEFINED loaded_pc_target AND bus_address EQUAL loaded_pc_target AND
             r_access_class EQUAL 0 AND r_bus_outcome EQUAL 4 AND
             r_value STREQUAL "0xe3a00011")
            math(EXPR loaded_pc_refills "${loaded_pc_refills}+1")
          endif()
        endif()
        if(NOT expected_opcode STREQUAL "" AND r_value STREQUAL expected_opcode AND
           r_width EQUAL stop_width AND r_bus_outcome EQUAL 4)
          math(EXPR stop_fetches "${stop_fetches}+1")
          set(stop_address "${r_address}")
        endif()
      elseif(r_category EQUAL 1)
        math(EXPR retired "${retired}+1")
        set(last_instruction_opcode "${r_opcode}")
        set(last_instruction_address "${r_instruction_address}")
        if(DEFINED loaded_pc_target)
          math(EXPR instruction_address "${r_instruction_address}")
          if(instruction_address EQUAL loaded_pc_target AND r_opcode STREQUAL "0xe3a00011")
            math(EXPR loaded_pc_targets "${loaded_pc_targets}+1")
          endif()
        endif()
      elseif(r_category EQUAL 4)
        math(EXPR exceptions "${exceptions}+1")
        if(NOT r_opcode STREQUAL expected_opcode OR
           NOT r_exception_kind EQUAL expected_exception OR
           NOT r_instruction_address STREQUAL stop_address OR
           NOT r_instruction_address STREQUAL last_instruction_address OR
           NOT last_instruction_opcode STREQUAL expected_opcode OR
           NOT r_guest_cycle STREQUAL cycles)
          set(${out} exception-context PARENT_SCOPE)
          return()
        endif()
      else()
        set(${out} trace-category PARENT_SCOPE)
        return()
      endif()
      set(last_category "${r_category}")
      set(last_cycle "${r_guest_cycle}")
      math(EXPR sequence "${sequence}+1")
    endforeach()
    if(NOT retired EQUAL instructions OR NOT last_cycle STREQUAL cycles)
      set(${out} trace-time PARENT_SCOPE)
      return()
    endif()
    if(NOT loaded_pc_opcode STREQUAL "" AND
       (NOT loaded_pc_refills EQUAL 1 OR NOT loaded_pc_targets EQUAL 1))
      set(${out} loaded-pc-context PARENT_SCOPE)
      return()
    endif()
    if(row_kind STREQUAL "guest-stop")
      if(NOT stop_fetches EQUAL 1 OR
         (expected_exception GREATER 0 AND (NOT exceptions EQUAL 1 OR NOT last_category EQUAL 4)) OR
         (expected_exception EQUAL 0 AND (NOT exceptions EQUAL 0 OR
          last_instruction_opcode STREQUAL expected_opcode)))
        set(${out} stop-operation PARENT_SCOPE)
        return()
      endif()
    elseif(NOT exceptions EQUAL 0)
      set(${out} unexpected-exception PARENT_SCOPE)
      return()
    endif()
    if(diagnostic_isa STREQUAL "thumb" AND
       (row_id STREQUAL "772491234" OR row_id STREQUAL "555469226" OR row_id STREQUAL "747712347"))
      check_thumb_bl(bl_finding "${trace_rows}" "${expected_state}")
      if(NOT bl_finding STREQUAL "ok")
        set(${out} "${bl_finding}" PARENT_SCOPE)
        return()
      endif()
    endif()
    set(${out} ok PARENT_SCOPE)
  endfunction()

  function(expect_arm_observation_refusal label expected doc trace_rows row_id
      row_kind expected_state expected_reason actual_sha)
    check_arm_observation(finding "${doc}" "${trace_rows}" "${row_id}"
      "${row_kind}" "${expected_state}" "${expected_reason}" "${actual_sha}")
    if(NOT finding STREQUAL expected)
      message(FATAL_ERROR
        "diagnostics-case: arm-control-failed:${label}: expected=${expected} actual=${finding}")
    endif()
    file(APPEND "${result_root}/controls.txt" "${label}=${finding}\n")
  endfunction()

  function(check_arm_observation_controls doc trace_rows row_id row_kind
      expected_state expected_reason actual_sha)
    if(expected_state STREQUAL "pass" AND
       (row_id STREQUAL "314196076" OR row_id STREQUAL "433134728"))
      foreach(control IN ITEMS refill target)
        set(changed_rows "")
        foreach(record IN LISTS trace_rows)
          string(JSON category GET "${record}" category)
          string(JSON value GET "${record}" value)
          string(JSON opcode GET "${record}" opcode)
          if(control STREQUAL "refill" AND category EQUAL 2 AND value STREQUAL "0xe3a00011")
            string(JSON record SET "${record}" access_class 1)
          elseif(control STREQUAL "target" AND category EQUAL 1 AND opcode STREQUAL "0xe3a00011")
            string(JSON record SET "${record}" opcode "\"0xe3a00022\"")
          endif()
          list(APPEND changed_rows "${record}")
        endforeach()
        expect_arm_observation_refusal("${row_id}-loaded-pc-${control}" loaded-pc-context
          "${doc}" "${changed_rows}" "${row_id}" "${row_kind}" "${expected_state}"
          "${expected_reason}" "${actual_sha}")
      endforeach()
    endif()
    if(diagnostic_isa STREQUAL "thumb" AND expected_state STREQUAL "pass" AND
       (row_id STREQUAL "772491234" OR row_id STREQUAL "555469226" OR row_id STREQUAL "747712347"))
      check_thumb_bl_controls("${doc}" "${trace_rows}" "${row_id}" "${actual_sha}")
    endif()
    if(row_kind STREQUAL "semantic")
      get_property(done GLOBAL PROPERTY ARM_SEMANTIC_CONTROLS_DONE)
      if(done OR NOT expected_state STREQUAL "pass")
        return()
      endif()
      foreach(field IN ITEMS status reason termination)
        string(JSON changed SET "${doc}" ${field} "\"PASS\"")
        expect_arm_observation_refusal("semantic-${field}" status "${changed}"
          "${trace_rows}" "${row_id}" "${row_kind}" "${expected_state}"
          "${expected_reason}" "${actual_sha}")
      endforeach()
      foreach(field IN ITEMS state case_id)
        string(JSON changed SET "${doc}" semantic ${field} "\"wrong\"")
        expect_arm_observation_refusal("semantic-${field}" semantic "${changed}"
          "${trace_rows}" "${row_id}" "${row_kind}" "${expected_state}"
          "${expected_reason}" "${actual_sha}")
      endforeach()
      string(JSON changed SET "${doc}" fixture id "\"unadmitted\"")
      expect_arm_observation_refusal(registered-identity identity "${changed}"
        "${trace_rows}" "${row_id}" "${row_kind}" "${expected_state}"
        "${expected_reason}" "${actual_sha}")
      string(JSON changed SET "${doc}" claim bios_provider "\"wrong\"")
      expect_arm_observation_refusal(claim-profile identity "${changed}"
        "${trace_rows}" "${row_id}" "${row_kind}" "${expected_state}"
        "${expected_reason}" "${actual_sha}")
      foreach(location IN ITEMS fixture claim)
        if(location STREQUAL "fixture")
          string(JSON changed SET "${doc}" fixture rom_sha256 "\"wrong\"")
        else()
          string(JSON changed SET "${doc}" claim cart rom_sha256 "\"wrong\"")
        endif()
        expect_arm_observation_refusal("${location}-sha" identity "${changed}"
          "${trace_rows}" "${row_id}" "${row_kind}" "${expected_state}"
          "${expected_reason}" "${actual_sha}")
      endforeach()
      string(JSON changed SET "${doc}" time cycles "\"0\"")
      expect_arm_observation_refusal(zero-time time "${changed}" "${trace_rows}"
        "${row_id}" "${row_kind}" "${expected_state}" "${expected_reason}" "${actual_sha}")
      foreach(field IN ITEMS complete count)
        string(JSON changed SET "${doc}" trace ${field} false)
        expect_arm_observation_refusal("trace-${field}" trace-metadata "${changed}"
          "${trace_rows}" "${row_id}" "${row_kind}" "${expected_state}"
          "${expected_reason}" "${actual_sha}")
      endforeach()
      list(GET trace_rows 0 first)
      string(JSON changed_record SET "${first}" sequence "\"99\"")
      list(REMOVE_AT trace_rows 0)
      list(PREPEND trace_rows "${changed_record}")
      expect_arm_observation_refusal(trace-sequence trace-order "${doc}"
        "${trace_rows}" "${row_id}" "${row_kind}" "${expected_state}"
        "${expected_reason}" "${actual_sha}")
      set_property(GLOBAL PROPERTY ARM_SEMANTIC_CONTROLS_DONE TRUE)
    else()
      # Every stop gets its own clean-first context controls. A reason token
      # cannot substitute for the real fetch/operation/exception witness.
      list(GET trace_rows 0 first)
      string(JSON changed_record SET "${first}" address "\"0x00000008\"")
      set(changed_rows "${trace_rows}")
      list(REMOVE_AT changed_rows 0)
      list(PREPEND changed_rows "${changed_record}")
      expect_arm_observation_refusal("${row_id}-vector-fetch" vector-fetch "${doc}"
        "${changed_rows}" "${row_id}" "${row_kind}" "${expected_state}"
        "${expected_reason}" "${actual_sha}")
      set(changed_rows "")
      foreach(record IN LISTS trace_rows)
        string(JSON category GET "${record}" category)
        if(row_id STREQUAL "448472880" OR expected_reason STREQUAL "unpredictable-instruction")
          string(JSON value GET "${record}" value)
          if(category EQUAL 2 AND (value STREQUAL "0xf0000000" OR value MATCHES "^0x0000(b400|bc00|c000|c800|c107|c207)$"))
            string(JSON record SET "${record}" value "\"0xe1a00000\"")
          endif()
          set(expected_finding stop-operation)
        elseif(category EQUAL 4)
          string(JSON record SET "${record}" exception_kind 0)
          set(expected_finding exception-context)
        endif()
        list(APPEND changed_rows "${record}")
      endforeach()
      expect_arm_observation_refusal("${row_id}-wrong-operation-context"
        "${expected_finding}" "${doc}" "${changed_rows}" "${row_id}"
        "${row_kind}" "${expected_state}" "${expected_reason}" "${actual_sha}")
    endif()
  endfunction()

  function(run_arm_pre_admission rom_name row_id row_kind expected_state expected_reason
      proof_variant)
    set(rom "${artifact_root}/roms/${rom_name}")
    set(case_receipt_path "${artifact_root}/cases/${row_id}/proof/${proof_variant}/build-receipt.json")
    if(NOT EXISTS "${rom}" OR IS_DIRECTORY "${rom}" OR IS_SYMLINK "${rom}" OR
       NOT EXISTS "${case_receipt_path}" OR IS_DIRECTORY "${case_receipt_path}" OR
       IS_SYMLINK "${case_receipt_path}")
      message(FATAL_ERROR "diagnostics-case: arm-pair-artifact-missing: ${rom_name}")
    endif()
    file(SHA256 "${rom}" actual_rom_sha)
    file(SIZE "${rom}" actual_rom_bytes)
    file(READ "${case_receipt_path}" case_build_json)
    string(JSON case_receipt_id ERROR_VARIABLE case_id_error GET "${case_build_json}" case_id)
    string(JSON case_receipt_kind ERROR_VARIABLE case_kind_error GET "${case_build_json}" source_kind)
    string(JSON case_receipt_sha ERROR_VARIABLE case_sha_error GET "${case_build_json}" rom_sha256)
    string(JSON case_receipt_bytes ERROR_VARIABLE case_bytes_error GET "${case_build_json}" rom_bytes)
    string(JSON case_receipt_commit ERROR_VARIABLE case_commit_error GET "${case_build_json}" source_commit)
    foreach(error IN ITEMS case_id_error case_kind_error case_sha_error
        case_bytes_error case_commit_error)
      if(NOT "${${error}}" STREQUAL "NOTFOUND")
        message(FATAL_ERROR "diagnostics-case: arm-case-receipt-invalid:${rom_name}")
      endif()
    endforeach()
    if(NOT "${case_receipt_id}" STREQUAL "${row_id}" OR
       NOT "${case_receipt_kind}" STREQUAL "${row_kind}" OR
       NOT "${case_receipt_sha}" STREQUAL "${actual_rom_sha}" OR
       NOT "${case_receipt_bytes}" EQUAL "${actual_rom_bytes}" OR
       NOT "${case_receipt_commit}" STREQUAL "${receipt_commit}")
      message(FATAL_ERROR "diagnostics-case: arm-case-receipt-identity-mismatch:${rom_name}")
    endif()

    set(trace_path "${result_root}/${rom_name}.trace.jsonl")
    execute_process(
      COMMAND "${runner}" --rom "${rom}" --cycles 1000 --trace-mask 7
        --trace-capacity 4096 --trace-output "${trace_path}"
      WORKING_DIRECTORY "${source_root}"
      RESULT_VARIABLE runner_rc OUTPUT_VARIABLE runner_json ERROR_VARIABLE runner_error)
    if(NOT "${runner_rc}" STREQUAL "1" OR "${runner_json}" STREQUAL "")
      message(FATAL_ERROR "diagnostics-case: arm-raw-runner-failed:${rom_name}: ${runner_error}${runner_json}")
    endif()
    file(WRITE "${result_root}/${rom_name}.json" "${runner_json}")
    # The canonical checker deliberately refuses raw content's null fixture ID
    # and empty evidence claim. Require exactly that admission refusal; never
    # insert synthetic authority merely to make an unregistered result pass.
    # Reuse the canonical authority already parsed above. Every real result
    # still runs the canonical checker; reloading722 rows for every ROM would
    # multiply identical authority work without adding evidence.
    set_property(GLOBAL PROPERTY GBL_FINDINGS 0)
    gbl_result_check("${result_root}/${rom_name}.json" M R V)
    get_property(finding_count GLOBAL PROPERTY GBL_FINDINGS)
    string(JSON raw_fixture_type TYPE "${runner_json}" fixture id)
    string(JSON raw_evidence_count LENGTH "${runner_json}" claim evidence)
    if(NOT finding_count EQUAL 2 OR NOT raw_fixture_type STREQUAL "NULL" OR
       NOT raw_evidence_count EQUAL 0)
      message(FATAL_ERROR "diagnostics-case: arm-canonical-admission-refusal-mismatch:${rom_name}")
    endif()
    set_property(GLOBAL PROPERTY GBL_FINDINGS 0)
    file(WRITE "${result_root}/${rom_name}.checker.txt"
      "canonical CheckResult function: missing fixture.id and empty claim.evidence; findings=2\n")
    file(STRINGS "${trace_path}" trace_rows)
    check_arm_observation(observation "${runner_json}" "${trace_rows}"
      "${row_id}" "${row_kind}" "${expected_state}" "${expected_reason}" "${actual_rom_sha}")
    if(NOT observation STREQUAL "ok")
      message(FATAL_ERROR "diagnostics-case: arm-observation-${observation}:${rom_name}")
    endif()
    if(diagnostic_isa STREQUAL "thumb" AND
       (row_id STREQUAL "772491234" OR row_id STREQUAL "555469226" OR row_id STREQUAL "747712347"))
      get_property(bl_receipt GLOBAL PROPERTY THUMB_BL_LAST_RECEIPT)
      file(WRITE "${result_root}/${rom_name}.bl-observation.json" "${bl_receipt}\n")
    endif()
    check_arm_observation_controls("${runner_json}" "${trace_rows}" "${row_id}"
      "${row_kind}" "${expected_state}" "${expected_reason}" "${actual_rom_sha}")
  endfunction()

  string(JSON case_count LENGTH "${matrix_json}" cases)
  math(EXPR case_last "${case_count}-1")
  foreach(index RANGE 0 ${case_last})
    string(JSON row_json GET "${matrix_json}" cases ${index})
    string(JSON row_id GET "${row_json}" id)
    string(JSON row_isa GET "${row_json}" isa)
    string(JSON source_kind ERROR_VARIABLE source_kind_error GET
      "${row_json}" source_test kind)
    if(NOT "${source_kind_error}" STREQUAL "NOTFOUND" OR
       NOT row_isa STREQUAL diagnostic_isa OR
       NOT source_kind MATCHES "^(semantic|guest-stop)$")
      continue()
    endif()
    if(source_kind STREQUAL "semantic")
      foreach(name IN ITEMS "${diagnostic_isa}-${row_id}.gba" "${diagnostic_isa}-${row_id}-broken.gba")
        list(APPEND expected_names "${name}")
        if(name MATCHES "-broken[.]gba$")
          set(state fail)
          set(variant "${diagnostic_isa}-${row_id}-broken")
          math(EXPR semantic_broken_count "${semantic_broken_count}+1")
        else()
          set(state pass)
          set(variant "${diagnostic_isa}-${row_id}")
          math(EXPR semantic_positive_count "${semantic_positive_count}+1")
        endif()
        run_arm_pre_admission("${name}" "${row_id}" semantic "${state}"
          unregistered-content "${variant}")
      endforeach()
    else()
      string(JSON stop_reason GET "${row_json}" source_test expected_reason)
      set(name "${diagnostic_isa}-stop-${row_id}.gba")
      set(variant "${diagnostic_isa}-stop-${row_id}")
      list(APPEND expected_names "${name}")
      run_arm_pre_admission("${name}" "${row_id}" guest-stop pending
        "${stop_reason}" "${variant}")
      math(EXPR stop_witness_count "${stop_witness_count}+1")
    endif()
  endforeach()
  if(NOT semantic_positive_count EQUAL expected_semantics OR
     NOT semantic_broken_count EQUAL expected_semantics OR NOT stop_witness_count EQUAL expected_stops)
    message(FATAL_ERROR
      "diagnostics-case: arm-observation-count-mismatch: positive=${semantic_positive_count} broken=${semantic_broken_count} stops=${stop_witness_count}")
  endif()
  list(SORT expected_names)
  set(receipt_names "")
  math(EXPR receipt_names_last "${receipt_output_count}-1")
  foreach(index RANGE 0 ${receipt_names_last})
    string(JSON name GET "${matrix_build_json}" output_names ${index})
    list(APPEND receipt_names "${name}")
  endforeach()
  list(SORT receipt_names)
  if(NOT "${receipt_names}" STREQUAL "${expected_names}")
    message(FATAL_ERROR "diagnostics-case: arm-matrix-output-set-mismatch")
  endif()
  file(GLOB actual_names RELATIVE "${artifact_root}/roms" "${artifact_root}/roms/*")
  list(SORT actual_names)
  if(NOT actual_names STREQUAL expected_names)
    message(FATAL_ERROR "diagnostics-case: arm-rom-directory-set-mismatch")
  endif()
  file(STRINGS "${result_root}/controls.txt" observed_controls)
  list(LENGTH observed_controls control_count)
  if(NOT control_count EQUAL expected_controls)
    message(FATAL_ERROR "diagnostics-case: arm-observation-controls-missing")
  endif()
  file(WRITE "${result_root}/summary.txt"
    "source.commit=${receipt_commit}\nsemantic.positive=${semantic_positive_count}\nsemantic.broken=${semantic_broken_count}\nstop.witnesses=${stop_witness_count}\nraw.status=GUEST_STOP\nsemantic.admission=unregistered-content\nresult.contract=unregistered-admission-refused\nraw.observation=checked\ntrace.context=checked\nvector.fetch=none\nobservation.controls=${control_count}\n")
  gbl_diagnostic_result_authority("${source_root}")
  message("diagnostics-case: ok (${expected_semantics} raw ${diagnostic_isa} pass/fail pairs; ${expected_stops} named guest stops; all unregistered)")
  return()
endif()
if(NOT CASE STREQUAL "smoke-pair")
  message(FATAL_ERROR "diagnostics-case: unsupported-case")
endif()
if(NOT DEFINED TOOLCHAIN_ROOT OR "${TOOLCHAIN_ROOT}" STREQUAL "")
  message(FATAL_ERROR "diagnostics-case: missing-TOOLCHAIN_ROOT")
endif()

get_filename_component(source_root "${SRC}" ABSOLUTE)
get_filename_component(binary_root "${BIN}" ABSOLUTE)
get_filename_component(toolchain_root "${TOOLCHAIN_ROOT}" ABSOLUTE)
set(manifest "${source_root}/tests/fixtures/manifest.json")
set(registry "${source_root}/registry/disputes.json")
set(vocab "${source_root}/contracts/evidence.json")
set(notice "${source_root}/diagnostics/armv4t/NOTICE")
set(inputs "${source_root}/diagnostics/armv4t/inputs.txt")
set(runner "${binary_root}/gamebloke-run")
set(artifact_root "${binary_root}/generated/diagnostics/armv4t")
set(result_root "${binary_root}/diagnostics-results")

foreach(path IN ITEMS "${manifest}" "${registry}" "${vocab}" "${notice}"
    "${inputs}" "${source_root}/cmake/GenerateFixtureIndex.cmake"
    "${source_root}/cmake/CheckResult.cmake")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}" OR IS_SYMLINK "${path}")
    message(FATAL_ERROR "diagnostics-case: required-input-missing: ${path}")
  endif()
endforeach()
if(NOT EXISTS "${runner}" OR IS_DIRECTORY "${runner}" OR IS_SYMLINK "${runner}")
  message(FATAL_ERROR "diagnostics-case: production-runner-missing")
endif()

include("${source_root}/cmake/CheckManifest.cmake")
gbl_contract_read(V "${vocab}")
gbl_registry_read(R "${registry}" V)
gbl_manifest_read(M "${manifest}" R V)
gbl_json_load(manifest_doc "${manifest}")

function(gbl_diagnostics_find_row out doc wanted_id)
  gbl_json_length(row_count "${doc}" fixtures)
  set(found -1)
  if(row_count GREATER 0)
    math(EXPR row_last "${row_count}-1")
    foreach(index RANGE ${row_last})
      gbl_json_string(row_id "${doc}" fixtures ${index} id)
      if("${row_id}" STREQUAL "${wanted_id}")
        set(found "${index}")
        break()
      endif()
    endforeach()
  endif()
  set(${out} "${found}" PARENT_SCOPE)
endfunction()

set(fixture_ids smoke smoke-broken)
set(source_commit "")
foreach(fixture_id IN LISTS fixture_ids)
  gbl_diagnostics_find_row(row_index "${manifest_doc}" "${fixture_id}")
  if(row_index LESS 0)
    message(FATAL_ERROR
      "diagnostics-case: missing-production-fixture-row: ${fixture_id}")
  endif()

  gbl_json_string(source_kind "${manifest_doc}" fixtures ${row_index} source kind)
  gbl_json_string(row_commit "${manifest_doc}" fixtures ${row_index} source commit)
  gbl_json_string(artifact_path "${manifest_doc}" fixtures ${row_index}
    build artifact path)
  gbl_json_string(artifact_sha "${manifest_doc}" fixtures ${row_index}
    build artifact sha256)
  gbl_json_string(license_id "${manifest_doc}" fixtures ${row_index}
    license spdx)
  gbl_json_string(redistributable "${manifest_doc}" fixtures ${row_index}
    license redistributable)
  gbl_json_string(bios_requirement "${manifest_doc}" fixtures ${row_index}
    bios_requirement)
  gbl_json_string(provenance_class "${manifest_doc}" fixtures ${row_index}
    expected provenance_class)
  gbl_json_string(recipe "${manifest_doc}" fixtures ${row_index} build recipe)
  gbl_json_string(toolchain_identity "${manifest_doc}" fixtures ${row_index}
    build toolchain identity)
  gbl_json_type(toolchain_sha_type "${manifest_doc}" fixtures ${row_index}
    build toolchain sha256)
  gbl__manifest_is_hex(commit_is_hex "${row_commit}" 40)
  gbl__manifest_is_hex(artifact_is_hex "${artifact_sha}" 64)
  set(toolchain_sha "")
  if("${toolchain_sha_type}" STREQUAL "STRING")
    gbl_json_string(toolchain_sha "${manifest_doc}" fixtures ${row_index}
      build toolchain sha256)
    gbl__manifest_is_hex(toolchain_sha_is_hex "${toolchain_sha}" 64)
  else()
    set(toolchain_sha_is_hex FALSE)
  endif()

  set(expected_artifact_path
    "generated/diagnostics/armv4t/roms/${fixture_id}.gba")
  if(NOT "${source_kind}" STREQUAL "git" OR
     NOT commit_is_hex OR
     NOT "${artifact_path}" STREQUAL "${expected_artifact_path}" OR
     NOT artifact_is_hex OR
     NOT "${license_id}" MATCHES "^LicenseRef-Gamebloke-" OR
     NOT "${redistributable}" STREQUAL "yes" OR
     NOT "${bios_requirement}" STREQUAL "none" OR
     NOT "${provenance_class}" STREQUAL "project-authored" OR
     NOT "${toolchain_identity}" MATCHES "xPack GNU Arm Embedded GCC 15\.2\.1-1\.1" OR
     (NOT "${toolchain_sha_type}" STREQUAL "NULL" AND
      NOT toolchain_sha_is_hex) OR
     ("${toolchain_sha_type}" STREQUAL "STRING" AND
      "${toolchain_sha}" STREQUAL "${artifact_sha}"))
    message(FATAL_ERROR "diagnostics-case: fixture-authority-invalid: ${fixture_id}")
  endif()
  if(NOT "${source_commit}" STREQUAL "" AND
     NOT "${source_commit}" STREQUAL "${row_commit}")
    message(FATAL_ERROR "diagnostics-case: source-revision-mismatch")
  endif()
  set(source_commit "${row_commit}")

  gbl_json_length(notice_count "${manifest_doc}" fixtures ${row_index}
    license notices)
  set(notice_listed FALSE)
  if(notice_count GREATER 0)
    math(EXPR notice_last "${notice_count}-1")
    foreach(notice_index RANGE ${notice_last})
      gbl_json_string(notice_ref "${manifest_doc}" fixtures ${row_index}
        license notices ${notice_index})
      if("${notice_ref}" STREQUAL "diagnostics/armv4t/NOTICE")
        set(notice_listed TRUE)
      endif()
    endforeach()
  endif()
  if(NOT notice_listed)
    message(FATAL_ERROR "diagnostics-case: fixture-notice-missing: ${fixture_id}")
  endif()

  gbl_json_length(use_count "${manifest_doc}" fixtures ${row_index} permitted_use)
  set(public_ci_allowed FALSE)
  if(use_count GREATER 0)
    math(EXPR use_last "${use_count}-1")
    foreach(use_index RANGE ${use_last})
      gbl_json_string(permitted_use "${manifest_doc}" fixtures ${row_index}
        permitted_use ${use_index})
      if("${permitted_use}" STREQUAL "public-ci")
        set(public_ci_allowed TRUE)
      endif()
    endforeach()
  endif()
  if(NOT public_ci_allowed)
    message(FATAL_ERROR "diagnostics-case: public-ci-use-missing: ${fixture_id}")
  endif()

  set(expected_sha_name "expected_sha_${fixture_id}")
  set(${expected_sha_name} "${artifact_sha}")
endforeach()

if("${expected_sha_smoke}" STREQUAL "${expected_sha_smoke-broken}")
  message(FATAL_ERROR "diagnostics-case: positive-and-broken-identical")
endif()

set(build_output_root "${source_root}/build/phase05-spike/05-05-admission")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${source_root}"
    "-DOUT=${build_output_root}"
    "-DTOOLCHAIN_ROOT=${toolchain_root}"
    -DMODE=case
    -DCASE_ID=20481
    "-DSOURCE_COMMIT=${source_commit}"
    -P "${source_root}/cmake/BuildArmDiagnostics.cmake"
  WORKING_DIRECTORY "${source_root}"
  RESULT_VARIABLE build_rc OUTPUT_VARIABLE build_out ERROR_VARIABLE build_error)
if(NOT "${build_rc}" STREQUAL "0")
  message(FATAL_ERROR
    "diagnostics-case: source-build-failed: ${build_error}${build_out}")
endif()
set(common_startup_object "${build_output_root}/proof/common/startup.o")
set(common_identity "${build_output_root}/proof/common/object-identity.txt")
if(NOT EXISTS "${common_startup_object}" OR IS_DIRECTORY "${common_startup_object}" OR
   NOT EXISTS "${common_identity}")
  message(FATAL_ERROR "diagnostics-case: common-startup-object-missing")
endif()
file(STRINGS "${common_identity}" common_identity_lines)
if(NOT common_identity_lines MATCHES "Reused for the clean and assertion-broken")
  message(FATAL_ERROR "diagnostics-case: common-startup-reuse-unproven")
endif()
set(common_map_line
  "project-object build/phase05-spike/05-05-admission/proof/common/startup.o")
foreach(variant IN ITEMS smoke smoke-broken)
  set(map_inputs "${build_output_root}/proof/${variant}/map-inputs.txt")
  if(NOT EXISTS "${map_inputs}")
    message(FATAL_ERROR "diagnostics-case: map-input-proof-missing: ${variant}")
  endif()
  file(STRINGS "${map_inputs}" map_lines)
  list(FIND map_lines "${common_map_line}" common_map_found)
  if(common_map_found EQUAL -1)
    message(FATAL_ERROR "diagnostics-case: common-object-not-shared: ${variant}")
  endif()
endforeach()
file(MAKE_DIRECTORY "${artifact_root}/roms")
foreach(fixture_id IN LISTS fixture_ids)
  set(source_image "${build_output_root}/roms/${fixture_id}.gba")
  set(artifact_image "${binary_root}/generated/diagnostics/armv4t/roms/${fixture_id}.gba")
  set(expected_sha_name "expected_sha_${fixture_id}")
  set(expected_sha "${${expected_sha_name}}")
  if(NOT EXISTS "${source_image}" OR IS_DIRECTORY "${source_image}" OR
     IS_SYMLINK "${source_image}")
    message(FATAL_ERROR "diagnostics-case: built-image-missing: ${fixture_id}")
  endif()
  file(SHA256 "${source_image}" source_sha)
  if(NOT "${source_sha}" STREQUAL "${expected_sha}")
    message(FATAL_ERROR
      "diagnostics-case: built-image-digest-mismatch: ${fixture_id}")
  endif()
  if(IS_SYMLINK "${artifact_image}" OR
     (EXISTS "${artifact_image}" AND IS_DIRECTORY "${artifact_image}"))
    message(FATAL_ERROR "diagnostics-case: unsafe-artifact-path: ${fixture_id}")
  endif()
  set(copy_artifact TRUE)
  if(EXISTS "${artifact_image}")
    file(SHA256 "${artifact_image}" actual_artifact_sha)
    if("${actual_artifact_sha}" STREQUAL "${expected_sha}")
      set(copy_artifact FALSE)
    endif()
  endif()
  if(copy_artifact)
    file(COPY_FILE "${source_image}" "${artifact_image}" ONLY_IF_DIFFERENT)
  endif()
endforeach()
file(MAKE_DIRECTORY "${result_root}")
foreach(fixture_id IN LISTS fixture_ids)
  set(artifact_image
    "${binary_root}/generated/diagnostics/armv4t/roms/${fixture_id}.gba")
  set(expected_sha_name "expected_sha_${fixture_id}")
  set(expected_sha "${${expected_sha_name}}")
  file(SHA256 "${artifact_image}" actual_artifact_sha)
  if(NOT "${actual_artifact_sha}" STREQUAL "${expected_sha}")
    message(FATAL_ERROR
      "diagnostics-case: artifact-digest-mismatch: ${fixture_id}")
  endif()
endforeach()

# Regenerate the canonical production index with its explicit artifact root,
# then rebuild the production runner so it consumes that immutable header.
set(generated "${binary_root}/generated")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DGBL_SOURCE_DIR=${source_root}"
    "-DGBL_OUTPUT_ROOT=${binary_root}"
    "-DGBL_ARTIFACT_ROOT=${binary_root}"
    "-DGBL_CONTROL_IMAGE=${generated}/fixtures/owned-arm-pass.gba"
    "-DGBL_CONTROL_FAIL_IMAGE=${generated}/fixtures/owned-arm-fail.gba"
    "-DGBL_CONTROL_SKIP_IMAGE=${generated}/fixtures/owned-arm-pass.gba"
    "-DGBL_CONTROL_ZERO_CASE_IMAGE=${generated}/fixtures/owned-arm-pass-zero-case.gba"
    "-DGBL_CONTROL_MANIFEST_OUTPUT=${generated}/control/manifest.json"
    "-DGBL_MANIFEST=${manifest}"
    "-DGBL_REGISTRY=${registry}"
    "-DGBL_VOCAB=${vocab}"
    "-DGBL_OUT_PRODUCTION=${generated}/production/fixture_index.h"
    "-DGBL_OUT_CONTROL=${generated}/control/fixture_index.h"
    -P "${source_root}/cmake/GenerateFixtureIndex.cmake"
  WORKING_DIRECTORY "${source_root}"
  RESULT_VARIABLE index_rc OUTPUT_VARIABLE index_out ERROR_VARIABLE index_error)
if(NOT "${index_rc}" STREQUAL "0")
  message(FATAL_ERROR "diagnostics-case: production-index-failed: ${index_error}${index_out}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${binary_root}" --target gamebloke-run
  WORKING_DIRECTORY "${source_root}"
  RESULT_VARIABLE runner_build_rc OUTPUT_VARIABLE runner_build_out
  ERROR_VARIABLE runner_build_error)
if(NOT "${runner_build_rc}" STREQUAL "0")
  message(FATAL_ERROR
    "diagnostics-case: production-runner-build-failed: ${runner_build_error}${runner_build_out}")
endif()

foreach(fixture_id IN LISTS fixture_ids)
  if(fixture_id STREQUAL "smoke")
    set(expected_status PASS)
    set(expected_semantic pass)
    set(expected_reason guest-mailbox-pass)
    set(expected_exit 0)
  else()
    set(expected_status GUEST_FAIL)
    set(expected_semantic fail)
    set(expected_reason guest-mailbox-fail)
    set(expected_exit 1)
  endif()
  set(rom "${binary_root}/generated/diagnostics/armv4t/roms/${fixture_id}.gba")
  set(result_file "${result_root}/${fixture_id}.json")
  execute_process(
    COMMAND "${runner}" --rom "${rom}" --cycles 1000 --fixture "${fixture_id}"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE runner_rc OUTPUT_VARIABLE runner_json ERROR_VARIABLE runner_error)
  if(NOT "${runner_rc}" STREQUAL "${expected_exit}" OR
     "${runner_json}" STREQUAL "")
    message(FATAL_ERROR
      "diagnostics-case: runner-exit-mismatch-${fixture_id}: ${runner_error}${runner_json}")
  endif()
  file(WRITE "${result_file}" "${runner_json}")

  string(JSON status ERROR_VARIABLE status_error GET "${runner_json}" status)
  string(JSON reason ERROR_VARIABLE reason_error GET "${runner_json}" reason)
  string(JSON ran ERROR_VARIABLE ran_error GET "${runner_json}" fixture ran)
  string(JSON loaded_sha ERROR_VARIABLE loaded_error GET "${runner_json}"
    fixture rom_sha256)
  string(JSON semantic ERROR_VARIABLE semantic_error GET "${runner_json}"
    semantic state)
  string(JSON case_id ERROR_VARIABLE case_error GET "${runner_json}"
    semantic case_id)
  string(JSON termination ERROR_VARIABLE termination_error GET "${runner_json}"
    termination)
  string(JSON expected_sha ERROR_VARIABLE expected_error GET "${runner_json}"
    expected_sha256)
  string(JSON cycles ERROR_VARIABLE cycles_error GET "${runner_json}"
    time cycles)
  string(JSON instructions ERROR_VARIABLE instructions_error GET "${runner_json}"
    time instructions)
  string(JSON budget_cycles ERROR_VARIABLE budget_error GET "${runner_json}"
    time budget_cycles)
  foreach(error IN ITEMS status_error reason_error ran_error loaded_error
      semantic_error case_error termination_error expected_error cycles_error
      instructions_error budget_error)
    if(NOT "${${error}}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "diagnostics-case: runner-result-invalid-${fixture_id}")
    endif()
  endforeach()
  set(expected_sha_name "expected_sha_${fixture_id}")
  set(manifest_sha "${${expected_sha_name}}")
  if(NOT "${status}" STREQUAL "${expected_status}" OR
     NOT "${reason}" STREQUAL "${expected_reason}" OR
     NOT "${ran}" STREQUAL "ON" OR
     NOT "${loaded_sha}" STREQUAL "${manifest_sha}" OR
     NOT "${expected_sha}" STREQUAL "${manifest_sha}" OR
     NOT "${semantic}" STREQUAL "${expected_semantic}" OR
     NOT "${case_id}" STREQUAL "20481" OR
     NOT "${termination}" STREQUAL "mailbox-complete" OR
     NOT "${cycles}" MATCHES "^[0-9]+$" OR
     NOT "${instructions}" MATCHES "^[0-9]+$" OR
     NOT "${budget_cycles}" STREQUAL "1000" OR
     cycles LESS 1 OR instructions LESS 1 OR cycles GREATER budget_cycles)
    message(FATAL_ERROR
      "diagnostics-case: semantic-or-identity-mismatch-${fixture_id}: ${runner_json}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DRESULT=${result_file}"
      "-DMANIFEST=${manifest}"
      "-DREGISTRY=${registry}"
      "-DVOCAB=${vocab}"
      -P "${source_root}/cmake/CheckResult.cmake"
    WORKING_DIRECTORY "${source_root}"
    RESULT_VARIABLE result_rc OUTPUT_VARIABLE result_out ERROR_VARIABLE result_error)
  if(NOT "${result_rc}" STREQUAL "0")
    message(FATAL_ERROR
      "diagnostics-case: canonical-result-rejected-${fixture_id}: ${result_error}${result_out}")
  endif()
  message(STATUS
    "diagnostics-case: ${fixture_id} ${status} semantic=${semantic}/${case_id} sha256=${loaded_sha}")
endforeach()

message("diagnostics-case: ok (production smoke pair; exact PASS/GUEST_FAIL)")
