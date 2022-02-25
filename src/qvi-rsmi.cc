/*
 * Copyright (c) 2021-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-rsmi.cc
 */

#include "qvi-common.h"
#include "qvi-rsmi.h"
#include "qvi-hwloc.h"

#ifdef ROCM_FOUND
#include "hwloc/rsmi.h"
#endif

int
qvi_hwloc_rsmi_get_device_cpuset_by_device_id(
    qvi_hwloc_t *hwl,
    uint32_t devid,
    hwloc_cpuset_t cpuset
) {
#ifndef ROCM_FOUND
    QVI_UNUSED(hwl);
    QVI_UNUSED(devid);
    QVI_UNUSED(cpuset);
    return QV_ERR_NOT_SUPPORTED;
#else
    // Because we rely on facilities that require that the given topology is the
    // system's topology, we just avoid all that by just catching that here.
    if (!qvi_hwloc_topo_is_this_system(hwl)) {
        return qvi_hwloc_bitmap_copy(
            hwloc_topology_get_topology_cpuset(qvi_hwloc_topo_get(hwl)),
            cpuset
        );
    }
    // Else get the real thing.
    int hrc = hwloc_rsmi_get_device_cpuset(
        qvi_hwloc_topo_get(hwl), devid, cpuset
    );
    if (hrc != 0) {
        return QV_ERR_HWLOC;
    }
    return QV_SUCCESS;
#endif
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */