cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS INPUT OUTPUT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "gba-header: missing-${required}")
  endif()
endforeach()

if(NOT EXISTS "${INPUT}" OR IS_DIRECTORY "${INPUT}" OR IS_SYMLINK "${INPUT}")
  message(FATAL_ERROR "gba-header: input-not-regular")
endif()
if(IS_SYMLINK "${OUTPUT}")
  message(FATAL_ERROR "gba-header: output-is-symlink")
endif()
if(INPUT STREQUAL OUTPUT)
  message(FATAL_ERROR "gba-header: input-equals-output")
endif()

file(SIZE "${INPUT}" input_size)
math(EXPR input_size_odd "${input_size} % 2")
if(input_size LESS 192 OR input_size GREATER 33554432 OR
   NOT input_size_odd EQUAL 0)
  message(FATAL_ERROR "gba-header: invalid-rom-length")
endif()

# The original diagnostic deliberately leaves the Nintendo-logo window zeroed.
# A matching logo or copied logo fragment is refused before the file is written.
file(READ "${INPUT}" logo_window OFFSET 4 LIMIT 156 HEX)
if(NOT logo_window MATCHES "^0+$")
  message(FATAL_ERROR "gba-header: logo-window-not-zero")
endif()

# GBA checksum: 0 - sum(header[0xA0..0xBC]) - 0x19, modulo 256.
file(READ "${INPUT}" checksum_bytes OFFSET 160 LIMIT 29 HEX)
string(LENGTH "${checksum_bytes}" checksum_hex_length)
if(NOT checksum_hex_length EQUAL 58)
  message(FATAL_ERROR "gba-header: truncated-header")
endif()
set(header_sum 0)
foreach(index RANGE 0 28)
  math(EXPR byte_offset "${index} * 2")
  string(SUBSTRING "${checksum_bytes}" ${byte_offset} 2 byte_hex)
  math(EXPR byte_value "0x${byte_hex}")
  math(EXPR header_sum "(${header_sum} + ${byte_value}) & 255")
endforeach()
math(EXPR checksum "(0 - ${header_sum} - 0x19) & 255")

get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
set(staged_output "${OUTPUT}.tmp")
set(checksum_file "${OUTPUT}.checksum.tmp")
file(REMOVE "${staged_output}" "${checksum_file}")
file(COPY_FILE "${INPUT}" "${staged_output}")

if(checksum EQUAL 0)
  execute_process(
    COMMAND dd if=/dev/zero "of=${staged_output}" bs=1 count=1 seek=189
      conv=notrunc
    RESULT_VARIABLE patch_result
    OUTPUT_QUIET ERROR_QUIET)
else()
  string(ASCII ${checksum} checksum_byte)
  file(WRITE "${checksum_file}" "${checksum_byte}")
  execute_process(
    COMMAND dd "of=${staged_output}" bs=1 seek=189 conv=notrunc
    INPUT_FILE "${checksum_file}"
    RESULT_VARIABLE patch_result
    OUTPUT_QUIET ERROR_QUIET)
endif()
file(REMOVE "${checksum_file}")
if(NOT "${patch_result}" STREQUAL "0")
  file(REMOVE "${staged_output}")
  message(FATAL_ERROR "gba-header: checksum-write-failed")
endif()

file(SIZE "${staged_output}" staged_size)
file(READ "${staged_output}" written_checksum OFFSET 189 LIMIT 1 HEX)
math(EXPR checksum_hex "${checksum}" OUTPUT_FORMAT HEXADECIMAL)
string(REGEX REPLACE "^0x" "" expected_checksum "${checksum_hex}")
string(TOLOWER "${expected_checksum}" expected_checksum)
string(LENGTH "${expected_checksum}" expected_checksum_length)
if(expected_checksum_length LESS 2)
  set(expected_checksum "0${expected_checksum}")
endif()
if(NOT staged_size EQUAL input_size OR
   NOT "${written_checksum}" STREQUAL "${expected_checksum}")
  file(REMOVE "${staged_output}")
  message(FATAL_ERROR "gba-header: checksum-postcondition-failed")
endif()

file(RENAME "${staged_output}" "${OUTPUT}" RESULT rename_result)
if(NOT "${rename_result}" STREQUAL "0")
  file(REMOVE "${staged_output}")
  message(FATAL_ERROR "gba-header: output-promotion-failed")
endif()
message(STATUS "gba-header: fixed size=${input_size} checksum=0x${expected_checksum}")
