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
  NAME runner.relocation_admitted
  LABELS diagnostics gate integration runner
  SKIPPABLE
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DMODE=admitted -DOPTIONAL=ON "-DGBL_REQUIRE_DIAGNOSTICS=${GBL_REQUIRE_DIAGNOSTICS}"
    -P "${CMAKE_SOURCE_DIR}/tests/gates/runner_relocation.cmake")
set_tests_properties(runner.relocation_admitted PROPERTIES
  FIXTURES_REQUIRED diagnostic_execution)
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
gbl_add_test(
  NAME ppu.tracer
  LABELS bus core integration ppu
  COMMAND $<TARGET_FILE:bus_test> ppu.tracer)
foreach(selector IN ITEMS ppu.boundary ppu.registers ppu.latch ppu.frame
    ppu.refusal ppu.controls)
  if(selector STREQUAL "ppu.frame")
    set(labels bus core integration ppu)
  else()
    set(labels bus core ppu unit)
  endif()
  gbl_add_test(
    NAME ${selector}
    LABELS ${labels}
    COMMAND $<TARGET_FILE:bus_test> ${selector})
endforeach()
gbl_add_test(
  NAME bus.irq.registers
  LABELS bus core unit
  COMMAND $<TARGET_FILE:bus_test> irq.registers)
gbl_add_test(
  NAME bus.keypad.registers
  LABELS bus core unit
  COMMAND $<TARGET_FILE:bus_test> keypad.registers)
foreach(selector IN ITEMS waitcnt.matrix waitcnt.boundary waitcnt.registers)
  gbl_add_test(
    NAME bus.${selector}
    LABELS bus core unit
    COMMAND $<TARGET_FILE:bus_test> ${selector})
endforeach()

add_executable(timer_test tests/timer_test.c)
target_link_libraries(timer_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME timer0.guest.tracer
  LABELS timer core integration
  COMMAND $<TARGET_FILE:timer_test> tracer)
gbl_add_test(
  NAME timer.costs
  LABELS core time timer unit
  COMMAND $<TARGET_FILE:timer_test> costs)
gbl_add_test(
  NAME timer.costs.contract
  LABELS gate time timer
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -P "${CMAKE_SOURCE_DIR}/tests/gates/phase7_costs.cmake")
gbl_add_test(
  NAME phase7.order
  LABELS gate time timer
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -P "${CMAKE_SOURCE_DIR}/tests/gates/phase7_order.cmake")
foreach(selector IN ITEMS keypad.condition keypad.refusal keypad.timeline
    time.events.order time.events.boundary time.events.refusal
    time.halt.boundary time.halt.refusal
    timer0.model.boundary timer.devices)
  gbl_add_test(
    NAME ${selector}
    LABELS core time timer unit
    COMMAND $<TARGET_FILE:timer_test> ${selector})
endforeach()
gbl_add_test(
  NAME phase7.selection.control
  LABELS gate time timer
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=control -P "${CMAKE_SOURCE_DIR}/tests/gates/phase7_selection.cmake")

gbl_add_test(
  NAME phase8.selection.control
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  LABELS gate time timer
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=control -P "${CMAKE_SOURCE_DIR}/tests/gates/phase8_selection.cmake")
gbl_add_test(
  NAME phase8.order
  LABELS gate time timer
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=order -P "${CMAKE_SOURCE_DIR}/tests/gates/phase8_selection.cmake")
gbl_add_test(
  NAME phase8.policy
  LABELS gate profile time
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -P "${CMAKE_SOURCE_DIR}/tests/gates/phase8_policy.cmake")

set(GBL_PHASE9_POLICY_GATE "${CMAKE_SOURCE_DIR}/tests/gates/phase9_policy.cmake")
gbl_add_test(
  NAME phase9.policy
  LABELS gate profile time
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=verify -P "${GBL_PHASE9_POLICY_GATE}")
gbl_add_test(
  NAME phase9.policy.control
  LABELS gate profile time
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=control -P "${GBL_PHASE9_POLICY_GATE}")

set(GBL_PHASE9_DISPUTE_GATE "${CMAKE_SOURCE_DIR}/tests/gates/phase9_dispute.cmake")
gbl_add_test(
  NAME phase9.dispute
  LABELS gate profile registry
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=verify -P "${GBL_PHASE9_DISPUTE_GATE}")
gbl_add_test(
  NAME phase9.dispute.control
  LABELS gate profile registry
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=control -P "${GBL_PHASE9_DISPUTE_GATE}")
gbl_add_test(
  NAME phase9.selection.control
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  LABELS gate time timer
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=control -P "${CMAKE_SOURCE_DIR}/tests/gates/phase9_selection.cmake")
gbl_add_test(
  NAME phase9.order
  LABELS gate time timer
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=order -P "${CMAKE_SOURCE_DIR}/tests/gates/phase9_selection.cmake")

add_executable(cpu_test tests/cpu_test.c)
target_link_libraries(cpu_test PRIVATE gamebloke_core gbl_warnings)
add_executable(dma_test tests/dma_test.c)
target_link_libraries(dma_test PRIVATE gamebloke_core gbl_warnings)
gbl_add_test(
  NAME cpu.tracer
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> tracer)
gbl_add_test(
  NAME cpu.dma.tracer
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> dma.tracer)
gbl_add_test(
  NAME cpu.dma.refusal
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> dma.refusal)
gbl_add_test(
  NAME cpu.lifecycle
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> lifecycle)
gbl_add_test(
  NAME cpu.budget
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> budget)
foreach(selector IN ITEMS arithmetic arm_alu arm_block arm_block_banks arm_block_bounds arm_block_edges arm_pc_return arm_shifts_conditions arm_transfer arm_subword arm_subword_alignment loaded_pc thumb thumb_alu thumb_bl thumb_control thumb_high thumb_multiple thumb_sp thumb_stack thumb_transfer decode alignment modes exceptions multiply_signed_timing multiply_unsigned
    no_bios peek mailbox psr trace_capacity trace slots slots_bounds isa_classification unit_boundaries irq.boundary irq.refusal power.boundary)
  gbl_add_test(
    NAME cpu.${selector}
    LABELS cpu core unit
    COMMAND $<TARGET_FILE:cpu_test> ${selector})
endforeach()
gbl_add_test(
  NAME cpu.waitcnt
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> waitcnt.cpu)
gbl_add_test(
  NAME cpu.dma.waitcnt
  LABELS cpu core unit
  COMMAND $<TARGET_FILE:cpu_test> dma.waitcnt)
foreach(selector IN ITEMS irq latch refusal dma)
  gbl_add_test(
    NAME cpu.ppu.${selector}
    LABELS core cpu ppu unit
    COMMAND $<TARGET_FILE:cpu_test> ppu.${selector})
endforeach()

# The Phase 09 DMA selectors are declared now and enter CTest when their lane
# inventory rows are added by plan 09-07. Until then they remain directly
# runnable without making earlier sequential plans fail configure-time parity.
if(GBL_INVENTORY_LANE STREQUAL "ci" OR GBL_INVENTORY_LANE STREQUAL "asan")
  file(STRINGS
    "${CMAKE_SOURCE_DIR}/tests/inventory/${GBL_INVENTORY_LANE}.txt"
    GBL_PHASE9_DMA_INVENTORY_ROWS)
  foreach(selector IN ITEMS registers arbitration events latch refusal controls)
    set(test_name "dma.${selector}")
    set(registered FALSE)
    foreach(inventory_row IN LISTS GBL_PHASE9_DMA_INVENTORY_ROWS)
      string(FIND "${inventory_row}" "${test_name} " row_position)
      if(row_position EQUAL 0)
        set(registered TRUE)
      endif()
    endforeach()
    if(registered)
      gbl_add_test(
        NAME ${test_name}
        LABELS core dma unit
        COMMAND $<TARGET_FILE:dma_test> ${selector})
    endif()
  endforeach()
  set(test_name "cpu.dma.cpu")
  set(registered FALSE)
  foreach(inventory_row IN LISTS GBL_PHASE9_DMA_INVENTORY_ROWS)
    string(FIND "${inventory_row}" "${test_name} " row_position)
    if(row_position EQUAL 0)
      set(registered TRUE)
    endif()
  endforeach()
  if(registered)
    gbl_add_test(
      NAME ${test_name}
      LABELS core cpu unit
      COMMAND $<TARGET_FILE:cpu_test> dma.cpu)
  endif()
endif()
foreach(selector IN ITEMS triggers order refusal)
  gbl_add_test(
    NAME dma.ppu.${selector}
    LABELS core dma ppu unit
    COMMAND $<TARGET_FILE:dma_test> ppu.${selector})
endforeach()

# These runner checks use only synthetic-control fixtures. Give each case its
# own scratch path because CTest may run them concurrently.
foreach(selector IN ITEMS observation output output-controls)
  gbl_add_test(
    NAME runner.ppu.${selector}
    LABELS integration ppu runner
    COMMAND $<TARGET_FILE:runner_test> ppu.${selector}
      "${CMAKE_BINARY_DIR}/runner-ppu-${selector}")
endforeach()

# The Phase 10 profile gate checks runtime observations without running a guest.
set(GBL_PHASE10_POLICY_GATE
  "${CMAKE_SOURCE_DIR}/tests/gates/phase10_policy.cmake")
gbl_add_test(
  NAME phase10.policy
  LABELS gate profile ppu time
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=verify -P "${GBL_PHASE10_POLICY_GATE}")
gbl_add_test(
  NAME phase10.policy.control
  LABELS control profile ppu time
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=control -P "${GBL_PHASE10_POLICY_GATE}")

# Selection registrations land with Task 1 so inventory checks cover the exact
# Phase 10 gate contract while Task 2 supplies the verifier implementation.
foreach(case IN ITEMS control verify order)
  if(case STREQUAL "control")
    set(test_name phase10.selection.control)
  else()
    set(test_name phase10.selection.${case})
  endif()
  gbl_add_test(
    NAME ${test_name}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    LABELS gate ppu time
    COMMAND "${CMAKE_COMMAND}"
      "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      "-DCASE=${case}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/phase10_selection.cmake")
endforeach()

# Phase 11 selection checks are registered in both core lanes. Their exact
# setup/required fixture graph is added at the top level after this include.
foreach(case IN ITEMS control verify order)
  if(case STREQUAL "control")
    set(test_name phase11.selection.control)
  else()
    set(test_name phase11.selection.${case})
  endif()
  gbl_add_test(
    NAME ${test_name}
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    LABELS gate ppu time
    COMMAND "${CMAKE_COMMAND}"
      "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      "-DCASE=${case}"
      -P "${CMAKE_SOURCE_DIR}/tests/gates/phase11_selection.cmake")
endforeach()
set_tests_properties(cpu.power.boundary time.halt.boundary time.halt.refusal
  PROPERTIES TIMEOUT 10)

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
add_custom_target(gbl_mutation_objects ALL
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -DCASE=prepare -DSELECTOR=all
    "-DCOMPILER=${CMAKE_C_COMPILER}" "-DCOMPILER_ID=${CMAKE_C_COMPILER_ID}"
    "-DC_FLAGS=${CMAKE_C_FLAGS}" "-DC_FLAGS_CONFIG=${GBL_MUTATION_C_FLAGS_CONFIG}"
    "-DLINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
    "-DLINKER_FLAGS_CONFIG=${GBL_MUTATION_LINK_FLAGS_CONFIG}"
    "-DVERSION=${PROJECT_VERSION}"
    -P "${CMAKE_SOURCE_DIR}/tests/gates/core_mutation.cmake"
  VERBATIM)
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
      "-DPREPARED_ROOT=${CMAKE_BINARY_DIR}/mutation-objects"
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
      "-DPREPARED_ROOT=${CMAKE_BINARY_DIR}/mutation-objects"
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
      "-DPREPARED_ROOT=${CMAKE_BINARY_DIR}/mutation-objects"
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
gbl__add_cpu_mutation_test(cpu.nc.block_effect_order block_effect_order arm_block_banks)
gbl__add_cpu_mutation_test(cpu.nc.loaded_pc loaded_pc loaded_pc)
gbl__add_cpu_mutation_test(cpu.nc.maximum_trace maximum_trace arm_block_bounds)
gbl__add_cpu_mutation_test(cpu.nc.multiply_flags multiply_flags multiply_unsigned)
gbl__add_cpu_mutation_test(cpu.nc.bl_halves bl_halves thumb_bl)
gbl__add_cpu_mutation_test(cpu.nc.late_refusal late_refusal unit_boundaries)

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
  NAME runner.input.cli
  LABELS runner integration gate
  COMMAND "${CMAKE_COMMAND}"
    "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
    -P "${CMAKE_SOURCE_DIR}/tests/gates/phase8_cli.cmake")

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
