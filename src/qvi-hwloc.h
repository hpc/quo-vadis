/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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

#ifdef __cplusplus
extern "C" {
#endif

/** ID used for invisible devices. */
const int QVI_HWLOC_DEVICE_INVISIBLE_ID = -1;
/** ID used to indicate an invalid or unset ID. */
const int QVI_HWLOC_DEVICE_INVALID_ID = -1;

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
qvi_hwloc_delete(
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
qvi_hwloc_bitmap_delete(
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
 * Caller is responsible for freeing returned resources.
 */
int
qvi_hwloc_bitmap_asprintf(
    hwloc_const_cpuset_t cpuset,
    char **result
);

/**
 * Caller is responsible for freeing returned resources.
 */
int
qvi_hwloc_bitmap_list_asprintf(
    hwloc_const_cpuset_t cpuset,
    char **result
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
    pid_t task_id,
    hwloc_cpuset_t *out_cpuset
);

/**
 *
 */
int
qvi_hwloc_task_get_cpubind_as_string(
    qvi_hwloc_t *hwl,
    pid_t task_id,
    char **cpusets
);

/**
 *
 */
int
qvi_hwloc_task_intersects_obj_by_type_id(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t type,
    pid_t task_id,
    int type_index,
    int *result
);

/**
 *
 */
int
qvi_hwloc_task_isincluded_in_obj_by_type_id(
    qvi_hwloc_t *hwl,
    pid_t task_id,
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
qvi_hwloc_get_obj_type_in_cpuset(
    qvi_hwloc_t *hwl,
    int npieces,
    hwloc_const_cpuset_t cpuset,
    qv_hw_obj_type_t *target_obj
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
int
qvi_hwloc_device_new(
    qvi_hwloc_device_t **dev
);

/**
 *
 */
void
qvi_hwloc_device_delete(
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
    pid_t task_id,
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
qvi_hwloc_get_device_id_in_cpuset(
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
qvi_hwloc_split_cpuset_by_chunk_id(
    qvi_hwloc_t *hwl,
    hwloc_const_cpuset_t cpuset,
    uint_t nchunks,
    uint_t chunk_id,
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
    uint_t nobjs,
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
 * Returns a reference to vector of supported device types.
 */
const std::vector<qv_hw_obj_type_t> &
qvi_hwloc_supported_devices(void);

/**
 * hwloc bitmap object.
 */
struct qvi_hwloc_bitmap {
private:
    /** Internal bitmap. */
    hwloc_bitmap_t m_data = nullptr;
public:
    /** Default constructor. */
    qvi_hwloc_bitmap(void)
    {
        const int rc = qvi_hwloc_bitmap_calloc(&m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    /** Construct via hwloc_const_bitmap_t. */
    explicit qvi_hwloc_bitmap(hwloc_const_bitmap_t bitmap)
    {
        int rc = qvi_hwloc_bitmap_calloc(&m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
        rc = set(bitmap);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    /** Copy constructor. */
    qvi_hwloc_bitmap(const qvi_hwloc_bitmap &src)
    {
        int rc = qvi_hwloc_bitmap_calloc(&m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
        rc = set(src.m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    /** Destructor. */
    ~qvi_hwloc_bitmap(void)
    {
        qvi_hwloc_bitmap_delete(&m_data);
    }
    /** Equality operator. */
    bool
    operator==(
        const qvi_hwloc_bitmap &x
    ) const {
        return hwloc_bitmap_compare(cdata(), x.cdata()) == 0;
    }
    /** Assignment operator. */
    void
    operator=(const qvi_hwloc_bitmap &src)
    {
        const int rc = qvi_hwloc_bitmap_copy(src.m_data, m_data);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    /** Sets the object's internal bitmap to match src's. */
    int
    set(hwloc_const_bitmap_t src)
    {
        return qvi_hwloc_bitmap_copy(src, m_data);
    }
    /**
     * Returns a hwloc_bitmap_t that allows
     * modification of internal instance data.
     */
    hwloc_bitmap_t
    data(void)
    {
        return m_data;
    }
    /**
     * Returns a hwloc_const_bitmap_t that
     * cannot modify internal instance data.
     */
    hwloc_const_bitmap_t
    cdata(void) const
    {
        return m_data;
    }
};

/**
 * Vector of cpuset objects.
 */
using qvi_hwloc_cpusets_t = std::vector<qvi_hwloc_bitmap>;

typedef struct qvi_hwloc_device_s {
    /** Device type. */
    qv_hw_obj_type_t type = QV_HW_OBJ_LAST;
    /** Device affinity. */
    qvi_hwloc_bitmap affinity;
    /** Vendor ID. */
    int vendor_id = QVI_HWLOC_DEVICE_INVALID_ID;
    /** System Management Interface ID. */
    int smi = QVI_HWLOC_DEVICE_INVALID_ID;
    /** CUDA/ROCm visible devices ID. */
    int id = QVI_HWLOC_DEVICE_INVISIBLE_ID;
    /** Device name. */
    std::string name;
    /** PCI bus ID. */
    std::string pci_bus_id;
    /** Universally Unique Identifier. */
    std::string uuid;
    /** Constructor. */
    qvi_hwloc_device_s(void) = default;
    /** Destructor. */
    ~qvi_hwloc_device_s(void) = default;
} qvi_hwloc_device_t;

/** Device list type. */
using qvi_hwloc_dev_list_t = std::vector<
    std::shared_ptr<qvi_hwloc_device_t>
>;

/**
 *
 */
int
qvi_hwloc_get_devices_in_bitmap(
    qvi_hwloc_t *hwl,
    qv_hw_obj_type_t dev_type,
    const qvi_hwloc_bitmap &bitmap,
    qvi_hwloc_dev_list_t &devs
);

int
qvi_hwloc_bind_string(
    qvi_hwloc_t *hwloc,
    hwloc_const_bitmap_t bitmap,
    qv_bind_string_flags_t flags,
    char **result
);

#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
