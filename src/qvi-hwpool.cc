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

#include "qvi-common.h"

#include "qvi-hwpool.h"
#include "qvi-hwloc.h"
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
     * Maps resource IDs to hint flags that they might have associated with
     * them.
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

    qvi_hwpool_cpus_s(void)
    {
        qvim_rc = qvi_construct_rc(cpuset);
    }

    virtual
    ~qvi_hwpool_cpus_s(void) = default;
};

struct qvi_hwpool_s {
    int qvim_rc = QV_ERR_INTERNAL;
    /** The CPUs in the resource pool. */
    qvi_hwpool_cpus_s cpus;
    /** Device information. */
    qvi_hwpool_devinfos_t devinfos;
    // TODO(skg) Add owner to structure?
    /** The obtained cpuset of this resource pool. */
    hwloc_bitmap_t obcpuset = nullptr;

    qvi_hwpool_s(void)
    {
        qvim_rc = qvi_construct_rc(cpus);
        if (qvim_rc != QV_SUCCESS) return;

        qvim_rc = qvi_hwloc_bitmap_calloc(&obcpuset);
    }

    ~qvi_hwpool_s(void)
    {
        qvi_hwloc_bitmap_free(&obcpuset);
    }
};

int
qvi_hwpool_new(
    qvi_hwpool_t **rpool
) {
    return qvi_new_rc(rpool);
}

int
qvi_hwpool_new_from_line(
    qvi_line_hwpool_t *line,
    qvi_hwpool_t **rpool
) {
    qvi_hwpool_t *irpool = nullptr;
    int rc = qvi_hwpool_new(&irpool);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_init(irpool, line->cpuset);
    if (rc != QV_SUCCESS) goto out;

    for (int i = 0; i < line->ndevinfos; ++i) {
        rc = qvi_hwpool_add_device(
            irpool,
            line->devinfos[i].type,
            line->devinfos[i].id,
            line->devinfos[i].pci_bus_id,
            line->devinfos[i].uuid,
            line->devinfos[i].affinity
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
    const int ndevinfos = rpool->devinfos.size();

    qvi_line_hwpool_t *iline = nullptr;
    rc = qvi_line_hwpool_new(&iline);
    if (rc != QV_SUCCESS) goto out;
    // Duplicate the cpuset.
    rc = qvi_hwloc_bitmap_dup(rpool->cpus.cpuset.data, &iline->cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Initialize and fill in the device information.
    iline->ndevinfos = ndevinfos;
    iline->devinfos = qvi_new qvi_line_devinfo_t[ndevinfos]();
    if (!iline->devinfos) {
        rc = QV_ERR_OOR;
        goto out;
    }
    do {
        int idx = 0, nw = 0;
        for (const auto &dinfo : rpool->devinfos) {
            iline->devinfos[idx].type = dinfo.second->type;
            iline->devinfos[idx].id = dinfo.second->id;
            // Duplicate the cpuset
            rc = qvi_hwloc_bitmap_dup(
                dinfo.second->affinity,
                &iline->devinfos[idx].affinity
            );
            if (rc != QV_SUCCESS) break;
            nw = asprintf(
                &iline->devinfos[idx].pci_bus_id,
                "%s", dinfo.second->pci_bus_id
            );
            if (nw == -1) {
                rc = QV_ERR_OOR;
                break;
            }
            nw = asprintf(
                &iline->devinfos[idx].uuid,
                "%s", dinfo.second->uuid
            );
            if (nw == -1) {
                rc = QV_ERR_OOR;
                break;
            }
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

void
qvi_hwpool_free(
    qvi_hwpool_t **rpool
) {
    qvi_delete(rpool);
}

int
qvi_hwpool_init(
    qvi_hwpool_t *rpool,
    hwloc_const_bitmap_t cpuset
) {
    return qvi_hwloc_bitmap_copy(cpuset, rpool->cpus.cpuset.data);
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
    auto dinfo = std::make_shared<qvi_devinfo_t>(
        type, id, pcibid, uuid, affinity
    );
    const int rc = qvi_construct_rc(dinfo);
    if (rc != QV_SUCCESS) return rc;
    rpool->devinfos.insert({type, dinfo});
    return rc;
}

int
qvi_hwpool_release_devices(
    qvi_hwpool_t *pool
) {
    pool->devinfos.clear();
    return QV_SUCCESS;
}

hwloc_const_cpuset_t
qvi_hwpool_cpuset_get(
    qvi_hwpool_t *rpool
) {
    assert(rpool);
    return rpool->cpus.cpuset.data;
}

const qvi_hwpool_devinfos_t *
qvi_hwpool_devinfos_get(
    qvi_hwpool_t *pool
) {
    assert(pool);
    return &pool->devinfos;
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
    int hwrc = hwloc_bitmap_or(
        pool->obcpuset,
        pool->obcpuset,
        request
    );
    return (hwrc == 0 ? QV_SUCCESS : QV_ERR_HWLOC);
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
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        const qv_hw_obj_type_t type = devts[i];
        int nobjs = 0;
        rc = qvi_hwloc_get_nobjs_in_cpuset(
            hwloc, type, pool->cpus.cpuset.data, &nobjs
        );
        if (rc != QV_SUCCESS) break;
        // Iterate over the number of devices in each type.
        for (int devi = 0; devi < nobjs; ++devi) {
            char *devids = nullptr, *pcibid = nullptr, *uuids = nullptr;
            // Device ID
            rc = qvi_hwloc_get_device_in_cpuset(
                hwloc, type, devi, pool->cpus.cpuset.data,
                QV_DEVICE_ID_ORDINAL, &devids
            );
            if (rc != QV_SUCCESS) break;
            // Convert to int.
            int devid = 0;
            rc = qvi_atoi(devids, &devid);
            if (rc != QV_SUCCESS) break;
            // PCI Bus ID
            rc = qvi_hwloc_get_device_in_cpuset(
                hwloc, type, devi, pool->cpus.cpuset.data,
                QV_DEVICE_ID_PCI_BUS_ID, &pcibid
            );
            if (rc != QV_SUCCESS) break;
            // UUID
            rc = qvi_hwloc_get_device_in_cpuset(
                hwloc, type, devi, pool->cpus.cpuset.data,
                QV_DEVICE_ID_UUID, &uuids
            );
            if (rc != QV_SUCCESS) break;
            // Get the device affinity.
            hwloc_bitmap_t devaff = nullptr;
            rc = qvi_hwloc_get_device_affinity(
                hwloc, type, devid, &devaff
            );
            if (rc != QV_SUCCESS) break;
            // Add it to the pool.
            rc = qvi_hwpool_add_device(
                pool, type, devid, pcibid, uuids, devaff
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
    // Convert input data to line protocol.
    qvi_line_hwpool_t *line = nullptr;
    int rc = qvi_hwpool_new_line_from_hwpool(hwp, &line);
    if (rc != QV_SUCCESS) return rc;
    // Pack the data
    rc = qvi_line_hwpool_pack(line, buff);
    qvi_line_hwpool_free(&line);
    return rc;
}

int
qvi_hwpool_unpack(
    void *buff,
    qvi_hwpool_t **hwp
) {
    qvi_line_hwpool_t *line = nullptr;
    int rc = qvi_line_hwpool_unpack(buff, &line);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_new_from_line(line, hwp);
out:
    qvi_line_hwpool_free(&line);
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(hwp);
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
