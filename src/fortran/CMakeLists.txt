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
    quo-vadisf
    OBJECT
    quo-vadisf.f90
)

set_property(
    TARGET
      quo-vadisf
    PROPERTY
      POSITION_INDEPENDENT_CODE ON
)
endif(QV_FORTRAN_HAPPY)

################################################################################
if(QV_FORTRAN_HAPPY AND MPI_Fortran_FOUND)
add_library(
    quo_vadis_mpif
    SHARED
    quo-vadis-mpif.f90
)

target_include_directories(
    quo_vadis_mpif
    PUBLIC
      ${MPI_Fortran_MODULE_DIR}
)

target_link_libraries(
    quo_vadis_mpif
    quo-vadisf
    quo-vadis-mpi
    ${MPI_Fortran_LIBRARIES}
    ${MPI_Fortran_LINK_FLAGS}
)

install(
    TARGETS
      quo_vadis_mpif
    DESTINATION
      lib
)

install(
    FILES
      "${CMAKE_Fortran_MODULE_DIRECTORY}/quo_vadis_mpif.mod"
    DESTINATION
      lib
)
endif(QV_FORTRAN_HAPPY AND MPI_Fortran_FOUND)

# vim: ts=4 sts=4 sw=4 expandtab
