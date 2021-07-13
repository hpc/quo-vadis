/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis-mpi.cc
 */


#include "qvi-common.h"

#include "qvi-context.h"
#include "qvi-utils.h"
#include "qvi-taskman-mpi.h"

// TODO(skg) This should probably be in a common area because other
// infrastructure will likely use something similar.
static int
connect_to_server(
    qv_context_t *ctx
) {
    char *url = nullptr;
    int rc = qvi_url(&url);
    if (rc != QV_SUCCESS) {
        qvi_log_error("{}", qvi_conn_ers());
        return rc;
    }

    rc = qvi_rmi_client_connect(ctx->rmi, url);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Cache pointer to initialized hwloc instance and topology.
    ctx->hwloc = qvi_rmi_client_hwloc_get(ctx->rmi);
out:
    if (url) free(url);
    return rc;
}

int
qv_mpi_create(
    qv_context_t **ctx,
    MPI_Comm comm
) {
    if (!ctx) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;
    cstr ers = nullptr;

    qv_context_t *ictx = nullptr;
    rc = qv_create(&ictx);
    if (rc != QV_SUCCESS) {
        ers = "qv_create() failed";
        goto out;
    }

    qvi_taskman_mpi_t *itaskman;
    rc = qvi_taskman_mpi_new(&itaskman);
    if (rc != QV_SUCCESS) {
        ers = "qvi_taskman_mpi_new() failed";
        goto out;
    }
    // Save taskman instance pointer to context.
    ictx->taskman = itaskman;

    rc = qvi_mpi_init(itaskman->m_mpi, ictx->task, comm);
    if (rc != QV_SUCCESS) {
        ers = "qvi_mpi_init failed";
        goto out;
    }

    rc = connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        ers = "connect_to_server() failed";
        goto out;
    }

    rc = qvi_bind_stack_init(
        ictx->bind_stack,
        ictx->task,
        ictx->hwloc
    );
    if (rc != QV_SUCCESS) {
        ers = "qvi_bind_stack_init() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, qv_strerr(rc));
        qvi_taskman_mpi_free(&itaskman);
        (void)qv_free(ictx);
        ictx = nullptr;
    }
    *ctx = ictx;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
