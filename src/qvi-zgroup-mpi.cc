/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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

#include "qvi-common.h"

#include "qvi-zgroup-mpi.h"
#include "qvi-group-mpi.h"

qvi_zgroup_mpi_s::~qvi_zgroup_mpi_s(void)
{
    qvi_mpi_free(&mpi);
}

int
qvi_zgroup_mpi_s::create(void)
{
    return qvi_mpi_new(&mpi);
}

int
qvi_zgroup_mpi_s::initialize(
    MPI_Comm comm
) {
    return qvi_mpi_init(mpi, comm);
}

qvi_task_t *
qvi_zgroup_mpi_s::task(void)
{
    return qvi_mpi_task_get(mpi);
}

int
qvi_zgroup_mpi_s::group_create_intrinsic(
    qv_scope_intrinsic_t scope,
    qvi_group_t **group
) {
    int rc = QV_SUCCESS;

    qvi_group_mpi_t *igroup = qvi_new qvi_group_mpi_t();
    if (!igroup) {
        rc = QV_ERR_OOR;
        goto out;
    }

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
qvi_zgroup_mpi_s::barrier(void)
{
    return qvi_mpi_node_barrier(mpi);
}

int
qvi_zgroup_mpi_new(
    qvi_zgroup_mpi_t **zgroup
) {
    int rc = QV_SUCCESS;

    qvi_zgroup_mpi_t *izgroup = qvi_new qvi_zgroup_mpi_t();
    if (!izgroup) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = izgroup->create();
out:
    if (rc != QV_SUCCESS) {
        qvi_zgroup_mpi_free(&izgroup);
    }
    *zgroup = izgroup;
    return rc;
}

void
qvi_zgroup_mpi_free(
    qvi_zgroup_mpi_t **zgroup
) {
    if (!zgroup) return;
    qvi_zgroup_mpi_t *izgroup = *zgroup;
    if (!izgroup) goto out;
    delete izgroup;
out:
    *zgroup = nullptr;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
