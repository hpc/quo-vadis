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
struct qvi_hw_server_t;
typedef struct qvi_hw_server_t qvi_hw_server_t;

/**
 *
 */
int
qvi_hw_server_construct(
    qvi_hw_server_t **hws
);

/**
 *
 */
void
qvi_hw_server_destruct(
    qvi_hw_server_t *hws
);

/**
 *
 */
int
qvi_hw_server_init(
    qvi_hw_server_t *hws
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
