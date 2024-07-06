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
#include "qvi-utils.h"

int
qvi_group_mpi_s::make_intrinsic(
    qv_scope_intrinsic_t scope
) {
    int rc = QV_SUCCESS;
    qvi_mpi_group_id_t mpi_group_type;
    // TODO(skg) Finish implementation.
    switch (scope) {
        case QV_SCOPE_SYSTEM:
        case QV_SCOPE_USER:
        case QV_SCOPE_JOB:
            mpi_group_type = QVI_MPI_GROUP_NODE;
            break;
        case QV_SCOPE_PROCESS:
            mpi_group_type = QVI_MPI_GROUP_SELF;
            break;
        default:
            rc = QV_ERR_INVLD_ARG;
            break;
    }
    if (rc != QV_SUCCESS) return rc;

    return qvi_mpi_group_create_from_group_id(
        mpi, mpi_group_type, &mpi_group
    );
}

int
qvi_group_mpi_s::self(
    qvi_group_t **child
) {
    // Create and initialize the child with the parent's MPI context.
    qvi_group_mpi_t *ichild = nullptr;
    int rc = qvi_new(&ichild, mpi);
    if (rc != QV_SUCCESS) goto out;
    // Create the underlying group using MPI_COMM_SELF.
    rc = qvi_mpi_group_create_from_mpi_comm(
        mpi, MPI_COMM_SELF, &ichild->mpi_group
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ichild);
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
    // Create and initialize the child with the parent's MPI context.
    qvi_group_mpi_t *ichild = nullptr;
    int rc = qvi_new(&ichild, mpi);
    if (rc != QV_SUCCESS) goto out;
    // Split this group using MPI.
    rc = qvi_mpi_group_create_from_split(
        mpi, mpi_group, color,
        key, &ichild->mpi_group
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
