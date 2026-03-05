/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwpool.cc
 *
 * Hardware Resource Pool
 */

#include "qvi-hwpool.h"

qv_scope_create_hints_t
qvi_hwpool_res::hints(void)
{
    return m_hints;
}

qvi_hwloc_bitmap &
qvi_hwpool_res::affinity(void)
{
    return m_affinity;
}

const qvi_hwloc_bitmap &
qvi_hwpool_res::affinity(void) const
{
    return m_affinity;
}

qvi_hwpool_dev::qvi_hwpool_dev(
    const qvi_hwloc_device &dev
) : qvi_hwpool_res(dev.affinity)
  , m_type(dev.type)
  , m_id(dev.id)
  , m_pci_bus_id(dev.pci_bus_id)
  , m_uuid(dev.uuid) { }

qvi_hwpool_dev::qvi_hwpool_dev(
    const std::shared_ptr<qvi_hwloc_device> &shdev
) : qvi_hwpool_dev(*shdev.get()) { }

qv_hw_obj_type_t
qvi_hwpool_dev::type(void)
    const {
    return m_type;
}

int
qvi_hwpool_dev::id(
    qv_device_id_type_t format,
    char **result
) const {
    int rc = QV_SUCCESS, nw = 0;
    switch (format) {
        case (QV_DEVICE_ID_UUID):
            nw = asprintf(result, "%s", m_uuid.c_str());
            break;
        case (QV_DEVICE_ID_PCI_BUS_ID):
            nw = asprintf(result, "%s", m_pci_bus_id.c_str());
            break;
        case (QV_DEVICE_ID_ORDINAL):
            nw = asprintf(result, "%d", m_id);
            break;
        default:
            rc = QV_ERR_INVLD_ARG;
            break;
    }
    if (qvi_unlikely(nw == -1)) rc = QV_ERR_OOR;

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        *result = nullptr;
    }
    return rc;
}

bool
qvi_hwpool::m_device_in_pool(
    const qvi_hwpool_dev &dev
) {
    auto [first, last] = m_devs.equal_range(dev.type());
    auto view = std::ranges::subrange(first, last);
    return std::ranges::any_of(
        view, [&](const auto &pair) { return pair.second == dev; }
    );
}

int
qvi_hwpool::m_add_devices_with_affinity(
    qvi_hwloc &hwloc
) {
    int rc = QV_SUCCESS;
    // Iterate over the supported device types.
    for (const auto devt : qvi_hwloc::supported_devices()) {
        qvi_hwloc_dev_list devs;
        rc = hwloc.get_devices_in_cpuset(
            devt, m_cpu.affinity().cdata(), devs
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        for (const auto &dev : devs) {
            rc = add_device(qvi_hwpool_dev(dev));
            if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        }
    }
    return rc;
}

int
qvi_hwpool::initialize(
    qvi_hwloc &hwloc,
    const qvi_hwloc_bitmap &cpuset
) {
    const int rc = m_cpu.affinity().set(cpuset.cdata());
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Add devices with affinity to the hardware pool.
    return m_add_devices_with_affinity(hwloc);
}

const qvi_hwloc_bitmap &
qvi_hwpool::cpuset(void) const
{
    return m_cpu.affinity();
}

const qvi_hwpool_devs_t &
qvi_hwpool::devices(void) const
{
    return m_devs;
}

int
qvi_hwpool::nobjects(
    qvi_hwloc &hwloc,
    qv_hw_obj_type_t obj_type,
    int *result
) const {
    if (qvi_hwloc::obj_is_host_resource(obj_type)) {
        return hwloc.get_nobjs_in_cpuset(
            obj_type, m_cpu.affinity().cdata(), result
        );
    }
    *result = m_devs.count(obj_type);
    return QV_SUCCESS;
}

int
qvi_hwpool::add_device(
    const qvi_hwpool_dev &dev
) {
    if (!m_device_in_pool(dev)) {
        m_devs.insert({dev.type(), dev});
    }
    return QV_SUCCESS;
}

int
qvi_hwpool::release_devices(void)
{
    m_devs.clear();
    return QV_SUCCESS;
}

qvi_hwpool
qvi_hwpool::set_union(
    const std::vector<qvi_hwpool> &hwpools
) {
    // Calculate the union of the hardware pool cpusets.
    qvi_hwloc_bitmap cpu_union;
    for (const auto &hwpool : hwpools) {
        cpu_union = cpu_union | hwpool.cpuset();
    }
    // Create the result hardware pool with the proper cpuset.
    qvi_hwpool result(cpu_union);
    // Do the same for its devices. Note that add_device() shall protect against
    // multiple insertions of the same device into the hardware pool.
    for (const auto &hwpool : hwpools) {
        for (const auto &[dev_type, dev] : hwpool.devices()) {
            result.add_device(dev);
        }
    }
    return result;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
