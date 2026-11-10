# Hygiene negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/hygiene_case.cmake
#
# Every offending string is assembled from fragments at test time (and token
# bodies from string(RANDOM)) into a sandbox under BIN/nc/<case>, so no
# committed line matches a hygiene pattern and no realistic literal can trip
# push protection (RESEARCH.md Pitfall 10). Each negative case asserts a
# non-zero exit AND the gate's stable reason code through cmake/ExpectFail.cmake.

foreach(var CASE SRC BIN)
  if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
    message(FATAL_ERROR "hygiene-case: -D${var}= is required")
  endif()
endforeach()
if(NOT CASE MATCHES "^[a-z_]+$")
  message(FATAL_ERROR "hygiene-case: bad case name: ${CASE}")
endif()

include("${SRC}/cmake/ExpectFail.cmake")

set(HYGIENE sh "${SRC}/scripts/hygiene.sh")

# The hook case inspects a file and owns no sandbox; every other case rebuilds
# its sandbox from scratch.
set(dir "${BIN}/nc/${CASE}")
if(NOT CASE STREQUAL "hook")
  file(REMOVE_RECURSE "${dir}")
  file(MAKE_DIRECTORY "${dir}")
endif()

# ---- fragment helpers -----------------------------------------------------
set(UPPER_DIGITS "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
set(ALNUM "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")

# rand(<out-var> <length> <alphabet>)
function(rand out length alphabet)
  string(RANDOM LENGTH ${length} ALPHABET "${alphabet}" body)
  set(${out} "${body}" PARENT_SCOPE)
endfunction()

function(mac_home out)
  string(CONCAT v "/Use" "rs/someone/x")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()

# Each builder returns one offending line (or an allowed line) assembled here.
function(line_aws out)
  rand(body 16 "${UPPER_DIGITS}")
  string(CONCAT v "id AK" "IA${body}")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()
function(line_github out)
  rand(body 36 "${ALNUM}")
  string(CONCAT v "gh gh" "p_${body}")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()
function(line_slack out)
  rand(body 24 "${ALNUM}")
  string(CONCAT v "slack xo" "xb-${body}")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()
function(line_assign out)
  rand(body 32 "${ALNUM}")
  string(CONCAT v "api" "_key = ${body}")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()

# j(<out-var> <fragment>...) concatenates fragments without list separators.
# Fragments never contain semicolons, so ARGN round-trips intact.
function(j out)
  string(JOIN "" joined ${ARGN})
  set(${out} "${joined}" PARENT_SCOPE)
endfunction()

# Allowed content in the shapes the offenders use, for the positive control.
function(write_clean_sandbox root)
  j(noreply "szSomeone" "@users.noreply." "github.com")
  j(example "someone" "@exam" "ple.com")
  j(tilde "~/proj" "ects/x")
  j(runner "/ho" "me/runner/work")
  rand(not_a_secret 8 "${ALNUM}")
  file(WRITE "${root}/keys.txt"
    "contact ${noreply}\nmail ${example}\npath ${tilde}\nci ${runner}\ntoken = ${not_a_secret}\n")
  # A newline in the file name still gets scanned, and still passes when clean.
  file(WRITE "${root}/a\nb.txt" "path ${tilde}\n")
endfunction()

# run_hygiene_lines(<out-var> <sandbox>) returns the raw output of --dir.
function(run_hygiene_dir out root)
  execute_process(COMMAND ${HYGIENE} --dir "${root}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text)
  set(${out} "${text}" PARENT_SCOPE)
  set(${out}_rc "${rc}" PARENT_SCOPE)
endfunction()

# expect_findings(<sandbox> <code> <count>) proves each planted line is found,
# not just the first: <count> findings of <code>, and no matched text echoed.
function(expect_findings root code count)
  run_hygiene_dir(text "${root}")
  string(REGEX MATCHALL "hygiene: ${code}: [^\n]*:[0-9]+" hits "${text}")
  list(LENGTH hits found)
  if(NOT found EQUAL count)
    message(FATAL_ERROR
      "hygiene-case: wanted ${count} '${code}' findings, got ${found}:\n${text}")
  endif()
endfunction()

# ---- cases ----------------------------------------------------------------
if(CASE STREQUAL "credential")
  line_aws(l1)
  line_github(l2)
  line_slack(l3)
  line_assign(l4)
  file(WRITE "${dir}/keys.txt" "${l1}\n${l2}\n${l3}\n${l4}\n")
  gbl_expect_fail(COMMAND ${HYGIENE} --dir "${dir}" REGEX "hygiene: credential")
  expect_findings("${dir}" credential 4)

elseif(CASE STREQUAL "private_key")
  j(header "-----" "BEGIN RSA PRIV" "ATE KEY-----")
  file(WRITE "${dir}/id.txt" "${header}\nnot-real\n")
  gbl_expect_fail(COMMAND ${HYGIENE} --dir "${dir}" REGEX "hygiene: credential")

elseif(CASE STREQUAL "email")
  j(addr "contact" "@" "nowhere.invalid")
  file(WRITE "${dir}/mail.txt" "reach me at ${addr}\n")
  gbl_expect_fail(COMMAND ${HYGIENE} --dir "${dir}" REGEX "hygiene: email")

elseif(CASE STREQUAL "home_path")
  # macOS form, Linux form, and the Windows form are three separate findings.
  mac_home(m)
  j(linux "/ho" "me/someone/x")
  j(win "C:" "\\Us" "ers\\someone")
  file(WRITE "${dir}/paths.txt" "${m}\n${linux}\n${win}\n")
  gbl_expect_fail(COMMAND ${HYGIENE} --dir "${dir}" REGEX "hygiene: home-path")
  expect_findings("${dir}" home-path 3)

elseif(CASE STREQUAL "home_path_encoded")
  j(enc "-Use" "rs-someone-")
  file(WRITE "${dir}/enc.txt" "dir ${enc}project\n")
  gbl_expect_fail(COMMAND ${HYGIENE} --dir "${dir}" REGEX "hygiene: home-path")

elseif(CASE STREQUAL "newline_name")
  mac_home(m)
  # The planted file's name contains a newline: a newline-split scanner skips it.
  file(WRITE "${dir}/a\nb.txt" "${m}\n")
  gbl_expect_fail(COMMAND ${HYGIENE} --dir "${dir}" REGEX "hygiene: home-path")

elseif(CASE STREQUAL "clean")
  write_clean_sandbox("${dir}")
  gbl_expect_pass(COMMAND ${HYGIENE} --dir "${dir}" REGEX "hygiene: ok \\(2 files\\)")

elseif(CASE STREQUAL "usage")
  gbl_expect_fail(COMMAND ${HYGIENE} --bogus REGEX "hygiene: usage" EXIT 2)

else()
  message(FATAL_ERROR "hygiene-case: unknown case: ${CASE}")
endif()
