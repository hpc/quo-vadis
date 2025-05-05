/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2022-2025 Triad National Security, LLC
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
struct qvi_subgroup_info {
    /** The rank of the master task. */
    static constexpr int master_rank = 0;
    /** Number of sub-groups created from split. */
    int ngroups = 0;
    /** My sub-group index (from 0 to ngroups - 1). */
    int index = 0;
    /** Number of members in my sub-group. */
    int size = 0;
    /** My rank in my sub-group. */
    int rank = 0;
};

/**
 * Provides supporting infrastructure for creating
 * sub-groups based on color, key, and rank.
 */
struct qvi_subgroup_color_key_rank {
    int color = 0;
    int key = 0;
    int rank = 0;
    int ncolors = 0;

    static bool
    by_color_key_rank(
        const qvi_subgroup_color_key_rank &a,
        const qvi_subgroup_color_key_rank &b
    ) {
        return a.color < b.color ||
            (a.color == b.color && a.key < b.key) ||
            (a.color == b.color && a.key == b.key && a.rank < b.rank);
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
