#
# Copyright (c) 2020-2023 Triad National Security, LLC
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
    qvi-test-common.h
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
      bash -c "export URL=\"tcp://127.0.0.1:55995\" && \
      ${CMAKE_CURRENT_BINARY_DIR}/test-rmi-server $URL"
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
      ( ${CMAKE_SOURCE_DIR}/tests/exec-timeout.sh \
        \"${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -s\" 5 & ) && \
      ( ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c & ) && \
      ( ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c & ) && \
      ( ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c & ) && \
        ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c"
)

################################################################################
################################################################################
add_executable(
    test-process-scopes
    qvi-test-common.h
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

################################################################################
################################################################################
if (OPENMP_FOUND)
    add_executable(
        test-threads
        test-threads.c
    )

    target_link_libraries(
        test-threads
        quo-vadis
        OpenMP::OpenMP_C
    )

    add_executable(
        test-openmp
        test-openmp.c
    )

    target_link_libraries(
        test-openmp
        OpenMP::OpenMP_C
    )

    add_test(
        NAME
          threads
        COMMAND
          bash -c "export QV_PORT=\"55999\" && \
          ( ${CMAKE_SOURCE_DIR}/tests/exec-timeout.sh ../src/quo-vadisd 5 & ) && \
            ${CMAKE_CURRENT_BINARY_DIR}/test-threads"
    )

    # Set threads test properties.
    # TODO(skg) ASAN detects a leak sometimes, so remove from core for now.
    set_tests_properties(
        threads
        PROPERTIES
          TIMEOUT 60
          LABELS "threads"
    )
endif()

################################################################################
################################################################################
if(MPI_FOUND)
    add_executable(
        test-mpi
        qvi-test-common.h
        test-mpi.c
    )

    target_link_libraries(
        test-mpi
        quo-vadis-mpi
    )

    add_executable(
        test-mpi-init
        qvi-test-common.h
        test-mpi-init.c
    )

    target_link_libraries(
        test-mpi-init
        quo-vadis-mpi
    )

    add_executable(
        test-mpi-api
        qvi-test-common.h
        test-mpi-api.c
    )

    target_link_libraries(
        test-mpi-api
        quo-vadis-mpi
    )

    add_executable(
        test-mpi-scopes
        qvi-test-common.h
        test-mpi-scopes.c
    )

    target_link_libraries(
        test-mpi-scopes
        quo-vadis-mpi
    )

    add_executable(
        test-mpi-scopes-affinity-preserving
        qvi-test-common.h
        test-mpi-scopes-affinity-preserving.c
    )

    target_link_libraries(
        test-mpi-scopes-affinity-preserving
        quo-vadis-mpi
    )

    add_executable(
        test-mpi-phases
        qvi-test-common.h
        test-mpi-phases.c
    )

    target_link_libraries(
        test-mpi-phases
        quo-vadis-mpi
    )

    add_executable(
        test-mpi-scope-create
        qvi-test-common.h
        test-mpi-scope-create.c
    )

    target_link_libraries(
        test-mpi-scope-create
        quo-vadis-mpi
    )

    add_executable(
        test-mpi-getdev
        qvi-test-common.h
        test-mpi-getdev.c
    )

    target_link_libraries(
        test-mpi-getdev
        quo-vadis-mpi
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

    # Set mpi test properties.
    set_tests_properties(
        mpi-api
        PROPERTIES
          TIMEOUT 60
          LABELS "mpi"
    )

############################# HYBRID TESTS (PThreads) ##########################
################################################################################
    add_executable(
         test-mpi-pthreads-layout
         qvi-test-common.h
         test-mpi-pthreads-layout.c
    )


    target_link_libraries(
         test-mpi-pthreads-layout
         quo-vadis
         quo-vadis-mpi
    )

############################# HYBRID TESTS (OpenMP) ############################
################################################################################
    if(OPENMP_FOUND)
        add_executable(
            test-mpi-threads
            qvi-test-common.h
            test-mpi-threads.c
        )

        target_link_libraries(
            test-mpi-threads
            quo-vadis
            quo-vadis-mpi
            OpenMP::OpenMP_C
        )

        add_executable(
            test-mpi-threads-layout
            qvi-test-common.h
            test-mpi-threads-layout.c
        )


        target_link_libraries(
            test-mpi-threads-layout
            quo-vadis
            quo-vadis-mpi
            OpenMP::OpenMP_C
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
          LABELS "fortran"
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

################################################################################
################################################################################
# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    test-hwloc
    PROPERTIES LINKER_LANGUAGE C
)

if(MPI_FOUND)
    set_target_properties(
        test-mpi
        test-mpi-init
        test-mpi-api
        test-mpi-getdev
        test-mpi-scopes
	test-mpi-pthreads-layout
        PROPERTIES LINKER_LANGUAGE C
    )

    if (OPENMP_FOUND)
        set_target_properties(
            test-mpi-threads
            PROPERTIES LINKER_LANGUAGE C
        )
        set_target_properties(
            test-mpi-threads-layout
            PROPERTIES LINKER_LANGUAGE C
        )
    endif()
endif()

# Set core test properties.
# TODO(skg) Add rmi and process-scopes
set_tests_properties(
    hwloc
    rmi-server
    rmi
    PROPERTIES
      TIMEOUT 60
      LABELS "core"
)

# vim: ts=4 sts=4 sw=4 expandtab
