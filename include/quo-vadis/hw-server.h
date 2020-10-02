/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file hw-server.h
 */

#ifndef QUO_VADIS_HW_SERVER_H
#define QUO_VADIS_HW_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qv_hw_server_s;
typedef struct qv_hw_server_s qv_hw_server_t;

/**
 *
 */
int
qv_hw_server_construct(
    qv_hw_server_t **hws
);

/**
 *
 */
void
qv_hw_server_destruct(
    qv_hw_server_t *hws
);

/**
 *
 */
int
qv_hw_server_init(
    qv_hw_server_t *hws
);

/**
 *
 */
int
qv_hw_server_finalize(
    qv_hw_server_t *hws
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
