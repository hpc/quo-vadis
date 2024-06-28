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
qvi_group_mpi_s::intrinsic(
    qv_scope_intrinsic_t scope,
    qvi_group_s **group
) {
    qvi_group_mpi_t *igroup = nullptr;
    int rc = qvi_new(&igroup);
    if (rc != QV_SUCCESS) goto out;

    rc = igroup->initialize(mpi);
    if (rc != QV_SUCCESS) goto out;

    qvi_mpi_group_id_t mpi_group;
    // TODO(skg) Finish implementation.
    switch (scope) {
        case QV_SCOPE_SYSTEM:
        case QV_SCOPE_USER:
        case QV_SCOPE_JOB:
            mpi_group = QVI_MPI_GROUP_NODE;
            break;
        case QV_SCOPE_PROCESS:
            mpi_group = QVI_MPI_GROUP_SELF;
            break;
        default:
            rc = QV_ERR_INVLD_ARG;
            break;
    }
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_mpi_group_create_from_group_id(
        mpi, mpi_group, &igroup->mpi_group
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&igroup);
    }
    *group = igroup;
    return rc;
}

int
qvi_group_mpi_s::self(
    qvi_group_t **child
) {
    qvi_group_mpi_t *ichild = nullptr;
    int rc = qvi_new(&ichild);
    if (rc != QV_SUCCESS) goto out;
    // Initialize the child with the parent's MPI instance.
    rc = ichild->initialize(mpi);
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
    qvi_group_mpi_t *ichild = nullptr;
    int rc = qvi_new(&ichild);
    if (rc != QV_SUCCESS) goto out;
    // Initialize the child with the parent's MPI instance.
    rc = ichild->initialize(mpi);
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
