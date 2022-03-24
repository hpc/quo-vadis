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

################################################################################
if(QV_FORTRAN_HAPPY)
    add_library(
        quo-vadisf-core
        OBJECT
        quo-vadisf.f90
    )

    set_property(
        TARGET
          quo-vadisf-core
        PROPERTY
          POSITION_INDEPENDENT_CODE ON
    )

    install(
        FILES
          "${CMAKE_Fortran_MODULE_DIRECTORY}/quo_vadisf.mod"
        DESTINATION
          include
    )

    ############################################################################
    if(MPI_Fortran_FOUND)
        add_library(
            quo-vadis-mpif
            SHARED
            quo-vadis-mpif.f90
        )

        target_include_directories(
            quo-vadis-mpif
            PUBLIC
              ${MPI_Fortran_MODULE_DIR}
        )

        target_link_libraries(
            quo-vadis-mpif
            quo-vadisf-core
            quo-vadis-mpi
            ${MPI_Fortran_LIBRARIES}
            ${MPI_Fortran_LINK_FLAGS}
        )

        install(
            TARGETS
              quo-vadis-mpif
            DESTINATION
              lib
        )

        install(
            FILES
              "${CMAKE_Fortran_MODULE_DIRECTORY}/quo_vadis_mpif.mod"
            DESTINATION
              include
        )

        # pkg-config ###########################################################
        set(QV_PKG_LIBRARIES "-lquo-vadis-mpif")
        configure_file(
            "${CMAKE_SOURCE_DIR}/pkgconfig/pkg-config.pc.in"
            "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-mpif.pc"
            @ONLY
        )

        install(
            FILES
              "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-mpif.pc"
            DESTINATION
              lib/pkgconfig
        )
    else()
        add_library(
            quo-vadisf
            SHARED
        )

        target_include_directories(
            quo-vadisf
            PUBLIC
              $<TARGET_PROPERTY:quo-vadisf-core,INCLUDE_DIRECTORIES>
        )

        target_link_libraries(
            quo-vadisf
            quo-vadisf-core
            quo-vadis-core
            ${MPI_Fortran_LIBRARIES}
            ${MPI_Fortran_LINK_FLAGS}
        )

        install(
            TARGETS
              quo-vadisf
            DESTINATION
              lib
        )

        # pkg-config ###########################################################
        set(QV_PKG_LIBRARIES "-lquo-vadisf")
        configure_file(
            "${CMAKE_SOURCE_DIR}/pkgconfig/pkg-config.pc.in"
            "${CMAKE_BINARY_DIR}/${PROJECT_NAME}f.pc"
            @ONLY
        )

        install(
            FILES
              "${CMAKE_BINARY_DIR}/${PROJECT_NAME}f.pc"
            DESTINATION
              lib/pkgconfig
        )
    endif(MPI_Fortran_FOUND)
endif(QV_FORTRAN_HAPPY)

# vim: ts=4 sts=4 sw=4 expandtab
