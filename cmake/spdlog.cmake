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

# Includes support for external projects
include(ExternalProject)

set(QVI_SPDLOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/spdlog/v1.8.5.tar.gz)
set(QVI_SPDLOG_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/spdlog)
set(QVI_SPDLOG_STATIC_LIB ${QVI_SPDLOG_PREFIX}/lib/libspdlog.a)
set(QVI_SPDLOG_INCLUDES ${QVI_SPDLOG_PREFIX}/include)

file(MAKE_DIRECTORY ${QVI_SPDLOG_INCLUDES})

ExternalProject_Add(
    libspdlog
    URL file://${QVI_SPDLOG_DIR}
    URL_MD5 "8755cdbc857794730a022722a66d431a"
    PREFIX ${QVI_SPDLOG_PREFIX}
    CMAKE_ARGS
      -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
      -DCMAKE_INSTALL_PREFIX:PATH=${QVI_SPDLOG_PREFIX}
      -DSPDLOG_BUILD_EXAMPLE:BOOL=NO
      -DCMAKE_INSTALL_LIBDIR:PATH=lib
    BUILD_BYPRODUCTS ${QVI_SPDLOG_STATIC_LIB}
)

add_library(spdlog STATIC IMPORTED GLOBAL)
add_dependencies(spdlog libspdlog)

set_target_properties(
    spdlog
    PROPERTIES
      IMPORTED_LOCATION ${QVI_SPDLOG_STATIC_LIB}
      INTERFACE_INCLUDE_DIRECTORIES ${QVI_SPDLOG_INCLUDES}
      INTERFACE_LINK_LIBRARIES Threads::Threads
)

# vim: ts=4 sts=4 sw=4 expandtab
