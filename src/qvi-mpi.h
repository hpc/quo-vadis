/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
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

#include "qvi-common.h"
#include "qvi-group.h"
#include "quo-vadis-mpi.h" // IWYU pragma: keep

// Forward declarations.
struct qvi_mpi_s;
typedef struct qvi_mpi_s qvi_mpi_t;

struct qvi_mpi_group_s;
typedef struct qvi_mpi_group_s qvi_mpi_group_t;

/**
 * Intrinsic group IDs.
 */
static constexpr qvi_group_id_t QVI_MPI_GROUP_NULL = 0;
static constexpr qvi_group_id_t QVI_MPI_GROUP_SELF = 1;
static constexpr qvi_group_id_t QVI_MPI_GROUP_NODE = 2;
static constexpr qvi_group_id_t QVI_MPI_GROUP_WORLD = 3;

/**
 *
 */
int
qvi_mpi_new(
    qvi_mpi_t **mpi
);

/**
 *
 */
void
qvi_mpi_delete(
    qvi_mpi_t **mpi
);

/**
 *
 */
int
qvi_mpi_init(
    qvi_mpi_t *mpi,
    MPI_Comm comm
);

/**
 *
 */
int
qvi_mpi_group_new(
    qvi_mpi_group_t **group
);

/**
 *
 */
void
qvi_mpi_group_delete(
    qvi_mpi_group_t **group
);

/**
 *
 */
int
qvi_mpi_group_size(
    const qvi_mpi_group_t *group
);

/**
 *
 */
int
qvi_mpi_group_rank(
    const qvi_mpi_group_t *group
);

/**
 *
 */
int
qvi_mpi_group_lookup_by_id(
    qvi_mpi_t *mpi,
    qvi_group_id_t id,
    qvi_mpi_group_t &group
);

/**
 *
 */
int
qvi_mpi_group_create_from_group_id(
    qvi_mpi_t *mpi,
    qvi_group_id_t id,
    qvi_mpi_group_t **group
);

/**
 *
 */
int
qvi_mpi_group_create_from_mpi_comm(
    qvi_mpi_t *mpi,
    MPI_Comm comm,
    qvi_mpi_group_t **new_group
);

/**
 *
 */
int
qvi_mpi_group_create_from_split(
    qvi_mpi_t *mpi,
    const qvi_mpi_group_t *parent,
    int color,
    int key,
    qvi_mpi_group_t **child
);

/**
 *
 */
int
qvi_mpi_group_barrier(
    qvi_mpi_group_t *group
);

/**
 *
 */
int
qvi_mpi_group_gather_bbuffs(
    qvi_mpi_group_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    bool *shared_alloc,
    qvi_bbuff_t ***rxbuffs
);

/**
 *
 */
int
qvi_mpi_group_scatter_bbuffs(
    qvi_mpi_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
);

/**
 *
 */
int
qvi_mpi_group_comm_dup(
    qvi_mpi_group_t *group,
    MPI_Comm *comm
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
