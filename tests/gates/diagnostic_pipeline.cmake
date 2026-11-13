cmake_minimum_required(VERSION 3.25)
foreach(required IN ITEMS SRC BIN TOOLCHAIN_ROOT HOST)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "diagnostic-pipeline: missing-${required}")
  endif()
endforeach()
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)
include("${SRC}/cmake/DiagnosticMatrix.cmake")
include("${SRC}/cmake/CheckDiagnosticAuthority.cmake")
include("${SRC}/cmake/PathContainment.cmake")
gbl_resolve_contained_path(binary_real binary_status "${SRC}/build" "${BIN}")
if(NOT binary_status STREQUAL "ok" OR BIN STREQUAL "${SRC}/build")
  message(FATAL_ERROR "diagnostic-pipeline: unsafe-binary-root")
endif()
gbl_diagnostic_tool("${SRC}" "${TOOLCHAIN_ROOT}" "${HOST}" ON available)
execute_process(COMMAND git -C "${SRC}" rev-parse HEAD OUTPUT_VARIABLE authority
  OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc)
if(NOT rc STREQUAL "0")
  message(FATAL_ERROR "diagnostic-pipeline: source-authority-unavailable")
endif()
gbl_check_diagnostic_authority("${SRC}" "${authority}")
# Reuse the real aggregate builder's validated parent identity, never manufacture
# its receipt. This avoids repeating the full ledger audit for each instrumented
# failure. The dedicated smoke child is outside the ARM semantic output set.
set(parent "${SRC}/build/phase05-matrix/arm/proof/matrix-parent.json")
if(NOT EXISTS "${parent}" OR IS_SYMLINK "${parent}")
  message(FATAL_ERROR "diagnostic-pipeline: real-arm-parent-proof-required")
endif()
file(READ "${parent}" parent_doc)
string(JSON parent_commit GET "${parent_doc}" source_commit)
string(JSON parent_digest GET "${parent_doc}" source_input_sha256)
gbl_diagnostic_input_digest("${SRC}" digest)
if(NOT parent_commit STREQUAL authority OR NOT parent_digest STREQUAL digest)
  message(FATAL_ERROR "diagnostic-pipeline: stale-arm-parent-proof")
endif()
file(READ "${SRC}/diagnostics/armv4t/cases.json" matrix)
string(JSON rows GET "${matrix}" cases)
string(JSON count LENGTH "${rows}")
math(EXPR last "${count}-1")
set(smoke_index "")
foreach(index RANGE 0 ${last})
  string(JSON id GET "${rows}" ${index} id)
  if(id EQUAL 20481)
    set(smoke_index "${index}")
    break()
  endif()
endforeach()
if(smoke_index STREQUAL "")
  message(FATAL_ERROR "diagnostic-pipeline: smoke-ledger-missing")
endif()
set(root "${BIN}/diagnostic-pipeline-controls")
gbl_resolve_contained_path(control_real control_status "${BIN}" "${root}")
if(NOT control_status STREQUAL "ok" OR NOT control_real STREQUAL root)
  message(FATAL_ERROR "diagnostic-pipeline: unsafe-control-root")
endif()
file(REMOVE_RECURSE "${root}")
set(real "${TOOLCHAIN_ROOT}/arm-none-eabi/${HOST}-15.2.1-1.1")
set(instrumented "${root}/tools/arm-none-eabi/${HOST}-15.2.1-1.1")
file(MAKE_DIRECTORY "${instrumented}/bin")
file(COPY_FILE "${real}/install.json" "${instrumented}/install.json")
file(GLOB real_tools "${real}/bin/*")
foreach(tool IN LISTS real_tools)
  get_filename_component(name "${tool}" NAME)
  if(NOT EXISTS "${instrumented}/bin/${name}" AND NOT IS_SYMLINK "${instrumented}/bin/${name}")
    file(CREATE_LINK "${tool}" "${instrumented}/bin/${name}" SYMBOLIC RESULT rc)
    if(NOT rc STREQUAL "0")
      message(FATAL_ERROR "diagnostic-pipeline: instrumentation-link-failed")
    endif()
  endif()
endforeach()
foreach(tool IN ITEMS as ld objcopy)
  # These links were created solely by this control, inside its own build root.
  file(REMOVE "${instrumented}/bin/arm-none-eabi-${tool}")
endforeach()
file(WRITE "${instrumented}/bin/arm-none-eabi-as" [=[#!/bin/sh
if [ "$1" != --version ] && [ "$GBL_DEPENDENCY_FAIL_STAGE" = compile ]; then exit 61; fi
exec "$GBL_DEPENDENCY_REAL_BIN/arm-none-eabi-as" "$@"
]=])
file(WRITE "${instrumented}/bin/arm-none-eabi-ld" [=[#!/bin/sh
if [ "$GBL_DEPENDENCY_FAIL_STAGE" = link ]; then exit 62; fi
"$GBL_DEPENDENCY_REAL_BIN/arm-none-eabi-ld" "$@" || exit $?
if [ "$GBL_DEPENDENCY_FAIL_STAGE" = map ]; then
  for arg do case "$arg" in -Map=*) printf '\nLOAD libinjected.a(member.o)\n' >> "${arg#-Map=}";; esac; done
fi
]=])
file(WRITE "${instrumented}/bin/arm-none-eabi-objcopy" [=[#!/bin/sh
"$GBL_DEPENDENCY_REAL_BIN/arm-none-eabi-objcopy" "$@" || exit $?
for arg do last=$arg; done
if [ "$GBL_DEPENDENCY_FAIL_STAGE" = header ]; then printf x > "$last"; fi
if [ "$GBL_DEPENDENCY_FAIL_STAGE" = scan ]; then printf 'synthetic unlisted scan control\n' > "$GBL_DEPENDENCY_OUTPUT/roms/unlisted.gba"; fi
]=])
foreach(tool IN ITEMS as ld objcopy)
  file(CHMOD "${instrumented}/bin/arm-none-eabi-${tool}" PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endforeach()
set(out "${SRC}/build/phase05-matrix/arm/cases/20481")
foreach(stage IN ITEMS clean compile link header map scan clean-after)
  set(expected "")
  if(stage STREQUAL "compile")
    set(expected "assembler-common-startup-failed")
  elseif(stage STREQUAL "link")
    set(expected "linker-failed")
  elseif(stage STREQUAL "header")
    set(expected "gba-header: invalid-rom-length")
  elseif(stage STREQUAL "map")
    set(expected "map-input-count-mismatch")
  elseif(stage STREQUAL "scan")
    set(expected "signature: rom-extension")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "GBL_DEPENDENCY_FAIL_STAGE=${stage}" "GBL_DEPENDENCY_REAL_BIN=${real}/bin"
    "GBL_DEPENDENCY_OUTPUT=${out}" "${CMAKE_COMMAND}" "-DSRC=${SRC}"
    "-DOUT=${out}" "-DTOOLCHAIN_ROOT=${root}/tools" "-DSOURCE_COMMIT=${authority}"
    -DMODE=case -DISA=arm -DCASE_ID=20481 -DMATRIX_CHILD=1 "-DMATRIX_CASE_INDEX=${smoke_index}"
    -P "${SRC}/cmake/BuildArmDiagnostics.cmake"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
  file(WRITE "${root}/${stage}.log" "${output}${error}")
  if(expected STREQUAL "")
    if(NOT rc STREQUAL "0" OR NOT EXISTS "${out}/roms/smoke.gba" OR
       NOT EXISTS "${out}/roms/smoke-broken.gba")
      message(FATAL_ERROR "diagnostic-pipeline: actual-clean-build-failed:${stage}:${output}${error}")
    endif()
  elseif(rc STREQUAL "0" OR NOT "${output}${error}" MATCHES "${expected}" OR
      "${output}${error}" MATCHES "SKIPPED:")
    message(FATAL_ERROR "diagnostic-pipeline: wrong-failure-cause:${stage}:${output}${error}")
  endif()
endforeach()
file(REMOVE_RECURSE "${out}")
message(STATUS "diagnostic-pipeline: actual source clean companions and compile/link/header/map/scan hard failures passed")
