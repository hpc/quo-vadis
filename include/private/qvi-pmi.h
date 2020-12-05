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
 * @file qvi-pmi.h
 */

#ifndef QVI_PMI_H
#define QVI_PMI_H

#include "private/qvi-common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations.
struct qvi_pmi_s;
typedef struct qvi_pmi_s qvi_pmi_t;

/**
 *
 */
int
qvi_pmi_construct(
    qvi_pmi_t **pmi
);

/**
 *
 */
void
qvi_pmi_destruct(
    qvi_pmi_t *pmi
);

/**
 *
 */
int
qvi_pmi_init(
    qvi_pmi_t *pmi
);

int
qvi_pmi_finalize(
    qvi_pmi_t *pmi
);

uint32_t
qvi_pmi_lid(
    qvi_pmi_t *pmi
);

uint32_t
qvi_pmi_gid(
    qvi_pmi_t *pmi
);

uint32_t
qvi_pmi_usize(
    qvi_pmi_t *pmi
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
