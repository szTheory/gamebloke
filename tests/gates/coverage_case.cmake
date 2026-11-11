cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS CASE SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "coverage-case: missing-${required}")
  endif()
endforeach()
if(NOT CASE STREQUAL "ledger-controls")
  message(FATAL_ERROR "coverage-case: unsupported-case")
endif()

get_filename_component(source_root "${SRC}" ABSOLUTE)
get_filename_component(binary_root "${BIN}" ABSOLUTE)
set(matrix "${source_root}/diagnostics/armv4t/cases.json")
set(checker "${source_root}/cmake/CheckArmCoverage.cmake")
set(builder "${source_root}/cmake/BuildArmDiagnostics.cmake")

foreach(required_path IN ITEMS "${matrix}" "${checker}" "${builder}")
  if(NOT EXISTS "${required_path}" OR IS_DIRECTORY "${required_path}" OR
     IS_SYMLINK "${required_path}")
    message(FATAL_ERROR "coverage-case: required-ledger-artifact-missing: ${required_path}")
  endif()
endforeach()
