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
project(quo-vadis VERSION 0.0.1 LANGUAGES C CXX)

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project Generator Info
message(
    STATUS
      "Generator used is ${CMAKE_GENERATOR}"
)

# Testing
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

# Pthread support
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Specify default build type
include(cmake/build-type.cmake)

# Generate a compile_commands.json file (for development tools)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Check include files
include(cmake/cif.cmake)
# Add config file
configure_file(config.h.in include/${PROJECT_NAME}/config.h)
# Install the generated configuration file
install(
    FILES
      ${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}/config.h
    DESTINATION
      include/${PROJECT_NAME}
)

# TODO(skg) Improve
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

# Internal, third-party packages
include(cmake/spdlog.cmake)
include(cmake/nng.cmake)
include(cmake/hwloc.cmake)

# Add our source
add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(daemon)
# Conditionally add test source
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

# CPack
# TODO(skg) Add more, improve.
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_SOURCE_GENERATOR "TBZ2;TGZ;ZIP")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "quo-vadis")
include(CPack)

# pkg-config
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
)
install(
    FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc"
    DESTINATION lib/pkgconfig
)

# vim: ts=4 sts=4 sw=4 expandtab
