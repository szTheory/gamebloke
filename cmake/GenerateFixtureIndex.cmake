cmake_minimum_required(VERSION 3.25)

# Build-only metadata generator. The production index is derived solely from the
# canonical manifest. The separate control index is derived from a generated
# synthetic manifest and never grants real fixture rights.
include("${CMAKE_CURRENT_LIST_DIR}/CheckManifest.cmake")

foreach(required IN ITEMS GBL_SOURCE_DIR GBL_CONTROL_IMAGE
    GBL_CONTROL_MANIFEST_OUTPUT GBL_MANIFEST GBL_REGISTRY GBL_VOCAB
    GBL_OUT_PRODUCTION GBL_OUT_CONTROL)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "fixture-index: usage: -D${required}= is required")
  endif()
endforeach()

get_filename_component(GBL_SOURCE_DIR "${GBL_SOURCE_DIR}" ABSOLUTE)
get_filename_component(GBL_CONTROL_IMAGE "${GBL_CONTROL_IMAGE}" ABSOLUTE)
get_filename_component(GBL_MANIFEST "${GBL_MANIFEST}" ABSOLUTE)
get_filename_component(GBL_REGISTRY "${GBL_REGISTRY}" ABSOLUTE)
get_filename_component(GBL_VOCAB "${GBL_VOCAB}" ABSOLUTE)
get_filename_component(GBL_OUT_PRODUCTION "${GBL_OUT_PRODUCTION}" ABSOLUTE)
get_filename_component(GBL_OUT_CONTROL "${GBL_OUT_CONTROL}" ABSOLUTE)
get_filename_component(GBL_CONTROL_MANIFEST_OUTPUT
  "${GBL_CONTROL_MANIFEST_OUTPUT}" ABSOLUTE)

# A failed rerun must not leave a stale index that looks current.
file(REMOVE "${GBL_OUT_PRODUCTION}" "${GBL_OUT_CONTROL}")
get_filename_component(production_dir "${GBL_OUT_PRODUCTION}" DIRECTORY)
get_filename_component(control_dir "${GBL_OUT_CONTROL}" DIRECTORY)
get_filename_component(control_manifest_dir
  "${GBL_CONTROL_MANIFEST_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${production_dir}" "${control_dir}"
  "${control_manifest_dir}")

if(NOT DEFINED GBL_CONTROL_MANIFEST OR "${GBL_CONTROL_MANIFEST}" STREQUAL "")
  if(NOT EXISTS "${GBL_CONTROL_IMAGE}" OR IS_DIRECTORY "${GBL_CONTROL_IMAGE}")
    message(FATAL_ERROR "fixture-index: control-image-missing")
  endif()
  # This CMake digest is independent of the generator's byte recipe.
  file(SHA256 "${GBL_CONTROL_IMAGE}" control_digest)
  string(CONCAT control_manifest
    "{\n"
    "  \"schema\": \"gamebloke.manifest/1\",\n"
    "  \"note\": \"Synthetic generated contract control only; not a real fixture or rights grant.\",\n"
    "  \"fixtures\": [\n"
    "    {\n"
    "      \"id\": \"synthetic-control-pass\",\n"
    "      \"source\": {\"kind\": \"in-tree\", \"path\": \"generated/fixtures/owned-arm-pass.gba\", \"sha256\": \"${control_digest}\"},\n"
    "      \"license\": {\"spdx\": \"LicenseRef-Gamebloke-Synthetic-Control\", \"notices\": [\"Artificial validator input for generated test bytes; no redistribution grant.\"], \"redistributable\": \"unknown\"},\n"
    "      \"build\": {\"recipe\": \"original Gamebloke instruction image generator\", \"toolchain\": {\"identity\": \"native C17 test generator\", \"sha256\": null}},\n"
    "      \"bios_requirement\": \"none\",\n"
    "      \"expected\": {\"provenance_class\": \"disputed\", \"dispute\": \"disp-gbatek-errata\"},\n"
    "      \"permitted_use\": [\"local-only\"]\n"
    "    }\n"
    "  ]\n"
    "}\n")
  file(WRITE "${GBL_CONTROL_MANIFEST_OUTPUT}" "${control_manifest}")
  set(control_manifest_path "${GBL_CONTROL_MANIFEST_OUTPUT}")
else()
  get_filename_component(control_manifest_path "${GBL_CONTROL_MANIFEST}" ABSOLUTE)
endif()

# Run the existing authority readers. Do not generate either header unless all
# vocabulary, registry, canonical-manifest, and synthetic-control findings are
# clear. The generated control's artificial license fields only test schema and
# dispute propagation; they do not approve any real input for redistribution.
gbl_contract_read(V "${GBL_VOCAB}")
gbl_registry_read(R "${GBL_REGISTRY}" V)
gbl_manifest_read(M "${GBL_MANIFEST}" R V)
gbl_manifest_read(C "${control_manifest_path}" R V)
get_property(findings GLOBAL PROPERTY GBL_FINDINGS)
if(findings GREATER 0)
  message(FATAL_ERROR "fixture-index: validation failed (${findings} findings)")
endif()

if(NOT EXISTS "${GBL_CONTROL_IMAGE}" OR IS_DIRECTORY "${GBL_CONTROL_IMAGE}")
  gbl_finding(fixture-index control-image-missing "synthetic-control-pass")
else()
  file(SHA256 "${GBL_CONTROL_IMAGE}" control_actual_digest)
  set(control_id "synthetic-control-pass")
  if(NOT control_id IN_LIST C_IDS)
    gbl_finding(fixture-index control-row-missing "${control_id}")
  else()
    set(control_expected_digest "${C_ROW_${control_id}_SHA256}")
    if(NOT "${control_expected_digest}" STREQUAL "${control_actual_digest}")
      gbl_finding(fixture-index digest-mismatch "${control_id}")
    endif()
  endif()
endif()

# For canonical in-tree rows, verify that the manifest digest still identifies
# the bytes at its validated relative path. URL rows carry the upstream digest;
# git rows have no byte digest in this schema and therefore remain non-matchable.
if(M_COUNT GREATER 0)
  gbl_json_load(production_doc "${GBL_MANIFEST}")
  math(EXPR production_last "${M_COUNT}-1")
  foreach(index RANGE ${production_last})
    gbl_json_string(row_id "${production_doc}" fixtures ${index} id)
    gbl_json_string(source_kind "${production_doc}" fixtures ${index} source kind)
    if("${source_kind}" STREQUAL "in-tree")
      gbl_json_string(relative_path "${production_doc}" fixtures ${index}
        source path)
      gbl_json_string(expected_digest "${production_doc}" fixtures ${index}
        source sha256)
      set(source_path "${GBL_SOURCE_DIR}/${relative_path}")
      if(NOT EXISTS "${source_path}" OR IS_DIRECTORY "${source_path}")
        gbl_finding(fixture-index in-tree-file-missing "${row_id}")
      else()
        file(SHA256 "${source_path}" actual_digest)
        if(NOT "${expected_digest}" STREQUAL "${actual_digest}")
          gbl_finding(fixture-index digest-mismatch "${row_id}")
        endif()
      endif()
    endif()
  endforeach()
endif()

get_property(findings GLOBAL PROPERTY GBL_FINDINGS)
if(findings GREATER 0)
  message(FATAL_ERROR "fixture-index: identity validation failed (${findings} findings)")
endif()

function(gbl_fixture_c_string out value where)
  # All emitted fields come from closed vocabularies or stricter manifest
  # grammars. Reject any character that would need C escaping instead of trying
  # to interpolate untrusted metadata into source text.
  if(NOT "${value}" MATCHES "^[A-Za-z0-9._,-]*$")
    message(FATAL_ERROR "fixture-index: unsafe-c-string: ${where}")
  endif()
  set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(gbl_fixture_emit_index output doc prefix)
  string(CONCAT contents
    "/* Generated from fatal-validated fixture metadata. Do not edit. */\n"
    "#ifndef GBL_GENERATED_FIXTURE_INDEX_H\n"
    "#define GBL_GENERATED_FIXTURE_INDEX_H\n"
    "#include <stddef.h>\n\n"
    "typedef struct gbl_fixture_metadata {\n"
    "    const char *id;\n"
    "    const char *rom_sha256;\n"
    "    const char *provenance_class;\n"
    "    const char *const *dispute_ids;\n"
    "    size_t dispute_count;\n"
    "    const char *bios_requirement;\n"
    "    const char *const *permitted_use;\n"
    "    size_t permitted_use_count;\n"
    "    const char *expected_mailbox_convention;\n"
    "} gbl_fixture_metadata;\n\n")

  set(ids "${${prefix}_IDS}")
  list(LENGTH ids row_count)
  set(row_index 0)
  foreach(id IN LISTS ids)
    gbl_fixture_c_string(c_id "${id}" "id")
    gbl_json_string(provenance "${doc}" fixtures ${row_index}
      expected provenance_class)
    gbl_fixture_c_string(c_provenance "${provenance}" "provenance_class")
    gbl_json_string(bios "${doc}" fixtures ${row_index} bios_requirement)
    gbl_fixture_c_string(c_bios "${bios}" "bios_requirement")
    set(digest "${${prefix}_ROW_${id}_SHA256}")
    gbl_fixture_c_string(c_digest "${digest}" "rom_sha256")

    gbl_json_type(dispute_type "${doc}" fixtures ${row_index} expected dispute)
    set(dispute "")
    if("${dispute_type}" STREQUAL "STRING")
      gbl_json_string(dispute "${doc}" fixtures ${row_index} expected dispute)
    endif()
    set(dispute_count 0)
    set(dispute_expr NULL)
    if(NOT "${dispute}" STREQUAL "")
      gbl_fixture_c_string(c_dispute "${dispute}" "dispute id")
      string(APPEND contents
        "static const char *const gbl_fixture_disputes_${row_index}[] = {\"${c_dispute}\"};\n")
      set(dispute_count 1)
      set(dispute_expr "gbl_fixture_disputes_${row_index}")
    endif()

    gbl_json_length(use_count "${doc}" fixtures ${row_index} permitted_use)
    string(APPEND contents
      "static const char *const gbl_fixture_uses_${row_index}[] = {\n")
    if(use_count GREATER 0)
      math(EXPR use_last "${use_count}-1")
      foreach(use_index RANGE ${use_last})
        gbl_json_string(use "${doc}" fixtures ${row_index} permitted_use
          ${use_index})
        gbl_fixture_c_string(c_use "${use}" "permitted_use")
        string(APPEND contents "    \"${c_use}\",\n")
      endforeach()
    endif()
    string(APPEND contents "};\n")

    string(APPEND contents
      "static const gbl_fixture_metadata gbl_fixture_row_${row_index} = {\n"
      "    \"${c_id}\", \"${c_digest}\", \"${c_provenance}\",\n"
      "    ${dispute_expr}, ${dispute_count}u, \"${c_bios}\",\n"
      "    gbl_fixture_uses_${row_index}, ${use_count}u,\n"
      "    \"five-word-v1-final-marker\"\n"
      "};\n")
    math(EXPR row_index "${row_index}+1")
  endforeach()

  if(row_count EQUAL 0)
    string(APPEND contents
      "static const gbl_fixture_metadata gbl_fixture_metadata_rows[1] = {{0}};\n")
  else()
    string(APPEND contents
      "static const gbl_fixture_metadata gbl_fixture_metadata_rows[] = {\n")
    math(EXPR row_last "${row_count}-1")
    foreach(index RANGE ${row_last})
      string(APPEND contents "    gbl_fixture_row_${index},\n")
    endforeach()
    string(APPEND contents "};\n")
  endif()
  string(APPEND contents
    "static const size_t gbl_fixture_metadata_count = ${row_count}u;\n"
    "#endif\n")
  file(WRITE "${output}" "${contents}")
endfunction()

# Re-read the now-valid documents for extraction. The validators above and this
# projection use the same authoritative files; no second schema or alternate
# metadata source is introduced.
gbl_json_load(production_doc "${GBL_MANIFEST}")
gbl_json_load(control_doc "${control_manifest_path}")
gbl_fixture_emit_index("${GBL_OUT_PRODUCTION}" "${production_doc}" M)
gbl_fixture_emit_index("${GBL_OUT_CONTROL}" "${control_doc}" C)
message("fixture-index: ok (production ${M_COUNT} rows; synthetic control ${C_COUNT} rows)")
