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

#include "qvi-utils.h"
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
    qvi_map_t(
        const qvi_map_config &config
    )
>;

struct qvi_map_config {
    bool be_verbose;
    size_t nsrc;
    size_t ndst;
    std::vector<qvi_hwloc_bitmap> src_affinities;
    std::vector<qvi_hwloc_bitmap> dst_affinities;
    std::vector<int> src_colors;
    qvi_map_fn_t map_fn;

    qvi_map_config(void)
        : be_verbose(qvi_envset(QVI_ENV_VMAP)) { }

    qvi_map_config(
        size_t nsrc,
        size_t ndst,
        qvi_map_fn_t map_fn = {}
    ) : be_verbose(qvi_envset(QVI_ENV_VMAP))
      , nsrc(nsrc)
      , ndst(ndst)
      , map_fn(map_fn) { }

    qvi_map_config(
        const std::vector<qvi_hwloc_bitmap> &src_affinities,
        const std::vector<qvi_hwloc_bitmap> &dst_affinities,
        qvi_map_fn_t map_fn = {}
    ) : be_verbose(qvi_envset(QVI_ENV_VMAP))
      , src_affinities(src_affinities)
      , dst_affinities(dst_affinities)
      , map_fn(map_fn) { }

    qvi_map_config(
        const std::vector<int> &src_colors,
        qvi_map_fn_t map_fn = {}
    ) : be_verbose(qvi_envset(QVI_ENV_VMAP))
      , src_colors(src_colors)
      , map_fn(map_fn) { }
};

/**
 * Takes the input map and returns a new map where sources are mapped to a
 * unique destination.
 */
qvi_map_t
qvi_map_uniq(
    const qvi_map_t &map
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
qvi_map_t
qvi_map_colors(
    const qvi_map_config &config
);

/**
 * Maps source IDs (0...config.nsrc-1) to destination indices
 * (0...config.ndst-1) by packing source IDs to destination IDs.
 */
qvi_map_t
qvi_map_packed(
    const qvi_map_config &config
);

/**
 * Maps sources to destinations round-robin.
 */
qvi_map_t
qvi_map_spread(
    const qvi_map_config &config
);

/**
 * Performs a close (affinity preserving) mapping.
 */
qvi_map_t
qvi_map_close(
    const qvi_map_config &config
);

/**
 * Performs an affinity-aware packed (afpacked) mapping of sources (source
 * affinities) to destinations (destination affinities). Each source is mapped
 * to exactly one destination.
 *
 * Sources are packed onto destinations in source order, each destination
 * holding up to ceil(nsrc / ndst) sources. For each source, the first
 * (lowest-index) under-capacity destination that fully contains the source's
 * affinity is preferred; otherwise the first under-capacity destination that
 * shares any affinity is used. If all of a source's affinity destinations are
 * already at capacity, the source overflows into the first destination it has
 * affinity to (this soft capacity keeps every source mapped). Every source is
 * expected to have affinity to at least one destination.
 */
qvi_map_t
qvi_map_afpacked(
    const qvi_map_config &config
);

/**
 * Takes a map and assigns its values as keys and keys as values.
 */
qvi_map_t
qvi_map_invert(
    const qvi_map_t &original
);

/**
 * Prints map assignments.
 */
void
qvi_map_emit(
    const std::string &name,
    const qvi_map_t &map
);

/**
 * Takes a vector of colors and clamps their values to [0, ndc), where ndc is
 * the number of distinct non-negative colors found in values. QV_SPLIT_UNDEFINED
 * entries opt out of the split: they are excluded from the distinct-color count
 * and passed through unchanged rather than folded into a real color.
 */
std::vector<int>
qvi_map_clamp_colors(
    const std::vector<int> &colors
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
