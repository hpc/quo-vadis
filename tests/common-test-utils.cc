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

#include "quo-vadis/config.h"

#include <mutex>
#include <string>

// Helps with deterministic task output: a
// deferred output stream implementation.
struct dos {
private:
    std::mutex m_mutex;
    char *m_buff = nullptr;
    dos(void) = default;
    ~dos(void)
    {
        free(m_buff);
        m_buff = NULL;
    }

public:
    static dos &
    the_dos(void) {
        static dos singleton;
        return singleton;
    }
    //Disable copy constructor.
    dos(const dos &) = delete;
    // Just return the singleton.
    dos &
    operator=(
        const dos &
    ) {
        // Just return the singleton.
        return dos::the_dos();
    }
    //
    void
    vadd(
        const char *format,
        va_list args
    ) {
        std::scoped_lock lock(m_mutex);

        char *inbuff = NULL;
        int np = vasprintf(&inbuff, format, args);
        if (np == -1) ctu_panic("OOR");

        char *newbuff = NULL;
        np = asprintf(&newbuff, "%s%s", m_buff ? m_buff : "", inbuff);
        if (np == -1) ctu_panic("OOR");

        free(inbuff);
        free(m_buff);

        m_buff = newbuff;
    }

    void
    flush(void)
    {
        std::scoped_lock lock(m_mutex);

        if (m_buff) {
            printf("%s", m_buff);
            fflush(stdout);
        }
        free(m_buff);
        m_buff = nullptr;
    }
};

void
ctu_dprintf(
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);
    dos::the_dos().vadd(format, args);
    va_end(args);
}

void
ctu_dflush(void)
{
    dos::the_dos().flush();
}

struct odos {
protected:
    std::string m_id;

public:
    odos(void)
    {
        m_id = std::to_string(getpid());
    }

    ~odos(void)
    {
        ctu_dflush();
    }

    virtual void pvadd(
        bool pred,
        const char *format,
        va_list args
    ) {
        if (pred) dos::the_dos().vadd(format, args);
    }

    virtual void vadd(
        const char *format,
        va_list args
    ) {
        pvadd(true, format, args);
    }

    virtual void padd(
        bool pred,
        const char *format,
        ...
    ) {
        va_list args;
        va_start(args, format);
        pvadd(pred, format, args);
        va_end(args);
    }

    virtual void add(
        const char *format,
        ...
    ) {
        va_list args;
        va_start(args, format);
        vadd(format, args);
        va_end(args);
    }

    virtual std::string
    id(void) {
        return m_id;
    }
};

struct thr_odos : odos {
public:
    thr_odos(void)
    {
        m_id = std::to_string(getpid()) + "," + std::to_string(ctu_gettid());
    }
};

#ifdef MPI_FOUND

#include "quo-vadis-mpi.h"

struct mpi_odos : odos {
private:
    const int tok_tag = 575;
    MPI_Comm comm;
    int rank = -1;
    int size = -1;
    bool initialized = false;

    void
    wait_token(void) {
        char token = 'a';
        // Wait for token from previous rank.
        if (rank > 0) {
            const int rc = MPI_Recv(
                &token, 1, MPI_CHAR, rank - 1,
                tok_tag, comm, MPI_STATUS_IGNORE
            );
            if (rc != MPI_SUCCESS) {
                ctu_panic("%s failed (rc=%d)", "MPI_Recv", rc);
            }
        }
    }

    void
    release_token(void)
    {
        char token = 'a';
        // Send token to next rank.
        if (rank < size - 1) {
            const int rc = MPI_Send(
                &token, 1, MPI_CHAR, rank + 1, tok_tag, comm
            );
            if (rc != MPI_SUCCESS) {
                ctu_panic("%s failed (rc=%d)", "MPI_Send", rc);
            }
        }
    }

public:
    mpi_odos(
        qv_scope_t *scope
    ) {
        int rc = MPI_SUCCESS;
        const char *ers = NULL;
        do {
            initialized = false;

            rc = qv_mpi_scope_comm_dup(scope, &comm);
            if (rc != QV_SUCCESS) {
                ers = "qv_mpi_scope_comm_dup";
                rc = MPI_ERR_COMM;
                break;
            }

            rc = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
            if (rc != MPI_SUCCESS) {
                ers = "MPI_Comm_rank";
                break;
            }

            rc = MPI_Comm_size(comm, &size);
            if (rc != MPI_SUCCESS) {
                ers = "MPI_Comm_size";
                break;
            }

            m_id = std::to_string(rank);
            initialized = true;
        } while (0);

        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", ers, rc);
        }
    }

    ~mpi_odos(void) {
        int rc = MPI_SUCCESS;
        const char *ers = NULL;

        if (!initialized) return;
        // Ensure all pending operations are complete.
        do {
            rc = MPI_Barrier(comm);
            if (rc != MPI_SUCCESS) {
                ers = "MPI_Barrier";
                break;
            }

            rc = MPI_Comm_free(&comm);
            if (rc != MPI_SUCCESS) {
                ers = "MPI_Comm_free";
                break;
            }
            dos::the_dos().flush();
            initialized = false;
        } while (0);
        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", ers, rc);
        }
    }

    virtual void pvadd(
        bool pred,
        const char *format,
        va_list args
    ) override {
        // Wait for everyone to complete last operation.
        int rc = MPI_Barrier(comm);
        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", "MPI_Barrier", rc);
        }
        // Wait for my turn.
        wait_token();
        if (pred) dos::the_dos().vadd(format, args);
        // Pass it on to the next one.
        release_token();
        // Wait for everyone to complete print.
        rc = MPI_Barrier(comm);
        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", "MPI_Barrier", rc);
        }
    }

    void
    vadd(
        const char *format,
        va_list args
    ) override {
        pvadd(true, format, args);
    }

    virtual void padd(
        bool pred,
        const char *format,
        ...
    ) override {
        va_list args;
        va_start(args, format);
        pvadd(pred, format, args);
        va_end(args);
    }

    void
    add(
        const char *format,
        ...
    ) override {
        va_list args;
        va_start(args, format);
        vadd(format, args);
        va_end(args);
    }
};

#endif // #ifdef MPI_FOUND

odos
ctu_reporter(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
) {
    switch (kind) {
        case CTU_SCOPE_KIND_PROCESS: return odos();
        case CTU_SCOPE_KIND_THREAD:  return thr_odos();
#ifdef MPI_FOUND
        case CTU_SCOPE_KIND_MPI:     return mpi_odos(scope);
#endif
        default: ctu_panic("Unsupported reporter!");
    }
}

static void
ctu_vemit(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *format,
    va_list args
) {
    auto reporter = ctu_reporter(scope, kind);
    reporter.vadd(format, args);
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
    ctu_vemit(scope, kind, format, args);
    va_end(args);
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
    if (pred) reporter.vadd(format, args);
    else reporter.add("");
    // Sync across the scope.
    int rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        const char *ers = "qv_scope_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
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
    char const *ers = NULL;
    auto reporter = ctu_reporter(scope, kind);
    const std::string myid = reporter.id();
    // Get new, current binding.
    char *binds = NULL;
    int rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    reporter.add("[%s] cpubind=%s\n", myid.c_str(), binds);
    free(binds);
}

void
ctu_bind_push(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
) {
    char const *ers = NULL;
    auto reporter = ctu_reporter(scope, kind);
    const std::string myid = reporter.id();

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get current binding.
    char *bind0s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &bind0s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    reporter.add(
        "[%s] Current cpubind before qv_bind_push() is %s\n", myid.c_str(), bind0s
    );
    // Change binding.
    rc = qv_scope_bind_push(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    reporter.add(
        "[%s] New cpubind after qv_bind_push() is %s\n", myid.c_str(), bind1s
    );

    free(bind0s);
    free(bind1s);
}

void
ctu_bind_pop(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
) {
    char const *ers = NULL;
    auto reporter = ctu_reporter(scope, kind);
    const std::string myid = reporter.id();

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get current binding.
    char *bind0s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &bind0s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    reporter.add(
        "[%%s] Current cpubind before qv_bind_pop() is %s\n",
        myid.c_str(), bind0s
    );

    // Change binding.
    rc = qv_scope_bind_pop(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    reporter.add(
        "[%s] New cpubind after qv_bind_pop() is %s\n",
        myid.c_str(), bind1s
    );

    free(bind0s);
    free(bind1s);
}

void
ctu_emit_host_hw_info(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *scope_name
) {
    auto reporter = ctu_reporter(scope, kind);
    const std::string myid = reporter.id();

    for (size_t i = 0; i < ctu_hw_obj_name_to_type_tab_size; ++i) {
        int n;
        int rc = qv_scope_hw_obj_count(
            scope, ctu_hw_obj_name_to_type_tab[i].type, &n
        );
        if (rc != QV_SUCCESS) {
            ctu_panic(
                "qv_scope_hw_obj_count(%s) failed\n",
                ctu_hw_obj_name_to_type_tab[i].name
            );
        }
        reporter.add(
            "[%s] %s scope: %s: n = %d\n",
            myid.c_str(), scope_name,
            ctu_hw_obj_name_to_type_tab[i].name, n
        );
    }
}

void
ctu_emit_device_info(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    qv_hw_obj_type_t dev_type,
    const char *scope_name
) {
    auto reporter = ctu_reporter(scope, kind);
    const std::string myid = reporter.id();
    // Get number of devices.
    int ndevs;
    int rc = qv_scope_hw_obj_count(scope, dev_type, &ndevs);
    if (rc != QV_SUCCESS) {
        const char *ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    reporter.add(
        "[%s] Discovered %d %s(s) in %s\n",
        myid.c_str(), ndevs, ctu_obj_name(dev_type), scope_name
    );
    for (int i = 0; i < ndevs; ++i) {
        for (size_t j = 0; j < ctu_devid_name_to_id_tab_size; ++j) {
            char *devids = NULL;
            int rc = qv_scope_device_id_get(
                scope,
                dev_type,
                i,
                ctu_devid_name_to_id_tab[j].devid,
                &devids
            );
            if (rc != QV_SUCCESS) {
                const char *ers = "qv_scope_device_id_get() failed";
                ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
            }
            reporter.add(
                "[%s] Device %d %s = %s\n",
                myid.c_str(), i, ctu_devid_name_to_id_tab[j].name, devids
            );
            free(devids);
        }
    }
}

void
ctu_cpuset_report(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
) {
    char const *ers = NULL;
    auto reporter = ctu_reporter(scope, kind);
    const std::string myid = reporter.id();

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int sgsize;
    rc = qv_scope_group_size(scope, &sgsize);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Change binding to get the underlying cpuset.
    rc = qv_scope_bind_push(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    for (int i = 0; i < sgsize; ++i) {
        if (sgrank == i) {
            ctu_emit(
                scope, kind, "[%s] scope cpuset is %s\n", myid.c_str(), bind1s
            );
        }
        else {
            ctu_emit(
                scope, kind, ""
            );
        }
    }
    free(bind1s);

    rc = qv_scope_bind_pop(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

void
ctu_scope_report(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *const scope_name
) {
    char const *ers = NULL;
    auto reporter = ctu_reporter(scope, kind);
    const std::string myid = reporter.id();

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int sgsize;
    rc = qv_scope_group_size(scope, &sgsize);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    reporter.padd(
        sgrank == 0,
        "[%s] Scope report for %s\n",
        myid.c_str(), scope_name
    );
    reporter.padd(
        sgrank != 0, ""
    );

    for (int i = 0; i < sgsize; ++i) {
        reporter.padd(
            sgrank == i,
            "[%s] hello from group rank %d of %d\n",
            myid.c_str(), sgrank, sgsize
        );
        reporter.padd(
            sgrank != i, ""
        );
    }

    ctu_cpuset_report(scope, kind);

    rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

void
ctu_change_bind(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
) {
    char const *ers = NULL;
    auto reporter = ctu_reporter(scope, kind);
    const std::string myid = reporter.id();

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Get current binding.
    char *bind0s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &bind0s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    reporter.add("[%s] Current cpubind is %s\n", myid.c_str(), bind0s);

    // Change binding.
    rc = qv_scope_bind_push(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_push() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    // Get new, current binding.
    char *bind1s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &bind1s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    reporter.add("[%s] New cpubind is %s\n", myid.c_str(), bind1s);

    rc = qv_scope_bind_pop(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    char *bind2s;
    rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &bind2s);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    reporter.add("[%s] Popped cpubind is %s\n", myid.c_str(), bind2s);

    if (strcmp(bind0s, bind2s)) {
        ers = "bind push/pop mismatch";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    free(bind0s);
    free(bind1s);
    free(bind2s);

    rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
