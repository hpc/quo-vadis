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

#include "private/qvi-common.h"

#include "mpi.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_mpi_s;
typedef struct qvi_mpi_s qvi_mpi_t;

struct qvi_mpi_group_s;
typedef struct qvi_mpi_group_s qvi_mpi_group_t;

/**
 * Built-in groups.
 */
enum qvi_mpi_group_intrinsic_e {
    QVI_MPI_GROUP_NULL = 0,
    QVI_MPI_GROUP_SELF,
    QVI_MPI_GROUP_NODE,
    // Sentinel value marking first available group ID.
    QVI_MPI_GROUP_INTRINSIC_END
};

/**
 *
 */
int
qvi_mpi_construct(
    qvi_mpi_t **pmi
);

/**
 *
 */
void
qvi_mpi_destruct(
    qvi_mpi_t *pmi
);

/**
 *
 */
int
qvi_mpi_init(
    qvi_mpi_t *pmi,
    MPI_Comm comm
);

/**
 *
 */
int
qvi_mpi_finalize(
    qvi_mpi_t *pmi
);

/**
 *
 */
int
qvi_mpi_node_size(
    qvi_mpi_t *mpi
);

/**
 *
 */
int
qvi_mpi_node_id(
    qvi_mpi_t *pmi
);

/**
 *
 */
int
qvi_mpi_world_size(
    qvi_mpi_t *pmi
);

/**
 *
 */
int
qvi_mpi_world_id(
    qvi_mpi_t *pmi
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */