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
 * @file qvi-zgroup-mpi.cc
 */

#include "qvi-zgroup-mpi.h"
#include "qvi-group-mpi.h"
#include "qvi-utils.h"

int
qvi_zgroup_mpi_s::group_create_intrinsic(
    qv_scope_intrinsic_t scope,
    qvi_group_t **group
) {
    int rc = QV_SUCCESS;

    qvi_group_mpi_t *igroup = new qvi_group_mpi_t();

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
        mpi,
        mpi_group,
        &igroup->mpi_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete igroup;
        igroup = nullptr;
    }
    *group = igroup;
    return rc;
}

int
qvi_zgroup_mpi_new(
    qvi_zgroup_mpi_t **zgroup
) {
    return qvi_new_rc(zgroup);
}

void
qvi_zgroup_mpi_free(
    qvi_zgroup_mpi_t **zgroup
) {
    qvi_delete(zgroup);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
