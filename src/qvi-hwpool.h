/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwpool.h
 *
 * Hardware Resource Pool
 */

#ifndef QVI_HWPOOL_H
#define QVI_HWPOOL_H

#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-line.h"
#include "qvi-utils.h"

/**
 * Maintains a mapping between a resource ID and associated hint flags.
 */
using qvi_hwpool_resource_id_hint_map_t = std::unordered_map<
    uint32_t, qv_scope_create_hint_t
>;

/**
 * Maintains information about a pool of hardware resources by resource ID.
 */
struct qvi_hwpool_resource_info_s {
    /**
     * Vector maintaining the reference count for a given resource ID. There is
     * a one-to-one correspondence between resource IDs and addressable vector
     * slots since every resource is reference counted. For example, each bit in
     * a cpuset will have a corresponding reference count indexed by the bit's
     * location in the cpuset.
     */
    std::vector<uint32_t> resid_ref_count;
    /**
     * Maps resource IDs to hint flags that
     * they might have associated with them.
     */
    qvi_hwpool_resource_id_hint_map_t resid_hint_map;
};

/**
 * Base hardware pool resource class.
 */
struct qvi_hwpool_resource_s {
    /** Resource info. */
    qvi_hwpool_resource_info_s resinfo;
    /** Base constructor that does minimal work. */
    qvi_hwpool_resource_s(void) = default;
    /** Virtual destructor. */
    virtual ~qvi_hwpool_resource_s(void) = default;
};

/**
 * Defines a cpuset pool.
 */
struct qvi_hwpool_cpus_s : qvi_hwpool_resource_s {
    int qvim_rc = QV_ERR_INTERNAL;
    /** The cpuset of the maintained CPUs. */
    qvi_hwloc_bitmap_t cpuset;
    /** Constructor */
    qvi_hwpool_cpus_s(void)
    {
        qvim_rc = qvi_construct_rc(cpuset);
    }
    /** Destructor */
    virtual ~qvi_hwpool_cpus_s(void) = default;
};

/** Device information. */
struct qvi_hwpool_devinfo_s {
    int qvim_rc = QV_ERR_INTERNAL;
    /** Device type. */
    qv_hw_obj_type_t type = QV_HW_OBJ_LAST;
    /** Device ID. */
    int id = 0;
    /** The PCI bus ID. */
    std::string pci_bus_id;
    /** UUID */
    std::string uuid;
    /** The bitmap encoding CPU affinity. */
    qvi_hwloc_bitmap_t affinity;
    /** No default constructor. */
    qvi_hwpool_devinfo_s(void) = delete;
    /** Constructor */
    qvi_hwpool_devinfo_s(
        qv_hw_obj_type_t type,
        int id,
        cstr_t pci_bus_id,
        cstr_t uuid,
        hwloc_const_cpuset_t affinity
    );
    /** Destructor */
    ~qvi_hwpool_devinfo_s(void) = default;
    /** Equality operator. */
    bool
    operator==(const qvi_hwpool_devinfo_s &x) const;
};

using qvi_hwpool_devinfos_t = std::multimap<
    int, std::shared_ptr<qvi_hwpool_devinfo_s>
>;

struct qvi_hwpool_s;
typedef struct qvi_hwpool_s qvi_hwpool_t;

/**
 *
 */
int
qvi_hwpool_new(
    qvi_hwpool_t **pool
);

/**
 *
 */
int
qvi_hwpool_new_from_line(
    qvi_line_hwpool_t *line,
    qvi_hwpool_t **pool
);

/**
 *
 */
int
qvi_hwpool_new_line_from_hwpool(
    const qvi_hwpool_t *pool,
    qvi_line_hwpool_t **line
);

/**
 *
 */
int
qvi_hwpool_init(
    qvi_hwpool_t *pool,
    hwloc_const_bitmap_t cpuset
);

/**
 *
 */
void
qvi_hwpool_free(
    qvi_hwpool_t **pool
);

/**
 *
 */
int
qvi_hwpool_dup(
    const qvi_hwpool_t *const rpool,
    qvi_hwpool_t **dup
);

/**
 *
 */
int
qvi_hwpool_add_device(
    qvi_hwpool_t *rpool,
    qv_hw_obj_type_t type,
    int id,
    cstr_t pcibid,
    cstr_t uuid,
    hwloc_const_cpuset_t affinity
);

/**
 * Adds all devices with affinity to the provided,
 * initialized hardware resource pool.
 */
int
qvi_hwpool_add_devices_with_affinity(
    qvi_hwpool_t *pool,
    qvi_hwloc_t *hwloc
);

/**
 *
 */
int
qvi_hwpool_release_devices(
    qvi_hwpool_t *pool
);

/**
 *
 */
hwloc_const_cpuset_t
qvi_hwpool_cpuset_get(
    qvi_hwpool_t *pool
);

/**
 *
 */
const qvi_hwpool_devinfos_t *
qvi_hwpool_devinfos_get(
    qvi_hwpool_t *pool
);

/**
 *
 */
int
qvi_hwpool_obtain_by_cpuset(
    qvi_hwpool_t *pool,
    qvi_hwloc_t *hwloc,
    hwloc_const_cpuset_t cpuset,
    qvi_hwpool_t **opool
);

/**
 *
 */
int
qvi_hwpool_pack(
    const qvi_hwpool_t *hwp,
    qvi_bbuff_t *buff
);

/**
 *
 */
int
qvi_hwpool_unpack(
    void *buff,
    qvi_hwpool_t **hwp
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
