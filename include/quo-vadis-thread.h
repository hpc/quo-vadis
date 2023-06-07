/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) Inria 2022-2023.  All rights reserved.
 * Copyright (c) Bordeaux INP 2022-2023. All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis-thread.h
 */

#ifndef QUO_VADIS_THREAD_H
#define QUO_VADIS_THREAD_H

#include "quo-vadis.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**                                                                                                     
 * Mapping policies types.                                                                              
 */
/*
typedef enum qv_policy_s {
  QV_POLICY_PACKED     = 1,
  QV_POLICY_COMPACT    = 1,
  QV_POLICY_CLOSE      = 1,
  QV_POLICY_SPREAD     = 2,
  QV_POLICY_DISTRIBUTE = 3,
  QV_POLICY_ALTERNATE  = 3,
  QV_POLICY_CORESFIRST = 3,
  QV_POLICY_SCATTER    = 4,
  QV_POLICY_CHOOSE     = 5,
} qv_policy_t;
*/

typedef enum qv_policy_s {
  QV_POLICY_PACKED     = 1,
  QV_POLICY_COMPACT, /* same as QV_POLICY_PACKED */
  QV_POLICY_CLOSE,   /* same as QV_POLICY_PACKED */
  QV_POLICY_SPREAD,
  QV_POLICY_DISTRIBUTE,
  QV_POLICY_ALTERNATE, /* same as QV_POLICY_DISTRIBUTE */
  QV_POLICY_CORESFIRST,/* same as QV_POLICY_DISTRIBUTE */
  QV_POLICY_SCATTER,
  QV_POLICY_CHOOSE
} qv_policy_t;

/**                                                                                                     
 * Layout for fine-grain binding                                                                        
 * with default behaviour                                                                               
 */

typedef struct qv_layout_params_s {
  qv_policy_t policy;
  qv_hw_obj_type_t obj_type;
  int stride;
} qv_layout_params_t;

typedef struct qv_layout_cached_info_s {
  qv_layout_params_t params;
  hwloc_const_cpuset_t cpuset;
  hwloc_obj_type_t hwloc_obj_type;
  int nb_objs;
  int nb_threads;
  int *rsrc_idx;
} qv_layout_cached_info_t;

typedef struct qv_layout_s {
  qvi_hwloc_t *hwl; // in cached infos instead?
  hwloc_topology_t hw_topo; // in cached infos instead?
  qv_layout_params_t params;
  qv_context_t *ctx;
  qv_layout_cached_info_t cached_info;
  int is_cached;
} qv_layout_t;

/**
 * Used to pass args to pthreads.
 */
typedef struct {
  qv_context_t *ctx;
  qv_scope_t *scope;
  qv_layout_t *thread_layout;
  int th_id;
  int num_th;
} qv_thread_args_t;


/**
 * Creates a thread context.
 */
int
qv_thread_context_create(
    qv_context_t **ctx
);


/**
 * Frees resources associated with a context created by
 * qv_thread_context_create().
 */
int
qv_thread_context_free(
    qv_context_t *ctx
);

int
qv_thread_layout_create(
    qv_context_t *ctx,
    qv_layout_params_t params,
    qv_layout_t **layout
);

int
qv_thread_layout_free(
   qv_layout_t *layout
);

int
qv_thread_layout_apply(
    qv_thread_args_t th_args
);


/**
 * Used for args to pthreads.
 */
int
qv_thread_args_set(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_layout_t *thread_layout,
    int th_id,
    int num_th,
    qv_thread_args_t *th_args
);

int
qv_thread_layout_set_policy(
   qv_layout_t *layout,
   qv_policy_t policy
);

int
qv_thread_layout_set_obj_type(
   qv_layout_t *layout,
   qv_hw_obj_type_t obj_type
);

int
qv_thread_layout_set_stride(
   qv_layout_t *layout,
   int stride
);


#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
