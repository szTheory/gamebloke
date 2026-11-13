# Consumer-side freshness check. Admission commits may follow the source-only
# authority, provided every producer input still has its exact pinned bytes/mode.
function(gbl_check_diagnostic_authority source_root commit)
  execute_process(COMMAND git -C "${source_root}" merge-base --is-ancestor
    "${commit}" HEAD RESULT_VARIABLE ancestor OUTPUT_QUIET ERROR_QUIET)
  if(NOT ancestor STREQUAL "0")
    message(FATAL_ERROR "fixture-index: diagnostic-source-not-ancestor")
  endif()
  execute_process(COMMAND git -C "${source_root}" show
    "${commit}:diagnostics/armv4t/inputs.txt" RESULT_VARIABLE list_rc
    OUTPUT_VARIABLE pinned_inputs ERROR_QUIET)
  if(NOT list_rc STREQUAL "0")
    message(FATAL_ERROR "fixture-index: diagnostic-input-list-unavailable")
  endif()
  # Force mode comparison for this command even when the repository has
  # core.filemode=false; tracked executable bits are part of source authority.
  execute_process(COMMAND git -C "${source_root}" -c core.filemode=true
    diff --quiet "${commit}" -- diagnostics/armv4t/inputs.txt
    RESULT_VARIABLE list_diff)
  if(NOT list_diff STREQUAL "0")
    message(FATAL_ERROR "fixture-index: stale-diagnostic-source-input")
  endif()
  file(STRINGS "${source_root}/diagnostics/armv4t/inputs.txt" lines)
  set(paths "")
  foreach(path IN LISTS lines)
    if(path STREQUAL "" OR path MATCHES "^#")
      continue()
    endif()
    if(NOT path MATCHES "^[A-Za-z0-9._/-]+$" OR path MATCHES "(^|/)\\.\\.?(/|$)" OR
       path MATCHES "^/" OR IS_SYMLINK "${source_root}/${path}" OR
       NOT EXISTS "${source_root}/${path}" OR IS_DIRECTORY "${source_root}/${path}")
      message(FATAL_ERROR "fixture-index: diagnostic-source-input-invalid:${path}")
    endif()
    list(APPEND paths "${path}")
  endforeach()
  if(NOT "diagnostics/armv4t/inputs.txt" IN_LIST paths)
    message(FATAL_ERROR "fixture-index: diagnostic-input-list-unbound")
  endif()
  execute_process(COMMAND git -C "${source_root}" -c core.filemode=true
    diff --quiet "${commit}" -- ${paths} RESULT_VARIABLE diff_rc)
  if(NOT diff_rc STREQUAL "0")
    message(FATAL_ERROR "fixture-index: stale-diagnostic-source-input")
  endif()
endfunction()
