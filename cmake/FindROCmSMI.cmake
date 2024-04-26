#
# Copyright (c) 2022-2024 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

find_path(
    ROCmSMI_HOME
    NAMES
      bin/rocm-smi
    HINTS
      /opt/rocm
)

find_path(
    ROCmSMI_INCLUDE_DIR
    NAMES
      rocm_smi/rocm_smi.h
    HINTS
      "${ROCmSMI_HOME}/rocm_smi/include"
)

find_library(
    ROCmSMI_LIBRARY
    NAMES
      rocm_smi64
    HINTS
      "${ROCmSMI_HOME}/lib"
)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set ROCmSMI_FOUND to TRUE if all
# listed variables are TRUE
find_package_handle_standard_args(
    ROCmSMI
    DEFAULT_MSG
    ROCmSMI_HOME ROCmSMI_INCLUDE_DIR ROCmSMI_LIBRARY
)

if (ROCmSMI_FOUND)
    # Force test compile of code every time.
    unset(QV_ROCmSMI_COMPILES CACHE)
    # Some versions of ROCm SMI are broken, so test here.
    try_compile(
        QV_ROCmSMI_COMPILES
        "${PROJECT_BINARY_DIR}/QVTryCompile"
        SOURCES
          "${CMAKE_SOURCE_DIR}/cmake/cmake-try-compile-rocm-smi.c"
        COMPILE_DEFINITIONS
          "-I${ROCmSMI_INCLUDE_DIR}"
        LINK_LIBRARIES
          "${ROCmSMI_LIBRARY}"
    )
    if (QV_ROCmSMI_COMPILES)
        message(STATUS "Found usable ROCm SMI: ${ROCmSMI_INCLUDE_DIR}")
    else()
        message(STATUS "Could not find a usable ROCm SMI")
        set(ROCmSMI_FOUND FALSE)
    endif()
endif()

mark_as_advanced(
    ROCmSMI_HOME
    ROCmSMI_INCLUDE_DIR
    ROCmSMI_LIBRARY
)

set(ROCmSMI_HOME "${ROCmSMI_HOME}")
set(ROCmSMI_LIBRARIES "${ROCmSMI_LIBRARY}")
set(ROCmSMI_INCLUDE_DIRS "${ROCmSMI_INCLUDE_DIR}")

add_library(
    ROCmSMI
    SHARED IMPORTED GLOBAL
)

set_target_properties(
    ROCmSMI
    PROPERTIES
      IMPORTED_LOCATION "${ROCmSMI_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${ROCmSMI_INCLUDE_DIRS}"
)

# vim: ts=4 sts=4 sw=4 expandtab
