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
 * @file qvi-pmi.cc
 */

#include "qvi-common.h"
#include "qvi-pmi.h"
#include "qvi-log.h"

#include "pmix.h"

struct qvi_pmi_s {
    pmix_proc_t myproc;
    /** Local (node) ID */
    uint32_t lid;
    /** Global (job) ID */
    uint32_t gid;
    /** Universe size   */
    uint32_t universe_size;
};

int
qvi_pmi_construct(
    qvi_pmi_t **pmi
) {
    if (!pmi) return QV_ERR_INVLD_ARG;

    int rc = QV_SUCCESS;

    qvi_pmi_t *ipmi = qvi_new qvi_pmi_t;
    if (!ipmi) {
        qvi_log_error("memory allocation failed");
        rc = QV_ERR_OOR;
    }
    *pmi = ipmi;
    return rc;
}

void
qvi_pmi_destruct(
    qvi_pmi_t *pmi
) {
    if (!pmi) return;
    delete pmi;
}

int
qvi_pmi_init(
    qvi_pmi_t *pmi
) {
    int rc;
    char const *ers = nullptr;
    // Multiple calls to PMIx_Init() are allowed, so just call it.
    rc = PMIx_Init(&pmi->myproc, nullptr, 0);
    if (rc != PMIX_SUCCESS) {
        ers = "PMIx_Init() failed";
        goto out;
    }
    //
    pmix_value_t *val;
    // Get our universe size
    pmix_proc_t proc;
    PMIX_PROC_CONSTRUCT(&proc);
    PMIX_LOAD_PROCID(&proc, pmi->myproc.nspace, PMIX_RANK_WILDCARD);

    rc = PMIx_Get(&proc, PMIX_UNIV_SIZE, nullptr, 0, &val);
    if (rc != PMIX_SUCCESS) {
        ers = "PMIx_Get(PMIX_UNIV_SIZE) failed";
        goto out;
    }
    pmi->universe_size = val->data.uint32;
    PMIX_VALUE_RELEASE(val);
    //
    rc = PMIx_Get(&pmi->myproc, PMIX_APP_RANK, nullptr, 0, &val);
    if (rc != PMIX_SUCCESS) {
        ers = "PMIx_Get(PMIX_APP_RANK) failed";
        goto out;
    }
    pmi->gid = val->data.rank;
    PMIX_VALUE_RELEASE(val);
    //
    rc = PMIx_Get(&pmi->myproc, PMIX_LOCAL_RANK, nullptr, 0, &val);
    if (rc != PMIX_SUCCESS) {
        ers = "PMIx_Get(PMIX_LOCAL_RANK) failed";
        goto out;
    }
    pmi->lid = val->data.uint16;
    PMIX_VALUE_RELEASE(val);
out:
    if (ers) {
        qvi_log_error("{} with rc={} ({})", ers, rc, PMIx_Error_string(rc));
        return QV_ERR_PMI;
    }
    return QV_SUCCESS;
}

int
qvi_pmi_finalize(
    qvi_pmi_t *pmi
) {
    QVI_UNUSED(pmi);

    int rc = PMIx_Finalize(nullptr, 0);
    if (rc != PMIX_SUCCESS) {
        char const *ers = "PMIx_Finalize() failed";
        qvi_log_warn("{} with rc={} ({})", ers, rc, PMIx_Error_string(rc));
    }
    return QV_SUCCESS;
}

uint32_t
qvi_pmi_lid(
    qvi_pmi_t *pmi
) {
    return pmi->lid;
}

uint32_t
qvi_pmi_gid(
    qvi_pmi_t *pmi
) {
    return pmi->gid;
}

uint32_t
qvi_pmi_usize(
    qvi_pmi_t *pmi
) {
    return pmi->universe_size;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
