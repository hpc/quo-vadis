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
 * @file qv-group.h
 */

#ifndef QUO_VADIS_GROUP_H
#define QUO_VADIS_GROUP_H

#ifdef __cplusplus
extern "C" {
#endif

struct qv_group_s;
typedef struct qv_group_s qv_group_t;

/**
 *
 */
int
qv_group_create(
    qv_group_t *group
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
