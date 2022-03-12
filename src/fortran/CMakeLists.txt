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
    quo_vadisf
    quo-vadisf.f90
)

target_link_libraries(
    quo_vadisf
    quo-vadis
)

install(TARGETS quo_vadisf DESTINATION lib)
endif(QV_FORTRAN_HAPPY)

################################################################################
if(QV_FORTRAN_HAPPY AND MPI_Fortran_FOUND)
add_library(
    quo_vadis_mpif
    quo-vadis-mpif.f90
)

target_include_directories(
    quo_vadis_mpif
    PUBLIC
      ${MPI_Fortran_MODULE_DIR}
)

target_link_libraries(
    quo_vadis_mpif
    quo-vadis-mpi
    quo_vadisf
    ${MPI_Fortran_LIBRARIES}
    ${MPI_Fortran_LINK_FLAGS}
)

install(
    FILES
      "${CMAKE_Fortran_MODULE_DIRECTORY}/quo_vadis_mpif.mod"
      "${CMAKE_Fortran_MODULE_DIRECTORY}/quo_vadisf.mod"
    DESTINATION
      include
)

install(TARGETS quo_vadis_mpif DESTINATION lib)

# pkg-config
set (QV_PKG_LIBRARIES "-lquo_vadisf -lquo-vadis")
if (MPI_FOUND)
  set(QV_PKG_LIBRARIES "-lquo_vadis_mpif -lquo-vadis-mpi -lqvi-mpi -lquo_vadisf -lquo-vadis")
endif(MPI_FOUND)

configure_file(
    "${CMAKE_SOURCE_DIR}/pkg-config.pc.in"
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}f.pc"
    @ONLY
)
install(
    FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}f.pc"
    DESTINATION lib/pkgconfig
)

endif(QV_FORTRAN_HAPPY AND MPI_Fortran_FOUND)

# vim: ts=4 sts=4 sw=4 expandtab
