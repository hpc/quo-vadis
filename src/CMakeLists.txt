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

################################################################################
################################################################################
add_library(
    qvi-core
    OBJECT
    qvi-taskman.h
    qvi-group.h
    qvi-bbuff.h
    qvi-log.h
    qvi-utils.h
    qvi-config.h
    qvi-bbuff.cc
    qvi-log.cc
    qvi-utils.cc
    qvi-config.cc
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
#add_library(
#    qvi-pmi
#    OBJECT
#    qvi-pmi.h
#    qvi-pmi.cc
#)
#
#target_include_directories(
#    qvi-pmi
#    PUBLIC
#      $<INSTALL_INTERFACE:include>
#      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
#      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
#)
#
#target_link_libraries(
#    qvi-pmi
#    qvi-core
#    pmix
#)

################################################################################
################################################################################
add_library(
    qvi-hwloc
    OBJECT
    qvi-hwloc.h
    qvi-bind.h
    qvi-hwloc.cc
    qvi-bind.cc
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
)

if(CUDAToolkit_FOUND AND NOT QV_DISABLE_GPU_SUPPORT)
target_link_libraries(
    qvi-hwloc
    CUDA::cudart
    CUDA::nvml
)
endif()

################################################################################
################################################################################
add_library(
    qvi-rmi
    OBJECT
    qvi-rmi.h
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
add_library(
    qvi-mpi
    OBJECT
    ../include/quo-vadis-mpi.h
    qvi-taskman-mpi.h
    qvi-mpi.h
    qvi-mpi.cc
    quo-vadis-mpi.cc
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
    qvi-rmi
    ${MPI_C_LIBRARIES}
)

################################################################################
################################################################################
# TODO(skg) Add an MPI variant of the library. For now, just include MPI bits.
add_library(
    quo-vadis
    ../include/quo-vadis.h
    qvi-context.h
    qvi-task.h
    qvi-scope.h
    qvi-task.cc
    qvi-scope.cc
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

# TODO(skg) Add qvi-pmi
target_link_libraries(
    quo-vadis
    qvi-core
    qvi-hwloc
    qvi-mpi
    qvi-rmi
)

install(TARGETS quo-vadis DESTINATION lib)

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

# vim: ts=4 sts=4 sw=4 expandtab
