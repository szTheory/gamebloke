include_guard(GLOBAL)
# Render only current in-memory rows from the canonical authority reader.
# No serialized success receipt can substitute for validation at the call site.
function(gbl_fixture_c_string out value where)
  # All emitted fields come from closed vocabularies or stricter manifest
  # grammars. Reject any character that would need C escaping instead of trying
  # to interpolate untrusted metadata into source text.
  if(NOT "${value}" MATCHES "^[A-Za-z0-9._,/-]*$")
    message(FATAL_ERROR "fixture-index: unsafe-c-string: ${where}")
  endif()
  set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(gbl_fixture_render_index out prefix)
  set(qualified 1)
  if(prefix STREQUAL "M" AND GBL_PROOF_METADATA)
    set(qualified 0)
  endif()
  string(CONCAT contents
    "/* Generated from fatal-validated fixture metadata. Do not edit. */\n"
    "#ifndef GBL_GENERATED_FIXTURE_INDEX_H\n"
    "#define GBL_GENERATED_FIXTURE_INDEX_H\n"
    "#include <stddef.h>\n\n"
    "#include <stdint.h>\n\n"
    "static const int gbl_fixture_index_qualified = ${qualified};\n\n"
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
    "    const char *result_channel_kind;\n"
    "    const char *result_source_commit;\n"
    "    const char *result_source_path;\n"
    "    uint32_t result_entry_address;\n"
    "    const char *result_instruction_set;\n"
    "    unsigned result_register_index;\n"
    "} gbl_fixture_metadata;\n\n")

  set(ids "${${prefix}_IDS}")
  list(LENGTH ids row_count)
  set(row_index 0)
  foreach(id IN LISTS ids)
    set(row_doc "${${prefix}_ROW_${id}_JSON}")
    gbl_fixture_c_string(c_id "${id}" "id")
    gbl_json_string(provenance "${row_doc}"
      expected provenance_class)
    gbl_fixture_c_string(c_provenance "${provenance}" "provenance_class")
    gbl_json_string(bios "${row_doc}" bios_requirement)
    gbl_fixture_c_string(c_bios "${bios}" "bios_requirement")
    set(digest "${${prefix}_ROW_${id}_SHA256}")
    gbl_fixture_c_string(c_digest "${digest}" "rom_sha256")

    set(channel_kind "${${prefix}_ROW_${id}_CHANNEL_KIND}")
    set(channel_source_commit "${${prefix}_ROW_${id}_CHANNEL_SOURCE_COMMIT}")
    set(channel_source_path "${${prefix}_ROW_${id}_CHANNEL_SOURCE_PATH}")
    set(channel_entry_address "${${prefix}_ROW_${id}_CHANNEL_ENTRY_ADDRESS}")
    set(channel_instruction_set "${${prefix}_ROW_${id}_CHANNEL_INSTRUCTION_SET}")
    set(channel_register_index "${${prefix}_ROW_${id}_CHANNEL_REGISTER_INDEX}")
    gbl_fixture_c_string(c_channel_kind "${channel_kind}" "result_channel.kind")
    gbl_fixture_c_string(c_channel_commit "${channel_source_commit}"
      "result_channel.source_commit")
    gbl_fixture_c_string(c_channel_path "${channel_source_path}"
      "result_channel.source_path")
    gbl_fixture_c_string(c_channel_instruction_set "${channel_instruction_set}"
      "result_channel.instruction_set")
    if("${channel_entry_address}" STREQUAL "")
      set(c_channel_address "0x00000000")
    elseif("${channel_entry_address}" MATCHES
        "^0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F][0-9A-F]$")
      set(c_channel_address "${channel_entry_address}")
    else()
      message(FATAL_ERROR "fixture-index: unsafe-numeric: result_channel.entry_address")
    endif()
    if("${channel_register_index}" STREQUAL "")
      set(c_channel_register_index 0)
    elseif("${channel_register_index}" MATCHES "^(0|[1-9]|1[0-4])$")
      set(c_channel_register_index "${channel_register_index}")
    else()
      message(FATAL_ERROR "fixture-index: unsafe-numeric: result_channel.register_index")
    endif()

    gbl_json_type(dispute_type "${row_doc}" expected dispute)
    set(dispute "")
    if("${dispute_type}" STREQUAL "STRING")
      gbl_json_string(dispute "${row_doc}" expected dispute)
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

    gbl_json_length(use_count "${row_doc}" permitted_use)
    string(APPEND contents
      "static const char *const gbl_fixture_uses_${row_index}[] = {\n")
    if(use_count GREATER 0)
      math(EXPR use_last "${use_count}-1")
      foreach(use_index RANGE ${use_last})
        gbl_json_string(use "${row_doc}" permitted_use
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
      "    \"five-word-v1-final-marker\", \"${c_channel_kind}\",\n"
      "    \"${c_channel_commit}\", \"${c_channel_path}\",\n"
      "    ${c_channel_address}, \"${c_channel_instruction_set}\", ${c_channel_register_index}u\n"
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
  set(${out} "${contents}" PARENT_SCOPE)
endfunction()
