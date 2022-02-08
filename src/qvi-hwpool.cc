/*
 * Copyright (c)      2022 Triad National Security, LLC
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

// TODO(skg) Consider using std::shared_ptr elsewhere.
using qvi_devinfos_t = std::unordered_set<
    std::shared_ptr<qvi_hwpool_devinfo_t>
>;

/** Device information. */
struct qvi_hwpool_devinfo_s {
    /** Device type. */
    qv_hw_obj_type_t type = QV_HW_OBJ_LAST;
    /** Device ID. */
    int id = 0;
    /** The bitmap encoding CPU affinity. */
    hwloc_bitmap_t cpuset = nullptr;
    /** Constructor */
    qvi_hwpool_devinfo_s(
        qv_hw_obj_type_t t,
        int i,
        hwloc_const_cpuset_t c
    ) : type(t)
      , id(i)
      , cpuset(nullptr)
    {
        (void)qvi_hwloc_bitmap_calloc(&cpuset);
        (void)qvi_hwloc_bitmap_copy(c, cpuset);
    }
    /** Destructor */
    ~qvi_hwpool_devinfo_s(void)
    {
        qvi_hwloc_bitmap_free(&cpuset);
    }
    /** Equality operator. */
    bool
    operator==(const qvi_hwpool_devinfo_t &x) const
    {
        return id == x.id && type == x.type;
    }
};

struct qvi_hwpool_s {
    /** The cpuset of this resource pool. */
    hwloc_bitmap_t cpuset = nullptr;
    /** Device information. */
    qvi_devinfos_t *devinfos = nullptr;
    // TODO(skg) Add owner to structure?
    /** The obtained cpuset of this resource pool. */
    hwloc_bitmap_t obcpuset = nullptr;
};

/**
 * Extend namespace std so we can easily add qvi_hwpool_devinfo_ts to
 * unordered_sets.
 */
namespace std {
    template <>
    struct hash<qvi_hwpool_devinfo_t>
    {
        size_t
        operator()(const qvi_hwpool_devinfo_t &x) const
        {
            const int a = x.id;
            const int b = (int)x.type;
            // Cantor pairing function.
            const int64_t c = (a + b) * (a + b + 1) / 2 + b;
            return hash<int64_t>()(c);
        }
    };
}

int
qvi_hwpool_new(
    qvi_hwpool_t **rpool
) {
    int rc = QV_SUCCESS;

    qvi_hwpool_t *irpool = qvi_new qvi_hwpool_t;
    if (!irpool) {
        rc = QV_ERR_OOR;
        goto out;
    }

    irpool->devinfos = qvi_new qvi_devinfos_t;
    if (!irpool->devinfos) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rc = qvi_hwloc_bitmap_calloc(&irpool->cpuset);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwloc_bitmap_calloc(&irpool->obcpuset);
    if (rc != QV_SUCCESS) goto out;
out:
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(&irpool);
    }
    *rpool = irpool;
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

    rc = qvi_hwloc_bitmap_copy(line->cpuset, irpool->cpuset);
    if (rc != QV_SUCCESS) goto out;

    for (int i = 0; i < line->ndevinfos; ++i) {
        rc = qvi_hwpool_add_device(
            irpool,
            line->devinfos[i].type,
            line->devinfos[i].id,
            line->devinfos[i].cpuset
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
    const int ndevinfos = rpool->devinfos->size();

    qvi_line_hwpool_t *iline = nullptr;
    rc = qvi_line_hwpool_new(&iline);
    if (rc != QV_SUCCESS) goto out;
    // Initialize and copy the cpuset.
    rc = qvi_hwloc_bitmap_calloc(&iline->cpuset);
    if (rc != QV_SUCCESS) goto out;
    rc = qvi_hwloc_bitmap_copy(rpool->cpuset, iline->cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Initialize and fill in the device information.
    iline->ndevinfos = ndevinfos;
    iline->devinfos = qvi_new qvi_line_devinfo_t[ndevinfos];
    if (!iline->devinfos) {
        rc = QV_ERR_OOR;
        goto out;
    }
    do {
        int idx = 0;
        for (const auto &devinfo : *rpool->devinfos) {
            iline->devinfos[idx].type = devinfo->type;
            iline->devinfos[idx].id = devinfo->id;
            // Initialize and copy cpuset
            rc = qvi_hwloc_bitmap_calloc(
                &iline->devinfos[idx].cpuset
            );
            if (rc != QV_SUCCESS) break;
            rc = qvi_hwloc_bitmap_copy(
                devinfo->cpuset, iline->devinfos[idx].cpuset
            );
            if (rc != QV_SUCCESS) break;
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

// TODO(skg) Add RMI to release resources.
void
qvi_hwpool_free(
    qvi_hwpool_t **rpool
) {
    if (!rpool) return;
    qvi_hwpool_t *irpool = *rpool;
    if (!irpool) goto out;
    if (irpool->devinfos) {
        delete irpool->devinfos;
    }
    qvi_hwloc_bitmap_free(&irpool->cpuset);
    qvi_hwloc_bitmap_free(&irpool->obcpuset);
    delete irpool;
out:
    *rpool = nullptr;
}

int
qvi_hwpool_init(
    qvi_hwpool_t *rpool,
    hwloc_const_bitmap_t cpuset
) {
    return qvi_hwloc_bitmap_copy(cpuset, rpool->cpuset);
}

int
qvi_hwpool_add_device(
    qvi_hwpool_t *rpool,
    qv_hw_obj_type_t type,
    int id,
    hwloc_const_cpuset_t cpuset
) {
    const bool itp = rpool->devinfos->insert(
        std::make_shared<qvi_hwpool_devinfo_t>(type, id, cpuset)
    ).second;
    // insert().second returns whether or not item insertion took place. If
    // true, this is a new, unseen device that we have inserted.
    if (!itp) {
        qvi_log_debug("Duplicate device (ID={}) found of type {}", id, type);
        return QV_ERR_INTERNAL;
    }
    return QV_SUCCESS;
}

hwloc_const_cpuset_t
qvi_hwpool_cpuset_get(
    qvi_hwpool_t *rpool
) {
    if (!rpool) return nullptr;
    return rpool->cpuset;
}

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

/**
 * Adds devices to an initialized hardware resource pool.
 */
static int
pool_add_devices(
    qvi_hwpool_t *pool,
    qvi_hwloc_t *hwloc
) {
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    int rc = QV_SUCCESS;
    // Iterate over the supported device types.
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        const qv_hw_obj_type_t type = devts[i];
        int nobjs = 0;
        rc = qvi_hwloc_get_nobjs_in_cpuset(
            hwloc, type, pool->cpuset, &nobjs
        );
        if (rc != QV_SUCCESS) break;
        // Iterate over the number of devices in each type.
        for (int devi = 0; devi < nobjs; ++devi) {
            char *devids = nullptr;
            rc = qvi_hwloc_get_device_in_cpuset(
                hwloc, type, devi, pool->cpuset,
                QV_DEVICE_ID_ORDINAL, &devids
            );
            if (rc != QV_SUCCESS) break;
            // Convert to int.
            int devid = 0;
            rc = qvi_atoi(devids, &devid);
            if (rc != QV_SUCCESS) break;
            // Get the device affinity.
            hwloc_bitmap_t devaff = nullptr;
            rc = qvi_hwloc_get_device_cpuset(
                hwloc, type, devid, &devaff
            );
            if (rc != QV_SUCCESS) break;
            // Add it to the pool.
            rc = qvi_hwpool_add_device(
                pool, type, devid, devaff
            );
            if (rc != QV_SUCCESS) break;
            //
            qvi_hwloc_bitmap_free(&devaff);
            free(devids);
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
    // Add devices.
    // // TODO(skg) Acquire devices.
    rc = pool_add_devices(ipool, hwloc);
out:
    if (rc != QV_SUCCESS) {
        qvi_hwpool_free(&ipool);
    }
    *opool = ipool;
    return rc;
}

/**
 *
 */
int
qvi_hwpool_split_devices(
    qvi_hwpool_t **pools,
    int npools,
    qvi_hwloc_t *hwloc,
    int ncolors,
    int color
) {
    int rc = QV_SUCCESS;
    // The hardware pools should have a common ancestor, so the device lists
    // should be itentical. Check in debug mode to make sure. The first pool
    // will serve as the device pool that we split among all incoming pools.
    qvi_hwpool_t *devpool = pools[0];
#if QVI_DEBUG_MODE == 1
#if 0
    qvi_device_tab_t *gold = devpool->devinfos;
    for (int i = 0; i < npools; ++i) {
        qvi_hwpool_t *pool = pools[i];
        assert(gold->size() == pool->devinfos->size());
        for (const auto &devt : *pool->devinfos) {
            assert(gold->at(devt.first).size() == devt.second.size());
            for (const auto &devid : devt.second) {
                const auto &got = gold->at(devt.first).find(devid);
                assert(got != gold->at(devt.first).end());
            }
        }
    }
#endif
#endif
    for (const auto &devt : *devpool->devinfos) {
    }
    return rc;
}

int
qvi_hwpool_obtain_split_by_group(
    qvi_hwpool_t *pool,
    qvi_hwloc_t *hwloc,
    int npieces,
    int group_id,
    qvi_hwpool_t **opool
) {
    qvi_hwpool_t *ipool = nullptr;

    hwloc_cpuset_t cpuset = nullptr;
    int rc = qvi_hwloc_split_cpuset_by_group_id(
        hwloc,
        pool->cpuset,
        npieces,
        group_id,
        &cpuset
    );
    if (rc != QV_SUCCESS) return rc;

    rc = pool_obtain_cpus_by_cpuset(pool, cpuset);
    if (rc != QV_SUCCESS) goto out;
    // We obtained the CPUs, so create the new pool.
    rc = qvi_hwpool_new(&ipool);
    if (rc != QV_SUCCESS) goto out;
    // Initialize the hardware pool.
    rc = qvi_hwpool_init(ipool, cpuset);
    if (rc != QV_SUCCESS) goto out;
    // Split the devices.
#if 0
    rc = pool_split_devices(
        pool,
        hwloc,
        npieces,
        group_id,
        ipool
    );
#endif
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
 *
 */
