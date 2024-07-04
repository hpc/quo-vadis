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
#include "qvi-task.h"
#include "qvi-utils.h"
#include "qvi-group.h"

// Type definitions.
typedef qvi_group_id_t qvi_process_group_id_t;

using qvi_process_group_tab_t = std::unordered_map<
    qvi_process_group_id_t, qvi_process_group_t
>;

struct qvi_process_group_s {
    /** ID used for table lookups */
    qvi_process_group_id_t tabid = 0;
    /** ID (rank) in group */
    int id = 0;
    /** Size of group */
    int size = 0;
    /** Constructor. */
    qvi_process_group_s(void) = default;
    /** Destructor. */
    ~qvi_process_group_s(void) = default;
};

struct qvi_process_s {
    /** Maintains the next available group ID value. */
    qvi_process_group_tab_t group_tab;
    /** Constructor. */
    qvi_process_s(void) = default;
    /** Destructor. */
    ~qvi_process_s(void) = default;
};

/**
 * Returns the next available group ID.
 */
static int
next_group_tab_id(
    qvi_process_t *,
    qvi_process_group_id_t *gid
) {
    return qvi_group_t::next_id(gid);
}

int
qvi_process_new(
    qvi_process_t **proc
) {
    return qvi_new(proc);
}

void
qvi_process_free(
    qvi_process_t **proc
) {
    return qvi_delete(proc);
}

int
qvi_process_node_barrier(
    qvi_process_t *
) {
    // Nothing to do since process groups contain a single member.
    return QV_SUCCESS;
}

int
qvi_process_group_new(
    qvi_process_group_t **procgrp
) {
    return qvi_new(procgrp);
}

void
qvi_process_group_free(
    qvi_process_group_t **procgrp
) {
    qvi_delete(procgrp);
}

int
qvi_process_group_create(
    qvi_process_t *proc,
    qvi_process_group_t **group
) {
    qvi_process_group_t *igroup = nullptr;
    qvi_process_group_id_t gtid;

    int rc = next_group_tab_id(proc, &gtid);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_process_group_new(&igroup);
    if (rc != QV_SUCCESS) goto out;

    igroup->tabid = gtid;
    igroup->id = 0;
    igroup->size = 1;

    proc->group_tab.insert({gtid, *igroup});
out:
    if (rc != QV_SUCCESS) {
        qvi_process_group_free(&igroup);
    }
    *group = igroup;
    return QV_SUCCESS;
}

int
qvi_process_group_id(
    const qvi_process_group_t *group
) {
    return group->id;
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
    qvi_bbuff_t ***rxbuffs,
    int *shared_alloc
) {
    const int group_size = qvi_process_group_size(group);
    // Make sure that we are dealing with a valid process group.
    // If not, this is an internal development error, so abort.
    if (root != 0 || group_size != 1) {
        qvi_abort();
    }

    int rc = QV_SUCCESS;
    std::vector<size_t> rxcounts = {qvi_bbuff_size(txbuff)};
    // Zero initialize array of pointers to nullptr.
    qvi_bbuff_t **bbuffs = new qvi_bbuff_t *[group_size]();

    byte_t *bytepos = (byte_t *)qvi_bbuff_data(txbuff);
    for (int i = 0; i < group_size; ++i) {
        rc = qvi_bbuff_new(&bbuffs[i]);
        if (rc != QV_SUCCESS) break;
        rc = qvi_bbuff_append(bbuffs[i], bytepos, rxcounts[i]);
        if (rc != QV_SUCCESS) break;
        bytepos += rxcounts[i];
    }
    if (rc != QV_SUCCESS) {
        if (bbuffs) {
            for (int i = 0; i < group_size; ++i) {
                qvi_bbuff_free(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
        bbuffs = nullptr;
    }
    *rxbuffs = bbuffs;
    *shared_alloc = 0;
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
    qvi_bbuff_t *inbuff = txbuffs[root];
    const size_t data_size = qvi_bbuff_size(inbuff);
    const void *data = qvi_bbuff_data(inbuff);

    qvi_bbuff_t *mybbuff = nullptr;
    int rc = qvi_bbuff_new(&mybbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_bbuff_append(mybbuff, data, data_size);
out:
    if (rc != QV_SUCCESS) {
        qvi_bbuff_free(&mybbuff);
    }
    *rxbuff = mybbuff;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
