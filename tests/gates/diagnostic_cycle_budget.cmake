# Keep admitted guest execution budgets in one place so the producer and
# receipt verifier enforce the same exact per-fixture bound.
function(gbl_diagnostic_cycle_budget out_budget fixture_id)
  set(cycle_budget_2000_fixtures
    thumb-1800200037 thumb-556470422 thumb-631794229 thumb-718382599
    thumb-736225106)
  set(cycle_budget_3000_fixtures thumb-1800200027 thumb-1800200028)
  set(cycle_budget 1000)
  if("${fixture_id}" IN_LIST cycle_budget_2000_fixtures)
    set(cycle_budget 2000)
  elseif("${fixture_id}" IN_LIST cycle_budget_3000_fixtures)
    set(cycle_budget 3000)
  endif()
  set(${out_budget} "${cycle_budget}" PARENT_SCOPE)
endfunction()
