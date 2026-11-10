# gbl_inventory_compare(<registered-list-var> <inventory-file>)
#
# <registered-list-var> names a list of registered-test lines, each
#   "<name> <label,label>[ skip-ok]"
# <inventory-file> is the sorted, checked-in tests/inventory/<lane>.txt in the
# same line format. '#' lines and blank lines are ignored.
#
# Fails with a stable reason line:
#   inventory: configure-mismatch   registered and listed sets differ
function(gbl_inventory_compare registered_var inventory_file)
  if(NOT EXISTS "${inventory_file}")
    message(FATAL_ERROR "inventory: configure-mismatch: inventory file not found: ${inventory_file}")
  endif()
  file(STRINGS "${inventory_file}" listed REGEX "^[^#]")

  set(registered "${${registered_var}}")
  list(SORT registered)

  set(only_registered "")
  foreach(line IN LISTS registered)
    if(NOT line IN_LIST listed)
      list(APPEND only_registered "${line}")
    endif()
  endforeach()
  set(only_listed "")
  foreach(line IN LISTS listed)
    if(NOT line IN_LIST registered)
      list(APPEND only_listed "${line}")
    endif()
  endforeach()

  if(only_registered OR only_listed)
    set(report "")
    foreach(line IN LISTS only_registered)
      string(APPEND report "\n  registered but not listed: ${line}")
    endforeach()
    foreach(line IN LISTS only_listed)
      string(APPEND report "\n  listed but not registered: ${line}")
    endforeach()
    message(FATAL_ERROR "inventory: configure-mismatch (${inventory_file})${report}")
  endif()
endfunction()
