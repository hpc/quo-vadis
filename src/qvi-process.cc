/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-process.cc
 */

#include "qvi-common.h"

#include "qvi-process.h"
#include "qvi-group.h"
#include "qvi-utils.h"

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
};

struct qvi_process_s {
    /** Task associated with this process */
    qvi_task_t *task = nullptr;
    /** Maintains the next available group ID value */
    qvi_process_group_tab_t *group_tab = nullptr;
};

/**
 * Returns the next available group ID.
 */
static int
next_group_tab_id(
    qvi_process_t *,
    qvi_process_group_id_t *gid
) {
    return qvi_group_next_id(gid);
}

int
qvi_process_new(
    qvi_process_t **proc
) {
    int rc = QV_SUCCESS;

    qvi_process_t *iproc = qvi_new qvi_process_t();
    if (!iproc) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Task
    rc = qvi_task_new(&iproc->task);
    if (rc != QV_SUCCESS) goto out;
    // Groups
    iproc->group_tab = qvi_new qvi_process_group_tab_t();
    if (!iproc->group_tab) {
        rc = QV_ERR_OOR;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_process_free(&iproc);
    }
    *proc = iproc;
    return rc;
}

void
qvi_process_free(
    qvi_process_t **proc
) {
    if (!proc) return;
    qvi_process_t *iproc = *proc;
    if (!iproc) goto out;
    if (iproc->group_tab) {
        delete iproc->group_tab;
        iproc->group_tab = nullptr;
    }
    qvi_task_free(&iproc->task);
    delete iproc;
out:
    *proc = nullptr;
}

/**
 *
 */
int
qvi_process_init(
    qvi_process_t *proc
) {
    // For now these are always fixed.
    const int world_id = 0, node_id = 0;
    return qvi_task_init(
        proc->task, QV_TASK_TYPE_PROCESS, getpid(), world_id, node_id
    );
}

int
qvi_process_finalize(
    qvi_process_t *
) {
    return QV_SUCCESS;
}

int
qvi_process_node_barrier(
    qvi_process_t *
) {
    // Nothing to do since process groups contain a single member.
    return QV_SUCCESS;
}

qvi_task_t *
qvi_process_task_get(
    qvi_process_t *proc
) {
    return proc->task;
}

int
qvi_process_group_new(
    qvi_process_group_t **procgrp
) {
    int rc = QV_SUCCESS;

    qvi_process_group_t *iprocgrp = qvi_new qvi_process_group_t();
    if (!iprocgrp) {
        rc = QV_ERR_OOR;
    }
    if (rc != QV_SUCCESS) {
        qvi_process_group_free(&iprocgrp);
    }
    *procgrp = iprocgrp;
    return rc;
}

void
qvi_process_group_free(
    qvi_process_group_t **procgrp
) {
    if (!procgrp) return;
    qvi_process_group_t *iprocgrp = *procgrp;
    if (!iprocgrp) goto out;
    delete iprocgrp;
out:
    *procgrp = nullptr;
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
    igroup->id    = 0;
    igroup->size  = 1;

    proc->group_tab->insert({gtid, *igroup});
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
    assert(root == 0 && group_size == 1);
    if (root != 0 || group_size != 1) {
        return QV_ERR_INTERNAL;
    }

    int rc = QV_SUCCESS;
    int *rxcounts = nullptr;
    byte_t *bytepos = nullptr;
    // Zero initialize array of pointers to nullptr.
    qvi_bbuff_t **bbuffs = qvi_new qvi_bbuff_t *[group_size]();
    if (!bbuffs) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rxcounts = qvi_new int[group_size]();
    if (!rxcounts) {
        rc = QV_ERR_OOR;
        goto out;
    }
    rxcounts[0] = qvi_bbuff_size(txbuff);

    bytepos = (byte_t *)qvi_bbuff_data(txbuff);
    for (int i = 0; i < group_size; ++i) {
        rc = qvi_bbuff_new(&bbuffs[i]);
        if (rc != QV_SUCCESS) break;
        rc = qvi_bbuff_append(bbuffs[i], bytepos, rxcounts[i]);
        if (rc != QV_SUCCESS) break;
        bytepos += rxcounts[i];
    }
out:
    delete[] rxcounts;
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
    assert(root == 0 && group_size == 1);
    if (root != 0 || group_size != 1) {
        return QV_ERR_INTERNAL;
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
