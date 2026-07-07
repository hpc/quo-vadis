/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-process-scopes.c
 */

#include "quo-vadis.h"
#include "common-test-utils.h"

int
main(void)
{
    char const *ers = NULL;
    int rc = QV_SUCCESS;

    qv_scope_t *self_scope = NULL;
    rc = qv_process_scope_get(
        QV_SCOPE_PROCESS, QV_SCOPE_FLAG_NONE, &self_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_PROCESS) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_scope_report(self_scope, CTU_SCOPE_KIND_PROCESS, "self_scope");
    ctu_emit(self_scope, CTU_SCOPE_KIND_PROCESS, "\n");

    rc = qv_scope_free(self_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *base_scope;
    rc = qv_process_scope_get(
        QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_USER) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_scope_report(base_scope, CTU_SCOPE_KIND_PROCESS, "base_scope");
    ctu_emit(base_scope, CTU_SCOPE_KIND_PROCESS, "\n");

    ctu_change_bind(base_scope, CTU_SCOPE_KIND_PROCESS);
    ctu_emit(base_scope, CTU_SCOPE_KIND_PROCESS, "\n");

    int srank;
    rc = qv_scope_group_rank(base_scope, &srank);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_rank() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (srank != 0) {
        ers = "Invalid task ID detected";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int sgsize;
    rc = qv_scope_group_size(base_scope, &sgsize);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (sgsize != 1) {
        ers = "Invalid number of tasks detected";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    int n_pus;
    rc = qv_scope_hw_obj_count(
        base_scope, QV_HW_OBJ_PU, &n_pus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_host_hw_info(
        base_scope, CTU_SCOPE_KIND_PROCESS, "base_scope"
    );
    ctu_emit(base_scope, CTU_SCOPE_KIND_PROCESS, "\n");

    const int npieces = 2;
    qv_scope_t *sub_scope;
    rc = qv_scope_split(
        base_scope, npieces, srank, &sub_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_hw_obj_count(
        sub_scope, QV_HW_OBJ_PU, &n_pus
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_hw_obj_count() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_host_hw_info(sub_scope, CTU_SCOPE_KIND_PROCESS, "sub_scope");
    ctu_emit(sub_scope, CTU_SCOPE_KIND_PROCESS, "\n");

    ctu_scope_report(sub_scope, CTU_SCOPE_KIND_PROCESS, "sub_scope");
    ctu_emit(sub_scope, CTU_SCOPE_KIND_PROCESS, "\n");

    ctu_change_bind(sub_scope, CTU_SCOPE_KIND_PROCESS);

    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(sub_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
