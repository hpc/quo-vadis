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
 * @file qvi-mpi.cc
 */

#include "qvi-mpi.h"
#include "qvi-bbuff.h"
#include "qvi-utils.h"

using qvi_mpi_group_tab_t = std::unordered_map<
    qvi_group_id_t, qvi_mpi_group_t
>;

struct qvi_mpi_comm_s {
    /** Underlying MPI communicator. */
    MPI_Comm mpi_comm = MPI_COMM_NULL;
    /** Communicator size. */
    int size = 0;
    /** Communicator rank. */
    int rank = 0;
    /** Constructor. */
    qvi_mpi_comm_s(void) = default;
    /** Constructor. */
    qvi_mpi_comm_s(
        MPI_Comm comm,
        bool dup
    ) {
        int rc;
        if (dup) {
            rc = MPI_Comm_dup(comm, &mpi_comm);
            if (rc != MPI_SUCCESS) throw qvi_runtime_error();
        }
        else {
            mpi_comm = comm;
        }
        rc = MPI_Comm_size(mpi_comm, &size);
        if (rc != MPI_SUCCESS) throw qvi_runtime_error();
        rc = MPI_Comm_rank(mpi_comm, &rank);
        if (rc != MPI_SUCCESS) throw qvi_runtime_error();
    }
    /** Destructor. */
    ~qvi_mpi_comm_s(void) = default;
    /** Frees the resources associated with the provided instance. */
    static void
    free(
        qvi_mpi_comm_s &comm
    ) {
        MPI_Comm mpi_comm = comm.mpi_comm;
        if (mpi_comm != MPI_COMM_NULL) {
            MPI_Comm_free(&mpi_comm);
            comm.mpi_comm = MPI_COMM_NULL;
        }
    }
};

struct qvi_mpi_group_s {
    /** The group's communicator info. */
    qvi_mpi_comm_s qvcomm;
};

struct qvi_mpi_s {
    /** Duplicate of MPI_COMM_SELF. */
    qvi_mpi_comm_s self_comm;
    /** Node communicator. */
    qvi_mpi_comm_s node_comm;
    /** Duplicate of initializing communicator. */
    qvi_mpi_comm_s world_comm;
    /** Group table (ID to internal structure mapping). */
    qvi_mpi_group_tab_t group_tab;
    /** Constructor. */
    qvi_mpi_s(void) = default;
    /** Destructor. */
    ~qvi_mpi_s(void)
    {
        for (auto &i : group_tab) {
            qvi_mpi_comm_s::free(i.second.qvcomm);
        }
    }
    /** Adds a new group to the group table. */
    int
    add_group(
        qvi_mpi_group_s &group,
        qvi_group_id_t given_id = QVI_MPI_GROUP_NULL
    ) {
        qvi_group_id_t gid = given_id;
        // Marker used to differentiate between intrinsic and automatic IDs.
        if (given_id == QVI_MPI_GROUP_NULL) {
            const int rc = qvi_group_t::next_id(&gid);
            if (rc != QV_SUCCESS) return rc;
        }
        group_tab.insert({gid, group});
        return QV_SUCCESS;
    }
};

int
qvi_mpi_new(
    qvi_mpi_t **mpi
) {
    return qvi_new(mpi);
}

void
qvi_mpi_delete(
    qvi_mpi_t **mpi
) {
    qvi_delete(mpi);
}

/**
 * Creates 'node' communicators from an arbitrary MPI communicator.
 */
static int
mpi_comm_to_new_node_comm(
    MPI_Comm comm,
    MPI_Comm *node_comm
) {
    const int rc = MPI_Comm_split_type(
        comm, MPI_COMM_TYPE_SHARED,
        0, MPI_INFO_NULL, node_comm
    );
    if (rc != MPI_SUCCESS) {
        qvi_log_error("MPI_Comm_split_type(MPI_COMM_TYPE_SHARED) failed");
        *node_comm = MPI_COMM_NULL;
        return QV_ERR_MPI;
    }
    return QV_SUCCESS;
}

/**
 * Initializes a QV group from an MPI communicator.
 */
static int
group_init_from_mpi_comm(
    MPI_Comm comm,
    qvi_mpi_group_t &group
) {
    group.qvcomm = qvi_mpi_comm_s(comm, false);
    return QV_SUCCESS;
}

int
qvi_mpi_group_comm_dup(
    qvi_mpi_group_t *group,
    MPI_Comm *comm
) {
    const int rc = MPI_Comm_dup(group->qvcomm.mpi_comm, comm);
    if (rc != MPI_SUCCESS) return QV_ERR_MPI;
    return QV_SUCCESS;
}

/**
 * Creates the node communicator.
 */
static int
create_intrinsic_comms(
    qvi_mpi_t *mpi,
    MPI_Comm comm
) {
    // Create node communicator.
    MPI_Comm node_comm = MPI_COMM_NULL;
    const int rc = mpi_comm_to_new_node_comm(comm, &node_comm);
    if (rc != QV_SUCCESS) return rc;
    // MPI_COMM_SELF duplicate.
    mpi->self_comm = qvi_mpi_comm_s(MPI_COMM_SELF, true);
    // Node communicator, no duplicate necessary here: created above.
    mpi->node_comm = qvi_mpi_comm_s(node_comm, false);
    // 'World' (aka initializing communicator) duplicate.
    mpi->world_comm = qvi_mpi_comm_s(comm, true);
    return rc;
}

static int
create_intrinsic_groups(
    qvi_mpi_t *mpi
) {
    qvi_mpi_group_t self_group, node_group, wrld_group;
    self_group.qvcomm = mpi->self_comm;
    node_group.qvcomm = mpi->node_comm;
    wrld_group.qvcomm = mpi->world_comm;

    int rc = mpi->add_group(self_group, QVI_MPI_GROUP_SELF);
    if (rc != QV_SUCCESS) return rc;

    rc = mpi->add_group(node_group, QVI_MPI_GROUP_NODE);
    if (rc != QV_SUCCESS) return rc;

    return mpi->add_group(wrld_group, QVI_MPI_GROUP_WORLD);
}

int
qvi_mpi_init(
    qvi_mpi_t *mpi,
    MPI_Comm comm
) {
    // If MPI isn't initialized, then we can't continue.
    int inited = 0;
    const int mpirc = MPI_Initialized(&inited);
    if (mpirc != MPI_SUCCESS) return QV_ERR_MPI;
    if (!inited) {
        const cstr_t ers = "MPI is not initialized. Cannot continue.";
        qvi_log_error(ers);
        return QV_ERR_MPI;
    }

    const int rc = create_intrinsic_comms(mpi, comm);
    if (rc != QV_SUCCESS) return rc;

    return create_intrinsic_groups(mpi);
}

int
qvi_mpi_group_new(
    qvi_mpi_group_t **group
) {
    return qvi_new(group);
}

void
qvi_mpi_group_delete(
    qvi_mpi_group_t **group
) {
    qvi_delete(group);
}

int
qvi_mpi_group_size(
    const qvi_mpi_group_t *group
) {
    return group->qvcomm.size;
}

int
qvi_mpi_group_rank(
    const qvi_mpi_group_t *group
) {
    return group->qvcomm.rank;
}

int
qvi_mpi_group_lookup_by_id(
    qvi_mpi_t *mpi,
    qvi_group_id_t id,
    qvi_mpi_group_t &group
) {
    auto got = mpi->group_tab.find(id);
    if (got == mpi->group_tab.end()) {
        return QV_ERR_NOT_FOUND;
    }
    group = got->second;
    return QV_SUCCESS;
}

int
qvi_mpi_group_create_from_group_id(
    qvi_mpi_t *mpi,
    qvi_group_id_t id,
    qvi_mpi_group_t **group
) {
    qvi_mpi_group_s tmp_group;
    int rc = qvi_mpi_group_lookup_by_id(mpi, id, tmp_group);
    if (rc != QV_SUCCESS) return rc;

    return qvi_mpi_group_create_from_mpi_comm(
        mpi, tmp_group.qvcomm.mpi_comm, group
    );
}

int
qvi_mpi_group_create_from_split(
    qvi_mpi_t *mpi,
    const qvi_mpi_group_t *parent,
    int color,
    int key,
    qvi_mpi_group_t **child
) {
    MPI_Comm split_comm = MPI_COMM_NULL;
    const int mpirc = MPI_Comm_split(
        parent->qvcomm.mpi_comm, color, key, &split_comm
    );
    if (mpirc != MPI_SUCCESS) return QV_ERR_MPI;

    const int rc = qvi_mpi_group_create_from_mpi_comm(
        mpi, split_comm, child
    );
    if (split_comm != MPI_COMM_NULL) {
        MPI_Comm_free(&split_comm);
    }
    return rc;
}

int
qvi_mpi_group_create_from_mpi_comm(
    qvi_mpi_t *mpi,
    MPI_Comm comm,
    qvi_mpi_group_t **new_group
) {
    int rc = qvi_mpi_group_new(new_group);
    if (rc != QV_SUCCESS) return rc;

    MPI_Comm node_comm = MPI_COMM_NULL;
    rc = mpi_comm_to_new_node_comm(comm, &node_comm);
    if (rc != QV_SUCCESS) goto out;

    rc = group_init_from_mpi_comm(
        node_comm, **new_group
    );
    if (rc != QV_SUCCESS) goto out;

    rc = mpi->add_group(**new_group);
out:
    if (rc != QV_SUCCESS) {
        qvi_mpi_group_delete(new_group);
        if (node_comm != MPI_COMM_NULL) {
            MPI_Comm_free(&node_comm);
        }
    }
    return rc;
}

/**
 * Performs a low-noise, high latency node-level barrier across the given
 * communicator.
 */
static int
sleepy_node_barrier(
    MPI_Comm node_comm
) {
    MPI_Request request;
    int rc = MPI_Ibarrier(node_comm, &request);
    if (rc != MPI_SUCCESS) return QV_ERR_MPI;

    int done = 0;
    do {
        rc = MPI_Test(&request, &done, MPI_STATUS_IGNORE);
        if (rc != MPI_SUCCESS) return QV_ERR_MPI;
        usleep(50000);
    } while (!done);

    return QV_SUCCESS;
}

int
qvi_mpi_group_barrier(
    qvi_mpi_group_t *group
) {
    return sleepy_node_barrier(group->qvcomm.mpi_comm);
}

int
qvi_mpi_group_gather_bbuffs(
    qvi_mpi_group_t *group,
    qvi_bbuff_t *txbuff,
    int root,
    bool *shared_alloc,
    qvi_bbuff_t ***rxbuffs
) {
    const int send_count = (int)txbuff->size();
    const int group_id = group->qvcomm.rank;
    const int group_size = group->qvcomm.size;

    int rc = QV_SUCCESS, mpirc = MPI_SUCCESS;
    std::vector<int> rxcounts, displs;
    std::vector<byte_t> allbytes;
    qvi_bbuff_t **bbuffs = nullptr;

    if (group_id == root) {
        rxcounts.resize(group_size);
    }
    // Figure out now much data are sent by each participant.
    mpirc = MPI_Gather(
        &send_count, 1, MPI_INT,
        rxcounts.data(), 1, MPI_INT,
        root, group->qvcomm.mpi_comm
    );
    if (mpirc != MPI_SUCCESS) {
        rc = QV_ERR_MPI;
        goto out;
    }
    // Root sets up relevant Gatherv data structures.
    if (group_id == root) {
        displs.resize(group_size);

        int total_bytes = 0;
        for (int i = 0; i < group_size; ++i) {
            displs[i] = total_bytes;
            total_bytes += rxcounts[i];
        }
        allbytes.resize(total_bytes);
    }

    mpirc = MPI_Gatherv(
        txbuff->data(), send_count, MPI_UINT8_T,
        allbytes.data(), rxcounts.data(), displs.data(), MPI_UINT8_T,
        root, group->qvcomm.mpi_comm
    );
    if (mpirc != MPI_SUCCESS) {
        rc = QV_ERR_MPI;
        goto out;
    }
    // Root creates new buffers from data gathered from each participant.
    if (group_id == root) {
        // Zero initialize array of pointers to nullptr.
        bbuffs = new qvi_bbuff_t *[group_size]();
        // TODO(skg) Use dup.
        byte_t *bytepos = allbytes.data();
        for (int i = 0; i < group_size; ++i) {
            rc = qvi_bbuff_new(&bbuffs[i]);
            if (rc != QV_SUCCESS) break;
            rc = bbuffs[i]->append(bytepos, rxcounts[i]);
            if (rc != QV_SUCCESS) break;
            bytepos += rxcounts[i];
        }
    }
out:
    if (rc != QV_SUCCESS) {
        if (bbuffs) {
            for (int i = 0; i < group_size; ++i) {
                qvi_bbuff_delete(&bbuffs[i]);
            }
            delete[] bbuffs;
        }
        bbuffs = nullptr;
    }
    *rxbuffs = bbuffs;
    *shared_alloc = false;
    return rc;
}

int
qvi_mpi_group_scatter_bbuffs(
    qvi_mpi_group_t *group,
    qvi_bbuff_t **txbuffs,
    int root,
    qvi_bbuff_t **rxbuff
) {
    const int group_size = group->qvcomm.size;
    const int group_id = group->qvcomm.rank;

    int rc = QV_SUCCESS, mpirc = MPI_SUCCESS, rxcount = 0;
    int total_bytes = 0;
    std::vector<int> txcounts, displs;
    std::vector<byte_t> mybytes, txbytes;
    qvi_bbuff_t *mybbuff = nullptr;
    // Root sets up relevant Scatterv data structures.
    if (group_id == root) {
        txcounts.resize(group_size);
        displs.resize(group_size);

        for (int i = 0; i < group_size; ++i) {
            txcounts[i] = (int)txbuffs[i]->size();
            displs[i] = total_bytes;
            total_bytes += txcounts[i];
        }
        // A flattened buffer containing all the relevant information.
        txbytes.resize(total_bytes);
        // Copy buffer data into flattened buffer.
        byte_t *bytepos = txbytes.data();
        for (int i = 0; i < group_size; ++i) {
            memmove(bytepos, txbuffs[i]->data(), txcounts[i]);
            bytepos += txcounts[i];
        }
    }
    // Scatter buffer sizes.
    mpirc = MPI_Scatter(
        txcounts.data(), 1, MPI_INT,
        &rxcount, 1, MPI_INT,
        root, group->qvcomm.mpi_comm
    );
    if (mpirc != MPI_SUCCESS) {
        rc = QV_ERR_MPI;
        goto out;
    }
    // Everyone allocates a buffer for their data.
    mybytes.resize(rxcount);

    mpirc = MPI_Scatterv(
        txbytes.data(), txcounts.data(), displs.data(), MPI_UINT8_T,
        mybytes.data(), rxcount, MPI_UINT8_T, root, group->qvcomm.mpi_comm
    );
    if (mpirc != MPI_SUCCESS) {
        rc = QV_ERR_MPI;
        goto out;
    }
    // Everyone creates new buffers from data received from root.
    rc = qvi_bbuff_new(&mybbuff);
    if (rc != QV_SUCCESS) goto out;
    rc = mybbuff->append(mybytes.data(), rxcount);
out:
    if (rc != QV_SUCCESS) {
        qvi_bbuff_delete(&mybbuff);
    }
    *rxbuff = mybbuff;
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
