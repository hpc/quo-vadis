#
# Copyright (c) 2020-2025 Triad National Security, LLC
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

set(QVI_CEREAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/cereal/v1.3.2.tar.gz)
set(QVI_CEREAL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/cereal)
set(QVI_CEREAL_STATIC_LIB ${QVI_CEREAL_PREFIX}/include/cereal/cereal.hpp)
set(QVI_CEREAL_INCLUDES ${QVI_CEREAL_PREFIX}/include)

file(MAKE_DIRECTORY ${QVI_CEREAL_INCLUDES})

ExternalProject_Add(
    libcereal
    URL file://${QVI_CEREAL_DIR}
    URL_MD5 "ab6070fadc7c50072ef4153fb1c46a87"
    PREFIX ${QVI_CEREAL_PREFIX}
    CMAKE_ARGS
      -DCMAKE_VERBOSE_MAKEFILE=${CMAKE_VERBOSE_MAKEFILE}
      -DCMAKE_INSTALL_PREFIX:PATH=${QVI_CEREAL_PREFIX}
      -DCMAKE_INSTALL_LIBDIR:PATH=lib
      -DBUILD_DOC:BOOL=NO
      -DBUILD_SANDBOX:BOOL=NO
      -DJUST_INSTALL_CEREAL:BOOL=YES
    BUILD_BYPRODUCTS ${QVI_CEREAL_STATIC_LIB}
)

add_library(cereal STATIC IMPORTED GLOBAL)
add_dependencies(cereal libcereal)

set_target_properties(
    cereal
    PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES ${QVI_CEREAL_INCLUDES}
)

# vim: ts=4 sts=4 sw=4 expandtab
