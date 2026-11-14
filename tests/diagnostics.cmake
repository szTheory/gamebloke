# All registrations live in top-directory scope, like tests/core.cmake.
include("${CMAKE_SOURCE_DIR}/cmake/DiagnosticMatrix.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/DiagnosticSkip.cmake")
set(diag_group all)
if(GBL_LANE STREQUAL "diagnostics")
  set(diag_group "${GBL_DIAGNOSTIC_GROUP}")
endif()
gbl_diagnostic_matrix("${CMAKE_SOURCE_DIR}" "${diag_group}")
if(GBL_LANE STREQUAL "diagnostics")
  add_executable(cpu_test tests/cpu_test.c)
  target_link_libraries(cpu_test PRIVATE gamebloke_core gbl_warnings)
  # These literal selectors are the same complete registry exercised by the
  # native core lane and cpu.pos.clean. Do not claim an unregistered selector.
  file(READ "${CMAKE_SOURCE_DIR}/tests/cpu_test.c" native_registry)
  string(REGEX MATCHALL "CPU_TEST_GROUP\\(\"[a-z0-9_]+\"" native_groups "${native_registry}")
  foreach(entry IN LISTS native_groups)
    string(REGEX REPLACE "CPU_TEST_GROUP\\(\"([a-z0-9_]+)\"" "\\1" selector "${entry}")
    gbl_add_test(NAME "diagnostics.native.${selector}" LABELS diagnostics cpu unit
      COMMAND "$<TARGET_FILE:cpu_test>" "${selector}")
  endforeach()
endif()
string(RANDOM LENGTH 24 ALPHABET 0123456789abcdef diag_token)
set(required OFF)
set(skippable SKIPPABLE)
if(GBL_LANE STREQUAL "diagnostics" OR GBL_REQUIRE_DIAGNOSTICS)
  set(required ON)
endif()
# Inventories stay literal when an optional native lane is made mandatory.
# REQUIRED is enforced by setup/case/set commands and disables CTest skip
# matching. Dedicated diagnostic inventories permit no skips at all.
if(GBL_LANE STREQUAL "diagnostics")
  set(skippable "")
endif()
set(diag_args "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
  "-DTOKEN=${diag_token}" "-DGROUP=${diag_group}"
  "-DSTAGE=${GBL_DIAGNOSTIC_STAGE}" "-DREQUIRED=${required}")
gbl_add_test(NAME diagnostics.execute LABELS diagnostics integration ${skippable}
  COMMAND "${CMAKE_COMMAND}" ${diag_args}
    "-DTOOLCHAIN_ROOT=${GBL_TOOLCHAIN_ROOT_ABSOLUTE}" "-DHOST=${GBL_DIAGNOSTIC_HOST}"
    -P "${CMAKE_SOURCE_DIR}/tests/gates/diagnostic_setup.cmake")
set_tests_properties(diagnostics.execute PROPERTIES FIXTURES_SETUP diagnostic_execution RUN_SERIAL TRUE)
gbl_diagnostic_skip(diagnostics.execute "${required}")
set(case_tests "")
foreach(fid IN LISTS GBL_DIAG_OUTPUTS)
  set(row "${GBL_DIAG_ROW_${fid}}")
  string(JSON kind GET "${row}" source_test kind)
  set(case_reason "")
  if(kind STREQUAL "guest-stop")
    string(JSON case_reason GET "${row}" source_test expected_reason)
  endif()
  set(name "diagnostics.case.${fid}")
  gbl_add_test(NAME "${name}" LABELS diagnostics integration ${skippable}
    COMMAND "${CMAKE_COMMAND}" ${diag_args} -DCASE=case "-DFIXTURE=${fid}"
      "-DEXPECTED_STOP_REASON=${case_reason}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/diagnostic_receipt.cmake")
  set_tests_properties("${name}" PROPERTIES FIXTURES_REQUIRED diagnostic_execution)
  gbl_diagnostic_skip("${name}" "${required}")
  list(APPEND case_tests "${name}")
endforeach()
foreach(api IN LISTS GBL_DIAG_APIS)
  string(SHA256 api_row_sha "${GBL_DIAG_API_${api}}")
  set(name "diagnostics.api.${api}")
  gbl_add_test(NAME "${name}" LABELS diagnostics cpu integration
    COMMAND "${CMAKE_COMMAND}" ${diag_args} -DCASE=api "-DAPI=${api}" "-DAPI_ROW_SHA256=${api_row_sha}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/diagnostic_receipt.cmake")
  set_tests_properties("${name}" PROPERTIES FIXTURES_REQUIRED diagnostic_execution)
  list(APPEND case_tests "${name}")
endforeach()
gbl_add_test(NAME diagnostics.execution_set LABELS diagnostics gate integration ${skippable}
  COMMAND "${CMAKE_COMMAND}" ${diag_args} -DCASE=guard
    -P "${CMAKE_SOURCE_DIR}/tests/gates/diagnostic_receipt.cmake")
set_tests_properties(diagnostics.execution_set PROPERTIES
  FIXTURES_REQUIRED diagnostic_execution DEPENDS "${case_tests}" RUN_SERIAL TRUE)
gbl_diagnostic_skip(diagnostics.execution_set "${required}")
if(GBL_LANE STREQUAL "diagnostics" AND NOT diag_group STREQUAL "thumb")
  # The ARM/all groups already own these actual ARM semantic and stop inputs.
  # This required gate has no skip property and consumes real setup evidence.
  gbl_add_test(NAME runner.relocation_admitted LABELS diagnostics gate integration runner
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      -DMODE=admitted -DGBL_REQUIRE_DIAGNOSTICS=ON
      -P "${CMAKE_SOURCE_DIR}/tests/gates/runner_relocation.cmake")
  set_tests_properties(runner.relocation_admitted PROPERTIES
    FIXTURES_REQUIRED diagnostic_execution RUN_SERIAL TRUE)
  # This control includes the full clean final gate before its negative input.
  # A separate clean invocation duplicated measured whole-ledger work without
  # adding another failure class.
  gbl_add_test(NAME diagnostics.coverage.final LABELS control diagnostics gate
    COMMAND "${CMAKE_COMMAND}" -DCASE=final-ledger-controls
      "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}/coverage-boundaries"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/coverage_case.cmake")
  gbl_add_test(NAME diagnostics.dependency.pipeline LABELS control diagnostics
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      "-DTOOLCHAIN_ROOT=${GBL_TOOLCHAIN_ROOT_ABSOLUTE}" "-DHOST=${GBL_DIAGNOSTIC_HOST}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/diagnostic_pipeline.cmake")
  set_tests_properties(diagnostics.dependency.pipeline PROPERTIES
    DEPENDS diagnostics.execution_set RUN_SERIAL TRUE)
  gbl_add_test(NAME diagnostics.receipt.controls LABELS control diagnostics
    COMMAND "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/diagnostic_receipt_controls.cmake")
  set_tests_properties(diagnostics.receipt.controls PROPERTIES
    FIXTURES_REQUIRED diagnostic_execution DEPENDS diagnostics.execution_set RUN_SERIAL TRUE)
endif()
gbl_add_test(NAME diagnostics.dependency.controls LABELS control diagnostics
  COMMAND "${CMAKE_COMMAND}" -DCASE=all "-DSRC=${CMAKE_SOURCE_DIR}"
    "-DBIN=${CMAKE_BINARY_DIR}" -P "${CMAKE_SOURCE_DIR}/tests/gates/diagnostic_dependency.cmake")
# Snapshot actual registration for evidence/review. The checked-in inventories
# remain literal and are still compared by the canonical deferred gate.
get_property(registered GLOBAL PROPERTY GBL_REGISTERED_TESTS)
list(SORT registered)
list(JOIN registered "\n" registration)
file(WRITE "${CMAKE_BINARY_DIR}/diagnostic-registration.txt" "${registration}\n")
