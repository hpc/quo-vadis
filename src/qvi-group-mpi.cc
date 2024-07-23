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
#include "qvi-task.h" // IWYU pragma: keep
#include "qvi-utils.h"

qvi_group_mpi_s::qvi_group_mpi_s(void)
{
    const int rc = qvi_new(&m_task);
    if (rc != QV_SUCCESS) throw qvi_runtime_error();
}

qvi_group_mpi_s::qvi_group_mpi_s(
    qvi_mpi_t *mpi_ctx
) : qvi_group_mpi_s()
{
    if (!mpi_ctx) throw qvi_runtime_error();
    m_mpi = mpi_ctx;
}

qvi_group_mpi_s::~qvi_group_mpi_s(void)
{
    qvi_mpi_group_free(&m_mpi_group);
    qvi_delete(&m_task);
}

int
qvi_group_mpi_s::make_intrinsic(
    qv_scope_intrinsic_t scope
) {
    int rc = QV_SUCCESS;
    qvi_group_id_t mpi_group_type;
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
        m_mpi, mpi_group_type, &m_mpi_group
    );
}

int
qvi_group_mpi_s::self(
    qvi_group_t **child
) {
    // Create and initialize the child with the parent's MPI context.
    qvi_group_mpi_t *ichild = nullptr;
    int rc = qvi_new(&ichild, m_mpi);
    if (rc != QV_SUCCESS) goto out;
    // Create the underlying group using MPI_COMM_SELF.
    rc = qvi_mpi_group_create_from_mpi_comm(
        m_mpi, MPI_COMM_SELF, &ichild->m_mpi_group
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
    int rc = qvi_new(&ichild, m_mpi);
    if (rc != QV_SUCCESS) goto out;
    // Split this group using MPI.
    rc = qvi_mpi_group_create_from_split(
        m_mpi, m_mpi_group, color,
        key, &ichild->m_mpi_group
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
