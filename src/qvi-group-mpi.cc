/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-mpi.cc
 */

#include "qvi-group-mpi.h"

int
qvi_group_mpi_s::self(
    qvi_group_t **child
) {
    qvi_group_mpi_t *ichild = new qvi_group_mpi_t();
    // Initialize the child with the parent's MPI instance.
    int rc = ichild->initialize(mpi);
    if (rc != QV_SUCCESS) goto out;
    // Create the underlying group using MPI_COMM_SELF.
    rc = qvi_mpi_group_create_from_mpi_comm(
        mpi, MPI_COMM_SELF, &ichild->mpi_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete ichild;
        ichild = nullptr;
    }
    *child = ichild;
    return rc;
}

int
qvi_group_mpi_s::split(
    int color,
    int key,
    qvi_group_t **child
) {
    qvi_group_mpi_t *ichild = new qvi_group_mpi_t();
    // Initialize the child with the parent's MPI instance.
    int rc = ichild->initialize(mpi);
    if (rc != QV_SUCCESS) goto out;
    // Split this group using MPI.
    rc = qvi_mpi_group_create_from_split(
        mpi, mpi_group, color,
        key, &ichild->mpi_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete ichild;
        ichild = nullptr;
    }
    *child = ichild;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
