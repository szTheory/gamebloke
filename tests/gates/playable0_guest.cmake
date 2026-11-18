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
foreach(name IN ITEMS owned-playable0.gba owned-playable0-input-disabled.gba
    owned-playable0-wrong-color.gba owned-playable0-wrong-address.gba)
  set(guest "${output_dir}/${name}")
  if(NOT EXISTS "${guest}" OR IS_DIRECTORY "${guest}")
    message(FATAL_ERROR "playable0-guest: generated local variant is missing: ${name}")
  endif()
  file(SIZE "${guest}" guest_size)
  if(NOT guest_size EQUAL 2048)
    message(FATAL_ERROR
      "playable0-guest: generated local variant ${name} has unexpected size ${guest_size}")
  endif()
endforeach()

# An ordinary generator invocation must not emit local-only guest variants.
set(default_dir "${BIN}/playable0-default-generator")
file(MAKE_DIRECTORY "${default_dir}")
execute_process(
  COMMAND "${generator}" "${default_dir}"
  RESULT_VARIABLE default_result
  OUTPUT_VARIABLE default_output
  ERROR_VARIABLE default_error)
if(NOT "${default_result}" STREQUAL "0")
  message(FATAL_ERROR
    "playable0-guest: default generator failed (${default_result}); ${default_error}${default_output}")
endif()
foreach(name IN ITEMS owned-playable0.gba owned-playable0-input-disabled.gba
    owned-playable0-wrong-color.gba owned-playable0-wrong-address.gba)
  if(EXISTS "${default_dir}/${name}")
    message(FATAL_ERROR
      "playable0-guest: default generator leaked local-only variant ${name}")
  endif()
endforeach()

# The lane refuses CI before any build artifact can be generated.
set(ci_refusal_dir "${BIN}/playable0-ci-refusal")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env CI=true "${CMAKE_COMMAND}"
    -S "${SRC}" -B "${ci_refusal_dir}" -DGBL_LANE=local-playable0
  RESULT_VARIABLE ci_result
  OUTPUT_VARIABLE ci_output
  ERROR_VARIABLE ci_error)
if("${ci_result}" STREQUAL "0" OR
    NOT "${ci_output}${ci_error}" MATCHES "local-playable0 lane is prohibited when CI=true")
  message(FATAL_ERROR
    "playable0-guest: CI guard did not refuse configure; exit=${ci_result}; ${ci_error}${ci_output}")
endif()
if(EXISTS "${ci_refusal_dir}/generated/fixtures/owned-playable0.gba")
  message(FATAL_ERROR "playable0-guest: CI refusal left a local guest artifact")
endif()
message(STATUS "playable0-guest: original local guest generation admitted")
