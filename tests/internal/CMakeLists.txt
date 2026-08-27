# -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*-
#
# Copyright (c) 2020-2026 Triad National Security, LLC
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
    test-hwloc.cc
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
    test-bbuff
    test-bbuff.cc
)

target_link_libraries(
    test-bbuff
    quo-vadis
)

add_test(
    bbuff
    test-bbuff
)

################################################################################
################################################################################
add_executable(
    test-map
    test-map.cc
)

target_link_libraries(
    test-map
    quo-vadis
)

add_test(
    map
    test-map
)

################################################################################
################################################################################
# Shared verification helpers for the qv_split() tests. These reach into the
# internal scope representation and reuse the library's own mapping primitives,
# so (unlike common-test-utils) they intentionally depend on internal QV
# headers. The process, thread, and MPI split tests all link against this and
# share its scope-kind-agnostic driver (ctu_split::run_all_split_tests).
add_library(
    common-split-utils
    common-split-utils.h
    common-split-utils.cc
)

target_link_libraries(
    common-split-utils
    PUBLIC
      common-test-utils
      quo-vadis
)

target_compile_options(
    common-split-utils
    PRIVATE
      "$<$<BOOL:${MPI_FOUND}>:-DCTU_HAS_MPI_SUPPORT>"
)

################################################################################
################################################################################
# C++ test that verifies the exact resource mapping (cpusets) of all
# qv_split combinations (automatic and explicit groupings) using internal APIs.
add_executable(
    test-process-all-split
    test-process-all-split.cc
)

target_link_libraries(
    test-process-all-split
    common-split-utils
    common-test-utils
    quo-vadis
)

# This test drives the public qv_split() API, which talks to quo-vadisd, so it
# must be wrapped by run-dtest.sh to bring up (and tear down) the daemon.
# run-dtest.sh requires QV_DAEMON to point to the quo-vadisd binary.
add_test(
    NAME
      process-all-split
    COMMAND
      bash -c "QV_DAEMON=$<TARGET_FILE:quo-vadisd> \
      ${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
      ${CMAKE_CURRENT_BINARY_DIR}/test-process-all-split"
)

set_tests_properties(
    process-all-split
    PROPERTIES
      TIMEOUT 60
      LABELS "process"
)

################################################################################
################################################################################
# Thread counterpart: the same qv_split verification battery, but the calling
# group is a set of thread subscopes produced by qv_thread_split. Reuses the
# shared driver via the thread backend.
add_executable(
    test-thread-all-split
    test-thread-all-split.cc
)

target_link_libraries(
    test-thread-all-split
    common-split-utils
    common-test-utils
    quo-vadis
)

add_test(
    NAME
      thread-all-split
    COMMAND
      bash -c "QV_DAEMON=$<TARGET_FILE:quo-vadisd> \
      ${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
      ${CMAKE_CURRENT_BINARY_DIR}/test-thread-all-split"
)

set_tests_properties(
    thread-all-split
    PROPERTIES
      TIMEOUT 60
      LABELS "thread"
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

################################################################################
################################################################################
add_executable(
    test-rmi-server
    test-rmi-server.cc
)

target_link_libraries(
    test-rmi-server
    quo-vadis
)

add_test(
    NAME
      rmi-server
    COMMAND
      test-rmi-server "tcp://127.0.0.1:55994"
)

################################################################################
################################################################################
if(MPI_FOUND)
    add_executable(
        test-mpi-install
        test-mpi-install.c
    )

    target_link_libraries(
        test-mpi-install
        ${MPI_C_LIBRARIES}
    )

    target_include_directories(
        test-mpi-install
        PRIVATE
          ${MPI_C_INCLUDE_DIRS}
    )

    add_test(
        NAME
          test-mpi-install
        COMMAND
          bash -c "\
          ${MPIEXEC_EXECUTABLE} --version && \
          ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 2 \
          ${CMAKE_CURRENT_BINARY_DIR}/test-mpi-install"
    )

    ############################################################################
    # C++ MPI counterpart of test-process-all-split: verifies the exact
    # resource mapping (cpusets) of all qv_split combinations across many tasks,
    # reusing the shared verification helpers in common-split-utils.
    add_executable(
        test-mpi-all-split
        test-mpi-all-split.cc
    )

    target_link_libraries(
        test-mpi-all-split
        common-split-utils
        common-test-utils
        quo-vadis
        quo-vadis-mpi
    )

    # This test drives the public qv_split() API, which talks to quo-vadisd, so
    # it must be wrapped by run-dtest.sh to bring up (and tear down) the daemon.
    add_test(
        NAME
          mpi-all-split
        COMMAND
          bash -c "QV_DAEMON=$<TARGET_FILE:quo-vadisd> \
          ${CMAKE_SOURCE_DIR}/tests/run-dtest.sh \
          ${MPIEXEC_EXECUTABLE} \
          ${MPIEXEC_NUMPROC_FLAG} \
          ${MPIEXEC_MAX_NUMPROCS} \
          ${CMAKE_CURRENT_BINARY_DIR}/test-mpi-all-split"
    )

    set_tests_properties(
        mpi-all-split
        PROPERTIES
          TIMEOUT 60
          LABELS "mpi"
    )
endif()

################################################################################
################################################################################
# Set core test properties.
set_tests_properties(
    hwloc
    rmi
    rmi-server
    map
    bbuff
    PROPERTIES
      TIMEOUT 60
      LABELS "core"
)

# vim: ts=4 sts=4 sw=4 expandtab
