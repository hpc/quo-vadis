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

#include "private/common.h"
#include "private/logger.h"

#include "quo-vadis/hw-server.h"
#include "quo-vadis/hw-loc.h"

/** qv_hw_server_t type definition */
struct qv_hw_server_s {
    qv_hwloc_t *qv_hwloc = nullptr;
};

int
qv_hw_server_construct(
    qv_hw_server_t **hws
) {
    if (!hws) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    qv_hw_server_t *ihws = (qv_hw_server_t *)calloc(1, sizeof(*ihws));
    if (!ihws) {
        QVI_LOG_ERROR("calloc() failed");
        return QV_ERR_OOR;
    }

    int rc = qv_hwloc_construct(&ihws->qv_hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_construct() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={}", ers, rc);
        qv_hw_server_destruct(ihws);
        return rc;
    }
    *hws = ihws;
    return QV_SUCCESS;
}

void
qv_hw_server_destruct(
    qv_hw_server_t *hws
) {
    if (!hws) return;

    qv_hwloc_destruct(hws->qv_hwloc);
    free(hws);
}

int
qv_hw_server_init(
    qv_hw_server_t *hws
) {
    if (!hws) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;

    int rc = qv_hwloc_init(hws->qv_hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_init() failed";
        goto out;
    }

    rc = qv_hwloc_topo_load(hws->qv_hwloc);
    if (rc != QV_SUCCESS) {
        ers = "qv_hwloc_topo_load() failed";
        goto out;
    }
out:
    if (ers) {
        QVI_LOG_ERROR("{} with rc={}", ers, rc);
        return rc;
    }
    return QV_SUCCESS;
}

int
qv_hw_server_finalize(
    qv_hw_server_t *hws
) {
    if (!hws) return QV_ERR_INVLD_ARG;

    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
