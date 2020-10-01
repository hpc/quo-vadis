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
 * @file hw-hwloc.cc
 */

#include "quo-vadis/common.h"
#include "quo-vadis/hw-loc.h"

#include "private/logger.h"

#include "hwloc.h"

// Type definition
struct qvi_hwloc_t {
    /** The node topology. */
    hwloc_topology_t topo;
};

int
qvi_hwloc_construct(
    qvi_hwloc_t **hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    qvi_hwloc_t *ihwl = (qvi_hwloc_t *)calloc(1, sizeof(*ihwl));
    if (!ihwl) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }
    *hwl = ihwl;

    return QV_SUCCESS;
}

void
qvi_hwloc_destruct(
    qvi_hwloc_t *hwl
) {
    if (!hwl) return;

    hwloc_topology_destroy(hwl->topo);
    free(hwl);
}

int
qvi_hwloc_init(
    qvi_hwloc_t *hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    int rc = hwloc_topology_init(&hwl->topo);
    if (rc != 0) {
        ers = "hwloc_topology_init() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={}", ers, rc);
        return QV_ERR_TOPO;
    }
    return QV_SUCCESS;
}

int
qvi_hwloc_topo_load(
    qvi_hwloc_t *hwl
) {
    if (!hwl) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    /* Set flags that influence hwloc's behavior. */
    static const unsigned int flags = HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM;
    int rc = hwloc_topology_set_flags(hwl->topo, flags);
    if (rc != 0) {
        ers = "hwloc_topology_set_flags() failed";
        goto out;
    }

    rc = hwloc_topology_set_all_types_filter(
        hwl->topo,
        HWLOC_TYPE_FILTER_KEEP_ALL
    );
    if (rc != 0) {
        ers = "hwloc_topology_set_all_types_filter() failed";
        goto out;
    }

    rc = hwloc_topology_set_io_types_filter(
        hwl->topo,
        HWLOC_TYPE_FILTER_KEEP_IMPORTANT
    );
    if (rc != 0) {
        ers = "hwloc_topology_set_io_types_filter() failed";
        goto out;
    }

    rc = hwloc_topology_load(hwl->topo);
    if (rc != 0) {
        ers = "hwloc_topology_load() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={}", ers, rc);
        return QV_ERR_TOPO;
    }
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
