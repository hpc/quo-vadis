#
# Copyright (c) 2022-2023 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

include(CheckLanguage)

option(
    QV_DISABLE_FORTRAN_SUPPORT
    "Unconditionally disable Fortran support"
    OFF
)

message(CHECK_START "Determining desired Fortran support level")

set(QV_FORTRAN_HAPPY FALSE)
if(NOT QV_DISABLE_FORTRAN_SUPPORT)
    message(CHECK_PASS "enabled")

    check_language(Fortran)
    if(NOT CMAKE_Fortran_COMPILER)
        message(STATUS "No Fortran support detected")
        return()
    endif()
    # Else we have the right bits to see if we have Fortran support.
    enable_language(Fortran)
    # Make sure that we can support Fortran C interfaces.
    include(FortranCInterface)
    FortranCInterface_VERIFY()
    # Make sure we found a Fortran compiler.
    if(NOT CMAKE_Fortran_COMPILER STREQUAL "")
        set(QV_FORTRAN_HAPPY TRUE)
        set(
            CMAKE_Fortran_MODULE_DIRECTORY
            "${CMAKE_BINARY_DIR}/src/fortran"
        )
    endif()
else()
    message(CHECK_PASS "disabled")
endif(NOT QV_DISABLE_FORTRAN_SUPPORT)

# vim: ts=4 sts=4 sw=4 expandtab
