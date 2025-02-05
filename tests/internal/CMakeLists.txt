# -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*-
#
# Copyright (c) 2020-2025 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

include_directories(
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tests>
)

################################################################################
################################################################################
add_executable(
    test-hwloc
    test-hwloc.c
)

target_link_libraries(
    test-hwloc
    quo-vadis
)

add_test(
    hwloc
    test-hwloc
)

################################################################################
################################################################################
add_executable(
    test-rmi
    test-rmi.cc
)

target_link_libraries(
    test-rmi
    quo-vadis
)

add_test(
    NAME
      rmi
    COMMAND
      bash -c "export URL=\"tcp://127.0.0.1:55995\" && \
      ( ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -s & ) && \
      ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c && \
      ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c && \
      ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -cc"
)

# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    test-hwloc
    PROPERTIES LINKER_LANGUAGE C
)

################################################################################
################################################################################
# Set core test properties.
set_tests_properties(
    hwloc
    rmi
    PROPERTIES
      TIMEOUT 60
      LABELS "core"
)

# vim: ts=4 sts=4 sw=4 expandtab
