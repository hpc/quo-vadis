/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwpool.h
 *
 * Implements hardware resource types and resource pools. These are the
 * structures that are used within scopes.
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
    /** Resource flags. */
    qv_scope_flags_t m_flags = QV_SCOPE_FLAG_NONE;
    /** The resource's affinity encoded as a bitmap. */
    qvi_hwloc_bitmap m_affinity = {};
    /** Polymorphic equality check. */
    virtual bool
    equals(const qvi_hwpool_res &other) const {
        return m_affinity == other.m_affinity;
    }
public:
    /** Constructor. */
    qvi_hwpool_res(void) = default;
    /** Constructor. */
    qvi_hwpool_res(
        const qvi_hwloc_bitmap &affinity
    ) : m_affinity(affinity) { }
    /** Returns the resource's flags. */
    qv_scope_flags_t
    flags(void) const;
    /** Returns a const reference to the resource's affinity bitmap. */
    const qvi_hwloc_bitmap &
    affinity(void) const;
    /** Equality operator. */
    bool
    operator==(
        const qvi_hwpool_res &other
    ) const {
        return equals(other);
    }
    /** Serializes a hardware pool resource. */
    template <class Archive>
    void
    serialize(
        Archive &archive
    ) {
        archive(m_flags, m_affinity);
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
    /** Device ID (ordinal). */
    int m_id = qvi_hwloc_device::INVALID_ID;
    /** The PCI bus ID. */
    std::string m_pci_bus_id = {};
    /** Universally Unique Identifier. */
    std::string m_uuid = {};
    /** Polymorphic equality check. */
    virtual bool
    equals(const qvi_hwpool_res &other) const override {
        if (auto o = dynamic_cast<const qvi_hwpool_dev *>(&other)) {
            return m_pci_bus_id == o->m_pci_bus_id;
        }
        return false;
    }
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
        const qvi_hwpool_dev &other
    ) const {
        return equals(other);
    }
    /** Returns the device's type. */
    qv_hw_obj_type_t
    type(void)
        const {
        return m_type;
    }
    /** Returns the device's ID. */
    int
    id(void)
        const {
        return m_id;
    }
    /** Returns the device's ID string formatted as specified. */
    int
    id(
        qv_device_id_type_t format,
        char **result
    ) const;
    /** Returns the device's ID string formatted as specified. */
    std::string
    id(
        qv_device_id_type_t format
    ) const;

    template <class Archive>
    void
    serialize(
        Archive &archive
    ) {
        archive(
            m_flags, m_affinity, m_type,
            m_id, m_pci_bus_id, m_uuid
        );
    }
};

struct qvi_hwpool {
    friend class cereal::access;
    /** Device list type. */
    using dev_list_t = std::vector<std::shared_ptr<qvi_hwpool_dev>>;
private:
    /** Maps device types to devices of those types. */
    using dev_map_t = std::map<qv_hw_obj_type_t, dev_list_t>;
    /** The hardware pool's CPU. */
    qvi_hwpool_cpu m_cpu;
    /** The hardware pool's devices. */
    dev_map_t m_dev_map;
    /** Returns whether a given device is already in the pool. */
    bool
    m_device_in_pool(
        const qvi_hwpool_dev &dev
    ) const;
    /**
     * Adds all devices with affinity to the
     * provided, initialized hardware resource pool.
     */
    int
    m_add_devices_with_affinity(
        const qvi_hwloc &hwloc
    );
public:
    /** Constructor (default). */
    qvi_hwpool(void) = default;
    /** Constructor (initialize only host resources). */
    qvi_hwpool(
        const qvi_hwloc_bitmap &cpuset
    ) : m_cpu(cpuset) { }
    /**
     * Populates a hardware pool from the given
     * hardware locality information and cpuset.
     */
    int
    populate(
        const qvi_hwloc &hwloc,
        const qvi_hwloc_bitmap &cpuset
    );
    /**
     * Returns a const reference to the hardware pool's cpuset.
     */
    const qvi_hwloc_bitmap &
    cpuset(void) const;
    /**
     * Returns a const reference to the devices of the given object type.
     */
    const qvi_hwpool::dev_list_t &
    devices(
        qv_hw_obj_type_t obj_type
    ) const;
    /**
     * Returns a vector of affinities, one for each given device type present in
     * the hardware pool.
     */
    std::vector<qvi_hwloc_bitmap>
    device_affinities(
        qv_hw_obj_type_t obj_type
    ) const;
    /**
     * Returns the number of objects in the hardware pool.
     */
    size_t
    nobjects(
        const qvi_hwloc &hwloc,
        qv_hw_obj_type_t obj_type
    ) const;
    /**
     * Adds a qvi_hwpool_dev_s device. Attempts to insert the same device
     * multiple times will succeed, but the device will be added exactly once.
     */
    int
    add_device(
        const qvi_hwpool_dev &dev
    );
    /**
     *
     */
    static qvi_hwpool
    set_union(
        const std::vector<qvi_hwpool> &hwpools
    );
    /**
     * Serializes a hardware pool.
     */
    template <class Archive>
    void
    serialize(
        Archive &archive
    ) {
        archive(m_cpu, m_dev_map);
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
