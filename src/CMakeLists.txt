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
    qvi-utils
    OBJECT
    qvi-utils.h
    qvi-bbuff.h
    qvi-log.h
    qvi-config.h
    qvi-utils.cc
    qvi-bbuff.cc
    qvi-log.cc
    qvi-config.cc
)

target_compile_options(
    qvi-utils
    PUBLIC
      -DSPDLOG_COMPILED_LIB
)

target_include_directories(
    qvi-utils
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-utils
    spdlog
)

################################################################################
################################################################################
add_library(
    qvi-pmi
    OBJECT
    qvi-pmi.h
    qvi-pmi.cc
)

target_include_directories(
    qvi-pmi
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-pmi
    qvi-utils
    pmix
)

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
    qvi-utils
    hwloc
)

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
    qvi-utils
    qvi-hwloc
    zmq
)

################################################################################
################################################################################
add_library(
    qvi-mpi
    OBJECT
    ../include/quo-vadis-mpi.h
    qvi-mpi.h
    qvi-mpi.cc
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
    qvi-utils
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

target_link_libraries(
    quo-vadis
    qvi-utils
    qvi-hwloc
    qvi-pmi
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
