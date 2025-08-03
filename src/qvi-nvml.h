/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-nvml.h
 */

#ifndef QVI_NVML_H
#define QVI_NVML_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

int
qvi_hwloc_nvml_get_device_cpuset_by_pci_bus_id(
    qvi_hwloc *hwl,
    const std::string &uuid,
    qvi_hwloc_bitmap &cpuset
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
