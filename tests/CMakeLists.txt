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

# TODO(skg) Add valgrind tests.
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
# TODO(skg) Link to MPI-specific library.
if (MPI_FOUND)
add_executable(
    test-mpi
    test-mpi.c
)

target_link_libraries(
    test-mpi
    quo-vadis
)

# TODO(skg)
#add_test(
#    mpi
#    test-mpi
#)

add_executable(
    test-init-mpi
    test-init-mpi.c
)

target_link_libraries(
    test-init-mpi
    quo-vadis
)

add_executable(
    test-scopes-mpi
    test-scopes-mpi.c
)

target_link_libraries(
    test-scopes-mpi
    quo-vadis
)
endif(MPI_FOUND)

################################################################################
################################################################################
add_executable(
    test-pmi
    test-pmi.c
)

target_link_libraries(
    test-pmi
    quo-vadis
)

# TODO(skg)
#add_test(
#    pmi
#    test-pmi
#)

################################################################################
################################################################################
add_executable(
    test-rmi
    test-rmi.c
)

target_link_libraries(
    test-rmi
    quo-vadis
)

# TODO(skg) Cleanup after failure: test-rmi can still be running on failure.
add_test(
    NAME
      rmi
    COMMAND
      bash -c "export URL=\"tcp://127.0.0.1:55995\"     && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -s &)  && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c &)  && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c &)  && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c &)  && \
       ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c     && \
      sleep 1                                           && \
      killall test-rmi"
)

################################################################################
################################################################################
add_executable(
    test-scopes
    test-scopes.c
)

target_link_libraries(
    test-scopes
    quo-vadis
)

add_test(
    scopes
    test-scopes
)

################################################################################
################################################################################
# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    test-hwloc
    test-rmi
    test-scopes
    PROPERTIES LINKER_LANGUAGE C
)

# Set test timeout
set_tests_properties(
    hwloc
    rmi
    scopes
    PROPERTIES TIMEOUT 10
)

# vim: ts=4 sts=4 sw=4 expandtab
