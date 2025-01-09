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

################################################################################
################################################################################
add_executable(
    test-process-scopes
    test-process-scopes.c
)

target_link_libraries(
    test-process-scopes
    quo-vadis
)

add_test(
    NAME
      process-scopes
    COMMAND
      bash -c "export QV_PORT=\"55999\" && \
      ( ${CMAKE_SOURCE_DIR}/tests/exec-timeout.sh ../src/quo-vadisd 5 & ) && \
        ${CMAKE_CURRENT_BINARY_DIR}/test-process-scopes"
)

# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    test-process-scopes
    PROPERTIES LINKER_LANGUAGE C
)

set_tests_properties(
    process-scopes
    PROPERTIES
      TIMEOUT 60
    LABELS "process"
)

################################################################################
################################################################################
if (OPENMP_FOUND)
    add_executable(
        test-omp
        test-omp.c
    )

    target_link_libraries(
        test-omp
        quo-vadis
        quo-vadis-omp
        OpenMP::OpenMP_C
    )

    add_test(
        NAME
          omp
        COMMAND
          bash -c "export QV_PORT=\"55999\" && \
          ( ${CMAKE_SOURCE_DIR}/tests/exec-timeout.sh ../src/quo-vadisd 5 & ) && \
            ${CMAKE_CURRENT_BINARY_DIR}/test-omp"
    )

    # Set omp test properties.
    set_tests_properties(
        omp
        PROPERTIES
          TIMEOUT 60
          LABELS "omp"
    )
endif()

################################################################################
################################################################################
if(MPI_FOUND)
    add_executable(
        test-mpi-init
        common-test-utils.h
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
        test-mpi-scopes-affinity-preserving
        test-mpi-scopes-affinity-preserving.c
    )

    target_link_libraries(
        test-mpi-scopes-affinity-preserving
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
        test-progress-thread
        test-progress-thread.c
    )

    target_link_libraries(
        test-progress-thread
        quo-vadis
        quo-vadis-mpi
    )

    add_executable(
        test-pthread-split
        common-test-utils.h
        test-pthread-split.c
    )

    target_link_libraries(
        test-pthread-split
        quo-vadis
        quo-vadis-mpi
    )

    add_executable(
        test-mpi-scope-create
        test-mpi-scope-create.c
    )

    target_link_libraries(
        test-mpi-scope-create
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

    # Use the C linker to test for C/C++ linkage problems.
    set_target_properties(
        test-mpi-init
        test-mpi-api
        test-mpi-scopes
        test-mpi-scopes-affinity-preserving
        test-mpi-phases
        test-pthread-split
        test-progress-thread
        test-mpi-scope-create
        test-mpi-getdev
        PROPERTIES LINKER_LANGUAGE C
    )

    # TODO(skg) Automate adding MPI tests.
    add_test(
        NAME
          mpi-api
        COMMAND
          bash -c "export QV_PORT=\"55999\" && \
          ( ${CMAKE_SOURCE_DIR}/tests/exec-timeout.sh ../src/quo-vadisd 5 & ) && \
            ${MPIEXEC_EXECUTABLE} \
            ${MPIEXEC_NUMPROC_FLAG} \
            ${MPIEXEC_MAX_NUMPROCS} \
            ${CMAKE_CURRENT_BINARY_DIR}/test-mpi-api"
    )

    add_test(
        NAME
          mpi-scopes
        COMMAND
          bash -c "export QV_PORT=\"55999\" && \
          ( ${CMAKE_SOURCE_DIR}/tests/exec-timeout.sh ../src/quo-vadisd 5 & ) && \
            ${MPIEXEC_EXECUTABLE} \
            ${MPIEXEC_NUMPROC_FLAG} \
            ${MPIEXEC_MAX_NUMPROCS} \
            ${CMAKE_CURRENT_BINARY_DIR}/test-mpi-scopes"
    )

    # Set mpi test properties.
    set_tests_properties(
        mpi-api
        mpi-scopes
        PROPERTIES
          TIMEOUT 60
          LABELS "mpi"
    )

############################# HYBRID TESTS (PThreads) ##########################
################################################################################


############################# HYBRID TESTS (OpenMP) ############################
################################################################################
    if(OPENMP_FOUND)
        add_executable(
            test-mpi-threads
            test-mpi-threads.c
        )

        target_link_libraries(
            test-mpi-threads
            quo-vadis
            quo-vadis-mpi
            OpenMP::OpenMP_C
        )

        set_target_properties(
            test-mpi-threads
            PROPERTIES LINKER_LANGUAGE C
        )
    endif()
endif()

################################################################################
################################################################################
if(QV_FORTRAN_HAPPY)
    add_executable(
        test-process-fortapi
        test-process-fortapi.f90
    )

    target_link_libraries(
        test-process-fortapi
        quo-vadis-processf
    )

    set_target_properties(
        test-process-fortapi
        PROPERTIES LINKER_LANGUAGE Fortran
    )
    add_test(
        NAME
          process-fortapi
        COMMAND
          bash -c "export QV_PORT=\"55999\" && \
          ( ${CMAKE_SOURCE_DIR}/tests/exec-timeout.sh ../src/quo-vadisd 5 & ) && \
          ${CMAKE_CURRENT_BINARY_DIR}/test-process-fortapi"
    )

    # Set fortran test properties.
    set_tests_properties(
        process-fortapi
        PROPERTIES
          TIMEOUT 60
          LABELS "process-fortran"
    )

    if(MPI_Fortran_FOUND)
        add_executable(
            test-mpi-fortapi
            test-mpi-fortapi.f90
        )

        target_link_libraries(
            test-mpi-fortapi
            quo-vadis-mpif
        )

        set_target_properties(
            test-mpi-fortapi
            PROPERTIES LINKER_LANGUAGE Fortran
        )
    endif()
endif()

# vim: ts=4 sts=4 sw=4 expandtab
