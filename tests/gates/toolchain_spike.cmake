cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS CASE SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "toolchain-spike: missing-${required}")
  endif()
endforeach()
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
      "Tag_CPU_name: ARM7TDMI\nTag_CPU_arch: v4T\nTag_THUMB_ISA_use: Thumb-1\n")
    list(APPEND attributes "${path}")
  endforeach()
  set(sections "${control_root}/sections.txt")
  file(WRITE "${sections}"
    "  [ 1] .gba_header PROGBITS 08000000 001000 0000c0 00  AX  0 0 4\n  [ 2] .text PROGBITS 080000c0 0010c0 000078 00  AX  0 0 4\n  [ 3] .ARM.attributes ARM_ATTRIBUTES 00000000 001138 00001a 00      0 0 1\n")
  set(disassembly "${control_root}/disassembly.txt")
  file(WRITE "${disassembly}"
    "Disassembly of section .gba_header:\n08000000 <_start>:\n  b 080000c0\nDisassembly of section .text:\n080000c0 <diagnostic_main>:\n  bx r3\n08000120 <thumb_probe>:\n  bx lr\n")
  set(gate "${source_root}/tests/gates/diagnostic_linkmap.cmake")

  function(run_linkmap label map_path attr_paths section_path expected outcome)
    execute_process(
      COMMAND "${CMAKE_COMMAND}" "-DMAP=${map_path}"
        "-DOBJECTS=${objects}" "-DATTRIBUTES=${attr_paths}"
        "-DSECTIONS=${section_path}" "-DDISASSEMBLY=${disassembly}"
        "-DSRC=${source_root}" -P "${gate}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    set(report "${stdout}\n${stderr}")
    if(outcome STREQUAL "pass")
      if(NOT "${rc}" STREQUAL "0" OR NOT report MATCHES "linkmap: ok")
        message(FATAL_ERROR
          "toolchain-spike: ${label}: clean map should pass; exit=${rc}: ${report}")
      endif()
    elseif("${rc}" STREQUAL "0" OR NOT report MATCHES "${expected}")
      message(FATAL_ERROR
        "toolchain-spike: ${label}: expected ${expected}; exit=${rc}: ${report}")
    endif()
    message(STATUS "toolchain-spike: ${label} ${outcome}")
  endfunction()

  run_linkmap("clean-map" "${map}" "${attributes}" "${sections}" "" "pass")
  file(WRITE "${control_root}/runtime.map"
    "LOAD ${startup}\nLOAD crt0.o\nLOAD ${smoke}\nLOAD linker stubs\n")
  run_linkmap("runtime-injection" "${control_root}/runtime.map"
    "${attributes}" "${sections}" "linkmap: runtime-input" "fail")
  file(WRITE "${control_root}/archive.map"
    "LOAD ${startup}\nLOAD ${smoke}\nLOAD libhelpers.a(member.o)\nLOAD linker stubs\n")
  run_linkmap("archive-member" "${control_root}/archive.map"
    "${attributes}" "${sections}" "linkmap: archive-input" "fail")
  file(WRITE "${control_root}/missing.map"
    "LOAD ${startup}\nLOAD linker stubs\n")
  run_linkmap("missing-object" "${control_root}/missing.map"
    "${attributes}" "${sections}" "linkmap: map-input-set-mismatch" "fail")
  file(WRITE "${control_root}/extra.o" "synthetic unlisted input name only\n")
  file(WRITE "${control_root}/extra.map"
    "LOAD ${startup}\nLOAD ${smoke}\nLOAD ${control_root}/extra.o\nLOAD linker stubs\n")
  run_linkmap("extra-object" "${control_root}/extra.map"
    "${attributes}" "${sections}" "linkmap: map-input-set-mismatch" "fail")

  file(WRITE "${control_root}/attributes/elf-wrong.txt"
    "Tag_CPU_name: ARM7TDMI\nTag_CPU_arch: v5TE\nTag_THUMB_ISA_use: Thumb-1\n")
  set(wrong_attributes
    "${control_root}/attributes/startup.txt;${control_root}/attributes/smoke.txt;${control_root}/attributes/elf-wrong.txt")
  run_linkmap("wrong-elf-architecture" "${map}" "${wrong_attributes}"
    "${sections}" "linkmap: architecture-cpu" "fail")

  # Signature controls and local proof modes are added after the map gate's
  # architecture behavior has a failing regression case.
  message(STATUS "toolchain-spike: offline-controls incomplete")
  return()
endif()

message(FATAL_ERROR "toolchain-spike: case-not-implemented: ${CASE}")
