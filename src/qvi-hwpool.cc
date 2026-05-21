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

std::string
qvi_hwpool_dev::id(
    qv_device_id_type_t format
) const {
    switch (format) {
        case (QV_DEVICE_ID_UUID):
            return m_uuid;
        case (QV_DEVICE_ID_PCI_BUS_ID):
            return m_pci_bus_id;
        case (QV_DEVICE_ID_ORDINAL):
            return std::to_string(m_id);
        [[unlikely]] default:
            throw qvi_runtime_error(QV_ERR_INVLD_ARG);
    }
}

int
qvi_hwpool_dev::id(
    qv_device_id_type_t format,
    char **result
) const {
    int rc = QV_SUCCESS;

    const std::string ids = id(format);
    const int nw = asprintf(result, "%s", ids.c_str());
    if (qvi_unlikely(nw == -1)) rc = QV_ERR_OOR;

    if (qvi_unlikely(rc != QV_SUCCESS)) {
        *result = nullptr;
    }
    return rc;
}

bool
qvi_hwpool::m_device_in_pool(
    const qvi_hwpool_dev &dev
) const {
    for (const auto &pool_dev : devices(dev.type())) {
        if (*pool_dev.get() == dev) return true;
    }
    return false;
}

int
qvi_hwpool::m_add_devices_with_affinity(
    const qvi_hwloc &hwloc
) {
    int rc = QV_SUCCESS;
    // Iterate over the supported device types.
    for (const auto devt : qvi_hwloc::supported_devices()) {
        qvi_hwloc_dev_list devs;
        rc = hwloc.get_devices_included_in_cpuset(
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
qvi_hwpool::populate(
    const qvi_hwloc &hwloc,
    const qvi_hwloc_bitmap &cpuset
) {
    m_cpu = qvi_hwpool_cpu(cpuset);
    // Add devices with affinity to the hardware pool.
    return m_add_devices_with_affinity(hwloc);
}

const qvi_hwloc_bitmap &
qvi_hwpool::cpuset(void) const
{
    return m_cpu.affinity();
}

const qvi_hwpool::dev_list_t &
qvi_hwpool::devices(
    qv_hw_obj_type_t obj_type
) const {
    static const dev_list_t empty_list = {};
    if (!m_dev_map.contains(obj_type)) return empty_list;
    return m_dev_map.at(obj_type);
}

std::vector<qvi_hwloc_bitmap>
qvi_hwpool::device_affinities(
    qv_hw_obj_type_t obj_type
) const {
    if (qvi_unlikely(qvi_hwloc::obj_is_host_resource(obj_type))) {
        throw qvi_runtime_error(QV_ERR_NOT_SUPPORTED);
    }
    else {
        std::vector<qvi_hwloc_bitmap> result;
        for (const auto &dev : devices(obj_type)) {
            result.emplace_back(dev.get()->affinity());
        }
        return result;
    }
}

size_t
qvi_hwpool::nobjects(
    const qvi_hwloc &hwloc,
    qv_hw_obj_type_t obj_type
) const {
    if (qvi_hwloc::obj_is_host_resource(obj_type)) {
        size_t result;
        const int rc = hwloc.get_nobjs_in_cpuset(
            obj_type, m_cpu.affinity().cdata(), result
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
        return result;
    }
    // Note that this path also covers QV_HW_OBJ_LAST because result
    // will be 0 in the case that QV_HW_OBJ_LAST is passed here.
    return devices(obj_type).size();
}

int
qvi_hwpool::add_device(
    const qvi_hwpool_dev &dev
) {
    if (!m_device_in_pool(dev)) {
        // Safely ensure the vector exists, get iterator to it.
        auto [it, _] = m_dev_map.try_emplace(dev.type());
        // Push the new item safely into the vector.
        it->second.emplace_back(std::make_shared<qvi_hwpool_dev>(dev));
        // Sort the list by device ID after each
        // insertion to maintain correct device ordinals.
        std::sort(
            it->second.begin(),
            it->second.end(),
            [](const std::shared_ptr<qvi_hwpool_dev> &a,
               const std::shared_ptr<qvi_hwpool_dev> &b) {
                return a.get()->id() < b.get()->id(); // Sort ascending.
            }
        );
    }
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
        for (const auto devt : qvi_hwloc::supported_devices()) {
            for (const auto &dev : hwpool.devices(devt)) {
                result.add_device(*dev.get());
            }
        }
    }
    return result;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
