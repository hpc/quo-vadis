/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-rsmi.cc
 */

#include "qvi-rsmi.h"
#include "qvi-hwloc.h"

#ifdef ROCmSMI_FOUND
#include "rocm_smi/rocm_smi.h"
#include "hwloc/rsmi.h" // IWYU pragma: keep
#endif

int
qvi_hwloc_rsmi_get_device_cpuset_by_device_id(
    qvi_hwloc_t *hwl,
    uint32_t devid,
    qvi_hwloc_bitmap &cpuset
) {
#ifndef ROCmSMI_FOUND
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
            cpuset.data()
        );
    }
    // Else get the real thing.
    int rc = QV_SUCCESS, hrc = 0;

    rsmi_status_t rsmi_rc = rsmi_init(0);
    if (rsmi_rc != RSMI_STATUS_SUCCESS) {
        qvi_log_error("rsmi_init() failed");
        rc = QV_ERR_HWLOC;
        goto out;
    }

    hrc = hwloc_rsmi_get_device_cpuset(
        qvi_hwloc_topo_get(hwl), devid, cpuset.data()
    );
    if (hrc != 0) {
        rc = QV_ERR_HWLOC;
    }
out:
    rsmi_shut_down();
    return rc;
#endif
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
