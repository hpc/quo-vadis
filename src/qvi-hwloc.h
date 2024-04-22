/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwloc.h
 */

#ifndef QVI_HWLOC_H
#define QVI_HWLOC_H

#include "qvi-common.h"
#include "qvi-task.h"

#ifdef __cplusplus
extern "C" {
#endif

struct qvi_hwloc_s;
typedef struct qvi_hwloc_s qvi_hwloc_t;

struct qvi_hwloc_device_s;
typedef struct qvi_hwloc_device_s qvi_hwloc_device_t;

/**
 * Returns the underlying hwloc type from the given qv_hw_obj_type_t type.
 */
hwloc_obj_type_t
qvi_hwloc_get_obj_type(
    qv_hw_obj_type_t external
);

/**
 * Returns whether the provided type is a host resource (e.g., core, NUMA node).
 */
bool
qvi_hwloc_obj_type_is_host_resource(
    qv_hw_obj_type_t type
);

/**
 *
 */
hwloc_topology_t
qvi_hwloc_get_topo_obj(
     qvi_hwloc_t *hwl
);

/**
 *
 */
int
qvi_hwloc_new(
    qvi_hwloc_t **hwl
);

/**
 *
 */
void
qvi_hwloc_free(
    qvi_hwloc_t **hwl
);

/**
 *
 */
int
qvi_hwloc_topology_init(
    qvi_hwloc_t *hwl,
    const char *xml
);

/**
 *
 */
int
qvi_hwloc_topology_load(
    qvi_hwloc_t *hwl
);

/**
 *
 */
int
qvi_hwloc_discover_devices(
    qvi_hwloc_t *hwl
);

/**
 *
 */
hwloc_topology_t
qvi_hwloc_topo_get(
    qvi_hwloc_t *hwl
);

/**
 *
 */
hwloc_const_cpuset_t
qvi_hwloc_topo_get_cpuset(
    qvi_hwloc_t *hwl
);

/**
 *
 */
int
qvi_hwloc_topo_is_this_system(
    qvi_hwloc_t *hwl
);

/**
 *
 */
int
qvi_hwloc_bitmap_calloc(
    hwloc_cpuset_t *cpuset
);

/**
 *
 */
void
qvi_hwloc_bitmap_free(
    hwloc_cpuset_t *cpuset
);

/**
 *
 */
int
qvi_hwloc_bitmap_copy(
    hwloc_const_cpuset_t src,
    hwloc_cpuset_t dest
);

/**
 *
 */
int
qvi_hwloc_bitmap_dup(
    hwloc_const_cpuset_t src,
    hwloc_cpuset_t *dest
);

/**
 * Returns the number of bits required to represent a given cpuset.
 */
int
qvi_hwloc_bitmap_nbits(
    hwloc_const_cpuset_t cpuset,
    size_t *nbits
);

/**
 *
 */
int
qvi_hwloc_get_nobjs_by_type(
   qvi_hwloc_t *hwloc,
   qv_hw_obj_type_t target_type,
   int *out_nobjs
);

/**
 *
 */
int
qvi_hwloc_emit_cpubind(
   qvi_hwloc_t *hwl,
   qvi_task_id_t task_id
);

/**
 *
 */
int
qvi_hwloc_bitmap_asprintf(
    char **result,
    hwloc_const_cpuset_t cpuset
);

/**
 *
 */
int
qvi_hwloc_bitmap_list_asprintf(
    char **result,
    hwloc_const_cpuset_t cpuset
);

/**
 *
 */
void
qvi_hwloc_cpuset_debug(
    cstr_t msg,
    hwloc_const_cpuset_t cpuset
);

/**
 *
 */
int
qvi_hwloc_bitmap_sscanf(
    hwloc_cpuset_t cpuset,
    char *str
);

/**
 *
 */
int
qvi_hwloc_task_get_cpubind(
    qvi_hwloc_t *hwl,
    qvi_task_id_t task_id,
    hwloc_cpuset_t *out_cpuset
);

/**
 *
 */
int
qvi_hwloc_task_get_cpubind_as_string(
    qvi_hwloc_t *hwl,
    qvi_task_id_t task_id,
    char **cpusets
);

/**
 *
 */
int
qvi_hwloc_task_intersects_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t type,
    qvi_task_id_t task_id,
    int type_index,
    int *result
);

/**
 *
 */
int
qvi_hwloc_task_isincluded_in_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qvi_task_id_t task_id,
    int type_index,
    int *result
);

/**
 *
 */
int
qvi_hwloc_topology_export(
    qvi_hwloc_t *hwl,
    const char *base_path,
    char **path
);

/**
 *
 */
int
qvi_hwloc_get_nobjs_in_cpuset(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    int *nobjs
);

/**
 *
 */
int
qvi_hwloc_obj_type_depth(
    qvi_hwloc_t *hwloc,
    qv_hw_obj_type_t type,
    int *depth
);

/**
 *
 */
int
qvi_hwloc_get_obj_in_cpuset_by_depth(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    int depth,
    int index,
    hwloc_obj_t *result_obj
);

/**
 *
 */
const qv_hw_obj_type_t *
qvi_hwloc_supported_devices(void);

/**
 *
 */
int
qvi_hwloc_n_supported_devices(void);

/**
 *
 */
int
qvi_hwloc_device_new(
    qvi_hwloc_device_t **dev
);

/**
 *
 */
void
qvi_hwloc_device_free(
    qvi_hwloc_device_t **dev
);

/**
 *
 */
int
qvi_hwloc_device_copy(
    qvi_hwloc_device_t *src,
    qvi_hwloc_device_t *dest
);

/**
 *
 */
int
qvi_hwloc_task_set_cpubind_from_cpuset(
    qvi_hwloc_t *hwl,
    qvi_task_id_t task_id,
    hwloc_const_cpuset_t cpuset
);

/**
 *
 */
int
qvi_hwloc_devices_emit(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t obj_type
);

/**
 *
 */
int
qvi_hwloc_get_device_in_cpuset(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t dev_obj,
    int i,
    hwloc_const_cpuset_t cpuset,
    qv_device_id_type_t dev_id_type,
    char **dev_id
);

/**
 *
 */
int
qvi_hwloc_split_cpuset_by_color(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    int ncolors,
    int color,
    hwloc_cpuset_t result
);

/**
 *
 */
int
qvi_hwloc_get_cpuset_for_nobjs(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t obj_type,
    int nobjs,
    hwloc_cpuset_t *result
);

/**
 *
 */
int
qvi_hwloc_get_device_affinity(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t dev_obj,
    int device_id,
    hwloc_cpuset_t *cpuset
);

#ifdef __cplusplus
}

/**
 * C++ style hwloc bitmap.
 */
struct qvi_hwloc_bitmap_s {
    int qvim_rc = QV_ERR_INTERNAL;
    /** Internal bitmap */
    hwloc_bitmap_t data = nullptr;
    /** Default constructor */
    qvi_hwloc_bitmap_s(void)
    {
        qvim_rc = qvi_hwloc_bitmap_calloc(&data);
    }
    /** Construct via hwloc_const_bitmap_t */
    qvi_hwloc_bitmap_s(hwloc_const_bitmap_t bitmap)
    {
        qvim_rc = qvi_hwloc_bitmap_calloc(&data);
        if (qvim_rc != QV_SUCCESS) return;
        set(bitmap);
    }
    /** Copy constructor. */
    qvi_hwloc_bitmap_s(const qvi_hwloc_bitmap_s &src)
    {
        qvim_rc = qvi_hwloc_bitmap_calloc(&data);
        if (qvim_rc != QV_SUCCESS) return;
        qvim_rc = qvi_hwloc_bitmap_copy(src.data, data);
    }
    /** Destructor */
    ~qvi_hwloc_bitmap_s(void)
    {
        qvi_hwloc_bitmap_free(&data);
    }
    /**
     * Assignment operator.
     *
     * Check qvim_rc on the LHS after assignment to verify status.
     */
    void
    operator=(const qvi_hwloc_bitmap_s &src)
    {
        qvim_rc = qvi_hwloc_bitmap_copy(src.data, data);
    }
    /**
     * Sets the object's internal bitmap to match src's.
     */
    int
    set(hwloc_const_bitmap_t src)
    {
        return qvi_hwloc_bitmap_copy(src, data);
    }
};

/**
 * Vector of cpuset objects.
 */
using qvi_hwloc_cpusets_t = std::vector<qvi_hwloc_bitmap_s>;

#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
