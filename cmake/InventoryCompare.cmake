# gbl_inventory_compare(<registered-list-var> <inventory-file>)
#
# <registered-list-var> names a list of registered-test lines, each
#   "<name> <label,label>[ skip-ok]"
# <inventory-file> is the sorted, checked-in tests/inventory/<lane>.txt in the
# same line format. '#' lines and blank lines are ignored.
#
# Fails with a stable reason line:
#   inventory: unsorted             listed lines are not in byte order (checked first)
#   inventory: configure-mismatch   registered and listed sets differ
#
# Script mode (used by the negative controls):
#   cmake -DREGISTERED_FILE=<file> -DINVENTORY=<file> -P cmake/InventoryCompare.cmake
# <file> for REGISTERED_FILE holds one registered line per file line.

# Script mode starts with every policy OLD (IN_LIST is a plain argument under
# CMP0057 OLD on the 3.25 floor); pin the floor policy set. A function keeps the
# policies in force where it is defined, so this precedes the definition.
cmake_minimum_required(VERSION 3.25)

function(gbl_inventory_compare registered_var inventory_file)
  if(NOT EXISTS "${inventory_file}")
    message(FATAL_ERROR "inventory: configure-mismatch: inventory file not found: ${inventory_file}")
  endif()
  file(STRINGS "${inventory_file}" listed REGEX "^[^#]")

  set(sorted_copy "${listed}")
  list(SORT sorted_copy)
  if(NOT "${sorted_copy}" STREQUAL "${listed}")
    message(FATAL_ERROR "inventory: unsorted: ${inventory_file}")
  endif()

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

# Script-mode entry (not triggered when this file is include()d elsewhere).
if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  foreach(var REGISTERED_FILE INVENTORY)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
      message(FATAL_ERROR "inventory-compare: -D${var}= is required")
    endif()
  endforeach()
  file(STRINGS "${REGISTERED_FILE}" script_registered)
  gbl_inventory_compare(script_registered "${INVENTORY}")
endif()
