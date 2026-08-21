# -*- Mode: CMake; c-basic-offset:4; indent-tabs-mode:nil -*-
#
# Copyright (c) 2020-2026 Triad National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

################################################################################
# Micro-benchmark suite for the quo-vadis public APIs.
#
# The shared driver (qvb-common.c) benchmarks every scope-kind-agnostic public
# function once; each backend (process/thread/mpi) is a thin binary that reuses
# it and adds only its kind-specific entry points.
################################################################################

# Process-scope benchmarks (always built).
add_executable(
    qvb-process
    qvb.h
    qvb-common.cc
    qvb-process.cc
)

target_link_libraries(
    qvb-process
    quo-vadis
)

add_test(
    NAME
      bench-process
    COMMAND
      bash -c "${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
      ${CMAKE_CURRENT_BINARY_DIR}/qvb-process"
    WORKING_DIRECTORY
      ${CMAKE_BINARY_DIR}/tests
)

set_tests_properties(
    bench-process
    PROPERTIES
      TIMEOUT 120
      LABELS "benchmark;process"
)

# Thread-scope benchmarks (always built; uses pthreads).
add_executable(
    qvb-thread
    qvb.h
    qvb-common.cc
    qvb-thread.cc
)

target_link_libraries(
    qvb-thread
    quo-vadis
    Threads::Threads
)

add_test(
    NAME
      bench-thread
    COMMAND
      bash -c "${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
      ${CMAKE_CURRENT_BINARY_DIR}/qvb-thread"
    WORKING_DIRECTORY
      ${CMAKE_BINARY_DIR}/tests
)

set_tests_properties(
    bench-thread
    PROPERTIES
      TIMEOUT 120
      LABELS "benchmark;thread"
)

################################################################################
# MPI-scope benchmarks (only if MPI is available).
################################################################################
if(MPI_FOUND)
    add_executable(
        qvb-mpi
        qvb.h
        qvb-common.cc
        qvb-mpi.cc
    )

    target_link_libraries(
        qvb-mpi
        quo-vadis
        quo-vadis-mpi
    )

    add_test(
        NAME
          bench-mpi
        COMMAND
          bash -c "${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
          ${MPIEXEC_EXECUTABLE} \
          ${MPIEXEC_NUMPROC_FLAG} \
          ${MPIEXEC_MAX_NUMPROCS} \
          ${CMAKE_CURRENT_BINARY_DIR}/qvb-mpi"
        WORKING_DIRECTORY
          ${CMAKE_BINARY_DIR}/tests
    )

    set_tests_properties(
        bench-mpi
        PROPERTIES
          TIMEOUT 120
          LABELS "benchmark;mpi"
    )
endif()

# vim: ts=4 sts=4 sw=4 expandtab
