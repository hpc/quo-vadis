/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis-process.h
 */

#ifndef QUO_VADIS_PROCESS_H
#define QUO_VADIS_PROCESS_H

#include "quo-vadis.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a process context.
 */
int
qv_process_context_create(
    qv_context_t **ctx
);

/**
 * Frees resources associated with a context created by
 * qv_process_context_create().
 */
int
qv_process_context_free(
    qv_context_t *ctx
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
