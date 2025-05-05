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
 * @file qvi-omp.cc
 */

#include "qvi-omp.h"
#include "qvi-bbuff.h"
#include "qvi-utils.h"
#include <omp.h>

qvi_omp_group::qvi_omp_group(
    int group_size,
    int group_rank
) : m_size(group_size)
  , m_rank(group_rank) { }

int
qvi_omp_group::create(
    int group_size,
    int group_rank,
    qvi_omp_group **group
) {
    return qvi_new(group, group_size, group_rank);
}

void
qvi_omp_group::destroy(
    qvi_omp_group **group
) {
    #pragma omp barrier
    qvi_delete(group);
}

int
qvi_omp_group::size(void)
{
    return m_size;
}

int
qvi_omp_group::rank(void)
{
    return m_rank;
}

int
qvi_omp_group::barrier(void)
{
    // TODO(skg) What should we do about barriers here? In particular, we need
    // to be careful about sub-groups, etc.
    if (0 == m_rank) {
        omp_set_num_threads(m_size);
    }
    #pragma omp barrier

    const int level = omp_get_level();
    assert(level > 0);
    const int num = omp_get_ancestor_thread_num(level - 1);
    omp_set_num_threads(num);
    return QV_SUCCESS;
}

int
qvi_omp_group::m_subgroup_info(
    int color,
    int key,
    qvi_subgroup_info *sginfo
) {
    qvi_subgroup_color_key_rank *ckrs = nullptr;

    #pragma omp single copyprivate(ckrs)
    ckrs = new qvi_subgroup_color_key_rank[m_size];
    // Gather colors and keys from ALL threads.
    ckrs[m_rank].color = color;
    ckrs[m_rank].key = key;
    ckrs[m_rank].rank = m_rank;
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
        std::sort(
            ckrs, ckrs + m_size,
            qvi_subgroup_color_key_rank::by_color_key_rank
        );
        // Calculate the number of distinct colors provided.
        std::set<int> color_set;
        for (int i = 0; i < m_size; ++i) {
            color_set.insert(ckrs[i].color);
        }
        ncolors = color_set.size();
    }
    // The number of distinct colors is the number of subgroups.
    sginfo->ngroups = ncolors;
    // Compute my sub-group size and sub-group rank.
    int group_rank = 0;
    int group_size = 0;
    for (int i = 0; i < m_size; ++i) {
        if (color != ckrs[i].color) continue;
        // Else we found the start of my color group.
        const int current_color = ckrs[i].color;
        for (int j = i; j < m_size && current_color == ckrs[j].color; ++j) {
            if (ckrs[j].rank == m_rank) {
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
qvi_omp_group::split(
    int color,
    int key,
    qvi_omp_group **child
) {
    qvi_omp_group *ichild = nullptr;

    qvi_subgroup_info sginfo;
    int rc = m_subgroup_info(color, key, &sginfo);
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
qvi_omp_group::gather(
    qvi_bbuff *txbuff,
    int,
    qvi_bbuff_alloc_type_t *alloc_type,
    qvi_bbuff ***rxbuffs
) {
    qvi_bbuff **bbuffs = nullptr;
    #pragma omp single copyprivate(bbuffs)
    bbuffs = new qvi_bbuff *[m_size]();

    const int rc = qvi_dup(*txbuff, &bbuffs[m_rank]);
    // Need to ensure that all threads have contributed to bbuffs.
    #pragma omp barrier
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        #pragma omp single
        if (bbuffs) {
            for (int i = 0; i < m_size; ++i) {
                qvi_delete(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
        bbuffs = nullptr;
    }
    *rxbuffs = bbuffs;
    *alloc_type = QVI_BBUFF_ALLOC_SHARED;
    return rc;
}

int
qvi_omp_group::scatter(
    qvi_bbuff **txbuffs,
    int, // rootid,
    qvi_bbuff **rxbuff
) {
    qvi_bbuff ***tmp = nullptr;
    #pragma omp single copyprivate(tmp)
    tmp = new qvi_bbuff**();
    #pragma omp master
    //#pragma omp masked filter(rootid)
    *tmp = txbuffs;
    #pragma omp barrier
    qvi_bbuff *inbuff = (*tmp)[m_rank];
    qvi_bbuff *mybbuff = nullptr;
    const int rc = qvi_dup(*inbuff, &mybbuff);
    #pragma omp barrier
    #pragma omp single
    delete tmp;
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        qvi_delete(&mybbuff);
    }
    *rxbuff = mybbuff;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
