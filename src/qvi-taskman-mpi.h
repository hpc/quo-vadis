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
 * @file qvi-taskman-mpi.h
 *
 * MPI process management interface.
 */

#ifndef QVI_TASKMAN_MPI_H
#define QVI_TASKMAN_MPI_H

#include "qvi-common.h"
#include "qvi-taskman.h"
#include "qvi-mpi.h"

struct qvi_group_mpi_s : public qvi_group_s {
    qvi_mpi_group_t *mpi_group = nullptr;

    // TODO(skg) Change to init, etc.
    qvi_group_mpi_s(void) {
        qvi_mpi_group_new(&mpi_group);
    }

    virtual ~qvi_group_mpi_s(void) {
        qvi_mpi_group_free(&mpi_group);
    }

    virtual int id(int *id) {
        return qvi_mpi_group_id(mpi_group, id);
    }

    virtual int size(int *size) {
        return qvi_mpi_group_size(mpi_group, size);
    }

    virtual int barrier(void) {
        return qvi_mpi_group_barrier(mpi_group);
    }
};
typedef qvi_group_mpi_s qvi_group_mpi_t;

/**
 *
 */
struct qvi_taskman_mpi_s : public qvi_taskman_s {
    /** Internal qvi_mpi instance. */
    qvi_mpi_t *m_mpi = nullptr;

    /** Constructor that does minimal work. */
    qvi_taskman_mpi_s(void) = default;

    /** The real 'constructor' that can possibly fail. */
    int
    initialize(void) {
        return qvi_mpi_new(&m_mpi);
    }

    /** Destructor */
    virtual ~qvi_taskman_mpi_s(void) {
        qvi_mpi_free(&m_mpi);
    }

    /** */
    virtual int group_create_base(
        qvi_group_t **group
    ) {
        qvi_group_mpi_t *igroup = new qvi_group_mpi_t;
        if (!igroup) return QV_ERR_OOR;

        // TODO(skg) We need to handle the top-level intrinsic process type.
        int rc = qvi_mpi_group_create_from_group_id(
            m_mpi,
            QVI_MPI_GROUP_NODE,
            &igroup->mpi_group
        );
        if (rc != QV_SUCCESS) {
            // TODO(skg) Add wrapper.
            delete igroup;
            igroup = nullptr;
        }
        *group = igroup;
        return rc;
    }

    /** */
    virtual void group_free(
        qvi_group_t **group
    ) {
    }

    /** Barrier */
    virtual int barrier(void) {
        return qvi_mpi_node_barrier(m_mpi);
    }
};
typedef qvi_taskman_mpi_s qvi_taskman_mpi_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
inline void
qvi_taskman_mpi_free(
    qvi_taskman_mpi_t **taskman
) {
    qvi_taskman_mpi_t *itm = *taskman;
    if (!itm) return;
    delete itm;
    *taskman = nullptr;
}

/**
 *
 */
inline int
qvi_taskman_mpi_new(
    qvi_taskman_mpi_t **taskman
) {
    int rc = QV_SUCCESS;

    qvi_taskman_mpi_t *itm = qvi_new qvi_taskman_mpi_t;
    if (!itm) rc = QV_ERR_OOR;

    rc = itm->initialize();
    if (rc != QV_SUCCESS) {
        qvi_taskman_mpi_free(&itm);
    }
    *taskman = itm;
    return rc;
}

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
