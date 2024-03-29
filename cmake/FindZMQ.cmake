#
# Copyright (c) 2020-2023 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

find_path(
    ZMQ_HOME
    NAMES
      include/zmq.h
    REQUIRED
)

find_path(
    ZMQ_INCLUDE_DIR
    NAMES
      zmq.h
    HINTS
      "${ZMQ_HOME}/include"
)

find_library(
    ZMQ_LIBRARY
    NAMES
      zmq
    HINTS
      "${ZMQ_HOME}/lib"
)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set ZMQ_FOUND to TRUE if all
# listed variables are TRUE
find_package_handle_standard_args(
    ZMQ
    DEFAULT_MSG
    ZMQ_HOME ZMQ_LIBRARY ZMQ_INCLUDE_DIR
)

mark_as_advanced(
    ZMQ_HOME
    ZMQ_INCLUDE_DIR
    ZMQ_LIBRARY
)

set(ZMQ_HOME ${ZMQ_HOME})
set(ZMQ_LIBRARIES ${ZMQ_LIBRARY})
set(ZMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_DIR})

add_library(
    zmq
    STATIC IMPORTED GLOBAL
)

set_target_properties(
    zmq
    PROPERTIES
      IMPORTED_LOCATION ${ZMQ_LIBRARIES}
      INTERFACE_INCLUDE_DIRECTORIES ${ZMQ_INCLUDE_DIRS}
)

# vim: ts=4 sts=4 sw=4 expandtab
