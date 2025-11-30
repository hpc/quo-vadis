/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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
#include "qvi-task.h"
#include "qvi-utils.h"

qvi_group_mpi::qvi_group_mpi(
    qv_scope_flags_t flags,
    MPI_Comm comm
) : qvi_group(flags)
  , m_created_mpi_ctx(true)
{
    int rc = qvi_new(&m_mpi, comm);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    // Finish task initialization after we finish MPI initialization because
    // the server daemon may have been started during qvi_mpi_init().
    rc = m_task.connect_to_server(m_flags);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
}

qvi_group_mpi::qvi_group_mpi(
    qv_scope_flags_t flags,
    qvi_mpi *mpi_ctx
) : qvi_group(flags)
{
    if (qvi_unlikely(!mpi_ctx)) throw qvi_runtime_error(QV_ERR_INTERNAL);
    m_mpi = mpi_ctx;

    const int rc = m_task.connect_to_server(flags);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
}

qvi_group_mpi::~qvi_group_mpi(void)
{
    if (m_created_mpi_ctx) qvi_delete(&m_mpi);
}

int
qvi_group_mpi::make_intrinsic(
    qv_scope_intrinsic_t scope,
    qv_scope_flags_t
) {
    qvi_group_id_t mpi_group_type;

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
            return QV_ERR_INVLD_ARG;
    }

    return m_mpi->group_from_group_id(
        mpi_group_type, m_mpi_group
    );
}

int
qvi_group_mpi::self(
    qvi_group **child
) {
    int rc = QV_SUCCESS;
    qvi_group_mpi *ichild = nullptr;
    do {
        // Create the child with the parent's MPI context.
        rc = qvi_new(&ichild, m_flags, m_mpi);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
        // Create the underlying group using MPI_COMM_SELF.
        rc = m_mpi->group_from_mpi_comm(
            MPI_COMM_SELF, ichild->m_mpi_group
        );
    } while (false);

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_group_mpi::split(
    int color,
    int key,
    qvi_group **child
) {
    int rc = QV_SUCCESS;
    // Create the child with the parent's MPI context.
    qvi_group_mpi *ichild = nullptr;
    do {
        rc = qvi_new(&ichild, m_flags, m_mpi);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
        // Split this group using MPI.
        rc = m_mpi->group_from_split(
            m_mpi_group, color, key, ichild->m_mpi_group
        );
    } while (false);

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
