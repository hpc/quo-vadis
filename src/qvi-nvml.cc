/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-nvml.cc
 */

#include "qvi-nvml.h" // IWYU pragma: keep
#include "qvi-hwloc.h"

#ifdef CUDAToolkit_FOUND
#include "hwloc/nvml.h"
#endif

int
qvi_hwloc_nvml_get_device_cpuset_by_pci_bus_id(
    qvi_hwloc_t *hwl,
    const std::string &uuid,
    qvi_hwloc_bitmap_s &cpuset
) {
#ifndef CUDAToolkit_FOUND
    QVI_UNUSED(hwl);
    QVI_UNUSED(uuid);
    QVI_UNUSED(cpuset);
    return QV_ERR_NOT_SUPPORTED;
#else
    int rc = QV_SUCCESS;
    // Because we rely on facilities that require that the given topology is the
    // system's topology, we just avoid all that by just catching that here.
    if (!qvi_hwloc_topo_is_this_system(hwl)) {
        return qvi_hwloc_bitmap_copy(
            hwloc_topology_get_topology_cpuset(qvi_hwloc_topo_get(hwl)),
            cpuset.data
        );
    }
    // This method should be called once before invoking any other methods in
    // the library. A reference count of the number of initializations is
    // maintained. Shutdown only occurs when the reference count reaches zero.
    // Initialize NVML, but don't initialize any GPUs yet.
    nvmlReturn_t nvrc = nvmlInit_v2();
    if (nvrc != NVML_SUCCESS) return QV_ERR_HWLOC;
    // Starting from NVML 5, this API causes NVML to initialize the target GPU
    // NVML may initialize additional GPUs if the target GPU is an SLI slave.
    nvmlDevice_t device;
    nvrc = nvmlDeviceGetHandleByPciBusId_v2(uuid.c_str(), &device);
    if (nvrc != NVML_SUCCESS) {
        rc = QV_ERR_HWLOC;
        goto out;
    }

    const int hwrc = hwloc_nvml_get_device_cpuset(
        qvi_hwloc_topo_get(hwl), device, cpuset.data
    );
    if (hwrc != 0) {
        rc = QV_ERR_HWLOC;
    }
out:
    // These are reference counted, so always match with our call to nvmlInit.
    (void)nvmlShutdown();
    return rc;
#endif
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
