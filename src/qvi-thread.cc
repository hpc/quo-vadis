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
 * @file qvi-thread.cc
 */

#include "qvi-thread.h"
#include "qvi-bbuff.h"
#include "qvi-utils.h"
#ifdef OPENMP_FOUND
#include <omp.h>
#endif

static int
qvi_thread_omp_get_num_threads(void)
{
#ifdef OPENMP_FOUND
    return omp_get_num_threads();
#else
    return 1;
#endif
}

static int
qvi_thread_omp_get_thread_num(void)
{
#ifdef OPENMP_FOUND
    return omp_get_thread_num();
#else
    return 0;
#endif
}

struct qvi_thread_group_s {
    /** Group size. */
    int size = 0;
    /** ID (rank) in group: this ID is unique to each thread. */
    int rank = 0;
    /** Constructor. */
    qvi_thread_group_s(void) = delete;
    /** Constructor. */
    qvi_thread_group_s(
        int group_size,
        int group_rank
    ) : size(group_size)
      , rank(group_rank) { }
};

void
qvi_thread_group_free(
    qvi_thread_group_t **group
) {
    #pragma omp barrier
    qvi_delete(group);
}

static int
qvi_thread_group_create_size(
    qvi_thread_group_t **group,
    int size
) {
    return qvi_new(group, size, qvi_thread_omp_get_thread_num());
}

int
qvi_thread_group_create(
    qvi_thread_group_t **group
) {
    return qvi_thread_group_create_size(
        group, qvi_thread_omp_get_num_threads()
    );
}

int
qvi_thread_group_create_single(
    qvi_thread_group_t **group
) {
    return qvi_thread_group_create_size(group, 1);
}

int
qvi_thread_group_id(
    const qvi_thread_group_t *group
) {
    return group->rank;
}

int
qvi_thread_group_size(
    const qvi_thread_group_t *group
) {
    return group->size;
}

int
qvi_thread_group_barrier(
    qvi_thread_group_t *
) {
    return QV_SUCCESS;
}

#ifdef OPENMP_FOUND
/**
 * Internal data structure for rank and size computing.
 */
struct qvi_thread_color_key_rank_s {
    int color = 0;
    int key = 0;
    int rank = 0;
};

/**
 *
 */
struct qvi_thread_subgroup_info_s {
    /** Number of sub-groups created from split. */
    int num_sgrp = 0;
    /** Number of members in this sub-group. */
    int sgrp_size = 0;
    /** My rank in this sub-group. */
    int sgrp_rank = 0;
};

static bool
ckr_compare_by_color(
    const qvi_thread_color_key_rank_s &a,
    const qvi_thread_color_key_rank_s &b
) {
    return a.color < b.color;
}

static bool
ckr_compare_by_key(
    const qvi_thread_color_key_rank_s &a,
    const qvi_thread_color_key_rank_s &b
) {
    // If colors are the same, sort by key.
    return a.color == b.color && a.key < b.key;
}

static bool
ckr_compare_by_rank(
    const qvi_thread_color_key_rank_s &a,
    const qvi_thread_color_key_rank_s &b
) {
    // If colors and keys are the same, sort by rank.
    return a.color == b.color && a.key == b.key && a.rank < b.rank;
}

#endif

static int
qvi_get_subgroup_info(
    qvi_thread_group_t *parent,
    int color,
    int key,
    qvi_thread_subgroup_info_s *sginfo
) {
    const int size = parent->size;
    const int rank = parent->rank;
    qvi_thread_color_key_rank_s *ckrs = nullptr;

    #pragma omp single copyprivate(ckrs)
    ckrs = new qvi_thread_color_key_rank_s[size];
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
        std::sort(ckrs, ckrs + size, ckr_compare_by_color);
        std::sort(ckrs, ckrs + size, ckr_compare_by_key);
        std::sort(ckrs, ckrs + size, ckr_compare_by_rank);
        // Calculate the number of distinct colors provided.
        std::set<int> color_set;
        for (int i = 0; i < size; ++i) {
            color_set.insert(ckrs[i].color);
        }
        ncolors = color_set.size();
    }
    // The number of distinct colors is the number of subgroups.
    sginfo->num_sgrp = ncolors;
    // Compute my sub-group size and sub-group rank.
    int group_rank = 0;
    int group_size = 0;
    for (int i = 0; i < size; ++i) {
        if (color != ckrs[i].color) continue;
        // Else we found the start of my color group.
        const int current_color = ckrs[i].color;
        for (int j = i; j < size && current_color == ckrs[j].color; ++j) {
            if (ckrs[j].rank == rank) {
                sginfo->sgrp_rank = group_rank;
            }
            group_size++;
            group_rank++;
        }
        sginfo->sgrp_size = group_size;
        break;
    }
    // Barrier to sync for array deletion.
    #pragma omp barrier
    // Only one task deletes.
    if (parent->rank == 0) {
        delete[] ckrs;
    }
    return QV_SUCCESS;
}

int
qvi_thread_group_create_from_split(
    qvi_thread_group_t *parent,
    int color,
    int key,
    qvi_thread_group_t **child
) {
#ifndef OPENMP_FOUND
    // TODO(skg) This should return self.
    *child = nullptr;
    return QV_SUCCESS;
#else
    qvi_thread_subgroup_info_s sginfo;
    int rc = qvi_get_subgroup_info(
        parent, color, key, &sginfo
    );
    if (rc != QV_SUCCESS) {
        *child = nullptr;
        return QV_ERR_SPLIT;
    }

    qvi_thread_group_t *ichild = nullptr;
    rc = qvi_new(&ichild, sginfo.sgrp_size, sginfo.sgrp_rank);
    if (rc != QV_SUCCESS) {
        *child = nullptr;
        return rc;
    }
    *child = ichild;

    return rc;
}
#endif

int
qvi_thread_group_gather_bbuffs(
    qvi_thread_group_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs,
    int *shared_alloc
) {
    QVI_UNUSED(root);
    const int send_count = (int)qvi_bbuff_size(txbuff);
    const int group_size = group->size;
    const int group_id   = group->rank;
    int rc = QV_SUCCESS;
    qvi_bbuff_t **bbuffs = nullptr;
    // Zero initialize array of pointers to nullptr.
#pragma omp single copyprivate(bbuffs) //shared bbuffs allocation
    bbuffs = new qvi_bbuff_t *[group_size]();

    rc = qvi_bbuff_new(&bbuffs[group_id]);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_bbuff_append(bbuffs[group_id], qvi_bbuff_data(txbuff), send_count);
    if (rc != QV_SUCCESS) goto out;

#pragma omp barrier // Need to ensure that all threads have contributed to bbuffs

out:
    if (rc != QV_SUCCESS) {
#pragma omp single
      {
        if (bbuffs) {
            for (int i = 0; i < group_size; ++i) {
                qvi_bbuff_free(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
      }
      bbuffs = nullptr;
    }
    *rxbuffs = bbuffs;
    *shared_alloc = 1;
    //for(int i = 0 ; i < group_size ; i++)
    //fprintf(stdout,"===== [%i] bbuff[%i] à %p\n",group_id,i,bbuffs[i]);
    return rc;
}

int
qvi_thread_group_scatter_bbuffs(
    qvi_thread_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
) {
    QVI_UNUSED(root);
    const int group_id = group->rank;
    qvi_bbuff_t  ***tmp = nullptr;

    /* GM: Oh man, that is UGLY */
    /* Surely, some nice OpenMP magic will fix this mess */
#pragma omp single copyprivate(tmp)
    tmp = new qvi_bbuff_t**();
#pragma omp master
    *tmp = txbuffs;
#pragma omp barrier

    //fprintf(stdout,">>>>>>>>>>>>>>>>>>>>>>> SCATTER [%i] tmp @ %p txbuffs @ %p\n",group_id,*tmp,(*tmp)[group_id]);

    qvi_bbuff_t *inbuff = (*tmp)[group_id];
    size_t data_size = qvi_bbuff_size(inbuff);
    void *data = qvi_bbuff_data(inbuff);

    qvi_bbuff_t *mybbuff = nullptr;
    int rc = qvi_bbuff_new(&mybbuff);
    if (rc == QV_SUCCESS) {
      rc = qvi_bbuff_append(mybbuff, data, data_size);
    }
#pragma omp barrier
#pragma omp single
    delete tmp;

    if (rc != QV_SUCCESS) {
        qvi_bbuff_free(&mybbuff);
    }
    *rxbuff = mybbuff;
    //fprintf(stdout,">>>>>>>>>>>>>>>>>>>>>>> SCATTER [%i] OUT with RC %i\n",group_id,rc);
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
