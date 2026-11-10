# Signature scan negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/signature_case.cmake
#
# Every stand-in is synthesized at test time under BIN/nc/<case> with dd from
# /dev/urandom and /dev/zero: no real logo, BIOS, or ROM bytes exist anywhere in
# this repository or in these sandboxes. Stand-in digests reach the scan only
# through --extra-digests, and the lists under tests/signatures/ are never
# modified (the well-formedness cases mutate a COPY of them under BIN).
# Each negative case asserts a non-zero exit AND the gate's stable reason code
# through cmake/ExpectFail.cmake.

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "signature-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "signature-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")

set(dir "${BIN}/nc/${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")

# Every tool call runs with CMAKE naming this CMake, so the 3.25 floor run hashes
# with the same binary (and an owner-set GBL_OWNER_LOGO_FILE never reaches a
# control unless the control passes it).
set(ENVCMD "${CMAKE_COMMAND}" -E env "CMAKE=${CMAKE_COMMAND}")
set(SCAN ${ENVCMD} sh "${SRC}/scripts/signature-scan.sh")
set(DIGEST ${ENVCMD} sh "${SRC}/scripts/signature-digest.sh")

# ---- synthesis helpers ------------------------------------------------------
function(dd_run)
  execute_process(COMMAND dd ${ARGN}
    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "signature-case: dd ${ARGN} failed (${rc})")
  endif()
endfunction()

function(require_size path want)
  file(SIZE "${path}" got)
  if(NOT got EQUAL want)
    message(FATAL_ERROR "signature-case: ${path} is ${got} bytes, wanted ${want}")
  endif()
endfunction()

# 192 bytes: 0..3 zero, 4..159 random (the 156-byte window), 160..191 zero. The
# zero bytes make it deterministically binary.
function(make_logo path)
  dd_run(if=/dev/zero "of=${path}" bs=192 count=1)
  dd_run(if=/dev/urandom "of=${path}" bs=4 seek=1 count=39 conv=notrunc)
  require_size("${path}" 192)
endfunction()

function(make_random path size)
  dd_run(if=/dev/urandom "of=${path}" "bs=${size}" count=1)
  require_size("${path}" ${size})
endfunction()

# 64 bytes with a leading zero byte: binary, and under the 160-byte window floor.
function(make_binary64 path)
  dd_run(if=/dev/zero "of=${path}" bs=64 count=1)
  dd_run(if=/dev/urandom "of=${path}" bs=1 seek=1 count=63 conv=notrunc)
  require_size("${path}" 64)
endfunction()

# helper_digest(<out-var> <logo|bios> <file>): the 64-hex digest the owner
# helper prints for <file>.
function(helper_digest out kind file)
  execute_process(COMMAND ${DIGEST} ${kind} "${file}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "signature-case: helper ${kind} failed (${rc}): ${err}${text}")
  endif()
  string(REGEX MATCH "^[0-9a-f]+" hex "${text}")
  string(LENGTH "${hex}" len)
  if(NOT len EQUAL 64)
    message(FATAL_ERROR "signature-case: helper printed no 64-hex digest")
  endif()
  set(${out} "${hex}" PARENT_SCOPE)
endfunction()

# Scan sandbox layout: scanned files live in ${dir}/tree; control files
# (allowlist, extra digests) stay outside it so they are not scanned.
file(MAKE_DIRECTORY "${dir}/tree")
set(TREE "${dir}/tree")
set(ALLOW "${dir}/allowlist.txt")
set(EXTRA "${dir}/extra.txt")
file(WRITE "${ALLOW}" "# sandbox allowlist\n")

function(allowlist)
  foreach(name IN LISTS ARGN)
    file(APPEND "${ALLOW}" "${name}\n")
  endforeach()
endfunction()

function(scan_cmd out)
  set(cmd ${SCAN} --dir "${TREE}" --allowlist "${ALLOW}")
  if(EXISTS "${EXTRA}")
    list(APPEND cmd --extra-digests "${EXTRA}")
  endif()
  set(${out} "${cmd}" PARENT_SCOPE)
endfunction()

# ---- sandbox source copy (well-formedness and owner-local cases) -----------
# The repository layout the checker reads: scripts/ and tests/signatures/.
function(make_src_copy dst)
  file(MAKE_DIRECTORY "${dst}/scripts" "${dst}/tests")
  file(COPY "${SRC}/scripts/signature-scan.sh" "${SRC}/scripts/signature-digest.sh"
    DESTINATION "${dst}/scripts")
  file(COPY "${SRC}/scripts/lib" DESTINATION "${dst}/scripts")
  file(COPY "${SRC}/tests/signatures" DESTINATION "${dst}/tests")
endfunction()

# mutate_line(<file> <anchor> <find> <replace>): on the first line containing
# <anchor>, replace the first <find> by <replace>. A stale case that no longer
# matches fails with `mutate: no-match` instead of testing an unmutated copy.
function(mutate_line path anchor find replace)
  file(READ "${path}" content)
  string(FIND "${content}" "${anchor}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "mutate: no-match: anchor not found in ${path}")
  endif()
  string(SUBSTRING "${content}" 0 ${pos} before)
  string(SUBSTRING "${content}" ${pos} -1 tail)
  string(FIND "${tail}" "\n" eol)
  if(eol EQUAL -1)
    string(LENGTH "${tail}" eol)
  endif()
  string(SUBSTRING "${tail}" 0 ${eol} line)
  string(SUBSTRING "${tail}" ${eol} -1 after)
  string(FIND "${line}" "${find}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "mutate: no-match: '${find}' not on the anchored line in ${path}")
  endif()
  string(SUBSTRING "${line}" 0 ${at} line_head)
  string(LENGTH "${find}" find_len)
  math(EXPR line_rest_at "${at} + ${find_len}")
  string(SUBSTRING "${line}" ${line_rest_at} -1 line_tail)
  file(WRITE "${path}" "${before}${line_head}${replace}${line_tail}${after}")
endfunction()

# drop_line(<file> <anchor>): remove the first line that contains <anchor>.
function(drop_line path anchor)
  file(READ "${path}" content)
  string(FIND "${content}" "${anchor}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "mutate: no-match: anchor not found in ${path}")
  endif()
  string(SUBSTRING "${content}" 0 ${pos} before)
  string(FIND "${before}" "\n" bol REVERSE)
  if(bol EQUAL -1)
    set(head "")
  else()
    math(EXPR keep "${bol} + 1")
    string(SUBSTRING "${before}" 0 ${keep} head)
  endif()
  string(SUBSTRING "${content}" ${pos} -1 tail)
  string(FIND "${tail}" "\n" eol)
  if(eol EQUAL -1)
    set(rest "")
  else()
    math(EXPR from "${eol} + 1")
    string(SUBSTRING "${tail}" ${from} -1 rest)
  endif()
  file(WRITE "${path}" "${head}${rest}")
endfunction()

# logo_header(<out-var> <logo-file>): the comment lines of a logo list, so a
# rewritten sandbox list keeps its header whatever its row currently is.
function(logo_header out path)
  file(READ "${path}" content)
  string(REGEX REPLACE "\n[^#\n][^\n]*" "" header "${content}")
  set(${out} "${header}" PARENT_SCOPE)
endfunction()

# run_checker(<expect-fn> <mode> <regex-or-empty> [ENV <var=value>...]) runs the
# real checker against the sandbox source copy ${dir}/src.
function(run_checker expect mode regex)
  cmake_parse_arguments(PARSE_ARGV 3 R "" "" "ENV")
  set(cmd ${ENVCMD})
  foreach(assignment IN LISTS R_ENV)
    list(APPEND cmd "${assignment}")
  endforeach()
  list(APPEND cmd "${CMAKE_COMMAND}" -DMODE=${mode} "-DSRC=${dir}/src"
    -P "${SRC}/cmake/CheckSignatureDigests.cmake")
  if(expect STREQUAL "fail")
    gbl_expect_fail(COMMAND ${cmd} REGEX "${regex}")
  elseif("${regex}" STREQUAL "")
    gbl_expect_pass(COMMAND ${cmd})
  else()
    gbl_expect_pass(COMMAND ${cmd} REGEX "${regex}")
  endif()
endfunction()

# ---- scan cases -------------------------------------------------------------
if(CASE STREQUAL "logo")
  # The planted file is allowlisted: the allowlist must not exempt the logo rule.
  make_logo("${TREE}/standin.dat")
  allowlist(standin.dat)
  helper_digest(hex logo "${TREE}/standin.dat")
  # Independent window definition: bytes 4..159 taken one byte at a time, so a
  # shifted or truncated window in the scripts cannot agree with it.
  dd_run(if=${TREE}/standin.dat "of=${dir}/window.bin" bs=1 skip=4 count=156)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E sha256sum "${dir}/window.bin"
    RESULT_VARIABLE rc OUTPUT_VARIABLE sums)
  string(REGEX MATCH "^[0-9a-f]+" window_hex "${sums}")
  if(NOT rc EQUAL 0 OR NOT "${window_hex}" STREQUAL "${hex}")
    message(FATAL_ERROR "signature-case: helper digest is not the digest of bytes 4..159")
  endif()
  file(WRITE "${EXTRA}" "${hex} logo:stand-in\n")
  scan_cmd(cmd)
  gbl_expect_fail(COMMAND ${cmd} REGEX "signature: logo-window: standin\\.dat")
  # No window bytes (312 hex characters) are dumped.
  execute_process(COMMAND ${cmd} OUTPUT_VARIABLE out ERROR_VARIABLE out)
  string(REPEAT "[0-9a-f]" 312 dump)
  if(out MATCHES "${dump}")
    message(FATAL_ERROR "signature-case: scan output carries a window dump")
  endif()

elseif(CASE STREQUAL "bios")
  make_random("${TREE}/standin.dat" 16384)
  allowlist(standin.dat)
  helper_digest(hex bios "${TREE}/standin.dat")
  file(WRITE "${EXTRA}" "${hex} bios:stand-in\n")
  scan_cmd(cmd)
  gbl_expect_fail(COMMAND ${cmd} REGEX "signature: bios-digest: standin\\.dat")

elseif(CASE STREQUAL "bios_unlisted_digest")
  # Same size, no listed digest: size alone is not a finding.
  make_random("${TREE}/standin.dat" 16384)
  allowlist(standin.dat)
  scan_cmd(cmd)
  gbl_expect_pass(COMMAND ${cmd} REGEX "signature: ok")

elseif(CASE STREQUAL "rom_ext")
  file(WRITE "${TREE}/game.Gba" "text stand-in\n")
  scan_cmd(cmd)
  gbl_expect_fail(COMMAND ${cmd} REGEX "signature: rom-extension: game\\.Gba")

elseif(CASE STREQUAL "save_ext")
  file(WRITE "${TREE}/slot.sav" "text stand-in\n")
  scan_cmd(cmd)
  gbl_expect_fail(COMMAND ${cmd} REGEX "signature: rom-extension: slot\\.sav")

elseif(CASE STREQUAL "binary_unlisted")
  make_binary64("${TREE}/blob.dat")
  scan_cmd(cmd)
  gbl_expect_fail(COMMAND ${cmd} REGEX "signature: binary-not-allowlisted: blob\\.dat")

elseif(CASE STREQUAL "binary_allowlisted")
  make_binary64("${TREE}/blob.dat")
  allowlist(blob.dat)
  # A zero-byte file is text (grep -I exits 1 on it), so it is not reported.
  file(WRITE "${TREE}/empty.txt" "")
  scan_cmd(cmd)
  gbl_expect_pass(COMMAND ${cmd} REGEX "signature: ok \\(2 files\\)")

elseif(CASE STREQUAL "bad_digest_row")
  file(WRITE "${TREE}/note.txt" "text\n")
  string(REPEAT "a" 63 short)
  file(WRITE "${EXTRA}" "${short} logo:short-row\n")
  scan_cmd(cmd)
  gbl_expect_fail(COMMAND ${cmd} REGEX "signature: bad-digest-row")

elseif(CASE STREQUAL "digest_too_short")
  make_random("${dir}/short.dat" 100)
  gbl_expect_fail(COMMAND ${DIGEST} logo "${dir}/short.dat"
    REGEX "signature-digest: too-short")

elseif(CASE STREQUAL "digest_wrong_size")
  make_random("${dir}/short.dat" 100)
  gbl_expect_fail(COMMAND ${DIGEST} bios "${dir}/short.dat"
    REGEX "signature-digest: wrong-size")

# ---- digest-list well-formedness cases (B-1) --------------------------------
# Each mutates a sandbox COPY; `digests_copy` proves the unmutated copy is sound,
# so every control below fails only for its planted reason.
elseif(CASE STREQUAL "digests_copy")
  make_src_copy("${dir}/src")
  run_checker(pass wellformed "signature-digests: wellformed")

elseif(CASE STREQUAL "digests_missing_list")
  make_src_copy("${dir}/src")
  if(NOT EXISTS "${dir}/src/tests/signatures/bios.sha256")
    message(FATAL_ERROR "mutate: no-match: bios.sha256 absent from the copy")
  endif()
  file(REMOVE "${dir}/src/tests/signatures/bios.sha256")
  run_checker(fail wellformed "signature-digests: missing-list")

elseif(CASE STREQUAL "digests_bad_bios_row")
  make_src_copy("${dir}/src")
  string(REPEAT "b" 63 short)
  file(APPEND "${dir}/src/tests/signatures/bios.sha256" "${short} sixty-three\n")
  run_checker(fail wellformed "signature-digests: bad-bios-row")

elseif(CASE STREQUAL "digests_no_gba_bios")
  make_src_copy("${dir}/src")
  drop_line("${dir}/src/tests/signatures/bios.sha256" " gba-bios\n")
  run_checker(fail wellformed "signature-digests: no-gba-bios")

elseif(CASE STREQUAL "digests_logo_row_count")
  make_src_copy("${dir}/src")
  set(logo "${dir}/src/tests/signatures/logo.sha256")
  logo_header(header "${logo}")
  file(WRITE "${logo}"
    "${header}owner-pending nintendo-logo-window\nowner-pending other\n")
  run_checker(fail wellformed "signature-digests: logo-row-count")

elseif(CASE STREQUAL "digests_bad_logo_row")
  make_src_copy("${dir}/src")
  set(logo "${dir}/src/tests/signatures/logo.sha256")
  logo_header(header "${logo}")
  file(WRITE "${logo}" "${header}pending nintendo-logo-window\n")
  run_checker(fail wellformed "signature-digests: bad-logo-row")

elseif(CASE STREQUAL "digests_wrong_default_path")
  make_src_copy("${dir}/src")
  mutate_line("${dir}/src/scripts/signature-scan.sh"
    "GBL_LOGO_LIST_DEFAULT=" "logo.sha256" "logo-elsewhere.sha256")
  # The scan still parses: the elsewhere file is a copy of the logo list.
  file(COPY_FILE "${dir}/src/tests/signatures/logo.sha256"
    "${dir}/src/tests/signatures/logo-elsewhere.sha256")
  run_checker(fail wellformed "signature-digests: wrong-default-path")

# ---- owner-local cases (W-3) ------------------------------------------------
elseif(CASE STREQUAL "owner_file_mismatch")
  make_src_copy("${dir}/src")
  make_logo("${dir}/owner-file.dat")
  set(logo "${dir}/src/tests/signatures/logo.sha256")
  logo_header(header "${logo}")
  # A stand-in digest that is not the planted file's.
  string(RANDOM LENGTH 64 ALPHABET "0123456789abcdef" standin)
  file(WRITE "${logo}" "${header}${standin} nintendo-logo-window\n")
  run_checker(fail owner-local "signature-digests: owner-file-mismatch"
    ENV "GBL_OWNER_LOGO_FILE=${dir}/owner-file.dat")

elseif(CASE STREQUAL "owner_file_match")
  make_src_copy("${dir}/src")
  make_logo("${dir}/owner-file.dat")
  set(logo "${dir}/src/tests/signatures/logo.sha256")
  logo_header(header "${logo}")
  helper_digest(hex logo "${dir}/owner-file.dat")
  file(WRITE "${logo}" "${header}${hex} nintendo-logo-window\n")
  run_checker(pass owner-local "signature-digests: owner-file-match"
    ENV "GBL_OWNER_LOGO_FILE=${dir}/owner-file.dat")

else()
  message(FATAL_ERROR "signature-case: unknown case: ${CASE}")
endif()
