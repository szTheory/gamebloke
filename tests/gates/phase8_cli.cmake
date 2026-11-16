cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase8-cli: usage: -D${required}= is required")
  endif()
endforeach()
get_filename_component(BIN "${BIN}" ABSOLUTE)
set(control "${BIN}/gamebloke-run-control")
set(fixture "${BIN}/generated/fixtures/owned-arm-pass.gba")
set(scratch "${BIN}/phase8-cli")
if(NOT EXISTS "${control}" OR NOT EXISTS "${fixture}")
  message(FATAL_ERROR "phase8-cli: required-input-missing")
endif()
file(MAKE_DIRECTORY "${scratch}")
set(script "${scratch}/cycle-zero.txt")
file(WRITE "${script}" "0 1\n")

execute_process(
  COMMAND "${control}" --rom "${fixture}" --fixture synthetic-control-pass
    --cycles 1000 --input-script "${script}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)
if(NOT "${result}" STREQUAL "0" OR NOT "${error}" STREQUAL "")
  message(FATAL_ERROR
    "phase8-cli: valid-cycle-zero-script expected exit 0; exit=${result}; stderr=${error}")
endif()
string(JSON keyinput ERROR_VARIABLE json_error GET "${output}" phase8 keyinput)
if(NOT "${json_error}" STREQUAL "NOTFOUND" OR NOT "${keyinput}" STREQUAL "1022")
  message(FATAL_ERROR
    "phase8-cli: valid-cycle-zero-script expected active-low KEYINPUT=1022; got=${keyinput}; ${json_error}")
endif()
string(JSON input_kind ERROR_VARIABLE kind_error GET "${output}"
  input_timeline kind)
if(NOT "${kind_error}" STREQUAL "NOTFOUND" OR
    NOT "${input_kind}" STREQUAL "script")
  message(FATAL_ERROR
    "phase8-cli: valid-cycle-zero-script expected script identity; got=${input_kind}; ${kind_error}")
endif()
message("phase8-cli: cycle-zero A press produced KEYINPUT 1022")
