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
#ifdef MPI_FOUND
#include "quo-vadis-mpi.h"
#endif

#include <mutex>

// Helps with deterministic task output: a
// deferred output stream implementation.
struct dos {
private:
    std::mutex m_mutex;
    char *m_buff = nullptr;
    dos(void) = default;
    ~dos(void) {
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

        printf("%s", m_buff);
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

#ifdef MPI_FOUND

typedef struct {
    MPI_Comm comm;
    int rank;
    int size;
    int tok_tag;
    bool initialized;
} ctu_mpi_ordered_printer_t;

static inline void
ctu_mpi_ordered_printer_init(
    ctu_scope_reporter_t *reporter,
    qv_scope_t *scope
) {
    int rc = MPI_SUCCESS;
    const char *ers = NULL;

    ctu_mpi_ordered_printer_t *printer = (ctu_mpi_ordered_printer_t *)reporter->printer;

    do {
        printer->initialized = false;
        printer->tok_tag = 575;

        rc = qv_mpi_scope_comm_dup(scope, &printer->comm);
        if (rc != QV_SUCCESS) {
            ers = "qv_mpi_scope_comm_dup";
            rc = MPI_ERR_COMM;
            break;
        }

        rc = MPI_Comm_rank(printer->comm, &printer->rank);
        if (rc != MPI_SUCCESS) {
            ers = "MPI_Comm_rank";
            break;
        }

        rc = MPI_Comm_size(printer->comm, &printer->size);
        if (rc != MPI_SUCCESS) {
            ers = "MPI_Comm_size";
            break;
        }

        printer->initialized = true;
    } while (0);

    if (rc != MPI_SUCCESS) {
        ctu_panic("%s failed (rc=%d)", ers, rc);
    }
}

static inline int
ctu_mpi_ordered_printer_id(
    ctu_scope_reporter_t *reporter
) {
    ctu_mpi_ordered_printer_t *printer = (ctu_mpi_ordered_printer_t *)reporter->printer;
    return printer->rank;
}

static inline void
ctu_mpi_ordered_printer_fini(
    ctu_scope_reporter_t *reporter
) {
    int rc = MPI_SUCCESS;
    const char *ers = NULL;

    ctu_mpi_ordered_printer_t *printer = (ctu_mpi_ordered_printer_t *)reporter->printer;

    if (!printer || !printer->initialized) return;
    // Ensure all pending operations are complete.
    do {
        rc = MPI_Barrier(printer->comm);
        if (rc != MPI_SUCCESS) {
            ers = "MPI_Barrier";
            break;
        }

        rc = MPI_Comm_free(&printer->comm);
        if (rc != MPI_SUCCESS) {
            ers = "MPI_Comm_free";
            break;
        }
        printer->initialized = false;
        free(printer);
    } while (0);
    if (rc != MPI_SUCCESS) {
        ctu_panic("%s failed (rc=%d)", ers, rc);
    }
}

static inline void
ctu_mpi_wait_token(
    ctu_scope_reporter_t *reporter
) {
    ctu_mpi_ordered_printer_t *printer = (ctu_mpi_ordered_printer_t *)reporter->printer;

    char token = 'a';
    // Wait for token from previous rank.
    if (printer->rank > 0) {
        const int rc = MPI_Recv(
            &token, 1, MPI_CHAR, printer->rank - 1,
            printer->tok_tag, printer->comm, MPI_STATUS_IGNORE
        );
        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", "MPI_Recv", rc);
        }
    }
}

static inline void
ctu_mpi_release_token(
    ctu_scope_reporter_t *reporter
) {
    ctu_mpi_ordered_printer_t *printer = (ctu_mpi_ordered_printer_t *)reporter->printer;

    char token = 'a';
    // Send token to next rank.
    if (printer->rank < printer->size - 1) {
        const int rc = MPI_Send(
            &token, 1, MPI_CHAR, printer->rank + 1,
            printer->tok_tag, printer->comm
        );
        if (rc != MPI_SUCCESS) {
            ctu_panic("%s failed (rc=%d)", "MPI_Send", rc);
        }
    }
}

static inline void
ctu_mpi_ordered_printer_vpprintf(
    struct ctu_scope_reporter *reporter,
    bool print,
    const char *format,
    va_list args
) {
    ctu_mpi_ordered_printer_t *printer = (ctu_mpi_ordered_printer_t *)reporter->printer;

    if (!printer || !printer->initialized || !format) {
        return;
    }
    // Wait for everyone to complete last operation.
    int rc = MPI_Barrier(printer->comm);
    if (rc != MPI_SUCCESS) {
        ctu_panic("%s failed (rc=%d)", "MPI_Barrier", rc);
    }
    // Wait for my turn.
    ctu_mpi_wait_token(reporter);
    if (print) {
        // Print formatted message.
        vprintf(format, args);
        fflush(stdout);
    }
    // Pass it on to the next one.
    ctu_mpi_release_token(reporter);
    // Wait for everyone to complete print.
    rc = MPI_Barrier(printer->comm);
    if (rc != MPI_SUCCESS) {
        ctu_panic("%s failed (rc=%d)", "MPI_Barrier", rc);
    }
}

static inline void
ctu_mpi_ordered_printer_printf(
    ctu_scope_reporter_t *reporter,
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);
    ctu_mpi_ordered_printer_vpprintf(reporter, true, format, args);
    va_end(args);
}

static inline void
ctu_mpi_ordered_printer_pprintf(
    struct ctu_scope_reporter *reporter,
    bool print,
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);
    ctu_mpi_ordered_printer_vpprintf(reporter, print, format, args);
    va_end(args);
}

#endif // #ifdef MPI_FOUND

ctu_scope_reporter_t *
ctu_reporter_alloc(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
) {
    ctu_scope_reporter_t *reporter = (ctu_scope_reporter_t *)calloc(1, sizeof(*reporter));
    if (!reporter) ctu_panic("OOR!");

    switch (kind) {
        case CTU_SCOPE_KIND_PROCESS:
        case CTU_SCOPE_KIND_PTHREAD:
        case CTU_SCOPE_KIND_OPENMP:
            reporter->printer = NULL;
            reporter->init = &ctu_default_printer_init;
            reporter->id = &ctu_default_printer_id;
            reporter->fini = &ctu_default_printer_fini;
            reporter->printf = &ctu_default_printer_printf;
            reporter->pprintf = &ctu_default_printer_pprintf;
            break;
        case CTU_SCOPE_KIND_MPI:
#ifdef QUO_VADIS_MPI
            reporter->printer = calloc(1, sizeof(ctu_mpi_ordered_printer_t));
            reporter->init = &ctu_mpi_ordered_printer_init;
            reporter->id = &ctu_mpi_ordered_printer_id;
            reporter->fini = &ctu_mpi_ordered_printer_fini;
            reporter->printf = &ctu_mpi_ordered_printer_printf;
            reporter->pprintf = &ctu_mpi_ordered_printer_pprintf;

            reporter->init(reporter, scope);
#else
            ctu_unused(scope);
#endif
            break;
        default:
            ctu_panic("Invalid scope kind!");
    }
    return reporter;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
