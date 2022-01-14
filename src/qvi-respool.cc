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
 * @file qvi-respool.cc
 *
 * Resource Pool
 */

#include "qvi-common.h"
#include "qvi-respool.h"

struct qvi_respool_s {
    /** The CPUs that are part of this resource pool. */
    hwloc_cpuset_t cpus = nullptr;
};

int
qvi_rmi_client_new(
    qvi_respool_t **rpool
) {
    qvi_respool_t *irpool = qvi_new qvi_respool_t;
    if (!irpool) return QV_ERR_OOR;

    int rc = qvi_hwloc_bitmap_calloc(&irpool->cpus);
    if (rc != QV_SUCCESS) goto out;
out:
    if (rc != QV_SUCCESS) qvi_respool_free(&irpool);
    *rpool = irpool;
    return rc;
}

void
qvi_rmi_client_free(
    qvi_respool_t **rpool
) {
    if (!rpool) return;
    qvi_respool_t *irpool = *rpool;
    if (!irpool) return;
    delete irpool;
    *rpool = nullptr;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 *
 */
