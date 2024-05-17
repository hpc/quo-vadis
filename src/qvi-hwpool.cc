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

qvi_hwpool_dev_s::qvi_hwpool_dev_s(
    qv_hw_obj_type_t type_a,
    int id_a,
    cstr_t pci_bus_id_a,
    cstr_t uuid_a,
    hwloc_const_cpuset_t affinity_a
) : type(type_a)
  , id(id_a)
  , pci_bus_id(pci_bus_id_a)
  , uuid(uuid_a)
{
    qvim_rc = affinity.set(affinity_a);
}

struct qvi_hwpool_s {
    int qvim_rc = QV_ERR_INTERNAL;
    /** The hardware pool's CPU. */
    qvi_hwpool_cpu_s cpu;
    /** The hardware pool's devices. */
    qvi_hwpool_devs_t devs;
    /** Constructor */
    qvi_hwpool_s(void)
    {
        qvim_rc = qvi_construct_rc(cpu);
    }
    /** Destructor */
    ~qvi_hwpool_s(void) = default;
};

int
qvi_hwpool_new(
    qvi_hwpool_t **rpool
) {
    return qvi_new_rc(rpool);
}

void
qvi_hwpool_free(
    qvi_hwpool_t **rpool
) {
    qvi_delete(rpool);
}

int
qvi_hwpool_dup(
    const qvi_hwpool_t *const rpool,
    qvi_hwpool_t **dup
) {
    qvi_hwpool_t *idup = nullptr;
    int rc = qvi_hwpool_new(&idup);
    if (rc != QV_SUCCESS) goto out;
    // This performs a deep copy of the underlying CPUs.
    idup->cpu = rpool->cpu;
    // Assignment here sets qvim_rc, so check it.
    if (idup->qvim_rc != QV_SUCCESS) {
        rc = idup->qvim_rc;
        goto out;
    }
    // This performs a deep copy of the underlying device infos.
    idup->devs = rpool->devs;
out:
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(&idup);
    }
    *dup = idup;
    return rc;
}

int
qvi_hwpool_new_from_line(
    qvi_line_hwpool_t *line,
    qvi_hwpool_t **rpool
) {
    qvi_hwpool_t *irpool = nullptr;
    int rc = qvi_hwpool_new(&irpool);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_init(irpool, line->cpuset.data);
    if (rc != QV_SUCCESS) goto out;

    for (int i = 0; i < line->ndevinfos; ++i) {
        rc = qvi_hwpool_add_device(
            irpool,
            line->devinfos[i].type,
            line->devinfos[i].id,
            line->devinfos[i].pci_bus_id.c_str(),
            line->devinfos[i].uuid.c_str(),
            line->devinfos[i].affinity.data
        );
        if (rc != QV_SUCCESS) break;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(&irpool);
    }
    *rpool = irpool;
    return rc;
}

int
qvi_hwpool_new_line_from_hwpool(
    const qvi_hwpool_t *rpool,
    qvi_line_hwpool_t **line
) {
    int rc = QV_SUCCESS;
    const int ndevinfos = rpool->devs.size();

    qvi_line_hwpool_t *iline = nullptr;
    rc = qvi_line_hwpool_new(&iline);
    if (rc != QV_SUCCESS) goto out;
    iline->cpuset = rpool->cpu.cpuset;
    // Initialize and fill in the device information.
    iline->ndevinfos = ndevinfos;
    iline->devinfos.resize(ndevinfos);
    do {
        int idx = 0;
        for (const auto &dinfo : rpool->devs) {
            // TODO(skg) Use copy constructor?
            iline->devinfos[idx].type = dinfo.second->type;
            iline->devinfos[idx].affinity = dinfo.second->affinity;
            iline->devinfos[idx].id = dinfo.second->id;
            iline->devinfos[idx].pci_bus_id = dinfo.second->pci_bus_id;
            iline->devinfos[idx].uuid = dinfo.second->uuid;
            idx++;
        }
    } while (0);
out:
    if (rc != QV_SUCCESS) {
        qvi_line_hwpool_free(&iline);
    }
    *line = iline;
    return rc;
}

int
qvi_hwpool_init(
    qvi_hwpool_t *rpool,
    hwloc_const_bitmap_t cpuset
) {
    return qvi_hwloc_bitmap_copy(cpuset, rpool->cpu.cpuset.data);
}

int
qvi_hwpool_add_device(
    qvi_hwpool_t *rpool,
    qv_hw_obj_type_t type,
    int id,
    cstr_t pcibid,
    cstr_t uuid,
    hwloc_const_cpuset_t affinity
) {
    auto dinfo = std::make_shared<qvi_hwpool_dev_s>(
        type, id, pcibid, uuid, affinity
    );
    const int rc = qvi_construct_rc(dinfo);
    if (rc != QV_SUCCESS) return rc;
    rpool->devs.insert({type, dinfo});
    return rc;
}

int
qvi_hwpool_release_devices(
    qvi_hwpool_t *pool
) {
    pool->devs.clear();
    return QV_SUCCESS;
}

hwloc_const_cpuset_t
qvi_hwpool_cpuset_get(
    qvi_hwpool_t *rpool
) {
    assert(rpool);
    return rpool->cpu.cpuset.data;
}

const qvi_hwpool_devs_t *
qvi_hwpool_devinfos_get(
    qvi_hwpool_t *pool
) {
    assert(pool);
    return &pool->devs;
}

#if 0
/**
 * Returns whether two cpusets are equal.
 */
static bool
cpusets_equal(
    hwloc_const_cpuset_t a,
    hwloc_const_cpuset_t b
) {
    return hwloc_bitmap_compare(a, b) == 0;
}

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
static int
pool_obtain_cpus_by_cpuset(
    qvi_hwpool_t *pool,
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

/**
 * Example:
 * obcpuset  0110 0101
 * release   0100 0100
 * obcpuset' 0010 0001
 */
#if 0
static int
pool_release_cpus_by_cpuset(
    qvi_hwpool_t *pool,
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

int
qvi_hwpool_add_devices_with_affinity(
    qvi_hwpool_t *pool,
    qvi_hwloc_t *hwloc
) {
    int rc = QV_SUCCESS;
    // Iterate over the supported device types.
    for (const auto devt : qvi_hwloc_supported_devices()) {
        int nobjs = 0;
        rc = qvi_hwloc_get_nobjs_in_cpuset(
            hwloc, devt, pool->cpu.cpuset.data, &nobjs
        );
        if (rc != QV_SUCCESS) break;
        // Iterate over the number of devices in each type.
        for (int devi = 0; devi < nobjs; ++devi) {
            char *devids = nullptr, *pcibid = nullptr, *uuids = nullptr;
            // Device ID
            rc = qvi_hwloc_get_device_in_cpuset(
                hwloc, devt, devi, pool->cpu.cpuset.data,
                QV_DEVICE_ID_ORDINAL, &devids
            );
            if (rc != QV_SUCCESS) break;
            // Convert to int.
            int devid = 0;
            rc = qvi_atoi(devids, &devid);
            if (rc != QV_SUCCESS) break;
            // PCI Bus ID
            rc = qvi_hwloc_get_device_in_cpuset(
                hwloc, devt, devi, pool->cpu.cpuset.data,
                QV_DEVICE_ID_PCI_BUS_ID, &pcibid
            );
            if (rc != QV_SUCCESS) break;
            // UUID
            rc = qvi_hwloc_get_device_in_cpuset(
                hwloc, devt, devi, pool->cpu.cpuset.data,
                QV_DEVICE_ID_UUID, &uuids
            );
            if (rc != QV_SUCCESS) break;
            // Get the device affinity.
            hwloc_bitmap_t devaff = nullptr;
            rc = qvi_hwloc_get_device_affinity(
                hwloc, devt, devid, &devaff
            );
            if (rc != QV_SUCCESS) break;
            // Add it to the pool.
            rc = qvi_hwpool_add_device(
                pool, devt, devid, pcibid, uuids, devaff
            );
            if (rc != QV_SUCCESS) break;
            //
            free(devids);
            free(pcibid);
            free(uuids);
            qvi_hwloc_bitmap_free(&devaff);
        }
        if (rc != QV_SUCCESS) break;
    }
    return rc;
}

int
qvi_hwpool_obtain_by_cpuset(
    qvi_hwpool_t *pool,
    qvi_hwloc_t *hwloc,
    hwloc_const_cpuset_t cpuset,
    qvi_hwpool_t **opool
) {
    qvi_hwpool_t *ipool = nullptr;

    int rc = pool_obtain_cpus_by_cpuset(pool, cpuset);
    if (rc != QV_SUCCESS) goto out;
    // We obtained the CPUs, so create the new pool.
    rc = qvi_hwpool_new(&ipool);
    if (rc != QV_SUCCESS) goto out;
    // Initialize the hardware pool.
    rc = qvi_hwpool_init(ipool, cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Add devices with affinity to the new hardware pool.
    // TODO(skg) Acquire devices.
    rc = qvi_hwpool_add_devices_with_affinity(ipool, hwloc);
out:
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(&ipool);
    }
    *opool = ipool;
    return rc;
}

int
qvi_hwpool_pack(
    const qvi_hwpool_t *hwp,
    qvi_bbuff_t *buff
) {
    return qvi_bbuff_rmi_pack(buff, hwp);
}

int
qvi_hwpool_unpack(
    void *buff,
    qvi_hwpool_t **hwp
) {
    return qvi_bbuff_rmi_unpack(buff, hwp);
}

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
            const int a = x.id;
            const int b = (int)x.type;
            const int64_t c = qvi_cantor_pairing(a, b);
            return hash<int64_t>()(c);
        }
    };
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
