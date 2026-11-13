# Fixture manifest and pins linter (D-02, D-12, D-13, D-14, D-15, R-3, EVID-01, EVID-02).
#
#   include(cmake/CheckManifest.cmake)
#   gbl_manifest_read(<prefix> <manifest-file> <registry-prefix> <vocab-prefix>)
#   gbl_pins_read(<prefix> <pins-file>)
#
# Script mode:   cmake -DMODE=manifest -DMANIFEST=<tests/fixtures/manifest.json> \
#                      -DREGISTRY=<registry/disputes.json> -DVOCAB=<contracts/evidence.json> \
#                      -P cmake/CheckManifest.cmake
#                cmake -DMODE=pins -DPINS=<pins.txt> -P cmake/CheckManifest.cmake
#                cmake -DMODE=all -DMANIFEST=... -DPINS=... -DREGISTRY=... -DVOCAB=... \
#                      -P cmake/CheckManifest.cmake
#
# A manifest row declares one fixture: where it comes from, under which license, how it
# is built, which BIOS it needs, what provenance class its expected value has, and where
# it may be used. pins.txt is the reviewed list of pinned fetches. MODE=all also checks
# that the two agree. <registry-prefix> and <vocab-prefix> name the prefixes of prior
# gbl_registry_read and gbl_contract_read calls in the caller's scope.
#
# Row fields (schema gamebloke.manifest/1; every key below is required unless noted):
#   id                         ^[a-z0-9][a-z0-9._-]*$, unique
#   source.kind                git | url | in-tree
#   source.url                 git and url rows; https:// only
#   source.commit              git rows; exactly 40 lowercase hex (`tag` is an extra field)
#   source.path                in-tree rows; relative path
#   source.sha256              url and in-tree rows; 64 lowercase hex
#   license.spdx               non-empty string
#   license.notices            non-empty array of non-empty strings
#   license.redistributable    yes | no | unknown
#   build.recipe               non-empty string
#   build.toolchain.identity   non-empty string
#   build.toolchain.sha256     key required; null allowed; otherwise 64 lowercase hex
#   build.artifact             optional git-only object with exactly `path` and `sha256`
#                              keys; path is build-root-relative and SHA identifies output
#   bios_requirement           none | project-stub | hle | replacement | user-official
#   expected.provenance_class  one of the vocabulary provenance classes
#   expected.dispute           key required; null, or a registry dispute id
#   permitted_use              non-empty unique array of public-ci | local-only | differential-only
#   fetch.pin_dest             optional object; when present pin_dest is a relative path
#
# Manifest reason codes (printed as `manifest: <code>: <detail>`, every finding reported):
#   usage                  script mode with a missing or unknown MODE, or a missing input
#   bad-schema             schema is not exactly gamebloke.manifest/1
#   missing-field          fixtures absent; a required field absent, null, or an empty
#                          string (expected.dispute and build.toolchain.sha256 accept null);
#                          license.notices an empty array; fetch present without pin_dest
#   wrong-type             fixtures not an array; notices or permitted_use not an array; a
#                          notices item that is not a non-empty string; toolchain.sha256
#                          neither string nor null; an object required but another type found
#   bad-id                 id outside the pattern
#   duplicate-id           two rows share an id
#   bad-source-kind        source.kind is not git, url, or in-tree (case matters)
#   unpinned-revision      a git row whose commit is absent or not 40 lowercase hex
#   missing-digest         a url or in-tree sha256 absent or not 64 lowercase hex; a
#                          non-empty toolchain.sha256 that is not 64 lowercase hex
#   bad-url                a source.url that is not https:// or holds whitespace, a quote,
#                          a backslash, or userinfo (`user[:password]` before the host)
#   bad-path               an in-tree path or fetch.pin_dest with a leading slash, an empty,
#                          `.`, or `..` component, or a character outside [A-Za-z0-9._/-]
#   bad-artifact-source    build.artifact is present on a non-git source row
#   bad-artifact-key       build.artifact has an unknown key or does not have exactly two keys
#   bad-provenance-class   a class outside the vocabulary
#   bad-bios-requirement   a value outside the five allowed
#   bad-permitted-use      empty, an unknown value, or a duplicate
#   bad-redistributable    a value outside yes, no, unknown, or not a string
#   dispute-unknown        expected.dispute is not a registry id
#   dispute-class-mismatch class `disputed` without a dispute, or a dispute under any
#                          other class
#   rights-mismatch        `public-ci` in permitted_use while license.redistributable is
#                          not `yes`, or while bios_requirement is `user-official` (a
#                          matching hash proves identity, not redistribution rights)
#   pin-mismatch           MODE=all only: a row's pin_dest absent from pins.txt; a url row
#                          whose sha256 or URL differs from its pin; a git row whose pinned
#                          URL does not contain its commit or is not on the source.url host
#                          (raw.githubusercontent.com counts as github.com) under its
#                          repository path; an in-tree row with `fetch`; a pins row that no
#                          manifest row names (an orphan pin). A git or url row without
#                          `fetch` is allowed: it is simply not fetched by fetch-pin.sh.
#
# Pins reason codes (printed as `pins: <code>: <detail>`):
#   missing-file           the pins file is absent
#   missing-header         the file is empty, or its first line is not exactly
#                          `# gamebloke.pins/1 <sha256> <url> <dest>`; no row is read then
#   bad-row                not exactly three whitespace-separated fields, or the row holds a
#                          semicolon, square bracket, quote, or backslash
#   bad-digest             a digest that is not 64 lowercase hex
#   bad-url                a URL that is not https:// or holds whitespace, a quote, a
#                          backslash, or userinfo before the host
#   bad-dest               the bad-path rule applied to the destination
#   duplicate-dest         two rows share a destination
# After the header, `#` comment lines and blank lines are ignored. A header-only file is
# the stated zero-row state (`pins: ok (0 rows)`).
#
# The json:, contract:, and registry: codes come from the loader and the readers this
# file calls. gbl_manifest_read sets <prefix>_IDS, <prefix>_COUNT, and per row
# <prefix>_ROW_<id>_CLASS|DISPUTE|KIND|COMMIT|SHA256|ARTIFACT_PATH|ARTIFACT_SHA256|PIN_DEST|URL.
# SHA256 is the runtime ROM identity: source.sha256 for url/in-tree, artifact.sha256 for
# source-built git rows, and empty for legacy git rows. gbl_pins_read sets
# <prefix>_DESTS, <prefix>_COUNT, and per dest <prefix>_DEST_<dest>_SHA256|URL.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set.
cmake_minimum_required(VERSION 3.25)
include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CheckRegistry.cmake")

set(GBL_MANIFEST_BIOS_REQUIREMENTS none project-stub hle replacement user-official)
set(GBL_MANIFEST_PERMITTED_USE public-ci local-only differential-only)
set(GBL_PINS_HEADER "# gamebloke.pins/1 <sha256> <url> <dest>")

# gbl__manifest_is_hex(<out> <value> <digits>): TRUE for exactly <digits> lowercase hex.
function(gbl__manifest_is_hex out value digits)
  string(REPEAT "[0-9a-f]" ${digits} re)
  if("${value}" MATCHES "^${re}$")
    set(${out} TRUE PARENT_SCOPE)
  else()
    set(${out} FALSE PARENT_SCOPE)
  endif()
endfunction()

# gbl__manifest_path_ok(<out> <value>): a relative path from [A-Za-z0-9._/-] with no
# empty, `.`, or `..` component.
function(gbl__manifest_path_ok out value)
  set(ok TRUE)
  if("${value}" STREQUAL "")
    set(ok FALSE)
  elseif("${value}" MATCHES "^/")
    set(ok FALSE)
  elseif(NOT "${value}" MATCHES "^[A-Za-z0-9._/-]+$")
    set(ok FALSE)
  elseif("${value}" MATCHES "(^|/)(\\.\\.?)?(/|$)")
    set(ok FALSE)
  endif()
  set(${out} ${ok} PARENT_SCOPE)
endfunction()

# gbl__manifest_url_ok(<out> <value>): https://, no whitespace, quote, or backslash, and no
# userinfo before the host.
function(gbl__manifest_url_ok out value)
  set(ok TRUE)
  if(NOT "${value}" MATCHES "^https://.")
    set(ok FALSE)
  elseif("${value}" MATCHES "[ \t\r\n\"\\\\]")
    set(ok FALSE)
  elseif("${value}" MATCHES "^https://[^/?#]*@")
    # Credentials in the authority would be stored in a committed file.
    set(ok FALSE)
  endif()
  set(${out} ${ok} PARENT_SCOPE)
endfunction()

# gbl__manifest_req_string(<out> <doc> <where> <label> <key>...): the non-empty string at
# <key>..., else <missing> after one finding (absent, null, or empty: missing-field; any
# other type: wrong-type).
function(gbl__manifest_req_string out doc where label)
  gbl_json_type(ty "${doc}" ${ARGN})
  set(value "<missing>")
  if("${ty}" STREQUAL "MISSING" OR "${ty}" STREQUAL "NULL")
    gbl_finding(manifest missing-field "${where}: ${label}")
  elseif("${ty}" STREQUAL "STRING")
    gbl_json_string(got "${doc}" ${ARGN})
    if("${got}" STREQUAL "<missing>")
      gbl_finding(manifest missing-field "${where}: ${label} is empty")
    else()
      set(value "${got}")
    endif()
  else()
    gbl_finding(manifest wrong-type "${where}: ${label}")
  endif()
  set(${out} "${value}" PARENT_SCOPE)
endfunction()

# gbl__manifest_req_object(<out-ok> <doc> <where> <label> <key>...): TRUE for an object.
function(gbl__manifest_req_object out doc where label)
  gbl_json_type(ty "${doc}" ${ARGN})
  set(ok FALSE)
  if("${ty}" STREQUAL "MISSING" OR "${ty}" STREQUAL "NULL")
    gbl_finding(manifest missing-field "${where}: ${label}")
  elseif("${ty}" STREQUAL "OBJECT")
    set(ok TRUE)
  else()
    gbl_finding(manifest wrong-type "${where}: ${label}")
  endif()
  set(${out} ${ok} PARENT_SCOPE)
endfunction()

# gbl__manifest_row(<doc> <index> <registry-prefix> <vocab-prefix>): checks one row and
# reports it to the caller through _gbl_row_* variables (set only for a well-formed id).
function(gbl__manifest_row doc i registry vocab)
  set(reg_ids "${${registry}_IDS}")
  set(classes "${${vocab}_CLASSES}")

  gbl_json_type(row_ty "${doc}")
  if(NOT "${row_ty}" STREQUAL "OBJECT")
    gbl_finding(manifest wrong-type "fixtures[${i}]: not an object")
    return()
  endif()
  set(where "fixtures[${i}]")
  set(row_class "")
  set(row_dispute "")
  set(row_kind "")
  set(row_commit "")
  set(row_sha256 "")
  set(row_artifact_path "")
  set(row_artifact_sha256 "")
  set(row_artifact_ok FALSE)
  set(row_pin_dest "")
  set(row_url "")
  set(redist "")
  set(bios "")
  set(seen_use "")
  set(id_ok FALSE)

  gbl__manifest_req_string(id "${doc}" "${where}" id id)
  if(NOT "${id}" STREQUAL "<missing>")
    if("${id}" MATCHES "^[a-z0-9][a-z0-9._-]*$")
      set(id_ok TRUE)
      set(where "${id}")
    else()
      gbl_finding(manifest bad-id "${where}")
    endif()
  endif()

  # source
  gbl__manifest_req_object(src_ok "${doc}" "${where}" source source)
  if(src_ok)
    gbl__manifest_req_string(kind "${doc}" "${where}" source.kind source kind)
    if(NOT "${kind}" STREQUAL "<missing>")
      if(NOT "${kind}" MATCHES "^(git|url|in-tree)$")
        gbl_finding(manifest bad-source-kind "${where}")
      else()
        set(row_kind "${kind}")
        if("${kind}" STREQUAL "git" OR "${kind}" STREQUAL "url")
          gbl__manifest_req_string(url "${doc}" "${where}" source.url source url)
          if(NOT "${url}" STREQUAL "<missing>")
            gbl__manifest_url_ok(url_ok "${url}")
            if(NOT url_ok)
              gbl_finding(manifest bad-url "${where}: source.url")
            else()
              set(row_url "${url}")
            endif()
          endif()
        endif()
        if("${kind}" STREQUAL "git")
          gbl_json_string(commit "${doc}" source commit)
          gbl__manifest_is_hex(commit_ok "${commit}" 40)
          if(commit_ok)
            set(row_commit "${commit}")
          else()
            gbl_finding(manifest unpinned-revision "${where}: source.commit")
          endif()
        else()
          if("${kind}" STREQUAL "in-tree")
            gbl__manifest_req_string(path "${doc}" "${where}" source.path source path)
            if(NOT "${path}" STREQUAL "<missing>")
              gbl__manifest_path_ok(path_ok "${path}")
              if(NOT path_ok)
                gbl_finding(manifest bad-path "${where}: source.path")
              endif()
            endif()
          endif()
          gbl_json_string(sha "${doc}" source sha256)
          gbl__manifest_is_hex(sha_ok "${sha}" 64)
          if(sha_ok)
            set(row_sha256 "${sha}")
          else()
            gbl_finding(manifest missing-digest "${where}: source.sha256")
          endif()
        endif()
      endif()
    endif()
  endif()

  # license
  gbl__manifest_req_object(lic_ok "${doc}" "${where}" license license)
  if(lic_ok)
    gbl__manifest_req_string(spdx "${doc}" "${where}" license.spdx license spdx)
    gbl_json_type(nty "${doc}" license notices)
    if("${nty}" STREQUAL "MISSING" OR "${nty}" STREQUAL "NULL")
      gbl_finding(manifest missing-field "${where}: license.notices")
    elseif(NOT "${nty}" STREQUAL "ARRAY")
      gbl_finding(manifest wrong-type "${where}: license.notices")
    else()
      gbl_json_length(nlen "${doc}" license notices)
      if(nlen EQUAL 0)
        gbl_finding(manifest missing-field "${where}: license.notices is empty")
      else()
        math(EXPR nlast "${nlen}-1")
        foreach(n RANGE ${nlast})
          gbl_json_string(note "${doc}" license notices ${n})
          if("${note}" STREQUAL "<missing>")
            gbl_finding(manifest wrong-type "${where}: license.notices[${n}]")
          endif()
        endforeach()
      endif()
    endif()
    gbl_json_type(rty "${doc}" license redistributable)
    if("${rty}" STREQUAL "MISSING" OR "${rty}" STREQUAL "NULL")
      gbl_finding(manifest missing-field "${where}: license.redistributable")
    elseif("${rty}" STREQUAL "STRING")
      gbl_json_string(redist "${doc}" license redistributable)
      if("${redist}" STREQUAL "<missing>")
        gbl_finding(manifest missing-field "${where}: license.redistributable is empty")
      elseif(NOT "${redist}" MATCHES "^(yes|no|unknown)$")
        gbl_finding(manifest bad-redistributable "${where}")
      endif()
    else()
      gbl_finding(manifest bad-redistributable "${where}: not a string")
    endif()
  endif()

  # build
  gbl__manifest_req_object(build_ok "${doc}" "${where}" build build)
  if(build_ok)
    gbl__manifest_req_string(recipe "${doc}" "${where}" build.recipe build recipe)
    gbl__manifest_req_object(tc_ok "${doc}" "${where}" build.toolchain
      build toolchain)
    if(tc_ok)
      gbl__manifest_req_string(ident "${doc}" "${where}" build.toolchain.identity
        build toolchain identity)
      gbl_json_type(tsty "${doc}" build toolchain sha256)
      if("${tsty}" STREQUAL "MISSING")
        gbl_finding(manifest missing-field "${where}: build.toolchain.sha256")
      elseif("${tsty}" STREQUAL "STRING")
        gbl_json_string(tsha "${doc}" build toolchain sha256)
        if("${tsha}" STREQUAL "<missing>")
          gbl_finding(manifest missing-field "${where}: build.toolchain.sha256 is empty")
        else()
          gbl__manifest_is_hex(tsha_ok "${tsha}" 64)
          if(NOT tsha_ok)
            gbl_finding(manifest missing-digest "${where}: build.toolchain.sha256")
          endif()
        endif()
      elseif(NOT "${tsty}" STREQUAL "NULL")
        gbl_finding(manifest wrong-type "${where}: build.toolchain.sha256")
      endif()
    endif()

    # A source-built git row has two independent identities: source.commit for
    # reviewed inputs and build.artifact.sha256 for the actual ROM bytes. Older
    # git rows deliberately retain an empty runtime digest.
    gbl_json_type(artifact_ty "${doc}" build artifact)
    if(NOT "${artifact_ty}" STREQUAL "MISSING")
      if(NOT "${artifact_ty}" STREQUAL "OBJECT")
        gbl_finding(manifest wrong-type "${where}: build.artifact")
      else()
        if(NOT "${row_kind}" STREQUAL "git")
          gbl_finding(manifest bad-artifact-source
            "${where}: build.artifact requires source.kind git")
        endif()

        string(JSON artifact_key_count LENGTH "${doc}" build artifact)
        if(NOT artifact_key_count EQUAL 2)
          gbl_finding(manifest bad-artifact-key
            "${where}: build.artifact requires exactly path and sha256")
        endif()
        if(artifact_key_count GREATER 0)
          math(EXPR artifact_key_last "${artifact_key_count}-1")
          foreach(artifact_key_index RANGE ${artifact_key_last})
            string(JSON artifact_key MEMBER "${doc}" build artifact
              ${artifact_key_index})
            if(NOT "${artifact_key}" STREQUAL "path" AND
                NOT "${artifact_key}" STREQUAL "sha256")
              gbl_finding(manifest bad-artifact-key
                "${where}: build.artifact.${artifact_key}")
            endif()
          endforeach()
        endif()

        gbl__manifest_req_string(artifact_path "${doc}" "${where}"
          build.artifact.path build artifact path)
        if(NOT "${artifact_path}" STREQUAL "<missing>")
          gbl__manifest_path_ok(artifact_path_ok "${artifact_path}")
          if(NOT artifact_path_ok)
            gbl_finding(manifest bad-path "${where}: build.artifact.path")
          else()
            set(row_artifact_path "${artifact_path}")
          endif()
        endif()

        gbl_json_type(artifact_sha_ty "${doc}" build artifact sha256)
        if("${artifact_sha_ty}" STREQUAL "MISSING" OR
            "${artifact_sha_ty}" STREQUAL "NULL")
          gbl_finding(manifest missing-field "${where}: build.artifact.sha256")
        elseif("${artifact_sha_ty}" STREQUAL "STRING")
          string(JSON artifact_sha GET "${doc}" build artifact sha256)
          gbl__manifest_is_hex(artifact_sha_ok "${artifact_sha}" 64)
          if(artifact_sha_ok)
            set(row_artifact_sha256 "${artifact_sha}")
          else()
            gbl_finding(manifest missing-digest
              "${where}: build.artifact.sha256")
          endif()
        else()
          gbl_finding(manifest wrong-type "${where}: build.artifact.sha256")
        endif()

        if("${row_kind}" STREQUAL "git" AND artifact_key_count EQUAL 2 AND
            NOT "${row_artifact_path}" STREQUAL "" AND
            NOT "${row_artifact_sha256}" STREQUAL "")
          set(row_artifact_ok TRUE)
          set(row_sha256 "${row_artifact_sha256}")
        endif()
      endif()
    endif()
  endif()

  # bios_requirement
  gbl__manifest_req_string(bios "${doc}" "${where}" bios_requirement bios_requirement)
  if(NOT "${bios}" STREQUAL "<missing>" AND NOT "${bios}" IN_LIST GBL_MANIFEST_BIOS_REQUIREMENTS)
    gbl_finding(manifest bad-bios-requirement "${where}")
  endif()

  # expected
  gbl__manifest_req_object(exp_ok "${doc}" "${where}" expected expected)
  if(exp_ok)
    gbl__manifest_req_string(cls "${doc}" "${where}" expected.provenance_class
      expected provenance_class)
    set(cls_ok FALSE)
    if(NOT "${cls}" STREQUAL "<missing>")
      if("${cls}" IN_LIST classes)
        set(cls_ok TRUE)
        set(row_class "${cls}")
      else()
        gbl_finding(manifest bad-provenance-class "${where}: ${cls}")
      endif()
    endif()
    gbl_json_type(dty "${doc}" expected dispute)
    set(dispute_state "invalid")
    if("${dty}" STREQUAL "MISSING")
      gbl_finding(manifest missing-field "${where}: expected.dispute")
    elseif("${dty}" STREQUAL "NULL")
      set(dispute_state "none")
    elseif("${dty}" STREQUAL "STRING")
      gbl_json_string(did "${doc}" expected dispute)
      if("${did}" STREQUAL "<missing>")
        gbl_finding(manifest missing-field "${where}: expected.dispute is empty")
      else()
        set(dispute_state "set")
        set(row_dispute "${did}")
        if(NOT "${did}" IN_LIST reg_ids)
          gbl_finding(manifest dispute-unknown "${where}: ${did}")
        endif()
      endif()
    else()
      gbl_finding(manifest wrong-type "${where}: expected.dispute")
    endif()
    if(cls_ok)
      if("${cls}" STREQUAL "disputed" AND "${dispute_state}" STREQUAL "none")
        gbl_finding(manifest dispute-class-mismatch "${where}: class disputed without a dispute")
      elseif(NOT "${cls}" STREQUAL "disputed" AND "${dispute_state}" STREQUAL "set")
        gbl_finding(manifest dispute-class-mismatch "${where}: dispute under class ${cls}")
      endif()
    endif()
  endif()

  # permitted_use
  gbl_json_type(pty "${doc}" permitted_use)
  if("${pty}" STREQUAL "MISSING" OR "${pty}" STREQUAL "NULL")
    gbl_finding(manifest missing-field "${where}: permitted_use")
  elseif(NOT "${pty}" STREQUAL "ARRAY")
    gbl_finding(manifest wrong-type "${where}: permitted_use")
  else()
    gbl_json_length(plen "${doc}" permitted_use)
    if(plen EQUAL 0)
      gbl_finding(manifest bad-permitted-use "${where}: empty")
    else()
      set(seen_use "")
      math(EXPR plast "${plen}-1")
      foreach(p RANGE ${plast})
        gbl_json_string(use "${doc}" permitted_use ${p})
        if("${use}" STREQUAL "<missing>" OR NOT "${use}" IN_LIST GBL_MANIFEST_PERMITTED_USE)
          gbl_finding(manifest bad-permitted-use "${where}: permitted_use[${p}]")
        elseif("${use}" IN_LIST seen_use)
          gbl_finding(manifest bad-permitted-use "${where}: duplicate ${use}")
        else()
          list(APPEND seen_use "${use}")
        endif()
      endforeach()
    endif()
  endif()

  # fetch (optional)
  gbl_json_type(fty "${doc}" fetch)
  if(NOT "${fty}" STREQUAL "MISSING")
    if(NOT "${fty}" STREQUAL "OBJECT")
      gbl_finding(manifest wrong-type "${where}: fetch")
    else()
      gbl__manifest_req_string(dest "${doc}" "${where}" fetch.pin_dest fetch pin_dest)
      if(NOT "${dest}" STREQUAL "<missing>")
        gbl__manifest_path_ok(dest_ok "${dest}")
        if(dest_ok)
          set(row_pin_dest "${dest}")
        else()
          gbl_finding(manifest bad-path "${where}: fetch.pin_dest")
        endif()
      endif()
    endif()
  endif()

  # Rights cross-check: `public-ci` is the one use that publishes the bytes.
  if("public-ci" IN_LIST seen_use)
    if(NOT "${redist}" STREQUAL "" AND NOT "${redist}" STREQUAL "yes")
      gbl_finding(manifest rights-mismatch
        "${where}: public-ci needs license.redistributable yes, not ${redist}")
    endif()
    if("${bios}" STREQUAL "user-official")
      gbl_finding(manifest rights-mismatch
        "${where}: public-ci conflicts with bios_requirement user-official")
    endif()
  endif()

  if(id_ok)
    set(_gbl_row_id "${id}" PARENT_SCOPE)
    set(_gbl_row_class "${row_class}" PARENT_SCOPE)
    set(_gbl_row_dispute "${row_dispute}" PARENT_SCOPE)
    set(_gbl_row_kind "${row_kind}" PARENT_SCOPE)
    set(_gbl_row_commit "${row_commit}" PARENT_SCOPE)
    set(_gbl_row_sha256 "${row_sha256}" PARENT_SCOPE)
    set(_gbl_row_artifact_path "${row_artifact_path}" PARENT_SCOPE)
    set(_gbl_row_artifact_sha256 "${row_artifact_sha256}" PARENT_SCOPE)
    set(_gbl_row_artifact_ok "${row_artifact_ok}" PARENT_SCOPE)
    set(_gbl_row_pin_dest "${row_pin_dest}" PARENT_SCOPE)
    set(_gbl_row_url "${row_url}" PARENT_SCOPE)
  endif()
endfunction()

function(gbl_manifest_read prefix manifest registry vocab)
  gbl_json_load(doc "${manifest}")

  gbl_json_string(schema "${doc}" schema)
  if(NOT "${schema}" STREQUAL "gamebloke.manifest/1")
    gbl_finding(manifest bad-schema "${manifest}")
  endif()

  set(ids "")
  set(count 0)
  gbl_json_type(fty "${doc}" fixtures)
  if("${fty}" STREQUAL "MISSING" OR "${fty}" STREQUAL "NULL")
    gbl_finding(manifest missing-field "fixtures")
  elseif(NOT "${fty}" STREQUAL "ARRAY")
    gbl_finding(manifest wrong-type "fixtures")
  else()
    gbl_json_length(count "${doc}" fixtures)
    if(count GREATER 0)
      math(EXPR last "${count}-1")
      foreach(i RANGE ${last})
        set(_gbl_row_id "")
        # Parse the large array once per row, then validate its small object.
        # CMake reparses JSON for every accessor; whole-array field access makes
        # an expanded production manifest needlessly quadratic in field count.
        string(JSON row_doc GET "${doc}" fixtures ${i})
        gbl__manifest_row("${row_doc}" ${i} ${registry} ${vocab})
        if(NOT "${_gbl_row_id}" STREQUAL "")
          set(id "${_gbl_row_id}")
          if("${id}" IN_LIST ids)
            gbl_finding(manifest duplicate-id "${id}")
          else()
            list(APPEND ids "${id}")
            set(${prefix}_ROW_${id}_JSON "${row_doc}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_CLASS "${_gbl_row_class}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_DISPUTE "${_gbl_row_dispute}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_KIND "${_gbl_row_kind}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_COMMIT "${_gbl_row_commit}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_SHA256 "${_gbl_row_sha256}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_ARTIFACT_PATH "${_gbl_row_artifact_path}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_ARTIFACT_SHA256 "${_gbl_row_artifact_sha256}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_ARTIFACT_OK "${_gbl_row_artifact_ok}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_PIN_DEST "${_gbl_row_pin_dest}" PARENT_SCOPE)
            set(${prefix}_ROW_${id}_URL "${_gbl_row_url}" PARENT_SCOPE)
          endif()
        endif()
      endforeach()
    endif()
  endif()

  set(${prefix}_IDS "${ids}" PARENT_SCOPE)
  set(${prefix}_COUNT "${count}" PARENT_SCOPE)
endfunction()

# gbl_pins_read(<prefix> <pins-file>): the pinned-fetch list. The text is scanned line by
# line with string(FIND) on newlines; a row never goes through list handling because a
# malformed row may hold semicolons or brackets.
function(gbl_pins_read prefix pins)
  set(dests "")
  set(count 0)
  if(NOT EXISTS "${pins}" OR IS_DIRECTORY "${pins}")
    gbl_finding(pins missing-file "${pins}")
    set(${prefix}_DESTS "" PARENT_SCOPE)
    set(${prefix}_COUNT 0 PARENT_SCOPE)
    return()
  endif()
  file(READ "${pins}" text)

  # Header first; a file that fails it reports no row findings.
  string(FIND "${text}" "\n" nl)
  if(nl EQUAL -1)
    set(first "${text}")
    set(rest "")
  else()
    string(SUBSTRING "${text}" 0 ${nl} first)
    math(EXPR from "${nl}+1")
    string(SUBSTRING "${text}" ${from} -1 rest)
  endif()
  if(NOT "${first}" STREQUAL "${GBL_PINS_HEADER}")
    gbl_finding(pins missing-header "${pins}")
    set(${prefix}_DESTS "" PARENT_SCOPE)
    set(${prefix}_COUNT 0 PARENT_SCOPE)
    return()
  endif()

  set(more TRUE)
  set(lineno 1)
  while(more)
    string(FIND "${rest}" "\n" nl)
    if(nl EQUAL -1)
      set(line "${rest}")
      set(more FALSE)
    else()
      string(SUBSTRING "${rest}" 0 ${nl} line)
      math(EXPR from "${nl}+1")
      string(SUBSTRING "${rest}" ${from} -1 rest)
    endif()
    math(EXPR lineno "${lineno}+1")

    if("${line}" MATCHES "^[ \t\r]*$" OR "${line}" MATCHES "^[ \t]*#")
      continue()
    endif()
    math(EXPR count "${count}+1")

    set(shape_ok TRUE)
    if("${line}" MATCHES "[];[\"\\\\]")
      gbl_finding(pins bad-row "line ${lineno}: forbidden character")
      set(shape_ok FALSE)
    endif()
    if(NOT "${line}" MATCHES "^[ \t]*([^ \t\r]+)[ \t]+([^ \t\r]+)[ \t]+([^ \t\r]+)[ \t\r]*$")
      if(shape_ok)
        gbl_finding(pins bad-row "line ${lineno}: not three fields")
      endif()
      continue()
    endif()
    set(digest "${CMAKE_MATCH_1}")
    set(url "${CMAKE_MATCH_2}")
    set(dest "${CMAKE_MATCH_3}")

    gbl__manifest_is_hex(digest_ok "${digest}" 64)
    if(NOT digest_ok)
      gbl_finding(pins bad-digest "line ${lineno}")
    endif()
    gbl__manifest_url_ok(url_ok "${url}")
    if(NOT url_ok)
      gbl_finding(pins bad-url "line ${lineno}")
    endif()
    gbl__manifest_path_ok(dest_ok "${dest}")
    if(NOT dest_ok)
      gbl_finding(pins bad-dest "line ${lineno}")
    elseif("${dest}" IN_LIST dests)
      gbl_finding(pins duplicate-dest "line ${lineno}: ${dest}")
    else()
      list(APPEND dests "${dest}")
      set(${prefix}_DEST_${dest}_SHA256 "${digest}" PARENT_SCOPE)
      set(${prefix}_DEST_${dest}_URL "${url}" PARENT_SCOPE)
    endif()
  endwhile()

  set(${prefix}_DESTS "${dests}" PARENT_SCOPE)
  set(${prefix}_COUNT "${count}" PARENT_SCOPE)
endfunction()

# gbl__manifest_url_parts(<host-out> <path-out> <url>): host and repository path of an
# https URL, `.git` and trailing slashes stripped; raw.githubusercontent.com is github.com.
function(gbl__manifest_url_parts host_out path_out url)
  set(host "")
  set(path "")
  if("${url}" MATCHES "^https://([^/?#]+)([^?#]*)")
    set(host "${CMAKE_MATCH_1}")
    set(path "${CMAKE_MATCH_2}")
    string(REGEX REPLACE "/+$" "" path "${path}")
    string(REGEX REPLACE "\\.git$" "" path "${path}")
    if("${host}" STREQUAL "raw.githubusercontent.com")
      set(host "github.com")
    endif()
  endif()
  set(${host_out} "${host}" PARENT_SCOPE)
  set(${path_out} "${path}" PARENT_SCOPE)
endfunction()

# gbl__manifest_pin_check(<manifest-prefix> <pins-prefix>): MODE=all cross-checks.
function(gbl__manifest_pin_check m p)
  set(referenced "")
  foreach(id IN LISTS ${m}_IDS)
    set(dest "${${m}_ROW_${id}_PIN_DEST}")
    set(kind "${${m}_ROW_${id}_KIND}")
    if("${dest}" STREQUAL "")
      continue()
    endif()
    list(APPEND referenced "${dest}")
    if("${kind}" STREQUAL "in-tree")
      gbl_finding(manifest pin-mismatch "${id}: an in-tree row has fetch")
    elseif(NOT "${dest}" IN_LIST ${p}_DESTS)
      gbl_finding(manifest pin-mismatch "${id}: ${dest} is not in pins.txt")
    elseif("${kind}" STREQUAL "url")
      if(NOT "${${m}_ROW_${id}_SHA256}" STREQUAL "${${p}_DEST_${dest}_SHA256}")
        gbl_finding(manifest pin-mismatch "${id}: sha256 differs from the pin for ${dest}")
      endif()
      if(NOT "${${m}_ROW_${id}_URL}" STREQUAL "${${p}_DEST_${dest}_URL}")
        gbl_finding(manifest pin-mismatch "${id}: pinned URL for ${dest} differs from source.url")
      endif()
    elseif("${kind}" STREQUAL "git")
      set(commit "${${m}_ROW_${id}_COMMIT}")
      set(pinned "${${p}_DEST_${dest}_URL}")
      if(NOT "${commit}" STREQUAL "")
        string(FIND "${pinned}" "${commit}" at)
        if(at EQUAL -1)
          gbl_finding(manifest pin-mismatch "${id}: pinned URL for ${dest} lacks the commit")
        endif()
      endif()
      # The pinned raw URL must come from the repository the row names.
      gbl__manifest_url_parts(src_host src_path "${${m}_ROW_${id}_URL}")
      gbl__manifest_url_parts(pin_host pin_path "${pinned}")
      if(NOT "${src_host}" STREQUAL "")
        string(FIND "${pin_path}/" "${src_path}/" under)
        if(NOT "${src_host}" STREQUAL "${pin_host}" OR NOT under EQUAL 0)
          gbl_finding(manifest pin-mismatch
            "${id}: pinned URL for ${dest} is not under source.url")
        endif()
      endif()
    endif()
  endforeach()
  foreach(dest IN LISTS ${p}_DESTS)
    if(NOT "${dest}" IN_LIST referenced)
      gbl_finding(manifest pin-mismatch "orphan pin: ${dest} is named by no manifest row")
    endif()
  endforeach()
endfunction()

# ---- script-mode entry (inert when include()d) --------------------------------
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  if(NOT DEFINED MODE OR "${MODE}" STREQUAL "")
    message(FATAL_ERROR "manifest: usage: -DMODE= is required (manifest, pins, or all)")
  endif()
  if(NOT "${MODE}" MATCHES "^(manifest|pins|all)$")
    message(FATAL_ERROR "manifest: usage: unknown MODE (manifest, pins, or all)")
  endif()
  set(required "")
  if("${MODE}" STREQUAL "manifest")
    set(required MANIFEST REGISTRY VOCAB)
  elseif("${MODE}" STREQUAL "pins")
    set(required PINS)
  else()
    set(required MANIFEST PINS REGISTRY VOCAB)
  endif()
  foreach(var IN LISTS required)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
      message(FATAL_ERROR "manifest: usage: -D${var}= is required for MODE=${MODE}")
    endif()
  endforeach()

  if("${MODE}" STREQUAL "pins")
    gbl_pins_read(P "${PINS}")
    gbl_findings_finish(pins "ok (${P_COUNT} rows)")
  else()
    gbl_contract_read(V "${VOCAB}")
    gbl_registry_read(R "${REGISTRY}" V)
    gbl_manifest_read(M "${MANIFEST}" R V)
    if("${MODE}" STREQUAL "manifest")
      gbl_findings_finish(manifest "ok (${M_COUNT} rows)")
    else()
      gbl_pins_read(P "${PINS}")
      gbl__manifest_pin_check(M P)
      get_property(findings GLOBAL PROPERTY GBL_FINDINGS)
      if(findings GREATER 0)
        message(FATAL_ERROR "manifest: failed (${findings} findings)")
      endif()
      message("manifest: ok (${M_COUNT} rows)")
      message("pins: ok (${P_COUNT} rows)")
    endif()
  endif()
endif()
