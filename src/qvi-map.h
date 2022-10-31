/*
 * Copyright (c) 2022      Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-map.h
 */

#ifndef QVI_MAP_H
#define QVI_MAP_H

#include "qvi-common.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus

/** Maintains a mapping between 'From IDs' to 'To IDs'. */
using qvi_map_t = std::map<int, int>;

/**
 * Maintains a mapping between resource IDs to a set of
 * consumer IDs that have shared affinity with a given resource.
 */
using qvi_map_shaffinity_t = std::map<int, std::set<int>>;

/**
 * Vector of cpusets.
 */
using qvi_map_cpusets_t = std::vector<hwloc_cpuset_t>;

/**
 * Prints debug output.
 */
void
qvi_map_debug_dump(
    const qvi_map_t &map
);

/**
 * Returns the number of From IDs that have already been mapped.
 */
uint_t
qvi_map_nfids_mapped(
    const qvi_map_t &map
);

/**
 * Returns whether or not the provided From ID is already mapped.
 */
bool
qvi_map_fid_mapped(
    const qvi_map_t &map,
    int cid
);

/**
 * Maps From IDs (fids) from 0...nfids-1 to resource indices (resource IDs)
 * by associating contiguous consumer IDs with resource IDs.
 */
int
qvi_map_packed(
    qvi_map_t &map,
    uint_t nfids,
    const qvi_map_cpusets_t &tres
);

int
qvi_map_disjoint_affinity(
    qvi_map_t &map,
    const qvi_map_shaffinity_t &damap
);

/**
 * Performs an affinity preserving mapping.
 */
int
qvi_map_affinity_preserving(
    qvi_map_t &map,
    const qvi_map_cpusets_t &faffs,
    const qvi_map_cpusets_t &tores
);

/**
 * Returns the cpuset mapped to the given From ID.
 */
hwloc_const_cpuset_t
qvi_map_cpuset_at(
    const qvi_map_t &map,
    const qvi_map_cpusets_t &cpusets,
    int fid
);

std::vector<int>
qvi_map_flatten(
    const qvi_map_t &map
);

#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the largest number that will fit in the space available.
 */
uint_t
qvi_map_maxfit(
    uint_t space_left,
    uint_t max_chunk
);

/**
 * Returns the max i per k.
 */
uint_t
qvi_map_maxiperk(
    uint_t i,
    uint_t k
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
