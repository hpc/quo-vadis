#
# Copyright (c) 2020-2022 Triad National Security, LLC
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
    test-mpi-api
    test-mpi-api.c
)

target_link_libraries(
    test-mpi-api
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

add_executable(
    test-mpi-getdev
    test-mpi-getdev.c
)

target_link_libraries(
    test-mpi-getdev
    quo-vadis-mpi
)
endif(MPI_FOUND)

################################################################################
################################################################################
if(QV_FORTRAN_HAPPY AND MPI_Fortran_FOUND)
add_executable(
    test-mpi-qvfort
    test-mpi-qvfort.f90
)

target_link_libraries(
    test-mpi-qvfort
    quo-vadis-mpif
)

set_target_properties(
    test-mpi-qvfort
    PROPERTIES LINKER_LANGUAGE Fortran
)

endif(QV_FORTRAN_HAPPY AND MPI_Fortran_FOUND)

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
# Use the C linker to test for C/C++ linkage problems.
# TODO(skg) Add test-pmi
set_target_properties(
    test-hwloc
    test-scopes
    PROPERTIES LINKER_LANGUAGE C
)

if (MPI_FOUND)
set_target_properties(
    test-mpi
    test-mpi-init
    test-mpi-api
    test-mpi-getdev
    test-mpi-scopes
    PROPERTIES LINKER_LANGUAGE C
)
endif (MPI_FOUND)

# Set test timeout
set_tests_properties(
    hwloc
    rmi
    scopes
    PROPERTIES TIMEOUT 10
)

# vim: ts=4 sts=4 sw=4 expandtab
