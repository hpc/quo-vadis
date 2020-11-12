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
 * @file qv-scope.h
 */

#ifndef QUO_VADIS_SCOPE_H
#define QUO_VADIS_SCOPE_H

#ifdef __cplusplus
extern "C" {
#endif

struct qv_scope_s;
typedef struct qv_scope_s qv_scope_t;

/**
 *
 */
int
qv_scope_split(
    qv_scope_t *scope
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
