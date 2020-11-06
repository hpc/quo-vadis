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

# TODO(skg) Add valgrind tests.
################################################################################
################################################################################
add_executable(
    test-hwloc
    test-hwloc.c
)

target_link_libraries(
    test-hwloc
    quo-vadis
)

add_test(
    hwloc
    test-hwloc
)

################################################################################
################################################################################
add_executable(
    test-rmi
    test-rmi.c
)

target_link_libraries(
    test-rmi
    quo-vadis
)

add_test(
    NAME
      rmi
    COMMAND
      bash -c "export URL=\"tcp://127.0.0.1:55995\"     && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -s &)  && \
      sleep 2                                           && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c &)  && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c &)  && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c &)  && \
       ${CMAKE_CURRENT_BINARY_DIR}/test-rmi $URL -c     && \
      killall test-rmi"
)

################################################################################
################################################################################
add_executable(
    test-scopes
    test-scopes.c
)

target_link_libraries(
    test-scopes
    quo-vadis
)

add_test(
    scopes
    test-scopes
)

################################################################################
################################################################################
# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    test-hwloc
    test-rmi
    test-scopes
    PROPERTIES LINKER_LANGUAGE C
)

# Set test timeout
set_tests_properties(
    hwloc
    rmi
    scopes
    PROPERTIES TIMEOUT 15
)

# vim: ts=4 sts=4 sw=4 expandtab
