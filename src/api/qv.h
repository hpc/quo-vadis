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
 * @file quo-vadis.h
 */

#ifndef QV_H_INCLUDED
#define QV_H_INCLUDED

/** Convenience definition. */
#define QV 1

#ifdef __cplusplus
extern "C" {
#endif

enum {
    QV_SUCCESS = 0,
    QV_SUCCESS_ALREADY_DONE,
    QV_ERR,
    QV_ERR_SYS,
    QV_ERR_OOR,
    QV_ERR_INVLD_ARG,
    QV_ERR_CALL_BEFORE_INIT,
    QV_ERR_TOPO,
    QV_ERR_MPI,
    QV_ERR_NOT_SUPPORTED,
    QV_ERR_POP,
    QV_ERR_NOT_FOUND
};

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
