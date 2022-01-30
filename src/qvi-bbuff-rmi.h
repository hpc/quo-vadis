/*
 * Copyright (c) 2021-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-bbuff-rmi.h
 */

#ifndef QVI_BBUFF_RMI_H
#define QVI_BBUFF_RMI_H

#include "qvi-common.h"
#include "qvi-bbuff.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 */
int
qvi_bbuff_rmi_vsprintf(
    qvi_bbuff_t *buff,
    const char *picture,
    va_list args
);

/**
 *
 */
int
qvi_bbuff_rmi_sprintf(
    qvi_bbuff_t *buff,
    const char *picture,
    ...
);

/**
 *
 */
int
qvi_bbuff_rmi_vsscanf(
    void *data,
    const char *picture,
    va_list args
);

/**
 *
 */
int
qvi_bbuff_rmi_sscanf(
    void *data,
    const char *picture,
    ...
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
