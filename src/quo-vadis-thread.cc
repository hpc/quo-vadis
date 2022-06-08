/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis-thread.cc
 */

#include "qvi-common.h"

#include "quo-vadis-thread.h"

#include "qvi-context.h"
#include "qvi-zgroup-thread.h"
#include "qvi-group-thread.h"

int
qv_thread_context_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    delete ctx->zgroup;
    qvi_context_free(&ctx);
    return QV_SUCCESS;
}

int
qv_thread_context_create(
    qv_context_t **ctx
) {
    if (!ctx) {
        return QV_ERR_INVLD_ARG;
    }

    int rc = QV_SUCCESS;
    qv_context_t *ictx = nullptr;
    qvi_zgroup_thread_t *izgroup = nullptr;
    // Create base context.
    rc = qvi_context_create(&ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Create and initialize the base group.
    rc = qvi_zgroup_thread_new(&izgroup);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Save zgroup instance pointer to context.
    ictx->zgroup = izgroup;

    rc = izgroup->initialize();
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Connect to RMI server.
    rc = qvi_context_connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    rc = qvi_bind_stack_init(
        ictx->bind_stack,
        qvi_thread_task_get(izgroup->zth),
        ictx->rmi
    );
out:
    if (rc != QV_SUCCESS) {
        (void)qv_thread_context_free(ictx);
        ictx = nullptr;
    }
    *ctx = ictx;
    return rc;
}

int
qv_thread_mgmt_toto(
   void
){

#ifdef _OPENMP
#warning "OPENMP support ON"
  fprintf(stdout,"Hello from OpenMP support\n");
#else
#warning "OPENMP support OFF"
  fprintf(stdout,"Hello\n");
#endif
  return QV_SUCCESS;
}


/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
