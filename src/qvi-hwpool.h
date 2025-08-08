/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwpool.h
 *
 * Hardware resource pool.
 */

#ifndef QVI_HWPOOL_H
#define QVI_HWPOOL_H

#include "qvi-common.h"
#include "qvi-hwloc.h"

/**
 * Base hardware pool resource class.
 */
struct qvi_hwpool_res {
    friend class cereal::access;
protected:
    /** Resource hint flags. */
    qv_scope_create_hints_t m_hints = QV_SCOPE_CREATE_HINT_NONE;
    /** The resource's affinity encoded as a bitmap. */
    qvi_hwloc_bitmap m_affinity;
public:
    /** Returns the resource's create hints. */
    qv_scope_create_hints_t
    hints(void);
    /**
     * Returns a reference to the resource's affinity encoded by a bitmap.
     */
    qvi_hwloc_bitmap &
    affinity(void);
    /**
     * Returns a const reference to the resource's affinity encoded by a bitmap.
     */
    const qvi_hwloc_bitmap &
    affinity(void) const;

    template <class Archive>
    void
    serialize(
        Archive &archive
    ) {
        archive(m_hints, m_affinity);
    }
};

/**
 * Defines a hardware pool CPU. A CPU here may have multiple
 * processing units (PUs), which are defined as the CPU's affinity. For now a
 * qvi_hwpool_cpu has the same structure as a qvi_hwpool_res.
 */
using qvi_hwpool_cpu = qvi_hwpool_res;

/**
 * Defines a hardware pool device. This differs from a qvi_hwloc_device
 * because we only maintain information relevant for user-facing operations.
 */
struct qvi_hwpool_dev : qvi_hwpool_res {
    friend class cereal::access;
private:
    /** Device type. */
    qv_hw_obj_type_t m_type = QV_HW_OBJ_LAST;
    /** The bitmap encoding CPU affinity. */
    qvi_hwloc_bitmap m_affinity;
    /** Device ID (ordinal). */
    int m_id = qvi_hwloc_device::INVALID_ID;
    /** The PCI bus ID. */
    std::string m_pci_bus_id;
    /** Universally Unique Identifier. */
    std::string m_uuid;
public:
    /** Default constructor. */
    qvi_hwpool_dev(void) = default;
    /** Constructor using qvi_hwloc_device. */
    explicit qvi_hwpool_dev(
        const qvi_hwloc_device &dev
    );
    /** Constructor using std::shared_ptr<qvi_hwloc_device>. */
    explicit qvi_hwpool_dev(
        const std::shared_ptr<qvi_hwloc_device> &shdev
    );
    /** Destructor. */
    virtual ~qvi_hwpool_dev(void) = default;
    /** Equality operator. */
    bool
    operator==(
        const qvi_hwpool_dev &x
    ) const;
    /** Returns the device's type. */
    qv_hw_obj_type_t
    type(void) const ;
    /** Returns the device's ID string formatted as specified. */
    int
    id(
        qv_device_id_type_t format,
        char **result
    ) const;

    template <class Archive>
    void
    serialize(
        Archive &archive
    ) {
        archive(
            m_hints, m_affinity, m_type,
            m_id, m_pci_bus_id, m_uuid
        );
    }
};

/**
 * Maintains a mapping between device types and devices of those types.
 */
using qvi_hwpool_devs_t = std::multimap<
    qv_hw_obj_type_t, std::shared_ptr<qvi_hwpool_dev>
>;

struct qvi_hwpool {
    friend class cereal::access;
private:
    /** The hardware pool's CPU. */
    qvi_hwpool_cpu m_cpu;
    /** The hardware pool's devices. */
    qvi_hwpool_devs_t m_devs;
    /**
     * Adds all devices with affinity to the
     * provided, initialized hardware resource pool.
     */
    int
    m_add_devices_with_affinity(
        qvi_hwloc &hwloc
    );
public:
    /**
     * Initializes a hardware pool from the given
     * hardware locality information and cpuset.
     */
    int
    initialize(
        qvi_hwloc &hwloc,
        const qvi_hwloc_bitmap &cpuset
    );
    /**
     * Returns a const reference to the hardware pool's cpuset.
     */
    const qvi_hwloc_bitmap &
    cpuset(void) const;
    /**
     * Returns a const reference to the hardware pool's devices.
     */
    const qvi_hwpool_devs_t &
    devices(void) const;
    /**
     * Returns the number of objects in the hardware pool.
     */
    int
    nobjects(
        qvi_hwloc &hwloc,
        qv_hw_obj_type_t obj_type,
        int *result
    ) const;
    /**
     * Adds a qvi_hwpool_dev_s device.
     */
    int
    add_device(
        const qvi_hwpool_dev &dev
    );
    /**
     * Releases all devices in the hwpool.
     */
    int
    release_devices(void);

    template <class Archive>
    void
    serialize(
        Archive &archive
    ) {
        archive(m_cpu, m_devs);
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
