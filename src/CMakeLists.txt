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
    qvi-logger
    logger.cc
    ../include/private/logger.h
)

target_compile_options(
    qvi-logger
    PRIVATE
      -DSPDLOG_COMPILED_LIB
)

target_include_directories(
    qvi-logger
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-logger
    PUBLIC
      spdlog
)

################################################################################
################################################################################
add_library(
    qv-hw-loc
    hw-loc.cc
    ../include/quo-vadis/hw-loc.h
)

target_include_directories(
    qv-hw-loc
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qv-hw-loc
    qvi-logger
    hwloc
)

################################################################################
################################################################################
add_library(
    qvi-rpc
    rpc.cc
    ../include/private/rpc.h
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
    qvi-logger
    nng
)

################################################################################
################################################################################
add_library(
    qvi-rmi
    rmi.cc
    ../include/private/rmi.h
)

target_include_directories(
    qvi-rmi
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-rmi
    qvi-rpc
    qv-hw-loc
)

# vim: ts=4 sts=4 sw=4 expandtab
