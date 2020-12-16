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
 * @file qv-hwloc.h
 */

#ifndef QUO_VADIS_HWLOC_H
#define QUO_VADIS_HWLOC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum qv_hwloc_obj_type_e {
    QV_HWLOC_OBJ_MACHINE,
    QV_HWLOC_OBJ_PACKAGE,
    QV_HWLOC_OBJ_CORE,
    QV_HWLOC_OBJ_PU,
    QV_HWLOC_OBJ_L1CACHE,
    QV_HWLOC_OBJ_L2CACHE,
    QV_HWLOC_OBJ_L3CACHE,
    QV_HWLOC_OBJ_L4CACHE,
    QV_HWLOC_OBJ_L5CACHE,
    QV_HWLOC_OBJ_NUMANODE,
    // TODO(skg) Consider just providing things like GPU
    QV_HWLOC_OBJ_OS_DEVICE
} qv_hwloc_obj_type_t;

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
