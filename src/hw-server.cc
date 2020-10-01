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
 * @file hw-server.cc
 */

#include "quo-vadis/common.h"
#include "quo-vadis/hw-server.h"
#include "quo-vadis/hw-loc.h"

#include "private/logger.h"

// Type definition
struct qvi_hw_server_t {
    qvi_hwloc_t *qvi_hwloc;
};

int
qvi_hw_server_construct(
    qvi_hw_server_t **hws
) {
    if (!hws) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    qvi_hw_server_t *ihws = (qvi_hw_server_t *)calloc(1, sizeof(*ihws));
    if (!ihws) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }

    int rc = qvi_hwloc_construct(&ihws->qvi_hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_construct() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={}", ers, rc);
        qvi_hw_server_destruct(ihws);
        return rc;
    }
    *hws = ihws;
    return QV_SUCCESS;
}

void
qvi_hw_server_destruct(
    qvi_hw_server_t *hws
) {
    if (!hws) return;

    qvi_hwloc_destruct(hws->qvi_hwloc);
    free(hws);
}

int
qvi_hw_server_init(
    qvi_hw_server_t *hws
) {
    if (!hws) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    int rc = qvi_hwloc_init(hws->qvi_hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_init() failed";
        goto out;
    }

    rc = qvi_hwloc_topo_load(hws->qvi_hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qvi_hwloc_topo_load() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={}", ers, rc);
        return rc;
    }
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
