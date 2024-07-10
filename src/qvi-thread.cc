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

/**
 * We need to have one structure for fields shared by all threads included in
 * another with fields specific to each thread.
 */
struct qvi_thread_group_shared_s {
    /** Barrier object (used in scope). */
    pthread_barrier_t barrier;
    /** Constructor. */
    qvi_thread_group_shared_s(void) = delete;
    /** Constructor. */
    qvi_thread_group_shared_s(
        int group_size
    ) {
        const int rc = pthread_barrier_init(&barrier, NULL, group_size);
        if (rc != 0) throw qvi_runtime_error();
    }
    /** Destructor. */
    ~qvi_thread_group_shared_s(void)
    {
        pthread_barrier_destroy(&barrier);
    }
};

struct qvi_thread_group_s {
    /**
     * Shared data between threads in the group:
     * ALL threads point to the same region.
     */
    qvi_thread_group_shared_s *shdata = nullptr;
    /** Group size. */
    int size = 0;
    /** ID (rank) in group: this ID is unique to each thread. */
    int rank = 0;
    /** Performs object construction, called by the real constructors. */
    void
    construct(
        int group_size,
        int group_rank
    ) {
        qvi_thread_group_shared_s *ishdata = nullptr;
        #pragma omp single copyprivate(ishdata)
        {
            const int rc = qvi_new(&ishdata, group_size);
            if (rc != QV_SUCCESS) throw qvi_runtime_error();
        }
        shdata = ishdata;
        size = group_size;
        rank = group_rank;
    }
    /** Constructor. */
    qvi_thread_group_s(void)
    {
        construct(
            qvi_thread_omp_get_num_threads(),
            qvi_thread_omp_get_thread_num()
        );
    }
    /** Constructor. */
    qvi_thread_group_s(
        int group_size,
        int group_rank
    ) {
        construct(group_size, group_rank);
    }
    /** Destructor. */
    ~qvi_thread_group_s(void)
    {
        // TODO(skg) FIXME
        qvi_thread_group_barrier(this);
        if (rank == 0) {
            //qvi_log_debug("ADDR={}", (void *)shdata);
            //delete shdata;
        }
    }
};

int
qvi_thread_group_new(
    qvi_thread_group_t **group
) {
    return qvi_new(group);
}

void
qvi_thread_group_free(
    qvi_thread_group_t **group
) {
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
    qvi_thread_group_t *group
) {
    const int rc = pthread_barrier_wait(&(group->shdata->barrier));
    if ((rc != 0) && (rc != PTHREAD_BARRIER_SERIAL_THREAD)) {
        return QV_ERR_INTERNAL;
    }
    return QV_SUCCESS;
}

#ifdef OPENMP_FOUND
/**
 * Internal data structure for rank and size computing.
 */
struct color_key_rank_s {
    int color = 0;
    int key = 0;
    int rank = 0;
};

static bool
ckr_compare_by_color(
    const color_key_rank_s &a,
    const color_key_rank_s &b
) {
    return a.color < b.color;
}

static bool
ckr_compare_by_key(
    const color_key_rank_s &a,
    const color_key_rank_s &b
) {
    // If colors are the same, sort by key.
    return a.color == b.color && a.key < b.key;
}

static bool
ckr_compare_by_rank(
    const color_key_rank_s &a,
    const color_key_rank_s &b
) {
    // If colors and keys are the same, sort by rank.
    return a.color == b.color && a.key == b.key && a.rank < b.rank;
}

/**
 *
 */
/*
static void display_tab(
    pid_t tid,
    color_key_id_s lptr[],
    int size
) {
    fprintf(stdout,"=============================================================================\n");
    fprintf(stdout, "[%6i] COLORS = ",tid);
    for(int i = 0 ; i < size ; i++)
       fprintf(stdout, "[%i]:%6i | ", i,lptr[i].color);
    fprintf(stdout,"\n");
    fprintf(stdout, "[%6i] KEYS   = ",tid);
    for(int i = 0 ; i < size ; i++)
       fprintf(stdout, "[%i]:%6i | ", i,lptr[i].key);
    fprintf(stdout,"\n");
    fprintf(stdout, "[%6i] RANK   = ",tid);
    for(int i = 0 ; i < size ; i++)
       fprintf(stdout, "[%i]:%6i | ", i,lptr[i].rank);
    fprintf(stdout,"\n");
    fprintf(stdout,"\n");
    fprintf(stdout,"=============================================================================\n");
    fprintf(stdout,"\n\n");
}
*/
#endif

static int
qvi_get_subgroup_info(
    qvi_thread_group_t *parent,
    int color,
    int key,
    int *sgrp_size,
    int *sgrp_rank,
    int *num_of_sgrp
) {
    const int size = parent->size;
    const int rank = parent->rank;
    color_key_rank_s *ckrs = nullptr;

    #pragma omp single copyprivate(ckrs)
    ckrs = new color_key_rank_s[size];
    // Gather colors and keys from ALL threads.
    ckrs[rank].color = color;
    ckrs[rank].key = key;
    ckrs[rank].rank = rank;
    // Barrier to be sure that all threads have contributed their values. We use
    // the parent's barrier because we may be working with a strict subset of
    // the threads in this context.
    int rc = qvi_thread_group_barrier(parent);
    if (rc != QV_SUCCESS) return rc;
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
    *num_of_sgrp = ncolors;
    // Compute my sub-group size and sub-group rank.
    int group_rank = 0;
    int group_size = 0;
    for (int i = 0; i < size; ++i) {
        if (color != ckrs[i].color) continue;
        // Else we found the start of my color group.
        const int current_color = ckrs[i].color;
        for (int j = i; j < size && current_color == ckrs[j].color; ++j) {
            if (ckrs[j].rank == rank) {
                *sgrp_rank = group_rank;
            }
            group_size++;
            group_rank++;
        }
        *sgrp_size = group_size;
        break;
    }
    // Barrier to sync for array deletion.
    rc = qvi_thread_group_barrier(parent);
    if (rc != QV_SUCCESS) return rc;
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
)
#ifndef OPENMP_FOUND
{
    // TODO(skg) This should return self.
    *child = nullptr;
    return QV_SUCCESS;
}
#else
{
    int sgrp_size = -1; /* size of this thread subgroup */
    int sgrp_rank = -1; /* subgroup rank */
    int num_sgrp = -1;  /* number of subgroups */
    int rc = qvi_get_subgroup_info(
        parent,
        color,
        key,
        &sgrp_size,
        &sgrp_rank,
        &num_sgrp
    );
    if (rc != QV_SUCCESS) {
        *child = nullptr;
        return QV_ERR_SPLIT;
    }

    qvi_thread_group_t *ichild = nullptr;
    rc = qvi_new(&ichild, sgrp_size, sgrp_rank);
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
