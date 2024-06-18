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

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-bbuff.h"
#include "qvi-hwloc.h"

/**
 * Base hardware pool resource class.
 */
struct qvi_hwpool_res_s {
    /** Resource hint flags. */
    qv_scope_create_hints_t hints = QV_SCOPE_CREATE_HINT_NONE;
};

/**
 * Defines a hardware pool CPU. A CPU here may have multiple
 * processing units (PUs), which are defined in the CPU's cpuset.
 */
struct qvi_hwpool_cpu_s : qvi_hwpool_res_s {
    /** The cpuset of the CPU's PUs. */
    qvi_hwloc_bitmap_s cpuset;
};

/**
 * Defines a hardware pool device. This differs from a qvi_hwloc_device_s
 * because we only maintain information relevant for user-facing operations.
 */
struct qvi_hwpool_dev_s : qvi_hwpool_res_s {
    /** Device type. */
    qv_hw_obj_type_t type = QV_HW_OBJ_LAST;
    /** The bitmap encoding CPU affinity. */
    qvi_hwloc_bitmap_s affinity;
    /** Device ID (ordinal). */
    int id = QVI_HWLOC_DEVICE_INVALID_ID;
    /** The PCI bus ID. */
    std::string pci_bus_id;
    /** Universally Unique Identifier. */
    std::string uuid;
    /** Default constructor. */
    qvi_hwpool_dev_s(void) = default;
    /** Constructor using qvi_hwloc_device_s. */
    explicit qvi_hwpool_dev_s(
        const qvi_hwloc_device_s &dev
    ) : type(dev.type)
      , affinity(dev.affinity)
      , id(dev.id)
      , pci_bus_id(dev.pci_bus_id)
      , uuid(dev.uuid)
    {
    }
    /** Constructor using std::shared_ptr<qvi_hwloc_device_s>. */
    explicit qvi_hwpool_dev_s(
        const std::shared_ptr<qvi_hwloc_device_s> &shdev
    ) : qvi_hwpool_dev_s(*shdev.get())
    {
    }
    /** Destructor. */
    virtual ~qvi_hwpool_dev_s(void) = default;
    /** Equality operator. */
    bool
    operator==(
        const qvi_hwpool_dev_s &x
    ) const {
        return uuid == x.uuid;
    }
};

/**
 * Maintains a mapping between device types and devices of those types.
 */
using qvi_hwpool_devs_t = std::multimap<
    qv_hw_obj_type_t, std::shared_ptr<qvi_hwpool_dev_s>
>;

struct qvi_hwpool_s {
    /** The hardware pool's CPU. */
    qvi_hwpool_cpu_s cpu;
    /** The hardware pool's devices. */
    qvi_hwpool_devs_t devs;
    /**
     * Initializes a hardware pool with the given cpuset.
     */
    int
    initialize(
        hwloc_const_bitmap_t cpuset
    ) {
        return cpu.cpuset.set(cpuset);
    }
    /**
     * Creates a new, initialized hardware pool based
     * on the affinity encoded in the provided cpuset.
     */
    static int
    new_hwpool_by_cpuset(
        qvi_hwloc_t *hwloc,
        hwloc_const_cpuset_t cpuset,
        qvi_hwpool_s **opool
    );
    /**
     * Returns a pointer to the hwpool's cpuset.
     */
    const qvi_hwloc_bitmap_s &
    get_cpuset(void)
    {
        return cpu.cpuset;
    }
    /**
     * Adds a qvi_hwpool_dev_s device.
     */
    int
    add_device(
        const qvi_hwpool_dev_s &dev
    ) {
        auto shdev = std::make_shared<qvi_hwpool_dev_s>(dev);
        devs.insert({dev.type, shdev});
        return QV_SUCCESS;
    }
    /**
     * Adds all devices with affinity to the
     * provided, initialized hardware resource pool.
     */
    int
    add_devices_with_affinity(
        qvi_hwloc_t *hwloc
    ) {
        int rc = QV_SUCCESS;
        // Iterate over the supported device types.
        for (const auto devt : qvi_hwloc_supported_devices()) {
            qvi_hwloc_dev_list_t devs;
            rc = qvi_hwloc_get_devices_in_bitmap(
                hwloc, devt, cpu.cpuset, devs
            );
            if (rc != QV_SUCCESS) return rc;
            for (const auto &dev : devs) {
                rc = add_device(qvi_hwpool_dev_s(dev));
                if (rc != QV_SUCCESS) return rc;
            }
        }
        return rc;
    }
    /**
     * Releases all devices in the hwpool.
     */
    int
    release_devices(void)
    {
        devs.clear();
        return QV_SUCCESS;
    }
    /**
     * Returns a const reference to the hardware pool's devices.
     */
    const qvi_hwpool_devs_t &
    get_devices(void)
    {
        return devs;
    }
    /**
     * Packs the instance into a bbuff.
     */
    int
    pack(
        qvi_bbuff_t *buff
    );
    /**
     * Unpacks the buffer and creates a new hardware pool instance.
     */
    static int
    unpack(
        qvi_bbuff_t *buff,
        qvi_hwpool_s **hwp
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
