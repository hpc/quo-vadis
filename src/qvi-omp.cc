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
 * @file qvi-omp.cc
 */

#include "qvi-omp.h"
#include "qvi-subgroup.h"
#include "qvi-bbuff.h"
#include "qvi-utils.h"
#include <omp.h>

struct qvi_omp_group_s {
    /** Group size. */
    int size = 0;
    /** ID (rank) in group: this ID is unique to each thread. */
    int rank = 0;
    /** Constructor. */
    qvi_omp_group_s(void) = delete;
    /** Constructor. */
    qvi_omp_group_s(
        int group_size,
        int group_rank
    ) : size(group_size)
      , rank(group_rank) { }
};

int
qvi_omp_group_new(
    int group_size,
    int group_rank,
    qvi_omp_group_t **group
) {
    return qvi_new(group, group_size, group_rank);
}

void
qvi_omp_group_free(
    qvi_omp_group_t **group
) {
    #pragma omp barrier
    qvi_delete(group);
}

int
qvi_omp_group_id(
    const qvi_omp_group_t *group
) {
    return group->rank;
}

int
qvi_omp_group_size(
    const qvi_omp_group_t *group
) {
    return group->size;
}

int
qvi_omp_group_barrier(
    qvi_omp_group_t *
) {
    return QV_SUCCESS;
}

static int
qvi_get_subgroup_info(
    qvi_omp_group_t *parent,
    int color,
    int key,
    qvi_subgroup_info_s *sginfo
) {
    const int size = parent->size;
    const int rank = parent->rank;
    qvi_subgroup_color_key_rank_s *ckrs = nullptr;

    #pragma omp single copyprivate(ckrs)
    ckrs = new qvi_subgroup_color_key_rank_s[size];
    // Gather colors and keys from ALL threads.
    ckrs[rank].color = color;
    ckrs[rank].key = key;
    ckrs[rank].rank = rank;
    // Barrier to be sure that all threads have contributed their values.
    #pragma omp barrier
    // Since these data are shared, only one thread has to sort them. The same
    // goes for calculating the number of distinct colors provided.
    int ncolors = 0;
    #pragma omp single copyprivate(ncolors)
    {
        // Sort the color/key/rank array. First according to color, then by key,
        // but in the same color realm. If color and key are identical, sort by
        // the rank from given group.
        std::sort(ckrs, ckrs + size, qvi_subgroup_color_key_rank_s::by_color);
        std::sort(ckrs, ckrs + size, qvi_subgroup_color_key_rank_s::by_key);
        std::sort(ckrs, ckrs + size, qvi_subgroup_color_key_rank_s::by_rank);
        // Calculate the number of distinct colors provided.
        std::set<int> color_set;
        for (int i = 0; i < size; ++i) {
            color_set.insert(ckrs[i].color);
        }
        ncolors = color_set.size();
    }
    // The number of distinct colors is the number of subgroups.
    sginfo->ngroups = ncolors;
    // Compute my sub-group size and sub-group rank.
    int group_rank = 0;
    int group_size = 0;
    for (int i = 0; i < size; ++i) {
        if (color != ckrs[i].color) continue;
        // Else we found the start of my color group.
        const int current_color = ckrs[i].color;
        for (int j = i; j < size && current_color == ckrs[j].color; ++j) {
            if (ckrs[j].rank == rank) {
                sginfo->rank = group_rank;
            }
            group_size++;
            group_rank++;
        }
        sginfo->size = group_size;
        break;
    }
    // Barrier to sync for array deletion.
    #pragma omp barrier
    // Only one task deletes.
    #pragma omp single
    delete[] ckrs;
    return QV_SUCCESS;
}

int
qvi_omp_group_create_from_split(
    qvi_omp_group_t *parent,
    int color,
    int key,
    qvi_omp_group_t **child
) {
    qvi_omp_group_t *ichild = nullptr;

    qvi_subgroup_info_s sginfo;
    int rc = qvi_get_subgroup_info(
        parent, color, key, &sginfo
    );
    if (qvi_likely(rc == QV_SUCCESS)) {
        rc = qvi_new(&ichild, sginfo.size, sginfo.rank);
    }
    if (rc != QV_SUCCESS) {
        qvi_delete(&ichild);
    }
    *child = ichild;
    return rc;
}

int
qvi_omp_group_gather_bbuffs(
    qvi_omp_group_t *group,
    qvi_bbuff_t *txbuff,
    int,
    bool *shared_alloc,
    qvi_bbuff_t ***rxbuffs
) {
    const int group_size = group->size;
    const int group_rank = group->rank;

    qvi_bbuff_t **bbuffs = nullptr;
    #pragma omp single copyprivate(bbuffs)
    bbuffs = new qvi_bbuff_t *[group_size]();

    const int rc = qvi_bbuff_dup(txbuff, &bbuffs[group_rank]);
    // Need to ensure that all threads have contributed to bbuffs.
    #pragma omp barrier
    if (rc != QV_SUCCESS) {
        #pragma omp single
        if (bbuffs) {
            for (int i = 0; i < group_size; ++i) {
                qvi_bbuff_free(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
        bbuffs = nullptr;
    }
    *rxbuffs = bbuffs;
    *shared_alloc = true;
    return rc;
}

int
qvi_omp_group_scatter_bbuffs(
    qvi_omp_group_t *group,
    qvi_bbuff_t **txbuffs,
    int,
    qvi_bbuff_t **rxbuff
) {
    qvi_bbuff_t ***tmp = nullptr;
    #pragma omp single copyprivate(tmp)
    tmp = new qvi_bbuff_t**();
    #pragma omp master
    *tmp = txbuffs;
    #pragma omp barrier
    qvi_bbuff_t *inbuff = (*tmp)[group->rank];
    qvi_bbuff_t *mybbuff = nullptr;
    const int rc = qvi_bbuff_dup(inbuff, &mybbuff);
    #pragma omp barrier
    #pragma omp single
    delete tmp;
    if (rc != QV_SUCCESS) {
        qvi_bbuff_free(&mybbuff);
    }
    *rxbuff = mybbuff;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
