/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file quo-vadis.cc
 */

#include "qvi-context.h" // IWYU pragma: keep
#include "qvi-scope.h"

int
qv_version(
    int *major,
    int *minor,
    int *patch
) {
    if (!major || !minor || !patch) {
        return QV_ERR_INVLD_ARG;
    }

    *major = PACKAGE_VERSION_MAJOR;
    *minor = PACKAGE_VERSION_MINOR;
    *patch = PACKAGE_VERSION_PATCH;

    return QV_SUCCESS;
}

int
qv_bind_push(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_bind_push(
            ctx->bind_stack, qvi_scope_cpuset_get(scope).cdata()
        );
    }
    qvi_catch_and_return();
}

int
qv_bind_pop(
    qv_context_t *ctx
) {
    if (!ctx) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_bind_pop(ctx->bind_stack);
    }
    qvi_catch_and_return();
}

int
qv_bind_string(
    qv_context_t *ctx,
    qv_bind_string_format_t format,
    char **str
) {
    if (!ctx || !str) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_bind_string(ctx->bind_stack, format, str);
    }
    qvi_catch_and_return();
}

int
qv_context_barrier(
    qv_context_t *ctx
) {
    if (!ctx) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return ctx->zgroup->barrier();
    }
    qvi_catch_and_return();
}

int
qv_scope_free(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        qvi_scope_free(&scope);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

int
qv_scope_nobjs(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t obj,
    int *nobjs
) {
    if (!ctx || !scope || !nobjs) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_scope_nobjs(scope, obj, nobjs);
    }
    qvi_catch_and_return();
}

int
qv_scope_taskid(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int *taskid
) {
    if (!ctx || !scope || !taskid) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_scope_taskid(scope, taskid);
    }
    qvi_catch_and_return();
}

int
qv_scope_ntasks(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int *ntasks
) {
    if (!ctx || !scope || !ntasks) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_scope_ntasks(scope, ntasks);
    }
    qvi_catch_and_return();
}

int
qv_scope_get(
    qv_context_t *ctx,
    qv_scope_intrinsic_t iscope,
    qv_scope_t **scope
) {
    if (!ctx || !scope) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_scope_get(
            ctx->zgroup, ctx->rmi, iscope, scope
        );
    }
    qvi_catch_and_return();
}

int
qv_scope_barrier(
    qv_context_t *ctx,
    qv_scope_t *scope
) {
    if (!ctx || !scope) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_scope_barrier(scope);
    }
    qvi_catch_and_return();
}

// TODO(skg) Add Fortran interface.
int
qv_scope_create(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int nobjs,
    qv_scope_create_hints_t hints,
    qv_scope_t **subscope
) {
    if (!ctx || !scope || (nobjs < 0) || !subscope) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_scope_create(
            scope, type, nobjs, hints, subscope
        );
    }
    qvi_catch_and_return();
}

int
qv_scope_split(
    qv_context_t *ctx,
    qv_scope_t *scope,
    int npieces,
    int color,
    qv_scope_t **subscope
) {
    if (!ctx || !scope || (npieces <= 0) | !subscope) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        // We use the sentinel value QV_HW_OBJ_LAST to differentiate between
        // calls from split() and split_at(). Since this call doesn't have a
        // hardware type argument, we use QV_HW_OBJ_LAST as the hardware type.
        return qvi_scope_split(
            scope, npieces, color, QV_HW_OBJ_LAST, subscope
        );
    }
    qvi_catch_and_return();
}

int
qv_scope_split_at(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t type,
    int group_id,
    qv_scope_t **subscope
) {
    if (!ctx || !scope || !subscope) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_scope_split_at(
            scope, type, group_id, subscope
        );
    }
    qvi_catch_and_return();
}

int
qv_scope_get_device_id(
    qv_context_t *ctx,
    qv_scope_t *scope,
    qv_hw_obj_type_t dev_obj,
    int dev_index,
    qv_device_id_type_t id_type,
    char **dev_id
) {
    if (!ctx || !scope || (dev_index < 0) || !dev_id) {
        return QV_ERR_INVLD_ARG;
    }
    try {
        std::lock_guard<std::mutex> guard(ctx->mutex);
        return qvi_scope_get_device_id(
            scope, dev_obj, dev_index, id_type, dev_id
        );
    }
    qvi_catch_and_return();
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
