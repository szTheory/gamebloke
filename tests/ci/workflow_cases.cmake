# Mutation controls for the workflow gates (D-03, D-10, D-11).
#   cmake -DCASE=<name> -DSRC=<source root> -DBIN=<build root> -P tests/ci/workflow_cases.cmake
#
# Every case reads the REAL .github/workflows/ci.yml, applies one
# first-occurrence replacement, writes the result to <BIN>/nc/<case>/ci.yml,
# and runs the case's gate on that copy through gbl_expect_fail with the
# reason it must give. No fixture workflow is committed, so a control cannot
# drift from the workflow it guards. An anchor that no longer occurs in
# ci.yml stops the case with `mutate: no-match`, so a stale control cannot
# silently test an unmutated file.
#
# Cases:
#   pins:     pins_unpinned pins_no_version_comment      (scripts/check-action-pins.sh)
#   workflow: clean (must pass) and the policy mutations (scripts/check-workflow.sh)
#   wiring:   registered_no_file_arg: tests/ci_gates.cmake registers both workflow gates
#             without a file argument (a named file would narrow them to one workflow)
#   sandbox:  extra_file extra_file_pins no_ci_yml all_files_clean: a sandbox repository
#             root (copies of both gate scripts and the presets) holding more than one
#             workflow file, run with NO file argument as the registered gates run
#             (WR-01: the gates must cover every .github/workflows file)
#
# Reason codes this file prints:
#   mutate: no-match
#   workflow-cases: ...
# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED CASE OR NOT DEFINED SRC OR NOT DEFINED BIN)
  message(FATAL_ERROR "workflow-cases: -DCASE=, -DSRC= and -DBIN= are required")
endif()
include("${SRC}/cmake/ExpectFail.cmake")

set(workflow "${SRC}/.github/workflows/ci.yml")
if(NOT EXISTS "${workflow}")
  message(FATAL_ERROR "workflow-cases: missing ${workflow}")
endif()
file(READ "${workflow}" text)

# First-occurrence replacement. Sets <out> in the caller's scope.
function(gbl__mutate out text anchor replacement)
  string(FIND "${text}" "${anchor}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "mutate: no-match: anchor not found in ci.yml: ${anchor}")
  endif()
  string(LENGTH "${anchor}" alen)
  string(SUBSTRING "${text}" 0 ${pos} head)
  math(EXPR tail_start "${pos} + ${alen}")
  string(SUBSTRING "${text}" ${tail_start} -1 tail)
  set(${out} "${head}${replacement}${tail}" PARENT_SCOPE)
endfunction()

# Per case: gate (pins|workflow), anchor, repl, regex. The clean case has no anchor.
set(gate "workflow")
set(sandbox OFF)
set(anchor "")
set(repl "")
set(regex "")

if(CASE STREQUAL "pins_unpinned")
  set(gate "pins")
  set(anchor "@3d3c42e5aac5ba805825da76410c181273ba90b1")
  set(repl "@v7")
  set(regex "pins: unpinned")
elseif(CASE STREQUAL "pins_no_version_comment")
  set(gate "pins")
  set(anchor " # v7.0.1")
  set(repl "")
  set(regex "pins: no-version-comment")
elseif(CASE STREQUAL "clean")
  set(gate "workflow")
elseif(CASE STREQUAL "moving_label")
  # The moving runner label is built from two fragments so this file never
  # carries the string the policy gate looks for.
  set(moving_a "ubuntu-")
  set(moving_b "latest")
  set(anchor "ubuntu-26.04")
  set(repl "${moving_a}${moving_b}")
  set(regex "workflow: moving-label")
elseif(CASE STREQUAL "path_filter")
  set(anchor "    branches: [main]")
  set(repl "${anchor}\n    paths: [\"src/**\"]")
  set(regex "workflow: path-filter")
elseif(CASE STREQUAL "pr_target")
  set(anchor "  pull_request:")
  set(repl "  pull_request_target:")
  set(regex "workflow: pr-target")
elseif(CASE STREQUAL "needs_missing")
  set(anchor "needs: [changes, hygiene, title, build, asan, diagnostics]")
  set(repl "needs: [changes, hygiene, title, build, asan]")
  set(regex "workflow: needs-missing: diagnostics")
elseif(CASE STREQUAL "expression_in_run")
  set(anchor "run: cmake --workflow --preset hygiene")
  set(repl [=[run: cmake --workflow --preset hygiene && echo "${{ github.head_ref }}"]=])
  set(regex "workflow: expression-in-run")
elseif(CASE STREQUAL "non_preset_run")
  set(anchor "cmake --workflow --preset ci")
  set(repl "cmake --preset ci && cmake --build --preset ci")
  set(regex "workflow: non-preset-run: build")
elseif(CASE STREQUAL "unknown_preset")
  set(anchor "--preset asan")
  set(repl "--preset nosuchlane")
  set(regex "workflow: unknown-preset: nosuchlane")
elseif(CASE STREQUAL "no_timeout")
  # Renaming the key removes the setting from the first job without depending
  # on the text after it.
  set(anchor "timeout-minutes:")
  set(repl "removed-timeout:")
  set(regex "workflow: no-timeout: changes")
elseif(CASE STREQUAL "persist_credentials")
  set(anchor "persist-credentials: false")
  set(repl "persist-credentials: true")
  set(regex "workflow: persist-credentials")
elseif(CASE STREQUAL "permissions")
  set(anchor "permissions:\n  contents: read")
  set(repl "permissions:\n  contents: write")
  set(regex "workflow: permissions")
elseif(CASE STREQUAL "required_check")
  set(anchor "    if: always()")
  set(repl "    if: success()")
  set(regex "workflow: required-check")
elseif(CASE STREQUAL "continue_on_error")
  set(anchor [=[    runs-on: ${{ matrix.runner }}]=])
  set(repl "${anchor}\n    continue-on-error: true")
  set(regex "workflow: continue-on-error")
elseif(CASE STREQUAL "job_permissions")
  # Job-level permissions override the top-level default (WR-02).
  set(anchor "  hygiene:\n")
  set(repl "${anchor}    permissions:\n      contents: write\n      id-token: write\n")
  set(regex "workflow: job-permissions: hygiene")
elseif(CASE STREQUAL "job_permissions_inline")
  set(anchor "  hygiene:\n")
  set(repl "${anchor}    permissions: write-all\n")
  set(regex "workflow: job-permissions: hygiene")
elseif(CASE STREQUAL "run_by_path")
  # A tool called by path is still a tool outside the preset-only rule (WR-03).
  set(anchor "run: cmake --workflow --preset hygiene")
  set(repl "run: /usr/bin/cmake --workflow --preset hygiene")
  set(regex "workflow: non-preset-run: hygiene")
elseif(CASE STREQUAL "run_script")
  # Not a tool name at all: the allowlist refuses any other run line (WR-03).
  set(anchor "run: cmake --workflow --preset hygiene")
  set(repl "run: ./tools/go.sh")
  set(regex "workflow: non-preset-run: hygiene")
elseif(CASE STREQUAL "extra_file" OR CASE STREQUAL "extra_file_pins"
    OR CASE STREQUAL "no_ci_yml" OR CASE STREQUAL "all_files_clean")
  set(sandbox ON)
elseif(CASE STREQUAL "registered_no_file_arg")
  # Reads the registration, not the workflow: the two gates must run over the
  # default file set, so a new workflow file is checked without a test edit.
  file(READ "${SRC}/tests/ci_gates.cmake" gates_text)
  foreach(script IN ITEMS check-workflow check-action-pins)
    string(REGEX MATCHALL "COMMAND sh scripts/${script}\\.sh[^\n]*" regs "${gates_text}")
    list(LENGTH regs nregs)
    if(NOT nregs EQUAL 1)
      message(FATAL_ERROR "workflow-cases: file-argument: ${script} is registered ${nregs} times, want 1")
    endif()
    if(NOT "${regs}" STREQUAL "COMMAND sh scripts/${script}.sh")
      message(FATAL_ERROR "workflow-cases: file-argument: ${script} is registered with an argument: ${regs}")
    endif()
  endforeach()
  message("workflow-cases: registered_no_file_arg ok")
  return()
else()
  message(FATAL_ERROR "workflow-cases: unknown CASE '${CASE}'")
endif()

set(dir "${BIN}/nc/${CASE}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")

if(sandbox)
  # A repository root of its own: the gate scripts locate .github/workflows and
  # CMakePresets.json from their own path, so copies here see only these files.
  set(root "${dir}/root")
  file(MAKE_DIRECTORY "${root}/scripts" "${root}/.github/workflows")
  file(COPY_FILE "${SRC}/scripts/check-workflow.sh" "${root}/scripts/check-workflow.sh")
  file(COPY_FILE "${SRC}/scripts/check-action-pins.sh" "${root}/scripts/check-action-pins.sh")
  file(COPY_FILE "${SRC}/CMakePresets.json" "${root}/CMakePresets.json")
  set(sha "3d3c42e5aac5ba805825da76410c181273ba90b1")
  set(extra_head "name: Extra\non:\n  pull_request:\n")
  set(extra_tail "permissions:\n  contents: read\njobs:\n  x:\n    runs-on: ubuntu-26.04\n    timeout-minutes: 3\n    steps:\n")
  if(NOT CASE STREQUAL "no_ci_yml")
    file(WRITE "${root}/.github/workflows/ci.yml" "${text}")
  endif()
  if(CASE STREQUAL "extra_file" OR CASE STREQUAL "extra_file_pins")
    # Trigger, unpinned action, and credentials all wrong in a file that is not ci.yml.
    file(WRITE "${root}/.github/workflows/extra.yml"
      "name: Extra\non:\n  pull_request_target:\n${extra_tail}      - uses: actions/checkout@v4\n")
  else()
    file(WRITE "${root}/.github/workflows/extra.yml"
      "${extra_head}${extra_tail}      - uses: actions/checkout@${sha} # v7.0.1\n        with:\n          persist-credentials: false\n")
  endif()
  set(wf sh "${root}/scripts/check-workflow.sh")
  set(pins sh "${root}/scripts/check-action-pins.sh")
  if(CASE STREQUAL "extra_file")
    gbl_expect_fail(COMMAND ${wf} REGEX "workflow: pr-target: [0-9]+ \\[extra\\.yml\\]")
  elseif(CASE STREQUAL "extra_file_pins")
    gbl_expect_fail(COMMAND ${pins} REGEX "pins: unpinned: [^\n]*extra\\.yml")
  elseif(CASE STREQUAL "no_ci_yml")
    gbl_expect_fail(COMMAND ${wf} REGEX "workflow: missing-ci-yml")
  else()
    gbl_expect_pass(COMMAND ${wf} REGEX "workflow: ok \\(2 files")
    gbl_expect_pass(COMMAND ${pins} REGEX "pins: ok")
  endif()
  return()
endif()

set(copy "${dir}/ci.yml")

if(CASE STREQUAL "clean")
  file(WRITE "${copy}" "${text}")
else()
  gbl__mutate(mutated "${text}" "${anchor}" "${repl}")
  if("${mutated}" STREQUAL "${text}")
    message(FATAL_ERROR "mutate: no-match: replacement left ci.yml unchanged (${CASE})")
  endif()
  file(WRITE "${copy}" "${mutated}")
endif()

if(gate STREQUAL "pins")
  set(gate_cmd sh "${SRC}/scripts/check-action-pins.sh" "${copy}")
else()
  set(gate_cmd sh "${SRC}/scripts/check-workflow.sh" "${copy}")
endif()

if(CASE STREQUAL "clean")
  gbl_expect_pass(COMMAND ${gate_cmd} REGEX "workflow: ok \\(" WORKING_DIRECTORY "${SRC}")
else()
  gbl_expect_fail(COMMAND ${gate_cmd} REGEX "${regex}" WORKING_DIRECTORY "${SRC}")
endif()
