/*
 * Copyright (c)      2022 Triad National Security, LLC
 *                         All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif

/**                                                                                                     
 * Mapping policies types.                                                                              
 */
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

/**                                                                                                     
 * Layout for fine-grain binding                                                                        
 * with default behaviour                                                                               
 */
typedef struct qv_layout_s {
  qv_policy_t policy;
  qv_hw_obj_type_t obj_type;
  int stride;
} qv_layout_t;

  
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
qv_thread_layout_init(
    qv_layout_t *layout,
    qv_policy_t policy,
    qv_hw_obj_type_t obj_type,
    int stride
);
  
int
qv_thread_layout_apply(
    qv_context_t *parent_ctx,
    qv_scope_t *parent_scope,
    qv_layout_t thread_layout
);

  
#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
