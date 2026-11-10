# Gate tests (ci and hygiene lanes). Included from the top-level CMakeLists.txt.
# Each gate has a negative control that must fail for its stated reason, and a
# positive control on otherwise identical clean input (D-03).
set(GBL_HYGIENE_DRIVER
  "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}")
set(GBL_HYGIENE_CASE "${CMAKE_SOURCE_DIR}/tests/gates/hygiene_case.cmake")

# ---- hygiene (VERIF-07) ---------------------------------------------------
# The real tree must be clean. Runs from the source root because --tree lists
# files with git.
gbl_add_test(
  NAME hygiene.tree
  LABELS gate hygiene
  COMMAND sh scripts/hygiene.sh --tree
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

foreach(case IN ITEMS
    credential credential_upper private_key email home_path home_path_encoded newline_name)
  gbl_add_test(
    NAME hygiene.nc.${case}
    LABELS control hygiene
    COMMAND ${GBL_HYGIENE_DRIVER} "-DCASE=${case}" -P "${GBL_HYGIENE_CASE}")
endforeach()

gbl_add_test(
  NAME hygiene.pos.clean
  LABELS hygiene positive
  COMMAND ${GBL_HYGIENE_DRIVER} -DCASE=clean -P "${GBL_HYGIENE_CASE}")

gbl_add_test(
  NAME hygiene.usage
  LABELS hygiene unit
  COMMAND ${GBL_HYGIENE_DRIVER} -DCASE=usage -P "${GBL_HYGIENE_CASE}")

# Staged mode scans the staged blobs and checks the commit identity.
foreach(case IN ITEMS staged_home_path staged_identity)
  gbl_add_test(
    NAME hygiene.nc.${case}
    LABELS control hygiene
    COMMAND ${GBL_HYGIENE_DRIVER} "-DCASE=${case}" -P "${GBL_HYGIENE_CASE}")
endforeach()
gbl_add_test(
  NAME hygiene.pos.staged_clean
  LABELS hygiene positive
  COMMAND ${GBL_HYGIENE_DRIVER} -DCASE=staged_clean -P "${GBL_HYGIENE_CASE}")

# The opt-in hook (D-14) stays present, executable, and wired to `--staged`.
# One control per failure reason, each on a planted copy of the real hook.
gbl_add_test(
  NAME hygiene.hook
  LABELS hygiene unit
  COMMAND ${GBL_HYGIENE_DRIVER} -DCASE=hook -P "${GBL_HYGIENE_CASE}")
foreach(case IN ITEMS hook_missing hook_not_executable hook_no_staged)
  gbl_add_test(
    NAME hygiene.nc.${case}
    LABELS control hygiene
    COMMAND ${GBL_HYGIENE_DRIVER} "-DCASE=${case}" -P "${GBL_HYGIENE_CASE}")
endforeach()

# ---- signature scan (EVID-04, VERIF-02, VERIF-03) -------------------------
# Digest-only: the lists under tests/signatures/ hold SHA-256 values, never
# bytes. The scan hashes with `cmake -E sha256sum`, so the CMake that runs the
# tests is passed through the CMAKE environment variable.
set(GBL_SIG_ENV "${CMAKE_COMMAND}" -E env "CMAKE=${CMAKE_COMMAND}")
set(GBL_SIG_CHECK "${CMAKE_SOURCE_DIR}/cmake/CheckSignatureDigests.cmake")

gbl_add_test(
  NAME signature.tree
  LABELS gate signature
  COMMAND ${GBL_SIG_ENV} sh scripts/signature-scan.sh --tree
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# The committed lists are well formed and the scan reads exactly these files.
gbl_add_test(
  NAME signature.digests.wellformed
  LABELS signature unit
  COMMAND "${CMAKE_COMMAND}" -DMODE=wellformed "-DSRC=${CMAKE_SOURCE_DIR}"
    -P "${GBL_SIG_CHECK}")

# While the logo row is `owner-pending` this reports SKIPPED, never PASS.
gbl_add_test(
  NAME signature.logo.real_digest
  LABELS signature unit
  SKIPPABLE
  COMMAND "${CMAKE_COMMAND}" -DMODE=real-logo "-DSRC=${CMAKE_SOURCE_DIR}"
    -P "${GBL_SIG_CHECK}")

# Owner-local cross-check; skips unless GBL_OWNER_LOGO_FILE names a file.
gbl_add_test(
  NAME signature.owner_local
  LABELS owner signature
  SKIPPABLE
  COMMAND "${CMAKE_COMMAND}" -DMODE=owner-local "-DSRC=${CMAKE_SOURCE_DIR}"
    -P "${GBL_SIG_CHECK}")

# Controls: every matching path and every digest-list failure reason is driven
# by a synthesized stand-in or a mutated sandbox copy (D-03, D-08). Positive
# companions run the same gate on clean input.
set(GBL_SIG_DRIVER
  "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}")
set(GBL_SIG_CASE "${CMAKE_SOURCE_DIR}/tests/gates/signature_case.cmake")

foreach(case IN ITEMS
    logo bios rom_ext save_ext binary_unlisted bad_digest_row
    digest_too_short digest_wrong_size
    digests_missing_list digests_bad_bios_row digests_no_gba_bios
    digests_logo_row_count digests_bad_logo_row digests_wrong_default_path
    owner_file_mismatch)
  gbl_add_test(
    NAME signature.nc.${case}
    LABELS control signature
    COMMAND ${GBL_SIG_DRIVER} "-DCASE=${case}" -P "${GBL_SIG_CASE}")
endforeach()

foreach(case IN ITEMS
    binary_allowlisted bios_unlisted_digest digests_copy owner_file_match)
  gbl_add_test(
    NAME signature.pos.${case}
    LABELS positive signature
    COMMAND ${GBL_SIG_DRIVER} "-DCASE=${case}" -P "${GBL_SIG_CASE}")
endforeach()

# ---- signal handling (CR-01) ----------------------------------------------
# A scan that is signalled must end non-zero and never print `ok`: in POSIX sh
# a trap action that returns lets the script resume with its temp dir deleted.
# The probe signals a running scan (TERM and HUP; an async job cannot trap an
# ignored INT) and the static gate covers the INT handler and every other
# script. The control puts the pre-fix idiom back and requires the probe to fail.
set(GBL_SIGNAL_PROBE "${CMAKE_SOURCE_DIR}/tests/gates/signal_probe.sh")
set(GBL_SIGNAL_CASE "${CMAKE_SOURCE_DIR}/tests/gates/signal_case.cmake")

gbl_add_test(
  NAME hygiene.signal
  LABELS hygiene unit
  COMMAND sh "${GBL_SIGNAL_PROBE}" "${CMAKE_SOURCE_DIR}/scripts/hygiene.sh"
    "hygiene: home-path" "${CMAKE_BINARY_DIR}/nc/probe_hygiene")

gbl_add_test(
  NAME signature.signal
  LABELS signature unit
  COMMAND ${GBL_SIG_ENV} sh "${GBL_SIGNAL_PROBE}"
    "${CMAKE_SOURCE_DIR}/scripts/signature-scan.sh"
    "signature: rom-extension" "${CMAKE_BINARY_DIR}/nc/probe_signature")

gbl_add_test(
  NAME traps.static
  LABELS gate traps
  COMMAND sh scripts/check-traps.sh
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

foreach(case IN ITEMS exit_idiom no_handler no_files resumes)
  gbl_add_test(
    NAME traps.nc.${case}
    LABELS control traps
    COMMAND ${GBL_SIG_DRIVER} "-DCASE=${case}" -P "${GBL_SIGNAL_CASE}")
endforeach()
gbl_add_test(
  NAME traps.pos.clean
  LABELS positive traps
  COMMAND ${GBL_SIG_DRIVER} -DCASE=clean -P "${GBL_SIGNAL_CASE}")

# ---- evidence contracts (Phase 2) -------------------------------------------
# The Phase 2 validator gates and their controls follow, run in the ci and
# hygiene lanes. Register only through gbl_add_test at this top-level directory
# scope (D-04): every validator reason code gets a negative control that mutates
# a copy of a real input at test time, and every validator gets a positive
# control. No bad example is committed.
set(GBL_EVID_DRIVER
  "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}")
set(GBL_CONTRACT_CASE "${CMAKE_SOURCE_DIR}/tests/gates/contract_case.cmake")

# The real vocabulary passes the validator that pins the nine states.
gbl_add_test(
  NAME contract.lint
  LABELS contract gate
  COMMAND "${CMAKE_COMMAND}" "-DVOCAB=${CMAKE_SOURCE_DIR}/contracts/evidence.json"
    -P "${CMAKE_SOURCE_DIR}/cmake/CheckContract.cmake")

foreach(case IN ITEMS
    bad_schema bad_state_rule claim_set class_set label_set missing_field state_set usage)
  gbl_add_test(
    NAME contract.nc.${case}
    LABELS contract control
    COMMAND ${GBL_EVID_DRIVER} "-DCASE=${case}" -P "${GBL_CONTRACT_CASE}")
endforeach()

gbl_add_test(
  NAME contract.pos.copy
  LABELS contract positive
  COMMAND ${GBL_EVID_DRIVER} -DCASE=copy -P "${GBL_CONTRACT_CASE}")

# ---- dispute registry (Phase 2) ---------------------------------------------
# The real registry is checked against the real vocabulary; a control per reason
# code mutates a copy of the real registry (the six seed ids are pinned, so a
# well-formed extra entry is accepted but a dropped seed is not).
set(GBL_REGISTRY_CASE "${CMAKE_SOURCE_DIR}/tests/gates/registry_case.cmake")

gbl_add_test(
  NAME registry.lint
  LABELS gate registry
  COMMAND "${CMAKE_COMMAND}" "-DREGISTRY=${CMAKE_SOURCE_DIR}/registry/disputes.json"
    "-DVOCAB=${CMAKE_SOURCE_DIR}/contracts/evidence.json"
    -P "${CMAKE_SOURCE_DIR}/cmake/CheckRegistry.cmake")

foreach(case IN ITEMS
    bad_id bad_schema bad_source_label bad_status duplicate_id missing_field no_sources
    seed_missing usage)
  gbl_add_test(
    NAME registry.nc.${case}
    LABELS control registry
    COMMAND ${GBL_EVID_DRIVER} "-DCASE=${case}" -P "${GBL_REGISTRY_CASE}")
endforeach()

foreach(case IN ITEMS copy extra_entry)
  gbl_add_test(
    NAME registry.pos.${case}
    LABELS positive registry
    COMMAND ${GBL_EVID_DRIVER} "-DCASE=${case}" -P "${GBL_REGISTRY_CASE}")
endforeach()

# ---- strict JSON loader (Phase 2) -------------------------------------------
# One negative control per loader reason code, plus the two helper controls
# (`mutate: no-match`, `no-echo: leaked`), and positives on tricky and real input.
set(GBL_JSON_CASE "${CMAKE_SOURCE_DIR}/tests/gates/json_case.cmake")

foreach(case IN ITEMS
    comment duplicate_key mutate_no_match no_echo_guard not_json not_object
    trailing_comma trailing_content unreadable usage)
  gbl_add_test(
    NAME json.nc.${case}
    LABELS control json
    COMMAND ${GBL_EVID_DRIVER} "-DCASE=${case}" -P "${GBL_JSON_CASE}")
endforeach()

foreach(case IN ITEMS strict_clean real_docs)
  gbl_add_test(
    NAME json.pos.${case}
    LABELS json positive
    COMMAND ${GBL_EVID_DRIVER} "-DCASE=${case}" -P "${GBL_JSON_CASE}")
endforeach()
