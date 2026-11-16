# Phase 7 timing-model contract and runtime/profile correspondence gate.
#
# The clean profile is checked before negative controls. Runtime rows are
# compared by direction, region, width, sequence class, and fetch/data role.
cmake_minimum_required(VERSION 3.25)

foreach(var SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "phase7-costs: -D${var}= is required")
  endif()
endforeach()

set(profile "${SRC}/profiles/agb.json")
file(READ "${profile}" profile_json)
string(JSON model_type ERROR_VARIABLE model_error TYPE "${profile_json}" timing_model)
if(NOT "${model_error}" STREQUAL "NOTFOUND" OR NOT "${model_type}" STREQUAL "OBJECT")
  message(FATAL_ERROR "phase7-costs: missing timing_model")
endif()
