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

add_executable(
    test-hw-server
    test-hw-server.c
)

target_include_directories(
    test-hw-server
    PRIVATE
      ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    test-hw-server
    qv-hw-server
)


################################################################################
################################################################################
add_executable(
    test-hw-loc
    test-hw-loc.c
)

target_include_directories(
    test-hw-loc
    PRIVATE
      ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    test-hw-loc
    qv-hw-loc
)

# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    test-hw-server
    test-hw-loc
    PROPERTIES LINKER_LANGUAGE C
)

# vim: ts=4 sts=4 sw=4 expandtab
