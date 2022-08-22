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
    test-threads
    test-threads.c
)

if (OPENMP_FOUND)
target_link_libraries(
    test-threads
    quo-vadis
    OpenMP::OpenMP_C
)
else()
 target_link_libraries(
    test-threads
    quo-vadis
)
endif()

add_executable(
    test-hwloc
    test-hwloc.c
)

add_test(
    threads
    test-threads
)
  
if (OPENMP_FOUND)
target_link_libraries(
    test-hwloc
    quo-vadis
    OpenMP::OpenMP_C
)
else()
target_link_libraries(
    test-hwloc
    quo-vadis
)
endif()

add_test(
    hwloc
    test-hwloc
)

add_executable(
    test-process-scopes
    test-process-scopes.c
)

if (OPENMP_FOUND)
target_link_libraries(
    test-process-scopes
    quo-vadis
    OpenMP::OpenMP_C
)
else()
target_link_libraries(
    test-process-scopes
    quo-vadis
)
endif()

add_test(
    NAME
      process-context
    COMMAND
      bash -c "export QV_PORT=\"55999\"                   && \
      ( sleep 5 && killall -9 quo-vadisd              & ) && \
      ( ${CMAKE_CURRENT_BINARY_DIR}/../src/quo-vadisd & ) && \
        ${CMAKE_CURRENT_BINARY_DIR}/test-process-scopes"
)

################################################################################
################################################################################
# TODO(skg) Add MPI tests.
if(MPI_FOUND)
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
################" HYBRID TESTS #####################
####################################################  
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
endif()  
endif()

if (OPENMP_FOUND)
    add_executable(
        test-openmp
        test-openmp.c
    )
    target_link_libraries(
        test-openmp
        OpenMP::OpenMP_C
    )
endif()

################################################################################
################################################################################
if(QV_FORTRAN_HAPPY)
    add_executable(
        test-process-fortapi
        test-process-fortapi.f90
    )

   if(OPENMP_FOUND)
      target_link_libraries(
         test-process-fortapi
         quo-vadis-processf
         OpenMP::OpenMP_Fortran
      )
   else()
      target_link_libraries(
        test-process-fortapi
        quo-vadis-processf
      )
    endif()

    set_target_properties(
        test-process-fortapi
        PROPERTIES LINKER_LANGUAGE Fortran
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
add_executable(
    test-rmi
    test-rmi.cc
)

if (OPENMP_FOUND)
target_link_libraries(
    test-rmi
    quo-vadis
    OpenMP::OpenMP_C
)
else()
target_link_libraries(
    test-rmi
    quo-vadis
)
endif()

add_test(
    NAME
      rmi
    COMMAND
      bash -c "export URL=\"tcp://127.0.0.1:55995\"      && \
      ( sleep 5 && killall -9 test-rmi               & ) && \
      ( ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -s & ) && \
      ( ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c & ) && \
      ( ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c & ) && \
      ( ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c & ) && \
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

if(MPI_FOUND)
set_target_properties(
    test-mpi
    test-mpi-init
    test-mpi-api
    test-mpi-getdev
    test-mpi-scopes
    PROPERTIES LINKER_LANGUAGE C
)

if (OPENMP_FOUND)
set_target_properties(
    test-mpi-threads
    PROPERTIES LINKER_LANGUAGE C
)
endif()
endif()

# Set test timeout
set_tests_properties(
    threads
    hwloc
    rmi
    scopes
    process-context
    PROPERTIES TIMEOUT 10
)

# vim: ts=4 sts=4 sw=4 expandtab
