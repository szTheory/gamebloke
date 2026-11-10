# Hygiene negative-control and positive-control cases.
#
#   cmake -DCASE=<name> -DSRC=<source dir> -DBIN=<build dir> -P tests/gates/hygiene_case.cmake
#
# Every offending string is assembled from fragments at test time (and token
# bodies from string(RANDOM)) into a sandbox under BIN/nc/<case>, so no
# committed line matches a hygiene pattern and no realistic literal can trip
# push protection (RESEARCH.md Pitfall 10). Each negative case asserts a
# non-zero exit AND the gate's stable reason code through cmake/ExpectFail.cmake.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

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

# The upper-case and compound-name assignment shapes (WR-07). Each builder
# splits the key word so no committed line reads as an assignment.
function(line_env_api_key out)
  rand(body 32 "${ALNUM}")
  string(CONCAT v "API" "_KEY=${body}")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()
function(line_env_aws out)
  rand(body 32 "${ALNUM}")
  string(CONCAT v "AWS_SEC" "RET_ACCESS_KEY = ${body}")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()
function(line_secret_key out)
  rand(body 32 "${ALNUM}")
  string(CONCAT v "sec" "ret_key = ${body}")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()
function(line_env_token out)
  rand(body 32 "${ALNUM}")
  string(CONCAT v "GITHUB_TO" "KEN: ${body}")
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
  # Short values after upper-case and compound key names are not credentials.
  string(CONCAT short_env "API" "_KEY=changeme")
  file(WRITE "${root}/keys.txt"
    "contact ${noreply}\nmail ${example}\npath ${tilde}\nci ${runner}\ntoken = ${not_a_secret}\n${short_env}\n")
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

# ---- git sandbox helpers (staged cases) -----------------------------------
# A real git repository under BIN, with a noreply identity assembled from
# fragments. The environment is scrubbed of GIT_* state so a run inside a hook
# or a CI job cannot leak into the sandbox, and cannot supply an identity.
set(STAGED_ENV "${CMAKE_COMMAND}" -E env
  --unset=GIT_DIR --unset=GIT_WORK_TREE --unset=GIT_INDEX_FILE
  --unset=GIT_AUTHOR_NAME --unset=GIT_AUTHOR_EMAIL
  --unset=GIT_COMMITTER_NAME --unset=GIT_COMMITTER_EMAIL)

function(git_in root)
  execute_process(COMMAND ${STAGED_ENV} git ${ARGN}
    WORKING_DIRECTORY "${root}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE text ERROR_VARIABLE text)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "hygiene-case: git ${ARGN} failed (${rc}) in sandbox:\n${text}")
  endif()
endfunction()

function(init_sandbox_repo root)
  git_in("${root}" -c init.defaultBranch=main init -q)
  j(noreply "szSomeone" "@users.noreply." "github.com")
  git_in("${root}" config user.email "${noreply}")
  git_in("${root}" config user.name "Sandbox")
endfunction()

# run_hook_case(<hook path> <regex>) runs the hook check on a planted path.
function(run_hook_case hook regex)
  gbl_expect_fail(
    COMMAND "${CMAKE_COMMAND}" -DCASE=hook "-DHOOK=${hook}"
      "-DSRC=${SRC}" "-DBIN=${BIN}" -P "${SRC}/tests/gates/hygiene_case.cmake"
    REGEX "${regex}")
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

elseif(CASE STREQUAL "credential_upper")
  # Four shapes, one per line; the gate must find each, and must print only
  # path:line (the planted value never reaches the output).
  line_env_api_key(l1)
  line_env_aws(l2)
  line_secret_key(l3)
  line_env_token(l4)
  file(WRITE "${dir}/env.txt" "${l1}\n${l2}\n${l3}\n${l4}\n")
  gbl_expect_fail(COMMAND ${HYGIENE} --dir "${dir}" REGEX "hygiene: credential")
  expect_findings("${dir}" credential 4)
  run_hygiene_dir(text "${dir}")
  foreach(l IN ITEMS l1 l2 l3 l4)
    string(REGEX REPLACE "^.*[=:] *" "" planted "${${l}}")
    string(FIND "${text}" "${planted}" at)
    if(NOT at EQUAL -1)
      message(FATAL_ERROR "hygiene-case: the gate echoed a planted value:\n${text}")
    endif()
  endforeach()

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

elseif(CASE STREQUAL "staged_home_path")
  # The scanner reads the staged blob, not the working file: stage the planted
  # path, then overwrite the working copy with clean text. Still a finding.
  mac_home(m)
  init_sandbox_repo("${dir}")
  file(WRITE "${dir}/notes.txt" "${m}\n")
  git_in("${dir}" add -- notes.txt)
  file(WRITE "${dir}/notes.txt" "nothing to see\n")
  gbl_expect_fail(
    COMMAND ${STAGED_ENV} ${HYGIENE} --staged
    WORKING_DIRECTORY "${dir}"
    REGEX "hygiene: home-path")

elseif(CASE STREQUAL "staged_identity")
  # Clean content, but the author address is outside the noreply set. The
  # address is built from fragments and never printed by the gate.
  init_sandbox_repo("${dir}")
  write_clean_sandbox("${dir}")
  git_in("${dir}" add -- .)
  j(bad "someone" "@elsewhere." "invalid")
  gbl_expect_fail(
    COMMAND ${STAGED_ENV} "GIT_AUTHOR_EMAIL=${bad}" ${HYGIENE} --staged
    WORKING_DIRECTORY "${dir}"
    REGEX "hygiene: identity: author")

elseif(CASE STREQUAL "staged_clean")
  init_sandbox_repo("${dir}")
  write_clean_sandbox("${dir}")
  git_in("${dir}" add -- .)
  gbl_expect_pass(
    COMMAND ${STAGED_ENV} ${HYGIENE} --staged
    WORKING_DIRECTORY "${dir}"
    REGEX "hygiene: ok \\(2 files\\)")

elseif(CASE STREQUAL "hook")
  # The opt-in hook must exist, be executable, and call the staged scan.
  # -DHOOK=<path> lets the controls point this check at planted copies.
  if(NOT DEFINED HOOK OR "${HOOK}" STREQUAL "")
    set(HOOK "${SRC}/.githooks/pre-commit")
  endif()
  if(NOT EXISTS "${HOOK}")
    message(FATAL_ERROR "hygiene: hook-missing: ${HOOK}")
  endif()
  # IF(IS_EXECUTABLE) is not available at the 3.25 floor; ask sh.
  execute_process(COMMAND sh -c "test -x \"$1\"" sh "${HOOK}" RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "hygiene: hook-not-executable: ${HOOK}")
  endif()
  file(STRINGS "${HOOK}" calls REGEX "^[^#]*scripts/hygiene\\.sh\"? +--staged")
  if(NOT calls)
    message(FATAL_ERROR "hygiene: hook-no-staged: ${HOOK}")
  endif()
  message("hygiene: hook ok")

elseif(CASE STREQUAL "hook_missing")
  run_hook_case("${dir}/absent-hook" "hygiene: hook-missing")

elseif(CASE STREQUAL "hook_not_executable")
  file(COPY_FILE "${SRC}/.githooks/pre-commit" "${dir}/pre-commit")
  file(CHMOD "${dir}/pre-commit" PERMISSIONS OWNER_READ OWNER_WRITE)
  run_hook_case("${dir}/pre-commit" "hygiene: hook-not-executable")

elseif(CASE STREQUAL "hook_no_staged")
  file(READ "${SRC}/.githooks/pre-commit" content)
  string(REPLACE "--staged" "" mutated "${content}")
  if(mutated STREQUAL content)
    message(FATAL_ERROR "mutate: no-match: the real hook has no --staged to remove")
  endif()
  file(WRITE "${dir}/pre-commit" "${mutated}")
  file(CHMOD "${dir}/pre-commit" PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  run_hook_case("${dir}/pre-commit" "hygiene: hook-no-staged")

elseif(CASE STREQUAL "usage")
  gbl_expect_fail(COMMAND ${HYGIENE} --bogus REGEX "hygiene: usage" EXIT 2)

else()
  message(FATAL_ERROR "hygiene-case: unknown case: ${CASE}")
endif()
