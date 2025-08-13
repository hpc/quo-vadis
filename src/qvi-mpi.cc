/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
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

using qvi_mpi_group_tab = std::unordered_map<
    qvi_group_id_t, qvi_mpi_group_t
>;

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
    if (qvi_unlikely(rc != MPI_SUCCESS)) return QV_ERR_MPI;

    int done = 0;
    do {
        rc = MPI_Test(&request, &done, MPI_STATUS_IGNORE);
        if (qvi_unlikely(rc != MPI_SUCCESS)) return QV_ERR_MPI;
        std::this_thread::sleep_for(std::chrono::microseconds(50000));
    } while (!done);

    return QV_SUCCESS;
}

struct qvi_mpi_comm {
    /** Underlying MPI communicator. */
    MPI_Comm mpi_comm = MPI_COMM_NULL;
    /** Communicator size. */
    int size = 0;
    /** Communicator rank. */
    int rank = 0;
    /** Constructor. */
    qvi_mpi_comm(void) = default;
    /** Constructor. */
    qvi_mpi_comm(
        MPI_Comm comm,
        bool dup
    ) {
        int rc;
        if (dup) {
            rc = MPI_Comm_dup(comm, &mpi_comm);
            if (qvi_unlikely(rc != MPI_SUCCESS)) {
                throw qvi_runtime_error(QV_ERR_MPI);
            }
        }
        else {
            mpi_comm = comm;
        }
        rc = MPI_Comm_size(mpi_comm, &size);
        if (qvi_unlikely(rc != MPI_SUCCESS)) {
            throw qvi_runtime_error(QV_ERR_MPI);
        }
        rc = MPI_Comm_rank(mpi_comm, &rank);
        if (qvi_unlikely(rc != MPI_SUCCESS)) {
            throw qvi_runtime_error(QV_ERR_MPI);
        }
    }
    /** Destructor. */
    ~qvi_mpi_comm(void) = default;
    /** Frees the resources associated with the provided instance. */
    static void
    free(
        qvi_mpi_comm &comm
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
    qvi_mpi_comm qvcomm = {};
    /** Constructor. */
    qvi_mpi_group_s(void) = default;
    /** Constructor. */
    qvi_mpi_group_s(
        const qvi_mpi_comm &comm
    ) : qvcomm(comm) { }
};

struct qvi_mpi_s {
    /** Node representative communicator. Only valid for elected processes. */
    qvi_mpi_comm node_rep_comm;
    /** Duplicate of MPI_COMM_SELF. */
    qvi_mpi_comm self_comm;
    /** Node communicator. */
    qvi_mpi_comm node_comm;
    /** Duplicate of initializing communicator. */
    qvi_mpi_comm world_comm;
    /** Group table (ID to internal structure mapping). */
    qvi_mpi_group_tab group_tab;
    /** Constructor. */
    qvi_mpi_s(void) = default;
    /** Destructor. */
    ~qvi_mpi_s(void)
    {
        for (auto &i : group_tab) {
            qvi_mpi_comm::free(i.second.qvcomm);
        }
    }
    /** Adds a new group to the group table. */
    int
    add_group(
        const qvi_mpi_group_s &group,
        qvi_group_id_t given_id = QVI_MPI_GROUP_NULL
    ) {
        qvi_group_id_t gid = given_id;
        // Marker used to differentiate between intrinsic and automatic IDs.
        if (given_id == QVI_MPI_GROUP_NULL) {
            const int rc = qvi_group::next_id(&gid);
            if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
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
    if (qvi_unlikely(rc != MPI_SUCCESS)) {
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
    group.qvcomm = qvi_mpi_comm(comm, false);
    return QV_SUCCESS;
}

int
qvi_mpi_group_comm_dup(
    qvi_mpi_group_t *group,
    MPI_Comm *comm
) {
    const int rc = MPI_Comm_dup(group->qvcomm.mpi_comm, comm);
    if (qvi_unlikely(rc != MPI_SUCCESS)) return QV_ERR_MPI;
    return QV_SUCCESS;
}

/**
 * Creates intrinsic communicators.
 */
static int
create_intrinsic_comms(
    qvi_mpi_t *mpi,
    MPI_Comm comm
) {
    // Create node communicator.
    MPI_Comm node_comm = MPI_COMM_NULL;
    const int rc = mpi_comm_to_new_node_comm(comm, &node_comm);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
    // MPI_COMM_SELF duplicate.
    mpi->self_comm = qvi_mpi_comm(MPI_COMM_SELF, true);
    // Node communicator, no duplicate necessary here: created above.
    mpi->node_comm = qvi_mpi_comm(node_comm, false);
    // 'World' (aka initializing communicator) duplicate.
    mpi->world_comm = qvi_mpi_comm(comm, true);
    return rc;
}

/**
 * Creates internal, administrative communicators.
 */
static int
create_admin_comms(
    qvi_mpi_t *mpi
) {
    assert(mpi->node_comm.mpi_comm != MPI_COMM_NULL);
    // Create a communicator that has node rank zero members in it. We will call
    // those processes 'node representatives.'
    const int color = (mpi->node_comm.rank == 0) ? 0 : MPI_UNDEFINED;
    MPI_Comm node_rep_comm;
    int mpirc = MPI_Comm_split(
        mpi->node_comm.mpi_comm, color,
        mpi->world_comm.rank, &node_rep_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;
    // Part of the node representatives communicator.
    if (node_rep_comm != MPI_COMM_NULL) {
        mpi->node_rep_comm = qvi_mpi_comm(node_rep_comm, false);
        // Add the comm to the group.
        return mpi->add_group(qvi_mpi_group_t(mpi->node_rep_comm));
    }
    return QV_SUCCESS;
}

static int
create_intrinsic_groups(
    qvi_mpi_t *mpi
) {
    int rc = mpi->add_group(
        qvi_mpi_group_t(mpi->self_comm), QVI_MPI_GROUP_SELF
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = mpi->add_group(
        qvi_mpi_group_t(mpi->node_comm), QVI_MPI_GROUP_NODE
    );
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return mpi->add_group(
        qvi_mpi_group_t(mpi->world_comm), QVI_MPI_GROUP_WORLD
    );
}

static int
get_portno_from_env(
    const qvi_mpi_comm &comm,
    int &portno
) {
    assert(comm.mpi_comm != MPI_COMM_NULL);

    int rc = QV_SUCCESS;
    do {
        MPI_Comm rep_comm = comm.mpi_comm;
        // Is QV_PORT set on this node, in my environment?
        const bool envset = qvi_envset(QVI_ENV_PORT);
        // See if any processes have QV_PORT set.
        bool anyset = 0;
        // See if QV_PORT is set on any of the nodes, while counting them.
        int mpirc = MPI_Allreduce(
            &envset, &anyset, 1, MPI_CXX_BOOL, MPI_LOR, rep_comm
        );
        if (qvi_unlikely(mpirc != MPI_SUCCESS)) {
            rc = QV_ERR_MPI;
            break;
        }
        if (anyset) {
            int envport = QVI_PORT_UNSET;
            // If this fails, envport is set to QVI_COMM_PORT_UNSET.
            (void)qvi_port_from_env(envport);

            std::vector<int> portnos(comm.size);
            mpirc = MPI_Allgather(
                &envport, 1, MPI_INT,
                portnos.data(),
                1, MPI_INT, rep_comm
            );
            if (qvi_unlikely(mpirc != MPI_SUCCESS)) {
                rc = QV_ERR_MPI;
                break;
            }
            // Pick one from the group.
            for (const auto pn : portnos) {
                if (pn == QVI_PORT_UNSET) continue;
                portno = pn;
            }
        }
    } while (false);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        portno = QVI_PORT_UNSET;
    }
    return rc;
}

static int
start_daemons(
    qvi_mpi_t *mpi
) {
    int rc = QV_SUCCESS, ret = QV_SUCCESS;
    do {
        // Default port.
        int portno = QVI_PORT_UNSET;
        // Not a node representative, so wait in barrier below.
        if (mpi->node_rep_comm.mpi_comm == MPI_COMM_NULL) break;
        // The rest do all the work.
        rc = get_portno_from_env(mpi->node_rep_comm, portno);
        if (qvi_unlikely(rc != QV_SUCCESS)) break;
        // Try to use the requested port.
        if (portno != QVI_PORT_UNSET) {
            // If there is a session directory using the given port, use it.
            if (qvi_session_exists(portno)) break;
            // Else start up the daemon using the requested port.
            rc = qvi_start_qvd(portno);
            // Done!
            break;
        }
        // No port requested, so try to discover one.
        rc = qvi_session_discover(10, portno);
        // Found one, so use it.
        if (rc == QV_SUCCESS) break;
        // Else, start up the daemon using our default port number.
        rc = qvi_start_qvd(57550);
    } while (false);
    // See if any errors happened above, but all barrier to avoid hangs.
    if (qvi_unlikely(rc != QV_SUCCESS)) ret = rc;
    rc = sleepy_node_barrier(mpi->node_comm.mpi_comm);
    if (qvi_unlikely(rc != QV_SUCCESS)) ret = rc;
    return ret;
}

int
qvi_mpi_init(
    qvi_mpi_t *mpi,
    MPI_Comm comm
) {
    // If MPI isn't initialized, then we can't continue.
    int inited = 0;
    const int mpirc = MPI_Initialized(&inited);
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;
    if (qvi_unlikely(!inited)) {
        const cstr_t ers = "MPI is not initialized. Cannot continue.";
        qvi_log_error(ers);
        return QV_ERR_MPI;
    }

    int rc = create_intrinsic_comms(mpi, comm);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = create_intrinsic_groups(mpi);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    rc = create_admin_comms(mpi);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    return start_daemons(mpi);
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

std::vector<pid_t>
qvi_mpi_group_pids(
    const qvi_mpi_group_t *group
) {
    static_assert(
        sizeof(int) == sizeof(pid_t),
        "int and pid_t must have the same size."
    );

    const pid_t mypid = getpid();
    std::vector<pid_t> pids(group->qvcomm.size);

    const int mpirc = MPI_Allgather(
        &mypid, 1, MPI_INT, pids.data(),
        1, MPI_INT, group->qvcomm.mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) {
        throw qvi_runtime_error(QV_ERR_MPI);
    }
    return pids;
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
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;

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

int
qvi_mpi_group_barrier(
    qvi_mpi_group_t *group
) {
    return sleepy_node_barrier(group->qvcomm.mpi_comm);
}

int
qvi_mpi_group_gather_bbuffs(
    qvi_mpi_group_t *group,
    const qvi_bbuff &txbuff,
    int root,
    std::vector<qvi_bbuff> &rxbuffs
) {
    const int send_count = (int)txbuff.size();
    const int group_id = group->qvcomm.rank;
    const int group_size = group->qvcomm.size;

    int rc = QV_SUCCESS, mpirc = MPI_SUCCESS;
    std::vector<int> rxcounts, displs;
    std::vector<byte_t> allbytes;

    if (group_id == root) {
        rxcounts.resize(group_size);
    }
    // Figure out now much data are sent by each participant.
    mpirc = MPI_Gather(
        &send_count, 1, MPI_INT,
        rxcounts.data(), 1, MPI_INT,
        root, group->qvcomm.mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;
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
        txbuff.cdata(), send_count, MPI_UINT8_T,
        allbytes.data(), rxcounts.data(), displs.data(), MPI_UINT8_T,
        root, group->qvcomm.mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;
    // Root creates new buffers from data gathered from each participant.
    if (group_id == root) {
        rxbuffs.resize(group_size);
        byte_t *bytepos = allbytes.data();
        for (int i = 0; i < group_size; ++i) {
            rc = rxbuffs[i].append(bytepos, rxcounts[i]);
            if (rc != QV_SUCCESS) return rc;
            bytepos += rxcounts[i];
        }
    }
    return QV_SUCCESS;
}

int
qvi_mpi_group_scatter_bbuffs(
    qvi_mpi_group_t *group,
    const std::vector<qvi_bbuff> &txbuffs,
    int root,
    qvi_bbuff &rxbuff
) {
    const int group_size = group->qvcomm.size;
    const int group_id = group->qvcomm.rank;

    int mpirc = MPI_SUCCESS, rxcount = 0;
    int total_bytes = 0;
    std::vector<int> txcounts, displs;
    std::vector<byte_t> mybytes, txbytes;
    // Root sets up relevant Scatterv data structures.
    if (group_id == root) {
        txcounts.resize(group_size);
        displs.resize(group_size);

        for (int i = 0; i < group_size; ++i) {
            txcounts[i] = (int)txbuffs[i].size();
            displs[i] = total_bytes;
            total_bytes += txcounts[i];
        }
        // A flattened buffer containing all the relevant information.
        txbytes.resize(total_bytes);
        // Copy buffer data into flattened buffer.
        byte_t *bytepos = txbytes.data();
        for (int i = 0; i < group_size; ++i) {
            memmove(bytepos, txbuffs[i].cdata(), txcounts[i]);
            bytepos += txcounts[i];
        }
    }
    // Scatter buffer sizes.
    mpirc = MPI_Scatter(
        txcounts.data(), 1, MPI_INT,
        &rxcount, 1, MPI_INT,
        root, group->qvcomm.mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;
    // Everyone allocates a buffer for their data.
    mybytes.resize(rxcount);

    mpirc = MPI_Scatterv(
        txbytes.data(), txcounts.data(), displs.data(), MPI_UINT8_T,
        mybytes.data(), rxcount, MPI_UINT8_T, root, group->qvcomm.mpi_comm
    );
    if (qvi_unlikely(mpirc != MPI_SUCCESS)) return QV_ERR_MPI;

    return rxbuff.append(mybytes.data(), rxcount);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
