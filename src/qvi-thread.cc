/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Inria.
 *                         All rights reserved.
 *
 * Copyright (c) 2022      Bordeaux INP.
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-thread.cc
 */

#include "qvi-common.h" // IWYU pragma: keep

#include "qvi-thread.h"
#include "qvi-group.h"
#include "qvi-utils.h"

#ifdef OPENMP_FOUND
#include <omp.h>
#endif

// Type definitions.
typedef qvi_group_id_t qvi_thread_group_id_t;

using qvi_thread_group_tab_t = std::unordered_map<
    qvi_thread_group_id_t, qvi_thread_group_t
>;


// We need to have one structure for fields
// shared by all threads included in another
// with fields specific to each thread
struct qvi_thread_group_shared_s {
    /** for resource freeing */
    int in_use = 0;
    /** UNUSED : ID (rank) in group */
    /* probably need a LIST of IDs ...*/
    int id = -1;
    /** ID used for table lookups */
    qvi_thread_group_id_t tabid = 0;
    /** Size of group */
     int size = 0;
    /** Barrier object (used in scope) */
    pthread_barrier_t barrier;
};

struct qvi_thread_group_s {
    /** ID (rank) in group */
    /** This ID is unique to each thread */
    int id = 0;
    /** shared data between threads in the group*/
    /** ALL threads point to the same region */
    qvi_thread_group_shared_t *sdata = nullptr;
};

struct qvi_thread_s {
    /** Task associated with this thread */
    qvi_task_t *task = nullptr;
    /** Group table (ID to internal structure mapping) */
    qvi_thread_group_tab_t *group_tab = nullptr;
    /** Barrier object (used in context) */
    pthread_barrier_t *barrier;
};

/**
 * Copies contents of internal structure from src to dst.
 */
static void
cp_thread_group(
    const qvi_thread_group_t *src,
    qvi_thread_group_t *dst
) {
    memmove(dst, src, sizeof(*src));
}

/**
 *
 */
static int
next_group_tab_id(
    qvi_thread_t *th,
    qvi_thread_group_id_t *gid
) {
    QVI_UNUSED(th);
    return qvi_group_next_id(gid);
}

/**
 *
 */
int
qvi_thread_new(
    qvi_thread_t **th
) {
    int rc = QV_SUCCESS;

    qvi_thread_t *ith = qvi_new qvi_thread_t();
    if (!ith) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Task
    rc = qvi_task_new(&ith->task);
    if (rc != QV_SUCCESS) goto out;
    // Groups
    ith->group_tab = qvi_new qvi_thread_group_tab_t();
    if (!ith->group_tab) {
        rc = QV_ERR_OOR;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_thread_free(&ith);
    }
    *th = ith;
    return rc;
}

/**
 *
 */
void
qvi_thread_free(
    qvi_thread_t **th
) {
    if (!th) return;
    qvi_thread_t *ith = *th;

    if (!ith) goto out;

    if (ith->group_tab) {
        delete ith->group_tab;
        ith->group_tab = nullptr;
    }
      // both pragma should be OK since finalize is called from zgroups
#pragma omp barrier // ensure that no thread need this anymore
#pragma omp single
    {
      pthread_barrier_destroy(ith->barrier);
      delete ith->barrier;
    }

    qvi_task_free(&ith->task);
    delete ith;
out:
    *th = nullptr;
}

/**
 *
 */
int
qvi_thread_init(
    qvi_thread_t *th
) {
    // For now these are always fixed.
    const int world_id = 0, node_id = 0;
    pthread_barrier_t *barrier = nullptr;

#pragma omp single copyprivate(barrier)
    barrier = qvi_new pthread_barrier_t();

    th->barrier = barrier;

#ifdef OPENMP_FOUND
    /* GM: since all spawned threads are in the zgroup */
    /* using omp_get_num_threads is OK */
#pragma omp single
     pthread_barrier_init(th->barrier,NULL,omp_get_num_threads());
#else
     pthread_barrier_init(th->barrier,NULL, 1); /* FIXME, thread number not right*/
#endif

    return qvi_task_init(
        th->task, QV_TASK_TYPE_THREAD, qvi_gettid(), world_id, node_id
    );
}

/**
 *
 */
int
qvi_thread_finalize(
    qvi_thread_t *th
) {
    QVI_UNUSED(th);
    return QV_SUCCESS;
}

/**
 *
 */
int
qvi_thread_node_barrier(
    qvi_thread_t *th
) {
    int rc = pthread_barrier_wait(th->barrier);
    if ((rc != 0) && (rc != PTHREAD_BARRIER_SERIAL_THREAD)) {
      rc = QV_ERR_INTERNAL;
    } else rc = QV_SUCCESS;
    return rc;
}

/**
 *
 */
qvi_task_t *
qvi_thread_task_get(
    qvi_thread_t *th
) {
    return th->task;
}

/**
 *
 */
int
qvi_thread_group_new(
    qvi_thread_group_t **thgrp
) {
    qvi_thread_group_t *ithgrp = qvi_new qvi_thread_group_t();
    qvi_thread_group_shared_t *sdata = nullptr;

    if (!ithgrp) {
	*thgrp = nullptr;
        return QV_ERR_OOR;
    }

#pragma omp single copyprivate(sdata)
    sdata = qvi_new qvi_thread_group_shared_t();
    if (!sdata) {
        qvi_thread_group_free(&ithgrp);
	*thgrp = nullptr;
        return QV_ERR_OOR;;
    }

#pragma omp single
    sdata->in_use = omp_get_num_threads();

    ithgrp->sdata = sdata;

    *thgrp = ithgrp;
    return QV_SUCCESS;
}

/**
 *
 */
void
qvi_thread_group_free(
    qvi_thread_group_t **thgrp
) {
    qvi_thread_group_t *ithgrp = *thgrp;
    if (!ithgrp) goto out;

    if ( --(ithgrp->sdata->in_use) == 0) {
      pthread_barrier_destroy(&(ithgrp->sdata->barrier));
      delete ithgrp->sdata;
    }

    delete ithgrp;
out:
    *thgrp = nullptr;
}

/**
 *
 */
static int
qvi_thread_group_create_size(
    qvi_thread_t *th,
    qvi_thread_group_t **group,
    int size
) {
    qvi_thread_group_t *igroup = nullptr;
    qvi_thread_group_id_t gtid;
    int rc, irc;

    rc = qvi_thread_group_new(&igroup);
    if (rc != QV_SUCCESS) {
      *group = nullptr;
      return rc;
    }

#pragma omp single copyprivate(gtid, rc)
    rc = next_group_tab_id(th, &gtid);
    if (rc != QV_SUCCESS) goto out;

#pragma omp single
    {
      igroup->sdata->tabid = gtid;
      igroup->sdata->size = size;
    }
#ifdef OPENMP_FOUND
    igroup->id = omp_get_thread_num();
#else
    igroup->id = 0;
#endif

#pragma omp single copyprivate(irc)
    irc = pthread_barrier_init(&(igroup->sdata->barrier),NULL,igroup->sdata->size);
    if (irc != 0) {
      rc = QV_ERR_INTERNAL;
      goto out;
    }

    //Insert the whole group structure or just the shared part??
#pragma omp single
    th->group_tab->insert({gtid, *igroup});

out:
    if (rc != QV_SUCCESS) {
      qvi_thread_group_free(&igroup);
    }

    *group = igroup;
    return QV_SUCCESS;
}

/**
 *
 */
int
qvi_thread_group_create(
    qvi_thread_t *th,
    qvi_thread_group_t **group
) {
#ifdef OPENMP_FOUND
    return qvi_thread_group_create_size(th, group, omp_get_num_threads());
#else
    return qvi_thread_group_create_size(th, group, 1); /* Size to fix here */
#endif
}

/**
 *
 */
int
qvi_thread_group_create_single(
    qvi_thread_t *th,
    qvi_thread_group_t **group
) {
    return qvi_thread_group_create_size(th, group, 1);
}

/**
 *
 */
int
qvi_thread_group_lookup_by_id(
    qvi_thread_t *th,
    qvi_thread_group_id_t id,
    qvi_thread_group_t *group
) {
    auto got = th->group_tab->find(id);
    if (got == th->group_tab->end()) {
        return QV_ERR_NOT_FOUND;
    }
    cp_thread_group(&got->second, group);
    return QV_SUCCESS;
}

/**
 *
 */
int
qvi_thread_group_id(
    const qvi_thread_group_t *group
) {
    return group->id;
}

/**
 *
 */
int
qvi_thread_group_size(
    const qvi_thread_group_t *group
) {
    return group->sdata->size;
}

/**
 *
 */
int
qvi_thread_group_barrier(
    qvi_thread_group_t *group
) {
    int rc = pthread_barrier_wait(&(group->sdata->barrier));
    if ((rc != 0) && (rc != PTHREAD_BARRIER_SERIAL_THREAD)) {
      rc = QV_ERR_INTERNAL;
    } else rc = QV_SUCCESS;
    return rc;
}

#ifdef OPENMP_FOUND
/**
 * Internal data structure for rank and size computing
 */
typedef struct color_key_id_s {
  int   color;
  int   key;
  int   rank;
} color_key_id_t;

/**
 *
 */
static void swap_elts(
    color_key_id_t *el1,
    color_key_id_t *el2
) {
    color_key_id_t tmp = *el1;
    *el1 = *el2;
    *el2 = tmp;
}

/**
 *
 */
static void bubble_sort_by_color(
     color_key_id_t tab[],
     int size_tab
) {
     if (size_tab > 1) {
       for(int i = 0 ; i < size_tab - 1 ; i++)
	 if (tab[i].color > tab[i+1].color)
	   swap_elts(&tab[i],&tab[i+1]);
       bubble_sort_by_color(tab,size_tab-1);
     }
}

/**
 *
 */
static void bubble_sort_by_key(
    color_key_id_t tab[],
    int size_tab
) {
    if (size_tab > 1) {
      for(int i = 0 ; i < size_tab - 1 ; i++)
        if ((tab[i].color == tab[i+1].color) &&
            (tab[i].key > tab[i+1].key))
	  swap_elts(&tab[i],&tab[i+1]);
      bubble_sort_by_key(tab,size_tab-1);
    }
}

/**
 *
 */
static void bubble_sort_by_rank(
    color_key_id_t tab[],
    int size_tab
) {
    if (size_tab > 1) {
      for(int i = 0 ; i < size_tab - 1 ; i++)
        if ((tab[i].color == tab[i+1].color) &&
            (tab[i].key == tab[i+1].key)     &&
            (tab[i].rank >  tab[i+1].rank))
          swap_elts(&tab[i],&tab[i+1]);
      bubble_sort_by_key(tab,size_tab-1);
    }
}

/**
 *
 */
/*
static void display_tab(
    pid_t tid,
    color_key_id_t lptr[],
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

/**
 *
 */
static int qvi_get_subgroup_info(
    const qvi_thread_group_t *parent,
    int color,
    int key,
    int *new_id,
    int *sgrp_size,
    int *sgrp_rank,
    int *num_of_sgrp
 ){
    int color_val= 0;
    int num_colors = 0;
    int idx = 0;
    int idx2 = 0;
    int size = parent->sdata->size;
    int id = parent->id;
    color_key_id_t *lptr = NULL;

#pragma omp single copyprivate(lptr)
    lptr = qvi_new color_key_id_t[size]();
    if (!lptr) {
      *new_id = *sgrp_size = *sgrp_rank = *num_of_sgrp = -1;
      return QV_ERR_OOR;
    }

    /* Gather colors and keys from ALL threads */
    lptr[id].color = color;
    lptr[id].key   = key;
    lptr[id].rank  = id;
#pragma omp barrier // to be sure that all threads have contributed

    /* Sort the color/key/rank  array, according to color first, */
    /* then to key, but in the same color realm  */
    /* if color and key are identical, sort by rank in old group */
#pragma omp single  /* since this data is shared, only one thread has to sort it */
    {
      bubble_sort_by_color(lptr, size);
      bubble_sort_by_key(lptr, size);
      bubble_sort_by_rank(lptr, size);
    }

    /* compute number of subgroups */
    num_colors = 1;
    color_val = lptr[0].color;
    for(int idx = 0 ; idx < size ; idx++) {
      if(lptr[idx].color != color_val) {
          num_colors++;
          color_val = lptr[idx].color;
      }
    }
    *num_of_sgrp = num_colors;

    /* compute number and ranks of subgroups */
    num_colors = 0;
    color_val = lptr[0].color;
    for(int idx = 0 ; idx < size ; idx++) {
      if(lptr[idx].color != color_val) {
	num_colors++;
	color_val = lptr[idx].color;
      }
      if(lptr[idx].color == color) {
	*sgrp_rank = num_colors;
	break;
      }
    }

    /* Compute subgroup size and thread id in subgroup */
    /* 1- Move to the right part in the array */
    idx = 0;
    while((idx < size) && (lptr[idx].color != color))
      idx++;

    /* 2- Compute the subgroup size */
    idx2 = idx;
    while((idx2 < size) && (lptr[idx2].color == color))
      idx2++;
    *sgrp_size = (idx2 - idx);

    /* 3- Compute id in the subgroup */
    idx2 = idx;
    while((idx2 < size) && (lptr[idx2].rank != id))
      idx2++;
    *new_id = (idx2 - idx);

#pragma omp barrier // to prevent the quickest thread to remove data before all others have used it
#pragma omp single
    delete [] lptr;
    return QV_SUCCESS;
}


/**
 *
 */
int
qvi_thread_group_create_from_split(
    qvi_thread_t *th,
    const qvi_thread_group_t *parent,
    int color,
    int key,
    qvi_thread_group_t **child
)
#ifndef OPENMP_FOUND
{
    *child = nullptr;
    return QV_SUCCESS;
}
#else
{
    int rc = QV_SUCCESS;
    int newid = -1;     /* thread new id in subgroup */
    int sgrp_size = -1; /* size of this thread subgroup */
    int sgrp_rank = -1; /* subgroup rank */
    int num_sgrp = -1;  /* number of subgroups */
    qvi_thread_group_t *new_group = nullptr;
    qvi_thread_group_id_t gtid;

    /* this used to internally allocate things */
    /* in a copyprivate fashion */
    //qvi_thread_group_t **new_group_ptr_array = nullptr;
    //qvi_thread_group_t *tmp_group = nullptr;
    qvi_thread_group_shared_t **sdata_ptr_array = nullptr;
    qvi_thread_group_shared_t *tmp_sdata = nullptr;
    omp_lock_t *lock_ptr = nullptr; // use pthread_mutex instead ??*/

    if (!parent) {
      *child = nullptr;
      return QV_ERR_SPLIT;
    }

    rc = qvi_get_subgroup_info(
	 parent,
	 color,
	 key,
	 &newid,
	 &sgrp_size,
	 &sgrp_rank,
	 &num_sgrp );
    if (rc != QV_SUCCESS) {
      *child = nullptr;
      return QV_ERR_SPLIT;
    }

    /* create and init each subgroup : qvi_thread_group_new canNOT be used */
    /* since it already contains a "single" clause. */
    /* Calling the function directly will deadlock. */
    /* The same goes for qvi_thread_group_create. */
    /* Do everything manually */

    /* Equivalent to (sub)group "new" (cf qvi_thread_group_new) */
    new_group = qvi_new qvi_thread_group_t();
    if (!new_group) {
      	*child = nullptr;
    	return QV_ERR_OOR;
    }

    /* sdata pointer allocation */
#pragma omp single copyprivate(sdata_ptr_array)
    sdata_ptr_array = qvi_new qvi_thread_group_shared_t *[num_sgrp]();
    if (!sdata_ptr_array) {
      delete new_group;
      *child = nullptr;
      return QV_ERR_OOR;
    }

    for(int i = 0 ; i < num_sgrp ; i++) {
#pragma omp single copyprivate(tmp_sdata)
      tmp_sdata = qvi_new qvi_thread_group_shared_t();
      if (!tmp_sdata) {
	delete new_group;
#pragma omp single
	delete [] sdata_ptr_array;
	*child = nullptr;
	return QV_ERR_OOR;
      }
      /* Since it's shared, only the root(s) need(s) to set this */
      if ((newid == 0) && (i == sgrp_rank))
	sdata_ptr_array[i] = tmp_sdata;
    }

#pragma omp single copyprivate(lock_ptr)
    {
      lock_ptr = qvi_new omp_lock_t();
      omp_init_lock(lock_ptr);
    }

    /* Only the root(s) need(s) to initialize this */
    if (newid == 0) {
      omp_set_lock(lock_ptr);
      rc = next_group_tab_id(th, &gtid);
      omp_unset_lock(lock_ptr);

      sdata_ptr_array[sgrp_rank]->tabid = gtid;

      sdata_ptr_array[sgrp_rank]->size = sgrp_size;
      sdata_ptr_array[sgrp_rank]->in_use = sgrp_size;

      pthread_barrier_init(&(sdata_ptr_array[sgrp_rank]->barrier),NULL,sgrp_size);
    }
#pragma omp barrier
    /* All threads set their own (sub)group sdata pointer */
    new_group->sdata = sdata_ptr_array[sgrp_rank];
    new_group->id = newid;

    /* Only the root(s) need(s) to insert groups */
    if (newid == 0) {
      omp_set_lock(lock_ptr);
      th->group_tab->insert({gtid, *new_group});
      omp_unset_lock(lock_ptr);
    }

    *child = new_group;
#pragma omp barrier // to prevent the quickest thread to remove data before all others have used it
#pragma omp single
    {
      delete lock_ptr;
      delete [] sdata_ptr_array;
    }
    return rc;
}
#endif

/**
 *
 */
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
    const int group_size = group->sdata->size;
    const int group_id   = group->id;
    int rc = QV_SUCCESS;
    qvi_bbuff_t **bbuffs = nullptr;
    // Zero initialize array of pointers to nullptr.
#pragma omp single copyprivate(bbuffs) //shared bbuffs allocation
    bbuffs = qvi_new qvi_bbuff_t *[group_size]();
    if (!bbuffs) {
        rc = QV_ERR_OOR;
        goto out;
    }

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

/**
 *
 */
int
qvi_thread_group_scatter_bbuffs(
    qvi_thread_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
) {
    QVI_UNUSED(root);
    const int group_id = group->id;
    qvi_bbuff_t  ***tmp = nullptr;

    /* GM: Oh man, that is UGLY */
    /* Surely, some nice OpenMP magic will fix this mess */
#pragma omp single copyprivate(tmp)
    tmp = qvi_new qvi_bbuff_t**();
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
