#
# Copyright (c)      2020 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c)      2020 Lawrence Livermore National Security, LLC
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
    qvi-utils.cc
    qvi-log.cc
    ../include/private/qvi-utils.h
    ../include/private/qvi-log.h
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
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
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
    qvi-pmi.cc
    ../include/private/qvi-pmi.h
)

target_include_directories(
    qvi-pmi
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
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
    qvi-mpi
    OBJECT
    qvi-mpi.cc
    ../include/private/qvi-mpi.h
    ../include/quo-vadis/qv-mpi.h
)

target_include_directories(
    qvi-mpi
    PUBLIC
      ${MPI_C_INCLUDE_DIRS}
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-mpi
    qvi-utils
    ${MPI_C_LIBRARIES}
)

################################################################################
################################################################################
add_library(
    qvi-hwloc
    OBJECT
    qvi-hwloc.cc
    ../include/private/qvi-hwloc.h
)

target_include_directories(
    qvi-hwloc
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
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
    qvi-rpc
    OBJECT
    qvi-rpc.cc
    ../include/private/qvi-rpc.h
)

target_include_directories(
    qvi-rpc
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-rpc
    qvi-utils
    qvi-hwloc
    nng
)

################################################################################
################################################################################
# TODO(skg) Add an MPI variant of the library. For now, just include MPI bits.
add_library(
    quo-vadis
    qv-task.cc
    ../include/private/qvi-task.h
    ../include/quo-vadis/qv-task.h
    qvi-rmi.cc
    ../include/private/qvi-rmi.h
    qv-group.cc
    ../include/quo-vadis/qv-group.h
    qv-scope.cc
    ../include/quo-vadis/qv-scope.h
)

target_include_directories(
    quo-vadis
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    quo-vadis
    qvi-utils
    qvi-pmi
    qvi-mpi
    qvi-hwloc
    qvi-rpc
)

install(TARGETS quo-vadis DESTINATION lib)

# vim: ts=4 sts=4 sw=4 expandtab
