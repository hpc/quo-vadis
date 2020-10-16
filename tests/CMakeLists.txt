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

################################################################################
################################################################################
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

add_test(hw-server test-hw-server)
# TODO(skg) Add valgrind tests.
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

add_test(hw-loc test-hw-loc)

################################################################################
################################################################################
add_executable(
    test-rpc
    test-rpc.c
)

target_include_directories(
    test-rpc
    PRIVATE
      ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    test-rpc
    qvi-rpc
)

add_test(
    NAME
      rpc
    COMMAND
      bash -c "export URL=\"tcp://127.0.0.1:55995\"    && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rpc $URL -s &) && \
      sleep 2 && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rpc $URL 2 &)  && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rpc $URL 1 &)  && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-rpc $URL 1 &)  && \
      ${CMAKE_CURRENT_BINARY_DIR}/test-rpc $URL 3      && \
      killall test-rpc"
)

################################################################################
################################################################################
# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    test-hw-server
    test-hw-loc
    test-rpc
    PROPERTIES LINKER_LANGUAGE C
)

# Set test timeout
set_tests_properties(
    hw-server
    hw-loc
    rpc
    PROPERTIES TIMEOUT 15
)

# vim: ts=4 sts=4 sw=4 expandtab
