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
    quo-vadis
    OBJECT
      ../include/quo-vadis.h
      qvi-common.h
      qvi-macros.h
      qvi-log.h
      qvi-utils.h
      qvi-bbuff.h
      qvi-hwloc.h
      qvi-nvml.h
      qvi-rsmi.h
      qvi-rsmi.h
      qvi-line.h
      qvi-hwpool.h
      qvi-rmi.h
      qvi-bbuff-rmi.h
      qvi-context.h
      qvi-task.h
      qvi-devinfo.h
      qvi-group.h
      qvi-scope.h
      qvi-zgroup.h
      qvi-bind.h
      qvi-log.cc
      qvi-utils.cc
      qvi-bbuff.cc
      qvi-hwloc.cc
      qvi-nvml.cc
      qvi-rsmi.cc
      qvi-line.cc
      qvi-hwpool.cc
      qvi-rmi.cc
      qvi-task.cc
      qvi-scope.cc
      qvi-bind.cc
      quo-vadis.cc
)

target_compile_options(
    quo-vadis
    PUBLIC
      -DSPDLOG_COMPILED_LIB
)

target_include_directories(
    quo-vadis
    PUBLIC
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
      $<BUILD_INTERFACE:${QVI_SPDLOG_INCLUDES}>
      $<BUILD_INTERFACE:${QVI_HWLOC_INCLUDES}>
      $<BUILD_INTERFACE:${ZMQ_INCLUDE_DIRS}>
)

target_link_libraries(
    quo-vadis
    PUBLIC
      spdlog
      hwloc
      zmq
      pciaccess
)

if(CUDAToolkit_FOUND AND NOT QV_DISABLE_GPU_SUPPORT)
target_link_libraries(
    quo-vadis
    PUBLIC
      CUDA::cudart
      CUDA::nvml
)
endif()

if(ROCM_FOUND AND NOT QV_DISABLE_GPU_SUPPORT)
target_link_libraries(
    quo-vadis
    PUBLIC
      ROCm
)
endif()

################################################################################
################################################################################
if (MPI_FOUND)
add_library(
    quo-vadis-mpi
    ../include/quo-vadis-mpi.h
    qvi-mpi.h
    qvi-group-mpi.h
    qvi-zgroup-mpi.h
    qvi-mpi.cc
    qvi-group-mpi.cc
    qvi-zgroup-mpi.cc
    quo-vadis-mpi.cc
)

target_include_directories(
    quo-vadis-mpi
    PUBLIC
      $<TARGET_PROPERTY:quo-vadis,INCLUDE_DIRECTORIES>
      ${MPI_C_INCLUDE_DIRS}
)

target_link_libraries(
    quo-vadis-mpi
    spdlog
    hwloc
    quo-vadis
    ${MPI_C_LIBRARIES}
)

install(TARGETS quo-vadis-mpi DESTINATION lib)

# pkg-config
configure_file(
    "${CMAKE_SOURCE_DIR}/pkg-config.pc.in"
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-mpi.pc"
    @ONLY
)
install(
    FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-mpi.pc"
    DESTINATION lib/pkgconfig
)

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
      $<TARGET_PROPERTY:quo-vadis,INCLUDE_DIRECTORIES>
)

target_link_libraries(
    quo-vadisd
    PRIVATE
      spdlog
      hwloc
      quo-vadis
)

install(TARGETS quo-vadisd DESTINATION bin)

# vim: ts=4 sts=4 sw=4 expandtab
