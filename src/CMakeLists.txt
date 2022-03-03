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
################################################################################
add_library(
    qvi-core
    qvi-common.h
    qvi-macros.h
    qvi-log.h
    qvi-utils.h
    qvi-bbuff.h
    qvi-log.cc
    qvi-utils.cc
    qvi-bbuff.cc
)

target_compile_options(
    qvi-core
    PUBLIC
      -DSPDLOG_COMPILED_LIB
)

target_include_directories(
    qvi-core
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-core
    PUBLIC
      Threads::Threads
      spdlog
)

################################################################################
################################################################################
add_library(
    qvi-hwloc
    qvi-hwloc.h
    qvi-nvml.h
    qvi-rsmi.h
    qvi-rsmi.h
    qvi-hwloc.cc
    qvi-nvml.cc
    qvi-rsmi.cc
)

target_include_directories(
    qvi-hwloc
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-hwloc
    qvi-core
    hwloc
    pciaccess
)

if(CUDAToolkit_FOUND AND NOT QV_DISABLE_GPU_SUPPORT)
target_link_libraries(
    qvi-hwloc
    CUDA::cudart
    CUDA::nvml
)
endif()

if(ROCM_FOUND AND NOT QV_DISABLE_GPU_SUPPORT)
target_link_libraries(
    qvi-hwloc
    ROCm
)
endif()

################################################################################
################################################################################
add_library(
    qvi-rmi
    qvi-line.h
    qvi-hwpool.h
    qvi-rmi.h
    qvi-bbuff-rmi.h
    qvi-line.cc
    qvi-hwpool.cc
    qvi-rmi.cc
)

target_include_directories(
    qvi-rmi
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-rmi
    qvi-core
    qvi-hwloc
    zmq
)

################################################################################
################################################################################
if (MPI_FOUND)
add_library(
    qvi-mpi
    ../include/quo-vadis-mpi.h
    qvi-mpi.h
    qvi-group-mpi.h
    qvi-zgroup-mpi.h
    qvi-mpi.cc
    qvi-group-mpi.cc
    qvi-zgroup-mpi.cc
)

target_include_directories(
    qvi-mpi
    PUBLIC
      ${MPI_C_INCLUDE_DIRS}
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-mpi
    qvi-core
    ${MPI_C_LIBRARIES}
)
################################################################################
# TODO(skg) The flags need work.
if(MPI_Fortran_FOUND)
add_library(
    quo_vadis_mpif
    quo-vadis-mpif.f90
)

target_include_directories(
    quo_vadis_mpif
    PUBLIC
      ${MPI_Fortran_MODULE_DIR}
      $<INSTALL_INTERFACE:src>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
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
      quo_vadis_mpif.mod
    DESTINATION
      include
)
endif(MPI_Fortran_FOUND)
################################################################################
endif(MPI_FOUND)

################################################################################
################################################################################
add_library(
    quo-vadis
    ../include/quo-vadis.h
    qvi-context.h
    qvi-task.h
    qvi-devinfo.h
    qvi-group.h
    qvi-scope.h
    qvi-zgroup.h
    qvi-bind.h
    qvi-task.cc
    qvi-scope.cc
    qvi-bind.cc
    quo-vadis.cc
)

target_include_directories(
    quo-vadis
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    quo-vadis
    qvi-core
    qvi-hwloc
    qvi-rmi
)

################################################################################
################################################################################
if (MPI_FOUND)
add_library(
    quo-vadis-mpi
    ../include/quo-vadis-mpi.h
    quo-vadis-mpi.cc
)

target_include_directories(
    quo-vadis-mpi
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    quo-vadis-mpi
    qvi-mpi
    quo-vadis
)
install(TARGETS quo-vadis-mpi DESTINATION lib)
endif(MPI_FOUND)

################################################################################
################################################################################
add_executable(
    quo-vadisd
    quo-vadisd.cc
)

target_include_directories(
    quo-vadisd
    PRIVATE
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    quo-vadisd
    quo-vadis
)

install(TARGETS quo-vadisd DESTINATION bin)

################################################################################
# Fortran
################################################################################
if(FORTRAN_FOUND)
add_library(
    quo_vadisf
    quo-vadisf.f90
)

target_link_libraries(
    quo_vadisf
    quo-vadis
)
endif(FORTRAN_FOUND)

# vim: ts=4 sts=4 sw=4 expandtab
