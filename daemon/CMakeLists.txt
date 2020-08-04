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
    quo-vadisd
    quo-vadisd.cc
)

target_include_directories(
    quo-vadisd
    PRIVATE
      ${PROJECT_BINARY_DIR}
      ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    quo-vadisd
    qvi-hw-server
)

install(TARGETS quo-vadisd DESTINATION bin)

# vim: ts=4 sts=4 sw=4 expandtab
