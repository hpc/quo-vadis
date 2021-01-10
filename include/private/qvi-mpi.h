/*
 * Copyright (c)      2020 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2020 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-mpi.h
 */

#ifndef QVI_MPI_H
#define QVI_MPI_H

#include "quo-vadis/qv-mpi.h"
#include "quo-vadis/qv-task.h"

#ifdef __cplusplus
extern "C" {
#endif

// Type definitions.
typedef uint64_t qvi_mpi_group_id_t;

// Forward declarations.
struct qvi_mpi_s;
typedef struct qvi_mpi_s qvi_mpi_t;

struct qvi_mpi_group_s;
typedef struct qvi_mpi_group_s qvi_mpi_group_t;

/**
 * Intrinsic group IDs.
 */
static const qvi_mpi_group_id_t QVI_MPI_GROUP_NULL = 0;
static const qvi_mpi_group_id_t QVI_MPI_GROUP_SELF = 1;
static const qvi_mpi_group_id_t QVI_MPI_GROUP_NODE = 2;
// Sentinel value marking first available group ID. This is expected to be the
// very last item, so don't change its relative position without carefully
// modifying at the code that uses this value.
static const qvi_mpi_group_id_t QVI_MPI_GROUP_INTRINSIC_END = 3;

/**
 *
 */
int
qvi_mpi_construct(
    qvi_mpi_t **mpi
);

/**
 *
 */
void
qvi_mpi_destruct(
    qvi_mpi_t *mpi
);

/**
 *
 */
int
qvi_mpi_init(
    qvi_mpi_t *mpi,
    qv_task_t *task,
    MPI_Comm comm
);

/**
 *
 */
int
qvi_mpi_finalize(
    qvi_mpi_t *mpi
);

/**
 *
 */
int
qvi_mpi_group_construct(
    qvi_mpi_group_t **group
);

/**
 *
 */
void
qvi_mpi_group_destruct(
    qvi_mpi_group_t *group
);

/**
 *
 */
int
qvi_mpi_group_size(
    qvi_mpi_t *mpi,
    const qvi_mpi_group_t *group,
    int *size
);

/**
 *
 */
int
qvi_mpi_group_lookup_by_id(
    qvi_mpi_t *mpi,
    qvi_mpi_group_id_t id,
    qvi_mpi_group_t *group
);

/**
 *
 */
int
qvi_mpi_group_incl(
    qvi_mpi_t *mpi,
    const qvi_mpi_group_t *group,
    int n_ids,
    const int *ids,
    qvi_mpi_group_t **maybe_group
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
