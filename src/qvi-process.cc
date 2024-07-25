/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-process.cc
 */

#include "qvi-process.h"
#include "qvi-bbuff.h"
#include "qvi-utils.h"

struct qvi_process_group_s {
    /** Size of group. This is fixed. */
    static constexpr int size = 1;
    /** ID (rank) in group. This is fixed. */
    static constexpr int rank = 0;
};

int
qvi_process_group_new(
    qvi_process_group_t **procgrp
) {
    return qvi_new(procgrp);
}

void
qvi_process_group_delete(
    qvi_process_group_t **procgrp
) {
    qvi_delete(procgrp);
}

int
qvi_process_group_id(
    const qvi_process_group_t *group
) {
    return group->rank;
}

int
qvi_process_group_size(
    const qvi_process_group_t *group
) {
    return group->size;
}

int
qvi_process_group_barrier(
    qvi_process_group_t *
) {
    // Nothing to do since process groups contain a single member.
    return QV_SUCCESS;
}

int
qvi_process_group_gather_bbuffs(
    qvi_process_group_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    bool *shared,
    qvi_bbuff_t ***rxbuffs
) {
    const int group_size = qvi_process_group_size(group);
    // Make sure that we are dealing with a valid process group.
    // If not, this is an internal development error, so abort.
    if (root != 0 || group_size != 1) {
        qvi_abort();
    }
    // Zero initialize array of pointers to nullptr.
    qvi_bbuff_t **bbuffs = new qvi_bbuff_t *[group_size]();

    const int rc = qvi_bbuff_dup(*txbuff, &bbuffs[0]);
    if (rc != QV_SUCCESS) {
        if (bbuffs) {
            qvi_bbuff_delete(&bbuffs[0]);
            delete[] bbuffs;
        }
        bbuffs = nullptr;
    }
    *rxbuffs = bbuffs;
    *shared = false;
    return rc;
}

int
qvi_process_group_scatter_bbuffs(
    qvi_process_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
) {
    const int group_size = qvi_process_group_size(group);
    // Make sure that we are dealing with a valid process group.
    // If not, this is an internal development error, so abort.
    if (root != 0 || group_size != 1) {
        qvi_abort();
    }
    // There should always be only one at the root (us).
    qvi_bbuff_t *mybbuff = nullptr;
    const int rc = qvi_bbuff_dup(*txbuffs[root], &mybbuff);
    if (rc != QV_SUCCESS) {
        qvi_bbuff_delete(&mybbuff);
    }
    *rxbuff = mybbuff;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
