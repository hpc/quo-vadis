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
 * @file qvi-context.h
 */

#ifndef QVI_CONTEXT_H
#define QVI_CONTEXT_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-rmi.h"
#include "qvi-bind.h"
#include "qvi-group.h"
#include "qvi-utils.h"

/**
 * The underlying data structure that defines an ultimately opaque QV context.
 */
struct qv_context_s {
    /** The context-level mutex. */
    std::mutex mutex;
    /** Client-side connection to the RMI. */
    qvi_rmi_client_t *rmi = nullptr;
    /** The bind stack. */
    qvi_bind_stack_t *bind_stack = nullptr;
    /**
     * Zeroth group used for bootstrapping operations that may require
     * group-level participation from the tasks composing the context.
     */
    qvi_group_t *zgroup = nullptr;
    /** Constructor. */
    qv_context_s(void)
    {
        int rc = qvi_rmi_client_new(&rmi);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();

        rc = qvi_bind_stack_new(&bind_stack);
        if (rc != QV_SUCCESS) throw qvi_runtime_error();

        // The zgroup is polymorphic and created in infrastructure-specific
        // context create functions like qvi_mpi_context_create().
    }
    /** Destructor. */
    ~qv_context_s(void)
    {
        qvi_bind_stack_free(&bind_stack);
        qvi_delete(&zgroup);
        qvi_rmi_client_free(&rmi);
    }
};

/**
 *
 */
int
qvi_context_connect_to_server(
    qv_context_t *ctx
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
