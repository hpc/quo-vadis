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

# Set minimum required cmake version
cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(quo-vadis VERSION 0.0.1)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Check include files
include(cmake/cif.cmake)
# Add config file
configure_file(config.h.in config.h)
# Add define that lets the source know one exists
add_definitions(-DHAVE_CONFIG_H)

# Internal, third-party packages
include(cmake/nng.cmake)
include(cmake/hwloc.cmake)

# Always use lib, not things like lib64 (looking at you, GNUInstallDirs)
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "" FORCE)

add_subdirectory(src)

# TODO(skg) Add more, improve.
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_SOURCE_GENERATOR "TBZ2;TGZ;ZIP")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "quo-vadis")
include(CPack)

# vim: ts=4 sts=4 sw=4 expandtab
