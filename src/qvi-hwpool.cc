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

int
qvi_hwpool_s::new_hwpool_by_cpuset(
    qvi_hwloc_t *hwloc,
    hwloc_const_cpuset_t cpuset,
    qvi_hwpool_s **opool
) {
    // We obtained the CPUs, so create the new pool.
    qvi_hwpool_s *ipool = nullptr;
    int rc = qvi_new_rc(&ipool);
    if (rc != QV_SUCCESS) goto out;
    // Initialize the hardware pool.
    rc = ipool->initialize(cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Add devices with affinity to the new hardware pool.
    // TODO(skg) Acquire devices.
    rc = ipool->add_devices_with_affinity(hwloc);
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ipool);
    }
    *opool = ipool;
    return rc;
}

int
qvi_hwpool_s::pack(
    qvi_bbuff_t *buff
) {
    return qvi_bbuff_rmi_pack(buff, this);
}

int
qvi_hwpool_s::unpack(
    qvi_bbuff_t *buff,
    qvi_hwpool_s **hwp
) {
    return qvi_bbuff_rmi_unpack(qvi_bbuff_data(buff), hwp);
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
