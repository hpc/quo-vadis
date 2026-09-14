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
# it and adds only its kind-specific entry points. All are compiled as C.
################################################################################

# Process-scope benchmarks (always built).
add_executable(
    qvb-process
    qvb.h
    qvb-common.c
    qvb-process.c
)

target_link_libraries(
    qvb-process
    common-test-utils
    quo-vadis
)

target_include_directories(
    qvb-process
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    qvb-process
    PROPERTIES LINKER_LANGUAGE C
)

add_test(
    NAME
      bench-process
    COMMAND
      bash -c "QV_DAEMON=$<TARGET_FILE:quo-vadisd> \
      ${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
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
    qvb-common.c
    qvb-thread.c
)

target_link_libraries(
    qvb-thread
    common-test-utils
    quo-vadis
    Threads::Threads
)

target_include_directories(
    qvb-thread
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    qvb-thread
    PROPERTIES LINKER_LANGUAGE C
)

add_test(
    NAME
      bench-thread
    COMMAND
      bash -c "QV_DAEMON=$<TARGET_FILE:quo-vadisd> \
      ${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
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
        qvb-common.c
        qvb-mpi.c
    )

    target_link_libraries(
        qvb-mpi
        common-test-utils
        quo-vadis
        quo-vadis-mpi
    )

    target_include_directories(
        qvb-mpi
        PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    # Use the C linker to test for C/C++ linkage problems.
    set_target_properties(
        qvb-mpi
        PROPERTIES LINKER_LANGUAGE C
    )

    add_test(
        NAME
          bench-mpi
        COMMAND
          bash -c "QV_DAEMON=$<TARGET_FILE:quo-vadisd> \
          ${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
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

    ############################################################################
    # MPI message-rate (throughput vs. concurrency) benchmark. Each MPI rank is
    # an independent client of quo-vadisd, so the number of concurrent clients
    # is the launcher-chosen communicator size (`mpiexec -n N`). It measures the
    # effective public-API message rate a user should expect; see
    # docs/mpi-msg-rate.md. Written in C++ (hence no LINKER_LANGUAGE C override).
    add_executable(
        qvb-mpi-msg-rate
        qvb.h
        qvb-mpi-msg-rate.cc
    )

    target_link_libraries(
        qvb-mpi-msg-rate
        common-test-utils
        quo-vadis
        quo-vadis-mpi
        m
    )

    target_include_directories(
        qvb-mpi-msg-rate
        PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    # Smoke test: a short, fixed-count run that only verifies the benchmark
    # builds, runs, and tears the daemon down cleanly. Publication-grade sweeps
    # are run manually (see docs/mpi-msg-rate.md).
    add_test(
        NAME
          bench-mpi-msg-rate
        COMMAND
          bash -c "QV_DAEMON=$<TARGET_FILE:quo-vadisd> \
          ${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
          ${MPIEXEC_EXECUTABLE} \
          ${MPIEXEC_NUMPROC_FLAG} \
          ${MPIEXEC_MAX_NUMPROCS} \
          ${CMAKE_CURRENT_BINARY_DIR}/qvb-mpi-msg-rate \
          --requests 200 --warmup 0 --trials 2"
        WORKING_DIRECTORY
          ${CMAKE_BINARY_DIR}/tests
    )

    set_tests_properties(
        bench-mpi-msg-rate
        PROPERTIES
          TIMEOUT 120
          LABELS "benchmark;mpi"
    )
endif()

# vim: ts=4 sts=4 sw=4 expandtab
