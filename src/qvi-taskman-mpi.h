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
 * @file qvi-taskman-mpi.h
 *
 * MPI process management interface.
 */

#ifndef QVI_TASKMAN_MPI_H
#define QVI_TASKMAN_MPI_H

#include "qvi-common.h"

#include "qvi-taskman.h"
#include "qvi-mpi.h"

struct qvi_group_mpi_s : public qvi_group_s {
    qvi_mpi_group_t *mpi_group = nullptr;

    qvi_group_mpi_s(void) = default;

    virtual
    ~qvi_group_mpi_s(void)
    {
        qvi_mpi_group_free(&mpi_group);
    }

    virtual int
    initialize(void)
    {
        return qvi_mpi_group_new(&mpi_group);
    }

    virtual int
    id(void)
    {
        return qvi_mpi_group_id(mpi_group);
    }

    virtual int
    size(void)
    {
        return qvi_mpi_group_size(mpi_group);
    }

    virtual int
    barrier(void)
    {
        return qvi_mpi_group_barrier(mpi_group);
    }
};
typedef qvi_group_mpi_s qvi_group_mpi_t;

/**
 *
 */
struct qvi_taskman_mpi_s : public qvi_taskman_s {
    /** Internal qvi_mpi instance. */
    qvi_mpi_t *mpi = nullptr;

    /** Constructor that does minimal work. */
    qvi_taskman_mpi_s(void) = default;

    /** The real 'constructor' that can possibly fail. */
    int
    initialize(void)
    {
        return qvi_mpi_new(&mpi);
    }

    virtual
    ~qvi_taskman_mpi_s(void)
    {
        qvi_mpi_free(&mpi);
    }

    int
    group_create_from_intrinsic_scope(
        qv_scope_intrinsic_t scope,
        qvi_group_mpi_t **group
    ) {
        int rc = QV_SUCCESS;

        qvi_group_mpi_t *igroup = new qvi_group_mpi_t;
        if (!igroup) {
            rc = QV_ERR_OOR;
            goto out;
        }

        qvi_mpi_group_id_t mpi_group;
        // TODO(skg) Finish implementation.
        switch (scope) {
            case QV_SCOPE_SYSTEM:
            case QV_SCOPE_USER:
            case QV_SCOPE_JOB:
                mpi_group = QVI_MPI_GROUP_NODE;
                break;
            case QV_SCOPE_PROCESS:
                mpi_group = QVI_MPI_GROUP_SELF;
                break;
            default:
                rc = QV_ERR_INVLD_ARG;
                break;
        }
        if (rc != QV_SUCCESS) goto out;

        rc = qvi_mpi_group_create_from_group_id(
            mpi,
            mpi_group,
            &igroup->mpi_group
        );
    out:
        if (rc != QV_SUCCESS) {
            group_free(igroup);
            igroup = nullptr;
        }
        *group = igroup;
        return rc;
    }

    int
    create_new_subgroup_by_color(
        qvi_group_t *parent,
        int color,
        qvi_group_t **child
    ) {
        const int split_key = qvi_group_id(parent);

        return group_create_from_split(
            parent,
            color,
            split_key,
            child
        );
    }

    virtual int
    scope_create_from_intrinsic(
        qvi_rmi_client_t *rmi,
        qv_scope_intrinsic_t intrinsic,
        qv_scope_t **scope
    ) {
        qvi_group_mpi_t *group = nullptr;
        qv_scope_t *iscope = nullptr;
        qvi_hwpool_t *hwpool = nullptr;

        int rc = qvi_scope_new(&iscope);
        if (rc != QV_SUCCESS) goto out;

        rc = group_create_from_intrinsic_scope(
            intrinsic,
            &group
        );
        if (rc != QV_SUCCESS) goto out;

        rc = qvi_rmi_scope_get_intrinsic_scope_hwpool(
            rmi,
            qvi_mpi_task_pid_get(mpi),
            intrinsic,
            &hwpool
        );
        if (rc != QV_SUCCESS) goto out;

        rc = qvi_scope_init(iscope, group, hwpool);
    out:
        if (rc != QV_SUCCESS) {
            group_free(group);
            qvi_hwpool_free(&hwpool);
            qvi_scope_free(&iscope);
        }
        *scope = iscope;
        return rc;
    }

    int
    group_create_from_split(
        qvi_group_t *parent,
        int color,
        int key,
        qvi_group_t **child
    ) {
        int rc = QV_SUCCESS;

        qvi_group_mpi_t *ichild = new qvi_group_mpi_t;
        if (!ichild) {
            rc = QV_ERR_OOR;
            goto out;
        }

        rc = qvi_mpi_group_create_from_split(
            mpi,
            static_cast<qvi_group_mpi_t *>(parent)->mpi_group,
            color,
            key,
            &ichild->mpi_group
        );
    out:
        if (rc != QV_SUCCESS) {
            group_free(ichild);
            ichild = nullptr;
        }
        *child = ichild;
        return rc;
    }

    // The algorithm needs
    //
    // qvi_hwpool_t *from,
    // qvi_hwloc_t *hwloc,
    // int npieces,
    // int group_id,
    // split cpusets (all of them)
    //  * we can do this in parallel
    // qvi_hwpool_t *to
    // TODO(skg) Lots of work needed
    virtual int
    scope_create_from_split(
        qvi_hwloc_t *hwloc,
        qvi_rmi_client_t *,
        qv_scope_t *parent,
        int ncolors,
        int color,
        qv_scope_t **child
    ) {
        int rc = QV_SUCCESS;
        const int root = 0;
        qvi_group_t *parent_group = qvi_scope_group_get(parent);
        qv_scope_t *child_scope = nullptr;
        qvi_group_t *child_group = nullptr;
        const qvi_hwpool_t *parent_hwpool = qvi_scope_hwpool_get(parent);
        qvi_hwpool_t *child_hwpool = nullptr;

        // TODO(skg) We should have an interface for different splitting
        // algorithms.
        hwloc_bitmap_t split_cpuset = nullptr;
        rc = qvi_hwloc_split_cpuset_by_group_id(
            hwloc,
            qvi_scope_cpuset_get(parent),
            ncolors,
            color,
            &split_cpuset
        );
        if (rc != QV_SUCCESS) return rc;

        qvi_hwpool_new(&child_hwpool);
        if (rc != QV_SUCCESS) return rc;
        rc = qvi_hwpool_init(child_hwpool, split_cpuset);
        if (rc != QV_SUCCESS) return rc;

        qvi_bbuff_t *mybbuff = nullptr;
        rc = qvi_bbuff_new(&mybbuff);
        if (rc != QV_SUCCESS) return rc;
        // TODO(skg) What about the devices?
        rc = qvi_hwpool_pack(child_hwpool, mybbuff);
        if (rc != QV_SUCCESS) return rc;

        qvi_bbuff_t **bbuffs;
        rc = qvi_mpi_group_gather_bbuffs(
            static_cast<qvi_group_mpi_t *>(parent_group)->mpi_group,
            mybbuff, root, &bbuffs
        );
        if (rc != QV_SUCCESS) goto out;

        if (qvi_group_id(parent_group) == root) {
            qvi_hwpool_t **hwpools = nullptr;
            hwpools = qvi_new qvi_hwpool_t*[qvi_group_size(parent_group)];

            for (int i = 0; i < qvi_group_size(parent_group); ++i) {
                qvi_hwpool_t *hp;
                qvi_hwpool_unpack(qvi_bbuff_data(bbuffs[i]), &hp);
                hwpools[i] = hp;
            }
            // TODO(skg) We need to update this function call to include device
            // info from the parent.
            // Root calculates the device split.
            rc = qvi_hwpool_split_devices(
                hwpools,
                qvi_group_size(parent_group),
                hwloc,
                ncolors,
                color
            );
            if (rc != QV_SUCCESS) goto out;
        }
        // Scatter the result.

        // Create new sub-group to get participants.
        rc = create_new_subgroup_by_color(
            qvi_scope_group_get(parent),
            color,
            &child_group
        );
        if (rc != QV_SUCCESS) goto out;
        // Create new sub-scope.
        rc = qvi_scope_new(&child_scope);
        if (rc != QV_SUCCESS) goto out;
        // Initialize new sub-scope.
        rc = qvi_scope_init(child_scope, child_group, child_hwpool);
        if (rc != QV_SUCCESS) goto out;
    out:
        qvi_hwloc_bitmap_free(&split_cpuset);
        if (rc != QV_SUCCESS) {
            qvi_group_free(&child_group);
            qvi_scope_free(&child_scope);
        }
        *child = child_scope;
        return rc;
    }

    virtual int
    barrier(void)
    {
        return qvi_mpi_node_barrier(mpi);
    }

    virtual void
    group_free(
        qvi_group_t *group
    ) {
        qvi_group_free(&group);
    }

};
typedef qvi_taskman_mpi_s qvi_taskman_mpi_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
inline void
qvi_taskman_mpi_free(
    qvi_taskman_mpi_t **taskman
) {
    if (!taskman) return;
    qvi_taskman_mpi_t *itm = *taskman;
    if (!itm) goto out;
    delete itm;
out:
    *taskman = nullptr;
}

/**
 *
 */
inline int
qvi_taskman_mpi_new(
    qvi_taskman_mpi_t **taskman
) {
    int rc = QV_SUCCESS;

    qvi_taskman_mpi_t *itm = qvi_new qvi_taskman_mpi_t;
    if (!itm) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = itm->initialize();
out:
    if (rc != QV_SUCCESS) {
        qvi_taskman_mpi_free(&itm);
    }
    *taskman = itm;
    return rc;
}

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
