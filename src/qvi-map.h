/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
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

/** Maintains a mapping between 'From IDs' to 'To IDs'. */
using qvi_map_t = std::map<uint_t, uint_t>;

/**
 * Defines a function pointer to a desired mapping function.
 */
using qvi_map_fn_t = std::function<
    int(qvi_map_t &map, uint_t nfids, const qvi_hwloc_cpusets_t &tres)
>;

/**
 * Maintains a mapping between resource IDs to a set of
 * consumer IDs that have shared affinity with a given resource.
 */
using qvi_map_shaffinity_t = std::map<uint_t, std::set<uint_t>>;

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
    uint_t cid
);

/**
 * Maps From IDs (fids) from 0...nfids-1 to resource indices (resource IDs)
 * by associating contiguous consumer IDs with resource IDs.
 */
int
qvi_map_packed(
    qvi_map_t &map,
    uint_t nfids,
    const qvi_hwloc_cpusets_t &tres
);

int
qvi_map_spread(
    qvi_map_t &map,
    uint_t nfids,
    const qvi_hwloc_cpusets_t &tres
);

/**
 * Calculates a shared affinity map of consumer IDs (from)
 * that have shared affinity with the resources (to).
 */
int
qvi_map_calc_shaffinity(
    const qvi_hwloc_cpusets_t &faffs,
    const qvi_hwloc_cpusets_t &tores,
    qvi_map_shaffinity_t &res_affinity_map
);

/**
 * The disjoint affinity mapper maps consuer IDs to resource IDs with NO shared
 * affinity. It assumes disjoint affinity in damap.
 */
int
qvi_map_disjoint_affinity(
    qvi_map_t &map,
    const qvi_map_shaffinity_t &damap
);

/**
 * Performs a mapping between the provided colors to the provided cpusets.
 */
int
qvi_map_colors(
    qvi_map_t &map,
    const std::vector<int> &fcolors,
    const qvi_hwloc_cpusets_t &tres
);

/**
 * Performs an affinity preserving mapping.
 */
int
qvi_map_affinity_preserving(
    qvi_map_t &map,
    const qvi_map_fn_t map_rest_fn,
    const qvi_hwloc_cpusets_t &faffs,
    const qvi_hwloc_cpusets_t &tores
);

/**
 * Returns the cpuset mapped to the given From ID.
 */
hwloc_const_cpuset_t
qvi_map_cpuset_at(
    const qvi_map_t &map,
    const qvi_hwloc_cpusets_t &cpusets,
    uint_t fid
);

std::vector<uint_t>
qvi_map_flatten(
    const qvi_map_t &map
);

std::vector<int>
qvi_map_flatten_to_colors(
    const qvi_map_t &map
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
