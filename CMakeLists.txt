#
# Copyright (c) 2020-2022 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# Set minimum required cmake version
cmake_minimum_required(VERSION 3.19)

# Set the project name and version
project(
    quo-vadis
    VERSION 0.0.1
    DESCRIPTION "Infrastructure for dynamic computer resource arbitration."
    LANGUAGES C CXX
)

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(C_STANDARD_REQUIRED TRUE)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CXX_STANDARD_REQUIRED TRUE)

# Setup Fortran support.
include(cmake/QVFortranSupport.cmake)

################################################################################
# RPATH Settings (Always RPATH)
# Note: See CMAKE_SKIP_RPATH to disable this behavior.
################################################################################
# From: https://gitlab.kitware.com/cmake/
# community/-/wikis/doc/cmake/RPATH-handling
# Don't skip the full RPATH for the build tree.
set(CMAKE_SKIP_BUILD_RPATH FALSE)
# When building, don't use the install RPATH already
# (but later on when installing).
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
# Add the automatically determined parts of the RPATH which
# point to directories outside the build tree to the install RPATH.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
# Set the RPATH to be used when installing.
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# Project Generator Info
message(STATUS "Generator used is ${CMAKE_GENERATOR}")

# Generate a compile_commands.json file (for development tools).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Testing
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

# Pthread support
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

# Append our module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

################################################################################
# ZeroMQ Support ###############################################################
################################################################################
find_package(ZMQ REQUIRED)

################################################################################
# Goup Support #################################################################
################################################################################
# TODO(skg) At this point both are 'required', but this isn't necessary: we only
# need one or the other.
# PMIx support
# find_package(PMIx REQUIRED)
# MPI support
# TODO(skg) Make sure that we are at least at MPI-3
option(QV_DISABLE_MPI_SUPPORT "Unconditionally disable MPI support" OFF)
message(CHECK_START "Determining MPI support status")
if(NOT QV_DISABLE_MPI_SUPPORT)
    message(CHECK_PASS "enabled")
else()
    message(CHECK_PASS "disabled")
endif()

if(NOT QV_DISABLE_MPI_SUPPORT)
    find_package(MPI)
endif()

################################################################################
# GPU Support ##################################################################
################################################################################
option(QV_DISABLE_GPU_SUPPORT "Unconditionally disable GPU support" OFF)

message(CHECK_START "Determining desired GPU support level")
if(NOT QV_DISABLE_GPU_SUPPORT)
    message(CHECK_PASS "enabled")
else()
    message(CHECK_PASS "disabled")
endif()

if(NOT QV_DISABLE_GPU_SUPPORT)
    # TODO(skg) Set minimum required version?
    find_package(CUDAToolkit)
    find_package(ROCm)
endif()

# Specify default build type
include(cmake/QVBuildType.cmake)

# Check include files
include(cmake/QVCheckIncludes.cmake)

# Add generated configuration file
configure_file(config.h.in include/${PROJECT_NAME}/config.h)

# Check datatype sizes
include(cmake/QVCheckTypes.cmake)

# TODO(skg) Improve
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

# Internal, third-party packages
include(cmake/QVSpdLog.cmake)
include(cmake/QVhwloc.cmake)

# Add our source
add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(src/fortran)
# Conditionally add test source
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

# CPack
# TODO(skg) Add more, improve.
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_SOURCE_GENERATOR "TBZ2;TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES "/\.cache/;/\.git/;/\.*.swp$;/build/")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt)
include(CPack)

# vim: ts=4 sts=4 sw=4 expandtab
