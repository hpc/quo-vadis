#
# Copyright (c) 2020-2021 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
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
# TODO(skg) Add MPI tests.
if (MPI_FOUND)
add_executable(
    test-mpi
    test-mpi.c
)

target_link_libraries(
    test-mpi
    quo-vadis-mpi
)

add_executable(
    test-mpi-init
    test-mpi-init.c
)

target_link_libraries(
    test-mpi-init
    quo-vadis-mpi
)

add_executable(
    test-mpi-scopes
    test-mpi-scopes.c
)

target_link_libraries(
    test-mpi-scopes
    quo-vadis-mpi
)

add_executable(
    test-mpi-phases
    test-mpi-phases.c
)

target_link_libraries(
    test-mpi-phases
    quo-vadis-mpi
)
endif(MPI_FOUND)

################################################################################
################################################################################
if(MPI_Fortran_FOUND)
add_executable(
    test-qvfort
    qvfort.f90
)

target_link_libraries(
    test-qvfort
    quo_vadis_mpif
)
endif(MPI_Fortran_FOUND)

################################################################################
################################################################################
#add_executable(
#    test-pmi
#    test-pmi.c
#)

#target_link_libraries(
#    test-pmi
#    quo-vadis
#)

# TODO(skg)
#add_test(
#    pmi
#    test-pmi
#)

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
      bash -c "export URL=\"tcp://127.0.0.1:55995\"    && \
      (sleep 5 && killall -9 test-rmi               &) && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -s &) && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c &) && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c &) && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c &) && \
       ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c"
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
add_executable(
    test-mpi-getdev
    test-mpi-getdev.c
)

target_link_libraries(
    test-mpi-getdev
    quo-vadis-mpi
)

################################################################################
################################################################################
# Use the C linker to test for C/C++ linkage problems.
# TODO(skg) Add test-pmi
set_target_properties(
    test-hwloc
    test-mpi
    test-mpi-init
    test-mpi-getdev
    test-mpi-scopes
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
