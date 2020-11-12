#
# Copyright (c)      2020 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c)      2020 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#


find_path(
    PMIX_INCLUDE_DIR
    pmix.h
)

find_library(
    PMIX_LIBRARY
    NAMES
      pmix
)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set PMIX_FOUND to TRUE if all
# listed variables are TRUE
find_package_handle_standard_args(
    PMIx
    DEFAULT_MSG
    PMIX_LIBRARY PMIX_INCLUDE_DIR
)

mark_as_advanced(
    PMIX_INCLUDE_DIR
    PMIX_LIBRARY
)

set(PMIX_LIBRARIES ${PMIX_LIBRARY} )
set(PMIX_INCLUDE_DIRS ${PMIX_INCLUDE_DIR} )

add_library(
    pmix
    STATIC IMPORTED GLOBAL
)

set_target_properties(
    pmix
    PROPERTIES
      IMPORTED_LOCATION ${PMIX_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${PMIX_INCLUDE_DIRS}
)

# vim: ts=4 sts=4 sw=4 expandtab
