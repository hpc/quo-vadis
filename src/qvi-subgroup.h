/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022-2024 Inria
 *                         All rights reserved.
 *
 * Copyright (c) 2022-2024 Bordeaux INP
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-subgroup.h
 */

#ifndef QVI_SUBGROUP_H
#define QVI_SUBGROUP_H

#include "qvi-common.h" // IWYU pragma: keep

/**
 * Stores sub-group information for infrastructure that
 * doesn't have native support for creating sub-groups.
 */
struct qvi_subgroup_info_s {
    /** Number of sub-groups created from split. */
    int ngroups = 0;
    /** Number of members in this sub-group. */
    int size = 0;
    /** My rank in this sub-group. */
    int rank = 0;
};

/**
 * Provides supporting infrastructure for creating
 * sub-groups based on color, key, and rank.
 */
struct qvi_subgroup_color_key_rank_s {
    int color = 0;
    int key = 0;
    int rank = 0;

    static bool
    by_color(
        const qvi_subgroup_color_key_rank_s &a,
        const qvi_subgroup_color_key_rank_s &b
    ) {
        return a.color < b.color;
    }

    static bool
    by_key(
        const qvi_subgroup_color_key_rank_s &a,
        const qvi_subgroup_color_key_rank_s &b
    ) {
        // If colors are the same, sort by key.
        return a.color == b.color && a.key < b.key;
    }

    static bool
    by_rank(
        const qvi_subgroup_color_key_rank_s &a,
        const qvi_subgroup_color_key_rank_s &b
    ) {
        // If colors and keys are the same, sort by rank.
        return a.color == b.color && a.key == b.key && a.rank < b.rank;
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
