include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/GblJson.cmake")
# One table is shared by workflow preflight, final proof and the local collector.
function(gbl_ci_policy source)
  gbl_json_load(policy "${source}/cmake/CiJobBudgets.json")
  string(JSON format GET "${policy}" format)
  string(JSON producer GET "${policy}" producer_head)
  string(JSON digest GET "${policy}" producer_input_sha256)
  string(JSON count LENGTH "${policy}" jobs)
  string(JSON jobs_type TYPE "${policy}" jobs)
  if(NOT format STREQUAL "gamebloke.ci-job-budgets/1" OR NOT count EQUAL 11 OR
     NOT jobs_type STREQUAL "ARRAY" OR
     NOT producer STREQUAL "85c361edfd6b56640e68e64c5e32527f85408836" OR
     NOT digest STREQUAL "95d1b9c7f8c580c37a0051efb54424d40d7c50a57d2774c33512001f31331d43")
    message(FATAL_ERROR "ci-preflight: policy-authority-mismatch")
  endif()
  set(expected changes hygiene title asan "CI required"
    "build (ubuntu-26.04, gcc, GNU)" "build (macos-26, cc, AppleClang)"
    "diagnostics (linux-x64, arm)" "diagnostics (linux-x64, thumb)"
    "diagnostics (darwin-arm64, arm)" "diagnostics (darwin-arm64, thumb)")
  set(seen "")
  foreach(i RANGE 0 10)
    string(JSON name GET "${policy}" jobs ${i} name)
    string(JSON seconds GET "${policy}" jobs ${i} seconds)
    string(JSON seconds_type TYPE "${policy}" jobs ${i} seconds)
    set(approved 180)
    if(name STREQUAL "build (macos-26, cc, AppleClang)" OR
       name STREQUAL "diagnostics (darwin-arm64, arm)")
      set(approved 300)
    elseif(name STREQUAL "build (ubuntu-26.04, gcc, GNU)")
      set(approved 240)
    endif()
    if(NOT name IN_LIST expected OR name IN_LIST seen OR NOT seconds STREQUAL approved OR
       NOT seconds_type STREQUAL "NUMBER")
      message(FATAL_ERROR "ci-preflight: unauthorized-job-budget:${name}")
    endif()
    list(APPEND seen "${name}")
  endforeach()
  set(GBL_CI_POLICY "${policy}" PARENT_SCOPE)
  set(GBL_CI_REQUIRED_JOBS "${seen}" PARENT_SCOPE)
  set(GBL_CI_PRODUCER_HEAD "${producer}" PARENT_SCOPE)
  set(GBL_CI_PRODUCER_INPUT "${digest}" PARENT_SCOPE)
endfunction()

function(gbl_ci_job_budget name out)
  list(FIND GBL_CI_REQUIRED_JOBS "${name}" index)
  if(index LESS 0)
    message(FATAL_ERROR "final-proof: unknown-required-job:${name}")
  endif()
  string(JSON seconds GET "${GBL_CI_POLICY}" jobs ${index} seconds)
  set(${out} "${seconds}" PARENT_SCOPE)
endfunction()
