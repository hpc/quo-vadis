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
    qvd
    qvd.cc
)

target_include_directories(
    qvd
    PRIVATE ..
    PRIVATE ${PROJECT_BINARY_DIR}
)

# nng
target_link_libraries(qvd PRIVATE nng)
add_dependencies(qvd nng)

# hwloc
target_link_libraries(qvd PRIVATE hwloc)
add_dependencies(qvd hwloc)

# vim: ts=4 sts=4 sw=4 expandtab
