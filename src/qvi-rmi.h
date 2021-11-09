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
 * @file qvi-rmi.h
 *
 * Resource Management and Inquiry
 */

#ifndef QVI_RMI_H
#define QVI_RMI_H

#include "qvi-common.h"
#include "qvi-bbuff.h"
#include "qvi-hwloc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Keep in sync with qvi_rmi_config_t structure.
#define QVI_RMI_CONFIG_PICTURE "ss"

// Forward declarations.
struct qvi_rmi_server_s;
typedef struct qvi_rmi_server_s qvi_rmi_server_t;

struct qvi_rmi_client_s;
typedef struct qvi_rmi_client_s qvi_rmi_client_t;

typedef struct qvi_rmi_config_s {
    qvi_hwloc_t *hwloc;
    char *url;
    char *hwtopo_path;
} qvi_rmi_config_t;

/**
 *
 */
int
qvi_rmi_config_new(
    qvi_rmi_config_t **config
);

/**
 *
 */
void
qvi_rmi_config_free(
    qvi_rmi_config_t **config
);

/**
 *
 */
int
qvi_rmi_config_cp(
    qvi_rmi_config_t *from,
    qvi_rmi_config_t *to
);

/**
 *
 */
int
qvi_rmi_config_pack(
    qvi_rmi_config_t *config,
    qvi_bbuff_t *buff
);

/**
 *
 */
int
qvi_rmi_config_unpack(
    void *buff,
    qvi_rmi_config_t *config
);

/**
 *
 */
int
qvi_rmi_server_new(
    qvi_rmi_server_t **server
);

/**
 *
 */
void
qvi_rmi_server_free(
    qvi_rmi_server_t **server
);

/**
 *
 */
int
qvi_rmi_server_config(
    qvi_rmi_server_t *server,
    qvi_rmi_config_t *config
);

/**
 *
 */
int
qvi_rmi_server_start(
    qvi_rmi_server_t *server,
    bool block
);

/**
 *
 */
int
qvi_rmi_client_new(
    qvi_rmi_client_t **client
);

/**
 *
 */
void
qvi_rmi_client_free(
    qvi_rmi_client_t **client
);

/**
 *
 */
int
qvi_rmi_client_connect(
    qvi_rmi_client_t *client,
    const char *url
);

/**
 *
 */
qvi_hwloc_t *
qvi_rmi_client_hwloc_get(
    qvi_rmi_client_t *client
);

/**
 *
 */
int
qvi_rmi_task_get_cpubind(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_cpuset_t *cpuset
);

/**
 *
 */
int
qvi_rmi_task_set_cpubind_from_cpuset(
    qvi_rmi_client_t *client,
    pid_t who,
    hwloc_const_cpuset_t cpuset
);

/**
 *
 */
int
qvi_rmi_scope_get_intrinsic_scope_cpuset(
    qvi_rmi_client_t *client,
    pid_t requestor_pid,
    qv_scope_intrinsic_t iscope,
    hwloc_cpuset_t cpuset
);

/**
 *
 */
int
qvi_rmi_get_nobjs_in_cpuset(
    qvi_rmi_client_t *client,
    qv_hw_obj_type_t target_obj,
    hwloc_const_cpuset_t cpuset,
    unsigned *nobjs
);

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
