/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwrespool.cc
 *
 * Hwardware Resource Pool
 */

#include "qvi-common.h"
#include "qvi-hwrespool.h"

using qvi_resource_tab_t = std::unordered_map<qv_hw_obj_type_t, qvi_hwres_t *>;

struct qvi_hwrespool_s {
    /** The table of hardware resources that are part of this resource pool. */
    qvi_resource_tab_t *restab = nullptr;
};

int
qvi_hwrespool_new(
    qvi_hwrespool_t **rpool
) {
    int rc = QV_SUCCESS;

    qvi_hwrespool_t *irpool = qvi_new qvi_hwrespool_t;
    if (!irpool) {
        rc = QV_ERR_OOR;
        goto out;
    }

    irpool->restab = qvi_new qvi_resource_tab_t;
    if (!irpool->restab) {
        rc = QV_ERR_OOR;
        goto out;
    }

    qvi_hwres_t *cpus;
    rc = qvi_hwres_new(&cpus, QV_HW_OBJ_MACHINE);
    if (rc != QV_SUCCESS) goto out;

    qvi_hwres_t *gpus;
    rc = qvi_hwres_new(&gpus, QV_HW_OBJ_GPU);
    if (rc != QV_SUCCESS) goto out;

    // Initialize the resource table.
    irpool->restab->insert({QV_HW_OBJ_MACHINE, cpus});
    irpool->restab->insert({QV_HW_OBJ_GPU, gpus});
out:
    if (rc != QV_SUCCESS) qvi_hwrespool_free(&irpool);
    *rpool = irpool;
    return rc;
}

void
qvi_hwrespool_free(
    qvi_hwrespool_t **rpool
) {
    if (!rpool) return;
    qvi_hwrespool_t *irpool = *rpool;
    if (!irpool) goto out;
    if (irpool->restab) {
        for (auto &i : *irpool->restab) {
            qvi_hwres_t *res = i.second;
            qvi_hwres_free(&res);
        }
        delete irpool->restab;
    }
    delete irpool;
out:
    *rpool = nullptr;
}

int
qvi_hwrespool_add(
    qvi_hwrespool_t *rpool,
    qvi_hwres_t *res
) {
    qv_hw_obj_type_t type = qvi_hwres_type(res);

    auto got = rpool->restab->find(type);
    if (got == rpool->restab->end()) {
        return QV_ERR_NOT_FOUND;
    }

    return qvi_hwres_add(got->second, res);
}

int
qvi_hwrespool_remove(
    qvi_hwrespool_t *rpool,
    qvi_hwres_t *res
) {
    qv_hw_obj_type_t type = qvi_hwres_type(res);

    auto got = rpool->restab->find(type);
    if (got == rpool->restab->end()) {
        return QV_ERR_NOT_FOUND;
    }

    return qvi_hwres_remove(got->second, res);
}


// TODO(skg) Perhaps add a return resources call? Would this replace remove?

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */
