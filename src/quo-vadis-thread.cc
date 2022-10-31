/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright © Inria 2022.  All rights reserved.
 * Copyright © Bordeaux INP 2022. All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis-thread.cc
 */

#include "qvi-common.h"

#include "quo-vadis-thread.h"

#include "qvi-context.h"
#include "qvi-zgroup-thread.h"
#include "qvi-group-thread.h"
#include "qvi-scope.h"

#ifdef OPENMP_FOUND
#include <omp.h>
#endif

int
qv_thread_context_free(
    qv_context_t *ctx
) {
    if (!ctx) return QV_ERR_INVLD_ARG;
    delete ctx->zgroup;
    qvi_context_free(&ctx);
    return QV_SUCCESS;
}

int
qv_thread_context_create(
    qv_context_t **ctx
) {
    if (!ctx) {
        return QV_ERR_INVLD_ARG;
    }

    int rc = QV_SUCCESS;
    qv_context_t *ictx = nullptr;
    qvi_zgroup_thread_t *izgroup = nullptr;

    // Create base context.
    rc = qvi_context_new(&ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    // Create and initialize the base group.
    rc = qvi_zgroup_thread_new(&izgroup);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    // Save zgroup instance pointer to context.
    ictx->zgroup = izgroup;
    
    rc = izgroup->initialize();
    if (rc != QV_SUCCESS) {
        goto out;
    }

    // Connect to RMI server.
    rc = qvi_context_connect_to_server(ictx);
    if (rc != QV_SUCCESS) {
        goto out;
    }

    rc = qvi_bind_stack_init(
        ictx->bind_stack,
        qvi_thread_task_get(izgroup->zth),
        ictx->rmi
    );
out:
    if (rc != QV_SUCCESS) {
        (void)qv_thread_context_free(ictx);
        ictx = nullptr;
    }
    *ctx = ictx;
    return rc;
}

int
qv_thread_layout_init(
    qv_layout_t *layout,		      
    qv_policy_t policy,
    qv_hw_obj_type_t obj_type,
    int stride
)
{
  int rc = QV_SUCCESS;
  layout->policy   = policy;
  layout->obj_type = obj_type;
  layout->stride   = stride;
  return rc;
}

static hwloc_obj_type_t
qv_thread_convert_obj_type(
    qv_hw_obj_type_t obj_type
)
{
  hwloc_obj_type_t type;
  
  if(obj_type == QV_HW_OBJ_PU)
    type = HWLOC_OBJ_PU;
  else if(obj_type == QV_HW_OBJ_CORE)
    type = HWLOC_OBJ_CORE;
  else if(obj_type == QV_HW_OBJ_PACKAGE)
    type = HWLOC_OBJ_PACKAGE;
  else if (obj_type == QV_HW_OBJ_MACHINE)
    type = HWLOC_OBJ_MACHINE;

  return type;
}

int
qv_thread_layout_apply(
    qv_context_t *parent_ctx,
    qv_scope_t *parent_scope,
    qv_layout_t thread_layout
)
{
  int rc = QV_SUCCESS;
  qvi_hwloc_t *hwl = qvi_rmi_client_hwloc_get(parent_ctx->rmi);
  hwloc_topology_t hwloc_topology = qvi_hwloc_get_topo_obj(hwl);
  hwloc_obj_type_t obj_type = qv_thread_convert_obj_type(thread_layout.obj_type);
  hwloc_const_cpuset_t cpuset = qvi_scope_cpuset_get(parent_scope);
#ifdef OPENMP_FOUND
  int thr_idx = omp_get_thread_num();
  int num_thr = omp_get_num_threads();
#endif 
  int nb_objs = 0; 
  rc = qvi_hwloc_get_nobjs_in_cpuset(hwl,thread_layout.obj_type,cpuset, &nb_objs);
  
  /* FIXME : what should we do in oversubscribing case ?? */
  if(nb_objs < num_thr)    
    {
      if (!thr_idx)
	fprintf(stdout,"====> Oversubscribing \n");
    }
  else {
    if (!thr_idx)
      fprintf(stdout,"====> Resource number is ok\n");
  }

  switch(thread_layout.policy)
    {
    case QV_POLICY_PACKED:
      //case QV_POLICY_COMPACT:
      //case QV_POLICY_CLOSE:
      {
	hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(hwloc_topology,
							      cpuset,
							      obj_type,
							      (thr_idx+thr_idx*thread_layout.stride)%nb_objs);
	
	
	hwloc_set_cpubind(hwloc_topology,obj->cpuset,HWLOC_CPUBIND_THREAD);	
	fprintf(stdout,"[OMP TH #%i] === bound on resrc #%i\n",thr_idx,obj->logical_index);
	/* perform sanity check here : get_proc_cpubind + obj from cpuset + logical idx and compare */
	break;
      }
    case QV_POLICY_SPREAD:
      {
	break;
      }
    case QV_POLICY_DISTRIBUTE:
      //case QV_POLICY_ALTERNATE:
      //case QV_POLICY_CORESFIRST:
      {
	break;
      }
    case QV_POLICY_SCATTER:
      {
	break;
      }
    case QV_POLICY_CHOOSE:
      {
	break;
      }      
    }
  
  return rc;
}




/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
