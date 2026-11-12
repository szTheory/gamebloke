# Core and runner tests. Included from the top-level CMakeLists.txt (never
# add_subdirectory(): see Pitfall 4 note there).
# --version and --help document exit status 0, so these require BOTH the exit
# status and the text (EXPECT=pass). PASS_REGULAR_EXPRESSION alone would make
# CTest ignore the exit status.
add_executable(hash_test tests/hash_test.c runner/sha256.c)
target_include_directories(hash_test PRIVATE runner)
target_link_libraries(hash_test PRIVATE gbl_warnings)
gbl_add_test(
  NAME runner.hash
  LABELS runner unit
  COMMAND $<TARGET_FILE:hash_test>)

gbl_add_test(
  NAME runner.version
  LABELS runner unit
  COMMAND "${CMAKE_COMMAND}" -DEXPECT=pass "-DCMD=$<TARGET_FILE:gamebloke-run>"
    -DARGS=--version "-DREGEX=^gamebloke-run 0\\.0\\.0"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")

gbl_add_test(
  NAME runner.tracer
  LABELS runner integration
  COMMAND $<TARGET_FILE:gamebloke-run-control>
    --rom "${GBL_FIXTURE_DIR}/owned-arm-pass.gba"
    --fixture synthetic-control-pass --cycles 1000)
gbl_add_test(
  NAME runner.unit
  LABELS runner unit
  COMMAND $<TARGET_FILE:runner_test>)
gbl_add_test(
  NAME runner.result_contract
  LABELS runner integration gate
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DVALIDATE_PRODUCER=ON
    -P "${CMAKE_SOURCE_DIR}/tests/gates/runner_case.cmake")
gbl_add_test(
  NAME runner.relocation
  LABELS gate integration runner
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DMODE=clean
    -P "${CMAKE_SOURCE_DIR}/tests/gates/runner_relocation.cmake")
gbl_add_test(
  NAME runner.relocation_control
  LABELS control runner
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DMODE=missing-executable
    -P "${CMAKE_SOURCE_DIR}/tests/gates/runner_relocation.cmake")

add_executable(version_test tests/version_test.c)
target_link_libraries(version_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME core.version
  LABELS core unit
  COMMAND $<TARGET_FILE:version_test>)

add_executable(bus_test tests/bus_test.c)
target_link_libraries(bus_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME bus.decode
  LABELS bus core unit
  COMMAND $<TARGET_FILE:bus_test> decode)
gbl_add_test(
  NAME bus.timing
  LABELS bus core unit
  COMMAND $<TARGET_FILE:bus_test> timing)
gbl_add_test(
  NAME bus.video
  LABELS bus core unit
  COMMAND $<TARGET_FILE:bus_test> video)

add_executable(cpu_test tests/cpu_test.c)
target_link_libraries(cpu_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME cpu.tracer
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> tracer)
gbl_add_test(
  NAME cpu.lifecycle
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> lifecycle)
gbl_add_test(
  NAME cpu.budget
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> budget)
foreach(selector IN ITEMS arithmetic arm_alu arm_block arm_block_banks arm_block_bounds arm_pc_return arm_shifts_conditions arm_transfer arm_subword arm_subword_alignment loaded_pc thumb decode alignment modes exceptions multiply_signed_timing multiply_unsigned
    no_bios peek mailbox psr trace_capacity trace slots slots_bounds)
  gbl_add_test(
    NAME cpu.${selector}
    LABELS cpu core unit
    COMMAND $<TARGET_FILE:cpu_test> ${selector})
endforeach()

# Compile selected disposable bus mutants against the real bus test. Pass the
# configured compiler, build flags, warning policy, and sanitizer flags so the
# asan lane instruments these controls without repeating a project subbuild.
string(TOUPPER "${CMAKE_BUILD_TYPE}" GBL_MUTATION_BUILD_CONFIG)
set(GBL_MUTATION_C_FLAGS_CONFIG_VAR "CMAKE_C_FLAGS_${GBL_MUTATION_BUILD_CONFIG}")
set(GBL_MUTATION_C_FLAGS_CONFIG "${${GBL_MUTATION_C_FLAGS_CONFIG_VAR}}")
set(GBL_MUTATION_LINK_FLAGS_CONFIG_VAR
  "CMAKE_EXE_LINKER_FLAGS_${GBL_MUTATION_BUILD_CONFIG}")
set(GBL_MUTATION_LINK_FLAGS_CONFIG
  "${${GBL_MUTATION_LINK_FLAGS_CONFIG_VAR}}")
function(gbl__add_bus_mutation_test test_name mutation selector)
  if(mutation STREQUAL "clean")
    set(labels bus positive)
  else()
    set(labels bus control)
  endif()
  gbl_add_test(
    NAME "${test_name}"
    LABELS ${labels}
    COMMAND "${CMAKE_COMMAND}"
      "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      "-DCASE=${mutation}" "-DSELECTOR=${selector}"
      "-DCOMPILER=${CMAKE_C_COMPILER}"
      "-DCOMPILER_ID=${CMAKE_C_COMPILER_ID}"
      "-DC_FLAGS=${CMAKE_C_FLAGS}"
      "-DC_FLAGS_CONFIG=${GBL_MUTATION_C_FLAGS_CONFIG}"
      "-DLINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
      "-DLINKER_FLAGS_CONFIG=${GBL_MUTATION_LINK_FLAGS_CONFIG}"
      "-DVERSION=${PROJECT_VERSION}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/core_mutation.cmake")
endfunction()
gbl__add_bus_mutation_test(bus.nc.ewram_mirror ewram_mirror decode)
gbl__add_bus_mutation_test(bus.nc.iwram_mirror iwram_mirror decode)
gbl__add_bus_mutation_test(bus.nc.vram_mirror vram_mirror decode)
gbl__add_bus_mutation_test(bus.nc.video_bytes video_bytes video)
gbl__add_bus_mutation_test(bus.nc.clock_overflow clock_overflow timing)
gbl__add_bus_mutation_test(bus.pos.clean clean video)

function(gbl__add_loader_mutation_test test_name mutation selector)
  if(mutation STREQUAL "clean")
    set(labels core loader positive)
  else()
    set(labels control core loader)
  endif()
  gbl_add_test(
    NAME "${test_name}"
    LABELS ${labels}
    COMMAND "${CMAKE_COMMAND}"
      "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      "-DCASE=loader_${mutation}" "-DSELECTOR=${selector}"
      "-DCOMPILER=${CMAKE_C_COMPILER}"
      "-DCOMPILER_ID=${CMAKE_C_COMPILER_ID}"
      "-DC_FLAGS=${CMAKE_C_FLAGS}"
      "-DC_FLAGS_CONFIG=${GBL_MUTATION_C_FLAGS_CONFIG}"
      "-DLINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
      "-DLINKER_FLAGS_CONFIG=${GBL_MUTATION_LINK_FLAGS_CONFIG}"
      "-DVERSION=${PROJECT_VERSION}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/core_mutation.cmake")
endfunction()

gbl__add_loader_mutation_test(
  loader.nc.allocate_before_validate allocate_before_validate lengths)
gbl__add_loader_mutation_test(loader.nc.refusal_state refusal_state diagnostics)
gbl__add_loader_mutation_test(
  loader.nc.refusal_rom_identity refusal_rom_identity transaction)
gbl__add_loader_mutation_test(
  loader.nc.refusal_allocator_identity refusal_allocator_identity transaction)
gbl__add_loader_mutation_test(loader.nc.allocation_state allocation_state diagnostics)
gbl__add_loader_mutation_test(loader.nc.caller_bytes caller_bytes diagnostics)
gbl__add_loader_mutation_test(loader.nc.logo_claim logo_claim diagnostics)
gbl__add_loader_mutation_test(
  loader.nc.logo_digest_bypass logo_digest_bypass logo-digest)
gbl__add_loader_mutation_test(
  loader.nc.logo_digest_forced_match logo_digest_forced_match logo-digest)
gbl__add_loader_mutation_test(loader.pos.clean clean diagnostics)

function(gbl__add_cpu_mutation_test test_name mutation selector)
  if(mutation STREQUAL "clean")
    set(labels core cpu positive)
  else()
    set(labels control core cpu)
  endif()
  gbl_add_test(
    NAME "${test_name}"
    LABELS ${labels}
    COMMAND "${CMAKE_COMMAND}"
      "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      "-DCASE=cpu_${mutation}" "-DSELECTOR=${selector}"
      "-DCOMPILER=${CMAKE_C_COMPILER}"
      "-DCOMPILER_ID=${CMAKE_C_COMPILER_ID}"
      "-DC_FLAGS=${CMAKE_C_FLAGS}"
      "-DC_FLAGS_CONFIG=${GBL_MUTATION_C_FLAGS_CONFIG}"
      "-DLINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
      "-DLINKER_FLAGS_CONFIG=${GBL_MUTATION_LINK_FLAGS_CONFIG}"
      "-DVERSION=${PROJECT_VERSION}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/core_mutation.cmake")
endfunction()

gbl__add_cpu_mutation_test(cpu.nc.pc_refill pc_refill decode)
gbl__add_cpu_mutation_test(cpu.nc.shift_carry shift_carry arithmetic)
gbl__add_cpu_mutation_test(cpu.nc.bank_alias bank_alias modes)
gbl__add_cpu_mutation_test(cpu.nc.exception_lr exception_lr exceptions)
gbl__add_cpu_mutation_test(cpu.nc.vector_fetch vector_fetch no_bios)
gbl__add_cpu_mutation_test(cpu.nc.peek_effect peek_effect peek)
gbl__add_cpu_mutation_test(cpu.nc.trace_loss trace_loss trace)
gbl__add_cpu_mutation_test(
  cpu.nc.budget_overshoot budget_overshoot trace)
gbl__add_cpu_mutation_test(cpu.pos.clean clean all)

gbl_add_test(
  NAME machine.state
  LABELS core machine unit
  COMMAND $<TARGET_FILE:bus_test> state)

add_executable(loader_test tests/loader_test.c)
target_link_libraries(loader_test PRIVATE gamebloke_core gbl_warnings)
target_compile_definitions(loader_test PRIVATE
  "GBL_TEST_LOGO_EXPECTATION_MODE=${GBL_PRODUCTION_LOGO_TEST_MODE}")
add_executable(loader_logo_test tests/loader_test.c)
target_link_libraries(loader_logo_test PRIVATE
  gamebloke_core_logo_synthetic gbl_warnings)
target_compile_definitions(loader_logo_test PRIVATE
  GBL_TEST_LOGO_EXPECTATION_MODE=1)
gbl_add_test(
  NAME loader.lengths
  LABELS core loader unit
  COMMAND $<TARGET_FILE:loader_test> lengths)
gbl_add_test(
  NAME loader.transaction
  LABELS core loader unit
  COMMAND $<TARGET_FILE:loader_test> transaction)
gbl_add_test(
  NAME loader.diagnostics
  LABELS core loader unit
  COMMAND $<TARGET_FILE:loader_test> diagnostics)
gbl_add_test(
  NAME loader.integration
  LABELS core integration loader unit
  COMMAND $<TARGET_FILE:loader_test> integration)
gbl_add_test(
  NAME loader.logo_digest
  LABELS core integration loader unit
  COMMAND $<TARGET_FILE:loader_logo_test> logo-digest)
gbl_add_test(
  NAME logo_digest.authority_cases
  LABELS control loader
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -P "${CMAKE_SOURCE_DIR}/tests/gates/logo_digest_case.cmake")

gbl_add_test(
  NAME runner.help
  LABELS runner unit
  COMMAND "${CMAKE_COMMAND}" -DEXPECT=pass "-DCMD=$<TARGET_FILE:gamebloke-run>"
    -DARGS=--help "-DREGEX=exit codes: 0 success, 2 usage error"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")

gbl_add_test(
  NAME runner.metadata
  LABELS runner gate
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    "-DHASH_TEST=$<TARGET_FILE:hash_test>"
    "-DPARENT_C_STANDARD=${CMAKE_C_STANDARD}"
    "-DPARENT_C_STANDARD_REQUIRED=${CMAKE_C_STANDARD_REQUIRED}"
    "-DPARENT_C_EXTENSIONS=${CMAKE_C_EXTENSIONS}"
    -P "${CMAKE_SOURCE_DIR}/tests/gates/runner_metadata.cmake")

# The documented usage-error exit code is exactly 2 (unlike a sanitizer abort).
gbl_add_test(
  NAME runner.usage_error
  LABELS runner unit
  COMMAND "${CMAKE_COMMAND}" "-DCMD=$<TARGET_FILE:gamebloke-run>" -DARGS=--bogus
    -DEXIT=2 "-DREGEX=usage: gamebloke-run"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")
gbl_add_test(
  NAME runner.no_args
  LABELS runner unit
  COMMAND "${CMAKE_COMMAND}" "-DCMD=$<TARGET_FILE:gamebloke-run>"
    -DEXIT=2 "-DREGEX=usage: gamebloke-run"
    -P "${CMAKE_SOURCE_DIR}/cmake/ExpectFail.cmake")

# The core uses no host includes (D-05), with a control that plants one. Further
# controls plant each bypass shape (a quoted system header, a line continuation,
# a comment between the directive and the header, a computed include) and the
# two ways to pass by scanning nothing (an empty and a missing directory); the
# positive companion is an in-tree quoted include plus freestanding headers.
gbl_add_test(
  NAME core.no_host_includes
  LABELS core gate
  COMMAND "${CMAKE_COMMAND}"
    "-DDIRS=${CMAKE_SOURCE_DIR}/src|${CMAKE_SOURCE_DIR}/include"
    -P "${CMAKE_SOURCE_DIR}/cmake/CheckCoreIncludes.cmake")
gbl_add_test(
  NAME core.nc.host_include
  LABELS control core
  COMMAND "${CMAKE_COMMAND}" "-DPLANT=${CMAKE_BINARY_DIR}/nc/host_include"
    -P "${CMAKE_SOURCE_DIR}/cmake/CheckCoreIncludes.cmake")
foreach(shape IN ITEMS quoted continuation comment computed empty missing)
  gbl_add_test(
    NAME core.nc.${shape}
    LABELS control core
    COMMAND "${CMAKE_COMMAND}" "-DPLANT=${CMAKE_BINARY_DIR}/nc/core_${shape}" "-DSHAPE=${shape}"
      -P "${CMAKE_SOURCE_DIR}/cmake/CheckCoreIncludes.cmake")
endforeach()
gbl_add_test(
  NAME core.pos.in_tree
  LABELS core positive
  COMMAND "${CMAKE_COMMAND}" "-DPLANT=${CMAKE_BINARY_DIR}/nc/core_in_tree" -DSHAPE=in_tree
    -P "${CMAKE_SOURCE_DIR}/cmake/CheckCoreIncludes.cmake")
