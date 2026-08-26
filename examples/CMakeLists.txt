# -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*-
#
# Copyright (c) 2020-2026 Triad National Security, LLC
#                         All rights reserved.
#
# Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
#                         All rights reserved.
#
# This file is part of the quo-vadis project. See the LICENSE file at the
# top-level directory of this distribution.
#

################################################################################
################################################################################
if(MPI_FOUND)
    add_executable(
        example-mpi-phases
        example-mpi-phases.c
    )

    target_link_libraries(
        example-mpi-phases
        quo-vadis-mpi
    )

    # Use the C linker to test for C/C++ linkage problems.
    set_target_properties(
        example-mpi-phases
        PROPERTIES LINKER_LANGUAGE C
    )
endif()

# vim: ts=4 sts=4 sw=4 expandtab
