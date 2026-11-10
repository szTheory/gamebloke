# Ruleset validator controls and apply-script controls (D-03, D-13, W4).
#   cmake -DCASE=<name> -DSRC=<source root> -DBIN=<build root> -P tests/ci/ruleset_cases.cmake
#
# Every case builds its JSON at test time with string(JSON) from the committed
# .github/rulesets/main.json, under <BIN>/nc/<case> (rebuilt each run). Nothing
# under tests/ is a canned ruleset or a canned GitHub response.
#
# Cases:
#   valid      the committed ruleset passes the validator
#   bypass     a bypass actor is added           -> ruleset: bypass-actors
#   extra_rule a deletion rule is added          -> ruleset: unauthorized-rule: deletion
#   context    the required context is renamed   -> ruleset: required-check
#
# Cases for scripts/apply-repo-settings.sh run it with a stub `gh` first on PATH
# (W4). The stub appends its arguments to calls.log, serves canned JSON for each
# endpoint, and on `--method POST` or `--method PUT` writes WRITE-ATTEMPT to
# calls.log and exits 1, unless the sandbox holds `allow-write` (only the
# apply-success scenarios do, to prove the write path itself).
#   usage                 --bogus                          -> repo-settings: usage (exit 2)
#   check_ok              remote = committed + server-only fields -> ruleset ok (id 42),
#                         at least four advisory lines; then --apply creates (POST) and
#                         updates (PUT) against the stub
#   check_server_defaults remote also carries a server-added parameter default -> still ok
#   ruleset_drift         remote carries an extra bypass actor     -> repo-settings: ruleset-drift
#   ruleset_missing       no ruleset named main                    -> repo-settings: ruleset-missing
#   no_ci_required_run    --apply with a failed CI required run    -> repo-settings: no-ci-required-run,
#                         and no write attempt
#
# Reason codes this file prints:
#   ruleset-case: ...
# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED CASE OR NOT DEFINED SRC OR NOT DEFINED BIN)
  message(FATAL_ERROR "ruleset-case: -DCASE=, -DSRC= and -DBIN= are required")
endif()
include("${SRC}/cmake/ExpectFail.cmake")

set(committed "${SRC}/.github/rulesets/main.json")
if(NOT EXISTS "${committed}")
  message(FATAL_ERROR "ruleset-case: missing ${committed}")
endif()
file(READ "${committed}" ruleset)

set(dir "${BIN}/nc/${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")

# Validator command for a ruleset file.
function(rs_validate_cmd out file)
  set(${out} "${CMAKE_COMMAND}" -DMODE=validate "-DFILE=${file}"
    -P "${SRC}/cmake/RulesetCheck.cmake" PARENT_SCOPE)
endfunction()

# --- stub gh helpers ----------------------------------------------------------

# Committed ruleset plus the fields the server adds on its own.
function(rs_with_server_fields out)
  set(r "${ruleset}")
  string(JSON r SET "${r}" id 42)
  string(JSON r SET "${r}" node_id "\"RRS_stub\"")
  string(JSON r SET "${r}" source_type "\"Repository\"")
  string(JSON r SET "${r}" source "\"stub/repo\"")
  string(JSON r SET "${r}" created_at "\"2026-01-01T00:00:00Z\"")
  string(JSON r SET "${r}" updated_at "\"2026-01-02T00:00:00Z\"")
  string(JSON r SET "${r}" current_user_can_bypass "\"never\"")
  string(JSON r SET "${r}" _links "{\"self\": {\"href\": \"https://example.invalid/stub\"}}")
  set(${out} "${r}" PARENT_SCOPE)
endfunction()

# Listing as `gh api --paginate --slurp` prints it: an array of pages.
function(rs_listing out present)
  if(present)
    string(JSON item SET "{}" id 42)
    string(JSON item SET "${item}" name "\"main\"")
    string(JSON page SET "[]" 0 "${item}")
  else()
    set(page "[]")
  endif()
  string(JSON list SET "[]" 0 "${page}")
  set(${out} "${list}" PARENT_SCOPE)
endfunction()

# rs_stub_dir(<dir> LIST <json> REMOTE <json> CONCLUSION <success|failure>
#             [ALLOW_WRITE AFTER_LIST <json> AFTER_REMOTE <json>])
function(rs_stub_dir dir)
  cmake_parse_arguments(PARSE_ARGV 1 S "ALLOW_WRITE" "LIST;REMOTE;CONCLUSION;AFTER_LIST;AFTER_REMOTE" "")
  file(REMOVE_RECURSE "${dir}")
  file(MAKE_DIRECTORY "${dir}/bin")
  file(WRITE "${dir}/bin/gh" [=[#!/bin/sh
d=$(dirname "$0")/..
echo "$*" >> "$d/calls.log"
case "$*" in
  *"--method POST"*|*"--method PUT"*)
    if [ -f "$d/allow-write" ]; then
      [ -f "$d/after-list.json" ] && cp "$d/after-list.json" "$d/list.json"
      [ -f "$d/after-ruleset.json" ] && cp "$d/after-ruleset.json" "$d/ruleset.json"
      echo '{}'
      exit 0
    fi
    echo WRITE-ATTEMPT >> "$d/calls.log"
    exit 1
    ;;
esac
ep=""
for a in "$@"; do
  case $a in repos/*) ep=$a ;; esac
done
case $ep in
  repos/stub/repo/rulesets) cat "$d/list.json" ;;
  repos/stub/repo/rulesets/42) cat "$d/ruleset.json" ;;
  repos/stub/repo/actions/permissions) cat "$d/perms.json" ;;
  repos/stub/repo/commits/main/check-runs*) cat "$d/checkruns.json" ;;
  repos/stub/repo) cat "$d/repo.json" ;;
  *) echo "gh-stub: unexpected: $*" >&2; exit 1 ;;
esac
]=])
  file(CHMOD "${dir}/bin/gh" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
  file(WRITE "${dir}/calls.log" "")
  file(WRITE "${dir}/list.json" "${S_LIST}")
  file(WRITE "${dir}/ruleset.json" "${S_REMOTE}")

  string(JSON run SET "{}" name "\"CI required\"")
  string(JSON run SET "${run}" conclusion "\"${S_CONCLUSION}\"")
  string(JSON runs SET "[]" 0 "${run}")
  string(JSON cr SET "{}" total_count 1)
  string(JSON cr SET "${cr}" check_runs "${runs}")
  file(WRITE "${dir}/checkruns.json" "${cr}")

  string(JSON on SET "{}" status "\"disabled\"")
  string(JSON sa SET "{}" secret_scanning "${on}")
  string(JSON sa SET "${sa}" secret_scanning_push_protection "${on}")
  string(JSON repo SET "{}" squash_merge_commit_title "\"COMMIT_OR_PR_TITLE\"")
  string(JSON repo SET "${repo}" security_and_analysis "${sa}")
  file(WRITE "${dir}/repo.json" "${repo}")
  string(JSON perms SET "{}" sha_pinning_required "false")
  file(WRITE "${dir}/perms.json" "${perms}")

  if(S_ALLOW_WRITE)
    file(WRITE "${dir}/allow-write" "")
    file(WRITE "${dir}/after-list.json" "${S_AFTER_LIST}")
    file(WRITE "${dir}/after-ruleset.json" "${S_AFTER_REMOTE}")
  endif()
endfunction()

# Command line that runs the apply script with the sandbox stub first on PATH.
function(rs_apply_cmd out dir mode)
  set(${out} "${CMAKE_COMMAND}" -E env "PATH=${dir}/bin:$ENV{PATH}"
    "CMAKE=${CMAKE_COMMAND}" sh "${SRC}/scripts/apply-repo-settings.sh" ${mode}
    --repo stub/repo PARENT_SCOPE)
endfunction()

# Fails the case when the stub saw a write it should not have.
function(rs_no_write dir)
  file(READ "${dir}/calls.log" calls)
  if("${calls}" MATCHES "WRITE-ATTEMPT")
    message(FATAL_ERROR "ruleset-case: write attempted\n${calls}")
  endif()
endfunction()

function(rs_require_call dir needle)
  file(READ "${dir}/calls.log" calls)
  string(FIND "${calls}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "ruleset-case: expected gh call not made: ${needle}\n${calls}")
  endif()
endfunction()

function(rs_run_stub_case)
  rs_with_server_fields(remote)
  rs_listing(present 1)
  rs_listing(absent 0)

  if(CASE STREQUAL "check_ok")
    set(d "${dir}/check")
    rs_stub_dir("${d}" LIST "${present}" REMOTE "${remote}" CONCLUSION success)
    rs_apply_cmd(cmd "${d}" --check)
    gbl_expect_pass(COMMAND ${cmd} REGEX "repo-settings: ruleset ok \\(id 42\\)")
    execute_process(COMMAND ${cmd} OUTPUT_VARIABLE out ERROR_VARIABLE out)
    string(REGEX MATCHALL "repo-settings: advisory:" hits "${out}")
    list(LENGTH hits n)
    if(n LESS 4)
      message(FATAL_ERROR "ruleset-case: expected at least 4 advisory lines, got ${n}\n${out}")
    endif()
    rs_no_write("${d}")

    # The write path itself: create when absent, update when present. The stub
    # swaps in the post-write state so the closing --check can pass.
    set(d "${dir}/apply_create")
    rs_stub_dir("${d}" LIST "${absent}" REMOTE "${remote}" CONCLUSION success
      ALLOW_WRITE AFTER_LIST "${present}" AFTER_REMOTE "${remote}")
    rs_apply_cmd(cmd "${d}" --apply)
    gbl_expect_pass(COMMAND ${cmd} REGEX "created ruleset main")
    rs_require_call("${d}" "--method POST repos/stub/repo/rulesets --input .github/rulesets/main.json")

    string(JSON drifted SET "${remote}" bypass_actors 0
      "{\"actor_id\": 5, \"actor_type\": \"RepositoryRole\", \"bypass_mode\": \"always\"}")
    set(d "${dir}/apply_update")
    rs_stub_dir("${d}" LIST "${present}" REMOTE "${drifted}" CONCLUSION success
      ALLOW_WRITE AFTER_LIST "${present}" AFTER_REMOTE "${remote}")
    rs_apply_cmd(cmd "${d}" --apply)
    gbl_expect_pass(COMMAND ${cmd} REGEX "updated ruleset main \\(id 42\\)")
    rs_require_call("${d}" "--method PUT repos/stub/repo/rulesets/42 --input .github/rulesets/main.json")
    file(READ "${d}/calls.log" calls)
    if("${calls}" MATCHES "--method POST")
      message(FATAL_ERROR "ruleset-case: update scenario created a second ruleset\n${calls}")
    endif()

  elseif(CASE STREQUAL "check_server_defaults")
    # W-8: a parameter default the server filled in and the committed file
    # omits is not drift.
    string(JSON withdefault SET "${remote}" rules 0 parameters allowed_merge_methods
      "[\"merge\", \"squash\", \"rebase\"]")
    set(d "${dir}/check")
    rs_stub_dir("${d}" LIST "${present}" REMOTE "${withdefault}" CONCLUSION success)
    rs_apply_cmd(cmd "${d}" --check)
    gbl_expect_pass(COMMAND ${cmd} REGEX "repo-settings: ruleset ok \\(id 42\\)")
    rs_no_write("${d}")

  elseif(CASE STREQUAL "ruleset_drift")
    string(JSON drifted SET "${remote}" bypass_actors 0
      "{\"actor_id\": 5, \"actor_type\": \"RepositoryRole\", \"bypass_mode\": \"always\"}")
    set(d "${dir}/check")
    rs_stub_dir("${d}" LIST "${present}" REMOTE "${drifted}" CONCLUSION success)
    rs_apply_cmd(cmd "${d}" --check)
    gbl_expect_fail(COMMAND ${cmd} REGEX "repo-settings: ruleset-drift" EXIT 1)
    rs_no_write("${d}")

  elseif(CASE STREQUAL "ruleset_missing")
    set(d "${dir}/check")
    rs_stub_dir("${d}" LIST "${absent}" REMOTE "${remote}" CONCLUSION success)
    rs_apply_cmd(cmd "${d}" --check)
    gbl_expect_fail(COMMAND ${cmd} REGEX "repo-settings: ruleset-missing" EXIT 1)
    rs_no_write("${d}")

  elseif(CASE STREQUAL "no_ci_required_run")
    set(d "${dir}/apply")
    rs_stub_dir("${d}" LIST "${absent}" REMOTE "${remote}" CONCLUSION failure)
    rs_apply_cmd(cmd "${d}" --apply)
    gbl_expect_fail(COMMAND ${cmd} REGEX "repo-settings: no-ci-required-run" EXIT 1)
    rs_no_write("${d}")
  endif()
endfunction()

if(CASE STREQUAL "valid")
  rs_validate_cmd(cmd "${committed}")
  gbl_expect_pass(COMMAND ${cmd} REGEX "ruleset: ok")
elseif(CASE STREQUAL "bypass" OR CASE STREQUAL "extra_rule" OR CASE STREQUAL "context")
  if(CASE STREQUAL "bypass")
    string(JSON mutated SET "${ruleset}" bypass_actors 0
      "{\"actor_id\": 5, \"actor_type\": \"RepositoryRole\", \"bypass_mode\": \"always\"}")
    set(regex "ruleset: bypass-actors")
  elseif(CASE STREQUAL "extra_rule")
    string(JSON mutated SET "${ruleset}" rules 2 "{\"type\": \"deletion\"}")
    set(regex "ruleset: unauthorized-rule: deletion")
  else()
    string(JSON mutated SET "${ruleset}" rules 1 parameters required_status_checks 0
      context "\"CI optional\"")
    set(regex "ruleset: required-check")
  endif()
  if("${mutated}" STREQUAL "${ruleset}")
    message(FATAL_ERROR "ruleset-case: mutation left the ruleset unchanged (${CASE})")
  endif()
  file(WRITE "${dir}/main.json" "${mutated}")
  rs_validate_cmd(cmd "${dir}/main.json")
  gbl_expect_fail(COMMAND ${cmd} REGEX "${regex}")
elseif(CASE STREQUAL "usage")
  gbl_expect_fail(
    COMMAND sh "${SRC}/scripts/apply-repo-settings.sh" --bogus
    REGEX "repo-settings: usage" EXIT 2)
else()
  # Every remaining case drives the apply script against a stub gh.
  set(stub_cases check_ok check_server_defaults ruleset_drift ruleset_missing
    no_ci_required_run)
  if(NOT CASE IN_LIST stub_cases)
    message(FATAL_ERROR "ruleset-case: unknown CASE '${CASE}'")
  endif()
  rs_run_stub_case()
endif()
