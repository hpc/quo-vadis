/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-rsmi.h
 */

#ifndef QVI_RSMI_H
#define QVI_RSMI_H

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-hwloc.h"

int
qvi_hwloc_rsmi_get_device_cpuset_by_device_id(
    qvi_hwloc_t *hwl,
    uint32_t devid,
    qvi_hwloc_bitmap &cpuset
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
