cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "phase8-policy: missing-${required}")
  endif()
endforeach()
get_filename_component(GBL_PHASE8_DEFAULT_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
get_filename_component(GBL_PHASE8_SRC "${SRC}" REALPATH
  BASE_DIR "${GBL_PHASE8_DEFAULT_ROOT}")
get_filename_component(GBL_PHASE8_BIN "${BIN}" REALPATH
  BASE_DIR "${GBL_PHASE8_SRC}")
if(NOT IS_DIRECTORY "${GBL_PHASE8_SRC}" OR NOT IS_DIRECTORY "${GBL_PHASE8_BIN}")
  message(FATAL_ERROR "phase8-policy: source-or-build-directory-missing")
endif()
set(GBL_PHASE8_CHECKER "${GBL_PHASE8_SRC}/cmake/CheckProfile.cmake")
set(GBL_PHASE8_POLICY_GATE "${CMAKE_CURRENT_LIST_FILE}")

if(CASE STREQUAL "prepare-red")
  set(scratch_root "${GBL_PHASE8_BIN}/phase8-policy-red")
  set(source_dir "${scratch_root}/source")
  set(build_dir "${scratch_root}/build")
  file(MAKE_DIRECTORY "${source_dir}")
  file(WRITE "${source_dir}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.25)\nproject(phase8_policy_red NONE)\nenable_testing()\nadd_test(NAME phase8.policy.missing_policy COMMAND \"${CMAKE_COMMAND}\" -DSRC=${GBL_PHASE8_SRC} -DBIN=${GBL_PHASE8_BIN} -DCASE=missing-policy -P \"${GBL_PHASE8_POLICY_GATE}\")\n")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${source_dir}" -B "${build_dir}"
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
  if(NOT "${configure_result}" STREQUAL "0")
    message(FATAL_ERROR
      "phase8-policy: red-project-configure\n${configure_output}${configure_error}")
  endif()
  message(STATUS "phase8-policy: scratch RED selector prepared")

elseif(CASE STREQUAL "missing-policy")
  set(work_dir "${GBL_PHASE8_BIN}/phase8-policy-controls/missing-policy")
  file(REMOVE_RECURSE "${work_dir}")
  file(MAKE_DIRECTORY "${work_dir}")
  file(COPY_FILE "${GBL_PHASE8_SRC}/profiles/agb.json"
    "${work_dir}/profile.json")
  file(READ "${work_dir}/profile.json" profile_json)
  string(JSON policy_type ERROR_VARIABLE policy_error TYPE
    "${profile_json}" irq_input_power_policy)
  if("${policy_error}" STREQUAL "NOTFOUND")
    string(JSON profile_json REMOVE "${profile_json}" irq_input_power_policy)
    file(WRITE "${work_dir}/profile.json" "${profile_json}\n")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DPROFILE=${work_dir}/profile.json"
      "-DDOC=${GBL_PHASE8_SRC}/docs/profile-agb.md"
      "-DREGISTRY=${GBL_PHASE8_SRC}/registry/disputes.json"
      "-DVOCAB=${GBL_PHASE8_SRC}/contracts/evidence.json"
      -P "${GBL_PHASE8_CHECKER}"
    RESULT_VARIABLE check_result OUTPUT_VARIABLE check_output ERROR_VARIABLE check_error)
  set(check_combined "${check_output}${check_error}")
  if("${check_result}" STREQUAL "0")
    message(FATAL_ERROR "phase8-policy: missing-policy-accepted")
  endif()
  if(NOT check_combined MATCHES "profile: irq-policy-missing: irq_input_power_policy")
    message(FATAL_ERROR
      "phase8-policy: missing-policy-wrong-reason\n${check_combined}")
  endif()
  message(STATUS "phase8-policy: missing policy table rejected")

else()
  message(FATAL_ERROR "phase8-policy: implementation-pending")
endif()
