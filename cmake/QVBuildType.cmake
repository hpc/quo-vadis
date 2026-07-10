#
# Copyright (c) 2020-2026 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c)      2020 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# Set a default build type if none was specified.
set(default_build_type "Release")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    set(default_build_type "Debug")
    set(QVI_IN_GIT_REPO ON)
else()
    set(QVI_IN_GIT_REPO OFF)
endif()

# Developer mode option.
option(QV_DEVELOPER_MODE "Toggle developer mode" ${QVI_IN_GIT_REPO})

if(QV_DEVELOPER_MODE)
    message(
        STATUS "Developer mode enabled. Adding picky compile flags."
    )
    # Store the flags. They get applied to our own source
    # below after third-party packages are configured.
    set(
        QVI_PICKY_COMPILE_OPTIONS
        -Wall
        -Wextra
        $<$<COMPILE_LANGUAGE:C>:-pedantic>
        $<$<COMPILE_LANGUAGE:CXX>:-pedantic>
    )
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(
        STATUS
          "Setting build type to '${default_build_type}' as none was specified."
    )
    set(
        CMAKE_BUILD_TYPE "${default_build_type}" CACHE
        STRING "Choose the type of build." FORCE
    )
    # Set the possible values of build type for cmake-gui
    set_property(
        CACHE CMAKE_BUILD_TYPE
        PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
    )
endif()

# vim: ts=4 sts=4 sw=4 expandtab
