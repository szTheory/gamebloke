cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SRC OR "${SRC}" STREQUAL "")
  message(FATAL_ERROR "toolchain-case missing-source-root")
endif()
if(NOT DEFINED CASE OR "${CASE}" STREQUAL "")
  message(FATAL_ERROR "toolchain-case missing-case")
endif()

set(lock "${SRC}/toolchain/arm-none-eabi.lock")
if(NOT EXISTS "${lock}")
  message(FATAL_ERROR "toolchain-lock missing")
endif()
file(STRINGS "${lock}" lock_lines)

function(require_line expected)
  if(NOT "${expected}" IN_LIST lock_lines)
    message(FATAL_ERROR "toolchain-lock missing exact row: ${expected}")
  endif()
endfunction()

function(resolve_native_host system_name machine_name success_name host_name reason_name)
  set(success FALSE)
  set(host "")
  set(reason "")
  if(system_name STREQUAL "Darwin" AND
     machine_name MATCHES "^(arm64|aarch64)$")
    set(success TRUE)
    set(host darwin-arm64)
  elseif(system_name STREQUAL "Linux" AND
     machine_name MATCHES "^(x86_64|amd64)$")
    set(success TRUE)
    set(host linux-x64)
  elseif(system_name STREQUAL "Linux" AND
     machine_name MATCHES "^(aarch64|arm64)$")
    set(success TRUE)
    set(host linux-arm64)
  else()
    set(reason unsupported-native-host)
  endif()
  set(${success_name} "${success}" PARENT_SCOPE)
  set(${host_name} "${host}" PARENT_SCOPE)
  set(${reason_name} "${reason}" PARENT_SCOPE)
endfunction()

function(detect_native_host uname_command success_name host_name system_name
    machine_name reason_name)
  set(success FALSE)
  set(host "")
  set(system "")
  set(machine "")
  set(reason "")

  execute_process(COMMAND "${uname_command}" -s
    RESULT_VARIABLE system_rc OUTPUT_VARIABLE system_output
    ERROR_VARIABLE system_error OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT "${system_rc}" STREQUAL "0")
    set(reason uname-system-command-failed)
  else()
    string(STRIP "${system_output}" system)
    if(system STREQUAL "")
      set(reason uname-system-output-empty)
    else()
      execute_process(COMMAND "${uname_command}" -m
        RESULT_VARIABLE machine_rc OUTPUT_VARIABLE machine_output
        ERROR_VARIABLE machine_error OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(NOT "${machine_rc}" STREQUAL "0")
        set(reason uname-machine-command-failed)
      else()
        string(STRIP "${machine_output}" machine)
        if(machine STREQUAL "")
          set(reason uname-machine-output-empty)
        else()
          resolve_native_host("${system}" "${machine}" success host reason)
        endif()
      endif()
    endif()
  endif()

  set(${success_name} "${success}" PARENT_SCOPE)
  set(${host_name} "${host}" PARENT_SCOPE)
  set(${system_name} "${system}" PARENT_SCOPE)
  set(${machine_name} "${machine}" PARENT_SCOPE)
  set(${reason_name} "${reason}" PARENT_SCOPE)
endfunction()

function(assert_host_detection_failure label uname_command expected_reason)
  detect_native_host("${uname_command}" detected_ok detected_host
    detected_system detected_machine detected_reason)
  if(detected_ok OR NOT detected_reason STREQUAL expected_reason)
    message(FATAL_ERROR
      "toolchain-case host-detection-${label}-control-failed: ok=${detected_ok} host=${detected_host} reason=${detected_reason}")
  endif()
  message(STATUS "toolchain-case host-detection-${label} PASSED (${detected_reason})")
endfunction()

function(run_host_detection_controls bin_root)
  # Reproduce the empty script-mode value observed on the supported hosts;
  # detection must still come from the uname subprocess.
  set(CMAKE_HOST_SYSTEM_PROCESSOR "")
  detect_native_host(uname detected_ok detected_host detected_system
    detected_machine detected_reason)
  if(NOT detected_ok OR NOT detected_reason STREQUAL "")
    message(FATAL_ERROR
      "toolchain-case script-mode-host-detection-failed: ${detected_reason}")
  endif()
  message(STATUS
    "toolchain-case script-mode host=${detected_host} uname=${detected_system}/${detected_machine} CMAKE_HOST_SYSTEM_PROCESSOR='${CMAKE_HOST_SYSTEM_PROCESSOR}'")

  resolve_native_host(Darwin arm64 darwin_ok darwin_host darwin_reason)
  if(NOT darwin_ok OR NOT darwin_host STREQUAL "darwin-arm64")
    message(FATAL_ERROR "toolchain-case darwin-arm64-host-map-failed")
  endif()
  resolve_native_host(Darwin x86_64 unsupported_ok unsupported_host
    unsupported_reason)
  if(unsupported_ok OR NOT unsupported_reason STREQUAL "unsupported-native-host")
    message(FATAL_ERROR "toolchain-case unsupported-darwin-host-control-failed")
  endif()
  message(STATUS "toolchain-case darwin architecture mapping PASSED")

  set(control_root "${bin_root}/host-detection-controls")
  file(MAKE_DIRECTORY "${control_root}")
  set(unsupported_uname "${control_root}/uname-unsupported")
  file(WRITE "${unsupported_uname}" [=[#!/bin/sh
set -eu
case "$1" in
  -s) printf 'Linux\n' ;;
  -m) printf 'riscv64\n' ;;
  *) exit 2 ;;
esac
]=])
  set(failed_uname "${control_root}/uname-failed")
  file(WRITE "${failed_uname}" [=[#!/bin/sh
set -eu
case "$1" in
  -s) printf 'Linux\n' ;;
  -m) exit 7 ;;
  *) exit 2 ;;
esac
]=])
  set(empty_uname "${control_root}/uname-empty")
  file(WRITE "${empty_uname}" [=[#!/bin/sh
set -eu
case "$1" in
  -s) printf 'Linux\n' ;;
  -m) printf '   \n' ;;
  *) exit 2 ;;
esac
]=])
  foreach(fake_uname IN ITEMS "${unsupported_uname}" "${failed_uname}"
      "${empty_uname}")
    file(CHMOD "${fake_uname}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
  endforeach()
  assert_host_detection_failure(unsupported "${unsupported_uname}"
    unsupported-native-host)
  assert_host_detection_failure(failed "${failed_uname}"
    uname-machine-command-failed)
  assert_host_detection_failure(empty "${empty_uname}"
    uname-machine-output-empty)
endfunction()

function(run_offline_suite)
  set(bin_root "${BIN}")
  cmake_path(ABSOLUTE_PATH bin_root BASE_DIRECTORY "${SRC}" NORMALIZE)
  set(tool_dir "${bin_root}/offline-tools")
  file(MAKE_DIRECTORY "${tool_dir}")
  detect_native_host(uname host_ok native_host host_system host_machine
    host_error)
  if(NOT host_ok)
    message(FATAL_ERROR "toolchain-case ${host_error}")
  endif()
  message(STATUS
    "toolchain-case native-host=${native_host} uname=${host_system}/${host_machine} CMAKE_HOST_SYSTEM_PROCESSOR='${CMAKE_HOST_SYSTEM_PROCESSOR}'")
  set(root_name "xpack-arm-none-eabi-gcc-15.2.1-1.1")
  set(curl "${tool_dir}/fake-curl.sh")
  set(curl_text [=[
#!/bin/sh
set -eu
out= url=
while [ "$#" -gt 0 ]; do case "$1" in --output) out=$2; shift 2;; --url) url=$2; shift 2;; *) shift;; esac; done
case "$url" in
 https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/*)
  printf 'stable\n' >> "$TOOLCHAIN_TEST_CURL_LOG"
  case "$TOOLCHAIN_FAKE_MODE" in
   unexpected-status) printf '503\n\n';;
   wrong-redirect-host) printf '302\nhttps://evil.invalid/file?fake-signed-token=x\n';;
   redirect-downgrade) printf '302\nhttp://release-assets.githubusercontent.com/file?fake-signed-token=x\n';;
   *) printf '302\nhttps://release-assets.githubusercontent.com/file?fake-signed-token=x\n';;
  esac
  : > "$out";;
 *)
  printf 'redirect\n' >> "$TOOLCHAIN_TEST_CURL_LOG"
  case "$TOOLCHAIN_FAKE_MODE" in
   redirect-count) printf '302';;
   redirected-status) printf '503';;
   empty-payload) : > "$out"; printf '200';;
   short-payload) cp "$TOOLCHAIN_TEST_SHORT" "$out"; printf '200';;
   long-payload) cp "$TOOLCHAIN_TEST_LONG" "$out"; printf '200';;
   *) cp "$TOOLCHAIN_TEST_ARCHIVE" "$out"; printf '200';;
  esac;;
esac
]=])
  file(WRITE "${curl}" "${curl_text}")
  file(CHMOD "${curl}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

  set(tar "${tool_dir}/fake-tar.sh")
  set(tar_text [=[
#!/bin/sh
set -eu
printf '%s\n' "$*" >> "$TOOLCHAIN_TEST_TAR_LOG"
case "$1" in
 -tzf)
  case "$TOOLCHAIN_FAKE_TAR_CASE" in
   corrupt-archive) exit 1;;
   absolute-member) printf '/tmp/gbl-escape\n';;
   parent-member) printf '%s\n' '@@ROOT@@/../../outside';;
   top-level-member) printf 'different-root/file\n';;
   escaping-symlink) printf '%s\n' '@@ROOT@@/' '@@ROOT@@/bin/escape';;
   unsafe-symlink-target|malformed-symlink) printf '%s\n' '@@ROOT@@/' '@@ROOT@@/bin/escape';;
   duplicate-member) printf '%s\n' '@@ROOT@@/' '@@ROOT@@/bin/arm-none-eabi-as' '@@ROOT@@/bin/arm-none-eabi-as';;
   unsafe-hardlink-target|hardlink-target-missing|hardlink-target-directory)
    printf '%s\n' '@@ROOT@@/' '@@ROOT@@/notices/' '@@ROOT@@/NOTICE-copy';;
   special-node) printf '%s\n' '@@ROOT@@/' '@@ROOT@@/dev/null';;
   malformed-listing) printf '%s\n' '@@ROOT@@/' '@@ROOT@@/bin/arm-none-eabi-as';;
   *) printf '%s\n' '@@ROOT@@/' '@@ROOT@@/bin/' '@@ROOT@@/bin/arm-none-eabi-as' \
     '@@ROOT@@/bin/arm-none-eabi-readelf' '@@ROOT@@/bin/arm-none-eabi-gcc' \
     '@@ROOT@@/notices/' '@@ROOT@@/notices/LICENSE' '@@ROOT@@/NOTICE-copy' '@@ROOT@@/bin/as-link';;
  esac;;
 -tvzf)
  case "$TOOLCHAIN_FAKE_TAR_CASE" in
   absolute-member) printf '%s\n' '-rw-r--r-- 0/0 1 Jan 1 00:00 /tmp/gbl-escape';;
   parent-member) printf '%s\n' '-rw-r--r-- 0/0 1 Jan 1 00:00 @@ROOT@@/../../outside';;
   top-level-member) printf '%s\n' '-rw-r--r-- 0/0 1 Jan 1 00:00 different-root/file';;
   escaping-symlink) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/' \
     'lrwxrwxrwx 0/0 0 Jan 1 00:00 @@ROOT@@/bin/escape -> ../../../../outside';;
   unsafe-symlink-target) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/' \
     'lrwxrwxrwx 0/0 0 Jan 1 00:00 @@ROOT@@/bin/escape -> unsafe target';;
   malformed-symlink) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/' \
     'lrwxrwxrwx 0/0 0 Jan 1 00:00 @@ROOT@@/bin/escape';;
   duplicate-member) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/' \
     '-rwxr-xr-x 0/0 1 Jan 1 00:00 @@ROOT@@/bin/arm-none-eabi-as' \
     '-rwxr-xr-x 0/0 1 Jan 1 00:00 @@ROOT@@/bin/arm-none-eabi-as';;
   unsafe-hardlink-target) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/' \
     'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/notices/' \
     'hrw-r--r-- 0/0 1 Jan 1 00:00 @@ROOT@@/NOTICE-copy link to ../../outside';;
   hardlink-target-missing) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/' \
     'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/notices/' \
     'hrw-r--r-- 0/0 1 Jan 1 00:00 @@ROOT@@/NOTICE-copy link to @@ROOT@@/notices/missing';;
   hardlink-target-directory) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/' \
     'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/notices/' \
     'hrw-r--r-- 0/0 1 Jan 1 00:00 @@ROOT@@/NOTICE-copy link to @@ROOT@@/notices';;
   special-node) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/' \
     'crw------- 0/0 0 Jan 1 00:00 @@ROOT@@/dev/null';;
   malformed-listing) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/';;
   *) printf '%s\n' 'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/' \
     'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/bin/' \
     '-rwxr-xr-x 0/0 1 Jan 1 00:00 @@ROOT@@/bin/arm-none-eabi-as' \
     '-rwxr-xr-x 0/0 1 Jan 1 00:00 @@ROOT@@/bin/arm-none-eabi-readelf' \
     '-rwxr-xr-x 0/0 1 Jan 1 00:00 @@ROOT@@/bin/arm-none-eabi-gcc' \
     'drwxr-xr-x 0/0 0 Jan 1 00:00 @@ROOT@@/notices/' \
     '-rw-r--r-- 0/0 1 Jan 1 00:00 @@ROOT@@/notices/LICENSE' \
     'hrw-r--r-- 0/0 1 Jan 1 00:00 @@ROOT@@/NOTICE-copy link to @@ROOT@@/notices/LICENSE' \
     'lrwxrwxrwx 0/0 0 Jan 1 00:00 @@ROOT@@/bin/as-link -> arm-none-eabi-as';;
  esac;;
 --no-same-owner)
  dest=
  while [ "$#" -gt 0 ]; do if [ "$1" = '-C' ]; then dest=$2; shift 2; else shift; fi; done
  case "$TOOLCHAIN_FAKE_TAR_CASE" in absolute-member|parent-member|top-level-member|escaping-symlink|special-node|malformed-listing) : > "$TOOLCHAIN_TEST_OUTSIDE";; esac
  p="$dest/@@ROOT@@"; mkdir -p "$p/bin" "$p/notices"
  cp "$TOOLCHAIN_TEST_AS" "$p/bin/arm-none-eabi-as"
  cp "$TOOLCHAIN_TEST_READELF" "$p/bin/arm-none-eabi-readelf"
  cp "$TOOLCHAIN_TEST_GCC" "$p/bin/arm-none-eabi-gcc"
  printf 'notice\n' > "$p/notices/LICENSE"
  ln "$p/notices/LICENSE" "$p/NOTICE-copy"
  ln -s arm-none-eabi-as "$p/bin/as-link";;
 *) exit 2;;
esac
]=])
  string(REPLACE "@@ROOT@@" "${root_name}" tar_text "${tar_text}")
  file(WRITE "${tar}" "${tar_text}")
  file(CHMOD "${tar}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
  foreach(tool arm-none-eabi-as arm-none-eabi-readelf arm-none-eabi-gcc)
    if(tool STREQUAL "arm-none-eabi-as")
      set(v "GNU assembler test")
    elseif(tool STREQUAL "arm-none-eabi-readelf")
      set(v "GNU readelf test")
    else()
      set(v "GNU Arm Embedded GCC test 15.2.1")
    endif()
    file(WRITE "${tool_dir}/${tool}" "#!/bin/sh\nprintf '%s\\\\n' '${v}'\n")
    file(CHMOD "${tool_dir}/${tool}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
  endforeach()

  set(cases wrong-release wrong-host wrong-host-row wrong-redirect-host redirect-downgrade
    redirect-count unexpected-status redirected-status empty-payload short-payload long-payload
    digest-mismatch corrupt-archive absolute-member parent-member top-level-member escaping-symlink
    unsafe-symlink-target malformed-symlink duplicate-member unsafe-hardlink-target
    hardlink-target-missing hardlink-target-directory special-node malformed-listing
    prior-install valid-redirect)
  if("${CASE}" STREQUAL "offline-all")
    foreach(s IN LISTS cases)
      run_offline_case("${s}")
    endforeach()
  else()
    list(FIND cases "${CASE}" idx)
    if(idx EQUAL -1)
      message(FATAL_ERROR "toolchain-case unsupported-offline-case: ${CASE}")
    endif()
    run_offline_case("${CASE}")
  endif()
endfunction()

function(make_case_lock path payload wrong_hash wrong_release)
  file(SIZE "${payload}" n)
  file(SHA256 "${payload}" hash)
  if(NOT "${wrong_hash}" STREQUAL "")
    set(hash "${wrong_hash}")
  endif()
  file(STRINGS "${lock}" rows)
  set(contents "")
  foreach(row IN LISTS rows)
    if(row MATCHES "^${native_host} ")
      string(REGEX REPLACE "^[^ ]+ [0-9]+ [0-9a-f]+" "${native_host} ${n} ${hash}" row "${row}")
    elseif(wrong_release AND row MATCHES "^release=")
      set(row "release=15.2.1-9.9")
    endif()
    string(APPEND contents "${row}\n")
  endforeach()
  file(WRITE "${path}" "${contents}")
endfunction()

function(run_offline_case s)
  set(d "${bin_root}/offline/${s}")
  file(REMOVE_RECURSE "${d}")
  file(MAKE_DIRECTORY "${d}")
  set(payload "${d}/payload")
  set(short "${d}/short")
  set(long "${d}/long")
  file(WRITE "${payload}" "synthetic xPack archive payload")
  file(WRITE "${short}" "short")
  file(WRITE "${long}" "synthetic xPack archive payload plus bytes")
  set(case_lock "${d}/lock")
  set(badhash "")
  set(badrelease false)
  if("${s}" STREQUAL "digest-mismatch")
    set(badhash "0000000000000000000000000000000000000000000000000000000000000000")
  elseif("${s}" STREQUAL "wrong-release")
    set(badrelease true)
  endif()
  make_case_lock("${case_lock}" "${payload}" "${badhash}" "${badrelease}")
  if("${s}" STREQUAL "wrong-host-row")
    file(READ "${case_lock}" text)
    if("${native_host}" STREQUAL "linux-x64")
      set(other linux-arm64)
    else()
      set(other linux-x64)
    endif()
    string(REPLACE "${native_host}.tar.gz" "${other}.tar.gz" text "${text}")
    file(WRITE "${case_lock}" "${text}")
  endif()

  set(root "${d}/root")
  set(curl_log "${d}/curl.log")
  set(tar_log "${d}/tar.log")
  set(outside "${bin_root}/offline/${s}-outside")
  file(REMOVE "${outside}")
  set(sent "${payload}")
  set(args)
  if("${s}" STREQUAL "short-payload")
    set(sent "${short}")
  elseif("${s}" STREQUAL "long-payload")
    set(sent "${long}")
  elseif("${s}" STREQUAL "wrong-host")
    if("${native_host}" STREQUAL "linux-x64")
      set(other darwin-arm64)
    else()
      set(other linux-x64)
    endif()
    list(APPEND args --host "${other}")
  elseif("${s}" STREQUAL "prior-install")
    set(old "${root}/arm-none-eabi/${native_host}-15.2.1-1.1")
    file(MAKE_DIRECTORY "${old}")
    file(WRITE "${old}/keep" "preserve\n")
  endif()
  set(envs "CURL=${curl}" "TAR=${tar}" "TOOLCHAIN_FAKE_MODE=${s}" "TOOLCHAIN_FAKE_TAR_CASE=${s}"
    "TOOLCHAIN_TEST_ARCHIVE=${sent}" "TOOLCHAIN_TEST_SHORT=${short}" "TOOLCHAIN_TEST_LONG=${long}"
    "TOOLCHAIN_TEST_CURL_LOG=${curl_log}" "TOOLCHAIN_TEST_TAR_LOG=${tar_log}"
    "TOOLCHAIN_TEST_OUTSIDE=${outside}" "TOOLCHAIN_TEST_AS=${tool_dir}/arm-none-eabi-as"
    "TOOLCHAIN_TEST_READELF=${tool_dir}/arm-none-eabi-readelf" "TOOLCHAIN_TEST_GCC=${tool_dir}/arm-none-eabi-gcc")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${envs} "${SRC}/scripts/fetch-toolchain.sh"
    --lock "${case_lock}" --root "${root}" ${args}
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)

  if("${s}" STREQUAL "valid-redirect")
    if(NOT "${result}" STREQUAL "0")
      message(FATAL_ERROR "valid redirect failed: ${out}${err}")
    endif()
    set(install "${root}/arm-none-eabi/${native_host}-15.2.1-1.1")
    file(READ "${install}/install.json" receipt)
    string(FIND "${receipt}" "fake-signed-token" leak)
    if(NOT leak EQUAL -1)
      message(FATAL_ERROR "signed URL present in receipt")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${envs} "${SRC}/scripts/fetch-toolchain.sh"
      --lock "${case_lock}" --root "${root}" --check RESULT_VARIABLE check
      OUTPUT_VARIABLE check_out ERROR_VARIABLE check_err)
    if(NOT "${check}" STREQUAL "0" OR NOT check_out MATCHES "toolchain-check PASSED")
      message(FATAL_ERROR "valid redirect check failed: ${check_out}${check_err}")
    endif()
    message(STATUS "toolchain-case valid-redirect PASSED with safe internal links")
    return()
  endif()

  if("${s}" STREQUAL "wrong-release")
    set(reason lock-release)
  elseif("${s}" STREQUAL "wrong-host")
    set(reason foreign-host)
  elseif("${s}" STREQUAL "wrong-host-row")
    set(reason lock-url)
  elseif("${s}" STREQUAL "wrong-redirect-host" OR "${s}" STREQUAL "redirect-downgrade")
    set(reason redirect-host-or-scheme)
  elseif("${s}" STREQUAL "unexpected-status")
    set(reason redirect-status)
  elseif("${s}" STREQUAL "redirect-count" OR "${s}" STREQUAL "redirected-status")
    set(reason redirected-status)
  elseif("${s}" MATCHES "^(empty|short|long)-payload$")
    set(reason size-mismatch)
  elseif("${s}" STREQUAL "digest-mismatch")
    set(reason digest-mismatch)
  elseif("${s}" STREQUAL "corrupt-archive")
    set(reason corrupt-archive)
  elseif("${s}" MATCHES "^(absolute|parent|top-level)-member$")
    set(reason unsafe-member-path)
  elseif("${s}" STREQUAL "escaping-symlink")
    set(reason escaping-symlink)
  elseif("${s}" STREQUAL "unsafe-symlink-target")
    set(reason unsafe-symlink-target)
  elseif("${s}" STREQUAL "malformed-symlink")
    set(reason malformed-symlink)
  elseif("${s}" STREQUAL "duplicate-member")
    set(reason duplicate-member)
  elseif("${s}" STREQUAL "unsafe-hardlink-target")
    set(reason unsafe-hardlink-target)
  elseif("${s}" MATCHES "^hardlink-target-(missing|directory)$")
    set(reason hardlink-target-missing-or-not-file)
  elseif("${s}" STREQUAL "special-node")
    set(reason unsafe-member-type)
  elseif("${s}" STREQUAL "malformed-listing")
    set(reason malformed-archive-listing)
  elseif("${s}" STREQUAL "prior-install")
    set(reason install-exists-use-check)
  endif()
  set(combined "${out}${err}")
  if("${result}" STREQUAL "0" OR NOT combined MATCHES "toolchain: ${reason}")
    message(FATAL_ERROR "${s} expected ${reason}, got ${result}: ${combined}")
  endif()
  string(FIND "${combined}" "fake-signed-token" leak)
  if(NOT leak EQUAL -1)
    message(FATAL_ERROR "signed URL query was logged")
  endif()
  if("${s}" STREQUAL "prior-install")
    file(READ "${old}/keep" keep)
    if(NOT keep STREQUAL "preserve\n")
      message(FATAL_ERROR "prior installation changed")
    endif()
  else()
    set(install "${root}/arm-none-eabi/${native_host}-15.2.1-1.1")
    if(EXISTS "${install}")
      message(FATAL_ERROR "refused install was left behind")
    endif()
  endif()
  file(GLOB leftovers LIST_DIRECTORIES TRUE "${root}/.arm-none-eabi-${native_host}.*")
  if(leftovers OR EXISTS "${outside}")
    message(FATAL_ERROR "temporary or out-of-root effects remain")
  endif()
  if("${s}" MATCHES "^(wrong-release|wrong-host|wrong-host-row|unexpected-status|empty-payload|short-payload|long-payload|digest-mismatch|prior-install)$")
    if(EXISTS "${tar_log}")
      message(FATAL_ERROR "tar invoked before request identity passed")
    endif()
  elseif(EXISTS "${tar_log}")
    file(READ "${tar_log}" calls)
    string(FIND "${calls}" "--no-same-owner" extraction)
    if(NOT extraction EQUAL -1)
      message(FATAL_ERROR "extraction ran before archive validation")
    endif()
  endif()
  if("${s}" MATCHES "^(wrong-release|wrong-host|wrong-host-row|prior-install)$" AND EXISTS "${curl_log}")
    message(FATAL_ERROR "curl invoked before refusal")
  endif()
  message(STATUS "toolchain-case ${s} PASSED")
endfunction()


if(CASE STREQUAL "lock")
  require_line("format=gamebloke.arm-none-eabi/1")
  require_line("release=15.2.1-1.1")
  require_line("darwin-arm64 293820200 574082d35e49a2bcbdc355836b2a3ae5e5bb3b9456c9f5e37177db2ab4aad870 https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-darwin-arm64.tar.gz release-assets.githubusercontent.com xpack-arm-none-eabi-gcc-15.2.1-1.1")
  require_line("linux-x64 307090703 da6a49ad4003944b823c6c93702a8787c922ab34bd7e918ec0eaf6933a9b1ff6 https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-linux-x64.tar.gz release-assets.githubusercontent.com xpack-arm-none-eabi-gcc-15.2.1-1.1")
  require_line("linux-arm64 302624714 67980c7990eba7bb7ffdf39699102effd70889f5ac427be19a8c8a6c5fab2972 https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-linux-arm64.tar.gz release-assets.githubusercontent.com xpack-arm-none-eabi-gcc-15.2.1-1.1")
  message(STATUS "toolchain-case lock PASSED")
elseif(CASE STREQUAL "host-detection-controls")
  if(NOT DEFINED BIN OR "${BIN}" STREQUAL "")
    message(FATAL_ERROR "toolchain-case missing-bin-root")
  endif()
  run_host_detection_controls("${BIN}")
  message(STATUS "toolchain-case host-detection-controls PASSED")
elseif(CASE STREQUAL "lock-host-mismatch")
  if(NOT DEFINED BIN OR "${BIN}" STREQUAL "")
    message(FATAL_ERROR "toolchain-case missing-bin-root")
  endif()
  set(fetcher "${SRC}/scripts/fetch-toolchain.sh")
  detect_native_host(uname host_ok native_host host_system host_machine
    host_error)
  if(NOT host_ok)
    message(FATAL_ERROR "toolchain-case ${host_error}")
  endif()
  if(native_host STREQUAL "darwin-arm64")
    set(other_host linux-x64)
  elseif(native_host STREQUAL "linux-x64")
    set(other_host darwin-arm64)
  else()
    set(other_host linux-x64)
  endif()
  file(MAKE_DIRECTORY "${BIN}/lock-host-mismatch")
  set(bad_lock "${BIN}/lock-host-mismatch/arm-none-eabi.lock")
  file(READ "${lock}" lock_contents)
  string(REPLACE "${native_host}.tar.gz" "${other_host}.tar.gz" bad_contents "${lock_contents}")
  if(bad_contents STREQUAL lock_contents)
    message(FATAL_ERROR "toolchain-case could-not-mutate-host-asset")
  endif()
  file(WRITE "${bad_lock}" "${bad_contents}")
  set(fake_curl "${BIN}/lock-host-mismatch/fake-curl.sh")
  file(WRITE "${fake_curl}" "#!/bin/sh\n: > \"$TOOLCHAIN_TEST_CALL_LOG\"\nprintf '500\\n'\n")
  file(CHMOD "${fake_curl}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
  set(call_log "${BIN}/lock-host-mismatch/curl-called")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "CURL=${fake_curl}" "TOOLCHAIN_TEST_CALL_LOG=${call_log}"
      "${fetcher}" --lock "${bad_lock}" --root "${BIN}/lock-host-mismatch/root"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
  if("${result}" STREQUAL "0" OR NOT stderr MATCHES "toolchain: lock-url")
    message(FATAL_ERROR "toolchain-case expected lock-url refusal, got result=${result}, output=${stdout}${stderr}")
  endif()
  if(EXISTS "${call_log}")
    message(FATAL_ERROR "toolchain-case mismatched-host-url-reached-curl")
  endif()
  message(STATUS "toolchain-case lock-host-mismatch PASSED native=${native_host} refused=${other_host}-asset-before-curl")
elseif(CASE MATCHES "^(offline-all|wrong-release|wrong-host|wrong-host-row|wrong-redirect-host|redirect-downgrade|redirect-count|unexpected-status|redirected-status|empty-payload|short-payload|long-payload|digest-mismatch|corrupt-archive|absolute-member|parent-member|top-level-member|escaping-symlink|unsafe-symlink-target|malformed-symlink|duplicate-member|unsafe-hardlink-target|hardlink-target-missing|hardlink-target-directory|special-node|malformed-listing|prior-install|valid-redirect)$")
  run_offline_suite()
elseif(CASE STREQUAL "installed")
  if(NOT DEFINED BIN OR "${BIN}" STREQUAL "")
    message(FATAL_ERROR "toolchain-case missing-bin-root")
  endif()
  if(NOT DEFINED TOOLCHAIN_ROOT OR "${TOOLCHAIN_ROOT}" STREQUAL "")
    message(FATAL_ERROR "toolchain-case missing-toolchain-root")
  endif()
  set(fetcher "${SRC}/scripts/fetch-toolchain.sh")
  if(NOT EXISTS "${fetcher}")
    message(FATAL_ERROR "toolchain-case fetcher-missing")
  endif()
  set(root_abs "${TOOLCHAIN_ROOT}")
  cmake_path(ABSOLUTE_PATH root_abs BASE_DIRECTORY "${SRC}" NORMALIZE)
  set(native_dir "${root_abs}/arm-none-eabi")
  detect_native_host(uname host_ok native_host host_system host_machine
    host_error)
  if(NOT host_ok)
    message(FATAL_ERROR "toolchain-case ${host_error}")
  endif()
  set(install_dir "${native_dir}/${native_host}-15.2.1-1.1")
  if(EXISTS "${install_dir}/install.json")
    execute_process(
      COMMAND "${fetcher}" --lock "${lock}" --root "${TOOLCHAIN_ROOT}" --check
      RESULT_VARIABLE install_result OUTPUT_VARIABLE install_stdout ERROR_VARIABLE install_stderr)
  else()
    execute_process(
      COMMAND "${fetcher}" --lock "${lock}" --root "${TOOLCHAIN_ROOT}"
      RESULT_VARIABLE install_result OUTPUT_VARIABLE install_stdout ERROR_VARIABLE install_stderr)
  endif()
  if(NOT "${install_result}" STREQUAL "0")
    message(FATAL_ERROR "toolchain-case installed failed (${install_result}): ${install_stdout}${install_stderr}")
  endif()
  if(NOT EXISTS "${install_dir}/install.json")
    message(FATAL_ERROR "toolchain-case receipt-missing")
  endif()
  file(READ "${install_dir}/install.json" receipt)
  string(JSON receipt_host ERROR_VARIABLE json_error GET "${receipt}" host)
  if(json_error OR NOT receipt_host STREQUAL native_host)
    message(FATAL_ERROR "toolchain-case receipt-host-mismatch")
  endif()
  string(JSON receipt_release ERROR_VARIABLE json_error GET "${receipt}" release)
  if(json_error OR NOT receipt_release STREQUAL "15.2.1-1.1")
    message(FATAL_ERROR "toolchain-case receipt-release-mismatch")
  endif()
  string(JSON receipt_hash ERROR_VARIABLE json_error GET "${receipt}" archive_sha256)
  string(JSON receipt_bytes ERROR_VARIABLE json_error GET "${receipt}" archive_bytes)
  string(JSON download_seconds ERROR_VARIABLE json_error GET "${receipt}" download_seconds)
  string(JSON archive_validation_seconds ERROR_VARIABLE validation_error
    GET "${receipt}" archive_validation_seconds)
  string(JSON extract_seconds ERROR_VARIABLE json_error GET "${receipt}" extract_seconds)
  string(JSON setup_seconds ERROR_VARIABLE setup_error GET "${receipt}" setup_seconds)
  string(JSON notice_count ERROR_VARIABLE json_error LENGTH "${receipt}" notices)
  string(LENGTH "${receipt_hash}" receipt_hash_length)
  if(native_host STREQUAL "darwin-arm64")
    set(expected_archive_hash "574082d35e49a2bcbdc355836b2a3ae5e5bb3b9456c9f5e37177db2ab4aad870")
    set(expected_archive_bytes "293820200")
  elseif(native_host STREQUAL "linux-x64")
    set(expected_archive_hash "da6a49ad4003944b823c6c93702a8787c922ab34bd7e918ec0eaf6933a9b1ff6")
    set(expected_archive_bytes "307090703")
  else()
    set(expected_archive_hash "67980c7990eba7bb7ffdf39699102effd70889f5ac427be19a8c8a6c5fab2972")
    set(expected_archive_bytes "302624714")
  endif()
  if(json_error OR NOT receipt_hash_length EQUAL 64 OR
     NOT receipt_hash MATCHES "^[0-9a-f]+$" OR
     NOT receipt_bytes STREQUAL expected_archive_bytes OR
     NOT receipt_hash STREQUAL expected_archive_hash OR
     NOT download_seconds MATCHES "^[0-9]+$" OR
     NOT validation_error STREQUAL "NOTFOUND" OR
     NOT archive_validation_seconds MATCHES "^[0-9]+$" OR
     NOT extract_seconds MATCHES "^[0-9]+$" OR
     NOT setup_error STREQUAL "NOTFOUND" OR
     NOT setup_seconds MATCHES "^[0-9]+$" OR notice_count LESS 1)
    message(FATAL_ERROR "toolchain-case receipt-identity-or-notices-invalid hash-length=${receipt_hash_length} notices=${notice_count} error=${json_error}")
  endif()
  string(FIND "${receipt}" "?" signed_query)
  if(NOT signed_query EQUAL -1)
    message(FATAL_ERROR "toolchain-case signed-url-present-in-receipt")
  endif()
  set(tool_bin "${install_dir}/bin")
  foreach(tool arm-none-eabi-as arm-none-eabi-readelf)
    execute_process(
      COMMAND "${tool_bin}/${tool}" --version
      RESULT_VARIABLE tool_result OUTPUT_VARIABLE tool_stdout ERROR_VARIABLE tool_stderr)
    if(NOT "${tool_result}" STREQUAL "0" OR NOT tool_stdout MATCHES "GNU")
      message(FATAL_ERROR "toolchain-case ${tool}-unusable: ${tool_stdout}${tool_stderr}")
    endif()
  endforeach()
  if(NOT install_stdout MATCHES "host=${native_host}" OR
     NOT install_stdout MATCHES "release=15.2.1-1.1" OR
     NOT install_stdout MATCHES "archive_validation_seconds=[0-9]+" OR
     NOT install_stdout MATCHES "setup_seconds=[0-9]+")
    message(FATAL_ERROR "toolchain-case install-receipt-output-missing: ${install_stdout}")
  endif()
  message(STATUS "toolchain-case installed PASSED host=${native_host} release=15.2.1-1.1 archive_sha256=${receipt_hash} archive_bytes=${receipt_bytes} download_seconds=${download_seconds} archive_validation_seconds=${archive_validation_seconds} extract_seconds=${extract_seconds} setup_seconds=${setup_seconds} notices=${notice_count}")
else()
  message(FATAL_ERROR "toolchain-case unsupported-case: ${CASE}")
endif()
