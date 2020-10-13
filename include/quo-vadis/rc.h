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
 * @file rc.h
 */

#ifndef QUO_VADIS_RC_H
#define QUO_VADIS_RC_H

#ifdef __cplusplus
extern "C" {
#endif

/** Return codes. */
// If this changes, please update the order and contents of qv_rc_strerrs.
enum {
    QV_SUCCESS = 0,
    QV_SUCCESS_ALREADY_DONE,
    QV_ERR,
    QV_ERR_INTERNAL,
    QV_ERR_SYS,
    QV_ERR_OOR,
    QV_ERR_INVLD_ARG,
    QV_ERR_CALL_BEFORE_INIT,
    QV_ERR_TOPO,
    QV_ERR_MPI,
    QV_ERR_MSG,
    QV_ERR_NOT_SUPPORTED,
    QV_ERR_POP,
    QV_ERR_NOT_FOUND
};

/** Description of the return codes. */
static const char *qv_rc_strerrs[] = {
    "Success",
    "Success, operation already complete",
    "Unspecified error",
    "Internal error",
    "System error",
    "Out of resources",
    "Invalid argument",
    "Call before initialization",
    "Hardware topology error",
    "MPI error",
    "Internal message error",
    "Operation not supported",
    "Pop operation error",
    "Not found"
};

static inline const char *
qv_strerr(int ec)
{
    return qv_rc_strerrs[ec];
}

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
