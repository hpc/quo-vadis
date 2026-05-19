/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2026 Triad National Security, LLC
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

#include "qvi-hwloc.h"

struct qvi_map_config;

/**
 * Maintains a mapping between a source IDs and their destination IDs. Source
 * IDs shall be unique, whereas destination sets may have intersecting values.
 * Insertion of a duplicate source ID into the map will result in the associated
 * destination ID being added to the destination set, if not already present.
 */
using qvi_map_t = std::map<size_t, std::set<size_t>>;

/**
 * Defines a function pointer to a desired mapping function.
 */
using qvi_map_fn_t = std::function<
    int(
        const qvi_map_config &config,
        qvi_map_t &map
    )
>;

struct qvi_map_config {
    size_t nsrc;
    size_t ndst;
    std::vector<qvi_hwloc_bitmap> src_affinities;
    std::vector<qvi_hwloc_bitmap> dst_affinities;
    std::vector<int> src_colors;
    qvi_map_fn_t map_fn;

    qvi_map_config(void) = default;

    qvi_map_config(
        size_t nsrc,
        size_t ndst,
        qvi_map_fn_t map_fn = {}
    ) : nsrc(nsrc)
      , ndst(ndst)
      , map_fn(map_fn) { }

    qvi_map_config(
        const std::vector<qvi_hwloc_bitmap> &src_affinities,
        const std::vector<qvi_hwloc_bitmap> &dst_affinities,
        qvi_map_fn_t map_fn = {}
    ) : src_affinities(src_affinities)
      , dst_affinities(dst_affinities)
      , map_fn(map_fn) { }

    qvi_map_config(
        const std::vector<int> &src_colors,
        const size_t &ndst,
        qvi_map_fn_t map_fn = {}
    ) : ndst(ndst)
      , src_colors(src_colors)
      , map_fn(map_fn) { }
};

/**
 * Prints map assignments.
 */
void
qvi_map_debug_dump(
    const std::string &name,
    const qvi_map_t &map
);

/**
 * Returns the number of source IDs that have already been mapped.
 */
size_t
qvi_map_nsrcids_mapped(
    const qvi_map_t &map
);

/**
 * Returns whether or not the provided source ID is already mapped.
 */
bool
qvi_map_srcid_mapped(
    const qvi_map_t &map,
    size_t srcid
);

/**
 * Maps From IDs (fids) from 0...nfids-1 to resource indices (resource IDs)
 * by associating contiguous consumer IDs with resource IDs.
 */
int
qvi_map_packed(
    const qvi_map_config &config,
    qvi_map_t &map
);

int
qvi_map_spread(
    const qvi_map_config &config,
    qvi_map_t &map
);

/**
 * Calculates a mapping between source IDs and the destination IDs with which
 * they have affinity.
 */
qvi_map_t
qvi_map_calc_affinities(
    const std::vector<qvi_hwloc_bitmap> &src,
    const std::vector<qvi_hwloc_bitmap> &dst
);

/**
 * Performs a mapping between the provided colors to the provided cpusets.
 */
int
qvi_map_colors(
    const qvi_map_config &config,
    qvi_map_t &map
);

/**
 * Performs an affinity preserving mapping.
 */
int
qvi_map_affinity_preserving(
    const qvi_map_config &config,
    qvi_map_t &map
);

std::vector<int>
qvi_map_flatten_to_colors(
    const qvi_map_t &map
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
