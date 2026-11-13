cmake_minimum_required(VERSION 3.25)
foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "synthetic-diagnostics: missing-${required}")
  endif()
endforeach()

# Test-only authority: copy current regular whitelisted bytes, then commit only
# that disposable snapshot. Never relax freshness checks in the real parser.
file(REAL_PATH "${SRC}" source)
get_filename_component(output "${BIN}" ABSOLUTE BASE_DIR "${source}")
set(build "${source}/build")
cmake_path(IS_PREFIX build "${output}" NORMALIZE safe)
set(existing "${output}")
while(NOT EXISTS "${existing}" AND NOT IS_SYMLINK "${existing}")
  get_filename_component(existing "${existing}" DIRECTORY)
endwhile()
file(REAL_PATH "${existing}" resolved_output)
if(NOT safe OR output STREQUAL build OR NOT resolved_output STREQUAL existing OR
   NOT IS_DIRECTORY "${existing}" OR IS_SYMLINK "${build}" OR IS_SYMLINK "${output}")
  message(FATAL_ERROR "synthetic-diagnostics: unsafe-output-root")
endif()

set(isolated_env "${CMAKE_COMMAND}" -E env
  --unset=GIT_DIR --unset=GIT_WORK_TREE --unset=GIT_INDEX_FILE
  --unset=GIT_COMMON_DIR --unset=GIT_OBJECT_DIRECTORY
  --unset=GIT_ALTERNATE_OBJECT_DIRECTORIES --unset=GIT_CONFIG_COUNT
  --unset=GIT_CONFIG_PARAMETERS --unset=GIT_CONFIG
  --unset=GIT_AUTHOR_NAME --unset=GIT_AUTHOR_EMAIL
  --unset=GIT_COMMITTER_NAME --unset=GIT_COMMITTER_EMAIL
  GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1)
execute_process(COMMAND ${isolated_env} git -C "${source}" show HEAD:diagnostics/armv4t/inputs.txt
  RESULT_VARIABLE rc OUTPUT_VARIABLE committed_inputs ERROR_VARIABLE error)
if(NOT rc STREQUAL "0")
  message(FATAL_ERROR "synthetic-diagnostics: input-whitelist-unavailable:${error}")
endif()
string(REPLACE ";" "\\;" committed_inputs "${committed_inputs}")
string(REPLACE "\n" ";" lines "${committed_inputs}")
set(inputs "")
foreach(path IN LISTS lines)
  if(path STREQUAL "" OR path MATCHES "^#")
    continue()
  endif()
  if(NOT path MATCHES "^[A-Za-z0-9][A-Za-z0-9._/-]*$" OR
     path MATCHES "(^|/)[.][.]?(/|$)" OR path IN_LIST inputs)
    message(FATAL_ERROR "synthetic-diagnostics: invalid-input-whitelist")
  endif()
  list(APPEND inputs "${path}")
endforeach()
set(paths ${inputs}
  tests/gates/toolchain_spike.cmake tests/gates/final_diagnostic_proof.cmake)
list(REMOVE_DUPLICATES paths)
foreach(path IN LISTS paths)
  get_filename_component(expected "${source}/${path}" ABSOLUTE)
  file(REAL_PATH "${expected}" resolved)
  execute_process(COMMAND test -f "${expected}" RESULT_VARIABLE regular)
  execute_process(COMMAND ${isolated_env} git -C "${source}" ls-files --error-unmatch -- "${path}"
    RESULT_VARIABLE tracked OUTPUT_QUIET ERROR_QUIET)
  if(NOT regular STREQUAL "0" OR NOT tracked STREQUAL "0" OR
     IS_SYMLINK "${expected}" OR NOT resolved STREQUAL expected)
    message(FATAL_ERROR "synthetic-diagnostics: input-not-regular-tracked:${path}")
  endif()
endforeach()
file(STRINGS "${source}/diagnostics/armv4t/inputs.txt" current_inputs)
set(seen_inputs "")
foreach(path IN LISTS current_inputs)
  if(path STREQUAL "" OR path MATCHES "^#")
    continue()
  endif()
  if(NOT path IN_LIST inputs OR path IN_LIST seen_inputs)
    message(FATAL_ERROR "synthetic-diagnostics: input-outside-whitelist:${path}")
  endif()
  list(APPEND seen_inputs "${path}")
endforeach()
list(SORT seen_inputs)
set(sorted_inputs "${inputs}")
list(SORT sorted_inputs)
if(NOT seen_inputs STREQUAL sorted_inputs)
  message(FATAL_ERROR "synthetic-diagnostics: incomplete-input-whitelist")
endif()

file(MAKE_DIRECTORY "${output}")
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef nonce)
set(snapshot "${output}/synthetic-authority-${nonce}")
if(EXISTS "${snapshot}" OR IS_SYMLINK "${snapshot}")
  message(FATAL_ERROR "synthetic-diagnostics: snapshot-already-exists")
endif()
file(MAKE_DIRECTORY "${snapshot}")
foreach(path IN LISTS paths)
  get_filename_component(parent "${snapshot}/${path}" DIRECTORY)
  file(MAKE_DIRECTORY "${parent}")
  file(COPY_FILE "${source}/${path}" "${snapshot}/${path}")
  file(SHA256 "${source}/${path}" original_sha)
  file(SHA256 "${snapshot}/${path}" copied_sha)
  if(NOT original_sha STREQUAL copied_sha)
    message(FATAL_ERROR "synthetic-diagnostics: snapshot-copy-mismatch:${path}")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DFILE=${snapshot}/diagnostics/armv4t/cases.json"
  -P "${snapshot}/cmake/GblJson.cmake"
  RESULT_VARIABLE rc OUTPUT_VARIABLE json_out ERROR_VARIABLE json_error)
if(NOT rc STREQUAL "0")
  message(FATAL_ERROR "synthetic-diagnostics: invalid-matrix:${json_out}${json_error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -DMODE=draft
  "-DMATRIX=${snapshot}/diagnostics/armv4t/cases.json"
  -P "${snapshot}/cmake/CheckArmCoverage.cmake"
  RESULT_VARIABLE rc OUTPUT_VARIABLE coverage_out ERROR_VARIABLE coverage_error)
if(NOT rc STREQUAL "0")
  message(FATAL_ERROR "synthetic-diagnostics: invalid-matrix:${coverage_out}${coverage_error}")
endif()
function(snapshot_git)
  # Ignore ambient Git configuration/hooks only for this test-only repository.
  execute_process(COMMAND ${isolated_env} git -C "${snapshot}" -c init.defaultBranch=synthetic-controls
    -c user.name=SyntheticControls -c user.email=controls@example.com
    ${ARGV} RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "synthetic-diagnostics: snapshot-git-failed:${error}")
  endif()
endfunction()
snapshot_git(init --quiet --template=)
snapshot_git(add -- ${paths})
snapshot_git(commit --quiet -m "test-only current synthetic parser authority")
file(WRITE "${output}/latest-snapshot.txt" "${snapshot}\n")
message(STATUS "synthetic-diagnostics: current working bytes isolated in a test-only committed snapshot; no hosted/build/admission authority")
execute_process(COMMAND ${isolated_env} "${CMAKE_COMMAND}" -DCASE=final-proof-controls
  "-DSRC=${snapshot}" -DBIN=build/controls
  -P "${snapshot}/tests/gates/toolchain_spike.cmake"
  RESULT_VARIABLE rc OUTPUT_VARIABLE controls_out ERROR_VARIABLE controls_error)
file(WRITE "${output}/controls.log" "${controls_out}${controls_error}")
if(NOT rc STREQUAL "0")
  message(FATAL_ERROR "synthetic-diagnostics: controls-failed:${controls_out}${controls_error}")
endif()
message("${controls_out}")
