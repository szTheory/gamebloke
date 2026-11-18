cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN CASE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "playable0-guest: missing required argument ${required}")
  endif()
endforeach()
get_filename_component(SRC "${SRC}" ABSOLUTE)
get_filename_component(BIN "${BIN}" ABSOLUTE)

if(NOT CASE STREQUAL "admission")
  message(FATAL_ERROR "playable0-guest: unknown case ${CASE}")
endif()

set(generator "${BIN}/gbl_instruction_image")
if(DEFINED GENERATOR AND NOT "${GENERATOR}" STREQUAL "")
  set(generator "${GENERATOR}")
endif()
set(output_dir "${BIN}/playable0-admission")
set(guest "${output_dir}/owned-playable0.gba")
if(NOT EXISTS "${generator}" OR IS_DIRECTORY "${generator}")
  message(FATAL_ERROR "playable0-guest: local instruction generator is missing")
endif()
file(MAKE_DIRECTORY "${output_dir}")
execute_process(
  COMMAND "${generator}" "${output_dir}" --include-local-playable0
  RESULT_VARIABLE generate_result
  OUTPUT_VARIABLE generate_output
  ERROR_VARIABLE generate_error)
if(NOT "${generate_result}" STREQUAL "0")
  message(FATAL_ERROR
    "playable0-guest: explicit local guest generation failed (${generate_result}); ${generate_error}${generate_output}")
endif()
if(NOT EXISTS "${guest}" OR IS_DIRECTORY "${guest}")
  message(FATAL_ERROR "playable0-guest: generated local guest is missing")
endif()
file(SIZE "${guest}" guest_size)
if(guest_size LESS 1)
  message(FATAL_ERROR "playable0-guest: generated local guest is empty")
endif()
message(STATUS "playable0-guest: original local guest generation admitted")
