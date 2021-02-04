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
 * @file quo-vadis.h
 */

#ifndef QUO_VADIS_H
#define QUO_VADIS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Convenience definition. */
#define QUO_VADIS 1

/** Return codes. */
#include "quo-vadis/qv-rc.h"
/** Types. */
#include "quo-vadis/qv-types.h"
/** Task things. */
#include "quo-vadis/qv-task.h"
/** Group support. */
#include "quo-vadis/qv-group.h"
/** Scope support. */
#include "quo-vadis/qv-scope.h"
/** MPI support. */
// TODO(skg) Add guard for when MPI support isn't available.
#include "quo-vadis/qv-mpi.h"

#ifdef __cplusplus
}
#endif

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
