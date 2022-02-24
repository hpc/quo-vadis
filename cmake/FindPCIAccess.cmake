#
# Copyright (c) 2020-2022 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#


find_path(
    PCIACCESS_INCLUDE_DIR
    pciaccess.h
)

find_library(
    PCIACCESS_LIBRARY
    NAMES
      pciaccess
)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set PCIACCESS_FOUND to TRUE if all
# listed variables are TRUE
find_package_handle_standard_args(
    PCIAccess
    DEFAULT_MSG
    PCIACCESS_LIBRARY PCIACCESS_INCLUDE_DIR
)

mark_as_advanced(
    PCIACCESS_INCLUDE_DIR
    PCIACCESS_LIBRARY
)

set(PCIACCESS_LIBRARIES ${PCIACCESS_LIBRARY})
set(PCIACCESS_INCLUDE_DIRS ${PCIACCESS_INCLUDE_DIR})

add_library(
    pciaccess
    STATIC IMPORTED GLOBAL
)

set_target_properties(
    pciaccess
    PROPERTIES
      IMPORTED_LOCATION ${PCIACCESS_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${PCIACCESS_INCLUDE_DIRS}
)

# vim: ts=4 sts=4 sw=4 expandtab
