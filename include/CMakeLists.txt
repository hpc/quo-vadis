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

# Install
install(
    FILES
      quo-vadis.h
      quo-vadis-process.h
      quo-vadis-pthread.h
    DESTINATION
      include
)

if(OpenMP_C_FOUND)
    install(
        FILES
          quo-vadis-omp.h
        DESTINATION
          include
    )
endif()

if(MPI_FOUND)
    install(
        FILES
          quo-vadis-mpi.h
        DESTINATION
          include
    )
endif()

# vim: ts=4 sts=4 sw=4 expandtab
