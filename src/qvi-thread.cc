/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
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

#include "qvi-common.h"

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
struct qvi_thread_group_s {
    /** UNUSED : ID (rank) in group */
    int id = -1;
    /** ID used for table lookups */
    qvi_thread_group_id_t tabid = 0;
    /** Size of group */
     int size = 0;
    /** Barrier object (used in scope) */
    pthread_barrier_t barrier;
};

struct qvi_thread_group_shared_s {
    /** ID (rank) in group */
    /** This ID is unique to each thread */
    int id = 0;
    /** shared data between threads in the group*/
    /** ALL threads point to the same region */
    qvi_thread_group_t *sdata = nullptr;
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


static int
next_group_tab_id(
    qvi_thread_t *th,
    qvi_thread_group_id_t *gid
) {
    QVI_UNUSED(th);

    return qvi_group_next_id(gid);
}

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

void
qvi_thread_free(
    qvi_thread_t **th
) {
    if (!th) return;
    qvi_thread_t *ith = *th;

    if (!ith) goto out;

#pragma omp single
    {
      fprintf(stdout,"|||||||||||||||||||  barrier destroy @ %p\n",ith->barrier);
      pthread_barrier_destroy(ith->barrier);
      delete ith->barrier;
    }
    
    if (ith->group_tab) {
        delete ith->group_tab;
        ith->group_tab = nullptr;
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

#pragma omp single
    {
#ifdef OPENMP_FOUND
      /* GM: since all spawned threads are in the zgroup */
      /* using omp_get_num_threads is OK */
      pthread_barrier_init(th->barrier,NULL,omp_get_num_threads());
#else
      /* pthread_barrier_init(th->barrier,NULL, ???); */
#endif
      fprintf(stdout,"||||||||||||||||||| barrier init  thread %i\n",qvi_gettid());
    }
    
    return qvi_task_init(
        th->task, QV_TASK_TYPE_THREAD, qvi_gettid(), world_id, node_id
    );
}

int
qvi_thread_finalize(
    qvi_thread_t *
) {
    return QV_SUCCESS;
}

int
qvi_thread_node_barrier(
    qvi_thread_t *th
) {
    fprintf(stdout,"||||||||||||||||||| barrier wait  thread %i\n",qvi_gettid());
    pthread_barrier_wait(th->barrier);
    return QV_SUCCESS;
}

qvi_task_t *
qvi_thread_task_get(
    qvi_thread_t *th
) {
    return th->task;
}

int
qvi_thread_group_new(
    qvi_thread_group_shared_t **thgrp
) {
    int rc = QV_SUCCESS;
    qvi_thread_group_shared_t *sdata = qvi_new qvi_thread_group_shared_t();
    qvi_thread_group_t *ithgrp = nullptr; 

    if (!sdata) {
        rc = QV_ERR_OOR;
    }
    if (rc != QV_SUCCESS) {
        qvi_thread_group_free(&sdata);
    }

#pragma omp single copyprivate(ithgrp)
    ithgrp = qvi_new qvi_thread_group_t();

    sdata->sdata = ithgrp;
    
    *thgrp = sdata;
    return rc;
}

void
qvi_thread_group_free(
    qvi_thread_group_shared_t **thgrp
) {
     if (!thgrp) return;
    qvi_thread_group_shared_t *ithgrp = *thgrp;
    if (!ithgrp) goto out;

#pragma omp single
    {
      fprintf(stdout,">>>>> barrier destroy for thread %i @ %p\n",ithgrp->id,&(ithgrp->sdata->barrier));
      pthread_barrier_destroy(&(ithgrp->sdata->barrier));
      delete ithgrp->sdata;
    }
    
    delete ithgrp;
out:
    *thgrp = nullptr;
}

int
qvi_thread_group_create(
    qvi_thread_t *th,
    qvi_thread_group_shared_t **group
) {
    qvi_thread_group_shared_t *igroup = nullptr;
    qvi_thread_group_id_t gtid;
    int rc;
    
#pragma omp single copyprivate(gtid, rc)
    rc = next_group_tab_id(th, &gtid);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_thread_group_new(&igroup);
    if (rc != QV_SUCCESS) goto out;

    igroup->sdata->tabid = gtid;

#ifdef OPENMP_FOUND
    igroup->id          = omp_get_thread_num();
    igroup->sdata->size = omp_get_num_threads();
    fprintf(stdout,">>>>> init group for thread %i (%i) out of %i | shared group addr %p (gtid %i)\n",
	    igroup->id,omp_get_thread_num(),igroup->sdata->size,igroup->sdata,gtid);
#else
    igroup->id          = 0;
    igroup->sdata->size = 1;
#endif
    
#pragma omp single        
    {
      pthread_barrier_init(&(igroup->sdata->barrier),NULL,igroup->sdata->size);
      fprintf(stdout,">>>>> barrier init done for thread %i @%p (size %i)\n",
	      igroup->id,&(igroup->sdata->barrier),igroup->sdata->size);
    }
    fprintf(stdout,">>>>> barrier object thread %i @%p (size %i)\n",igroup->id,&(igroup->sdata->barrier),igroup->sdata->size);
    
#pragma omp single
    th->group_tab->insert({gtid, *(igroup->sdata)});
    
out:
    if (rc != QV_SUCCESS) {
      qvi_thread_group_free(&igroup);
    }
    *group = igroup;
    return QV_SUCCESS;
}

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

int
qvi_thread_group_id(
    const qvi_thread_group_shared_t *group
) {
    return group->id;
}

int
qvi_thread_group_size(
    const qvi_thread_group_shared_t *group
) {
    return group->sdata->size;
}

int
qvi_thread_group_barrier(
    qvi_thread_group_shared_t *group
) {

    fprintf(stdout,">>>>> Barrier wait for thread %i @ %p\n",group->id,&(group->sdata->barrier));
    pthread_barrier_wait(&(group->sdata->barrier));
  
    return QV_SUCCESS;
}

int
qvi_thread_group_gather_bbuffs(
    qvi_thread_group_shared_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs
) {
    const int group_size = qvi_thread_group_size(group);
    // Make sure that we are dealing with a valid thread group.
    assert(root == 0 && group_size == 1);
    if (root != 0 || group_size != 1) {
        return QV_ERR_INTERNAL;
    }

    int rc = QV_SUCCESS;
    int *rxcounts = nullptr;
    byte_t *bytepos = nullptr;
    // Zero initialize array of pointers to nullptr.
    qvi_bbuff_t **bbuffs = qvi_new qvi_bbuff_t *[group_size]();
    if (!bbuffs) {
        rc = QV_ERR_OOR;
        goto out;
    }

    rxcounts = qvi_new int[group_size]();
    if (!rxcounts) {
        rc = QV_ERR_OOR;
        goto out;
    }
    rxcounts[0] = qvi_bbuff_size(txbuff);

    bytepos = (byte_t *)qvi_bbuff_data(txbuff);
    for (int i = 0; i < group_size; ++i) {
        rc = qvi_bbuff_new(&bbuffs[i]);
        if (rc != QV_SUCCESS) break;
        rc = qvi_bbuff_append(bbuffs[i], bytepos, rxcounts[i]);
        if (rc != QV_SUCCESS) break;
        bytepos += rxcounts[i];
    }
out:
    delete[] rxcounts;
    if (rc != QV_SUCCESS) {
        if (bbuffs) {
            for (int i = 0; i < group_size; ++i) {
                qvi_bbuff_free(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
        bbuffs = nullptr;
    }
    *rxbuffs = bbuffs;
    return rc;
}

int
qvi_thread_group_scatter_bbuffs(
    qvi_thread_group_shared_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
) {
    const int group_size = qvi_thread_group_size(group);
    // Make sure that we are dealing with a valid thread group.
    assert(root == 0 && group_size == 1);
    if (root != 0 || group_size != 1) {
        return QV_ERR_INTERNAL;
    }
    // There should always be only one at the root (us).
    qvi_bbuff_t *inbuff = txbuffs[root];
    const size_t data_size = qvi_bbuff_size(inbuff);
    const void *data = qvi_bbuff_data(inbuff);

    qvi_bbuff_t *mybbuff = nullptr;
    int rc = qvi_bbuff_new(&mybbuff);
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_bbuff_append(mybbuff, data, data_size);
out:
    if (rc != QV_SUCCESS) {
        qvi_bbuff_free(&mybbuff);
    }
    *rxbuff = mybbuff;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
