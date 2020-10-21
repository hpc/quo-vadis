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

# Includes support for external projects
include(ExternalProject)

set(QVI_NNG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/nng-1.3.2)
set(QVI_NNG_BIN ${CMAKE_CURRENT_BINARY_DIR}/nng)
set(QVI_NNG_STATIC_LIB ${QVI_NNG_BIN}/lib/libnng.a)
set(QVI_NNG_INCLUDES ${QVI_NNG_BIN}/include)

file(MAKE_DIRECTORY ${QVI_NNG_INCLUDES})

ExternalProject_Add(
    libnng
    SOURCE_DIR ${QVI_NNG_DIR}
    PREFIX ${QVI_NNG_BIN}
    CMAKE_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${QVI_NNG_BIN}
      -DNNG_TESTS:BOOL=OFF
      -DCMAKE_INSTALL_LIBDIR:PATH=lib
    BUILD_BYPRODUCTS ${QVI_NNG_STATIC_LIB}
)

add_library(nng STATIC IMPORTED GLOBAL)
add_dependencies(nng libnng)

set_target_properties(
    nng
    PROPERTIES
      IMPORTED_LOCATION ${QVI_NNG_STATIC_LIB}
      INTERFACE_INCLUDE_DIRECTORIES ${QVI_NNG_INCLUDES}
)

# vim: ts=4 sts=4 sw=4 expandtab
