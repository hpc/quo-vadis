/*
 * Copyright (c)      2021 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c)      2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-config.
 */

#ifndef QVI_CONFIG_H
#define QVI_CONFIG_H

#include "qvi-common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Keep in sync with qvi_config_rmi_t structure.
#define QVI_CONFIG_RMI_PICTURE "ss"

typedef struct qvi_config_rmi_s {
    qvi_hwloc_t *hwloc;
    char *url;
    char *hwtopo_path;
} qvi_config_rmi_t;

/**
 *
 */
int
qvi_config_rmi_new(
    qvi_config_rmi_t **config
);

/**
 *
 */
void
qvi_config_rmi_free(
    qvi_config_rmi_t **config
);

/**
 *
 */
int
qvi_config_rmi_cp(
    qvi_config_rmi_t *from,
    qvi_config_rmi_t *to
);

/**
 *
 */
int
qvi_config_rmi_pack(
    qvi_config_rmi_t *config,
    qvi_bbuff_t *buff
);

/**
 *
 */
int
qvi_config_rmi_unpack(
    void *buff,
    qvi_config_rmi_t *config
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
