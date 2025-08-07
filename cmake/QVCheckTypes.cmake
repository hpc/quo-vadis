#
# Copyright (c) 2022-2025 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# Checks size of data types.
include(CheckTypeSize)

# We only support 64-bit architectures and builds. This is primarily because
# hwloc relies on a large virtual address space for its shared-memory
# sub-system.
message(CHECK_START "Verifying that this is a 64-bit+ build")
if(CMAKE_SIZEOF_VOID_P LESS_EQUAL 4)
    math(EXPR QV_BUILD_BITS "${CMAKE_SIZEOF_VOID_P} * 8" OUTPUT_FORMAT DECIMAL)
    message(FATAL_ERROR "${QV_BUILD_BITS}-bit builds are not supported")
else()
    message(CHECK_PASS "verified")
endif()

check_type_size(int QVI_SIZEOF_INT)
check_type_size(pid_t QVI_SIZEOF_PID_T)

# We assume sizeof(int) and sizeof(pid_t) are the same.
message(CHECK_START "Verifying int and pid_t are same size")
if(NOT QVI_SIZEOF_INT EQUAL QVI_SIZEOF_PID_T)
    message(FATAL_ERROR "int and pid_t are not the same size")
else()
    message(CHECK_PASS "verified")
endif()

# vim: ts=4 sts=4 sw=4 expandtab
