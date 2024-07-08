/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-process.h
 */

#ifndef QVI_PROCESS_H
#define QVI_PROCESS_H

#include "qvi-common.h"

// Forward declarations.
struct qvi_process_group_s;
typedef struct qvi_process_group_s qvi_process_group_t;

/**
 *
 */
int
qvi_process_group_new(
    qvi_process_group_t **group
);

/**
 *
 */
void
qvi_process_group_free(
    qvi_process_group_t **group
);

/**
 *
 */
int
qvi_process_group_size(
    const qvi_process_group_t *group
);

/**
 *
 */
int
qvi_process_group_id(
    const qvi_process_group_t *group
);

/**
 *
 */
int
qvi_process_group_barrier(
    qvi_process_group_t *group
);

/**
 *
 */
int
qvi_process_group_gather_bbuffs(
    qvi_process_group_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs,
    int *shared_alloc
);

/**
 *
 */
int
qvi_process_group_scatter_bbuffs(
    qvi_process_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
