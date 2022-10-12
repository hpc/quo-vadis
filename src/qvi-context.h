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
 * @file qvi-context.h
 *
 * @note This file breaks our convention by defining a struct within a header.
 * There is good reason for this: we want to hide the implementation details of
 * QV contexts, but require its definition in a header that is accessible to
 * multiple internal consumers.
 */

#ifndef QVI_CONTEXT_H
#define QVI_CONTEXT_H

#include "qvi-common.h"

#include "qvi-rmi.h"
#include "qvi-zgroup.h"
#include "qvi-bind.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The underlying data structure that defines an ultimately opaque QV context.
 */
struct qv_context_s {
    qvi_rmi_client_t *rmi = nullptr;
    qvi_zgroup_t *zgroup = nullptr;
    qvi_bind_stack_t *bind_stack = nullptr;
};

/**
 *
 */
int
qvi_context_new(
    qv_context_t **ctx
);

/**
 *
 */
void
qvi_context_free(
    qv_context_t **ctx
);

/**
 *
 */
int
qvi_context_connect_to_server(
    qv_context_t *ctx
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
