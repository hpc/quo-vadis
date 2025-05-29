#
# Copyright (c) 2020-2025 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

# Install
install(
    FILES
      quo-vadis.h
      quo-vadis-thread.h
    DESTINATION
      include
)

if(MPI_FOUND)
    install(
        FILES
          quo-vadis-mpi.h
        DESTINATION
          include
    )
endif()

# vim: ts=4 sts=4 sw=4 expandtab
