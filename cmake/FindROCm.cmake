#
# Copyright (c)      2022 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

find_path(
    ROCM_HOME
    NAMES
      bin/rocm-smi
    HINTS
      /opt/rocm
)

find_path(
    ROCM_SMI_INCLUDE_DIR
    NAMES
      rocm_smi/rocm_smi.h
    HINTS
      "${ROCM_HOME}/rocm_smi/include"
)

find_path(
    ROCM_OPENCL_INCLUDE_DIR
    NAMES
      CL/cl.h
    HINTS
      "${ROCM_HOME}/opencl/include"
)

find_library(
    ROCM_LIBRARY
    NAMES
      rocm_smi64
    HINTS
      "${ROCM_HOME}/lib"
)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set ROCM_FOUND to TRUE if all
# listed variables are TRUE
find_package_handle_standard_args(
    ROCm
    DEFAULT_MSG
    ROCM_HOME ROCM_SMI_INCLUDE_DIR ROCM_OPENCL_INCLUDE_DIR ROCM_LIBRARY
)

mark_as_advanced(
    ROCM_HOME
    ROCM_SMI_INCLUDE_DIR
    ROCM_OPENCL_INCLUDE_DIR
    ROCM_LIBRARY
)

set(ROCM_HOME "${ROCM_HOME}")
set(ROCM_LIBRARIES "${ROCM_LIBRARY}")
set(ROCM_INCLUDE_DIRS "${ROCM_SMI_INCLUDE_DIR}")
list(APPEND ROCM_INCLUDE_DIRS "${ROCM_OPENCL_INCLUDE_DIR}")

add_library(
    ROCm
    SHARED IMPORTED GLOBAL
)

set_target_properties(
    ROCm
    PROPERTIES
      IMPORTED_LOCATION "${ROCM_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${ROCM_INCLUDE_DIRS}"
)

# vim: ts=4 sts=4 sw=4 expandtab
