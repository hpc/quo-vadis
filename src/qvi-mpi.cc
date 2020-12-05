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
 * @file qvi-mpi.cc
 */

#include "private/qvi-common.h"
#include "private/qvi-mpi.h"
#include "private/qvi-log.h"

#include <unordered_map>

struct qvi_mpi_group_s {
    MPI_Group mpi_group;
};

struct qvi_mpi_s {
    /** Node ID */
    int node_id;
    /** World ID */
    int world_id;
    /** Node size */
    int node_size;
    /** World size */
    int world_size;
    /** Duplicate of MPI_COMM_WORLD */
    MPI_Comm world_comm;
    /** Node communicator */
    MPI_Comm node_comm;
    /** Maintains the next available group ID value */
    uint64_t group_next_id;
    /** Group table (ID to internal structure mapping) */
    std::unordered_map<uint64_t, qvi_mpi_group_t> group_tab;
};

/**
 * Returns the next available group ID.
 */
static int
next_group_id(
    qvi_mpi_t *mpi,
    uint64_t *gid
) {
    if (mpi->group_next_id == UINT64_MAX) {
        qvi_log_error("qvi_mpi_group ID space exhausted");
        return QV_ERR_OOR;
    }
    *gid = mpi->group_next_id++;
    return QV_SUCCESS;
}

int
qvi_mpi_construct(
    qvi_mpi_t **mpi
) {
    if (!mpi) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;

    qvi_mpi_t *impi = (qvi_mpi_s *)calloc(1, sizeof(*impi));
    if (!impi) {
        qvi_log_error("calloc() failed");
        rc = QV_ERR_OOR;
    }
    // Communicators
    impi->world_comm = MPI_COMM_NULL;
    impi->node_comm = MPI_COMM_NULL;
    // Groups
    impi->group_next_id = QVI_MPI_GROUP_INTRINSIC_END;
    *mpi = impi;
    return rc;
}

void
qvi_mpi_destruct(
    qvi_mpi_t *mpi
) {
    if (!mpi) return;
    if (mpi->world_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&mpi->world_comm);
    }
    if (mpi->node_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&mpi->node_comm);
    }
    // TODO(skg) Free groups
    free(mpi);
}

/**
 * Creates the node communicator. Returns the MPI status.
 */
static int
create_intrinsic_comms(
    qvi_mpi_t *mpi,
    MPI_Comm comm
) {
    if (!mpi) return QV_ERR_INVLD_ARG;

    int rc;
    // MPI_COMM_WORLD duplicate
    rc = MPI_Comm_dup(
        MPI_COMM_WORLD,
        &mpi->world_comm
    );
    if (rc != MPI_SUCCESS) {
        static char const *ers = "MPI_Comm_dup(MPI_COMM_WORLD) failed";
        qvi_log_error(ers);
        return rc;
    }
    // Node communicator
    rc = MPI_Comm_split_type(
        comm,
        MPI_COMM_TYPE_SHARED,
        0,
        MPI_INFO_NULL,
        &mpi->node_comm
    );
    if (rc != MPI_SUCCESS) {
        qvi_log_error("MPI_Comm_split_type() failed");
        return rc;
    }
    return rc;
}

static int
create_intrinsic_groups(
    qvi_mpi_t *mpi
) {
    char const *ers = nullptr;
    int rc;


    qvi_mpi_group_t self_group;
    rc = MPI_Comm_group(
        MPI_COMM_SELF,
        &self_group.mpi_group
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_group(MPI_COMM_SELF) failed";
        goto out;
    }
    qvi_mpi_group_t node_group;
    rc = MPI_Comm_group(
        mpi->node_comm,
        &node_group.mpi_group
    );
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_group(node_comm) failed";
        goto out;
    }
    mpi->group_tab[QVI_MPI_GROUP_SELF] = self_group;
    mpi->group_tab[QVI_MPI_GROUP_NODE] = node_group;
out:
    if (ers) {
        qvi_log_error(ers);
    }
    return rc;
}

int
qvi_mpi_init(
    qvi_mpi_t *mpi,
    MPI_Comm comm
) {
    if (!mpi) return QV_ERR_INVLD_ARG;

    char const *ers = nullptr;
    int inited, rc;
    // If MPI isn't initialized, then we can't continue.
    rc = MPI_Initialized(&inited);
    if (rc != MPI_SUCCESS) return QV_ERR_MPI;
    if (!inited) {
        ers = "MPI is not initialized. Cannot continue.";
        qvi_log_error(ers);
        return QV_ERR_MPI;
    }
    // MPI is initialized.
    rc = MPI_Comm_size(comm, &mpi->world_size);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size(MPI_COMM_WORLD) failed";
        goto out;
    }
    rc = MPI_Comm_rank(comm, &mpi->world_id);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank(MPI_COMM_WORLD) failed";
        goto out;
    }
    rc = create_intrinsic_comms(mpi, comm);
    if (rc != MPI_SUCCESS) {
        ers = "create_intrinsic_comms() failed";
        goto out;
    }
    rc = MPI_Comm_size(mpi->node_comm, &mpi->node_size);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_size(node_comm) failed";
        goto out;
    }
    rc = MPI_Comm_rank(mpi->node_comm, &mpi->node_id);
    if (rc != MPI_SUCCESS) {
        ers = "MPI_Comm_rank(node_comm) failed";
        goto out;
    }
    rc = create_intrinsic_groups(mpi);
    if (rc != MPI_SUCCESS) {
        ers = "create_intrinsic_groups() failed";
        goto out;
    }
out:
    if (ers) {
        qvi_log_error("{} with rc={}", ers, rc);
        return QV_ERR_MPI;
    }
    return QV_SUCCESS;
}

int
qvi_mpi_finalize(
    qvi_mpi_t *mpi
) {
    QVI_UNUSED(mpi);
    return QV_SUCCESS;
}

int
qvi_mpi_node_id(
    qvi_mpi_t *mpi
) {
    return mpi->node_id;
}

int
qvi_mpi_world_id(
    qvi_mpi_t *mpi
) {
    return mpi->world_id;
}

int
qvi_mpi_node_size(
    qvi_mpi_t *mpi
) {
    return mpi->node_size;
}

int
qvi_mpi_world_size(
    qvi_mpi_t *mpi
) {
    return mpi->world_size;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
