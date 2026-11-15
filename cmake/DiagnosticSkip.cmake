include_guard(GLOBAL)

# CMake scripts report intentional optional absence with this single output
# line. Match the ENTIRE output, so a fatal error or forwarded tool text cannot
# turn a nonzero diagnostic command into a permitted CTest skip.
function(gbl_diagnostic_skip name required)
  if(required)
    set_tests_properties("${name}" PROPERTIES SKIP_REGULAR_EXPRESSION "")
  else()
    set_tests_properties("${name}" PROPERTIES
      SKIP_REGULAR_EXPRESSION "^SKIPPED: arm-toolchain-absent[\r\n]*$")
  endif()
endfunction()
