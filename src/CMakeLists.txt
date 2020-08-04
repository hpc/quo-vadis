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
    ../include/qvprivate/logger.h
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
    qvi-hw-loc
    hw-loc.cc
    ../include/quo-vadis/hw-loc.h
)

target_include_directories(
    qvi-hw-loc
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-hw-loc
    qvi-logger
    hwloc
)

################################################################################
################################################################################
add_library(
    qvi-hw-server
    hw-server.cc
    ../include/quo-vadis/hw-server.h
)

target_include_directories(
    qvi-hw-server
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    qvi-hw-server
    qvi-hw-loc
    nng
)

# vim: ts=4 sts=4 sw=4 expandtab
