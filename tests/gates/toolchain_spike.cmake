cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS CASE SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "toolchain-spike: missing-${required}")
  endif()
endforeach()
if(CASE STREQUAL "final-producer-tracer")
  # Execute the public smoke recipe in a fresh isolated output root. A zero
  # process status without its promised bytes cannot satisfy final proof.
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DSRC=${SRC}"
    -DOUT=build/phase05-spike/plan21-smoke-tracer
    -DTOOLCHAIN_ROOT=build/toolchains -DMODE=smoke
    -P "${SRC}/cmake/BuildArmDiagnostics.cmake"
    WORKING_DIRECTORY "${SRC}" RESULT_VARIABLE rc
    OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "toolchain-spike: final-producer-contract: smoke recipe failed: ${output}${error}")
  endif()
  foreach(name IN ITEMS smoke.gba smoke-broken.gba)
    if(NOT EXISTS "${SRC}/build/phase05-spike/plan21-smoke-tracer/roms/${name}")
      message(FATAL_ERROR "toolchain-spike: final-producer-contract: successful smoke recipe omitted ${name}")
    endif()
  endforeach()
  message(STATUS "toolchain-spike: final-producer-contract passed")
  return()
endif()
if(CASE MATCHES "^final-(host-build|host-set|output-proof|hosted-proof|proof-controls)$")
  include("${SRC}/tests/gates/final_diagnostic_proof.cmake")
  return()
endif()
if(NOT CASE MATCHES "^(offline-controls|local-proof|proof-only|hosted-proof)$")
  message(FATAL_ERROR "toolchain-spike: unsupported-case")
endif()
file(REAL_PATH "${SRC}" source_root)
set(bin_request "${BIN}")
cmake_path(ABSOLUTE_PATH bin_request BASE_DIRECTORY "${source_root}"
  NORMALIZE OUTPUT_VARIABLE bin_root)
set(spike_root "${source_root}/build/phase05-spike")
cmake_path(IS_PREFIX spike_root "${bin_root}" NORMALIZE bin_under_spike)
if(NOT bin_under_spike OR IS_SYMLINK "${bin_root}" OR
   IS_SYMLINK "${source_root}/build/phase05-spike")
  message(FATAL_ERROR "toolchain-spike: output-outside-ignored-root")
endif()
file(MAKE_DIRECTORY "${bin_root}")
set(linkmap_gate "${source_root}/tests/gates/diagnostic_linkmap.cmake")

function(run_linkmap label map_path object_paths attr_paths section_path
    disassembly_path expected)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DMAP=${map_path}"
      "-DOBJECTS=${object_paths}" "-DATTRIBUTES=${attr_paths}"
      "-DSECTIONS=${section_path}" "-DDISASSEMBLY=${disassembly_path}"
      "-DSRC=${source_root}" -P "${linkmap_gate}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  set(report "${stdout}\n${stderr}")
  if(expected STREQUAL "pass")
    if(NOT "${rc}" STREQUAL "0" OR NOT report MATCHES "linkmap: ok")
      message(FATAL_ERROR
        "toolchain-spike: ${label}: clean map should pass; exit=${rc}: ${report}")
    endif()
  elseif("${rc}" STREQUAL "0" OR NOT report MATCHES "${expected}")
    message(FATAL_ERROR
      "toolchain-spike: ${label}: expected ${expected}; exit=${rc}: ${report}")
  endif()
  message(STATUS "toolchain-spike: ${label} ${expected}")
endfunction()

if(CASE STREQUAL "offline-controls")
  file(REMOVE_RECURSE "${bin_root}/controls/offline")
  set(control_root "${bin_root}/controls/offline")
  file(MAKE_DIRECTORY "${control_root}/objects" "${control_root}/attributes"
    "${control_root}/signatures")
  set(startup "${control_root}/objects/startup.o")
  set(smoke "${control_root}/objects/smoke.o")
  file(WRITE "${startup}" "synthetic startup object name only\n")
  file(WRITE "${smoke}" "synthetic smoke object name only\n")
  set(objects "${startup};${smoke}")
  set(map "${control_root}/clean.map")
  file(WRITE "${map}" "LOAD ${startup}\nLOAD ${smoke}\nLOAD linker stubs\n")
  set(attributes "")
  foreach(name IN ITEMS startup smoke elf)
    set(path "${control_root}/attributes/${name}.txt")
    file(WRITE "${path}"
      "Tag_CPU_name: ARM7TDMI\nTag_CPU_arch: v4T\nTag_ARM_ISA_use: Yes\nTag_THUMB_ISA_use: Thumb-1\n")
    list(APPEND attributes "${path}")
  endforeach()
  set(sections "${control_root}/sections.txt")
  file(WRITE "${sections}"
    "  [ 1] .gba_header PROGBITS 08000000 001000 0000c0 00  AX  0 0 4\n  [ 2] .text PROGBITS 080000c0 0010c0 000078 00  AX  0 0 4\n  [ 3] .ARM.attributes ARM_ATTRIBUTES 00000000 001138 00001a 00      0 0 1\n")
  set(disassembly "${control_root}/disassembly.txt")
  file(WRITE "${disassembly}"
    "Disassembly of section .gba_header:\n08000000 <_start>:\n  b 080000c0\nDisassembly of section .text:\n080000c0 <diagnostic_main>:\n  bx r3\n08000120 <thumb_probe>:\n  bx lr\n")
  run_linkmap("clean-map" "${map}" "${objects}" "${attributes}"
    "${sections}" "${disassembly}" "pass")
  file(WRITE "${control_root}/runtime.map"
    "LOAD ${startup}\nLOAD crt0.o\nLOAD ${smoke}\nLOAD linker stubs\n")
  run_linkmap("runtime-injection" "${control_root}/runtime.map"
    "${objects}" "${attributes}" "${sections}" "${disassembly}"
    "linkmap: runtime-input")
  file(WRITE "${control_root}/archive.map"
    "LOAD ${startup}\nLOAD ${smoke}\nLOAD libhelpers.a(member.o)\nLOAD linker stubs\n")
  run_linkmap("archive-member" "${control_root}/archive.map"
    "${objects}" "${attributes}" "${sections}" "${disassembly}"
    "linkmap: archive-input")
  file(WRITE "${control_root}/missing.map"
    "LOAD ${startup}\nLOAD linker stubs\n")
  run_linkmap("missing-object" "${control_root}/missing.map"
    "${objects}" "${attributes}" "${sections}" "${disassembly}"
    "linkmap: map-input-set-mismatch")
  file(WRITE "${control_root}/extra.o" "synthetic unlisted input name only\n")
  file(WRITE "${control_root}/extra.map"
    "LOAD ${startup}\nLOAD ${smoke}\nLOAD ${control_root}/extra.o\nLOAD linker stubs\n")
  run_linkmap("extra-object" "${control_root}/extra.map"
    "${objects}" "${attributes}" "${sections}" "${disassembly}"
    "linkmap: map-input-set-mismatch")

  file(WRITE "${control_root}/attributes/elf-wrong.txt"
    "Tag_CPU_name: ARM7TDMI\nTag_CPU_arch: v5TE\nTag_THUMB_ISA_use: Thumb-1\n")
  set(wrong_attributes
    "${control_root}/attributes/startup.txt;${control_root}/attributes/smoke.txt;${control_root}/attributes/elf-wrong.txt")
  run_linkmap("wrong-elf-architecture" "${map}" "${objects}"
    "${wrong_attributes}" "${sections}" "${disassembly}"
    "linkmap: architecture-cpu")
  file(WRITE "${control_root}/attributes/elf-missing.txt"
    "Tag_CPU_name: ARM7TDMI\nTag_ARM_ISA_use: Yes\nTag_THUMB_ISA_use: Thumb-1\n")
  set(missing_attributes
    "${control_root}/attributes/startup.txt;${control_root}/attributes/smoke.txt;${control_root}/attributes/elf-missing.txt")
  run_linkmap("missing-elf-architecture" "${map}" "${objects}"
    "${missing_attributes}" "${sections}" "${disassembly}"
    "linkmap: architecture-cpu")
  set(extended_sections "${control_root}/sections-extended.txt")
  file(WRITE "${extended_sections}"
    "  [ 1] .gba_header PROGBITS 08000000 001000 0000c0 00  AX  0 0 4\n  [ 2] .text PROGBITS 080000c0 0010c0 000078 00  AX  0 0 4\n  [ 3] .unexpected PROGBITS 08000138 001138 000004 00  AX  0 0 4\n")
  run_linkmap("executable-extension" "${map}" "${objects}"
    "${attributes}" "${extended_sections}" "${disassembly}"
    "linkmap: executable-extension")

  set(signature_root "${control_root}/signatures")
  file(WRITE "${signature_root}/clean.dat" "clean synthetic text\n")
  file(WRITE "${signature_root}/clean.allow" "clean.dat\n")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "CMAKE=${CMAKE_COMMAND}" sh
      "${source_root}/scripts/signature-scan.sh" --dir "${signature_root}"
      --allowlist "${signature_root}/clean.allow"
    RESULT_VARIABLE clean_signature_rc OUTPUT_VARIABLE clean_signature_out
    ERROR_VARIABLE clean_signature_error)
  if(NOT "${clean_signature_rc}" STREQUAL "0" OR
     NOT clean_signature_out MATCHES "signature: ok")
    message(FATAL_ERROR
      "toolchain-spike: clean-signature-control-failed: ${clean_signature_error}${clean_signature_out}")
  endif()

  function(run_signature_control kind expected)
    set(case_root "${signature_root}/${kind}")
    file(MAKE_DIRECTORY "${case_root}")
    set(fake "${case_root}/standin.dat")
    if(kind STREQUAL "logo")
      string(REPEAT "X" 156 fake_window)
      file(WRITE "${fake}" "HEAD${fake_window}TAIL")
      set(digest_kind logo)
      set(digest_label logo)
    else()
      string(REPEAT "B" 16384 fake_window)
      file(WRITE "${fake}" "${fake_window}")
      set(digest_kind bios)
      set(digest_label bios)
    endif()
    file(WRITE "${case_root}/allow.txt" "standin.dat\n")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env "CMAKE=${CMAKE_COMMAND}" sh
        "${source_root}/scripts/signature-digest.sh" "${digest_kind}" "${fake}"
      RESULT_VARIABLE digest_rc OUTPUT_VARIABLE digest_output
      ERROR_VARIABLE digest_error)
    string(REGEX MATCH "^[0-9a-f]+" digest "${digest_output}")
    string(LENGTH "${digest}" digest_length)
    if(NOT "${digest_rc}" STREQUAL "0" OR NOT digest_length EQUAL 64)
      message(FATAL_ERROR
        "toolchain-spike: synthetic-signature-digest-failed: ${digest_error}${digest_output}")
    endif()
    file(WRITE "${case_root}/extra-digests.txt"
      "${digest} ${digest_label}:stand-in\n")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env "CMAKE=${CMAKE_COMMAND}" sh
        "${source_root}/scripts/signature-scan.sh" --dir "${case_root}"
        --allowlist "${case_root}/allow.txt" --extra-digests
        "${case_root}/extra-digests.txt"
      RESULT_VARIABLE scan_rc OUTPUT_VARIABLE scan_output
      ERROR_VARIABLE scan_error)
    if("${scan_rc}" STREQUAL "0" OR NOT "${scan_output}${scan_error}" MATCHES "${expected}")
      message(FATAL_ERROR
        "toolchain-spike: ${kind}-signature-control-missed: ${scan_output}${scan_error}")
    endif()
    message(STATUS "toolchain-spike: synthetic-${kind}-signature ${expected}")
  endfunction()
  run_signature_control(logo "signature: logo-window: standin[.]dat")
  run_signature_control(bios "signature: bios-digest: standin[.]dat")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DCASE=hosted-proof "-DSRC=${source_root}"
      "-DBIN=${bin_root}" "-DHOSTED_RECEIPT=${control_root}/no-hosted-proof.json"
      -P "${source_root}/tests/gates/toolchain_spike.cmake"
    RESULT_VARIABLE hosted_missing_rc OUTPUT_VARIABLE hosted_missing_out
    ERROR_VARIABLE hosted_missing_error)
  if("${hosted_missing_rc}" STREQUAL "0" OR
     NOT "${hosted_missing_out}${hosted_missing_error}" MATCHES
       "toolchain-spike: hosted-proof-receipt-missing")
    message(FATAL_ERROR "toolchain-spike: missing-hosted-proof-did-not-fail-closed")
  endif()
  message(STATUS "toolchain-spike: missing-hosted-proof fail-closed")

  execute_process(COMMAND git -C "${source_root}" rev-parse HEAD
    RESULT_VARIABLE control_head_rc OUTPUT_VARIABLE control_head
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${control_head_rc}" STREQUAL "0" OR
     NOT control_head MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "toolchain-spike: hosted-control-source-head-unavailable")
  endif()
  set(hosted_control_root "${control_root}/hosted-control")
  set(hosted_control_roms "${hosted_control_root}/roms")
  file(MAKE_DIRECTORY "${hosted_control_roms}")
  file(WRITE "${hosted_control_roms}/smoke.gba" "synthetic hosted smoke row\n")
  file(WRITE "${hosted_control_roms}/smoke-broken.gba"
    "synthetic hosted broken row\n")
  file(SHA256 "${hosted_control_roms}/smoke.gba" control_smoke_sha)
  file(SHA256 "${hosted_control_roms}/smoke-broken.gba" control_broken_sha)

  function(make_hosted_control_row name digest output_name)
    set(row "{}")
    string(JSON row SET "${row}" name "\"${name}\"")
    string(JSON row SET "${row}" sha256 "\"${digest}\"")
    set(${output_name} "${row}" PARENT_SCOPE)
  endfunction()
  function(run_hosted_control label outputs_json expected)
    set(receipt "{}")
    string(JSON receipt SET "${receipt}" format
      "\"gamebloke.hosted-diagnostic-proof/1\"")
    string(JSON receipt SET "${receipt}" source_head "\"${control_head}\"")
    string(JSON receipt SET "${receipt}" host "\"linux-x64\"")
    string(JSON receipt SET "${receipt}" job_id "\"control-job\"")
    string(JSON receipt SET "${receipt}" job_name "\"diagnostics (control)\"")
    string(JSON receipt SET "${receipt}" job_result "\"success\"")
    string(JSON receipt SET "${receipt}" elapsed_seconds 1)
    string(JSON receipt SET "${receipt}" output_names
      "[\"smoke.gba\",\"smoke-broken.gba\"]")
    string(JSON receipt SET "${receipt}" outputs "${outputs_json}")
    set(receipt_path "${hosted_control_root}/${label}.json")
    file(WRITE "${receipt_path}" "${receipt}\n")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -DCASE=hosted-proof
        "-DSRC=${source_root}" "-DBIN=${bin_root}"
        "-DHOSTED_RECEIPT=${receipt_path}"
        "-DHOSTED_OUTPUT_DIR=${hosted_control_roms}"
        -P "${source_root}/tests/gates/toolchain_spike.cmake"
      RESULT_VARIABLE control_rc OUTPUT_VARIABLE control_out
      ERROR_VARIABLE control_error)
    set(control_report "${control_out}\n${control_error}")
    if(expected STREQUAL "pass")
      if(NOT "${control_rc}" STREQUAL "0" OR
         NOT control_report MATCHES "toolchain-spike: hosted-proof")
        message(FATAL_ERROR
          "toolchain-spike: hosted-${label}-control-failed: ${control_report}")
      endif()
    elseif("${control_rc}" STREQUAL "0" OR
       NOT control_report MATCHES "${expected}")
      message(FATAL_ERROR
        "toolchain-spike: hosted-${label}-control-missed: ${control_report}")
    endif()
    message(STATUS "toolchain-spike: hosted-${label} ${expected}")
  endfunction()

  make_hosted_control_row("smoke.gba" "${control_smoke_sha}" smoke_row)
  make_hosted_control_row("smoke-broken.gba" "${control_broken_sha}" broken_row)
  set(clean_outputs "[]")
  string(JSON clean_outputs SET "${clean_outputs}" 0 "${smoke_row}")
  string(JSON clean_outputs SET "${clean_outputs}" 1 "${broken_row}")
  run_hosted_control(clean "${clean_outputs}" pass)

  set(duplicate_outputs "[]")
  string(JSON duplicate_outputs SET "${duplicate_outputs}" 0 "${smoke_row}")
  string(JSON duplicate_outputs SET "${duplicate_outputs}" 1 "${smoke_row}")
  run_hosted_control(duplicate "${duplicate_outputs}"
    "toolchain-spike: hosted-proof-output-name-set-mismatch")

  set(missing_outputs "[]")
  string(JSON missing_outputs SET "${missing_outputs}" 0 "${smoke_row}")
  run_hosted_control(missing "${missing_outputs}"
    "toolchain-spike: hosted-proof-output-count")

  set(extra_outputs "[]")
  string(JSON extra_outputs SET "${extra_outputs}" 0 "${smoke_row}")
  string(JSON extra_outputs SET "${extra_outputs}" 1 "${broken_row}")
  make_hosted_control_row("surprise.gba" "${control_smoke_sha}" extra_row)
  string(JSON extra_outputs SET "${extra_outputs}" 2 "${extra_row}")
  run_hosted_control(extra "${extra_outputs}"
    "toolchain-spike: hosted-proof-output-count")

  message(STATUS "toolchain-spike: offline-controls ok")
  return()
endif()

function(assert_rom_names rom_directory label)
  file(GLOB entries LIST_DIRECTORIES TRUE "${rom_directory}/*")
  list(SORT entries)
  set(expected "${rom_directory}/smoke-broken.gba"
    "${rom_directory}/smoke.gba")
  if(NOT "${entries}" STREQUAL "${expected}")
    message(FATAL_ERROR "toolchain-spike: ${label}-rom-name-set-mismatch")
  endif()
  foreach(path IN LISTS entries)
    if(IS_DIRECTORY "${path}" OR IS_SYMLINK "${path}")
      message(FATAL_ERROR "toolchain-spike: ${label}-rom-not-regular")
    endif()
  endforeach()
endfunction()

function(audit_variant host_root variant label)
  set(proof "${host_root}/proof/${variant}")
  set(objects "${host_root}/proof/common/startup.o;${proof}/smoke.o")
  set(attributes
    "${proof}/startup.attributes.txt;${proof}/smoke.attributes.txt;${proof}/elf.attributes.txt")
  run_linkmap("${label}-${variant}" "${proof}/smoke.map" "${objects}"
    "${attributes}" "${proof}/elf-sections.txt"
    "${proof}/executable.disasm.txt" "pass")
endfunction()

function(compare_roms left right label output_name)
  execute_process(COMMAND cmp -s "${left}" "${right}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  if(NOT "${rc}" STREQUAL "0" AND NOT "${rc}" STREQUAL "1")
    message(FATAL_ERROR
      "toolchain-spike: ${label}-cmp-failed: exit=${rc}: ${stdout}${stderr}")
  endif()
  set(${output_name} "${rc}" PARENT_SCOPE)
  if("${rc}" STREQUAL "0")
    message(STATUS "toolchain-spike: ${label} cmp exit=0 (byte-identical)")
  else()
    message(STATUS "toolchain-spike: ${label} cmp exit=1 (byte-different; preserved)")
  endif()
endfunction()

function(check_signature_output rom_directory host_root label)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "CMAKE=${CMAKE_COMMAND}" sh
      "${source_root}/scripts/signature-scan.sh" --dir "${rom_directory}"
      --allowlist "${host_root}/proof/signature-allowlist.txt"
    RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  if(NOT "${rc}" STREQUAL "0" OR NOT stdout MATCHES "signature: ok")
    message(FATAL_ERROR
      "toolchain-spike: ${label}-signature-scan-failed: ${stdout}${stderr}")
  endif()
endfunction()

function(source_head output_name)
  execute_process(COMMAND git -C "${source_root}" rev-parse HEAD
    RESULT_VARIABLE rc OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(LENGTH "${head}" head_length)
  if(NOT "${rc}" STREQUAL "0" OR NOT head MATCHES "^[0-9a-f]+$" OR
     NOT head_length EQUAL 40)
    message(FATAL_ERROR "toolchain-spike: source-head-unavailable")
  endif()
  set(${output_name} "${head}" PARENT_SCOPE)
endfunction()

function(write_local_receipt receipt_path source_revision image_metadata
    image_repo_digest docker_identity provision_seconds toolchain_seconds
    download_seconds extract_seconds build_seconds total_seconds host_real
    smoke_cmp broken_cmp parity_value source_fingerprint)
  set(receipt "{}")
  string(JSON receipt SET "${receipt}" format
    "\"gamebloke.local-toolchain-proof/1\"")
  string(JSON receipt SET "${receipt}" source_head "\"${source_revision}\"")
  string(JSON receipt SET "${receipt}" local_linux_host "\"Linux/aarch64\"")
  string(JSON receipt SET "${receipt}" pinned_image
    "\"ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c\"")
  string(JSON receipt SET "${receipt}" image_id "\"${image_metadata}\"")
  string(JSON receipt SET "${receipt}" image_repo_digest "\"${image_repo_digest}\"")
  string(JSON receipt SET "${receipt}" docker_server "\"${docker_identity}\"")
  string(JSON receipt SET "${receipt}" container_package_provision_seconds
    "${provision_seconds}")
  string(JSON receipt SET "${receipt}" container_toolchain_check_seconds
    "${toolchain_seconds}")
  string(JSON receipt SET "${receipt}" toolchain_install_receipt_download_seconds
    "${download_seconds}")
  string(JSON receipt SET "${receipt}" toolchain_install_receipt_extract_seconds
    "${extract_seconds}")
  string(JSON receipt SET "${receipt}" container_build_seconds "${build_seconds}")
  string(JSON receipt SET "${receipt}" container_total_seconds "${total_seconds}")
  string(JSON receipt SET "${receipt}" host_wall_seconds "${host_real}")
  string(JSON receipt SET "${receipt}" timing_resolution_seconds "1")
  string(JSON receipt SET "${receipt}" smoke_cmp_exit "${smoke_cmp}")
  string(JSON receipt SET "${receipt}" broken_cmp_exit "${broken_cmp}")
  string(JSON receipt SET "${receipt}" byte_parity "${parity_value}")
  string(JSON receipt SET "${receipt}" rom_digest_admitted "false")
  string(JSON receipt SET "${receipt}" hosted_linux_x64
    "\"not-observed-in-05-02\"")
  string(JSON receipt SET "${receipt}" output_names
    "[\"smoke.gba\",\"smoke-broken.gba\"]")
  string(JSON receipt SET "${receipt}" source_inputs "${source_fingerprint}")
  file(WRITE "${receipt_path}" "${receipt}\n")
endfunction()

function(build_source_fingerprint output_name)
  set(fingerprint "{}")
  foreach(relative IN ITEMS
      diagnostics/armv4t/startup.S diagnostics/armv4t/smoke.S
      diagnostics/armv4t/linker.ld cmake/FixGbaHeader.cmake
      cmake/BuildArmDiagnostics.cmake tests/gates/diagnostic_linkmap.cmake
      tests/gates/toolchain_spike.cmake)
    file(SHA256 "${source_root}/${relative}" sha)
    string(REPLACE "/" "_" key "${relative}")
    string(REPLACE "." "_" key "${key}")
    string(JSON fingerprint SET "${fingerprint}" "${key}" "\"${sha}\"")
  endforeach()
  set(${output_name} "${fingerprint}" PARENT_SCOPE)
endfunction()

function(verify_source_fingerprint receipt)
  string(JSON recorded_fingerprint ERROR_VARIABLE fingerprint_error
    GET "${receipt}" source_inputs)
  if(NOT "${fingerprint_error}" STREQUAL "NOTFOUND")
    message(FATAL_ERROR "toolchain-spike: source-fingerprint-missing")
  endif()
  build_source_fingerprint(current_fingerprint)
  if(NOT "${recorded_fingerprint}" STREQUAL "${current_fingerprint}")
    message(FATAL_ERROR "toolchain-spike: source-fingerprint-stale")
  endif()
endfunction()

function(verify_local_receipt)
  set(receipt_path "${bin_root}/local-proof.json")
  if(NOT EXISTS "${receipt_path}" OR IS_SYMLINK "${receipt_path}")
    message(FATAL_ERROR "toolchain-spike: local-proof-missing")
  endif()
  file(READ "${receipt_path}" receipt)
  string(JSON format GET "${receipt}" format)
  string(JSON recorded_head GET "${receipt}" source_head)
  string(JSON host GET "${receipt}" local_linux_host)
  string(JSON image GET "${receipt}" pinned_image)
  string(JSON digest_admitted GET "${receipt}" rom_digest_admitted)
  string(JSON output_count LENGTH "${receipt}" output_names)
  set(output_names "")
  if(output_count EQUAL 2)
    foreach(index RANGE 0 1)
      string(JSON output_name GET "${receipt}" output_names ${index})
      list(APPEND output_names "${output_name}")
    endforeach()
  endif()
  list(SORT output_names)
  if(NOT format STREQUAL "gamebloke.local-toolchain-proof/1" OR
     NOT host STREQUAL "Linux/aarch64" OR
     NOT image STREQUAL "ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c" OR
     NOT digest_admitted STREQUAL "OFF" OR
     NOT output_names STREQUAL "smoke-broken.gba;smoke.gba")
    message(FATAL_ERROR "toolchain-spike: local-proof-identity-mismatch")
  endif()
  foreach(timing_field IN ITEMS container_package_provision_seconds
      container_toolchain_check_seconds toolchain_install_receipt_download_seconds
      toolchain_install_receipt_extract_seconds container_build_seconds
      container_total_seconds host_wall_seconds)
    string(JSON timing_value ERROR_VARIABLE timing_error GET "${receipt}" "${timing_field}")
    if(NOT "${timing_error}" STREQUAL "NOTFOUND" OR
       NOT "${timing_value}" MATCHES "^[0-9]+([.][0-9]+)?$")
      message(FATAL_ERROR "toolchain-spike: local-proof-timing-invalid-${timing_field}")
    endif()
  endforeach()
  source_head(current_head)
  execute_process(COMMAND git -C "${source_root}" merge-base --is-ancestor
      "${recorded_head}" "${current_head}"
    RESULT_VARIABLE ancestor_rc)
  if(NOT "${ancestor_rc}" STREQUAL "0")
    message(FATAL_ERROR "toolchain-spike: local-proof-stale-source-head")
  endif()
  verify_source_fingerprint("${receipt}")

  set(mac_root "${bin_root}/native")
  set(linux_root "${bin_root}/linux-arm64")
  assert_rom_names("${mac_root}/roms" "macos-arm64")
  assert_rom_names("${linux_root}/roms" "linux-arm64")
  foreach(variant IN ITEMS smoke smoke-broken)
    audit_variant("${mac_root}" "${variant}" "macos-arm64")
    audit_variant("${linux_root}" "${variant}" "linux-arm64")
  endforeach()
  check_signature_output("${mac_root}/roms" "${mac_root}" "macos-arm64")
  check_signature_output("${linux_root}/roms" "${linux_root}" "linux-arm64")
  compare_roms("${mac_root}/roms/smoke.gba" "${linux_root}/roms/smoke.gba"
    "smoke-recheck" current_smoke_cmp)
  compare_roms("${mac_root}/roms/smoke-broken.gba"
    "${linux_root}/roms/smoke-broken.gba" "broken-recheck" current_broken_cmp)
  string(JSON recorded_smoke_cmp GET "${receipt}" smoke_cmp_exit)
  string(JSON recorded_broken_cmp GET "${receipt}" broken_cmp_exit)
  if(NOT "${current_smoke_cmp}" STREQUAL "${recorded_smoke_cmp}" OR
     NOT "${current_broken_cmp}" STREQUAL "${recorded_broken_cmp}")
    message(FATAL_ERROR "toolchain-spike: local-proof-cmp-evidence-stale")
  endif()
  message(STATUS "toolchain-spike: proof-only ok source=${recorded_head} host=${host}")
endfunction()

if(CASE STREQUAL "local-proof")
  foreach(required IN ITEMS TOOLCHAIN_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
      message(FATAL_ERROR "toolchain-spike: missing-${required}")
    endif()
  endforeach()
  set(toolchain_request "${TOOLCHAIN_ROOT}")
  cmake_path(ABSOLUTE_PATH toolchain_request BASE_DIRECTORY "${source_root}"
    NORMALIZE OUTPUT_VARIABLE toolchain_root)
  cmake_path(IS_PREFIX source_root "${toolchain_root}" NORMALIZE toolchain_under_source)
  file(RELATIVE_PATH toolchain_relative "${source_root}" "${toolchain_root}")
  if(NOT toolchain_under_source OR IS_SYMLINK "${toolchain_root}" OR
     NOT toolchain_relative STREQUAL "build/toolchains")
    message(FATAL_ERROR "toolchain-spike: toolchain-root-outside-approved-cache")
  endif()

  set(image "ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c")
  execute_process(COMMAND docker info --format "{{.OSType}}/{{.Architecture}}"
    RESULT_VARIABLE docker_info_rc OUTPUT_VARIABLE docker_identity
    ERROR_VARIABLE docker_info_error OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${docker_info_rc}" STREQUAL "0" OR
     NOT docker_identity MATCHES "^linux/(aarch64|arm64)$")
    message(FATAL_ERROR
      "toolchain-spike: native-linux-arm64-unavailable: ${docker_info_error}${docker_identity}")
  endif()
  execute_process(COMMAND docker image inspect --format
      "{{.Id}}|{{.Os}}/{{.Architecture}}" "${image}"
    RESULT_VARIABLE image_rc OUTPUT_VARIABLE image_metadata
    ERROR_VARIABLE image_error OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${image_rc}" STREQUAL "0" OR
     NOT image_metadata MATCHES "^sha256:[0-9a-f]+[|]linux/arm64$")
    message(FATAL_ERROR
      "toolchain-spike: pinned-linux-arm64-image-unavailable: ${image_error}${image_metadata}")
  endif()
  execute_process(COMMAND docker image inspect --format
      "{{.RepoDigests}}" "${image}"
    RESULT_VARIABLE repo_digest_rc OUTPUT_VARIABLE image_repo_digest
    ERROR_VARIABLE repo_digest_error OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${repo_digest_rc}" STREQUAL "0" OR
     NOT image_repo_digest MATCHES "ubuntu@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c")
    message(FATAL_ERROR
      "toolchain-spike: image-repo-digest-mismatch: ${repo_digest_error}${image_repo_digest}")
  endif()
  source_head(build_head)
  set(linux_root "${bin_root}/linux-arm64")
  set(receipt_path "${bin_root}/local-proof.json")
  set(container_timing "${bin_root}/linux-arm64-container-timings.txt")
  file(REMOVE_RECURSE "${linux_root}")
  file(REMOVE "${receipt_path}" "${container_timing}")

  set(container_script [=[
set -eu
export DEBIAN_FRONTEND=noninteractive
total_started=$(date +%s)
provision_started=$total_started
apt-get update -qq
apt-get install -y -qq --no-install-recommends ca-certificates cmake curl ninja-build git libc6-dev gcc >/dev/null
provision_finished=$(date +%s)
toolchain_started=$provision_finished
if [ -f build/toolchains/arm-none-eabi/linux-arm64-15.2.1-1.1/install.json ]; then
  scripts/fetch-toolchain.sh --check --host linux-arm64 --root build/toolchains
else
  scripts/fetch-toolchain.sh --host linux-arm64 --root build/toolchains
fi
toolchain_finished=$(date +%s)
cmake -DSRC=/work -DOUT=build/phase05-spike/linux-arm64 -DTOOLCHAIN_ROOT=build/toolchains -DMODE=smoke -DHOST=linux-arm64 -P cmake/BuildArmDiagnostics.cmake
build_finished=$(date +%s)
mkdir -p build/phase05-spike
printf 'provision_seconds=%s\ntoolchain_seconds=%s\nbuild_seconds=%s\ntotal_seconds=%s\n' \
  "$((provision_finished-provision_started))" \
  "$((toolchain_finished-toolchain_started))" \
  "$((build_finished-toolchain_finished))" \
  "$((build_finished-total_started))" \
  > build/phase05-spike/linux-arm64-container-timings.txt
]=])
  execute_process(COMMAND /usr/bin/time -p docker run --pull=never --rm
      --cpus 4 --memory 8g --mount "type=bind,src=${source_root},dst=/work"
      -w /work "${image}" sh -c "${container_script}"
    RESULT_VARIABLE docker_run_rc OUTPUT_VARIABLE docker_run_output
    ERROR_VARIABLE docker_run_error)
  if(NOT "${docker_run_rc}" STREQUAL "0")
    message(FATAL_ERROR
      "toolchain-spike: linux-arm64-witness-failed: ${docker_run_output}${docker_run_error}")
  endif()
  if(NOT EXISTS "${linux_root}/proof/build-receipt.json" OR
     NOT EXISTS "${container_timing}")
    message(FATAL_ERROR "toolchain-spike: linux-arm64-receipt-missing")
  endif()

  set(mac_root "${bin_root}/native")
  assert_rom_names("${mac_root}/roms" "macos-arm64")
  assert_rom_names("${linux_root}/roms" "linux-arm64")
  foreach(variant IN ITEMS smoke smoke-broken)
    audit_variant("${mac_root}" "${variant}" "macos-arm64")
    audit_variant("${linux_root}" "${variant}" "linux-arm64")
  endforeach()
  check_signature_output("${mac_root}/roms" "${mac_root}" "macos-arm64")
  check_signature_output("${linux_root}/roms" "${linux_root}" "linux-arm64")

  compare_roms("${mac_root}/roms/smoke.gba" "${linux_root}/roms/smoke.gba"
    "smoke" smoke_cmp)
  compare_roms("${mac_root}/roms/smoke-broken.gba"
    "${linux_root}/roms/smoke-broken.gba" "smoke-broken" broken_cmp)
  if("${smoke_cmp}" STREQUAL "0" AND "${broken_cmp}" STREQUAL "0")
    set(parity_value true)
  else()
    set(parity_value false)
    message(STATUS
      "toolchain-spike: local arm64 mismatch is preserved; no digest admitted; hosted linux-x64 remains a separate later witness")
  endif()

  file(READ "${toolchain_root}/arm-none-eabi/linux-arm64-15.2.1-1.1/install.json"
    linux_tool_receipt)
  string(JSON download_seconds GET "${linux_tool_receipt}" download_seconds)
  string(JSON extract_seconds GET "${linux_tool_receipt}" extract_seconds)
  file(READ "${container_timing}" timing_text)
  string(REGEX MATCH "provision_seconds=([0-9]+)" _ "${timing_text}")
  set(provision_seconds "${CMAKE_MATCH_1}")
  string(REGEX MATCH "toolchain_seconds=([0-9]+)" _ "${timing_text}")
  set(toolchain_seconds "${CMAKE_MATCH_1}")
  string(REGEX MATCH "build_seconds=([0-9]+)" _ "${timing_text}")
  set(build_seconds "${CMAKE_MATCH_1}")
  string(REGEX MATCH "total_seconds=([0-9]+)" _ "${timing_text}")
  set(total_seconds "${CMAKE_MATCH_1}")
  string(REGEX MATCH "real ([0-9.]+)" _ "${docker_run_error}")
  set(host_real "${CMAKE_MATCH_1}")
  if("${provision_seconds}" STREQUAL "" OR "${toolchain_seconds}" STREQUAL "" OR
     "${build_seconds}" STREQUAL "" OR "${total_seconds}" STREQUAL "" OR
     "${host_real}" STREQUAL "")
    message(FATAL_ERROR "toolchain-spike: actual-timing-evidence-missing")
  endif()
  build_source_fingerprint(source_inputs)
  write_local_receipt("${receipt_path}" "${build_head}" "${image_metadata}"
    "${image_repo_digest}" "${docker_identity}" "${provision_seconds}"
    "${toolchain_seconds}" "${download_seconds}" "${extract_seconds}"
    "${build_seconds}" "${total_seconds}"
    "${host_real}" "${smoke_cmp}" "${broken_cmp}" "${parity_value}"
    "${source_inputs}")
  message(STATUS "toolchain-spike: local-proof receipt ${receipt_path}")
  message("${docker_run_output}")
  message("${docker_run_error}")
  return()
endif()

if(CASE STREQUAL "proof-only")
  verify_local_receipt()
  return()
endif()

if(CASE STREQUAL "hosted-proof")
  set(receipt_path "${bin_root}/hosted-linux-x64.json")
  if(DEFINED HOSTED_RECEIPT AND NOT "${HOSTED_RECEIPT}" STREQUAL "")
    set(receipt_path "${HOSTED_RECEIPT}")
  endif()
  if(NOT EXISTS "${receipt_path}" OR IS_DIRECTORY "${receipt_path}" OR
     IS_SYMLINK "${receipt_path}")
    message(FATAL_ERROR "toolchain-spike: hosted-proof-receipt-missing")
  endif()
  file(READ "${receipt_path}" hosted_receipt)
  foreach(field IN ITEMS format source_head host job_id job_name job_result
      elapsed_seconds output_names outputs)
    string(JSON hosted_${field} ERROR_VARIABLE field_error
      GET "${hosted_receipt}" "${field}")
    if(NOT "${field_error}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "toolchain-spike: hosted-proof-field-missing-${field}")
    endif()
  endforeach()
  source_head(current_head)
  string(JSON hosted_output_count LENGTH "${hosted_receipt}" output_names)
  set(hosted_output_names "")
  if(hosted_output_count EQUAL 2)
    foreach(index RANGE 0 1)
      string(JSON output_name GET "${hosted_receipt}" output_names ${index})
      list(APPEND hosted_output_names "${output_name}")
    endforeach()
  endif()
  list(SORT hosted_output_names)
  if(NOT hosted_format STREQUAL "gamebloke.hosted-diagnostic-proof/1" OR
     NOT hosted_source_head STREQUAL current_head OR
     NOT hosted_host STREQUAL "linux-x64" OR
     hosted_job_id STREQUAL "" OR hosted_job_name STREQUAL "" OR
     NOT hosted_job_result STREQUAL "success" OR
     NOT hosted_elapsed_seconds MATCHES "^[0-9]+$" OR
     hosted_elapsed_seconds GREATER 180 OR
     NOT hosted_output_names STREQUAL "smoke-broken.gba;smoke.gba")
    message(FATAL_ERROR "toolchain-spike: hosted-proof-evidence-invalid")
  endif()
  # Compare hosted log hashes to existing local counterparts; do not create or
  # populate a directory that implies downloaded hosted binaries.
  set(hosted_output_dir "${bin_root}/local-counterparts/roms")
  if(DEFINED HOSTED_OUTPUT_DIR AND NOT "${HOSTED_OUTPUT_DIR}" STREQUAL "")
    set(hosted_output_dir "${HOSTED_OUTPUT_DIR}")
  endif()
  assert_rom_names("${hosted_output_dir}" "hosted-proof-local-counterparts")
  string(JSON outputs_type TYPE "${hosted_receipt}" outputs)
  if(NOT outputs_type STREQUAL "ARRAY")
    message(FATAL_ERROR "toolchain-spike: hosted-proof-outputs-not-array")
  endif()
  string(JSON output_count LENGTH "${hosted_outputs}")
  if(NOT output_count EQUAL 2)
    message(FATAL_ERROR "toolchain-spike: hosted-proof-output-count")
  endif()
  set(output_names "")
  foreach(index RANGE 0 1)
    string(JSON output_name ERROR_VARIABLE output_name_error
      GET "${hosted_outputs}" ${index} name)
    if(NOT "${output_name_error}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "toolchain-spike: hosted-proof-output-name-missing")
    endif()
    if(NOT output_name STREQUAL "smoke.gba" AND
       NOT output_name STREQUAL "smoke-broken.gba")
      message(FATAL_ERROR "toolchain-spike: hosted-proof-output-name")
    endif()
    list(APPEND output_names "${output_name}")
  endforeach()
  list(SORT output_names)
  if(NOT output_names STREQUAL "smoke-broken.gba;smoke.gba")
    message(FATAL_ERROR "toolchain-spike: hosted-proof-output-name-set-mismatch")
  endif()

  foreach(index RANGE 0 1)
    string(JSON output_name GET "${hosted_outputs}" ${index} name)
    string(JSON output_sha ERROR_VARIABLE output_sha_error
      GET "${hosted_outputs}" ${index} sha256)
    if(NOT "${output_sha_error}" STREQUAL "NOTFOUND")
      message(FATAL_ERROR "toolchain-spike: hosted-proof-output-digest-missing")
    endif()
    string(LENGTH "${output_sha}" output_sha_length)
    if(NOT output_sha MATCHES "^[0-9a-f]+$" OR NOT output_sha_length EQUAL 64)
      message(FATAL_ERROR "toolchain-spike: hosted-proof-output-digest")
    endif()
    file(SHA256 "${hosted_output_dir}/${output_name}" actual_sha)
    if(NOT actual_sha STREQUAL output_sha)
      message(FATAL_ERROR "toolchain-spike: hosted-proof-output-mismatch")
    endif()
  endforeach()
  message(STATUS
    "toolchain-spike: hosted-proof output SHA-256 values match local counterpart files under ${hosted_output_dir} (job=${hosted_job_id}, elapsed=${hosted_elapsed_seconds}s); hash identity only, not literal cmp evidence")
  return()
endif()

message(FATAL_ERROR "toolchain-spike: case-not-implemented: ${CASE}")
