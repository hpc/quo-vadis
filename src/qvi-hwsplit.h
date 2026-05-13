/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2026 Triad National Security, LLC
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
#include "qvi-scope.h"

// TODO Consider renaming members that can only be called by the root something
// obvious.

/**
 * A collection of information relevant to split operations requiring aggregated
 * (e.g., global) knowledge to perform a split.
 *
 * NOTE: since splitting and mapping operations are performed by a single
 * process, that process requires global knowledge of the split parameters.
 * Also, that process needs a way to relay that information to its members. This
 * is done in one of two ways: before starting threads or by group-level message
 * passing.
 */
struct qvi_hwsplit {
private:
    /** The root rank. */
    static constexpr int s_root = 0;
    /** Deleted default constructor. */
    qvi_hwsplit(void) = delete;
    /** Constructor. */
    qvi_hwsplit(
        qv_scope *parent,
        size_t group_size,
        size_t split_size,
        qv_hw_obj_type_t split_at_type
    );
    /** Destructor. */
    ~qvi_hwsplit(void) = default;
    /** A const reference to my RMI. */
    qvi_rmi_client &m_rmi;
    /** A const reference to my base hardware pool. */
    const qvi_hwpool &m_my_hwpool;
    /** The number of members that are part of the split. */
    size_t m_group_size = 0;
    /** The number of pieces in the split. */
    size_t m_split_size = 0;
    /**
     * The potential hardware resource that we are splitting at. QV_HW_OBJ_LAST
     * indicates that we are called from a split() context. Any other hardware
     * resource type indicates that we are splitting at that type: called from a
     * split_at() context.
     */
    qv_hw_obj_type_t m_split_at_type;
    /**
     * The base hardware pool that is to be split and operated on. This hardware
     * pool is created by the root by calculating a hardware union over the
     * m_my_hwpools owned by all the split participants. We do this so that all
     * hardware resources are consolidated into one hardware pool, valid only at
     * the root.
     */
    qvi_hwpool m_base_hwpool;
    /** The split member's current CPU affinity. */
    qvi_hwloc_bitmap m_my_cpu_affinity;
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
    std::vector<qvi_hwpool> m_hwpools;
    /**
     * Vector of colors, one for each member of the group. Note that the number
     * of colors will always match the group size and that their array index
     * corresponds to a task ID.
     */
    std::vector<int> m_colors;
    /** Vector of cpusets that resulted from the m_split() operation. */
    std::vector<qvi_hwloc_bitmap> m_split_cpusets;
    /** Vector of task affinities. */
    std::vector<qvi_hwloc_bitmap> m_task_affinities;
    /** Mapper configuration. */
    qvi_map_config m_map_config;
    /**
     * Resizes the relevant containers to make
     * room for |group size| number of elements.
     */
    void
    m_reserve(void);
    /** Gathers group-level split data to the specified root. */
    static int
    m_gather_split_data(
        const qvi_group &group,
        int rootid,
        qvi_hwsplit &hwsplit,
        int color
    );
    /**
     * Scatters split results from the specified root to other group members.
     */
    static int
    m_scatter_split_results(
        const qvi_group &group,
        int rootid,
        const qvi_hwsplit &hwsplit,
        int *colorp,
        qvi_hwpool &result
    );
    /**
     * Returns a pair where .first is the real split type identified and .second
     * is the hardware pool's primary cpuset for a given hardware object type.
     * This is the cpuset that is typically be used for splitting hardware
     * resources based on the provided hardware object type. For example, either
     * the cpuset of the hardware pool (CPU resources) or the union of the GPUs'
     * hardware affinities.
     */
    std::pair<qv_hw_obj_type_t, qvi_hwloc_bitmap>
    m_primary_cpuset_for_split(
        qv_hw_obj_type_t requested_type
    ) const;
    /** */
    int
    m_setup_map_config(void);
    /** */
    int
    m_apply_mapping(
        const qvi_map_t &map
    );
    /** Splits aggregate scope data. This can only be called by the root. */
    int
    m_split(void);
public:
    /** Performs a collective split. */
    static int
    split(
        qv_scope_t *parent,
        size_t npieces,
        int color,
        qv_hw_obj_type_t maybe_obj_type,
        int *colorp,
        qvi_hwpool &result
    );
    /** Performs a thread-split operation, returns relevant hardware pools. */
    static int
    thread_split(
        qv_scope_t *parent,
        size_t npieces,
        int *kcolors,
        size_t k,
        qv_hw_obj_type_t maybe_obj_type,
        std::vector<int> &kcolorps,
        std::vector<qvi_hwpool> &khwpools
    );
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
