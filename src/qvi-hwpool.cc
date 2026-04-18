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
#include "qvi-map.h"

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
        default:
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
    auto [first, last] = m_devs.equal_range(dev.type());
    auto view = std::ranges::subrange(first, last);
    return std::ranges::any_of(
        view, [&](const auto &pair) { return pair.second == dev; }
    );
}

// TODO(skg) This is suspect. Perhaps we should favor affinity preserving
// mapping?
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

std::pair<qv_hw_obj_type_t, qvi_hwloc_bitmap>
qvi_hwpool::m_primary_cpuset_for_split(
    qv_hw_obj_type_t requested_type
) const {
    qv_hw_obj_type_t real_type = requested_type;
    // Were we provided a real resource type that we have to split? Or was
    // QV_HW_OBJ_LAST instead provided to indicate that we were called from a
    // split() context, which uses the host's primary compute resource to split
    // the resources. If a host has GPUs, use the union of those. Otherwise use
    // the host's CPU resources.
    if (requested_type == QV_HW_OBJ_LAST) {
        if (devices().count(QV_HW_OBJ_GPU) > 0) {
            real_type = QV_HW_OBJ_GPU;
        }
        // No GPUs, so pick PUs as the host resource, since this is the atomic
        // unit at which host resources are split.
        else {
            real_type = QV_HW_OBJ_PU;
        }
    }

    if (qvi_hwloc::obj_is_host_resource(real_type)) {
        return {real_type, m_cpu.affinity()};
    }
    // Else, an OS device. The cpuset will be
    // the union over the devices affinities.
    qvi_hwloc_bitmap result;
    for (const auto &dev : devices(real_type)) {
        result = result | dev.affinity();
    }
    return {real_type, result};
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

std::vector<qvi_hwpool_dev>
qvi_hwpool::devices(
    qv_hw_obj_type_t obj_type
) const {
    const auto [first, last] = m_devs.equal_range(obj_type);
    const auto subrange = std::ranges::subrange(first, last);
    // Extract only the devices from the pairs.
    const auto dev_view = subrange | std::views::values;
    std::vector<qvi_hwpool_dev> result;
    std::ranges::copy(dev_view, std::back_inserter(result));
    return result;
}

// TODO(skg) Complete implementation.
std::vector<qvi_hwloc_bitmap>
qvi_hwpool::affinities(
    qv_hw_obj_type_t obj_type
) const {
    std::vector<qvi_hwloc_bitmap> result;
    if (qvi_hwloc::obj_is_host_resource(obj_type)) {
        throw qvi_runtime_error(QV_ERR_NOT_SUPPORTED);
    }
    else {
        for (const auto &dev : devices(obj_type)) {
            result.emplace_back(dev.affinity());
        }
    }
    return result;
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
    return m_devs.count(obj_type);
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

std::vector<qvi_hwpool>
qvi_hwpool::split_atn(
    const qvi_hwloc &hwloc,
    qv_hw_obj_type_t obj_type,
    size_t npieces
) {
    size_t npiecesp = npieces;
    // Determine the primary object type that we are splitting over.
    const auto [real_type, primary_cpuset] = m_primary_cpuset_for_split(obj_type);
    // Not called from a split() context and dealing with GPUs.
    if (obj_type != QV_HW_OBJ_LAST && real_type == QV_HW_OBJ_GPU) {
        npiecesp = std::max(npieces, nobjects(hwloc, real_type));
    }
    // Split the primary cpuset into npieces.
    std::vector<qvi_hwloc_bitmap> hwpool_cpusets;
    int rc = hwloc.bitmap_split(primary_cpuset, npiecesp, hwpool_cpusets);
    if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    // Create the n hardware pools, initiate each with a cpuset from split.
    std::vector<qvi_hwpool> result;
    for (size_t i = 0; i < npiecesp; ++i) {
        result.emplace_back(qvi_hwpool(hwpool_cpusets[i]));
    }
    // Iterate over supported device types and add devices based on affinity.
    for (const auto devt : qvi_hwloc::supported_devices()) {
        const auto devs = devices(devt);
        const auto dev_affinities = affinities(devt);
        // Map devices to cpusets, trying to maintain good affinity.
        qvi_map_t map;
        rc = qvi_map_affinity_preserving(
            map, qvi_map_spread, dev_affinities, hwpool_cpusets
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
        // Now that we have the mapping, assign
        // devices to the associated hardware pools.
        for (const auto &[devi, pooli] : map) {
            rc = result[pooli].add_device(devs[devi]);
            if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
        }
    }
    return result;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
