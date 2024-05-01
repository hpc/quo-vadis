# -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*-
#
# Copyright (c) 2020-2024 Triad National Security, LLC
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

# Append our module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Setup Fortran support.
include(cmake/QVFortranSupport.cmake)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    # DOWNLOAD_EXTRACT_TIMESTAMP=TRUE
    cmake_policy(SET CMP0135 OLD)
endif()

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

################################################################################
# Pthread support ##############################################################
################################################################################
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

################################################################################
# ZeroMQ Support ###############################################################
################################################################################
find_package(ZMQ REQUIRED)

################################################################################
# Goup Support #################################################################
################################################################################

# MPI support
################################################################################
option(QV_MPI_SUPPORT "Toggle MPI support" ON)

option(
    QV_MPI_PROCESSES_ARE_THREADS
    "Set to TRUE when the MPI library implements MPI processes as threads"
    FALSE
)

message(CHECK_START "Determining MPI support status")
if(QV_MPI_SUPPORT)
    message(CHECK_PASS "enabled")
else()
    message(CHECK_PASS "disabled")
endif()

if(QV_MPI_SUPPORT)
    find_package(MPI)
    if (NOT MPI_FOUND)
        message(STATUS "MPI not found. Please set MPI_HOME")
    else()
        # The MPI library needs to be at least MPI-3.
        set(QV_MPI_VER_MIN "3.0")
        set(QV_MPI_VER "${MPI_C_VERSION_MAJOR}.${MPI_C_VERSION_MINOR}")
        if (${QV_MPI_VER} VERSION_LESS ${QV_MPI_VER_MIN})
            message(
                FATAL_ERROR
                "Minimum MPI version not met: ${QV_MPI_VER} < ${QV_MPI_VER_MIN}"
            )
        endif()
        # By default we assume MPI processes are implemented as OS processes.
        message(CHECK_START "Determining underlying MPI process type")
        set(MPI_PROCESSES_ARE_THREADS FALSE)
        if(QV_MPI_PROCESSES_ARE_THREADS)
            set(MPI_PROCESSES_ARE_THREADS TRUE)
            message(CHECK_PASS "threads")
        else()
            message(CHECK_PASS "OS processes")
        endif()
    endif()
endif()

# OpenMP support
################################################################################
option(QV_OMP_SUPPORT "Toggle OpenMP support" ON)

message(CHECK_START "Determining OpenMP support status")
if(QV_OMP_SUPPORT)
    message(CHECK_PASS "enabled")
else()
    message(CHECK_PASS "disabled")
endif()

if(QV_OMP_SUPPORT)
    find_package(OpenMP)
endif()

################################################################################
# GPU Support ##################################################################
################################################################################
option(QV_GPU_SUPPORT "Toggle GPU support" ON)

if(QV_GPU_SUPPORT)
    # TODO(skg) Set minimum required version?
    find_package(CUDAToolkit)
    find_package(ROCmSMI)
    if (NOT (CUDAToolkit_FOUND OR ROCmSMI_FOUND))
        set(QV_GPU_SUPPORT OFF)
        message(STATUS "Turning off GPU support: no suitable support found")
    endif()
endif()

message(CHECK_START "Determining GPU support level")
if(QV_GPU_SUPPORT)
    message(CHECK_PASS "enabled")
else()
    message(CHECK_PASS "disabled")
endif()

# Specify default build type
include(cmake/QVBuildType.cmake)

# Specify sanitizers
include(cmake/QVSanitize.cmake)

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
    add_subdirectory(tests/internal)
    add_subdirectory(tests)
endif()

# CPack
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
# Name the output source packages as quo-vadis-version.*
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
set(CPACK_SOURCE_GENERATOR "TBZ2;TGZ;ZIP")
set(
    CPACK_SOURCE_IGNORE_FILES
    "/\.cache/;/\.git/;/\.*.swp$;/build/;cscope.*;/\.gitignore;core.*"
)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt)
include(CPack)

# vim: ts=4 sts=4 sw=4 expandtab
