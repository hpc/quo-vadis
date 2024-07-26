/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
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

// TODOs
// * Resource reference counting.
// * Need to deal with resource unavailability.
// * Split and attach devices properly.
// * Have bitmap scratch pad that is initialized once, then destroyed? This
//   approach may be an nice allocation optimization, but in heavily threaded
//   code may be a bottleneck.


// Notes:
// * Does it make sense attempting resource exclusivity? Why not just let the
// users get what they ask for and hope that the abstractions that we provide do
// a good enough job most of the time. Making the user deal with resource
// exhaustion and retries (which will eventually be the case with
// QV_RES_UNAVAILABLE) is error prone and often frustrating.
//
// * Reference Counting: we should probably still implement a rudimentary
// reference counting system, but perhaps not for enforcing resource
// exclusivity. Rather we could use this information to guide a collection of
// resource allocators that would use resource availability for their pool
// management strategies.

// A Straightforward Reference Counting Approach: Maintain an array of integers
// with length number of cpuset bits. As each resource (bitmap) is obtained,
// increment the internal counter of each corresponding position. When a
// resource is released, decrement in a similar way. If a location in the array
// is zero, then the resource is not in use. For devices, we can take a similar
// approach using the device IDs instead of the bit positions.

#include "qvi-hwpool.h"
#include "qvi-bbuff-rmi.h"
#include "qvi-utils.h"

#if 0
/**
 *
 */
static int
cpus_available(
    hwloc_const_cpuset_t which,
    hwloc_const_cpuset_t from,
    bool *avail
) {
    // TODO(skg) Cache storage for calculation?
    hwloc_cpuset_t tcpus = nullptr;
    int rc = qvi_hwloc_bitmap_calloc(&tcpus);
    if (rc != QV_SUCCESS) return rc;

    int hrc = hwloc_bitmap_and(tcpus, which, from);
    if (hrc != 0) {
        rc = QV_ERR_HWLOC;
    }
    if (rc == QV_SUCCESS) {
        *avail = cpusets_equal(tcpus, which);
    }
    else {
        *avail = false;
    }
    qvi_hwloc_bitmap_free(&tcpus);
    return rc;
}
#endif

/**
 * Example:
 * obcpuset  0110 0101
 * request   1000 1010
 * obcpuset' 1110 1111
 */
#if 0
static int
pool_obtain_cpus_by_cpuset(
    qvi_hwpool_s *pool,
    hwloc_const_cpuset_t request
) {
#if 0
    int hwrc = hwloc_bitmap_or(
        pool->obcpuset,
        pool->obcpuset,
        request
    );
    return (hwrc == 0 ? QV_SUCCESS : QV_ERR_HWLOC);
#endif
    QVI_UNUSED(pool);
    QVI_UNUSED(request);
    return QV_SUCCESS;
}
#endif

/**
 * Example:
 * obcpuset  0110 0101
 * release   0100 0100
 * obcpuset' 0010 0001
 */
#if 0
static int
pool_release_cpus_by_cpuset(
    qvi_hwpool_s *pool,
    hwloc_const_cpuset_t release
) {
    int hwrc = hwloc_bitmap_andnot(
        pool->obcpuset,
        pool->obcpuset,
        release
    );
    return (hwrc == 0 ? QV_SUCCESS : QV_ERR_HWLOC);
}
#endif

qvi_hwloc_bitmap_s &
qvi_hwpool_cpu_s::cpuset(void)
{
    return m_cpuset;
}

const qvi_hwloc_bitmap_s &
qvi_hwpool_cpu_s::cpuset(void)
    const {
    return m_cpuset;
}

int
qvi_hwpool_cpu_s::packinto(
    qvi_bbuff_t *buff
) const {
    // Pack hints.
    const int rc = qvi_bbuff_rmi_pack_item(buff, m_hints);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Pack cpuset.
    return qvi_bbuff_rmi_pack_item(buff, m_cpuset);
}

int
qvi_hwpool_cpu_s::unpack(
    byte_t *buffpos,
    size_t *bytes_written,
    qvi_hwpool_cpu_s &cpu
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
        cpu.m_cpuset, buffpos, &bw
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

qvi_hwpool_dev_s::qvi_hwpool_dev_s(
    const qvi_hwloc_device_s &dev
) : m_type(dev.type)
  , m_affinity(dev.affinity)
  , m_id(dev.id)
  , m_pci_bus_id(dev.pci_bus_id)
  , m_uuid(dev.uuid) { }

qvi_hwpool_dev_s::qvi_hwpool_dev_s(
    const std::shared_ptr<qvi_hwloc_device_s> &shdev
) : qvi_hwpool_dev_s(*shdev.get()) { }

bool
qvi_hwpool_dev_s::operator==(
    const qvi_hwpool_dev_s &x
) const {
    return m_uuid == x.m_uuid;
}

qv_hw_obj_type_t
qvi_hwpool_dev_s::type(void)
    const {
    return m_type;
}

int
qvi_hwpool_dev_s::id(
    qv_device_id_type_t format,
    char **result
) {
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

const qvi_hwloc_bitmap_s &
qvi_hwpool_dev_s::affinity(void)
    const {
    return m_affinity;
}

int
qvi_hwpool_dev_s::packinto(
    qvi_bbuff_t *buff
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
qvi_hwpool_dev_s::unpack(
    byte_t *buffpos,
    size_t *bytes_written,
    qvi_hwpool_dev_s *dev
) {
    size_t bw = 0, total_bw = 0;

    int rc = qvi_bbuff_rmi_unpack_item(
        &dev->m_hints, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        dev->m_affinity, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &dev->m_type, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &dev->m_id, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        dev->m_pci_bus_id, buffpos, &bw
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        dev->m_uuid, buffpos, &bw
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
qvi_hwpool_s::add_devices_with_affinity(
    qvi_hwloc_t *hwloc
) {
    int rc = QV_SUCCESS;
    // Iterate over the supported device types.
    for (const auto devt : qvi_hwloc_supported_devices()) {
        qvi_hwloc_dev_list_t devs;
        rc = qvi_hwloc_get_devices_in_bitmap(
            hwloc, devt, m_cpu.cpuset(), devs
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        for (const auto &dev : devs) {
            rc = add_device(qvi_hwpool_dev_s(dev));
            if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
        }
    }
    return rc;
}

int
qvi_hwpool_s::new_hwpool(
    qvi_hwloc_t *hwloc,
    hwloc_const_cpuset_t cpuset,
    qvi_hwpool_s **opool
) {
    qvi_hwpool_s *ipool = nullptr;
    int rc = qvi_new(&ipool);
    if (qvi_unlikely(rc != QV_SUCCESS)) goto out;
    // Initialize the hardware pool.
    rc = ipool->initialize(hwloc, cpuset);
out:
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&ipool);
    }
    *opool = ipool;
    return rc;
}

int
qvi_hwpool_s::initialize(
    qvi_hwloc_t *hwloc,
    hwloc_const_bitmap_t cpuset
) {
    const int rc = m_cpu.cpuset().set(cpuset);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // Add devices with affinity to the hardware pool.
    return add_devices_with_affinity(hwloc);
}

const qvi_hwloc_bitmap_s &
qvi_hwpool_s::cpuset(void) const
{
    return m_cpu.cpuset();
}

const qvi_hwpool_devs_t &
qvi_hwpool_s::devices(void) const
{
    return m_devs;
}

int
qvi_hwpool_s::nobjects(
    qvi_hwloc_t *hwloc,
    qv_hw_obj_type_t obj_type,
    int *result
) {
    if (qvi_hwloc_obj_type_is_host_resource(obj_type)) {
        return qvi_hwloc_get_nobjs_in_cpuset(
            hwloc, obj_type, m_cpu.cpuset().cdata(), result
        );
    }
    *result = m_devs.count(obj_type);
    return QV_SUCCESS;
}

int
qvi_hwpool_s::add_device(
    const qvi_hwpool_dev_s &dev
) {
    auto shdev = std::make_shared<qvi_hwpool_dev_s>(dev);
    m_devs.insert({dev.type(), shdev});
    return QV_SUCCESS;
}

int
qvi_hwpool_s::release_devices(void)
{
    m_devs.clear();
    return QV_SUCCESS;
}

int
qvi_hwpool_s::packinto(
    qvi_bbuff_t *buff
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
qvi_hwpool_s::unpack(
    byte_t *buffpos,
    size_t *bytes_written,
    qvi_hwpool_s **hwp
) {
    size_t bw = 0, total_bw = 0;
    // Create the new hardware pool.
    qvi_hwpool_s *ihwp = nullptr;
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
        qvi_hwpool_dev_s dev;
        rc = qvi_bbuff_rmi_unpack_item(
            &dev, buffpos, &bw
        );
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

#if 0
/**
 * Extend namespace std so we can easily add qvi_devinfo_ts to
 * unordered_sets.
 */
namespace std {
    template <>
    struct hash<qvi_hwpool_dev_s>
    {
        size_t
        operator()(const qvi_hwpool_dev_s &x) const
        {
            const int a = x.m_id;
            const int b = (int)x.type;
            const int64_t c = qvi_cantor_pairing(a, b);
            return hash<int64_t>()(c);
        }
    };
}
#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
