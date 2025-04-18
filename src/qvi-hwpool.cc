/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2025 Triad National Security, LLC
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
#include "qvi-bbuff-rmi.h"
#include "qvi-utils.h"

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

int
qvi_hwpool_cpu::packinto(
    qvi_bbuff *buff
) const {
    // Pack hints.
    const int rc = qvi_bbuff_rmi_pack_item(buff, m_hints);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack cpuset.
    return qvi_bbuff_rmi_pack_item(buff, m_affinity);
}

int
qvi_hwpool_cpu::unpack(
    byte_t *buffpos,
    size_t *bytes_written,
    qvi_hwpool_cpu &cpu
) {
    size_t bw = 0, total_bw = 0;
    // Unpack hints.
    int rc = qvi_bbuff_rmi_unpack_item(
        &cpu.m_hints, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;
    // Unpack bitmap.
    rc = qvi_bbuff_rmi_unpack_item(
        cpu.m_affinity, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        total_bw = 0;
    }
    *bytes_written = total_bw;
    return rc;
}

qvi_hwpool_dev::qvi_hwpool_dev(
    const qvi_hwloc_device_s &dev
) : m_type(dev.type)
  , m_affinity(dev.affinity)
  , m_id(dev.id)
  , m_pci_bus_id(dev.pci_bus_id)
  , m_uuid(dev.uuid) { }

qvi_hwpool_dev::qvi_hwpool_dev(
    const std::shared_ptr<qvi_hwloc_device_s> &shdev
) : qvi_hwpool_dev(*shdev.get()) { }

bool
qvi_hwpool_dev::operator==(
    const qvi_hwpool_dev &x
) const {
    return m_uuid == x.m_uuid;
}

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

int
qvi_hwpool_dev::packinto(
    qvi_bbuff *buff
) const {
    // Pack device hints.
    int rc = qvi_bbuff_rmi_pack_item(buff, m_hints);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack device affinity.
    rc = qvi_bbuff_rmi_pack_item(buff, m_affinity);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack device type.
    rc = qvi_bbuff_rmi_pack_item(buff, m_type);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack device ID.
    rc = qvi_bbuff_rmi_pack_item(buff, m_id);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack device PCI bus ID.
    rc = qvi_bbuff_rmi_pack_item(buff, m_pci_bus_id);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack device UUID.
    return qvi_bbuff_rmi_pack_item(buff, m_uuid);
}

int
qvi_hwpool_dev::unpack(
    byte_t *buffpos,
    size_t *bytes_written,
    qvi_hwpool_dev &dev
) {
    size_t bw = 0, total_bw = 0;

    int rc = qvi_bbuff_rmi_unpack_item(
        &dev.m_hints, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        dev.m_affinity, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &dev.m_type, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &dev.m_id, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        dev.m_pci_bus_id, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        dev.m_uuid, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        total_bw = 0;
    }
    *bytes_written = total_bw;
    return rc;
}

int
qvi_hwpool::m_add_devices_with_affinity(
    qvi_hwloc_t *hwloc
) {
    int rc = QV_SUCCESS;
    // Iterate over the supported device types.
    for (const auto devt : qvi_hwloc_supported_devices()) {
        qvi_hwloc_dev_list_t devs;
        rc = qvi_hwloc_get_devices_in_bitmap(
            hwloc, devt, m_cpu.affinity(), devs
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
qvi_hwpool::create(
    qvi_hwloc_t *hwloc,
    hwloc_const_cpuset_t cpuset,
    qvi_hwpool **hwpool
) {
    qvi_hwpool *ihwpool = nullptr;
    int rc = qvi_new(&ihwpool);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Initialize the hardware pool.
    rc = ihwpool->initialize(hwloc, cpuset);
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ihwpool);
    }
    *hwpool = ihwpool;
    return rc;
}

int
qvi_hwpool::initialize(
    qvi_hwloc_t *hwloc,
    hwloc_const_bitmap_t cpuset
) {
    const int rc = m_cpu.affinity().set(cpuset);
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
    qvi_hwloc_t *hwloc,
    qv_hw_obj_type_t obj_type,
    int *result
) const {
    if (qvi_hwloc_obj_type_is_host_resource(obj_type)) {
        return qvi_hwloc_get_nobjs_in_cpuset(
            hwloc, obj_type, m_cpu.affinity().cdata(), result
        );
    }
    *result = m_devs.count(obj_type);
    return QV_SUCCESS;
}

int
qvi_hwpool::add_device(
    const qvi_hwpool_dev &dev
) {
    auto shdev = std::make_shared<qvi_hwpool_dev>(dev);
    m_devs.insert({dev.type(), shdev});
    return QV_SUCCESS;
}

int
qvi_hwpool::release_devices(void)
{
    m_devs.clear();
    return QV_SUCCESS;
}

int
qvi_hwpool::packinto(
    qvi_bbuff *buff
) const {
    // Pack the CPU.
    int rc = qvi_bbuff_rmi_pack_item(buff, m_cpu);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack the number of devices.
    const size_t ndev = m_devs.size();
    rc = qvi_bbuff_rmi_pack_item(buff, ndev);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack the devices.
    for (const auto &dev : m_devs) {
        rc = qvi_bbuff_rmi_pack_item(buff, dev.second.get());
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    }
    return rc;
}

int
qvi_hwpool::unpack(
    byte_t *buffpos,
    size_t *bytes_written,
    qvi_hwpool **hwp
) {
    size_t bw = 0, total_bw = 0;
    // Create the new hardware pool.
    qvi_hwpool *ihwp = nullptr;
    int rc = qvi_new(&ihwp);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Unpack the CPU into the hardare pool.
    rc = qvi_bbuff_rmi_unpack_item(
        ihwp->m_cpu, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;
    // Unpack the number of devices.
    size_t ndev;
    rc = qvi_bbuff_rmi_unpack_item(
        &ndev, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;
    // Unpack and add the devices.
    for (size_t i = 0; i < ndev; ++i) {
        qvi_hwpool_dev dev;
        rc = qvi_bbuff_rmi_unpack_item(dev, buffpos, &bw);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
        total_bw += bw;
        buffpos += bw;
        // Add the unpacked device.
        rc = ihwp->add_device(dev);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
    }
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ihwp);
        total_bw = 0;
    }
    *bytes_written = total_bw;
    *hwp = ihwp;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
