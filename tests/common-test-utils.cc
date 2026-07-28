/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file common-test-utils.cc
 */

#include "common-test-utils.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

static std::string
vfstring(
    const char *format,
    va_list args
) {
    // Determine required buffer size.
    va_list args_copy;
    va_copy(args_copy, args);
    const int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);
    // An error?
    if (size < 0) {
        return {};
    }
    // Format the message.
    std::vector<char> buffer(size + 1);
    vsnprintf(buffer.data(), buffer.size(), format, args);
    return std::string(buffer.data());
}

static std::string
fstring(
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);
    const auto result = vfstring(format, args);
    va_end(args);
    return result;
}

// Base logger.
struct logger {
private:
    std::mutex m_mutex;
    logger(void) = default;
    ~logger(void) = default;

public:
    static logger &
    the_logger(void) {
        static logger singleton;
        return singleton;
    }
    //Disable copy constructor.
    logger(const logger &) = delete;
    // Just return the singleton.
    logger &
    operator=(
        const logger &
    ) {
        // Just return the singleton.
        return logger::the_logger();
    }
    // Main logging function.
    void
    log(
        const std::string &message
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Determine if this process has a message to log.
        if (!message.empty()) {
            printf("%s", message.c_str());
            fflush(stdout);
        }
    }
    //
    void
    vlogf(
        const char *format,
        va_list args
    ) {
        log(vfstring(format, args));
    }
};

void
ctu_logf(
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);
    logger::the_logger().vlogf(format, args);
    va_end(args);
}

struct ologger {
protected:
    std::string m_id;

public:
    ologger(void) : m_id(std::to_string(getpid())) { }

    virtual ~ologger(void) = default;

    virtual void plog(
        bool pred,
        const std::string &msg
    ) {
        if (pred) logger::the_logger().log(msg);
    }

    void plogf(
        bool pred,
        const char *format,
        ...
    ) {
        va_list args;
        va_start(args, format);
        plog(pred, vfstring(format, args));
        va_end(args);
    }

    virtual std::string
    id(void) {
        return m_id;
    }
};

struct thr_ologger : ologger {
public:
    thr_ologger(void)
    {
        m_id = std::to_string(getpid()) + "," + std::to_string(ctu_gettid());
    }
};

#ifdef CTU_HAS_MPI_SUPPORT
#include "quo-vadis-mpi.h"

struct mpi_ologger : ologger {
private:
    MPI_Comm m_comm = MPI_COMM_NULL;
    int m_rank = -1;
    int m_size = -1;

public:
    mpi_ologger(
        qv_scope_t *scope
    ) {
        int rc = MPI_SUCCESS;
        const char *ers = NULL;
        do {
            rc = qv_mpi_comm_dup(scope, &m_comm);
            if (rc != QV_SUCCESS) {
                ers = "qv_mpi_comm_dup";
                rc = MPI_ERR_COMM;
                break;
            }

            if (m_comm != MPI_COMM_NULL) {
                rc = MPI_Comm_rank(MPI_COMM_WORLD, &m_rank);
                if (rc != MPI_SUCCESS) {
                    ers = "MPI_Comm_rank";
                    break;
                }

                rc = MPI_Comm_size(m_comm, &m_size);
                if (rc != MPI_SUCCESS) {
                    ers = "MPI_Comm_size";
                    break;
                }
            }
            m_id = std::to_string(m_rank);
        } while (0);

        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", ers, rc);
        }
    }

    virtual ~mpi_ologger(void)
    {
        if (m_comm != MPI_COMM_NULL) MPI_Comm_free(&m_comm);
    }

    void plog(
        bool pred,
        const std::string &msg
    ) override {
        int world_rank = -1, world_size = -1;
        int rc = MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", "MPI_Comm_rank", rc);
        }
        rc = MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", "MPI_Comm_size", rc);
        }
        // Determine if this process has a message to log.
        const bool have_message = !msg.empty();
        const int msg_len = static_cast<int>(msg.size());
        // Strategy: Always route through world rank 0, ordered by world rank.
        if (world_rank == 0) {
            // Collect world ranks participating in this communicator.
            std::vector<int> participating_ranks(world_size, 0);
            // Check if we're in the communicator.
            int in_comm = (m_comm != MPI_COMM_NULL) ? 1 : 0;
            // Gather participation info.
            rc = MPI_Allgather(
                &in_comm, 1, MPI_INT,
                participating_ranks.data(),
                1, MPI_INT, MPI_COMM_WORLD
            );
            if (rc != MPI_SUCCESS) {
                ctu_panic("%s failed (rc=%d)", "MPI_Allgather", rc);
            }
            // Print own message first if we have one.
            if (have_message) {
                logger::the_logger().log(msg);
            }
            // Receive messages from other world ranks in order.
            for (int src = 1; src < world_size; ++src) {
                if (participating_ranks[src]) {
                    // This rank is in the communicator
                    int recv_len = 0;
                    rc = MPI_Recv(
                        &recv_len, 1, MPI_INT, src, 0,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE
                    );
                    if (rc != MPI_SUCCESS) {
                        ctu_panic("%s failed (rc=%d)", "MPI_Recv", rc);
                    }
                    if (recv_len > 0) {
                        std::vector<char> buffer(recv_len + 1, '\0');
                        rc = MPI_Recv(
                            buffer.data(), recv_len, MPI_CHAR, src,
                            1, MPI_COMM_WORLD, MPI_STATUS_IGNORE
                        );
                        if (rc != MPI_SUCCESS) {
                            ctu_panic("%s failed (rc=%d)", "MPI_Recv", rc);
                        }
                        logger::the_logger().log(std::string(buffer.data()));
                    }
                }
            }
        }
        else {
            // Check if we're in the communicator and participating.
            const int in_comm = (m_comm != MPI_COMM_NULL && pred) ? 1 : 0;
            // Participate in gather.
            std::vector<int> participating_ranks(world_size, 0);
            rc = MPI_Allgather(
                &in_comm, 1, MPI_INT,
                participating_ranks.data(),
                1, MPI_INT, MPI_COMM_WORLD
            );
            if (rc != MPI_SUCCESS) {
                ctu_panic("%s failed (rc=%d)", "MPI_Allgather", rc);
            }
            // If we're in the communicator, send to world rank 0.
            if (in_comm) {
                rc = MPI_Send(&msg_len, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
                if (rc != MPI_SUCCESS) {
                    ctu_panic("%s failed (rc=%d)", "MPI_Send", rc);
                }
                if (msg_len > 0) {
                    rc = MPI_Send(
                        msg.c_str(), msg_len,
                        MPI_CHAR,0, 1, MPI_COMM_WORLD
                    );
                    if (rc != MPI_SUCCESS) {
                        ctu_panic("%s failed (rc=%d)", "MPI_Send", rc);
                    }
                }
            }
        }
        // Barrier on world communicator to ensure all
        // processes wait for logging to complete.
        rc = MPI_Barrier(MPI_COMM_WORLD);
        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", "MPI_Barrier", rc);
        }
    }
};

#endif // #ifdef CTU_HAS_MPI_SUPPORT

static std::pair<int, int>
ctu_scope_size_rank(
    qv_scope_t *scope
) {
    char const *ers = NULL;

    int sgsize;
    int rc = qv_group_size(scope, &sgsize);
    if (rc != QV_SUCCESS) {
        ers = "qv_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int sgrank;
    rc = qv_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    return {sgsize, sgrank};
}

static std::string
ctu_current_binding(
    qv_scope_t *scope
) {
    char const *ers = NULL;
    // Get current binding.
    char *cpusets;
    int rc = qv_bind_string(scope, QV_BIND_STRING_LOGICAL, &cpusets);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    std::string result(cpusets);
    free(cpusets);
    return result;
}

static std::string
ctu_scope_cpuset(
    qv_scope_t *scope
) {
    char const *ers = NULL;
    // Change binding to get the scope's underlying cpuset.
    int rc = qv_bind_push(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get the current binding after push.
    auto result = ctu_current_binding(scope);
    // Pop to not affect other calls related to the scope.
    rc = qv_bind_pop(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    return result;
}

std::unique_ptr<ologger>
ctu_reporter(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
) {
#ifndef CTU_HAS_MPI_SUPPORT
    (void)scope;
#endif
    switch (kind) {
        case CTU_SCOPE_KIND_PROCESS: return std::make_unique<ologger>();
        case CTU_SCOPE_KIND_THREAD:  return std::make_unique<thr_ologger>();
#ifdef CTU_HAS_MPI_SUPPORT
        case CTU_SCOPE_KIND_MPI:     return std::make_unique<mpi_ologger>(scope);
#endif
        default: ctu_panic("Unsupported reporter!");
    }
}

static void
ctu_pvemit(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    bool pred,
    const char *format,
    va_list args
) {
    auto reporter = ctu_reporter(scope, kind);
    reporter->plog(pred, vfstring(format, args));
}

void
ctu_emit(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);
    ctu_pvemit(scope, kind, true, format, args);
    va_end(args);
}

void
ctu_pemit(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    bool pred,
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);
    ctu_pvemit(scope, kind, pred, format, args);
    va_end(args);
}

void
ctu_emit_task_bind(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
) {
    auto reporter = ctu_reporter(scope, kind);
    const auto myid = reporter->id();
    auto binds = ctu_current_binding(scope);
    reporter->plogf(true, "[%s] cpubind=%s\n", myid.c_str(), binds.c_str());
}

void
ctu_emit_host_hw_info(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *scope_name
) {
    auto reporter = ctu_reporter(scope, kind);
    const auto myid = reporter->id();
    std::string myoutput;

    for (size_t i = 0; i < ctu_hw_obj_name_to_type_tab_size; ++i) {
        int n;
        int rc = qv_hw_obj_count(
            scope, ctu_hw_obj_name_to_type_tab[i].type, &n
        );
        if (rc != QV_SUCCESS) {
            ctu_panic(
                "qv_hw_obj_count(%s) failed\n",
                ctu_hw_obj_name_to_type_tab[i].name
            );
        }
        myoutput += fstring(
            "[%s] %s: %s: n = %d\n",
            myid.c_str(), scope_name,
            ctu_hw_obj_name_to_type_tab[i].name, n
        );
    }
    reporter->plog(true, myoutput);
}

void
ctu_emit_device_info(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    qv_hw_obj_type_t dev_type,
    const char *scope_name
) {
    auto reporter = ctu_reporter(scope, kind);
    const std::string myid = reporter->id();
    std::string myoutput;
    // Get number of devices.
    int ndevs;
    int rc = qv_hw_obj_count(scope, dev_type, &ndevs);
    if (rc != QV_SUCCESS) {
        const char *ers = "qv_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    std::string bind_report = {};

    if (ndevs > 0) {
        auto binds = ctu_scope_cpuset(scope);
        bind_report = " on " + binds;
    }

    myoutput += fstring(
        "[%s] %s: Discovered %d %s(s)%s\n",
        myid.c_str(), scope_name, ndevs,
        ctu_obj_name(dev_type), bind_report.c_str()
    );
    for (int i = 0; i < ndevs; ++i) {
        for (size_t j = 0; j < ctu_devid_name_to_id_tab_size; ++j) {
            char *devids = NULL;
            int rc = qv_device_id(
                scope,
                dev_type,
                i,
                ctu_devid_name_to_id_tab[j].devid,
                &devids
            );
            if (rc != QV_SUCCESS) {
                const char *ers = "qv_device_id() failed";
                ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
            }
            myoutput += fstring(
                "[%s] Device %d %s = %s\n",
                myid.c_str(), i, ctu_devid_name_to_id_tab[j].name, devids
            );
            free(devids);
        }
    }
    reporter->plog(true, myoutput);
}

void
ctu_emit_scope_report(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *const scope_name
) {
    auto reporter = ctu_reporter(scope, kind);
    const auto myid = reporter->id();
    const auto [sgsize, sgrank] = ctu_scope_size_rank(scope);

    auto binds = ctu_scope_cpuset(scope);
    reporter->plogf(
        true,
        "[%s] %s: hello from group rank %d of %d on %s\n",
        myid.c_str(), scope_name, sgrank, sgsize, binds.c_str()
    );
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
