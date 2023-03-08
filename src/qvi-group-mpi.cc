/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-group-mpi.cc
 */

#include "qvi-common.h"
#include "qvi-group-mpi.h"

qvi_group_mpi_s::~qvi_group_mpi_s(void)
{
    qvi_mpi_group_free(&mpi_group);
}

int
qvi_group_mpi_s::create(void)
{
    return qvi_mpi_group_new(&mpi_group);
}

int
qvi_group_mpi_s::initialize(
    qvi_mpi_t *mpi
) {
    assert(mpi);
    if (!mpi) return QV_ERR_INTERNAL;

    this->mpi = mpi;
    return QV_SUCCESS;
}

qvi_task_id_t
qvi_group_mpi_s::task_id(void)
{
    return qvi_task_task_id(qvi_mpi_task_get(mpi));
}

int
qvi_group_mpi_s::id(void)
{
    return qvi_mpi_group_id(mpi_group);
}

int
qvi_group_mpi_s::size(void)
{
    return qvi_mpi_group_size(mpi_group);
}

int
qvi_group_mpi_s::barrier(void)
{
    return qvi_mpi_group_barrier(mpi_group);
}

int
qvi_group_mpi_s::self(
    qvi_group_t **child
) {
    int rc = QV_SUCCESS;

    qvi_group_mpi_t *ichild = qvi_new qvi_group_mpi_t();
    if (!ichild) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Initialize the child with the parent's MPI instance.
    rc = ichild->initialize(mpi);
    if (rc != QV_SUCCESS) goto out;
    // Create the underlying group using MPI_COMM_SELF.
    rc = qvi_mpi_group_create_from_mpi_comm(
        mpi, MPI_COMM_SELF, &ichild->mpi_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete ichild;
        ichild = nullptr;
    }
    *child = ichild;
    return rc;
}

int
qvi_group_mpi_s::split(
    int color,
    int key,
    qvi_group_t **child
) {
    int rc = QV_SUCCESS;

    qvi_group_mpi_t *ichild = qvi_new qvi_group_mpi_t();
    if (!ichild) {
        rc = QV_ERR_OOR;
        goto out;
    }
    // Initialize the child with the parent's MPI instance.
    rc = ichild->initialize(mpi);
    if (rc != QV_SUCCESS) goto out;
    // Split this group using MPI.
    rc = qvi_mpi_group_create_from_split(
        mpi, mpi_group, color,
        key, &ichild->mpi_group
    );
out:
    if (rc != QV_SUCCESS) {
        delete ichild;
        ichild = nullptr;
    }
    *child = ichild;
    return rc;
}

int
qvi_group_mpi_s::gather(
    qvi_bbuff_t *txbuff,
    int root,
    qvi_bbuff_t ***rxbuffs,
    int *shared
) {
    return qvi_mpi_group_gather_bbuffs(
        mpi_group, txbuff, root, rxbuffs, shared
    );
}

int
qvi_group_mpi_s::scatter(
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
) {
    return qvi_mpi_group_scatter_bbuffs(
        mpi_group, txbuffs, root, rxbuff
    );
}

int
qvi_group_mpi_s::comm_dup(
    MPI_Comm *comm
) {
    return qvi_mpi_group_comm_dup(mpi_group, comm);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
