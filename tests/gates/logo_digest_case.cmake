# Configure the real private digest parser against owned metadata in unique
# build-tree sandboxes. No production authority file is copied or modified.
cmake_minimum_required(VERSION 3.25)

foreach(required IN ITEMS SRC BIN)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "logo-digest-case: missing ${required}")
  endif()
endforeach()
get_filename_component(BIN "${BIN}" ABSOLUTE)
set(parent_cache "${BIN}/CMakeCache.txt")
if(NOT EXISTS "${parent_cache}" OR IS_DIRECTORY "${parent_cache}")
  message(FATAL_ERROR "logo-digest-case: parent-cache-missing")
endif()

function(gbl__read_cache_entry cache_path key output_type output_value)
  file(STRINGS "${cache_path}" entries REGEX "^${key}:[^=]+=")
  list(LENGTH entries entry_count)
  if(NOT entry_count EQUAL 1)
    message(FATAL_ERROR "logo-digest-case: parent-cache-entry-missing: ${key}")
  endif()
  list(GET entries 0 entry)
  if(NOT entry MATCHES "^[^:]+:([^=]+)=(.*)$")
    message(FATAL_ERROR "logo-digest-case: parent-cache-entry-invalid: ${key}")
  endif()
  set(${output_type} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${output_value} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

gbl__read_cache_entry("${parent_cache}" CMAKE_GENERATOR
  parent_CMAKE_GENERATOR_type parent_CMAKE_GENERATOR)
gbl__read_cache_entry("${parent_cache}" CMAKE_MAKE_PROGRAM
  parent_CMAKE_MAKE_PROGRAM_type parent_CMAKE_MAKE_PROGRAM)

function(gbl__verify_generator cache_path case_name)
  foreach(key IN ITEMS CMAKE_GENERATOR CMAKE_MAKE_PROGRAM)
    gbl__read_cache_entry("${cache_path}" "${key}" "case_${key}_type"
      "case_${key}")
    if(NOT "${case_${key}}" STREQUAL "${parent_${key}}" OR
        NOT "${case_${key}_type}" STREQUAL "${parent_${key}_type}")
      message(FATAL_ERROR
        "logo-digest-case: ${case_name}: isolated-generator-not-inherited: ${key}")
    endif()
  endforeach()
endfunction()

string(RANDOM LENGTH 10 ALPHABET 0123456789abcdef run_id)
set(root "${BIN}/nc/logo_digest_case/${run_id}")
set(real_parser "${SRC}/cmake/ReadLogoDigest.cmake")
set(valid_digest
  "8ec8d30af0acb1e6ff15d8ea332a0e6b60ab60ab41cef968e07c7706b349d9d1")
set(pending_row "owner-pending nintendo-logo-window")
set(valid_row "${valid_digest} nintendo-logo-window")

function(gbl__write_project directory)
  file(MAKE_DIRECTORY "${directory}")
  file(WRITE "${directory}/CMakeLists.txt" [=[cmake_minimum_required(VERSION 3.25)
project(logo_digest_case NONE)
include("${PARSER}")
gbl_read_logo_digest("${AUTHORITY}" actual)
if(NOT "${actual}" STREQUAL "${EXPECTED}")
  message(FATAL_ERROR "logo-digest-case: wrong-parser-output")
endif()
if(EXPECT_REJECT)
  message(FATAL_ERROR "logo-digest-case: invalid-row-control-failed")
endif()
message(STATUS "logo-digest-case: accepted")
]=])
endfunction()

function(gbl__run_case name mode content expected forbidden)
  set(case_root "${root}/${name}")
  set(project_dir "${case_root}/project")
  set(authority "${case_root}/logo.sha256")
  file(MAKE_DIRECTORY "${case_root}")
  if(NOT mode STREQUAL "missing")
    file(WRITE "${authority}" "${content}")
  endif()
  gbl__write_project("${project_dir}")

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${project_dir}" -B "${case_root}/build"
      -G "${parent_CMAKE_GENERATOR}"
      "-DCMAKE_MAKE_PROGRAM:${parent_CMAKE_MAKE_PROGRAM_type}=${parent_CMAKE_MAKE_PROGRAM}"
      "-DPARSER=${real_parser}" "-DAUTHORITY=${authority}"
      "-DEXPECTED=${expected}" -DEXPECT_REJECT=FALSE
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  set(combined "${output}${error}")
  gbl__verify_generator("${case_root}/build/CMakeCache.txt" "${name}")

  if(mode STREQUAL "accept")
    if(NOT result EQUAL 0 OR NOT combined MATCHES "logo-digest-case: accepted")
      message(FATAL_ERROR "logo-digest-case: ${name}: accepting metadata was refused")
    endif()
  else()
    if(result EQUAL 0 OR NOT combined MATCHES "logo-digest: invalid-authority")
      message(FATAL_ERROR "logo-digest-case: ${name}: refusal reason was not stable")
    endif()
    if(NOT "${forbidden}" STREQUAL "")
      string(FIND "${combined}" "${forbidden}" leaked_at)
      if(NOT leaked_at EQUAL -1)
        message(FATAL_ERROR "logo-digest-case: ${name}: metadata echoed in diagnostics")
      endif()
    endif()
  endif()
  message("logo-digest-case: ${name} ok")
endfunction()

gbl__run_case(valid_pending accept "# synthetic comment\n\n${pending_row}\n" "" "")
gbl__run_case(valid_digest accept "# owned vector\n${valid_row}\n"
  "${valid_digest}" "")
gbl__run_case(missing missing "" "" "")
gbl__run_case(empty reject "" "" "")
gbl__run_case(duplicate reject "${pending_row}\n${pending_row}\n" ""
  "${pending_row}")
gbl__run_case(wrong_label reject "${valid_digest} other-label\n" ""
  "${valid_digest} other-label")

string(SUBSTRING "${valid_digest}" 0 63 digest_63)
string(SUBSTRING "${valid_digest}" 1 -1 digest_tail)
string(CONCAT digest_65 "${valid_digest}" "0")
string(CONCAT digest_nonhex "g" "${digest_tail}")
gbl__run_case(short reject "${digest_63} nintendo-logo-window\n" ""
  "${digest_63} nintendo-logo-window")
gbl__run_case(long reject "${digest_65} nintendo-logo-window\n" ""
  "${digest_65} nintendo-logo-window")
gbl__run_case(nonhex reject "${digest_nonhex} nintendo-logo-window\n" ""
  "${digest_nonhex} nintendo-logo-window")
gbl__run_case(extra_token reject "${valid_row} extra\n" "" "${valid_row} extra")

string(ASCII 9 tab)
gbl__run_case(tab_separator reject "${valid_digest}${tab}nintendo-logo-window\n"
  "" "${valid_digest}")
gbl__run_case(double_space reject "${valid_digest}  nintendo-logo-window\n"
  "" "${valid_digest}")
gbl__run_case(semicolon reject "${valid_digest}; nintendo-logo-window\n"
  "" "${valid_digest}")

# Meta-control: disable parsing in a disposable stub. The same invalid-row
# control must go red with its explicit marker, proving it detects a bypass.
set(disabled_root "${root}/disabled_parser")
set(disabled_parser "${disabled_root}/ReadLogoDigest-disabled.cmake")
file(MAKE_DIRECTORY "${disabled_root}")
file(WRITE "${disabled_parser}" [=[function(gbl_read_logo_digest path out_var)
  set(${out_var} "" PARENT_SCOPE)
endfunction()
]=])
set(disabled_authority "${disabled_root}/invalid.sha256")
file(WRITE "${disabled_authority}" "${valid_digest} wrong-label\n")
set(disabled_project "${disabled_root}/project")
gbl__write_project("${disabled_project}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${disabled_project}" -B "${disabled_root}/build"
    -G "${parent_CMAKE_GENERATOR}"
    "-DCMAKE_MAKE_PROGRAM:${parent_CMAKE_MAKE_PROGRAM_type}=${parent_CMAKE_MAKE_PROGRAM}"
    "-DPARSER=${disabled_parser}" "-DAUTHORITY=${disabled_authority}"
    -DEXPECTED= -DEXPECT_REJECT=TRUE
  RESULT_VARIABLE disabled_result OUTPUT_VARIABLE disabled_output
  ERROR_VARIABLE disabled_error)
set(disabled_combined "${disabled_output}${disabled_error}")
gbl__verify_generator("${disabled_root}/build/CMakeCache.txt" "disabled_parser")
if(disabled_result EQUAL 0 OR
    NOT disabled_combined MATCHES "logo-digest-case: invalid-row-control-failed")
  message(FATAL_ERROR "logo-digest-case: disabled parser did not make invalid-row control red")
endif()

message("logo-digest-case: ok (13 authority cases and disabled-parser control)")
