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
    test-msg
    test-msg.c
)

target_include_directories(
    test-msg
    PRIVATE
      ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(
    test-msg
    qvi-msg
)

add_test(
    NAME
      msg
    COMMAND
      bash -c "export URL=\"tcp://127.0.0.1:55995\"    && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-msg $URL -s &) && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-msg $URL 1 &)  && \
      (${CMAKE_CURRENT_BINARY_DIR}/test-msg $URL 1 &)  && \
      ${CMAKE_CURRENT_BINARY_DIR}/test-msg $URL 3      && \
      killall test-msg"
)

################################################################################
################################################################################
# Use the C linker to test for C/C++ linkage problems.
set_target_properties(
    test-hw-server
    test-hw-loc
    test-msg
    PROPERTIES LINKER_LANGUAGE C
)

# vim: ts=4 sts=4 sw=4 expandtab
