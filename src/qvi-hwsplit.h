/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-hwsplit.h
 */

#ifndef QVI_HWSPLIT_H
#define QVI_HWSPLIT_H

#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-hwpool.h"
#include "qvi-map.h"

/**
 * Hardware split aggregation: a collection of information relevant to split
 * operations requiring aggregated (e.g., global) knowledge to perform a split.
 *
 * NOTE: since splitting and mapping operations are performed by a single
 * process, this structure does not support collective operations that require
 * coordination between cooperating tasks. The structure for that is
 * qvi_hwsplit_coll_s. Typically, collective operations will fill in a
 * this structure, but that isn't a requirement.
 */
struct qvi_hwsplit {
private:
    /** A pointer to my RMI. */
    qvi_rmi_client *m_rmi = nullptr;
    /** The base hardware pool we are splitting. */
    qvi_hwpool *m_hwpool = nullptr;
    /** The number of members that are part of the split. */
    uint_t m_group_size = 0;
    /** The number of pieces in the split. */
    uint_t m_split_size = 0;
    /**
     * The potential hardware resource that we are splitting at. QV_HW_OBJ_LAST
     * indicates that we are called from a split() context. Any other hardware
     * resource type indicates that we are splitting at that type: called from a
     * split_at() context.
     */
    qv_hw_obj_type_t m_split_at_type;
    /**
     * Vector of task TIDs, one for each member of the group. Note that the
     * number of task IDs will always match the group size and that their array
     * index corresponds to a task ID. It is handy to have the task IDs for
     * splitting so we can query task characteristics during a split.
     */
    std::vector<pid_t> m_group_tids;
    /**
     * Vector of hardware pools, one for each member of the group. Note that the
     * number of hardware pools will always match the group size and that their
     * array index corresponds to a task ID: 0 ... group_size - 1.
     */
    std::vector<qvi_hwpool *> m_hwpools;
    /**
     * Vector of colors, one for each member of the group. Note that the number
     * of colors will always match the group size and that their array index
     * corresponds to a task ID.
     */
    std::vector<int> m_colors;
    /** Vector of task affinities. */
    qvi_hwloc_cpusets_t m_affinities;
    /** */
    qvi_map_fn_t
    affinity_preserving_policy(void) const;
    /** */
    int
    split_affinity_preserving_pass1(void);
    /** User-defined split. */
    int
    split_user_defined(void);
    /** Affinity preserving split. */
    int
    split_affinity_preserving(void);
    /** */
    int
    split_packed(void);
    /** */
    int
    split_spread(void);
    /** Straightforward user-defined device splitting. */
    int
    split_devices_user_defined(void);
    /** Affinity preserving device splitting. */
    int
    split_devices_affinity_preserving(void);
public:
    // TODO(skg) Cleanup private, protected, public interfaces.
    /** Constructor. */
    qvi_hwsplit(void) = default;
    /** Constructor. */
    qvi_hwsplit(
        qvi_group *group,
        uint_t group_size,
        uint_t split_size,
        qv_hw_obj_type_t split_at_type
    );
    /** Destructor. */
    ~qvi_hwsplit(void);
    /**
     * Resizes the relevant containers to make
     * room for |group size| number of elements.
     */
    void
    reserve(void);
    /**
     * Returns a copy of the aggregate cpuset. Note that the cpuset will be shared
     * among the aggregate, but other resources may be distributed differently.
     * For example, some hardware pools may have GPUs, while others may not.
     */
    qvi_hwloc_bitmap
    cpuset(void) const;
    /**
     * Performs a straightforward splitting of the provided cpuset:
     * split the provided base cpuset into split_size distinct pieces.
     */
    int
    split_cpuset(
        qvi_hwloc_cpusets_t &result
    ) const;
    /** Performs a thread-split operation, returns relevant hardare pools. */
    static int
    thread_split(
        qv_scope_t *parent,
        uint_t npieces,
        int *kcolors,
        uint_t k,
        qv_hw_obj_type_t maybe_obj_type,
        qvi_hwpool ***khwpools
    );
    /** Returns device affinities that are part of the split. */
    int
    osdev_cpusets(
        qvi_hwloc_cpusets_t &result
    ) const;

    int
    primary_cpusets(
        qvi_hwloc_cpusets_t &result
    ) const;

    /** Releases all devices contained in the hardware split. */
    int
    release_devices(void);
    /** Splits aggregate scope data. */
    int
    split(void);

    int gather(
        qvi_group &group,
        const qvi_hwpool &hwpool,
        int color
    );

    int
    scatter(
        qvi_group &group,
        int *colorp,
        qvi_hwpool **result
    );
    // TODO(skg) Update arg order
    int
    split(
        qvi_group &group,
        const qvi_hwpool &hwpool,
        int color,
        int *colorp,
        qvi_hwpool **result
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
