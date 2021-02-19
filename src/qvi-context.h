/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-context.h
 *
 * @note This file breaks convention by defining a symbol starting with qv_.
 * There is good reason for this: we want to hide the implementation details of
 * QV contexts, but require its definition in a header that is accessible to
 * multiple internal consumers.
 */

#ifndef QVI_CONTEXT_H
#define QVI_CONTEXT_H

#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-rmi.h"
// TODO(skg) This should be included conditionally.
#include "qvi-mpi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The underlying datastructure that defines an opaque QV context.
 */
struct qv_context_s {
    qv_task_t *task = nullptr;
    qvi_hwloc_t *hwloc = nullptr;
    qvi_rmi_client_t *rmi = nullptr;
    qvi_mpi_t *mpi = nullptr;
};

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
