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
    libqvi-logger
    logger.cc
)

target_include_directories(
    libqvi-logger
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    libqvi-logger
    PUBLIC
      spdlog
)

################################################################################
################################################################################
add_library(
    libqvi-hw-loc
    hw-loc.cc
)

target_include_directories(
    libqvi-hw-loc
    PUBLIC
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(
    libqvi-hw-loc
    libqvi-logger
    hwloc
)

################################################################################
################################################################################
add_library(
    qvi-hw-server
    hw-server.cc
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
    libqvi-hw-loc
    nng
)

# vim: ts=4 sts=4 sw=4 expandtab
