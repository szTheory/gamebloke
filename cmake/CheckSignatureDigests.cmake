# Checks on the committed signature digest lists (D-08).
#
#   cmake -DMODE=wellformed|real-logo|owner-local -DSRC=<source root> -P cmake/CheckSignatureDigests.cmake
#
# SRC is always passed, so the controls can point it at a sandbox copy of the
# repository layout (scripts/ and tests/signatures/). The checker under test is
# always this file; only its inputs are planted.
#
# wellformed: fails at the first condition with `signature-digests: <reason>: <detail>`:
#   missing-list        logo.sha256 or bios.sha256 does not exist under SRC/tests/signatures
#   bad-bios-row        a bios row is not 64 lowercase hex, one space, and a label
#   no-gba-bios         no bios row carries the label gba-bios
#   logo-row-count      the logo list does not have exactly one non-comment row
#   bad-logo-row        that row is neither `<64hex> <label>` nor `owner-pending <label>`
#   wrong-default-path  the scan's default list paths are not these files
# Success prints `signature-digests: wellformed`.
#
# real-logo: prints `SKIPPED: logo digest owner-pending (...)` while the row is
# the marker, else `signature-digests: real logo digest present`.
#
# owner-local: prints `SKIPPED: GBL_OWNER_LOGO_FILE not set` when the variable is
# empty; else compares the owner helper's digest of that file with the logo row
# (`owner-helper-failed`, `owner-file-mismatch`, `owner-file-match`). Never prints
# file bytes.

foreach(var MODE SRC)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "signature-digests: usage: -D${var}= is required")
  endif()
endforeach()

set(LOGO_LIST "${SRC}/tests/signatures/logo.sha256")
set(BIOS_LIST "${SRC}/tests/signatures/bios.sha256")
string(REPEAT "[0-9a-f]" 64 HEX64)

# rows(<out-var> <file>): non-comment, non-empty lines, as a list. Digest rows
# hold no semicolons or brackets, and a row that does is rejected below as
# malformed whichever way the list splits it.
function(rows out file)
  file(STRINGS "${file}" lines REGEX "^[^#]")
  set(${out} "${lines}" PARENT_SCOPE)
endfunction()

function(check_wellformed)
  # (1) missing-list
  foreach(list IN ITEMS "${LOGO_LIST}" "${BIOS_LIST}")
    if(NOT EXISTS "${list}")
      message(FATAL_ERROR "signature-digests: missing-list: ${list}")
    endif()
  endforeach()

  # (2) bad-bios-row, (3) no-gba-bios
  rows(bios_rows "${BIOS_LIST}")
  set(have_gba FALSE)
  foreach(row IN LISTS bios_rows)
    if(NOT row MATCHES "^${HEX64} [^ ]+$")
      message(FATAL_ERROR "signature-digests: bad-bios-row: ${BIOS_LIST}")
    endif()
    if(row MATCHES " gba-bios$")
      set(have_gba TRUE)
    endif()
  endforeach()
  if(NOT have_gba)
    message(FATAL_ERROR "signature-digests: no-gba-bios: ${BIOS_LIST}")
  endif()

  # (4) logo-row-count, (5) bad-logo-row
  rows(logo_rows "${LOGO_LIST}")
  list(LENGTH logo_rows logo_count)
  if(NOT logo_count EQUAL 1)
    message(FATAL_ERROR "signature-digests: logo-row-count: wanted 1 row, found ${logo_count} in ${LOGO_LIST}")
  endif()
  if(NOT logo_rows MATCHES "^${HEX64} [^ ]+$" AND NOT logo_rows MATCHES "^owner-pending [^ ]+$")
    message(FATAL_ERROR "signature-digests: bad-logo-row: ${LOGO_LIST}")
  endif()

  # (6) wrong-default-path: the scan must read these very files.
  execute_process(
    COMMAND sh "${SRC}/scripts/signature-scan.sh" --print-config
    RESULT_VARIABLE rc OUTPUT_VARIABLE config ERROR_VARIABLE config)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "signature-digests: wrong-default-path: --print-config failed (${rc}): ${config}")
  endif()
  foreach(pair IN ITEMS "logo-list|${LOGO_LIST}" "bios-list|${BIOS_LIST}")
    string(REPLACE "|" ";" pair "${pair}")
    list(GET pair 0 key)
    list(GET pair 1 expected)
    if(NOT config MATCHES "(^|\n)${key}: ([^\n]*)")
      message(FATAL_ERROR "signature-digests: wrong-default-path: no ${key} line in --print-config output")
    endif()
    set(printed "${CMAKE_MATCH_2}")
    file(REAL_PATH "${printed}" printed_real)
    file(REAL_PATH "${expected}" expected_real)
    if(NOT printed_real STREQUAL expected_real)
      message(FATAL_ERROR "signature-digests: wrong-default-path: ${key} is ${printed_real}, wanted ${expected_real}")
    endif()
  endforeach()
  message("signature-digests: wellformed")
endfunction()

# logo_row(<out-var>): the single logo row, after the wellformed check has run.
function(logo_row out)
  if(NOT EXISTS "${LOGO_LIST}")
    message(FATAL_ERROR "signature-digests: missing-list: ${LOGO_LIST}")
  endif()
  rows(logo_rows "${LOGO_LIST}")
  list(LENGTH logo_rows logo_count)
  if(NOT logo_count EQUAL 1)
    message(FATAL_ERROR "signature-digests: logo-row-count: wanted 1 row, found ${logo_count} in ${LOGO_LIST}")
  endif()
  set(${out} "${logo_rows}" PARENT_SCOPE)
endfunction()

if(MODE STREQUAL "wellformed")
  check_wellformed()

elseif(MODE STREQUAL "real-logo")
  logo_row(row)
  if(row MATCHES "^owner-pending ")
    message("SKIPPED: logo digest owner-pending (tests/signatures/logo.sha256; owner action in plan 11)")
  elseif(row MATCHES "^${HEX64} [^ ]+$")
    message("signature-digests: real logo digest present")
  else()
    message(FATAL_ERROR "signature-digests: bad-logo-row: ${LOGO_LIST}")
  endif()

elseif(MODE STREQUAL "owner-local")
  if("$ENV{GBL_OWNER_LOGO_FILE}" STREQUAL "")
    message("SKIPPED: GBL_OWNER_LOGO_FILE not set")
  else()
    logo_row(row)
    # The helper hashes with the CMake that runs this check.
    set(ENV{CMAKE} "${CMAKE_COMMAND}")
    execute_process(
      COMMAND sh "${SRC}/scripts/signature-digest.sh" logo "$ENV{GBL_OWNER_LOGO_FILE}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE helper_out ERROR_VARIABLE helper_err)
    if(NOT rc EQUAL 0)
      message(FATAL_ERROR "signature-digests: owner-helper-failed: ${helper_err}${helper_out}")
    endif()
    string(REGEX MATCH "^${HEX64}" helper_digest "${helper_out}")
    string(REGEX MATCH "^${HEX64}" row_digest "${row}")
    if("${helper_digest}" STREQUAL "" OR NOT "${helper_digest}" STREQUAL "${row_digest}")
      message(FATAL_ERROR "signature-digests: owner-file-mismatch: the owner file's logo window digest is not the committed row")
    endif()
    message("signature-digests: owner-file-match")
  endif()

else()
  message(FATAL_ERROR "signature-digests: usage: unknown MODE '${MODE}'")
endif()
