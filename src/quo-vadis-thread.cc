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
 * @file quo-vadis-thread.cc
 */

#include "qvi-common.h" // IWYU pragma: keep

#include "quo-vadis-thread.h"
#include "qvi-context.h"
#include "qvi-group-thread.h"
#include "qvi-scope.h"
#include "qvi-utils.h"
#ifdef OPENMP_FOUND
#include <omp.h>
#endif

int
qv_thread_context_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    try {
        qvi_delete(&ctx);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

static int
qvi_thread_context_create(
    qv_context_t **ctx
) {
    // Create base context.
    qv_context_t *ictx = nullptr;
    int rc = qvi_new_rc(&ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    // Create and initialize the base group.
    qvi_zgroup_thread_s *izgroup;
    rc = qvi_new_rc(&izgroup);
    if (rc != QV_SUCCESS) {
        goto out;
    }
    ictx->zgroup = izgroup;
    // Connect to RMI server.
    rc = qvi_context_connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    rc = qvi_bind_stack_init(
        ictx->bind_stack,
        ictx->zgroup->task(),
        ictx->rmi
    );
out:
    if (rc != QV_SUCCESS) {
        qvi_delete(&ictx);
    }
    *ctx = ictx;
    return rc;
}

int
qv_thread_context_create(
    qv_context_t **ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    try {
        return qvi_thread_context_create(ctx);
    }
    qvi_catch_and_return();
}

int
qv_thread_scope_split(
    qv_context_t *,
    qv_scope_t *scope,
    int npieces,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscope
){
    qv_hw_obj_type_t type;
    const int rc = qvi_scope_obj_type(scope, npieces, &type);
    if (rc != QV_SUCCESS) return rc;
    //qvi_log_info("=================== Type for split is {}",(int)type);
    return qvi_scope_ksplit_at(scope, type, color_array, nthreads, subscope);
}

int
qv_thread_scope_split_at(
    qv_context_t *,
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int *color_array,
    int nthreads,
    qv_scope_t ***subscopes
) {
    return qvi_scope_ksplit_at(scope, type, color_array, nthreads, subscopes);
}

#ifndef USE_LAYOUTS
//New interface
void *
qv_thread_routine(
    void * arg
) {
    qv_thread_args_t *arg_ptr = (qv_thread_args_t *)arg;
    int rc = qv_bind_push(arg_ptr->ctx, arg_ptr->scope);
    if (rc != QV_SUCCESS) {
        qvi_log_error("qv_bind_push() failed");
        pthread_exit(NULL);
    }

    void *ret = arg_ptr->thread_routine(arg_ptr->arg);
    // Free memory allocated in qv_pthread_create
    delete arg_ptr;
    pthread_exit(ret);
}

int
qv_pthread_create(
     pthread_t *thread,
     pthread_attr_t *attr,
     void *(*thread_routine)(void *arg),
     void *arg,
     qv_context_t *ctx,
     qv_scope_t *scope
) {
     // Memory will be freed in qv_thread_routine to avoid memory leaks.
     qv_thread_args_t *arg_ptr = nullptr;
     int rc = qvi_new_rc(&arg_ptr);
     if (rc != QV_SUCCESS) return rc;

     arg_ptr->ctx = ctx;
     arg_ptr->scope = scope;
     arg_ptr->thread_routine = thread_routine;
     arg_ptr->arg = arg;

    return pthread_create(thread, attr, qv_thread_routine, arg_ptr);
}
#else // USE_LAYOUTS
// Layout interface
int
qv_thread_layout_create( // use hwpool if necessary
    qv_context_t *ctx,
    qv_layout_params_t params,
    qv_layout_t **layout
) {
    int rc = QV_SUCCESS;
    qv_layout_t *ilay = qvi_new qv_layout_t();

    ilay->hwl       = qvi_rmi_client_hwloc_get(ctx->rmi);
    ilay->hw_topo   = qvi_hwloc_get_topo_obj(ilay->hwl);
    ilay->params    = params;
    ilay->ctx       = ctx;
    memset(&ilay->cached_info,0,sizeof(qv_layout_cached_info_t));
    ilay->is_cached = 0;

    *layout = ilay;
    return rc;
}

int
qv_thread_layout_free(
   qv_layout_t *layout
){
    if (!layout) return QV_ERR_INVLD_ARG;
    //fixme check that is was actually allocated.
    delete[] layout->cached_info.rsrc_idx;
    delete layout;
    return QV_SUCCESS;
}

int
qv_thread_layout_set_policy(
   qv_layout_t *layout,
   qv_policy_t policy
) {
  int rc = QV_SUCCESS;

  if (!layout) {
      rc = QV_ERR_INVLD_ARG;
      goto out;
  }

  layout->params.policy = policy;

 out:
  return rc;
}

int
qv_thread_layout_set_obj_type(
   qv_layout_t *layout,
   qv_hw_obj_type_t obj_type
) {
  int rc = QV_SUCCESS;

  if (!layout) {
      rc = QV_ERR_INVLD_ARG;
      goto out;
  }

  layout->params.obj_type = obj_type;

 out:
  return rc;
}

int
qv_thread_layout_set_stride(
   qv_layout_t *layout,
   int stride
) {
  int rc = QV_SUCCESS;

  if (!layout) {
      rc = QV_ERR_INVLD_ARG;
      goto out;
  }

  layout->params.stride = stride;

 out:
  return rc;
}

// Intel policies (KMP_AFFINITY) are :
// - disabled: prevents the runtime library from making any affinity-related
//             system calls (to avoid interference with other platform affinity mechanisms).
// - compact: threads are placed as close together as possible.
// - scatter: threads are distributed as evenly as possible across the entire system.
//            (opposite of compact).
// - explicit: threads are placed according to a list of OS proc IDs (required)
static int
compute(
    int *array,
    int nb_threads,
    int nb_objs,
    int stride,
    qv_policy_t policy
) {
    int rc = QV_SUCCESS;
    switch(policy)
      {
      case QV_POLICY_SPREAD: // Same as SCATTER?
          {
              break;
          }
      case QV_POLICY_DISTRIBUTE:
      case QV_POLICY_ALTERNATE:
      case QV_POLICY_CORESFIRST:
          {
              break;
          }
      case QV_POLICY_SCATTER:
          {
              break;
          }
      case QV_POLICY_CHOOSE: // rename to EXPLICIT?
          {
              break;
          }
      case QV_POLICY_PACKED:
      case QV_POLICY_COMPACT:
      case QV_POLICY_CLOSE:
      default:
          {
              for(int idx = 0 ; idx < nb_threads ; idx++) {
                  array[idx] = (idx+idx*(stride-1))%(nb_objs);
              }
              break;
          }
      }
    return rc;
}

int
qv_thread_layout_apply( //use map interface if necessary
    qv_thread_args_t th_args
) {
  int rc = QV_SUCCESS;
  qv_context_t *parent_ctx = th_args.ctx;
  qv_scope_t *parent_scope = th_args.scope;
  qv_layout_t *thread_layout = th_args.thread_layout;
  int num_thr = th_args.num_th;

  /* Brand new case: compute and cache as much as possible*/
  if(thread_layout->is_cached == 0){
      thread_layout->ctx = parent_ctx;
      thread_layout->cached_info.params = thread_layout->params;
      thread_layout->cached_info.cpuset = qvi_scope_cpuset_get(parent_scope);
      thread_layout->cached_info.hwloc_obj_type = qvi_hwloc_get_obj_type(thread_layout->cached_info.params.obj_type);
      rc = qvi_hwloc_get_nobjs_in_cpuset(thread_layout->hwl,
                                         thread_layout->cached_info.params.obj_type,
                                         thread_layout->cached_info.cpuset,
                                         &(thread_layout->cached_info.nb_objs));
      thread_layout->cached_info.nb_threads = num_thr;
      thread_layout->cached_info.rsrc_idx = new int[thread_layout->cached_info.nb_threads]{-1};

      rc = compute(thread_layout->cached_info.rsrc_idx,
                   thread_layout->cached_info.nb_threads,
                   thread_layout->cached_info.nb_objs,
                   thread_layout->cached_info.params.stride,
                   thread_layout->cached_info.params.policy);

      thread_layout->is_cached = 1;

  } else if (thread_layout->is_cached) {
      // Are we using the right context?
      if (thread_layout->ctx != parent_ctx) {
          // pointer comparison might not be enough.
          // do we need a unique ctxt id?
          rc = QV_ERR_INVLD_ARG;
          return rc;
      } else {
          // check changes and recompute if necessary
          int recompute = 0;

          hwloc_obj_type_t hwloc_obj_type = qvi_hwloc_get_obj_type(thread_layout->params.obj_type);
          if ( hwloc_obj_type  != thread_layout->cached_info.hwloc_obj_type ){
              int nb_objs;

              //no need to systematically recompute in this case:
              // recompute iff number of objects is different.
              // otherwise: keep same parameters and apply to new objects
              thread_layout->cached_info.hwloc_obj_type = hwloc_obj_type;

              rc = qvi_hwloc_get_nobjs_in_cpuset(thread_layout->hwl,
                                                 thread_layout->params.obj_type,
                                                 thread_layout->cached_info.cpuset,
                                                 &nb_objs);

              if(nb_objs != thread_layout->cached_info.nb_objs){
                  thread_layout->cached_info.nb_objs = nb_objs;
                  recompute++;
              }
          }

          /* cpuset is different, recompute info */
          //if (hwloc_bitmap_compare(thread_layout->cached_info.cpuset,qvi_scope_cpuset_get(parent_scope)) != 0 ) {
          //   thread_layout->cached_info.cpuset = qvi_scope_cpuset_get(parent_scope);
          //    recompute++;
          //}


          if (num_thr != thread_layout->cached_info.nb_threads) {
              thread_layout->cached_info.nb_threads = num_thr;
              recompute++;
          }

          if(thread_layout->cached_info.params.stride != thread_layout->params.stride){
              thread_layout->cached_info.params.stride = thread_layout->params.stride;
              recompute++;
          }

          if (thread_layout->cached_info.params.policy != thread_layout->params.policy){
              thread_layout->cached_info.params.policy = thread_layout->params.policy;
              recompute++;
          }

          if(recompute){
              rc = compute(thread_layout->cached_info.rsrc_idx,
                           thread_layout->cached_info.nb_threads,
                           thread_layout->cached_info.nb_objs,
                           thread_layout->cached_info.params.stride,
                           thread_layout->cached_info.params.policy);
              //fprintf(stdout,"***********************>>>>>>>>>  recompute done\n");
          }
      }
  }

  // FIXME : what should we do in oversubscribing case?
  // GM EDIT: Mutiple threads on objects featuring multiple PUs is not an issue
  // Oversub test : check that an index does not appear twice in the resrc indices array
  if(thread_layout->cached_info.nb_objs < thread_layout->cached_info.nb_threads)
    {
      if (!thr_idx)
          fprintf(stdout,"====> Oversubscribing \n");
    }
  else {
    if (!thr_idx)
      fprintf(stdout,"====> Resource number is ok\n");
  }

  /* enforce binding */
  hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(thread_layout->hw_topo,
                                                        thread_layout->cached_info.cpuset,
                                                        thread_layout->cached_info.hwloc_obj_type,
                                                        thread_layout->cached_info.rsrc_idx[thr_idx]);

  hwloc_set_cpubind(thread_layout->hw_topo,obj->cpuset,HWLOC_CPUBIND_THREAD);

  fprintf(stdout,"[THREAD #%i] === bound on resrc #%i\n",thr_idx,obj->logical_index);

  /* Sanity check */
  //assert(obj->logical_index == thread_layout->cached_info.rsrc_idx[thr_idx]);

  return rc;
}

int
qv_thread_args_set(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_layout_t *thread_layout,
    int th_id,
    int num_th,
    qv_thread_args_t *th_args
) {
    if (!ctx || !scope || !thread_layout || !th_args) {
        return QV_ERR_INVLD_ARG;
    }

    th_args->ctx = ctx;
    th_args->scope = scope;
    th_args->thread_layout = thread_layout;
    th_args->th_id = th_id;
    th_args->num_th = num_th;

    return QV_SUCCESS;
}

#endif // USE_LAYOUTS

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
