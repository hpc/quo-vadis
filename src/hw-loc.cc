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

#include "quo-vadis/config.h"
#include "quo-vadis/common.h"
#include "quo-vadis/logger.h"
#include "quo-vadis/hw-loc.h"


#include "hwloc.h"

// Type definition
struct qvi_hwloc_t {
    /** The topology. */
    hwloc_topology_t topo;
};

int
qvi_hwloc_construct(
    qvi_hwloc_t **hwl
) {
    if (!hwl) return EINVAL;

    *hwl = new qvi_hwloc_t;

    return QV_SUCCESS;
}

int
qvi_hwloc_init(
    qvi_hwloc_t *hw
) {
    char const*ers = nullptr;
    int rc = 0;

    rc = hwloc_topology_init(&(hw->topo));
    if (rc != 0) {
        ers = "hwloc_topology_init failed";
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
