/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file common-test-utils.h
 *
 * Common test infrastructure.
 */

#ifndef COMMON_TEST_UTILS_H
#define COMMON_TEST_UTILS_H

// Avoid including internal QV headers here so
// we mimic real application usage in our tests.
#include "quo-vadis.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define CTU_STRINGIFY(x) #x
#define CTU_TOSTRING(x)  CTU_STRINGIFY(x)

#define ctu_unused(x)                                                          \
do {                                                                           \
    (void)(x);                                                                 \
} while (0)

#define ctu_panic(...)                                                         \
do {                                                                           \
    fprintf(stderr, "\n%s@%d: ", __func__, __LINE__);                          \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
    fflush(stderr);                                                            \
    exit(EXIT_FAILURE);                                                        \
} while (0)

#define ctu_assert(condition, ...)                                             \
do {                                                                           \
    if (!(condition)) {                                                        \
        ctu_panic(__VA_ARGS__);                                                \
    }                                                                          \
} while (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CTU_SCOPE_KIND_PROCESS = 0,
    CTU_SCOPE_KIND_PTHREAD,
    CTU_SCOPE_KIND_OPENMP,
    CTU_SCOPE_KIND_MPI
} ctu_scope_kind_t;

typedef struct {
    char const *name;
    qv_hw_obj_type_t type;
} ctu_hw_obj_name_to_type_t;

typedef struct {
    char const *name;
    qv_device_id_type_t devid;
} ctu_devid_name_to_id_t;

typedef struct ctu_scope_reporter {
    void *printer;
    void (*init)(struct ctu_scope_reporter *reporter, qv_scope_t *scope);
    int  (*id)(struct ctu_scope_reporter *reporter);
    void (*fini)(struct ctu_scope_reporter *reporter);
    void (*printf)(struct ctu_scope_reporter *reporter, const char *restrict, ...);
    void (*pprintf)(struct ctu_scope_reporter *reporter, bool print, const char *restrict, ...);
} ctu_scope_reporter_t;

// Maps QV object type names to their underlying values.
static const ctu_hw_obj_name_to_type_t ctu_hw_obj_name_to_type_tab[] = {
    {CTU_TOSTRING(QV_HW_OBJ_MACHINE),  QV_HW_OBJ_MACHINE},
    {CTU_TOSTRING(QV_HW_OBJ_PACKAGE),  QV_HW_OBJ_PACKAGE},
    {CTU_TOSTRING(QV_HW_OBJ_NUMANODE), QV_HW_OBJ_NUMANODE},
    {CTU_TOSTRING(QV_HW_OBJ_CORE),     QV_HW_OBJ_CORE},
    {CTU_TOSTRING(QV_HW_OBJ_PU),       QV_HW_OBJ_PU},
    {CTU_TOSTRING(QV_HW_OBJ_L1CACHE),  QV_HW_OBJ_L1CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L2CACHE),  QV_HW_OBJ_L2CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L3CACHE),  QV_HW_OBJ_L3CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L4CACHE),  QV_HW_OBJ_L4CACHE},
    {CTU_TOSTRING(QV_HW_OBJ_L5CACHE),  QV_HW_OBJ_L5CACHE}
};

static const size_t ctu_hw_obj_name_to_type_tab_size =
    sizeof(ctu_hw_obj_name_to_type_tab) / sizeof(ctu_hw_obj_name_to_type_t);

// Maps QV device ID type names to their underlying values.
static const ctu_devid_name_to_id_t ctu_devid_name_to_id_tab[] = {
    {CTU_TOSTRING(QV_DEVICE_ID_UUID),       QV_DEVICE_ID_UUID},
    {CTU_TOSTRING(QV_DEVICE_ID_PCI_BUS_ID), QV_DEVICE_ID_PCI_BUS_ID},
    {CTU_TOSTRING(QV_DEVICE_ID_ORDINAL),    QV_DEVICE_ID_ORDINAL}
};

// A deferred printf implementation.
void
ctu_dprintf(
    const char *format,
    ...
);

// Outputs the stored content filled by calls to ctu_dprintf() to stdout.
void
ctu_dflush(void);

static inline const char *
ctu_obj_name(
    qv_hw_obj_type_t type
) {
    switch(type) {
        case QV_HW_OBJ_MACHINE:  return CTU_TOSTRING(QV_HW_OBJ_MACHINE);
        case QV_HW_OBJ_PACKAGE:  return CTU_TOSTRING(QV_HW_OBJ_PACKAGE);
        case QV_HW_OBJ_CORE:     return CTU_TOSTRING(QV_HW_OBJ_CORE);
        case QV_HW_OBJ_PU:       return CTU_TOSTRING(QV_HW_OBJ_PU);
        case QV_HW_OBJ_L1CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L1CACHE);
        case QV_HW_OBJ_L2CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L2CACHE);
        case QV_HW_OBJ_L3CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L3CACHE);
        case QV_HW_OBJ_L4CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L4CACHE);
        case QV_HW_OBJ_L5CACHE:  return CTU_TOSTRING(QV_HW_OBJ_L5CACHE);
        case QV_HW_OBJ_NUMANODE: return CTU_TOSTRING(QV_HW_OBJ_NUMANODE);
        /** Device types. */
        case QV_HW_OBJ_GPU:      return CTU_TOSTRING(QV_HW_OBJ_GPU);
        case QV_HW_OBJ_NIC:      return CTU_TOSTRING(QV_HW_OBJ_NIC);
        default: return "?";
    }
}

static const size_t ctu_devid_name_to_id_tab_size =
    sizeof(ctu_devid_name_to_id_tab) / sizeof(ctu_devid_name_to_id_t);

static inline pid_t
ctu_gettid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

static inline void
ctu_default_printer_init(
    ctu_scope_reporter_t *reporter,
    qv_scope_t *scope
) {
    ctu_unused(reporter);
    ctu_unused(scope);
}

static inline int
ctu_default_printer_id(
    ctu_scope_reporter_t *reporter
) {
    ctu_unused(reporter);
    return ctu_gettid();
}

static inline void
ctu_default_printer_fini(
    ctu_scope_reporter_t *reporter
) {
    ctu_unused(reporter);
}

static inline void
ctu_default_printer_vpprintf(
    struct ctu_scope_reporter *reporter,
    bool print,
    const char *format,
    va_list args
) {
    ctu_unused(reporter);
    if (!format) return;
    if (print) {
        // Print formatted message.
        vprintf(format, args);
        fflush(stdout);
    }
}

static inline void
ctu_default_printer_printf(
    ctu_scope_reporter_t *reporter,
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);
    ctu_default_printer_vpprintf(reporter, true, format, args);
    va_end(args);
}

static inline void
ctu_default_printer_pprintf(
    ctu_scope_reporter_t *reporter,
    bool print,
    const char *format,
    ...
) {
    va_list args;
    va_start(args, format);
    ctu_default_printer_vpprintf(reporter, print, format, args);
    va_end(args);
}

static inline void
ctu_emit_task_bind(
    qv_scope_t *scope
) {
    const int pid = ctu_gettid();
    char const *ers = NULL;
    // Get new, current binding.
    char *binds = NULL;
    int rc = qv_scope_bind_string(scope, QV_BIND_STRING_LOGICAL, &binds);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_string() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    printf("[%d] cpubind=%s\n", pid, binds);
    free(binds);
}

/**
 * A verbose version of qv_bind_push().
 */
static inline void
ctu_bind_push(
    qv_scope_t *scope
) {
    char const *ers = NULL;
    const int pid = ctu_gettid();

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
    printf("[%d] Current cpubind before qv_bind_push() is %s\n", pid, bind0s);

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
    printf("[%d] New cpubind after qv_bind_push() is %s\n", pid, bind1s);

    free(bind0s);
    free(bind1s);
}

/**
 * A verbose version of qv_bind_pop().
 */
static inline void
ctu_bind_pop(
    qv_scope_t *scope
) {
    char const *ers = NULL;

    const int pid = ctu_gettid();

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
    printf("[%d] Current cpubind before qv_bind_pop() is %s\n", pid, bind0s);

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
    printf("[%d] New cpubind after qv_bind_pop() is %s\n", pid, bind1s);

    free(bind0s);
    free(bind1s);
}

static inline int
ctu_emit_host_hw_info(
    qv_scope_t *scope,
    const char *scope_name
) {
    printf("\n# System Hardware Overview for %s\n", scope_name);
    for (size_t i = 0; i < ctu_hw_obj_name_to_type_tab_size; ++i) {
        int n;
        int rc = qv_scope_hw_obj_count(
            scope, ctu_hw_obj_name_to_type_tab[i].type, &n
        );
        if (rc != QV_SUCCESS) {
            fprintf(
                stderr, "qv_scope_hw_obj_count(%s) failed\n",
                ctu_hw_obj_name_to_type_tab[i].name
            );
            return rc;
        }
        printf("# %s: n = %d\n", ctu_hw_obj_name_to_type_tab[i].name, n);
    }
    printf("# ---------------------------------------\n");
    return QV_SUCCESS;
}

static inline void
ctu_emit_device_info(
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_type,
    const char *scope_name
) {
    // Get number of devices.
    int ndevs;
    int rc = qv_scope_hw_obj_count(scope, dev_type, &ndevs);
    if (rc != QV_SUCCESS) {
        const char *ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    printf(
        "\n# Discovered %d %s(s) in %s\n",
        ndevs, ctu_obj_name(dev_type), scope_name
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
            printf(
                "# Device %u %s = %s\n", i,
                ctu_devid_name_to_id_tab[j].name, devids
            );
            free(devids);
        }
    }
    printf("# -----------------------------------------------------------\n");
}

ctu_scope_reporter_t *
ctu_reporter_alloc(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
);

static inline void
ctu_reporter_free(
    ctu_scope_reporter_t *reporter
) {
    if (reporter) {
        if (reporter->fini) {
            reporter->fini(reporter);
        }
        free(reporter);
    }
}

static inline void
ctu_hostres_report(
    qv_scope_t *scope,
    ctu_scope_reporter_t *reporter
) {
    char const *ers = NULL;
    const int myid = reporter->id(reporter);

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    reporter->pprintf(
        reporter, sgrank == 0,
        "# System Hardware Overview --------------\n"
    );
    for (size_t i = 0; i < ctu_hw_obj_name_to_type_tab_size; ++i) {
        int n;
        int rc = qv_scope_hw_obj_count(
            scope, ctu_hw_obj_name_to_type_tab[i].type, &n
        );
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_hw_obj_count() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }

        reporter->printf(
            reporter, "[%d] %s=%d\n", myid,
            ctu_hw_obj_name_to_type_tab[i].name, n
        );

        rc = qv_scope_barrier(scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_scope_barrier() failed";
            ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
        }
    }
    reporter->pprintf(
        reporter, sgrank == 0,
        "# ---------------------------------------\n"
    );
}

static inline void
ctu_cpuset_report(
    qv_scope_t *scope,
    ctu_scope_reporter_t *reporter
) {
    char const *ers = NULL;
    const int myid = reporter->id(reporter);

    int sgrank;
    int rc = qv_scope_group_rank(scope, &sgrank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    reporter->pprintf(
        reporter, sgrank == 0,
        "# Process Affinity Overview--------------\n"
    );

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
    reporter->printf(reporter, "[%d] scope cpuset is %s\n", myid, bind1s);
    free(bind1s);

    rc = qv_scope_bind_pop(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_bind_pop() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    reporter->pprintf(
        reporter, sgrank == 0,
        "# ---------------------------------------\n"
    );
}

static inline void
ctu_scope_report(
    qv_scope_t *scope,
    ctu_scope_kind_t kind,
    const char *const scope_name
) {
    char const *ers = NULL;

    ctu_scope_reporter_t *reporter = ctu_reporter_alloc(scope, kind);
    const int myid = reporter->id(reporter);

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

    reporter->pprintf(
        reporter, sgrank == 0, "\n###### Start scope report for %s\n", scope_name
    );

    reporter->printf(
        reporter,
        "[%d] hello from group rank %d of %d\n",
        myid, sgrank, sgsize
    );

    ctu_cpuset_report(scope, reporter);

    reporter->pprintf(
        reporter, sgrank == 0, "###### Endof scope report for %s\n", scope_name
    );

    rc = qv_scope_barrier(scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_barrier() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_reporter_free(reporter);
}

/**
 * Collective call over the provided scope that tests pushing and popping of
 * binding policies.
 */
static inline void
ctu_change_bind(
    qv_scope_t *scope,
    ctu_scope_kind_t kind
) {
    char const *ers = NULL;

    ctu_scope_reporter_t *reporter = ctu_reporter_alloc(scope, kind);
    const int myid = reporter->id(reporter);

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
    reporter->printf(reporter, "[%d] Current cpubind is %s\n", myid, bind0s);

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
    reporter->printf(reporter, "[%d] New cpubind is %s\n", myid, bind1s);

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
    reporter->printf(reporter, "[%d] Popped cpubind is %s\n", myid, bind2s);

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

    ctu_reporter_free(reporter);
}

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
