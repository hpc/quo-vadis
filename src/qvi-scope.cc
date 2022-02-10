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
 * @file qvi-scope.cc
 */

#include "qvi-common.h"

#include "qvi-scope.h"
#include "qvi-rmi.h"
#include "qvi-hwpool.h"

// Type definition
struct qv_scope_s {
    /** Pointer to initialized RMI infrastructure. */
    qvi_rmi_client_t *rmi = nullptr;
    /** Task group associated with this scope instance. */
    qvi_group_t *group = nullptr;
    /** Hardware resource pool. */
    qvi_hwpool_t *hwpool = nullptr;
};

int
qvi_scope_new(
    qv_scope_t **scope
) {
    int rc = QV_SUCCESS;

    qv_scope_t *iscope = qvi_new qv_scope_t;
    if (!scope) rc = QV_ERR_OOR;
    // hwpool and group will be initialized in qvi_scope_init().
    if (rc != QV_SUCCESS) qvi_scope_free(&iscope);
    *scope = iscope;
    return rc;
}

void
qvi_scope_free(
    qv_scope_t **scope
) {
    if (!scope) return;
    qv_scope_t *iscope = *scope;
    if (!iscope) goto out;
    qvi_hwpool_free(&iscope->hwpool);
    qvi_group_free(&iscope->group);
    delete iscope;
out:
    *scope = nullptr;
}

static int
scope_init(
    qv_scope_t *scope,
    qvi_rmi_client_t *rmi,
    qvi_group_t *group,
    qvi_hwpool_t *hwpool
) {
    assert(rmi);
    assert(group);
    assert(hwpool);
    if (!rmi || !hwpool || !scope) return QV_ERR_INTERNAL;
    scope->rmi = rmi;
    scope->group = group;
    scope->hwpool = hwpool;
    return QV_SUCCESS;
}

hwloc_const_cpuset_t
qvi_scope_cpuset_get(
    qv_scope_t *scope
) {
    return qvi_hwpool_cpuset_get(scope->hwpool);
}

const qvi_hwpool_t *
qvi_scope_hwpool_get(
    qv_scope_t *scope
) {
    return scope->hwpool;
}

int
qvi_scope_taskid(
    qv_scope_t *scope,
    int *taskid
) {
    *taskid = scope->group->id();
    return QV_SUCCESS;
}

int
qvi_scope_ntasks(
    qv_scope_t *scope,
    int *ntasks
) {
    *ntasks = scope->group->size();
    return QV_SUCCESS;
}

int
qvi_scope_barrier(
    qv_scope_t *scope
) {
    return scope->group->barrier();
}

int
qvi_scope_get(
    qvi_zgroup_t *zgroup,
    qvi_rmi_client_t *rmi,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    int rc = QV_SUCCESS;
    qvi_group_t *group = nullptr;
    qvi_hwpool_t *hwpool = nullptr;
    // Get the requested intrinsic group.
    rc = zgroup->group_create_intrinsic(
        iscope, &group
    );
    if (rc != QV_SUCCESS) goto out;
    // Get the requested intrinsic hardware pool.
    rc = qvi_rmi_scope_get_intrinsic_hwpool(
        rmi,
        qvi_task_pid(zgroup->task()),
        iscope,
        &hwpool
    );
    if (rc != QV_SUCCESS) goto out;
    // Create the scope.
    rc = qvi_scope_new(scope);
    if (rc != QV_SUCCESS) goto out;
    // Initialize the scope.
    rc = scope_init(*scope, rmi, group, hwpool);
out:
    if (rc != QV_SUCCESS) {
        qvi_scope_free(scope);
        qvi_hwpool_free(&hwpool);
        qvi_group_free(&group);
    }
    return rc;
}

// TODO(skg) We should have structures around bbuffs and hwpools.
static int
gather_hwpools(
    qvi_group_t *group,
    int root,
    qvi_hwpool_t *txpool,
    qvi_hwpool_t ***rxpools
) {
    const int group_size = group->size();

    int rc = QV_SUCCESS;
    qvi_bbuff_t *txbuff = nullptr;
    qvi_bbuff_t **bbuffs = nullptr;
    qvi_hwpool_t **hwpools = nullptr;

    rc = qvi_bbuff_new(&txbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_pack(txpool, txbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = group->gather(txbuff, root, &bbuffs);
    if (rc != QV_SUCCESS) goto out;

    if (group->id() == root) {
        hwpools = qvi_new qvi_hwpool_t*[group_size];
        if (!hwpools) {
            rc = QV_ERR_OOR;
            goto out;
        }
        std::fill(hwpools, hwpools + group_size, nullptr);
        // Unpack the hwpools.
        for (int i = 0; i < group_size; ++i) {
            rc = qvi_hwpool_unpack(
                qvi_bbuff_data(bbuffs[i]), &hwpools[i]
            );
            if (rc != QV_SUCCESS) break;
        }
    }
out:
    if (bbuffs) {
        for (int i = 0; i < group_size; ++i) {
            qvi_bbuff_free(&bbuffs[i]);
        }
        delete[] bbuffs;
    }
    qvi_bbuff_free(&txbuff);
    if (rc != QV_SUCCESS) {
        if (hwpools) {
            for (int i = 0; i < group_size; ++i) {
                qvi_hwpool_free(&hwpools[i]);
            }
            delete[] hwpools;
            hwpools = nullptr;
        }
    }
    *rxpools = hwpools;
    return rc;
}

static int
scatter_hwpools(
    qvi_group_t *group,
    int root,
    qvi_hwpool_t **pools,
    qvi_hwpool_t **pool
) {
    const int group_size = group->size();

    int rc = QV_SUCCESS;
    qvi_bbuff_t **txbuffs = nullptr;
    qvi_bbuff_t *rxbuff = nullptr;

    if (root == group->id()) {
        txbuffs = qvi_new qvi_bbuff_t *[group_size];
        if (!txbuffs) {
            rc = QV_ERR_OOR;
            goto out;
        }
        std::fill(txbuffs, txbuffs + group_size, nullptr);
        // Pack the hwpools.
        for (int i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_new(&txbuffs[i]);
            if (rc != QV_SUCCESS) goto out;
            rc = qvi_hwpool_pack(pools[i], txbuffs[i]);
            if (rc != QV_SUCCESS) break;
        }
        if (rc != QV_SUCCESS) goto out;
    }

    rc = group->scatter(txbuffs, root, &rxbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_unpack(qvi_bbuff_data(rxbuff), pool);
out:
    if (txbuffs) {
        for (int i = 0; i < group_size; ++i) {
            qvi_bbuff_free(&txbuffs[i]);
        }
        delete[] txbuffs;
    }
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(pool);
    }
    return rc;
}

// TODO(skg) This is where other split algorithms may either live or take place.
int
qvi_scope_split(
    qv_scope_t *parent,
    int ncolors,
    int color,
    qv_scope_t **child
) {
    int rc = QV_SUCCESS;
    const int root = 0;

    qvi_group_t *group = nullptr;
    qvi_hwpool_t *hwpool = nullptr;
    qvi_hwpool_t **hwpools = nullptr;

    hwloc_bitmap_t cpuset = nullptr;
    rc = qvi_rmi_split_cpuset_by_group_id(
        parent->rmi,
        qvi_hwpool_cpuset_get(parent->hwpool),
        ncolors, color, &cpuset
    );
    if (rc != QV_SUCCESS) goto out;
    // Now that we have the CPU split, create initial hardware pools.
    rc = qvi_hwpool_new(&hwpool);
    if (rc != QV_SUCCESS) goto out;
    // Initialize initial hwpool with split cpuset.
    rc = qvi_hwpool_init(hwpool, cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Gather hwpools so we can split hardware resources.
    rc = gather_hwpools(
        parent->group, root, hwpool, &hwpools
    );
    if (rc != QV_SUCCESS) goto out;
    // No longer needed, as we have consolidated the pools.
    qvi_hwpool_free(&hwpool);
    // TODO(skg) Do the device split.
    // Scatter result of device split.
    rc = scatter_hwpools(
        parent->group, root, hwpools, &hwpool
    );
    if (rc != QV_SUCCESS) goto out;
    // Split underlying group.
    rc = parent->group->split(
        color, parent->group->id(), &group
    );

    rc = qvi_scope_new(child);
    if (rc != QV_SUCCESS) goto out;

    rc = scope_init(
        *child, parent->rmi, group, hwpool
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_scope_free(child);
        qvi_group_free(&group);
        qvi_hwpool_free(&hwpool);
        qvi_hwloc_bitmap_free(&cpuset);
    }
    return rc;
}

int
qvi_scope_nobjs(
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *n
) {
    // TODO(skg) We should update how we do this.
    return qvi_rmi_get_nobjs_in_cpuset(
        scope->rmi,
        obj,
        qvi_hwpool_cpuset_get(scope->hwpool),
        n
    );
}

int
qvi_scope_get_device(
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int i,
    qv_device_id_type_t id_type,
    char **dev_id
) {
    // TODO(skg) Update how we do this.
    return qvi_rmi_get_device_in_cpuset(
        scope->rmi,
        dev_obj,
        i,
        qvi_hwpool_cpuset_get(scope->hwpool),
        id_type,
        dev_id
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
