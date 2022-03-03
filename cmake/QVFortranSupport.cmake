#
# Copyright (c)      2022 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

option(
    QV_DISABLE_FORTRAN_SUPPORT
    "Unconditionally disable Fortran support"
    OFF
)

message(CHECK_START "Determining desired Fortran support level")

set(QV_FORTRAN_HAPPY FALSE)
if(NOT QV_DISABLE_FORTRAN_SUPPORT)
    message(CHECK_PASS "enabled")
    # Enable Fortran support.
    enable_language(Fortran)
    # Make sure that we can support Fortran C interfaces.
    include(FortranCInterface)
    FortranCInterface_VERIFY()
    # Make sure we found a Fortran compiler.
    if(CMAKE_Fortran_COMPILER STREQUAL "")
        return()
    else()
        set(QV_FORTRAN_HAPPY TRUE)
    endif()
    set(CMAKE_Fortran_MODULE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src")
else()
    message(CHECK_PASS "disabled")
endif(NOT QV_DISABLE_FORTRAN_SUPPORT)

# vim: ts=4 sts=4 sw=4 expandtab
